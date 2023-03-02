/******************************************************************************
 *
 * Copyright(c) 2019 - 2020 Realtek Corporation.
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
 * Author: vincent_fann@realtek.com
 *
 *****************************************************************************/
#define _PHL_DBG_CMD_C_
#include "phl_dbg_cmd.h"
#include "phl_ps_dbg_cmd.h"

/*
 * proc debug command of core
 */
enum PHL_DBG__CORE_CMD_ID {
	PHL_DBG_CORE_HELP,
	PHL_DBG_CORE_GIT_INFO
};

static const struct phl_dbg_cmd_info phl_dbg_core_cmd_i[] = {
	{"git_info", PHL_DBG_CORE_GIT_INFO}
};

void phl_dbg_git_info(struct phl_info_t *phl_info, char input[][MAX_ARGV],
		      u32 input_num, char *output, u32 out_len)
{
/* #REMOVE BEGIN */
#if CONFIG_GEN_GIT_INFO
#include "../phl_git_info.h"

	u32 used = 0;

	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"\ncore_ver   : %s\n", RTK_CORE_TAGINFO);
	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"phl_ver    : %s\n", RTK_PHL_TAGINFO);
	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"halmac_ver : %s\n", RTK_HALMAC_TAGINFO);
	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"halbb_ver  : %s\n", RTK_HALBB_TAGINFO);
	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"halrf_ver  : %s\n", RTK_HALRF_TAGINFO);
	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"btc_ver    : %s\n", RTK_BTC_TAGINFO);

	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"\ncore_sha1  : %s\n", RTK_CORE_SHA1);
	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"phl_sha1   : %s\n", RTK_PHL_SHA1);
	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"halmac_sha1: %s\n", RTK_HALMAC_SHA1);
	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"halbb_sha1 : %s\n", RTK_HALBB_SHA1);
	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"halrf_sha1 : %s\n", RTK_HALRF_SHA1);
	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"btc_sha1   : %s\n", RTK_BTC_SHA1);
#endif /* CONFIG_GEN_GIT_INFO */
/* #REMOVE END */
}

void phl_dbg_core_cmd_parser(void *phl, char input[][MAX_ARGV],
		        u32 input_num, char *output, u32 out_len)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	u8 id = 0;
	u32 i;
	u32 used = 0;
	u32 phl_ary_size = sizeof(phl_dbg_core_cmd_i) /
			   sizeof(struct phl_dbg_cmd_info);

	if (phl_ary_size == 0)
		return;

	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used, "\n");
	/* Parsing Cmd ID */
	if (input_num) {
		for (i = 0; i < phl_ary_size; i++) {
			if (_os_strcmp(phl_dbg_core_cmd_i[i].name, input[0]) == 0) {
				id = phl_dbg_core_cmd_i[i].id;
				PHL_INFO("[%s]===>\n", phl_dbg_core_cmd_i[i].name);
				break;
			}
		}
		if (i == phl_ary_size) {
			PHL_DBG_MON_INFO(out_len, used, output + used,
				out_len - used, "PHL CMD not found!\n");
			return;
		}
	}

	switch (id) {
	case PHL_DBG_CORE_HELP:
	{
		PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
				 "phl_dbg_core_cmd_parser : PHL_DBG_CORE_HELP \n");
		for (i = 0; i < phl_ary_size - 2; i++)
			PHL_DBG_MON_INFO(out_len, used, output + used,
					 out_len - used, "%-5d: %s\n",
			          (int)i, phl_dbg_core_cmd_i[i + 2].name);

	}
	break;
	case PHL_DBG_CORE_GIT_INFO:
	{
		phl_dbg_git_info(phl_info, input, input_num, output, out_len);
	}
	break;
	default:
		PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "[DBG] Do not support this command\n");
		break;
	}
}

s32
phl_dbg_core_proc_cmd(struct phl_info_t *phl_info,
		 char *input, char *output, u32 out_len)
{
	char *token;
	u32 argc = 0;
	char argv[MAX_ARGC][MAX_ARGV];

	do {
		token = _os_strsep(&input, ", ");
		if (token) {
			if (_os_strlen((u8 *)token) <= MAX_ARGV)
				_os_strcpy(argv[argc], token);

			argc++;
		} else {
			break;
		}
	} while (argc < MAX_ARGC);

	if (argc == 1)
		argv[0][_os_strlen((u8 *)argv[0])] = '\0';

	phl_dbg_core_cmd_parser(phl_info, argv, argc, output, out_len);

	return 0;
}

enum rtw_phl_status
rtw_phl_dbg_core_cmd(struct phl_info_t *phl_info,
		     struct rtw_proc_cmd *incmd,
		     char *output,
		     u32 out_len)
{
	if (incmd->in_type == RTW_ARG_TYPE_BUF) {
		phl_dbg_core_proc_cmd(phl_info, incmd->in.buf, output, out_len);
	} else if(incmd->in_type == RTW_ARG_TYPE_ARRAY){
		phl_dbg_core_cmd_parser(phl_info, incmd->in.vector,
				   incmd->in_cnt_len, output, out_len);
	}
	return RTW_PHL_STATUS_SUCCESS;
}

#ifdef CONFIG_PHL_TEST_SUITE
bool
_is_hex_digit(char ch_tmp)
{
	if( (ch_tmp >= '0' && ch_tmp <= '9') ||
		(ch_tmp >= 'a' && ch_tmp <= 'f') ||
		(ch_tmp >= 'A' && ch_tmp <= 'F') ) {
		return true;
	} else {
		return false;
	}
}


u32
_map_char_to_hex_digit(char ch_tmp)
{
	if(ch_tmp >= '0' && ch_tmp <= '9')
		return (ch_tmp - '0');
	else if(ch_tmp >= 'a' && ch_tmp <= 'f')
		return (10 + (ch_tmp - 'a'));
	else if(ch_tmp >= 'A' && ch_tmp <= 'F')
		return (10 + (ch_tmp - 'A'));
	else
		return 0;
}


bool
_get_hex_from_string(char *szstr, u32 *val)
{
	char *sz_scan = szstr;

	/* Check input parameter.*/
	if (szstr == NULL || val == NULL) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
			"_get_hex_from_string(): Invalid inpur argumetns! szStr: %p, pu4bVal: %p \n",
			 szstr, val);
		return false;
	}

	/* Initialize output. */
	*val = 0;

	/* Skip leading space. */
	while(*sz_scan != '\0' && (*sz_scan == ' ' || *sz_scan == '\t')) {
		sz_scan++;
	}

	/* Skip leading '0x' or '0X'. */
	if (*sz_scan == '0' && (*(sz_scan+1) == 'x' || *(sz_scan+1) == 'X')) {
		sz_scan += 2;
	}

	if (!_is_hex_digit(*sz_scan)) {
		return false;
	}

	do {
		(*val) <<= 4;
		*val += _map_char_to_hex_digit(*sz_scan);

		sz_scan++;
	} while (_is_hex_digit(*sz_scan));

	return true;
}

void
_phl_dbg_cmd_switch_chbw(struct phl_info_t *phl_info, char input[][MAX_ARGV],
			 u32 input_num, char *output, u32 out_len)
{
	u32 band = 0;
	u32 bw = 0;
	u32 offset = 0;
	u32 ch = 36;
	u32 used = 0;
	struct rtw_chan_def chdef = {0};

	do {
		if (input_num < 5){
			PHL_DBG_MON_INFO(out_len, used, output + used,
					 out_len - used,
					 "\n[DBG] echo phl set_ch [band(0,1)] [ch(hex)] [bw(0,1,2,3)] [offset(0,1,2,3)]\n");
			break;
		}

		if (!_get_hex_from_string(input[1], &band))
			break;

		if (band > 1)
			break;

		if (!_get_hex_from_string(input[2], &ch))
			break;


		if (!_get_hex_from_string(input[3], &bw))
			break;


		if (!_get_hex_from_string(input[4], &offset))
			break;

		PHL_DBG_MON_INFO(out_len, used, output + used,
				 out_len - used,
				 "\n[DBG] PHL_DBG_SET_CH_BW ==> band = %d\n",
				 (int)band);
		PHL_DBG_MON_INFO(out_len, used, output + used,
				 out_len - used,
				 "[DBG] PHL_DBG_SET_CH_BW ==> ch = %d\n",
				 (int)ch);
		PHL_DBG_MON_INFO(out_len, used, output + used,
				 out_len - used,
				 "[DBG] PHL_DBG_SET_CH_BW ==> bw = %d\n",
				 (int)bw);
		PHL_DBG_MON_INFO(out_len, used, output + used,
				 out_len - used,
				 "[DBG] PHL_DBG_SET_CH_BW ==> offset = %d\n",
				 (int)offset);

		chdef.chan = (u8)ch;
		chdef.band = rtw_phl_get_band_type(chdef.chan);
		chdef.bw = (enum channel_width)bw;
		chdef.offset = (enum chan_offset)offset;

		rtw_hal_set_ch_bw(phl_info->hal, (u8)band, &chdef, false);

	} while (0);
}

