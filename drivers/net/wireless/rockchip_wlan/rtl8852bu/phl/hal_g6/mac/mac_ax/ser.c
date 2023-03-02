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

#include "ser.h"

static void dump_err_status_dispatcher(struct mac_ax_adapter *adapter);
static void dump_err_status_dmac(struct mac_ax_adapter *adapter);
static void dump_err_status_cmac(struct mac_ax_adapter *adapter, u8 band);

u32 mac_trigger_cmac_err(struct mac_ax_adapter *adapter)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u8 val8;
	u16 val16;
	u32 ret;

	ret = check_mac_en(adapter, MAC_AX_BAND_0, MAC_AX_CMAC_SEL);
	if (ret != MACSUCCESS)
		return ret;

	val8 = MAC_REG_R8(R_AX_CMAC_FUNC_EN);
	MAC_REG_W8(R_AX_CMAC_FUNC_EN, val8 & (~B_AX_TMAC_EN));
	PLTFM_DELAY_MS(1);
	MAC_REG_W8(R_AX_CMAC_FUNC_EN, val8);

	val16 = MAC_REG_R16(R_AX_PTCL_IMR0) | B_AX_F2PCMD_EMPTY_ERR_INT_EN;
	MAC_REG_W16(R_AX_PTCL_IMR0, val16);
	MAC_REG_W16(R_AX_PTCL_IMR0, val16 & ~B_AX_F2PCMD_EMPTY_ERR_INT_EN);

	return MACSUCCESS;
}

u32 mac_trigger_cmac1_err(struct mac_ax_adapter *adapter)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u8 val8;
	u16 val16;
	u32 ret;

	ret = check_mac_en(adapter, MAC_AX_BAND_1, MAC_AX_CMAC_SEL);
	if (ret != MACSUCCESS)
		return ret;

	val8 = MAC_REG_R8(R_AX_CMAC_FUNC_EN_C1);
	MAC_REG_W8(R_AX_CMAC_FUNC_EN_C1, val8 & (~B_AX_TMAC_EN));
	PLTFM_DELAY_MS(1);
	MAC_REG_W8(R_AX_CMAC_FUNC_EN_C1, val8);

	val16 = MAC_REG_R16(R_AX_PTCL_IMR0_C1) | B_AX_F2PCMD_EMPTY_ERR_INT_EN;
	MAC_REG_W16(R_AX_PTCL_IMR0_C1, val16);
	MAC_REG_W16(R_AX_PTCL_IMR0_C1, val16 & ~B_AX_F2PCMD_EMPTY_ERR_INT_EN);

	return MACSUCCESS;
}

u32 mac_trigger_dmac_err(struct mac_ax_adapter *adapter)
{
	struct cpuio_buf_req_t buf_req;
	struct cpuio_ctrl_t ctrl_para;
	u32 ret;

	// Use CPUIO to enqueue packet.
	//WD
	buf_req.len = 0x20;
	ret = mac_dle_buf_req_wd(adapter, &buf_req);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]WDE DLE buf req\n");
		return ret;
	}

	// Enqueue two pkt_id, but only has one pkt_id.
	PLTFM_MEMSET((void *)&ctrl_para, 0, sizeof(ctrl_para));
	ctrl_para.cmd_type = CPUIO_OP_CMD_ENQ_TO_HEAD;
	ctrl_para.start_pktid = buf_req.pktid;
	ctrl_para.end_pktid = buf_req.pktid;
	ctrl_para.pkt_num = 1;
	ctrl_para.dst_pid = WDE_DLE_PORT_ID_WDRLS;
	ctrl_para.dst_qid = 4;
	ret = mac_set_cpuio_wd(adapter, &ctrl_para);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]WDE DLE enqueue to head\n");
		return ret;
	}
	return MACSUCCESS;
}

