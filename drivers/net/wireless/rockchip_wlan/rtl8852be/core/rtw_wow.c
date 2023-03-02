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
	struct rtw_dev2hst_gpio_info *d2h_gpio_info = &wow_gpio->d2h_gpio_info;
	u8 toggle_pulse = DEV2HST_TOGGLE, gpio_time_unit = 1, gpio_pulse_count = 3;
	u8 gpio_pulse_period = 20, gpio_pulse_dura = 10;
	u8 rsn_a_en = 0, rsn_a = 0, rsn_a_time_unit = 0, rsn_a_toggle_pulse = DEV2HST_TOGGLE;
	u8 rsn_a_pulse_count = 0, rsn_a_pulse_period = 0, rsn_a_pulse_duration = 0;
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
	rsn_a_en = 1;
	rsn_a_toggle_pulse = DEV2HST_PULSE;
	rsn_a_time_unit = 1;
	rsn_a_pulse_count = 1;
	rsn_a_pulse_duration = 5;
	rsn_a_pulse_period = 10;
	rsn_a = 0x21;
#endif /* ROKU_PRIVATE */
	pwrctrlpriv->hst2dev_high_active = HIGH_ACTIVE_HST2DEV;
	/*default low active*/
	d2h_gpio_info->gpio_active = HIGH_ACTIVE_DEV2HST;

	d2h_gpio_info->toggle_pulse = toggle_pulse;
	d2h_gpio_info->gpio_time_unit = gpio_time_unit;
	d2h_gpio_info->gpio_pulse_dura = gpio_pulse_dura;
	d2h_gpio_info->gpio_pulse_period = gpio_pulse_period;
	d2h_gpio_info->gpio_pulse_count = gpio_pulse_count;

	wow_gpio->dev2hst_gpio = WAKEUP_GPIO_IDX;
	d2h_gpio_info->rsn_a_en = rsn_a_en;
	d2h_gpio_info->rsn_a_toggle_pulse = rsn_a_toggle_pulse;
	d2h_gpio_info->rsn_a_time_unit = rsn_a_time_unit;
	d2h_gpio_info->rsn_a = rsn_a;
	d2h_gpio_info->rsn_a_pulse_duration = rsn_a_pulse_duration;
	d2h_gpio_info->rsn_a_pulse_period = rsn_a_pulse_period;
	d2h_gpio_info->rsn_a_pulse_count = rsn_a_pulse_count;

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
	wow_gpio->dev2hst_high = d2h_gpio_info->gpio_active == 0 ? 1 : 0;
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

	_rtw_mutex_init(&pwrctrlpriv->wowlan_pattern_cam_mutex);

	pwrctrlpriv->wowlan_aoac_rpt_loc = 0;
#endif /* CONFIG_WOWLAN */

}

void rtw_free_wow(_adapter *adapter)
{
	struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(adapter);

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
static void nlo_scan_ch_init(struct rtw_nlo_info *wow_nlo,
			     struct ieee80211_channel **channels,
			     u32 n_channels)
{
	u8 i = 0;

	while (i < MAX_NLO_CHANNEL && i < n_channels) {
		wow_nlo->channel_list[i].bw = CHANNEL_WIDTH_20;
		wow_nlo->channel_list[i].center_chan = channels[i]->hw_value;
		wow_nlo->channel_list[i].chan = channels[i]->hw_value;
		wow_nlo->channel_list[i].period = 100;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0))
		if (channels[i]->flags & IEEE80211_CHAN_PASSIVE_SCAN)
#else
		if (channels[i]->flags & IEEE80211_CHAN_NO_IR)
#endif
			wow_nlo->channel_list[i].tx_pkt = 0;
		else
			wow_nlo->channel_list[i].tx_pkt = 1;
		wow_nlo->channel_list[i].tx_data_pause = 1;
		i++;
	}
	wow_nlo->channel_num = i;
}

static void nlo_ssid_init(struct rtw_nlo_info *wow_nlo,
			  struct cfg80211_ssid *ssids, int n_ssids)
{
	u8 i = 0;

	while (i < MAX_NLO_NUM && i < n_ssids) {
		_rtw_memcpy((void *)wow_nlo->ssid[i], (void *)ssids[i].ssid,
			    ssids[i].ssid_len);
		wow_nlo->ssidlen[i] = ssids[i].ssid_len;
		i++;
	}

	wow_nlo->num_of_networks = i;
	wow_nlo->num_of_hidden_ap = i;
}

int rtw_nlo_enable(struct net_device *net, struct cfg80211_ssid *ssids,
		   int n_ssids, struct ieee80211_channel **channels,
		   u32 n_channels, u32 delay, u32 interval, u32 iterations,
		   u32 slow_interval)
{
	_adapter *padapter = (_adapter *)rtw_netdev_priv(net);
	struct wow_priv *wowpriv = adapter_to_wowlan(padapter);
	struct rtw_nlo_info *wow_nlo = &wowpriv->wow_nlo;

	_rtw_memset((void *)wow_nlo, 0, sizeof(struct rtw_nlo_info));

	wow_nlo->nlo_en = _TRUE;

	nlo_scan_ch_init(wow_nlo, channels, n_channels);
	nlo_ssid_init(wow_nlo, ssids, n_ssids);

	wow_nlo->delay = delay * 1000;
	wow_nlo->period = interval * 1000;
	wow_nlo->cycle = iterations;
	wow_nlo->slow_period = slow_interval * 1000;
	wow_nlo->construct_pbreq = NULL;;

	rtw_nlo_debug(net);

	return 0;
}

int rtw_nlo_disable(struct net_device *net)
{
	_adapter *padapter = (_adapter *)rtw_netdev_priv(net);
	struct wow_priv *wowpriv = adapter_to_wowlan(padapter);
	struct rtw_nlo_info *wow_nlo = &wowpriv->wow_nlo;

	wow_nlo->nlo_en = _FALSE;

	return 0;
}

void rtw_nlo_debug(struct net_device *net)
{
	_adapter *padapter = (_adapter *)rtw_netdev_priv(net);
	struct wow_priv *wowpriv = adapter_to_wowlan(padapter);
	struct rtw_nlo_info *wow_nlo = &wowpriv->wow_nlo;
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(padapter);
	int i;

	RTW_INFO("********NLO_INFO********\n");
	RTW_INFO("ssid_num: %d\n", wow_nlo->num_of_networks);
	for (i = 0; i < wow_nlo->num_of_networks; i++) {
		RTW_INFO("%d SSID (%s) length (%d)\n",
			 i, wow_nlo->ssid[i], wow_nlo->ssidlen[i]);
	}
	RTW_INFO("delay: %d\n", wow_nlo->delay);
	RTW_INFO("fast_scan_iterations: %d\n", wow_nlo->cycle);
	RTW_INFO("fast_scan_period: %d\n", wow_nlo->period);
	RTW_INFO("slow_scan_period: %d\n", wow_nlo->slow_period);

	RTW_INFO("********SCAN_INFO*******\n");
	RTW_INFO("ch_num: %d\n", wow_nlo->channel_num);
	for (i = 0 ;i < wow_nlo->channel_num; i++) {
		RTW_INFO("[%02d] avtive:%d, timeout:%d, ch:%02d\n",
			 i, wow_nlo->channel_list[i].tx_pkt,
			 wow_nlo->channel_list[i].period,
			 wow_nlo->channel_list[i].chan);
	}
	RTW_INFO("************************\n");
}
#endif /* CONFIG_PNO_SUPPORT */

