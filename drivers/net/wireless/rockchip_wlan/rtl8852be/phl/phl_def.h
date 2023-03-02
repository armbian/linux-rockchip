/******************************************************************************
 *
 * Copyright(c) 2019 - 2021 Realtek Corporation.
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
#ifndef _PHL_DEF_H_
#define _PHL_DEF_H_

enum phl_packet_type {
	PACKET_NORMAL,
	PACKET_DHCP,
	PACKET_ARP,
	PACKET_EAPOL,
	PACKET_EAPOL_START,
	PACKET_MAX
};

/*HW_BAND0 - CMAC0 + PHY0 + S0*/
/*HW_BAND1 - CMAC1 + PHY1 + S1*/
/*wifi_role->hw_band*/
enum phl_band_idx {
	HW_BAND_0,
	HW_BAND_1,
	HW_BAND_MAX
};

/*wifi_role->hw_port*/
enum phl_hw_port {
	HW_PORT0,
	HW_PORT1,
	HW_PORT2,
	HW_PORT3,
	HW_PORT4,
	HW_PORT_MAX,
};

#define RTW_MAX_TID_NUM 16
#define RTW_MAX_AC_QUEUE_NUM 4
enum phl_ac_queue {
	PHL_BE_QUEUE_SEL		= 0,
	PHL_BK_QUEUE_SEL		= 1,
	PHL_VI_QUEUE_SEL		= 2,
	PHL_VO_QUEUE_SEL		= 3,
	PHL_AC_QUEUE_TOTAL
};

enum phl_stat_info_query {
	STAT_INFO_FA_ALL,
	STAT_INFO_CCA_ALL,
};

/**
 * struct rtw_chan_def - channel defination
 * @chan: the (control/primary) channel
 * @center_ch: the center channel
 * @bw: channel bandwidth
 * @center_freq1: center frequency of first segment
 * @center_freq2: center frequency of second segment
 *	(only with 80+80 MHz)
 */

struct rtw_chan_def {
	enum band_type band; /* protocol -2.4G,5G,6G*/
	enum channel_width bw;
	enum chan_offset offset;
	u8 chan; /*primary channel*/
	u8 center_ch;
	u16 hw_value;
	u32 center_freq1;
	u32 center_freq2;
};

struct chg_opch_param {
	struct rtw_wifi_role_t *wrole;
	struct rtw_chan_def new_chdef;
	struct rtw_chan_def ori_chdef;
	enum rtw_phl_status cmd_start_sts;
	void (*chg_opch_done)(void *priv, u8 ridx, enum rtw_phl_status status);
};

/**
 * struct rtw_chan_ctx - channel context
 * @list:
 * @chan_ctx_lock:
 * @chan_def:
 */
struct rtw_chan_ctx {
	_os_list list;
	struct rtw_chan_def chan_def;
	u8 role_map; /*used role_idx*/
	bool dfs_enabled;
};


#ifdef CONFIG_PCI_HCI
struct rtw_pci_info {
	u8 dummy;
};
#endif


#ifdef CONFIG_USB_HCI
struct rtw_usb_info {
	enum rtw_usb_speed usb_speed; /* USB 1.1, 2.0 or 3.0 */
	u16 usb_bulkout_size;
	u8 outep_num;
	u8 inep_num;
};

enum phl_usb_rx_agg_mode {
	PHL_RX_AGG_DISABLE,
	PHL_RX_AGG_DEFAULT,
	PHL_RX_AGG_SMALL_PKT,
	PHL_RX_AGG_USER_DEFINE,
};
/*
 * refers to _usb.h
 * #define SWITCHMODE           0x2
 * #define FORCEUSB3MODE        0x1
 * #define FORCEUSB2MODE        0x0
*/
enum rtw_usb_sw_ability {
	RTW_USB2_ONLY = 0,
	RTW_USB3_ONLY,
	RTW_USB_SUPPORT_SWITCH,
	RTW_USB_SUPPORT_MAX
};
#endif

#ifdef CONFIG_SDIO_HCI
struct rtw_sdio_info {
	unsigned int clock;
	unsigned int timing;
	u8 sd3_bus_mode;
	u16 block_sz;
	u16 io_align_sz;
	u16 tx_align_sz;
	bool tx_512_by_byte_mode;	/* Send 512 bytes by cmd53 byte or */
					/* block mode. */
};
#endif

enum rtw_rx_status {
	RTW_STATUS_RX_OK,
	RTW_STATUS_RXDMA_HANG,
	RTW_STATUS_RXFIFO_HANG
};

struct rtw_ic_info {
	enum rtl_ic_id ic_id;
	enum rtw_hci_type hci_type;
	#ifdef CONFIG_SDIO_HCI
	struct rtw_sdio_info sdio_info;
	#endif

	#ifdef CONFIG_USB_HCI
	struct rtw_usb_info usb_info;
	#endif

	#ifdef CONFIG_PCI_HCI
	struct rtw_pci_info pci_info;
	#endif
};

enum rtw_proc_cmd_type {
	RTW_PROC_CMD_UNKNOW,
	RTW_PROC_CMD_BB,	/* 1 */
	RTW_PROC_CMD_RF,	/* 2 */
	RTW_PROC_CMD_MAC,	/* 3 */
	RTW_PROC_CMD_PHL,	/* 4 */
	RTW_PROC_CMD_CORE,	/* 5 */
	RTW_PROC_CMD_BTC,	/* 6 */
	RTW_PROC_CMD_EFUSE,	/* 7 */
	RTW_PROC_CMD_MAX
};

enum rtw_arg_type {
	RTW_ARG_TYPE_UNKNOW,
	RTW_ARG_TYPE_BUF,	/* 1 */
	RTW_ARG_TYPE_ARRAY,	/* 2 */
	RTW_ARG_TYPE_MAX
};

#define		MAX_ARGC	20
#define		MAX_ARGV	16


struct rtw_proc_cmd {
	enum rtw_arg_type in_type;
	u32 in_cnt_len;
	union {
		char *buf;
		char vector[MAX_ARGC][MAX_ARGV];
	}in;
};

enum rtw_para_src {
	RTW_PARA_SRC_INTNAL, /* 0 */
	RTW_PARA_SRC_EXTNAL, /* 1 */
	RTW_PARA_SRC_EXTNAL_BUF, /* 2 */
	RTW_PARA_SRC_CUSTOM, /* 3 */
	RTW_PARA_SRC_MAX
};

struct rtw_para_info_t {
	enum rtw_para_src para_src;
	char para_path[256];
	char *hal_phy_folder;
	char postfix[33];

	u8 *ext_para_file_buf;
	u32 ext_para_file_buf_len;
	u32 para_data_len;
	u32 *para_data;
};

#define regd_name_max_size 32

struct rtw_para_pwrlmt_info_t {
	enum rtw_para_src para_src;
	char para_path[256];
	char *hal_phy_folder;
	char postfix[33];

	u8 *ext_para_file_buf;
	u32 ext_para_file_buf_len;
	u32 para_data_len;
	u32 *para_data;

	char ext_regd_name[regd_name_max_size][10];
	u16 ext_regd_arridx;
	u16 ext_reg_map_num;
	u8 *ext_reg_codemap;
};

#define RTW_PHL_HANDLER_STATUS_INITIALIZED BIT0
#define RTW_PHL_HANDLER_STATUS_SET BIT1
#define RTW_PHL_HANDLER_STATUS_RELEASED BIT2

#define RTW_PHL_HANDLER_PRIO_HIGH 0
#define RTW_PHL_HANDLER_PRIO_NORMAL 1
#define RTW_PHL_HANDLER_PRIO_LOW 2

enum rtw_phl_evt {
	RTW_PHL_EVT_RX = BIT0,
	RTW_PHL_EVT_TX_RECYCLE = BIT1,

	RTW_PHL_EVT_MAX = BIT31
};

enum rtw_phl_config_int {
	RTW_PHL_STOP_RX_INT,
	RTW_PHL_RESUME_RX_INT,
	RTW_PHL_SER_HANDSHAKE_MODE,
	RTW_PHL_EN_HCI_INT,
	RTW_PHL_DIS_HCI_INT,
	RTW_PHL_CLR_HCI_INT,
	RTW_PHL_CONFIG_INT_MAX
};

/**
 * phl_handler - scheduled by core layer or phl itself
 * and the properties is assigned by different hanlder type
 * @status: handler current status defined by RTW_PHL_HANDLER_STATUS_XXX
 * @type: define different properties of handler - tasklet, thread, workitem
 * @handle: store different type of handler structure
 * @callback: handler callback function
 * @context: context used in handler callback function
 */
struct rtw_phl_handler {
	char status;
	char type;
	void *drv_priv;
	struct _os_handler os_handler;
	void (*callback)(void *context);
	void *context;
};

struct rtw_xmit_req;
struct rtw_aoac_report;
struct rtw_phl_evt_ops {
	enum rtw_phl_status (*rx_process)(void *drv_priv);
	enum rtw_phl_status (*tx_recycle)(void *drv_priv, struct rtw_xmit_req *txreq);
	enum rtw_phl_status (*tx_test_recycle)(void *phl, struct rtw_xmit_req *txreq);
	bool (*set_rf_state)(void *drv_priv, enum rtw_rf_state state_to_set);
	void (*wow_handle_sec_info_update)(void *drv_priv, struct rtw_aoac_report *aoac_info, u8 aoac_report_get_ok, u8 phase);
	void (*indicate_wake_rsn)(void *drv_priv, u8 rsn);
#ifdef CONFIG_SYNC_INTERRUPT
	void (*interrupt_restore)(void *drv_priv, u8 rx);
	void (*set_interrupt_caps)(void *drv_priv, u8 en);
#endif /* CONFIG_SYNC_INTERRUPT */
	void (*ap_ps_sta_ps_change)(void *drv_priv, u8 role_id, u8 *sta_mac,
	                            int power_save);
	u8 (*issue_null_data)(void *priv, u8 ridx, bool ps);
};

/*
 * PHL CMD support direct execution, no-wait: synchronization, wait:asynchronization
 * PHL_CMD_CMD_DIRECTLY: call PHL API including I/O operation directly
 * PHL_CMD_NO_WARIT: send phl cmd msg to cmd dispatcher and do not wait for completion
 * PHL_CMD_WAIT: send phl cmd msg to cmd dispatcher and wait for completion
 */
enum phl_cmd_type {
	PHL_CMD_DIRECTLY,
	PHL_CMD_NO_WAIT,
	PHL_CMD_WAIT,
	PHL_CMD_MAX,
};

enum role_type {
	PHL_RTYPE_NONE,
	PHL_RTYPE_STATION,
	PHL_RTYPE_AP,
	PHL_RTYPE_VAP,
	PHL_RTYPE_ADHOC,
	PHL_RTYPE_ADHOC_MASTER,
	PHL_RTYPE_MESH,
	PHL_RTYPE_MONITOR,
	PHL_RTYPE_P2P_DEVICE,
	PHL_RTYPE_P2P_GC,
	PHL_RTYPE_P2P_GO,
	PHL_RTYPE_TDLS,
	PHL_RTYPE_NAN,
	PHL_MLME_MAX
};

enum role_state {
	PHL_ROLE_START, /* 0 - PHL*/
	PHL_ROLE_STOP, /* 1 - PHL*/
	PHL_ROLE_CHG_TYPE, /* 2 - PHL*/
	PHL_ROLE_UPDATE_NOA, /* 3 - PHL*/
	PHL_ROLE_MSTS_STA_CONN_START, /*CORE*/
	PHL_ROLE_MSTS_STA_CONN_END,/*CORE*/
	PHL_ROLE_MSTS_STA_DIS_CONN,/*CORE*/
	PHL_ROLE_MSTS_AP_START,/*CORE*/
	PHL_ROLE_MSTS_AP_STOP,/*CORE*/
	PHL_ROLE_STATE_UNKNOWN,
};

enum mlme_state {
	MLME_NO_LINK,
	MLME_LINKING,
	MLME_LINKED
};
enum wr_chg_id {
	WR_CHG_TYPE,
	WR_CHG_MADDR,
	WR_CHG_AP_PARAM,
	WR_CHG_EDCA_PARAM,
	WR_CHG_MU_EDCA_PARAM,
	WR_CHG_MU_EDCA_CFG,
	WR_CHG_BSS_COLOR,
	WR_CHG_RTS_TH,
	WR_CHG_DFS_HE_TB_CFG,
	WR_CHG_TRX_PATH,
	WR_CHG_STBC_CFG,
	WR_CHG_MAX,
};

enum wr_status{
	WR_STATUS_PS_ANN = BIT0,
	WR_STATUS_BCN_STOP = BIT1,
	WR_STATUS_TSF_SYNC = BIT2,
	WR_STATUS_MAX = BIT7
};

enum rtw_cfg_type { /* sync with pcfg_type */
	CFG_TBTT_AGG,
	CFG_TBTT_SHIFT,
	CFG_HIQ_WIN,
	CFG_HIQ_DTIM,
	CFG_HIQ_MAX,
	CFG_BCN_INTERVAL,	/* Beacon Interval */
	CFG_BSS_CLR
};

struct rtw_ap_param {
	u32 cfg_id;
	u32 value;
};

struct rtw_edca_param {
	/* Access Category, 0:BE, 1:BK, 2:VI, 3:VO */
	u8 ac;
	/*
	 * EDCA parameter
	 * |31...16|15...12|11...8|7...0|
	 * |   TXOP|  CWMAX| CWMIN| AIFS|
	 */
	u32 param;
};

struct rtw_mu_edca_param {
	u8 ac;
	u8 aifsn;
	u8 cw;
	u8 timer;
};

struct rtw_trx_path_param {
	enum rf_path tx;
	enum rf_path rx;
	u8 tx_nss;
	u8 rx_nss;
};

#define MAX_STORE_BCN_NUM 3
enum conf_lvl {
	CONF_LVL_NONE = 0,
	CONF_LVL_LOW,
	CONF_LVL_MID,
	CONF_LVL_HIGH
};

struct rtw_bcn_offset {
	u16 offset; /*TU*/
	enum conf_lvl conf_lvl; /*confidence level*/
	u16 cr_tbtt_shift; /* CR current setting */
};

/*
 * Store rx bcn tsf info
 * @num: the store noumber of "info" array
 * @idx: store current index of "info" array
 * @info: store array. info[0]: store tsf, info[1]: store mod(TU), info[2]: store hw rx time
 * @offset_i: Bcn offset info. Dont't access directionly this variable for application.
                   You can get offset_i info from phl_get_sta_bcn_offset_info.
 */
struct rtw_rx_bcn_info {
	u8 idx;
	u8 num;
	u64 info[3][MAX_STORE_BCN_NUM];
	struct rtw_bcn_offset offset_i;
};

struct rtw_bcn_pkt_info {
	struct rtw_phl_stainfo_t *sta;
	u64 tsf;
	u64 hw_tsf;
};

struct rtw_rts_threshold {
	u16 rts_time_th;
	u16 rts_len_th;
};

enum phl_module_id{
	/* 0 ~ 128 PHL background module starts from here*/
	/* 1,2,3 cmd controller section */
	PHL_BK_MDL_START = 0,
	PHL_MDL_PHY_MGNT = 1,
	PHL_MDL_TX = 2,
	PHL_MDL_RX = 3,

	/* above enum is fixed, add new module id from here*/
	/* 10 ~ 40 protocol, wifi role section*/
	PHL_BK_MDL_ROLE_START = 10,
	PHL_MDL_MRC = 10, /* Multi-Role Controller intead of STA/P2P role /NAN/AP*/
	PHL_MDL_SOUND = 11,

	PHL_BK_MDL_ROLE_END = 40,

