/******************************************************************************
 *
 * Copyright(c) 2007 - 2019 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/
#define _HCI_INTF_C_

#include <drv_types.h>

#include <linux/pci_regs.h>
#include <rtw_trx_pci.h>
#ifndef CONFIG_PCI_HCI

	#error "CONFIG_PCI_HCI shall be on!\n"

#endif


#ifdef CONFIG_80211N_HT
	extern int rtw_ht_enable;
	extern int rtw_bw_mode;
	extern int rtw_ampdu_enable;/* for enable tx_ampdu */
#endif

#ifdef CONFIG_GLOBAL_UI_PID
int ui_pid[3] = {0, 0, 0};
#endif

#ifdef CONFIG_PM
	static int rtw_pci_suspend(struct pci_dev *pdev, pm_message_t state);
	static int rtw_pci_resume(struct pci_dev *pdev);
#endif

static int rtw_dev_probe(struct pci_dev *pdev, const struct pci_device_id *pdid);
static void rtw_dev_remove(struct pci_dev *pdev);
static void rtw_dev_shutdown(struct pci_dev *pdev);

static struct specific_device_id specific_device_id_tbl[] = {
	{.idVendor = 0x0b05, .idProduct = 0x1791, .flags = SPEC_DEV_ID_DISABLE_HT},
	{.idVendor = 0x13D3, .idProduct = 0x3311, .flags = SPEC_DEV_ID_DISABLE_HT},
	{}
};

struct pci_device_id rtw_pci_id_tbl[] = {
#ifdef CONFIG_RTL8852A
	{PCI_DEVICE(PCI_VENDER_ID_REALTEK, 0xA852), .driver_data = RTL8852A},/*FPGA*/
	{PCI_DEVICE(PCI_VENDER_ID_REALTEK, 0x8852), .driver_data = RTL8852A},
	{PCI_DEVICE(PCI_VENDER_ID_REALTEK, 0x885B), .driver_data = RTL8852A},
	{PCI_DEVICE(PCI_VENDER_ID_REALTEK, 0x885C), .driver_data = RTL8852A},
#endif
#ifdef CONFIG_RTL8852B
	{PCI_DEVICE(PCI_VENDER_ID_REALTEK, 0xB852), .driver_data = RTL8852B},/*FPGA*/
#endif
	{},
};

struct pci_drv_priv {
	struct pci_driver rtw_pci_drv;
	int drv_registered;
};


static struct pci_drv_priv pci_drvpriv = {
	.rtw_pci_drv.name = (char *)DRV_NAME,
	.rtw_pci_drv.probe = rtw_dev_probe,
	.rtw_pci_drv.remove = rtw_dev_remove,
	.rtw_pci_drv.shutdown = rtw_dev_shutdown,
	.rtw_pci_drv.id_table = rtw_pci_id_tbl,
#ifdef CONFIG_PM
	.rtw_pci_drv.suspend = rtw_pci_suspend,
	.rtw_pci_drv.resume = rtw_pci_resume,
#endif
};


MODULE_DEVICE_TABLE(pci, rtw_pci_id_tbl);

void	PlatformClearPciPMEStatus(_adapter *adapter)
{
	struct dvobj_priv *pdvobjpriv = adapter_to_dvobj(adapter);
	PPCI_DATA pci_data = dvobj_to_pci(pdvobjpriv);
	struct pci_dev *pdev = pci_data->ppcidev;
	BOOLEAN		PCIClkReq = _FALSE;
	u8	PMCSReg;

	if (pdev->pm_cap) {
		/* Get the PM CSR (Control/Status Register), */
		/* The PME_Status is located at PM Capatibility offset 5, bit 7 */

		pci_read_config_byte(pdev, pdev->pm_cap + 5, &PMCSReg);
		if (PMCSReg & BIT7) {
			/* PME event occurred, clear the PM_Status by write 1 */
			PMCSReg = PMCSReg | BIT7;

			pci_write_config_byte(pdev, pdev->pm_cap + 5, PMCSReg);
			PCIClkReq = _TRUE;
			/* Read it back to check */
			pci_read_config_byte(pdev, pdev->pm_cap + 5, &PMCSReg);
			RTW_INFO("%s(): Clear PME status 0x%2x to 0x%2x\n", __func__, pdev->pm_cap + 5, PMCSReg);
		} else {
			RTW_INFO("%s(): PME status(0x%2x) = 0x%2x\n", __func__, pdev->pm_cap + 5, PMCSReg);
		}
	} else {
		RTW_INFO("%s(): Cannot find PME Capability\n", __func__);
	}

	RTW_INFO("PME, value_offset = %x, PME EN = %x\n", pdev->pm_cap + 5, PCIClkReq);
}



#ifdef CONFIG_PCI_DYNAMIC_ASPM_LINK_CTRL
static bool _rtw_pci_set_aspm_lnkctl_reg(struct pci_dev *pdev, u8 mask, u8 val)
{
	u8 linkctrl, new_val;

	if (!pdev || !pdev->pcie_cap || !mask)
		return false;

	pci_read_config_byte(pdev, pdev->pcie_cap + PCI_EXP_LNKCTL, &linkctrl);
	new_val = (linkctrl & ~mask) | val;
	if (new_val == linkctrl)
		return false;

	pci_write_config_byte(pdev, pdev->pcie_cap + PCI_EXP_LNKCTL, new_val);

	return true;
}

