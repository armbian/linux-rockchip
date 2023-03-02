/******************************************************************************
 *
 * Copyright(c) 2019 Realtek Corporation. All rights reserved.
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
 ******************************************************************************/

#include "dbcc.h"
#include "cpuio.h"

static u32 band1_enable(struct mac_ax_adapter *adapter,
			struct mac_ax_trx_info *info)
{
	u32 ret;
	u32 sleep_bak[4] = {0};
	u32 pause_bak[4] = {0};
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct mac_ax_sch_tx_en_cfg txen_bak;

	txen_bak.band = 0;
	ret = stop_sch_tx(adapter, SCH_TX_SEL_ALL, &txen_bak);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]stop sch tx %d\n", ret);
		return ret;
	}

	sleep_bak[0] = MAC_REG_R32(R_AX_MACID_SLEEP_0);
	sleep_bak[1] = MAC_REG_R32(R_AX_MACID_SLEEP_1);
	sleep_bak[2] = MAC_REG_R32(R_AX_MACID_SLEEP_2);
	sleep_bak[3] = MAC_REG_R32(R_AX_MACID_SLEEP_3);
	pause_bak[0] = MAC_REG_R32(R_AX_SS_MACID_PAUSE_0);
	pause_bak[1] = MAC_REG_R32(R_AX_SS_MACID_PAUSE_1);
	pause_bak[2] = MAC_REG_R32(R_AX_SS_MACID_PAUSE_2);
	pause_bak[3] = MAC_REG_R32(R_AX_SS_MACID_PAUSE_3);

	MAC_REG_W32(R_AX_MACID_SLEEP_0, 0xFFFFFFFF);
	MAC_REG_W32(R_AX_MACID_SLEEP_1, 0xFFFFFFFF);
	MAC_REG_W32(R_AX_MACID_SLEEP_2, 0xFFFFFFFF);
	MAC_REG_W32(R_AX_MACID_SLEEP_3, 0xFFFFFFFF);
	MAC_REG_W32(R_AX_SS_MACID_PAUSE_0, 0xFFFFFFFF);
	MAC_REG_W32(R_AX_SS_MACID_PAUSE_1, 0xFFFFFFFF);
	MAC_REG_W32(R_AX_SS_MACID_PAUSE_2, 0xFFFFFFFF);
	MAC_REG_W32(R_AX_SS_MACID_PAUSE_3, 0xFFFFFFFF);

	ret = tx_idle_poll_band(adapter, 0, 0);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]tx idle poll %d\n", ret);
		return ret;
	}

	ret = dle_quota_change(adapter, info->qta_mode);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]DLE quota change %d\n", ret);
		return ret;
	}

	MAC_REG_W32(R_AX_MACID_SLEEP_0, sleep_bak[0]);
	MAC_REG_W32(R_AX_MACID_SLEEP_1, sleep_bak[1]);
	MAC_REG_W32(R_AX_MACID_SLEEP_2, sleep_bak[2]);
	MAC_REG_W32(R_AX_MACID_SLEEP_3, sleep_bak[3]);
	MAC_REG_W32(R_AX_SS_MACID_PAUSE_0, pause_bak[0]);
	MAC_REG_W32(R_AX_SS_MACID_PAUSE_1, pause_bak[1]);
	MAC_REG_W32(R_AX_SS_MACID_PAUSE_2, pause_bak[2]);
	MAC_REG_W32(R_AX_SS_MACID_PAUSE_3, pause_bak[3]);

	ret = resume_sch_tx(adapter, &txen_bak);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]CMAC%d resume sch tx %d\n",
			      txen_bak.band, ret);
		return ret;
	}

	ret = cmac_func_en(adapter, MAC_AX_BAND_1, MAC_AX_FUNC_EN);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]CMAC%d func en %d\n", MAC_AX_BAND_1, ret);
		return ret;
	}

	ret = cmac_init(adapter, info, MAC_AX_BAND_1);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]CMAC%d init %d\n", MAC_AX_BAND_1, ret);
		return ret;
	}

	MAC_REG_W32(R_AX_SYS_ISO_CTRL_EXTEND,
		    MAC_REG_R32(R_AX_SYS_ISO_CTRL_EXTEND) |
		    (B_AX_R_SYM_FEN_WLBBFUN_1 | B_AX_R_SYM_FEN_WLBBGLB_1));

	adapter->sm.bb1_func = MAC_AX_FUNC_ON;

	ret = mac_enable_imr(adapter, MAC_AX_BAND_1, MAC_AX_CMAC_SEL);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR] enable CMAC1 IMR %d\n", ret);
		return ret;
	}

	return MACSUCCESS;
}

