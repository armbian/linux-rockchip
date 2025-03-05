/*
 * Fundamental types and constants relating to OWE (RFC 8110 and WFA spec) -
 * "Opportunistic Wireless Encryption"
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

#ifndef _802_11owe_h_
#define _802_11owe_h_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

BWL_PRE_PACKED_STRUCT struct dot11_dh_param_ie {
	uint8   id;	/* OWE */
	uint8   len;
	uint8   ext_id;	/* EXT_MNG_OWE_DH_PARAM_ID */
	uint16  group;
	uint8   pub_key[];
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_dh_param_ie dot11_dh_param_ie_t;

#define DOT11_DH_EXTID_OFFSET   (OFFSETOF(dot11_dh_param_ie_t, ext_id))

#define DOT11_OWE_DH_PARAM_IE(_ie) (\
	DOT11_MNG_IE_ID_EXT_MATCH(_ie, EXT_MNG_OWE_DH_PARAM_ID))

#define DOT11_MNG_OWE_IE_ID_EXT_INIT(_ie, _id, _len) do {\
	(_ie)->id = DOT11_MNG_ID_EXT_ID; \
	(_ie)->len = _len; \
	(_ie)->ext_id = _id; \
} while (0)

/* OWE definitions */
/* ID + len + OUI + OI type + BSSID + SSID_len */
#define OWE_TRANS_MODE_IE_FIXED_LEN  13u

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif /* _802_11owe_h_ */