void rtw_pci_set_aspm_lnkctl(_adapter *padapter, u8 mode)
{
	struct dvobj_priv *pdvobjpriv = adapter_to_dvobj(padapter);
	PPCI_DATA pci_data = dvobj_to_pci(pdvobjpriv);
	struct pci_priv	*pcipriv = &(pci_data->pcipriv);
	struct pci_dev	*pdev = pci_data->ppcidev;
	struct pci_dev	*br_pdev = pdev->bus->self;
	struct registry_priv  *registry_par = &padapter->registrypriv;
	u32 pci_dynamic_aspm_linkctrl = registry_par->pci_dynamic_aspm_linkctrl;
	u8 lnkctl_val, lnkctl_mask;
	u8 dev_lnkctl_val, br_lnkctl_val;

	if (!pci_dynamic_aspm_linkctrl)
		return;

	switch (mode) {
	case ASPM_MODE_PERF:
		lnkctl_val = pci_dynamic_aspm_linkctrl & GENMASK(1, 0);
		lnkctl_mask = (pci_dynamic_aspm_linkctrl & GENMASK(5, 4)) >> 4;
		break;
	case ASPM_MODE_PS:
		lnkctl_val = (pci_dynamic_aspm_linkctrl & GENMASK(9, 8)) >> 8;
		lnkctl_mask = (pci_dynamic_aspm_linkctrl & GENMASK(13, 12)) >> 12;
		break;
	case ASPM_MODE_DEF:
		lnkctl_val = 0x0; /* fill val to make checker happy */
		lnkctl_mask = 0x0;
		break;
	default:
		return;
	}

	/* if certain mask==0x0, we restore the default value with mask 0x03 */
	if (lnkctl_mask == 0x0) {
		lnkctl_mask = PCI_EXP_LNKCTL_ASPMC;
		dev_lnkctl_val = pcipriv->linkctrl_reg;
		br_lnkctl_val = pcipriv->pcibridge_linkctrlreg;
	} else {
		dev_lnkctl_val = lnkctl_val;
		br_lnkctl_val = lnkctl_val;
	}

	if (_rtw_pci_set_aspm_lnkctl_reg(pdev, lnkctl_mask, dev_lnkctl_val))
		rtw_udelay_os(50);
	_rtw_pci_set_aspm_lnkctl_reg(br_pdev, lnkctl_mask, br_lnkctl_val);
}
#endif


static u8 rtw_pci_get_amd_l1_patch(struct dvobj_priv *pdvobjpriv, struct pci_dev *pdev)
{
	u8	status = _FALSE;
	u8	offset_e0;
	u32	offset_e4;

	pci_write_config_byte(pdev, 0xE0, 0xA0);
	pci_read_config_byte(pdev, 0xE0, &offset_e0);

	if (offset_e0 == 0xA0) {
		pci_read_config_dword(pdev, 0xE4, &offset_e4);
		if (offset_e4 & BIT(23))
			status = _TRUE;
	}

	return status;
}

static s32	rtw_set_pci_cache_line_size(struct pci_dev *pdev, u8 CacheLineSizeToSet)
{
	u8	ucPciCacheLineSize;
	s32	Result;

	/* ucPciCacheLineSize  = pPciConfig->CacheLineSize; */
	pci_read_config_byte(pdev, PCI_CACHE_LINE_SIZE, &ucPciCacheLineSize);

	if (ucPciCacheLineSize < 8 || ucPciCacheLineSize > 16) {
		RTW_INFO("Driver Sets default Cache Line Size...\n");

		ucPciCacheLineSize = CacheLineSizeToSet;

		Result = pci_write_config_byte(pdev, PCI_CACHE_LINE_SIZE, ucPciCacheLineSize);

		if (Result != 0) {
			RTW_INFO("pci_write_config_byte (CacheLineSize) Result=%d\n", Result);
			goto _SET_CACHELINE_SIZE_FAIL;
		}

		Result = pci_read_config_byte(pdev, PCI_CACHE_LINE_SIZE, &ucPciCacheLineSize);
		if (Result != 0) {
			RTW_INFO("pci_read_config_byte (PciCacheLineSize) Result=%d\n", Result);
			goto _SET_CACHELINE_SIZE_FAIL;
		}

		if (ucPciCacheLineSize != CacheLineSizeToSet) {
			RTW_INFO("Failed to set Cache Line Size to 0x%x! ucPciCacheLineSize=%x\n", CacheLineSizeToSet, ucPciCacheLineSize);
			goto _SET_CACHELINE_SIZE_FAIL;
		}
	}

	return _SUCCESS;

_SET_CACHELINE_SIZE_FAIL:

	return _FAIL;
}


#define PCI_CMD_ENABLE_BUS_MASTER		BIT(2)
#define PCI_CMD_DISABLE_INTERRUPT		BIT(10)
#define CMD_BUS_MASTER				BIT(2)