void _dump_wifi_role(struct phl_info_t *phl_info, char *output, u32 out_len)
{
	struct rtw_wifi_role_t *wrole = NULL;
	struct rtw_phl_stainfo_t *sta_info = NULL;
	u32 used = 0;
	u8 j = 0;
	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
				"==> PHL_DBG_DUMP_WROLE CH/BW information\n");
	for( j = 0; j < MAX_WIFI_ROLE_NUMBER; j++) {
		wrole = rtw_phl_get_wrole_by_ridx(phl_info->phl_com, j);
		if (NULL == wrole)
			continue;
		PHL_DBG_MON_INFO(out_len, used, output + used,
				 out_len - used, "wrole idx = 0x%x \n", j);
		PHL_DBG_MON_INFO(out_len, used, output + used,
				 out_len - used, "wrole->type = 0x%x \n",
				 wrole->type);
		/* debug_dump_mac_addr(wrole->mac_addr); */
		PHL_DBG_MON_INFO(out_len, used, output + used,
				 out_len - used, "wrole->chandef.bw = 0x%x \n",
				 wrole->chandef.bw);
		PHL_DBG_MON_INFO(out_len, used, output + used,
				 out_len - used, "wrole->chandef.band = 0x%x \n",
				 wrole->chandef.band);
		PHL_DBG_MON_INFO(out_len, used, output + used,
				 out_len - used, "wrole->chandef.center_ch = 0x%x \n",
				 wrole->chandef.center_ch);
		PHL_DBG_MON_INFO(out_len, used, output + used,
				 out_len - used, "wrole->chandef.chan = 0x%x \n",
				 wrole->chandef.chan);
		PHL_DBG_MON_INFO(out_len, used, output + used,
				 out_len - used, "wrole->chandef.center_freq1 = %u \n",
				 (int)wrole->chandef.center_freq1);
		PHL_DBG_MON_INFO(out_len, used, output + used,
				 out_len - used, "wrole->chandef.center_freq2 = %u \n",
				 (int)wrole->chandef.center_freq2);
		PHL_DBG_MON_INFO(out_len, used, output + used,
				 out_len - used, "wrole->chandef.offset = 0x%x \n",
				 wrole->chandef.offset);

		sta_info = rtw_phl_get_stainfo_self(phl_info, wrole);
		if (NULL == sta_info)
			continue;
		PHL_DBG_MON_INFO(out_len, used, output + used,
				 out_len - used, "sta_info->macid = 0x%x \n",
				 sta_info->macid);
		PHL_DBG_MON_INFO(out_len, used, output + used,
				 out_len - used, "sta_info->aid = 0x%x \n",
				 sta_info->aid);
		PHL_DBG_MON_INFO(out_len, used, output + used,
				 out_len - used, "mac_addr : %02x-%02x-%02x-%02x-%02x-%02x- \n",
				 sta_info->mac_addr[0], sta_info->mac_addr[1],
				 sta_info->mac_addr[2], sta_info->mac_addr[3],
				 sta_info->mac_addr[4], sta_info->mac_addr[5]);
		PHL_DBG_MON_INFO(out_len, used, output + used,
				 out_len - used, "sta_info->wmode = 0x%x \n",
				 sta_info->wmode);
		PHL_DBG_MON_INFO(out_len, used, output + used,
					out_len - used, "-----------------------------\n");
	}
}

void _dump_rx_rate(struct phl_info_t *phl_info, char input[][MAX_ARGV],
		      u32 input_num, char *output, u32 out_len)
{
	u32 ctrl = 0;
	u32 used = 0;
	struct rtw_stats *rx_stat = &phl_info->phl_com->phl_stats;
	if(input_num < 2)
		return;
	_get_hex_from_string(input[1], &ctrl);

	if (ctrl == 2) {
		/*TODO: Clear Counter*/
		return;
	}

	PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "\nOFDM 6M = %d ; OFDM 24M = %d ; OFDM 54M = %d\n",
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_OFDM6],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_OFDM24],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_OFDM54]);

	PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "VHT 1SS\
			\nMCS0 = %d ; MCS1 = %d ; MCS2 = %d ;\
			\nMCS3 = %d ; MCS4 = %d ; MCS5 = %d ;\
			\nMCS6 = %d ; MCS7 = %d ; MCS8 = %d ;\
			\nMCS9 = %d ;\n",
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS1_MCS0],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS1_MCS1],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS1_MCS2],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS1_MCS3],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS1_MCS4],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS1_MCS5],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS1_MCS6],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS1_MCS7],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS1_MCS8],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS1_MCS9]
			);
	PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "VHT 2SS\
			\nMCS0 = %d ; MCS1 = %d ; MCS2 = %d ;\
			\nMCS3 = %d ; MCS4 = %d ; MCS5 = %d ;\
			\nMCS6 = %d ; MCS7 = %d ; MCS8 = %d ;\
			\nMCS9 = %d ;\n",
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS2_MCS0],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS2_MCS1],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS2_MCS2],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS2_MCS3],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS2_MCS4],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS2_MCS5],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS2_MCS6],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS2_MCS7],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS2_MCS8],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_VHT_NSS2_MCS9]
			);
	PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "HE 1SS\
			\nMCS0 = %d ; MCS1 = %d ; MCS2 = %d ;\
			\nMCS3 = %d ; MCS4 = %d ; MCS5 = %d ;\
			\nMCS6 = %d ; MCS7 = %d ; MCS8 = %d ;\
			\nMCS9 = %d ; MCS10 = %d ; MCS11 = %d;\n",
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS1_MCS0],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS1_MCS1],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS1_MCS2],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS1_MCS3],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS1_MCS4],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS1_MCS5],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS1_MCS6],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS1_MCS7],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS1_MCS8],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS1_MCS9],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS1_MCS10],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS1_MCS11]);
	PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "HE 2SS\
			\nMCS0 = %d ; MCS1 = %d ; MCS2 = %d ;\
			\nMCS3 = %d ; MCS4 = %d ; MCS5 = %d ;\
			\nMCS6 = %d ; MCS7 = %d ; MCS8 = %d ;\
			\nMCS9 = %d ; MCS10 = %d ; MCS11 = %d ;\n",
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS2_MCS0],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS2_MCS1],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS2_MCS2],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS2_MCS3],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS2_MCS4],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS2_MCS5],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS2_MCS6],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS2_MCS7],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS2_MCS8],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS2_MCS9],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS2_MCS10],
			(int)rx_stat->rx_rate_nmr[RTW_DATA_RATE_HE_NSS2_MCS11]
			);

}

static const char *_get_mac_pwr_st_str(enum rtw_mac_pwr_st st)
{
	switch (st) {
	case RTW_MAC_PWR_OFF:
		return "off";
	case RTW_MAC_PWR_ON:
		return "on";
	case RTW_MAC_PWR_LPS:
		return "lps";
	default:
		return "n/a";
	}
}

static const char *_get_wow_opmode_str(enum rtw_wow_op_mode mode)
{
	switch (mode) {
	case RTW_WOW_OP_PWR_DOWN:
		return "power down";
	case RTW_WOW_OP_DISCONNECT_STBY:
		return "disconnected standby";
	case RTW_WOW_OP_CONNECT_STBY:
		return "connected standby";
	default:
		return "n/a";
	}
}

void _dump_wow_stats(struct phl_info_t *phl_info, char input[][MAX_ARGV],
	u32 input_num, char *output, u32 out_len)
{
#ifdef CONFIG_WOWLAN
	struct phl_wow_info *wow_info = phl_to_wow_info(phl_info);
	struct phl_wow_stat *wow_stat = &wow_info->wow_stat;
	u32 used = 0;

	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
					 "\n%-20s : op_mode = %s, func_en = %s, mac pwr = %s, wake_rsn = %s\
					 \n%-20s : keep_alive_en = %d, disc_det_en = %d, arp_en = %d, ndp_en = %d, gtk_en = %d, dot11w_en = %d\
					 \n%-20s : init err = 0x%x, deinit err = 0x%x\
					 \n%-20s : aoac_rpt_fail_cnt = 0x%x\n",
	"[wow information]",
	_get_wow_opmode_str(wow_stat->op_mode), (wow_stat->func_en == 1 ? "yes" : "no"),
	_get_mac_pwr_st_str(wow_stat->mac_pwr), rtw_phl_get_wow_rsn_str(phl_info, wow_stat->wake_rsn),
	"[wow function]",
	wow_stat->keep_alive_en, wow_stat->disc_det_en, wow_stat->arp_en, wow_stat->ndp_en, wow_stat->gtk_en, wow_stat->dot11w_en,
	"[wow error]",
	wow_stat->err.init, wow_stat->err.deinit,
	"[wow aoac]",
	wow_stat->aoac_rpt_fail_cnt);
