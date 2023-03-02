/******************************************************************************
 *
 * Copyright(c) 2007 - 2019 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/
#define _RTW_WOW_C_

#include <drv_types.h>

#if defined(CONFIG_WOWLAN) || defined(CONFIG_AP_WOWLAN)
	#include <linux/inetdevice.h>
	#define ETH_TYPE_OFFSET	12
	#define PROTOCOL_OFFSET	23
	#define IP_OFFSET	30
	#define IPv6_OFFSET	38
	#define IPv6_PROTOCOL_OFFSET	20
#endif

#ifdef CONFIG_WOWLAN

void rtw_init_wow(_adapter *padapter)
{
	struct registry_priv  *registry_par = &padapter->registrypriv;
	struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(padapter);
	struct wow_priv *wowpriv = adapter_to_wowlan(padapter);

#ifdef CONFIG_GPIO_WAKEUP
	struct dvobj_priv *dvobj = adapter_to_dvobj(padapter);
	void *phl = GET_PHL_INFO(dvobj);
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	struct rtw_wow_gpio_info *wow_gpio = &wowpriv->wow_gpio;
	u8 toggle_pulse = DEV2HST_TOGGLE, gpio_time_unit = 1, gpio_pulse_count = 3, gpio_pulse_period = 10, gpio_pulse_dura = 5;
	u8 gpio_pulse_en_a = 0, customer_id = 0, gpio_duration_unit_a = 0, gpio_pulse_count_a = 0, gpio_duration_a = 0, special_reason_a = 0;
#endif

#if defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_ANDROID_POWER)
	pwrctrlpriv->early_suspend.suspend = NULL;
	rtw_register_early_suspend(pwrctrlpriv);
#endif /* CONFIG_HAS_EARLYSUSPEND || CONFIG_ANDROID_POWER */
	pwrctrlpriv->wowlan_mode = _FALSE;
	pwrctrlpriv->wowlan_ap_mode = _FALSE;
	pwrctrlpriv->wowlan_p2p_mode = _FALSE;
	pwrctrlpriv->wowlan_in_resume = _FALSE;
	pwrctrlpriv->wowlan_last_wake_reason = 0;

#ifdef CONFIG_GPIO_WAKEUP
#ifdef ROKU_PRIVATE
	gpio_pulse_en_a = DEV2HST_PULSE;
	customer_id = 0x3f;
	gpio_duration_unit_a = 1;
	gpio_pulse_count_a = 1;
	gpio_duration_a = 5;
	special_reason_a = 0x21;
#endif /* ROKU_PRIVATE */
	/*default low active*/
	wow_gpio->gpio_active = HIGH_ACTIVE_DEV2HST;
	pwrctrlpriv->hst2dev_high_active = HIGH_ACTIVE_HST2DEV;
	wow_gpio->dev2hst_gpio = WAKEUP_GPIO_IDX;
	wow_gpio->toggle_pulse = toggle_pulse;
	wow_gpio->gpio_time_unit = gpio_time_unit;
	wow_gpio->gpio_pulse_dura = gpio_pulse_dura;
	wow_gpio->gpio_pulse_period = gpio_pulse_period;
	wow_gpio->gpio_pulse_count = gpio_pulse_count;

	wow_gpio->customer_id = customer_id;
	wow_gpio->gpio_pulse_en_a = gpio_pulse_en_a;
	wow_gpio->gpio_duration_unit_a = gpio_duration_unit_a;
	wow_gpio->gpio_duration_a = gpio_duration_a;
	wow_gpio->special_reason_a = special_reason_a;
	wow_gpio->gpio_pulse_count_a = gpio_pulse_count_a;

#ifdef CONFIG_RTW_ONE_PIN_GPIO
	wow_gpio->dev2hst_gpio_mode = RTW_AX_SW_IO_MODE_INPUT;
	status = rtw_phl_cfg_wow_set_sw_gpio_mode(phl, wow_gpio);
#else
	#ifdef CONFIG_WAKEUP_GPIO_INPUT_MODE
	wow_gpio->dev2hst_gpio_mode = RTW_AX_SW_IO_MODE_OUTPUT_OD;
	#else
	wow_gpio->dev2hst_gpio_mode = RTW_AX_SW_IO_MODE_OUTPUT_PP;
	#endif /*CONFIG_WAKEUP_GPIO_INPUT_MODE*/
	/* switch GPIO to open-drain or push-pull */
	status = rtw_phl_cfg_wow_set_sw_gpio_mode(phl, wow_gpio);
	wow_gpio->dev2hst_high = wow_gpio->gpio_active == 0 ? 1 : 0;
	status = rtw_phl_cfg_wow_sw_gpio_ctrl(phl, wow_gpio);
	RTW_INFO("%s: set GPIO_%d %d as default. status=%d\n",
		 __func__, WAKEUP_GPIO_IDX, wow_gpio->dev2hst_high, status);
#endif /* CONFIG_RTW_ONE_PIN_GPIO */
#endif /* CONFIG_GPIO_WAKEUP */

#ifdef CONFIG_WOWLAN
#ifdef CONFIG_LPS_1T1R
#define WOW_LPS_1T1R_FMT ", WOW_LPS_1T1R=%d"
#define WOW_LPS_1T1R_ARG , pwrctrlpriv->wowlan_lps_1t1r
#else
#define WOW_LPS_1T1R_FMT ""
#define WOW_LPS_1T1R_ARG
#endif

	pwrctrlpriv->wowlan_power_mgmt = padapter->registrypriv.wow_power_mgnt;
	pwrctrlpriv->wowlan_lps_level = padapter->registrypriv.wow_lps_level;
