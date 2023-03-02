/******************************************************************************
 *
 * Copyright(c) 2016 - 2019 Realtek Corporation.
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
#define _RTL8852B_BTC_C_

#include "../../hal_headers_le.h"
#include "../hal_btc.h"

#ifdef CONFIG_BTCOEX
#ifdef CONFIG_RTL8852B

#include "btc_8852b.h"

/* WL rssi threshold in % (dbm = % - 110)
 * array size limit by BTC_WL_RSSI_THMAX
 * BT rssi threshold in % (dbm = % - 100)
 * array size limit by BTC_BT_RSSI_THMAX
 */
static const u8 btc_8852b_wl_rssi_thres[BTC_WL_RSSI_THMAX] = {60, 50, 40, 30};
static const u8 btc_8852b_bt_rssi_thres[BTC_BT_RSSI_THMAX] = {40, 36, 31, 28};

static struct btc_chip_ops btc_8852b_ops = {
	_8852b_rfe_type,
	_8852b_init_cfg,
	_8852b_wl_pri,
	_8852b_wl_tx_power,
	_8852b_wl_rx_gain,
	_8852b_wl_s1_standby,
	_8852b_wl_req_mac,
	_8852b_update_bt_cnt,
	_8852b_bt_rssi
};

/* Set  WL/BT periodical moniter reg, Max size: CXMREG_MAX*/
static struct fbtc_mreg btc_8852b_mon_reg[] = {
	{REG_MAC, 4, 0xda24},
	{REG_MAC, 4, 0xda28},
	{REG_MAC, 4, 0xda2c},
	{REG_MAC, 4, 0xda30},
	{REG_MAC, 4, 0xda4c},
	{REG_MAC, 4, 0xda10},
	{REG_MAC, 4, 0xda20},
	{REG_MAC, 4, 0xda34},
	{REG_MAC, 4, 0xcef4},
	{REG_MAC, 4, 0x8424},
	{REG_MAC, 4, 0xd200},
	{REG_MAC, 4, 0xd220},
	{REG_BB, 4, 0x980},
	/*{REG_RF, 2, 0x2},*/ /* for RF, bytes->path, 1:A, 2:B... */
	/*{REG_RF, 2, 0x18},*/
	/*{REG_BT_RF, 2, 0x9},*/
	/*{REG_BT_MODEM, 2, 0xa} */
};

/* wl_tx_power: 255->original, else-> bit7:signed bit, ex: 13:13dBm, 0x85:-5dBm
 * wl_rx_gain: 0->original, 1-> for Free-run, 2-> for BTG co-rx
 * bt_tx_power: decrease power, 0->original, 5 -> decreas 5dB.
 * bt_rx_gain: BT LNA constrain Level, 7->original
 */

struct btc_rf_trx_para btc_8852b_rf_ul[] = {
	{255, 0, 0, 7}, /* 0 -> original */
	{255, 2, 0, 7}, /* 1 -> for BT-connected ACI issue && BTG co-rx */
	{255, 0, 0, 7}, /* 2 ->reserved for shared-antenna */
	{255, 0, 0, 7}, /* 3- >reserved for shared-antenna */
	{255, 0, 0, 7}, /* 4 ->reserved for shared-antenna */
	{255, 0, 0, 7}, /* the below id is for non-shared-antenna free-run */
	{6, 1, 0, 7},
	{13, 1, 0, 7},
	{13, 1, 0, 7}
};

struct btc_rf_trx_para btc_8852b_rf_dl[] = {
	{255, 0, 0, 7}, /* 0 -> original */
	{255, 2, 0, 7}, /* 1 -> reserved for shared-antenna */
	{255, 0, 0, 7}, /* 2 ->reserved for shared-antenna */
	{255, 0, 0, 7}, /* 3- >reserved for shared-antenna */
	{255, 0, 0, 7}, /* 4 ->reserved for shared-antenna */
	{255, 0, 0, 7}, /* the below id is for non-shared-antenna free-run */
	{255, 1, 0, 7},
	{255, 1, 0, 7},
	{255, 1, 0, 7}
};

const struct btc_chip chip_8852b = {
	CHIP_WIFI6_8852B, /* chip id */
	0x0, /* chip para ver */
	0x6, /* desired bt_ver */
	0x06010000, /* desired halbtc ver */
	0x1, /* scoreboard version */
	0x1, /* mailbox version*/
	BTC_COEX_RTK_MODE, /* pta_mode */
	BTC_COEX_INNER, /* pta_direction */
	6, /* afh_guard_ch */
	btc_8852b_wl_rssi_thres, /* wl rssi threshold level */
	btc_8852b_bt_rssi_thres, /* bt rssi threshold level */
	(u8)2, /* rssi tolerance */
	&btc_8852b_ops, /* chip-dependent function */
	ARRAY_SIZE(btc_8852b_mon_reg),
	btc_8852b_mon_reg, /* wl moniter register */
	ARRAY_SIZE(btc_8852b_rf_ul),
	btc_8852b_rf_ul,
	ARRAY_SIZE(btc_8852b_rf_dl),
	btc_8852b_rf_dl
};