	/* 41 ~ 70 mandatory background module section*/
	PHL_BK_MDL_MDRY_START = 41,
	PHL_MDL_POWER_MGNT = 41,
	PHL_MDL_SER = 42,

	PHL_BK_MDL_MDRY_END = 70,

	/* 70 ~ 127 optional background module section*/
	PHL_BK_MDL_OPT_START = 71,
	PHL_MDL_BTC = 71,
	/*PHL_MDL_RSVD = 72,*/
	PHL_MDL_CUSTOM = 73,
	PHL_MDL_WOW = 74,
	PHL_MDL_PSTS = 75,
	PHL_MDL_LED = 76,
	PHL_MDL_GENERAL = 77,
	PHL_MDL_REGU = 78,

	PHL_BK_MDL_OPT_END = 127,

	/* Fixed BK MDL Max Value*/
	PHL_BK_MDL_END = 128,

	/* 129 ~ 256 PHL foreground module starts from here*/
	PHL_FG_MDL_START = 129,
	PHL_FUNC_MDL_TEST_MODULE = 129,
	PHL_FG_MDL_SCAN = 130,
	PHL_FG_MDL_CONNECT = 131,
	PHL_FG_MDL_DISCONNECT = 132,
	PHL_FG_MDL_AP_START = 133,
	PHL_FG_MDL_AP_STOP = 134,
	PHL_FG_MDL_ECSA = 135,
	PHL_FG_MDL_END = 254,

	/* Fixed MDL Max Value*/
	PHL_MDL_ID_MAX = 255
};

/* General phl event id shall share this common enum definition
 * if definition of private events for a specific module is required,
 * please be sure to start its enum from PRIVATE_EVT_START(0x8000)
 */
enum phl_msg_evt_id {
	MSG_EVT_NONE = 0,
	MSG_EVT_PHY_ON = 1,
	MSG_EVT_PHY_IDLE = 2,
	MSG_EVT_SCAN_START = 3,
	MSG_EVT_SCAN_END = 4,
	MSG_EVT_CONNECT_START = 5,
	MSG_EVT_CONNECT_LINKED = 6,
	MSG_EVT_CONNECT_END = 7,
	MSG_EVT_SER_L1 = 8,
	MSG_EVT_SER_L2 = 9,
	MSG_EVT_FWDL_OK = 10,
	MSG_EVT_FWDL_FAIL = 11,
	MSG_EVT_HAL_INIT_OK = 12,
	MSG_EVT_HAL_INIT_FAIL = 13,
	MSG_EVT_MP_CMD_DONE = 14,
	/* wow */
	MSG_EVT_WOW_ENTER = 15,
	MSG_EVT_WOW_LEAVE = 16,
	MSG_EVT_WOW_WAKE_RSN = 17,
	MSG_EVT_BCN_RESEND = 18,
	MSG_EVT_DUMP_PLE_BUFFER = 19,
	MSG_EVT_MP_RX_PHYSTS = 20,
	MSG_EVT_ROLE_NTFY = 21,
	MSG_EVT_RX_PSTS = 22,
	MSG_EVT_SWCH_START = 23,
	MSG_EVT_SWCH_DONE = 24,
	MSG_EVT_DISCONNECT_PREPARE = 25,
	MSG_EVT_DISCONNECT = 26,
	MSG_EVT_TSF_SYNC_DONE = 27,
	MSG_EVT_TX_RESUME = 28,
	MSG_EVT_AP_START_PREPARE =29,
	MSG_EVT_AP_START = 30,
	MSG_EVT_AP_START_END = 31,
	MSG_EVT_AP_STOP_PREPARE = 32,
	MSG_EVT_AP_STOP = 33,
	MSG_EVT_PCIE_TRX_MIT = 34,
	MSG_EVT_BTC_TMR = 35,
	MSG_EVT_BTC_FWEVNT = 36,
	MSG_EVT_BTC_REQ_BT_SLOT = 37,
	MSG_EVT_BTC_PKT_EVT_NTFY = 38,
	/* ser*/
	MSG_EVT_SER_L0_RESET = 39,		/* L0 notify only */
	MSG_EVT_SER_M1_PAUSE_TRX = 40,
	MSG_EVT_SER_IO_TIMER_EXPIRE = 41,
	MSG_EVT_SER_FW_TIMER_EXPIRE = 42,
	MSG_EVT_SER_M3_DO_RECOV = 43,
	MSG_EVT_SER_M5_READY = 44,
	MSG_EVT_SER_M9_L2_RESET = 45,
	MSG_EVT_SER_EVENT_CHK = 46,
	MSG_EVT_SER_POLLING_CHK = 47,
	MSG_EVT_ECSA_START = 48,
	MSG_EVT_ECSA_UPDATE_FIRST_BCN_DONE = 49,
	MSG_EVT_ECSA_COUNT_DOWN = 50,
	MSG_EVT_ECSA_SWITCH_START = 51,
	MSG_EVT_ECSA_SWITCH_DONE = 52,
	MSG_EVT_ECSA_CHECK_TX_RESUME = 53,
	MSG_EVT_ECSA_DONE = 54,
	MSG_EVT_LISTEN_STATE_EXPIRE = 55,
	/* beamform */
	MSG_EVT_SET_VHT_GID = 56,
	MSG_EVT_HW_WATCHDOG = 57,
	MSG_EVT_DEV_CANNOT_IO = 58,
	MSG_EVT_DEV_RESUME_IO = 59,
	MSG_EVT_FORCE_USB_SW = 60,
	MSG_EVT_GET_USB_SPEED = 61,
	MSG_EVT_GET_USB_SW_ABILITY = 62,
	MSG_EVT_CFG_AMPDU = 63,
	MSG_EVT_DFS_PAUSE_TX = 64,
	MSG_EVT_ROLE_RECOVER = 65,
	MSG_EVT_ROLE_SUSPEND = 66,
	MSG_EVT_HAL_SET_L2_LEAVE = 67,
	MSG_EVT_NOTIFY_HAL = 68,
	MSG_EVT_ISSUE_BCN = 69,
	MSG_EVT_FREE_BCN = 70,
	MSG_EVT_STOP_BCN = 71,
	MSG_EVT_SEC_KEY = 72,
	MSG_EVT_ROLE_START = 73,
	MSG_EVT_ROLE_CHANGE = 74,
	MSG_EVT_ROLE_STOP = 75,
	MSG_EVT_STA_INFO_CTRL = 76,
	MSG_EVT_STA_MEDIA_STATUS_UPT = 77,
	MSG_EVT_CFG_CHINFO = 78,
	MSG_EVT_STA_CHG_STAINFO = 79,
	MSG_EVT_HW_TRX_RST_RESUME = 80,
	MSG_EVT_HW_TRX_PAUSE = 81,
	MSG_EVT_SW_TX_RESUME = 82,
	MSG_EVT_SW_RX_RESUME = 83,
	MSG_EVT_SW_TX_PAUSE = 84,
	MSG_EVT_SW_RX_PAUSE = 85,
	MSG_EVT_SW_TX_RESET = 86,
	MSG_EVT_SW_RX_RESET = 87,
	MSG_EVT_TRX_SW_PAUSE = 88,
	MSG_EVT_TRX_SW_RESUME = 89,
	MSG_EVT_TRX_PAUSE_W_RST = 90,
	MSG_EVT_TRX_RESUME_W_RST = 91,
	/* Regulation*/
	MSG_EVT_REGU_SET_DOMAIN = 92,
	MSG_EVT_RF_ON = 93,
	MSG_EVT_RF_OFF = 94,
	MSG_EVT_WPS_PRESSED = 95,
	MSG_EVT_WPS_RELEASED = 96,
	MSG_EVT_SURPRISE_REMOVE = 97,
	MSG_EVT_DATA_PATH_START = 98,
	MSG_EVT_DATA_PATH_STOP = 99,
	MSG_EVT_TRX_PWR_REQ = 100,
	/* tdls */
	MSG_EVT_TDLS_SYNC = 101,
	/* beamformee */
	MSG_EVT_SET_BFEE_AID = 102,
	/* ccx */
	MSG_EVT_CCX_REPORT_TX_OK = 103,
	MSG_EVT_CCX_REPORT_TX_FAIL = 104,
	/* ps */
	MSG_EVT_PS_CAP_CHG = 105,
	MSG_EVT_PS_PERIOD_CHK = 106,
	MSG_EVT_PS_DBG_LPS_ENTER = 107,
	MSG_EVT_PS_DBG_LPS_LEAVE = 108,
	MSG_EVT_PS_DBG_IPS_ENTER = 109,
	MSG_EVT_PS_DBG_IPS_LEAVE = 110,
	/* Change operating ch def(ch / bw) */
	MSG_EVT_CHG_OP_CH_DEF_START = 111,
	MSG_EVT_CHG_OP_CH_DEF_END = 112,
	MSG_EVT_MDL_CHECK_STOP = 113,
	MSG_EVT_HW_RF_CHG = 114,

	MSG_EVT_TX_PKT_NTFY = 115,
	MSG_EVT_SW_WATCHDOG = 116,
	/* ltr */
	MSG_EVT_LTR_TX_DLY = 199,
	/* dbg */
	MSG_EVT_DBG_SIP_REG_DUMP = 200,
	MSG_EVT_DBG_FULL_REG_DUMP = 201,
	MSG_EVT_DBG_L2_DIAGNOSE = 202,
	MSG_EVT_DBG_RX_DUMP = 203,
	MSG_EVT_DBG_TX_DUMP = 204,
	/* dbg end */
	/* p2pps */
	MSG_EVT_TSF32_TOG = 205,
	/* p2pps end */
	/*Add EVT-ID for linux core cmd temporality*/

	/* sub module IO */
	MSG_EVT_NOTIFY_BB = 300,
	MSG_EVT_NOTIFY_RF = 301,
	MSG_EVT_NOTIFY_MAC = 302,
	/* sub module IO end*/

	MSG_EVT_LINUX_CMD_WRK = 888,
	MSG_EVT_LINUX_CMD_WRK_TRI_PS = 889,
	/* LED */
	MSG_EVT_LED_TICK = 5000,
	MSG_EVT_LED_EVT_START = 5001,
	MSG_EVT_LED_EVT_END = 5050,
	MSG_EVT_MAX = 0x7fff
};



enum phl_msg_recver_layer {
	MSG_RECV_PHL = 0,
	MSG_RECV_CORE = 1,
	MSG_RECV_MAX
};

enum phl_msg_indicator {
	MSG_INDC_PRE_PHASE = BIT0,
	MSG_INDC_FAIL = BIT1,
	MSG_INDC_CANCEL = BIT2,
	MSG_INDC_CANNOT_IO = BIT3
};

enum phl_msg_opt {
	MSG_OPT_SKIP_NOTIFY_OPT_MDL = BIT0,
	MSG_OPT_BLIST_PRESENT = BIT1,
	MSG_OPT_CLR_SNDR_MSG_IF_PENDING = BIT2,
	MSG_OPT_SEND_IN_ABORT = BIT3,
	MSG_OPT_PENDING_DURING_CANNOT_IO = BIT4,
};


/* all module share this common enum definition */
enum phy_bk_module_opcode {
	BK_MODL_OP_NONE = 0,
	BK_MODL_OP_CHK_NEW_MSG,
	BK_MODL_OP_INPUT_CMD,
	BK_MODL_OP_STATE,
	BK_MODL_OP_CUS_SET_ROLE_CAP,
	BK_MODL_OP_CUS_UPDATE_ROLE_CAP,
	BK_MODL_OP_MAX
};

/* Foreground cmd token opcode */
enum phy_fg_cmd_req_opcode {
	FG_REQ_OP_NONE = 0,
	FG_REQ_OP_GET_ROLE,
	FG_REQ_OP_GET_MDL_ID,
#ifdef RTW_WKARD_MRC_ISSUE_NULL_WITH_SCAN_OPS
	FG_REQ_OP_GET_SCAN_PARAM,
	FG_REQ_OP_GET_ISSUE_NULL_OPS,
#endif
#ifdef RTW_WKARD_CMD_SCAN_EXTEND_ACTIVE_SCAN
	FG_REQ_OP_NOTIFY_BCN_RCV,
#endif
#ifdef RTW_WKARD_CMD_SCAN_EXTEND_ACTION_FRAME_TX
	FG_REQ_OP_NOTIFY_ACTION_FRAME_TX,
#endif
	FG_REQ_OP_MAX
};

/* priority of phl background
module which would be considered when dispatching phl msg*/
enum phl_bk_module_priority {
	PHL_MDL_PRI_ROLE = 0,
	PHL_MDL_PRI_OPTIONAL,
	PHL_MDL_PRI_MANDATORY,
	PHL_MDL_PRI_MAX
};

enum phl_data_ctl_cmd {
	PHL_DATA_CTL_HW_TRX_RST_RESUME = 1,
	PHL_DATA_CTL_HW_TRX_PAUSE = 2,
	PHL_DATA_CTL_SW_TX_RESUME = 3,
	PHL_DATA_CTL_SW_RX_RESUME = 4,
	PHL_DATA_CTL_SW_TX_PAUSE = 5,
	PHL_DATA_CTL_SW_RX_PAUSE = 6,
	PHL_DATA_CTL_SW_TX_RESET = 7,
	PHL_DATA_CTL_SW_RX_RESET = 8,
	PHL_DATA_CTL_TRX_SW_PAUSE = 9,
	PHL_DATA_CTL_TRX_SW_RESUME = 10,
	PHL_DATA_CTL_TRX_PAUSE_W_RST = 11,
	PHL_DATA_CTL_TRX_RESUME_W_RST = 12,
	PHL_DATA_CTL_MAX = 0xFF
};

/**
 * phl_msg - define a general msg format for PHL/CORE layer module to handle
 * one can easily extend additional mgnt info by encapsulating inside a file
 * refer to
 *		struct phl_msg_ex 		in phl_msg_hub.c
 *		struct phl_dispr_msg_ex		in phl_cmd_dispatcher.c
 *
 * @msg_id: indicate msg source & msg type
 *	    BYTE 3: RSVD
 *	    BYTE 2: PHL Module ID,  refer to enum phl_module_id
 *	    BYTE 0-1: event id, refer to enum phl_msg_evt_id
 * @inbuf: input buffer that sent along with msg
 * @inlen: input buffer length
 * @outbuf: output buffer that returned after all phl modules have recved msg.
 * @outlen: output buffer length
 * @band_idx: index of Band(PHY) which associate to this msg

 * @rsvd: feature reserved, passing object pointer.
 *        For example,
 *        - cmd_scan : [0]: wifi_role.
 *        - CANNOT_IO error: [0]: mdl handle.
 */
struct phl_msg{
	u32 msg_id;
	enum phl_band_idx band_idx;
	u8* inbuf;
	u8* outbuf;
	u32 inlen;
	u32 outlen;
	void *rsvd[4];
};

struct msg_notify_map {
	u8* id_arr;
	u8 len;
};
struct msg_dispatch_seq {
	struct msg_notify_map map[PHL_MDL_PRI_MAX];
};
struct msg_self_def_seq {
	struct msg_dispatch_seq pre_prot_phase;
	struct msg_dispatch_seq post_prot_phase;
};
struct msg_completion_routine {
	void* priv;
	void (*completion)(void* priv, struct phl_msg* msg);
};
/**
 * phl_msg_attribute: used in phl_disp_eng_send_msg
 * @opt: refers to enum phl_msg_opt.
 * @notify: input id array (refer to enum phl_module_id)
 * 	    for indicating additional dependency
 * @completion: completion routine
 */
struct phl_msg_attribute {
	u8 opt;
	struct msg_notify_map notify;
	struct msg_completion_routine completion;
#ifdef CONFIG_CMD_DISP_SUPPORT_CUSTOM_SEQ
	void *dispr_attr;
#endif
};