#ifdef CONFIG_LPS_1T1R
	pwrctrlpriv->wowlan_lps_1t1r = padapter->registrypriv.wow_lps_1t1r;
#endif

	RTW_INFO("%s: WOW_LPS_mode=%d, WOW_LPS_level=%d"WOW_LPS_1T1R_FMT"\n",
		__func__, pwrctrlpriv->wowlan_power_mgmt, pwrctrlpriv->wowlan_lps_level
		WOW_LPS_1T1R_ARG
	);

	if (!(registry_par->wakeup_event & BIT(3)))
		rtw_wow_pattern_clean(padapter, RTW_CUSTOMIZED_PATTERN);

	rtw_wow_pattern_clean(padapter, RTW_DEFAULT_PATTERN);

#ifdef CONFIG_PNO_SUPPORT
	pwrctrlpriv->pno_inited = _FALSE;
	pwrctrlpriv->pnlo_info = NULL;
	pwrctrlpriv->pscan_info = NULL;
	pwrctrlpriv->pno_ssid_list = NULL;
#endif /* CONFIG_PNO_SUPPORT */
	_rtw_mutex_init(&pwrctrlpriv->wowlan_pattern_cam_mutex);

	pwrctrlpriv->wowlan_aoac_rpt_loc = 0;
#endif /* CONFIG_WOWLAN */

}

void rtw_free_wow(_adapter *adapter)
{
	struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(adapter);
#ifdef CONFIG_PNO_SUPPORT
	if (pwrctrlpriv->pnlo_info != NULL)
		printk("****** pnlo_info memory leak********\n");

	if (pwrctrlpriv->pscan_info != NULL)
		printk("****** pscan_info memory leak********\n");

	if (pwrctrlpriv->pno_ssid_list != NULL)
		printk("****** pno_ssid_list memory leak********\n");
#endif
	_rtw_mutex_free(&pwrctrlpriv->wowlan_pattern_cam_mutex);

#if defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_ANDROID_POWER)
	rtw_unregister_early_suspend(pwrctrlpriv);
#endif /* CONFIG_HAS_EARLYSUSPEND || CONFIG_ANDROID_POWER */
}

void rtw_wowlan_set_pattern_cast_type(_adapter *adapter, struct rtw_wowcam_upd_info *wowcam_info)
{
	if (is_broadcast_mac_addr(wowcam_info->ptrn))
		wowcam_info->bc = 1;
	else if (is_multicast_mac_addr(wowcam_info->ptrn))
		wowcam_info->mc = 1;
	else if (!memcmp(wowcam_info->ptrn, adapter_mac_addr(adapter), ETH_ALEN))
		wowcam_info->uc = 1;
}

inline bool _rtw_wow_chk_cap(_adapter *adapter, u8 cap)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct wow_ctl_t *wow_ctl = &dvobj->wow_ctl;

	if (wow_ctl->wow_cap & cap)
		return _TRUE;
	return _FALSE;
}
bool rtw_wowlan_parser_pattern_cmd(u8 *input, char *pattern,
				   int *pattern_len, char *bit_mask)
{
	char *cp = NULL;
	size_t len = 0;
	int pos = 0, mask_pos = 0, res = 0;
	u8 member[2] = {0};

	/* To get the pattern string after "=", when we use :
	 * iwpriv wlanX pattern=XX:XX:..:XX
	 */
	cp = strchr(input, '=');
	if (cp) {
		*cp = 0;
		cp++;
		input = cp;
	}

	/* To take off the newline character '\n'(0x0a) at the end of pattern string,
	 * when we use echo xxxx > /proc/xxxx
	 */
	cp = strchr(input, '\n');
	if (cp)
		*cp = 0;

	while (input) {
		cp = strsep((char **)(&input), ":");

		if (bit_mask && (strcmp(cp, "-") == 0 ||
				 strcmp(cp, "xx") == 0 ||
				 strcmp(cp, "--") == 0)) {
			/* skip this byte and leave mask bit unset */
		} else {
			u8 hex;

			if (strlen(cp) != 2) {
				RTW_ERR("%s:[ERROR] hex len != 2, input=[%s]\n",
					__func__, cp);
				goto error;
			}

			if (hexstr2bin(cp, &hex, 1) < 0) {
				RTW_ERR("%s:[ERROR] pattern is invalid, input=[%s]\n",
					__func__, cp);
				goto error;
			}

			pattern[pos] = hex;
			mask_pos = pos / 8;
			if (bit_mask)
				bit_mask[mask_pos] |= 1 << (pos % 8);
		}

		pos++;
	}

	(*pattern_len) = pos;

	return _TRUE;
error:
	return _FALSE;
}

u8 rtw_wow_pattern_set(_adapter *adapter, struct rtw_wowcam_upd_info *wowcam_info,
		       enum pattern_type set_type)
{
	struct wow_priv *wowpriv = adapter_to_wowlan(adapter);
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	void *phl = GET_PHL_INFO(dvobj);
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	u8 cam_idx = MAX_WKFM_CAM_NUM;

	rtw_wowlan_set_pattern_cast_type(adapter, wowcam_info);
	status = rtw_phl_add_wow_ptrn_info(phl, wowcam_info, &cam_idx);
	if (status != RTW_PHL_STATUS_SUCCESS) {
		RTW_INFO("Add wow pattern fail(%d)\n", status);
		return _FAIL;
	}

