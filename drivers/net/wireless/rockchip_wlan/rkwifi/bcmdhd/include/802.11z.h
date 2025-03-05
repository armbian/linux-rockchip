/*
 * Fundamental types and constants relating to 802.11z -
 * "Extensions to Direct-Link Setup (DLS)"
 *
 * TDLS - Tunneled DLS
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

#ifndef _802_11z_h_
#define _802_11z_h_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

#ifndef _NET_ETHERNET_H_
#include <ethernet.h>
#endif

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

#define DOT11_EXTCAP_LEN_TDLS	5
#define DOT11_11AC_EXTCAP_LEN_TDLS	8

#define DOT11_EXTCAP_LEN_TDLS_WBW		8

/* TDLS Capabilities */
#define DOT11_TDLS_CAP_TDLS			37	/* TDLS support */
#define DOT11_TDLS_CAP_PU_BUFFER_STA		28	/* TDLS Peer U-APSD buffer STA support */
#define DOT11_TDLS_CAP_PEER_PSM			20	/* TDLS Peer PSM support */
#define DOT11_TDLS_CAP_CH_SW			30	/* TDLS Channel switch */
#define DOT11_TDLS_CAP_PROH			38	/* TDLS prohibited */
#define DOT11_TDLS_CAP_CH_SW_PROH		39	/* TDLS Channel switch prohibited */
#define DOT11_TDLS_CAP_TDLS_WIDER_BW		61	/* TDLS Wider Band-Width */

#define TDLS_CAP_MAX_BIT			39	/* TDLS max bit defined in ext cap */

/** Timeout Interval IE */
BWL_PRE_PACKED_STRUCT struct ti_ie {
	uint8 ti_type;
	uint32 ti_val;
} BWL_POST_PACKED_STRUCT;
typedef struct ti_ie ti_ie_t;
#define TI_TYPE_REASSOC_DEADLINE	1
#define TI_TYPE_KEY_LIFETIME		2

/** Link Identifier Element */
BWL_PRE_PACKED_STRUCT struct link_id_ie {
	uint8 id;
	uint8 len;
	struct ether_addr	bssid;
	struct ether_addr	tdls_init_mac;
	struct ether_addr	tdls_resp_mac;
} BWL_POST_PACKED_STRUCT;
typedef struct link_id_ie link_id_ie_t;
#define TDLS_LINK_ID_IE_LEN		18u

/** Link Wakeup Schedule Element */
BWL_PRE_PACKED_STRUCT struct wakeup_sch_ie {
	uint8 id;
	uint8 len;
	uint32 offset;			/* in ms between TSF0 and start of 1st Awake Window */
	uint32 interval;		/* in ms bwtween the start of 2 Awake Windows */
	uint32 awake_win_slots;	/* in backof slots, duration of Awake Window */
	uint32 max_wake_win;	/* in ms, max duration of Awake Window */
	uint16 idle_cnt;		/* number of consecutive Awake Windows */
} BWL_POST_PACKED_STRUCT;
typedef struct wakeup_sch_ie wakeup_sch_ie_t;
#define TDLS_WAKEUP_SCH_IE_LEN		18

/** Channel Switch Timing Element */
BWL_PRE_PACKED_STRUCT struct channel_switch_timing_ie {
	uint8 id;
	uint8 len;
	uint16 switch_time;		/* in ms, time to switch channels */
	uint16 switch_timeout;	/* in ms */
} BWL_POST_PACKED_STRUCT;
typedef struct channel_switch_timing_ie channel_switch_timing_ie_t;
#define TDLS_CHANNEL_SWITCH_TIMING_IE_LEN		4

/** PTI Control Element */
BWL_PRE_PACKED_STRUCT struct pti_control_ie {
	uint8 id;
	uint8 len;
	uint8 tid;
	uint16 seq_control;
} BWL_POST_PACKED_STRUCT;
typedef struct pti_control_ie pti_control_ie_t;
#define TDLS_PTI_CONTROL_IE_LEN		3

/** PU Buffer Status Element */
BWL_PRE_PACKED_STRUCT struct pu_buffer_status_ie {
	uint8 id;
	uint8 len;
	uint8 status;
} BWL_POST_PACKED_STRUCT;
typedef struct pu_buffer_status_ie pu_buffer_status_ie_t;
#define TDLS_PU_BUFFER_STATUS_IE_LEN	1
#define TDLS_PU_BUFFER_STATUS_AC_BK		1
#define TDLS_PU_BUFFER_STATUS_AC_BE		2
#define TDLS_PU_BUFFER_STATUS_AC_VI		4
#define TDLS_PU_BUFFER_STATUS_AC_VO		8

/* TDLS Action Field Values */
#define TDLS_SETUP_REQ				0
#define TDLS_SETUP_RESP				1
#define TDLS_SETUP_CONFIRM			2
#define TDLS_TEARDOWN				3
#define TDLS_PEER_TRAFFIC_IND			4
#define TDLS_CHANNEL_SWITCH_REQ			5
#define TDLS_CHANNEL_SWITCH_RESP		6
#define TDLS_PEER_PSM_REQ			7
#define TDLS_PEER_PSM_RESP			8
#define TDLS_PEER_TRAFFIC_RESP			9
#define TDLS_DISCOVERY_REQ			10

/* 802.11z TDLS Public Action Frame action field */
#define TDLS_DISCOVERY_RESP			14

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif /* _802_11z_h_ */
