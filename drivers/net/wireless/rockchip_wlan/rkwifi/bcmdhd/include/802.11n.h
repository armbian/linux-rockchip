/*
 * Fundamental types and constants relating to 802.11n -
 * "Enhancements for Higher Throughput"
 *
 * HT - Higher Throughput
 * OBSS - Overlapping BSS
 * EXTCH/EXT_CH - Extension Channel
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

#ifndef _802_11n_h_
#define _802_11n_h_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

#ifndef _NET_ETHERNET_H_
#include <ethernet.h>
#endif

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

BWL_PRE_PACKED_STRUCT struct dot11_action_ht_ch_width {
	uint8	category;
	uint8	action;
	uint8	ch_width;
} BWL_POST_PACKED_STRUCT;

BWL_PRE_PACKED_STRUCT struct dot11_action_ht_mimops {
	uint8	category;
	uint8	action;
	uint8	control;
} BWL_POST_PACKED_STRUCT;

/* 802.11 BRCM "Compromise" Pre N constants */
#define PREN_PREAMBLE		24	/* green field preamble time */
#define PREN_MM_EXT		12	/* extra mixed mode preamble time */
#define PREN_PREAMBLE_EXT	4	/* extra preamble (multiply by unique_streams-1) */

/* 802.11N PHY constants */
#define RIFS_11N_TIME		2	/* NPHY RIFS time */

/* 802.11 HT PLCP format 802.11n-2009, sec 20.3.9.4.3
 * HT-SIG is composed of two 24 bit parts, HT-SIG1 and HT-SIG2
 */
/* HT-SIG1 */
#define HT_SIG1_MCS_MASK        0x00007F
#define HT_SIG1_CBW             0x000080
#define HT_SIG1_CBW_SHIFT       7u
#define HT_SIG1_HT_LENGTH       0xFFFF00
#define HT_SIG1_HT_LENGTH_SHIFT 8u

/* HT-SIG2 */
#define HT_SIG2_SMOOTHING       0x000001
#define HT_SIG2_NOT_SOUNDING    0x000002
#define HT_SIG2_RESERVED        0x000004
#define HT_SIG2_AGGREGATION     0x000008
#define HT_SIG2_STBC_MASK       0x000030
#define HT_SIG2_STBC_SHIFT      4
#define HT_SIG2_FEC_CODING      0x000040
#define HT_SIG2_SHORT_GI        0x000080
#define HT_SIG2_ESS_MASK        0x000300
#define HT_SIG2_ESS_SHIFT       8
#define HT_SIG2_CRC             0x03FC00
#define HT_SIG2_TAIL            0x1C0000

/* HT Timing-related parameters (802.11-2012, sec 20.3.6) */
#define HT_T_LEG_PREAMBLE      16
#define HT_T_L_SIG              4
#define HT_T_SIG                8
#define HT_T_LTF1               4
#define HT_T_GF_LTF1            8
#define HT_T_LTFs               4
#define HT_T_STF                4
#define HT_T_GF_STF             8
#define HT_T_SYML               4

#define HT_N_SERVICE           16       /* bits in SERVICE field */
#define HT_N_TAIL               6       /* tail bits per BCC encoder */

/* ************* HT definitions. ************* */
#define MCSSET_LEN	16	/* 16-bits per 8-bit set to give 128-bits bitmap of MCS Index */
#define MAX_MCS_NUM	(128)	/* max mcs number = 128 */
#define BASIC_HT_MCS	0xFFu	/* HT MCS supported rates */

BWL_PRE_PACKED_STRUCT struct ht_cap_ie {
	uint16	cap;
	uint8	params;
	uint8	supp_mcs[MCSSET_LEN];
	uint16	ext_htcap;
	uint32	txbf_cap;
	uint8	as_cap;
} BWL_POST_PACKED_STRUCT;
typedef struct ht_cap_ie ht_cap_ie_t;