	wowpriv->wow_ptrn_valid[cam_idx] = set_type;

	RTW_INFO("wowcam_id: %d, type: %d\n", cam_idx, set_type);

	return _SUCCESS;
}

void rtw_wow_pattern_clean(_adapter *adapter, enum pattern_type clean_type)
{
	struct wow_priv *wowpriv = adapter_to_wowlan(adapter);
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	void *phl = GET_PHL_INFO(dvobj);
	u8 cam_idx;

	for (cam_idx = 0; cam_idx < MAX_WKFM_CAM_NUM; cam_idx++) {
		if (wowpriv->wow_ptrn_valid[cam_idx] == clean_type) {
			rtw_phl_remove_wow_ptrn_info(phl, cam_idx);
			wowpriv->wow_ptrn_valid[cam_idx] = RTW_INVALID_PATTERN;
		}
	}
}

void rtw_set_default_pattern(_adapter *adapter)
{
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapter);
	struct mlme_ext_priv *pmlmeext = &adapter->mlmeextpriv;
	struct mlme_ext_info *pmlmeinfo = &pmlmeext->mlmext_info;
	struct rtw_wowcam_upd_info wowcam_info = {0};
	u8 index = 0;
	u8 multicast_addr[3] = {0x01, 0x00, 0x5e};
	u8 multicast_ip[4] = {0xe0, 0x28, 0x28, 0x2a};

	u8 unicast_mask[5] = {0x3f, 0x70, 0x80, 0xc0, 0x03};
	u8 icmpv6_mask[7] = {0x00, 0x70, 0x10, 0x00, 0xc0, 0xc0, 0x3f};
	u8 multicast_mask[5] = {0x07, 0x70, 0x80, 0xc0, 0x03};

	u8 ip_protocol[3] = {0x08, 0x00, 0x45};
	u8 ipv6_protocol[3] = {0x86, 0xdd, 0x60};

	u8 *target = NULL;

	/*TCP/ICMP unicast*/
	for (index = 0 ; index < DEFAULT_PATTERN_NUM ; index++) {
		_rtw_memset((void *)&wowcam_info, 0, sizeof(wowcam_info));

		switch (index) {
		case 0:
			target = wowcam_info.ptrn;
			_rtw_memcpy(target, adapter_mac_addr(adapter), ETH_ALEN);

			target += ETH_TYPE_OFFSET;
			_rtw_memcpy(target, &ip_protocol, sizeof(ip_protocol));

			/* TCP */
			target += (PROTOCOL_OFFSET - ETH_TYPE_OFFSET);
			_rtw_memset(target, 0x06, 1);

			target += (IP_OFFSET - PROTOCOL_OFFSET);

			_rtw_memcpy(target, pmlmeinfo->ip_addr, RTW_IP_ADDR_LEN);

			_rtw_memcpy(wowcam_info.mask,
				    &unicast_mask, sizeof(unicast_mask));

			wowcam_info.ptrn_len = IP_OFFSET + RTW_IP_ADDR_LEN;

			rtw_wow_pattern_set(adapter, &wowcam_info, RTW_DEFAULT_PATTERN);

			break;
		case 1:
			target = wowcam_info.ptrn;
			_rtw_memcpy(target, adapter_mac_addr(adapter), ETH_ALEN);

			target += ETH_TYPE_OFFSET;
			_rtw_memcpy(target, &ip_protocol, sizeof(ip_protocol));

			/* ICMP */
			target += (PROTOCOL_OFFSET - ETH_TYPE_OFFSET);
			_rtw_memset(target, 0x01, 1);

			target += (IP_OFFSET - PROTOCOL_OFFSET);
			_rtw_memcpy(target, pmlmeinfo->ip_addr, RTW_IP_ADDR_LEN);

			_rtw_memcpy(wowcam_info.mask,
				    &unicast_mask, sizeof(unicast_mask));

			wowcam_info.ptrn_len = IP_OFFSET + RTW_IP_ADDR_LEN;

			rtw_wow_pattern_set(adapter, &wowcam_info, RTW_DEFAULT_PATTERN);

			break;
#ifdef CONFIG_IPV6
		case 2:
			if (pwrpriv->wowlan_ns_offload_en == _TRUE) {
				target = wowcam_info.ptrn;
				target += ETH_TYPE_OFFSET;

				_rtw_memcpy(target, &ipv6_protocol,
					    sizeof(ipv6_protocol));

				/* ICMPv6 */
				target += (IPv6_PROTOCOL_OFFSET -
					   ETH_TYPE_OFFSET);
				_rtw_memset(target, 0x3a, 1);

				target += (IPv6_OFFSET - IPv6_PROTOCOL_OFFSET);
				_rtw_memcpy(target, pmlmeinfo->ip6_addr,
					    RTW_IPv6_ADDR_LEN);

				_rtw_memcpy(wowcam_info.mask,
					    &icmpv6_mask, sizeof(icmpv6_mask));

				wowcam_info.ptrn_len = IPv6_OFFSET + RTW_IPv6_ADDR_LEN;

				rtw_wow_pattern_set(adapter, &wowcam_info,
						    RTW_DEFAULT_PATTERN);
			}
			break;
#endif /*CONFIG_IPV6*/
		case 3:
			target = wowcam_info.ptrn;
			_rtw_memcpy(target, &multicast_addr, sizeof(multicast_addr));

			target += ETH_TYPE_OFFSET;
			_rtw_memcpy(target, &ip_protocol, sizeof(ip_protocol));

			/* UDP */
			target += (PROTOCOL_OFFSET - ETH_TYPE_OFFSET);
			_rtw_memset(target, 0x11, 1);

			target += (IP_OFFSET - PROTOCOL_OFFSET);
			_rtw_memcpy(target, &multicast_ip, sizeof(multicast_ip));

			_rtw_memcpy(wowcam_info.mask,
				    &multicast_mask, sizeof(multicast_mask));

			wowcam_info.ptrn_len = IP_OFFSET + sizeof(multicast_ip);

			rtw_wow_pattern_set(adapter, &wowcam_info, RTW_DEFAULT_PATTERN);

			break;
		default:
			break;
		}
	}

	return;
}

