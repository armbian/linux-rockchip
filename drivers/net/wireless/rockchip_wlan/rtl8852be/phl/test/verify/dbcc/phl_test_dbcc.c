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
#define _PHL_TEST_DBCC_C_
#include "../../../phl_headers.h"
#include "../phl_test_verify_def.h"
#include "../phl_test_verify_api.h"
#include "phl_test_dbcc_def.h"

#ifdef CONFIG_PHL_TEST_VERIFY
static u8 dbcc_get_class_from_buf(struct verify_context *ctx)
{
	u8 *buf_tmp = NULL;
	u8 dbcc_class = DBCC_CLASS_MAX;

	if (ctx && ctx->buf &&
		(ctx->buf_len > VERIFY_CMD_HDR_SIZE)) {
		buf_tmp	= (u8 *)VERIFY_GET_SUBUF((u8 *)ctx->buf);
		dbcc_class = buf_tmp[0];
	}
	return dbcc_class;
}

static enum rtw_phl_status
dbcc_config(struct verify_context *ctx, struct dbcc_config_arg *arg)
{
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_FAILURE;
	struct phl_info_t *phl_info = ctx->phl;

	if (arg == NULL) {
		return phl_status;
	}

	PHL_INFO("%s: en %u act %u id %u\n", __FUNCTION__, arg->dbcc_en, arg->action, arg->macid);
#ifdef CONFIG_DBCC_SUPPORT
	//if (phl_info->phl_com->dev_cap.dbcc_sup)
	phl_status = rtw_phl_dbcc_test(phl_info, arg->action, &arg->dbcc_en);
#else
	PHL_ERR("%s: %s No Support DBCC\n", __FUNCTION__,
			phl_info->phl_com->hal_spec.ic_name);
	phl_status = RTW_PHL_STATUS_SUCCESS;
#endif
	return phl_status;
}

static enum rtw_phl_status
dbcc_tx_tmac(struct verify_context *ctx, struct dbcc_tx_arg *arg)
{
	struct rtw_trx_test_param test_param = { 0 };

	rtw_phl_trx_default_param(ctx->phl, &test_param);

	test_param.tx_cap.f_rate = arg->data_rate;

	test_param.tx_cap.macid = arg->macid;
	if (arg->macid) {
		test_param.tx_cap.dma_ch = 4;
		/* test_param.tx_cap.q_sel = 8; */
		test_param.tx_cap.band = 1;
		test_param.cur_addr[4] = 0xbb;
		test_param.cur_addr[5] = 0xbb;
	} else {
		test_param.tx_cap.dma_ch = 0;
		/* test_param.tx_cap.q_sel = 0; */
		test_param.tx_cap.band = 0;
		test_param.cur_addr[4] = 0xaa;
		test_param.cur_addr[5] = 0xaa;
	}
	test_param.tx_cap.hal_port = 0;
	test_param.tx_payload_size = 1000;
	test_param.tx_cap.f_gi_ltf = 0;
	test_param.tx_cap.f_stbc = 0;
	test_param.tx_cap.f_ldpc = 0;
	test_param.tx_cap.tid = 0;

	rtw_phl_trx_testsuite(ctx->phl, &test_param);

	return RTW_PHL_STATUS_SUCCESS;
}

static enum rtw_phl_status
dbcc_tx(struct verify_context *ctx, struct dbcc_tx_arg *arg)
{
	void *d = phl_to_drvpriv(ctx->phl);
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_FAILURE;

	if (arg == NULL) {
		return phl_status;
	}

	/* TBD: only for this moment, we force the macid 0 and 1 */
	if (arg->tx_method == 0) {
		phl_status = dbcc_tx_tmac(ctx, arg);
	} else if (arg->tx_method == 1) {
		if (arg->tx_count > 0) {
			int i;
			bool cctx = false;

			if (arg->macid == 2)
				cctx = true;
			for (i = 0; i < arg->tx_count; i++) {
				if (cctx)
					arg->macid = i % 2;
				phl_status = dbcc_tx_tmac(ctx, arg);
				_os_delay_ms(d, 10);
			}
		}
	}

	return phl_status;
}