BWL_PRE_PACKED_STRUCT struct dot11_ht_cap_ie {
	uint8	id;
	uint8	len;
	ht_cap_ie_t ht_cap;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_ht_cap_ie dot11_ht_cap_ie_t;

#define HT_CAP_IE_LEN		26	/* HT capability len (based on .11n d2.0) */

#define HT_CAP_LDPC_CODING	0x0001	/* Support for rx of LDPC coded pkts */
#define HT_CAP_40MHZ		0x0002	/* FALSE:20Mhz, TRUE:20/40MHZ supported */
#define HT_CAP_MIMO_PS_MASK	0x000C	/* Mimo PS mask */
#define HT_CAP_MIMO_PS_SHIFT	0x0002	/* Mimo PS shift */
#define HT_CAP_MIMO_PS_OFF	0x0003	/* Mimo PS, no restriction */
#define HT_CAP_MIMO_PS_RTS	0x0001	/* Mimo PS, send RTS/CTS around MIMO frames */
#define HT_CAP_MIMO_PS_ON	0x0000	/* Mimo PS, MIMO disallowed */
#define HT_CAP_GF		0x0010	/* Greenfield preamble support */
#define HT_CAP_SHORT_GI_20	0x0020	/* 20MHZ short guard interval support */
#define HT_CAP_SHORT_GI_40	0x0040	/* 40Mhz short guard interval support */
#define HT_CAP_TX_STBC		0x0080	/* Tx STBC support */
#define HT_CAP_RX_STBC_MASK	0x0300	/* Rx STBC mask */
#define HT_CAP_RX_STBC_SHIFT	8	/* Rx STBC shift */
#define HT_CAP_DELAYED_BA	0x0400	/* delayed BA support */
#define HT_CAP_MAX_AMSDU	0x0800	/* Max AMSDU size in bytes , 0=3839, 1=7935 */

#define HT_CAP_DSSS_CCK		0x1000	/* DSSS/CCK supported by the BSS */
#define HT_CAP_PSMP		0x2000	/* Power Save Multi Poll support */
#define HT_CAP_40MHZ_INTOLERANT 0x4000	/* 40MHz Intolerant */
#define HT_CAP_LSIG_TXOP	0x8000	/* L-SIG TXOP protection support */

#define HT_CAP_RX_STBC_NO		0x0	/* no rx STBC support */
#define HT_CAP_RX_STBC_ONE_STREAM	0x1	/* rx STBC support of 1 spatial stream */
#define HT_CAP_RX_STBC_TWO_STREAM	0x2	/* rx STBC support of 1-2 spatial streams */
#define HT_CAP_RX_STBC_THREE_STREAM	0x3	/* rx STBC support of 1-3 spatial streams */

#define HT_CAP_TXBF_CAP_IMPLICIT_TXBF_RX	0x1
#define HT_CAP_TXBF_CAP_NDP_RX			0x8
#define HT_CAP_TXBF_CAP_NDP_TX			0x10
#define HT_CAP_TXBF_CAP_EXPLICIT_CSI		0x100
#define HT_CAP_TXBF_CAP_EXPLICIT_NC_STEERING	0x200
#define HT_CAP_TXBF_CAP_EXPLICIT_C_STEERING	0x400
#define HT_CAP_TXBF_CAP_EXPLICIT_CSI_FB_MASK	0x1800
#define HT_CAP_TXBF_CAP_EXPLICIT_CSI_FB_SHIFT	11
#define HT_CAP_TXBF_CAP_EXPLICIT_NC_FB_MASK	0x6000
#define HT_CAP_TXBF_CAP_EXPLICIT_NC_FB_SHIFT	13
#define HT_CAP_TXBF_CAP_EXPLICIT_C_FB_MASK	0x18000
#define HT_CAP_TXBF_CAP_EXPLICIT_C_FB_SHIFT	15
#define HT_CAP_TXBF_CAP_CSI_BFR_ANT_SHIFT	19
#define HT_CAP_TXBF_CAP_NC_BFR_ANT_SHIFT	21
#define HT_CAP_TXBF_CAP_C_BFR_ANT_SHIFT		23
#define HT_CAP_TXBF_CAP_C_BFR_ANT_MASK		0x1800000

#define HT_CAP_TXBF_CAP_CHAN_ESTIM_SHIFT	27
#define HT_CAP_TXBF_CAP_CHAN_ESTIM_MASK		0x18000000

#define HT_CAP_TXBF_FB_TYPE_NONE		0
#define HT_CAP_TXBF_FB_TYPE_DELAYED		1
#define HT_CAP_TXBF_FB_TYPE_IMMEDIATE		2
#define HT_CAP_TXBF_FB_TYPE_BOTH		3

#define HT_CAP_TX_BF_CAP_EXPLICIT_CSI_FB_MASK	0x400
#define HT_CAP_TX_BF_CAP_EXPLICIT_CSI_FB_SHIFT	10
#define HT_CAP_TX_BF_CAP_EXPLICIT_COMPRESSED_FB_MASK 0x18000
#define HT_CAP_TX_BF_CAP_EXPLICIT_COMPRESSED_FB_SHIFT 15

#define HT_CAP_MCS_FLAGS_SUPP_BYTE 12 /* byte offset in HT Cap Supported MCS for various flags */
#define HT_CAP_MCS_RX_8TO15_BYTE_OFFSET		1
#define HT_CAP_MCS_FLAGS_TX_RX_UNEQUAL		0x02
#define HT_CAP_MCS_FLAGS_MAX_SPATIAL_STREAM_MASK 0x0C

#define HT_MAX_AMSDU			7935	/* max amsdu size (bytes) per the HT spec */
#define HT_MIN_AMSDU			3835	/* min amsdu size (bytes) per the HT spec */

#define HT_PARAMS_RX_FACTOR_MASK	0x03	/* ampdu rcv factor mask */
#define HT_PARAMS_DENSITY_MASK		0x1C	/* ampdu density mask */
#define HT_PARAMS_DENSITY_SHIFT		2	/* ampdu density shift */

#define HT_CAP_EXT_PCO			0x0001
#define HT_CAP_EXT_PCO_TTIME_MASK	0x0006
#define HT_CAP_EXT_PCO_TTIME_SHIFT	1
#define HT_CAP_EXT_MCS_FEEDBACK_MASK	0x0300
#define HT_CAP_EXT_MCS_FEEDBACK_SHIFT	8
#define HT_CAP_EXT_HTC			0x0400
#define HT_CAP_EXT_RD_RESP		0x0800

/** 'ht_add' is called 'HT Operation' information element in the 802.11 standard */
BWL_PRE_PACKED_STRUCT struct ht_add_ie {
	uint8	ctl_ch;			/* control channel number */
	uint8	byte1;			/* ext ch,rec. ch. width, RIFS support */
	uint16	opmode;			/* operation mode */
	uint16	misc_bits;		/* misc bits */
	uint8	basic_mcs[MCSSET_LEN];	/* required MCS set */
} BWL_POST_PACKED_STRUCT;
typedef struct ht_add_ie ht_add_ie_t;

#define HT_ADD_IE_LEN	22	/* HT capability len (based on .11n d1.0) */

/* byte1 defn's */
#define HT_BW_ANY		0x04	/* set, STA can use 20 or 40MHz */
#define HT_RIFS_PERMITTED	0x08	/* RIFS allowed */

/* opmode defn's */
#define HT_OPMODE_MASK	        0x0003	/* protection mode mask */
#define HT_OPMODE_SHIFT		0	/* protection mode shift */
#define HT_OPMODE_PURE		0x0000	/* protection mode PURE */
#define HT_OPMODE_OPTIONAL	0x0001	/* protection mode optional */
#define HT_OPMODE_HT20IN40	0x0002	/* protection mode 20MHz HT in 40MHz BSS */
#define HT_OPMODE_MIXED	0x0003	/* protection mode Mixed Mode */
#define HT_OPMODE_NONGF	0x0004	/* protection mode non-GF */
#define DOT11N_TXBURST		0x0008	/* Tx burst limit */
#define DOT11N_OBSS_NONHT	0x0010	/* OBSS Non-HT STA present */
#define HT_OPMODE_CCFS2_MASK	0x1fe0	/* Channel Center Frequency Segment 2 mask */
#define HT_OPMODE_CCFS2_SHIFT	5	/* Channel Center Frequency Segment 2 shift */

/* misc_bites defn's */
#define HT_BASIC_STBC_MCS	0x007f	/* basic STBC MCS */
#define HT_DUAL_STBC_PROT	0x0080	/* Dual STBC Protection */
#define HT_SECOND_BCN		0x0100	/* Secondary beacon support */
#define HT_LSIG_TXOP		0x0200	/* L-SIG TXOP Protection full support */
#define HT_PCO_ACTIVE		0x0400	/* PCO active */
#define HT_PCO_PHASE		0x0800	/* PCO phase */
#define HT_DUALCTS_PROTECTION	0x0080	/* DUAL CTS protection needed */

/* Tx Burst Limits */
#define DOT11N_2G_TXBURST_LIMIT	6160	/* 2G band Tx burst limit per 802.11n Draft 1.10 (usec) */
#define DOT11N_5G_TXBURST_LIMIT	3080	/* 5G band Tx burst limit per 802.11n Draft 1.10 (usec) */

/* Macros for opmode */
#define GET_HT_OPMODE(add_ie)		((ltoh16_ua(&add_ie->opmode) & HT_OPMODE_MASK) \
					>> HT_OPMODE_SHIFT)
