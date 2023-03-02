/******************************************************************************
 *
 * Copyright(c) 2019 Realtek Corporation.
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
#define _PHL_PS_DBG_CMD_C_
#include "../phl_headers.h"
#ifdef CONFIG_POWER_SAVE
struct phl_ps_cmd_info {
	char name[16];
	u8 id;
};

enum PHL_PS_CMD_ID {
	PHL_PS_HELP,
	PHL_PS_SHOW,
	PHL_PS_TEST,
	PHL_PS_STOP_PS
};

struct phl_ps_cmd_info phl_ps_cmd_i[] = {
	{"-h", PHL_PS_HELP},
	{"show", PHL_PS_SHOW},
	{"test", PHL_PS_TEST},
	{"stop_ps", PHL_PS_STOP_PS}
};

/* echo phl ps show */
static void _phl_ps_cmd_show(struct phl_info_t *phl_info, u32 *used, char input[][MAX_ARGV],
			u32 input_num, char *output, u32 out_len)
{
	if (phl_info == NULL)
		return;

	phl_ps_dbg_dump(phl_info, used, input, input_num, output, out_len);
}

/* debug to disable power saving */
static void _phl_ps_cmd_stop_ps(struct phl_info_t *phl_info, u32 *used, char input[][MAX_ARGV],
			u32 input_num, char *output, u32 out_len)
{
	if (phl_info == NULL)
		return;

	phl_ps_dbg_stop_ps(phl_info, used, input, input_num, output, out_len);
}

static void _phl_ps_cmd_test_ps(struct phl_info_t *phl_info, u32 *used, char input[][MAX_ARGV],
			u32 input_num, char *output, u32 out_len)
{
	if (phl_info == NULL)
		return;

	phl_ps_dbg_test_ps(phl_info, used, input, input_num, output, out_len);
}

void phl_ps_cmd_parser(struct phl_info_t *phl_info, char input[][MAX_ARGV],
			u32 input_num, char *output, u32 out_len)
{
	u32 used = 0;
	u8 id = 0;
	u32 i;
	u32 array_size = sizeof(phl_ps_cmd_i) / sizeof(struct phl_ps_cmd_info);

	PS_CNSL(out_len, used, output + used, out_len - used, "\n");

	/* Parsing Cmd ID */
	if (input_num) {
		for (i = 0; i < array_size; i++) {
			if (!_os_strcmp(phl_ps_cmd_i[i].name, input[1])) {
				id = phl_ps_cmd_i[i].id;
				break;
			}
		}
	}

	switch (id) {
	case PHL_PS_TEST:
		_phl_ps_cmd_test_ps(phl_info, &used, input, input_num,
					output, out_len);
		break;
	case PHL_PS_SHOW:
		_phl_ps_cmd_show(phl_info, &used, input, input_num,
					output, out_len);
		break;
	case PHL_PS_STOP_PS:
		_phl_ps_cmd_stop_ps(phl_info, &used, input, input_num,
					output, out_len);
		break;
	default:
		PS_CNSL(out_len, used, output + used, out_len - used,
			 "command not supported !!\n");

		/* fall through */
	case PHL_PS_HELP:
		PS_CNSL(out_len, used, output + used, out_len - used,
			 "PS cmd ==>\n");

		for (i = 0; i < array_size - 1; i++)
			PS_CNSL(out_len, used, output + used, out_len - used,
				 " %s\n", phl_ps_cmd_i[i + 1].name);
		break;
	}
}

