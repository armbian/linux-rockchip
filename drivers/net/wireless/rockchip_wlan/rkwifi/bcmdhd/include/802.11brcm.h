/*
 * Broadcom proprietary types and constants relating to 802.11
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

#ifndef _802_11brcm_h_
#define _802_11brcm_h_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

#ifndef _NET_ETHERNET_H_
#include <ethernet.h>
#endif

#include <802.11n.h>

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

/**
 * RWL wifi protocol: The Vendor Specific Action frame is defined for vendor-specific signaling
 *  category+OUI+vendor specific content ( this can be variable)
 */
BWL_PRE_PACKED_STRUCT struct dot11_action_wifi_vendor_specific {
	uint8	category;
	uint8	OUI[3];
	uint8	type;
	uint8	subtype;
	uint8	data[1040];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_action_wifi_vendor_specific dot11_action_wifi_vendor_specific_t;

#define BCM_ACTION_OUI_BYTE0	0x00
#define BCM_ACTION_OUI_BYTE1	0x90
#define BCM_ACTION_OUI_BYTE2	0x4c

BWL_PRE_PACKED_STRUCT struct dot11_brcm_extch {
	uint8	id;		/* IE ID, 221, DOT11_MNG_PROPR_ID */
	uint8	len;		/* IE length */
	uint8	oui[3];		/* Proprietary OUI, BRCM_PROP_OUI */
	uint8	type;           /* type indicates what follows */
	uint8	extch;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_brcm_extch dot11_brcm_extch_ie_t;

#define BRCM_EXTCH_IE_LEN	5

/* OUI for BRCM proprietary IE */
#define BRCM_PROP_OUI		"\x00\x90\x4C"	/* Broadcom proprietary OUI */

/* Broadcom Proprietary OUI type list. Please update below page when adding a new type.
 * Twiki https://hwnbu-twiki.lvn.broadcom.net/bin/view/Mwgroup/WlDriverIOVars?topic=WlBrcmPropIE
 */
/* The following BRCM_PROP_OUI types are currently in use (defined in
 * relevant subsections). Each of them will be in a separate proprietary(221) IE
 * #define SES_VNDR_IE_TYPE		1   (defined in src/ses/shared/ses.h)
 * #define BRCM_SYSCAP_IE_TYPE		3
 * #define WET_TUNNEL_IE_TYPE		3
 * #define ULB_BRCM_PROP_IE_TYPE	91
 * #define SDB_BRCM_PROP_IE_TYPE	92
 * #define BCM_RESERVED_ACCESS		93	(Reserved, definition pending)
 */
#define RWL_WIFI_DEFAULT		0
#define DPT_IE_TYPE			2
#define VHT_FEATURES_IE_TYPE		4
#define RWL_WIFI_FIND_MY_PEER		9	/* Used while finding server */
#define RWL_WIFI_FOUND_PEER		10	/* Server response to the client  */
#define PROXD_IE_TYPE			11	/* Wifi proximity action frame type */
#define BRCM_FTM_IE_TYPE		14
#define HT_CAP_IE_TYPE			51	/* used in prop IE 221 only */
#define HT_ADD_IE_TYPE			52	/* faked out as current spec is illegal */
#define BRCM_EXTCH_IE_TYPE		53	/* 802.11n ID not yet assigned */
#define MEMBER_OF_BRCM_PROP_IE_TYPE	54      /* used in prop IE 221 only */
#define RELMCAST_BRCM_PROP_IE_TYPE	55	/* used in prop IE 221 only */
/* BCM proprietary IE type for AIBSS */
#define BCM_AIBSS_IE_TYPE		56
/*
 * This BRCM_PROP_OUI types is intended for use in events to embed additional
 * data, and would not be expected to appear on the air -- but having an IE
 * format allows IE frame data with extra data in events in that allows for
 * more flexible parsing.
 */
#define BRCM_EVT_WL_BSS_INFO		64
#define RWL_ACTION_WIFI_FRAG_TYPE	85	/* Fragment indicator for receiver */
#define BTC_INFO_BRCM_PROP_IE_TYPE	90

/* Action frame type */
#define PROXD_AF_TYPE			11	/* Wifi proximity action frame type */
#define BRCM_RELMACST_AF_TYPE	        12	/* RMC action frame type */
/* Action frame type for FTM Initiator Report */
#define BRCM_FTM_VS_AF_TYPE		14

enum {
	BRCM_FTM_VS_INITIATOR_RPT_SUBTYPE = 1,	/* FTM Initiator Report */
	BRCM_FTM_VS_COLLECT_SUBTYPE = 2,	/* FTM Collect debug protocol */
};

/**
 * Following is the generic structure for brcm_prop_ie (uses BRCM_PROP_OUI).
 * DPT uses this format with type set to DPT_IE_TYPE
 */
#ifdef NEW_BRCM_PROP_IE
BWL_PRE_PACKED_STRUCT struct brcm_prop_ie_s {
	uint8 id;		/* IE ID, 221, DOT11_MNG_PROPR_ID */
	uint8 len;		/* IE length */
	uint8 oui[3];		/* Proprietary OUI, BRCM_PROP_OUI */
	uint8 type;		/* type of this IE */
} BWL_POST_PACKED_STRUCT;
typedef struct brcm_prop_ie_s brcm_prop_ie_t;
#else
typedef struct dpt_brcm_prop_ie_s brcm_prop_ie_t;
#endif /* NEW_BRCM_PROP_IE */
#define BRCM_PROP_IE_LEN	6	/* len of fixed part of brcm_prop ie */

/**
 * Following is the generic structure for brcm_prop_ie (uses BRCM_PROP_OUI).
 * DPT uses this format with type set to DPT_IE_TYPE
 */
BWL_PRE_PACKED_STRUCT struct dpt_brcm_prop_ie_s {
	uint8 id;		/* IE ID, 221, DOT11_MNG_PROPR_ID */
	uint8 len;		/* IE length */
	uint8 oui[3];		/* Proprietary OUI, BRCM_PROP_OUI */
	uint8 type;		/* type of this IE */
	uint16 cap;
} BWL_POST_PACKED_STRUCT;
typedef struct brcm_prop_ie_s dpt_brcm_prop_ie_t;

/* BRCM OUI: Used in the proprietary(221) IE in all broadcom devices */
#define BRCM_OUI		"\x00\x10\x18"	/* Broadcom OUI */

/** BRCM info element */
BWL_PRE_PACKED_STRUCT struct brcm_ie {
	uint8	id;		/* IE ID, 221, DOT11_MNG_PROPR_ID */
	uint8	len;		/* IE length */
	uint8	oui[3];		/* Proprietary OUI, BRCM_OUI */
	uint8	ver;		/* type/ver of this IE */
	uint8	assoc;		/* # of assoc STAs */
	uint8	flags;		/* misc flags */
	uint8	flags1;		/* misc flags */
	uint16	amsdu_mtu_pref;	/* preferred A-MSDU MTU */
	uint8	flags2;		/* DTPC Cap flags */
} BWL_POST_PACKED_STRUCT;
typedef	struct brcm_ie brcm_ie_t;
#define BRCM_IE_LEN		12u	/* BRCM IE length */
#define BRCM_IE_VER		2u	/* BRCM IE version */
#define BRCM_IE_LEGACY_AES_VER	1u	/* BRCM IE legacy AES version */

/* brcm_ie flags */
#define	BRF_ABCAP		0x01	/* afterburner is obsolete,  defined for backward compat */
#define	BRF_ABRQRD		0x02	/* afterburner is obsolete,  defined for backward compat */
#define	BRF_LZWDS		0x04	/* lazy wds enabled */
#define	BRF_BLOCKACK		0x08	/* BlockACK capable */
#define BRF_ABCOUNTER_MASK	0xf0	/* afterburner is obsolete,  defined for backward compat */
#define BRF_PROP_11N_MCS	0x10	/* re-use afterburner bit */
#define BRF_MEDIA_CLIENT	0x20	/* re-use afterburner bit to indicate media client device */

/**
 * Support for Broadcom proprietary HT MCS rates. Re-uses afterburner bits since
 * afterburner is not used anymore. Checks for BRF_ABCAP to stay compliant with 'old'
 * images in the field.
 */
#define GET_BRF_PROP_11N_MCS(brcm_ie) \
	(!((brcm_ie)->flags & BRF_ABCAP) && ((brcm_ie)->flags & BRF_PROP_11N_MCS))

/* brcm_ie flags1 */
#define	BRF1_AMSDU		0x01	/* A-MSDU capable */
#define	BRF1_WNM		0x02	/* WNM capable */
#define BRF1_WMEPS		0x04	/* AP is capable of handling WME + PS w/o APSD */
#define BRF1_PSOFIX		0x08	/* AP has fixed PS mode out-of-order packets */
#define	BRF1_RX_LARGE_AGG	0x10	/* device can rx large aggregates */
#define BRF1_RFAWARE_DCS	0x20    /* RFAWARE dynamic channel selection (DCS) */
#define BRF1_SOFTAP		0x40    /* Configure as Broadcom SOFTAP */
#define BRF1_DWDS		0x80    /* DWDS capable */

/* brcm_ie flags2 */
#define BRF2_DTPC_TX		0x01u	/* DTPC: DTPC TX Cap */
#define BRF2_DTPC_RX		0x02u	/* DTPC: DTPC RX Cap */
#define BRF2_DTPC_TX_RX		0x03u	/* DTPC: Enable Both DTPC TX and RX Cap */
#define BRF2_DTPC_NONBF		0x04u	/* DTPC: Enable DTPC for NON-TXBF */
#define BRF2_TWT_RESP_CAP	0x08u	/* TWT responder Cap for Brcm Softap
					 * only brcm sta parse this
					 */
#define BRF2_TWT_REQ_CAP	0x10u	/* TWT requester Cap for BRCM STA
					 * only brcm softap parse this
					 */

/** BRCM PROP DEVICE PRIMARY MAC ADDRESS IE */
BWL_PRE_PACKED_STRUCT struct member_of_brcm_prop_ie {
	uchar id;
	uchar len;
	uchar oui[3];
	uint8	type;           /* type indicates what follows */
	struct ether_addr ea;   /* Device Primary MAC Adrress */
} BWL_POST_PACKED_STRUCT;
typedef struct member_of_brcm_prop_ie member_of_brcm_prop_ie_t;

#define MEMBER_OF_BRCM_PROP_IE_LEN	10	/* IE max length */
#define MEMBER_OF_BRCM_PROP_IE_HDRLEN	(sizeof(member_of_brcm_prop_ie_t))

/** BRCM Reliable Multicast IE */
BWL_PRE_PACKED_STRUCT struct relmcast_brcm_prop_ie {
	uint8 id;
	uint8 len;
	uint8 oui[3];
	uint8 type;		/* type indicates what follows */
	struct ether_addr ea;	/* The ack sender's MAC Adrress */
	struct ether_addr mcast_ea;  /* The multicast MAC address */
	uint8 updtmo; /* time interval(second) for client to send null packet to report its rssi */
} BWL_POST_PACKED_STRUCT;
typedef struct relmcast_brcm_prop_ie relmcast_brcm_prop_ie_t;

/* IE length */
/* BRCM_PROP_IE_LEN = sizeof(relmcast_brcm_prop_ie_t)-((sizeof (id) + sizeof (len)))? */
#define RELMCAST_BRCM_PROP_IE_LEN	(sizeof(relmcast_brcm_prop_ie_t)-(2*sizeof(uint8)))

/* BRCM BTC IE */
BWL_PRE_PACKED_STRUCT struct btc_brcm_prop_ie {
	uint8 id;
	uint8 len;
	uint8 oui[3];
	uint8 type;		/* type inidicates what follows */
	uint32 info;
} BWL_POST_PACKED_STRUCT;
typedef struct btc_brcm_prop_ie btc_brcm_prop_ie_t;

#define BRCM_BTC_INFO_TYPE_LEN	(sizeof(btc_brcm_prop_ie_t) - (2 * sizeof(uint8)))

/* CAP IE: HT 1.0 spec. simply stole a 802.11 IE, we use our prop. IE until this is resolved */
/* the capability IE is primarily used to convey this nodes abilities */
BWL_PRE_PACKED_STRUCT struct ht_prop_cap_ie {
	uint8	id;		/* IE ID, 221, DOT11_MNG_PROPR_ID */
	uint8	len;		/* IE length */
	uint8	oui[3];		/* Proprietary OUI, BRCM_PROP_OUI */
	uint8	type;		/* type indicates what follows */
	ht_cap_ie_t cap_ie;
} BWL_POST_PACKED_STRUCT;
typedef struct ht_prop_cap_ie ht_prop_cap_ie_t;

#define HT_PROP_IE_OVERHEAD	4	/* overhead bytes for prop oui ie */

/* ADD IE: HT 1.0 spec. simply stole a 802.11 IE, we use our prop. IE until this is resolved */
/* the additional IE is primarily used to convey the current BSS configuration */
BWL_PRE_PACKED_STRUCT struct ht_prop_add_ie {
	uint8	id;		/* IE ID, 221, DOT11_MNG_PROPR_ID */
	uint8	len;		/* IE length */
	uint8	oui[3];		/* Proprietary OUI, BRCM_PROP_OUI */
	uint8	type;		/* indicates what follows */
	ht_add_ie_t add_ie;
} BWL_POST_PACKED_STRUCT;
typedef struct ht_prop_add_ie ht_prop_add_ie_t;

/**
 * BRCM vht features IE header
 * The header if the fixed part of the IE
 * On the 5GHz band this is the entire IE,
 * on 2.4GHz the VHT IEs as defined in the 802.11ac
 * specification follows
 *
 *
 * VHT features rates  bitmap.
 * Bit0:		5G MCS 0-9 BW 160MHz
 * Bit1:		5G MCS 0-9 support BW 80MHz
 * Bit2:		5G MCS 0-9 support BW 20MHz
 * Bit3:		2.4G MCS 0-9 support BW 20MHz
 * Bits:4-7	Reserved for future use
 *
 */
BWL_PRE_PACKED_STRUCT struct vht_features_ie_hdr {
	uint8 oui[3];		/* Proprietary OUI, BRCM_PROP_OUI */
	uint8 type;		/* type of this IE = 4 */
	uint8 rate_mask;	/* VHT rate mask */
} BWL_POST_PACKED_STRUCT;
typedef struct vht_features_ie_hdr vht_features_ie_hdr_t;

/* tag_ID/length/value_buffer tuple */
typedef BWL_PRE_PACKED_STRUCT struct {
	uint8	id;
	uint8	len;
	uint8	data[1];
} BWL_POST_PACKED_STRUCT ftm_vs_tlv_t;

BWL_PRE_PACKED_STRUCT struct dot11_ftm_vs_ie {
	uint8 id;		/* DOT11_MNG_VS_ID */
	uint8 len;		/* length following */
	uint8 oui[3];		/* BRCM_PROP_OUI (or Customer) */
	uint8 sub_type;		/* BRCM_FTM_IE_TYPE (or Customer) */
	uint8 version;
	ftm_vs_tlv_t tlvs[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_ftm_vs_ie dot11_ftm_vs_ie_t;

/* same as payload of dot11_ftm_vs_ie.
* This definition helps in having struct access
* of pay load while building FTM VS IE from other modules(NAN)
*/
BWL_PRE_PACKED_STRUCT struct dot11_ftm_vs_ie_pyld {
	uint8 sub_type;		/* BRCM_FTM_IE_TYPE (or Customer) */
	uint8 version;
	ftm_vs_tlv_t tlvs[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_ftm_vs_ie_pyld dot11_ftm_vs_ie_pyld_t;

/* ftm vs api version */
#define BCM_FTM_VS_PARAMS_VERSION 0x01

/* ftm vendor specific information tlv types */
enum {
	FTM_VS_TLV_NONE = 0,
	FTM_VS_TLV_REQ_PARAMS = 1,		/* additional request params (in FTM_REQ) */
	FTM_VS_TLV_MEAS_INFO = 2,		/* measurement information (in FTM_MEAS) */
	FTM_VS_TLV_SEC_PARAMS = 3,		/* security parameters (in either) */
	FTM_VS_TLV_SEQ_PARAMS = 4,		/* toast parameters (FTM_REQ, BRCM proprietary) */
	FTM_VS_TLV_MF_BUF = 5,			/* multi frame buffer - may span ftm vs ie's */
	FTM_VS_TLV_TIMING_PARAMS = 6,            /* timing adjustments */
	FTM_VS_TLV_MF_STATS_BUF = 7		/* multi frame statistics buffer */
	/* add additional types above */
};

/* BCM proprietary flag type for WL_DISCO_VSIE */
#define SSE_OUI			"\x00\x00\xF0"
#define VENDOR_ENTERPRISE_STA_OUI_TYPE	0x22
#define MAX_VSIE_DISASSOC       (1)
#define DISCO_VSIE_LEN          0x09u

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif /* _802_11brcm_h_ */