void rtw_dump_priv_pattern(_adapter *adapter, u8 idx)
{
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(adapter);
	char str_1[128];
	char *p_str;
	u8 val8 = 0;
	int i = 0, j = 0, len = 0, max_len = 0;

	RTW_INFO("=========[%d]========\n", idx);

	RTW_INFO(">>>priv_pattern_content:\n");
	p_str = str_1;
	max_len = sizeof(str_1);
	for (i = 0 ; i < MAX_WKFM_PATTERN_SIZE / 8 ; i++) {
		_rtw_memset(p_str, 0, max_len);
		len = 0;
		for (j = 0 ; j < 8 ; j++) {
			val8 = pwrctl->patterns[idx].content[i * 8 + j];
			len += snprintf(p_str + len, max_len - len,
					"%02x ", val8);
		}
		RTW_INFO("%s\n", p_str);
	}

	RTW_INFO(">>>priv_pattern_mask:\n");
	for (i = 0 ; i < MAX_WKFM_SIZE / 8 ; i++) {
		_rtw_memset(p_str, 0, max_len);
		len = 0;
		for (j = 0 ; j < 8 ; j++) {
			val8 = pwrctl->patterns[idx].mask[i * 8 + j];
			len += snprintf(p_str + len, max_len - len,
					"%02x ", val8);
		}
		RTW_INFO("%s\n", p_str);
	}

	RTW_INFO(">>>priv_pattern_len:\n");
	RTW_INFO("%s: len: %d\n", __func__, pwrctl->patterns[idx].len);
}

void rtw_wow_pattern_sw_dump(_adapter *adapter)
{
	int i;

	RTW_INFO("********[RTK priv-patterns]*********\n");
	for (i = 0 ; i < MAX_WKFM_CAM_NUM; i++)
		rtw_dump_priv_pattern(adapter, i);
}

void rtw_get_sec_iv(_adapter *padapter, u8 *pcur_dot11txpn, u8 *StaAddr)
{
	struct sta_info		*psta;
	struct security_priv *psecpriv = &padapter->securitypriv;

	_rtw_memset(pcur_dot11txpn, 0, 8);
	if (NULL == StaAddr)
		return;
	psta = rtw_get_stainfo(&padapter->stapriv, StaAddr);
	RTW_INFO("%s(): StaAddr: %02x %02x %02x %02x %02x %02x\n",
		 __func__, StaAddr[0], StaAddr[1], StaAddr[2],
		 StaAddr[3], StaAddr[4], StaAddr[5]);

	if (psta) {
		if ((psecpriv->dot11PrivacyAlgrthm == _AES_) ||
		    (psecpriv->dot11PrivacyAlgrthm == _CCMP_256_))
			AES_IV(pcur_dot11txpn, psta->dot11txpn, 0);
		else if (psecpriv->dot11PrivacyAlgrthm == _TKIP_)
			TKIP_IV(pcur_dot11txpn, psta->dot11txpn, 0);
		else if ((psecpriv->dot11PrivacyAlgrthm == _GCMP_) ||
			 (psecpriv->dot11PrivacyAlgrthm == _GCMP_256_))
			GCMP_IV(pcur_dot11txpn, psta->dot11txpn, 0);
	}
}

void rtw_construct_remote_control_info(_adapter *adapter,
				       struct rtw_remote_wake_ctrl_info *ctrl_info)
{
	struct security_priv *securitypriv = &adapter->securitypriv;
	struct stainfo_rxcache *rxcache = NULL;
	struct sta_info *sta = NULL;
	u8 gtk_rx_iv[4][IV_LENGTH] = {0};
	u8 tid_id = 0;
	u8 i = 0;

	sta = rtw_get_stainfo(&adapter->stapriv, get_bssid(&adapter->mlmepriv));

	if (!sta) {
		rtw_warn_on(1);
		return;
	}

	rxcache = &sta->sta_recvpriv.rxcache;

	rtw_get_sec_iv(adapter, ctrl_info->ptk_tx_iv, get_bssid(&adapter->mlmepriv));
	RTW_INFO("[wow] ptk_tx_iv = " IV_FMT "\n", IV_ARG(ctrl_info->ptk_tx_iv));

	ctrl_info->valid_check = REMOTECTRL_INFO_VALID_CHECK;
	ctrl_info->symbol_check_en |= REMOTECTRL_INFO_SYMBOL_CHK_PTK |
				      REMOTECTRL_INFO_SYMBOL_CHK_GTK;

	ctrl_info->gtk_key_idx = securitypriv->dot118021XGrpKeyid;
	RTW_INFO("[wow] gtk_key_idx = %d\n", ctrl_info->gtk_key_idx);