/**
 * phl_module_op_info - set by core layer or phl itself,
 * op code process is an synchronous process.
 * which would be handled directly by module handler
 * @op_code: refer to enum phy_module_opcode
 * @inbuf: input buffer that sent along with msg
 * @inlen: input buffer length
 * @outbuf: output buffer that returned after all phy modules have recved msg.
 * @outlen: output buffer length
 */
struct phl_module_op_info{
	u32 op_code;
	u8* inbuf;
	u8* outbuf;
	u32 inlen;
	u32 outlen;
};

/**
 * phl_cmd_token_req - request foramt for applying token of a specific cmd
 * dispatcher.
 * cmd token request is regarded as foreground module, thus,
 * need to contend for cmd token.
 * Normally, these req would be linked to a specific wifi role
 * and acquiring RF resource for a specific task.
 *
 * @module_id: starting from PHL_FG_MDL_START
 * @priv: private context from requestor
 * @role: designated role info associated with current request.
 * -----------------------------------------
 * regarding on "return code" for following ops, refer to enum phl_mdl_ret_code
 * -----------------------------------------
 * @acquired: notify requestor when cmd token has acquired for this cmd and
              cannot have any I/O operation.
 * @abort: notify requestor when cmd has been canceled
           after calling rtw_phl_phy_cancel_token_req and
           cannot have any I/O operation.
 * @msg_hdlr: notify requestor about incoming msg.
 * @set_info: notify requestor to handle specific op code.
 * @query_info: notify requestor to handle specific op code.
 */

struct phl_cmd_token_req{
	u8 module_id;
	void* priv;
	void* role;
	enum phl_mdl_ret_code (*acquired)(void* dispr, void* priv);
	enum phl_mdl_ret_code (*abort)(void* dispr, void* priv);
	enum phl_mdl_ret_code (*msg_hdlr)(void* dispr, void* priv,
							struct phl_msg* msg);
	enum phl_mdl_ret_code (*set_info)(void* dispr, void* priv,
					struct phl_module_op_info* info);
	enum phl_mdl_ret_code (*query_info)(void* dispr, void* priv,
				struct phl_module_op_info* info);
};
/**
 * phl_module_ops - standard interface for interacting with a cmd dispatcher.
 * -----------------------------------------
 * regarding on "return code" for following ops, refer to enum phl_mdl_ret_code
 * -----------------------------------------
 * @init: notify module for initialization.
 * @deinit: notify module for de-initialization.
 * @start: notify module to start.
 * @stop: notify module to stop.
 * @msg_hdlr: notify module about incoming msg.
 * @set_info: notify module to handle specific op code.
 * @query_info: notify module to handle specific op code.
 */
struct phl_bk_module_ops {
	enum phl_mdl_ret_code (*init)(void* phl_info, void* dispr, void** priv);
	void (*deinit)(void* dispr, void* priv);
	enum phl_mdl_ret_code (*start)(void* dispr, void* priv);
	enum phl_mdl_ret_code (*stop)(void* dispr, void* priv);
	enum phl_mdl_ret_code (*msg_hdlr)(void* dispr, void* priv,
							struct phl_msg* msg);
	enum phl_mdl_ret_code (*set_info)(void* dispr, void* priv,
			struct phl_module_op_info* info);
	enum phl_mdl_ret_code (*query_info)(void* dispr, void* priv,
			struct phl_module_op_info* info);
};

/**
 * phl_data_ctl_t - datapath control parameters for dispatcher controller
 * @cmd: data path control command
 * @id: module id which request data path control
 */
struct phl_data_ctl_t {
	enum phl_data_ctl_cmd cmd;
	enum phl_module_id id;
};

#define MSG_MDL_ID_FIELD(_msg_id) (((_msg_id) >> 16) & 0xFF)
#define MSG_EVT_ID_FIELD(_msg_id) ((_msg_id) & 0xFFFF)
#define MSG_INDC_FIELD(_msg_id) (((_msg_id) >> 24) & 0xFF)
#define IS_PRIVATE_MSG(_msg_id) ((_msg_id) & PRIVATE_EVT_START)
#define IS_MSG_FAIL(_msg_id) ((_msg_id) & ( MSG_INDC_FAIL <<  24))
#define IS_MSG_IN_PRE_PHASE(_msg_id) ((_msg_id) & ( MSG_INDC_PRE_PHASE <<  24))
#define IS_MSG_CANCEL(_msg_id) ((_msg_id) & ( MSG_INDC_CANCEL <<  24))
#define IS_MSG_CANNOT_IO(_msg_id) ((_msg_id) & ( MSG_INDC_CANNOT_IO <<  24))
#define SET_MSG_MDL_ID_FIELD(_msg_id, _id) \
	((_msg_id) = (((_msg_id) & 0xFF00FFFF) | ((u32)(_id) << 16)))
#define SET_MSG_EVT_ID_FIELD(_msg_id, _id) \
	((_msg_id) = (((_msg_id) & 0xFFFF0000) | (_id)))
#define SET_MSG_INDC_FIELD(_msg_id, _indc) \
	((_msg_id) = (((_msg_id) & ~((u32)(_indc) << 24))|((u32)(_indc) << 24)))
#define CLEAR_MSG_INDC_FIELD(_msg_id, _indc) ((_msg_id) &= ~((_indc) << 24))

#ifdef CONFIG_PHL_REDUCE_MEM
#define RTW_MAX_FW_SIZE 0x100000
#else
#define RTW_MAX_FW_SIZE 0x400000
#endif

enum rtw_fw_src {
	RTW_FW_SRC_INTNAL, /* 0 */
	RTW_FW_SRC_EXTNAL, /* 1 */
	RTW_FW_SRC_MAX
};

enum rtw_fw_rsn {
	RTW_FW_RSN_INIT, /* 0 */
	RTW_FW_RSN_SPIC, /* 1 */
	RTW_FW_RSN_LPS, /* 2 */
	RTW_FW_RSN_MCC, /* 3 */
	RTW_FW_RSN_WOW, /* 4 */
	RTW_FW_RSN_MAX
};

struct rtw_fw_cap_t {
	enum rtw_fw_src fw_src;
	u32 offload_cap;
	u8 dlram_en;
	u8 dlrom_en;
};

#define INVALID_WIFI_ROLE_IDX MAX_WIFI_ROLE_NUMBER
#define UNSPECIFIED_ROLE_ID 0xFF
#define MAX_SECCAM_NUM_PER_ENTRY 7

/* Role hw TX CAP*/
struct role_cap_t {
	enum wlan_mode wmode;
	enum channel_width bw;
	u8 rty_lmt;		/* retry limit for DATA frame, 0xFF: invalid */
	u8 rty_lmt_rts;		/* retry limit for RTS frame, 0xFF: invalid */

	u8 tx_num_ampdu;
	u8 tx_amsdu_in_ampdu; /*from SW & HW*/
	u8 tx_ampdu_len_exp; /*from  SW & HW*/
	u8 tx_htc;
	u8 tx_sgi;

	u8 tx_ht_ldpc:1;
	u8 tx_vht_ldpc:1;
	u8 tx_he_ldpc:1;
	u8 tx_ht_stbc:1;
	u8 tx_vht_stbc:1;
	u8 tx_he_stbc:1;

	u8 supported_rates[12];
};

struct role_sw_cap_t {
	u16 bf_cap; /* use define : HW_CAP_BFER_XX_XX */
	u16 stbc_cap;/* use define: HW_CAP_STBC_XX */
};

/*
Protocol - RX CAP from 80211 PKT,
driver TX related function need to
reference __rx__ of rtw_phl_stainfo_t->asoc_cap
*/
struct protocol_cap_t {
	/* MAC related */
	u16 bcn_interval;	/* beacon interval */
	u8 num_ampdu;
	u8 ampdu_density:3;	/* rx ampdu cap */
	u8 ampdu_len_exp; /* rx ampdu cap */
	u8 amsdu_in_ampdu:1; /* rx ampdu cap */
	u8 max_amsdu_len:2; /* 0: 4k, 1: 8k, 2: 11k */
	u8 htc_rx:1;
	u8 sm_ps:2;
	u8 trig_padding:2;
	u8 twt:6;
	u8 all_ack:1;
	u8 a_ctrl:4;
	u8 ops:1;
	u8 ht_vht_trig_rx:1;
	u8 bsscolor;
	u16 rts_th:10;

	u8 short_slot:1;	/* Short Slot Time */

	u8 preamble:1;		/* Preamble, 0: long, 1: short */
	u8 sgi_20:1;		/* HT Short GI for 20 MHz */
	u8 sgi_40:1;		/* HT Short GI for 40 MHz */
	u8 sgi_80:1;		/* VHT Short GI for 80 MHz */
	u8 sgi_160:1;		/* VHT Short GI for 160/80+80 MHz */
	struct rtw_edca_param edca[4]; 	/* Access Category, 0:BE, 1:BK, 2:VI, 3:VO */
	u8 mu_qos_info;
	struct rtw_mu_edca_param mu_edca[4];

	/* BB related */
	u8 ht_ldpc:1;
	u8 vht_ldpc:1;
	u8 he_ldpc:1;
	u8 he_su_bfmr:1;
	u8 he_su_bfme:1;
	u8 he_mu_bfmr:1;
	u8 he_mu_bfme:1;
	u8 bfme_sts:3;
	u8 num_snd_dim:3;

	u8 ht_su_bfmr:1;
	u8 ht_su_bfme:1;
	u8 vht_su_bfmr:1;
	u8 vht_su_bfme:1;
	u8 vht_mu_bfmr:1;
	u8 vht_mu_bfme:1;
	u8 ht_vht_ng:2;
	u8 ht_vht_cb:2;
	/*
	 * supported_rates: Supported data rate of CCK/OFDM.
	 * The rate definition follow Wi-Fi spec, unit is 500kb/s,
	 * and the MSB(bit 7) represent basic rate.
	 * ex. CCK 2Mbps not basic rate is encoded as 0x04,
	 *     and OFDM 6M basic rate is encoded as 0x8c.
	 * Suppose rates come from Supported Rates and Extended Supported
	 * Rates IE.
	 * Value 0 means it is end of array, and no more valid data rate follow.
	 */
	u8 supported_rates[12];
	u8 ht_rx_mcs[4];
	u8 ht_tx_mcs[4];
	u8 ht_basic_mcs[4];	/* Basic rate of HT */
	u8 vht_rx_mcs[2];
	u8 vht_tx_mcs[2];
	u8 vht_basic_mcs[2];	/* Basic rate of VHT */
	u8 he_rx_mcs[6];/*80,160,80+80*/
	u8 he_tx_mcs[6];/*80,160,80+80*/
	u8 he_basic_mcs[2];	/* Basic rate of HE */
	u8 stbc_ht_rx:2;
	u8 stbc_vht_rx:3;
	u8 stbc_he_rx:1;
	u8 stbc_tx:1;
	u8 stbc_ht_tx:1;
	u8 stbc_vht_tx:1;
	u8 stbc_he_tx:1;
	u8 ltf_gi;
	u8 doppler_tx:1;
	u8 doppler_rx:1;
	u8 dcm_max_const_tx:2;
	u8 dcm_max_nss_tx:1;
	u8 dcm_max_const_rx:2;
	u8 dcm_max_nss_rx:1;
	u8 partial_bw_su_in_mu:1;
	u8 bfme_sts_greater_80mhz:3;
	u8 num_snd_dim_greater_80mhz:3;
	u8 stbc_tx_greater_80mhz:1;
	u8 stbc_rx_greater_80mhz:1;
	u8 ng_16_su_fb:1;
	u8 ng_16_mu_fb:1;
	u8 cb_sz_su_fb:1;
	u8 cb_sz_mu_fb:1;
	u8 trig_su_bfm_fb:1;
	u8 trig_mu_bfm_fb:1;
	u8 trig_cqi_fb:1;
	u8 partial_bw_su_er:1;
	u8 pkt_padding:2;
	u8 ppe_thr[8][4];
	u8 pwr_bst_factor:1;
	u8 max_nc:3;
	u8 dcm_max_ru:2;
	u8 long_sigb_symbol:1;
	u8 non_trig_cqi_fb:1;
	u8 tx_1024q_ru:1;
	u8 rx_1024q_ru:1;
	u8 fbw_su_using_mu_cmprs_sigb:1;
	u8 fbw_su_using_mu_non_cmprs_sigb:1;
	u8 er_su:1;
	u8 tb_pe:3;
	u16 txop_du_rts_th;
	u8 he_rx_ndp_4x32:1;

	/* RF related */
	u8 nss_tx:3;
	u8 nss_rx:3;

	u8 num_ampdu_bk;
	u8 cap_option;
};

#define EXT_CAP_LIMIT_2G_RX_STBC	BIT0


#define LOAD_MAC_REG_FILE				BIT0
#define LOAD_BB_PHY_REG_FILE			BIT1
#define LOAD_BB_PHY_REG_MP_FILE			BIT2
#define LOAD_RF_RADIO_FILE				BIT3
#define LOAD_RF_TXPWR_BY_RATE			BIT4
#define LOAD_RF_TXPWR_TRACK_FILE		BIT5
#define LOAD_RF_TXPWR_LMT_FILE			BIT6
#define LOAD_RF_TXPWR_LMT_RU_FILE		BIT7
#define LOAD_BB_PHY_REG_GAIN_FILE		BIT8

#define PHL_UNDEFINED_SW_CAP 0xFF

struct rtw_pcie_ltr_lat_ctrl {
	enum rtw_pcie_bus_func_cap_t ctrl;
	u32 val;
};

enum rtw_pcie_ltr_state {
	RTW_PCIE_LTR_SW_ACT = 1,
	RTW_PCIE_LTR_SW_IDLE = 2
};

struct rtw_pcie_trx_mit_info_t {
	u32 tx_timer;
	u8 tx_counter;
	u32 rx_timer;
	u8 rx_counter;
	u8 fixed_mitigation; /*no watchdog dynamic setting*/
	void *priv;
};

