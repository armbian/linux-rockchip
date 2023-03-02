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
#define _PCI_OPS_LINUX_C_

#include <drv_types.h>

#ifdef RTK_129X_PLATFORM
#include <soc/realtek/rtd129x_lockapi.h>

#define IO_2K_MASK 0xFFFFF800
#define IO_4K_MASK 0xFFFFF000
#define MAX_RETRY 5

u32 pci_io_read_129x(struct dvobj_priv *pdvobjpriv, u32 addr, u8 size)
{
	unsigned long mask_addr = pdvobjpriv->mask_addr;
	unsigned long tran_addr = pdvobjpriv->tran_addr;
	PPCI_DATA pci_data = dvobj_to_pci(pdvobjpriv);
	u8 busnumber = pdvobjpriv->pcipriv.busnumber;
	u32 rval = 0;
	u32 mask;
	u32 translate_val = 0;
	u32 tmp_addr = addr & 0xFFF;
	u32 pci_error_status = 0;
	int retry_cnt = 0;
	unsigned long flags, sp_flags;

	_rtw_spinlock_irq(&pdvobjpriv->io_reg_lock, &sp_flags);

	/* PCIE1.1 0x9804FCEC, PCIE2.0 0x9803CCEC & 0x9803CC68
	 * can't be used because of 1295 hardware issue.
	 */
	if ((tmp_addr == 0xCEC) || ((busnumber == 0x01) &&
	    (tmp_addr == 0xC68))) {
		mask = IO_2K_MASK;
		writel(0xFFFFF800, (u8 *)mask_addr);
		translate_val = readl((u8 *)tran_addr);
		writel(translate_val|(addr&mask), (u8 *)tran_addr);
	} else if (addr >= 0x1000) {
		mask = IO_4K_MASK;
		translate_val = readl((u8 *)tran_addr);
		writel(translate_val|(addr&mask), (u8 *)tran_addr);
	} else
		mask = 0x0;

pci_read_129x_retry:

	/* All RBUS1 driver need to have a workaround for emmc hardware error */
	/* Need to protect 0xXXXX_X8XX~ 0xXXXX_X9XX */
	if ((tmp_addr > 0x7FF) && (tmp_addr < 0xA00))
		rtk_lockapi_lock(flags, __func__);

	switch (size) {
	case 1:
		rval = readb((u8 *)pci_data->pci_mem_start + (addr&~mask));
		break;
	case 2:
		rval = readw((u8 *)pci_data->pci_mem_start + (addr&~mask));
		break;
	case 4:
		rval = readl((u8 *)pci_data->pci_mem_start + (addr&~mask));
		break;
	default:
		RTW_WARN("RTD129X: %s: wrong size %d\n", __func__, size);
		break;
	}

	if ((tmp_addr > 0x7FF) && (tmp_addr < 0xA00))
		rtk_lockapi_unlock(flags, __func__);

	//DLLP error patch
	pci_error_status = readl( (u8 *)(pdvobjpriv->ctrl_start + 0x7C));
	if(pci_error_status & 0x1F) {
		writel(pci_error_status, (u8 *)(pdvobjpriv->ctrl_start + 0x7C));
		RTW_WARN("RTD129X: %s: DLLP(#%d) 0x%x reg=0x%x val=0x%x\n",
			__func__, retry_cnt, pci_error_status, addr, rval);

		if(retry_cnt < MAX_RETRY) {
			retry_cnt++;
			goto pci_read_129x_retry;
		}
	}

	/* PCIE1.1 0x9804FCEC, PCIE2.0 0x9803CCEC & 0x9803CC68
	 * can't be used because of 1295 hardware issue.
	 */
	if ((tmp_addr == 0xCEC) || ((busnumber == 0x01) &&
	    (tmp_addr == 0xC68))) {
		writel(translate_val, (u8 *)tran_addr);
		writel(0xFFFFF000, (u8 *)mask_addr);
	} else if (addr >= 0x1000) {
		writel(translate_val, (u8 *)tran_addr);
	}

	_rtw_spinunlock_irq(&pdvobjpriv->io_reg_lock, &sp_flags);

	return rval;
}

