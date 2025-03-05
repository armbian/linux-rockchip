/*
 * Linux DHD Bus Module for PCIE
 *
 * Copyright (C) 2022, Broadcom.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id$
 */

/* include files */
#include <typedefs.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <bcmdevs_legacy.h>    /* need to still support chips no longer in trunk firmware */
#include <siutils.h>
#include <hndsoc.h>
#include <hndpmu.h>
#include <sbchipc.h>
#if defined(DHD_DEBUG)
#include <hnd_armtrap.h>
#include <hnd_cons.h>
#endif /* defined(DHD_DEBUG) */
#include <dngl_stats.h>
#include <pcie_core.h>
#include <dhd.h>
#include <dhd_bus.h>
#include <dhd_proto.h>
#include <dhd_dbg.h>
#include <dhdioctl.h>
#include <bcmmsgbuf.h>
#include <pcicfg.h>
#include <dhd_pcie.h>
#include <dhd_linux.h>
#if IS_ENABLED(CONFIG_PCIEASPM_ROCKCHIP_WIFI_EXTENSION)
#include <rk_dhd_pcie_linux.h>
#endif /* CONFIG_PCIEASPM_ROCKCHIP_WIFI_EXTENSION */
#ifdef OEM_ANDROID
#ifdef CONFIG_ARCH_MSM
#if IS_ENABLED(CONFIG_PCI_MSM) || defined(CONFIG_ARCH_MSM8996)
#include <linux/msm_pcie.h>
#else
#include <mach/msm_pcie.h>
#endif /* CONFIG_PCI_MSM */
#endif /* CONFIG_ARCH_MSM */
#endif /* OEM_ANDROID */

#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
#include <linux/pm_runtime.h>

#ifndef AUTO_SUSPEND_TIMEOUT
#define AUTO_SUSPEND_TIMEOUT 1000
#endif /* AUTO_SUSPEND_TIMEOUT */
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

#include <linux/irq.h>
#ifdef USE_SMMU_ARCH_MSM
#include <asm/dma-iommu.h>
#include <linux/iommu.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#endif /* USE_SMMU_ARCH_MSM */
#include <dhd_config.h>

#ifdef PCIE_OOB
#include "ftdi_sio_external.h"
#endif /* PCIE_OOB */

#if defined(WL_CFG80211)
#include <wl_cfg80211.h>
#endif	/* WL_CFG80211 */

#include <dhd_plat.h>

#ifdef CONFIG_ARCH_ROCKCHIP
#include <linux/aspm_ext.h>
#endif

#define PCI_CFG_RETRY		10		/* PR15065: retry count for pci cfg accesses */
#define OS_HANDLE_MAGIC		0x1234abcd	/* Magic # to recognize osh */
#define BCM_MEM_FILENAME_LEN	24		/* Mem. filename length */

#ifdef PCIE_OOB
#define HOST_WAKE 4   /* GPIO_0 (HOST_WAKE) - Output from WLAN */
#define DEVICE_WAKE 5  /* GPIO_1 (DEVICE_WAKE) - Input to WLAN */
#define BIT_WL_REG_ON 6
#define BIT_BT_REG_ON 7

int gpio_handle_val = 0;
unsigned char gpio_port = 0;
unsigned char gpio_direction = 0;
#define OOB_PORT "ttyUSB0"
#endif /* PCIE_OOB */

#ifndef BCMPCI_DEV_ID
#define BCMPCI_DEV_ID PCI_ANY_ID
#endif

#ifndef SYNAPCI_DEV_ID
#define SYNAPCI_DEV_ID PCI_ANY_ID
#endif

#ifdef FORCE_TPOWERON
extern uint32 tpoweron_scale;
#endif /* FORCE_TPOWERON */
/* user defined data structures  */

typedef bool (*dhdpcie_cb_fn_t)(void *);

typedef struct dhdpcie_info
{
	dhd_bus_t	*bus;
	osl_t		*osh;
	struct pci_dev  *dev;		/* pci device handle */
	volatile char	*regs;		/* pci device memory va */
	volatile char	*tcm;		/* pci device memory va */
	uint32		bar1_size;	/* pci device memory size */
	struct pcos_info *pcos_info;
	uint16		last_intrstatus;	/* to cache intrstatus */
	int	irq;
	char pciname[32];
	struct pci_saved_state* default_state;
	struct pci_saved_state* state;
#ifdef BCMPCIE_OOB_HOST_WAKE
	void *os_cxt;			/* Pointer to per-OS private data */
#endif /* BCMPCIE_OOB_HOST_WAKE */
#ifdef DHD_WAKE_STATUS
	spinlock_t	pkt_wake_lock;
	unsigned int	total_wake_count;
	int		pkt_wake;
	int		wake_irq;
	int		pkt_wake_dump;
#endif /* DHD_WAKE_STATUS */
#ifdef USE_SMMU_ARCH_MSM
	void *smmu_cxt;
#endif /* USE_SMMU_ARCH_MSM */
} dhdpcie_info_t;

struct pcos_info {
	dhdpcie_info_t *pc;
	spinlock_t lock;
	wait_queue_head_t intr_wait_queue;
	timer_list_compat_t tuning_timer;
	int tuning_timer_exp;
	atomic_t timer_enab;
	struct tasklet_struct tuning_tasklet;
};

#ifdef BCMPCIE_OOB_HOST_WAKE
typedef struct dhdpcie_os_info {
	int			oob_irq_num;	/* valid when hardware or software oob in use */
	unsigned long		oob_irq_flags;	/* valid when hardware or software oob in use */
	bool			oob_irq_registered;
	bool			oob_irq_enabled;
	bool			oob_irq_wake_enabled;
	spinlock_t		oob_irq_spinlock;
	void			*dev;		/* handle to the underlying device */
} dhdpcie_os_info_t;
static irqreturn_t wlan_oob_irq(int irq, void *data);
#endif /* BCMPCIE_OOB_HOST_WAKE */

#ifdef USE_SMMU_ARCH_MSM
typedef struct dhdpcie_smmu_info {
	struct dma_iommu_mapping *smmu_mapping;
	dma_addr_t smmu_iova_start;
	size_t smmu_iova_len;
} dhdpcie_smmu_info_t;
#endif /* USE_SMMU_ARCH_MSM */

/* function declarations */
static int __devinit
dhdpcie_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static void __devexit dhdpcie_pci_remove(struct pci_dev *pdev);
static void __devexit dhdpcie_pci_shutdown(struct pci_dev *pdev);
static void __devexit dhdpcie_pci_stop(struct pci_dev *pdev);
static int dhdpcie_init(struct pci_dev *pdev);
static irqreturn_t dhdpcie_isr(int irq, void *arg);
/* OS Routine functions for PCI suspend/resume */

#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
static int dhdpcie_set_suspend_resume(struct pci_dev *dev, bool state, bool byint);
#else
static int dhdpcie_set_suspend_resume(dhd_bus_t *bus, bool state);
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */
static int dhdpcie_resume_host_dev(dhd_bus_t *bus);
static int dhdpcie_suspend_host_dev(dhd_bus_t *bus);
static int dhdpcie_resume_dev(struct pci_dev *dev);
static int dhdpcie_suspend_dev(struct pci_dev *dev);
#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
static int dhdpcie_pm_system_suspend_noirq(struct device * dev);
static int dhdpcie_pm_system_resume_noirq(struct device * dev);
#else
static int dhdpcie_pci_suspend(struct device * dev);
static int dhdpcie_pci_resume(struct device * dev);
static int dhdpcie_pci_resume_early(struct device * dev);
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
static int dhdpcie_pm_runtime_suspend(struct device * dev);
static int dhdpcie_pm_runtime_resume(struct device * dev);
static int dhdpcie_pm_system_suspend_noirq(struct device * dev);
static int dhdpcie_pm_system_resume_noirq(struct device * dev);
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

static void dhdpcie_config_save_restore_coherent(dhd_bus_t *bus, bool state);

uint32
dhdpcie_access_cap(struct pci_dev *pdev, int cap, uint offset, bool is_ext, bool is_write,
	uint32 writeval);

static struct pci_device_id dhdpcie_pci_devid[] __devinitdata = {
	{ vendor: VENDOR_BROADCOM,
	device: BCMPCI_DEV_ID,
	subvendor: PCI_ANY_ID,
	subdevice: PCI_ANY_ID,
	class: PCI_CLASS_NETWORK_OTHER << 8,
	class_mask: 0xffff00,
	driver_data: 0,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0))
	override_only: 0,
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)) */
	},
	{ vendor: VENDOR_SYNAPTICS,
	device: BCMPCI_DEV_ID,
	subvendor: PCI_ANY_ID,
	subdevice: PCI_ANY_ID,
	class: PCI_CLASS_NETWORK_OTHER << 8,
	class_mask: 0xffff00,
	driver_data: 0,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0))
	override_only: 0,
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)) */
	},
#if (BCMPCI_DEV_ID != PCI_ANY_ID) && defined(BCMPCI_NOOTP_DEV_ID)
	{ vendor: VENDOR_BROADCOM,
	device: BCMPCI_NOOTP_DEV_ID,
	subvendor: PCI_ANY_ID,
	subdevice: PCI_ANY_ID,
	class: PCI_CLASS_NETWORK_OTHER << 8,
	class_mask: 0xffff00,
	driver_data: 0,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0))
	override_only: 0,
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)) */
	},
#endif /* BCMPCI_DEV_ID != PCI_ANY_ID && BCMPCI_NOOTP_DEV_ID */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0))
	{ 0, 0, 0, 0, 0, 0, 0, 0}
#else
	{ 0, 0, 0, 0, 0, 0, 0}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)) */
};
MODULE_DEVICE_TABLE(pci, dhdpcie_pci_devid);

/* Power Management Hooks */
#if defined(DHD_PCIE_NATIVE_RUNTIMEPM)
static const struct dev_pm_ops dhdpcie_pm_ops = {
	SET_RUNTIME_PM_OPS(dhdpcie_pm_runtime_suspend, dhdpcie_pm_runtime_resume, NULL)
	.suspend_noirq = dhdpcie_pm_system_suspend_noirq,
	.resume_noirq = dhdpcie_pm_system_resume_noirq
};
#else
/* for linux platforms */
static const struct dev_pm_ops dhd_pcie_pm_ops = {
	.suspend = dhdpcie_pci_suspend,
	.resume	= dhdpcie_pci_resume,
	.resume_early = dhdpcie_pci_resume_early,
};
#endif

static struct pci_driver dhdpcie_driver = {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0))
	node:		{&dhdpcie_driver.node, &dhdpcie_driver.node},
#endif /* LINUX_VERSION_CODE < 6.8.0 */
	name:		"pcieh"BUS_TYPE,
	id_table:	dhdpcie_pci_devid,
	probe:		dhdpcie_pci_probe,
	remove:		dhdpcie_pci_remove,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0))
	.driver = {.pm = &dhd_pcie_pm_ops, .coredump = NULL},
#else
	.driver = {.pm = &dhd_pcie_pm_ops, },
#endif /* LINUX_VERSION_CODE >= 4.16.0 */
	shutdown:	dhdpcie_pci_shutdown,
};

int dhdpcie_init_succeeded = FALSE;

#ifdef USE_SMMU_ARCH_MSM
static int dhdpcie_smmu_init(struct pci_dev *pdev, void *smmu_cxt)
{
	struct dma_iommu_mapping *mapping;
	struct device_node *root_node = NULL;
	dhdpcie_smmu_info_t *smmu_info = (dhdpcie_smmu_info_t *)smmu_cxt;
	int smmu_iova_address[2];
	char *wlan_node = "android,bcmdhd_wlan";
	char *wlan_smmu_node = "wlan-smmu-iova-address";
	int atomic_ctx = 1;
	int s1_bypass = 1;
	int ret = 0;

	DHD_ERROR(("%s: SMMU initialize\n", __FUNCTION__));

	root_node = of_find_compatible_node(NULL, NULL, wlan_node);
	if (!root_node) {
		WARN(1, "failed to get device node of BRCM WLAN\n");
		return -ENODEV;
	}

	if (of_property_read_u32_array(root_node, wlan_smmu_node,
		smmu_iova_address, 2) == 0) {
		DHD_ERROR(("%s : get SMMU start address 0x%x, size 0x%x\n",
			__FUNCTION__, smmu_iova_address[0], smmu_iova_address[1]));
		smmu_info->smmu_iova_start = smmu_iova_address[0];
		smmu_info->smmu_iova_len = smmu_iova_address[1];
	} else {
		DHD_CONS_ONLY(("%s : can't get smmu iova address property\n",
			__FUNCTION__));
		return -ENODEV;
	}

	if (smmu_info->smmu_iova_len <= 0) {
		DHD_ERROR(("%s: Invalid smmu iova len %d\n",
			__FUNCTION__, (int)smmu_info->smmu_iova_len));
		return -EINVAL;
	}

	DHD_ERROR(("%s : SMMU init start\n", __FUNCTION__));

	if (pci_set_dma_mask(pdev, DMA_BIT_MASK(64)) ||
		pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64))) {
		DHD_ERROR(("%s: DMA set 64bit mask failed.\n", __FUNCTION__));
		return -EINVAL;
	}

	mapping = arm_iommu_create_mapping(&platform_bus_type,
		smmu_info->smmu_iova_start, smmu_info->smmu_iova_len);
	if (IS_ERR(mapping)) {
		DHD_ERROR(("%s: create mapping failed, err = %d\n",
			__FUNCTION__, ret));
		ret = PTR_ERR(mapping);
		goto map_fail;
	}

	ret = iommu_domain_set_attr(mapping->domain,
		DOMAIN_ATTR_ATOMIC, &atomic_ctx);
	if (ret) {
		DHD_ERROR(("%s: set atomic_ctx attribute failed, err = %d\n",
			__FUNCTION__, ret));
		goto set_attr_fail;
	}

	ret = iommu_domain_set_attr(mapping->domain,
		DOMAIN_ATTR_S1_BYPASS, &s1_bypass);
	if (ret < 0) {
		DHD_ERROR(("%s: set s1_bypass attribute failed, err = %d\n",
			__FUNCTION__, ret));
		goto set_attr_fail;
	}

	ret = arm_iommu_attach_device(&pdev->dev, mapping);
	if (ret) {
		DHD_ERROR(("%s: attach device failed, err = %d\n",
			__FUNCTION__, ret));
		goto attach_fail;
	}

	smmu_info->smmu_mapping = mapping;

	return ret;

attach_fail:
set_attr_fail:
	arm_iommu_release_mapping(mapping);
map_fail:
	return ret;
}