	tid_id = rxcache->last_tid;
	_rtw_memcpy(ctrl_info->ptk_rx_iv, rxcache->iv[tid_id], IV_LENGTH);
	RTW_INFO("[wow] ptk_rx_iv = " IV_FMT "\n", IV_ARG(ctrl_info->ptk_rx_iv));

	for (i = 0; i < 4; i++) {
		rtw_pn_to_iv(securitypriv->iv_seq[i], gtk_rx_iv[i], i,
			     securitypriv->dot118021XGrpPrivacy);
		RTW_INFO("[wow] gtk_rx_iv[%u] = " IV_FMT "\n", i, IV_ARG(gtk_rx_iv[i]));
	}
	_rtw_memcpy(ctrl_info->gtk_rx_iv_idx0, gtk_rx_iv[0], IV_LENGTH);
	_rtw_memcpy(ctrl_info->gtk_rx_iv_idx1, gtk_rx_iv[1], IV_LENGTH);
	_rtw_memcpy(ctrl_info->gtk_rx_iv_idx2, gtk_rx_iv[2], IV_LENGTH);
	_rtw_memcpy(ctrl_info->gtk_rx_iv_idx3, gtk_rx_iv[3], IV_LENGTH);
}

void rtw_wow_lps_level_decide(_adapter *adapter, u8 wow_en)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct pwrctrl_priv *pwrpriv = dvobj_to_pwrctl(dvobj);

	if (wow_en) {
		pwrpriv->lps_level_bk = pwrpriv->lps_level;
		pwrpriv->lps_level = pwrpriv->wowlan_lps_level;
		#ifdef CONFIG_LPS_1T1R
		pwrpriv->lps_1t1r_bk = pwrpriv->lps_1t1r;
		pwrpriv->lps_1t1r = pwrpriv->wowlan_lps_1t1r;
		#endif
	} else {
		pwrpriv->lps_level = pwrpriv->lps_level_bk;
		#ifdef CONFIG_LPS_1T1R
		pwrpriv->lps_1t1r = pwrpriv->lps_1t1r_bk;
		#endif
	}
}

int rtw_pm_set_wow_lps(_adapter *padapter, u8 mode)
{
	int	ret = 0;
	struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(padapter);

	if (mode < PM_PS_MODE_NUM) {
		if (pwrctrlpriv->wowlan_power_mgmt != mode) 
			pwrctrlpriv->wowlan_power_mgmt = mode;
	} else
		ret = -EINVAL;

	return ret;
}
int rtw_pm_set_wow_lps_level(_adapter *padapter, u8 level)
{
	int	ret = 0;
	struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(padapter);

	if (level < LPS_LEVEL_MAX)
		pwrctrlpriv->wowlan_lps_level = level;
	else
		ret = -EINVAL;

	return ret;
}
#ifdef CONFIG_LPS_1T1R
int rtw_pm_set_wow_lps_1t1r(_adapter *padapter, u8 en)
{
	int	ret = 0;
	struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(padapter);

	en = en ? 1 : 0;
	pwrctrlpriv->wowlan_lps_1t1r = en;

	return ret;
}
#endif /* CONFIG_LPS_1T1R */

#ifdef CONFIG_GTK_OL
void _update_aoac_rpt_phase_0(_adapter *adapter, struct rtw_aoac_report *aoac_info)
{
	struct security_priv *securitypriv = &adapter->securitypriv;
	struct stainfo_rxcache *rxcache = NULL;
	struct sta_info *sta = NULL;
	u8 pn[8] = {0};
	u8 gtk_key_idx = 0;
	u8 i = 0;

	/* handle ptk rx iv */
	/* This Rx IV has no effect, the driver does not drop unicast packets
	 * due to different PNs. (Refer to recv_ucast_pn_decache)
	 */
	sta = rtw_get_stainfo(&adapter->stapriv, get_bssid(&adapter->mlmepriv));
	if (sta) {
		if (rtw_iv_to_pn(aoac_info->ptk_rx_iv, pn, NULL,
				 securitypriv->dot11PrivacyAlgrthm)) {
			rxcache = &sta->sta_recvpriv.rxcache;
			for (i = 0; i < TID_NUM; i++)
				_rtw_memcpy(rxcache->iv[i], aoac_info->ptk_rx_iv,
					    IV_LENGTH);
			sta->dot11rxpn.val = RTW_GET_LE64(pn);
			RTW_INFO("[wow] ptk_rx_pn = " PN_FMT "\n", PN_ARG(pn));
		}
	}

	/* handle gtk rx iv */
	gtk_key_idx = aoac_info->key_idx;
	if (rtw_iv_to_pn(aoac_info->gtk_rx_iv[gtk_key_idx], pn, NULL,
			 securitypriv->dot118021XGrpPrivacy)) {
		_rtw_memcpy(securitypriv->iv_seq[gtk_key_idx], pn, 8);
		RTW_INFO("[wow] gtk_rx_pn[%u] = " PN_FMT "\n", gtk_key_idx, PN_ARG(pn));
	}
}

