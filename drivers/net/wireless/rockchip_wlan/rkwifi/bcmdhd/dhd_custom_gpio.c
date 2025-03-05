/*
 * Customer code to add GPIO control during WLAN start/stop
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
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id$
 */

#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>
#include <bcmutils.h>
#include <dngl_stats.h>
#include <dhd.h>
#include <dhd_linux.h>

#include <wlioctl.h>

#include <dhd_dbg.h>

#ifndef BCMDONGLEHOST
#include <wlc_pub.h>
#include <wl_dbg.h>
#else
#define WL_ERROR(x) printf x
#define WL_TRACE(x)
#endif

#if defined(OOB_INTR_ONLY) || defined(BCMSPI_ANDROID)

#if defined(BCMLXSDMMC)
extern int sdioh_mmc_irq(int irq);
#endif /* (BCMLXSDMMC)  */

/* Customer specific Host GPIO defintion  */
static int dhd_oob_gpio_num = -1;

module_param(dhd_oob_gpio_num, int, 0644);
MODULE_PARM_DESC(dhd_oob_gpio_num, "DHD oob gpio number");

/* This function will return:
 *  1) return :  Host gpio interrupt number per customer platform
 *  2) irq_flags_ptr : Type of Host interrupt as Level or Edge
 *
 *  NOTE :
 *  Customer should check his platform definitions
 *  and his Host Interrupt spec
 *  to figure out the proper setting for his platform.
 *  Broadcom provides just reference settings as example.
 *
 */
int dhd_customer_oob_irq_map(void *adapter, unsigned long *irq_flags_ptr)
{
	int  host_oob_irq = 0;

#if defined(BOARD_HIKEY)
	host_oob_irq = wifi_platform_get_irq_number(adapter, irq_flags_ptr);

#else
#if defined(CUSTOM_OOB_GPIO_NUM)
	if (dhd_oob_gpio_num < 0) {
		dhd_oob_gpio_num = CUSTOM_OOB_GPIO_NUM;
	}
#endif /* CUSTOMER_OOB_GPIO_NUM */

	if (dhd_oob_gpio_num < 0) {
		WL_ERROR(("%s: ERROR customer specific Host GPIO is NOT defined \n",
		__FUNCTION__));
		return (dhd_oob_gpio_num);
	}

	WL_ERROR(("%s: customer specific Host GPIO number is (%d)\n",
	         __FUNCTION__, dhd_oob_gpio_num));

#endif /* BOARD_HIKER */

	return (host_oob_irq);
}
#endif /* defined(OOB_INTR_ONLY) || defined(BCMSPI_ANDROID) */

/* Customer function to control hw specific wlan gpios */
int
dhd_customer_gpio_wlan_ctrl(void *adapter, int onoff)
{
	int err = 0;

	return err;
}

#if 0
/* Function to get custom MAC address */
int
dhd_custom_get_mac_address(void *adapter, unsigned char *buf)
{
	int ret = 0;

	WL_TRACE(("%s Enter\n", __FUNCTION__));
	if (!buf)
		return -EINVAL;

	/* Customer access to MAC address stored outside of DHD driver */
#if defined(BOARD_HIKEY) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	ret = wifi_platform_get_mac_addr(adapter, buf);
#endif

#ifdef EXAMPLE_GET_MAC
	/* EXAMPLE code */
	{
		struct ether_addr ea_example = {{0x00, 0x11, 0x22, 0x33, 0x44, 0xFF}};
		bcopy((char *)&ea_example, buf, sizeof(struct ether_addr));
	}
#endif /* EXAMPLE_GET_MAC */

	return ret;
}
#endif /* GET_CUSTOM_MAC_ENABLE */

#ifdef SYNA_SAR_CUSTOMER_PARAMETER
struct customer_cntry_list {
	eCountry_flag_type  type;
	char                ccode[WLC_CNTRY_BUF_SZ];   /* Country code */
};