u32 mac_dump_err_status(struct mac_ax_adapter *adapter,
			enum mac_ax_err_info err)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	PLTFM_MSG_ERR("--->\n err=0x%x\n", err);
	if (err == MAC_AX_ERR_L1_ERR_DMAC ||
	    err == MAC_AX_ERR_L0_PROMOTE_TO_L1 ||
	    err == MAC_AX_ERR_L0_ERR_CMAC0 ||
	    err == MAC_AX_ERR_L0_ERR_CMAC1) {
		PLTFM_MSG_ERR("R_AX_SER_DBG_INFO =0x%08x\n",
			      MAC_REG_R32(R_AX_SER_DBG_INFO));

		dump_err_status_dmac(adapter);
		dump_err_status_cmac(adapter, MAC_AX_BAND_0);
		dump_err_status_cmac(adapter, MAC_AX_BAND_1);

		if (adapter->hw_info->intf == MAC_AX_INTF_PCIE) {
			PLTFM_MSG_ERR("R_AX_DBG_ERR_FLAG=0x%08x\n",
				      MAC_REG_R32(R_AX_DBG_ERR_FLAG));
			PLTFM_MSG_ERR("R_AX_LBC_WATCHDOG=0x%08x\n",
				      MAC_REG_R32(R_AX_LBC_WATCHDOG));
		}
	}
	PLTFM_MSG_ERR("<---\n");

	return MACSUCCESS;
}

u32 mac_set_err_status(struct mac_ax_adapter *adapter,
		       enum mac_ax_err_info err)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 cnt = MAC_SET_ERR_DLY_CNT;
	u32 ret = MACSUCCESS;

	if (err > MAC_AX_SET_ERR_MAX) {
		PLTFM_MSG_ERR("Bad set-err-status value\n");
		return MACFUNCINPUT;
	}

	PLTFM_MUTEX_LOCK(&adapter->hw_info->err_set_lock);

	while (--cnt) {
		if (!MAC_REG_R32(R_AX_HALT_H2C_CTRL))
			break;
		PLTFM_DELAY_US(MAC_SET_ERR_DLY_US);
	}
	if (!cnt) {
		PLTFM_MSG_ERR("FW does not receive previous msg\n");
		ret = MACPOLLTO;
		goto end;
	}

	if (err == MAC_AX_ERR_L1_DISABLE_EN)
		adapter->sm.fw_rst = MAC_AX_FW_RESET_RECV_DONE;

	MAC_REG_W32(R_AX_HALT_H2C, err);
	MAC_REG_W32(R_AX_HALT_H2C_CTRL, B_AX_HALT_H2C_TRIGGER);

end:
	PLTFM_MUTEX_UNLOCK(&adapter->hw_info->err_set_lock);
	return ret;
}

u32 mac_get_err_status(struct mac_ax_adapter *adapter,
		       enum mac_ax_err_info *err)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 cnt = MAC_SET_ERR_DLY_CNT;
	u32 ret = MACSUCCESS;
	u32 error_senario = 0;

	PLTFM_MUTEX_LOCK(&adapter->hw_info->err_get_lock);
	adapter->sm.l2_st = MAC_AX_L2_DIS;

	while (--cnt) {
		if (MAC_REG_R32(R_AX_HALT_C2H_CTRL))
			break;
		PLTFM_DELAY_US(MAC_SET_ERR_DLY_US);
	}
	if (!cnt) {
		PLTFM_MSG_ERR("Polling FW err status fail\n");
		ret = MACPOLLTO;
		goto end;
	}

	*err = (enum mac_ax_err_info)MAC_REG_R32(R_AX_HALT_C2H);
	MAC_REG_W32(R_AX_HALT_C2H_CTRL, 0);
	switch (*err) {
	case MAC_AX_ERR_L1_ERR_DMAC:
	case MAC_AX_ERR_L0_PROMOTE_TO_L1:
		adapter->sm.fw_rst = MAC_AX_FW_RESET_RECV;
		break;
	case MAC_AX_ERR_L1_RESET_DISABLE_DMAC_DONE:
		adapter->sm.fw_rst = MAC_AX_FW_RESET_PROCESS;
		break;
	case MAC_AX_ERR_L1_RESET_RECOVERY_DONE:
		adapter->sm.fw_rst = MAC_AX_FW_RESET_IDLE;
		break;
	default:
		break;
	}

	/* Decode the error status from halc2h */
	error_senario = (*err) >> DBG_SENARIO_SH;
	if (!(MAC_REG_R32(R_AX_UDM0) & (1 << B_AX_UDM0_DBG_MODE_SH))) {
		if (error_senario == CPU_EXCEPTION)
			*err = MAC_AX_ERR_CPU_EXCEPTION;
		else if (error_senario == ASSERTION)
			*err = MAC_AX_ERR_ASSERTION;
	}

	//3  3. Execute Recode Normal Debug Register
	if (*err == MAC_AX_ERR_L0_ERR_CMAC0 ||
	    *err == MAC_AX_ERR_L0_ERR_CMAC1) {
		pltfm_dbg_dump(adapter);
	}
	fw_st_dbg_dump(adapter);
	mac_dump_err_status(adapter, *err);
	if (*err == MAC_AX_ERR_L0_ERR_CMAC0 ||
	    *err == MAC_AX_ERR_L0_ERR_CMAC1) {
		pltfm_dbg_dump(adapter);
	}

	//3 4. Execute Recode Share memory debug information
	if (MAC_REG_R32(R_AX_UDM0) & (1 << B_AX_UDM0_DBG_MODE_SH)) {
		/* if debug mode =1 , dump share buffer */
		if (error_senario) {
			*err = (enum mac_ax_err_info)MAC_AX_DUMP_SHAREBUFF_INDICATOR;
			//notify phl to print share buffer
		}
	}