#endif /* CONFIG_WOWLAN */
}

#ifdef CONFIG_MCC_SUPPORT
void _dump_mcc_info(struct phl_info_t *phl_info, char input[][MAX_ARGV],
	u32 input_num, char *output, u32 out_len)
{
	bool get_ok = false;
	struct rtw_phl_mcc_en_info en_info = {0};
	enum rtw_phl_tdmra_wmode mcc_mode;
	enum rtw_phl_mcc_state state;
	enum rtw_phl_mcc_coex_mode coex_mode;
	struct rtw_phl_mcc_bt_info bt_info = {0};
	struct rtw_phl_mcc_role *mrole = NULL;
	struct rtw_chan_def *chdef = NULL;
	struct rtw_phl_mcc_dur_info *dur_i = NULL;
	struct rtw_phl_mcc_pattern *m_pattern = NULL;
	struct rtw_phl_mcc_dbg_slot_info *dbg_slot_i = NULL;
	u32 used = 0;
	u8 i = 0;

	get_ok = rtw_phl_mcc_get_dbg_info(phl_info, 0, MCC_DBG_STATE, &state);
	if (false == get_ok) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "_dump_mcc_info(): get MCC_DBG_STATE fail\n");
		goto exit;
	}
	get_ok = rtw_phl_mcc_get_dbg_info(phl_info, 0, MCC_DBG_OP_MODE, &mcc_mode);
	if (false == get_ok) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "_dump_mcc_info(): get MCC_DBG_OP_MODE fail\n");
		goto exit;
	}
	get_ok = rtw_phl_mcc_get_dbg_info(phl_info, 0, MCC_DBG_COEX_MODE, &coex_mode);
	if (false == get_ok) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "_dump_mcc_info(): get MCC_DBG_COEX_MODE fail\n");
		goto exit;
	}
	PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "\n======== MCC Info ========\n");
	PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "MCC Info: State(%s, %d), Mode(%s, %d), Coex(%s, %d)\n",
			((state == MCC_NONE) ? "None" :
			((state == MCC_TRIGGER_FW_EN) ? "Trig_Fw_En" :
			((state == MCC_FW_EN_FAIL) ? "Fw_En_Fail" :
			((state == MCC_RUNING) ? "Runing" :
			((state == MCC_TRIGGER_FW_DIS) ? "Trig_Fw_Dis" :
			((state == MCC_FW_DIS_FAIL) ? "Fw_Dis_Fail" :
			((state == MCC_STOP) ? "Fw_Stop" : "Unknown"))))))),
			state,
			((mcc_mode == RTW_PHL_TDMRA_WMODE_NONE) ? "None" :
			((mcc_mode == RTW_PHL_TDMRA_AP_CLIENT_WMODE) ? "AP_Client" :
			((mcc_mode == RTW_PHL_TDMRA_2CLIENTS_WMODE) ? "2Clients" :
			((mcc_mode == RTW_PHL_TDMRA_AP_WMODE) ? "AP" : "Unknown")))),
			mcc_mode,
			((coex_mode == RTW_PHL_MCC_COEX_MODE_NONE) ? "None" :
			((coex_mode == RTW_PHL_MCC_COEX_MODE_BT_MASTER) ? "BT_M" :
			((coex_mode == RTW_PHL_MCC_COEX_MODE_WIFI_MASTER) ? "Wifi_M" :
			((coex_mode == RTW_PHL_MCC_COEX_MODE_BT_WIFI_BALANCE) ? "Balance" : "Unknown")))),
			coex_mode);
	get_ok = rtw_phl_mcc_get_dbg_info(phl_info, 0, MCC_DBG_BT_INFO, &bt_info);
	if (false == get_ok) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "_dump_mcc_info(): get MCC_DBG_BT_INFO fail\n");
		goto exit;
	}
	if (bt_info.bt_seg_num <= BT_SEG_NUM) {
		PHL_DBG_MON_INFO(out_len, used, output + used,
				out_len - used, "MCC Info: Bt_dur(%d), Add_Bt_Role(%d), Seg_Num(%d), bt_seg[0](%d), bt_seg[1](%d)\n",
				bt_info.bt_dur, bt_info.add_bt_role,
				bt_info.bt_seg_num, bt_info.bt_seg[0],
				bt_info.bt_seg[1]);
	}
	get_ok = rtw_phl_mcc_get_dbg_info(phl_info, 0, MCC_DBG_EN_INFO, &en_info);
	if (false == get_ok) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "_dump_mcc_info(): get MCC_DBG_EN_INFO fail\n");
		goto exit;
	}
	m_pattern = &en_info.m_pattern;
	PHL_DBG_MON_INFO(out_len, used, output + used,
				out_len - used, "MCC Info: mcc_intvl(%d), Start_tsf(0x%04x%04x %04x%04x), bcns_offset(%d), tob_r(%d), toa_r(%d), tob_a(%d), toa_a(%d)\n",
				en_info.mcc_intvl, (u16)(en_info.tsf_high >> 16),
				(u16)(en_info.tsf_high),
				(u16)(en_info.tsf_low >> 16),
				(u16)(en_info.tsf_low), m_pattern->bcns_offset,
				m_pattern->tob_r, m_pattern->toa_r,
				m_pattern->tob_a, m_pattern->toa_a);
	PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "\n======== MCC Role Info ========\n");
	for (i = 0; i < en_info.mrole_num; i++) {
		mrole = &en_info.mcc_role[i];
		chdef = mrole->chandef;
		dur_i = &mrole->policy.dur_info;
		if (NULL == chdef)
			break;
		PHL_DBG_MON_INFO(out_len, used, output + used,
				out_len - used, "MCC Info: Role(%d): macid(0x%x), Bcn_intvl(%d), band(%d), chan(%d), center_ch(%d), bw(%d), offset(%d)\n",
				i, mrole->macid, mrole->bcn_intvl, chdef->band,
				chdef->chan, chdef->center_ch, chdef->bw,
				chdef->offset);
		PHL_DBG_MON_INFO(out_len, used, output + used,
				out_len - used, "MCC Info: Role(%d): dur(0x%x), lim_en(%d), tag(%d), max_dur(%d), max_toa(%d), max_tob(%d)\n\n",
				i, dur_i->dur, dur_i->dur_limit.enable,
				dur_i->dur_limit.tag, dur_i->dur_limit.max_dur,
				dur_i->dur_limit.max_toa,
				dur_i->dur_limit.max_tob);
	}
	PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "======== Hal MCC Info ========\n");
	PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "MCC Info: btc_in_group(%d)\n",
			en_info.dbg_hal_i.btc_in_group);
	for (i = 0; i < en_info.dbg_hal_i.slot_num; i++) {
		dbg_slot_i = &en_info.dbg_hal_i.dbg_slot_i[i];
		PHL_DBG_MON_INFO(out_len, used, output + used,
				out_len - used, "MCC Info: FW Slot(%d), BT_Role(%d), Dur(%d), Ch(%d), Macid(0x%x)\n",
				i, dbg_slot_i->bt_role, dbg_slot_i->dur,
				dbg_slot_i->ch, dbg_slot_i->macid);
	}
exit:
	return;
}
#endif
void phl_dbg_cmd_snd(struct phl_info_t *phl_info, char input[][MAX_ARGV],
		      u32 input_num, char *output, u32 out_len)
{
	u32 role_idx;
	u32 ctrl;
	u32 used = 0;

	if (input_num < 2)
		return;

	_get_hex_from_string(input[1], &ctrl);
	_get_hex_from_string(input[2], &role_idx);

	PHL_DBG_MON_INFO(out_len, used, output + used,
			 out_len - used, "ctrl 0x%x with role_idx 0x%x!!\n",
			 (int)ctrl, (int)role_idx);

	if (1 == ctrl) {
		rtw_phl_sound_start(phl_info, (u8)role_idx, 0, 200,
				    PHL_SND_TEST_F_ONE_TIME |
				    PHL_SND_TEST_F_PASS_STS_CHK);
		PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "SOUND START(once) : wrole %d !!\n", (int)role_idx);
	} else if (2 == ctrl) {
		rtw_phl_sound_abort(phl_info);
		PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "SOUND ABORT!!\n");
	} else if (3 == ctrl) {
		rtw_phl_sound_start(phl_info, (u8)role_idx, 0, 200,
				    PHL_SND_TEST_F_PASS_STS_CHK);
		PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "SOUND START(loop) : wrole %d !!\n", (int)role_idx);
	} else if(9 == ctrl) {
		PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "DUMP BF Entry in debugview\n");
			rtw_hal_bf_dbg_dump_entry_all(phl_info->hal);
	} else {
		PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "SOUND TEST CMD not found!\n");
	}
}