static s32 rtw_pci_parse_configuration(struct pci_dev *pdev, struct dvobj_priv *dvobj)
{
	PPCI_DATA pci_data = dvobj_to_pci(dvobj);
	struct pci_priv	*pcipriv = &(pci_data->pcipriv);
	/* PPCI_COMMON_CONFIG pPciConfig = (PPCI_COMMON_CONFIG) pucBuffer; */
	/* u16	usPciCommand = pPciConfig->Command; */
	u16	usPciCommand = 0;
	int	Result, ret = _FAIL;
	u8	LinkCtrlReg;
	u8	ClkReqReg;

	/* RTW_INFO("%s==>\n", __func__); */

	pci_read_config_word(pdev, PCI_COMMAND, &usPciCommand);

	do {
		/* 3 Enable bus matering if it isn't enabled by the BIOS */
		if (!(usPciCommand & PCI_CMD_ENABLE_BUS_MASTER)) {
			RTW_INFO("Bus master is not enabled by BIOS! usPciCommand=%x\n", usPciCommand);

			usPciCommand |= CMD_BUS_MASTER;

			Result = pci_write_config_word(pdev, PCI_COMMAND, usPciCommand);
			if (Result != 0) {
				RTW_INFO("pci_write_config_word (Command) Result=%d\n", Result);
				ret = _FAIL;
				break;
			}

			Result = pci_read_config_word(pdev, PCI_COMMAND, &usPciCommand);
			if (Result != 0) {
				RTW_INFO("pci_read_config_word (Command) Result=%d\n", Result);
				ret = _FAIL;
				break;
			}

			if (!(usPciCommand & PCI_CMD_ENABLE_BUS_MASTER)) {
				RTW_INFO("Failed to enable bus master! usPciCommand=%x\n", usPciCommand);
				ret = _FAIL;
				break;
			}
		}
		RTW_INFO("Bus master is enabled. usPciCommand=%x\n", usPciCommand);

		/* 3 Enable interrupt */
		if ((usPciCommand & PCI_CMD_DISABLE_INTERRUPT)) {
			RTW_INFO("INTDIS==1 usPciCommand=%x\n", usPciCommand);

			usPciCommand &= (~PCI_CMD_DISABLE_INTERRUPT);

			Result = pci_write_config_word(pdev, PCI_COMMAND, usPciCommand);
			if (Result != 0) {
				RTW_INFO("pci_write_config_word (Command) Result=%d\n", Result);
				ret = _FAIL;
				break;
			}

			Result = pci_read_config_word(pdev, PCI_COMMAND, &usPciCommand);
			if (Result != 0) {
				RTW_INFO("pci_read_config_word (Command) Result=%d\n", Result);
				ret = _FAIL;
				break;
			}

			if ((usPciCommand & PCI_CMD_DISABLE_INTERRUPT)) {
				RTW_INFO("Failed to set INTDIS to 0! usPciCommand=%x\n", usPciCommand);
				ret = _FAIL;
				break;
			}
		}

		/*  */
		/* Description: Find PCI express capability offset. Porting from 818xB by tynli 2008.12.19 */
		/*  */
		/* ------------------------------------------------------------- */

		/* 3 PCIeCap */
		if (pdev->pcie_cap) {
			pcipriv->pciehdr_offset = pdev->pcie_cap;
			RTW_INFO("PCIe Header Offset =%x\n", pdev->pcie_cap);

			/* 3 Link Control Register */
			/* Read "Link Control Register" Field (80h ~81h) */
			Result = pci_read_config_byte(pdev, pdev->pcie_cap + 0x10, &LinkCtrlReg);
			if (Result != 0) {
				RTW_INFO("pci_read_config_byte (Link Control Register) Result=%d\n", Result);
				break;
			}

			pcipriv->linkctrl_reg = LinkCtrlReg;
			RTW_INFO("Link Control Register =%x\n", LinkCtrlReg);

			/* 3 Get Capability of PCI Clock Request */
			/* The clock request setting is located at 0x81[0] */
			Result = pci_read_config_byte(pdev, pdev->pcie_cap + 0x11, &ClkReqReg);
			if (Result != 0) {
				pcipriv->pci_clk_req = _FALSE;
				RTW_INFO("pci_read_config_byte (Clock Request Register) Result=%d\n", Result);
				break;
			}
			if (ClkReqReg & BIT(0))
				pcipriv->pci_clk_req = _TRUE;
			else
				pcipriv->pci_clk_req = _FALSE;
			RTW_INFO("Clock Request =%x\n", pcipriv->pci_clk_req);
		} else {
			/* We didn't find a PCIe capability. */
			RTW_INFO("Didn't Find PCIe Capability\n");
			break;
		}

		/* 3 Fill Cacheline */
		ret = rtw_set_pci_cache_line_size(pdev, 8);
		if (ret != _SUCCESS) {
			RTW_INFO("rtw_set_pci_cache_line_size fail\n");
			break;
		}

		/* Include 92C suggested by SD1. Added by tynli. 2009.11.25.
		 * Enable the Backdoor
		 */
		{
			u8	tmp;

			Result = pci_read_config_byte(pdev, 0x98, &tmp);

			tmp |= BIT4;

			Result = pci_write_config_byte(pdev, 0x98, tmp);

		}
		ret = _SUCCESS;
	} while (_FALSE);

	return ret;
}

