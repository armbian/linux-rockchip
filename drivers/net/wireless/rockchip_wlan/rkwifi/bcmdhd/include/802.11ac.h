/*
 * Fundamental types and constants relating to 802.11ac -
 * "Enhancements for Very High Throughput for Operation in Bands below 6 GHz"
 *
 * VHT - Very High Throughput
 * OPER_MODE - Operating Mode
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

#ifndef _802_11ac_h_
#define _802_11ac_h_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

#ifndef _NET_ETHERNET_H_
#include <ethernet.h>
#endif

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

BWL_PRE_PACKED_STRUCT struct dot11_action_vht_oper_mode {
	uint8	category;
	uint8	action;
	uint8	mode;
} BWL_POST_PACKED_STRUCT;

/* These lengths assume 64 MU groups, as specified in 802.11ac-2013 */
#define DOT11_ACTION_GID_MEMBERSHIP_LEN  8    /* bytes */
#define DOT11_ACTION_GID_USER_POS_LEN   16    /* bytes */
BWL_PRE_PACKED_STRUCT struct dot11_action_group_id {
	uint8   category;
	uint8   action;
	uint8   membership_status[DOT11_ACTION_GID_MEMBERSHIP_LEN];
	uint8   user_position[DOT11_ACTION_GID_USER_POS_LEN];
} BWL_POST_PACKED_STRUCT;