void _convert_tx_rate(enum hal_rate_mode mode, u8 mcs_ss_idx, char *str, u32 str_len)
{
	switch (mode) {
	case HAL_LEGACY_MODE:
		switch (mcs_ss_idx) {
		case RTW_DATA_RATE_CCK1:	_os_snprintf(str, str_len,"CCK 1"); break;
		case RTW_DATA_RATE_CCK2:	_os_snprintf(str, str_len,"CCK 2"); break;
		case RTW_DATA_RATE_CCK5_5:	_os_snprintf(str, str_len,"CCK 5_5"); break;
		case RTW_DATA_RATE_CCK11:	_os_snprintf(str, str_len,"CCK 11"); break;
		case RTW_DATA_RATE_OFDM6:	_os_snprintf(str, str_len,"OFDM 6"); break;
		case RTW_DATA_RATE_OFDM9:	_os_snprintf(str, str_len,"OFDM 9"); break;
		case RTW_DATA_RATE_OFDM12:	_os_snprintf(str, str_len,"OFDM 12"); break;
		case RTW_DATA_RATE_OFDM18:	_os_snprintf(str, str_len,"OFDM 18"); break;
		case RTW_DATA_RATE_OFDM24:	_os_snprintf(str, str_len,"OFDM 24"); break;
		case RTW_DATA_RATE_OFDM36:	_os_snprintf(str, str_len,"OFDM 36"); break;
		case RTW_DATA_RATE_OFDM48:	_os_snprintf(str, str_len,"OFDM 48"); break;
		case RTW_DATA_RATE_OFDM54:	_os_snprintf(str, str_len,"OFDM 54"); break;
		default:
			_os_snprintf(str, str_len,"Unkown rate(0x%01x)", mcs_ss_idx);
			break;
		}
		break;
	case HAL_HT_MODE:
		_os_snprintf(str, str_len,"MCS%d", mcs_ss_idx);
		break;
	case HAL_VHT_MODE:
	case HAL_HE_MODE:
		_os_snprintf(str, str_len,"%dSS MCS%d",
		             ((mcs_ss_idx & 0x70)>> 4) + 1,
		             (mcs_ss_idx & 0x0F));
		break;
	default:
		_os_snprintf(str, str_len,"Unknown mode(0x%01x)", mode);
		break;
	}
}