void _8852b_rfe_type(struct btc_t *btc)
{
	struct rtw_phl_com_t *p = btc->phl;
	struct rtw_hal_com_t *h = btc->hal;
	struct btc_module *module = &btc->mdinfo;

	PHL_INFO("[BTC], %s !! \n", __FUNCTION__);

	module->rfe_type = p->dev_cap.rfe_type;/* get from final capability of device  */
	module->kt_ver = h->cv;
	module->bt_solo = 0;
	module->switch_type = BTC_SWITCH_INTERNAL;

#if BTC_NON_SHARED_ANT_FREERUN
	module->ant.num = 3;
#else
	if (module->rfe_type > 0)
		module->ant.num = (module->rfe_type % 2?  2 : 3);
	else
		module->ant.num = 2;
#endif
	module->ant.diversity = 0;
	module->ant.isolation = 10;

	if (module->ant.num == 3) {
		module->ant.type = BTC_ANT_DEDICATED;
		module->bt_pos = BTC_BT_ALONE;
	} else {
		module->ant.type = BTC_ANT_SHARED;
		module->bt_pos = BTC_BT_BTG;
	}
}

void _8852b_wl_tx_power(struct btc_t *btc, u32 level)
{
	/*
	* =========== All-Time WL Tx power control ===========
    	* (ex: all-time fix WL Tx 10dBm , don¡¦t care GNT _BT and GNT _LTE)
	* Turn off per-packet power control
	* 0xD220[1] = 0, 0xD220[2] = 0;
	*
	* disable using related power table
	* 0xd208[20] = 0, 0xd208[21] = 0, 0xd21c[17] = 0, 0xd20c[29] = 0;
	*
	* enable force tx power mode and value
	* 0xD200[9] = 1;
	* 0xD200[8:0] = 0x28; S(9,2): 1step= 0.25dB, i.e. 40*0.25 = 10 dBm
	* =======================================================
	*
	* =========== per-packet Tx power control ===========
	* (ex: GNT_BT = 1 -> 5dBm,  GNT _BT = 0 -> 10dBm)
	* Turn on per-packet power control
	* 0xD220[1] = 1, 0xD220[2] = 0;
	* 0xD220[11:3] = 0x14; S(9,2): 1step = 0.25dB, i.e. 20*0.25 = 5 dBm
	*
	* disable using related power table
	* 0xd208[20] = 0, 0xd208[21] = 0, 0xd21c[17] = 0, 0xd20c[29] = 0;
	*
	* enable force tx power mode and value
	* 0xD200[9] = 1;
	* 0xD200[8:0] = 0x28;  S(9,2), sign, 1 step = 0.25dB, i.e. 40*0.25 = 10 dBm
	* =========================================================================
	*
	* level define:
	*    if level = 255 -> back to original (btc don't control)
	*    else in dBm --> bit7->signed bit, ex: 0xa-> +10dBm, 0x85-> -5dBm
	* pwr_val define:
	      bit15~0  --> All-time (GNT_BT = 0) Tx power control
	      bit31~16 --> Per-Packet (GNT_BT = 1) Tx power control
	*/
	u32 pwr_val;

	if (level == BTC_WL_DEF_TX_PWR) { /* back to original */
		pwr_val = bMASKDW;
	} else { /* only apply "force tx power" */
		pwr_val = (level & 0x7f) << 2; /* to fit s(9,2) format */
		if (pwr_val > BTC_WL_DEF_TX_PWR)
			pwr_val = BTC_WL_DEF_TX_PWR;

		if (level & BIT(7)) /* negative value */
			pwr_val |= BIT(8); /* to fit s(9,2) format */
		pwr_val |= bMASKHW;
	}

	rtw_hal_rf_wl_tx_power_control(btc->hal, pwr_val);
}