void _update_aoac_rpt_phase_1(_adapter *adapter, struct rtw_aoac_report *aoac_info)
{
	struct security_priv *securitypriv = &adapter->securitypriv;
	struct sta_info *sta = NULL;
	u8 pn[8] = {0};
	u8 gtk_key_idx = 0;
	u8 key_len = 0;
	u8 i = 0;

	/* handle ptk tx iv */
	sta = rtw_get_stainfo(&adapter->stapriv, get_bssid(&adapter->mlmepriv));
	if (sta) {
		if (rtw_iv_to_pn(aoac_info->ptk_tx_iv, pn, NULL,
				 securitypriv->dot11PrivacyAlgrthm)) {
			sta->dot11txpn.val = RTW_GET_LE64(pn);
			RTW_INFO("[wow] ptk_tx_pn = " PN_FMT "\n", PN_ARG(pn));
		}
	}

	if (aoac_info->rekey_ok) {
		/* update gtk key */
		gtk_key_idx = aoac_info->key_idx;
		securitypriv->dot118021XGrpKeyid = gtk_key_idx;

		switch (securitypriv->dot118021XGrpPrivacy) {
		case _TKIP_:
		case _AES_:
		case _GCMP_:
			key_len = 16;
			break;
		case _GCMP_256_:
		case _CCMP_256_:
			key_len = 32;
			break;
		default:
			key_len = 0;
		}

		if (key_len)
			_rtw_memcpy(securitypriv->dot118021XGrpKey[gtk_key_idx].skey,
				    aoac_info->gtk, key_len);
		/* update tkip dot118021XGrptxmickey dot118021XGrprxmickey */
		if (securitypriv->dot118021XGrpPrivacy == _TKIP_) {
			/* The order of the GTK Tx/Rx mic keys in the AOAC report is
			 * reversed compared to the GTK Tx/Rx mic keys provided by
			 * wpa_supplicant.
			 */
			_rtw_memcpy(securitypriv->dot118021XGrptxmickey[gtk_key_idx].skey,
				    &aoac_info->gtk[24], 8);
			_rtw_memcpy(securitypriv->dot118021XGrprxmickey[gtk_key_idx].skey,
				    &aoac_info->gtk[16], 8);
		}
		rtw_set_key(adapter, securitypriv, gtk_key_idx, 1, _TRUE);

		/* update eapol replay_counter */
		_rtw_memcpy(sta->replay_ctr, aoac_info->eapol_key_replay_count,
			    RTW_REPLAY_CTR_LEN);
	} else {
		RTW_INFO("[wow] no rekey event\n");
	}

	for (i = 0; i < 4; i++) {
		if (rtw_iv_to_pn(aoac_info->gtk_rx_iv[i], pn, NULL,
				 securitypriv->dot118021XGrpPrivacy)) {
			_rtw_memcpy(securitypriv->iv_seq[i], pn, 8);
			RTW_INFO("[wow] gtk_rx_pn[%u] = " PN_FMT "\n", i, PN_ARG(pn));
		}
	}
}

void rtw_update_gtk_ofld_info(void *drv_priv, struct rtw_aoac_report *aoac_info,
			      u8 aoac_report_get_ok, u8 rx_ready)
{
	struct dvobj_priv *dvobj = (struct dvobj_priv *)drv_priv;
	_adapter *adapter = dvobj_get_primary_adapter(dvobj);

	if (!rx_ready)
		_update_aoac_rpt_phase_0(adapter, aoac_info);
	else
		_update_aoac_rpt_phase_1(adapter, aoac_info);
}
#endif /* CONFIG_GTK_OL */
#endif /* CONFIG_WOWLAN */

#ifdef CONFIG_PNO_SUPPORT
#define CSCAN_TLV_TYPE_SSID_IE	'S'
#define CIPHER_IE "key_mgmt="
#define CIPHER_NONE "NONE"
#define CIPHER_WPA_PSK "WPA-PSK"
#define CIPHER_WPA_EAP "WPA-EAP IEEE8021X"
/*
 *  SSIDs list parsing from cscan tlv list
 */
int rtw_parse_ssid_list_tlv(char **list_str, pno_ssid_t *ssid,
			    int max, int *bytes_left)
{
	char *str;

	int idx = 0;

	if ((list_str == NULL) || (*list_str == NULL) || (*bytes_left < 0)) {
		RTW_INFO("%s error paramters\n", __func__);
		return -1;
	}

	str = *list_str;
	while (*bytes_left > 0) {

		if (str[0] != CSCAN_TLV_TYPE_SSID_IE) {
			*list_str = str;
			RTW_INFO("nssid=%d left_parse=%d %d\n", idx, *bytes_left, str[0]);
			return idx;
		}

		/* Get proper CSCAN_TLV_TYPE_SSID_IE */
		*bytes_left -= 1;
		str += 1;

		if (str[0] == 0) {
			/* Broadcast SSID */
			ssid[idx].SSID_len = 0;
			_rtw_memset((char *)ssid[idx].SSID, 0x0, WLAN_SSID_MAXLEN);
			*bytes_left -= 1;
			str += 1;

			RTW_INFO("BROADCAST SCAN  left=%d\n", *bytes_left);
		} else if (str[0] <= WLAN_SSID_MAXLEN) {
			/* Get proper SSID size */
			ssid[idx].SSID_len = str[0];
			*bytes_left -= 1;
			str += 1;

			/* Get SSID */
			if (ssid[idx].SSID_len > *bytes_left) {
				RTW_INFO("%s out of memory range len=%d but left=%d\n",
					__func__, ssid[idx].SSID_len, *bytes_left);
				return -1;
			}

			_rtw_memcpy((char *)ssid[idx].SSID, str, ssid[idx].SSID_len);

			*bytes_left -= ssid[idx].SSID_len;
			str += ssid[idx].SSID_len;

			RTW_INFO("%s :size=%d left=%d\n",
				(char *)ssid[idx].SSID, ssid[idx].SSID_len, *bytes_left);
		} else {
			RTW_INFO("### SSID size more that %d\n", str[0]);
			return -1;
		}

		if (idx++ >  max) {
			RTW_INFO("%s number of SSIDs more that %d\n", __func__, idx);
			return -1;
		}
	}

	*list_str = str;
	return idx;
}