static void dhdpcie_smmu_remove(struct pci_dev *pdev, void *smmu_cxt)
{
	dhdpcie_smmu_info_t *smmu_info;

	if (!smmu_cxt) {
		return;
	}

	smmu_info = (dhdpcie_smmu_info_t *)smmu_cxt;
	if (smmu_info->smmu_mapping) {
		arm_iommu_detach_device(&pdev->dev);
		arm_iommu_release_mapping(smmu_info->smmu_mapping);
		smmu_info->smmu_mapping = NULL;
	}
}
#endif /* USE_SMMU_ARCH_MSM */

#ifdef FORCE_TPOWERON
static void
dhd_bus_get_tpoweron(dhd_bus_t *bus)
{

	uint32 tpoweron_rc;
	uint32 tpoweron_ep;

	tpoweron_rc = dhdpcie_rc_access_cap(bus, PCIE_EXTCAP_ID_L1SS,
		PCIE_EXTCAP_L1SS_CONTROL2_OFFSET, TRUE, FALSE, 0);
	tpoweron_ep = dhdpcie_ep_access_cap(bus, PCIE_EXTCAP_ID_L1SS,
		PCIE_EXTCAP_L1SS_CONTROL2_OFFSET, TRUE, FALSE, 0);
	DHD_ERROR(("%s: tpoweron_rc:0x%x tpoweron_ep:0x%x\n",
		__FUNCTION__, tpoweron_rc, tpoweron_ep));
}

static void
dhd_bus_set_tpoweron(dhd_bus_t *bus, uint16 tpoweron)
{

	dhd_bus_get_tpoweron(bus);
	/* Set the tpoweron */
	DHD_ERROR(("%s tpoweron: 0x%x\n", __FUNCTION__, tpoweron));
	dhdpcie_rc_access_cap(bus, PCIE_EXTCAP_ID_L1SS,
		PCIE_EXTCAP_L1SS_CONTROL2_OFFSET, TRUE, TRUE, tpoweron);
	dhdpcie_ep_access_cap(bus, PCIE_EXTCAP_ID_L1SS,
		PCIE_EXTCAP_L1SS_CONTROL2_OFFSET, TRUE, TRUE, tpoweron);

	dhd_bus_get_tpoweron(bus);

}

static bool
dhdpcie_chip_req_forced_tpoweron(dhd_bus_t *bus)
{
	/*
	 * On Fire's reference platform, coming out of L1.2,
	 * there is a constant delay of 45us between CLKREQ# and stable REFCLK
	 * Due to this delay, with tPowerOn < 50
	 * there is a chance of the refclk sense to trigger on noise.
	 *
	 * Which ever chip needs forced tPowerOn of 50us should be listed below.
	 */
	if (si_chipid(bus->sih) == BCM4377_CHIP_ID) {
		return TRUE;
	}
	return FALSE;
}
#endif /* FORCE_TPOWERON */

static bool
dhd_bus_aspm_enable_dev(dhd_bus_t *bus, struct pci_dev *dev, bool enable)
{
	uint32 linkctrl_before;
	uint32 linkctrl_after = 0;
	uint8 linkctrl_aspm;
	char *device;

	device = (dev == bus->dev) ? "EP" : "RC";

	linkctrl_before = dhdpcie_access_cap(dev, PCIE_CAP_ID_EXP, PCIE_CAP_LINKCTRL_OFFSET,
		FALSE, FALSE, 0);
	linkctrl_aspm = (linkctrl_before & PCIE_ASPM_CTRL_MASK);

	if (enable) {
		if (linkctrl_aspm == PCIE_ASPM_L1_ENAB) {
			DHD_ERROR(("%s: %s already enabled  linkctrl: 0x%x\n",
				__FUNCTION__, device, linkctrl_before));
			return FALSE;
		}
		/* Enable only L1 ASPM (bit 1) */
		dhdpcie_access_cap(dev, PCIE_CAP_ID_EXP, PCIE_CAP_LINKCTRL_OFFSET, FALSE,
			TRUE, (linkctrl_before | PCIE_ASPM_L1_ENAB));
	} else {
		if (linkctrl_aspm == 0) {
			DHD_ERROR(("%s: %s already disabled linkctrl: 0x%x\n",
				__FUNCTION__, device, linkctrl_before));
			return FALSE;
		}
		/* Disable complete ASPM (bit 1 and bit 0) */
		dhdpcie_access_cap(dev, PCIE_CAP_ID_EXP, PCIE_CAP_LINKCTRL_OFFSET, FALSE,
			TRUE, (linkctrl_before & (~PCIE_ASPM_ENAB)));
	}

	linkctrl_after = dhdpcie_access_cap(dev, PCIE_CAP_ID_EXP, PCIE_CAP_LINKCTRL_OFFSET,
		FALSE, FALSE, 0);
	DHD_ERROR(("%s: %s %s, linkctrl_before: 0x%x linkctrl_after: 0x%x\n",
		__FUNCTION__, device, (enable ? "ENABLE " : "DISABLE"),
		linkctrl_before, linkctrl_after));

	return TRUE;
}

static bool
dhd_bus_is_aspm_enab_dev(dhd_bus_t *bus, struct pci_dev *dev)
{
	uint32 linkctrl = dhdpcie_access_cap(dev, PCIE_CAP_ID_EXP,
		PCIE_CAP_LINKCTRL_OFFSET, FALSE, FALSE, 0);

	DHD_INFO(("%s: %s: linkctrl:0x%x\n",
		__FUNCTION__, (dev == bus->dev) ? "EP" : "RC", linkctrl));

	return ((linkctrl & PCIE_ASPM_CTRL_MASK) == PCIE_ASPM_L1_ENAB);
}

bool
dhd_bus_is_aspm_enab_rc_ep(dhd_bus_t *bus)
{
	uint32 rc_aspm_enab;
	uint32 ep_aspm_enab;

	rc_aspm_enab = dhd_bus_is_aspm_enab_dev(bus, bus->rc_dev);
	ep_aspm_enab = dhd_bus_is_aspm_enab_dev(bus, bus->dev);

	return (rc_aspm_enab && ep_aspm_enab);
}

static bool
dhd_bus_is_rc_ep_aspm_capable(dhd_bus_t *bus)
{
	uint32 rc_aspm_cap;
	uint32 ep_aspm_cap;

	/* RC ASPM capability */
	rc_aspm_cap = dhdpcie_access_cap(bus->rc_dev, PCIE_CAP_ID_EXP, PCIE_CAP_LINKCTRL_OFFSET,
		FALSE, FALSE, 0);
	if (rc_aspm_cap == BCME_ERROR) {
		DHD_ERROR(("%s RC is not ASPM capable\n", __FUNCTION__));
		return FALSE;
	}

	/* EP ASPM capability */
	ep_aspm_cap = dhdpcie_access_cap(bus->dev, PCIE_CAP_ID_EXP, PCIE_CAP_LINKCTRL_OFFSET,
		FALSE, FALSE, 0);
	if (ep_aspm_cap == BCME_ERROR) {
		DHD_ERROR(("%s EP is not ASPM capable\n", __FUNCTION__));
		return FALSE;
	}

	return TRUE;
}

bool
dhd_bus_aspm_enable_rc_ep(dhd_bus_t *bus, bool enable)
{
	bool ret;

	if (!bus->rc_ep_aspm_cap) {
		DHD_ERROR(("%s: NOT ASPM  CAPABLE rc_ep_aspm_cap: %d\n",
			__FUNCTION__, bus->rc_ep_aspm_cap));
		return FALSE;
	}

	if (enable) {
		/* Enable only L1 ASPM first RC then EP */
		ret = dhd_bus_aspm_enable_dev(bus, bus->rc_dev, enable);
		ret = dhd_bus_aspm_enable_dev(bus, bus->dev, enable);
	} else {
		/* Disable complete ASPM first EP then RC */
		ret = dhd_bus_aspm_enable_dev(bus, bus->dev, enable);
		ret = dhd_bus_aspm_enable_dev(bus, bus->rc_dev, enable);
	}

	return ret;
}

static bool
dhd_bus_is_l1ss_enab_dev(dhd_bus_t *bus, struct pci_dev *dev)
{
	uint32 l1ssctrl;

	/* Extendend Capacility Reg */
	l1ssctrl = dhdpcie_access_cap(dev, PCIE_EXTCAP_ID_L1SS,
		PCIE_EXTCAP_L1SS_CONTROL_OFFSET, TRUE, FALSE, 0);

	DHD_INFO(("%s: %s: l1ssctrl:0x%x\n",
		__FUNCTION__, (dev == bus->dev) ? "EP" : "RC", l1ssctrl));

	return ((l1ssctrl & PCIE_EXT_L1SS_MASK) == PCIE_EXT_L1SS_ENAB);
}

bool
dhd_bus_is_l1ss_enab_rc_ep(dhd_bus_t *bus)
{
	uint32 rc_l1ss_enab;
	uint32 ep_l1ss_enab;

	rc_l1ss_enab = dhd_bus_is_l1ss_enab_dev(bus, bus->rc_dev);
	ep_l1ss_enab = dhd_bus_is_l1ss_enab_dev(bus, bus->dev);

	return (rc_l1ss_enab && ep_l1ss_enab);
}

static void
dhd_bus_l1ss_enable_dev(dhd_bus_t *bus, struct pci_dev *dev, bool enable)
{
	uint32 l1ssctrl_before;
	uint32 l1ssctrl_after = 0;
	uint8 l1ss_ep;
	char *device;

	device = (dev == bus->dev) ? "EP" : "RC";

	/* Extendend Capacility Reg */
	l1ssctrl_before = dhdpcie_access_cap(dev, PCIE_EXTCAP_ID_L1SS,
		PCIE_EXTCAP_L1SS_CONTROL_OFFSET, TRUE, FALSE, 0);
	l1ss_ep = (l1ssctrl_before & PCIE_EXT_L1SS_MASK);

	if (enable) {
		if (l1ss_ep == PCIE_EXT_L1SS_ENAB) {
			DHD_ERROR(("%s: %s already enabled,  l1ssctrl: 0x%x\n",
				__FUNCTION__, device, l1ssctrl_before));
			return;
		}
		dhdpcie_access_cap(dev, PCIE_EXTCAP_ID_L1SS, PCIE_EXTCAP_L1SS_CONTROL_OFFSET,
			TRUE, TRUE, (l1ssctrl_before | PCIE_EXT_L1SS_ENAB));
	} else {
		if (l1ss_ep == 0) {
			DHD_ERROR(("%s: %s already disabled, l1ssctrl: 0x%x\n",
				__FUNCTION__, device, l1ssctrl_before));
			return;
		}
		dhdpcie_access_cap(dev, PCIE_EXTCAP_ID_L1SS, PCIE_EXTCAP_L1SS_CONTROL_OFFSET,
			TRUE, TRUE, (l1ssctrl_before & (~PCIE_EXT_L1SS_ENAB)));
	}
	l1ssctrl_after = dhdpcie_access_cap(dev, PCIE_EXTCAP_ID_L1SS,
		PCIE_EXTCAP_L1SS_CONTROL_OFFSET, TRUE, FALSE, 0);
	DHD_ERROR(("%s: %s %s, l1ssctrl_before: 0x%x l1ssctrl_after: 0x%x\n",
		__FUNCTION__, device, (enable ? "ENABLE " : "DISABLE"),
		l1ssctrl_before, l1ssctrl_after));

}

static bool
dhd_bus_is_rc_ep_l1ss_capable(dhd_bus_t *bus)
{
	uint32 rc_l1ss_cap;
	uint32 ep_l1ss_cap;

#if IS_ENABLED(CONFIG_PCIEASPM_ROCKCHIP_WIFI_EXTENSION)
	if (rk_dhd_bus_is_rc_ep_l1ss_capable(bus)) {
		DHD_ERROR(("%s L1ss is capable\n", __FUNCTION__));
		return TRUE;
	} else {
		DHD_ERROR(("%s L1ss is not capable\n", __FUNCTION__));
		return FALSE;
	}
#endif

	/* RC Extendend Capacility */
	rc_l1ss_cap = dhdpcie_access_cap(bus->rc_dev, PCIE_EXTCAP_ID_L1SS,
		PCIE_EXTCAP_L1SS_CONTROL_OFFSET, TRUE, FALSE, 0);
	if (rc_l1ss_cap == BCME_ERROR) {
		DHD_ERROR(("%s RC is not l1ss capable\n", __FUNCTION__));
		return FALSE;
	}

	/* EP Extendend Capacility */
	ep_l1ss_cap = dhdpcie_access_cap(bus->dev, PCIE_EXTCAP_ID_L1SS,
		PCIE_EXTCAP_L1SS_CONTROL_OFFSET, TRUE, FALSE, 0);
	if (ep_l1ss_cap == BCME_ERROR) {
		DHD_ERROR(("%s EP is not l1ss capable\n", __FUNCTION__));
		return FALSE;
	}

	return TRUE;
}

void
dhd_bus_l1ss_enable_rc_ep(dhd_bus_t *bus, bool enable)
{
	bool ret;

	if ((!bus->rc_ep_aspm_cap) || (!bus->rc_ep_l1ss_cap)) {
		DHD_ERROR(("%s: NOT L1SS CAPABLE rc_ep_aspm_cap: %d rc_ep_l1ss_cap: %d\n",
			__FUNCTION__, bus->rc_ep_aspm_cap, bus->rc_ep_l1ss_cap));
		return;
	}

	/* Disable ASPM of RC and EP */
	ret = dhd_bus_aspm_enable_rc_ep(bus, FALSE);

	if (enable) {
		/* Enable RC then EP */
		dhd_bus_l1ss_enable_dev(bus, bus->rc_dev, enable);
		dhd_bus_l1ss_enable_dev(bus, bus->dev, enable);
	} else {
		/* Disable EP then RC */
		dhd_bus_l1ss_enable_dev(bus, bus->dev, enable);
		dhd_bus_l1ss_enable_dev(bus, bus->rc_dev, enable);
	}

	/* Enable ASPM of RC and EP only if this API disabled */
	if (ret == TRUE) {
		dhd_bus_aspm_enable_rc_ep(bus, TRUE);
	}
}

