/*
 * Fundamental types and constants relating to 802.11v -
 * "Wireless Network Management" extending certain 802.11k -
 * "Radio Resource Measurement of Wireless LANs" features
 *
 * WNM - Wireless Network Management
 * UWNM - Unprotected Wireless Network Management
 * BSSTRANS - BSS Management Transition
 * TIMBC - TIM Broadcast
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

#ifndef _802_11v_h_
#define _802_11v_h_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

#define DOT11_EXTCAP_LEN_FMS			2
#define DOT11_EXTCAP_LEN_PROXY_ARP		2
#define DOT11_EXTCAP_LEN_TFS			3
#define DOT11_EXTCAP_LEN_WNM_SLEEP		3
#define DOT11_EXTCAP_LEN_TIMBC			3
#define DOT11_EXTCAP_LEN_BSSTRANS		3
#define DOT11_EXTCAP_LEN_DMS			4
#define DOT11_EXTCAP_LEN_WNM_NOTIFICATION	6

/* ************* 802.11v related definitions. ************* */

/** BSS Management Transition Query frame header */
BWL_PRE_PACKED_STRUCT struct dot11_bsstrans_query {
	uint8 category;			/* category of action frame (10) */
	uint8 action;			/* WNM action: trans_query (6) */
	uint8 token;			/* dialog token */
	uint8 reason;			/* transition query reason */
	uint8 data[BCM_FLEX_ARRAY];	/* Elements */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_bsstrans_query dot11_bsstrans_query_t;
#define DOT11_BSSTRANS_QUERY_LEN 4	/* Fixed length */

/* BTM transition reason */
#define DOT11_BSSTRANS_REASON_UNSPECIFIED		0
#define DOT11_BSSTRANS_REASON_EXC_FRAME_LOSS		1
#define DOT11_BSSTRANS_REASON_EXC_TRAFFIC_DELAY		2
#define DOT11_BSSTRANS_REASON_INSUFF_QOS_CAPACITY	3
#define DOT11_BSSTRANS_REASON_FIRST_ASSOC		4
#define DOT11_BSSTRANS_REASON_LOAD_BALANCING		5
#define DOT11_BSSTRANS_REASON_BETTER_AP_FOUND		6
#define DOT11_BSSTRANS_REASON_DEAUTH_RX			7
#define DOT11_BSSTRANS_REASON_8021X_EAP_AUTH_FAIL	8
#define DOT11_BSSTRANS_REASON_4WAY_HANDSHK_FAIL		9
#define DOT11_BSSTRANS_REASON_MANY_REPLAYCNT_FAIL	10
#define DOT11_BSSTRANS_REASON_MANY_DATAMIC_FAIL		11
#define DOT11_BSSTRANS_REASON_EXCEED_MAX_RETRANS	12
#define DOT11_BSSTRANS_REASON_MANY_BCAST_DISASSOC_RX	13
#define DOT11_BSSTRANS_REASON_MANY_BCAST_DEAUTH_RX	14
#define DOT11_BSSTRANS_REASON_PREV_TRANSITION_FAIL	15
#define DOT11_BSSTRANS_REASON_LOW_RSSI			16
#define DOT11_BSSTRANS_REASON_ROAM_FROM_NON_80211	17
#define DOT11_BSSTRANS_REASON_RX_BTM_REQ		18
#define DOT11_BSSTRANS_REASON_PREF_LIST_INCLUDED	19
#define DOT11_BSSTRANS_REASON_LEAVING_ESS		20

/** BSS Management Transition Request frame header */
BWL_PRE_PACKED_STRUCT struct dot11_bsstrans_req {
	uint8 category;			/* category of action frame (10) */
	uint8 action;			/* WNM action: trans_req (7) */
	uint8 token;			/* dialog token */
	uint8 reqmode;			/* transition request mode */
	uint16 disassoc_tmr;		/* disassociation timer */
	uint8 validity_intrvl;		/* validity interval */
	uint8 data[BCM_FLEX_ARRAY];	/* optional: BSS term duration, ... */
					/* ...session info URL, candidate list */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_bsstrans_req dot11_bsstrans_req_t;
#define DOT11_BSSTRANS_REQ_LEN 7	/* Fixed length */
#define DOT11_BSSTRANS_REQ_FIXED_LEN 7u	/* Fixed length */

/* BSS Mgmt Transition Request Mode Field - 802.11v */
#define DOT11_BSSTRANS_REQMODE_PREF_LIST_INCL		0x01
#define DOT11_BSSTRANS_REQMODE_ABRIDGED			0x02
#define DOT11_BSSTRANS_REQMODE_DISASSOC_IMMINENT	0x04
#define DOT11_BSSTRANS_REQMODE_BSS_TERM_INCL		0x08
#define DOT11_BSSTRANS_REQMODE_ESS_DISASSOC_IMNT	0x10

/** BSS Management transition response frame header */
BWL_PRE_PACKED_STRUCT struct dot11_bsstrans_resp {
	uint8 category;			/* category of action frame (10) */
	uint8 action;			/* WNM action: trans_resp (8) */
	uint8 token;			/* dialog token */
	uint8 status;			/* transition status */
	uint8 term_delay;		/* validity interval */
	uint8 data[BCM_FLEX_ARRAY];	/* optional: BSSID target, candidate list */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_bsstrans_resp dot11_bsstrans_resp_t;
#define DOT11_BSSTRANS_RESP_LEN 5	/* Fixed length */

/* BSS Mgmt Transition Response Status Field */
#define DOT11_BSSTRANS_RESP_STATUS_ACCEPT			0
#define DOT11_BSSTRANS_RESP_STATUS_REJECT			1
#define DOT11_BSSTRANS_RESP_STATUS_REJ_INSUFF_BCN		2
#define DOT11_BSSTRANS_RESP_STATUS_REJ_INSUFF_CAP		3
#define DOT11_BSSTRANS_RESP_STATUS_REJ_TERM_UNDESIRED		4
#define DOT11_BSSTRANS_RESP_STATUS_REJ_TERM_DELAY_REQ		5
#define DOT11_BSSTRANS_RESP_STATUS_REJ_BSS_LIST_PROVIDED	6
#define DOT11_BSSTRANS_RESP_STATUS_REJ_NO_SUITABLE_BSS		7
#define DOT11_BSSTRANS_RESP_STATUS_REJ_LEAVING_ESS		8

/* Wireless Network Management (WNM) action types */
#define DOT11_WNM_ACTION_EVENT_REQ		0
#define DOT11_WNM_ACTION_EVENT_REP		1
#define DOT11_WNM_ACTION_DIAG_REQ		2
#define DOT11_WNM_ACTION_DIAG_REP		3
#define DOT11_WNM_ACTION_LOC_CFG_REQ		4
#define DOT11_WNM_ACTION_LOC_RFG_RESP		5
#define DOT11_WNM_ACTION_BSSTRANS_QUERY		6
#define DOT11_WNM_ACTION_BSSTRANS_REQ		7
#define DOT11_WNM_ACTION_BSSTRANS_RESP		8
#define DOT11_WNM_ACTION_FMS_REQ		9
#define DOT11_WNM_ACTION_FMS_RESP		10
#define DOT11_WNM_ACTION_COL_INTRFRNCE_REQ	11
#define DOT11_WNM_ACTION_COL_INTRFRNCE_REP	12
#define DOT11_WNM_ACTION_TFS_REQ		13
#define DOT11_WNM_ACTION_TFS_RESP		14
#define DOT11_WNM_ACTION_TFS_NOTIFY_REQ		15
#define DOT11_WNM_ACTION_WNM_SLEEP_REQ		16
#define DOT11_WNM_ACTION_WNM_SLEEP_RESP		17
#define DOT11_WNM_ACTION_TIMBC_REQ		18
#define DOT11_WNM_ACTION_TIMBC_RESP		19
#define DOT11_WNM_ACTION_QOS_TRFC_CAP_UPD	20
#define DOT11_WNM_ACTION_CHAN_USAGE_REQ		21
#define DOT11_WNM_ACTION_CHAN_USAGE_RESP	22
#define DOT11_WNM_ACTION_DMS_REQ		23
#define DOT11_WNM_ACTION_DMS_RESP		24
#define DOT11_WNM_ACTION_TMNG_MEASUR_REQ	25
#define DOT11_WNM_ACTION_NOTFCTN_REQ		26
#define DOT11_WNM_ACTION_NOTFCTN_RESP		27
#define DOT11_WNM_ACTION_TFS_NOTIFY_RESP	28

/* Unprotected Wireless Network Management (WNM) action types */
#define DOT11_UWNM_ACTION_TIM			0
#define DOT11_UWNM_ACTION_TIMING_MEASUREMENT	1

/** BSS Max Idle Period element */
BWL_PRE_PACKED_STRUCT struct dot11_bss_max_idle_period_ie {
	uint8 id;				/* 90, DOT11_MNG_BSS_MAX_IDLE_PERIOD_ID */
	uint8 len;
	uint16 max_idle_period;			/* in unit of 1000 TUs */
	uint8 idle_opt;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_bss_max_idle_period_ie dot11_bss_max_idle_period_ie_t;
#define DOT11_BSS_MAX_IDLE_PERIOD_IE_LEN	3	/* bss max idle period IE size */
#define DOT11_BSS_MAX_IDLE_PERIOD_OPT_PROTECTED	1	/* BSS max idle option */

/** TIM Broadcast request element */
BWL_PRE_PACKED_STRUCT struct dot11_timbc_req_ie {
	uint8 id;				/* 94, DOT11_MNG_TIMBC_REQ_ID */
	uint8 len;
	uint8 interval;				/* in unit of beacon interval */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_timbc_req_ie dot11_timbc_req_ie_t;
#define DOT11_TIMBC_REQ_IE_LEN		1	/* Fixed length */

/** TIM Broadcast request frame header */
BWL_PRE_PACKED_STRUCT struct dot11_timbc_req {
	uint8 category;				/* category of action frame (10) */
	uint8 action;				/* WNM action: DOT11_WNM_ACTION_TIMBC_REQ(18) */
	uint8 token;				/* dialog token */
	uint8 data[BCM_FLEX_ARRAY];		/* TIM broadcast request element */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_timbc_req dot11_timbc_req_t;
#define DOT11_TIMBC_REQ_LEN		3	/* Fixed length */

/** TIM Broadcast response element */
BWL_PRE_PACKED_STRUCT struct dot11_timbc_resp_ie {
	uint8 id;				/* 95, DOT11_MNG_TIM_BROADCAST_RESP_ID */
	uint8 len;
	uint8 status;				/* status of add request */
	uint8 interval;				/* in unit of beacon interval */
	int32 offset;				/* in unit of ms */
	uint16 high_rate;			/* in unit of 0.5 Mb/s */
	uint16 low_rate;			/* in unit of 0.5 Mb/s */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_timbc_resp_ie dot11_timbc_resp_ie_t;
#define DOT11_TIMBC_DENY_RESP_IE_LEN	1	/* Deny. Fixed length */
#define DOT11_TIMBC_ACCEPT_RESP_IE_LEN	10	/* Accept. Fixed length */

#define DOT11_TIMBC_STATUS_ACCEPT		0
#define DOT11_TIMBC_STATUS_ACCEPT_TSTAMP	1
#define DOT11_TIMBC_STATUS_DENY			2
#define DOT11_TIMBC_STATUS_OVERRIDDEN		3
#define DOT11_TIMBC_STATUS_RESERVED		4

/** TIM Broadcast request frame header */
BWL_PRE_PACKED_STRUCT struct dot11_timbc_resp {
	uint8 category;			/* category of action frame (10) */
	uint8 action;			/* action: DOT11_WNM_ACTION_TIMBC_RESP(19) */
	uint8 token;			/* dialog token */
	uint8 data[BCM_FLEX_ARRAY];	/* TIM broadcast response element */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_timbc_resp dot11_timbc_resp_t;
#define DOT11_TIMBC_RESP_LEN	3	/* Fixed length */

/** TIM Broadcast frame header */
BWL_PRE_PACKED_STRUCT struct dot11_timbc {
	uint8 category;			/* category of action frame (11) */
	uint8 action;			/* action: TIM (0) */
	uint8 check_beacon;		/* need to check-beacon */
	uint8 tsf[8];			/* Time Synchronization Function */
	dot11_tim_ie_t tim_ie;		/* TIM element */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_timbc dot11_timbc_t;
#define DOT11_TIMBC_HDR_LEN	(sizeof(dot11_timbc_t) - sizeof(dot11_tim_ie_t))
#define DOT11_TIMBC_FIXED_LEN	(sizeof(dot11_timbc_t) - 1)	/* Fixed length */
#define DOT11_TIMBC_LEN			11	/* Fixed length */

/** TFS request element */
BWL_PRE_PACKED_STRUCT struct dot11_tfs_req_ie {
	uint8 id;				/* 91, DOT11_MNG_TFS_REQUEST_ID */
	uint8 len;
	uint8 tfs_id;
	uint8 actcode;
	uint8 data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tfs_req_ie dot11_tfs_req_ie_t;
#define DOT11_TFS_REQ_IE_LEN		2	/* Fixed length, without id and len */

/** TFS request action codes (bitfield) */
#define DOT11_TFS_ACTCODE_DELETE	1
#define DOT11_TFS_ACTCODE_NOTIFY	2

/** TFS request subelement IDs */
#define DOT11_TFS_REQ_TFS_SE_ID		1
#define DOT11_TFS_REQ_VENDOR_SE_ID	221

/** TFS subelement */
BWL_PRE_PACKED_STRUCT struct dot11_tfs_se {
	uint8 sub_id;
	uint8 len;
	uint8 data[BCM_FLEX_ARRAY];		/* TCLAS element(s) + optional TCLAS proc */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tfs_se dot11_tfs_se_t;

/** TFS response element */
BWL_PRE_PACKED_STRUCT struct dot11_tfs_resp_ie {
	uint8 id;				/* 92, DOT11_MNG_TFS_RESPONSE_ID */
	uint8 len;
	uint8 tfs_id;
	uint8 data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tfs_resp_ie dot11_tfs_resp_ie_t;
#define DOT11_TFS_RESP_IE_LEN		1u	/* Fixed length, without id and len */

/** TFS response subelement IDs (same subelments, but different IDs than in TFS request */
#define DOT11_TFS_RESP_TFS_STATUS_SE_ID		1
#define DOT11_TFS_RESP_TFS_SE_ID		2
#define DOT11_TFS_RESP_VENDOR_SE_ID		221

/** TFS status subelement */
BWL_PRE_PACKED_STRUCT struct dot11_tfs_status_se {
	uint8 sub_id;				/* 92, DOT11_MNG_TFS_RESPONSE_ID */
	uint8 len;
	uint8 resp_st;
	uint8 data[BCM_FLEX_ARRAY];		/* Potential dot11_tfs_se_t included */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tfs_status_se dot11_tfs_status_se_t;
#define DOT11_TFS_STATUS_SE_LEN			1	/* Fixed length, without id and len */

/* Following Definition should be merged to FMS_TFS macro below */
/* TFS Response status code. Identical to FMS Element status, without N/A  */
#define DOT11_TFS_STATUS_ACCEPT			0
#define DOT11_TFS_STATUS_DENY_FORMAT		1
#define DOT11_TFS_STATUS_DENY_RESOURCE		2
#define DOT11_TFS_STATUS_DENY_POLICY		4
#define DOT11_TFS_STATUS_DENY_UNSPECIFIED	5
#define DOT11_TFS_STATUS_ALTPREF_POLICY		7
#define DOT11_TFS_STATUS_ALTPREF_TCLAS_UNSUPP	14

/* FMS Element Status and TFS Response Status Definition */
#define DOT11_FMS_TFS_STATUS_ACCEPT		0
#define DOT11_FMS_TFS_STATUS_DENY_FORMAT	1
#define DOT11_FMS_TFS_STATUS_DENY_RESOURCE	2
#define DOT11_FMS_TFS_STATUS_DENY_MULTIPLE_DI	3
#define DOT11_FMS_TFS_STATUS_DENY_POLICY	4
#define DOT11_FMS_TFS_STATUS_DENY_UNSPECIFIED	5
#define DOT11_FMS_TFS_STATUS_ALT_DIFF_DI	6
#define DOT11_FMS_TFS_STATUS_ALT_POLICY		7
#define DOT11_FMS_TFS_STATUS_ALT_CHANGE_DI	8
#define DOT11_FMS_TFS_STATUS_ALT_MCRATE		9
#define DOT11_FMS_TFS_STATUS_TERM_POLICY	10
#define DOT11_FMS_TFS_STATUS_TERM_RESOURCE	11
#define DOT11_FMS_TFS_STATUS_TERM_HIGHER_PRIO	12
#define DOT11_FMS_TFS_STATUS_ALT_CHANGE_MDI	13
#define DOT11_FMS_TFS_STATUS_ALT_TCLAS_UNSUPP	14

/** TFS Management Request frame header */
BWL_PRE_PACKED_STRUCT struct dot11_tfs_req {
	uint8 category;				/* category of action frame (10) */
	uint8 action;				/* WNM action: TFS request (13) */
	uint8 token;				/* dialog token */
	uint8 data[BCM_FLEX_ARRAY];		/* Elements */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tfs_req dot11_tfs_req_t;
#define DOT11_TFS_REQ_LEN		3	/* Fixed length */

/** TFS Management Response frame header */
BWL_PRE_PACKED_STRUCT struct dot11_tfs_resp {
	uint8 category;				/* category of action frame (10) */
	uint8 action;				/* WNM action: TFS request (14) */
	uint8 token;				/* dialog token */
	uint8 data[BCM_FLEX_ARRAY];		/* Elements */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tfs_resp dot11_tfs_resp_t;
#define DOT11_TFS_RESP_LEN		3	/* Fixed length */

/** TFS Management Notify frame request header */
BWL_PRE_PACKED_STRUCT struct dot11_tfs_notify_req {
	uint8 category;				/* category of action frame (10) */
	uint8 action;				/* WNM action: TFS notify request (15) */
	uint8 tfs_id_cnt;			/* TFS IDs count */
	uint8 tfs_id[BCM_FLEX_ARRAY];		/* Array of TFS IDs */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tfs_notify_req dot11_tfs_notify_req_t;
#define DOT11_TFS_NOTIFY_REQ_LEN	3	/* Fixed length */

/** TFS Management Notify frame response header */
BWL_PRE_PACKED_STRUCT struct dot11_tfs_notify_resp {
	uint8 category;				/* category of action frame (10) */
	uint8 action;				/* WNM action: TFS notify response (28) */
	uint8 tfs_id_cnt;			/* TFS IDs count */
	uint8 tfs_id[BCM_FLEX_ARRAY];		/* Array of TFS IDs */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tfs_notify_resp dot11_tfs_notify_resp_t;
#define DOT11_TFS_NOTIFY_RESP_LEN	3	/* Fixed length */

/** WNM-Sleep Management Request frame header */
BWL_PRE_PACKED_STRUCT struct dot11_wnm_sleep_req {
	uint8 category;				/* category of action frame (10) */
	uint8 action;				/* WNM action: wnm-sleep request (16) */
	uint8 token;				/* dialog token */
	uint8 data[BCM_FLEX_ARRAY];		/* Elements */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_wnm_sleep_req dot11_wnm_sleep_req_t;
#define DOT11_WNM_SLEEP_REQ_LEN		3	/* Fixed length */

/** WNM-Sleep Management Response frame header */
BWL_PRE_PACKED_STRUCT struct dot11_wnm_sleep_resp {
	uint8 category;				/* category of action frame (10) */
	uint8 action;				/* WNM action: wnm-sleep request (17) */
	uint8 token;				/* dialog token */
	uint16 key_len;				/* key data length */
	uint8 data[BCM_FLEX_ARRAY];		/* Elements */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_wnm_sleep_resp dot11_wnm_sleep_resp_t;
#define DOT11_WNM_SLEEP_RESP_LEN	5	/* Fixed length */

#define DOT11_WNM_SLEEP_SUBELEM_ID_GTK	0
#define DOT11_WNM_SLEEP_SUBELEM_ID_IGTK	1

BWL_PRE_PACKED_STRUCT struct dot11_wnm_sleep_subelem_gtk {
	uint8 sub_id;
	uint8 len;
	uint16 key_info;
	uint8 key_length;
	uint8 rsc[8];
	uint8 key[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_wnm_sleep_subelem_gtk dot11_wnm_sleep_subelem_gtk_t;
#define DOT11_WNM_SLEEP_SUBELEM_GTK_FIXED_LEN	11	/* without sub_id, len, and key */
#define DOT11_WNM_SLEEP_SUBELEM_GTK_MAX_LEN	43	/* without sub_id and len */

BWL_PRE_PACKED_STRUCT struct dot11_wnm_sleep_subelem_igtk {
	uint8 sub_id;
	uint8 len;
	uint16 key_id;
	uint8 pn[6];
	uint8 key[16];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_wnm_sleep_subelem_igtk dot11_wnm_sleep_subelem_igtk_t;
#define DOT11_WNM_SLEEP_SUBELEM_IGTK_LEN 24	/* Fixed length */

BWL_PRE_PACKED_STRUCT struct dot11_wnm_sleep_ie {
	uint8 id;				/* 93, DOT11_MNG_WNM_SLEEP_MODE_ID */
	uint8 len;
	uint8 act_type;
	uint8 resp_status;
	uint16 interval;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_wnm_sleep_ie dot11_wnm_sleep_ie_t;
#define DOT11_WNM_SLEEP_IE_LEN		4	/* Fixed length */

#define DOT11_WNM_SLEEP_ACT_TYPE_ENTER	0
#define DOT11_WNM_SLEEP_ACT_TYPE_EXIT	1

#define DOT11_WNM_SLEEP_RESP_ACCEPT	0
#define DOT11_WNM_SLEEP_RESP_UPDATE	1
#define DOT11_WNM_SLEEP_RESP_DENY	2
#define DOT11_WNM_SLEEP_RESP_DENY_TEMP	3
#define DOT11_WNM_SLEEP_RESP_DENY_KEY	4
#define DOT11_WNM_SLEEP_RESP_DENY_INUSE	5
#define DOT11_WNM_SLEEP_RESP_LAST	6

/** DMS Management Request frame header */
BWL_PRE_PACKED_STRUCT struct dot11_dms_req {
	uint8 category;				/* category of action frame (10) */
	uint8 action;				/* WNM action: dms request (23) */
	uint8 token;				/* dialog token */
	uint8 data[BCM_FLEX_ARRAY];		/* Elements */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_dms_req dot11_dms_req_t;
#define DOT11_DMS_REQ_LEN		3	/* Fixed length */

/** DMS Management Response frame header */
BWL_PRE_PACKED_STRUCT struct dot11_dms_resp {
	uint8 category;				/* category of action frame (10) */
	uint8 action;				/* WNM action: dms request (24) */
	uint8 token;				/* dialog token */
	uint8 data[BCM_FLEX_ARRAY];		/* Elements */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_dms_resp dot11_dms_resp_t;
#define DOT11_DMS_RESP_LEN		3	/* Fixed length */

/** DMS request element */
BWL_PRE_PACKED_STRUCT struct dot11_dms_req_ie {
	uint8 id;				/* 99, DOT11_MNG_DMS_REQUEST_ID */
	uint8 len;
	uint8 data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_dms_req_ie dot11_dms_req_ie_t;
#define DOT11_DMS_REQ_IE_LEN		2	/* Fixed length */

/** DMS response element */
BWL_PRE_PACKED_STRUCT struct dot11_dms_resp_ie {
	uint8 id;				/* 100, DOT11_MNG_DMS_RESPONSE_ID */
	uint8 len;
	uint8 data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_dms_resp_ie dot11_dms_resp_ie_t;
#define DOT11_DMS_RESP_IE_LEN		2	/* Fixed length */

/** DMS request descriptor */
BWL_PRE_PACKED_STRUCT struct dot11_dms_req_desc {
	uint8 dms_id;
	uint8 len;
	uint8 type;
	uint8 data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_dms_req_desc dot11_dms_req_desc_t;
#define DOT11_DMS_REQ_DESC_LEN		3	/* Fixed length */

#define DOT11_DMS_REQ_TYPE_ADD		0
#define DOT11_DMS_REQ_TYPE_REMOVE	1
#define DOT11_DMS_REQ_TYPE_CHANGE	2

/** DMS response status */
BWL_PRE_PACKED_STRUCT struct dot11_dms_resp_st {
	uint8 dms_id;
	uint8 len;
	uint8 type;
	uint16 lsc;
	uint8 data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_dms_resp_st dot11_dms_resp_st_t;
#define DOT11_DMS_RESP_STATUS_LEN	5	/* Fixed length */

#define DOT11_DMS_RESP_TYPE_ACCEPT	0
#define DOT11_DMS_RESP_TYPE_DENY	1
#define DOT11_DMS_RESP_TYPE_TERM	2

#define DOT11_DMS_RESP_LSC_UNSUPPORTED	0xFFFF

/** WNM-Notification Request frame header */
BWL_PRE_PACKED_STRUCT struct dot11_wnm_notif_req {
	uint8 category;				/* category of action frame (10) */
	uint8 action;				/* WNM action: Notification request (26) */
	uint8 token;				/* dialog token */
	uint8 type;				/* type */
	uint8 data[BCM_FLEX_ARRAY];		/* Sub-elements */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_wnm_notif_req dot11_wnm_notif_req_t;
#define DOT11_WNM_NOTIF_REQ_LEN		4	/* Fixed length */

/** FMS Management Request frame header */
BWL_PRE_PACKED_STRUCT struct dot11_fms_req {
	uint8 category;				/* category of action frame (10) */
	uint8 action;				/* WNM action: fms request (9) */
	uint8 token;				/* dialog token */
	uint8 data[BCM_FLEX_ARRAY];		/* Elements */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_fms_req dot11_fms_req_t;
#define DOT11_FMS_REQ_LEN		3	/* Fixed length */

/** FMS Management Response frame header */
BWL_PRE_PACKED_STRUCT struct dot11_fms_resp {
	uint8 category;				/* category of action frame (10) */
	uint8 action;				/* WNM action: fms request (10) */
	uint8 token;				/* dialog token */
	uint8 data[BCM_FLEX_ARRAY];		/* Elements */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_fms_resp dot11_fms_resp_t;
#define DOT11_FMS_RESP_LEN		3	/* Fixed length */

/** FMS Descriptor element */
BWL_PRE_PACKED_STRUCT struct dot11_fms_desc {
	uint8 id;
	uint8 len;
	uint8 num_fms_cnt;
	uint8 data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_fms_desc dot11_fms_desc_t;
#define DOT11_FMS_DESC_LEN		1	/* Fixed length */

#define DOT11_FMS_CNTR_MAX		0x8
#define DOT11_FMS_CNTR_ID_MASK		0x7
#define DOT11_FMS_CNTR_ID_SHIFT		0x0
#define DOT11_FMS_CNTR_COUNT_MASK	0xf1
#define DOT11_FMS_CNTR_SHIFT		0x3

/** FMS request element */
BWL_PRE_PACKED_STRUCT struct dot11_fms_req_ie {
	uint8 id;
	uint8 len;
	uint8 fms_token;			/* token used to identify fms stream set */
	uint8 data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_fms_req_ie dot11_fms_req_ie_t;
#define DOT11_FMS_REQ_IE_FIX_LEN		1	/* Fixed length */

BWL_PRE_PACKED_STRUCT struct dot11_rate_id_field {
	uint8 mask;
	uint8 mcs_idx;
	uint16 rate;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rate_id_field dot11_rate_id_field_t;
#define DOT11_RATE_ID_FIELD_MCS_SEL_MASK	0x7
#define DOT11_RATE_ID_FIELD_MCS_SEL_OFFSET	0
#define DOT11_RATE_ID_FIELD_RATETYPE_MASK	0x18
#define DOT11_RATE_ID_FIELD_RATETYPE_OFFSET	3
#define DOT11_RATE_ID_FIELD_LEN		sizeof(dot11_rate_id_field_t)

/** FMS request subelements */
BWL_PRE_PACKED_STRUCT struct dot11_fms_se {
	uint8 sub_id;
	uint8 len;
	uint8 interval;
	uint8 max_interval;
	dot11_rate_id_field_t rate;
	uint8 data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_fms_se dot11_fms_se_t;
#define DOT11_FMS_REQ_SE_LEN		6	/* Fixed length */

#define DOT11_FMS_REQ_SE_ID_FMS		1	/* FMS subelement */
#define DOT11_FMS_REQ_SE_ID_VS		221	/* Vendor Specific subelement */

/** FMS response element */
BWL_PRE_PACKED_STRUCT struct dot11_fms_resp_ie {
	uint8 id;
	uint8 len;
	uint8 fms_token;
	uint8 data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_fms_resp_ie dot11_fms_resp_ie_t;
#define DOT11_FMS_RESP_IE_FIX_LEN		1	/* Fixed length */

/* FMS status subelements */
#define DOT11_FMS_STATUS_SE_ID_FMS	1	/* FMS Status */
#define DOT11_FMS_STATUS_SE_ID_TCLAS	2	/* TCLAS Status */
#define DOT11_FMS_STATUS_SE_ID_VS	221	/* Vendor Specific subelement */

/** FMS status subelement */
BWL_PRE_PACKED_STRUCT struct dot11_fms_status_se {
	uint8 sub_id;
	uint8 len;
	uint8 status;
	uint8 interval;
	uint8 max_interval;
	uint8 fmsid;
	uint8 counter;
	dot11_rate_id_field_t rate;
	uint8 mcast_addr[ETHER_ADDR_LEN];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_fms_status_se dot11_fms_status_se_t;
#define DOT11_FMS_STATUS_SE_LEN		15	/* Fixed length */

/** TCLAS status subelement */
BWL_PRE_PACKED_STRUCT struct dot11_tclas_status_se {
	uint8 sub_id;
	uint8 len;
	uint8 fmsid;
	uint8 data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tclas_status_se dot11_tclas_status_se_t;
#define DOT11_TCLAS_STATUS_SE_LEN		1	/* Fixed length */

/** TCLAS frame classifier type */
BWL_PRE_PACKED_STRUCT struct dot11_tclas_fc_hdr {
	uint8 type;
	uint8 mask;
	uint8 data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tclas_fc_hdr dot11_tclas_fc_hdr_t;
#define DOT11_TCLAS_FC_HDR_LEN		2	/* Fixed length */

#define DOT11_TCLAS_MASK_0		0x1
#define DOT11_TCLAS_MASK_1		0x2
#define DOT11_TCLAS_MASK_2		0x4
#define DOT11_TCLAS_MASK_3		0x8
#define DOT11_TCLAS_MASK_4		0x10
#define DOT11_TCLAS_MASK_5		0x20
#define DOT11_TCLAS_MASK_6		0x40
#define DOT11_TCLAS_MASK_7		0x80

#define DOT11_TCLAS_FC_0_ETH		0
#define DOT11_TCLAS_FC_1_IP		1
#define DOT11_TCLAS_FC_2_8021Q		2
#define DOT11_TCLAS_FC_3_OFFSET		3
#define DOT11_TCLAS_FC_4_IP_HIGHER	4
#define DOT11_TCLAS_FC_5_8021D		5
#define DOT11_TCLAS_FC_10_IP_HIGHER	10 /* classifier type 10, IP extensions and
					    * higher layer parameters
					    */

/** TCLAS frame classifier type 0 parameters for Ethernet */
BWL_PRE_PACKED_STRUCT struct dot11_tclas_fc_0_eth {
	uint8 type;
	uint8 mask;
	uint8 sa[ETHER_ADDR_LEN];
	uint8 da[ETHER_ADDR_LEN];
	uint16 eth_type;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tclas_fc_0_eth dot11_tclas_fc_0_eth_t;
#define DOT11_TCLAS_FC_0_ETH_LEN	16

/** TCLAS frame classifier type 1 parameters for IPV4 */
BWL_PRE_PACKED_STRUCT struct dot11_tclas_fc_1_ipv4 {
	uint8 type;
	uint8 mask;
	uint8 version;
	uint32 src_ip;
	uint32 dst_ip;
	uint16 src_port;
	uint16 dst_port;
	uint8 dscp;
	uint8 protocol;
	uint8 reserved;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tclas_fc_1_ipv4 dot11_tclas_fc_1_ipv4_t;
#define DOT11_TCLAS_FC_1_IPV4_LEN	18

/** TCLAS frame classifier type 2 parameters for 802.1Q */
BWL_PRE_PACKED_STRUCT struct dot11_tclas_fc_2_8021q {
	uint8 type;
	uint8 mask;
	uint16 tci;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tclas_fc_2_8021q dot11_tclas_fc_2_8021q_t;
#define DOT11_TCLAS_FC_2_8021Q_LEN	4

/** TCLAS frame classifier type 3 parameters for filter offset */
BWL_PRE_PACKED_STRUCT struct dot11_tclas_fc_3_filter {
	uint8 type;
	uint8 mask;
	uint16 offset;
	uint8 data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tclas_fc_3_filter dot11_tclas_fc_3_filter_t;
#define DOT11_TCLAS_FC_3_FILTER_LEN	4

/** TCLAS frame classifier type 4 parameters for IPV4 is the same as TCLAS type 1 */
typedef struct dot11_tclas_fc_1_ipv4 dot11_tclas_fc_4_ipv4_t;
#define DOT11_TCLAS_FC_4_IPV4_LEN	DOT11_TCLAS_FC_1_IPV4_LEN

/** TCLAS frame classifier type 4 parameters for IPV6 */
BWL_PRE_PACKED_STRUCT struct dot11_tclas_fc_4_ipv6 {
	uint8 type;
	uint8 mask;
	uint8 version;
	uint8 saddr[16];
	uint8 daddr[16];
	uint16 src_port;
	uint16 dst_port;
	uint8 dscp;
	uint8 nexthdr;
	uint8 flow_lbl[3];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tclas_fc_4_ipv6 dot11_tclas_fc_4_ipv6_t;
#define DOT11_TCLAS_FC_4_IPV6_LEN	44

/** TCLAS frame classifier type 5 parameters for 802.1D */
BWL_PRE_PACKED_STRUCT struct dot11_tclas_fc_5_8021d {
	uint8 type;
	uint8 mask;
	uint8 pcp;
	uint8 cfi;
	uint16 vid;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tclas_fc_5_8021d dot11_tclas_fc_5_8021d_t;
#define DOT11_TCLAS_FC_5_8021D_LEN	6

/** TCLAS frame classifier type 10 parameters for IP extensions and higher layer parameters */
BWL_PRE_PACKED_STRUCT struct dot11_tclas_fc_10_ip_ext {
	uint8 type;
	uint8 proto_inst;	/* protocol instance */
	uint8 proto_or_nh;	/* protocol(for IPv4) or next header(for IPv6) */
	uint8 data[];
	/* variable filter value */
	/* variable filer mask */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tclas_fc_10_ip_ext dot11_tclas_fc_10_ip_ext_t;
#define DOT11_TCLAS_FC_10_IP_EXT_LEN	3u

/** TCLAS frame classifier type parameters */
BWL_PRE_PACKED_STRUCT union dot11_tclas_fc {
	uint8 data[1];
	dot11_tclas_fc_hdr_t hdr;
	dot11_tclas_fc_0_eth_t t0_eth;
	dot11_tclas_fc_1_ipv4_t	t1_ipv4;
	dot11_tclas_fc_2_8021q_t t2_8021q;
	dot11_tclas_fc_3_filter_t t3_filter;
	dot11_tclas_fc_4_ipv4_t	t4_ipv4;
	dot11_tclas_fc_4_ipv6_t	t4_ipv6;
	dot11_tclas_fc_5_8021d_t t5_8021d;
} BWL_POST_PACKED_STRUCT;
typedef union dot11_tclas_fc dot11_tclas_fc_t;

#define DOT11_TCLAS_FC_MIN_LEN		4	/* Classifier Type 2 has the min size */
#define DOT11_TCLAS_FC_MAX_LEN		254

/** TCLAS element */
BWL_PRE_PACKED_STRUCT struct dot11_tclas_ie {
	uint8 id;				/* 14, DOT11_MNG_TCLAS_ID */
	uint8 len;
	uint8 user_priority;
	dot11_tclas_fc_t fc;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tclas_ie dot11_tclas_ie_t;
#define DOT11_TCLAS_IE_LEN		3u	/* Fixed length, include id and len */

/** TCLAS processing element */
BWL_PRE_PACKED_STRUCT struct dot11_tclas_proc_ie {
	uint8 id;				/* 44, DOT11_MNG_TCLAS_PROC_ID */
	uint8 len;
	uint8 process;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tclas_proc_ie dot11_tclas_proc_ie_t;
#define DOT11_TCLAS_PROC_IE_LEN		3	/* Fixed length, include id and len */

#define DOT11_TCLAS_PROC_LEN		1u	/* Proc ie length is always 1 byte */

#define DOT11_TCLAS_PROC_MATCHALL	0	/* All high level element need to match */
#define DOT11_TCLAS_PROC_MATCHONE	1	/* One high level element need to match */
#define DOT11_TCLAS_PROC_NONMATCH	2	/* Non match to any high level element */

/* TSPEC element defined in 802.11 std section 8.4.2.32 - Not supported */
#define DOT11_TSPEC_IE_LEN		57	/* Fixed length */

/** TCLAS Mask element */
BWL_PRE_PACKED_STRUCT struct dot11_tclas_mask_ie {
	uint8 id;				/* DOT11_MNG_ID_EXT_ID (255) */
	uint8 len;
	uint8 id_ext;				/* TCLAS_EXTID_MNG_MASK_ID (89) */
	dot11_tclas_fc_t fc;			/* Variable length frame classifier (fc) */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tclas_mask_ie dot11_tclas_mask_ie_t;
#define DOT11_TCLAS_MASK_IE_LEN		1u	/* Fixed length, excludes id and len */
#define DOT11_TCLAS_MASK_IE_HDR_LEN	3u	/* Fixed length */

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif /* _802_11v_h_ */