/* maximum: (A~Z+0~9) * (A~Z+0~9) = (26+10)*(26+10) = 1296 */
#define MAX_COUNTRY_LIST        500
struct customer_cntry_list syna_cntry_flag[MAX_COUNTRY_LIST] = {
	/* FCC TYPE */
	{SYNA_COUNTRY_TYPE_FCC, "AG"},
	{SYNA_COUNTRY_TYPE_FCC, "AI"},
	{SYNA_COUNTRY_TYPE_FCC, "AR"},
	{SYNA_COUNTRY_TYPE_FCC, "AW"},
	{SYNA_COUNTRY_TYPE_FCC, "BB"},
	{SYNA_COUNTRY_TYPE_FCC, "BL"},
	{SYNA_COUNTRY_TYPE_FCC, "BM"},
	{SYNA_COUNTRY_TYPE_FCC, "BO"},
	{SYNA_COUNTRY_TYPE_FCC, "BS"},
	{SYNA_COUNTRY_TYPE_FCC, "BZ"},
	{SYNA_COUNTRY_TYPE_FCC, "CA"},
	{SYNA_COUNTRY_TYPE_FCC, "CL"},
	{SYNA_COUNTRY_TYPE_FCC, "CO"},
	{SYNA_COUNTRY_TYPE_FCC, "CR"},
	{SYNA_COUNTRY_TYPE_FCC, "CW"},
	{SYNA_COUNTRY_TYPE_FCC, "DO"},
	{SYNA_COUNTRY_TYPE_FCC, "EC"},
	{SYNA_COUNTRY_TYPE_FCC, "FM"},
	{SYNA_COUNTRY_TYPE_FCC, "GT"},
	{SYNA_COUNTRY_TYPE_FCC, "GU"},
	{SYNA_COUNTRY_TYPE_FCC, "GY"},
	{SYNA_COUNTRY_TYPE_FCC, "HN"},
	{SYNA_COUNTRY_TYPE_FCC, "HT"},
	{SYNA_COUNTRY_TYPE_FCC, "IN"},
	{SYNA_COUNTRY_TYPE_FCC, "IR"},
	{SYNA_COUNTRY_TYPE_FCC, "JM"},
	{SYNA_COUNTRY_TYPE_FCC, "KH"},
	{SYNA_COUNTRY_TYPE_FCC, "KN"},
	{SYNA_COUNTRY_TYPE_FCC, "KR"},
	{SYNA_COUNTRY_TYPE_FCC, "KY"},
	{SYNA_COUNTRY_TYPE_FCC, "LC"},
	{SYNA_COUNTRY_TYPE_FCC, "MF"},
	{SYNA_COUNTRY_TYPE_FCC, "MP"},
	{SYNA_COUNTRY_TYPE_FCC, "NI"},
	{SYNA_COUNTRY_TYPE_FCC, "PA"},
	{SYNA_COUNTRY_TYPE_FCC, "PE"},
	{SYNA_COUNTRY_TYPE_FCC, "PG"},
	{SYNA_COUNTRY_TYPE_FCC, "PM"},
	{SYNA_COUNTRY_TYPE_FCC, "PW"},
	{SYNA_COUNTRY_TYPE_FCC, "PR"},
	{SYNA_COUNTRY_TYPE_FCC, "PY"},
	{SYNA_COUNTRY_TYPE_FCC, "SR"},
	{SYNA_COUNTRY_TYPE_FCC, "SV"},
	{SYNA_COUNTRY_TYPE_FCC, "SX"},
	{SYNA_COUNTRY_TYPE_FCC, "TC"},
	{SYNA_COUNTRY_TYPE_FCC, "TT"},
	{SYNA_COUNTRY_TYPE_FCC, "US"},
	{SYNA_COUNTRY_TYPE_FCC, "UY"},
	{SYNA_COUNTRY_TYPE_FCC, "VC"},
	{SYNA_COUNTRY_TYPE_FCC, "VE"},
	{SYNA_COUNTRY_TYPE_FCC, "VG"},
	{SYNA_COUNTRY_TYPE_FCC, "VI"},
	{SYNA_COUNTRY_TYPE_FCC, "VU"},
	{SYNA_COUNTRY_TYPE_FCC, "WF"},
	{SYNA_COUNTRY_TYPE_FCC, "WS"},
	{SYNA_COUNTRY_TYPE_INVALID, ""}
};