struct bus_sw_cap_t {
#ifdef CONFIG_PCI_HCI
	enum rtw_pcie_bus_func_cap_t l0s_ctrl;
	enum rtw_pcie_bus_func_cap_t l1_ctrl;
	enum rtw_pcie_bus_func_cap_t l1ss_ctrl;
	enum rtw_pcie_bus_func_cap_t wake_ctrl;
	enum rtw_pcie_bus_func_cap_t crq_ctrl;
	u32 txbd_num;
	u32 rxbd_num;
	u32 rpbd_num;
	u32 rxbuf_num;
	u32 rpbuf_num;
	u32 read_txbd_lvl; /* 0: always read, 1: < 1/2 tx res, 2: < 1/4 tx res */
	struct rtw_pcie_trx_mit_info_t mit_ctl;
	u8 clkdly_ctrl;
	u8 l0sdly_ctrl;
	u8 l1dly_ctrl;
	struct rtw_pcie_ltr_lat_ctrl ltr_act;
	struct rtw_pcie_ltr_lat_ctrl ltr_idle;
	u8 ltr_init_state;
	u16 ltr_sw_ctrl_thre; /* [15:8] tx [7:0] rx */
	u8 ltr_sw_ctrl;
	u8 ltr_hw_ctrl;
	u32 ltr_last_trigger_time;
	u32 ltr_sw_act_tri_cnt;
	u32 ltr_sw_idle_tri_cnt;
	u8 ltr_cur_state;
	#ifdef RTW_WKARD_GET_PROCESSOR_ID
	u64 proc_id; /* processor id */
	#endif
#elif defined (CONFIG_USB_HCI)
	u32 tx_buf_size;
	u32 tx_buf_num;
	u32 tx_mgnt_buf_size;
	u32 tx_mgnt_buf_num;
	u32 tx_h2c_buf_num;
	u32 rx_buf_size;
	u32 rx_buf_num;
	u32 in_token_num;
#elif defined(CONFIG_SDIO_HCI)
	u32 tx_buf_size;
	u32 tx_buf_num;
	u32 tx_mgnt_buf_size;
	u32 tx_mgnt_buf_num;
	u32 rx_buf_size;
	u32 rx_buf_num;
#else
	u8 temp_for_struct_empty; /* for undefined interface */
#endif
};
struct bus_cap_t {
#ifdef CONFIG_PCI_HCI
	enum rtw_pcie_bus_func_cap_t l0s_ctrl;
	enum rtw_pcie_bus_func_cap_t l1_ctrl;
	enum rtw_pcie_bus_func_cap_t l1ss_ctrl;
	enum rtw_pcie_bus_func_cap_t wake_ctrl;
	enum rtw_pcie_bus_func_cap_t crq_ctrl;
	u32 txbd_num;
	u32 rxbd_num;
	u32 rpbd_num;
	u32 rxbuf_num;
	u32 rpbuf_num;
	u32 read_txbd_th;
	u8 clkdly_ctrl;
	u8 l0sdly_ctrl;
	u8 l1dly_ctrl;
	struct rtw_pcie_ltr_lat_ctrl ltr_act;
	struct rtw_pcie_ltr_lat_ctrl ltr_idle;
	u8 ltr_init_state;
	u8 ltr_sw_ctrl;
	u8 ltr_hw_ctrl;
	#ifdef RTW_WKARD_GET_PROCESSOR_ID
	u64 proc_id; /* processor id */
	#endif
#elif defined (CONFIG_USB_HCI)
	u32 tx_buf_size;
	u32 tx_buf_num;
	u32 tx_mgnt_buf_size;
	u32 tx_mgnt_buf_num;
	u32 tx_h2c_buf_num;
	u32 rx_buf_size;
	u32 rx_buf_num;
	u32 in_token_num;
#elif defined(CONFIG_SDIO_HCI)
	u32 tx_buf_size;
	u32 tx_buf_num;
	u32 tx_mgnt_buf_size;
	u32 tx_mgnt_buf_num;
	u32 rx_buf_size;
	u32 rx_buf_num;
#else
	u8 temp_for_struct_empty; /* for undefined interface */
#endif
};

#ifdef CONFIG_PHL_TWT

#define DELETE_ALL 0xFF
#define IGNORE_CFG_ID 0xFF
#define IGNORE_MACID 0xFF

enum rtw_phl_twt_sup_cap {
	RTW_PHL_TWT_REQ_SUP = BIT(0), /* REQUESTER */
	RTW_PHL_TWT_RSP_SUP = BIT(1)/* RESPONDER */
};

enum rtw_phl_nego_type {
	RTW_PHL_INDIV_TWT = 0, /*individual TWT*/
	RTW_PHL_WAKE_TBTT_INR = 1, /*wake TBTT and wake interval*/
	RTW_PHL_BCAST_TWT = 2, /*Broadcast TWT*/
	RTW_PHL_MANAGE_BCAST_TWT = 3 /*Manage memberships in broadcast TWT schedules*/
};

enum rtw_phl_wake_dur_unit{ /*wake duration unit*/
	RTW_PHL_WAKE_256US = 0,
	RTW_PHL_WAKE_1TU = 1
};

enum rtw_phl_setup_cmd{
	RTW_PHL_REQUEST_TWT = 0,
	RTW_PHL_SUGGEST_TWT = 1,
	RTW_PHL_DEMAND_TWT = 2,
	RTW_PHL_TWT_GROUPING = 3,
	RTW_PHL_ACCEPT_TWT = 4,
	RTW_PHL_ALTERNATE_TWT = 5,
	RTW_PHL_DICTATE_TWT = 6,
	RTW_PHL_REJECT_TWT = 7
};

enum rtw_phl_flow_type{
	RTW_PHL_ANNOUNCED_TWT = 0,
	RTW_PHL_UNANNOUNCED_TWT = 1
};

enum rtw_phl_twt_sta_action {
	TWT_STA_NONE = 0,
	TWT_STA_ADD_MACID = 1,
	TWT_STA_DEL_MACID = 2,
	TWT_STA_TETMINATW_SP = 3,
	TWT_STA_SUSPEND_TWT = 4,
	TWT_STA_RESUME_TWT = 5
};

enum rtw_phl_twt_cfg_action {
	TWT_CFG_ADD = 0,
	TWT_CFG_DELETE = 1,
	TWT_CFG_MODIFY = 2
};

struct rtw_phl_twt_flow_type01 {
	u8 twt_flow_id;
	u8 teardown_all;
};

struct rtw_phl_twt_flow_type2 {
	u8 reserved;
};

struct rtw_phl_twt_flow_type3 {
	u8 bcast_twt_id;
	u8 teardown_all;
};

struct rtw_phl_twt_flow_field{
	enum rtw_phl_nego_type nego_type;
	union {
		struct rtw_phl_twt_flow_type01 twt_flow01;
		struct rtw_phl_twt_flow_type2 twt_flow2;
		struct rtw_phl_twt_flow_type3 twt_flow3;
	} info;
};

/*phl_twt_setup_info Start*/

/*Broadcast TWT Parameter Set field*/
struct rtw_phl_bcast_twt_para_set{
	u8 reserved; /*todo*/
};

/*Individual TWT Parameter Set field*/
struct rtw_phl_twt_group_asgmt{
	u8 reserved; /*todo*/
};

struct rtw_phl_req_type_indiv{
	enum rtw_phl_setup_cmd twt_setup_cmd; /*twt setup command*/
	enum rtw_phl_flow_type flow_type;
	u8 twt_request;
	u8 trigger;
	u8 implicit;
	u8 twt_flow_id;
	u8 twt_wake_int_exp;/*twt wake interval exponent*/
	u8 twt_protection;
};

struct rtw_phl_indiv_twt_para_set{
	struct rtw_phl_req_type_indiv req_type;
	struct rtw_phl_twt_group_asgmt twt_group_asgmt; /* twt group assignment*/
	u32 target_wake_t_h; /* if contain twt_group_assignment then don't contain target_wake_time*/
	u32 target_wake_t_l;
	u16 twt_wake_int_mantissa; /*twt wake interval mantissa*/
	u8 nom_min_twt_wake_dur; /*nominal minimum twt wake duration*/
	u8 twt_channel;
};

struct rtw_phl_twt_control{
	enum rtw_phl_nego_type nego_type; /*negotiation type*/
	enum rtw_phl_wake_dur_unit wake_dur_unit; /*wake duration unit*/
	u8 ndp_paging_indic; /*ndp paging indicator*/
	u8 responder_pm_mode;
	u8 twt_info_frame_disable; /*twt information frame disable*/
};
struct rtw_phl_twt_element{
/* element info*/
	/*control filed*/
	struct rtw_phl_twt_control twt_ctrl;
	/*twt para info*/
	union {
		struct rtw_phl_indiv_twt_para_set i_twt_para_set;
		struct rtw_phl_bcast_twt_para_set b_twt_para_set;
	} info;
};

struct rtw_phl_twt_setup_info{
	struct rtw_phl_twt_element twt_element;
	//struct rtw_phl_stainfo_t *phl_sta; //sta entry
	u8 dialog_token;
};
/*phl_twt_setup_info End*/


/*phl_twt_info Start*/
struct rtw_twt_sta_info{
	_os_list list;
	struct rtw_phl_stainfo_t *phl_sta; /*sta entry*/
	u8 id; /*twt_flow_identifier or broadcast_twt_id*/
};

struct rtw_phl_twt_info{
	enum rtw_phl_wake_dur_unit wake_dur_unit;
	enum rtw_phl_nego_type nego_type;
	enum rtw_phl_flow_type flow_type;
	u8 twt_id; /*config id*/
	u8 bcast_twt_id; /*ignore in individual TWT*/
	u8 twt_action;
	u8 responder_pm_mode;
	u8 trigger;
	u8 implicit_lastbcast; /*implicit or lastbroadcast*/
	u8 twt_protection;
	u8 twt_wake_int_exp;
	u8 nom_min_twt_wake_dur;
	u16 twt_wake_int_mantissa;
	u32 target_wake_time_h;
	u32 target_wake_time_l;
};


#endif /* CONFIG_PHL_TWT */


enum rtw_lps_listen_bcn_mode {
	RTW_LPS_RLBM_MIN         = 0,
	RTW_LPS_RLBM_MAX         = 1,
	RTW_LPS_RLBM_USERDEFINE  = 2,
	RTW_LPS_LISTEN_BCN_MAX,
};

enum rtw_lps_smart_ps_mode {
	RTW_LPS_LEGACY_PWR1      = 0,
	RTW_LPS_TRX_PWR0         = 1,
	RTW_LPS_SMART_PS_MAX,
};

struct  rtw_wow_cap_t {
	u8 magic_sup;
	u8 pattern_sup;
	u8 ping_pattern_wake_sup;
	u8 arp_ofld_sup;
	u8 ns_oflod_sup;
	u8 gtk_ofld_sup;
	u8 nlo_sup;
};

/**
 * enum phl_ps_leave_fail_act decide the action when leave ps fail
 * BIT 0 : reject all subsequent power request
 * BIT 1 : trigger L2 reset
 */
enum phl_ps_leave_fail_act {
	PS_LEAVE_FAIL_ACT_REJ_PWR = BIT0,
	PS_LEAVE_FAIL_ACT_L2 = BIT1
};
#define PS_LEAVE_FAIL_ACT_NONE 0

enum phl_ps_operation_mode {
	PS_OP_MODE_DISABLED = 0,
	PS_OP_MODE_FORCE_ENABLED = 1,
	PS_OP_MODE_AUTO = 2
};

enum phl_ps_pwr_lvl {
	PS_PWR_LVL_PWROFF = 0, /* hal deinit */
	PS_PWR_LVL_PWR_GATED = 1, /* FW control*/
	PS_PWR_LVL_CLK_GATED = 2, /* FW control*/
	PS_PWR_LVL_RF_OFF = 3, /* FW control*/
	PS_PWR_LVL_PWRON = 4, /* hal init */
	PS_PWR_LVL_MAX
};

/**
 * enum phl_stop_rson record the reason to stop power saving
 * BIT 0 : by core initialization setting
 * BIT 1 : by debug flow setting
 * BIT 2 : by battery change
 */
enum phl_ps_rt_rson {
	PS_RT_DEBUG = BIT0,
	PS_RT_CORE_INIT = BIT1,
	PS_RT_BATTERY_CHG = BIT2,
};
#define PS_RT_RSON_NONE 0

#define PS_CAP_PWRON BIT0
#define PS_CAP_RF_OFF BIT1
#define PS_CAP_CLK_GATED BIT2
#define PS_CAP_PWR_GATED BIT3
#define PS_CAP_PWR_OFF BIT4

/**
 * ips_en/lps_en
 * refs. structure "phl_ps_operation_mode"
 * 0: disable -> disable all ps mechanism
 * 1: force enable -> ignore all other condition, force enter ps
 * 2: auto -> will be affected by runtime capability set by core
 *
 * ips_cap/ips_wow_cap/lps_cap/lps_wow_cap are bit defined
 * corresponding bit is set if specific power level is supported
 * BIT0: Power on
 * BIT1: Rf off
 * BIT2: Clock gating
 * BIT3: Power gating
 * BIT4: Power off
 */
struct rtw_ps_cap_t {
	/* rf state */
	enum rtw_rf_state init_rf_state;
	u8 init_rt_stop_rson;
	u8 leave_fail_act; /* action when leave ps fail */
	/* ips */
	u8 ips_en;
	u8 ips_cap;
	u8 ips_wow_en;
	u8 ips_wow_cap;
	/* lps */
	u8 lps_en;
	u8 lps_cap;
	u8 lps_awake_interval;
	enum rtw_lps_listen_bcn_mode lps_listen_bcn_mode;
	enum rtw_lps_smart_ps_mode lps_smart_ps_mode;
	u8 lps_rssi_enter_threshold;
	u8 lps_rssi_leave_threshold;
	u8 lps_rssi_diff_threshold;
	bool lps_pause_tx;
	/* wow lps */
	u8 lps_wow_en;
	u8 lps_wow_cap;
	u8 lps_wow_awake_interval;
	enum rtw_lps_listen_bcn_mode lps_wow_listen_bcn_mode;
	enum rtw_lps_smart_ps_mode lps_wow_smart_ps_mode;
};

struct rtw_edcca_cap_t {
	u8 edcca_adap_th_2g;
	u8 edcca_adap_th_5g;

	u8 edcca_carrier_sense_th;
};

struct phy_sw_cap_t {
	struct rtw_para_info_t mac_reg_info;
	struct rtw_para_info_t bb_phy_reg_info;
	struct rtw_para_info_t bb_phy_reg_mp_info;
	struct rtw_para_info_t bb_phy_reg_gain_info;

	struct rtw_para_info_t rf_radio_a_info;
	struct rtw_para_info_t rf_radio_b_info;
	struct rtw_para_info_t rf_txpwr_byrate_info;
	struct rtw_para_info_t rf_txpwrtrack_info;

	struct rtw_para_pwrlmt_info_t rf_txpwrlmt_info;
	struct rtw_para_pwrlmt_info_t rf_txpwrlmt_ru_info;

	u8 proto_sup;
	u8 band_sup;
	u8 bw_sup;
	u8 txss;
	u8 rxss;
	u16 hw_rts_time_th;
	u16 hw_rts_len_th;
	bool bfreed_para;
};

/* final capability of phy */
struct phy_cap_t {
	u8 proto_sup;
	u8 band_sup;
	u8 bw_sup;
	u8 txss;
	u8 rxss;
	u16 hw_rts_time_th;
	u16 hw_rts_len_th;
};

/* final capability of device */
struct dev_cap_t {
	u64 hw_sup_flags;/*hw's feature support flags*/
#ifdef RTW_WKARD_LAMODE
	bool la_mode;
#endif
	u8 pkg_type;
	u8 rfe_type;
	u8 bypass_rfe_chk;
	u8 xcap;
	struct rtw_fw_cap_t fw_cap;
#ifdef CONFIG_MCC_SUPPORT
	bool mcc_sup;
#endif
#ifdef CONFIG_DBCC_SUPPORT
	bool dbcc_sup;
#endif
#ifdef CONFIG_PHL_TWT
	u8 twt_sup;
#endif /* CONFIG_PHL_TWT */

	struct rtw_wow_cap_t wow_cap;
	struct rtw_ps_cap_t ps_cap;
	u8 hw_hdr_conv;
	u8 domain;
	u8 btc_mode;
	u8 ap_ps;           /* support for AP mode PS in PHL */
	u8 pwrbyrate_off;
	u8 pwrlmt_type;
	u8 rf_board_opt;
	u8 sta_ulru; /* support UL OFDAM for STA mode (reply trigger frame) */
#ifdef RTW_WKARD_BB_DISABLE_STA_2G40M_ULOFDMA
	u8 sta_ulru_2g40mhz; /* when "sta_ulru" is enabled, support UL OFDAM on 2.4G 40MHz ? */
#endif
	u8 tx_mu_ru;
	struct rtw_edcca_cap_t edcca_cap;
#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	bool bfree_para_info;	/* keep load file para info buf,default 0*/
#endif
	u8 hw_stype_cap;
	u8 wl_func_cap;
	u8 rpq_agg_num; /* 0: no adjust, use mac default size: 121 */
	bool quota_turbo;
};