/**  Wide Bandwidth Channel Switch IE data structure */
BWL_PRE_PACKED_STRUCT struct dot11_wide_bw_channel_switch {
	uint8 id;				/* id DOT11_MNG_WIDE_BW_CHANNEL_SWITCH_ID */
	uint8 len;				/* length of IE */
	uint8 channel_width;			/* new channel width */
	uint8 center_frequency_segment_0;	/* center frequency segment 0 */
	uint8 center_frequency_segment_1;	/* center frequency segment 1 */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_wide_bw_channel_switch dot11_wide_bw_chan_switch_ie_t;

#define DOT11_WIDE_BW_SWITCH_IE_LEN     3       /* length of IE data, not including 2 byte header */

/** Channel Switch Wrapper IE data structure */
BWL_PRE_PACKED_STRUCT struct dot11_channel_switch_wrapper {
	uint8 id;				/* id DOT11_MNG_WIDE_BW_CHANNEL_SWITCH_ID */
	uint8 len;				/* length of IE */
	dot11_wide_bw_chan_switch_ie_t wb_chan_switch_ie;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_channel_switch_wrapper dot11_chan_switch_wrapper_ie_t;

/* Proposed wide bandwidth channel IE */
typedef enum wide_bw_chan_width {
	WIDE_BW_CHAN_WIDTH_20	= 0,
	WIDE_BW_CHAN_WIDTH_40	= 1,
	WIDE_BW_CHAN_WIDTH_80	= 2,
	WIDE_BW_CHAN_WIDTH_160	= 3,
	WIDE_BW_CHAN_WIDTH_80_80	= 4
} wide_bw_chan_width_t;

/**  Wide Bandwidth Channel IE data structure */
BWL_PRE_PACKED_STRUCT struct dot11_wide_bw_channel {
	uint8 id;				/* id DOT11_MNG_WIDE_BW_CHANNEL_ID */
	uint8 len;				/* length of IE */
	uint8 channel_width;			/* channel width */
	uint8 center_frequency_segment_0;	/* center frequency segment 0 */
	uint8 center_frequency_segment_1;	/* center frequency segment 1 */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_wide_bw_channel dot11_wide_bw_chan_ie_t;

#define DOT11_WIDE_BW_IE_LEN     3       /* length of IE data, not including 2 byte header */
/** VHT Transmit Power Envelope IE data structure */
BWL_PRE_PACKED_STRUCT struct dot11_vht_transmit_power_envelope {
	uint8 id;				/* id DOT11_MNG_WIDE_BW_CHANNEL_SWITCH_ID */
	uint8 len;				/* length of IE */
	uint8 transmit_power_info;
	uint8 local_max_transmit_power_20;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_vht_transmit_power_envelope dot11_vht_transmit_power_envelope_ie_t;

/* vht transmit power envelope IE length depends on channel width */
#define DOT11_VHT_TRANSMIT_PWR_ENVELOPE_IE_LEN_40MHZ	1
#define DOT11_VHT_TRANSMIT_PWR_ENVELOPE_IE_LEN_80MHZ	2
#define DOT11_VHT_TRANSMIT_PWR_ENVELOPE_IE_LEN_160MHZ	3

/* VHT Operating mode bit fields -  (11ac D8.0/802.11-2016 - 9.4.1.53) */
#define DOT11_OPER_MODE_CHANNEL_WIDTH_SHIFT 0
#define DOT11_OPER_MODE_CHANNEL_WIDTH_MASK 0x3
#define DOT11_OPER_MODE_160_8080_BW_SHIFT 2
#define DOT11_OPER_MODE_160_8080_BW_MASK 0x04
#define DOT11_OPER_MODE_NOLDPC_SHIFT 3
#define DOT11_OPER_MODE_NOLDPC_MASK 0x08
#define DOT11_OPER_MODE_RXNSS_SHIFT 4
#define DOT11_OPER_MODE_RXNSS_MASK 0x70
#define DOT11_OPER_MODE_RXNSS_TYPE_SHIFT 7
#define DOT11_OPER_MODE_RXNSS_TYPE_MASK 0x80

#define DOT11_OPER_MODE_RESET_CHAN_WIDTH_160MHZ(oper_mode) \
	(oper_mode & (~(DOT11_OPER_MODE_CHANNEL_WIDTH_MASK | \
		DOT11_OPER_MODE_160_8080_BW_MASK)))
#define DOT11_OPER_MODE_SET_CHAN_WIDTH_160MHZ(oper_mode) \
	(oper_mode = (DOT11_OPER_MODE_RESET_CHAN_WIDTH_160MHZ(oper_mode) | \
		(DOT11_OPER_MODE_80MHZ | DOT11_OPER_MODE_160_8080_BW_MASK)))

#ifdef DOT11_OPER_MODE_LEFT_SHIFT_FIX

#define DOT11_OPER_MODE(type, nss, chanw) (\
	((type) << DOT11_OPER_MODE_RXNSS_TYPE_SHIFT &\
		 DOT11_OPER_MODE_RXNSS_TYPE_MASK) |\
	(((nss) - 1u) << DOT11_OPER_MODE_RXNSS_SHIFT & DOT11_OPER_MODE_RXNSS_MASK) |\
	((chanw) << DOT11_OPER_MODE_CHANNEL_WIDTH_SHIFT &\
		 DOT11_OPER_MODE_CHANNEL_WIDTH_MASK))

#define DOT11_D8_OPER_MODE(type, nss, ldpc, bw160_8080, chanw) (\
	((type) << DOT11_OPER_MODE_RXNSS_TYPE_SHIFT &\
		 DOT11_OPER_MODE_RXNSS_TYPE_MASK) |\
	(((nss) - 1u) << DOT11_OPER_MODE_RXNSS_SHIFT & DOT11_OPER_MODE_RXNSS_MASK) |\
	((ldpc) << DOT11_OPER_MODE_NOLDPC_SHIFT & DOT11_OPER_MODE_NOLDPC_MASK) |\
	((bw160_8080) << DOT11_OPER_MODE_160_8080_BW_SHIFT &\
		 DOT11_OPER_MODE_160_8080_BW_MASK) |\
	((chanw) << DOT11_OPER_MODE_CHANNEL_WIDTH_SHIFT &\
		 DOT11_OPER_MODE_CHANNEL_WIDTH_MASK))

#else

/* avoid invalidation from above fix on release branches, can be removed when older release
 * branches no longer use component/proto from trunk
 */

#define DOT11_OPER_MODE(type, nss, chanw) (\
	((type) << DOT11_OPER_MODE_RXNSS_TYPE_SHIFT &\
		 DOT11_OPER_MODE_RXNSS_TYPE_MASK) |\
	(((nss) - 1) << DOT11_OPER_MODE_RXNSS_SHIFT & DOT11_OPER_MODE_RXNSS_MASK) |\
	((chanw) << DOT11_OPER_MODE_CHANNEL_WIDTH_SHIFT &\
		 DOT11_OPER_MODE_CHANNEL_WIDTH_MASK))

#define DOT11_D8_OPER_MODE(type, nss, ldpc, bw160_8080, chanw) (\
	((type) << DOT11_OPER_MODE_RXNSS_TYPE_SHIFT &\
		 DOT11_OPER_MODE_RXNSS_TYPE_MASK) |\
	(((nss) - 1) << DOT11_OPER_MODE_RXNSS_SHIFT & DOT11_OPER_MODE_RXNSS_MASK) |\
	((ldpc) << DOT11_OPER_MODE_NOLDPC_SHIFT & DOT11_OPER_MODE_NOLDPC_MASK) |\
	((bw160_8080) << DOT11_OPER_MODE_160_8080_BW_SHIFT &\
		 DOT11_OPER_MODE_160_8080_BW_MASK) |\
	((chanw) << DOT11_OPER_MODE_CHANNEL_WIDTH_SHIFT &\
		 DOT11_OPER_MODE_CHANNEL_WIDTH_MASK))