#define HT_MIXEDMODE_PRESENT(add_ie)	((ltoh16_ua(&add_ie->opmode) & HT_OPMODE_MASK) \
					== HT_OPMODE_MIXED)	/* mixed mode present */
#define HT_HT20_PRESENT(add_ie)	((ltoh16_ua(&add_ie->opmode) & HT_OPMODE_MASK) \
					== HT_OPMODE_HT20IN40)	/* 20MHz HT present */
#define HT_OPTIONAL_PRESENT(add_ie)	((ltoh16_ua(&add_ie->opmode) & HT_OPMODE_MASK) \
					== HT_OPMODE_OPTIONAL)	/* Optional protection present */
#define HT_USE_PROTECTION(add_ie)	(HT_HT20_PRESENT((add_ie)) || \
					HT_MIXEDMODE_PRESENT((add_ie))) /* use protection */
#define HT_NONGF_PRESENT(add_ie)	((ltoh16_ua(&add_ie->opmode) & HT_OPMODE_NONGF) \
					== HT_OPMODE_NONGF)	/* non-GF present */
#define DOT11N_TXBURST_PRESENT(add_ie)	((ltoh16_ua(&add_ie->opmode) & DOT11N_TXBURST) \
					== DOT11N_TXBURST)	/* Tx Burst present */