void _8852b_set_wl_lna2(struct btc_t *btc, u8 level)
{
	/* level=0 Default: TIA 1/0= (LNA2,TIAN6) = (7,1)/(5,1) = 21dB/12dB
         * level=1 Fix LNA2=5: TIA 1/0= (LNA2,TIAN6) = (5,0)/(5,1) = 18dB/12dB
         * To improve BT ACI in co-rx
         */
#if 0   /* RDC: disable this function for A-cut RxA TIA gain bug,
	 * it should be enabled for B-cut if nug fixed */
	switch (level) {
	case 0: /* default */
		_write_wl_rf_reg(btc, RF_PATH_B, 0xef, bMASKRF, 0x1000);
		_write_wl_rf_reg(btc, RF_PATH_B, 0x33, bMASKRF, 0x3);
		_write_wl_rf_reg(btc, RF_PATH_B, 0x3f, bMASKRF, 0x17);
		_write_wl_rf_reg(btc, RF_PATH_B, 0x33, bMASKRF, 0x2);
		_write_wl_rf_reg(btc, RF_PATH_B, 0x3f, bMASKRF, 0x15);
		_write_wl_rf_reg(btc, RF_PATH_B, 0xef, bMASKRF, 0x0);
		break;
	case 1: /* Fix LNA2=5  */
		_write_wl_rf_reg(btc, RF_PATH_B, 0xef, bMASKRF, 0x1000);
		_write_wl_rf_reg(btc, RF_PATH_B, 0x33, bMASKRF, 0x3);
		_write_wl_rf_reg(btc, RF_PATH_B, 0x3f, bMASKRF, 0x5);
		_write_wl_rf_reg(btc, RF_PATH_B, 0x33, bMASKRF, 0x2);
		_write_wl_rf_reg(btc, RF_PATH_B, 0x3f, bMASKRF, 0x15);
		_write_wl_rf_reg(btc, RF_PATH_B, 0xef, bMASKRF, 0x0);
		break;
	}
#endif
}

void _8852b_wl_rx_gain(struct btc_t *btc, u32 level)
{
	/* wl bb setting for FDD -> rtw_hal_bb_ctrl_btc_preagc()
	 * OP1db Back-off(no LNA6) +  Fixed TIA corner + RX_BB Back-off +
	 * DFIR Type 3
	*/

	switch (level) {
	case 0: /* original */
		rtw_hal_bb_ctrl_btc_preagc(btc->hal, false);
		/* _8852b_set_wl_lna2(btc, 0); */
		break;
	case 1: /* for FDD free-run */
		rtw_hal_bb_ctrl_btc_preagc(btc->hal, true);
		/* _8852b_set_wl_lna2(btc, 0); */
		break;
	case 2: /* for BTG Co-Rx*/
		rtw_hal_bb_ctrl_btc_preagc(btc->hal, false);
		/* _8852b_set_wl_lna2(btc, 1); */
		break;
	}
}

u8 _8852b_bt_rssi(struct btc_t *btc, u8 val)
{
	val = (val <= 127? 100 : (val >= 156? val - 156 : 0));
	val = val + 6; /* compensate offset */

	if (val > 100)
		val = 100;

	return (val);
}

void _8852b_set_wl_trx_mask(struct btc_t *btc, u8 path, u8 group, u32 val)
{
	_write_wl_rf_reg(btc, path, 0xef, bMASKRF, 0x20000);
	_write_wl_rf_reg(btc, path, 0x33, bMASKRF, group);
	_write_wl_rf_reg(btc, path, 0x3f, bMASKRF, val);
	_write_wl_rf_reg(btc, path, 0xef, bMASKRF, 0x0);
}

void _8852b_wl_s1_standby(struct btc_t *btc, u32 state)
{
	_write_wl_rf_reg(btc, RF_PATH_B, 0xef, bMASKRF, 0x80000);
	_write_wl_rf_reg(btc, RF_PATH_B, 0x33, bMASKRF, 0x1);
	_write_wl_rf_reg(btc, RF_PATH_B, 0x3e, bMASKRF, 0x31);

	/* set WL standby = Rx for GNT_BT_Tx = 1->0 settle issue */
	if (state == 1)
		_write_wl_rf_reg(btc, RF_PATH_B, 0x3f, bMASKRF, 0x00579);
	else
		_write_wl_rf_reg(btc, RF_PATH_B, 0x3f, bMASKRF, 0x00020);

	_write_wl_rf_reg(btc, RF_PATH_B, 0xef, bMASKRF, 0x0);
}

void _8852b_wl_req_mac(struct btc_t *btc, u8 mac_id)
{
	u32 val1;

	val1 = _read_cx_reg(btc, R_BTC_CFG);

	if (mac_id == HW_PHY_0)
		val1 = val1 & (~B_BTC_WL_SRC);
	else
		val1 = val1 | B_BTC_WL_SRC;

	_write_cx_reg(btc, R_BTC_CFG, val1);
}