/*
 * 2009/10/28 MH Enable rtl8192ce DMA64 function. We need to enable 0x719 BIT5
 *   */
#ifdef CONFIG_64BIT_DMA
u8 PlatformEnableDMA64(_adapter *adapter)
{
	struct dvobj_priv *pdvobjpriv = adapter_to_dvobj(adapter);
	PPCI_DATA pci_data = dvobj_to_pci(pdvobjpriv);
	struct pci_dev	*pdev = pci_data->ppcidev;
	
	u8	bResult = _TRUE;
	u8	value;

	pci_read_config_byte(pdev, 0x719, &value);

	/* 0x719 Bit5 is DMA64 bit fetch. */
	value |= (BIT5);

	pci_write_config_byte(pdev, 0x719, value);

	return bResult;
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)) || (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 18))
#define rtw_pci_interrupt(x, y, z) rtw_pci_interrupt(x, y)
#endif

static irqreturn_t rtw_pci_interrupt(int irq, void *priv, struct pt_regs *regs)
{
	struct dvobj_priv *dvobj = (struct dvobj_priv *)priv;
	PPCI_DATA pci_data = dvobj_to_pci(dvobj);
	enum rtw_phl_status pstatus =  RTW_PHL_STATUS_SUCCESS;
	unsigned long sp_flags;

	if (pci_data->irq_enabled == 0)
		return IRQ_HANDLED;

	_rtw_spinlock_irq(&pci_data->irq_th_lock, &sp_flags);
	if (rtw_phl_recognize_interrupt(dvobj->phl)) {
		pstatus = rtw_phl_interrupt_handler(dvobj->phl);
	}
	_rtw_spinunlock_irq(&pci_data->irq_th_lock, &sp_flags);

	if (pstatus == RTW_PHL_STATUS_FAILURE)
		return IRQ_HANDLED;
	/* return IRQ_NONE; */

	return IRQ_HANDLED;
}

#if defined(RTK_DMP_PLATFORM) || defined(CONFIG_PLATFORM_RTL8197D)
	#define pci_iounmap(x, y) iounmap(y)
#endif

int pci_alloc_irq(struct dvobj_priv *dvobj)
{
	int err;
	PPCI_DATA pci_data = dvobj_to_pci(dvobj);
	struct pci_dev	*pdev = pci_data->ppcidev;
	int ret;

#ifndef CONFIG_RTW_PCI_MSI_DISABLE
	ret = pci_enable_msi(pdev);

	RTW_INFO("pci_enable_msi ret=%d\n", ret);
#endif

#if defined(IRQF_SHARED)
	err = request_irq(pdev->irq, &rtw_pci_interrupt, IRQF_SHARED, DRV_NAME, dvobj);
#else
	err = request_irq(pdev->irq, &rtw_pci_interrupt, SA_SHIRQ, DRV_NAME, dvobj);
#endif
	if (err)
		RTW_INFO("Error allocating IRQ %d", pdev->irq);
	else {
		pci_data->irq_alloc = 1;
		pci_data->irq = pdev->irq;
		RTW_INFO("Request_irq OK, IRQ %d\n", pdev->irq);
	}

	return err ? _FAIL : _SUCCESS;
}

static struct dvobj_priv *pci_dvobj_init(struct pci_dev *pdev,
					const struct pci_device_id *pdid)
{
	int err;
	u32	status = _FAIL;
	struct dvobj_priv *dvobj = NULL;
	struct pci_priv	*pcipriv = NULL;
	struct pci_dev	*bridge_pdev = pdev->bus->self;
	/* u32	pci_cfg_space[16]; */
	unsigned long pmem_start, pmem_len, pmem_flags;
	int	i;
	PPCI_DATA pci_data;

	dvobj = devobj_init();
	if (dvobj == NULL)
		goto exit;

	pci_data = dvobj_to_pci(dvobj);

	pci_data->ppcidev = pdev;
	pcipriv = &(pci_data->pcipriv);
	pci_set_drvdata(pdev, dvobj);


	err = pci_enable_device(pdev);
	if (err != 0) {
		RTW_ERR("%s : Cannot enable new PCI device\n", pci_name(pdev));
		goto free_dvobj;
	}

#ifdef CONFIG_64BIT_DMA
	if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(64))) {
		RTW_INFO("RTL819xCE: Using 64bit DMA\n");
		err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
		if (err != 0) {
			RTW_ERR("Unable to obtain 64bit DMA for consistent allocations\n");
			goto disable_picdev;
		}
		pci_data->bdma64 = _TRUE;
	} else
#endif
	{
		if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(32))) {
			err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
			if (err != 0) {
				RTW_ERR("Unable to obtain 32bit DMA for consistent allocations\n");
				goto disable_picdev;
			}
		}
	}

	pci_set_master(pdev);

	err = pci_request_regions(pdev, DRV_NAME);
	if (err != 0) {
		RTW_ERR("Can't obtain PCI resources\n");
		goto disable_picdev;
	}