static int gSyna_country_flag_qty = 55;

static int syna_country_flag_find(char *cntry)
{
	char country[WLC_CNTRY_BUF_SZ];
	int  ret = BCME_NOTFOUND;
	int  n;

	if (!cntry) {
		DHD_ERROR(("%s: invalid cntry!\n", __FUNCTION__));
		return BCME_BADARG;
	} else {
		n = strcspn(cntry, " ,");
		if ((n != 2) && (n != 3)) {
			DHD_ERROR(("%s: invalid n=%d!\n", __FUNCTION__, n));
			return BCME_BUFTOOLONG;
		} else {
			strlcpy(country, cntry, WLC_CNTRY_BUF_SZ);
			country[n] = '\0';
			DHD_TRACE(("%s: here n=%d, country='%s'!\n",
			           __FUNCTION__, n, country));
		}
	}

	n = 0;
	while ((MAX_COUNTRY_LIST > n) &&
	       (SYNA_COUNTRY_TYPE_INVALID != syna_cntry_flag[n].type) &&
	       (0 < strlen(syna_cntry_flag[n].ccode))) {
		if (!strncmp(country, syna_cntry_flag[n].ccode, WLC_CNTRY_BUF_SZ)) {
			ret = n;
			break;
		} else {
			n++;
		}
	}

	/* sync up the quantity for appending new item */
	if ((MAX_COUNTRY_LIST > n) &&
	    (SYNA_COUNTRY_TYPE_INVALID == syna_cntry_flag[n].type)) {
		gSyna_country_flag_qty = n;
		DHD_TRACE(("%s: update qty=%d!\n", __FUNCTION__, n));
	}

	return ret;
}

eCountry_flag_type syna_country_check_type(char *cntry)
{
	int ret = SYNA_COUNTRY_TYPE_INVALID;
	int i;

	i = syna_country_flag_find(cntry);
	if (0 <= i) {
		ret = syna_cntry_flag[i].type;
	}

	return ret;
}

int syna_country_update_type_list(eCountry_flag_type type, char *list_str)
{
	char   *next = NULL;
	int     n = 0;
	size_t  len;

	if (SYNA_COUNTRY_TYPE_QTY <= type) {
		return BCME_BADARG;
	} else if ((next = list_str) == NULL) {
		return BCME_BADARG;
	}

	while ((len = strcspn(next, " ,")) > 0) {
		if (len >= WLC_CNTRY_BUF_SZ) {
			DHD_ERROR(("%s: string '%s' before ',' or ' ' is too long\n",
			           __FUNCTION__, next));
			return BCME_ERROR;
		}

		n = syna_country_flag_find(next);
		if (0 > n) {
			if (BCME_NOTFOUND == n) {
				if (MAX_COUNTRY_LIST <= (1 + gSyna_country_flag_qty)) {
					DHD_ERROR(("%s: too many ccode(max=%d) list: '%s'\n",
					           __FUNCTION__, MAX_COUNTRY_LIST, list_str));
					break;
				} else {
					/* adopt this new item */
					n = gSyna_country_flag_qty;
					gSyna_country_flag_qty++;
				}
			} else {
				DHD_ERROR(("%s: skip this invalid item '%s', err=%d\n",
				           __FUNCTION__, next, n));
			}
		}

		if (0 <= n) {
			/* update or add a new item */
			syna_cntry_flag[n].type = type;
			strlcpy(syna_cntry_flag[n].ccode, next, WLC_CNTRY_BUF_SZ);
			syna_cntry_flag[n].ccode[len] = '\0';
			DHD_TRACE(("%s: cntry list [%d] type=%d ccode=%s\n",
			           __FUNCTION__, n,
			           syna_cntry_flag[n].type, syna_cntry_flag[n].ccode));

			/* fill with a new blank item as terminate */
			if ((gSyna_country_flag_qty == (n + 1)) &&
				(MAX_COUNTRY_LIST > gSyna_country_flag_qty)) {
				syna_cntry_flag[n+1].type = SYNA_COUNTRY_TYPE_INVALID;
				syna_cntry_flag[n+1].ccode[0] = '\0';
			}
		}

		next += len;
		next += strspn(next, " ,");
	}

	return BCME_OK;
}
#endif /* SYNA_SAR_CUSTOMER_PARAMETER */