#ifdef RTW_PHL_BCN //phl def

#define BCN_ID_MAX		(0xFF)
#define MAX_BCN_SIZE	1000

enum bcn_offload_flags{
	BCN_HW_SEQ = 0,
	BCN_HW_TIM,

	BCN_HW_MAX = 32,
};

struct rtw_bcn_info_cmn {
	u8 role_idx;
	u8 bcn_id;
	u8 bcn_added;

	u8 bssid[6];
	u32 bcn_interval;

	u8 bcn_buf[MAX_BCN_SIZE];
	u32 bcn_length;
	u32 bcn_rate;

	u32 bcn_dtim;
	u32 ie_offset_tim;

	u32 bcn_offload;
};

struct rtw_bcn_info_hw {
	u8 band;
	u8 port;
	u8 mbssid;
	u8 mac_id;
};

struct rtw_bcn_entry {
	_os_list list;
	struct rtw_bcn_info_cmn	*bcn_cmn;	//fill by core
	struct rtw_bcn_info_hw 	bcn_hw;	//fill by phl //?? void mapping ?? for 8852, 8834 ...blabla
};
#endif

struct rtw_phl_com_t;

struct phl_msg_receiver {
		void* priv;
		void (*incoming_evt_notify)(void* priv, struct phl_msg *msg);
};

#ifdef CONFIG_PHL_P2PPS

#define MAX_NOA_DESC 5
#define NOAID_NONE 0xFF

enum p2pps_trig_tag {
	P2PPS_TRIG_GO = 0,
	P2PPS_TRIG_GC = 1,
	P2PPS_TRIG_GC_255 = 2,
	P2PPS_TRIG_MCC = 3,
	P2PPS_TRIG_2G_SCC_1AP_1STA_BT = 4,
	P2PPS_TRIG_MAX = MAX_NOA_DESC
};

struct rtw_phl_noa_desc {
	u8 enable; /*false=disable, true=enable*/
	struct rtw_wifi_role_t *w_role;
	enum p2pps_trig_tag tag;
	u32 start_t_h;
	u32 start_t_l;
	u32 interval;
	u32 duration;
	u8 count;
	u8 noa_id; /*filed by phl noa module*/
};

struct rtw_phl_opps_desc {
	u16 ctw;
	u8 all_slep;
};

struct rtw_phl_tsf32_tog_rpt{
	u8 band;
	u8 port;
	u8 valid;
	u16 early;
	u16 status;
	u32 tsf_l;
	u32 tsf_h;
};

struct rtw_phl_p2pps_ops {
	void *priv; /* ops private, define by core layer*/
	void (*tsf32_tog_update_noa)(void *priv, struct rtw_wifi_role_t *w_role,
					struct rtw_phl_tsf32_tog_rpt *rpt);
	void (*tsf32_tog_update_single_noa)(void *priv,
					struct rtw_wifi_role_t *w_role,
					struct rtw_phl_noa_desc *desc);
};


#endif

struct rtw_wifi_role_t {
	struct rtw_phl_com_t *phl_com;/*point to phl_com*/
	#ifdef RTW_WKARD_ROLE_TYPE
	enum role_type real_type;
	#endif /* RTW_WKARD_ROLE_TYPE */
	enum role_type type;/*will mapping to net type*/
	enum role_type target_type;
	#ifdef RTW_WKARD_PHL_NTFY_MEDIA_STS
	bool is_gc;
	#endif
	enum mlme_state mstate;
	bool active;
	enum wr_status status;
	u8 id;/* recode role_idx in phl_com */
	u8 hw_wmm; /*HW EDCA - wmm0 or wmm1*/
	#ifdef RTW_WKARD_HW_WMM_ALLOCATE
	_os_atomic hw_wmm0_ref_cnt;
	#endif
	u8 mac_addr[MAC_ALEN];
	u8 hw_band; /*MAC Band0 or Band1*/
	u8 hw_port; /*MAC HW Port*/
	/*
	 * final protocol capability of role from intersection of
	 * sw role cap, sw protocol cap and hw protocol cap
	 */
	struct protocol_cap_t proto_role_cap;

	/*
	 * final capability of role from intersection of
	 * sw role cap, final phy cap and final dev cap
	 */
	struct role_cap_t cap;

	/*#ifdef CONFIG_AP*/
#ifdef RTW_PHL_BCN
	struct rtw_bcn_info_cmn bcn_cmn; //todo: ieee mbssid case & multi-bcn (in one iface) case
	u8 hw_mbssid;
#endif
	u8 dtim_period;
	u8 mbid_num;
	u32 hiq_win;
	/*#endif CONFIG_AP*/

	struct rtw_chan_def chandef;
	struct rtw_chan_ctx *chanctx;/*point to chanctx*/

	struct phl_queue assoc_sta_queue;

#ifdef CONFIG_PHL_TWT
	struct rtw_phl_twt_setup_info twt_setup_info;
#endif /* CONFIG_PHL_TWT */

#ifdef CONFIG_PHL_P2PPS
	struct rtw_phl_noa_desc noa_desc[MAX_NOA_DESC];
#endif

	void *core_data; /* Track back to counter part in core layer */
#ifdef RTW_WKARD_BFEE_SET_AID
	u16 last_set_aid;
#endif
};

#define TXTP_CALC_DIFF_MS 1000
#define RXTP_CALC_DIFF_MS 1000

#define	TX_ULTRA_LOW_TP_THRES_KBPS 100
#define	RX_ULTRA_LOW_TP_THRES_KBPS 100
#define	TX_LOW_TP_THRES_MBPS 2
#define	RX_LOW_TP_THRES_MBPS 2
#define	TX_MID_TP_THRES_MBPS  10
#define	RX_MID_TP_THRES_MBPS  10
#define	TX_HIGH_TP_THRES_MBPS  50
#define	RX_HIGH_TP_THRES_MBPS  50


enum rtw_tfc_lvl {
	RTW_TFC_IDLE = 0,
	RTW_TFC_ULTRA_LOW = 1,
	RTW_TFC_LOW = 2,
	RTW_TFC_MID = 3,
	RTW_TFC_HIGH = 4,
	RTW_TFC_LVL_MAX = 0xFF
};

enum rtw_tfc_sts {
	TRAFFIC_CHANGED = BIT0,
	TRAFFIC_INCREASE = BIT1,
	TRAFFIC_DECREASE = BIT2,
	TRAFFIC_STS_MAX = BIT7
};

struct rtw_traffic_t {
	enum rtw_tfc_lvl lvl;
	enum rtw_tfc_sts sts;
};

struct rtw_stats_tp {
	u64 last_calc_bits;
	u32 last_calc_time_ms;
};
/*statistic*/
struct rtw_stats {
	u64 tx_byte_uni;/*unicast tx byte*/
	u64 rx_byte_uni;/*unicast rx byte*/
	u64 tx_byte_total;
	u64 rx_byte_total;
	u32 tx_tp_kbits;
	u32 rx_tp_kbits;
	u16 tx_moving_average_tp; /* tx average MBps*/
	u16 rx_moving_average_tp; /* rx average MBps*/
	u32 last_tx_time_ms;
	u32 last_rx_time_ms;
	u32 txreq_num;
	u32 rx_rate;
	u32 rx_rate_nmr[RTW_DATA_RATE_HE_NSS4_MCS11 +1];
	u64 ser_event[8]; /* RTW_PHL_SER_MAX */
	struct rtw_stats_tp txtp;
	struct rtw_stats_tp rxtp;
	struct rtw_traffic_t tx_traffic;
	struct rtw_traffic_t rx_traffic;
	u32 rx_tf_cnt; /* rx trigger frame number (accumulated, only reset in disconnect) */
	u32 pre_rx_tf_cnt; /* last record rx trigger frame number from BB */
};
enum sta_chg_id {
	STA_CHG_BW,
	STA_CHG_NSS,
	STA_CHG_RAMASK,
	STA_CHG_SEC_MODE,
	STA_CHG_MBSSID,
	STA_CHG_RA_GILTF,
	STA_CHG_MAX
};

enum phl_upd_mode {
	PHL_UPD_ROLE_CREATE,
	PHL_UPD_ROLE_REMOVE,
	PHL_UPD_ROLE_TYPE_CHANGE,
	PHL_UPD_ROLE_INFO_CHANGE,
	PHL_UPD_STA_INFO_CHANGE,
	PHL_UPD_STA_CON_DISCONN,
	PHL_UPD_ROLE_MAX
};

#ifdef CONFIG_PHL_TXSC
#define PHL_TXSC_ENTRY_NUM 8
#define MAX_WD_SIZE	128

struct phl_txsc_entry {
	bool txsc_wd_cached;
	u8 txsc_wd_cache[MAX_WD_SIZE];
	u8 txsc_wd_len;
	u32 txsc_cache_hit;
};
#endif

struct rtw_hal_stainfo_t;
struct rtw_phl_stainfo_t {
	_os_list list;
	struct rtw_wifi_role_t *wrole;
#ifdef RTW_WKARD_CHECK_STAINFO_DOUBLE_DEL
	bool allocated;
#endif
	bool active;
	u16 aid;
	u16 macid;
	u8 mac_addr[MAC_ALEN];

	struct rtw_chan_def chandef;
	struct rtw_stats stats;
	enum wlan_mode wmode;

	/*mlme protocol or MAC related CAP*/
	u8 bcn_hit_cond;
	u8 hit_rule;
	u8 tf_trs;
	u8 tgt_ind;
	u8 frm_tgt_ind;
	u8 addr_sel;
	u8 addr_msk;

	/* rx agg */
	struct phl_tid_ampdu_rx *tid_rx[RTW_MAX_TID_NUM]; /* TID_MAX_NUM */
	_os_lock tid_rx_lock;               /* guarding @tid_rx */
	_os_event comp_sync;     /* reorder timer completion event */
	_os_timer reorder_timer; /* reorder timer for all @tid_rx of the
	                          * stainfo */
	/* TODO: add missing part */

	/*mlme protocol or PHY related CAP*/
	struct protocol_cap_t asoc_cap;
	enum rtw_protect_mode protect;

	/*security related*/
	u8 wapi;
	u8 sec_mode;

	/*
	 * STA powersave, those could be implemented as bit flags but there's no
	 * corresponding atomic bit operations available on Windows.
	 */
	_os_atomic ps_sta;      /* the sta is in PS mode or not */

	struct rtw_hal_stainfo_t *hal_sta;

#ifdef CONFIG_PHL_TXSC
	struct phl_txsc_entry phl_txsc[PHL_TXSC_ENTRY_NUM];
#endif
	struct rtw_rx_bcn_info bcn_i;
	void *core_data; /* Track back to counter part in core layer */
};




#define WL_FUNC_P2P		BIT0
#define WL_FUNC_MIRACAST	BIT1
#define WL_FUNC_TDLS		BIT2
#define WL_FUNC_FTM		BIT3
#define WL_FUNC_BIT_NUM	4


/* HW MAC capability*/
#define HW_SUP_DBCC			BIT0
#define HW_SUP_AMSDU			BIT1
#define HW_SUP_TCP_TX_CHKSUM		BIT2
#define HW_SUP_TCP_RX_CHKSUM		BIT3
#define HW_SUP_TXPKT_CONVR		BIT4
#define HW_SUP_RXPKT_CONVR		BIT5
#define HW_SUP_MULTI_BSSID		BIT6
#define HW_SUP_OFDMA			BIT7
#define HW_SUP_CHAN_INFO		BIT8
#define HW_SUP_TSSI			BIT9
#define HW_SUP_TANK_K			BIT10

/*BUS Section CAP */
#define HW_SUP_PCIE_PLFH		BIT20	/*payload from host*/
#define HW_SUP_USB_MULTI_FUN		BIT21
#define HW_SUP_SDIO_MULTI_FUN		BIT22

/* Beamform CAP */
#define HW_CAP_BF_NON_SUPPORT 0
#define HW_CAP_BFEE_HT_SU BIT(0)
#define HW_CAP_BFER_HT_SU BIT(1)
#define HW_CAP_BFEE_VHT_SU BIT(2)
#define HW_CAP_BFER_VHT_SU BIT(3)
#define HW_CAP_BFEE_VHT_MU BIT(4)
#define HW_CAP_BFER_VHT_MU BIT(5)
#define HW_CAP_BFEE_HE_SU BIT(6)
#define HW_CAP_BFER_HE_SU BIT(7)
#define HW_CAP_BFEE_HE_MU BIT(8)
#define HW_CAP_BFER_HE_MU BIT(9)
#define HW_CAP_HE_NON_TB_CQI BIT(10)
#define HW_CAP_HE_TB_CQI BIT(11)

#define RTW_HW_CAP_ULRU_AUTO 0
#define RTW_HW_CAP_ULRU_DISABLE 1
#define RTW_HW_CAP_ULRU_ENABLE 2

/* STBC CAP */
#define HW_CAP_STBC_HT_TX BIT(0)
#define HW_CAP_STBC_VHT_TX BIT(1)
#define HW_CAP_STBC_HE_TX BIT(2)
#define HW_CAP_STBC_HE_TX_GT_80M BIT(3)
#define HW_CAP_STBC_HT_RX BIT(4)
#define HW_CAP_STBC_VHT_RX BIT(5)
#define HW_CAP_STBC_HE_RX BIT(6)
#define HW_CAP_STBC_HE_RX_GT_80M BIT(7)

struct hal_spec_t {
	char *ic_name;
	u16 macid_num;

	u8 sec_cam_ent_num;
	u8 sec_cap;
	u8 wow_cap;

	u8 rfpath_num_2g:4;	/* used for tx power index path */
	u8 rfpath_num_5g:4;	/* used for tx power index path */
	u8 rf_reg_path_num;
	u8 max_tx_cnt;

	u8 band_cap;	/* value of BAND_CAP_XXX */
	u8 bw_cap;	/* value of BW_CAP_XXX */
	u8 port_num;
	u8 wmm_num;
	u8 proto_cap;	/* value of PROTO_CAP_XXX */
	u8 wl_func;	/* value of WL_FUNC_XXX */

	/********* xmit ************/


	/********* recv ************/
	u8 rx_bd_info_sz;

	u16 rx_tag[2];
	#ifdef CONFIG_USB_HCI
	u8 max_bulkin_num;
	u8 max_bulkout_num;
	#endif
	#ifdef CONFIG_PCI_HCI
	u16 txbd_multi_tag;
	u8 txbd_upd_lmt;
	#ifdef RTW_WKARD_BUSCAP_IN_HALSPEC
	u8 phyaddr_num;
	#endif
	#endif
	u8 cts2_thres_en;
	u16 cts2_thres;
	/********* beamformer ************/
	u8 max_csi_buf_su_nr;
	u8 max_csi_buf_mu_nr;
	u8 max_bf_ent_nr;
	u8 max_su_sta_nr;
	u8 max_mu_sta_nr;

};

#define phl_get_hci_type(_phlcom) (_phlcom->hci_type)
#define phl_get_ic_spec(_phlcom) (&_phlcom->hal_spec)
#define phl_get_fw_buf(_phlcom) (_phlcom->fw_info.ram_buff)
#define phl_get_fw_size(_phlcom) (_phlcom->fw_info.ram_size)

enum rtw_drv_mode {
	RTW_DRV_MODE_NORMAL = 0,
	RTW_DRV_MODE_EQC = 1,
	RTW_DRV_MODE_HIGH_THERMAL = 2,

	/* 11~20 for MP submodule section*/
	RTW_DRV_MODE_MP_SMDL_START = 11,
	RTW_DRV_MODE_MP = 11,
	RTW_DRV_MODE_HOMOLOGATION = 12,
	RTW_DRV_MODE_MP_SMDL_END = 20,