#endif /* DOT11_OPER_MODE_LEFT_SHIFT_FIX */

#define DOT11_OPER_MODE_CHANNEL_WIDTH(mode) \
	(((mode) & DOT11_OPER_MODE_CHANNEL_WIDTH_MASK)\
		>> DOT11_OPER_MODE_CHANNEL_WIDTH_SHIFT)
#define DOT11_OPER_MODE_160_8080(mode) \
	(((mode) & DOT11_OPER_MODE_160_8080_BW_MASK)\
		>> DOT11_OPER_MODE_160_8080_BW_SHIFT)
#define DOT11_OPER_MODE_NOLDPC(mode) \
		(((mode) & DOT11_OPER_MODE_NOLDPC_MASK)\
			>> DOT11_OPER_MODE_NOLDPC_SHIFT)
#define DOT11_OPER_MODE_RXNSS(mode) \
	((((mode) & DOT11_OPER_MODE_RXNSS_MASK)		\
		>> DOT11_OPER_MODE_RXNSS_SHIFT) + 1)
#define DOT11_OPER_MODE_RXNSS_TYPE(mode) \
	(((mode) & DOT11_OPER_MODE_RXNSS_TYPE_MASK)\
		>> DOT11_OPER_MODE_RXNSS_TYPE_SHIFT)

#define DOT11_OPER_MODE_20MHZ 0
#define DOT11_OPER_MODE_40MHZ 1
#define DOT11_OPER_MODE_80MHZ 2
#define DOT11_OPER_MODE_160MHZ 3
#define DOT11_OPER_MODE_8080MHZ 3
#define DOT11_OPER_MODE_1608080MHZ 1

#define DOT11_OPER_MODE_CHANNEL_WIDTH_20MHZ(mode) (\
	((mode) & DOT11_OPER_MODE_CHANNEL_WIDTH_MASK) == DOT11_OPER_MODE_20MHZ)
#define DOT11_OPER_MODE_CHANNEL_WIDTH_40MHZ(mode) (\
	((mode) & DOT11_OPER_MODE_CHANNEL_WIDTH_MASK) == DOT11_OPER_MODE_40MHZ)
#define DOT11_OPER_MODE_CHANNEL_WIDTH_80MHZ(mode) (\
	((mode) & DOT11_OPER_MODE_CHANNEL_WIDTH_MASK) == DOT11_OPER_MODE_80MHZ)
#define DOT11_OPER_MODE_CHANNEL_WIDTH_160MHZ(mode) (\
	((mode) & DOT11_OPER_MODE_160_8080_BW_MASK))
#define DOT11_OPER_MODE_CHANNEL_WIDTH_8080MHZ(mode) (\
	((mode) & DOT11_OPER_MODE_160_8080_BW_MASK))