/* Customized Locale table : OPTIONAL feature */
const struct cntry_locales_custom translate_custom_table[] = {
/* Table should be filled out based on custom platform regulatory requirement */
#ifdef EXAMPLE_TABLE
	{"",   "XY", 4},  /* Universal if Country code is unknown or empty */
	{"US", "US", 69}, /* input ISO "US" to : US regrev 69 */
	{"CA", "US", 69}, /* input ISO "CA" to : US regrev 69 */
	{"EU", "EU", 5},  /* European union countries to : EU regrev 05 */
	{"AT", "EU", 5},
	{"BE", "EU", 5},
	{"BG", "EU", 5},
	{"CY", "EU", 5},
	{"CZ", "EU", 5},
	{"DK", "EU", 5},
	{"EE", "EU", 5},
	{"FI", "EU", 5},
	{"FR", "EU", 5},
	{"DE", "EU", 5},
	{"GR", "EU", 5},
	{"HU", "EU", 5},
	{"IE", "EU", 5},
	{"IT", "EU", 5},
	{"LV", "EU", 5},
	{"LI", "EU", 5},
	{"LT", "EU", 5},
	{"LU", "EU", 5},
	{"MT", "EU", 5},
	{"NL", "EU", 5},
	{"PL", "EU", 5},
	{"PT", "EU", 5},
	{"RO", "EU", 5},
	{"SK", "EU", 5},
	{"SI", "EU", 5},
	{"ES", "EU", 5},
	{"SE", "EU", 5},
	{"GB", "EU", 5},
	{"KR", "XY", 3},
	{"AU", "XY", 3},
	{"CN", "XY", 3}, /* input ISO "CN" to : XY regrev 03 */
	{"TW", "XY", 3},
	{"AR", "XY", 3},
	{"MX", "XY", 3},
	{"IL", "IL", 0},
	{"CH", "CH", 0},
	{"TR", "TR", 0},
	{"NO", "NO", 0},
#endif /* EXMAPLE_TABLE */
#if defined(BOARD_HIKEY)
#if defined(BCM4335_CHIP)
	{"",   "XZ", 11},  /* Universal if Country code is unknown or empty */
#endif
	{"AE", "AE", 1},
	{"AR", "AR", 1},
	{"AT", "AT", 1},
	{"AU", "AU", 2},
	{"BE", "BE", 1},
	{"BG", "BG", 1},
	{"BN", "BN", 1},
	{"CA", "CA", 2},
	{"CH", "CH", 1},
	{"CY", "CY", 1},
	{"CZ", "CZ", 1},
	{"DE", "DE", 3},
	{"DK", "DK", 1},
	{"EE", "EE", 1},
	{"ES", "ES", 1},
	{"FI", "FI", 1},
	{"FR", "FR", 1},
	{"GB", "GB", 1},
	{"GR", "GR", 1},
	{"HR", "HR", 1},
	{"HU", "HU", 1},
	{"IE", "IE", 1},
	{"IS", "IS", 1},
	{"IT", "IT", 1},
	{"ID", "ID", 1},
	{"JP", "JP", 8},
	{"KR", "KR", 24},
	{"KW", "KW", 1},
	{"LI", "LI", 1},
	{"LT", "LT", 1},
	{"LU", "LU", 1},
	{"LV", "LV", 1},
	{"MA", "MA", 1},
	{"MT", "MT", 1},
	{"MX", "MX", 1},
	{"NL", "NL", 1},
	{"NO", "NO", 1},
	{"PL", "PL", 1},
	{"PT", "PT", 1},
	{"PY", "PY", 1},
	{"RO", "RO", 1},
	{"SE", "SE", 1},
	{"SI", "SI", 1},
	{"SK", "SK", 1},
	{"TR", "TR", 7},
	{"TW", "TW", 1},
	{"IR", "XZ", 11},	/* Universal if Country code is IRAN, (ISLAMIC REPUBLIC OF) */
	{"SD", "XZ", 11},	/* Universal if Country code is SUDAN */
	{"SY", "XZ", 11},	/* Universal if Country code is SYRIAN ARAB REPUBLIC */
	{"GL", "XZ", 11},	/* Universal if Country code is GREENLAND */
	{"PS", "XZ", 11},	/* Universal if Country code is PALESTINIAN TERRITORY, OCCUPIED */
	{"TL", "XZ", 11},	/* Universal if Country code is TIMOR-LESTE (EAST TIMOR) */
	{"MH", "XZ", 11},	/* Universal if Country code is MARSHALL ISLANDS */
#endif /* BOARD_HIKEY */
};

