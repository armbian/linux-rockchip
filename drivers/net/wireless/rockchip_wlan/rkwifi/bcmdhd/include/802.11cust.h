/*
 * Customer specific types and constants relating to 802.11
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

#ifndef _802_11cust_h_
#define _802_11cust_h_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

/* Action frame type for vendor specific action frames */
#define	VS_AF_TYPE	221

#ifdef IBSS_RMC
/* customer's OUI */
#define RMC_PROP_OUI		"\x00\x16\x32"
#endif

/* WFA definitions for LEGACY P2P */

#ifndef CISCO_AIRONET_OUI
#define CISCO_AIRONET_OUI	"\x00\x40\x96"	/* Cisco AIRONET OUI */
#endif

/* Single PMK IE */
#define CCX_SPMK_TYPE	3	/* CCX Extended Cap IE type for SPMK */
/* CCX Extended Capability IE */
BWL_PRE_PACKED_STRUCT struct ccx_spmk_cap_ie {
	uint8 id;		/* 221, DOT11_MNG_PROPR_ID */
	uint8 len;
	uint8 oui[DOT11_OUI_LEN];	/* 00:40:96, CISCO_AIRONET_OUI */
	uint8 type;		/* 11 */
	uint8 cap;
} BWL_POST_PACKED_STRUCT;
typedef struct ccx_spmk_cap_ie ccx_spmk_cap_ie_t;

/* QoS FastLane IE. */
BWL_PRE_PACKED_STRUCT struct ccx_qfl_ie {
	uint8	id;		/* 221, DOT11_MNG_VS_ID */
	uint8	length;		/* 5 */
	uint8	oui[3];		/* 00:40:96 */
	uint8	type;		/* 11 */
	uint8	data;
} BWL_POST_PACKED_STRUCT;
typedef struct ccx_qfl_ie ccx_qfl_ie_t;
#define CCX_QFL_IE_TYPE	11
#define CCX_QFL_ENABLE_SHIFT	5
#define CCX_QFL_ENALBE (1 << CCX_QFL_ENABLE_SHIFT)

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif /* _802_11cust_h_ */
