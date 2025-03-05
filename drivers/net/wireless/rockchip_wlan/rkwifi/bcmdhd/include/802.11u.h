/*
 * Fundamental types and constants relating to 802.11u -
 * "Interworking with External Networks"
 *
 * IW - InterWorking
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

#ifndef _802_11u_h_
#define _802_11u_h_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

/* 802.11u GAS action frames */
#define GAS_REQUEST_ACTION_FRAME				10
#define GAS_RESPONSE_ACTION_FRAME				11
#define GAS_COMEBACK_REQUEST_ACTION_FRAME		12
#define GAS_COMEBACK_RESPONSE_ACTION_FRAME		13

/* 802.11u interworking access network options */
#define IW_ANT_MASK					0x0f
#define IW_INTERNET_MASK				0x10
#define IW_ASRA_MASK					0x20
#define IW_ESR_MASK					0x40
#define IW_UESA_MASK					0x80

/* 802.11u interworking access network type */
#define IW_ANT_PRIVATE_NETWORK				0
#define IW_ANT_PRIVATE_NETWORK_WITH_GUEST		1
#define IW_ANT_CHARGEABLE_PUBLIC_NETWORK		2
#define IW_ANT_FREE_PUBLIC_NETWORK			3
#define IW_ANT_PERSONAL_DEVICE_NETWORK			4
#define IW_ANT_EMERGENCY_SERVICES_NETWORK		5
#define IW_ANT_TEST_NETWORK				14
#define IW_ANT_WILDCARD_NETWORK				15

#define IW_ANT_LEN			1
#define IW_VENUE_LEN			2
#define IW_HESSID_LEN			6
#define IW_HESSID_OFF			(IW_ANT_LEN + IW_VENUE_LEN)
#define IW_MAX_LEN			(IW_ANT_LEN + IW_VENUE_LEN + IW_HESSID_LEN)

/* 802.11u advertisement protocol */
#define ADVP_ANQP_PROTOCOL_ID				0
#define ADVP_MIH_PROTOCOL_ID				1

/* 802.11u advertisement protocol masks */
#define ADVP_QRL_MASK					0x7f
#define ADVP_PAME_BI_MASK				0x80

/* 802.11u advertisement protocol values */
#define ADVP_QRL_REQUEST				0x00
#define ADVP_QRL_RESPONSE				0x7f
#define ADVP_PAME_BI_DEPENDENT				0x00
#define ADVP_PAME_BI_INDEPENDENT			ADVP_PAME_BI_MASK

/* 802.11u ANQP information ID */
#define ANQP_ID_QUERY_LIST				256
#define ANQP_ID_CAPABILITY_LIST				257
#define ANQP_ID_VENUE_NAME_INFO				258
#define ANQP_ID_EMERGENCY_CALL_NUMBER_INFO		259
#define ANQP_ID_NETWORK_AUTHENTICATION_TYPE_INFO	260
#define ANQP_ID_ROAMING_CONSORTIUM_LIST			261
#define ANQP_ID_IP_ADDRESS_TYPE_AVAILABILITY_INFO	262
#define ANQP_ID_NAI_REALM_LIST				263
#define ANQP_ID_G3PP_CELLULAR_NETWORK_INFO		264
#define ANQP_ID_AP_GEOSPATIAL_LOCATION			265
#define ANQP_ID_AP_CIVIC_LOCATION			266
#define ANQP_ID_AP_LOCATION_PUBLIC_ID_URI		267
#define ANQP_ID_DOMAIN_NAME_LIST			268
#define ANQP_ID_EMERGENCY_ALERT_ID_URI			269
#define ANQP_ID_EMERGENCY_NAI				271
#define ANQP_ID_NEIGHBOR_REPORT				272
#define ANQP_ID_VENDOR_SPECIFIC_LIST			56797

/* 802.11u ANQP ID len */
#define ANQP_INFORMATION_ID_LEN				2

/* 802.11u ANQP OUI */
#define ANQP_OUI_SUBTYPE				9

/* 802.11u venue name */
#define VENUE_LANGUAGE_CODE_SIZE			3
#define VENUE_NAME_SIZE					255

