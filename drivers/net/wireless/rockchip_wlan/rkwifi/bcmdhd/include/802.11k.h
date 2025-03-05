/*
 * Fundamental types and constants relating to 802.11k -
 * "Radio Resource Measurement of Wireless LANs" &
 * Fundamental types and constants relating to 802.11v -
 * "Wireless Network Management" extensions
 *
 * RRM - Radio Resource Measurement
 * RM - same as RRM?
 * NGBR - Neighbor Report
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

#ifndef _802_11k_h_
#define _802_11k_h_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

/* ************* 802.11k related definitions. ************* */

/* Radio measurements enabled capability ie */
#define DOT11_RRM_CAP_LEN		5	/* length of rrm cap bitmap */
#define RCPI_IE_LEN 1
#define RSNI_IE_LEN 1
BWL_PRE_PACKED_STRUCT struct dot11_rrm_cap_ie {
	uint8 cap[DOT11_RRM_CAP_LEN];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rrm_cap_ie dot11_rrm_cap_ie_t;

/* Bitmap definitions for cap ie */
#define DOT11_RRM_CAP_LINK		0
#define DOT11_RRM_CAP_NEIGHBOR_REPORT	1
#define DOT11_RRM_CAP_PARALLEL		2
#define DOT11_RRM_CAP_REPEATED		3
#define DOT11_RRM_CAP_BCN_PASSIVE	4
#define DOT11_RRM_CAP_BCN_ACTIVE	5
#define DOT11_RRM_CAP_BCN_TABLE		6
#define DOT11_RRM_CAP_BCN_REP_COND	7
#define DOT11_RRM_CAP_FM		8
#define DOT11_RRM_CAP_CLM		9
#define DOT11_RRM_CAP_NHM		10
#define DOT11_RRM_CAP_SM		11
#define DOT11_RRM_CAP_LCIM		12
#define DOT11_RRM_CAP_LCIA		13
#define DOT11_RRM_CAP_TSCM		14
#define DOT11_RRM_CAP_TTSCM		15
#define DOT11_RRM_CAP_AP_CHANREP	16
#define DOT11_RRM_CAP_RMMIB		17
/* bit18-bit23, not used for RRM_IOVAR */
#define DOT11_RRM_CAP_MPC0		24
#define DOT11_RRM_CAP_MPC1		25
#define DOT11_RRM_CAP_MPC2		26
#define DOT11_RRM_CAP_MPTI		27
#define DOT11_RRM_CAP_NBRTSFO		28
#define DOT11_RRM_CAP_RCPI		29
#define DOT11_RRM_CAP_RSNI		30
#define DOT11_RRM_CAP_BSSAAD		31
#define DOT11_RRM_CAP_BSSAAC		32
#define DOT11_RRM_CAP_AI		33
#define DOT11_RRM_CAP_FTM_RANGE		34
#define DOT11_RRM_CAP_CIVIC_LOC		35
#define DOT11_RRM_CAP_IDENT_LOC		36
#define DOT11_RRM_CAP_LAST		36

#ifdef WL11K_ALL_MEAS
#define DOT11_RRM_CAP_LINK_ENAB			(1 << DOT11_RRM_CAP_LINK)
#define DOT11_RRM_CAP_FM_ENAB			(1 << (DOT11_RRM_CAP_FM - 8))
#define DOT11_RRM_CAP_CLM_ENAB			(1 << (DOT11_RRM_CAP_CLM - 8))
#define DOT11_RRM_CAP_NHM_ENAB			(1 << (DOT11_RRM_CAP_NHM - 8))
#define DOT11_RRM_CAP_SM_ENAB			(1 << (DOT11_RRM_CAP_SM - 8))
#define DOT11_RRM_CAP_LCIM_ENAB			(1 << (DOT11_RRM_CAP_LCIM - 8))
#define DOT11_RRM_CAP_TSCM_ENAB			(1 << (DOT11_RRM_CAP_TSCM - 8))
#ifdef WL11K_AP
#define DOT11_RRM_CAP_MPC0_ENAB			(1 << (DOT11_RRM_CAP_MPC0 - 24))
#define DOT11_RRM_CAP_MPC1_ENAB			(1 << (DOT11_RRM_CAP_MPC1 - 24))
#define DOT11_RRM_CAP_MPC2_ENAB			(1 << (DOT11_RRM_CAP_MPC2 - 24))
#define DOT11_RRM_CAP_MPTI_ENAB			(1 << (DOT11_RRM_CAP_MPTI - 24))
#else
#define DOT11_RRM_CAP_MPC0_ENAB			0
#define DOT11_RRM_CAP_MPC1_ENAB			0
#define DOT11_RRM_CAP_MPC2_ENAB			0
#define DOT11_RRM_CAP_MPTI_ENAB			0
#endif /* WL11K_AP */
#define DOT11_RRM_CAP_CIVIC_LOC_ENAB		(1 << (DOT11_RRM_CAP_CIVIC_LOC - 32))
#define DOT11_RRM_CAP_IDENT_LOC_ENAB		(1 << (DOT11_RRM_CAP_IDENT_LOC - 32))
#else
#define DOT11_RRM_CAP_LINK_ENAB			0
#define DOT11_RRM_CAP_FM_ENAB			0
#define DOT11_RRM_CAP_CLM_ENAB			0
#define DOT11_RRM_CAP_NHM_ENAB			0
#define DOT11_RRM_CAP_SM_ENAB			0
#define DOT11_RRM_CAP_LCIM_ENAB			0
#define DOT11_RRM_CAP_TSCM_ENAB			0
#define DOT11_RRM_CAP_MPC0_ENAB			0
#define DOT11_RRM_CAP_MPC1_ENAB			0
#define DOT11_RRM_CAP_MPC2_ENAB			0
#define DOT11_RRM_CAP_MPTI_ENAB			0
#define DOT11_RRM_CAP_CIVIC_LOC_ENAB		0
#define DOT11_RRM_CAP_IDENT_LOC_ENAB		0
#endif /* WL11K_ALL_MEAS */
#ifdef WL11K_NBR_MEAS
#define DOT11_RRM_CAP_NEIGHBOR_REPORT_ENAB	(1 << DOT11_RRM_CAP_NEIGHBOR_REPORT)
#else
#define DOT11_RRM_CAP_NEIGHBOR_REPORT_ENAB	0
#endif /* WL11K_NBR_MEAS */
#ifdef WL11K_BCN_MEAS
#define DOT11_RRM_CAP_BCN_PASSIVE_ENAB		(1 << DOT11_RRM_CAP_BCN_PASSIVE)
#define DOT11_RRM_CAP_BCN_ACTIVE_ENAB		(1 << DOT11_RRM_CAP_BCN_ACTIVE)
#else
#define DOT11_RRM_CAP_BCN_PASSIVE_ENAB		0
#define DOT11_RRM_CAP_BCN_ACTIVE_ENAB		0
#endif /* WL11K_BCN_MEAS */
#define DOT11_RRM_CAP_MPA_MASK		0x7
/* Operating Class (formerly "Regulatory Class") definitions */
#define DOT11_OP_CLASS_NONE			255

BWL_PRE_PACKED_STRUCT struct do11_ap_chrep {
	uint8 id;
	uint8 len;
	uint8 reg;
	uint8 chanlist[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct do11_ap_chrep dot11_ap_chrep_t;

/* Radio Measurements action ids */
#define DOT11_RM_ACTION_RM_REQ		0	/* Radio measurement request */
#define DOT11_RM_ACTION_RM_REP		1	/* Radio measurement report */
#define DOT11_RM_ACTION_LM_REQ		2	/* Link measurement request */
#define DOT11_RM_ACTION_LM_REP		3	/* Link measurement report */
#define DOT11_RM_ACTION_NR_REQ		4	/* Neighbor report request */
#define DOT11_RM_ACTION_NR_REP		5	/* Neighbor report response */

/** Generic radio measurement action frame header */
BWL_PRE_PACKED_STRUCT struct dot11_rm_action {
	uint8 category;				/* category of action frame (5) */
	uint8 action;				/* radio measurement action */
	uint8 token;				/* dialog token */
	uint8 data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rm_action dot11_rm_action_t;
#define DOT11_RM_ACTION_LEN 3

BWL_PRE_PACKED_STRUCT struct dot11_rmreq {
	uint8 category;				/* category of action frame (5) */
	uint8 action;				/* radio measurement action */
	uint8 token;				/* dialog token */
	uint16 reps;				/* no. of repetitions */
	uint8 data[BCM_FLEX_ARRAY];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmreq dot11_rmreq_t;
#define DOT11_RMREQ_LEN	5

BWL_PRE_PACKED_STRUCT struct dot11_rm_ie {
	uint8 id;
	uint8 len;
	uint8 token;
	uint8 mode;
	uint8 type;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rm_ie dot11_rm_ie_t;
#define DOT11_RM_IE_LEN	5

/* Definitions for "mode" bits in rm req */
#define DOT11_RMREQ_MODE_PARALLEL	1
#define DOT11_RMREQ_MODE_ENABLE		2
#define DOT11_RMREQ_MODE_REQUEST	4
#define DOT11_RMREQ_MODE_REPORT		8
#define DOT11_RMREQ_MODE_DURMAND	0x10	/* Duration Mandatory */

/* Definitions for "mode" bits in rm rep */
#define DOT11_RMREP_MODE_LATE		1
#define DOT11_RMREP_MODE_INCAPABLE	2
#define DOT11_RMREP_MODE_REFUSED	4

BWL_PRE_PACKED_STRUCT struct dot11_rmreq_bcn {
	uint8 id;		/* use dot11_rm_ie_t ? */
	uint8 len;
	uint8 token;
	uint8 mode;
	uint8 type;
	uint8 reg;
	uint8 channel;
	uint16 interval;
	uint16 duration;
	uint8 bcn_mode;
	struct ether_addr	bssid;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmreq_bcn dot11_rmreq_bcn_t;
#define DOT11_RMREQ_BCN_LEN	18u

BWL_PRE_PACKED_STRUCT struct dot11_rmrep_bcn {
	uint8 reg;
	uint8 channel;
	uint32 starttime[2];
	uint16 duration;
	uint8 frame_info;
	uint8 rcpi;
	uint8 rsni;
	struct ether_addr	bssid;
	uint8 antenna_id;
	uint32 parent_tsf;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmrep_bcn dot11_rmrep_bcn_t;
#define DOT11_RMREP_BCN_LEN	26

/* Beacon request measurement mode */
#define DOT11_RMREQ_BCN_PASSIVE	0
#define DOT11_RMREQ_BCN_ACTIVE	1
#define DOT11_RMREQ_BCN_TABLE	2

/* Sub-element IDs for Beacon Request */
#define DOT11_RMREQ_BCN_SSID_ID 0
#define DOT11_RMREQ_BCN_REPINFO_ID  1
#define DOT11_RMREQ_BCN_REPDET_ID   2
#define DOT11_RMREQ_BCN_REQUEST_ID  10
#define DOT11_RMREQ_BCN_APCHREP_ID  DOT11_MNG_AP_CHREP_ID
#define DOT11_RMREQ_BCN_LAST_RPT_IND_REQ_ID 164

/* Reporting Detail element definition */
#define DOT11_RMREQ_BCN_REPDET_FIXED	0	/* Fixed length fields only */
#define DOT11_RMREQ_BCN_REPDET_REQUEST	1	/* + requested information elems */
#define DOT11_RMREQ_BCN_REPDET_ALL	2	/* All fields */

/* Reporting Information (reporting condition) element definition */
#define DOT11_RMREQ_BCN_REPINFO_LEN	2	/* Beacon Reporting Information length */
#define DOT11_RMREQ_BCN_REPCOND_DEFAULT	0	/* Report to be issued after each measurement */

/* Last Beacon Report Indication Request definition */
#define DOT11_RMREQ_BCN_LAST_RPT_IND_REQ_ENAB  1

BWL_PRE_PACKED_STRUCT struct dot11_rmrep_last_bcn_rpt_ind_req {
	uint8 id;                       /* DOT11_RMREQ_BCN_LAST_RPT_IND_REQ_ID */
	uint8 len;                      /* length of remaining fields */
	uint8 data;                     /* data = 1 means last bcn rpt ind requested */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmrep_last_bcn_rpt_ind_req dot11_rmrep_last_bcn_rpt_ind_req_t;

/* Sub-element IDs for Beacon Report */
#define DOT11_RMREP_BCN_FRM_BODY	1
#define DOT11_RMREP_BCN_FRM_BODY_FRAG_ID	2
#define DOT11_RMREP_BCN_LAST_RPT_IND 164
#define DOT11_RMREP_BCN_FRM_BODY_LEN_MAX	224 /* 802.11k-2008 7.3.2.22.6 */

/* Refer IEEE P802.11-REVmd/D1.0 9.4.2.21.7 Beacon report */
BWL_PRE_PACKED_STRUCT struct dot11_rmrep_bcn_frm_body_fragmt_id {
	uint8 id;                       /* DOT11_RMREP_BCN_FRM_BODY_FRAG_ID */
	uint8 len;                      /* length of remaining fields */
	/* More fragments(B15), fragment Id(B8-B14), Bcn rpt instance ID (B0 - B7) */
	uint16 frag_info_rpt_id;
} BWL_POST_PACKED_STRUCT;

typedef struct dot11_rmrep_bcn_frm_body_fragmt_id dot11_rmrep_bcn_frm_body_fragmt_id_t;

BWL_PRE_PACKED_STRUCT struct dot11_rmrep_bcn_frm_body_frag_id {
	uint8 id;                       /* DOT11_RMREP_BCN_FRM_BODY_FRAG_ID */
	uint8 len;                      /* length of remaining fields */
	uint8 bcn_rpt_id;               /* Bcn rpt instance ID */
	uint8 frag_info;                /* fragment Id(7 bits) | More fragments(1 bit) */
} BWL_POST_PACKED_STRUCT;

typedef struct dot11_rmrep_bcn_frm_body_frag_id dot11_rmrep_bcn_frm_body_frag_id_t;
#define DOT11_RMREP_BCNRPT_FRAG_ID_DATA_LEN  2u
#define DOT11_RMREP_BCNRPT_FRAG_ID_SE_LEN sizeof(dot11_rmrep_bcn_frm_body_frag_id_t)
#define DOT11_RMREP_BCNRPT_FRAG_ID_NUM_SHIFT  1u
#define DOT11_RMREP_BCNRPT_FRAGMT_ID_SE_LEN sizeof(dot11_rmrep_bcn_frm_body_fragmt_id_t)
#define DOT11_RMREP_BCNRPT_BCN_RPT_ID_MASK  0x00FFu
#define DOT11_RMREP_BCNRPT_FRAGMT_ID_NUM_SHIFT  8u
#define DOT11_RMREP_BCNRPT_FRAGMT_ID_NUM_MASK  0x7F00u
#define DOT11_RMREP_BCNRPT_MORE_FRAG_SHIFT  15u
#define DOT11_RMREP_BCNRPT_MORE_FRAG_MASK  0x8000u

BWL_PRE_PACKED_STRUCT struct dot11_rmrep_last_bcn_rpt_ind {
	uint8 id;                       /* DOT11_RMREP_BCN_LAST_RPT_IND */
	uint8 len;                      /* length of remaining fields */
	uint8 data;                     /* data = 1 is last bcn rpt */
} BWL_POST_PACKED_STRUCT;

typedef struct dot11_rmrep_last_bcn_rpt_ind dot11_rmrep_last_bcn_rpt_ind_t;
#define DOT11_RMREP_LAST_BCN_RPT_IND_DATA_LEN 1
#define DOT11_RMREP_LAST_BCN_RPT_IND_SE_LEN sizeof(dot11_rmrep_last_bcn_rpt_ind_t)

/* Sub-element IDs for Frame Report */
#define DOT11_RMREP_FRAME_COUNT_REPORT 1

/* Channel load request */
BWL_PRE_PACKED_STRUCT struct dot11_rmreq_chanload {
	uint8 id;		/* use dot11_rm_ie_t ? */
	uint8 len;
	uint8 token;
	uint8 mode;
	uint8 type;
	uint8 reg;
	uint8 channel;
	uint16 interval;
	uint16 duration;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmreq_chanload dot11_rmreq_chanload_t;
#define DOT11_RMREQ_CHANLOAD_LEN	11

/** Channel load report */
BWL_PRE_PACKED_STRUCT struct dot11_rmrep_chanload {
	uint8 reg;
	uint8 channel;
	uint32 starttime[2];
	uint16 duration;
	uint8 channel_load;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmrep_chanload dot11_rmrep_chanload_t;
#define DOT11_RMREP_CHANLOAD_LEN	13

/** Noise histogram request */
BWL_PRE_PACKED_STRUCT struct dot11_rmreq_noise {
	uint8 id;		/* use dot11_rm_ie_t ? */
	uint8 len;
	uint8 token;
	uint8 mode;
	uint8 type;
	uint8 reg;
	uint8 channel;
	uint16 interval;
	uint16 duration;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmreq_noise dot11_rmreq_noise_t;
#define DOT11_RMREQ_NOISE_LEN 11

/** Noise histogram report */
BWL_PRE_PACKED_STRUCT struct dot11_rmrep_noise {
	uint8 reg;
	uint8 channel;
	uint32 starttime[2];
	uint16 duration;
	uint8 antid;
	uint8 anpi;
	uint8 ipi0_dens;
	uint8 ipi1_dens;
	uint8 ipi2_dens;
	uint8 ipi3_dens;
	uint8 ipi4_dens;
	uint8 ipi5_dens;
	uint8 ipi6_dens;
	uint8 ipi7_dens;
	uint8 ipi8_dens;
	uint8 ipi9_dens;
	uint8 ipi10_dens;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmrep_noise dot11_rmrep_noise_t;
#define DOT11_RMREP_NOISE_LEN 25

/** Frame request */
BWL_PRE_PACKED_STRUCT struct dot11_rmreq_frame {
	uint8 id;		/* use dot11_rm_ie_t ? */
	uint8 len;
	uint8 token;
	uint8 mode;
	uint8 type;
	uint8 reg;
	uint8 channel;
	uint16 interval;
	uint16 duration;
	uint8 req_type;
	struct ether_addr	ta;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmreq_frame dot11_rmreq_frame_t;
#define DOT11_RMREQ_FRAME_LEN 18

/** Frame report */
BWL_PRE_PACKED_STRUCT struct dot11_rmrep_frame {
	uint8 reg;
	uint8 channel;
	uint32 starttime[2];
	uint16 duration;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmrep_frame dot11_rmrep_frame_t;
#define DOT11_RMREP_FRAME_LEN 12

/** Frame report entry */
BWL_PRE_PACKED_STRUCT struct dot11_rmrep_frmentry {
	struct ether_addr	ta;
	struct ether_addr	bssid;
	uint8 phy_type;
	uint8 avg_rcpi;
	uint8 last_rsni;
	uint8 last_rcpi;
	uint8 ant_id;
	uint16 frame_cnt;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmrep_frmentry dot11_rmrep_frmentry_t;
#define DOT11_RMREP_FRMENTRY_LEN 19

/** STA statistics request */
BWL_PRE_PACKED_STRUCT struct dot11_rmreq_stat {
	uint8 id;		/* use dot11_rm_ie_t ? */
	uint8 len;
	uint8 token;
	uint8 mode;
	uint8 type;
	struct ether_addr	peer;
	uint16 interval;
	uint16 duration;
	uint8 group_id;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmreq_stat dot11_rmreq_stat_t;
#define DOT11_RMREQ_STAT_LEN 16

/** STA statistics report */
BWL_PRE_PACKED_STRUCT struct dot11_rmrep_stat {
	uint16 duration;
	uint8 group_id;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmrep_stat dot11_rmrep_stat_t;

/* Statistics Group Report: Group IDs */
enum {
	DOT11_RRM_STATS_GRP_ID_0 = 0,
	DOT11_RRM_STATS_GRP_ID_1,
	DOT11_RRM_STATS_GRP_ID_2,
	DOT11_RRM_STATS_GRP_ID_3,
	DOT11_RRM_STATS_GRP_ID_4,
	DOT11_RRM_STATS_GRP_ID_5,
	DOT11_RRM_STATS_GRP_ID_6,
	DOT11_RRM_STATS_GRP_ID_7,
	DOT11_RRM_STATS_GRP_ID_8,
	DOT11_RRM_STATS_GRP_ID_9,
	DOT11_RRM_STATS_GRP_ID_10,
	DOT11_RRM_STATS_GRP_ID_11,
	DOT11_RRM_STATS_GRP_ID_12,
	DOT11_RRM_STATS_GRP_ID_13,
	DOT11_RRM_STATS_GRP_ID_14,
	DOT11_RRM_STATS_GRP_ID_15,
	DOT11_RRM_STATS_GRP_ID_16
};

/* Statistics Group Report: Group Data length  */
#define DOT11_RRM_STATS_RPT_LEN_GRP_ID_0	28
typedef struct rrm_stat_group_0 {
	uint32	txfrag;
	uint32	txmulti;
	uint32	txfail;
	uint32	rxframe;
	uint32	rxmulti;
	uint32	rxbadfcs;
	uint32	txframe;
} rrm_stat_group_0_t;

#define DOT11_RRM_STATS_RPT_LEN_GRP_ID_1	24
typedef struct rrm_stat_group_1 {
	uint32	txretry;
	uint32	txretries;
	uint32	rxdup;
	uint32	txrts;
	uint32	rtsfail;
	uint32	ackfail;
} rrm_stat_group_1_t;

/* group 2-9 use same qos data structure (tid 0-7), total 52 bytes */
#define DOT11_RRM_STATS_RPT_LEN_GRP_ID_2_9	52
typedef struct rrm_stat_group_qos {
	uint32	txfrag;
	uint32	txfail;
	uint32	txretry;
	uint32	txretries;
	uint32	rxdup;
	uint32	txrts;
	uint32	rtsfail;
	uint32	ackfail;
	uint32	rxfrag;
	uint32	txframe;
	uint32	txdrop;
	uint32	rxmpdu;
	uint32	rxretries;
} rrm_stat_group_qos_t;

/* dot11BSSAverageAccessDelay Group (only available at an AP): 8 byte */
#define DOT11_RRM_STATS_RPT_LEN_GRP_ID_10	8
typedef BWL_PRE_PACKED_STRUCT struct rrm_stat_group_10 {
	uint8	apavgdelay;
	uint8	avgdelaybe;
	uint8	avgdelaybg;
	uint8	avgdelayvi;
	uint8	avgdelayvo;
	uint16	stacount;
	uint8	chanutil;
} BWL_POST_PACKED_STRUCT rrm_stat_group_10_t;

/* AMSDU, 40 bytes */
#define DOT11_RRM_STATS_RPT_LEN_GRP_ID_11	40
typedef struct rrm_stat_group_11 {
	uint32	txamsdu;
	uint32	amsdufail;
	uint32	amsduretry;
	uint32	amsduretries;
	uint32	txamsdubyte_h;
	uint32	txamsdubyte_l;
	uint32	amsduackfail;
	uint32	rxamsdu;
	uint32	rxamsdubyte_h;
	uint32	rxamsdubyte_l;
} rrm_stat_group_11_t;

/* AMPDU, 36 bytes */
#define DOT11_RRM_STATS_RPT_LEN_GRP_ID_12	36
typedef struct rrm_stat_group_12 {
	uint32	txampdu;
	uint32	txmpdu;
	uint32	txampdubyte_h;
	uint32	txampdubyte_l;
	uint32	rxampdu;
	uint32	rxmpdu;
	uint32	rxampdubyte_h;
	uint32	rxampdubyte_l;
	uint32	ampducrcfail;
} rrm_stat_group_12_t;

/* BACK etc, 36 bytes */
#define DOT11_RRM_STATS_RPT_LEN_GRP_ID_13	36
typedef struct rrm_stat_group_13 {
	uint32	rximpbarfail;
	uint32	rxexpbarfail;
	uint32	chanwidthsw;
	uint32	txframe20mhz;
	uint32	txframe40mhz;
	uint32	rxframe20mhz;
	uint32	rxframe40mhz;
	uint32	psmpgrantdur;
	uint32	psmpuseddur;
} rrm_stat_group_13_t;

/* RD Dual CTS etc, 36 bytes */
#define DOT11_RRM_STATS_RPT_LEN_GRP_ID_14	36
typedef struct rrm_stat_group_14 {
	uint32	grantrdgused;
	uint32	grantrdgunused;
	uint32	txframeingrantrdg;
	uint32	txbyteingrantrdg_h;
	uint32	txbyteingrantrdg_l;
	uint32	dualcts;
	uint32	dualctsfail;
	uint32	rtslsi;
	uint32	rtslsifail;
} rrm_stat_group_14_t;

/* bf and STBC etc, 20 bytes */
#define DOT11_RRM_STATS_RPT_LEN_GRP_ID_15	20
typedef struct rrm_stat_group_15 {
	uint32	bfframe;
	uint32	stbccts;
	uint32	stbcctsfail;
	uint32	nonstbccts;
	uint32	nonstbcctsfail;
} rrm_stat_group_15_t;

/* RSNA, 28 bytes */
#define DOT11_RRM_STATS_RPT_LEN_GRP_ID_16	28
typedef struct rrm_stat_group_16 {
	uint32	rsnacmacicverr;
	uint32	rsnacmacreplay;
	uint32	rsnarobustmgmtccmpreplay;
	uint32	rsnatkipicverr;
	uint32	rsnatkipicvreplay;
	uint32	rsnaccmpdecrypterr;
	uint32	rsnaccmpreplay;
} rrm_stat_group_16_t;

/* Transmit stream/category measurement request */
BWL_PRE_PACKED_STRUCT struct dot11_rmreq_tx_stream {
	uint8 id;		/* use dot11_rm_ie_t ? */
	uint8 len;
	uint8 token;
	uint8 mode;
	uint8 type;
	uint16 interval;
	uint16 duration;
	struct ether_addr	peer;
	uint8 traffic_id;
	uint8 bin0_range;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmreq_tx_stream dot11_rmreq_tx_stream_t;
#define DOT11_RMREQ_TXSTREAM_LEN	17

/** Transmit stream/category measurement report */
BWL_PRE_PACKED_STRUCT struct dot11_rmrep_tx_stream {
	uint32 starttime[2];
	uint16 duration;
	struct ether_addr	peer;
	uint8 traffic_id;
	uint8 reason;
	uint32 txmsdu_cnt;
	uint32 msdu_discarded_cnt;
	uint32 msdufailed_cnt;
	uint32 msduretry_cnt;
	uint32 cfpolls_lost_cnt;
	uint32 avrqueue_delay;
	uint32 avrtx_delay;
	uint8 bin0_range;
	uint32 bin0;
	uint32 bin1;
	uint32 bin2;
	uint32 bin3;
	uint32 bin4;
	uint32 bin5;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmrep_tx_stream dot11_rmrep_tx_stream_t;
#define DOT11_RMREP_TXSTREAM_LEN	71

typedef struct rrm_tscm {
	uint32 msdu_tx;
	uint32 msdu_exp;
	uint32 msdu_fail;
	uint32 msdu_retries;
	uint32 cfpolls_lost;
	uint32 queue_delay;
	uint32 tx_delay_sum;
	uint32 tx_delay_cnt;
	uint32 bin0_range_us;
	uint32 bin0;
	uint32 bin1;
	uint32 bin2;
	uint32 bin3;
	uint32 bin4;
	uint32 bin5;
} rrm_tscm_t;
enum {
	DOT11_FTM_LOCATION_SUBJ_LOCAL = 0,		/* Where am I? */
	DOT11_FTM_LOCATION_SUBJ_REMOTE = 1,		/* Where are you? */
	DOT11_FTM_LOCATION_SUBJ_THIRDPARTY = 2   /* Where is he/she? */
};

BWL_PRE_PACKED_STRUCT struct dot11_rmreq_ftm_lci {
	uint8 id;
	uint8 len;
	uint8 token;
	uint8 mode;
	uint8 type;
	uint8 subj;
	/* optional sub-elements */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmreq_ftm_lci dot11_rmreq_ftm_lci_t;

BWL_PRE_PACKED_STRUCT struct dot11_rmrep_ftm_lci {
	uint8 id;
	uint8 len;
	uint8 token;
	uint8 mode;
	uint8 type;
	uint8 lci_sub_id;
	uint8 lci_sub_len;
	/* optional LCI field */
	/* optional sub-elements */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmrep_ftm_lci dot11_rmrep_ftm_lci_t;

#define DOT11_FTM_LCI_SUBELEM_ID		0
#define DOT11_FTM_LCI_SUBELEM_LEN		2
#define DOT11_FTM_LCI_FIELD_LEN			16
#define DOT11_FTM_LCI_UNKNOWN_LEN		2

BWL_PRE_PACKED_STRUCT struct dot11_rmreq_ftm_civic {
	uint8 id;
	uint8 len;
	uint8 token;
	uint8 mode;
	uint8 type;
	uint8 subj;
	uint8 civloc_type;
	uint8 siu;	/* service interval units */
	uint16 si;  /* service interval */
	/* optional sub-elements */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmreq_ftm_civic dot11_rmreq_ftm_civic_t;
#define DOT11_RMREQ_CIVIC_LEN	10

BWL_PRE_PACKED_STRUCT struct dot11_rmrep_ftm_civic {
	uint8 id;
	uint8 len;
	uint8 token;
	uint8 mode;
	uint8 type;
	uint8 civloc_type;
	uint8 civloc_sub_id;
	uint8 civloc_sub_len;
	/* optional location civic field */
	/* optional sub-elements */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmrep_ftm_civic dot11_rmrep_ftm_civic_t;

#define DOT11_FTM_CIVIC_LOC_TYPE_RFC4776	0
#define DOT11_FTM_CIVIC_SUBELEM_ID		0
#define DOT11_FTM_CIVIC_SUBELEM_LEN		2
#define DOT11_FTM_CIVIC_LOC_SI_NONE		0
#define DOT11_FTM_CIVIC_TYPE_LEN		1
#define DOT11_FTM_CIVIC_UNKNOWN_LEN		3

/* Location Identifier measurement request */
BWL_PRE_PACKED_STRUCT struct dot11_rmreq_locid {
	uint8 id;
	uint8 len;
	uint8 token;
	uint8 mode;
	uint8 type;
	uint8 subj;
	uint8 siu;
	uint16 si;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmreq_locid dot11_rmreq_locid_t;
#define DOT11_RMREQ_LOCID_LEN	9

/* Location Identifier measurement report */
BWL_PRE_PACKED_STRUCT struct dot11_rmrep_locid {
	uint8 id;
	uint8 len;
	uint8 token;
	uint8 mode;
	uint8 type;
	uint8 exp_tsf[8];
	uint8 locid_sub_id;
	uint8 locid_sub_len;
	/* optional location identifier field */
	/* optional sub-elements */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmrep_locid dot11_rmrep_locid_t;
#define DOT11_LOCID_UNKNOWN_LEN		10
#define DOT11_LOCID_SUBELEM_ID		0

BWL_PRE_PACKED_STRUCT struct dot11_ftm_range_subel {
	uint8 id;
	uint8 len;
	uint16 max_age;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_ftm_range_subel dot11_ftm_range_subel_t;
#define DOT11_FTM_RANGE_SUBELEM_ID      4
#define DOT11_FTM_RANGE_SUBELEM_LEN     2

BWL_PRE_PACKED_STRUCT struct dot11_rmreq_ftm_range {
	uint8 id;
	uint8 len;
	uint8 token;
	uint8 mode;
	uint8 type;
	uint16 max_init_delay;		/* maximum random initial delay */
	uint8 min_ap_count;
	uint8 data[BCM_FLEX_ARRAY];
	/* neighbor report sub-elements */
	/* optional sub-elements */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmreq_ftm_range dot11_rmreq_ftm_range_t;
#define DOT11_RMREQ_FTM_RANGE_LEN 8

#define DOT11_FTM_RANGE_LEN		3
BWL_PRE_PACKED_STRUCT struct dot11_ftm_range_entry {
	uint32 start_tsf;		/* 4 lsb of tsf */
	struct ether_addr bssid;
	uint8 range[DOT11_FTM_RANGE_LEN];
	uint8 max_err[DOT11_FTM_RANGE_LEN];
	uint8  rsvd;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_ftm_range_entry dot11_ftm_range_entry_t;
#define DOT11_FTM_RANGE_ENTRY_MAX_COUNT   15

enum {
	DOT11_FTM_RANGE_ERROR_AP_INCAPABLE = 2,
	DOT11_FTM_RANGE_ERROR_AP_FAILED = 3,
	DOT11_FTM_RANGE_ERROR_TX_FAILED = 8,
	DOT11_FTM_RANGE_ERROR_MAX
};

BWL_PRE_PACKED_STRUCT struct dot11_ftm_range_error_entry {
	uint32 start_tsf;		/* 4 lsb of tsf */
	struct ether_addr bssid;
	uint8  code;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_ftm_range_error_entry dot11_ftm_range_error_entry_t;
#define DOT11_FTM_RANGE_ERROR_ENTRY_MAX_COUNT   11

BWL_PRE_PACKED_STRUCT struct dot11_rmrep_ftm_range {
    uint8 id;
    uint8 len;
    uint8 token;
    uint8 mode;
    uint8 type;
    uint8 entry_count;
    uint8 data[2]; /* includes pad */
	/*
	dot11_ftm_range_entry_t entries[entry_count];
	uint8 error_count;
	dot11_ftm_error_entry_t errors[error_count];
	 */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmrep_ftm_range dot11_rmrep_ftm_range_t;

#define DOT11_FTM_RANGE_REP_MIN_LEN     6       /* No extra byte for error_count */
#define DOT11_FTM_RANGE_ENTRY_CNT_MAX   15
#define DOT11_FTM_RANGE_ERROR_CNT_MAX   11
#define DOT11_FTM_RANGE_REP_FIXED_LEN   1       /* No extra byte for error_count */
/** Measurement pause request */
BWL_PRE_PACKED_STRUCT struct dot11_rmreq_pause_time {
	uint8 id;		/* use dot11_rm_ie_t ? */
	uint8 len;
	uint8 token;
	uint8 mode;
	uint8 type;
	uint16 pause_time;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_rmreq_pause_time dot11_rmreq_pause_time_t;
#define DOT11_RMREQ_PAUSE_LEN	7

/* Neighbor Report subelements ID (11k & 11v) */
#define DOT11_NGBR_TSF_INFO_SE_ID	1
#define DOT11_NGBR_CCS_SE_ID		2
#define DOT11_NGBR_BSSTRANS_PREF_SE_ID	3
#define DOT11_NGBR_BSS_TERM_DUR_SE_ID	4
#define DOT11_NGBR_BEARING_SE_ID	5
#define DOT11_NGBR_WIDE_BW_CHAN_SE_ID	6 /* proposed */

/** Neighbor Report, BSS Transition Candidate Preference subelement */
BWL_PRE_PACKED_STRUCT struct dot11_ngbr_bsstrans_pref_se {
	uint8 sub_id;
	uint8 len;
	uint8 preference;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_ngbr_bsstrans_pref_se dot11_ngbr_bsstrans_pref_se_t;
#define DOT11_NGBR_BSSTRANS_PREF_SE_LEN		1
#define DOT11_NGBR_BSSTRANS_PREF_SE_IE_LEN	3
#define DOT11_NGBR_BSSTRANS_PREF_SE_HIGHEST	0xff

/** Neighbor Report, BSS Termination Duration subelement */
BWL_PRE_PACKED_STRUCT struct dot11_ngbr_bss_term_dur_se {
	uint8 sub_id;
	uint8 len;
	uint8 tsf[8];
	uint16 duration;
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_ngbr_bss_term_dur_se dot11_ngbr_bss_term_dur_se_t;
#define DOT11_NGBR_BSS_TERM_DUR_SE_LEN	10

/* Neighbor Report BSSID Information Field */
#define DOT11_NGBR_BI_REACHABILTY_UNKN	0x0002
#define DOT11_NGBR_BI_REACHABILTY	0x0003
#define DOT11_NGBR_BI_SEC		0x0004
#define DOT11_NGBR_BI_KEY_SCOPE		0x0008
#define DOT11_NGBR_BI_CAP		0x03f0
#define DOT11_NGBR_BI_CAP_SPEC_MGMT	0x0010
#define DOT11_NGBR_BI_CAP_QOS		0x0020
#define DOT11_NGBR_BI_CAP_APSD		0x0040
#define DOT11_NGBR_BI_CAP_RDIO_MSMT	0x0080
#define DOT11_NGBR_BI_CAP_DEL_BA	0x0100
#define DOT11_NGBR_BI_CAP_IMM_BA	0x0200
#define DOT11_NGBR_BI_MOBILITY		0x0400
#define DOT11_NGBR_BI_HT		0x0800
#define DOT11_NGBR_BI_VHT		0x1000
#define DOT11_NGBR_BI_FTM		0x2000

/** Neighbor Report element (11k & 11v) */
BWL_PRE_PACKED_STRUCT struct dot11_neighbor_rep_ie {
	uint8 id;
	uint8 len;
	struct ether_addr bssid;
	uint32 bssid_info;
	uint8 reg;		/* Operating class */
	uint8 channel;
	uint8 phytype;
	uint8 data[BCM_FLEX_ARRAY];	/* Variable size subelements */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_neighbor_rep_ie dot11_neighbor_rep_ie_t;
#define DOT11_NEIGHBOR_REP_IE_FIXED_LEN	13u

/** Link Measurement */
BWL_PRE_PACKED_STRUCT struct dot11_lmreq {
	uint8 category;				/* category of action frame (5) */
	uint8 action;				/* radio measurement action */
	uint8 token;				/* dialog token */
	uint8 txpwr;				/* Transmit Power Used */
	uint8 maxtxpwr;				/* Max Transmit Power */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_lmreq dot11_lmreq_t;
#define DOT11_LMREQ_LEN	5

BWL_PRE_PACKED_STRUCT struct dot11_lmrep {
	uint8 category;				/* category of action frame (5) */
	uint8 action;				/* radio measurement action */
	uint8 token;				/* dialog token */
	dot11_tpc_rep_t tpc;			/* TPC element */
	uint8 rxant;				/* Receive Antenna ID */
	uint8 txant;				/* Transmit Antenna ID */
	uint8 rcpi;				/* RCPI */
	uint8 rsni;				/* RSNI */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_lmrep dot11_lmrep_t;
#define DOT11_LMREP_LEN	11

#define DOT11_MP_CAP_SPECTRUM			0x01	/* d11 cap. spectrum */
#define DOT11_MP_CAP_SHORTSLOT			0x02	/* d11 cap. shortslot */
/* Measurement Pilot */
BWL_PRE_PACKED_STRUCT struct dot11_mprep {
	uint8 cap_info;				/* Condensed capability Info. */
	uint8 country[2];				/* Condensed country string */
	uint8 opclass;				/* Op. Class */
	uint8 channel;				/* Channel */
	uint8 mp_interval;			/* Measurement Pilot Interval */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_mprep dot11_mprep_t;
#define DOT11_MPREP_LEN	6

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif /* _802_11k_h_ */
