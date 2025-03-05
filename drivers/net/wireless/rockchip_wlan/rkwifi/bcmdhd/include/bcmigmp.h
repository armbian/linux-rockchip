/*
 * Fundamental constants relating to IGMP Protocol
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

#ifndef _bcmigmp_h_
#define _bcmigmp_h_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

#define IGMP_V2 2u
#define IGMP_V3 3u

/* These fields are stored in network order */
BWL_PRE_PACKED_STRUCT struct igmpv2_hdr {
	uint8  type;				/* type */
	uint8  max_resp_time;			/* time */
	uint16 chksum;				/* checksum */
	uint8  group_addr[IPV4_ADDR_LEN];	/* group address */
} BWL_POST_PACKED_STRUCT;

/* these fields are stored in network order */
BWL_PRE_PACKED_STRUCT struct igmpv3_hdr {
	uint8  type;				/* type */
	uint8  rsvd_1;				/* reserved */
	uint16 chksum;				/* checksum */
	uint16  rsvd_2;				/* reserved */
	uint16  num_groups;			/* number of groups */
	uint8  data[];				/* data */
} BWL_POST_PACKED_STRUCT;

#define IGMPV3_MODE_IS_INCLUDE		1u
#define IGMPV3_MODE_IS_EXCLUDE		2u
#define IGMPV3_CHANGE_TO_INCLUDE_MODE	3u
#define IGMPV3_CHANGE_TO_EXCLUDE_MODE	4u
#define IGMPV3_ALLOW_NEW_SOURCES	5u
#define IGMPV3_BLOCK_OLD_SOURCES	6u

/* these fields are stored in network order */
BWL_PRE_PACKED_STRUCT struct igmpv3_group {
	uint8  record_type;			/* Record Type */
	uint8  aux_data_len;			/* Aux Data Len */
	uint16 number_sources;			/* Number of Sources */
	uint8  group_addr[IPV4_ADDR_LEN];	/* group address */
	uint8  data[];				/* data */
} BWL_POST_PACKED_STRUCT;

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif	/* #ifndef _bcmigmp_h_ */