void
dhd_bus_aer_config(dhd_bus_t *bus)
{
	uint32 val;

	DHD_ERROR_MEM(("%s: Configure AER registers for EP\n", __FUNCTION__));
	val = dhdpcie_ep_access_cap(bus, PCIE_ADVERRREP_CAPID,
		PCIE_ADV_CORR_ERR_MASK_OFFSET, TRUE, FALSE, 0);
	if (val != (uint32)-1) {
		val &= ~CORR_ERR_AE;
		dhdpcie_ep_access_cap(bus, PCIE_ADVERRREP_CAPID,
			PCIE_ADV_CORR_ERR_MASK_OFFSET, TRUE, TRUE, val);
	} else {
		DHD_ERROR(("%s: Invalid EP's PCIE_ADV_CORR_ERR_MASK: 0x%x\n",
			__FUNCTION__, val));
	}

	DHD_ERROR_MEM(("%s: Configure AER registers for RC\n", __FUNCTION__));
	val = dhdpcie_rc_access_cap(bus, PCIE_ADVERRREP_CAPID,
		PCIE_ADV_CORR_ERR_MASK_OFFSET, TRUE, FALSE, 0);
	if (val != (uint32)-1) {
		val &= ~CORR_ERR_AE;
		dhdpcie_rc_access_cap(bus, PCIE_ADVERRREP_CAPID,
			PCIE_ADV_CORR_ERR_MASK_OFFSET, TRUE, TRUE, val);
	} else {
		DHD_ERROR(("%s: Invalid RC's PCIE_ADV_CORR_ERR_MASK: 0x%x\n",
			__FUNCTION__, val));
	}
}

static int dhdpcie_pci_suspend(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pdev = to_pci_dev(dev);
	dhdpcie_info_t *pch = pci_get_drvdata(pdev);
	dhd_bus_t *bus = NULL;
	unsigned long flags;
	int timeleft = 0;
	uint bitmask = 0xFFFFFFFF;

	printf("%s: Enter\n", __FUNCTION__);
	if (pch) {
		bus = pch->bus;
	}
	if (!bus) {
		return ret;
	}

#if defined(DEVICE_TX_STUCK_DETECT) && defined(ASSOC_CHECK_SR)
	dhd_assoc_check_sr(bus->dhd, TRUE);
#endif /* DEVICE_TX_STUCK_DETECT && ASSOC_CHECK_SR */
	DHD_GENERAL_LOCK(bus->dhd, flags);
	if (!DHD_BUS_BUSY_CHECK_IDLE(bus->dhd)) {
		DHD_BUS_BUSY_SET_SUSPEND_IN_PROGRESS(bus->dhd);
		DHD_GENERAL_UNLOCK(bus->dhd, flags);
		DHD_ERROR(("%s: wait to clear dhd_bus_busy_state: 0x%x\n",
			__FUNCTION__, bus->dhd->dhd_bus_busy_state));
		timeleft = dhd_os_busbusy_wait_bitmask(bus->dhd,
				&bus->dhd->dhd_bus_busy_state, bitmask,
				DHD_BUS_BUSY_SUSPEND_IN_PROGRESS);
		if ((timeleft == 0) || (timeleft == 1)) {
			DHD_ERROR(("%s: Timed out dhd_bus_busy_state=0x%x\n",
				__FUNCTION__, bus->dhd->dhd_bus_busy_state));
			ret = -EBUSY;
			goto exit;
		}
	} else {
		DHD_BUS_BUSY_SET_SUSPEND_IN_PROGRESS(bus->dhd);
		DHD_GENERAL_UNLOCK(bus->dhd, flags);
	}

#ifdef DHD_CFG80211_SUSPEND_RESUME
	dhd_cfg80211_suspend(bus->dhd);
#endif /* DHD_CFG80211_SUSPEND_RESUME */

	if (!bus->dhd->dongle_reset)
		ret = dhdpcie_set_suspend_resume(bus, TRUE);

exit:
	DHD_GENERAL_LOCK(bus->dhd, flags);
	DHD_BUS_BUSY_CLEAR_SUSPEND_IN_PROGRESS(bus->dhd);
	dhd_os_busbusy_wake(bus->dhd);
	printf("%s: Exit ret=%d\n", __FUNCTION__, ret);
	DHD_GENERAL_UNLOCK(bus->dhd, flags);

	return ret;
}

static int dhdpcie_pci_resume_early(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pdev = to_pci_dev(dev);
	dhdpcie_info_t *pch = pci_get_drvdata(pdev);
	dhd_bus_t *bus = NULL;
	uint32 pmcsr;

	if (pch) {
		bus = pch->bus;
	}
	if (!bus) {
		return ret;
	}
	if (bus->dhd->busstate == DHD_BUS_DOWN) {
		return ret;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 9))
	/* On fc30 (linux ver 5.0.9),
	 *  PMEStat of PMCSR(cfg reg) is cleared before this callback by kernel.
	 *  So, we use SwPme of FunctionControl(enum reg) instead of PMEStat without kernel change.
	 */
	if (bus->sih->buscorerev >= 64) {
		uint32 ftnctrl;
		volatile void *regsva = (volatile void *)bus->regs;

		ftnctrl = pcie_corereg(bus->osh, regsva,
				OFFSETOF(sbpcieregs_t, ftn_ctrl.control), 0, 0);
		pmcsr = OSL_PCI_READ_CONFIG(bus->osh, PCIE_CFG_PMCSR, sizeof(pmcsr));

		DHD_ERROR(("%s(): pmcsr is 0x%x, ftnctrl is 0x%8x \r\n",
			__FUNCTION__, pmcsr, ftnctrl));
		if (ftnctrl & PCIE_FTN_SWPME_MASK) {
			DHD_ERROR(("%s(): Wakeup due to WLAN \r\n", __FUNCTION__));
		}
	} else
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 9)) */
	{
		pmcsr = OSL_PCI_READ_CONFIG(bus->osh, PCIE_CFG_PMCSR, sizeof(pmcsr));

		DHD_ERROR(("%s(): pmcsr is 0x%x \r\n", __FUNCTION__, pmcsr));
		if (pmcsr & PCIE_PMCSR_PMESTAT) {
			DHD_ERROR(("%s(): Wakeup due to WLAN \r\n", __FUNCTION__));
		}
	}

	/*
	 * TODO: Add code to take adavantage of what is read from pmcsr
	 */

	return ret;
}

static int dhdpcie_pci_resume(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pdev = to_pci_dev(dev);
	dhdpcie_info_t *pch = pci_get_drvdata(pdev);
	dhd_bus_t *bus = NULL;
	unsigned long flags;

	printf("%s: Enter\n", __FUNCTION__);
	if (pch) {
		bus = pch->bus;
	}
	if (!bus) {
		return ret;
	}

	DHD_GENERAL_LOCK(bus->dhd, flags);
	DHD_BUS_BUSY_SET_RESUME_IN_PROGRESS(bus->dhd);
	DHD_GENERAL_UNLOCK(bus->dhd, flags);

	if (!bus->dhd->dongle_reset)
		ret = dhdpcie_set_suspend_resume(bus, FALSE);

	DHD_GENERAL_LOCK(bus->dhd, flags);
	DHD_BUS_BUSY_CLEAR_RESUME_IN_PROGRESS(bus->dhd);
	dhd_os_busbusy_wake(bus->dhd);
	printf("%s: Exit ret=%d\n", __FUNCTION__, ret);
	DHD_GENERAL_UNLOCK(bus->dhd, flags);
#if defined(DEVICE_TX_STUCK_DETECT) && defined(ASSOC_CHECK_SR)
	dhd_assoc_check_sr(bus->dhd, FALSE);
#endif /* DEVICE_TX_STUCK_DETECT && ASSOC_CHECK_SR */

#ifdef WL_TWT
	dhd_config_twt_event_mask_in_suspend(bus->dhd, FALSE);
	dhd_send_twt_info_suspend(bus->dhd, FALSE);
#endif /* WL_TWT */

#ifdef DHD_CFG80211_SUSPEND_RESUME
	dhd_cfg80211_resume(bus->dhd);
#endif /* DHD_CFG80211_SUSPEND_RESUME */
	return ret;
}

static int
#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
dhdpcie_set_suspend_resume(dhd_bus_t *bus, bool state, bool byint)
#else
dhdpcie_set_suspend_resume(dhd_bus_t *bus, bool state)
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */
{
	int ret = 0;

	ASSERT(bus && !bus->dhd->dongle_reset);

	/* When firmware is not loaded do the PCI bus */
	/* suspend/resume only */
	if (bus->dhd->busstate == DHD_BUS_DOWN) {
		ret = dhdpcie_pci_suspend_resume(bus, state);
		return ret;
	}
#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
	ret = dhdpcie_bus_suspend(bus, state, byint);
#else
	ret = dhdpcie_bus_suspend(bus, state);
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0) && defined(DHD_TCP_LIMIT_OUTPUT)
	if (ret == BCME_OK) {
		/*
		 * net.ipv4.tcp_limit_output_bytes is used for all ipv4 sockets
		 * so, returning back to original value when there is no traffic(suspend)
		 */
		if (state == TRUE) {
			dhd_ctrl_tcp_limit_output_bytes(0);
		} else {
			dhd_ctrl_tcp_limit_output_bytes(1);
		}
	}
#endif /* LINUX_VERSION_CODE > 4.19.0 && DHD_TCP_LIMIT_OUTPUT */

	return ret;
}

#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
static int dhdpcie_pm_runtime_suspend(struct device * dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	dhdpcie_info_t *pch = pci_get_drvdata(pdev);
	dhd_bus_t *bus = NULL;
	int ret = 0;

	if (!pch)
		return -EBUSY;

	bus = pch->bus;

	DHD_RPM(("%s Enter\n", __FUNCTION__));

	if (atomic_read(&bus->dhd->block_bus))
		return -EHOSTDOWN;

	dhd_netif_stop_queue(bus);
	atomic_set(&bus->dhd->block_bus, TRUE);

	if (dhdpcie_set_suspend_resume(pdev, TRUE, TRUE)) {
		pm_runtime_mark_last_busy(dev);
		ret = -EAGAIN;
	}

	atomic_set(&bus->dhd->block_bus, FALSE);
	dhd_bus_start_queue(bus);

	return ret;
}

static int dhdpcie_pm_runtime_resume(struct device * dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	dhdpcie_info_t *pch = pci_get_drvdata(pdev);
	dhd_bus_t *bus = pch->bus;

	DHD_RPM(("%s Enter\n", __FUNCTION__));

	if (atomic_read(&bus->dhd->block_bus))
		return -EHOSTDOWN;

	if (dhdpcie_set_suspend_resume(pdev, FALSE, TRUE))
		return -EAGAIN;

	return 0;
}

static int dhdpcie_pm_system_suspend_noirq(struct device * dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	dhdpcie_info_t *pch = pci_get_drvdata(pdev);
	dhd_bus_t *bus = NULL;
	int ret;

	DHD_RPM(("%s Enter\n", __FUNCTION__));

	if (!pch)
		return -EBUSY;

	bus = pch->bus;

	if (atomic_read(&bus->dhd->block_bus))
		return -EHOSTDOWN;

	dhd_netif_stop_queue(bus);
	atomic_set(&bus->dhd->block_bus, TRUE);

	ret = dhdpcie_set_suspend_resume(pdev, TRUE, FALSE);

	if (ret) {
		dhd_bus_start_queue(bus);
		atomic_set(&bus->dhd->block_bus, FALSE);
	}

	return ret;
}

static int dhdpcie_pm_system_resume_noirq(struct device * dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	dhdpcie_info_t *pch = pci_get_drvdata(pdev);
	dhd_bus_t *bus = NULL;
	int ret;

	if (!pch)
		return -EBUSY;

	bus = pch->bus;

	DHD_RPM(("%s Enter\n", __FUNCTION__));

	ret = dhdpcie_set_suspend_resume(pdev, FALSE, FALSE);

	atomic_set(&bus->dhd->block_bus, FALSE);
	dhd_bus_start_queue(bus);
	pm_runtime_mark_last_busy(dhd_bus_to_dev(bus));

	return ret;
}
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

#if defined(OEM_ANDROID) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0))
extern void dhd_dpc_tasklet_kill(dhd_pub_t *dhdp);
#endif /* OEM_ANDROID && LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0) */

static void
dhdpcie_suspend_dump_cfgregs(struct dhd_bus *bus, char *suspend_state)
{
	DHD_RPM(("%s: BaseAddress0(0x%x)=0x%x, "
		"BaseAddress1(0x%x)=0x%x PCIE_CFG_PMCSR(0x%x)=0x%x "
		"PCI_BAR1_WIN(0x%x)=(0x%x)\n",
		suspend_state,
		PCIECFGREG_BASEADDR0,
		dhd_pcie_config_read(bus,
			PCIECFGREG_BASEADDR0, sizeof(uint32)),
		PCIECFGREG_BASEADDR1,
		dhd_pcie_config_read(bus,
			PCIECFGREG_BASEADDR1, sizeof(uint32)),
		PCIE_CFG_PMCSR,
		dhd_pcie_config_read(bus,
			PCIE_CFG_PMCSR, sizeof(uint32)),
		PCI_BAR1_WIN,
		dhd_pcie_config_read(bus,
			PCI_BAR1_WIN, sizeof(uint32))));
}

static int dhdpcie_suspend_dev(struct pci_dev *dev)
{
	int ret;
	dhdpcie_info_t *pch = pci_get_drvdata(dev);
	dhd_bus_t *bus = pch->bus;

#if defined(OEM_ANDROID) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0))
	if (bus->is_linkdown) {
		DHD_ERROR(("%s: PCIe link is down\n", __FUNCTION__));
		return BCME_ERROR;
	}
#endif /* OEM_ANDROID && LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0) */

	DHD_RPM(("%s: Enter\n", __FUNCTION__));

	/*
	 * Disable L1ss on EP and RC side ... defaults to NOP
	 * If needed implement this function in the dhd_custom_xxx.c
	 * accordingly.
	 */
	dhd_plat_l1ss_ctrl(0);

	dhdpcie_suspend_dump_cfgregs(bus, "BEFORE_EP_SUSPEND");
#if defined(OEM_ANDROID) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0))
	dhd_dpc_tasklet_kill(bus->dhd);
#endif /* OEM_ANDROID && LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0) */
	pci_save_state(dev);
#if defined(OEM_ANDROID) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0))
	pch->state = pci_store_saved_state(dev);
#endif /* OEM_ANDROID && LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0) */
	pci_enable_wake(dev, PCI_D0, TRUE);
	if (pci_is_enabled(dev))
		pci_disable_device(dev);

	ret = pci_set_power_state(dev, PCI_D3hot);
	if (ret) {
		DHD_ERROR(("%s: pci_set_power_state error %d\n",
			__FUNCTION__, ret));
	}
#ifdef OEM_ANDROID
//	dev->state_saved = FALSE;
#endif /* OEM_ANDROID */
	dhdpcie_suspend_dump_cfgregs(bus, "AFTER_EP_SUSPEND");
	return ret;
}

#ifdef DHD_WAKE_STATUS
int bcmpcie_get_total_wake(struct dhd_bus *bus)
{
	dhdpcie_info_t *pch = pci_get_drvdata(bus->dev);

	return pch->total_wake_count;
}