void _convert_rx_rate(u32 rx_rate, char *str, u32 str_len)
{
	switch(rx_rate) {
	case RTW_DATA_RATE_CCK1:	_os_snprintf(str, str_len,"CCK 1"); break;
	case RTW_DATA_RATE_CCK2:	_os_snprintf(str, str_len,"CCK 2"); break;
	case RTW_DATA_RATE_CCK5_5:	_os_snprintf(str, str_len,"CCK 5_5"); break;
	case RTW_DATA_RATE_CCK11:	_os_snprintf(str, str_len,"CCK 11"); break;
	case RTW_DATA_RATE_OFDM6:	_os_snprintf(str, str_len,"OFDM 6"); break;
	case RTW_DATA_RATE_OFDM9:	_os_snprintf(str, str_len,"OFDM 9"); break;
	case RTW_DATA_RATE_OFDM12:	_os_snprintf(str, str_len,"OFDM 12"); break;
	case RTW_DATA_RATE_OFDM18:	_os_snprintf(str, str_len,"OFDM 18"); break;
	case RTW_DATA_RATE_OFDM24:	_os_snprintf(str, str_len,"OFDM 24"); break;
	case RTW_DATA_RATE_OFDM36:	_os_snprintf(str, str_len,"OFDM 36"); break;
	case RTW_DATA_RATE_OFDM48:	_os_snprintf(str, str_len,"OFDM 48"); break;
	case RTW_DATA_RATE_OFDM54:	_os_snprintf(str, str_len,"OFDM 54"); break;

	case RTW_DATA_RATE_MCS0:	_os_snprintf(str, str_len,"MCS 0"); break;
	case RTW_DATA_RATE_MCS1:	_os_snprintf(str, str_len,"MCS 1"); break;
	case RTW_DATA_RATE_MCS2:	_os_snprintf(str, str_len,"MCS 2"); break;
	case RTW_DATA_RATE_MCS3:	_os_snprintf(str, str_len,"MCS 3"); break;
	case RTW_DATA_RATE_MCS4:	_os_snprintf(str, str_len,"MCS 4"); break;
	case RTW_DATA_RATE_MCS5:	_os_snprintf(str, str_len,"MCS 5"); break;
	case RTW_DATA_RATE_MCS6:	_os_snprintf(str, str_len,"MCS 6"); break;
	case RTW_DATA_RATE_MCS7:	_os_snprintf(str, str_len,"MCS 7"); break;
	case RTW_DATA_RATE_MCS8:	_os_snprintf(str, str_len,"MCS 8"); break;
	case RTW_DATA_RATE_MCS9:	_os_snprintf(str, str_len,"MCS 9"); break;
	case RTW_DATA_RATE_MCS10:	_os_snprintf(str, str_len,"MCS 10"); break;
	case RTW_DATA_RATE_MCS11:	_os_snprintf(str, str_len,"MCS 11"); break;
	case RTW_DATA_RATE_MCS12:	_os_snprintf(str, str_len,"MCS 12"); break;
	case RTW_DATA_RATE_MCS13:	_os_snprintf(str, str_len,"MCS 13"); break;
	case RTW_DATA_RATE_MCS14:	_os_snprintf(str, str_len,"MCS 14"); break;
	case RTW_DATA_RATE_MCS15:	_os_snprintf(str, str_len,"MCS 15"); break;
	case RTW_DATA_RATE_MCS16:	_os_snprintf(str, str_len,"MCS 16"); break;
	case RTW_DATA_RATE_MCS17:	_os_snprintf(str, str_len,"MCS 17"); break;
	case RTW_DATA_RATE_MCS18:	_os_snprintf(str, str_len,"MCS 18"); break;
	case RTW_DATA_RATE_MCS19:	_os_snprintf(str, str_len,"MCS 19"); break;
	case RTW_DATA_RATE_MCS20:	_os_snprintf(str, str_len,"MCS 20"); break;
	case RTW_DATA_RATE_MCS21:	_os_snprintf(str, str_len,"MCS 21"); break;
	case RTW_DATA_RATE_MCS22:	_os_snprintf(str, str_len,"MCS 22"); break;
	case RTW_DATA_RATE_MCS23:	_os_snprintf(str, str_len,"MCS 23"); break;
	case RTW_DATA_RATE_MCS24:	_os_snprintf(str, str_len,"MCS 24"); break;
	case RTW_DATA_RATE_MCS25:	_os_snprintf(str, str_len,"MCS 25"); break;
	case RTW_DATA_RATE_MCS26:	_os_snprintf(str, str_len,"MCS 26"); break;
	case RTW_DATA_RATE_MCS27:	_os_snprintf(str, str_len,"MCS 27"); break;
	case RTW_DATA_RATE_MCS28:	_os_snprintf(str, str_len,"MCS 28"); break;
	case RTW_DATA_RATE_MCS29:	_os_snprintf(str, str_len,"MCS 29"); break;
	case RTW_DATA_RATE_MCS30:	_os_snprintf(str, str_len,"MCS 30"); break;
	case RTW_DATA_RATE_MCS31:	_os_snprintf(str, str_len,"MCS 31"); break;

	case RTW_DATA_RATE_VHT_NSS1_MCS0:	_os_snprintf(str, str_len,"NSS1_MCS0"); break;
	case RTW_DATA_RATE_VHT_NSS1_MCS1:	_os_snprintf(str, str_len,"NSS1_MCS1"); break;
	case RTW_DATA_RATE_VHT_NSS1_MCS2:	_os_snprintf(str, str_len,"NSS1_MCS2"); break;
	case RTW_DATA_RATE_VHT_NSS1_MCS3:	_os_snprintf(str, str_len,"NSS1_MCS3"); break;
	case RTW_DATA_RATE_VHT_NSS1_MCS4:	_os_snprintf(str, str_len,"NSS1_MCS4"); break;
	case RTW_DATA_RATE_VHT_NSS1_MCS5:	_os_snprintf(str, str_len,"NSS1_MCS5"); break;
	case RTW_DATA_RATE_VHT_NSS1_MCS6:	_os_snprintf(str, str_len,"NSS1_MCS6"); break;
	case RTW_DATA_RATE_VHT_NSS1_MCS7:	_os_snprintf(str, str_len,"NSS1_MCS7"); break;
	case RTW_DATA_RATE_VHT_NSS1_MCS8:	_os_snprintf(str, str_len,"NSS1_MCS8"); break;
	case RTW_DATA_RATE_VHT_NSS1_MCS9:	_os_snprintf(str, str_len,"NSS1_MCS9"); break;
	case RTW_DATA_RATE_VHT_NSS2_MCS0:	_os_snprintf(str, str_len,"NSS2_MCS0"); break;
	case RTW_DATA_RATE_VHT_NSS2_MCS1:	_os_snprintf(str, str_len,"NSS2_MCS1"); break;
	case RTW_DATA_RATE_VHT_NSS2_MCS2:	_os_snprintf(str, str_len,"NSS2_MCS2"); break;
	case RTW_DATA_RATE_VHT_NSS2_MCS3:	_os_snprintf(str, str_len,"NSS2_MCS3"); break;
	case RTW_DATA_RATE_VHT_NSS2_MCS4:	_os_snprintf(str, str_len,"NSS2_MCS4"); break;
	case RTW_DATA_RATE_VHT_NSS2_MCS5:	_os_snprintf(str, str_len,"NSS2_MCS5"); break;
	case RTW_DATA_RATE_VHT_NSS2_MCS6:	_os_snprintf(str, str_len,"NSS2_MCS6"); break;
	case RTW_DATA_RATE_VHT_NSS2_MCS7:	_os_snprintf(str, str_len,"NSS2_MCS7"); break;
	case RTW_DATA_RATE_VHT_NSS2_MCS8:	_os_snprintf(str, str_len,"NSS2_MCS8"); break;
	case RTW_DATA_RATE_VHT_NSS2_MCS9:	_os_snprintf(str, str_len,"NSS2_MCS9"); break;

	case RTW_DATA_RATE_VHT_NSS3_MCS0:	_os_snprintf(str, str_len,"NSS3_MCS0"); break;
	case RTW_DATA_RATE_VHT_NSS3_MCS1:	_os_snprintf(str, str_len,"NSS3_MCS1"); break;
	case RTW_DATA_RATE_VHT_NSS3_MCS2:	_os_snprintf(str, str_len,"NSS3_MCS2"); break;
	case RTW_DATA_RATE_VHT_NSS3_MCS3:	_os_snprintf(str, str_len,"NSS3_MCS3"); break;
	case RTW_DATA_RATE_VHT_NSS3_MCS4:	_os_snprintf(str, str_len,"NSS3_MCS4"); break;
	case RTW_DATA_RATE_VHT_NSS3_MCS5:	_os_snprintf(str, str_len,"NSS3_MCS5"); break;
	case RTW_DATA_RATE_VHT_NSS3_MCS6:	_os_snprintf(str, str_len,"NSS3_MCS6"); break;
	case RTW_DATA_RATE_VHT_NSS3_MCS7:	_os_snprintf(str, str_len,"NSS3_MCS7"); break;
	case RTW_DATA_RATE_VHT_NSS3_MCS8:	_os_snprintf(str, str_len,"NSS3_MCS8"); break;
	case RTW_DATA_RATE_VHT_NSS3_MCS9:	_os_snprintf(str, str_len,"NSS3_MCS9"); break;
	case RTW_DATA_RATE_VHT_NSS4_MCS0:	_os_snprintf(str, str_len,"NSS4_MCS0"); break;
	case RTW_DATA_RATE_VHT_NSS4_MCS1:	_os_snprintf(str, str_len,"NSS4_MCS1"); break;
	case RTW_DATA_RATE_VHT_NSS4_MCS2:	_os_snprintf(str, str_len,"NSS4_MCS2"); break;
	case RTW_DATA_RATE_VHT_NSS4_MCS3:	_os_snprintf(str, str_len,"NSS4_MCS3"); break;
	case RTW_DATA_RATE_VHT_NSS4_MCS4:	_os_snprintf(str, str_len,"NSS4_MCS4"); break;
	case RTW_DATA_RATE_VHT_NSS4_MCS5:	_os_snprintf(str, str_len,"NSS4_MCS5"); break;
	case RTW_DATA_RATE_VHT_NSS4_MCS6:	_os_snprintf(str, str_len,"NSS4_MCS6"); break;
	case RTW_DATA_RATE_VHT_NSS4_MCS7:	_os_snprintf(str, str_len,"NSS4_MCS7"); break;
	case RTW_DATA_RATE_VHT_NSS4_MCS8:	_os_snprintf(str, str_len,"NSS4_MCS8"); break;
	case RTW_DATA_RATE_VHT_NSS4_MCS9:	_os_snprintf(str, str_len,"NSS4_MCS9"); break;

	case RTW_DATA_RATE_HE_NSS1_MCS0:	_os_snprintf(str, str_len,"HE_NSS1_MCS0"); break;
	case RTW_DATA_RATE_HE_NSS1_MCS1:	_os_snprintf(str, str_len,"HE_NSS1_MCS1"); break;
	case RTW_DATA_RATE_HE_NSS1_MCS2:	_os_snprintf(str, str_len,"HE_NSS1_MCS2"); break;
	case RTW_DATA_RATE_HE_NSS1_MCS3:	_os_snprintf(str, str_len,"HE_NSS1_MCS3"); break;
	case RTW_DATA_RATE_HE_NSS1_MCS4:	_os_snprintf(str, str_len,"HE_NSS1_MCS4"); break;
	case RTW_DATA_RATE_HE_NSS1_MCS5:	_os_snprintf(str, str_len,"HE_NSS1_MCS5"); break;
	case RTW_DATA_RATE_HE_NSS1_MCS6:	_os_snprintf(str, str_len,"HE_NSS1_MCS6"); break;
	case RTW_DATA_RATE_HE_NSS1_MCS7:	_os_snprintf(str, str_len,"HE_NSS1_MCS7"); break;
	case RTW_DATA_RATE_HE_NSS1_MCS8:	_os_snprintf(str, str_len,"HE_NSS1_MCS8"); break;
	case RTW_DATA_RATE_HE_NSS1_MCS9:	_os_snprintf(str, str_len,"HE_NSS1_MCS9"); break;
	case RTW_DATA_RATE_HE_NSS1_MCS10:	_os_snprintf(str, str_len,"HE_NSS1_MCS10"); break;
	case RTW_DATA_RATE_HE_NSS1_MCS11:	_os_snprintf(str, str_len,"HE_NSS1_MCS11"); break;

	case RTW_DATA_RATE_HE_NSS2_MCS0:	_os_snprintf(str, str_len,"HE_NSS2_MCS0"); break;
	case RTW_DATA_RATE_HE_NSS2_MCS1:	_os_snprintf(str, str_len,"HE_NSS2_MCS1"); break;
	case RTW_DATA_RATE_HE_NSS2_MCS2:	_os_snprintf(str, str_len,"HE_NSS2_MCS2"); break;
	case RTW_DATA_RATE_HE_NSS2_MCS3:	_os_snprintf(str, str_len,"HE_NSS2_MCS3"); break;
	case RTW_DATA_RATE_HE_NSS2_MCS4:	_os_snprintf(str, str_len,"HE_NSS2_MCS4"); break;
	case RTW_DATA_RATE_HE_NSS2_MCS5:	_os_snprintf(str, str_len,"HE_NSS2_MCS5"); break;
	case RTW_DATA_RATE_HE_NSS2_MCS6:	_os_snprintf(str, str_len,"HE_NSS2_MCS6"); break;
	case RTW_DATA_RATE_HE_NSS2_MCS7:	_os_snprintf(str, str_len,"HE_NSS2_MCS7"); break;
	case RTW_DATA_RATE_HE_NSS2_MCS8:	_os_snprintf(str, str_len,"HE_NSS2_MCS8"); break;
	case RTW_DATA_RATE_HE_NSS2_MCS9:	_os_snprintf(str, str_len,"HE_NSS2_MCS9"); break;
	case RTW_DATA_RATE_HE_NSS2_MCS10:	_os_snprintf(str, str_len,"HE_NSS2_MCS10"); break;
	case RTW_DATA_RATE_HE_NSS2_MCS11:	_os_snprintf(str, str_len,"HE_NSS2_MCS11"); break;

	case RTW_DATA_RATE_HE_NSS3_MCS0:	_os_snprintf(str, str_len,"HE_NSS3_MCS0"); break;
	case RTW_DATA_RATE_HE_NSS3_MCS1:	_os_snprintf(str, str_len,"HE_NSS3_MCS1"); break;
	case RTW_DATA_RATE_HE_NSS3_MCS2:	_os_snprintf(str, str_len,"HE_NSS3_MCS2"); break;
	case RTW_DATA_RATE_HE_NSS3_MCS3:	_os_snprintf(str, str_len,"HE_NSS3_MCS3"); break;
	case RTW_DATA_RATE_HE_NSS3_MCS4:	_os_snprintf(str, str_len,"HE_NSS3_MCS4"); break;
	case RTW_DATA_RATE_HE_NSS3_MCS5:	_os_snprintf(str, str_len,"HE_NSS3_MCS5"); break;
	case RTW_DATA_RATE_HE_NSS3_MCS6:	_os_snprintf(str, str_len,"HE_NSS3_MCS6"); break;
	case RTW_DATA_RATE_HE_NSS3_MCS7:	_os_snprintf(str, str_len,"HE_NSS3_MCS7"); break;
	case RTW_DATA_RATE_HE_NSS3_MCS8:	_os_snprintf(str, str_len,"HE_NSS3_MCS8"); break;
	case RTW_DATA_RATE_HE_NSS3_MCS9:	_os_snprintf(str, str_len,"HE_NSS3_MCS9"); break;
	case RTW_DATA_RATE_HE_NSS3_MCS10:	_os_snprintf(str, str_len,"HE_NSS3_MCS10"); break;
	case RTW_DATA_RATE_HE_NSS3_MCS11:	_os_snprintf(str, str_len,"HE_NSS3_MCS11"); break;

	case RTW_DATA_RATE_HE_NSS4_MCS0:	_os_snprintf(str, str_len,"HE_NSS4_MCS0"); break;
	case RTW_DATA_RATE_HE_NSS4_MCS1:	_os_snprintf(str, str_len,"HE_NSS4_MCS1"); break;
	case RTW_DATA_RATE_HE_NSS4_MCS2:	_os_snprintf(str, str_len,"HE_NSS4_MCS2"); break;
	case RTW_DATA_RATE_HE_NSS4_MCS3:	_os_snprintf(str, str_len,"HE_NSS4_MCS3"); break;
	case RTW_DATA_RATE_HE_NSS4_MCS4:	_os_snprintf(str, str_len,"HE_NSS4_MCS4"); break;
	case RTW_DATA_RATE_HE_NSS4_MCS5:	_os_snprintf(str, str_len,"HE_NSS4_MCS5"); break;
	case RTW_DATA_RATE_HE_NSS4_MCS6:	_os_snprintf(str, str_len,"HE_NSS4_MCS6"); break;
	case RTW_DATA_RATE_HE_NSS4_MCS7:	_os_snprintf(str, str_len,"HE_NSS4_MCS7"); break;
	case RTW_DATA_RATE_HE_NSS4_MCS8:	_os_snprintf(str, str_len,"HE_NSS4_MCS8"); break;
	case RTW_DATA_RATE_HE_NSS4_MCS9:	_os_snprintf(str, str_len,"HE_NSS4_MCS9"); break;
	case RTW_DATA_RATE_HE_NSS4_MCS10:	_os_snprintf(str, str_len,"HE_NSS4_MCS10"); break;
	case RTW_DATA_RATE_HE_NSS4_MCS11:	_os_snprintf(str, str_len,"HE_NSS4_MCS11"); break;

	case RTW_DATA_RATE_MAX:			_os_snprintf(str, str_len,"RATE_MAX"); break;
	default:
		_os_snprintf(str, str_len,"Unknown rate(0x%01x)", (unsigned int)rx_rate);
		break;
	}
}