static enum rtw_phl_status
dbcc_tx_status(struct verify_context *ctx, struct dbcc_tx_cnt_arg *arg)
{
#ifndef CONFIG_DBCC_SUPPORT
	return RTW_PHL_STATUS_SUCCESS;
#else
#if 0	/* no used in this moment */
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_FAILURE;
	struct phl_info_t *phl_info = ctx->phl;

	if (arg == NULL) {
		return phl_status;
	}

	PHL_INFO("%s: arg %u cmd %u band %u", __FUNCTION__, arg->arg_class, arg->cmd, arg->band);
	if (arg->cmd == 0) {
		phl_status = rtw_hal_dbcc_reset_tx_cnt(phl_info->hal, ctx->phl_com, arg->band);
	} else if (arg->cmd == 1) {
		if (arg->band) {
			phl_status = rtw_hal_dbcc_tx_cnt(phl_info->hal, ctx->phl_com, arg->band);
			PHL_INFO("%s: BB REG 0x14240 = 0x%X\n", __FUNCTION__,
				rtw_hal_read_bbreg(phl_info->hal, 0x14240, 0xffffffff));
		} else {
			phl_status = rtw_hal_dbcc_tx_cnt(phl_info->hal, ctx->phl_com, arg->band);
			PHL_INFO("%s: BB REG 0x14140 = 0x%X\n", __FUNCTION__,
				rtw_hal_read_bbreg(phl_info->hal, 0x14140, 0xffffffff));
		}
	}

	return phl_status;
#else
	return RTW_PHL_STATUS_SUCCESS;
#endif
#endif
}

enum rtw_phl_status rtw_test_dbcc_cmd_process(void *priv)
{
	struct verify_context *ctx = NULL;
	struct rtw_phl_com_t *phl_com = NULL;
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_FAILURE;
	u8 dbcc_class = DBCC_CLASS_MAX;

	FUNCIN();

	if (priv == NULL)
		return phl_status;

	ctx = (struct verify_context *)priv;
	phl_com = ctx->phl_com;

	if ((ctx->buf_len < VERIFY_CMD_HDR_SIZE)) {
		PHL_ERR("%s: Invalid buffer content!\n", __FUNCTION__);
		return phl_status;
	}

	dbcc_class = dbcc_get_class_from_buf(ctx);
	switch (dbcc_class) {
	case DBCC_CLASS_CONFIG:
		PHL_INFO("%s: class = DBCC_CLASS_CONFIG\n", __FUNCTION__);
		phl_status = dbcc_config(ctx, (struct dbcc_config_arg *)VERIFY_GET_SUBUF((u8 *)ctx->buf));
		break;
	case DBCC_CLASS_TX:
		PHL_INFO("%s: class = DBCC_CLASS_TX\n", __FUNCTION__);
		phl_status = dbcc_tx(ctx, (struct dbcc_tx_arg *)VERIFY_GET_SUBUF((u8 *)ctx->buf));
		break;
	case DBCC_CLASS_TX_ST:
		PHL_INFO("%s: class = DBCC_CLASS_TX_ST\n", __FUNCTION__);
		phl_status = dbcc_tx_status(ctx, (struct dbcc_tx_cnt_arg *)VERIFY_GET_SUBUF((u8 *)ctx->buf));
		break;
	case DBCC_CLASS_RX:
		PHL_INFO("%s: class = DBCC_CLASS_RX\n", __FUNCTION__);
		//phl_status = dbcc_rx(ctx, (struct dbcc_rx_arg *)VERIFY_GET_SUBUF((u8 *)ctx->buf));
		break;
	case DBCC_CLASS_RX_ST:
		PHL_INFO("%s: class = DBCC_CLASS_RX_ST\n", __FUNCTION__);
		//phl_status = dbcc_rx_status(ctx, (struct dbcc_rx_arg *)VERIFY_GET_SUBUF((u8 *)ctx->buf));
		break;
	default:
		PHL_WARN("%s: Unknown DBCC_CLASS! (%d)\n", __FUNCTION__, dbcc_class);
		break;
	}

	FUNCOUT();

	return phl_status;
}
#else
enum rtw_phl_status rtw_test_dbcc_cmd_process(void *priv)
{
	return RTW_PHL_STATUS_SUCCESS;
}
#endif /* CONFIG_PHL_TEST_VERIFY */