int bcmpcie_set_get_wake(struct dhd_bus *bus, int flag)
{
	dhdpcie_info_t *pch = pci_get_drvdata(bus->dev);
	unsigned long flags;
	int ret;

	DHD_PKT_WAKE_LOCK(&pch->pkt_wake_lock, flags);

	ret = pch->pkt_wake;
	pch->total_wake_count += flag;
	pch->pkt_wake = flag;
	DHD_PKT_WAKE_UNLOCK(&pch->pkt_wake_lock, flags);
	return ret;
}

int bcmpcie_get_wake(struct dhd_bus *bus)
{
	dhdpcie_info_t *pch = pci_get_drvdata(bus->dev);
	unsigned long flags;
	int ret;

	DHD_PKT_WAKE_LOCK(&pch->pkt_wake_lock, flags);
	ret = pch->pkt_wake;
	DHD_PKT_WAKE_UNLOCK(&pch->pkt_wake_lock, flags);
	return ret;
}

int bcmpcie_set_get_wake_pkt_dump(struct dhd_bus *bus, int wake_pkt_dump)
{
	dhdpcie_info_t *pch = pci_get_drvdata(bus->dev);
	unsigned long flags;
	int ret;

	DHD_PKT_WAKE_LOCK(&pch->pkt_wake_lock, flags);
	ret = pch->pkt_wake_dump;
	pch->pkt_wake_dump = wake_pkt_dump;
	DHD_PKT_WAKE_UNLOCK(&pch->pkt_wake_lock, flags);
	return ret;
}
#endif /* DHD_WAKE_STATUS */

static int dhdpcie_resume_dev(struct pci_dev *dev)
{
	int err = 0;
	dhdpcie_info_t *pch = pci_get_drvdata(dev);
#if defined(OEM_ANDROID) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0))
	pci_load_and_free_saved_state(dev, &pch->state);
#endif /* OEM_ANDROID && LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0) */
	DHD_RPM(("%s: Enter\n", __FUNCTION__));

	/* Resture back current bar1 window */
	OSL_PCI_WRITE_CONFIG(pch->bus->osh, PCI_BAR1_WIN, 4, pch->bus->curr_bar1_win);

#ifdef FORCE_TPOWERON
	if (dhdpcie_chip_req_forced_tpoweron(pch->bus)) {
		dhd_bus_set_tpoweron(pch->bus, tpoweron_scale);
	}
#endif /* FORCE_TPOWERON */
	err = pci_enable_device(dev);
	if (err) {
		DHD_CONS_ONLY(("%s:pci_enable_device error %d \n", __FUNCTION__, err));
		goto out;
	}
	pci_set_master(dev);
	err = pci_set_power_state(dev, PCI_D0);
	if (err) {
		DHD_CONS_ONLY(("%s:pci_set_power_state error %d \n", __FUNCTION__, err));
		goto out;
	}

#ifdef OEM_ANDROID
	dev->state_saved = TRUE;
#endif /* OEM_ANDROID */
	pci_restore_state(dev);

	BCM_REFERENCE(pch);
	dhdpcie_suspend_dump_cfgregs(pch->bus, "AFTER_EP_RESUME");

	/*
	 * Re-enable L1ss in Resume path. Implementation defalts to NOP
	 * If need override in the paltform file
	 */
	dhd_plat_l1ss_ctrl(1);

out:
	return err;
}

static int dhdpcie_resume_host_dev(dhd_bus_t *bus)
{
	int bcmerror = 0;

	bcmerror = dhdpcie_start_host_dev(bus);
	if (bcmerror < 0) {
		DHD_ERROR(("%s: PCIe RC resume failed!!! (%d)\n",
			__FUNCTION__, bcmerror));
		bus->is_linkdown = 1;
	}

	return bcmerror;
}

static int dhdpcie_suspend_host_dev(dhd_bus_t *bus)
{
	int bcmerror = 0;
#ifdef CONFIG_ARCH_EXYNOS
	/*
	 * XXX : SWWLAN-82173, SWWLAN-82183 WAR for SS PCIe RC
	 * SS PCIe RC/EP is 1 to 1 mapping using different channel
	 * RC0 - LTE, RC1 - WiFi RC0-1 is working independently
	 */

	if (bus->rc_dev) {
		pci_save_state(bus->rc_dev);
	} else {
		DHD_ERROR(("%s: RC %x:%x handle is NULL\n",
			__FUNCTION__, dhd_plat_get_rc_vendor_id(), dhd_plat_get_rc_device_id()));
	}
#endif /* CONFIG_ARCH_EXYNOS */
	bcmerror = dhdpcie_stop_host_dev(bus);
	return bcmerror;
}

int
dhdpcie_set_master_and_d0_pwrstate(dhd_bus_t *bus)
{
	int err;
	pci_set_master(bus->dev);
	err = pci_set_power_state(bus->dev, PCI_D0);
	if (err) {
		DHD_ERROR(("%s: pci_set_power_state error %d \n", __FUNCTION__, err));
	}
	return err;
}

uint32
dhdpcie_rc_config_read(dhd_bus_t *bus, uint offset)
{
	uint val = -1; /* Initialise to 0xfffffff */
	if (bus->rc_dev) {
		pci_read_config_dword(bus->rc_dev, offset, &val);
		OSL_DELAY(100);
	} else {
		DHD_ERROR(("%s: RC %x:%x handle is NULL\n",
			__FUNCTION__, dhd_plat_get_rc_vendor_id(), dhd_plat_get_rc_device_id()));
	}
	DHD_ERROR(("%s: RC %x:%x offset 0x%x val 0x%x\n",
		__FUNCTION__, dhd_plat_get_rc_vendor_id(), dhd_plat_get_rc_device_id(),
		offset, val));
	return (val);
}

/*
 * Reads/ Writes the value of capability register
 * from the given CAP_ID section of PCI Root Port
 *
 * Arguements
 * @bus current dhd_bus_t pointer
 * @cap Capability or Extended Capability ID to get
 * @offset offset of Register to Read
 * @is_ext TRUE if @cap is given for Extended Capability
 * @is_write is set to TRUE to indicate write
 * @val value to write
 *
 * Return Value
 * Returns 0xffffffff on error
 * on write success returns BCME_OK (0)
 * on Read Success returns the value of register requested
 * Note: caller shoud ensure valid capability ID and Ext. Capability ID.
 */

uint32
dhdpcie_access_cap(struct pci_dev *pdev, int cap, uint offset, bool is_ext, bool is_write,
	uint32 writeval)
{
	int cap_ptr = 0;
	uint32 ret = -1;
	uint32 readval;

	if (!(pdev)) {
		DHD_ERROR(("%s: pdev is NULL\n", __FUNCTION__));
		return ret;
	}

	/* Find Capability offset */
	if (is_ext) {
		/* removing max EXT_CAP_ID check as
		 * linux kernel definition's max value is not upadted yet as per spec
		 */
		cap_ptr = pci_find_ext_capability(pdev, cap);

	} else {
		/* removing max PCI_CAP_ID_MAX check as
		 * pervious kernel versions dont have this definition
		 */
		cap_ptr = pci_find_capability(pdev, cap);
	}

	/* Return if capability with given ID not found */
	if (cap_ptr == 0) {
		DHD_ERROR(("%s: PCI Cap(0x%02x) not supported.\n",
			__FUNCTION__, cap));
		return BCME_ERROR;
	}

	if (is_write) {
		pci_write_config_dword(pdev, (cap_ptr + offset), writeval);
		ret = BCME_OK;

	} else {

		pci_read_config_dword(pdev, (cap_ptr + offset), &readval);
		ret = readval;
	}

	return ret;
}

uint32
dhdpcie_rc_access_cap(dhd_bus_t *bus, int cap, uint offset, bool is_ext, bool is_write,
	uint32 writeval)
{
	if (!(bus->rc_dev)) {
		DHD_ERROR(("%s: RC %x:%x handle is NULL\n",
			__FUNCTION__, dhd_plat_get_rc_vendor_id(), dhd_plat_get_rc_device_id()));
		return BCME_ERROR;
	}

	return dhdpcie_access_cap(bus->rc_dev, cap, offset, is_ext, is_write, writeval);
}

uint32
dhdpcie_ep_access_cap(dhd_bus_t *bus, int cap, uint offset, bool is_ext, bool is_write,
	uint32 writeval)
{
	if (!(bus->dev)) {
		DHD_ERROR(("%s: EP handle is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	return dhdpcie_access_cap(bus->dev, cap, offset, is_ext, is_write, writeval);
}

/* API wrapper to read Root Port link capability
 * Returns 2 = GEN2 1 = GEN1 BCME_ERR on linkcap not found
 */

uint32 dhd_debug_get_rc_linkcap(dhd_bus_t *bus)
{
	uint32 linkcap = -1;
	linkcap = dhdpcie_rc_access_cap(bus, PCIE_CAP_ID_EXP,
			PCIE_CAP_LINKCAP_OFFSET, FALSE, FALSE, 0);
	linkcap &= PCIE_CAP_LINKCAP_LNKSPEED_MASK;
	return linkcap;
}

static void dhdpcie_config_save_restore_coherent(dhd_bus_t *bus, bool state)
{
	if (bus->coreid == ARMCA7_CORE_ID) {
		if (state) {
			/* Sleep */
			bus->coherent_state = dhdpcie_bus_cfg_read_dword(bus,
				PCIE_CFG_SUBSYSTEM_CONTROL, 4) & PCIE_BARCOHERENTACCEN_MASK;
		} else {
			uint32 val = (dhdpcie_bus_cfg_read_dword(bus, PCIE_CFG_SUBSYSTEM_CONTROL,
				4) & ~PCIE_BARCOHERENTACCEN_MASK) | bus->coherent_state;
			dhdpcie_bus_cfg_write_dword(bus, PCIE_CFG_SUBSYSTEM_CONTROL, 4, val);
		}
	}
}

int dhdpcie_pci_suspend_resume(dhd_bus_t *bus, bool state)
{
	int rc;

	struct pci_dev *dev = bus->dev;

	if (state) {
		bus->ptm_ctrl_reg = dhdpcie_bus_cfg_read_dword(bus, PCI_CFG_PTM_CTRL, 4);

		dhdpcie_config_save_restore_coherent(bus, state);

#if !defined(BCMPCIE_OOB_HOST_WAKE) && !defined(PCIE_OOB)
		dhdpcie_pme_active(bus->osh, state);
#endif /* !BCMPCIE_OOB_HOST_WAKE && !PCIE_OOB */
		rc = dhdpcie_suspend_dev(dev);
		if (!rc) {
			dhdpcie_suspend_host_dev(bus);
		}
	} else {
		rc = dhdpcie_resume_host_dev(bus);
		if (!rc) {
			rc = dhdpcie_resume_dev(dev);
			if (PCIECTO_ENAB(bus)) {
				/* reinit CTO configuration
				 * because cfg space got reset at D3 (PERST)
				 */
				dhdpcie_cto_cfg_init(bus, TRUE);
			}

			dhdpcie_bus_cfg_write_dword(bus, PCI_CFG_PTM_CTRL, 4, bus->ptm_ctrl_reg);

			if (PCIE_ENUM_RESET_WAR_ENAB(bus->sih->buscorerev)) {
				dhdpcie_ssreset_dis_enum_rst(bus);
			}
#if !defined(BCMPCIE_OOB_HOST_WAKE) && !defined(PCIE_OOB)
			dhdpcie_pme_active(bus->osh, state);
#endif /* !BCMPCIE_OOB_HOST_WAKE && !PCIE_OOB */
			dhdpcie_config_save_restore_coherent(bus, state);
		}
#if defined(OEM_ANDROID)
#if defined(DHD_HANG_SEND_UP_TEST)
		if (bus->is_linkdown ||
			bus->dhd->req_hang_type == HANG_REASON_PCIE_RC_LINK_UP_FAIL)
#else /* DHD_HANG_SEND_UP_TEST */
		if (bus->is_linkdown)
#endif /* DHD_HANG_SEND_UP_TEST */
		{
			bus->dhd->hang_reason = HANG_REASON_PCIE_RC_LINK_UP_FAIL;
			dhd_os_send_hang_message(bus->dhd);
		}
#endif /* OEM_ANDROID */
	}
	return rc;
}

static int dhdpcie_device_scan(struct device *dev, void *data)
{
	struct pci_dev *pcidev;
	int *cnt = data;

	GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
	pcidev = container_of(dev, struct pci_dev, dev);
	GCC_DIAGNOSTIC_POP();

	if ((pcidev->vendor != VENDOR_BROADCOM) && (pcidev->vendor != VENDOR_SYNAPTICS))
		return 0;

	DHD_INFO(("Found Broadcom or Synaptics PCI device 0x%04x\n", pcidev->device));
	*cnt += 1;
	if (pcidev->driver && strcmp(pcidev->driver->name, dhdpcie_driver.name))
		DHD_ERROR(("Broadcom or Synaptics PCI Device 0x%04x has allocated with driver %s\n",
			pcidev->device, pcidev->driver->name));

	return 0;
}

int
dhdpcie_bus_register(void)
{
	int error = 0;

	if (!(error = pci_register_driver(&dhdpcie_driver))) {
		bus_for_each_dev(dhdpcie_driver.driver.bus, NULL, &error, dhdpcie_device_scan);
		if (!error) {
			DHD_ERROR(("No Broadcom or Synaptics PCI device enumerated!\n"));
#ifdef DHD_PRELOAD
			return 0;
#endif
		} else if (!dhdpcie_init_succeeded) {
			DHD_ERROR(("%s: dhdpcie initialize failed.\n", __FUNCTION__));
		} else {
			return 0;
		}

		pci_unregister_driver(&dhdpcie_driver);
		error = BCME_ERROR;
	}

	return error;
}

void
dhdpcie_bus_unregister(void)
{
	unsigned long flags;
	dhd_pub_t *dhdp = NULL;
	if (!g_dhd_bus) {
		DHD_ERROR(("%s: Bus is NULL\n", __FUNCTION__));
		return;
	}
	dhdp = g_dhd_bus->dhd;
	DHD_GENERAL_LOCK(dhdp, flags);
	/* Check if bus is in suspend due to wifi off with accel boot */
	if (dhdp->up == FALSE && dhdp->busstate == DHD_BUS_SUSPEND) {
		DHD_GENERAL_UNLOCK(dhdp, flags);
		DHD_ERROR(("%s Bus is in suspend state\n", __FUNCTION__));
		dhdpcie_pci_suspend_resume(dhdp->bus, FALSE);
	} else {
		DHD_GENERAL_UNLOCK(dhdp, flags);
	}
	pci_unregister_driver(&dhdpcie_driver);
}

#if defined(CONFIG_ARCH_WAIPIO) || defined(CONFIG_SOC_S5E9925)
bool devid_mismatch = FALSE;

bool dhdpcie_validate_devid(uint16 device)
{
	uint16 config_devid;

#ifdef BCM4389_CHIP_DEF
	config_devid = BCM4389_D11AX_ID;
#elif defined(BCM4375_CHIP)
	config_devid = BCM4375_D11AX_ID;
#else
	DHD_ERROR(("%s: Unknown dev id, BCMXXXX_CHIP is not defined \n",
			__FUNCTION__));
	config_devid = 0;

	return FALSE;
#endif /* BCM4389_CHIP_DEF */

	DHD_ERROR(("%s: config_devid %X device %X\n", __FUNCTION__, config_devid, device));

	if ((config_devid == BCM4389_D11AX_ID) && (device == BCM4389_CHIP_ID)) {
		/* The dev id is 4389 if OTP is not written. */
		return TRUE;
	}

	return config_devid == device;
}
#endif /* CONFIG_ARCH_WAIPIO || CONFIG_SOC_S5E9925 */

int __devinit
dhdpcie_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int err = 0;
	DHD_MUTEX_LOCK();

	if (dhdpcie_chipmatch (pdev->vendor, pdev->device)) {
		DHD_ERROR(("%s: chipmatch failed!!\n", __FUNCTION__));
		err = -ENODEV;
		goto exit;
	}

	DHD_CONS_ONLY(("PCI_PROBE:  bus 0x%X, slot 0x%X,vendor 0x%X, device 0x%X"
		"(good PCI location)\n", pdev->bus->number,
		PCI_SLOT(pdev->devfn), pdev->vendor, pdev->device));

#if defined(CONFIG_ARCH_WAIPIO) || defined(CONFIG_SOC_S5E9925)
	if (dhdpcie_validate_devid(pdev->device) == FALSE) {
		DHD_ERROR(("%s: dev id and BCMXXXX_CHIP definition is not matched\n",
				__FUNCTION__));
		devid_mismatch = TRUE;
		err = -ENODEV;
		goto exit;
	}
#endif /* CONFIG_ARCH_WAIPIO || CONFIG_SOC_S5E9925 */

	if (dhdpcie_init_succeeded == TRUE) {
		DHD_ERROR(("%s(): === Driver Already attached to a BRCM device === \r\n",
			__FUNCTION__));
		err = -ENODEV;
		goto exit;
	}

	if (dhdpcie_init (pdev)) {
		DHD_ERROR(("%s: PCIe Enumeration failed\n", __FUNCTION__));
		err = -ENODEV;
		goto exit;
	}

#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
	/*
	Since MSM PCIe RC dev usage conunt already incremented +2 even
	before dhdpcie_pci_probe() called, then we inevitably to call
	pm_runtime_put_noidle() two times to make the count start with zero.
	*/

	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

#ifdef BCMPCIE_DISABLE_ASYNC_SUSPEND
	/* disable async suspend */
	device_disable_async_suspend(&pdev->dev);
#endif /* BCMPCIE_DISABLE_ASYNC_SUSPEND */

	DHD_TRACE(("%s: PCIe Enumeration done!!\n", __FUNCTION__));
exit:
	DHD_MUTEX_UNLOCK();
	return err;
}

int
dhdpcie_detach(dhdpcie_info_t *pch)
{
	if (pch) {
#if defined(OEM_ANDROID) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0))
		if (!dhd_download_fw_on_driverload) {
			pci_load_and_free_saved_state(pch->dev, &pch->default_state);
		}
#endif /* OEM_ANDROID && LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0) */
		MFREE(pch->osh, pch, sizeof(dhdpcie_info_t));
	}
	return 0;
}

void __devexit
dhdpcie_pci_stop(struct pci_dev *pdev)
{
	osl_t *osh = NULL;
	dhdpcie_info_t *pch = NULL;
	dhd_bus_t *bus = NULL;

	DHD_TRACE(("%s Enter\n", __FUNCTION__));
	pch = pci_get_drvdata(pdev);
	bus = pch->bus;
	osh = pch->osh;

#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
	pm_runtime_get_noresume(&pdev->dev);
	pm_runtime_get_noresume(&pdev->dev);
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

	if (bus) {

		bus->rc_dev = NULL;

		dhdpcie_bus_release(bus);
	}

	/*
	 * For module type driver,
	 * it needs to back up configuration space before rmmod
	 * Since original backed up configuration space won't be restored if state_saved = false
	 * This back up the configuration space again & state_saved = true
	 */
	pci_save_state(pdev);

	if (pci_is_enabled(pdev))
		pci_disable_device(pdev);
#ifdef BCMPCIE_OOB_HOST_WAKE
	/* pcie os info detach */
	MFREE(osh, pch->os_cxt, sizeof(dhdpcie_os_info_t));
#endif /* BCMPCIE_OOB_HOST_WAKE */
#ifdef USE_SMMU_ARCH_MSM
	/* smmu info detach */
	dhdpcie_smmu_remove(pdev, pch->smmu_cxt);
	MFREE(osh, pch->smmu_cxt, sizeof(dhdpcie_smmu_info_t));
#endif /* USE_SMMU_ARCH_MSM */
	/* pcie info detach */
	dhdpcie_detach(pch);
	/* osl detach */
	osl_detach(osh);

	dhdpcie_init_succeeded = FALSE;

	DHD_TRACE(("%s Exit\n", __FUNCTION__));

	return;
}

void __devexit
dhdpcie_pci_remove(struct pci_dev *pdev)
{
	DHD_MUTEX_LOCK();
	DHD_ERROR(("%s Enter\n", __FUNCTION__));
	dhdpcie_pci_stop(pdev);
	DHD_MUTEX_UNLOCK();
	return;
}

void __devexit
dhdpcie_pci_shutdown(struct pci_dev *pdev)
{
	dhdpcie_info_t *pch = NULL;
	dhd_bus_t *bus = NULL;

	pch = pci_get_drvdata(pdev);
	bus = pch->bus;
	DHD_ERROR(("%s Enter\n", __FUNCTION__));

	/* Stop all interface network queue */
	dhd_bus_stop_queue(bus);
	/* Disable IRQ */
	dhdpcie_disable_irq(bus);
	/* Kill dpc */
	dhd_dpc_kill(bus->dhd);
	/* Stop RPM timer */
	DHD_STOP_RPM_TIMER(bus->dhd);
	dhdpcie_busbusy_wait(bus->dhd);
	return;
}

/* Enable Linux Msi */
int
dhdpcie_enable_msi(struct pci_dev *pdev, unsigned int min_vecs, unsigned int max_vecs)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0))
	return pci_alloc_irq_vectors(pdev, min_vecs, max_vecs, PCI_IRQ_MSI);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
	return pci_enable_msi_range(pdev, min_vecs, max_vecs);