#ifdef RTK_129X_PLATFORM
	if (pdev->bus->number == 0x00) {
		pmem_start = PCIE_SLOT1_MEM_START;
		pmem_len   = PCIE_SLOT1_MEM_LEN;
		pmem_flags = 0;
		RTW_PRINT("RTD129X: PCIE SLOT1\n");
	} else if (pdev->bus->number == 0x01) {
		pmem_start = PCIE_SLOT2_MEM_START;
		pmem_len   = PCIE_SLOT2_MEM_LEN;
		pmem_flags = 0;
		RTW_PRINT("RTD129X: PCIE SLOT2\n");
	} else {
		RTW_ERR(KERN_ERR "RTD129X: Wrong Slot Num\n");
		goto release_regions;
	}
#else
	/* Search for memory map resource (index 0~5) */
	for (i = 0 ; i < 6 ; i++) {
		pmem_start = pci_resource_start(pdev, i);
		pmem_len = pci_resource_len(pdev, i);
		pmem_flags = pci_resource_flags(pdev, i);

		if (pmem_flags & IORESOURCE_MEM)
			break;
	}

	if (i == 6) {
		RTW_ERR("%s: No MMIO resource found, abort!\n", __func__);
		goto release_regions;
	}
#endif /* RTK_DMP_PLATFORM */

#ifdef RTK_DMP_PLATFORM
	pci_data->pci_mem_start = (unsigned long)ioremap_nocache(pmem_start, pmem_len);
#elif defined(RTK_129X_PLATFORM)
	if (pdev->bus->number == 0x00)
		pci_data->ctrl_start =
			(unsigned long)ioremap(PCIE_SLOT1_CTRL_START, 0x200);
	else if (pdev->bus->number == 0x01)
		pci_data->ctrl_start =
			(unsigned long)ioremap(PCIE_SLOT2_CTRL_START, 0x200);

	if (pci_data->ctrl_start == 0) {
		RTW_ERR("RTD129X: Can't map CTRL mem\n");
		goto release_regions;
	}

	pci_data->mask_addr = pci_data->ctrl_start + PCIE_MASK_OFFSET;
	pci_data->tran_addr = pci_data->ctrl_start + PCIE_TRANSLATE_OFFSET;

	pci_data->pci_mem_start =
		(unsigned long)ioremap_nocache(pmem_start, pmem_len);
#else
	/* shared mem start */
	pci_data->pci_mem_start = (unsigned long)pci_iomap(pdev, i, pmem_len);
#endif
	if (pci_data->pci_mem_start == 0) {
		RTW_ERR("Can't map PCI mem\n");
		goto release_regions;
	}

	RTW_INFO("Memory mapped space start: 0x%08lx len:%08lx flags:%08lx, after map:0x%08lx\n",
		 pmem_start, pmem_len, pmem_flags, pci_data->pci_mem_start);

#if 0
	/* Read PCI configuration Space Header */
	for (i = 0; i < 16; i++)
		pci_read_config_dword(pdev, (i << 2), &pci_cfg_space[i]);
#endif

	/*step 1-1., decide the chip_type via device info*/
	dvobj->interface_type = RTW_HCI_PCIE;
	dvobj->ic_id = pdid->driver_data;
	dvobj->intf_ops = &pci_ops;

	/* rtw_pci_parse_configuration(pdev, dvobj, (u8 *)&pci_cfg_space); */
	if (rtw_pci_parse_configuration(pdev, dvobj) == _FAIL) {
		RTW_ERR("PCI parse configuration error\n");
		goto iounmap;
	}

	if (bridge_pdev) {
		pci_read_config_byte(bridge_pdev,
				     bridge_pdev->pcie_cap + PCI_EXP_LNKCTL,
				     &pcipriv->pcibridge_linkctrlreg);

		if (bridge_pdev->vendor == AMD_VENDOR_ID)
			pcipriv->amd_l1_patch = rtw_pci_get_amd_l1_patch(dvobj, bridge_pdev);
	}

	status = _SUCCESS;

iounmap:
	if (status != _SUCCESS && pci_data->pci_mem_start != 0) {
#if 1/* def RTK_DMP_PLATFORM */
		pci_iounmap(pdev, (void *)pci_data->pci_mem_start);
#endif
		pci_data->pci_mem_start = 0;
	}

#ifdef RTK_129X_PLATFORM
	if (status != _SUCCESS && pci_data->ctrl_start != 0) {
		pci_iounmap(pdev, (void *)pci_data->ctrl_start);
		pci_data->ctrl_start = 0;
	}
#endif

release_regions:
	if (status != _SUCCESS)
		pci_release_regions(pdev);
disable_picdev:
	if (status != _SUCCESS)
		pci_disable_device(pdev);
free_dvobj:
	if (status != _SUCCESS && dvobj) {
		pci_set_drvdata(pdev, NULL);
		devobj_deinit(dvobj);
		dvobj = NULL;
	}
exit:
	return dvobj;
}


