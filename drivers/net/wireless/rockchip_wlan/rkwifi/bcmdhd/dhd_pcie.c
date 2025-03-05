/*
 * DHD Bus Module for PCIE
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

/** XXX Twiki: [PCIeFullDongleArchitecture] */

/* include files */
#include <typedefs.h>
#include <bcmutils.h>
#include <bcmrand.h>
#include <bcmdevs.h>
#include <bcmdevs_legacy.h>    /* need to still support chips no longer in trunk firmware */
#include <siutils.h>
#include <hndoobr.h>
#include <hndsoc.h>
#include <hndpmu.h>
#include <etd.h>
#include <hnd_debug.h>
#include <sbchipc.h>
#include <sbhndarm.h>
#include <sbsysmem.h>
#include <hnd_armtrap.h>
#if defined(DHD_DEBUG)
#include <hnd_cons.h>
#endif /* defined(DHD_DEBUG) */
#include <dngl_stats.h>
#include <pcie_core.h>
#include <dhd.h>
#include <dhd_bus.h>
#include <dhd_flowring.h>
#include <dhd_proto.h>
#include <dhd_dbg.h>
#include <dhd_debug.h>
#if defined(__linux__)
#include <dhd_daemon.h>
#include <dhd_linux.h>
#include <dhd_plat.h>
#endif /* __linux__ */
#include <dhdioctl.h>
#include <sdiovar.h>
#include <bcmmsgbuf.h>
#include <pcicfg.h>
#include <dhd_pcie.h>
#include <bcmpcie.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmstdlib_s.h>
#ifdef DHDTCPACK_SUPPRESS
#include <dhd_ip.h>
#endif /* DHDTCPACK_SUPPRESS */
#include <bcmevent.h>
#include <dhd_config.h>

#if IS_ENABLED(CONFIG_PCIEASPM_ROCKCHIP_WIFI_EXTENSION)
#include <rk_dhd_pcie_linux.h>
#endif /* CONFIG_PCIEASPM_ROCKCHIP_WIFI_EXTENSION */
#ifdef BCM_ROUTER_DHD
#include <bcmnvram.h>
#define STR_END		"END\0\0"
#define BOARDREV_PROMOTABLE_STR	"0xff"
#endif
#if defined(BCMEMBEDIMAGE)
#include BCMEMBEDIMAGE
#endif /* BCMEMBEDIMAGE */

#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
#include <linux/pm_runtime.h>
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

#if defined(DEBUGGER) || defined(DHD_DSCOPE)
#include <debugger.h>
#endif /* DEBUGGER || DHD_DSCOPE */

#include <dngl_rtlv.h>
#if defined(FW_SIGNATURE)
#include <bootrommem.h>
#include <fwpkg_utils.h>
#endif /* FW_SIGNATURE */

#ifdef DNGL_AXI_ERROR_LOGGING
#include <dhd_linux_wq.h>
#include <dhd_linux.h>
#endif /* DNGL_AXI_ERROR_LOGGING */

#if defined(DHD_CONTROL_PCIE_CPUCORE_WIFI_TURNON)
#include <dhd_linux_priv.h>
#endif /* DHD_CONTROL_PCIE_CPUCORE_WIFI_TURNON */
#include <dhd_linux_pktdump.h>

#ifdef CONFIG_ARCH_ROCKCHIP
#include <linux/aspm_ext.h>
#endif

#define EXTENDED_PCIE_DEBUG_DUMP 1	/* Enable Extended pcie registers dump */

#define MEMBLOCK	2048		/* Block size used for downloading of dongle image */
#ifdef LINUX
#define MAX_WKLK_IDLE_CHECK	3 /* times dhd_wake_lock checked before deciding not to suspend */
#endif /* LINUX */

#define	DHD_MAX_ITEMS_HPP_TXCPL_RING	512
#define	DHD_MAX_ITEMS_HPP_RXCPL_RING	512
#define MAX_HP2P_CMPL_RINGS		2u

/* XXX defines for 4378 */
#define ARMCR4REG_CORECAP	(0x4/sizeof(uint32))
#define ARMCR4REG_MPUCTRL	(0x90/sizeof(uint32))
#define ACC_MPU_SHIFT		25
#define ACC_MPU_MASK		(0x1u << ACC_MPU_SHIFT)

/* XXX Offset for 4375 work around register */
#define REG_WORK_AROUND		(0x1e4/sizeof(uint32))

/* XXX defines for 43602a0 workaround JIRA CRWLARMCR4-53 */
#define ARMCR4REG_BANKIDX	(0x40/sizeof(uint32))
#define ARMCR4REG_BANKPDA	(0x4C/sizeof(uint32))
/* Temporary war to fix precommit till sync issue between trunk & precommit branch is resolved */

/* CTO Prevention Recovery */
#define CTO_TO_CLEAR_WAIT_MS 50
#define CTO_TO_CLEAR_WAIT_MAX_CNT 200

/* FLR setting */
#define PCIE_FLR_CAPAB_BIT		28
#define PCIE_FUNCTION_LEVEL_RESET_BIT	15

#ifdef BCMQT_HW
extern int qt_flr_reset;
/* FLR takes longer on QT Z boards so increasing the delay by 30% */
#define DHD_FUNCTION_LEVEL_RESET_DELAY	70u
#define DHD_SSRESET_STATUS_RETRY_DELAY	55u
#else
#define DHD_FUNCTION_LEVEL_RESET_DELAY	70u	/* 70 msec delay */
#define DHD_SSRESET_STATUS_RETRY_DELAY	40u
#endif /* BCMQT_HW */
/*
 * Increase SSReset de-assert time to 8ms.
 * since it takes longer time if re-scan time on 4378B0.
 */
#define DHD_SSRESET_STATUS_RETRIES	200u

/* Fetch address of a member in the pciedev_shared structure in dongle memory */
#define DHD_PCIE_SHARED_MEMBER_ADDR(bus, member) \
	(bus)->shared_addr + OFFSETOF(pciedev_shared_t, member)

/* Fetch address of a member in rings_info_ptr structure in dongle memory */
#define DHD_RING_INFO_MEMBER_ADDR(bus, member) \
	(bus)->pcie_sh->rings_info_ptr + OFFSETOF(ring_info_t, member)

/* Fetch address of a member in the ring_mem structure in dongle memory */
#define DHD_RING_MEM_MEMBER_ADDR(bus, ringid, member) \
	(bus)->ring_sh[ringid].ring_mem_addr + OFFSETOF(ring_mem_t, member)

/* Mpu conntrol */
#define MPU_REG(regs, reg)	(&((sysmemregs_t *)regs)->reg)
#define BUS_MPU_ENABLE_MASK	0x1u
#define BUS_MPU_ROM_PR_MASK	0x2u
#define BUS_MPU_VEC_PR_MASK	0x4u
#define BUS_MPU_LOCK_MASK	0x8u

#if defined(SUPPORT_MULTIPLE_BOARD_REV)
extern unsigned int system_rev;
#endif /* SUPPORT_MULTIPLE_BOARD_REV */

#ifdef EWP_EDL
extern int host_edl_support;
#endif

#ifdef BCMQT_HW
extern int qt_dngl_timeout;
#endif /* BCMQT_HW */

#define CONS_ADDR_SIGNATURE	0xac0ffee
/* This can be overwritten by module parameter(dma_ring_indices) defined in dhd_linux.c */
uint dma_ring_indices = 0;
/* This can be overwritten by module parameter(h2d_phase) defined in dhd_linux.c */
bool h2d_phase = 0;
/* This can be overwritten by module parameter(force_trap_bad_h2d_phase)
 * defined in dhd_linux.c
 */
bool force_trap_bad_h2d_phase = 0;

int dhd_dongle_ramsize;
struct dhd_bus *g_dhd_bus = NULL;
#ifdef DNGL_AXI_ERROR_LOGGING
static void dhd_log_dump_axi_error(uint8 *axi_err);
#endif /* DNGL_AXI_ERROR_LOGGING */

static int dhdpcie_checkdied(dhd_bus_t *bus, char *data, uint size);
static int dhdpcie_bus_readconsole(dhd_bus_t *bus);
#if defined(DHD_FW_COREDUMP)
static int dhdpcie_mem_dump(dhd_bus_t *bus);
static int dhdpcie_get_mem_dump(dhd_bus_t *bus);
#endif /* DHD_FW_COREDUMP */

static int dhdpcie_bus_membytes(dhd_bus_t *bus, bool write, ulong address, uint8 *data, uint size);
static int dhdpcie_bus_doiovar(dhd_bus_t *bus, const bcm_iovar_t *vi, uint32 actionid,
	const char *name, void *params,
	uint plen, void *arg, uint len, int val_size);
static int dhdpcie_bus_lpback_req(struct  dhd_bus *bus, uint32 intval);
static int dhdpcie_bus_dmaxfer_req(struct  dhd_bus *bus,
	uint32 len, uint32 srcdelay, uint32 destdelay,
	uint32 d11_lpbk, uint32 core_num, uint32 wait,
	uint32 mem_addr);
static uint serialized_backplane_access(dhd_bus_t* bus, uint addr, uint size, uint* val, bool read);
static int dhdpcie_bus_download_state(dhd_bus_t *bus, bool enter);
static int _dhdpcie_download_firmware(struct dhd_bus *bus);
static int dhdpcie_download_firmware(dhd_bus_t *bus, osl_t *osh);

#if defined(FW_SIGNATURE)
static int dhdpcie_bus_download_fw_signature(dhd_bus_t *bus, bool *do_write);
static int dhdpcie_bus_write_fw_signature(dhd_bus_t *bus);
static int dhdpcie_bus_download_ram_bootloader(dhd_bus_t *bus);
static int dhdpcie_bus_write_fws_status(dhd_bus_t *bus);
static int dhdpcie_bus_write_fws_mem_info(dhd_bus_t *bus);
static int dhdpcie_bus_write_fwsig(dhd_bus_t *bus, char *fwsig_path, char *nvsig_path);
static int dhdpcie_download_rtlv_end(dhd_bus_t *bus);
static int dhdpcie_bus_save_download_info(dhd_bus_t *bus, uint32 download_addr,
	uint32 download_size, const char *signature_fname,
	const char *bloader_fname, uint32 bloader_download_addr);
static int dhdpcie_read_fwstatus(dhd_bus_t *bus, bl_verif_status_t *status);
#endif /* FW_SIGNATURE */

static int dhdpcie_bus_write_vars(dhd_bus_t *bus);
static bool dhdpcie_bus_process_mailbox_intr(dhd_bus_t *bus, uint32 intstatus);
static bool dhdpci_bus_read_frames(dhd_bus_t *bus);
static int dhdpcie_readshared(dhd_bus_t *bus);
static void dhdpcie_init_shared_addr(dhd_bus_t *bus);
static bool dhdpcie_dongle_attach(dhd_bus_t *bus);
static void dhdpcie_bus_dongle_setmemsize(dhd_bus_t *bus, int mem_size);
static void dhdpcie_bus_release_dongle(dhd_bus_t *bus, osl_t *osh,
	bool dongle_isolation, bool reset_flag);
static void dhdpcie_bus_release_malloc(dhd_bus_t *bus, osl_t *osh);
static int dhdpcie_downloadvars(dhd_bus_t *bus, void *arg, int len);
static void dhdpcie_setbar1win(dhd_bus_t *bus, uint32 addr);
static void dhd_deinit_bar1_switch_lock(dhd_bus_t *bus);
static void dhd_init_pwr_req_lock(dhd_bus_t *bus);
static void dhd_deinit_pwr_req_lock(dhd_bus_t *bus);
static void dhd_init_bus_lp_state_lock(dhd_bus_t *bus);
static void dhd_deinit_bus_lp_state_lock(dhd_bus_t *bus);
static void dhd_init_backplane_access_lock(dhd_bus_t *bus);
static void dhd_deinit_backplane_access_lock(dhd_bus_t *bus);
static uint8 dhdpcie_bus_rtcm8(dhd_bus_t *bus, ulong offset);
static void dhdpcie_bus_wtcm8(dhd_bus_t *bus, ulong offset, uint8 data);
static void dhdpcie_bus_wtcm16(dhd_bus_t *bus, ulong offset, uint16 data);
static uint16 dhdpcie_bus_rtcm16(dhd_bus_t *bus, ulong offset);
static void dhdpcie_bus_wtcm32(dhd_bus_t *bus, ulong offset, uint32 data);
static uint32 dhdpcie_bus_rtcm32(dhd_bus_t *bus, ulong offset);
#ifdef DHD_SUPPORT_64BIT
static void dhdpcie_bus_wtcm64(dhd_bus_t *bus, ulong offset, uint64 data) __attribute__ ((used));
static uint64 dhdpcie_bus_rtcm64(dhd_bus_t *bus, ulong offset) __attribute__ ((used));
#endif /* DHD_SUPPORT_64BIT */
static void dhdpcie_bus_cfg_set_bar0_win(dhd_bus_t *bus, uint32 data);
static void dhdpcie_bus_reg_unmap(osl_t *osh, volatile char *addr, int size);
static int dhdpcie_cc_nvmshadow(dhd_bus_t *bus, struct bcmstrbuf *b);
static void dhd_bus_dump_rxlat_info(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf);
static void dhd_bus_dump_rxlat_histo(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf);
static void dhd_bus_dump_txcpl_info(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf);
static void dhd_bus_dump_mdring_info(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf);
static bool dhd_bus_support_dar_sec_status(dhd_bus_t *bus);
static int dhd_bus_security_info(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf);
static int dhd_bus_sboot_disable(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf);
static int dhd_bus_get_security_status(dhd_bus_t *bus, uint32 *status);
static void dhdpcie_fw_trap(dhd_bus_t *bus);
static void dhd_fillup_ring_sharedptr_info(dhd_bus_t *bus, ring_info_t *ring_info);
static void dhdpcie_handle_mb_data(dhd_bus_t *bus);
extern void dhd_dpc_enable(dhd_pub_t *dhdp);
#ifdef PCIE_INB_DW
static void dhd_bus_ds_trace(dhd_bus_t *bus, uint32 dsval,
	bool d2h, enum dhd_bus_ds_state inbstate);
#else
static void dhd_bus_ds_trace(dhd_bus_t *bus, uint32 dsval, bool d2h);
#endif /* PCIE_INB_DW */
#ifdef DHD_MMIO_TRACE
static void dhd_bus_mmio_trace(dhd_bus_t *bus, uint32 addr, uint32 value, bool set);
#endif /* defined(DHD_MMIO_TRACE) */

#ifdef IDLE_TX_FLOW_MGMT
static void dhd_bus_check_idle_scan(dhd_bus_t *bus);
static void dhd_bus_idle_scan(dhd_bus_t *bus);
#endif /* IDLE_TX_FLOW_MGMT */

#ifdef BCMEMBEDIMAGE
static int dhdpcie_download_code_array(dhd_bus_t *bus);
#endif /* BCMEMBEDIMAGE */
#ifdef BCM_ROUTER_DHD
extern char * nvram_get(const char *name);
#endif
#if defined(BCMEMBEDIMAGE) && defined(BCM_ROUTER_DHD)
static void  select_fd_image(
		struct dhd_bus *bus, unsigned char **p_dlarray,
		char **p_dlimagename, char **p_dlimagever,
		char **p_dlimagedate, int *image_size);
#endif /* defined(BCMEMBEDIMAGE) && defined (BCM_ROUTER_DHD) */

#ifdef BCM_ROUTER_DHD
int dbushost_initvars_flash(si_t *sih, osl_t *osh, char **base, uint len);
#endif

#if defined(DHD_H2D_LOG_TIME_SYNC)
static void dhdpci_bus_rte_log_time_sync_poll(dhd_bus_t *bus);
#endif /* DHD_H2D_LOG_TIME_SYNC */

#define PCI_VENDOR_ID_BROADCOM          0x14e4
#define PCI_VENDOR_ID_SYNAPTICS         0x1e01

#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
#define MAX_D3_ACK_TIMEOUT	100
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

#ifdef BCMQT
#define DHD_DEFAULT_DOORBELL_TIMEOUT 40	/* ms */
#else
#define DHD_DEFAULT_DOORBELL_TIMEOUT 200	/* ms */
#endif
#if defined(PCIE_OOB) || defined(PCIE_INB_DW)
static uint dhd_doorbell_timeout = DHD_DEFAULT_DOORBELL_TIMEOUT;
#endif /* PCIE_OOB || PCIE_INB_DW */

static bool dhdpcie_check_firmware_compatible(uint32 f_api_version, uint32 h_api_version);
static int dhdpcie_cto_error_recovery(struct dhd_bus *bus);

static int dhdpcie_init_d11status(struct dhd_bus *bus);

static int dhdpcie_wrt_rnd(struct dhd_bus *bus);

#if defined(linux) || defined(LINUX)
#if !defined(DHD_FW_MEM_CORRUPTION) && defined(BCM4375_CHIP)
#define NUM_PATTERNS 6
#else
#define NUM_PATTERNS 2
#endif /* !DHD_FW_MEM_CORRUPTION && BCM4375_CHIP */
static bool dhd_bus_tcm_test(struct dhd_bus *bus);
#endif /* LINUX || linux */

#if defined(FW_SIGNATURE)
static int dhd_bus_dump_fws(dhd_bus_t *bus, struct bcmstrbuf *strbuf);
#endif
static void dhdpcie_pme_stat_clear(dhd_bus_t *bus);

static int dhdpcie_download_rtlv(dhd_bus_t *bus, dngl_rtlv_type_t type, dngl_rtlv_len_t len,
	uint8 *value);
#if defined(SUPPORT_MULTIPLE_BOARD_REVISION)
extern void concate_custom_board_revision(char *nv_path);
#endif /* SUPPORT_MULTIPLE_BOARD_REVISION */

/* IOVar table */
enum {
	IOV_INTR = 1,
#ifdef DHD_BUS_MEM_ACCESS
	IOV_MEMBYTES,
#endif /* DHD_BUS_MEM_ACCESS */
	IOV_MEMSIZE,
	IOV_SET_DOWNLOAD_STATE,
	IOV_SET_DOWNLOAD_INFO,
	IOV_DEVRESET,
	IOV_VARS,
	IOV_MSI_SIM,
	IOV_PCIE_LPBK,
	IOV_CC_NVMSHADOW,
	IOV_RAMSIZE,
	IOV_RAMSTART,
	IOV_SLEEP_ALLOWED,
	IOV_PCIE_DMAXFER,
	IOV_PCIE_SUSPEND,
#ifdef DHD_PCIE_REG_ACCESS
	IOV_PCIEREG,
	IOV_PCIECFGREG,
	IOV_PCIECOREREG,
	IOV_PCIESERDESREG,
	IOV_PCIEASPM,
	IOV_BAR0_SECWIN_REG,
	IOV_SBREG,
#endif /* DHD_PCIE_REG_ACCESS */
	IOV_DONGLEISOLATION,
	IOV_LTRSLEEPON_UNLOOAD,
	IOV_METADATA_DBG,
	IOV_RX_METADATALEN,
	IOV_TX_METADATALEN,
	IOV_TXP_THRESHOLD,
	IOV_BUZZZ_DUMP,
	IOV_DUMP_RINGUPD_BLOCK,
	IOV_DMA_RINGINDICES,
	IOV_FORCE_FW_TRAP,
	IOV_DB1_FOR_MB,
	IOV_FLOW_PRIO_MAP,
	IOV_RX_CPL_POST_BOUND,
	IOV_TX_CPL_BOUND,
	IOV_CTRL_CPL_POST_BOUND,
	IOV_TX_POST_BOUND,
	IOV_HANGREPORT,
	IOV_H2D_MAILBOXDATA,
	IOV_INFORINGS,
	IOV_H2D_PHASE,
	IOV_H2D_ENABLE_TRAP_BADPHASE,
	IOV_RING_SIZE_VER,
	IOV_H2D_TXPOST_MAX_ITEM,
	IOV_H2D_HTPUT_TXPOST_MAX_ITEM,
	IOV_D2H_TXCPL_MAX_ITEM,
	IOV_H2D_RXPOST_MAX_ITEM,
	IOV_D2H_RXCPL_MAX_ITEM,
	IOV_H2D_CTRLPOST_MAX_ITEM,
	IOV_D2H_CTRLCPL_MAX_ITEM,
	IOV_RX_BUF_BURST,
	IOV_RX_BUFPOST_THRESHOLD,
	IOV_TRAPDATA,
	IOV_TRAPDATA_RAW,
	IOV_CTO_PREVENTION,
#ifdef PCIE_OOB
	IOV_OOB_BT_REG_ON,
	IOV_OOB_ENABLE,
#endif /* PCIE_OOB */
#ifdef DEVICE_TX_STUCK_DETECT
	IOV_DEVICE_TX_STUCK_DETECT,
#endif /* DEVICE_TX_STUCK_DETECT */
	IOV_PCIE_WD_RESET,
	IOV_DUMP_DONGLE,
	IOV_IDMA_ENABLE,
	IOV_IFRM_ENABLE,
	IOV_CLEAR_RING,
	IOV_DAR_ENABLE,
	IOV_DHD_CAPS,   /**< returns string with dhd capabilities */
#if defined(DEBUGGER) || defined(DHD_DSCOPE)
	IOV_GDB_SERVER,  /**< starts gdb server on given interface */
#endif /* DEBUGGER || DHD_DSCOPE */
#if defined(GDB_PROXY)
	IOV_GDB_PROXY_PROBE, /**< gdb proxy support presence check */
	IOV_GDB_PROXY_STOP_COUNT, /**< gdb proxy firmware stop count */
#endif /* GDB_PROXY */
	IOV_INB_DW_ENABLE,
#if defined(PCIE_OOB) || defined(PCIE_INB_DW)
	IOV_DEEP_SLEEP,
#endif /* PCIE_OOB || PCIE_INB_DW */
	IOV_CTO_THRESHOLD,
#ifdef D2H_MINIDUMP
	IOV_MINIDUMP_OVERRIDE,
#endif /* D2H_MINIDUMP */
	IOV_HSCBSIZE, /* get HSCB buffer size */
#ifdef DHD_BUS_MEM_ACCESS
	IOV_HSCBBYTES, /* copy HSCB buffer */
#endif
	IOV_HP2P_ENABLE,
	IOV_HP2P_PKT_THRESHOLD,
	IOV_HP2P_TIME_THRESHOLD,
	IOV_HP2P_PKT_EXPIRY,
	IOV_HP2P_TXCPL_MAXITEMS,
	IOV_HP2P_RXCPL_MAXITEMS,
	IOV_EXTDTXS_IN_TXCPL,
	IOV_HOSTRDY_AFTER_INIT,
	IOV_HP2P_MF_ENABLE,

	IOV_DONGLE_RXLAT_INFO,
	IOV_DONGLE_RXLAT_HISTO,
	IOV_DONGLE_TXCPL_INFO,
	IOV_DONGLE_SEC_INFO,
	IOV_DONGLE_SBOOT_DIS,
	IOV_LPM_MODE,
	IOV_MDRING_LINK,
	IOV_MDRING_UNLINK,
	IOV_MDRING_DUMP,
	IOV_PTM_ENABLE,
	IOV_PCIE_DMAXFER_PTRN,

	IOV_PCIE_LAST /**< unused IOVAR */
};

const bcm_iovar_t dhdpcie_iovars[] = {
	{"intr",	IOV_INTR,	0, 	0, IOVT_BOOL,	0 },
#ifdef DHD_BUS_MEM_ACCESS
	{"membytes",	IOV_MEMBYTES,	0, 	0, IOVT_BUFFER,	2 * sizeof(int) },
#endif /* DHD_BUS_MEM_ACCESS */
	{"memsize",	IOV_MEMSIZE,	0, 	0, IOVT_UINT32,	0 },
	{"dwnldstate",	IOV_SET_DOWNLOAD_STATE,	0, 	0, IOVT_BOOL,	0 },
	{"dwnldinfo",	IOV_SET_DOWNLOAD_INFO,	0, 	0, IOVT_BUFFER,
	sizeof(fw_download_info_t) },
	{"vars",	IOV_VARS,	0, 	0, IOVT_BUFFER,	0 },
	{"devreset",	IOV_DEVRESET,	0, 	0, IOVT_UINT8,	0 },
	{"pcie_device_trap", IOV_FORCE_FW_TRAP, 0, 	0, 0,	0 },
	{"pcie_lpbk",	IOV_PCIE_LPBK,	0,	0, IOVT_UINT32,	0 },
	{"cc_nvmshadow", IOV_CC_NVMSHADOW, 0,	0, IOVT_BUFFER, 0 },
	{"ramsize",	IOV_RAMSIZE,	0, 	0, IOVT_UINT32,	0 },
	{"ramstart",	IOV_RAMSTART,	0, 	0, IOVT_UINT32,	0 },
#ifdef DHD_PCIE_REG_ACCESS
	{"pciereg",	IOV_PCIEREG,	0, 	0, IOVT_BUFFER,	2 * sizeof(int32) },
	{"pciecfgreg",	IOV_PCIECFGREG,	DHD_IOVF_PWRREQ_BYPASS,	0, IOVT_BUFFER,	2 * sizeof(int32) },
	{"pciecorereg",	IOV_PCIECOREREG,	0, 	0, IOVT_BUFFER,	2 * sizeof(int32) },
	{"pcieserdesreg",	IOV_PCIESERDESREG,	0, 	0, IOVT_BUFFER,	3 * sizeof(int32) },
	{"bar0secwinreg",	IOV_BAR0_SECWIN_REG,	0, 	0, IOVT_BUFFER,	sizeof(sdreg_t) },
	{"sbreg",	IOV_SBREG,	0,	0, IOVT_BUFFER,	sizeof(uint8) },
#endif /* DHD_PCIE_REG_ACCESS */
	{"pcie_dmaxfer", IOV_PCIE_DMAXFER, 0, 0, IOVT_BUFFER, sizeof(dma_xfer_info_t)},
	{"lpbk_dmaxfer_data_pattern", IOV_PCIE_DMAXFER_PTRN, 0, 0, IOVT_UINT32, 0},
	{"pcie_suspend", IOV_PCIE_SUSPEND,	DHD_IOVF_PWRREQ_BYPASS,	0, IOVT_UINT32,	0 },
#ifdef PCIE_OOB
	{"oob_bt_reg_on", IOV_OOB_BT_REG_ON,    0,	0, IOVT_UINT32,    0 },
	{"oob_enable",   IOV_OOB_ENABLE,    0,	0, IOVT_UINT32,    0 },
#endif /* PCIE_OOB */
	{"sleep_allowed",	IOV_SLEEP_ALLOWED,	0,	0, IOVT_BOOL,	0 },
	{"dngl_isolation", IOV_DONGLEISOLATION,	0, 	0, IOVT_UINT32,	0 },
	{"ltrsleep_on_unload", IOV_LTRSLEEPON_UNLOOAD,	0,	0, IOVT_UINT32,	0 },
	{"dump_ringupdblk", IOV_DUMP_RINGUPD_BLOCK,	0, 	0, IOVT_BUFFER,	0 },
	{"dma_ring_indices", IOV_DMA_RINGINDICES,	0, 	0, IOVT_UINT32,	0},
	{"metadata_dbg", IOV_METADATA_DBG,	0,	0, IOVT_BOOL,	0 },
	{"rx_metadata_len", IOV_RX_METADATALEN,	0, 	0, IOVT_UINT32,	0 },
	{"tx_metadata_len", IOV_TX_METADATALEN,	0, 	0, IOVT_UINT32,	0 },
	{"db1_for_mb", IOV_DB1_FOR_MB,	0, 	0, IOVT_UINT32,	0 },
	{"txp_thresh", IOV_TXP_THRESHOLD,	0,	0, IOVT_UINT32,	0 },
	{"buzzz_dump", IOV_BUZZZ_DUMP,		0, 	0, IOVT_UINT32,	0 },
	{"flow_prio_map", IOV_FLOW_PRIO_MAP,	0, 	0, IOVT_UINT32,	0 },
	{"rx_cpl_post_bound",     IOV_RX_CPL_POST_BOUND,    0, 0,	IOVT_UINT32,    0 },
	{"tx_cpl_bound",     IOV_TX_CPL_BOUND,    0, 0,	IOVT_UINT32,    0 },
	{"ctrl_cpl_post_bound",     IOV_CTRL_CPL_POST_BOUND,    0, 0,	IOVT_UINT32,    0 },
	{"tx_post_bound",     IOV_TX_POST_BOUND,    0, 0,	IOVT_UINT32,    0 },
#ifdef DHD_PCIE_REG_ACCESS
	{"aspm", IOV_PCIEASPM, 0, 0, IOVT_INT32, 0 },
#endif /* DHD_PCIE_REG_ACCESS */
	{"fw_hang_report", IOV_HANGREPORT,	0, 0,	IOVT_BOOL,	0 },
	{"h2d_mb_data",     IOV_H2D_MAILBOXDATA,    0, 0,      IOVT_UINT32,    0 },
	{"inforings",   IOV_INFORINGS,    0, 0,      IOVT_UINT32,    0 },
	{"h2d_phase",   IOV_H2D_PHASE,    0, 0,      IOVT_UINT32,    0 },
	{"force_trap_bad_h2d_phase", IOV_H2D_ENABLE_TRAP_BADPHASE,    0, 0,
	IOVT_UINT32,    0 },
	{"ring_size_ver",   IOV_RING_SIZE_VER,    0, 0,      IOVT_UINT32,    0 },
	{"h2d_max_txpost",   IOV_H2D_TXPOST_MAX_ITEM,    0, 0,      IOVT_UINT32,    0 },
	{"h2d_htput_max_txpost", IOV_H2D_HTPUT_TXPOST_MAX_ITEM,    0, 0,      IOVT_UINT32,    0 },
	{"d2h_max_txcpl",   IOV_D2H_TXCPL_MAX_ITEM,    0, 0,      IOVT_UINT32,    0 },
	{"h2d_max_rxpost",   IOV_H2D_RXPOST_MAX_ITEM,    0, 0,      IOVT_UINT32,    0 },
	{"d2h_max_rxcpl",   IOV_D2H_RXCPL_MAX_ITEM,    0, 0,      IOVT_UINT32,    0 },
	{"h2d_max_ctrlpost",   IOV_H2D_CTRLPOST_MAX_ITEM,    0, 0,      IOVT_UINT32,    0 },
	{"d2h_max_ctrlcpl",   IOV_D2H_CTRLCPL_MAX_ITEM,    0, 0,      IOVT_UINT32,    0 },
	{"rx_buf_burst",   IOV_RX_BUF_BURST,    0, 0,      IOVT_UINT32,    0 },
	{"rx_bufpost_threshold",   IOV_RX_BUFPOST_THRESHOLD,    0, 0,      IOVT_UINT32,    0 },
	{"trap_data",	IOV_TRAPDATA,	0, 0,	IOVT_BUFFER,	0 },
	{"trap_data_raw",	IOV_TRAPDATA_RAW,	0, 0,	IOVT_BUFFER,	0 },
	{"cto_prevention",	IOV_CTO_PREVENTION,	0, 0,	IOVT_UINT32,	0 },
	{"pcie_wd_reset",	IOV_PCIE_WD_RESET,	0,	0, IOVT_BOOL,	0 },
#ifdef DEVICE_TX_STUCK_DETECT
	{"dev_tx_stuck_monitor", IOV_DEVICE_TX_STUCK_DETECT, 0, 0, IOVT_UINT32, 0 },
#endif /* DEVICE_TX_STUCK_DETECT */
	{"dump_dongle", IOV_DUMP_DONGLE, 0, 0, IOVT_BUFFER,
	MAX(sizeof(dump_dongle_in_t), sizeof(dump_dongle_out_t))},
	{"clear_ring",   IOV_CLEAR_RING,    0, 0,  IOVT_UINT32,    0 },
	{"idma_enable",   IOV_IDMA_ENABLE,    0, 0,  IOVT_UINT32,    0 },
	{"ifrm_enable",   IOV_IFRM_ENABLE,    0, 0,  IOVT_UINT32,    0 },
	{"dar_enable",   IOV_DAR_ENABLE,    0, 0,  IOVT_UINT32,    0 },
	{"cap", IOV_DHD_CAPS,	0, 0, IOVT_BUFFER,	0},
#if defined(DEBUGGER) || defined(DHD_DSCOPE)
	{"gdb_server", IOV_GDB_SERVER,    0, 0,      IOVT_UINT32,    0 },
#endif /* DEBUGGER || DHD_DSCOPE */
#if defined(GDB_PROXY)
	{"gdb_proxy_probe", IOV_GDB_PROXY_PROBE, 0, 0, IOVT_BUFFER, 2 * sizeof(int32) },
	{"gdb_proxy_stop_count", IOV_GDB_PROXY_STOP_COUNT, 0, 0, IOVT_UINT32, 0 },
#endif /* GDB_PROXY */
	{"inb_dw_enable",   IOV_INB_DW_ENABLE,    0, 0,  IOVT_UINT32,    0 },
#if defined(PCIE_OOB) || defined(PCIE_INB_DW)
	{"deep_sleep", IOV_DEEP_SLEEP, 0, 0, IOVT_UINT32,    0},
#endif /* PCIE_OOB || PCIE_INB_DW */
	{"cto_threshold",	IOV_CTO_THRESHOLD,	0,	0, IOVT_UINT32,	0 },
#ifdef D2H_MINIDUMP
	{"minidump_override", IOV_MINIDUMP_OVERRIDE, 0, 0, IOVT_UINT32, 0 },
#endif /* D2H_MINIDUMP */
	{"hscbsize",	IOV_HSCBSIZE,	0,	0,	IOVT_UINT32,	0 },
#ifdef DHD_BUS_MEM_ACCESS
	{"hscbbytes",	IOV_HSCBBYTES,	0,	0,	IOVT_BUFFER,	2 * sizeof(int32) },
#endif

	{"extdtxs_in_txcpl", IOV_EXTDTXS_IN_TXCPL,	0,	0, IOVT_UINT32,	0 },
	{"hostrdy_after_init", IOV_HOSTRDY_AFTER_INIT,	0,	0, IOVT_UINT32,	0 },
	{"hp2p_mf_enable", IOV_HP2P_MF_ENABLE,	0,	0, IOVT_UINT32,	0 },

	{"dump_rxlat", IOV_DONGLE_RXLAT_INFO, 0, 0, IOVT_BUFFER,
	sizeof(rx_cpl_history_t) * MAX_RXCPL_HISTORY + 128 },
	{"dump_rxlat_hist", IOV_DONGLE_RXLAT_HISTO, 0, 0, IOVT_BUFFER,
	(MAX_RX_LAT_PRIO * MAX_RX_LAT_HIST_BIN * 2) + 128 },
	{"lpm_mode",	IOV_LPM_MODE,	0,      0,	IOVT_UINT32,	0 },

	{"dump_txcpl", IOV_DONGLE_TXCPL_INFO, 0, 0, IOVT_BUFFER,
	sizeof(tx_cpl_history_t) * MAX_TXCPL_HISTORY + 128 },

	{"security_status", IOV_DONGLE_SEC_INFO, 0, 0, IOVT_BUFFER,
	sizeof(tx_cpl_history_t) * MAX_TXCPL_HISTORY + 128 },

	{"sboot_disable", IOV_DONGLE_SBOOT_DIS, 0, 0, IOVT_BUFFER,
	sizeof(tx_cpl_history_t) * MAX_TXCPL_HISTORY + 128 },

	{"mdring_link", IOV_MDRING_LINK,	0,	0, IOVT_UINT32,	0 },
	{"mdring_unlink", IOV_MDRING_UNLINK,	0,	0, IOVT_UINT32,	0 },
	{"dump_mdring", IOV_MDRING_DUMP,	0,	0, IOVT_BUFFER,
	MAX_MDRING_ITEM_DUMP * D2HRING_MDCMPLT_ITEMSIZE },
	{"ptm_enable", IOV_PTM_ENABLE,	0,	0, IOVT_UINT32,	0 },

	{NULL, 0, 0, 0, 0, 0 }
};

#ifdef BCMQT_HW
#define MAX_READ_TIMEOUT	100 * 1000	/* 100 ms in dongle time */
#elif defined(NDIS)
#define MAX_READ_TIMEOUT	5 * 1000 * 1000
#else
#define MAX_READ_TIMEOUT	2 * 1000 * 1000
#endif

#define DHD_INFORING_BOUND	32
#define DHD_BTLOGRING_BOUND	32

#if defined(DEBUGGER) || defined(DHD_DSCOPE)
/** the GDB debugger layer will call back into this (bus) layer to read/write dongle memory */
static struct dhd_gdb_bus_ops_s  bus_ops = {
	.read_u16 = dhdpcie_bus_rtcm16,
	.read_u32 = dhdpcie_bus_rtcm32,
	.write_u32 = dhdpcie_bus_wtcm32,
};
#endif /* DEBUGGER || DHD_DSCOPE */

bool
dhd_bus_get_flr_force_fail(struct dhd_bus *bus)
{
	return bus->flr_force_fail;
}

/**
 * Register/Unregister functions are called by the main DHD entry point (eg module insertion) to
 * link with the bus driver, in order to look for or await the device.
 */
int
dhd_bus_register(void)
{
	DHD_TRACE(("%s: Enter\n", __FUNCTION__));

	return dhdpcie_bus_register();
}

void
dhd_bus_unregister(void)
{
	DHD_TRACE(("%s: Enter\n", __FUNCTION__));

	dhdpcie_bus_unregister();
	return;
}

/** returns a host virtual address */
uint32 *
dhdpcie_bus_reg_map(osl_t *osh, ulong addr, int size)
{
	return (uint32 *)REG_MAP(addr, size);
}

void
dhdpcie_bus_reg_unmap(osl_t *osh, volatile char *addr, int size)
{
	REG_UNMAP(addr);
	return;
}

/**
 * retrun H2D Doorbell registers address
 * use DAR registers instead of enum register for corerev >= 23 (4347B0)
 */
static INLINE uint
dhd_bus_db0_addr_get(struct dhd_bus *bus)
{
	uint addr = PCIH2D_MailBox;
	uint dar_addr = DAR_PCIH2D_DB0_0(bus->sih->buscorerev);

	/* Note: For DAR doorbell registers, the values is invalid. */
	return ((DAR_ACTIVE(bus->dhd)) ? dar_addr : addr);
}

static INLINE uint
dhd_bus_db0_addr_2_get(struct dhd_bus *bus)
{
	/* Note: For DAR doorbell registers, the values is invalid. */
	return ((DAR_ACTIVE(bus->dhd)) ? DAR_PCIH2D_DB2_0(bus->sih->buscorerev) : PCIH2D_MailBox_2);
}

static INLINE uint
dhd_bus_db1_addr_get(struct dhd_bus *bus)
{
	/* Note: For DB1, don't use DAR register */
	return PCIH2D_DB1;
}

static INLINE uint
dhd_bus_db1_addr_3_get(struct dhd_bus *bus)
{
	/* Note: For DB7, don't use DAR register */
	return PCIH2D_DB1_3;
}

static void
dhd_init_pwr_req_lock(dhd_bus_t *bus)
{
	if (!bus->pwr_req_lock) {
		bus->pwr_req_lock = osl_spin_lock_init(bus->osh);
	}
}

static void
dhd_deinit_pwr_req_lock(dhd_bus_t *bus)
{
	if (bus->pwr_req_lock) {
		osl_spin_lock_deinit(bus->osh, bus->pwr_req_lock);
		bus->pwr_req_lock = NULL;
	}
}

#ifdef PCIE_INB_DW
void
dhdpcie_set_dongle_deepsleep(dhd_bus_t *bus, bool val)
{
	ulong flags_ds;
	if (INBAND_DW_ENAB(bus)) {
		DHD_BUS_DONGLE_DS_LOCK(bus->dongle_ds_lock, flags_ds);
		bus->dongle_in_deepsleep = val;
		DHD_BUS_DONGLE_DS_UNLOCK(bus->dongle_ds_lock, flags_ds);
	}
}
void
dhd_init_dongle_ds_lock(dhd_bus_t *bus)
{
	if (!bus->dongle_ds_lock) {
		bus->dongle_ds_lock = osl_spin_lock_init(bus->osh);
	}
}
void
dhd_deinit_dongle_ds_lock(dhd_bus_t *bus)
{
	if (bus->dongle_ds_lock) {
		osl_spin_lock_deinit(bus->osh, bus->dongle_ds_lock);
		bus->dongle_ds_lock = NULL;
	}
}
#endif /* PCIE_INB_DW */

/*
 * WAR for SWWLAN-215055 - [4378B0] ARM fails to boot without DAR WL domain request
 */
static INLINE void
dhd_bus_pcie_pwr_req_wl_domain(struct dhd_bus *bus, uint offset, bool enable)
{
	if (enable) {
		si_corereg(bus->sih, bus->sih->buscoreidx, offset,
			SRPWR_DMN1_ARMBPSD_MASK << SRPWR_REQON_SHIFT,
			SRPWR_DMN1_ARMBPSD_MASK << SRPWR_REQON_SHIFT);
	} else {
		si_corereg(bus->sih, bus->sih->buscoreidx, offset,
			SRPWR_DMN1_ARMBPSD_MASK << SRPWR_REQON_SHIFT, 0);
	}
}

static INLINE void
_dhd_bus_pcie_pwr_req_clear_cmn(struct dhd_bus *bus)
{
	uint mask;

	/*
	 * If multiple de-asserts, decrement ref and return
	 * Clear power request when only one pending
	 * so initial request is not removed unexpectedly
	 */
	if (bus->pwr_req_ref > 1) {
		bus->pwr_req_ref--;
		return;
	}

	ASSERT(bus->pwr_req_ref == 1);

	if (MULTIBP_ENAB(bus->sih)) {
		/* Common BP controlled by HW so only need to toggle WL/ARM backplane */
		mask = SRPWR_DMN1_ARMBPSD_MASK;
	} else {
		mask = SRPWR_DMN0_PCIE_MASK | SRPWR_DMN1_ARMBPSD_MASK;
	}

	si_srpwr_request(bus->sih, mask, 0);
	bus->pwr_req_ref = 0;
}

static INLINE void
dhd_bus_pcie_pwr_req_clear(struct dhd_bus *bus)
{
	unsigned long flags = 0;

	DHD_BUS_PWR_REQ_LOCK(bus->pwr_req_lock, flags);
	_dhd_bus_pcie_pwr_req_clear_cmn(bus);
	DHD_BUS_PWR_REQ_UNLOCK(bus->pwr_req_lock, flags);
}

static INLINE void
dhd_bus_pcie_pwr_req_clear_nolock(struct dhd_bus *bus)
{
	_dhd_bus_pcie_pwr_req_clear_cmn(bus);
}

static INLINE void
_dhd_bus_pcie_pwr_req_cmn(struct dhd_bus *bus)
{
	uint mask, val;

	/* If multiple request entries, increment reference and return */
	if (bus->pwr_req_ref > 0) {
		bus->pwr_req_ref++;
		return;
	}

	ASSERT(bus->pwr_req_ref == 0);

	if (MULTIBP_ENAB(bus->sih)) {
		/* Common BP controlled by HW so only need to toggle WL/ARM backplane */
		mask = SRPWR_DMN1_ARMBPSD_MASK;
		val = SRPWR_DMN1_ARMBPSD_MASK;
	} else {
		mask = SRPWR_DMN0_PCIE_MASK | SRPWR_DMN1_ARMBPSD_MASK;
		val = SRPWR_DMN0_PCIE_MASK | SRPWR_DMN1_ARMBPSD_MASK;
	}

	si_srpwr_request(bus->sih, mask, val);

	bus->pwr_req_ref = 1;
}

static INLINE void
dhd_bus_pcie_pwr_req(struct dhd_bus *bus)
{
	unsigned long flags = 0;

	DHD_BUS_PWR_REQ_LOCK(bus->pwr_req_lock, flags);
	_dhd_bus_pcie_pwr_req_cmn(bus);
	DHD_BUS_PWR_REQ_UNLOCK(bus->pwr_req_lock, flags);
}

static INLINE void
_dhd_bus_pcie_pwr_req_pd0123_cmn(struct dhd_bus *bus)
{
	uint mask, val;

	mask = SRPWR_DMN_ALL_MASK(bus->sih);
	val = SRPWR_DMN_ALL_MASK(bus->sih);

	si_srpwr_request(bus->sih, mask, val);
}

void
dhd_bus_pcie_pwr_req_reload_war(struct dhd_bus *bus)
{
	unsigned long flags = 0;

	/*
	 * Few corerevs need the power domain to be active for FLR.
	 * Return if the pwr req is not applicable for the corerev
	 */
	if (!(PCIE_PWR_REQ_RELOAD_WAR_ENAB(bus->sih->buscorerev))) {
		return;
	}

	DHD_BUS_PWR_REQ_LOCK(bus->pwr_req_lock, flags);
	_dhd_bus_pcie_pwr_req_pd0123_cmn(bus);
	DHD_BUS_PWR_REQ_UNLOCK(bus->pwr_req_lock, flags);
}

static INLINE void
_dhd_bus_pcie_pwr_req_clear_pd0123_cmn(struct dhd_bus *bus)
{
	uint mask;

	mask = SRPWR_DMN_ALL_MASK(bus->sih);

	si_srpwr_request(bus->sih, mask, 0);
}

void
dhd_bus_pcie_pwr_req_clear_reload_war(struct dhd_bus *bus)
{
	unsigned long flags = 0;

	/* return if the pwr clear is not applicable for the corerev */
	if (!(PCIE_PWR_REQ_RELOAD_WAR_ENAB(bus->sih->buscorerev))) {
		return;
	}
	DHD_BUS_PWR_REQ_LOCK(bus->pwr_req_lock, flags);
	_dhd_bus_pcie_pwr_req_clear_pd0123_cmn(bus);
	DHD_BUS_PWR_REQ_UNLOCK(bus->pwr_req_lock, flags);
}

static INLINE void
dhd_bus_pcie_pwr_req_nolock(struct dhd_bus *bus)
{
	_dhd_bus_pcie_pwr_req_cmn(bus);
}

bool
dhdpcie_chip_support_msi(dhd_bus_t *bus)
{
	/* XXX For chips with buscorerev <= 14 intstatus
	 * is not getting cleared from these firmwares.
	 * Either host can read and clear intstatus for these
	 * or not enable MSI at all.
	 * Here option 2 of not enabling MSI is choosen.
	 * Also for hw4 chips, msi is not enabled.
	 */
	DHD_INFO(("%s: buscorerev=%d chipid=0x%x\n",
		__FUNCTION__, bus->sih->buscorerev, si_chipid(bus->sih)));
	if (bus->sih->buscorerev <= 14 ||
		si_chipid(bus->sih) == BCM4381_CHIP_ID ||
		si_chipid(bus->sih) == BCM4382_CHIP_ID ||
		si_chipid(bus->sih) == BCM4385_CHIP_ID ||
		si_chipid(bus->sih) == BCM4375_CHIP_ID ||
		si_chipid(bus->sih) == BCM4376_CHIP_ID ||
		si_chipid(bus->sih) == BCM4362_CHIP_ID ||
		si_chipid(bus->sih) == BCM43751_CHIP_ID ||
		si_chipid(bus->sih) == BCM43752_CHIP_ID ||
		si_chipid(bus->sih) == BCM43756_CHIP_ID ||
		si_chipid(bus->sih) == BCM4361_CHIP_ID ||
		si_chipid(bus->sih) == BCM4359_CHIP_ID) {
		return FALSE;
	} else {
		return TRUE;
	}
}

/**
 * Called once for each hardware (dongle) instance that this DHD manages.
 *
 * 'regs' is the host virtual address that maps to the start of the PCIe BAR0 window. The first 4096
 * bytes in this window are mapped to the backplane address in the PCIEBAR0Window register. The
 * precondition is that the PCIEBAR0Window register 'points' at the PCIe core.
 *
 * 'tcm' is the *host* virtual address at which tcm is mapped.
 */
int dhdpcie_bus_attach(osl_t *osh, dhd_bus_t **bus_ptr,
	volatile char *regs, volatile char *tcm, void *pci_dev, wifi_adapter_info_t *adapter)
{
	dhd_bus_t *bus = NULL;
	int ret = BCME_OK;

	DHD_TRACE(("%s: ENTER\n", __FUNCTION__));

	do {
		if (!(bus = MALLOCZ(osh, sizeof(dhd_bus_t)))) {
			DHD_ERROR(("%s: MALLOC of dhd_bus_t failed\n", __FUNCTION__));
			ret = BCME_NORESOURCE;
			break;
		}
		bus->bus = adapter->bus_type;
		bus->bus_num = adapter->bus_num;
		bus->slot_num = adapter->slot_num;

		bus->regs = regs;
		bus->tcm = tcm;
		bus->osh = osh;
#ifndef NDIS
		/* Save pci_dev into dhd_bus, as it may be needed in dhd_attach */
		bus->dev = (struct pci_dev *)pci_dev;
#endif

		dll_init(&bus->flowring_active_list);
#ifdef IDLE_TX_FLOW_MGMT
		bus->active_list_last_process_ts = OSL_SYSUPTIME();
#endif /* IDLE_TX_FLOW_MGMT */

#ifdef DEVICE_TX_STUCK_DETECT
		/* Enable the Device stuck detection feature by default */
		bus->dev_tx_stuck_monitor = TRUE;
		bus->device_tx_stuck_check = OSL_SYSUPTIME();
#endif /* DEVICE_TX_STUCK_DETECT */

		/* Attach pcie shared structure */
		if (!(bus->pcie_sh = MALLOCZ(osh, sizeof(pciedev_shared_t)))) {
			DHD_ERROR(("%s: MALLOC of bus->pcie_sh failed\n", __FUNCTION__));
			ret = BCME_NORESOURCE;
			break;
		}

		/* dhd_common_init(osh); */

		if (dhdpcie_dongle_attach(bus)) {
			DHD_ERROR(("%s: dhdpcie_probe_attach failed\n", __FUNCTION__));
			ret = BCME_NOTREADY;
			break;
		}

		/* software resources */
		if (!(bus->dhd = dhd_attach(osh, bus, PCMSGBUF_HDRLEN))) {
			DHD_ERROR(("%s: dhd_attach failed\n", __FUNCTION__));
			ret = BCME_NORESOURCE;
			break;
		}
#if defined(GET_OTP_MAC_ENABLE) || defined(GET_OTP_MODULE_NAME)
		dhd_conf_get_otp(bus->dhd, bus->sih);
#endif
		DHD_ERROR(("%s: making DHD_BUS_DOWN\n", __FUNCTION__));
		bus->dhd->busstate = DHD_BUS_DOWN;
		bus->dhd->hostrdy_after_init = TRUE;
		bus->db1_for_mb = TRUE;
		bus->dhd->hang_report = TRUE;
		bus->use_mailbox = FALSE;
		bus->use_d0_inform = FALSE;
		bus->intr_enabled = FALSE;
		bus->flr_force_fail = FALSE;
		/* update the dma indices if set through module parameter. */
		if (dma_ring_indices != 0) {
			dhdpcie_set_dma_ring_indices(bus->dhd, dma_ring_indices);
		}
		/* update h2d phase support if set through module parameter */
		bus->dhd->h2d_phase_supported = h2d_phase ? TRUE : FALSE;
		/* update force trap on bad phase if set through module parameter */
		bus->dhd->force_dongletrap_on_bad_h2d_phase =
			force_trap_bad_h2d_phase ? TRUE : FALSE;
#ifdef IDLE_TX_FLOW_MGMT
		bus->enable_idle_flowring_mgmt = FALSE;
#endif /* IDLE_TX_FLOW_MGMT */
		bus->irq_registered = FALSE;

#ifdef DHD_MSI_SUPPORT
		bus->d2h_intr_method = enable_msi && dhdpcie_chip_support_msi(bus) ?
			PCIE_MSI : PCIE_INTX;
		if (bus->dhd->conf->d2h_intr_method >= 0)
			bus->d2h_intr_method = bus->dhd->conf->d2h_intr_method;
#else
		bus->d2h_intr_method = PCIE_INTX;
#endif /* DHD_MSI_SUPPORT */

#ifdef NDIS
		/* For NDIS use D2H INTMASK based control */
		bus->d2h_intr_control = PCIE_D2H_INTMASK_CTRL;
#else
		/* For MSI, use host irq based control and for INTX use D2H INTMASK based control */
		if (bus->d2h_intr_method == PCIE_MSI) {
			bus->d2h_intr_control = PCIE_HOST_IRQ_CTRL;
		} else {
			bus->d2h_intr_control = PCIE_D2H_INTMASK_CTRL;
		}
#endif /* NDIS */

		DHD_TRACE(("%s: EXIT SUCCESS\n",
			__FUNCTION__));
		g_dhd_bus = bus;
		*bus_ptr = bus;
		return ret;
	} while (0);

	DHD_TRACE(("%s: EXIT FAILURE\n", __FUNCTION__));
	if (bus && bus->pcie_sh) {
		MFREE(osh, bus->pcie_sh, sizeof(pciedev_shared_t));
	}

	if (bus) {
		MFREE(osh, bus, sizeof(dhd_bus_t));
	}

	return ret;
}

bool
dhd_bus_skip_clm(dhd_pub_t *dhdp)
{
	switch (dhd_bus_chip_id(dhdp)) {
		case BCM4369_CHIP_ID:
			return TRUE;
		default:
			return FALSE;
	}
}

uint
dhd_bus_chip(struct dhd_bus *bus)
{
	ASSERT(bus->sih != NULL);
	return bus->sih->chip;
}

uint
dhd_bus_chiprev(struct dhd_bus *bus)
{
	ASSERT(bus);
	ASSERT(bus->sih != NULL);
	return bus->sih->chiprev;
}

void *
dhd_bus_pub(struct dhd_bus *bus)
{
	return bus->dhd;
}

void *
dhd_bus_sih(struct dhd_bus *bus)
{
	return (void *)bus->sih;
}

void *
dhd_bus_txq(struct dhd_bus *bus)
{
	return &bus->txq;
}

/** Get Chip ID version */
uint dhd_bus_chip_id(dhd_pub_t *dhdp)
{
	dhd_bus_t *bus = dhdp->bus;
	return  bus->sih->chip;
}

/** Get Chip Rev ID version */
uint dhd_bus_chiprev_id(dhd_pub_t *dhdp)
{
	dhd_bus_t *bus = dhdp->bus;
	return bus->sih->chiprev;
}

/** Get Chip Pkg ID version */
uint dhd_bus_chippkg_id(dhd_pub_t *dhdp)
{
	dhd_bus_t *bus = dhdp->bus;
	return bus->sih->chippkg;
}

int dhd_bus_get_ids(struct dhd_bus *bus, uint32 *bus_type, uint32 *bus_num, uint32 *slot_num)
{
	*bus_type = bus->bus;
	*bus_num = bus->bus_num;
	*slot_num = bus->slot_num;
	return 0;
}

/** Conduct Loopback test */
int
dhd_bus_dmaxfer_lpbk(dhd_pub_t *dhdp, uint32 type)
{
	dma_xfer_info_t dmaxfer_lpbk;
	int ret = BCME_OK;

#define PCIE_DMAXFER_LPBK_LENGTH	4096
	memset(&dmaxfer_lpbk, 0, sizeof(dma_xfer_info_t));
	dmaxfer_lpbk.version = DHD_DMAXFER_VERSION;
	dmaxfer_lpbk.length = (uint16)sizeof(dma_xfer_info_t);
	dmaxfer_lpbk.num_bytes = PCIE_DMAXFER_LPBK_LENGTH;
	dmaxfer_lpbk.type = type;
	dmaxfer_lpbk.should_wait = TRUE;

	ret = dhd_bus_iovar_op(dhdp, "pcie_dmaxfer", NULL, 0,
		(char *)&dmaxfer_lpbk, sizeof(dma_xfer_info_t), IOV_SET);
	if (ret < 0) {
		DHD_ERROR(("failed to start PCIe Loopback Test!!! "
			"Type:%d Reason:%d\n", type, ret));
		return ret;
	}

	if (dmaxfer_lpbk.status != DMA_XFER_SUCCESS) {
		DHD_ERROR(("failed to check PCIe Loopback Test!!! "
			"Type:%d Status:%d Error code:%d\n", type,
			dmaxfer_lpbk.status, dmaxfer_lpbk.error_code));
		ret = BCME_ERROR;
	} else {
		DHD_ERROR(("successful to check PCIe Loopback Test"
			" Type:%d\n", type));
	}
#undef PCIE_DMAXFER_LPBK_LENGTH

	return ret;
}

/* Check if there is DPC scheduling errors */
bool
dhd_bus_query_dpc_sched_errors(dhd_pub_t *dhdp)
{
	dhd_bus_t *bus = dhdp->bus;
	bool sched_err;

	if ((bus->dpc_entry_time < bus->isr_exit_time) && bus->dpc_sched) {
		/* Kernel doesn't schedule the DPC after processing PCIe IRQ */
		sched_err = TRUE;
	} else if (bus->dpc_entry_time < bus->resched_dpc_time) {
		/* Kernel doesn't schedule the DPC after DHD tries to reschedule
		 * the DPC due to pending work items to be processed.
		 */
		sched_err = TRUE;
	} else {
		sched_err = FALSE;
	}

	if (sched_err) {
		/* print out minimum timestamp info */
		DHD_ERROR(("isr_entry_time="SEC_USEC_FMT
			" prev_isr_entry_time="SEC_USEC_FMT
			" isr_exit_time="SEC_USEC_FMT
			" dpc_entry_time="SEC_USEC_FMT
			"\ndpc_exit_time="SEC_USEC_FMT
			" isr_sched_dpc_time="SEC_USEC_FMT
			" resched_dpc_time="SEC_USEC_FMT
			" dpc_sched=%u\n",
			GET_SEC_USEC(bus->isr_entry_time),
			GET_SEC_USEC(bus->prev_isr_entry_time),
			GET_SEC_USEC(bus->isr_exit_time),
			GET_SEC_USEC(bus->dpc_entry_time),
			GET_SEC_USEC(bus->dpc_exit_time),
			GET_SEC_USEC(bus->isr_sched_dpc_time),
			GET_SEC_USEC(bus->resched_dpc_time),
			bus->dpc_sched));
	}

	return sched_err;
}

/** Read and clear intstatus. This should be called with interrupts disabled or inside isr */
uint32
dhdpcie_bus_intstatus(dhd_bus_t *bus)
{
	uint32 intstatus = 0;
	uint32 intmask = 0;

	if (__DHD_CHK_BUS_LPS_D3_ACKED(bus)) {
		DHD_ERROR(("%s: trying to clear intstatus after D3 Ack\n", __FUNCTION__));
		return intstatus;
	}
	/* XXX: check for PCIE Gen2 also */
	if ((bus->sih->buscorerev == 6) || (bus->sih->buscorerev == 4) ||
		(bus->sih->buscorerev == 2)) {
		intstatus = dhdpcie_bus_cfg_read_dword(bus, PCIIntstatus, 4);
		dhdpcie_bus_cfg_write_dword(bus, PCIIntstatus, 4, intstatus);
		intstatus &= I_MB;
	} else {
		/* this is a PCIE core register..not a config register... */
		intstatus = si_corereg(bus->sih, bus->sih->buscoreidx, bus->pcie_mailbox_int, 0, 0);

#ifdef DHD_MMIO_TRACE
		dhd_bus_mmio_trace(bus, bus->pcie_mailbox_int, intstatus, FALSE);
#endif /* defined(DHD_MMIO_TRACE) */

		/* this is a PCIE core register..not a config register... */
		intmask = si_corereg(bus->sih, bus->sih->buscoreidx, bus->pcie_mailbox_mask, 0, 0);
		/* Is device removed. intstatus & intmask read 0xffffffff */
		if (intstatus == (uint32)-1 || intmask == (uint32)-1) {
			DHD_ERROR(("%s: Device is removed or Link is down.\n", __FUNCTION__));
			DHD_ERROR(("%s: INTSTAT : 0x%x INTMASK : 0x%x.\n",
			    __FUNCTION__, intstatus, intmask));
			bus->is_linkdown = TRUE;
			dhd_pcie_debug_info_dump(bus->dhd);
			return intstatus;
		}

#ifndef DHD_READ_INTSTATUS_IN_DPC
		intstatus &= intmask;
#endif /* DHD_READ_INTSTATUS_IN_DPC */

#ifdef DHD_MMIO_TRACE
		dhd_bus_mmio_trace(bus, bus->pcie_mailbox_mask, intmask, FALSE);
#endif /* defined(DHD_MMIO_TRACE) */

		/* XXX: define the mask in a .h file  */
		/*
		 * The fourth argument to si_corereg is the "mask" fields of the register to update
		 * and the fifth field is the "value" to update. Now if we are interested in only
		 * few fields of the "mask" bit map, we should not be writing back what we read
		 * By doing so, we might clear/ack interrupts that are not handled yet.
		 */
#ifdef DHD_MMIO_TRACE
		dhd_bus_mmio_trace(bus, bus->pcie_mailbox_int, intstatus, TRUE);
#endif /* defined(DHD_MMIO_TRACE) */
		si_corereg(bus->sih, bus->sih->buscoreidx, bus->pcie_mailbox_int, bus->def_intmask,
			intstatus);

		intstatus &= bus->def_intmask;
	}

	return intstatus;
}

void
dhdpcie_cto_recovery_handler(dhd_pub_t *dhd)
{
	dhd_bus_t *bus = dhd->bus;
	int ret;

	if (dhd->up == FALSE) {
		DHD_ERROR(("%s : dhd is not up\n", __FUNCTION__));
		return;
	}

	if (bus->is_linkdown) {
		DHD_ERROR(("%s: link is down\n", __FUNCTION__));
		return;
	}

	if (bus->sih == NULL) {
		DHD_ERROR(("%s: The address of sih is invalid\n", __FUNCTION__));
		return;
	}

	/* Disable PCIe Runtime PM to avoid D3_ACK timeout.
	 */
	DHD_DISABLE_RUNTIME_PM(dhd);

	/* Sleep for 1 seconds so that any AXI timeout
	 * if running on ALP clock also will be captured
	 */
	OSL_SLEEP(1000);

	/* reset backplane and cto,
	 * then access through pcie is recovered.
	 */
	ret = dhdpcie_cto_error_recovery(bus);
	if (!ret) {
		/* Waiting for backplane reset */
		OSL_SLEEP(10);
		/* Dump debug Info */
		dhd_prot_debug_info_print(bus->dhd);
		/* Dump console buffer */
		dhd_bus_dump_console_buffer(bus);
#if defined(DHD_FW_COREDUMP)
		/* save core dump or write to a file */
		if (!bus->is_linkdown && bus->dhd->memdump_enabled) {
#ifdef DHD_SSSR_DUMP
			DHD_ERROR(("%s : Set collect_sssr as TRUE\n", __FUNCTION__));
			bus->dhd->collect_sssr = TRUE;
#endif /* DHD_SSSR_DUMP */
			bus->dhd->memdump_type = DUMP_TYPE_CTO_RECOVERY;
			dhdpcie_mem_dump(bus);
		}
#endif /* DHD_FW_COREDUMP */
	}
#ifdef OEM_ANDROID
	bus->is_linkdown = TRUE;
	bus->dhd->hang_reason = HANG_REASON_PCIE_CTO_DETECT;
	/* Send HANG event */
	dhd_os_send_hang_message(bus->dhd);
#endif /* OEM_ANDROID */
}

void
dhd_bus_dump_imp_cfg_registers(struct dhd_bus *bus)
{
	uint32 status_cmd = dhd_pcie_config_read(bus, PCIECFGREG_STATUS_CMD, sizeof(uint32));
	uint32 pmcsr = dhd_pcie_config_read(bus, PCIE_CFG_PMCSR, sizeof(uint32));
	uint32 base_addr0 = dhd_pcie_config_read(bus, PCIECFGREG_BASEADDR0, sizeof(uint32));
	uint32 base_addr1 = dhd_pcie_config_read(bus, PCIECFGREG_BASEADDR1, sizeof(uint32));
	uint32 linkctl = dhd_pcie_config_read(bus, PCIECFGREG_LINK_STATUS_CTRL, sizeof(uint32));
	uint32 l1ssctrl =
		dhd_pcie_config_read(bus, PCIECFGREG_PML1_SUB_CTRL1, sizeof(uint32));
	uint32 devctl = dhd_pcie_config_read(bus, PCIECFGREG_DEV_STATUS_CTRL, sizeof(uint32));
	uint32 devctl2 = dhd_pcie_config_read(bus, PCIECFGGEN_DEV_STATUS_CTRL2, sizeof(uint32));

	DHD_ERROR(("status_cmd(0x%x)=0x%x, pmcsr(0x%x)=0x%x "
		"base_addr0(0x%x)=0x%x base_addr1(0x%x)=0x%x "
		"linkctl(0x%x)=0x%x l1ssctrl(0x%x)=0x%x "
		"devctl(0x%x)=0x%x devctl2(0x%x)=0x%x \n",
		PCIECFGREG_STATUS_CMD, status_cmd,
		PCIE_CFG_PMCSR, pmcsr,
		PCIECFGREG_BASEADDR0, base_addr0,
		PCIECFGREG_BASEADDR1, base_addr1,
		PCIECFGREG_LINK_STATUS_CTRL, linkctl,
		PCIECFGREG_PML1_SUB_CTRL1, l1ssctrl,
		PCIECFGREG_DEV_STATUS_CTRL, devctl,
		PCIECFGGEN_DEV_STATUS_CTRL2, devctl2));
}

/**
 * Name:  dhdpcie_bus_isr
 * Parameters:
 * 1: IN int irq   -- interrupt vector
 * 2: IN void *arg      -- handle to private data structure
 * Return value:
 * Status (TRUE or FALSE)
 *
 * Description:
 * Interrupt Service routine checks for the status register,
 * disable interrupt and queue DPC if mail box interrupts are raised.
 */
int32
dhdpcie_bus_isr(dhd_bus_t *bus)
{
	uint32 intstatus = 0;

	do {
		DHD_INTR(("%s: Enter\n", __FUNCTION__));
		/* verify argument */
		if (!bus) {
			DHD_LOG_MEM(("%s : bus is null pointer, exit \n", __FUNCTION__));
			break;
		}

		if (bus->dhd->dongle_reset) {
			DHD_LOG_MEM(("%s : dongle is reset\n", __FUNCTION__));
			break;
		}

		if (bus->dhd->busstate == DHD_BUS_DOWN) {
			DHD_LOG_MEM(("%s : bus is down \n", __FUNCTION__));
			break;
		}

		/* avoid processing of interrupts until msgbuf prot is inited */
		if (!bus->intr_enabled) {
			DHD_INFO(("%s, not ready to receive interrupts\n", __FUNCTION__));
			break;
		}

		/* Do not process any ISR after receiving D3_ACK */
		if (__DHD_CHK_BUS_LPS_D3_ACKED(bus)) {
			DHD_LOG_MEM(("%s: D3 Ack Received, skip\n", __FUNCTION__));
			break;
		}

		if (PCIECTO_ENAB(bus)) {
			/* read pci_intstatus */
			intstatus = dhdpcie_bus_cfg_read_dword(bus, PCI_INT_STATUS, 4);

			if (intstatus == (uint32)-1 ||
				bus->dhd->dhd_induce_error == DHD_INDUCE_PCIE_LINK_DOWN_IN_ISR) {
				DHD_ERROR(("%s: Invalid cfg intstatus(0x%x):0x%x, pcie link down,"
					"induce %u\n", __FUNCTION__, PCI_INT_STATUS, intstatus,
					bus->dhd->dhd_induce_error));
				bus->is_linkdown = 1;
				dhdpcie_disable_irq_nosync(bus);
				dhd_pcie_debug_info_dump(bus->dhd);
#ifdef OEM_ANDROID
				bus->dhd->hang_reason = HANG_REASON_PCIE_LINK_DOWN_EP_DETECT;
				dhd_os_send_hang_message(bus->dhd);
#endif /* OEM_ANDROID */
				break;
			}

			if (intstatus & PCI_CTO_INT_MASK) {
				DHD_ERROR(("%s: ##### CTO RECOVERY REPORTED BY DONGLE "
					"intstat=0x%x enab=%d\n", __FUNCTION__,
					intstatus, bus->cto_enable));
				bus->cto_triggered = 1;
				bus->dhd->do_chip_bighammer = TRUE;
				dhd_bus_dump_imp_cfg_registers(bus);
				/*
				 * DAR still accessible
				 */
				dhd_bus_dump_dar_registers(bus);

				/* Disable further PCIe interrupts */
#ifndef NDIS
				dhdpcie_disable_irq_nosync(bus); /* Disable interrupt!! */
#endif
				/* Stop Tx flow */
				dhd_bus_stop_queue(bus);

				/* Schedule CTO recovery */
				dhd_schedule_cto_recovery(bus->dhd);

				return TRUE;
			}
		}

#ifndef DHD_READ_INTSTATUS_IN_DPC
		if (bus->d2h_intr_method == PCIE_MSI &&
				!dhd_conf_legacy_msi_chip(bus->dhd)) {
			/* For MSI, as intstatus is cleared by firmware, no need to read */
			goto skip_intstatus_read;
		}

		intstatus = dhdpcie_bus_intstatus(bus);

		/* Check if the interrupt is ours or not */
		if (intstatus == 0) {
			bus->non_ours_irq_count++;
			bus->last_non_ours_irq_time = OSL_LOCALTIME_NS();
			break;
		}

		/* save the intstatus */
		/* read interrupt status register!! Status bits will be cleared in DPC !! */
		bus->intstatus = intstatus;

		/* return error for 0xFFFFFFFF */
		if (intstatus == (uint32)-1) {
			DHD_LOG_MEM(("%s : wrong interrupt status val : 0x%x\n",
				__FUNCTION__, intstatus));
			bus->is_linkdown = 1;
			dhdpcie_disable_irq_nosync(bus);
			break;
		}

skip_intstatus_read:
		/*  Overall operation:
		 *    - Mask further interrupts
		 *    - Read/ack intstatus
		 *    - Take action based on bits and state
		 *    - Reenable interrupts (as per state)
		 */

		/* Count the interrupt call */
		bus->intrcount++;
#endif /* DHD_READ_INTSTATUS_IN_DPC */

		bus->ipend = TRUE;

		/* Due to irq mismatch WARNING in linux, currently keeping it disabled and
		 * using dongle intmask to control INTR enable/disable
		 */
		if (bus->d2h_intr_control == PCIE_HOST_IRQ_CTRL) {
			if (!dhdpcie_irq_disabled(bus)) {
				bus->host_irq_disable_count++;
				dhdpcie_disable_irq_nosync(bus); /* Disable interrupt!! */
			}
		} else {
			dhdpcie_bus_intr_disable(bus); /* Disable interrupt using IntMask!! */
			bus->dngl_intmask_disable_count++;
		}

		bus->intdis = TRUE;
#ifdef DHD_FLOW_RING_STATUS_TRACE
		if (bus->dhd->dma_h2d_ring_upd_support && bus->dhd->dma_d2h_ring_upd_support &&
			(bus->dhd->ring_attached == TRUE)) {
			dhd_bus_flow_ring_status_isr_trace(bus->dhd);
		}
#endif /* DHD_FLOW_RING_STATUS_TRACE */
#if defined(PCIE_ISR_THREAD)
		DHD_TRACE(("Calling dhd_bus_dpc() from %s\n", __FUNCTION__));
		DHD_OS_WAKE_LOCK(bus->dhd);
		while (dhd_bus_dpc(bus));
		DHD_OS_WAKE_UNLOCK(bus->dhd);
#else // defined(PCIE_ISR_THREAD)
		bus->dpc_sched = TRUE;
		bus->isr_sched_dpc_time = OSL_LOCALTIME_NS();
#ifndef NDIS
		dhd_sched_dpc(bus->dhd);     /* queue DPC now!! */
#endif /* !NDIS */
#endif /* defined(PCIE_ISR_THREAD) */

		DHD_INTR(("%s: Exit Success DPC Queued\n", __FUNCTION__));
		return TRUE;

	} while (0);

	DHD_INTR(("%s: Exit Failure\n", __FUNCTION__));
	return FALSE;
}

int
dhdpcie_set_pwr_state(dhd_bus_t *bus, uint state)
{
	uint32 cur_state = 0;
	uint32 pm_csr = 0;
	osl_t *osh = bus->osh;

	pm_csr = OSL_PCI_READ_CONFIG(osh, PCIECFGREG_PM_CSR, sizeof(uint32));
	cur_state = pm_csr & PCIECFGREG_PM_CSR_STATE_MASK;

	if (cur_state == state) {
		DHD_ERROR(("%s: Already in state %u \n", __FUNCTION__, cur_state));
		return BCME_OK;
	}

	if (state > PCIECFGREG_PM_CSR_STATE_D3_HOT)
		return BCME_ERROR;

	/* Validate the state transition
	* if already in a lower power state, return error
	*/
	if (state != PCIECFGREG_PM_CSR_STATE_D0 &&
			cur_state <= PCIECFGREG_PM_CSR_STATE_D3_COLD &&
			cur_state > state) {
		DHD_ERROR(("%s: Invalid power state transition !\n", __FUNCTION__));
		return BCME_ERROR;
	}

	pm_csr &= ~PCIECFGREG_PM_CSR_STATE_MASK;
	pm_csr |= state;

	OSL_PCI_WRITE_CONFIG(osh, PCIECFGREG_PM_CSR, sizeof(uint32), pm_csr);

	/* need to wait for the specified mandatory pcie power transition delay time */
	if (state == PCIECFGREG_PM_CSR_STATE_D3_HOT ||
			cur_state == PCIECFGREG_PM_CSR_STATE_D3_HOT)
			OSL_DELAY(DHDPCIE_PM_D3_DELAY);
	else if (state == PCIECFGREG_PM_CSR_STATE_D2 ||
			cur_state == PCIECFGREG_PM_CSR_STATE_D2)
			OSL_DELAY(DHDPCIE_PM_D2_DELAY);

	/* read back the power state and verify */
	pm_csr = OSL_PCI_READ_CONFIG(osh, PCIECFGREG_PM_CSR, sizeof(uint32));
	cur_state = pm_csr & PCIECFGREG_PM_CSR_STATE_MASK;
	if (cur_state != state) {
		DHD_ERROR(("%s: power transition failed ! Current state is %u \n",
				__FUNCTION__, cur_state));
		return BCME_ERROR;
	} else {
		DHD_ERROR(("%s: power transition to %u success \n",
				__FUNCTION__, cur_state));
	}

	return BCME_OK;
}

int
dhdpcie_config_check(dhd_bus_t *bus)
{
	uint32 i, val;
	int ret = BCME_ERROR;

	for (i = 0; i < DHDPCIE_CONFIG_CHECK_RETRY_COUNT; i++) {
		val = OSL_PCI_READ_CONFIG(bus->osh, PCI_CFG_VID, sizeof(uint32));
		if (((val & 0xFFFF) == VENDOR_BROADCOM) || ((val & 0xFFFF) == VENDOR_SYNAPTICS)) {
			ret = BCME_OK;
			break;
		}
		OSL_DELAY(DHDPCIE_CONFIG_CHECK_DELAY_MS * 1000);
	}

	return ret;
}

int
dhdpcie_config_restore(dhd_bus_t *bus, bool restore_pmcsr)
{
	uint32 i;
	osl_t *osh = bus->osh;

	if (BCME_OK != dhdpcie_config_check(bus)) {
		return BCME_ERROR;
	}

	for (i = PCI_CFG_REV >> 2; i < DHDPCIE_CONFIG_HDR_SIZE; i++) {
		OSL_PCI_WRITE_CONFIG(osh, i << 2, sizeof(uint32), bus->saved_config.header[i]);
	}
	OSL_PCI_WRITE_CONFIG(osh, PCI_CFG_CMD, sizeof(uint32), bus->saved_config.header[1]);

	if (restore_pmcsr)
		OSL_PCI_WRITE_CONFIG(osh, PCIECFGREG_PM_CSR,
			sizeof(uint32), bus->saved_config.pmcsr);

	OSL_PCI_WRITE_CONFIG(osh, PCIECFGREG_MSI_CAP, sizeof(uint32), bus->saved_config.msi_cap);
	OSL_PCI_WRITE_CONFIG(osh, PCIECFGREG_MSI_ADDR_L, sizeof(uint32),
			bus->saved_config.msi_addr0);
	OSL_PCI_WRITE_CONFIG(osh, PCIECFGREG_MSI_ADDR_H,
			sizeof(uint32), bus->saved_config.msi_addr1);
	OSL_PCI_WRITE_CONFIG(osh, PCIECFGREG_MSI_DATA,
			sizeof(uint32), bus->saved_config.msi_data);

	OSL_PCI_WRITE_CONFIG(osh, PCIECFGREG_DEV_STATUS_CTRL,
			sizeof(uint32), bus->saved_config.exp_dev_ctrl_stat);
	OSL_PCI_WRITE_CONFIG(osh, PCIECFGGEN_DEV_STATUS_CTRL2,
			sizeof(uint32), bus->saved_config.exp_dev_ctrl_stat2);
	OSL_PCI_WRITE_CONFIG(osh, PCIECFGREG_LINK_STATUS_CTRL,
			sizeof(uint32), bus->saved_config.exp_link_ctrl_stat);
	OSL_PCI_WRITE_CONFIG(osh, PCIECFGREG_LINK_STATUS_CTRL2,
			sizeof(uint32), bus->saved_config.exp_link_ctrl_stat2);

	OSL_PCI_WRITE_CONFIG(osh, PCIECFGREG_PML1_SUB_CTRL1,
			sizeof(uint32), bus->saved_config.l1pm0);
	OSL_PCI_WRITE_CONFIG(osh, PCIECFGREG_PML1_SUB_CTRL2,
			sizeof(uint32), bus->saved_config.l1pm1);

	OSL_PCI_WRITE_CONFIG(bus->osh, PCI_BAR0_WIN, sizeof(uint32),
			bus->saved_config.bar0_win);
	dhdpcie_setbar1win(bus, bus->saved_config.bar1_win);

	return BCME_OK;
}

int
dhdpcie_config_save(dhd_bus_t *bus)
{
	uint32 i;
	osl_t *osh = bus->osh;

	if (BCME_OK != dhdpcie_config_check(bus)) {
		return BCME_ERROR;
	}

	for (i = 0; i < DHDPCIE_CONFIG_HDR_SIZE; i++) {
		bus->saved_config.header[i] = OSL_PCI_READ_CONFIG(osh, i << 2, sizeof(uint32));
	}

	bus->saved_config.pmcsr = OSL_PCI_READ_CONFIG(osh, PCIECFGREG_PM_CSR, sizeof(uint32));

	bus->saved_config.msi_cap = OSL_PCI_READ_CONFIG(osh, PCIECFGREG_MSI_CAP,
			sizeof(uint32));
	bus->saved_config.msi_addr0 = OSL_PCI_READ_CONFIG(osh, PCIECFGREG_MSI_ADDR_L,
			sizeof(uint32));
	bus->saved_config.msi_addr1 = OSL_PCI_READ_CONFIG(osh, PCIECFGREG_MSI_ADDR_H,
			sizeof(uint32));
	bus->saved_config.msi_data = OSL_PCI_READ_CONFIG(osh, PCIECFGREG_MSI_DATA,
			sizeof(uint32));

	bus->saved_config.exp_dev_ctrl_stat = OSL_PCI_READ_CONFIG(osh,
			PCIECFGREG_DEV_STATUS_CTRL, sizeof(uint32));
	bus->saved_config.exp_dev_ctrl_stat2 = OSL_PCI_READ_CONFIG(osh,
			PCIECFGGEN_DEV_STATUS_CTRL2, sizeof(uint32));
	bus->saved_config.exp_link_ctrl_stat = OSL_PCI_READ_CONFIG(osh,
			PCIECFGREG_LINK_STATUS_CTRL, sizeof(uint32));
	bus->saved_config.exp_link_ctrl_stat2 = OSL_PCI_READ_CONFIG(osh,
			PCIECFGREG_LINK_STATUS_CTRL2, sizeof(uint32));

	bus->saved_config.l1pm0 = OSL_PCI_READ_CONFIG(osh, PCIECFGREG_PML1_SUB_CTRL1,
			sizeof(uint32));
	bus->saved_config.l1pm1 = OSL_PCI_READ_CONFIG(osh, PCIECFGREG_PML1_SUB_CTRL2,
			sizeof(uint32));

	bus->saved_config.bar0_win = OSL_PCI_READ_CONFIG(osh, PCI_BAR0_WIN,
			sizeof(uint32));
	bus->saved_config.bar1_win = OSL_PCI_READ_CONFIG(osh, PCI_BAR1_WIN,
			sizeof(uint32));

	return BCME_OK;
}

#ifdef CONFIG_ARCH_EXYNOS
dhd_pub_t *link_recovery = NULL;
#endif /* CONFIG_ARCH_EXYNOS */

static void
dhdpcie_bus_intr_init(dhd_bus_t *bus)
{
	uint buscorerev = bus->sih->buscorerev;
	bus->pcie_mailbox_int = PCIMailBoxInt(buscorerev);
	bus->pcie_mailbox_mask = PCIMailBoxMask(buscorerev);
	bus->d2h_mb_mask = PCIE_MB_D2H_MB_MASK(buscorerev);
	bus->def_intmask = PCIE_MB_D2H_MB_MASK(buscorerev);
	if (buscorerev < 64) {
		bus->def_intmask |= PCIE_MB_TOPCIE_FN0_0 | PCIE_MB_TOPCIE_FN0_1;
	}
}

static void
dhdpcie_cc_watchdog_reset(dhd_bus_t *bus)
{
	uint32 wd_en = (bus->sih->buscorerev >= 66) ? WD_SSRESET_PCIE_F0_EN :
		(WD_SSRESET_PCIE_F0_EN | WD_SSRESET_PCIE_ALL_FN_EN);
	pcie_watchdog_reset(bus->osh, bus->sih, WD_ENABLE_MASK, wd_en);
}

void
dhdpcie_dongle_reset(dhd_bus_t *bus)
{

	/* if the pcie link is down, watchdog reset
	 * should not be done, as it may hang
	 */
	if (bus->is_linkdown) {
		return;
	}

	/* Currently BP reset using CFG reg is done only for android platforms */
#ifdef DHD_USE_BP_RESET_SPROM
	/* This is for architectures that does NOT control subsystem reset */
	(void)dhd_bus_cfg_sprom_ctrl_bp_reset(bus);
	return;
#elif defined(DHD_USE_BP_RESET_SS_CTRL)
	/* This is for architectures that supports Subsystem Control */
	(void)dhd_bus_cfg_ss_ctrl_bp_reset(bus);
	return;
#else

#ifdef BCMQT_HW
	/* flr takes a long time on qt and is only required when testing with BT
	 * included database. Fall back to watchdog reset by default and only perform
	 * flr if enabled through module parameter
	 */
	if (qt_flr_reset && (dhd_bus_perform_flr(bus, FALSE) != BCME_UNSUPPORTED)) {
		return;
	}
#else
	/* dhd_bus_perform_flr will return BCME_UNSUPPORTED if chip is not FLR capable */
	if (dhd_bus_perform_flr(bus, FALSE) == BCME_UNSUPPORTED)
#endif
	{
		/* Legacy chipcommon watchdog reset */
		dhdpcie_cc_watchdog_reset(bus);
	}
	return;
#endif /* DHD_USE_BP_RESET */
}

#ifdef BCMQT_HW
/* Xtal to ALP clock ratio can change from chip to chip and this functions returns the
 * Xtal to ALP clock ratio by extracting the value for the current chip.
 */
static uint32
dhdpcie_get_xtal_to_alp_ratio(dhd_bus_t *bus)
{
	uint16 chipid = si_chipid(bus->sih);
	uint32 xtal_to_alp_ratio;

	switch (chipid) {
		case BCM4397_CHIP_ID:
			xtal_to_alp_ratio = 2;
			break;
		default:
			xtal_to_alp_ratio = 1;
			break;
	}

	return xtal_to_alp_ratio;
}

/* Calculate dongle/host clock ratio for QT so the waiting period in host driver can be scaled
 * properly. The dongle uses ALP clock by default which can't be read directly. But ILP and
 * ALP clocks are scaled disproportionally in QT. So DHD must know the preset crystal frequency
 * for ALP clock in order to calculate the scale ratio. The logic below takes 3 sources of xtal
 * frequency as following priority:
 * 1 module parameter
 * 2 nvram "xtalfreq" line (not available for the first dongle reset)
 * 3 Hard coded 37.4MHz
 * If the QT simulation of a chip uses a different frequency xtal other than 37.4MHz, it's
 * strongly recommended to expend the hard coded value to per chip basis or override with module
 * parameter.
 */
#define XTAL_FREQ_37M4		37400000u
void dhdpcie_htclkratio_cal(dhd_bus_t *bus)
{
	uint cur_coreidx, pmu_idx, ilp_on_fast_lpo = 0;
	uint32 ilp_start, ilp_tick = 0, alp_to_ilp_ratio = 0;
	int xtalfreq = 0;
	uint32 alp_freq = 0;
	uint32 xtal_to_alp_ratio = dhdpcie_get_xtal_to_alp_ratio(bus);

	/* If a larger than 1 htclkratio is set through module parameter, use it directly */
	if (htclkratio > 1) {
		goto exit;
	}

	/* Store current core id */
	cur_coreidx = si_coreidx(bus->sih);
	if (!si_setcore(bus->sih, PMU_CORE_ID, 0)) {
		htclkratio = 2000;
		goto exit;
	}

	pmu_idx = si_coreidx(bus->sih);

	/* Count IPL ticks in 1 second of host domain clock */
	ilp_start = si_corereg(bus->sih, pmu_idx, offsetof(pmuregs_t, pmutimer), 0, 0);
	osl_sleep(1000);
	ilp_tick = si_corereg(bus->sih, pmu_idx, offsetof(pmuregs_t, pmutimer), 0, 0);
	/* -1 to compensate the incomplete cycle at the beginning */
	ilp_tick -= ilp_start - 1;

	ilp_on_fast_lpo = si_corereg(bus->sih, pmu_idx, offsetof(pmuregs_t, pmustatus), 0, 0) &
		PST_ILPFASTLPO;

	if (PMUREV(bus->sih->pmurev) <= 45) {
		/* If ILP is running on Fast LPO is enabled, XtalFreqRatio uses 1 Mhz ILP
		 * instead of 32 KHz. But PMUTimer runs at 32 KHz only irrespective of Fast
		 * LPO is enabled or not. Thereby scale up ilp_tick when Fast LPO is enabled.
		 * In case of Zebu/QT, Fast LPO is exactly 32 times of ILP clock.
		 * [Reference: CRWLPMU-284]
		 */
		if (ilp_on_fast_lpo) {
			ilp_tick *= 32;
		}
	}
	/* Get xtal vs ILP ratio from XtalFreqRatio(0x66c) */
	alp_to_ilp_ratio = si_corereg(bus->sih, pmu_idx, offsetof(pmuregs_t, pmu_xtalfreq), 0, 0);
	alp_to_ilp_ratio = (alp_to_ilp_ratio & PMU_XTALFREQ_REG_ILPCTR_MASK) / 4;

	/* Go back to original core */
	si_setcoreidx(bus->sih, cur_coreidx);

	/* Use module parameter if one is provided. Otherwise use default 37.4MHz */
	if (dngl_xtalfreq) {
		xtalfreq = dngl_xtalfreq;
	} else {
		xtalfreq = XTAL_FREQ_37M4;
	}

	alp_freq = xtalfreq / xtal_to_alp_ratio;

	/* htclkratio = Expected ALP Freq / Real ALP Freq
	 *            = Expected ALP Freq / (Real ILP Tick * Real alp_to_ilp_ratio)
	 */
	htclkratio = alp_freq / (ilp_tick * alp_to_ilp_ratio);
	bus->xtalfreq = xtalfreq;
	bus->ilp_tick = ilp_tick;
	bus->alp_to_ilp_ratio = alp_to_ilp_ratio;
	bus->xtal_to_alp_ratio = xtal_to_alp_ratio;

exit:
	DHD_ERROR(("Dongle/Host clock ratio %u with %dHz xtal frequency, ilp_tick: %d, "
			"alp_to_ilp_ratio: %d, ilp_on_fast_lpo: %d\n",
			htclkratio, xtalfreq, ilp_tick, alp_to_ilp_ratio, !!ilp_on_fast_lpo));
}

/* Re-calculate htclkratio if nvram provides a different xtalfreq */
void dhdpcie_htclkratio_recal(dhd_bus_t *bus, char *nvram, uint nvram_sz)
{
	char *freq_c = NULL;
	uint len, p;
	int xtalfreq = 0;
	uint cur_coreidx, pmu_idx;
	uint32 alp_to_ilp_ratio = 0;
	uint32 alp_freq = 0;

	/* Do not re-calculate if xtalfreq is overridden by module parameter */
	if (dngl_xtalfreq)
		return;

	/* look for "xtalfreq=xxxx" line in nvram */
	len = strlen("xtalfreq");
	for (p = 0; p < (nvram_sz - len) && nvram[p]; ) {
		if ((bcmp(&nvram[p], "xtalfreq", len) == 0) && (nvram[p + len] == '=')) {
			freq_c = &nvram[p + len + 1u];
			break;
		}
		/* jump to next line */
		while (nvram[p++]);
	}

	if (freq_c) {
		xtalfreq = bcm_strtoul(freq_c, NULL, 0);
		if (xtalfreq > (INT_MAX / 1000u)) {
			DHD_ERROR(("xtalfreq %d in nvram is too big\n", xtalfreq));
			xtalfreq = 0;
		}
		xtalfreq *= 1000;
	}

	/* Skip recalculation if:
	 *   nvram doesn't provide "xtalfreq", or
	 *   first calculation was not performed because module parameter override, or
	 *   xtalfreq in nvram is the same as the one used in first calculation
	 */
	if (xtalfreq == 0 || bus->xtalfreq == 0 || xtalfreq == bus->xtalfreq) {
		return;
	}

	/* Print out a error message here. Even if the ratio is corrected with nvram setting, dongle
	 * reset has been performed before DHD has access to NVRAM. Insufficient waiting period
	 * for reset might cause unexpected behavior.
	 */
	DHD_ERROR(("Re-calculating htclkratio because nvram xtalfreq %dHz is different from %dHz\n",
		xtalfreq, bus->xtalfreq));

	alp_freq = xtalfreq / bus->xtal_to_alp_ratio;
	htclkratio = alp_freq / (bus->ilp_tick * bus->alp_to_ilp_ratio);
	bus->xtalfreq = xtalfreq;

	/* Recalculate alp_to_ilp_ratio just to aid in debuggability. Mainly to compare with the
	 * previously calculated alp_to_ilp_ratio.
	 */

	cur_coreidx = si_coreidx(bus->sih);
	si_setcore(bus->sih, PMU_CORE_ID, 0);
	pmu_idx = si_coreidx(bus->sih);

	alp_to_ilp_ratio = si_corereg(bus->sih, pmu_idx, offsetof(pmuregs_t, pmu_xtalfreq), 0, 0);
	alp_to_ilp_ratio = (alp_to_ilp_ratio & PMU_XTALFREQ_REG_ILPCTR_MASK) / 4;

	si_setcoreidx(bus->sih, cur_coreidx);

	DHD_ERROR(("Corrected Host/Dongle clock ratio %u with %dHz xtal frequency, ilp_tick: %d, "
			"alp_to_ilp_ratio: %d, alp_to_ilp_ratio (recalculated): %d\n", htclkratio,
			xtalfreq, bus->ilp_tick, bus->alp_to_ilp_ratio, alp_to_ilp_ratio));
}
#endif /* BCMQT_HW */

static bool
is_bmpu_supported(dhd_bus_t *bus)
{
	if (BCM4378_CHIP(bus->sih->chip) ||
		BCM4376_CHIP(bus->sih->chip) ||
		BCM4387_CHIP(bus->sih->chip) ||
		BCM4385_CHIP(bus->sih->chip)) {
		return TRUE;
	}
	return FALSE;
}

#define CHIP_COMMON_SCR_DHD_TO_BL_ADDR(sih) (SI_ENUM_BASE(sih) + CC_SCR_DHD_TO_BL)
#define CHIP_COMMON_SCR_BL_TO_DHD_ADDR(sih) (SI_ENUM_BASE(sih) + CC_SCR_BL_TO_DHD)
void
dhdpcie_bus_mpu_disable(dhd_bus_t *bus)
{
	volatile uint32 *cr4_regs;
	uint val = 0;

	if (is_bmpu_supported(bus) == FALSE) {
		return;
	}

	/* reset to default values dhd_to_bl and bl_to_dhd regs */
	(void)serialized_backplane_access(bus, CHIP_COMMON_SCR_DHD_TO_BL_ADDR(bus->sih),
		sizeof(val), &val, FALSE);
	(void)serialized_backplane_access(bus, CHIP_COMMON_SCR_BL_TO_DHD_ADDR(bus->sih),
		sizeof(val), &val, FALSE);

	cr4_regs = si_setcore(bus->sih, ARMCR4_CORE_ID, 0);
	if (cr4_regs == NULL) {
		DHD_ERROR(("%s: Failed to find ARM core!\n", __FUNCTION__));
		return;
	}
	if (R_REG(bus->osh, cr4_regs + ARMCR4REG_CORECAP) & ACC_MPU_MASK) {
		/* bus mpu is supported */
		W_REG(bus->osh, cr4_regs + ARMCR4REG_MPUCTRL, 0);
	}
}

uint32 pcie_slave_wrapper_offsets[] = {
	AI_ERRLOGCTRL,
	AI_ERRLOGDONE,
	AI_ERRLOGSTATUS,
	AI_ERRLOGADDRLO,
	AI_ERRLOGADDRHI,
	AI_ERRLOGID,
	AI_ERRLOGUSER,
	AI_ERRLOGFLAGS
};

#define PCIE_SLAVER_WRAPPER_BASE	0x18102000u

static INLINE void dhd_sbreg_op(dhd_pub_t *dhd, uint addr, uint *val, bool read);

static uint32
dhd_get_pcie_slave_wrapper(si_t *sih)
{
	uint32 pcie_slave_wrapper = 0;
	uint16 chipid = si_chipid(sih);

	switch (chipid) {
		case BCM4389_CHIP_ID:
		case BCM4387_CHIP_ID:
			pcie_slave_wrapper = PCIE_SLAVER_WRAPPER_BASE;
			break;
		default:
			pcie_slave_wrapper = 0;
			break;
	}

	return pcie_slave_wrapper;
}

static void
dhd_dump_pcie_slave_wrapper_regs(dhd_bus_t *bus)
{
	uint32 i, val;
	uint32 pcie_slave_wrapper_base = 0;
	uint32 total_wrapper_regs;

	if (bus->dhd == NULL) {
		return;
	}

	pcie_slave_wrapper_base = dhd_get_pcie_slave_wrapper(bus->sih);
	if (!pcie_slave_wrapper_base) {
		DHD_ERROR(("%s pcie slave wrapper base not populated\n", __FUNCTION__));
		return;
	}

	DHD_ERROR(("%s: ##### Dumping PCIe slave wrapper regs #####\n", __FUNCTION__));

	total_wrapper_regs =
		sizeof(pcie_slave_wrapper_offsets) / sizeof(pcie_slave_wrapper_offsets[0]);

	for (i = 0; i < total_wrapper_regs; i++) {
		dhd_sbreg_op(bus->dhd, (pcie_slave_wrapper_base + pcie_slave_wrapper_offsets[i]),
			&val, TRUE);
	}

	DHD_ERROR(("%s: ##### ##### #####\n", __FUNCTION__));
}

bool
dhd_check_htput_chip(dhd_bus_t *bus)
{
	bool htput_support = FALSE;
	uint16 chipid = si_chipid(bus->sih);

	switch (chipid) {
		/* Enable htput support for all 160Mhz chips */
		case BCM4388_CHIP_ID:
		case BCM4389_CHIP_ID:
		case BCM4397_CHIP_ID:
			htput_support = TRUE;
			break;
		default:
			htput_support = TRUE;
	}

	DHD_ERROR(("%s: htput_support:%d\n", __FUNCTION__, htput_support));

	return htput_support;
}

static bool
dhdpcie_dongle_attach(dhd_bus_t *bus)
{
	osl_t *osh = bus->osh;
	volatile void *regsva = (volatile void*)bus->regs;
	uint16 devid;
	uint32 val;
	sbpcieregs_t *sbpcieregs;
	bool dongle_reset_needed;
	uint16 chipid;

	BCM_REFERENCE(chipid);

	DHD_TRACE(("%s: ENTER\n", __FUNCTION__));

	chipid = dhd_get_chipid(bus);

	if (chipid == (uint16)-1)
		goto fail;

	/* Configure CTO Prevention functionality */
#if defined(BCMFPGA_HW) || defined(BCMQT_HW)
	DHD_ERROR(("Disable CTO\n"));
	bus->cto_enable = FALSE;
#else
#if defined(BCMPCIE_CTO_PREVENTION)

	if (BCM4349_CHIP(chipid) || BCM4350_CHIP(chipid) || BCM4345_CHIP(chipid)) {
		DHD_ERROR(("Disable CTO\n"));
		bus->cto_enable = FALSE;
	} else {
		DHD_ERROR(("Enable CTO\n"));
		bus->cto_enable = TRUE;
	}
#else
	DHD_ERROR(("Disable CTO\n"));
	bus->cto_enable = FALSE;
#endif /* BCMPCIE_CTO_PREVENTION */
#endif /* BCMFPGA_HW || BCMQT_HW */

	if (PCIECTO_ENAB(bus)) {
		/* WAR - Delay the CTO init in 4397A0. */
		if (!(BCM4397_CHIP(chipid) && dhd_get_chiprev(bus) == 0)) {
			dhdpcie_cto_init(bus, TRUE);
		}
	}

#ifdef CONFIG_ARCH_EXYNOS
	link_recovery = bus->dhd;
#endif /* CONFIG_ARCH_EXYNOS */

	dhd_init_pwr_req_lock(bus);
	dhd_init_bus_lp_state_lock(bus);
	dhd_init_backplane_access_lock(bus);

	bus->alp_only = TRUE;
	bus->sih = NULL;

	/* Checking PCIe bus status with reading configuration space */
	val = OSL_PCI_READ_CONFIG(osh, PCI_CFG_VID, sizeof(uint32));
	if (((val & 0xFFFF) != VENDOR_BROADCOM) && ((val & 0xFFFF) != VENDOR_SYNAPTICS)) {
		DHD_ERROR(("%s : failed to read PCI configuration space!\n", __FUNCTION__));
		goto fail;
	}
	devid = (val >> 16) & 0xFFFF;
	bus->cl_devid = devid;

	/* Set bar0 window to si_enum_base */
	dhdpcie_bus_cfg_set_bar0_win(bus, si_enum_base(devid));

	/*
	 * Checking PCI_SPROM_CONTROL register for preventing invalid address access
	 * due to switch address space from PCI_BUS to SI_BUS.
	 */
	val = OSL_PCI_READ_CONFIG(osh, PCI_SPROM_CONTROL, sizeof(uint32));
	if (val == 0xffffffff) {
		DHD_ERROR(("%s : failed to read SPROM control register\n", __FUNCTION__));
		goto fail;
	}

#if defined(NDIS)
	/* Save good copy of PCIe config space */
	if (BCME_OK != dhdpcie_config_save(bus)) {
		DHD_ERROR(("%s : failed to save PCI configuration space!\n", __FUNCTION__));
		goto fail;
	}
#endif

	/* si_attach() will provide an SI handle and scan the backplane */
	if (!(bus->sih = si_attach((uint)devid, osh, regsva, PCI_BUS, bus,
	                           &bus->vars, &bus->varsz))) {
		DHD_ERROR(("%s: si_attach failed!\n", __FUNCTION__));
		goto fail;
	}

	if (MULTIBP_ENAB(bus->sih) && (bus->sih->buscorerev >= 66)) {
		/*
		 * HW JIRA - CRWLPCIEGEN2-672
		 * Producer Index Feature which is used by F1 gets reset on F0 FLR
		 * fixed in REV68
		 */
		if (PCIE_ENUM_RESET_WAR_ENAB(bus->sih->buscorerev)) {
			dhdpcie_ssreset_dis_enum_rst(bus);
		}

		/* IOV_DEVRESET could exercise si_detach()/si_attach() again so reset
		*   dhdpcie_bus_release_dongle() --> si_detach()
		*   dhdpcie_dongle_attach() --> si_attach()
		*/
		bus->pwr_req_ref = 0;
	}

	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req_nolock(bus);
	}

	/* Calling after requesting dm1 (Wl) power as there are mac wrapper regs */
	dhd_dump_pcie_slave_wrapper_regs(bus);

	/* Get info on the ARM and SOCRAM cores... */
	/* Should really be qualified by device id */
	if ((si_setcore(bus->sih, ARM7S_CORE_ID, 0)) ||
	    (si_setcore(bus->sih, ARMCM3_CORE_ID, 0)) ||
	    (si_setcore(bus->sih, ARMCR4_CORE_ID, 0)) ||
	    (si_setcore(bus->sih, ARMCA7_CORE_ID, 0))) {
		bus->armrev = si_corerev(bus->sih);
		bus->coreid = si_coreid(bus->sih);
	} else {
		DHD_ERROR(("%s: failed to find ARM core!\n", __FUNCTION__));
		goto fail;
	}

	/* CA7 requires coherent bits on */
	if (bus->coreid == ARMCA7_CORE_ID) {
		val = dhdpcie_bus_cfg_read_dword(bus, PCIE_CFG_SUBSYSTEM_CONTROL, 4);
		dhdpcie_bus_cfg_write_dword(bus, PCIE_CFG_SUBSYSTEM_CONTROL, 4,
			(val | PCIE_BARCOHERENTACCEN_MASK));
	}

	/* EFI requirement - stop driver load if FW is already running
	*  need to do this here before pcie_watchdog_reset, because
	*  pcie_watchdog_reset will put the ARM back into halt state
	*/
	if (!dhdpcie_is_arm_halted(bus)) {
		DHD_ERROR(("%s: ARM is not halted,FW is already running! Abort.\n",
				__FUNCTION__));
		goto fail;
	}

	BCM_REFERENCE(dongle_reset_needed);

	/* For inbuilt drivers pcie clk req will be done by RC,
	 * so do not do clkreq from dhd
	 */
#if defined(linux) || defined(LINUX)
	if (dhd_download_fw_on_driverload)
#endif /* linux || LINUX */
	{
		/* Enable CLKREQ# */
		dhdpcie_clkreq(bus->osh, 1, 1);
	}

	/* Calculate htclkratio only for QT, for FPGA it is fixed at 30 */
#ifdef BCMQT_HW
	dhdpcie_htclkratio_cal(bus);
#endif /* BCMQT_HW */

	/*
	 * bus->dhd will be NULL if it is called from dhd_bus_attach, so need to reset
	 * without checking dongle_isolation flag, but if it is called via some other path
	 * like quiesce FLR, then based on dongle_isolation flag, watchdog_reset should
	 * be called.
	 */
	if (bus->dhd == NULL) {
		/* dhd_attach not yet happened, do dongle reset */
#ifdef DHD_SKIP_DONGLE_RESET_IN_ATTACH
		dongle_reset_needed = FALSE;
#else
		dongle_reset_needed = TRUE;
#endif /* DHD_SKIP_DONGLE_RESET_IN_ATTACH */
	} else {
		/* Based on dongle_isolationflag, reset dongle */
		dongle_reset_needed = !(bus->dhd->dongle_isolation);
	}

	/* Fix for FLR reset & CTO init specific to 4397a0. Write a value 0x1E in PMU CC reg18 */
	if (BCM4397_CHIP(bus->sih->chip) && (bus->sih->chiprev == 0)) {
		uint origidx = 0;
		chipcregs_t *cc;

		origidx = si_coreidx(bus->sih);
		pmu_corereg(bus->sih, SI_CC_IDX, chipcontrol_addr, ~0, PMU_CHIPCTL18);
		pmu_corereg(bus->sih, SI_CC_IDX, chipcontrol_data,
			(PMU_CC18_WL_BOOKER_FORCEPWRDWN_EN | PMU_CC18_WL_P_CHAN_TIMER_SEL_MASK),
			(PMU_CC18_WL_BOOKER_FORCEPWRDWN_EN |
			((PMU_CC18_WL_P_CHAN_TIMER_SEL_8ms << PMU_CC18_WL_P_CHAN_TIMER_SEL_OFF) &
			PMU_CC18_WL_P_CHAN_TIMER_SEL_MASK)));

		/* 4397A0 WAR. Set max rsrc mask to 0x7fffffff and force HT before CTO init */
		pmu_corereg(bus->sih, SI_CC_IDX, max_res_mask, ~0, 0x7fffffff);

		cc = (chipcregs_t*)si_setcore(bus->sih, CC_CORE_ID, 0);
		if (cc) {
			OR_REG(osh, &cc->clk_ctl_st, CCS_FORCEHT);
			SPINWAIT(((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL) == 0),
				PMU_MAX_TRANSITION_DLY);
		} else {
			DHD_ERROR(("CC is NULL\n"));
		}
		si_setcore(bus->sih, origidx, 0);
		if (PCIECTO_ENAB(bus)) {
			dhdpcie_cto_init(bus, TRUE);
		}
	}

	/* need to set the force_bt_quiesce flag here
	 * before calling dhdpcie_dongle_flr_or_pwr_toggle
	 */
	bus->force_bt_quiesce = TRUE;
	/*
	 * For buscorerev = 66 and after, F0 FLR should be done independent from F1.
	 * So don't need BT quiesce.
	 */
	if (bus->sih->buscorerev >= 66) {
		bus->force_bt_quiesce = FALSE;
	}

	/*
	 * Issue dongle to reset all the cores on the chip - similar to rmmod dhd
	 * This is required to avoid spurious interrupts to the Host and bring back
	 * dongle to a sane state (on host soft-reboot / watchdog-reboot).
	 */
	if (dongle_reset_needed) {
		dhdpcie_dongle_reset(bus);
	}

	dhdpcie_dongle_flr_or_pwr_toggle(bus);

	dhdpcie_bus_mpu_disable(bus);

	si_setcore(bus->sih, PCIE2_CORE_ID, 0);
	sbpcieregs = (sbpcieregs_t*)(bus->regs);

	/* WAR where the BAR1 window may not be sized properly */
	W_REG(osh, &sbpcieregs->configaddr, 0x4e0);
	val = R_REG(osh, &sbpcieregs->configdata);
	W_REG(osh, &sbpcieregs->configdata, val);

	/* if chip uses sysmem instead of tcm, typically ARM CA chips */
	if (si_setcore(bus->sih, SYSMEM_CORE_ID, 0)) {
		if (!(bus->orig_ramsize = si_sysmem_size(bus->sih))) {
			DHD_ERROR(("%s: failed to find SYSMEM memory!\n", __FUNCTION__));
			goto fail;
		}
		/* also populate base address */
		switch ((uint16)bus->sih->chip) {
			case BCM4385_CHIP_ID:
				bus->dongle_ram_base = CA7_4385_RAM_BASE;
				break;
			case BCM4388_CHIP_ID:
			case BCM4389_CHIP_ID:
				bus->dongle_ram_base = CA7_4389_RAM_BASE;
				break;
			default:
				/* also populate base address */
				bus->dongle_ram_base = 0x200000;
				DHD_ERROR(("%s: WARNING: Using default ram base at 0x%x\n",
					__FUNCTION__, bus->dongle_ram_base));
				break;
		}
	} else if (!si_setcore(bus->sih, ARMCR4_CORE_ID, 0)) {
		if (!(bus->orig_ramsize = si_socram_size(bus->sih))) {
			DHD_ERROR(("%s: failed to find SOCRAM memory!\n", __FUNCTION__));
			goto fail;
		}
	} else {
		/* cr4 has a different way to find the RAM size from TCM's */
		if (!(bus->orig_ramsize = si_tcm_size(bus->sih))) {
			DHD_ERROR(("%s: failed to find CR4-TCM memory!\n", __FUNCTION__));
			goto fail;
		}
		/* also populate base address */
		switch ((uint16)bus->sih->chip) {
		case BCM4339_CHIP_ID:
		case BCM4335_CHIP_ID:
			bus->dongle_ram_base = CR4_4335_RAM_BASE;
			break;
		case BCM4358_CHIP_ID:
		case BCM4354_CHIP_ID:
		case BCM4356_CHIP_ID:
		case BCM43567_CHIP_ID:
		case BCM43569_CHIP_ID:
		case BCM4350_CHIP_ID:
		case BCM43570_CHIP_ID:
			bus->dongle_ram_base = CR4_4350_RAM_BASE;
			break;
		case BCM4360_CHIP_ID:
			bus->dongle_ram_base = CR4_4360_RAM_BASE;
			break;

		case BCM4364_CHIP_ID:
			bus->dongle_ram_base = CR4_4364_RAM_BASE;
			break;

		CASE_BCM4345_CHIP:
			bus->dongle_ram_base = (bus->sih->chiprev < 6)  /* changed at 4345C0 */
				? CR4_4345_LT_C0_RAM_BASE : CR4_4345_GE_C0_RAM_BASE;
			break;
		CASE_BCM43602_CHIP:
			bus->dongle_ram_base = CR4_43602_RAM_BASE;
			break;
		case BCM4349_CHIP_GRPID:
			/* RAM based changed from 4349c0(revid=9) onwards */
			bus->dongle_ram_base = ((bus->sih->chiprev < 9) ?
				CR4_4349_RAM_BASE : CR4_4349_RAM_BASE_FROM_REV_9);
			break;
		case BCM4347_CHIP_ID:
		case BCM4357_CHIP_ID:
		case BCM4361_CHIP_ID:
			bus->dongle_ram_base = CR4_4347_RAM_BASE;
			break;
		case BCM43751_CHIP_ID:
			bus->dongle_ram_base = CR4_43751_RAM_BASE;
			break;
		case BCM43752_CHIP_ID:
			bus->dongle_ram_base = CR4_43752_RAM_BASE;
			break;
		case BCM43756_CHIP_ID:
			bus->dongle_ram_base = CR4_43756_RAM_BASE;
			break;
		case BCM43711_CHIP_ID:
			bus->dongle_ram_base = CR4_43711_RAM_BASE;
			break;
		case BCM4376_CHIP_GRPID:
			bus->dongle_ram_base = CR4_4376_RAM_BASE;
			break;
		case BCM4378_CHIP_GRPID:
			bus->dongle_ram_base = CR4_4378_RAM_BASE;
			break;
		case BCM4362_CHIP_ID:
			bus->dongle_ram_base = CR4_4362_RAM_BASE;
			break;
		case BCM4375_CHIP_ID:
		case BCM4369_CHIP_ID:
			bus->dongle_ram_base = CR4_4369_RAM_BASE;
			break;
		case BCM4377_CHIP_ID:
			bus->dongle_ram_base = CR4_4377_RAM_BASE;
			break;
		case BCM4381_CHIP_ID:
			bus->dongle_ram_base = CR4_4381_RAM_BASE;
			break;
		case BCM4382_CHIP_ID:
			bus->dongle_ram_base = CR4_4382_RAM_BASE;
			break;
		case BCM4387_CHIP_GRPID:
			bus->dongle_ram_base = CR4_4387_RAM_BASE;
			break;
		case BCM4385_CHIP_ID:
			bus->dongle_ram_base = CR4_4385_RAM_BASE;
			break;
		default:
			bus->dongle_ram_base = 0;
			DHD_ERROR(("%s: WARNING: Using default ram base at 0x%x\n",
			           __FUNCTION__, bus->dongle_ram_base));
		}
	}
	bus->ramsize = bus->orig_ramsize;
	if (dhd_dongle_ramsize) {
		dhdpcie_bus_dongle_setmemsize(bus, dhd_dongle_ramsize);
	}

	if (bus->ramsize > DONGLE_TCM_MAP_SIZE) {
		DHD_ERROR(("%s : invalid ramsize %d(0x%x) is returned from dongle\n",
				__FUNCTION__, bus->ramsize, bus->ramsize));
		goto fail;
	}

	DHD_ERROR(("DHD: dongle ram size is set to %d(orig %d) at 0x%x\n",
	           bus->ramsize, bus->orig_ramsize, bus->dongle_ram_base));

	dhdpcie_bar1_window_switch_enab(bus);

	/* Init bar1_switch_lock only after bar1_switch_enab is inited */
	dhd_init_bar1_switch_lock(bus);

	bus->srmemsize = si_socram_srmem_size(bus->sih);

	dhdpcie_bus_intr_init(bus);

	/* Set the poll and/or interrupt flags */
	bus->intr = (bool)dhd_intr;
	if ((bus->poll = (bool)dhd_poll))
		bus->pollrate = 1;
#ifdef DHD_DISABLE_ASPM
	dhd_bus_aspm_enable_rc_ep(bus, FALSE);
#endif /* DHD_DISABLE_ASPM */
#ifdef PCIE_OOB
	dhdpcie_oob_init(bus);
#endif /* PCIE_OOB */
#ifdef PCIE_INB_DW
	bus->inb_enabled = TRUE;
#endif /* PCIE_INB_DW */
#if defined(PCIE_OOB) || defined(PCIE_INB_DW)
	bus->ds_enabled = TRUE;
	bus->deep_sleep = TRUE;
#endif

	/*
	 * This check is to prevent to go back to enable during fw download
	 * after forced to disable by dhd commend(eg. "dhd dar_enable 0") after insmod
	 */
	if ((bus->dhd == NULL) || (bus->dhd->dongle_reset == FALSE)) {
		bus->dar_enabled = TRUE;
		bus->idma_enabled = TRUE;
		bus->ifrm_enabled = TRUE;
	}

	dhdpcie_pme_stat_clear(bus);

	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req_clear_nolock(bus);

		/*
		 * One time clearing of Common Power Domain since HW default is set
		 * Needs to be after FLR because FLR resets PCIe enum back to HW defaults
		 * for 4378B0 (rev 68).
		 * On 4378A0 (rev 66), PCIe enum reset is disabled due to CRWLPCIEGEN2-672
		 */
		si_srpwr_request(bus->sih, SRPWR_DMN0_PCIE_MASK, 0);

		/*
		 * WAR to fix ARM cold boot;
		 * Assert WL domain in DAR helps but not enum
		 */
		if (bus->sih->buscorerev >= 68) {
			dhd_bus_pcie_pwr_req_wl_domain(bus,
				DAR_PCIE_PWR_CTRL((bus->sih)->buscorerev), TRUE);
		}
	}

	DHD_TRACE(("%s: EXIT: SUCCESS\n", __FUNCTION__));

	return 0;

fail:
/* for EFI even if there is an error, load still succeeds
* so si_detach should not be called here, it is called during unload
*/
	/*
	 * As request irq is done later, till then CTO will not be detected,
	 * so unconditionally dump cfg and DAR registers.
	 */
	dhd_bus_dump_imp_cfg_registers(bus);
	/* Check if CTO has happened */
	if (PCIECTO_ENAB(bus)) {
		/* read pci_intstatus */
		uint32 pci_intstatus =
			dhdpcie_bus_cfg_read_dword(bus, PCI_INT_STATUS, 4);
		if (pci_intstatus == (uint32)-1) {
			DHD_ERROR(("%s : Invalid pci_intstatus(0x%x)\n",
				__FUNCTION__, pci_intstatus));
		} else if (pci_intstatus & PCI_CTO_INT_MASK) {
			DHD_ERROR(("%s: ##### CTO REPORTED BY DONGLE "
				"intstat=0x%x enab=%d\n", __FUNCTION__,
				pci_intstatus, bus->cto_enable));
		}
		dhd_bus_dump_dar_registers(bus);
	}
	dhd_deinit_pwr_req_lock(bus);
	dhd_deinit_bus_lp_state_lock(bus);
	dhd_deinit_backplane_access_lock(bus);

	if (bus->sih != NULL) {
		/* Dump DAR registers only if si_attach has succeeded */
		dhd_bus_dump_dar_registers(bus);
		if (MULTIBP_ENAB(bus->sih)) {
			dhd_bus_pcie_pwr_req_clear_nolock(bus);
		}

		si_detach(bus->sih);
		bus->sih = NULL;
	}
	DHD_TRACE(("%s: EXIT: FAILURE\n", __FUNCTION__));
	return -1;
}

int
dhpcie_bus_unmask_interrupt(dhd_bus_t *bus)
{
	dhdpcie_bus_cfg_write_dword(bus, PCIIntmask, 4, I_MB);
	return 0;
}
int
dhpcie_bus_mask_interrupt(dhd_bus_t *bus)
{
	dhdpcie_bus_cfg_write_dword(bus, PCIIntmask, 4, 0x0);
	return 0;
}

/* Non atomic function, caller should hold appropriate lock */
void
dhdpcie_bus_intr_enable(dhd_bus_t *bus)
{
	DHD_TRACE(("%s Enter\n", __FUNCTION__));
	if (bus) {
		if (bus->sih && !bus->is_linkdown) {
			/* Skip after receiving D3 ACK */
			if (__DHD_CHK_BUS_LPS_D3_ACKED(bus)) {
				return;
			}
			if ((bus->sih->buscorerev == 2) || (bus->sih->buscorerev == 6) ||
				(bus->sih->buscorerev == 4)) {
				dhpcie_bus_unmask_interrupt(bus);
			} else {
#ifdef DHD_MMIO_TRACE
				dhd_bus_mmio_trace(bus, bus->pcie_mailbox_mask,
					bus->def_intmask, TRUE);
#endif /* defined(DHD_MMIO_TRACE) */
				si_corereg(bus->sih, bus->sih->buscoreidx, bus->pcie_mailbox_mask,
					bus->def_intmask, bus->def_intmask);
			}
		}

#if defined(NDIS)
		dhd_msix_message_set(bus->dhd, 0, 0, TRUE);
#endif
	}

	DHD_TRACE(("%s Exit\n", __FUNCTION__));
}

/* Non atomic function, caller should hold appropriate lock */
void
dhdpcie_bus_intr_disable(dhd_bus_t *bus)
{
	DHD_TRACE(("%s Enter\n", __FUNCTION__));
	if (bus && bus->sih && !bus->is_linkdown) {
		/* Skip after receiving D3 ACK */
		if (__DHD_CHK_BUS_LPS_D3_ACKED(bus)) {
			return;
		}

		if ((bus->sih->buscorerev == 2) || (bus->sih->buscorerev == 6) ||
			(bus->sih->buscorerev == 4)) {
			dhpcie_bus_mask_interrupt(bus);
		} else {
#ifdef DHD_MMIO_TRACE
			dhd_bus_mmio_trace(bus, bus->pcie_mailbox_mask, 0, TRUE);
#endif /* defined(DHD_MMIO_TRACE) */
			si_corereg(bus->sih, bus->sih->buscoreidx, bus->pcie_mailbox_mask,
				bus->def_intmask, 0);
		}
	}
#if defined(NDIS)
	/*
	 * dhdpcie_bus_intr_disable may get called from
	 * dhdpcie_dongle_attach -> dhdpcie_dongle_reset
	 * with dhd = NULL during attach time. So check for bus->dhd NULL before
	 * calling dhd_msix_message_set
	 */
	if (bus && bus->dhd) {
		dhd_msix_message_set(bus->dhd, 0, 0, FALSE);
	}
#endif

	DHD_TRACE(("%s Exit\n", __FUNCTION__));
}

/*
 *  dhdpcie_advertise_bus_cleanup advertises that clean up is under progress
 * to other bus user contexts like Tx, Rx, IOVAR, WD etc and it waits for other contexts
 * to gracefully exit. All the bus usage contexts before marking busstate as busy, will check for
 * whether the busstate is DHD_BUS_DOWN or DHD_BUS_DOWN_IN_PROGRESS, if so
 * they will exit from there itself without marking dhd_bus_busy_state as BUSY.
 */
void
dhdpcie_advertise_bus_cleanup(dhd_pub_t *dhdp)
{
	unsigned long flags;
	int timeleft;

	dhdp->dhd_watchdog_ms_backup = dhd_watchdog_ms;
	if (dhdp->dhd_watchdog_ms_backup) {
		DHD_ERROR(("%s: Disabling wdtick before dhd deinit\n",
			__FUNCTION__));
		dhd_os_wd_timer(dhdp, 0);
	}
	if (dhdp->busstate != DHD_BUS_DOWN) {
#ifdef DHD_DONGLE_TRAP_IN_DETACH
		/*
		 * For x86 platforms, rmmod/insmod is failing due to some power
		 * resources are not held high.
		 * Hence induce DB7 trap during detach and in FW trap handler all
		 * power resources are held high.
		 */
		if (!dhd_query_bus_erros(dhdp) && dhdp->db7_trap.fw_db7w_trap) {
			dhdp->db7_trap.fw_db7w_trap_inprogress = TRUE;
			dhdpcie_fw_trap(dhdp->bus);
			OSL_DELAY(100 * 1000); // wait 100 msec
			if (!dhdp->db7_trap.fw_db7w_trap_recieved) {
				DHD_ERROR(("%s : Did not receive DB7 Ack\n", __FUNCTION__));
#ifdef WBRC
				if (dhdp->fw_mode_changed == FALSE) {
					DHD_ERROR(("%s : Set do_chip_bighammer\n", __FUNCTION__));
					dhdp->do_chip_bighammer = TRUE;
				}
#endif /* WBRC */
			}
			dhdp->db7_trap.fw_db7w_trap_inprogress = FALSE;
		} else {
			DHD_ERROR(("%s: DB7 Not sent!!!\n",
				__FUNCTION__));
		}
#endif /* DHD_DONGLE_TRAP_IN_DETACH */
		DHD_GENERAL_LOCK(dhdp, flags);
		dhdp->busstate = DHD_BUS_DOWN_IN_PROGRESS;
		DHD_GENERAL_UNLOCK(dhdp, flags);
	}

	timeleft = dhd_os_busbusy_wait_negation(dhdp, &dhdp->dhd_bus_busy_state);
#ifdef LINUX
	if ((timeleft == 0) || (timeleft == 1))
#else
	if (timeleft == 0)
#endif
	{
		/* XXX This condition ideally should not occur, this means some
		 * bus usage context is not clearing the respective usage bit, print
		 * dhd_bus_busy_state and crash the host for further debugging.
		 */
		DHD_ERROR(("%s : Timeout due to dhd_bus_busy_state=0x%x\n",
				__FUNCTION__, dhdp->dhd_bus_busy_state));
		ASSERT(0);
	}

	return;
}

static void
dhdpcie_advertise_bus_remove(dhd_pub_t	 *dhdp)
{
	unsigned long flags;
	int timeleft;

	DHD_GENERAL_LOCK(dhdp, flags);
	dhdp->busstate = DHD_BUS_REMOVE;
	DHD_GENERAL_UNLOCK(dhdp, flags);

	timeleft = dhd_os_busbusy_wait_negation(dhdp, &dhdp->dhd_bus_busy_state);
	if ((timeleft == 0) || (timeleft == 1)) {
		DHD_ERROR(("%s : Timeout due to dhd_bus_busy_state=0x%x\n",
				__FUNCTION__, dhdp->dhd_bus_busy_state));
		ASSERT(0);
	}

	return;
}

/*
 * dhdpcie_busbusy_wait mark busstate as DHD_BUS_DOWN_IN_PROGRESS and waits
 * for all the contexts to garacefully exit. All the bus usage contexts before
 * marking busstate as busy, will check for whether the busstate is DHD_BUS_DOWN
 * or DHD_BUS_DOWN_IN_PROGRESS, if so they will exit from there itself without
 * marking dhd_bus_busy_state as BUSY.
 */
void
dhdpcie_busbusy_wait(dhd_pub_t *dhdp)
{
	unsigned long flags;
	int timeleft;

	dhdp->dhd_watchdog_ms_backup = dhd_watchdog_ms;
	if (dhdp->dhd_watchdog_ms_backup) {
		DHD_ERROR(("%s: Disabling wdtick\n", __FUNCTION__));
		dhd_os_wd_timer(dhdp, 0);
	}

	DHD_GENERAL_LOCK(dhdp, flags);
	dhdp->busstate = DHD_BUS_DOWN_IN_PROGRESS;
	DHD_GENERAL_UNLOCK(dhdp, flags);

	timeleft = dhd_os_busbusy_wait_negation(dhdp, &dhdp->dhd_bus_busy_state);
	if ((timeleft == 0) || (timeleft == 1)) {
		DHD_ERROR(("%s : Timeout due to dhd_bus_busy_state=0x%x\n",
				__FUNCTION__, dhdp->dhd_bus_busy_state));
	}

	DHD_GENERAL_LOCK(dhdp, flags);
	dhdp->busstate = DHD_BUS_DOWN;
	DHD_GENERAL_UNLOCK(dhdp, flags);
	return;
}

static void
dhdpcie_bus_remove_prep(dhd_bus_t *bus)
{
	unsigned long flags;
	DHD_TRACE(("%s Enter\n", __FUNCTION__));

	DHD_GENERAL_LOCK(bus->dhd, flags);
	DHD_ERROR(("%s: making DHD_BUS_DOWN\n", __FUNCTION__));
	bus->dhd->busstate = DHD_BUS_DOWN;
	DHD_GENERAL_UNLOCK(bus->dhd, flags);

#ifdef PCIE_INB_DW
	/* De-Initialize the lock to serialize Device Wake Inband activities */
	if (bus->inb_lock) {
		osl_spin_lock_deinit(bus->dhd->osh, bus->inb_lock);
		bus->inb_lock = NULL;
	}
#endif

	dhd_os_sdlock(bus->dhd);

	if (bus->sih && !bus->dhd->dongle_isolation) {

		dhd_bus_pcie_pwr_req_reload_war(bus);

		/* Skip below WARs for Android as insmod fails after rmmod in Brix Android */
#if !defined(OEM_ANDROID)
		/* HW4347-909, Set PCIE TRefUp time to 100us for 4347/4377 */
		if ((bus->sih->buscorerev == 19) || (bus->sih->buscorerev == 23)) {
			pcie_set_trefup_time_100us(bus->sih);
		}

		/* disable fast lpo from 4347/4377 */
		/* Do not disable fast lpo for 4378/4387/4389/4381/4382.
		 * Because we always enable fast lpo. it causes insmod/rmmod reload failure.
		 */
		if ((PMUREV(bus->sih->pmurev) > 31) &&
			!(PCIE_FASTLPO_ENABLED(bus->sih->buscorerev))) {
			si_pmu_fast_lpo_disable(bus->sih);
		}
#endif /* !OEM_ANDROID */

		/* if the pcie link is down, watchdog reset
		* should not be done, as it may hang
		*/

		if (!bus->is_linkdown) {
			/* For Non-EFI modular builds, do dongle reset during rmmod */
			/* For EFI-DHD this compile flag will be defined.
			 * In EFI, depending on bt over pcie mode
			 * we either power toggle or do F0 FLR
			 * from dhdpcie_bus_release dongle. So no need to
			 * do dongle reset from here
			 */
			dhdpcie_dongle_reset(bus);
		}

		bus->dhd->is_pcie_watchdog_reset = TRUE;
	}

	dhd_os_sdunlock(bus->dhd);

	DHD_TRACE(("%s Exit\n", __FUNCTION__));
}

void
dhd_init_bus_lp_state_lock(dhd_bus_t *bus)
{
	if (!bus->bus_lp_state_lock) {
		bus->bus_lp_state_lock = osl_spin_lock_init(bus->osh);
	}
}

void
dhd_deinit_bus_lp_state_lock(dhd_bus_t *bus)
{
	if (bus->bus_lp_state_lock) {
		osl_spin_lock_deinit(bus->osh, bus->bus_lp_state_lock);
		bus->bus_lp_state_lock = NULL;
	}
}

void
dhd_init_backplane_access_lock(dhd_bus_t *bus)
{
	if (!bus->backplane_access_lock) {
		bus->backplane_access_lock = osl_spin_lock_init(bus->osh);
	}
}

void
dhd_deinit_backplane_access_lock(dhd_bus_t *bus)
{
	if (bus->backplane_access_lock) {
		osl_spin_lock_deinit(bus->osh, bus->backplane_access_lock);
		bus->backplane_access_lock = NULL;
	}
}

void
dhd_init_dpc_histos(dhd_pub_t *dhd)
{
	dhd_bus_t *bus = dhd->bus;
	if (!bus->dpc_time_histo) {
		bus->dpc_time_histo = dhd_histo_init(dhd);
	}
	if (!bus->ctrl_cpl_post_time_histo) {
		bus->ctrl_cpl_post_time_histo = dhd_histo_init(dhd);
	}
	if (!bus->tx_post_time_histo) {
		bus->tx_post_time_histo = dhd_histo_init(dhd);
	}
	if (!bus->tx_cpl_time_histo) {
		bus->tx_cpl_time_histo = dhd_histo_init(dhd);
	}
	if (!bus->rx_cpl_post_time_histo) {
		bus->rx_cpl_post_time_histo = dhd_histo_init(dhd);
	}
}

void
dhd_deinit_dpc_histos(dhd_pub_t *dhd)
{
	dhd_bus_t *bus = dhd->bus;
	if (bus->dpc_time_histo) {
		dhd_histo_deinit(dhd, bus->dpc_time_histo);
	}
	if (bus->ctrl_cpl_post_time_histo) {
		dhd_histo_deinit(dhd, bus->ctrl_cpl_post_time_histo);
	}
	if (bus->tx_post_time_histo) {
		dhd_histo_deinit(dhd, bus->tx_post_time_histo);
	}
	if (bus->tx_cpl_time_histo) {
		dhd_histo_deinit(dhd, bus->tx_cpl_time_histo);
	}
	if (bus->rx_cpl_post_time_histo) {
		dhd_histo_deinit(dhd, bus->rx_cpl_post_time_histo);
	}
}

void
dhd_dump_dpc_histos(dhd_pub_t *dhd, struct bcmstrbuf *strbuf)
{
	dhd_bus_t *bus = dhd->bus;
	bcm_bprintf(strbuf, "==== DPC Histograms in Usec ====\n");
	dhd_histo_tag_dump(dhd, strbuf, "usec/histo");
	dhd_histo_dump(dhd, strbuf, bus->dpc_time_histo, "dpc");
	dhd_histo_dump(dhd, strbuf, bus->ctrl_cpl_post_time_histo, "ctrl_cpl_post");
	dhd_histo_dump(dhd, strbuf, bus->tx_post_time_histo, "tx_post");
	dhd_histo_dump(dhd, strbuf, bus->tx_cpl_time_histo, "tx_cpl");
	dhd_histo_dump(dhd, strbuf, bus->rx_cpl_post_time_histo, "rx_cpl_post");
	bcm_bprintf(strbuf, "================================\n");
}

void
dhd_clear_dpc_histos(dhd_pub_t *dhd)
{
	dhd_bus_t *bus = dhd->bus;
	dhd_histo_clear(dhd, bus->dpc_time_histo);
	dhd_histo_clear(dhd, bus->ctrl_cpl_post_time_histo);
	dhd_histo_clear(dhd, bus->tx_post_time_histo);
	dhd_histo_clear(dhd, bus->tx_cpl_time_histo);
	dhd_histo_clear(dhd, bus->rx_cpl_post_time_histo);
}
/** Detach and free everything */
void
dhdpcie_bus_release(dhd_bus_t *bus)
{
	bool dongle_isolation = FALSE;
#ifdef BCMQT
	uint buscorerev = 0;
#endif /* BCMQT */
	osl_t *osh = NULL;
#if defined(__linux__)
	int bcmerror = 0;
#endif /* __linux__ */

	DHD_TRACE(("%s: Enter\n", __FUNCTION__));

	if (bus) {

		osh = bus->osh;
		ASSERT(osh);

		if (bus->dhd) {
#if defined(DEBUGGER) || defined(DHD_DSCOPE)
			debugger_close();
#endif /* DEBUGGER || DHD_DSCOPE */
			dhdpcie_advertise_bus_remove(bus->dhd);
			dongle_isolation = bus->dhd->dongle_isolation;
			bus->dhd->is_pcie_watchdog_reset = FALSE;
			dhdpcie_bus_remove_prep(bus);

			if (bus->intr) {
				dhdpcie_bus_intr_disable(bus);
				dhdpcie_free_irq(bus);
			}
			dhd_deinit_dpc_histos(bus->dhd);

			dhd_deinit_bus_lp_state_lock(bus);
			dhd_deinit_bar1_switch_lock(bus);
			dhd_deinit_backplane_access_lock(bus);
			dhd_deinit_pwr_req_lock(bus);
#ifdef PCIE_INB_DW
			dhd_deinit_dongle_ds_lock(bus);
#endif /* PCIE_INB_DW */
#ifdef BCMQT
			if (IDMA_ACTIVE(bus->dhd)) {
			/**
			 * On FPGA during exit path force set "IDMA Control Register"
			 * to default value 0x0. Otherwise host dongle syc for IDMA fails
			 * during next IDMA initilization(without system reboot)
			 */
				buscorerev = bus->sih->buscorerev;
				si_corereg(bus->sih, bus->sih->buscoreidx,
					IDMAControl(buscorerev), ~0, 0);
			}
#endif /* BCMQT */
			/**
			 * dhdpcie_bus_release_dongle free bus->sih  handle, which is needed to
			 * access Dongle registers.
			 * dhd_detach will communicate with dongle to delete flowring ..etc.
			 * So dhdpcie_bus_release_dongle should be called only after the dhd_detach.
			 */
			dhd_detach(bus->dhd);
			dhdpcie_bus_release_dongle(bus, osh, dongle_isolation, TRUE);
#if defined(__linux__)
			DHD_ERROR(("%s: disable pcie dev\n", __FUNCTION__));
			bcmerror = dhdpcie_bus_disable_device(bus);
			if (bcmerror) {
				DHD_ERROR(("%s: dhdpcie_bus_disable_device: %d\n",
					__FUNCTION__, bcmerror));
			}
			DHD_ERROR(("%s: stop host dev dev\n", __FUNCTION__));
			bcmerror = dhdpcie_bus_stop_host_dev(bus);
			if (bcmerror) {
				DHD_ERROR(("%s: dhdpcie_bus_stop_host_dev failed: %d\n",
					__FUNCTION__, bcmerror));
			}
#endif /* __linux__ */
			dhd_free(bus->dhd);
			bus->dhd = NULL;
		}
		/* unmap the regs and tcm here!! */
		if (bus->regs) {
			dhdpcie_bus_reg_unmap(osh, bus->regs, DONGLE_REG_MAP_SIZE);
			bus->regs = NULL;
		}
		if (bus->tcm) {
			dhdpcie_bus_reg_unmap(osh, bus->tcm, DONGLE_TCM_MAP_SIZE);
			bus->tcm = NULL;
		}

		dhdpcie_bus_release_malloc(bus, osh);
		/* Detach pcie shared structure */
		if (bus->pcie_sh) {
			MFREE(osh, bus->pcie_sh, sizeof(pciedev_shared_t));
		}

		if (bus->console.buf != NULL) {
			MFREE(osh, bus->console.buf, bus->console.bufsize);
		}

		/* Finally free bus info */
		MFREE(osh, bus, sizeof(dhd_bus_t));

		g_dhd_bus = NULL;
	}

	DHD_TRACE(("%s: Exit\n", __FUNCTION__));
} /* dhdpcie_bus_release */

void
dhdpcie_bus_release_dongle(dhd_bus_t *bus, osl_t *osh, bool dongle_isolation, bool reset_flag)
{
	DHD_TRACE(("%s: Enter bus->dhd %p bus->dhd->dongle_reset %d \n", __FUNCTION__,
		bus->dhd, bus->dhd->dongle_reset));

	if ((bus->dhd && bus->dhd->dongle_reset) && reset_flag) {
		goto fail;
	}

	if (bus->is_linkdown) {
		DHD_ERROR(("%s : Skip release dongle due to linkdown \n", __FUNCTION__));
		goto fail;
	}

	if (bus->sih) {

		/*
		 * Perform dongle reset only if dongle isolation is not enabled.
		 * In android platforms, dongle isolation will be enabled and
		 * quiescing dongle will be done using DB7 trap.
		 */
		if (!dongle_isolation &&
			bus->dhd && !bus->dhd->is_pcie_watchdog_reset) {
			dhdpcie_dongle_reset(bus);
		}

		/* Only for EFI this will be effective */
		dhdpcie_dongle_flr_or_pwr_toggle(bus);

		if (bus->ltrsleep_on_unload) {
			si_corereg(bus->sih, bus->sih->buscoreidx,
				OFFSETOF(sbpcieregs_t, u.pcie2.ltr_state), ~0, 0);
		}

		if (bus->sih->buscorerev == 13)
			 pcie_serdes_iddqdisable(bus->osh, bus->sih,
			                         (sbpcieregs_t *) bus->regs);

		/* For inbuilt drivers pcie clk req will be done by RC,
		 * so do not do clkreq from dhd
		 */
#if defined(linux) || defined(LINUX)
		if (dhd_download_fw_on_driverload)
#endif /* linux || LINUX */
		{
			/* Disable CLKREQ# */
			dhdpcie_clkreq(bus->osh, 1, 0);
		}
	}
fail:
	/* Resources should be freed */
	if (bus->sih) {
		si_detach(bus->sih);
		bus->sih = NULL;
	}
	if (bus->vars && bus->varsz) {
		MFREE(osh, bus->vars, bus->varsz);
		bus->vars = NULL;
	}

	DHD_TRACE(("%s Exit\n", __FUNCTION__));
}

uint32
dhdpcie_bus_cfg_read_dword(dhd_bus_t *bus, uint32 addr, uint32 size)
{
	uint32 data = OSL_PCI_READ_CONFIG(bus->osh, addr, size);
	return data;
}

/** 32 bit config write */
void
dhdpcie_bus_cfg_write_dword(dhd_bus_t *bus, uint32 addr, uint32 size, uint32 data)
{
	OSL_PCI_WRITE_CONFIG(bus->osh, addr, size, data);
}

void
dhdpcie_bus_cfg_set_bar0_win(dhd_bus_t *bus, uint32 data)
{
	OSL_PCI_WRITE_CONFIG(bus->osh, PCI_BAR0_WIN, 4, data);
}

void
dhdpcie_bus_dongle_setmemsize(struct dhd_bus *bus, int mem_size)
{
	int32 min_size =  DONGLE_MIN_MEMSIZE;
	/* Restrict the memsize to user specified limit */
	DHD_ERROR(("user: Restrict the dongle ram size to %d, min accepted %d max accepted %d\n",
		mem_size, min_size, (int32)bus->orig_ramsize));
	if ((mem_size > min_size) &&
		(mem_size < (int32)bus->orig_ramsize)) {
		bus->ramsize = mem_size;
	} else {
		DHD_ERROR(("%s: Invalid mem_size %d\n", __FUNCTION__, mem_size));
	}
}

void
dhdpcie_bus_release_malloc(dhd_bus_t *bus, osl_t *osh)
{
	DHD_TRACE(("%s: Enter\n", __FUNCTION__));

	if (bus->dhd && bus->dhd->dongle_reset)
		return;

	if (bus->vars && bus->varsz) {
		MFREE(osh, bus->vars, bus->varsz);
	}

	DHD_TRACE(("%s: Exit\n", __FUNCTION__));
	return;

}

/** Stop bus module: clear pending frames, disable data flow */
void dhd_bus_stop(struct dhd_bus *bus, bool enforce_mutex)
{
	unsigned long flags;

	DHD_TRACE(("%s: Enter\n", __FUNCTION__));

	if (!bus->dhd)
		return;

	if (bus->dhd->busstate == DHD_BUS_DOWN) {
		DHD_ERROR(("%s: already down by net_dev_reset\n", __FUNCTION__));
		goto done;
	}

	DHD_STOP_RPM_TIMER(bus->dhd);

	DHD_GENERAL_LOCK(bus->dhd, flags);
	DHD_ERROR(("%s: making DHD_BUS_DOWN\n", __FUNCTION__));
	bus->dhd->busstate = DHD_BUS_DOWN;
	DHD_GENERAL_UNLOCK(bus->dhd, flags);

#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
	atomic_set(&bus->dhd->block_bus, TRUE);
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

	dhdpcie_bus_intr_disable(bus);

	if (!bus->is_linkdown) {
		uint32 status;
		status = dhdpcie_bus_cfg_read_dword(bus, PCIIntstatus, 4);
		dhdpcie_bus_cfg_write_dword(bus, PCIIntstatus, 4, status);
	}

#if defined(linux) || defined(LINUX)
	if (!dhd_download_fw_on_driverload) {
		dhd_dpc_kill(bus->dhd);
	}
#endif /* linux || LINUX */

#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
	pm_runtime_disable(dhd_bus_to_dev(bus));
	pm_runtime_set_suspended(dhd_bus_to_dev(bus));
	pm_runtime_enable(dhd_bus_to_dev(bus));
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

	/* Clear rx control and wake any waiters */
	/* XXX More important in disconnect, but no context? */
	dhd_os_set_ioctl_resp_timeout(IOCTL_DISABLE_TIMEOUT);
	dhd_wakeup_ioctl_event(bus->dhd, IOCTL_RETURN_ON_BUS_STOP);

done:
	return;
}

#ifdef DEVICE_TX_STUCK_DETECT
void
dhd_bus_send_msg_to_daemon(int reason)
{
	bcm_to_info_t to_info;

	to_info.magic = BCM_TO_MAGIC;
	to_info.reason = reason;

	dhd_send_msg_to_daemon(NULL, (void *)&to_info, sizeof(bcm_to_info_t));
	return;
}

#define DHD_MEMORY_SET_PATTERN 0xAA

/**
 * scan the flow rings in active list to check if stuck and notify application
 * The conditions for warn/stuck detection are
 * 1. Flow ring is active
 * 2. There are packets to be consumed by the consumer (wr != rd)
 * If 1 and 2 are true, then
 * 3. Warn, if Tx completion is not received for a duration of DEVICE_TX_STUCK_WARN_DURATION
 * 4. Trap FW, if Tx completion is not received for a duration of DEVICE_TX_STUCK_DURATION
 */
static void
dhd_bus_device_tx_stuck_scan(dhd_bus_t *bus)
{
	uint32 tx_cmpl;
	unsigned long list_lock_flags;
	unsigned long ring_lock_flags;
	dll_t *item, *prev;
	flow_ring_node_t *flow_ring_node;
	if_flow_lkup_t *if_flow_lkup = (if_flow_lkup_t *)bus->dhd->if_flow_lkup;
	uint8 ifindex;
#ifndef FW_HAS_AGING_LOGIC_ALL_IF
	/**
	 * Since the aging logic is implemented only for INFRA in FW,
	 * DHD should monitor only INFRA for stuck detection.
	 */
	uint8 role;
#endif /* FW_HAS_AGING_LOGIC_ALL_IF */
	bool ring_empty;
	bool active;
	uint8 status;

	DHD_FLOWRING_LIST_LOCK(bus->dhd->flowring_list_lock, list_lock_flags);

	for (item = dll_tail_p(&bus->flowring_active_list);
			!dll_end(&bus->flowring_active_list, item); item = prev) {

		prev = dll_prev_p(item);

		flow_ring_node = dhd_constlist_to_flowring(item);
		ifindex = flow_ring_node->flow_info.ifindex;
#ifndef FW_HAS_AGING_LOGIC_ALL_IF
		role = if_flow_lkup[ifindex].role;
		if (role != WLC_E_IF_ROLE_STA) {
			continue;
		}
#endif /* FW_HAS_AGING_LOGIC_ALL_IF */
		DHD_FLOWRING_LOCK(flow_ring_node->lock, ring_lock_flags);
		tx_cmpl = flow_ring_node->tx_cmpl;
		active = flow_ring_node->active;
		status = flow_ring_node->status;
		ring_empty = dhd_prot_is_h2d_ring_empty(bus->dhd, flow_ring_node->prot_info);
		DHD_FLOWRING_UNLOCK(flow_ring_node->lock, ring_lock_flags);
		/*
		 * Need not monitor the flow ring if,
		 * 1. flow ring is empty
		 * 2. LINK is down
		 * 3. flow ring is not in FLOW_RING_STATUS_OPEN state
		 */
		if ((ring_empty) || !(if_flow_lkup[ifindex].status) ||
			(status != FLOW_RING_STATUS_OPEN)) {
			/* reset conters... etc */
			flow_ring_node->stuck_count = 0;
			flow_ring_node->tx_cmpl_prev = tx_cmpl;
			continue;
		}
		/**
		 * DEVICE_TX_STUCK_WARN_DURATION, DEVICE_TX_STUCK_DURATION are integer
		 * representation of time, to decide if a flow is in warn state or stuck.
		 *
		 * flow_ring_node->stuck_count is an integer counter representing how long
		 * tx_cmpl is not received though there are pending packets in the ring
		 * to be consumed by the dongle for that particular flow.
		 *
		 * This method of determining time elapsed is helpful in sleep/wake scenarios.
		 * If host sleeps and wakes up, that sleep time is not considered into
		 * stuck duration.
		 */
		if ((tx_cmpl == flow_ring_node->tx_cmpl_prev) && active) {

			flow_ring_node->stuck_count++;

			DHD_ERROR(("%s: flowid: %d tx_cmpl: %u tx_cmpl_prev: %u stuck_count: %d\n",
				__func__, flow_ring_node->flowid, tx_cmpl,
				flow_ring_node->tx_cmpl_prev, flow_ring_node->stuck_count));
			dhd_prot_dump_ring_ptrs(flow_ring_node->prot_info);

			switch (flow_ring_node->stuck_count) {
				case DEVICE_TX_STUCK_WARN_DURATION:
					/**
					 * Notify Device Tx Stuck Notification App about the
					 * device Tx stuck warning for this flowid.
					 * App will collect the logs required.
					 */
					DHD_ERROR(("stuck warning for flowid: %d sent to app\n",
						flow_ring_node->flowid));
					dhd_bus_send_msg_to_daemon(REASON_DEVICE_TX_STUCK_WARNING);
					break;
				case DEVICE_TX_STUCK_DURATION:
					/**
					 * Notify Device Tx Stuck Notification App about the
					 * device Tx stuck info for this flowid.
					 * App will collect the logs required.
					 */
					DHD_ERROR(("stuck information for flowid: %d sent to app\n",
						flow_ring_node->flowid));
					dhd_bus_send_msg_to_daemon(REASON_DEVICE_TX_STUCK);
					break;
				default:
					break;
			}
		} else {
			flow_ring_node->tx_cmpl_prev = tx_cmpl;
			flow_ring_node->stuck_count = 0;
		}
	}
	DHD_FLOWRING_LIST_UNLOCK(bus->dhd->flowring_list_lock, list_lock_flags);
}
/**
 * schedules dhd_bus_device_tx_stuck_scan after DEVICE_TX_STUCK_CKECK_TIMEOUT,
 * to determine if any flowid is stuck.
 */
static void
dhd_bus_device_stuck_scan(dhd_bus_t *bus)
{
	uint32 time_stamp; /* in millisec */
	uint32 diff;

	/* Need not run the algorith if Dongle has trapped */
	if (bus->dhd->dongle_trap_occured) {
		return;
	}
	time_stamp = OSL_SYSUPTIME();
	diff = time_stamp - bus->device_tx_stuck_check;
	if (diff > DEVICE_TX_STUCK_CKECK_TIMEOUT) {
		dhd_bus_device_tx_stuck_scan(bus);
		bus->device_tx_stuck_check = OSL_SYSUPTIME();
	}
	return;
}
#if defined(ASSOC_CHECK_SR)
void
dhd_assoc_check_sr(dhd_pub_t *dhd, bool state)
{
	int ret = -1;
	/* if stuck monitor is disabled */
	if (!dhd->bus->dev_tx_stuck_monitor) {
		return;
	}

	if (state) { /* suspend */
		dhd->assoc_at_suspend = dhd_is_associated(dhd, 0, NULL);
		DHD_ERROR(("%s assoc_at_suspend: %d\n", __FUNCTION__, dhd->assoc_at_suspend));
	} else if ((dhd->assoc_at_suspend) && (!dhd_is_associated(dhd, 0, NULL))) { /* resume */
		DHD_ERROR(("%s - assoc lost in D3. Issue disassoc\n", __FUNCTION__));
		ret = dhd_wl_ioctl_cmd(dhd, WLC_DISASSOC, NULL, 0, IOV_SET, 0);
		if (ret != BCME_OK) {
			DHD_ERROR(("%s error issuing disassoc, %d\n", __FUNCTION__, ret));
		}
	}
}
#endif /* ASSOC_CHECK_SR */
#endif /* DEVICE_TX_STUCK_DETECT */

/**
 * Watchdog timer function.
 * @param dhd   Represents a specific hardware (dongle) instance that this DHD manages
 */
bool dhd_bus_watchdog(dhd_pub_t *dhd)
{
	unsigned long flags;
	dhd_bus_t *bus = dhd->bus;

	if (dhd_query_bus_erros(bus->dhd)) {
		return FALSE;
	}

	DHD_GENERAL_LOCK(dhd, flags);
	if (DHD_BUS_CHECK_DOWN_OR_DOWN_IN_PROGRESS(dhd) ||
			DHD_BUS_CHECK_SUSPEND_OR_SUSPEND_IN_PROGRESS(dhd)) {
		DHD_GENERAL_UNLOCK(dhd, flags);
		return FALSE;
	}
	DHD_BUS_BUSY_SET_IN_WD(dhd);
	DHD_GENERAL_UNLOCK(dhd, flags);

	/* Poll for console output periodically */
	if (dhd->busstate == DHD_BUS_DATA &&
		dhd->dhd_console_ms != 0 &&
		DHD_CHK_BUS_NOT_IN_LPS(bus)) {
		bus->console.count += dhd_watchdog_ms;
		if (bus->console.count >= dhd->dhd_console_ms) {
			bus->console.count -= dhd->dhd_console_ms;

			if (MULTIBP_ENAB(bus->sih)) {
				dhd_bus_pcie_pwr_req(bus);
			}

			/* Make sure backplane clock is on */
			if (dhd->db7_trap.fw_db7w_trap_inprogress == FALSE) {
				if (dhdpcie_bus_readconsole(bus) < 0) {
					DHD_ERROR(("%s: disable dconpoll\n", __FUNCTION__));
					dhd->dhd_console_ms = 0; /* On error, stop trying */
				}
			}

			if (MULTIBP_ENAB(bus->sih)) {
				dhd_bus_pcie_pwr_req_clear(bus);
			}
		}
	}

#ifdef DHD_READ_INTSTATUS_IN_DPC
	if (bus->poll) {
		bus->ipend = TRUE;
		bus->dpc_sched = TRUE;
		dhd_sched_dpc(bus->dhd);     /* queue DPC now!! */
	}
#endif /* DHD_READ_INTSTATUS_IN_DPC */

#ifdef DEVICE_TX_STUCK_DETECT
	if (dhd->bus->dev_tx_stuck_monitor == TRUE) {
		dhd_bus_device_stuck_scan(bus);
	}
#endif /* DEVICE_TX_STUCK_DETECT */

	DHD_GENERAL_LOCK(dhd, flags);
	DHD_BUS_BUSY_CLEAR_IN_WD(dhd);
	dhd_os_busbusy_wake(dhd);
	DHD_GENERAL_UNLOCK(dhd, flags);
#if (defined(PCIE_OOB) || defined(PCIE_INB_DW))
	dhd->bus->inb_dw_deassert_cnt += dhd_watchdog_ms;
	if (dhd->bus->inb_dw_deassert_cnt >=
		DHD_INB_DW_DEASSERT_MS) {
		dhd->bus->inb_dw_deassert_cnt = 0;
		/* Inband device wake is deasserted from DPC context after DS_Exit is received,
		 * but if at all there is no d2h interrupt received, dpc will not be scheduled
		 * and inband DW is not deasserted, hence DW is deasserted from watchdog thread
		 * for every 250ms.
		 */
		dhd_bus_dw_deassert(dhd);
	}
#endif
	return TRUE;
} /* dhd_bus_watchdog */

#if defined(SUPPORT_MULTIPLE_REVISION)
static int concate_revision_bcm4358(dhd_bus_t *bus, char *fw_path, char *nv_path)
{
	uint32 chiprev;
#if defined(SUPPORT_MULTIPLE_CHIPS)
	char chipver_tag[20] = "_4358";
#else
	char chipver_tag[10] = {0, };
#endif /* SUPPORT_MULTIPLE_CHIPS */

	chiprev = dhd_bus_chiprev(bus);
	if (chiprev == 0) {
		DHD_ERROR(("----- CHIP 4358 A0 -----\n"));
		strcat(chipver_tag, "_a0");
	} else if (chiprev == 1) {
		DHD_ERROR(("----- CHIP 4358 A1 -----\n"));
#if defined(SUPPORT_MULTIPLE_CHIPS) || defined(SUPPORT_MULTIPLE_MODULE_CIS)
		strcat(chipver_tag, "_a1");
#endif /* defined(SUPPORT_MULTIPLE_CHIPS) || defined(SUPPORT_MULTIPLE_MODULE_CIS) */
	} else if (chiprev == 3) {
		DHD_ERROR(("----- CHIP 4358 A3 -----\n"));
#if defined(SUPPORT_MULTIPLE_CHIPS)
		strcat(chipver_tag, "_a3");
#endif /* SUPPORT_MULTIPLE_CHIPS */
	} else {
		DHD_ERROR(("----- Unknown chip version, ver=%x -----\n", chiprev));
	}

	strcat(fw_path, chipver_tag);

#if defined(SUPPORT_MULTIPLE_MODULE_CIS) && defined(USE_CID_CHECK)
	if (chiprev == 1 || chiprev == 3) {
		int ret = dhd_check_module_b85a();
		if ((chiprev == 1) && (ret < 0)) {
			memset(chipver_tag, 0x00, sizeof(chipver_tag));
			strcat(chipver_tag, "_b85");
			strcat(chipver_tag, "_a1");
		}
	}

	DHD_ERROR(("%s: chipver_tag %s \n", __FUNCTION__, chipver_tag));
#endif /* defined(SUPPORT_MULTIPLE_MODULE_CIS) && defined(USE_CID_CHECK) */

#if defined(SUPPORT_MULTIPLE_BOARD_REV)
	if (system_rev >= 10) {
		DHD_ERROR(("----- Board Rev  [%d]-----\n", system_rev));
		strcat(chipver_tag, "_r10");
	}
#endif /* SUPPORT_MULTIPLE_BOARD_REV */
	strcat(nv_path, chipver_tag);

	return 0;
}

static int concate_revision_bcm4359(dhd_bus_t *bus, char *fw_path, char *nv_path)
{
	uint32 chip_ver;
	char chipver_tag[10] = {0, };
#if defined(SUPPORT_MULTIPLE_MODULE_CIS) && defined(USE_CID_CHECK) && \
	defined(SUPPORT_BCM4359_MIXED_MODULES)
	int module_type = -1;
#endif /* SUPPORT_MULTIPLE_MODULE_CIS && USE_CID_CHECK && SUPPORT_BCM4359_MIXED_MODULES */

	chip_ver = bus->sih->chiprev;
	if (chip_ver == 4) {
		DHD_ERROR(("----- CHIP 4359 B0 -----\n"));
		strncat(chipver_tag, "_b0", strlen("_b0"));
	} else if (chip_ver == 5) {
		DHD_ERROR(("----- CHIP 4359 B1 -----\n"));
		strncat(chipver_tag, "_b1", strlen("_b1"));
	} else if (chip_ver == 9) {
		DHD_ERROR(("----- CHIP 4359 C0 -----\n"));
		strncat(chipver_tag, "_c0", strlen("_c0"));
	} else {
		DHD_ERROR(("----- Unknown chip version, ver=%x -----\n", chip_ver));
		return -1;
	}

#if defined(SUPPORT_MULTIPLE_MODULE_CIS) && defined(USE_CID_CHECK) && \
	defined(SUPPORT_BCM4359_MIXED_MODULES)
	module_type =  dhd_check_module_b90();

	switch (module_type) {
		case BCM4359_MODULE_TYPE_B90B:
			strcat(fw_path, chipver_tag);
			break;
		case BCM4359_MODULE_TYPE_B90S:
		default:
			/*
			 * .cid.info file not exist case,
			 * loading B90S FW force for initial MFG boot up.
			*/
			if (chip_ver == 5) {
				strncat(fw_path, "_b90s", strlen("_b90s"));
			}
			strcat(fw_path, chipver_tag);
			strcat(nv_path, chipver_tag);
			break;
	}
#else /* SUPPORT_MULTIPLE_MODULE_CIS && USE_CID_CHECK && SUPPORT_BCM4359_MIXED_MODULES */
	strcat(fw_path, chipver_tag);
	strcat(nv_path, chipver_tag);
#endif /* SUPPORT_MULTIPLE_MODULE_CIS && USE_CID_CHECK && SUPPORT_BCM4359_MIXED_MODULES */

	return 0;
}

#define NVRAM_FEM_MURATA	"_murata"
#if defined(DHD_FW_COREDUMP) && defined(DHD_COREDUMP)
extern char map_path[PATH_MAX];
#endif /* DHD_FW_COREDUMP && DHD_COREDUMP */

static int
concate_revision_from_cisinfo(dhd_bus_t *bus, char *fw_path, char *nv_path)
{
	int ret = BCME_OK;
#if defined(SUPPORT_MIXED_MODULES)
#if defined(USE_CID_CHECK)
	char module_type[MAX_VNAME_LEN];
	naming_info_t *info = NULL;
	bool is_murata_fem = FALSE;

	memset(module_type, 0, sizeof(module_type));

	if (dhd_check_module_bcm(module_type,
			MODULE_NAME_INDEX_MAX, &is_murata_fem) == BCME_OK) {
		info = dhd_find_naming_info(bus->dhd, module_type);
	} else {
		/* in case of .cid.info doesn't exists */
		info = dhd_find_naming_info_by_chip_rev(bus->dhd, &is_murata_fem);
	}

#ifdef BCM4361_CHIP
	if (bcmstrnstr(nv_path, PATH_MAX,  "_murata", 7)) {
		is_murata_fem = FALSE;
	}
#endif /* BCM4361_CHIP */

	if (info) {
#ifdef BCM4361_CHIP
		if (is_murata_fem) {
			strncat(nv_path, NVRAM_FEM_MURATA, strlen(NVRAM_FEM_MURATA));
		}
#endif /* BCM4361_CHIP */
		strncat(nv_path, info->nvram_ext, strlen(info->nvram_ext));
#if defined(SUPPORT_MULTIPLE_NVRAM) || defined(SUPPORT_MULTIPLE_CLMBLOB)
		dhd_set_platform_ext_name_for_chip_version(info->nvram_ext);
#endif /* SUPPORT_MULTIPLE_NVRAM || SUPPORT_MULTIPLE_CLMBLOB */
		strncat(fw_path, info->fw_ext, strlen(info->fw_ext));
#if defined(DHD_COREDUMP) && defined(SUPPORT_MULTIPLE_REVISION_MAP)
		if (!bcmstrnstr(map_path, PATH_MAX, info->fw_ext, strlen(info->fw_ext)))
			strncat(map_path, info->fw_ext, strlen(info->fw_ext));
#endif /* DHD_COREDUMP  && SUPPORT_MULTIPLE_REVISION_MAP */
	} else {
		DHD_ERROR(("%s:failed to find extension for nvram and firmware\n", __FUNCTION__));
		ret = BCME_ERROR;
	}
#endif /* USE_CID_CHECK */
#ifdef USE_DIRECT_VID_TAG
	int revid = bus->sih->chiprev;
	unsigned char chipstr[MAX_VID_LEN];

	memset(chipstr, 0, sizeof(chipstr));
	snprintf(chipstr, sizeof(chipstr), "_4389");

	/* write chipstr/vid into nvram tag */
	ret = concate_nvram_by_vid(bus->dhd, nv_path, chipstr);
	/* write chiprev into FW tag */
	if (ret == BCME_OK) {
		if (revid == 1) {
			strncat(fw_path, B0_REV, strlen(fw_path));
			DHD_ERROR(("%s: fw_path : %s\n", __FUNCTION__, fw_path));
		} else if (revid == 2) {
			strncat(fw_path, C0_REV, strlen(fw_path));
			DHD_ERROR(("%s: fw_path : %s\n", __FUNCTION__, fw_path));
		} else if (revid == 3) {
			strncat(fw_path, C1_REV, strlen(fw_path));
			DHD_ERROR(("%s: fw_path : %s\n", __FUNCTION__, fw_path));

		} else {
			DHD_ERROR(("%s: INVALID CHIPREV %d\n", __FUNCTION__, revid));
		}
	}
#endif /* USE_DIRECT_VID_TAG */
#else /* SUPPORT_MIXED_MODULE */
	char chipver_tag[10] = {0, };

	strcat(fw_path, chipver_tag);
	strcat(nv_path, chipver_tag);
#endif /* SUPPORT_MIXED_MODULE */

	return ret;
}

#ifdef CONCAT_DEF_REV_FOR_NOMATCH_VID
#define DEF_REVISION "_c0"
static int
vendor_concat_revision(dhd_bus_t *bus, char *fw_path, char *nv_path)
{
	int ret = BCME_OK;
	char *tag = DEF_REVISION;

	DHD_ERROR(("%s:failed to find extension, use %s as default\n", __FUNCTION__, DEF_REVISION));
	/* Failed to find info, use _c0 as default */
	strlcat(nv_path, tag, PATH_MAX);
	strlcat(fw_path, tag, PATH_MAX);
#if defined(DHD_COREDUMP) && defined(SUPPORT_MULTIPLE_REVISION_MAP)
	if (!bcmstrnstr(map_path, PATH_MAX, tag, strlen(tag))) {
		strlcat(map_path, tag, PATH_MAX);
	}
#endif /* DHD_COREDUMP && SUPPORT_MULTIPLE_REVISION_MAP */
	return ret;
}
#endif /* CONCAT_DEF_REV_FOR_NOMATCH_VID */

int
concate_revision(dhd_bus_t *bus, char *fw_path, char *nv_path)
{
	int res = 0;

	if (!bus || !bus->sih) {
		DHD_ERROR(("%s:Bus is Invalid\n", __FUNCTION__));
		return -1;
	}

	if (!fw_path || !nv_path) {
		DHD_ERROR(("fw_path or nv_path is null.\n"));
		return res;
	}

	switch (si_chipid(bus->sih)) {

	case BCM43569_CHIP_ID:
	case BCM4358_CHIP_ID:
		res = concate_revision_bcm4358(bus, fw_path, nv_path);
		break;
	case BCM4355_CHIP_ID:
	case BCM4359_CHIP_ID:
		res = concate_revision_bcm4359(bus, fw_path, nv_path);
		break;
	case BCM4361_CHIP_ID:
	case BCM4347_CHIP_ID:
	case BCM4375_CHIP_ID:
	case BCM4389_CHIP_ID:
		res = concate_revision_from_cisinfo(bus, fw_path, nv_path);
		break;
	default:
		DHD_ERROR(("REVISION SPECIFIC feature is not required\n"));
		return res;
	}

#ifdef CONCAT_DEF_REV_FOR_NOMATCH_VID
	if (res != BCME_OK) {
		res = vendor_concat_revision(bus, fw_path, nv_path);
	}
#endif /* CONCAT_DEF_REV_FOR_NOMATCH_VID */
	return res;
}
#endif /* SUPPORT_MULTIPLE_REVISION */

uint16
dhd_get_chipid(struct dhd_bus *bus)
{
	if (bus && bus->sih) {
		return (uint16)si_chipid(bus->sih);
	} else if (bus && bus->regs) {
		chipcregs_t *cc = (chipcregs_t *)bus->regs;
		uint w, chipid;

		/* Set bar0 window to si_enum_base */
		dhdpcie_bus_cfg_set_bar0_win(bus, si_enum_base(0));

		w = R_REG(bus->osh, &cc->chipid);
		chipid = w & CID_ID_MASK;

		return (uint16)chipid;
	} else {
		return 0;
	}
}

uint16
dhd_get_chiprev(struct dhd_bus *bus)
{
	if (bus && bus->sih) {
		return bus->sih->chiprev;
	} else if (bus && bus->regs) {
		chipcregs_t *cc = (chipcregs_t *)bus->regs;
		uint w, chiprev;

		/* Set bar0 window to si_enum_base */
		dhdpcie_bus_cfg_set_bar0_win(bus, si_enum_base(0));

		w = R_REG(bus->osh, &cc->chipid);
		chiprev = (w & CID_REV_MASK) >> CID_REV_SHIFT;

		return (uint16)chiprev;
	} else {
		return 0xffff;
	}
}

/**
 * Loads firmware given by caller supplied path and nvram image into PCIe dongle.
 *
 * BCM_REQUEST_FW specific :
 * Given the chip type, determines the to be used file paths within /lib/firmware/brcm/ containing
 * firmware and nvm for that chip. If the download fails, retries download with a different nvm file
 *
 * BCMEMBEDIMAGE specific:
 * If bus->fw_path is empty, or if the download of bus->fw_path failed, firmware contained in header
 * file will be used instead.
 *
 * @return BCME_OK on success
 */
int
dhd_bus_download_firmware(struct dhd_bus *bus, osl_t *osh,
                          char *pfw_path, char *pnv_path,
                          char *pclm_path, char *pconf_path)
{
	int ret;

	bus->fw_path = pfw_path;
	bus->nv_path = pnv_path;
	bus->dhd->clm_path = pclm_path;
	bus->dhd->conf_path = pconf_path;

#if defined(SUPPORT_MULTIPLE_REVISION)
	if (concate_revision(bus, bus->fw_path, bus->nv_path) != 0) {
		DHD_ERROR(("%s: fail to concatnate revison \n",
			__FUNCTION__));
		/* Proceed if SUPPORT_MULTIPLE_CHIPS is enabled */
#ifndef SUPPORT_MULTIPLE_CHIPS
		return BCME_BADARG;
#endif /* !SUPPORT_MULTIPLE_CHIPS */
	}
#endif /* SUPPORT_MULTIPLE_REVISION */

#if defined(SUPPORT_MULTIPLE_NVRAM)
	if (dhd_get_platform_naming_for_nvram_clmblob_file(NVRAM, bus->nv_path) != BCME_OK) {
		DHD_ERROR(("%s: Platform's NVRAMs file don't exist."
			" Download default NVRAM file.\n", __FUNCTION__));
	}
#endif /* SUPPORT_MULTIPLE_NVRAM */

#if defined(SUPPORT_MULTIPLE_BOARD_REVISION)
	concate_custom_board_revision(bus->nv_path);
#endif /* SUPPORT_MULTIPLE_BOARD_REVISION */

#if defined(DHD_BLOB_EXISTENCE_CHECK)
	dhd_set_blob_support(bus->dhd, bus->fw_path);
#endif /* DHD_BLOB_EXISTENCE_CHECK */

	DHD_ERROR(("%s: firmware path=%s, nvram path=%s\n",
		__FUNCTION__, bus->fw_path, bus->nv_path));
#if defined(LINUX) || defined(linux)
	dhdpcie_dump_resource(bus);
#endif /* LINUX || linux */

	ret = dhdpcie_download_firmware(bus, osh);

	return ret;
}

void
dhd_set_bus_params(struct dhd_bus *bus)
{
	struct dhd_conf *conf = bus->dhd->conf;

	if (conf->dhd_poll >= 0) {
		bus->poll = conf->dhd_poll;
		if (!bus->pollrate)
			bus->pollrate = 1;
		printf("%s: set polling mode %d\n", __FUNCTION__, conf->dhd_poll);
	}
	if (conf->d2h_intr_control >= 0)
		bus->d2h_intr_control = conf->d2h_intr_control;
	printf("d2h_intr_method -> %s(%d); d2h_intr_control -> %s(%d)\n",
		bus->d2h_intr_method ? "PCIE_MSI" : "PCIE_INTX", bus->d2h_intr_method,
		bus->d2h_intr_control ? "HOST_IRQ" : "D2H_INTMASK", bus->d2h_intr_control);

	if (conf->aspm != -1) {
		bool aspm = conf->aspm ? TRUE : FALSE;
		dhd_bus_aspm_enable_rc_ep(bus, aspm);
	}
	if (conf->l1ss != -1) {
		bool l1ss = conf->l1ss ? TRUE : FALSE;
		dhd_bus_l1ss_enable_rc_ep(bus, l1ss);
	}
}

/**
 * Loads firmware given by 'bus->fw_path' into PCIe dongle.
 *
 * BCM_REQUEST_FW specific :
 * Given the chip type, determines the to be used file paths within /lib/firmware/brcm/ containing
 * firmware and nvm for that chip. If the download fails, retries download with a different nvm file
 *
 * BCMEMBEDIMAGE specific:
 * If bus->fw_path is empty, or if the download of bus->fw_path failed, firmware contained in header
 * file will be used instead.
 *
 * @return BCME_OK on success
 */
#if defined(BCM_REQUEST_FW)
extern char clm_path[MOD_PARAM_PATHLEN];
#endif /* BCM_REQUEST_FW */
static int
dhdpcie_download_firmware(struct dhd_bus *bus, osl_t *osh)
{
	int ret = 0;
#if defined(BCM_REQUEST_FW)
	uint chipid = bus->sih->chip;
	uint revid = bus->sih->chiprev;
#if defined(DHD_LINUX_STD_FW_API)
	char fw_path[64] = "brcm/bcm";	/* path to firmware image */
#else
	char fw_path[64] = "/lib/firmware/brcm/bcm";	/* path to firmware image */
#endif /* DHD_LINUX_STD_FW_API */
	char nv_path[64];		/* path to nvram vars file */

	bus->fw_path = fw_path;
	bus->nv_path = nv_path;
	switch (chipid) {
	case BCM43570_CHIP_ID:
		bcmstrncat(fw_path, "43570", 5);
		switch (revid) {
		case 0:
			bcmstrncat(fw_path, "a0", 2);
			break;
		case 2:
			bcmstrncat(fw_path, "a2", 2);
			break;
		default:
			DHD_ERROR(("%s: revid is not found %x\n", __FUNCTION__,
			revid));
			break;
		}
		break;
	case BCM4381_CHIP_ID:
		bcmstrncat(fw_path, "4381", 4);
		switch (revid) {
		case 1:
			bcmstrncat(fw_path, "a0-pcie", 7);
			break;
		default:
			break;
		}
		break;
	default:
		DHD_ERROR(("%s: unsupported device %x\n", __FUNCTION__,
		chipid));
		return 0;
	}
	/* load board specific nvram file */
	snprintf(bus->nv_path, sizeof(nv_path), "%s.nvm", fw_path);
	/* load signature */
	snprintf(bus->fwsig_filename, sizeof(bus->fwsig_filename), "%s.sig", fw_path);
	/* load clm_blob */
	snprintf(clm_path, sizeof(clm_path), "%s.clm_blob", fw_path);
	/* load firmware */
	snprintf(bus->fw_path, sizeof(fw_path), "%s.bin", fw_path);
#endif /* BCM_REQUEST_FW */

	DHD_OS_WAKE_LOCK(bus->dhd);

	dhd_conf_set_path_params(bus->dhd, bus->fw_path, bus->nv_path);
	dhd_set_bus_params(bus);

	ret = _dhdpcie_download_firmware(bus);

	DHD_OS_WAKE_UNLOCK(bus->dhd);
	return ret;
} /* dhdpcie_download_firmware */

#ifdef DHD_LINUX_STD_FW_API
static int
dhdpcie_download_code_file(struct dhd_bus *bus, char *pfw_path)
{
	int bcmerror = BCME_ERROR;
	int offset = 0;
	int len = 0;
	bool store_reset;
	int offset_end = bus->ramsize;
	const struct firmware *fw = NULL;
	int buf_offset = 0, residual_len = 0;
#ifdef CHECK_DOWNLOAD_FW
	uint8 *memptr_tmp = NULL; // terence: check downloaded firmware is correct
#endif

#if defined(DHD_FW_MEM_CORRUPTION)
	if (dhd_bus_get_fw_mode(bus->dhd) == DHD_FLAG_MFG_MODE) {
		dhd_tcm_test_enable = TRUE;
	} else {
		dhd_tcm_test_enable = FALSE;
	}
#endif /* DHD_FW_MEM_CORRUPTION */
	DHD_ERROR(("%s: dhd_tcm_test_enable %u, dhd_tcm_test_status %u, dhd_tcm_test_mode %u\n",
		__FUNCTION__, dhd_tcm_test_enable, dhd_tcm_test_status, dhd_tcm_test_mode));

	if (dhd_tcm_test_enable && dhd_tcm_test_mode != TCM_TEST_MODE_DISABLE) {
		if (((dhd_tcm_test_mode == TCM_TEST_MODE_ONCE) &&
				(dhd_tcm_test_status == TCM_TEST_NOT_RUN)) ||
				(dhd_tcm_test_mode == TCM_TEST_MODE_ALWAYS)) {
			if (dhd_tcm_test_status != TCM_TEST_PASSED) {
				if (dhd_bus_tcm_test(bus) == FALSE) {
					DHD_ERROR(("dhd_bus_tcm_test failed\n"));
					dhd_tcm_test_status = TCM_TEST_FAILED;
					bcmerror = BCME_ERROR;
					goto err;
				} else {
					dhd_tcm_test_status = TCM_TEST_PASSED;
				}
			}
		}
	}

	DHD_ERROR(("%s: download firmware %s\n", __FUNCTION__, pfw_path));

	/* check if CR4/CA7 */
	store_reset = (si_setcore(bus->sih, ARMCR4_CORE_ID, 0) ||
			si_setcore(bus->sih, ARMCA7_CORE_ID, 0));

	bcmerror = dhd_os_get_img_fwreq(&fw, bus->fw_path);
	if (bcmerror < 0) {
		DHD_ERROR(("dhd_os_get_img(Request Firmware API) error : %d\n",
			bcmerror));
		goto err;
	}
#ifdef CHECK_DOWNLOAD_FW
	if (bus->dhd->conf->fwchk) {
		memptr_tmp = MALLOC(bus->dhd->osh, MEMBLOCK + DHD_SDALIGN);
		if (memptr_tmp == NULL) {
			DHD_ERROR(("%s: Failed to allocate memory %d bytes\n", __FUNCTION__, MEMBLOCK));
			goto err;
		}
	}
#endif

	bus->fw_download_len = fw->size;
	bus->fw_download_addr = bus->dongle_ram_base;
	residual_len = fw->size;
	while (residual_len) {
		len = MIN(residual_len, MEMBLOCK);

		/* if address is 0, store the reset instruction to be written in 0 */
		if (store_reset) {
			ASSERT(offset == 0);
			bus->resetinstr = *(((const uint32*)fw->data + buf_offset));
			/* Add start of RAM address to the address given by user */
			offset += bus->dongle_ram_base;
			offset_end += offset;
			store_reset = FALSE;
		}

		/* Ignore compiler warnings due to -Werror=cast-qual */
		GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
		bcmerror = dhdpcie_bus_membytes(bus, TRUE, offset,
			(uint8 *)fw->data + buf_offset, len);
		GCC_DIAGNOSTIC_POP();

		if (bcmerror) {
			DHD_ERROR(("%s: error %d on writing %d membytes at 0x%08x\n",
				__FUNCTION__, bcmerror, MEMBLOCK, offset));
			goto err;
		}
#ifdef CHECK_DOWNLOAD_FW
		if (bus->dhd->conf->fwchk) {
			bcmerror = dhdpcie_bus_membytes(bus, FALSE, offset, memptr_tmp, len);
			if (bcmerror) {
				DHD_ERROR(("%s: error %d on reading %d membytes at 0x%08x\n",
				        __FUNCTION__, bcmerror, MEMBLOCK, offset));
				goto err;
			}
			if (memcmp(memptr_tmp, (uint8 *)fw->data + buf_offset, len)) {
				DHD_ERROR(("%s: Downloaded image is corrupted at 0x%08x\n", __FUNCTION__, offset));
				bcmerror = BCME_ERROR;
				goto err;
			} else
				DHD_INFO(("%s: Download, Upload and compare succeeded.\n", __FUNCTION__));
		}
#endif
		offset += MEMBLOCK;

		if (offset >= offset_end) {
			DHD_ERROR(("%s: invalid address access to %x (offset end: %x)\n",
				__FUNCTION__, offset, offset_end));
			bcmerror = BCME_ERROR;
			goto err;
		}
		residual_len -= len;
		buf_offset += len;
	}
err:
#ifdef CHECK_DOWNLOAD_FW
	if (memptr_tmp)
		MFREE(bus->dhd->osh, memptr_tmp, MEMBLOCK + DHD_SDALIGN);
#endif
	if (fw) {
		dhd_os_close_img_fwreq(fw);
	}
	return bcmerror;
} /* dhdpcie_download_code_file */

#else

/**
 * Downloads a file containing firmware into dongle memory. In case of a .bea file, the DHD
 * is updated with the event logging partitions within that file as well.
 *
 * @param pfw_path    Path to .bin or .bea file
 */
static int
dhdpcie_download_code_file(struct dhd_bus *bus, char *pfw_path)
{
	int bcmerror = BCME_ERROR;
	int offset = 0;
	int len = 0;
	bool store_reset;
	void *filep = NULL;
	uint8 *memblock = NULL, *memptr = NULL;
#ifdef CHECK_DOWNLOAD_FW
	uint8 *memptr_tmp = NULL; // terence: check downloaded firmware is correct
#endif
	int offset_end = bus->ramsize;
	uint32 file_size = 0, read_len = 0;
	fwpkg_info_t *fwpkg;

#if defined(CACHE_FW_IMAGES)
	int buf_offset, total_len, residual_len;
	char * dnld_buf = NULL;
#endif /* CACHE_FW_IMAGES */

#if defined(linux) || defined(LINUX)
#if defined(DHD_FW_MEM_CORRUPTION)
	if (dhd_bus_get_fw_mode(bus->dhd) == DHD_FLAG_MFG_MODE) {
		dhd_tcm_test_enable = TRUE;
	} else {
		dhd_tcm_test_enable = FALSE;
	}
#endif /* DHD_FW_MEM_CORRUPTION */
	DHD_ERROR(("%s: dhd_tcm_test_enable %u, dhd_tcm_test_status %u\n", __FUNCTION__,
		dhd_tcm_test_enable, dhd_tcm_test_status));
	/* run TCM test if not passed yet */
	if (dhd_tcm_test_enable && dhd_tcm_test_status != TCM_TEST_PASSED) {
		if (dhd_bus_tcm_test(bus) == FALSE) {
			DHD_ERROR(("dhd_bus_tcm_test failed\n"));
			dhd_tcm_test_status = TCM_TEST_FAILED;
			bcmerror = BCME_ERROR;
			goto err;
		} else {
			dhd_tcm_test_status = TCM_TEST_PASSED;
		}
	}
#endif /* LINUX || linux */
	DHD_ERROR_MEM(("%s: download firmware %s\n", __FUNCTION__, pfw_path));

	bcmerror = fwpkg_init(&bus->fwpkg, pfw_path);
	if (bcmerror == BCME_ERROR) {
		printf("%s: fwpkg_init failed %s\n", __FUNCTION__, pfw_path);
		goto err;
	}
	fwpkg = &bus->fwpkg;
	/* Should succeed in opening image if it is actually given through registry
	 * entry or in module param.
	 */
	bcmerror = fwpkg_open_firmware_img(fwpkg, pfw_path, &filep);
	if (bcmerror == BCME_ERROR) {
		printf("%s: Open firmware file failed %s\n", __FUNCTION__, pfw_path);
		goto err;
	}

	if (bcmerror == BCME_UNSUPPORTED) {
		file_size = fwpkg->file_size;
		DHD_ERROR(("%s Using SINGLE image (size %d)\n",
			__FUNCTION__, file_size));
	} else {
		file_size = fwpkg_get_firmware_img_size(fwpkg);
		strlcpy(bus->fwsig_filename, pfw_path, sizeof(bus->fwsig_filename));
		DHD_ERROR(("%s Using COMBINED image (size %d)\n",
			__FUNCTION__, file_size));
	}
	bus->fw_download_len = file_size;
	bus->fw_download_addr = bus->dongle_ram_base;

	memptr = memblock = MALLOC(bus->dhd->osh, MEMBLOCK + DHD_SDALIGN);
	if (memblock == NULL) {
		DHD_ERROR(("%s: Failed to allocate memory %d bytes\n", __FUNCTION__, MEMBLOCK));
		bcmerror = BCME_NOMEM;
		goto err;
	}
#ifdef CHECK_DOWNLOAD_FW
	if (bus->dhd->conf->fwchk) {
		memptr_tmp = MALLOC(bus->dhd->osh, MEMBLOCK + DHD_SDALIGN);
		if (memptr_tmp == NULL) {
			DHD_ERROR(("%s: Failed to allocate memory %d bytes\n", __FUNCTION__, MEMBLOCK));
			goto err;
		}
	}
#endif
	if ((uint32)(uintptr)memblock % DHD_SDALIGN) {
		memptr += (DHD_SDALIGN - ((uint32)(uintptr)memblock % DHD_SDALIGN));
	}

	/* check if CR4/CA7 */
	store_reset = (si_setcore(bus->sih, ARMCR4_CORE_ID, 0) ||
			si_setcore(bus->sih, ARMCA7_CORE_ID, 0));
#if defined(CACHE_FW_IMAGES)
	total_len = bus->ramsize;
	dhd_os_close_image(filep);
	filep = NULL;
	buf_offset = 0;
	bcmerror = dhd_get_download_buffer(bus->dhd, pfw_path, FW, &dnld_buf, &total_len);
	if (bcmerror != BCME_OK) {
		DHD_ERROR(("%s: dhd_get_download_buffer failed (%d)\n", __FUNCTION__, bcmerror));
		goto err;
	}
	residual_len = total_len;
	/* Download image with MEMBLOCK size */
	while (residual_len)
#else
	/* Download image with MEMBLOCK size */
	while ((len = dhd_os_get_image_block((char*)memptr, MEMBLOCK, filep)))
#endif /* CACHE_FW_IMAGES */
	{
#if defined(CACHE_FW_IMAGES)
		len = MIN(residual_len, MEMBLOCK);
		memcpy(memptr, dnld_buf + buf_offset, len);
		residual_len -= len;
		buf_offset += len;
#else
		if (len < 0) {
			DHD_ERROR(("%s: dhd_os_get_image_block failed (%d)\n", __FUNCTION__, len));
			bcmerror = BCME_ERROR;
			goto err;
		}
#endif /* CACHE_FW_IMAGES */

		read_len += len;
		if (read_len > file_size) {
			DHD_ERROR(("%s: WARNING! reading beyond EOF, len=%d; read_len=%u;"
				" file_size=%u truncating len to %d \n", __FUNCTION__,
				len, read_len, file_size, (len - (read_len - file_size))));
			len -= (read_len - file_size);
		}

		/* if address is 0, store the reset instruction to be written in 0 */
		if (store_reset) {
			ASSERT(offset == 0);
			bus->resetinstr = *(((uint32*)memptr));
			/* Add start of RAM address to the address given by user */
			offset += bus->dongle_ram_base;
			offset_end += offset;
			store_reset = FALSE;
		}

		bcmerror = dhdpcie_bus_membytes(bus, TRUE, offset, (uint8 *)memptr, len);
		if (bcmerror) {
			DHD_ERROR(("%s: error %d on writing %d membytes at 0x%08x\n",
				__FUNCTION__, bcmerror, MEMBLOCK, offset));
			goto err;
		}

#ifdef CHECK_DOWNLOAD_FW
		if (bus->dhd->conf->fwchk) {
			bcmerror = dhdpcie_bus_membytes(bus, FALSE, offset, memptr_tmp, len);
			if (bcmerror) {
				DHD_ERROR(("%s: error %d on reading %d membytes at 0x%08x\n",
				        __FUNCTION__, bcmerror, MEMBLOCK, offset));
				goto err;
			}
			if (memcmp(memptr_tmp, memptr, len)) {
				DHD_ERROR(("%s: Downloaded image is corrupted at 0x%08x\n", __FUNCTION__, offset));
				bcmerror = BCME_ERROR;
				goto err;
			} else
				DHD_INFO(("%s: Download, Upload and compare succeeded.\n", __FUNCTION__));
		}
#endif
		offset += MEMBLOCK;

		if (offset >= offset_end) {
			DHD_ERROR(("%s: invalid address access to %x (offset end: %x)\n",
				__FUNCTION__, offset, offset_end));
			bcmerror = BCME_ERROR;
			goto err;
		}

		if (read_len >= file_size) {
			break;
		}
	}
#if defined(DHD_DEBUG) && defined(DHD_FW_MEM_CORRUPTION)
	/* Upload and compare the downloaded code */
	{
		unsigned char *ulblock = NULL;
		unsigned int uploded_len;
		read_len = 0;
		uploded_len = 0;
		bcmerror = BCME_ERROR;
		ulblock = MALLOC(bus->dhd->osh, MEMBLOCK);
		if (ulblock == NULL)
			goto upload_err;
		/* Close and re-open the image file to reset the file pointer.
		*/
		dhd_os_close_image1(bus->dhd, imgbuf);
		imgbuf = dhd_os_open_image1(bus->dhd, pfw_path);
		if (imgbuf == NULL) {
			goto upload_err;
		}
		/* Upload image to verify downloaded contents. */
		offset = bus->dongle_ram_base;
		offset_end = offset + bus->ramsize;
		/* Upload image with MEMBLOCK size */
		while ((len = dhd_os_get_image_block((char*)memptr, MEMBLOCK, imgbuf))) {
			if (len < 0) {
				DHD_ERROR(("%s: dhd_os_get_image_block failed (%d)\n", __FUNCTION__, len));
				bcmerror = BCME_ERROR;
				goto upload_err;
			}

			read_len += len;
			if (read_len > file_size) {
				DHD_ERROR(("%s: WARNING! reading beyond EOF, len=%d; read_len=%u;"
					" file_size=%u truncating len to %d \n", __FUNCTION__,
					len, read_len, file_size, (len - (read_len - file_size))));
				len -= (read_len - file_size);
			}
			memset(ulblock, 0xaa, MEMBLOCK);
			bcmerror = dhdpcie_bus_membytes(bus, FALSE, offset,
				(uint8 *)ulblock, len);
			if (bcmerror) {
				DHD_ERROR(("%s: error %d on reading %d membytes at 0x%08x\n",
					__FUNCTION__, bcmerror, len, offset));
				goto upload_err;
			}
			if (memcmp(memptr, ulblock, len)) {
				DHD_ERROR(("%s: Downloaded image is corrupted! \n",
					__FUNCTION__));
				bcmerror = BCME_ERROR;
				goto upload_err;
			}

			uploded_len += len;
			offset += MEMBLOCK;

			if (offset >= offset_end) {
				DHD_ERROR(("%s: invalid address access to %x (offset end: %x)\n",
					__FUNCTION__, offset, offset_end));
				bcmerror = BCME_ERROR;
				goto upload_err;
			}

			if (read_len >= file_size) {
				break;
			}
		}
		DHD_ERROR(("%s: Download, Upload and compare succeeded.\n", __FUNCTION__));

upload_err:
		if (ulblock)
			MFREE(bus->dhd->osh, ulblock, MEMBLOCK);
	}
#endif /* DHD_DEBUG && DHD_FW_MEM_CORRUPTION */
err:
	if (memblock) {
		MFREE(bus->dhd->osh, memblock, MEMBLOCK + DHD_SDALIGN);
#ifdef CHECK_DOWNLOAD_FW
		if (memptr_tmp)
			MFREE(bus->dhd->osh, memptr_tmp, MEMBLOCK + DHD_SDALIGN);
#endif
	}
#if defined(CACHE_FW_IMAGES)
	if (dnld_buf) {
		dhd_free_download_buffer(bus->dhd, dnld_buf, total_len);
	}
#endif /* CACHE_FW_IMAGES */

	if (filep) {
		dhd_os_close_image1(bus->dhd, filep);
	}

	return bcmerror;
} /* dhdpcie_download_code_file */
#endif /* DHD_LINUX_STD_FW_API */

static int
dhdpcie_download_nvram(struct dhd_bus *bus)
{
	int bcmerror = BCME_ERROR;
	uint len, memblock_len = 0;
	char * memblock = NULL;
	char *bufp;
	char *pnv_path;
	bool nvram_file_exists;
	bool nvram_uefi_exists = FALSE;
	bool local_alloc = FALSE;
#ifdef SUPPORT_OTA_UPDATE
	ota_update_info_t *ota_info = &bus->dhd->ota_update_info;
#endif /* SUPPORT_OTA_UPDATE */
	pnv_path = bus->nv_path;

	nvram_file_exists = ((pnv_path != NULL) && (pnv_path[0] != '\0'));

	/* First try UEFI */
	len = MAX_NVRAMBUF_SIZE;
	bcmerror = dhd_get_download_buffer(bus->dhd, NULL, NVRAM, &memblock, (int *)&len);

	/* If UEFI empty, then read from file system */
	if ((len <= 0) || (memblock == NULL)) {

		if (nvram_file_exists) {
			len = MAX_NVRAMBUF_SIZE;
			bcmerror = dhd_get_download_buffer(bus->dhd, pnv_path, NVRAM, &memblock,
					(int *)&len);
			if ((len <= 0 || len > MAX_NVRAMBUF_SIZE)) {
				goto err;
			}
		}
#ifdef BCM_ROUTER_DHD
		else if (bus->nvram_params_len) {
			memblock = MALLOCZ(bus->dhd->osh, MAX_NVRAMBUF_SIZE);
			if (memblock == NULL) {
				DHD_ERROR(("%s: Failed to allocate memory %d bytes\n",
					__FUNCTION__, MAX_NVRAMBUF_SIZE));
				goto err;
			}
			local_alloc = TRUE;
			/* nvram is string with null terminated. cannot use strlen */
			len = bus->nvram_params_len;
			ASSERT(len <= MAX_NVRAMBUF_SIZE);
			memcpy(memblock, bus->nvram_params, len);
		}
#endif /* BCM_ROUTER_DHD */
		else {
			/* For SROM OTP no external file or UEFI required */
			bcmerror = BCME_OK;
		}
	} else {
		nvram_uefi_exists = TRUE;
	}
	memblock_len = MAX_NVRAMBUF_SIZE;

	DHD_ERROR_MEM(("%s: dhd_get_download_buffer len %d\n", __FUNCTION__, len));

	if (len > 0 && len < MAX_NVRAMBUF_SIZE && memblock != NULL) {
		bufp = (char *) memblock;

#ifdef CACHE_FW_IMAGES
		if (bus->processed_nvram_params_len) {
			len = bus->processed_nvram_params_len;
		}

		if (!bus->processed_nvram_params_len) {
			bufp[len-1] = 0;
			if (nvram_uefi_exists || nvram_file_exists) {
				len = process_nvram_vars(bufp, len);
				bus->processed_nvram_params_len = len;
			}
		} else
#else
		{
			bufp[len-1] = 0;
			if (nvram_uefi_exists || nvram_file_exists) {
				len = process_nvram_vars(bufp, len);
			}
		}
#endif /* CACHE_FW_IMAGES */

		DHD_ERROR_MEM(("%s: process_nvram_vars len %d\n", __FUNCTION__, len));

		if (len % 4) {
			len += 4 - (len % 4);
		}
		bufp += len;
		*bufp++ = 0;
		if (len)
			bcmerror = dhdpcie_downloadvars(bus, memblock, len + 1);
		if (bcmerror) {
			DHD_ERROR(("%s: error downloading vars: %d\n",
				__FUNCTION__, bcmerror));
		}
	}

err:
	if (memblock) {
#ifdef SUPPORT_OTA_UPDATE
		if (ota_info->nvram_len) {
			MFREE(bus->dhd->osh, ota_info->nvram_buf, ota_info->nvram_len);
			ota_info->nvram_len = 0;
		}
		else
#endif /* SUPPORT_OTA_UPDATE */
		{
			if (local_alloc) {
				MFREE(bus->dhd->osh, memblock, MAX_NVRAMBUF_SIZE);
			} else {
				dhd_free_download_buffer(bus->dhd, memblock, memblock_len);
			}
		}
	}

	return bcmerror;
}

#if defined(BCMEMBEDIMAGE) && defined(BCM_ROUTER_DHD)

#ifdef DLIMAGE_43602a1
#define CHIPID_43602            BCM43602_CHIP_ID
#define CHIPID_43462            BCM43462_CHIP_ID
#define CHIPID_43522            BCM43522_CHIP_ID
#define CHIP_43602_CHIPREV_A0   0
#define CHIP_43602_CHIPREV_A1   1
#define CHIP_43602_PKG_OPT      1
#endif

#define CHIPID_NONE            -1

struct fd_chip_image
{
	unsigned char *dlarray;
	int dlimagesize;
	char *dlimagename;
	char *dlimagever;
	char *dliamgedate;
} static chip_dl_image_array[] __initdata =
{
#ifdef DLIMAGE_43602a1
	{dlarray_43602a1, sizeof(dlarray_43602a1), dlimagename_43602a1,
	dlimagever_43602a1, dlimagedate_43602a1},
#endif
	/* {image attributes for other chips, only if image is compiled} */
};

enum chip_image_rev
{
#ifdef DLIMAGE_43602a1
	CHIP_43602_A1_CHIP_IMAGE,
#endif
	/* index in the above array */
};

struct chip_image_map
{
	uint32 chipid;
	uint32 chiprev;
	uint32 chippkg;
	uint32 image_idx;
} static chip_image_index_map_table [] __initdata  =
{
#ifdef DLIMAGE_43602a1
	{CHIPID_43602, CHIP_43602_CHIPREV_A1, CHIP_43602_PKG_OPT, CHIP_43602_A1_CHIP_IMAGE},
	{CHIPID_43462, CHIP_43602_CHIPREV_A1, CHIP_43602_PKG_OPT, CHIP_43602_A1_CHIP_IMAGE},
	{CHIPID_43522, CHIP_43602_CHIPREV_A1, CHIP_43602_PKG_OPT, CHIP_43602_A1_CHIP_IMAGE},
#endif
	/* {for a given chipid, chiprev, chippkg, what is the index (the above enum) */
	{CHIPID_NONE, 0, 0, 0} /* CHIPID_NONE is -1, used to mark end of list */
};

static void  __init select_fd_image(
		struct dhd_bus *bus, unsigned char **p_dlarray,
		char **p_dlimagename, char **p_dlimagever,
		char **p_dlimagedate, int *image_size) {

	uint32 chipid, chiprev, chippkg_opt;
	int image_index;
	struct chip_image_map *p_image_index;

	chipid = 0;
	image_index = -1;
	p_image_index = &chip_image_index_map_table[0];
	while (chipid != CHIPID_NONE) {
		chipid = p_image_index->chipid;
		chiprev = p_image_index->chiprev;
		chippkg_opt = p_image_index->chippkg;

		if ((chipid == bus->sih->chip) && (chiprev == bus->sih->chiprev) &&
			(chippkg_opt == bus->sih->chippkg)) {
			image_index = p_image_index->image_idx;
			break;
		}
		p_image_index++;
	}

	if (image_index != -1) {
		*p_dlarray     = chip_dl_image_array[image_index].dlarray;
		*p_dlimagename = chip_dl_image_array[image_index].dlimagename;
		*p_dlimagever  = chip_dl_image_array[image_index].dlimagever;
		*p_dlimagedate = chip_dl_image_array[image_index].dliamgedate;
		*image_size    = chip_dl_image_array[image_index].dlimagesize;
	} else {
		*p_dlarray     = 0;
		DHD_ERROR(("####################################################################\n"
			"# %s: Dongle image not available for chipid = 0x%x"
			"  chiprev = %d  chippkg = %d\n"
			"####################################################################\n",
			__FUNCTION__, bus->sih->chip, bus->sih->chiprev, bus->sih->chippkg));
	}
}
#endif /* defined(BCMEMBEDIMAGE) && defined (BCM_ROUTER_DHD) */

#ifdef BCMEMBEDIMAGE
int
dhdpcie_download_code_array(struct dhd_bus *bus)
{
	int bcmerror = -1;
	int offset = 0;
	unsigned char *p_dlarray  = NULL;
	unsigned int dlarray_size = 0;
	unsigned int downloded_len, remaining_len, len;
	char *p_dlimagename, *p_dlimagever, *p_dlimagedate;
	uint8 *memblock = NULL, *memptr;

	downloded_len = 0;
	remaining_len = 0;
	len = 0;

#ifndef BCM_ROUTER_DHD
	p_dlarray = dlarray;
	dlarray_size = sizeof(dlarray);
	p_dlimagename = dlimagename;
	p_dlimagever  = dlimagever;
	p_dlimagedate = dlimagedate;
#else
	select_fd_image(bus, &p_dlarray, &p_dlimagename, &p_dlimagever,
		&p_dlimagedate, &dlarray_size);
#endif /* endif for BCM_ROUTER_DHD */

	if ((p_dlarray == 0) ||	(dlarray_size == 0) ||(dlarray_size > bus->ramsize) ||
		(p_dlimagename == 0) ||	(p_dlimagever  == 0) ||	(p_dlimagedate == 0))
		goto err;

	memptr = memblock = MALLOC(bus->dhd->osh, MEMBLOCK + DHD_SDALIGN);
	if (memblock == NULL) {
		DHD_ERROR(("%s: Failed to allocate memory %d bytes\n", __FUNCTION__, MEMBLOCK));
		goto err;
	}
	if ((uint32)(uintptr)memblock % DHD_SDALIGN)
		memptr += (DHD_SDALIGN - ((uint32)(uintptr)memblock % DHD_SDALIGN));

	while (downloded_len  < dlarray_size) {
		remaining_len = dlarray_size - downloded_len;
		if (remaining_len >= MEMBLOCK)
			len = MEMBLOCK;
		else
			len = remaining_len;

		memcpy(memptr, (p_dlarray + downloded_len), len);
		/* check if CR4/CA7 */
		if (si_setcore(bus->sih, ARMCR4_CORE_ID, 0) ||
			si_setcore(bus->sih, SYSMEM_CORE_ID, 0)) {
			/* if address is 0, store the reset instruction to be written in 0 */
			if (offset == 0) {
				bus->resetinstr = *(((uint32*)memptr));
				/* Add start of RAM address to the address given by user */
				offset += bus->dongle_ram_base;
			}
		}
		bcmerror = dhdpcie_bus_membytes(bus, TRUE, offset, (uint8 *)memptr, len);
		downloded_len += len;
		if (bcmerror) {
			DHD_ERROR(("%s: error %d on writing %d membytes at 0x%08x\n",
				__FUNCTION__, bcmerror, MEMBLOCK, offset));
			goto err;
		}
		offset += MEMBLOCK;
	}

#ifdef DHD_DEBUG
	/* Upload and compare the downloaded code */
	{
		unsigned char *ularray = NULL;
		unsigned int uploded_len;
		uploded_len = 0;
		bcmerror = -1;
		ularray = MALLOC(bus->dhd->osh, dlarray_size);
		if (ularray == NULL)
			goto upload_err;
		/* Upload image to verify downloaded contents. */
		offset = bus->dongle_ram_base;
		memset(ularray, 0xaa, dlarray_size);
		while (uploded_len  < dlarray_size) {
			remaining_len = dlarray_size - uploded_len;
			if (remaining_len >= MEMBLOCK)
				len = MEMBLOCK;
			else
				len = remaining_len;
			bcmerror = dhdpcie_bus_membytes(bus, FALSE, offset,
				(uint8 *)(ularray + uploded_len), len);
			if (bcmerror) {
				DHD_ERROR(("%s: error %d on reading %d membytes at 0x%08x\n",
					__FUNCTION__, bcmerror, MEMBLOCK, offset));
				goto upload_err;
			}

			uploded_len += len;
			offset += MEMBLOCK;
		}
		if (memcmp(p_dlarray, ularray, dlarray_size)) {
			DHD_ERROR(("%s: Downloaded image is corrupted (%s, %s, %s).\n",
				__FUNCTION__, p_dlimagename, p_dlimagever, p_dlimagedate));
			goto upload_err;

		} else
			DHD_ERROR(("%s: Download, Upload and compare succeeded (%s, %s, %s).\n",
				__FUNCTION__, p_dlimagename, p_dlimagever, p_dlimagedate));

upload_err:
		if (ularray)
			MFREE(bus->dhd->osh, ularray, dlarray_size);
	}
#endif /* DHD_DEBUG */
err:

	if (memblock)
		MFREE(bus->dhd->osh, memblock, MEMBLOCK + DHD_SDALIGN);

	return bcmerror;
} /* dhdpcie_download_code_array */
#endif /* BCMEMBEDIMAGE */

#ifdef BCM_ROUTER_DHD
static int
_dhdpcie_get_nvram_params(struct dhd_bus *bus)
{
	int nvram_len = MAX_NVRAMBUF_SIZE;
	int tmp_nvram_len, boardrev_str_len;
	char *boardrev_str;
	char *boardtype_str;
	char *ptr;

	bus->nvram_params = MALLOC(bus->dhd->osh, nvram_len);
	if (!bus->nvram_params) {
		DHD_ERROR(("%s: fail to get nvram buffer to download.\n", __FUNCTION__));
		return -1;
	}

	bus->nvram_params[0] = 0;
	ptr = bus->nvram_params;
	/*
	 * For full dongle router platforms, we would have two dhd instances running,
	 * serving two radios, one for 5G and another for 2G. But, both dongle instances
	 * would come up as wl0, as one is not aware of the other. In order to avoid
	 * this situation, we pass the dhd instance number through nvram parameter
	 * wlunit=0 and wlunit=1 to the dongle and make sure the two dongle instances
	 * come up as wl0 and wl1.
	 */

	tmp_nvram_len = strlen("wlunit=xx\n\n") + 1;
	tmp_nvram_len =
		snprintf(ptr, tmp_nvram_len, "wlunit=%d", dhd_get_instance(bus->dhd));
	ptr += (tmp_nvram_len + 1); /* leave NULL */
	tmp_nvram_len++;

	if ((boardrev_str = si_getdevpathvar(bus->sih, "boardrev")) == NULL)
		boardrev_str = nvram_get("boardrev");

	boardrev_str_len = strlen("boardrev=0xXXXX") + 1;
	boardrev_str_len = snprintf(ptr, boardrev_str_len, "boardrev=%s",
		boardrev_str? boardrev_str : BOARDREV_PROMOTABLE_STR);
	ptr += (boardrev_str_len + 1); /* leave NULL */
	tmp_nvram_len += (boardrev_str_len + 1);

	/* If per device boardtype is not available, use global boardtype */
	if ((boardtype_str = si_getdevpathvar(bus->sih, "boardtype")) == NULL) {
		if ((boardtype_str = nvram_get("boardtype")) != NULL) {
			int boardtype_str_len = 0;

			boardtype_str_len = strlen("boardtype=0xXXXX") + 1;
			boardtype_str_len = snprintf(ptr, boardtype_str_len,
					"boardtype=%s", boardtype_str);
			ptr += (boardtype_str_len + 1); /* leave NULL */
			tmp_nvram_len += (boardtype_str_len + 1);
		}
	}

	if (dbushost_initvars_flash(bus->sih,
		bus->osh, &ptr,
		(nvram_len - tmp_nvram_len)) != 0) {
		DHD_ERROR(("%s: fail to read nvram from flash.\n", __FUNCTION__));
	}

	tmp_nvram_len = (int)(ptr - bus->nvram_params);

	bcopy(STR_END, ptr, sizeof(STR_END));
	tmp_nvram_len += sizeof(STR_END);
	bus->nvram_params_len  = tmp_nvram_len;
	return 0;
}

static void
_dhdpcie_free_nvram_params(struct dhd_bus *bus)
{
	if (bus->nvram_params) {
		MFREE(bus->dhd->osh, bus->nvram_params, MAX_NVRAMBUF_SIZE);
	}
}

/** Handler to send a signal to the dhdmonitor process to notify of firmware traps */
void
dhdpcie_handle_dongle_trap(struct dhd_bus *bus)
{
	char *failed_if;

	/* Call the bus module watchdog */
	dhd_bus_watchdog(bus->dhd);

	/* Get the failed interface name to be later used by
	 * dhd_monitor to capture the required logs
	 */
	failed_if = dhd_ifname(bus->dhd, 0);
	dhd_schedule_trap_log_dump(bus->dhd, (uint8 *)failed_if, strlen(failed_if));
}

#endif /* BCM_ROUTER_DHD */

/**
 * Downloads firmware file given by 'bus->fw_path' into PCIe dongle
 *
 * BCMEMBEDIMAGE specific:
 * If bus->fw_path is empty, or if the download of bus->fw_path failed, firmware contained in header
 * file will be used instead.
 *
 */
static int
_dhdpcie_download_firmware(struct dhd_bus *bus)
{
	int bcmerror = -1;

	bool embed = FALSE;	/* download embedded firmware */
	bool dlok = FALSE;	/* download firmware succeeded */

	/* Out immediately if no image to download */
	if ((bus->fw_path == NULL) || (bus->fw_path[0] == '\0')) {
#ifdef BCMEMBEDIMAGE
		embed = TRUE;
#else
		DHD_ERROR(("%s: no fimrware file\n", __FUNCTION__));
		return 0;
#endif
	}
#ifdef BCM_ROUTER_DHD
	if (_dhdpcie_get_nvram_params(bus) < 0) {
		DHD_ERROR(("%s: fail to get nvram from system.\n", __FUNCTION__));
		return 0;
	}
#endif
	/* Keep arm in reset */
	if (dhdpcie_bus_download_state(bus, TRUE)) {
		DHD_ERROR(("%s: error placing ARM core in reset\n", __FUNCTION__));
		goto err;
	}

	/* External image takes precedence if specified */
	if ((bus->fw_path != NULL) && (bus->fw_path[0] != '\0')) {
		if ((bcmerror = dhdpcie_download_code_file(bus, bus->fw_path))) {
			DHD_ERROR(("%s:%d dongle image file download failed\n", __FUNCTION__,
				__LINE__));
#ifdef BCMEMBEDIMAGE
			embed = TRUE;
#else
			goto err;
#endif
		} else {
			embed = FALSE;
			dlok = TRUE;
		}
	}

#ifdef BCMEMBEDIMAGE
	if (embed) {
		if (dhdpcie_download_code_array(bus)) {
			DHD_ERROR(("%s: dongle image array download failed\n", __FUNCTION__));
			goto err;
		} else {
			dlok = TRUE;
		}
	}
#else
	BCM_REFERENCE(embed);
#endif
	if (!dlok) {
		DHD_ERROR(("%s:%d dongle image download failed\n", __FUNCTION__, __LINE__));
		goto err;
	}

	/* EXAMPLE: nvram_array */
	/* If a valid nvram_arry is specified as above, it can be passed down to dongle */
	/* dhd_bus_set_nvram_params(bus, (char *)&nvram_array); */

	/* External nvram takes precedence if specified */
	if ((bcmerror = dhdpcie_download_nvram(bus))) {
		DHD_ERROR(("%s:%d dongle nvram file download failed\n", __FUNCTION__, __LINE__));
		goto err;
	}

	/* Take arm out of reset */
	if (dhdpcie_bus_download_state(bus, FALSE)) {
		DHD_ERROR(("%s: error getting out of ARM core reset\n", __FUNCTION__));
		goto err;
	}

	bcmerror = 0;

err:
#ifdef BCM_ROUTER_DHD
	_dhdpcie_free_nvram_params(bus);
#endif /* BCM_ROUTER_DHD */
	return bcmerror;
} /* _dhdpcie_download_firmware */

static int
dhdpcie_bus_readconsole(dhd_bus_t *bus)
{
	dhd_console_t *c = &bus->console;
	uint8 line[CONSOLE_LINE_MAX], ch;
	uint32 n, idx, addr;
	int rv;
	uint readlen = 0;
	uint i = 0;

	/* Don't do anything until FWREADY updates console address */
	if (bus->console_addr == 0)
		return -1;

	/* Read console log struct */
	addr = bus->console_addr + OFFSETOF(hnd_cons_t, log);

	if ((rv = dhdpcie_bus_membytes(bus, FALSE, addr, (uint8 *)&c->log, sizeof(c->log))) < 0)
		return rv;

	/* Allocate console buffer (one time only) */
	if (c->buf == NULL) {
		c->bufsize = ltoh32(c->log.buf_size);
		if ((c->buf = MALLOC(bus->dhd->osh, c->bufsize)) == NULL)
			return BCME_NOMEM;
		DHD_INFO(("conlog: bufsize=0x%x\n", c->bufsize));
	}
	idx = ltoh32(c->log.idx);

	/* Protect against corrupt value */
	if (idx > c->bufsize)
		return BCME_ERROR;

	/* Skip reading the console buffer if the index pointer has not moved */
	if (idx == c->last)
		return BCME_OK;

	DHD_INFO(("conlog: addr=0x%x, idx=0x%x, last=0x%x \n", c->log.buf,
	   idx, c->last));

	/* Read the console buffer data to a local buffer
	 * optimize and read only the portion of the buffer needed, but
	 * important to handle wrap-around. Read ptr is 'c->last',
	 * write ptr is 'idx'
	 */
	addr = ltoh32(c->log.buf);

	/* wrap around case - write ptr < read ptr */
	if (idx < c->last) {
		/* from read ptr to end of buffer */
		readlen = c->bufsize - c->last;
		if ((rv = dhdpcie_bus_membytes(bus, FALSE,
				addr + c->last, c->buf, readlen)) < 0) {
			DHD_ERROR(("conlog: read error[1] ! \n"));
			return rv;
		}
		/* from beginning of buffer to write ptr */
		if ((rv = dhdpcie_bus_membytes(bus, FALSE,
				addr, c->buf + readlen,
				idx)) < 0) {
			DHD_ERROR(("conlog: read error[2] ! \n"));
			return rv;
		}
		readlen += idx;
	} else {
		/* non-wraparound case, write ptr > read ptr */
		readlen = (uint)idx - c->last;
		if ((rv = dhdpcie_bus_membytes(bus, FALSE,
				addr + c->last, c->buf, readlen)) < 0) {
			DHD_ERROR(("conlog: read error[3] ! \n"));
			return rv;
		}
	}
	/* update read ptr */
	c->last = idx;

	/* now output the read data from the local buffer to the host console */
	while (i < readlen) {
		for (n = 0; n < CONSOLE_LINE_MAX - 2 && i < readlen; n++) {
			ch = c->buf[i];
			++i;
			if (ch == '\n')
				break;
			line[n] = ch;
		}

		if (n > 0) {
			if (line[n - 1] == '\r')
				n--;
			line[n] = 0;
			printf("CONSOLE: %s\n", line);
		}
	}

	return BCME_OK;

} /* dhdpcie_bus_readconsole */

void
dhd_bus_dump_console_buffer(dhd_bus_t *bus)
{
	uint32 n, i;
	uint32 addr;
	char *console_buffer = NULL;
	uint32 console_ptr, console_size, console_index;
	uint8 line[CONSOLE_LINE_MAX], ch;
	int rv;

	DHD_ERROR(("%s: Dump Complete Console Buffer\n", __FUNCTION__));

	if (bus->is_linkdown) {
		DHD_ERROR(("%s: Skip dump Console Buffer due to PCIe link down\n", __FUNCTION__));
		return;
	}

	addr =	bus->pcie_sh->console_addr + OFFSETOF(hnd_cons_t, log);
	if ((rv = dhdpcie_bus_membytes(bus, FALSE, addr,
		(uint8 *)&console_ptr, sizeof(console_ptr))) < 0) {
		goto exit;
	}

	addr =	bus->pcie_sh->console_addr + OFFSETOF(hnd_cons_t, log.buf_size);
	if ((rv = dhdpcie_bus_membytes(bus, FALSE, addr,
		(uint8 *)&console_size, sizeof(console_size))) < 0) {
		goto exit;
	}

	addr =	bus->pcie_sh->console_addr + OFFSETOF(hnd_cons_t, log.idx);
	if ((rv = dhdpcie_bus_membytes(bus, FALSE, addr,
		(uint8 *)&console_index, sizeof(console_index))) < 0) {
		goto exit;
	}

	console_ptr = ltoh32(console_ptr);
	console_size = ltoh32(console_size);
	console_index = ltoh32(console_index);

	if (console_size > CONSOLE_BUFFER_MAX ||
		!(console_buffer = MALLOC(bus->dhd->osh, console_size))) {
		goto exit;
	}

	if ((rv = dhdpcie_bus_membytes(bus, FALSE, console_ptr,
		(uint8 *)console_buffer, console_size)) < 0) {
		goto exit;
	}

	for (i = 0, n = 0; i < console_size; i += n + 1) {
		for (n = 0; n < CONSOLE_LINE_MAX - 2; n++) {
			ch = console_buffer[(console_index + i + n) % console_size];
			if (ch == '\n')
				break;
			line[n] = ch;
		}

		if (n > 0) {
			if (line[n - 1] == '\r')
				n--;
			line[n] = 0;
			/* Don't use DHD_ERROR macro since we print
			 * a lot of information quickly. The macro
			 * will truncate a lot of the printfs
			 */

			printf("CONSOLE: %s\n", line);
		}
	}

exit:
	if (console_buffer)
		MFREE(bus->dhd->osh, console_buffer, console_size);
	return;
}

static void dhdpcie_force_console_dump(dhd_bus_t *bus)
{
	uint32 cons_sign = 0;
	uint32 cons_addr = 0;
	uint32 val = 0;

	cons_sign = bus->dongle_ram_base + bus->ramsize - 12;

	val = LTOH32(dhdpcie_bus_rtcm32(bus, cons_sign));

	if (val == CONS_ADDR_SIGNATURE) {
		cons_addr = bus->dongle_ram_base + bus->ramsize - 8;
		val = LTOH32(dhdpcie_bus_rtcm32(bus, cons_addr));
		bus->console_addr = val;
		dhd_msg_level |= DHD_FWLOG_VAL;
		dhdpcie_bus_readconsole(bus);
	}
	return;
}

static void
dhdpcie_schedule_log_dump(dhd_bus_t *bus)
{
#if defined(DHD_DUMP_FILE_WRITE_FROM_KERNEL) && defined(DHD_LOG_DUMP)
	log_dump_type_t *flush_type;

	/* flush_type is freed at do_dhd_log_dump function */
	flush_type = MALLOCZ(bus->dhd->osh, sizeof(log_dump_type_t));
	if (flush_type) {
		*flush_type = DLD_BUF_TYPE_ALL;
		dhd_schedule_log_dump(bus->dhd, flush_type);
	} else {
		DHD_ERROR(("%s Fail to malloc flush_type\n", __FUNCTION__));
	}
#endif /* DHD_DUMP_FILE_WRITE_FROM_KERNEL && DHD_LOG_DUMP */
}

#ifdef DHD_SSSR_DUMP
extern uint fis_enab;
#endif /* DHD_SSSR_DUMP */

dhd_pcie_link_state_type_t
dhdpcie_get_link_state(dhd_bus_t *bus)
{
	uint32 intstatus;
	uint origidx;
	dhd_pcie_link_state_type_t link_state = DHD_PCIE_ALL_GOOD;
	uint32 status_cmd = dhd_pcie_config_read(bus, PCIECFGREG_STATUS_CMD, sizeof(uint32));
	if (status_cmd == (uint32) -1) {
		DHD_ERROR(("%s: pcie link down: status_cmd:0x%x\n", __FUNCTION__, status_cmd));
		link_state = DHD_PCIE_LINK_DOWN;
		bus->is_linkdown = 1;
		goto exit;
	}

	/* Check the common backplane status */
	intstatus = si_corereg(bus->sih, bus->sih->buscoreidx, bus->pcie_mailbox_int, 0, 0);
	if (intstatus == (uint32) -1) {
		DHD_ERROR(("%s: common backplane is down, intstatus:0x%x\n",
			__FUNCTION__, intstatus));
		link_state = DHD_PCIE_COMMON_BP_DOWN;
		goto exit;
	}

	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req(bus);
	}

	origidx = si_coreidx(bus->sih);

	/* Switch to SYSMEM core and check for wlan backplane status */
	if (si_setcore(bus->sih, SYSMEM_CORE_ID, 0)) {
		uint32 ioctrl = si_wrapperreg(bus->sih, AI_IOCTRL, 0, 0);
		uint32 resetctrl = si_wrapperreg(bus->sih, AI_RESETCTRL, 0, 0);
		DHD_ERROR(("%s: sysmem ioctrl:0x%x resetctrl:0x%x\n",
			__FUNCTION__, ioctrl, resetctrl));
		if (ioctrl == (uint32) -1) {
			DHD_ERROR(("%s: wlan backplane is down ioctrl:0x%x\n",
				__FUNCTION__, ioctrl));
			link_state = DHD_PCIE_WLAN_BP_DOWN;
		}
	} else {
		DHD_ERROR(("%s: Failed to find SYSMEM core!\n", __FUNCTION__));
	}

	/* Restore back to original core */
	si_setcoreidx(bus->sih, origidx);

	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req_clear(bus);
	}

exit:
	return link_state;
}

void
dhd_validate_pcie_link_cbp_wlbp(dhd_bus_t *bus)
{
	bus->link_state = dhdpcie_get_link_state(bus);

	if (bus->link_state == DHD_PCIE_ALL_GOOD) {
		dhd_bus_dump_imp_cfg_registers(bus);
		dhd_bus_dump_dar_registers(bus);
	} else if (bus->link_state == DHD_PCIE_LINK_DOWN) {
		DHD_ERROR(("%s: pcie link down\n", __FUNCTION__));
		bus->is_linkdown = 1;
	} else if (bus->link_state == DHD_PCIE_COMMON_BP_DOWN) {
		dhd_bus_dump_imp_cfg_registers(bus);
		dhd_bus_dump_dar_registers(bus);
		bus->is_linkdown = 1;
	} else if (bus->link_state == DHD_PCIE_WLAN_BP_DOWN) {
		dhd_bus_dump_imp_cfg_registers(bus);
		dhd_bus_dump_dar_registers(bus);
#if defined(DHD_FW_COREDUMP)
#ifdef DHD_SSSR_DUMP
#ifdef OEM_ANDROID
			DHD_ERROR(("%s : Set collect_sssr and fis_enab as TRUE\n", __FUNCTION__));
			bus->dhd->collect_sssr = TRUE;
			fis_enab = TRUE;
#endif /* OEM_ANDROID */
#endif /* DHD_SSSR_DUMP */
			/* save core dump or write to a file */
			if (bus->dhd->memdump_enabled) {
				bus->dhd->memdump_type = DUMP_TYPE_READ_SHM_FAIL;
				dhdpcie_mem_dump(bus);
			}
#endif /* DHD_FW_COREDUMP */
	}
	return;
}

/**
 * Opens the file given by bus->fw_path, reads part of the file into a buffer and closes the file.
 *
 * @return BCME_OK on success
 */
static int
dhdpcie_checkdied(dhd_bus_t *bus, char *data, uint size)
{
	int bcmerror = 0;
	uint msize = 512;
	char *mbuffer = NULL;
	uint maxstrlen = 256;
	char *str = NULL;
	pciedev_shared_t *local_pciedev_shared = bus->pcie_sh;
	struct bcmstrbuf strbuf;
	unsigned long flags;
	bool dongle_trap_occured = FALSE;

	DHD_TRACE(("%s: Enter\n", __FUNCTION__));

	if (DHD_NOCHECKDIED_ON() || dhd_query_bus_erros(bus->dhd)) {
		return 0;
	}

	if (data == NULL) {
		/*
		 * Called after a rx ctrl timeout. "data" is NULL.
		 * allocate memory to trace the trap or assert.
		 */
		size = msize;
		mbuffer = data = MALLOC(bus->dhd->osh, msize);

		if (mbuffer == NULL) {
			DHD_ERROR(("%s: MALLOC(%d) failed \n", __FUNCTION__, msize));
			bcmerror = BCME_NOMEM;
			goto done2;
		}
	}

	if ((str = MALLOC(bus->dhd->osh, maxstrlen)) == NULL) {
		DHD_ERROR(("%s: MALLOC(%d) failed \n", __FUNCTION__, maxstrlen));
		bcmerror = BCME_NOMEM;
		goto done2;
	}
	DHD_GENERAL_LOCK(bus->dhd, flags);
	DHD_BUS_BUSY_SET_IN_CHECKDIED(bus->dhd);
	DHD_GENERAL_UNLOCK(bus->dhd, flags);

	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req(bus);
	}
	if ((bcmerror = dhdpcie_readshared(bus)) < 0) {
		goto done1;
	}

	bcm_binit(&strbuf, data, size);

	bcm_bprintf(&strbuf, "msgtrace address : 0x%08X\nconsole address  : 0x%08X\n",
	            local_pciedev_shared->msgtrace_addr, local_pciedev_shared->console_addr);

	if ((local_pciedev_shared->flags & PCIE_SHARED_ASSERT_BUILT) == 0) {
		/* NOTE: Misspelled assert is intentional - DO NOT FIX.
		 * (Avoids conflict with real asserts for programmatic parsing of output.)
		 */
		bcm_bprintf(&strbuf, "Assrt not built in dongle\n");
	}

	if ((bus->pcie_sh->flags & (PCIE_SHARED_ASSERT|PCIE_SHARED_TRAP)) == 0) {
		/* NOTE: Misspelled assert is intentional - DO NOT FIX.
		 * (Avoids conflict with real asserts for programmatic parsing of output.)
		 */
		bcm_bprintf(&strbuf, "No trap%s in dongle",
		          (bus->pcie_sh->flags & PCIE_SHARED_ASSERT_BUILT)
		          ?"/assrt" :"");
	} else {
		if (bus->pcie_sh->flags & PCIE_SHARED_ASSERT) {
			/* Download assert */
			bcm_bprintf(&strbuf, "Dongle assert");
			if (bus->pcie_sh->assert_exp_addr != 0) {
				str[0] = '\0';
				if ((bcmerror = dhdpcie_bus_membytes(bus, FALSE,
					bus->pcie_sh->assert_exp_addr,
					(uint8 *)str, maxstrlen)) < 0) {
					goto done1;
				}

				str[maxstrlen - 1] = '\0';
				bcm_bprintf(&strbuf, " expr \"%s\"", str);
			}

			if (bus->pcie_sh->assert_file_addr != 0) {
				str[0] = '\0';
				if ((bcmerror = dhdpcie_bus_membytes(bus, FALSE,
					bus->pcie_sh->assert_file_addr,
					(uint8 *)str, maxstrlen)) < 0) {
					goto done1;
				}

				str[maxstrlen - 1] = '\0';
				bcm_bprintf(&strbuf, " file \"%s\"", str);
			}

			bcm_bprintf(&strbuf, " line %d ",  bus->pcie_sh->assert_line);
		}

		if (bus->pcie_sh->flags & PCIE_SHARED_TRAP) {
			trap_t *tr = &bus->dhd->last_trap_info;
			dongle_trap_occured = TRUE;
			if ((bcmerror = dhdpcie_bus_membytes(bus, FALSE,
				bus->pcie_sh->trap_addr, (uint8*)tr, sizeof(trap_t))) < 0) {
				bus->dhd->dongle_trap_occured = TRUE;
				goto done1;
			}
			dhd_bus_dump_trap_info(bus, &strbuf);
		}
	}

	if (bus->pcie_sh->flags & (PCIE_SHARED_ASSERT | PCIE_SHARED_TRAP)) {
		DHD_ERROR(("%s: %s\n", __FUNCTION__, strbuf.origbuf));
#ifdef REPORT_FATAL_TIMEOUTS
		/**
		 * stop the timers as FW trapped
		 */
		if (dhd_stop_scan_timer(bus->dhd, FALSE, 0)) {
			DHD_ERROR(("dhd_stop_scan_timer failed\n"));
			ASSERT(0);
		}
		if (dhd_stop_bus_timer(bus->dhd)) {
			DHD_ERROR(("dhd_stop_bus_timer failed\n"));
			ASSERT(0);
		}
		if (dhd_stop_cmd_timer(bus->dhd)) {
			DHD_ERROR(("dhd_stop_cmd_timer failed\n"));
			ASSERT(0);
		}
		if (dhd_stop_join_timer(bus->dhd)) {
			DHD_ERROR(("dhd_stop_join_timer failed\n"));
			ASSERT(0);
		}
#endif /* REPORT_FATAL_TIMEOUTS */

		/* wake up IOCTL wait event */
		dhd_wakeup_ioctl_event(bus->dhd, IOCTL_RETURN_ON_TRAP);

		dhd_bus_dump_console_buffer(bus);
		dhd_prot_debug_info_print(bus->dhd);

#ifdef DHD_FW_COREDUMP
#ifdef OEM_ANDROID
		if (bus->dhd->up == 0) {
			DHD_ERROR(("%s: socram will be collected in dhd_net_bus_devreset failure\n",
				__FUNCTION__));
			bus->dhd->dongle_trap_during_wifi_onoff = 1;
		} else
#endif /* OEM_ANDROID */
		{
			/* save core dump or write to a file */
			if (bus->dhd->memdump_enabled) {
#ifdef DHD_SSSR_DUMP
				DHD_ERROR(("%s : Set collect_sssr as TRUE\n", __FUNCTION__));
				bus->dhd->collect_sssr = TRUE;
#endif /* DHD_SSSR_DUMP */
#ifdef DHD_SDTC_ETB_DUMP
				DHD_ERROR(("%s : Set collect_sdtc as TRUE\n", __FUNCTION__));
				bus->dhd->collect_sdtc = TRUE;
#endif /* DHD_SDTC_ETB_DUMP */
				bus->dhd->memdump_type = DUMP_TYPE_DONGLE_TRAP;
				dhdpcie_mem_dump(bus);
			}
		}
#endif /* DHD_FW_COREDUMP */

		/* set the trap occured flag only after all the memdump,
		* logdump and sssr dump collection has been scheduled
		*/
		if (dongle_trap_occured) {
			bus->dhd->dongle_trap_occured = TRUE;
			if (bus->dhd->check_trap_rot &&
				bus->dhd->ext_trap_data_supported &&
				bus->pcie_sh->flags2 & PCIE_SHARED2_ETD_ADDR_SUPPORT) {
				uint32 trap_data = *(uint32 *)bus->dhd->extended_trap_data;
				DHD_ERROR(("%s : etd data : %x\n", __FUNCTION__, trap_data));
				if (!(trap_data & D2H_DEV_EXT_TRAP_DATA)) {
					uint32 *ext_data = bus->dhd->extended_trap_data;
					/* Skip the first word which is trap_data */
					ext_data++;
					DHD_ERROR(("Dongle trap but no etd\n"));
					if (dhdpcie_bus_membytes(bus, FALSE,
						local_pciedev_shared->etd_addr,
						(uint8 *)ext_data,
						BCMPCIE_EXT_TRAP_DATA_MAXLEN -
							sizeof(trap_data)) < 0) {
						DHD_ERROR(("Error to read etd from dongle\n"));
					}
				} else {
					DHD_ERROR(("Dongle trap with etd\n"));
				}
			}

		}

#ifdef WL_CFGVENDOR_SEND_ALERT_EVENT
		bus->dhd->alert_reason = ALERT_DONGLE_TRAP;
		dhd_os_send_alert_message(bus->dhd);
#endif /* WL_CFGVENDOR_SEND_ALERT_EVENT */
		dhd_schedule_reset(bus->dhd);

#ifdef NDIS
		/* ASSERT only if hang detection/recovery is disabled. If enabled then let
		 * windows HDR mechansim trigger FW download via surprise removal
		 */
		dhd_bus_check_died(bus);
#endif

	} else if (bus->dhd->dongle_trap_data) {
		DHD_ERROR(("%s: TRAP/ASSERT flag not set in shared flags:0x%x\n",
			__FUNCTION__, bus->pcie_sh->flags));
		bus->dhd->dongle_trap_occured = TRUE;
#ifdef DHD_FW_COREDUMP
#ifdef OEM_ANDROID
		if (bus->dhd->up == 0) {
			DHD_ERROR(("%s: socram will be collected in dhd_net_bus_devreset failure\n",
				__FUNCTION__));
		} else
#endif /* OEM_ANDROID */
		{
			/* save core dump or write to a file */
			if (bus->dhd->memdump_enabled) {
#ifdef DHD_SSSR_DUMP
				DHD_ERROR(("%s : Set collect_sssr as TRUE\n", __FUNCTION__));
				bus->dhd->collect_sssr = TRUE;
#endif /* DHD_SSSR_DUMP */
#ifdef DHD_SDTC_ETB_DUMP
				DHD_ERROR(("%s : Set collect_sdtc as TRUE\n", __FUNCTION__));
				bus->dhd->collect_sdtc = TRUE;
#endif /* DHD_SDTC_ETB_DUMP */
				bus->dhd->memdump_type = DUMP_TYPE_DONGLE_TRAP;
				dhdpcie_mem_dump(bus);
			}
		}
#endif /* DHD_FW_COREDUMP */
	}

done1:
	if (bcmerror) {
		/* dhdpcie_checkdied is invoked only when dongle has trapped
		 * or after PCIe link down..etc. so set dongle_trap_occured so that
		 * log_dump logic can rely on only one flag dongle_trap_occured.
		 */
		bus->dhd->dongle_trap_occured = TRUE;
		dhdpcie_schedule_log_dump(bus);
	}
	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req_clear(bus);
	}

	DHD_GENERAL_LOCK(bus->dhd, flags);
	DHD_BUS_BUSY_CLEAR_IN_CHECKDIED(bus->dhd);
	dhd_os_busbusy_wake(bus->dhd);
	DHD_GENERAL_UNLOCK(bus->dhd, flags);
done2:
	if (mbuffer)
		MFREE(bus->dhd->osh, mbuffer, msize);
	if (str)
		MFREE(bus->dhd->osh, str, maxstrlen);

	return bcmerror;
} /* dhdpcie_checkdied */

/* Custom copy of dhdpcie_mem_dump() that can be called at interrupt level */
void dhdpcie_mem_dump_bugcheck(dhd_bus_t *bus, uint8 *buf)
{
	int ret = 0;
	int size; /* Full mem size */
	int start; /* Start address */
	int read_size = 0; /* Read size of each iteration */
	uint8 *databuf = buf;

	if (bus == NULL) {
		return;
	}

	start = bus->dongle_ram_base;
	read_size = 4;
	/* check for dead bus */
	{
		uint test_word = 0;
		ret = dhdpcie_bus_membytes(bus, FALSE, start, (uint8*)&test_word, read_size);
		/* if read error or bus timeout */
		if (ret || (test_word == 0xFFFFFFFF)) {
			return;
		}
	}

	/* Get full mem size */
	size = bus->ramsize;
	/* Read mem content */
	while (size)
	{
		read_size = MIN(MEMBLOCK, size);
		if ((ret = dhdpcie_bus_membytes(bus, FALSE, start, databuf, read_size))) {
			return;
		}

		/* Decrement size and increment start address */
		size -= read_size;
		start += read_size;
		databuf += read_size;
	}
	bus->dhd->soc_ram = buf;
	bus->dhd->soc_ram_length = bus->ramsize;
	return;
}

#if defined(DHD_FW_COREDUMP)
static int
dhdpcie_get_mem_dump(dhd_bus_t *bus)
{
	int ret = BCME_OK;
	int size = 0;
	int start = 0;
	int read_size = 0; /* Read size of each iteration */
	uint8 *p_buf = NULL, *databuf = NULL;
#ifdef BOARD_HIKEY
	unsigned long flags_bus;
#endif /* BOARD_HIKEY */
#if defined(DHD_FILE_DUMP_EVENT) && defined(DHD_FW_COREDUMP)
	dhd_dongledump_status_t dump_status;
#endif /* DHD_FILE_DUMP_EVENT && DHD_FW_COREDUMP */

	if (!bus) {
		DHD_ERROR(("%s: bus is NULL\n", __FUNCTION__));
		ret = BCME_ERROR;
		goto exit;
	}

	if (!bus->dhd) {
		DHD_ERROR(("%s: dhd is NULL\n", __FUNCTION__));
		ret = BCME_ERROR;
		goto exit;
	}

	if (bus->link_state == DHD_PCIE_WLAN_BP_DOWN) {
		DHD_ERROR(("%s: DHD_PCIE_WLAN_BP_DOWN return success and collect only FIS if set\n",
			__FUNCTION__));
		ret = BCME_OK;
		goto exit;
	}

	size = bus->ramsize; /* Full mem size */
	start = bus->dongle_ram_base; /* Start address */

	/* Get full mem size */
	p_buf = dhd_get_fwdump_buf(bus->dhd, size);
	if (!p_buf) {
		DHD_ERROR(("%s: Out of memory (%d bytes)\n",
			__FUNCTION__, size));
		ret = BCME_ERROR;
		goto exit;
	}

#if defined(DHD_FILE_DUMP_EVENT) && defined(DHD_FW_COREDUMP)
	dump_status = dhd_get_dump_status(bus->dhd);
	if (dump_status != DUMP_IN_PROGRESS) {
		dhd_set_dump_status(bus->dhd, DUMP_IN_PROGRESS);
	}
#endif /* DHD_FILE_DUMP_EVENT && DHD_FW_COREDUMP */

	/* Read mem content */
	DHD_TRACE_HW4(("Dump dongle memory\n"));
	databuf = p_buf;

	while (size > 0) {
		read_size = MIN(MEMBLOCK, size);
#ifdef BOARD_HIKEY
		/* Hold BUS_LP_STATE_LOCK to avoid simultaneous bus access */
		DHD_BUS_LP_STATE_LOCK(bus->bus_lp_state_lock, flags_bus);
#endif /* BOARD_HIKEY */
		ret = dhdpcie_bus_membytes(bus, FALSE, start, databuf, read_size);
#ifdef BOARD_HIKEY
		DHD_BUS_LP_STATE_UNLOCK(bus->bus_lp_state_lock, flags_bus);
#endif /* BOARD_HIKEY */
		if (ret) {
			DHD_ERROR(("%s: Error membytes %d\n", __FUNCTION__, ret));
#ifdef DHD_DEBUG_UART
			bus->dhd->memdump_success = FALSE;
#endif	/* DHD_DEBUG_UART */
			break;
		}
		DHD_TRACE(("."));

		/* Decrement size and increment start address */
		size -= read_size;
		start += read_size;
		databuf += read_size;
	}
exit:
#if defined(DHD_FILE_DUMP_EVENT) && defined(DHD_FW_COREDUMP)
	if (ret != BCME_OK) {
		dhd_set_dump_status(bus->dhd, DUMP_FAILURE);
	}
#endif /* DHD_FILE_DUMP_EVENT && DHD_FW_COREDUMP */

	return ret;
}

static int
dhdpcie_mem_dump(dhd_bus_t *bus)
{
	dhd_pub_t *dhdp;
	int ret = BCME_OK;
	uint32 dhd_console_ms_prev = 0;

#ifdef GDB_PROXY
	bus->gdb_proxy_mem_dump_count++;
#endif /* GDB_PROXY */
	dhdp = bus->dhd;
	if (!dhdp) {
		DHD_ERROR(("%s: dhdp is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	dhd_console_ms_prev = dhdp->dhd_console_ms;
	if (dhd_console_ms_prev) {
		DHD_ERROR(("%s: Disabling console msgs(0x%d) before mem dump to local buf\n",
			__FUNCTION__, dhd_console_ms_prev));
		dhdp->dhd_console_ms = 0;
	}

#if defined(__linux__)
	dhd_plat_pcie_register_dump(dhdp->plat_info);
#endif /* __linux__ */

	if (DHD_BUS_CHECK_DOWN_OR_DOWN_IN_PROGRESS(dhdp)) {
		DHD_ERROR(("%s: bus is down! can't collect mem dump. \n", __FUNCTION__));
		ret = BCME_ERROR;
		goto exit;
	}

	/* Induce DB7 trap for below non-trap cases */
	switch (dhdp->memdump_type) {
		case DUMP_TYPE_RESUMED_ON_TIMEOUT:
			/* intentional fall through */
		case DUMP_TYPE_D3_ACK_TIMEOUT:
			/* intentional fall through */
		case DUMP_TYPE_PKTID_AUDIT_FAILURE:
			/* intentional fall through */
		case DUMP_TYPE_PKTID_INVALID:
			/* intentional fall through */
		case DUMP_TYPE_SCAN_TIMEOUT:
			/* intentional fall through */
		case DUMP_TYPE_SCAN_BUSY:
			/* intentional fall through */
		case DUMP_TYPE_BY_LIVELOCK:
			/* intentional fall through */
		case DUMP_TYPE_TRANS_ID_MISMATCH:
			/* intentional fall through */
		case DUMP_TYPE_IFACE_OP_FAILURE:
			/* intentional fall through */
		case DUMP_TYPE_PKTID_POOL_DEPLETED:
			/* intentional fall through */
		case DUMP_TYPE_ESCAN_SYNCID_MISMATCH:
			/* intentional fall through */
		case DUMP_TYPE_PROXD_TIMEOUT:
			/* intentional fall through */
		case DUMP_TYPE_INBAND_DEVICE_WAKE_FAILURE:
			/* intentional fall through */
		case DUMP_TYPE_INVALID_SHINFO_NRFRAGS:
			/* intentional fall through */
		case DUMP_TYPE_P2P_DISC_BUSY:
			/* intentional fall through */
			if (dhdp->db7_trap.fw_db7w_trap) {
				/* Set fw_db7w_trap_inprogress here and clear from DPC */
				dhdp->db7_trap.fw_db7w_trap_inprogress = TRUE;
				dhdpcie_fw_trap(dhdp->bus);
				OSL_DELAY(100 * 1000); // wait 100 msec
				/*
				 * Since dpc may have triggered db7, it will not pre-empt itself
				 * so if trap_data is 0, re-read trap_data before giving up
				 */
				if (dhdp->dongle_trap_data == 0) {
					dhdp->dongle_trap_data = dhd_prot_process_trapbuf(dhdp);
				}
				if (dhdp->db7_trap.fw_db7w_trap_recieved) {
					/* Collect SSSR dump for all non trap cases
					 * only after DB7 ack
					 */
#ifdef DHD_SSSR_DUMP
					DHD_ERROR(("%s : Set collect_sssr as TRUE after DB7\n",
						__FUNCTION__));
					dhdp->collect_sssr = TRUE;
#endif /* DHD_SSSR_DUMP */
#ifdef DHD_SDTC_ETB_DUMP
					DHD_ERROR(("%s : Set collect_sdtc as TRUE after DB7\n",
						__FUNCTION__));
					dhdp->collect_sdtc = TRUE;
#endif /* DHD_SDTC_ETB_DUMP */
				} else {
					DHD_ERROR(("%s : Did not receive DB7 Ack\n", __FUNCTION__));
#ifdef WBRC
					DHD_ERROR(("%s : Set do_chip_bighammer\n", __FUNCTION__));
					bus->dhd->do_chip_bighammer = TRUE;
#endif /* WBRC */
				}
				DHD_ERROR(("Function_Intstatus(0x%x)=0x%x "
					"Function_Intmask(0x%x)=0x%x\n",
					PCIFunctionIntstatus(bus->sih->buscorerev),
					si_corereg(bus->sih, bus->sih->buscoreidx,
					PCIFunctionIntstatus(bus->sih->buscorerev), 0, 0),
					PCIFunctionIntmask(bus->sih->buscorerev),
					si_corereg(bus->sih, bus->sih->buscoreidx,
					PCIFunctionIntmask(bus->sih->buscorerev), 0, 0)));
			} else {
				DHD_ERROR(("%s: DB7 Not supported!!!\n",
					__FUNCTION__));
			}
			break;
		default:
			break;
	}

#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
	if (pm_runtime_get_sync(dhd_bus_to_dev(bus)) < 0)
		return BCME_ERROR;
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

	if (dhdp->skip_memdump_map_read == FALSE) {
		ret = dhdpcie_get_mem_dump(bus);
		if (ret) {
			DHD_ERROR(("%s: failed to get mem dump, err=%d\n", __FUNCTION__, ret));
			goto exit;
		}
	} else {
		DHD_ERROR(("%s: Skipped to get mem dump, err=%d\n", __FUNCTION__, ret));
		dhdp->skip_memdump_map_read = FALSE;
	}
#ifdef DHD_DEBUG_UART
	bus->dhd->memdump_success = TRUE;
#endif	/* DHD_DEBUG_UART */

	dhd_schedule_memdump(dhdp, dhdp->soc_ram, dhdp->soc_ram_length);
	/* buf, actually soc_ram free handled in dhd_{free,clear} */

#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
	pm_runtime_mark_last_busy(dhd_bus_to_dev(bus));
	pm_runtime_put_autosuspend(dhd_bus_to_dev(bus));
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

exit:
	if (dhd_console_ms_prev) {
		DHD_ERROR(("%s: enable console msgs(0x%d) after collecting memdump to local buf\n",
			__FUNCTION__, dhd_console_ms_prev));
		dhdp->dhd_console_ms = dhd_console_ms_prev;
	}

	return ret;
}

int
dhd_bus_get_mem_dump(dhd_pub_t *dhdp)
{
	if (!dhdp) {
		DHD_ERROR(("%s: dhdp is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	return dhdpcie_get_mem_dump(dhdp->bus);
}

int
dhd_bus_mem_dump(dhd_pub_t *dhdp)
{
	dhd_bus_t *bus = dhdp->bus;
	int ret = BCME_ERROR;

	if (dhdp->busstate == DHD_BUS_DOWN) {
		DHD_ERROR(("%s bus is down\n", __FUNCTION__));
		return BCME_ERROR;
	}

	/* Try to resume if already suspended or suspend in progress */

	/* Skip if still in suspended or suspend in progress */
	if (DHD_BUS_CHECK_SUSPEND_OR_ANY_SUSPEND_IN_PROGRESS(dhdp)) {
		DHD_ERROR(("%s: bus is in suspend(%d) or suspending(0x%x) state, so skip\n",
			__FUNCTION__, dhdp->busstate, dhdp->dhd_bus_busy_state));
		return BCME_ERROR;
	}

	DHD_OS_WAKE_LOCK(dhdp);
	ret = dhdpcie_mem_dump(bus);
	DHD_OS_WAKE_UNLOCK(dhdp);
	return ret;
}
#endif	/* DHD_FW_COREDUMP */

int
dhd_socram_dump(dhd_bus_t *bus)
{
#if defined(DHD_FW_COREDUMP)
	DHD_OS_WAKE_LOCK(bus->dhd);
	dhd_bus_mem_dump(bus->dhd);
	DHD_OS_WAKE_UNLOCK(bus->dhd);
	return 0;
#else
	return -1;
#endif
}

/**
 * Transfers bytes from host to dongle using pio mode.
 * Parameter 'address' is a backplane address.
 */
static int
dhdpcie_bus_membytes(dhd_bus_t *bus, bool write, ulong address, uint8 *data, uint size)
{
	uint dsize;
	int detect_endian_flag = 0x01;
	bool little_endian;

	if (bus->is_linkdown) {
		DHD_ERROR(("%s: PCIe link was down\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req(bus);
	}
	/* Detect endianness. */
	little_endian = *(char *)&detect_endian_flag;

	/* In remap mode, adjust address beyond socram and redirect
	 * to devram at SOCDEVRAM_BP_ADDR since remap address > orig_ramsize
	 * is not backplane accessible
	 */

	/* Determine initial transfer parameters */
#ifdef DHD_SUPPORT_64BIT
	dsize = sizeof(uint64);
#else /* !DHD_SUPPORT_64BIT */
	dsize = sizeof(uint32);
#endif /* DHD_SUPPORT_64BIT */

	/* Do the transfer(s) */
	DHD_INFO(("%s: %s %d bytes in window 0x%08lx\n",
	          __FUNCTION__, (write ? "write" : "read"), size, address));
	if (write) {
		while (size) {
#ifdef DHD_SUPPORT_64BIT
			if (size >= sizeof(uint64) && little_endian &&	!(address % 8)) {
				dhdpcie_bus_wtcm64(bus, address, *((uint64 *)data));
			}
#else /* !DHD_SUPPORT_64BIT */
			if (size >= sizeof(uint32) && little_endian &&	!(address % 4)) {
				dhdpcie_bus_wtcm32(bus, address, *((uint32*)data));
			}
#endif /* DHD_SUPPORT_64BIT */
			else {
				dsize = sizeof(uint8);
				dhdpcie_bus_wtcm8(bus, address, *data);
			}

			/* Adjust for next transfer (if any) */
			if ((size -= dsize)) {
				data += dsize;
				address += dsize;
			}
		}
	} else {
		while (size) {
#ifdef DHD_SUPPORT_64BIT
			if (size >= sizeof(uint64) && little_endian &&	!(address % 8))
			{
				*(uint64 *)data = dhdpcie_bus_rtcm64(bus, address);
			}
#else /* !DHD_SUPPORT_64BIT */
			if (size >= sizeof(uint32) && little_endian &&	!(address % 4))
			{
				*(uint32 *)data = dhdpcie_bus_rtcm32(bus, address);
			}
#endif /* DHD_SUPPORT_64BIT */
			else {
				dsize = sizeof(uint8);
				*data = dhdpcie_bus_rtcm8(bus, address);
			}

			/* Adjust for next transfer (if any) */
			if ((size -= dsize) > 0) {
				data += dsize;
				address += dsize;
			}
		}
	}
	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req_clear(bus);
	}
	return BCME_OK;
} /* dhdpcie_bus_membytes */

extern bool agg_h2d_db_enab;
/**
 * Transfers one transmit (ethernet) packet that was queued in the (flow controlled) flow ring queue
 * to the (non flow controlled) flow ring.
 */
int
BCMFASTPATH(dhd_bus_schedule_queue)(struct dhd_bus  *bus, uint16 flow_id, bool txs,
	uint32 bound, bool *is_qempty)
/** XXX function name could be more descriptive, eg use 'tx' and 'flow ring' in name */
{
	flow_ring_node_t *flow_ring_node;
	int ret = BCME_OK;
#ifdef DHD_LOSSLESS_ROAMING
	dhd_pub_t *dhdp = bus->dhd;
#endif /* DHD_LOSSLESS_ROAMING */
	uint32 cnt = 0;

	DHD_INFO(("%s: flow_id is %d\n", __FUNCTION__, flow_id));

	/* ASSERT on flow_id */
	if (flow_id >= bus->max_submission_rings) {
		DHD_ERROR(("%s: flow_id is invalid %d, max %d\n", __FUNCTION__,
			flow_id, bus->max_submission_rings));
		return 0;
	}

	flow_ring_node = DHD_FLOW_RING(bus->dhd, flow_id);

	if (flow_ring_node->prot_info == NULL) {
	    DHD_ERROR((" %s : invalid flow_ring_node \n", __FUNCTION__));
	    return BCME_NOTREADY;
	}

#ifdef DHD_LOSSLESS_ROAMING
	if ((dhdp->dequeue_prec_map & (1 << flow_ring_node->flow_info.tid)) == 0) {
		DHD_ERROR_RLMT(("%s: roam in progress, tid %d is not in precedence map 0x%x."
			" block scheduling\n",
			__FUNCTION__, flow_ring_node->flow_info.tid, dhdp->dequeue_prec_map));
		return BCME_OK;
	}
#endif /* DHD_LOSSLESS_ROAMING */

	{
		unsigned long flags;
		void *txp = NULL;
		flow_queue_t *queue;
#ifdef TPUT_MONITOR
		int pktlen;
#endif

		queue = &flow_ring_node->queue; /* queue associated with flow ring */

		DHD_FLOWRING_LOCK(flow_ring_node->lock, flags);

		if (flow_ring_node->status != FLOW_RING_STATUS_OPEN) {
			DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);
			return BCME_NOTREADY;
		}

		if (queue->len == 0) {
			DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);
			return BCME_NOTREADY;
		}

		while ((txp = dhd_flow_queue_dequeue(bus->dhd, queue)) != NULL) {
			if (bus->dhd->conf->orphan_move <= 1)
				PKTORPHAN(txp, bus->dhd->conf->tsq);

			/*
			 * Modifying the packet length caused P2P cert failures.
			 * Specifically on test cases where a packet of size 52 bytes
			 * was injected, the sniffer capture showed 62 bytes because of
			 * which the cert tests failed. So making the below change
			 * only Router specific.
			 */
#if defined(BCM_ROUTER_DHD)
			if (PKTLEN(bus->dhd->osh, txp) < (ETHER_MIN_LEN - ETHER_CRC_LEN)) {
				PKTSETLEN(bus->dhd->osh, txp, (ETHER_MIN_LEN - ETHER_CRC_LEN));
			}
#endif /* BCM_ROUTER_DHD */

#ifdef DHDTCPACK_SUPPRESS
			if (bus->dhd->tcpack_sup_mode != TCPACK_SUP_HOLD) {
				ret = dhd_tcpack_check_xmit(bus->dhd, txp);
				if (ret != BCME_OK) {
					DHD_ERROR(("%s: dhd_tcpack_check_xmit() error.\n",
						__FUNCTION__));
				}
			}
#endif /* DHDTCPACK_SUPPRESS */
			/* Attempt to transfer packet over flow ring */
			/* XXX: ifidx is wrong */
			++cnt;
#ifdef TPUT_MONITOR
			pktlen  = PKTLEN(OSH_NULL, txp);
			if ((bus->dhd->conf->data_drop_mode == TXPKT_DROP) && (pktlen > 500))
				ret = BCME_OK;
			else
#endif
			ret = dhd_prot_txdata(bus->dhd, txp, flow_ring_node->flow_info.ifindex);
			if (ret != BCME_OK) { /* may not have resources in flow ring */
				DHD_INFO(("%s: Reinserrt %d\n", __FUNCTION__, ret));
#ifdef AGG_H2D_DB
				if (agg_h2d_db_enab) {
					dhd_prot_schedule_aggregate_h2d_db(bus->dhd, flow_id);
				} else
#endif /* AGG_H2D_DB */
				{
					dhd_prot_txdata_write_flush(bus->dhd, flow_id);
				}
				/* reinsert at head */
				dhd_flow_queue_reinsert(bus->dhd, queue, txp);
				DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);

				if (is_qempty) {
					*is_qempty = FALSE;
				}
				/* If we are able to requeue back, return success */
				return BCME_OK;
			}

#ifdef DHD_MEM_STATS
			DHD_MEM_STATS_LOCK(bus->dhd->mem_stats_lock, flags);
			bus->dhd->txpath_mem += PKTLEN(bus->dhd->osh, txp);
			DHD_INFO(("%s txpath_mem: %llu PKTLEN: %d\n",
				__FUNCTION__, bus->dhd->txpath_mem, PKTLEN(bus->dhd->osh, txp)));
			DHD_MEM_STATS_UNLOCK(bus->dhd->mem_stats_lock, flags);
#endif /* DHD_MEM_STATS */

			/* check bound and break if exceeded */
			if (bound && cnt >= bound) {
				break;
			}
		}

		{
#ifdef AGG_H2D_DB
			if (agg_h2d_db_enab) {
				dhd_prot_schedule_aggregate_h2d_db(bus->dhd, flow_id);
			} else
#endif /* AGG_H2D_DB */
			{
				dhd_prot_txdata_write_flush(bus->dhd, flow_id);
			}
		}

		DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);

		if (is_qempty) {
			*is_qempty = queue->len > 0 ? FALSE : TRUE;
		}
	}

	return ret;
} /* dhd_bus_schedule_queue */

/** Sends an (ethernet) data frame (in 'txp') to the dongle. Callee disposes of txp. */
int
BCMFASTPATH(dhd_bus_txdata)(struct dhd_bus *bus, void *txp, uint8 ifidx)
{
	uint16 flowid;
#ifdef IDLE_TX_FLOW_MGMT
	uint8	node_status;
#endif /* IDLE_TX_FLOW_MGMT */
	flow_queue_t *queue;
	flow_ring_node_t *flow_ring_node;
	unsigned long flags;
	int ret = BCME_OK;
	void *txp_pend = NULL;
#if (defined(BCM_ROUTER_DHD) && defined(HNDCTF))
	void *ntxp = NULL;
	uint8 prio = PKTPRIO(txp);
#endif
	uint8 *pktdata = (uint8 *)PKTDATA(bus->dhd->osh, txp);

	if (!bus->dhd->flowid_allocator) {
		DHD_ERROR(("%s: Flow ring not intited yet  \n", __FUNCTION__));
		goto toss;
	}

	flowid = DHD_PKT_GET_FLOWID(txp);

	flow_ring_node = DHD_FLOW_RING(bus->dhd, flowid);

	DHD_TRACE(("%s: pkt flowid %d, status %d active %d\n",
		__FUNCTION__, flowid, flow_ring_node->status, flow_ring_node->active));

	DHD_FLOWRING_LOCK(flow_ring_node->lock, flags);
	if ((flowid > bus->dhd->max_tx_flowid) ||
#ifdef IDLE_TX_FLOW_MGMT
		(!flow_ring_node->active))
#else
		(!flow_ring_node->active) ||
		(flow_ring_node->status == FLOW_RING_STATUS_DELETE_PENDING) ||
		(flow_ring_node->status == FLOW_RING_STATUS_STA_FREEING))
#endif /* IDLE_TX_FLOW_MGMT */
	{
		DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);
		DHD_INFO(("%s: Dropping pkt flowid %d, status %d active %d\n",
			__FUNCTION__, flowid, flow_ring_node->status,
			flow_ring_node->active));
		ret = BCME_ERROR;
			goto toss;
	}

#ifdef IDLE_TX_FLOW_MGMT
	node_status = flow_ring_node->status;

	/* handle diffrent status states here!! */
	switch (node_status)
	{
		case FLOW_RING_STATUS_OPEN:

			if (bus->enable_idle_flowring_mgmt) {
				/* Move the node to the head of active list */
				dhd_flow_ring_move_to_active_list_head(bus, flow_ring_node);
			}
			break;

		case FLOW_RING_STATUS_SUSPENDED:
			DHD_INFO(("Need to Initiate TX Flow resume\n"));
			/* Issue resume_ring request */
			dhd_bus_flow_ring_resume_request(bus,
					flow_ring_node);
			break;

		case FLOW_RING_STATUS_CREATE_PENDING:
		case FLOW_RING_STATUS_RESUME_PENDING:
			/* Dont do anything here!! */
			DHD_INFO(("Waiting for Flow create/resume! status is %u\n",
				node_status));
			break;

		case FLOW_RING_STATUS_DELETE_PENDING:
		default:
			DHD_ERROR(("Dropping packet!! flowid %u status is %u\n",
				flowid, node_status));
			/* error here!! */
			ret = BCME_ERROR;
			DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);
			goto toss;
	}
	/* Now queue the packet */
#endif /* IDLE_TX_FLOW_MGMT */

	queue = &flow_ring_node->queue; /* queue associated with flow ring */

#if (defined(BCM_ROUTER_DHD) && defined(HNDCTF))
	FOREACH_CHAINED_PKT(txp, ntxp) {
		/* Tag the packet with flowid - Remember, only the head packet */
		/* of the chain has been tagged with the FlowID in dhd_sendpkt */
		/* Also set the priority */
		DHD_PKT_SET_FLOWID(txp, flowid);
		PKTSETPRIO(txp, prio);

		if ((ret = dhd_flow_queue_enqueue(bus->dhd, queue, txp)) != BCME_OK) {
			txp_pend = txp;
			PKTSETCLINK((txp), ntxp);
			break;
		}
	}
#else  /* !(defined(BCM_ROUTER_DHD) && defined(HNDCTF)) */
	if (dhd_match_pkt_type(bus->dhd, pktdata, (uint32)PKTLEN(bus->dhd->osh, txp))) {
		if ((ret = dhd_flow_queue_enqueue_head(bus->dhd, queue, txp)) != BCME_OK)
			txp_pend = txp;
	} else
	if ((ret = dhd_flow_queue_enqueue(bus->dhd, queue, txp)) != BCME_OK)
		txp_pend = txp;
#endif /* defined(BCM_ROUTER_DHD) && defined(HNDCTF */

	DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);

	if (flow_ring_node->status) {
		DHD_INFO(("%s: Enq pkt flowid %d, status %d active %d\n",
		    __FUNCTION__, flowid, flow_ring_node->status,
		    flow_ring_node->active));
		if (txp_pend) {
			txp = txp_pend;
			goto toss;
		}
		return BCME_OK;
	}
	ret = dhd_bus_schedule_queue(bus, flowid, FALSE, 0, NULL); /* from queue to flowring */

	/* If we have anything pending, try to push into q */
	if (txp_pend) {
		DHD_FLOWRING_LOCK(flow_ring_node->lock, flags);

#if (defined(BCM_ROUTER_DHD) && defined(HNDCTF))
		FOREACH_CHAINED_PKT(txp_pend, ntxp) {
			/* Tag the packet with flowid and set packet priority */
			DHD_PKT_SET_FLOWID(txp_pend, flowid);
			PKTSETPRIO(txp_pend, prio);

			if ((ret = dhd_flow_queue_enqueue(bus->dhd, queue, txp_pend))
				     != BCME_OK) {
				DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);
				PKTSETCLINK((txp_pend), ntxp);
				txp = txp_pend;
				goto toss;
			}
		}
#else  /* !(defined(BCM_ROUTER_DHD) && defined(HNDCTF)) */
		if (dhd_match_pkt_type(bus->dhd, pktdata, (uint32)PKTLEN(bus->dhd->osh, txp))) {
			if ((ret = dhd_flow_queue_enqueue_head(bus->dhd, queue, txp_pend)) != BCME_OK) {
				DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);
				txp = txp_pend;
				goto toss;
			}
		} else
		if ((ret = dhd_flow_queue_enqueue(bus->dhd, queue, txp_pend)) != BCME_OK) {
			DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);
			txp = txp_pend;
			goto toss;
		}
#endif /* defined(BCM_ROUTER_DHD) && defined(HNDCTF) */

		DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);
	}

	return ret;

toss:
	DHD_INFO(("%s: Toss %d\n", __FUNCTION__, ret));
	PKTCFREE(bus->dhd->osh, txp, TRUE);
	return ret;
} /* dhd_bus_txdata */

void
dhd_bus_stop_queue(struct dhd_bus *bus)
{
	dhd_txflowcontrol(bus->dhd, ALL_INTERFACES, ON);
}

void
dhd_bus_start_queue(struct dhd_bus *bus)
{
	/*
	 * Tx queue has been stopped due to resource shortage (or)
	 * bus is not in a state to turn on.
	 *
	 * Note that we try to re-start network interface only
	 * when we have enough resources, one has to first change the
	 * flag indicating we have all the resources.
	 */
	if (dhd_prot_check_tx_resource(bus->dhd)) {
		DHD_ERROR(("%s: Interface NOT started, previously stopped "
			"due to resource shortage\n", __FUNCTION__));
		return;
	}
	dhd_txflowcontrol(bus->dhd, ALL_INTERFACES, OFF);
}

/* Device console input function */
int dhd_bus_console_in(dhd_pub_t *dhd, uchar *msg, uint msglen)
{
	dhd_bus_t *bus = dhd->bus;
	uint32 addr, val;
	int rv;
#ifdef PCIE_INB_DW
	unsigned long flags = 0;
#endif /* PCIE_INB_DW */

	/* Address could be zero if CONSOLE := 0 in dongle Makefile */
	if (bus->console_addr == 0)
		return BCME_UNSUPPORTED;

	/* Don't allow input if dongle is in reset */
	if (bus->dhd->dongle_reset) {
		return BCME_NOTREADY;
	}

	/* Zero cbuf_index */
	addr = bus->console_addr + OFFSETOF(hnd_cons_t, cbuf_idx);
	/* handle difference in definition of hnd_log_t in certain branches */
	if (dhd->wlc_ver_major < 14) {
		addr -= (uint32)sizeof(uint32);
	}
	val = htol32(0);
	if ((rv = dhdpcie_bus_membytes(bus, TRUE, addr, (uint8 *)&val, sizeof(val))) < 0)
		goto done;

	/* Write message into cbuf */
	addr = bus->console_addr + OFFSETOF(hnd_cons_t, cbuf);
	/* handle difference in definition of hnd_log_t in certain branches */
	if (dhd->wlc_ver_major < 14) {
		addr -= sizeof(uint32);
	}
	if ((rv = dhdpcie_bus_membytes(bus, TRUE, addr, (uint8 *)msg, msglen)) < 0)
		goto done;

	/* Write length into vcons_in */
	addr = bus->console_addr + OFFSETOF(hnd_cons_t, vcons_in);
	val = htol32(msglen);
	if ((rv = dhdpcie_bus_membytes(bus, TRUE, addr, (uint8 *)&val, sizeof(val))) < 0)
		goto done;

#ifdef PCIE_INB_DW
	/* Use a lock to ensure this tx DEVICE_WAKE + tx H2D_HOST_CONS_INT sequence is
	 * mutually exclusive with the rx D2H_DEV_DS_ENTER_REQ + tx H2D_HOST_DS_ACK sequence.
	 */
	DHD_BUS_INB_DW_LOCK(bus->inb_lock, flags);
#endif /* PCIE_INB_DW */

	/* generate an interrupt to dongle to indicate that it needs to process cons command */
	dhdpcie_send_mb_data(bus, H2D_HOST_CONS_INT);

#ifdef PCIE_INB_DW
	DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
#endif /* PCIE_INB_DW */
done:
	return rv;
} /* dhd_bus_console_in */

/**
 * Called on frame reception, the frame was received from the dongle on interface 'ifidx' and is
 * contained in 'pkt'. Processes rx frame, forwards up the layer to netif.
 */
void
BCMFASTPATH(dhd_bus_rx_frame)(struct dhd_bus *bus, void* pkt, int ifidx, uint pkt_count)
{
	dhd_rx_frame(bus->dhd, ifidx, pkt, pkt_count, 0);
}

/* Aquire/Release bar1_switch_lock only if the chip supports bar1 switching */
#define DHD_BUS_BAR1_SWITCH_LOCK(bus, flags) \
	((bus)->bar1_switch_enab) ? DHD_BAR1_SWITCH_LOCK((bus)->bar1_switch_lock, flags) : \
		BCM_REFERENCE(flags)

#define DHD_BUS_BAR1_SWITCH_UNLOCK(bus, flags) \
	((bus)->bar1_switch_enab) ? DHD_BAR1_SWITCH_UNLOCK((bus)->bar1_switch_lock, flags) : \
		BCM_REFERENCE(flags)

/* Init/Deinit bar1_switch_lock only if the chip supports bar1 switching */
void
dhd_init_bar1_switch_lock(dhd_bus_t *bus)
{
	if (bus->bar1_switch_enab && !bus->bar1_switch_lock) {
		bus->bar1_switch_lock = osl_spin_lock_init(bus->osh);
	}
}

static void
dhd_deinit_bar1_switch_lock(dhd_bus_t *bus)
{
	if (bus->bar1_switch_enab && bus->bar1_switch_lock) {
		osl_spin_lock_deinit(bus->osh, bus->bar1_switch_lock);
		bus->bar1_switch_lock = NULL;
	}
}

/*
 * The bpwindow for any address will be lower bound of multiples of bar1_size.
 * For eg, if addr=0x938fff and bar1_size is 0x400000, then
 * address will fall in the window of 0x800000-0xbfffff, so need
 * to select bpwindow as 0x800000.
 * To achieve this mask the LSB nibbles of bar1_size of the given addr.
 */
#define DHD_BUS_BAR1_BPWIN(addr, bar1_size) \
	(uint32)((addr) & ~((bar1_size) - 1))

/**
 * dhdpcie_bar1_window_switch_enab
 *
 * Check if the chip requires BAR1 window switching based on
 * dongle_ram_base, ramsize and mapped bar1_size and sets
 * bus->bar1_switch_enab accordingly
 * @bus: dhd bus context
 *
 */
void
dhdpcie_bar1_window_switch_enab(dhd_bus_t *bus)
{
	uint32 ramstart = bus->dongle_ram_base;
	uint32 ramend = bus->dongle_ram_base + bus->ramsize - 1;
	uint32 bpwinstart = DHD_BUS_BAR1_BPWIN(ramstart, bus->bar1_size);
	uint32 bpwinend = DHD_BUS_BAR1_BPWIN(ramend, bus->bar1_size);

	bus->bar1_switch_enab = FALSE;

	/*
	 * Window switch is needed to access complete BAR1
	 * if bpwinstart and bpwinend are different
	 */
	if (bpwinstart != bpwinend) {
		bus->bar1_switch_enab = TRUE;
	}

	DHD_ERROR_MEM(("%s: bar1_switch_enab=%d ramstart=0x%x ramend=0x%x bar1_size=0x%x\n",
		__FUNCTION__, bus->bar1_switch_enab, ramstart, ramend, bus->bar1_size));
}

/**
 * dhdpcie_setbar1win
 *
 * os independendent function for setting bar1 window in order to allow
 * also set current window positon.
 *
 * @bus: dhd bus context
 * @addr: new backplane windows address for BAR1
 */
static void
dhdpcie_setbar1win(dhd_bus_t *bus, uint32 addr)
{
	OSL_PCI_WRITE_CONFIG(bus->osh, PCI_BAR1_WIN, 4, addr);
	bus->curr_bar1_win = addr;
}

/**
 * dhdpcie_bus_chkandshift_bpoffset
 *
 * Check the provided address is within the current BAR1 window,
 * if not, shift the window
 *
 * @bus: dhd bus context
 * @offset: back plane address that the caller wants to access
 *
 * Return: new offset for access
 */
static ulong
dhdpcie_bus_chkandshift_bpoffset(dhd_bus_t *bus, ulong offset)
{

	uint32 bpwin;
	/* Determine BAR1 backplane window using window size
	 * Window address mask should be ~(size - 1)
	 */
	bpwin = DHD_BUS_BAR1_BPWIN(offset, bus->bar1_size);

	if (bpwin != bus->curr_bar1_win) {
		DHD_INFO(("%s: move BAR1 window curr_bar1_win=0x%x bpwin=0x%x offset=0x%lx\n",
			__FUNCTION__, bus->curr_bar1_win, bpwin, offset));
		/* Move BAR1 window */
		dhdpcie_setbar1win(bus, bpwin);
	}

	return offset - bpwin;
}

/** 'offset' is a backplane address */
void
dhdpcie_bus_wtcm8(dhd_bus_t *bus, ulong offset, uint8 data)
{
	ulong flags = 0;

	if (bus->is_linkdown) {
		DHD_LOG_MEM(("%s: PCIe link was down\n", __FUNCTION__));
		return;
	}

	DHD_BUS_BAR1_SWITCH_LOCK(bus, flags);

	offset = dhdpcie_bus_chkandshift_bpoffset(bus, offset);

#if defined(linux) || defined(LINUX)
	W_REG(bus->dhd->osh, (volatile uint8 *)(bus->tcm + offset), data);
#else
	*(volatile uint8 *)(bus->tcm + offset) = (uint8)data;
#endif /* linux || LINUX */

	DHD_BUS_BAR1_SWITCH_UNLOCK(bus, flags);
}

void
dhdpcie_bus_wtcm16(dhd_bus_t *bus, ulong offset, uint16 data)
{
	ulong flags = 0;

	if (bus->is_linkdown) {
		DHD_LOG_MEM(("%s: PCIe link was down\n", __FUNCTION__));
		return;
	}

	DHD_BUS_BAR1_SWITCH_LOCK(bus, flags);

	offset = dhdpcie_bus_chkandshift_bpoffset(bus, offset);

#if defined(linux) || defined(LINUX)
	W_REG(bus->dhd->osh, (volatile uint16 *)(bus->tcm + offset), data);
#else
	*(volatile uint16 *)(bus->tcm + offset) = (uint16)data;
#endif /* linux || LINUX */

	DHD_BUS_BAR1_SWITCH_UNLOCK(bus, flags);
}

void
dhdpcie_bus_wtcm32(dhd_bus_t *bus, ulong offset, uint32 data)
{
	ulong flags = 0;

	if (bus->is_linkdown) {
		DHD_LOG_MEM(("%s: PCIe link was down\n", __FUNCTION__));
		return;
	}

	DHD_BUS_BAR1_SWITCH_LOCK(bus, flags);

	offset = dhdpcie_bus_chkandshift_bpoffset(bus, offset);

#if defined(linux) || defined(LINUX)
	W_REG(bus->dhd->osh, (volatile uint32 *)(bus->tcm + offset), data);
#else
	*(volatile uint32 *)(bus->tcm + offset) = (uint32)data;
#endif /* linux || LINUX */

	DHD_BUS_BAR1_SWITCH_UNLOCK(bus, flags);
}

#ifdef DHD_SUPPORT_64BIT
void
dhdpcie_bus_wtcm64(dhd_bus_t *bus, ulong offset, uint64 data)
{
	ulong flags = 0;

	if (bus->is_linkdown) {
		DHD_LOG_MEM(("%s: PCIe link was down\n", __FUNCTION__));
		return;
	}

	DHD_BUS_BAR1_SWITCH_LOCK(bus, flags);

	offset = dhdpcie_bus_chkandshift_bpoffset(bus, offset);

#if defined(linux) || defined(LINUX)
	W_REG(bus->dhd->osh, (volatile uint64 *)(bus->tcm + offset), data);
#else
	*(volatile uint64 *)(bus->tcm + offset) = (uint64)data;
#endif /* linux || LINUX */

	DHD_BUS_BAR1_SWITCH_UNLOCK(bus, flags);
}
#endif /* DHD_SUPPORT_64BIT */

uint8
dhdpcie_bus_rtcm8(dhd_bus_t *bus, ulong offset)
{
	volatile uint8 data;
	ulong flags = 0;

	if (bus->is_linkdown) {
		DHD_LOG_MEM(("%s: PCIe link was down\n", __FUNCTION__));
		data = (uint8)-1;
		return data;
	}

	DHD_BUS_BAR1_SWITCH_LOCK(bus, flags);

	offset = dhdpcie_bus_chkandshift_bpoffset(bus, offset);

#if defined(linux) || defined(LINUX)
	data = R_REG(bus->dhd->osh, (volatile uint8 *)(bus->tcm + offset));
#else
	data = *(volatile uint8 *)(bus->tcm + offset);
#endif /* linux || LINUX */

	DHD_BUS_BAR1_SWITCH_UNLOCK(bus, flags);
	return data;
}

uint16
dhdpcie_bus_rtcm16(dhd_bus_t *bus, ulong offset)
{
	volatile uint16 data;
	ulong flags = 0;

	if (bus->is_linkdown) {
		DHD_LOG_MEM(("%s: PCIe link was down\n", __FUNCTION__));
		data = (uint16)-1;
		return data;
	}

	DHD_BUS_BAR1_SWITCH_LOCK(bus, flags);

	offset = dhdpcie_bus_chkandshift_bpoffset(bus, offset);

#if (defined(linux) || defined(LINUX))
	data = R_REG(bus->dhd->osh, (volatile uint16 *)(bus->tcm + offset));
#else
	data = *(volatile uint16 *)(bus->tcm + offset);
#endif /* linux || LINUX */

	DHD_BUS_BAR1_SWITCH_UNLOCK(bus, flags);
	return data;
}

uint32
dhdpcie_bus_rtcm32(dhd_bus_t *bus, ulong offset)
{
	volatile uint32 data;
	ulong flags = 0;

	if (bus->is_linkdown) {
		DHD_LOG_MEM(("%s: PCIe link was down\n", __FUNCTION__));
		data = (uint32)-1;
		return data;
	}

	DHD_BUS_BAR1_SWITCH_LOCK(bus, flags);

	offset = dhdpcie_bus_chkandshift_bpoffset(bus, offset);

#if (defined(linux) || defined(LINUX))
	data = R_REG(bus->dhd->osh, (volatile uint32 *)(bus->tcm + offset));
#else
	data = *(volatile uint32 *)(bus->tcm + offset);
#endif /* linux || LINUX */

	DHD_BUS_BAR1_SWITCH_UNLOCK(bus, flags);
	return data;
}

#ifdef DHD_SUPPORT_64BIT
uint64
dhdpcie_bus_rtcm64(dhd_bus_t *bus, ulong offset)
{
	volatile uint64 data;
	ulong flags = 0;

	if (bus->is_linkdown) {
		DHD_LOG_MEM(("%s: PCIe link was down\n", __FUNCTION__));
		data = (uint64)-1;
		return data;
	}

	DHD_BUS_BAR1_SWITCH_LOCK(bus, flags);

	offset = dhdpcie_bus_chkandshift_bpoffset(bus, offset);

#if (defined(linux) || defined(LINUX))
	data = R_REG(bus->dhd->osh, (volatile uint64 *)(bus->tcm + offset));
#else
	data = *(volatile uint64 *)(bus->tcm + offset);
#endif /* linux || LINUX */

	DHD_BUS_BAR1_SWITCH_UNLOCK(bus, flags);
	return data;
}
#endif /* DHD_SUPPORT_64BIT */

void
dhdpcie_update_ring_ptrs_in_tcm_with_req_pwr(dhd_bus_t *bus, void *data, uint8 type,
	uint16 ringid, bool read, bool req_pwr)
{
	ulong addr;

	if (type == RING_WR_UPD) {
		addr = bus->ring_sh[ringid].ring_state_w;
	} else if (type == RING_RD_UPD) {
		addr = bus->ring_sh[ringid].ring_state_r;
	} else {
		DHD_ERROR(("%s: invalid type:%d\n", __FUNCTION__, type));
		return;
	}

	if (req_pwr && MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req(bus);
	}

	if (read) {
		/* Read */
		*(uint16*)data = LTOH16(dhdpcie_bus_rtcm16(bus, addr));
	} else {
		/* Write */
		dhdpcie_bus_wtcm16(bus, addr, (uint16) HTOL16(*(uint16 *)data));
	}

	if (req_pwr && MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req_clear(bus);
	}
}

void
dhdpcie_update_ring_ptrs_in_tcm(dhd_bus_t *bus, void *data, uint8 type,	uint16 ringid,
	bool read)
{
#ifdef PCIE_INB_DW
	if (INBAND_DW_ENAB(bus)) {
		ulong flags_ds;
		DHD_BUS_DONGLE_DS_LOCK(bus->dongle_ds_lock, flags_ds);
		dhdpcie_update_ring_ptrs_in_tcm_with_req_pwr(bus, data, type, ringid, read,
			bus->dongle_in_deepsleep);
		DHD_BUS_DONGLE_DS_UNLOCK(bus->dongle_ds_lock, flags_ds);
	} else
#endif /* PCIE_INB_DW */
	{
		/* Request power explicitly */
		dhdpcie_update_ring_ptrs_in_tcm_with_req_pwr(bus, data, type, ringid, read, TRUE);
	}
}

/** A snippet of dongle memory is shared between host and dongle */
void
dhd_bus_cmn_writeshared(dhd_bus_t *bus, void *data, uint32 len, uint8 type, uint16 ringid)
{
	uint64 long_data;
	ulong addr; /* dongle address */

	DHD_INFO(("%s: writing to dongle type %d len %d\n", __FUNCTION__, type, len));

	if (bus->is_linkdown) {
		DHD_ERROR(("%s: PCIe link was down\n", __FUNCTION__));
		return;
	}

	/*
	 * Use explicit tcm ring ptr update functions when DMA indices are not enabled to
	 * as backplane power request calls are causing TPUT drops
	 */
	if (!(bus->dhd->dma_d2h_ring_upd_support || bus->dhd->dma_h2d_ring_upd_support)) {
		if ((type == RING_WR_UPD) || (type == RING_RD_UPD)) {
			dhdpcie_update_ring_ptrs_in_tcm(bus, data, type, ringid, FALSE);
			return;
		}
	}

	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req(bus);
	}
	switch (type) {
		case RING_WR_UPD :
			addr = bus->ring_sh[ringid].ring_state_w;
			dhdpcie_bus_wtcm16(bus, addr, (uint16) HTOL16(*(uint16 *)data));
			break;

		case RING_RD_UPD :
			addr = bus->ring_sh[ringid].ring_state_r;
			dhdpcie_bus_wtcm16(bus, addr, (uint16) HTOL16(*(uint16 *)data));
			break;

		case D2H_DMA_SCRATCH_BUF:
			addr = DHD_PCIE_SHARED_MEMBER_ADDR(bus, host_dma_scratch_buffer);
			long_data = HTOL64(*(uint64 *)data);
			dhdpcie_bus_membytes(bus, TRUE, addr, (uint8*) &long_data, len);
			if (dhd_msg_level & DHD_INFO_VAL) {
				prhex(__FUNCTION__, data, len);
			}
			break;

		case D2H_DMA_SCRATCH_BUF_LEN :
			addr = DHD_PCIE_SHARED_MEMBER_ADDR(bus, host_dma_scratch_buffer_len);
			dhdpcie_bus_wtcm32(bus, addr, (uint32) HTOL32(*(uint32 *)data));
			if (dhd_msg_level & DHD_INFO_VAL) {
				prhex(__FUNCTION__, data, len);
			}
			break;

		case H2D_DMA_INDX_WR_BUF:
			long_data = HTOL64(*(uint64 *)data);
			addr = DHD_RING_INFO_MEMBER_ADDR(bus, h2d_w_idx_hostaddr);
			dhdpcie_bus_membytes(bus, TRUE, addr, (uint8*) &long_data, len);
			if (dhd_msg_level & DHD_INFO_VAL) {
				prhex(__FUNCTION__, data, len);
			}
			break;

		case H2D_DMA_INDX_RD_BUF:
			long_data = HTOL64(*(uint64 *)data);
			addr = DHD_RING_INFO_MEMBER_ADDR(bus, h2d_r_idx_hostaddr);
			dhdpcie_bus_membytes(bus, TRUE, addr, (uint8*) &long_data, len);
			if (dhd_msg_level & DHD_INFO_VAL) {
				prhex(__FUNCTION__, data, len);
			}
			break;

		case D2H_DMA_INDX_WR_BUF:
			long_data = HTOL64(*(uint64 *)data);
			addr = DHD_RING_INFO_MEMBER_ADDR(bus, d2h_w_idx_hostaddr);
			dhdpcie_bus_membytes(bus, TRUE, addr, (uint8*) &long_data, len);
			if (dhd_msg_level & DHD_INFO_VAL) {
				prhex(__FUNCTION__, data, len);
			}
			break;

		case D2H_DMA_INDX_RD_BUF:
			long_data = HTOL64(*(uint64 *)data);
			addr = DHD_RING_INFO_MEMBER_ADDR(bus, d2h_r_idx_hostaddr);
			dhdpcie_bus_membytes(bus, TRUE, addr, (uint8*) &long_data, len);
			if (dhd_msg_level & DHD_INFO_VAL) {
				prhex(__FUNCTION__, data, len);
			}
			break;

		case H2D_IFRM_INDX_WR_BUF:
			long_data = HTOL64(*(uint64 *)data);
			addr = DHD_RING_INFO_MEMBER_ADDR(bus, ifrm_w_idx_hostaddr);
			dhdpcie_bus_membytes(bus, TRUE, addr, (uint8*) &long_data, len);
			if (dhd_msg_level & DHD_INFO_VAL) {
				prhex(__FUNCTION__, data, len);
			}
			break;

		case RING_ITEM_LEN :
			addr = DHD_RING_MEM_MEMBER_ADDR(bus, ringid, len_items);
			dhdpcie_bus_wtcm16(bus, addr, (uint16) HTOL16(*(uint16 *)data));
			break;

		case RING_MAX_ITEMS :
			addr = DHD_RING_MEM_MEMBER_ADDR(bus, ringid, max_item);
			dhdpcie_bus_wtcm16(bus, addr, (uint16) HTOL16(*(uint16 *)data));
			break;

		case RING_BUF_ADDR :
			long_data = HTOL64(*(uint64 *)data);
			addr = DHD_RING_MEM_MEMBER_ADDR(bus, ringid, base_addr);
			dhdpcie_bus_membytes(bus, TRUE, addr, (uint8 *) &long_data, len);
			if (dhd_msg_level & DHD_INFO_VAL) {
				prhex(__FUNCTION__, data, len);
			}
			break;

		case D2H_MB_DATA:
			addr = bus->d2h_mb_data_ptr_addr;
			dhdpcie_bus_wtcm32(bus, addr, (uint32) HTOL32(*(uint32 *)data));
			break;

		case H2D_MB_DATA:
			addr = bus->h2d_mb_data_ptr_addr;
			dhdpcie_bus_wtcm32(bus, addr, (uint32) HTOL32(*(uint32 *)data));
			break;

		case HOST_API_VERSION:
			addr = DHD_PCIE_SHARED_MEMBER_ADDR(bus, host_cap);
			dhdpcie_bus_wtcm32(bus, addr, (uint32) HTOL32(*(uint32 *)data));
			break;

		case DNGL_TO_HOST_TRAP_ADDR:
			long_data = HTOL64(*(uint64 *)data);
			addr = DHD_PCIE_SHARED_MEMBER_ADDR(bus, host_trap_addr);
			dhdpcie_bus_membytes(bus, TRUE, addr, (uint8 *) &long_data, len);
			DHD_INFO(("Wrote trap addr:0x%x\n", (uint32) HTOL32(*(uint32 *)data)));
			break;

#ifdef D2H_MINIDUMP
		case DNGL_TO_HOST_TRAP_ADDR_LEN:
			addr = DHD_PCIE_SHARED_MEMBER_ADDR(bus, device_trap_debug_buffer_len);
			dhdpcie_bus_wtcm16(bus, addr, (uint16) HTOL16(*(uint16 *)data));
			break;
#endif /* D2H_MINIDUMP */

		case HOST_SCB_ADDR:
			addr = DHD_PCIE_SHARED_MEMBER_ADDR(bus, host_scb_addr);
#ifdef DHD_SUPPORT_64BIT
			dhdpcie_bus_wtcm64(bus, addr, (uint64) HTOL64(*(uint64 *)data));
#else /* !DHD_SUPPORT_64BIT */
			dhdpcie_bus_wtcm32(bus, addr, *((uint32*)data));
#endif /* DHD_SUPPORT_64BIT */
			DHD_INFO(("Wrote host_scb_addr:0x%x\n",
				(uint32) HTOL32(*(uint32 *)data)));
			break;

#ifdef DHD_PCIE_PTM
		case HOST_CAP2:
			addr = DHD_PCIE_SHARED_MEMBER_ADDR(bus, host_cap2);
			dhdpcie_bus_wtcm32(bus, addr, (uint32) HTOL32(*(uint32 *)data));
			DHD_INFO(("Wrote host_cap2 0x%x\n", (uint32) HTOL32(*(uint32 *)data)));
			break;
#endif /* DHD_PCIE_PTM */

		default:
			break;
	}
	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req_clear(bus);
	}
} /* dhd_bus_cmn_writeshared */

/** A snippet of dongle memory is shared between host and dongle */
void
dhd_bus_cmn_readshared(dhd_bus_t *bus, void* data, uint8 type, uint16 ringid)
{
	ulong addr; /* dongle address */

	/*
	 * Use explicit tcm ring ptr update functions when DMA indices are not enabled to
	 * as backplane power request calls are causing TPUT drops
	 */
	if (!(bus->dhd->dma_d2h_ring_upd_support || bus->dhd->dma_h2d_ring_upd_support)) {
		if ((type == RING_WR_UPD) || (type == RING_RD_UPD)) {
			dhdpcie_update_ring_ptrs_in_tcm(bus, data, type, ringid, TRUE);
			return;
		}
	}

	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req(bus);
	}
	switch (type) {
		case RING_WR_UPD :
			addr = bus->ring_sh[ringid].ring_state_w;
			*(uint16*)data = LTOH16(dhdpcie_bus_rtcm16(bus, addr));
			break;

		case RING_RD_UPD :
			addr = bus->ring_sh[ringid].ring_state_r;
			*(uint16*)data = LTOH16(dhdpcie_bus_rtcm16(bus, addr));
			break;

		case TOTAL_LFRAG_PACKET_CNT :
			addr = DHD_PCIE_SHARED_MEMBER_ADDR(bus, total_lfrag_pkt_cnt);
			*(uint16*)data = LTOH16(dhdpcie_bus_rtcm16(bus, addr));
			break;

		case H2D_MB_DATA:
			addr = bus->h2d_mb_data_ptr_addr;
			*(uint32*)data = LTOH32(dhdpcie_bus_rtcm32(bus, addr));
			break;

		case D2H_MB_DATA:
			addr = bus->d2h_mb_data_ptr_addr;
			*(uint32*)data = LTOH32(dhdpcie_bus_rtcm32(bus, addr));
			break;

		case MAX_HOST_RXBUFS :
			addr = DHD_PCIE_SHARED_MEMBER_ADDR(bus, max_host_rxbufs);
			*(uint16*)data = LTOH16(dhdpcie_bus_rtcm16(bus, addr));
			break;

		case HOST_SCB_ADDR:
			addr = DHD_PCIE_SHARED_MEMBER_ADDR(bus, host_scb_size);
			*(uint32*)data = LTOH32(dhdpcie_bus_rtcm32(bus, addr));
			break;

		default :
			break;
	}
	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req_clear(bus);
	}
}

uint32 dhd_bus_get_sharedflags(dhd_bus_t *bus)
{
	return ((pciedev_shared_t*)bus->pcie_sh)->flags;
}

void dhd_prot_clearcounts(dhd_pub_t *dhd);

void
dhd_bus_clearcounts(dhd_pub_t *dhdp)
{
	dhd_prot_clearcounts(dhdp);
	dhdp->rx_pktgetpool_fail = 0;

	dhd_clear_dpc_histos(dhdp);
}

/**
 * @param params    input buffer, NULL for 'set' operation.
 * @param plen      length of 'params' buffer, 0 for 'set' operation.
 * @param arg       output buffer
 */
int
dhd_bus_iovar_op(dhd_pub_t *dhdp, const char *name,
                 void *params, uint plen, void *arg, uint len, bool set)
{
	dhd_bus_t *bus = dhdp->bus;
	const bcm_iovar_t *vi = NULL;
	int bcmerror = BCME_UNSUPPORTED;
	uint val_size;
	uint32 actionid;

	DHD_TRACE(("%s: Enter\n", __FUNCTION__));

	ASSERT(name);
	if (!name)
		return BCME_BADARG;

	/* Get MUST have return space */
	ASSERT(set || (arg && len));
	if (!(set || (arg && len)))
		return BCME_BADARG;

	/* Set does NOT take qualifiers */
	ASSERT(!set || (!params && !plen));
	if (!(!set || (!params && !plen)))
		return BCME_BADARG;

	DHD_INFO(("%s: %s %s, len %d plen %d\n", __FUNCTION__,
	         name, (set ? "set" : "get"), len, plen));

	/* Look up var locally; if not found pass to host driver */
	if ((vi = bcm_iovar_lookup(dhdpcie_iovars, name)) == NULL) {
		goto exit;
	}

	if (MULTIBP_ENAB(bus->sih)) {
		if (vi->flags & DHD_IOVF_PWRREQ_BYPASS) {
			DHD_ERROR(("%s: Bypass pwr request\n", __FUNCTION__));
		} else {
			dhd_bus_pcie_pwr_req(bus);
		}
	}

	/* set up 'params' pointer in case this is a set command so that
	 * the convenience int and bool code can be common to set and get
	 */
	if (params == NULL) {
		params = arg;
		plen = len;
	}

	if (vi->type == IOVT_VOID)
		val_size = 0;
	else if (vi->type == IOVT_BUFFER)
		val_size = len;
	else
		/* all other types are integer sized */
		val_size = sizeof(int);

	actionid = set ? IOV_SVAL(vi->varid) : IOV_GVAL(vi->varid);
	bcmerror = dhdpcie_bus_doiovar(bus, vi, actionid, name, params, plen, arg, len, val_size);

exit:
	/* In DEVRESET_QUIESCE/DEVRESET_ON,
	 * this includes dongle re-attach which initialize pwr_req_ref count to 0 and
	 * causes pwr_req_ref count miss-match in pwr req clear function and hang.
	 * In this case, bypass pwr req clear.
	 */
	if (bcmerror == BCME_DNGL_DEVRESET) {
		bcmerror = BCME_OK;
	} else {
		if (MULTIBP_ENAB(bus->sih)) {
			if (vi != NULL) {
				if (vi->flags & DHD_IOVF_PWRREQ_BYPASS) {
					DHD_ERROR(("%s: Bypass pwr request clear\n", __FUNCTION__));
				} else {
					dhd_bus_pcie_pwr_req_clear(bus);
				}
			}
		}
	}
	return bcmerror;
} /* dhd_bus_iovar_op */

#ifdef BCM_BUZZZ
#include <bcm_buzzz.h>

int
dhd_buzzz_dump_cntrs(char *p, uint32 *core, uint32 *log,
	const int num_counters)
{
	int bytes = 0;
	uint32 ctr;
	uint32 curr[BCM_BUZZZ_COUNTERS_MAX], prev[BCM_BUZZZ_COUNTERS_MAX];
	uint32 delta[BCM_BUZZZ_COUNTERS_MAX];

	/* Compute elapsed counter values per counter event type */
	for (ctr = 0U; ctr < num_counters; ctr++) {
		prev[ctr] = core[ctr];
		curr[ctr] = *log++;
		core[ctr] = curr[ctr];  /* saved for next log */

		if (curr[ctr] < prev[ctr])
			delta[ctr] = curr[ctr] + (~0U - prev[ctr]);
		else
			delta[ctr] = (curr[ctr] - prev[ctr]);

		bytes += sprintf(p + bytes, "%12u ", delta[ctr]);
	}

	return bytes;
}

typedef union cm3_cnts { /* export this in bcm_buzzz.h */
	uint32 u32;
	uint8  u8[4];
	struct {
		uint8 cpicnt;
		uint8 exccnt;
		uint8 sleepcnt;
		uint8 lsucnt;
	};
} cm3_cnts_t;

int
dhd_bcm_buzzz_dump_cntrs6(char *p, uint32 *core, uint32 *log)
{
	int bytes = 0;

	uint32 cyccnt, instrcnt;
	cm3_cnts_t cm3_cnts;
	uint8 foldcnt;

	{   /* 32bit cyccnt */
		uint32 curr, prev, delta;
		prev = core[0]; curr = *log++; core[0] = curr;
		if (curr < prev)
			delta = curr + (~0U - prev);
		else
			delta = (curr - prev);

		bytes += sprintf(p + bytes, "%12u ", delta);
		cyccnt = delta;
	}

	{	/* Extract the 4 cnts: cpi, exc, sleep and lsu */
		int i;
		uint8 max8 = ~0;
		cm3_cnts_t curr, prev, delta;
		prev.u32 = core[1]; curr.u32 = * log++; core[1] = curr.u32;
		for (i = 0; i < 4; i++) {
			if (curr.u8[i] < prev.u8[i])
				delta.u8[i] = curr.u8[i] + (max8 - prev.u8[i]);
			else
				delta.u8[i] = (curr.u8[i] - prev.u8[i]);
			bytes += sprintf(p + bytes, "%4u ", delta.u8[i]);
		}
		cm3_cnts.u32 = delta.u32;
	}

	{   /* Extract the foldcnt from arg0 */
		uint8 curr, prev, delta, max8 = ~0;
		bcm_buzzz_arg0_t arg0; arg0.u32 = *log;
		prev = core[2]; curr = arg0.klog.cnt; core[2] = curr;
		if (curr < prev)
			delta = curr + (max8 - prev);
		else
			delta = (curr - prev);
		bytes += sprintf(p + bytes, "%4u ", delta);
		foldcnt = delta;
	}

	instrcnt = cyccnt - (cm3_cnts.u8[0] + cm3_cnts.u8[1] + cm3_cnts.u8[2]
		                 + cm3_cnts.u8[3]) + foldcnt;
	if (instrcnt > 0xFFFFFF00)
		bytes += sprintf(p + bytes, "[%10s] ", "~");
	else
		bytes += sprintf(p + bytes, "[%10u] ", instrcnt);
	return bytes;
}

int
dhd_buzzz_dump_log(char *p, uint32 *core, uint32 *log, bcm_buzzz_t *buzzz)
{
	int bytes = 0;
	bcm_buzzz_arg0_t arg0;
	static uint8 * fmt[] = BCM_BUZZZ_FMT_STRINGS;

	if (buzzz->counters == 6) {
		bytes += dhd_bcm_buzzz_dump_cntrs6(p, core, log);
		log += 2; /* 32bit cyccnt + (4 x 8bit) CM3 */
	} else {
		bytes += dhd_buzzz_dump_cntrs(p, core, log, buzzz->counters);
		log += buzzz->counters; /* (N x 32bit) CR4=3, CA7=4 */
	}

	/* Dump the logged arguments using the registered formats */
	arg0.u32 = *log++;

	switch (arg0.klog.args) {
		case 0:
			bytes += sprintf(p + bytes, fmt[arg0.klog.id]);
			break;
		case 1:
		{
			uint32 arg1 = *log++;
			bytes += sprintf(p + bytes, fmt[arg0.klog.id], arg1);
			break;
		}
		case 2:
		{
			uint32 arg1, arg2;
			arg1 = *log++; arg2 = *log++;
			bytes += sprintf(p + bytes, fmt[arg0.klog.id], arg1, arg2);
			break;
		}
		case 3:
		{
			uint32 arg1, arg2, arg3;
			arg1 = *log++; arg2 = *log++; arg3 = *log++;
			bytes += sprintf(p + bytes, fmt[arg0.klog.id], arg1, arg2, arg3);
			break;
		}
		case 4:
		{
			uint32 arg1, arg2, arg3, arg4;
			arg1 = *log++; arg2 = *log++;
			arg3 = *log++; arg4 = *log++;
			bytes += sprintf(p + bytes, fmt[arg0.klog.id], arg1, arg2, arg3, arg4);
			break;
		}
		default:
			DHD_CONS_ONLY(("%s: Maximum one argument supported\n", __FUNCTION__));
			break;
	}

	bytes += sprintf(p + bytes, "\n");

	return bytes;
}

void dhd_buzzz_dump(bcm_buzzz_t *buzzz_p, void *buffer_p, char *p)
{
	int i;
	uint32 total, part1, part2, log_sz, core[BCM_BUZZZ_COUNTERS_MAX];
	void * log;

	for (i = 0; i < BCM_BUZZZ_COUNTERS_MAX; i++) {
		core[i] = 0;
	}

	log_sz = buzzz_p->log_sz;

	part1 = ((uint32)buzzz_p->cur - (uint32)buzzz_p->log) / log_sz;

	if (buzzz_p->wrap == TRUE) {
		part2 = ((uint32)buzzz_p->end - (uint32)buzzz_p->cur) / log_sz;
		total = (buzzz_p->buffer_sz - BCM_BUZZZ_LOGENTRY_MAXSZ) / log_sz;
	} else {
		part2 = 0U;
		total = buzzz_p->count;
	}

	if (total == 0U) {
		DHD_CONS_ONLY(("%s: bcm_buzzz_dump total<%u> done\n", __FUNCTION__, total));
		return;
	} else {
		DHD_CONS_ONLY(("%s: bcm_buzzz_dump total<%u> : part2<%u> + part1<%u>\n",
			__FUNCTION__, total, part2, part1));
	}

	if (part2) {   /* with wrap */
		log = (void*)((size_t)buffer_p + (buzzz_p->cur - buzzz_p->log));
		while (part2--) {   /* from cur to end : part2 */
			p[0] = '\0';
			dhd_buzzz_dump_log(p, core, (uint32 *)log, buzzz_p);
			printf("%s", p);
			log = (void*)((size_t)log + buzzz_p->log_sz);
		}
	}

	log = (void*)buffer_p;
	while (part1--) {
		p[0] = '\0';
		dhd_buzzz_dump_log(p, core, (uint32 *)log, buzzz_p);
		printf("%s", p);
		log = (void*)((size_t)log + buzzz_p->log_sz);
	}

	DHD_CONS_ONLY(("%s: bcm_buzzz_dump done.\n", __FUNCTION__));
}

int dhd_buzzz_dump_dngl(dhd_bus_t *bus)
{
	bcm_buzzz_t * buzzz_p = NULL;
	void * buffer_p = NULL;
	char * page_p = NULL;
	pciedev_shared_t *sh;
	int ret = 0;

	if (bus->dhd->busstate != DHD_BUS_DATA) {
		return BCME_UNSUPPORTED;
	}
	if ((page_p = (char *)MALLOC(bus->dhd->osh, 4096)) == NULL) {
		DHD_CONS_ONLY(("%s: Page memory allocation failure\n", __FUNCTION__));
		goto done;
	}
	if ((buzzz_p = MALLOC(bus->dhd->osh, sizeof(bcm_buzzz_t))) == NULL) {
		DHD_CONS_ONLY(("%s: BCM BUZZZ memory allocation failure\n", __FUNCTION__));
		goto done;
	}

	ret = dhdpcie_readshared(bus);
	if (ret < 0) {
		DHD_ERROR(("%s :Shared area read failed \n", __FUNCTION__));
		goto done;
	}

	sh = bus->pcie_sh;

	DHD_INFO(("%s buzzz:%08x\n", __FUNCTION__, sh->buzz_dbg_ptr));

	if (sh->buzz_dbg_ptr != 0U) {	/* Fetch and display dongle BUZZZ Trace */

		dhdpcie_bus_membytes(bus, FALSE, (ulong)sh->buzz_dbg_ptr,
		                     (uint8 *)buzzz_p, sizeof(bcm_buzzz_t));

		DHD_CONS_ONLY(("BUZZZ[0x%08x]: log<0x%08x> cur<0x%08x> end<0x%08x> "
			"count<%u> status<%u> wrap<%u>\n"
			"cpu<0x%02X> counters<%u> group<%u> buffer_sz<%u> log_sz<%u>\n",
			(int)sh->buzz_dbg_ptr,
			(int)buzzz_p->log, (int)buzzz_p->cur, (int)buzzz_p->end,
			buzzz_p->count, buzzz_p->status, buzzz_p->wrap,
			buzzz_p->cpu_idcode, buzzz_p->counters, buzzz_p->group,
			buzzz_p->buffer_sz, buzzz_p->log_sz));

		if (buzzz_p->count == 0) {
			DHD_CONS_ONLY(("%s: Empty dongle BUZZZ trace\n\n", __FUNCTION__));
			goto done;
		}

		/* Allocate memory for trace buffer and format strings */
		buffer_p = MALLOC(bus->dhd->osh, buzzz_p->buffer_sz);
		if (buffer_p == NULL) {
			DHD_CONS_ONLY(("%s: Buffer memory allocation failure\n", __FUNCTION__));
			goto done;
		}

		/* Fetch the trace. format strings are exported via bcm_buzzz.h */
		dhdpcie_bus_membytes(bus, FALSE, (uint32)buzzz_p->log,   /* Trace */
		                     (uint8 *)buffer_p, buzzz_p->buffer_sz);

		/* Process and display the trace using formatted output */

		{
			int ctr;
			for (ctr = 0; ctr < buzzz_p->counters; ctr++) {
				printf("<Evt[%02X]> ", buzzz_p->eventid[ctr]);
			}
			DHD_CONS_ONLY(("<code execution point>\n"));
		}

		dhd_buzzz_dump(buzzz_p, buffer_p, page_p);

		DHD_CONS_ONLY(("%s: ----- End of dongle BCM BUZZZ Trace -----\n\n", __FUNCTION__));

		MFREE(bus->dhd->osh, buffer_p, buzzz_p->buffer_sz); buffer_p = NULL;
	}

done:

	if (page_p)   MFREE(bus->dhd->osh, page_p, 4096);
	if (buzzz_p)  MFREE(bus->dhd->osh, buzzz_p, sizeof(bcm_buzzz_t));
	if (buffer_p) MFREE(bus->dhd->osh, buffer_p, buzzz_p->buffer_sz);

	return BCME_OK;
}
#endif /* BCM_BUZZZ */

#define PCIE_GEN2(sih) ((BUSTYPE((sih)->bustype) == PCI_BUS) &&	\
	((sih)->buscoretype == PCIE2_CORE_ID))
#ifdef DHD_PCIE_REG_ACCESS
static bool
pcie2_mdiosetblock(dhd_bus_t *bus, uint blk)
{
	uint mdiodata, mdioctrl, i = 0;
	uint pcie_serdes_spinwait = 200;

	mdioctrl = MDIOCTL2_DIVISOR_VAL | (0x1F << MDIOCTL2_REGADDR_SHF);
	mdiodata = (blk << MDIODATA2_DEVADDR_SHF) | MDIODATA2_DONE;

	si_corereg(bus->sih, bus->sih->buscoreidx, PCIE2_MDIO_CONTROL, ~0, mdioctrl);
	si_corereg(bus->sih, bus->sih->buscoreidx, PCIE2_MDIO_WR_DATA, ~0, mdiodata);

	OSL_DELAY(10);
	/* retry till the transaction is complete */
	while (i < pcie_serdes_spinwait) {
		uint mdioctrl_read = si_corereg(bus->sih, bus->sih->buscoreidx, PCIE2_MDIO_WR_DATA,
			0, 0);
		if (!(mdioctrl_read & MDIODATA2_DONE)) {
			break;
		}
		OSL_DELAY(1000);
		i++;
	}

	if (i >= pcie_serdes_spinwait) {
		DHD_ERROR(("pcie_mdiosetblock: timed out\n"));
		return FALSE;
	}

	return TRUE;
}
#endif /* DHD_PCIE_REG_ACCESS */

static void
dhdpcie_enum_reg_init(dhd_bus_t *bus)
{
	/* initialize Function control register (clear bit 4) to HW init value */
	si_corereg(bus->sih, bus->sih->buscoreidx,
		OFFSETOF(sbpcieregs_t, ftn_ctrl.control), ~0,
		PCIE_CPLCA_ENABLE | PCIE_DLY_PERST_TO_COE);

	/* clear IntMask */
	si_corereg(bus->sih, bus->sih->buscoreidx,
		OFFSETOF(sbpcieregs_t, ftn_ctrl.intmask), ~0, 0);
	/* clear IntStatus */
	si_corereg(bus->sih, bus->sih->buscoreidx,
		OFFSETOF(sbpcieregs_t, ftn_ctrl.intstatus), ~0,
		si_corereg(bus->sih, bus->sih->buscoreidx,
			OFFSETOF(sbpcieregs_t, ftn_ctrl.intstatus), 0, 0));

	/* clear MSIVector */
	si_corereg(bus->sih, bus->sih->buscoreidx,
		OFFSETOF(sbpcieregs_t, ftn_ctrl.msi_vector), ~0, 0);
	/* clear MSIIntMask */
	si_corereg(bus->sih, bus->sih->buscoreidx,
		OFFSETOF(sbpcieregs_t, ftn_ctrl.msi_intmask), ~0, 0);
	/* clear MSIIntStatus */
	si_corereg(bus->sih, bus->sih->buscoreidx,
		OFFSETOF(sbpcieregs_t, ftn_ctrl.msi_intstatus), ~0,
		si_corereg(bus->sih, bus->sih->buscoreidx,
			OFFSETOF(sbpcieregs_t, ftn_ctrl.msi_intstatus), 0, 0));

	/* clear PowerIntMask */
	si_corereg(bus->sih, bus->sih->buscoreidx,
		OFFSETOF(sbpcieregs_t, ftn_ctrl.pwr_intmask), ~0, 0);
	/* clear PowerIntStatus */
	si_corereg(bus->sih, bus->sih->buscoreidx,
		OFFSETOF(sbpcieregs_t, ftn_ctrl.pwr_intstatus), ~0,
		si_corereg(bus->sih, bus->sih->buscoreidx,
			OFFSETOF(sbpcieregs_t, ftn_ctrl.pwr_intstatus), 0, 0));

	/* clear MailboxIntMask */
	si_corereg(bus->sih, bus->sih->buscoreidx,
		OFFSETOF(sbpcieregs_t, ftn_ctrl.mbox_intmask), ~0, 0);
	/* clear MailboxInt */
	si_corereg(bus->sih, bus->sih->buscoreidx,
		OFFSETOF(sbpcieregs_t, ftn_ctrl.mbox_intstatus), ~0,
		si_corereg(bus->sih, bus->sih->buscoreidx,
			OFFSETOF(sbpcieregs_t, ftn_ctrl.mbox_intstatus), 0, 0));
}

int
dhd_bus_perform_flr(dhd_bus_t *bus, bool force_fail)
{
	uint flr_capab;
	uint val;
	int retry = 0;
	bool in_flr_already = FALSE;

	DHD_ERROR(("******** Perform FLR ********\n"));

	/* Kernel Panic for 4378Ax during traptest/devreset4 reload case:
	 * For 4378Ax, enum registers will not be reset with FLR (producer index WAR).
	 * So, the MailboxIntMask is left as 0xffff during fw boot-up,
	 * and the fw trap handling during fw boot causes Kernel Panic.
	 * Jira: SWWLAN-212578: [4378A0 PCIe DVT] :
	 *       Kernel Panic seen in F0 FLR with BT Idle/Traffic/DMA
	 */
	if (bus->sih && PCIE_ENUM_RESET_WAR_ENAB(bus->sih->buscorerev)) {
		if (bus->pcie_mailbox_mask != 0) {
			dhdpcie_bus_intr_disable(bus);
		}
		/* initialize F0 enum registers before FLR for rev66/67 */
		dhdpcie_enum_reg_init(bus);
	}

	/* Read PCIE_CFG_DEVICE_CAPABILITY bit 28 to check FLR capability */
	val = OSL_PCI_READ_CONFIG(bus->osh, PCIE_CFG_DEVICE_CAPABILITY, sizeof(val));
	flr_capab =  val & (1 << PCIE_FLR_CAPAB_BIT);
	DHD_INFO(("Read Device Capability: reg=0x%x read val=0x%x flr_capab=0x%x\n",
		PCIE_CFG_DEVICE_CAPABILITY, val, flr_capab));
	if (!flr_capab) {
	       DHD_ERROR(("Chip does not support FLR\n"));
	       return BCME_UNSUPPORTED;
	}
	/* Check if we are already in FLR, if we are then just go to restore part of it  */
	val = OSL_PCI_READ_CONFIG(bus->osh, PCIE_CFG_SUBSYSTEM_CONTROL, sizeof(val));
	DHD_ERROR(("config space 0x88 is 0x%x\n", val));
	if (val & (1 << PCIE_SSRESET_STATUS_BIT)) {
		DHD_ERROR(("******** device already in FLR, so clear only ********\n"));
		in_flr_already = TRUE;
	}
	else {
		DHD_ERROR(("******** device not in FLR ********\n"));
	}
	if (!in_flr_already) {

		bus->ptm_ctrl_reg = dhdpcie_bus_cfg_read_dword(bus, PCI_CFG_PTM_CTRL, 4);

		/* Save pcie config space */
		DHD_INFO(("Save Pcie Config Space\n"));
		DHD_PCIE_CONFIG_SAVE(bus);

		/* Set bit 15 of PCIE_CFG_DEVICE_CONTROL */
		DHD_INFO(("Set PCIE_FUNCTION_LEVEL_RESET_BIT(%d) of"
			" PCIE_CFG_DEVICE_CONTROL(0x%x)\n",
			PCIE_FUNCTION_LEVEL_RESET_BIT, PCIE_CFG_DEVICE_CONTROL));
		val = OSL_PCI_READ_CONFIG(bus->osh, PCIE_CFG_DEVICE_CONTROL, sizeof(val));
		DHD_INFO(("read_config: reg=0x%x read val=0x%x\n", PCIE_CFG_DEVICE_CONTROL, val));
		val = val | (1 << PCIE_FUNCTION_LEVEL_RESET_BIT);
		DHD_INFO(("write_config: reg=0x%x write val=0x%x\n", PCIE_CFG_DEVICE_CONTROL, val));
		OSL_PCI_WRITE_CONFIG(bus->osh, PCIE_CFG_DEVICE_CONTROL, sizeof(val), val);
		if (bus->lpm_keep_in_reset) {
			/* Keep the device in FLR */
			DHD_ERROR(("******** device force FLR only ********\n"));
			return BCME_OK;
		}
		else {
			DHD_ERROR(("******** device force FLR only not set ********\n"));
		}

		/* wait for DHD_FUNCTION_LEVEL_RESET_DELAY msec */
#ifdef BCMSLTGT
		DHD_ERROR_MEM(("Delay of %d msec\n", DHD_FUNCTION_LEVEL_RESET_DELAY * htclkratio));
#else
		DHD_ERROR_MEM(("Delay of %d msec\n", DHD_FUNCTION_LEVEL_RESET_DELAY));
#endif /* BCMSLTGT */

		CAN_SLEEP() ? OSL_SLEEP(DHD_FUNCTION_LEVEL_RESET_DELAY) :
			OSL_DELAY(DHD_FUNCTION_LEVEL_RESET_DELAY * USEC_PER_MSEC);
#ifdef USE_ISB_IN_FLR
		OSL_ISB();
#endif /* USE_ISB_IN_FLR */

		if (force_fail) {
			DHD_ERROR(("Set PCIE_SSRESET_DISABLE_BIT(%d) of"
				" PCIE_CFG_SUBSYSTEM_CONTROL(0x%x)\n",
				PCIE_SSRESET_DISABLE_BIT, PCIE_CFG_SUBSYSTEM_CONTROL));
			val = OSL_PCI_READ_CONFIG(bus->osh, PCIE_CFG_SUBSYSTEM_CONTROL,
				sizeof(val));
			DHD_ERROR(("read_config: reg=0x%x read val=0x%x\n",
				PCIE_CFG_SUBSYSTEM_CONTROL, val));
			val = val | (1 << PCIE_SSRESET_DISABLE_BIT);
			DHD_ERROR(("write_config: reg=0x%x write val=0x%x\n",
				PCIE_CFG_SUBSYSTEM_CONTROL, val));
			OSL_PCI_WRITE_CONFIG(bus->osh, PCIE_CFG_SUBSYSTEM_CONTROL,
				sizeof(val), val);

			val = OSL_PCI_READ_CONFIG(bus->osh, PCIE_CFG_SUBSYSTEM_CONTROL,
				sizeof(val));
			DHD_ERROR(("read_config: reg=0x%x read val=0x%x\n",
				PCIE_CFG_SUBSYSTEM_CONTROL, val));
		}
	}

	/* Clear bit 15 of PCIE_CFG_DEVICE_CONTROL */
	DHD_INFO(("Clear PCIE_FUNCTION_LEVEL_RESET_BIT(%d) of PCIE_CFG_DEVICE_CONTROL(0x%x)\n",
		PCIE_FUNCTION_LEVEL_RESET_BIT, PCIE_CFG_DEVICE_CONTROL));
	val = OSL_PCI_READ_CONFIG(bus->osh, PCIE_CFG_DEVICE_CONTROL, sizeof(val));
	DHD_INFO(("read_config: reg=0x%x read val=0x%x\n", PCIE_CFG_DEVICE_CONTROL, val));
	val = val & ~(1 << PCIE_FUNCTION_LEVEL_RESET_BIT);
	DHD_INFO(("write_config: reg=0x%x write val=0x%x\n", PCIE_CFG_DEVICE_CONTROL, val));
	OSL_PCI_WRITE_CONFIG(bus->osh, PCIE_CFG_DEVICE_CONTROL, sizeof(val), val);

	/* Wait till bit 13 of PCIE_CFG_SUBSYSTEM_CONTROL is cleared */
	DHD_INFO(("Wait till PCIE_SSRESET_STATUS_BIT(%d) of PCIE_CFG_SUBSYSTEM_CONTROL(0x%x)"
		"is cleared\n",	PCIE_SSRESET_STATUS_BIT, PCIE_CFG_SUBSYSTEM_CONTROL));
	do {
		val = OSL_PCI_READ_CONFIG(bus->osh, PCIE_CFG_SUBSYSTEM_CONTROL, sizeof(val));
		DHD_ERROR(("read_config: reg=0x%x read val=0x%x\n",
			PCIE_CFG_SUBSYSTEM_CONTROL, val));
		val = val & (1 << PCIE_SSRESET_STATUS_BIT);
		OSL_DELAY(DHD_SSRESET_STATUS_RETRY_DELAY);
	} while (val && (retry++ < DHD_SSRESET_STATUS_RETRIES));

	if (val) {
		DHD_ERROR_MEM(("ERROR: reg=0x%x bit %d is not cleared\n",
			PCIE_CFG_SUBSYSTEM_CONTROL, PCIE_SSRESET_STATUS_BIT));
		/* User has to fire the IOVAR again, if force_fail is needed */
		if (force_fail) {
			bus->flr_force_fail = FALSE;
			DHD_ERROR(("%s cleared flr_force_fail flag\n", __FUNCTION__));
		}
		return BCME_DONGLE_DOWN;
	}

	dhdpcie_bus_cfg_write_dword(bus, PCI_CFG_PTM_CTRL, 4, bus->ptm_ctrl_reg);

	/* Restore pcie config space */
	DHD_INFO(("Restore Pcie Config Space\n"));
	DHD_PCIE_CONFIG_RESTORE(bus);

	DHD_ERROR(("******** FLR Succedeed ********\n"));

	return BCME_OK;
}

#define DHD_BP_RESET_ASPM_DISABLE_DELAY		500u	/* usec */

#define DHD_BP_RESET_STATUS_RETRY_DELAY		40u	/* usec */
#define DHD_BP_RESET_STATUS_RETRIES		50u

#define PCIE_CFG_SUBSYSTEM_CONTROL_BP_RESET_BIT		10
#define PCIE_CFG_SUBSYSTEM_CONTROL_BP_RESET_STATUS_BIT	12

int
dhd_bus_cfg_ss_ctrl_bp_reset(struct dhd_bus *bus)
{
	uint val;
	int retry = 0;
	int ret = BCME_OK;
	bool reset_stat_bit;

	DHD_ERROR(("******** Perform BP reset ********\n"));

	/* Disable ASPM */
	DHD_ERROR(("Disable ASPM: Clear bits(1-0) of PCIECFGREG_LINK_STATUS_CTRL(0x%x)\n",
		PCIECFGREG_LINK_STATUS_CTRL));
	val = OSL_PCI_READ_CONFIG(bus->osh, PCIECFGREG_LINK_STATUS_CTRL, sizeof(val));
	DHD_ERROR(("read_config: reg=0x%x read val=0x%x\n", PCIECFGREG_LINK_STATUS_CTRL, val));
	val = val & (~PCIE_ASPM_ENAB);
	DHD_ERROR(("write_config: reg=0x%x write val=0x%x\n", PCIECFGREG_LINK_STATUS_CTRL, val));
	OSL_PCI_WRITE_CONFIG(bus->osh, PCIECFGREG_LINK_STATUS_CTRL, sizeof(val), val);

	/* wait for delay usec */
	DHD_ERROR(("Delay of %d usec\n", DHD_BP_RESET_ASPM_DISABLE_DELAY));
	OSL_DELAY(DHD_BP_RESET_ASPM_DISABLE_DELAY);

	/* Set bp reset bit 10 of PCIE_CFG_SUBSYSTEM_CONTROL */
	DHD_ERROR(("Set PCIE_CFG_SUBSYSTEM_CONTROL_BP_RESET_BIT(%d)"
		" of PCIE_CFG_SUBSYSTEM_CONTROL(0x%x)\n",
		PCIE_CFG_SUBSYSTEM_CONTROL_BP_RESET_BIT, PCIE_CFG_SUBSYSTEM_CONTROL));
	val = OSL_PCI_READ_CONFIG(bus->osh, PCIE_CFG_SUBSYSTEM_CONTROL, sizeof(val));
	DHD_ERROR(("read_config: reg=0x%x read val=0x%x\n", PCIE_CFG_SUBSYSTEM_CONTROL, val));
	val = val | (1 << PCIE_CFG_SUBSYSTEM_CONTROL_BP_RESET_BIT);
	DHD_ERROR(("write_config: reg=0x%x write val=0x%x\n", PCIE_CFG_SUBSYSTEM_CONTROL, val));
	OSL_PCI_WRITE_CONFIG(bus->osh, PCIE_CFG_SUBSYSTEM_CONTROL, sizeof(val), val);

	/* Wait till bp reset status bit 12 of PCIE_CFG_SUBSYSTEM_CONTROL is set */
	DHD_ERROR(("Wait till PCIE_CFG_SUBSYSTEM_CONTROL_BP_RESET_STATUS_BIT(%d) of "
		"PCIE_CFG_SUBSYSTEM_CONTROL(0x%x) is set\n",
		PCIE_CFG_SUBSYSTEM_CONTROL_BP_RESET_STATUS_BIT, PCIE_CFG_SUBSYSTEM_CONTROL));
	do {
		val = OSL_PCI_READ_CONFIG(bus->osh, PCIE_CFG_SUBSYSTEM_CONTROL, sizeof(val));
		DHD_ERROR(("read_config: reg=0x%x read val=0x%x\n",
			PCIE_CFG_SUBSYSTEM_CONTROL, val));
		reset_stat_bit = val & (1 << PCIE_CFG_SUBSYSTEM_CONTROL_BP_RESET_STATUS_BIT);
		OSL_DELAY(DHD_BP_RESET_STATUS_RETRY_DELAY);
	} while (!reset_stat_bit && (retry++ < DHD_BP_RESET_STATUS_RETRIES));

	if (!reset_stat_bit) {
		DHD_ERROR(("ERROR: reg=0x%x bit %d is not set\n",
			PCIE_CFG_SUBSYSTEM_CONTROL,
			PCIE_CFG_SUBSYSTEM_CONTROL_BP_RESET_STATUS_BIT));
		ret = BCME_ERROR;
		goto aspm_enab;
	}

	/* Clear bp reset bit 10 of PCIE_CFG_SUBSYSTEM_CONTROL */
	DHD_ERROR(("Clear PCIE_CFG_SUBSYSTEM_CONTROL_BP_RESET_BIT(%d)"
		" of PCIECFGREG_SPROM_CTRL(0x%x)\n",
		PCIE_CFG_SUBSYSTEM_CONTROL_BP_RESET_BIT, PCIE_CFG_SUBSYSTEM_CONTROL));
	val = OSL_PCI_READ_CONFIG(bus->osh, PCIE_CFG_SUBSYSTEM_CONTROL, sizeof(val));
	DHD_ERROR(("read_config: reg=0x%x read val=0x%x\n", PCIE_CFG_SUBSYSTEM_CONTROL, val));
	val = val & ~(1 << PCIE_CFG_SUBSYSTEM_CONTROL_BP_RESET_BIT);
	DHD_ERROR(("write_config: reg=0x%x write val=0x%x\n", PCIE_CFG_SUBSYSTEM_CONTROL, val));
	OSL_PCI_WRITE_CONFIG(bus->osh, PCIE_CFG_SUBSYSTEM_CONTROL, sizeof(val), val);

	/* Wait till bp reset status bit 12 of PCIE_CFG_SUBSYSTEM_CONTROL is cleared */
	DHD_ERROR(("Wait till PCIE_CFG_SUBSYSTEM_CONTROL_BP_RESET_STATUS_BIT(%d) of "
		"PCIE_CFG_SUBSYSTEM_CONTROL(0x%x) is cleared\n",
		PCIE_CFG_SUBSYSTEM_CONTROL_BP_RESET_STATUS_BIT, PCIE_CFG_SUBSYSTEM_CONTROL));
	do {
		val = OSL_PCI_READ_CONFIG(bus->osh, PCIE_CFG_SUBSYSTEM_CONTROL, sizeof(val));
		DHD_ERROR(("read_config: reg=0x%x read val=0x%x\n",
			PCIE_CFG_SUBSYSTEM_CONTROL, val));
		reset_stat_bit = val & (1 << PCIE_CFG_SUBSYSTEM_CONTROL_BP_RESET_STATUS_BIT);
		OSL_DELAY(DHD_BP_RESET_STATUS_RETRY_DELAY);
	} while (reset_stat_bit && (retry++ < DHD_BP_RESET_STATUS_RETRIES));

	if (reset_stat_bit) {
		DHD_ERROR(("ERROR: reg=0x%x bit %d is not cleared\n",
			PCIE_CFG_SUBSYSTEM_CONTROL,
			PCIE_CFG_SUBSYSTEM_CONTROL_BP_RESET_STATUS_BIT));
		ret = BCME_ERROR;
	}

aspm_enab:
	/* Enable ASPM */
	DHD_ERROR(("Enable ASPM: set bit 1 of PCIECFGREG_LINK_STATUS_CTRL(0x%x)\n",
		PCIECFGREG_LINK_STATUS_CTRL));
	val = OSL_PCI_READ_CONFIG(bus->osh, PCIECFGREG_LINK_STATUS_CTRL, sizeof(val));
	DHD_ERROR(("read_config: reg=0x%x read val=0x%x\n", PCIECFGREG_LINK_STATUS_CTRL, val));
	val = val | (PCIE_ASPM_L1_ENAB);
	DHD_ERROR(("write_config: reg=0x%x write val=0x%x\n", PCIECFGREG_LINK_STATUS_CTRL, val));
	OSL_PCI_WRITE_CONFIG(bus->osh, PCIECFGREG_LINK_STATUS_CTRL, sizeof(val), val);

	if (ret) {
		DHD_ERROR(("******** BP reset Failed ********\n"));
	} else {
		DHD_ERROR(("******** BP reset Succedeed ********\n"));
	}

	return ret;
}

#define PCIE_CFG_SPROM_CTRL_SB_RESET_BIT	10
#define PCIE_CFG_CLOCK_CTRL_STATUS_BP_RESET_BIT	21

int
dhd_bus_cfg_sprom_ctrl_bp_reset(struct dhd_bus *bus)
{
	uint val;
	int retry = 0;
	uint dar_clk_ctrl_status_reg = DAR_CLK_CTRL(bus->sih->buscorerev);
	int ret = BCME_OK;
	bool cond;

	DHD_ERROR(("******** Perform BP reset ********\n"));

	/* Disable ASPM */
	DHD_INFO(("Disable ASPM: Clear bits(1-0) of PCIECFGREG_LINK_STATUS_CTRL(0x%x)\n",
		PCIECFGREG_LINK_STATUS_CTRL));
	val = OSL_PCI_READ_CONFIG(bus->osh, PCIECFGREG_LINK_STATUS_CTRL, sizeof(val));
	DHD_INFO(("read_config: reg=0x%x read val=0x%x\n", PCIECFGREG_LINK_STATUS_CTRL, val));
	val = val & (~PCIE_ASPM_ENAB);
	DHD_INFO(("write_config: reg=0x%x write val=0x%x\n", PCIECFGREG_LINK_STATUS_CTRL, val));
	OSL_PCI_WRITE_CONFIG(bus->osh, PCIECFGREG_LINK_STATUS_CTRL, sizeof(val), val);

	/* wait for delay usec */
	DHD_INFO(("Delay of %d usec\n", DHD_BP_RESET_ASPM_DISABLE_DELAY));
	OSL_DELAY(DHD_BP_RESET_ASPM_DISABLE_DELAY);

	/* Set bit 10 of PCIECFGREG_SPROM_CTRL */
	DHD_INFO(("Set PCIE_CFG_SPROM_CTRL_SB_RESET_BIT(%d) of PCIECFGREG_SPROM_CTRL(0x%x)\n",
		PCIE_CFG_SPROM_CTRL_SB_RESET_BIT, PCIECFGREG_SPROM_CTRL));
	val = OSL_PCI_READ_CONFIG(bus->osh, PCIECFGREG_SPROM_CTRL, sizeof(val));
	DHD_INFO(("read_config: reg=0x%x read val=0x%x\n", PCIECFGREG_SPROM_CTRL, val));
	val = val | (1 << PCIE_CFG_SPROM_CTRL_SB_RESET_BIT);
	DHD_INFO(("write_config: reg=0x%x write val=0x%x\n", PCIECFGREG_SPROM_CTRL, val));
	OSL_PCI_WRITE_CONFIG(bus->osh, PCIECFGREG_SPROM_CTRL, sizeof(val), val);

	/* Wait till bit backplane reset is ASSERTED i,e
	 * bit 10 of PCIECFGREG_SPROM_CTRL is cleared.
	 * Only after this, poll for 21st bit of DAR reg 0xAE0 is valid
	 * else DAR register will read previous old value
	 */
	DHD_INFO(("Wait till PCIE_CFG_SPROM_CTRL_SB_RESET_BIT(%d) of "
		"PCIECFGREG_SPROM_CTRL(0x%x) is cleared\n",
		PCIE_CFG_SPROM_CTRL_SB_RESET_BIT, PCIECFGREG_SPROM_CTRL));
	do {
		val = OSL_PCI_READ_CONFIG(bus->osh, PCIECFGREG_SPROM_CTRL, sizeof(val));
		DHD_INFO(("read_config: reg=0x%x read val=0x%x\n", PCIECFGREG_SPROM_CTRL, val));
		cond = val & (1 << PCIE_CFG_SPROM_CTRL_SB_RESET_BIT);
		OSL_DELAY(DHD_BP_RESET_STATUS_RETRY_DELAY);
	} while (cond && (retry++ < DHD_BP_RESET_STATUS_RETRIES));

	if (cond) {
		DHD_ERROR(("ERROR: reg=0x%x bit %d is not cleared\n",
			PCIECFGREG_SPROM_CTRL, PCIE_CFG_SPROM_CTRL_SB_RESET_BIT));
		ret = BCME_ERROR;
		goto aspm_enab;
	}

	/* Wait till bit 21 of dar_clk_ctrl_status_reg is cleared */
	DHD_INFO(("Wait till PCIE_CFG_CLOCK_CTRL_STATUS_BP_RESET_BIT(%d) of "
		"dar_clk_ctrl_status_reg(0x%x) is cleared\n",
		PCIE_CFG_CLOCK_CTRL_STATUS_BP_RESET_BIT, dar_clk_ctrl_status_reg));
	do {
		val = si_corereg(bus->sih, bus->sih->buscoreidx,
			dar_clk_ctrl_status_reg, 0, 0);
		DHD_INFO(("read_dar si_corereg: reg=0x%x read val=0x%x\n",
			dar_clk_ctrl_status_reg, val));
		cond = val & (1 << PCIE_CFG_CLOCK_CTRL_STATUS_BP_RESET_BIT);
		OSL_DELAY(DHD_BP_RESET_STATUS_RETRY_DELAY);
	} while (cond && (retry++ < DHD_BP_RESET_STATUS_RETRIES));

	if (cond) {
		DHD_ERROR(("ERROR: reg=0x%x bit %d is not cleared\n",
			dar_clk_ctrl_status_reg, PCIE_CFG_CLOCK_CTRL_STATUS_BP_RESET_BIT));
		ret = BCME_ERROR;
	}

aspm_enab:
	/* Enable ASPM */
	DHD_INFO(("Enable ASPM: set bit 1 of PCIECFGREG_LINK_STATUS_CTRL(0x%x)\n",
		PCIECFGREG_LINK_STATUS_CTRL));
	val = OSL_PCI_READ_CONFIG(bus->osh, PCIECFGREG_LINK_STATUS_CTRL, sizeof(val));
	DHD_INFO(("read_config: reg=0x%x read val=0x%x\n", PCIECFGREG_LINK_STATUS_CTRL, val));
	val = val | (PCIE_ASPM_L1_ENAB);
	DHD_INFO(("write_config: reg=0x%x write val=0x%x\n", PCIECFGREG_LINK_STATUS_CTRL, val));
	OSL_PCI_WRITE_CONFIG(bus->osh, PCIECFGREG_LINK_STATUS_CTRL, sizeof(val), val);

	DHD_ERROR(("******** BP reset Succedeed ********\n"));

	return ret;
}

#if defined(LINUX) || defined(linux)
int
dhd_bus_devreset(dhd_pub_t *dhdp, uint8 flag)
{
	dhd_bus_t *bus = dhdp->bus;
	int bcmerror = 0;
	unsigned long flags;
	int retry = POWERUP_MAX_RETRY;

	if (flag == TRUE) { /* Turn off WLAN */
		/* Removing Power */
		DHD_ERROR(("%s: == Power OFF ==\n", __FUNCTION__));

		/* wait for other contexts to finish -- if required a call
		* to OSL_DELAY for 1s can be added to give other contexts
		* a chance to finish
		*/
		dhdpcie_advertise_bus_cleanup(bus->dhd);

		if (bus->dhd->busstate != DHD_BUS_DOWN) {
#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
			atomic_set(&bus->dhd->block_bus, TRUE);
			dhd_flush_rx_tx_wq(bus->dhd);
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

#ifdef BCMPCIE_OOB_HOST_WAKE
			/* Clean up any pending host wake IRQ */
			dhd_bus_oob_intr_set(bus->dhd, FALSE);
			dhd_bus_oob_intr_unregister(bus->dhd);
#endif /* BCMPCIE_OOB_HOST_WAKE */
			dhd_os_wd_timer(dhdp, 0);
			dhd_bus_stop(bus, TRUE);
			if (bus->intr) {
				dhdpcie_bus_intr_disable(bus);
				dhdpcie_free_irq(bus);
			}
#if IS_ENABLED(CONFIG_PCIEASPM_ROCKCHIP_WIFI_EXTENSION)
			if (!rk_dhd_bus_pcie_wait_for_l1ss(bus)) {
				DHD_ERROR(("%s: wait for l1ss success\n", __FUNCTION__));
			} else {
				DHD_ERROR(("%s: wait for l1ss failed\n", __FUNCTION__));
			}
#endif /* CONFIG_PCIEASPM_ROCKCHIP_WIFI_EXTENSION */
			dhd_deinit_bus_lp_state_lock(bus);
			dhd_deinit_bar1_switch_lock(bus);
			dhd_deinit_backplane_access_lock(bus);
			dhd_deinit_pwr_req_lock(bus);
#ifdef PCIE_INB_DW
			dhd_deinit_dongle_ds_lock(bus);
#endif /* PCIE_INB_DW */
			dhd_bus_release_dongle(bus);
			dhdpcie_bus_free_resource(bus);
			bcmerror = dhdpcie_bus_disable_device(bus);
			if (bcmerror) {
				DHD_ERROR(("%s: dhdpcie_bus_disable_device: %d\n",
					__FUNCTION__, bcmerror));
#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
				atomic_set(&bus->dhd->block_bus, FALSE);
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */
			}
			/* Clean up protocol data after Bus Master Enable bit clear
			 * so that host can safely unmap DMA and remove the allocated buffers
			 * from the PKTID MAP. Some Applicantion Processors supported
			 * System MMU triggers Kernel panic when they detect to attempt to
			 * DMA-unmapped memory access from the devices which use the
			 * System MMU. Therefore, Kernel panic can be happened since it is
			 * possible that dongle can access to DMA-unmapped memory after
			 * calling the dhd_prot_reset().
			 * For this reason, the dhd_prot_reset() and dhd_clear() functions
			 * should be located after the dhdpcie_bus_disable_device().
			 */
			dhd_prot_reset(dhdp);
			/* XXX Reset dhd_pub_t instance to initial status
			 * for built-in type driver
			 */
			dhd_clear(dhdp);

			bcmerror = dhdpcie_bus_stop_host_dev(bus);
			if (bcmerror) {
				DHD_ERROR(("%s: dhdpcie_bus_stop host_dev failed: %d\n",
					__FUNCTION__, bcmerror));
#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
				atomic_set(&bus->dhd->block_bus, FALSE);
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */
				goto done;
			}

			DHD_GENERAL_LOCK(bus->dhd, flags);
			DHD_ERROR(("%s: making DHD_BUS_DOWN\n", __FUNCTION__));
			bus->dhd->busstate = DHD_BUS_DOWN;
			DHD_GENERAL_UNLOCK(bus->dhd, flags);
#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
			atomic_set(&bus->dhd->block_bus, FALSE);
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */
		} else {
			if (bus->intr) {
				dhdpcie_free_irq(bus);
			}
#ifdef BCMPCIE_OOB_HOST_WAKE
			/* Clean up any pending host wake IRQ */
			dhd_bus_oob_intr_set(bus->dhd, FALSE);
			dhd_bus_oob_intr_unregister(bus->dhd);
#endif /* BCMPCIE_OOB_HOST_WAKE */
			dhd_dpc_kill(bus->dhd);
			if (!bus->no_bus_init) {
				dhd_bus_release_dongle(bus);
				dhdpcie_bus_free_resource(bus);
				bcmerror = dhdpcie_bus_disable_device(bus);
				if (bcmerror) {
					DHD_ERROR(("%s: dhdpcie_bus_disable_device: %d\n",
						__FUNCTION__, bcmerror));
				}

				/* Clean up protocol data after Bus Master Enable bit clear
				 * so that host can safely unmap DMA and remove the allocated
				 * buffers from the PKTID MAP. Some Applicantion Processors
				 * supported System MMU triggers Kernel panic when they detect
				 * to attempt to DMA-unmapped memory access from the devices
				 * which use the System MMU.
				 * Therefore, Kernel panic can be happened since it is possible
				 * that dongle can access to DMA-unmapped memory after calling
				 * the dhd_prot_reset().
				 * For this reason, the dhd_prot_reset() and dhd_clear() functions
				 * should be located after the dhdpcie_bus_disable_device().
				 */
				dhd_prot_reset(dhdp);
				/* XXX Reset dhd_pub_t instance to initial status
				 * for built-in type driver
				 */
				dhd_clear(dhdp);
			} else {
				bus->no_bus_init = FALSE;
			}

			bcmerror = dhdpcie_bus_stop_host_dev(bus);
			if (bcmerror) {
				DHD_ERROR(("%s: dhdpcie_bus_stop_host_dev failed: %d\n",
					__FUNCTION__, bcmerror));
				goto done;
			}
		}

		bus->dhd->dongle_reset = TRUE;
		DHD_ERROR(("%s:  WLAN OFF Done\n", __FUNCTION__));

	} else { /* Turn on WLAN */
		if (bus->dhd->busstate == DHD_BUS_DOWN) {
			/* Powering On */
			DHD_ERROR(("%s: == Power ON ==\n", __FUNCTION__));
			/* PCIe RC Turn on */
			do {
#ifdef CONFIG_ARCH_ROCKCHIP
				if (bus->rc_dev) {
					bcmerror = rockchip_dw_pcie_pm_ctrl_for_user(bus->rc_dev, ROCKCHIP_PCIE_PM_CTRL_RESET);
					if (bcmerror) {
						DHD_ERROR(("%s Failed to bring up PCIe link\n", __FUNCTION__));
						OSL_SLEEP(10);
						continue;
					}

				}
#endif /* CONFIG_ARCH_ROCKCHIP */
				bcmerror = dhdpcie_bus_start_host_dev(bus);
				if (!bcmerror) {
					DHD_ERROR_MEM(("%s: dhdpcie_bus_start_host_dev OK\n",
						__FUNCTION__));
					break;
				} else {
					OSL_SLEEP(10);
				}
			} while (retry--);

			if (bcmerror) {
				DHD_ERROR(("%s: host pcie clock enable failed: %d\n",
					__FUNCTION__, bcmerror));
				goto done;
			}
#if defined(DHD_CONTROL_PCIE_ASPM_WIFI_TURNON)
			dhd_bus_aspm_enable_rc_ep(bus, FALSE);
#endif /* DHD_CONTROL_PCIE_ASPM_WIFI_TURNON */
			bus->is_linkdown = 0;
			bus->cto_triggered = 0;
			bcmerror = dhdpcie_bus_enable_device(bus);
			if (bcmerror) {
				DHD_ERROR(("%s: host configuration restore failed: %d\n",
					__FUNCTION__, bcmerror));
				goto done;
			}

			bcmerror = dhdpcie_bus_alloc_resource(bus);
			if (bcmerror) {
				DHD_ERROR(("%s: dhdpcie_bus_resource_alloc failed: %d\n",
					__FUNCTION__, bcmerror));
				goto done;
			}

#ifdef FORCE_DONGLE_RESET_IN_DEVRESET_ON
			/*
			 * This will be enabled from phone platforms to
			 * reset dongle during Wifi ON
			 */
			dhdpcie_dongle_reset(bus);
#endif /* FORCE_DONGLE_RESET_IN_DEVRESET_ON */

			bcmerror = dhdpcie_bus_dongle_attach(bus);
			if (bcmerror) {
				DHD_ERROR(("%s: dhdpcie_bus_dongle_attach failed: %d\n",
					__FUNCTION__, bcmerror));
				goto done;
			}

			bcmerror = dhd_bus_request_irq(bus);
			if (bcmerror) {
				DHD_ERROR(("%s: dhd_bus_request_irq failed: %d\n",
					__FUNCTION__, bcmerror));
				goto done;
			}

			bus->dhd->dongle_reset = FALSE;

#if defined(DHD_CONTROL_PCIE_CPUCORE_WIFI_TURNON)
			dhd_irq_set_affinity(bus->dhd, cpumask_of(1));
#endif /* DHD_CONTROL_PCIE_CPUCORE_WIFI_TURNON */

			bcmerror = dhd_bus_start(dhdp);
			if (bcmerror) {
				DHD_ERROR(("%s: dhd_bus_start: %d\n",
					__FUNCTION__, bcmerror));
				goto done;
			}

			/* Renabling watchdog which is disabled in dhdpcie_advertise_bus_cleanup */
			if (bus->dhd->dhd_watchdog_ms_backup) {
				DHD_ERROR(("%s: Enabling wdtick after dhd init\n",
					__FUNCTION__));
				dhd_os_wd_timer(bus->dhd, bus->dhd->dhd_watchdog_ms_backup);
			}
			DHD_ERROR(("%s: WLAN Power On Done\n", __FUNCTION__));
		} else {
			DHD_ERROR(("%s: what should we do here\n", __FUNCTION__));
			goto done;
		}
	}

done:
	return bcmerror;
}
#else
int
dhd_bus_devreset(dhd_pub_t *dhdp, uint8 flag)
{
	dhd_bus_t *bus = dhdp->bus;
	int bcmerror = 0;
	unsigned long flags;

	if (flag == TRUE) {
		/* Removing Power */
		if (!dhdp->dongle_reset) {
			DHD_ERROR(("%s: == Power OFF ==\n", __FUNCTION__));
			dhdpcie_advertise_bus_cleanup(bus->dhd);
			dhd_os_sdlock(dhdp);
			dhd_os_wd_timer(dhdp, 0);
			dhd_bus_stop(bus, FALSE);
			dhd_prot_reset(dhdp);

			dhdpcie_bus_release_dongle(bus, bus->dhd->osh,
				bus->dhd->dongle_isolation, TRUE);
			bus->dhd->dongle_reset = TRUE;

			dhd_os_sdunlock(dhdp);

			DHD_GENERAL_LOCK(bus->dhd, flags);
			DHD_ERROR(("%s: making DHD_BUS_DOWN\n", __FUNCTION__));
			bus->dhd->busstate = DHD_BUS_DOWN;
			DHD_GENERAL_UNLOCK(bus->dhd, flags);

			DHD_ERROR(("%s:  WLAN OFF Done\n", __FUNCTION__));
		} else {
			DHD_ERROR(("%s: Dongle is already in RESET!\n", __FUNCTION__));
			bcmerror = BCME_DONGLE_DOWN;
		}
	} else {
		/* Powering On */
		DHD_ERROR(("%s: == Power ON ==\n", __FUNCTION__));

		if (bus->dhd->dongle_reset) {
			dhd_os_sdlock(dhdp); /* Turn on WLAN */

			if (dhdpcie_dongle_attach(bus)) {
				DHD_ERROR(("%s: dhdpcie_probe_attach failed\n", __FUNCTION__));
				dhd_os_sdunlock(dhdp);
				return BCME_DONGLE_DOWN;
			}
			DHD_GENERAL_LOCK(bus->dhd, flags);
			DHD_ERROR(("%s: making DHD_BUS_DOWN\n", __FUNCTION__));
			bus->dhd->busstate = DHD_BUS_DOWN;
			DHD_GENERAL_UNLOCK(bus->dhd, flags);

			DHD_INFO(("%s: About to download firmware\n", __FUNCTION__));
			if (dhd_bus_download_firmware(bus, bus->dhd->osh,
				bus->fw_path, bus->nv_path) == 0) {

				bcmerror = dhd_bus_init((dhd_pub_t *) bus->dhd, FALSE);
				if (bcmerror == BCME_OK) {
					bus->dhd->dongle_reset = FALSE;

					dhd_os_wd_timer(dhdp, dhd_watchdog_ms);
					DHD_ERROR(("%s: WLAN Power On Done\n", __FUNCTION__));
				} else {
					DHD_ERROR(("%s: dhd_bus_init FAILed\n", __FUNCTION__));
					dhd_bus_stop(bus, FALSE);
				}
			} else {
				DHD_ERROR(("%s: dhd_bus_download_firmware FAILed\n", __FUNCTION__));
				bcmerror = BCME_DONGLE_DOWN;
			}

			dhd_os_sdunlock(dhdp);
		} else {
			bcmerror = BCME_DONGLE_DOWN;
			DHD_ERROR(("%s called when dongle is not in reset\n", __FUNCTION__));
		}
	}
	return bcmerror;
}
#endif /* LINUX || linux */

#ifdef DHD_PCIE_REG_ACCESS
static int
pcie2_mdioop(dhd_bus_t *bus, uint physmedia, uint regaddr, bool write, uint *val,
	bool slave_bypass)
{
	uint pcie_serdes_spinwait = 200, i = 0, mdio_ctrl;
	uint32 reg32;

	pcie2_mdiosetblock(bus, physmedia);

	/* enable mdio access to SERDES */
	mdio_ctrl = MDIOCTL2_DIVISOR_VAL;
	mdio_ctrl |= (regaddr << MDIOCTL2_REGADDR_SHF);

	if (slave_bypass)
		mdio_ctrl |= MDIOCTL2_SLAVE_BYPASS;

	if (!write)
		mdio_ctrl |= MDIOCTL2_READ;

	si_corereg(bus->sih, bus->sih->buscoreidx, PCIE2_MDIO_CONTROL, ~0, mdio_ctrl);

	if (write) {
		reg32 =  PCIE2_MDIO_WR_DATA;
		si_corereg(bus->sih, bus->sih->buscoreidx, PCIE2_MDIO_WR_DATA, ~0,
			*val | MDIODATA2_DONE);
	} else
		reg32 =  PCIE2_MDIO_RD_DATA;

	/* retry till the transaction is complete */
	while (i < pcie_serdes_spinwait) {
		uint done_val =  si_corereg(bus->sih, bus->sih->buscoreidx, reg32, 0, 0);
		if (!(done_val & MDIODATA2_DONE)) {
			if (!write) {
				*val = si_corereg(bus->sih, bus->sih->buscoreidx,
					PCIE2_MDIO_RD_DATA, 0, 0);
				*val = *val & MDIODATA2_MASK;
			}
			return 0;
		}
		OSL_DELAY(1000);
		i++;
	}
	return -1;
}
#endif /* DHD_PCIE_REG_ACCESS */

static int
dhdpcie_get_dma_ring_indices(dhd_pub_t *dhd)
{
	int h2d_support, d2h_support;

	d2h_support = dhd->dma_d2h_ring_upd_support ? 1 : 0;
	h2d_support = dhd->dma_h2d_ring_upd_support ? 1 : 0;
	return (d2h_support | (h2d_support << 1));

}
int
dhdpcie_set_dma_ring_indices(dhd_pub_t *dhd, int32 int_val)
{
	int bcmerror = 0;
	/* Can change it only during initialization/FW download */
	if (dhd->busstate == DHD_BUS_DOWN) {
		if ((int_val > 3) || (int_val < 0)) {
			DHD_ERROR(("Bad argument. Possible values: 0, 1, 2 & 3\n"));
			bcmerror = BCME_BADARG;
		} else {
			dhd->dma_d2h_ring_upd_support = (int_val & 1) ? TRUE : FALSE;
			dhd->dma_h2d_ring_upd_support = (int_val & 2) ? TRUE : FALSE;
			dhd->dma_ring_upd_overwrite = TRUE;
		}
	} else {
		DHD_ERROR(("%s: Can change only when bus down (before FW download)\n",
			__FUNCTION__));
		bcmerror = BCME_NOTDOWN;
	}

	return bcmerror;

}

/* si_backplane_access() manages a shared resource - BAR0 mapping, hence its
 * calls shall be serialized. This wrapper function provides such serialization
 * and shall be used everywjer einstead of direct call of si_backplane_access()
 *
 * Linux DHD driver calls si_backplane_access() from 3 three contexts: tasklet
 * (that may call dhdpcie_sssr_dump() from dhdpcie_sssr_dump()), iovar
 * ("sbreg", "membyres", etc.) and procfs (used by GDB proxy). To avoid race
 * conditions calls of si_backplane_access() shall be serialized. Presence of
 * tasklet context implies that serialization shall b ebased on spinlock. Hence
 * Linux implementation of dhd_pcie_backplane_access_[un]lock() is
 * spinlock-based.
 *
 * Other platforms may add their own implementations of
 * dhd_pcie_backplane_access_[un]lock() as needed (e.g. if serialization is not
 * needed implementation might be empty)
 */
static uint
serialized_backplane_access(dhd_bus_t *bus, uint addr, uint size, uint *val, bool read)
{
	uint ret;
	unsigned long flags;
	DHD_BACKPLANE_ACCESS_LOCK(bus->backplane_access_lock, flags);
	ret = si_backplane_access(bus->sih, addr, size, val, read);
	DHD_BACKPLANE_ACCESS_UNLOCK(bus->backplane_access_lock, flags);
	return ret;
}

#ifndef DHD_CAP_PLATFORM
#define DHD_CAP_PLATFORM "x86 "
#endif

#ifndef DHD_CAP_CUSTOMER
#define DHD_CAP_CUSTOMER "brcm "
#endif

void
BCMRAMFN(dhd_cap_bcmstrbuf)(dhd_pub_t *dhd, struct bcmstrbuf *b)
{
	bcm_bprintf(b, DHD_CAP_PLATFORM);
	bcm_bprintf(b, DHD_CAP_CUSTOMER);
#ifdef PCIE_FULL_DONGLE
	bcm_bprintf(b, "pcie ");
#endif /* PCIE_FULL_DONGLE */
	/* regaccess and memaccess will be present only for internal reference builds @brcm */
#ifdef DHD_NO_MOG
	bcm_bprintf(b, "internal ");
#else
	bcm_bprintf(b, "external ");
#endif /* DHD_NO_MOG */
#ifdef WLAN_ACCEL_BOOT
	bcm_bprintf(b, "wlan-accel ");
#endif /* WLAN_ACCEL_BOOT */
#ifdef ENABLE_DHD_GRO
	bcm_bprintf(b, "gro ");
#endif /* ENABLE_DHD_GRO */
#ifdef WBRC
	bcm_bprintf(b, "wbrc ");
#endif /* ENABLE_DHD_GRO */
#ifdef WL_CFG80211
	bcm_bprintf(b, "cfg80211 ");
#endif /* WL_CFG80211 */
#ifdef DHD_FILE_DUMP_EVENT
	bcm_bprintf(b, "haldump ");
#endif /* DHD_FILE_DUMP_EVENT */
#ifdef DHD_LB_RXP
	bcm_bprintf(b, "lbrxp ");
#endif /* DHD_LB_RXP */
#ifdef DHD_LB_TXP
#ifdef DHD_LB_TXP_DEFAULT_ENAB
	bcm_bprintf(b, "lbtxp ");
#endif /* DHD_LB_TXP_DEFAULT_ENAB */
#endif /* DHD_LB_TXP */
	if (dhd->htput_support) {
		bcm_bprintf(b, "htput ");
	}
}

/** Return dhd capability string */
static char*
dhd_cap(dhd_pub_t *dhd, char *buf, uint bufsize)
{
	struct bcmstrbuf b;

	bcm_binit(&b, buf, bufsize);

	dhd_cap_bcmstrbuf(dhd, &b);

	/* this is either full or overflow. return error */
	if (b.size <= 1)
		return NULL;

	return (buf);
}

/**
 * IOVAR handler of the DHD bus layer (in this case, the PCIe bus).
 *
 * @param actionid  e.g. IOV_SVAL(IOV_PCIEREG)
 * @param params    input buffer
 * @param plen      length in [bytes] of input buffer 'params'
 * @param arg       output buffer
 * @param len       length in [bytes] of output buffer 'arg'
 */
static int
dhdpcie_bus_doiovar(dhd_bus_t *bus, const bcm_iovar_t *vi, uint32 actionid, const char *name,
                void *params, uint plen, void *arg, uint len, int val_size)
{
	int bcmerror = 0;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 int_val3 = 0;
	bool bool_val = 0;

	DHD_TRACE(("%s: Enter, action %d name %s params %p plen %d arg %p len %d val_size %d\n",
	           __FUNCTION__, actionid, name, params, plen, arg, len, val_size));

	if ((bcmerror = bcm_iovar_lencheck(vi, arg, len, IOV_ISSET(actionid))) != 0)
		goto exit;

	if (plen >= sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	if (plen >= sizeof(int_val) * 2)
		bcopy((void*)((uintptr)params + sizeof(int_val)), &int_val2, sizeof(int_val2));

	if (plen >= sizeof(int_val) * 3)
		bcopy((void*)((uintptr)params + 2 * sizeof(int_val)), &int_val3, sizeof(int_val3));

	bool_val = (int_val != 0) ? TRUE : FALSE;

	/* Check if dongle is in reset. If so, only allow DEVRESET iovars */
	if (bus->dhd->dongle_reset && !(actionid == IOV_SVAL(IOV_DEVRESET) ||
	                                actionid == IOV_GVAL(IOV_DEVRESET))) {
		bcmerror = BCME_NOTREADY;
		goto exit;
	}

	switch (actionid) {

	case IOV_SVAL(IOV_VARS):
		bcmerror = dhdpcie_downloadvars(bus, arg, len);
		break;
#ifdef DHD_PCIE_REG_ACCESS
	case IOV_SVAL(IOV_PCIEREG):
		/* XXX: validate int_val ??? */
		si_corereg(bus->sih, bus->sih->buscoreidx, OFFSETOF(sbpcieregs_t, configaddr), ~0,
			int_val);
		si_corereg(bus->sih, bus->sih->buscoreidx, OFFSETOF(sbpcieregs_t, configdata), ~0,
			int_val2);
		break;

	case IOV_GVAL(IOV_PCIEREG):
		/* XXX: validate int_val ??? */
		si_corereg(bus->sih, bus->sih->buscoreidx, OFFSETOF(sbpcieregs_t, configaddr), ~0,
			int_val);
		int_val = si_corereg(bus->sih, bus->sih->buscoreidx,
			OFFSETOF(sbpcieregs_t, configdata), 0, 0);
		bcopy(&int_val, arg, sizeof(int_val));
		break;

	case IOV_SVAL(IOV_PCIECOREREG):
		/* XXX: validate int_val ??? */
		si_corereg(bus->sih, bus->sih->buscoreidx, int_val, ~0, int_val2);
		break;
	case IOV_GVAL(IOV_BAR0_SECWIN_REG):
	{
		sdreg_t sdreg;
		uint32 addr, size;

		bcopy(params, &sdreg, sizeof(sdreg));

		addr = sdreg.offset;
		size = sdreg.func;

		if (serialized_backplane_access(bus, addr, size, (uint *)&int_val, TRUE) != BCME_OK)
		{
			DHD_ERROR(("Invalid size/addr combination \n"));
			bcmerror = BCME_ERROR;
			break;
		}
		bcopy(&int_val, arg, sizeof(int32));
		break;
	}

	case IOV_SVAL(IOV_BAR0_SECWIN_REG):
	{
		sdreg_t sdreg;
		uint32 addr, size;

		bcopy(params, &sdreg, sizeof(sdreg));

		addr = sdreg.offset;
		size = sdreg.func;
		if (serialized_backplane_access(bus, addr, size,
			(uint *)(&sdreg.value), FALSE) != BCME_OK) {
			DHD_ERROR(("Invalid size/addr combination \n"));
			bcmerror = BCME_ERROR;
		}
		break;
	}

	case IOV_GVAL(IOV_SBREG):
	{
		sdreg_t sdreg;
		uint32 addr, size;

		bcopy(params, &sdreg, sizeof(sdreg));

		addr = sdreg.offset | SI_ENUM_BASE(bus->sih);
		size = sdreg.func;

		if (serialized_backplane_access(bus, addr, size, (uint *)&int_val, TRUE) != BCME_OK)
		{
			DHD_ERROR(("Invalid size/addr combination \n"));
			bcmerror = BCME_ERROR;
			break;
		}
		bcopy(&int_val, arg, size);
		break;
	}

	case IOV_SVAL(IOV_SBREG):
	{
		sdreg_t sdreg;
		uint32 addr, size;

		bcopy(params, &sdreg, sizeof(sdreg));

		addr = sdreg.offset | SI_ENUM_BASE(bus->sih);
		size = sdreg.func;
		if (serialized_backplane_access(bus, addr, size,
			(uint *)(&sdreg.value), FALSE) != BCME_OK) {
			DHD_ERROR(("Invalid size/addr combination \n"));
			bcmerror = BCME_ERROR;
		}
		break;
	}

	case IOV_GVAL(IOV_PCIESERDESREG):
	{
		uint val;
		if (!PCIE_GEN2(bus->sih)) {
			DHD_ERROR(("supported only in pcie gen2\n"));
			bcmerror = BCME_ERROR;
			break;
		}

		if (!pcie2_mdioop(bus, int_val, int_val2, FALSE, &val, FALSE)) {
			bcopy(&val, arg, sizeof(int32));
		} else {
			DHD_ERROR(("pcie2_mdioop failed.\n"));
			bcmerror = BCME_ERROR;
		}
		break;
	}

	case IOV_SVAL(IOV_PCIESERDESREG):
		if (!PCIE_GEN2(bus->sih)) {
			DHD_ERROR(("supported only in pcie gen2\n"));
			bcmerror = BCME_ERROR;
			break;
		}
		if (pcie2_mdioop(bus, int_val, int_val2, TRUE, (uint *)&int_val3, FALSE)) {
			DHD_ERROR(("pcie2_mdioop failed.\n"));
			bcmerror = BCME_ERROR;
		}
		break;
	case IOV_GVAL(IOV_PCIECOREREG):
		/* XXX: validate int_val ??? */
		int_val = si_corereg(bus->sih, bus->sih->buscoreidx, int_val, 0, 0);
		bcopy(&int_val, arg, sizeof(int_val));
		break;

	case IOV_SVAL(IOV_PCIECFGREG):
		/* XXX: validate int_val ??? */
		OSL_PCI_WRITE_CONFIG(bus->osh, int_val, 4, int_val2);
		break;

	case IOV_GVAL(IOV_PCIECFGREG):
		/* XXX: validate int_val ??? */
		int_val = OSL_PCI_READ_CONFIG(bus->osh, int_val, 4);
		bcopy(&int_val, arg, sizeof(int_val));
		break;
#endif /* DHD_PCIE_REG_ACCESS */
	case IOV_SVAL(IOV_PCIE_LPBK):
		bcmerror = dhdpcie_bus_lpback_req(bus, int_val);
		break;

	case IOV_SVAL(IOV_PCIE_DMAXFER): {
		dma_xfer_info_t *dmaxfer = (dma_xfer_info_t *)arg;
		uint32 mem_addr;

		if (!dmaxfer)
			return BCME_BADARG;
		if (dmaxfer->version != DHD_DMAXFER_VERSION)
			return BCME_VERSION;
		if (dmaxfer->length != sizeof(dma_xfer_info_t)) {
			return BCME_BADLEN;
		}

		mem_addr = (uint32)dmaxfer->tput;
		dmaxfer->tput = 0;
		bcmerror = dhdpcie_bus_dmaxfer_req(bus, dmaxfer->num_bytes,
				dmaxfer->src_delay, dmaxfer->dest_delay,
				dmaxfer->type, dmaxfer->core_num,
				dmaxfer->should_wait, mem_addr);

		if (dmaxfer->should_wait && bcmerror >= 0) {
			bcmerror = dhdmsgbuf_dmaxfer_status(bus->dhd, dmaxfer);
		}
		break;
	}

	case IOV_GVAL(IOV_PCIE_DMAXFER): {
		dma_xfer_info_t *dmaxfer = (dma_xfer_info_t *)params;
		if (!dmaxfer)
			return BCME_BADARG;
		if (dmaxfer->version != DHD_DMAXFER_VERSION)
			return BCME_VERSION;
		if (dmaxfer->length != sizeof(dma_xfer_info_t)) {
			return BCME_BADLEN;
		}
		bcmerror = dhdmsgbuf_dmaxfer_status(bus->dhd, dmaxfer);
		break;
	}
	case IOV_GVAL(IOV_PCIE_DMAXFER_PTRN): {
		int_val = bus->lpbk_xfer_data_pattern_type;
		bcopy(&int_val, arg, val_size);
		break;
	}
	case IOV_SVAL(IOV_PCIE_DMAXFER_PTRN): {
		bus->lpbk_xfer_data_pattern_type = int_val;
		break;
	}

#ifdef PCIE_OOB
	case IOV_GVAL(IOV_OOB_BT_REG_ON):
		int_val = dhd_oob_get_bt_reg_on(bus);
		bcopy(&int_val, arg, val_size);
		break;
	case IOV_SVAL(IOV_OOB_BT_REG_ON):
		dhd_oob_set_bt_reg_on(bus, (uint8)int_val);
		break;
	case IOV_GVAL(IOV_OOB_ENABLE):
		int_val = bus->oob_enabled;
		bcopy(&int_val, arg, val_size);
		break;
	case IOV_SVAL(IOV_OOB_ENABLE):
		bus->oob_enabled = (bool)int_val;
		break;
#endif /* PCIE_OOB */
#ifdef PCIE_INB_DW
	case IOV_GVAL(IOV_INB_DW_ENABLE):
		int_val = bus->inb_enabled;
		bcopy(&int_val, arg, val_size);
		break;
	case IOV_SVAL(IOV_INB_DW_ENABLE):
		bus->inb_enabled = (bool)int_val;
		break;
#endif /* PCIE_INB_DW */
#if defined(PCIE_OOB) || defined(PCIE_INB_DW)
	case IOV_GVAL(IOV_DEEP_SLEEP):
		int_val = bus->ds_enabled;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_DEEP_SLEEP):
		if (int_val == 1) {
			bus->deep_sleep = TRUE;
			if (!bus->ds_enabled) {
				bus->ds_enabled = TRUE;
				/* Deassert */
				if (dhd_bus_set_device_wake(bus, FALSE) == BCME_OK) {
#ifdef PCIE_INB_DW
					if (INBAND_DW_ENAB(bus)) {
						int timeleft;
						timeleft = dhd_os_ds_enter_wait(bus->dhd, NULL);
						if (timeleft == 0) {
							DHD_ERROR(("DS-ENTER timeout\n"));
							bus->ds_enabled = FALSE;
							break;
						}
					}
#endif /* PCIE_INB_DW */
				}
				else {
					DHD_ERROR(("%s: Enable Deep Sleep failed !\n",
							__FUNCTION__));
					bus->ds_enabled = FALSE;
				}
			} else {
				DHD_ERROR(("%s: Deep Sleep already enabled !\n", __FUNCTION__));
			}
		}
		else if (int_val == 0) {
			bus->deep_sleep = FALSE;
			if (bus->ds_enabled) {
				bus->calc_ds_exit_latency = TRUE;
				/* Assert */
				if (dhd_bus_set_device_wake(bus, TRUE) == BCME_OK) {
					bus->ds_enabled = FALSE;
					if (INBAND_DW_ENAB(bus)) {
						if (bus->ds_exit_latency != 0) {
							DHD_ERROR(("DS-EXIT latency = %llu us\n",
								bus->ds_exit_latency));
						} else {
							DHD_ERROR(("Failed to measure DS-EXIT"
								" latency!(Possibly a non"
								" waitable context)\n"));
						}
					}
				} else {
					DHD_ERROR(("%s: Disable Deep Sleep failed !\n",
						__FUNCTION__));
				}
				bus->calc_ds_exit_latency = FALSE;
			} else {
				DHD_ERROR(("%s: Deep Sleep already disabled !\n", __FUNCTION__));
			}
		}
		else
			DHD_ERROR(("%s: Invalid number, allowed only 0|1\n", __FUNCTION__));

		break;
#endif /* PCIE_OOB || PCIE_INB_DW */
#ifdef DEVICE_TX_STUCK_DETECT
	case IOV_GVAL(IOV_DEVICE_TX_STUCK_DETECT):
		int_val = bus->dev_tx_stuck_monitor;
		bcopy(&int_val, arg, val_size);
		break;
	case IOV_SVAL(IOV_DEVICE_TX_STUCK_DETECT):
		bus->dev_tx_stuck_monitor = (bool)int_val;
		break;
#endif /* DEVICE_TX_STUCK_DETECT */
	case IOV_GVAL(IOV_PCIE_SUSPEND):
		int_val = (bus->dhd->busstate == DHD_BUS_SUSPEND) ? 1 : 0;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_PCIE_SUSPEND):
		if (bool_val) { /* Suspend */
			int ret;
			unsigned long flags;

			/*
			 * If some other context is busy, wait until they are done,
			 * before starting suspend
			 */
			ret = dhd_os_busbusy_wait_condition(bus->dhd,
				&bus->dhd->dhd_bus_busy_state, DHD_BUS_BUSY_IN_DHD_IOVAR);
			if (ret == 0) {
				DHD_ERROR(("%s:Wait Timedout, dhd_bus_busy_state = 0x%x\n",
					__FUNCTION__, bus->dhd->dhd_bus_busy_state));
				return BCME_BUSY;
			}

			DHD_GENERAL_LOCK(bus->dhd, flags);
			DHD_BUS_BUSY_SET_SUSPEND_IN_PROGRESS(bus->dhd);
			DHD_GENERAL_UNLOCK(bus->dhd, flags);
#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
			dhdpcie_bus_suspend(bus, TRUE, TRUE);
#else
			dhdpcie_bus_suspend(bus, TRUE);
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

			DHD_GENERAL_LOCK(bus->dhd, flags);
			DHD_BUS_BUSY_CLEAR_SUSPEND_IN_PROGRESS(bus->dhd);
			dhd_os_busbusy_wake(bus->dhd);
			DHD_GENERAL_UNLOCK(bus->dhd, flags);
		} else { /* Resume */
			unsigned long flags;
			DHD_GENERAL_LOCK(bus->dhd, flags);
			DHD_BUS_BUSY_SET_RESUME_IN_PROGRESS(bus->dhd);
			DHD_GENERAL_UNLOCK(bus->dhd, flags);

			dhdpcie_bus_suspend(bus, FALSE);

			DHD_GENERAL_LOCK(bus->dhd, flags);
			DHD_BUS_BUSY_CLEAR_RESUME_IN_PROGRESS(bus->dhd);
			dhd_os_busbusy_wake(bus->dhd);
			DHD_GENERAL_UNLOCK(bus->dhd, flags);
		}
		break;

	case IOV_GVAL(IOV_MEMSIZE):
		int_val = (int32)bus->ramsize;
		bcopy(&int_val, arg, val_size);
		break;
#ifdef DHD_BUS_MEM_ACCESS
	case IOV_SVAL(IOV_MEMBYTES):
	case IOV_GVAL(IOV_MEMBYTES):
	{
		uint32 address;		/* absolute backplane address */
		uint size, dsize;
		uint8 *data;

		bool set = (actionid == IOV_SVAL(IOV_MEMBYTES));

		ASSERT(plen >= 2*sizeof(int));

		address = (uint32)int_val;
		bcopy((char *)params + sizeof(int_val), &int_val, sizeof(int_val));
		size = (uint)int_val;

		/* Do some validation */
		dsize = set ? plen - (2 * sizeof(int)) : len;
		if (dsize < size) {
			DHD_ERROR(("%s: error on %s membytes, addr 0x%08x size %d dsize %d\n",
			           __FUNCTION__, (set ? "set" : "get"), address, size, dsize));
			bcmerror = BCME_BADARG;
			break;
		}

		DHD_INFO(("%s: Request to %s %d bytes at address 0x%08x\n dsize %d ", __FUNCTION__,
		          (set ? "write" : "read"), size, address, dsize));

		/* check if CR4 */
		if (si_setcore(bus->sih, ARMCR4_CORE_ID, 0) ||
		    si_setcore(bus->sih, ARMCA7_CORE_ID, 0) ||
		    si_setcore(bus->sih, SYSMEM_CORE_ID, 0)) {
			/* if address is 0, store the reset instruction to be written in 0 */
			if (set && address == bus->dongle_ram_base) {
				bus->resetinstr = *(((uint32*)params) + 2);
			}
		}

		/* Generate the actual data pointer */
		data = set ? (uint8*)params + 2 * sizeof(int): (uint8*)arg;

		/* Call to do the transfer */
		bcmerror = dhdpcie_bus_membytes(bus, set, address, data, size);

		break;
	}
#endif /* DHD_BUS_MEM_ACCESS */

	/* Debug related. Dumps core registers or one of the dongle memory */
	case IOV_GVAL(IOV_DUMP_DONGLE):
	{
		dump_dongle_in_t ddi = *(dump_dongle_in_t*)params;
		dump_dongle_out_t *ddo = (dump_dongle_out_t*)arg;
		uint32 *p = ddo->val;
		const uint max_offset = 4096 - 1; /* one core contains max 4096/4 registers */

		if (plen < sizeof(ddi) || len < sizeof(ddo)) {
			bcmerror = BCME_BADARG;
			break;
		}

		switch (ddi.type) {
		case DUMP_DONGLE_COREREG:
			ddo->n_bytes = 0;

			if (si_setcoreidx(bus->sih, ddi.index) == NULL) {
				break; // beyond last core: core enumeration ended
			}

			ddo->address = si_addrspace(bus->sih, CORE_SLAVE_PORT_0, CORE_BASE_ADDR_0);
			ddo->address += ddi.offset; // BP address at which this dump starts

			ddo->id = si_coreid(bus->sih);
			ddo->rev = si_corerev(bus->sih);

			while (ddi.offset < max_offset &&
				sizeof(dump_dongle_out_t) + ddo->n_bytes < (uint)len) {
				*p++ = si_corereg(bus->sih, ddi.index, ddi.offset, 0, 0);
				ddi.offset += sizeof(uint32);
				ddo->n_bytes += sizeof(uint32);
			}
			break;
		default:
			// TODO: implement d11 SHM/TPL dumping
			bcmerror = BCME_BADARG;
			break;
		}
		break;
	}

	/* Debug related. Returns a string with dongle capabilities */
	case IOV_GVAL(IOV_DHD_CAPS):
	{
		if (dhd_cap(bus->dhd, (char*)arg, len) == NULL) {
			bcmerror = BCME_BUFTOOSHORT;
		}
		break;
	}

#if defined(DEBUGGER) || defined(DHD_DSCOPE)
	case IOV_SVAL(IOV_GDB_SERVER):
		/* debugger_*() functions may sleep, so cannot hold spinlock */
		if (int_val > 0) {
			debugger_init((void *) bus, &bus_ops, int_val, SI_ENUM_BASE(bus->sih));
		} else {
			debugger_close();
		}
		break;
#endif /* DEBUGGER || DHD_DSCOPE */
#if defined(GDB_PROXY)
	case IOV_GVAL(IOV_GDB_PROXY_PROBE):
	{
		dhd_gdb_proxy_probe_data_t ret;
		ret.data_len = (uint32)sizeof(ret);
		ret.magic = DHD_IOCTL_MAGIC;
		ret.flags = 0;
		if (bus->gdb_proxy_access_enabled) {
			ret.flags |= DHD_GDB_PROXY_PROBE_ACCESS_ENABLED;
			if (bus->dhd->busstate < DHD_BUS_LOAD) {
				ret.flags |= DHD_GDB_PROXY_PROBE_FIRMWARE_NOT_RUNNING;
			} else {
				ret.flags |= DHD_GDB_PROXY_PROBE_FIRMWARE_RUNNING;
			}
		}
		if (bus->gdb_proxy_bootloader_mode) {
			ret.flags |= DHD_GDB_PROXY_PROBE_BOOTLOADER_MODE;
		}
		ret.last_id = bus->gdb_proxy_last_id;
		if (bus->hostfw_buf.va) {
			ret.flags |= DHD_GDB_PROXY_PROBE_HOSTMEM_CODE;
			ret.hostmem_code_win_base =
				(uint32)PCIEDEV_ARM_ADDR(PHYSADDRLO(bus->hostfw_buf.pa),
				PCIEDEV_TRANS_WIN_HOSTMEM);
			ret.hostmem_code_win_length = bus->hostfw_buf.len;
		}
		ret.flags |= DHD_GDB_PROXY_PROBE_MEMDUMP_COUNT;
		ret.mem_dump_count = bus->gdb_proxy_mem_dump_count;
		if (plen && int_val) {
			bus->gdb_proxy_last_id = (uint32)int_val;
		}
		if (len >= sizeof(ret)) {
			bcopy(&ret, arg, sizeof(ret));
			bus->dhd->gdb_proxy_active = TRUE;
		} else {
			bcmerror = BCME_BADARG;
		}
		break;
	}
	case IOV_GVAL(IOV_GDB_PROXY_STOP_COUNT):
		int_val = (int32)bus->dhd->gdb_proxy_stop_count;
		bcopy(&int_val, arg, sizeof(int_val));
		break;
	case IOV_SVAL(IOV_GDB_PROXY_STOP_COUNT):
		bus->dhd->gdb_proxy_stop_count = (uint32)int_val;
		break;
#endif /* GDB_PROXY */

#ifdef BCM_BUZZZ
	/* Dump dongle side buzzz trace to console */
	case IOV_GVAL(IOV_BUZZZ_DUMP):
		bcmerror = dhd_buzzz_dump_dngl(bus);
		break;
#endif /* BCM_BUZZZ */

	case IOV_SVAL(IOV_SET_DOWNLOAD_STATE):
		bcmerror = dhdpcie_bus_download_state(bus, bool_val);
		break;

#if defined(FW_SIGNATURE)
	case IOV_SVAL(IOV_SET_DOWNLOAD_INFO):
	{
		fw_download_info_t *info = (fw_download_info_t*)params;
		DHD_INFO(("dwnldinfo: sig=%s fw=%x,%u bl=%s,0x%x\n",
			info->fw_signature_fname,
			info->fw_start_addr, info->fw_size,
			info->bootloader_fname, info->bootloader_start_addr));
		bcmerror = dhdpcie_bus_save_download_info(bus,
			info->fw_start_addr, info->fw_size, info->fw_signature_fname,
			info->bootloader_fname, info->bootloader_start_addr);
		break;
	}
#endif /* FW_SIGNATURE */

	case IOV_GVAL(IOV_RAMSIZE):
		int_val = (int32)bus->ramsize;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_RAMSIZE):
		bus->ramsize = int_val;
		bus->orig_ramsize = int_val;
		break;

	case IOV_GVAL(IOV_RAMSTART):
		int_val = (int32)bus->dongle_ram_base;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_GVAL(IOV_CC_NVMSHADOW):
	{
		struct bcmstrbuf dump_b;

		bcm_binit(&dump_b, arg, len);
		bcmerror = dhdpcie_cc_nvmshadow(bus, &dump_b);
		break;
	}

	case IOV_GVAL(IOV_SLEEP_ALLOWED):
		bool_val = bus->sleep_allowed;
		bcopy(&bool_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_SLEEP_ALLOWED):
		bus->sleep_allowed = bool_val;
		break;

	case IOV_GVAL(IOV_DONGLEISOLATION):
		int_val = bus->dhd->dongle_isolation;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_DONGLEISOLATION):
		bus->dhd->dongle_isolation = bool_val;
		break;

	case IOV_GVAL(IOV_LTRSLEEPON_UNLOOAD):
		int_val = bus->ltrsleep_on_unload;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_LTRSLEEPON_UNLOOAD):
		bus->ltrsleep_on_unload = bool_val;
		break;

	case IOV_GVAL(IOV_DUMP_RINGUPD_BLOCK):
	{
		struct bcmstrbuf dump_b;
		bcm_binit(&dump_b, arg, len);
		bcmerror = dhd_prot_ringupd_dump(bus->dhd, &dump_b);
		break;
	}
	case IOV_GVAL(IOV_DMA_RINGINDICES):
	{
		int_val = dhdpcie_get_dma_ring_indices(bus->dhd);
		bcopy(&int_val, arg, sizeof(int_val));
		break;
	}
	case IOV_SVAL(IOV_DMA_RINGINDICES):
		bcmerror = dhdpcie_set_dma_ring_indices(bus->dhd, int_val);
		break;

	case IOV_GVAL(IOV_METADATA_DBG):
		int_val = dhd_prot_metadata_dbg_get(bus->dhd);
		bcopy(&int_val, arg, val_size);
		break;
	case IOV_SVAL(IOV_METADATA_DBG):
		dhd_prot_metadata_dbg_set(bus->dhd, (int_val != 0));
		break;

	case IOV_GVAL(IOV_RX_METADATALEN):
		int_val = dhd_prot_metadatalen_get(bus->dhd, TRUE);
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_RX_METADATALEN):
#if !defined(BCM_ROUTER_DHD)
		if (int_val > 64) {
			bcmerror = BCME_BUFTOOLONG;
			break;
		}
		dhd_prot_metadatalen_set(bus->dhd, int_val, TRUE);
#else
		bcmerror = BCME_UNSUPPORTED;
#endif /* BCM_ROUTER_DHD */
		break;

	case IOV_SVAL(IOV_TXP_THRESHOLD):
		dhd_prot_txp_threshold(bus->dhd, TRUE, int_val);
		break;

	case IOV_GVAL(IOV_TXP_THRESHOLD):
		int_val = dhd_prot_txp_threshold(bus->dhd, FALSE, int_val);
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_DB1_FOR_MB):
		if (int_val)
			bus->db1_for_mb = TRUE;
		else
			bus->db1_for_mb = FALSE;
		break;

	case IOV_GVAL(IOV_DB1_FOR_MB):
		if (bus->db1_for_mb)
			int_val = 1;
		else
			int_val = 0;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_GVAL(IOV_TX_METADATALEN):
		int_val = dhd_prot_metadatalen_get(bus->dhd, FALSE);
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_TX_METADATALEN):
#if !defined(BCM_ROUTER_DHD)
		if (int_val > 64) {
			bcmerror = BCME_BUFTOOLONG;
			break;
		}
		dhd_prot_metadatalen_set(bus->dhd, int_val, FALSE);
#else
		bcmerror = BCME_UNSUPPORTED;
#endif /* BCM_ROUTER_DHD */
		break;

	case IOV_SVAL(IOV_DEVRESET):
	{
		devreset_info_t *devreset = (devreset_info_t *)arg;

		if (!devreset) {
			return BCME_BADARG;
		}

		if (devreset->length == sizeof(devreset_info_t)) {
			if (devreset->version != DHD_DEVRESET_VERSION) {
				return BCME_VERSION;
			}
			int_val = devreset->mode;
		}

		switch (int_val) {
			case DHD_BUS_DEVRESET_ON:
				bcmerror = dhd_bus_devreset(bus->dhd, (uint8)int_val);
				break;
			case DHD_BUS_DEVRESET_OFF:
				bcmerror = dhd_bus_devreset(bus->dhd, (uint8)int_val);
				break;
#if !defined(NDIS)
			case DHD_BUS_DEVRESET_FLR:
				bcmerror = dhd_bus_perform_flr(bus, bus->flr_force_fail);
				break;
			case DHD_BUS_DEVRESET_FLR_FORCE_FAIL:
				bus->flr_force_fail = TRUE;
				break;
#endif /* !defined(NDIS) */
			default:
				DHD_ERROR(("%s: invalid argument for devreset\n", __FUNCTION__));
				break;
		}
		break;
	}
	case IOV_SVAL(IOV_FORCE_FW_TRAP):
		if (bus->dhd->busstate == DHD_BUS_DATA)
			dhdpcie_fw_trap(bus);
		else {
			DHD_ERROR(("%s: Bus is NOT up\n", __FUNCTION__));
			bcmerror = BCME_NOTUP;
		}
		break;
	case IOV_GVAL(IOV_FLOW_PRIO_MAP):
		int_val = bus->dhd->flow_prio_map_type;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_FLOW_PRIO_MAP):
		int_val = (int32)dhd_update_flow_prio_map(bus->dhd, (uint8)int_val);
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_H2D_MAILBOXDATA):
		dhdpcie_send_mb_data(bus, (uint)int_val);
		break;

	case IOV_SVAL(IOV_INFORINGS):
		dhd_prot_init_info_rings(bus->dhd);
		break;

	case IOV_SVAL(IOV_H2D_PHASE):
		if (bus->dhd->busstate != DHD_BUS_DOWN) {
			DHD_ERROR(("%s: Can change only when bus down (before FW download)\n",
				__FUNCTION__));
			bcmerror = BCME_NOTDOWN;
			break;
		}
		if (int_val)
			bus->dhd->h2d_phase_supported = TRUE;
		else
			bus->dhd->h2d_phase_supported = FALSE;
		break;

	case IOV_GVAL(IOV_H2D_PHASE):
		int_val = (int32) bus->dhd->h2d_phase_supported;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_H2D_ENABLE_TRAP_BADPHASE):
		if (bus->dhd->busstate != DHD_BUS_DOWN) {
			DHD_ERROR(("%s: Can change only when bus down (before FW download)\n",
				__FUNCTION__));
			bcmerror = BCME_NOTDOWN;
			break;
		}
		if (int_val)
			bus->dhd->force_dongletrap_on_bad_h2d_phase = TRUE;
		else
			bus->dhd->force_dongletrap_on_bad_h2d_phase = FALSE;
		break;

	case IOV_GVAL(IOV_H2D_ENABLE_TRAP_BADPHASE):
		int_val = (int32) bus->dhd->force_dongletrap_on_bad_h2d_phase;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_RING_SIZE_VER):
		if (bus->dhd->busstate != DHD_BUS_DOWN) {
			DHD_ERROR(("%s: Can change only when bus down (before FW download)\n",
				__FUNCTION__));
			bcmerror = BCME_NOTDOWN;
			break;
		}
		dhd_prot_set_ring_size_ver(bus->dhd, int_val);
		break;

	case IOV_GVAL(IOV_RING_SIZE_VER):
		int_val = ring_size_version;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_H2D_TXPOST_MAX_ITEM):
		if (bus->dhd->busstate != DHD_BUS_DOWN) {
			DHD_ERROR(("%s: Can change only when bus down (before FW download)\n",
				__FUNCTION__));
			bcmerror = BCME_NOTDOWN;
			break;
		}
		h2d_max_txpost = (uint16)int_val;
		break;

	case IOV_GVAL(IOV_H2D_TXPOST_MAX_ITEM):
		int_val = h2d_max_txpost;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_H2D_HTPUT_TXPOST_MAX_ITEM):
		if (bus->dhd->busstate != DHD_BUS_DOWN) {
			DHD_ERROR(("%s: Can change only when bus down (before FW download)\n",
				__FUNCTION__));
			bcmerror = BCME_NOTDOWN;
			break;
		}
		h2d_htput_max_txpost = (uint16)int_val;
		break;

	case IOV_GVAL(IOV_H2D_HTPUT_TXPOST_MAX_ITEM):
		int_val = h2d_htput_max_txpost;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_D2H_TXCPL_MAX_ITEM):
		if (bus->dhd->busstate != DHD_BUS_DOWN) {
			DHD_ERROR(("%s: Can change only when bus down (before FW download)\n",
				__FUNCTION__));
			bcmerror = BCME_NOTDOWN;
			break;
		}
		d2h_max_txcpl = (uint16)int_val;
		break;

	case IOV_GVAL(IOV_D2H_TXCPL_MAX_ITEM):
		int_val = d2h_max_txcpl;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_H2D_RXPOST_MAX_ITEM):
		if (bus->dhd->busstate != DHD_BUS_DOWN) {
			DHD_ERROR(("%s: Can change only when bus down (before FW download)\n",
				__FUNCTION__));
			bcmerror = BCME_NOTDOWN;
			break;
		}
		h2d_max_rxpost = (uint16)int_val;
		break;

	case IOV_GVAL(IOV_H2D_RXPOST_MAX_ITEM):
		int_val = h2d_max_rxpost;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_D2H_RXCPL_MAX_ITEM):
		if (bus->dhd->busstate != DHD_BUS_DOWN) {
			DHD_ERROR(("%s: Can change only when bus down (before FW download)\n",
				__FUNCTION__));
			bcmerror = BCME_NOTDOWN;
			break;
		}
		d2h_max_rxcpl = (uint16)int_val;
		break;

	case IOV_GVAL(IOV_D2H_RXCPL_MAX_ITEM):
		int_val = d2h_max_rxcpl;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_H2D_CTRLPOST_MAX_ITEM):
		if (bus->dhd->busstate != DHD_BUS_DOWN) {
			DHD_ERROR(("%s: Can change only when bus down (before FW download)\n",
				__FUNCTION__));
			bcmerror = BCME_NOTDOWN;
			break;
		}
		h2d_max_ctrlpost = (uint16)int_val;
		break;

	case IOV_GVAL(IOV_H2D_CTRLPOST_MAX_ITEM):
		int_val = h2d_max_ctrlpost;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_D2H_CTRLCPL_MAX_ITEM):
		if (bus->dhd->busstate != DHD_BUS_DOWN) {
			DHD_ERROR(("%s: Can change only when bus down (before FW download)\n",
				__FUNCTION__));
			bcmerror = BCME_NOTDOWN;
			break;
		}
		d2h_max_ctrlcpl = (uint16)int_val;
		break;

	case IOV_GVAL(IOV_D2H_CTRLCPL_MAX_ITEM):
		int_val = d2h_max_ctrlcpl;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_RX_BUF_BURST):
		if (bus->dhd->busstate != DHD_BUS_DOWN) {
			DHD_ERROR(("%s: Can change only when bus down (before FW download)\n",
				__FUNCTION__));
			bcmerror = BCME_NOTDOWN;
			break;
		}
		rx_buf_burst = (uint16)int_val;
		break;

	case IOV_GVAL(IOV_RX_BUF_BURST):
		int_val = rx_buf_burst;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_RX_BUFPOST_THRESHOLD):
		if (bus->dhd->busstate != DHD_BUS_DOWN) {
			DHD_ERROR(("%s: Can change only when bus down (before FW download)\n",
				__FUNCTION__));
			bcmerror = BCME_NOTDOWN;
			break;
		}
		rx_bufpost_threshold = (uint16)int_val;
		break;

	case IOV_GVAL(IOV_RX_BUFPOST_THRESHOLD):
		int_val = rx_bufpost_threshold;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_GVAL(IOV_TX_CPL_BOUND):
		int_val = (int32)dhd_prot_get_tx_cpl_bound(bus->dhd);
		bcopy(&int_val, arg, val_size);
		break;
	case IOV_SVAL(IOV_TX_CPL_BOUND):
		if (int_val <= 0) {
			bcmerror = BCME_BADARG;
			DHD_ERROR(("%s: invalid tx_cpl_bound value %d !\n",
				__FUNCTION__, int_val));
		} else {
			dhd_prot_set_tx_cpl_bound(bus->dhd, (uint)int_val);
		}
		break;
	case IOV_GVAL(IOV_RX_CPL_POST_BOUND):
		int_val = (int32)dhd_prot_get_rx_cpl_post_bound(bus->dhd);
		bcopy(&int_val, arg, val_size);
		break;
	case IOV_SVAL(IOV_RX_CPL_POST_BOUND):
		if (int_val <= 0) {
			bcmerror = BCME_BADARG;
			DHD_ERROR(("%s: invalid rx_cpl_post_bound value %d !\n",
				__FUNCTION__, int_val));
		} else {
			dhd_prot_set_rx_cpl_post_bound(bus->dhd, (uint)int_val);
		}
		break;
	case IOV_GVAL(IOV_CTRL_CPL_POST_BOUND):
		int_val = (int32)dhd_prot_get_ctrl_cpl_post_bound(bus->dhd);
		bcopy(&int_val, arg, val_size);
		break;
	case IOV_SVAL(IOV_CTRL_CPL_POST_BOUND):
		if (int_val <= 0) {
			bcmerror = BCME_BADARG;
			DHD_ERROR(("%s: invalid ctrl_cpl_post_bound value %d !\n",
				__FUNCTION__, int_val));
		} else {
			dhd_prot_set_ctrl_cpl_post_bound(bus->dhd, (uint)int_val);
		}
		break;
	case IOV_GVAL(IOV_TX_POST_BOUND):
		int_val = (int32)dhd_prot_get_tx_post_bound(bus->dhd);
		bcopy(&int_val, arg, val_size);
		break;
	case IOV_SVAL(IOV_TX_POST_BOUND):
		if (int_val <= 0) {
			bcmerror = BCME_BADARG;
			DHD_ERROR(("%s: invalid tx_post_bound value %d !\n",
				__FUNCTION__, int_val));
		} else {
			dhd_prot_set_tx_post_bound(bus->dhd, (uint)int_val);
		}
		break;

	case IOV_GVAL(IOV_TRAPDATA):
	{
		struct bcmstrbuf dump_b;
		bcm_binit(&dump_b, arg, len);
		bcmerror = dhd_prot_dump_extended_trap(bus->dhd, &dump_b, FALSE);
		break;
	}

	case IOV_GVAL(IOV_TRAPDATA_RAW):
	{
		struct bcmstrbuf dump_b;
		bcm_binit(&dump_b, arg, len);
		bcmerror = dhd_prot_dump_extended_trap(bus->dhd, &dump_b, TRUE);
		break;
	}

#ifdef DHD_PCIE_REG_ACCESS
	case IOV_GVAL(IOV_PCIEASPM): {
		uint8 clkreq = 0;
		uint32 aspm = 0;

		/* this command is to hide the details, but match the lcreg
		#define PCIE_CLKREQ_ENAB		0x100
		#define PCIE_ASPM_L1_ENAB        	2
		#define PCIE_ASPM_L0s_ENAB       	1
		*/

		clkreq = dhdpcie_clkreq(bus->dhd->osh, 0, 0);
		aspm = dhdpcie_lcreg(bus->dhd->osh, 0, 0);

		int_val = ((clkreq & 0x1) << 8) | (aspm & PCIE_ASPM_ENAB);
		bcopy(&int_val, arg, val_size);
		break;
	}

	case IOV_SVAL(IOV_PCIEASPM): {
		uint32 tmp;

		tmp = dhdpcie_lcreg(bus->dhd->osh, 0, 0);
		dhdpcie_lcreg(bus->dhd->osh, PCIE_ASPM_ENAB,
			(tmp & ~PCIE_ASPM_ENAB) | (int_val & PCIE_ASPM_ENAB));

		dhdpcie_clkreq(bus->dhd->osh, 1, ((int_val & 0x100) >> 8));
		break;
	}
#endif /* DHD_PCIE_REG_ACCESS */
	case IOV_SVAL(IOV_HANGREPORT):
		bus->dhd->hang_report = bool_val;
		DHD_ERROR(("%s: Set hang_report as %d\n",
			__FUNCTION__, bus->dhd->hang_report));
		break;

	case IOV_GVAL(IOV_HANGREPORT):
		int_val = (int32)bus->dhd->hang_report;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_CTO_PREVENTION):
		bcmerror = dhdpcie_cto_init(bus, bool_val);
		break;

	case IOV_GVAL(IOV_CTO_PREVENTION):
		if (bus->sih->buscorerev < 19) {
			bcmerror = BCME_UNSUPPORTED;
			break;
		}
		int_val = (int32)bus->cto_enable;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_CTO_THRESHOLD):
		{
			if (bus->sih->buscorerev < 19) {
				bcmerror = BCME_UNSUPPORTED;
				break;
			}
			bus->cto_threshold = (uint32)int_val;
		}
		break;

	case IOV_GVAL(IOV_CTO_THRESHOLD):
		if (bus->sih->buscorerev < 19) {
			bcmerror = BCME_UNSUPPORTED;
			break;
		}
		if (bus->cto_threshold) {
			int_val = (int32)bus->cto_threshold;
		} else {
			int_val = pcie_cto_to_thresh_default(bus->sih->buscorerev);
		}

		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_PCIE_WD_RESET):
		if (bool_val) {
			/* Legacy chipcommon watchdog reset */
			dhdpcie_cc_watchdog_reset(bus);
		}
		break;

	case IOV_GVAL(IOV_IDMA_ENABLE):
		int_val = bus->idma_enabled;
		bcopy(&int_val, arg, val_size);
		break;
	case IOV_SVAL(IOV_IDMA_ENABLE):
		bus->idma_enabled = (bool)int_val;
		break;
	case IOV_GVAL(IOV_IFRM_ENABLE):
		int_val = bus->ifrm_enabled;
		bcopy(&int_val, arg, val_size);
		break;
	case IOV_SVAL(IOV_IFRM_ENABLE):
		bus->ifrm_enabled = (bool)int_val;
		break;
	case IOV_GVAL(IOV_CLEAR_RING):
		bcopy(&int_val, arg, val_size);
		dhd_flow_rings_flush(bus->dhd, 0);
		break;
	case IOV_GVAL(IOV_DAR_ENABLE):
		int_val = bus->dar_enabled;
		bcopy(&int_val, arg, val_size);
		break;
	case IOV_SVAL(IOV_DAR_ENABLE):
		bus->dar_enabled = (bool)int_val;
		break;
	case IOV_GVAL(IOV_HSCBSIZE):
		bcmerror = dhd_get_hscb_info(bus->dhd, NULL, (uint32 *)arg);
		break;
#ifdef DHD_BUS_MEM_ACCESS
	case IOV_GVAL(IOV_HSCBBYTES):
		bcmerror = dhd_get_hscb_buff(bus->dhd, int_val, int_val2, (void*)arg);
		break;
#endif
#ifdef D2H_MINIDUMP
	case IOV_GVAL(IOV_MINIDUMP_OVERRIDE):
		int_val = bus->d2h_minidump_override;
		bcopy(&int_val, arg, val_size);
		break;
	case IOV_SVAL(IOV_MINIDUMP_OVERRIDE):
		/* Can change it only before FW download */
		if (bus->dhd->busstate != DHD_BUS_DOWN) {
			DHD_ERROR(("%s: Can change only when bus down (before FW download)\n",
				__FUNCTION__));
			bcmerror = BCME_NOTDOWN;
			break;
		}
		bus->d2h_minidump_override = (bool)int_val;
		break;
#endif /* D2H_MINIDUMP */

	case IOV_SVAL(IOV_EXTDTXS_IN_TXCPL):
		if (bus->dhd->busstate != DHD_BUS_DOWN) {
			return BCME_NOTDOWN;
		}
		if (int_val)
			bus->dhd->extdtxs_in_txcpl = TRUE;
		else
			bus->dhd->extdtxs_in_txcpl = FALSE;
		break;

	case IOV_GVAL(IOV_EXTDTXS_IN_TXCPL):
		int_val = bus->dhd->extdtxs_in_txcpl;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_HOSTRDY_AFTER_INIT):
		if (bus->dhd->busstate != DHD_BUS_DOWN) {
			return BCME_NOTDOWN;
		}
		if (int_val)
			bus->dhd->hostrdy_after_init = TRUE;
		else
			bus->dhd->hostrdy_after_init = FALSE;
		break;

	case IOV_GVAL(IOV_HOSTRDY_AFTER_INIT):
		int_val = bus->dhd->hostrdy_after_init;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_GVAL(IOV_DONGLE_RXLAT_INFO):
	{
		unsigned long flags = 0;
		struct bcmstrbuf strbuf;

		bcm_binit(&strbuf, arg, len);
		DHD_GENERAL_LOCK(bus->dhd, flags);
		dhd_bus_dump_rxlat_info(bus->dhd, &strbuf);
		DHD_GENERAL_UNLOCK(bus->dhd, flags);
		break;
	}

	case IOV_GVAL(IOV_DONGLE_RXLAT_HISTO):
	{
		unsigned long flags = 0;
		struct bcmstrbuf strbuf;

		bcm_binit(&strbuf, arg, len);
		DHD_GENERAL_LOCK(bus->dhd, flags);
		dhd_bus_dump_rxlat_histo(bus->dhd, &strbuf);
		DHD_GENERAL_UNLOCK(bus->dhd, flags);
		break;
	}

	case IOV_GVAL(IOV_DONGLE_TXCPL_INFO):
	{
		unsigned long flags = 0;
		struct bcmstrbuf strbuf;

		bcm_binit(&strbuf, arg, len);
		DHD_GENERAL_LOCK(bus->dhd, flags);
		dhd_bus_dump_txcpl_info(bus->dhd, &strbuf);
		DHD_GENERAL_UNLOCK(bus->dhd, flags);
		break;
	}

	case IOV_GVAL(IOV_DONGLE_SEC_INFO):
	{
		unsigned long flags = 0;
		struct bcmstrbuf strbuf;

		bcm_binit(&strbuf, arg, len);
		DHD_GENERAL_LOCK(bus->dhd, flags);
		bcmerror = dhd_bus_security_info(bus->dhd, &strbuf);
		DHD_GENERAL_UNLOCK(bus->dhd, flags);
		break;
	}

	case IOV_GVAL(IOV_DONGLE_SBOOT_DIS):
	{
		unsigned long flags = 0;
		struct bcmstrbuf strbuf;

		bcm_binit(&strbuf, arg, len);
		DHD_GENERAL_LOCK(bus->dhd, flags);
		bcmerror = dhd_bus_sboot_disable(bus->dhd, &strbuf);
		DHD_GENERAL_UNLOCK(bus->dhd, flags);
		break;
	}
	case IOV_GVAL(IOV_LPM_MODE):
		int_val = bus->lpm_mode;
		if (bus->lpm_mode) {
			if (!bus->lpm_mem_kill) {
				int_val |= LPM_MODE_NO_MEMKILL;
			}
			if (!bus->lpm_force_flr) {
				int_val |= LPM_MODE_NO_FLR;
			}
		}
		(void)memcpy_s(arg, val_size, &int_val, sizeof(int_val));
		break;
	case IOV_SVAL(IOV_LPM_MODE):
		if (int_val) {
			bus->lpm_mode = TRUE;
			if (int_val & LPM_MODE_NO_MEMKILL) {
				bus->lpm_mem_kill = FALSE;
			}
			else {
				bus->lpm_mem_kill = TRUE;
			}
			if (int_val & LPM_MODE_NO_FLR) {
				bus->lpm_force_flr = FALSE;
			}
			else {
				bus->lpm_force_flr = TRUE;
			}
		}
		else {
			bus->lpm_mode = FALSE;
			bus->lpm_mem_kill = FALSE;
			bus->lpm_force_flr = FALSE;
		}
		break;
	case IOV_GVAL(IOV_MDRING_DUMP):
	{
		struct bcmstrbuf strbuf;

		if (bus->dhd->mdring_capable) {
			bcm_binit(&strbuf, arg, len);
			dhd_bus_dump_mdring_info(bus->dhd, &strbuf);
		}
		break;
	}
	case IOV_SVAL(IOV_MDRING_LINK):
		if (bus->dhd->mdring_capable && int_val) {
			dhd_prot_mdring_link_unlink(bus->dhd, int_val, TRUE);
		}
		break;
	case IOV_GVAL(IOV_MDRING_LINK):
		if (bus->dhd->mdring_capable) {
			int_val = dhd_prot_mdring_linked_ring(bus->dhd);
			bcopy(&int_val, arg, val_size);
		}
		break;
	case IOV_SVAL(IOV_MDRING_UNLINK):
		if (bus->dhd->mdring_capable) {
			dhd_prot_mdring_link_unlink(bus->dhd, int_val, FALSE);
		}
		break;
	case IOV_GVAL(IOV_MDRING_UNLINK):
		if (bus->dhd->mdring_capable) {
			int_val = dhd_prot_mdring_linked_ring(bus->dhd);
			bcopy(&int_val, arg, val_size);
		}
		break;
	case IOV_SVAL(IOV_PTM_ENABLE):
		if (int_val) {
			dhdpcie_send_mb_data(bus, H2D_HOST_PTM_ENABLE);
		} else {
			dhdpcie_send_mb_data(bus, H2D_HOST_PTM_DISABLE);
		}
		break;
	default:
		bcmerror = BCME_UNSUPPORTED;
		break;
	}

exit:
	return bcmerror;
} /* dhdpcie_bus_doiovar */

/** Transfers bytes from host to dongle using pio mode */
static int
dhdpcie_bus_lpback_req(struct  dhd_bus *bus, uint32 len)
{
	if (bus->dhd == NULL) {
		DHD_ERROR(("%s: bus not inited\n", __FUNCTION__));
		return 0;
	}
	if (bus->dhd->prot == NULL) {
		DHD_ERROR(("%s: prot is not inited\n", __FUNCTION__));
		return 0;
	}
	if (bus->dhd->busstate != DHD_BUS_DATA) {
		DHD_ERROR(("%s: not in a readystate to LPBK  is not inited\n", __FUNCTION__));
		return 0;
	}
	dhdmsgbuf_lpbk_req(bus->dhd, len);
	return 0;
}

void
dhd_bus_dump_dar_registers(struct dhd_bus *bus)
{
	uint32 dar_clk_ctrl_val, dar_pwr_ctrl_val, dar_intstat_val,
		dar_errlog_val, dar_erraddr_val, dar_pcie_mbint_val;
	uint32 dar_clk_ctrl_reg, dar_pwr_ctrl_reg, dar_intstat_reg,
		dar_errlog_reg, dar_erraddr_reg, dar_pcie_mbint_reg;

	if (bus->is_linkdown) {
		DHD_ERROR(("%s: link is down\n", __FUNCTION__));
		return;
	}

	if (bus->sih == NULL) {
		DHD_ERROR(("%s: si_attach has not happened, cannot dump DAR registers\n",
			__FUNCTION__));
		return;
	}

	bus->dar_err_set = FALSE;

	if (DAR_PWRREQ(bus)) {
		dhd_bus_pcie_pwr_req(bus);
	}

	dar_clk_ctrl_reg = (uint32)DAR_CLK_CTRL(bus->sih->buscorerev);
	dar_pwr_ctrl_reg = (uint32)DAR_PCIE_PWR_CTRL(bus->sih->buscorerev);
	dar_intstat_reg = (uint32)DAR_INTSTAT(bus->sih->buscorerev);
	dar_errlog_reg = (uint32)DAR_ERRLOG(bus->sih->buscorerev);
	dar_erraddr_reg = (uint32)DAR_ERRADDR(bus->sih->buscorerev);
	dar_pcie_mbint_reg = (uint32)DAR_PCIMailBoxInt(bus->sih->buscorerev);

	if (bus->sih->buscorerev < 24) {
		DHD_ERROR(("%s: DAR not supported for corerev(%d) < 24\n",
			__FUNCTION__, bus->sih->buscorerev));
		return;
	}

	dar_clk_ctrl_val = si_corereg(bus->sih, bus->sih->buscoreidx, dar_clk_ctrl_reg, 0, 0);
	dar_pwr_ctrl_val = si_corereg(bus->sih, bus->sih->buscoreidx, dar_pwr_ctrl_reg, 0, 0);
	dar_intstat_val = si_corereg(bus->sih, bus->sih->buscoreidx, dar_intstat_reg, 0, 0);
	dar_errlog_val = si_corereg(bus->sih, bus->sih->buscoreidx, dar_errlog_reg, 0, 0);
	dar_erraddr_val = si_corereg(bus->sih, bus->sih->buscoreidx, dar_erraddr_reg, 0, 0);
	dar_pcie_mbint_val = si_corereg(bus->sih, bus->sih->buscoreidx, dar_pcie_mbint_reg, 0, 0);

	DHD_RPM(("%s: dar_clk_ctrl(0x%x:0x%x) dar_pwr_ctrl(0x%x:0x%x) "
		"dar_intstat(0x%x:0x%x)\n",
		__FUNCTION__, dar_clk_ctrl_reg, dar_clk_ctrl_val,
		dar_pwr_ctrl_reg, dar_pwr_ctrl_val, dar_intstat_reg, dar_intstat_val));

	DHD_RPM(("%s: dar_errlog(0x%x:0x%x) dar_erraddr(0x%x:0x%x) "
		"dar_pcie_mbint(0x%x:0x%x)\n",
		__FUNCTION__, dar_errlog_reg, dar_errlog_val,
		dar_erraddr_reg, dar_erraddr_val, dar_pcie_mbint_reg, dar_pcie_mbint_val));

	if (dar_errlog_val || dar_erraddr_val) {
		bus->dar_err_set = TRUE;
	}

}

/* Ring DoorBell1 to indicate Hostready i.e. D3 Exit */
void
dhd_bus_hostready(struct  dhd_bus *bus)
{
	if (!bus->dhd->d2h_hostrdy_supported) {
		return;
	}

	if (bus->is_linkdown) {
		DHD_ERROR(("%s: PCIe link was down\n", __FUNCTION__));
		return;
	}

	DHD_ERROR(("%s : Read PCICMD Reg: 0x%08X\n", __FUNCTION__,
		dhd_pcie_config_read(bus, PCI_CFG_CMD, sizeof(uint32))));

	dhd_bus_dump_dar_registers(bus);

	if (bus->is_linkdown) {
		DHD_ERROR(("%s: PCIe link was down before DB1\n", __FUNCTION__));
		return;
	}

#ifdef DHD_MMIO_TRACE
	dhd_bus_mmio_trace(bus, dhd_bus_db1_addr_get(bus), 0x1, TRUE);
#endif /* defined(DHD_MMIO_TRACE) */
	si_corereg(bus->sih, bus->sih->buscoreidx, dhd_bus_db1_addr_get(bus), ~0, 0x12345678);
	bus->hostready_count ++;
	DHD_ERROR(("%s: Ring Hostready:%d\n", __FUNCTION__, bus->hostready_count));
}

/* Clear INTSTATUS */
void
dhdpcie_bus_clear_intstatus(struct dhd_bus *bus)
{
	uint32 intstatus = 0;
	/* Skip after receiving D3 ACK */
	if (DHD_CHK_BUS_LPS_D3_ACKED(bus)) {
		return;
	}
	/* XXX: check for PCIE Gen2 also */
	if ((bus->sih->buscorerev == 6) || (bus->sih->buscorerev == 4) ||
		(bus->sih->buscorerev == 2)) {
		intstatus = dhdpcie_bus_cfg_read_dword(bus, PCIIntstatus, 4);
		dhdpcie_bus_cfg_write_dword(bus, PCIIntstatus, 4, intstatus);
	} else {
		/* this is a PCIE core register..not a config register... */
		intstatus = si_corereg(bus->sih, bus->sih->buscoreidx, bus->pcie_mailbox_int, 0, 0);
		si_corereg(bus->sih, bus->sih->buscoreidx, bus->pcie_mailbox_int, bus->def_intmask,
			intstatus);
	}
}

#ifdef PWRSTATS_SYSFS
uint64
dhdpcie_get_last_suspend_time(dhd_pub_t *dhdp)
{
	return dhdp->bus->last_suspend_end_time;
}
#endif /* PWRSTATS_SYSFS */

#define BCM4387_WLAN_SYS_MEMDOWN_FORCE_PMU      0x444
#define BCM4378_WLAN_SYS_MEMDOWN_FORCE_PMU      0x133

static void
dhd_pcie_handle_lpm_memkill(struct dhd_bus *bus)
{
	uint16 chipid = si_chipid(bus->sih);

	if (bus->lpm_mem_kill == FALSE) {
		DHD_ERROR(("LPM MODE: MEM Kill not enabled\n"));
		return;
	}
	if (chipid == BCM4378_CHIP_ID) {
		pmu_corereg(bus->sih, SI_CC_IDX, chipcontrol_addr, ~0, PMU_CHIPCTL13);
		pmu_corereg(bus->sih, SI_CC_IDX, chipcontrol_data,
			BCM4378_WLAN_SYS_MEMDOWN_FORCE_PMU, BCM4378_WLAN_SYS_MEMDOWN_FORCE_PMU);
	}
	else if (chipid == BCM4387_CHIP_ID) {
		pmu_corereg(bus->sih, SI_CC_IDX, chipcontrol_addr, ~0, PMU_CHIPCTL13);
		pmu_corereg(bus->sih, SI_CC_IDX, chipcontrol_data,
			BCM4387_WLAN_SYS_MEMDOWN_FORCE_PMU, BCM4387_WLAN_SYS_MEMDOWN_FORCE_PMU);
	}
	else {
		DHD_ERROR(("LPM MODE: Mem kill support not added for chip %d\n", chipid));
		return;
	}
	DHD_ERROR(("PMU chipcontrol addr %d value is 0x%04x\n",
		pmu_corereg(bus->sih, SI_CC_IDX, chipcontrol_addr, 0, 0),
		pmu_corereg(bus->sih, SI_CC_IDX, chipcontrol_data, 0, 0)));
}

int
#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
dhdpcie_bus_suspend(struct dhd_bus *bus, bool state, bool byint)
#else
dhdpcie_bus_suspend(struct dhd_bus *bus, bool state)
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */
{
	int timeleft;
	int rc = 0;
	unsigned long flags;
#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
	int d3_read_retry = 0;
	uint32 d2h_mb_data = 0;
	uint32 zero = 0;
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

	printf("%s: state=%d\n", __FUNCTION__, state);
	if (bus->dhd == NULL) {
		DHD_ERROR(("%s: bus not inited\n", __FUNCTION__));
		return BCME_ERROR;
	}
	if (bus->dhd->prot == NULL) {
		DHD_ERROR(("%s: prot is not inited\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (dhd_query_bus_erros(bus->dhd)) {
		return BCME_ERROR;
	}

	DHD_GENERAL_LOCK(bus->dhd, flags);
	if (!(bus->dhd->busstate == DHD_BUS_DATA || bus->dhd->busstate == DHD_BUS_SUSPEND)) {
		DHD_ERROR(("%s: not in a readystate\n", __FUNCTION__));
		DHD_GENERAL_UNLOCK(bus->dhd, flags);
		return BCME_ERROR;
	}
	DHD_GENERAL_UNLOCK(bus->dhd, flags);
	if (bus->dhd->dongle_reset) {
		DHD_ERROR(("Dongle is in reset state.\n"));
		return -EIO;
	}

	/* Check whether we are already in the requested state.
	 * state=TRUE means Suspend
	 * state=FALSE meanse Resume
	 */
	if (state == TRUE && bus->dhd->busstate == DHD_BUS_SUSPEND) {
		DHD_ERROR(("Bus is already in SUSPEND state.\n"));
		return BCME_OK;
	} else if (state == FALSE && bus->dhd->busstate == DHD_BUS_DATA) {
		DHD_ERROR(("Bus is already in RESUME state.\n"));
		return BCME_OK;
	}

	if (state) {
#ifdef OEM_ANDROID
		int idle_retry = 0;
		int active;
#endif /* OEM_ANDROID */

		if (bus->is_linkdown) {
			DHD_ERROR(("%s: PCIe link was down, state=%d\n",
				__FUNCTION__, state));
			return BCME_ERROR;
		}

		/* Suspend */
		DHD_RPM(("%s: Entering suspend state\n", __FUNCTION__));

		bus->dhd->dhd_watchdog_ms_backup = dhd_watchdog_ms;
		if (bus->dhd->dhd_watchdog_ms_backup) {
			DHD_ERROR(("%s: Disabling wdtick before going to suspend\n",
				__FUNCTION__));
			dhd_os_wd_timer(bus->dhd, 0);
		}

		DHD_GENERAL_LOCK(bus->dhd, flags);
#if defined(LINUX) || defined(linux)
		if (DHD_BUS_BUSY_CHECK_IN_TX(bus->dhd)) {
			DHD_ERROR(("Tx Request is not ended\n"));
			bus->dhd->busstate = DHD_BUS_DATA;
			DHD_GENERAL_UNLOCK(bus->dhd, flags);
			return -EBUSY;
		}
#endif /* LINUX || linux */

		bus->last_suspend_start_time = OSL_LOCALTIME_NS();

		/* stop all interface network queue. */
		dhd_bus_stop_queue(bus);
		DHD_GENERAL_UNLOCK(bus->dhd, flags);
#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
		if (byint) {
			DHD_OS_WAKE_LOCK_WAIVE(bus->dhd);
			/* Clear wait_for_d3_ack before sending D3_INFORM */
			bus->wait_for_d3_ack = 0;
			dhdpcie_send_mb_data(bus, H2D_HOST_D3_INFORM);

			timeleft = dhd_os_d3ack_wait(bus->dhd, &bus->wait_for_d3_ack);
			DHD_OS_WAKE_LOCK_RESTORE(bus->dhd);
		} else {
			/* Clear wait_for_d3_ack before sending D3_INFORM */
			bus->wait_for_d3_ack = 0;
			dhdpcie_send_mb_data(bus, H2D_HOST_D3_INFORM | H2D_HOST_ACK_NOINT);
			while (!bus->wait_for_d3_ack && d3_read_retry < MAX_D3_ACK_TIMEOUT) {
				dhdpcie_handle_mb_data(bus);
				usleep_range(1000, 1500);
				d3_read_retry++;
			}
		}
#else
		DHD_OS_WAKE_LOCK_WAIVE(bus->dhd);

#ifdef PCIE_INB_DW
		/* As D3_INFORM will be sent after De-assert,
		 * skip sending DS-ACK for DS-REQ.
		 */
	DHD_BUS_INB_DW_LOCK(bus->inb_lock, flags);
	bus->skip_ds_ack = TRUE;
	DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
#endif /* PCIE_INB_DW */

#if defined(PCIE_OOB) || defined(PCIE_INB_DW)
		dhd_bus_set_device_wake(bus, TRUE);
#endif /* defined(PCIE_OOB) || defined(PCIE_INB_DW) */
#ifdef PCIE_OOB
		bus->oob_presuspend = TRUE;
#endif
#ifdef PCIE_INB_DW
		/* De-assert at this point for In-band device_wake */
		if (INBAND_DW_ENAB(bus)) {
			dhd_bus_set_device_wake(bus, FALSE);
			DHD_BUS_INB_DW_LOCK(bus->inb_lock, flags);
			dhdpcie_bus_set_pcie_inband_dw_state(bus, DW_DEVICE_HOST_SLEEP_WAIT);
			DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
		}
#endif /* PCIE_INB_DW */
		/* Clear wait_for_d3_ack before sending D3_INFORM */
		bus->wait_for_d3_ack = 0;
		/*
		 * Send H2D_HOST_D3_INFORM to dongle and mark bus->bus_low_power_state
		 * to DHD_BUS_D3_INFORM_SENT in dhd_prot_ring_write_complete_mbdata
		 * inside atomic context, so that no more DBs will be
		 * rung after sending D3_INFORM
		 */
#ifdef PCIE_INB_DW
		if (INBAND_DW_ENAB(bus)) {
			DHD_BUS_INB_DW_LOCK(bus->inb_lock, flags);
			dhdpcie_send_mb_data(bus, H2D_HOST_D3_INFORM);
			DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
		} else
#endif /* PCIE_INB_DW */
		{
			dhdpcie_send_mb_data(bus, H2D_HOST_D3_INFORM);
		}

		/* Wait for D3 ACK for D3_ACK_RESP_TIMEOUT seconds */

		timeleft = dhd_os_d3ack_wait(bus->dhd, &bus->wait_for_d3_ack);
#ifdef DHD_RECOVER_TIMEOUT
		/* XXX: WAR for missing D3 ACK MB interrupt */
		if (bus->wait_for_d3_ack == 0) {
			/* If wait_for_d3_ack was not updated because D2H MB was not received */
			uint32 intstatus = si_corereg(bus->sih, bus->sih->buscoreidx,
				bus->pcie_mailbox_int, 0, 0);
			int host_irq_disabled = dhdpcie_irq_disabled(bus);
			if ((intstatus) && (intstatus != (uint32)-1) &&
				(timeleft == 0) && (!dhd_query_bus_erros(bus->dhd))) {
				DHD_ERROR(("%s: resumed on timeout for D3 ack. intstatus=%x"
					" host_irq_disabled=%d\n",
					__FUNCTION__, intstatus, host_irq_disabled));
				dhd_pcie_intr_count_dump(bus->dhd);
				dhd_print_tasklet_status(bus->dhd);
				dhd_prot_ctrl_info_print(bus->dhd);
				/* Clear Interrupts */
				dhdpcie_bus_clear_intstatus(bus);
				if (dhd_prot_check_pending_ctrl_cmpls(bus->dhd)) {
					DHD_ERROR(("##### %s: D3 ack timeout trying again #####\n",
						__FUNCTION__));
					dhd_schedule_delayed_dpc_on_dpc_cpu(bus->dhd, 0);
					timeleft = dhd_os_d3ack_wait(bus->dhd,
						&bus->wait_for_d3_ack);
				}
			}
		} /* bus->wait_for_d3_ack was 0 */
#endif /* DHD_RECOVER_TIMEOUT */

		DHD_OS_WAKE_LOCK_RESTORE(bus->dhd);
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

#ifdef OEM_ANDROID
		/* To allow threads that got pre-empted to complete.
		 */
		while ((active = dhd_os_check_wakelock_all(bus->dhd)) &&
			(idle_retry < MAX_WKLK_IDLE_CHECK)) {
			OSL_SLEEP(1);
			idle_retry++;
		}
#endif /* OEM_ANDROID */

		if (bus->wait_for_d3_ack) {
			DHD_RPM(("%s: Got D3 Ack \n", __FUNCTION__));
			/* Got D3 Ack. Suspend the bus */
#ifdef OEM_ANDROID
			if (active) {
				DHD_ERROR(("%s():Suspend failed because of wakelock "
					"restoring Dongle to D0\n", __FUNCTION__));

				if (bus->dhd->dhd_watchdog_ms_backup) {
					DHD_ERROR(("%s: Enabling wdtick due to wakelock active\n",
						__FUNCTION__));
					dhd_os_wd_timer(bus->dhd,
						bus->dhd->dhd_watchdog_ms_backup);
				}

				/*
				 * Dongle still thinks that it has to be in D3 state until
				 * it gets a D0 Inform, but we are backing off from suspend.
				 * Ensure that Dongle is brought back to D0.
				 *
				 * Bringing back Dongle from D3 Ack state to D0 state is a
				 * 2 step process. Dongle would want to know that D0 Inform
				 * would be sent as a MB interrupt to bring it out of D3 Ack
				 * state to D0 state. So we have to send both this message.
				 */

				/* Clear wait_for_d3_ack to send D0_INFORM or host_ready */
				bus->wait_for_d3_ack = 0;

				DHD_SET_BUS_NOT_IN_LPS(bus);
#ifdef PCIE_INB_DW
				if (INBAND_DW_ENAB(bus)) {
					DHD_BUS_INB_DW_LOCK(bus->inb_lock, flags);
					/* Since suspend has failed due to wakelock is held,
					 * update the DS state to DW_DEVICE_HOST_WAKE_WAIT.
					 * So that host sends the DS-ACK for DS-REQ.
					 */
					DHD_ERROR(("Suspend failed due to wakelock is held, "
					 "set inband dw state to DW_DEVICE_HOST_WAKE_WAIT\n"));
					dhdpcie_bus_set_pcie_inband_dw_state(bus,
						DW_DEVICE_HOST_WAKE_WAIT);
					dhd_bus_ds_trace(bus, 0, TRUE,
						dhdpcie_bus_get_pcie_inband_dw_state(bus));
					bus->skip_ds_ack = FALSE;
					DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
				}
#endif /* PCIE_INB_DW */
				/* Enable back the intmask which was cleared in DPC
				 * after getting D3_ACK.
				 */
				dhdpcie_bus_intr_enable(bus);
				bus->dngl_intmask_enable_count++;

				if (bus->use_d0_inform) {
					DHD_OS_WAKE_LOCK_WAIVE(bus->dhd);
					dhdpcie_send_mb_data(bus,
						(H2D_HOST_D0_INFORM_IN_USE | H2D_HOST_D0_INFORM));
					DHD_OS_WAKE_LOCK_RESTORE(bus->dhd);
				}
				/* ring doorbell 1 (hostready) */
				dhd_bus_hostready(bus);

				DHD_GENERAL_LOCK(bus->dhd, flags);
				bus->dhd->busstate = DHD_BUS_DATA;
				/* resume all interface network queue. */
				dhd_bus_start_queue(bus);
				DHD_GENERAL_UNLOCK(bus->dhd, flags);
				rc = BCME_ERROR;
			} else {
				/* Actual Suspend after no wakelock */
#endif /* OEM_ANDROID */
				/* At this time bus->bus_low_power_state will be
				 * made to DHD_BUS_D3_ACK_RECIEVED after receiving D3_ACK
				 * in dhd_bus_handle_d3_ack()
				 */
#ifdef PCIE_OOB
				bus->oob_presuspend = FALSE;
				if (OOB_DW_ENAB(bus)) {
					dhd_bus_set_device_wake(bus, FALSE);
				}
#endif
#ifdef PCIE_OOB
				bus->oob_presuspend = TRUE;
#endif
#ifdef PCIE_INB_DW
				if (INBAND_DW_ENAB(bus)) {
					DHD_BUS_INB_DW_LOCK(bus->inb_lock, flags);
					if (dhdpcie_bus_get_pcie_inband_dw_state(bus) ==
						DW_DEVICE_HOST_SLEEP_WAIT) {
						dhdpcie_bus_set_pcie_inband_dw_state(bus,
							DW_DEVICE_HOST_SLEEP);
#ifdef PCIE_INB_DW
						dhd_bus_ds_trace(bus, 0, TRUE,
							dhdpcie_bus_get_pcie_inband_dw_state(bus));
#else
						dhd_bus_ds_trace(bus, 0, TRUE);
#endif /* PCIE_INB_DW */
					}
					DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
				}
#endif /* PCIE_INB_DW */
				if (bus->use_d0_inform &&
					(bus->api.fw_rev < PCIE_SHARED_VERSION_6)) {
					DHD_OS_WAKE_LOCK_WAIVE(bus->dhd);
					dhdpcie_send_mb_data(bus, (H2D_HOST_D0_INFORM_IN_USE));
					DHD_OS_WAKE_LOCK_RESTORE(bus->dhd);
				}
#ifdef PCIE_OOB
				dhd_bus_set_device_wake(bus, FALSE);
#endif /* PCIE_OOB */

#if defined(BCMPCIE_OOB_HOST_WAKE)
				if (bus->dhd->dhd_induce_error == DHD_INDUCE_DROP_OOB_IRQ) {
					DHD_ERROR(("%s: Inducing DROP OOB IRQ\n", __FUNCTION__));
				} else {
					dhdpcie_oob_intr_set(bus, TRUE);
				}
#endif /* BCMPCIE_OOB_HOST_WAKE */

				DHD_GENERAL_LOCK(bus->dhd, flags);
				/* The Host cannot process interrupts now so disable the same.
				 * No need to disable the dongle INTR using intmask, as we are
				 * already calling disabling INTRs from DPC context after
				 * getting D3_ACK in dhd_bus_handle_d3_ack.
				 * Code may not look symmetric between Suspend and
				 * Resume paths but this is done to close down the timing window
				 * between DPC and suspend context and bus->bus_low_power_state
				 * will be set to DHD_BUS_D3_ACK_RECIEVED in DPC.
				 */
				bus->dhd->d3ackcnt_timeout = 0;
				bus->dhd->busstate = DHD_BUS_SUSPEND;
				DHD_GENERAL_UNLOCK(bus->dhd, flags);
#if defined(LINUX) || defined(linux)
				dhdpcie_dump_resource(bus);
#endif /* LINUX || linux */
				DHD_INFO(("lpm_mode %d, fw_lpm_support %d\n", bus->lpm_mode,
					bus->dhd->fw_lpm_support));
				/* Once LPM mode is entered, recovery is only through WL_REG_ON */
				if (bus->lpm_mode && bus->dhd->fw_lpm_support) {
					/* send DB7 with PCIE_DB7_MAGIC_NUMBER_DPC_LPM */
					int trap_data = 0;
					uint32 addr = dhd_bus_db1_addr_3_get(bus);

					DHD_ERROR(("forcing LPM mode\n"));
					si_corereg(bus->sih, bus->sih->buscoreidx, addr, ~0,
						PCIE_DB7_MAGIC_NUMBER_DPC_LPM);
					/* wait 500ms */
					OSL_DELAY(500000);
					/* read trap data to confirm */
					trap_data = dhd_prot_process_trapbuf(bus->dhd);
					DHD_ERROR(("trap data in response to LPM mode entry"
						" is 0x%x\n", trap_data));
					/* force memory bits down */
					if (trap_data) {
						dhd_pcie_handle_lpm_memkill(bus);
					}
				}
				DHD_INFO(("Doing the D3\n"));
				rc = dhdpcie_pci_suspend_resume(bus, state);
				if (!rc) {
					bus->last_suspend_end_time = OSL_LOCALTIME_NS();
				}
				DHD_INFO(("Doing the FLR, lpm_mode %d, lpm_force_flr %d,"
					" fwsupport %d\n",
					bus->lpm_mode, bus->lpm_force_flr,
					bus->dhd->fw_lpm_support));
				if (bus->lpm_mode && bus->dhd->fw_lpm_support &&
					bus->lpm_force_flr) {
					bus->lpm_keep_in_reset = TRUE;
					dhd_bus_perform_flr(bus, FALSE);
					bus->lpm_keep_in_reset = FALSE;
					DHD_ERROR(("FLR done\n"));
				}
#ifdef OEM_ANDROID
			}
#endif /* OEM_ANDROID */
		} else if (timeleft == 0) { /* D3 ACK Timeout */
#ifdef DHD_FW_COREDUMP
			uint32 cur_memdump_mode = bus->dhd->memdump_enabled;
#endif /* DHD_FW_COREDUMP */

			/* check if the D3 ACK timeout due to scheduling issue */
			bus->dhd->is_sched_error = !dhd_query_bus_erros(bus->dhd) &&
				dhd_bus_query_dpc_sched_errors(bus->dhd);
			DHD_ERROR(("%s: resumed on timeout for D3 ACK%s d3_inform_cnt %d\n",
				__FUNCTION__, bus->dhd->is_sched_error ?
				" due to scheduling problem" : "", bus->dhd->d3ackcnt_timeout));
#if defined(DHD_KERNEL_SCHED_DEBUG) && defined(DHD_FW_COREDUMP)
			/* XXX DHD triggers Kernel panic if the resumed on timeout occurrs
			 * due to tasklet or workqueue scheduling problems in the Linux Kernel.
			 * Customer informs that it is hard to find any clue from the
			 * host memory dump since the important tasklet or workqueue information
			 * is already disappered due the latency while printing out the timestamp
			 * logs for debugging scan timeout issue.
			 * For this reason, customer requestes us to trigger Kernel Panic rather
			 * than taking a SOCRAM dump.
			 */
			if (bus->dhd->is_sched_error && cur_memdump_mode == DUMP_MEMFILE_BUGON) {
				/* change g_assert_type to trigger Kernel panic */
				g_assert_type = 2;
				/* use ASSERT() to trigger panic */
				ASSERT(0);
			}
#endif /* DHD_KERNEL_SCHED_DEBUG && DHD_FW_COREDUMP */
			DHD_SET_BUS_NOT_IN_LPS(bus);

			DHD_GENERAL_LOCK(bus->dhd, flags);
			bus->dhd->busstate = DHD_BUS_DATA;
			/* resume all interface network queue. */
			dhd_bus_start_queue(bus);
			DHD_GENERAL_UNLOCK(bus->dhd, flags);
			/* Dump important config space registers */
			dhd_bus_dump_imp_cfg_registers(bus);
			/* XXX : avoid multiple socram dump from dongle trap and
			 * invalid PCIe bus assceess due to PCIe link down
			 */
			if (bus->dhd->check_trap_rot) {
				DHD_ERROR(("Check dongle trap in the case of d3 ack timeout\n"));
				dhdpcie_checkdied(bus, NULL, 0);
			}
			/* Set d3_ack_timeout_occurred after checkdied,
			 * as checkdied returns for any bus error
			 */
			bus->dhd->d3ack_timeout_occured = TRUE;
			/* If the D3 Ack has timeout */
			bus->dhd->d3ackcnt_timeout++;

			dhd_validate_pcie_link_cbp_wlbp(bus);
			if (bus->link_state != DHD_PCIE_ALL_GOOD) {
				DHD_ERROR(("%s: bus->link_state:%d\n",
					__FUNCTION__, bus->link_state));
#ifdef OEM_ANDROID
				dhd_os_check_hang(bus->dhd, 0, -ETIMEDOUT);
#endif /* OEM_ANDROID */
			}

			if (bus->dhd->dongle_trap_occured) {
#ifdef OEM_ANDROID
				dhd_os_check_hang(bus->dhd, 0, -EREMOTEIO);
#endif /* OEM_ANDROID */
			} else if (!bus->is_linkdown &&
				!bus->cto_triggered && (bus->link_state == DHD_PCIE_ALL_GOOD)) {
				uint32 intstatus = 0;

				/* Check if PCIe bus status is valid */
				intstatus = si_corereg(bus->sih, bus->sih->buscoreidx,
					bus->pcie_mailbox_int, 0, 0);
				if (intstatus == (uint32)-1) {
					/* Invalidate PCIe bus status */
					bus->is_linkdown = 1;
				}

				dhd_bus_dump_console_buffer(bus);
				dhd_prot_debug_info_print(bus->dhd);
#ifdef DHD_FW_COREDUMP
				if (cur_memdump_mode) {
					/* write core dump to file */
					bus->dhd->memdump_type = DUMP_TYPE_D3_ACK_TIMEOUT;
					dhdpcie_mem_dump(bus);
				}
#endif /* DHD_FW_COREDUMP */

#ifdef NDIS
				/* ASSERT only if hang detection/recovery is disabled.
				 * If enabled then let
				 * windows HDR mechansim trigger FW download via surprise removal
				 */
				dhd_bus_check_died(bus);
#endif
#ifdef OEM_ANDROID
				DHD_ERROR(("%s: Event HANG send up due to D3_ACK timeout\n",
					__FUNCTION__));
				dhd_os_check_hang(bus->dhd, 0, -ETIMEDOUT);
#endif /* OEM_ANDROID */
			}
			rc = -ETIMEDOUT;
		}
#ifdef PCIE_OOB
		bus->oob_presuspend = FALSE;
#endif /* PCIE_OOB */
	} else {
		/* Resume */
		DHD_RPM(("%s: Entering resume state\n", __FUNCTION__));
		bus->last_resume_start_time = OSL_LOCALTIME_NS();

		/**
		 * PCIE2_BAR0_CORE2_WIN gets reset after D3 cold.
		 * si_backplane_access(function to read/write backplane)
		 * updates the window(PCIE2_BAR0_CORE2_WIN) only if
		 * window being accessed is different form the window
		 * being pointed by second_bar0win.
		 * Since PCIE2_BAR0_CORE2_WIN is already reset because of D3 cold,
		 * invalidating second_bar0win after resume updates
		 * PCIE2_BAR0_CORE2_WIN with right window.
		 */
		si_invalidate_second_bar0win(bus->sih);
#if defined(linux) && defined(OEM_ANDROID)
#if defined(BCMPCIE_OOB_HOST_WAKE)
		DHD_OS_OOB_IRQ_WAKE_UNLOCK(bus->dhd);
#endif /* BCMPCIE_OOB_HOST_WAKE */
#endif /* linux && OEM_ANDROID */
#ifdef PCIE_INB_DW
		if (INBAND_DW_ENAB(bus)) {
			DHD_BUS_INB_DW_LOCK(bus->inb_lock, flags);
			if (dhdpcie_bus_get_pcie_inband_dw_state(bus) == DW_DEVICE_HOST_SLEEP) {
				dhdpcie_bus_set_pcie_inband_dw_state(bus, DW_DEVICE_HOST_WAKE_WAIT);
#ifdef PCIE_INB_DW
				dhd_bus_ds_trace(bus, 0, TRUE,
					dhdpcie_bus_get_pcie_inband_dw_state(bus));
#else
				dhd_bus_ds_trace(bus, 0, TRUE);
#endif /* PCIE_INB_DW */
			}
			bus->skip_ds_ack = FALSE;
			DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
		}
#endif /* PCIE_INB_DW */
		rc = dhdpcie_pci_suspend_resume(bus, state);
#if defined(LINUX) || defined(linux)
		dhdpcie_dump_resource(bus);
#endif /* LINUX || linux */

		/* set bus_low_power_state to DHD_BUS_NO_LOW_POWER_STATE */
		DHD_SET_BUS_NOT_IN_LPS(bus);

		if (!rc && bus->dhd->busstate == DHD_BUS_SUSPEND) {
			if (bus->use_d0_inform) {
				DHD_OS_WAKE_LOCK_WAIVE(bus->dhd);
				dhdpcie_send_mb_data(bus, (H2D_HOST_D0_INFORM));
				DHD_OS_WAKE_LOCK_RESTORE(bus->dhd);
			}
			/* ring doorbell 1 (hostready) */
			dhd_bus_hostready(bus);
		}
		DHD_GENERAL_LOCK(bus->dhd, flags);
		bus->dhd->busstate = DHD_BUS_DATA;
#ifdef PCIE_OOB
		/*
		 * Assert & Deassert the Device Wake. The following is the explanation for doing so.
		 * 0) At this point,
		 *    Host is in suspend state, Link is in L2/L3, Dongle is in D3 Cold
		 *    Device Wake is enabled.
		 * 1) When the Host comes out of Suspend, it first sends PERST# in the Link.
		 *    Looking at this the Dongle moves from D3 Cold to NO DS State
		 * 2) Now The Host OS calls the "resume" function of DHD. From here the DHD first
		 *    Asserts the Device Wake.
		 *    From the defn, when the Device Wake is asserted, The dongle FW will ensure
		 *    that the Dongle is out of deep sleep IF the device is already in deep sleep.
		 *    But note that now the Dongle is NOT in Deep sleep and is actually in
		 *    NO DS state. So just driving the Device Wake high does not trigger any state
		 *    transitions. The Host should actually "Toggle" the Device Wake to ensure
		 *    that Dongle synchronizes with the Host and starts the State Transition to D0.
		 * 4) Note that the above explanation is applicable Only when the Host comes out of
		 *    suspend and the Dongle comes out of D3 Cold
		 */
		/* This logic is not required when hostready is enabled */

		if (!bus->dhd->d2h_hostrdy_supported) {
			dhd_bus_set_device_wake(bus, TRUE);
			OSL_DELAY(1000);
			dhd_bus_set_device_wake(bus, FALSE);
		}

#endif /* PCIE_OOB */
		/* resume all interface network queue. */
		dhd_bus_start_queue(bus);

		/* Enable back the intmask which was cleared in DPC
		 * after getting D3_ACK.
		 */
		dhdpcie_bus_intr_enable(bus);
		bus->dngl_intmask_enable_count++;

		DHD_GENERAL_UNLOCK(bus->dhd, flags);

		if (bus->dhd->dhd_watchdog_ms_backup) {
			DHD_ERROR(("%s: Enabling wdtick after resume\n",
				__FUNCTION__));
			dhd_os_wd_timer(bus->dhd, bus->dhd->dhd_watchdog_ms_backup);
		}

		bus->last_resume_end_time = OSL_LOCALTIME_NS();

		/* Update TCM rd index for EDL ring */
		DHD_EDL_RING_TCM_RD_UPDATE(bus->dhd);

	}
	return rc;
}

#define BUS_SUSPEND	TRUE
#define BUS_RESUME	FALSE
int dhd_bus_suspend(dhd_pub_t *dhd)
{
	int ret;
#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
	/* TODO: Check whether the arguments are correct */
	ret = dhdpcie_bus_suspend(dhd->bus, TRUE, BUS_SUSPEND);
#else
	ret = dhdpcie_bus_suspend(dhd->bus, BUS_SUSPEND);
#endif
	return ret;
}

int dhd_bus_resume(dhd_pub_t *dhd, int stage)
{
	int ret;
	BCM_REFERENCE(stage);

#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
	/* TODO: Check whether the arguments are correct */
	ret = dhdpcie_bus_suspend(dhd->bus, FALSE, BUS_RESUME);
#else
	ret = dhdpcie_bus_suspend(dhd->bus, BUS_RESUME);
#endif
	return ret;
}

uint32
dhdpcie_force_alp(struct dhd_bus *bus, bool enable)
{
	ASSERT(bus && bus->sih);
	if (enable) {
	si_corereg(bus->sih, bus->sih->buscoreidx,
		OFFSETOF(sbpcieregs_t, u.pcie2.clk_ctl_st), CCS_FORCEALP, CCS_FORCEALP);
	} else {
		si_corereg(bus->sih, bus->sih->buscoreidx,
			OFFSETOF(sbpcieregs_t, u.pcie2.clk_ctl_st), CCS_FORCEALP, 0);
	}
	return 0;
}

/* set pcie l1 entry time: dhd pciereg 0x1004[22:16] */
uint32
dhdpcie_set_l1_entry_time(struct dhd_bus *bus, int l1_entry_time)
{
	uint reg_val;

	ASSERT(bus && bus->sih);

	si_corereg(bus->sih, bus->sih->buscoreidx, OFFSETOF(sbpcieregs_t, configaddr), ~0,
		0x1004);
	reg_val = si_corereg(bus->sih, bus->sih->buscoreidx,
		OFFSETOF(sbpcieregs_t, configdata), 0, 0);
	reg_val = (reg_val & ~(0x7f << 16)) | ((l1_entry_time & 0x7f) << 16);
	si_corereg(bus->sih, bus->sih->buscoreidx, OFFSETOF(sbpcieregs_t, configdata), ~0,
		reg_val);

	return 0;
}

static uint32
dhd_apply_d11_war_length(struct  dhd_bus *bus, uint32 len, uint32 d11_lpbk)
{
	uint16 chipid = si_chipid(bus->sih);
	/*
	 * XXX: WAR for CRWLDOT11M-3011
	 * program the DMA descriptor Buffer length as the expected frame length
	 *  + 8 bytes extra for corerev 82 when buffer length % 128 is equal to 4
	 */
	if ((chipid == BCM4375_CHIP_ID ||
		chipid == BCM4362_CHIP_ID ||
		chipid == BCM4377_CHIP_ID ||
		chipid == BCM43751_CHIP_ID ||
		chipid == BCM43752_CHIP_ID ||
		chipid == BCM43756_CHIP_ID ||
		chipid == BCM43711_CHIP_ID) &&
		(d11_lpbk != M2M_DMA_LPBK && d11_lpbk != M2M_NON_DMA_LPBK)) {
			len += 8;
	}
	DHD_ERROR(("%s: len %d\n", __FUNCTION__, len));
	return len;
}

/** Transfers bytes from host to dongle and to host again using DMA */
static int
dhdpcie_bus_dmaxfer_req(struct  dhd_bus *bus,
		uint32 len, uint32 srcdelay, uint32 destdelay,
		uint32 d11_lpbk, uint32 core_num, uint32 wait,
		uint32 mem_addr)
{
	int ret = 0;

	if (bus->dhd == NULL) {
		DHD_ERROR(("%s: bus not inited\n", __FUNCTION__));
		return BCME_ERROR;
	}
	if (bus->dhd->prot == NULL) {
		DHD_ERROR(("%s: prot is not inited\n", __FUNCTION__));
		return BCME_ERROR;
	}
	if (bus->dhd->busstate != DHD_BUS_DATA) {
		DHD_ERROR(("%s: not in a readystate to LPBK  is not inited\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (len < 5 || len > 4194296) {
		DHD_ERROR(("%s: len is too small or too large\n", __FUNCTION__));
		return BCME_ERROR;
	}

	len = dhd_apply_d11_war_length(bus, len, d11_lpbk);

	bus->dmaxfer_complete = FALSE;
	ret = dhdmsgbuf_dmaxfer_req(bus->dhd, len, srcdelay, destdelay,
		d11_lpbk, core_num, mem_addr);
	if (ret != BCME_OK || !wait) {
		DHD_INFO(("%s: dmaxfer req returns status %u; wait = %u\n", __FUNCTION__,
				ret, wait));
	} else {
		ret = dhd_os_dmaxfer_wait(bus->dhd, &bus->dmaxfer_complete);
		if (ret < 0)
			ret = BCME_NOTREADY;
	}

	return ret;

}

bool
dhd_bus_is_multibp_capable(struct dhd_bus *bus)
{
	return MULTIBP_CAP(bus->sih);
}

#define PCIE_REV_FOR_4378A0	66	/* dhd_bus_perform_flr_with_quiesce() causes problems */
#define PCIE_REV_FOR_4378B0	68

static int
dhdpcie_bus_download_state(dhd_bus_t *bus, bool enter)
{
	int bcmerror = 0;
	volatile uint32 *cr4_regs;
	bool do_flr;
	bool do_wr_flops = TRUE;

	if (!bus->sih) {
		DHD_ERROR(("%s: NULL sih!!\n", __FUNCTION__));
		return BCME_ERROR;
	}

	do_flr = ((bus->sih->buscorerev != PCIE_REV_FOR_4378A0) &&
			(bus->sih->buscorerev != PCIE_REV_FOR_4378B0));

	/*
	 * Jira SWWLAN-214966: 4378B0 BToverPCIe: fails to download firmware
	 *   with "insmod dhd.ko firmware_path=rtecdc.bin nvram_path=nvram.txt" format
	 *   CTO is seen during autoload case.
	 * Need to assert PD1 power req during ARM out of reset.
	 * And doing FLR after this would conflict as FLR resets PCIe enum space.
	 */
	if (MULTIBP_ENAB(bus->sih) && !do_flr) {
		dhd_bus_pcie_pwr_req(bus);
	}

	/* To enter download state, disable ARM and reset SOCRAM.
	 * To exit download state, simply reset ARM (default is RAM boot).
	 */
	if (enter) {
#ifndef BCMQT	/* for performance reasons, skip the FLR for QT */
#endif /* !BCMQT */

		/* Make sure BAR1 maps to backplane address 0 */
		dhdpcie_setbar1win(bus, 0x00000000);
		bus->alp_only = TRUE;
#ifdef GDB_PROXY
		bus->gdb_proxy_access_enabled = TRUE;
		bus->gdb_proxy_bootloader_mode = FALSE;
#endif /* GDB_PROXY */

		/* some chips (e.g. 43602) have two ARM cores, the CR4 is receives the firmware. */
		cr4_regs = si_setcore(bus->sih, ARMCR4_CORE_ID, 0);

		if (cr4_regs == NULL && !(si_setcore(bus->sih, ARM7S_CORE_ID, 0)) &&
		    !(si_setcore(bus->sih, ARMCM3_CORE_ID, 0)) &&
		    !(si_setcore(bus->sih, ARMCA7_CORE_ID, 0))) {
			DHD_ERROR(("%s: Failed to find ARM core!\n", __FUNCTION__));
			bcmerror = BCME_ERROR;
			goto fail;
		}

		if (si_setcore(bus->sih, ARMCA7_CORE_ID, 0)) {
			/* Halt ARM & remove reset */
			si_core_reset(bus->sih, SICF_CPUHALT, SICF_CPUHALT);
			if (!(si_setcore(bus->sih, SYSMEM_CORE_ID, 0))) {
				DHD_ERROR(("%s: Failed to find SYSMEM core!\n", __FUNCTION__));
				bcmerror = BCME_ERROR;
				goto fail;
			}
			si_core_reset(bus->sih, 0, 0);
			/* reset last 4 bytes of RAM address. to be used for shared area */
			dhdpcie_init_shared_addr(bus);
		} else if (cr4_regs == NULL) { /* no CR4 present on chip */
			si_core_disable(bus->sih, 0);

			if (!(si_setcore(bus->sih, SOCRAM_CORE_ID, 0))) {
				DHD_ERROR(("%s: Failed to find SOCRAM core!\n", __FUNCTION__));
				bcmerror = BCME_ERROR;
				goto fail;
			}

			si_core_reset(bus->sih, 0, 0);

			/* Clear the top bit of memory */
			if (bus->ramsize) {
				uint32 zeros = 0;
				if (dhdpcie_bus_membytes(bus, TRUE, bus->ramsize - 4,
				                     (uint8*)&zeros, 4) < 0) {
					bcmerror = BCME_ERROR;
					goto fail;
				}
			}
		} else {
			/* For CR4,
			 * Halt ARM
			 * Remove ARM reset
			 * Read RAM base address [0x18_0000]
			 * [next] Download firmware
			 * [done at else] Populate the reset vector
			 * [done at else] Remove ARM halt
			*/
			/* Halt ARM & remove reset */
			si_core_reset(bus->sih, SICF_CPUHALT, SICF_CPUHALT);
			if (BCM43602_CHIP(bus->sih->chip)) {
				/* XXX CRWLARMCR4-53 43602a0 HW bug when banks are powered down */
				W_REG(bus->pcie_mb_intr_osh, cr4_regs + ARMCR4REG_BANKIDX, 5);
				W_REG(bus->pcie_mb_intr_osh, cr4_regs + ARMCR4REG_BANKPDA, 0);
				W_REG(bus->pcie_mb_intr_osh, cr4_regs + ARMCR4REG_BANKIDX, 7);
				W_REG(bus->pcie_mb_intr_osh, cr4_regs + ARMCR4REG_BANKPDA, 0);
			}
			/* reset last 4 bytes of RAM address. to be used for shared area */
			dhdpcie_init_shared_addr(bus);
		}
	} else {
		if (si_setcore(bus->sih, ARMCA7_CORE_ID, 0)) {
			/* write vars */
			if ((bcmerror = dhdpcie_bus_write_vars(bus))) {
				DHD_ERROR(("%s: could not write vars to RAM\n", __FUNCTION__));
				goto fail;
			}
			/* write random numbers to sysmem for the purpose of
			 * randomizing heap address space.
			 */
			if ((bcmerror = dhdpcie_wrt_rnd(bus)) != BCME_OK) {
				DHD_ERROR(("%s: Failed to get random seed to write to TCM !\n",
					__FUNCTION__));
				goto fail;
			}

#if defined(FW_SIGNATURE)
			if ((bcmerror = dhdpcie_bus_download_fw_signature(bus, &do_wr_flops))
				!= BCME_OK) {
				goto fail;
			}
#endif /* FW_SIGNATURE */

			if (do_wr_flops) {
				uint32 resetinstr_data = 0;

				/* switch back to arm core again */
				if (!(si_setcore(bus->sih, ARMCA7_CORE_ID, 0))) {
					DHD_ERROR(("%s: Failed to find ARM CA7 core!\n",
						__FUNCTION__));
					bcmerror = BCME_ERROR;
					goto fail;
				}

				bcmerror = dhd_bus_get_security_status(bus, &resetinstr_data);
				if (bcmerror == BCME_UNSUPPORTED) {
					/*
					 * read address 0 with reset instruction,
					 * to validate that is not secured
					 */
					bcmerror = dhdpcie_bus_membytes(bus, FALSE, 0,
						(uint8 *)&resetinstr_data, sizeof(resetinstr_data));
				}
				if ((resetinstr_data == 0xFFFFFFFF) ||
					(resetinstr_data & DAR_SEC_SBOOT_MASK)) {
					DHD_ERROR(("%s: **** FLOPS Vector is secured, "
						"Signature file is missing! ***\n", __FUNCTION__));
					bcmerror = BCME_NO_SIG_FILE;
					goto fail;
				}
				/* write address 0 with reset instruction */
				bcmerror = dhdpcie_bus_membytes(bus, TRUE, 0,
					(uint8 *)&bus->resetinstr, sizeof(bus->resetinstr));
				/* now remove reset and halt and continue to run CA7 */
			}
		} else if (!si_setcore(bus->sih, ARMCR4_CORE_ID, 0)) {
			if (!(si_setcore(bus->sih, SOCRAM_CORE_ID, 0))) {
				DHD_ERROR(("%s: Failed to find SOCRAM core!\n", __FUNCTION__));
				bcmerror = BCME_ERROR;
				goto fail;
			}

			if (!si_iscoreup(bus->sih)) {
				DHD_ERROR(("%s: SOCRAM core is down after reset?\n", __FUNCTION__));
				bcmerror = BCME_ERROR;
				goto fail;
			}

			/* Enable remap before ARM reset but after vars.
			 * No backplane access in remap mode
			 */
			if (!si_setcore(bus->sih, PCMCIA_CORE_ID, 0) &&
			    !si_setcore(bus->sih, SDIOD_CORE_ID, 0)) {
				DHD_ERROR(("%s: Can't change back to SDIO core?\n", __FUNCTION__));
				bcmerror = BCME_ERROR;
				goto fail;
			}

			/* XXX Change standby configuration here if necessary */

			if (!(si_setcore(bus->sih, ARM7S_CORE_ID, 0)) &&
			    !(si_setcore(bus->sih, ARMCM3_CORE_ID, 0))) {
				DHD_ERROR(("%s: Failed to find ARM core!\n", __FUNCTION__));
				bcmerror = BCME_ERROR;
				goto fail;
			}
		} else {
			if (BCM43602_CHIP(bus->sih->chip)) {
				/* Firmware crashes on SOCSRAM access when core is in reset */
				if (!(si_setcore(bus->sih, SOCRAM_CORE_ID, 0))) {
					DHD_ERROR(("%s: Failed to find SOCRAM core!\n",
						__FUNCTION__));
					bcmerror = BCME_ERROR;
					goto fail;
				}
				si_core_reset(bus->sih, 0, 0);
				si_setcore(bus->sih, ARMCR4_CORE_ID, 0);
			}

			/* write vars */
			if ((bcmerror = dhdpcie_bus_write_vars(bus))) {
				DHD_ERROR(("%s: could not write vars to RAM\n", __FUNCTION__));
				goto fail;
			}

			/* write a random number rTLV to TCM for the purpose of
			 * randomizing heap address space.
			 */
			if ((bcmerror = dhdpcie_wrt_rnd(bus)) != BCME_OK) {
				DHD_ERROR(("%s: Failed to get random seed to write to TCM !\n",
					__FUNCTION__));
				goto fail;
			}

#if defined(FW_SIGNATURE)
			if ((bcmerror = dhdpcie_bus_download_fw_signature(bus, &do_wr_flops))
				!= BCME_OK) {
				goto fail;
			}
#endif /* FW_SIGNATURE */
			if (do_wr_flops) {
				uint32 resetinstr_data;

				/* switch back to arm core again */
				if (!(si_setcore(bus->sih, ARMCR4_CORE_ID, 0))) {
					DHD_ERROR(("%s: Failed to find ARM CR4 core!\n",
						__FUNCTION__));
					bcmerror = BCME_ERROR;
					goto fail;
				}

				/*
				 * read address 0 with reset instruction,
				 * to validate that is not secured
				 */
				bcmerror = dhdpcie_bus_membytes(bus, FALSE, 0,
					(uint8 *)&resetinstr_data, sizeof(resetinstr_data));

				if (resetinstr_data == 0xFFFFFFFF) {
					DHD_ERROR(("%s: **** FLOPS Vector is secured, "
						"Signature file is missing! ***\n", __FUNCTION__));
					bcmerror = BCME_NO_SIG_FILE;
					goto fail;
				}

				/* write address 0 with reset instruction */
				bcmerror = dhdpcie_bus_membytes(bus, TRUE, 0,
					(uint8 *)&bus->resetinstr, sizeof(bus->resetinstr));

				if (bcmerror == BCME_OK) {
					uint32 tmp;

					bcmerror = dhdpcie_bus_membytes(bus, FALSE, 0,
					                                (uint8 *)&tmp, sizeof(tmp));

					if (bcmerror == BCME_OK && tmp != bus->resetinstr) {
						DHD_ERROR(("%s: Failed to write 0x%08x to addr 0\n",
						          __FUNCTION__, bus->resetinstr));
						DHD_ERROR(("%s: contents of addr 0 is 0x%08x\n",
						          __FUNCTION__, tmp));
						bcmerror = BCME_ERROR;
						goto fail;
					}
				}
				/* now remove reset and halt and continue to run CR4 */
			}
		}

		bus->arm_oor_time = OSL_LOCALTIME_NS();
		si_core_reset(bus->sih, 0, 0);

		DHD_ERROR(("%s: Took ARM out of Reset\n", __FUNCTION__));

		/* Allow HT Clock now that the ARM is running. */
		bus->alp_only = FALSE;

		bus->dhd->busstate = DHD_BUS_LOAD;
	}

fail:
	/* Always return to PCIE core */
	si_setcore(bus->sih, PCIE2_CORE_ID, 0);

	if (MULTIBP_ENAB(bus->sih) && !do_flr) {
		dhd_bus_pcie_pwr_req_clear(bus);
	}

	return bcmerror;
} /* dhdpcie_bus_download_state */

#if defined(FW_SIGNATURE)

static int
dhdpcie_bus_download_fw_signature(dhd_bus_t *bus, bool *do_write)
{
	int bcmerror = BCME_OK;

	DHD_INFO(("FWSIG: bl=%s,%x fw=%x,%u sig=%s,%x,%u"
		" stat=%x,%u ram=%x,%x\n",
		bus->bootloader_filename, bus->bootloader_addr,
		bus->fw_download_addr, bus->fw_download_len,
		bus->fwsig_filename, bus->fwsig_download_addr,
		bus->fwsig_download_len,
		bus->fwstat_download_addr, bus->fwstat_download_len,
		bus->dongle_ram_base, bus->ramtop_addr));

	if (bus->fwsig_filename[0] == 0) {
		DHD_INFO(("%s: missing signature file\n", __FUNCTION__));
		goto exit;
	}

	/* Write RAM Bootloader to TCM if requested */
	if ((bcmerror = dhdpcie_bus_download_ram_bootloader(bus))
		!= BCME_OK) {
		DHD_ERROR(("%s: could not write RAM BL to TCM, err %d\n",
			__FUNCTION__, bcmerror));
		goto exit;
	}

	/* Write FW signature to memory */
	if ((bcmerror = dhdpcie_bus_write_fw_signature(bus))) {
		DHD_ERROR(("%s: could not write FWsig , err %d\n",
			__FUNCTION__, bcmerror));
		goto exit;
	}

	/* In case of BL RAM, do write flops */
	if (bus->bootloader_filename[0] != 0) {
		*do_write = TRUE;
	} else {
		*do_write = FALSE;
	}

exit:
	return bcmerror;
}

/* Complete RAM structure, write the followings
 * signature image
 * signature verification status
 * FW memory map
 * end-of-TLVs marker
 */
static int
dhdpcie_bus_write_fw_signature(dhd_bus_t *bus)
{
	int bcmerror = BCME_OK;

	/* Write FW signature rTLV to TCM */
	if ((bcmerror = dhdpcie_bus_write_fwsig(bus, bus->fwsig_filename,
		NULL))) {
		DHD_ERROR(("%s: could not write FWsig to TCM, err %d\n",
			__FUNCTION__, bcmerror));
		goto exit;
	}

	/* Write FW signature verification status rTLV to TCM */
	if ((bcmerror = dhdpcie_bus_write_fws_status(bus)) != BCME_OK) {
		DHD_ERROR(("%s: could not write FWinfo to TCM, err %d\n",
			__FUNCTION__, bcmerror));
		goto exit;
	}

	/* Write FW memory map rTLV to TCM */
	if ((bcmerror = dhdpcie_bus_write_fws_mem_info(bus)) != BCME_OK) {
		DHD_ERROR(("%s: could not write FWinfo to TCM, err %d\n",
			__FUNCTION__, bcmerror));
		goto exit;
	}

	/* Write a end-of-TLVs marker to TCM */
	if ((bcmerror = dhdpcie_download_rtlv_end(bus)) != BCME_OK) {
		DHD_ERROR(("%s: could not write rTLV-end marker to TCM, err %d\n",
			__FUNCTION__, bcmerror));
		goto exit;
	}

exit:
	return bcmerror;
}
#endif /* defined(FW_SIGNATURE) */

/* Download a reversed-TLV to the top of dongle RAM without overlapping any existing rTLVs */
static int
dhdpcie_download_rtlv(dhd_bus_t *bus, dngl_rtlv_type_t type, dngl_rtlv_len_t len, uint8 *value)
{
	int bcmerror = BCME_OK;
#ifdef DHD_DEBUG
	uint8 *readback_buf = NULL;
	uint32 readback_val = 0;
#endif /* DHD_DEBUG */
	uint32 dest_addr = 0;		/* dongle RAM dest address */
	uint32 dest_size = 0;		/* dongle RAM dest size */
	uint32 dest_raw_size = 0;	/* dest size with added checksum */

	/* Calculate the destination dongle RAM address and size */
	dest_size = ROUNDUP(len, 4);
	dest_addr = bus->ramtop_addr - sizeof(dngl_rtlv_type_t) - sizeof(dngl_rtlv_len_t)
		- dest_size;
	bus->ramtop_addr = dest_addr;

	/* Create the rTLV size field.  This consists of 2 16-bit fields:
	 * The lower 16 bits is the size.  The higher 16 bits is a checksum
	 * consisting of the size with all bits reversed.
	 *     +-------------+-------------+
	 *     |   checksum  |   size      |
	 *     +-------------+-------------+
	 *      High 16 bits    Low 16 bits
	 */
	if (type == DNGL_RTLV_TYPE_RNG_SIGNATURE) {
		/* Random number seed TLV (DNGL_RTLV_TYPE_RNG_SIGNATURE) was the very
		 * first TLV, its length does not have checksum, firmware crashes if
		 * checksum is present
		 */
		dest_raw_size = dest_size & 0x0000FFFF;
	} else {
		dest_raw_size = (~dest_size << 16) | (dest_size & 0x0000FFFF);
	}
	/* Write the value block */
	if (dest_size > 0) {
		bcmerror = dhdpcie_bus_membytes(bus, TRUE, dest_addr, value, dest_size);
		if (bcmerror) {
			DHD_ERROR(("%s: error %d on writing %d membytes to 0x%08x\n",
				__FUNCTION__, bcmerror, dest_size, dest_addr));
			goto exit;
		}
	}

	/* Write the length word */
	bcmerror = dhdpcie_bus_membytes(bus, TRUE, dest_addr + dest_size,
		(uint8*)&dest_raw_size, sizeof(dngl_rtlv_len_t));

	/* Write the type word */
	bcmerror = dhdpcie_bus_membytes(bus, TRUE,
		dest_addr + dest_size + sizeof(dngl_rtlv_len_t),
		(uint8*)&type, sizeof(dngl_rtlv_type_t));

#ifdef DHD_DEBUG
	/* Read back and compare the downloaded data */
	if (dest_size > 0) {
		readback_buf = (uint8*)MALLOC(bus->dhd->osh, dest_size);
		if (!readback_buf) {
			bcmerror = BCME_NOMEM;
			goto exit;
		}
		memset(readback_buf, 0xaa, dest_size);
		bcmerror = dhdpcie_bus_membytes(bus, FALSE, dest_addr, readback_buf, dest_size);
		if (bcmerror) {
			DHD_ERROR(("%s: readback error %d, %d bytes from 0x%08x\n",
				__FUNCTION__, bcmerror, dest_size, dest_addr));
			goto exit;
		}
		if (memcmp(value, readback_buf, dest_size) != 0) {
			DHD_ERROR(("%s: Downloaded data mismatch.\n", __FUNCTION__));
			bcmerror = BCME_ERROR;
			goto exit;
		} else {
			DHD_ERROR(("Download and compare of TLV 0x%x succeeded"
				" (size %u, addr %x).\n", type, dest_size, dest_addr));
		}
	}

	/* Read back and compare the downloaded len field */
	bcmerror = dhdpcie_bus_membytes(bus, FALSE, dest_addr + dest_size,
		(uint8*)&readback_val, sizeof(dngl_rtlv_len_t));
	if (!bcmerror) {
		if (readback_val != dest_raw_size) {
			bcmerror = BCME_BADLEN;
		}
	}
	if (bcmerror) {
		DHD_ERROR(("%s: Downloaded len error %d\n", __FUNCTION__, bcmerror));
		goto exit;
	}

	/* Read back and compare the downloaded type field */
	bcmerror = dhdpcie_bus_membytes(bus, FALSE,
		dest_addr + dest_size + sizeof(dngl_rtlv_len_t),
		(uint8*)&readback_val, sizeof(dngl_rtlv_type_t));
	if (!bcmerror) {
		if (readback_val != type) {
			bcmerror = BCME_BADOPTION;
		}
	}
	if (bcmerror) {
		DHD_ERROR(("%s: Downloaded type error %d\n", __FUNCTION__, bcmerror));
		goto exit;
	}
#endif /* DHD_DEBUG */

	bus->ramtop_addr = dest_addr;

exit:
#ifdef DHD_DEBUG
	if (readback_buf) {
		MFREE(bus->dhd->osh, readback_buf, dest_size);
	}
#endif /* DHD_DEBUG */

	return bcmerror;
} /* dhdpcie_download_rtlv */

#if defined(FW_SIGNATURE)
/* Download a reversed-TLV END marker to the top of dongle RAM */
static int
dhdpcie_download_rtlv_end(dhd_bus_t *bus)
{
	return dhdpcie_download_rtlv(bus, DNGL_RTLV_TYPE_END_MARKER, 0, NULL);
}

/* Write the FW signature verification status to dongle memory */
static int
dhdpcie_bus_write_fws_status(dhd_bus_t *bus)
{
	bl_verif_status_t vstatus;
	int ret;

	bzero(&vstatus, sizeof(vstatus));

	ret = dhdpcie_download_rtlv(bus, DNGL_RTLV_TYPE_FWSIGN_STATUS, sizeof(vstatus),
		(uint8*)&vstatus);
	bus->fwstat_download_addr = bus->ramtop_addr;
	bus->fwstat_download_len = sizeof(vstatus);

	return ret;
} /* dhdpcie_bus_write_fws_status */

/* Write the FW signature verification memory map to dongle memory */
static int
dhdpcie_bus_write_fws_mem_info(dhd_bus_t *bus)
{
	bl_mem_info_t memmap;
	int ret;

	bzero(&memmap, sizeof(memmap));
	memmap.firmware.start = bus->fw_download_addr;
	memmap.firmware.end = memmap.firmware.start + bus->fw_download_len;
	memmap.heap.start = ROUNDUP(memmap.firmware.end + BL_HEAP_START_GAP_SIZE, 4);
	memmap.heap.end = memmap.heap.start + BL_HEAP_SIZE;
	memmap.signature.start = bus->fwsig_download_addr;
	memmap.signature.end = memmap.signature.start + bus->fwsig_download_len;
	memmap.vstatus.start = bus->fwstat_download_addr;
	memmap.vstatus.end = memmap.vstatus.start + bus->fwstat_download_len;
	DHD_INFO(("%s: mem_info: fw=%x-%x heap=%x-%x sig=%x-%x vst=%x-%x res=%x\n",
		__FUNCTION__,
		memmap.firmware.start, memmap.firmware.end,
		memmap.heap.start, memmap.heap.end,
		memmap.signature.start, memmap.signature.end,
		memmap.vstatus.start, memmap.vstatus.end,
		memmap.reset_vec.start));

	ret = dhdpcie_download_rtlv(bus, DNGL_RTLV_TYPE_FWSIGN_MEM_MAP, sizeof(memmap),
		(uint8*)&memmap);
	bus->fw_memmap_download_addr = bus->ramtop_addr;
	bus->fw_memmap_download_len = sizeof(memmap);

	return ret;
} /* dhdpcie_bus_write_fws_mem_info */

/* Download a bootloader image to dongle RAM */
static int
dhdpcie_bus_download_ram_bootloader(dhd_bus_t *bus)
{
	int ret = BCME_OK;
	uint32 dongle_ram_base_save;

	DHD_INFO(("download_bloader: %s,0x%x. ramtop=0x%x\n",
		bus->bootloader_filename, bus->bootloader_addr, bus->ramtop_addr));
	if (bus->bootloader_filename[0] == '\0') {
		return ret;
	}

	/* Save ram base */
	dongle_ram_base_save = bus->dongle_ram_base;

	/* Set ram base to bootloader download start address */
	bus->dongle_ram_base = bus->bootloader_addr;

	/* Download the bootloader image to TCM */
	ret = dhdpcie_download_code_file(bus, bus->bootloader_filename);

	/* Restore ram base */
	bus->dongle_ram_base = dongle_ram_base_save;

	return ret;
} /* dhdpcie_bus_download_ram_bootloader */

/* Save the FW download address and size */
static int
dhdpcie_bus_save_download_info(dhd_bus_t *bus, uint32 download_addr,
	uint32 download_size, const char *signature_fname,
	const char *bloader_fname, uint32 bloader_download_addr)
{
	/* DHD EXE may not have size of the downloaded FW image in case of swpaging binary */
	if (download_size) {
		bus->fw_download_len = download_size;
	}
	bus->fw_download_addr = download_addr;
	strlcpy(bus->fwsig_filename, signature_fname, sizeof(bus->fwsig_filename));
	strlcpy(bus->bootloader_filename, bloader_fname, sizeof(bus->bootloader_filename));
	bus->bootloader_addr = bloader_download_addr;
#ifdef GDB_PROXY
	/* GDB proxy bootloader mode - if signature file specified (i.e.
	 * bootloader is used), but bootloader is not specified (i.e. ROM
	 * bootloader is uses).
	 * Bootloader mode is significant only for for preattachment debugging
	 * of chips, in which debug cell can't be initialized before ARM CPU
	 * start
	 */
	bus->gdb_proxy_bootloader_mode = bus->fwsig_filename[0] != 0;
#endif /* GDB_PROXY */
	return BCME_OK;
} /* dhdpcie_bus_save_download_info */

/* Read a binary file and write it to the specified socram dest address */
#ifdef DHD_LINUX_STD_FW_API
static int
dhdpcie_download_sig_file(dhd_bus_t *bus, char *path, uint32 type)
{
	int bcmerror = BCME_ERROR;
	int srcsize = 0;
	uint32 dest_size = 0;	/* dongle RAM dest size */
	const struct firmware *sig = NULL;

	if (path == NULL || path[0] == '\0') {
		DHD_ERROR(("%s: no file\n", __FUNCTION__));
		bcmerror = BCME_NOTFOUND;
		goto exit;
	}

	bcmerror = dhd_os_get_img_fwreq(&sig, bus->fwsig_filename);
	if (bcmerror < 0) {
		DHD_ERROR(("dhd_os_get_img(Request Firmware API) error : %d\n",
			bcmerror));
		goto exit;
	}
	DHD_ERROR(("%s: dhd_os_get_img(Request Firmware API) success. size %d.\n",
		__func__, sig->size));

	srcsize = sig->size;
	if (srcsize <= 0 || srcsize > MEMBLOCK) {
		DHD_ERROR(("%s: invalid fwsig size %u\n", __FUNCTION__, srcsize));
		bcmerror = BCME_BUFTOOSHORT;
		goto exit;
	}

	dest_size = ROUNDUP(srcsize, 4);
	/* Write the src buffer as a rTLV to the dongle */
	GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
	bcmerror = dhdpcie_download_rtlv(bus, type, dest_size, (uint8 *)sig->data);
	GCC_DIAGNOSTIC_POP();
	if (bcmerror) {
		DHD_ERROR(("%s: error %d on writing %d membytes at 0x%08x\n",
			__FUNCTION__, bcmerror, srcsize, bus->ramtop_addr));
		goto exit;
	}

	bus->fwsig_download_addr = bus->ramtop_addr;
	bus->fwsig_download_len = dest_size;
exit:
	if (sig) {
		dhd_os_close_img_fwreq(sig);
	}

	return bcmerror;
}
#else
static int
dhdpcie_download_sig_file(dhd_bus_t *bus, char *path, uint32 type)
{
	int bcmerror = BCME_OK;
	void *filep = NULL;
	uint8 *srcbuf = NULL;
	int srcsize = 0;
	int len;
	uint32 dest_size = 0;	/* dongle RAM dest size */
	fwpkg_info_t *fwpkg = NULL;

	if (path == NULL || path[0] == '\0') {
		DHD_ERROR(("%s: no file\n", __FUNCTION__));
		bcmerror = BCME_NOTFOUND;
		goto exit;
	}

	bcmerror = fwpkg_init(&bus->fwpkg, path);
	if (bcmerror == BCME_ERROR) {
		goto exit;
	}
	fwpkg = &bus->fwpkg;

	/* Open file, get size */
	bcmerror = fwpkg_open_signature_img(fwpkg, path, &filep);
	if (bcmerror == BCME_ERROR) {
		DHD_ERROR(("%s: error opening file %s\n", __FUNCTION__, path));
		goto exit;
	}

	srcsize = fwpkg_get_signature_img_size(fwpkg);

	if (srcsize <= 0 || srcsize > MEMBLOCK) {
		DHD_ERROR(("%s: invalid fwsig size %u\n", __FUNCTION__, srcsize));
		bcmerror = BCME_BUFTOOSHORT;
		goto exit;
	}
	dest_size = ROUNDUP(srcsize, 4);

	/* Allocate src buffer, read in the entire file */
	srcbuf = (uint8 *)MALLOCZ(bus->dhd->osh, dest_size);
	if (!srcbuf) {
		bcmerror = BCME_NOMEM;
		goto exit;
	}
	len = dhd_os_get_image_block(srcbuf, srcsize, filep);
	if (len != srcsize) {
		DHD_ERROR(("%s: dhd_os_get_image_block failed (%d)\n", __FUNCTION__, len));
		bcmerror = BCME_BADLEN;
		goto exit;
	}

	/* Write the src buffer as a rTLV to the dongle */
	bcmerror = dhdpcie_download_rtlv(bus, type, dest_size, srcbuf);

	bus->fwsig_download_addr = bus->ramtop_addr;
	bus->fwsig_download_len = dest_size;

exit:
	if (filep) {
		dhd_os_close_image1(bus->dhd, filep);
	}
	if (srcbuf) {
		MFREE(bus->dhd->osh, srcbuf, dest_size);
	}

	return bcmerror;
} /* dhdpcie_download_sig_file */
#endif /* DHD_LINUX_STD_FW_API */

static int
dhdpcie_bus_write_fwsig(dhd_bus_t *bus, char *fwsig_path, char *nvsig_path)
{
	int bcmerror = BCME_OK;

	/* Download the FW signature file to the chip */
	bcmerror = dhdpcie_download_sig_file(bus, fwsig_path, DNGL_RTLV_TYPE_FW_SIGNATURE);
	if (bcmerror) {
		goto exit;
	}

exit:
	if (bcmerror) {
		DHD_ERROR(("%s: error %d\n", __FUNCTION__, bcmerror));
	}
	return bcmerror;
} /* dhdpcie_bus_write_fwsig */

static int
dhdpcie_read_fwstatus(dhd_bus_t *bus, bl_verif_status_t *status)
{
	int ret = BCME_OK;

	bzero(status, sizeof(*status));
	if (bus->fwstat_download_addr != 0) {
		ret = dhdpcie_bus_membytes(bus, FALSE, bus->fwstat_download_addr,
			(uint8 *)status, sizeof(*status));
		if (ret != BCME_OK) {
			DHD_ERROR(("%s: error %d on reading %zu membytes at 0x%08x\n",
				__FUNCTION__, ret, sizeof(*status), bus->fwstat_download_addr));
		}
	}

	return ret;
}

/* Dump secure firmware status. */
static int
dhd_bus_dump_fws(dhd_bus_t *bus, struct bcmstrbuf *strbuf)
{
	bl_verif_status_t status;
	bl_mem_info_t     meminfo;
	int               err = BCME_OK;

	err = dhdpcie_read_fwstatus(bus, &status);
	if (err != BCME_OK) {
		return (err);
	}

	bzero(&meminfo, sizeof(meminfo));
	if (bus->fw_memmap_download_addr != 0) {
		err = dhdpcie_bus_membytes(bus, FALSE, bus->fw_memmap_download_addr,
			(uint8 *)&meminfo, sizeof(meminfo));
		if (err != BCME_OK) {
			DHD_ERROR(("%s: error %d on reading %zu membytes at 0x%08x\n",
				__FUNCTION__, err, sizeof(meminfo), bus->fw_memmap_download_addr));
			return (err);
		}
	}

	bcm_bprintf(strbuf, "Firmware signing\nSignature: (%08x) len (%d)\n",
		bus->fwsig_download_addr, bus->fwsig_download_len);

	bcm_bprintf(strbuf,
		"Verification status: (%08x)\n"
		"\tstatus: %d\n"
		"\tstate: %u\n"
		"\talloc_bytes: %u\n"
		"\tmax_alloc_bytes: %u\n"
		"\ttotal_alloc_bytes: %u\n"
		"\ttotal_freed_bytes: %u\n"
		"\tnum_allocs: %u\n"
		"\tmax_allocs: %u\n"
		"\tmax_alloc_size: %u\n"
		"\talloc_failures: %u\n",
		bus->fwstat_download_addr,
		status.status,
		status.state,
		status.alloc_bytes,
		status.max_alloc_bytes,
		status.total_alloc_bytes,
		status.total_freed_bytes,
		status.num_allocs,
		status.max_allocs,
		status.max_alloc_size,
		status.alloc_failures);

	bcm_bprintf(strbuf,
		"Memory info: (%08x)\n"
		"\tfw   %08x-%08x\n\theap %08x-%08x\n\tsig  %08x-%08x\n\tvst  %08x-%08x\n",
		bus->fw_memmap_download_addr,
		meminfo.firmware.start,  meminfo.firmware.end,
		meminfo.heap.start,      meminfo.heap.end,
		meminfo.signature.start, meminfo.signature.end,
		meminfo.vstatus.start,   meminfo.vstatus.end);

	return (err);
}
#endif /* FW_SIGNATURE */

/* Write nvram data to the top of dongle RAM, ending with a size in # of 32-bit words */
static int
dhdpcie_bus_write_vars(dhd_bus_t *bus)
{
	int bcmerror = 0;
	uint32 varsize, phys_size;
	uint32 varaddr;
	uint8 *vbuffer;
	uint32 varsizew;
	uint32 rd_varsizew;
#ifdef DHD_DEBUG
	uint8 *nvram_ularray;
#endif /* DHD_DEBUG */

	/* Even if there are no vars are to be written, we still need to set the ramsize. */
	varsize = bus->varsz ? ROUNDUP(bus->varsz, 4) : 0;
	varaddr = (bus->ramsize - 4) - varsize;

	varaddr += bus->dongle_ram_base;
	bus->ramtop_addr = varaddr;

	if (bus->vars) {

		/* XXX In case the controller has trouble with odd bytes... */
		vbuffer = (uint8 *)MALLOC(bus->dhd->osh, varsize);
		if (!vbuffer)
			return BCME_NOMEM;

		bzero(vbuffer, varsize);
		bcopy(bus->vars, vbuffer, bus->varsz);
		/* Write the vars list */
		bcmerror = dhdpcie_bus_membytes(bus, TRUE, varaddr, vbuffer, varsize);

		/* Implement read back and verify later */
#ifdef DHD_DEBUG
		/* Verify NVRAM bytes */
		DHD_INFO(("%s: Compare NVRAM dl & ul; varsize=%d\n", __FUNCTION__, varsize));
		nvram_ularray = (uint8*)MALLOC(bus->dhd->osh, varsize);
		if (!nvram_ularray) {
			MFREE(bus->dhd->osh, vbuffer, varsize);
			return BCME_NOMEM;
		}

		/* Upload image to verify downloaded contents. */
		memset(nvram_ularray, 0xaa, varsize);

		/* Read the vars list to temp buffer for comparison */
		bcmerror = dhdpcie_bus_membytes(bus, FALSE, varaddr, nvram_ularray, varsize);
		if (bcmerror) {
				DHD_ERROR(("%s: error %d on reading %d nvram bytes at 0x%08x\n",
					__FUNCTION__, bcmerror, varsize, varaddr));
		}

		/* Compare the org NVRAM with the one read from RAM */
		if (memcmp(vbuffer, nvram_ularray, varsize)) {
			DHD_ERROR(("%s: Downloaded NVRAM image is corrupted.\n", __FUNCTION__));
			prhex("nvram file", vbuffer, varsize);
			prhex("downloaded nvram", nvram_ularray, varsize);
			MFREE(bus->dhd->osh, nvram_ularray, varsize);
			MFREE(bus->dhd->osh, vbuffer, varsize);
			return BCME_ERROR;
		} else
			DHD_ERROR(("%s: Download, Upload and compare of NVRAM succeeded.\n",
			__FUNCTION__));

		MFREE(bus->dhd->osh, nvram_ularray, varsize);
#endif /* DHD_DEBUG */

		MFREE(bus->dhd->osh, vbuffer, varsize);
	}

	phys_size = REMAP_ENAB(bus) ? bus->ramsize : bus->orig_ramsize;

	phys_size += bus->dongle_ram_base;

	/* adjust to the user specified RAM */
	DHD_INFO(("%s: Physical memory size: %d, usable memory size: %d\n", __FUNCTION__,
		phys_size, bus->ramsize));
	DHD_INFO(("%s: Vars are at %d, orig varsize is %d\n", __FUNCTION__,
		varaddr, varsize));
	varsize = ((phys_size - 4) - varaddr);

	/*
	 * Determine the length token:
	 * Varsize, converted to words, in lower 16-bits, checksum in upper 16-bits.
	 */
	if (bcmerror) {
		varsizew = 0;
		bus->nvram_csm = varsizew;
	} else {
		varsizew = varsize / 4;
		varsizew = (~varsizew << 16) | (varsizew & 0x0000FFFF);
		bus->nvram_csm = varsizew;
		varsizew = htol32(varsizew);
	}

	DHD_ERROR(("%s: New varsize is %d, length token(nvram_csm)=0x%08x\n", __FUNCTION__, varsize, varsizew));

	/* Write the length token to the last word */
	bcmerror = dhdpcie_bus_membytes(bus, TRUE, (phys_size - 4),
		(uint8*)&varsizew, 4);

	/* Read back the last word */
	bcmerror = dhdpcie_bus_membytes(bus, FALSE, (phys_size - 4),
		(uint8*)&rd_varsizew, 4);
	if (rd_varsizew != varsizew) {
		DHD_ERROR(("%s: readback nvram_csm(0x%08x:0x%08x) is wrong at 0x%x\n",
			__FUNCTION__, rd_varsizew, varsizew, (phys_size - 4)));
	}

	return bcmerror;
} /* dhdpcie_bus_write_vars */

int
dhdpcie_downloadvars(dhd_bus_t *bus, void *arg, int len)
{
	int bcmerror = BCME_OK;
#ifdef KEEP_JP_REGREV
	/* XXX Needed by customer's request */
	char *tmpbuf;
	uint tmpidx;
#endif /* KEEP_JP_REGREV */
#ifdef GDB_PROXY
	const char nodeadman_record[] = "deadman_to=0";
#endif /* GDB_PROXY */
#ifdef DHD_SUPPORT_SPMI_MODE
	char tag_spmi_mode[16] = {0};
#endif /* DHD_SUPPORT_SPMI_MODE */

	DHD_TRACE(("%s: Enter\n", __FUNCTION__));

	if (!len) {
		bcmerror = BCME_BUFTOOSHORT;
		goto err;
	}

	/* Free the old ones and replace with passed variables */
	if (bus->vars)
		MFREE(bus->dhd->osh, bus->vars, bus->varsz);
#ifdef GDB_PROXY
	if (bus->dhd->gdb_proxy_nodeadman) {
		len += sizeof(nodeadman_record);
	}
#endif /* GDB_PROXY */
#ifdef DHD_SUPPORT_SPMI_MODE
	if (bus->dhd->dhd_spmi_mode) {
		snprintf(tag_spmi_mode, sizeof(tag_spmi_mode), "spmi_mode=%d\n",
			bus->dhd->dhd_spmi_mode);
		len += sizeof(tag_spmi_mode);
	}
#endif /* DHD_SUPPORT_SPMI_MODE */

	bus->vars = MALLOC(bus->dhd->osh, len);
	bus->varsz = bus->vars ? len : 0;
	if (bus->vars == NULL) {
		bcmerror = BCME_NOMEM;
		goto err;
	}

	/* Copy the passed variables, which should include the terminating double-null */
	bcopy(arg, bus->vars, bus->varsz);
#ifdef GDB_PROXY
	if (bus->dhd->gdb_proxy_nodeadman &&
		!replace_nvram_variable(bus->vars, bus->varsz, nodeadman_record, NULL))
	{
		bcmerror = BCME_NOMEM;
		goto err;
	}
#endif /* GDB_PROXY */
#ifdef DHD_SUPPORT_SPMI_MODE
	if (bus->dhd->dhd_spmi_mode) {
		char *pos = NULL;
		char tag_spmi[] = "spmi=";
		uint spmi = 0;

		/* check if the spmi feature is enabled */
		pos = bcmstrnstr(bus->vars, bus->varsz, tag_spmi, strlen(tag_spmi));
		if (pos) {
			sscanf(pos, "spmi=%u\n", &spmi);
			DHD_INFO(("%s: spmi feature is %s\n", __FUNCTION__,
				spmi ? "enabled" : "disabled"));
		}

		if (spmi && !replace_nvram_variable(bus->vars,
			bus->varsz, tag_spmi_mode, NULL)) {
			bcmerror = BCME_NOMEM;
			goto err;
		}
	}
#endif /* DHD_SUPPORT_SPMI_MODE */

	/* Re-Calculate htclkratio only for QT, for FPGA it is fixed at 30 */
#ifdef BCMQT_HW
	dhdpcie_htclkratio_recal(bus, bus->vars, bus->varsz);
#endif

#ifdef DHD_USE_SINGLE_NVRAM_FILE
	/* XXX Change the default country code only for MFG firmware */
	if (dhd_bus_get_fw_mode(bus->dhd) == DHD_FLAG_MFG_MODE) {
		char *sp = NULL;
		char *ep = NULL;
		int i;
		char tag[2][8] = {"ccode=", "regrev="};

		/* Find ccode and regrev info */
		for (i = 0; i < 2; i++) {
			sp = strnstr(bus->vars, tag[i], bus->varsz);
			if (!sp) {
				DHD_ERROR(("%s: Could not find ccode info from the nvram %s\n",
					__FUNCTION__, bus->nv_path));
				bcmerror = BCME_ERROR;
				goto err;
			}
			sp = strchr(sp, '=');
			ep = strchr(sp, '\0');
			/* We assumed that string length of both ccode and
			 * regrev values should not exceed WLC_CNTRY_BUF_SZ
			 */
			if (ep && ((ep - sp) <= WLC_CNTRY_BUF_SZ)) {
				sp++;
				while (*sp != '\0') {
					DHD_INFO(("%s: parse '%s', current sp = '%c'\n",
						__FUNCTION__, tag[i], *sp));
					*sp++ = '0';
				}
			} else {
				DHD_ERROR(("%s: Invalid parameter format when parsing for %s\n",
					__FUNCTION__, tag[i]));
				bcmerror = BCME_ERROR;
				goto err;
			}
		}
	}
#endif /* DHD_USE_SINGLE_NVRAM_FILE */

#ifdef KEEP_JP_REGREV
	/* XXX Needed by customer's request */
#ifdef DHD_USE_SINGLE_NVRAM_FILE
	if (dhd_bus_get_fw_mode(bus->dhd) != DHD_FLAG_MFG_MODE)
#endif /* DHD_USE_SINGLE_NVRAM_FILE */
	{
		char *pos = NULL;
		tmpbuf = MALLOCZ(bus->dhd->osh, bus->varsz + 1);
		if (tmpbuf == NULL) {
			goto err;
		}
		memcpy(tmpbuf, bus->vars, bus->varsz);
		for (tmpidx = 0; tmpidx < bus->varsz; tmpidx++) {
			if (tmpbuf[tmpidx] == 0) {
				tmpbuf[tmpidx] = '\n';
			}
		}
		bus->dhd->vars_ccode[0] = 0;
		bus->dhd->vars_regrev = 0;
		if ((pos = strstr(tmpbuf, "ccode"))) {
			sscanf(pos, "ccode=%3s\n", bus->dhd->vars_ccode);
		}
		if ((pos = strstr(tmpbuf, "regrev"))) {
			sscanf(pos, "regrev=%u\n", &(bus->dhd->vars_regrev));
		}
		MFREE(bus->dhd->osh, tmpbuf, bus->varsz + 1);
	}
#endif /* KEEP_JP_REGREV */

err:
	return bcmerror;
}

/* loop through the capability list and see if the pcie capabilty exists */
uint8
dhdpcie_find_pci_capability(osl_t *osh, uint8 req_cap_id)
{
	uint8 cap_id;
	uint8 cap_ptr = 0;
	uint8 byte_val;

	/* check for Header type 0 */
	byte_val = read_pci_cfg_byte(PCI_CFG_HDR);
	if ((byte_val & 0x7f) != PCI_HEADER_NORMAL) {
		DHD_ERROR(("%s : PCI config header not normal.\n", __FUNCTION__));
		goto end;
	}

	/* check if the capability pointer field exists */
	byte_val = read_pci_cfg_byte(PCI_CFG_STAT);
	if (!(byte_val & PCI_CAPPTR_PRESENT)) {
		DHD_ERROR(("%s : PCI CAP pointer not present.\n", __FUNCTION__));
		goto end;
	}

	cap_ptr = read_pci_cfg_byte(PCI_CFG_CAPPTR);
	/* check if the capability pointer is 0x00 */
	if (cap_ptr == 0x00) {
		DHD_ERROR(("%s : PCI CAP pointer is 0x00.\n", __FUNCTION__));
		goto end;
	}

	/* loop thr'u the capability list and see if the pcie capabilty exists */

	cap_id = read_pci_cfg_byte(cap_ptr);

	while (cap_id != req_cap_id) {
		cap_ptr = read_pci_cfg_byte((cap_ptr + 1));
		if (cap_ptr == 0x00) break;
		cap_id = read_pci_cfg_byte(cap_ptr);
	}

end:
	return cap_ptr;
}

void
dhdpcie_pme_active(osl_t *osh, bool enable)
{
	uint8 cap_ptr;
	uint32 pme_csr;

	cap_ptr = dhdpcie_find_pci_capability(osh, PCI_CAP_POWERMGMTCAP_ID);

	if (!cap_ptr) {
		DHD_ERROR(("%s : Power Management Capability not present\n", __FUNCTION__));
		return;
	}

	pme_csr = OSL_PCI_READ_CONFIG(osh, cap_ptr + PME_CSR_OFFSET, sizeof(uint32));
	DHD_ERROR(("%s : pme_sts_ctrl 0x%x\n", __FUNCTION__, pme_csr));

	pme_csr |= PME_CSR_PME_STAT;
	if (enable) {
		pme_csr |= PME_CSR_PME_EN;
	} else {
		pme_csr &= ~PME_CSR_PME_EN;
	}

	OSL_PCI_WRITE_CONFIG(osh, cap_ptr + PME_CSR_OFFSET, sizeof(uint32), pme_csr);
}

bool
dhdpcie_pme_cap(osl_t *osh)
{
	uint8 cap_ptr;
	uint32 pme_cap;

	cap_ptr = dhdpcie_find_pci_capability(osh, PCI_CAP_POWERMGMTCAP_ID);

	if (!cap_ptr) {
		DHD_ERROR(("%s : Power Management Capability not present\n", __FUNCTION__));
		return FALSE;
	}

	pme_cap = OSL_PCI_READ_CONFIG(osh, cap_ptr, sizeof(uint32));

	DHD_ERROR(("%s : pme_cap 0x%x\n", __FUNCTION__, pme_cap));

	return ((pme_cap & PME_CAP_PM_STATES) != 0);
}

static void
dhdpcie_pme_stat_clear(dhd_bus_t *bus)
{
	uint32 pmcsr = dhd_pcie_config_read(bus, PCIE_CFG_PMCSR, sizeof(uint32));

	OSL_PCI_WRITE_CONFIG(bus->osh, PCIE_CFG_PMCSR, sizeof(uint32), pmcsr | PCIE_PMCSR_PMESTAT);
}

uint32
dhdpcie_lcreg(osl_t *osh, uint32 mask, uint32 val)
{

	uint8	pcie_cap;
	uint8	lcreg_offset;	/* PCIE capability LCreg offset in the config space */
	uint32	reg_val;

	pcie_cap = dhdpcie_find_pci_capability(osh, PCI_CAP_PCIECAP_ID);

	if (!pcie_cap) {
		DHD_ERROR(("%s : PCIe Capability not present\n", __FUNCTION__));
		return 0;
	}

	lcreg_offset = pcie_cap + PCIE_CAP_LINKCTRL_OFFSET;

	/* set operation */
	if (mask) {
		/* read */
		reg_val = OSL_PCI_READ_CONFIG(osh, lcreg_offset, sizeof(uint32));

		/* modify */
		reg_val &= ~mask;
		reg_val |= (mask & val);

		/* write */
		OSL_PCI_WRITE_CONFIG(osh, lcreg_offset, sizeof(uint32), reg_val);
	}
	return OSL_PCI_READ_CONFIG(osh, lcreg_offset, sizeof(uint32));
}

#if defined(NDIS)
/* set min res mask to highest value, preventing sleep */
void
dhdpcie_set_pmu_min_res_mask(struct dhd_bus *bus, uint min_res_mask)
{
	si_pmu_set_min_res_mask(bus->sih, bus->osh, min_res_mask);
}
#endif /* defined(NDIS) */

uint8
dhdpcie_clkreq(osl_t *osh, uint32 mask, uint32 val)
{
	uint8	pcie_cap;
	uint32	reg_val;
	uint8	lcreg_offset;	/* PCIE capability LCreg offset in the config space */

	pcie_cap = dhdpcie_find_pci_capability(osh, PCI_CAP_PCIECAP_ID);

	if (!pcie_cap) {
		DHD_ERROR(("%s : PCIe Capability not present\n", __FUNCTION__));
		return 0;
	}

	lcreg_offset = pcie_cap + PCIE_CAP_LINKCTRL_OFFSET;

	reg_val = OSL_PCI_READ_CONFIG(osh, lcreg_offset, sizeof(uint32));
	/* set operation */
	if (mask) {
		if (val)
			reg_val |= PCIE_CLKREQ_ENAB;
		else
			reg_val &= ~PCIE_CLKREQ_ENAB;
		OSL_PCI_WRITE_CONFIG(osh, lcreg_offset, sizeof(uint32), reg_val);
		reg_val = OSL_PCI_READ_CONFIG(osh, lcreg_offset, sizeof(uint32));
	}
	if (reg_val & PCIE_CLKREQ_ENAB)
		return 1;
	else
		return 0;
}

void dhd_dump_intr_counters(dhd_pub_t *dhd, struct bcmstrbuf *strbuf)
{
	dhd_bus_t *bus;
	uint64 current_time = OSL_LOCALTIME_NS();

	if (!dhd) {
		DHD_ERROR(("%s: dhd is NULL\n", __FUNCTION__));
		return;
	}

	bus = dhd->bus;
	if (!bus) {
		DHD_ERROR(("%s: bus is NULL\n", __FUNCTION__));
		return;
	}

	bcm_bprintf(strbuf, "\n ------- DUMPING INTR enable/disable counters-------\n");
	bcm_bprintf(strbuf, "host_irq_disable_count=%lu host_irq_enable_count=%lu\n"
		"dngl_intmask_disable_count=%lu dngl_intmask_enable_count=%lu\n"
		"dpc_return_busdown_count=%lu non_ours_irq_count=%lu\n",
		bus->host_irq_disable_count, bus->host_irq_enable_count,
		bus->dngl_intmask_disable_count, bus->dngl_intmask_enable_count,
		bus->dpc_return_busdown_count, bus->non_ours_irq_count);
#ifdef BCMPCIE_OOB_HOST_WAKE
	bcm_bprintf(strbuf, "oob_intr_count=%lu oob_intr_enable_count=%lu"
		" oob_intr_disable_count=%lu\noob_irq_num=%d"
		" last_oob_irq_times="SEC_USEC_FMT":"SEC_USEC_FMT
		" last_oob_irq_enable_time="SEC_USEC_FMT"\nlast_oob_irq_disable_time="SEC_USEC_FMT
		" oob_irq_enabled=%d oob_gpio_level=%d\n",
		bus->oob_intr_count, bus->oob_intr_enable_count,
		bus->oob_intr_disable_count, dhdpcie_get_oob_irq_num(bus),
		GET_SEC_USEC(bus->last_oob_irq_isr_time),
		GET_SEC_USEC(bus->last_oob_irq_thr_time),
		GET_SEC_USEC(bus->last_oob_irq_enable_time),
		GET_SEC_USEC(bus->last_oob_irq_disable_time), dhdpcie_get_oob_irq_status(bus),
		dhdpcie_get_oob_irq_level());
#endif /* BCMPCIE_OOB_HOST_WAKE */
	bcm_bprintf(strbuf, "\ncurrent_time="SEC_USEC_FMT" isr_entry_time="SEC_USEC_FMT
		" prev_isr_entry_time="SEC_USEC_FMT" isr_exit_time="SEC_USEC_FMT"\n"
		"isr_sched_dpc_time="SEC_USEC_FMT" rpm_sched_dpc_time="SEC_USEC_FMT"\n"
		" last_non_ours_irq_time="SEC_USEC_FMT" dpc_entry_time="SEC_USEC_FMT"\n"
		"last_process_ctrlbuf_time="SEC_USEC_FMT " last_process_flowring_time="SEC_USEC_FMT
		" last_process_txcpl_time="SEC_USEC_FMT"\nlast_process_rxcpl_time="SEC_USEC_FMT
		" last_process_infocpl_time="SEC_USEC_FMT" last_process_edl_time="SEC_USEC_FMT
		"\ndpc_exit_time="SEC_USEC_FMT" resched_dpc_time="SEC_USEC_FMT"\n"
		"last_d3_inform_time="SEC_USEC_FMT" dpc_sched=%u\n",
		GET_SEC_USEC(current_time), GET_SEC_USEC(bus->isr_entry_time),
		GET_SEC_USEC(bus->prev_isr_entry_time),
		GET_SEC_USEC(bus->isr_exit_time), GET_SEC_USEC(bus->isr_sched_dpc_time),
		GET_SEC_USEC(bus->rpm_sched_dpc_time),
		GET_SEC_USEC(bus->last_non_ours_irq_time), GET_SEC_USEC(bus->dpc_entry_time),
		GET_SEC_USEC(bus->last_process_ctrlbuf_time),
		GET_SEC_USEC(bus->last_process_flowring_time),
		GET_SEC_USEC(bus->last_process_txcpl_time),
		GET_SEC_USEC(bus->last_process_rxcpl_time),
		GET_SEC_USEC(bus->last_process_infocpl_time),
		GET_SEC_USEC(bus->last_process_edl_time),
		GET_SEC_USEC(bus->dpc_exit_time), GET_SEC_USEC(bus->resched_dpc_time),
		GET_SEC_USEC(bus->last_d3_inform_time), bus->dpc_sched);

	bcm_bprintf(strbuf, "\nlast_suspend_start_time="SEC_USEC_FMT" last_suspend_end_time="
		SEC_USEC_FMT" last_resume_start_time="SEC_USEC_FMT" last_resume_end_time="
		SEC_USEC_FMT"\n", GET_SEC_USEC(bus->last_suspend_start_time),
		GET_SEC_USEC(bus->last_suspend_end_time),
		GET_SEC_USEC(bus->last_resume_start_time),
		GET_SEC_USEC(bus->last_resume_end_time));

#if defined(SHOW_LOGTRACE) && defined(DHD_USE_KTHREAD_FOR_LOGTRACE)
	bcm_bprintf(strbuf, "logtrace_thread_entry_time="SEC_USEC_FMT
		" logtrace_thread_sem_down_time="SEC_USEC_FMT
		"\nlogtrace_thread_flush_time="SEC_USEC_FMT
		" logtrace_thread_unexpected_break_time="SEC_USEC_FMT
		"\nlogtrace_thread_complete_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(dhd->logtrace_thr_ts.entry_time),
		GET_SEC_USEC(dhd->logtrace_thr_ts.sem_down_time),
		GET_SEC_USEC(dhd->logtrace_thr_ts.flush_time),
		GET_SEC_USEC(dhd->logtrace_thr_ts.unexpected_break_time),
		GET_SEC_USEC(dhd->logtrace_thr_ts.complete_time));
#endif /* SHOW_LOGTRACE && DHD_USE_KTHREAD_FOR_LOGTRACE */
}

void dhd_dump_intr_registers(dhd_pub_t *dhd, struct bcmstrbuf *strbuf)
{
	uint32 intstatus = 0;
	uint32 intmask = 0;
	uint32 d2h_db0 = 0;
	uint32 d2h_mb_data = 0;

	intstatus = si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
		dhd->bus->pcie_mailbox_int, 0, 0);
#ifdef DHD_MMIO_TRACE
	dhd_bus_mmio_trace(dhd->bus, dhd->bus->pcie_mailbox_int, intstatus, FALSE);
#endif /* defined(DHD_MMIO_TRACE) */
	intmask = si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
		dhd->bus->pcie_mailbox_mask, 0, 0);
#ifdef DHD_MMIO_TRACE
	dhd_bus_mmio_trace(dhd->bus, dhd->bus->pcie_mailbox_mask, intmask, FALSE);
#endif /* defined(DHD_MMIO_TRACE) */
	d2h_db0 = si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, PCID2H_MailBox, 0, 0);
	dhd_bus_cmn_readshared(dhd->bus, &d2h_mb_data, D2H_MB_DATA, 0);

	bcm_bprintf(strbuf, "intstatus=0x%x intmask=0x%x d2h_db0=0x%x\n",
		intstatus, intmask, d2h_db0);
	bcm_bprintf(strbuf, "d2h_mb_data=0x%x def_intmask=0x%x\n",
		d2h_mb_data, dhd->bus->def_intmask);
}

void dhd_bus_dump_flowring(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf)
{
	uint16 flowid;
	int ix = 0;
	flow_ring_node_t *flow_ring_node;
	flow_info_t *flow_info;
#ifdef TX_STATUS_LATENCY_STATS
	uint8 ifindex;
	if_flow_lkup_t *if_flow_lkup;
	dhd_if_tx_status_latency_t if_tx_status_latency[DHD_MAX_IFS];
#endif /* TX_STATUS_LATENCY_STATS */
	unsigned long flags;

#ifdef TX_STATUS_LATENCY_STATS
	bzero(if_tx_status_latency, sizeof(if_tx_status_latency));
#endif /* TX_STATUS_LATENCY_STATS */

	bcm_bprintf(strbuf, "Flowring info:\n==============\n");
	bcm_bprintf(strbuf, "[RD=read ptr; WR=write ptr; T=TCM; H=Host; L=Local; D=DMA index'd]\n");
#ifndef BCM_ROUTER_DHD
	bcm_bprintf(strbuf,
		"%4s %4s %2s %4s %17s %4s %10s %17s %17s %17s %17s %14s %14s %10s ",
		"Num:", "Flow", "If", "Prio", ":Dest_MacAddress:", "Qlen",
		" Overflows", "TRD: HLRD: HDRD", "TWR: HLWR: HDWR", "BASE(VA)", "BASE(PA)",
		"WORK_ITEM_SIZE", "MAX_WORK_ITEMS", "TOTAL_SIZE");
#else
	bcm_bprintf(strbuf,
		"%4s %4s %2s %4s %17s %4s %4s %6s %10s %17s %17s %17s %17s %14s %14s %10s ",
		"Num:", "Flow", "If", "Prio", ":Dest_MacAddress:", "Qlen", "CLen", "L2CLen",
		" Overflows", "TRD: HLRD: HDRD", "TWR: HLWR: HDWR", "BASE(VA)", "BASE(PA)",
		"WORK_ITEM_SIZE", "MAX_WORK_ITEMS", "TOTAL_SIZE");
#endif /* !BCM_ROUTER_DHD */

#ifdef TX_STATUS_LATENCY_STATS
	/* Average Tx status/Completion Latency in micro secs */
	bcm_bprintf(strbuf, "%16s %16s ", "       NumTxPkts", "    AvgTxCmpL_Us");
#endif /* TX_STATUS_LATENCY_STATS */

	bcm_bprintf(strbuf, "\n");

	for (flowid = 0; flowid < dhdp->num_h2d_rings; flowid++) {
		flow_ring_node = DHD_FLOW_RING(dhdp, flowid);
		DHD_FLOWRING_LOCK(flow_ring_node->lock, flags);
		if (flow_ring_node->status != FLOW_RING_STATUS_OPEN) {
			DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);
			continue;
		}

		flow_info = &flow_ring_node->flow_info;
		bcm_bprintf(strbuf,
			"%4d %4d %2d %4d "MACDBG" %4d"
#ifdef BCM_ROUTER_DHD
			"%4d %6d"
#endif
			"%10u ", ix++,
			flow_ring_node->flowid, flow_info->ifindex, flow_info->tid,
			MAC2STRDBG(flow_info->da),
			DHD_FLOW_QUEUE_LEN(&flow_ring_node->queue),
#ifdef BCM_ROUTER_DHD
			DHD_CUMM_CTR_READ(DHD_FLOW_QUEUE_CLEN_PTR(&flow_ring_node->queue)),
			DHD_CUMM_CTR_READ(DHD_FLOW_QUEUE_L2CLEN_PTR(&flow_ring_node->queue)),
#endif
			DHD_FLOW_QUEUE_FAILURES(&flow_ring_node->queue));
		dhd_prot_print_flow_ring(dhdp, flow_ring_node->prot_info, TRUE, strbuf,
			"%5d:%5d:%5d %5d:%5d:%5d %17p %8x:%8x %14d %14d %10d");

#ifdef TX_STATUS_LATENCY_STATS
		bcm_bprintf(strbuf, "%16llu %16llu ",
			flow_info->num_tx_pkts,
			flow_info->num_tx_status ?
			DIV_U64_BY_U64(flow_info->cum_tx_status_latency,
			flow_info->num_tx_status) : 0);
		ifindex = flow_info->ifindex;
		ASSERT(ifindex < DHD_MAX_IFS);
		if (ifindex < DHD_MAX_IFS) {
			if_tx_status_latency[ifindex].num_tx_status += flow_info->num_tx_status;
			if_tx_status_latency[ifindex].cum_tx_status_latency +=
				flow_info->cum_tx_status_latency;
		} else {
			DHD_ERROR(("%s: Bad IF index: %d associated with flowid: %d\n",
				__FUNCTION__, ifindex, flowid));
		}
#endif /* TX_STATUS_LATENCY_STATS */
		bcm_bprintf(strbuf, "\n");
		DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);
	}

#ifdef TX_STATUS_LATENCY_STATS
	bcm_bprintf(strbuf, "\n%s  %16s  %16s\n", "If", "AvgTxCmpL_Us", "NumTxStatus");
	if_flow_lkup = (if_flow_lkup_t *)dhdp->if_flow_lkup;
	for (ix = 0; ix < DHD_MAX_IFS; ix++) {
		if (!if_flow_lkup[ix].status) {
			continue;
		}
		bcm_bprintf(strbuf, "%2d  %16llu  %16llu\n",
			ix,
			if_tx_status_latency[ix].num_tx_status ?
			DIV_U64_BY_U64(if_tx_status_latency[ix].cum_tx_status_latency,
			if_tx_status_latency[ix].num_tx_status): 0,
			if_tx_status_latency[ix].num_tx_status);
	}
#endif /* TX_STATUS_LATENCY_STATS */

	bcm_bprintf(strbuf, "\n");
}

void
dhd_bus_counters(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf)
{
	dhd_prot_counters(dhdp, strbuf, TRUE, TRUE);

	dhd_bus_dump_flowring(dhdp, strbuf);

	dhd_dump_dpc_histos(dhdp, strbuf);
}

/** Add bus dump output to a buffer */
void dhd_bus_dump(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf)
{
	uint16 flowid;
#ifdef BCMDBG
	int ix = 0;
	flow_ring_node_t *flow_ring_node;
	flow_info_t *flow_info;
	flow_info_t *local_flow_info;
#endif /* BCMDBG */

	BCM_REFERENCE(flowid);

#if defined(FW_SIGNATURE)
	/* Dump secure firmware status. */
	if (dhdp->busstate <= DHD_BUS_LOAD) {
		dhd_bus_dump_fws(dhdp->bus, strbuf);
	}
#endif

	if (dhdp->busstate != DHD_BUS_DATA)
		return;

#ifdef DHD_WAKE_STATUS
	bcm_bprintf(strbuf, "wake %u rxwake %u readctrlwake %u\n",
		bcmpcie_get_total_wake(dhdp->bus), dhdp->bus->wake_counts.rxwake,
		dhdp->bus->wake_counts.rcwake);
#ifdef DHD_WAKE_RX_STATUS
	bcm_bprintf(strbuf, " unicast %u muticast %u broadcast %u arp %u\n",
		dhdp->bus->wake_counts.rx_ucast, dhdp->bus->wake_counts.rx_mcast,
		dhdp->bus->wake_counts.rx_bcast, dhdp->bus->wake_counts.rx_arp);
	bcm_bprintf(strbuf, " multi4 %u multi6 %u icmp %u icmp6 %u multiother %u\n",
		dhdp->bus->wake_counts.rx_multi_ipv4, dhdp->bus->wake_counts.rx_multi_ipv6,
		dhdp->bus->wake_counts.rx_icmp, dhdp->bus->wake_counts.rx_icmpv6,
		dhdp->bus->wake_counts.rx_multi_other);
	bcm_bprintf(strbuf, " icmp6_ra %u, icmp6_na %u, icmp6_ns %u\n",
		dhdp->bus->wake_counts.rx_icmpv6_ra, dhdp->bus->wake_counts.rx_icmpv6_na,
		dhdp->bus->wake_counts.rx_icmpv6_ns);
#endif /* DHD_WAKE_RX_STATUS */
#ifdef DHD_WAKE_EVENT_STATUS
#ifdef CUSTOM_WAKE_REASON_STATS
	bcm_bprintf(strbuf, "rc_event_idx = %d, which indicates queue head\n",
		dhdp->bus->wake_counts.rc_event_idx);
	for (flowid = 0; flowid < MAX_WAKE_REASON_STATS; flowid++)
		if (dhdp->bus->wake_counts.rc_event[flowid] != -1)
#else
	for (flowid = 0; flowid < WLC_E_LAST; flowid++)
		if (dhdp->bus->wake_counts.rc_event[flowid] != 0)
#endif /* CUSTOM_WAKE_REASON_STATS */
			bcm_bprintf(strbuf, " %s = %u\n", bcmevent_get_name(flowid),
				dhdp->bus->wake_counts.rc_event[flowid]);
	bcm_bprintf(strbuf, "\n");
#endif /* DHD_WAKE_EVENT_STATUS */
#endif /* DHD_WAKE_STATUS */

	dhd_prot_print_info(dhdp, strbuf);
	dhd_dump_intr_registers(dhdp, strbuf);
	dhd_dump_intr_counters(dhdp, strbuf);
	bcm_bprintf(strbuf, "h2d_mb_data_ptr_addr 0x%x, d2h_mb_data_ptr_addr 0x%x\n",
		dhdp->bus->h2d_mb_data_ptr_addr, dhdp->bus->d2h_mb_data_ptr_addr);
	bcm_bprintf(strbuf, "dhd cumm_ctr %d\n", DHD_CUMM_CTR_READ(&dhdp->cumm_ctr));
#ifdef DHD_LIMIT_MULTI_CLIENT_FLOWRINGS
	bcm_bprintf(strbuf, "multi_client_flow_rings:%u max_multi_client_flow_rings:%d\n",
		OSL_ATOMIC_READ(dhdp->osh, &dhdp->multi_client_flow_rings),
		dhdp->max_multi_client_flow_rings);
#endif /* DHD_LIMIT_MULTI_CLIENT_FLOWRINGS */

	if (dhdp->htput_support) {
		bcm_bprintf(strbuf, "htput_flow_ring_start:%d total_htput:%d client_htput=%d\n",
			dhdp->htput_flow_ring_start, HTPUT_TOTAL_FLOW_RINGS,
			dhdp->htput_client_flow_rings);
	}

	dhd_bus_dump_flowring(dhdp, strbuf);

#ifdef BCMDBG
	if (!dhdp->d2h_sync_mode) {
		ix = 0;
		bcm_bprintf(strbuf, "\n%4s %4s %2s %10s %7s %6s %5s %5s %10s %7s %7s %7s \n",
			"Num:", "Flow", "If", "     ACKED", "D11SPRS", "WLSPRS", "TSDWL",
			"NOACK", "SPRS_ACKED", "EXPIRED", "DROPPED", "FWFREED");
		for (flowid = 0; flowid < dhdp->num_h2d_rings; flowid++) {
			flow_ring_node = DHD_FLOW_RING(dhdp, flowid);
			if (!flow_ring_node->active)
				continue;

			flow_info = &flow_ring_node->flow_info;
			bcm_bprintf(strbuf, "%4d %4d %2d ",
				ix++, flow_ring_node->flowid, flow_info->ifindex);
			local_flow_info = &flow_ring_node->flow_info;
			bcm_bprintf(strbuf, "%10d %7d %6d %5d %5d %10d %7d %7d %7d\n",
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_DISCARD],
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_D11SUPPRESS],
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_WLSUPPRESS],
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_TOSSED_BYWLC],
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_DISCARD_NOACK],
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_SUPPRESS_ACKED],
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_EXPIRED],
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_DROPPED],
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_MKTFREE]);
		}
	}
#endif /* BCMDBG */

	dhd_dump_dpc_histos(dhdp, strbuf);

	bcm_bprintf(strbuf, "D3 inform cnt %d\n", dhdp->bus->d3_inform_cnt);
	bcm_bprintf(strbuf, "D0 inform cnt %d\n", dhdp->bus->d0_inform_cnt);
	bcm_bprintf(strbuf, "D0 inform in use cnt %d\n", dhdp->bus->d0_inform_in_use_cnt);
	if (dhdp->d2h_hostrdy_supported) {
		bcm_bprintf(strbuf, "hostready count:%d\n", dhdp->bus->hostready_count);
	}
#ifdef PCIE_INB_DW
	/* Inband device wake counters */
	if (INBAND_DW_ENAB(dhdp->bus)) {
		bcm_bprintf(strbuf, "Inband device_wake assert count: %d\n",
			dhdp->bus->inband_dw_assert_cnt);
		bcm_bprintf(strbuf, "Inband device_wake deassert count: %d\n",
			dhdp->bus->inband_dw_deassert_cnt);
		bcm_bprintf(strbuf, "Inband DS-EXIT <host initiated> count: %d\n",
			dhdp->bus->inband_ds_exit_host_cnt);
		bcm_bprintf(strbuf, "Inband DS-EXIT <device initiated> count: %d\n",
			dhdp->bus->inband_ds_exit_device_cnt);
		bcm_bprintf(strbuf, "Inband DS-EXIT Timeout count: %d\n",
			dhdp->bus->inband_ds_exit_to_cnt);
		bcm_bprintf(strbuf, "Inband HOST_SLEEP-EXIT Timeout count: %d\n",
			dhdp->bus->inband_host_sleep_exit_to_cnt);
	}
#endif /* PCIE_INB_DW */
	bcm_bprintf(strbuf, "d2h_intr_method -> %s d2h_intr_control -> %s\n",
		dhdp->bus->d2h_intr_method ? "PCIE_MSI" : "PCIE_INTX",
		dhdp->bus->d2h_intr_control ? "HOST_IRQ" : "D2H_INTMASK");

	bcm_bprintf(strbuf, "\n\nDB7 stats - db7_send_cnt: %d, db7_trap_cnt: %d, "
		"max duration: %lld (%lld - %lld), db7_timing_error_cnt: %d\n",
		dhdp->db7_trap.debug_db7_send_cnt,
		dhdp->db7_trap.debug_db7_trap_cnt,
		dhdp->db7_trap.debug_max_db7_dur,
		dhdp->db7_trap.debug_max_db7_trap_time,
		dhdp->db7_trap.debug_max_db7_send_time,
		dhdp->db7_trap.debug_db7_timing_error_cnt);
}

int
dhd_dump_flowrings(dhd_pub_t *dhdp, char *buf, int buflen)
{
	struct bcmstrbuf b;
	struct bcmstrbuf *strbuf = &b;

	if (!dhdp || !dhdp->prot || !buf) {
		return BCME_ERROR;
	}

	bcm_binit(strbuf, buf, buflen);
	dhd_bus_dump_flowring(dhdp, strbuf);
	return (!strbuf->size ? BCME_BUFTOOSHORT : strbuf->size);
}

static void
dhd_bus_dump_txcpl_info(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf)
{
	tx_cpl_info_t *txcpl_info = &dhdp->txcpl_info;
	int i;

	bcm_bprintf(strbuf, "\nTx Completion History\n");
	bcm_bprintf(strbuf, "Host(us)\t\tPTM_high(ns)\t\tPTM_low(ns)\t\t"
		"Latency(ms)\t\tTID\t\tFlowID\n");
	for (i = 0; i < MAX_TXCPL_HISTORY; i ++) {
		bcm_bprintf(strbuf, "0x%x\t\t0x%x\t\t0x%x\t\t%u\t\t%u\t\t%u\n",
			txcpl_info->tx_history[i].host_time,
			txcpl_info->tx_history[i].ptm_high,
			txcpl_info->tx_history[i].ptm_low,
			txcpl_info->tx_history[i].latency,
			txcpl_info->tx_history[i].tid,
			txcpl_info->tx_history[i].flowid);
	}
	bzero(txcpl_info->tx_history,
		sizeof(tx_cpl_history_t) * MAX_TXCPL_HISTORY);

	bcm_bprintf(strbuf, "\n");
}

static void
dhd_bus_dump_mdring_info(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf)
{
	int i;
	int count;

	bcm_bprintf(strbuf, "\nMetadata Ring Dump\n");
	for (i = 0, count = 0;
		i < MAX_MDRING_ITEM_DUMP * D2HRING_MDCMPLT_ITEMSIZE; i += 4) {
		if ((i % D2HRING_MDCMPLT_ITEMSIZE) == 0) {
			bcm_bprintf(strbuf, "\nEntry:%d:", ++count);
		}
		bcm_bprintf(strbuf, "0x%x%x%x%x:",
			dhdp->mdring_info[i], dhdp->mdring_info[i+1],
			dhdp->mdring_info[i+2], dhdp->mdring_info[i+3]);
	}
	bcm_bprintf(strbuf, "\n");
}

static bool
dhd_bus_support_dar_sec_status(dhd_bus_t *bus)
{
	DHD_ERROR(("%s: buscorerev=%d chipid=0x%x\n",
		__FUNCTION__, bus->sih->buscorerev, si_chipid(bus->sih)));

	/* Support for DAR security status register provided for pcie core
	 * generation2 at revisions > 73 and for generation3 at revisions > 128
	 */
	if ((bus->sih->buscorerev < 74) || (bus->sih->buscorerev == 128)) {
		return FALSE;
	}

	return TRUE;
}

static int
dhd_bus_get_security_status(dhd_bus_t *bus, uint32 *status)
{
	uint32 dar_sec_reg;

	if (!dhd_bus_support_dar_sec_status(bus)) {
		return BCME_UNSUPPORTED;
	}
	/* DAR Security Status Register address */
	dar_sec_reg = (uint32)DAR_SEC_STATUS(bus->sih->buscorerev);

	/* read DAR Security Status Register */
	*status = si_corereg(bus->sih, bus->sih->buscoreidx, dar_sec_reg, 0, 0);

	return BCME_OK;
}

static int
dhd_bus_security_info(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf)
{
	int ret = BCME_OK;
	dhd_bus_t *bus = dhdp->bus;
	uint32 dar_sec_val;

	ret = dhd_bus_get_security_status(bus, &dar_sec_val);
	if (ret != BCME_OK) {
		return ret;
	}

	bcm_bprintf(strbuf, "\nDAR Security Status Reg\n");
	bcm_bprintf(strbuf, "  Jtag Disable:     %d\n",
		(dar_sec_val & DAR_SEC_JTAG_MASK) >> DAR_SEC_JTAG_SHIFT);
	bcm_bprintf(strbuf, "  Secure Boot:      %d\n",
		(dar_sec_val & DAR_SEC_SBOOT_MASK) >> DAR_SEC_SBOOT_SHIFT);
	bcm_bprintf(strbuf, "  Arm Dbg Disable:  %d\n",
		(dar_sec_val & DAR_SEC_ARM_DBG_MASK) >> DAR_SEC_ARM_DBG_SHIFT);
	bcm_bprintf(strbuf, "  Transient Unlock: %d\n",
		(dar_sec_val & DAR_SEC_UNLOCK_MASK) >> DAR_SEC_UNLOCK_SHIFT);

	/* The following bits in security status register provided for pcie core
	 * generation2 at revisions > 76 and for generation3 at revisions > 129
	 */
	if (((bus->sih->buscorerev > 76) && (bus->sih->buscorerev < 128)) ||
		(bus->sih->buscorerev > 129)) {
		bcm_bprintf(strbuf, "  Rom Protect:      %d\n",
			(dar_sec_val & DAR_SEC_ROM_PROT_MASK) >> DAR_SEC_ROM_PROT_SHIFT);
		bcm_bprintf(strbuf, "  Non Secure Write: %d\n",
			(dar_sec_val & DAR_SEC_NSEC_WR_MASK) >> DAR_SEC_NSEC_WR_SHIFT);
		bcm_bprintf(strbuf, "  Non Secure Read:  %d\n",
			(dar_sec_val & DAR_SEC_NSEC_RD_MASK) >> DAR_SEC_NSEC_RD_SHIFT);
	}
	return BCME_OK;
}

static int
dhd_bus_sboot_disable(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf)
{
	int ret = BCME_OK;
	dhd_bus_t *bus = dhdp->bus;
	uint32 save_idx, val;
	sysmemregs_t *regs;
	uint32 dar_sec_val;

	save_idx = si_coreidx(bus->sih);
	ret = dhd_bus_get_security_status(bus, &dar_sec_val);
	if (ret != BCME_OK) {
		goto exit;
	}

	/* check if chip is Transient Unlocked */
	if (!(dar_sec_val & DAR_SEC_UNLOCK_MASK)) {
		bcm_bprintf(strbuf, "\nFail: Secure Boot still enabled!\n");
		goto exit;
	}

	if ((regs = si_setcore(bus->sih, SYSMEM_CORE_ID, 0)) != NULL) {
		/* check if mpu control is unlocked */
		val = R_REG(dhdp->osh, MPU_REG(regs, mpucontrol));
		if (val & BUS_MPU_LOCK_MASK) {
			bcm_bprintf(strbuf, "\nFail: Mpu Control is locked!\n");
			goto exit;
		}

		/* clear RomProtect and VectorProtect */
		W_REG(dhdp->osh, MPU_REG(regs, mpucontrol),
			val & ~(BUS_MPU_ROM_PR_MASK | BUS_MPU_VEC_PR_MASK));

		/* validate RomProtect and VectorProtect are cleared */
		val = R_REG(dhdp->osh, MPU_REG(regs, mpucontrol));
		if (val & (BUS_MPU_ROM_PR_MASK | BUS_MPU_VEC_PR_MASK)) {
			bcm_bprintf(strbuf, "\nFail: Secure Boot still enabled!\n");
		} else {
			bcm_bprintf(strbuf, "\nSecure Boot disabled!\n");
		}

	}
exit:
	si_setcoreidx(bus->sih, save_idx);
	return ret;
}

static void
dhd_bus_dump_rxlat_info(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf)
{
	if (dhdp->rx_cpl_lat_capable) {
		rx_cpl_lat_info_t *rxcpl_info = &dhdp->rxcpl_lat_info;
		int i;

		bcm_bprintf(strbuf, "\nRx Completion History\n");
		bcm_bprintf(strbuf, "Host(us)\t\tPTM_high(ns)\t\tPTM_low(ns)\t\tRx_t0\t\tBand"
			"\t\tPrio\t\tRSSI\t\tLatency(us)\n");
		for (i = 0; i < MAX_RXCPL_HISTORY; i ++) {
			if (rxcpl_info->rx_history[i].rx_t0 || rxcpl_info->rx_history[i].ptm_low) {
				bcm_bprintf(strbuf, "0x%x\t\t0x%x\t\t0x%x\t\t0x%x\t\t%s\t\t%d\t\t%d"
					"\t\t%d\n",
					rxcpl_info->rx_history[i].host_time,
					rxcpl_info->rx_history[i].ptm_high,
					rxcpl_info->rx_history[i].ptm_low,
					rxcpl_info->rx_history[i].rx_t0,
					rxcpl_info->rx_history[i].slice? "5G":"2G",
					rxcpl_info->rx_history[i].priority,
					rxcpl_info->rx_history[i].rssi,
					rxcpl_info->rx_history[i].latency);
			}
		}
		bzero(rxcpl_info->rx_history,
			sizeof(rx_cpl_history_t) * MAX_RXCPL_HISTORY);
	}
	bcm_bprintf(strbuf, "\n");
}

static void
dhd_bus_dump_rxlat_histo(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf)
{
	int bin, prio;

	if (dhdp->rx_cpl_lat_capable) {
		rx_cpl_lat_info_t *rxcpl_info = &dhdp->rxcpl_lat_info;

		bcm_bprintf(strbuf,
			"\nRx:5G Latency histogram, each bin:%d us\n", RX_LAT_BIN_SCALE);
		bcm_bprintf(strbuf,
			"Bin\tPrio-0\tPrio-1\tPrio-2\tPrio-3\tPrio-4\tPrio-5\tPrio-6\tPrio-7\n");
		for (bin = 0; bin < MAX_RX_LAT_HIST_BIN; bin++) {
			bcm_bprintf(strbuf, "\n%d", bin);
			for (prio = 0; prio < MAX_RX_LAT_PRIO; prio++) {
				bcm_bprintf(strbuf, "\t");
				if (rxcpl_info->rx_dur_5g[prio][bin]) {
					bcm_bprintf(strbuf, "%d",
						rxcpl_info->rx_dur_5g[prio][bin]);
				}
			}
		}
		bcm_bprintf(strbuf,
			"\nRx:2G Latency histogram, each bin:%d us\n", RX_LAT_BIN_SCALE);
		bcm_bprintf(strbuf,
			"Bin\tPrio-0\tPrio-1\tPrio-2\tPrio-3\tPrio-4\tPrio-5\tPrio-6\tPrio-7\n");
		for (bin = 0; bin < MAX_RX_LAT_HIST_BIN; bin++) {
			bcm_bprintf(strbuf, "\n%d", bin);
			for (prio = 0; prio < MAX_RX_LAT_PRIO; prio++) {
				bcm_bprintf(strbuf, "\t");
				if (rxcpl_info->rx_dur_2g[prio][bin]) {
					bcm_bprintf(strbuf, "%d",
						rxcpl_info->rx_dur_2g[prio][bin]);
				}
			}
		}
		bzero(rxcpl_info->rx_dur_5g, MAX_RX_LAT_PRIO * MAX_RX_LAT_HIST_BIN);
		bzero(rxcpl_info->rx_dur_2g, MAX_RX_LAT_PRIO * MAX_RX_LAT_HIST_BIN);
	}
	bcm_bprintf(strbuf, "\n");
}

#ifdef DNGL_AXI_ERROR_LOGGING
bool
dhd_axi_sig_match(dhd_pub_t *dhdp)
{
	uint32 axi_tcm_addr = dhdpcie_bus_rtcm32(dhdp->bus, dhdp->axierror_logbuf_addr);

	if (dhdp->dhd_induce_error == DHD_INDUCE_DROP_AXI_SIG) {
		DHD_ERROR(("%s: Induce AXI signature drop\n", __FUNCTION__));
		return FALSE;
	}

	DHD_ERROR(("%s: axi_tcm_addr: 0x%x, tcm range: 0x%x ~ 0x%x\n",
		__FUNCTION__, axi_tcm_addr, dhdp->bus->dongle_ram_base,
		dhdp->bus->dongle_ram_base + dhdp->bus->ramsize));
	if (axi_tcm_addr >= dhdp->bus->dongle_ram_base &&
	    axi_tcm_addr < dhdp->bus->dongle_ram_base + dhdp->bus->ramsize) {
		uint32 axi_signature = dhdpcie_bus_rtcm32(dhdp->bus, (axi_tcm_addr +
			OFFSETOF(hnd_ext_trap_axi_error_v1_t, signature)));
		if (axi_signature == HND_EXT_TRAP_AXIERROR_SIGNATURE) {
			return TRUE;
		} else {
			DHD_ERROR(("%s: No AXI signature: 0x%x\n",
				__FUNCTION__, axi_signature));
			return FALSE;
		}
	} else {
		DHD_ERROR(("%s: No AXI shared tcm address debug info.\n", __FUNCTION__));
		return FALSE;
	}
}

void
dhd_axi_error(dhd_pub_t *dhdp)
{
	dhd_axi_error_dump_t *axi_err_dump;
	uint8 *axi_err_buf = NULL;
	uint8 *p_axi_err = NULL;
	uint32 axi_logbuf_addr;
	uint32 axi_tcm_addr;
	int err, size;

	/* XXX: On the Dongle side, if an invalid Host Address is generated for a transaction
	 * it results in SMMU Fault. Now the Host won't respond for the invalid transaction.
	 * On the Dongle side, after 50msec this results in AXI Slave Error.
	 * Hence introduce a delay higher than 50msec to ensure AXI Slave error happens and
	 * the Dongle collects the required information.
	 */
	OSL_DELAY(75000);

	axi_logbuf_addr = dhdp->axierror_logbuf_addr;
	if (!axi_logbuf_addr) {
		DHD_ERROR(("%s: No AXI TCM address debug info.\n", __FUNCTION__));
		goto sched_axi;
	}

	axi_err_dump = dhdp->axi_err_dump;
	if (!axi_err_dump) {
		goto sched_axi;
	}

	if (!dhd_axi_sig_match(dhdp)) {
		goto sched_axi;
	}

	/* Reading AXI error data for SMMU fault */
	DHD_ERROR(("%s: Read AXI data from TCM address\n", __FUNCTION__));
	axi_tcm_addr = dhdpcie_bus_rtcm32(dhdp->bus, axi_logbuf_addr);
	size = sizeof(hnd_ext_trap_axi_error_v1_t);
	axi_err_buf = MALLOCZ(dhdp->osh, size);
	if (axi_err_buf == NULL) {
		DHD_ERROR(("%s: out of memory !\n", __FUNCTION__));
		goto sched_axi;
	}

	p_axi_err = axi_err_buf;
	err = dhdpcie_bus_membytes(dhdp->bus, FALSE, axi_tcm_addr, p_axi_err, size);
	if (err) {
		DHD_ERROR(("%s: error %d on reading %d membytes at 0x%08x\n",
			__FUNCTION__, err, size, axi_tcm_addr));
		goto sched_axi;
	}

	/* Dump data to Dmesg */
	dhd_log_dump_axi_error(axi_err_buf);
	err = memcpy_s(&axi_err_dump->etd_axi_error_v1, size, axi_err_buf, size);
	if (err) {
		DHD_ERROR(("%s: failed to copy etd axi error info, err=%d\n",
			__FUNCTION__, err));
	}

sched_axi:
	if (axi_err_buf) {
		MFREE(dhdp->osh, axi_err_buf, size);
	}
	dhd_schedule_axi_error_dump(dhdp, NULL);
}

static void
dhd_log_dump_axi_error(uint8 *axi_err)
{
	dma_dentry_v1_t dma_dentry;
	dma_fifo_v1_t dma_fifo;
	int i = 0, j = 0;

	if (*(uint8 *)axi_err == HND_EXT_TRAP_AXIERROR_VERSION_1) {
		hnd_ext_trap_axi_error_v1_t *axi_err_v1 = (hnd_ext_trap_axi_error_v1_t *)axi_err;
		DHD_ERROR(("%s: signature : 0x%x\n", __FUNCTION__, axi_err_v1->signature));
		DHD_ERROR(("%s: version : 0x%x\n", __FUNCTION__, axi_err_v1->version));
		DHD_ERROR(("%s: length : 0x%x\n", __FUNCTION__, axi_err_v1->length));
		DHD_ERROR(("%s: dma_fifo_valid_count : 0x%x\n",
			__FUNCTION__, axi_err_v1->dma_fifo_valid_count));
		DHD_ERROR(("%s: axi_errorlog_status : 0x%x\n",
			__FUNCTION__, axi_err_v1->axi_errorlog_status));
		DHD_ERROR(("%s: axi_errorlog_core : 0x%x\n",
			__FUNCTION__, axi_err_v1->axi_errorlog_core));
		DHD_ERROR(("%s: axi_errorlog_hi : 0x%x\n",
			__FUNCTION__, axi_err_v1->axi_errorlog_hi));
		DHD_ERROR(("%s: axi_errorlog_lo : 0x%x\n",
			__FUNCTION__, axi_err_v1->axi_errorlog_lo));
		DHD_ERROR(("%s: axi_errorlog_id : 0x%x\n",
			__FUNCTION__, axi_err_v1->axi_errorlog_id));

		for (i = 0; i < MAX_DMAFIFO_ENTRIES_V1; i++) {
			dma_fifo = axi_err_v1->dma_fifo[i];
			DHD_ERROR(("%s: valid:%d : 0x%x\n",
				__FUNCTION__, i, dma_fifo.valid));
			DHD_ERROR(("%s: direction:%d : 0x%x\n",
				__FUNCTION__, i, dma_fifo.direction));
			DHD_ERROR(("%s: index:%d : 0x%x\n",
				__FUNCTION__, i, dma_fifo.index));
			DHD_ERROR(("%s: dpa:%d : 0x%x\n",
				__FUNCTION__, i, dma_fifo.dpa));
			DHD_ERROR(("%s: desc_lo:%d : 0x%x\n",
				__FUNCTION__, i, dma_fifo.desc_lo));
			DHD_ERROR(("%s: desc_hi:%d : 0x%x\n",
				__FUNCTION__, i, dma_fifo.desc_hi));
			DHD_ERROR(("%s: din:%d : 0x%x\n",
				__FUNCTION__, i, dma_fifo.din));
			DHD_ERROR(("%s: dout:%d : 0x%x\n",
				__FUNCTION__, i, dma_fifo.dout));
			for (j = 0; j < MAX_DMAFIFO_DESC_ENTRIES_V1; j++) {
				dma_dentry = axi_err_v1->dma_fifo[i].dentry[j];
				DHD_ERROR(("%s: ctrl1:%d : 0x%x\n",
					__FUNCTION__, i, dma_dentry.ctrl1));
				DHD_ERROR(("%s: ctrl2:%d : 0x%x\n",
					__FUNCTION__, i, dma_dentry.ctrl2));
				DHD_ERROR(("%s: addrlo:%d : 0x%x\n",
					__FUNCTION__, i, dma_dentry.addrlo));
				DHD_ERROR(("%s: addrhi:%d : 0x%x\n",
					__FUNCTION__, i, dma_dentry.addrhi));
			}
		}
	}
	else {
		DHD_ERROR(("%s: Invalid AXI version: 0x%x\n", __FUNCTION__, (*(uint8 *)axi_err)));
	}
}
#endif /* DNGL_AXI_ERROR_LOGGING */

/**
 * Brings transmit packets on all flow rings closer to the dongle, by moving (a subset) from their
 * flow queue to their flow ring.
 */
static bool
dhd_update_txflowrings(dhd_pub_t *dhd)
{
	unsigned long flags;
	dll_t *item, *next;
	flow_ring_node_t *flow_ring_node;
	struct dhd_bus *bus = dhd->bus;
	int count = 0;
	bool more = FALSE;

	if (dhd_query_bus_erros(dhd)) {
		return more;
	}

	/* Hold flowring_list_lock to ensure no race condition while accessing the List */
	DHD_FLOWRING_LIST_LOCK(bus->dhd->flowring_list_lock, flags);
	for (item = dll_head_p(&bus->flowring_active_list);
		(!dhd_is_device_removed(dhd) && !dll_end(&bus->flowring_active_list, item));
		item = next, count++) {
		if (dhd->hang_was_sent) {
			more = FALSE;
			break;
		}

		if (count > bus->max_tx_flowrings) {
			DHD_ERROR(("%s : overflow max flowrings\n", __FUNCTION__));
#ifdef OEM_ANDROID
			dhd->hang_reason = HANG_REASON_UNKNOWN;
			dhd_os_send_hang_message(dhd);
#endif /* OEM_ANDROID */
			more = FALSE;
			break;
		}

		next = dll_next_p(item);
		flow_ring_node = dhd_constlist_to_flowring(item);

		/* Ensure that flow_ring_node in the list is Not Null */
		ASSERT(flow_ring_node != NULL);

		/* Ensure that the flowring node has valid contents */
		ASSERT(flow_ring_node->prot_info != NULL);

		more = dhd_prot_update_txflowring(dhd, flow_ring_node->flowid,
			flow_ring_node->prot_info);
	}
	DHD_FLOWRING_LIST_UNLOCK(bus->dhd->flowring_list_lock, flags);

	return more;
}

/** Mailbox ringbell Function */
static void
dhd_bus_gen_devmb_intr(struct dhd_bus *bus)
{
	if ((bus->sih->buscorerev == 2) || (bus->sih->buscorerev == 6) ||
		(bus->sih->buscorerev == 4)) {
		DHD_ERROR(("%s: mailbox communication not supported\n", __FUNCTION__));
		return;
	}
	if (bus->db1_for_mb)  {
		/* this is a pcie core register, not the config register */
		/* XXX: make sure we are on PCIE */
		DHD_INFO(("%s: writing a mail box interrupt to the device, through doorbell 1\n", __FUNCTION__));
		if (DAR_PWRREQ(bus)) {
			dhd_bus_pcie_pwr_req(bus);
		}
#ifdef DHD_MMIO_TRACE
		dhd_bus_mmio_trace(bus, dhd_bus_db1_addr_get(bus), 0x2, TRUE);
#endif /* defined(DHD_MMIO_TRACE) */
		si_corereg(bus->sih, bus->sih->buscoreidx, dhd_bus_db1_addr_get(bus),
			~0, 0x12345678);
	} else {
		DHD_INFO(("%s: writing a mail box interrupt to the device, through config space\n", __FUNCTION__));
		dhdpcie_bus_cfg_write_dword(bus, PCISBMbx, 4, (1 << 0));
		/* XXX CRWLPCIEGEN2-182 requires double write */
		dhdpcie_bus_cfg_write_dword(bus, PCISBMbx, 4, (1 << 0));
	}
}

/* Upon receiving a mailbox interrupt,
 * if H2D_FW_TRAP bit is set in mailbox location
 * device traps
 */
static void
dhdpcie_fw_trap(dhd_bus_t *bus)
{
	if (bus->dhd->db7_trap.fw_db7w_trap) {
		uint32 addr = dhd_bus_db1_addr_3_get(bus);

		DHD_ERROR(("%s: send DB7 trap!!!\n", __FUNCTION__));
		bus->dhd->db7_trap.fw_db7w_trap_recieved = FALSE;
		bus->dhd->db7_trap.debug_db7_send_time = OSL_LOCALTIME_NS();
		bus->dhd->db7_trap.debug_db7_send_cnt++;
		si_corereg(bus->sih, bus->sih->buscoreidx, addr, ~0,
			bus->dhd->db7_trap.db7_magic_number);
		return;
	}

	DHD_ERROR(("%s: send trap!!!\n", __FUNCTION__));
	/* Send the mailbox data and generate mailbox intr. */
	dhdpcie_send_mb_data(bus, H2D_FW_TRAP);
	/* For FWs that cannot interprete H2D_FW_TRAP */
	(void)dhd_wl_ioctl_set_intiovar(bus->dhd, "bus:disconnect", 99, WLC_SET_VAR, TRUE, 0);
}

void
dhdpcie_db7_trap(struct dhd_bus *bus)
{
	if (bus->dhd->busstate != DHD_BUS_DATA) {
		DHD_ERROR(("%s: Bus is NOT up. busstate: %d\n", __FUNCTION__, bus->dhd->busstate));
		return;
	}
	if (bus->dhd->db7_trap.fw_db7w_trap) {
		unsigned long flags = 0;

		DHD_GENERAL_LOCK(bus->dhd, flags);
		bus->dhd->db7_trap.fw_db7w_trap_inprogress = TRUE;
		DHD_GENERAL_UNLOCK(bus->dhd, flags);
		dhdpcie_fw_trap(bus);
		OSL_DELAY(100 * 1000); // wait 100 msec
		if (!bus->dhd->db7_trap.fw_db7w_trap_recieved) {
			DHD_ERROR(("%s : Did not receive DB7 Ack\n", __FUNCTION__));
		}
		DHD_GENERAL_LOCK(bus->dhd, flags);
		bus->dhd->db7_trap.fw_db7w_trap_inprogress = FALSE;
		DHD_GENERAL_UNLOCK(bus->dhd, flags);
	} else {
		DHD_TRACE(("%s: DB7 Not supported!!!\n", __FUNCTION__));
	}

}

#ifdef PCIE_INB_DW

void
dhd_bus_inb_ack_pending_ds_req(dhd_bus_t *bus)
{
	/* The DHD_BUS_INB_DW_LOCK must be held before
	* calling this function !!
	*/
	if ((dhdpcie_bus_get_pcie_inband_dw_state(bus) ==
		DW_DEVICE_DS_DEV_SLEEP_PEND) &&
		(bus->host_active_cnt == 0) && (!bus->skip_ds_ack)) {
		dhdpcie_bus_set_pcie_inband_dw_state(bus, DW_DEVICE_DS_DEV_SLEEP);
		dhdpcie_send_mb_data(bus, H2D_HOST_DS_ACK);
	}
}

int
dhd_bus_inb_set_device_wake(struct dhd_bus *bus, bool val)
{
	int timeleft;
	unsigned long flags;
	int ret;

	if (!INBAND_DW_ENAB(bus)) {
		return BCME_ERROR;
	}
	if (val) {
		DHD_BUS_INB_DW_LOCK(bus->inb_lock, flags);

		/*
		 * Reset the Door Bell Timeout value. So that the Watchdog
		 * doesn't try to Deassert Device Wake, while we are in
		 * the process of still Asserting the same.
		 */
		dhd_bus_doorbell_timeout_reset(bus);

		if (dhdpcie_bus_get_pcie_inband_dw_state(bus) ==
			DW_DEVICE_DS_DEV_SLEEP) {
			/* Clear wait_for_ds_exit */
			bus->wait_for_ds_exit = 0;
			if (bus->calc_ds_exit_latency) {
				bus->ds_exit_latency = 0;
				bus->ds_exit_ts2 = 0;
				bus->ds_exit_ts1 = OSL_SYSUPTIME_US();
			}
			ret = dhdpcie_send_mb_data(bus, H2DMB_DS_DEVICE_WAKE_ASSERT);
			if (ret != BCME_OK) {
				DHD_ERROR(("Failed: assert Inband device_wake\n"));
				DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
				ret = BCME_ERROR;
				goto exit;
			}
			dhdpcie_bus_set_pcie_inband_dw_state(bus,
				DW_DEVICE_DS_DISABLED_WAIT);
			bus->inband_dw_assert_cnt++;
		} else if (dhdpcie_bus_get_pcie_inband_dw_state(bus) ==
			DW_DEVICE_DS_DISABLED_WAIT) {
			DHD_ERROR(("Inband device wake is already asserted, "
				"waiting for DS-Exit\n"));
		}
		else {
			DHD_INFO(("Not in DS SLEEP state \n"));
			DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
			ret = BCME_OK;
			goto exit;
		}

		/*
		 * Since we are going to wait/sleep .. release the lock.
		 * The Device Wake sanity is still valid, because
		 * a) If there is another context that comes in and tries
		 *    to assert DS again and if it gets the lock, since
		 *    ds_state would be now != DW_DEVICE_DS_DEV_SLEEP the
		 *    context would return saying Not in DS Sleep.
		 * b) If ther is another context that comes in and tries
		 *    to de-assert DS and gets the lock,
		 *    since the ds_state is != DW_DEVICE_DS_DEV_WAKE
		 *    that context would return too. This can not happen
		 *    since the watchdog is the only context that can
		 *    De-Assert Device Wake and as the first step of
		 *    Asserting the Device Wake, we have pushed out the
		 *    Door Bell Timeout.
		 *
		 */

		if (!CAN_SLEEP()) {
			dhdpcie_bus_set_pcie_inband_dw_state(bus,
				DW_DEVICE_DS_DEV_WAKE);
			DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
			/* Called from context that cannot sleep */
			OSL_DELAY(1000);
		} else {
			DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
			/* Wait for DS EXIT for DS_EXIT_TIMEOUT seconds */
			timeleft = dhd_os_ds_exit_wait(bus->dhd, &bus->wait_for_ds_exit);
			if (!bus->wait_for_ds_exit || timeleft == 0) {
				DHD_ERROR(("dhd_bus_inb_set_device_wake:DS-EXIT timeout, "
					"wait_for_ds_exit : %d\n", bus->wait_for_ds_exit));
				bus->inband_ds_exit_to_cnt++;
				bus->ds_exit_timeout = 0;
#ifdef DHD_FW_COREDUMP
				if (bus->dhd->memdump_enabled) {
					/* collect core dump */
					DHD_GENERAL_LOCK(bus->dhd, flags);
					DHD_BUS_BUSY_CLEAR_RPM_SUSPEND_IN_PROGRESS(bus->dhd);
					DHD_GENERAL_UNLOCK(bus->dhd, flags);
					bus->dhd->memdump_type =
						DUMP_TYPE_INBAND_DEVICE_WAKE_FAILURE;
					dhd_bus_mem_dump(bus->dhd);
				}
#else
				ASSERT(0);
#endif /* DHD_FW_COREDUMP */
				ret = BCME_ERROR;
				goto exit;
			}
		}
		ret = BCME_OK;
	} else {
		DHD_BUS_INB_DW_LOCK(bus->inb_lock, flags);
		if ((dhdpcie_bus_get_pcie_inband_dw_state(bus) ==
			DW_DEVICE_DS_DEV_WAKE)) {
			ret = dhdpcie_send_mb_data(bus, H2DMB_DS_DEVICE_WAKE_DEASSERT);
			if (ret != BCME_OK) {
				DHD_ERROR(("Failed: deassert Inband device_wake\n"));
				DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
				goto exit;
			}
			dhdpcie_bus_set_pcie_inband_dw_state(bus,
				DW_DEVICE_DS_ACTIVE);
			bus->inband_dw_deassert_cnt++;
		} else if ((dhdpcie_bus_get_pcie_inband_dw_state(bus) ==
			DW_DEVICE_DS_DEV_SLEEP_PEND) &&
			(bus->host_active_cnt == 0) && (!bus->skip_ds_ack)) {
			dhdpcie_bus_set_pcie_inband_dw_state(bus, DW_DEVICE_DS_DEV_SLEEP);
			dhdpcie_send_mb_data(bus, H2D_HOST_DS_ACK);
		}

		ret = BCME_OK;
		DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
	}

exit:
	return ret;
}

#ifdef PCIE_INB_DSACK_EXT_WAIT
void
dhd_bus_ds_ack_debug_dump(struct dhd_bus *bus)
{
	if ((DIV_U64_BY_U32(bus->isr_entry_time, NSEC_PER_SEC) -
		DIV_U64_BY_U32(bus->prev_isr_entry_time, NSEC_PER_SEC)) >=
		DW_DS_ACK_RETRY_THRESHOLD) {
		bus->dhd->dsack_hc_due_to_isr_delay = TRUE;
		DHD_ERROR(("ISR did not run causing DS ack timeout\n"));
	} else if ((bus->dpc_entry_time > bus->isr_entry_time) &&
		(DIV_U64_BY_U32(bus->dpc_entry_time, NSEC_PER_SEC) -
		DIV_U64_BY_U32(bus->isr_entry_time, NSEC_PER_SEC)) >=
		DW_DS_ACK_RETRY_THRESHOLD) {
		bus->dhd->dsack_hc_due_to_dpc_delay = TRUE;
		DHD_ERROR(("Delay in scheduling DPC causing DS ack timeout\n"));
	} else {
		DHD_ERROR(("Failed to send DS ack\n"));
	}
	DHD_ERROR(("prev_isr_entry_time="SEC_USEC_FMT
		" isr_entry_time="SEC_USEC_FMT
		" dpc_entry_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(bus->prev_isr_entry_time),
		GET_SEC_USEC(bus->isr_entry_time),
		GET_SEC_USEC(bus->dpc_entry_time)));
}
#endif /* PCIE_INB_DSACK_EXT_WAIT */
#endif /* PCIE_INB_DW */
#if defined(PCIE_OOB) || defined(PCIE_INB_DW)
void
dhd_bus_doorbell_timeout_reset(struct dhd_bus *bus)
{
	if (dhd_doorbell_timeout) {
#ifdef BCMQT
		uint wd_scale = 1;
#else
		uint wd_scale = dhd_watchdog_ms;
#endif
		if (dhd_watchdog_ms) {
			dhd_timeout_start(&bus->doorbell_timer,
				(dhd_doorbell_timeout * 1000) / wd_scale);
		}
	}
	else if (!(bus->dhd->busstate == DHD_BUS_SUSPEND)) {
		dhd_bus_set_device_wake(bus, FALSE);
	}
}

int
dhd_bus_set_device_wake(struct dhd_bus *bus, bool val)
{
	if (bus->ds_enabled && bus->dhd->ring_attached) {
#ifdef PCIE_INB_DW
		if (INBAND_DW_ENAB(bus)) {
			return dhd_bus_inb_set_device_wake(bus, val);
		}
#endif /* PCIE_INB_DW */
#ifdef PCIE_OOB
		if (OOB_DW_ENAB(bus)) {
			return dhd_os_oob_set_device_wake(bus, val);
		}
#endif /* PCIE_OOB */
	}
	return BCME_OK;
}

void
dhd_bus_dw_deassert(dhd_pub_t *dhd)
{
	dhd_bus_t *bus = dhd->bus;
	unsigned long flags;

	if (dhd_query_bus_erros(bus->dhd)) {
		return;
	}

	/* If haven't communicated with device for a while, deassert the Device_Wake GPIO */
	if (dhd_doorbell_timeout != 0 && bus->dhd->busstate == DHD_BUS_DATA &&
		dhd_timeout_expired(&bus->doorbell_timer) &&
		!dhd_query_bus_erros(bus->dhd)) {
		DHD_GENERAL_LOCK(dhd, flags);
		if (DHD_BUS_BUSY_CHECK_IDLE(dhd) &&
			!DHD_CHECK_CFG_IN_PROGRESS(dhd)) {
			DHD_BUS_BUSY_SET_IN_DS_DEASSERT(dhd);
			DHD_GENERAL_UNLOCK(dhd, flags);
			dhd_bus_set_device_wake(bus, FALSE);
			DHD_GENERAL_LOCK(dhd, flags);
			DHD_BUS_BUSY_CLEAR_IN_DS_DEASSERT(dhd);
			dhd_os_busbusy_wake(bus->dhd);
			DHD_GENERAL_UNLOCK(dhd, flags);
		} else {
			DHD_GENERAL_UNLOCK(dhd, flags);
		}
	}

#ifdef PCIE_INB_DW
	if (INBAND_DW_ENAB(bus)) {
		if (bus->ds_exit_timeout) {
			bus->ds_exit_timeout --;
			if (bus->ds_exit_timeout == 1) {
				DHD_ERROR(("DS-EXIT TIMEOUT\n"));
				bus->ds_exit_timeout = 0;
				bus->inband_ds_exit_to_cnt++;
			}
		}
		if (bus->host_sleep_exit_timeout) {
			bus->host_sleep_exit_timeout --;
			if (bus->host_sleep_exit_timeout == 1) {
				DHD_ERROR(("HOST_SLEEP-EXIT TIMEOUT\n"));
				bus->host_sleep_exit_timeout = 0;
				bus->inband_host_sleep_exit_to_cnt++;
			}
		}
	}
#endif /* PCIE_INB_DW */
}
#endif /* defined(PCIE_OOB) || defined(PCIE_INB_DW) */

/** mailbox doorbell ring function */
void
dhd_bus_ringbell(struct dhd_bus *bus, uint32 value)
{
	/* Skip once bus enters low power state (D3_INFORM/D3_ACK) */
	if (__DHD_CHK_BUS_IN_LPS(bus)) {
		DHD_ERROR(("%s: trying to ring the doorbell after D3 inform %d\n",
			__FUNCTION__, bus->bus_low_power_state));
		return;
	}

	/* Skip in the case of link down */
	if (bus->is_linkdown) {
		DHD_ERROR(("%s: PCIe link was down\n", __FUNCTION__));
		return;
	}

	if ((bus->sih->buscorerev == 2) || (bus->sih->buscorerev == 6) ||
		(bus->sih->buscorerev == 4)) {
		si_corereg(bus->sih, bus->sih->buscoreidx, bus->pcie_mailbox_int,
			PCIE_INTB, PCIE_INTB);
	} else {
		/* this is a pcie core register, not the config regsiter */
		/* XXX: makesure we are on PCIE */
		DHD_INFO(("%s: writing a door bell to the device\n", __FUNCTION__));
		if (IDMA_ACTIVE(bus->dhd)) {
			if (DAR_PWRREQ(bus)) {
				dhd_bus_pcie_pwr_req(bus);
			}
			si_corereg(bus->sih, bus->sih->buscoreidx, dhd_bus_db0_addr_2_get(bus),
				~0, value);
		} else {
			if (DAR_PWRREQ(bus)) {
				dhd_bus_pcie_pwr_req(bus);
			}
			si_corereg(bus->sih, bus->sih->buscoreidx,
				dhd_bus_db0_addr_get(bus), ~0, 0x12345678);
		}
	}
}

/** mailbox doorbell ring function for IDMA/IFRM using dma channel2 */
void
dhd_bus_ringbell_2(struct dhd_bus *bus, uint32 value, bool devwake)
{
	/* this is a pcie core register, not the config regsiter */
	/* XXX: makesure we are on PCIE */
	/* Skip once bus enters low power state (D3_INFORM/D3_ACK) */
	if (__DHD_CHK_BUS_IN_LPS(bus)) {
		DHD_ERROR(("%s: trying to ring the doorbell after D3 inform %d\n",
			__FUNCTION__, bus->bus_low_power_state));
		return;
	}

	/* Skip in the case of link down */
	if (bus->is_linkdown) {
		DHD_ERROR(("%s: PCIe link was down\n", __FUNCTION__));
		return;
	}

	DHD_INFO(("writing a door bell 2 to the device\n"));
	if (DAR_PWRREQ(bus)) {
		dhd_bus_pcie_pwr_req(bus);
	}
	si_corereg(bus->sih, bus->sih->buscoreidx, dhd_bus_db0_addr_2_get(bus),
		~0, value);
}

void
dhdpcie_bus_ringbell_fast(struct dhd_bus *bus, uint32 value)
{
	/* Skip once bus enters low power state (D3_INFORM/D3_ACK) */
	if (__DHD_CHK_BUS_IN_LPS(bus)) {
		DHD_RPM(("%s: trying to ring the doorbell after D3 inform %d\n",
			__FUNCTION__, bus->bus_low_power_state));
		return;
	}

	/* Skip in the case of link down */
	if (bus->is_linkdown) {
		DHD_ERROR(("%s: PCIe link was down\n", __FUNCTION__));
		return;
	}

#if defined(PCIE_OOB) || defined(PCIE_INB_DW)
	if (OOB_DW_ENAB(bus)) {
		dhd_bus_set_device_wake(bus, TRUE);
	}
	dhd_bus_doorbell_timeout_reset(bus);
#endif /* defined(PCIE_OOB) || defined(PCIE_INB_DW) */
#ifdef DHD_MMIO_TRACE
	dhd_bus_mmio_trace(bus, dhd_bus_db0_addr_get(bus), value,
		((value >> 24u) == 0xFF) ? TRUE : FALSE);
#endif /* defined(DHD_MMIO_TRACE) */
	if (DAR_PWRREQ(bus)) {
		dhd_bus_pcie_pwr_req(bus);
	}

	W_REG(bus->pcie_mb_intr_osh, bus->pcie_mb_intr_addr, value);
}

void
dhdpcie_bus_ringbell_2_fast(struct dhd_bus *bus, uint32 value, bool devwake)
{
	/* Skip once bus enters low power state (D3_INFORM/D3_ACK) */
	if (__DHD_CHK_BUS_IN_LPS(bus)) {
		DHD_ERROR(("%s: trying to ring the doorbell after D3 inform %d\n",
			__FUNCTION__, bus->bus_low_power_state));
		return;
	}

	/* Skip in the case of link down */
	if (bus->is_linkdown) {
		DHD_ERROR(("%s: PCIe link was down\n", __FUNCTION__));
		return;
	}

#if defined(PCIE_OOB) || defined(PCIE_INB_DW)
	if (devwake) {
		if (OOB_DW_ENAB(bus)) {
			dhd_bus_set_device_wake(bus, TRUE);
		}
	}
	dhd_bus_doorbell_timeout_reset(bus);
#endif /* defined(PCIE_OOB) || defined(PCIE_INB_DW) */

#ifdef DHD_MMIO_TRACE
	dhd_bus_mmio_trace(bus, dhd_bus_db0_addr_2_get(bus), value, TRUE);
#endif /* defined(DHD_MMIO_TRACE) */
	if (DAR_PWRREQ(bus)) {
		dhd_bus_pcie_pwr_req(bus);
	}
	W_REG(bus->pcie_mb_intr_osh, bus->pcie_mb_intr_2_addr, value);
}

static void
dhd_bus_ringbell_oldpcie(struct dhd_bus *bus, uint32 value)
{
	uint32 w;
	/* Skip once bus enters low power state (D3_INFORM/D3_ACK) */
	if (__DHD_CHK_BUS_IN_LPS(bus)) {
		DHD_ERROR(("%s: trying to ring the doorbell after D3 inform %d\n",
			__FUNCTION__, bus->bus_low_power_state));
		return;
	}

	/* Skip in the case of link down */
	if (bus->is_linkdown) {
		DHD_ERROR(("%s: PCIe link was down\n", __FUNCTION__));
		return;
	}

	w = (R_REG(bus->pcie_mb_intr_osh, bus->pcie_mb_intr_addr) & ~PCIE_INTB) | PCIE_INTB;
	W_REG(bus->pcie_mb_intr_osh, bus->pcie_mb_intr_addr, w);
}

dhd_mb_ring_t
dhd_bus_get_mbintr_fn(struct dhd_bus *bus)
{
	if ((bus->sih->buscorerev == 2) || (bus->sih->buscorerev == 6) ||
		(bus->sih->buscorerev == 4)) {
		bus->pcie_mb_intr_addr = si_corereg_addr(bus->sih, bus->sih->buscoreidx,
			bus->pcie_mailbox_int);
		if (bus->pcie_mb_intr_addr) {
			bus->pcie_mb_intr_osh = si_osh(bus->sih);
			return dhd_bus_ringbell_oldpcie;
		}
	} else {
		bus->pcie_mb_intr_addr = si_corereg_addr(bus->sih, bus->sih->buscoreidx,
			dhd_bus_db0_addr_get(bus));
		if (bus->pcie_mb_intr_addr) {
			bus->pcie_mb_intr_osh = si_osh(bus->sih);
			return dhdpcie_bus_ringbell_fast;
		}
	}
	return dhd_bus_ringbell;
}

dhd_mb_ring_2_t
dhd_bus_get_mbintr_2_fn(struct dhd_bus *bus)
{
	bus->pcie_mb_intr_2_addr = si_corereg_addr(bus->sih, bus->sih->buscoreidx,
		dhd_bus_db0_addr_2_get(bus));
	if (bus->pcie_mb_intr_2_addr) {
		bus->pcie_mb_intr_osh = si_osh(bus->sih);
		return dhdpcie_bus_ringbell_2_fast;
	}
	return dhd_bus_ringbell_2;
}

bool
BCMFASTPATH(dhd_bus_dpc)(struct dhd_bus *bus)
{
	bool resched = FALSE;	  /* Flag indicating resched wanted */
	unsigned long flags;

	DHD_TRACE(("%s: Enter\n", __FUNCTION__));

	bus->dpc_entry_time = OSL_LOCALTIME_NS();

	if (dhd_query_bus_erros(bus->dhd)) {
		if (bus->dhd->db7_trap.fw_db7w_trap_inprogress == FALSE) {
			DHD_ERROR_RLMT(("%s: db7 not in progress, exit\n", __FUNCTION__));
			return 0;
		}
	}

	/* Due to irq mismatch WARNING in linux, currently keeping it disabled and
	 * using dongle intmask to control INTR enable/disable
	 */
	if (bus->d2h_intr_control == PCIE_HOST_IRQ_CTRL) {
		/*
		 * Disable IRQ at start of DPC if it is not disabled and
		 * Enable back at the end of dpc if irq is disabled.
		 */
		if (!dhdpcie_irq_disabled(bus)) {
			bus->host_irq_disable_count++;
			dhdpcie_disable_irq_nosync(bus); /* Disable host IRQ!! */
		}
	}

	DHD_GENERAL_LOCK(bus->dhd, flags);
	/* Check for only DHD_BUS_DOWN and not for DHD_BUS_DOWN_IN_PROGRESS
	 * to avoid IOCTL Resumed On timeout when ioctl is waiting for response
	 * and rmmod is fired in parallel, which will make DHD_BUS_DOWN_IN_PROGRESS
	 * and if we return from here, then IOCTL response will never be handled
	 */
	if (bus->dhd->busstate == DHD_BUS_DOWN) {
		DHD_ERROR(("%s: Bus down, ret\n", __FUNCTION__));
		bus->intstatus = 0;
		DHD_GENERAL_UNLOCK(bus->dhd, flags);
		bus->dpc_return_busdown_count++;
		return 0;
	}
	DHD_BUS_BUSY_SET_IN_DPC(bus->dhd);
	DHD_GENERAL_UNLOCK(bus->dhd, flags);

#ifdef DHD_READ_INTSTATUS_IN_DPC
	if (bus->ipend) {
		bus->ipend = FALSE;
		bus->intstatus = dhdpcie_bus_intstatus(bus);
		/* Check if the interrupt is ours or not */
		if (bus->intstatus == 0) {
			goto INTR_ON;
		}
		bus->intrcount++;
	}
#endif /* DHD_READ_INTSTATUS_IN_DPC */

	resched = dhdpcie_bus_process_mailbox_intr(bus, bus->intstatus);
	if (!resched) {
		bus->intstatus = 0;
#ifdef DHD_READ_INTSTATUS_IN_DPC
INTR_ON:
#endif /* DHD_READ_INTSTATUS_IN_DPC */
		bus->dpc_exit_time = OSL_LOCALTIME_NS();
		bus->dpc_time_usec = DIV_U64_BY_U64((bus->dpc_exit_time - bus->dpc_entry_time), NSEC_PER_USEC);
		if (!dhd_query_bus_erros(bus->dhd)) {
			/* Due to irq mismatch WARNING in linux, currently keeping it disabled and
			 * using dongle intmask to control INTR enable/disable
			 */
			if (bus->d2h_intr_control == PCIE_HOST_IRQ_CTRL) {
				bus->host_irq_enable_count += dhdpcie_irq_disabled(bus);
				dhdpcie_enable_irq_loop(bus);
			} else {
				/* Enable back interrupt using Intmask! */
				dhdpcie_bus_intr_enable(bus);
				bus->dngl_intmask_enable_count++;
			}
		}
	} else {
		bus->resched_dpc_time = OSL_LOCALTIME_NS();
		bus->dpc_time_usec = DIV_U64_BY_U64((bus->resched_dpc_time - bus->dpc_entry_time), NSEC_PER_USEC);
	}
	dhd_histo_update(bus->dhd, bus->dpc_time_histo, (uint32)bus->dpc_time_usec);

	bus->dpc_sched = resched;
#ifdef DHD_FLOW_RING_STATUS_TRACE
	if (bus->dhd->dma_h2d_ring_upd_support && bus->dhd->dma_d2h_ring_upd_support &&
			(bus->dhd->ring_attached == TRUE)) {
		dhd_bus_flow_ring_status_dpc_trace(bus->dhd);
	}
#endif /* DHD_FLOW_RING_STATUS_TRACE */

	DHD_GENERAL_LOCK(bus->dhd, flags);
	DHD_BUS_BUSY_CLEAR_IN_DPC(bus->dhd);
	dhd_os_busbusy_wake(bus->dhd);
	DHD_GENERAL_UNLOCK(bus->dhd, flags);

	return resched;

}

int
dhdpcie_send_mb_data(dhd_bus_t *bus, uint32 h2d_mb_data)
{
	uint32 cur_h2d_mb_data = 0;

	if (bus->is_linkdown) {
		DHD_ERROR(("%s: PCIe link was down\n", __FUNCTION__));
		return BCME_ERROR;
	}

	DHD_INFO(("%s: H2D_MB_DATA: 0x%08X\n", __FUNCTION__, h2d_mb_data));

#ifdef PCIE_INB_DW
	if (h2d_mb_data == H2D_HOST_DS_ACK) {
		dhdpcie_set_dongle_deepsleep(bus, TRUE);
	}
	dhd_bus_ds_trace(bus, h2d_mb_data, FALSE, dhdpcie_bus_get_pcie_inband_dw_state(bus));
#else
	dhd_bus_ds_trace(bus, h2d_mb_data, FALSE);
#endif /* PCIE_INB_DW */
	if (bus->api.fw_rev >= PCIE_SHARED_VERSION_6 && !bus->use_mailbox) {
		DHD_INFO(("API rev is 6, sending mb data as H2D Ctrl message to dongle, 0x%04x\n",
			h2d_mb_data));
		/* Prevent asserting device_wake during doorbell ring for mb data to avoid loop. */
#ifdef PCIE_OOB
		bus->oob_enabled = FALSE;
#endif /* PCIE_OOB */
		/* XXX: check the error return value here... */
		if (dhd_prot_h2d_mbdata_send_ctrlmsg(bus->dhd, h2d_mb_data)) {
			DHD_ERROR(("failure sending the H2D Mailbox message "
					"to firmware\n"));
			goto fail;
		}
#ifdef PCIE_OOB
		bus->oob_enabled = TRUE;
#endif /* PCIE_OOB */
		goto done;
	}

	dhd_bus_cmn_readshared(bus, &cur_h2d_mb_data, H2D_MB_DATA, 0);

	if (cur_h2d_mb_data != 0) {
		uint32 i = 0;
		DHD_INFO(("%s: GRRRRRRR: MB transaction is already pending 0x%04x\n", __FUNCTION__, cur_h2d_mb_data));
		/* XXX: start a zero length timer to keep checking this to be zero */
		while ((i++ < 100) && cur_h2d_mb_data) {
			OSL_DELAY(10);
			dhd_bus_cmn_readshared(bus, &cur_h2d_mb_data, H2D_MB_DATA, 0);
		}
		if (i >= 100) {
			DHD_ERROR(("%s : waited 1ms for the dngl "
				"to ack the previous mb transaction\n", __FUNCTION__));
			DHD_ERROR(("%s : MB transaction is still pending 0x%04x\n",
				__FUNCTION__, cur_h2d_mb_data));
		}
	}

	dhd_bus_cmn_writeshared(bus, &h2d_mb_data, sizeof(uint32), H2D_MB_DATA, 0);
	dhd_bus_gen_devmb_intr(bus);

done:
	if (h2d_mb_data == H2D_HOST_D3_INFORM) {
		DHD_INFO_HW4(("%s: send H2D_HOST_D3_INFORM to dongle\n", __FUNCTION__));
		bus->last_d3_inform_time = OSL_LOCALTIME_NS();
		bus->d3_inform_cnt++;
	}
	if (h2d_mb_data == H2D_HOST_D0_INFORM_IN_USE) {
		DHD_INFO_HW4(("%s: send H2D_HOST_D0_INFORM_IN_USE to dongle\n", __FUNCTION__));
		bus->d0_inform_in_use_cnt++;
	}
	if (h2d_mb_data == H2D_HOST_D0_INFORM) {
		DHD_INFO_HW4(("%s: send H2D_HOST_D0_INFORM to dongle\n", __FUNCTION__));
		bus->d0_inform_cnt++;
	}
	if (h2d_mb_data == H2D_HOST_PTM_ENABLE) {
		DHD_INFO(("%s: send H2D_HOST_PTM_ENABLE to dongle\n", __FUNCTION__));
	}
	if (h2d_mb_data == H2D_HOST_PTM_DISABLE) {
		DHD_INFO(("%s: send H2D_HOST_PTM_DISABLE to dongle\n", __FUNCTION__));
	}
	return BCME_OK;
fail:
	return BCME_ERROR;
}

static void
dhd_bus_handle_d3_ack(dhd_bus_t *bus)
{
	bus->dngl_intmask_disable_count++;

	/* Disable dongle interrupts by intmask and clear the intstatus
	 * if it is set to avoid unnecessary intrrupts after D3 ACK.
	 */
	dhdpcie_bus_intr_disable(bus); /* Disable interrupt using IntMask!! */
	dhdpcie_bus_clear_intstatus(bus);

	DHD_SET_BUS_LPS_D3_ACKED(bus);
	DHD_RPM(("%s: D3_ACK Recieved\n", __FUNCTION__));

	if (bus->dhd->dhd_induce_error == DHD_INDUCE_D3_ACK_TIMEOUT) {
		/* Set bus_low_power_state to DHD_BUS_D3_ACK_RECIEVED */
		DHD_ERROR(("%s: Due to d3ack induce error forcefully set "
		"bus_low_power_state to DHD_BUS_D3_INFORM_SENT\n", __FUNCTION__));
		DHD_SET_BUS_LPS_D3_INFORMED(bus);
	}
	/* Check for D3 ACK induce flag, which is set by firing dhd iovar to induce D3 Ack timeout.
	 * If flag is set, D3 wake is skipped, which results in to D3 Ack timeout.
	 */
	if (bus->dhd->dhd_induce_error != DHD_INDUCE_D3_ACK_TIMEOUT) {
		bus->wait_for_d3_ack = 1;
		dhd_os_d3ack_wake(bus->dhd);
	} else {
		DHD_ERROR(("%s: Inducing D3 ACK timeout\n", __FUNCTION__));
	}

	bus->last_d3_ack_time = OSL_LOCALTIME_NS();
}

void
dhd_bus_handle_mb_data(dhd_bus_t *bus, uint32 d2h_mb_data)
{
#ifdef PCIE_INB_DW
	unsigned long flags = 0;
#endif /* PCIE_INB_DW */

	DHD_INFO(("D2H_MB_DATA: 0x%04x\n", d2h_mb_data));
#ifdef PCIE_INB_DW
	DHD_BUS_INB_DW_LOCK(bus->inb_lock, flags);
	dhd_bus_ds_trace(bus, d2h_mb_data, TRUE, dhdpcie_bus_get_pcie_inband_dw_state(bus));
	DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
#else
	dhd_bus_ds_trace(bus, d2h_mb_data, TRUE);
#endif /* PCIE_INB_DW */

	if (d2h_mb_data & D2H_DEV_FWHALT) {
		if (bus->dhd->db7_trap.fw_db7w_trap_inprogress == FALSE) {
			DHD_ERROR(("FW trap has happened, dongle_trap_data 0x%8x\n",
				bus->dhd->dongle_trap_data));
		}

		if (bus->dhd->dongle_trap_data & D2H_DEV_TRAP_HOSTDB) {
			uint64 db7_dur;

			bus->dhd->db7_trap.debug_db7_trap_time = OSL_LOCALTIME_NS();
			bus->dhd->db7_trap.debug_db7_trap_cnt++;
			db7_dur = bus->dhd->db7_trap.debug_db7_trap_time -
				bus->dhd->db7_trap.debug_db7_send_time;
			if (db7_dur > bus->dhd->db7_trap.debug_max_db7_dur) {
				bus->dhd->db7_trap.debug_max_db7_send_time =
					bus->dhd->db7_trap.debug_db7_send_time;
				bus->dhd->db7_trap.debug_max_db7_trap_time =
					bus->dhd->db7_trap.debug_db7_trap_time;
			}
			bus->dhd->db7_trap.debug_max_db7_dur =
				MAX(bus->dhd->db7_trap.debug_max_db7_dur, db7_dur);
			if (bus->dhd->db7_trap.fw_db7w_trap_inprogress == FALSE) {
				bus->dhd->db7_trap.debug_db7_timing_error_cnt++;
			}
		} else {
			dhdpcie_checkdied(bus, NULL, 0);
#ifdef BCM_ROUTER_DHD
			dhdpcie_handle_dongle_trap(bus);
#endif
#ifdef OEM_ANDROID
			dhd_os_check_hang(bus->dhd, 0, -EREMOTEIO);
#endif /* OEM_ANDROID */
		}
		if (bus->dhd->db7_trap.fw_db7w_trap_inprogress) {
			bus->dhd->db7_trap.fw_db7w_trap_inprogress = FALSE;
#if !defined(OEM_ANDROID)
			/* Fix for UTF issue */
			bus->dhd->dongle_trap_occured = TRUE;
#endif
		}
		goto exit;
	}
	if (d2h_mb_data & D2H_DEV_DS_ENTER_REQ)  {
		bool ds_acked = FALSE;
		BCM_REFERENCE(ds_acked);
		if (__DHD_CHK_BUS_LPS_D3_ACKED(bus)) {
			DHD_ERROR(("DS-ENTRY AFTER D3-ACK!!!!! QUITING\n"));
			DHD_ERROR(("%s: making DHD_BUS_DOWN\n", __FUNCTION__));
			bus->dhd->busstate = DHD_BUS_DOWN;
			goto exit;
		}
		/* what should we do */
		DHD_INFO(("D2H_MB_DATA: DEEP SLEEP REQ\n"));
#ifdef PCIE_INB_DW
		if (INBAND_DW_ENAB(bus)) {
			/* As per inband state machine, host should not send DS-ACK
			 * during suspend or suspend in progress, instead D3 inform will be sent.
			 */
			DHD_BUS_INB_DW_LOCK(bus->inb_lock, flags);
			if (!bus->skip_ds_ack) {
				if (dhdpcie_bus_get_pcie_inband_dw_state(bus)
					== DW_DEVICE_DS_ACTIVE) {
					dhdpcie_bus_set_pcie_inband_dw_state(bus,
						DW_DEVICE_DS_DEV_SLEEP_PEND);
					 if (bus->host_active_cnt == 0) {
						dhdpcie_bus_set_pcie_inband_dw_state(bus,
							DW_DEVICE_DS_DEV_SLEEP);
						dhdpcie_send_mb_data(bus, H2D_HOST_DS_ACK);
						ds_acked = TRUE;
						DHD_INFO(("D2H_MB_DATA: sent DEEP SLEEP"
							"ACK to DNGL\n"));
					} else {
						DHD_INFO(("%s: Host is active, "
							"skip sending DS-ACK. "
							"host_active_cnt is %d\n",
							__FUNCTION__, bus->host_active_cnt));
					}
				}
				/* Currently DW_DEVICE_HOST_SLEEP_WAIT is set only
				 * under dhd_bus_suspend() function.
				 */
				else if (dhdpcie_bus_get_pcie_inband_dw_state(bus)
				== DW_DEVICE_HOST_SLEEP_WAIT) {
					DHD_ERROR(("%s: DS-ACK not sent due to suspend "
						"in progress\n", __FUNCTION__));
				} else {
					DHD_ERROR_RLMT(("%s: Failed to send DS-ACK, DS state is %d",
						__FUNCTION__,
						dhdpcie_bus_get_pcie_inband_dw_state(bus)));
				}
				DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
				dhd_os_ds_enter_wake(bus->dhd);
			} else {
				DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
				DHD_INFO(("%s: Skip DS-ACK due to "
					"suspend in progress\n", __FUNCTION__));
			}
		} else
#endif /* PCIE_INB_DW */
		{
			dhdpcie_send_mb_data(bus, H2D_HOST_DS_ACK);
			DHD_INFO(("D2H_MB_DATA: sent DEEP SLEEP ACK\n"));
		}
	}
	if (d2h_mb_data & D2H_DEV_DS_EXIT_NOTE)  {
#ifdef PCIE_INB_DW
		if (INBAND_DW_ENAB(bus)) {
			if (bus->calc_ds_exit_latency) {
				bus->ds_exit_ts2 = OSL_SYSUPTIME_US();
				if (bus->ds_exit_ts2 > bus->ds_exit_ts1 &&
						bus->ds_exit_ts1 != 0)
					bus->ds_exit_latency = bus->ds_exit_ts2 - bus->ds_exit_ts1;
				else
					bus->ds_exit_latency = 0;
			}
		}
#endif /* PCIE_INB_DW */
		/* what should we do */
		DHD_INFO(("D2H_MB_DATA: DEEP SLEEP EXIT\n"));
#ifdef PCIE_INB_DW
		if (INBAND_DW_ENAB(bus)) {
			DHD_BUS_INB_DW_LOCK(bus->inb_lock, flags);
			if (dhdpcie_bus_get_pcie_inband_dw_state(bus) ==
					DW_DEVICE_DS_DISABLED_WAIT) {
				/* wake up only if some one is waiting in
				* DW_DEVICE_DS_DISABLED_WAIT state
				* in this case the waiter will change the state
				* to DW_DEVICE_DS_DEV_WAKE
				*/
				bus->inband_ds_exit_host_cnt++;
				/* To synchronize with the previous memory operations call wmb() */
				OSL_SMP_WMB();
				bus->wait_for_ds_exit = 1;
				/* Call another wmb() to make sure before waking up the
				 * other event value gets updated.
				 */
				OSL_SMP_WMB();
				dhdpcie_bus_set_pcie_inband_dw_state(bus,
					DW_DEVICE_DS_DEV_WAKE);
				DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
				dhd_os_ds_exit_wake(bus->dhd);
			} else if (dhdpcie_bus_get_pcie_inband_dw_state(bus) ==
					DW_DEVICE_DS_DEV_SLEEP) {
				DHD_INFO(("recvd unsolicited DS-EXIT from dongle in DEV_SLEEP\n"));
				/*
				* unsolicited state change to DW_DEVICE_DS_DEV_WAKE if
				* D2H_DEV_DS_EXIT_NOTE received in DW_DEVICE_DS_DEV_SLEEP state.
				* This is need when dongle is woken by external events like
				* WOW, ping ..etc
				*/
				bus->inband_ds_exit_device_cnt++;
				dhdpcie_bus_set_pcie_inband_dw_state(bus,
					DW_DEVICE_DS_DEV_WAKE);
				DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
			} else {
				DHD_INFO(("D2H_MB_DATA: not in DS_DISABLED_WAIT/DS_DEV_SLEEP\n"));
				bus->inband_ds_exit_host_cnt++;
				DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
			}
			/*
			 * bus->deep_sleep is TRUE by default. deep_sleep is set to FALSE when the
			 * dhd iovar deep_sleep is fired with value 0(user request to not enter
			 * deep sleep). So donot attempt to go to deep sleep when user has
			 * explicitly asked not to go to deep sleep. bus->deep_sleep is set to
			 * TRUE when the dhd iovar deep_sleep is fired with value 1.
			 */
			if (bus->deep_sleep) {
				dhd_bus_set_device_wake(bus, FALSE);
				dhdpcie_set_dongle_deepsleep(bus, FALSE);
			}
		}
#endif /* PCIE_INB_DW */
	}
	if (d2h_mb_data & D2HMB_DS_HOST_SLEEP_EXIT_ACK)  {
		/* what should we do */
		DHD_INFO(("D2H_MB_DATA: D0 ACK\n"));
#ifdef PCIE_INB_DW
		if (INBAND_DW_ENAB(bus)) {
			DHD_BUS_INB_DW_LOCK(bus->inb_lock, flags);
			if (dhdpcie_bus_get_pcie_inband_dw_state(bus) ==
				DW_DEVICE_HOST_WAKE_WAIT) {
				dhdpcie_bus_set_pcie_inband_dw_state(bus, DW_DEVICE_DS_ACTIVE);
			}
			DHD_BUS_INB_DW_UNLOCK(bus->inb_lock, flags);
		}
#endif /* PCIE_INB_DW */
	}
	if (d2h_mb_data & D2H_DEV_D3_ACK)  {
		/* what should we do */
		DHD_INFO_HW4(("D2H_MB_DATA: D3 ACK\n"));
		if (!bus->wait_for_d3_ack) {
#if defined(DHD_HANG_SEND_UP_TEST)
			if (bus->dhd->req_hang_type == HANG_REASON_D3_ACK_TIMEOUT) {
				DHD_ERROR(("TEST HANG: Skip to process D3 ACK\n"));
			} else {
				dhd_bus_handle_d3_ack(bus);
			}
#else /* DHD_HANG_SEND_UP_TEST */
			dhd_bus_handle_d3_ack(bus);
#endif /* DHD_HANG_SEND_UP_TEST */
		}
	}

	if (d2h_mb_data & D2H_DEV_PTM_ENABLED)  {
		DHD_INFO(("D2H_MB_DATA: PTM ENABLED\n"));
	}

	if (d2h_mb_data & D2H_DEV_PTM_DISABLED)  {
		DHD_INFO(("D2H_MB_DATA: PTM DISABLED\n"));
	}

exit:
	return;
}

static void
dhdpcie_handle_mb_data(dhd_bus_t *bus)
{
	uint32 d2h_mb_data = 0;
	uint32 zero = 0;

	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req(bus);
	}

	if (bus->is_linkdown) {
		DHD_ERROR(("%s: PCIe link is down\n", __FUNCTION__));
		return;
	}

	dhd_bus_cmn_readshared(bus, &d2h_mb_data, D2H_MB_DATA, 0);
	if (D2H_DEV_MB_INVALIDATED(d2h_mb_data)) {
		DHD_ERROR(("%s: Invalid D2H_MB_DATA: 0x%08x\n",
			__FUNCTION__, d2h_mb_data));
		goto exit;
	}

	dhd_bus_cmn_writeshared(bus, &zero, sizeof(uint32), D2H_MB_DATA, 0);

	DHD_INFO_HW4(("%s: D2H_MB_DATA: 0x%04x\n", __FUNCTION__, d2h_mb_data));
	if (d2h_mb_data & D2H_DEV_FWHALT)  {
		DHD_ERROR(("FW trap has happened\n"));
		dhdpcie_checkdied(bus, NULL, 0);
		/* not ready yet dhd_os_ind_firmware_stall(bus->dhd); */
#ifdef BCM_ROUTER_DHD
		dhdpcie_handle_dongle_trap(bus);
#endif
		goto exit;
	}
	if (d2h_mb_data & D2H_DEV_DS_ENTER_REQ)  {
		/* what should we do */
		DHD_INFO(("%s: D2H_MB_DATA: DEEP SLEEP REQ\n", __FUNCTION__));
		dhdpcie_send_mb_data(bus, H2D_HOST_DS_ACK);
		DHD_INFO(("%s: D2H_MB_DATA: sent DEEP SLEEP ACK\n", __FUNCTION__));
	}
	if (d2h_mb_data & D2H_DEV_DS_EXIT_NOTE)  {
		/* what should we do */
		DHD_INFO(("%s: D2H_MB_DATA: DEEP SLEEP EXIT\n", __FUNCTION__));
	}
	if (d2h_mb_data & D2H_DEV_D3_ACK)  {
		/* what should we do */
		DHD_INFO_HW4(("%s: D2H_MB_DATA: D3 ACK\n", __FUNCTION__));
		if (!bus->wait_for_d3_ack) {
#if defined(DHD_HANG_SEND_UP_TEST)
			if (bus->dhd->req_hang_type == HANG_REASON_D3_ACK_TIMEOUT) {
				DHD_ERROR(("TEST HANG: Skip to process D3 ACK\n"));
			} else {
			dhd_bus_handle_d3_ack(bus);
			}
#else /* DHD_HANG_SEND_UP_TEST */
			dhd_bus_handle_d3_ack(bus);
#endif /* DHD_HANG_SEND_UP_TEST */
		}
	}

exit:
	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req_clear(bus);
	}
}

static void
dhdpcie_read_handle_mb_data(dhd_bus_t *bus)
{
	uint32 d2h_mb_data = 0;
	uint32 zero = 0;

	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req(bus);
	}

	dhd_bus_cmn_readshared(bus, &d2h_mb_data, D2H_MB_DATA, 0);
	if (!d2h_mb_data) {
		goto exit;
	}

	dhd_bus_cmn_writeshared(bus, &zero, sizeof(uint32), D2H_MB_DATA, 0);

	dhd_bus_handle_mb_data(bus, d2h_mb_data);

exit:
	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req_clear(bus);
	}
}

#define DHD_SCHED_RETRY_DPC_DELAY_MS 100u

static void
dhd_bus_handle_intx_ahead_dma_indices(dhd_bus_t *bus)
{
	if (bus->d2h_intr_method == PCIE_MSI) {
		DHD_INFO(("%s: not required for msi\n", __FUNCTION__));
		return;
	}

	if (bus->dhd->dma_d2h_ring_upd_support == FALSE) {
		DHD_INFO(("%s: not required for non-dma-indices\n", __FUNCTION__));
		return;
	}

	if (dhd_query_bus_erros(bus->dhd)) {
		return;
	}

#ifndef NDIS
	/*
	 * skip delayed dpc if tasklet is scheduled by non-ISR
	 * From ISR context, if dpc is scheduled, the isr_sched_dpc_time
	 * will be lesser than dpc_exit_time.
	 */
	if (bus->isr_sched_dpc_time < bus->dpc_exit_time) {
		DHD_INFO(("%s: skip delayed dpc as tasklet is scheduled from non isr\n",
			__FUNCTION__));
		return;
	}
#endif /* NDIS */

	if (DHD_CHK_BUS_LPS_D3_ACKED(bus)) {
		DHD_INFO(("%s: skip delayed dpc as d3 ack is received\n", __FUNCTION__));
		return;
	}

	DHD_INFO(("%s: schedule delayed DPC\n", __FUNCTION__));
	dhd_schedule_delayed_dpc_on_dpc_cpu(bus->dhd, DHD_SCHED_RETRY_DPC_DELAY_MS);
	return;
}

static bool
dhdpcie_bus_process_mailbox_intr(dhd_bus_t *bus, uint32 intstatus)
{
	bool resched = FALSE;

	/* Do not process dpc after receiving D3_ACK */
	if (DHD_CHK_BUS_LPS_D3_ACKED(bus)) {
		DHD_RPM(("%s: D3 Ack Received, skip dpc\n", __FUNCTION__));
		return resched;
	}

	if ((bus->sih->buscorerev == 2) || (bus->sih->buscorerev == 6) ||
		(bus->sih->buscorerev == 4)) {
		/* Msg stream interrupt */
		if (intstatus & I_BIT1) {
			resched = dhdpci_bus_read_frames(bus);
		} else if (intstatus & I_BIT0) {
			/* do nothing for Now */
		}
	} else {
		if (intstatus & (PCIE_MB_TOPCIE_FN0_0 | PCIE_MB_TOPCIE_FN0_1))
			bus->api.handle_mb_data(bus);

		/* The fact that we are here implies that dhdpcie_bus_intstatus( )
		* retuned a non-zer0 status after applying the current mask.
		* No further check required, in fact bus->instatus can be eliminated.
		* Both bus->instatus, and bud->intdis are shared between isr and dpc.
		*/
#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
		if (pm_runtime_get(dhd_bus_to_dev(bus)) >= 0) {
			resched = dhdpci_bus_read_frames(bus);
			pm_runtime_mark_last_busy(dhd_bus_to_dev(bus));
			pm_runtime_put_autosuspend(dhd_bus_to_dev(bus));
		}
#else
		resched = dhdpci_bus_read_frames(bus);
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */
	}

	dhd_bus_handle_intx_ahead_dma_indices(bus);

	return resched;
}

#if defined(DHD_H2D_LOG_TIME_SYNC)
static void
dhdpci_bus_rte_log_time_sync_poll(dhd_bus_t *bus)
{
	unsigned long time_elapsed;

	/* Poll for timeout value periodically */
	if ((bus->dhd->busstate == DHD_BUS_DATA) &&
		(bus->dhd->dhd_rte_time_sync_ms != 0) &&
		DHD_CHK_BUS_NOT_IN_LPS(bus)) {
		/*
		 * XXX OSL_SYSUPTIME_US() overflow should not happen.
		 * As it is a unsigned 64 bit value 18446744073709551616L,
		 * which needs 213503982334 days to overflow
		 */
		time_elapsed = OSL_SYSUPTIME_US() - bus->dhd_rte_time_sync_count;
		/* Compare time is milli seconds */
		if ((time_elapsed / 1000) >= bus->dhd->dhd_rte_time_sync_ms) {
			/*
			 * Its fine, if it has crossed the timeout value. No need to adjust the
			 * elapsed time
			 */
			bus->dhd_rte_time_sync_count += time_elapsed;

			/* Schedule deffered work. Work function will send IOVAR. */
			dhd_h2d_log_time_sync_deferred_wq_schedule(bus->dhd);
		}
	}
}
#endif /* DHD_H2D_LOG_TIME_SYNC */

static bool
dhdpci_bus_read_frames(dhd_bus_t *bus)
{
	bool more = FALSE;
	uint32 ctrlcpl_items = 0;
	uint32 txcpl_items = 0;
	uint32 rxcpl_items = 0;
	uint32 evtlog_items = 0;
	uint64 read_frames_entry_time = OSL_LOCALTIME_NS();

	/* First check if there a FW trap */
	if ((bus->api.fw_rev >= PCIE_SHARED_VERSION_6) &&
		(bus->dhd->dongle_trap_data = dhd_prot_process_trapbuf(bus->dhd))) {
#ifdef DNGL_AXI_ERROR_LOGGING
		if (bus->dhd->axi_error) {
			DHD_ERROR(("AXI Error happened\n"));
			return FALSE;
		}
#endif /* DNGL_AXI_ERROR_LOGGING */
		dhd_bus_handle_mb_data(bus, D2H_DEV_FWHALT);
		return FALSE;
	}

	if (dhd_query_bus_erros(bus->dhd)) {
		DHD_ERROR(("%s: detected bus errors. Hence donot process msg rings\n",
			__FUNCTION__));
		return FALSE;
	}
#ifdef DHD_DMA_INDICES_SEQNUM
	dhd_prot_save_dmaidx(bus->dhd);
#endif /* DHD_DMA_INDICES_SEQNUM */
	/* There may be frames in both ctrl buf and data buf; check ctrl buf first */
	more |= dhd_prot_process_ctrlbuf(bus->dhd, &ctrlcpl_items);
	bus->last_process_ctrlbuf_time = OSL_LOCALTIME_NS();

	bus->ctrl_cpl_post_time_usec =
		DIV_U64_BY_U64((bus->last_process_ctrlbuf_time - read_frames_entry_time), NSEC_PER_USEC);
	dhd_histo_update(bus->dhd, bus->ctrl_cpl_post_time_histo,
		(uint32)bus->ctrl_cpl_post_time_usec);

	/* Do not process rest of ring buf once bus enters low power state (D3_INFORM/D3_ACK) */
	if (DHD_CHK_BUS_IN_LPS(bus)) {
		DHD_RPM(("%s: Bus is in power save state (%d). "
			"Skip processing rest of ring buffers.\n",
			__FUNCTION__, bus->bus_low_power_state));
		return FALSE;
	}

	/* update the flow ring cpls */
	more |= dhd_update_txflowrings(bus->dhd);
	bus->last_process_flowring_time = OSL_LOCALTIME_NS();

	bus->tx_post_time_usec =
		DIV_U64_BY_U64((bus->last_process_flowring_time - bus->last_process_ctrlbuf_time), NSEC_PER_USEC);
	dhd_histo_update(bus->dhd, bus->tx_post_time_histo, (uint32)bus->tx_post_time_usec);

	/* With heavy TX traffic, we could get a lot of TxStatus
	 * so add bound
	 */
	more |= dhd_prot_process_msgbuf_txcpl(bus->dhd, DHD_REGULAR_RING, &txcpl_items);
	bus->last_process_txcpl_time = OSL_LOCALTIME_NS();

	bus->tx_cpl_time_usec =
		DIV_U64_BY_U64((bus->last_process_txcpl_time - bus->last_process_flowring_time), NSEC_PER_USEC);
	dhd_histo_update(bus->dhd, bus->tx_cpl_time_histo, (uint32)bus->tx_cpl_time_usec);

	/* With heavy RX traffic, this routine potentially could spend some time
	 * processing RX frames without RX bound
	 */
	more |= dhd_prot_process_msgbuf_rxcpl(bus->dhd, DHD_REGULAR_RING, &rxcpl_items);
	bus->last_process_rxcpl_time = OSL_LOCALTIME_NS();

	bus->rx_cpl_post_time_usec =
		DIV_U64_BY_U64((bus->last_process_rxcpl_time - bus->last_process_txcpl_time), NSEC_PER_USEC);
	dhd_histo_update(bus->dhd, bus->rx_cpl_post_time_histo, (uint32)bus->rx_cpl_post_time_usec);

	/* Process info ring completion messages */
#ifdef EWP_EDL
	if (!bus->dhd->dongle_edl_support)
#endif
	{
		more |= dhd_prot_process_msgbuf_infocpl(bus->dhd, DHD_INFORING_BOUND,
			&evtlog_items);
		bus->last_process_infocpl_time = OSL_LOCALTIME_NS();
	}
#ifdef EWP_EDL
	else {
		more |= dhd_prot_process_msgbuf_edl(bus->dhd, &evtlog_items);
		bus->last_process_edl_time = OSL_LOCALTIME_NS();
	}
#endif /* EWP_EDL */

#ifdef IDLE_TX_FLOW_MGMT
	if (bus->enable_idle_flowring_mgmt) {
		/* Look for idle flow rings */
		dhd_bus_check_idle_scan(bus);
	}
#endif /* IDLE_TX_FLOW_MGMT */

	/* don't talk to the dongle if fw is about to be reloaded */
	if (bus->dhd->hang_was_sent) {
		more = FALSE;
	}

#if defined(DHD_H2D_LOG_TIME_SYNC)
	dhdpci_bus_rte_log_time_sync_poll(bus);
#endif /* DHD_H2D_LOG_TIME_SYNC */

#if defined(DHD_WAKE_STATUS)
	/* Check if host was woken up by any packets */
	if (dhd_bus_get_bus_wake(bus->dhd) > 0) {
		/*
		 * If wake is due to Rx packets,
		 * pktwake info will be printed and cleared from dhd_rx_frame()
		 */
		DHD_ERROR(("#### dhdpcie_host_wake: rxcpl:%d ctrlcpl:%d txcpl:%d evtlog:%d ####\n",
			rxcpl_items, ctrlcpl_items, txcpl_items, evtlog_items));

		dhd_bus_set_get_bus_wake(bus->dhd, 0);

		if (rxcpl_items > 0) {
			/* Request packet dump for first Rx packet */
			dhd_bus_set_get_bus_wake_pkt_dump(bus->dhd, 1);
		}
	}
#endif /* DHD_WAKE_STATUS */
	return more;
}

bool
dhdpcie_tcm_valid(dhd_bus_t *bus)
{
	uint32 addr = 0;
	int rv;
	uint32 shaddr = 0;
	pciedev_shared_t sh;

	shaddr = bus->dongle_ram_base + bus->ramsize - 4;

	/* Read last word in memory to determine address of pciedev_shared structure */
	addr = LTOH32(dhdpcie_bus_rtcm32(bus, shaddr));

	if ((addr == 0) || (addr == bus->nvram_csm) || (addr < bus->dongle_ram_base) ||
		(addr > shaddr)) {
		DHD_ERROR(("%s: address (0x%08x) of pciedev_shared invalid addr\n",
			__FUNCTION__, addr));
		return FALSE;
	}

	/* Read hndrte_shared structure */
	if ((rv = dhdpcie_bus_membytes(bus, FALSE, addr, (uint8 *)&sh,
		sizeof(pciedev_shared_t))) < 0) {
		DHD_ERROR(("Failed to read PCIe shared struct with %d\n", rv));
		return FALSE;
	}

	/* Compare any field in pciedev_shared_t */
	if (sh.console_addr != bus->pcie_sh->console_addr) {
		DHD_ERROR(("Contents of pciedev_shared_t structure are not matching.\n"));
		return FALSE;
	}

	return TRUE;
}

static void
dhdpcie_update_bus_api_revisions(uint32 firmware_api_version, uint32 host_api_version)
{
	snprintf(bus_api_revision, BUS_API_REV_STR_LEN, "\nBus API revisions:(FW rev%d)(DHD rev%d)",
			firmware_api_version, host_api_version);
	return;
}

static bool
dhdpcie_check_firmware_compatible(uint32 firmware_api_version, uint32 host_api_version)
{
	bool retcode = FALSE;

	DHD_INFO(("firmware api revision %d, host api revision %d\n",
		firmware_api_version, host_api_version));

	switch (firmware_api_version) {
	case PCIE_SHARED_VERSION_7:
	case PCIE_SHARED_VERSION_6:
	case PCIE_SHARED_VERSION_5:
		retcode = TRUE;
		break;
	default:
		if (firmware_api_version <= host_api_version)
			retcode = TRUE;
	}
	return retcode;
}

static int
dhdpcie_readshared(dhd_bus_t *bus)
{
	uint32 addr = 0;
	int dma_indx_wr_buf, dma_indx_rd_buf;
	uint32 shaddr = 0;
	pciedev_shared_t *sh = bus->pcie_sh;
	dhd_timeout_t tmo;
	bool idma_en = FALSE;
#if defined(PCIE_INB_DW)
	bool d2h_inband_dw = FALSE;
#endif /* defined(PCIE_INB_DW) */
#if defined(PCIE_OOB)
	bool d2h_no_oob_dw = FALSE;
#endif /* defined(PCIE_INB_DW) */
	uint32 timeout = MAX_READ_TIMEOUT;
	uint32 elapsed;
	uint32 intstatus;
	int ret = BCME_OK;

	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req(bus);
	}

	shaddr = bus->dongle_ram_base + bus->ramsize - 4;

#ifdef BCMSLTGT
#ifdef BCMQT_HW
	if (qt_dngl_timeout) {
		timeout = qt_dngl_timeout * 1000;
	}
#endif /* BCMQT_HW */
	DHD_ERROR(("%s: host timeout in QT/FPGA mode %ld ms\n",
		__FUNCTION__, (timeout * htclkratio) / USEC_PER_MSEC));
#endif /* BCMSLTGT */
#ifdef GDB_PROXY
	if (bus->dhd->gdb_proxy_active) {
		/* +3s ASLR delay timeout for case of preattachment debugging */
		timeout += 3 * 1000 * 1000;
	}
#endif /* GDB_PROXY */

#ifdef GDB_PROXY
	/* Loop while timeout is caused by firmware stop in GDB */
	GDB_PROXY_TIMEOUT_DO(bus->dhd)
	{
#endif /* GDB_PROXY */
		/* start a timer for 5 seconds */
		dhd_timeout_start(&tmo, timeout);

		while (((addr == 0) || (addr == bus->nvram_csm)) && !dhd_timeout_expired(&tmo)) {
			/* Read last word in memory to determine address of pciedev_shared structure
			 */
			addr = LTOH32(dhdpcie_bus_rtcm32(bus, shaddr));
			if (addr != bus->nvram_csm) {
				DHD_ERROR(("%s: addr=0x%x nvram_csm=0x%x\n",
					__FUNCTION__, addr, bus->nvram_csm));
			}
		}
#ifdef GDB_PROXY
	}
	GDB_PROXY_TIMEOUT_WHILE(dhd_timeout_expired(&tmo))
#endif /* GDB_PROXY */
	if (addr == (uint32)-1) {
		DHD_ERROR(("%s: ##### pciedev shared address is 0xffffffff ####\n", __FUNCTION__));

/* In phones sometimes NOC is seen for any further access after readshared fails */
#ifdef DHD_NO_DUMP_FOR_READSHARED_FAIL
		DHD_ERROR(("%s:### NO dumps will be colelcted and will be marked as linkdown ###\n",
			__FUNCTION__));
		bus->is_linkdown = 1;
		ret = BCME_ERROR;
		goto done;
#endif /* DHD_NO_DUMP_FOR_READSHARED_FAIL */

		dhd_bus_dump_imp_cfg_registers(bus);
		dhd_bus_dump_dar_registers(bus);
		/* Check the PCIe link status by reading intstatus register */
		intstatus = si_corereg(bus->sih,
				bus->sih->buscoreidx, bus->pcie_mailbox_int, 0, 0);
		if (intstatus == (uint32)-1) {
			DHD_ERROR(("%s : PCIe link might be down\n", __FUNCTION__));
			bus->is_linkdown = TRUE;
		} else {
#if defined(DHD_FW_COREDUMP)
#ifdef DHD_SSSR_DUMP
#ifdef OEM_ANDROID
			DHD_ERROR(("%s : Set collect_sssr and fis_enab as TRUE\n", __FUNCTION__));
			bus->dhd->collect_sssr = TRUE;
			fis_enab = TRUE;
#endif /* OEM_ANDROID */
#endif /* DHD_SSSR_DUMP */
			/* save core dump or write to a file */
			if (bus->dhd->memdump_enabled) {
				bus->dhd->memdump_type = DUMP_TYPE_READ_SHM_FAIL;
				dhdpcie_mem_dump(bus);
			}
#endif /* DHD_FW_COREDUMP */
		}
		ret = BCME_ERROR;
		goto done;
	}

	if ((addr == 0) || (addr == bus->nvram_csm) || (addr < bus->dongle_ram_base) ||
		(addr > shaddr)) {
		uint origidx = si_coreidx(bus->sih);
		if (si_setcore(bus->sih, ARMCA7_CORE_ID, 0)) {
			uint32 ioctrl = si_wrapperreg(bus->sih, AI_IOCTRL, 0, 0);
			uint32 resetctrl = si_wrapperreg(bus->sih, AI_RESETCTRL, 0, 0);
			DHD_ERROR(("%s: ca7 ioctrl:0x%x resetctrl:0x%x\n",
				__FUNCTION__, ioctrl, resetctrl));
		} else {
			DHD_ERROR(("%s: Failed to find CA7 core!\n", __FUNCTION__));
		}

		/* Restore back to original core */
		si_setcoreidx(bus->sih, origidx);

		elapsed = tmo.elapsed;
#ifdef BCMSLTGT
		elapsed *= htclkratio;
#endif /* BCMSLTGT */
		DHD_ERROR(("%s: address (0x%08x) of pciedev_shared invalid\n",
			__FUNCTION__, addr));
		DHD_ERROR(("%s: Waited %u usec, dongle is not ready\n", __FUNCTION__, elapsed));
#ifndef OEM_ANDROID
		if (addr != (uint32)-1) {	/* skip further PCIE reads if read this addr */
#ifdef DHD_SSSR_DUMP
			if (!bus->dhd->sssr_inited) {
				bus->dhd->force_sssr_init = TRUE;
				/* Hard code sssr_reg_info for 4389 */
				DHD_SSSR_DUMP_INIT(bus->dhd);
			}
			bus->dhd->collect_sssr = TRUE;
#endif /* DHD_SSSR_DUMP */
			if (bus->dhd->memdump_enabled) {
				bus->dhd->memdump_type = DUMP_TYPE_DONGLE_INIT_FAILURE;
				dhdpcie_mem_dump(bus);
			}
		}
#endif /* !OEM_ANDROID */
#if defined(NDIS)
		/* This is a very common code path to catch f/w init failures.
		   Capture a socram dump.
		*/
		ASSERT(0);
#endif /* defined(NDIS) */
		ret = BCME_ERROR;
		goto done;
	} else {
		bus->rd_shared_pass_time = OSL_LOCALTIME_NS();
		elapsed = tmo.elapsed;
#ifdef BCMSLTGT
		elapsed *= htclkratio;
#endif /* BCMSLTGT */
		bus->shared_addr = (ulong)addr;
		DHD_ERROR(("### Total time ARM OOR to Readshared pass took %llu usec ###\n",
			DIV_U64_BY_U32((bus->rd_shared_pass_time - bus->arm_oor_time),
			NSEC_PER_USEC)));
		DHD_ERROR(("%s: PCIe shared addr (0x%08x) read took %u usec "
			"before dongle is ready\n", __FUNCTION__, addr, elapsed));
	}

	/* Read hndrte_shared structure */
	if ((ret = dhdpcie_bus_membytes(bus, FALSE, addr, (uint8 *)sh,
		sizeof(pciedev_shared_t))) < 0) {
		DHD_ERROR(("%s: Failed to read PCIe shared struct with %d\n", __FUNCTION__, ret));
		goto done;
	}

	/* Endianness */
	sh->flags = ltoh32(sh->flags);
	sh->trap_addr = ltoh32(sh->trap_addr);
	sh->assert_exp_addr = ltoh32(sh->assert_exp_addr);
	sh->assert_file_addr = ltoh32(sh->assert_file_addr);
	sh->assert_line = ltoh32(sh->assert_line);
	sh->console_addr = ltoh32(sh->console_addr);
	sh->msgtrace_addr = ltoh32(sh->msgtrace_addr);
	sh->dma_rxoffset = ltoh32(sh->dma_rxoffset);
	sh->rings_info_ptr = ltoh32(sh->rings_info_ptr);
	sh->flags2 = ltoh32(sh->flags2);

	/* load bus console address */
	bus->console_addr = sh->console_addr;

	/* Read the dma rx offset */
	bus->dma_rxoffset = bus->pcie_sh->dma_rxoffset;
	dhd_prot_rx_dataoffset(bus->dhd, bus->dma_rxoffset);

	DHD_INFO(("%s: DMA RX offset from shared Area %d\n", __FUNCTION__, bus->dma_rxoffset));

	bus->api.fw_rev = sh->flags & PCIE_SHARED_VERSION_MASK;
	if (!(dhdpcie_check_firmware_compatible(bus->api.fw_rev, PCIE_SHARED_VERSION)))
	{
		DHD_ERROR(("%s: pcie_shared version %d in dhd "
		           "is older than pciedev_shared version %d in dongle\n",
		           __FUNCTION__, PCIE_SHARED_VERSION,
		           bus->api.fw_rev));
		ret = BCME_ERROR;
		goto done;
	}
	dhdpcie_update_bus_api_revisions(bus->api.fw_rev, PCIE_SHARED_VERSION);

	bus->rw_index_sz = (sh->flags & PCIE_SHARED_2BYTE_INDICES) ?
		sizeof(uint16) : sizeof(uint32);
	DHD_INFO(("%s: Dongle advertizes %d size indices\n",
		__FUNCTION__, bus->rw_index_sz));

#ifdef IDLE_TX_FLOW_MGMT
	if (sh->flags & PCIE_SHARED_IDLE_FLOW_RING) {
		DHD_ERROR(("%s: FW Supports IdleFlow ring managment!\n",
			__FUNCTION__));
		bus->enable_idle_flowring_mgmt = TRUE;
	}
#endif /* IDLE_TX_FLOW_MGMT */

#ifdef PCIE_OOB
	bus->dhd->d2h_no_oob_dw = (sh->flags & PCIE_SHARED_NO_OOB_DW) ? TRUE : FALSE;
	d2h_no_oob_dw = bus->dhd->d2h_no_oob_dw;
#endif /* PCIE_OOB */

#ifdef PCIE_INB_DW
	bus->dhd->d2h_inband_dw = (sh->flags & PCIE_SHARED_INBAND_DS) ? TRUE : FALSE;
	d2h_inband_dw = bus->dhd->d2h_inband_dw;
#endif /* PCIE_INB_DW */

#if defined(PCIE_INB_DW)
	DHD_ERROR(("FW supports Inband dw ? %s\n",
		d2h_inband_dw ? "Y":"N"));
#endif /* defined(PCIE_INB_DW) */

#if defined(PCIE_OOB)
	DHD_ERROR(("FW supports oob dw ? %s\n",
		d2h_no_oob_dw ? "N":"Y"));
#endif /* defined(PCIE_OOB) */

	if (IDMA_CAPABLE(bus)) {
		if (bus->sih->buscorerev == 23) {
#ifdef PCIE_INB_DW
			if (bus->dhd->d2h_inband_dw)
			{
				idma_en = TRUE;
			}
#endif /* PCIE_INB_DW */
		} else {
			idma_en = TRUE;
		}
	}

	if (idma_en) {
		bus->dhd->idma_enable = (sh->flags & PCIE_SHARED_IDMA) ? TRUE : FALSE;
		bus->dhd->ifrm_enable = (sh->flags & PCIE_SHARED_IFRM) ? TRUE : FALSE;
	}

	bus->dhd->d2h_sync_mode = sh->flags & PCIE_SHARED_D2H_SYNC_MODE_MASK;

	bus->dhd->dar_enable = (sh->flags & PCIE_SHARED_DAR) ? TRUE : FALSE;
	DHD_ERROR(("FW supports DAR ? %s\n", bus->dhd->dar_enable ? "Y":"N"));

	/* Does the FW support DMA'ing r/w indices */
	if (sh->flags & PCIE_SHARED_DMA_INDEX) {
		if (!bus->dhd->dma_ring_upd_overwrite) {
#if defined(BCM_ROUTER_DHD)
		/* Router platform does not use IOV_DMA_RINGINDICES */
			if (sh->flags & PCIE_SHARED_2BYTE_INDICES)
#endif /* BCM_ROUTER_DHD */
			{
				if (!IFRM_ENAB(bus->dhd)) {
					bus->dhd->dma_h2d_ring_upd_support = TRUE;
				}
				bus->dhd->dma_d2h_ring_upd_support = TRUE;
			}
		}

		DHD_INFO(("%s: Host support DMAing indices: H2D:%d - D2H:%d. FW supports it\n",
			__FUNCTION__,
			(bus->dhd->dma_h2d_ring_upd_support ? 1 : 0),
			(bus->dhd->dma_d2h_ring_upd_support ? 1 : 0)));
	} else if (!(sh->flags & PCIE_SHARED_D2H_SYNC_MODE_MASK)) {
		DHD_ERROR(("%s FW has to support either dma indices or d2h sync\n",
			__FUNCTION__));
		ret = BCME_UNSUPPORTED;
		goto done;
	} else {
		bus->dhd->dma_h2d_ring_upd_support = FALSE;
		bus->dhd->dma_d2h_ring_upd_support = FALSE;
	}

	/* Does the firmware support fast delete ring? */
	if (sh->flags2 & PCIE_SHARED2_FAST_DELETE_RING) {
		DHD_INFO(("%s: Firmware supports fast delete ring\n",
			__FUNCTION__));
		bus->dhd->fast_delete_ring_support = TRUE;
	} else {
		DHD_INFO(("%s: Firmware does not support fast delete ring\n",
			__FUNCTION__));
		bus->dhd->fast_delete_ring_support = FALSE;
	}

	/* get ring_info, ring_state and mb data ptrs and store the addresses in bus structure */
	{
		ring_info_t  ring_info;

		/* boundary check */
		if ((sh->rings_info_ptr < bus->dongle_ram_base) || (sh->rings_info_ptr > shaddr)) {
			DHD_ERROR(("%s: rings_info_ptr is invalid 0x%x, skip reading ring info\n",
				__FUNCTION__, sh->rings_info_ptr));
			ret = BCME_ERROR;
			goto done;
		}

		if ((ret = dhdpcie_bus_membytes(bus, FALSE, sh->rings_info_ptr,
			(uint8 *)&ring_info, sizeof(ring_info_t))) < 0) {
			goto done;
		}

		bus->h2d_mb_data_ptr_addr = ltoh32(sh->h2d_mb_data_ptr);
		bus->d2h_mb_data_ptr_addr = ltoh32(sh->d2h_mb_data_ptr);

		if (bus->api.fw_rev >= PCIE_SHARED_VERSION_6) {
			bus->max_tx_flowrings = ltoh16(ring_info.max_tx_flowrings);
			bus->max_submission_rings = ltoh16(ring_info.max_submission_queues);
			bus->max_completion_rings = ltoh16(ring_info.max_completion_rings);
			bus->max_cmn_rings = bus->max_submission_rings - bus->max_tx_flowrings;
			bus->api.handle_mb_data = dhdpcie_read_handle_mb_data;
			bus->use_mailbox = sh->flags & PCIE_SHARED_USE_MAILBOX;
		}
		else {
			bus->max_tx_flowrings = ltoh16(ring_info.max_tx_flowrings);
			bus->max_submission_rings = bus->max_tx_flowrings;
			bus->max_completion_rings = BCMPCIE_D2H_COMMON_MSGRINGS;
			bus->max_cmn_rings = BCMPCIE_H2D_COMMON_MSGRINGS;
			bus->api.handle_mb_data = dhdpcie_handle_mb_data;
			bus->use_mailbox = TRUE;
		}
		if (bus->max_completion_rings == 0) {
			DHD_ERROR(("dongle completion rings are invalid %d\n",
				bus->max_completion_rings));
			ret = BCME_ERROR;
			goto done;
		}
		if (bus->max_submission_rings == 0) {
			DHD_ERROR(("dongle submission rings are invalid %d\n",
				bus->max_submission_rings));
			ret = BCME_ERROR;
			goto done;
		}
		if (bus->max_tx_flowrings == 0) {
			DHD_ERROR(("dongle txflow rings are invalid %d\n", bus->max_tx_flowrings));
			ret = BCME_ERROR;
			goto done;
		}

		/* If both FW and Host support DMA'ing indices, allocate memory and notify FW
		 * The max_sub_queues is read from FW initialized ring_info
		 */
		if (bus->dhd->dma_h2d_ring_upd_support || IDMA_ENAB(bus->dhd)) {
			dma_indx_wr_buf = dhd_prot_dma_indx_init(bus->dhd, bus->rw_index_sz,
				H2D_DMA_INDX_WR_BUF, bus->max_submission_rings);
			dma_indx_rd_buf = dhd_prot_dma_indx_init(bus->dhd, bus->rw_index_sz,
				D2H_DMA_INDX_RD_BUF, bus->max_completion_rings);

			if ((dma_indx_wr_buf != BCME_OK) || (dma_indx_rd_buf != BCME_OK)) {
				DHD_ERROR(("%s: Failed to allocate memory for dma'ing h2d indices"
						"Host will use w/r indices in TCM\n",
						__FUNCTION__));
				bus->dhd->dma_h2d_ring_upd_support = FALSE;
				bus->dhd->idma_enable = FALSE;
			}
		}

		if (bus->dhd->dma_d2h_ring_upd_support) {
			dma_indx_wr_buf = dhd_prot_dma_indx_init(bus->dhd, bus->rw_index_sz,
				D2H_DMA_INDX_WR_BUF, bus->max_completion_rings);
			dma_indx_rd_buf = dhd_prot_dma_indx_init(bus->dhd, bus->rw_index_sz,
				H2D_DMA_INDX_RD_BUF, bus->max_submission_rings);

			if ((dma_indx_wr_buf != BCME_OK) || (dma_indx_rd_buf != BCME_OK)) {
				DHD_ERROR(("%s: Failed to allocate memory for dma'ing d2h indices"
						"Host will use w/r indices in TCM\n",
						__FUNCTION__));
				bus->dhd->dma_d2h_ring_upd_support = FALSE;
			}
		}
#ifdef DHD_DMA_INDICES_SEQNUM
		if (bus->dhd->dma_d2h_ring_upd_support) {
			uint32 bufsz = bus->rw_index_sz * bus->max_completion_rings;
			if (dhd_prot_dma_indx_copybuf_init(bus->dhd, bufsz, D2H_DMA_INDX_WR_BUF)
				!= BCME_OK) {
				ret = BCME_NOMEM;
				goto done;
			}
			bufsz = bus->rw_index_sz * bus->max_submission_rings;
			if (dhd_prot_dma_indx_copybuf_init(bus->dhd, bufsz, H2D_DMA_INDX_RD_BUF)
				!= BCME_OK) {
				ret = BCME_NOMEM;
				goto done;
			}
		}
#endif /* DHD_DMA_INDICES_SEQNUM */
		if (IFRM_ENAB(bus->dhd)) {
			dma_indx_wr_buf = dhd_prot_dma_indx_init(bus->dhd, bus->rw_index_sz,
				H2D_IFRM_INDX_WR_BUF, bus->max_tx_flowrings);

			if (dma_indx_wr_buf != BCME_OK) {
				DHD_ERROR(("%s: Failed to alloc memory for Implicit DMA\n",
						__FUNCTION__));
				bus->dhd->ifrm_enable = FALSE;
			}
		}

		/* read ringmem and ringstate ptrs from shared area and store in host variables */
		dhd_fillup_ring_sharedptr_info(bus, &ring_info);
		if (dhd_msg_level & DHD_INFO_VAL) {
			bcm_print_bytes("ring_info_raw", (uchar *)&ring_info, sizeof(ring_info_t));
		}
		DHD_INFO(("%s: ring_info\n", __FUNCTION__));

		DHD_ERROR(("%s: max H2D queues %d\n",
			__FUNCTION__, ltoh16(ring_info.max_tx_flowrings)));

		DHD_INFO(("mail box address\n"));
		DHD_INFO(("%s: h2d_mb_data_ptr_addr 0x%04x\n",
			__FUNCTION__, bus->h2d_mb_data_ptr_addr));
		DHD_INFO(("%s: d2h_mb_data_ptr_addr 0x%04x\n",
			__FUNCTION__, bus->d2h_mb_data_ptr_addr));
	}

	DHD_INFO(("%s: d2h_sync_mode 0x%08x\n",
		__FUNCTION__, bus->dhd->d2h_sync_mode));

	bus->dhd->d2h_hostrdy_supported =
		((sh->flags & PCIE_SHARED_HOSTRDY_SUPPORT) == PCIE_SHARED_HOSTRDY_SUPPORT);

	bus->dhd->ext_trap_data_supported =
		((sh->flags2 & PCIE_SHARED2_EXTENDED_TRAP_DATA) == PCIE_SHARED2_EXTENDED_TRAP_DATA);

	if ((sh->flags2 & PCIE_SHARED2_TXSTATUS_METADATA) == 0)
		bus->dhd->pcie_txs_metadata_enable = 0;

	if (sh->flags2 & PCIE_SHARED2_TRAP_ON_HOST_DB7)  {
		memset(&bus->dhd->db7_trap, 0, sizeof(bus->dhd->db7_trap));
		bus->dhd->db7_trap.fw_db7w_trap = 1;
		/* add an option to let the user select ?? */
		bus->dhd->db7_trap.db7_magic_number = PCIE_DB7_MAGIC_NUMBER_DPC_TRAP;
	}
	if (sh->flags2 & PCIE_SHARED2_LPM_SUPPORT) {
		bus->dhd->fw_lpm_support = TRUE;
		DHD_ERROR(("FW supports LPM mode\n"));
	}

#ifdef D2H_MINIDUMP
	bus->d2h_minidump = (sh->flags2 & PCIE_SHARED2_FW_SMALL_MEMDUMP) ? TRUE : FALSE;
	DHD_ERROR(("FW supports minidump ? %s \n", bus->d2h_minidump ? "Y" : "N"));
	if (bus->d2h_minidump_override) {
		bus->d2h_minidump = FALSE;
	}
	DHD_ERROR(("d2h_minidump: %d d2h_minidump_override: %d\n",
		bus->d2h_minidump, bus->d2h_minidump_override));
#endif /* D2H_MINIDUMP */

	bus->dhd->hscb_enable =
		(sh->flags2 & PCIE_SHARED2_HSCB) == PCIE_SHARED2_HSCB;

#ifdef EWP_EDL
	if (host_edl_support) {
		bus->dhd->dongle_edl_support = (sh->flags2 & PCIE_SHARED2_EDL_RING) ? TRUE : FALSE;
		DHD_ERROR(("Dongle EDL support: %u\n", bus->dhd->dongle_edl_support));
	}
#endif /* EWP_EDL */

	bus->dhd->debug_buf_dest_support =
		(sh->flags2 & PCIE_SHARED2_DEBUG_BUF_DEST) ? TRUE : FALSE;
	DHD_ERROR_MEM(("FW supports debug buf dest ? %s \n",
		bus->dhd->debug_buf_dest_support ? "Y" : "N"));

	bus->dhd->rx_cpl_lat_capable =
		(sh->flags2 & PCIE_SHARED2_RX_CMPL_PRIO_VALID) ? TRUE : FALSE;

	bus->dhd->mdring_capable =
		(sh->flags2 & PCIE_SHARED2_METADATA_RING) ? TRUE : FALSE;

	DHD_ERROR(("FW supports MD ring ? %s\n",
			bus->dhd->mdring_capable ? "Y" : "N"));

	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req_clear(bus);

		/*
		 * WAR to fix ARM cold boot;
		 * De-assert WL domain in DAR
		 */
		if (bus->sih->buscorerev >= 68) {
			dhd_bus_pcie_pwr_req_wl_domain(bus,
				DAR_PCIE_PWR_CTRL((bus->sih)->buscorerev), FALSE);
		}
	}
done:
#if defined(FW_SIGNATURE)
	if ((ret == BCME_ERROR) && (bus->fwsig_filename[0] != 0)) {
		bl_verif_status_t status;

		(void)dhdpcie_read_fwstatus(bus, &status);
		DHD_ERROR(("Verification status: (%08x)\n"
			"\tstatus: %d\n"
			"\tstate: %u\n"
			"\talloc_bytes: %u\n"
			"\tmax_alloc_bytes: %u\n"
			"\ttotal_alloc_bytes: %u\n"
			"\ttotal_freed_bytes: %u\n"
			"\tnum_allocs: %u\n"
			"\tmax_allocs: %u\n"
			"\tmax_alloc_size: %u\n"
			"\talloc_failures: %u\n",
			bus->fwstat_download_addr,
			status.status,
			status.state,
			status.alloc_bytes,
			status.max_alloc_bytes,
			status.total_alloc_bytes,
			status.total_freed_bytes,
			status.num_allocs,
			status.max_allocs,
			status.max_alloc_size,
			status.alloc_failures));
	}
#endif /* FW_SIGNATURE */
	return ret;
} /* dhdpcie_readshared */

/** Read ring mem and ring state ptr info from shared memory area in device memory */
static void
dhd_fillup_ring_sharedptr_info(dhd_bus_t *bus, ring_info_t *ring_info)
{
	uint16 i = 0;
	uint16 j = 0;
	uint32 tcm_memloc;
	uint32 d2h_w_idx_ptr, d2h_r_idx_ptr, h2d_w_idx_ptr, h2d_r_idx_ptr;
	uint16 max_tx_flowrings = bus->max_tx_flowrings;

	/* Ring mem ptr info */
	/* Alloated in the order
		H2D_MSGRING_CONTROL_SUBMIT              0
		H2D_MSGRING_RXPOST_SUBMIT               1
		D2H_MSGRING_CONTROL_COMPLETE            2
		D2H_MSGRING_TX_COMPLETE                 3
		D2H_MSGRING_RX_COMPLETE                 4
	*/

	{
		/* ringmemptr holds start of the mem block address space */
		tcm_memloc = ltoh32(ring_info->ringmem_ptr);

		/* Find out ringmem ptr for each ring common  ring */
		for (i = 0; i <= BCMPCIE_COMMON_MSGRING_MAX_ID; i++) {
			bus->ring_sh[i].ring_mem_addr = tcm_memloc;
			/* Update mem block */
			tcm_memloc = tcm_memloc + sizeof(ring_mem_t);
			DHD_INFO(("%s: ring id %d ring mem addr 0x%04x \n", __FUNCTION__,
				i, bus->ring_sh[i].ring_mem_addr));
		}
	}

	/* Ring state mem ptr info */
	{
		d2h_w_idx_ptr = ltoh32(ring_info->d2h_w_idx_ptr);
		d2h_r_idx_ptr = ltoh32(ring_info->d2h_r_idx_ptr);
		h2d_w_idx_ptr = ltoh32(ring_info->h2d_w_idx_ptr);
		h2d_r_idx_ptr = ltoh32(ring_info->h2d_r_idx_ptr);

		/* Store h2d common ring write/read pointers */
		for (i = 0; i < BCMPCIE_H2D_COMMON_MSGRINGS; i++) {
			bus->ring_sh[i].ring_state_w = h2d_w_idx_ptr;
			bus->ring_sh[i].ring_state_r = h2d_r_idx_ptr;

			/* update mem block */
			h2d_w_idx_ptr = h2d_w_idx_ptr + bus->rw_index_sz;
			h2d_r_idx_ptr = h2d_r_idx_ptr + bus->rw_index_sz;

			DHD_INFO(("%s: h2d w/r : idx %d write %x read %x \n", __FUNCTION__, i,
				bus->ring_sh[i].ring_state_w, bus->ring_sh[i].ring_state_r));
		}

		/* Store d2h common ring write/read pointers */
		for (j = 0; j < BCMPCIE_D2H_COMMON_MSGRINGS; j++, i++) {
			bus->ring_sh[i].ring_state_w = d2h_w_idx_ptr;
			bus->ring_sh[i].ring_state_r = d2h_r_idx_ptr;

			/* update mem block */
			d2h_w_idx_ptr = d2h_w_idx_ptr + bus->rw_index_sz;
			d2h_r_idx_ptr = d2h_r_idx_ptr + bus->rw_index_sz;

			DHD_INFO(("%s: d2h w/r : idx %d write %x read %x \n", __FUNCTION__, i,
				bus->ring_sh[i].ring_state_w, bus->ring_sh[i].ring_state_r));
		}

		/* Store txflow ring write/read pointers */
		if (bus->api.fw_rev < PCIE_SHARED_VERSION_6) {
			max_tx_flowrings -= BCMPCIE_H2D_COMMON_MSGRINGS;
		} else {
			/* Account for Debug info h2d ring located after the last tx flow ring */
			max_tx_flowrings = max_tx_flowrings + 1;
		}
		for (j = 0; j < max_tx_flowrings; i++, j++)
		{
			bus->ring_sh[i].ring_state_w = h2d_w_idx_ptr;
			bus->ring_sh[i].ring_state_r = h2d_r_idx_ptr;

			/* update mem block */
			h2d_w_idx_ptr = h2d_w_idx_ptr + bus->rw_index_sz;
			h2d_r_idx_ptr = h2d_r_idx_ptr + bus->rw_index_sz;

			DHD_INFO(("%s: FLOW Rings h2d w/r : idx %d write %x read %x \n",
				__FUNCTION__, i,
				bus->ring_sh[i].ring_state_w,
				bus->ring_sh[i].ring_state_r));
		}
		/* store wr/rd pointers for debug info completion or EDL ring */
		bus->ring_sh[i].ring_state_w = d2h_w_idx_ptr;
		bus->ring_sh[i].ring_state_r = d2h_r_idx_ptr;
		d2h_w_idx_ptr = d2h_w_idx_ptr + bus->rw_index_sz;
		d2h_r_idx_ptr = d2h_r_idx_ptr + bus->rw_index_sz;
		DHD_INFO(("d2h w/r : idx %d write %x read %x \n", i,
			bus->ring_sh[i].ring_state_w,
			bus->ring_sh[i].ring_state_r));
	}
} /* dhd_fillup_ring_sharedptr_info */

/**
 * Initialize bus module: prepare for communication with the dongle. Called after downloading
 * firmware into the dongle.
 */
int dhd_bus_init(dhd_pub_t *dhdp, bool enforce_mutex)
{
	dhd_bus_t *bus = dhdp->bus;
	int  ret = 0;

	DHD_TRACE(("%s: Enter\n", __FUNCTION__));

	ASSERT(bus->dhd);
	if (!bus->dhd)
		return 0;

	bus->link_state = DHD_PCIE_ALL_GOOD;

	dhd_bus_pcie_pwr_req_clear_reload_war(bus);

	dhd_init_dpc_histos(bus->dhd);

	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req(bus);
	}

	/* Configure AER registers to log the TLP header */
	dhd_bus_aer_config(bus);

	/* Make sure we're talking to the core. */
	bus->reg = si_setcore(bus->sih, PCIE2_CORE_ID, 0);
	ASSERT(bus->reg != NULL);

	/* before opening up bus for data transfer, check if shared are is intact */
	ret = dhdpcie_readshared(bus);
	if (ret < 0) {
		DHD_ERROR(("%s :Shared area read failed \n", __FUNCTION__));
		dhdpcie_force_console_dump(bus);
		goto exit;
	}

	/* Make sure we're talking to the core. */
	bus->reg = si_setcore(bus->sih, PCIE2_CORE_ID, 0);
	ASSERT(bus->reg != NULL);

	/* Set bus state according to enable result */
	dhdp->busstate = DHD_BUS_DATA;
	DHD_SET_BUS_NOT_IN_LPS(bus);
	dhdp->dhd_bus_busy_state = 0;

	/* D11 status via PCIe completion header */
	if ((ret = dhdpcie_init_d11status(bus)) < 0) {
		goto exit;
	}

#if defined(OEM_ANDROID) || defined(LINUX)
	if (!dhd_download_fw_on_driverload)
		dhd_dpc_enable(bus->dhd);
#endif /* OEM_ANDROID || LINUX */
	/* Enable the interrupt after device is up */
	dhdpcie_bus_intr_enable(bus);

	DHD_ERROR(("%s: Enabling bus->intr_enabled\n", __FUNCTION__));
	bus->intr_enabled = TRUE;

	/* XXX These need to change w/API updates */
	/* bcmsdh_intr_unmask(bus->sdh); */
	bus->idletime = 0;
#ifdef PCIE_INB_DW
	bus->skip_ds_ack = FALSE;
	/* Initialize the lock to serialize Device Wake Inband activities */
	if (!bus->inb_lock) {
		bus->inb_lock = osl_spin_lock_init(bus->dhd->osh);
	}
#endif

	/* XXX Temp errnum workaround: return ok, caller checks bus state */

	/* Make use_d0_inform TRUE for Rev 5 for backward compatibility */
	if (bus->api.fw_rev < PCIE_SHARED_VERSION_6) {
		bus->use_d0_inform = TRUE;
	} else {
		bus->use_d0_inform = FALSE;
	}
	bus->lpm_mode = FALSE;
	bus->lpm_mem_kill = FALSE;

	bus->hostready_count = 0;

	bus->host_irq_enable_count = 0;
	bus->host_irq_disable_count = 0;
	bus->dngl_intmask_disable_count = 0;
	bus->dngl_intmask_enable_count = 0;
	if (dhdp->rx_cpl_lat_capable) {
		bzero(&dhdp->rxcpl_lat_info, sizeof(rx_cpl_lat_info_t));
	}
exit:
	if (MULTIBP_ENAB(bus->sih)) {
		dhd_bus_pcie_pwr_req_clear(bus);
	}
	return ret;
}

static void
dhdpcie_init_shared_addr(dhd_bus_t *bus)
{
	uint32 addr = 0;
	uint32 val = 0;
	addr = bus->dongle_ram_base + bus->ramsize - 4;
	dhdpcie_bus_membytes(bus, TRUE, addr, (uint8 *)&val, sizeof(val));
}

bool
dhdpcie_chipmatch(uint16 vendor, uint16 device)
{
	if ((vendor != PCI_VENDOR_ID_BROADCOM) && (vendor != PCI_VENDOR_ID_SYNAPTICS))  {
		DHD_ERROR(("%s: Unsupported vendor %x device %x\n", __FUNCTION__,
			vendor, device));
		return (-ENODEV);
	}

	switch (device) {
		case BCM4345_CHIP_ID:
		case BCM43454_CHIP_ID:
		case BCM43455_CHIP_ID:
		case BCM43457_CHIP_ID:
		case BCM43458_CHIP_ID:
		case BCM4350_D11AC_ID:
		case BCM4350_D11AC2G_ID:
		case BCM4350_D11AC5G_ID:
		case BCM4350_CHIP_ID:
		case BCM4354_D11AC_ID:
		case BCM4354_D11AC2G_ID:
		case BCM4354_D11AC5G_ID:
		case BCM4354_CHIP_ID:
		case BCM4356_D11AC_ID:
		case BCM4356_D11AC2G_ID:
		case BCM4356_D11AC5G_ID:
		case BCM4356_CHIP_ID:
		case BCM4371_D11AC_ID:
		case BCM4371_D11AC2G_ID:
		case BCM4371_D11AC5G_ID:
		case BCM4371_CHIP_ID:
		case BCM4345_D11AC_ID:
		case BCM4345_D11AC2G_ID:
		case BCM4345_D11AC5G_ID:
		case BCM43452_D11AC_ID:
		case BCM43452_D11AC2G_ID:
		case BCM43452_D11AC5G_ID:
		case BCM4335_D11AC_ID:
		case BCM4335_D11AC2G_ID:
		case BCM4335_D11AC5G_ID:
		case BCM4335_CHIP_ID:
		case BCM43602_D11AC_ID:
		case BCM43602_D11AC2G_ID:
		case BCM43602_D11AC5G_ID:
		case BCM43602_CHIP_ID:
		case BCM43569_D11AC_ID:
		case BCM43569_D11AC2G_ID:
		case BCM43569_D11AC5G_ID:
		case BCM43569_CHIP_ID:
		/* XXX: For 4358, BCM4358_CHIP_ID is not checked intentionally as
		 * this is not a real chip id, but propagated from the OTP.
		 */
		case BCM4358_D11AC_ID:
		case BCM4358_D11AC2G_ID:
		case BCM4358_D11AC5G_ID:
		case BCM4349_D11AC_ID:
		case BCM4349_D11AC2G_ID:
		case BCM4349_D11AC5G_ID:
		case BCM4355_D11AC_ID:
		case BCM4355_D11AC2G_ID:
		case BCM4355_D11AC5G_ID:
		case BCM4355_CHIP_ID:
		/* XXX: BCM4359_CHIP_ID is not checked intentionally as this is
		 * not a real chip id, but propogated from the OTP.
		 */
		case BCM4359_D11AC_ID:
		case BCM4359_D11AC2G_ID:
		case BCM4359_D11AC5G_ID:
		case BCM43596_D11AC_ID:
		case BCM43596_D11AC2G_ID:
		case BCM43596_D11AC5G_ID:
		case BCM43597_D11AC_ID:
		case BCM43597_D11AC2G_ID:
		case BCM43597_D11AC5G_ID:
		case BCM4364_D11AC_ID:
		case BCM4364_D11AC2G_ID:
		case BCM4364_D11AC5G_ID:
		case BCM4364_CHIP_ID:
		case BCM4361_D11AC_ID:
		case BCM4361_D11AC2G_ID:
		case BCM4361_D11AC5G_ID:
		case BCM4361_CHIP_ID:
		case BCM4347_D11AC_ID:
		case BCM4347_D11AC2G_ID:
		case BCM4347_D11AC5G_ID:
		case BCM4347_CHIP_ID:
		case BCM4369_D11AX_ID:
		case BCM4369_D11AX2G_ID:
		case BCM4369_D11AX5G_ID:
		case BCM4369_CHIP_ID:
		case BCM4376_D11AX_ID:
		case BCM4376_D11AX2G_ID:
		case BCM4376_D11AX5G_ID:
		case BCM4376_CHIP_ID:
		case BCM4377_M_D11AX_ID:
		case BCM4377_D11AX_ID:
		case BCM4377_D11AX2G_ID:
		case BCM4377_D11AX5G_ID:
		case BCM4377_CHIP_ID:
		case BCM4378_D11AX_ID:
		case BCM4378_D11AX2G_ID:
		case BCM4378_D11AX5G_ID:
		case BCM4378_CHIP_ID:
		case BCM4387_D11AX_ID:
		case BCM4387_CHIP_ID:
		case BCM4362_D11AX_ID:
		case BCM4362_D11AX2G_ID:
		case BCM4362_D11AX5G_ID:
		case BCM4362_CHIP_ID:
		case BCM4375_D11AX_ID:
		case BCM4375_D11AX2G_ID:
		case BCM4375_D11AX5G_ID:
		case BCM4375_CHIP_ID:
		case BCM43751_D11AX_ID:
		case BCM43751_D11AX2G_ID:
		case BCM43751_D11AX5G_ID:
		case BCM43751_CHIP_ID:
		case BCM43752_D11AX_ID:
		case BCM43752_D11AX2G_ID:
		case BCM43752_D11AX5G_ID:
		case BCM43752_CHIP_ID:
		case BCM43756_D11AX_ID:
		case BCM43756_CHIP_ID:
		case BCM43756E_D11AX6E_ID:
		case BCM43756E_D11AC_ID:
		case BCM43756E_D11AX_ID:
		case BCM43711_CHIP_ID:
		case BCM43711_D11AX6E_ID:
		case BCM43711_D11AC_ID:
		case BCM43711_D11AX_ID:
		case BCM4381_CHIP_ID:
		case BCM4381_D11AX_ID:
		case BCM4382_CHIP_ID:
		case BCM4382_D11AX_ID:
		case BCM4388_CHIP_ID:
		case BCM4388_D11AX_ID:
		case BCM4389_CHIP_ID:
		case BCM4389_D11AX_ID:
		case BCM4385_D11AX_ID:
		case BCM4385_CHIP_ID:

			return 0;
		default:
			DHD_ERROR(("%s: Unsupported vendor %x device %x\n",
				__FUNCTION__, vendor, device));
			return (-ENODEV);
	}
} /* dhdpcie_chipmatch */

/**
 * Name:  dhdpcie_cc_nvmshadow
 *
 * Description:
 * A shadow of OTP/SPROM exists in ChipCommon Region
 * betw. 0x800 and 0xBFF (Backplane Addr. 0x1800_0800 and 0x1800_0BFF).
 * Strapping option (SPROM vs. OTP), presence of OTP/SPROM and its size
 * can also be read from ChipCommon Registers.
 */
/* XXX So far tested with 4345 and 4350 (Hence the checks in the function.) */
static int
dhdpcie_cc_nvmshadow(dhd_bus_t *bus, struct bcmstrbuf *b)
{
	uint16 dump_offset = 0;
	uint32 dump_size = 0, otp_size = 0, sprom_size = 0;

	/* Table for 65nm OTP Size (in bits) */
	int  otp_size_65nm[8] = {0, 2048, 4096, 8192, 4096, 6144, 512, 1024};

	volatile uint16 *nvm_shadow;

	uint cur_coreid;
	uint chipc_corerev;
	chipcregs_t *chipcregs;

	/* Save the current core */
	cur_coreid = si_coreid(bus->sih);
	/* Switch to ChipC */
	chipcregs = (chipcregs_t *)si_setcore(bus->sih, CC_CORE_ID, 0);
	ASSERT(chipcregs != NULL);

	chipc_corerev = si_corerev(bus->sih);

	/* Check ChipcommonCore Rev */
	if (chipc_corerev < 44) {
		DHD_ERROR(("%s: ChipcommonCore Rev %d < 44\n", __FUNCTION__, chipc_corerev));
		return BCME_UNSUPPORTED;
	}

	/* Check ChipID */
	if (((uint16)bus->sih->chip != BCM4350_CHIP_ID) && !BCM4345_CHIP((uint16)bus->sih->chip) &&
	        ((uint16)bus->sih->chip != BCM4355_CHIP_ID) &&
	        ((uint16)bus->sih->chip != BCM4364_CHIP_ID)) {
		DHD_ERROR(("%s: cc_nvmdump not supported\n", __FUNCTION__));
		return BCME_UNSUPPORTED;
	}

	/* Check if SRC_PRESENT in SpromCtrl(0x190 in ChipCommon Regs) is set */
	if (chipcregs->sromcontrol & SRC_PRESENT) {
		/* SPROM Size: 1Kbits (0x0), 4Kbits (0x1), 16Kbits(0x2) */
		sprom_size = (1 << (2 * ((chipcregs->sromcontrol & SRC_SIZE_MASK)
					>> SRC_SIZE_SHIFT))) * 1024;
		bcm_bprintf(b, "\nSPROM Present (Size %d bits)\n", sprom_size);
	}

	/* XXX Check if OTP exists. 2 possible approaches:
	 * 1) Check if OtpPresent in SpromCtrl (0x190 in ChipCommon Regs) is set OR
	 * 2) Check if OtpSize > 0
	 */
	if (chipcregs->sromcontrol & SRC_OTPPRESENT) {
		bcm_bprintf(b, "\nOTP Present");

		if (((chipcregs->otplayout & OTPL_WRAP_TYPE_MASK) >> OTPL_WRAP_TYPE_SHIFT)
			== OTPL_WRAP_TYPE_40NM) {
			/* 40nm OTP: Size = (OtpSize + 1) * 1024 bits */
			/* Chipcommon rev51 is a variation on rev45 and does not support
			 * the latest OTP configuration.
			 */
			if (chipc_corerev != 51 && chipc_corerev >= 49) {
				otp_size = (((chipcregs->otplayout & OTPL_ROW_SIZE_MASK)
					>> OTPL_ROW_SIZE_SHIFT) + 1) * 1024;
				bcm_bprintf(b, "(Size %d bits)\n", otp_size);
			} else {
				otp_size =  (((chipcregs->capabilities & CC_CAP_OTPSIZE)
				        >> CC_CAP_OTPSIZE_SHIFT) + 1) * 1024;
				bcm_bprintf(b, "(Size %d bits)\n", otp_size);
			}
		} else {
			/* This part is untested since newer chips have 40nm OTP */
			/* Chipcommon rev51 is a variation on rev45 and does not support
			 * the latest OTP configuration.
			 */
			if (chipc_corerev != 51 && chipc_corerev >= 49) {
				otp_size = otp_size_65nm[(chipcregs->otplayout & OTPL_ROW_SIZE_MASK)
						>> OTPL_ROW_SIZE_SHIFT];
				bcm_bprintf(b, "(Size %d bits)\n", otp_size);
			} else {
				otp_size = otp_size_65nm[(chipcregs->capabilities & CC_CAP_OTPSIZE)
					        >> CC_CAP_OTPSIZE_SHIFT];
				bcm_bprintf(b, "(Size %d bits)\n", otp_size);
				DHD_INFO(("%s: 65nm/130nm OTP Size not tested. \n",
					__FUNCTION__));
			}
		}
	}

	/* Chipcommon rev51 is a variation on rev45 and does not support
	 * the latest OTP configuration.
	 */
	if (chipc_corerev != 51 && chipc_corerev >= 49) {
		if (((chipcregs->sromcontrol & SRC_PRESENT) == 0) &&
			((chipcregs->otplayout & OTPL_ROW_SIZE_MASK) == 0)) {
			DHD_ERROR(("%s: SPROM and OTP could not be found "
				"sromcontrol = %x, otplayout = %x \n",
				__FUNCTION__, chipcregs->sromcontrol, chipcregs->otplayout));
			return BCME_NOTFOUND;
		}
	} else {
		if (((chipcregs->sromcontrol & SRC_PRESENT) == 0) &&
			((chipcregs->capabilities & CC_CAP_OTPSIZE) == 0)) {
			DHD_ERROR(("%s: SPROM and OTP could not be found "
				"sromcontrol = %x, capablities = %x \n",
				__FUNCTION__, chipcregs->sromcontrol, chipcregs->capabilities));
			return BCME_NOTFOUND;
		}
	}

	/* Check the strapping option in SpromCtrl: Set = OTP otherwise SPROM */
	if ((!(chipcregs->sromcontrol & SRC_PRESENT) || (chipcregs->sromcontrol & SRC_OTPSEL)) &&
		(chipcregs->sromcontrol & SRC_OTPPRESENT)) {

		bcm_bprintf(b, "OTP Strap selected.\n"
		               "\nOTP Shadow in ChipCommon:\n");

		dump_size = otp_size / 16 ; /* 16bit words */

	} else if (((chipcregs->sromcontrol & SRC_OTPSEL) == 0) &&
		(chipcregs->sromcontrol & SRC_PRESENT)) {

		bcm_bprintf(b, "SPROM Strap selected\n"
				"\nSPROM Shadow in ChipCommon:\n");

		/* If SPROM > 8K only 8Kbits is mapped to ChipCommon (0x800 - 0xBFF) */
		/* dump_size in 16bit words */
		dump_size = sprom_size > 8 ? (8 * 1024) / 16 : sprom_size / 16;
	} else {
		DHD_ERROR(("%s: NVM Shadow does not exist in ChipCommon\n",
			__FUNCTION__));
		return BCME_NOTFOUND;
	}

	if (bus->regs == NULL) {
		DHD_ERROR(("ChipCommon Regs. not initialized\n"));
		return BCME_NOTREADY;
	} else {
		bcm_bprintf(b, "\n OffSet:");

		/* Chipcommon rev51 is a variation on rev45 and does not support
		 * the latest OTP configuration.
		 */
		if (chipc_corerev != 51 && chipc_corerev >= 49) {
			/* Chip common can read only 8kbits,
			* for ccrev >= 49 otp size is around 12 kbits so use GCI core
			*/
			nvm_shadow = (volatile uint16 *)si_setcore(bus->sih, GCI_CORE_ID, 0);
		} else {
			/* Point to the SPROM/OTP shadow in ChipCommon */
			nvm_shadow = chipcregs->sromotp;
		}

		if (nvm_shadow == NULL) {
			DHD_ERROR(("%s: NVM Shadow is not intialized\n", __FUNCTION__));
			return BCME_NOTFOUND;
		}

		/*
		* Read 16 bits / iteration.
		* dump_size & dump_offset in 16-bit words
		*/
		while (dump_offset < dump_size) {
			if (dump_offset % 2 == 0)
				/* Print the offset in the shadow space in Bytes */
				bcm_bprintf(b, "\n 0x%04x", dump_offset * 2);

			bcm_bprintf(b, "\t0x%04x", *(nvm_shadow + dump_offset));
			dump_offset += 0x1;
		}
	}

	/* Switch back to the original core */
	si_setcore(bus->sih, cur_coreid, 0);

	return BCME_OK;
} /* dhdpcie_cc_nvmshadow */

/** Flow rings are dynamically created and destroyed */
void dhd_bus_clean_flow_ring(dhd_bus_t *bus, void *node)
{
	void *pkt;
	flow_queue_t *queue;
	flow_ring_node_t *flow_ring_node = (flow_ring_node_t *)node;
	unsigned long flags;

	queue = &flow_ring_node->queue;

#ifdef DHDTCPACK_SUPPRESS
	/* Clean tcp_ack_info_tbl in order to prevent access to flushed pkt,
	 * when there is a newly coming packet from network stack.
	 */
	dhd_tcpack_info_tbl_clean(bus->dhd);
#endif /* DHDTCPACK_SUPPRESS */

	/* clean up BUS level info */
	DHD_FLOWRING_LOCK(flow_ring_node->lock, flags);

	/* Flush all pending packets in the queue, if any */
	while ((pkt = dhd_flow_queue_dequeue(bus->dhd, queue)) != NULL) {
		PKTFREE(bus->dhd->osh, pkt, TRUE);
	}
	ASSERT(DHD_FLOW_QUEUE_EMPTY(queue));

	/* Reinitialise flowring's queue */
	dhd_flow_queue_reinit(bus->dhd, queue, bus->dhd->conf->flow_ring_queue_threshold);
	flow_ring_node->status = FLOW_RING_STATUS_CLOSED;
	flow_ring_node->active = FALSE;

	DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);

	/* Hold flowring_list_lock to ensure no race condition while accessing the List */
	DHD_FLOWRING_LIST_LOCK(bus->dhd->flowring_list_lock, flags);
	dll_delete(&flow_ring_node->list);
	dll_init(&flow_ring_node->list);
	DHD_FLOWRING_LIST_UNLOCK(bus->dhd->flowring_list_lock, flags);

	/* Release the flowring object back into the pool */
	dhd_prot_flowrings_pool_release(bus->dhd,
		flow_ring_node->flowid, flow_ring_node->prot_info);

	/* Free the flowid back to the flowid allocator */
	dhd_flowid_free(bus->dhd, flow_ring_node->flow_info.ifindex,
	                flow_ring_node->flowid);
}

/**
 * Allocate a Flow ring buffer,
 * Init Ring buffer, send Msg to device about flow ring creation
*/
int
dhd_bus_flow_ring_create_request(dhd_bus_t *bus, void *arg)
{
	flow_ring_node_t *flow_ring_node = (flow_ring_node_t *)arg;

	DHD_INFO(("%s :Flow create\n", __FUNCTION__));

	/* Send Msg to device about flow ring creation */
	if (dhd_prot_flow_ring_create(bus->dhd, flow_ring_node) != BCME_OK)
		return BCME_NOMEM;

	return BCME_OK;
}

/** Handle response from dongle on a 'flow ring create' request */
void
dhd_bus_flow_ring_create_response(dhd_bus_t *bus, uint16 flowid, int32 status)
{
	flow_ring_node_t *flow_ring_node;
	unsigned long flags;

	DHD_INFO(("%s :Flow Response %d \n", __FUNCTION__, flowid));

	/* Boundary check of the flowid */
	if (flowid > bus->dhd->max_tx_flowid) {
		DHD_ERROR(("%s: flowid is invalid %d, max id %d\n", __FUNCTION__,
			flowid, bus->dhd->max_tx_flowid));
		return;
	}

	flow_ring_node = DHD_FLOW_RING(bus->dhd, flowid);
	if (!flow_ring_node) {
		DHD_ERROR(("%s: flow_ring_node is NULL\n", __FUNCTION__));
		return;
	}

	ASSERT(flow_ring_node->flowid == flowid);
	if (flow_ring_node->flowid != flowid) {
		DHD_ERROR(("%s: flowid %d is different from the flowid "
			"of the flow_ring_node %d\n", __FUNCTION__, flowid,
			flow_ring_node->flowid));
		return;
	}

	if (status != BCME_OK) {
		DHD_ERROR(("%s Flow create Response failure error status = %d \n",
		     __FUNCTION__, status));
		/* Call Flow clean up */
		dhd_bus_clean_flow_ring(bus, flow_ring_node);
		return;
	}

	DHD_FLOWRING_LOCK(flow_ring_node->lock, flags);
	flow_ring_node->status = FLOW_RING_STATUS_OPEN;
	DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);

	/* Now add the Flow ring node into the active list
	 * Note that this code to add the newly created node to the active
	 * list was living in dhd_flowid_lookup. But note that after
	 * adding the node to the active list the contents of node is being
	 * filled in dhd_prot_flow_ring_create.
	 * If there is a D2H interrupt after the node gets added to the
	 * active list and before the node gets populated with values
	 * from the Bottom half dhd_update_txflowrings would be called.
	 * which will then try to walk through the active flow ring list,
	 * pickup the nodes and operate on them. Now note that since
	 * the function dhd_prot_flow_ring_create is not finished yet
	 * the contents of flow_ring_node can still be NULL leading to
	 * crashes. Hence the flow_ring_node should be added to the
	 * active list only after its truely created, which is after
	 * receiving the create response message from the Host.
	 */
	DHD_FLOWRING_LIST_LOCK(bus->dhd->flowring_list_lock, flags);
	dll_prepend(&bus->flowring_active_list, &flow_ring_node->list);
	DHD_FLOWRING_LIST_UNLOCK(bus->dhd->flowring_list_lock, flags);

	dhd_bus_schedule_queue(bus, flowid, FALSE, 0, NULL); /* from queue to flowring */

	return;
}

int
dhd_bus_flow_ring_delete_request(dhd_bus_t *bus, void *arg)
{
	void * pkt;
	flow_queue_t *queue;
	flow_ring_node_t *flow_ring_node;
	unsigned long flags;

	DHD_INFO(("%s :Flow Delete\n", __FUNCTION__));

	flow_ring_node = (flow_ring_node_t *)arg;

#ifdef DHDTCPACK_SUPPRESS
	/* Clean tcp_ack_info_tbl in order to prevent access to flushed pkt,
	 * when there is a newly coming packet from network stack.
	 */
	dhd_tcpack_info_tbl_clean(bus->dhd);
#endif /* DHDTCPACK_SUPPRESS */
	DHD_FLOWRING_LOCK(flow_ring_node->lock, flags);
	if (flow_ring_node->status == FLOW_RING_STATUS_DELETE_PENDING) {
		DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);
		DHD_ERROR(("%s :Delete Pending flowid %u\n", __FUNCTION__, flow_ring_node->flowid));
		return BCME_ERROR;
	}
	flow_ring_node->status = FLOW_RING_STATUS_DELETE_PENDING;

	queue = &flow_ring_node->queue; /* queue associated with flow ring */

	/* Flush all pending packets in the queue, if any */
	while ((pkt = dhd_flow_queue_dequeue(bus->dhd, queue)) != NULL) {
		PKTFREE(bus->dhd->osh, pkt, TRUE);
	}
	ASSERT(DHD_FLOW_QUEUE_EMPTY(queue));

	DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);

	/* Send Msg to device about flow ring deletion */
	dhd_prot_flow_ring_delete(bus->dhd, flow_ring_node);

	return BCME_OK;
}

void
dhd_bus_flow_ring_delete_response(dhd_bus_t *bus, uint16 flowid, uint32 status)
{
	flow_ring_node_t *flow_ring_node;

	DHD_INFO(("%s :Flow Delete Response %d \n", __FUNCTION__, flowid));

	/* Boundary check of the flowid */
	if (flowid > bus->dhd->max_tx_flowid) {
		DHD_ERROR(("%s: flowid is invalid %d, max id %d\n", __FUNCTION__,
			flowid, bus->dhd->max_tx_flowid));
		return;
	}

	flow_ring_node = DHD_FLOW_RING(bus->dhd, flowid);
	if (!flow_ring_node) {
		DHD_ERROR(("%s: flow_ring_node is NULL\n", __FUNCTION__));
		return;
	}

	ASSERT(flow_ring_node->flowid == flowid);
	if (flow_ring_node->flowid != flowid) {
		DHD_ERROR(("%s: flowid %d is different from the flowid "
			"of the flow_ring_node %d\n", __FUNCTION__, flowid,
			flow_ring_node->flowid));
		return;
	}

	if (status != BCME_OK) {
		DHD_ERROR(("%s Flow Delete Response failure error status = %d \n",
		    __FUNCTION__, status));
		return;
	}
	/* Call Flow clean up */
	dhd_bus_clean_flow_ring(bus, flow_ring_node);

	return;

}

int dhd_bus_flow_ring_flush_request(dhd_bus_t *bus, void *arg)
{
	void *pkt;
	flow_queue_t *queue;
	flow_ring_node_t *flow_ring_node;
	unsigned long flags;

	DHD_INFO(("%s :Flow Flush\n", __FUNCTION__));

	flow_ring_node = (flow_ring_node_t *)arg;

	DHD_FLOWRING_LOCK(flow_ring_node->lock, flags);
	queue = &flow_ring_node->queue; /* queue associated with flow ring */
	/* Flow ring status will be set back to FLOW_RING_STATUS_OPEN
	 * once flow ring flush response is received for this flowring node.
	 */
	flow_ring_node->status = FLOW_RING_STATUS_FLUSH_PENDING;

#ifdef DHDTCPACK_SUPPRESS
	/* Clean tcp_ack_info_tbl in order to prevent access to flushed pkt,
	 * when there is a newly coming packet from network stack.
	 */
	dhd_tcpack_info_tbl_clean(bus->dhd);
#endif /* DHDTCPACK_SUPPRESS */

	/* Flush all pending packets in the queue, if any */
	while ((pkt = dhd_flow_queue_dequeue(bus->dhd, queue)) != NULL) {
		PKTFREE(bus->dhd->osh, pkt, TRUE);
	}
	ASSERT(DHD_FLOW_QUEUE_EMPTY(queue));

	DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);

	/* Send Msg to device about flow ring flush */
	dhd_prot_flow_ring_flush(bus->dhd, flow_ring_node);

	return BCME_OK;
}

void
dhd_bus_flow_ring_flush_response(dhd_bus_t *bus, uint16 flowid, uint32 status)
{
	flow_ring_node_t *flow_ring_node;

	if (status != BCME_OK) {
		DHD_ERROR(("%s Flow flush Response failure error status = %d \n",
		    __FUNCTION__, status));
		return;
	}

	/* Boundary check of the flowid */
	if (flowid > bus->dhd->max_tx_flowid) {
		DHD_ERROR(("%s: flowid is invalid %d, max id %d\n", __FUNCTION__,
			flowid, bus->dhd->max_tx_flowid));
		return;
	}

	flow_ring_node = DHD_FLOW_RING(bus->dhd, flowid);
	if (!flow_ring_node) {
		DHD_ERROR(("%s: flow_ring_node is NULL\n", __FUNCTION__));
		return;
	}

	ASSERT(flow_ring_node->flowid == flowid);
	if (flow_ring_node->flowid != flowid) {
		DHD_ERROR(("%s: flowid %d is different from the flowid "
			"of the flow_ring_node %d\n", __FUNCTION__, flowid,
			flow_ring_node->flowid));
		return;
	}

	flow_ring_node->status = FLOW_RING_STATUS_OPEN;
	return;
}

uint32
dhd_bus_max_h2d_queues(struct dhd_bus *bus)
{
	return bus->max_submission_rings;
}

/* To be symmetric with SDIO */
void
dhd_bus_pktq_flush(dhd_pub_t *dhdp)
{
	return;
}

void
dhd_bus_set_linkdown(dhd_pub_t *dhdp, bool val)
{
	dhdp->bus->is_linkdown = val;
}

int
dhd_bus_get_linkdown(dhd_pub_t *dhdp)
{
	return dhdp->bus->is_linkdown;
}

int
dhd_bus_get_cto(dhd_pub_t *dhdp)
{
	return dhdp->bus->cto_triggered;
}

bool
dhd_bus_get_read_shm(dhd_pub_t *dhdp)
{
	return FALSE;
}

#ifdef IDLE_TX_FLOW_MGMT
/* resume request */
int
dhd_bus_flow_ring_resume_request(dhd_bus_t *bus, void *arg)
{
	flow_ring_node_t *flow_ring_node = (flow_ring_node_t *)arg;

	DHD_ERROR(("%s :Flow Resume Request flow id %u\n", __FUNCTION__, flow_ring_node->flowid));

	flow_ring_node->status = FLOW_RING_STATUS_RESUME_PENDING;

	/* Send Msg to device about flow ring resume */
	dhd_prot_flow_ring_resume(bus->dhd, flow_ring_node);

	return BCME_OK;
}

/* add the node back to active flowring */
void
dhd_bus_flow_ring_resume_response(dhd_bus_t *bus, uint16 flowid, int32 status)
{

	flow_ring_node_t *flow_ring_node;

	DHD_TRACE(("%s :flowid %d \n", __FUNCTION__, flowid));

	flow_ring_node = DHD_FLOW_RING(bus->dhd, flowid);
	ASSERT(flow_ring_node->flowid == flowid);

	if (status != BCME_OK) {
		DHD_ERROR(("%s Error Status = %d \n",
			__FUNCTION__, status));
		return;
	}

	DHD_TRACE(("%s :Number of pkts queued in FlowId:%d is -> %u!!\n",
		__FUNCTION__, flow_ring_node->flowid,  flow_ring_node->queue.len));

	flow_ring_node->status = FLOW_RING_STATUS_OPEN;

	dhd_bus_schedule_queue(bus, flowid, FALSE, 0, NULL);
	return;
}

/* scan the flow rings in active list for idle time out */
void
dhd_bus_check_idle_scan(dhd_bus_t *bus)
{
	uint64 time_stamp; /* in millisec */
	uint64 diff;

	time_stamp = OSL_SYSUPTIME();
	diff = time_stamp - bus->active_list_last_process_ts;

	if (diff > IDLE_FLOW_LIST_TIMEOUT) {
		dhd_bus_idle_scan(bus);
		bus->active_list_last_process_ts = OSL_SYSUPTIME();
	}

	return;
}

/* scan the nodes in active list till it finds a non idle node */
void
dhd_bus_idle_scan(dhd_bus_t *bus)
{
	dll_t *item, *prev;
	flow_ring_node_t *flow_ring_node;
	uint64 time_stamp, diff;
	unsigned long flags;
	uint16 ringid[MAX_SUSPEND_REQ];
	uint16 count = 0;

	time_stamp = OSL_SYSUPTIME();
	DHD_FLOWRING_LIST_LOCK(bus->dhd->flowring_list_lock, flags);

	for (item = dll_tail_p(&bus->flowring_active_list);
	         !dll_end(&bus->flowring_active_list, item); item = prev) {
		prev = dll_prev_p(item);

		flow_ring_node = dhd_constlist_to_flowring(item);

		if (flow_ring_node->flowid == (bus->max_submission_rings - 1))
			continue;

		if (flow_ring_node->status != FLOW_RING_STATUS_OPEN) {
			/* Takes care of deleting zombie rings */
			/* delete from the active list */
			DHD_INFO(("deleting flow id %u from active list\n",
				flow_ring_node->flowid));
			__dhd_flow_ring_delete_from_active_list(bus, flow_ring_node);
			continue;
		}

		diff = time_stamp - flow_ring_node->last_active_ts;

		if ((diff > IDLE_FLOW_RING_TIMEOUT) && !(flow_ring_node->queue.len))  {
			DHD_ERROR(("\nSuspending flowid %d\n", flow_ring_node->flowid));
			/* delete from the active list */
			__dhd_flow_ring_delete_from_active_list(bus, flow_ring_node);
			flow_ring_node->status = FLOW_RING_STATUS_SUSPENDED;
			ringid[count] = flow_ring_node->flowid;
			count++;
			if (count == MAX_SUSPEND_REQ) {
				/* create a batch message now!! */
				dhd_prot_flow_ring_batch_suspend_request(bus->dhd, ringid, count);
				count = 0;
			}

		} else {

			/* No more scanning, break from here! */
			break;
		}
	}

	if (count) {
		dhd_prot_flow_ring_batch_suspend_request(bus->dhd, ringid, count);
	}

	DHD_FLOWRING_LIST_UNLOCK(bus->dhd->flowring_list_lock, flags);

	return;
}

void dhd_flow_ring_move_to_active_list_head(struct dhd_bus *bus, flow_ring_node_t *flow_ring_node)
{
	unsigned long flags;
	dll_t* list;

	DHD_FLOWRING_LIST_LOCK(bus->dhd->flowring_list_lock, flags);
	/* check if the node is already at head, otherwise delete it and prepend */
	list = dll_head_p(&bus->flowring_active_list);
	if (&flow_ring_node->list != list) {
		dll_delete(&flow_ring_node->list);
		dll_prepend(&bus->flowring_active_list, &flow_ring_node->list);
	}

	/* update flow ring timestamp */
	flow_ring_node->last_active_ts = OSL_SYSUPTIME();

	DHD_FLOWRING_LIST_UNLOCK(bus->dhd->flowring_list_lock, flags);

	return;
}

void dhd_flow_ring_add_to_active_list(struct dhd_bus *bus, flow_ring_node_t *flow_ring_node)
{
	unsigned long flags;

	DHD_FLOWRING_LIST_LOCK(bus->dhd->flowring_list_lock, flags);

	dll_prepend(&bus->flowring_active_list, &flow_ring_node->list);
	/* update flow ring timestamp */
	flow_ring_node->last_active_ts = OSL_SYSUPTIME();

	DHD_FLOWRING_LIST_UNLOCK(bus->dhd->flowring_list_lock, flags);

	return;
}
void __dhd_flow_ring_delete_from_active_list(struct dhd_bus *bus, flow_ring_node_t *flow_ring_node)
{
	dll_delete(&flow_ring_node->list);
}

void dhd_flow_ring_delete_from_active_list(struct dhd_bus *bus, flow_ring_node_t *flow_ring_node)
{
	unsigned long flags;

	DHD_FLOWRING_LIST_LOCK(bus->dhd->flowring_list_lock, flags);

	__dhd_flow_ring_delete_from_active_list(bus, flow_ring_node);

	DHD_FLOWRING_LIST_UNLOCK(bus->dhd->flowring_list_lock, flags);

	return;
}
#endif /* IDLE_TX_FLOW_MGMT */

#if defined(LINUX) || defined(linux)
int
dhdpcie_bus_start_host_dev(struct dhd_bus *bus)
{
	return dhdpcie_start_host_dev(bus);
}

int
dhdpcie_bus_stop_host_dev(struct dhd_bus *bus)
{
	return dhdpcie_stop_host_dev(bus);
}

int
dhdpcie_bus_disable_device(struct dhd_bus *bus)
{
	return dhdpcie_disable_device(bus);
}

int
dhdpcie_bus_enable_device(struct dhd_bus *bus)
{
	return dhdpcie_enable_device(bus);
}

int
dhdpcie_bus_alloc_resource(struct dhd_bus *bus)
{
	return dhdpcie_alloc_resource(bus);
}

void
dhdpcie_bus_free_resource(struct dhd_bus *bus)
{
	dhdpcie_free_resource(bus);
}

int
dhd_bus_request_irq(struct dhd_bus *bus)
{
	return dhdpcie_bus_request_irq(bus);
}

bool
dhdpcie_bus_dongle_attach(struct dhd_bus *bus)
{
	return dhdpcie_dongle_attach(bus);
}

int
dhd_bus_release_dongle(struct dhd_bus *bus)
{
	bool dongle_isolation;
	osl_t *osh;

	DHD_TRACE(("%s: Enter\n", __FUNCTION__));

	if (bus) {
		osh = bus->osh;
		ASSERT(osh);

		if (bus->dhd) {
#if defined(DEBUGGER) || defined(DHD_DSCOPE)
			debugger_close();
#endif /* DEBUGGER || DHD_DSCOPE */

			dongle_isolation = bus->dhd->dongle_isolation;
			dhdpcie_bus_release_dongle(bus, osh, dongle_isolation, TRUE);
		}
	}

	return 0;
}
#endif /* LINUX || linux */

int
dhdpcie_cto_cfg_init(struct dhd_bus *bus, bool enable)
{
	if (enable) {
		dhdpcie_bus_cfg_write_dword(bus, PCI_INT_MASK, 4,
			PCI_CTO_INT_MASK | PCI_SBIM_MASK_SERR);
	} else {
		dhdpcie_bus_cfg_write_dword(bus, PCI_INT_MASK, 4, 0);
	}
	return 0;
}

int
dhdpcie_cto_init(struct dhd_bus *bus, bool enable)
{
	volatile void *regsva = (volatile void *)bus->regs;
	uint32 val;
	uint16 chipid = dhd_get_chipid(bus);
	uint32 ctoctrl;

	bus->cto_enable = enable;

	dhdpcie_cto_cfg_init(bus, enable);

	if (enable) {
		if (bus->cto_threshold == 0) {
			if ((chipid == BCM4387_CHIP_ID) ||
			    (chipid == BCM4388_CHIP_ID) ||
			    (chipid == BCM4389_CHIP_ID) ||
			    (chipid == BCM4397_CHIP_ID)) {
				bus->cto_threshold = PCIE_CTO_TO_THRESH_DEFAULT_REV69;
			} else {
				bus->cto_threshold = PCIE_CTO_TO_THRESH_DEFAULT;
			}
		}
		val = ((bus->cto_threshold << PCIE_CTO_TO_THRESHOLD_SHIFT) &
			PCIE_CTO_TO_THRESHHOLD_MASK) |
			((PCIE_CTO_CLKCHKCNT_VAL << PCIE_CTO_CLKCHKCNT_SHIFT) &
			PCIE_CTO_CLKCHKCNT_MASK) |
			PCIE_CTO_ENAB_MASK;

		pcie_corereg(bus->osh, regsva, OFFSETOF(sbpcieregs_t, ctoctrl), ~0, val);
	} else {
		pcie_corereg(bus->osh, regsva, OFFSETOF(sbpcieregs_t, ctoctrl), ~0, 0);
	}

	ctoctrl = pcie_corereg(bus->osh, regsva, OFFSETOF(sbpcieregs_t, ctoctrl), 0, 0);

	DHD_ERROR(("%s: ctoctrl(0x%x) enable/disable %d for chipid(0x%x)\n",
		__FUNCTION__, ctoctrl, bus->cto_enable, chipid));

	return 0;
}

static int
dhdpcie_cto_error_recovery(struct dhd_bus *bus)
{
	uint32 pci_intmask, err_status;
	uint8 i = 0;
	uint32 val;

	pci_intmask = dhdpcie_bus_cfg_read_dword(bus, PCI_INT_MASK, 4);
	dhdpcie_bus_cfg_write_dword(bus, PCI_INT_MASK, 4, pci_intmask & ~PCI_CTO_INT_MASK);

	DHD_OS_WAKE_LOCK(bus->dhd);

	DHD_ERROR(("--- CTO Triggered --- %d\n", bus->pwr_req_ref));

	/*
	 * DAR still accessible
	 */
	dhd_bus_dump_dar_registers(bus);

	/* reset backplane */
	val = dhdpcie_bus_cfg_read_dword(bus, PCI_SPROM_CONTROL, 4);
	dhdpcie_bus_cfg_write_dword(bus, PCI_SPROM_CONTROL, 4, val | SPROM_CFG_TO_SB_RST);

	/* clear timeout error */
	while (1) {
		err_status =  si_corereg(bus->sih, bus->sih->buscoreidx,
			DAR_ERRLOG(bus->sih->buscorerev),
			0, 0);
		if (err_status & PCIE_CTO_ERR_MASK) {
			si_corereg(bus->sih, bus->sih->buscoreidx,
					DAR_ERRLOG(bus->sih->buscorerev),
					~0, PCIE_CTO_ERR_MASK);
		} else {
			break;
		}
		OSL_DELAY(CTO_TO_CLEAR_WAIT_MS * 1000);
		i++;
		if (i > CTO_TO_CLEAR_WAIT_MAX_CNT) {
			DHD_ERROR(("cto recovery fail\n"));

			DHD_OS_WAKE_UNLOCK(bus->dhd);
			return BCME_ERROR;
		}
	}

	/* clear interrupt status */
	dhdpcie_bus_cfg_write_dword(bus, PCI_INT_STATUS, 4, PCI_CTO_INT_MASK);

	/* Halt ARM & remove reset */
	/* TBD : we can add ARM Halt here in case */

	/* reset SPROM_CFG_TO_SB_RST */
	val = dhdpcie_bus_cfg_read_dword(bus, PCI_SPROM_CONTROL, 4);

	DHD_ERROR(("cto recovery reset 0x%x:SPROM_CFG_TO_SB_RST(0x%x) 0x%x\n",
		PCI_SPROM_CONTROL, SPROM_CFG_TO_SB_RST, val));
	dhdpcie_bus_cfg_write_dword(bus, PCI_SPROM_CONTROL, 4, val & ~SPROM_CFG_TO_SB_RST);

	val = dhdpcie_bus_cfg_read_dword(bus, PCI_SPROM_CONTROL, 4);
	DHD_ERROR(("cto recovery success 0x%x:SPROM_CFG_TO_SB_RST(0x%x) 0x%x\n",
		PCI_SPROM_CONTROL, SPROM_CFG_TO_SB_RST, val));

	DHD_OS_WAKE_UNLOCK(bus->dhd);

	return BCME_OK;
}

void
dhdpcie_ssreset_dis_enum_rst(struct dhd_bus *bus)
{
	uint32 val;

	val = dhdpcie_bus_cfg_read_dword(bus, PCIE_CFG_SUBSYSTEM_CONTROL, 4);
	dhdpcie_bus_cfg_write_dword(bus, PCIE_CFG_SUBSYSTEM_CONTROL, 4,
		val | (0x1 << PCIE_SSRESET_DIS_ENUM_RST_BIT));
}

#if defined(DBG_PKT_MON) || defined(DHD_PKT_LOGGING)
/*
 * XXX: WAR: Update dongle that driver supports sending of d11
 * tx_status through unused status field of PCIe completion header
 * if dongle also supports the same WAR.
 */
static int
dhdpcie_init_d11status(struct dhd_bus *bus)
{
	uint32 addr;
	uint32 flags2;
	int ret = 0;

	if (bus->pcie_sh->flags2 & PCIE_SHARED2_D2H_D11_TX_STATUS) {
		flags2 = bus->pcie_sh->flags2;
		addr = bus->shared_addr + OFFSETOF(pciedev_shared_t, flags2);
		flags2 |= PCIE_SHARED2_H2D_D11_TX_STATUS;
		ret = dhdpcie_bus_membytes(bus, TRUE, addr,
			(uint8 *)&flags2, sizeof(flags2));
		if (ret < 0) {
			DHD_ERROR(("%s: update flag bit (H2D_D11_TX_STATUS) failed\n",
				__FUNCTION__));
			return ret;
		}
		bus->pcie_sh->flags2 = flags2;
		bus->dhd->d11_tx_status = TRUE;
	}
	return ret;
}

#else
static int
dhdpcie_init_d11status(struct dhd_bus *bus)
{
	return 0;
}
#endif /* DBG_PKT_MON || DHD_PKT_LOGGING */

int
dhdpcie_get_max_eventbufpost(struct dhd_bus *bus)
{
	int evt_buf_pool = EVENT_BUF_POOL_LOW;
	if (bus->pcie_sh->flags2 & (0x1 << PCIE_SHARED_EVENT_BUF_POOL_MAX_POS)) {
		evt_buf_pool = EVENT_BUF_POOL_MEDIUM;
	} else if (bus->pcie_sh->flags2 & (0x2 << PCIE_SHARED_EVENT_BUF_POOL_MAX_POS)) {
		evt_buf_pool = EVENT_BUF_POOL_HIGH;
	} else if (bus->pcie_sh->flags2 & (0x3 << PCIE_SHARED_EVENT_BUF_POOL_MAX_POS)) {
		evt_buf_pool = EVENT_BUF_POOL_HIGHEST;
	}
	return evt_buf_pool;
}

int
dhd_bus_oob_intr_register(dhd_pub_t *dhdp)
{
	int err = 0;
#ifdef BCMPCIE_OOB_HOST_WAKE
	err = dhdpcie_oob_intr_register(dhdp->bus);
#endif /* BCMPCIE_OOB_HOST_WAKE */
	return err;
}

void
dhd_bus_oob_intr_unregister(dhd_pub_t *dhdp)
{
#ifdef BCMPCIE_OOB_HOST_WAKE
	dhdpcie_oob_intr_unregister(dhdp->bus);
#endif /* BCMPCIE_OOB_HOST_WAKE */
}

void
dhd_bus_oob_intr_set(dhd_pub_t *dhdp, bool enable)
{
#ifdef BCMPCIE_OOB_HOST_WAKE
	dhdpcie_oob_intr_set(dhdp->bus, enable);
#endif /* BCMPCIE_OOB_HOST_WAKE */
}

int
dhd_bus_get_oob_irq_num(dhd_pub_t *dhdp)
{
	int irq_num = 0;
#ifdef BCMPCIE_OOB_HOST_WAKE
	irq_num = dhdpcie_get_oob_irq_num(dhdp->bus);
#endif /* BCMPCIE_OOB_HOST_WAKE */
	return irq_num;
}

#ifdef BCMDBG
void
dhd_bus_flow_ring_cnt_update(dhd_bus_t *bus, uint16 flowid, uint32 txstatus)
{
	flow_ring_node_t *flow_ring_node;
	/* If we have d2h sync enabled due to marker overloading, we cannot update this. */
	if (bus->dhd->d2h_sync_mode)
		return;
	if (txstatus >= DHD_MAX_TX_STATUS_MSGS) {
		/*
		 * XXX: changed DHD_ERROR to DHD_INFO
		 * There are flood of messages with P2P FW
		 * It is being root-caused.
		 */
		DHD_INFO(("%s Unknown txtstatus = %d \n",
		    __FUNCTION__, txstatus));
		return;
	}
	flow_ring_node = DHD_FLOW_RING(bus->dhd, flowid);
	ASSERT(flow_ring_node->flowid == flowid);
	flow_ring_node->flow_info.tx_status[txstatus]++;
	return;
}
#endif /* BCMDBG */

bool
dhdpcie_bus_get_pcie_hostready_supported(dhd_bus_t *bus)
{
	return bus->dhd->d2h_hostrdy_supported;
}

void
dhd_pcie_dump_core_regs(dhd_pub_t * pub, uint32 index, uint32 first_addr, uint32 last_addr)
{
	dhd_bus_t *bus = pub->bus;
	uint32	coreoffset = index << 12;
	uint32	core_addr = SI_ENUM_BASE(bus->sih) + coreoffset;
	uint32 value;

	while (first_addr <= last_addr) {
		core_addr = SI_ENUM_BASE(bus->sih) + coreoffset + first_addr;
		if (serialized_backplane_access(bus, core_addr, 4, &value, TRUE) != BCME_OK) {
			DHD_ERROR(("Invalid size/addr combination \n"));
		}
		DHD_ERROR(("[0x%08x]: 0x%08x\n", core_addr, value));
		first_addr = first_addr + 4;
	}
}

bool
dhdpcie_bus_get_pcie_idma_supported(dhd_bus_t *bus)
{
	if (!bus->dhd)
		return FALSE;
	else if (bus->idma_enabled) {
		return bus->dhd->idma_enable;
	} else {
		return FALSE;
	}
}

bool
dhdpcie_bus_get_pcie_ifrm_supported(dhd_bus_t *bus)
{
	if (!bus->dhd)
		return FALSE;
	else if (bus->ifrm_enabled) {
		return bus->dhd->ifrm_enable;
	} else {
		return FALSE;
	}
}

bool
dhdpcie_bus_get_pcie_dar_supported(dhd_bus_t *bus)
{
	if (!bus->dhd) {
		return FALSE;
	} else if (bus->dar_enabled) {
		return bus->dhd->dar_enable;
	} else {
		return FALSE;
	}
}

#ifdef PCIE_OOB
bool
dhdpcie_bus_get_pcie_oob_dw_supported(dhd_bus_t *bus)
{
	if (!bus->dhd)
		return FALSE;
	if (bus->oob_enabled) {
		return !bus->dhd->d2h_no_oob_dw;
	} else {
		return FALSE;
	}
}
#endif /* PCIE_OOB */

void
dhdpcie_bus_enab_pcie_dw(dhd_bus_t *bus, uint8 dw_option)
{
	DHD_ERROR(("ENABLING DW:%d\n", dw_option));
	bus->dw_option = dw_option;
}

#ifdef PCIE_INB_DW
bool
dhdpcie_bus_get_pcie_inband_dw_supported(dhd_bus_t *bus)
{
	if (!bus->dhd)
		return FALSE;
	if (bus->inb_enabled) {
		return bus->dhd->d2h_inband_dw;
	} else {
		return FALSE;
	}
}

void
dhdpcie_bus_set_pcie_inband_dw_state(dhd_bus_t *bus, enum dhd_bus_ds_state state)
{
	if (!INBAND_DW_ENAB(bus))
		return;

	DHD_INFO(("%s:%d\n", __FUNCTION__, state));
	bus->dhd->ds_state = state;
	if (state == DW_DEVICE_DS_DISABLED_WAIT || state == DW_DEVICE_DS_D3_INFORM_WAIT) {
		bus->ds_exit_timeout = 100;
	}
	if (state == DW_DEVICE_HOST_WAKE_WAIT) {
		bus->host_sleep_exit_timeout = 100;
	}
	if (state == DW_DEVICE_DS_DEV_WAKE) {
		bus->ds_exit_timeout = 0;
	}
	if (state == DW_DEVICE_DS_ACTIVE) {
		bus->host_sleep_exit_timeout = 0;
	}
}

enum dhd_bus_ds_state
dhdpcie_bus_get_pcie_inband_dw_state(dhd_bus_t *bus)
{
	if (!INBAND_DW_ENAB(bus))
		return DW_DEVICE_DS_INVALID;
	return bus->dhd->ds_state;
}
#endif /* PCIE_INB_DW */

#ifdef DHD_MMIO_TRACE
static void
dhd_bus_mmio_trace(dhd_bus_t *bus, uint32 addr, uint32 value, bool set)
{
	uint32 cnt = bus->mmio_trace_count % MAX_MMIO_TRACE_SIZE;
	uint64 ts_cur = OSL_LOCALTIME_NS();
	uint32 tmp_cnt;

	tmp_cnt = (bus->mmio_trace_count) ? ((bus->mmio_trace_count - 1)
		% MAX_MMIO_TRACE_SIZE) : cnt;

	if (((DIV_U64_BY_U64(ts_cur, NSEC_PER_USEC) -
		DIV_U64_BY_U64(bus->mmio_trace[tmp_cnt].timestamp, NSEC_PER_USEC))
		> MIN_MMIO_TRACE_TIME) || (bus->mmio_trace[tmp_cnt].value !=
		(value & DHD_RING_IDX))) {
		bus->mmio_trace_count++;
	} else {
		cnt = tmp_cnt;
	}
	bus->mmio_trace[cnt].timestamp = ts_cur;
	bus->mmio_trace[cnt].addr = addr;
	bus->mmio_trace[cnt].set = set;
	bus->mmio_trace[cnt].value = value;
}

void
dhd_dump_bus_mmio_trace(dhd_bus_t *bus, struct bcmstrbuf *strbuf)
{
	int dumpsz;
	int i;

	dumpsz = bus->mmio_trace_count < MAX_MMIO_TRACE_SIZE ?
		bus->mmio_trace_count : MAX_MMIO_TRACE_SIZE;
	if (dumpsz == 0) {
		bcm_bprintf(strbuf, "\nEmpty MMIO TRACE\n");
		return;
	}
	bcm_bprintf(strbuf, "---- MMIO TRACE ------\n");
	bcm_bprintf(strbuf, "Decoding value field, Ex: 0xFF2C00E4, 0xFF->WR/0XDD->RD "
		"0x2C->Ringid 0x00E4->RD/WR Value\n");
	bcm_bprintf(strbuf, "Timestamp ns\t\tAddr\t\tW/R\tValue\n");
	for (i = 0; i < dumpsz; i ++) {
		bcm_bprintf(strbuf, SEC_USEC_FMT"\t0x%08x\t%s\t0x%08x\n",
			GET_SEC_USEC(bus->mmio_trace[i].timestamp),
			bus->mmio_trace[i].addr,
			bus->mmio_trace[i].set ? "W" : "R",
			bus->mmio_trace[i].value);
	}
}
#endif /* defined(DHD_MMIO_TRACE) */

static void
#ifdef PCIE_INB_DW
dhd_bus_ds_trace(dhd_bus_t *bus, uint32 dsval, bool d2h, enum dhd_bus_ds_state inbstate)
#else
dhd_bus_ds_trace(dhd_bus_t *bus, uint32 dsval, bool d2h)
#endif /* PCIE_INB_DW */
{
	uint32 cnt = bus->ds_trace_count % MAX_DS_TRACE_SIZE;

	bus->ds_trace[cnt].timestamp = OSL_LOCALTIME_NS();
	bus->ds_trace[cnt].d2h = d2h;
	bus->ds_trace[cnt].dsval = dsval;
#ifdef PCIE_INB_DW
	bus->ds_trace[cnt].inbstate = inbstate;
#endif /* PCIE_INB_DW */
	bus->ds_trace_count ++;
}

#ifdef PCIE_INB_DW
const char *
dhd_convert_dsval(uint32 val, bool d2h)
{
	if (d2h) {
		switch (val) {
			case D2H_DEV_D3_ACK:
				return "D2H_DEV_D3_ACK";
			case D2H_DEV_DS_ENTER_REQ:
				return "D2H_DEV_DS_ENTER_REQ";
			case D2H_DEV_DS_EXIT_NOTE:
				return "D2H_DEV_DS_EXIT_NOTE";
			case D2H_DEV_FWHALT:
				return "D2H_DEV_FWHALT";
			case D2HMB_DS_HOST_SLEEP_EXIT_ACK:
				return "D2HMB_DS_HOST_SLEEP_EXIT_ACK";
			default:
				return "INVALID";
		}
	} else {
		switch (val) {
			case H2DMB_DS_DEVICE_WAKE_DEASSERT:
				return "H2DMB_DS_DEVICE_WAKE_DEASSERT";
			case H2DMB_DS_DEVICE_WAKE_ASSERT:
				return "H2DMB_DS_DEVICE_WAKE_ASSERT";
			case H2D_HOST_D3_INFORM:
				return "H2D_HOST_D3_INFORM";
			case H2D_HOST_DS_ACK:
				return "H2D_HOST_DS_ACK";
			case H2D_HOST_DS_NAK:
				return "H2D_HOST_DS_NAK";
			case H2D_HOST_CONS_INT:
				return "H2D_HOST_CONS_INT";
			case H2D_FW_TRAP:
				return "H2D_FW_TRAP";
			default:
				return "INVALID";
		}
	}
}

const char *
dhd_convert_inb_state_names(enum dhd_bus_ds_state inbstate)
{
	switch (inbstate) {
		case DW_DEVICE_DS_DEV_SLEEP:
			return "DW_DEVICE_DS_DEV_SLEEP";
		break;
		case DW_DEVICE_DS_DISABLED_WAIT:
			return "DW_DEVICE_DS_DISABLED_WAIT";
		break;
		case DW_DEVICE_DS_DEV_WAKE:
			return "DW_DEVICE_DS_DEV_WAKE";
		break;
		case DW_DEVICE_DS_ACTIVE:
			return "DW_DEVICE_DS_ACTIVE";
		break;
		case DW_DEVICE_HOST_SLEEP_WAIT:
			return "DW_DEVICE_HOST_SLEEP_WAIT";
		break;
		case DW_DEVICE_HOST_SLEEP:
			return "DW_DEVICE_HOST_SLEEP";
		break;
		case DW_DEVICE_HOST_WAKE_WAIT:
			return "DW_DEVICE_HOST_WAKE_WAIT";
		break;
		case DW_DEVICE_DS_D3_INFORM_WAIT:
			return "DW_DEVICE_DS_D3_INFORM_WAIT";
		break;
		default:
			return "INVALID";
	}
}
#endif /* PCIE_INB_DW */

void
dhd_dump_bus_ds_trace(dhd_bus_t *bus, struct bcmstrbuf *strbuf)
{
	int dumpsz;
	int i;

	dumpsz = bus->ds_trace_count < MAX_DS_TRACE_SIZE ?
		bus->ds_trace_count : MAX_DS_TRACE_SIZE;
	if (dumpsz == 0) {
		bcm_bprintf(strbuf, "\nEmpty DS TRACE\n");
		return;
	}
	bcm_bprintf(strbuf, "---- DS TRACE ------\n");
#ifdef PCIE_INB_DW
	bcm_bprintf(strbuf, "%s\t\t%s\t%-30s\t\t%s\n",
		"Timestamp us", "Dir", "Value", "Inband-State");
	for (i = 0; i < dumpsz; i ++) {
		bcm_bprintf(strbuf, "%llu\t%s\t%-30s\t\t%s\n",
		bus->ds_trace[i].timestamp,
		bus->ds_trace[i].d2h ? "D2H":"H2D",
		dhd_convert_dsval(bus->ds_trace[i].dsval, bus->ds_trace[i].d2h),
		dhd_convert_inb_state_names(bus->ds_trace[i].inbstate));
	}
#else
	bcm_bprintf(strbuf, "Timestamp us\t\tDir\tValue\n");
	for (i = 0; i < dumpsz; i ++) {
		bcm_bprintf(strbuf, "%llu\t%s\t%d\n",
		bus->ds_trace[i].timestamp,
		bus->ds_trace[i].d2h ? "D2H":"H2D",
		bus->ds_trace[i].dsval);
	}
#endif /* PCIE_INB_DW */
	bcm_bprintf(strbuf, "--------------------------\n");
}

void
dhd_bus_dump_trap_info(dhd_bus_t *bus, struct bcmstrbuf *strbuf)
{
	trap_t *tr = &bus->dhd->last_trap_info;
	bcm_bprintf(strbuf,
		"\nTRAP type 0x%x @ epc 0x%x, cpsr 0x%x, spsr 0x%x, sp 0x%x,"
		" lp 0x%x, rpc 0x%x"
		"\nTrap offset 0x%x, r0 0x%x, r1 0x%x, r2 0x%x, r3 0x%x, "
		"r4 0x%x, r5 0x%x, r6 0x%x, r7 0x%x, r8 0x%x, r9 0x%x, "
		"r10 0x%x, r11 0x%x, r12 0x%x\n\n",
		ltoh32(tr->type), ltoh32(tr->epc), ltoh32(tr->cpsr), ltoh32(tr->spsr),
		ltoh32(tr->r13), ltoh32(tr->r14), ltoh32(tr->pc),
		ltoh32(bus->pcie_sh->trap_addr),
		ltoh32(tr->r0), ltoh32(tr->r1), ltoh32(tr->r2), ltoh32(tr->r3),
		ltoh32(tr->r4), ltoh32(tr->r5), ltoh32(tr->r6), ltoh32(tr->r7),
		ltoh32(tr->r8), ltoh32(tr->r9), ltoh32(tr->r10),
		ltoh32(tr->r11), ltoh32(tr->r12));
}

int
dhd_bus_readwrite_bp_addr(dhd_pub_t *dhdp, uint addr, uint size, uint* data, bool read)
{
	int bcmerror = 0;
	struct dhd_bus *bus = dhdp->bus;

	if (serialized_backplane_access(bus, addr, size, data, read) != BCME_OK) {
			DHD_ERROR(("Invalid size/addr combination \n"));
			bcmerror = BCME_ERROR;
	}

	return bcmerror;
}

int
dhd_get_idletime(dhd_pub_t *dhd)
{
	return dhd->bus->idletime;
}

bool
dhd_get_rpm_state(dhd_pub_t *dhd)
{
	return dhd->bus->rpm_enabled;
}

void
dhd_set_rpm_state(dhd_pub_t *dhd, bool state)
{
	DHD_RPM(("%s: %d\n", __FUNCTION__, state));
	dhd->bus->rpm_enabled = state;
}

static INLINE void
dhd_sbreg_op(dhd_pub_t *dhd, uint addr, uint *val, bool read)
{
	OSL_DELAY(1);
	if (serialized_backplane_access(dhd->bus, addr, sizeof(uint), val, read) != BCME_OK) {
		DHD_ERROR(("sbreg: Invalid uint addr: 0x%x \n", addr));
	} else {
		DHD_ERROR(("sbreg: addr:0x%x val:0x%x read:%d\n", addr, *val, read));
	}
	return;
}

#ifdef DHD_SSSR_DUMP
static int
dhdpcie_get_sssr_fifo_dump(dhd_pub_t *dhd, uint *buf, uint fifo_size,
	uint addr_reg, uint data_reg)
{
	uint addr;
	uint val = 0;
	int i;

	DHD_ERROR(("%s\n", __FUNCTION__));

	if (!buf) {
		DHD_ERROR(("%s: buf is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (!fifo_size) {
		DHD_ERROR(("%s: fifo_size is 0\n", __FUNCTION__));
		return BCME_ERROR;
	}

	/* Set the base address offset to 0 */
	addr = addr_reg;
	val = 0;
	dhd_sbreg_op(dhd, addr, &val, FALSE);

	addr = data_reg;
	/* Read 4 bytes at once and loop for fifo_size / 4 */
	for (i = 0; i < fifo_size / 4; i++) {
		if (serialized_backplane_access(dhd->bus, addr,
				sizeof(uint), &val, TRUE) != BCME_OK) {
			DHD_ERROR(("%s: error in serialized_backplane_access\n", __FUNCTION__));
			return BCME_ERROR;
		}
		buf[i] = val;
		OSL_DELAY(1);
	}
	return BCME_OK;
}

static int
dhdpcie_get_sssr_dig_dump(dhd_pub_t *dhd, uint *buf, uint fifo_size,
	uint addr_reg)
{
	uint addr;
	uint val = 0;
	int i;
	si_t *sih = dhd->bus->sih;
	bool vasip_enab, dig_mem_check;
	uint32 ioctrl_addr = 0;

	DHD_ERROR(("%s addr_reg=0x%x size=0x%x\n", __FUNCTION__, addr_reg, fifo_size));

	if (!buf) {
		DHD_ERROR(("%s: buf is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (!fifo_size) {
		DHD_ERROR(("%s: fifo_size is 0\n", __FUNCTION__));
		return BCME_ERROR;
	}

	vasip_enab = FALSE;
	dig_mem_check = FALSE;
	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	switch (dhd->sssr_reg_info->rev2.version) {
		case SSSR_REG_INFO_VER_3 :
			/* intentional fall through */
		case SSSR_REG_INFO_VER_2 :
			if ((dhd->sssr_reg_info->rev2.length > OFFSETOF(sssr_reg_info_v2_t,
			dig_mem_info)) && dhd->sssr_reg_info->rev2.dig_mem_info.dig_sr_size) {
				dig_mem_check = TRUE;
			}
			break;
		case SSSR_REG_INFO_VER_1 :
			if (dhd->sssr_reg_info->rev1.vasip_regs.vasip_sr_size) {
				vasip_enab = TRUE;
			} else if ((dhd->sssr_reg_info->rev1.length > OFFSETOF(sssr_reg_info_v1_t,
				dig_mem_info)) && dhd->sssr_reg_info->rev1.
				dig_mem_info.dig_sr_size) {
				dig_mem_check = TRUE;
			}
			ioctrl_addr = dhd->sssr_reg_info->rev1.vasip_regs.wrapper_regs.ioctrl;
			break;
		case SSSR_REG_INFO_VER_0 :
			if (dhd->sssr_reg_info->rev0.vasip_regs.vasip_sr_size) {
				vasip_enab = TRUE;
			}
			ioctrl_addr = dhd->sssr_reg_info->rev0.vasip_regs.wrapper_regs.ioctrl;
			break;
		default :
			DHD_ERROR(("invalid sssr_reg_ver"));
			return BCME_UNSUPPORTED;
	}
	if (addr_reg) {
		DHD_ERROR(("dig_mem_check=%d vasip_enab=%d\n", dig_mem_check, vasip_enab));
		if (!vasip_enab && dig_mem_check) {
			int err = dhdpcie_bus_membytes(dhd->bus, FALSE, addr_reg, (uint8 *)buf,
				fifo_size);
			if (err != BCME_OK) {
				DHD_ERROR(("%s: Error reading dig dump from dongle !\n",
					__FUNCTION__));
			}
		} else {
			/* Check if vasip clk is disabled, if yes enable it */
			addr = ioctrl_addr;
			dhd_sbreg_op(dhd, addr, &val, TRUE);
			if (!val) {
				val = 1;
				dhd_sbreg_op(dhd, addr, &val, FALSE);
			}

			addr = addr_reg;
			/* Read 4 bytes at once and loop for fifo_size / 4 */
			for (i = 0; i < fifo_size / 4; i++, addr += 4) {
				if (serialized_backplane_access(dhd->bus, addr, sizeof(uint),
					&val, TRUE) != BCME_OK) {
					DHD_ERROR(("%s: Invalid uint addr: 0x%x \n", __FUNCTION__,
						addr));
					return BCME_ERROR;
				}
				buf[i] = val;
				OSL_DELAY(1);
			}
		}
	} else {
		uint cur_coreid;
		uint chipc_corerev;
		chipcregs_t *chipcregs;

		/* Save the current core */
		cur_coreid = si_coreid(sih);

		/* Switch to ChipC */
		chipcregs = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);
		if (!chipcregs) {
			DHD_ERROR(("%s: si_setcore returns NULL for core id %u \n",
				__FUNCTION__, CC_CORE_ID));
			return BCME_ERROR;
		}

		chipc_corerev = si_corerev(sih);

		if ((chipc_corerev == 64) || (chipc_corerev == 65)) {
			W_REG(si_osh(sih), &chipcregs->sr_memrw_addr, 0);

			/* Read 4 bytes at once and loop for fifo_size / 4 */
			for (i = 0; i < fifo_size / 4; i++) {
				buf[i] = R_REG(si_osh(sih), &chipcregs->sr_memrw_data);
				OSL_DELAY(1);
			}
		}

		/* Switch back to the original core */
		si_setcore(sih, cur_coreid, 0);
	}

	return BCME_OK;
}

#if defined(BCMPCIE) && defined(EWP_ETD_PRSRV_LOGS)
void
dhdpcie_get_etd_preserve_logs(dhd_pub_t *dhd,
		uint8 *ext_trap_data, void *event_decode_data)
{
	hnd_ext_trap_hdr_t *hdr = NULL;
	bcm_tlv_t *tlv;
	eventlog_trapdata_info_t *etd_evtlog = NULL;
	eventlog_trap_buf_info_t *evtlog_buf_arr = NULL;
	uint arr_size = 0;
	int i = 0;
	int err = 0;
	uint32 seqnum = 0;

	if (!ext_trap_data || !event_decode_data || !dhd)
		return;

	if (!dhd->concise_dbg_buf)
		return;

	/* First word is original trap_data, skip */
	ext_trap_data += sizeof(uint32);

	hdr = (hnd_ext_trap_hdr_t *)ext_trap_data;
	tlv = bcm_parse_tlvs(hdr->data, hdr->len, TAG_TRAP_LOG_DATA);
	if (tlv) {
		uint32 baseaddr = 0;
		uint32 endaddr = dhd->bus->dongle_ram_base + dhd->bus->ramsize - 4;

		etd_evtlog = (eventlog_trapdata_info_t *)tlv->data;
		DHD_ERROR(("%s: etd_evtlog tlv found, num_elements=%x; "
			"seq_num=%x; log_arr_addr=%x\n", __FUNCTION__,
			(etd_evtlog->num_elements),
			ntoh32(etd_evtlog->seq_num), (etd_evtlog->log_arr_addr)));
		if (!etd_evtlog->num_elements ||
				etd_evtlog->num_elements > MAX_EVENTLOG_BUFFERS) {
			DHD_ERROR(("%s: ETD has bad 'num_elements' !\n", __FUNCTION__));
			return;
		}
		if (!etd_evtlog->log_arr_addr) {
			DHD_ERROR(("%s: ETD has bad 'log_arr_addr' !\n", __FUNCTION__));
			return;
		}

		arr_size = (uint32)sizeof(*evtlog_buf_arr) * (etd_evtlog->num_elements);
		evtlog_buf_arr = MALLOCZ(dhd->osh, arr_size);
		if (!evtlog_buf_arr) {
			DHD_ERROR(("%s: out of memory !\n",	__FUNCTION__));
			return;
		}

		/* boundary check */
		baseaddr = etd_evtlog->log_arr_addr;
		if ((baseaddr < dhd->bus->dongle_ram_base) || (baseaddr > endaddr) ||
			((baseaddr + arr_size) > endaddr)) {
			DHD_ERROR(("%s: Error reading invalid address\n",
				__FUNCTION__));
			goto err;
		}

		/* read the eventlog_trap_buf_info_t array from dongle memory */
		err = dhdpcie_bus_membytes(dhd->bus, FALSE,
				(ulong)(etd_evtlog->log_arr_addr),
				(uint8 *)evtlog_buf_arr, arr_size);
		if (err != BCME_OK) {
			DHD_ERROR(("%s: Error reading event log array from dongle !\n",
				__FUNCTION__));
			goto err;
		}
		/* ntoh is required only for seq_num, because in the original
		* case of event logs from info ring, it is sent from dongle in that way
		* so for ETD also dongle follows same convention
		*/
		seqnum = ntoh32(etd_evtlog->seq_num);
		memset(dhd->concise_dbg_buf, 0, CONCISE_DUMP_BUFLEN);
		for (i = 0; i < (etd_evtlog->num_elements); ++i) {
			/* boundary check */
			baseaddr = evtlog_buf_arr[i].buf_addr;
			if ((baseaddr < dhd->bus->dongle_ram_base) || (baseaddr > endaddr) ||
				((baseaddr + evtlog_buf_arr[i].len) > endaddr)) {
				DHD_ERROR(("%s: Error reading invalid address\n",
					__FUNCTION__));
				goto err;
			}
			/* read each individual event log buf from dongle memory */
			err = dhdpcie_bus_membytes(dhd->bus, FALSE,
					((ulong)evtlog_buf_arr[i].buf_addr),
					dhd->concise_dbg_buf, (evtlog_buf_arr[i].len));
			if (err != BCME_OK) {
				DHD_ERROR(("%s: Error reading event log buffer from dongle !\n",
					__FUNCTION__));
				goto err;
			}
			dhd_dbg_msgtrace_log_parser(dhd, dhd->concise_dbg_buf,
				event_decode_data, (evtlog_buf_arr[i].len),
				FALSE, hton32(seqnum));
			++seqnum;
		}
err:
		MFREE(dhd->osh, evtlog_buf_arr, arr_size);
	} else {
		DHD_ERROR(("%s: Error getting trap log data in ETD !\n", __FUNCTION__));
	}
}
#endif /* BCMPCIE && DHD_LOG_DUMP */

static uint32
dhdpcie_resume_chipcommon_powerctrl(dhd_pub_t *dhd, uint32 reg_val)
{
	uint addr;
	uint val = 0;
	uint powerctrl_mask;

	DHD_ERROR(("%s\n", __FUNCTION__));

	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	switch (dhd->sssr_reg_info->rev2.version) {
		case SSSR_REG_INFO_VER_3 :
			/* intentional fall through */
		case SSSR_REG_INFO_VER_2 :
			addr = dhd->sssr_reg_info->rev2.chipcommon_regs.base_regs.powerctrl;
			powerctrl_mask = dhd->sssr_reg_info->rev2.
				chipcommon_regs.base_regs.powerctrl_mask;
			break;
		case SSSR_REG_INFO_VER_1 :
		case SSSR_REG_INFO_VER_0 :
			addr = dhd->sssr_reg_info->rev1.chipcommon_regs.base_regs.powerctrl;
			powerctrl_mask = dhd->sssr_reg_info->rev1.
				chipcommon_regs.base_regs.powerctrl_mask;
			break;
		default :
			DHD_ERROR(("invalid sssr_reg_ver"));
			return BCME_UNSUPPORTED;
	}

	/* conditionally clear bits [11:8] of PowerCtrl */
	dhd_sbreg_op(dhd, addr, &val, TRUE);

	if (!(val & powerctrl_mask)) {
		dhd_sbreg_op(dhd, addr, &reg_val, FALSE);
	}
	return BCME_OK;
}

static uint32
dhdpcie_suspend_chipcommon_powerctrl(dhd_pub_t *dhd)
{
	uint addr;
	uint val = 0, reg_val = 0;
	uint powerctrl_mask;

	DHD_ERROR(("%s\n", __FUNCTION__));

	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	switch (dhd->sssr_reg_info->rev2.version) {
		case SSSR_REG_INFO_VER_3 :
			/* intentional fall through */
		case SSSR_REG_INFO_VER_2 :
			addr = dhd->sssr_reg_info->rev2.chipcommon_regs.base_regs.powerctrl;
			powerctrl_mask = dhd->sssr_reg_info->rev2.
				chipcommon_regs.base_regs.powerctrl_mask;
			break;
		case SSSR_REG_INFO_VER_1 :
		case SSSR_REG_INFO_VER_0 :
			addr = dhd->sssr_reg_info->rev1.chipcommon_regs.base_regs.powerctrl;
			powerctrl_mask = dhd->sssr_reg_info->rev1.
				chipcommon_regs.base_regs.powerctrl_mask;
			break;
		default :
			DHD_ERROR(("invalid sssr_reg_ver"));
			return BCME_UNSUPPORTED;
	}

	/* conditionally clear bits [11:8] of PowerCtrl */
	dhd_sbreg_op(dhd, addr, &reg_val, TRUE);
	if (reg_val & powerctrl_mask) {
		val = 0;
		dhd_sbreg_op(dhd, addr, &val, FALSE);
	}
	return reg_val;
}

static int
dhdpcie_clear_intmask_and_timer(dhd_pub_t *dhd)
{
	uint addr;
	uint val;
	uint32 cc_intmask, pmuintmask0, pmuintmask1, resreqtimer, macresreqtimer,
	 macresreqtimer1, vasip_sr_size = 0;

	DHD_ERROR(("%s\n", __FUNCTION__));

	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	switch (dhd->sssr_reg_info->rev2.version) {
		case SSSR_REG_INFO_VER_3 :
			/* intentional fall through */
		case SSSR_REG_INFO_VER_2 :
			cc_intmask = dhd->sssr_reg_info->rev2.chipcommon_regs.base_regs.intmask;
			pmuintmask0 = dhd->sssr_reg_info->rev2.pmu_regs.base_regs.pmuintmask0;
			pmuintmask1 = dhd->sssr_reg_info->rev2.pmu_regs.base_regs.pmuintmask1;
			resreqtimer = dhd->sssr_reg_info->rev2.pmu_regs.base_regs.resreqtimer;
			macresreqtimer = dhd->sssr_reg_info->rev2.pmu_regs.base_regs.macresreqtimer;
			macresreqtimer1 = dhd->sssr_reg_info->rev2.
				pmu_regs.base_regs.macresreqtimer1;
			break;
		case SSSR_REG_INFO_VER_1 :
		case SSSR_REG_INFO_VER_0 :
			cc_intmask = dhd->sssr_reg_info->rev1.chipcommon_regs.base_regs.intmask;
			pmuintmask0 = dhd->sssr_reg_info->rev1.pmu_regs.base_regs.pmuintmask0;
			pmuintmask1 = dhd->sssr_reg_info->rev1.pmu_regs.base_regs.pmuintmask1;
			resreqtimer = dhd->sssr_reg_info->rev1.pmu_regs.base_regs.resreqtimer;
			macresreqtimer = dhd->sssr_reg_info->rev1.pmu_regs.base_regs.macresreqtimer;
			macresreqtimer1 = dhd->sssr_reg_info->rev1.
				pmu_regs.base_regs.macresreqtimer1;
			vasip_sr_size = dhd->sssr_reg_info->rev1.vasip_regs.vasip_sr_size;
			break;
		default :
			DHD_ERROR(("invalid sssr_reg_ver"));
			return BCME_UNSUPPORTED;
	}

	/* clear chipcommon intmask */
	val = 0x0;
	dhd_sbreg_op(dhd, cc_intmask, &val, FALSE);

	/* clear PMUIntMask0 */
	val = 0x0;
	dhd_sbreg_op(dhd, pmuintmask0, &val, FALSE);

	/* clear PMUIntMask1 */
	val = 0x0;
	dhd_sbreg_op(dhd, pmuintmask1, &val, FALSE);

	/* clear res_req_timer */
	val = 0x0;
	dhd_sbreg_op(dhd, resreqtimer, &val, FALSE);

	/* clear macresreqtimer */
	val = 0x0;
	dhd_sbreg_op(dhd, macresreqtimer, &val, FALSE);

	/* clear macresreqtimer1 */
	val = 0x0;
	dhd_sbreg_op(dhd, macresreqtimer1, &val, FALSE);

	/* clear VasipClkEn */
	if (vasip_sr_size) {
		addr = dhd->sssr_reg_info->rev1.vasip_regs.wrapper_regs.ioctrl;
		val = 0x0;
		dhd_sbreg_op(dhd, addr, &val, FALSE);
	}

	return BCME_OK;
}

static void
dhdpcie_update_d11_status_from_trapdata(dhd_pub_t *dhd)
{
#define TRAP_DATA_MAIN_CORE_BIT_MASK	(1 << 1)
#define TRAP_DATA_AUX_CORE_BIT_MASK	(1 << 4)
	uint trap_data_mask[MAX_NUM_D11CORES] =
		{TRAP_DATA_MAIN_CORE_BIT_MASK, TRAP_DATA_AUX_CORE_BIT_MASK};
	int i;
	/* Apply only for 4375 chip */
	if (dhd_bus_chip_id(dhd) == BCM4375_CHIP_ID) {
		for (i = 0; i < MAX_NUM_D11CORES; i++) {
			if (dhd->sssr_d11_outofreset[i] &&
				(dhd->dongle_trap_data & trap_data_mask[i])) {
				dhd->sssr_d11_outofreset[i] = TRUE;
			} else {
				dhd->sssr_d11_outofreset[i] = FALSE;
			}
			DHD_ERROR(("%s: sssr_d11_outofreset[%d] : %d after AND with "
				"trap_data:0x%x-0x%x\n",
				__FUNCTION__, i, dhd->sssr_d11_outofreset[i],
				dhd->dongle_trap_data, trap_data_mask[i]));
		}
	}
}

static int
dhdpcie_d11_check_outofreset(dhd_pub_t *dhd)
{
	int i;
	uint addr = 0;
	uint val = 0;
	uint8 num_d11cores;

	DHD_ERROR(("%s\n", __FUNCTION__));

	num_d11cores = dhd_d11_slices_num_get(dhd);

	for (i = 0; i < num_d11cores; i++) {
		/* Check if bit 0 of resetctrl is cleared */
		/* SSSR register information structure v0 and
		 * v1 shares most except dig_mem
		 */
		switch (dhd->sssr_reg_info->rev2.version) {
			case SSSR_REG_INFO_VER_3 :
				/* intentional fall through */
			case SSSR_REG_INFO_VER_2 :
				addr = dhd->sssr_reg_info->rev2.
					mac_regs[i].wrapper_regs.resetctrl;
				break;
			case SSSR_REG_INFO_VER_1 :
			case SSSR_REG_INFO_VER_0 :
				addr = dhd->sssr_reg_info->rev1.
					mac_regs[i].wrapper_regs.resetctrl;
				break;
			default :
				DHD_ERROR(("invalid sssr_reg_ver"));
				return BCME_UNSUPPORTED;
		}
		if (!addr) {
			DHD_ERROR(("%s: skipping for core[%d] as 'addr' is NULL\n",
				__FUNCTION__, i));
			continue;
		}
		dhd_sbreg_op(dhd, addr, &val, TRUE);
		if (!(val & 1)) {
			dhd->sssr_d11_outofreset[i] = TRUE;
		} else {
			dhd->sssr_d11_outofreset[i] = FALSE;
		}
		DHD_ERROR(("%s: sssr_d11_outofreset[%d] : %d\n",
			__FUNCTION__, i, dhd->sssr_d11_outofreset[i]));
	}
	/* XXX Temporary WAR for 4375 to handle AXI errors on bad core
	 * to not collect SSSR dump for the core whose bit is not set in trap_data.
	 * It will be reverted once AXI errors are fixed
	 */
	dhdpcie_update_d11_status_from_trapdata(dhd);

	return BCME_OK;
}

static int
dhdpcie_d11_clear_clk_req(dhd_pub_t *dhd)
{
	int i;
	uint val = 0;
	uint8 num_d11cores;
	uint32 clockrequeststatus, clockcontrolstatus, clockcontrolstatus_val;

	DHD_ERROR(("%s\n", __FUNCTION__));

	num_d11cores = dhd_d11_slices_num_get(dhd);

	for (i = 0; i < num_d11cores; i++) {
		if (dhd->sssr_d11_outofreset[i]) {
			/* clear request clk only if itopoobb/extrsrcreqs is non zero */
			/* SSSR register information structure v0 and
			 * v1 shares most except dig_mem
			 */
			switch (dhd->sssr_reg_info->rev2.version) {
				case SSSR_REG_INFO_VER_3 :
					/* intentional fall through */
				case SSSR_REG_INFO_VER_2 :
					clockrequeststatus = dhd->sssr_reg_info->rev2.
						mac_regs[i].wrapper_regs.extrsrcreq;
					clockcontrolstatus = dhd->sssr_reg_info->rev2.
						mac_regs[i].base_regs.clockcontrolstatus;
					clockcontrolstatus_val = dhd->sssr_reg_info->rev2.
						mac_regs[i].base_regs.clockcontrolstatus_val;
					break;
				case SSSR_REG_INFO_VER_1 :
				case SSSR_REG_INFO_VER_0 :
					clockrequeststatus = dhd->sssr_reg_info->rev1.
						mac_regs[i].wrapper_regs.itopoobb;
					clockcontrolstatus = dhd->sssr_reg_info->rev1.
						mac_regs[i].base_regs.clockcontrolstatus;
					clockcontrolstatus_val = dhd->sssr_reg_info->rev1.
						mac_regs[i].base_regs.clockcontrolstatus_val;
					break;
				default :
					DHD_ERROR(("invalid sssr_reg_ver"));
					return BCME_UNSUPPORTED;
			}
			dhd_sbreg_op(dhd, clockrequeststatus, &val, TRUE);
			if (val != 0) {
				/* clear clockcontrolstatus */
				dhd_sbreg_op(dhd, clockcontrolstatus,
				 &clockcontrolstatus_val, FALSE);
			}
		}
	}
	return BCME_OK;
}

static int
dhdpcie_arm_clear_clk_req(dhd_pub_t *dhd)
{
	uint val = 0;
	uint cfgval = 0;
	uint32 resetctrl, clockrequeststatus, clockcontrolstatus, clockcontrolstatus_val;

	DHD_ERROR(("%s\n", __FUNCTION__));

	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	switch (dhd->sssr_reg_info->rev2.version) {
		case SSSR_REG_INFO_VER_3 :
			/* intentional fall through */
		case SSSR_REG_INFO_VER_2 :
			resetctrl = dhd->sssr_reg_info->rev2.
				arm_regs.wrapper_regs.resetctrl;
			clockrequeststatus = dhd->sssr_reg_info->rev2.
				arm_regs.wrapper_regs.extrsrcreq;
			clockcontrolstatus = dhd->sssr_reg_info->rev2.
				arm_regs.base_regs.clockcontrolstatus;
			clockcontrolstatus_val = dhd->sssr_reg_info->rev2.
				arm_regs.base_regs.clockcontrolstatus_val;
			break;
		case SSSR_REG_INFO_VER_1 :
		case SSSR_REG_INFO_VER_0 :
			resetctrl = dhd->sssr_reg_info->rev1.
				arm_regs.wrapper_regs.resetctrl;
			clockrequeststatus = dhd->sssr_reg_info->rev1.
				arm_regs.wrapper_regs.itopoobb;
			clockcontrolstatus = dhd->sssr_reg_info->rev1.
				arm_regs.base_regs.clockcontrolstatus;
			clockcontrolstatus_val = dhd->sssr_reg_info->rev1.
				arm_regs.base_regs.clockcontrolstatus_val;
			break;
		default :
			DHD_ERROR(("invalid sssr_reg_ver"));
			return BCME_UNSUPPORTED;
	}

	/* Check if bit 0 of resetctrl is cleared */
	dhd_sbreg_op(dhd, resetctrl, &val, TRUE);
	if (!(val & 1)) {
		/* clear request clk only if itopoobb/extrsrcreqs is non zero */
		dhd_sbreg_op(dhd, clockrequeststatus, &val, TRUE);
		if (val != 0) {
			/* clear clockcontrolstatus */
			dhd_sbreg_op(dhd, clockcontrolstatus, &clockcontrolstatus_val, FALSE);
		}

		if (MULTIBP_ENAB(dhd->bus->sih)) {
			/* Clear coherent bits for CA7 because CPU is halted */
			if (dhd->bus->coreid == ARMCA7_CORE_ID) {
				cfgval = dhdpcie_bus_cfg_read_dword(dhd->bus,
					PCIE_CFG_SUBSYSTEM_CONTROL, 4);
				dhdpcie_bus_cfg_write_dword(dhd->bus, PCIE_CFG_SUBSYSTEM_CONTROL, 4,
					(cfgval & ~PCIE_BARCOHERENTACCEN_MASK));
			}

			/* Just halt ARM but do not reset the core */
			resetctrl &= ~(SI_CORE_SIZE - 1);
			resetctrl += OFFSETOF(aidmp_t, ioctrl);

			dhd_sbreg_op(dhd, resetctrl, &val, TRUE);
			val |= SICF_CPUHALT;
			dhd_sbreg_op(dhd, resetctrl, &val, FALSE);
		}
	}

	return BCME_OK;
}

static int
dhdpcie_arm_resume_clk_req(dhd_pub_t *dhd)
{
	uint val = 0;
	uint32 resetctrl;

	DHD_ERROR(("%s\n", __FUNCTION__));

	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	switch (dhd->sssr_reg_info->rev2.version) {
		case SSSR_REG_INFO_VER_3 :
			/* intentional fall through */
		case SSSR_REG_INFO_VER_2 :
			resetctrl = dhd->sssr_reg_info->rev2.
				arm_regs.wrapper_regs.resetctrl;
			break;
		case SSSR_REG_INFO_VER_1 :
		case SSSR_REG_INFO_VER_0 :
			resetctrl = dhd->sssr_reg_info->rev1.
				arm_regs.wrapper_regs.resetctrl;
			break;
		default :
			DHD_ERROR(("invalid sssr_reg_ver"));
			return BCME_UNSUPPORTED;
	}

	/* Check if bit 0 of resetctrl is cleared */
	dhd_sbreg_op(dhd, resetctrl, &val, TRUE);
	if (!(val & 1)) {
		if (MULTIBP_ENAB(dhd->bus->sih) && (dhd->bus->coreid != ARMCA7_CORE_ID)) {
			/* Take ARM out of halt but do not reset core */
			resetctrl &= ~(SI_CORE_SIZE - 1);
			resetctrl += OFFSETOF(aidmp_t, ioctrl);

			dhd_sbreg_op(dhd, resetctrl, &val, TRUE);
			val &= ~SICF_CPUHALT;
			dhd_sbreg_op(dhd, resetctrl, &val, FALSE);
			dhd_sbreg_op(dhd, resetctrl, &val, TRUE);
		}
	}

	return BCME_OK;
}

static int
dhdpcie_pcie_clear_clk_req(dhd_pub_t *dhd)
{
	uint val = 0;
	uint32 clockrequeststatus, clockcontrolstatus_addr, clockcontrolstatus_val;

	DHD_ERROR(("%s\n", __FUNCTION__));

	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	switch (dhd->sssr_reg_info->rev2.version) {
		case SSSR_REG_INFO_VER_3 :
			/* intentional fall through */
		case SSSR_REG_INFO_VER_2 :
			clockrequeststatus = dhd->sssr_reg_info->rev2.
				pcie_regs.wrapper_regs.extrsrcreq;
			clockcontrolstatus_addr = dhd->sssr_reg_info->rev2.
				pcie_regs.base_regs.clockcontrolstatus;
			clockcontrolstatus_val = dhd->sssr_reg_info->rev2.
				pcie_regs.base_regs.clockcontrolstatus_val;
			break;
		case SSSR_REG_INFO_VER_1 :
		case SSSR_REG_INFO_VER_0 :
			clockrequeststatus = dhd->sssr_reg_info->rev1.
				pcie_regs.wrapper_regs.itopoobb;
			clockcontrolstatus_addr = dhd->sssr_reg_info->rev1.
				pcie_regs.base_regs.clockcontrolstatus;
			clockcontrolstatus_val = dhd->sssr_reg_info->rev1.
				pcie_regs.base_regs.clockcontrolstatus_val;
			break;
		default :
			DHD_ERROR(("invalid sssr_reg_ver"));
			return BCME_UNSUPPORTED;
	}

	/* clear request clk only if itopoobb/extrsrcreqs is non zero */
	dhd_sbreg_op(dhd, clockrequeststatus, &val, TRUE);
	if (val) {
		/* clear clockcontrolstatus */
		dhd_sbreg_op(dhd, clockcontrolstatus_addr, &clockcontrolstatus_val, FALSE);
	}
	return BCME_OK;
}

static int
dhdpcie_pcie_send_ltrsleep(dhd_pub_t *dhd)
{
	uint addr;
	uint val = 0;

	DHD_ERROR(("%s\n", __FUNCTION__));

	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	switch (dhd->sssr_reg_info->rev2.version) {
		case SSSR_REG_INFO_VER_3 :
			/* intentional fall through */
		case SSSR_REG_INFO_VER_2 :
			addr = dhd->sssr_reg_info->rev2.pcie_regs.base_regs.ltrstate;
			break;
		case SSSR_REG_INFO_VER_1 :
		case SSSR_REG_INFO_VER_0 :
			addr = dhd->sssr_reg_info->rev1.pcie_regs.base_regs.ltrstate;
			break;
		default :
			DHD_ERROR(("invalid sssr_reg_ver"));
			return BCME_UNSUPPORTED;
	}

	val = LTR_ACTIVE;
	dhd_sbreg_op(dhd, addr, &val, FALSE);

	val = LTR_SLEEP;
	dhd_sbreg_op(dhd, addr, &val, FALSE);

	return BCME_OK;
}

static int
dhdpcie_clear_clk_req(dhd_pub_t *dhd)
{
	DHD_ERROR(("%s\n", __FUNCTION__));

	dhdpcie_arm_clear_clk_req(dhd);

	dhdpcie_d11_clear_clk_req(dhd);

	dhdpcie_pcie_clear_clk_req(dhd);

	return BCME_OK;
}

static int
dhdpcie_bring_d11_outofreset(dhd_pub_t *dhd)
{
	int i;
	uint val = 0;
	uint8 num_d11cores;
	uint32 resetctrl_addr, ioctrl_addr, ioctrl_resetseq_val0, ioctrl_resetseq_val1,
	 ioctrl_resetseq_val2, ioctrl_resetseq_val3, ioctrl_resetseq_val4;

	DHD_ERROR(("%s\n", __FUNCTION__));

	num_d11cores = dhd_d11_slices_num_get(dhd);

	for (i = 0; i < num_d11cores; i++) {
		if (dhd->sssr_d11_outofreset[i]) {
			/* SSSR register information structure v0 and v1 shares
			 * most except dig_mem
			 */
			switch (dhd->sssr_reg_info->rev2.version) {
				case SSSR_REG_INFO_VER_3 :
					/* intentional fall through */
				case SSSR_REG_INFO_VER_2 :
					resetctrl_addr = dhd->sssr_reg_info->rev2.mac_regs[i].
						wrapper_regs.resetctrl;
					ioctrl_addr = dhd->sssr_reg_info->rev2.mac_regs[i].
						wrapper_regs.ioctrl;
					ioctrl_resetseq_val0 = dhd->sssr_reg_info->rev2.
						mac_regs[0].wrapper_regs.ioctrl_resetseq_val[0];
					ioctrl_resetseq_val1 = dhd->sssr_reg_info->rev2.
						mac_regs[0].wrapper_regs.ioctrl_resetseq_val[1];
					ioctrl_resetseq_val2 = dhd->sssr_reg_info->rev2.
						mac_regs[0].wrapper_regs.ioctrl_resetseq_val[2];
					ioctrl_resetseq_val3 = dhd->sssr_reg_info->rev2.
						mac_regs[0].wrapper_regs.ioctrl_resetseq_val[3];
					ioctrl_resetseq_val4 = dhd->sssr_reg_info->rev2.
						mac_regs[0].wrapper_regs.ioctrl_resetseq_val[4];
					break;
				case SSSR_REG_INFO_VER_1 :
				case SSSR_REG_INFO_VER_0 :
					resetctrl_addr = dhd->sssr_reg_info->rev1.mac_regs[i].
						wrapper_regs.resetctrl;
					ioctrl_addr = dhd->sssr_reg_info->rev1.mac_regs[i].
						wrapper_regs.ioctrl;
					ioctrl_resetseq_val0 = dhd->sssr_reg_info->rev1.
						mac_regs[0].wrapper_regs.ioctrl_resetseq_val[0];
					ioctrl_resetseq_val1 = dhd->sssr_reg_info->rev1.
						mac_regs[0].wrapper_regs.ioctrl_resetseq_val[1];
					ioctrl_resetseq_val2 = dhd->sssr_reg_info->rev1.
						mac_regs[0].wrapper_regs.ioctrl_resetseq_val[2];
					ioctrl_resetseq_val3 = dhd->sssr_reg_info->rev1.
						mac_regs[0].wrapper_regs.ioctrl_resetseq_val[3];
					ioctrl_resetseq_val4 = dhd->sssr_reg_info->rev1.
						mac_regs[0].wrapper_regs.ioctrl_resetseq_val[4];
					break;
				default :
					DHD_ERROR(("invalid sssr_reg_ver"));
					return BCME_UNSUPPORTED;
			}
			/* disable core by setting bit 0 */
			val = 1;
			dhd_sbreg_op(dhd, resetctrl_addr, &val, FALSE);
			OSL_DELAY(6000);

			dhd_sbreg_op(dhd, ioctrl_addr, &ioctrl_resetseq_val0, FALSE);

			dhd_sbreg_op(dhd, ioctrl_addr, &ioctrl_resetseq_val1, FALSE);

			/* enable core by clearing bit 0 */
			val = 0;
			dhd_sbreg_op(dhd, resetctrl_addr, &val, FALSE);

			dhd_sbreg_op(dhd, ioctrl_addr, &ioctrl_resetseq_val2, FALSE);

			dhd_sbreg_op(dhd, ioctrl_addr, &ioctrl_resetseq_val3, FALSE);

			dhd_sbreg_op(dhd, ioctrl_addr, &ioctrl_resetseq_val4, FALSE);
		}
	}
	return BCME_OK;
}

#ifdef DHD_SSSR_DUMP_BEFORE_SR
static int
dhdpcie_sssr_dump_get_before_sr(dhd_pub_t *dhd)
{
	int i;
	uint32 sr_size, xmtaddress, xmtdata, dig_buf_size, dig_buf_addr;
	uint8 num_d11cores;

	DHD_ERROR(("%s\n", __FUNCTION__));

	num_d11cores = dhd_d11_slices_num_get(dhd);

	for (i = 0; i < num_d11cores; i++) {
		if (dhd->sssr_d11_outofreset[i]) {
			sr_size = dhd_sssr_mac_buf_size(dhd, i);
			xmtaddress = dhd_sssr_mac_xmtaddress(dhd, i);
			xmtdata = dhd_sssr_mac_xmtdata(dhd, i);
			dhdpcie_get_sssr_fifo_dump(dhd, dhd->sssr_d11_before[i],
				sr_size, xmtaddress, xmtdata);
		}
	}

	dig_buf_size = dhd_sssr_dig_buf_size(dhd);
	dig_buf_addr = dhd_sssr_dig_buf_addr(dhd);
	if (dig_buf_size) {
		dhdpcie_get_sssr_dig_dump(dhd, dhd->sssr_dig_buf_before,
			dig_buf_size, dig_buf_addr);
	}

	return BCME_OK;
}
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

static int
dhdpcie_sssr_dump_get_after_sr(dhd_pub_t *dhd)
{
	int i;
	uint32 sr_size, xmtaddress, xmtdata, dig_buf_size, dig_buf_addr;
	uint8 num_d11cores;

	DHD_ERROR(("%s\n", __FUNCTION__));

	num_d11cores = dhd_d11_slices_num_get(dhd);

	for (i = 0; i < num_d11cores; i++) {
		if (dhd->sssr_d11_outofreset[i]) {
			sr_size = dhd_sssr_mac_buf_size(dhd, i);
			xmtaddress = dhd_sssr_mac_xmtaddress(dhd, i);
			xmtdata = dhd_sssr_mac_xmtdata(dhd, i);
			dhdpcie_get_sssr_fifo_dump(dhd, dhd->sssr_d11_after[i],
				sr_size, xmtaddress, xmtdata);
		}
	}

	dig_buf_size = dhd_sssr_dig_buf_size(dhd);
	dig_buf_addr = dhd_sssr_dig_buf_addr(dhd);

	if (dig_buf_size) {
		dhdpcie_get_sssr_dig_dump(dhd, dhd->sssr_dig_buf_after, dig_buf_size, dig_buf_addr);
	}

	return BCME_OK;
}

#define GCI_CHIPSTATUS_AUX	GCI_CHIPSTATUS_10
#define GCI_CHIPSTATUS_MAIN	GCI_CHIPSTATUS_11
#define GCI_CHIPSTATUS_DIG	GCI_CHIPSTATUS_12
#define GCI_CHIPSTATUS_SCAN	GCI_CHIPSTATUS_13

#define GCI_CHIPSTATUS_ILLEGAL_INSTR_BITMASK	(1u << 3)
int
dhdpcie_validate_gci_chip_intstatus(dhd_pub_t *dhd)
{
	int gci_intstatus;
	si_t *sih = dhd->bus->sih;

	/* For now validate only for 4389 chip */
	if (si_chipid(sih) != BCM4389_CHIP_ID) {
		DHD_ERROR(("%s: skipping for chipid:0x%x\n", __FUNCTION__, si_chipid(sih)));
		return BCME_OK;
	}

	gci_intstatus = si_gci_chipstatus(sih, GCI_CHIPSTATUS_MAIN);
	if (gci_intstatus & GCI_CHIPSTATUS_ILLEGAL_INSTR_BITMASK) {
		DHD_ERROR(("%s: Illegal instruction set for MAIN core 0x%x\n",
			__FUNCTION__, gci_intstatus));
		return BCME_ERROR;
	}

	gci_intstatus = si_gci_chipstatus(sih, GCI_CHIPSTATUS_AUX);
	if (gci_intstatus & GCI_CHIPSTATUS_ILLEGAL_INSTR_BITMASK) {
		DHD_ERROR(("%s: Illegal instruction set for AUX core 0x%x\n",
			__FUNCTION__, gci_intstatus));
		return BCME_ERROR;
	}

	gci_intstatus = si_gci_chipstatus(sih, GCI_CHIPSTATUS_SCAN);
	if (gci_intstatus & GCI_CHIPSTATUS_ILLEGAL_INSTR_BITMASK) {
		DHD_ERROR(("%s: Illegal instruction set for SCAN core 0x%x\n",
			__FUNCTION__, gci_intstatus));
		return BCME_ERROR;
	}

	gci_intstatus = si_gci_chipstatus(sih, GCI_CHIPSTATUS_DIG);
	if (gci_intstatus & GCI_CHIPSTATUS_ILLEGAL_INSTR_BITMASK) {
		DHD_ERROR(("%s: Illegal instruction set for DIG core 0x%x\n",
			__FUNCTION__, gci_intstatus));
		return BCME_ERROR;
	}

	return BCME_OK;
}

int
dhdpcie_sssr_dump(dhd_pub_t *dhd)
{
	uint32 powerctrl_val;

	if (!dhd->sssr_inited) {
		DHD_ERROR(("%s: SSSR not inited\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (dhd->bus->is_linkdown) {
		DHD_ERROR(("%s: PCIe link is down\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (dhd->bus->cto_triggered) {
		DHD_ERROR(("%s: CTO Triggered\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (dhdpcie_validate_gci_chip_intstatus(dhd) != BCME_OK) {
		DHD_ERROR(("%s: ## Invalid GCI Chip intstatus, Abort SSSR ##\n",
			__FUNCTION__));
		return BCME_ERROR;
	}
	DHD_ERROR(("%s: Before WL down (powerctl: pcie:0x%x chipc:0x%x) "
		"PMU rctl:0x%x res_state:0x%x\n", __FUNCTION__,
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
			OFFSETOF(chipcregs_t, powerctl), 0, 0),
		si_corereg(dhd->bus->sih, 0, OFFSETOF(chipcregs_t, powerctl), 0, 0),
		PMU_REG(dhd->bus->sih, retention_ctl, 0, 0),
		PMU_REG(dhd->bus->sih, res_state, 0, 0)));

	dhdpcie_d11_check_outofreset(dhd);

#ifdef DHD_SSSR_DUMP_BEFORE_SR
	DHD_ERROR(("%s: Collecting Dump before SR\n", __FUNCTION__));
	if (dhdpcie_sssr_dump_get_before_sr(dhd) != BCME_OK) {
		DHD_ERROR(("%s: dhdpcie_sssr_dump_get_before_sr failed\n", __FUNCTION__));
		return BCME_ERROR;
	}
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

	dhdpcie_clear_intmask_and_timer(dhd);
	dhdpcie_clear_clk_req(dhd);
	powerctrl_val = dhdpcie_suspend_chipcommon_powerctrl(dhd);
	dhdpcie_pcie_send_ltrsleep(dhd);

	if (MULTIBP_ENAB(dhd->bus->sih)) {
		dhd_bus_pcie_pwr_req_wl_domain(dhd->bus, OFFSETOF(chipcregs_t, powerctl), FALSE);
	}

	/* Wait for some time before Restore */
	OSL_DELAY(6000);

	DHD_ERROR(("%s: After WL down (powerctl: pcie:0x%x chipc:0x%x) "
		"PMU rctl:0x%x res_state:0x%x\n", __FUNCTION__,
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
			OFFSETOF(chipcregs_t, powerctl), 0, 0),
		si_corereg(dhd->bus->sih, 0, OFFSETOF(chipcregs_t, powerctl), 0, 0),
		PMU_REG(dhd->bus->sih, retention_ctl, 0, 0),
		PMU_REG(dhd->bus->sih, res_state, 0, 0)));

	if (MULTIBP_ENAB(dhd->bus->sih)) {
		dhd_bus_pcie_pwr_req_wl_domain(dhd->bus, OFFSETOF(chipcregs_t, powerctl), TRUE);
		/* Add delay for WL domain to power up */
		OSL_DELAY(15000);

		DHD_ERROR(("%s: After WL up again (powerctl: pcie:0x%x chipc:0x%x) "
			"PMU rctl:0x%x res_state:0x%x\n", __FUNCTION__,
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				OFFSETOF(chipcregs_t, powerctl), 0, 0),
			si_corereg(dhd->bus->sih, 0, OFFSETOF(chipcregs_t, powerctl), 0, 0),
			PMU_REG(dhd->bus->sih, retention_ctl, 0, 0),
			PMU_REG(dhd->bus->sih, res_state, 0, 0)));
	}

	dhdpcie_resume_chipcommon_powerctrl(dhd, powerctrl_val);
	dhdpcie_arm_resume_clk_req(dhd);
	dhdpcie_bring_d11_outofreset(dhd);

	DHD_ERROR(("%s: Collecting Dump after SR\n", __FUNCTION__));
	if (dhdpcie_sssr_dump_get_after_sr(dhd) != BCME_OK) {
		DHD_ERROR(("%s: dhdpcie_sssr_dump_get_after_sr failed\n", __FUNCTION__));
		return BCME_ERROR;
	}
	dhd->sssr_dump_collected = TRUE;
	dhd_write_sssr_dump(dhd, SSSR_DUMP_MODE_SSSR);

	return BCME_OK;
}

#define PCIE_CFG_DSTATE_MASK	0x11u

static int
dhdpcie_fis_trigger(dhd_pub_t *dhd)
{
	uint32 fis_ctrl_status, fis_min_res_mask;
	uint32 cfg_status_cmd;
	uint32 cfg_pmcsr;

	if (!dhd->sssr_inited) {
		DHD_ERROR(("%s: SSSR not inited\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (dhd->bus->is_linkdown) {
		DHD_ERROR(("%s: PCIe link is down\n", __FUNCTION__));
		return BCME_ERROR;
	}

	/* Set fis_triggered flag to ignore link down callback from RC */
	dhd->fis_triggered = TRUE;

	/* set fis_min_res_mask to 0x1 */
	PMU_REG(dhd->bus->sih, fis_min_res_mask, ~0, 0x1);

	/* Set FIS PwrswForceOnAll */
	PMU_REG(dhd->bus->sih, fis_ctrl_status, PMU_FIS_FORCEON_ALL_MASK, PMU_FIS_FORCEON_ALL_MASK);

	fis_ctrl_status = PMU_REG(dhd->bus->sih, fis_ctrl_status, 0, 0);
	fis_min_res_mask = PMU_REG(dhd->bus->sih, fis_min_res_mask, 0, 0);
	DHD_ERROR(("%s: fis_ctrl_status=0x%x fis_min_res_mask=0x%x\n",
		__FUNCTION__, fis_ctrl_status, fis_min_res_mask));

	cfg_status_cmd = dhd_pcie_config_read(dhd->bus, PCIECFGREG_STATUS_CMD, sizeof(uint32));
	cfg_pmcsr = dhd_pcie_config_read(dhd->bus, PCIE_CFG_PMCSR, sizeof(uint32));
	DHD_ERROR(("before save: Status Command(0x%x)=0x%x PCIE_CFG_PMCSR(0x%x)=0x%x\n",
		PCIECFGREG_STATUS_CMD, cfg_status_cmd, PCIE_CFG_PMCSR, cfg_pmcsr));

	DHD_PCIE_CONFIG_SAVE(dhd->bus);

	/* Trigger FIS */
	si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
		DAR_FIS_CTRL(dhd->bus->sih->buscorerev), ~0, DAR_FIS_START_MASK);
	OSL_DELAY(100 * 1000);

#ifdef OEM_ANDROID
	/*
	 * For android built-in platforms need to perform REG ON/OFF
	 * to restore pcie link.
	 * dhd_download_fw_on_driverload will be FALSE for built-in.
	 */
	if (!dhd_download_fw_on_driverload) {
		DHD_ERROR(("%s: Toggle REG_ON and restore config space\n", __FUNCTION__));
		dhdpcie_bus_stop_host_dev(dhd->bus);
		dhd_wifi_platform_set_power(dhd, FALSE);
		dhd_wifi_platform_set_power(dhd, TRUE);
		dhdpcie_bus_start_host_dev(dhd->bus);
		/* Restore inited pcie cfg from pci_load_saved_state */
		dhdpcie_bus_enable_device(dhd->bus);
	}
#endif /* OEM_ANDROID */

	cfg_status_cmd = dhd_pcie_config_read(dhd->bus, PCIECFGREG_STATUS_CMD, sizeof(uint32));
	cfg_pmcsr = dhd_pcie_config_read(dhd->bus, PCIE_CFG_PMCSR, sizeof(uint32));
	DHD_ERROR(("after regon-restore: Status Command(0x%x)=0x%x PCIE_CFG_PMCSR(0x%x)=0x%x\n",
		PCIECFGREG_STATUS_CMD, cfg_status_cmd, PCIE_CFG_PMCSR, cfg_pmcsr));

	/* To-Do: below is debug code, remove this if EP is in D0 after REG-ON restore */
	DHD_PCIE_CONFIG_RESTORE(dhd->bus);

	cfg_status_cmd = dhd_pcie_config_read(dhd->bus, PCIECFGREG_STATUS_CMD, sizeof(uint32));
	cfg_pmcsr = dhd_pcie_config_read(dhd->bus, PCIE_CFG_PMCSR, sizeof(uint32));
	DHD_ERROR(("after normal-restore: Status Command(0x%x)=0x%x PCIE_CFG_PMCSR(0x%x)=0x%x\n",
		PCIECFGREG_STATUS_CMD, cfg_status_cmd, PCIE_CFG_PMCSR, cfg_pmcsr));

	/*
	 * To-Do: below is debug code, remove this if EP is in D0 after REG-ON restore
	 * in both MSM and LSI RCs
	 */
	if ((cfg_pmcsr & PCIE_CFG_DSTATE_MASK) != 0) {
		int ret = dhdpcie_set_master_and_d0_pwrstate(dhd->bus);
		if (ret != BCME_OK) {
			DHD_ERROR(("%s: Setting D0 failed, ABORT FIS collection\n", __FUNCTION__));
			return ret;
		}
		cfg_status_cmd =
			dhd_pcie_config_read(dhd->bus, PCIECFGREG_STATUS_CMD, sizeof(uint32));
		cfg_pmcsr = dhd_pcie_config_read(dhd->bus, PCIE_CFG_PMCSR, sizeof(uint32));
		DHD_ERROR(("after force-d0: Status Command(0x%x)=0x%x PCIE_CFG_PMCSR(0x%x)=0x%x\n",
			PCIECFGREG_STATUS_CMD, cfg_status_cmd, PCIE_CFG_PMCSR, cfg_pmcsr));
	}

	/* Clear fis_triggered as REG OFF/ON recovered link */
	dhd->fis_triggered = FALSE;

	return BCME_OK;
}

int
dhd_bus_fis_trigger(dhd_pub_t *dhd)
{
	return dhdpcie_fis_trigger(dhd);
}

static int
dhdpcie_reset_hwa(dhd_pub_t *dhd)
{
	int ret;
	sssr_reg_info_cmn_t *sssr_reg_info_cmn = dhd->sssr_reg_info;
	sssr_reg_info_v3_t *sssr_reg_info = (sssr_reg_info_v3_t *)&sssr_reg_info_cmn->rev3;

	/* HWA wrapper registers */
	uint32 ioctrl, resetctrl;
	/* HWA base registers */
	uint32 clkenable, clkgatingenable, clkext, clkctlstatus;
	uint32 hwa_resetseq_val[SSSR_HWA_RESET_SEQ_STEPS];
	int i = 0;

	if (sssr_reg_info->version < SSSR_REG_INFO_VER_3) {
		DHD_ERROR(("%s: not supported for version:%d\n",
			__FUNCTION__, sssr_reg_info->version));
		return BCME_UNSUPPORTED;
	}

	if (sssr_reg_info->hwa_regs.base_regs.clkenable == 0) {
		DHD_ERROR(("%s: hwa regs are not set\n", __FUNCTION__));
		return BCME_UNSUPPORTED;
	}

	DHD_ERROR(("%s: version:%d\n", __FUNCTION__, sssr_reg_info->version));

	ioctrl = sssr_reg_info->hwa_regs.wrapper_regs.ioctrl;
	resetctrl = sssr_reg_info->hwa_regs.wrapper_regs.resetctrl;

	clkenable = sssr_reg_info->hwa_regs.base_regs.clkenable;
	clkgatingenable = sssr_reg_info->hwa_regs.base_regs.clkgatingenable;
	clkext = sssr_reg_info->hwa_regs.base_regs.clkext;
	clkctlstatus = sssr_reg_info->hwa_regs.base_regs.clkctlstatus;

	ret = memcpy_s(hwa_resetseq_val, sizeof(hwa_resetseq_val),
		sssr_reg_info->hwa_regs.hwa_resetseq_val,
		sizeof(sssr_reg_info->hwa_regs.hwa_resetseq_val));
	if (ret) {
		DHD_ERROR(("%s: hwa_resetseq_val memcpy_s failed: %d\n",
			__FUNCTION__, ret));
		return ret;
	}

	dhd_sbreg_op(dhd, ioctrl, &hwa_resetseq_val[i++], FALSE);
	dhd_sbreg_op(dhd, resetctrl, &hwa_resetseq_val[i++], FALSE);
	dhd_sbreg_op(dhd, resetctrl, &hwa_resetseq_val[i++], FALSE);
	dhd_sbreg_op(dhd, ioctrl, &hwa_resetseq_val[i++], FALSE);

	dhd_sbreg_op(dhd, clkenable, &hwa_resetseq_val[i++], FALSE);
	dhd_sbreg_op(dhd, clkgatingenable, &hwa_resetseq_val[i++], FALSE);
	dhd_sbreg_op(dhd, clkext, &hwa_resetseq_val[i++], FALSE);
	dhd_sbreg_op(dhd, clkctlstatus, &hwa_resetseq_val[i++], FALSE);

	return BCME_OK;
}

static int
dhdpcie_fis_dump(dhd_pub_t *dhd)
{
	int i;
	uint8 num_d11cores;
	struct dhd_bus *bus = dhd->bus;
	uint32 save_idx = 0;

	DHD_ERROR(("%s\n", __FUNCTION__));

	if (!dhd->sssr_inited) {
		DHD_ERROR(("%s: SSSR not inited\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (dhd->bus->is_linkdown) {
		DHD_ERROR(("%s: PCIe link is down\n", __FUNCTION__));
		return BCME_ERROR;
	}

	/* bring up all pmu resources */
	PMU_REG(dhd->bus->sih, min_res_mask, ~0,
		PMU_REG(dhd->bus->sih, max_res_mask, 0, 0));
	OSL_DELAY(10 * 1000);

	num_d11cores = dhd_d11_slices_num_get(dhd);

	for (i = 0; i < num_d11cores; i++) {
		dhd->sssr_d11_outofreset[i] = TRUE;
	}

	dhdpcie_bring_d11_outofreset(dhd);
	OSL_DELAY(6000);

	/* clear FIS Done */
	PMU_REG(dhd->bus->sih, fis_ctrl_status, PMU_CLEAR_FIS_DONE_MASK, PMU_CLEAR_FIS_DONE_MASK);

	if (dhdpcie_reset_hwa(dhd) != BCME_OK) {
		DHD_ERROR(("%s: dhdpcie_reset_hwa failed\n", __FUNCTION__));
		return BCME_ERROR;
	}

	dhdpcie_d11_check_outofreset(dhd);

	/* take sysmem out of reset - otherwise
	 * socram collected again will read only
	 * 0xffff
	 */
	save_idx = si_coreidx(bus->sih);
	if (si_setcore(bus->sih, SYSMEM_CORE_ID, 0)) {
		si_core_reset(bus->sih, 0, 0);
		si_setcoreidx(bus->sih, save_idx);
	}

	DHD_ERROR(("%s: Collecting Dump after SR\n", __FUNCTION__));
	if (dhdpcie_sssr_dump_get_after_sr(dhd) != BCME_OK) {
		DHD_ERROR(("%s: dhdpcie_sssr_dump_get_after_sr failed\n", __FUNCTION__));
		return BCME_ERROR;
	}
	dhd->sssr_dump_collected = TRUE;
	dhd_write_sssr_dump(dhd, SSSR_DUMP_MODE_FIS);

	if (dhd->bus->link_state != DHD_PCIE_ALL_GOOD) {
		/* reset link state and collect socram */
		dhd->bus->link_state = DHD_PCIE_ALL_GOOD;
		DHD_ERROR(("%s: recollect socram\n", __FUNCTION__));
		/* re-read socram into buffer */
		dhdpcie_get_mem_dump(bus);
	}

	return BCME_OK;
}

int
dhd_bus_fis_dump(dhd_pub_t *dhd)
{
	return dhdpcie_fis_dump(dhd);
}
#endif /* DHD_SSSR_DUMP */

#ifdef DHD_SDTC_ETB_DUMP
int
dhd_bus_get_etb_info(dhd_pub_t *dhd, uint32 etbinfo_addr, etb_info_t *etb_info)
{

	int ret = 0;

	if ((ret = dhdpcie_bus_membytes(dhd->bus, FALSE, etbinfo_addr,
		(unsigned char *)etb_info, sizeof(*etb_info)))) {
		DHD_ERROR(("%s: Read Error membytes %d\n", __FUNCTION__, ret));
		return BCME_ERROR;
	}

	return BCME_OK;
}

int
dhd_bus_get_sdtc_etb(dhd_pub_t *dhd, uint8 *sdtc_etb_mempool, uint addr, uint read_bytes)
{
	int ret = 0;

	if ((ret = dhdpcie_bus_membytes(dhd->bus, FALSE, addr,
		(unsigned char *)sdtc_etb_mempool, read_bytes))) {
		DHD_ERROR(("%s: Read Error membytes %d\n", __FUNCTION__, ret));
		return BCME_ERROR;
	}
	return BCME_OK;
}
#endif /* DHD_SDTC_ETB_DUMP */

#ifdef DHD_WAKE_STATUS
wake_counts_t*
dhd_bus_get_wakecount(dhd_pub_t *dhd)
{
	return &dhd->bus->wake_counts;
}
int
dhd_bus_get_bus_wake(dhd_pub_t *dhd)
{
	return bcmpcie_get_wake(dhd->bus);
}
int
dhd_bus_set_get_bus_wake(dhd_pub_t *dhd, int set)
{
	return bcmpcie_set_get_wake(dhd->bus, set);
}

int
dhd_bus_set_get_bus_wake_pkt_dump(dhd_pub_t *dhd, int wake_pkt_dump)
{
	return bcmpcie_set_get_wake_pkt_dump(dhd->bus, wake_pkt_dump);
}
#endif /* DHD_WAKE_STATUS */

/* Writes random number(s) to the TCM. FW upon initialization reads this register
 * to fetch the random number, and uses it to randomize heap address space layout.
 */
static int
dhdpcie_wrt_rnd(struct dhd_bus *bus)
{
	uint8 rand_buf[BCM_ENTROPY_HOST_NBYTES];
	int ret = BCME_OK;
	memset(rand_buf, 0, sizeof(rand_buf));

	/* Now write the random number(s) */
	ret = dhd_get_random_bytes(rand_buf, sizeof(rand_buf));
	if (ret != BCME_OK) {
		return ret;
	}
	dhdpcie_download_rtlv(bus, DNGL_RTLV_TYPE_RNG_SIGNATURE, sizeof(rand_buf), rand_buf);

	return BCME_OK;
}

#ifdef D2H_MINIDUMP
bool
dhd_bus_is_minidump_enabled(dhd_pub_t *dhdp)
{
	return dhdp->bus->d2h_minidump;
}
#endif /* D2H_MINIDUMP */

void
dhd_pcie_intr_count_dump(dhd_pub_t *dhd)
{
	struct dhd_bus *bus = dhd->bus;
	uint64 current_time;

	DHD_ERROR(("\n ------- DUMPING INTR enable/disable counters  ------- \r\n"));
	DHD_ERROR(("dngl_intmask_enable_count=%lu host_irq_enable_count=%lu\n",
		bus->dngl_intmask_enable_count, bus->host_irq_enable_count));
	DHD_ERROR(("host_irq_disable_count=%lu dngl_intmask_disable_count=%lu\n",
		bus->host_irq_disable_count, bus->dngl_intmask_disable_count));
#ifdef BCMPCIE_OOB_HOST_WAKE
	DHD_ERROR(("oob_intr_count=%lu oob_intr_enable_count=%lu oob_intr_disable_count=%lu\n",
		bus->oob_intr_count, bus->oob_intr_enable_count,
		bus->oob_intr_disable_count));
	DHD_ERROR(("oob_irq_num=%d last_oob_irq_times="SEC_USEC_FMT":"SEC_USEC_FMT"\n",
		dhdpcie_get_oob_irq_num(bus),
		GET_SEC_USEC(bus->last_oob_irq_isr_time),
		GET_SEC_USEC(bus->last_oob_irq_thr_time)));
	DHD_ERROR(("last_oob_irq_enable_time="SEC_USEC_FMT
		" last_oob_irq_disable_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(bus->last_oob_irq_enable_time),
		GET_SEC_USEC(bus->last_oob_irq_disable_time)));
	DHD_ERROR(("oob_irq_enabled=%d oob_gpio_level=%d\n",
		dhdpcie_get_oob_irq_status(bus),
		dhdpcie_get_oob_irq_level()));

#if defined(__linux__)
	dhd_plat_pin_dbg_show(bus->dhd->plat_info);
#endif /* __linux__ */
#endif /* BCMPCIE_OOB_HOST_WAKE */
	DHD_ERROR(("dpc_return_busdown_count=%lu non_ours_irq_count=%lu\n",
		bus->dpc_return_busdown_count, bus->non_ours_irq_count));

	current_time = OSL_LOCALTIME_NS();
	DHD_ERROR(("\ncurrent_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(current_time)));
	DHD_ERROR(("isr_entry_time="SEC_USEC_FMT
		" prev_isr_entry_time="SEC_USEC_FMT
		" isr_exit_time="SEC_USEC_FMT" dpc_sched=%u\n",
		GET_SEC_USEC(bus->isr_entry_time),
		GET_SEC_USEC(bus->prev_isr_entry_time),
		GET_SEC_USEC(bus->isr_exit_time), bus->dpc_sched));
	DHD_ERROR(("isr_sched_dpc_time="SEC_USEC_FMT
		" rpm_sched_dpc_time="SEC_USEC_FMT
		" last_non_ours_irq_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(bus->isr_sched_dpc_time),
		GET_SEC_USEC(bus->rpm_sched_dpc_time),
		GET_SEC_USEC(bus->last_non_ours_irq_time)));
	DHD_ERROR(("dpc_entry_time="SEC_USEC_FMT
		" last_process_ctrlbuf_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(bus->dpc_entry_time),
		GET_SEC_USEC(bus->last_process_ctrlbuf_time)));
	DHD_ERROR(("last_process_flowring_time="SEC_USEC_FMT
		" last_process_txcpl_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(bus->last_process_flowring_time),
		GET_SEC_USEC(bus->last_process_txcpl_time)));
	DHD_ERROR(("last_process_rxcpl_time="SEC_USEC_FMT
		" last_process_infocpl_time="SEC_USEC_FMT
		" last_process_edl_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(bus->last_process_rxcpl_time),
		GET_SEC_USEC(bus->last_process_infocpl_time),
		GET_SEC_USEC(bus->last_process_edl_time)));
	DHD_ERROR(("dpc_exit_time="SEC_USEC_FMT
		" resched_dpc_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(bus->dpc_exit_time),
		GET_SEC_USEC(bus->resched_dpc_time)));
	DHD_ERROR(("last_d3_inform_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(bus->last_d3_inform_time)));

	DHD_ERROR(("\nlast_suspend_start_time="SEC_USEC_FMT
		" last_suspend_end_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(bus->last_suspend_start_time),
		GET_SEC_USEC(bus->last_suspend_end_time)));
	DHD_ERROR(("last_resume_start_time="SEC_USEC_FMT
		" last_resume_end_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(bus->last_resume_start_time),
		GET_SEC_USEC(bus->last_resume_end_time)));

#if defined(SHOW_LOGTRACE) && defined(DHD_USE_KTHREAD_FOR_LOGTRACE)
	DHD_ERROR(("logtrace_thread_entry_time="SEC_USEC_FMT
		" logtrace_thread_sem_down_time="SEC_USEC_FMT
		"\nlogtrace_thread_flush_time="SEC_USEC_FMT
		" logtrace_thread_unexpected_break_time="SEC_USEC_FMT
		"\nlogtrace_thread_complete_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(dhd->logtrace_thr_ts.entry_time),
		GET_SEC_USEC(dhd->logtrace_thr_ts.sem_down_time),
		GET_SEC_USEC(dhd->logtrace_thr_ts.flush_time),
		GET_SEC_USEC(dhd->logtrace_thr_ts.unexpected_break_time),
		GET_SEC_USEC(dhd->logtrace_thr_ts.complete_time)));
#endif /* SHOW_LOGTRACE && DHD_USE_KTHREAD_FOR_LOGTRACE */
}

void
dhd_bus_intr_count_dump(dhd_pub_t *dhd)
{
	dhd_pcie_intr_count_dump(dhd);
}

int
dhd_pcie_dump_wrapper_regs(dhd_pub_t *dhd)
{
	uint32 save_idx, val;
	si_t *sih = dhd->bus->sih;
	uint32 oob_base, oob_base1;
	uint32 wrapper_dump_list[] = {
		AI_OOBSELOUTA30, AI_OOBSELOUTA74, AI_OOBSELOUTB30, AI_OOBSELOUTB74,
		AI_OOBSELOUTC30, AI_OOBSELOUTC74, AI_OOBSELOUTD30, AI_OOBSELOUTD74,
		AI_RESETSTATUS, AI_RESETCTRL,
		AI_ITIPOOBA, AI_ITIPOOBB, AI_ITIPOOBC, AI_ITIPOOBD,
		AI_ITIPOOBAOUT, AI_ITIPOOBBOUT, AI_ITIPOOBCOUT, AI_ITIPOOBDOUT
	};
	uint32 i;
	hndoobr_reg_t *reg;
	cr4regs_t *cr4regs;
	ca7regs_t *ca7regs;

	save_idx = si_coreidx(sih);

	DHD_ERROR(("%s: Master wrapper Reg\n", __FUNCTION__));

	if (si_setcore(sih, PCIE2_CORE_ID, 0) != NULL) {
		for (i = 0; i < (uint32)sizeof(wrapper_dump_list) / 4; i++) {
			val = si_wrapperreg(sih, wrapper_dump_list[i], 0, 0);
			DHD_ERROR(("sbreg: addr:0x%x val:0x%x\n", wrapper_dump_list[i], val));
		}
	}

	dhd_dump_pcie_slave_wrapper_regs(dhd->bus);

	if ((cr4regs = si_setcore(sih, ARMCR4_CORE_ID, 0)) != NULL) {
		DHD_ERROR(("%s: ARM CR4 wrapper Reg\n", __FUNCTION__));
		for (i = 0; i < (uint32)sizeof(wrapper_dump_list) / 4; i++) {
			val = si_wrapperreg(sih, wrapper_dump_list[i], 0, 0);
			DHD_ERROR(("sbreg: addr:0x%x val:0x%x\n", wrapper_dump_list[i], val));
		}
		DHD_ERROR(("%s: ARM CR4 core Reg\n", __FUNCTION__));
		val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, corecontrol));
		DHD_ERROR(("reg:0x%x val:0x%x\n", (uint)OFFSETOF(cr4regs_t, corecontrol), val));
		val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, corecapabilities));
		DHD_ERROR(("reg:0x%x val:0x%x\n",
			(uint)OFFSETOF(cr4regs_t, corecapabilities), val));
		val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, corestatus));
		DHD_ERROR(("reg:0x%x val:0x%x\n", (uint)OFFSETOF(cr4regs_t, corestatus), val));
		val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, nmiisrst));
		DHD_ERROR(("reg:0x%x val:0x%x\n", (uint)OFFSETOF(cr4regs_t, nmiisrst), val));
		val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, nmimask));
		DHD_ERROR(("reg:0x%x val:0x%x\n", (uint)OFFSETOF(cr4regs_t, nmimask), val));
		val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, isrmask));
		DHD_ERROR(("reg:0x%x val:0x%x\n", (uint)OFFSETOF(cr4regs_t, isrmask), val));
		val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, swintreg));
		DHD_ERROR(("reg:0x%x val:0x%x\n", (uint)OFFSETOF(cr4regs_t, swintreg), val));
		val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, intstatus));
		DHD_ERROR(("reg:0x%x val:0x%x\n", (uint)OFFSETOF(cr4regs_t, intstatus), val));
		val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, cyclecnt));
		DHD_ERROR(("reg:0x%x val:0x%x\n", (uint)OFFSETOF(cr4regs_t, cyclecnt), val));
		val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, inttimer));
		DHD_ERROR(("reg:0x%x val:0x%x\n", (uint)OFFSETOF(cr4regs_t, inttimer), val));
		val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, clk_ctl_st));
		DHD_ERROR(("reg:0x%x val:0x%x\n", (uint)OFFSETOF(cr4regs_t, clk_ctl_st), val));
		val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, powerctl));
		DHD_ERROR(("reg:0x%x val:0x%x\n", (uint)OFFSETOF(cr4regs_t, powerctl), val));
	}
	/* XXX: Currently dumping CA7 registers causing CTO, temporarily disabling it */
	BCM_REFERENCE(ca7regs);

	DHD_ERROR(("%s: OOBR Reg\n", __FUNCTION__));

	oob_base = si_oobr_baseaddr(sih, FALSE);
	oob_base1 = si_oobr_baseaddr(sih, TRUE);
	if (oob_base) {
		dhd_sbreg_op(dhd, oob_base + OOB_STATUSA, &val, TRUE);
		dhd_sbreg_op(dhd, oob_base + OOB_STATUSB, &val, TRUE);
		dhd_sbreg_op(dhd, oob_base + OOB_STATUSC, &val, TRUE);
		dhd_sbreg_op(dhd, oob_base + OOB_STATUSD, &val, TRUE);
	} else if ((reg = si_setcore(sih, HND_OOBR_CORE_ID, 0)) != NULL) {
		val = R_REG(dhd->osh, &reg->intstatus[0]);
		DHD_ERROR(("reg: addr:%p val:0x%x\n", reg, val));
		val = R_REG(dhd->osh, &reg->intstatus[1]);
		DHD_ERROR(("reg: addr:%p val:0x%x\n", reg, val));
		val = R_REG(dhd->osh, &reg->intstatus[2]);
		DHD_ERROR(("reg: addr:%p val:0x%x\n", reg, val));
		val = R_REG(dhd->osh, &reg->intstatus[3]);
		DHD_ERROR(("reg: addr:%p val:0x%x\n", reg, val));
	}

	if (oob_base1) {
		DHD_ERROR(("%s: Second OOBR Reg\n", __FUNCTION__));

		dhd_sbreg_op(dhd, oob_base1 + OOB_STATUSA, &val, TRUE);
		dhd_sbreg_op(dhd, oob_base1 + OOB_STATUSB, &val, TRUE);
		dhd_sbreg_op(dhd, oob_base1 + OOB_STATUSC, &val, TRUE);
		dhd_sbreg_op(dhd, oob_base1 + OOB_STATUSD, &val, TRUE);
	}

	si_setcoreidx(dhd->bus->sih, save_idx);

	return 0;
}

static void
dhdpcie_hw_war_regdump(dhd_bus_t *bus)
{
	uint32 save_idx, val;
	volatile uint32 *reg;

	save_idx = si_coreidx(bus->sih);
	if ((reg = si_setcore(bus->sih, CC_CORE_ID, 0)) != NULL) {
		val = R_REG(bus->osh, reg + REG_WORK_AROUND);
		DHD_ERROR(("CC HW_WAR :0x%x\n", val));
	}

	if ((reg = si_setcore(bus->sih, ARMCR4_CORE_ID, 0)) != NULL) {
		val = R_REG(bus->osh, reg + REG_WORK_AROUND);
		DHD_ERROR(("ARM HW_WAR:0x%x\n", val));
	}

	if ((reg = si_setcore(bus->sih, PCIE2_CORE_ID, 0)) != NULL) {
		val = R_REG(bus->osh, reg + REG_WORK_AROUND);
		DHD_ERROR(("PCIE HW_WAR :0x%x\n", val));
	}
	si_setcoreidx(bus->sih, save_idx);

	val = PMU_REG_NEW(bus->sih, min_res_mask, 0, 0);
	DHD_ERROR(("MINRESMASK :0x%x\n", val));
}

int
dhd_pcie_dma_info_dump(dhd_pub_t *dhd)
{
	if (dhd->bus->is_linkdown) {
		DHD_ERROR(("\n ------- SKIP DUMPING DMA Registers "
			"due to PCIe link down ------- \r\n"));
		return 0;
	}

	DHD_ERROR(("\n ------- DUMPING DMA Registers ------- \r\n"));

	//HostToDev
	DHD_ERROR(("HostToDev TX: XmtCtrl=0x%08x XmtPtr=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x200, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x204, 0, 0)));
	DHD_ERROR(("            : XmtAddrLow=0x%08x XmtAddrHigh=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x208, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x20C, 0, 0)));
	DHD_ERROR(("            : XmtStatus0=0x%08x XmtStatus1=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x210, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x214, 0, 0)));

	DHD_ERROR(("HostToDev RX: RcvCtrl=0x%08x RcvPtr=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x220, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x224, 0, 0)));
	DHD_ERROR(("            : RcvAddrLow=0x%08x RcvAddrHigh=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x228, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x22C, 0, 0)));
	DHD_ERROR(("            : RcvStatus0=0x%08x RcvStatus1=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x230, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x234, 0, 0)));

	//DevToHost
	DHD_ERROR(("DevToHost TX: XmtCtrl=0x%08x XmtPtr=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x240, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x244, 0, 0)));
	DHD_ERROR(("            : XmtAddrLow=0x%08x XmtAddrHigh=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x248, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x24C, 0, 0)));
	DHD_ERROR(("            : XmtStatus0=0x%08x XmtStatus1=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x250, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x254, 0, 0)));

	DHD_ERROR(("DevToHost RX: RcvCtrl=0x%08x RcvPtr=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x260, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x264, 0, 0)));
	DHD_ERROR(("            : RcvAddrLow=0x%08x RcvAddrHigh=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x268, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x26C, 0, 0)));
	DHD_ERROR(("            : RcvStatus0=0x%08x RcvStatus1=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x270, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x274, 0, 0)));

	return 0;
}

bool
dhd_pcie_check_lps_d3_acked(dhd_pub_t *dhd)
{
	return DHD_CHK_BUS_LPS_D3_ACKED(dhd->bus);
}

bool
dhd_pcie_dump_int_regs(dhd_pub_t *dhd)
{
	uint32 intstatus = 0;
	uint32 intmask = 0;
	uint32 d2h_db0 = 0;
	uint32 d2h_mb_data = 0;

	DHD_ERROR(("\n ------- DUMPING INTR Status and Masks ------- \r\n"));
	intstatus = si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
		dhd->bus->pcie_mailbox_int, 0, 0);
	if (intstatus == (uint32)-1) {
		DHD_ERROR(("intstatus=0x%x \n", intstatus));
		return FALSE;
	}

	intmask = si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
		dhd->bus->pcie_mailbox_mask, 0, 0);
	if (intmask == (uint32) -1) {
		DHD_ERROR(("intstatus=0x%x intmask=0x%x \n", intstatus, intmask));
		return FALSE;
	}

	d2h_db0 = si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
		PCID2H_MailBox, 0, 0);
	if (d2h_db0 == (uint32)-1) {
		DHD_ERROR(("intstatus=0x%x intmask=0x%x d2h_db0=0x%x\n",
		intstatus, intmask, d2h_db0));
		return FALSE;
	}

	DHD_ERROR(("intstatus=0x%x intmask=0x%x d2h_db0=0x%x\n",
		intstatus, intmask, d2h_db0));
	dhd_bus_cmn_readshared(dhd->bus, &d2h_mb_data, D2H_MB_DATA, 0);
	DHD_ERROR(("d2h_mb_data=0x%x def_intmask=0x%x \r\n", d2h_mb_data,
		dhd->bus->def_intmask));

	return TRUE;
}

void
dhd_pcie_dump_rc_conf_space_cap(dhd_pub_t *dhd)
{
	DHD_ERROR(("\n ------- DUMPING PCIE RC config space Registers ------- \r\n"));
	DHD_ERROR(("Pcie RC Uncorrectable Error Status Val=0x%x\n",
		dhdpcie_rc_access_cap(dhd->bus, PCIE_EXTCAP_ID_ERR,
		PCIE_EXTCAP_AER_UCERR_OFFSET, TRUE, FALSE, 0)));
#ifdef EXTENDED_PCIE_DEBUG_DUMP
	DHD_ERROR(("hdrlog0 =0x%08x hdrlog1 =0x%08x hdrlog2 =0x%08x hdrlog3 =0x%08x\n",
		dhdpcie_rc_access_cap(dhd->bus, PCIE_EXTCAP_ID_ERR,
		PCIE_EXTCAP_ERR_HEADER_LOG_0, TRUE, FALSE, 0),
		dhdpcie_rc_access_cap(dhd->bus, PCIE_EXTCAP_ID_ERR,
		PCIE_EXTCAP_ERR_HEADER_LOG_1, TRUE, FALSE, 0),
		dhdpcie_rc_access_cap(dhd->bus, PCIE_EXTCAP_ID_ERR,
		PCIE_EXTCAP_ERR_HEADER_LOG_2, TRUE, FALSE, 0),
		dhdpcie_rc_access_cap(dhd->bus, PCIE_EXTCAP_ID_ERR,
		PCIE_EXTCAP_ERR_HEADER_LOG_3, TRUE, FALSE, 0)));
#endif /* EXTENDED_PCIE_DEBUG_DUMP */
}

int
dhd_pcie_debug_info_dump(dhd_pub_t *dhd)
{
	int host_irq_disabled;

	DHD_ERROR(("bus->bus_low_power_state = %d\n", dhd->bus->bus_low_power_state));
	host_irq_disabled = dhdpcie_irq_disabled(dhd->bus);
	DHD_ERROR(("host pcie_irq disabled = %d\n", host_irq_disabled));
	dhd_print_tasklet_status(dhd);
	dhd_pcie_intr_count_dump(dhd);

#if defined(LINUX) || defined(linux)
	DHD_ERROR(("\n ------- DUMPING PCIE EP Resouce Info ------- \r\n"));
	dhdpcie_dump_resource(dhd->bus);
#endif /* LINUX || linux */

	dhd_pcie_dump_rc_conf_space_cap(dhd);

	DHD_ERROR(("RootPort PCIe linkcap=0x%08x\n",
		dhd_debug_get_rc_linkcap(dhd->bus)));
	DHD_ERROR(("\n ------- DUMPING PCIE EP config space Registers ------- \r\n"));
	/* XXX: hwnbu-twiki.sj.broadcom.com/bin/view/Mwgroup/CurrentPcieGen2ProgramGuide */
	dhd_bus_dump_imp_cfg_registers(dhd->bus);
#ifdef EXTENDED_PCIE_DEBUG_DUMP
	DHD_ERROR(("Pcie EP Uncorrectable Error Status Val=0x%x\n",
		dhdpcie_ep_access_cap(dhd->bus, PCIE_EXTCAP_ID_ERR,
		PCIE_EXTCAP_AER_UCERR_OFFSET, TRUE, FALSE, 0)));
	DHD_ERROR(("hdrlog0(0x%x)=0x%08x hdrlog1(0x%x)=0x%08x hdrlog2(0x%x)=0x%08x "
		"hdrlog3(0x%x)=0x%08x\n", PCI_TLP_HDR_LOG1,
		dhd_pcie_config_read(dhd->bus, PCI_TLP_HDR_LOG1, sizeof(uint32)),
		PCI_TLP_HDR_LOG2,
		dhd_pcie_config_read(dhd->bus, PCI_TLP_HDR_LOG2, sizeof(uint32)),
		PCI_TLP_HDR_LOG3,
		dhd_pcie_config_read(dhd->bus, PCI_TLP_HDR_LOG3, sizeof(uint32)),
		PCI_TLP_HDR_LOG4,
		dhd_pcie_config_read(dhd->bus, PCI_TLP_HDR_LOG4, sizeof(uint32))));
	if (dhd->bus->sih->buscorerev >= 24) {
		DHD_ERROR(("DeviceStatusControl(0x%x)=0x%x SubsystemControl(0x%x)=0x%x "
			"L1SSControl2(0x%x)=0x%x\n", PCIECFGREG_DEV_STATUS_CTRL,
			dhd_pcie_config_read(dhd->bus, PCIECFGREG_DEV_STATUS_CTRL,
			sizeof(uint32)), PCIE_CFG_SUBSYSTEM_CONTROL,
			dhd_pcie_config_read(dhd->bus, PCIE_CFG_SUBSYSTEM_CONTROL,
			sizeof(uint32)), PCIECFGREG_PML1_SUB_CTRL2,
			dhd_pcie_config_read(dhd->bus, PCIECFGREG_PML1_SUB_CTRL2,
			sizeof(uint32))));
		dhd_bus_dump_dar_registers(dhd->bus);
	}
#endif /* EXTENDED_PCIE_DEBUG_DUMP */

	if (dhd->bus->is_linkdown) {
		DHD_ERROR(("Skip dumping the PCIe Core registers. link may be DOWN\n"));
		return 0;
	}

	if (MULTIBP_ENAB(dhd->bus->sih)) {
		dhd_bus_pcie_pwr_req(dhd->bus);
	}

	dhd_pcie_dump_wrapper_regs(dhd);

	if (dhd->bus->dar_err_set) {
		DHD_ERROR(("Skip dumping the PCIe Core registers. DAR error log set\n"));
		goto exit;
	}

	DHD_ERROR(("\n ------- DUMPING PCIE core Registers ------- \r\n"));
	/* XXX: hwnbu-twiki.sj.broadcom.com/twiki/pub/Mwgroup/
	 * CurrentPcieGen2ProgramGuide/pcie_ep.htm
	 */

	DHD_ERROR(("ClkReq0(0x%x)=0x%x ClkReq1(0x%x)=0x%x ClkReq2(0x%x)=0x%x "
		"ClkReq3(0x%x)=0x%x\n", PCIECFGREG_PHY_DBG_CLKREQ0,
		dhd_pcie_corereg_read(dhd->bus->sih, PCIECFGREG_PHY_DBG_CLKREQ0),
		PCIECFGREG_PHY_DBG_CLKREQ1,
		dhd_pcie_corereg_read(dhd->bus->sih, PCIECFGREG_PHY_DBG_CLKREQ1),
		PCIECFGREG_PHY_DBG_CLKREQ2,
		dhd_pcie_corereg_read(dhd->bus->sih, PCIECFGREG_PHY_DBG_CLKREQ2),
		PCIECFGREG_PHY_DBG_CLKREQ3,
		dhd_pcie_corereg_read(dhd->bus->sih, PCIECFGREG_PHY_DBG_CLKREQ3)));

#ifdef EXTENDED_PCIE_DEBUG_DUMP
	if (dhd->bus->sih->buscorerev >= 24) {

		DHD_ERROR(("ltssm_hist_0(0x%x)=0x%x ltssm_hist_1(0x%x)=0x%x "
			"ltssm_hist_2(0x%x)=0x%x "
			"ltssm_hist_3(0x%x)=0x%x\n", PCIECFGREG_PHY_LTSSM_HIST_0,
			dhd_pcie_corereg_read(dhd->bus->sih, PCIECFGREG_PHY_LTSSM_HIST_0),
			PCIECFGREG_PHY_LTSSM_HIST_1,
			dhd_pcie_corereg_read(dhd->bus->sih, PCIECFGREG_PHY_LTSSM_HIST_1),
			PCIECFGREG_PHY_LTSSM_HIST_2,
			dhd_pcie_corereg_read(dhd->bus->sih, PCIECFGREG_PHY_LTSSM_HIST_2),
			PCIECFGREG_PHY_LTSSM_HIST_3,
			dhd_pcie_corereg_read(dhd->bus->sih, PCIECFGREG_PHY_LTSSM_HIST_3)));

		DHD_ERROR(("trefup(0x%x)=0x%x trefup_ext(0x%x)=0x%x\n",
			PCIECFGREG_TREFUP,
			dhd_pcie_corereg_read(dhd->bus->sih, PCIECFGREG_TREFUP),
			PCIECFGREG_TREFUP_EXT,
			dhd_pcie_corereg_read(dhd->bus->sih, PCIECFGREG_TREFUP_EXT)));
		DHD_ERROR(("errlog(0x%x)=0x%x errlog_addr(0x%x)=0x%x "
			"Function_Intstatus(0x%x)=0x%x "
			"Function_Intmask(0x%x)=0x%x Power_Intstatus(0x%x)=0x%x "
			"Power_Intmask(0x%x)=0x%x\n",
			PCIE_CORE_REG_ERRLOG,
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
			PCIE_CORE_REG_ERRLOG, 0, 0),
			PCIE_CORE_REG_ERR_ADDR,
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				PCIE_CORE_REG_ERR_ADDR, 0, 0),
			PCIFunctionIntstatus(dhd->bus->sih->buscorerev),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				PCIFunctionIntstatus(dhd->bus->sih->buscorerev), 0, 0),
			PCIFunctionIntmask(dhd->bus->sih->buscorerev),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				PCIFunctionIntmask(dhd->bus->sih->buscorerev), 0, 0),
			PCIPowerIntstatus(dhd->bus->sih->buscorerev),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				PCIPowerIntstatus(dhd->bus->sih->buscorerev), 0, 0),
			PCIPowerIntmask(dhd->bus->sih->buscorerev),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				PCIPowerIntmask(dhd->bus->sih->buscorerev), 0, 0)));
		DHD_ERROR(("err_hdrlog1(0x%x)=0x%x err_hdrlog2(0x%x)=0x%x "
			"err_hdrlog3(0x%x)=0x%x err_hdrlog4(0x%x)=0x%x\n",
			(uint)OFFSETOF(sbpcieregs_t, u.pcie2.err_hdr_logreg1),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				OFFSETOF(sbpcieregs_t, u.pcie2.err_hdr_logreg1), 0, 0),
			(uint)OFFSETOF(sbpcieregs_t, u.pcie2.err_hdr_logreg2),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				OFFSETOF(sbpcieregs_t, u.pcie2.err_hdr_logreg2), 0, 0),
			(uint)OFFSETOF(sbpcieregs_t, u.pcie2.err_hdr_logreg3),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				OFFSETOF(sbpcieregs_t, u.pcie2.err_hdr_logreg3), 0, 0),
			(uint)OFFSETOF(sbpcieregs_t, u.pcie2.err_hdr_logreg4),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				OFFSETOF(sbpcieregs_t, u.pcie2.err_hdr_logreg4), 0, 0)));
		DHD_ERROR(("err_code(0x%x)=0x%x PCIH2D_MailBox(%08x)=%08x\n",
			(uint)OFFSETOF(sbpcieregs_t, u.pcie2.err_code_logreg),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				OFFSETOF(sbpcieregs_t, u.pcie2.err_code_logreg), 0, 0),
			dhd_bus_db0_addr_get(dhd->bus),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				dhd_bus_db0_addr_get(dhd->bus), 0, 0)));

		dhdpcie_hw_war_regdump(dhd->bus);
	}
#endif /* EXTENDED_PCIE_DEBUG_DUMP */

	dhd_pcie_dma_info_dump(dhd);
exit:
	if (MULTIBP_ENAB(dhd->bus->sih)) {
		dhd_bus_pcie_pwr_req_clear(dhd->bus);
	}

	return 0;
}

bool
dhd_bus_force_bt_quiesce_enabled(struct dhd_bus *bus)
{
	return bus->force_bt_quiesce;
}

uint8
dhd_d11_slices_num_get(dhd_pub_t *dhdp)
{
	return (dhdp->bus->sih && si_scan_core_present(dhdp->bus->sih)) ?
		MAX_NUM_D11_CORES_WITH_SCAN : MAX_NUM_D11CORES;
}

#if defined(linux) || defined(LINUX)
static bool
dhd_bus_tcm_test(struct dhd_bus *bus)
{
	int ret = 0;
	int size; /* Full mem size */
	int start; /* Start address */
	int read_size = 0; /* Read size of each iteration */
	int num = 0;
	uint8 *read_buf, *write_buf;
	uint8 init_val[NUM_PATTERNS] = {
		0xFFu, /* 11111111 */
		0x00u, /* 00000000 */
#if !defined(DHD_FW_MEM_CORRUPTION) && defined(BCM4375_CHIP)
		0x77u, /* 01110111 */
		0x22u, /* 00100010 */
		0x27u, /* 00100111 */
		0x72u, /* 01110010 */
#endif /* !DHD_FW_MEM_CORRUPTION */
	};

	if (!bus) {
		DHD_ERROR(("%s: bus is NULL !\n", __FUNCTION__));
		return FALSE;
	}

	read_buf = MALLOCZ(bus->dhd->osh, MEMBLOCK);

	if (!read_buf) {
		DHD_ERROR(("%s: MALLOC of read_buf failed\n", __FUNCTION__));
		return FALSE;
	}

	write_buf = MALLOCZ(bus->dhd->osh, MEMBLOCK);

	if (!write_buf) {
		MFREE(bus->dhd->osh, read_buf, MEMBLOCK);
		DHD_ERROR(("%s: MALLOC of write_buf failed\n", __FUNCTION__));
		return FALSE;
	}

	DHD_ERROR(("%s: start %x,  size: %x\n", __FUNCTION__, bus->dongle_ram_base, bus->ramsize));
	DHD_ERROR(("%s: memblock size %d,  #pattern %d\n", __FUNCTION__, MEMBLOCK, NUM_PATTERNS));

	while (num < NUM_PATTERNS) {
		start = bus->dongle_ram_base;
		/* Get full mem size */
		size = bus->ramsize;

		memset(write_buf, init_val[num], MEMBLOCK);
		while (size > 0) {
			read_size = MIN(MEMBLOCK, size);
			memset(read_buf, 0, read_size);

			/* Write */
			if ((ret = dhdpcie_bus_membytes(bus, TRUE, start, write_buf, read_size))) {
				DHD_ERROR(("%s: Write Error membytes %d\n", __FUNCTION__, ret));
				MFREE(bus->dhd->osh, read_buf, MEMBLOCK);
				MFREE(bus->dhd->osh, write_buf, MEMBLOCK);
				return FALSE;
			}

			/* Read */
			if ((ret = dhdpcie_bus_membytes(bus, FALSE, start, read_buf, read_size))) {
				DHD_ERROR(("%s: Read Error membytes %d\n", __FUNCTION__, ret));
				MFREE(bus->dhd->osh, read_buf, MEMBLOCK);
				MFREE(bus->dhd->osh, write_buf, MEMBLOCK);
				return FALSE;
			}

			/* Compare */
			if (memcmp(read_buf, write_buf, read_size)) {
				DHD_ERROR(("%s: Mismatch at %x, iter : %d\n",
					__FUNCTION__, start, num));
				prhex("Readbuf", read_buf, read_size);
				prhex("Writebuf", write_buf, read_size);
				MFREE(bus->dhd->osh, read_buf, MEMBLOCK);
				MFREE(bus->dhd->osh, write_buf, MEMBLOCK);
				return FALSE;
			}

			/* Decrement size and increment start address */
			size -= read_size;
			start += read_size;
		}
		num++;
	}

	MFREE(bus->dhd->osh, read_buf, MEMBLOCK);
	MFREE(bus->dhd->osh, write_buf, MEMBLOCK);

	DHD_ERROR(("%s: Success iter : %d\n", __FUNCTION__, num));
	return TRUE;
}
#endif /* LINUX || linux */

#define PCI_CFG_LINK_SPEED_SHIFT	16
int
dhd_get_pcie_linkspeed(dhd_pub_t *dhd)
{
	uint32 pcie_lnkst;
	uint32 pcie_lnkspeed;
	pcie_lnkst = OSL_PCI_READ_CONFIG(dhd->osh, PCIECFGREG_LINK_STATUS_CTRL,
		sizeof(pcie_lnkst));

	pcie_lnkspeed = (pcie_lnkst >> PCI_CFG_LINK_SPEED_SHIFT) & PCI_LINK_SPEED_MASK;
	DHD_INFO(("%s: Link speed: %d\n", __FUNCTION__, pcie_lnkspeed));
	return pcie_lnkspeed;
}

int
dhd_bus_checkdied(struct dhd_bus *bus, char *data, uint size)
{
	return dhdpcie_checkdied(bus, data, size);
}

/* Common backplane can be hung by butting APB2 bridge in reset */
void
dhdpcie_induce_cbp_hang(dhd_pub_t *dhd)
{
	uint32 addr, val;
	uint32 apb2_wrapper_reg = 0x18106000;
	uint32 apb2_reset_ctrl_offset = 0x800;
	addr = apb2_wrapper_reg + apb2_reset_ctrl_offset;
	val = 1;
	dhd_sbreg_op(dhd, addr, &val, FALSE);
}

bool
dhd_bus_init_done(struct dhd_bus *bus)
{
	return bus->intr_enabled;
}

#ifdef DHD_SSSR_DUMP
void
dhd_fill_sssr_reg_info_4389(dhd_pub_t *dhd)
{
	sssr_reg_info_cmn_t *sssr_reg_info_cmn = dhd->sssr_reg_info;
	sssr_reg_info_v3_t *sssr_reg_info;

	sssr_reg_info = (sssr_reg_info_v3_t *)&sssr_reg_info_cmn->rev3;

	DHD_ERROR(("%s:\n", __FUNCTION__));
	sssr_reg_info->version = SSSR_REG_INFO_VER_3;
	sssr_reg_info->length = sizeof(sssr_reg_info_v3_t);

	sssr_reg_info->pmu_regs.base_regs.pmuintmask0 = 0x18012700;
	sssr_reg_info->pmu_regs.base_regs.pmuintmask1 = 0x18012704;
	sssr_reg_info->pmu_regs.base_regs.resreqtimer = 0x18012644;
	sssr_reg_info->pmu_regs.base_regs.macresreqtimer = 0x18012688;
	sssr_reg_info->pmu_regs.base_regs.macresreqtimer1 = 0x180126f0;
	sssr_reg_info->pmu_regs.base_regs.macresreqtimer2 = 0x18012738;

	sssr_reg_info->chipcommon_regs.base_regs.intmask = 0x18000024;
	sssr_reg_info->chipcommon_regs.base_regs.powerctrl = 0x180001e8;
	sssr_reg_info->chipcommon_regs.base_regs.clockcontrolstatus = 0x180001e0;
	sssr_reg_info->chipcommon_regs.base_regs.powerctrl_mask = 0x1f00;

	sssr_reg_info->arm_regs.base_regs.clockcontrolstatus = 0x180201e0;
	sssr_reg_info->arm_regs.base_regs.clockcontrolstatus_val = 0x20;
	sssr_reg_info->arm_regs.wrapper_regs.resetctrl = 0x18120800;
	sssr_reg_info->arm_regs.wrapper_regs.extrsrcreq = 0x18006234;

	sssr_reg_info->pcie_regs.base_regs.ltrstate = 0x18001c38;
	sssr_reg_info->pcie_regs.base_regs.clockcontrolstatus = 0x180011e0;
	sssr_reg_info->pcie_regs.base_regs.clockcontrolstatus_val = 0x0;
	sssr_reg_info->pcie_regs.wrapper_regs.extrsrcreq = 0x180061b4;

	sssr_reg_info->mac_regs[0].base_regs.xmtaddress = 0x18021130;
	sssr_reg_info->mac_regs[0].base_regs.xmtdata = 0x18021134;
	sssr_reg_info->mac_regs[0].base_regs.clockcontrolstatus = 0x180211e0;
	sssr_reg_info->mac_regs[0].base_regs.clockcontrolstatus_val = 0x20;
	sssr_reg_info->mac_regs[0].wrapper_regs.resetctrl = 0x18121800;
	sssr_reg_info->mac_regs[0].wrapper_regs.extrsrcreq = 0x180062b4;
	sssr_reg_info->mac_regs[0].wrapper_regs.ioctrl = 0x18121408;
	sssr_reg_info->mac_regs[0].wrapper_regs.ioctrl_resetseq_val[0] = 0xc7;
	sssr_reg_info->mac_regs[0].wrapper_regs.ioctrl_resetseq_val[1] = 0x15f;
	sssr_reg_info->mac_regs[0].wrapper_regs.ioctrl_resetseq_val[2] = 0x151;
	sssr_reg_info->mac_regs[0].wrapper_regs.ioctrl_resetseq_val[3] = 0x155;
	sssr_reg_info->mac_regs[0].wrapper_regs.ioctrl_resetseq_val[4] = 0xc5;
	sssr_reg_info->mac_regs[0].sr_size = 0x40000;

	sssr_reg_info->mac_regs[1].base_regs.xmtaddress = 0x18022130;
	sssr_reg_info->mac_regs[1].base_regs.xmtdata = 0x18022134;
	sssr_reg_info->mac_regs[1].base_regs.clockcontrolstatus = 0x180221e0;
	sssr_reg_info->mac_regs[1].base_regs.clockcontrolstatus_val = 0x20;
	sssr_reg_info->mac_regs[1].wrapper_regs.resetctrl = 0x18122800;
	sssr_reg_info->mac_regs[1].wrapper_regs.extrsrcreq = 0x18006334;
	sssr_reg_info->mac_regs[1].wrapper_regs.ioctrl = 0x18122408;
	sssr_reg_info->mac_regs[1].wrapper_regs.ioctrl_resetseq_val[0] = 0xc7;
	sssr_reg_info->mac_regs[1].wrapper_regs.ioctrl_resetseq_val[1] = 0x15f;
	sssr_reg_info->mac_regs[1].wrapper_regs.ioctrl_resetseq_val[2] = 0x151;
	sssr_reg_info->mac_regs[1].wrapper_regs.ioctrl_resetseq_val[3] = 0x155;
	sssr_reg_info->mac_regs[1].wrapper_regs.ioctrl_resetseq_val[4] = 0xc5;
	sssr_reg_info->mac_regs[1].sr_size = 0x30000;

	sssr_reg_info->mac_regs[2].base_regs.xmtaddress = 0x18023130;
	sssr_reg_info->mac_regs[2].base_regs.xmtdata = 0x18023134;
	sssr_reg_info->mac_regs[2].base_regs.clockcontrolstatus = 0x180231e0;
	sssr_reg_info->mac_regs[2].base_regs.clockcontrolstatus_val = 0x20;
	sssr_reg_info->mac_regs[2].wrapper_regs.resetctrl = 0x18123800;
	sssr_reg_info->mac_regs[2].wrapper_regs.extrsrcreq = 0x180063b4;
	sssr_reg_info->mac_regs[2].wrapper_regs.ioctrl = 0x18123408;
	sssr_reg_info->mac_regs[2].wrapper_regs.ioctrl_resetseq_val[0] = 0xc7;
	sssr_reg_info->mac_regs[2].wrapper_regs.ioctrl_resetseq_val[1] = 0x15f;
	sssr_reg_info->mac_regs[2].wrapper_regs.ioctrl_resetseq_val[2] = 0x151;
	sssr_reg_info->mac_regs[2].wrapper_regs.ioctrl_resetseq_val[3] = 0x155;
	sssr_reg_info->mac_regs[2].wrapper_regs.ioctrl_resetseq_val[4] = 0xc5;
	sssr_reg_info->mac_regs[2].sr_size = 0x30000;

	sssr_reg_info->dig_mem_info.dig_sr_addr = 0x18520000;
	sssr_reg_info->dig_mem_info.dig_sr_size = 0x10000;

	sssr_reg_info->fis_enab = 1;

	sssr_reg_info->hwa_regs.base_regs.clkenable = 0x180242d0;
	sssr_reg_info->hwa_regs.base_regs.clkgatingenable = 0x180242d4;
	sssr_reg_info->hwa_regs.base_regs.clkext = 0x180242e0;
	sssr_reg_info->hwa_regs.base_regs.clkctlstatus = 0x180241e0;
	sssr_reg_info->hwa_regs.wrapper_regs.ioctrl = 0x18124408;
	sssr_reg_info->hwa_regs.wrapper_regs.resetctrl = 0x18124800;

	sssr_reg_info->hwa_regs.hwa_resetseq_val[0] = 0x3;
	sssr_reg_info->hwa_regs.hwa_resetseq_val[1] = 0x1;
	sssr_reg_info->hwa_regs.hwa_resetseq_val[2] = 0x0;
	sssr_reg_info->hwa_regs.hwa_resetseq_val[3] = 0x1;
	sssr_reg_info->hwa_regs.hwa_resetseq_val[4] = 0x1ff;
	sssr_reg_info->hwa_regs.hwa_resetseq_val[5] = 0x1ff;
	sssr_reg_info->hwa_regs.hwa_resetseq_val[6] = 0x3;
	sssr_reg_info->hwa_regs.hwa_resetseq_val[7] = 0x20;

	dhd->sssr_inited = TRUE;
}

void
dhdpcie_fill_sssr_reg_info(dhd_pub_t *dhd)
{
	if (dhd_get_chipid(dhd->bus) == BCM4389_CHIP_ID) {
		dhd_fill_sssr_reg_info_4389(dhd);
	}
}

#endif /* DHD_SSSR_DUMP */

#if defined(NDIS)
void *
dhd_bus_get_socram_buf(struct dhd_bus *bus, struct dhd_pub *dhdp)
{
	return dhd_get_fwdump_buf(dhdp, bus->ramsize);
}
#endif /* NDIS */

void
dhd_bus_set_signature_path(struct dhd_bus *bus, char *sig_path)
{
	strlcpy(bus->fwsig_filename, sig_path, sizeof(bus->fwsig_filename));
}

#define BUS_SLEEP_WAIT_CNT	3
#define BUS_SLEEP_WAIT_MS	20
int
dhd_bus_sleep(dhd_pub_t *dhdp, bool sleep, uint32 *intstatus)
{
	dhd_bus_t *bus = dhdp->bus;
	int active, cnt = 0;

	if (bus) {
		while ((active = dhd_os_check_wakelock_all(bus->dhd)) &&
				(cnt < BUS_SLEEP_WAIT_CNT)) {
			OSL_SLEEP(BUS_SLEEP_WAIT_MS);
			cnt++;
		}
	} else {
		DHD_ERROR(("bus is NULL\n"));
		active = -1;
	}

	return active;
}