end:
	adapter->sm.l2_st = MAC_AX_L2_EN;
	PLTFM_MUTEX_UNLOCK(&adapter->hw_info->err_get_lock);
	return ret;
}

u32 mac_lv1_rcvy(struct mac_ax_adapter *adapter, enum mac_ax_lv1_rcvy_step step)
{
	u32 ret = MACSUCCESS;
#if MAC_AX_PCIE_SUPPORT
	u8 val8;
#endif

	switch (step) {
	case MAC_AX_LV1_RCVY_STEP_1:
		if (adapter->sm.fw_rst != MAC_AX_FW_RESET_RECV) {
			PLTFM_MSG_ERR("The rst-flow state is wrong\n");
			return MACPROCERR;
		}
#if MAC_AX_PCIE_SUPPORT
		if (adapter->hw_info->intf == MAC_AX_INTF_PCIE) {
			val8 = 0;
			ret = lv1rst_stop_dma_pcie(adapter, val8);
			if (ret) {
				PLTFM_MSG_ERR("lv1 rcvy pcie stop dma fail\n");
				return ret;
			}
		}
#endif

#if MAC_AX_USB_SUPPORT
		if (adapter->hw_info->intf == MAC_AX_INTF_USB) {
			ret = usb_flush_mode(adapter, MAC_AX_FUNC_EN);
			if (ret) {
				PLTFM_MSG_ERR("lv1 rcvy USB flush mode fail\n");
				return ret;
			}
			PLTFM_DELAY_MS(30);
		}
#endif
		break;

	case MAC_AX_LV1_RCVY_STEP_2:
		if (adapter->sm.fw_rst != MAC_AX_FW_RESET_PROCESS) {
			PLTFM_MSG_ERR("The rst-flow state is wrong\n");
			return MACPROCERR;
		}
#if MAC_AX_PCIE_SUPPORT
		if (adapter->hw_info->intf == MAC_AX_INTF_PCIE) {
			val8 = 0;
			ret = lv1rst_start_dma_pcie(adapter, val8);
			if (ret) {
				PLTFM_MSG_ERR("lv1 rcvy pcie start dma fail\n");
				return ret;
			}
		}
#endif

#if MAC_AX_USB_SUPPORT
		if (adapter->hw_info->intf == MAC_AX_INTF_USB) {
			ret = 0;
			ret = usb_flush_mode(adapter, MAC_AX_FUNC_DIS);
			if (ret) {
				PLTFM_MSG_ERR("lv1 rcvy USB norm mode fail\n");
				return ret;
			}
		}
#endif
		break;

	default:
		return MACLV1STEPERR;
	}

	return ret;
}

u32 mac_err_imr_ctrl(struct mac_ax_adapter *adapter, enum mac_ax_func_sw sw)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 v32_dmac, v32_cmac0, v32_cmac1;
	u32 ret = MACSUCCESS;
	u8 is_dbcc;

	v32_dmac = sw != MAC_AX_FUNC_DIS ? DMAC_ERR_IMR_EN : DMAC_ERR_IMR_DIS;
	v32_cmac0 = sw != MAC_AX_FUNC_DIS ? CMAC0_ERR_IMR_EN : CMAC0_ERR_IMR_DIS;
	v32_cmac1 = sw != MAC_AX_FUNC_DIS ? CMAC1_ERR_IMR_EN : CMAC1_ERR_IMR_DIS;
	is_dbcc = is_curr_dbcc(adapter);