/* Customized Locale convertor
*  input : ISO 3166-1 country abbreviation
*  output: customized cspec
*/
void
#ifdef CUSTOM_COUNTRY_CODE
get_customized_country_code(void *adapter, char *country_iso_code,
	wl_country_t *cspec, u32 flags)
#else
get_customized_country_code(void *adapter, char *country_iso_code, wl_country_t *cspec)
#endif /* CUSTOM_COUNTRY_CODE */
{
#if defined(OEM_ANDROID)
#if (defined(CUSTOMER_HW) || defined(CUSTOMER_HW2)) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39))

	struct cntry_locales_custom *cloc_ptr;

	if (!cspec)
		return;
#ifdef CUSTOM_COUNTRY_CODE
	cloc_ptr = wifi_platform_get_country_code(adapter, country_iso_code, flags);
#else
	cloc_ptr = wifi_platform_get_country_code(adapter, country_iso_code);
#endif /* CUSTOM_COUNTRY_CODE */

	if (cloc_ptr) {
		strlcpy(cspec->ccode, cloc_ptr->custom_locale, WLC_CNTRY_BUF_SZ);
		cspec->rev = cloc_ptr->custom_locale_rev;
	}
	return;
#else
	int size, i;

	size = ARRAYSIZE(translate_custom_table);

	if (cspec == 0)
		 return;

	if (size == 0)
		 return;

	for (i = 0; i < size; i++) {
		if (strcmp(country_iso_code, translate_custom_table[i].iso_abbrev) == 0) {
			memcpy(cspec->ccode,
				translate_custom_table[i].custom_locale, WLC_CNTRY_BUF_SZ);
			cspec->rev = translate_custom_table[i].custom_locale_rev;
			return;
		}
	}
#ifdef EXAMPLE_TABLE
	/* if no country code matched return first universal code from translate_custom_table */
	memcpy(cspec->ccode, translate_custom_table[0].custom_locale, WLC_CNTRY_BUF_SZ);
	cspec->rev = translate_custom_table[0].custom_locale_rev;
#endif /* EXMAPLE_TABLE */
	return;
#endif /* (defined(CUSTOMER_HW2) || defined(BOARD_HIKEY)) &&
	* (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36))
	*/
#endif /* OEM_ANDROID */
}