#else
	return pci_enable_msi_block(pdev, max_vecs);
#endif
}

/* Disable Linux Msi */
void
dhdpcie_disable_msi(struct pci_dev *pdev)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0))
	pci_free_irq_vectors(pdev);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0))
	pci_disable_msi(pdev);
#else
	pci_disable_msi(pdev);
#endif
	return;
}

/* Request Linux irq */
int
dhdpcie_request_irq(dhdpcie_info_t *dhdpcie_info)
{
	dhd_bus_t *bus = dhdpcie_info->bus;
	struct pci_dev *pdev = dhdpcie_info->bus->dev;
	int err = 0;

	if (!bus->irq_registered) {
		snprintf(dhdpcie_info->pciname, sizeof(dhdpcie_info->pciname),
			"dhdpcie:%s", pci_name(pdev));

		if (bus->d2h_intr_method == PCIE_MSI) {
			if (dhdpcie_enable_msi(pdev, 1, 1) < 0) {
				DHD_ERROR(("%s: dhdpcie_enable_msi() failed\n", __FUNCTION__));
				dhdpcie_disable_msi(pdev);
				bus->d2h_intr_method = PCIE_INTX;
			}
		}

		if (bus->d2h_intr_method == PCIE_MSI)
			printf("%s: MSI enabled, irq=%d\n", __FUNCTION__, pdev->irq);
		else
			printf("%s: INTx enabled, irq=%d\n", __FUNCTION__, pdev->irq);

		err = request_irq(pdev->irq, dhdpcie_isr, IRQF_SHARED,
			dhdpcie_info->pciname, bus);
		if (err < 0) {
			DHD_ERROR(("%s: request_irq() failed with %d\n", __FUNCTION__, err));
			if (bus->d2h_intr_method == PCIE_MSI) {
				dhdpcie_disable_msi(pdev);
			}
			return -1;
		}
		else {
			bus->irq_registered = TRUE;
		}
	} else {
		DHD_ERROR(("%s: PCI IRQ is already registered\n", __FUNCTION__));
	}

	dhdpcie_enable_irq_loop(bus);

	DHD_TRACE(("%s %s\n", __FUNCTION__, dhdpcie_info->pciname));

	return 0; /* SUCCESS */
}

/**
 *	dhdpcie_get_pcieirq - return pcie irq number to linux-dhd
 */
int
dhdpcie_get_pcieirq(struct dhd_bus *bus, unsigned int *irq)
{
	struct pci_dev *pdev = bus->dev;

	if (!pdev) {
		DHD_ERROR(("%s : bus->dev is NULL\n", __FUNCTION__));
		return -ENODEV;
	}

	*irq  = pdev->irq;

	return 0; /* SUCCESS */
}

#ifdef CONFIG_PHYS_ADDR_T_64BIT
#define PRINTF_RESOURCE	"0x%016llx"
#else
#define PRINTF_RESOURCE	"0x%08x"
#endif

#ifdef EXYNOS_PCIE_MODULE_PATCH
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0))
extern struct pci_saved_state *bcm_pcie_default_state;
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0) */
#endif /* EXYNOS_MODULE_PATCH */

/*

Name:  osl_pci_get_resource

Parametrs:

1: struct pci_dev *pdev   -- pci device structure
2: pci_res                       -- structure containing pci configuration space values

Return value:

int   - Status (TRUE or FALSE)

Description:
Access PCI configuration space, retrieve  PCI allocated resources , updates in resource structure.

 */
#ifdef DHD_DEBUG_REG_DUMP
uint64 regs_addr;
#endif /* DHD_DEBUG_REG_DUMP */
int dhdpcie_get_resource(dhdpcie_info_t *dhdpcie_info)
{
	phys_addr_t  bar0_addr, bar1_addr;
	ulong bar1_size;
	struct pci_dev *pdev = NULL;
	pdev = dhdpcie_info->dev;
#ifdef EXYNOS_PCIE_MODULE_PATCH
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0))
	if (bcm_pcie_default_state) {
		pci_load_saved_state(pdev, bcm_pcie_default_state);
		pci_restore_state(pdev);
	}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0) */
#endif /* EXYNOS_MODULE_PATCH */

	/*
	 * For built-in type driver,
	 * it can't restore configuration backup because of state_saved = false at first load time
	 * For module type driver,
	 * it couldn't remap the BAR0/BAR1 address
	 * without restoring configuration backup at second load,
	 * and remains configuration backup in pci_dev, DHD didn't remove it from the bus
	 * pci_restore_state() restores proper BAR0/BAR1 address
	 */
	pci_restore_state(pdev);

	do {
		if (pci_enable_device(pdev)) {
			DHD_CONS_ONLY(("%s: Cannot enable PCI device\n", __FUNCTION__));
			break;
		}
		pci_set_master(pdev);
		bar0_addr = pci_resource_start(pdev, 0);	/* Bar-0 mapped address */
		bar1_addr = pci_resource_start(pdev, 2);	/* Bar-1 mapped address */

		/* read Bar-1 mapped memory range */
		bar1_size = pci_resource_len(pdev, 2);

		if ((bar1_size == 0) || (bar1_addr == 0)) {
			DHD_CONS_ONLY(("%s: BAR1 Not enabled for this device  size(%ld),"
				" addr(0x"PRINTF_RESOURCE")\n",
				__FUNCTION__, bar1_size, bar1_addr));
			goto err;
		}

		dhdpcie_info->regs = (volatile char *) REG_MAP(bar0_addr, DONGLE_REG_MAP_SIZE);
		dhdpcie_info->bar1_size =
			(bar1_size > DONGLE_TCM_MAP_SIZE) ? bar1_size : DONGLE_TCM_MAP_SIZE;
		dhdpcie_info->tcm = (volatile char *) REG_MAP(bar1_addr, dhdpcie_info->bar1_size);

		if (!dhdpcie_info->regs || !dhdpcie_info->tcm) {
			DHD_ERROR(("%s:ioremap() failed\n", __FUNCTION__));
			break;
		}
#ifdef EXYNOS_PCIE_MODULE_PATCH
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0))
		if (bcm_pcie_default_state == NULL) {
			pci_save_state(pdev);
			bcm_pcie_default_state = pci_store_saved_state(pdev);
		}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0) */
#endif /* EXYNOS_MODULE_PATCH */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0))
	/* Backup PCIe configuration so as to use Wi-Fi on/off process
	 * in case of built in driver
	 */
	pci_save_state(pdev);
	dhdpcie_info->default_state = pci_store_saved_state(pdev);

	if (dhdpcie_info->default_state == NULL) {
		DHD_ERROR(("%s pci_store_saved_state returns NULL\n",
			__FUNCTION__));
		REG_UNMAP(dhdpcie_info->regs);
		REG_UNMAP(dhdpcie_info->tcm);
		pci_disable_device(pdev);
		break;
	}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0) */

		DHD_TRACE(("%s:Phys addr : reg space = %p base addr 0x"PRINTF_RESOURCE" \n",
			__FUNCTION__, dhdpcie_info->regs, bar0_addr));
		DHD_TRACE(("%s:Phys addr : tcm_space = %p base addr 0x"PRINTF_RESOURCE" \n",
			__FUNCTION__, dhdpcie_info->tcm, bar1_addr));
#ifdef DHD_DEBUG_REG_DUMP
		regs_addr = (uint64)dhdpcie_info->regs;
		DHD_ERROR(("%s: saved regs_addr = 0x%llx\n", __FUNCTION__, regs_addr));
#endif /* DHD_DEBUG_REG_DUMP */
		return 0; /* SUCCESS  */
	} while (0);
err:
	return -1;  /* FAILURE */
}

int dhdpcie_scan_resource(dhdpcie_info_t *dhdpcie_info)
{

	DHD_TRACE(("%s: ENTER\n", __FUNCTION__));

	do {
		/* define it here only!! */
		if (dhdpcie_get_resource (dhdpcie_info)) {
			DHD_ERROR(("%s: Failed to get PCI resources\n", __FUNCTION__));
			break;
		}
		DHD_TRACE(("%s:Exit - SUCCESS \n",
			__FUNCTION__));

		return 0; /* SUCCESS */

	} while (0);

	DHD_TRACE(("%s:Exit - FAILURE \n", __FUNCTION__));

	return -1; /* FAILURE */

}