static void pci_dvobj_deinit(struct pci_dev *pdev)
{
	struct dvobj_priv *dvobj = pci_get_drvdata(pdev);
	PPCI_DATA pci_data = dvobj_to_pci(dvobj);

	pci_set_drvdata(pdev, NULL);
	if (dvobj) {
		if (pci_data->irq_alloc) {
			free_irq(pdev->irq, dvobj);
#ifndef CONFIG_RTW_PCI_MSI_DISABLE
			pci_disable_msi(pdev);
#endif
			pci_data->irq_alloc = 0;
		}

		if (pci_data->pci_mem_start != 0) {
#if 1/* def RTK_DMP_PLATFORM */
			pci_iounmap(pdev, (void *)pci_data->pci_mem_start);
#endif
			pci_data->pci_mem_start = 0;
		}

#ifdef RTK_129X_PLATFORM
		if (pci_data->ctrl_start != 0) {
			pci_iounmap(pdev, (void *)pci_data->ctrl_start);
			pci_data->ctrl_start = 0;
		}
#endif
		devobj_deinit(dvobj);
	}

	pci_release_regions(pdev);
	pci_disable_device(pdev);

}



/*GEORGIA_TODO_FIXIT-FOR Multi-ICs*/
static void disable_ht_for_spec_devid(const struct pci_device_id *pdid)
{
#ifdef CONFIG_80211N_HT
	u16 vid, pid;
	u32 flags;
	int i;
	int num = sizeof(specific_device_id_tbl) / sizeof(struct specific_device_id);

	for (i = 0; i < num; i++) {
		vid = specific_device_id_tbl[i].idVendor;
		pid = specific_device_id_tbl[i].idProduct;
		flags = specific_device_id_tbl[i].flags;

		if ((pdid->vendor == vid) && (pdid->device == pid) && (flags & SPEC_DEV_ID_DISABLE_HT)) {
			rtw_ht_enable = 0;
			rtw_bw_mode = 0;
			rtw_ampdu_enable = 0;
		}

	}
#endif
}

#ifdef CONFIG_PM
static int rtw_pci_suspend(struct pci_dev *pdev, pm_message_t state)
{
	int ret = 0;
	struct dvobj_priv *dvobj = pci_get_drvdata(pdev);
	_adapter *padapter = dvobj_get_primary_adapter(dvobj);

	ret = rtw_suspend_common(padapter);
	ret = pci_save_state(pdev);
	if (ret != 0) {
		RTW_INFO("%s Failed on pci_save_state (%d)\n", __func__, ret);
		goto exit;
	}

#ifdef CONFIG_WOWLAN
	device_set_wakeup_enable(&pdev->dev, true);
#endif
	pci_disable_device(pdev);

#ifdef CONFIG_WOWLAN
	ret = pci_enable_wake(pdev, pci_choose_state(pdev, state), true);
	if (ret != 0)
		RTW_INFO("%s Failed on pci_enable_wake (%d)\n", __func__, ret);
#endif
	ret = pci_set_power_state(pdev, pci_choose_state(pdev, state));
	if (ret != 0)
		RTW_INFO("%s Failed on pci_set_power_state (%d)\n", __func__, ret);

exit:
	return ret;

}

static int rtw_resume_process(_adapter *padapter)
{
	return rtw_resume_common(padapter);
}

static int rtw_pci_resume(struct pci_dev *pdev)
{
	struct dvobj_priv *dvobj = pci_get_drvdata(pdev);
	_adapter *padapter = dvobj_get_primary_adapter(dvobj);
	struct net_device *pnetdev = padapter->pnetdev;
	struct pwrctrl_priv *pwrpriv = dvobj_to_pwrctl(dvobj);
	int	err = 0;

	err = pci_set_power_state(pdev, PCI_D0);
	if (err != 0) {
		RTW_INFO("%s Failed on pci_set_power_state (%d)\n", __func__, err);
		goto exit;
	}

	err = pci_enable_device(pdev);
	if (err != 0) {
		RTW_INFO("%s Failed on pci_enable_device (%d)\n", __func__, err);
		goto exit;
	}


#ifdef CONFIG_WOWLAN
	err =  pci_enable_wake(pdev, PCI_D0, 0);
	if (err != 0) {
		RTW_INFO("%s Failed on pci_enable_wake (%d)\n", __func__, err);
		goto exit;
	}
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 37))
	pci_restore_state(pdev);
#else
	err = pci_restore_state(pdev);
	if (err != 0) {
		RTW_INFO("%s Failed on pci_restore_state (%d)\n", __func__, err);
		goto exit;
	}
#endif

#ifdef CONFIG_WOWLAN
	device_set_wakeup_enable(&pdev->dev, false);
#endif

	if (pwrpriv->wowlan_mode || pwrpriv->wowlan_ap_mode) {
		rtw_resume_lock_suspend();
		err = rtw_resume_process(padapter);
		rtw_resume_unlock_suspend();
	} else {
#ifdef CONFIG_RESUME_IN_WORKQUEUE
		rtw_resume_in_workqueue(pwrpriv);
#else
		if (rtw_is_earlysuspend_registered(pwrpriv)) {
			/* jeff: bypass resume here, do in late_resume */
			rtw_set_do_late_resume(pwrpriv, _TRUE);
		} else {
			rtw_resume_lock_suspend();
			err = rtw_resume_process(padapter);
			rtw_resume_unlock_suspend();
		}
#endif
	}

exit:

	return err;
}
#endif/* CONFIG_PM */