	/* 21~30 for FPGA submodule section*/
	RTW_DRV_MODE_FPGA_SMDL_START = 21,
	RTW_DRV_MODE_FPGA_SMDL_END = 30,

	/* 31~60 for VERIFY submodule section*/
	RTW_DRV_MODE_VERIFY_SMDL_START = 31,
	RTW_DRV_MODE_VERIFY_SMDL_END = 60,

	/* 61~80 for TOOL submodule section*/
	RTW_DRV_MODE_TOOL_SMDL_START = 61,
	RTW_DRV_MODE_TOOL_SMDL_END = 80,

	/* Fixed Max Value*/
	RTW_DRV_MODE_MAX = 255
};

struct rtw_evt_info_t {
	_os_lock evt_lock;
	enum rtw_phl_evt evt_bitmap;
};

// WiFi FW
struct rtw_fw_info_t {
	u8 fw_en;
	u8 fw_src;
	u8 fw_type;
	u8 dlram_en;
	u8 dlrom_en;
	u8 *rom_buff;
	u32 rom_addr;
	u32 rom_size;
	char rom_path[256];
	u8 *ram_buff;
	u32 ram_size;
	char ram_path[256];
	u8 *buf;
	u32 buf_size;
	u8 *wow_buf;
	u32 wow_buf_size;
	u8 *sym_buf;
	u32 sym_buf_size;
};

enum rtw_fw_status {
	RTW_FW_STATUS_OK,
	RTW_FW_STATUS_NOFW,
	RTW_FW_STATUS_ASSERT,
	RTW_FW_STATUS_EXCEP,
	RTW_FW_STATUS_RXI300,
	RTW_FW_STATUS_HANG
};

#ifdef CONFIG_PHL_DFS
enum dfs_regd_t {
	DFS_REGD_UNKNOWN	= 0,
	DFS_REGD_FCC	= 1,
	DFS_REGD_JAP	= 2,
	DFS_REGD_ETSI	= 3,
};
struct rtw_dfs_t {
	u8 region_domain;
	bool dfs_enabled;
};
#endif

#ifdef CONFIG_PHL_CHANNEL_INFO

#define CHAN_INFO_MAX_SIZE 65535
#define MAX_CHAN_INFO_PKT_KEEP 2
#define CHAN_INFO_PKT_TOTAL MAX_CHAN_INFO_PKT_KEEP + 1

struct csi_header_t {
	u8 mac_addr[6];			/* mdata: u8 ta[6]? */
	u32 hw_assigned_timestamp;	/* mdata: u32 freerun_cnt */
	u8 channel;			/* Drv define */
	u8 bandwidth;			/* mdata: u8 bw */
	u16 rx_data_rate;		/* mdata: u16 rx_rate */
	u8 nc;				/* ch_rpt_hdr_info */
	u8 nr;				/* ch_rpt_hdr_info */
	u16 num_sub_carrier;		/* Drv define*/
	u8 num_bit_per_tone;		/* Drv define per I/Q */
	u8 avg_idle_noise_pwr;		/* ch_rpt_hdr_info */
	u8 evm[2];			/* ch_rpt_hdr_info */
	u8 rssi[2];			/* phy_info_rpt */
	u32 csi_data_length;		/* ch_rpt_hdr_info */
	u8 rxsc;			/* phy_info_rpt */
	u8 ch_matrix_report;		/* mdata: u8 get_ch_info */
	u8 csi_valid;			/* ch_rpt_hdr_info */
};

struct chan_info_t {
	_os_list list;
	u8* chan_info_buffer;
	u32 length;
	struct csi_header_t csi_header;
};

struct rx_chan_info_pool {
	struct chan_info_t channl_info_pkt[CHAN_INFO_PKT_TOTAL];
	_os_list idle;
	_os_list busy;
	_os_lock idle_lock;	/* spinlock */
	_os_lock busy_lock;	/* spinlock */
	u32 idle_cnt;
	u32 busy_cnt;
};

#endif /* CONFIG_PHL_CHANNEL_INFO */

#ifdef CONFIG_MCC_SUPPORT
#define BT_SEG_NUM 2
#define SLOT_NUM 4
#define MIN_TDMRA_SLOT_NUM 2
#define NONSPECIFIC_SETTING 0xff

/*Export to core layer. Phl get the judgement of slot mode*/
enum rtw_phl_mcc_coex_mode {
	RTW_PHL_MCC_COEX_MODE_NONE = 0,
	RTW_PHL_MCC_COEX_MODE_BT_MASTER,
	RTW_PHL_MCC_COEX_MODE_WIFI_MASTER,
	RTW_PHL_MCC_COEX_MODE_BT_WIFI_BALANCE
};

enum rtw_phl_tdmra_wmode {
	RTW_PHL_TDMRA_WMODE_NONE = 0,
	RTW_PHL_TDMRA_AP_CLIENT_WMODE,
	RTW_PHL_TDMRA_2CLIENTS_WMODE,
	RTW_PHL_TDMRA_AP_WMODE,
	RTW_PHL_TDMRA_UNKNOWN_WMODE
};

enum rtw_phl_mcc_dbg_type {
	MCC_DBG_NONE = 0,
	MCC_DBG_STATE,
	MCC_DBG_OP_MODE,
	MCC_DBG_COEX_MODE,
	MCC_DBG_BT_INFO,
	MCC_DBG_EN_INFO
};

enum rtw_phl_mcc_state {
	MCC_NONE = 0,
	MCC_CFG_EN_INFO,
	MCC_TRIGGER_FW_EN,
	MCC_FW_EN_FAIL,
	MCC_RUNING,
	MCC_TRIGGER_FW_DIS,
	MCC_FW_DIS_FAIL,
	MCC_STOP
};

enum rtw_phl_mcc_dur_lim_tag {
	RTW_MCC_DUR_LIM_NONE = 0,
	RTW_MCC_DUR_LIM_NOA
};

/*Export to core layer and hal layyer. Phl get the c2h report mode and config to halmac*/
enum rtw_phl_mcc_rpt {
	RTW_MCC_RPT_OFF = 0,
	RTW_MCC_RPT_FAIL_ONLY,
	RTW_MCC_RPT_ALL
};

/*Export to core layer. Phl get switch ch setting of role from core layer*/
struct rtw_phl_mcc_setting_info {
	struct rtw_wifi_role_t *wrole;
	u8 role_map;/*the wifi role map in operating mcc */
	u8 tx_null_early;
	u16 dur; /*core specific duration in a period of 100 ms */
	bool en_fw_mcc_log;
	u8 fw_mcc_log_lv;/* fw mcc log level */
};

/*Export to core layer. Core get NOA info to update p2p beacon*/
struct rtw_phl_mcc_noa {
	struct rtw_wifi_role_t *wrole;
	u32 start_t_h;
	u32 start_t_l;
	u16 dur;
	u16 interval;
	u8 cnt;
};

struct rtw_phl_mcc_ops {
	void *priv; /* ops private, define by core layer*/
	int (*mcc_update_noa)(void *priv, struct rtw_phl_mcc_noa *param);
	int (*mcc_get_setting)(void *priv, struct rtw_phl_mcc_setting_info *param);
};

/*
 * Export to phl layer and hal layer.
 * Record the debug info.
*/
struct rtw_phl_mcc_dbg_slot_info {
	bool bt_role;
	u16 dur;
	u16 ch;
	u16 macid;
};

struct rtw_phl_mcc_dbg_hal_info {
	u8 slot_num;
	struct rtw_phl_mcc_dbg_slot_info dbg_slot_i[SLOT_NUM];
	bool btc_in_group;
};

struct rtw_phl_mcc_macid_bitmap {
	u32 *bitmap;
	u8 len;
};

struct rtw_phl_mcc_sync_tsf_info {
	u8 sync_en;
	u16 source;
	u16 target;
	u16 offset;
};

struct rtw_phl_mcc_dur_lim_info {
	bool enable;
	enum rtw_phl_mcc_dur_lim_tag tag;
	u16 max_tob;
	u16 max_toa;
	u16 max_dur;
};

struct rtw_phl_mcc_dur_info {
	u16 dur;
	struct rtw_phl_mcc_dur_lim_info dur_limit;
};

struct rtw_phl_mcc_policy_info {
	u8 c2h_rpt;
	u8 tx_null_early;
	u8 dis_tx_null;
	u8 in_curr_ch;
	u8 dis_sw_retry;
	u8 sw_retry_count;
	struct rtw_phl_mcc_dur_info dur_info;
	u8 rfk_chk;
	u8 protect_bcn;
	u8 courtesy_en;
	u8 courtesy_num;
	u8 courtesy_target;
};

struct rtw_phl_mcc_role {
	struct rtw_wifi_role_t *wrole;
	struct rtw_phl_mcc_macid_bitmap used_macid;
	struct rtw_chan_def *chandef;
	struct rtw_phl_mcc_policy_info policy;
	u16 macid;
	u16 bcn_intvl;
	bool bt_role;
	u8 group;
};

/*
 * @c_en: Enable courtesy function
 * @c_num: the time slot of src_role replace by tgt_role
 */
struct rtw_phl_mcc_courtesy {
	bool c_en;
	bool c_num;
	struct rtw_phl_mcc_role *src_role;
	struct rtw_phl_mcc_role *tgt_role;
};

/*
 * @slot: duration, unit: TU
 * @bt_role: True: bt role, False: Wifi role
 * @mrole: mcc role info for Wifi Role
 */
struct rtw_phl_mcc_slot_info {
	u16 slot;
	bool bt_role;
	struct rtw_phl_mcc_role *mrole;
};

/*
 * @slot_num: total slot num(Wifi+BT)
 * @bt_slot_num: total BT slot num
 * |      Dur1      |     Dur2      |
 * 	bcn              bcn
 * |tob_r | toa_r|tob_a | toa_a|
 */
struct rtw_phl_mcc_pattern {
	u8 slot_num;
	u8 bt_slot_num;
	struct rtw_phl_mcc_role *role_ref;
	struct rtw_phl_mcc_role *role_ano;
	s16 tob_r;
	s16 toa_r;
	s16 tob_a;
	s16 toa_a;
	u16 bcns_offset;

	u16 calc_fail;
	/**
	* |tob_r|toa_r|
	* -----------<d_r_d_a_spacing>-----------
	*                                               |tob_a|toa_a|
	**/
	u16 d_r_d_a_spacing_max;
	struct rtw_phl_mcc_courtesy courtesy_i;
	/*record slot order for X wifi slot + Y bt slot*/
	struct rtw_phl_mcc_slot_info slot_order[SLOT_NUM];
};

/*
 * Enable info for mcc
 * @ref_role_idx: the index of reference role
 * @mrole_map: use mcc role num
 * @mrole_num: use mcc role num
 * @group: assigned by halmac mcc, the group resource of fw feture, phl layer ignore it
 *	fw mcc can handle differenec slot pattern, and the group is the id of slot pattern.
 * @tsf_high, tsf_low: Start TSF
 * @tsf_high_l, tsf_low_l: Limitation of Start TSF
 * @dbg_hal_i: Debug info for hal mcc
 */
struct rtw_phl_mcc_en_info {
	struct rtw_phl_mcc_role mcc_role[MCC_ROLE_NUM];
	struct rtw_phl_mcc_sync_tsf_info sync_tsf_info;
	struct rtw_phl_mcc_pattern m_pattern;
	u8 ref_role_idx;
	u8 mrole_map;
	u8 mrole_num;
	u8 group;
	u16 mcc_intvl;
	u32 tsf_high;
	u32 tsf_low;
	u32 tsf_high_l;
	u32 tsf_low_l;
	struct rtw_phl_mcc_dbg_hal_info dbg_hal_i;
};

/*
 * Bt info
 * @bt_dur: bt slot
 * @bt_seg: segment bt slot
 * @bt_seg_num: segment num
 * @add_bt_role: if add_bt_role = true, we need to add bt slot to fw
 */
struct rtw_phl_mcc_bt_info {
	u16 bt_dur;
	u16 bt_seg[BT_SEG_NUM];
	u8 bt_seg_num;
	bool add_bt_role;
};

enum rtw_phl_mcc_chk_inprocess_type {
	RTW_PHL_MCC_CHK_INPROGRESS = 0,
	RTW_PHL_MCC_CHK_INPROGRESS_SINGLE_CH,
	RTW_PHL_MCC_CHK_INPROGRESS_MULTI_CH,
	RTW_PHL_MCC_CHK_MAX,
};

enum mr_coex_trigger {
	MR_COEX_TRIG_BY_BT,
	MR_COEX_TRIG_BY_LINKING,
	MR_COEX_TRIG_BY_DIS_LINKING,
	MR_COEX_TRIG_BY_CHG_SLOT,
	MR_COEX_TRIG_BY_SCAN,
	MR_COEX_TRIG_BY_ECSA,
	MR_COEX_TRIG_BY_CHG_OP_CHDEF,
	MR_COEX_TRIG_MAX,
};

#endif /* CONFIG_MCC_SUPPORT */

/*multi-roles control components*/
enum mr_op_mode {
	MR_OP_NON,
	MR_OP_SCC,
	MR_OP_MCC,
	MR_OP_MAX,
};

enum mr_op_type {
	MR_OP_TYPE_NONE,
	MR_OP_TYPE_STATION_ONLY,
	MR_OP_TYPE_AP_ONLY,
	MR_OP_TYPE_STATION_AP,
	MR_OP_TYPE_MAX,
};

struct mr_info {
	u8 sta_num;
	u8 ld_sta_num;
	u8 lg_sta_num;		/* WIFI_STATION_STATE && WIFI_UNDER_LINKING */

	u8 ap_num;
	u8 ld_ap_num;	/*&& asoc_sta_count > 2*/
	u8 monitor_num;

	u8 p2p_device_num;
	u8 p2p_gc_num;
	u8 p2p_go_num;

#ifdef CONFIG_PHL_TDLS
	u8 ld_tdls_num;	/* phl_role->type == PHL_RTYPE_TDLS */
#endif

#if 0
#ifdef CONFIG_AP_MODE
	u8 starting_ap_num;	/*WIFI_FW_AP_STATE*/
#endif
	u8 adhoc_num;		/* (WIFI_ADHOC_STATE | WIFI_ADHOC_MASTER_STATE) && WIFI_ASOC_STATE */
	u8 ld_adhoc_num;	/* (WIFI_ADHOC_STATE | WIFI_ADHOC_MASTER_STATE) && WIFI_ASOC_STATE && asoc_sta_count > 2 */
#ifdef CONFIG_RTW_MESH
	u8 mesh_num;		/* WIFI_MESH_STATE &&  WIFI_ASOC_STATE */
	u8 ld_mesh_num;		/* WIFI_MESH_STATE &&  WIFI_ASOC_STATE && asoc_sta_count > 2 */
#endif
#endif
};

enum mr_coex_mode {
	MR_COEX_MODE_NONE = 0,
	MR_COEX_MODE_2GSCC_1AP_1STA_BTC = 1,
	MR_COEX_MODE_TDMRA = 2
};

/*export to core layer*/
struct mr_query_info {
	struct mr_info cur_info;
	enum mr_op_mode op_mode;
	enum mr_op_type op_type;
};

struct hw_band_ctl_t {
	_os_lock lock;
	u8 id;
	u8 port_map; /*used port_idx*/
	u8 role_map; /*used role_idx*/
	u8 wmm_map;
	struct mr_info cur_info;
	enum mr_op_mode op_mode;
	enum mr_op_type op_type;
	enum phl_hw_port tsf_sync_port;
	struct phl_queue chan_ctx_queue;/*struct rtw_chan_ctx*/
	enum mr_coex_mode coex_mode;
#ifdef CONFIG_MCC_SUPPORT
	void *mcc_info; /*struct phl_mcc_info*/
#endif
};