void dhdpcie_dump_resource(dhd_bus_t *bus)
{
	dhdpcie_info_t *pch;

	if (bus == NULL) {
		DHD_ERROR(("%s: bus is NULL\n", __FUNCTION__));
		return;
	}

	if (bus->dev == NULL) {
		DHD_ERROR(("%s: bus->dev is NULL\n", __FUNCTION__));
		return;
	}

	pch = pci_get_drvdata(bus->dev);
	if (pch == NULL) {
		DHD_ERROR(("%s: pch is NULL\n", __FUNCTION__));
		return;
	}

	/* BAR0 */
	DHD_RPM(("%s: BAR0(VA): 0x%pK, BAR0(PA): "PRINTF_RESOURCE", SIZE: %d\n",
		__FUNCTION__, pch->regs, pci_resource_start(bus->dev, 0),
		DONGLE_REG_MAP_SIZE));

	/* BAR1 */
	DHD_RPM(("%s: BAR1(VA): 0x%pK, BAR1(PA): "PRINTF_RESOURCE", SIZE: %d\n",
		__FUNCTION__, pch->tcm, pci_resource_start(bus->dev, 2),
		pch->bar1_size));
}

int dhdpcie_init(struct pci_dev *pdev)
{

	osl_t 				*osh = NULL;
	dhd_bus_t 			*bus = NULL;
	dhdpcie_info_t		*dhdpcie_info =  NULL;
	wifi_adapter_info_t	*adapter = NULL;
#ifdef BCMPCIE_OOB_HOST_WAKE
	dhdpcie_os_info_t	*dhdpcie_osinfo = NULL;
#endif /* BCMPCIE_OOB_HOST_WAKE */
#ifdef USE_SMMU_ARCH_MSM
	dhdpcie_smmu_info_t	*dhdpcie_smmu_info = NULL;
#endif /* USE_SMMU_ARCH_MSM */
	int ret = 0;

	do {
		/* osl attach */
		if (!(osh = osl_attach(pdev, PCI_BUS, FALSE))) {
			DHD_ERROR(("%s: osl_attach failed\n", __FUNCTION__));
			break;
		}

		/* initialize static buffer */
		adapter = dhd_wifi_platform_get_adapter(PCI_BUS, pdev->bus->number,
			PCI_SLOT(pdev->devfn));
		if (adapter != NULL) {
			DHD_ERROR(("%s: found adapter info '%s'\n", __FUNCTION__, adapter->name));
			adapter->bus_type = PCI_BUS;
			adapter->bus_num = pdev->bus->number;
			adapter->slot_num = PCI_SLOT(pdev->devfn);
			adapter->pci_dev = pdev;
		} else {
			DHD_ERROR(("%s: can't find adapter info for this chip\n", __FUNCTION__));
#ifdef ADAPTER_IDX
			break;
#endif
		}
		osl_static_mem_init(osh, adapter);

		/*  allocate linux spcific pcie structure here */
		if (!(dhdpcie_info = MALLOC(osh, sizeof(dhdpcie_info_t)))) {
			DHD_ERROR(("%s: MALLOC of dhd_bus_t failed\n", __FUNCTION__));
			break;
		}
		bzero(dhdpcie_info, sizeof(dhdpcie_info_t));
		dhdpcie_info->osh = osh;
		dhdpcie_info->dev = pdev;

#ifdef BCMPCIE_OOB_HOST_WAKE
		/* allocate OS speicific structure */
		dhdpcie_osinfo = MALLOC(osh, sizeof(dhdpcie_os_info_t));
		if (dhdpcie_osinfo == NULL) {
			DHD_ERROR(("%s: MALLOC of dhdpcie_os_info_t failed\n",
				__FUNCTION__));
			break;
		}
		bzero(dhdpcie_osinfo, sizeof(dhdpcie_os_info_t));
		dhdpcie_info->os_cxt = (void *)dhdpcie_osinfo;

		/* Initialize host wake IRQ */
		spin_lock_init(&dhdpcie_osinfo->oob_irq_spinlock);
		/* Get customer specific host wake IRQ parametres: IRQ number as IRQ type */
		dhdpcie_osinfo->oob_irq_num = wifi_platform_get_irq_number(adapter,
			&dhdpcie_osinfo->oob_irq_flags);
		if (dhdpcie_osinfo->oob_irq_num < 0) {
			DHD_ERROR(("%s: Host OOB irq is not defined\n", __FUNCTION__));
		}
#endif /* BCMPCIE_OOB_HOST_WAKE */

#ifdef USE_SMMU_ARCH_MSM
		/* allocate private structure for using SMMU */
		dhdpcie_smmu_info = MALLOC(osh, sizeof(dhdpcie_smmu_info_t));
		if (dhdpcie_smmu_info == NULL) {
			DHD_ERROR(("%s: MALLOC of dhdpcie_smmu_info_t failed\n",
				__FUNCTION__));
			break;
		}
		bzero(dhdpcie_smmu_info, sizeof(dhdpcie_smmu_info_t));
		dhdpcie_info->smmu_cxt = (void *)dhdpcie_smmu_info;

		/* Initialize smmu structure */
		if (dhdpcie_smmu_init(pdev, dhdpcie_info->smmu_cxt) < 0) {
			DHD_ERROR(("%s: Failed to initialize SMMU\n",
				__FUNCTION__));
			break;
		}
#endif /* USE_SMMU_ARCH_MSM */

#ifdef DHD_SET_PCIE_DMA_MASK_FOR_GS101
		/* S.SLSI PCIe DMA engine cannot support 64 bit bus address. Hence, set 36 bit */
		if (pci_set_dma_mask(pdev, DMA_BIT_MASK(DHD_PCIE_DMA_MASK_FOR_GS101)) ||
			pci_set_consistent_dma_mask(pdev,
				DMA_BIT_MASK(DHD_PCIE_DMA_MASK_FOR_GS101))) {
			DHD_ERROR(("%s: DMA set %d bit mask failed.\n",
				__FUNCTION__, DHD_PCIE_DMA_MASK_FOR_GS101));
			return -EINVAL;
		}
#endif /* DHD_SET_PCIE_DMA_MASK_FOR_GS101 */

#ifdef DHD_WAKE_STATUS
		/* Initialize pkt_wake_lock */
		spin_lock_init(&dhdpcie_info->pkt_wake_lock);
#endif /* DHD_WAKE_STATUS */

		/* Find the PCI resources, verify the  */
		/* vendor and device ID, map BAR regions and irq,  update in structures */
		if (dhdpcie_scan_resource(dhdpcie_info)) {
			DHD_ERROR(("%s: dhd_Scan_PCI_Res failed\n", __FUNCTION__));

			break;
		}

		/* Bus initialization */
		ret = dhdpcie_bus_attach(osh, &bus, dhdpcie_info->regs, dhdpcie_info->tcm, pdev, adapter);
		if (ret != BCME_OK) {
			DHD_ERROR(("%s:dhdpcie_bus_attach() failed\n", __FUNCTION__));
			break;
		}

		dhdpcie_info->bus = bus;
		bus->bar1_size = dhdpcie_info->bar1_size;
		bus->is_linkdown = 0;
		bus->no_bus_init = FALSE;
		bus->cto_triggered = 0;

		bus->rc_dev = NULL;

		/* Get RC Device Handle */
		if (bus->dev->bus) {
			/* self member of structure pci_bus is bridge device as seen by parent */
			bus->rc_dev = bus->dev->bus->self;
			if (bus->rc_dev)
				DHD_ERROR(("%s: rc_dev from dev->bus->self (%x:%x) is %pK\n", __FUNCTION__,
					bus->rc_dev->vendor, bus->rc_dev->device, bus->rc_dev));
			else
				DHD_ERROR(("%s: bus->dev->bus->self is NULL\n", __FUNCTION__));
		} else {
			DHD_ERROR(("%s: unable to get rc_dev as dev->bus is NULL\n", __FUNCTION__));
		}

		/* if rc_dev is still NULL, try to get from vendor/device IDs */
		if (bus->rc_dev == NULL) {
			bus->rc_dev = pci_get_device(dhd_plat_get_rc_vendor_id(),
					dhd_plat_get_rc_device_id(), NULL);
			DHD_ERROR(("%s: rc_dev from pci_get_device (%x:%x) is %p\n", __FUNCTION__,
				dhd_plat_get_rc_vendor_id(), dhd_plat_get_rc_device_id(),
				bus->rc_dev));
		}

		bus->rc_ep_aspm_cap = dhd_bus_is_rc_ep_aspm_capable(bus);
		bus->rc_ep_l1ss_cap = dhd_bus_is_rc_ep_l1ss_capable(bus);
		DHD_ERROR(("%s: rc_ep_aspm_cap: %d rc_ep_l1ss_cap: %d\n",
			__FUNCTION__, bus->rc_ep_aspm_cap, bus->rc_ep_l1ss_cap));

#ifdef FORCE_TPOWERON
		if (dhdpcie_chip_req_forced_tpoweron(bus)) {
			dhd_bus_set_tpoweron(bus, tpoweron_scale);
		}
#endif /* FORCE_TPOWERON */

#ifdef DONGLE_ENABLE_ISOLATION
		bus->dhd->dongle_isolation = TRUE;
#endif /* DONGLE_ENABLE_ISOLATION */

		if (bus->intr) {
			/* Register interrupt callback, but mask it (not operational yet). */
			DHD_INTR(("%s: Registering and masking interrupts\n", __FUNCTION__));
			bus->intr_enabled = FALSE;
			dhdpcie_bus_intr_disable(bus);

			if (dhdpcie_request_irq(dhdpcie_info)) {
				DHD_ERROR(("%s: request_irq() failed\n", __FUNCTION__));
				break;
			}
		} else {
			bus->pollrate = 1;
			DHD_INFO(("%s: PCIe interrupt function is NOT registered "
				"due to polling mode\n", __FUNCTION__));
		}

#if defined(BCM_REQUEST_FW)
		if (dhd_bus_download_firmware(bus, osh, NULL, NULL) < 0) {
		DHD_ERROR(("%s: failed to download firmware\n", __FUNCTION__));
		}
		bus->nv_path = NULL;
		bus->fw_path = NULL;
#endif /* BCM_REQUEST_FW */

		/* set private data for pci_dev */
		pci_set_drvdata(pdev, dhdpcie_info);

		/* Ensure BAR1 switch feature enable if needed before FW download */
		dhdpcie_bar1_window_switch_enab(bus);
		dhd_init_bar1_switch_lock(bus);

#if defined(BCMDHD_MODULAR) && defined(INSMOD_FW_LOAD)
		if (1)
#else
		if (dhd_download_fw_on_driverload)
#endif
		{
			if (dhd_bus_start(bus->dhd)) {
				DHD_ERROR(("%s: dhd_bus_start() failed\n", __FUNCTION__));
				if (!allow_delay_fwdl)
					break;
			}
		} else {
			/* Set ramdom MAC address during boot time */
			get_random_bytes(&bus->dhd->mac.octet[3], 3);
			/* Adding BRCM OUI */
			bus->dhd->mac.octet[0] = 0;
			bus->dhd->mac.octet[1] = 0x90;
			bus->dhd->mac.octet[2] = 0x4C;
		}

		/* Attach to the OS network interface */
		DHD_TRACE(("%s(): Calling dhd_attach_net() \n", __FUNCTION__));
		if (dhd_attach_net(bus->dhd, TRUE)) {
			DHD_ERROR(("%s(): ERROR.. dhd_attach_net() failed\n", __FUNCTION__));
			break;
		}

		dhdpcie_init_succeeded = TRUE;
#if defined(CONFIG_ARCH_MSM) && defined(CONFIG_SEC_PCIE_L1SS)
		sec_pcie_set_use_ep_loaded(bus->rc_dev);
#endif /* CONFIG_ARCH_MSM && CONFIG_SEC_PCIE_L1SS */
#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
		pm_runtime_set_autosuspend_delay(&pdev->dev, AUTO_SUSPEND_TIMEOUT);
		pm_runtime_use_autosuspend(&pdev->dev);
		atomic_set(&bus->dhd->block_bus, FALSE);
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

#if defined(MULTIPLE_SUPPLICANT)
		wl_android_post_init(); // terence 20120530: fix critical section in dhd_open and dhdsdio_probe
#endif /* MULTIPLE_SUPPLICANT */

		DHD_TRACE(("%s:Exit - SUCCESS \n", __FUNCTION__));
		return 0;  /* return  SUCCESS  */

	} while (0);
	/* reverse the initialization in order in case of error */

	if (bus)
		dhdpcie_bus_release(bus);

#ifdef BCMPCIE_OOB_HOST_WAKE
	if (dhdpcie_osinfo) {
		MFREE(osh, dhdpcie_osinfo, sizeof(dhdpcie_os_info_t));
	}
#endif /* BCMPCIE_OOB_HOST_WAKE */

#ifdef USE_SMMU_ARCH_MSM
	if (dhdpcie_smmu_info) {
		MFREE(osh, dhdpcie_smmu_info, sizeof(dhdpcie_smmu_info_t));
		dhdpcie_info->smmu_cxt = NULL;
	}
#endif /* USE_SMMU_ARCH_MSM */

	if (dhdpcie_info)
		dhdpcie_detach(dhdpcie_info);
	if (adapter)
		pci_disable_device(pdev);
	if (osh)
		osl_detach(osh);
	if (adapter != NULL) {
		adapter->bus_type = -1;
		adapter->bus_num = -1;
		adapter->slot_num = -1;
	}

	dhdpcie_init_succeeded = FALSE;

	DHD_TRACE(("%s:Exit - FAILURE \n", __FUNCTION__));

	return -1; /* return FAILURE  */
}

/* Free Linux irq */
void
dhdpcie_free_irq(dhd_bus_t *bus)
{
	struct pci_dev *pdev = NULL;

	DHD_TRACE(("%s: freeing up the IRQ\n", __FUNCTION__));
	if (bus) {
		pdev = bus->dev;
		if (bus->irq_registered) {
#if defined(SET_PCIE_IRQ_CPU_CORE) || defined(DHD_CONTROL_PCIE_CPUCORE_WIFI_TURNON) || \
	defined(CLEAN_IRQ_AFFINITY_HINT)
			/* clean up the affinity_hint before
			 * the unregistration of PCIe irq
			 */
			(void)irq_set_affinity_hint(pdev->irq, NULL);
#endif /* SET_PCIE_IRQ_CPU_CORE || DHD_CONTROL_PCIE_CPUCORE_WIFI_TURNON ||
		* CLEAN_IRQ_AFFINITY_HINT
		*/
			free_irq(pdev->irq, bus);
			bus->irq_registered = FALSE;
			if (bus->d2h_intr_method == PCIE_MSI) {
				dhdpcie_disable_msi(pdev);
			}
		} else {
			DHD_ERROR(("%s: PCIe IRQ is not registered\n", __FUNCTION__));
		}
	}
	DHD_TRACE(("%s: Exit\n", __FUNCTION__));
	return;
}