_adapter *rtw_pci_primary_adapter_init(struct dvobj_priv *dvobj, struct pci_dev *pdev)
{
	_adapter *padapter = NULL;
	int status = _FAIL;
	u8 hw_mac_addr[ETH_ALEN] = {0};

	padapter = (_adapter *)rtw_zvmalloc(sizeof(*padapter));
	if (padapter == NULL)
		goto exit;

	/*registry_priv*/
	if (rtw_load_registry(padapter) != _SUCCESS)
		goto free_adapter;

	padapter->dvobj = dvobj;

	dvobj->padapters[dvobj->iface_nums++] = padapter;
	padapter->iface_id = IFACE_ID0;

	/* set adapter_type/iface type for primary padapter */
	padapter->isprimary = _TRUE;
	padapter->adapter_type = PRIMARY_ADAPTER;

	if (rtw_init_drv_sw(padapter) == _FAIL)
		goto free_adapter;

	/* get mac addr */
	rtw_hw_get_mac_addr(dvobj, hw_mac_addr);

	rtw_macaddr_cfg(adapter_mac_addr(padapter), hw_mac_addr);

	status = _SUCCESS;

free_adapter:
	if (status != _SUCCESS && padapter) {
		rtw_vmfree((u8 *)padapter, sizeof(*padapter));
		padapter = NULL;
	}
exit:
	return padapter;
}

static void rtw_pci_primary_adapter_deinit(_adapter *padapter)
{
	rtw_free_drv_sw(padapter);

	/* TODO: use rtw_os_ndevs_deinit instead at the first stage of driver's dev deinit function */
	rtw_os_ndev_free(padapter);

	rtw_vmfree((u8 *)padapter, sizeof(_adapter));
}

/*
 * drv_init() - a device potentially for us
 *
 * notes: drv_init() is called when the bus driver has located a card for us to support.
 *        We accept the new device by returning 0.
*/
static int rtw_dev_probe(struct pci_dev *pdev, const struct pci_device_id *pdid)
{
	_adapter *padapter = NULL;
	struct dvobj_priv *dvobj;

	RTW_INFO("+%s\n", __func__);

	/* step 0. */
	disable_ht_for_spec_devid(pdid);

	/* Initialize dvobj_priv */
	dvobj = pci_dvobj_init(pdev, pdid);
	if (dvobj == NULL) {
		RTW_ERR("pci_dvobj_init Failed!\n");
		goto exit;
	}

	if (devobj_trx_resource_init(dvobj) == _FAIL)
		goto free_dvobj;

	/*init hw - register and get chip-info */
	if (rtw_hw_init(dvobj) == _FAIL) {
		RTW_ERR("rtw_hw_init Failed!\n");
		goto free_trx_reso;
	}

	/* Initialize primary adapter */
	padapter = rtw_pci_primary_adapter_init(dvobj, pdev);
	if (padapter == NULL) {
		RTW_ERR("rtw_pci_primary_adapter_init Failed!\n");
		goto free_hw;
	}

	/* Initialize virtual interface */
#ifdef CONFIG_CONCURRENT_MODE
	if (rtw_drv_add_vir_ifaces(dvobj) == _FAIL)
		goto free_if_vir;
#endif

	/*init data of dvobj from registary and ic spec*/
	if (devobj_data_init(dvobj) == _FAIL) {
		RTW_ERR("devobj_data_init Failed!\n");
		goto free_devobj_data;
	}

#ifdef CONFIG_GLOBAL_UI_PID
	if (ui_pid[1] != 0) {
		RTW_INFO("ui_pid[1]:%d\n", ui_pid[1]);
		rtw_signal_process(ui_pid[1], SIGUSR2);
	}
#endif

	/* dev_alloc_name && register_netdev */
	if (rtw_os_ndevs_init(dvobj) != _SUCCESS) {
		RTW_ERR("rtw_os_ndevs_init Failed!\n");
		goto free_devobj_data;
	}

#ifdef CONFIG_HOSTAPD_MLME
	hostapd_mode_init(padapter);
#endif

	/* alloc irq */
	if (pci_alloc_irq(dvobj) != _SUCCESS) {
		RTW_ERR("pci_alloc_irq Failed!\n");
		goto os_ndevs_deinit;
	}

	RTW_INFO("-%s success\n", __func__);
	return 0; /* _SUCCESS;*/


os_ndevs_deinit:
	rtw_os_ndevs_deinit(dvobj);

free_devobj_data:
	devobj_data_deinit(dvobj);

free_if_vir:
#ifdef CONFIG_CONCURRENT_MODE
	rtw_drv_stop_vir_ifaces(dvobj);
	rtw_drv_free_vir_ifaces(dvobj);
#endif
	rtw_pci_primary_adapter_deinit(padapter);

free_hw:
	rtw_hw_deinit(dvobj);

free_trx_reso:
	devobj_trx_resource_deinit(dvobj);

free_dvobj:
	pci_dvobj_deinit(pdev);

exit:
	return -ENODEV;
}