#define DOT11N_OBSS_NONHT_PRESENT(add_ie)	((ltoh16_ua(&add_ie->opmode) & DOT11N_OBSS_NONHT) \
					== DOT11N_OBSS_NONHT)	/* OBSS Non-HT present */
#define HT_OPMODE_CCFS2_GET(add_ie)	((ltoh16_ua(&(add_ie)->opmode) & HT_OPMODE_CCFS2_MASK) \
					>> HT_OPMODE_CCFS2_SHIFT)	/* get CCFS2 */
#define HT_OPMODE_CCFS2_SET(add_ie, ccfs2)	do { /* set CCFS2 */ \
	(add_ie)->opmode &= htol16(~HT_OPMODE_CCFS2_MASK); \
	(add_ie)->opmode |= htol16(((ccfs2) << HT_OPMODE_CCFS2_SHIFT) & HT_OPMODE_CCFS2_MASK); \
} while (0)

/* Macros for HT MCS field access */
#define HT_CAP_MCS_BITMASK(supp_mcs)                 \
	((supp_mcs)[HT_CAP_MCS_RX_8TO15_BYTE_OFFSET])
#define HT_CAP_MCS_TX_RX_UNEQUAL(supp_mcs)          \
	((supp_mcs)[HT_CAP_MCS_FLAGS_SUPP_BYTE] & HT_CAP_MCS_FLAGS_TX_RX_UNEQUAL)
#define HT_CAP_MCS_TX_STREAM_SUPPORT(supp_mcs)          \
		((supp_mcs)[HT_CAP_MCS_FLAGS_SUPP_BYTE] & HT_CAP_MCS_FLAGS_MAX_SPATIAL_STREAM_MASK)

BWL_PRE_PACKED_STRUCT struct obss_params {
	uint16	passive_dwell;
	uint16	active_dwell;
	uint16	bss_widthscan_interval;
	uint16	passive_total;
	uint16	active_total;
	uint16	chanwidth_transition_dly;
	uint16	activity_threshold;
} BWL_POST_PACKED_STRUCT;
typedef struct obss_params obss_params_t;