/* Operating mode information element 802.11ac D3.0 - 8.4.2.168 */
BWL_PRE_PACKED_STRUCT struct dot11_oper_mode_notif_ie {
	uint8 mode;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_oper_mode_notif_ie dot11_oper_mode_notif_ie_t;

#define DOT11_OPER_MODE_NOTIF_IE_LEN	1

/* VHT category action types - 802.11ac D3.0 - 8.5.23.1 */
#define DOT11_VHT_ACTION_CBF				0	/* Compressed Beamforming */
#define DOT11_VHT_ACTION_GID_MGMT			1	/* Group ID Management */
#define DOT11_VHT_ACTION_OPER_MODE_NOTIF	2	/* Operating mode notif'n */

/* 802.11 VHT constants */

typedef int vht_group_id_t;

/* for VHT-A1 */
/* SIG-A1 reserved bits */
#define VHT_SIGA1_CONST_MASK            0x800004

#define VHT_SIGA1_BW_MASK               0x000003
#define VHT_SIGA1_20MHZ_VAL             0x000000
#define VHT_SIGA1_40MHZ_VAL             0x000001
#define VHT_SIGA1_80MHZ_VAL             0x000002
#define VHT_SIGA1_160MHZ_VAL            0x000003

#define VHT_SIGA1_STBC                  0x000008

#define VHT_SIGA1_GID_MASK              0x0003f0
#define VHT_SIGA1_GID_SHIFT             4
#define VHT_SIGA1_GID_TO_AP             0x00
#define VHT_SIGA1_GID_NOT_TO_AP         0x3f
#define VHT_SIGA1_GID_MAX_GID           0x3f

#define VHT_SIGA1_NSTS_SHIFT_MASK_USER0 0x001C00
#define VHT_SIGA1_NSTS_SHIFT            10
#define VHT_SIGA1_MAX_USERPOS           3

#define VHT_SIGA1_PARTIAL_AID_MASK      0x3fe000
#define VHT_SIGA1_PARTIAL_AID_SHIFT     13

#define VHT_SIGA1_TXOP_PS_NOT_ALLOWED   0x400000

/* for VHT-A2 */
#define VHT_SIGA2_GI_NONE               0x000000
#define VHT_SIGA2_GI_SHORT              0x000001
#define VHT_SIGA2_GI_W_MOD10            0x000002
#define VHT_SIGA2_CODING_LDPC           0x000004
#define VHT_SIGA2_LDPC_EXTRA_OFDM_SYM   0x000008
#define VHT_SIGA2_BEAMFORM_ENABLE       0x000100
#define VHT_SIGA2_MCS_SHIFT             4

#define VHT_SIGA2_B9_RESERVED           0x000200
#define VHT_SIGA2_TAIL_MASK             0xfc0000
#define VHT_SIGA2_TAIL_VALUE            0x000000

/* VHT Timing-related parameters (802.11ac D4.0, sec 22.3.6) */
#define VHT_T_LEG_PREAMBLE      16
#define VHT_T_L_SIG              4
#define VHT_T_SIG_A              8
#define VHT_T_LTF                4
#define VHT_T_STF                4
#define VHT_T_SIG_B              4
#define VHT_T_SYML               4

#define VHT_N_SERVICE           16	/* bits in SERVICE field */
#define VHT_N_TAIL               6	/* tail bits per BCC encoder */

#define VHT_MAX_MPDU			11454	/* max mpdu size for now (bytes) */
#define VHT_MPDU_MSDU_DELTA		56	/* Difference in spec - vht mpdu, amsdu len */
/* Max AMSDU len - per spec */
#define VHT_MAX_AMSDU			(VHT_MAX_MPDU - VHT_MPDU_MSDU_DELTA)

/* ************* VHT definitions. ************* */

/**
 * VHT Capabilites IE (sec 8.4.2.160)
 */

BWL_PRE_PACKED_STRUCT struct vht_cap_ie {
	uint32  vht_cap_info;
	/* supported MCS set - 64 bit field */
	uint16	rx_mcs_map;
	uint16  rx_max_rate;
	uint16  tx_mcs_map;
	uint16	tx_max_rate;
} BWL_POST_PACKED_STRUCT;
typedef struct vht_cap_ie vht_cap_ie_t;

/* 4B cap_info + 8B supp_mcs */
#define VHT_CAP_IE_LEN 12

/* VHT Capabilities Info field - 32bit - in VHT Cap IE */
#define VHT_CAP_INFO_MAX_MPDU_LEN_MASK          0x00000003
#define VHT_CAP_INFO_SUPP_CHAN_WIDTH_MASK       0x0000000c
#define VHT_CAP_INFO_LDPC                       0x00000010
#define VHT_CAP_INFO_SGI_80MHZ                  0x00000020
#define VHT_CAP_INFO_SGI_160MHZ                 0x00000040
#define VHT_CAP_INFO_TX_STBC                    0x00000080
#define VHT_CAP_INFO_RX_STBC_MASK               0x00000700
#define VHT_CAP_INFO_RX_STBC_SHIFT              8u
#define VHT_CAP_INFO_SU_BEAMFMR                 0x00000800
#define VHT_CAP_INFO_SU_BEAMFMEE                0x00001000
#define VHT_CAP_INFO_NUM_BMFMR_ANT_MASK         0x0000e000
#define VHT_CAP_INFO_NUM_BMFMR_ANT_SHIFT        13u
#define VHT_CAP_INFO_NUM_SOUNDING_DIM_MASK      0x00070000
#define VHT_CAP_INFO_NUM_SOUNDING_DIM_SHIFT     16u
#define VHT_CAP_INFO_MU_BEAMFMR                 0x00080000
#define VHT_CAP_INFO_MU_BEAMFMEE                0x00100000
#define VHT_CAP_INFO_TXOPPS                     0x00200000
#define VHT_CAP_INFO_HTCVHT                     0x00400000
#define VHT_CAP_INFO_AMPDU_MAXLEN_EXP_MASK      0x03800000
#define VHT_CAP_INFO_AMPDU_MAXLEN_EXP_SHIFT     23u
#define VHT_CAP_INFO_LINK_ADAPT_CAP_MASK        0x0c000000
#define VHT_CAP_INFO_LINK_ADAPT_CAP_SHIFT       26u
#define VHT_CAP_INFO_EXT_NSS_BW_SUP_MASK        0xc0000000
#define VHT_CAP_INFO_EXT_NSS_BW_SUP_SHIFT       30u

/* get Extended NSS BW Support passing vht cap info */
#define VHT_CAP_EXT_NSS_BW_SUP(cap_info) \
	(((cap_info) & VHT_CAP_INFO_EXT_NSS_BW_SUP_MASK) >> VHT_CAP_INFO_EXT_NSS_BW_SUP_SHIFT)

/* VHT CAP INFO extended NSS BW support - refer to IEEE 802.11 REVmc D8.0 Figure 9-559 */
#define VHT_CAP_INFO_EXT_NSS_BW_HALF_160	1 /* 160MHz at half NSS CAP */
#define VHT_CAP_INFO_EXT_NSS_BW_HALF_160_80P80	2 /* 160 & 80p80 MHz at half NSS CAP */

/* VHT Supported MCS Set - 64-bit - in VHT Cap IE */
#define VHT_CAP_SUPP_MCS_RX_HIGHEST_RATE_MASK   0x1fff
#define VHT_CAP_SUPP_MCS_RX_HIGHEST_RATE_SHIFT  0

#define VHT_CAP_SUPP_MCS_TX_HIGHEST_RATE_MASK   0x1fff
#define VHT_CAP_SUPP_MCS_TX_HIGHEST_RATE_SHIFT  0

/* defines for field(s) in vht_cap_ie->rx_max_rate */
#define VHT_CAP_MAX_NSTS_MASK			0xe000
#define VHT_CAP_MAX_NSTS_SHIFT			13

/* defines for field(s) in vht_cap_ie->tx_max_rate */
#define VHT_CAP_EXT_NSS_BW_CAP			0x2000

#define VHT_CAP_MCS_MAP_0_7                     0
#define VHT_CAP_MCS_MAP_0_8                     1
#define VHT_CAP_MCS_MAP_0_9                     2
#define VHT_CAP_MCS_MAP_NONE                    3
#define VHT_CAP_MCS_MAP_S                       2 /* num bits for 1-stream */
#define VHT_CAP_MCS_MAP_M                       0x3 /* mask for 1-stream */
/* assumes VHT_CAP_MCS_MAP_NONE is 3 and 2 bits are used for encoding */
#define VHT_CAP_MCS_MAP_NONE_ALL                0xffff

/* VHT rates bitmap */
#define VHT_CAP_MCS_0_7_RATEMAP		0x00ff
#define VHT_CAP_MCS_0_8_RATEMAP		0x01ff
#define VHT_CAP_MCS_0_9_RATEMAP		0x03ff
#define VHT_CAP_MCS_FULL_RATEMAP	VHT_CAP_MCS_0_9_RATEMAP

#define VHT_PROP_MCS_MAP_10_11                   0
#define VHT_PROP_MCS_MAP_UNUSED1                 1
#define VHT_PROP_MCS_MAP_UNUSED2                 2
#define VHT_PROP_MCS_MAP_NONE                    3
#define VHT_PROP_MCS_MAP_NONE_ALL                0xffff

/* VHT prop rates bitmap */
#define VHT_PROP_MCS_10_11_RATEMAP	0x0c00
#define VHT_PROP_MCS_FULL_RATEMAP	VHT_PROP_MCS_10_11_RATEMAP

#if !defined(VHT_CAP_MCS_MAP_0_9_NSS3)
/* remove after moving define to wlc_rate.h */
/* mcsmap with MCS0-9 for Nss = 3 */
#define VHT_CAP_MCS_MAP_0_9_NSS3 \
	        ((VHT_CAP_MCS_MAP_0_9 << VHT_MCS_MAP_GET_SS_IDX(1)) | \
	         (VHT_CAP_MCS_MAP_0_9 << VHT_MCS_MAP_GET_SS_IDX(2)) | \
	         (VHT_CAP_MCS_MAP_0_9 << VHT_MCS_MAP_GET_SS_IDX(3)))
#endif /* !VHT_CAP_MCS_MAP_0_9_NSS3 */

#define VHT_CAP_MCS_MAP_NSS_MAX                 8

/* get mcsmap with given mcs for given nss streams */
#define VHT_CAP_MCS_MAP_CREATE(mcsmap, nss, mcs) \
	do { \
		int i; \
		for (i = 1; i <= nss; i++) { \
			VHT_MCS_MAP_SET_MCS_PER_SS(i, mcs, mcsmap); \
		} \
	} while (0)

/* Map the mcs code to mcs bit map */
#define VHT_MCS_CODE_TO_MCS_MAP(mcs_code) \
	((mcs_code == VHT_CAP_MCS_MAP_0_7) ? VHT_CAP_MCS_0_7_RATEMAP : \
	 (mcs_code == VHT_CAP_MCS_MAP_0_8) ? VHT_CAP_MCS_0_8_RATEMAP : \
	 (mcs_code == VHT_CAP_MCS_MAP_0_9) ? VHT_CAP_MCS_0_9_RATEMAP : 0)

/* Map the proprietary mcs code to proprietary mcs bitmap */
#define VHT_PROP_MCS_CODE_TO_PROP_MCS_MAP(mcs_code) \
	((mcs_code == VHT_PROP_MCS_MAP_10_11) ? VHT_PROP_MCS_10_11_RATEMAP : 0)

/* Map the mcs bit map to mcs code */
#define VHT_MCS_MAP_TO_MCS_CODE(mcs_map) \
	((mcs_map == VHT_CAP_MCS_0_7_RATEMAP) ? VHT_CAP_MCS_MAP_0_7 : \
	 (mcs_map == VHT_CAP_MCS_0_8_RATEMAP) ? VHT_CAP_MCS_MAP_0_8 : \
	 (mcs_map == VHT_CAP_MCS_0_9_RATEMAP) ? VHT_CAP_MCS_MAP_0_9 : VHT_CAP_MCS_MAP_NONE)

/* Map the proprietary mcs map to proprietary mcs code */
#define VHT_PROP_MCS_MAP_TO_PROP_MCS_CODE(mcs_map) \
	(((mcs_map & 0xc00) == 0xc00)  ? VHT_PROP_MCS_MAP_10_11 : VHT_PROP_MCS_MAP_NONE)

/** VHT Capabilities Supported Channel Width */
typedef enum vht_cap_chan_width {
	VHT_CAP_CHAN_WIDTH_SUPPORT_MANDATORY = 0x00,
	VHT_CAP_CHAN_WIDTH_SUPPORT_160       = 0x04,
	VHT_CAP_CHAN_WIDTH_SUPPORT_160_8080  = 0x08
} vht_cap_chan_width_t;

/** VHT Capabilities Supported max MPDU LEN (sec 8.4.2.160.2) */
typedef enum vht_cap_max_mpdu_len {
	VHT_CAP_MPDU_MAX_4K     = 0x00,
	VHT_CAP_MPDU_MAX_8K     = 0x01,
	VHT_CAP_MPDU_MAX_11K    = 0x02
} vht_cap_max_mpdu_len_t;

/* Maximum MPDU Length byte counts for the VHT Capabilities advertised limits */
#define VHT_MPDU_LIMIT_4K        3895
#define VHT_MPDU_LIMIT_8K        7991
#define VHT_MPDU_LIMIT_11K      11454

/**
 * VHT Operation IE (sec 8.4.2.161)
 */

BWL_PRE_PACKED_STRUCT struct vht_op_ie {
	uint8	chan_width;
	uint8	chan1;
	uint8	chan2;
	uint16	supp_mcs;  /*  same def as above in vht cap */
} BWL_POST_PACKED_STRUCT;
typedef struct vht_op_ie vht_op_ie_t;

/* 3B VHT Op info + 2B Basic MCS */
#define VHT_OP_IE_LEN 5

typedef enum vht_op_chan_width {
	VHT_OP_CHAN_WIDTH_20_40	= 0,
	VHT_OP_CHAN_WIDTH_80	= 1,
	VHT_OP_CHAN_WIDTH_160	= 2, /* deprecated - IEEE 802.11 REVmc D8.0 Table 11-25 */
	VHT_OP_CHAN_WIDTH_80_80	= 3  /* deprecated - IEEE 802.11 REVmc D8.0 Table 11-25 */
} vht_op_chan_width_t;

#define VHT_OP_INFO_LEN		3

/* AID length */
#define AID_IE_LEN		2

/* Def for rx & tx basic mcs maps - ea ss num has 2 bits of info */
#define VHT_MCS_MAP_GET_SS_IDX(nss) (((nss)-1) * VHT_CAP_MCS_MAP_S)
#define VHT_MCS_MAP_GET_MCS_PER_SS(nss, mcsMap) \
	(((mcsMap) >> VHT_MCS_MAP_GET_SS_IDX(nss)) & VHT_CAP_MCS_MAP_M)
#define VHT_MCS_MAP_SET_MCS_PER_SS(nss, numMcs, mcsMap) \
	do { \
	 (mcsMap) &= (~(VHT_CAP_MCS_MAP_M << VHT_MCS_MAP_GET_SS_IDX(nss))); \
	 (mcsMap) |= (((numMcs) & VHT_CAP_MCS_MAP_M) << VHT_MCS_MAP_GET_SS_IDX(nss)); \
	} while (0)
#define VHT_MCS_SS_SUPPORTED(nss, mcsMap) \
		 (VHT_MCS_MAP_GET_MCS_PER_SS((nss), (mcsMap)) != VHT_CAP_MCS_MAP_NONE)

/* Get the max ss supported from the mcs map */
#define VHT_MAX_SS_SUPPORTED(mcsMap) \
	VHT_MCS_SS_SUPPORTED(8, mcsMap) ? 8 : \
	VHT_MCS_SS_SUPPORTED(7, mcsMap) ? 7 : \
	VHT_MCS_SS_SUPPORTED(6, mcsMap) ? 6 : \
	VHT_MCS_SS_SUPPORTED(5, mcsMap) ? 5 : \
	VHT_MCS_SS_SUPPORTED(4, mcsMap) ? 4 : \
	VHT_MCS_SS_SUPPORTED(3, mcsMap) ? 3 : \
	VHT_MCS_SS_SUPPORTED(2, mcsMap) ? 2 : \
	VHT_MCS_SS_SUPPORTED(1, mcsMap) ? 1 : 0

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif /* _802_11ac_h_ */