#if MAC_AX_FW_REG_OFLD
	if (adapter->sm.fwdl == MAC_AX_FWDL_INIT_RDY) {
		ret = MAC_REG_W_OFLD((u16)R_AX_DMAC_ERR_IMR, DMAC_ERR_IMR_MASK,
				     v32_dmac, 0);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("dmac err imr w ofld fail\n");
			return ret;
		}

		ret = MAC_REG_W_OFLD((u16)R_AX_CMAC_ERR_IMR, CMAC0_ERR_IMR_MASK,
				     v32_cmac0, (is_dbcc ? 0 : 1));
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("cmac0 err imr w ofld fail\n");
			return ret;
		}

		if (is_dbcc) {
			ret = MAC_REG_W_OFLD((u16)R_AX_CMAC_ERR_IMR_C1,
					     CMAC1_ERR_IMR_MASK,
					     v32_cmac1, 1);
			if (ret != MACSUCCESS) {
				PLTFM_MSG_ERR("cmac1 err imr w ofld fail\n");
				return ret;
			}
		}

		return ret;
	}
#endif

	MAC_REG_W32(R_AX_DMAC_ERR_IMR, v32_dmac);
	MAC_REG_W32(R_AX_CMAC_ERR_IMR, v32_cmac0);

	if (is_dbcc)
		MAC_REG_W32(R_AX_CMAC_ERR_IMR_C1, v32_cmac1);

	return ret;
}

static void dump_err_status_dispatcher(struct mac_ax_adapter *adapter)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	PLTFM_MSG_ERR("R_AX_HOST_DISPATCHER_ERR_IMR=0x%08x\n",
		      MAC_REG_R32(R_AX_HOST_DISPATCHER_ERR_IMR));
	PLTFM_MSG_ERR("R_AX_HOST_DISPATCHER_ERR_ISR=0x%08x\n",
		      MAC_REG_R32(R_AX_HOST_DISPATCHER_ERR_ISR));

	PLTFM_MSG_ERR("R_AX_CPU_DISPATCHER_ERR_IMR=0x%08x\n",
		      MAC_REG_R32(R_AX_CPU_DISPATCHER_ERR_IMR));
	PLTFM_MSG_ERR("R_AX_CPU_DISPATCHER_ERR_ISR=0x%08x\n",
		      MAC_REG_R32(R_AX_CPU_DISPATCHER_ERR_ISR));
	PLTFM_MSG_ERR("R_AX_OTHER_DISPATCHER_ERR_IMR=0x%08x ",
		      MAC_REG_R32(R_AX_OTHER_DISPATCHER_ERR_IMR));
	PLTFM_MSG_ERR("R_AX_OTHER_DISPATCHER_ERR_ISR=0x%08x\n",
		      MAC_REG_R32(R_AX_OTHER_DISPATCHER_ERR_ISR));
}