/*
 * dev_remove() - our device is being removed
*/
/* rmmod module & unplug(SurpriseRemoved) will call r871xu_dev_remove() => how to recognize both */
static void rtw_dev_remove(struct pci_dev *pdev)
{
	struct dvobj_priv *dvobj = pci_get_drvdata(pdev);
	_adapter *padapter = dvobj_get_primary_adapter(dvobj);
	struct net_device *pnetdev = padapter->pnetdev;

	if (dvobj->processing_dev_remove == _TRUE) {
		RTW_WARN("%s-line%d: Warning! device has been removed!\n", __func__, __LINE__);
		return;
	}

	RTW_INFO("+%s\n", __func__);

	dvobj->processing_dev_remove = _TRUE;

	if (unlikely(!padapter))
		return;

	/* TODO: use rtw_os_ndevs_deinit instead at the first stage of driver's dev deinit function */
	rtw_os_ndevs_unregister(dvobj);

#if defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_ANDROID_POWER)
	rtw_unregister_early_suspend(dvobj_to_pwrctl(dvobj));
#endif
#if 0 /*GEORGIA_TODO_FIXIT*/
	if (GET_PHL_COM(pdvobjpriv)->fw_ready == _TRUE) {
		rtw_pm_set_ips(padapter, IPS_NONE);
		rtw_pm_set_lps(padapter, PM_PS_MODE_ACTIVE);

		LeaveAllPowerSaveMode(padapter);
	}
#endif
	dev_set_drv_stopped(adapter_to_dvobj(padapter));	/*for stop thread*/
#if 0 /*#ifdef CONFIG_CORE_CMD_THREAD*/
	rtw_stop_cmd_thread(padapter);
#endif
#ifdef CONFIG_CONCURRENT_MODE
	rtw_drv_stop_vir_ifaces(dvobj);
#endif
	rtw_pci_dynamic_aspm_set_mode(padapter, ASPM_MODE_DEF);

	rtw_drv_stop_prim_iface(padapter);

	rtw_hw_stop(dvobj);
	dev_set_surprise_removed(dvobj);

	rtw_pci_primary_adapter_deinit(padapter);

#ifdef CONFIG_CONCURRENT_MODE
	rtw_drv_free_vir_ifaces(dvobj);
#endif
	rtw_hw_deinit(dvobj);
	devobj_data_deinit(dvobj);
	devobj_trx_resource_deinit(dvobj);
	pci_dvobj_deinit(pdev);

	RTW_INFO("-%s done\n", __func__);
	return;
}

static void rtw_dev_shutdown(struct pci_dev *pdev)
{
	rtw_dev_remove(pdev);
}

#ifdef CONFIG_PLATFORM_AML_S905
extern struct device *get_pcie_reserved_mem_dev(void);
struct device * g_pcie_reserved_mem_dev;
#endif

static int __init rtw_drv_entry(void)
{
	int ret = 0;

	RTW_PRINT("module init start\n");

#ifdef CONFIG_PLATFORM_AML_S905
#ifdef USE_AML_PCIE_TEE_MEM
	g_pcie_reserved_mem_dev = get_pcie_reserved_mem_dev();
	if (g_pcie_reserved_mem_dev)
		RTW_PRINT("#######use amlogic pcie TEE protect mem#######\n");
#endif
#endif

	dump_drv_version(RTW_DBGDUMP);
#ifdef BTCOEXVERSION
	RTW_PRINT(DRV_NAME" BT-Coex version = %s\n", BTCOEXVERSION);
#endif /* BTCOEXVERSION */

#if (defined(CONFIG_RTKM) && defined(CONFIG_RTKM_BUILT_IN))
	ret = rtkm_prealloc_init();
	if (ret) {
		RTW_INFO("%s: pre-allocate memory failed!!(%d)\n", __FUNCTION__,
			 ret);
		goto exit;
	}
#endif /* CONFIG_RTKM */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24))
	/* console_suspend_enabled=0; */
#endif

	pci_drvpriv.drv_registered = _TRUE;
	rtw_suspend_lock_init();
	rtw_drv_proc_init();
	rtw_nlrtw_init();
	rtw_ndev_notifier_register();
	rtw_inetaddr_notifier_register();

	ret = pci_register_driver(&pci_drvpriv.rtw_pci_drv);

	if (ret != 0) {
		pci_drvpriv.drv_registered = _FALSE;
		rtw_suspend_lock_uninit();
		rtw_drv_proc_deinit();
		rtw_nlrtw_deinit();
		rtw_ndev_notifier_unregister();
		rtw_inetaddr_notifier_unregister();
		goto exit;
	}

exit:
	RTW_PRINT("module init ret=%d\n", ret);
	return ret;
}

static void __exit rtw_drv_halt(void)
{
	RTW_PRINT("module exit start\n");

	pci_drvpriv.drv_registered = _FALSE;

	pci_unregister_driver(&pci_drvpriv.rtw_pci_drv);

	rtw_suspend_lock_uninit();
	rtw_drv_proc_deinit();
	rtw_nlrtw_deinit();
	rtw_ndev_notifier_unregister();
	rtw_inetaddr_notifier_unregister();

	RTW_PRINT("module exit success\n");

	rtw_mstat_dump(RTW_DBGDUMP);

#if (defined(CONFIG_RTKM) && defined(CONFIG_RTKM_BUILT_IN))
	rtkm_prealloc_destroy();
#elif (defined(CONFIG_RTKM) && defined(CONFIG_RTKM_STANDALONE))
	rtkm_dump_mstatus(RTW_DBGDUMP);
#endif /* CONFIG_RTKM */
}


module_init(rtw_drv_entry);
module_exit(rtw_drv_halt);