void _8852b_update_bt_cnt(struct btc_t *btc)
{
	struct btc_cx *cx = &btc->cx;
	u32 val, val1;

	val = _read_cx_reg(btc, R_BTC_BT_STAST_HIGH);
	cx->cnt_bt[BTC_BCNT_HIPRI_TX] = val & bMASKLW;
	cx->cnt_bt[BTC_BCNT_HIPRI_RX] = (val & bMASKHW) >> 16;

	val = _read_cx_reg(btc, R_BTC_BT_STAST_LOW);
	cx->cnt_bt[BTC_BCNT_LOPRI_TX] = val & bMASKLW;
	cx->cnt_bt[BTC_BCNT_LOPRI_RX] = (val & bMASKHW) >> 16;

	/* clock-gate off before reset counter*/
	val1 = _read_cx_reg(btc, R_BTC_CFG);
	_write_cx_reg(btc, R_BTC_CFG, val1 | B_BTC_DIS_BTC_CLK_G);

	val = _read_cx_reg(btc, R_BTC_CSR_MODE);
	_write_cx_reg(btc, R_BTC_CSR_MODE, val & (~B_BTC_BT_CNT_REST));
	_write_cx_reg(btc, R_BTC_CSR_MODE, val | B_BTC_BT_CNT_REST);

	_write_cx_reg(btc, R_BTC_CFG, val1 & (~B_BTC_DIS_BTC_CLK_G));
}

void _8852b_init_cfg(struct btc_t *btc)
{
	struct rtw_hal_com_t *h = btc->hal;
	struct btc_module *module = &btc->mdinfo;
	u32 val = 0;

	PHL_INFO("[BTC], %s !! \n", __FUNCTION__);

	/* PTA init  */
	rtw_hal_mac_coex_init(h, chip_8852b.pta_mode, chip_8852b.pta_direction);

	/* set WL Tx response = Hi-Pri */
	btc->chip->ops->wl_pri(btc, BTC_PRI_MASK_TX_RESP, true);

	/* set WL Tx beacon = Hi-Pri */
	btc->chip->ops->wl_pri(btc, BTC_PRI_MASK_BEACON, true);

	/* set rf gnt debug off*/
	_write_wl_rf_reg(btc, RF_PATH_A, 0x2, bMASKRF, 0x0);
	_write_wl_rf_reg(btc, RF_PATH_B, 0x2, bMASKRF, 0x0);

	/* set WL Tx thru in TRX mask table if GNT_WL=0 && BT_S1=ss group */
	if (module->ant.type == BTC_ANT_SHARED) {
		_8852b_set_wl_trx_mask(btc, RF_PATH_A, BTC_BT_SS_GROUP, 0x5ff);
		_8852b_set_wl_trx_mask(btc, RF_PATH_B, BTC_BT_SS_GROUP, 0x5ff);
		/* set path-A(S0) Tx/Rx no-mask if GNT_WL=0 && BT_S1=tx group */
		_8852b_set_wl_trx_mask(btc, RF_PATH_A, BTC_BT_TX_GROUP, 0x5ff);
	} else { /* set WL Tx stb if GNT_WL = 0 && BT_S1 = ss group for 3-ant */
		_8852b_set_wl_trx_mask(btc, RF_PATH_A, BTC_BT_SS_GROUP, 0x5df);
		_8852b_set_wl_trx_mask(btc, RF_PATH_B, BTC_BT_SS_GROUP, 0x5df);
	}

	/* set PTA break table */
	_write_cx_reg(btc, R_BTC_BREAK_TABLE, 0xf0ffffff);

	 /* enable BT counter 0xda40[16,2] = 2b'11 */
        val = _read_cx_reg(btc, R_BTC_CSR_MODE);
        _write_cx_reg(btc, R_BTC_CSR_MODE, val | BIT16 | BIT2);
}

void _8852b_wl_pri (struct btc_t *btc, u8 map, bool state)
{
	u32 reg = 0, bitmap = 0, val = 0;

	switch (map) {
	case BTC_PRI_MASK_TX_RESP:
		reg = R_BTC_BT_COEX_MSK_TABLE;
		bitmap = B_BTC_PRI_MASK_TX_RESP_V1;
		break;
	case BTC_PRI_MASK_BEACON:
		reg = R_BTC_WL_PRI_MSK;
		bitmap = B_BTC_PTA_WL_PRI_MASK_BCNQ;
		break;
	case BTC_PRI_MASK_RX_CCK:
		reg = R_BTC_BT_COEX_MSK_TABLE;
		bitmap = B_BTC_PRI_MASK_RXCCK_V1;
		break;
	}

	val = _read_cx_reg(btc, reg);
	if (state)
		val = val | bitmap;
	else
		val = val & (~bitmap);

	_write_cx_reg(btc, reg, val);
}

#endif /* CONFIG_RTL8852B */
#endif