static void dump_err_status_dmac(struct mac_ax_adapter *adapter)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 dmac_err;

	dmac_err = MAC_REG_R32(R_AX_DMAC_ERR_ISR);
	PLTFM_MSG_ERR("R_AX_DMAC_ERR_ISR =0x%08x\n", dmac_err);

	PLTFM_MSG_ERR("R_AX_DMAC_FUNC_EN =0x%08x\n",
		      MAC_REG_R32(R_AX_DMAC_FUNC_EN));
	PLTFM_MSG_ERR("R_AX_DMAC_CLK_EN =0x%08x\n",
		      MAC_REG_R32(R_AX_DMAC_CLK_EN));

	if (dmac_err) {
		PLTFM_MSG_ERR("R_AX_WDE_ERR_FLAG_CFG =0x%08x\n",
			      MAC_REG_R32(R_AX_WDE_ERR_FLAG_CFG));
		PLTFM_MSG_ERR("R_AX_PLE_ERR_FLAG_CFG =0x%08x\n",
			      MAC_REG_R32(R_AX_PLE_ERR_FLAG_CFG));
	}

	if (dmac_err & B_AX_WDRLS_ERR_FLAG) {
		PLTFM_MSG_ERR("R_AX_WDRLS_ERR_IMR =0x%08x\n",
			      MAC_REG_R32(R_AX_WDRLS_ERR_IMR));
		PLTFM_MSG_ERR("R_AX_WDRLS_ERR_ISR =0x%08x\n",
			      MAC_REG_R32(R_AX_WDRLS_ERR_ISR));
		PLTFM_MSG_ERR("R_AX_RPQ_RXBD_IDX =0x%08x\n",
			      MAC_REG_R32(R_AX_RPQ_RXBD_IDX));
	}

	if (dmac_err & B_AX_WSEC_ERR_FLAG) {
		PLTFM_MSG_ERR("R_AX_SEC_ERR_IMR_ISR =0x%08x\n",
			      MAC_REG_R32(R_AX_SEC_DEBUG));
		PLTFM_MSG_ERR("SEC_local_Register 0x9D00 =0x%08x\n",
			      MAC_REG_R32(R_AX_SEC_ENG_CTRL));
		PLTFM_MSG_ERR("SEC_local_Register 0x9D04 =0x%08x\n",
			      MAC_REG_R32(R_AX_SEC_MPDU_PROC));
		PLTFM_MSG_ERR("SEC_local_Register 0x9D10 =0x%08x\n",
			      MAC_REG_R32(R_AX_SEC_CAM_ACCESS));
		PLTFM_MSG_ERR("SEC_local_Register 0x9D14 =0x%08x\n",
			      MAC_REG_R32(R_AX_SEC_CAM_RDATA));
		PLTFM_MSG_ERR("SEC_local_Register 0x9D18 =0x%08x\n",
			      MAC_REG_R32(R_AX_SEC_CAM_WDATA));
		PLTFM_MSG_ERR("SEC_local_Register 0x9D20 =0x%08x\n",
			      MAC_REG_R32(R_AX_SEC_TX_DEBUG));
		PLTFM_MSG_ERR("SEC_local_Register 0x9D24 =0x%08x\n",
			      MAC_REG_R32(R_AX_SEC_RX_DEBUG));
		PLTFM_MSG_ERR("SEC_local_Register 0x9D28 =0x%08x\n",
			      MAC_REG_R32(R_AX_SEC_TRX_PKT_CNT));
		PLTFM_MSG_ERR("SEC_local_Register 0x9D2C =0x%08x\n",
			      MAC_REG_R32(R_AX_SEC_TRX_BLK_CNT));
	}

	if (dmac_err & B_AX_MPDU_ERR_FLAG) {
		PLTFM_MSG_ERR("R_AX_MPDU_TX_ERR_IMR =0x%08x\n",
			      MAC_REG_R32(R_AX_MPDU_TX_ERR_IMR));
		PLTFM_MSG_ERR("R_AX_MPDU_TX_ERR_ISR =0x%08x\n",
			      MAC_REG_R32(R_AX_MPDU_TX_ERR_ISR));
		PLTFM_MSG_ERR("R_AX_MPDU_RX_ERR_IMR =0x%08x\n",
			      MAC_REG_R32(R_AX_MPDU_RX_ERR_IMR));
		PLTFM_MSG_ERR("R_AX_MPDU_RX_ERR_ISR =0x%08x\n",
			      MAC_REG_R32(R_AX_MPDU_RX_ERR_ISR));
	}

	if (dmac_err & B_AX_STA_SCHEDULER_ERR_FLAG) {
		PLTFM_MSG_ERR("R_AX_STA_SCHEDULER_ERR_IMR =0x%08x\n",
			      MAC_REG_R32(R_AX_STA_SCHEDULER_ERR_IMR));
		PLTFM_MSG_ERR("R_AX_STA_SCHEDULER_ERR_ISR= 0x%08x\n",
			      MAC_REG_R32(R_AX_STA_SCHEDULER_ERR_ISR));
	}

	if (dmac_err & B_AX_WDE_DLE_ERR_FLAG) {
		PLTFM_MSG_ERR("R_AX_WDE_ERR_IMR=0x%08x\n",
			      MAC_REG_R32(R_AX_WDE_ERR_IMR));
		PLTFM_MSG_ERR("R_AX_WDE_ERR_ISR=0x%08x\n",
			      MAC_REG_R32(R_AX_WDE_ERR_ISR));
		PLTFM_MSG_ERR("R_AX_PLE_ERR_IMR=0x%08x\n",
			      MAC_REG_R32(R_AX_PLE_ERR_IMR));
		PLTFM_MSG_ERR("R_AX_PLE_ERR_FLAG_ISR=0x%08x\n",
			      MAC_REG_R32(R_AX_PLE_ERR_FLAG_ISR));
	}

	if (dmac_err & B_AX_TXPKTCTRL_ERR_FLAG) {
		PLTFM_MSG_ERR("R_AX_TXPKTCTL_ERR_IMR_ISR=0x%08x\n",
			      MAC_REG_R32(R_AX_TXPKTCTL_ERR_IMR_ISR));
		PLTFM_MSG_ERR("R_AX_TXPKTCTL_ERR_IMR_ISR_B1=0x%08x\n",
			      MAC_REG_R32(R_AX_TXPKTCTL_ERR_IMR_ISR_B1));
	}

	if (dmac_err & B_AX_PLE_DLE_ERR_FLAG) {
		PLTFM_MSG_ERR("R_AX_WDE_ERR_IMR=0x%08x\n",
			      MAC_REG_R32(R_AX_WDE_ERR_IMR));
		PLTFM_MSG_ERR("R_AX_WDE_ERR_ISR=0x%08x\n",
			      MAC_REG_R32(R_AX_WDE_ERR_ISR));
		PLTFM_MSG_ERR("R_AX_PLE_ERR_IMR=0x%08x\n",
			      MAC_REG_R32(R_AX_PLE_ERR_IMR));
		PLTFM_MSG_ERR("R_AX_PLE_ERR_FLAG_ISR=0x%08x\n",
			      MAC_REG_R32(R_AX_PLE_ERR_FLAG_ISR));
		PLTFM_MSG_ERR("R_AX_WD_CPUQ_OP_0=0x%08x\n",
			      MAC_REG_R32(R_AX_WD_CPUQ_OP_0));
		PLTFM_MSG_ERR("R_AX_WD_CPUQ_OP_1=0x%08x\n",
			      MAC_REG_R32(R_AX_WD_CPUQ_OP_1));
		PLTFM_MSG_ERR("R_AX_WD_CPUQ_OP_2=0x%08x\n",
			      MAC_REG_R32(R_AX_WD_CPUQ_OP_2));
		PLTFM_MSG_ERR("R_AX_WD_CPUQ_OP_STATUS=0x%08x\n",
			      MAC_REG_R32(R_AX_WD_CPUQ_OP_STATUS));
		PLTFM_MSG_ERR("R_AX_PL_CPUQ_OP_0=0x%08x\n",
			      MAC_REG_R32(R_AX_PL_CPUQ_OP_0));
		PLTFM_MSG_ERR("R_AX_PL_CPUQ_OP_1=0x%08x\n",
			      MAC_REG_R32(R_AX_PL_CPUQ_OP_1));
		PLTFM_MSG_ERR("R_AX_PL_CPUQ_OP_2=0x%08x\n",
			      MAC_REG_R32(R_AX_PL_CPUQ_OP_2));
		PLTFM_MSG_ERR("R_AX_PL_CPUQ_OP_STATUS=0x%08x\n",
			      MAC_REG_R32(R_AX_PL_CPUQ_OP_STATUS));
		PLTFM_MSG_ERR("R_AX_RXDMA_PKT_INFO_0=0x%08x\n",
			      MAC_REG_R32(R_AX_RXDMA_PKT_INFO_0));
		PLTFM_MSG_ERR("R_AX_RXDMA_PKT_INFO_1=0x%08x\n",
			      MAC_REG_R32(R_AX_RXDMA_PKT_INFO_1));
		PLTFM_MSG_ERR("R_AX_RXDMA_PKT_INFO_2=0x%08x\n",
			      MAC_REG_R32(R_AX_RXDMA_PKT_INFO_2));
		dump_err_status_dispatcher(adapter);
	}

	if (dmac_err & B_AX_PKTIN_ERR_FLAG) {
		PLTFM_MSG_ERR("R_AX_PKTIN_ERR_IMR =0x%08x\n",
			      MAC_REG_R32(R_AX_PKTIN_ERR_IMR));
		PLTFM_MSG_ERR("R_AX_PKTIN_ERR_ISR =0x%08x\n",
			      MAC_REG_R32(R_AX_PKTIN_ERR_ISR));
		PLTFM_MSG_ERR("R_AX_PKTIN_ERR_IMR =0x%08x ",
			      MAC_REG_R32(R_AX_PKTIN_ERR_IMR));
		PLTFM_MSG_ERR("R_AX_PKTIN_ERR_ISR =0x%08x\n",
			      MAC_REG_R32(R_AX_PKTIN_ERR_ISR));
	}

	if (dmac_err & B_AX_DISPATCH_ERR_FLAG)
		dump_err_status_dispatcher(adapter);

	if (dmac_err & B_AX_DLE_CPUIO_ERR_FLAG) {
		PLTFM_MSG_ERR("R_AX_CPUIO_ERR_IMR=0x%08x\n",
			      MAC_REG_R32(R_AX_CPUIO_ERR_IMR));
		PLTFM_MSG_ERR("R_AX_CPUIO_ERR_ISR=0x%08x\n",
			      MAC_REG_R32(R_AX_CPUIO_ERR_ISR));
	}

	if (dmac_err & BIT(11)) {
		PLTFM_MSG_ERR("R_AX_BBRPT_COM_ERR_IMR_ISR=0x%08x\n",
			      MAC_REG_R32(R_AX_BBRPT_COM_ERR_IMR_ISR));
	}
}