void phl_ps_dbg_dump(struct phl_info_t *phl_info, u32 *used,
	char input[][MAX_ARGV], u32 input_num, char *output, u32 out_len)
{
	struct phl_module_op_info op_info = {0};
	struct phl_cmd_ps_basic_info info = {0};
	struct rtw_ps_cap_t *ps_cap = NULL;

	PS_CNSL(out_len, *used, output + *used, out_len - *used,
		"========== Basic Info ==========\n");

	op_info.op_code = PS_MDL_OP_BASIC_INFO;
	op_info.inbuf = (u8 *)&info;
#ifdef CONFIG_CMD_DISP
	if (phl_disp_eng_query_bk_module_info(phl_info, HW_BAND_0,
			PHL_MDL_POWER_MGNT, &op_info) != RTW_PHL_STATUS_SUCCESS)
		return;

	PS_CNSL(out_len, *used, output + *used, out_len - *used,
		"ps_mode: %s, cur_pwr_lvl: %s\
		\nap_active: %s, gc_active: %s\
		\ntx traffic lvl: %s, rx traffic lvl: %s\n",
		phl_ps_ps_mode_to_str(info.ps_mode), phl_ps_pwr_lvl_to_str(info.cur_pwr_lvl),
		(info.ap_active == true ? "yes" : "no"), (info.gc_active == true ? "yes" : "no"),
		phl_tfc_lvl_to_str(phl_info->phl_com->phl_stats.tx_traffic.lvl), phl_tfc_lvl_to_str(phl_info->phl_com->phl_stats.rx_traffic.lvl));

	if (info.sta != NULL) {
		PS_CNSL(out_len, *used, output + *used, out_len - *used,
				"chnl: %d, rssi: %d, rssi_bcn: %d\n",
				info.sta->chandef.chan, rtw_hal_get_sta_rssi(info.sta), phl_get_min_rssi_bcn(phl_info));
	}

	PS_CNSL(out_len, *used, output + *used, out_len - *used,
		"========== Advanced Info ==========\n");

	PS_CNSL(out_len, *used, output + *used, out_len - *used,
		"last enter reason: %s\
		\nlast leave reason: %s\
		\nreject all pwr req: %s\
		\nbtc req pwr: %s\
		\nruntime stop reason: %d\n",
		info.enter_rson,
		info.leave_rson,
		(info.rej_pwr_req == true ? "yes" : "no"),
		(info.btc_req_pwr == true ? "yes" : "no"),
		info.rt_stop_rson);

	PS_CNSL(out_len, *used, output + *used, out_len - *used,
		"========== Capability ==========\n");

	ps_cap = _get_ps_cap(phl_info);

	PS_CNSL(out_len, *used, output + *used, out_len - *used,
		"init_rf_state: %s, init_rt_stop_rson: 0x%x, leave_fail_act: 0x%x\
		\nlps: %s, lps_cap: %s, lps_pause_tx: %d\
		\nawake_interval: %d, listen_bcn_mode: %d, smart_ps_mode: %d\
		\nrssi_enter_threshold: %d, rssi_leave_threshold: %d, rssi_diff_threshold: %d\
		\nips: %s, ips_cap: %s\n",
		(ps_cap->init_rf_state ? "off" : "on"), ps_cap->init_rt_stop_rson, ps_cap->leave_fail_act,
		phl_ps_op_mode_to_str(ps_cap->lps_en), phl_ps_pwr_lvl_to_str(phl_ps_judge_pwr_lvl(ps_cap->lps_cap, PS_MODE_LPS, true)), ps_cap->lps_pause_tx,
		ps_cap->lps_awake_interval, ps_cap->lps_listen_bcn_mode, ps_cap->lps_smart_ps_mode,
		ps_cap->lps_rssi_enter_threshold, ps_cap->lps_rssi_leave_threshold, ps_cap->lps_rssi_diff_threshold,
		phl_ps_op_mode_to_str(ps_cap->ips_en), phl_ps_pwr_lvl_to_str(phl_ps_judge_pwr_lvl(ps_cap->ips_cap, PS_MODE_IPS, true)));

#else
	PS_CNSL(out_len, *used, output + *used, out_len - *used,
		"Not support.\n");
#endif
}

void phl_ps_dbg_stop_ps(struct phl_info_t *phl_info, u32 *used,
	char input[][MAX_ARGV], u32 input_num, char *output, u32 out_len)
{
	u32 stop = 0;
	bool ps_en = false;
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;

	do {
		if (input_num < 3){
			PS_CNSL(out_len, *used, output + *used, out_len - *used,
					 "[DBG] echo phl ps stop_ps <0/1>\n");
			break;
		}

		if (!_get_hex_from_string(input[2], &stop))
			break;

		if ((phl_com->dev_sw_cap.ps_cap.ips_en != PS_OP_MODE_AUTO)&&
			(phl_com->dev_sw_cap.ps_cap.lps_en != PS_OP_MODE_AUTO)){
				PS_CNSL(out_len, *used, output + *used, out_len - *used,
					 "One of ips or lps should set PS_OP_MODE_AUTO\n");
				break;
		}

		if (stop)
			ps_en = false;
		else
			ps_en = true;

		rtw_phl_ps_set_rt_cap((void*)phl_info, HW_BAND_0, ps_en, PS_RT_DEBUG);

	} while (0);
}

void phl_ps_dbg_test_ps(struct phl_info_t *phl_info, u32 *used,
	char input[][MAX_ARGV], u32 input_num, char *output, u32 out_len)
{
	u32 enter = 0;
	u8 ps_mode = PS_MODE_NONE;

	do {
		if (input_num < 4){
			PS_CNSL(out_len, *used, output + *used, out_len - *used,
					 "[DBG] echo phl ps test <ips/lps> <0/1>\n");
			break;
		}

		if (!_os_strcmp(input[2], "lps")) {
			ps_mode = PS_MODE_LPS;
		} else if (!_os_strcmp(input[2], "ips")) {
			ps_mode = PS_MODE_IPS;
		} else {
			break;
		}

		if (!_get_hex_from_string(input[3], &enter))
			break;

		phl_ps_dbg_set_ps(phl_info, ps_mode, (enter ? true : false));

	} while (0);
}

#endif /* CONFIG_POWER_SAVE */