void phl_dbg_cmd_asoc_sta(struct phl_info_t *phl_info, char input[][MAX_ARGV],
		      u32 input_num, char *output, u32 out_len)
{
	u32 ctrl = 0, wrole_idx = 0;
	u32 used = 0;
	struct rtw_wifi_role_t *wrole = NULL;
	struct rtw_phl_stainfo_t *n, *psta;
	void *drv = phl_to_drvpriv(phl_info);
	char tx_rate_str[32] = {0};
	char rx_rate_str[32] = {0};

	PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "\nDUMP Associate STA infomations\n");
	if(input_num < 2) {
		PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "phl asoc_sta [wrole index] [1=dump list basic information]\n");
		return;
	}
	_get_hex_from_string(input[1], &ctrl);
	_get_hex_from_string(input[2], &wrole_idx);
	if (wrole_idx >= MAX_WIFI_ROLE_NUMBER) {
		PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "Wrole Index shall < 5\n");
		return;
	}
	wrole = rtw_phl_get_wrole_by_ridx(phl_info->phl_com, (u8)wrole_idx);
	if (1 == ctrl) {
		_os_spinlock(drv, &wrole->assoc_sta_queue.lock, _bh, NULL);
		phl_list_for_loop_safe(psta, n, struct rtw_phl_stainfo_t,
			       &wrole->assoc_sta_queue.queue, list) {
			PHL_DBG_MON_INFO(out_len, used, output + used,
				out_len - used, "\nmac_id 0x%x ; aid 0x%x ; mac addr %02x-%02x-%02x-%02x-%02x-%02x ; AX_Support %d\n",
				(int)psta->macid, (int)psta->aid,
				psta->mac_addr[0], psta->mac_addr[1],
				psta->mac_addr[2], psta->mac_addr[3],
				psta->mac_addr[4], psta->mac_addr[5],
				(psta->wmode&WLAN_MD_11AX) ? 1 : 0);

			PHL_DBG_MON_INFO(out_len, used, output + used,
				out_len - used, "WROLE-IDX:%d wlan_mode:0x%02x, chan:%d, bw%d\n",
				psta->wrole->id, psta->wmode, psta->chandef.chan, psta->chandef.bw);

			PHL_DBG_MON_INFO(out_len, used, output + used,
				out_len - used, "[Stats] Tput -[Tx:%d KBits Rx :%d KBits]\n",
				(int)psta->stats.tx_tp_kbits, (int)psta->stats.rx_tp_kbits);
			PHL_DBG_MON_INFO(out_len, used, output + used,
				out_len - used, "[Stats] RSSI:-%d(dBm)\n",
				PHL_MAX_RSSI - psta->hal_sta->rssi_stat.ma_rssi);

			_convert_tx_rate( psta->hal_sta->ra_info.rpt_rt_i.mode,
					psta->hal_sta->ra_info.rpt_rt_i.mcs_ss_idx,
			 		tx_rate_str, 32);
			PHL_DBG_MON_INFO(out_len, used, output + used,
				out_len - used, "[Stats] Tx Rate :%s\n",
				tx_rate_str);

			_convert_rx_rate(psta->stats.rx_rate, rx_rate_str, 32);
			PHL_DBG_MON_INFO(out_len, used, output + used,
				out_len - used, "[Stats] Rx Rate :%s\n",
				rx_rate_str);

			PHL_DBG_MON_INFO(out_len, used, output + used,
				out_len - used, "[Stats] rx_ok_cnt:%d rx_err_cnt:%d\n",
				(int)psta->hal_sta->trx_stat.rx_ok_cnt,
				(int)psta->hal_sta->trx_stat.rx_err_cnt);


		}
		_os_spinunlock(drv, &wrole->assoc_sta_queue.lock, _bh, NULL);
	}
}

#ifdef CONFIG_PCI_HCI
void _dbg_tx_stats_pcie(struct phl_info_t *phl_info, char input[][MAX_ARGV],
			u32 input_num, char *output, u32 out_len, u32 *used,
			u32 ctrl, u32 ch)

{
	struct hci_info_t *hci_info = (struct hci_info_t *)phl_info->hci;
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);
	struct rtw_wp_rpt_stats *rpt_stats = NULL;

	rpt_stats = (struct rtw_wp_rpt_stats *)hal_com->trx_stat.wp_rpt_stats;

	if (ch >= hci_info->total_txch_num) {
		PHL_DBG_MON_INFO(out_len, *used, output + *used, out_len - *used,
				 "\n Invalid channel number\n");
		return;
	}
	if (0xFF == ctrl) {
		rpt_stats[ch].tx_ok_cnt = 0;
		rpt_stats[ch].rty_fail_cnt = 0;
		rpt_stats[ch].lifetime_drop_cnt = 0;
		rpt_stats[ch].macid_drop_cnt = 0;
		rpt_stats[ch].sw_drop_cnt = 0;
		rpt_stats[ch].recycle_fail_cnt = 0;
		rpt_stats[ch].delay_tx_ok_cnt = 0;
		rpt_stats[ch].delay_rty_fail_cnt = 0;
		rpt_stats[ch].delay_lifetime_drop_cnt = 0;
		rpt_stats[ch].delay_macid_drop_cnt = 0;
		return;
	}

	PHL_DBG_MON_INFO(out_len, *used, output + *used, out_len - *used,
		"\n== wp report statistics == \n");
	PHL_DBG_MON_INFO(out_len, *used, output + *used, out_len - *used,
		"ch			: %u\n", (int)ch);
	PHL_DBG_MON_INFO(out_len, *used, output + *used, out_len - *used,
		"busy count		: %u\n", (int)rpt_stats[ch].busy_cnt);
	PHL_DBG_MON_INFO(out_len, *used, output + *used, out_len - *used,
		"ok count		: %u\n", (int)rpt_stats[ch].tx_ok_cnt);
	PHL_DBG_MON_INFO(out_len, *used, output + *used, out_len - *used,
		"retry fail count	: %u\n",
			 (int)rpt_stats[ch].rty_fail_cnt);
	PHL_DBG_MON_INFO(out_len, *used, output + *used, out_len - *used,
		"lifetime drop count	: %u\n",
			 (int)rpt_stats[ch].lifetime_drop_cnt);
	PHL_DBG_MON_INFO(out_len, *used, output + *used, out_len - *used,
		"macid drop count	: %u\n",
			 (int)rpt_stats[ch].macid_drop_cnt);
	PHL_DBG_MON_INFO(out_len, *used, output + *used, out_len - *used,
		"sw drop count		: %u\n",
			 (int)rpt_stats[ch].sw_drop_cnt);
	PHL_DBG_MON_INFO(out_len, *used, output + *used, out_len - *used,
		"recycle fail count	: %u\n",
			 (int)rpt_stats[ch].recycle_fail_cnt);
	PHL_DBG_MON_INFO(out_len, *used, output + *used, out_len - *used,
		"delay ok count			: %u\n",
			 (int)rpt_stats[ch].delay_tx_ok_cnt);
	PHL_DBG_MON_INFO(out_len, *used, output + *used, out_len - *used,
		"delay retry fail count		: %u\n",
			 (int)rpt_stats[ch].delay_rty_fail_cnt);
	PHL_DBG_MON_INFO(out_len, *used, output + *used, out_len - *used,
		"delay lifetime drop count	: %u\n",
			 (int)rpt_stats[ch].delay_lifetime_drop_cnt);
	PHL_DBG_MON_INFO(out_len, *used, output + *used, out_len - *used,
		"delay macid drop count		: %u\n",
			 (int)rpt_stats[ch].delay_macid_drop_cnt);
	PHL_DBG_MON_INFO(out_len, *used, output + *used, out_len - *used,
		"ltr tx delay count		: %u\n",
			 (int)hal_com->trx_stat.ltr_tx_dly_count);
	PHL_DBG_MON_INFO(out_len, *used, output + *used, out_len - *used,
		"ltr last tx delay time	: %u\n",
			 (int)hal_com->trx_stat.ltr_last_tx_dly_time);
}
#endif

