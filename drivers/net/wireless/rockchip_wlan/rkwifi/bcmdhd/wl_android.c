/*
 * Linux cfg80211 driver - Android related functions
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

#define CLEANED_UP_IOCTL 1

#include <linux/module.h>
#include <linux/netdevice.h>
#include <net/netlink.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include <wl_android.h>
#include <wldev_common.h>
#include <wlioctl.h>
#include <wlioctl_utils.h>
#include <bcmutils.h>
#include <bcmstdlib_s.h>
#include <linux_osl.h>
#include <dhd_dbg.h>
#include <dngl_stats.h>
#include <dhd.h>
#include <bcmip.h>
#ifdef PNO_SUPPORT
#include <dhd_pno.h>
#endif
#ifdef BCMSDIO
#include <bcmsdbus.h>
#endif
#ifdef WL_CFG80211
#include <wl_cfg80211.h>
#include <wl_cfgscan.h>
#include <wl_cfgvif.h>
#include <dhd_cfg80211.h>
#endif
#ifdef WL_NAN
#include <wl_cfgnan.h>
#endif /* WL_NAN */
#ifdef DHDTCPACK_SUPPRESS
#include <dhd_ip.h>
#endif /* DHDTCPACK_SUPPRESS */
#ifdef USE_NEW_RSPEC_DEFS
#include <bcmwifi_rspec.h>
#endif /* USE_NEW_RSPEC_DEFS */
#include <dhd_linux.h>
#include <bcmiov.h>
#ifdef DHD_PKT_LOGGING
#include <dhd_pktlog.h>
#endif /* DHD_PKT_LOGGING */
#include <wl_cfgvendor.h>
#include <brcm_nl80211.h>
#ifdef WL_MBO
#include <mbo.h>
#endif /* WL_MBO */
#ifdef RTT_SUPPORT
#include <dhd_rtt.h>
#endif /* RTT_SUPPORT */
#ifdef DHD_EVENT_LOG_FILTER
#include <dhd_event_log_filter.h>
#endif /* DHD_EVENT_LOG_FILTER */

#ifdef WL_TWT
#include <802.11ah.h>
#endif /* WL_TWT */
/*
 * Android private command strings, PLEASE define new private commands here
 * so they can be updated easily in the future (if needed)
 */

#define CMD_START		"START"
#define CMD_STOP		"STOP"
#define	CMD_SCAN_ACTIVE		"SCAN-ACTIVE"
#define	CMD_SCAN_PASSIVE	"SCAN-PASSIVE"
#define CMD_RSSI		"RSSI"
#define CMD_LINKSPEED		"LINKSPEED"
#define CMD_RXFILTER_START	"RXFILTER-START"
#define CMD_RXFILTER_STOP	"RXFILTER-STOP"
#define CMD_RXFILTER_ADD	"RXFILTER-ADD"
#define CMD_RXFILTER_REMOVE	"RXFILTER-REMOVE"
#define CMD_BTCOEXSCAN_START	"BTCOEXSCAN-START"
#define CMD_BTCOEXSCAN_STOP	"BTCOEXSCAN-STOP"
#define CMD_BTCOEXMODE		"BTCOEXMODE"
#define CMD_SETSUSPENDOPT	"SETSUSPENDOPT"
#define CMD_SETSUSPENDMODE      "SETSUSPENDMODE"
#define CMD_SETDTIM_IN_SUSPEND  "SET_DTIM_IN_SUSPEND"
#define CMD_MAXDTIM_IN_SUSPEND  "MAX_DTIM_IN_SUSPEND"
#define CMD_DISDTIM_IN_SUSPEND  "DISABLE_DTIM_IN_SUSPEND"
#define CMD_P2P_DEV_ADDR	"P2P_DEV_ADDR"
#define CMD_SETFWPATH		"SETFWPATH"
#define CMD_SETBAND		"SETBAND"
#define CMD_GETBAND		"GETBAND"
#define CMD_COUNTRY		"COUNTRY"
#define CMD_P2P_SET_NOA		"P2P_SET_NOA"
#define CMD_P2P_GET_NOA			"P2P_GET_NOA"
#define CMD_P2P_SD_OFFLOAD		"P2P_SD_"
#define CMD_P2P_LISTEN_OFFLOAD		"P2P_LO_"
#define CMD_P2P_SET_PS		"P2P_SET_PS"
#define CMD_P2P_ECSA		"P2P_ECSA"
#define CMD_P2P_INC_BW		"P2P_INCREASE_BW"
#define CMD_SET_AP_WPS_P2P_IE 		"SET_AP_WPS_P2P_IE"
#define CMD_SETROAMMODE 	"SETROAMMODE"
#define CMD_SETIBSSBEACONOUIDATA	"SETIBSSBEACONOUIDATA"
#define CMD_MIRACAST		"MIRACAST"
#ifdef WL_NAN
#define CMD_NAN         "NAN_"
#endif /* WL_NAN */
#define CMD_COUNTRY_DELIMITER "/"

#if defined(WL_SUPPORT_AUTO_CHANNEL)
#define CMD_GET_BEST_CHANNELS	"GET_BEST_CHANNELS"
#endif /* WL_SUPPORT_AUTO_CHANNEL */

#define CMD_80211_MODE    "MODE"  /* 802.11 mode a/b/g/n/ac */
#define CMD_CHANSPEC      "CHANSPEC"
#define CMD_DATARATE      "DATARATE"
#define CMD_ASSOC_CLIENTS "ASSOCLIST"
#define CMD_SET_CSA       "SETCSA"
#ifdef WL_SUPPORT_AUTO_CHANNEL
#define CMD_SET_HAPD_AUTO_CHANNEL	"HAPD_AUTO_CHANNEL"
#endif /* WL_SUPPORT_AUTO_CHANNEL */
#define CMD_KEEP_ALIVE          "KEEPALIVE"

#ifdef PNO_SUPPORT
#define CMD_PNOSSIDCLR_SET	"PNOSSIDCLR"
#define CMD_PNOSETUP_SET	"PNOSETUP "
#define CMD_PNOENABLE_SET	"PNOFORCE"
#define CMD_PNODEBUG_SET	"PNODEBUG"
#define CMD_WLS_BATCHING	"WLS_BATCHING"
#endif /* PNO_SUPPORT */

#define CMD_HAPD_SET_AX_MODE "HAPD_SET_AX_MODE"
#define CMD_SET_HOTSPOT_ANTENNA_MODE "SET_HOTSPOT_ANTENNA_MODE"
#define CMD_GET_HOTSPOT_ANTENNA_MODE "GET_HOTSPOT_ANTENNA_MODE"

#define	CMD_HAPD_MAC_FILTER	"HAPD_MAC_FILTER"

#if defined(SUPPORT_RANDOM_MAC_SCAN)
#define ENABLE_RANDOM_MAC "ENABLE_RANDOM_MAC"
#define DISABLE_RANDOM_MAC "DISABLE_RANDOM_MAC"
#endif /* SUPPORT_RANDOM_MAC_SCAN */
#define CMD_GET_FACTORY_MAC      "FACTORY_MAC"

#ifdef WLFBT
#define CMD_GET_FTKEY      "GET_FTKEY"
#endif

#define CMD_ROAM_OFFLOAD			"SETROAMOFFLOAD"
#define CMD_INTERFACE_CREATE			"INTERFACE_CREATE"
#define CMD_INTERFACE_DELETE			"INTERFACE_DELETE"
#define CMD_GET_LINK_STATUS			"GETLINKSTATUS"

#define CMD_GET_STA_INFO   "GETSTAINFO"

/* related with CMD_GET_LINK_STATUS */
#define WL_ANDROID_LINK_VHT					0x01
#define WL_ANDROID_LINK_MIMO					0x02
#define WL_ANDROID_LINK_AP_VHT_SUPPORT		0x04
#define WL_ANDROID_LINK_AP_MIMO_SUPPORT	0x08

#ifdef P2PRESP_WFDIE_SRC
#define CMD_P2P_SET_WFDIE_RESP      "P2P_SET_WFDIE_RESP"
#define CMD_P2P_GET_WFDIE_RESP      "P2P_GET_WFDIE_RESP"
#endif /* P2PRESP_WFDIE_SRC */

#define CMD_DFS_AP_MOVE			"DFS_AP_MOVE"
#define CMD_WBTEXT_ENABLE		"WBTEXT_ENABLE"
#define CMD_WBTEXT_PROFILE_CONFIG	"WBTEXT_PROFILE_CONFIG"
#define CMD_WBTEXT_WEIGHT_CONFIG	"WBTEXT_WEIGHT_CONFIG"
#define CMD_WBTEXT_TABLE_CONFIG		"WBTEXT_TABLE_CONFIG"
#define CMD_WBTEXT_DELTA_CONFIG		"WBTEXT_DELTA_CONFIG"
#define CMD_WBTEXT_BTM_TIMER_THRESHOLD	"WBTEXT_BTM_TIMER_THRESHOLD"
#define CMD_WBTEXT_BTM_DELTA		"WBTEXT_BTM_DELTA"
#define CMD_WBTEXT_ESTM_ENABLE	"WBTEXT_ESTM_ENABLE"

#ifdef WBTEXT
#define CMD_WBTEXT_PROFILE_CONFIG	"WBTEXT_PROFILE_CONFIG"
#define CMD_WBTEXT_WEIGHT_CONFIG	"WBTEXT_WEIGHT_CONFIG"
#define CMD_WBTEXT_TABLE_CONFIG		"WBTEXT_TABLE_CONFIG"
#define CMD_WBTEXT_DELTA_CONFIG		"WBTEXT_DELTA_CONFIG"
#define DEFAULT_WBTEXT_PROFILE_A_V2		"a -70 -75 70 10 -75 -128 0 10"
#define DEFAULT_WBTEXT_PROFILE_B_V2		"b -60 -75 70 10 -75 -128 0 10"
#define DEFAULT_WBTEXT_PROFILE_A_V3		"a -70 -75 70 10 -75 -128 0 10"
#define DEFAULT_WBTEXT_PROFILE_B_V3		"b -60 -75 70 10 -75 -128 0 10"
#define DEFAULT_WBTEXT_WEIGHT_RSSI_A	"RSSI a 65"
#define DEFAULT_WBTEXT_WEIGHT_RSSI_B	"RSSI b 65"
#define DEFAULT_WBTEXT_WEIGHT_CU_A	"CU a 35"
#define DEFAULT_WBTEXT_WEIGHT_CU_B	"CU b 35"
#define DEFAULT_WBTEXT_WEIGHT_ESTM_DL_A	"ESTM_DL a 70"
#define DEFAULT_WBTEXT_WEIGHT_ESTM_DL_B	"ESTM_DL b 70"
#define DEFAULT_WBTEXT_CU_RSSI_TRIG_A	-70
#define DEFAULT_WBTEXT_CU_RSSI_TRIG_B	-60
#ifdef WBTEXT_SCORE_V2
#define DEFAULT_WBTEXT_TABLE_RSSI_A	"RSSI a 0 55 100 55 60 90 \
60 70 60 70 80 20 80 90 0 90 128 0"
#define DEFAULT_WBTEXT_TABLE_RSSI_B	"RSSI b 0 55 100 55 60 90 \
60 70 60 70 80 20 80 90 0 90 128 0"
#define DEFAULT_WBTEXT_TABLE_CU_A	"CU a 0 30 100 30 80 20 \
80 100 20"
#define DEFAULT_WBTEXT_TABLE_CU_B	"CU b 0 10 100 10 70 20 \
70 100 20"
#else
#define DEFAULT_WBTEXT_TABLE_RSSI_A	"RSSI a 0 55 100 55 60 90 \
60 65 70 65 70 50 70 128 20"
#define DEFAULT_WBTEXT_TABLE_RSSI_B	"RSSI b 0 55 100 55 60 90 \
60 65 70 65 70 50 70 128 20"
#define DEFAULT_WBTEXT_TABLE_CU_A	"CU a 0 30 100 30 50 90 \
50 60 70 60 80 50 80 100 20"
#define DEFAULT_WBTEXT_TABLE_CU_B	"CU b 0 10 100 10 25 90 \
25 40 70 40 70 50 70 100 20"
#endif /* WBTEXT_SCORE_V2 */
#endif /* WBTEXT */

#define BUFSZ 8
#define BUFSZN	BUFSZ + 1

#define  MAXBANDS    2  /**< Maximum #of bands */
#define BAND_2G_INDEX      1
#define BAND_5G_INDEX      0

typedef union {
	wl_roam_prof_band_v1_t v1;
	wl_roam_prof_band_v2_t v2;
	wl_roam_prof_band_v3_t v3;
	wl_roam_prof_band_v4_t v4;
} wl_roamprof_band_t;

#ifdef WLWFDS
#define CMD_ADD_WFDS_HASH	"ADD_WFDS_HASH"
#define CMD_DEL_WFDS_HASH	"DEL_WFDS_HASH"
#endif /* WLWFDS */

#define CMD_MURX_BFE_CAP "MURX_BFE_CAP"

#ifdef SUPPORT_RSSI_SUM_REPORT
#define CMD_SET_RSSI_LOGGING				"SET_RSSI_LOGGING"
#define CMD_GET_RSSI_LOGGING				"GET_RSSI_LOGGING"
#define CMD_GET_RSSI_PER_ANT				"GET_RSSI_PER_ANT"
#endif /* SUPPORT_RSSI_SUM_REPORT */

#define CMD_GET_SNR							"GET_SNR"

#ifdef SUPPORT_AP_HIGHER_BEACONRATE
#define CMD_SET_AP_BEACONRATE				"SET_AP_BEACONRATE"
#define CMD_GET_AP_BASICRATE				"GET_AP_BASICRATE"
#endif /* SUPPORT_AP_HIGHER_BEACONRATE */

#ifdef SUPPORT_AP_RADIO_PWRSAVE
#define CMD_SET_AP_RPS						"SET_AP_RPS"
#define CMD_GET_AP_RPS						"GET_AP_RPS"
#define CMD_SET_AP_RPS_PARAMS				"SET_AP_RPS_PARAMS"
#endif /* SUPPORT_AP_RADIO_PWRSAVE */

#ifdef SUPPORT_AP_SUSPEND
#define CMD_SET_AP_SUSPEND			"SET_AP_SUSPEND"
#endif /* SUPPORT_AP_SUSPEND */

#ifdef SUPPORT_AP_BWCTRL
#define CMD_SET_AP_BW			"SET_AP_BW"
#define CMD_GET_AP_BW			"GET_AP_BW"
#endif /* SUPPORT_AP_BWCTRL */

/* miracast related definition */
#define MIRACAST_MODE_OFF	0
#define MIRACAST_MODE_SOURCE	1
#define MIRACAST_MODE_SINK	2

#ifdef CONNECTION_STATISTICS
#define CMD_GET_CONNECTION_STATS	"GET_CONNECTION_STATS"

struct connection_stats {
	u32 txframe;
	u32 txbyte;
	u32 txerror;
	u32 rxframe;
	u32 rxbyte;
	u32 txfail;
	u32 txretry;
	u32 txretrie;
	u32 txrts;
	u32 txnocts;
	u32 txexptime;
	u32 txrate;
	u8	chan_idle;
};
#endif /* CONNECTION_STATISTICS */

#ifdef SUPPORT_LQCM
#define CMD_SET_LQCM_ENABLE			"SET_LQCM_ENABLE"
#define CMD_GET_LQCM_REPORT			"GET_LQCM_REPORT"
#endif

static LIST_HEAD(miracast_resume_list);
static u8 miracast_cur_mode;

#ifdef DHD_LOG_DUMP
#define CMD_NEW_DEBUG_PRINT_DUMP	"DEBUG_DUMP"
#define SUBCMD_UNWANTED			"UNWANTED"
#define SUBCMD_DISCONNECTED		"DISCONNECTED"
void dhd_log_dump_trigger(dhd_pub_t *dhdp, int subcmd);
#endif /* DHD_LOG_DUMP */

#ifdef DHD_STATUS_LOGGING
#define CMD_DUMP_STATUS_LOG		"DUMP_STAT_LOG"
#define CMD_QUERY_STATUS_LOG		"QUERY_STAT_LOG"
#endif /* DHD_STATUS_LOGGING */

#ifdef DHD_HANG_SEND_UP_TEST
#define CMD_MAKE_HANG  "MAKE_HANG"
#endif /* CMD_DHD_HANG_SEND_UP_TEST */
#ifdef DHD_DEBUG_UART
extern bool dhd_debug_uart_is_running(struct net_device *dev);
#endif	/* DHD_DEBUG_UART */

#ifdef RTT_GEOFENCE_INTERVAL
#if defined(RTT_SUPPORT) && defined(WL_NAN)
#define CMD_GEOFENCE_INTERVAL	"GEOFENCE_INT"
#endif /* RTT_SUPPORT && WL_NAN */
#endif /* RTT_GEOFENCE_INTERVAL */

struct io_cfg {
	s8 *iovar;
	s32 param;
	u32 ioctl;
	void *arg;
	u32 len;
	struct list_head list;
};

#if defined(BCMFW_ROAM_ENABLE)
#define CMD_SET_ROAMPREF	"SET_ROAMPREF"

#define MAX_NUM_SUITES		10
#define WIDTH_AKM_SUITE		8
#define JOIN_PREF_RSSI_LEN		0x02
#endif /* BCMFW_ROAM_ENABLE */

#if defined(CONFIG_TIZEN)
/*
 * adding these private commands corresponding to atd-server's implementation
 * __atd_control_pm_state()
 */
#define CMD_POWERSAVEMODE_SET "SETPOWERSAVEMODE"
#define CMD_POWERSAVEMODE_GET "GETPOWERSAVEMODE"
#endif /* CONFIG_TIZEN */

#define CMD_DEBUG_VERBOSE          "DEBUG_VERBOSE"

#ifdef SET_PCIE_IRQ_CPU_CORE
#define CMD_PCIE_IRQ_CORE	"PCIE_IRQ_CORE"
#endif /* SET_PCIE_IRQ_CPU_CORE */

#ifdef DHD_PKT_LOGGING
#define CMD_PKTLOG_FILTER_ENABLE	"PKTLOG_FILTER_ENABLE"
#define CMD_PKTLOG_FILTER_DISABLE	"PKTLOG_FILTER_DISABLE"
#define CMD_PKTLOG_FILTER_PATTERN_ENABLE	"PKTLOG_FILTER_PATTERN_ENABLE"
#define CMD_PKTLOG_FILTER_PATTERN_DISABLE	"PKTLOG_FILTER_PATTERN_DISABLE"
#define CMD_PKTLOG_FILTER_ADD	"PKTLOG_FILTER_ADD"
#define CMD_PKTLOG_FILTER_DEL	"PKTLOG_FILTER_DEL"
#define CMD_PKTLOG_FILTER_INFO	"PKTLOG_FILTER_INFO"
#define CMD_PKTLOG_START	"PKTLOG_START"
#define CMD_PKTLOG_STOP		"PKTLOG_STOP"
#define CMD_PKTLOG_FILTER_EXIST "PKTLOG_FILTER_EXIST"
#define CMD_PKTLOG_MINMIZE_ENABLE	"PKTLOG_MINMIZE_ENABLE"
#define CMD_PKTLOG_MINMIZE_DISABLE	"PKTLOG_MINMIZE_DISABLE"
#define CMD_PKTLOG_CHANGE_SIZE	"PKTLOG_CHANGE_SIZE"
#define CMD_PKTLOG_DEBUG_DUMP	"PKTLOG_DEBUG_DUMP"
#endif /* DHD_PKT_LOGGING */

#ifdef TPUT_DEBUG_DUMP
#define CMD_TPUT_DEBUG_MODE_ENABLE "TPUT_DEBUG_MODE_ENABLE"
#define CMD_TPUT_DEBUG_MODE_DISABLE "TPUT_DEBUG_MODE_DISABLE"
#endif /* TPUT_DEBUG_DUMP */

#ifdef DHD_EVENT_LOG_FILTER
#define CMD_EWP_FILTER		"EWP_FILTER"
#endif /* DHD_EVENT_LOG_FILTER */

#ifdef WL_BCNRECV
#define CMD_BEACON_RECV "BEACON_RECV"
#endif /* WL_BCNRECV */
#ifdef WL_CAC_TS
#define CMD_CAC_TSPEC "CAC_TSPEC"
#endif /* WL_CAC_TS */
#ifdef WL_GET_CU
#define CMD_GET_CHAN_UTIL "GET_CU"
#endif /* WL_GET_CU */

#ifdef SUPPORT_SOFTAP_ELNA_BYPASS
#define CMD_SET_SOFTAP_ELNA_BYPASS				"SET_SOFTAP_ELNA_BYPASS"
#define CMD_GET_SOFTAP_ELNA_BYPASS				"GET_SOFTAP_ELNA_BYPASS"
#endif /* SUPPORT_SOFTAP_ELNA_BYPASS */

#ifdef WL_NAN
#define CMD_GET_NAN_STATUS	"GET_NAN_STATUS"
#endif /* WL_NAN */

#ifdef WL_TWT
#define CMD_TWT_SETUP		"TWT_SETUP"
#define CMD_TWT_TEARDOWN	"TWT_TEARDOWN"
#define CMD_TWT_INFO		"TWT_INFO_FRM"
#define CMD_TWT_STATUS_QUERY	"GET_TWT_STATUS"
#define CMD_TWT_CAPABILITY	"GET_TWT_CAP"
#define CMD_TWT_GET_STATS	"GET_TWT_STATISTICS"
#define CMD_TWT_CLR_STATS	"CLEAR_TWT_STATISTICS"
#define CMD_TWT_SOFTAP_ENABLE	"TWT_SOFTAP_ENABLE"
#endif /* WL_TWT */

#define CMD_GET_6G_SOFTAP_FREQ_LIST	"GET_6G_SOFTAP_FREQ_LIST"

/* drv command info structure */
typedef struct wl_drv_cmd_info {
	uint8  *command;        /* pointer to the actual command */
	uint16 tot_len;         /* total length of the command */
	uint16 bytes_written;   /* Bytes written for get response */
} wl_drv_cmd_info_t;

typedef struct wl_drv_sub_cmd wl_drv_sub_cmd_t;
typedef int (drv_cmd_handler_t)(struct net_device *dev,
		const wl_drv_sub_cmd_t *cmd, char *command, wl_drv_cmd_info_t *cmd_info);

struct wl_drv_sub_cmd {
	char *name;
	uint8  version;              /* cmd  version */
	uint16 id;                   /* id for the dongle f/w switch/case */
	uint16 type;                 /* base type of argument */
	drv_cmd_handler_t *handler;  /* cmd handler  */
};

#ifdef WL_MBO

#define CMD_MBO		"MBO"
enum {
	WL_MBO_CMD_NON_CHAN_PREF = 1,
	WL_MBO_CMD_CELL_DATA_CAP = 2
};
#define WL_ANDROID_MBO_FUNC(suffix) wl_android_mbo_subcmd_ ##suffix

static int wl_android_process_mbo_cmd(struct net_device *dev,
		char *command, int total_len);
static int wl_android_mbo_subcmd_cell_data_cap(struct net_device *dev,
		const wl_drv_sub_cmd_t *cmd, char *command, wl_drv_cmd_info_t *cmd_info);
static int wl_android_mbo_subcmd_non_pref_chan(struct net_device *dev,
		const wl_drv_sub_cmd_t *cmd, char *command, wl_drv_cmd_info_t *cmd_info);

static const wl_drv_sub_cmd_t mbo_cmd_list[] = {
	{"non_pref_chan", 0x01, WL_MBO_CMD_NON_CHAN_PREF,
	IOVT_BUFFER, WL_ANDROID_MBO_FUNC(non_pref_chan)
	},
	{"cell_data_cap", 0x01, WL_MBO_CMD_CELL_DATA_CAP,
	IOVT_BUFFER, WL_ANDROID_MBO_FUNC(cell_data_cap)
	},
	{NULL, 0, 0, 0, NULL}
};

#endif /* WL_MBO */

#ifdef WL_GENL
static s32 wl_genl_handle_msg(struct sk_buff *skb, struct genl_info *info);
static int wl_genl_init(void);
static int wl_genl_deinit(void);

extern struct net init_net;
/* attribute policy: defines which attribute has which type (e.g int, char * etc)
 * possible values defined in net/netlink.h
 */
static struct nla_policy wl_genl_policy[BCM_GENL_ATTR_MAX + 1] = {
	[BCM_GENL_ATTR_STRING] = { .type = NLA_NUL_STRING },
	[BCM_GENL_ATTR_MSG] = { .type = NLA_BINARY },
};

#define WL_GENL_VER 1
/* family definition */
static struct genl_family wl_genl_family = {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0))
	.id = GENL_ID_GENERATE,    /* Genetlink would generate the ID */
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0) */
	.hdrsize = 0,
	.name = "bcm-genl",        /* Netlink I/F for Android */
	.version = WL_GENL_VER,     /* Version Number */
	.maxattr = BCM_GENL_ATTR_MAX,
};

/* commands: mapping between the command enumeration and the actual function */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
struct genl_ops wl_genl_ops[] = {
	{
	.cmd = BCM_GENL_CMD_MSG,
	.flags = 0,
	.policy = wl_genl_policy,
	.doit = wl_genl_handle_msg,
	.dumpit = NULL,
	},
};
#else
struct genl_ops wl_genl_ops = {
	.cmd = BCM_GENL_CMD_MSG,
	.flags = 0,
	.policy = wl_genl_policy,
	.doit = wl_genl_handle_msg,
	.dumpit = NULL,

};
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0) */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0))
static struct genl_multicast_group wl_genl_mcast = {
	.id = GENL_ID_GENERATE,    /* Genetlink would generate the ID */
	.name = "bcm-genl-mcast",
};
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0))
static struct genl_multicast_group wl_genl_mcast[] = {
	 { .name = "bcm-genl-mcast", },
};
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0) */
#endif /* WL_GENL */

#ifdef SUPPORT_LQCM
#define LQCM_ENAB_MASK			0x000000FF	/* LQCM enable flag mask */
#define LQCM_TX_INDEX_MASK		0x0000FF00	/* LQCM tx index mask */
#define LQCM_RX_INDEX_MASK		0x00FF0000	/* LQCM rx index mask */

#define LQCM_TX_INDEX_SHIFT		8	/* LQCM tx index shift */
#define LQCM_RX_INDEX_SHIFT		16	/* LQCM rx index shift */
#endif /* SUPPORT_LQCM */

#ifdef DHD_SEND_HANG_PRIVCMD_ERRORS
#define NUMBER_SEQUENTIAL_PRIVCMD_ERRORS	7
static int priv_cmd_errors = 0;
#endif /* DHD_SEND_HANG_PRIVCMD_ERRORS */

/**
 * Extern function declarations (TODO: move them to dhd_linux.h)
 */
int dhd_net_bus_devreset(struct net_device *dev, uint8 flag);
int dhd_dev_init_ioctl(struct net_device *dev);
#ifdef WL_CFG80211
int wl_cfg80211_get_p2p_dev_addr(struct net_device *net, struct ether_addr *p2pdev_addr);
int wl_cfg80211_set_btcoex_dhcp(struct net_device *dev, dhd_pub_t *dhd, char *command);
#ifdef WES_SUPPORT
int wl_cfg80211_set_wes_mode(struct net_device *dev, int mode);
int wl_cfg80211_get_wes_mode(struct net_device *dev);
int wl_cfg80211_set_ncho_mode(struct net_device *dev, int mode);
int wl_cfg80211_get_ncho_mode(struct net_device *dev);
#endif /* WES_SUPPORT */
#else
int wl_cfg80211_get_p2p_dev_addr(struct net_device *net, struct ether_addr *p2pdev_addr)
{ return 0; }
int wl_cfg80211_set_p2p_noa(struct net_device *net, char* buf, int len)
{ return 0; }
int wl_cfg80211_get_p2p_noa(struct net_device *net, char* buf, int len)
{ return 0; }
int wl_cfg80211_set_p2p_ps(struct net_device *net, char* buf, int len)
{ return 0; }
int wl_cfg80211_set_p2p_ecsa(struct net_device *net, char* buf, int len)
{ return 0; }
int wl_cfg80211_increase_p2p_bw(struct net_device *net, char* buf, int len)
{ return 0; }
#endif /* WK_CFG80211 */
#ifdef WBTEXT
static int wl_android_wbtext(struct net_device *dev, char *command, int total_len);
static int wl_cfg80211_wbtext_btm_timer_threshold(struct net_device *dev,
	char *command, int total_len);
static int wl_cfg80211_wbtext_btm_delta(struct net_device *dev,
	char *command, int total_len);
static int wl_cfg80211_wbtext_estm_enable(struct net_device *dev,
	char *command, int total_len);
static int wlc_wbtext_get_roam_prof(struct net_device *ndev, wl_roamprof_band_t *rp,
	uint8 band, uint8 *roam_prof_ver, uint8 *roam_prof_size);
#endif /* WBTEXT */
#ifdef WES_SUPPORT
#ifdef WBTEXT
static int wl_android_wbtext_enable(struct net_device *dev, int mode);
#endif /* WBTEXT */
/* wl_roam.c */
extern int get_roamscan_mode(struct net_device *dev, int *mode);
extern int set_roamscan_mode(struct net_device *dev, int mode);
extern int get_roamscan_chanspec_list(struct net_device *dev, chanspec_t *chanspecs);
extern int set_roamscan_chanspec_list(struct net_device *dev, uint n, chanspec_t *chanspecs);
extern int add_roamscan_chanspec_list(struct net_device *dev, uint n, chanspec_t *chanspecs);

static char* legacy_cmdlist[] =
{
	CMD_GETROAMSCANCHLEGACY, CMD_ADDROAMSCANCHLEGACY,
	CMD_GETROAMSCANFQLEGACY, CMD_ADDROAMSCANFQLEGACY,
	CMD_GETROAMTRIGLEGACY, CMD_SETROAMTRIGLEGACY,
	CMD_REASSOCLEGACY, CMD_REASSOCFREQLEGACY,
	CMD_GETSCANCHANNELTIMELEGACY, CMD_SETSCANCHANNELTIMELEGACY,
	CMD_GETSCANUNASSOCTIMELEGACY, CMD_SETSCANUNASSOCTIMELEGACY,
	CMD_GETSCANPASSIVETIMELEGACY, CMD_SETSCANPASSIVETIMELEGACY,
	CMD_GETSCANHOMETIMELEGACY, CMD_SETSCANHOMETIMELEGACY,
	CMD_GETSCANHOMEAWAYTIMELEGACY, CMD_SETSCANHOMEAWAYTIMELEGACY,
	"\0"
};

static char* ncho_cmdlist[] =
{
	CMD_ROAMTRIGGER_GET, CMD_ROAMTRIGGER_SET,
	CMD_ROAMDELTA_GET, CMD_ROAMDELTA_SET,
	CMD_ROAMSCANPERIOD_GET, CMD_ROAMSCANPERIOD_SET,
	CMD_FULLROAMSCANPERIOD_GET, CMD_FULLROAMSCANPERIOD_SET,
	CMD_COUNTRYREV_GET, CMD_COUNTRYREV_SET,
	CMD_GETROAMSCANCONTROL,	CMD_SETROAMSCANCONTROL,
	CMD_GETROAMSCANCHANNELS, CMD_SETROAMSCANCHANNELS, CMD_ADDROAMSCANCHANNELS,
	CMD_GETROAMSCANFREQS, CMD_SETROAMSCANFREQS, CMD_ADDROAMSCANFREQS,
	CMD_SENDACTIONFRAME,
	CMD_REASSOC, CMD_REASSOCFREQ,
	CMD_GETSCANCHANNELTIME,	CMD_SETSCANCHANNELTIME,
	CMD_GETSCANUNASSOCTIME,	CMD_SETSCANUNASSOCTIME,
	CMD_GETSCANPASSIVETIME,	CMD_SETSCANPASSIVETIME,
	CMD_GETSCANHOMETIME, CMD_SETSCANHOMETIME,
	CMD_GETSCANHOMEAWAYTIME, CMD_SETSCANHOMEAWAYTIME,
	CMD_GETSCANNPROBES, CMD_SETSCANNPROBES,
	CMD_GETDFSSCANMODE,
	CMD_SETJOINPREFER,
	CMD_GETWESMODE,	CMD_SETWESMODE,
	CMD_GETROAMALLOWBAND, CMD_SETROAMALLOWBAND,
	"\0"
};
#endif /* WES_SUPPORT */
#ifdef ROAM_CHANNEL_CACHE
extern void wl_update_roamscan_cache_by_band(struct net_device *dev, int band);
#endif /* ROAM_CHANNEL_CACHE */

int wl_android_priority_roam_enable(struct net_device *dev, int mode);
#ifdef CONFIG_SILENT_ROAM
int wl_android_sroam_turn_on(struct net_device *dev, int mode);
#endif /* CONFIG_SILENT_ROAM */

#ifdef ENABLE_4335BT_WAR
extern int bcm_bt_lock(int cookie);
extern void bcm_bt_unlock(int cookie);
static int lock_cookie_wifi = 'W' | 'i'<<8 | 'F'<<16 | 'i'<<24;	/* cookie is "WiFi" */
#endif /* ENABLE_4335BT_WAR */

extern bool ap_fw_loaded;
extern char iface_name[IFNAMSIZ];
#ifdef DHD_PM_CONTROL_FROM_FILE
extern bool g_pm_control;
#endif	/* DHD_PM_CONTROL_FROM_FILE */

/* private command support for restoring roam/scan parameters */
#if defined(SUPPORT_RESTORE_SCAN_PARAMS) || defined(WES_SUPPORT)
#define CMD_RESTORE_SCAN_PARAMS "RESTORE_SCAN_PARAMS"

typedef int (*PRIV_CMD_HANDLER) (struct net_device *dev, char *command);
typedef int (*PRIV_CMD_HANDLER_WITH_LEN) (struct net_device *dev, char *command, int total_len);

enum {
	RESTORE_TYPE_UNSPECIFIED = 0,
	RESTORE_TYPE_PRIV_CMD = 1,
	RESTORE_TYPE_PRIV_CMD_WITH_LEN = 2
};

typedef struct android_restore_scan_params {
	char command[64];
	int parameter;
	int cmd_type;
	union {
		PRIV_CMD_HANDLER cmd_handler;
		PRIV_CMD_HANDLER_WITH_LEN cmd_handler_w_len;
	};
} android_restore_scan_params_t;

/* function prototypes of private command handler */
int wl_android_default_set_scan_params(struct net_device *dev, char *command, int total_len);
static int wl_android_set_roam_trigger(struct net_device *dev, char* command);
int wl_android_set_roam_delta(struct net_device *dev, char* command);
int wl_android_set_roam_scan_period(struct net_device *dev, char* command);
int wl_android_set_full_roam_scan_period(struct net_device *dev, char* command);
int wl_android_set_roam_scan_control(struct net_device *dev, char *command);
int wl_android_set_scan_channel_time(struct net_device *dev, char *command);
int wl_android_set_scan_home_time(struct net_device *dev, char *command);
int wl_android_set_scan_home_away_time(struct net_device *dev, char *command);
int wl_android_set_scan_nprobes(struct net_device *dev, char *command);
static int wl_android_set_band(struct net_device *dev, char *command);
int wl_android_set_wes_mode(struct net_device *dev, char *command);
int wl_android_set_okc_mode(struct net_device *dev, char *command);
int wl_android_set_scan_passive_time(struct net_device *dev, char *command);
int wl_android_set_roam_allowed_band(struct net_device *dev, char *command);

/* default values */
#ifdef ROAM_API
#define DEFAULT_ROAM_TIRGGER	-75
#define DEFAULT_ROAM_DELTA	10
#define DEFAULT_ROAMSCANPERIOD	10
#define DEFAULT_FULLROAMSCANPERIOD_SET	120
#endif /* ROAM_API */
#ifdef WES_SUPPORT
#define DEFAULT_ROAMSCANCONTROL	0
#define DEFAULT_SCANCHANNELTIME	40
#ifdef BCM4361_CHIP
#define DEFAULT_SCANHOMETIME	60
#else
#define DEFAULT_SCANHOMETIME	45
#endif /* BCM4361_CHIP */
#define DEFAULT_SCANPROBES		2
#define DEFAULT_DFSSCANMODE		1
#define DEFAULT_WESMODE			0
#define DEFAULT_OKCMODE			1
#endif /* WES_SUPPORT */
#define DEFAULT_BAND		0
#ifdef WBTEXT
#define DEFAULT_WBTEXT_ENABLE	1
#endif /* WBTEXT */

/* restoring parameter list, please don't change order */
static android_restore_scan_params_t restore_params[] =
{
/* wbtext need to be disabled while updating roam/scan parameters */
#ifdef WBTEXT
	{ CMD_WBTEXT_ENABLE, 0, RESTORE_TYPE_PRIV_CMD_WITH_LEN,
		.cmd_handler_w_len = wl_android_wbtext},
#endif /* WBTEXT */
#ifdef ROAM_API
	{ CMD_ROAMTRIGGER_SET, DEFAULT_ROAM_TIRGGER,
		RESTORE_TYPE_PRIV_CMD, .cmd_handler = wl_android_set_roam_trigger},
	{ CMD_ROAMDELTA_SET, DEFAULT_ROAM_DELTA,
		RESTORE_TYPE_PRIV_CMD, .cmd_handler = wl_android_set_roam_delta},
	{ CMD_ROAMSCANPERIOD_SET, DEFAULT_ROAMSCANPERIOD,
		RESTORE_TYPE_PRIV_CMD, .cmd_handler = wl_android_set_roam_scan_period},
	{ CMD_FULLROAMSCANPERIOD_SET, DEFAULT_FULLROAMSCANPERIOD_SET,
		RESTORE_TYPE_PRIV_CMD, .cmd_handler = wl_android_set_full_roam_scan_period},
#endif /* ROAM_API */
#ifdef WES_SUPPORT
	{ CMD_SETROAMSCANCONTROL, DEFAULT_ROAMSCANCONTROL,
		RESTORE_TYPE_PRIV_CMD, .cmd_handler = wl_android_set_roam_scan_control},
	{ CMD_SETSCANCHANNELTIME, DEFAULT_SCANCHANNELTIME,
		RESTORE_TYPE_PRIV_CMD, .cmd_handler = wl_android_set_scan_channel_time},
	{ CMD_SETSCANHOMETIME, DEFAULT_SCANHOMETIME,
		RESTORE_TYPE_PRIV_CMD, .cmd_handler = wl_android_set_scan_home_time},
	{ CMD_GETSCANHOMEAWAYTIME, DHD_SCAN_HOME_AWAY_TIME,
		RESTORE_TYPE_PRIV_CMD, .cmd_handler = wl_android_set_scan_home_away_time},
	{ CMD_SETSCANNPROBES, DEFAULT_SCANPROBES,
		RESTORE_TYPE_PRIV_CMD, .cmd_handler = wl_android_set_scan_nprobes},
	{ CMD_SETWESMODE, DEFAULT_WESMODE,
		RESTORE_TYPE_PRIV_CMD, .cmd_handler = wl_android_set_wes_mode},
	{ CMD_SETSCANPASSIVETIME, DHD_SCAN_PASSIVE_TIME,
		RESTORE_TYPE_PRIV_CMD, .cmd_handler = wl_android_set_scan_passive_time},
	{ CMD_SETROAMALLOWBAND, WLC_ROAM_ALLOW_BAND_AUTO,
		RESTORE_TYPE_PRIV_CMD, .cmd_handler = wl_android_set_roam_allowed_band},
#endif /* WES_SUPPORT */
	{ CMD_SETBAND, DEFAULT_BAND,
		RESTORE_TYPE_PRIV_CMD, .cmd_handler = wl_android_set_band},
#ifdef WBTEXT
	{ CMD_WBTEXT_ENABLE, DEFAULT_WBTEXT_ENABLE,
		RESTORE_TYPE_PRIV_CMD_WITH_LEN, .cmd_handler_w_len = wl_android_wbtext},
#endif /* WBTEXT */
	{ "\0", 0, RESTORE_TYPE_UNSPECIFIED, .cmd_handler = NULL}
};
#endif /* SUPPORT_RESTORE_SCAN_PARAMS || WES_SUPPORT */

#ifdef SUPPORT_LATENCY_CRITICAL_DATA
#define CMD_GET_LATENCY_CRITICAL_DATA	"GET_LATENCY_CRT_DATA"
#define CMD_SET_LATENCY_CRITICAL_DATA	"SET_LATENCY_CRT_DATA"
int wl_android_set_latency_crt_data(struct net_device *dev, int mode);
#endif	/* SUPPORT_LATENCY_CRITICAL_DATA */

#ifdef WL_LATENCY_CONFIG
#define CMD_SET_LOW_LATENCY	"LOW_LATENCY"
#define DEF_LOW_LATENCY_PERIOD	30u
#endif /* WL_LATENCY_CONFIG */

typedef struct android_priv_cmd_log_cfg_table {
	char command[64];
	int  enable;
} android_priv_cmd_log_cfg_table_t;

static android_priv_cmd_log_cfg_table_t loging_params[] = {
	{CMD_GET_SNR, FALSE},
#ifdef SUPPORT_LQCM
	{CMD_GET_LQCM_REPORT, FALSE},
#endif
#ifdef WL_GET_CU
	{CMD_GET_CHAN_UTIL, FALSE},
#endif
	{"\0", FALSE}
};

#ifdef WL_UWB_COEX
#define CMD_SET_UWB_COEX_ENABLE "SET_UWBCX_ENABLE"
#define CMD_GET_UWB_COEX_ENABLE "GET_UWBCX_ENABLE"

#define CMD_SET_UWB_COEX_PREPARE_TIME "SET_UWBCX_PREPARE_TIME"
#define CMD_GET_UWB_COEX_PREPARE_TIME "GET_UWBCX_PREPARE_TIME"

static int wl_android_uwbcx_set_enable(struct net_device *dev, const char *command);
static int wl_android_uwbcx_get_enable(struct net_device *dev, char *command, int tot_len);

static int wl_android_uwbcx_set_prepare_time(struct net_device *dev, const char *command);
static int wl_android_uwbcx_get_prepare_time(struct net_device *dev, char *command, int tot_len);
#endif /* WL_UWB_COEX */

#ifdef WL_DUAL_STA
#define CMD_SET_PRIMARY_INET "SET_PRIMARY_INET"
#endif /* WL_DUAL_STA */

#define CMD_SETWSECINFO	"SETWSECINFO"

#if defined(LIMIT_AP_BW)
#define CMD_SET_SOFTAP_BW "CMD_SET_SOFTAP_BW"
#define CMD_GET_SOFTAP_BW "CMD_GET_SOFTAP_BW"
static int wl_android_set_softap_bw(struct net_device *ndev, char *command);
static int wl_android_get_softap_bw(struct net_device *ndev, char *command, int total_len);
#endif /* LIMIT_AP_BW */

/**
 * Local (static) functions and variables
 */

/* Initialize g_wifi_on to 1 so dhd_bus_start will be called for the first
 * time (only) in dhd_open, subsequential wifi on will be handled by
 * wl_android_wifi_on
 */
int g_wifi_on = TRUE;

/**
 * Local (static) function definitions
 */

static char* wl_android_get_band_str(u16 band)
{
	switch (band) {
#ifdef WL_6G_BAND
		case WLC_BAND_6G:
			return "6G";
#endif /* WL_6G_BAND */
		case WLC_BAND_5G:
			return "5G";
		case WLC_BAND_2G:
			return "2G";
		default:
			WL_ERR(("Unkown band: %d \n", band));
			return "Unknown band";
	}
}

#ifdef WBTEXT
static int wl_android_bandstr_to_fwband(char *band, u8 *fw_band)
{
	int err = BCME_OK;

	if (!strcasecmp(band, "a")) {
		*fw_band = WLC_BAND_5G;
	} else if (!strcasecmp(band, "b")) {
		*fw_band = WLC_BAND_2G;
#ifdef WL_6G_BAND
	} else if (!strcasecmp(band, "6g")) {
		*fw_band = WLC_BAND_6G;
#endif /* WL_6G_BAND */
	} else if (!strcasecmp(band, "all")) {
		*fw_band = WLC_BAND_ALL;
	} else {
		err = BCME_BADBAND;
	}

	return err;
}
#endif /* WBTEXT */

#ifdef WLWFDS
static int wl_android_set_wfds_hash(
	struct net_device *dev, char *command, bool enable)
{
	int error = 0;
	wl_p2p_wfds_hash_t *wfds_hash = NULL;
	char *smbuf = NULL;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);

	smbuf = (char *)MALLOC(cfg->osh, WLC_IOCTL_MAXLEN);
	if (smbuf == NULL) {
		DHD_ERROR(("wl_android_set_wfds_hash: failed to allocated memory %d bytes\n",
			WLC_IOCTL_MAXLEN));
		return -ENOMEM;
	}

	if (enable) {
		wfds_hash = (wl_p2p_wfds_hash_t *)(command + strlen(CMD_ADD_WFDS_HASH) + 1);
		error = wldev_iovar_setbuf(dev, "p2p_add_wfds_hash", wfds_hash,
			sizeof(wl_p2p_wfds_hash_t), smbuf, WLC_IOCTL_MAXLEN, NULL);
	}
	else {
		wfds_hash = (wl_p2p_wfds_hash_t *)(command + strlen(CMD_DEL_WFDS_HASH) + 1);
		error = wldev_iovar_setbuf(dev, "p2p_del_wfds_hash", wfds_hash,
			sizeof(wl_p2p_wfds_hash_t), smbuf, WLC_IOCTL_MAXLEN, NULL);
	}

	if (error) {
		DHD_ERROR(("wl_android_set_wfds_hash: failed to %s, error=%d\n", command, error));
	}

	if (smbuf) {
		MFREE(cfg->osh, smbuf, WLC_IOCTL_MAXLEN);
	}
	return error;
}
#endif /* WLWFDS */

static int wl_android_get_link_speed(struct net_device *net, char *command, int total_len)
{
	int link_speed;
	int bytes_written;
	int error;

	error = wldev_get_link_speed(net, &link_speed);
	if (error) {
		DHD_ERROR(("Get linkspeed failed \n"));
		return -1;
	}

	/* Convert Kbps to Android Mbps */
	link_speed = link_speed / 1000;
	bytes_written = snprintf(command, total_len, "LinkSpeed %d", link_speed);
	DHD_INFO(("wl_android_get_link_speed: command result is %s\n", command));
	return bytes_written;
}

static int wl_android_get_rssi(struct net_device *net, char *command, int total_len)
{
	wlc_ssid_t ssid = {0, {0}};
	int bytes_written = 0;
	int error = 0;
	scb_val_t scbval;
	char *delim = NULL;
	struct net_device *target_ndev = net;
#ifdef WL_VIRTUAL_APSTA
	char *pos = NULL;
	struct bcm_cfg80211 *cfg;
#endif /* WL_VIRTUAL_APSTA */

	delim = strchr(command, ' ');
	/* For Ap mode rssi command would be
	 * driver rssi <sta_mac_addr>
	 * for STA/GC mode
	 * driver rssi
	*/
	if (delim) {
		/* Ap/GO mode
		* driver rssi <sta_mac_addr>
		*/
		DHD_TRACE(("wl_android_get_rssi: cmd:%s\n", delim));
		/* skip space from delim after finding char */
		delim++;
		if (!(bcm_ether_atoe((delim), &scbval.ea))) {
			DHD_ERROR(("wl_android_get_rssi: address err\n"));
			return -1;
		}
		scbval.val = htod32(0);
		DHD_TRACE(("wl_android_get_rssi: address:"MACDBG, MAC2STRDBG(scbval.ea.octet)));
#ifdef WL_VIRTUAL_APSTA
		/* RSDB AP may have another virtual interface
		 * In this case, format of private command is as following,
		 * DRIVER rssi <sta_mac_addr> <AP interface name>
		 */

		/* Current position is start of MAC address string */
		pos = delim;
		delim = strchr(pos, ' ');
		if (delim) {
			/* skip space from delim after finding char */
			delim++;
			if (strnlen(delim, IFNAMSIZ)) {
				cfg = wl_get_cfg(net);
				target_ndev = wl_get_ap_netdev(cfg, delim);
				if (target_ndev == NULL)
					target_ndev = net;
			}
		}
#endif /* WL_VIRTUAL_APSTA */
	}
	else {
		/* STA/GC mode */
		bzero(&scbval, sizeof(scb_val_t));
	}

	error = wldev_get_rssi(target_ndev, &scbval);
	if (error)
		return -1;

	error = wldev_get_ssid(target_ndev, &ssid);
	if (error)
		return -1;
	if ((ssid.SSID_len == 0) || (ssid.SSID_len > DOT11_MAX_SSID_LEN)) {
		DHD_ERROR(("wl_android_get_rssi: wldev_get_ssid failed\n"));
	} else if (total_len <= ssid.SSID_len) {
		return -ENOMEM;
	} else {
		memcpy(command, ssid.SSID, ssid.SSID_len);
		bytes_written = ssid.SSID_len;
	}
	if ((total_len - bytes_written) < (strlen(" rssi -XXX") + 1))
		return -ENOMEM;

	bytes_written += scnprintf(&command[bytes_written], total_len - bytes_written,
		" rssi %d", scbval.val);
	command[bytes_written] = '\0';

	DHD_TRACE(("wl_android_get_rssi: command result is %s (%d)\n", command, bytes_written));
	return bytes_written;
}

static int wl_android_set_suspendopt(struct net_device *dev, char *command)
{
	int suspend_flag;
	int ret_now;
	int ret = 0;

	suspend_flag = *(command + strlen(CMD_SETSUSPENDOPT) + 1) - '0';

	if (suspend_flag != 0) {
		suspend_flag = 1;
	}
	ret_now = net_os_set_suspend_disable(dev, suspend_flag);

	if (ret_now != suspend_flag) {
		if (!(ret = net_os_set_suspend(dev, ret_now, 1))) {
			DHD_INFO(("wl_android_set_suspendopt: Suspend Flag %d -> %d\n",
				ret_now, suspend_flag));
		} else {
			DHD_ERROR(("wl_android_set_suspendopt: failed %d\n", ret));
		}
	}

	return ret;
}

static int wl_android_set_suspendmode(struct net_device *dev, char *command)
{
	int ret = 0;

#if (!defined(CONFIG_HAS_EARLYSUSPEND) || !defined(DHD_USE_EARLYSUSPEND)) || \
	defined(DHD_USE_PM_SLEEP)
	int suspend_flag;

	suspend_flag = *(command + strlen(CMD_SETSUSPENDMODE) + 1) - '0';
	if (suspend_flag != 0) {
		suspend_flag = 1;
#if defined(DHD_USE_PM_SLEEP)
		/* The suspend setting is handled in linux pm_nofi callback. */
		DHD_INFO(("wl_android_set_suspendmode: Suspend Mode %d doesn't set\n",
				suspend_flag));
		return ret;
#endif /* DHD_USE_PM_SLEEP */
	}

	if (!(ret = net_os_set_suspend(dev, suspend_flag, 0)))
		DHD_INFO(("wl_android_set_suspendmode: Suspend Mode %d\n", suspend_flag));
	else
		DHD_ERROR(("wl_android_set_suspendmode: failed %d\n", ret));
#endif /* (!CONFIG_HAS_EARLYSUSPEND || !DHD_USE_EARLYSUSPEND) || DHD_USE_PM_SLEEP */

	return ret;
}

int wl_android_get_80211_mode(struct net_device *dev, char *command, int total_len)
{
	uint8 mode[5];
	int  error = 0;
	int bytes_written = 0;

	error = wldev_get_mode(dev, mode, sizeof(mode));
	if (error)
		return -1;

	DHD_INFO(("wl_android_get_80211_mode: mode:%s\n", mode));
	bytes_written = snprintf(command, total_len, "%s %s", CMD_80211_MODE, mode);
	DHD_INFO(("wl_android_get_80211_mode: command:%s EXIT\n", command));
	return bytes_written;

}

extern chanspec_t
wl_chspec_driver_to_host(chanspec_t chanspec);
int wl_android_get_chanspec(struct net_device *dev, char *command, int total_len)
{
	int error = 0;
	int bytes_written = 0;
	int chsp = {0};
	uint16 band = 0;
	uint16 bw = 0;
	uint16 channel = 0;
	u32 sb = 0;
	chanspec_t chanspec;

	/* command is
	 * driver chanspec
	 */
	error = wldev_iovar_getint(dev, "chanspec", &chsp);
	if (error)
		return -1;

	chanspec = wl_chspec_driver_to_host(chsp);
	DHD_INFO(("wl_android_get_80211_mode: return value of chanspec:%x\n", chanspec));

	channel = chanspec & WL_CHANSPEC_CHAN_MASK;
	band = chanspec & WL_CHANSPEC_BAND_MASK;
	bw = chanspec & WL_CHANSPEC_BW_MASK;
	UNUSED_PARAMETER(band);

	DHD_INFO(("wl_android_get_80211_mode: channel:%d band:%d bandwidth:%d\n",
		channel, band, bw));

	if (bw == WL_CHANSPEC_BW_160) {
		bw = WL_CH_BANDWIDTH_160MHZ;
	} else if (bw == WL_CHANSPEC_BW_80) {
		bw = WL_CH_BANDWIDTH_80MHZ;
	} else if (bw == WL_CHANSPEC_BW_40) {
		bw = WL_CH_BANDWIDTH_40MHZ;
	} else if (bw == WL_CHANSPEC_BW_20) {
		bw = WL_CH_BANDWIDTH_20MHZ;
	} else {
		bw = WL_CH_BANDWIDTH_20MHZ;
	}

	if (bw == WL_CH_BANDWIDTH_40MHZ) {
		if (CHSPEC_SB_UPPER(chanspec)) {
			channel += CH_10MHZ_APART;
		} else {
			channel -= CH_10MHZ_APART;
		}
	}
	else if (bw == WL_CH_BANDWIDTH_80MHZ) {
		sb = chanspec & WL_CHANSPEC_CTL_SB_MASK;
		if (sb == WL_CHANSPEC_CTL_SB_LL) {
			channel -= (CH_10MHZ_APART + CH_20MHZ_APART);
		} else if (sb == WL_CHANSPEC_CTL_SB_LU) {
			channel -= CH_10MHZ_APART;
		} else if (sb == WL_CHANSPEC_CTL_SB_UL) {
			channel += CH_10MHZ_APART;
		} else {
			/* WL_CHANSPEC_CTL_SB_UU */
			channel += (CH_10MHZ_APART + CH_20MHZ_APART);
		}
	} else if (bw == WL_CH_BANDWIDTH_160MHZ) {
		channel = wf_chspec_primary20_chan(chanspec);
	}
	bytes_written = snprintf(command, total_len, "%s channel %d band %s bw %d", CMD_CHANSPEC,
		channel, wl_android_get_band_str(CHSPEC2WLC_BAND(chanspec)), bw);

	DHD_INFO(("wl_android_get_chanspec: command:%s EXIT\n", command));
	return bytes_written;

}

/* returns current datarate datarate returned from firmware are in 500kbps */
int wl_android_get_datarate(struct net_device *dev, char *command, int total_len)
{
	int  error = 0;
	int datarate = 0;
	int bytes_written = 0;

	error = wldev_get_datarate(dev, &datarate);
	if (error)
		return -1;

	DHD_INFO(("wl_android_get_datarate: datarate:%d\n", datarate));

	bytes_written = snprintf(command, total_len, "%s %d", CMD_DATARATE, (datarate/2));
	return bytes_written;
}

int wl_android_get_assoclist(struct net_device *dev, char *command, int total_len)
{
	int  error = 0;
	int bytes_written = 0;
	uint i;
	int len = 0;
	char mac_buf[MAX_NUM_OF_ASSOCLIST *
		sizeof(struct ether_addr) + sizeof(uint)] = {0};
	struct maclist *assoc_maclist = (struct maclist *)mac_buf;

	DHD_TRACE(("wl_android_get_assoclist: ENTER\n"));

	assoc_maclist->count = htod32(MAX_NUM_OF_ASSOCLIST);

	error = wldev_ioctl_get(dev, WLC_GET_ASSOCLIST, assoc_maclist, sizeof(mac_buf));
	if (error)
		return -1;

	assoc_maclist->count = dtoh32(assoc_maclist->count);
	bytes_written = snprintf(command, total_len, "%s listcount: %d Stations:",
		CMD_ASSOC_CLIENTS, assoc_maclist->count);

	for (i = 0; i < assoc_maclist->count; i++) {
		len = snprintf(command + bytes_written, total_len - bytes_written, " " MACDBG,
			MAC2STRDBG(assoc_maclist->ea[i].octet));
		/* A return value of '(total_len - bytes_written)' or more means that the
		 * output was truncated
		 */
		if ((len > 0) && (len < (total_len - bytes_written))) {
			bytes_written += len;
		} else {
			DHD_ERROR(("wl_android_get_assoclist: Insufficient buffer %d,"
				" bytes_written %d\n",
				total_len, bytes_written));
			bytes_written = -1;
			break;
		}
	}
	return bytes_written;
}

extern chanspec_t
wl_chspec_host_to_driver(chanspec_t chanspec);
static int wl_android_set_csa(struct net_device *dev, char *command)
{
	int error = 0;
	char smbuf[WLC_IOCTL_SMLEN];
	wl_chan_switch_t csa_arg = {0, };
	u32 chnsp = 0;
	int err = 0;

	DHD_INFO(("wl_android_set_csa: command:%s\n", command));

	command = (command + strlen(CMD_SET_CSA));
	/* Order is mode, count channel */
	if (!*++command) {
		DHD_ERROR(("wl_android_set_csa:error missing arguments\n"));
		return -1;
	}
	csa_arg.mode = bcm_atoi(command);

	if (csa_arg.mode != 0 && csa_arg.mode != 1) {
		DHD_ERROR(("Invalid mode\n"));
		return -1;
	}

	if (!*++command) {
		DHD_ERROR(("wl_android_set_csa: error missing count\n"));
		return -1;
	}
	command++;
	csa_arg.count = bcm_atoi(command);

	csa_arg.reg = 0;
	csa_arg.chspec = 0;
	command += 2;
	if (!*command) {
		DHD_ERROR(("wl_android_set_csa: error missing channel\n"));
		return -1;
	}

	chnsp = wf_chspec_aton(command);
	if (chnsp == 0)	{
		DHD_ERROR(("wl_android_set_csa:chsp is not correct\n"));
		return -1;
	}
	chnsp = wl_chspec_host_to_driver(chnsp);
	csa_arg.chspec = chnsp;

	if (chnsp & WL_CHANSPEC_BAND_5G) {
		u32 chanspec = chnsp;
		err = wldev_iovar_getint(dev, "per_chan_info", &chanspec);
		if (!err) {
			if ((chanspec & WL_CHAN_RADAR) || (chanspec & WL_CHAN_PASSIVE)) {
				DHD_ERROR(("Channel is radar sensitive\n"));
				return -1;
			}
			if (chanspec == 0) {
				DHD_ERROR(("Invalid hw channel\n"));
				return -1;
			}
		} else  {
			DHD_ERROR(("does not support per_chan_info\n"));
			return -1;
		}
		DHD_INFO(("non radar sensitivity\n"));
	}
	error = wldev_iovar_setbuf(dev, "csa", &csa_arg, sizeof(csa_arg),
		smbuf, sizeof(smbuf), NULL);
	if (error) {
		DHD_ERROR(("wl_android_set_csa:set csa failed:%d\n", error));
		return -1;
	}
	return 0;
}

static int
wl_android_set_bcn_li_dtim(struct net_device *dev, char *command)
{
	int ret = 0;
	int dtim;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);

	dtim = *(command + strlen(CMD_SETDTIM_IN_SUSPEND) + 1) - '0';

	if (dtim > (MAX_DTIM_ALLOWED_INTERVAL / MAX_DTIM_SKIP_BEACON_INTERVAL)) {
		DHD_ERROR(("%s: failed, invalid dtim %d\n",
			__FUNCTION__, dtim));
		return BCME_ERROR;
	}

	cfg->suspend_bcn_li_dtim = dtim;
#ifdef OEM_ANDROID
	if (cfg->soft_suspend) {
		wl_cfg80211_set_suspend_bcn_li_dtim(cfg, dev, TRUE);
	}
#endif /* OEM_ANDROID */

	return ret;
}

static int
wl_android_set_max_dtim(struct net_device *dev, char *command)
{
	int ret = 0;
	int dtim_flag;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);

	dtim_flag = *(command + strlen(CMD_MAXDTIM_IN_SUSPEND) + 1) - '0';
	WL_INFORM(("use MAX bcn_li_dtim in suspend %s\n",
			(dtim_flag ? "Enable" : "Disable")));

	cfg->max_dtim_enable = dtim_flag ? TRUE : FALSE;

#ifdef OEM_ANDROID
	if (cfg->soft_suspend) {
		wl_cfg80211_set_suspend_bcn_li_dtim(cfg, dev, TRUE);
	}
#endif /* OEM_ANDROID */

	return ret;
}

#ifdef DISABLE_DTIM_IN_SUSPEND
static int
wl_android_set_disable_dtim_in_suspend(struct net_device *dev, char *command)
{
	int ret = 0;
	int dtim_flag;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);

	dtim_flag = *(command + strlen(CMD_DISDTIM_IN_SUSPEND) + 1) - '0';

	cfg->disable_dtim_in_suspend = dtim_flag ? TRUE : FALSE;
#ifdef OEM_ANDROID
	if (cfg->soft_suspend) {
		wl_cfg80211_set_suspend_bcn_li_dtim(cfg, dev, TRUE);
	}
#endif /* OEM_ANDROID */

	WL_INFORM_MEM(("wl_android_set_disable_dtim_in_suspend: "
			"use Disable bcn_li_dtim in suspend %s\n",
			(dtim_flag ? "Enable" : "Disable")));

	return ret;
}
#endif /* DISABLE_DTIM_IN_SUSPEND */

typedef enum band_define {
	BAND_DEF_2G5G = 0,	/* 0: Auto (2.4GHz + 5GHz) */
	BAND_DEF_5G,		/* 1: 5GHz */
	BAND_DEF_2G,		/* 2: 2.4GHz */
	BAND_DEF_2G5G6G,	/* 3, Auto (2.4GHz + 5GHz + 6GHz) */
	BAND_DEF_6G,		/* 4: 6GHz */
	BAND_DEF_5G6G,		/* 5: 5GHz + 6GHz */
	BAND_DEF_2G6G,		/* 6: 2.4GHz + 6GHz */
	BAMD_DEF_MAX
} band_define_t;

static int
wl_android_get_band(struct net_device *dev, char *command, int total_len)
{
	uint band, band_def = 0;
	int bytes_written;
	int error = BCME_OK;
#ifdef WL_6G_BAND
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
#endif /* WL_6G_BAND */

	error = wldev_iovar_getint(dev, "if_band", &band);
	if (error == BCME_UNSUPPORTED) {
		error = wldev_get_band(dev, &band);
		if (error) {
			return BCME_ERROR;
		}
	}

	/* Changed Band types to Band Definition */
	switch (band) {
		case WLC_BAND_AUTO:
#ifdef WL_6G_BAND
			if (cfg->band_6g_supported) {
				band_def = BAND_DEF_2G5G6G;
				break;
			}
#endif /* WL_6G_BAND */
			band_def = BAND_DEF_2G5G;
			break;
		case WLC_BAND_2G:
			band_def = BAND_DEF_2G;
			break;
		case WLC_BAND_5G:
#ifdef WL_6G_BAND
		case WLC_BAND_6G:
			if (cfg->band_6g_supported) {
				band_def = BAND_DEF_5G6G;
				break;
			}
#endif /* WL_6G_BAND */
			band_def = BAND_DEF_5G;
			break;
		default:
			WL_ERR(("Unkown band: %d\n", band));
			return BCME_ERROR;
	}

	bytes_written = snprintf(command, total_len, "Band %d", band_def);
	return bytes_written;
}

static int
wl_android_set_band(struct net_device *dev, char *command)
{
	int error = BCME_OK;
	uint band_def, band = 0;
	band_def = *(command + strlen(CMD_SETBAND) + 1) - '0';

	/* Changed Band Definition to Band types */
	switch (band_def) {
		case BAND_DEF_2G:
			band = WLC_BAND_2G;
			break;
#ifndef WL_6G_BAND
		case BAND_DEF_2G5G:
			band = WLC_BAND_AUTO;
			break;
		case BAND_DEF_5G:
			band = WLC_BAND_5G;
			break;
#else
		case BAND_DEF_2G5G6G:
			band = WLC_BAND_AUTO;
			break;
		case BAND_DEF_5G6G:
			band = WLC_BAND_5G;
			break;
		/* Can't set only 5GHz or 6GHz band on 6GHz supported device. */
		case BAND_DEF_6G:
		case BAND_DEF_2G6G:
#endif /* !WL_6G_BAND */
		default:
			WL_ERR(("Unsupported band definition: %d\n", band_def));
			return BCME_ERROR;
	}

#ifdef WL_HOST_BAND_MGMT
	if ((error = wl_cfg80211_set_band(dev, band)) < 0) {
		if (error == BCME_UNSUPPORTED) {
			/* If roam_var is unsupported, fallback to the original method */
			WL_ERR(("WL_HOST_BAND_MGMT defined, "
				"but roam_band iovar unsupported in the firmware\n"));
		} else {
			error = BCME_ERROR;
		}
	}
	if (((error == BCME_OK) && (band == WLC_BAND_AUTO)) || (error == BCME_UNSUPPORTED)) {
		/* Apply if roam_band iovar is not supported or band setting is AUTO */
		error = wldev_set_band(dev, band);
	}
#else
	error = wl_cfg80211_set_if_band(dev, band);
#endif /* WL_HOST_BAND_MGMT */
#ifdef ROAM_CHANNEL_CACHE
	wl_update_roamscan_cache_by_band(dev, band);
#endif /* ROAM_CHANNEL_CACHE */
	return error;
}

#if defined(WES_SUPPORT) && defined(WBTEXT)
static bool wl_android_check_wbtext_support(struct net_device *dev)
{
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(dev);
	return dhdp->wbtext_support;
}
#endif /* WES_SUPPORT && WBTEXT */

int
wl_android_rcroam_turn_on(struct net_device *dev, int rcroam_enab)
{
	int ret = BCME_OK;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(dev);
	u8 ioctl_buf[WLC_IOCTL_SMLEN];
	wlc_rcroam_t *prcroam;
	wlc_rcroam_info_v1_t *rcroam;
	uint rcroamlen = sizeof(*rcroam) + RCROAM_HDRLEN;

	WL_INFORM(("RCROAM mode %s\n", rcroam_enab ? "enable" : "disable"));

	prcroam = (wlc_rcroam_t *)MALLOCZ(dhdp->osh, rcroamlen);
	if (!prcroam) {
		WL_ERR(("Fail to malloc buffer\n"));
		return BCME_NOMEM;
	}

	/* Get RCROAM param */
	ret = wldev_iovar_getbuf(dev, "rcroam", NULL, 0, prcroam, rcroamlen, NULL);
	if (ret) {
		WL_ERR(("Failed to get RCROAM info(%d)\n", ret));
		goto done;
	}

	if (prcroam->ver != WLC_RC_ROAM_VER_1) {
		ret = BCME_VERSION;
		WL_ERR(("Ver(%d:%d). mismatch RCROAM info(%d)\n",
			prcroam->ver, WLC_RC_ROAM_VER_1, ret));
		goto done;
	}

	/* Set RCROAM param */
	rcroam = (wlc_rcroam_info_v1_t *)prcroam->data;
	prcroam->ver = WLC_RC_ROAM_VER_1;
	prcroam->len = sizeof(*rcroam);
	rcroam->enab = rcroam_enab;

	ret = wldev_iovar_setbuf(dev, "rcroam", prcroam, rcroamlen,
		ioctl_buf, sizeof(ioctl_buf), NULL);
	if (ret) {
		WL_ERR(("Failed to set RCROAM %s(%d)\n",
			rcroam_enab ? "Enable" : "Disable", ret));
		goto done;
	}
done:
	if (prcroam) {
		MFREE(dhdp->osh, prcroam, rcroamlen);
	}

	return ret;
}

#ifdef WBTEXT
static int wl_android_wbtext(struct net_device *dev, char *command, int total_len)
{
	int error = BCME_OK, argc = 0;
	int data, bytes_written;
	int roam_trigger[2];
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(dev);

	argc = sscanf(command+sizeof(CMD_WBTEXT_ENABLE), "%d", &data);
	if (!argc) {
		error = wldev_iovar_getint(dev, "wnm_bsstrans_resp", &data);
		if (error) {
			DHD_ERROR(("wl_android_wbtext: Failed to set wbtext error = %d\n",
				error));
			return error;
		}
		bytes_written = snprintf(command, total_len, "WBTEXT %s\n",
				(data == WL_BSSTRANS_POLICY_PRODUCT_WBTEXT)?
				"ENABLED" : "DISABLED");
		return bytes_written;
	} else {
		if (data) {
			data = WL_BSSTRANS_POLICY_PRODUCT_WBTEXT;
		}

		if ((error = wldev_iovar_setint(dev, "wnm_bsstrans_resp", data)) != BCME_OK) {
			DHD_ERROR(("wl_android_wbtext: Failed to set wbtext error = %d\n",
				error));
			return error;
		}

		if (data) {
			/* reset roam_prof when wbtext is on */
			if ((error = wl_cfg80211_wbtext_set_default(dev)) != BCME_OK) {
				return error;
			}
		} else {
			/* reset legacy roam trigger when wbtext is off */
			roam_trigger[0] = DEFAULT_ROAM_TRIGGER_VALUE;
			roam_trigger[1] = WLC_BAND_ALL;
			if ((error = wldev_ioctl_set(dev, WLC_SET_ROAM_TRIGGER, roam_trigger,
					sizeof(roam_trigger))) != BCME_OK) {
				DHD_ERROR(("wl_android_wbtext: Failed to reset roam trigger = %d\n",
					error));
				return error;
			}
		}
		dhdp->wbtext_policy = data;
	}
	return error;
}

#ifdef WES_SUPPORT
static int
wl_android_wbtext_enable(struct net_device *dev, int mode)
{
	int error = BCME_OK;
	char commandp[WLC_IOCTL_SMLEN];

	if (wl_android_check_wbtext_support(dev)) {
		bzero(commandp, sizeof(commandp));
		snprintf(commandp, WLC_IOCTL_SMLEN, "WBTEXT_ENABLE %d", mode);
		error = wl_android_wbtext(dev, commandp, WLC_IOCTL_SMLEN);
		if (error) {
			WL_ERR(("Failed to set WBTEXT = %d\n", error));
			return error;
		}
	}

	return error;
}
#endif /* WES_SUPPORT */

static int wl_cfg80211_wbtext_btm_timer_threshold(struct net_device *dev,
	char *command, int total_len)
{
	int error = BCME_OK, argc = 0;
	int data, bytes_written;

	argc = sscanf(command, CMD_WBTEXT_BTM_TIMER_THRESHOLD " %d\n", &data);
	if (!argc) {
		error = wldev_iovar_getint(dev, "wnm_bsstrans_timer_threshold", &data);
		if (error) {
			WL_ERR(("Failed to get wnm_bsstrans_timer_threshold (%d)\n", error));
			return error;
		}
		bytes_written = snprintf(command, total_len, "%d\n", data);
		return bytes_written;
	} else {
		if ((error = wldev_iovar_setint(dev, "wnm_bsstrans_timer_threshold",
				data)) != BCME_OK) {
			WL_ERR(("Failed to set wnm_bsstrans_timer_threshold (%d)\n", error));
			return error;
		}
	}
	return error;
}

static int wl_cfg80211_wbtext_btm_delta(struct net_device *dev,
	char *command, int total_len)
{
	int error = BCME_OK, argc = 0;
	int data = 0, bytes_written;

	argc = sscanf(command, CMD_WBTEXT_BTM_DELTA " %d\n", &data);
	if (!argc) {
		error = wldev_iovar_getint(dev, "wnm_btmdelta", &data);
		if (error) {
			WL_ERR(("Failed to get wnm_btmdelta (%d)\n", error));
			return error;
		}
		bytes_written = snprintf(command, total_len, "%d\n", data);
		return bytes_written;
	} else {
		if ((error = wldev_iovar_setint(dev, "wnm_btmdelta",
				data)) != BCME_OK) {
			WL_ERR(("Failed to set wnm_btmdelta (%d)\n", error));
			return error;
		}
	}
	return error;
}

static int wl_cfg80211_wbtext_estm_enable(struct net_device *dev,
	char *command, int total_len)
{
	int error = BCME_OK;
	int data = 0, bytes_written = 0;
	int wnmmask = 0;
	char *pcmd = command;

	bcmstrtok(&pcmd, " ", NULL);

	error = wldev_iovar_getint(dev, "wnm", &wnmmask);
	if (error) {
		WL_ERR(("Failed to get wnm_btmdelta (%d)\n", error));
		return error;
	}
	WL_DBG(("wnmmask %x\n", wnmmask));
	if (*pcmd == WL_IOCTL_ACTION_GET) {
		bytes_written = snprintf(command, total_len, "wbtext_estm_enable %d\n",
			(wnmmask & WL_WNM_ESTM) ? 1:0);
		return bytes_written;
	} else {
		data = bcm_atoi(pcmd);
		if (data == 0) {
			wnmmask &= ~WL_WNM_ESTM;
		} else {
			wnmmask |= WL_WNM_ESTM;
		}
		WL_DBG(("wnmmask %x\n", wnmmask));
		if ((error = wldev_iovar_setint(dev, "wnm", wnmmask)) != BCME_OK) {
			WL_ERR(("Failed to set wnm mask (%d)\n", error));
			return error;
		}
	}
	return error;
}
#endif /* WBTEXT */

#ifdef PNO_SUPPORT
#define PNO_PARAM_SIZE 50
#define VALUE_SIZE 50
#define LIMIT_STR_FMT  ("%50s %50s")
static int
wls_parse_batching_cmd(struct net_device *dev, char *command, int total_len)
{
	int err = BCME_OK;
	uint i, tokens, len_remain;
	char *pos, *pos2, *token, *token2, *delim;
	char param[PNO_PARAM_SIZE+1], value[VALUE_SIZE+1];
	struct dhd_pno_batch_params batch_params;

	DHD_PNO(("wls_parse_batching_cmd: command=%s, len=%d\n", command, total_len));
	len_remain = total_len;
	if (len_remain > (strlen(CMD_WLS_BATCHING) + 1)) {
		pos = command + strlen(CMD_WLS_BATCHING) + 1;
		len_remain -= strlen(CMD_WLS_BATCHING) + 1;
	} else {
		WL_ERR(("wls_parse_batching_cmd: No arguments, total_len %d\n", total_len));
		err = BCME_ERROR;
		goto exit;
	}
	bzero(&batch_params, sizeof(struct dhd_pno_batch_params));
	if (!strncmp(pos, PNO_BATCHING_SET, strlen(PNO_BATCHING_SET))) {
		if (len_remain > (strlen(PNO_BATCHING_SET) + 1)) {
			pos += strlen(PNO_BATCHING_SET) + 1;
		} else {
			WL_ERR(("wls_parse_batching_cmd: %s missing arguments, total_len %d\n",
				PNO_BATCHING_SET, total_len));
			err = BCME_ERROR;
			goto exit;
		}
		while ((token = strsep(&pos, PNO_PARAMS_DELIMETER)) != NULL) {
			bzero(param, sizeof(param));
			bzero(value, sizeof(value));
			if (token == NULL || !*token)
				break;
			if (*token == '\0')
				continue;
			delim = strchr(token, PNO_PARAM_VALUE_DELLIMETER);
			if (delim != NULL)
				*delim = ' ';

			tokens = sscanf(token, LIMIT_STR_FMT, param, value);
			if (!strncmp(param, PNO_PARAM_SCANFREQ, strlen(PNO_PARAM_SCANFREQ))) {
				batch_params.scan_fr = simple_strtol(value, NULL, 0);
				DHD_PNO(("scan_freq : %d\n", batch_params.scan_fr));
			} else if (!strncmp(param, PNO_PARAM_BESTN, strlen(PNO_PARAM_BESTN))) {
				batch_params.bestn = simple_strtol(value, NULL, 0);
				DHD_PNO(("bestn : %d\n", batch_params.bestn));
			} else if (!strncmp(param, PNO_PARAM_MSCAN, strlen(PNO_PARAM_MSCAN))) {
				batch_params.mscan = simple_strtol(value, NULL, 0);
				DHD_PNO(("mscan : %d\n", batch_params.mscan));
			} else if (!strncmp(param, PNO_PARAM_CHANNEL, strlen(PNO_PARAM_CHANNEL))) {
				i = 0;
				pos2 = value;
				tokens = sscanf(value, "<%s>", value);
				if (tokens != 1) {
					err = BCME_ERROR;
					DHD_ERROR(("wls_parse_batching_cmd: invalid format"
					" for channel"
					" <> params\n"));
					goto exit;
				}
				while ((token2 = strsep(&pos2,
						PNO_PARAM_CHANNEL_DELIMETER)) != NULL) {
					if (token2 == NULL || !*token2)
						break;
					if (*token2 == '\0')
						continue;
					if (*token2 == 'A' || *token2 == 'B') {
						batch_params.band = (*token2 == 'A')?
							WLC_BAND_5G : WLC_BAND_2G;
						DHD_PNO(("band : %s\n",
							(*token2 == 'A')? "A" : "B"));
					} else {
						if ((batch_params.nchan >= WL_NUMCHANNELS) ||
							(i >= WL_NUMCHANNELS)) {
							DHD_ERROR(("Too many nchan %d\n",
								batch_params.nchan));
							err = BCME_BUFTOOSHORT;
							goto exit;
						}
						batch_params.chan_list[i++] =
							simple_strtol(token2, NULL, 0);
						batch_params.nchan++;
						DHD_PNO(("channel :%d\n",
							batch_params.chan_list[i-1]));
					}
				 }
			} else if (!strncmp(param, PNO_PARAM_RTT, strlen(PNO_PARAM_RTT))) {
				batch_params.rtt = simple_strtol(value, NULL, 0);
				DHD_PNO(("rtt : %d\n", batch_params.rtt));
			} else {
				DHD_ERROR(("wls_parse_batching_cmd : unknown param: %s\n", param));
				err = BCME_ERROR;
				goto exit;
			}
		}
		err = dhd_dev_pno_set_for_batch(dev, &batch_params);
		if (err < 0) {
			DHD_ERROR(("failed to configure batch scan\n"));
		} else {
			bzero(command, total_len);
			err = snprintf(command, total_len, "%d", err);
		}
	} else if (!strncmp(pos, PNO_BATCHING_GET, strlen(PNO_BATCHING_GET))) {
		err = dhd_dev_pno_get_for_batch(dev, command, total_len);
		if (err < 0) {
			DHD_ERROR(("failed to getting batching results\n"));
		} else {
			err = strlen(command);
		}
	} else if (!strncmp(pos, PNO_BATCHING_STOP, strlen(PNO_BATCHING_STOP))) {
		err = dhd_dev_pno_stop_for_batch(dev);
		if (err < 0) {
			DHD_ERROR(("failed to stop batching scan\n"));
		} else {
			bzero(command, total_len);
			err = snprintf(command, total_len, "OK");
		}
	} else {
		DHD_ERROR(("wls_parse_batching_cmd : unknown command\n"));
		err = BCME_ERROR;
		goto exit;
	}
exit:
	return err;
}

#ifndef WL_SCHED_SCAN
static int wl_android_set_pno_setup(struct net_device *dev, char *command, int total_len)
{
	wlc_ssid_ext_t *ssids_local = NULL;
	int res = -1;
	int nssid = 0;
	cmd_tlv_t *cmd_tlv_temp;
	char *str_ptr;
	int tlv_size_left;
	int pno_time = 0;
	int pno_repeat = 0;
	int pno_freq_expo_max = 0;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);

#ifdef PNO_SET_DEBUG
	int i;
	char pno_in_example[] = {
		'P', 'N', 'O', 'S', 'E', 'T', 'U', 'P', ' ',
		'S', '1', '2', '0',
		'S',
		0x05,
		'd', 'l', 'i', 'n', 'k',
		'S',
		0x04,
		'G', 'O', 'O', 'G',
		'T',
		'0', 'B',
		'R',
		'2',
		'M',
		'2',
		0x00
		};
#endif /* PNO_SET_DEBUG */
	DHD_PNO(("wl_android_set_pno_setup: command=%s, len=%d\n", command, total_len));

	if (total_len < (strlen(CMD_PNOSETUP_SET) + sizeof(cmd_tlv_t))) {
		DHD_ERROR(("wl_android_set_pno_setup: argument=%d less min size\n", total_len));
		goto exit_proc;
	}
#ifdef PNO_SET_DEBUG
	memcpy(command, pno_in_example, sizeof(pno_in_example));
	total_len = sizeof(pno_in_example);
#endif
	str_ptr = command + strlen(CMD_PNOSETUP_SET);
	tlv_size_left = total_len - strlen(CMD_PNOSETUP_SET);

	cmd_tlv_temp = (cmd_tlv_t *)str_ptr;
	ssids_local = (wlc_ssid_ext_t *)MALLOCZ(cfg->osh,
		sizeof(wlc_ssid_ext_t) * MAX_PFN_LIST_COUNT);
	if (!ssids_local) {
		DHD_ERROR(("wl_android_set_pno_setup: No memory\n"));
		return -ENOMEM;
	}

	if ((cmd_tlv_temp->prefix == PNO_TLV_PREFIX) &&
		(cmd_tlv_temp->version == PNO_TLV_VERSION) &&
		(cmd_tlv_temp->subtype == PNO_TLV_SUBTYPE_LEGACY_PNO)) {

		str_ptr += sizeof(cmd_tlv_t);
		tlv_size_left -= sizeof(cmd_tlv_t);

		if ((nssid = wl_parse_ssid_list_tlv(&str_ptr, ssids_local,
			MAX_PFN_LIST_COUNT, &tlv_size_left)) <= 0) {
			DHD_ERROR(("SSID is not presented or corrupted ret=%d\n", nssid));
			goto exit_proc;
		} else {
			if ((str_ptr[0] != PNO_TLV_TYPE_TIME) || (tlv_size_left <= 1)) {
				DHD_ERROR(("wl_android_set_pno_setup: scan duration corrupted"
					" field size %d\n",
					tlv_size_left));
				goto exit_proc;
			}
			str_ptr++;
			pno_time = simple_strtoul(str_ptr, &str_ptr, 16);
			DHD_PNO(("wl_android_set_pno_setup: pno_time=%d\n", pno_time));

			if (str_ptr[0] != 0) {
				if ((str_ptr[0] != PNO_TLV_FREQ_REPEAT)) {
					DHD_ERROR(("wl_android_set_pno_setup: pno repeat:"
						" corrupted field\n"));
					goto exit_proc;
				}
				str_ptr++;
				pno_repeat = simple_strtoul(str_ptr, &str_ptr, 16);
				DHD_PNO(("wl_android_set_pno_setup: got pno_repeat=%d\n",
					pno_repeat));
				if (str_ptr[0] != PNO_TLV_FREQ_EXPO_MAX) {
					DHD_ERROR(("wl_android_set_pno_setup: FREQ_EXPO_MAX"
						" corrupted field size\n"));
					goto exit_proc;
				}
				str_ptr++;
				pno_freq_expo_max = simple_strtoul(str_ptr, &str_ptr, 16);
				DHD_PNO(("wl_android_set_pno_setup: pno_freq_expo_max=%d\n",
					pno_freq_expo_max));
			}
		}
	} else {
		DHD_ERROR(("wl_android_set_pno_setup: get wrong TLV command\n"));
		goto exit_proc;
	}

	res = dhd_dev_pno_set_for_ssid(dev, ssids_local, nssid, pno_time, pno_repeat,
		pno_freq_expo_max, NULL, 0);
exit_proc:
	if (ssids_local) {
		MFREE(cfg->osh, ssids_local,
			sizeof(wlc_ssid_ext_t) * MAX_PFN_LIST_COUNT);
	}
	return res;
}
#endif /* !WL_SCHED_SCAN */
#endif /* PNO_SUPPORT  */

static int wl_android_get_p2p_dev_addr(struct net_device *ndev, char *command, int total_len)
{
	int ret;
	struct ether_addr p2pdev_addr;

#define MAC_ADDR_STR_LEN 18
	if (total_len < MAC_ADDR_STR_LEN) {
		DHD_ERROR(("wl_android_get_p2p_dev_addr: buflen %d is less than p2p dev addr\n",
			total_len));
		return -1;
	}

	ret = wl_cfg80211_get_p2p_dev_addr(ndev, &p2pdev_addr);
	if (ret) {
		DHD_ERROR(("wl_android_get_p2p_dev_addr: Failed to get p2p dev addr\n"));
		return -1;
	}
	return (snprintf(command, total_len, MACF, ETHERP_TO_MACF(&p2pdev_addr)));
}

int
wl_android_set_ap_mac_list(struct net_device *dev, int macmode, struct maclist *maclist)
{
	int i, j, match;
	int ret	= 0;
	char mac_buf[MAX_NUM_OF_ASSOCLIST *
		sizeof(struct ether_addr) + sizeof(uint)] = {0};
	struct maclist *assoc_maclist = (struct maclist *)mac_buf;

	/* set filtering mode */
	if ((ret = wldev_ioctl_set(dev, WLC_SET_MACMODE, &macmode, sizeof(macmode)) != 0)) {
		DHD_ERROR(("wl_android_set_ap_mac_list : WLC_SET_MACMODE error=%d\n", ret));
		return ret;
	}
	if (macmode != MACLIST_MODE_DISABLED) {
		/* set the MAC filter list */
		if ((ret = wldev_ioctl_set(dev, WLC_SET_MACLIST, maclist,
			sizeof(int) + sizeof(struct ether_addr) * maclist->count)) != 0) {
			DHD_ERROR(("wl_android_set_ap_mac_list : WLC_SET_MACLIST error=%d\n", ret));
			return ret;
		}
		/* get the current list of associated STAs */
		assoc_maclist->count = MAX_NUM_OF_ASSOCLIST;
		if ((ret = wldev_ioctl_get(dev, WLC_GET_ASSOCLIST, assoc_maclist,
			sizeof(mac_buf))) != 0) {
			DHD_ERROR(("wl_android_set_ap_mac_list: WLC_GET_ASSOCLIST error=%d\n",
				ret));
			return ret;
		}
		/* do we have any STA associated?  */
		if (assoc_maclist->count) {
			/* iterate each associated STA */
			for (i = 0; i < assoc_maclist->count; i++) {
				match = 0;
				/* compare with each entry */
				for (j = 0; j < maclist->count; j++) {
					DHD_INFO(("wl_android_set_ap_mac_list: associated="MACDBG
					"list = "MACDBG "\n",
					MAC2STRDBG(assoc_maclist->ea[i].octet),
					MAC2STRDBG(maclist->ea[j].octet)));
					if (memcmp(assoc_maclist->ea[i].octet,
						maclist->ea[j].octet, ETHER_ADDR_LEN) == 0) {
						match = 1;
						break;
					}
				}
				/* do conditional deauth */
				/*   "if not in the allow list" or "if in the deny list" */
				if ((macmode == MACLIST_MODE_ALLOW && !match) ||
					(macmode == MACLIST_MODE_DENY && match)) {
					scb_val_t scbval;

					scbval.val = htod32(1);
					memcpy(&scbval.ea, &assoc_maclist->ea[i],
						ETHER_ADDR_LEN);
					if ((ret = wldev_ioctl_set(dev,
						WLC_SCB_DEAUTHENTICATE_FOR_REASON,
						&scbval, sizeof(scb_val_t))) != 0)
						DHD_ERROR(("wl_android_set_ap_mac_list:"
							" WLC_SCB_DEAUTHENTICATE"
							" error=%d\n",
							ret));
				}
			}
		}
	}
	return ret;
}

/*
 * HAPD_MAC_FILTER mac_mode mac_cnt mac_addr1 mac_addr2
 *
 */
static int
wl_android_set_mac_address_filter(struct net_device *dev, char* str)
{
	int i;
	int ret = 0;
	int macnum = 0;
	int macmode = MACLIST_MODE_DISABLED;
	struct maclist *list;
	char eabuf[ETHER_ADDR_STR_LEN];
	const char *token;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);

	/* string should look like below (macmode/macnum/maclist) */
	/*   1 2 00:11:22:33:44:55 00:11:22:33:44:ff  */

	/* get the MAC filter mode */
	token = strsep((char**)&str, " ");
	if (!token) {
		return -1;
	}
	macmode = bcm_atoi(token);

	if (macmode < MACLIST_MODE_DISABLED || macmode > MACLIST_MODE_ALLOW) {
		DHD_ERROR(("wl_android_set_mac_address_filter: invalid macmode %d\n", macmode));
		return -1;
	}

	token = strsep((char**)&str, " ");
	if (!token) {
		return -1;
	}
	macnum = bcm_atoi(token);
	if (macnum < 0 || macnum > MAX_NUM_MAC_FILT) {
		DHD_ERROR(("wl_android_set_mac_address_filter: invalid number of MAC"
			" address entries %d\n",
			macnum));
		return -1;
	}
	/* allocate memory for the MAC list */
	list = (struct maclist*) MALLOCZ(cfg->osh, sizeof(int) +
		sizeof(struct ether_addr) * macnum);
	if (!list) {
		DHD_ERROR(("wl_android_set_mac_address_filter : failed to allocate memory\n"));
		return -1;
	}
	/* prepare the MAC list */
	list->count = htod32(macnum);
	bzero((char *)eabuf, ETHER_ADDR_STR_LEN);
	for (i = 0; i < list->count; i++) {
		token = strsep((char**)&str, " ");
		if (token == NULL) {
			DHD_ERROR(("wl_android_set_mac_address_filter : No mac address present\n"));
			ret = -EINVAL;
			goto exit;
		}
		strlcpy(eabuf, token, sizeof(eabuf));
		if (!(ret = bcm_ether_atoe(eabuf, &list->ea[i]))) {
			DHD_ERROR(("wl_android_set_mac_address_filter : mac parsing err index=%d,"
				" addr=%s\n",
				i, eabuf));
			list->count = i;
			break;
		}
		DHD_INFO(("wl_android_set_mac_address_filter : %d/%d MACADDR=%s",
			i, list->count, eabuf));
	}
	if (i == 0)
		goto exit;

	/* set the list */
	if ((ret = wl_android_set_ap_mac_list(dev, macmode, list)) != 0)
		DHD_ERROR(("wl_android_set_mac_address_filter: Setting MAC list failed error=%d\n",
			ret));

exit:
	MFREE(cfg->osh, list, sizeof(int) + sizeof(struct ether_addr) * macnum);

	return ret;
}

static int wl_android_get_factory_mac_addr(struct net_device *ndev, char *command, int total_len)
{
	int ret;
	struct bcm_cfg80211 *cfg = wl_get_cfg(ndev);

	if (total_len < ETHER_ADDR_STR_LEN) {
		DHD_ERROR(("wl_android_get_factory_mac_addr buflen %d"
			"is less than factory mac addr\n", total_len));
		return BCME_ERROR;
	}
	ret = snprintf(command, total_len, MACDBG,
		MAC2STRDBG(bcmcfg_to_prmry_ndev(cfg)->perm_addr));
	return ret;
}

#if defined(WLAN_ACCEL_BOOT)
bool g_wifi_accel_on = FALSE;
int wl_android_wifi_accel_on(struct net_device *dev, bool force_reg_on)
{
	int ret = 0;

	DHD_ERROR(("%s: force_reg_on:%d g_wifi_accel_on:%d\n",
		__FUNCTION__, force_reg_on, g_wifi_accel_on));
	if (!dev) {
		DHD_ERROR(("%s: dev is null\n", __FUNCTION__));
		return -EINVAL;
	}

	if (force_reg_on) {
		if (g_wifi_accel_on) {
			/* First resume the bus if it is in suspended state */
			ret = dhd_net_bus_resume(dev, 0);
			if (ret) {
				DHD_ERROR(("%s: dhd_net_bus_resume failed\n", __FUNCTION__));
			}
			/* Toggle wl_reg_on */
			ret = wl_android_wifi_off(dev, TRUE);
			if (ret) {
				DHD_ERROR(("%s: wl_android_wifi_off failed\n", __FUNCTION__));
			}
		}
		ret = wl_android_wifi_on(dev);
		if (ret) {
			DHD_ERROR(("%s: wl_android_wifi_on failed\n", __FUNCTION__));
		}
		g_wifi_accel_on = TRUE;
	} else {
		ret = dhd_net_bus_resume(dev, 0);
	}

	return ret;
}

int wl_android_wifi_accel_off(struct net_device *dev, bool force_reg_on)
{
	int ret = 0;

	DHD_ERROR(("%s: force_reg_on:%d g_wifi_accel_on:%d\n",
		__FUNCTION__, force_reg_on, g_wifi_accel_on));
	if (!dev) {
		DHD_ERROR(("%s: dev is null\n", __FUNCTION__));
		return -EINVAL;
	}

	if (force_reg_on) {
		ret = wl_android_wifi_off(dev, TRUE);
		g_wifi_accel_on = FALSE;
	} else {
		ret = dhd_net_bus_suspend(dev);
	}

	return ret;
}
#endif /* WLAN_ACCEL_BOOT */

#ifdef WBRC
extern int wbrc_wl2bt_reset(void);
extern int wbrc_wlan_on_ack(void);
extern int wbrc_wlan_on_started(void);
#endif /* WBRC */

/**
 * Global function definitions (declared in wl_android.h)
 */

int wl_android_wifi_on(struct net_device *dev)
{
	int ret = 0;
	int retry = POWERUP_MAX_RETRY;
	dhd_pub_t *dhdp;

	BCM_REFERENCE(dhdp);
	if (!dev) {
		DHD_ERROR(("wl_android_wifi_on: dev is null\n"));
		return -EINVAL;
	}

	dhdp = wl_cfg80211_get_dhdp(dev);
	dhd_net_if_lock(dev);
	WL_MSG(dev->name, "in g_wifi_on=%d\n", g_wifi_on);
	if (!g_wifi_on) {
		do {
#ifdef WBRC
			/*
			 * Inform wbrc that wlan is started to reset wlan_on_ack and
			 * wlan_on_ack will be set back after successful WLAN ON
			 * by calling wbrc_wlan_on_ack().
			 * This is to ensure BT dev_open waits till WLAN ON is finished.
			 */
			(void)wbrc_wlan_on_started();

			if (dhdp->do_chip_bighammer) {
				dhdp->do_chip_bighammer = FALSE;
				/* Inform BT to reset which wait till BT OFF is done */
				if (wbrc_wl2bt_reset()) {
					DHD_ERROR(("%s:no BT\n", __FUNCTION__));
				} else {
					dhdp->chip_bighammer_count++;
				}
			}
#endif /* WBRC */
			dhd_net_wifi_platform_set_power(dev, TRUE, WIFI_TURNON_DELAY);
#ifdef BCMSDIO
			ret = dhd_net_bus_resume(dev, 0);
			if (ret)
				goto retry_power;
#endif /* BCMSDIO */
			ret = dhd_net_bus_devreset(dev, FALSE);
#ifdef WBRC
			if (dhdp->dhd_induce_bh_error == DHD_INDUCE_BH_ON_FAIL_ONCE) {
				DHD_ERROR(("%s: dhd_induce_bh_error = %d\n",
					__FUNCTION__, dhdp->dhd_induce_bh_error));
				/* Forcefully set error */
				ret = BCME_ERROR;
				/* Clear the induced bh error */
				dhdp->dhd_induce_bh_error = DHD_INDUCE_ERROR_CLEAR;
			}
			if (dhdp->dhd_induce_bh_error == DHD_INDUCE_BH_ON_FAIL_ALWAYS) {
				DHD_ERROR(("%s: dhd_induce_bh_error = %d\n",
					__FUNCTION__, dhdp->dhd_induce_bh_error));
				/* Forcefully set error */
				ret = BCME_ERROR;
			}
#endif /* WBRC */
			if (ret)
				goto retry_power;
#if defined(BCMSDIO) || defined(BCMDBUS)
#ifdef BCMSDIO
			dhd_net_bus_resume(dev, 1);
#endif /* BCMSDIO */
			ret = dhd_dev_init_ioctl(dev);
			if (ret < 0) {
				goto retry_power;
			}
#endif /* BCMSDIO || BCMDBUS */
			if (ret == 0) {
#ifdef WBRC
				/* Ack wbrc driver on wlan ON succeed */
				(void)wbrc_wlan_on_ack();
#endif /* WBRC */
				break;
			}
retry_power:
			DHD_ERROR(("%s: failed to power up wifi chip, retry again (%d left) **\n",
				__FUNCTION__, retry));
			/* Set big hammer flag */
			dhdp->do_chip_bighammer = TRUE;
			dhd_net_bus_devreset(dev, TRUE);
#ifdef BCMSDIO
			dhd_net_bus_suspend(dev);
#endif /* BCMSDIO */
			dhd_net_wifi_platform_set_power(dev, FALSE, WIFI_TURNOFF_DELAY);

		} while (retry-- > 0);
		if (ret != 0) {
			DHD_ERROR(("\nfailed to power up wifi chip, max retry reached **\n\n"));
#ifdef BCM_DETECT_TURN_ON_FAILURE
			BUG_ON(1);
#endif /* BCM_DETECT_TURN_ON_FAILURE */
			goto exit;
		}
		g_wifi_on = TRUE;
	}

exit:
	if (ret)
		WL_MSG(dev->name, "Failed %d\n", ret);
	else
		WL_MSG(dev->name, "Success\n");
	dhd_net_if_unlock(dev);

	return ret;
}

int wl_android_wifi_off(struct net_device *dev, bool force_off)
{
	int ret = 0;

	if (!dev) {
		DHD_TRACE(("wl_android_wifi_off: dev is null\n"));
		return -EINVAL;
	}

#if defined(BCMPCIE) && defined(DHD_DEBUG_UART)
	ret = dhd_debug_uart_is_running(dev);
	if (ret) {
		DHD_ERROR(("wl_android_wifi_off: - Debug UART App is running\n"));
		return -EBUSY;
	}
#endif	/* BCMPCIE && DHD_DEBUG_UART */
	dhd_net_if_lock(dev);
	WL_MSG(dev->name, " g_wifi_on=%d force_off=%d\n", g_wifi_on, force_off);
	if (g_wifi_on || force_off) {
#if defined(BCMSDIO) || defined(BCMPCIE) || defined(BCMDBUS)
		ret = dhd_net_bus_devreset(dev, TRUE);
#ifdef BCMSDIO
		dhd_net_bus_suspend(dev);
#endif /* BCMSDIO */
#endif /* BCMSDIO || BCMPCIE || BCMDBUS */
		dhd_net_wifi_platform_set_power(dev, FALSE, WIFI_TURNOFF_DELAY);
		g_wifi_on = FALSE;
	}
	WL_MSG(dev->name, "out\n");
	dhd_net_if_unlock(dev);

	return ret;
}

static int wl_android_set_fwpath(struct net_device *net, char *command, int total_len)
{
	if ((strlen(command) - strlen(CMD_SETFWPATH)) > MOD_PARAM_PATHLEN)
		return -1;
	return dhd_net_set_fw_path(net, command + strlen(CMD_SETFWPATH) + 1);
}

#ifdef CONNECTION_STATISTICS
static int
wl_chanim_stats(struct net_device *dev, u8 *chan_idle)
{
	int err;
	wl_chanim_stats_t *list;
	/* Parameter _and_ returned buffer of chanim_stats. */
	wl_chanim_stats_t param;
	u8 result[WLC_IOCTL_SMLEN];
	chanim_stats_t *stats;

	bzero(&param, sizeof(param));

	param.buflen = htod32(sizeof(wl_chanim_stats_t));
	param.count = htod32(WL_CHANIM_COUNT_ONE);

	if ((err = wldev_iovar_getbuf(dev, "chanim_stats", (char*)&param, sizeof(wl_chanim_stats_t),
		(char*)result, sizeof(result), 0)) < 0) {
		WL_ERR(("Failed to get chanim results %d \n", err));
		return err;
	}

	list = (wl_chanim_stats_t*)result;

	list->buflen = dtoh32(list->buflen);
	list->version = dtoh32(list->version);
	list->count = dtoh32(list->count);

	if (list->buflen == 0) {
		list->version = 0;
		list->count = 0;
	} else if (list->version != WL_CHANIM_STATS_VERSION) {
		WL_ERR(("Sorry, firmware has wl_chanim_stats version %d "
			"but driver supports only version %d.\n",
				list->version, WL_CHANIM_STATS_VERSION));
		list->buflen = 0;
		list->count = 0;
	}

	stats = list->stats;
	stats->glitchcnt = dtoh32(stats->glitchcnt);
	stats->badplcp = dtoh32(stats->badplcp);
	stats->chanspec = dtoh16(stats->chanspec);
	stats->timestamp = dtoh32(stats->timestamp);
	stats->chan_idle = dtoh32(stats->chan_idle);

	WL_INFORM(("chanspec: 0x%4x glitch: %d badplcp: %d idle: %d timestamp: %d\n",
		stats->chanspec, stats->glitchcnt, stats->badplcp, stats->chan_idle,
		stats->timestamp));

	*chan_idle = stats->chan_idle;

	return (err);
}

static int
wl_android_get_connection_stats(struct net_device *dev, char *command, int total_len)
{
	static char iovar_buf[WLC_IOCTL_MAXLEN];
	const wl_cnt_wlc_t* wlc_cnt = NULL;
#ifndef DISABLE_IF_COUNTERS
	wl_if_stats_t* if_stats = NULL;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
#ifdef BCMDONGLEHOST
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(dev);
#endif /* BCMDONGLEHOST */
#endif /* DISABLE_IF_COUNTERS */

	int link_speed = 0;
	struct connection_stats *output;
	unsigned int bufsize = 0;
	int bytes_written = -1;
	int ret = 0;

	WL_INFORM(("wl_android_get_connection_stats: enter Get Connection Stats\n"));

	if (total_len <= 0) {
		WL_ERR(("wl_android_get_connection_stats: invalid buffer size %d\n", total_len));
		goto error;
	}

	bufsize = total_len;
	if (bufsize < sizeof(struct connection_stats)) {
		WL_ERR(("wl_android_get_connection_stats: not enough buffer size, provided=%u,"
			" requires=%zu\n",
			bufsize,
			sizeof(struct connection_stats)));
		goto error;
	}

	output = (struct connection_stats *)command;

#ifndef DISABLE_IF_COUNTERS
	if_stats = (wl_if_stats_t *)MALLOCZ(cfg->osh, sizeof(*if_stats));
	if (if_stats == NULL) {
		WL_ERR(("wl_android_get_connection_stats: MALLOCZ failed\n"));
		goto error;
	}
	bzero(if_stats, sizeof(*if_stats));

#ifdef BCMDONGLEHOST
	if (FW_SUPPORTED(dhdp, ifst)) {
		ret = wl_cfg80211_ifstats_counters(dev, if_stats);
	} else
#endif /* BCMDONGLEHOST */
	{
		ret = wldev_iovar_getbuf(dev, "if_counters", NULL, 0,
			(char *)if_stats, sizeof(*if_stats), NULL);
	}

	ret = wldev_iovar_getbuf(dev, "if_counters", NULL, 0,
		(char *)if_stats, sizeof(*if_stats), NULL);
	if (ret) {
		WL_ERR(("wl_android_get_connection_stats: if_counters not supported ret=%d\n",
			ret));

		/* In case if_stats IOVAR is not supported, get information from counters. */
#endif /* DISABLE_IF_COUNTERS */
		ret = wldev_iovar_getbuf(dev, "counters", NULL, 0,
			iovar_buf, WLC_IOCTL_MAXLEN, NULL);
		if (unlikely(ret)) {
			WL_ERR(("counters error (%d) - size = %zu\n", ret, sizeof(wl_cnt_wlc_t)));
			goto error;
		}
		ret = wl_cntbuf_to_xtlv_format(NULL, iovar_buf, WL_CNTBUF_MAX_SIZE, 0);
		if (ret != BCME_OK) {
			WL_ERR(("wl_android_get_connection_stats:"
			" wl_cntbuf_to_xtlv_format ERR %d\n",
			ret));
			goto error;
		}

		if (!(wlc_cnt = GET_WLCCNT_FROM_CNTBUF(iovar_buf))) {
			WL_ERR(("wl_android_get_connection_stats: wlc_cnt NULL!\n"));
			goto error;
		}

		output->txframe   = dtoh32(wlc_cnt->txframe);
		output->txbyte    = dtoh32(wlc_cnt->txbyte);
		output->txerror   = dtoh32(wlc_cnt->txerror);
		output->rxframe   = dtoh32(wlc_cnt->rxframe);
		output->rxbyte    = dtoh32(wlc_cnt->rxbyte);
		output->txfail    = dtoh32(wlc_cnt->txfail);
		output->txretry   = dtoh32(wlc_cnt->txretry);
		output->txretrie  = dtoh32(wlc_cnt->txretrie);
		output->txrts     = dtoh32(wlc_cnt->txrts);
		output->txnocts   = dtoh32(wlc_cnt->txnocts);
		output->txexptime = dtoh32(wlc_cnt->txexptime);
#ifndef DISABLE_IF_COUNTERS
	} else {
		/* Populate from if_stats. */
		if (dtoh16(if_stats->version) > WL_IF_STATS_T_VERSION) {
			WL_ERR(("wl_android_get_connection_stats: incorrect version of"
				" wl_if_stats_t,"
				" expected=%u got=%u\n",
				WL_IF_STATS_T_VERSION, if_stats->version));
			goto error;
		}

		output->txframe   = (uint32)dtoh64(if_stats->txframe);
		output->txbyte    = (uint32)dtoh64(if_stats->txbyte);
		output->txerror   = (uint32)dtoh64(if_stats->txerror);
		output->rxframe   = (uint32)dtoh64(if_stats->rxframe);
		output->rxbyte    = (uint32)dtoh64(if_stats->rxbyte);
		output->txfail    = (uint32)dtoh64(if_stats->txfail);
		output->txretry   = (uint32)dtoh64(if_stats->txretry);
		output->txretrie  = (uint32)dtoh64(if_stats->txretrie);
		if (dtoh16(if_stats->length) > OFFSETOF(wl_if_stats_t, txexptime)) {
			output->txexptime = (uint32)dtoh64(if_stats->txexptime);
			output->txrts     = (uint32)dtoh64(if_stats->txrts);
			output->txnocts   = (uint32)dtoh64(if_stats->txnocts);
		} else {
			output->txexptime = 0;
			output->txrts     = 0;
			output->txnocts   = 0;
		}
	}
#endif /* DISABLE_IF_COUNTERS */

	/* link_speed is in kbps */
	ret = wldev_get_link_speed(dev, &link_speed);
	if (ret || link_speed < 0) {
		WL_ERR(("wl_android_get_connection_stats: wldev_get_link_speed()"
			" failed, ret=%d, speed=%d\n",
			ret, link_speed));
		goto error;
	}

	output->txrate    = link_speed;

	/* Channel idle ratio. */
	if (wl_chanim_stats(dev, &(output->chan_idle)) < 0) {
		output->chan_idle = 0;
	};

	bytes_written = sizeof(struct connection_stats);

error:
#ifndef DISABLE_IF_COUNTERS
	if (if_stats) {
		MFREE(cfg->osh, if_stats, sizeof(*if_stats));
	}
#endif /* DISABLE_IF_COUNTERS */

	return bytes_written;
}
#endif /* CONNECTION_STATISTICS */

#ifdef WL_MBO
static int
wl_android_process_mbo_cmd(struct net_device *dev, char *command, int total_len)
{
	int ret = BCME_ERROR;
	char *pcmd = command;
	char *str = NULL;
	wl_drv_cmd_info_t cmd_info;
	const wl_drv_sub_cmd_t *mbo_cmd = &mbo_cmd_list[0];

	/* skip to cmd name after "mbo" */
	str = bcmstrtok(&pcmd, " ", NULL);

	/* If mbo subcmd name is not provided, return error */
	if (*pcmd == '\0') {
		WL_ERR(("mbo subcmd not provided %s\n", __FUNCTION__));
		ret = -EINVAL;
		return ret;
	}

	/* get the mbo command name to str */
	str = bcmstrtok(&pcmd, " ", NULL);

	cmd_info.bytes_written = 0;
	while (mbo_cmd->name != NULL) {
		if (strnicmp(mbo_cmd->name, str, strlen(mbo_cmd->name)) == 0) {
			/* dispatch cmd to appropriate handler */
			if (mbo_cmd->handler) {
				cmd_info.command = command;
				cmd_info.tot_len = total_len;
				ret = mbo_cmd->handler(dev, mbo_cmd, pcmd, &cmd_info);
			}
			return ret;
		}
		mbo_cmd++;
	}
	return ret;
}

static int
wl_android_send_wnm_notif(struct net_device *dev, bcm_iov_buf_t *iov_buf,
	uint16 iov_buf_len, uint8 *iov_resp, uint16 iov_resp_len, uint8 sub_elem_type)
{
	int ret = BCME_OK;
	uint8 *pxtlv = NULL;
	uint16 iovlen = 0;
	uint16 buflen = 0, buflen_start = 0;

	memset_s(iov_buf, iov_buf_len, 0, iov_buf_len);
	iov_buf->version = WL_MBO_IOV_VERSION_1_1;
	iov_buf->id = WL_MBO_CMD_SEND_NOTIF;
	buflen = buflen_start = iov_buf_len - sizeof(bcm_iov_buf_t);
	pxtlv = (uint8 *)&iov_buf->data[0];
	ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_SUB_ELEM_TYPE,
		sizeof(sub_elem_type), &sub_elem_type, BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		return ret;
	}
	iov_buf->len = buflen_start - buflen;
	iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;
	ret = wldev_iovar_setbuf(dev, "mbo",
			iov_buf, iovlen, iov_resp, WLC_IOCTL_MAXLEN, NULL);
	if (ret != BCME_OK) {
		WL_ERR(("Fail to sent wnm notif %d\n", ret));
	}
	return ret;
}

static int
wl_android_mbo_resp_parse_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	wl_drv_cmd_info_t *cmd_info = (wl_drv_cmd_info_t *)ctx;
	uint8 *command = cmd_info->command;
	uint16 total_len = cmd_info->tot_len;
	uint16 bytes_written = 0;

	UNUSED_PARAMETER(len);
	/* TODO: validate data value */
	if (data == NULL) {
		WL_ERR(("%s: Bad argument !!\n", __FUNCTION__));
		return -EINVAL;
	}
	switch (type) {
		case WL_MBO_XTLV_CELL_DATA_CAP:
		{
			bytes_written = snprintf(command, total_len, "cell_data_cap: %u\n", *data);
			cmd_info->bytes_written = bytes_written;
		}
		break;
		default:
			WL_ERR(("%s: Unknown tlv %u\n", __FUNCTION__, type));
	}
	return BCME_OK;
}

static int
wl_android_mbo_subcmd_cell_data_cap(struct net_device *dev, const wl_drv_sub_cmd_t *cmd,
		char *command, wl_drv_cmd_info_t *cmd_info)
{
	int ret = BCME_OK;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	uint16 iovlen = 0;
	char *pcmd = command;
	bcm_iov_buf_t *iov_buf = NULL;
	bcm_iov_buf_t *p_resp = NULL;
	uint8 *iov_resp = NULL;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
	uint16 version;

	/* first get the configured value */
	iov_buf = (bcm_iov_buf_t *)MALLOCZ(cfg->osh, WLC_IOCTL_MEDLEN);
	if (iov_buf == NULL) {
		ret = -ENOMEM;
		WL_ERR(("iov buf memory alloc exited\n"));
		goto exit;
	}
	iov_resp = (uint8 *)MALLOCZ(cfg->osh, WLC_IOCTL_MAXLEN);
	if (iov_resp == NULL) {
		ret = -ENOMEM;
		WL_ERR(("iov resp memory alloc exited\n"));
		goto exit;
	}

	/* fill header */
	iov_buf->version = WL_MBO_IOV_VERSION_1_1;
	iov_buf->id = WL_MBO_CMD_CELLULAR_DATA_CAP;

	ret = wldev_iovar_getbuf(dev, "mbo", iov_buf, WLC_IOCTL_MEDLEN, iov_resp,
		WLC_IOCTL_MAXLEN,
		NULL);
	if (ret != BCME_OK) {
		goto exit;
	}
	p_resp = (bcm_iov_buf_t *)iov_resp;

	/* get */
	if (*pcmd == WL_IOCTL_ACTION_GET) {
		/* Check for version */
		version = dtoh16(*(uint16 *)iov_resp);
		if (version != WL_MBO_IOV_VERSION_1_1) {
			ret = -EINVAL;
		}
		if (p_resp->id == WL_MBO_CMD_CELLULAR_DATA_CAP) {
			ret = bcm_unpack_xtlv_buf((void *)cmd_info, (uint8 *)p_resp->data,
				p_resp->len, BCM_XTLV_OPTION_ALIGN32,
				wl_android_mbo_resp_parse_cbfn);
			if (ret == BCME_OK) {
				ret = cmd_info->bytes_written;
			}
		} else {
			ret = -EINVAL;
			WL_ERR(("Mismatch: resp id %d req id %d\n", p_resp->id, cmd->id));
			goto exit;
		}
	} else {
		uint8 cell_cap = bcm_atoi(pcmd);
		const uint8* old_cell_cap = NULL;
		uint16 len = 0;

		old_cell_cap = bcm_get_data_from_xtlv_buf((uint8 *)p_resp->data, p_resp->len,
			WL_MBO_XTLV_CELL_DATA_CAP, &len, BCM_XTLV_OPTION_ALIGN32);
		if (old_cell_cap && *old_cell_cap == cell_cap) {
			WL_ERR(("No change is cellular data capability\n"));
			/* No change in value */
			goto exit;
		}

		buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);

		if (cell_cap < MBO_CELL_DATA_CONN_AVAILABLE ||
			cell_cap > MBO_CELL_DATA_CONN_NOT_CAPABLE) {
			WL_ERR(("wrong value %u\n", cell_cap));
			ret = -EINVAL;
			goto exit;
		}
		pxtlv = (uint8 *)&iov_buf->data[0];
		ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_CELL_DATA_CAP,
			sizeof(cell_cap), &cell_cap, BCM_XTLV_OPTION_ALIGN32);
		if (ret != BCME_OK) {
			goto exit;
		}
		iov_buf->len = buflen_start - buflen;
		iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;
		ret = wldev_iovar_setbuf(dev, "mbo",
				iov_buf, iovlen, iov_resp, WLC_IOCTL_MAXLEN, NULL);
		if (ret != BCME_OK) {
			WL_ERR(("Fail to set iovar %d\n", ret));
			ret = -EINVAL;
			goto exit;
		}
		/* Skip for CUSTOMER_HW4 - WNM notification
		 * for cellular data capability is handled by host
		 */
		/* send a WNM notification request to associated AP */
		if (wl_get_drv_status(cfg, CONNECTED, dev)) {
			WL_INFORM(("Sending WNM Notif\n"));
			ret = wl_android_send_wnm_notif(dev, iov_buf, WLC_IOCTL_MEDLEN,
				iov_resp, WLC_IOCTL_MAXLEN, MBO_ATTR_CELL_DATA_CAP);
			if (ret != BCME_OK) {
				WL_ERR(("Fail to send WNM notification %d\n", ret));
				ret = -EINVAL;
			}
		}
	}
exit:
	if (iov_buf) {
		MFREE(cfg->osh, iov_buf, WLC_IOCTL_MEDLEN);
	}
	if (iov_resp) {
		MFREE(cfg->osh, iov_resp, WLC_IOCTL_MAXLEN);
	}
	return ret;
}

static int
wl_android_mbo_non_pref_chan_parse_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	wl_drv_cmd_info_t *cmd_info = (wl_drv_cmd_info_t *)ctx;
	uint8 *command = cmd_info->command + cmd_info->bytes_written;
	uint16 total_len = cmd_info->tot_len;
	uint16 bytes_written = 0;

	WL_DBG(("Total bytes written at begining %u\n", cmd_info->bytes_written));
	UNUSED_PARAMETER(len);
	if (data == NULL) {
		WL_ERR(("%s: Bad argument !!\n", __FUNCTION__));
		return -EINVAL;
	}
	switch (type) {
		case WL_MBO_XTLV_OPCLASS:
		{
			bytes_written = snprintf(command, total_len, "%u:", *data);
			WL_ERR(("wr %u %u\n", bytes_written, *data));
			command += bytes_written;
			cmd_info->bytes_written += bytes_written;
		}
		break;
		case WL_MBO_XTLV_CHAN:
		{
			bytes_written = snprintf(command, total_len, "%u:", *data);
			WL_ERR(("wr %u\n", bytes_written));
			command += bytes_written;
			cmd_info->bytes_written += bytes_written;
		}
		break;
		case WL_MBO_XTLV_PREFERENCE:
		{
			bytes_written = snprintf(command, total_len, "%u:", *data);
			WL_ERR(("wr %u\n", bytes_written));
			command += bytes_written;
			cmd_info->bytes_written += bytes_written;
		}
		break;
		case WL_MBO_XTLV_REASON_CODE:
		{
			bytes_written = snprintf(command, total_len, "%u ", *data);
			WL_ERR(("wr %u\n", bytes_written));
			command += bytes_written;
			cmd_info->bytes_written += bytes_written;
		}
		break;
		default:
			WL_ERR(("%s: Unknown tlv %u\n", __FUNCTION__, type));
	}
	WL_DBG(("Total bytes written %u\n", cmd_info->bytes_written));
	return BCME_OK;
}

static int
wl_android_mbo_subcmd_non_pref_chan(struct net_device *dev,
		const wl_drv_sub_cmd_t *cmd, char *command,
		wl_drv_cmd_info_t *cmd_info)
{
	int ret = BCME_OK;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	uint16 iovlen = 0;
	char *pcmd = command;
	bcm_iov_buf_t *iov_buf = NULL;
	bcm_iov_buf_t *p_resp = NULL;
	uint8 *iov_resp = NULL;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
	uint16 version;

	WL_ERR(("%s:%d\n", __FUNCTION__, __LINE__));
	iov_buf = (bcm_iov_buf_t *)MALLOCZ(cfg->osh, WLC_IOCTL_MEDLEN);
	if (iov_buf == NULL) {
		ret = -ENOMEM;
		WL_ERR(("iov buf memory alloc exited\n"));
		goto exit;
	}
	iov_resp = (uint8 *)MALLOCZ(cfg->osh, WLC_IOCTL_MAXLEN);
	if (iov_resp == NULL) {
		ret = -ENOMEM;
		WL_ERR(("iov resp memory alloc exited\n"));
		goto exit;
	}
	/* get */
	if (*pcmd == WL_IOCTL_ACTION_GET) {
		/* fill header */
		iov_buf->version = WL_MBO_IOV_VERSION_1_1;
		iov_buf->id = WL_MBO_CMD_LIST_CHAN_PREF;

		ret = wldev_iovar_getbuf(dev, "mbo", iov_buf, WLC_IOCTL_MEDLEN, iov_resp,
				WLC_IOCTL_MAXLEN, NULL);
		if (ret != BCME_OK) {
			goto exit;
		}
		p_resp = (bcm_iov_buf_t *)iov_resp;
		/* Check for version */
		version = dtoh16(*(uint16 *)iov_resp);
		if (version != WL_MBO_IOV_VERSION_1_1) {
			WL_ERR(("Version mismatch. returned ver %u expected %u\n",
				version, WL_MBO_IOV_VERSION_1_1));
			ret = -EINVAL;
		}
		if (p_resp->id == WL_MBO_CMD_LIST_CHAN_PREF) {
			ret = bcm_unpack_xtlv_buf((void *)cmd_info, (uint8 *)p_resp->data,
				p_resp->len, BCM_XTLV_OPTION_ALIGN32,
				wl_android_mbo_non_pref_chan_parse_cbfn);
			if (ret == BCME_OK) {
				ret = cmd_info->bytes_written;
			}
		} else {
			ret = -EINVAL;
			WL_ERR(("Mismatch: resp id %d req id %d\n", p_resp->id, cmd->id));
			goto exit;
		}
	} else {
		char *str = pcmd;
		uint opcl = 0, ch = 0, pref = 0, rc = 0;

		str = bcmstrtok(&pcmd, " ", NULL);
		if (!(strnicmp(str, "set", 3)) || (!strnicmp(str, "clear", 5))) {
			/* delete all configurations */
			iov_buf->version = WL_MBO_IOV_VERSION_1_1;
			iov_buf->id = WL_MBO_CMD_DEL_CHAN_PREF;
			iov_buf->len = 0;
			iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;
			ret = wldev_iovar_setbuf(dev, "mbo",
				iov_buf, iovlen, iov_resp, WLC_IOCTL_MAXLEN, NULL);
			if (ret != BCME_OK) {
				WL_ERR(("Fail to set iovar %d\n", ret));
				ret = -EINVAL;
				goto exit;
			}
		} else {
			WL_ERR(("Unknown command %s\n", str));
			goto exit;
		}
		/* parse non pref channel list */
		if (strnicmp(str, "set", 3) == 0) {
			uint8 cnt = 0;
			str = bcmstrtok(&pcmd, " ", NULL);
			while (str != NULL) {
				ret = sscanf(str, "%u:%u:%u:%u", &opcl, &ch, &pref, &rc);
				WL_ERR(("buflen %u op %u, ch %u, pref %u rc %u\n",
					buflen, opcl, ch, pref, rc));
				if (ret != 4) {
					WL_ERR(("Not all parameter presents\n"));
					ret = -EINVAL;
				}
				/* TODO: add a validation check here */
				memset_s(iov_buf, WLC_IOCTL_MEDLEN, 0, WLC_IOCTL_MEDLEN);
				buflen = buflen_start = WLC_IOCTL_MEDLEN;
				pxtlv = (uint8 *)&iov_buf->data[0];
				/* opclass */
				ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_OPCLASS,
					sizeof(uint8), (uint8 *)&opcl, BCM_XTLV_OPTION_ALIGN32);
				if (ret != BCME_OK) {
					goto exit;
				}
				/* channel */
				ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_CHAN,
					sizeof(uint8), (uint8 *)&ch, BCM_XTLV_OPTION_ALIGN32);
				if (ret != BCME_OK) {
					goto exit;
				}
				/* preference */
				ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_PREFERENCE,
					sizeof(uint8), (uint8 *)&pref, BCM_XTLV_OPTION_ALIGN32);
				if (ret != BCME_OK) {
					goto exit;
				}
				/* reason */
				ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_REASON_CODE,
					sizeof(uint8), (uint8 *)&rc, BCM_XTLV_OPTION_ALIGN32);
				if (ret != BCME_OK) {
					goto exit;
				}
				WL_ERR(("len %u\n", (buflen_start - buflen)));
				/* Now set the new non pref channels */
				iov_buf->version = WL_MBO_IOV_VERSION_1_1;
				iov_buf->id = WL_MBO_CMD_ADD_CHAN_PREF;
				iov_buf->len = buflen_start - buflen;
				iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;
				ret = wldev_iovar_setbuf(dev, "mbo",
					iov_buf, iovlen, iov_resp, WLC_IOCTL_MEDLEN, NULL);
				if (ret != BCME_OK) {
					WL_ERR(("Fail to set iovar %d\n", ret));
					ret = -EINVAL;
					goto exit;
				}
				cnt++;
				if (cnt >= MBO_MAX_CHAN_PREF_ENTRIES) {
					break;
				}
				WL_ERR(("%d cnt %u\n", __LINE__, cnt));
				str = bcmstrtok(&pcmd, " ", NULL);
			}
		}
		/* send a WNM notification request to associated AP */
		if (wl_get_drv_status(cfg, CONNECTED, dev)) {
			WL_INFORM(("Sending WNM Notif\n"));
			ret = wl_android_send_wnm_notif(dev, iov_buf, WLC_IOCTL_MEDLEN,
				iov_resp, WLC_IOCTL_MAXLEN, MBO_ATTR_NON_PREF_CHAN_REPORT);
			if (ret != BCME_OK) {
				WL_ERR(("Fail to send WNM notification %d\n", ret));
				ret = -EINVAL;
			}
		}
	}
exit:
	if (iov_buf) {
		MFREE(cfg->osh, iov_buf, WLC_IOCTL_MEDLEN);
	}
	if (iov_resp) {
		MFREE(cfg->osh, iov_resp, WLC_IOCTL_MAXLEN);
	}
	return ret;
}
#endif /* WL_MBO */

#ifdef WL_CP_COEX
extern int wl_cfg80211_send_msg_to_ril(void);
extern void wl_cfg80211_register_dev_ril_bridge_event_notifier(void);
extern void wl_cfg80211_unregister_dev_ril_bridge_event_notifier(void);
extern struct wl_cp_coex g_cx;
#endif /* WL_CP_COEX */

#if defined(WL_SUPPORT_AUTO_CHANNEL)
static s32
wl_android_set_auto_channel_scan_state(struct net_device *ndev)
{
	u32 val = 0;
	s32 ret = BCME_ERROR;
	struct bcm_cfg80211 *cfg = wl_get_cfg(ndev);
	/* Set interface up, explicitly. */
	val = 1;

	ret = wldev_ioctl_set(ndev, WLC_UP, (void *)&val, sizeof(val));
	if (ret < 0) {
		WL_ERR(("set interface up failed, error = %d\n", ret));
		goto done;
	}

	/* Stop all scan explicitly, till auto channel selection complete. */
	wl_set_drv_status(cfg, SCANNING, ndev);
	if (cfg->escan_info.ndev == NULL) {
		ret = BCME_OK;
		goto done;
	}

	wl_cfgscan_cancel_scan(cfg);

done:
	return ret;
}

s32
wl_android_get_freq_list_chanspecs(struct net_device *ndev, wl_uint32_list_t *list,
	s32 buflen, const char* cmd_str, int sta_channel, chanspec_band_t sta_acs_band)
{
	u32 freq = 0;
	chanspec_t chanspec = 0;
	s32 ret = BCME_OK;
	int i = 0;
	char *pcmd, *token;
	int len = buflen;
	u32 freq_bands = 0;
#ifdef WL_6G_BAND
	struct bcm_cfg80211 *cfg = wl_get_cfg(ndev);
#endif /* WL_6G_BAND */

	pcmd = bcmstrstr(cmd_str, FREQ_STR);
	pcmd += strlen(FREQ_STR);

	len -= sizeof(list->count);

	while ((token = strsep(&pcmd, ",")) != NULL) {
		if (*token == '\0')
			continue;

		if (len < sizeof(list->element[i]))
			break;

		freq = bcm_atoi(token);
		chanspec = wl_freq_to_chanspec(freq);
		/* Convert chanspec from frequency */
		if ((freq > 0) && (chanspec != INVCHANSPEC)) {
#ifdef WL_UNII4_CHAN
			/* Skip UNII-4 frequencies */
			if (CHSPEC_IS5G(chanspec) &&
				IS_UNII4_CHANNEL(wf_chspec_primary20_chan(chanspec))) {
				WL_DBG(("Skipped UNII-4 chanspec 0x%x\n", chanspec));
				continue;
			}
#endif /* WL_UNII4_CHAN */
#ifdef WL_5G_SOFTAP_ONLY_ON_DEF_CHAN
			if (CHSPEC_IS5G(chanspec) &&
				!(IS_5G_APCS_CHANNEL(wf_chspec_primary20_chan(chanspec)))) {
				WL_DBG(("Skipped unsupported 5G chanspec 0x%x\n", chanspec));
				continue;
			}
#endif /* WL_5G_SOFTAP_ONLY_ON_DEF_CHAN */
			WL_DBG(("%s:Adding chanspec in list : 0x%x at the index %d\n",
				__FUNCTION__, chanspec, i));
			/* mark all the bands found */
			freq_bands |= CHSPEC_TO_WLC_BAND(CHSPEC_BAND(chanspec));
			list->element[i] = chanspec;
			len -= sizeof(list->element[i]);
			i++;

			WL_DBG(("chspec[%d]: 0x%x (band=0x%x)\n",
				i, chanspec, CHSPEC_BAND(chanspec)));
		}
		WL_DBG(("** freq_bands=0x%x\n", freq_bands));
	}

	list->count = i;
	/* valid chanspec present in the list */
	if (list->count && sta_channel) {
		/* STA associated case. Can't do ACS.
		* Frequency list is order of lower to higher band.
		* check with the highest band entry.
		*/
		list->count = 1;
		chanspec = list->element[i-1];
		if (sta_acs_band == WL_CHANSPEC_BAND_2G) {
#ifdef WL_6G_BAND
			if ((freq_bands & WLC_BAND_6G) && cfg->band_6g_supported) {
				list->element[0] = wl_freq_to_chanspec(APCS_DEFAULT_6G_FREQ);
			} else
#endif /* WL_6G_BAND */
			if (freq_bands & WLC_BAND_5G) {
				list->element[0] = wl_freq_to_chanspec(APCS_DEFAULT_5G_FREQ);
			} else {
				freq = wl_channel_to_frequency(sta_channel, sta_acs_band);
				list->element[0] = wl_freq_to_chanspec(freq);
				WL_INFORM_MEM(("Softap on same band as STA."
						"Use SCC chanspec:0x%x\n",
						list->element[0]));
			}
		} else if (sta_acs_band == WL_CHANSPEC_BAND_5G) {
			if (freq_bands & WLC_BAND_5G) {
				if (IS_5G_APCS_CHANNEL(sta_channel)) {
					freq = wl_channel_to_frequency(sta_channel, sta_acs_band);
					list->element[0] = wl_freq_to_chanspec(freq);
				} else {
					list->element[0] =
						wl_freq_to_chanspec(APCS_DEFAULT_2G_FREQ);
				}
			} else {
				list->element[0] = wl_freq_to_chanspec(APCS_DEFAULT_2G_FREQ);
			}
		} else
#ifdef WL_6G_BAND
		if (sta_acs_band == WL_CHANSPEC_BAND_6G) {
			if ((freq_bands & WLC_BAND_6G) && cfg->band_6g_supported) {
				freq = wl_channel_to_frequency(sta_channel, sta_acs_band);
				list->element[0] = wl_freq_to_chanspec(freq);
				WL_INFORM_MEM(("Softap on same band as STA."
						"Use SCC chanspec:0x%x\n",
						list->element[0]));
			} else
			{
				list->element[0] = wl_freq_to_chanspec(APCS_DEFAULT_2G_FREQ);
			}
		} else
#endif /* WL_6G_BAND */
		{
			WL_ERR(("ACS: Invalid sta acs band: sta_acs_band = 0x%x\n", sta_acs_band));
			list->count = 0;
			return BCME_ERROR;
		}
	}

	if (!list->count)
		return BCME_ERROR;

	return ret;
}

s32
wl_android_get_band_chanspecs(struct net_device *ndev, void *buf, s32 buflen,
	chanspec_band_t band, bool acs_req)
{
	u32 channel = 0;
	s32 ret = BCME_ERROR;
	s32 i = 0;
	s32 j = 0;
	struct bcm_cfg80211 *cfg = wl_get_cfg(ndev);
	wl_uint32_list_t *list = NULL;
	chanspec_t chanspec = 0;

	if (band != 0xff) {
		chanspec |= (band | WL_CHANSPEC_BW_20 |
			WL_CHANSPEC_CTL_SB_NONE);
		chanspec = wl_chspec_host_to_driver(chanspec);
	}

	ret = wldev_iovar_getbuf_bsscfg(ndev, "chanspecs", (void *)&chanspec,
		sizeof(chanspec), buf, buflen, 0, &cfg->ioctl_buf_sync);
	if (ret < 0) {
		WL_ERR(("get 'chanspecs' failed, error = %d\n", ret));
		goto done;
	}

	list = (wl_uint32_list_t *)buf;
	/* Skip DFS and inavlid P2P channel. */
	for (i = 0, j = 0; i < dtoh32(list->count); i++) {
		if (!CHSPEC_IS20(list->element[i])) {
			continue;
		}
		chanspec = (chanspec_t) dtoh32(list->element[i]);
		channel = chanspec | WL_CHANSPEC_BW_20;
		channel = wl_chspec_host_to_driver(channel);

		ret = wldev_iovar_getint(ndev, "per_chan_info", &channel);
		if (ret < 0) {
			WL_ERR(("get 'per_chan_info' failed, error = %d\n", ret));
			goto done;
		}

		if (CHSPEC_IS5G(chanspec) && (CHANNEL_IS_RADAR(channel) ||
#ifndef ALLOW_5G_ACS
			((acs_req == true) &&
			!(IS_5G_APCS_CHANNEL(wf_chspec_primary20_chan(chanspec)))) ||
#endif /* !ALLOW_5G_ACS */
			(0))) {
			continue;
		} else if (!(CHSPEC_IS2G(chanspec) || CHSPEC_IS5G(chanspec)) &&
			!(CHSPEC_IS_6G_PSC(chanspec))) {
			continue;
#ifdef WL_UNII4_CHAN
		} else if (CHSPEC_IS5G(chanspec) &&
			IS_UNII4_CHANNEL(wf_chspec_primary20_chan(chanspec))) {
			/* Skip UNII-4 channels */
			WL_INFORM_MEM(("Skip UNII-4 chanspec from list : %x\n",
				chanspec));
			continue;
#endif /* WL_UNII4_CHAN */
		}
		else {
			list->element[j] = list->element[i];
			WL_DBG(("Adding chanspec in list : %x\n", list->element[j]));
		}

		j++;
	}

	list->count = j;

done:
	return ret;
}

static s32
wl_android_get_best_channel(struct net_device *ndev, void *buf, int buflen,
	int *channel)
{
	s32 ret = BCME_ERROR;
	int chosen = 0;
	int retry = 0;

	/* Start auto channel selection scan. */
	ret = wldev_ioctl_set(ndev, WLC_START_CHANNEL_SEL, NULL, 0);
	if (ret < 0) {
		WL_ERR(("can't start auto channel scan, error = %d\n", ret));
		*channel = 0;
		goto done;
	}

	/* Wait for auto channel selection, worst case possible delay is 5250ms. */
	retry = CHAN_SEL_RETRY_COUNT;

	while (retry--) {
		OSL_SLEEP(CHAN_SEL_IOCTL_DELAY);
		chosen = 0;
		ret = wldev_ioctl_get(ndev, WLC_GET_CHANNEL_SEL, &chosen, sizeof(chosen));
		if ((ret == 0) && (dtoh32(chosen) != 0)) {
			*channel = (u16)(chosen & 0x00FF);
			WL_INFORM_MEM(("selected channel = %d\n", *channel));
			break;
		}
		WL_DBG(("attempt = %d, ret = %d, chosen = %d\n",
			(CHAN_SEL_RETRY_COUNT - retry), ret, dtoh32(chosen)));
	}

	if (retry <= 0)	{
		WL_ERR(("failure, auto channel selection timed out\n"));
		*channel = 0;
		ret = BCME_ERROR;
	}

done:
	return ret;
}

static s32
wl_android_restore_auto_channel_scan_state(struct net_device *ndev)
{
	struct bcm_cfg80211 *cfg = wl_get_cfg(ndev);
	/* Clear scan stop driver status. */
	wl_clr_drv_status(cfg, SCANNING, ndev);

	return BCME_OK;
}

s32
wl_android_get_best_channels(struct net_device *dev, char* cmd, int total_len)
{
	int channel = 0;
	s32 ret = BCME_ERROR;
	u8 *buf = NULL;
	char *pos = cmd;
	struct bcm_cfg80211 *cfg = NULL;
	struct net_device *ndev = NULL;

	bzero(cmd, total_len);
	cfg = wl_get_cfg(dev);

	buf = (u8 *)MALLOC(cfg->osh, CHANSPEC_BUF_SIZE);
	if (buf == NULL) {
		WL_ERR(("failed to allocate chanspec buffer\n"));
		return -ENOMEM;
	}

	/*
	 * Always use primary interface, irrespective of interface on which
	 * command came.
	 */
	ndev = bcmcfg_to_prmry_ndev(cfg);

	/*
	 * Make sure that FW and driver are in right state to do auto channel
	 * selection scan.
	 */
	ret = wl_android_set_auto_channel_scan_state(ndev);
	if (ret < 0) {
		WL_ERR(("can't set auto channel scan state, error = %d\n", ret));
		goto done;
	}

	/* Best channel selection in 2.4GHz band. */
	ret = wl_android_get_band_chanspecs(ndev, (void *)buf, CHANSPEC_BUF_SIZE,
		WL_CHANSPEC_BAND_2G, false);
	if (ret < 0) {
		WL_ERR(("can't get chanspecs in 2.4GHz, error = %d\n", ret));
		goto done;
	}

	ret = wl_android_get_best_channel(ndev, (void *)buf, CHANSPEC_BUF_SIZE,
		&channel);
	if (ret < 0) {
		WL_ERR(("can't select best channel scan in 2.4GHz, error = %d\n", ret));
		goto done;
	}

	if (CHANNEL_IS_2G(channel)) {
		channel = ieee80211_channel_to_frequency(channel, IEEE80211_BAND_2GHZ);
	} else {
		WL_ERR(("invalid 2.4GHz channel, channel = %d\n", channel));
		channel = 0;
	}

	pos += snprintf(pos, total_len, "%04d ", channel);

	/* Best channel selection in 5GHz band. */
	ret = wl_android_get_band_chanspecs(ndev, (void *)buf, CHANSPEC_BUF_SIZE,
		WL_CHANSPEC_BAND_5G, false);
	if (ret < 0) {
		WL_ERR(("can't get chanspecs in 5GHz, error = %d\n", ret));
		goto done;
	}

	ret = wl_android_get_best_channel(ndev, (void *)buf, CHANSPEC_BUF_SIZE,
		&channel);
	if (ret < 0) {
		WL_ERR(("can't select best channel scan in 5GHz, error = %d\n", ret));
		goto done;
	}

	if (CHANNEL_IS_5G(channel)) {
		channel = ieee80211_channel_to_frequency(channel, IEEE80211_BAND_5GHZ);
	} else {
		WL_ERR(("invalid 5GHz channel, channel = %d\n", channel));
		channel = 0;
	}

	pos += snprintf(pos, total_len - (pos - cmd), "%04d ", channel);

	/* Set overall best channel same as 5GHz best channel. */
	pos += snprintf(pos, total_len - (pos - cmd), "%04d ", channel);

done:
	if (NULL != buf) {
		MFREE(cfg->osh, buf, CHANSPEC_BUF_SIZE);
	}

	/* Restore FW and driver back to normal state. */
	ret = wl_android_restore_auto_channel_scan_state(ndev);
	if (ret < 0) {
		WL_ERR(("can't restore auto channel scan state, error = %d\n", ret));
	}

	return (pos - cmd);
}

int
wl_android_set_spect(struct net_device *dev, int spect)
{
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
	int wlc_down = 1;
	int wlc_up = 1;
	int err = BCME_OK;

	if (!wl_get_drv_status_all(cfg, CONNECTED)) {
		err = wldev_ioctl_set(dev, WLC_DOWN, &wlc_down, sizeof(wlc_down));
		if (err) {
			WL_ERR(("%s: WLC_DOWN failed: code: %d\n", __func__, err));
			return err;
		}

		err = wldev_ioctl_set(dev, WLC_SET_SPECT_MANAGMENT, &spect, sizeof(spect));
		if (err) {
			WL_ERR(("%s: error setting spect: code: %d\n", __func__, err));
			return err;
		}

		err = wldev_ioctl_set(dev, WLC_UP, &wlc_up, sizeof(wlc_up));
		if (err) {
			WL_ERR(("%s: WLC_UP failed: code: %d\n", __func__, err));
			return err;
		}
	}
	return err;
}

int
wl_android_get_sta_channel(struct bcm_cfg80211 *cfg)
{
	chanspec_t *sta_chanspec = NULL;
	u32 channel = 0;

#ifdef WL_DUAL_APSTA
	if (wl_get_drv_status_all(cfg, CONNECTED) >= 2) {
		/* If both STA interfaces are connected return failure */
		return 0;
	} else {
		struct net_info *iter, *next;

		GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
		for_each_ndev(cfg, iter, next) {
			GCC_DIAGNOSTIC_POP();
			if ((iter->ndev) && (wl_get_drv_status(cfg, CONNECTED, iter->ndev)) &&
				(iter->ndev->ieee80211_ptr->iftype == NL80211_IFTYPE_STATION)) {
				if ((sta_chanspec = (chanspec_t *)wl_read_prof(cfg,
					iter->ndev, WL_PROF_CHAN))) {
					channel = wf_chspec_ctlchan(*sta_chanspec);
				}
			}
		}
	}
#else
	if (wl_get_drv_status(cfg, CONNECTED, bcmcfg_to_prmry_ndev(cfg))) {
		if ((sta_chanspec = (chanspec_t *)wl_read_prof(cfg,
			bcmcfg_to_prmry_ndev(cfg), WL_PROF_CHAN))) {
			channel = wf_chspec_ctlchan(*sta_chanspec);
		}
	}
#endif /* WL_DUAL_APSTA */

	return channel;
}

static int
wl_cfg80211_get_acs_band(int band)
{
	chanspec_band_t acs_band = WLC_ACS_BAND_INVALID;
	switch (band) {
		case WLC_BAND_AUTO:
			WL_DBG(("ACS full channel scan \n"));
			/* Restricting band to 2G in case of hw_mode any */
			acs_band = WL_CHANSPEC_BAND_2G;
			break;
#ifdef WL_6G_BAND
		case WLC_BAND_6G:
			WL_DBG(("ACS 6G band scan \n"));
			acs_band = WL_CHANSPEC_BAND_6G;
			break;
#endif /* WL_6G_BAND */
		case WLC_BAND_5G:
			WL_DBG(("ACS 5G band scan \n"));
			acs_band = WL_CHANSPEC_BAND_5G;
			break;
		case WLC_BAND_2G:
			/*
			 * If channel argument is not provided/ argument 20 is provided,
			 * Restrict channel to 2GHz, 20MHz BW, No SB
			 */
			WL_DBG(("ACS 2G band scan \n"));
			acs_band = WL_CHANSPEC_BAND_2G;
			break;
		default:
			WL_ERR(("ACS: No band chosen\n"));
			break;
	}
	WL_DBG(("%s: ACS: band = %d, acs_band = 0x%x\n", __FUNCTION__, band, acs_band));
	return acs_band;
}

/* SoftAP feature */
static int
wl_android_set_auto_channel(struct net_device *dev, const char* cmd_str,
	char* command, int total_len)
{
	int channel = 0, sta_channel = 0;
	chanspec_t sta_chanspec = 0;
	int chosen = 0;
	int retry = 0;
	int ret = 0;
	int spect = 0;
	u8 *reqbuf = NULL;
	uint32 band = WLC_BAND_INVALID;
	chanspec_band_t acs_band = WLC_ACS_BAND_INVALID;
	chanspec_band_t sta_band = WLC_ACS_BAND_INVALID;
	uint32 buf_size;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
	bool acs_freq_list_present = false;
	char *pcmd;
	int freq = 0;
	wl_ap_oper_data_t ap_oper_data = {0};

	if (cmd_str) {
		WL_INFORM(("Command: %s len:%d \n", cmd_str, (int)strlen(cmd_str)));
		pcmd = bcmstrstr(cmd_str, FREQ_STR);
		if (pcmd) {
			acs_freq_list_present = true;
			WL_INFORM_MEM(("ACS has freq list\n"));
		} else if (strnicmp(cmd_str, APCS_BAND_AUTO, strlen(APCS_BAND_AUTO)) == 0) {
			band = WLC_BAND_AUTO;
#ifdef WL_6G_BAND
		} else if (strnicmp(cmd_str, APCS_BAND_6G, strlen(APCS_BAND_6G)) == 0) {
			band = WLC_BAND_6G;
#endif /* WL_6G_BAND */
		} else if (strnicmp(cmd_str, APCS_BAND_5G, strlen(APCS_BAND_5G)) == 0) {
			band = WLC_BAND_5G;
		} else if (strnicmp(cmd_str, APCS_BAND_2G, strlen(APCS_BAND_2G)) == 0) {
			band = WLC_BAND_2G;
		} else {
			/*
			 * For backward compatibility: Some platforms used to issue argument 20 or 0
			 * to enforce the 2G channel selection
			 */
			channel = bcm_atoi(cmd_str);
			if ((channel == APCS_BAND_2G_LEGACY1) ||
				(channel == APCS_BAND_2G_LEGACY2)) {
				band = WLC_BAND_2G;
			} else {
				WL_ERR(("Invalid argument\n"));
				return -EINVAL;
			}
		}
	} else {
		/* If no argument is provided, default to 2G */
		WL_ERR(("No argument given default to 2.4G scan\n"));
		band = WLC_BAND_2G;
	}
	WL_INFORM(("HAPD_AUTO_CHANNEL = %d, band=%d \n", channel, band));

#ifdef WL_CP_COEX
	if (band == WLC_BAND_2G) {
		(void)memset_s(&g_cx, sizeof(g_cx), 0, sizeof(g_cx));
		wl_cfg80211_register_dev_ril_bridge_event_notifier();
		/* wait 500ms for receiving packet from CP */
		wl_cfg80211_send_msg_to_ril();
		wl_cfg80211_unregister_dev_ril_bridge_event_notifier();

		WL_ERR(("[CP_COEX] - Channel:%u\n", g_cx.ch_cpcoex));
		if (g_cx.ch_cpcoex) {
			channel = g_cx.ch_cpcoex;
			acs_band = WL_CHANSPEC_BAND_2G;
			goto done2;
		}
	}
#endif /* WL_CP_COEX */

	/* Check whether AP is already operational */
	wl_get_ap_chanspecs(cfg, &ap_oper_data);
	if (ap_oper_data.count >= MAX_AP_IFACES) {
		WL_ERR(("ACS request in multi AP case!! count:%d\n",
			ap_oper_data.count));
		return -EINVAL;
	}

	if (ap_oper_data.count == 1) {
		chanspec_t ch = ap_oper_data.iface[0].chspec;
		u16 ap_band;

		/* Single AP case. Bring up the AP in the other band */
		ap_band = CHSPEC_TO_WLC_BAND(ch);
		WL_INFORM_MEM(("AP operational in band:%d and incoming band:%d\n",
				ap_band, band));
		/* if incoming and operational AP band is same, it is invalid case */
		if (ap_band == band) {
			WL_ERR(("DUAL AP not allowed on same band\n"));
			goto done2;
		}
	}

	/* If STA is connected, return is STA chanspec, else ACS can be issued,
	 * set spect to 0 and proceed with ACS
	 */
	sta_chanspec = wl_cfg80211_get_sta_chanspec(cfg);
	if (sta_chanspec) {
		if (wf_chspec_valid(sta_chanspec)) {
			sta_channel = wf_chspec_ctlchan((chanspec_t)sta_chanspec);
			sta_band = CHSPEC_BAND((chanspec_t)sta_chanspec);
			WL_DBG(("sta_chanspec = 0x%x, sta_channel = %d, sta_band = 0x%x, "
				"sap band = %d\n", sta_chanspec, sta_channel, sta_band, band));
		} else {
			WL_ERR(("Invalid sta chanspec 0x%x\n", sta_chanspec));
			return -EINVAL;
		}
	}

	if ((sta_band != WLC_ACS_BAND_INVALID) && sta_chanspec && (band != WLC_BAND_INVALID)) {
		switch (sta_band) {
			case (WL_CHANSPEC_BAND_5G): {
				if ((IS_5G_APCS_CHANNEL(sta_channel)) &&
					(band == WLC_BAND_5G || band == WLC_BAND_AUTO)) {
					/* SCC is allowed only for DEF_5G Channel */
					channel = sta_channel;
					acs_band = sta_band;
				} else {
					channel = APCS_DEFAULT_2G_CH;
					acs_band = wl_cfg80211_get_acs_band(WLC_BAND_2G);
				}
				break;
			}
#ifdef WL_6G_BAND
			case (WL_CHANSPEC_BAND_6G):
			{
				if ((CHSPEC_IS_6G_PSC((chanspec_t)sta_chanspec)) &&
					(band == WLC_BAND_6G || band == WLC_BAND_AUTO)) {
					/* scc */
					channel = sta_channel;
					acs_band = sta_band;
				} else {
					WL_INFORM_MEM(("Fall back to 2g default\n"));
					channel = APCS_DEFAULT_2G_CH;
					acs_band = wl_cfg80211_get_acs_band(WLC_BAND_2G);
				}
				break;
			}
#endif /* WL_6G_BAND */
			case (WL_CHANSPEC_BAND_2G): {
#ifdef WL_6G_BAND
				if (band == WLC_BAND_6G) {
					channel = APCS_DEFAULT_6G_CH;
					acs_band = wl_cfg80211_get_acs_band(WLC_BAND_6G);
				} else
#endif /* WL_6G_BAND */
				if (band == WLC_BAND_5G) {
					channel = APCS_DEFAULT_5G_CH;
					acs_band = wl_cfg80211_get_acs_band(WLC_BAND_5G);
				} else if (band == WLC_BAND_AUTO) {
#ifdef WL_6G_BAND
					if (cfg->band_6g_supported) {
						channel = APCS_DEFAULT_6G_CH;
						acs_band = wl_cfg80211_get_acs_band(WLC_BAND_6G);
					} else {
						channel = APCS_DEFAULT_5G_CH;
						acs_band = wl_cfg80211_get_acs_band(WLC_BAND_5G);
					}
#else
					channel = APCS_DEFAULT_5G_CH;
					acs_band = wl_cfg80211_get_acs_band(WLC_BAND_5G);
#endif	/* WL_6G_BAND */
				} else {
					/* scc */
					channel = sta_channel;
					acs_band = sta_band;
				}
				break;
			}
			default:
				/* Intentional fall through to use same sta channel for softap */
				channel = sta_channel;
				acs_band = sta_band;
				break;
		}
		goto done2;
	}

	chosen = wl_ext_autochannel(dev, ACS_DRV_BIT, band, NULL);
	channel = wf_chspec_ctlchan(chosen);
	if (channel) {
		acs_band = CHSPEC_BAND(channel);
		goto done2;
	} else
		goto done;

	/* If AP is started on wlan0 iface,
	 * do not issue any iovar to fw and choose default ACS channel for softap
	 */
	if (dev == bcmcfg_to_prmry_ndev(cfg)) {
		if (wl_get_mode_by_netdev(cfg, dev) == WL_MODE_AP) {
			WL_INFORM_MEM(("Softap started on primary iface\n"));
			goto done;
		}
	}

	ret = wldev_ioctl_get(dev, WLC_GET_SPECT_MANAGMENT, &spect, sizeof(spect));
	if (ret) {
		WL_ERR(("ACS: error getting the spect, ret=%d\n", ret));
		goto done;
	}

	if (spect > 0) {
		ret = wl_android_set_spect(dev, 0);
		if (ret < 0) {
			WL_ERR(("ACS: error while setting spect, ret=%d\n", ret));
			goto done;
		}
	}

	reqbuf = (u8 *)MALLOCZ(cfg->osh, CHANSPEC_BUF_SIZE);
	if (reqbuf == NULL) {
		WL_ERR(("failed to allocate chanspec buffer\n"));
		return -ENOMEM;
	}

	if (acs_freq_list_present) {
		wl_uint32_list_t *list = NULL;
		bzero(reqbuf, sizeof(*reqbuf));
		list = (wl_uint32_list_t *)reqbuf;

		ret = wl_android_get_freq_list_chanspecs(dev, list, CHANSPEC_BUF_SIZE,
			cmd_str, sta_channel, sta_band);
		if (ret < 0) {
			WL_ERR(("ACS chanspec set failed!\n"));
			goto done;
		}

		/* skip ACS for single channel case */
		if (list->count == 1) {
			channel = wf_chspec_ctlchan((chanspec_t)list->element[0]);
			acs_band = CHSPEC_BAND((chanspec_t)list->element[0]);
			WL_INFORM_MEM(("chosen acs_chspec = 0x%x, channel =%d, acs_band =0x%x, "
				"sta_channel =%d, sta_acs_band = 0x%x\n",
				(chanspec_t)list->element[0], channel, acs_band,
				sta_channel, sta_band));
			goto done2;
		}
	} else {
		acs_band = wl_cfg80211_get_acs_band(band);
		if (acs_band == WLC_ACS_BAND_INVALID) {
			WL_ERR(("ACS: No band chosen\n"));
			goto done2;
		}

		if ((ret = wl_android_get_band_chanspecs(dev, reqbuf, CHANSPEC_BUF_SIZE,
			acs_band, true)) < 0) {
			WL_ERR(("ACS chanspec retrieval failed! \n"));
			goto done;
		}
	}

	buf_size = CHANSPEC_BUF_SIZE;
	ret = wldev_ioctl_set(dev, WLC_START_CHANNEL_SEL, (void *)reqbuf,
		buf_size);
	if (ret < 0) {
		WL_ERR(("can't start auto channel scan, err = %d\n", ret));
		channel = 0;
		goto done;
	}

	/* Wait for auto channel selection, max 3000 ms */
	if ((band == WLC_BAND_2G) || (band == WLC_BAND_5G) || (band == WLC_BAND_6G)) {
		OSL_SLEEP(500);
	} else {
		/*
		 * Full channel scan at the minimum takes 1.2secs
		 * even with parallel scan. max wait time: 3500ms
		 */
		OSL_SLEEP(1000);
	}

	retry = APCS_MAX_RETRY;
	while (retry--) {
		ret = wldev_ioctl_get(dev, WLC_GET_CHANNEL_SEL, &chosen,
			sizeof(chosen));
		if (ret < 0) {
			chosen = 0;
		} else {
			chosen = dtoh32(chosen);
		}

		if (wf_chspec_valid((chanspec_t)chosen)) {
#ifdef WL_6G_BAND
			if (CHSPEC_BAND((chanspec_t)chosen) == WL_CHANSPEC_BAND_6G &&
					cfg->band_6g_supported) {
				/* Cache the chanspec for 6g, as fw will choose the BW */
#if defined(LIMIT_AP_BW)
				chosen = (int) wl_cfg80211_get_ap_bw_limited_chspec(cfg,
					WL_CHANSPEC_BAND_6G, (chanspec_t) chosen);

#endif /* LIMIT_AP_BW */
				cfg->acs_chspec = (chanspec_t)chosen;
			}
#endif /* WL_6G_BAND */
			channel = wf_chspec_ctlchan((chanspec_t)chosen);
			acs_band = CHSPEC_BAND((chanspec_t)chosen);
			break;
		}
		WL_DBG(("%d tried, ret = %d, chosen = 0x%x, acs_band = 0x%x\n",
			(APCS_MAX_RETRY - retry), ret, chosen, acs_band));
		OSL_SLEEP(250);
	}

done:
	if ((retry == 0) || (ret < 0)) {
		/* On failure, fallback to a default channel */
		if (band == WLC_BAND_5G) {
			channel = APCS_DEFAULT_5G_CH;
#ifdef WL_6G_BAND
		} else if (band == WLC_BAND_6G) {
			channel = APCS_DEFAULT_6G_CH;
#endif /* WL_6G_BAND */
		} else {
			channel = APCS_DEFAULT_2G_CH;
		}
		WL_ERR(("ACS failed. Fall back to default channel (%d) \n", channel));
	}
done2:
	if (spect > 0) {
		if ((ret = wl_android_set_spect(dev, spect) < 0)) {
			WL_ERR(("ACS: error while setting spect\n"));
		}
	}

	if (reqbuf) {
		MFREE(cfg->osh, reqbuf, CHANSPEC_BUF_SIZE);
	}

	WL_MSG(dev->name, "band = %d, sta_band=0x%x acs_channel = %d, acs_band = 0x%x\n",
			band, sta_band, channel, acs_band);
	if (channel && (acs_band != WLC_ACS_BAND_INVALID)) {
		freq = wl_channel_to_frequency(channel, wl_cfg80211_get_acs_band(band));
		if (!freq) {
			WL_ERR(("ACS: Invalid freq\n"));
			return BCME_ERROR;
		}
		ret = snprintf(command, total_len, "%d freq=%d", channel, freq);
		WL_INFORM_MEM(("command result is %s \n", command));
	}

	return ret;
}
#endif /* WL_SUPPORT_AUTO_CHANNEL */

int wl_android_set_roam_mode(struct net_device *dev, char *command)
{
	int error = BCME_OK;
	int mode = 0;

	if (sscanf(command, "%*s %d", &mode) != 1) {
		DHD_ERROR(("wl_android_set_roam_mode: Failed to get Parameter\n"));
		return BCME_ERROR;
	}

	error = wldev_iovar_setint(dev, "roam_off", mode);
	if (error) {
		WL_ERR(("Failed to set roaming Mode %d, error = %d\n",
			mode, error));
	} else {
		WL_ERR(("succeeded to set roaming Mode %d,error = %d\n",
			mode, error));
		ROAMOFF_DBG_SAVE(dev, SET_ROAM_ROAMMODE, mode);
	}

	return  error;
}

int wl_android_set_ibss_beacon_ouidata(struct net_device *dev, char *command, int total_len)
{
	char ie_buf[VNDR_IE_MAX_LEN];
	char *ioctl_buf = NULL;
	char hex[] = "XX";
	char *pcmd = NULL;
	int ielen = 0, datalen = 0, idx = 0, tot_len = 0;
	vndr_ie_setbuf_t *vndr_ie = NULL;
	s32 iecount;
	uint32 pktflag;
	s32 err = BCME_OK, bssidx;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);

	/* Check the VSIE (Vendor Specific IE) which was added.
	 *  If exist then send IOVAR to delete it
	 */
	if (wl_cfg80211_ibss_vsie_delete(dev) != BCME_OK) {
		return -EINVAL;
	}

	if (total_len < (strlen(CMD_SETIBSSBEACONOUIDATA) + 1)) {
		WL_ERR(("error. total_len:%d\n", total_len));
		return -EINVAL;
	}

	pcmd = command + strlen(CMD_SETIBSSBEACONOUIDATA) + 1;
	for (idx = 0; idx < DOT11_OUI_LEN; idx++) {
		if (*pcmd == '\0') {
			WL_ERR(("error while parsing OUI.\n"));
			return -EINVAL;
		}
		hex[0] = *pcmd++;
		hex[1] = *pcmd++;
		ie_buf[idx] =  (uint8)simple_strtoul(hex, NULL, 16);
	}
	pcmd++;
	while ((*pcmd != '\0') && (idx < VNDR_IE_MAX_LEN)) {
		hex[0] = *pcmd++;
		hex[1] = *pcmd++;
		ie_buf[idx++] =  (uint8)simple_strtoul(hex, NULL, 16);
		datalen++;
	}

	if (datalen <= 0) {
		WL_ERR(("error. vndr ie len:%d\n", datalen));
		return -EINVAL;
	}

	tot_len = (int)(sizeof(vndr_ie_setbuf_t) + (datalen - 1));
	vndr_ie = (vndr_ie_setbuf_t *)MALLOCZ(cfg->osh, tot_len);
	if (!vndr_ie) {
		WL_ERR(("IE memory alloc failed\n"));
		return -ENOMEM;
	}
	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strlcpy(vndr_ie->cmd, "add", sizeof(vndr_ie->cmd));

	/* Set the IE count - the buffer contains only 1 IE */
	iecount = htod32(1);
	memcpy((void *)&vndr_ie->vndr_ie_buffer.iecount, &iecount, sizeof(s32));

	/* Set packet flag to indicate that BEACON's will contain this IE */
	pktflag = htod32(VNDR_IE_BEACON_FLAG | VNDR_IE_PRBRSP_FLAG);
	memcpy((void *)&vndr_ie->vndr_ie_buffer.vndr_ie_list[0].pktflag, &pktflag,
		sizeof(u32));
	/* Set the IE ID */
	vndr_ie->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.id = (uchar) DOT11_MNG_PROPR_ID;

	memcpy(&vndr_ie->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui, &ie_buf,
		DOT11_OUI_LEN);
	memcpy(&vndr_ie->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.data,
		&ie_buf[DOT11_OUI_LEN], datalen);

	ielen = DOT11_OUI_LEN + datalen;
	vndr_ie->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.len = (uchar) ielen;

	ioctl_buf = (char *)MALLOC(cfg->osh, WLC_IOCTL_MEDLEN);
	if (!ioctl_buf) {
		WL_ERR(("ioctl memory alloc failed\n"));
		if (vndr_ie) {
			MFREE(cfg->osh, vndr_ie, tot_len);
		}
		return -ENOMEM;
	}
	bzero(ioctl_buf, WLC_IOCTL_MEDLEN);	/* init the buffer */
	if ((bssidx = wl_get_bssidx_by_wdev(cfg, dev->ieee80211_ptr)) < 0) {
		WL_ERR(("Find index failed\n"));
		err = BCME_ERROR;
		goto end;
	}
	err = wldev_iovar_setbuf_bsscfg(dev, "vndr_ie", vndr_ie, tot_len, ioctl_buf,
			WLC_IOCTL_MEDLEN, bssidx, &cfg->ioctl_buf_sync);
end:
	if (err != BCME_OK) {
		err = -EINVAL;
		if (vndr_ie) {
			MFREE(cfg->osh, vndr_ie, tot_len);
		}
	}
	else {
		/* do NOT free 'vndr_ie' for the next process */
		wl_cfg80211_ibss_vsie_set_buffer(dev, vndr_ie, tot_len);
	}

	if (ioctl_buf) {
		MFREE(cfg->osh, ioctl_buf, WLC_IOCTL_MEDLEN);
	}

	return err;
}

#if defined(BCMFW_ROAM_ENABLE)
static int
wl_android_set_roampref(struct net_device *dev, char *command, int total_len)
{
	int error = 0;
	char smbuf[WLC_IOCTL_SMLEN];
	uint8 buf[JOIN_PREF_MAX_BUF_SIZE];
	uint8 *pref = buf;
	char *pcmd;
	int num_ucipher_suites = 0;
	int num_akm_suites = 0;
	wpa_suite_t ucipher_suites[MAX_NUM_SUITES];
	wpa_suite_t akm_suites[MAX_NUM_SUITES];
	int num_tuples = 0;
	int total_bytes = 0;
	int total_len_left;
	int i, j;
	char hex[] = "XX";

	pcmd = command + strlen(CMD_SET_ROAMPREF) + 1;
	total_len_left = total_len - strlen(CMD_SET_ROAMPREF) + 1;

	num_akm_suites = simple_strtoul(pcmd, NULL, 16);
	if (num_akm_suites > MAX_NUM_SUITES) {
		DHD_ERROR(("too many AKM suites = %d\n", num_akm_suites));
		return -1;
	}

	/* Increment for number of AKM suites field + space */
	pcmd += 3;
	total_len_left -= 3;

	/* check to make sure pcmd does not overrun */
	if (total_len_left < (num_akm_suites * WIDTH_AKM_SUITE))
		return -1;

	bzero(buf, sizeof(buf));
	bzero(akm_suites, sizeof(akm_suites));
	bzero(ucipher_suites, sizeof(ucipher_suites));

	/* Save the AKM suites passed in the command */
	for (i = 0; i < num_akm_suites; i++) {
		/* Store the MSB first, as required by join_pref */
		for (j = 0; j < 4; j++) {
			hex[0] = *pcmd++;
			hex[1] = *pcmd++;
			buf[j] = (uint8)simple_strtoul(hex, NULL, 16);
		}
		memcpy((uint8 *)&akm_suites[i], buf, sizeof(uint32));
	}

	total_len_left -= (num_akm_suites * WIDTH_AKM_SUITE);
	num_ucipher_suites = simple_strtoul(pcmd, NULL, 16);
	/* Increment for number of cipher suites field + space */
	pcmd += 3;
	total_len_left -= 3;

	if (total_len_left < (num_ucipher_suites * WIDTH_AKM_SUITE))
		return -1;

	/* Save the cipher suites passed in the command */
	for (i = 0; i < num_ucipher_suites; i++) {
		/* Store the MSB first, as required by join_pref */
		for (j = 0; j < 4; j++) {
			hex[0] = *pcmd++;
			hex[1] = *pcmd++;
			buf[j] = (uint8)simple_strtoul(hex, NULL, 16);
		}
		memcpy((uint8 *)&ucipher_suites[i], buf, sizeof(uint32));
	}

	/* Join preference for RSSI
	 * Type	  : 1 byte (0x01)
	 * Length : 1 byte (0x02)
	 * Value  : 2 bytes	(reserved)
	 */
	*pref++ = WL_JOIN_PREF_RSSI;
	*pref++ = JOIN_PREF_RSSI_LEN;
	*pref++ = 0;
	*pref++ = 0;

	/* Join preference for WPA
	 * Type	  : 1 byte (0x02)
	 * Length : 1 byte (not used)
	 * Value  : (variable length)
	 *		reserved: 1 byte
	 *      count	: 1 byte (no of tuples)
	 *		Tuple1	: 12 bytes
	 *			akm[4]
	 *			ucipher[4]
	 *			mcipher[4]
	 *		Tuple2	: 12 bytes
	 *		Tuplen	: 12 bytes
	 */
	num_tuples = num_akm_suites * num_ucipher_suites;
	if (num_tuples != 0) {
		if (num_tuples <= JOIN_PREF_MAX_WPA_TUPLES) {
			*pref++ = WL_JOIN_PREF_WPA;
			*pref++ = 0;
			*pref++ = 0;
			*pref++ = (uint8)num_tuples;
			total_bytes = JOIN_PREF_RSSI_SIZE + JOIN_PREF_WPA_HDR_SIZE +
				(JOIN_PREF_WPA_TUPLE_SIZE * num_tuples);
		} else {
			DHD_ERROR(("wl_android_set_roampref: Too many wpa configs"
				" for join_pref \n"));
			return -1;
		}
	} else {
		/* No WPA config, configure only RSSI preference */
		total_bytes = JOIN_PREF_RSSI_SIZE;
	}

	/* akm-ucipher-mcipher tuples in the format required for join_pref */
	for (i = 0; i < num_ucipher_suites; i++) {
		for (j = 0; j < num_akm_suites; j++) {
			memcpy(pref, (uint8 *)&akm_suites[j], WPA_SUITE_LEN);
			pref += WPA_SUITE_LEN;
			memcpy(pref, (uint8 *)&ucipher_suites[i], WPA_SUITE_LEN);
			pref += WPA_SUITE_LEN;
			/* Set to 0 to match any available multicast cipher */
			bzero(pref, WPA_SUITE_LEN);
			pref += WPA_SUITE_LEN;
		}
	}

	prhex("join pref", (uint8 *)buf, total_bytes);
	error = wldev_iovar_setbuf(dev, "join_pref", buf, total_bytes, smbuf, sizeof(smbuf), NULL);
	if (error) {
		DHD_ERROR(("Failed to set join_pref, error = %d\n", error));
	}
	return error;
}
#endif /* defined(BCMFW_ROAM_ENABLE */

static int
wl_android_iolist_add(struct net_device *dev, struct list_head *head, struct io_cfg *config)
{
	struct io_cfg *resume_cfg;
	s32 ret;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);

	resume_cfg = (struct io_cfg *)MALLOCZ(cfg->osh, sizeof(struct io_cfg));
	if (!resume_cfg)
		return -ENOMEM;

	if (config->iovar) {
		ret = wldev_iovar_getint(dev, config->iovar, &resume_cfg->param);
		if (ret) {
			DHD_ERROR(("wl_android_iolist_add: Failed to get current %s value\n",
				config->iovar));
			goto error;
		}

		ret = wldev_iovar_setint(dev, config->iovar, config->param);
		if (ret) {
			DHD_ERROR(("wl_android_iolist_add: Failed to set %s to %d\n",
				config->iovar, config->param));
			goto error;
		}
		resume_cfg->iovar = config->iovar;
#ifdef DEBUG_SETROAMMODE
		if (strnicmp(config->iovar, CMD_ROAMOFF, strlen(CMD_ROAMOFF)) == 0) {
			ROAMOFF_DBG_SAVE(dev, SET_ROAM_IOLIST_ADD, config->param);
		}
#endif /* DEBUG_SETROAMMODE */
	} else {
		resume_cfg->arg = MALLOCZ(cfg->osh, config->len);
		if (!resume_cfg->arg) {
			ret = -ENOMEM;
			goto error;
		}
		ret = wldev_ioctl_get(dev, config->ioctl, resume_cfg->arg, config->len);
		if (ret) {
			DHD_ERROR(("wl_android_iolist_add: Failed to get ioctl %d\n",
				config->ioctl));
			goto error;
		}
		ret = wldev_ioctl_set(dev, config->ioctl + 1, config->arg, config->len);
		if (ret) {
			DHD_ERROR(("wl_android_iolist_add: Failed to set %s to %d\n",
				config->iovar, config->param));
			goto error;
		}
		if (config->ioctl + 1 == WLC_SET_PM)
			wl_cfg80211_update_power_mode(dev);
		resume_cfg->ioctl = config->ioctl;
		resume_cfg->len = config->len;
	}

	/* assuming only one active user and no list protection */
	list_add(&resume_cfg->list, head);

	return 0;
error:
	MFREE(cfg->osh, resume_cfg->arg, config->len);
	MFREE(cfg->osh, resume_cfg, sizeof(struct io_cfg));
	return ret;
}

static void
wl_android_iolist_resume(struct net_device *dev, struct list_head *head)
{
	struct io_cfg *config;
	struct list_head *cur, *q;
	s32 ret = 0;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
	GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
	list_for_each_safe(cur, q, head) {
		config = list_entry(cur, struct io_cfg, list);
		GCC_DIAGNOSTIC_POP();
		if (config->iovar) {
			if (!ret) {
				ret = wldev_iovar_setint(dev, config->iovar,
					config->param);
#ifdef DEBUG_SETROAMMODE
				if (strnicmp(config->iovar, CMD_ROAMOFF,
					strlen(CMD_ROAMOFF)) == 0) {
					ROAMOFF_DBG_SAVE(dev, SET_ROAM_IOLIST_RESUME,
						config->param);
				}
#endif /* DEBUG_SETROAMMODE */
			}
		} else {
			if (!ret)
				ret = wldev_ioctl_set(dev, config->ioctl + 1,
					config->arg, config->len);
			if (config->ioctl + 1 == WLC_SET_PM)
				wl_cfg80211_update_power_mode(dev);
			MFREE(cfg->osh, config->arg, config->len);
		}
		list_del(cur);
		MFREE(cfg->osh, config, sizeof(struct io_cfg));
	}
}

static int
wl_android_set_miracast(struct net_device *dev, char *command)
{
	int mode, val = 0;
	int ret = 0;
	struct io_cfg config;

	if (sscanf(command, "%*s %d", &mode) != 1) {
		DHD_ERROR(("wl_android_set_miracasts: Failed to get Parameter\n"));
		return -1;
	}

	DHD_INFO(("wl_android_set_miracast: enter miracast mode %d\n", mode));

	if (miracast_cur_mode == mode) {
		return 0;
	}

	wl_android_iolist_resume(dev, &miracast_resume_list);
	miracast_cur_mode = MIRACAST_MODE_OFF;

	bzero((void *)&config, sizeof(config));
	switch (mode) {
	case MIRACAST_MODE_SOURCE:
#ifdef MIRACAST_MCHAN_ALGO
		/* setting mchan_algo to platform specific value */
		config.iovar = "mchan_algo";

		/* check for station's beacon interval(BI)
		 * If BI is over 100ms, don't use mchan_algo
		 */
		ret = wldev_ioctl_get(dev, WLC_GET_BCNPRD, &val, sizeof(int));
		if (!ret && val > 100) {
			config.param = 0;
			DHD_ERROR(("wl_android_set_miracast: Connected station's beacon interval: "
				"%d and set mchan_algo to %d \n",
				val, config.param));
		} else {
			config.param = MIRACAST_MCHAN_ALGO;
		}
		ret = wl_android_iolist_add(dev, &miracast_resume_list, &config);
		if (ret) {
			goto resume;
		}
#endif /* MIRACAST_MCHAN_ALGO */

#ifdef MIRACAST_MCHAN_BW
		/* setting mchan_bw to platform specific value */
		config.iovar = "mchan_bw";
		config.param = MIRACAST_MCHAN_BW;
		ret = wl_android_iolist_add(dev, &miracast_resume_list, &config);
		if (ret) {
			goto resume;
		}
#endif /* MIRACAST_MCHAN_BW */

#ifdef MIRACAST_AMPDU_SIZE
		/* setting apmdu to platform specific value */
		config.iovar = "ampdu_mpdu";
		config.param = MIRACAST_AMPDU_SIZE;
		ret = wl_android_iolist_add(dev, &miracast_resume_list, &config);
		if (ret) {
			goto resume;
		}
#endif /* MIRACAST_AMPDU_SIZE */
		/* FALLTROUGH */
		/* Source mode shares most configurations with sink mode.
		 * Fall through here to avoid code duplication
		 */
		fallthrough;

	case MIRACAST_MODE_SINK:
		/* disable internal roaming */
		config.iovar = "roam_off";
		config.param = 1;
		config.arg = NULL;
		config.len = 0;
		ret = wl_android_iolist_add(dev, &miracast_resume_list, &config);
		if (ret) {
			goto resume;
		}

		/* tunr off pm */
		ret = wldev_ioctl_get(dev, WLC_GET_PM, &val, sizeof(val));
		if (ret) {
			goto resume;
		}

		if (val != PM_OFF) {
			val = PM_OFF;
			config.iovar = NULL;
			config.ioctl = WLC_GET_PM;
			config.arg = &val;
			config.len = sizeof(int);
			ret = wl_android_iolist_add(dev, &miracast_resume_list, &config);
			if (ret) {
				goto resume;
			}
		}
		break;
	case MIRACAST_MODE_OFF:
	default:
		break;
	}
	miracast_cur_mode = mode;

	return 0;

resume:
	DHD_ERROR(("wl_android_set_miracast: turnoff miracast mode because of err%d\n", ret));
	wl_android_iolist_resume(dev, &miracast_resume_list);
	return ret;
}

#ifdef WL_NETLINK
#define NETLINK_OXYGEN     30
#define AIBSS_BEACON_TIMEOUT	10

static struct sock *nl_sk = NULL;

static void wl_netlink_recv(struct sk_buff *skb)
{
	WL_ERR(("netlink_recv called\n"));
}

static int wl_netlink_init(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
	struct netlink_kernel_cfg cfg = {
		.input	= wl_netlink_recv,
	};
#endif

	if (nl_sk != NULL) {
		WL_ERR(("nl_sk already exist\n"));
		return BCME_ERROR;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0))
	nl_sk = netlink_kernel_create(&init_net, NETLINK_OXYGEN,
		0, wl_netlink_recv, NULL, THIS_MODULE);
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 0))
	nl_sk = netlink_kernel_create(&init_net, NETLINK_OXYGEN, THIS_MODULE, &cfg);
#else
	nl_sk = netlink_kernel_create(&init_net, NETLINK_OXYGEN, &cfg);
#endif

	if (nl_sk == NULL) {
		WL_ERR(("nl_sk is not ready\n"));
		return BCME_ERROR;
	}

	return BCME_OK;
}

static void wl_netlink_deinit(void)
{
	if (nl_sk) {
		netlink_kernel_release(nl_sk);
		nl_sk = NULL;
	}
}

s32
wl_netlink_send_msg(int pid, int type, int seq, const void *data, size_t size)
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh = NULL;
	int ret = -1;

	if (nl_sk == NULL) {
		WL_ERR(("nl_sk was not initialized\n"));
		goto nlmsg_failure;
	}

	skb = alloc_skb(NLMSG_SPACE(size), GFP_ATOMIC);
	if (skb == NULL) {
		WL_ERR(("failed to allocate memory\n"));
		goto nlmsg_failure;
	}

	nlh = nlmsg_put(skb, 0, 0, 0, size, 0);
	if (nlh == NULL) {
		WL_ERR(("failed to build nlmsg, skb_tailroom:%d, nlmsg_total_size:%d\n",
			skb_tailroom(skb), nlmsg_total_size(size)));
		dev_kfree_skb(skb);
		goto nlmsg_failure;
	}

	memcpy(nlmsg_data(nlh), data, size);
	nlh->nlmsg_seq = seq;
	nlh->nlmsg_type = type;

	/* netlink_unicast() takes ownership of the skb and frees it itself. */
	ret = netlink_unicast(nl_sk, skb, pid, 0);
	WL_DBG(("netlink_unicast() pid=%d, ret=%d\n", pid, ret));

nlmsg_failure:
	return ret;
}
#endif /* WL_NETLINK */

int wl_keep_alive_set(struct net_device *dev, char* extra)
{
	wl_mkeep_alive_pkt_v1_t	mkeep_alive_pkt;
	int ret;
	uint period_msec = 0;
	char *buf;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);

	if (extra == NULL) {
		 DHD_ERROR(("wl_keep_alive_set: extra is NULL\n"));
		 return -1;
	}
	if (sscanf(extra, "%d", &period_msec) != 1) {
		 DHD_ERROR(("wl_keep_alive_set: sscanf error. check period_msec value\n"));
		 return -EINVAL;
	}
	DHD_ERROR(("wl_keep_alive_set: period_msec is %d\n", period_msec));

	bzero(&mkeep_alive_pkt, sizeof(wl_mkeep_alive_pkt_v1_t));

	mkeep_alive_pkt.period_msec = period_msec;
	mkeep_alive_pkt.version = htod16(WL_MKEEP_ALIVE_VERSION_1);
	mkeep_alive_pkt.length = htod16(WL_MKEEP_ALIVE_FIXED_LEN);

	/* Setup keep alive zero for null packet generation */
	mkeep_alive_pkt.keep_alive_id = 0;
	mkeep_alive_pkt.len_bytes = 0;

	buf = (char *)MALLOC(cfg->osh, WLC_IOCTL_SMLEN);
	if (!buf) {
		DHD_ERROR(("wl_keep_alive_set: buffer alloc failed\n"));
		return BCME_NOMEM;
	}
	ret = wldev_iovar_setbuf(dev, "mkeep_alive", (char *)&mkeep_alive_pkt,
			WL_MKEEP_ALIVE_FIXED_LEN, buf, WLC_IOCTL_SMLEN, NULL);
	if (ret < 0)
		DHD_ERROR(("wl_keep_alive_set:keep_alive set failed:%d\n", ret));
	else
		DHD_TRACE(("wl_keep_alive_set: keep_alive set ok\n"));
	MFREE(cfg->osh, buf, WLC_IOCTL_SMLEN);
	return ret;
}

#ifdef P2PRESP_WFDIE_SRC
static int wl_android_get_wfdie_resp(struct net_device *dev, char *command, int total_len)
{
	int error = 0;
	int bytes_written = 0;
	int only_resp_wfdsrc = 0;

	error = wldev_iovar_getint(dev, "p2p_only_resp_wfdsrc", &only_resp_wfdsrc);
	if (error) {
		DHD_ERROR(("wl_android_get_wfdie_resp: Failed to get the mode"
			" for only_resp_wfdsrc, error = %d\n",
			error));
		return -1;
	}

	bytes_written = snprintf(command, total_len, "%s %d",
		CMD_P2P_GET_WFDIE_RESP, only_resp_wfdsrc);

	return bytes_written;
}

static int wl_android_set_wfdie_resp(struct net_device *dev, int only_resp_wfdsrc)
{
	int error = 0;

	error = wldev_iovar_setint(dev, "p2p_only_resp_wfdsrc", only_resp_wfdsrc);
	if (error) {
		DHD_ERROR(("wl_android_set_wfdie_resp: Failed to set only_resp_wfdsrc %d,"
			" error = %d\n",
			only_resp_wfdsrc, error));
		return -1;
	}

	return 0;
}
#endif /* P2PRESP_WFDIE_SRC */

static int wl_android_get_link_status(struct net_device *dev, char *command,
	int total_len)
{
	int bytes_written, error, result = 0, single_stream, stf = -1, i, nss = 0, mcs_map;
	uint32 rspec;
	uint encode, txexp;
	wl_bss_info_v109_t *bi;
	int datalen = sizeof(uint32) + sizeof(wl_bss_info_v109_t);
	char buf[WLC_IOCTL_SMLEN];

	if (datalen > WLC_IOCTL_SMLEN) {
		WL_ERR(("data too big\n"));
		return -1;
	}

	bzero(buf, datalen);
	/* get BSS information */
	*(u32 *) buf = htod32(datalen);
	error = wldev_ioctl_get(dev, WLC_GET_BSS_INFO, (void *)buf, datalen);
	if (unlikely(error)) {
		WL_ERR(("Could not get bss info %d\n", error));
		return -1;
	}

	bi = (wl_bss_info_v109_t*) (buf + sizeof(uint32));

	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		if (bi->BSSID.octet[i] > 0) {
			break;
		}
	}

	if (i == ETHER_ADDR_LEN) {
		WL_DBG(("No BSSID\n"));
		return -1;
	}

	/* check VHT capability at beacon */
	if (bi->vht_cap) {
		if (CHSPEC_IS5G(bi->chanspec)) {
			result |= WL_ANDROID_LINK_AP_VHT_SUPPORT;
		}
	}

	/* get a rspec (radio spectrum) rate */
	error = wldev_iovar_getint(dev, "nrate", &rspec);
	if (unlikely(error) || rspec == 0) {
		WL_ERR(("get link status error (%d)\n", error));
		return -1;
	}

	/* referred wl_nrate_print() for the calculation */
	encode = (rspec & WL_RSPEC_ENCODING_MASK);
	txexp = (rspec & WL_RSPEC_TXEXP_MASK) >> WL_RSPEC_TXEXP_SHIFT;

	switch (encode) {
	case WL_RSPEC_ENCODE_HT:
		/* check Rx MCS Map for HT */
		for (i = 0; i < MAX_STREAMS_SUPPORTED; i++) {
			int8 bitmap = 0xFF;
			if (i == MAX_STREAMS_SUPPORTED-1) {
				bitmap = 0x7F;
			}
			if (bi->basic_mcs[i] & bitmap) {
				nss++;
			}
		}
		break;
	case WL_RSPEC_ENCODE_VHT:
		/* check Rx MCS Map for VHT */
		for (i = 1; i <= VHT_CAP_MCS_MAP_NSS_MAX; i++) {
			mcs_map = VHT_MCS_MAP_GET_MCS_PER_SS(i, dtoh16(bi->vht_rxmcsmap));
			if (mcs_map != VHT_CAP_MCS_MAP_NONE) {
				nss++;
			}
		}
		break;
	}

	/* check MIMO capability with nss in beacon */
	if (nss > 1) {
		result |= WL_ANDROID_LINK_AP_MIMO_SUPPORT;
	}

	/* Legacy rates WL_RSPEC_ENCODE_RATE are single stream, and
	 * HT rates for mcs 0-7 are single stream.
	 * In case of VHT NSS comes from rspec.
	 */
	single_stream = (encode == WL_RSPEC_ENCODE_RATE) ||
		((encode == WL_RSPEC_ENCODE_HT) && (rspec & WL_RSPEC_HT_MCS_MASK) < 8) ||
		((encode == WL_RSPEC_ENCODE_VHT) &&
		((rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT) == 1);

	if (txexp == 0) {
		if ((rspec & WL_RSPEC_STBC) && single_stream) {
			stf = OLD_NRATE_STF_STBC;
		} else {
			stf = (single_stream) ? OLD_NRATE_STF_SISO : OLD_NRATE_STF_SDM;
		}
	} else if (txexp == 1 && single_stream) {
		stf = OLD_NRATE_STF_CDD;
	}

	/* check 11ac (VHT) */
	if (encode == WL_RSPEC_ENCODE_VHT) {
		if (CHSPEC_IS5G(bi->chanspec)) {
			result |= WL_ANDROID_LINK_VHT;
		}
	}

	/* check MIMO */
	if (result & WL_ANDROID_LINK_AP_MIMO_SUPPORT) {
		switch (stf) {
		case OLD_NRATE_STF_SISO:
			break;
		case OLD_NRATE_STF_CDD:
		case OLD_NRATE_STF_STBC:
			result |= WL_ANDROID_LINK_MIMO;
			break;
		case OLD_NRATE_STF_SDM:
			if (!single_stream) {
				result |= WL_ANDROID_LINK_MIMO;
			}
			break;
		}
	}

	WL_DBG(("%s:result=%d, stf=%d, single_stream=%d, mcs map=%d\n",
		__FUNCTION__, result, stf, single_stream, nss));

	bytes_written = snprintf(command, total_len, "%s %d", CMD_GET_LINK_STATUS, result);

	return bytes_written;
}

#ifdef P2P_LISTEN_OFFLOADING

s32
wl_cfg80211_p2plo_deinit(struct bcm_cfg80211 *cfg)
{
	s32 bssidx;
	int ret = 0;
	int p2plo_pause = 0;
	dhd_pub_t *dhd = NULL;
	if (!cfg || !cfg->p2p) {
		WL_ERR(("Wl %p or cfg->p2p %p is null\n",
			cfg, cfg ? cfg->p2p : 0));
		return 0;
	}

	dhd =  (dhd_pub_t *)(cfg->pub);
	if (!dhd->up) {
		WL_ERR(("bus is already down\n"));
		return ret;
	}

	bssidx = wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_DEVICE);
	ret = wldev_iovar_setbuf_bsscfg(bcmcfg_to_prmry_ndev(cfg),
			"p2po_stop", (void*)&p2plo_pause, sizeof(p2plo_pause),
			cfg->ioctl_buf, WLC_IOCTL_SMLEN, bssidx, &cfg->ioctl_buf_sync);
	if (ret < 0) {
		WL_ERR(("p2po_stop Failed :%d\n", ret));
	}

	return  ret;
}

s32
wl_cfg80211_p2plo_listen_start(struct net_device *dev, u8 *buf, int len)
{
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
	s32 bssidx = wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_DEVICE);
	wl_p2plo_listen_t p2plo_listen;
	int ret = -EAGAIN;
	int channel = 0;
	int period = 0;
	int interval = 0;
	int count = 0;
	if (WL_DRV_STATUS_SENDING_AF_FRM_EXT(cfg)) {
		WL_ERR(("Sending Action Frames. Try it again.\n"));
		goto exit;
	}

	if (wl_get_drv_status_all(cfg, SCANNING)) {
		WL_ERR(("Scanning already\n"));
		goto exit;
	}

	if (wl_get_drv_status(cfg, SCAN_ABORTING, dev)) {
		WL_ERR(("Scanning being aborted\n"));
		goto exit;
	}

	if (wl_get_p2p_status(cfg, DISC_IN_PROGRESS)) {
		WL_ERR(("p2p listen offloading already running\n"));
		goto exit;
	}

	/* Just in case if it is not enabled */
	if ((ret = wl_cfgp2p_enable_discovery(cfg, dev, NULL, 0)) < 0) {
		WL_ERR(("cfgp2p_enable discovery failed"));
		goto exit;
	}

	bzero(&p2plo_listen, sizeof(wl_p2plo_listen_t));

	if (len) {
		sscanf(buf, " %10d %10d %10d %10d", &channel, &period, &interval, &count);
		if ((channel == 0) || (period == 0) ||
				(interval == 0) || (count == 0)) {
			WL_ERR(("Wrong argument %d/%d/%d/%d \n",
				channel, period, interval, count));
			ret = -EAGAIN;
			goto exit;
		}
		p2plo_listen.period = period;
		p2plo_listen.interval = interval;
		p2plo_listen.count = count;

		WL_ERR(("channel:%d period:%d, interval:%d count:%d\n",
			channel, period, interval, count));
	} else {
		WL_ERR(("Argument len is wrong.\n"));
		ret = -EAGAIN;
		goto exit;
	}

	if ((ret = wldev_iovar_setbuf_bsscfg(dev, "p2po_listen_channel", (void*)&channel,
			sizeof(channel), cfg->ioctl_buf, WLC_IOCTL_SMLEN,
			bssidx, &cfg->ioctl_buf_sync)) < 0) {
		WL_ERR(("p2po_listen_channel Failed :%d\n", ret));
		goto exit;
	}

	if ((ret = wldev_iovar_setbuf_bsscfg(dev, "p2po_listen", (void*)&p2plo_listen,
			sizeof(wl_p2plo_listen_t), cfg->ioctl_buf, WLC_IOCTL_SMLEN,
			bssidx, &cfg->ioctl_buf_sync)) < 0) {
		WL_ERR(("p2po_listen Failed :%d\n", ret));
		goto exit;
	}

	wl_set_p2p_status(cfg, DISC_IN_PROGRESS);
exit :
	return ret;
}

s32
wl_cfg80211_p2plo_listen_stop(struct net_device *dev)
{
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
	s32 bssidx = wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_DEVICE);
	int ret = -EAGAIN;

	if ((ret = wldev_iovar_setbuf_bsscfg(dev, "p2po_stop", NULL,
			0, cfg->ioctl_buf, WLC_IOCTL_SMLEN,
			bssidx, &cfg->ioctl_buf_sync)) < 0) {
		WL_ERR(("p2po_stop Failed :%d\n", ret));
		goto exit;
	}

exit:
	return ret;
}

s32
wl_cfg80211_p2plo_offload(struct net_device *dev, char *cmd, char* buf, int len)
{
	int ret = 0;

	WL_ERR(("Entry cmd:%s arg_len:%d \n", cmd, len));

	if (strncmp(cmd, "P2P_LO_START", strlen("P2P_LO_START")) == 0) {
		ret = wl_cfg80211_p2plo_listen_start(dev, buf, len);
	} else if (strncmp(cmd, "P2P_LO_STOP", strlen("P2P_LO_STOP")) == 0) {
		ret = wl_cfg80211_p2plo_listen_stop(dev);
	} else {
		WL_ERR(("Request for Unsupported CMD:%s \n", buf));
		ret = -EINVAL;
	}
	return ret;
}

void
wl_cfg80211_cancel_p2plo(struct bcm_cfg80211 *cfg)
{
	struct wireless_dev *wdev;
	if (!cfg) {
		return;
	}

	wdev = bcmcfg_to_p2p_wdev(cfg);

	if (wl_get_p2p_status(cfg, DISC_IN_PROGRESS)) {
		WL_INFORM_MEM(("P2P_FIND: Discovery offload is already in progress."
					"it aborted\n"));
		wl_clr_p2p_status(cfg, DISC_IN_PROGRESS);
		if (wdev != NULL) {
#if defined(WL_CFG80211_P2P_DEV_IF)
			cfg80211_remain_on_channel_expired(wdev,
					cfg->last_roc_id,
					&cfg->remain_on_chan, GFP_KERNEL);
#else
			cfg80211_remain_on_channel_expired(wdev,
					cfg->last_roc_id,
					&cfg->remain_on_chan,
					cfg->remain_on_chan_type, GFP_KERNEL);
#endif /* WL_CFG80211_P2P_DEV_IF */
		}
		wl_cfg80211_p2plo_deinit(cfg);
	}
}
#endif /* P2P_LISTEN_OFFLOADING */

#ifdef WL_MURX
int
wl_android_murx_bfe_cap(struct net_device *dev, int val)
{
	int err = BCME_OK;
	int iface_count = wl_cfg80211_iface_count(dev);
	struct ether_addr bssid;
	wl_reassoc_params_t params;

	if (iface_count > 1) {
		WL_ERR(("murx_bfe_cap change is not allowed when "
				"there are multiple interfaces\n"));
		return -EINVAL;
	}
	/* Now there is only single interface */
	err = wldev_iovar_setint(dev, "murx_bfe_cap", val);
	if (unlikely(err)) {
		WL_ERR(("Failed to set murx_bfe_cap IOVAR to %d,"
				"error %d\n", val, err));
		return err;
	}

	/* If successful intiate a reassoc */
	bzero(&bssid, ETHER_ADDR_LEN);
	if ((err = wldev_ioctl_get(dev, WLC_GET_BSSID, &bssid, ETHER_ADDR_LEN)) < 0) {
		WL_ERR(("Failed to get bssid, error=%d\n", err));
		return err;
	}

	bzero(&params, sizeof(wl_reassoc_params_t));
	memcpy(&params.bssid, &bssid, ETHER_ADDR_LEN);

	if ((err = wldev_ioctl_set(dev, WLC_REASSOC, &params,
		sizeof(wl_reassoc_params_t))) < 0) {
		WL_ERR(("reassoc failed err:%d \n", err));
	} else {
		WL_DBG(("reassoc issued successfully\n"));
	}

	return err;
}
#endif /* WL_MURX */

#ifdef SUPPORT_RSSI_SUM_REPORT
int
wl_android_get_rssi_per_ant(struct net_device *dev, char *command, int total_len)
{
	wl_rssi_ant_mimo_t rssi_ant_mimo;
	char *ifname = NULL;
	char *peer_mac = NULL;
	char *mimo_cmd = "mimo";
	char *pos, *token;
	int err = BCME_OK;
	int bytes_written = 0;
	bool mimo_rssi = FALSE;

	bzero(&rssi_ant_mimo, sizeof(wl_rssi_ant_mimo_t));
	/*
	 * STA I/F: DRIVER GET_RSSI_PER_ANT <ifname> <mimo>
	 * AP/GO I/F: DRIVER GET_RSSI_PER_ANT <ifname> <Peer MAC addr> <mimo>
	 */
	pos = command;

	/* drop command */
	token = bcmstrtok(&pos, " ", NULL);

	/* get the interface name */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token) {
		WL_ERR(("Invalid arguments\n"));
		return -EINVAL;
	}
	ifname = token;

	/* Optional: Check the MIMO RSSI mode or peer MAC address */
	token = bcmstrtok(&pos, " ", NULL);
	if (token) {
		/* Check the MIMO RSSI mode */
		if (strncmp(token, mimo_cmd, strlen(mimo_cmd)) == 0) {
			mimo_rssi = TRUE;
		} else {
			peer_mac = token;
		}
	}

	/* Optional: Check the MIMO RSSI mode - RSSI sum across antennas */
	token = bcmstrtok(&pos, " ", NULL);
	if (token && strncmp(token, mimo_cmd, strlen(mimo_cmd)) == 0) {
		mimo_rssi = TRUE;
	}

	err = wl_get_rssi_per_ant(dev, ifname, peer_mac, &rssi_ant_mimo);
	if (unlikely(err)) {
		WL_ERR(("Failed to get RSSI info, err=%d\n", err));
		return err;
	}

	/* Parse the results */
	WL_DBG(("ifname %s, version %d, count %d, mimo rssi %d\n",
		ifname, rssi_ant_mimo.version, rssi_ant_mimo.count, mimo_rssi));
	if (mimo_rssi) {
		WL_DBG(("MIMO RSSI: %d\n", rssi_ant_mimo.rssi_sum));
		bytes_written = snprintf(command, total_len, "%s MIMO %d",
			CMD_GET_RSSI_PER_ANT, rssi_ant_mimo.rssi_sum);
	} else {
		int cnt;
		bytes_written = snprintf(command, total_len, "%s PER_ANT ", CMD_GET_RSSI_PER_ANT);
		for (cnt = 0; cnt < rssi_ant_mimo.count; cnt++) {
			WL_DBG(("RSSI[%d]: %d\n", cnt, rssi_ant_mimo.rssi_ant[cnt]));
			bytes_written = snprintf(command, total_len, "%d ",
				rssi_ant_mimo.rssi_ant[cnt]);
		}
	}

	return bytes_written;
}

int
wl_android_set_rssi_logging(struct net_device *dev, char *command, int total_len)
{
	rssilog_set_param_t set_param;
	char *pos, *token;
	int err = BCME_OK;

	bzero(&set_param, sizeof(rssilog_set_param_t));
	/*
	 * DRIVER SET_RSSI_LOGGING <enable/disable> <RSSI Threshold> <Time Threshold>
	 */
	pos = command;

	/* drop command */
	token = bcmstrtok(&pos, " ", NULL);

	/* enable/disable */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token) {
		WL_ERR(("Invalid arguments\n"));
		return -EINVAL;
	}
	set_param.enable = bcm_atoi(token);

	/* RSSI Threshold */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token) {
		WL_ERR(("Invalid arguments\n"));
		return -EINVAL;
	}
	set_param.rssi_threshold = bcm_atoi(token);

	/* Time Threshold */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token) {
		WL_ERR(("Invalid arguments\n"));
		return -EINVAL;
	}
	set_param.time_threshold = bcm_atoi(token);

	WL_DBG(("enable %d, RSSI threshold %d, Time threshold %d\n", set_param.enable,
		set_param.rssi_threshold, set_param.time_threshold));

	err = wl_set_rssi_logging(dev, (void *)&set_param);
	if (unlikely(err)) {
		WL_ERR(("Failed to configure RSSI logging: enable %d, RSSI Threshold %d,"
			" Time Threshold %d\n", set_param.enable, set_param.rssi_threshold,
			set_param.time_threshold));
	}

	return err;
}

int
wl_android_get_rssi_logging(struct net_device *dev, char *command, int total_len)
{
	rssilog_get_param_t get_param;
	int err = BCME_OK;
	int bytes_written = 0;

	err = wl_get_rssi_logging(dev, (void *)&get_param);
	if (unlikely(err)) {
		WL_ERR(("Failed to get RSSI logging info\n"));
		return BCME_ERROR;
	}

	WL_DBG(("report_count %d, enable %d, rssi_threshold %d, time_threshold %d\n",
		get_param.report_count, get_param.enable, get_param.rssi_threshold,
		get_param.time_threshold));

	/* Parse the parameter */
	if (!get_param.enable) {
		WL_DBG(("RSSI LOGGING: Feature is disables\n"));
		bytes_written = snprintf(command, total_len,
			"%s FEATURE DISABLED\n", CMD_GET_RSSI_LOGGING);
	} else if (get_param.enable &
		(RSSILOG_FLAG_FEATURE_SW | RSSILOG_FLAG_REPORT_READY)) {
		if (!get_param.report_count) {
			WL_DBG(("[PASS] RSSI difference across antennas is within"
				" threshold limits\n"));
			bytes_written = snprintf(command, total_len, "%s PASS\n",
				CMD_GET_RSSI_LOGGING);
		} else {
			WL_DBG(("[FAIL] RSSI difference across antennas found "
				"to be greater than %3d dB\n", get_param.rssi_threshold));
			WL_DBG(("[FAIL] RSSI difference check have failed for "
				"%d out of %d times\n", get_param.report_count,
				get_param.time_threshold));
			WL_DBG(("[FAIL] RSSI difference is being monitored once "
				"per second, for a %d secs window\n", get_param.time_threshold));
			bytes_written = snprintf(command, total_len, "%s FAIL - RSSI Threshold "
				"%d dBm for %d out of %d times\n", CMD_GET_RSSI_LOGGING,
				get_param.rssi_threshold, get_param.report_count,
				get_param.time_threshold);
		}
	} else {
		WL_DBG(("[BUSY] Reprot is not ready\n"));
		bytes_written = snprintf(command, total_len, "%s BUSY - NOT READY\n",
			CMD_GET_RSSI_LOGGING);
	}

	return bytes_written;
}
#endif /* SUPPORT_RSSI_SUM_REPORT */

#ifdef SET_PCIE_IRQ_CPU_CORE
void
wl_android_set_irq_cpucore(struct net_device *net, int affinity_cmd)
{
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(net);
	if (!dhdp) {
		WL_ERR(("dhd is NULL\n"));
		return;
	}

	dhd_set_irq_cpucore(dhdp, affinity_cmd);
}
#endif /* SET_PCIE_IRQ_CPU_CORE */

#ifdef SUPPORT_LQCM
static int
wl_android_lqcm_enable(struct net_device *net, int lqcm_enable)
{
	int err = 0;

	err = wldev_iovar_setint(net, "lqcm", lqcm_enable);
	if (err != BCME_OK) {
		WL_ERR(("failed to set lqcm enable %d, error = %d\n", lqcm_enable, err));
		return -EIO;
	}
	return err;
}

static int
wl_android_get_lqcm_report(struct net_device *dev, char *command, int total_len)
{
	int bytes_written, err = 0;
	uint32 lqcm_report = 0;
	uint32 lqcm_enable, tx_lqcm_idx, rx_lqcm_idx;

	err = wldev_iovar_getint(dev, "lqcm", &lqcm_report);
	if (err != BCME_OK) {
		WL_ERR(("failed to get lqcm report, error = %d\n", err));
		return -EIO;
	}
	lqcm_enable = lqcm_report & LQCM_ENAB_MASK;
	tx_lqcm_idx = (lqcm_report & LQCM_TX_INDEX_MASK) >> LQCM_TX_INDEX_SHIFT;
	rx_lqcm_idx = (lqcm_report & LQCM_RX_INDEX_MASK) >> LQCM_RX_INDEX_SHIFT;

	WL_DBG(("lqcm report EN:%d, TX:%d, RX:%d\n", lqcm_enable, tx_lqcm_idx, rx_lqcm_idx));

	bytes_written = snprintf(command, total_len, "%s %d",
			CMD_GET_LQCM_REPORT, lqcm_report);

#ifdef RPM_FAST_TRIGGER
	WL_INFORM(("Trgger RPM Fast\n"));
	dhd_trigger_rpm_fast(wl_get_cfg(dev));
#endif /* RPM_FAST_TRIGGER */

	return bytes_written;
}
#endif /* SUPPORT_LQCM */

int
wl_android_get_snr(struct net_device *dev, char *command, int total_len)
{
	int bytes_written, error = 0;
	s32 snr = 0;

	error = wldev_iovar_getint(dev, "snr", &snr);
	if (error) {
		DHD_ERROR(("wl_android_get_snr: Failed to get SNR %d, error = %d\n",
			snr, error));
		return -EIO;
	}

	bytes_written = snprintf(command, total_len, "snr %d", snr);
	DHD_INFO(("wl_android_get_snr: command result is %s\n", command));
	return bytes_written;
}

#ifdef SUPPORT_AP_HIGHER_BEACONRATE
int
wl_android_set_ap_beaconrate(struct net_device *dev, char *command)
{
	int rate = 0;
	char *pos, *token;
	char *ifname = NULL;
	int err = BCME_OK;

	/*
	 * DRIVER SET_AP_BEACONRATE <rate> <ifname>
	 */
	pos = command;

	/* drop command */
	token = bcmstrtok(&pos, " ", NULL);

	/* Rate */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token)
		return -EINVAL;
	rate = bcm_atoi(token);

	/* get the interface name */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token)
		return -EINVAL;
	ifname = token;

	WL_DBG(("rate %d, ifacename %s\n", rate, ifname));

	err = wl_set_ap_beacon_rate(dev, rate, ifname);
	if (unlikely(err)) {
		WL_ERR(("Failed to set ap beacon rate to %d, error = %d\n", rate, err));
	}

	return err;
}

int wl_android_get_ap_basicrate(struct net_device *dev, char *command, int total_len)
{
	char *pos, *token;
	char *ifname = NULL;
	int bytes_written = 0;
	/*
	 * DRIVER GET_AP_BASICRATE <ifname>
	 */
	pos = command;

	/* drop command */
	token = bcmstrtok(&pos, " ", NULL);

	/* get the interface name */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token)
		return -EINVAL;
	ifname = token;

	WL_DBG(("ifacename %s\n", ifname));

	bytes_written = wl_get_ap_basic_rate(dev, command, ifname, total_len);
	if (bytes_written < 1) {
		WL_ERR(("Failed to get ap basic rate, error = %d\n", bytes_written));
		return -EPROTO;
	}

	return bytes_written;
}
#endif /* SUPPORT_AP_HIGHER_BEACONRATE */

#ifdef SUPPORT_AP_RADIO_PWRSAVE
int
wl_android_get_ap_rps(struct net_device *dev, char *command, int total_len)
{
	char *pos, *token;
	char *ifname = NULL;
	int bytes_written = 0;
	char name[IFNAMSIZ];
	/*
	 * DRIVER GET_AP_RPS <ifname>
	 */
	pos = command;

	/* drop command */
	token = bcmstrtok(&pos, " ", NULL);

	/* get the interface name */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token)
		return -EINVAL;
	ifname = token;

	strlcpy(name, ifname, sizeof(name));
	WL_DBG(("ifacename %s\n", name));

	bytes_written = wl_get_ap_rps(dev, command, name, total_len);
	if (bytes_written < 1) {
		WL_ERR(("Failed to get rps, error = %d\n", bytes_written));
		return -EPROTO;
	}

	return bytes_written;

}

int
wl_android_set_ap_rps(struct net_device *dev, char *command, int total_len)
{
	int enable = 0;
	char *pos, *token;
	char *ifname = NULL;
	int err = BCME_OK;
	char name[IFNAMSIZ];

	/*
	 * DRIVER SET_AP_RPS <0/1> <ifname>
	 */
	pos = command;

	/* drop command */
	token = bcmstrtok(&pos, " ", NULL);

	/* Enable */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token)
		return -EINVAL;
	enable = bcm_atoi(token);

	/* get the interface name */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token)
		return -EINVAL;
	ifname = token;

	strlcpy(name, ifname, sizeof(name));
	WL_DBG(("enable %d, ifacename %s\n", enable, name));

	err = wl_set_ap_rps(dev, enable? TRUE: FALSE, name);
	if (unlikely(err)) {
		WL_ERR(("Failed to set rps, enable %d, error = %d\n", enable, err));
	}

	return err;
}

int
wl_android_set_ap_rps_params(struct net_device *dev, char *command, int total_len)
{
	ap_rps_info_t rps;
	char *pos, *token;
	char *ifname = NULL;
	int err = BCME_OK;
	char name[IFNAMSIZ];

	bzero(&rps, sizeof(rps));
	/*
	 * DRIVER SET_AP_RPS_PARAMS <pps> <level> <quiettime> <assoccheck> <ifname>
	 */
	pos = command;

	/* drop command */
	token = bcmstrtok(&pos, " ", NULL);

	/* pps */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token)
		return -EINVAL;
	rps.pps = bcm_atoi(token);

	/* level */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token)
		return -EINVAL;
	rps.level = bcm_atoi(token);

	/* quiettime */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token)
		return -EINVAL;
	rps.quiet_time = bcm_atoi(token);

	/* sta assoc check */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token)
		return -EINVAL;
	rps.sta_assoc_check = bcm_atoi(token);

	/* get the interface name */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token)
		return -EINVAL;
	ifname = token;
	strlcpy(name, ifname, sizeof(name));

	WL_DBG(("pps %d, level %d, quiettime %d, sta_assoc_check %d, "
		"ifacename %s\n", rps.pps, rps.level, rps.quiet_time,
		rps.sta_assoc_check, name));

	err = wl_update_ap_rps_params(dev, &rps, name);
	if (unlikely(err)) {
		WL_ERR(("Failed to update rps, pps %d, level %d, quiettime %d, "
			"sta_assoc_check %d, err = %d\n", rps.pps, rps.level, rps.quiet_time,
			rps.sta_assoc_check, err));
	}

	return err;
}
#endif /* SUPPORT_AP_RADIO_PWRSAVE */

#if defined(DHD_HANG_SEND_UP_TEST)
void
wl_android_make_hang_with_reason(struct net_device *dev, const char *string_num)
{
	dhd_make_hang_with_reason(dev, string_num);
}
#endif /* DHD_HANG_SEND_UP_TEST */

#ifdef DHD_SEND_HANG_PRIVCMD_ERRORS
static void
wl_android_check_priv_cmd_errors(struct net_device *dev)
{
	dhd_pub_t *dhdp;
	int memdump_mode;

	if (!dev) {
		WL_ERR(("dev is NULL\n"));
		return;
	}

	dhdp = wl_cfg80211_get_dhdp(dev);
	if (!dhdp) {
		WL_ERR(("dhdp is NULL\n"));
		return;
	}

#ifdef DHD_FW_COREDUMP
	memdump_mode = dhdp->memdump_enabled;
#else
	/* Default enable if DHD doesn't support SOCRAM dump */
	memdump_mode = 1;
#endif /* DHD_FW_COREDUMP */

	if (report_hang_privcmd_err) {
		priv_cmd_errors++;
	} else {
		priv_cmd_errors = 0;
	}

	/* Trigger HANG event only if memdump mode is enabled
	 * due to customer's request
	 */
	if (memdump_mode == DUMP_MEMFILE_BUGON &&
		(priv_cmd_errors > NUMBER_SEQUENTIAL_PRIVCMD_ERRORS)) {
		WL_ERR(("Send HANG event due to sequential private cmd errors\n"));
		priv_cmd_errors = 0;
#ifdef DHD_FW_COREDUMP
		/* Take a SOCRAM dump */
		dhdp->memdump_type = DUMP_TYPE_SEQUENTIAL_PRIVCMD_ERROR;
		dhd_common_socram_dump(dhdp);
#endif /* DHD_FW_COREDUMP */
		/* Send the HANG event to upper layer */
		dhdp->hang_reason = HANG_REASON_SEQUENTIAL_PRIVCMD_ERROR;
		dhd_os_check_hang(dhdp, 0, -EREMOTEIO);
	}
}
#endif /* DHD_SEND_HANG_PRIVCMD_ERRORS */

#ifdef DHD_PKT_LOGGING
static int
wl_android_pktlog_filter_enable(struct net_device *dev, char *command, int total_len)
{
	int bytes_written = 0;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(dev);
	dhd_pktlog_filter_t *filter;
	int err = BCME_OK;

	if (!dhdp || !dhdp->pktlog) {
		DHD_PKT_LOG(("%s(): dhdp=%p pktlog=%p\n",
			__FUNCTION__, dhdp, (dhdp ? dhdp->pktlog : NULL)));
		return -EINVAL;
	}

	filter = dhdp->pktlog->pktlog_filter;

	err = dhd_pktlog_filter_enable(filter, PKTLOG_TXPKT_CASE, TRUE);
	err = dhd_pktlog_filter_enable(filter, PKTLOG_TXSTATUS_CASE, TRUE);
	err = dhd_pktlog_filter_enable(filter, PKTLOG_RXPKT_CASE, TRUE);

	if (err == BCME_OK) {
		bytes_written = snprintf(command, total_len, "OK");
		DHD_ERROR(("%s: pktlog filter enable success\n", __FUNCTION__));
	} else {
		DHD_ERROR(("%s: pktlog filter enable fail\n", __FUNCTION__));
		return BCME_ERROR;
	}

	return bytes_written;
}

static int
wl_android_pktlog_filter_disable(struct net_device *dev, char *command, int total_len)
{
	int bytes_written = 0;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(dev);
	dhd_pktlog_filter_t *filter;
	int err = BCME_OK;

	if (!dhdp || !dhdp->pktlog) {
		DHD_PKT_LOG(("%s(): dhdp=%p pktlog=%p\n",
			__FUNCTION__, dhdp, (dhdp ? dhdp->pktlog : NULL)));
		return -EINVAL;
	}

	filter = dhdp->pktlog->pktlog_filter;

	err = dhd_pktlog_filter_enable(filter, PKTLOG_TXPKT_CASE, FALSE);
	err = dhd_pktlog_filter_enable(filter, PKTLOG_TXSTATUS_CASE, FALSE);
	err = dhd_pktlog_filter_enable(filter, PKTLOG_RXPKT_CASE, FALSE);

	if (err == BCME_OK) {
		bytes_written = snprintf(command, total_len, "OK");
		DHD_ERROR(("%s: pktlog filter disable success\n", __FUNCTION__));
	} else {
		DHD_ERROR(("%s: pktlog filter disable fail\n", __FUNCTION__));
		return BCME_ERROR;
	}

	return bytes_written;
}

static int
wl_android_pktlog_filter_pattern_enable(struct net_device *dev, char *command, int total_len)
{
	int bytes_written = 0;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(dev);
	dhd_pktlog_filter_t *filter;
	int err = BCME_OK;

	if (!dhdp || !dhdp->pktlog) {
		DHD_PKT_LOG(("%s(): dhdp=%p pktlog=%p\n",
			__FUNCTION__, dhdp, (dhdp ? dhdp->pktlog : NULL)));
		return -EINVAL;
	}

	filter = dhdp->pktlog->pktlog_filter;

	if (strlen(CMD_PKTLOG_FILTER_PATTERN_ENABLE) + 1 > total_len) {
		return BCME_ERROR;
	}

	err = dhd_pktlog_filter_pattern_enable(filter,
			command + strlen(CMD_PKTLOG_FILTER_PATTERN_ENABLE) + 1, TRUE);

	if (err == BCME_OK) {
		bytes_written = snprintf(command, total_len, "OK");
		DHD_ERROR(("%s: pktlog filter pattern enable success\n", __FUNCTION__));
	} else {
		DHD_ERROR(("%s: pktlog filter pattern enable fail\n", __FUNCTION__));
		return BCME_ERROR;
	}

	return bytes_written;
}

static int
wl_android_pktlog_filter_pattern_disable(struct net_device *dev, char *command, int total_len)
{
	int bytes_written = 0;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(dev);
	dhd_pktlog_filter_t *filter;
	int err = BCME_OK;

	if (!dhdp || !dhdp->pktlog) {
		DHD_PKT_LOG(("%s(): dhdp=%p pktlog=%p\n",
			__FUNCTION__, dhdp, (dhdp ? dhdp->pktlog : NULL)));
		return -EINVAL;
	}

	filter = dhdp->pktlog->pktlog_filter;

	if (strlen(CMD_PKTLOG_FILTER_PATTERN_DISABLE) + 1 > total_len) {
		return BCME_ERROR;
	}

	err = dhd_pktlog_filter_pattern_enable(filter,
			command + strlen(CMD_PKTLOG_FILTER_PATTERN_DISABLE) + 1, FALSE);

	if (err == BCME_OK) {
		bytes_written = snprintf(command, total_len, "OK");
		DHD_ERROR(("%s: pktlog filter pattern disable success\n", __FUNCTION__));
	} else {
		DHD_ERROR(("%s: pktlog filter pattern disable fail\n", __FUNCTION__));
		return BCME_ERROR;
	}

	return bytes_written;
}

static int
wl_android_pktlog_filter_add(struct net_device *dev, char *command, int total_len)
{
	int bytes_written = 0;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(dev);
	dhd_pktlog_filter_t *filter;
	int err = BCME_OK;

	if (!dhdp || !dhdp->pktlog) {
		DHD_PKT_LOG(("%s(): dhdp=%p pktlog=%p\n",
			__FUNCTION__, dhdp, (dhdp ? dhdp->pktlog : NULL)));
		return -EINVAL;
	}

	filter = dhdp->pktlog->pktlog_filter;

	if (strlen(CMD_PKTLOG_FILTER_ADD) + 1 > total_len) {
		return BCME_ERROR;
	}

	err = dhd_pktlog_filter_add(filter, command + strlen(CMD_PKTLOG_FILTER_ADD) + 1);

	if (err == BCME_OK) {
		bytes_written = snprintf(command, total_len, "OK");
		DHD_ERROR(("%s: pktlog filter add success\n", __FUNCTION__));
	} else {
		DHD_ERROR(("%s: pktlog filter add fail\n", __FUNCTION__));
		return BCME_ERROR;
	}

	return bytes_written;
}

static int
wl_android_pktlog_filter_del(struct net_device *dev, char *command, int total_len)
{
	int bytes_written = 0;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(dev);
	dhd_pktlog_filter_t *filter;
	int err = BCME_OK;

	if (!dhdp || !dhdp->pktlog) {
		DHD_ERROR(("%s(): dhdp=%p pktlog=%p\n",
			__FUNCTION__, dhdp, (dhdp ? dhdp->pktlog : NULL)));
		return -EINVAL;
	}

	filter = dhdp->pktlog->pktlog_filter;

	if (strlen(CMD_PKTLOG_FILTER_DEL) + 1 > total_len) {
		DHD_PKT_LOG(("%s(): wrong cmd length %d found\n",
			__FUNCTION__, (int)strlen(CMD_PKTLOG_FILTER_DEL)));
		return BCME_ERROR;
	}

	err = dhd_pktlog_filter_del(filter, command + strlen(CMD_PKTLOG_FILTER_DEL) + 1);
	if (err == BCME_OK) {
		bytes_written = snprintf(command, total_len, "OK");
		DHD_ERROR(("%s: pktlog filter del success\n", __FUNCTION__));
	} else {
		DHD_ERROR(("%s: pktlog filter del fail\n", __FUNCTION__));
		return BCME_ERROR;
	}

	return bytes_written;
}

static int
wl_android_pktlog_filter_info(struct net_device *dev, char *command, int total_len)
{
	int bytes_written = 0;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(dev);
	dhd_pktlog_filter_t *filter;
	int err = BCME_OK;

	if (!dhdp || !dhdp->pktlog) {
		DHD_PKT_LOG(("%s(): dhdp=%p pktlog=%p\n",
			__FUNCTION__, dhdp, (dhdp ? dhdp->pktlog : NULL)));
		return -EINVAL;
	}

	filter = dhdp->pktlog->pktlog_filter;

	err = dhd_pktlog_filter_info(filter);

	if (err == BCME_OK) {
		bytes_written = snprintf(command, total_len, "OK");
		DHD_ERROR(("%s: pktlog filter info success\n", __FUNCTION__));
	} else {
		DHD_ERROR(("%s: pktlog filter info fail\n", __FUNCTION__));
		return BCME_ERROR;
	}

	return bytes_written;
}

static int
wl_android_pktlog_start(struct net_device *dev, char *command, int total_len)
{
	int bytes_written = 0;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(dev);

	if (!dhdp || !dhdp->pktlog) {
		DHD_PKT_LOG(("%s(): dhdp=%p pktlog=%p\n",
			__FUNCTION__, dhdp, (dhdp ? dhdp->pktlog : NULL)));
		return -EINVAL;
	}

	if (!dhdp->pktlog->pktlog_ring) {
		DHD_PKT_LOG(("%s(): pktlog_ring=%p\n",
			__FUNCTION__, dhdp->pktlog->pktlog_ring));
		return -EINVAL;
	}

	atomic_set(&dhdp->pktlog->pktlog_ring->start, TRUE);

	bytes_written = snprintf(command, total_len, "OK");

	DHD_ERROR(("%s: pktlog start success\n", __FUNCTION__));

	return bytes_written;
}

static int
wl_android_pktlog_stop(struct net_device *dev, char *command, int total_len)
{
	int bytes_written = 0;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(dev);

	if (!dhdp || !dhdp->pktlog) {
		DHD_PKT_LOG(("%s(): dhdp=%p pktlog=%p\n",
			__FUNCTION__, dhdp, (dhdp ? dhdp->pktlog : NULL)));
		return -EINVAL;
	}

	if (!dhdp->pktlog->pktlog_ring) {
		DHD_PKT_LOG(("%s(): _pktlog_ring=%p\n",
			__FUNCTION__, dhdp->pktlog->pktlog_ring));
		return -EINVAL;
	}

	atomic_set(&dhdp->pktlog->pktlog_ring->start, FALSE);

	bytes_written = snprintf(command, total_len, "OK");

	DHD_ERROR(("%s: pktlog stop success\n", __FUNCTION__));

	return bytes_written;
}

static int
wl_android_pktlog_filter_exist(struct net_device *dev, char *command, int total_len)
{
	int bytes_written = 0;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(dev);
	dhd_pktlog_filter_t *filter;
	uint32 id;
	bool exist = FALSE;

	if (!dhdp || !dhdp->pktlog) {
		DHD_PKT_LOG(("%s(): dhdp=%p pktlog=%p\n",
			__FUNCTION__, dhdp, (dhdp ? dhdp->pktlog : NULL)));
		return -EINVAL;
	}

	filter = dhdp->pktlog->pktlog_filter;

	if (strlen(CMD_PKTLOG_FILTER_EXIST) + 1 > total_len) {
		return BCME_ERROR;
	}

	exist = dhd_pktlog_filter_existed(filter, command + strlen(CMD_PKTLOG_FILTER_EXIST) + 1,
			&id);

	if (exist) {
		bytes_written = snprintf(command, total_len, "TRUE");
		DHD_ERROR(("%s: pktlog filter pattern id: %d is existed\n", __FUNCTION__, id));
	} else {
		bytes_written = snprintf(command, total_len, "FALSE");
		DHD_ERROR(("%s: pktlog filter pattern id: %d is not existed\n", __FUNCTION__, id));
	}

	return bytes_written;
}

static int
wl_android_pktlog_minmize_enable(struct net_device *dev, char *command, int total_len)
{
	int bytes_written = 0;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(dev);

	if (!dhdp || !dhdp->pktlog) {
		DHD_PKT_LOG(("%s(): dhdp=%p pktlog=%p\n",
			__FUNCTION__, dhdp, (dhdp ? dhdp->pktlog : NULL)));
		return -EINVAL;
	}

	if (!dhdp->pktlog->pktlog_ring) {
		DHD_PKT_LOG(("%s(): pktlog_ring=%p\n",
			__FUNCTION__, dhdp->pktlog->pktlog_ring));
		return -EINVAL;
	}

	dhdp->pktlog->pktlog_ring->pktlog_minmize = TRUE;

	bytes_written = snprintf(command, total_len, "OK");

	DHD_ERROR(("%s: pktlog pktlog_minmize enable\n", __FUNCTION__));

	return bytes_written;
}

static int
wl_android_pktlog_minmize_disable(struct net_device *dev, char *command, int total_len)
{
	int bytes_written = 0;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(dev);

	if (!dhdp || !dhdp->pktlog) {
		DHD_PKT_LOG(("%s(): dhdp=%p pktlog=%p\n",
			__FUNCTION__, dhdp, (dhdp ? dhdp->pktlog : NULL)));
		return -EINVAL;
	}

	if (!dhdp->pktlog->pktlog_ring) {
		DHD_PKT_LOG(("%s(): pktlog_ring=%p\n",
			__FUNCTION__, dhdp->pktlog->pktlog_ring));
		return -EINVAL;
	}

	dhdp->pktlog->pktlog_ring->pktlog_minmize = FALSE;

	bytes_written = snprintf(command, total_len, "OK");

	DHD_ERROR(("%s: pktlog pktlog_minmize disable\n", __FUNCTION__));

	return bytes_written;
}

static int
wl_android_pktlog_change_size(struct net_device *dev, char *command, int total_len)
{
	int bytes_written = 0;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(dev);
	int err = BCME_OK;
	int size;

	if (!dhdp || !dhdp->pktlog) {
		DHD_PKT_LOG(("%s(): dhdp=%p pktlog=%p\n",
			__FUNCTION__, dhdp, (dhdp ? dhdp->pktlog : NULL)));
		return -EINVAL;
	}

	if (strlen(CMD_PKTLOG_CHANGE_SIZE) + 1 > total_len) {
		return BCME_ERROR;
	}

	size = bcm_strtoul(command + strlen(CMD_PKTLOG_CHANGE_SIZE) + 1, NULL, 0);

	dhdp->pktlog->pktlog_ring =
		dhd_pktlog_ring_change_size(dhdp->pktlog->pktlog_ring, size);
	if (!dhdp->pktlog->pktlog_ring) {
		err = BCME_ERROR;
	}

	if (err == BCME_OK) {
		bytes_written = snprintf(command, total_len, "OK");
		DHD_ERROR(("%s: pktlog change size success\n", __FUNCTION__));
	} else {
		DHD_ERROR(("%s: pktlog change size fail\n", __FUNCTION__));
		return BCME_ERROR;
	}

	return bytes_written;
}

static int
wl_android_pktlog_dbg_dump(struct net_device *dev, char *command, int total_len)
{
	int bytes_written = 0;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(dev);
	int err = BCME_OK;

	if (!dhdp || !dhdp->pktlog) {
		DHD_PKT_LOG(("%s(): dhdp=%p pktlog=%p\n",
			__FUNCTION__, dhdp, (dhdp ? dhdp->pktlog : NULL)));
		return -EINVAL;
	}

	if (strlen(CMD_PKTLOG_DEBUG_DUMP) + 1 > total_len) {
		return BCME_ERROR;
	}

	err = dhd_pktlog_debug_dump(dhdp);
	if (err == BCME_OK) {
		bytes_written = snprintf(command, total_len, "OK");
		DHD_INFO(("%s: pktlog dbg dump success\n", __FUNCTION__));
	} else {
		DHD_ERROR(("%s: pktlog dbg dump fail\n", __FUNCTION__));
		return BCME_ERROR;
	}

	return bytes_written;
}
#endif /* DHD_PKT_LOGGING */

#if defined(CONFIG_TIZEN)
static int wl_android_set_powersave_mode(
	struct net_device *dev, char* command, int total_len)
{
	int pm;

	int err = BCME_OK;
#ifdef DHD_PM_OVERRIDE
	extern bool g_pm_override;
#endif /* DHD_PM_OVERRIDE */
	sscanf(command, "%*s %10d", &pm);
	if (pm < PM_OFF || pm > PM_FAST) {
		WL_ERR(("check pm=%d\n", pm));
		return BCME_ERROR;
	}

#ifdef DHD_PM_OVERRIDE
	if (pm > PM_OFF) {
		g_pm_override = FALSE;
	}
#endif /* DHD_PM_OVERRIDE */

	err =  wldev_ioctl_set(dev, WLC_SET_PM, &pm, sizeof(pm));

#ifdef DHD_PM_OVERRIDE
	if (pm == PM_OFF) {
		g_pm_override = TRUE;
	}

	WL_ERR(("%s: PM:%d, pm_override=%d\n", __FUNCTION__, pm, g_pm_override));
#endif /* DHD_PM_OVERRIDE */
	return err;
}

static int wl_android_get_powersave_mode(
	struct net_device *dev, char *command, int total_len)
{
	int err, bytes_written;
	int pm;

	err = wldev_ioctl_get(dev, WLC_GET_PM, &pm, sizeof(pm));
	if (err != BCME_OK) {
		WL_ERR(("failed to get pm (%d)", err));
		return err;
	}

	bytes_written = snprintf(command, total_len, "%s %d",
		CMD_POWERSAVEMODE_GET, pm);

	return bytes_written;
}
#endif /* CONFIG_TIZEN */

#ifdef DHD_EVENT_LOG_FILTER
uint32 dhd_event_log_filter_serialize(dhd_pub_t *dhdp, char *buf, uint32 tot_len, int type);

#ifdef DHD_EWPR_VER2
uint32 dhd_event_log_filter_serialize_bit(dhd_pub_t *dhdp, char *buf, uint32 tot_len,
	int index1, int index2, int index3);
#endif

static int
wl_android_ewp_filter(struct net_device *dev, char *command, uint32 tot_len)
{
	uint32 bytes_written = 0;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(dev);
#ifdef DHD_EWPR_VER2
	int index1 = 0, index2 = 0, index3 = 0;
	unsigned char *index_str = (unsigned char *)(command +
		strlen(CMD_EWP_FILTER) + 1);
#else
	int type = 0;
#endif

	if (!dhdp || !command) {
		DHD_ERROR(("%s(): dhdp=%p \n", __FUNCTION__, dhdp));
		return -EINVAL;
	}

	if (!FW_SUPPORTED(dhdp, ecounters)) {
		DHD_ERROR(("does not support ecounters!\n"));
		return BCME_UNSUPPORTED;
	}

#ifdef DHD_EWPR_VER2
	if (strlen(command) > strlen(CMD_EWP_FILTER) + 1) {
		sscanf(index_str, "%10d %10d %10d", &index1, &index2, &index3);
		DHD_TRACE(("%s(): get index request: %d %d %d\n", __FUNCTION__,
			index1, index2, index3));
	}
	bytes_written += dhd_event_log_filter_serialize_bit(dhdp,
		&command[bytes_written], tot_len - bytes_written, index1, index2, index3);
#else
	/* NEED TO GET TYPE if EXIST */
	type = 0;

	bytes_written += dhd_event_log_filter_serialize(dhdp,
		&command[bytes_written], tot_len - bytes_written, type);
#endif

	return (int)bytes_written;
}
#endif /* DHD_EVENT_LOG_FILTER */

#ifdef SUPPORT_AP_SUSPEND
int
wl_android_set_ap_suspend(struct net_device *dev, char *command, int total_len)
{
	int suspend = 0;
	char *pos, *token;
	char *ifname = NULL;
	int err = BCME_OK;
	char name[IFNAMSIZ];

	/*
	 * DRIVER SET_AP_SUSPEND <0/1> <ifname>
	 */
	pos = command;

	/* drop command */
	token = bcmstrtok(&pos, " ", NULL);

	/* Enable */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token) {
		return -EINVAL;
	}
	suspend = bcm_atoi(token);

	/* get the interface name */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token) {
		return -EINVAL;
	}
	ifname = token;

	strlcpy(name, ifname, sizeof(name));
	WL_DBG(("suspend %d, ifacename %s\n", suspend, name));

	err = wl_set_ap_suspend(dev, suspend? TRUE: FALSE, name);
	if (unlikely(err)) {
		WL_ERR(("Failed to set suspend, suspend %d, error = %d\n", suspend, err));
	}

	return err;
}
#endif /* SUPPORT_AP_SUSPEND */

#ifdef SUPPORT_AP_BWCTRL
int
wl_android_set_ap_bw(struct net_device *dev, char *command, int total_len)
{
	int bw = DOT11_OPER_MODE_20MHZ;
	char *pos, *token;
	char *ifname = NULL;
	int err = BCME_OK;
	char name[IFNAMSIZ];

	/*
	 * DRIVER SET_AP_BW <0/1/2> <ifname>
	 * 0 : 20MHz, 1 : 40MHz, 2 : 80MHz 3: 80+80 or 160MHz
	 * This is from operating mode field
	 * in 8.4.1.50 of 802.11ac-2013
	 */
	pos = command;

	/* drop command */
	token = bcmstrtok(&pos, " ", NULL);

	/* BW */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token) {
		return -EINVAL;
	}
	bw = bcm_atoi(token);

	/* get the interface name */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token) {
		return -EINVAL;
	}
	ifname = token;

	strlcpy(name, ifname, sizeof(name));
	WL_DBG(("bw %d, ifacename %s\n", bw, name));

	err = wl_set_ap_bw(dev, bw, name);
	if (unlikely(err)) {
		WL_ERR(("Failed to set bw, bw %d, error = %d\n", bw, err));
	}

	return err;
}

int
wl_android_get_ap_bw(struct net_device *dev, char *command, int total_len)
{
	char *pos, *token;
	char *ifname = NULL;
	int bytes_written = 0;
	char name[IFNAMSIZ];

	/*
	 * DRIVER GET_AP_BW <ifname>
	 * returns 0 : 20MHz, 1 : 40MHz, 2 : 80MHz 3: 80+80 or 160MHz
	 * This is from operating mode field
	 * in 8.4.1.50 of 802.11ac-2013
	 */
	pos = command;

	/* drop command */
	token = bcmstrtok(&pos, " ", NULL);

	/* get the interface name */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token) {
		return -EINVAL;
	}
	ifname = token;

	strlcpy(name, ifname, sizeof(name));
	WL_DBG(("ifacename %s\n", name));

	bytes_written = wl_get_ap_bw(dev, command, name, total_len);
	if (bytes_written < 1) {
		WL_ERR(("Failed to get bw, error = %d\n", bytes_written));
		return -EPROTO;
	}

	return bytes_written;

}
#endif /* SUPPORT_AP_BWCTRL */

static int
wl_android_priv_cmd_log_enable_check(char* cmd)
{
	int cnt = 0;

	while (strlen(loging_params[cnt].command) > 0) {
		if (!strnicmp(cmd, loging_params[cnt].command,
			strlen(loging_params[cnt].command))) {
			return loging_params[cnt].enable;
		}

		cnt++;
	}

	return TRUE;
}

int wl_android_priv_cmd(struct net_device *net, struct ifreq *ifr)
{
#define PRIVATE_COMMAND_MAX_LEN	8192
#define PRIVATE_COMMAND_DEF_LEN	4096
	int ret = 0;
	char *command = NULL;
	int bytes_written = 0;
	android_wifi_priv_cmd priv_cmd;
	int buf_size = 0;
	struct bcm_cfg80211 *cfg = wl_get_cfg(net);

	net_os_wake_lock(net);

	if (!capable(CAP_NET_ADMIN)) {
		ret = -EPERM;
		goto exit;
	}

	if (!ifr->ifr_data) {
		ret = -EINVAL;
		goto exit;
	}

#ifdef CONFIG_COMPAT
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0))
	if (in_compat_syscall())
#else
	if (is_compat_task())
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0) */
	{
		compat_android_wifi_priv_cmd compat_priv_cmd;
		if (copy_from_user(&compat_priv_cmd, ifr->ifr_data,
			sizeof(compat_android_wifi_priv_cmd))) {
			ret = -EFAULT;
			goto exit;

		}
		priv_cmd.buf = compat_ptr(compat_priv_cmd.buf);
		priv_cmd.used_len = compat_priv_cmd.used_len;
		priv_cmd.total_len = compat_priv_cmd.total_len;
	} else
#endif /* CONFIG_COMPAT */
	{
		if (copy_from_user(&priv_cmd, ifr->ifr_data, sizeof(android_wifi_priv_cmd))) {
			ret = -EFAULT;
			goto exit;
		}
	}
	if ((priv_cmd.total_len > PRIVATE_COMMAND_MAX_LEN) || (priv_cmd.total_len < 0)) {
		DHD_ERROR(("wl_android_priv_cmd: buf length invalid:%d\n",
			priv_cmd.total_len));
		ret = -EINVAL;
		goto exit;
	}

	buf_size = max(priv_cmd.total_len, PRIVATE_COMMAND_DEF_LEN);
	command = (char *)MALLOC(cfg->osh, (buf_size + 1));
	if (!command) {
		DHD_ERROR(("wl_android_priv_cmd: failed to allocate memory\n"));
		ret = -ENOMEM;
		goto exit;
	}
	if (copy_from_user(command, priv_cmd.buf, priv_cmd.total_len)) {
		ret = -EFAULT;
		goto exit;
	}
	command[priv_cmd.total_len] = '\0';

	if (wl_android_priv_cmd_log_enable_check(command)) {
		DHD_ERROR(("wl_android_priv_cmd: Android private cmd \"%s\" on %s\n",
			command, ifr->ifr_name));
	}
	else {
		DHD_INFO(("wl_android_priv_cmd: Android private cmd \"%s\" on %s\n",
			command, ifr->ifr_name));

	}

	bytes_written = wl_handle_private_cmd(net, command, priv_cmd.total_len);
	if (bytes_written >= 0) {
		if ((bytes_written == 0) && (priv_cmd.total_len > 0)) {
			command[0] = '\0';
		}
		if (bytes_written >= priv_cmd.total_len) {
			DHD_ERROR(("wl_android_priv_cmd: err. bytes_written:%d >= total_len:%d,"
				" buf_size:%d \n", bytes_written, priv_cmd.total_len, buf_size));
			ret = BCME_BUFTOOSHORT;
			goto exit;
		}
		bytes_written++;
		priv_cmd.used_len = bytes_written;
		if (copy_to_user(priv_cmd.buf, command, bytes_written)) {
			DHD_ERROR(("wl_android_priv_cmd: failed to copy data to user buffer\n"));
			ret = -EFAULT;
		}
	}
	else {
		/* Propagate the error */
		ret = bytes_written;
	}

exit:
#ifdef DHD_SEND_HANG_PRIVCMD_ERRORS
	if (ret) {
		/* Avoid incrementing priv_cmd_errors in case of unsupported feature
		* or BUSY state specific to TWT commands
		*/
		if (
#ifdef WL_TWT
			!(((ret == -EBUSY) || (ret == -EOPNOTSUPP)) &&
			((strnicmp(command, CMD_TWT_SETUP, strlen(CMD_TWT_SETUP)) == 0) ||
			(strnicmp(command, CMD_TWT_TEARDOWN, strlen(CMD_TWT_TEARDOWN)) == 0) ||
			(strnicmp(command, CMD_TWT_INFO, strlen(CMD_TWT_INFO)) == 0))) &&
#endif /* WL_TWT */
			(ret != BCME_UNSUPPORTED)) {
			wl_android_check_priv_cmd_errors(net);
		}
	} else {
		priv_cmd_errors = 0;
	}
#endif /* DHD_SEND_HANG_PRIVCMD_ERRORS */
	net_os_wake_unlock(net);
	MFREE(cfg->osh, command, (buf_size + 1));
	return ret;
}

#ifdef WL_BCNRECV
#define BCNRECV_ATTR_HDR_LEN 30
int
wl_android_bcnrecv_event(struct net_device *ndev, uint attr_type,
		uint status, uint reason, uint8 *data, uint data_len)
{
	s32 err = BCME_OK;
	struct sk_buff *skb;
	gfp_t kflags;
	struct bcm_cfg80211 *cfg = wl_get_cfg(ndev);
	struct wiphy *wiphy = bcmcfg_to_wiphy(cfg);
	uint len;

	len = BCNRECV_ATTR_HDR_LEN + data_len;

	kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	skb = CFG80211_VENDOR_EVENT_ALLOC(wiphy, ndev_to_wdev(ndev), len,
		BRCM_VENDOR_EVENT_BEACON_RECV, kflags);
	if (!skb) {
		WL_ERR(("skb alloc failed"));
		return -ENOMEM;
	}
	if ((attr_type == BCNRECV_ATTR_BCNINFO) && (data)) {
		/* send bcn info to upper layer */
		nla_put(skb, BCNRECV_ATTR_BCNINFO, data_len, data);
	} else if (attr_type == BCNRECV_ATTR_STATUS) {
		nla_put_u32(skb, BCNRECV_ATTR_STATUS, status);
		if (reason) {
			nla_put_u32(skb, BCNRECV_ATTR_REASON, reason);
		}
	} else {
		WL_ERR(("UNKNOWN ATTR_TYPE. attr_type:%d\n", attr_type));
		kfree_skb(skb);
		return -EINVAL;
	}
	cfg80211_vendor_event(skb, kflags);
	return err;
}

static int
_wl_android_bcnrecv_start(struct bcm_cfg80211 *cfg, struct net_device *ndev, bool user_trigger)
{
	s32 err = BCME_OK;
	struct net_device *pdev = bcmcfg_to_prmry_ndev(cfg);
	dhd_pub_t *dhd = cfg->pub;

	/* check any scan is in progress before beacon recv scan trigger IOVAR */
	if (wl_get_drv_status_all(cfg, SCANNING)) {
		err = BCME_UNSUPPORTED;
		WL_ERR(("Scan in progress, Aborting beacon recv start, "
			"error:%d\n", err));
		goto exit;
	}

	if (wl_get_p2p_status(cfg, SCANNING)) {
		err = BCME_UNSUPPORTED;
		WL_ERR(("P2P Scan in progress, Aborting beacon recv start, "
			"error:%d\n", err));
		goto exit;
	}

	if (wl_get_drv_status(cfg, REMAINING_ON_CHANNEL, ndev)) {
		err = BCME_UNSUPPORTED;
		WL_ERR(("P2P remain on channel, Aborting beacon recv start, "
			"error:%d\n", err));
		goto exit;
	}

	/* check STA is in connected state, Beacon recv required connected state
	 * else exit from beacon recv scan
	 */
	if (!wl_get_drv_status(cfg, CONNECTED, ndev)) {
		err = BCME_UNSUPPORTED;
		WL_ERR(("STA is in not associated state error:%d\n", err));
		goto exit;
	}

#ifdef WL_NAN
	/* Check NAN is enabled, if enabled exit else continue */
	if (wl_cfgnan_is_enabled(cfg)) {
		err = BCME_UNSUPPORTED;
		WL_ERR(("Nan is enabled, NAN+STA+FAKEAP concurrency is not supported\n"));
		goto exit;
	}
#endif /* WL_NAN */

	if (dhd->early_suspended) {
		/* Set BEACON_RECV in suspend mode */
		WL_INFORM_MEM(("Already suspend mode, Aborting beacon recv start\n"));
		cfg->bcnrecv_info.bcnrecv_state = BEACON_RECV_SUSPENDED;
		if ((err = wl_android_bcnrecv_event(pdev, BCNRECV_ATTR_STATUS,
			WL_BCNRECV_SUSPENDED, WL_BCNRECV_SUSPEND, NULL, 0)) != BCME_OK) {
			WL_ERR(("failed to send bcnrecv event, error:%d\n", err));
		}
		goto exit;
	}

	/* Triggering an sendup_bcn iovar */
	err = wldev_iovar_setint_no_wl(pdev, "sendup_bcn", 1);
	if (unlikely(err)) {
		WL_ERR(("sendup_bcn failed to set, error:%d\n", err));
	} else {
		cfg->bcnrecv_info.bcnrecv_state = BEACON_RECV_STARTED;
		WL_INFORM_MEM(("bcnrecv started. user_trigger:%d ifindex:%d\n",
			user_trigger, ndev->ifindex));
		if (user_trigger) {
			if ((err = wl_android_bcnrecv_event(pdev, BCNRECV_ATTR_STATUS,
					WL_BCNRECV_STARTED, 0, NULL, 0)) != BCME_OK) {
				WL_ERR(("failed to send bcnrecv event, error:%d\n", err));
			}
		}
	}
exit:
	/*
	 * BCNRECV start request can be rejected from dongle
	 * in various conditions.
	 * Error code need to be overridden to BCME_UNSUPPORTED
	 * to avoid hang event from continous private
	 * command error
	 */
	if (err) {
		err = BCME_UNSUPPORTED;
	}
	return err;
}

int
_wl_android_bcnrecv_stop(struct bcm_cfg80211 *cfg, struct net_device *ndev, uint reason)
{
	s32 err = BCME_OK;
	u32 status;
	struct net_device *pdev = bcmcfg_to_prmry_ndev(cfg);

	/* Stop bcnrx except for fw abort event case */
	if (reason != WL_BCNRECV_ROAMABORT) {
		err = wldev_iovar_setint_no_wl(pdev, "sendup_bcn", 0);
		if (unlikely(err)) {
			WL_ERR(("sendup_bcn failed to set error:%d\n", err));
			goto exit;
		}
	}

	/* Send notification for all cases */
	if (reason == WL_BCNRECV_SUSPEND) {
		cfg->bcnrecv_info.bcnrecv_state = BEACON_RECV_SUSPENDED;
		status = WL_BCNRECV_SUSPENDED;
	} else {
		cfg->bcnrecv_info.bcnrecv_state = BEACON_RECV_STOPPED;
		WL_INFORM_MEM(("bcnrecv stopped. reason:%d ifindex:%d\n",
			reason, ndev->ifindex));
		if (reason == WL_BCNRECV_USER_TRIGGER) {
			status = WL_BCNRECV_STOPPED;
		} else {
			status = WL_BCNRECV_ABORTED;
		}
	}
	if ((err = wl_android_bcnrecv_event(pdev, BCNRECV_ATTR_STATUS, status,
			reason, NULL, 0)) != BCME_OK) {
		WL_ERR(("failed to send bcnrecv event, error:%d\n", err));
	}
exit:
	return err;
}

static int
wl_android_bcnrecv_start(struct bcm_cfg80211 *cfg, struct net_device *ndev)
{
	s32 err = BCME_OK;

	/* Adding scan_sync mutex to avoid race condition in b/w scan_req and bcn recv */
	mutex_lock(&cfg->scan_sync);
	mutex_lock(&cfg->bcn_sync);
	err = _wl_android_bcnrecv_start(cfg, ndev, true);
	mutex_unlock(&cfg->bcn_sync);
	mutex_unlock(&cfg->scan_sync);
	return err;
}

int
wl_android_bcnrecv_stop(struct net_device *ndev, uint reason)
{
	s32 err = BCME_OK;
	struct bcm_cfg80211 *cfg = wl_get_cfg(ndev);

	mutex_lock(&cfg->bcn_sync);
	if ((cfg->bcnrecv_info.bcnrecv_state == BEACON_RECV_STARTED) ||
	   (cfg->bcnrecv_info.bcnrecv_state == BEACON_RECV_SUSPENDED)) {
		err = _wl_android_bcnrecv_stop(cfg, ndev, reason);
	}
	mutex_unlock(&cfg->bcn_sync);
	return err;
}

int
wl_android_bcnrecv_suspend(struct net_device *ndev)
{
	s32 ret = BCME_OK;
	struct bcm_cfg80211 *cfg = wl_get_cfg(ndev);

	mutex_lock(&cfg->bcn_sync);
	if (cfg->bcnrecv_info.bcnrecv_state == BEACON_RECV_STARTED) {
		WL_INFORM_MEM(("bcnrecv suspend\n"));
		ret = _wl_android_bcnrecv_stop(cfg, ndev, WL_BCNRECV_SUSPEND);
	}
	mutex_unlock(&cfg->bcn_sync);
	return ret;
}

int
wl_android_bcnrecv_resume(struct net_device *ndev)
{
	s32 ret = BCME_OK;
	struct bcm_cfg80211 *cfg = wl_get_cfg(ndev);

	/* Adding scan_sync mutex to avoid race condition in b/w scan_req and bcn recv */
	mutex_lock(&cfg->scan_sync);
	mutex_lock(&cfg->bcn_sync);
	if (cfg->bcnrecv_info.bcnrecv_state == BEACON_RECV_SUSPENDED) {
		WL_INFORM_MEM(("bcnrecv resume\n"));
		ret = _wl_android_bcnrecv_start(cfg, ndev, false);
	}
	mutex_unlock(&cfg->bcn_sync);
	mutex_unlock(&cfg->scan_sync);
	return ret;
}

/* Beacon recv functionality code implementation */
int
wl_android_bcnrecv_config(struct net_device *ndev, char *cmd_argv, int total_len)
{
	struct bcm_cfg80211 *cfg = NULL;
	uint err = BCME_OK;

	if (!ndev) {
		WL_ERR(("ndev is NULL\n"));
		return -EINVAL;
	}

	cfg = wl_get_cfg(ndev);
	if (!cfg) {
		WL_ERR(("cfg is NULL\n"));
		return -EINVAL;
	}

	/* sync commands from user space */
	mutex_lock(&cfg->usr_sync);
	if (strncmp(cmd_argv, "start", strlen("start")) == 0) {
		WL_INFORM(("BCNRECV start\n"));
		err = wl_android_bcnrecv_start(cfg, ndev);
		if (err != BCME_OK) {
			WL_ERR(("Failed to process the start command, error:%d\n", err));
			goto exit;
		}
	} else if (strncmp(cmd_argv, "stop", strlen("stop")) == 0) {
		WL_INFORM(("BCNRECV stop\n"));
		err = wl_android_bcnrecv_stop(ndev, WL_BCNRECV_USER_TRIGGER);
		if (err != BCME_OK) {
			WL_ERR(("Failed to stop the bcn recv, error:%d\n", err));
			goto exit;
		}
	} else {
		err = BCME_ERROR;
	}
exit:
	mutex_unlock(&cfg->usr_sync);
	return err;
}
#endif /* WL_BCNRECV */

#ifdef SUPPORT_LATENCY_CRITICAL_DATA
int
wl_android_set_latency_crt_data(struct net_device *dev, int mode)
{
	int ret;
#ifdef WL_LATENCY_CONFIG
	char iovreq[WLC_IOCTL_SMLEN] = {0};
	wl_low_latency_v1_t low_latency;
#endif /* WL_LATENCY_CONFIG */
	struct bcm_cfg80211 *cfg = NULL;
#ifdef DHD_GRO_ENABLE_HOST_CTRL
	dhd_pub_t *dhdp = NULL;
#endif /* DHD_GRO_ENABLE_HOST_CTRL */

	if (mode >= LATENCY_CRT_DATA_MODE_LAST) {
		ret = BCME_BADARG;
		goto exit;
	}

	if (!dev) {
		WL_ERR(("ndev is NULL\n"));
		ret = -EINVAL;
		goto exit;
	}

	cfg = wl_get_cfg(dev);
	if (!cfg) {
		WL_ERR(("cfg is NULL\n"));
		ret = -EINVAL;
		goto exit;
	}

	if (mode == cfg->latency_mode) {
		WL_INFORM_MEM(("Mode value: %d is already configured!!\n", cfg->latency_mode));
		ret = BCME_OK;
		goto exit;
	}

#ifdef DHD_GRO_ENABLE_HOST_CTRL
	dhdp = wl_cfg80211_get_dhdp(dev);
	if (mode != LATENCY_CRT_DATA_MODE_OFF) {
		WL_ERR(("Not permitted GRO by framework\n"));
		dhdp->permitted_gro = FALSE;
	} else {
		WL_ERR(("Permitted GRO by framework\n"));
		dhdp->permitted_gro = TRUE;
	}
#endif /* DHD_GRO_ENABLE_HOST_CTRL */

#ifdef WL_LATENCY_CONFIG
	bzero(&low_latency, sizeof(wl_low_latency_v1_t));

	/* set the version and length of wl_low_latency_v1_t */
	low_latency.version = WL_LOW_LATENCY_V1;
	low_latency.length = sizeof(wl_low_latency_v1_t);
	low_latency.config.version = WL_LOW_LATENCY_CONFIG_V1;
	low_latency.config.length = sizeof(low_latency.config);
	low_latency.config.periodicity = DEF_LOW_LATENCY_PERIOD;
	low_latency.mode = mode;

	WL_DBG(("low_latency: mode is %d, periodicity is %d\n",
		low_latency.mode, low_latency.config.periodicity));

	ret = wldev_iovar_setbuf(dev, "low_latency", (char *)&low_latency,
			sizeof(wl_low_latency_v1_t), iovreq, WLC_IOCTL_SMLEN, NULL);
	if (ret < 0) {
		WL_ERR(("low_latency set failed:%d\n", ret));
		goto exit;
	} else {
		WL_INFORM_MEM(("low_latency set ok\n"));
	}
#endif /* WL_LATENCY_CONFIG */

	ret = wldev_iovar_setint(dev, "latency_critical_data", mode);
	if (ret != BCME_OK) {
		WL_ERR(("failed to set latency_critical_data mode %d, error = %d\n",
			mode, ret));
		goto exit;
	}

	/* Cache mode value */
	cfg->latency_mode = mode;
exit:
	return ret;
}

static int
wl_android_get_latency_crt_data(struct net_device *dev, char *command, int total_len)
{
	int ret;
	int mode = LATENCY_CRT_DATA_MODE_OFF;
	int bytes_written;

	ret = wldev_iovar_getint(dev, "latency_critical_data", &mode);
	if (ret != BCME_OK) {
		WL_ERR(("failed to get latency_critical_data error = %d\n", ret));
		return ret;
	}

	bytes_written = snprintf(command, total_len, "%s %d",
		CMD_GET_LATENCY_CRITICAL_DATA, mode);

	return bytes_written;
}
#endif	/* SUPPORT_LATENCY_CRITICAL_DATA */

#ifdef WL_LATENCY_CONFIG
static int
wl_android_set_low_latency(struct net_device *dev, char* cmd_str)
{
	int ret;
	u8 mode = 0, period = 0;
	char iovreq[WLC_IOCTL_SMLEN] = {0};
	wl_low_latency_v1_t low_latency;
	char *param = NULL, *val_p = NULL;

	bzero(&low_latency, sizeof(wl_low_latency_v1_t));

	/* set the version and length of wl_low_latency_v1_t */
	low_latency.version = WL_LOW_LATENCY_V1;
	low_latency.length = sizeof(wl_low_latency_v1_t);
	low_latency.config.version = WL_LOW_LATENCY_CONFIG_V1;
	low_latency.config.length = sizeof(low_latency.config);

	param = cmd_str;

	/* parsing string options */
	while ((val_p = bcmstrtok(&param, " ", NULL)) != NULL) {
		if (!strnicmp(val_p, "mode", strlen(val_p))) {
			val_p = bcmstrtok(&param, " ", NULL);
			if (val_p) {
				/* get mode */
				mode = bcm_atoi(val_p);
				if (mode >= LATENCY_CRT_DATA_MODE_LAST) {
					ret = BCME_BADARG;
					goto exit;
				}
			} else {
				WL_ERR(("%s : no mode value\n", __FUNCTION__));
				ret = BCME_BADARG;
				goto exit;
			}
		} else if (!strnicmp(val_p, "period", strlen(val_p))) {
			val_p = bcmstrtok(&param, " ", NULL);
			if (val_p) {
				/* get period */
				period = bcm_atoi(val_p);
			} else {
				WL_ERR(("%s : no period value\n", __FUNCTION__));
				ret = BCME_BADARG;
				goto exit;
			}
		} else {
			WL_ERR(("%s : unsupported parameter: %s\n", __FUNCTION__, param));
			ret = BCME_BADARG;
			goto exit;
		}
	}

	low_latency.config.periodicity = period;
	low_latency.mode = mode;

	WL_DBG(("low_latency: mode is %d, periodicity is %d\n",
		low_latency.mode, low_latency.config.periodicity));

	ret = wldev_iovar_setbuf(dev, "low_latency", (char *)&low_latency,
			sizeof(wl_low_latency_v1_t), iovreq, WLC_IOCTL_SMLEN, NULL);
	if (ret < 0) {
		WL_ERR(("low_latency set failed:%d\n", ret));
	} else {
		WL_INFORM_MEM(("low_latency set ok\n"));
	}
exit:
	return ret;
}
#endif /* WL_LATENCY_CONFIG */

#ifdef WL_CAC_TS
/* CAC TSPEC functionality code implementation */
static void
wl_android_update_tsinfo(uint8 access_category, tspec_arg_t *tspec_arg)
{
	uint8 tspec_id;
	/* Using direction as bidirectional by default */
	uint8 direction = TSPEC_BI_DIRECTION;
	/* Using U-APSD as the default power save mode */
	uint8 user_psb = TSPEC_UAPSD_PSB;
	uint8 ADDTS_AC2PRIO[4] = {PRIO_8021D_BE, PRIO_8021D_BK, PRIO_8021D_VI, PRIO_8021D_VO};

	/* Map tspec_id from access category */
	tspec_id = ADDTS_AC2PRIO[access_category];

	/* Update the tsinfo */
	tspec_arg->tsinfo.octets[0] = (uint8)(TSPEC_EDCA_ACCESS | direction |
		(tspec_id << TSPEC_TSINFO_TID_SHIFT));
	tspec_arg->tsinfo.octets[1] = (uint8)((tspec_id << TSPEC_TSINFO_PRIO_SHIFT) |
		user_psb);
	tspec_arg->tsinfo.octets[2] = 0x00;
}

static s32
wl_android_handle_cac_action(struct bcm_cfg80211 * cfg, struct net_device * ndev, char * argv)
{
	tspec_arg_t tspec_arg;
	s32 err = BCME_ERROR;
	u8 ts_cmd[12] = "cac_addts";
	uint8 access_category;
	s32 bssidx;

	/* Following handling is done only for the primary interface */
	memset_s(&tspec_arg, sizeof(tspec_arg), 0, sizeof(tspec_arg));
	if (strncmp(argv, "addts", strlen("addts")) == 0) {
		tspec_arg.version = TSPEC_ARG_VERSION;
		tspec_arg.length = sizeof(tspec_arg_t) - (2 * sizeof(uint16));
		/* Read the params passed */
		sscanf(argv, "%*s %hhu %hu %hu", &access_category,
			&tspec_arg.nom_msdu_size, &tspec_arg.surplus_bw);
		if ((access_category > TSPEC_MAX_ACCESS_CATEGORY) ||
			((tspec_arg.surplus_bw < TSPEC_MIN_SURPLUS_BW) ||
			(tspec_arg.surplus_bw > TSPEC_MAX_SURPLUS_BW)) ||
			(tspec_arg.nom_msdu_size > TSPEC_MAX_MSDU_SIZE)) {
			WL_ERR(("Invalid params access_category %hhu nom_msdu_size %hu"
				" surplus BW %hu\n", access_category, tspec_arg.nom_msdu_size,
				tspec_arg.surplus_bw));
			return BCME_USAGE_ERROR;
		}

		/* Update tsinfo */
		wl_android_update_tsinfo(access_category, &tspec_arg);
		/* Update other tspec parameters */
		tspec_arg.dialog_token = TSPEC_DEF_DIALOG_TOKEN;
		tspec_arg.mean_data_rate = TSPEC_DEF_MEAN_DATA_RATE;
		tspec_arg.min_phy_rate = TSPEC_DEF_MIN_PHY_RATE;
	} else if (strncmp(argv, "delts", strlen("delts")) == 0) {
		snprintf(ts_cmd, sizeof(ts_cmd), "cac_delts");
		tspec_arg.length = sizeof(tspec_arg_t) - (2 * sizeof(uint16));
		tspec_arg.version = TSPEC_ARG_VERSION;
		/* Read the params passed */
		sscanf(argv, "%*s %hhu", &access_category);

		if (access_category > TSPEC_MAX_ACCESS_CATEGORY) {
			WL_INFORM_MEM(("Invalide param, access_category %hhu\n", access_category));
			return BCME_USAGE_ERROR;
		}
		/* Update tsinfo */
		wl_android_update_tsinfo(access_category, &tspec_arg);
	}

	if ((bssidx = wl_get_bssidx_by_wdev(cfg, ndev->ieee80211_ptr)) < 0) {
		WL_ERR(("Find index failed\n"));
		err = BCME_ERROR;
		return err;
	}
	err = wldev_iovar_setbuf_bsscfg(ndev, ts_cmd, &tspec_arg, sizeof(tspec_arg),
			cfg->ioctl_buf, WLC_IOCTL_MAXLEN, bssidx, &cfg->ioctl_buf_sync);
	if (unlikely(err)) {
		WL_ERR(("%s error (%d)\n", ts_cmd, err));
	}

	return err;
}

static s32
wl_android_cac_ts_config(struct net_device *ndev, char *cmd_argv, int total_len)
{
	struct bcm_cfg80211 *cfg = NULL;
	s32 err = BCME_OK;

	if (!ndev) {
		WL_ERR(("ndev is NULL\n"));
		return -EINVAL;
	}

	cfg = wl_get_cfg(ndev);
	if (!cfg) {
		WL_ERR(("cfg is NULL\n"));
		return -EINVAL;
	}

	/* Request supported only for primary interface */
	if (ndev != bcmcfg_to_prmry_ndev(cfg)) {
		WL_ERR(("Request on non-primary interface\n"));
		return -1;
	}

	/* sync commands from user space */
	mutex_lock(&cfg->usr_sync);
	err = wl_android_handle_cac_action(cfg, ndev, cmd_argv);
	mutex_unlock(&cfg->usr_sync);

	return err;
}
#endif /* WL_CAC_TS */

#ifdef WL_GET_CU
/* Implementation to get channel usage from framework */
static s32
wl_android_get_channel_util(struct net_device *ndev, char *command, int total_len)
{
	s32 bytes_written, err = 0;
	wl_bssload_t bssload;
	u8 smbuf[WLC_IOCTL_SMLEN];

	if ((err = wldev_iovar_getbuf(ndev, "bssload_report", NULL,
		0, smbuf, WLC_IOCTL_SMLEN, NULL))) {
		WL_ERR(("Getting bssload report failed with err=%d \n", err));
		return err;
	}

	(void)memcpy_s(&bssload, sizeof(wl_bssload_t), smbuf, sizeof(wl_bssload_t));
	WL_DBG(("Channel Utilization %u\n", bssload.chan_util));
	bytes_written = snprintf(command, total_len, "%hhu", bssload.chan_util);

	return bytes_written;
}
#endif /* WL_GET_CU */

#ifdef RTT_GEOFENCE_INTERVAL
#if defined(RTT_SUPPORT) && defined(WL_NAN)
static void
wl_android_set_rtt_geofence_interval(struct net_device *ndev, char *command)
{
	int rtt_interval = 0;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(ndev);
	char *rtt_intp = command + strlen(CMD_GEOFENCE_INTERVAL) + 1;

	rtt_interval = bcm_atoi(rtt_intp);
	dhd_rtt_set_geofence_rtt_interval(dhdp, rtt_interval);
}
#endif /* RTT_SUPPORT && WL_NAN */
#endif /* RTT_GEOFENCE_INTERVAL */

#ifdef SUPPORT_SOFTAP_ELNA_BYPASS
int
wl_android_set_softap_elna_bypass(struct net_device *dev, char *command, int total_len)
{
	char *ifname = NULL;
	char *pos, *token;
	int err = BCME_OK;
	int enable = FALSE;

	/*
	 * STA/AP/GO I/F: DRIVER SET_SOFTAP_ELNA_BYPASS <ifname> <enable/disable>
	 * the enable/disable format follows Samsung specific rules as following
	 * Enable : 0
	 * Disable :-1
	 */
	pos = command;

	/* drop command */
	token = bcmstrtok(&pos, " ", NULL);

	/* get the interface name */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token) {
		WL_ERR(("%s: Invalid arguments about interface name\n", __FUNCTION__));
		return -EINVAL;
	}
	ifname = token;

	/* get enable/disable flag */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token) {
		WL_ERR(("%s: Invalid arguments about Enable/Disable\n", __FUNCTION__));
		return -EINVAL;
	}
	enable = bcm_atoi(token);

	CUSTOMER_HW4_EN_CONVERT(enable);
	err = wl_set_softap_elna_bypass(dev, ifname, enable);
	if (unlikely(err)) {
		WL_ERR(("%s: Failed to set ELNA Bypass of SoftAP mode, err=%d\n",
			__FUNCTION__, err));
		return err;
	}

	return err;
}

int
wl_android_get_softap_elna_bypass(struct net_device *dev, char *command, int total_len)
{
	char *ifname = NULL;
	char *pos, *token;
	int err = BCME_OK;
	int bytes_written = 0;
	int softap_elnabypass = 0;

	/*
	 * STA/AP/GO I/F: DRIVER GET_SOFTAP_ELNA_BYPASS <ifname>
	 */
	pos = command;

	/* drop command */
	token = bcmstrtok(&pos, " ", NULL);

	/* get the interface name */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token) {
		WL_ERR(("%s: Invalid arguments about interface name\n", __FUNCTION__));
		return -EINVAL;
	}
	ifname = token;

	err = wl_get_softap_elna_bypass(dev, ifname, &softap_elnabypass);
	if (unlikely(err)) {
		WL_ERR(("%s: Failed to get ELNA Bypass of SoftAP mode, err=%d\n",
			__FUNCTION__, err));
		return err;
	} else {
		softap_elnabypass--; //Convert format to Customer HW4
		WL_DBG(("%s: eLNA Bypass feature enable status is %d\n",
			__FUNCTION__, softap_elnabypass));
		bytes_written = snprintf(command, total_len, "%s %d",
			CMD_GET_SOFTAP_ELNA_BYPASS, softap_elnabypass);
	}

	return bytes_written;
}
#endif /* SUPPORT_SOFTAP_ELNA_BYPASS */

#ifdef CUSTOM_SOFTAP_SET_ANT
#define HOTSPOT_ANTENNA_MODE_MIMO  1
#define HOTSPOT_ANTENNA_MODE_SISO  2

static int
wl_android_set_softap_antenna(struct net_device *dev, char *command)
{
	char *ifname = NULL;
	char *pos, *token;
	int err = BCME_OK;
	int mimo_siso = 0;
	int set_chain = 0;

	/*
	 * DRIVER SET_HOTSPOT_ANTENNA_MODE <ifname> <MIMO/SISO>
	 * MIMO:1, SISO:2
	 */
	pos = command;

	/* drop command */
	token = bcmstrtok(&pos, " ", NULL);

	/* get the interface name */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token) {
		WL_ERR(("%s: Invalid arguments about interface name\n", __FUNCTION__));
		return -EINVAL;
	}
	ifname = token;

	/* get mimo_siso value */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token) {
		WL_ERR(("%s: Invalid arguments about MIMO/SISO\n", __FUNCTION__));
		return -EINVAL;
	}
	mimo_siso = bcm_atoi(token);

	if (mimo_siso == HOTSPOT_ANTENNA_MODE_SISO) {
		/* Set SISO mode */
		set_chain = 1;
	} else {
		/* Set MIMO mode for other cases */
		set_chain = 3;
	}

	WL_ERR(("[Test Mode] %s: Set Antenna for SoftAP [%d]\n", __FUNCTION__, set_chain));
	err = wl_set_softap_antenna(dev, ifname, set_chain);
	if (unlikely(err)) {
		WL_ERR(("%s: Failed to set Antenna of SoftAP mode[%d], err=%d\n",
			__FUNCTION__, mimo_siso, err));
		return -EIO;
	}

	return err;
}

static int
wl_android_get_softap_antenna(struct net_device *dev, char *command, int total_len)
{
	char *ifname = NULL;
	char *pos, *token;
	int err = BCME_OK;
	int bytes_written = 0;
	int mimo_siso = 0;
	uint32 cur_rxchain;

	/*
	 * DRIVER GET_HOTSPOT_ANTENNA_MODE <ifname>
	 */
	pos = command;

	/* drop command */
	token = bcmstrtok(&pos, " ", NULL);

	/* get the interface name */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token) {
		WL_ERR(("%s: Invalid arguments about interface name\n", __FUNCTION__));
		return -EINVAL;
	}
	ifname = token;

	err = wl_get_softap_antenna(dev, ifname, &cur_rxchain);
	if (unlikely(err)) {
		WL_ERR(("%s: Failed to get MIMO/SISO info of SoftAP mode, err=%d\n",
			__FUNCTION__, err));
		return -EIO;
	} else {
		if (cur_rxchain == 3) {
			mimo_siso = HOTSPOT_ANTENNA_MODE_MIMO;
		} else {
			mimo_siso = HOTSPOT_ANTENNA_MODE_SISO;
		}
		WL_DBG(("[Test Mode] %s: SoftAP antenna status is %d\n",
			__FUNCTION__, mimo_siso));
		bytes_written = snprintf(command, total_len, "%s %d",
			CMD_GET_HOTSPOT_ANTENNA_MODE, mimo_siso);
	}

	return bytes_written;
}
#endif /* CUSTOM_SOFTAP_SET_ANT */

#ifdef WL_NAN
int
wl_android_get_nan_status(struct net_device *dev, char *command, int total_len)
{
	int bytes_written = 0;
	int error = BCME_OK;
	wl_nan_conf_status_t nstatus;

	error = wl_cfgnan_get_status(dev, &nstatus);
	if (error) {
		WL_ERR(("Failed to get nan status (%d)\n", error));
		return error;
	}

	bytes_written = snprintf(command, total_len,
			"EN:%d Role:%d EM:%d CID:"MACF" NMI:"MACF" SC(2G):%d SC(5G):%d "
			"MR:"NMRSTR" AMR:"NMRSTR" IMR:"NMRSTR
			"HC:%d AMBTT:%04x TSF[%04x:%04x]\n",
			nstatus.enabled,
			nstatus.role,
			nstatus.election_mode,
			ETHERP_TO_MACF(&(nstatus.cid)),
			ETHERP_TO_MACF(&(nstatus.nmi)),
			nstatus.social_chans[0],
			nstatus.social_chans[1],
			NMR2STR(nstatus.mr),
			NMR2STR(nstatus.amr),
			NMR2STR(nstatus.imr),
			nstatus.hop_count,
			nstatus.ambtt,
			nstatus.cluster_tsf_h,
			nstatus.cluster_tsf_l);
	return bytes_written;
}
#endif /* WL_NAN */

#ifdef SUPPORT_NAN_RANGING_TEST_BW
enum {
	NAN_RANGING_5G_BW20 = 1,
	NAN_RANGING_5G_BW40,
	NAN_RANGING_5G_BW80
};

int
wl_nan_ranging_bw(struct net_device *net, int bw, char *command)
{
	int bytes_written, err = BCME_OK;
	u8 ioctl_buf[WLC_IOCTL_SMLEN];
	s32 val = 1;
	struct {
		u32 band;
		u32 bw_cap;
	} param = {0, 0};

	if (bw < NAN_RANGING_5G_BW20 || bw > NAN_RANGING_5G_BW80) {
		WL_ERR(("Wrong BW cmd:%d, %s\n", bw, __FUNCTION__));
		bytes_written = scnprintf(command, sizeof("FAIL"), "FAIL");
		return bytes_written;
	}

	switch (bw) {
		case NAN_RANGING_5G_BW20:
			WL_ERR(("NAN_RANGING 5G/BW20\n"));
			param.band = WLC_BAND_5G;
			param.bw_cap = 0x1;
			break;
		case NAN_RANGING_5G_BW40:
			WL_ERR(("NAN_RANGING 5G/BW40\n"));
			param.band = WLC_BAND_5G;
			param.bw_cap = 0x3;
			break;
		case NAN_RANGING_5G_BW80:
			WL_ERR(("NAN_RANGING 5G/BW80\n"));
			param.band = WLC_BAND_5G;
			param.bw_cap = 0x7;
			break;
	}

	err = wldev_ioctl_set(net, WLC_DOWN, &val, sizeof(s32));
	if (err) {
		WL_ERR(("WLC_DOWN error %d\n", err));
		bytes_written = scnprintf(command, sizeof("FAIL"), "FAIL");
	} else {
		err = wldev_iovar_setbuf(net, "bw_cap", &param, sizeof(param),
			ioctl_buf, sizeof(ioctl_buf), NULL);

		if (err) {
			WL_ERR(("BW set failed\n"));
			bytes_written = scnprintf(command, sizeof("FAIL"), "FAIL");
		} else {
			WL_ERR(("BW set done\n"));
			bytes_written = scnprintf(command, sizeof("OK"), "OK");
		}

		err = wldev_ioctl_set(net, WLC_UP, &val, sizeof(s32));
		if (err < 0) {
			WL_ERR(("WLC_UP error %d\n", err));
			bytes_written = scnprintf(command, sizeof("FAIL"), "FAIL");
		}
	}
	return bytes_written;
}
#endif /* SUPPORT_NAN_RANGING_TEST_BW */

static int
wl_android_set_softap_ax_mode(struct net_device *dev, const char* cmd_str)
{
	int enable = 0;
	int err = 0;
	s32 bssidx = 0;
	struct bcm_cfg80211 *cfg = NULL;

	if (!dev) {
		err = -EINVAL;
		goto exit;
	}

	cfg = wl_get_cfg(dev);
	if (!cfg) {
		err = -EINVAL;
		goto exit;
	}

	if (cmd_str) {
		enable = bcm_atoi(cmd_str);
	} else {
		WL_ERR(("failed due to wrong received parameter\n"));
		err = -EINVAL;
		goto exit;
	}

	if ((bssidx = wl_get_bssidx_by_wdev(cfg, dev->ieee80211_ptr)) < 0) {
		WL_ERR(("find softap index from wdev failed\n"));
		err = -EINVAL;
		goto exit;
	}

	WL_INFORM(("HAPD_SET_AX_MODE = %d\n", enable));
	err = wl_cfg80211_change_he_features(dev, cfg, bssidx, WL_HE_FEATURES_HE_AP, (bool)enable);
	if (err) {
		WL_ERR(("failed to set softap ax mode(%d)\n", enable));

	}
exit :
	return err;
}

#ifdef WL_TWT
static int
wl_android_twt_bcmerr_to_kernel_err(int bcm_err)
{
	int ret = 0;

	switch (bcm_err) {
		case BCME_OK:
			ret = 0;
			break;
		case BCME_ERROR:
			ret = -EAGAIN;
			break;
		case BCME_BADARG: /* Bad Argument */
			ret = -EINVAL;
			break;
		case BCME_RANGE: /* TWT parameter value provided is not in allowed range */
			ret = -ERANGE;
			break;
		case BCME_BUSY:
			/* Busy
			* 1. STA is in off-channel/Scan in progress
			* 2. STA is in allTWT Suspend mode
			* 3. Other TWT Action frame transmission is not completed yet
			*/
			ret = -EBUSY;
			break;
		case BCME_NOTASSOCIATED: /* STA Not Associated */
			ret = -ENOTCONN;
			break;
		case BCME_EPERM: /* Not Permitted because of
			* 1. More than one TWT
			* 2. More than one concurrent connection is active.
			* 3. BT is active
			*/
			ret = -EPERM;
			break;
		case BCME_NOTFOUND: /* Not Found. Peer not found for given mac address */
		case BCME_NORESOURCE: /* Not Enough resources to establish new TWT session */
			ret = -ENODEV;
			break;
		case BCME_NOMEM:
			ret = -ENOMEM;
			break;
		case BCME_UNSUPPORTED: /* Unsupported.
			* 1. No TWT support
			* 2. TWT capabilities is not set by device or peer device
			*/
			ret = -EOPNOTSUPP;
			break;
		case BCME_VERSION:
			ret = -ENOEXEC;
			break;
		case BCME_BUFTOOSHORT:
			ret = -ENOBUFS;
			break;
		default:
			ret = -EAGAIN;
			break;
	}

	WL_DBG(("twt wifi_error ret:%d\n", ret));

	return ret;
}

static int
wl_android_twt_setup(struct net_device *ndev, char *command, int total_len)
{
	wl_twt_config_t val;
	s32 bw;
	char *token, *pos;
	u8 mybuf[WLC_IOCTL_SMLEN] = {0};
	u8 resp_buf[WLC_IOCTL_SMLEN] = {0};
	u64 twt;
	uint8 *rem = mybuf;
	uint16 rem_len = sizeof(mybuf);
	int32 val32;

	WL_DBG_MEM(("Enter. cmd:%s\n", command));

	if (strlen(command) == strlen(CMD_TWT_SETUP)) {
		WL_ERR(("Error, twt_setup cmd  missing params\n"));
		bw = -EINVAL;
		goto exit;
	}

	bzero(&val, sizeof(val));
	val.version = WL_TWT_SETUP_VER;
	val.length = sizeof(val.version) + sizeof(val.length);

	/* Default values, Overide Below */
	val.desc.flow_flags = 0;
	val.desc.wake_time_h = 0xFFFFFFFF;
	val.desc.wake_time_l = 0xFFFFFFFF;
	val.desc.wake_int_min = 0xFFFFFFFF;
	val.desc.wake_int_max = 0xFFFFFFFF;
	val.desc.wake_dur_min = 0xFFFFFFFF;
	val.desc.wake_dur_max = 0xFFFFFFFF;
	val.desc.avg_pkt_num  = 0xFFFFFFFF;
	val.desc.avg_pkt_size = 0xFFFFFFFF;

	pos = command + sizeof(CMD_TWT_SETUP);

	/* Config ID */
	token = strsep((char**)&pos, " ");
	if (!token) {
		WL_ERR(("Mandatory param ConfigID not present\n"));
		bw = -EINVAL;
		goto exit;
	}
	val.desc.configID = (u8)bcm_atoi(token);

	/* negotiation_type */
	token = strsep((char**)&pos, " ");
	if (!token) {
		WL_ERR(("Mandatory param negotiation type not present\n"));
		bw = -EINVAL;
		goto exit;
	}
	val.desc.negotiation_type  = (u8)bcm_atoi(token);

	/* Flow Type */
	token = strsep((char**)&pos, " ");
	if (!token) {
		WL_ERR(("Mandatory param Flow Type not present\n"));
		bw = -EINVAL;
		goto exit;
	}
	if (htod32((u32)bcm_atoi(token)) == 1) {
		val.desc.flow_flags |= WL_TWT_FLOW_FLAG_UNANNOUNCED;
	}

	/* Trigger Type */
	token = strsep((char**)&pos, " ");
	if (!token) {
		WL_ERR(("Mandatory param Trigger Type not present\n"));
		bw = -EINVAL;
		goto exit;
	}
	if (htod32((u32)bcm_atoi(token)) == 1) {
		val.desc.flow_flags |= WL_TWT_FLOW_FLAG_TRIGGER;
	}

	/* Wake Duration */
	token = strsep((char**)&pos, " ");
	if (!token) {
		WL_ERR(("Mandatory param wake Duration not present\n"));
		bw = -EINVAL;
		goto exit;
	}
	val.desc.wake_dur = htod32((u32)bcm_atoi(token));

	/* Wake interval */
	token = strsep((char**)&pos, " ");
	if (!token) {
		WL_ERR(("Mandaory param Wake Interval not present\n"));
		bw = -EINVAL;
		goto exit;
	}
	val.desc.wake_int = htod32((u32)bcm_atoi(token));

	/* Wake Time parameter */
	token = strsep((char**)&pos, " ");
	if (!token) {
		WL_ERR(("No Wake Time parameter provided, using default\n"));
	} else {
		twt = (u64)bcm_atoi(token);
		val32 = htod32((u32)(twt >> 32));
		if ((val32 != -1) && ((int32)(htod32((u32)twt)) != -1)) {
			val.desc.wake_time_h = htod32((u32)(twt >> 32));
			val.desc.wake_time_l = htod32((u32)twt);
		}
	}

	/* Minimum allowed Wake interval */
	token = strsep((char**)&pos, " ");
	if (!token) {
		WL_ERR(("No Minimum allowed Wake interval provided, using default\n"));
	} else {
		val32 = htod32((u32)bcm_atoi(token));
		if (val32 != -1) {
			val.desc.wake_int_min = htod32((u32)bcm_atoi(token));
		}
	}

	/* Max Allowed Wake interval */
	token = strsep((char**)&pos, " ");
	if (!token) {
		WL_ERR(("Maximum allowed Wake interval not provided, using default\n"));
	} else {
		val32 = htod32((u32)bcm_atoi(token));
		if (val32 != -1) {
			val.desc.wake_int_max = htod32((u32)bcm_atoi(token));
		}
	}

	/* Minimum allowed Wake duration */
	token = strsep((char**)&pos, " ");
	if (!token) {
		WL_ERR(("Maximum allowed Wake duration not provided, using default\n"));
	} else {
		val32 = htod32((u32)bcm_atoi(token));
		if (val32 != -1) {
			val.desc.wake_dur_min = htod32((u32)bcm_atoi(token));
		}
	}

	/* Maximum allowed Wake duration */
	token = strsep((char**)&pos, " ");
	if (!token) {
		WL_ERR(("Maximum allowed Wake duration not provided, using default\n"));
	} else {
		val32 = htod32((u32)bcm_atoi(token));
		if (val32 != -1) {
			val.desc.wake_dur_max = htod32((u32)bcm_atoi(token));
		}
	}

	/* Average number of packets */
	token = strsep((char**)&pos, " ");
	if (!token) {
		WL_ERR(("Average number of packets not provided, using default\n"));
	} else {
		val32 = htod32((u32)bcm_atoi(token));
		if (val32 != -1) {
			val.desc.avg_pkt_num  = htod32((u32)bcm_atoi(token));
		}
	}

	/* Average packets size */
	token = strsep((char**)&pos, " ");
	if (!token) {
		WL_ERR(("Average packets size not provided, using default\n"));
	} else {
		val32 = htod32((u32)bcm_atoi(token));
		if (val32 != -1) {
			val.desc.avg_pkt_size = htod32((u32)bcm_atoi(token));
		}
	}

	bw = bcm_pack_xtlv_entry(&rem, &rem_len, WL_TWT_CMD_CONFIG,
			sizeof(val), (uint8 *)&val, BCM_XTLV_OPTION_ALIGN32);
	if (bw != BCME_OK) {
		goto exit;
	}

	bw = wldev_iovar_setbuf(ndev, "twt",
		mybuf, sizeof(mybuf) - rem_len, resp_buf, WLC_IOCTL_SMLEN, NULL);
	if (bw < 0) {
		WL_ERR(("twt config set failed. ret:%d\n", bw));
	}
exit:
	return wl_android_twt_bcmerr_to_kernel_err(bw);
}

static int
wl_android_twt_display_cap(wl_twt_cap_t *result, char *command, int total_len)
{
	int rem_len = 0, bytes_written = 0;

	rem_len = total_len;

	bytes_written = scnprintf(command, rem_len, "0x%04x%04x\n",
		result->device_cap, result->peer_cap);
	CHECK_SCNPRINTF_RET_VAL(bytes_written);
	command += bytes_written;
	rem_len -= bytes_written;

	WL_DBG(("Device TWT Capabilities:\n"));
	WL_DBG(("Requester Support %d, \t",
		!!(result->device_cap & WL_TWT_CAP_FLAGS_REQ_SUPPORT)));
	WL_DBG(("Responder Support %d, \t",
		!!(result->device_cap & WL_TWT_CAP_FLAGS_RESP_SUPPORT)));
	WL_DBG(("Broadcast TWT Support %d, \t",
		!!(result->device_cap & WL_TWT_CAP_FLAGS_BTWT_SUPPORT)));
	WL_DBG(("Flexible TWT Support %d, \t",
		!!(result->device_cap & WL_TWT_CAP_FLAGS_FLEX_SUPPORT)));
	WL_DBG(("TWT Required by peer %d, \n",
		!!(result->device_cap & WL_TWT_CAP_FLAGS_TWT_REQUIRED)));
	/* Peer capabilities */
	WL_DBG(("\nPeer TWT Capabilities:\n"));
	WL_DBG(("Requester Support %d, \t",
		!!(result->peer_cap & WL_TWT_CAP_FLAGS_REQ_SUPPORT)));
	WL_DBG(("Responder Support %d, \t",
		!!(result->peer_cap & WL_TWT_CAP_FLAGS_RESP_SUPPORT)));
	WL_DBG(("Broadcast TWT Support %d, \t",
		!!(result->peer_cap & WL_TWT_CAP_FLAGS_BTWT_SUPPORT)));
	WL_DBG(("Flexible TWT Support %d, \t",
		!!(result->peer_cap & WL_TWT_CAP_FLAGS_FLEX_SUPPORT)));
	WL_DBG(("TWT Required by peer %d, \n",
		!!(result->peer_cap & WL_TWT_CAP_FLAGS_TWT_REQUIRED)));
	WL_DBG(("\t-----------------------------------------------------------------\n\n"));

	if ((total_len - rem_len) > 0) {
		return (total_len - rem_len);
	} else {
		return BCME_ERROR;
	}
}

static int
wl_android_twt_cap(struct net_device *dev, char *command, int total_len)
{
	int ret = BCME_OK;
	char iovbuf[WLC_IOCTL_SMLEN] = {0, };
	uint8 *pxtlv = NULL;
	uint8 *iovresp = NULL;
	wl_twt_cap_cmd_t cmd_cap;
	wl_twt_cap_t result;

	uint16 buflen = 0, bufstart = 0;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);

	bzero(&cmd_cap, sizeof(cmd_cap));

	cmd_cap.version = WL_TWT_CAP_CMD_VERSION_1;
	cmd_cap.length = sizeof(cmd_cap) - OFFSETOF(wl_twt_cap_cmd_t, peer);

	iovresp = (uint8 *)MALLOCZ(cfg->osh, WLC_IOCTL_MEDLEN);
	if (iovresp == NULL) {
		DHD_ERROR(("%s: iov resp memory alloc exited\n", __FUNCTION__));
		goto exit;
	}

	buflen = bufstart = WLC_IOCTL_SMLEN;
	pxtlv = (uint8 *)iovbuf;

	ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_TWT_CMD_CAP,
			sizeof(cmd_cap), (uint8 *)&cmd_cap, BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		DHD_ERROR(("%s : Error return during pack xtlv :%d\n", __FUNCTION__, ret));
		goto exit;
	}

	if ((ret = wldev_iovar_getbuf(dev, "twt", iovbuf, bufstart-buflen,
		iovresp, WLC_IOCTL_MEDLEN, NULL))) {
		DHD_ERROR(("Getting twt status failed with err=%d \n", ret));
		goto exit;
	}
	if (ret) {
		DHD_ERROR(("%s : Error return during twt iovar set :%d\n", __FUNCTION__, ret));
	}

	(void)memcpy_s(&result, sizeof(result), iovresp, sizeof(result));

	if (dtoh16(result.version) == WL_TWT_CAP_CMD_VERSION_1) {
		DHD_ERROR(("capability ver %d, \n", dtoh16(result.version)));
		ret = wl_android_twt_display_cap(&result, command, total_len);
		return ret;
	} else {
		ret = BCME_UNSUPPORTED;
		DHD_ERROR(("Version 1 unsupported. ver %d, \n", dtoh16(result.version)));
		goto exit;
	}

exit:
	if (iovresp) {
		MFREE(cfg->osh, iovresp, WLC_IOCTL_MEDLEN);
	}

	return ret;
}

static int
wl_android_twt_status_display_v1(wl_twt_status_v1_t *status, char *command, int total_len)
{
	uint i;
	wl_twt_sdesc_v0_t *desc = NULL;
	u64 wake_time = 0;
	int rem_len = 0, bytes_written = 0;

	rem_len = total_len;

	WL_DBG(("\nNumber of Individual TWTs: %d\n", status->num_fid));
	WL_DBG(("Number of Broadcast TWTs: %d\n", status->num_bid));
	WL_DBG(("TWT SPPS Enabled %d \t STA Wake Status %d \t Wake Override %d\n",
		!!(status->status_flags & WL_TWT_STATUS_FLAG_SPPS_ENAB),
		!!(status->status_flags & WL_TWT_STATUS_FLAG_WAKE_STATE),
		!!(status->status_flags & WL_TWT_STATUS_FLAG_WAKE_OVERRIDE)));
	WL_DBG(("\t---------------- Individual TWT list-------------------\n"));

	for (i = 0; i < WL_TWT_MAX_ITWT; i ++) {
		WL_DBG(("Config ID %d\n", status->itwt_status[i].configID));
		if (status->itwt_status[i].configID != WL_TWT_INV_CONFIG_ID) {
			desc = &status->itwt_status[i].desc;
			bytes_written = scnprintf(command, rem_len,
				"%d %d %u %u\n", status->itwt_status[i].configID,
				status->itwt_status[i].state, desc->wake_dur,
				desc->wake_int);
			CHECK_SCNPRINTF_RET_VAL(bytes_written);
			command += bytes_written;
			rem_len -= bytes_written;

			WL_DBG(("Config ID %d \tFlow ID %d \tState %d\t",
				status->itwt_status[i].configID,
				desc->flow_id,
				status->itwt_status[i].state));
			WL_DBG(("peer: "MACF"\n",
				ETHER_TO_MACF(status->itwt_status[i].peer)));
			WL_DBG(("Unannounced %d\tTriggered %d\tProtection %d\t"
				"Info Frame Disabled %d\n",
				!!(desc->flow_flags & WL_TWT_FLOW_FLAG_UNANNOUNCED),
				!!(desc->flow_flags & WL_TWT_FLOW_FLAG_TRIGGER),
				!!(desc->flow_flags & WL_TWT_FLOW_FLAG_PROTECT),
				!!(desc->flow_flags & WL_TWT_FLOW_FLAG_INFO_FRM_DISABLED)));
			WL_DBG(("target wake time: %llu\t", wake_time));
			WL_DBG(("wake duration: %u\t", desc->wake_dur));
			WL_DBG(("wake interval: %u\t", desc->wake_int));
			WL_DBG(("TWT channel: %u\n", desc->channel));
			WL_DBG(("AvgPktNum: %u\tAvgPktSize: %u\n",
				status->itwt_status[i].avg_pkt_num,
				status->itwt_status[i].avg_pkt_size));
		}
	}

	WL_DBG(("\t---------------- Broadcast TWT list-------------------\n"));
	for (i = 0; i < WL_TWT_MAX_BTWT; i ++) {
		if (status->btwt_status[i].configID != WL_TWT_INV_CONFIG_ID) {
			desc = &status->btwt_status[i].desc;
			bytes_written = scnprintf(command, rem_len,
				"%d %d %u %u\n", status->btwt_status[i].configID,
				status->btwt_status[i].state, desc->wake_dur,
				desc->wake_int);
			CHECK_SCNPRINTF_RET_VAL(bytes_written);
			command += bytes_written;
			rem_len -= bytes_written;

			WL_DBG(("Config ID %d \tBroadcast ID %d \tState %d\t",
				status->btwt_status[i].configID,
				desc->bid, status->btwt_status[i].state));
			WL_DBG(("peer: "MACF"\n",
				ETHER_TO_MACF(status->btwt_status[i].peer)));
			WL_DBG(("Unannounced %d\tTriggered %d\tProtection %d\t"
				"Info Frame Disabled %d\t",
				!!(desc->flow_flags & WL_TWT_FLOW_FLAG_UNANNOUNCED),
				!!(desc->flow_flags & WL_TWT_FLOW_FLAG_TRIGGER),
				!!(desc->flow_flags & WL_TWT_FLOW_FLAG_PROTECT),
				!!(desc->flow_flags & WL_TWT_FLOW_FLAG_INFO_FRM_DISABLED)));
			WL_DBG(("Frame Recommendation %d\tBTWT Persistence %d\n",
				desc->frame_recomm, desc->btwt_persistence));
			WL_DBG(("target wake time: %llu\t", wake_time));
			WL_DBG(("wake duration: %u\t", desc->wake_dur));
			WL_DBG(("wake interval: %u\t", desc->wake_int));
			WL_DBG(("TWT channel: %u\n", desc->channel));
			WL_DBG(("AvgPktNum: %u\tAvgPktSize: %u\n",
				status->btwt_status[i].avg_pkt_num,
				status->btwt_status[i].avg_pkt_size));
		}
	}

	if ((total_len - rem_len) > 0) {
		return (total_len - rem_len);
	} else {
		return BCME_OK;
	}
}

static int
wl_android_twt_status_query(struct net_device *dev, char *command, int total_len)
{
	int ret = BCME_OK;
	char iovbuf[WLC_IOCTL_SMLEN] = {0, };
	uint8 *pxtlv = NULL;
	uint8 *iovresp = NULL;
	wl_twt_status_cmd_v1_t status_cmd;
	wl_twt_status_v1_t result;
	char *token, *pos;

	uint16 buflen = 0, bufstart = 0;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);

	bzero(&status_cmd, sizeof(status_cmd));

	status_cmd.version = WL_TWT_CMD_STATUS_VERSION_1;
	status_cmd.length = sizeof(status_cmd) - OFFSETOF(wl_twt_status_cmd_v1_t, peer);

	pos = command + sizeof(CMD_TWT_STATUS_QUERY);
	/* Config ID */
	token = strsep((char**)&pos, " ");
	if (!token) {
		WL_ERR(("Mandatory param config ID not present\n"));
		ret = -EINVAL;
		goto exit;
	}
	status_cmd.configID = (u8)bcm_atoi(token);

	iovresp = (uint8 *)MALLOCZ(cfg->osh, WLC_IOCTL_MEDLEN);
	if (iovresp == NULL) {
		DHD_ERROR(("%s: iov resp memory alloc exited\n", __FUNCTION__));
		goto exit;
	}

	buflen = bufstart = WLC_IOCTL_SMLEN;
	pxtlv = (uint8 *)iovbuf;

	ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_TWT_CMD_STATUS,
			sizeof(status_cmd), (uint8 *)&status_cmd, BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		DHD_ERROR(("%s : Error return during pack xtlv :%d\n", __FUNCTION__, ret));
		goto exit;
	}

	if ((ret = wldev_iovar_getbuf(dev, "twt", iovbuf, bufstart-buflen,
		iovresp, WLC_IOCTL_MEDLEN, NULL))) {
		DHD_ERROR(("Getting twt status failed with err=%d \n", ret));
		goto exit;
	}
	if (ret) {
		DHD_ERROR(("%s : Error return during twt iovar set :%d\n", __FUNCTION__, ret));
	}

	(void)memcpy_s(&result, sizeof(result), iovresp, sizeof(result));

	if (dtoh16(result.version) == WL_TWT_CMD_STATUS_VERSION_1) {
		DHD_ERROR(("status query ver %d, \n", dtoh16(result.version)));
		ret = wl_android_twt_status_display_v1(&result, command, total_len);
		return ret;
	} else {
		ret = BCME_UNSUPPORTED;
		DHD_ERROR(("Version 1 unsupported. ver %d, \n", dtoh16(result.version)));
		goto exit;
	}

exit:
	if (iovresp) {
		MFREE(cfg->osh, iovresp, WLC_IOCTL_MEDLEN);
	}

	return ret;
}

static int
wl_android_twt_info(struct net_device *ndev, char *command, int total_len)
{
	wl_twt_info_t val;
	s32 bw;
	char *token, *pos;
	u8 mybuf[WLC_IOCTL_SMLEN] = {0};
	u8 res_buf[WLC_IOCTL_SMLEN] = {0};
	u64 twt;
	uint8 *rem = mybuf;
	uint16 rem_len = sizeof(mybuf);
	int32 val32;

	WL_DBG_MEM(("Enter. cmd:%s\n", command));

	if (strlen(command) == strlen(CMD_TWT_INFO)) {
		WL_ERR(("Error, twt teardown cmd missing params\n"));
		bw = -EINVAL;
		goto exit;
	}

	bzero(&val, sizeof(val));
	val.version = WL_TWT_INFO_VER;
	val.length = sizeof(val.version) + sizeof(val.length);

	/* Default values, Overide Below */
	val.infodesc.flow_id = 0xFF;
	val.desc.next_twt_h = 0xFFFFFFFF;
	val.desc.next_twt_l = 0xFFFFFFFF;

	pos = command + sizeof(CMD_TWT_TEARDOWN);

	/* Config ID */
	token = strsep((char**)&pos, " ");
	if (!token) {
		WL_ERR(("Mandatory param config ID not present\n"));
		bw = -EINVAL;
		goto exit;
	}
	val.configID = (u8)bcm_atoi(token);

	/* resume offset */
	token = strsep((char**)&pos, " ");
	if (!token) {
		WL_ERR(("resume offset not provided, using default\n"));
	} else {
		twt = (u64)bcm_atoi(token);
		val32 = htod32((u32)(twt >> 32));
		if (!(((val32 == -1) && ((int32)(htod32((u32)twt)) == -1)) ||
			((val32 == 0) && ((int32)(htod32((u32)twt)) == 0)))) {
			val.infodesc.next_twt_h = htod32((u32)(twt >> 32));
			val.infodesc.next_twt_l = htod32((u32)twt);
			val.infodesc.flow_flags |= WL_TWT_INFO_FLAG_RESUME;
		}
	}

	bw = bcm_pack_xtlv_entry(&rem, &rem_len, WL_TWT_CMD_INFO,
		sizeof(val), (uint8 *)&val, BCM_XTLV_OPTION_ALIGN32);
	if (bw != BCME_OK) {
		goto exit;
	}

	bw = wldev_iovar_setbuf(ndev, "twt",
		mybuf, sizeof(mybuf) - rem_len, res_buf, WLC_IOCTL_SMLEN, NULL);
	if (bw < 0) {
		WL_ERR(("twt info failed. ret:%d\n", bw));
	}
exit:
	return wl_android_twt_bcmerr_to_kernel_err(bw);

}

static int
wl_android_twt_teardown(struct net_device *ndev, char *command, int total_len)
{
	wl_twt_teardown_t val;
	s32 bw;
	char *token, *pos;
	u8 mybuf[WLC_IOCTL_SMLEN] = {0};
	u8 res_buf[WLC_IOCTL_SMLEN] = {0};
	uint8 *rem = mybuf;
	uint16 rem_len = sizeof(mybuf);

	WL_DBG_MEM(("Enter. cmd:%s\n", command));

	if (strlen(command) == strlen(CMD_TWT_TEARDOWN)) {
		WL_ERR(("Error, twt teardown cmd missing params\n"));
		bw = -EINVAL;
		goto exit;
	}

	bzero(&val, sizeof(val));
	val.version = WL_TWT_TEARDOWN_VER;
	val.length = sizeof(val.version) + sizeof(val.length);

	/* Default values, Overide Below */
	val.teardesc.flow_id = 0xFF;
	val.teardesc.bid = 0xFF;

	pos = command + sizeof(CMD_TWT_TEARDOWN);

	/* Config ID */
	token = strsep((char**)&pos, " ");
	if (!token) {
		WL_ERR(("Mandatory param config ID not present\n"));
		bw = -EINVAL;
		goto exit;
	}
	val.configID = (u8)bcm_atoi(token);

	/* negotiation_type */
	token = strsep((char**)&pos, " ");
	if (!token) {
		WL_ERR(("Mandatory param negotiation type not present\n"));
		bw = -EINVAL;
		goto exit;
	}
	val.teardesc.negotiation_type  = htod32((u32)bcm_atoi(token));

	bw = bcm_pack_xtlv_entry(&rem, &rem_len, WL_TWT_CMD_TEARDOWN,
		sizeof(val), (uint8 *)&val, BCM_XTLV_OPTION_ALIGN32);
	if (bw != BCME_OK) {
		goto exit;
	}

	bw = wldev_iovar_setbuf(ndev, "twt",
		mybuf, sizeof(mybuf) - rem_len, res_buf, WLC_IOCTL_SMLEN, NULL);
	if (bw < 0) {
		WL_ERR(("twt teardown failed. ret:%d\n", bw));
	}
exit:
	return wl_android_twt_bcmerr_to_kernel_err(bw);
}

/* wl twt stats result display version 2 */
static int
wl_android_twt_stats_display_v2(wl_twt_stats_v2_t *stats, char *command, int total_len)
{
	u32 i;
	wl_twt_peer_stats_v2_t *peer_stats;
	int rem_len = 0, bytes_written = 0;

	rem_len = total_len;
	for (i = 0; i < stats->num_stats; i++) {
		peer_stats = &stats->peer_stats_list[i];

		bytes_written = scnprintf(command, rem_len,
			"%u %u %u %u %u",
			peer_stats->eosp_dur_avg, peer_stats->tx_pkts_avg, peer_stats->rx_pkts_avg,
			peer_stats->tx_pkt_sz_avg, peer_stats->rx_pkt_sz_avg);
		CHECK_SCNPRINTF_RET_VAL(bytes_written);
		command += bytes_written;
		rem_len -= bytes_written;
	}

	if ((total_len - rem_len) > 0) {
		return (total_len - rem_len);
	} else {
		return BCME_ERROR;
	}
}

static int
wl_android_twt_stats(struct net_device *ndev, char *command, int total_len)
{
	wl_twt_stats_cmd_v1_t query;
	wl_twt_stats_v2_t stats_v2;
	int ret = BCME_OK;
	char iovbuf[WLC_IOCTL_SMLEN] = {0, };
	uint8 *pxtlv = NULL;
	uint8 *iovresp = NULL;
	char *token, *pos;
	uint16 buflen = 0, bufstart = 0;
	struct bcm_cfg80211 *cfg = wl_get_cfg(ndev);

	WL_DBG_MEM(("Enter. cmd:%s\n", command));

	bzero(&query, sizeof(query));
	query.version = WL_TWT_STATS_CMD_VERSION_1;
	query.length = sizeof(query) - OFFSETOF(wl_twt_stats_cmd_v1_t, peer);

	/* Default values, Overide Below */
	query.num_bid = 0xFF;
	query.num_fid = 0xFF;

	if (!(strnicmp(command, CMD_TWT_CLR_STATS, strlen(CMD_TWT_CLR_STATS)))) {
		query.flags |= WL_TWT_STATS_CMD_FLAGS_RESET;
		pos = command + sizeof(CMD_TWT_CLR_STATS);
	} else if (!(strnicmp(command, CMD_TWT_GET_STATS, strlen(CMD_TWT_GET_STATS)))) {
		pos = command + sizeof(CMD_TWT_GET_STATS);
	}

	/* Config ID */
	token = strsep((char**)&pos, " ");
	if (!token) {
		WL_ERR(("Mandatory param config ID not present\n"));
		ret = -EINVAL;
		goto exit;
	}
	query.configID = (u8)bcm_atoi(token);

	iovresp = (uint8 *)MALLOCZ(cfg->osh, WLC_IOCTL_MEDLEN);
	if (iovresp == NULL) {
		DHD_ERROR(("%s: iov resp memory alloc exited\n", __FUNCTION__));
		goto exit;
	}

	buflen = bufstart = WLC_IOCTL_SMLEN;
	pxtlv = (uint8 *)iovbuf;
	ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_TWT_CMD_STATS,
			sizeof(query), (uint8 *)&query, BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		DHD_ERROR(("%s : Error return during pack xtlv :%d\n", __FUNCTION__, ret));
		goto exit;
	}

	if ((ret = wldev_iovar_getbuf(ndev, "twt", iovbuf, bufstart-buflen,
		iovresp, WLC_IOCTL_MEDLEN, NULL))) {
		DHD_ERROR(("twt status failed with err=%d \n", ret));
		goto exit;
	}

	(void)memcpy_s(&stats_v2, sizeof(stats_v2), iovresp, sizeof(stats_v2));

	if (dtoh16(stats_v2.version) == WL_TWT_STATS_VERSION_2) {
		if (!(strnicmp(command, CMD_TWT_GET_STATS, strlen(CMD_TWT_GET_STATS)))) {
			DHD_ERROR(("stats query ver %d, \n", dtoh16(stats_v2.version)));
			ret = wl_android_twt_stats_display_v2((wl_twt_stats_v2_t*)iovresp,
				command, total_len);
		}
	} else {
		ret = BCME_UNSUPPORTED;
		DHD_ERROR(("Version 1 unsupported. ver %d, \n", dtoh16(stats_v2.version)));
		goto exit;
	}

exit:
	if (iovresp) {
		MFREE(cfg->osh, iovresp, WLC_IOCTL_MEDLEN);
	}

	return ret;
}

static int
wl_android_twt_softap_enable(struct net_device *ndev, int twt_softap_enable)
{
	s32 err = 0;
	struct bcm_cfg80211 *cfg = NULL;
	u8 mybuf[WLC_IOCTL_SMLEN] = {0};
	u8 res_buf[WLC_IOCTL_SMLEN] = {0};
	u8 *rem = mybuf;
	u16 rem_len = sizeof(mybuf);
	wl_twt_resp_cfg_t val;

	if (!ndev) {
		err = -EINVAL;
		return err;
	}

	cfg = wl_get_cfg(ndev);
	if (!cfg) {
		err = -EINVAL;
		return err;
	}

	/* Enable/disable SoftAP for TWT */
	bzero(&val, sizeof(val));
	val.version = WL_TWT_RESP_CFG_VER;
	val.length = sizeof(val.version) + sizeof(val.length);

	if (twt_softap_enable == 1) {
		val.twt_resp_enab = 1;
	}

	err = bcm_pack_xtlv_entry(&rem, &rem_len, WL_TWT_CMD_RESP_CONFIG,
		sizeof(val), (uint8 *)&val, BCM_XTLV_OPTION_ALIGN32);
	if (err != BCME_OK) {
		goto exit;
	}

	err = wldev_iovar_setbuf(ndev, "twt",
		mybuf, sizeof(mybuf) - rem_len, res_buf, WLC_IOCTL_SMLEN, NULL);
	if (err < 0) {
		WL_ERR(("twt softAP enable/disable failed. ret:%d\n", err));
	}

exit:
	return wl_android_twt_bcmerr_to_kernel_err(err);
}
#endif /* WL_TWT */

static int
wl_android_get_6g_softap_freq_list(struct net_device *ndev, char *command, int total_len)
{
	struct bcm_cfg80211 *cfg = wl_get_cfg(ndev);
	int err = BCME_OK;
	uint16 *chan_list = NULL;
	uint32 i, freq, list_count;
	uint32 chspec, chaninfo;
	char *input_command = NULL;
	int rem_len = 0, bytes_written = 0;

	rem_len = total_len;
	input_command = command;

	chan_list = MALLOCZ(cfg->osh, CHANINFO_LIST_BUF_SIZE);
	if (chan_list == NULL) {
		WL_ERR(("failed to allocate chanspec buffer\n"));
		return -ENOMEM;
	}
	err = wldev_iovar_getbuf_bsscfg(ndev, "chan_info_list", NULL,
		0, chan_list, CHANINFO_LIST_BUF_SIZE, 0, NULL);
	if (err == BCME_UNSUPPORTED) {
		WL_INFORM(("get chan_info_list, UNSUPPORTED\n"));
		goto exit;
	} else if (err != BCME_OK) {
		WL_ERR(("get chan_info_list err(%d)\n", err));
		goto exit;
	}

	list_count = ((wl_chanspec_list_v1_t *)chan_list)->count;
	for (i = 0; i < list_count; i++) {
		chspec = dtoh32(((wl_chanspec_list_v1_t *)chan_list)->chspecs[i].chanspec);

		/* Check if the chanspec is 6G, PSC and VLP */
		if (CHSPEC_IS6G(chspec) && CHSPEC_IS20(chspec)) {
			chaninfo =
				dtoh32(((wl_chanspec_list_v1_t *)chan_list)->chspecs[i].chaninfo);
			if ((chaninfo & WL_CHAN_BAND_6G_PSC) && (chaninfo & WL_CHAN_BAND_6G_VLP)) {
				freq = wl_channel_to_frequency(wf_chspec_ctlchan(chspec),
					CHSPEC_BAND(chspec));
				bytes_written = scnprintf(command, rem_len, "%d ", freq);
				CHECK_SCNPRINTF_RET_VAL(bytes_written);
				command += bytes_written;
				rem_len -= bytes_written;
			}
		}
	}

	if ((total_len - rem_len) >= 0) {
		err = total_len - rem_len;
		WL_INFORM_MEM(("6G SoftAP FreqList: %s \n", input_command));
	} else {
		err = BCME_ERROR;
	}
exit:
	if (chan_list) {
		MFREE(cfg->osh, chan_list, CHANINFO_LIST_BUF_SIZE);
	}
	return err;
}

#ifdef WL_DUAL_STA
static int
wl_android_set_primary_inet(struct net_device *ndev, char *command, int total_len)
{
	int err = BCME_OK;

	err = wl_cfgvendor_multista_set_primary_connection(ndev->ieee80211_ptr->wiphy,
		ndev_to_wdev(ndev), NULL, 0);

	return err;
}
#endif /* WL_DUAL_STA */

int
wl_android_set_wsec_info(struct net_device *dev, char *command)
{
	int error = BCME_OK;
	uint32 wsec_info;
#ifdef DHD_PM_CONTROL_FROM_FILE
	if (g_pm_control) {
		WL_ERR(("Ignored Set wsec_info\n"));
		return BCME_UNSUPPORTED;
	}
#endif	/* DHD_PM_CONTROL_FROM_FILE */

	if (sscanf(command, "%*s %d", &wsec_info) != 1) {
		WL_ERR(("Failed to get Parameter\n"));
		return BCME_ERROR;
	}

	if ((wsec_info != 0) && (wsec_info != 1)) {
		WL_ERR(("Invalid wsec info value %d\n", wsec_info));
		return BCME_ERROR;
	}

	error = wl_cfg80211_set_wsec_info(dev, &wsec_info, sizeof(wsec_info),
		WL_WSEC_INFO_6G_LEGACY_SEC);
	if (error) {
		WL_ERR(("Failed to set wsec info %d, error = %d\n", wsec_info, error));
	} else {
		WL_ERR(("Succeeded to set wsec info %d,error = %d\n", wsec_info, error));
	}

	return  error;
}

int
wl_handle_private_cmd(struct net_device *net, char *command, u32 cmd_len)
{
	int bytes_written = 0;
	android_wifi_priv_cmd priv_cmd;

	bzero(&priv_cmd, sizeof(android_wifi_priv_cmd));
	priv_cmd.total_len = cmd_len;

	if (strnicmp(command, CMD_START, strlen(CMD_START)) == 0) {
		DHD_INFO(("wl_handle_private_cmd, Received regular START command\n"));
#ifdef SUPPORT_DEEP_SLEEP
		trigger_deep_sleep = 1;
#else
#ifdef  BT_OVER_SDIO
		bytes_written = dhd_net_bus_get(net);
#else
		bytes_written = wl_android_wifi_on(net);
#endif /* BT_OVER_SDIO */
#endif /* SUPPORT_DEEP_SLEEP */
	}
	else if (strnicmp(command, CMD_SETFWPATH, strlen(CMD_SETFWPATH)) == 0) {
		bytes_written = wl_android_set_fwpath(net, command, priv_cmd.total_len);
	}

	if (!g_wifi_on) {
		DHD_ERROR(("wl_handle_private_cmd: Ignore private cmd \"%s\" - iface is down\n",
			command));
		return 0;
	}

	if (strnicmp(command, CMD_STOP, strlen(CMD_STOP)) == 0) {
#ifdef SUPPORT_DEEP_SLEEP
		trigger_deep_sleep = 1;
#else
#ifdef  BT_OVER_SDIO
		bytes_written = dhd_net_bus_put(net);
#else
		bytes_written = wl_android_wifi_off(net, FALSE);
#endif /* BT_OVER_SDIO */
#endif /* SUPPORT_DEEP_SLEEP */
	}
	else if (strnicmp(command, CMD_SCAN_ACTIVE, strlen(CMD_SCAN_ACTIVE)) == 0) {
		wl_cfg80211_set_passive_scan(net, command);
	}
	else if (strnicmp(command, CMD_SCAN_PASSIVE, strlen(CMD_SCAN_PASSIVE)) == 0) {
		wl_cfg80211_set_passive_scan(net, command);
	}
	else if (strnicmp(command, CMD_RSSI, strlen(CMD_RSSI)) == 0) {
		bytes_written = wl_android_get_rssi(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_LINKSPEED, strlen(CMD_LINKSPEED)) == 0) {
		bytes_written = wl_android_get_link_speed(net, command, priv_cmd.total_len);
	}
#ifdef PKT_FILTER_SUPPORT
	else if (strnicmp(command, CMD_RXFILTER_START, strlen(CMD_RXFILTER_START)) == 0) {
		bytes_written = net_os_enable_packet_filter(net, 1);
	}
	else if (strnicmp(command, CMD_RXFILTER_STOP, strlen(CMD_RXFILTER_STOP)) == 0) {
		bytes_written = net_os_enable_packet_filter(net, 0);
	}
	else if (strnicmp(command, CMD_RXFILTER_ADD, strlen(CMD_RXFILTER_ADD)) == 0) {
		int filter_num = *(command + strlen(CMD_RXFILTER_ADD) + 1) - '0';
		bytes_written = net_os_rxfilter_add_remove(net, TRUE, filter_num);
	}
	else if (strnicmp(command, CMD_RXFILTER_REMOVE, strlen(CMD_RXFILTER_REMOVE)) == 0) {
		int filter_num = *(command + strlen(CMD_RXFILTER_REMOVE) + 1) - '0';
		bytes_written = net_os_rxfilter_add_remove(net, FALSE, filter_num);
	}
#endif /* PKT_FILTER_SUPPORT */
	else if (strnicmp(command, CMD_BTCOEXSCAN_START, strlen(CMD_BTCOEXSCAN_START)) == 0) {
		/* TBD: BTCOEXSCAN-START */
	}
	else if (strnicmp(command, CMD_BTCOEXSCAN_STOP, strlen(CMD_BTCOEXSCAN_STOP)) == 0) {
		/* TBD: BTCOEXSCAN-STOP */
	}
	else if (strnicmp(command, CMD_BTCOEXMODE, strlen(CMD_BTCOEXMODE)) == 0) {
#ifdef WL_CFG80211
#ifdef OEM_ANDROID
		void *dhdp = wl_cfg80211_get_dhdp(net);
		bytes_written = wl_cfg80211_set_btcoex_dhcp(net, dhdp, command);
#endif // OEM_ANDROID
#else
#ifdef PKT_FILTER_SUPPORT
		uint mode = *(command + strlen(CMD_BTCOEXMODE) + 1) - '0';

		if (mode == 1)
			net_os_enable_packet_filter(net, 0); /* DHCP starts */
		else
			net_os_enable_packet_filter(net, 1); /* DHCP ends */
#endif /* PKT_FILTER_SUPPORT */
#endif /* WL_CFG80211 */
	}
	else if (strnicmp(command, CMD_SETSUSPENDOPT, strlen(CMD_SETSUSPENDOPT)) == 0) {
		bytes_written = wl_android_set_suspendopt(net, command);
	}
	else if (strnicmp(command, CMD_SETSUSPENDMODE, strlen(CMD_SETSUSPENDMODE)) == 0) {
		bytes_written = wl_android_set_suspendmode(net, command);
	}
	else if (strnicmp(command, CMD_SETDTIM_IN_SUSPEND, strlen(CMD_SETDTIM_IN_SUSPEND)) == 0) {
		bytes_written = wl_android_set_bcn_li_dtim(net, command);
	}
	else if (strnicmp(command, CMD_MAXDTIM_IN_SUSPEND, strlen(CMD_MAXDTIM_IN_SUSPEND)) == 0) {
		bytes_written = wl_android_set_max_dtim(net, command);
	}
#ifdef DISABLE_DTIM_IN_SUSPEND
	else if (strnicmp(command, CMD_DISDTIM_IN_SUSPEND, strlen(CMD_DISDTIM_IN_SUSPEND)) == 0) {
		bytes_written = wl_android_set_disable_dtim_in_suspend(net, command);
	}
#endif /* DISABLE_DTIM_IN_SUSPEND */
	else if (strnicmp(command, CMD_SETBAND, strlen(CMD_SETBAND)) == 0) {
		bytes_written = wl_android_set_band(net, command);
	}
	else if (strnicmp(command, CMD_GETBAND, strlen(CMD_GETBAND)) == 0) {
		bytes_written = wl_android_get_band(net, command, priv_cmd.total_len);
	}
#ifdef WL_CFG80211
#ifndef CUSTOMER_SET_COUNTRY
	/* CUSTOMER_SET_COUNTRY feature is define for only GGSM model */
	else if (strnicmp(command, CMD_COUNTRY, strlen(CMD_COUNTRY)) == 0) {
		/*
		 * Usage examples:
		 * DRIVER COUNTRY US
		 * DRIVER COUNTRY US/7
		 * Wrong revinfo should be filtered:
		 * DRIVER COUNTRY US/-1
		 */
		char *country_code = command + strlen(CMD_COUNTRY) + 1;
		char *rev_info_delim = country_code + 2; /* 2 bytes of country code */
		int revinfo = -1;
#if defined(DHD_BLOB_EXISTENCE_CHECK)
		dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(net);
#endif /* DHD_BLOB_EXISTENCE_CHECK */
		if ((rev_info_delim) &&
			(strnicmp(rev_info_delim, CMD_COUNTRY_DELIMITER,
			strlen(CMD_COUNTRY_DELIMITER)) == 0)) {
			revinfo  = bcm_atoi(rev_info_delim + 1);
		} else {
			revinfo = 0;
		}

		if (revinfo < 0) {
			DHD_ERROR(("%s:failed due to wrong revinfo %d\n", __FUNCTION__, revinfo));
			return BCME_BADARG;
		}

#if defined(DHD_BLOB_EXISTENCE_CHECK)
		if (dhdp->is_blob) {
			revinfo = 0;
		}
#endif /* DHD_BLOB_EXISTENCE_CHECK */

		bytes_written = wl_cfg80211_set_country_code(net, country_code,
			true, true, revinfo);
	}
#endif /* CUSTOMER_SET_COUNTRY */
#endif /* WL_CFG80211 */
	else if (strnicmp(command, CMD_SET_CSA, strlen(CMD_SET_CSA)) == 0) {
		bytes_written = wl_android_set_csa(net, command);
	} else if (strnicmp(command, CMD_80211_MODE, strlen(CMD_80211_MODE)) == 0) {
		bytes_written = wl_android_get_80211_mode(net, command, priv_cmd.total_len);
	} else if (strnicmp(command, CMD_CHANSPEC, strlen(CMD_CHANSPEC)) == 0) {
		bytes_written = wl_android_get_chanspec(net, command, priv_cmd.total_len);
	} else if (strnicmp(command, CMD_DATARATE, strlen(CMD_DATARATE)) == 0) {
		bytes_written = wl_android_get_datarate(net, command, priv_cmd.total_len);
	} else if (strnicmp(command, CMD_ASSOC_CLIENTS,	strlen(CMD_ASSOC_CLIENTS)) == 0) {
		bytes_written = wl_android_get_assoclist(net, command, priv_cmd.total_len);
	}

#ifdef PNO_SUPPORT
	else if (strnicmp(command, CMD_PNOSSIDCLR_SET, strlen(CMD_PNOSSIDCLR_SET)) == 0) {
		bytes_written = dhd_dev_pno_stop_for_ssid(net);
	}
#ifndef WL_SCHED_SCAN
	else if (strnicmp(command, CMD_PNOSETUP_SET, strlen(CMD_PNOSETUP_SET)) == 0) {
		bytes_written = wl_android_set_pno_setup(net, command, priv_cmd.total_len);
	}
#endif /* !WL_SCHED_SCAN */
	else if (strnicmp(command, CMD_PNOENABLE_SET, strlen(CMD_PNOENABLE_SET)) == 0) {
		int enable = *(command + strlen(CMD_PNOENABLE_SET) + 1) - '0';
		bytes_written = (enable)? 0 : dhd_dev_pno_stop_for_ssid(net);
	}
	else if (strnicmp(command, CMD_WLS_BATCHING, strlen(CMD_WLS_BATCHING)) == 0) {
		bytes_written = wls_parse_batching_cmd(net, command, priv_cmd.total_len);
	}
#endif /* PNO_SUPPORT */
	else if (strnicmp(command, CMD_P2P_DEV_ADDR, strlen(CMD_P2P_DEV_ADDR)) == 0) {
		bytes_written = wl_android_get_p2p_dev_addr(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_P2P_SET_NOA, strlen(CMD_P2P_SET_NOA)) == 0) {
		int skip = strlen(CMD_P2P_SET_NOA) + 1;
		bytes_written = wl_cfg80211_set_p2p_noa(net, command + skip,
			priv_cmd.total_len - skip);
	}
#ifdef WL_SDO
	else if (strnicmp(command, CMD_P2P_SD_OFFLOAD, strlen(CMD_P2P_SD_OFFLOAD)) == 0) {
		u8 *buf = command;
		u8 *cmd_id = NULL;
		int len;

		cmd_id = strsep((char **)&buf, " ");
		if (!cmd_id) {
			/* Propagate the error */
			bytes_written = -EINVAL;
		} else {
			/* if buf == NULL, means no arg */
			if (buf == NULL) {
				len = 0;
			} else {
				len = strlen(buf);
			}
			bytes_written = wl_cfg80211_sd_offload(net, cmd_id, buf, len);
		}
	}
#endif /* WL_SDO */
#ifdef P2P_LISTEN_OFFLOADING
	else if (strnicmp(command, CMD_P2P_LISTEN_OFFLOAD, strlen(CMD_P2P_LISTEN_OFFLOAD)) == 0) {
		u8 *sub_command = strchr(command, ' ');
		bytes_written = wl_cfg80211_p2plo_offload(net, command, sub_command,
				sub_command ? strlen(sub_command) : 0);
	}
#endif /* P2P_LISTEN_OFFLOADING */
	else if (strnicmp(command, CMD_P2P_GET_NOA, strlen(CMD_P2P_GET_NOA)) == 0) {
		bytes_written = wl_cfg80211_get_p2p_noa(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_P2P_SET_PS, strlen(CMD_P2P_SET_PS)) == 0) {
		int skip = strlen(CMD_P2P_SET_PS) + 1;
		bytes_written = wl_cfg80211_set_p2p_ps(net, command + skip,
			priv_cmd.total_len - skip);
	}
	else if (strnicmp(command, CMD_P2P_ECSA, strlen(CMD_P2P_ECSA)) == 0) {
		int skip = strlen(CMD_P2P_ECSA) + 1;
		bytes_written = wl_cfg80211_set_p2p_ecsa(net, command + skip,
			priv_cmd.total_len - skip);
	}
	/* This command is not for normal VSDB operation but for only specific P2P operation.
	 * Ex) P2P OTA backup operation
	 */
	else if (strnicmp(command, CMD_P2P_INC_BW, strlen(CMD_P2P_INC_BW)) == 0) {
		int skip = strlen(CMD_P2P_INC_BW) + 1;
		bytes_written = wl_cfg80211_increase_p2p_bw(net,
				command + skip, priv_cmd.total_len - skip);
	}
#ifdef WL_CFG80211
	else if (strnicmp(command, CMD_SET_AP_WPS_P2P_IE,
		strlen(CMD_SET_AP_WPS_P2P_IE)) == 0) {
		int skip = strlen(CMD_SET_AP_WPS_P2P_IE) + 3;
		bytes_written = wl_cfg80211_set_wps_p2p_ie(net, command + skip,
			priv_cmd.total_len - skip, *(command + skip - 2) - '0');
	}
#ifdef WLFBT
	else if (strnicmp(command, CMD_GET_FTKEY, strlen(CMD_GET_FTKEY)) == 0) {
		bytes_written = wl_cfg80211_get_fbt_key(net, command, priv_cmd.total_len);
	}
#endif /* WLFBT */
#endif /* WL_CFG80211 */
#if defined(WL_SUPPORT_AUTO_CHANNEL)
	else if (strnicmp(command, CMD_GET_BEST_CHANNELS,
		strlen(CMD_GET_BEST_CHANNELS)) == 0) {
		bytes_written = wl_android_get_best_channels(net, command,
			priv_cmd.total_len);
	}
#endif /* WL_SUPPORT_AUTO_CHANNEL */
#if defined(WL_SUPPORT_AUTO_CHANNEL)
	else if (strnicmp(command, CMD_SET_HAPD_AUTO_CHANNEL,
		strlen(CMD_SET_HAPD_AUTO_CHANNEL)) == 0) {
		int skip = strlen(CMD_SET_HAPD_AUTO_CHANNEL) + 1;
		bytes_written = wl_android_set_auto_channel(net, (const char*)command+skip, command,
			priv_cmd.total_len);
	}
#endif /* WL_SUPPORT_AUTO_CHANNEL */
	else if (strnicmp(command, CMD_HAPD_MAC_FILTER, strlen(CMD_HAPD_MAC_FILTER)) == 0) {
		int skip = strlen(CMD_HAPD_MAC_FILTER) + 1;
		wl_android_set_mac_address_filter(net, command+skip);
	}
	else if (strnicmp(command, CMD_SETROAMMODE, strlen(CMD_SETROAMMODE)) == 0)
		bytes_written = wl_android_set_roam_mode(net, command);
#if defined(BCMFW_ROAM_ENABLE)
	else if (strnicmp(command, CMD_SET_ROAMPREF, strlen(CMD_SET_ROAMPREF)) == 0) {
		bytes_written = wl_android_set_roampref(net, command, priv_cmd.total_len);
	}
#endif /* BCMFW_ROAM_ENABLE */
	else if (strnicmp(command, CMD_MIRACAST, strlen(CMD_MIRACAST)) == 0)
		bytes_written = wl_android_set_miracast(net, command);
	else if (strnicmp(command, CMD_SETIBSSBEACONOUIDATA, strlen(CMD_SETIBSSBEACONOUIDATA)) == 0)
		bytes_written = wl_android_set_ibss_beacon_ouidata(net,
		command, priv_cmd.total_len);
	else if (strnicmp(command, CMD_KEEP_ALIVE, strlen(CMD_KEEP_ALIVE)) == 0) {
		int skip = strlen(CMD_KEEP_ALIVE) + 1;
		bytes_written = wl_keep_alive_set(net, command + skip);
	}
	else if (strnicmp(command, CMD_ROAM_OFFLOAD, strlen(CMD_ROAM_OFFLOAD)) == 0) {
		int enable = *(command + strlen(CMD_ROAM_OFFLOAD) + 1) - '0';
		bytes_written = wl_cfg80211_enable_roam_offload(net, enable);
	}
	else if (strnicmp(command, CMD_INTERFACE_CREATE, strlen(CMD_INTERFACE_CREATE)) == 0) {
		char *name = (command + strlen(CMD_INTERFACE_CREATE) +1);
		WL_INFORM(("Creating %s interface\n", name));
		if (wl_cfg80211_add_if(wl_get_cfg(net), net, WL_IF_TYPE_STA,
				name, NULL) == NULL) {
			bytes_written = -ENODEV;
		} else {
			/* Return success */
			bytes_written = 0;
		}
	}
	else if (strnicmp(command, CMD_INTERFACE_DELETE, strlen(CMD_INTERFACE_DELETE)) == 0) {
		char *name = (command + strlen(CMD_INTERFACE_DELETE) +1);
		WL_INFORM(("Deleteing %s interface\n", name));
		bytes_written = wl_cfg80211_del_if(wl_get_cfg(net), net, NULL, name);
	}
	else if (strnicmp(command, CMD_GET_LINK_STATUS, strlen(CMD_GET_LINK_STATUS)) == 0) {
		bytes_written = wl_android_get_link_status(net, command, priv_cmd.total_len);
	}
#ifdef P2PRESP_WFDIE_SRC
	else if (strnicmp(command, CMD_P2P_SET_WFDIE_RESP,
		strlen(CMD_P2P_SET_WFDIE_RESP)) == 0) {
		int mode = *(command + strlen(CMD_P2P_SET_WFDIE_RESP) + 1) - '0';
		bytes_written = wl_android_set_wfdie_resp(net, mode);
	} else if (strnicmp(command, CMD_P2P_GET_WFDIE_RESP,
		strlen(CMD_P2P_GET_WFDIE_RESP)) == 0) {
		bytes_written = wl_android_get_wfdie_resp(net, command, priv_cmd.total_len);
	}
#endif /* P2PRESP_WFDIE_SRC */
	else if (strnicmp(command, CMD_DFS_AP_MOVE, strlen(CMD_DFS_AP_MOVE)) == 0) {
		char *data = (command + strlen(CMD_DFS_AP_MOVE) +1);
		bytes_written = wl_cfg80211_dfs_ap_move(net, data, command, priv_cmd.total_len);
	}
#ifdef WBTEXT
	else if (strnicmp(command, CMD_WBTEXT_ENABLE, strlen(CMD_WBTEXT_ENABLE)) == 0) {
		bytes_written = wl_android_wbtext(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_WBTEXT_PROFILE_CONFIG,
			strlen(CMD_WBTEXT_PROFILE_CONFIG)) == 0) {
		char *data = (command + strlen(CMD_WBTEXT_PROFILE_CONFIG) + 1);
		bytes_written = wl_cfg80211_wbtext_config(net, data, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_WBTEXT_WEIGHT_CONFIG,
			strlen(CMD_WBTEXT_WEIGHT_CONFIG)) == 0) {
		char *data = (command + strlen(CMD_WBTEXT_WEIGHT_CONFIG) + 1);
		bytes_written = wl_cfg80211_wbtext_weight_config(net, data,
				command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_WBTEXT_TABLE_CONFIG,
			strlen(CMD_WBTEXT_TABLE_CONFIG)) == 0) {
		char *data = (command + strlen(CMD_WBTEXT_TABLE_CONFIG) + 1);
		bytes_written = wl_cfg80211_wbtext_table_config(net, data,
				command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_WBTEXT_DELTA_CONFIG,
			strlen(CMD_WBTEXT_DELTA_CONFIG)) == 0) {
		char *data = (command + strlen(CMD_WBTEXT_DELTA_CONFIG) + 1);
		bytes_written = wl_cfg80211_wbtext_delta_config(net, data,
				command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_WBTEXT_BTM_TIMER_THRESHOLD,
			strlen(CMD_WBTEXT_BTM_TIMER_THRESHOLD)) == 0) {
		bytes_written = wl_cfg80211_wbtext_btm_timer_threshold(net, command,
			priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_WBTEXT_BTM_DELTA,
			strlen(CMD_WBTEXT_BTM_DELTA)) == 0) {
		bytes_written = wl_cfg80211_wbtext_btm_delta(net, command,
			priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_WBTEXT_ESTM_ENABLE,
			strlen(CMD_WBTEXT_ESTM_ENABLE)) == 0) {
		bytes_written = wl_cfg80211_wbtext_estm_enable(net, command,
			priv_cmd.total_len);
	}
#endif /* WBTEXT */
#ifdef WLWFDS
	else if (strnicmp(command, CMD_ADD_WFDS_HASH, strlen(CMD_ADD_WFDS_HASH)) == 0) {
		bytes_written = wl_android_set_wfds_hash(net, command, 1);
	}
	else if (strnicmp(command, CMD_DEL_WFDS_HASH, strlen(CMD_DEL_WFDS_HASH)) == 0) {
		bytes_written = wl_android_set_wfds_hash(net, command, 0);
	}
#endif /* WLWFDS */
	else if (strnicmp(command, CMD_MURX_BFE_CAP,
			strlen(CMD_MURX_BFE_CAP)) == 0) {
#ifdef WL_MURX
		uint val = *(command + strlen(CMD_MURX_BFE_CAP) + 1) - '0';
		bytes_written = wl_android_murx_bfe_cap(net, val);
#else
		return BCME_UNSUPPORTED;
#endif /* WL_MURX */
	}
#ifdef SUPPORT_AP_HIGHER_BEACONRATE
	else if (strnicmp(command, CMD_GET_AP_BASICRATE, strlen(CMD_GET_AP_BASICRATE)) == 0) {
		bytes_written = wl_android_get_ap_basicrate(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_SET_AP_BEACONRATE, strlen(CMD_SET_AP_BEACONRATE)) == 0) {
		bytes_written = wl_android_set_ap_beaconrate(net, command);
	}
#endif /* SUPPORT_AP_HIGHER_BEACONRATE */
#ifdef SUPPORT_AP_RADIO_PWRSAVE
	else if (strnicmp(command, CMD_SET_AP_RPS_PARAMS, strlen(CMD_SET_AP_RPS_PARAMS)) == 0) {
		bytes_written = wl_android_set_ap_rps_params(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_SET_AP_RPS, strlen(CMD_SET_AP_RPS)) == 0) {
		bytes_written = wl_android_set_ap_rps(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_GET_AP_RPS, strlen(CMD_GET_AP_RPS)) == 0) {
		bytes_written = wl_android_get_ap_rps(net, command, priv_cmd.total_len);
	}
#endif /* SUPPORT_AP_RADIO_PWRSAVE */
#ifdef SUPPORT_AP_SUSPEND
	else if (strnicmp(command, CMD_SET_AP_SUSPEND, strlen(CMD_SET_AP_SUSPEND)) == 0) {
		bytes_written = wl_android_set_ap_suspend(net, command, priv_cmd.total_len);
	}
#endif /* SUPPORT_AP_SUSPEND */
#ifdef SUPPORT_AP_BWCTRL
	else if (strnicmp(command, CMD_SET_AP_BW, strlen(CMD_SET_AP_BW)) == 0) {
		bytes_written = wl_android_set_ap_bw(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_GET_AP_BW, strlen(CMD_GET_AP_BW)) == 0) {
		bytes_written = wl_android_get_ap_bw(net, command, priv_cmd.total_len);
	}
#endif /* SUPPORT_AP_BWCTRL */
#ifdef SUPPORT_RSSI_SUM_REPORT
	else if (strnicmp(command, CMD_SET_RSSI_LOGGING, strlen(CMD_SET_RSSI_LOGGING)) == 0) {
		bytes_written = wl_android_set_rssi_logging(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_GET_RSSI_LOGGING, strlen(CMD_GET_RSSI_LOGGING)) == 0) {
		bytes_written = wl_android_get_rssi_logging(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_GET_RSSI_PER_ANT, strlen(CMD_GET_RSSI_PER_ANT)) == 0) {
		bytes_written = wl_android_get_rssi_per_ant(net, command, priv_cmd.total_len);
	}
#endif /* SUPPORT_RSSI_SUM_REPORT */
#if defined(SUPPORT_RANDOM_MAC_SCAN)
	else if (strnicmp(command, ENABLE_RANDOM_MAC, strlen(ENABLE_RANDOM_MAC)) == 0) {
		bytes_written = wl_cfg80211_set_random_mac(net, TRUE);
	} else if (strnicmp(command, DISABLE_RANDOM_MAC, strlen(DISABLE_RANDOM_MAC)) == 0) {
		bytes_written = wl_cfg80211_set_random_mac(net, FALSE);
	}
#endif /* SUPPORT_RANDOM_MAC_SCAN */
#ifdef CONNECTION_STATISTICS
	else if (strnicmp(command, CMD_GET_CONNECTION_STATS,
		strlen(CMD_GET_CONNECTION_STATS)) == 0) {
		bytes_written = wl_android_get_connection_stats(net, command,
			priv_cmd.total_len);
	}
#endif
#ifdef DHD_LOG_DUMP
	else if (strnicmp(command, CMD_NEW_DEBUG_PRINT_DUMP,
		strlen(CMD_NEW_DEBUG_PRINT_DUMP)) == 0) {
		dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(net);
		/* check whether it has more command */
		if (strnicmp(command + strlen(CMD_NEW_DEBUG_PRINT_DUMP), " ", 1) == 0) {
			/* compare unwanted/disconnected command */
			if (strnicmp(command + strlen(CMD_NEW_DEBUG_PRINT_DUMP) + 1,
				SUBCMD_UNWANTED, strlen(SUBCMD_UNWANTED)) == 0) {
				dhd_log_dump_trigger(dhdp, CMD_UNWANTED);
			} else if (strnicmp(command + strlen(CMD_NEW_DEBUG_PRINT_DUMP) + 1,
				SUBCMD_DISCONNECTED, strlen(SUBCMD_DISCONNECTED)) == 0) {
				dhd_log_dump_trigger(dhdp, CMD_DISCONNECTED);
			} else {
				dhd_log_dump_trigger(dhdp, CMD_DEFAULT);
			}
		} else {
			dhd_log_dump_trigger(dhdp, CMD_DEFAULT);
		}
	}
#endif /* DHD_LOG_DUMP */
#ifdef TPUT_DEBUG_DUMP
	else if (strnicmp(command, CMD_TPUT_DEBUG_MODE_ENABLE,
			strlen(CMD_TPUT_DEBUG_MODE_ENABLE)) == 0) {
		wl_cfgdbg_tput_debug_mode(net, true);
	}
	else if (strnicmp(command, CMD_TPUT_DEBUG_MODE_DISABLE,
			strlen(CMD_TPUT_DEBUG_MODE_DISABLE)) == 0) {
		wl_cfgdbg_tput_debug_mode(net, false);
	}
#endif /* WL_TPUT_DEBUG_DUMP */
#ifdef DHD_STATUS_LOGGING
	else if (strnicmp(command, CMD_DUMP_STATUS_LOG, strlen(CMD_DUMP_STATUS_LOG)) == 0) {
		dhd_statlog_dump_scr(wl_cfg80211_get_dhdp(net));
	}
	else if (strnicmp(command, CMD_QUERY_STATUS_LOG, strlen(CMD_QUERY_STATUS_LOG)) == 0) {
		dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(net);
		bytes_written = dhd_statlog_query(dhdp, command, priv_cmd.total_len);
	}
#endif /* DHD_STATUS_LOGGING */
#if defined(CONFIG_TIZEN)
	else if (strnicmp(command, CMD_POWERSAVEMODE_SET,
			strlen(CMD_POWERSAVEMODE_SET)) == 0) {
		bytes_written = wl_android_set_powersave_mode(net, command,
			priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_POWERSAVEMODE_GET,
			strlen(CMD_POWERSAVEMODE_GET)) == 0) {
		bytes_written = wl_android_get_powersave_mode(net, command,
			priv_cmd.total_len);
	}
#endif /* CONFIG_TIZEN */
#ifdef SET_PCIE_IRQ_CPU_CORE
	else if (strnicmp(command, CMD_PCIE_IRQ_CORE, strlen(CMD_PCIE_IRQ_CORE)) == 0) {
		int affinity_cmd = *(command + strlen(CMD_PCIE_IRQ_CORE) + 1) - '0';
		wl_android_set_irq_cpucore(net, affinity_cmd);
	}
#endif /* SET_PCIE_IRQ_CPU_CORE */
#if defined(DHD_HANG_SEND_UP_TEST)
	else if (strnicmp(command, CMD_MAKE_HANG, strlen(CMD_MAKE_HANG)) == 0) {
		int skip = strlen(CMD_MAKE_HANG) + 1;
		wl_android_make_hang_with_reason(net, (const char*)command+skip);
	}
#endif /* DHD_HANG_SEND_UP_TEST */
#ifdef SUPPORT_LQCM
	else if (strnicmp(command, CMD_SET_LQCM_ENABLE, strlen(CMD_SET_LQCM_ENABLE)) == 0) {
		int lqcm_enable = *(command + strlen(CMD_SET_LQCM_ENABLE) + 1) - '0';
		bytes_written = wl_android_lqcm_enable(net, lqcm_enable);
	}
	else if (strnicmp(command, CMD_GET_LQCM_REPORT,
			strlen(CMD_GET_LQCM_REPORT)) == 0) {
		bytes_written = wl_android_get_lqcm_report(net, command,
			priv_cmd.total_len);
	}
#endif
	else if (strnicmp(command, CMD_GET_SNR, strlen(CMD_GET_SNR)) == 0) {
		bytes_written = wl_android_get_snr(net, command, priv_cmd.total_len);
	}
#ifdef DHD_PKT_LOGGING
	else if (strnicmp(command, CMD_PKTLOG_FILTER_ENABLE,
		strlen(CMD_PKTLOG_FILTER_ENABLE)) == 0) {
		bytes_written = wl_android_pktlog_filter_enable(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_PKTLOG_FILTER_DISABLE,
		strlen(CMD_PKTLOG_FILTER_DISABLE)) == 0) {
		bytes_written = wl_android_pktlog_filter_disable(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_PKTLOG_FILTER_PATTERN_ENABLE,
		strlen(CMD_PKTLOG_FILTER_PATTERN_ENABLE)) == 0) {
		bytes_written =
			wl_android_pktlog_filter_pattern_enable(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_PKTLOG_FILTER_PATTERN_DISABLE,
		strlen(CMD_PKTLOG_FILTER_PATTERN_DISABLE)) == 0) {
		bytes_written =
			wl_android_pktlog_filter_pattern_disable(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_PKTLOG_FILTER_ADD, strlen(CMD_PKTLOG_FILTER_ADD)) == 0) {
		bytes_written = wl_android_pktlog_filter_add(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_PKTLOG_FILTER_DEL, strlen(CMD_PKTLOG_FILTER_DEL)) == 0) {
		bytes_written = wl_android_pktlog_filter_del(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_PKTLOG_FILTER_INFO, strlen(CMD_PKTLOG_FILTER_INFO)) == 0) {
		bytes_written = wl_android_pktlog_filter_info(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_PKTLOG_START, strlen(CMD_PKTLOG_START)) == 0) {
		bytes_written = wl_android_pktlog_start(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_PKTLOG_STOP, strlen(CMD_PKTLOG_STOP)) == 0) {
		bytes_written = wl_android_pktlog_stop(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_PKTLOG_FILTER_EXIST, strlen(CMD_PKTLOG_FILTER_EXIST)) == 0) {
		bytes_written = wl_android_pktlog_filter_exist(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_PKTLOG_MINMIZE_ENABLE,
		strlen(CMD_PKTLOG_MINMIZE_ENABLE)) == 0) {
		bytes_written = wl_android_pktlog_minmize_enable(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_PKTLOG_MINMIZE_DISABLE,
		strlen(CMD_PKTLOG_MINMIZE_DISABLE)) == 0) {
		bytes_written = wl_android_pktlog_minmize_disable(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_PKTLOG_CHANGE_SIZE,
		strlen(CMD_PKTLOG_CHANGE_SIZE)) == 0) {
		bytes_written = wl_android_pktlog_change_size(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_PKTLOG_DEBUG_DUMP, strlen(CMD_PKTLOG_DEBUG_DUMP)) == 0) {
		bytes_written = wl_android_pktlog_dbg_dump(net, command, priv_cmd.total_len);
	}
#endif /* DHD_PKT_LOGGING */
	else if (strnicmp(command, CMD_DEBUG_VERBOSE, strlen(CMD_DEBUG_VERBOSE)) == 0) {
		int verbose_level = *(command + strlen(CMD_DEBUG_VERBOSE) + 1) - '0';
		bytes_written = wl_cfg80211_set_dbg_verbose(net, verbose_level);
	}
#ifdef DHD_EVENT_LOG_FILTER
	else if (strnicmp(command, CMD_EWP_FILTER,
		strlen(CMD_EWP_FILTER)) == 0) {
		bytes_written = wl_android_ewp_filter(net, command, priv_cmd.total_len);
	}
#endif /* DHD_EVENT_LOG_FILTER */
#ifdef WL_BCNRECV
	else if (strnicmp(command, CMD_BEACON_RECV,
		strlen(CMD_BEACON_RECV)) == 0) {
		char *data = (command + strlen(CMD_BEACON_RECV) + 1);
		bytes_written = wl_android_bcnrecv_config(net,
				data, priv_cmd.total_len);
	}
#endif /* WL_BCNRECV */
#ifdef WL_MBO
	else if (strnicmp(command, CMD_MBO, strlen(CMD_MBO)) == 0) {
		bytes_written = wl_android_process_mbo_cmd(net, command,
			priv_cmd.total_len);
	}
#endif /* WL_MBO */
#ifdef WL_CAC_TS
	else if (strnicmp(command, CMD_CAC_TSPEC,
		strlen(CMD_CAC_TSPEC)) == 0) {
		char *data = (command + strlen(CMD_CAC_TSPEC) + 1);
		bytes_written = wl_android_cac_ts_config(net,
				data, priv_cmd.total_len);
	}
#endif /* WL_CAC_TS */
#ifdef WL_GET_CU
	else if (strnicmp(command, CMD_GET_CHAN_UTIL,
		strlen(CMD_GET_CHAN_UTIL)) == 0) {
		bytes_written = wl_android_get_channel_util(net,
			command, priv_cmd.total_len);
	}
#endif /* WL_GET_CU */
#ifdef RTT_GEOFENCE_INTERVAL
#if defined(RTT_SUPPORT) && defined(WL_NAN)
	else if (strnicmp(command, CMD_GEOFENCE_INTERVAL,
			strlen(CMD_GEOFENCE_INTERVAL)) == 0) {
		(void)wl_android_set_rtt_geofence_interval(net, command);
	}
#endif /* RTT_SUPPORT && WL_NAN */
#endif /* RTT_GEOFENCE_INTERVAL */
#ifdef SUPPORT_SOFTAP_ELNA_BYPASS
	else if (strnicmp(command, CMD_SET_SOFTAP_ELNA_BYPASS,
			strlen(CMD_SET_SOFTAP_ELNA_BYPASS)) == 0) {
		bytes_written =
			wl_android_set_softap_elna_bypass(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_GET_SOFTAP_ELNA_BYPASS,
			strlen(CMD_GET_SOFTAP_ELNA_BYPASS)) == 0) {
		bytes_written =
			wl_android_get_softap_elna_bypass(net, command, priv_cmd.total_len);
	}
#endif /* SUPPORT_SOFTAP_ELNA_BYPASS */
#ifdef CUSTOM_SOFTAP_SET_ANT
	else if (strnicmp(command, CMD_SET_HOTSPOT_ANTENNA_MODE,
			strlen(CMD_SET_HOTSPOT_ANTENNA_MODE)) == 0) {
		bytes_written = wl_android_set_softap_antenna(net, command);
	}
	else if (strnicmp(command, CMD_GET_HOTSPOT_ANTENNA_MODE,
			strlen(CMD_GET_HOTSPOT_ANTENNA_MODE)) == 0) {
		bytes_written = wl_android_get_softap_antenna(net, command, priv_cmd.total_len);
	}
#endif /* CUSTOM_SOFTAP_SET_ANT */
#ifdef WL_NAN
	else if (strnicmp(command, CMD_GET_NAN_STATUS,
			strlen(CMD_GET_NAN_STATUS)) == 0) {
		bytes_written =
			wl_android_get_nan_status(net, command, priv_cmd.total_len);
	}
#endif /* WL_NAN */
#if defined(SUPPORT_NAN_RANGING_TEST_BW)
	else if (strnicmp(command, CMD_NAN_RANGING_SET_BW, strlen(CMD_NAN_RANGING_SET_BW)) == 0) {
		int bw_cmd = *(command + strlen(CMD_NAN_RANGING_SET_BW) + 1) - '0';
		bytes_written = wl_nan_ranging_bw(net, bw_cmd, command);
	}
#endif /* SUPPORT_NAN_RANGING_TEST_BW */
	else if (strnicmp(command, CMD_GET_FACTORY_MAC, strlen(CMD_GET_FACTORY_MAC)) == 0) {
		bytes_written = wl_android_get_factory_mac_addr(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_HAPD_SET_AX_MODE, strlen(CMD_HAPD_SET_AX_MODE)) == 0) {
		int skip = strlen(CMD_HAPD_SET_AX_MODE) + 1;
		bytes_written = wl_android_set_softap_ax_mode(net, command + skip);
	}
#ifdef SUPPORT_LATENCY_CRITICAL_DATA
	else if (strnicmp(command, CMD_SET_LATENCY_CRITICAL_DATA,
		strlen(CMD_SET_LATENCY_CRITICAL_DATA)) == 0) {
		int enable = *(command + strlen(CMD_SET_LATENCY_CRITICAL_DATA) + 1) - '0';
		bytes_written = wl_android_set_latency_crt_data(net, enable);
	}
	else if  (strnicmp(command, CMD_GET_LATENCY_CRITICAL_DATA,
		strlen(CMD_GET_LATENCY_CRITICAL_DATA)) == 0) {
		bytes_written = wl_android_get_latency_crt_data(net, command, priv_cmd.total_len);
	}
#endif	/* SUPPORT_LATENCY_CRITICAL_DATA */
#if defined(WL_LATENCY_CONFIG)
	else if (strnicmp(command, CMD_SET_LOW_LATENCY,
		strlen(CMD_SET_LOW_LATENCY)) == 0) {
		int skip = strlen(CMD_SET_LOW_LATENCY) + 1;
		bytes_written = wl_android_set_low_latency(net, command + skip);
	}
#endif /* WL_LATENCY_CONFIG */
#ifdef WL_TWT
	else if (strnicmp(command, CMD_TWT_SETUP, strlen(CMD_TWT_SETUP)) == 0) {
		bytes_written = wl_android_twt_setup(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_TWT_TEARDOWN, strlen(CMD_TWT_TEARDOWN)) == 0) {
		bytes_written = wl_android_twt_teardown(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_TWT_INFO, strlen(CMD_TWT_INFO)) == 0) {
		bytes_written = wl_android_twt_info(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_TWT_STATUS_QUERY, strlen(CMD_TWT_STATUS_QUERY)) == 0) {
		bytes_written = wl_android_twt_status_query(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_TWT_CAPABILITY, strlen(CMD_TWT_CAPABILITY)) == 0) {
		bytes_written = wl_android_twt_cap(net, command, priv_cmd.total_len);
	}
	else if ((strnicmp(command, CMD_TWT_GET_STATS, strlen(CMD_TWT_GET_STATS)) == 0) ||
		(strnicmp(command, CMD_TWT_CLR_STATS, strlen(CMD_TWT_CLR_STATS)) == 0)) {
		bytes_written = wl_android_twt_stats(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_TWT_SOFTAP_ENABLE, strlen(CMD_TWT_SOFTAP_ENABLE)) == 0) {
		int twt_softap_enable = *(command + strlen(CMD_TWT_SOFTAP_ENABLE) + 1) - '0';
		bytes_written = wl_android_twt_softap_enable(net, twt_softap_enable);
	}
#endif /* WL_TWT */
	else if (strnicmp(command, CMD_GET_6G_SOFTAP_FREQ_LIST,
		strlen(CMD_GET_6G_SOFTAP_FREQ_LIST)) == 0) {
		bytes_written = wl_android_get_6g_softap_freq_list(net, command,
			priv_cmd.total_len);
	}
#ifdef WL_UWB_COEX
	else if (strnicmp(command, CMD_SET_UWB_COEX_ENABLE,
			strlen(CMD_SET_UWB_COEX_ENABLE)) == 0) {
		int skip = strlen(CMD_SET_UWB_COEX_ENABLE) + 1;
		bytes_written = wl_android_uwbcx_set_enable(net, (const char *)(command + skip));
	}
	else if (strnicmp(command, CMD_GET_UWB_COEX_ENABLE,
			strlen(CMD_GET_UWB_COEX_ENABLE)) == 0) {
		bytes_written = wl_android_uwbcx_get_enable(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_SET_UWB_COEX_PREPARE_TIME,
			strlen(CMD_SET_UWB_COEX_PREPARE_TIME)) == 0) {
		int skip = strlen(CMD_SET_UWB_COEX_PREPARE_TIME) + 1;
		bytes_written = wl_android_uwbcx_set_prepare_time(net,
				(const char *)(command + skip));
	}
	else if (strnicmp(command, CMD_GET_UWB_COEX_PREPARE_TIME,
			strlen(CMD_GET_UWB_COEX_PREPARE_TIME)) == 0) {
		bytes_written = wl_android_uwbcx_get_prepare_time(net,
				command, priv_cmd.total_len);
	}
#endif /* WL_UWB_COEX */
#ifdef WL_DUAL_STA
	else if (strnicmp(command, CMD_SET_PRIMARY_INET,
			strlen(CMD_SET_PRIMARY_INET)) == 0) {
		bytes_written = wl_android_set_primary_inet(net,
				command, priv_cmd.total_len);
	}
#endif /* WL_DUAL_STA */
	else if (strnicmp(command, CMD_SETWSECINFO, strlen(CMD_SETWSECINFO)) == 0) {
		bytes_written = wl_android_set_wsec_info(net, command);
	}
#if defined(LIMIT_AP_BW)
	else if (strnicmp(command, CMD_SET_SOFTAP_BW,
		strlen(CMD_SET_SOFTAP_BW)) == 0) {
		bytes_written = wl_android_set_softap_bw(net, command);
	} else if (strnicmp(command, CMD_GET_SOFTAP_BW,
		strlen(CMD_GET_SOFTAP_BW)) == 0) {
		bytes_written = wl_android_get_softap_bw(net, command,
			priv_cmd.total_len);
	}
#endif /* LIMIT_AP_BW */
	else if (wl_android_ext_priv_cmd(net, command, priv_cmd.total_len, &bytes_written) == 0) {
	}
	else {
		DHD_ERROR(("Unknown PRIVATE command %s - ignored\n", command));
		bytes_written = scnprintf(command, sizeof("FAIL"), "FAIL");
	}

	return bytes_written;
}

/*
* ENABLE_INSMOD_NO_FW_LOAD	X O O O
* ENABLE_INSMOD_NO_POWER_OFF	X X O O
* NO_POWER_OFF_AFTER_OPEN	X X X O
* after insmod					H L H H
* wlan0 down					H L L H
* fw trap trigger wlan0 down		H L L L
*/

int wl_android_init(void)
{
	int ret = 0;

#ifdef ENABLE_INSMOD_NO_POWER_OFF
	dhd_download_fw_on_driverload = TRUE;
#elif defined(ENABLE_INSMOD_NO_FW_LOAD)
	dhd_download_fw_on_driverload = FALSE;
#endif /* ENABLE_INSMOD_NO_FW_LOAD */
	if (!iface_name[0]) {
		bzero(iface_name, IFNAMSIZ);
		bcm_strncpy_s(iface_name, IFNAMSIZ, "wlan", IFNAMSIZ);
	}

#ifdef WL_GENL
	wl_genl_init();
#endif
#ifdef WL_NETLINK
	wl_netlink_init();
#endif

	return ret;
}

int wl_android_exit(void)
{
	int ret = 0;
	struct io_cfg *cur, *q;

#ifdef WL_GENL
	wl_genl_deinit();
#endif /* WL_GENL */
#ifdef WL_NETLINK
	wl_netlink_deinit();
#endif

	GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
	list_for_each_entry_safe(cur, q, &miracast_resume_list, list) {
		GCC_DIAGNOSTIC_POP();
		list_del(&cur->list);
		kfree(cur);
	}

	return ret;
}

void wl_android_post_init(void)
{

#ifdef ENABLE_4335BT_WAR
	bcm_bt_unlock(lock_cookie_wifi);
	DHD_ERROR(("wl_android_post_init: btlock released\n"));
#endif /* ENABLE_4335BT_WAR */

	if (!dhd_download_fw_on_driverload) {
		g_wifi_on = FALSE;
		DHD_ERROR(("%s: %d\n", __FUNCTION__, g_wifi_on));
	}
}

#ifdef WL_GENL
/* Generic Netlink Initializaiton */
static int wl_genl_init(void)
{
	int ret;

	WL_DBG(("GEN Netlink Init\n\n"));

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0))
	/* register new family */
	ret = genl_register_family(&wl_genl_family);
	if (ret != 0)
		goto failure;

	/* register functions (commands) of the new family */
	ret = genl_register_ops(&wl_genl_family, &wl_genl_ops);
	if (ret != 0) {
		WL_ERR(("register ops failed: %i\n", ret));
		genl_unregister_family(&wl_genl_family);
		goto failure;
	}

	ret = genl_register_mc_group(&wl_genl_family, &wl_genl_mcast);
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0))
	ret = genl_register_family_with_ops_groups(&wl_genl_family, wl_genl_ops, wl_genl_mcast);
#else
	ret = genl_register_family(&wl_genl_family);
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0) */
	if (ret != 0) {
		WL_ERR(("register mc_group failed: %i\n", ret));
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0))
		genl_unregister_ops(&wl_genl_family, &wl_genl_ops);
#endif
		genl_unregister_family(&wl_genl_family);
		goto failure;
	}

	return 0;

failure:
	WL_ERR(("Registering Netlink failed!!\n"));
	return -1;
}

/* Generic netlink deinit */
static int wl_genl_deinit(void)
{

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0))
	if (genl_unregister_ops(&wl_genl_family, &wl_genl_ops) < 0)
		WL_ERR(("Unregister wl_genl_ops failed\n"));
#endif
	if (genl_unregister_family(&wl_genl_family) < 0)
		WL_ERR(("Unregister wl_genl_ops failed\n"));

	return 0;
}

s32 wl_event_to_bcm_event(u16 event_type)
{
	/* When you add any new event, please mention the
	 * version of BCM supplicant supporting it
	 */
	u16 event = -1;

	switch (event_type) {
		case WLC_E_SERVICE_FOUND:
			event = BCM_E_SVC_FOUND;
			break;
		case WLC_E_P2PO_ADD_DEVICE:
			event = BCM_E_DEV_FOUND;
			break;
		case WLC_E_P2PO_DEL_DEVICE:
			event = BCM_E_DEV_LOST;
			break;
	/* Above events are supported from BCM Supp ver 47 Onwards */

		default:
			WL_ERR(("Event not supported\n"));
	}

	return event;
}

s32
wl_genl_send_msg(
	struct net_device *ndev,
	u32 event_type,
	const u8 *buf,
	u16 len,
	u8 *subhdr,
	u16 subhdr_len)
{
	int ret = 0;
	struct sk_buff *skb;
	void *msg;
	u32 attr_type = 0;
	bcm_event_hdr_t *hdr = NULL;
	int mcast = 1; /* By default sent as mutlicast type */
	int pid = 0;
	u8 *ptr = NULL, *p = NULL;
	u32 tot_len = sizeof(bcm_event_hdr_t) + subhdr_len + len;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	struct bcm_cfg80211 *cfg = wl_get_cfg(ndev);

	WL_DBG(("Enter \n"));

	/* Decide between STRING event and Data event */
	if (event_type == 0)
		attr_type = BCM_GENL_ATTR_STRING;
	else
		attr_type = BCM_GENL_ATTR_MSG;

	skb = genlmsg_new(NLMSG_GOODSIZE, kflags);
	if (skb == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	msg = genlmsg_put(skb, 0, 0, &wl_genl_family, 0, BCM_GENL_CMD_MSG);
	if (msg == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	if (attr_type == BCM_GENL_ATTR_STRING) {
		/* Add a BCM_GENL_MSG attribute. Since it is specified as a string.
		 * make sure it is null terminated
		 */
		if (subhdr || subhdr_len) {
			WL_ERR(("No sub hdr support for the ATTR STRING type \n"));
			ret =  -EINVAL;
			goto out;
		}

		ret = nla_put_string(skb, BCM_GENL_ATTR_STRING, buf);
		if (ret != 0) {
			WL_ERR(("nla_put_string failed\n"));
			goto out;
		}
	} else {
		/* ATTR_MSG */

		/* Create a single buffer for all */
		p = ptr = (u8 *)MALLOCZ(cfg->osh, tot_len);
		if (!ptr) {
			ret = -ENOMEM;
			WL_ERR(("ENOMEM!!\n"));
			goto out;
		}

		/* Include the bcm event header */
		hdr = (bcm_event_hdr_t *)ptr;
		hdr->event_type = wl_event_to_bcm_event(event_type);
		hdr->len = len + subhdr_len;
		ptr += sizeof(bcm_event_hdr_t);

		/* Copy subhdr (if any) */
		if (subhdr && subhdr_len) {
			memcpy(ptr, subhdr, subhdr_len);
			ptr += subhdr_len;
		}

		/* Copy the data */
		if (buf && len) {
			memcpy(ptr, buf, len);
		}

		ret = nla_put(skb, BCM_GENL_ATTR_MSG, tot_len, p);
		if (ret != 0) {
			WL_ERR(("nla_put_string failed\n"));
			goto out;
		}
	}

	if (mcast) {
		int err = 0;
		/* finalize the message */
		genlmsg_end(skb, msg);
		/* NETLINK_CB(skb).dst_group = 1; */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
		if ((err = genlmsg_multicast(skb, 0, wl_genl_mcast.id, GFP_ATOMIC)) < 0)
#else
		if ((err = genlmsg_multicast(&wl_genl_family, skb, 0, 0, GFP_ATOMIC)) < 0)
#endif
			WL_ERR(("genlmsg_multicast for attr(%d) failed. Error:%d \n",
				attr_type, err));
		else
			WL_DBG(("Multicast msg sent successfully. attr_type:%d len:%d \n",
				attr_type, tot_len));
	} else {
		NETLINK_CB(skb).dst_group = 0; /* Not in multicast group */

		/* finalize the message */
		genlmsg_end(skb, msg);

		/* send the message back */
		if (genlmsg_unicast(&init_net, skb, pid) < 0)
			WL_ERR(("genlmsg_unicast failed\n"));
	}

out:
	if (p) {
		MFREE(cfg->osh, p, tot_len);
	}
	if (ret)
		nlmsg_free(skb);

	return ret;
}

static s32
wl_genl_handle_msg(
	struct sk_buff *skb,
	struct genl_info *info)
{
	struct nlattr *na;
	u8 *data = NULL;

	WL_DBG(("Enter \n"));

	if (info == NULL) {
		return -EINVAL;
	}

	na = info->attrs[BCM_GENL_ATTR_MSG];
	if (!na) {
		WL_ERR(("nlattribute NULL\n"));
		return -EINVAL;
	}

	data = (char *)nla_data(na);
	if (!data) {
		WL_ERR(("Invalid data\n"));
		return -EINVAL;
	} else {
		/* Handle the data */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 0)) || defined(WL_COMPAT_WIRELESS)
		WL_DBG(("%s: Data received from pid (%d) \n", __func__,
			info->snd_pid));
#else
		WL_DBG(("%s: Data received from pid (%d) \n", __func__,
			info->snd_portid));
#endif /* (LINUX_VERSION < VERSION(3, 7, 0) || WL_COMPAT_WIRELESS */
	}

	return 0;
}
#endif /* WL_GENL */

int wl_fatal_error(void * wl, int rc)
{
	return FALSE;
}

void
wl_android_set_wifi_on_flag(bool enable)
{
	DHD_ERROR(("%s: %d\n", __FUNCTION__, enable));
	g_wifi_on = enable;
}

#ifdef WBTEXT
static int
wlc_wbtext_get_roam_prof(struct net_device *ndev, wl_roamprof_band_t *rp,
	uint8 band, uint8 *roam_prof_ver, uint8 *roam_prof_size)
{
	int err = BCME_OK;
	struct bcm_cfg80211 *cfg = wl_get_cfg(ndev);
	u8 *ioctl_buf = NULL;

	ioctl_buf = (u8 *)MALLOCZ(cfg->osh, WLC_IOCTL_MEDLEN);
	if (unlikely(!ioctl_buf)) {
		WL_ERR(("%s: failed to allocate memory\n", __func__));
		err =  -ENOMEM;
		goto exit;
	}
	rp->v1.band = band;
	rp->v1.len = 0;
	/* Getting roam profile from fw */
	if ((err = wldev_iovar_getbuf(ndev, "roam_prof", rp, sizeof(*rp),
		ioctl_buf, WLC_IOCTL_MEDLEN, NULL))) {
		WL_ERR(("Getting roam_profile failed with err=%d \n", err));
		goto exit;
	}
	memcpy_s(rp, sizeof(*rp), ioctl_buf, sizeof(*rp));
	/* roam_prof version get */
	if (rp->v1.ver > WL_ROAM_PROF_VER_3) {
		WL_ERR(("bad version (=%d) in return data\n", rp->v1.ver));
		err = BCME_VERSION;
		goto exit;
	}
	switch (rp->v1.ver) {
		case WL_ROAM_PROF_VER_0:
		{
			*roam_prof_size = sizeof(wl_roam_prof_v1_t);
			*roam_prof_ver = WL_ROAM_PROF_VER_0;
		}
		break;
		case WL_ROAM_PROF_VER_1:
		{
			*roam_prof_size = sizeof(wl_roam_prof_v2_t);
			*roam_prof_ver = WL_ROAM_PROF_VER_1;
		}
		break;
		case WL_ROAM_PROF_VER_2:
		{
			*roam_prof_size = sizeof(wl_roam_prof_v3_t);
			*roam_prof_ver = WL_ROAM_PROF_VER_2;
		}
		break;
		case WL_ROAM_PROF_VER_3:
		{
			*roam_prof_size = sizeof(wl_roam_prof_v4_t);
			*roam_prof_ver = WL_ROAM_PROF_VER_3;
		}
		break;
		default:
			WL_ERR(("bad version = %d \n", rp->v1.ver));
			err = BCME_VERSION;
			goto exit;
	}
	WL_DBG(("roam prof ver %u size %u\n", *roam_prof_ver, *roam_prof_size));
	if ((rp->v1.len % *roam_prof_size) != 0) {
		WL_ERR(("bad length (=%d) in return data\n", rp->v1.len));
		err = BCME_BADLEN;
	}
exit:
	if (ioctl_buf) {
		MFREE(cfg->osh, ioctl_buf, WLC_IOCTL_MEDLEN);
	}
	return err;
}

s32
wl_cfg80211_wbtext_set_default(struct net_device *ndev)
{
	char *commandp = NULL;
	s32 ret = BCME_OK;
	char *data;
	u8 *ioctl_buf = NULL;
	wl_roamprof_band_t rp;
	uint8 bandidx = 0;
	int wnmmask = 0;
	struct bcm_cfg80211 *cfg = wl_get_cfg(ndev);

	WL_DBG(("set wbtext to default\n"));

	commandp = (char *)MALLOCZ(cfg->osh, WLC_IOCTL_SMLEN);
	if (unlikely(!commandp)) {
		WL_ERR(("%s: failed to allocate memory\n", __func__));
		ret =  -ENOMEM;
		goto exit;
	}
	ioctl_buf = (char *)MALLOCZ(cfg->osh, WLC_IOCTL_SMLEN);
	if (unlikely(!ioctl_buf)) {
		WL_ERR(("%s: failed to allocate memory\n", __func__));
		ret =  -ENOMEM;
		goto exit;
	}

	rp.v1.band = WLC_BAND_2G;
	rp.v1.len = 0;
	/* Getting roam profile from fw */
	if ((ret = wldev_iovar_getbuf(ndev, "roam_prof", &rp, sizeof(rp),
		ioctl_buf, WLC_IOCTL_SMLEN, NULL))) {
		WL_ERR(("Getting roam_profile failed with err=%d \n", ret));
		goto exit;
	}
	memcpy_s(&rp, sizeof(rp), ioctl_buf, sizeof(rp));
	for (bandidx = 0; bandidx < MAXBANDS; bandidx++) {
		switch (rp.v1.ver) {
			case WL_ROAM_PROF_VER_1:
			{
				memset_s(commandp, WLC_IOCTL_SMLEN, 0, WLC_IOCTL_SMLEN);
				if (bandidx == BAND_5G_INDEX) {
					snprintf(commandp, WLC_IOCTL_SMLEN, "%s %s",
						CMD_WBTEXT_PROFILE_CONFIG,
						DEFAULT_WBTEXT_PROFILE_A_V2);
					data = (commandp + strlen(CMD_WBTEXT_PROFILE_CONFIG) + 1);
				} else {
					snprintf(commandp, WLC_IOCTL_SMLEN, "%s %s",
						CMD_WBTEXT_PROFILE_CONFIG,
						DEFAULT_WBTEXT_PROFILE_B_V2);
					data = (commandp + strlen(CMD_WBTEXT_PROFILE_CONFIG) + 1);
				}
			}
			break;
			case WL_ROAM_PROF_VER_2:
			case WL_ROAM_PROF_VER_3:
			{
				memset_s(commandp, WLC_IOCTL_SMLEN, 0, WLC_IOCTL_SMLEN);
				if (bandidx == BAND_5G_INDEX) {
					snprintf(commandp, WLC_IOCTL_SMLEN, "%s %s",
						CMD_WBTEXT_PROFILE_CONFIG,
						DEFAULT_WBTEXT_PROFILE_A_V3);
					data = (commandp + strlen(CMD_WBTEXT_PROFILE_CONFIG) + 1);
				} else {
					snprintf(commandp, WLC_IOCTL_SMLEN, "%s %s",
						CMD_WBTEXT_PROFILE_CONFIG,
						DEFAULT_WBTEXT_PROFILE_B_V3);
					data = (commandp + strlen(CMD_WBTEXT_PROFILE_CONFIG) + 1);
				}
			}
			break;
			default:
				WL_ERR(("No Support for roam prof ver = %d \n", rp.v1.ver));
				ret = -EINVAL;
				goto exit;
		}
		/* set roam profile */
		ret = wl_cfg80211_wbtext_config(ndev, data, commandp, WLC_IOCTL_SMLEN);
		if (ret != BCME_OK) {
			WL_ERR(("%s: Failed to set roam_prof %s error = %d\n",
				__FUNCTION__, data, ret));
			goto exit;
		}
	}

	/* wbtext code for backward compatibility. Newer firmwares set default value
	* from fw init
	*/
	/* set RSSI weight */
	memset_s(commandp, WLC_IOCTL_SMLEN, 0, WLC_IOCTL_SMLEN);
	snprintf(commandp, WLC_IOCTL_SMLEN, "%s %s",
		CMD_WBTEXT_WEIGHT_CONFIG, DEFAULT_WBTEXT_WEIGHT_RSSI_A);
	data = (commandp + strlen(CMD_WBTEXT_WEIGHT_CONFIG) + 1);
	ret = wl_cfg80211_wbtext_weight_config(ndev, data, commandp, WLC_IOCTL_SMLEN);
	if (ret != BCME_OK) {
		WL_ERR(("%s: Failed to set weight config %s error = %d\n",
			__FUNCTION__, data, ret));
		goto exit;
	}

	memset_s(commandp, WLC_IOCTL_SMLEN, 0, WLC_IOCTL_SMLEN);
	snprintf(commandp, WLC_IOCTL_SMLEN, "%s %s",
		CMD_WBTEXT_WEIGHT_CONFIG, DEFAULT_WBTEXT_WEIGHT_RSSI_B);
	data = (commandp + strlen(CMD_WBTEXT_WEIGHT_CONFIG) + 1);
	ret = wl_cfg80211_wbtext_weight_config(ndev, data, commandp, WLC_IOCTL_SMLEN);
	if (ret != BCME_OK) {
		WL_ERR(("%s: Failed to set weight config %s error = %d\n",
			__FUNCTION__, data, ret));
		goto exit;
	}

	/* set CU weight */
	memset_s(commandp, WLC_IOCTL_SMLEN, 0, WLC_IOCTL_SMLEN);
	snprintf(commandp, WLC_IOCTL_SMLEN, "%s %s",
		CMD_WBTEXT_WEIGHT_CONFIG, DEFAULT_WBTEXT_WEIGHT_CU_A);
	data = (commandp + strlen(CMD_WBTEXT_WEIGHT_CONFIG) + 1);
	ret = wl_cfg80211_wbtext_weight_config(ndev, data, commandp, WLC_IOCTL_SMLEN);
	if (ret != BCME_OK) {
		WL_ERR(("%s: Failed to set weight config %s error = %d\n",
			__FUNCTION__, data, ret));
		goto exit;
	}

	memset_s(commandp, WLC_IOCTL_SMLEN, 0, WLC_IOCTL_SMLEN);
	snprintf(commandp, WLC_IOCTL_SMLEN, "%s %s",
		CMD_WBTEXT_WEIGHT_CONFIG, DEFAULT_WBTEXT_WEIGHT_CU_B);
	data = (commandp + strlen(CMD_WBTEXT_WEIGHT_CONFIG) + 1);
	ret = wl_cfg80211_wbtext_weight_config(ndev, data, commandp, WLC_IOCTL_SMLEN);
	if (ret != BCME_OK) {
		WL_ERR(("%s: Failed to set weight config %s error = %d\n",
			__FUNCTION__, data, ret));
		goto exit;
	}

	ret = wldev_iovar_getint(ndev, "wnm", &wnmmask);
	if (ret != BCME_OK) {
		WL_ERR(("%s: Failed to get wnmmask error = %d\n", __func__, ret));
		goto exit;
	}
	/* set ESTM DL weight. */
	if (wnmmask & WL_WNM_ESTM) {
		WL_ERR(("Setting ESTM wt\n"));
		memset_s(commandp, WLC_IOCTL_SMLEN, 0, WLC_IOCTL_SMLEN);
		snprintf(commandp, WLC_IOCTL_SMLEN, "%s %s",
			CMD_WBTEXT_WEIGHT_CONFIG, DEFAULT_WBTEXT_WEIGHT_ESTM_DL_A);
		data = (commandp + strlen(CMD_WBTEXT_WEIGHT_CONFIG) + 1);
		ret = wl_cfg80211_wbtext_weight_config(ndev, data, commandp, WLC_IOCTL_SMLEN);
		if (ret != BCME_OK) {
			WL_ERR(("%s: Failed to set weight config %s error = %d\n",
				__FUNCTION__, data, ret));
			goto exit;
		}

		memset_s(commandp, WLC_IOCTL_SMLEN, 0, WLC_IOCTL_SMLEN);
		snprintf(commandp, WLC_IOCTL_SMLEN, "%s %s",
			CMD_WBTEXT_WEIGHT_CONFIG, DEFAULT_WBTEXT_WEIGHT_ESTM_DL_B);
		data = (commandp + strlen(CMD_WBTEXT_WEIGHT_CONFIG) + 1);
		ret = wl_cfg80211_wbtext_weight_config(ndev, data, commandp, WLC_IOCTL_SMLEN);
		if (ret != BCME_OK) {
			WL_ERR(("%s: Failed to set weight config %s error = %d\n",
				__FUNCTION__, data, ret));
			goto exit;
		}
	}

	/* set RSSI table */
	memset_s(commandp, WLC_IOCTL_SMLEN, 0, WLC_IOCTL_SMLEN);
	snprintf(commandp, WLC_IOCTL_SMLEN, "%s %s",
		CMD_WBTEXT_TABLE_CONFIG, DEFAULT_WBTEXT_TABLE_RSSI_A);
	data = (commandp + strlen(CMD_WBTEXT_TABLE_CONFIG) + 1);
	ret = wl_cfg80211_wbtext_table_config(ndev, data, commandp, WLC_IOCTL_SMLEN);
	if (ret != BCME_OK) {
		WL_ERR(("%s: Failed to set RSSI table %s error = %d\n",
			__FUNCTION__, data, ret));
		goto exit;
	}

	memset_s(commandp, WLC_IOCTL_SMLEN, 0, WLC_IOCTL_SMLEN);
	snprintf(commandp, WLC_IOCTL_SMLEN, "%s %s",
		CMD_WBTEXT_TABLE_CONFIG, DEFAULT_WBTEXT_TABLE_RSSI_B);
	data = (commandp + strlen(CMD_WBTEXT_TABLE_CONFIG) + 1);
	ret = wl_cfg80211_wbtext_table_config(ndev, data, commandp, WLC_IOCTL_SMLEN);
	if (ret != BCME_OK) {
		WL_ERR(("%s: Failed to set RSSI table %s error = %d\n",
			__FUNCTION__, data, ret));
		goto exit;
	}

	/* set CU table */
	memset_s(commandp, WLC_IOCTL_SMLEN, 0, WLC_IOCTL_SMLEN);
	snprintf(commandp, WLC_IOCTL_SMLEN, "%s %s",
		CMD_WBTEXT_TABLE_CONFIG, DEFAULT_WBTEXT_TABLE_CU_A);
	data = (commandp + strlen(CMD_WBTEXT_TABLE_CONFIG) + 1);
	ret = wl_cfg80211_wbtext_table_config(ndev, data, commandp, WLC_IOCTL_SMLEN);
	if (ret != BCME_OK) {
		WL_ERR(("%s: Failed to set CU table %s error = %d\n",
			__FUNCTION__, data, ret));
		goto exit;
	}

	memset_s(commandp, WLC_IOCTL_SMLEN, 0, WLC_IOCTL_SMLEN);
	snprintf(commandp, WLC_IOCTL_SMLEN, "%s %s",
		CMD_WBTEXT_TABLE_CONFIG, DEFAULT_WBTEXT_TABLE_CU_B);
	data = (commandp + strlen(CMD_WBTEXT_TABLE_CONFIG) + 1);
	ret = wl_cfg80211_wbtext_table_config(ndev, data, commandp, WLC_IOCTL_SMLEN);
	if (ret != BCME_OK) {
		WL_ERR(("%s: Failed to set CU table %s error = %d\n",
			__FUNCTION__, data, ret));
		goto exit;
	}

exit:
	if (commandp) {
		MFREE(cfg->osh, commandp, WLC_IOCTL_SMLEN);
	}
	if (ioctl_buf) {
		MFREE(cfg->osh, ioctl_buf, WLC_IOCTL_SMLEN);
	}
	return ret;
}

s32
wl_cfg80211_wbtext_config(struct net_device *ndev, char *data, char *command, int total_len)
{
	uint i = 0;
	long int rssi_lower, roam_trigger;
	struct bcm_cfg80211 *cfg = wl_get_cfg(ndev);
	wl_roamprof_band_t *rp = NULL;
	int err = -EINVAL, bytes_written = 0;
	size_t len = strlen(data);
	int rp_len = 0;
	u8 *ioctl_buf = NULL;
	uint8 roam_prof_size = 0, roam_prof_ver = 0, fs_per = 0, prof_cnt = 0;

	data[len] = '\0';
	ioctl_buf = (u8 *)MALLOCZ(cfg->osh, WLC_IOCTL_MEDLEN);
	if (unlikely(!ioctl_buf)) {
		WL_ERR(("%s: failed to allocate memory\n", __func__));
		err =  -ENOMEM;
		goto exit;
	}
	rp = (wl_roamprof_band_t *)MALLOCZ(cfg->osh, sizeof(*rp));
	if (unlikely(!rp)) {
		WL_ERR(("%s: failed to allocate memory\n", __func__));
		err =  -ENOMEM;
		goto exit;
	}
	if (*data && (!strncmp(data, "b", 1))) {
		rp->v1.band = WLC_BAND_2G;
	} else if (*data && (!strncmp(data, "a", 1))) {
		rp->v1.band = WLC_BAND_5G;
	} else {
		err = snprintf(command, total_len, "Missing band\n");
		goto exit;
	}
	data++;
	rp->v1.len = 0;
	/* Getting roam profile from fw */
	if ((err = wldev_iovar_getbuf(ndev, "roam_prof", rp, sizeof(*rp),
		ioctl_buf, WLC_IOCTL_MEDLEN, NULL))) {
		WL_ERR(("Getting roam_profile failed with err=%d \n", err));
		goto exit;
	}
	memcpy_s(rp, sizeof(*rp), ioctl_buf, sizeof(*rp));
	/* roam_prof version get */
	if (rp->v1.ver > WL_ROAM_PROF_VER_3) {
		WL_ERR(("bad version (=%d) in return data\n", rp->v1.ver));
		err = -EINVAL;
		goto exit;
	}
	switch (rp->v1.ver) {
		case WL_ROAM_PROF_VER_0:
		{
			roam_prof_size = sizeof(wl_roam_prof_v1_t);
			roam_prof_ver = WL_ROAM_PROF_VER_0;
		}
		break;
		case WL_ROAM_PROF_VER_1:
		{
			roam_prof_size = sizeof(wl_roam_prof_v2_t);
			roam_prof_ver = WL_ROAM_PROF_VER_1;
		}
		break;
		case WL_ROAM_PROF_VER_2:
		{
			roam_prof_size = sizeof(wl_roam_prof_v3_t);
			roam_prof_ver = WL_ROAM_PROF_VER_2;
		}
		break;
		case WL_ROAM_PROF_VER_3:
		{
			roam_prof_size = sizeof(wl_roam_prof_v4_t);
			roam_prof_ver = WL_ROAM_PROF_VER_3;
		}
		break;
		default:
			WL_ERR(("bad version = %d \n", rp->v1.ver));
			goto exit;
	}
	WL_DBG(("roam prof ver %u size %u\n", roam_prof_ver, roam_prof_size));
	if ((rp->v1.len % roam_prof_size) != 0) {
		WL_ERR(("bad length (=%d) in return data\n", rp->v1.len));
		err = -EINVAL;
		goto exit;
	}
	for (i = 0; i < WL_MAX_ROAM_PROF_BRACKETS; i++) {
		/* printing contents of roam profile data from fw and exits
		 * if code hits any of one of the below condtion. If remaining
		 * length of buffer is less than roam profile size or
		 * if there is no valid entry.
		 */
		if (((i * roam_prof_size) > rp->v1.len)) {
			break;
		}
		if (roam_prof_ver == WL_ROAM_PROF_VER_0) {
			fs_per = rp->v1.roam_prof[i].fullscan_period;
		} else if (roam_prof_ver == WL_ROAM_PROF_VER_1) {
			fs_per = rp->v2.roam_prof[i].fullscan_period;
		} else if (roam_prof_ver == WL_ROAM_PROF_VER_2) {
			fs_per = rp->v3.roam_prof[i].fullscan_period;
		} else if (roam_prof_ver == WL_ROAM_PROF_VER_3) {
			fs_per = rp->v4.roam_prof[i].fullscan_period;
		}
		if (fs_per == 0) {
			break;
		}
		prof_cnt++;
	}

	if (!*data) {
		for (i = 0; (i < prof_cnt) && (i < WL_MAX_ROAM_PROF_BRACKETS); i++) {
			if (roam_prof_ver == WL_ROAM_PROF_VER_1) {
				bytes_written += scnprintf(command+bytes_written,
					total_len - bytes_written,
					"RSSI[%d,%d] CU(trigger:%d%%: duration:%ds)",
					rp->v2.roam_prof[i].roam_trigger,
					rp->v2.roam_prof[i].rssi_lower,
					rp->v2.roam_prof[i].channel_usage,
					rp->v2.roam_prof[i].cu_avg_calc_dur);
			} else if (roam_prof_ver == WL_ROAM_PROF_VER_2) {
				bytes_written += scnprintf(command+bytes_written,
					total_len - bytes_written,
					"RSSI[%d,%d] CU(trigger:%d%%: duration:%ds)",
					rp->v3.roam_prof[i].roam_trigger,
					rp->v3.roam_prof[i].rssi_lower,
					rp->v3.roam_prof[i].channel_usage,
					rp->v3.roam_prof[i].cu_avg_calc_dur);
			} else if (roam_prof_ver == WL_ROAM_PROF_VER_3) {
				bytes_written += snprintf(command+bytes_written,
					total_len - bytes_written,
					"RSSI[%d,%d] CU(trigger:%d%%: duration:%ds)",
					rp->v4.roam_prof[i].roam_trigger,
					rp->v4.roam_prof[i].rssi_lower,
					rp->v4.roam_prof[i].channel_usage,
					rp->v4.roam_prof[i].cu_avg_calc_dur);
			}
		}
		bytes_written += scnprintf(command+bytes_written, total_len - bytes_written, "\n");
		err = bytes_written;
		goto exit;
	} else {
		/* Do not set roam_prof from upper layer if fw doesn't have 2 rows */
		if (prof_cnt != 2) {
			WL_ERR(("FW must have 2 rows to fill roam_prof\n"));
			err = -EINVAL;
			goto exit;
		}
		/* setting roam profile to fw */
		data++;
		for (i = 0; i < WL_MAX_ROAM_PROF_BRACKETS; i++) {
			roam_trigger = simple_strtol(data, &data, 10);
			if (roam_trigger >= 0) {
				WL_ERR(("roam trigger[%d] value must be negative\n", i));
				err = -EINVAL;
				goto exit;
			}
			data++;
			rssi_lower = simple_strtol(data, &data, 10);
			if (rssi_lower >= 0) {
				WL_ERR(("rssi lower[%d] value must be negative\n", i));
				err = -EINVAL;
				goto exit;
			}
			if (roam_prof_ver == WL_ROAM_PROF_VER_1) {
				rp->v2.roam_prof[i].roam_trigger = roam_trigger;
				rp->v2.roam_prof[i].rssi_lower = rssi_lower;
				data++;
				rp->v2.roam_prof[i].channel_usage = simple_strtol(data, &data, 10);
				data++;
				rp->v2.roam_prof[i].cu_avg_calc_dur =
					simple_strtol(data, &data, 10);
			}
			if (roam_prof_ver == WL_ROAM_PROF_VER_2) {
				rp->v3.roam_prof[i].roam_trigger = roam_trigger;
				rp->v3.roam_prof[i].rssi_lower = rssi_lower;
				data++;
				rp->v3.roam_prof[i].channel_usage = simple_strtol(data, &data, 10);
				data++;
				rp->v3.roam_prof[i].cu_avg_calc_dur =
					simple_strtol(data, &data, 10);
			}
			if (roam_prof_ver == WL_ROAM_PROF_VER_3) {
				rp->v4.roam_prof[i].roam_trigger = roam_trigger;
				rp->v4.roam_prof[i].rssi_lower = rssi_lower;
				data++;
				rp->v4.roam_prof[i].channel_usage = simple_strtol(data, &data, 10);
				data++;
				rp->v4.roam_prof[i].cu_avg_calc_dur =
					simple_strtol(data, &data, 10);
			}

			rp_len += roam_prof_size;

			if (*data == '\0') {
				break;
			}
			data++;
		}
		if (i != 1) {
			WL_ERR(("Only two roam_prof rows supported.\n"));
			err = -EINVAL;
			goto exit;
		}
		rp->v1.len = rp_len;
		if ((err = wldev_iovar_setbuf(ndev, "roam_prof", rp,
				sizeof(*rp), cfg->ioctl_buf, WLC_IOCTL_MEDLEN,
				&cfg->ioctl_buf_sync)) < 0) {
			WL_ERR(("seting roam_profile failed with err %d\n", err));
		}
	}
exit:
	if (rp) {
		MFREE(cfg->osh, rp, sizeof(*rp));
	}
	if (ioctl_buf) {
		MFREE(cfg->osh, ioctl_buf, WLC_IOCTL_MEDLEN);
	}
	return err;
}

int wl_cfg80211_wbtext_weight_config(struct net_device *ndev, char *data,
		char *command, int total_len)
{
	int bytes_written = 0, err = -EINVAL, argc = 0;
	char rssi[BUFSZN], band[BUFSZN], weight[BUFSZN];
	char *endptr = NULL;
	wnm_bss_select_weight_cfg_t *bwcfg;
	u8 ioctl_buf[WLC_IOCTL_SMLEN];
	struct bcm_cfg80211 *cfg = wl_get_cfg(ndev);

	bwcfg = (wnm_bss_select_weight_cfg_t *)MALLOCZ(cfg->osh, sizeof(*bwcfg));
	if (unlikely(!bwcfg)) {
		WL_ERR(("%s: failed to allocate memory\n", __func__));
		err = -ENOMEM;
		goto exit;
	}
	bwcfg->version =  WNM_BSSLOAD_MONITOR_VERSION_1;
	bwcfg->type = 0;
	bwcfg->weight = 0;

	argc = sscanf(data,
		"%"SIZE_CONST_STRING(BUFSZ)"s %"SIZE_CONST_STRING(BUFSZ)"s "
		"%"SIZE_CONST_STRING(BUFSZ)"s", rssi, band, weight);

	if (!strcasecmp(rssi, "rssi"))
		bwcfg->type = WNM_BSS_SELECT_TYPE_RSSI;
	else if (!strcasecmp(rssi, "cu"))
		bwcfg->type = WNM_BSS_SELECT_TYPE_CU;
	else if (!strcasecmp(rssi, "estm_dl"))
		bwcfg->type = WNM_BSS_SELECT_TYPE_ESTM_DL;
	else {
		/* Usage DRIVER WBTEXT_WEIGHT_CONFIG <rssi/cu/estm_dl> <band> <weight> */
		WL_ERR(("%s: Command usage error\n", __func__));
		goto exit;
	}

	if (BCME_BADBAND == wl_android_bandstr_to_fwband(band, &bwcfg->band)) {
		WL_ERR(("%s: Command usage error\n", __func__));
		goto exit;
	}

	if (argc == 2) {
		/* If there is no data after band, getting wnm_bss_select_weight from fw */
		if (bwcfg->band == WLC_BAND_ALL) {
			WL_ERR(("band option \"all\" is for set only, not get\n"));
			goto exit;
		}
		if ((err = wldev_iovar_getbuf(ndev, "wnm_bss_select_weight", bwcfg,
				sizeof(*bwcfg),
				ioctl_buf, sizeof(ioctl_buf), NULL))) {
			WL_ERR(("Getting wnm_bss_select_weight failed with err=%d \n", err));
			goto exit;
		}
		memcpy(bwcfg, ioctl_buf, sizeof(*bwcfg));
		bytes_written = snprintf(command, total_len, "%s %s weight = %d\n",
			(bwcfg->type == WNM_BSS_SELECT_TYPE_RSSI) ? "RSSI" :
			(bwcfg->type == WNM_BSS_SELECT_TYPE_CU) ? "CU": "ESTM_DL",
			wl_android_get_band_str(bwcfg->band), bwcfg->weight);
		err = bytes_written;
		goto exit;
	} else {
		/* if weight is non integer returns command usage error */
		bwcfg->weight = simple_strtol(weight, &endptr, 0);
		if (*endptr != '\0') {
			WL_ERR(("%s: Command usage error", __func__));
			goto exit;
		}
		/* setting weight for iovar wnm_bss_select_weight to fw */
		if ((err = wldev_iovar_setbuf(ndev, "wnm_bss_select_weight", bwcfg,
				sizeof(*bwcfg),
				ioctl_buf, sizeof(ioctl_buf), NULL))) {
			WL_ERR(("setting wnm_bss_select_weight failed with err=%d\n", err));
		}
	}
exit:
	if (bwcfg) {
		MFREE(cfg->osh, bwcfg, sizeof(*bwcfg));
	}
	return err;
}

/* WBTEXT_TUPLE_MIN_LEN_CHECK :strlen(low)+" "+strlen(high)+" "+strlen(factor) */
#define WBTEXT_TUPLE_MIN_LEN_CHECK 5

int wl_cfg80211_wbtext_table_config(struct net_device *ndev, char *data,
	char *command, int total_len)
{
	struct bcm_cfg80211 *cfg = wl_get_cfg(ndev);
	int bytes_written = 0, err = -EINVAL;
	char rssi[BUFSZN], band[BUFSZN];
	int btcfg_len = 0, i = 0, parsed_len = 0;
	wnm_bss_select_factor_cfg_t *btcfg;
	size_t slen = strlen(data);
	char *start_addr = NULL;
	u8 ioctl_buf[WLC_IOCTL_SMLEN];

	data[slen] = '\0';
	btcfg = (wnm_bss_select_factor_cfg_t *)MALLOCZ(cfg->osh,
		(sizeof(*btcfg) + sizeof(*btcfg) * WL_FACTOR_TABLE_MAX_LIMIT));
	if (unlikely(!btcfg)) {
		WL_ERR(("%s: failed to allocate memory\n", __func__));
		err = -ENOMEM;
		goto exit;
	}

	btcfg->version = WNM_BSS_SELECT_FACTOR_VERSION_1;
	btcfg->band = WLC_BAND_AUTO;
	btcfg->type = 0;
	btcfg->count = 0;

	sscanf(data, "%"SIZE_CONST_STRING(BUFSZ)"s %"SIZE_CONST_STRING(BUFSZ)"s", rssi, band);

	if (!strcasecmp(rssi, "rssi")) {
		btcfg->type = WNM_BSS_SELECT_TYPE_RSSI;
	}
	else if (!strcasecmp(rssi, "cu")) {
		btcfg->type = WNM_BSS_SELECT_TYPE_CU;
	}
	else {
		WL_ERR(("%s: Command usage error\n", __func__));
		goto exit;
	}

	if (BCME_BADBAND == wl_android_bandstr_to_fwband(band, &btcfg->band)) {
		WL_ERR(("%s: Command usage, Wrong band\n", __func__));
		goto exit;
	}

	if ((slen - 1) == (strlen(rssi) + strlen(band))) {
		/* Getting factor table using iovar 'wnm_bss_select_table' from fw */
		if ((err = wldev_iovar_getbuf(ndev, "wnm_bss_select_table", btcfg,
				sizeof(*btcfg),
				ioctl_buf, sizeof(ioctl_buf), NULL))) {
			WL_ERR(("Getting wnm_bss_select_table failed with err=%d \n", err));
			goto exit;
		}
		memcpy(btcfg, ioctl_buf, sizeof(*btcfg));
		memcpy(btcfg, ioctl_buf, (btcfg->count+1) * sizeof(*btcfg));

		bytes_written += snprintf(command + bytes_written, total_len - bytes_written,
					"No of entries in table: %d\n", btcfg->count);
		bytes_written += snprintf(command + bytes_written, total_len - bytes_written,
				"%s factor table\n",
				(btcfg->type == WNM_BSS_SELECT_TYPE_RSSI) ? "RSSI" : "CU");
		bytes_written += snprintf(command + bytes_written, total_len - bytes_written,
					"low\thigh\tfactor\n");
		for (i = 0; i <= btcfg->count-1; i++) {
			bytes_written += snprintf(command + bytes_written,
				total_len - bytes_written, "%d\t%d\t%d\n", btcfg->params[i].low,
				btcfg->params[i].high, btcfg->params[i].factor);
		}
		err = bytes_written;
		goto exit;
	} else {
		uint16 len = (sizeof(wnm_bss_select_factor_params_t) * WL_FACTOR_TABLE_MAX_LIMIT);
		memset_s(btcfg->params, len, 0, len);
		data += (strlen(rssi) + strlen(band) + 2);
		start_addr = data;
		slen = slen - (strlen(rssi) + strlen(band) + 2);
		for (i = 0; i < WL_FACTOR_TABLE_MAX_LIMIT; i++) {
			if (parsed_len + WBTEXT_TUPLE_MIN_LEN_CHECK <= slen) {
				btcfg->params[i].low = simple_strtol(data, &data, 10);
				data++;
				btcfg->params[i].high = simple_strtol(data, &data, 10);
				data++;
				btcfg->params[i].factor = simple_strtol(data, &data, 10);
				btcfg->count++;
				if (*data == '\0') {
					break;
				}
				data++;
				parsed_len = data - start_addr;
			} else {
				WL_ERR(("%s:Command usage:less no of args\n", __func__));
				goto exit;
			}
		}
		btcfg_len = sizeof(*btcfg) + ((btcfg->count) * sizeof(*btcfg));
		if ((err = wldev_iovar_setbuf(ndev, "wnm_bss_select_table", btcfg, btcfg_len,
				cfg->ioctl_buf, WLC_IOCTL_MEDLEN, &cfg->ioctl_buf_sync)) < 0) {
			WL_ERR(("seting wnm_bss_select_table failed with err %d\n", err));
			goto exit;
		}
	}
exit:
	if (btcfg) {
		MFREE(cfg->osh, btcfg,
			(sizeof(*btcfg) + sizeof(*btcfg) * WL_FACTOR_TABLE_MAX_LIMIT));
	}
	return err;
}

s32
wl_cfg80211_wbtext_delta_config(struct net_device *ndev, char *data, char *command, int total_len)
{
	uint i = 0;
	struct bcm_cfg80211 *cfg = wl_get_cfg(ndev);
	int err = -EINVAL, bytes_written = 0, argc = 0, val, len = 0;
	char delta[BUFSZN], band[BUFSZN], *endptr = NULL;
	wl_roamprof_band_t *rp = NULL;
	uint8 band_val = 0, roam_prof_size = 0, roam_prof_ver = 0;

	rp = (wl_roamprof_band_t *)MALLOCZ(cfg->osh, sizeof(*rp));
	if (unlikely(!rp)) {
		WL_ERR(("%s: failed to allocate memory\n", __func__));
		err = -ENOMEM;
		goto exit;
	}

	argc = sscanf(data, "%"SIZE_CONST_STRING(BUFSZ)"s %"SIZE_CONST_STRING(BUFSZ)"s",
		band, delta);
	if (BCME_BADBAND == wl_android_bandstr_to_fwband(band, &band_val)) {
		WL_ERR(("%s: Missing band\n", __func__));
		goto exit;
	}
	if ((err = wlc_wbtext_get_roam_prof(ndev, rp, band_val, &roam_prof_ver,
		&roam_prof_size))) {
		WL_ERR(("Getting roam_profile failed with err=%d \n", err));
		err = -EINVAL;
		goto exit;
	}
	if (argc == 2) {
		/* if delta is non integer returns command usage error */
		val = simple_strtol(delta, &endptr, 0);
		if (*endptr != '\0') {
			WL_ERR(("%s: Command usage error", __func__));
			goto exit;
		}
		for (i = 0; i < WL_MAX_ROAM_PROF_BRACKETS; i++) {
		/*
		 * Checking contents of roam profile data from fw and exits
		 * if code hits below condtion. If remaining length of buffer is
		 * less than roam profile size or if there is no valid entry.
		 */
			if (len >= rp->v1.len) {
				break;
			}
			if (roam_prof_ver == WL_ROAM_PROF_VER_1) {
				if (rp->v2.roam_prof[i].fullscan_period == 0) {
					break;
				}
				if (rp->v2.roam_prof[i].channel_usage != 0) {
					rp->v2.roam_prof[i].roam_delta = val;
				}
			} else if (roam_prof_ver == WL_ROAM_PROF_VER_2) {
				if (rp->v3.roam_prof[i].fullscan_period == 0) {
					break;
				}
				if (rp->v3.roam_prof[i].channel_usage != 0) {
					rp->v3.roam_prof[i].roam_delta = val;
				}
			} else if (roam_prof_ver == WL_ROAM_PROF_VER_3) {
				if (rp->v4.roam_prof[i].fullscan_period == 0) {
					break;
				}
				if (rp->v4.roam_prof[i].channel_usage != 0) {
					rp->v4.roam_prof[i].roam_delta = val;
				}
			}
			len += roam_prof_size;
		}
	}
	else {
		if (rp->v2.roam_prof[0].channel_usage != 0) {
			bytes_written = snprintf(command, total_len,
				"%s Delta %d\n", wl_android_get_band_str(rp->v1.band),
				rp->v2.roam_prof[0].roam_delta);
		}
		err = bytes_written;
		goto exit;
	}
	rp->v1.len = len;
	if ((err = wldev_iovar_setbuf(ndev, "roam_prof", rp,
			sizeof(*rp), cfg->ioctl_buf, WLC_IOCTL_MEDLEN,
			&cfg->ioctl_buf_sync)) < 0) {
		WL_ERR(("seting roam_profile failed with err %d\n", err));
	}
exit :
	if (rp) {
		MFREE(cfg->osh, rp, sizeof(*rp));
	}
	return err;
}
#endif /* WBTEXT */

#ifdef WL_UWB_COEX
static int
wl_android_uwbcx_set_enable(struct net_device *dev, const char *command)
{
	int enable;
	int start_ch;
	int end_ch;

	sscanf(command, "%d %d %d", &enable, &start_ch, &end_ch);

	return wl_cfg_uwb_coex_enable(dev, enable, start_ch, end_ch);
}

static int
wl_android_uwbcx_get_enable(struct net_device *dev, char *command, int tot_len)
{
	int i;
	int bytes_written = 0;

	int ret = BCME_OK;
	uint8 *resp_buf = NULL;

	bool enable = FALSE;
	uint16 start_ch = 0;
	uint16 end_ch = 0;
	bool is_bit_set = FALSE;

	bcm_iov_batch_subcmd_t *sub_cmd_ret;
	uwbcx_coex_bitmap_v2_t *coex_bitmap_cfg;
	uwbcx_coex_bitmap_t *coex_bitmap;

	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);

	resp_buf = (uint8 *)MALLOCZ(cfg->osh, WLC_IOCTL_SMLEN);
	if (unlikely(!resp_buf)) {
		WL_ERR(("%s - Failed to alloc mem\n", __FUNCTION__));
		ret = BCME_NOMEM;
		goto exit;
	}

	ret = wl_cfg_uwb_coex_execute_ioctl(dev, cfg, FALSE,
		WL_UWBCX_CMD_COEX_BITMAP, NULL, 0, resp_buf, WLC_IOCTL_SMLEN);
	if (!ret) {
		sub_cmd_ret = (bcm_iov_batch_subcmd_t *)(resp_buf +
				OFFSETOF(bcm_iov_batch_buf_t, cmds[0]));
		coex_bitmap_cfg = (uwbcx_coex_bitmap_v2_t *)(sub_cmd_ret->data);
		coex_bitmap = (uwbcx_coex_bitmap_t *)(&coex_bitmap_cfg->coex_bitmap);

		if (coex_bitmap_cfg->version !=  UWBCX_COEX_BITMAP_VERSION_V2) {
			WL_ERR(("%s - Failed due to UWB coex bitmap ver mismatch\n", __FUNCTION__));
			ret = BCME_VERSION;
			goto exit;
		}

		for (i = 0; i < UWB_COEX_CH_MAP_NUM; i++) {
			is_bit_set = FALSE;
			if (i < 16u) {
				is_bit_set = coex_bitmap->low_bitmap & (1 << i);
			} else if (i >= 16u && i < 32u) {
				is_bit_set = coex_bitmap->mid_low_bitmap & (1 << (i - 16u));
			} else if (i >= 32u && i < 48u) {
				is_bit_set = coex_bitmap->mid_high_bitmap & (1 << (i - 32u));
			} else if (i >= 48u) {
				is_bit_set = coex_bitmap->high_bitmap & (1 << (i - 48u));
			}

			if (is_bit_set) {
				if (!start_ch) {
					start_ch = wl_cfg_uwb_coex_get_ch_val(i);
				}
				end_ch = wl_cfg_uwb_coex_get_ch_val(i);
			}
		}

		enable = (start_ch > 0 && end_ch > 0) ? TRUE : FALSE;

		WL_ERR(("UWB Coex State: %s Ch.[%d/%d]\n",
			enable ? "Enable" : "Disable", start_ch, end_ch));
		bytes_written = snprintf(command, tot_len, "%s %d %d %d",
			CMD_GET_UWB_COEX_ENABLE, enable, start_ch, end_ch);
	}
	else {
		WL_ERR(("%s - Failed to get uwbcx enable\n", __FUNCTION__));
		bytes_written = ret;
	}
exit:
	if (resp_buf) {
		MFREE(cfg->osh, resp_buf, WLC_IOCTL_SMLEN);
	}

	return bytes_written;
}

static int
wl_android_uwbcx_set_prepare_time(struct net_device *dev, const char *command)
{
	int ret = BCME_OK;

	int param;
	uint8 adv_req_timer;
	uint8 *resp_buf = NULL;

	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);

	resp_buf = (uint8 *)MALLOCZ(cfg->osh, WLC_IOCTL_SMLEN);
	if (unlikely(!resp_buf)) {
		WL_ERR(("%s - Failed to alloc mem\n", __FUNCTION__));
		ret = BCME_NOMEM;
		goto exit;
	}

	sscanf(command, "%d", &param);
	if (param < 0 || param > 255) {
		ret = BCME_RANGE;
		goto exit;
	}
	adv_req_timer = (uint8)param;

	ret = wl_cfg_uwb_coex_execute_ioctl(dev, cfg, TRUE,
		WL_UWBCX_CMD_ADV_REQ_TIMER, &adv_req_timer, sizeof(adv_req_timer),
		resp_buf, WLC_IOCTL_SMLEN);
	if (!ret) {
		WL_ERR(("Set UWB Coex prepare time: %d\n", adv_req_timer));
	}
	else {
		WL_ERR(("%s - Failed to set UWB Coex prepare time %d\n",
			__FUNCTION__, adv_req_timer));
	}

exit:
	if (resp_buf) {
		MFREE(cfg->osh, resp_buf, WLC_IOCTL_SMLEN);
	}

	return ret;
}

static int
wl_android_uwbcx_get_prepare_time(struct net_device *dev, char *command, int tot_len)
{
	int ret = BCME_OK;
	int bytes_written = 0;

	uint8 adv_req_timer = 0;

	uint8 *resp_buf = NULL;
	bcm_iov_batch_subcmd_t *sub_cmd_ret;

	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);

	resp_buf = (uint8 *)MALLOCZ(cfg->osh, WLC_IOCTL_SMLEN);
	if (unlikely(!resp_buf)) {
		WL_ERR(("%s - Failed to alloc mem\n", __FUNCTION__));
		ret = BCME_NOMEM;
		goto exit;
	}

	ret = wl_cfg_uwb_coex_execute_ioctl(dev, cfg, FALSE,
		WL_UWBCX_CMD_ADV_REQ_TIMER, NULL, 0,
		resp_buf, WLC_IOCTL_SMLEN);
	if (!ret) {
		sub_cmd_ret = (bcm_iov_batch_subcmd_t *)(resp_buf +
				OFFSETOF(bcm_iov_batch_buf_t, cmds[0]));
		adv_req_timer = *(uint8 *)sub_cmd_ret->data;

		WL_ERR(("UWB Coex prepare time: %d\n", adv_req_timer));
		bytes_written = snprintf(command, tot_len, "%s %d",
				CMD_GET_UWB_COEX_PREPARE_TIME, adv_req_timer);
	}
	else {
		WL_ERR(("%s - Failed to get UWB Coex prepare time (%d)\n",
			__FUNCTION__, ret));
		bytes_written = ret;
	}

exit:
	if (resp_buf) {
		MFREE(cfg->osh, resp_buf, WLC_IOCTL_SMLEN);
	}

	return bytes_written;
}
#endif /* WL_UWB_COEX */

#if defined(CUSTOM_CONTROL_HE_6G_FEATURES)
int
wl_android_set_he_6g_band(struct net_device *dev, bool enable)
{
	s32 err = BCME_OK;
	s32 bssidx = 0;
	struct bcm_cfg80211 *cfg = NULL;

	if (!dev) {
		err = BCME_NOTFOUND;
		return err;
	}

	cfg = wl_get_cfg(dev);
	if (!cfg) {
		err = BCME_NOTFOUND;
		return err;
	}

	if ((bssidx = wl_get_bssidx_by_wdev(cfg, dev->ieee80211_ptr)) < 0) {
		WL_ERR(("find bss index from wdev failed\n"));
		err = BCME_NOTFOUND;
		return err;
	}
#ifdef DHD_PM_CONTROL_FROM_FILE
	if (g_pm_control) {
		enable = TRUE;
	}
#endif	/* DHD_PM_CONTROL_FROM_FILE */

	WL_ERR(("%s: Set he mode 6G band to %s\n", __FUNCTION__, enable ? "Enable" : "Disable"));
	/* Enable/disable for 6G */
	err = wl_cfg80211_set_he_mode(dev, cfg, bssidx, WL_HE_FEATURES_6G, enable);
	if (err != BCME_OK) {
		WL_ERR(("%s: failed to set he mode 6G band - err(%d)\n",
			__FUNCTION__, err));
	}

	return err;
}
#endif /* CUSTOM_CONTROL_HE_6G_FEATURES */

#if defined(LIMIT_AP_BW)
static int
wl_android_set_softap_bw(struct net_device *ndev, char *command)
{
	struct bcm_cfg80211 *cfg = NULL;
	uint32 bw;
	char *token, *pos;
	int err = BCME_OK;

	if (!ndev || (!(cfg = wl_get_cfg(ndev)))) {
		err = BCME_NOTFOUND;
		return err;
	}
	pos = command;

	/* drop command */
	token = bcmstrtok(&pos, " ", NULL);

	/* get band */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token) {
		WL_ERR((CMD_SET_SOFTAP_BW ": band is not specified\n"));
		return -EINVAL;
	}
	if (strncmp(token, "6g", strlen("6g"))) {
		WL_ERR((CMD_SET_SOFTAP_BW " support 6G only\n"));
		return -EINVAL;
	}

	/* get bandwidth */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token) {
		WL_ERR((CMD_SET_SOFTAP_BW ": bandwidth is not specified\n"));
		return -EINVAL;
	}
	bw = (uint32)bcm_atoi(token);
	err = wl_cfg80211_set_softap_bw(cfg, WL_CHANSPEC_BAND_6G, bw);
	if (err != BCME_OK) {
		return -EINVAL;
	}

	WL_INFORM(("SOFTAP BANDWITH LIMIT: %d\n", bw));
	return BCME_OK;
}

static int
wl_android_get_softap_bw(struct net_device *ndev, char *command, int total_len)
{
	struct bcm_cfg80211 *cfg = NULL;
	int err = BCME_OK;
	char *token, *pos;
	uint32 bw = 0;
	int rem_len = 0, bytes_written = 0;

	if (!ndev || (!(cfg = wl_get_cfg(ndev)))) {
		err = BCME_NOTFOUND;
		goto exit;
		return err;
	}

	pos = command;

	/* drop command */
	token = bcmstrtok(&pos, " ", NULL);

	/* get band */
	token = bcmstrtok(&pos, " ", NULL);
	if (!token) {
		WL_ERR((CMD_GET_SOFTAP_BW ": band is not specified\n"));
		err = -EINVAL;
		goto exit;
	}
	if (strncmp(token, "6g", strlen("6g"))) {
		WL_ERR((CMD_GET_SOFTAP_BW " support 6G only\n"));
		err = -EINVAL;
		goto exit;
	}

	bw = wl_cfg80211_get_ap_bw_limit_bit(cfg, WL_CHANSPEC_BAND_6G);
	bytes_written = scnprintf(command, rem_len, "%s = %d ",
		CMD_GET_SOFTAP_BW, bw);
	CHECK_SCNPRINTF_RET_VAL(bytes_written);

exit:
	WL_INFORM_MEM(("%s ret:%d bw:%d\n", CMD_GET_SOFTAP_BW, err, bw));
	return bytes_written;
}
#endif /* SUPPORT_AP_INIT_BWCONF */