BWL_PRE_PACKED_STRUCT struct dot11_obss_ie {
	uint8	id;
	uint8	len;
	obss_params_t obss_params;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_obss_ie dot11_obss_ie_t;
#define DOT11_OBSS_SCAN_IE_LEN	sizeof(obss_params_t)	/* HT OBSS len (based on 802.11n d3.0) */

/* HT control field */
#define HT_CTRL_LA_TRQ		0x00000002	/* sounding request */
#define HT_CTRL_LA_MAI		0x0000003C	/* MCS request or antenna selection indication */
#define HT_CTRL_LA_MAI_SHIFT	2
#define HT_CTRL_LA_MAI_MRQ	0x00000004	/* MCS request */
#define HT_CTRL_LA_MAI_MSI	0x00000038	/* MCS request sequence identifier */
#define HT_CTRL_LA_MFSI		0x000001C0	/* MFB sequence identifier */
#define HT_CTRL_LA_MFSI_SHIFT	6
#define HT_CTRL_LA_MFB_ASELC	0x0000FE00	/* MCS feedback, antenna selection command/data */
#define HT_CTRL_LA_MFB_ASELC_SH	9
#define HT_CTRL_LA_ASELC_CMD	0x00000C00	/* ASEL command */
#define HT_CTRL_LA_ASELC_DATA	0x0000F000	/* ASEL data */
#define HT_CTRL_CAL_POS		0x00030000	/* Calibration position */
#define HT_CTRL_CAL_SEQ		0x000C0000	/* Calibration sequence */
#define HT_CTRL_CSI_STEERING	0x00C00000	/* CSI/Steering */
#define HT_CTRL_CSI_STEER_SHIFT	22
#define HT_CTRL_CSI_STEER_NFB	0		/* no fedback required */
#define HT_CTRL_CSI_STEER_CSI	1		/* CSI, H matrix */
#define HT_CTRL_CSI_STEER_NCOM	2		/* non-compressed beamforming */
#define HT_CTRL_CSI_STEER_COM	3		/* compressed beamforming */
#define HT_CTRL_NDP_ANNOUNCE	0x01000000	/* NDP announcement */
#define HT_CTRL_AC_CONSTRAINT	0x40000000	/* AC Constraint */
#define HT_CTRL_RDG_MOREPPDU	0x80000000	/* RDG/More PPDU */

/* HT action ids */
#define DOT11_ACTION_ID_HT_CH_WIDTH	0	/* notify channel width action id */
#define DOT11_ACTION_ID_HT_MIMO_PS	1	/* mimo ps action id */

/**
 * Extension Channel Offset IE: 802.11n-D1.0 spec. added sideband
 * offset for 40MHz operation.  The possible 3 values are:
 * 1 = above control channel
 * 3 = below control channel
 * 0 = no extension channel
 */
BWL_PRE_PACKED_STRUCT struct dot11_extch {
	uint8	id;		/* IE ID, 62, DOT11_MNG_EXT_CHANNEL_OFFSET */
	uint8	len;		/* IE length */
	uint8	extch;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_extch dot11_extch_ie_t;

#define DOT11_EXTCH_IE_LEN	1
#define DOT11_EXT_CH_MASK	0x03	/* extension channel mask */
#define DOT11_EXT_CH_UPPER	0x01	/* ext. ch. on upper sb */
#define DOT11_EXT_CH_LOWER	0x03	/* ext. ch. on lower sb */
#define DOT11_EXT_CH_NONE	0x00	/* no extension ch.  */

/** 11n Extended Channel Switch IE data structure */
BWL_PRE_PACKED_STRUCT struct dot11_ext_csa {
	uint8 id;	/* id DOT11_MNG_EXT_CSA_ID */
	uint8 len;	/* length of IE */
	struct dot11_csa_body b;	/* body of the ie */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_ext_csa dot11_ext_csa_ie_t;
#define DOT11_EXT_CSA_IE_LEN	4	/* length of extended channel switch IE body */

BWL_PRE_PACKED_STRUCT struct dot11_action_ext_csa {
	uint8	category;
	uint8	action;
	dot11_ext_csa_ie_t chan_switch_ie;	/* for switch IE */
} BWL_POST_PACKED_STRUCT;

BWL_PRE_PACKED_STRUCT struct dot11_obss_coex {
	uint8	id;
	uint8	len;
	uint8	info;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_obss_coex dot11_obss_coex_t;
#define DOT11_OBSS_COEXINFO_LEN	1	/* length of OBSS Coexistence INFO IE */

#define	DOT11_OBSS_COEX_INFO_REQ		0x01
#define	DOT11_OBSS_COEX_40MHZ_INTOLERANT	0x02
#define	DOT11_OBSS_COEX_20MHZ_WIDTH_REQ	0x04

BWL_PRE_PACKED_STRUCT struct dot11_obss_chanlist {
	uint8	id;
	uint8	len;
	uint8	regclass;
	uint8	chanlist[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_obss_chanlist dot11_obss_chanlist_t;
#define DOT11_OBSS_CHANLIST_FIXED_LEN	1	/* fixed length of regclass */

/* Extended Capability Information Field */
#define DOT11_OBSS_COEX_MNG_SUPPORT	0x01	/* 20/40 BSS Coexistence Management support */

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif /* _802_11n_h_ */