void phl_dbg_ltr_stats(struct phl_info_t *phl_info, char input[][MAX_ARGV],
		       u32 input_num, char *output, u32 out_len)
{
	#ifdef CONFIG_PCI_HCI
	u32 used = 0;
	#ifdef RTW_WKARD_DYNAMIC_LTR
		if (input_num == 3) {
			if (0 == _os_strcmp(input[1], "set")) {
					if (0 == _os_strcmp(input[2], "disable")) {
						phl_ltr_hw_ctrl(phl_info->phl_com, false);
						phl_ltr_sw_ctrl(phl_info->phl_com, phl_info->hal, false);
					} else if (0 == _os_strcmp(input[2], "enable")) {
						phl_ltr_hw_ctrl(phl_info->phl_com, true);
						phl_ltr_sw_ctrl(phl_info->phl_com, phl_info->hal, true);
					} else {
						return;
					}
			}
			return;
		}

		PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"\nltr sw ctrl   	   : %u\n",
				rtw_hal_ltr_is_sw_ctrl(phl_info->phl_com, phl_info->hal) ? 1 : 0);
		PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"ltr hw ctrl   	   : %u\n",
				rtw_hal_ltr_is_hw_ctrl(phl_info->phl_com, phl_info->hal) ? 1 : 0);
		PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"ltr current state   : %u\n",
				phl_ltr_get_cur_state(phl_info->phl_com));
		PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"ltr last trigger time : %lu\n",
				phl_ltr_get_last_trigger_time(phl_info->phl_com));
		PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"ltr active trigger cnt: %lu\n",
				phl_ltr_get_tri_cnt(phl_info->phl_com, RTW_PCIE_LTR_SW_ACT));
		PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"ltr idle trigger cnt: %lu\n",
				phl_ltr_get_tri_cnt(phl_info->phl_com, RTW_PCIE_LTR_SW_IDLE));
	#else
		PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"Not Support Dynamical LTR\n");
	#endif
	#endif
}

void phl_dbg_trx_stats(struct phl_info_t *phl_info, char input[][MAX_ARGV],
		       u32 input_num, char *output, u32 out_len)
{
 	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct rtw_stats *phl_stats = &phl_com->phl_stats;
	u32 used = 0;
	u32 ctrl = 0;
	u32 ch = 0;

	if (input_num < 2) {
		PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"Invalid Input\n");
		return;
	}
	_get_hex_from_string(input[1], &ch);

	if (input_num == 3) {
		_get_hex_from_string(input[2], &ctrl);
	}

	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"\nunicast tx bytes	: %llu\n", phl_stats->tx_byte_uni);
	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"total tx bytes		: %llu\n", phl_stats->tx_byte_total);
	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		 "tx throughput		: %d(kbps)\n",
			 (int)phl_stats->tx_tp_kbits);
	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"last tx time		: %d(ms)\n",
			 (int)phl_stats->last_tx_time_ms);

	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"tx request num to phl	: %d\n",
			 (int)phl_stats->txreq_num);

#ifdef CONFIG_PCI_HCI
	_dbg_tx_stats_pcie(phl_info, input, input_num, output, out_len, &used,
			   ctrl, ch);
#endif

	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"\nunicast rx bytes	: %llu\n", phl_stats->rx_byte_uni);
	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"total rx bytes		: %llu\n", phl_stats->rx_byte_total);
	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		 "rx throughput		: %d(kbps)\n",
			 (int)phl_stats->rx_tp_kbits);
	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
		"last rx time		: %d(ms)\n",
			 (int)phl_stats->last_rx_time_ms);
}

void phl_dbg_cmd_show_rssi(struct phl_info_t *phl_info, char input[][MAX_ARGV],
		 	   u32 input_num, char *output, u32 out_len)
{
	u32 used = 0;
	struct rtw_phl_rssi_stat *rssi_stat = NULL;
	struct rtw_phl_stainfo_t *psta = NULL;
	struct rtw_phl_com_t *phl_com = NULL;
	u32 macid = 0;

	if (NULL == phl_info)
		return;

	rssi_stat = &phl_info->phl_com->rssi_stat;
	phl_com = phl_info->phl_com;

	_os_spinlock(phl_com->drv_priv,
		     &(phl_com->rssi_stat.lock), _bh, NULL);

	PHL_DBG_MON_INFO(out_len, used, output + used,
			 out_len - used, "\nShow Moving Average RSSI Statistics Informations\n");

	PHL_DBG_MON_INFO(out_len, used, output + used,
			 out_len - used,
			 "DATA (A1 Match, AddrCam Valid) : %d\n",
			 rssi_stat->ma_rssi[RTW_RSSI_DATA_ACAM_A1M]);
	PHL_DBG_MON_INFO(out_len, used, output + used,
			 out_len - used,
			 "DATA (A1 Not Match, AddrCam Valid) : %d\n",
			 rssi_stat->ma_rssi[RTW_RSSI_DATA_ACAM]);
	PHL_DBG_MON_INFO(out_len, used, output + used,
			 out_len - used,
			 "DATA (other) : %d\n",
			 rssi_stat->ma_rssi[RTW_RSSI_DATA_OTHER]);

	PHL_DBG_MON_INFO(out_len, used, output + used,
			 out_len - used,
			 "MGNT (A1 Match, AddrCam Valid) : %d\n",
			 rssi_stat->ma_rssi[RTW_RSSI_MGNT_ACAM_A1M]);
	PHL_DBG_MON_INFO(out_len, used, output + used,
			 out_len - used,
			 "MGNT (A1 Not Match, AddrCam Valid) : %d\n",
			 rssi_stat->ma_rssi[RTW_RSSI_MGNT_ACAM]);
	PHL_DBG_MON_INFO(out_len, used, output + used,
			 out_len - used,
			 "MGNT (other) : %d\n",
			 rssi_stat->ma_rssi[RTW_RSSI_MGNT_OTHER]);

	PHL_DBG_MON_INFO(out_len, used, output + used,
			 out_len - used,
			 "CTRL (A1 Match, AddrCam Valid) : %d\n",
			 rssi_stat->ma_rssi[RTW_RSSI_CTRL_ACAM_A1M]);
	PHL_DBG_MON_INFO(out_len, used, output + used,
			 out_len - used,
			 "CTRL (A1 Not Match, AddrCam Valid) : %d\n",
			 rssi_stat->ma_rssi[RTW_RSSI_CTRL_ACAM]);
	PHL_DBG_MON_INFO(out_len, used, output + used,
			 out_len - used,
			 "CTRL (other) : %d\n",
			 rssi_stat->ma_rssi[RTW_RSSI_CTRL_OTHER]);

	PHL_DBG_MON_INFO(out_len, used, output + used,
			 out_len - used,
			 "Unknown : %d\n",
			 rssi_stat->ma_rssi[RTW_RSSI_UNKNOWN]);
	_os_spinunlock(phl_com->drv_priv,
		     &(phl_com->rssi_stat.lock), _bh, NULL);

	if(input_num > 1) {
		_get_hex_from_string(input[1], &macid);
		psta = rtw_phl_get_stainfo_by_macid(phl_info, (u16)macid);
		if (psta != NULL) {
			PHL_DBG_MON_INFO(out_len, used, output + used,
			 out_len - used,
			 "STA macid : %d ; mv_avg_rssi = %d ; asoc_rssi = %d (bb : %d, %d)\n",
			 (int)macid, (int)psta->hal_sta->rssi_stat.ma_rssi,
			 (int)psta->hal_sta->rssi_stat.assoc_rssi,
			 (int)(psta->hal_sta->rssi_stat.rssi >> 1),
			 (int)(psta->hal_sta->rssi_stat.rssi_ma >> 5));
		}
	}

}

