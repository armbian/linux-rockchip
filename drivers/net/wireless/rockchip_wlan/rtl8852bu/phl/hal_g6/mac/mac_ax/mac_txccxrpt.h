#ifndef _MAC_TXCCXRPT_H_
#define _MAC_TXCCXRPT_H_

/* dword0 */
#define TXCCXRPT_RPT_SEL_SH		0
#define TXCCXRPT_RPT_SEL_MSK		0x1f
#define TXCCXRPT_POLLUTED		BIT(5)
#define TXCCXRPT_TX_STATE_SH		6
#define TXCCXRPT_TX_STATE_MSK		0x3
#define TXCCXRPT_SW_DEFINE_SH		8
#define TXCCXRPT_SW_DEFINE_MSK		0xf
#define TXCCXRPT_TRY_RATE		BIT(14)
#define TXCCXRPT_FIXRATE		BIT(15)
#define TXCCXRPT_MACID_SH		16
#define TXCCXRPT_MACID_MSK		0x7f
#define TXCCXRPT_QSEL_SH		24
#define TXCCXRPT_QSEL_MSK		0x3f
#define TXCCXRPT_TXOP_START		BIT(31)

/* dword1 */
#define TXCCXRPT_QUEUE_TIME_SH		0
#define TXCCXRPT_QUEUE_TIME_MSK		0xffff
#define TXCCXRPT_ACCTXTIME_SH		16
#define TXCCXRPT_ACCTXTIME_MSK		0xff
#define TXCCXRPT_BMC		BIT(29)
#define TXCCXRPT_BITMAP_SHORT_SH		30
#define TXCCXRPT_BITMAP_SHORT_MSK		0x3

/* dword2 */
#define TXCCXRPT_FINAL_RATE_SH		0
#define TXCCXRPT_FINAL_RATE_MSK		0x1ff
#define TXCCXRPT_FINAL_GI_LTF_SH		9
#define TXCCXRPT_FINAL_GI_LTF_MSK		0x7
#define TXCCXRPT_DATA_BW_SH		12
#define TXCCXRPT_DATA_BW_MSK		0x3
#define TXCCXRPT_MU2SU		BIT(14)
#define TXCCXRPT_MU_LMT		BIT(15)
#define TXCCXRPT_FINAL_RTS_RATE_SH		16
#define TXCCXRPT_FINAL_RTS_RATE_MSK		0x1ff
#define TXCCXRPT_FINAL_RTS_GI_LTF_SH		25
#define TXCCXRPT_FINAL_RTS_GI_LTF_MSK		0x7
#define TXCCXRPT_RTS_TX_STATE_SH		28
#define TXCCXRPT_RTS_TX_STATE_MSK		0x3
#define TXCCXRPT_COLLISION_HEAD		BIT(30)
#define TXCCXRPT_COLLISION_TAIL		BIT(31)

/* dword3 */
#define TXCCXRPT_TOTAL_PKT_NUM_SH		0
#define TXCCXRPT_TOTAL_PKT_NUM_MSK		0xff
#define TXCCXRPT_DATA_TX_CNT_SH		8
#define TXCCXRPT_DATA_TX_CNT_MSK		0x3f
#define TXCCXRPT_BPRI		BIT(14)
#define TXCCXRPT_BBAR		BIT(15)
#define TXCCXRPT_PKT_OK_NUM_SH		16
#define TXCCXRPT_PKT_OK_NUM_MSK		0xff
#define TXCCXRPT_RTS_TX_COUNT_SH		24
#define TXCCXRPT_RTS_TX_COUNT_MSK		0x3f

/* dword4 */
#define TXCCXRPT_INIT_RATE_SH		0
#define TXCCXRPT_INIT_RATE_MSK		0x1ff
#define TXCCXRPT_INIT_GI_LTF_SH		9
#define TXCCXRPT_INIT_GI_LTF_MSK		0x7
#define TXCCXRPT_PPDU_TYPE_SH		12
#define TXCCXRPT_PPDU_TYPE_MSK		0x3
#define TXCCXRPT_HE_TB_PPDU_FLAG		BIT(14)
#define TXCCXRPT_PPDU_FST_RPT		BIT(15)
#define TXCCXRPT_SU_TXPWR_SH		16
#define TXCCXRPT_SU_TXPWR_MSK		0x3f
#define TXCCXRPT_DIFF_PKT_NUM_SH		24
#define TXCCXRPT_DIFF_PKT_NUM_MSK		0xf
#define TXCCXRPT_USER_DEFINE_EXT_L_SH		28
#define TXCCXRPT_USER_DEFINE_EXT_L_MSK		0xf

/* dword5 */
#define TXCCXRPT_USER_DEFINE_SH		0
#define TXCCXRPT_USER_DEFINE_MSK		0xff
#define TXCCXRPT_FW_DEFINE_SH		8
#define TXCCXRPT_FW_DEFINE_MSK		0xff
#define TXCCXRPT_TXPWR_PD_SH		16
#define TXCCXRPT_TXPWR_PD_MSK		0x1f
#define TXCCXRPT_BSR		BIT(21)
#define TXCCXRPT_SR_RX_COUNT_SH		24
#define TXCCXRPT_SR_RX_COUNT_MSK		0xf
#define TXCCXRPT_USER_DEFINE_EXT_H_SH		28
#define TXCCXRPT_USER_DEFINE_EXT_H_MSK		0xf

struct mac_ccxrpt {
	/* dword 0 */
	u32 rpt_sel:5;
	u32 polluted:1;
	u32 tx_state:2;
	u32 sw_define:4;
	u32 rsvd0:2;
	u32 try_rate:1;
	u32 fixrate:1;
	u32 macid:7;
	u32 rsvd1:1;
	u32 qsel:6;
	u32 rsvd2:1;
	u32 txop_start:1;
	/* dword 1 */
	u32 queue_time:16;
	u32 acctxtime:8;
	u32 rsvd3:5;
	u32 bmc:1;
	u32 bitmap_short:2;
	/* dword 2 */
	u32 final_rate:9;
	u32 final_gi_ltf:3;
	u32 data_bw:2;
	u32 mu2su:1;
	u32 mu_lmt:1;
	u32 final_rts_rate:9;
	u32 final_rts_gi_ltf:3;
	u32 rts_tx_state:2;
	u32 collision_head:1;
	u32 collision_tail:1;
	/* dword 3 */
	u32 total_pkt_num:8;
	u32 data_tx_cnt:6;
	u32 bpri:1;
	u32 bbar:1;
	u32 pkt_ok_num:8;
	u32 rts_tx_count:6;
	u32 rsvd4:2;
	/* dword 4 */
	u32 init_rate:9;
	u32 init_gi_ltf:3;
	u32 ppdu_type:2;
	u32 he_tb_ppdu_flag:1;
	u32 ppdu_fst_rpt:1;
	u32 su_txpwr:6;
	u32 rsvd5:2;
	u32 diff_pkt_num:4;
	u32 user_define_ext_l:4;
	/* dword 5 */
	u32 user_define:8;
	u32 fw_define:8;
	u32 txpwr_pd:5;
	u32 bsr:1;
	u32 rsvd6:2;
	u32 sr_rx_count:4;
	u32 user_define_ext_h:4;
};
#endif
