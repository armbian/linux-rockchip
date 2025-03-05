/*
 * Definitions for ioctls to access DHD iovars.
 * Based on wlioctl.h (for Broadcom 802.11abg driver).
 * (Moves towards generic ioctls for BCM drivers/iovars.)
 *
 * Definitions subject to change without notice.
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
 * <<Broadcom-WL-IPTag/Dual:>>
 */

#ifndef _dhdioctl_h_
#define	_dhdioctl_h_

#include <typedefs.h>

/* Linux network driver ioctl encoding */
typedef struct dhd_ioctl {
	uint32 cmd;	/* common ioctl definition */
	void *buf;	/* pointer to user buffer */
	uint32 len;	/* length of user buffer */
	uint32 set;	/* get or set request boolean (optional) */
	uint32 used;	/* bytes read or written (optional) */
	uint32 needed;	/* bytes needed (optional) */
	uint32 driver;	/* to identify target driver */
} dhd_ioctl_t;

/* Underlying BUS definition */
enum {
	BUS_TYPE_USB = 0, /* for USB dongles */
	BUS_TYPE_SDIO, /* for SDIO dongles */
	BUS_TYPE_PCIE /* for PCIE dongles */
};

typedef enum {
	DMA_XFER_SUCCESS = 0,
	DMA_XFER_IN_PROGRESS,
	DMA_XFER_FAILED
} dma_xfer_status_t;

typedef enum d11_lpbk_type {
	M2M_DMA_LPBK = 0,
	D11_LPBK = 1,
	BMC_LPBK = 2,
	M2M_NON_DMA_LPBK = 3,
	D11_HOST_MEM_LPBK = 4,
	BMC_HOST_MEM_LPBK = 5,
	M2M_WRITE_TO_RAM = 6,
	M2M_READ_FROM_RAM = 7,
	D11_WRITE_TO_RAM = 8,
	D11_READ_FROM_RAM = 9,
	MAX_LPBK = 10
} dma_xfer_type_t;

typedef struct dmaxfer_info {
	uint16 version;
	uint16 length;
	dma_xfer_status_t status;
	dma_xfer_type_t type;
	uint src_delay;
	uint dest_delay;
	uint should_wait;
	uint core_num;
	int error_code;
	uint32 num_bytes;
	uint64 time_taken;
	uint64 tput;
} dma_xfer_info_t;

#define DHD_DMAXFER_VERSION 0x1

#define DHD_FILENAME_MAX 64
#define DHD_PATHNAME_MAX 128

#ifdef EFI
struct control_signal_ops {
	uint32 signal;
	uint32 val;
};
enum {
	WL_REG_ON = 0,
	DEVICE_WAKE = 1,
	TIME_SYNC = 2
};

typedef struct wifi_properties {
	uint8 version;
	uint32 vendor;
	uint32 model;
	uint8 mac_addr[6];
	uint32 chip_revision;
	uint8 silicon_revision;
	uint8 is_powered;
	uint8 is_sleeping;
	char module_revision[16];	/* null terminated string */
	uint8 is_fw_loaded;
	char  fw_filename[DHD_FILENAME_MAX];		/* null terminated string */
	char nvram_filename[DHD_FILENAME_MAX];	/* null terminated string */
	uint8 channel;
	uint8 module_sn[6];
} wifi_properties_t;

#define DHD_WIFI_PROPERTIES_VERSION 0x1

#define DHD_OTP_SIZE_WORDS 912

typedef struct intr_poll_data {
	uint16 version;
	uint16 length;
	uint32 type;
	uint32 value;
} intr_poll_t;

typedef enum intr_poll_data_type {
	INTR_POLL_DATA_PERIOD = 0,
	INTR_POLL_DATA_NUM_PKTS_THRESH,
	INTR_POLL_DATA_PKT_INTVL_THRESH
} intr_poll_type_t;

#define DHD_INTR_POLL_VERSION 0x1u
#endif /* EFI */

typedef struct tput_test {
	uint16 version;
	uint16 length;
	uint8 direction;
	uint8 tput_test_running;
	uint8 mac_sta[6];
	uint8 mac_ap[6];
	uint8 PAD[2];
	uint32 payload_size;
	uint32 num_pkts;
	uint32 timeout_ms;
	uint32 flags;

	uint32 pkts_good;
	uint32 pkts_bad;
	uint32 pkts_cmpl;
	uint64 time_ms;
	uint64 tput_bps;
} tput_test_t;