/* 802.11u venue groups */
#define VENUE_UNSPECIFIED				0
#define VENUE_ASSEMBLY					1
#define VENUE_BUSINESS					2
#define VENUE_EDUCATIONAL				3
#define VENUE_FACTORY					4
#define VENUE_INSTITUTIONAL				5
#define VENUE_MERCANTILE				6
#define VENUE_RESIDENTIAL				7
#define VENUE_STORAGE					8
#define VENUE_UTILITY					9
#define VENUE_VEHICULAR					10
#define VENUE_OUTDOOR					11

/* 802.11u network authentication type indicator */
#define NATI_UNSPECIFIED				-1
#define NATI_ACCEPTANCE_OF_TERMS_CONDITIONS		0
#define NATI_ONLINE_ENROLLMENT_SUPPORTED		1
#define NATI_HTTP_HTTPS_REDIRECTION			2
#define NATI_DNS_REDIRECTION				3

/* 802.11u IP address type availability - IPv6 */
#define IPA_IPV6_SHIFT					0
#define IPA_IPV6_MASK					(0x03 << IPA_IPV6_SHIFT)
#define	IPA_IPV6_NOT_AVAILABLE				0x00
#define IPA_IPV6_AVAILABLE				0x01
#define IPA_IPV6_UNKNOWN_AVAILABILITY			0x02

/* 802.11u IP address type availability - IPv4 */
#define IPA_IPV4_SHIFT					2
#define IPA_IPV4_MASK					(0x3f << IPA_IPV4_SHIFT)
#define	IPA_IPV4_NOT_AVAILABLE				0x00
#define IPA_IPV4_PUBLIC					0x01
#define IPA_IPV4_PORT_RESTRICT				0x02
#define IPA_IPV4_SINGLE_NAT				0x03
#define IPA_IPV4_DOUBLE_NAT				0x04
#define IPA_IPV4_PORT_RESTRICT_SINGLE_NAT		0x05
#define IPA_IPV4_PORT_RESTRICT_DOUBLE_NAT		0x06
#define IPA_IPV4_UNKNOWN_AVAILABILITY			0x07

/* 802.11u NAI realm encoding */
#define REALM_ENCODING_RFC4282				0
#define REALM_ENCODING_UTF8				1

/* 802.11u IANA EAP method type numbers */
#define REALM_EAP_TLS					13
#define REALM_EAP_LEAP					17
#define REALM_EAP_SIM					18
#define REALM_EAP_TTLS					21
#define REALM_EAP_AKA					23
#define REALM_EAP_PEAP					25
#define REALM_EAP_FAST					43
#define REALM_EAP_PSK					47
#define REALM_EAP_AKAP					50
#define REALM_EAP_EXPANDED				254

/* 802.11u authentication ID */
#define REALM_EXPANDED_EAP				1
#define REALM_NON_EAP_INNER_AUTHENTICATION		2
#define REALM_INNER_AUTHENTICATION_EAP			3
#define REALM_EXPANDED_INNER_EAP			4
#define REALM_CREDENTIAL				5
#define REALM_TUNNELED_EAP_CREDENTIAL			6
#define REALM_VENDOR_SPECIFIC_EAP			221

/* 802.11u non-EAP inner authentication type */
#define REALM_RESERVED_AUTH				0
#define REALM_PAP					1
#define REALM_CHAP					2
#define REALM_MSCHAP					3
#define REALM_MSCHAPV2					4

/* 802.11u credential type */
#define REALM_SIM					1
#define REALM_USIM					2
#define REALM_NFC					3
#define REALM_HARDWARE_TOKEN				4
#define REALM_SOFTOKEN					5
#define REALM_CERTIFICATE				6
#define REALM_USERNAME_PASSWORD				7
#define REALM_SERVER_SIDE				8
#define REALM_RESERVED_CRED				9
#define REALM_VENDOR_SPECIFIC_CRED			10

/* 802.11u 3GPP PLMN */
#define G3PP_GUD_VERSION				0
#define G3PP_PLMN_LIST_IE				0

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif /* _802_11u_h_ */