static void dump_err_status_cmac(struct mac_ax_adapter *adapter, u8 band)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 cmac_err;
	u32 ret;

	ret = check_mac_en(adapter, band, MAC_AX_CMAC_SEL);
	if (ret != MACSUCCESS)
		return;

	PLTFM_MSG_ERR("CMAC Band =0x%02x\n", band);

	cmac_err = MAC_REG_R32(band == MAC_AX_BAND_0 ? R_AX_CMAC_ERR_ISR :
			       R_AX_CMAC_ERR_ISR_C1);
	PLTFM_MSG_ERR("R_AX_CMAC_ERR_ISR =0x%08x\n", cmac_err);

	PLTFM_MSG_ERR("R_AX_CMAC_FUNC_EN =0x%08x\n",
		      MAC_REG_R32(band == MAC_AX_BAND_0 ? R_AX_CMAC_FUNC_EN :
		      R_AX_CMAC_FUNC_EN_C1));
	PLTFM_MSG_ERR("R_AX_CK_EN =0x%08x\n",
		      MAC_REG_R32(band == MAC_AX_BAND_0 ? R_AX_CK_EN :
		      R_AX_CK_EN_C1));

	if (cmac_err & B_AX_SCHEDULE_TOP_ERR_IND) {
		PLTFM_MSG_ERR("R_AX_SCHEDULE_ERR_IMR=0x%08x\n",
			      MAC_REG_R32(band == MAC_AX_BAND_0 ?
			      R_AX_SCHEDULE_ERR_IMR : R_AX_SCHEDULE_ERR_IMR_C1));
		PLTFM_MSG_ERR("R_AX_SCHEDULE_ERR_ISR=0x%04x\n",
			      MAC_REG_R16(band == MAC_AX_BAND_0 ?
			      R_AX_SCHEDULE_ERR_ISR : R_AX_SCHEDULE_ERR_ISR_C1));
	}

	if (cmac_err & B_AX_PTCL_TOP_ERR_IND) {
		PLTFM_MSG_ERR("R_AX_PTCL_IMR0=0x%08x\n",
			      MAC_REG_R32(band == MAC_AX_BAND_0 ?
			      R_AX_PTCL_IMR0 : R_AX_PTCL_IMR0_C1));
		PLTFM_MSG_ERR("R_AX_PTCL_ISR0=0x%08x\n",
			      MAC_REG_R32(band == MAC_AX_BAND_0 ?
			      R_AX_PTCL_ISR0 : R_AX_PTCL_ISR0_C1));
	}

	if (cmac_err & B_AX_DMA_TOP_ERR_IND) {
		PLTFM_MSG_ERR("R_AX_DLE_CTRL=0x%08x\n",
			      MAC_REG_R32(band == MAC_AX_BAND_0 ?
			      R_AX_DLE_CTRL : R_AX_DLE_CTRL_C1));
	}

	if (cmac_err & B_AX_PHYINTF_ERR_IND) {
		PLTFM_MSG_ERR("R_AX_PHYINFO_ERR_IMR=0x%04x\n",
			      MAC_REG_R16(band == MAC_AX_BAND_0 ?
			      R_AX_PHYINFO_ERR_IMR : R_AX_PHYINFO_ERR_IMR_C1));
	}

	if (cmac_err & B_AX_TXPWR_CTRL_ERR_IND) {
		PLTFM_MSG_ERR("R_AX_TXPWR_IMR=0x%08x\n",
			      MAC_REG_R32(band == MAC_AX_BAND_0 ?
			      R_AX_TXPWR_IMR : R_AX_TXPWR_IMR_C1));
		PLTFM_MSG_ERR("R_AX_TXPWR_ISR=0x%08x\n",
			      MAC_REG_R32(band == MAC_AX_BAND_0 ?
			      R_AX_TXPWR_ISR : R_AX_TXPWR_ISR_C1));
	}

	if (cmac_err & B_AX_WMAC_RX_ERR_IND) {
		PLTFM_MSG_ERR("R_AX_DBGSEL_TRXPTCL=0x%08x\n",
			      MAC_REG_R32(band == MAC_AX_BAND_0 ?
			      R_AX_DBGSEL_TRXPTCL : R_AX_DBGSEL_TRXPTCL_C1));
		PLTFM_MSG_ERR("R_AX_PHYINFO_ERR_ISR=0x%08x\n",
			      MAC_REG_R32(band == MAC_AX_BAND_0 ?
			      R_AX_PHYINFO_ERR_ISR : R_AX_PHYINFO_ERR_ISR_C1));
	}

	if (cmac_err & B_AX_WMAC_TX_ERR_IND) {
		PLTFM_MSG_ERR("R_AX_TMAC_ERR_IMR_ISR=0x%08x\n",
			      MAC_REG_R32(band == MAC_AX_BAND_0 ?
			      R_AX_TMAC_ERR_IMR_ISR : R_AX_TMAC_ERR_IMR_ISR_C1));
		PLTFM_MSG_ERR("R_AX_DBGSEL_TRXPTCL=0x%08x\n",
			      MAC_REG_R32(band == MAC_AX_BAND_0 ?
			      R_AX_DBGSEL_TRXPTCL : R_AX_DBGSEL_TRXPTCL_C1));
	}
}