typedef enum {
	TPUT_DIR_TX = 0,
	TPUT_DIR_RX
} tput_dir_t;

/*
 * Current supported roles considered for policy management are AP, P2P and NAN.
 * Hence max value is limited to 3.
 */
#define DHD_MAX_IFACE_PRIORITY 3u
typedef enum dhd_iftype {
	DHD_IF_TYPE_STA		= 0,
	DHD_IF_TYPE_AP		= 1,

	DHD_IF_TYPE_NAN_NMI	= 3,
	DHD_IF_TYPE_NAN		= 4,
	DHD_IF_TYPE_P2P_GO	= 5,
	DHD_IF_TYPE_P2P_GC	= 6,
	DHD_IF_TYPE_P2P_DISC	= 7,
	DHD_IF_TYPE_IBSS	= 8,
	DHD_IF_TYPE_MONITOR	= 9,
	DHD_IF_TYPE_AIBSS	= 10,
	DHD_IF_TYPE_MAX
} dhd_iftype_t;

typedef struct dhd_iface_mgmt_data {
	uint8 policy;
	uint8 priority[DHD_IF_TYPE_MAX];
} dhd_iface_mgmt_data_t;

typedef enum dhd_iface_mgmt_policy {
	DHD_IF_POLICY_DEFAULT		= 0,
	DHD_IF_POLICY_FCFS		= 1,
	DHD_IF_POLICY_LP		= 2,
	DHD_IF_POLICY_ROLE_PRIORITY	= 3,
	DHD_IF_POLICY_CUSTOM		= 4,
	DHD_IF_POLICY_INVALID		= 5
} dhd_iface_mgmt_policy_t;

#define TPUT_TEST_T_VER 1
#define TPUT_TEST_T_LEN 68
#define TPUT_TEST_MIN_PAYLOAD_SIZE 16
#define TPUT_TEST_USE_ETHERNET_HDR 0x1
#define TPUT_TEST_USE_802_11_HDR 0x2

/* per-driver magic numbers */
#define DHD_IOCTL_MAGIC		0x00444944

/* bump this number if you change the ioctl interface */
#define DHD_IOCTL_VERSION	1

/*
 * Increase the DHD_IOCTL_MAXLEN to 16K for supporting download of NVRAM files of size
 * > 8K. In the existing implementation when NVRAM is to be downloaded via the "vars"
 * DHD IOVAR, the NVRAM is copied to the DHD Driver memory. Later on when "dwnldstate" is
 * invoked with FALSE option, the NVRAM gets copied from the DHD driver to the Dongle
 * memory. The simple way to support this feature without modifying the DHD application,
 * driver logic is to increase the DHD_IOCTL_MAXLEN size. This macro defines the "size"
 * of the buffer in which data is exchanged between the DHD App and DHD driver.
 */
#define	DHD_IOCTL_MAXLEN	(16384)	/* max length ioctl buffer required */
#define	DHD_IOCTL_SMLEN		256		/* "small" length ioctl buffer required */

/*
 * For cases where 16K buf is not sufficient.
 * Ex:- DHD dump output buffer is more than 16K.
 */
#define	DHD_IOCTL_MAXLEN_48K	(48 * 1024)
#define	DHD_IOCTL_MAXLEN_32K	(32768u)
#define DHD_DUMP_IOCTL_MAXLEN DHD_IOCTL_MAXLEN_48K

/* common ioctl definitions */
#define DHD_GET_MAGIC				0
#define DHD_GET_VERSION				1
#define DHD_GET_VAR				2
#define DHD_SET_VAR				3