/*

Name:  dhdpcie_isr

Parametrs:

1: IN int irq   -- interrupt vector
2: IN void *arg      -- handle to private data structure

Return value:

Status (TRUE or FALSE)

Description:
Interrupt Service routine checks for the status register,
disable interrupt and queue DPC if mail box interrupts are raised.
*/

irqreturn_t
dhdpcie_isr(int irq, void *arg)
{
	dhd_bus_t *bus = (dhd_bus_t*)arg;
	bus->prev_isr_entry_time = bus->isr_entry_time;
	bus->isr_entry_time = OSL_LOCALTIME_NS();
	if (!dhdpcie_bus_isr(bus)) {
		DHD_LOG_MEM(("%s: dhdpcie_bus_isr returns with FALSE\n", __FUNCTION__));
	}
	bus->isr_exit_time = OSL_LOCALTIME_NS();
	return IRQ_HANDLED;
}

int
dhdpcie_disable_irq_nosync(dhd_bus_t *bus)
{
	struct pci_dev *dev;
	if ((bus == NULL) || (bus->dev == NULL)) {
		DHD_ERROR(("%s: bus or bus->dev is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	dev = bus->dev;
	disable_irq_nosync(dev->irq);
	return BCME_OK;
}

int
dhdpcie_disable_irq(dhd_bus_t *bus)
{
	struct pci_dev *dev;
	if ((bus == NULL) || (bus->dev == NULL)) {
		DHD_ERROR(("%s: bus or bus->dev is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	dev = bus->dev;
	disable_irq(dev->irq);
	return BCME_OK;
}

int
dhdpcie_enable_irq(dhd_bus_t *bus)
{
	struct pci_dev *dev;
	if ((bus == NULL) || (bus->dev == NULL)) {
		DHD_ERROR(("%s: bus or bus->dev is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	dev = bus->dev;
	enable_irq(dev->irq);
	return BCME_OK;
}

void
dhdpcie_enable_irq_loop(dhd_bus_t *bus)
{
	/* Enable IRQ in a loop till host_irq_disable_count becomes 0 */
	uint host_irq_disable_count = dhdpcie_irq_disabled(bus);
	while (host_irq_disable_count--) {
		dhdpcie_enable_irq(bus); /* Enable back interrupt!! */
	}
}

int
dhdpcie_irq_disabled(dhd_bus_t *bus)
{
	struct irq_desc *desc = NULL;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0))
	desc = irq_to_desc(bus->dev->irq);
#else // (LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0))
	desc = irq_data_to_desc(irq_get_irq_data(bus->dev->irq));
#endif // (LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0))

	/* depth will be zero, if enabled */
	return desc->depth;
}

int
dhdpcie_start_host_dev(dhd_bus_t *bus)
{
	int ret = 0;
#ifdef CONFIG_ARCH_MSM
#endif /* CONFIG_ARCH_MSM */
	DHD_TRACE(("%s Enter:\n", __FUNCTION__));

	if (bus == NULL) {
		return BCME_ERROR;
	}

	if (bus->dev == NULL) {
		return BCME_ERROR;
	}

	ret = dhd_plat_pcie_resume(bus->dhd->plat_info);

#ifdef CONFIG_ARCH_MSM
	ret = msm_pcie_pm_control(MSM_PCIE_RESUME, bus->dev->bus->number,
		bus->dev, NULL, 0);
#endif /* CONFIG_ARCH_MSM */
	if (ret) {
		DHD_ERROR(("%s Failed to bring up PCIe link\n", __FUNCTION__));
		goto done;
	}

done:
	DHD_TRACE(("%s Exit:\n", __FUNCTION__));
	return ret;
}

int
dhdpcie_stop_host_dev(dhd_bus_t *bus)
{
	int ret = 0;
#ifdef CONFIG_ARCH_MSM
#endif /* CONFIG_ARCH_MSM */

	DHD_TRACE(("%s Enter:\n", __FUNCTION__));

	if (bus == NULL) {
		return BCME_ERROR;
	}

	if (bus->dev == NULL) {
		return BCME_ERROR;
	}

	dhd_plat_pcie_suspend(bus->dhd->plat_info);

#ifdef CONFIG_ARCH_MSM
	ret = msm_pcie_pm_control(MSM_PCIE_SUSPEND, bus->dev->bus->number,
		bus->dev, NULL, 0);
#endif /* CONFIG_ARCH_MSM */
	if (ret) {
		DHD_ERROR(("Failed to stop PCIe link\n"));
		goto done;
	}
done:
	DHD_TRACE(("%s Exit:\n", __FUNCTION__));
	return ret;
}

int
dhdpcie_disable_device(dhd_bus_t *bus)
{
	DHD_TRACE(("%s Enter:\n", __FUNCTION__));

	if (bus == NULL) {
		return BCME_ERROR;
	}

	if (bus->dev == NULL) {
		return BCME_ERROR;
	}

	if (pci_is_enabled(bus->dev))
		pci_disable_device(bus->dev);

	return 0;
}

int
dhdpcie_enable_device(dhd_bus_t *bus)
{
	int ret = BCME_ERROR;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0))
	dhdpcie_info_t *pch;
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0) */

	DHD_TRACE(("%s Enter:\n", __FUNCTION__));

	if (bus == NULL) {
		return BCME_ERROR;
	}

	if (bus->dev == NULL) {
		return BCME_ERROR;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0))
	pch = pci_get_drvdata(bus->dev);
	if (pch == NULL) {
		return BCME_ERROR;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) && (LINUX_VERSION_CODE < \
	KERNEL_VERSION(3, 19, 0)) && !defined(CONFIG_SOC_EXYNOS8890)
	/* Updated with pci_load_and_free_saved_state to compatible
	 * with Kernel version 3.14.0 to 3.18.41.
	 */
	pci_load_and_free_saved_state(bus->dev, &pch->default_state);
	pch->default_state = pci_store_saved_state(bus->dev);
#else
	pci_load_saved_state(bus->dev, pch->default_state);
#endif /* LINUX_VERSION >= 3.14.0 && LINUX_VERSION < 3.19.0 && !CONFIG_SOC_EXYNOS8890 */

	/* Check if Device ID is valid */
	if (bus->dev->state_saved) {
		uint32 vid, saved_vid;
		pci_read_config_dword(bus->dev, PCI_CFG_VID, &vid);
		saved_vid = bus->dev->saved_config_space[PCI_CFG_VID];
		if (vid != saved_vid) {
			DHD_ERROR(("%s: VID(0x%x) is different from saved VID(0x%x) "
				"Skip the bus init\n", __FUNCTION__, vid, saved_vid));
			bus->no_bus_init = TRUE;
			/* Check if the PCIe link is down */
			if (vid == (uint32)-1) {
				bus->is_linkdown = 1;
			}
			return BCME_ERROR;
		}
	}

	pci_restore_state(bus->dev);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)) */

	ret = pci_enable_device(bus->dev);
	if (ret) {
		pci_disable_device(bus->dev);
	} else {
		pci_set_master(bus->dev);
	}

	return ret;
}

int
dhdpcie_alloc_resource(dhd_bus_t *bus)
{
	dhdpcie_info_t *dhdpcie_info;
	phys_addr_t bar0_addr, bar1_addr;
	ulong bar1_size;

	do {
		if (bus == NULL) {
			DHD_ERROR(("%s: bus is NULL\n", __FUNCTION__));
			break;
		}

		if (bus->dev == NULL) {
			DHD_ERROR(("%s: bus->dev is NULL\n", __FUNCTION__));
			break;
		}

		dhdpcie_info = pci_get_drvdata(bus->dev);
		if (dhdpcie_info == NULL) {
			DHD_ERROR(("%s: dhdpcie_info is NULL\n", __FUNCTION__));
			break;
		}

		bar0_addr = pci_resource_start(bus->dev, 0);	/* Bar-0 mapped address */
		bar1_addr = pci_resource_start(bus->dev, 2);	/* Bar-1 mapped address */

		/* read Bar-1 mapped memory range */
		bar1_size = pci_resource_len(bus->dev, 2);

		if ((bar1_size == 0) || (bar1_addr == 0)) {
			DHD_CONS_ONLY(("%s: BAR1 Not enabled for this device size(%ld),"
				" addr(0x"PRINTF_RESOURCE")\n",
				__FUNCTION__, bar1_size, bar1_addr));
			break;
		}

		dhdpcie_info->regs = (volatile char *) REG_MAP(bar0_addr, DONGLE_REG_MAP_SIZE);
		if (!dhdpcie_info->regs) {
			DHD_ERROR(("%s: ioremap() for regs is failed\n", __FUNCTION__));
			break;
		}

		bus->regs = dhdpcie_info->regs;
		dhdpcie_info->bar1_size =
			(bar1_size > DONGLE_TCM_MAP_SIZE) ? bar1_size : DONGLE_TCM_MAP_SIZE;
		dhdpcie_info->tcm = (volatile char *) REG_MAP(bar1_addr, dhdpcie_info->bar1_size);
		if (!dhdpcie_info->tcm) {
			DHD_ERROR(("%s: ioremap() for regs is failed\n", __FUNCTION__));
			REG_UNMAP(dhdpcie_info->regs);
			bus->regs = NULL;
			break;
		}

		bus->tcm = dhdpcie_info->tcm;
		bus->bar1_size = dhdpcie_info->bar1_size;

		DHD_TRACE(("%s:Phys addr : reg space = %p base addr 0x"PRINTF_RESOURCE" \n",
			__FUNCTION__, dhdpcie_info->regs, bar0_addr));
		DHD_TRACE(("%s:Phys addr : tcm_space = %p base addr 0x"PRINTF_RESOURCE" \n",
			__FUNCTION__, dhdpcie_info->tcm, bar1_addr));
#ifdef DHD_DEBUG_REG_DUMP
		regs_addr = (uint64)dhdpcie_info->regs;
		DHD_ERROR(("%s: saved regs_addr = 0x%llx\n", __FUNCTION__, regs_addr));
#endif /* DHD_DEBUG_REG_DUMP */
		return 0;
	} while (0);

	return BCME_ERROR;
}

void
dhdpcie_free_resource(dhd_bus_t *bus)
{
	dhdpcie_info_t *dhdpcie_info;

	if (bus == NULL) {
		DHD_ERROR(("%s: bus is NULL\n", __FUNCTION__));
		return;
	}

	if (bus->dev == NULL) {
		DHD_ERROR(("%s: bus->dev is NULL\n", __FUNCTION__));
		return;
	}

	dhdpcie_info = pci_get_drvdata(bus->dev);
	if (dhdpcie_info == NULL) {
		DHD_ERROR(("%s: dhdpcie_info is NULL\n", __FUNCTION__));
		return;
	}

	if (bus->regs) {
		REG_UNMAP(dhdpcie_info->regs);
		bus->regs = NULL;
	}

	if (bus->tcm) {
		REG_UNMAP(dhdpcie_info->tcm);
		bus->tcm = NULL;
	}
}

int
dhdpcie_bus_request_irq(struct dhd_bus *bus)
{
	dhdpcie_info_t *dhdpcie_info;
	int ret = 0;

	if (bus == NULL) {
		DHD_ERROR(("%s: bus is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (bus->dev == NULL) {
		DHD_ERROR(("%s: bus->dev is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	dhdpcie_info = pci_get_drvdata(bus->dev);
	if (dhdpcie_info == NULL) {
		DHD_ERROR(("%s: dhdpcie_info is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (bus->intr) {
		/* Register interrupt callback, but mask it (not operational yet). */
		DHD_INTR(("%s: Registering and masking interrupts\n", __FUNCTION__));
		bus->intr_enabled = FALSE;
		dhdpcie_bus_intr_disable(bus);
		ret = dhdpcie_request_irq(dhdpcie_info);
		if (ret) {
			DHD_ERROR(("%s: request_irq() failed, ret=%d\n",
				__FUNCTION__, ret));
			return ret;
		}
	}

	return ret;
}

#ifdef BCMPCIE_OOB_HOST_WAKE
#ifdef CONFIG_BCMDHD_GET_OOB_STATE
extern int dhd_get_wlan_oob_gpio(void);
#ifdef PRINT_WAKEUP_GPIO_STATUS
extern int dhd_get_wlan_oob_gpio_number(void);
#endif /* PRINT_WAKEUP_GPIO_STATUS */
#endif /* CONFIG_BCMDHD_GET_OOB_STATE */

int dhdpcie_get_oob_irq_level(void)
{
	int gpio_level;

#ifdef CONFIG_BCMDHD_GET_OOB_STATE
	gpio_level = dhd_get_wlan_oob_gpio();
#else
	gpio_level = BCME_UNSUPPORTED;
#endif /* CONFIG_BCMDHD_GET_OOB_STATE */
	return gpio_level;
}
#ifdef PRINT_WAKEUP_GPIO_STATUS
int dhdpcie_get_oob_gpio_number(void)
{
	int gpio_number = BCME_UNSUPPORTED;
#ifdef CONFIG_BCMDHD_GET_OOB_STATE
	gpio_number = dhd_get_wlan_oob_gpio_number();
#endif /* CONFIG_BCMDHD_GET_OOB_STATE */
	return gpio_number;
}
#endif /* PRINT_WAKEUP_GPIO_STATUS */
int dhdpcie_get_oob_irq_status(struct dhd_bus *bus)
{
	dhdpcie_info_t *pch;
	dhdpcie_os_info_t *dhdpcie_osinfo;

	if (bus == NULL) {
		DHD_ERROR(("%s: bus is NULL\n", __FUNCTION__));
		return 0;
	}

	if (bus->dev == NULL) {
		DHD_ERROR(("%s: bus->dev is NULL\n", __FUNCTION__));
		return 0;
	}

	pch = pci_get_drvdata(bus->dev);
	if (pch == NULL) {
		DHD_ERROR(("%s: pch is NULL\n", __FUNCTION__));
		return 0;
	}

	dhdpcie_osinfo = (dhdpcie_os_info_t *)pch->os_cxt;

	return dhdpcie_osinfo ? dhdpcie_osinfo->oob_irq_enabled : 0;
}

int dhdpcie_get_oob_irq_num(struct dhd_bus *bus)
{
	dhdpcie_info_t *pch;
	dhdpcie_os_info_t *dhdpcie_osinfo;

	if (bus == NULL) {
		DHD_ERROR(("%s: bus is NULL\n", __FUNCTION__));
		return 0;
	}

	if (bus->dev == NULL) {
		DHD_ERROR(("%s: bus->dev is NULL\n", __FUNCTION__));
		return 0;
	}

	pch = pci_get_drvdata(bus->dev);
	if (pch == NULL) {
		DHD_ERROR(("%s: pch is NULL\n", __FUNCTION__));
		return 0;
	}

	dhdpcie_osinfo = (dhdpcie_os_info_t *)pch->os_cxt;

	return dhdpcie_osinfo ? dhdpcie_osinfo->oob_irq_num : 0;
}

void dhdpcie_oob_intr_set(dhd_bus_t *bus, bool enable)
{
	unsigned long flags;
	dhdpcie_info_t *pch;
	dhdpcie_os_info_t *dhdpcie_osinfo;

	if (bus == NULL) {
		DHD_ERROR(("%s: bus is NULL\n", __FUNCTION__));
		return;
	}

	if (bus->dev == NULL) {
		DHD_ERROR(("%s: bus->dev is NULL\n", __FUNCTION__));
		return;
	}

	pch = pci_get_drvdata(bus->dev);
	if (pch == NULL) {
		DHD_ERROR(("%s: pch is NULL\n", __FUNCTION__));
		return;
	}

	dhdpcie_osinfo = (dhdpcie_os_info_t *)pch->os_cxt;
	DHD_OOB_IRQ_LOCK(&dhdpcie_osinfo->oob_irq_spinlock, flags);
	if ((dhdpcie_osinfo->oob_irq_enabled != enable) &&
		(dhdpcie_osinfo->oob_irq_num > 0)) {
		if (enable) {
			enable_irq(dhdpcie_osinfo->oob_irq_num);
			bus->oob_intr_enable_count++;
			bus->last_oob_irq_enable_time = OSL_LOCALTIME_NS();
		} else {
			disable_irq_nosync(dhdpcie_osinfo->oob_irq_num);
			bus->oob_intr_disable_count++;
			bus->last_oob_irq_disable_time = OSL_LOCALTIME_NS();
		}
		dhdpcie_osinfo->oob_irq_enabled = enable;
	}
	DHD_OOB_IRQ_UNLOCK(&dhdpcie_osinfo->oob_irq_spinlock, flags);
}

#if defined(DHD_USE_SPIN_LOCK_BH) && !defined(DHD_USE_PCIE_OOB_THREADED_IRQ)
#error "Cannot enable DHD_USE_SPIN_LOCK_BH without enabling DHD_USE_PCIE_OOB_THREADED_IRQ"
#endif /* DHD_USE_SPIN_LOCK_BH && !DHD_USE_PCIE_OOB_THREADED_IRQ */

#ifdef DHD_USE_PCIE_OOB_THREADED_IRQ
static irqreturn_t wlan_oob_irq_isr(int irq, void *data)
{
	dhd_bus_t *bus = (dhd_bus_t *)data;
	dhdpcie_oob_intr_set(bus, FALSE);
	DHD_INTR(("%s: IRQ ISR\n", __FUNCTION__));
	bus->last_oob_irq_isr_time = OSL_LOCALTIME_NS();
	return IRQ_WAKE_THREAD;
}
#endif /* DHD_USE_PCIE_OOB_THREADED_IRQ */

static irqreturn_t wlan_oob_irq(int irq, void *data)
{
	dhd_bus_t *bus;
	bus = (dhd_bus_t *)data;
#ifdef DHD_USE_PCIE_OOB_THREADED_IRQ
	DHD_INTR(("%s: IRQ Thread\n", __FUNCTION__));
	bus->last_oob_irq_thr_time = OSL_LOCALTIME_NS();
#else
	dhdpcie_oob_intr_set(bus, FALSE);
	DHD_INTR(("%s: IRQ ISR\n", __FUNCTION__));
	bus->last_oob_irq_isr_time = OSL_LOCALTIME_NS();
#endif /* DHD_USE_PCIE_OOB_THREADED_IRQ */

	if (bus->dhd->up == 0) {
		DHD_ERROR(("%s: ########### IRQ during dhd pub up is 0 ############\n",
			__FUNCTION__));
	}

	bus->oob_intr_count++;
#ifdef DHD_WAKE_STATUS
	{
		bcmpcie_set_get_wake(bus, 1);
	}
#endif /* DHD_WAKE_STATUS */
#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
	dhd_bus_wakeup_work(bus->dhd);
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */
	/* Hold wakelock if bus_low_power_state is
	 * DHD_BUS_D3_INFORM_SENT OR DHD_BUS_D3_ACK_RECIEVED
	 */
	if (bus->dhd->up && DHD_CHK_BUS_IN_LPS(bus)) {
		DHD_OS_OOB_IRQ_WAKE_LOCK_TIMEOUT(bus->dhd, OOB_WAKE_LOCK_TIMEOUT);
	}
	return IRQ_HANDLED;
}

int dhdpcie_oob_intr_register(dhd_bus_t *bus)
{
	int err = 0;
	dhdpcie_info_t *pch;
	dhdpcie_os_info_t *dhdpcie_osinfo;

	DHD_TRACE(("%s: Enter\n", __FUNCTION__));
	if (bus == NULL) {
		DHD_ERROR(("%s: bus is NULL\n", __FUNCTION__));
		return -EINVAL;
	}

	if (bus->dev == NULL) {
		DHD_ERROR(("%s: bus->dev is NULL\n", __FUNCTION__));
		return -EINVAL;
	}

	pch = pci_get_drvdata(bus->dev);
	if (pch == NULL) {
		DHD_ERROR(("%s: pch is NULL\n", __FUNCTION__));
		return -EINVAL;
	}

	dhdpcie_osinfo = (dhdpcie_os_info_t *)pch->os_cxt;
	if (dhdpcie_osinfo->oob_irq_registered) {
		DHD_ERROR(("%s: irq is already registered\n", __FUNCTION__));
		return -EBUSY;
	}

	if (dhdpcie_osinfo->oob_irq_num > 0) {
		printf("%s OOB irq=%d flags=0x%X\n", __FUNCTION__,
			(int)dhdpcie_osinfo->oob_irq_num,
			(int)dhdpcie_osinfo->oob_irq_flags);
#ifdef DHD_USE_PCIE_OOB_THREADED_IRQ
		err = request_threaded_irq(dhdpcie_osinfo->oob_irq_num,
			wlan_oob_irq_isr, wlan_oob_irq,
			dhdpcie_osinfo->oob_irq_flags, "dhdpcie_host_wake",
			bus);
#else
		err = request_irq(dhdpcie_osinfo->oob_irq_num, wlan_oob_irq,
			dhdpcie_osinfo->oob_irq_flags, "dhdpcie_host_wake",
			bus);
#endif /* DHD_USE_THREADED_IRQ_PCIE_OOB */
		if (err) {
			DHD_ERROR(("%s: request_irq failed with %d\n",
				__FUNCTION__, err));
			return err;
		}
#if defined(DISABLE_WOWLAN)
		printf("%s: disable_irq_wake\n", __FUNCTION__);
		dhdpcie_osinfo->oob_irq_wake_enabled = FALSE;
#else
		printf("%s: enable_irq_wake\n", __FUNCTION__);
		err = enable_irq_wake(dhdpcie_osinfo->oob_irq_num);
		if (!err) {
			dhdpcie_osinfo->oob_irq_wake_enabled = TRUE;
		} else
			printf("%s: enable_irq_wake failed with %d\n", __FUNCTION__, err);
#endif
		dhdpcie_osinfo->oob_irq_enabled = TRUE;
	}

	dhdpcie_osinfo->oob_irq_registered = TRUE;

	return 0;
}

void dhdpcie_oob_intr_unregister(dhd_bus_t *bus)
{
	int err = 0;
	dhdpcie_info_t *pch;
	dhdpcie_os_info_t *dhdpcie_osinfo;

	DHD_TRACE(("%s: Enter\n", __FUNCTION__));
	if (bus == NULL) {
		DHD_ERROR(("%s: bus is NULL\n", __FUNCTION__));
		return;
	}

	if (bus->dev == NULL) {
		DHD_ERROR(("%s: bus->dev is NULL\n", __FUNCTION__));
		return;
	}

	pch = pci_get_drvdata(bus->dev);
	if (pch == NULL) {
		DHD_ERROR(("%s: pch is NULL\n", __FUNCTION__));
		return;
	}

	dhdpcie_osinfo = (dhdpcie_os_info_t *)pch->os_cxt;
	if (!dhdpcie_osinfo->oob_irq_registered) {
		DHD_ERROR(("%s: irq is not registered\n", __FUNCTION__));
		return;
	}
	if (dhdpcie_osinfo->oob_irq_num > 0) {
		if (dhdpcie_osinfo->oob_irq_wake_enabled) {
			err = disable_irq_wake(dhdpcie_osinfo->oob_irq_num);
			if (!err) {
				dhdpcie_osinfo->oob_irq_wake_enabled = FALSE;
			}
		}
		if (dhdpcie_osinfo->oob_irq_enabled) {
			disable_irq(dhdpcie_osinfo->oob_irq_num);
			dhdpcie_osinfo->oob_irq_enabled = FALSE;
		}
		free_irq(dhdpcie_osinfo->oob_irq_num, bus);
	}
	dhdpcie_osinfo->oob_irq_registered = FALSE;
}
#endif /* BCMPCIE_OOB_HOST_WAKE */

#ifdef PCIE_OOB
void dhdpcie_oob_init(dhd_bus_t *bus)
{
	/* XXX this should be passed in as a command line parameter */
	gpio_handle_val = get_handle(OOB_PORT);
	if (gpio_handle_val < 0)
	{
		DHD_ERROR(("%s: Could not get GPIO handle.\n", __FUNCTION__));
		ASSERT(FALSE);
	}

	gpio_direction = 0;
	ftdi_set_bitmode(gpio_handle_val, 0, BITMODE_BITBANG);

	/* Note BT core is also enabled here */
	gpio_port = 1 << BIT_WL_REG_ON | 1 << BIT_BT_REG_ON | 1 << DEVICE_WAKE;
	gpio_write_port(gpio_handle_val, gpio_port);

	gpio_direction = 1 << BIT_WL_REG_ON | 1 << BIT_BT_REG_ON | 1 << DEVICE_WAKE;
	ftdi_set_bitmode(gpio_handle_val, gpio_direction, BITMODE_BITBANG);

	bus->oob_enabled = TRUE;
	bus->oob_presuspend = FALSE;

	/* drive the Device_Wake GPIO low on startup */
	bus->device_wake_state = TRUE;
	dhd_bus_set_device_wake(bus, FALSE);
	dhd_bus_doorbell_timeout_reset(bus);

}

void
dhd_oob_set_bt_reg_on(struct dhd_bus *bus, bool val)
{
	DHD_INFO(("Set Device_Wake to %d\n", val));
	if (val)
	{
		gpio_port = gpio_port | (1 << BIT_BT_REG_ON);
		gpio_write_port(gpio_handle_val, gpio_port);
	} else {
		gpio_port = gpio_port & (0xff ^ (1 << BIT_BT_REG_ON));
		gpio_write_port(gpio_handle_val, gpio_port);
	}
}

int
dhd_oob_get_bt_reg_on(struct dhd_bus *bus)
{
	int ret;
	uint8 val;
	ret = gpio_read_port(gpio_handle_val, &val);

	if (ret < 0) {
		/* XXX handle error properly */
		DHD_ERROR(("gpio_read_port returns %d\n", ret));
		return ret;
	}

	if (val & (1 << BIT_BT_REG_ON))
	{
		ret = 1;
	} else {
		ret = 0;
	}

	return ret;
}

int
dhd_os_oob_set_device_wake(struct dhd_bus *bus, bool val)
{
	if (bus->device_wake_state != val)
	{
		DHD_INFO(("Set Device_Wake to %d\n", val));

		if (bus->oob_enabled && !bus->oob_presuspend)
		{
			if (val)
			{
				gpio_port = gpio_port | (1 << DEVICE_WAKE);
				gpio_write_port_non_block(gpio_handle_val, gpio_port);
			} else {
				gpio_port = gpio_port & (0xff ^ (1 << DEVICE_WAKE));
				gpio_write_port_non_block(gpio_handle_val, gpio_port);
			}
		}

		bus->device_wake_state = val;
	}
	return BCME_OK;
}

INLINE void
dhd_os_ib_set_device_wake(struct dhd_bus *bus, bool val)
{
	/* TODO: Currently Inband implementation of Device_Wake is not supported,
	 * so this function is left empty later this can be used to support the same.
	 */
}
#endif /* PCIE_OOB */

struct device * dhd_bus_to_dev(dhd_bus_t *bus)
{
	struct pci_dev *pdev;
	pdev = bus->dev;

	if (pdev)
		return &pdev->dev;
	else
		return NULL;
}

#ifdef DHD_FW_COREDUMP
int
dhd_dongle_mem_dump(void)
{
	if (!g_dhd_bus) {
		DHD_ERROR(("%s: Bus is NULL\n", __FUNCTION__));
		return -ENODEV;
	}

	dhd_bus_dump_console_buffer(g_dhd_bus);
	dhd_prot_debug_info_print(g_dhd_bus->dhd);

	g_dhd_bus->dhd->memdump_enabled = DUMP_MEMFILE_BUGON;
	g_dhd_bus->dhd->memdump_type = DUMP_TYPE_AP_ABNORMAL_ACCESS;

	dhd_bus_mem_dump(g_dhd_bus->dhd);
	return 0;
}
#ifndef BCMDHD_MDRIVER
EXPORT_SYMBOL(dhd_dongle_mem_dump);
#endif
#endif /* DHD_FW_COREDUMP */

#if defined(CONFIG_ARCH_MSM) && defined(CONFIG_SEC_PCIE_L1SS)
void
dhd_bus_inform_ep_loaded_to_rc(dhd_pub_t *dhdp, bool up)
{
	if (dhdp->bus->rc_dev) {
		sec_pcie_set_ep_driver_loaded(dhdp->bus->rc_dev, up);
	}
}
#endif /* CONFIG_ARCH_MSM && CONFIG_SEC_PCIE_L1SS */

bool
dhd_bus_check_driver_up(void)
{
	dhd_bus_t *bus;
	dhd_pub_t *dhdp;
	bool isup = FALSE;

	bus = (dhd_bus_t *)g_dhd_bus;
	if (!bus) {
		DHD_ERROR(("%s: bus is NULL\n", __FUNCTION__));
		return isup;
	}

	dhdp = bus->dhd;
	if (dhdp) {
		isup = dhdp->up;
	}

	return isup;
}
#ifndef BCMDHD_MDRIVER
EXPORT_SYMBOL(dhd_bus_check_driver_up);
#endif