#define MAX_BAND_NUM 2
struct rtw_hal_com_t;

#ifdef CONFIG_PHL_P2PPS
struct rtw_phl_noa {
	struct rtw_wifi_role_t *wrole;
	enum p2pps_trig_tag tag;
	u32 start_t_h;
	u32 start_t_l;
	u16 dur;
	u8 cnt;
	u16 interval;
};
#endif

struct rtw_phl_mr_ops {
	void *priv; /* ops private, define by core layer*/
#ifdef CONFIG_PHL_P2PPS
	int (*phl_mr_update_noa)(void *priv, struct rtw_phl_noa *param);
#endif
#ifdef CONFIG_MCC_SUPPORT
	struct rtw_phl_mcc_ops *mcc_ops;
#endif
};

struct mr_ctl_t {
	struct rtw_hal_com_t *hal_com;
	_os_lock lock;
	struct hw_band_ctl_t band_ctrl[MAX_BAND_NUM];
	struct phl_bk_module_ops bk_ops;
	u8 role_map;
	bool is_sb;
	struct rtw_phl_mr_ops mr_ops;
#ifdef CONFIG_MCC_SUPPORT
	u8 init_mcc;
	void *com_mcc;/*struct phl_com_mcc_info*/
#endif
};

enum rtw_rssi_type {
	RTW_RSSI_DATA_ACAM,
	RTW_RSSI_DATA_ACAM_A1M,
	RTW_RSSI_DATA_OTHER,
	RTW_RSSI_CTRL_ACAM,
	RTW_RSSI_CTRL_ACAM_A1M,
	RTW_RSSI_CTRL_OTHER,
	RTW_RSSI_MGNT_ACAM,
	RTW_RSSI_MGNT_ACAM_A1M,
	RTW_RSSI_MGNT_OTHER,
	RTW_RSSI_UNKNOWN,
	RTW_RSSI_TYPE_MAX
};

#define PHL_MAX_RSSI 110
#define PHL_RSSI_MAVG_NUM 16
#define UPDATE_MA_RSSI(_RSSI, _TYPE, _VAL) \
	do { \
		u8 oldest_rssi = 0; \
		if(_RSSI->ma_rssi_ele_idx[_TYPE] < PHL_RSSI_MAVG_NUM) { \
			oldest_rssi = _RSSI->ma_rssi_ele[_TYPE][\
					_RSSI->ma_rssi_ele_idx[_TYPE]]; \
			_RSSI->ma_rssi_ele[_TYPE][_RSSI->ma_rssi_ele_idx[_TYPE]] = \
				((_VAL > PHL_MAX_RSSI) ? PHL_MAX_RSSI : _VAL ); \
		} else { \
			_RSSI->ma_rssi_ele_idx[_TYPE] = 0; \
			oldest_rssi = _RSSI->ma_rssi_ele[_TYPE][0]; \
			_RSSI->ma_rssi_ele[_TYPE][0] = \
				((_VAL > PHL_MAX_RSSI) ? PHL_MAX_RSSI : _VAL ); \
		} \
		_RSSI->ma_rssi_ele_sum[_TYPE] -= oldest_rssi;\
		_RSSI->ma_rssi_ele_sum[_TYPE] += \
			((_VAL > PHL_MAX_RSSI) ? PHL_MAX_RSSI : _VAL ); \
		_RSSI->ma_rssi_ele_idx[_TYPE]++; \
		if(_RSSI->ma_rssi_ele_cnt[_TYPE] < PHL_RSSI_MAVG_NUM) \
			_RSSI->ma_rssi_ele_cnt[_TYPE]++; \
		_RSSI->ma_rssi[_TYPE] = (u8)(_RSSI->ma_rssi_ele_sum[_TYPE] / \
			_RSSI->ma_rssi_ele_cnt[_TYPE]);\
	} while (0)

#define PHL_TRANS_2_RSSI(X) (X >> 1)

struct rtw_phl_rssi_stat {
	_os_lock lock;
	u8 ma_rssi_ele_idx[RTW_RSSI_TYPE_MAX];
	u8 ma_rssi_ele_cnt[RTW_RSSI_TYPE_MAX]; /* maximum : PHL_RSSI_MAVG_NUM */
	u8 ma_rssi_ele[RTW_RSSI_TYPE_MAX][PHL_RSSI_MAVG_NUM]; /* rssi element for moving average */
	u32 ma_rssi_ele_sum[RTW_RSSI_TYPE_MAX];
	u8 ma_rssi[RTW_RSSI_TYPE_MAX]; /* moving average : 0~PHL_MAX_RSSI (dBm = rssi - PHL_MAX_RSSI) */
};

#define PHL_MAX_PPDU_CNT 8
#define PHL_MAX_PPDU_STA_NUM 4
struct rtw_phl_ppdu_sts_sta_ent {
	u8 vld;
	/*u8 rssi;*/
	u16 macid;
};
struct rtw_phl_ppdu_phy_info {
	bool is_valid;
	u8 rssi; /*signal power : 0 - PHL_MAX_RSSI, rssi dbm = PHL_MAX_RSSI - value*/
	u8 rssi_path[RTW_PHL_MAX_RF_PATH];/*PATH A, PATH B ... PATH D*/
	u8 ch_idx;
	u8 tx_bf;
	u8 frame_type; /* type + subtype */
};
#ifdef CONFIG_PHY_INFO_NTFY
struct rtw_phl_ppdu_sts_ntfy {
	bool vld;
	u8 frame_type;
	u8 src_mac_addr[MAC_ADDRESS_LENGTH];
	struct rtw_phl_ppdu_phy_info phy_info;
};
#endif
struct rtw_phl_ppdu_sts_ent {
	/* from normal data */
	u8 frame_type;
	u8 addr_cam_vld;
	u8 crc32;
	u8 ppdu_type;
	u16 rx_rate;
	u8 src_mac_addr[MAC_ADDRESS_LENGTH];

	/* from ppdu status */
	bool valid;
	bool phl_done;
	u8 usr_num;
	u32 freerun_cnt;
	struct rtw_phl_ppdu_phy_info phy_info;
	struct rtw_phl_ppdu_sts_sta_ent sta[PHL_MAX_PPDU_STA_NUM];
#ifdef CONFIG_PHL_RX_PSTS_PER_PKT
	/* for ppdu status per pkt */
	struct phl_queue frames;
#endif
};
struct rtw_phl_ppdu_sts_info {
	struct rtw_phl_ppdu_sts_ent sts_ent[HW_BAND_MAX][PHL_MAX_PPDU_CNT];
	u8 cur_rx_ppdu_cnt[HW_BAND_MAX];
	bool en_ppdu_sts[HW_BAND_MAX];
	bool latest_rx_is_psts[HW_BAND_MAX];
#ifdef CONFIG_PHL_RX_PSTS_PER_PKT
	bool en_psts_per_pkt;
	bool psts_ampdu;
#define RTW_PHL_PSTS_FLTR_MGNT BIT(RTW_FRAME_TYPE_MGNT)
#define RTW_PHL_PSTS_FLTR_CTRL BIT(RTW_FRAME_TYPE_CTRL)
#define RTW_PHL_PSTS_FLTR_DATA BIT(RTW_FRAME_TYPE_DATA)
#define RTW_PHL_PSTS_FLTR_EXT_RSVD BIT(RTW_FRAME_TYPE_EXT_RSVD)
	u8 ppdu_sts_filter;
	u8 en_fake_psts;
	u8 cur_ppdu_cnt[HW_BAND_MAX];
#endif
#ifdef CONFIG_PHY_INFO_NTFY
#define MAX_PSTS_MSG_AGGR_NUM 10
	struct rtw_phl_ppdu_sts_ntfy msg_aggr_buf[MAX_PSTS_MSG_AGGR_NUM];
	u8 msg_aggr_cnt;
#endif
};

struct rtw_phl_gid_pos_tbl {
#define RTW_VHT_GID_MGNT_FRAME_GID_SZ 8
#define RTW_VHT_GID_MGNT_FRAME_POS_SZ 16
	u8 gid_vld[RTW_VHT_GID_MGNT_FRAME_GID_SZ]; /* from 0 - 63 */
	u8 pos[RTW_VHT_GID_MGNT_FRAME_POS_SZ]; /* 0 - 63, per 2 bit*/
};


struct rtw_iot_t {
	u32 id;
};

#ifdef CONFIG_PHL_THERMAL_PROTECT
enum phl_thermal_protect_action{
	PHL_THERMAL_PROTECT_ACTION_NONE = 0,
	PHL_THERMAL_PROTECT_ACTION_LEVEL1 = 1,
	PHL_THERMAL_PROTECT_ACTION_LEVEL2 = 2,
};
#endif

struct rtw_phl_evt_ops;
struct rtw_phl_com_t {
	struct rtw_wifi_role_t wifi_roles[MAX_WIFI_ROLE_NUMBER];
	struct mr_ctl_t mr_ctrl; /*multi wifi_role control module*/
	struct rtw_phl_evt_ops evt_ops;
	enum rtw_hci_type hci_type;
	enum rtw_drv_mode drv_mode;/*normal or mp mode*/
	enum rtw_dev_state dev_state;

	struct hal_spec_t hal_spec;

	struct role_sw_cap_t role_sw_cap; /* SW control capability of role for any purpose */
	struct protocol_cap_t proto_sw_cap[2]; /* SW control wifi protocol capability for any purpose */
	struct phy_sw_cap_t phy_sw_cap[2]; /* SW control phy capability for any purpose */
	struct phy_cap_t phy_cap[2]; /* final capability of phy (intersection of sw/hw) */

	struct dev_cap_t dev_sw_cap;
	struct dev_cap_t dev_cap; /* final capability of device (intersection of sw/hw) */

	struct bus_sw_cap_t bus_sw_cap; /* SW controlled bus capability */

	struct rtw_fw_info_t fw_info;
	struct rtw_evt_info_t evt_info;
	struct rtw_stats phl_stats;
	#ifdef CONFIG_PHL_DFS
	struct rtw_dfs_t dfs_info;
	#endif
	struct rtw_iot_t id;
	/* Flags to control/check RX packets */
	bool append_fcs;
	bool accept_icv_err;

	u8 rf_type; /*enum rf_type , is RF_PATH - GET_HAL_RFPATH*/
	u8 rf_path_num; /*GET_HAL_RFPATH_NUM*/
	u8 regulation;  /*regulation*/
	u8 edcca_mode;

#ifdef CONFIG_PHL_CHANNEL_INFO
	struct rx_chan_info_pool *chan_info_pool;
	struct chan_info_t *chan_info; /* Handle channel info packet */
#endif /* CONFIG_PHL_CHANNEL_INFO */
	void *p2pps_info;

	struct rtw_phl_ppdu_sts_info ppdu_sts_info;
	struct rtw_phl_rssi_stat rssi_stat;
#ifdef CONFIG_PHL_THERMAL_PROTECT
	enum phl_thermal_protect_action thermal_protect_action;
#endif
	void *test_mgnt;

	void *phl_priv; /* pointer to phl_info */
	void *drv_priv;
#ifdef RTW_WKARD_BFEE_SET_AID
	u8 is_in_lps;
#endif
};

struct phl_sec_param_h {
	u8 keyid;
	u8 enc_type;
	u8 key_type;
	u8 key_len;
	u8 spp;
};

#define PHL_MAX_AGG_WSIZE 32

struct mp_usr_sw_tx_gen_in {
	u32 data_rate : 9;
	u32 mcs : 6;
	u32 mpdu_len : 14;
	u32 n_mpdu : 9;
	u32 fec : 1;
	u32 dcm : 1;
	u32 rsvd0 : 1;
	u32 aid : 12;
	u32 scrambler_seed : 8; // rand (1~255)
	u32 random_init_seed : 8; // rand (1~255)
	u32 rsvd1 : 4;
	u32 apep : 22;
	u32 ru_alloc : 8;
	u32 rsvd2 : 2;
	u32 nss : 4;
	u32 txbf : 1;
	u32 pwr_boost_db : 5;
	u32 rsvd3 : 22;
};


struct mp_sw_tx_param_t {
	u32 dbw : 2; //0:BW20, 1:BW40, 2:BW80, 3:BW160/BW80+80
	u32 source_gen_mode : 2;
	u32 locked_clk : 1;
	u32 dyn_bw : 1;
	u32 ndp_en : 1;
	u32 long_preamble_en : 1; //bmode
	u32 stbc : 1;
	u32 gi : 2; //0:0.4,1:0.8,2:1.6,3:3.2
	u32 tb_l_len : 12;
	u32 tb_ru_tot_sts_max : 3;
	u32 vht_txop_not_allowed : 1;
	u32 tb_disam : 1;
	u32 doppler : 2;
	u32 he_ltf_type : 2;//0:1x,1:2x,2:4x

	u32 ht_l_len : 12;
	u32 preamble_puncture : 1;
	u32 he_mcs_sigb : 3;//0~5
	u32 he_dcm_sigb : 1;
	u32 he_sigb_compress_en : 1;
	u32 max_tx_time_0p4us : 14;


	u32 ul_flag : 1;
	u32 tb_ldpc_extra : 1;
	u32 bss_color : 6;
	u32 sr : 4;
	u32 beamchange_en : 1;
	u32 he_er_u106ru_en : 1;
	u32 ul_srp1 : 4;
	u32 ul_srp2 : 4;
	u32 ul_srp3 : 4;
	u32 ul_srp4 : 4;
	u32 mode : 2;

	u32 group_id : 6;
	u32 ppdu_type : 4;//0: bmode,1:Legacy,2:HT_MF,3:HT_GF,4:VHT,5:HE_SU,6:HE_ER_SU,7:HE_MU,8:HE_TB
	u32 txop : 7;
	u32 tb_strt_sts : 3;
	u32 tb_pre_fec_padding_factor : 2;
	u32 cbw : 2;
	u32 txsc : 4;
	u32 tb_mumimo_mode_en : 1;
	u32 rsvd1 : 3;

	u8 nominal_t_pe : 2; // def = 2
	u8 ness : 2; // def = 0
	u8 rsvd2 : 4;

	u8 n_user;
	u16 tb_rsvd : 9;//def = 0
	u16 rsvd3 : 7;

	struct mp_usr_sw_tx_gen_in usr[4];
};

struct mp_usr_plcp_gen_in {
	u32 mcs : 6;
	u32 mpdu_len : 14;
	u32 n_mpdu : 9;
	u32 fec : 1;
	u32 dcm : 1;
	u32 rsvd0 : 1;
	u32 aid : 12;
	u32 scrambler_seed : 8; // rand (1~255)
	u32 random_init_seed : 8; // rand (1~255)
	u32 rsvd1 : 4;
	u32 apep : 22;
	u32 ru_alloc : 8;
	u32 rsvd2 : 2;
	u32 nss : 4;
	u32 txbf : 1;
	u32 pwr_boost_db : 5;
	u32 rsvd3 : 22;
};

enum pkt_ofld_type {
	PKT_TYPE_PROBE_RSP = 0,
	PKT_TYPE_PS_POLL = 1,
	PKT_TYPE_NULL_DATA = 2,
	PKT_TYPE_QOS_NULL = 3,
	PKT_TYPE_CTS2SELF = 4,
	PKT_TYPE_ARP_RSP = 5,
	PKT_TYPE_NDP = 6,
	PKT_TYPE_EAPOL_KEY = 7,
	PKT_TYPE_SA_QUERY = 8,
	PKT_TYPE_REALWOW_KAPKT = 9, /* RealWoW Keep Alive Packet */
	PKT_TYPE_REALWOW_ACK = 10, /* RealWoW Ack Patten */
	PKT_TYPE_REALWOW_WP = 11, /* RealWoW Wakeup Patten */
	PKT_TYPE_PROBE_REQ = 12,
	PKT_OFLD_TYPE_MAX,
};