/* message levels */
#define DHD_ERROR_VAL	0x0001
#define DHD_TRACE_VAL	0x0002
#define DHD_INFO_VAL	0x0004
#define DHD_DATA_VAL	0x0008
#define DHD_CTL_VAL	0x0010
#define DHD_TIMER_VAL	0x0020
#define DHD_HDRS_VAL	0x0040
#define DHD_BYTES_VAL	0x0080
#define DHD_INTR_VAL	0x0100
#define DHD_LOG_VAL	0x0200
#define DHD_GLOM_VAL	0x0400
#define DHD_EVENT_VAL	0x0800
#define DHD_BTA_VAL	0x1000
#if defined(NDIS) && (NDISVER >= 0x0630) && defined(BCMDONGLEHOST)
#define DHD_SCAN_VAL	0x2000
#else
#define DHD_ISCAN_VAL	0x2000
#endif
#define DHD_ARPOE_VAL	0x4000
#define DHD_REORDER_VAL	0x8000
#define DHD_NOCHECKDIED_VAL		0x20000 /* UTF WAR */
#define DHD_PNO_VAL		0x80000
#define DHD_RTT_VAL		0x100000
#define DHD_MSGTRACE_VAL	0x200000
#define DHD_FWLOG_VAL		0x400000
#define DHD_DBGIF_VAL		0x800000
#define DHD_RPM_VAL		0x1000000
#define DHD_PKT_MON_VAL		0x2000000
#define DHD_PKT_MON_DUMP_VAL	0x4000000
#define DHD_ERROR_MEM_VAL	0x8000000
#define DHD_DNGL_IOVAR_SET_VAL	0x10000000 /**< logs the setting of dongle iovars */
#define DHD_LPBKDTDUMP_VAL	0x20000000
#define DHD_PRSRV_MEM_VAL	0x40000000
#define DHD_IOVAR_MEM_VAL	0x80000000
#define DHD_ANDROID_VAL	0x10000
#define DHD_IW_VAL	0x20000
#define DHD_CFG_VAL	0x40000
#define DHD_CONFIG_VAL	0x80000
#define DHD_DUMP_VAL	0x100000
#define DUMP_EAPOL_VAL	0x0001
#define DUMP_ARP_VAL	0x0002
#define DUMP_DHCP_VAL	0x0004
#define DUMP_ICMP_VAL	0x0008
#define DUMP_DNS_VAL	0x0010
#define DUMP_TRX_VAL	0x0080

/* Message levels for Mesh */
#define MESH_MSG_HDR_VAL        0x00000001u /* Header details */

#ifdef SDTEST
/* For pktgen iovar */
typedef struct dhd_pktgen {
	uint32 version;		/* To allow structure change tracking */
	uint32 freq;		/* Max ticks between tx/rx attempts */
	uint32 count;		/* Test packets to send/rcv each attempt */
	uint32 print;		/* Print counts every <print> attempts */
	uint32 total;		/* Total packets (or bursts) */
	uint32 minlen;		/* Minimum length of packets to send */
	uint32 maxlen;		/* Maximum length of packets to send */
	uint32 numsent;		/* Count of test packets sent */
	uint32 numrcvd;		/* Count of test packets received */
	uint32 numfail;		/* Count of test send failures */
	uint32 mode;		/* Test mode (type of test packets) */
	uint32 stop;		/* Stop after this many tx failures */
} dhd_pktgen_t;

/* Version in case structure changes */
#define DHD_PKTGEN_VERSION 2

/* Type of test packets to use */
#define DHD_PKTGEN_ECHO		1 /* Send echo requests */
#define DHD_PKTGEN_SEND 	2 /* Send discard packets */
#define DHD_PKTGEN_RXBURST	3 /* Request dongle send N packets */
#define DHD_PKTGEN_RECV		4 /* Continuous rx from continuous tx dongle */
#endif /* SDTEST */

/* For membytes iovar - flags param bit definitions and accessor macros */
#define DHD_MEMBYTES_FLAGS_MAGIC		0x5DA60900 /* indicates flags presence */
#define DHD_MEMBYTES_FLAGS_MAGIC_MASK		0xFFFFFF00 /* magic bytes mask */
#define DHD_MEMBYTES_FLAGS_BAR_MASK		0x00000007 /* bar region mask */
#define DHD_MEMBYTES_FLAGS_BAR_MIN		0 /* Min valid value for BAR region */
#define DHD_MEMBYTES_FLAGS_BAR_MAX		5 /* Max valid value for BAR region */
#define DHD_MEMBYTES_FLAGS_INIT(x)		((x) = ((uint32)DHD_MEMBYTES_FLAGS_MAGIC))
#define DHD_MEMBYTES_FLAGS_GET_MAGIC(x)		((x) & DHD_MEMBYTES_FLAGS_MAGIC_MASK)
#define DHD_MEMBYTES_FLAGS_GET_BAR(x)		((x) & DHD_MEMBYTES_FLAGS_BAR_MASK)
#define DHD_MEMBYTES_FLAGS_SET_BAR(x, b)	((x) |= ((b) & DHD_MEMBYTES_FLAGS_BAR_MASK))

/* Enter idle immediately (no timeout) */
#define DHD_IDLE_IMMEDIATE	(-1)

/* Values for idleclock iovar: other values are the sd_divisor to use when idle */
#define DHD_IDLE_ACTIVE	0	/* Do not request any SD clock change when idle */
#define DHD_IDLE_STOP   (-1)	/* Request SD clock be stopped (and use SD1 mode) */