void phl_dbg_cmd_parser(struct phl_info_t *phl_info, char input[][MAX_ARGV],
		        u32 input_num, char *output, u32 out_len)
{
	u8 id = 0;
	u32 i;
	u32 used = 0;
	u32 phl_ary_size = sizeof(phl_dbg_cmd_i) /
			   sizeof(struct phl_dbg_cmd_info);

	if (phl_ary_size == 0)
		return;

	PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used, "\n");
	/* Parsing Cmd ID */
	if (input_num) {
		for (i = 0; i < phl_ary_size; i++) {
			if (_os_strcmp(phl_dbg_cmd_i[i].name, input[0]) == 0) {
				id = phl_dbg_cmd_i[i].id;
				PHL_DBG("[%s]===>\n", phl_dbg_cmd_i[i].name);
				break;
			}
		}
		if (i == phl_ary_size) {
			PHL_DBG_MON_INFO(out_len, used, output + used,
				out_len - used, "PHL CMD not found!\n");
			return;
		}
	}

	switch (id) {
	case PHL_DBG_MON_HELP:
	{
		PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
				 "phl_dbg_cmd_parser : PHL_DBG_MON_HELP \n");
		for (i = 0; i < phl_ary_size - 2; i++)
			PHL_DBG_MON_INFO(out_len, used, output + used,
					 out_len - used, "%-5d: %s\n",
			          (int)i, phl_dbg_cmd_i[i + 2].name);

	}
	break;
	case PHL_DBG_MON_TEST:
	{
		PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
				 "[DBG] PHL_DBG_MON_TEST (SAMPLE) \n");
	}
	break;
	case PHL_DBG_COMP:
	{
		u32 ctrl = 0, comp = 0;
		u32 ret = 0;

		PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
			"[DBG] PHL_DBG_COMP [1=set, 2=clear] [comp_bit] \n");
		if (input_num <= 2)
			break;
		_get_hex_from_string(input[1], &ctrl);
		_get_hex_from_string(input[2], &comp);

		ret = rtw_phl_dbg_ctrl_comp((u8)ctrl, (u8)comp);
		PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
			"[DBG] PHL_DBG_COMP (COMP = 0x%x) \n", (int)ret);
	}
	break;
	case PHL_DBG_DUMP_WROLE:
	{
		_dump_wifi_role(phl_info, output, out_len);
	}
	break;
	case PHL_DBG_SET_CH_BW:
	{
		_phl_dbg_cmd_switch_chbw(phl_info, input, input_num,
					 output, out_len);
	}
	break;
	case PHL_DBG_SHOW_RX_RATE:
	{
		_dump_rx_rate(phl_info, input, input_num, output, out_len);
	}
	break;
	case PHL_DBG_SOUND :
	{
		phl_dbg_cmd_snd(phl_info, input, input_num, output, out_len);
	}
	break;
	case PHL_DBG_ASOC_STA:
	{
		phl_dbg_cmd_asoc_sta(phl_info, input, input_num, output, out_len);
	}
	break;
	#ifdef CONFIG_FSM
	case PHL_DBG_FSM:
	{
		phl_fsm_dbg(phl_info, input, input_num, output, out_len);
	}
	break;
	#endif
	case PHL_DBG_TRX_STATS:
	{
		phl_dbg_trx_stats(phl_info, input, input_num, output, out_len);
	}
	break;
	case PHL_DBG_LTR:
	{
		phl_dbg_ltr_stats(phl_info, input, input_num, output, out_len);
	}
	break;
	case PHL_SHOW_RSSI_STAT :
	{
		phl_dbg_cmd_show_rssi(phl_info, input, input_num, output, out_len);
	}
	break;
	case PHL_DBG_SER:
	{
		phl_ser_cmd_parser(phl_info, input, input_num, output, out_len);
	}
	break;
	case PHL_DBG_WOW:
	{
		_dump_wow_stats(phl_info, input, input_num, output, out_len);
	}
	break;
#ifdef CONFIG_POWER_SAVE
	case PHL_DBG_PS:
	{
		phl_ps_cmd_parser(phl_info, input, input_num, output, out_len);
	}
	break;
#endif
#ifdef CONFIG_MCC_SUPPORT
	case PHL_DBG_MCC:
	{
		_dump_mcc_info(phl_info, input, input_num, output, out_len);
	}
	break;
#endif /* CONFIG_MCC_SUPPORT */

	case PHL_DBG_ECSA:
	{
#ifdef CONFIG_PHL_ECSA
		struct rtw_wifi_role_t *role = NULL;
		struct rtw_phl_ecsa_param param = {0};
		int chan = 0;
		int bw = 0;
		int offset = 0;
		int count = 0;
		int op_class = 0;
		int mode = 0;
		int delay_start_ms = 0;
		if (input_num <= 7)
			break;
		_os_sscanf(input[1], "%d", &chan);
		_os_sscanf(input[2], "%d", &bw);
		_os_sscanf(input[3], "%d", &offset);
		_os_sscanf(input[4], "%d", &count);
		_os_sscanf(input[5], "%d", &op_class);
		_os_sscanf(input[6], "%d", &mode);
		_os_sscanf(input[7], "%d", &delay_start_ms);
		role = rtw_phl_get_wrole_by_ridx(phl_info->phl_com, 2);
		if(role){
			param.ecsa_type = ECSA_TYPE_AP;
			param.ch = (u8)chan;
			param.count = (u8)count;
			param.flag = 0;
			param.mode = (u8)mode;
			param.op_class = (u8)op_class;
			param.delay_start_ms = delay_start_ms;
			param.new_chan_def.chan = (u8)chan;
			param.new_chan_def.bw = (u8)bw;
			param.new_chan_def.offset = (u8)offset;

			rtw_phl_ecsa_start(phl_info, role, &param);
		}
		else
			PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "[DBG] Role 2 is NULL\n");
#else
		PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "[DBG] ECSA not support!\n");
#endif /* CONFIG_PHL_ECSA */
	}
	break;
	case PHL_DBG_CFG_TX_DUTY:
	{
		int tx_duty = 100;
		PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
			"[DBG] PHL_DBG_CFG_TX_DUTY [100-30] \n");
		PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
			"[DBG] TX duty interval = 100ms \n");
		PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
			"[DBG] No TX duty control: 100 \n");
		PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
			"[DBG] Minimum TX duty control: 30 (TX 30ms pause TX 70ms)\n");
		if (input_num <= 1)
			break;
		_os_sscanf(input[1], "%d", &tx_duty);
		if (tx_duty < 30 || tx_duty > 100) {
			PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
				"[DBG] TX duty should be 30~100 \n");
			break;
		}
		if (tx_duty == 100)
			phl_thermal_protect_stop_tx_duty(phl_info);
		else
			phl_thermal_protect_cfg_tx_duty(phl_info, 100, (u8)tx_duty);

		PHL_DBG_MON_INFO(out_len, used, output + used, out_len - used,
			"[DBG] Current TX duty control: %d \n", tx_duty);
	}
	break;
	default:
		PHL_DBG_MON_INFO(out_len, used, output + used,
			out_len - used, "[DBG] Do not support this command\n");
		break;
	}
}

s32
phl_dbg_proc_cmd(struct phl_info_t *phl_info,
		 char *input, char *output, u32 out_len)
{
	char *token;
	u32 argc = 0;
	char argv[MAX_ARGC][MAX_ARGV];

	do {
		token = _os_strsep(&input, ", ");
		if (token) {
			if (_os_strlen((u8 *)token) <= MAX_ARGV)
				_os_strcpy(argv[argc], token);

			argc++;
		} else {
			break;
		}
	} while (argc < MAX_ARGC);

	if (argc == 1)
		argv[0][_os_strlen((u8 *)argv[0])] = '\0';

	phl_dbg_cmd_parser(phl_info, argv, argc, output, out_len);

	return 0;
}

enum rtw_hal_status
rtw_phl_dbg_proc_cmd(struct phl_info_t *phl_info,
		     struct rtw_proc_cmd *incmd,
		     char *output,
		     u32 out_len)
{
	if (incmd->in_type == RTW_ARG_TYPE_BUF) {
		phl_dbg_proc_cmd(phl_info, incmd->in.buf, output, out_len);
	} else if(incmd->in_type == RTW_ARG_TYPE_ARRAY){
		phl_dbg_cmd_parser(phl_info, incmd->in.vector,
				   incmd->in_cnt_len, output, out_len);
	}
	return RTW_HAL_STATUS_SUCCESS;
}

#endif