struct scan_ofld_ch_info {
	u8 center_chan;
	u8 chan; /* primary channel */
	u8 bw;
	u8 period;
	bool tx_pkt; /* 1:probe request will be sent */
	bool tx_data_pause; /* 1:no data will be sent during fw scanning */
};

enum SCAN_OFLD_OP {
	SCAN_OFLD_OP_STOP,
	SCAN_OFLD_OP_START,
	SCAN_OFLD_OP_SET
};

enum SCAN_OFLD_MD {
	/* scan once */
	SCAN_OFLD_MD_ONCE,
	/**
	 * normal period repeatd
	 * e.g., period = 2s
	 * scan - 2s - scan - 2s -....
	 */

	SCAN_OFLD_MD_PD,
	/**
	 * slow period repeat
	 * e.g., period = 2s, cycle = 2, slow period = 3s
	 * scan - 2s - scan - 2s - scan - 3s - scan - 3s
	 */
	SCAN_OFLD_MD_PD_SLOW,
	/**
	 * seamless
	 * scan - scan - scan - scan - scan  -....
	 */
	SCAN_OFLD_MD_SEEMLESS,
};

struct scan_ofld_info {
	enum SCAN_OFLD_OP operation;
	enum SCAN_OFLD_MD mode;
	/* destinated tsf to start scanning, set 0 for both to scan immediately*/
	u32 tsf_low;
	u32 tsf_high;
	u32 probe_req_pkt_id;
	u32 period; /* SCAN_OFLD_MD_PD & SCAN_OFLD_MD_PD_SLOW */
	u8 cycle; /* SCAN_OFLD_MD_PD & SCAN_OFLD_MD_PD_SLOW*/
	u32 slow_period; /* SCAN_OFLD_MD_PD_SLOW */
};

struct mp_plcp_param_t {
	u32 dbw : 2; //0:BW20, 1:BW40, 2:BW80, 3:BW160/BW80+80
	u32 source_gen_mode : 2;
	u32 locked_clk : 1;
	u32 dyn_bw : 1;
	u32 ndp_en : 1;
	u32 long_preamble_en : 1; //bmode
	u32 stbc : 1;
	u32 gi : 2; //0:0.4,1:0.8,2:1.6,3:3.2
	u32 tb_l_len : 12;
	u32 tb_ru_tot_sts_max : 3;
	u32 vht_txop_not_allowed : 1;
	u32 tb_disam : 1;
	u32 doppler : 2;
	u32 he_ltf_type : 2;//0:1x,1:2x,2:4x

	u32 ht_l_len : 12;
	u32 preamble_puncture : 1;
	u32 he_mcs_sigb : 3;//0~5
	u32 he_dcm_sigb : 1;
	u32 he_sigb_compress_en : 1;
	u32 max_tx_time_0p4us : 14;


	u32 ul_flag : 1;
	u32 tb_ldpc_extra : 1;
	u32 bss_color : 6;
	u32 sr : 4;
	u32 beamchange_en : 1;
	u32 he_er_u106ru_en : 1;
	u32 ul_srp1 : 4;
	u32 ul_srp2 : 4;
	u32 ul_srp3 : 4;
	u32 ul_srp4 : 4;
	u32 mode : 2;

	u32 group_id : 6;
	u32 ppdu_type : 4;//0: bmode,1:Legacy,2:HT_MF,3:HT_GF,4:VHT,5:HE_SU,6:HE_ER_SU,7:HE_MU,8:HE_TB
	u32 txop : 7;
	u32 tb_strt_sts : 3;
	u32 tb_pre_fec_padding_factor : 2;
	u32 cbw : 2;
	u32 txsc : 4;
	u32 tb_mumimo_mode_en : 1;
	u32 rsvd1 : 3;

	u8 nominal_t_pe : 2; // def = 2
	u8 ness : 2; // def = 0
	u8 rsvd2 : 4;

	u8 n_user;
	u16 tb_rsvd : 9;//def = 0
	u16 rsvd3 : 7;

	struct mp_usr_plcp_gen_in usr[4];
};



#define MP_MAC_AX_MAX_RU_NUM	4

struct mp_mac_ax_tf_depend_user_para {
	u8 pref_AC: 2;
	u8 rsvd: 6;
};

struct mp_mac_ax_tf_user_para {
	u16 aid12: 12;
	u16 ul_mcs: 4;
	u8 macid;
	u8 ru_pos;

	u8 ul_fec_code: 1;
	u8 ul_dcm: 1;
	u8 ss_alloc: 6;
	u8 ul_tgt_rssi: 7;
	u8 rsvd: 1;
	u16 rsvd2;
};


struct mp_mac_ax_tf_pkt_para {
	u8 ul_bw: 2;
	u8 gi_ltf: 2;
	u8 num_he_ltf: 3;
	u8 ul_stbc: 1;
	u8 doppler: 1;
	u8 ap_tx_power: 6;
	u8 rsvd0: 1;
	u8 user_num: 3;
	u8 pktnum: 3;
	u8 rsvd1: 2;
	u8 pri20_bitmap;

	struct mp_mac_ax_tf_user_para user[MP_MAC_AX_MAX_RU_NUM];
	struct mp_mac_ax_tf_depend_user_para dep_user[MP_MAC_AX_MAX_RU_NUM];
};

struct mp_mac_ax_tf_wd_para {
	u16 datarate: 9;
	u16 mulport_id: 3;
	u16 pwr_ofset: 3;
	u16 rsvd: 1;
};

struct mp_mac_ax_f2p_test_para {
	struct mp_mac_ax_tf_pkt_para tf_pkt;
	struct mp_mac_ax_tf_wd_para tf_wd;
	u8 mode: 2;
	u8 frexch_type: 6;
	u8 sigb_len;
};

struct mp_mac_ax_f2p_wd {
	/* dword 0 */
	u32 cmd_qsel:6;
	u32 rsvd0:2;
	u32 rsvd1:2;
	u32 ls:1;
	u32 fs:1;
	u32 total_number:4;
	u32 seq:8;
	u32 length:8;
	/* dword 1 */
	u32 rsvd2;
};

struct mp_mac_ax_f2p_tx_cmd {
	/* dword 0 */
	u32 cmd_type:8;
	u32 cmd_sub_type:8;
	u32 dl_user_num:5;
	u32 bw:2;
	u32 tx_power:9;
	/* dword 1 */
	u32 fw_define:16;
	u32 ss_sel_mode:2;
	u32 next_qsel:6;
	u32 twt_group:4;
	u32 dis_chk_slp:1;
	u32 ru_mu_2_su:1;
	u32 dl_t_pe:2;
	/* dword 2 */
	u32 sigb_ch1_len:8;
	u32 sigb_ch2_len:8;
	u32 sigb_sym_num:6;
	u32 sigb_ch2_ofs:5;
	u32 dis_htp_ack:1;
	u32 tx_time_ref:2;
	u32 pri_user_idx:2;
	/* dword 3 */
	u32 ampdu_max_txtime:14;
	u32 rsvd0:2;
	u32 group_id:6;
	u32 rsvd1:2;
	u32 rsvd2:4;
	u32 twt_chk_en:1;
	u32 twt_port_id:3;
	/* dword 4 */
	u32 twt_start_time:32;
	/* dword 5 */
	u32 twt_end_time:32;
	/* dword 6 */
	u32 apep_len:12;
	u32 tri_pad:2;
	u32 ul_t_pe:2;
	u32 rf_gain_idx:10;
	u32 fixed_gain_en:1;
	u32 ul_gi_ltf:3;
	u32 ul_doppler:1;
	u32 ul_stbc:1;
	/* dword 7 */
	u32 ul_mid_per:1;
	u32 ul_cqi_rrp_tri:1;
	u32 rsvd3:6;
	u32 rsvd4:8;
	u32 sigb_dcm:1;
	u32 sigb_comp:1;
	u32 doppler:1;
	u32 stbc:1;
	u32 mid_per:1;
	u32 gi_ltf_size:3;
	u32 sigb_mcs:3;
	u32 rsvd5:5;
	/* dword 8 */
	u32 macid_u0:8;
	u32 ac_type_u0:2;
	u32 mu_sta_pos_u0:2;
	u32 dl_rate_idx_u0:9;
	u32 dl_dcm_en_u0:1;
	u32 rsvd6:2;
	u32 ru_alo_idx_u0:8;
	/* dword 9 */
	u32 pwr_boost_u0:5;
	u32 agg_bmp_alo_u0:3;
	u32 ampdu_max_txnum_u0:8;
	u32 user_define_u0:8;
	u32 user_define_ext_u0:8;
	/* dword 10 */
	u32 ul_addr_idx_u0:8;
	u32 ul_dcm_u0:1;
	u32 ul_fec_cod_u0:1;
	u32 ul_ru_rate_u0:7;
	u32 rsvd8:7;
	u32 ul_ru_alo_idx_u0:8;
	/* dword 11 */
	u32 rsvd9:32;
	/* dword 12 */
	u32 macid_u1:8;
	u32 ac_type_u1:2;
	u32 mu_sta_pos_u1:2;
	u32 dl_rate_idx_u1:9;
	u32 dl_dcm_en_u1:1;
	u32 rsvd10:2;
	u32 ru_alo_idx_u1:8;
	/* dword 13 */
	u32 pwr_boost_u1:5;
	u32 agg_bmp_alo_u1:3;
	u32 ampdu_max_txnum_u1:8;
	u32 user_define_u1:8;
	u32 user_define_ext_u1:8;
	/* dword 14 */
	u32 ul_addr_idx_u1:8;
	u32 ul_dcm_u1:1;
	u32 ul_fec_cod_u1:1;
	u32 ul_ru_rate_u1:7;
	u32 rsvd12:7;
	u32 ul_ru_alo_idx_u1:8;
	/* dword 15 */
	u32 rsvd13:32;
	/* dword 16 */
	u32 macid_u2:8;
	u32 ac_type_u2:2;
	u32 mu_sta_pos_u2:2;
	u32 dl_rate_idx_u2:9;
	u32 dl_dcm_en_u2:1;
	u32 rsvd14:2;
	u32 ru_alo_idx_u2:8;
	/* dword 17 */
	u32 pwr_boost_u2:5;
	u32 agg_bmp_alo_u2:3;
	u32 ampdu_max_txnum_u2:8;
	u32 user_define_u2:8;
	u32 user_define_ext_u2:8;
	/* dword 18 */
	u32 ul_addr_idx_u2:8;
	u32 ul_dcm_u2:1;
	u32 ul_fec_cod_u2:1;
	u32 ul_ru_rate_u2:7;
	u32 rsvd16:7;
	u32 ul_ru_alo_idx_u2:8;
	/* dword 19 */
	u32 rsvd17:32;
	/* dword 20 */
	u32 macid_u3:8;
	u32 ac_type_u3:2;
	u32 mu_sta_pos_u3:2;
	u32 dl_rate_idx_u3:9;
	u32 dl_dcm_en_u3:1;
	u32 rsvd18:2;
	u32 ru_alo_idx_u3:8;
	/* dword 21 */
	u32 pwr_boost_u3:5;
	u32 agg_bmp_alo_u3:3;
	u32 ampdu_max_txnum_u3:8;
	u32 user_define_u3:8;
	u32 user_define_ext_u3:8;
	/* dword 22 */
	u32 ul_addr_idx_u3:8;
	u32 ul_dcm_u3:1;
	u32 ul_fec_cod_u3:1;
	u32 ul_ru_rate_u3:7;
	u32 rsvd20:7;
	u32 ul_ru_alo_idx_u3:8;
	/* dword 23 */
	u32 rsvd21:32;
	/* dword 24 */
	u32 pkt_id_0:12;
	u32 rsvd22:3;
	u32 valid_0:1;
	u32 ul_user_num_0:4;
	u32 rsvd23:12;
	/* dword 25 */
	u32 pkt_id_1:12;
	u32 rsvd24:3;
	u32 valid_1:1;
	u32 ul_user_num_1:4;
	u32 rsvd25:12;
	/* dword 26 */
	u32 pkt_id_2:12;
	u32 rsvd26:3;
	u32 valid_2:1;
	u32 ul_user_num_2:4;
	u32 rsvd27:12;
	/* dword 27 */
	u32 pkt_id_3:12;
	u32 rsvd28:3;
	u32 valid_3:1;
	u32 ul_user_num_3:4;
	u32 rsvd29:12;
	/* dword 28 */
	u32 pkt_id_4:12;
	u32 rsvd30:3;
	u32 valid_4:1;
	u32 ul_user_num_4:4;
	u32 rsvd31:12;
	/* dword 29 */
	u32 pkt_id_5:12;
	u32 rsvd32:3;
	u32 valid_5:1;
	u32 ul_user_num_5:4;
	u32 rsvd33:12;
};

u8 mp_start(void *priv);

#ifdef CONFIG_DBCC_SUPPORT
enum dbcc_test_id {
	DBCC_PRE_CFG,
	DBCC_CFG,
	DBCC_CLEAN_TXQ,
};
#endif

struct rtw_role_cmd {
	struct rtw_wifi_role_t *wrole;
	enum role_state rstate;
};

enum phl_btc_pkt_evt_type {
	BTC_PKT_EVT_NORMAL,
	BTC_PKT_EVT_DHCP,
	BTC_PKT_EVT_ARP,
	BTC_PKT_EVT_EAPOL,
	BTC_PKT_EVT_EAPOL_START,
	BTC_PKT_EVT_ADD_KEY,
	BTC_PKT_EVT_MAX
};

struct rtw_pkt_evt_ntfy {
	struct rtw_wifi_role_t *wrole;
	enum phl_btc_pkt_evt_type type;
};

struct role_ntfy_info {
	u8 role_id;
	u16 macid;
	enum role_state rstate;
};

struct battery_chg_ntfy_info {
	bool ips_allow;
	bool lps_allow;
};

struct ps_ntfy_info {
	bool sync;
	void *ctx;
	void (*cb)(void *phl, void *hdl, void *ctx, enum rtw_phl_status stat);
};

struct set_rf_ntfy_info {
	enum rtw_rf_state state_to_set;
	_os_event done;
};


/**
 * rtw_phl_rainfo - structure use to query RA information
 * from hal layer to core/phl layer
 * @rate: current rate selected by RA, define by general definition enum rtw_data_rate
 * @bw: current BW, define by general definition enum channel_width
 * @gi_ltf: current gi_ltf, define by general definition enum rtw_gi_ltf
 */
struct rtw_phl_rainfo {
	enum rtw_data_rate rate;
	enum channel_width bw;
	enum rtw_gi_ltf gi_ltf;
};

struct rtw_env_report {
	bool rpt_status; /*1 means CCX_SUCCESS,0 means fail*/
	u8 clm_ratio;
	u8 nhm_ratio;
	u8 nhm_tx_ratio;
	u8 nhm_pwr;
	u8 nhm_cca_ratio;
};

enum rtw_phl_ser_lv1_recv_step {
	RTW_PHL_SER_LV1_RCVY_STEP_1 = 0,
	RTW_PHL_SER_LV1_SER_RCVY_STEP_2,

	/* keep last */
	RTW_PHL_SER_LV1_RCVY_STEP_LAST,
	RTW_PHL_SER_LV1_RCVY_STEP_MAX = RTW_PHL_SER_LV1_RCVY_STEP_LAST,
	RTW_PHL_SER_LV1_RCVY_STEP_INVALID = RTW_PHL_SER_LV1_RCVY_STEP_LAST,
};

#endif /*_PHL_DEF_H_*/