void pci_io_write_129x(struct dvobj_priv *pdvobjpriv,
			      u32 addr, u8 size, u32 wval)
{
	unsigned long mask_addr = pdvobjpriv->mask_addr;
	unsigned long tran_addr = pdvobjpriv->tran_addr;
	PPCI_DATA pci_data = dvobj_to_pci(pdvobjpriv);
	u8 busnumber = pdvobjpriv->pcipriv.busnumber;
	u32 mask;
	u32 translate_val = 0;
	u32 tmp_addr = addr & 0xFFF;
	unsigned long sp_flags;

	_rtw_spinlock_irq(&pdvobjpriv->io_reg_lock, &sp_flags);

	/* PCIE1.1 0x9804FCEC, PCIE2.0 0x9803CCEC & 0x9803CC68
	 * can't be used because of 1295 hardware issue.
	 */
	if ((tmp_addr == 0xCEC) || ((busnumber == 0x01) &&
	    (tmp_addr == 0xC68))) {
		mask = IO_2K_MASK;
		writel(0xFFFFF800, (u8 *)mask_addr);
		translate_val = readl((u8 *)tran_addr);
		writel(translate_val|(addr&mask), (u8 *)tran_addr);
	} else if (addr >= 0x1000) {
		mask = IO_4K_MASK;
		translate_val = readl((u8 *)tran_addr);
		writel(translate_val|(addr&mask), (u8 *)tran_addr);
	} else
		mask = 0x0;

	/* All RBUS1 driver need to have a workaround for emmc hardware error */
	/* Need to protect 0xXXXX_X8XX~ 0xXXXX_X9XX */
	if ((tmp_addr > 0x7FF) && (tmp_addr < 0xA00))
		rtk_lockapi_lock(flags, __func__);

	switch (size) {
	case 1:
		writeb((u8)wval,
		       (u8 *)pci_data->pci_mem_start + (addr&~mask));
		break;
	case 2:
		writew((u16)wval,
		       (u8 *)pci_data->pci_mem_start + (addr&~mask));
		break;
	case 4:
		writel((u32)wval,
		       (u8 *)pci_data->pci_mem_start + (addr&~mask));
		break;
	default:
		RTW_WARN("RTD129X: %s: wrong size %d\n", __func__, size);
		break;
	}

	if ((tmp_addr > 0x7FF) && (tmp_addr < 0xA00))
		rtk_lockapi_unlock(flags, __func__);

	/* PCIE1.1 0x9804FCEC, PCIE2.0 0x9803CCEC & 0x9803CC68
	 * can't be used because of 1295 hardware issue.
	 */
	if ((tmp_addr == 0xCEC) || ((busnumber == 0x01) &&
	    (tmp_addr == 0xC68))) {
		writel(translate_val, (u8 *)tran_addr);
		writel(0xFFFFF000, (u8 *)mask_addr);
	} else if (addr >= 0x1000) {
		writel(translate_val, (u8 *)tran_addr);
	}

	_rtw_spinunlock_irq(&pdvobjpriv->io_reg_lock, &sp_flags);
}

u8 os_pci_read8(struct dvobj_priv *dvobj, u32 addr)
{
	return (u8)pci_io_read_129x(dvobj, addr, 1);
}

u16 os_pci_read16(struct dvobj_priv *dvobj, u32 addr)
{
	return (u16)pci_io_read_129x(dvobj, addr, 2);
}

u32 os_pci_read32(struct dvobj_priv *dvobj, u32 addr)
{
	return (u32)pci_io_read_129x(dvobj, addr, 4);
}

/*
 * 2009.12.23. by tynli. Suggested by SD1 victorh.
 * For ASPM hang on AMD and Nvidia.
 * 20100212 Tynli: Do read IO operation after write for
 * all PCI bridge suggested by SD1. Origianally this is only for INTEL.
 */
static int os_pci_write8(struct dvobj_priv *dvobj, u32 addr, u8 val)
{
	pci_io_write_129x(dvobj, addr, 1, val);
	return 1;
}

static int os_pci_write16(struct dvobj_priv *dvobj, u32 addr, u16 val)
{
	pci_io_write_129x(dvobj, addr, 2, val);
	return 2;
}