enum dhd_maclist_xtlv_type {
	DHD_MACLIST_XTLV_R = 0x1,
	DHD_MACLIST_XTLV_X = 0x2,
	DHD_SVMPLIST_XTLV = 0x3
};

typedef struct _dhd_maclist_t {
	uint16 version;			/* Version */
	uint16 bytes_len;		/* Total bytes length of lists, XTLV headers and paddings */
	uint8 plist[BCM_FLEX_ARRAY];	/* Pointer to the first list */
} dhd_maclist_t;

typedef struct _dhd_pd11regs_param {
	uint16 start_idx;
	uint8 verbose;
	uint8 pad;
	uint8 plist[BCM_FLEX_ARRAY];
} dhd_pd11regs_param;

typedef struct _dhd_pd11regs_buf {
	uint16 idx;
	uint8 pad[2];
	uint8 pbuf[BCM_FLEX_ARRAY];
} dhd_pd11regs_buf;

/* BT logging and memory dump */

#define BT_LOG_BUF_MAX_SIZE		(DHD_IOCTL_MAXLEN - (2 * sizeof(int)))
#define BT_LOG_BUF_NOT_AVAILABLE	0
#define BT_LOG_NEXT_BUF_NOT_AVAIL	1
#define BT_LOG_NEXT_BUF_AVAIL		2
#define BT_LOG_NOT_READY		3

typedef struct bt_log_buf_info {
	int availability;
	int size;
	char buf[BT_LOG_BUF_MAX_SIZE];
} bt_log_buf_info_t;

/* request BT memory in chunks */
typedef struct bt_mem_req {
	int offset;	/* offset from BT memory start */
	int buf_size;	/* buffer size per chunk */
} bt_mem_req_t;

typedef struct fw_download_info {
	uint32  fw_start_addr;
	uint32  fw_size;
	uint32  fw_entry_pt;
	char    fw_signature_fname[DHD_FILENAME_MAX];
	char    bootloader_fname[DHD_FILENAME_MAX];
	uint32  bootloader_start_addr;
	char    fw_path[DHD_PATHNAME_MAX];
} fw_download_info_t;

/* max dest supported */
#define DEBUG_BUF_DEST_MAX	4

/* debug buf dest stat */
typedef struct debug_buf_dest_stat {
	uint32 stat[DEBUG_BUF_DEST_MAX];
} debug_buf_dest_stat_t;

/* devreset */
#define DHD_DEVRESET_VERSION 1

typedef struct devreset_info {
	uint16 version;
	uint16 length;
	uint16 mode;
	int16 status;
} devreset_info_t;

#ifdef DHD_TX_PROFILE

#define DHD_TX_PROFILE_VERSION	1

/* tx_profile structure for tagging */
typedef struct dhd_tx_profile_protocol {
	uint16	version;
	uint8	profile_index;
	uint8	layer;
	uint32	protocol_number;
	uint16	src_port;
	uint16	dest_port;
} dhd_tx_profile_protocol_t;

#define DHD_TX_PROFILE_DATA_LINK_LAYER	(2u)	/* data link layer protocols */
#define DHD_TX_PROFILE_NETWORK_LAYER	(3u)	/* network layer protocols */

#define DHD_MAX_PROFILE_INDEX	(7u)	/* three bits are available to encode
					   the tx profile index in the rate
					   field in host_txbuf_post_t
					 */
#define DHD_MAX_PROFILES	(1u)	/* ucode only supports 1 profile atm */

#endif /* defined(DHD_TX_PROFILE) */

typedef struct dhd_loglevel_data {
	uint32 type;
	uint32 component;
	uint32 dhd_print_lv;
	uint32 dhd_log_lv;
	uint32 wl_print_lv;
	uint32 wl_log_lv;
} dhd_loglevel_data_t;

typedef enum dhd_loglevel_type {
	DHD_LOGLEVEL_TYPE_ALL		= 0,
	DHD_LOGLEVEL_TYPE_PRINT		= 1,
	DHD_LOGLEVEL_TYPE_LOG		= 2,
	DHD_LOGLEVEL_TYPE_INVALID	= 3
} dhd_loglevel_type_t;

typedef enum dhd_loglevel_comp {
	DHD_LOGLEVEL_COMP_DHD           = 0,
	DHD_LOGLEVEL_COMP_WL            = 1,
	DHD_LOGLEVEL_COMP_INVALID	= 2
} dhd_loglevel_comp_t;
#endif /* _dhdioctl_h_ */