int rtw_parse_cipher_list(struct pno_nlo_info *nlo_info, char *list_str)
{

	char *pch, *pnext, *pend;
	u8 key_len = 0, index = 0;

	pch = list_str;

	if (nlo_info == NULL || list_str == NULL) {
		RTW_INFO("%s error paramters\n", __func__);
		return -1;
	}

	while (strlen(pch) != 0) {
		pnext = strstr(pch, "key_mgmt=");
		if (pnext != NULL) {
			pch = pnext + strlen(CIPHER_IE);
			pend = strstr(pch, "}");
			if (strncmp(pch, CIPHER_NONE,
				    strlen(CIPHER_NONE)) == 0)
				nlo_info->ssid_cipher_info[index] = 0x00;
			else if (strncmp(pch, CIPHER_WPA_PSK,
					 strlen(CIPHER_WPA_PSK)) == 0)
				nlo_info->ssid_cipher_info[index] = 0x66;
			else if (strncmp(pch, CIPHER_WPA_EAP,
					 strlen(CIPHER_WPA_EAP)) == 0)
				nlo_info->ssid_cipher_info[index] = 0x01;
			index++;
			pch = pend + 1;
		} else
			break;
	}
	return 0;
}

int rtw_dev_nlo_info_set(struct pno_nlo_info *nlo_info, pno_ssid_t *ssid,
		 int num, int pno_time, int pno_repeat, int pno_freq_expo_max)
{

	int i = 0;
	struct file *fp;
	mm_segment_t fs;
	loff_t pos = 0;
	u8 *source = NULL;
	long len = 0;

	RTW_INFO("+%s+\n", __func__);

	nlo_info->fast_scan_period = pno_time;
	nlo_info->ssid_num = num & BIT_LEN_MASK_32(8);
	nlo_info->hidden_ssid_num = num & BIT_LEN_MASK_32(8);
	nlo_info->slow_scan_period = (pno_time * 2);
	nlo_info->fast_scan_iterations = 5;

	if (nlo_info->hidden_ssid_num > 8)
		nlo_info->hidden_ssid_num = 8;

	/* TODO: channel list and probe index is all empty. */
	for (i = 0 ; i < num ; i++) {
		nlo_info->ssid_length[i]
			= ssid[i].SSID_len;
	}

	/* cipher array */
	fp = filp_open("/data/misc/wifi/wpa_supplicant.conf", O_RDONLY,  0644);
	if (IS_ERR(fp)) {
		RTW_INFO("Error, wpa_supplicant.conf doesn't exist.\n");
		RTW_INFO("Error, cipher array using default value.\n");
		return 0;
	}

	len = i_size_read(fp->f_path.dentry->d_inode);
	if (len < 0 || len > 2048) {
		RTW_INFO("Error, file size is bigger than 2048.\n");
		RTW_INFO("Error, cipher array using default value.\n");
		return 0;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);

	source = rtw_zmalloc(2048);

	if (source != NULL) {
		len = vfs_read(fp, source, len, &pos);
		rtw_parse_cipher_list(nlo_info, source);
		rtw_mfree(source, 2048);
	}

	set_fs(fs);
	filp_close(fp, NULL);

	RTW_INFO("-%s-\n", __func__);
	return 0;
}

int rtw_dev_ssid_list_set(struct pno_ssid_list *pno_ssid_list,
			  pno_ssid_t *ssid, u8 num)
{

	int i = 0;
	if (num > MAX_PNO_LIST_COUNT)
		num = MAX_PNO_LIST_COUNT;

	for (i = 0 ; i < num ; i++) {
		_rtw_memcpy(&pno_ssid_list->node[i].SSID,
			    ssid[i].SSID, ssid[i].SSID_len);
		pno_ssid_list->node[i].SSID_len = ssid[i].SSID_len;
	}
	return 0;
}

int rtw_dev_scan_info_set(_adapter *padapter, pno_ssid_t *ssid,
	  unsigned char ch, unsigned char ch_offset, unsigned short bw_mode)
{

	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(padapter);
	struct pno_scan_info *scan_info = pwrctl->pscan_info;
	u8 band = ch <= 14 ? BAND_ON_24G : BAND_ON_5G;
	int i;

	scan_info->channel_num = MAX_SCAN_LIST_COUNT;
	scan_info->orig_ch = ch;
	scan_info->orig_bw = bw_mode;
	scan_info->orig_40_offset = ch_offset;

	for (i = 0 ; i < scan_info->channel_num ; i++) {
		if (i < 11)
			scan_info->ssid_channel_info[i].active = 1;
		else
			scan_info->ssid_channel_info[i].active = 0;

		scan_info->ssid_channel_info[i].timeout = 100;

		scan_info->ssid_channel_info[i].tx_power =
			phy_get_tx_power_index_ex(padapter, 0, CCK, MGN_1M, bw_mode, band, i + 1);

		scan_info->ssid_channel_info[i].channel = i + 1;
	}

	RTW_INFO("%s, channel_num: %d, orig_ch: %d, orig_bw: %d orig_40_offset: %d\n",
		 __func__, scan_info->channel_num, scan_info->orig_ch,
		 scan_info->orig_bw, scan_info->orig_40_offset);
	return 0;
}

