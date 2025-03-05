/*
 * Fundamental types and constants relating to the 802.11 base spec..
 *
 * All amendment specific work should be placed in separate
 * amendment specific header files e.g. 802.11k.h.
 *
 * WFA related work should be placed in 802.11wfa.h.
 * Broadcom specific work should be placed in 802.11brcm.h.
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

#ifndef _802_11_H_
#define _802_11_H_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

#ifndef _NET_ETHERNET_H_
#include <ethernet.h>
#endif

/* Include WPA definitions here for compatibility */
#include <wpa.h>

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

#define DOT11_TU_TO_US			1024	/* 802.11 Time Unit is 1024 microseconds */
#define DOT11_SEC_TO_TU			977u	/* 1000000 / DOT11_TU_TO_US = ~977 TU */

/* Generic 802.11 frame constants */
#define DOT11_A3_HDR_LEN		24	/* d11 header length with A3 */
#define DOT11_A4_HDR_LEN		30	/* d11 header length with A4 */
#define DOT11_MAC_HDR_LEN		DOT11_A3_HDR_LEN	/* MAC header length */
#define DOT11_FCS_LEN			4u	/* d11 FCS length */
#define DOT11_ICV_LEN			4	/* d11 ICV length */
#define DOT11_ICV_AES_LEN		8	/* d11 ICV/AES length */
#define DOT11_MAX_ICV_AES_LEN		16	/* d11 MAX ICV/AES length */
#define DOT11_QOS_LEN			2	/* d11 QoS length */
#define DOT11_HTC_LEN			4	/* d11 HT Control field length */

#define DOT11_KEY_INDEX_SHIFT		6	/* d11 key index shift */
#define DOT11_IV_LEN			4	/* d11 IV length */
#define DOT11_IV_TKIP_LEN		8	/* d11 IV TKIP length */
#define DOT11_IV_AES_OCB_LEN		4	/* d11 IV/AES/OCB length */
#define DOT11_IV_AES_CCM_LEN		8	/* d11 IV/AES/CCM length */
#define DOT11_IV_WAPI_LEN		18	/* d11 IV WAPI length */
/* TODO: Need to change DOT11_IV_MAX_LEN to 18, but currently unable to change as the old
 * branches are still referencing to this component.
 */
#define DOT11_IV_MAX_LEN		8	/* maximum iv len for any encryption */

/* Includes MIC */
#define DOT11_MAX_MPDU_BODY_LEN		2304	/* max MPDU body length */
/* A4 header + QoS + CCMP + PDU + ICV + FCS = 2352 */
#define DOT11_MAX_MPDU_LEN		(DOT11_A4_HDR_LEN + \
					 DOT11_QOS_LEN + \
					 DOT11_IV_AES_CCM_LEN + \
					 DOT11_MAX_MPDU_BODY_LEN + \
					 DOT11_ICV_LEN + \
					 DOT11_FCS_LEN)	/* d11 max MPDU length */

#define DOT11_MAX_SSID_LEN		32	/* d11 max ssid length */

/* dot11RTSThreshold */
#define DOT11_DEFAULT_RTS_LEN		2347	/* d11 default RTS length */
#define DOT11_MAX_RTS_LEN		2347	/* d11 max RTS length */

/* dot11FragmentationThreshold */
#define DOT11_MIN_FRAG_LEN		256	/* d11 min fragmentation length */
#define DOT11_MAX_FRAG_LEN		2346	/* Max frag is also limited by aMPDUMaxLength
						* of the attached PHY
						*/
#define DOT11_DEFAULT_FRAG_LEN		2346	/* d11 default fragmentation length */

/* dot11BeaconPeriod */
#define DOT11_MIN_BEACON_PERIOD		1	/* d11 min beacon period */
#define DOT11_MAX_BEACON_PERIOD		0xFFFF	/* d11 max beacon period */

/* dot11DTIMPeriod */
#define DOT11_MIN_DTIM_PERIOD		1	/* d11 min DTIM period */
#define DOT11_MAX_DTIM_PERIOD		0xFF	/* d11 max DTIM period */

/** 802.2 LLC/SNAP header used by 802.11 per 802.1H */
#define DOT11_LLC_SNAP_HDR_LEN		8	/* d11 LLC/SNAP header length */
/* minimum LLC header length; DSAP, SSAP, 8 bit Control (unnumbered) */
#define DOT11_LLC_HDR_LEN_MIN		3
#define DOT11_OUI_LEN			3	/* d11 OUI length */
BWL_PRE_PACKED_STRUCT struct dot11_llc_snap_header {
	uint8	dsap;				/* always 0xAA */
	uint8	ssap;				/* always 0xAA */
	uint8	ctl;				/* always 0x03 */
	uint8	oui[DOT11_OUI_LEN];		/* RFC1042: 0x00 0x00 0x00
						 * Bridge-Tunnel: 0x00 0x00 0xF8
						 */
	uint16	type;				/* ethertype */
} BWL_POST_PACKED_STRUCT;

/* RFC1042 header used by 802.11 per 802.1H */
#define RFC1042_HDR_LEN	(ETHER_HDR_LEN + DOT11_LLC_SNAP_HDR_LEN)	/* RCF1042 header length */

#define SFH_LLC_SNAP_SZ	(RFC1042_HDR_LEN)

#define COPY_SFH_LLCSNAP(dst, src) \
	do { \
		*((uint32 *)dst + 0) = *((uint32 *)src + 0); \
		*((uint32 *)dst + 1) = *((uint32 *)src + 1); \
		*((uint32 *)dst + 2) = *((uint32 *)src + 2); \
		*((uint32 *)dst + 3) = *((uint32 *)src + 3); \
		*((uint32 *)dst + 4) = *((uint32 *)src + 4); \
		*(uint16 *)((uint32 *)dst + 5) = *(uint16 *)((uint32 *)src + 5); \
	} while (0)

/* Generic 802.11 MAC header */
/**
 * N.B.: This struct reflects the full 4 address 802.11 MAC header.
 *		 The fields are defined such that the shorter 1, 2, and 3
 *		 address headers just use the first k fields.
 */
BWL_PRE_PACKED_STRUCT struct dot11_header {
	uint16			fc;		/* frame control */
	uint16			durid;		/* duration/ID */
	struct ether_addr	a1;		/* address 1 */
	struct ether_addr	a2;		/* address 2 */
	struct ether_addr	a3;		/* address 3 */
	uint16			seq;		/* sequence control */
	struct ether_addr	a4;		/* address 4 */
} BWL_POST_PACKED_STRUCT;

/* Control frames */

BWL_PRE_PACKED_STRUCT struct dot11_rts_frame {
	uint16			fc;		/* frame control */
	uint16			durid;		/* duration/ID */
	struct ether_addr	ra;		/* receiver address */
	struct ether_addr	ta;		/* transmitter address */
} BWL_POST_PACKED_STRUCT;
#define	DOT11_RTS_LEN		16		/* d11 RTS frame length */

BWL_PRE_PACKED_STRUCT struct dot11_cts_frame {
	uint16			fc;		/* frame control */
	uint16			durid;		/* duration/ID */
	struct ether_addr	ra;		/* receiver address */
} BWL_POST_PACKED_STRUCT;
#define	DOT11_CTS_LEN		10u		/* d11 CTS frame length */

BWL_PRE_PACKED_STRUCT struct dot11_ack_frame {
	uint16			fc;		/* frame control */
	uint16			durid;		/* duration/ID */
	struct ether_addr	ra;		/* receiver address */
} BWL_POST_PACKED_STRUCT;
#define	DOT11_ACK_LEN		10		/* d11 ACK frame length */

BWL_PRE_PACKED_STRUCT struct dot11_ps_poll_frame {
	uint16			fc;		/* frame control */
	uint16			durid;		/* AID */
	struct ether_addr	bssid;		/* receiver address, STA in AP */
	struct ether_addr	ta;		/* transmitter address */
} BWL_POST_PACKED_STRUCT;
#define	DOT11_PS_POLL_LEN	16		/* d11 PS poll frame length */

BWL_PRE_PACKED_STRUCT struct dot11_cf_end_frame {
	uint16			fc;		/* frame control */
	uint16			durid;		/* duration/ID */
	struct ether_addr	ra;		/* receiver address */
	struct ether_addr	bssid;		/* transmitter address, STA in AP */
} BWL_POST_PACKED_STRUCT;
#define	DOT11_CS_END_LEN	16		/* d11 CF-END frame length */