static u32 band1_disable(struct mac_ax_adapter *adapter,
			 struct mac_ax_trx_info *info)
{
	u32 ret;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct mac_ax_pkt_drop_info drop_info;

	MAC_REG_W32(R_AX_SYS_ISO_CTRL_EXTEND,
		    MAC_REG_R32(R_AX_SYS_ISO_CTRL_EXTEND) &
		    ~B_AX_R_SYM_FEN_WLBBFUN_1);

	drop_info.band = MAC_AX_BAND_1;
	drop_info.sel = MAC_AX_PKT_DROP_SEL_BAND;
	ret = adapter->ops->pkt_drop(adapter, &drop_info);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]CMAC%d pkt drop %d\n", drop_info.band, ret);
		return ret;
	}

	ret = mac_remove_role_by_band(adapter, MAC_AX_BAND_1, 0);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]Remove Address CAM %d\n", ret);
		return ret;
	}

	ret = cmac_func_en(adapter, MAC_AX_BAND_1, MAC_AX_FUNC_DIS);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]CMAC%d func dis %d\n", MAC_AX_BAND_1, ret);
		return ret;
	}

	ret = dle_quota_change(adapter, info->qta_mode);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]DLE quota change %d\n", ret);
		return ret;
	}

	return 0;
}

u32 mac_dbcc_enable(struct mac_ax_adapter *adapter,
		    struct mac_ax_trx_info *info, u8 dbcc_en)
{
	u32 ret;

	if (dbcc_en) {
		ret = band1_enable(adapter, info);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR] band1_enable %d\n", ret);
			return ret;
		}
		ret = mac_notify_fw_dbcc(adapter, 1);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("%s: [ERR] notfify dbcc fail %d\n",
				      __func__, ret);
			return ret;
		}
	} else {
		ret = mac_notify_fw_dbcc(adapter, 0);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("%s: [ERR] notfify dbcc fail %d\n",
				      __func__, ret);
			return ret;
		}
		ret = band1_disable(adapter, info);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR] band1_disable %d\n", ret);
			return ret;
		}
	}

	return MACSUCCESS;
}

u32 mac_dbcc_move_macid(struct mac_ax_adapter *adapter, u8 macid, u8 dbcc_en)
{
	struct mac_role_tbl *role;
	struct deq_enq_info q_info;
	u32 ret;

	role = mac_role_srch(adapter, macid);
	if (!role) {
		PLTFM_MSG_ERR("dbcc%d move macid%d srch role fail\n",
			      dbcc_en, macid);
		return MACNOITEM;
	}

	if (dbcc_en) {
		ret = mac_ss_wmm_sta_move(adapter,
					  (enum mac_ax_ss_wmm)role->wmm,
					  MAC_AX_SS_WMM_TBL_C1_WMM0);
	} else {
		ret = mac_ss_wmm_sta_move(adapter,
					  (enum mac_ax_ss_wmm)role->wmm,
					  MAC_AX_SS_WMM_TBL_C0_WMM0);
	}

	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("dbcc%d move macid%d wmm%d move %d\n",
			      dbcc_en, macid, role->wmm, ret);
		return ret;
	}

	PLTFM_MEMSET(&q_info, 0, sizeof(struct deq_enq_info));

	if (dbcc_en) {
		q_info.src_pid = WDE_DLE_PID_C0;
		q_info.src_qid = WDE_DLE_QID_MG0_C0;
		q_info.dst_pid = WDE_DLE_PID_C1;
		q_info.dst_qid = WDE_DLE_QID_MG0_C1;
	} else {
		q_info.src_pid = WDE_DLE_PID_C1;
		q_info.src_qid = WDE_DLE_QID_MG0_C1;
		q_info.dst_pid = WDE_DLE_PID_C0;
		q_info.dst_qid = WDE_DLE_QID_MG0_C0;
	}

	ret = deq_enq_all(adapter, &q_info);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("dbcc%d move mgq %d\n", dbcc_en, ret);
		return ret;
	}

	return MACSUCCESS;
}