int rtw_dev_pno_set(struct net_device *net, pno_ssid_t *ssid, int num,
		    int pno_time, int pno_repeat, int pno_freq_expo_max)
{

	_adapter *padapter = (_adapter *)rtw_netdev_priv(net);
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(padapter);
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;

	int ret = -1;

	if (num == 0) {
		RTW_INFO("%s, nssid is zero, no need to setup pno ssid list\n", __func__);
		return 0;
	}

	if (pwrctl == NULL) {
		RTW_INFO("%s, ERROR: pwrctl is NULL\n", __func__);
		return -1;
	} else {
		pwrctl->pnlo_info =
			(pno_nlo_info_t *)rtw_zmalloc(sizeof(pno_nlo_info_t));
		pwrctl->pno_ssid_list =
			(pno_ssid_list_t *)rtw_zmalloc(sizeof(pno_ssid_list_t));
		pwrctl->pscan_info =
			(pno_scan_info_t *)rtw_zmalloc(sizeof(pno_scan_info_t));
	}

	if (pwrctl->pnlo_info == NULL ||
	    pwrctl->pscan_info == NULL ||
	    pwrctl->pno_ssid_list == NULL) {
		RTW_INFO("%s, ERROR: alloc nlo_info, ssid_list, scan_info fail\n", __func__);
		goto failing;
	}

	pwrctl->wowlan_in_resume = _FALSE;

	pwrctl->pno_inited = _TRUE;
	/* NLO Info */
	ret = rtw_dev_nlo_info_set(pwrctl->pnlo_info, ssid, num,
				   pno_time, pno_repeat, pno_freq_expo_max);

	/* SSID Info */
	ret = rtw_dev_ssid_list_set(pwrctl->pno_ssid_list, ssid, num);

	/* SCAN Info */
	ret = rtw_dev_scan_info_set(padapter, ssid, pmlmeext->chandef.chan,
			    pmlmeext->chandef.offset, pmlmeext->chandef.bw);

	RTW_INFO("+%s num: %d, pno_time: %d, pno_repeat:%d, pno_freq_expo_max:%d+\n",
		 __func__, num, pno_time, pno_repeat, pno_freq_expo_max);

	return 0;

failing:
	if (pwrctl->pnlo_info) {
		rtw_mfree((u8 *)pwrctl->pnlo_info, sizeof(pno_nlo_info_t));
		pwrctl->pnlo_info = NULL;
	}
	if (pwrctl->pno_ssid_list) {
		rtw_mfree((u8 *)pwrctl->pno_ssid_list, sizeof(pno_ssid_list_t));
		pwrctl->pno_ssid_list = NULL;
	}
	if (pwrctl->pscan_info) {
		rtw_mfree((u8 *)pwrctl->pscan_info, sizeof(pno_scan_info_t));
		pwrctl->pscan_info = NULL;
	}

	return -1;
}

#ifdef CONFIG_PNO_SET_DEBUG
void rtw_dev_pno_debug(struct net_device *net)
{
	_adapter *padapter = (_adapter *)rtw_netdev_priv(net);
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(padapter);
	int i = 0, j = 0;

	RTW_INFO("*******NLO_INFO********\n");
	RTW_INFO("ssid_num: %d\n", pwrctl->pnlo_info->ssid_num);
	RTW_INFO("fast_scan_iterations: %d\n",
		 pwrctl->pnlo_info->fast_scan_iterations);
	RTW_INFO("fast_scan_period: %d\n", pwrctl->pnlo_info->fast_scan_period);
	RTW_INFO("slow_scan_period: %d\n", pwrctl->pnlo_info->slow_scan_period);



	for (i = 0 ; i < MAX_PNO_LIST_COUNT ; i++) {
		RTW_INFO("%d SSID (%s) length (%d) cipher(%x) channel(%d)\n",
			i, pwrctl->pno_ssid_list->node[i].SSID, pwrctl->pnlo_info->ssid_length[i],
			pwrctl->pnlo_info->ssid_cipher_info[i], pwrctl->pnlo_info->ssid_channel_info[i]);
	}

	RTW_INFO("******SCAN_INFO******\n");
	RTW_INFO("ch_num: %d\n", pwrctl->pscan_info->channel_num);
	RTW_INFO("orig_ch: %d\n", pwrctl->pscan_info->orig_ch);
	RTW_INFO("orig bw: %d\n", pwrctl->pscan_info->orig_bw);
	RTW_INFO("orig 40 offset: %d\n", pwrctl->pscan_info->orig_40_offset);
	for (i = 0 ; i < MAX_SCAN_LIST_COUNT ; i++) {
		RTW_INFO("[%02d] avtive:%d, timeout:%d, tx_power:%d, ch:%02d\n",
			 i, pwrctl->pscan_info->ssid_channel_info[i].active,
			 pwrctl->pscan_info->ssid_channel_info[i].timeout,
			 pwrctl->pscan_info->ssid_channel_info[i].tx_power,
			 pwrctl->pscan_info->ssid_channel_info[i].channel);
	}
	RTW_INFO("*****************\n");
}
#endif /* CONFIG_PNO_SET_DEBUG */
#endif /* CONFIG_PNO_SUPPORT */