int os_pci_write32(struct dvobj_priv *dvobj, u32 addr, u32 val)
{
	pci_io_write_129x(dvobj, addr, 4, val);
	return 4;
}

#elif defined (RTK_1319_PLATFORM)
#include <soc/realtek/rtk_pcie.h>

#ifndef RTK_1319_PCIE_PORT
	#define RTK_1319_PCIE_PORT 2
#endif

u8 os_pci_read8(struct dvobj_priv *dvobj, u32 addr)
{
#if (1 == RTK_1319_PCIE_PORT)
	return (u8)rtk_pcie2_13xx_read(addr, 1);
#elif (2 == RTK_1319_PCIE_PORT)
	return (u8)rtk_pcie3_13xx_read(addr, 1);
#endif
}

u16 os_pci_read16(struct dvobj_priv *dvobj, u32 addr)
{
#if (1 == RTK_1319_PCIE_PORT)
	return (u16)rtk_pcie2_13xx_read(addr, 2);
#elif (2 == RTK_1319_PCIE_PORT)
	return (u16)rtk_pcie3_13xx_read(addr, 2);
#endif
}

u32 os_pci_read32(struct dvobj_priv *dvobj, u32 addr)
{
#if (1 == RTK_1319_PCIE_PORT)
	return (u32)rtk_pcie2_13xx_read(addr, 4);
#elif (2 == RTK_1319_PCIE_PORT)
	return (u32)rtk_pcie3_13xx_read(addr, 4);
#endif
}

int os_pci_write8(struct dvobj_priv *dvobj, u32 addr, u8 val)
{
#if (1 == RTK_1319_PCIE_PORT)
	rtk_pcie2_13xx_write(addr, 1, val);
#elif (2 == RTK_1319_PCIE_PORT)
	rtk_pcie3_13xx_write(addr, 1, val);
#endif
	return 1;
}

int os_pci_write16(struct dvobj_priv *dvobj, u32 addr, u16 val)
{
#if (1 == RTK_1319_PCIE_PORT)
	rtk_pcie2_13xx_write(addr, 2, val);
#elif (2 == RTK_1319_PCIE_PORT)
	rtk_pcie3_13xx_write(addr, 2, val);
#endif
	return 2;
}

int os_pci_write32(struct dvobj_priv *dvobj, u32 addr, u32 val)
{
#if (1 == RTK_1319_PCIE_PORT)
	rtk_pcie2_13xx_write(addr, 4, val);
#elif (2 == RTK_1319_PCIE_PORT)
	rtk_pcie3_13xx_write(addr, 4, val);
#endif
	return 4;
}

#else /* original*/

u8 os_pci_read8(struct dvobj_priv *dvobj, u32 addr)
{
	return 0xff & readb((u8 *)dvobj_to_pci(dvobj)->pci_mem_start + addr);
}

u16 os_pci_read16(struct dvobj_priv *dvobj, u32 addr)
{
	return readw((u8 *)dvobj_to_pci(dvobj)->pci_mem_start + addr);
}

u32 os_pci_read32(struct dvobj_priv *dvobj, u32 addr)
{
	return readl((u8 *)dvobj_to_pci(dvobj)->pci_mem_start + addr);
}

/*
 * 2009.12.23. by tynli. Suggested by SD1 victorh.
 * For ASPM hang on AMD and Nvidia.
 * 20100212 Tynli: Do read IO operation after write for
 * all PCI bridge suggested by SD1. Origianally this is only for INTEL.
 */
int os_pci_write8(struct dvobj_priv *dvobj, u32 addr, u8 val)
{
	writeb(val, (u8 *)dvobj_to_pci(dvobj)->pci_mem_start + addr);
	return 1;
}

int os_pci_write16(struct dvobj_priv *dvobj, u32 addr, u16 val)
{
	writew(val, (u8 *)dvobj_to_pci(dvobj)->pci_mem_start + addr);
	return 2;
}

int os_pci_write32(struct dvobj_priv *dvobj, u32 addr, u32 val)
{
	writel(val, (u8 *)dvobj_to_pci(dvobj)->pci_mem_start + addr);
	return 4;
}
#endif
 