u32 mac_ser_ctrl(struct mac_ax_adapter *adapter, enum mac_ax_func_sw sw)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	enum mac_ax_err_info err_info;
	u32 val32, ret, cnt;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B) && is_cv(adapter, CAV)) {
		PLTFM_MSG_WARN("[WARN]SER ctrl not support\n");
		return MACSUCCESS;
	}

	if (sw == MAC_AX_FUNC_EN) {
		err_info = MAC_AX_ERR_L1_RCVY_START_REQ;
		adapter->sm.ser_ctrl_st = MAC_AX_SER_CTRL_SRT;
	} else if (sw == MAC_AX_FUNC_DIS) {
		err_info = MAC_AX_ERR_L1_RCVY_STOP_REQ;
		adapter->sm.ser_ctrl_st = MAC_AX_SER_CTRL_STOP;
	} else {
		adapter->sm.ser_ctrl_st = MAC_AX_SER_CTRL_ERR;
		PLTFM_MSG_ERR("[ERR]SER ctrl input err %d\n", sw);
		return MACFUNCINPUT;
	}

	ret = mac_set_err_status(adapter, err_info);
	if (ret != MACSUCCESS) {
		adapter->sm.ser_ctrl_st = MAC_AX_SER_CTRL_ERR;
		PLTFM_MSG_ERR("[ERR]set err for stop ser %d\n", ret);
		return ret;
	}

	cnt = MAC_SET_ERR_DLY_CNT;
	while (cnt) {
		val32 = MAC_REG_R32(R_AX_HALT_H2C_CTRL);
		if (!(val32 & B_AX_HALT_H2C_TRIGGER))
			break;
		PLTFM_DELAY_US(MAC_SET_ERR_DLY_US);
		cnt--;
	}

	if (!cnt) {
		adapter->sm.ser_ctrl_st = MAC_AX_SER_CTRL_ERR;
		PLTFM_MSG_ERR("[ERR]FW not handle haltH2C req\n");
		ret = MACPOLLTO;
		return ret;
	}

	if (sw == MAC_AX_FUNC_EN)
		return MACSUCCESS;

	cnt = MAC_SER_STOP_DLY_CNT;
	while (cnt) {
		PLTFM_DELAY_US(MAC_SER_STOP_DLY_US);
		val32 = MAC_REG_R32(R_AX_UDM0);
		val32 = GET_FIELD(val32, FW_ST);
		if (val32 != FW_ST_ERR_IN)
			break;
		cnt--;
	}

	if (!cnt) {
		adapter->sm.ser_ctrl_st = MAC_AX_SER_CTRL_ERR;
		PLTFM_MSG_ERR("[ERR]stop ser polling FW ST timeout\n");
		return MACPOLLTO;
	}

	return ret;
}