/** generic vendor specific action frame with variable length */
BWL_PRE_PACKED_STRUCT struct dot11_action_vs_frmhdr {
	uint8	category;
	uint8	OUI[3];
	uint8	type;
	uint8	subtype;
	uint8	data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_action_vs_frmhdr dot11_action_vs_frmhdr_t;

#define DOT11_ACTION_VS_HDR_LEN	6

/* BA/BAR Control parameters */
#define DOT11_BA_CTL_POLICY_NORMAL	0x0000	/* normal ack */
#define DOT11_BA_CTL_POLICY_NOACK	0x0001	/* no ack */
#define DOT11_BA_CTL_POLICY_MASK	0x0001	/* ack policy mask */

#define DOT11_BA_CTL_MTID		0x0002	/* multi tid BA */
#define DOT11_BA_CTL_COMPRESSED		0x0004	/* compressed bitmap */
#define DOT11_BA_CTL_GCR		0x0008	/* GCR BAR */

#define DOT11_BA_CTL_NUMMSDU_MASK	0x0FC0	/* num msdu in bitmap mask */
#define DOT11_BA_CTL_NUMMSDU_SHIFT	6	/* num msdu in bitmap shift */

#define DOT11_BA_CTL_TID_MASK		0xF000	/* tid mask */
#define DOT11_BA_CTL_TID_SHIFT		12	/* tid shift */

/** control frame header (BA/BAR) */
BWL_PRE_PACKED_STRUCT struct dot11_ctl_header {
	uint16			fc;		/* frame control */
	uint16			durid;		/* duration/ID */
	struct ether_addr	ra;		/* receiver address */
	struct ether_addr	ta;		/* transmitter address */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_ctl_header dot11_ctl_header_t;
#define DOT11_CTL_HDR_LEN	16		/* control frame hdr len */

/** BAR frame payload */
BWL_PRE_PACKED_STRUCT struct dot11_bar {
	uint16			bar_control;	/* BAR Control */
	uint16			seqnum;		/* Starting Sequence control */
} BWL_POST_PACKED_STRUCT;
#define DOT11_BAR_LEN		4		/* BAR frame payload length */

/** GCR BAR frame payload */
BWL_PRE_PACKED_STRUCT struct dot11_gcr_bar {
	uint16				bar_control;	/* BAR Control */
	uint16				seqnum;		/* Starting Sequence control */
	struct ether_addr		gcr;		/* GCR address */
} BWL_POST_PACKED_STRUCT;
#define DOT11_GCR_BAR_LEN		10u		/* GCR BAR frame payload length */

#define DOT11_BA_BITMAP_LEN	128		/* bitmap length */
#define DOT11_BA_CMP_BITMAP_LEN	8		/* compressed bitmap length */
/** BA frame payload */
BWL_PRE_PACKED_STRUCT struct dot11_ba {
	uint16			ba_control;	/* BA Control */
	uint16			seqnum;		/* Starting Sequence control */
	uint8			bitmap[DOT11_BA_BITMAP_LEN];	/* Block Ack Bitmap */
} BWL_POST_PACKED_STRUCT;
#define DOT11_BA_LEN		4		/* BA frame payload len (wo bitmap) */

/** Management frame header */
BWL_PRE_PACKED_STRUCT struct dot11_management_header {
	uint16			fc;		/* frame control */
	uint16			durid;		/* duration/ID */
	struct ether_addr	da;		/* receiver address */
	struct ether_addr	sa;		/* transmitter address */
	struct ether_addr	bssid;		/* BSS ID */
	uint16			seq;		/* sequence control */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_management_header dot11_management_header_t;
#define	DOT11_MGMT_HDR_LEN	24u		/* d11 management header length */

/* Management frame payloads */

BWL_PRE_PACKED_STRUCT struct dot11_bcn_prb {
	uint32			timestamp[2];
	uint16			beacon_interval;
	uint16			capability;
	uint8			ies[];
} BWL_POST_PACKED_STRUCT;
#define	DOT11_BCN_PRB_LEN	12		/* 802.11 beacon/probe frame fixed length */
#define	DOT11_BCN_PRB_FIXED_LEN	12u		/* 802.11 beacon/probe frame fixed length */

BWL_PRE_PACKED_STRUCT struct dot11_auth {
	uint16			alg;		/* algorithm */
	uint16			seq;		/* sequence control */
	uint16			status;		/* status code */
} BWL_POST_PACKED_STRUCT;
#define DOT11_AUTH_FIXED_LEN		6	/* length of auth frame without challenge IE */
#define DOT11_AUTH_SEQ_STATUS_LEN	4	/* length of auth frame without challenge IE and
						 * without algorithm
						 */

BWL_PRE_PACKED_STRUCT struct dot11_assoc_req {
	uint16			capability;	/* capability information */
	uint16			listen;		/* listen interval */
} BWL_POST_PACKED_STRUCT;
#define DOT11_ASSOC_REQ_FIXED_LEN	4	/* length of assoc frame without info elts */

BWL_PRE_PACKED_STRUCT struct dot11_reassoc_req {
	uint16			capability;	/* capability information */
	uint16			listen;		/* listen interval */
	struct ether_addr	ap;		/* Current AP address */
} BWL_POST_PACKED_STRUCT;
#define DOT11_REASSOC_REQ_FIXED_LEN	10	/* length of assoc frame without info elts */

BWL_PRE_PACKED_STRUCT struct dot11_assoc_resp {
	uint16			capability;	/* capability information */
	uint16			status;		/* status code */
	uint16			aid;		/* association ID */
} BWL_POST_PACKED_STRUCT;
#define DOT11_ASSOC_RESP_FIXED_LEN	6	/* length of assoc resp frame without info elts */

BWL_PRE_PACKED_STRUCT struct dot11_action_measure {
	uint8	category;
	uint8	action;
	uint8	token;
	uint8	data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
#define DOT11_ACTION_MEASURE_LEN	3	/* d11 action measurement header length */

BWL_PRE_PACKED_STRUCT struct dot11_action_sa_query {
	uint8	category;
	uint8	action;
	uint16	id;
} BWL_POST_PACKED_STRUCT;

#define SM_PWRSAVE_ENABLE	1
#define SM_PWRSAVE_MODE		2

/* ************* 802.11h related definitions. ************* */
BWL_PRE_PACKED_STRUCT struct dot11_power_cnst {
	uint8 id;
	uint8 len;
	uint8 power;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_power_cnst dot11_power_cnst_t;

BWL_PRE_PACKED_STRUCT struct dot11_power_cap {
	int8 min;
	int8 max;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_power_cap dot11_power_cap_t;

BWL_PRE_PACKED_STRUCT struct dot11_tpc_rep {
	uint8 id;
	uint8 len;
	uint8 tx_pwr;
	uint8 margin;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tpc_rep dot11_tpc_rep_t;
#define DOT11_MNG_IE_TPC_REPORT_SIZE	(sizeof(dot11_tpc_rep_t))
#define DOT11_MNG_IE_TPC_REPORT_LEN	2	/* length of IE data, not including 2 byte header */

BWL_PRE_PACKED_STRUCT struct dot11_supp_channels {
	uint8 id;
	uint8 len;
	uint8 first_channel;
	uint8 num_channels;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_supp_channels dot11_supp_channels_t;

BWL_PRE_PACKED_STRUCT struct dot11_action_frmhdr {
	uint8	category;
	uint8	action;
	uint8	data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_action_frmhdr dot11_action_frmhdr_t;

/* Action Field length */
#define DOT11_ACTION_CATEGORY_LEN	1u
#define DOT11_ACTION_ACTION_LEN		1u
#define DOT11_ACTION_DIALOG_TOKEN_LEN	1u
#define DOT11_ACTION_CAPABILITY_LEN	2u
#define DOT11_ACTION_STATUS_CODE_LEN	2u
#define DOT11_ACTION_REASON_CODE_LEN	2u
#define DOT11_ACTION_TARGET_CH_LEN	1u
#define DOT11_ACTION_OPER_CLASS_LEN	1u

#define DOT11_ACTION_FRMHDR_LEN	2

/** CSA IE data structure */
BWL_PRE_PACKED_STRUCT struct dot11_channel_switch {
	uint8 id;	/* id DOT11_MNG_CHANNEL_SWITCH_ID */
	uint8 len;	/* length of IE */
	uint8 mode;	/* mode 0 or 1 */
	uint8 channel;	/* channel switch to */
	uint8 count;	/* number of beacons before switching */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_channel_switch dot11_chan_switch_ie_t;

#define DOT11_SWITCH_IE_LEN	3	/* length of IE data, not including 2 byte header */
/* CSA mode - 802.11h-2003 $7.3.2.20 */
#define DOT11_CSA_MODE_ADVISORY		0	/* no DOT11_CSA_MODE_NO_TX restriction imposed */
#define DOT11_CSA_MODE_NO_TX		1	/* no transmission upon receiving CSA frame. */

BWL_PRE_PACKED_STRUCT struct dot11_csa_body {
	uint8 mode;	/* mode 0 or 1 */
	uint8 reg;	/* regulatory class */
	uint8 channel;	/* channel switch to */
	uint8 count;	/* number of beacons before switching */
} BWL_POST_PACKED_STRUCT;

BWL_PRE_PACKED_STRUCT struct dot11y_action_ext_csa {
	uint8	category;
	uint8	action;
	struct dot11_csa_body b;	/* body of the ie */
} BWL_POST_PACKED_STRUCT;

/* TPE Transmit Power Information Field */
#define DOT11_TPE_INFO_MAX_TX_PWR_CNT_MASK               0x07u
#define DOT11_TPE_INFO_MAX_TX_PWR_INTRPN_MASK            0x38u
#define DOT11_TPE_INFO_MAX_TX_PWR_INTRPN_SHIFT           3u
#define DOT11_TPE_INFO_MAX_TX_PWR_CAT_MASK               0xC0u
#define DOT11_TPE_INFO_MAX_TX_PWR_CAT_SHIFT              6u

/* TPE Transmit Power Information Field Accessor */
#define DOT11_TPE_INFO_MAX_TX_PWR_CNT(x) \
	(x & DOT11_TPE_INFO_MAX_TX_PWR_CNT_MASK)
#define DOT11_TPE_INFO_MAX_TX_PWR_INTRPN(x) \
	(((x) & DOT11_TPE_INFO_MAX_TX_PWR_INTRPN_MASK) >> \
	DOT11_TPE_INFO_MAX_TX_PWR_INTRPN_SHIFT)
#define DOT11_TPE_INFO_MAX_TX_PWR_CAT(x) \
	(((x) & DOT11_TPE_INFO_MAX_TX_PWR_CAT_MASK) >> \
	DOT11_TPE_INFO_MAX_TX_PWR_CAT_SHIFT)

/* Maximum Transmit Power Interpretation subfield */
#define DOT11_TPE_MAX_TX_PWR_INTRPN_LOCAL_EIRP			0u
#define DOT11_TPE_MAX_TX_PWR_INTRPN_LOCAL_EIRP_PSD		1u
#define DOT11_TPE_MAX_TX_PWR_INTRPN_REG_CLIENT_EIRP		2u
#define DOT11_TPE_MAX_TX_PWR_INTRPN_REG_CLIENT_EIRP_PSD		3u
#define DOT11_TPE_MAX_TX_PWR_INTRPN_MAX				4u

/* Maximum Transmit Power category subfield  */
#define DOT11_TPE_MAX_TX_PWR_CAT_DEFAULT			0u

/* Maximum Transmit Power category subfield in US */
#define DOT11_TPE_MAX_TX_PWR_CAT_US_DEFAULT			0u
#define DOT11_TPE_MAX_TX_PWR_CAT_US_SUB_DEV			1u
#define DOT11_TPE_MAX_TX_PWR_CAT_MAX				2u

/* Maximum Transmit Power Count subfield values when
 * Maximum Transmit Power Interpretation subfield is 0 or 2
 */
#define DOT11_TPE_INFO_MAX_TX_CNT_EIRP_20_MHZ			0u
#define DOT11_TPE_INFO_MAX_TX_CNT_EIRP_20_40_MHZ		1u
#define DOT11_TPE_INFO_MAX_TX_CNT_EIRP_20_40_80_MHZ		2u
#define DOT11_TPE_INFO_MAX_TX_CNT_EIRP_20_40_80_160_MHZ		3u
#define DOT11_TPE_INFO_MAX_TX_PWR_CNT_MAX			7u

/* Maximum Transmit Power Count subfield values when
 * Maximum Transmit Power Interpretation subfield is 1 or 3
 */
#define DOT11_TPE_INFO_MAX_TX_CNT_PSD_VAL_0                 0u
#define DOT11_TPE_INFO_MAX_TX_CNT_PSD_VAL_1                 1u
#define DOT11_TPE_INFO_MAX_TX_CNT_PSD_VAL_2                 2u
#define DOT11_TPE_INFO_MAX_TX_CNT_PSD_VAL_3                 4u
#define DOT11_TPE_INFO_MAX_TX_CNT_PSD_VAL_4                 8u

#define DOT11_TPE_MAX_TX_PWR_EIRP_MIN                    -128 /* 0.5 db step */
#define DOT11_TPE_MAX_TX_PWR_EIRP_MAX                     126  /* 0.5 db step */
#define DOT11_TPE_MAX_TX_PWR_EIRP_NO_LIMIT                127  /* 0.5 db step */

#define DOT11_TPE_MAX_TX_PWR_PSD_BLOCKED                 -128
#define DOT11_TPE_MAX_TX_PWR_PSD_NO_LIMIT                 127u
/** Transmit Power Envelope IE data structure as per 11ax draft */
BWL_PRE_PACKED_STRUCT struct dot11_transmit_power_envelope {
	uint8 id;				/* id DOT11_MNG_WIDE_BW_CHANNEL_SWITCH_ID */
	uint8 len;				/* length of IE */
	uint8 transmit_power_info;
	uint8 max_transmit_power[]; /* Variable length */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_transmit_power_envelope dot11_transmit_power_envelope_ie_t;
/* id (1) + len (1) + transmit_power_info(1) + max_transmit_power(1) */
#define DOT11_TPE_ELEM_MIN_LEN  4u

BWL_PRE_PACKED_STRUCT struct dot11_extcap_ie {
	uint8 id;
	uint8 len;
	uint8 cap[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_extcap_ie dot11_extcap_ie_t;

#define DOT11_EXTCAP_LEN_COEX	1
#define DOT11_EXTCAP_LEN_BT	3
#define DOT11_EXTCAP_LEN_IW	4
#define DOT11_EXTCAP_LEN_SI	6

#define DOT11_EXTCAP_LEN_OPMODE_NOTIFICATION	8
#define DOT11_EXTCAP_LEN_TWT			10u
#define DOT11_EXTCAP_LEN_BCN_PROT		11u

/* Extended RSN capabilities (RSNXE capabilities) */
#define DOT11_EXT_RSN_CAP_LEN_MASK		0x000fu	/* Field length mask */
#define DOT11_EXT_RSN_CAP_SAE_H2E		5u	/* SAE Hash-to-element support */
#define DOT11_CAP_SAE_HASH_TO_ELEMENT		DOT11_EXT_RSN_CAP_SAE_H2E /* duped. to be removed */
#define DOT11_EXT_RSN_CAP_SAE_PK		6u	/* SAE-PK support */
/* Draft P802.11az/D2.5 - Table 9-321 Extended RSN Capabilities field */
#define DOT11_EXT_RSN_CAP_SECURE_LTF		8u	/* Secure LTF support */
#define DOT11_EXT_RSN_CAP_SECURE_RTT		9u	/* Secure RTT(EDMG measurement) support */
/* Protection of Ranging management frame is required */
#define DOT11_EXT_RSN_CAP_URNM_MFPR_X20		10u	/* except BW20 ranging */
#define DOT11_EXT_RSN_CAP_RANGE_PMF_REQUIRED	DOT11_EXT_RSN_CAP_URNM_MFPR_X20 /* to be removed */
#define DOT11_EXT_RSN_CAP_URNM_MFPR		15u	/* P802.11az/D4.1 */

/* Last bit in Extended RSN Capabilities defined in P802.11
 * Please update DOT11_EXT_RSN_CAP_LAST_BIT_IDX to the last bit when P802.11 inteoduced new bit
 */
#define DOT11_EXT_RSN_CAP_LAST_BIT_IDX	DOT11_EXT_RSN_CAP_URNM_MFPR /* update this */
#define DOT11_EXT_RSN_CAP_NUM_BITS_MAX	(DOT11_EXT_RSN_CAP_LAST_BIT_IDX + 1) /* last bit idx + 1 */

/* Please use DOT11_EXT_RSN_CAP_BYTE_LEN_MAX for any of RSNXE cap buffer size ONLY in ATTACH time.
 * Othewise, change of DOT11_EXT_RSN_CAP_BYTE_LEN_MAX cause ROM invalidation.
 * DOT11_EXT_RSN_CAP_BYTE_LEN_MAX can grow with 802.11 change.
 */
#define DOT11_EXT_RSN_CAP_BYTE_LEN_MAX	((DOT11_EXT_RSN_CAP_NUM_BITS_MAX + 7) >> 3) /* byte len */

BWL_PRE_PACKED_STRUCT struct dot11_rsnxe {
	uint8 id;	/* id DOT11_MNG_RSNXE_ID */
	uint8 len;
	uint8 cap[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rsnxe dot11_rsnxe_t;

#define RSNXE_CAP_LENGTH_MASK		(0x0f)
#define RSNXE_CAP_LENGTH(cap)		((uint8)(cap) & RSNXE_CAP_LENGTH_MASK)
#define RSNXE_SET_CAP_LENGTH(cap, len)\
		(cap = (cap & ~RSNXE_CAP_LENGTH_MASK) | ((uint8)(len) & RSNXE_CAP_LENGTH_MASK))
/* The first 4 bit of CAPs field is the length of the Extended RSN Capabilities minus 1 */
#define IS_RSNXE_VALID(ie)	((ie) && (((dot11_rsnxe_t *)(ie))->len >= 1u) && \
					(((dot11_rsnxe_t *)(ie))->len > \
					RSNXE_CAP_LENGTH(((dot11_rsnxe_t *)(ie))->cap[0])))

BWL_PRE_PACKED_STRUCT struct dot11_rejected_groups_ie {
	uint8 id;	/* DOT11_MNG_EXT_ID */
	uint8 len;
	uint8 id_ext; /* DOT11_MNG_REJECTED_GROUPS_ID */
	uint16 groups[];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rejected_groups_ie dot11_rejected_groups_ie_t;

/* 802.11h/802.11k Measurement Request/Report IEs */
/* Measurement Type field */
#define DOT11_MEASURE_TYPE_BASIC	0   /* d11 measurement basic type */
#define DOT11_MEASURE_TYPE_CCA		1   /* d11 measurement CCA type */
#define DOT11_MEASURE_TYPE_RPI		2   /* d11 measurement RPI type */
#define DOT11_MEASURE_TYPE_CHLOAD	3   /* d11 measurement Channel Load type */
#define DOT11_MEASURE_TYPE_NOISE	4   /* d11 measurement Noise Histogram type */
#define DOT11_MEASURE_TYPE_BEACON	5   /* d11 measurement Beacon type */
#define DOT11_MEASURE_TYPE_FRAME	6   /* d11 measurement Frame type */
#define DOT11_MEASURE_TYPE_STAT		7   /* d11 measurement STA Statistics type */
#define DOT11_MEASURE_TYPE_LCI		8   /* d11 measurement LCI type */
#define DOT11_MEASURE_TYPE_TXSTREAM	9   /* d11 measurement TX Stream type */
#define DOT11_MEASURE_TYPE_MCDIAGS	10  /* d11 measurement multicast diagnostics */
#define DOT11_MEASURE_TYPE_CIVICLOC	11  /* d11 measurement location civic */
#define DOT11_MEASURE_TYPE_LOC_ID	12  /* d11 measurement location identifier */
#define DOT11_MEASURE_TYPE_DIRCHANQ	13  /* d11 measurement dir channel quality */
#define DOT11_MEASURE_TYPE_DIRMEAS	14  /* d11 measurement directional */
#define DOT11_MEASURE_TYPE_DIRSTATS	15  /* d11 measurement directional stats */
#define DOT11_MEASURE_TYPE_FTMRANGE	16  /* d11 measurement Fine Timing */
#define DOT11_MEASURE_TYPE_PAUSE	255	/* d11 measurement pause type */

/* Measurement Request Modes */
#define DOT11_MEASURE_MODE_PARALLEL	(1<<0)	/* d11 measurement parallel */
#define DOT11_MEASURE_MODE_ENABLE	(1<<1)	/* d11 measurement enable */
#define DOT11_MEASURE_MODE_REQUEST	(1<<2)	/* d11 measurement request */
#define DOT11_MEASURE_MODE_REPORT	(1<<3)	/* d11 measurement report */
#define DOT11_MEASURE_MODE_DUR		(1<<4)	/* d11 measurement dur mandatory */
/* Measurement Report Modes */
#define DOT11_MEASURE_MODE_LATE		(1<<0)	/* d11 measurement late */
#define DOT11_MEASURE_MODE_INCAPABLE	(1<<1)	/* d11 measurement incapable */
#define DOT11_MEASURE_MODE_REFUSED	(1<<2)	/* d11 measurement refuse */
/* Basic Measurement Map bits */
#define DOT11_MEASURE_BASIC_MAP_BSS	((uint8)(1<<0))	/* d11 measurement basic map BSS */
#define DOT11_MEASURE_BASIC_MAP_OFDM	((uint8)(1<<1))	/* d11 measurement map OFDM */
#define DOT11_MEASURE_BASIC_MAP_UKNOWN	((uint8)(1<<2))	/* d11 measurement map unknown */
#define DOT11_MEASURE_BASIC_MAP_RADAR	((uint8)(1<<3))	/* d11 measurement map radar */
#define DOT11_MEASURE_BASIC_MAP_UNMEAS	((uint8)(1<<4))	/* d11 measurement map unmeasuremnt */

BWL_PRE_PACKED_STRUCT struct dot11_meas_req {
	uint8 id;
	uint8 len;
	uint8 token;
	uint8 mode;
	uint8 type;
	uint8 channel;
	uint8 start_time[8];
	uint16 duration;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_meas_req dot11_meas_req_t;
#define DOT11_MNG_IE_MREQ_LEN 14	/* d11 measurement request IE length */
/* length of Measure Request IE data not including variable len */
#define DOT11_MNG_IE_MREQ_FIXED_LEN 3	/* d11 measurement request IE fixed length */

BWL_PRE_PACKED_STRUCT struct dot11_meas_req_loc {
	uint8 id;
	uint8 len;
	uint8 token;
	uint8 mode;
	uint8 type;
	BWL_PRE_PACKED_STRUCT union
	{
		BWL_PRE_PACKED_STRUCT struct {
			uint8 subject;
			uint8 data[BCM_FLEX_ARRAY];
		} BWL_POST_PACKED_STRUCT lci;
		BWL_PRE_PACKED_STRUCT struct {
			uint8 subject;
			uint8 type;  /* type of civic location */
			uint8 siu;   /* service interval units */
			uint16 si; /* service interval */
			uint8 data[BCM_FLEX_ARRAY];
		} BWL_POST_PACKED_STRUCT civic;
		BWL_PRE_PACKED_STRUCT struct {
			uint8 subject;
			uint8 siu;   /* service interval units */
			uint16 si; /* service interval */
			uint8 data[BCM_FLEX_ARRAY];
		} BWL_POST_PACKED_STRUCT locid;
		BWL_PRE_PACKED_STRUCT struct {
			uint16 max_init_delay;		/* maximum random initial delay */
			uint8 min_ap_count;
			uint8 data[BCM_FLEX_ARRAY];
		} BWL_POST_PACKED_STRUCT ftm_range;
	} BWL_POST_PACKED_STRUCT req;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_meas_req_loc dot11_meas_req_loc_t;
#define DOT11_MNG_IE_MREQ_MIN_LEN           4	/* d11 measurement report IE length */
#define DOT11_MNG_IE_MREQ_LCI_FIXED_LEN     4	/* d11 measurement report IE length */
#define DOT11_MNG_IE_MREQ_CIVIC_FIXED_LEN   8	/* d11 measurement report IE length */
#define DOT11_MNG_IE_MREQ_FRNG_FIXED_LEN    6	/* d11 measurement report IE length */

BWL_PRE_PACKED_STRUCT struct dot11_lci_subelement {
	uint8 subelement;
	uint8 length;
	uint8 lci_data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_lci_subelement dot11_lci_subelement_t;

BWL_PRE_PACKED_STRUCT struct dot11_colocated_bssid_list_se {
	uint8 sub_id;
	uint8 length;
	uint8 max_bssid_ind; /* MaxBSSID Indicator */
	struct ether_addr bssid[BCM_FLEX_ARRAY]; /* variable */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_colocated_bssid_list_se dot11_colocated_bssid_list_se_t;
#define DOT11_LCI_COLOCATED_BSSID_LIST_FIXED_LEN     3
#define DOT11_LCI_COLOCATED_BSSID_SUBELEM_ID         7

BWL_PRE_PACKED_STRUCT struct dot11_civic_subelement {
	uint8 type;  /* type of civic location */
	uint8 subelement;
	uint8 length;
	uint8 civic_data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_civic_subelement dot11_civic_subelement_t;

BWL_PRE_PACKED_STRUCT struct dot11_meas_rep {
	uint8 id;
	uint8 len;
	uint8 token;
	uint8 mode;
	uint8 type;
	BWL_PRE_PACKED_STRUCT union
	{
		BWL_PRE_PACKED_STRUCT struct {
			uint8 channel;
			uint8 start_time[8];
			uint16 duration;
			uint8 map;
		} BWL_POST_PACKED_STRUCT basic;
		BWL_PRE_PACKED_STRUCT struct {
			uint8 subelement;
			uint8 length;
			uint8 data[1];
		} BWL_POST_PACKED_STRUCT lci;
		BWL_PRE_PACKED_STRUCT struct {
			uint8 type;  /* type of civic location */
			uint8 subelement;
			uint8 length;
			uint8 data[1];
		} BWL_POST_PACKED_STRUCT civic;
		BWL_PRE_PACKED_STRUCT struct {
			uint8 exp_tsf[8];
			uint8 subelement;
			uint8 length;
			uint8 data[1];
		} BWL_POST_PACKED_STRUCT locid;
		BWL_PRE_PACKED_STRUCT struct {
			uint8 entry_count;
			uint8 data[1];
		} BWL_POST_PACKED_STRUCT ftm_range;
		/* Flexible arrays like data[] not allowed in unions */
		uint8 data[1];
	} BWL_POST_PACKED_STRUCT rep;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_meas_rep dot11_meas_rep_t;
#define DOT11_MNG_IE_MREP_MIN_LEN           5	/* d11 measurement report IE length */
#define DOT11_MNG_IE_MREP_LCI_FIXED_LEN     5	/* d11 measurement report IE length */
#define DOT11_MNG_IE_MREP_CIVIC_FIXED_LEN   6	/* d11 measurement report IE length */
#define DOT11_MNG_IE_MREP_LOCID_FIXED_LEN   13	/* d11 measurement report IE length */
#define DOT11_MNG_IE_MREP_BASIC_FIXED_LEN   15	/* d11 measurement report IE length */
#define DOT11_MNG_IE_MREP_FRNG_FIXED_LEN    4

/* length of Measure Report IE data not including variable len */
#define DOT11_MNG_IE_MREP_FIXED_LEN	3	/* d11 measurement response IE fixed length */

BWL_PRE_PACKED_STRUCT struct dot11_meas_rep_basic {
	uint8 channel;
	uint8 start_time[8];
	uint16 duration;
	uint8 map;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_meas_rep_basic dot11_meas_rep_basic_t;
#define DOT11_MEASURE_BASIC_REP_LEN	12	/* d11 measurement basic report length */

BWL_PRE_PACKED_STRUCT struct dot11_quiet {
	uint8 id;
	uint8 len;
	uint8 count;	/* TBTTs until beacon interval in quiet starts */
	uint8 period;	/* Beacon intervals between periodic quiet periods ? */
	uint16 duration;	/* Length of quiet period, in TU's */
	uint16 offset;	/* TU's offset from TBTT in Count field */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_quiet dot11_quiet_t;

BWL_PRE_PACKED_STRUCT struct chan_map_tuple {
	uint8 channel;
	uint8 map;
} BWL_POST_PACKED_STRUCT;
typedef struct chan_map_tuple chan_map_tuple_t;

BWL_PRE_PACKED_STRUCT struct dot11_ibss_dfs {
	uint8 id;
	uint8 len;
	uint8 eaddr[ETHER_ADDR_LEN];
	uint8 interval;
	chan_map_tuple_t map[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_ibss_dfs dot11_ibss_dfs_t;

/* WME Access Category Indices (ACIs) */
#define AC_BE			0	/* Best Effort */
#define AC_BK			1	/* Background */
#define AC_VI			2	/* Video */
#define AC_VO			3	/* Voice */
#define AC_COUNT		4	/* number of ACs */

typedef uint8 ac_bitmap_t;	/* AC bitmap of (1 << AC_xx) */

#define AC_BITMAP_NONE		0x0	/* No ACs */
#define AC_BITMAP_ALL		0xf	/* All ACs */
#define AC_BITMAP_TST(ab, ac)	(((ab) & (1 << (ac))) != 0)
#define AC_BITMAP_SET(ab, ac)	(((ab) |= (1 << (ac))))
#define AC_BITMAP_RESET(ab, ac) (((ab) &= ~(1 << (ac))))

/* Management PKT Lifetime indices */
#define MGMT_ALL		0xffff
#define MGMT_AUTH_LT	FC_SUBTYPE_AUTH
#define MGMT_ASSOC_LT	FC_SUBTYPE_ASSOC_REQ

BWL_PRE_PACKED_STRUCT struct edcf_acparam {
	uint8	ACI;
	uint8	ECW;
	uint16  TXOP;		/* stored in network order (ls octet first) */
} BWL_POST_PACKED_STRUCT;
typedef struct edcf_acparam edcf_acparam_t;

/* ACI */
#define EDCF_AIFSN_MIN               1           /* AIFSN minimum value */
#define EDCF_AIFSN_MAX               15          /* AIFSN maximum value */
#define EDCF_AIFSN_MASK              0x0f        /* AIFSN mask */
#define EDCF_ACM_MASK                0x10        /* ACM mask */
#define EDCF_ACI_MASK                0x60        /* ACI mask */
#define EDCF_ACI_SHIFT               5           /* ACI shift */
#define EDCF_AIFSN_SHIFT             12          /* 4 MSB(0xFFF) in ifs_ctl for AC idx */

/* ECW */
#define EDCF_ECW_MIN                 0           /* cwmin/cwmax exponent minimum value */
#define EDCF_ECW_MAX                 15          /* cwmin/cwmax exponent maximum value */
#define EDCF_ECW2CW(exp)             ((1 << (exp)) - 1)
#define EDCF_ECWMIN_MASK             0x0f        /* cwmin exponent form mask */
#define EDCF_ECWMAX_MASK             0xf0        /* cwmax exponent form mask */
#define EDCF_ECWMAX_SHIFT            4           /* cwmax exponent form shift */

/* TXOP */
#define EDCF_TXOP_MIN                0           /* TXOP minimum value */
#define EDCF_TXOP_MAX                65535       /* TXOP maximum value */
#define EDCF_TXOP2USEC(txop)         ((txop) << 5)

/* Default EDCF parameters that AP advertises for STA to use; WMM draft Table 12 */
#define EDCF_AC_BE_ACI_STA           0x03	/* STA ACI value for best effort AC */
#define EDCF_AC_BE_ECW_STA           0xA4	/* STA ECW value for best effort AC */
#define EDCF_AC_BE_TXOP_STA          0x0000	/* STA TXOP value for best effort AC */
#define EDCF_AC_BK_ACI_STA           0x27	/* STA ACI value for background AC */
#define EDCF_AC_BK_ECW_STA           0xA4	/* STA ECW value for background AC */
#define EDCF_AC_BK_TXOP_STA          0x0000	/* STA TXOP value for background AC */
#define EDCF_AC_VI_ACI_STA           0x42	/* STA ACI value for video AC */
#define EDCF_AC_VI_ECW_STA           0x43	/* STA ECW value for video AC */
#define EDCF_AC_VI_TXOP_STA          0x005e	/* STA TXOP value for video AC */
#define EDCF_AC_VO_ACI_STA           0x62	/* STA ACI value for audio AC */
#define EDCF_AC_VO_ECW_STA           0x32	/* STA ECW value for audio AC */
#define EDCF_AC_VO_TXOP_STA          0x002f	/* STA TXOP value for audio AC */

/* Default EDCF parameters that AP uses; WMM draft Table 14 */
#define EDCF_AC_BE_ACI_AP            0x03	/* AP ACI value for best effort AC */
#define EDCF_AC_BE_ECW_AP            0x64	/* AP ECW value for best effort AC */
#define EDCF_AC_BE_TXOP_AP           0x0000	/* AP TXOP value for best effort AC */
#define EDCF_AC_BK_ACI_AP            0x27	/* AP ACI value for background AC */
#define EDCF_AC_BK_ECW_AP            0xA4	/* AP ECW value for background AC */
#define EDCF_AC_BK_TXOP_AP           0x0000	/* AP TXOP value for background AC */
#define EDCF_AC_VI_ACI_AP            0x41	/* AP ACI value for video AC */
#define EDCF_AC_VI_ECW_AP            0x43	/* AP ECW value for video AC */
#define EDCF_AC_VI_TXOP_AP           0x005e	/* AP TXOP value for video AC */
#define EDCF_AC_VO_ACI_AP            0x61	/* AP ACI value for audio AC */
#define EDCF_AC_VO_ECW_AP            0x32	/* AP ECW value for audio AC */
#define EDCF_AC_VO_TXOP_AP           0x002f	/* AP TXOP value for audio AC */

/** EDCA Parameter IE */
BWL_PRE_PACKED_STRUCT struct edca_param_ie {
	uint8 qosinfo;
	uint8 rsvd;
	edcf_acparam_t acparam[AC_COUNT];
} BWL_POST_PACKED_STRUCT;
typedef struct edca_param_ie edca_param_ie_t;
#define EDCA_PARAM_IE_LEN            18          /* EDCA Parameter IE length */

/** QoS Capability IE */
BWL_PRE_PACKED_STRUCT struct qos_cap_ie {
	uint8 qosinfo;
} BWL_POST_PACKED_STRUCT;
typedef struct qos_cap_ie qos_cap_ie_t;

BWL_PRE_PACKED_STRUCT struct dot11_qbss_load_ie {
	uint8 id;			/* 11, DOT11_MNG_QBSS_LOAD_ID */
	uint8 length;
	uint16 station_count;		/* total number of STAs associated */
	uint8 channel_utilization;	/* % of time, normalized to 255, QAP sensed medium busy */
	uint16 aac;			/* available admission capacity */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_qbss_load_ie dot11_qbss_load_ie_t;
#define BSS_LOAD_IE_SIZE	7	/* BSS load IE size */

#define WLC_QBSS_LOAD_CHAN_FREE_MAX	0xff	/* max for channel free score */

/* Estimated Service Parameters (ESP) IE - 802.11-2016 9.4.2.174 */
typedef BWL_PRE_PACKED_STRUCT struct dot11_esp_ie {
	uint8		id;
	uint8		length;
	uint8		id_ext;
	/* variable len info */
	uint8		esp_info_lists[];
} BWL_POST_PACKED_STRUCT dot11_esp_ie_t;

#define DOT11_ESP_IE_HDR_SIZE	(OFFSETOF(dot11_esp_ie_t, esp_info_lists))

/* ESP Information list - 802.11-2016 9.4.2.174 */
typedef BWL_PRE_PACKED_STRUCT struct dot11_esp_ie_info_list {
	/* acess category, data format, ba win size */
	uint8		ac_df_baws;
	/* estimated air time fraction */
	uint8		eat_frac;
	/* data PPDU duration target (50us units) */
	uint8		ppdu_dur;
} BWL_POST_PACKED_STRUCT dot11_esp_ie_info_list_t;

#define DOT11_ESP_IE_INFO_LIST_SIZE	(sizeof(dot11_esp_ie_info_list_t))

#define DOT11_ESP_NBR_INFO_LISTS	4u	/* max nbr of esp information lists */
#define DOT11_ESP_INFO_LIST_AC_BK	0u	/* access category of esp information list AC_BK */
#define DOT11_ESP_INFO_LIST_AC_BE	1u	/* access category of esp information list AC_BE */
#define DOT11_ESP_INFO_LIST_AC_VI	2u	/* access category of esp information list AC_VI */
#define DOT11_ESP_INFO_LIST_AC_VO	3u	/* access category of esp information list AC_VO */

#define DOT11_ESP_INFO_LIST_DF_MASK    0x18		/* Data Format Mask */
#define DOT11_ESP_INFO_LIST_BAWS_MASK  0xE0		/* BA window size mask */

/* nom_msdu_size */
#define FIXED_MSDU_SIZE 0x8000		/* MSDU size is fixed */
#define MSDU_SIZE_MASK	0x7fff		/* (Nominal or fixed) MSDU size */

/* surplus_bandwidth */
/* Represented as 3 bits of integer, binary point, 13 bits fraction */
#define	INTEGER_SHIFT	13	/* integer shift */
#define FRACTION_MASK	0x1FFF	/* fraction mask */

/** Management Notification Frame */
BWL_PRE_PACKED_STRUCT struct dot11_management_notification {
	uint8 category;			/* DOT11_ACTION_NOTIFICATION */
	uint8 action;
	uint8 token;
	uint8 status;
	uint8 data[BCM_FLEX_ARRAY];	/* Elements */
} BWL_POST_PACKED_STRUCT;
#define DOT11_MGMT_NOTIFICATION_LEN 4	/* Fixed length */

/* Authentication frame payload constants */
#define DOT11_OPEN_SYSTEM	0	/* d11 open authentication */
#define DOT11_SHARED_KEY	1	/* d11 shared authentication */
#define DOT11_FAST_BSS		2	/* d11 fast bss authentication */
#define DOT11_SAE		3	/* d11 simultaneous authentication of equals */
#define DOT11_FILS_SKEY		4	/* d11 fils shared key authentication w/o pfs */
#define DOT11_FILS_SKEY_PFS	5	/* d11 fils shared key authentication w/ pfs */
#define DOT11_FILS_PKEY		6	/* d11 fils public key authentication */
#define DOT11_PASN		7	/* Pre association security negotiation */
#define DOT11_MAX_AUTH_ALG  DOT11_PASN /* maximum value of an auth alg */
#define DOT11_CHALLENGE_LEN	128	/* d11 challenge text length */

/* Frame control macros */
#define FC_PVER_MASK		0x3	/* PVER mask */
#define FC_PVER_SHIFT		0	/* PVER shift */
#define FC_TYPE_MASK		0xC	/* type mask */
#define FC_TYPE_SHIFT		2	/* type shift */
#define FC_SUBTYPE_MASK		0xF0	/* subtype mask */
#define FC_SUBTYPE_SHIFT	4	/* subtype shift */
#define FC_TODS			0x100	/* to DS */
#define FC_TODS_SHIFT		8	/* to DS shift */
#define FC_FROMDS		0x200	/* from DS */
#define FC_FROMDS_SHIFT		9	/* from DS shift */
#define FC_MOREFRAG		0x400	/* more frag. */
#define FC_MOREFRAG_SHIFT	10	/* more frag. shift */
#define FC_RETRY		0x800	/* retry */
#define FC_RETRY_SHIFT		11	/* retry shift */
#define FC_PM			0x1000	/* PM */
#define FC_PM_SHIFT		12	/* PM shift */
#define FC_MOREDATA		0x2000	/* more data */
#define FC_MOREDATA_SHIFT	13	/* more data shift */
#define FC_WEP			0x4000	/* WEP */
#define FC_WEP_SHIFT		14	/* WEP shift */
#define FC_ORDER		0x8000	/* order */
#define FC_ORDER_SHIFT		15	/* order shift */

/* sequence control macros */
#define SEQNUM_SHIFT		4	/* seq. number shift */
#define SEQNUM_MAX		0x1000	/* max seqnum + 1 */
#define FRAGNUM_MASK		0xF	/* frag. number mask */

/* Frame Control type/subtype defs */

/* FC Types */
#define FC_TYPE_MNG		0	/* management type */
#define FC_TYPE_CTL		1	/* control type */
#define FC_TYPE_DATA		2	/* data type */
#define FC_TYPE_EXT		3	/* extension type */

/* Management Subtypes */
#define FC_SUBTYPE_ASSOC_REQ		0	/* assoc. request */
#define FC_SUBTYPE_ASSOC_RESP		1	/* assoc. response */
#define FC_SUBTYPE_REASSOC_REQ		2	/* reassoc. request */
#define FC_SUBTYPE_REASSOC_RESP		3	/* reassoc. response */
#define FC_SUBTYPE_PROBE_REQ		4	/* probe request */
#define FC_SUBTYPE_PROBE_RESP		5	/* probe response */
#define FC_SUBTYPE_BEACON		8	/* beacon */
#define FC_SUBTYPE_ATIM			9	/* ATIM */
#define FC_SUBTYPE_DISASSOC		10	/* disassoc. */
#define FC_SUBTYPE_AUTH			11	/* authentication */
#define FC_SUBTYPE_DEAUTH		12	/* de-authentication */
#define FC_SUBTYPE_ACTION		13	/* action */
#define FC_SUBTYPE_ACTION_NOACK		14	/* action no-ack */

/* Control Subtypes */
#define FC_SUBTYPE_TRIGGER		2	/* Trigger frame */
#define FC_SUBTYPE_NDPA                 5	/* NDPA  */
#define FC_SUBTYPE_CTL_WRAPPER		7	/* Control Wrapper */
#define FC_SUBTYPE_BLOCKACK_REQ		8	/* Block Ack Req */
#define FC_SUBTYPE_BLOCKACK		9	/* Block Ack */
#define FC_SUBTYPE_PS_POLL		10	/* PS poll */
#define FC_SUBTYPE_RTS			11	/* RTS */
#define FC_SUBTYPE_CTS			12	/* CTS */
#define FC_SUBTYPE_ACK			13	/* ACK */
#define FC_SUBTYPE_CF_END		14	/* CF-END */
#define FC_SUBTYPE_CF_END_ACK		15	/* CF-END ACK */

/* Data Subtypes */
#define FC_SUBTYPE_DATA			0	/* Data */
#define FC_SUBTYPE_DATA_CF_ACK		1	/* Data + CF-ACK */
#define FC_SUBTYPE_DATA_CF_POLL		2	/* Data + CF-Poll */
#define FC_SUBTYPE_DATA_CF_ACK_POLL	3	/* Data + CF-Ack + CF-Poll */
#define FC_SUBTYPE_NULL			4	/* Null */
#define FC_SUBTYPE_CF_ACK		5	/* CF-Ack */
#define FC_SUBTYPE_CF_POLL		6	/* CF-Poll */
#define FC_SUBTYPE_CF_ACK_POLL		7	/* CF-Ack + CF-Poll */
#define FC_SUBTYPE_QOS_DATA		8	/* QoS Data */
#define FC_SUBTYPE_QOS_DATA_CF_ACK	9	/* QoS Data + CF-Ack */
#define FC_SUBTYPE_QOS_DATA_CF_POLL	10	/* QoS Data + CF-Poll */
#define FC_SUBTYPE_QOS_DATA_CF_ACK_POLL	11	/* QoS Data + CF-Ack + CF-Poll */
#define FC_SUBTYPE_QOS_NULL		12	/* QoS Null */
#define FC_SUBTYPE_QOS_CF_POLL		14	/* QoS CF-Poll */
#define FC_SUBTYPE_QOS_CF_ACK_POLL	15	/* QoS CF-Ack + CF-Poll */

/* Data Subtype Groups */
#define FC_SUBTYPE_ANY_QOS(s)		(((s) & 8) != 0)
#define FC_SUBTYPE_ANY_NULL(s)		(((s) & 4) != 0)
#define FC_SUBTYPE_ANY_CF_POLL(s)	(((s) & 2) != 0)
#define FC_SUBTYPE_ANY_CF_ACK(s)	(((s) & 1) != 0)
#define FC_SUBTYPE_ANY_PSPOLL(s)	(((s) & 10) != 0)

/* Type/Subtype Combos */
#define FC_KIND_MASK		(FC_TYPE_MASK | FC_SUBTYPE_MASK)	/* FC kind mask */

#define FC_KIND(t, s)	(((t) << FC_TYPE_SHIFT) | ((s) << FC_SUBTYPE_SHIFT))	/* FC kind */

#define FC_SUBTYPE(fc)	(((fc) & FC_SUBTYPE_MASK) >> FC_SUBTYPE_SHIFT)	/* Subtype from FC */
#define FC_TYPE(fc)	(((fc) & FC_TYPE_MASK) >> FC_TYPE_SHIFT)	/* Type from FC */

#define FC_ASSOC_REQ	FC_KIND(FC_TYPE_MNG, FC_SUBTYPE_ASSOC_REQ)	/* assoc. request */
#define FC_ASSOC_RESP	FC_KIND(FC_TYPE_MNG, FC_SUBTYPE_ASSOC_RESP)	/* assoc. response */
#define FC_REASSOC_REQ	FC_KIND(FC_TYPE_MNG, FC_SUBTYPE_REASSOC_REQ)	/* reassoc. request */
#define FC_REASSOC_RESP	FC_KIND(FC_TYPE_MNG, FC_SUBTYPE_REASSOC_RESP)	/* reassoc. response */
#define FC_PROBE_REQ	FC_KIND(FC_TYPE_MNG, FC_SUBTYPE_PROBE_REQ)	/* probe request */
#define FC_PROBE_RESP	FC_KIND(FC_TYPE_MNG, FC_SUBTYPE_PROBE_RESP)	/* probe response */
#define FC_BEACON	FC_KIND(FC_TYPE_MNG, FC_SUBTYPE_BEACON)		/* beacon */
#define FC_ATIM		FC_KIND(FC_TYPE_MNG, FC_SUBTYPE_ATIM)		/* ATIM */
#define FC_DISASSOC	FC_KIND(FC_TYPE_MNG, FC_SUBTYPE_DISASSOC)	/* disassoc */
#define FC_AUTH		FC_KIND(FC_TYPE_MNG, FC_SUBTYPE_AUTH)		/* authentication */
#define FC_DEAUTH	FC_KIND(FC_TYPE_MNG, FC_SUBTYPE_DEAUTH)		/* deauthentication */
#define FC_ACTION	FC_KIND(FC_TYPE_MNG, FC_SUBTYPE_ACTION)		/* action */
#define FC_ACTION_NOACK	FC_KIND(FC_TYPE_MNG, FC_SUBTYPE_ACTION_NOACK)	/* action no-ack */

#define FC_CTL_TRIGGER	FC_KIND(FC_TYPE_CTL, FC_SUBTYPE_TRIGGER)	/* Trigger frame */
#define FC_CTL_NDPA	FC_KIND(FC_TYPE_CTL, FC_SUBTYPE_NDPA)	/* NDPA frame */
#define FC_CTL_WRAPPER	FC_KIND(FC_TYPE_CTL, FC_SUBTYPE_CTL_WRAPPER)	/* Control Wrapper */
#define FC_BLOCKACK_REQ	FC_KIND(FC_TYPE_CTL, FC_SUBTYPE_BLOCKACK_REQ)	/* Block Ack Req */
#define FC_BLOCKACK	FC_KIND(FC_TYPE_CTL, FC_SUBTYPE_BLOCKACK)	/* Block Ack */
#define FC_PS_POLL	FC_KIND(FC_TYPE_CTL, FC_SUBTYPE_PS_POLL)	/* PS poll */
#define FC_RTS		FC_KIND(FC_TYPE_CTL, FC_SUBTYPE_RTS)		/* RTS */
#define FC_CTS		FC_KIND(FC_TYPE_CTL, FC_SUBTYPE_CTS)		/* CTS */
#define FC_ACK		FC_KIND(FC_TYPE_CTL, FC_SUBTYPE_ACK)		/* ACK */
#define FC_CF_END	FC_KIND(FC_TYPE_CTL, FC_SUBTYPE_CF_END)		/* CF-END */
#define FC_CF_END_ACK	FC_KIND(FC_TYPE_CTL, FC_SUBTYPE_CF_END_ACK)	/* CF-END ACK */

#define FC_DATA		FC_KIND(FC_TYPE_DATA, FC_SUBTYPE_DATA)		/* data */
#define FC_NULL_DATA	FC_KIND(FC_TYPE_DATA, FC_SUBTYPE_NULL)		/* null data */
#define FC_DATA_CF_ACK	FC_KIND(FC_TYPE_DATA, FC_SUBTYPE_DATA_CF_ACK)	/* data CF ACK */
#define FC_QOS_DATA	FC_KIND(FC_TYPE_DATA, FC_SUBTYPE_QOS_DATA)	/* QoS data */
#define FC_QOS_NULL	FC_KIND(FC_TYPE_DATA, FC_SUBTYPE_QOS_NULL)	/* QoS null */

/* QoS Control Field */

/* 802.1D Priority */
#define QOS_PRIO_SHIFT		0	/* QoS priority shift */
#define QOS_PRIO_MASK		0x0007	/* QoS priority mask */
#define QOS_PRIO(qos)		(((qos) & QOS_PRIO_MASK) >> QOS_PRIO_SHIFT)	/* QoS priority */

/* Traffic Identifier */
#define QOS_TID_SHIFT		0	/* QoS TID shift */
#define QOS_TID_MASK		0x000f	/* QoS TID mask */
#define QOS_TID(qos)		(((qos) & QOS_TID_MASK) >> QOS_TID_SHIFT)	/* QoS TID */

/* End of Service Period (U-APSD) */
#define QOS_EOSP_SHIFT		4	/* QoS End of Service Period shift */
#define QOS_EOSP_MASK		0x0010	/* QoS End of Service Period mask */
#define QOS_EOSP(qos)		(((qos) & QOS_EOSP_MASK) >> QOS_EOSP_SHIFT)	/* Qos EOSP */

/* Ack Policy */
#define QOS_ACK_NORMAL_ACK	0	/* Normal Ack */
#define QOS_ACK_NO_ACK		1	/* No Ack (eg mcast) */
#define QOS_ACK_NO_EXP_ACK	2	/* No Explicit Ack */
#define QOS_ACK_BLOCK_ACK	3	/* Block Ack */
#define QOS_ACK_SHIFT		5	/* QoS ACK shift */
#define QOS_ACK_MASK		0x0060	/* QoS ACK mask */
#define QOS_ACK(qos)		(((qos) & QOS_ACK_MASK) >> QOS_ACK_SHIFT)	/* QoS ACK */

/* A-MSDU flag */
#define QOS_AMSDU_SHIFT		7	/* AMSDU shift */
#define QOS_AMSDU_MASK		0x0080	/* AMSDU mask */

/* QOS Mesh Flags */
#define QOS_MESH_CTL_FLAG       0x0100u // Mesh Control Present
#define QOS_MESH_PSL_FLAG       0x0200u // Mesh Power Save Level
#define QOS_MESH_RSPI_FLAG      0x0400u // Mesh RSPI

/* QOS Mesh Accessor macros */
#define QOS_MESH_CTL(qos)       (((qos) & QOS_MESH_CTL_FLAG) != 0)
#define QOS_MESH_PSL(qos)       (((qos) & QOS_MESH_PSL_FLAG) != 0)
#define QOS_MESH_RSPI(qos)      (((qos) & QOS_MESH_RSPI_FLAG) != 0)

/* Management Frames */

/* Management Frame Constants */

/* Fixed fields */
#define DOT11_MNG_AUTH_ALGO_LEN		2	/* d11 management auth. algo. length */
#define DOT11_MNG_AUTH_SEQ_LEN		2	/* d11 management auth. seq. length */
#define DOT11_MNG_BEACON_INT_LEN	2	/* d11 management beacon interval length */
#define DOT11_MNG_CAP_LEN		2	/* d11 management cap. length */
#define DOT11_MNG_AP_ADDR_LEN		6	/* d11 management AP address length */
#define DOT11_MNG_LISTEN_INT_LEN	2	/* d11 management listen interval length */
#define DOT11_MNG_REASON_LEN		2	/* d11 management reason length */
#define DOT11_MNG_AID_LEN		2	/* d11 management AID length */
#define DOT11_MNG_STATUS_LEN		2	/* d11 management status length */
#define DOT11_MNG_TIMESTAMP_LEN		8	/* d11 management timestamp length */

/* DUR/ID field in assoc resp is 0xc000 | AID */
#define DOT11_AID_MASK			0x3fff	/* d11 AID mask */
#define DOT11_AID_OCTET_VAL_SHIFT	3u	/* AID octet value shift */
#define DOT11_AID_BIT_POS_IN_OCTET	0x07	/* AID bit position in octet */

/* Reason Codes */
#define DOT11_RC_RESERVED		0	/* d11 RC reserved */
#define DOT11_RC_UNSPECIFIED		1	/* Unspecified reason */
#define DOT11_RC_AUTH_INVAL		2	/* Previous authentication no longer valid */
#define DOT11_RC_DEAUTH_LEAVING		3	/* Deauthenticated because sending station
						 * is leaving (or has left) IBSS or ESS
						 */
#define DOT11_RC_INACTIVITY		4	/* Disassociated due to inactivity */
#define DOT11_RC_BUSY			5	/* Disassociated because AP is unable to handle
						 * all currently associated stations
						 */
#define DOT11_RC_INVAL_CLASS_2		6	/* Class 2 frame received from
						 * nonauthenticated station
						 */
#define DOT11_RC_INVAL_CLASS_3		7	/* Class 3 frame received from
						 *  nonassociated station
						 */
#define DOT11_RC_DISASSOC_LEAVING	8	/* Disassociated because sending station is
						 * leaving (or has left) BSS
						 */
#define DOT11_RC_NOT_AUTH		9	/* Station requesting (re)association is not
						 * authenticated with responding station
						 */
#define DOT11_RC_BAD_PC			10	/* Unacceptable power capability element */
#define DOT11_RC_BAD_CHANNELS		11	/* Unacceptable supported channels element */

/* 12 is unused by STA but could be used by AP */
#define DOT11_RC_DISASSOC_BTM		12	/* Disassociated due to BSS Transition Magmt */

/* 13-23 are WPA/802.11i reason codes defined in wpa.h */

#define DOT11_RC_TDLS_PEER_UNREACH	25
#define DOT11_RC_TDLS_DOWN_UNSPECIFIED	26

/* 32-39 are QSTA specific reasons added in 11e */
#define DOT11_RC_UNSPECIFIED_QOS	32	/* unspecified QoS-related reason */
#define DOT11_RC_INSUFFCIENT_BW		33	/* QAP lacks sufficient bandwidth */
#define DOT11_RC_EXCESSIVE_FRAMES	34	/* excessive number of frames need ack */
#define DOT11_RC_TX_OUTSIDE_TXOP	35	/* transmitting outside the limits of txop */
#define DOT11_RC_LEAVING_QBSS		36	/* QSTA is leaving the QBSS (or restting) */
#define DOT11_RC_BAD_MECHANISM		37	/* does not want to use the mechanism */
#define DOT11_RC_SETUP_NEEDED		38	/* mechanism needs a setup */
#define DOT11_RC_TIMEOUT		39	/* timeout */

#define DOT11_RC_INVALID_FT_ACTION_FC	48	/* Invalid FT Action frame count */
#define DOT11_RC_INVALID_PMKID		49	/* Invalid PMKID */
#define DOT11_RC_INVALID_MDE		50	/* Invalid MDE */
#define DOT11_RC_INVALID_FTE		51	/* Invalid FTE */

#define DOT11_RC_MESH_PEERING_CANCELLED		52
#define DOT11_RC_MESH_MAX_PEERS			53
#define DOT11_RC_MESH_CONFIG_POLICY_VIOLN	54
#define DOT11_RC_MESH_CLOSE_RECVD		55
#define DOT11_RC_MESH_MAX_RETRIES		56
#define DOT11_RC_MESH_CONFIRM_TIMEOUT		57
#define DOT11_RC_MESH_INVALID_GTK		58
#define DOT11_RC_MESH_INCONSISTENT_PARAMS	59

#define DOT11_RC_MESH_INVALID_SEC_CAP		60
#define DOT11_RC_MESH_PATHERR_NOPROXYINFO	61
#define DOT11_RC_MESH_PATHERR_NOFWINFO		62
#define DOT11_RC_MESH_PATHERR_DSTUNREACH	63
#define DOT11_RC_MESH_MBSSMAC_EXISTS		64
#define DOT11_RC_MESH_CHANSWITCH_REGREQ		65
#define DOT11_RC_MESH_CHANSWITCH_UNSPEC		66

#define DOT11_RC_POOR_RSSI_CONDITIONS	71	/* Poor RSSI */
#define DOT11_RC_MAX			71	/* Reason codes > 71 are reserved */

/* Status Codes */
#define DOT11_SC_SUCCESS		0	/* Successful */
#define DOT11_SC_FAILURE		1	/* Unspecified failure */
#define DOT11_SC_TDLS_WAKEUP_SCH_ALT	2	/* TDLS wakeup schedule rejected but
						 * alternative schedule provided
						 */
#define DOT11_SC_TDLS_WAKEUP_SCH_REJ	3	/* TDLS wakeup schedule rejected */
#define DOT11_SC_TDLS_SEC_DISABLED	5	/* TDLS Security disabled */
#define DOT11_SC_LIFETIME_REJ		6	/* Unacceptable lifetime */
#define DOT11_SC_NOT_SAME_BSS		7	/* Not in same BSS */
#define DOT11_SC_CAP_MISMATCH		10	/* Cannot support all requested
						 * capabilities in the Capability
						 * Information field
						 */
#define DOT11_SC_REASSOC_FAIL		11	/* Reassociation denied due to inability
						 * to confirm that association exists
						 */
#define DOT11_SC_ASSOC_FAIL		12	/* Association denied due to reason
						 * outside the scope of this standard
						 */
#define DOT11_SC_AUTH_MISMATCH		13	/* Responding station does not support
						 * the specified authentication
						 * algorithm
						 */
#define DOT11_SC_AUTH_SEQ		14	/* Received an Authentication frame
						 * with authentication transaction
						 * sequence number out of expected
						 * sequence
						 */
#define DOT11_SC_AUTH_CHALLENGE_FAIL	15	/* Authentication rejected because of
						 * challenge failure
						 */
#define DOT11_SC_AUTH_TIMEOUT		16	/* Authentication rejected due to timeout
						 * waiting for next frame in sequence
						 */
#define DOT11_SC_ASSOC_BUSY_FAIL	17	/* Association denied because AP is
						 * unable to handle additional
						 * associated stations
						 */
#define DOT11_SC_ASSOC_RATE_MISMATCH	18	/* Association denied due to requesting
						 * station not supporting all of the
						 * data rates in the BSSBasicRateSet
						 * parameter
						 */
#define DOT11_SC_ASSOC_SHORT_REQUIRED	19	/* Association denied due to requesting
						 * station not supporting the Short
						 * Preamble option
						 */
#define DOT11_SC_ASSOC_PBCC_REQUIRED	20	/* Association denied due to requesting
						 * station not supporting the PBCC
						 * Modulation option
						 */
#define DOT11_SC_ASSOC_AGILITY_REQUIRED	21	/* Association denied due to requesting
						 * station not supporting the Channel
						 * Agility option
						 */
#define DOT11_SC_ASSOC_SPECTRUM_REQUIRED 22	/* Association denied because Spectrum
						 * Management capability is required.
						 */
#define DOT11_SC_ASSOC_BAD_POWER_CAP	23	/* Association denied because the info
						 * in the Power Cap element is
						 * unacceptable.
						 */
#define DOT11_SC_ASSOC_BAD_SUP_CHANNELS	24	/* Association denied because the info
						 * in the Supported Channel element is
						 * unacceptable
						 */
#define DOT11_SC_ASSOC_SHORTSLOT_REQUIRED 25	/* Association denied due to requesting
						 * station not supporting the Short Slot
						 * Time option
						 */
#define DOT11_SC_ASSOC_DSSSOFDM_REQUIRED 26	/* Association denied because requesting station
						 * does not support the DSSS-OFDM option
						 */
#define DOT11_SC_ASSOC_HT_REQUIRED	27	/* Association denied because the requesting
						 * station does not support HT features
						 */
#define DOT11_SC_ASSOC_R0KH_UNREACHABLE	28	/* Association denied due to AP
						 * being unable to reach the R0 Key Holder
						 */
#define DOT11_SC_ASSOC_TRY_LATER	30	/* Association denied temporarily, try again later
						 */
#define DOT11_SC_ASSOC_MFP_VIOLATION	31	/* Association denied due to Robust Management
						 * frame policy violation
						 */

#define DOT11_SC_POOR_RSSI_CONDN	34	/* Association denied due to poor RSSI */
#define	DOT11_SC_DECLINED		37	/* request declined */
#define	DOT11_SC_INVALID_PARAMS		38	/* One or more params have invalid values */

/* The allocation or TS has not been created because the request cannot be honored;
 * however, a suggested TSPEC/DMG TSPEC is provided so that the initiating STA can attempt
 * to set another allocation or TS with the suggested changes to the TSPEC/DMG TSPEC.
 */
#define DOT11_SC_REJECTED_WITH_SUGGESTED_CHANGES 39

#define DOT11_SC_INVALID_GROUP_CIPHER	41	/* invalid pairwise cipher */
#define DOT11_SC_INVALID_PAIRWISE_CIPHER 42	/* invalid pairwise cipher */
#define	DOT11_SC_INVALID_AKMP		43	/* Association denied due to invalid AKMP */
#define	DOT11_SC_UNSUPPORTED_RSNE_VERSION 44	/* Unsupported RSNE version. */
#define DOT11_SC_INVALID_RSNIE_CAP	45	/* invalid RSN IE capabilities */
#define DOT11_SC_DLS_NOT_ALLOWED	48	/* DLS is not allowed in the BSS by policy */
#define	DOT11_SC_INVALID_PMKID		53	/* Association denied due to invalid PMKID */
#define	DOT11_SC_INVALID_MDID		54	/* Association denied due to invalid MDID */
#define	DOT11_SC_INVALID_FTIE		55	/* Association denied due to invalid FTIE */

/* Requested TCLAS processing is not supported by the AP or PCP. */
#define	DOT11_SC_REQUESTED_TCLAS_NOT_SUPPORTED_BY_AP		56u

/* The AP or PCP has insufficient TCLAS processing resources to satisfy the request. */
#define DOT11_SC_INSUFFICIENT_TCLAS_PROCESSING_RESOURCES	57u

#define DOT11_SC_ADV_PROTO_NOT_SUPPORTED 59	/* ad proto not supported */
#define DOT11_SC_NO_OUTSTAND_REQ	60	/* no outstanding req */
#define DOT11_SC_RSP_NOT_RX_FROM_SERVER	61	/* no response from server */
#define DOT11_SC_TIMEOUT		62	/* timeout */
#define DOT11_SC_QUERY_RSP_TOO_LARGE	63	/* query rsp too large */
#define DOT11_SC_SERVER_UNREACHABLE	65	/* server unreachable */

#define DOT11_SC_UNEXP_MSG		70	/* Unexpected message */
#define DOT11_SC_INVALID_SNONCE		71	/* Invalid SNonce */
#define DOT11_SC_INVALID_RSNIE		72	/* Invalid contents of RSNIE */

#define DOT11_SC_ANTICLOG_TOCKEN_REQUIRED 76	/* Anti-clogging tocken required */
#define DOT11_SC_INVALID_FINITE_CYCLIC_GRP 77	/* Invalid contents of RSNIE */
#define DOT11_SC_TRANSMIT_FAILURE	79      /* transmission failure */

#define DOT11_SC_REQUESTED_TCLAS_NOT_SUPPORTED	80u	/* Requested TCLAS not supported */
#define DOT11_SC_TCLAS_RESOURCES_EXHAUSTED	81u	/* TCLAS resources exhausted */
#define DOT11_SC_TCLAS_PROCESSING_TERMINATED	97u	/* End traffic classification */

#define DOT11_SC_ASSOC_VHT_REQUIRED	104	/* Association denied because the requesting
						 * station does not support VHT features.
						 */
#define DOT11_SC_UNKNOWN_PASSWORD_IDENTIFIER 123u /* mismatch of password id */

#define DOT11_SC_SAE_HASH_TO_ELEMENT	126u	/* SAE Hash-to-element PWE required */
#define DOT11_SC_SAE_PK			127u	/* SAE PK required */

/* Requested TCLAS processing has been terminated by the AP due to insufficient QoS capacity. */
#define DOT11_SC_TCLAS_PROCESSING_TERMINATED_INSUFFICIENT_QOS	128u

/* Requested TCLAS processing has been terminated by the AP due to conflict with
 * higher layer QoS policies.
 */
#define DOT11_SC_TCLAS_PROCESSING_TERMINATED_POLICY_CONFLICT	129u

#define DOT11_SC_INVALID_PUBLIC_KEY	130u /* Public key format is invalid */
#define DOT11_SC_PASN_BASE_AKM_FAILED	131u /* Failure from Base AKM processing during PASN */
#define DOT11_SC_OCI_MISMATCH		132u /* OCI does not match received */

/* Draft P802.11be D1.4 Table 9-78 Status codes */
/* DENIED_STA_AFFILIATED_WITH_MLD_WITH_EXISTING_MLD_ASSOCIATION
 * Association denied because the requesting STA is affiliated with a non-AP MLD
 * that is associated with the AP MLD.
 */
#define DOT11_SC_DENIED_EXIST_MLD_ASSOC	130u
/* EPCS_DENIED_UNAUTHO- RIZED
 * EPCS priority access denied because the non-AP MLD or non-AP EHT STA is not authorized
 * to use the service.
 */
#define DOT11_SC_EPCS_DENIED_UNAUTH	131u
/* EPCS_DENIED- _OTHER_REASON
 * EPCS priority access denied due to reason out- side the scope of this standard.
 */
#define DOT11_SC_EPCS_DENIED_O_REASON	132u
/* DENIED_TID_TO_LINK_MAPPING
 * Request denied because the requested TID-to-link map- ping is unacceptable.
 */
#define DOT11_SC_DENIED_TID_MAP		133u
/* PREFERRED_TID_TO_LINK_MAP- PING_SUGGESTED
 * Preferred TID-to-link mapping suggested.
 */
#define DOT11_SC_PREFER_TID_MAP		134u
/* DENIED_EHT_NOT_SUPPORTED
 * Association denied because the requesting STA does not support EHT features.
 */
#define DOT11_SC_DENIED_EHT_UNSUPPORTED	135u	/* TBD */

/* Info Elts, length of INFORMATION portion of Info Elts */
#define DOT11_MNG_DS_PARAM_LEN			1	/* d11 management DS parameter length */
#define DOT11_MNG_IBSS_PARAM_LEN		2	/* d11 management IBSS parameter length */

/* TIM Info element has 3 bytes fixed info in INFORMATION field,
 * followed by 1 to 251 bytes of Partial Virtual Bitmap
 */
#define DOT11_MNG_TIM_FIXED_LEN			3	/* d11 management TIM fixed length */
#define DOT11_MNG_TIM_DTIM_COUNT		0	/* d11 management DTIM count */
#define DOT11_MNG_TIM_DTIM_PERIOD		1	/* d11 management DTIM period */
#define DOT11_MNG_TIM_BITMAP_CTL		2	/* d11 management TIM BITMAP control  */
#define DOT11_MNG_TIM_PVB			3	/* d11 management TIM PVB */

#define DOT11_MNG_TIM_BITMAP_CTL_BCMC_MASK	0x01	/* Mask for bcmc bit in tim bitmap ctrl */
#define DOT11_MNG_TIM_BITMAP_CTL_PVBOFF_MASK	0xFE	/* Mask for partial virtual bitmap */

/* TLV defines */
#define TLV_TAG_OFF         0	/* tag offset */
#define TLV_LEN_OFF         1	/* length offset */
#define TLV_HDR_LEN         2	/* header length */
#define TLV_BODY_OFF        2	/* body offset */
#define TLV_BODY_LEN_MAX    255	/* max body length */
#define TLV_EXT_HDR_LEN     3u  /* extended IE header length */
#define TLV_EXT_BODY_OFF    3u  /* extended IE body offset */

/* Management Frame Information Element IDs */
enum dot11_tag_ids {
	DOT11_MNG_SSID_ID			= 0,	/* d11 management SSID id */
	DOT11_MNG_RATES_ID			= 1,	/* d11 management rates id */
	DOT11_MNG_FH_PARMS_ID			= 2,	/* d11 management FH parameter id */
	DOT11_MNG_DS_PARMS_ID			= 3,	/* d11 management DS parameter id */
	DOT11_MNG_CF_PARMS_ID			= 4,	/* d11 management CF parameter id */
	DOT11_MNG_TIM_ID			= 5,	/* d11 management TIM id */
	DOT11_MNG_IBSS_PARMS_ID			= 6,	/* d11 management IBSS parameter id */
	DOT11_MNG_COUNTRY_ID			= 7,	/* d11 management country id */
	DOT11_MNG_HOPPING_PARMS_ID		= 8,	/* d11 management hopping parameter id */
	DOT11_MNG_HOPPING_TABLE_ID		= 9,	/* d11 management hopping table id */
	DOT11_MNG_FTM_SYNC_INFO_ID		= 9,	/* 11mc D4.3 */
	DOT11_MNG_REQUEST_ID			= 10,	/* d11 management request id */
	DOT11_MNG_QBSS_LOAD_ID			= 11,	/* d11 management QBSS Load id */
	DOT11_MNG_EDCA_PARAM_ID			= 12,	/* 11E EDCA Parameter id */
	DOT11_MNG_TSPEC_ID			= 13,	/* d11 management TSPEC id */
	DOT11_MNG_TCLAS_ID			= 14,	/* d11 management TCLAS id */
	DOT11_MNG_CHALLENGE_ID			= 16,	/* d11 management chanllenge id */
	DOT11_MNG_PWR_CONSTRAINT_ID		= 32,	/* 11H PowerConstraint */
	DOT11_MNG_PWR_CAP_ID			= 33,	/* 11H PowerCapability */
	DOT11_MNG_TPC_REQUEST_ID		= 34,	/* 11H TPC Request */
	DOT11_MNG_TPC_REPORT_ID			= 35,	/* 11H TPC Report */
	DOT11_MNG_SUPP_CHANNELS_ID		= 36,	/* 11H Supported Channels */
	DOT11_MNG_CHANNEL_SWITCH_ID		= 37,	/* 11H ChannelSwitch Announcement */
	DOT11_MNG_MEASURE_REQUEST_ID		= 38,	/* 11H MeasurementRequest */
	DOT11_MNG_MEASURE_REPORT_ID		= 39,	/* 11H MeasurementReport */
	DOT11_MNG_QUIET_ID			= 40,	/* 11H Quiet */
	DOT11_MNG_IBSS_DFS_ID			= 41,	/* 11H IBSS_DFS */
	DOT11_MNG_ERP_ID			= 42,	/* d11 management ERP id */
	DOT11_MNG_TS_DELAY_ID			= 43,	/* d11 management TS Delay id */
	DOT11_MNG_TCLAS_PROC_ID			= 44,	/* d11 management TCLAS processing id */
	DOT11_MNG_HT_CAP			= 45,	/* d11 mgmt HT cap id */
	DOT11_MNG_QOS_CAP_ID			= 46,	/* 11E QoS Capability id */
	DOT11_MNG_NONERP_ID			= 47,	/* d11 management NON-ERP id */
	DOT11_MNG_RSN_ID			= 48,	/* d11 management RSN id */
	DOT11_MNG_EXT_RATES_ID			= 50,	/* d11 management ext. rates id */
	DOT11_MNG_AP_CHREP_ID			= 51,	/* 11k AP Channel report id */
	DOT11_MNG_NEIGHBOR_REP_ID		= 52,	/* 11k & 11v Neighbor report id */
	DOT11_MNG_RCPI_ID			= 53,	/* 11k RCPI */
	DOT11_MNG_MDIE_ID			= 54,	/* 11r Mobility domain id */
	DOT11_MNG_FTIE_ID			= 55,	/* 11r Fast Bss Transition id */
	DOT11_MNG_FT_TI_ID			= 56,	/* 11r Timeout Interval id */
	DOT11_MNG_RDE_ID			= 57,	/* 11r RIC Data Element id */
	DOT11_MNG_DSE_LOC_ID			= 58,	/* ?? DSE Registered Location */
	DOT11_MNG_REGCLASS_ID			= 59,	/* d11 management regulatory class id */
	DOT11_MNG_EXT_CSA_ID			= 60,	/* d11 Extended CSA */
	DOT11_MNG_HT_ADD			= 61,	/* d11 mgmt additional HT info */
	DOT11_MNG_EXT_CHANNEL_OFFSET		= 62,	/* d11 mgmt ext channel offset */
	DOT11_MNG_BSS_AVR_ACCESS_DELAY_ID	= 63,	/* 11k bss average access delay */
	DOT11_MNG_ANTENNA_ID			= 64,	/* 11k antenna id */
	DOT11_MNG_RSNI_ID			= 65,	/* 11k RSNI id */
	DOT11_MNG_MEASUREMENT_PILOT_TX_ID	= 66,	/* 11k measurement pilot tx info id */
	DOT11_MNG_BSS_AVAL_ADMISSION_CAP_ID	= 67,	/* 11k bss aval admission cap id */
	DOT11_MNG_BSS_AC_ACCESS_DELAY_ID	= 68,	/* 11k bss AC access delay id */
	DOT11_MNG_WAPI_ID			= 68,	/* d11 management WAPI id */
	DOT11_MNG_TIME_ADVERTISE_ID		= 69,	/* 11p time advertisement */
	DOT11_MNG_RRM_CAP_ID			= 70,	/* 11k radio measurement capability */
	DOT11_MNG_MULTIPLE_BSSID_ID		= 71,	/* 11k multiple BSSID id */
	DOT11_MNG_HT_BSS_COEXINFO_ID		= 72,	/* d11 mgmt OBSS Coexistence INFO */
	DOT11_MNG_HT_BSS_CHANNEL_REPORT_ID	= 73,	/* d11 mgmt OBSS Intolerant Channel list */
	DOT11_MNG_HT_OBSS_ID			= 74,	/* d11 mgmt OBSS HT info */
	DOT11_MNG_MMIE_ID			= 76,	/* d11 mgmt MIC IE */
	DOT11_MNG_NONTRANS_BSSID_CAP_ID		= 83,	/* 11k nontransmitted BSSID capability */
	DOT11_MNG_SSID_LIST_ID			= 84,	/* ?? SSID list */
	DOT11_MNG_MULTIPLE_BSSIDINDEX_ID	= 85,	/* 11k multiple BSSID index */
	DOT11_MNG_FMS_DESCR_ID			= 86,	/* 11v FMS descriptor */
	DOT11_MNG_FMS_REQ_ID			= 87,	/* 11v FMS request id */
	DOT11_MNG_FMS_RESP_ID			= 88,	/* 11v FMS response id */
	DOT11_MNG_QOS_TRF_CAP_ID		= 89,	/* ?? QoS Traffic Capabilities */
	DOT11_MNG_BSS_MAX_IDLE_PERIOD_ID	= 90,	/* 11v bss max idle id */
	DOT11_MNG_TFS_REQUEST_ID		= 91,	/* 11v tfs request id */
	DOT11_MNG_TFS_RESPONSE_ID		= 92,	/* 11v tfs response id */
	DOT11_MNG_WNM_SLEEP_MODE_ID		= 93,	/* 11v wnm-sleep mode id */
	DOT11_MNG_TIMBC_REQ_ID			= 94,	/* 11v TIM broadcast request id */
	DOT11_MNG_TIMBC_RESP_ID			= 95,	/* 11v TIM broadcast response id */
	DOT11_MNG_CHANNEL_USAGE			= 97,	/* 11v channel usage */
	DOT11_MNG_TIME_ZONE_ID			= 98,	/* 11v time zone */
	DOT11_MNG_DMS_REQUEST_ID		= 99,	/* 11v dms request id */
	DOT11_MNG_DMS_RESPONSE_ID		= 100,	/* 11v dms response id */
	DOT11_MNG_LINK_IDENTIFIER_ID		= 101,	/* 11z TDLS Link Identifier IE */
	DOT11_MNG_WAKEUP_SCHEDULE_ID		= 102,	/* 11z TDLS Wakeup Schedule IE */
	DOT11_MNG_CHANNEL_SWITCH_TIMING_ID	= 104,	/* 11z TDLS Channel Switch Timing IE */
	DOT11_MNG_PTI_CONTROL_ID		= 105,	/* 11z TDLS PTI Control IE */
	DOT11_MNG_PU_BUFFER_STATUS_ID		= 106,	/* 11z TDLS PU Buffer Status IE */
	DOT11_MNG_INTERWORKING_ID		= 107,	/* 11u interworking */
	DOT11_MNG_ADVERTISEMENT_ID		= 108,	/* 11u advertisement protocol */
	DOT11_MNG_EXP_BW_REQ_ID			= 109,	/* 11u expedited bandwith request */
	DOT11_MNG_QOS_MAP_ID			= 110,	/* 11u QoS map set */
	DOT11_MNG_ROAM_CONSORT_ID		= 111,	/* 11u roaming consortium */
	DOT11_MNG_EMERGCY_ALERT_ID		= 112,	/* 11u emergency alert identifier */
	DOT11_MNG_MESH_CONFIG			= 113,	/* Mesh Configuration */
	DOT11_MNG_MESH_ID			= 114,	/* Mesh ID */
	DOT11_MNG_MESH_PEER_MGMT_ID		= 117,	/* Mesh PEER MGMT IE */
	DOT11_MNG_EXT_CAP_ID			= 127,	/* d11 mgmt ext capability */
	DOT11_MNG_EXT_PREQ_ID			= 130,	/* Mesh PREQ IE */
	DOT11_MNG_EXT_PREP_ID			= 131,	/* Mesh PREP IE */
	DOT11_MNG_EXT_PERR_ID			= 132,	/* Mesh PERR IE */
	DOT11_MNG_MIC_ID			= 140,	/* MIC IE */
	DOT11_MNG_MULTI_BAND_ID			= 158,	/* Multi-band IE */
	DOT11_MNG_ADDBA_EXT_ID			= 159,	/* ADDBA ext IE */
	DOT11_MNG_INTRA_AC_PRIO_ID		= 184,	/* Intra-Access Category Priority IE */
	DOT11_MNG_SCS_DESCR_ID			= 185,	/* SCS Descriptor IE */
	DOT11_MNG_GCR_GRPADDR_ID		= 189,	/* GCR group address IE */
	DOT11_MNG_VHT_CAP_ID			= 191,	/* d11 mgmt VHT cap id */
	DOT11_MNG_VHT_OPERATION_ID		= 192,	/* d11 mgmt VHT op id */
	DOT11_MNG_EXT_BSSLOAD_ID		= 193,	/* d11 mgmt VHT extended bss load id */
	DOT11_MNG_WIDE_BW_CHANNEL_SWITCH_ID	= 194,	/* Wide BW Channel Switch IE */
	DOT11_MNG_VHT_TRANSMIT_POWER_ENVELOPE_ID= 195,	/* VHT transmit Power Envelope IE */
	DOT11_MNG_CHANNEL_SWITCH_WRAPPER_ID	= 196,	/* Channel Switch Wrapper IE */
	DOT11_MNG_AID_ID			= 197,	/* Association ID  IE */
	DOT11_MNG_OPER_MODE_NOTIF_ID		= 199,	/* d11 mgmt VHT oper mode notif */
	DOT11_MNG_RNR_ID			= 201,
	/* FIXME: Use these temp. IDs until ANA assigns IDs */
	DOT11_MNG_FTM_PARAMS_ID			= 206,	/* mcd3.2/2014 this is not final yet */
	DOT11_MNG_TWT_ID			= 216,	/* 11ah D5.0 */
	DOT11_MNG_WPA_ID			= 221,	/* d11 management WPA id */
	DOT11_MNG_PROPR_ID			= 221,	/* d11 management proprietary id */
	/* should start using this one instead of above two */
	DOT11_MNG_VS_ID				= 221,	/* d11 management Vendor Specific IE */
	DOT11_MNG_MESH_CSP_ID			= 222,	/* d11 Mesh Channel Switch Parameter */
	DOT11_MNG_FILS_IND_ID			= 240,	/* 11ai FILS Indication element */
	DOT11_MNG_FRAGMENT_ID			= 242,	/* IE's fragment ID */
	DOT11_MNG_RSNXE_ID			= 244,	/* RSN Extension Element (RSNXE) ID */

	/* The follwing ID extensions should be defined >= 255
	 * i.e. the values should include 255 (DOT11_MNG_ID_EXT_ID + ID Extension).
	 */
	DOT11_MNG_ID_EXT_ID			= 255	/* Element ID Extension 11mc D4.3 */
};

/* FILS and OCE ext ids */
#define FILS_EXTID_MNG_REQ_PARAMS		2u	/* FILS Request Parameters element */
#define DOT11_MNG_FILS_REQ_PARAMS		(DOT11_MNG_ID_EXT_ID + FILS_EXTID_MNG_REQ_PARAMS)
#define FILS_EXTID_MNG_KEY_CONFIRMATION_ID	3u	/* FILS Key Confirmation element */
#define DOT11_MNG_FILS_KEY_CONFIRMATION		(DOT11_MNG_ID_EXT_ID + \
						 FILS_EXTID_MNG_KEY_CONFIRMATION_ID)
#define FILS_EXTID_MNG_SESSION_ID		4u	/* FILS Session element */
#define DOT11_MNG_FILS_SESSION			(DOT11_MNG_ID_EXT_ID + FILS_EXTID_MNG_SESSION_ID)
#define FILS_EXTID_MNG_HLP_CONTAINER_ID		5u	/* FILS HLP Container element */
#define DOT11_MNG_FILS_HLP_CONTAINER		(DOT11_MNG_ID_EXT_ID + \
						 FILS_EXTID_MNG_HLP_CONTAINER_ID)
#define FILS_EXTID_MNG_KEY_DELIVERY_ID		7u	/* FILS Key Delivery element */
#define DOT11_MNG_FILS_KEY_DELIVERY		(DOT11_MNG_ID_EXT_ID + \
						 FILS_EXTID_MNG_KEY_DELIVERY_ID)
#define FILS_EXTID_MNG_WRAPPED_DATA_ID		8u	/* FILS Wrapped Data element */
#define DOT11_MNG_FILS_WRAPPED_DATA		(DOT11_MNG_ID_EXT_ID + \
						 FILS_EXTID_MNG_WRAPPED_DATA_ID)
/* FILS wrapped data element is renamed to wrapped data element.
 * The data will be used by FILS and PASN.
 */
#define EXTID_MNG_WRAPPED_DATA_ID		FILS_EXTID_MNG_WRAPPED_DATA_ID
#define DOT11_MNG_WRAPPED_DATA			DOT11_MNG_FILS_WRAPPED_DATA

#define EXT_MNG_EXT_REQ_ID			10u	/* Extended Request element */
#define DOT11_MNG_EXT_REQ_ID			(DOT11_MNG_ID_EXT_ID + EXT_MNG_EXT_REQ_ID)
#define OCE_EXTID_MNG_ESP_ID			11u	/* Estimated Service Parameters element */
#define DOT11_MNG_ESP				(DOT11_MNG_ID_EXT_ID + OCE_EXTID_MNG_ESP_ID)
#define FILS_EXTID_MNG_PUBLIC_KEY_ID		12u	/* FILS Public Key element */
#define DOT11_MNG_FILS_PUBLIC_KEY		(DOT11_MNG_ID_EXT_ID + FILS_EXTID_MNG_PUBLIC_KEY_ID)
#define FILS_EXTID_MNG_NONCE_ID			13u	/* FILS Nonce element */
#define DOT11_MNG_FILS_NONCE			(DOT11_MNG_ID_EXT_ID + FILS_EXTID_MNG_NONCE_ID)

#define EXT_MNG_OWE_DH_PARAM_ID			32u	/* OWE DH Param ID - RFC 8110 */
#define DOT11_MNG_OWE_DH_PARAM_ID		(DOT11_MNG_ID_EXT_ID + EXT_MNG_OWE_DH_PARAM_ID)
#define EXT_MSG_PASSWORD_IDENTIFIER_ID		33u	/* Password ID EID */
#define DOT11_MSG_PASSWORD_IDENTIFIER_ID	(DOT11_MNG_ID_EXT_ID + \
						 EXT_MSG_PASSWORD_IDENTIFIER_ID)
#define EXT_MNG_HE_CAP_ID			35u	/* HE Capabilities, 11ax */
#define DOT11_MNG_HE_CAP_ID			(DOT11_MNG_ID_EXT_ID + EXT_MNG_HE_CAP_ID)
#define EXT_MNG_HE_OP_ID			36u	/* HE Operation IE, 11ax */
#define DOT11_MNG_HE_OP_ID			(DOT11_MNG_ID_EXT_ID + EXT_MNG_HE_OP_ID)
#define EXT_MNG_UORA_ID				37u	/* UORA Parameter Set */
#define DOT11_MNG_UORA_ID			(DOT11_MNG_ID_EXT_ID + EXT_MNG_UORA_ID)
#define EXT_MNG_MU_EDCA_ID			38u	/* MU EDCA Parameter Set */
#define DOT11_MNG_MU_EDCA_ID			(DOT11_MNG_ID_EXT_ID + EXT_MNG_MU_EDCA_ID)
#define EXT_MNG_SRPS_ID				39u	/* Spatial Reuse Parameter Set */
#define DOT11_MNG_SRPS_ID			(DOT11_MNG_ID_EXT_ID + EXT_MNG_SRPS_ID)
#define EXT_MNG_BSSCOLOR_CHANGE_ID		42u	/* BSS Color Change Announcement */
#define DOT11_MNG_BSSCOLOR_CHANGE_ID		(DOT11_MNG_ID_EXT_ID + EXT_MNG_BSSCOLOR_CHANGE_ID)
#define OCV_EXTID_MNG_OCI_ID			54u     /* OCI element */
#define DOT11_MNG_OCI_ID			(DOT11_MNG_ID_EXT_ID + OCV_EXTID_MNG_OCI_ID)
#define EXT_MNG_NON_INHERITANCE_ID		56u     /* Non-Inheritance element */
#define DOT11_MNG_NON_INHERITANCE_ID		(DOT11_MNG_ID_EXT_ID + EXT_MNG_NON_INHERITANCE_ID)
#define EXT_MNG_SHORT_SSID_ID			58u	/* SHORT SSID ELEMENT */
#define DOT11_MNG_SHORT_SSID_LIST_ID		(DOT11_MNG_ID_EXT_ID + EXT_MNG_SHORT_SSID_ID)
#define EXT_MNG_HE_6G_CAP_ID			59u	/* HE Extended Capabilities, 11ax */
#define DOT11_MNG_HE_6G_CAP_ID			(DOT11_MNG_ID_EXT_ID + EXT_MNG_HE_6G_CAP_ID)

#define MSCS_EXTID_MNG_DESCR_ID			88u	/* Ext ID for the MSCS descriptor */
#define DOT11_MNG_MSCS_DESCR_ID			(DOT11_MNG_ID_EXT_ID + MSCS_EXTID_MNG_DESCR_ID)

#define TCLAS_EXTID_MNG_MASK_ID			89u	/* Ext ID for the TCLAS Mask element */
#define DOT11_MNG_TCLASS_MASK_ID		(DOT11_MNG_ID_EXT_ID + TCLAS_EXTID_MNG_MASK_ID)

#define SAE_EXT_REJECTED_GROUPS_ID		92u	/* SAE Rejected Groups element */
#define DOT11_MNG_REJECTED_GROUPS_ID		(DOT11_MNG_ID_EXT_ID + SAE_EXT_REJECTED_GROUPS_ID)
#define SAE_EXT_ANTICLOG_TOKEN_CONTAINER_ID	93u	/* SAE Anti-clogging token container */
#define DOT11_MNG_ANTICLOG_TOKEN_CONTAINER_ID	(DOT11_MNG_ID_EXT_ID + \
						 SAE_EXT_ANTICLOG_TOKEN_CONTAINER_ID)

/* P802.11az/D2.5 Table 9-92 */
#define EXT_MNG_SLTF_PARAMS_ID			94u	/* Secure LTF Parameters */
#define DOT11_MNG_SLTF_PARAMS_ID		(DOT11_MNG_ID_EXT_ID + EXT_MNG_SLTF_PARAMS_ID)
#define EXT_MNG_ISTA_PASV_TB_RMR_ID		95u	/* ISTA Passive TB Ranging Meas. Report */
#define DOT11_MNG_ISTA_PASV_TB_RMR_ID		(DOT11_MNG_ID_EXT_ID + EXT_MNG_ISTA_PASV_TB_RMR_ID)
#define EXT_MNG_RSTA_PASV_TB_RMR_ID		96u	/* RSTA Passive TB Ranging Meas. Report */
#define DOT11_MNG_RSTA_PASV_TB_RMR_ID		(DOT11_MNG_ID_EXT_ID + EXT_MNG_RSTA_PASV_TB_RMR_ID)
#define EXT_MNG_PASV_TB_LCI_TABLE_ID		97u	/* Passive TB Ranging LCI Table element */
#define DOT11_MNG_PASV_TB_LCI_TABLE_ID		(DOT11_MNG_ID_EXT_ID + EXT_MNG_PASV_TB_LCI_TABLE_ID)
#define EXT_MNG_ISTA_AVAIL_WINDOW_ID		98u	/* ISTA Availablity Window */
#define DOT11_MNG_ISTA_AVAIL_WINDOW_ID		(DOT11_MNG_ID_EXT_ID + EXT_MNG_ISTA_AVAIL_WINDOW_ID)
#define EXT_MNG_RSTA_AVAIL_WINDOW_ID		99u	/* RSTA Availablity Window */
#define DOT11_MNG_RSTA_AVAIL_WINDOW_ID		(DOT11_MNG_ID_EXT_ID + EXT_MNG_RSTA_AVAIL_WINDOW_ID)
#define EXT_MNG_PASN_PARAMS_ID			100u	/* PASN Parameters */
#define DOT11_MNG_PASN_PARAMS_ID		(DOT11_MNG_ID_EXT_ID + EXT_MNG_PASN_PARAMS_ID)
#define EXT_MNG_RANGING_PARAMS_ID		101u	/* Ranging Parameters */
#define DOT11_MNG_RANGING_PARAMS_ID		(DOT11_MNG_ID_EXT_ID + EXT_MNG_RANGING_PARAMS_ID)
#define EXT_MNG_DIR_MEAS_RESULTS_ID		102u	/* Direction Measurement Results */
#define DOT11_MNG_DIR_MEAS_RESULTS_ID		(DOT11_MNG_ID_EXT_ID + EXT_MNG_DIR_MEAS_RESULTS_ID)
#define EXT_MNG_MULTI_AOD_FEEDBACK_ID		103u	/* Multiple AOD Feedback */
#define DOT11_MNG_MULTI_AOD_FEEDDBACK_ID	(DOT11_MNG_ID_EXT_ID + \
						 EXT_MNG_MULTI_AOD_FEEDBACK_ID)
#define EXT_MNG_MULTI_BEST_AWV_ID_ID		104u	/* Multiple Best AWV ID */
#define DOT11_MNG_MULTI_BEST_AWV_ID_ID		(DOT11_MNG_ID_EXT_ID + EXT_MNG_MULTI_BEST_AWV_ID_ID)
#define EXT_MNG_LOS_LIKELIHOOD_ID		105u	/* LOS Likelihood */
#define DOT11_MNG_LOS_LIKELIHOOD_ID		(DOT11_MNG_ID_EXT_ID + EXT_MNG_LOS_LIKELIHOOD_ID)
/* Draft P802.11be D1.2 Table 9-92 Element IDs */
#define EXT_MNG_EHT_OP_ID			106u	/* EHT Operation */
#define DOT11_MNG_EHT_OP_ID			(DOT11_MNG_ID_EXT_ID + EXT_MNG_EHT_OP_ID)
#define EXT_MNG_ML_ID				107u	/* Multi-Link */
#define DOT11_MNG_ML_ID				(DOT11_MNG_ID_EXT_ID + EXT_MNG_ML_ID)
#define EXT_MNG_EHT_CAP_ID			108u	/* EHT Capabilities */
#define DOT11_MNG_EHT_CAP_ID			(DOT11_MNG_ID_EXT_ID + EXT_MNG_EHT_CAP_ID)
#define EXT_MNG_TID_MAP_ID			109u	/* TID-To-Link Mapping */
#define DOT11_MNG_TID_MAP_ID			(DOT11_MNG_ID_EXT_ID + EXT_MNG_TID_MAP_ID)
#define EXT_MNG_ML_TRAFFIC_ID			110u	/* Multi-Link Traffic */
#define DOT11_MNG_ML_TRAFFIC_ID			(DOT11_MNG_ID_EXT_ID + EXT_MNG_ML_TRAFFIC_ID)
/* P802.11be D1.6 Table 9-128 Element IDs */
#define EXT_MNG_QOS_CHAR_ID			113u	/* QoS Characteristics */
#define DOT11_MNG_QOS_CHAR_ID			(DOT11_MNG_ID_EXT_ID + EXT_MNG_QOS_CHAR_ID)

/* deprecated definitions, do not use, to be deleted later */
#define FILS_HLP_CONTAINER_EXT_ID		FILS_EXTID_MNG_HLP_CONTAINER_ID
#define DOT11_ESP_EXT_ID			OCE_EXTID_MNG_ESP_ID
#define FILS_REQ_PARAMS_EXT_ID			FILS_EXTID_MNG_REQ_PARAMS
#define EXT_MNG_RAPS_ID				37u	/* OFDMA Random Access Parameter Set */
#define DOT11_MNG_RAPS_ID			(DOT11_MNG_ID_EXT_ID + EXT_MNG_RAPS_ID)
/* End of deprecated definitions */

#define DOT11_MNG_IE_ID_EXT_MATCH(_ie, _id) (\
	((_ie)->id == DOT11_MNG_ID_EXT_ID) && \
	((_ie)->len > 0) && \
	((_id) == ((const uint8 *)(_ie) + TLV_HDR_LEN)[0]))

#define DOT11_MNG_IE_ID_EXT_INIT(_ie, _id, _len) do {\
		(_ie)->id = DOT11_MNG_ID_EXT_ID; \
		(_ie)->len = _len; \
		(_ie)->id_ext = _id; \
	} while (0)

/* Rate Defines */

/* Valid rates for the Supported Rates and Extended Supported Rates IEs.
 * Encoding is the rate in 500kbps units, rouding up for fractional values.
 * 802.11-2012, section 6.5.5.2, DATA_RATE parameter enumerates all the values.
 * The rate values cover DSSS, HR/DSSS, ERP, and OFDM phy rates.
 * The defines below do not cover the rates specific to 10MHz, {3, 4.5, 27},
 * and 5MHz, {1.5, 2.25, 3, 4.5, 13.5}, which are not supported by Broadcom devices.
 */

#define DOT11_RATE_1M   2       /* 1  Mbps in 500kbps units */
#define DOT11_RATE_2M   4       /* 2  Mbps in 500kbps units */
#define DOT11_RATE_5M5  11      /* 5.5 Mbps in 500kbps units */
#define DOT11_RATE_11M  22      /* 11 Mbps in 500kbps units */
#define DOT11_RATE_6M   12      /* 6  Mbps in 500kbps units */
#define DOT11_RATE_9M   18      /* 9  Mbps in 500kbps units */
#define DOT11_RATE_12M  24      /* 12 Mbps in 500kbps units */
#define DOT11_RATE_18M  36      /* 18 Mbps in 500kbps units */
#define DOT11_RATE_24M  48      /* 24 Mbps in 500kbps units */
#define DOT11_RATE_36M  72      /* 36 Mbps in 500kbps units */
#define DOT11_RATE_48M  96      /* 48 Mbps in 500kbps units */
#define DOT11_RATE_54M  108     /* 54 Mbps in 500kbps units */
#define DOT11_RATE_MAX  108     /* highest rate (54 Mbps) in 500kbps units */

/* Supported Rates and Extended Supported Rates IEs
 * The supported rates octets are defined a the MSB indicatin a Basic Rate
 * and bits 0-6 as the rate value
 */
#define DOT11_RATE_BASIC                0x80 /* flag for a Basic Rate */
#define DOT11_RATE_MASK                 0x7F /* mask for numeric part of rate */

/* BSS Membership Selector parameters
 * These selector values are advertised in Supported Rates and Extended Supported Rates IEs
 * in the supported rates list with the Basic rate bit set.
 * Constants below include the basic bit.
 */
/* IEEE Std 802.11-2016 Table 9-78 BSS membership selector value encoding */
#define DOT11_BSS_MEMBERSHIP_HT		0xFF	/* Basic 0x80 + 127, HT Required to join */
#define DOT11_BSS_MEMBERSHIP_VHT	0xFE	/* Basic 0x80 + 126, VHT Required to join */
/* IEEE P802.11-REVmd D5.0 Table 9-93 BSS membership selector value encoding */
#define DOT11_BSS_MEMBERSHIP_GLK	0xFD	/* Basic 0x80 + 125, GLK Op is required */
#define DOT11_BSS_MEMBERSHIP_EPD	0xFC	/* Basic 0x80 + 124, EPD is required */
#define DOT11_BSS_SAE_HASH_TO_ELEMENT	0xFB	/* Basic 0x80 + 123, SAE Hash-to-element */
/* Draft P802.11ax D8.0 Table 9-93 BSS membership selector value encoding */
#define DOT11_BSS_MEMBERSHIP_HE		0xFA	/* Basic 0x80 + 122, HE Required to join */
/* P802.11be D1.6 Table 9-129 BSS membership selector value encoding - TBD */
#define DOT11_BSS_MEMBERSHIP_EHT	0xF9	/* Basic 0x80 + 121, EHT Required to join */

/* TS Delay element offset & size */
#define DOT11_MGN_TS_DELAY_LEN		4	/* length of TS DELAY IE */
#define TS_DELAY_FIELD_SIZE		4	/* TS DELAY field size */

/* ERP info element bit values */
#define DOT11_MNG_ERP_LEN		1	/* ERP is currently 1 byte long */
#define DOT11_MNG_NONERP_PRESENT	0x01	/* NonERP (802.11b) STAs are present
						 *in the BSS
						 */
#define DOT11_MNG_USE_PROTECTION	0x02	/* Use protection mechanisms for
						 *ERP-OFDM frames
						 */
#define DOT11_MNG_BARKER_PREAMBLE	0x04	/* Short Preambles: 0 == allowed,
						 * 1 == not allowed
						 */
/* Capability Information Field */
#define DOT11_CAP_ESS			0x0001	/* ESS */
#define DOT11_CAP_IBSS			0x0002	/* IBSS */
#define DOT11_CAP_POLLABLE		0x0004	/* pollable */
#define DOT11_CAP_POLL_RQ		0x0008	/* poll request */
#define DOT11_CAP_PRIVACY		0x0010	/* privacy */
#define DOT11_CAP_SHORT			0x0020	/* short preamble */
#define DOT11_CAP_PBCC			0x0040	/* reserved (IEEE Std 802.11-2016) */
#define DOT11_CAP_CRITICAL_UPD		0x0040	/* critical update (Draft P802.11be D0.2) */
#define DOT11_CAP_AGILITY		0x0080	/* reserved (IEEE Std 802.11-2016) */
#define DOT11_CAP_SPECTRUM		0x0100	/* spectrum management */
#define DOT11_CAP_QOS			0x0200	/* qos */
#define DOT11_CAP_SHORTSLOT		0x0400	/* short slot time */
#define DOT11_CAP_APSD			0x0800	/* apsd */
#define DOT11_CAP_RRM			0x1000	/* radio measurement */
#define DOT11_CAP_CCK_OFDM		0x2000	/* reserved (IEEE Std 802.11-2016) */
#define DOT11_CAP_DELAY_BA		0x4000	/* delayed block ack */
#define DOT11_CAP_IMMEDIATE_BA		0x8000	/* immediate block ack */

/* Extended capabilities IE bitfields */
/* 20/40 BSS Coexistence Management support bit position */
#define DOT11_EXT_CAP_OBSS_COEX_MGMT		0u
/* Extended Channel Switching support bit position */
#define DOT11_EXT_CAP_EXT_CHAN_SWITCHING	2u
/* scheduled PSMP support bit position */
#define DOT11_EXT_CAP_SPSMP			6u
/*  Flexible Multicast Service */
#define DOT11_EXT_CAP_FMS			11u
/* proxy ARP service support bit position */
#define DOT11_EXT_CAP_PROXY_ARP			12u
/* Civic Location */
#define DOT11_EXT_CAP_CIVIC_LOC			14u
/* Geospatial Location */
#define DOT11_EXT_CAP_LCI			15u
/* Traffic Filter Service */
#define DOT11_EXT_CAP_TFS			16u
/* WNM-Sleep Mode */
#define DOT11_EXT_CAP_WNM_SLEEP			17u
/* TIM Broadcast service */
#define DOT11_EXT_CAP_TIMBC			18u
/* BSS Transition Management support bit position */
#define DOT11_EXT_CAP_BSSTRANS_MGMT		19u
/* Multiple BSSID support position */
#define DOT11_EXT_CAP_MULTIBSSID		22u
/* Direct Multicast Service */
#define DOT11_EXT_CAP_DMS			26u
/* Interworking support bit position */
#define DOT11_EXT_CAP_IW			31u
/* QoS map support bit position */
#define DOT11_EXT_CAP_QOS_MAP			32u
/* service Interval granularity bit position and mask */
#define DOT11_EXT_CAP_SI			41u
#define DOT11_EXT_CAP_SI_MASK			0x0E
/* Location Identifier service */
#define DOT11_EXT_CAP_IDENT_LOC			44u
/* WNM notification */
#define DOT11_EXT_CAP_WNM_NOTIF			46u

/* SCS (Stream Classification Service) support */
#define DOT11_EXT_CAP_SCS			54u

/* Operating mode notification - VHT (11ac D3.0 - 8.4.2.29) */
#define DOT11_EXT_CAP_OPER_MODE_NOTIF		62u
/* Number of MSDU-per-AMSDU: 0:unlimited 1:32 2:16 3:8 */
#define DOT11_EXT_CAP_MDSU_PER_AMSDU_LO		63u
#define DOT11_EXT_CAP_MDSU_PER_AMSDU_HI		64u
/* Fine timing measurement - D3.0 */
#define DOT11_EXT_CAP_FTM_RESPONDER		70u
#define DOT11_EXT_CAP_FTM_INITIATOR		71u /* tentative 11mcd3.0 */
#define DOT11_EXT_CAP_FILS			72u /* FILS Capability */
/* TWT support */
#define DOT11_EXT_CAP_TWT_REQUESTER		77u
#define DOT11_EXT_CAP_TWT_RESPONDER		78u
#define DOT11_EXT_CAP_OBSS_NB_RU_OFDMA		79u
/* FIXME: Use these temp. IDs until ANA assigns IDs */
#define DOT11_EXT_CAP_EMBSS_ADVERTISE		80u
/* SAE password ID */
#define DOT11_EXT_CAP_SAE_PWD_ID_INUSE		81u
#define DOT11_EXT_CAP_SAE_PWD_ID_USED_EXCLUSIVE	82u
/* Beacon Protection Enabled 802.11 D3.0 - 9.4.2.26
 * This field is reserved for a STA.
 */
#define DOT11_EXT_CAP_BCN_PROT			84u

/* Mirrored SCS (MSCS) support */
#define DOT11_EXT_CAP_MSCS			85u

/* Draft P802.11az/D2.5 - Table 9-153 Extended Capabilities field */
#define DOT11_EXT_CAP_NTB_RANGING_RESPONDER	90u /* 11az NTB RSTA */
#define DOT11_EXT_CAP_TB_RANGING_RESPONDER	91u /* 11az TB RSTA */
#define DOT11_EXT_CAP_PSV_RANGING_RESPONDER	92u /* 11az Passive RSTA */
#define DOT11_EXT_CAP_PSV_RANGING_INITIATOR	93u /* 11az Passive ISTA */
#define DOT11_EXT_CAP_AOA_MEASUREMENT		94u /* AOA measurement available */
#define DOT11_EXT_CAP_PHASE_SHIFT_FEEDBACK	95u /* Phase shift feedback support */
#define DOT11_EXT_CAP_DMG_LOCATION_SUPPORT_AP	96u /* DMG/Location supporting APs in the area */
#define DOT11_EXT_CAP_I2R_LMR_FEEDBACK_POLICY	97u /* I2R LMR Feedback policy (of RSTA) */

/* Last bit index of Extended Capabilities defined in P802.11
 * Please update DOT11_EXT_CAP_LAST_BIT_IDX to the last bit when P802.11 introduces new bit
 */
#define DOT11_EXT_CAP_LAST_BIT_IDX	DOT11_EXT_CAP_I2R_LMR_FEEDBACK_POLICY /* update this */
#define DOT11_EXT_CAP_NUM_BITS_MAX	(DOT11_EXT_CAP_LAST_BIT_IDX + 1) /* last bit index + 1 */

/* Please use DOT11_EXT_CAP_BYTE_LEN_MAX for any of ext cap buffer size ONLY in ATTACH time.
 * Othewise, change of DOT11_EXT_CAP_BYTE_LEN_MAX cause ROM invalidation.
 * DOT11_EXT_CAP_BYTE_LEN_MAX can grow with 802.11 change.
 */
#define DOT11_EXT_CAP_BYTE_LEN_MAX	((DOT11_EXT_CAP_NUM_BITS_MAX + 7) >> 3) /* byte len */

/********************************************************************
 * DO NOT use following macros for Trunk/PUMA and later branch
 * DOT11_EXT_CAP_MAX_IDX, DOT11_EXT_CAP_MAX_BIT_IDX,
 * DOT11_EXTCAP_LEN_MAX and dot11_extcap_t will be cleaned up
 */

/* TODO: Update DOT11_EXT_CAP_MAX_IDX to reflect the highest offset.
 * Note: DOT11_EXT_CAP_MAX_IDX must only be used in attach path.
 *       It will cause ROM invalidation otherwise.
 */
#define DOT11_EXT_CAP_MAX_IDX			85u

/* Remove this hack (DOT11_EXT_CAP_MAX_BIT_IDX) when no one
 * references DOT11_EXTCAP_LEN_MAX
 */
#define DOT11_EXT_CAP_MAX_BIT_IDX		95u	/* !!!update this please!!! */

/* Remove DOT11_EXTCAP_LEN_MAX when no one references it */
/* extended capability */
#ifndef DOT11_EXTCAP_LEN_MAX
#define DOT11_EXTCAP_LEN_MAX ((DOT11_EXT_CAP_MAX_BIT_IDX + 8) >> 3)
#endif
/* Remove dot11_extcap when no one references it */
BWL_PRE_PACKED_STRUCT struct dot11_extcap {
	uint8 extcap[DOT11_EXTCAP_LEN_MAX];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_extcap dot11_extcap_t;

/*
 * Action Frame Constants
 */
#define DOT11_ACTION_HDR_LEN		2	/* action frame category + action field */
#define DOT11_ACTION_CAT_OFF		0	/* category offset */
#define DOT11_ACTION_ACT_OFF		1	/* action offset */

/* Action Category field (sec 8.4.1.11) */
#define DOT11_ACTION_CAT_ERR_MASK	0x80	/* category error mask */
#define DOT11_ACTION_CAT_MASK		0x7F	/* category mask */
#define DOT11_ACTION_CAT_SPECT_MNG	0	/* category spectrum management */
#define DOT11_ACTION_CAT_QOS		1	/* category QoS */
#define DOT11_ACTION_CAT_DLS		2	/* category DLS */
#define DOT11_ACTION_CAT_BLOCKACK	3	/* category block ack */
#define DOT11_ACTION_CAT_PUBLIC		4	/* category public */
#define DOT11_ACTION_CAT_RRM		5	/* category radio measurements */
#define DOT11_ACTION_CAT_FBT		6	/* category fast bss transition */
#define DOT11_ACTION_CAT_HT		7	/* category for HT */
#define DOT11_ACTION_CAT_SA_QUERY	8	/* security association query */
#define DOT11_ACTION_CAT_PDPA		9	/* protected dual of public action */
#define DOT11_ACTION_CAT_WNM		10	/* category for WNM */
#define DOT11_ACTION_CAT_UWNM		11	/* category for Unprotected WNM */
#define DOT11_ACTION_CAT_MESH		13	/* category for Mesh */
#define DOT11_ACTION_CAT_SELFPROT	15	/* category for Mesh, self protected */
#define DOT11_ACTION_NOTIFICATION	17

#define DOT11_ACTION_RAV_STREAMING	19	/* category for Robust AV streaming:
						 * SCS, MSCS, etc.
						 */

#define DOT11_ACTION_CAT_VHT		21	/* VHT action */
#define DOT11_ACTION_CAT_S1G		22	/* S1G action */
#define DOT11_ACTION_CAT_FILS		26	/* FILS action frame */
/* HE Action frames - Draft P802.11ax D7.0 Table 9-53 Category values */
#define DOT11_ACTION_CAT_HE		30	/* HE action frame */
#define DOT11_ACTION_CAT_HEP		31	/* Protected HE action frame */
/* Protected Fine Timing action frame - Draft P802.11az/D2.5 Table 9-51 Category values */
#define DOT11_ACTION_CAT_PFT		34u	/* Protected Fine Timing action frame */
/* EHT Action frames - Draft P802.11be D1.2 Table 9-51 Category values */
#define DOT11_ACTION_CAT_EHT		36u	/* EHT action frame */
#define DOT11_ACTION_CAT_EHTP		37u	/* Protected EHT action frame */
#define DOT11_ACTION_CAT_VSP		126	/* protected vendor specific */
#define DOT11_ACTION_CAT_VS		127	/* category Vendor Specific */

/* Spectrum Management Action IDs (sec 7.4.1) */
#define DOT11_SM_ACTION_M_REQ		0	/* d11 action measurement request */
#define DOT11_SM_ACTION_M_REP		1	/* d11 action measurement response */
#define DOT11_SM_ACTION_TPC_REQ		2	/* d11 action TPC request */
#define DOT11_SM_ACTION_TPC_REP		3	/* d11 action TPC response */
#define DOT11_SM_ACTION_CHANNEL_SWITCH	4	/* d11 action channel switch */
#define DOT11_SM_ACTION_EXT_CSA		5	/* d11 extened CSA for 11n */

/* QoS action ids */
#define DOT11_QOS_ACTION_ADDTS_REQ	0	/* d11 action ADDTS request */
#define DOT11_QOS_ACTION_ADDTS_RESP	1	/* d11 action ADDTS response */
#define DOT11_QOS_ACTION_DELTS		2	/* d11 action DELTS */
#define DOT11_QOS_ACTION_SCHEDULE	3	/* d11 action schedule */
#define DOT11_QOS_ACTION_QOS_MAP	4	/* d11 action QOS map */

/* Public action ids */
#define DOT11_PUB_ACTION_BSS_COEX_MNG	0	/* 20/40 Coexistence Management action id */
#define DOT11_PUB_ACTION_CHANNEL_SWITCH	4	/* d11 action channel switch */
#define DOT11_PUB_ACTION_MP		7	/* Measurement Pilot public action id */
#define DOT11_PUB_ACTION_VENDOR_SPEC	9	/* Vendor specific */
#define DOT11_PUB_ACTION_GAS_CB_REQ	12	/* GAS Comeback Request */
#define DOT11_PUB_ACTION_FTM_REQ	32	/* FTM request */
#define DOT11_PUB_ACTION_FTM		33	/* FTM measurement */
#define DOT11_PUB_ACTION_LMR		47	/* 11az Location Measurement Report */
#define DOT11_PUB_ACTION_PSV_TB_RMR_ISTA	48 /* ISTA Passive TB Ranging Measurement Report */
#define DOT11_PUB_ACTION_PSV_TB_RMR_RSTA_PRI	49 /* Primary RSTA Broadcast Passive TB RMR */
#define DOT11_PUB_ACTION_PSV_TB_RMR_RSTA_2ND	50 /* Secondary RSTA Broadcast Passive TB RMR */

#define DOT11_PUB_ACTION_FTM_REQ_TRIGGER_START	1u	/* FTM request start trigger */
#define DOT11_PUB_ACTION_FTM_REQ_TRIGGER_STOP	0u	/* FTM request stop trigger */

/* Block Ack action types */
#define DOT11_BA_ACTION_ADDBA_REQ	0	/* ADDBA Req action frame type */
#define DOT11_BA_ACTION_ADDBA_RESP	1	/* ADDBA Resp action frame type */
#define DOT11_BA_ACTION_DELBA		2	/* DELBA action frame type */

/* ADDBA action parameters */
#define DOT11_ADDBA_PARAM_AMSDU_SUP	0x0001	/* AMSDU supported under BA */
#define DOT11_ADDBA_PARAM_POLICY_MASK	0x0002	/* policy mask(ack vs delayed) */
#define DOT11_ADDBA_PARAM_POLICY_SHIFT	1	/* policy shift */
#define DOT11_ADDBA_PARAM_TID_MASK	0x003c	/* tid mask */
#define DOT11_ADDBA_PARAM_TID_SHIFT	2	/* tid shift */
#define DOT11_ADDBA_PARAM_BSIZE_MASK	0xffc0	/* buffer size mask */
#define DOT11_ADDBA_PARAM_BSIZE_SHIFT	6	/* buffer size shift */

#define DOT11_ADDBA_POLICY_DELAYED	0	/* delayed BA policy */
#define DOT11_ADDBA_POLICY_IMMEDIATE	1	/* immediate BA policy */

/* Fast Transition action types */
#define DOT11_FT_ACTION_FT_RESERVED		0
#define DOT11_FT_ACTION_FT_REQ			1	/* FBT request - for over-the-DS FBT */
#define DOT11_FT_ACTION_FT_RES			2	/* FBT response - for over-the-DS FBT */
#define DOT11_FT_ACTION_FT_CON			3	/* FBT confirm - for OTDS with RRP */
#define DOT11_FT_ACTION_FT_ACK			4	/* FBT ack */

/* DLS action types */
#define DOT11_DLS_ACTION_REQ			0	/* DLS Request */
#define DOT11_DLS_ACTION_RESP			1	/* DLS Response */
#define DOT11_DLS_ACTION_TD			2	/* DLS Teardown */

/* Robust Audio Video streaming action types */
#define DOT11_RAV_SCS_REQ			0	/* SCS Request */
#define DOT11_RAV_SCS_RES			1	/* SCS Response */
#define DOT11_RAV_GM_REQ			2	/* Group Membership Request */
#define DOT11_RAV_GM_RES			3	/* Group Membership Response */
#define DOT11_RAV_MSCS_REQ			4	/* MSCS Request */
#define DOT11_RAV_MSCS_RES			5	/* MSCS Response */

#define DOT11_MNG_COUNTRY_ID_LEN 3

/* FILS category action types - 802.11ai D11.0 - 9.6.8.1 */
#define DOT11_FILS_ACTION_DISCOVERY		34	/* FILS Discovery */

/** DLS Request frame header */
BWL_PRE_PACKED_STRUCT struct dot11_dls_req {
	uint8 category;			/* category of action frame (2) */
	uint8 action;			/* DLS action: req (0) */
	struct ether_addr	da;	/* destination address */
	struct ether_addr	sa;	/* source address */
	uint16 cap;			/* capability */
	uint16 timeout;			/* timeout value */
	uint8 data[BCM_FLEX_ARRAY];	/* IE:support rate, extend support rate, HT cap */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_dls_req dot11_dls_req_t;
#define DOT11_DLS_REQ_LEN 18	/* Fixed length */

/** DLS response frame header */
BWL_PRE_PACKED_STRUCT struct dot11_dls_resp {
	uint8 category;			/* category of action frame (2) */
	uint8 action;			/* DLS action: req (0) */
	uint16 status;			/* status code field */
	struct ether_addr	da;	/* destination address */
	struct ether_addr	sa;	/* source address */
	uint8 data[BCM_FLEX_ARRAY];	/* optional: capability, rate ... */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_dls_resp dot11_dls_resp_t;
#define DOT11_DLS_RESP_LEN 16	/* Fixed length */

/** TIM element */
BWL_PRE_PACKED_STRUCT struct dot11_tim_ie {
	uint8 id;			/* 5, DOT11_MNG_TIM_ID	 */
	uint8 len;			/* 4 - 255 */
	uint8 dtim_count;		/* DTIM decrementing counter */
	uint8 dtim_period;		/* DTIM period */
	uint8 bitmap_control;		/* AID 0 + bitmap offset */
	uint8 pvb[BCM_FLEX_ARRAY];	/* Partial Virtual Bitmap, variable length */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tim_ie dot11_tim_ie_t;
#define DOT11_TIM_IE_FIXED_LEN	3	/* Fixed length, without id and len */
#define DOT11_TIM_IE_FIXED_TOTAL_LEN	5	/* Fixed length, with id and len */

/* Intra-Access Category Priority element */
BWL_PRE_PACKED_STRUCT struct dot11_intra_ac_prio_ie {
	uint8 id;				/* 184, DOT11_MNG_INTRA_AC_PRIO_ID */
	uint8 len;
	uint8 priority;				/* Bits 0..2 : User Priority
						 * Bit  3    : Alternate Queue
						 * Bit  4    : Drop Eligibility
						 * Bits 5..7 : Reserved
						 */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_intra_ac_prio_ie dot11_intra_ac_prio_ie_t;
#define DOT11_INTRA_AC_PRIO_IE_LEN	3u	/* Fixed length, include id and len */

/* Bitmap definitions for the User Priority Bitmap
 * Each bit in the bitmap corresponds to a user priority.
 */
#define DOT11_UP_CTRL_UP_0		0u
#define DOT11_UP_CTRL_UP_1		1u
#define DOT11_UP_CTRL_UP_2		2u
#define DOT11_UP_CTRL_UP_3		3u
#define DOT11_UP_CTRL_UP_4		4u
#define DOT11_UP_CTRL_UP_5		5u
#define DOT11_UP_CTRL_UP_6		6u
#define DOT11_UP_CTRL_UP_7		7u

/* User priority control (up_ctl)  macros */
#define DOT11_UPC_UP_BITMAP_MASK	0xFFu	/* UP bitmap mask */
#define DOT11_UPC_UP_BITMAP_SHIFT	0u	/* UP bitmap shift */
#define DOT11_UPC_UP_LIMIT_MASK		0x700u	/* UP limit mask */
#define DOT11_UPC_UP_LIMIT_SHIFT	8u	/* UP limit shift */

/* MSCS Request Types */
#define DOT11_MSCS_REQ_TYPE_ADD		0u
#define DOT11_MSCS_REQ_TYPE_REMOVE	1u
#define DOT11_MSCS_REQ_TYPE_CHANGE	2u

/** MSCS Descriptor element */
BWL_PRE_PACKED_STRUCT struct dot11_mscs_descr_ie {
	uint8  id;				/* DOT11_MNG_ID_EXT_ID (255) */
	uint8  len;
	uint8  id_ext;				/* MSCS_EXTID_MNG_DESCR_ID (88) */
	uint8  req_type;			/* MSCS request type */
	uint16 up_ctl;				/* User priority control:
						 * Bits 0..7, up_bitmap(8 bits);
						 * Bits 8..10, up_limit (3 bits)
						 * Bits 11..15 reserved (5 bits)
						 */
	uint32 stream_timeout;
	uint8  data[];
	/* optional tclas mask elements */	/* dot11_tclas_mask_ie_t */
	/* optional sub-elements */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_mscs_descr_ie dot11_mscs_descr_ie_t;
#define DOT11_MSCS_DESCR_IE_LEN		8u	/* Fixed length, exludes id and len */
#define DOT11_MSCS_DESCR_IE_HDR_LEN	10u	/* Entire descriptor header length */

/** MSCS Request frame, refer section 9.4.18.6 in the spec P802.11REVmd_D3.1 */
BWL_PRE_PACKED_STRUCT struct dot11_mscs_req {
	uint8 category;				/* ACTION_RAV_STREAMING (19) */
	uint8 robust_action;			/* action: MSCS Req (4), MSCS Res (5), etc. */
	uint8 dialog_token;			/* To identify the MSCS request and response */
	dot11_mscs_descr_ie_t mscs_descr;	/* MSCS descriptor */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_mscs_req dot11_mscs_req_t;
#define DOT11_MSCS_REQ_HDR_LEN		3u	/* Fixed length */

/** MSCS Response frame, refer section 9.4.18.7 in the spec P802.11REVmd_D3.1 */
BWL_PRE_PACKED_STRUCT struct dot11_mscs_res {
	uint8  category;			/* ACTION_RAV_STREAMING (19) */
	uint8  robust_action;			/* action: MSCS Req (4), MSCS Res (5), etc. */
	uint8  dialog_token;			/* To identify the MSCS request and response */
	uint16 status;				/* status code */
	uint8  data[];				/* optional MSCS descriptor */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_mscs_res dot11_mscs_res_t;
#define DOT11_MSCS_RES_HDR_LEN		5u	/* Fixed length */

/* MSCS subelement */
#define DOT11_MSCS_SUBELEM_ID_STATUS	1u	/* MSCS subelement ID for the status */

BWL_PRE_PACKED_STRUCT struct dot11_mscs_subelement {
	uint8 id;				/* MSCS specific subelement ID */
	uint8 len;				/* Length in bytes */
	uint8 data[];				/* Subelement specific data */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_mscs_subelement dot11_mscs_subelement_t;
#define DOT11_MSCS_DESCR_SUBELEM_IE_STATUS_LEN	2u	/* Subelement ID status length */

/* SCS Request Types */
#define DOT11_SCS_REQ_TYPE_ADD		0u
#define DOT11_SCS_REQ_TYPE_REMOVE	1u
#define DOT11_SCS_REQ_TYPE_CHANGE	2u

/* 9.4.2.316 QoS Characteristics element in Draft P802.11be_D2.2
 * Figure 9-1002as Control Info field in Draft P802.11be_D2.2
 */
#define DOT11_QOS_CHAR_DATA_RATE_LEN	3u
#define DOT11_QOS_CHAR_DELAY_BOUND_LEN	3u
BWL_PRE_PACKED_STRUCT struct dot11_qos_char_ie {
	uint8  id;						/* DOT11_MNG_ID_EXT_ID (255) */
	uint8  len;
	uint8  id_ext;						/* EXT_MNG_QOS_CHAR_ID (133) */
	uint32 ctrl_info;					/* bits 0..1   -> direction
								 * bits 2..5   -> tid
								 * bits 6..8   -> user priority
								 * bits 9..24  -> params bitmap
								 * bits 25..28 -> linkID
								 * bits 29..31 -> resreved
								 */
	uint32 min_srv_interval;				/* Minimum Service Interval in
								 * microseconds
								 */
	uint32 max_srv_interval;				/* Maximum Service Interval in
								 * microseconds
								 */
	uint8 min_data_rate[DOT11_QOS_CHAR_DATA_RATE_LEN];	/* Minimum Data Rate in kilobits
								 * per second
								 */
	uint8 delay_bound[DOT11_QOS_CHAR_DELAY_BOUND_LEN];	/* Delay Bound in microseconds */
	uint8  data[];
	/* optional Maxmimum MSDU Size */
	/* optional Service Start Time */
	/* optional Service Start Time LinkID */
	/* optional Mean Data Rate */
	/* optional Burst Size */
	/* optional MSDU Lifetime */
	/* optional MSDU Delivery Ratio */
	/* optional MSDU Count Exponent */
	/* optional Medium Time */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_qos_char_ie dot11_qos_char_ie_t;

/** SCS Descriptor element */
BWL_PRE_PACKED_STRUCT struct dot11_scs_descr_ie {
	uint8  id;				/* DOT11_MNG_SCS_DESCR_ID (185) */
	uint8  len;
	uint8  scsid;				/* SCS descriptor id */
	uint8  req_type;			/* SCS request type(0/1/2) */
	uint8  data[];
	/* optional Intra-Access Category Priority element, dot11_intrac_ac_prio_ie_t */
	/* zero or more tclas elements, dot11_tclas_ie_t */
	/* optional tclas processing element, dot11_tclas_proc_ie_t */
	/* zero or one dot11_qos_char_ie_t */
	/* zero or more SCS sub-elements, dot11_scs_subelement_t */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_scs_descr_ie dot11_scs_descr_ie_t;
#define DOT11_SCS_DESCR_IE_LEN		2u	/* Fixed length, exludes id and len */
#define DOT11_SCS_DESCR_IE_HDR_LEN	4u	/* Entire descriptor header length */

/** SCS Request frame, refer section 9.4.18.6 in the spec IEEE802.11REVmd (aka IEEE802.11-2020) */
BWL_PRE_PACKED_STRUCT struct dot11_scs_req {
	uint8 category;				/* ACTION_RAV_STREAMING (19) */
	uint8 robust_action;			/* action: SCS Req (0) */
	uint8 dialog_token;			/* To identify the SCS request and response */
	uint8 data[];				/* SCS descriptor list */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_scs_req dot11_scs_req_t;
#define DOT11_SCS_REQ_HDR_LEN		3u	/* Fixed length */

BWL_PRE_PACKED_STRUCT struct dot11_scs_status_duple {
	uint8 scsid;
	uint16 status;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_scs_status_duple dot11_scs_status_duple_t;

/** SCS Response frame, refer section 9.6.18.3 in 802.11-2020.
 * Currently, an approval(doc: 11-21/668r8) is pending in the IEEE802.11REVme.
 * Figure 9-1176 SCS Response frame Action field format in Draft P802.11be_D2.2 spec.
 */
BWL_PRE_PACKED_STRUCT struct dot11_scs_res {
	uint8  category;	/* ACTION_RAV_STREAMING (19) */
	uint8  robust_action;	/* action: SCS Res (1) */
	uint8  dialog_token;	/* To identify the SCS request and response */
	uint8  count;		/* Specifies the number of items in the scs_status_list */
	dot11_scs_status_duple_t scs_status_list[];
	/* optionally SCS descriptor list; dot11_scs_descr_ie_t scs_descr_list[] */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_scs_res dot11_scs_res_t;
#define DOT11_SCS_RES_HDR_LEN		4u	/* Fixed length */

/* SCS subelement */
BWL_PRE_PACKED_STRUCT struct dot11_scs_subelement {
	uint8 id;				/* SCS specific subelement ID */
	uint8 len;				/* Length in bytes */
	uint8 data[];				/* Subelement specific data */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_scs_subelement dot11_scs_subelement_t;
#define DOT11_SCS_DESCR_SUBELEM_IE_LEN	2u	/* Subelement IE length */

BWL_PRE_PACKED_STRUCT struct dot11_addba_req {
	uint8 category;				/* category of action frame (3) */
	uint8 action;				/* action: addba req */
	uint8 token;				/* identifier */
	uint16 addba_param_set;		/* parameter set */
	uint16 timeout;				/* timeout in seconds */
	uint16 start_seqnum;		/* starting sequence number */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_addba_req dot11_addba_req_t;
#define DOT11_ADDBA_REQ_LEN		9	/* length of addba req frame */

BWL_PRE_PACKED_STRUCT struct dot11_addba_resp {
	uint8 category;				/* category of action frame (3) */
	uint8 action;				/* action: addba resp */
	uint8 token;				/* identifier */
	uint16 status;				/* status of add request */
	uint16 addba_param_set;			/* negotiated parameter set */
	uint16 timeout;				/* negotiated timeout in seconds */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_addba_resp dot11_addba_resp_t;
#define DOT11_ADDBA_RESP_LEN		9	/* length of addba resp frame */

/* DELBA action parameters */
#define DOT11_DELBA_PARAM_INIT_MASK	0x0800	/* initiator mask */
#define DOT11_DELBA_PARAM_INIT_SHIFT	11	/* initiator shift */
#define DOT11_DELBA_PARAM_TID_MASK	0xf000	/* tid mask */
#define DOT11_DELBA_PARAM_TID_SHIFT	12	/* tid shift */

BWL_PRE_PACKED_STRUCT struct dot11_delba {
	uint8 category;				/* category of action frame (3) */
	uint8 action;				/* action: addba req */
	uint16 delba_param_set;			/* paarmeter set */
	uint16 reason;				/* reason for dellba */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_delba dot11_delba_t;
#define DOT11_DELBA_LEN			6	/* length of delba frame */

/* SA Query action field value */
#define SA_QUERY_REQUEST		0
#define SA_QUERY_RESPONSE		1

/* MLME Enumerations */
#define DOT11_BSSTYPE_INFRASTRUCTURE		0	/* d11 infrastructure */
#define DOT11_BSSTYPE_INDEPENDENT		1	/* d11 independent */
#define DOT11_BSSTYPE_ANY			2	/* d11 any BSS type */
#define DOT11_BSSTYPE_MESH			3	/* d11 Mesh */
#define DOT11_SCANTYPE_ACTIVE			0	/* d11 scan active */
#define DOT11_SCANTYPE_PASSIVE			1	/* d11 scan passive */

/* 802.11 A PHY constants */
#define APHY_SLOT_TIME          9       /* APHY slot time */
#define APHY_SIFS_TIME          16      /* APHY SIFS time */
#define APHY_DIFS_TIME          (APHY_SIFS_TIME + (2 * APHY_SLOT_TIME))  /* APHY DIFS time */
#define APHY_PREAMBLE_TIME      16      /* APHY preamble time */
#define APHY_SIGNAL_TIME        4       /* APHY signal time */
#define APHY_SYMBOL_TIME        4       /* APHY symbol time */
#define APHY_SERVICE_NBITS      16      /* APHY service nbits */
#define APHY_TAIL_NBITS         6       /* APHY tail nbits */
#define APHY_CWMIN              15      /* APHY cwmin */
#define APHY_PHYHDR_DUR		20	/* APHY PHY Header Duration */

/* 802.11 B PHY constants */
#define BPHY_SLOT_TIME          20      /* BPHY slot time */
#define BPHY_SIFS_TIME          10      /* BPHY SIFS time */
#define BPHY_DIFS_TIME          50      /* BPHY DIFS time */
#define BPHY_PLCP_TIME          192     /* BPHY PLCP time */
#define BPHY_PLCP_SHORT_TIME    96      /* BPHY PLCP short time */
#define BPHY_CWMIN              31      /* BPHY cwmin */
#define BPHY_SHORT_PHYHDR_DUR	96	/* BPHY Short PHY Header Duration */
#define BPHY_LONG_PHYHDR_DUR	192	/* BPHY Long PHY Header Duration */

/* 802.11 G constants */
#define DOT11_OFDM_SIGNAL_EXTENSION	6	/* d11 OFDM signal extension */

#define PHY_CWMAX		1023	/* PHY cwmax */

#define	DOT11_MAXNUMFRAGS	16	/* max # fragments per MSDU */

/** Vendor IE structure */
BWL_PRE_PACKED_STRUCT struct vndr_ie {
	uchar id;
	uchar len;
	uchar oui [3];
	uchar data [1];   /* Variable size data */
} BWL_POST_PACKED_STRUCT;
typedef struct vndr_ie vndr_ie_t;

#define VNDR_IE_HDR_LEN		2u	/* id + len field */
#define VNDR_IE_MIN_LEN		3u	/* size of the oui field */
#define VNDR_IE_FIXED_LEN	(VNDR_IE_HDR_LEN + VNDR_IE_MIN_LEN)

#define VNDR_IE_MAX_LEN		255u	/* vendor IE max length, without ID and len */

/* HT/AMPDU specific define */
#define AMPDU_MAX_MPDU_DENSITY		7	/* max mpdu density; in 1/4 usec units */
#define AMPDU_DENSITY_NONE		0	/* No density requirement */
#define AMPDU_DENSITY_1over4_US		1	/* 1/4 us density */
#define AMPDU_DENSITY_1over2_US		2	/* 1/2 us density */
#define AMPDU_DENSITY_1_US		3	/*   1 us density */
#define AMPDU_DENSITY_2_US		4	/*   2 us density */
#define AMPDU_DENSITY_4_US		5	/*   4 us density */
#define AMPDU_DENSITY_8_US		6	/*   8 us density */
#define AMPDU_DENSITY_16_US		7	/*  16 us density */
#define AMPDU_RX_FACTOR_8K		0	/* max rcv ampdu len (8kb) */
#define AMPDU_RX_FACTOR_16K		1	/* max rcv ampdu len (16kb) */
#define AMPDU_RX_FACTOR_32K		2	/* max rcv ampdu len (32kb) */
#define AMPDU_RX_FACTOR_64K		3	/* max rcv ampdu len (64kb) */

/* AMPDU RX factors for VHT rates */
#define AMPDU_RX_FACTOR_128K		4	/* max rcv ampdu len (128kb) */
#define AMPDU_RX_FACTOR_256K		5	/* max rcv ampdu len (256kb) */
#define AMPDU_RX_FACTOR_512K		6	/* max rcv ampdu len (512kb) */
#define AMPDU_RX_FACTOR_1024K		7	/* max rcv ampdu len (1024kb) */

#define AMPDU_RX_FACTOR_BASE		8*1024	/* ampdu factor base for rx len */
#define AMPDU_RX_FACTOR_BASE_PWR	13	/* ampdu factor base for rx len in power of 2 */
#define AMPDU_DELIMITER_LEN		4u	/* length of ampdu delimiter */
#define AMPDU_DELIMITER_LEN_MAX		63	/* max length of ampdu delimiter(enforced in HW) */

/* RSN authenticated key managment suite */
#define RSN_AKM_NONE			0u	/* None (IBSS) */
#define RSN_AKM_UNSPECIFIED		1u	/* Over 802.1x */
#define RSN_AKM_PSK			2u	/* Pre-shared Key */
#define RSN_AKM_FBT_1X			3u	/* Fast Bss transition using 802.1X */
#define RSN_AKM_FBT_PSK			4u	/* Fast Bss transition using Pre-shared Key */
/* RSN_AKM_MFP_1X and RSN_AKM_MFP_PSK are not used any more
 * Just kept here to avoid build issue in BISON/CARIBOU branch
 */
#define RSN_AKM_MFP_1X			5u	/* SHA256 key derivation, using 802.1X */
#define RSN_AKM_MFP_PSK			6u	/* SHA256 key derivation, using Pre-shared Key */
#define RSN_AKM_SHA256_1X		5u	/* SHA256 key derivation, using 802.1X */
#define RSN_AKM_SHA256_PSK		6u	/* SHA256 key derivation, using Pre-shared Key */
#define RSN_AKM_TPK			7u	/* TPK(TDLS Peer Key) handshake */
#define RSN_AKM_SAE_PSK			8u      /* AKM for SAE with 4-way handshake */
#define RSN_AKM_SAE_FBT			9u       /* AKM for SAE with FBT */
#define RSN_AKM_SUITEB_SHA256_1X	11u	/* Suite B SHA256 */
#define RSN_AKM_SUITEB_SHA384_1X	12u	/* Suite B-192 SHA384 */
#define RSN_AKM_FBT_SHA384_1X		13u	/* FBT SHA384 */
#define RSN_AKM_FILS_SHA256		14u	/* SHA256 key derivation, using FILS */
#define RSN_AKM_FILS_SHA384		15u	/* SHA384 key derivation, using FILS */
#define RSN_AKM_FBT_SHA256_FILS		16u
#define RSN_AKM_FBT_SHA384_FILS		17u
#define RSN_AKM_OWE			18u	/* RFC 8110  OWE */
#define RSN_AKM_FBT_SHA384_PSK		19u
#define RSN_AKM_PSK_SHA384		20u
#define RSN_AKM_PASN			21u	/* Pre-Association Security Negotiation */
#define RSN_AKM_FBT_SHA384_1X_UNRSTD	22u	/* Unrestricted FBT over 802.1X */
#define RSN_AKM_SHA384_1X		23u	/* Over 802.1X SHA384 */
#define RSN_AKM_SAE_EXT_PSK		24u     /* AKM for SAE (variable length PMK) */
#define RSN_AKM_SAE_EXT_FBT		25u     /* AKM for SAE (variable length PMK) with FBT */
/* OSEN authenticated key managment suite */
#define OSEN_AKM_UNSPECIFIED	RSN_AKM_UNSPECIFIED	/* Over 802.1x */
/* WFA DPP RSN authenticated key managment */
#define RSN_AKM_DPP			02u	/* DPP RSN */

/* Key related defines */
#define DOT11_MAX_DEFAULT_KEYS	4	/* number of default keys */
#define DOT11_MAX_IGTK_KEYS		2
#define DOT11_MAX_BIGTK_KEYS		2
#define DOT11_MAX_KEY_SIZE	32	/* max size of any key */
#define DOT11_MAX_IV_SIZE	16	/* max size of any IV */
#define DOT11_EXT_IV_FLAG	(1<<5)	/* flag to indicate IV is > 4 bytes */
#define DOT11_FTM_PFT_IV_FLAG   0x10    /* Protected Fine Timing frame flag in IV */
#define DOT11_WPA_KEY_RSC_LEN   8       /* WPA RSC key len */
#define WEP1_KEY_SIZE		5	/* max size of any WEP key */
#define WEP1_KEY_HEX_SIZE	10	/* size of WEP key in hex. */
#define WEP128_KEY_SIZE		13	/* max size of any WEP key */
#define WEP128_KEY_HEX_SIZE	26	/* size of WEP key in hex. */
#define TKIP_MIC_SIZE		8	/* size of TKIP MIC */
#define TKIP_EOM_SIZE		7	/* max size of TKIP EOM */
#define TKIP_EOM_FLAG		0x5a	/* TKIP EOM flag byte */
#define TKIP_KEY_SIZE		32	/* size of any TKIP key, includs MIC keys */
#define TKIP_TK_SIZE		16
#define TKIP_MIC_KEY_SIZE	8
#define TKIP_MIC_AUTH_TX	16	/* offset to Authenticator MIC TX key */
#define TKIP_MIC_AUTH_RX	24	/* offset to Authenticator MIC RX key */
#define TKIP_MIC_SUP_RX		TKIP_MIC_AUTH_TX	/* offset to Supplicant MIC RX key */
#define TKIP_MIC_SUP_TX		TKIP_MIC_AUTH_RX	/* offset to Supplicant MIC TX key */
#define AES_KEY_SIZE		16	/* size of AES key */
#define AES_MIC_SIZE		8	/* size of AES MIC */
#define BIP_KEY_SIZE		16	/* size of BIP key */
#define BIP_MIC_SIZE		8   /* sizeof BIP MIC */

#define AES_GCM_MIC_SIZE	16	/* size of MIC for 128-bit GCM - .11adD9 */

#define AES256_KEY_SIZE		32	/* size of AES 256 key - .11acD5 */
#define AES256_MIC_SIZE		16	/* size of MIC for 256 bit keys, incl BIP */

#define TIE_TYPE_RESERVED		0
#define TIE_TYPE_REASSOC_DEADLINE	1
#define TIE_TYPE_KEY_LIEFTIME		2
#define TIE_TYPE_ASSOC_COMEBACK		3
BWL_PRE_PACKED_STRUCT struct dot11_timeout_ie {
	uint8 id;
	uint8 len;
	uint8 type;		/* timeout interval type */
	uint32 value;		/* timeout interval value */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_timeout_ie dot11_timeout_ie_t;

/** GTK ie */
BWL_PRE_PACKED_STRUCT struct dot11_gtk_ie {
	uint8 id;
	uint8 len;
	uint16 key_info;
	uint8 key_len;
	uint8 rsc[8];
	uint8 data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_gtk_ie dot11_gtk_ie_t;

#define BSSID_INVALID           "\x00\x00\x00\x00\x00\x00"
#define BSSID_BROADCAST         "\xFF\xFF\xFF\xFF\xFF\xFF"

/* AP Location Public ID Info encoding */
#define PUBLIC_ID_URI_FQDN_SE_ID		0
/* URI/FQDN Descriptor field values */
#define LOCATION_ENCODING_HELD			1
#define LOCATION_ENCODING_SUPL			2
#define URI_FQDN_SIZE					255

/* Short SSID list Extended Capabilities element */
BWL_PRE_PACKED_STRUCT struct short_ssid_list_ie {
	uint8 id;
	uint8 len;
	uint8 id_ext;
	uint8 data[BCM_FLEX_ARRAY];    /* Capabilities Information */
} BWL_POST_PACKED_STRUCT;

typedef struct short_ssid_list_ie short_ssid_list_ie_t;
#define SHORT_SSID_LIST_IE_FIXED_LEN	3	/* SHORT SSID LIST IE LENGTH */

/** IEEE 802.11 Annex E */
typedef enum {
	DOT11_2GHZ_20MHZ_CLASS_12	= 81,	/* Ch 1-11 */
	DOT11_5GHZ_20MHZ_CLASS_1	= 115,	/* Ch 36-48 */
	DOT11_5GHZ_20MHZ_CLASS_2_DFS	= 118,	/* Ch 52-64 */
	DOT11_5GHZ_20MHZ_CLASS_3	= 124,	/* Ch 149-161 */
	DOT11_5GHZ_20MHZ_CLASS_4_DFS	= 121,	/* Ch 100-140 */
	DOT11_5GHZ_20MHZ_CLASS_5	= 125,	/* Ch 149-165 */
	DOT11_5GHZ_40MHZ_CLASS_22	= 116,	/* Ch 36-44,   lower */
	DOT11_5GHZ_40MHZ_CLASS_23_DFS	= 119,	/* Ch 52-60,   lower */
	DOT11_5GHZ_40MHZ_CLASS_24_DFS	= 122,	/* Ch 100-132, lower */
	DOT11_5GHZ_40MHZ_CLASS_25	= 126,	/* Ch 149-157, lower */
	DOT11_5GHZ_40MHZ_CLASS_27	= 117,	/* Ch 40-48,   upper */
	DOT11_5GHZ_40MHZ_CLASS_28_DFS	= 120,	/* Ch 56-64,   upper */
	DOT11_5GHZ_40MHZ_CLASS_29_DFS	= 123,	/* Ch 104-136, upper */
	DOT11_5GHZ_40MHZ_CLASS_30	= 127,	/* Ch 153-161, upper */
	DOT11_2GHZ_40MHZ_CLASS_32	= 83,	/* Ch 1-7,     lower */
	DOT11_2GHZ_40MHZ_CLASS_33	= 84,	/* Ch 5-11,    upper */
} dot11_op_class_t;

/* QoS map */
#define QOS_MAP_FIXED_LENGTH	(8 * 2)	/* DSCP ranges fixed with 8 entries */

/* Supported Operating Classes element */
BWL_PRE_PACKED_STRUCT struct supp_op_classes_ie {
	uint8 id;
	uint8 len;
	uint8 cur_op_class;
	uint8 op_classes[];    /* Supported Operating Classes */
} BWL_POST_PACKED_STRUCT;
typedef struct supp_op_classes_ie supp_op_classes_ie_t;

/* Requset IE */
BWL_PRE_PACKED_STRUCT struct dot11_req_ie {
	uint8	id;		/* DOT11_MNG_REQUEST_ID */
	uint8	len;
	uint8	ids[];		/* requested element IDs */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_req_ie dot11_req_ie_t;

/* Extended Requset IE */
BWL_PRE_PACKED_STRUCT struct dot11_ext_req_ie {
	uint8	id;		/* DOT11_MNG_ID_EXT_ID */
	uint8	len;
	uint8	id_ext;		/* element ID extension */
	uint8	reqd_id;	/* requested element ID */
	uint8	id_exts[];	/* requested element ID extensions */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_ext_req_ie dot11_ext_req_ie_t;

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

/* Include 802.11*.h here for wlioctl.h compatibility.
 * Move them to wlioctl.h when possible.
 */
#include <802.11k.h>
#include <802.11n.h>
#include <802.11v.h>
#include <802.11ac.h>
#include <802.11az.h>

/* Include 802.11*.h here for nan component shared by older branches.
 * Move them to where their contents are referenced when possible.
 */
#include <802.11w.h>
#include <802.11wfa.h>

/* Include 802.11wapi.h for components/bcmcrypto/src/sms4.c in some older branches.
 * Move the inclusion there when possible.
 */
#include <802.11wapi.h>

/* Include 802.11brcm.h for components/bcmutils/src/bcmevent.c in some older branches.
 * Move the inclusion there when possible.
 */
#include <802.11brcm.h>

/* Include 802.11cust.h here for src/wl/sys/wlc_act_frame.c in some older branches.
 * Move it there when possible.
 */
#include <802.11cust.h>

/* Include 802.11u/owe.h here for host compatibility.
 * Move them there when possible.
 */
#include <802.11u.h>
#include <802.11z.h>
#include <802.11owe.h>

#endif /* _802_11_H_ */
