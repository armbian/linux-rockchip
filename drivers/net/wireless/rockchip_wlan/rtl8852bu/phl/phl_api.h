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
#ifndef _PHL_API_H_
#define _PHL_API_H_

u8 rtw_phl_read8(void *phl, u32 addr);
u16 rtw_phl_read16(void *phl, u32 addr);
u32 rtw_phl_read32(void *phl, u32 addr);
void rtw_phl_write8(void *phl, u32 addr, u8 val);
void rtw_phl_write16(void *phl, u32 addr, u16 val);
void rtw_phl_write32(void *phl, u32 addr, u32 val);

u32 rtw_phl_read_macreg(void *phl, u32 offset, u32 bit_mask);
void rtw_phl_write_macreg(void *phl,
			u32 offset, u32 bit_mask, u32 data);
u32 rtw_phl_read_bbreg(void *phl, u32 offset, u32 bit_mask);
void rtw_phl_write_bbreg(void *phl,
			u32 offset, u32 bit_mask, u32 data);
u32 rtw_phl_read_rfreg(void *phl,
			enum rf_path path, u32 offset, u32 bit_mask);
void rtw_phl_write_rfreg(void *phl,
			enum rf_path path, u32 offset, u32 bit_mask, u32 data);
enum rtw_phl_status rtw_phl_interrupt_handler(void *phl);
void rtw_phl_enable_interrupt(void *phl);
void rtw_phl_disable_interrupt(void *phl);
bool rtw_phl_recognize_interrupt(void *phl);
void rtw_phl_clear_interrupt(void *phl);
void rtw_phl_restore_interrupt(void *phl);


#ifdef PHL_PLATFORM_LINUX
void rtw_phl_mac_reg_dump(void *sel, void *phl);
void rtw_phl_bb_reg_dump(void *sel, void *phl);
void rtw_phl_bb_reg_dump_ex(void *sel, void *phl);
void rtw_phl_rf_reg_dump(void *sel, void *phl);
#endif
bool rtw_phl_get_sec_cam(void *phl, u16 num, u8 *buf, u16 size);
bool rtw_phl_get_addr_cam(void *phl, u16 num, u8 *buf, u16 size);

struct rtw_phl_com_t *rtw_phl_get_com(void *phl);
enum rtw_phl_status rtw_phl_init(void *drv_priv, void **phl,
					struct rtw_ic_info *ic_info);
void rtw_phl_deinit(void *phl);

void rtw_phl_watchdog_init(void *phl,
                           u16 period,
                           void (*core_sw_wdog)(void *drv_priv),
                           void (*core_hw_wdog)(void *drv_priv));
void rtw_phl_watchdog_deinit(void *phl);
void rtw_phl_watchdog_start(void *phl);
void rtw_phl_watchdog_stop(void *phl);

enum rtw_phl_status rtw_phl_trx_alloc(void *phl);
void rtw_phl_trx_free(void *phl);
void rtw_phl_trx_free_handler(void *phl);
void rtw_phl_trx_free_sw_rsc(void *phl);
void rtw_phl_cap_pre_config(void *phl);
enum rtw_phl_status rtw_phl_preload(void *phl);
enum rtw_phl_status rtw_phl_start(void *phl);
void rtw_phl_stop(void *phl);
bool rtw_phl_is_init_completed(void *phl);

void rtw_phl_ps_set_rt_cap(void *phl, u8 band_idx, bool ps_allow, enum phl_ps_rt_rson rt_rson);
enum rtw_phl_status rtw_phl_ps_set_rf_state(void *phl, u8 band_idx, enum rtw_rf_state rf_state);

enum rtw_phl_status rtw_phl_suspend(void *phl, struct rtw_phl_stainfo_t *sta, u8 wow_en);
enum rtw_phl_status rtw_phl_resume(void *phl, struct rtw_phl_stainfo_t *sta, u8 *hw_reinit);

enum rtw_phl_status rtw_phl_tx_req_notify(void *phl);
enum rtw_phl_status rtw_phl_add_tx_req(void *phl, struct rtw_xmit_req *tx_req);
void rtw_phl_tx_stop(void *phl);
void rtw_phl_tx_resume(void *phl);
u16 rtw_phl_tring_rsc(void *phl, u16 macid, u8 tid);
u16 rtw_phl_query_new_rx_num(void *phl);
struct rtw_recv_pkt *rtw_phl_query_rx_pkt(void *phl);

void rtw_phl_rx_deferred_In_token(void *phl);
void rtw_phl_post_in_complete(void *phl, void *rxobj, u32 inbuf_len, u8 status_code);
enum rtw_phl_status rtw_phl_return_rxbuf(void *phl, u8* rxpkt);

enum rtw_phl_status  rtw_phl_recycle_tx_buf(void *phl, u8 *tx_buf_ptr);

enum rtw_phl_status
rtw_phl_cmd_cfg_ampdu(void *phl,
			struct rtw_wifi_role_t *wrole,
			struct rtw_phl_stainfo_t *sta,
			enum phl_cmd_type cmd_type,
			u32 cmd_timeout);

void rtw_phl_proc_cmd(void *phl, char proc_cmd,
	struct rtw_proc_cmd *incmd, char *output, u32 out_len);

void rtw_phl_get_fw_ver(void *phl, char *ver_str, u16 len);

enum rtw_fw_status rtw_phl_get_fw_status(void *phl);

/* command thread jobs */
enum rtw_phl_status rtw_phl_job_run_func(void *phl,
	void *func, void *priv, void *parm, char *name);

/*WIFI Role management section*/
u8 rtw_phl_wifi_role_alloc(void *phl, u8 *mac_addr, enum role_type type,
				u8 ridx, struct rtw_wifi_role_t **wifi_role, bool ignore_hw_fail);

enum rtw_phl_status
rtw_phl_cmd_wrole_change(void *phl,
				struct rtw_wifi_role_t *wrole,
				enum wr_chg_id chg_id, u8 *chg_info, u8 chg_info_len,
				enum phl_cmd_type cmd_type, u32 cmd_timeout);

void rtw_phl_wifi_role_free(void *phl, u8 role_idx);

/*WIFI sta_info management section*/
struct rtw_phl_stainfo_t *
rtw_phl_alloc_stainfo_sw(void *phl,
                         u8 *sta_addr,
                         struct rtw_wifi_role_t *wrole);

enum rtw_phl_status
rtw_phl_free_stainfo_sw(void *phl, struct rtw_phl_stainfo_t *sta);

enum rtw_phl_status
rtw_phl_cmd_alloc_stainfo(void *phl,
                          struct rtw_phl_stainfo_t **sta,
                          u8 *sta_addr,
                          struct rtw_wifi_role_t *wrole,
                          bool alloc,
                          bool only_hw,
                          enum phl_cmd_type cmd_type,
                          u32 cmd_timeout);

enum rtw_phl_status
rtw_phl_cmd_change_stainfo(void *phl,
	struct rtw_phl_stainfo_t *sta, enum sta_chg_id chg_id,
	u8 *chg_info, u8 chg_info_len,
	enum phl_cmd_type cmd_type, u32 cmd_timeout);

enum rtw_phl_status
rtw_phl_cmd_update_media_status(void *phl,
                                struct rtw_phl_stainfo_t *sta,
                                u8 *sta_addr,
                                bool is_connect,
                                enum phl_cmd_type cmd_type,
                                u32 cmd_timeout);

struct rtw_phl_stainfo_t *
rtw_phl_get_stainfo_self(void *phl, struct rtw_wifi_role_t *wrole);

struct rtw_phl_stainfo_t *
rtw_phl_get_stainfo_by_addr(void *phl, struct rtw_wifi_role_t *wrole, u8 *addr);

struct rtw_phl_stainfo_t *
rtw_phl_get_stainfo_by_macid(void *phl, u16 macid);

u8
rtw_phl_get_sta_rssi(struct rtw_phl_stainfo_t *sta);

enum rtw_phl_status
rtw_phl_query_rainfo(void *phl, struct rtw_phl_stainfo_t *phl_sta,
		     struct rtw_phl_rainfo *ra_info);

void rtw_phl_sta_up_rx_bcn(void *phl, struct rtw_bcn_pkt_info *info);

/*macid management section, temporary for debuge*/
u16
rtw_phl_get_macid_max_num(void *phl);

u16
rtw_phl_wrole_bcmc_id_get(void *phl, struct rtw_wifi_role_t *wrole);

u8
rtw_phl_macid_is_bmc(void *phl, u16 macid);

u8
rtw_phl_macid_is_used(void *phl, u16 macid);

enum rtw_phl_status
rtw_phl_cmd_add_key(void *phl,
                    struct rtw_phl_stainfo_t *sta,
                    struct phl_sec_param_h *crypt,
                    u8 *keybuf,
                    enum phl_cmd_type cmd_type,
                    u32 cmd_timeout);

enum rtw_phl_status
rtw_phl_cmd_del_key(void *phl,
                    struct rtw_phl_stainfo_t *sta,
                    struct phl_sec_param_h *crypt,
                    enum phl_cmd_type cmd_type,
                    u32 cmd_timeout);
/* phy msg forwarder functions*/
enum rtw_phl_status rtw_phl_msg_hub_register_recver(void* phl,
		struct phl_msg_receiver* ctx, enum phl_msg_recver_layer layer);
enum rtw_phl_status rtw_phl_msg_hub_update_recver_mask(void* phl,
		enum phl_msg_recver_layer layer, u8* mdl_id, u32 len, u8 clr);
enum rtw_phl_status rtw_phl_msg_hub_deregister_recver(void* phl,
					enum phl_msg_recver_layer layer);
enum rtw_phl_status rtw_phl_msg_hub_send(void* phl,
		struct phl_msg_attribute* attr, struct phl_msg* msg);


u8 rtw_phl_trans_sec_mode(u8 unicast, u8 multicast);

u8 rtw_phl_get_sec_cam_idx(void *phl, struct rtw_phl_stainfo_t *sta,
			u8 keyid, u8 key_type);

void rtw_phl_test_txtb_cfg(struct rtw_phl_com_t* phl_com, void *buf,
	u32 buf_len, u8 *cfg_bssid, u16 cfg_aid, u8 cfg_bsscolor);
/* command dispatcher module section*/
enum rtw_phl_status rtw_phl_register_module(void *phl, u8 band_idx,
					enum phl_module_id id,
					struct phl_bk_module_ops* ops);
enum rtw_phl_status rtw_phl_deregister_module(void *phl,u8 band_idx,
					enum phl_module_id id);
u8 rtw_phl_is_fg_empty(void *phl, u8 band_idx);
/* opt: refer to enum phl_msg_opt */
enum rtw_phl_status rtw_phl_send_msg_to_dispr(void *phl, struct phl_msg* msg,
					      struct phl_msg_attribute* attr, u32* msg_hdl);
enum rtw_phl_status rtw_phl_cancel_dispr_msg(void *phl, u8 band_idx, u32* msg_hdl);

enum rtw_phl_status rtw_phl_add_cmd_token_req(void *phl, u8 band_idx,
				struct phl_cmd_token_req* req, u32* req_hdl);
enum rtw_phl_status rtw_phl_cancel_cmd_token(void *phl, u8 band_idx, u32* req_hdl);
enum rtw_phl_status rtw_phl_set_cur_cmd_info(void *phl, u8 band_idx,
					       struct phl_module_op_info* op_info);
enum rtw_phl_status rtw_phl_query_cur_cmd_info(void *phl, u8 band_idx,
					       struct phl_module_op_info* op_info);

enum rtw_phl_status rtw_phl_free_cmd_token(void *phl, u8 band_idx, u32* req_hdl);
enum rtw_phl_status rtw_phl_set_bk_module_info(void *phl, u8 band_idx,
		enum phl_module_id id,	struct phl_module_op_info* op_info);
enum rtw_phl_status rtw_phl_query_bk_module_info(void *phl, u8 band_idx,
		enum phl_module_id id,	struct phl_module_op_info* op_info);
enum rtw_phl_status rtw_phl_set_msg_disp_seq(void *phl,
						struct phl_msg_attribute *attr,
						struct msg_self_def_seq* seq);

/* BA session management */
void rtw_phl_stop_rx_ba_session(void *phl, struct rtw_phl_stainfo_t *sta,
				u16 tid);
enum rtw_phl_status
rtw_phl_start_rx_ba_session(void *phl, struct rtw_phl_stainfo_t *sta,
			    u8 dialog_token, u16 timeout, u16 start_seq_num,
			    u16 ba_policy, u16 tid, u16 buf_size);
void rtw_phl_rx_bar(void *phl, struct rtw_phl_stainfo_t *sta, u8 tid, u16 seq);
void rtw_phl_flush_reorder_buf(void *phl, struct rtw_phl_stainfo_t *sta);
enum rtw_phl_status
rtw_phl_enter_mon_mode(void *phl, struct rtw_wifi_role_t *wrole);
enum rtw_phl_status
rtw_phl_leave_mon_mode(void *phl, struct rtw_wifi_role_t *wrole);
#ifdef RTW_PHL_BCN
enum rtw_phl_status rtw_phl_free_bcn_entry(void *phl, struct rtw_wifi_role_t *wrole);
enum rtw_phl_status
rtw_phl_cmd_issue_beacon(void *phl,
				struct rtw_wifi_role_t *wifi_role,
				struct rtw_bcn_info_cmn *bcn_cmn,
				enum phl_cmd_type cmd_type,
				u32 cmd_timeout);
enum rtw_phl_status
rtw_phl_cmd_stop_beacon(void *phl,
				struct rtw_wifi_role_t *wifi_role,
				u8 stop,
				enum phl_cmd_type cmd_type,
				u32 cmd_timeout);
#endif

#ifdef CONFIG_CMD_DISP
enum rtw_phl_status
rtw_phl_cmd_chg_op_chdef(struct rtw_wifi_role_t *wrole,
	struct rtw_chan_def *new_chdef, bool cmd_wait, u32 cmd_timeout,
	void (*chg_opch_done)(void *priv, u8 ridx, enum rtw_phl_status status));


enum rtw_phl_status
rtw_phl_cmd_set_ch_bw(struct rtw_wifi_role_t *wifi_role,
                      struct rtw_chan_def *chdef,
                      bool do_rfk,
                      enum phl_cmd_type cmd_type,
                      u32 cmd_timeout);

enum rtw_phl_status
rtw_phl_cmd_enqueue(void *phl,
                    enum phl_band_idx band_idx,
                    enum phl_msg_evt_id evt_id,
                    u8 *cmd_buf,
                    u32 cmd_len,
                    void (*core_cmd_complete)(void *priv, u8 *cmd, u32 cmd_len, enum rtw_phl_status status),
                    enum phl_cmd_type cmd_type,
                    u32 cmd_timeout);

#endif /*CONFIG_CMD_DISP*/

u8 rtw_phl_get_cur_ch(struct rtw_wifi_role_t *wifi_role);
enum rtw_phl_status
rtw_phl_get_cur_hal_chdef(struct rtw_wifi_role_t *wifi_role,
					struct rtw_chan_def *cur_chandef);

enum band_type rtw_phl_get_band_type(u8 chan);
u8 rtw_phl_get_center_ch(u8 ch,
	enum channel_width bw, enum chan_offset offset);

enum rtw_phl_status
rtw_phl_cmd_dfs_tx_pause(struct rtw_wifi_role_t *wifi_role, bool pause,
                      		enum phl_cmd_type cmd_type, u32 cmd_timeout);

u8
rtw_phl_get_operating_class(
	struct rtw_chan_def chan_def
);

bool
rtw_phl_get_chandef_from_operating_class(
	u8 channel,
	u8 operating_class,
	struct rtw_chan_def *chan_def
);
/*
 * export API from sw cap module
 */
void rtw_phl_final_cap_decision(void *phl);


enum rtw_phl_status
rtw_phl_get_dft_proto_cap(void *phl, u8 hw_band, enum role_type rtype,
				struct protocol_cap_t *role_proto_cap);
enum rtw_phl_status
rtw_phl_get_dft_cap(void *phl, u8 hw_band, struct role_cap_t *role_cap);

void rtw_phl_mac_dbg_status_dump(void *phl, u32 *val, u8 *en);

#ifdef CONFIG_DBCC_SUPPORT
enum rtw_phl_status
rtw_phl_dbcc_test(void *phl, enum dbcc_test_id id, void *param);
#endif
/*
 * API for config channel info CR
 */
#ifdef CONFIG_PHL_CHANNEL_INFO
enum rtw_phl_status
rtw_phl_cmd_cfg_chinfo(void *phl, struct rtw_phl_stainfo_t *sta,
		      u8 enable, enum phl_cmd_type cmd_type, u32 cmd_timeout);
enum rtw_phl_status rtw_phl_query_chan_info(void *phl, u32 buf_len,
	u8* chan_info_buffer, u32 *length, struct csi_header_t *csi_header);
#endif /* CONFIG_PHL_CHANNEL_INFO */

void rtw_phl_set_edcca_mode(void *phl, enum rtw_edcca_mode mode);
enum rtw_edcca_mode rtw_phl_get_edcca_mode(void *phl);

bool rtw_phl_set_user_def_chplan(void *phl, struct rtw_user_def_chplan *udef);
bool rtw_phl_valid_regulation_domain(u8 domain);
bool rtw_phl_regulation_set_domain(void *phl, u8 domain,
				       	enum regulation_rsn reason);
bool rtw_phl_regulation_set_country(void *phl, char *country,
					enum regulation_rsn reason);
bool rtw_phl_regulation_set_capability(void *phl,
		enum rtw_regulation_capability capability);
bool rtw_phl_regulation_query_chplan(
			void *phl, enum rtw_regulation_query type,
			struct rtw_chlist *filter,
			struct rtw_regulation_chplan *plan);
bool rtw_phl_query_specific_chplan(void *phl, u8 domain,
			struct rtw_regulation_chplan *plan);
bool rtw_phl_query_country_chplan(char *country,
			struct rtw_regulation_country_chplan *country_chplan);
bool rtw_phl_generate_scan_instance(struct instance_strategy *strategy,
				struct rtw_regulation_chplan *chplan,
				struct instance *inst);
bool rtw_phl_scan_instance_insert_ch(void *phl, struct instance *inst,
					enum band_type band, u8 channel,
						u8 strategy_period);
bool rtw_phl_regulation_valid_channel(void *phl, enum band_type band,
					u16 channel, u8 reject);
bool rtw_phl_regulation_dfs_channel(void *phl, enum band_type band,
					u16 channel, bool *dfs);
bool rtw_phl_query_regulation_info(void *phl, struct rtw_regulation_info *info);
bool rtw_phl_regulation_query_ch(void *phl, enum band_type band, u8 channel,
					struct rtw_regulation_channel *ch);
u8 rtw_phl_get_domain_regulation_2g(u8 domain);
u8 rtw_phl_get_domain_regulation_5g(u8 domain);

enum rtw_phl_status rtw_phl_get_mac_addr_efuse(void* phl, u8 *addr);

/**
 * rtw_phl_usb_tx_ep_id - query  USB tx end point index
 * identified by macid, tid and band
 * @macid: input target macid is 0 ~ 127
* @tid: input target tid, range is 0 ~ 7
 * @band: input target band, 0 for band 0 / 1 for band 1
 *
 * returns corresponding end point idx of a specific tid
 */
u8 rtw_phl_usb_tx_ep_id(void *phl, u16 macid, u8 tid, u8 band);

enum rtw_phl_status
rtw_phl_cfg_trx_path(void* phl, enum rf_path tx, u8 tx_nss,
		     enum rf_path rx, u8 rx_nss);

void rtw_phl_reset_stat_ma_rssi(struct rtw_phl_com_t *phl_com);

u8
rtw_phl_get_ma_rssi(struct rtw_phl_com_t *phl_com,
		    enum rtw_rssi_type rssi_type);

bool rtw_phl_chanctx_chk(void *phl, struct rtw_wifi_role_t *wifi_role,
		u8 chan, enum channel_width bw, enum chan_offset offset);
bool rtw_phl_chanctx_add(void *phl, struct rtw_wifi_role_t *wifi_role,
		u8 *chan, enum channel_width *bw, enum chan_offset *offset);
int rtw_phl_chanctx_del(void *phl, struct rtw_wifi_role_t *wifi_role,
						struct rtw_chan_def *chan_def);
enum rtw_phl_status rtw_phl_chanctx_del_no_self(void *phl, struct rtw_wifi_role_t *wifi_role);
int rtw_phl_mr_get_chanctx_num(void *phl, struct rtw_wifi_role_t *wifi_role);
enum rtw_phl_status rtw_phl_mr_get_chandef(void *phl, struct rtw_wifi_role_t *wifi_role,
							struct rtw_chan_def *chandef);
enum rtw_phl_status rtw_phl_mr_upt_chandef(void *phl, struct rtw_wifi_role_t *wifi_role);

#ifdef CONFIG_MCC_SUPPORT
u8 rtw_phl_mr_query_mcc_inprogress (void *phl, struct rtw_wifi_role_t *wrole,
							enum rtw_phl_mcc_chk_inprocess_type check_type);
#endif

u8 rtw_phl_mr_dump_mac_addr(void *phl,
					struct rtw_wifi_role_t *wifi_role);
u8 rtw_phl_mr_buddy_dump_mac_addr(void *phl,
					struct rtw_wifi_role_t *wifi_role);
enum rtw_phl_status
rtw_phl_mr_rx_filter(void *phl, struct rtw_wifi_role_t *wrole);

#ifdef CONFIG_FSM
enum rtw_phl_status
rtw_phl_mr_offch_hdl(void *phl,
		     struct rtw_wifi_role_t *wrole,
		     bool off_ch,
		     void *obj_priv,
		     u8 (*issue_null_data)(void *priv, u8 ridx, bool ps),
		     struct rtw_chan_def *chandef);
#endif
void rtw_phl_mr_ops_init (void *phl, struct rtw_phl_mr_ops *mr_ops);

#ifdef	PHL_MR_PROC_CMD
void rtw_phl_mr_dump_info(void *phl, bool show_caller);
void rtw_phl_mr_dump_band_ctl(void *phl, bool show_caller);
bool rtw_phl_chanctx_test(void *phl, struct rtw_wifi_role_t *wifi_role, bool is_add,
		u8 *chan, enum channel_width *bw, enum chan_offset *offset);
#endif
void rtw_phl_sta_dump_info(void *phl, bool show_caller, struct rtw_wifi_role_t *wr, u8 mode);

bool rtw_phl_mr_query_info(void *phl, struct rtw_wifi_role_t *wrole,
				struct mr_query_info *info);

u8 rtw_phl_mr_get_opch_list(void *phl, struct rtw_wifi_role_t *wifi_role,
				struct rtw_chan_def *chdef_list, u8 list_size);

void rtw_phl_mr_dump_cur_chandef(void *phl, struct rtw_wifi_role_t *wifi_role);

enum mr_op_mode
rtw_phl_mr_get_opmode(void *phl, struct rtw_wifi_role_t *wrole);

void rtw_phl_led_set_ctrl_mode(void *phl, enum rtw_led_id led_id,
			       enum rtw_led_ctrl_mode ctrl_mode);
void rtw_phl_led_set_toggle_intervals(void *phl, u8 intervals_idx,
				      u32 *intervals, u8 intervals_len);
void rtw_phl_led_set_action(void *phl, enum rtw_led_event event,
			    enum rtw_led_state state_condition,
			    struct rtw_led_action_args_t *action_args_arr,
			    u8 action_args_arr_len, u32 toggle_delay_unit);
void rtw_phl_led_control(void *phl, enum rtw_led_event led_event);

#ifdef CONFIG_RTW_ACS
u16 rtw_phl_acs_get_channel_by_idx(void *phl, u8 ch_idx);
u8 rtw_phl_acs_get_clm_ratio_by_idx(void *phl, u8 ch_idx);
s8  rtw_phl_noise_query_by_idx(void *phl, u8 ch_idx);
#endif /* CONFIG_RTW_ACS */

void rtw_phl_get_env_rpt(void *phl, struct rtw_env_report *env_rpt, struct rtw_wifi_role_t *wrole);

#ifdef RTW_WKARD_DYNAMIC_BFEE_CAP
enum rtw_phl_status
rtw_phl_bfee_ctrl(void *phl, struct rtw_wifi_role_t *wrole, bool ctrl);
#endif

enum rtw_phl_status
rtw_phl_snd_init_ops_send_ndpa(void *phl,
                               enum rtw_phl_status (*snd_send_ndpa)(void *,
                                                                  struct rtw_wifi_role_t *,
                                                                  u8 *,
                                                                  u32 *,
                                                                  enum channel_width));

u8 rtw_phl_snd_chk_in_progress(void *phl);

enum rtw_phl_status
rtw_phl_sound_start(void *phl, u8 wrole_idx, u8 st_dlg_tkn, u8 period, u8 test_flag);

enum rtw_phl_status
rtw_phl_sound_abort(void *phl);

enum rtw_phl_status
rtw_phl_set_power_lmt(void *phl, u8 hw_band);

s8 rtw_phl_get_power_limit(void *phl, u8 hw_band,
	u16 rate, u8 bandwidth, u8 beamforming, u8 tx_num, u8 channel);

void
rtw_phl_enable_ext_pwr_lmt(void *phl, u8 hw_band,
	struct rtw_phl_ext_pwr_lmt_info *ext_pwr_lmt_info);

void rtw_phl_init_ppdu_sts_para(struct rtw_phl_com_t *phl_com,
				bool en_psts_per_pkt, bool psts_ampdu,
				u8 rx_fltr);

enum rtw_phl_status rtw_phl_rf_on(void *phl);
enum rtw_phl_status rtw_phl_rf_off(void *phl);


#ifdef CONFIG_PHL_TWT
enum rtw_phl_status
rtw_phl_twt_alloc_twt_config(void *phl, struct rtw_wifi_role_t *role,
		struct rtw_phl_twt_setup_info setup_info, u8 benable, u8 *id);

enum rtw_phl_status
rtw_phl_twt_free_twt_config(void *phl, u8 id);

enum rtw_phl_status
rtw_phl_twt_enable_twt_config(void *phl, u8 id);

enum rtw_phl_status
rtw_phl_twt_free_all_twt_by_role(void *phl, struct rtw_wifi_role_t *role);

enum rtw_phl_status
rtw_phl_twt_disable_all_twt_by_role(void *phl, struct rtw_wifi_role_t *role);

enum rtw_phl_status
rtw_phl_twt_enable_all_twt_by_role(void *phl, struct rtw_wifi_role_t *role);

enum rtw_phl_status
rtw_phl_twt_add_sta_info(void *phl, struct rtw_phl_stainfo_t *phl_sta,
			u8 config_id, u8 id);

enum rtw_phl_status
rtw_phl_twt_teardown_sta(void *phl, struct rtw_phl_stainfo_t *phl_sta,
			struct rtw_phl_twt_flow_field *twt_flow, u8 *bitmap);

enum rtw_phl_status
rtw_phl_twt_get_new_flow_id(void *phl, struct rtw_phl_stainfo_t *phl_sta, u8 *id);


enum rtw_phl_status
rtw_phl_twt_accept_for_sta_mode(void *phl, struct rtw_phl_stainfo_t *phl_sta,
			struct rtw_phl_twt_setup_info *setup_info, u8 *id);

enum rtw_phl_status
rtw_phl_twt_teardown_for_sta_mode(void *phl, struct rtw_phl_stainfo_t *phl_sta,
				struct rtw_phl_twt_flow_field *twt_flow);

enum rtw_phl_status
rtw_phl_twt_delete_all_sta_info(void *phl, struct rtw_phl_stainfo_t *phl_sta,
				u8 *bitmap);

enum rtw_phl_status
rtw_phl_twt_get_target_wake_time(void *phl, u8 port, u8 id, u16 offset, u32 *tsf_h,
				u32 *tsf_l);

enum rtw_phl_status
rtw_phl_twt_sta_announce_to_fw(void *phl, u16 macid);

enum rtw_phl_status
rtw_phl_twt_handle_c2h(void *phl_com, void *c);

enum rtw_phl_status
rtw_phl_twt_fill_twt_element(struct rtw_phl_twt_element *twt_ele, u8 *buf,
								u8 *length);

enum rtw_phl_status
rtw_phl_twt_fill_flow_field(struct rtw_phl_twt_flow_field *twt_flow, u8 *buf,
								u16 *length);
enum rtw_phl_status
rtw_phl_twt_parse_element(u8 *twt_ele, u16 length,
				struct rtw_phl_twt_element *twt_element);

enum rtw_phl_status
rtw_phl_twt_parse_setup_info(u8 *pkt, u16 length,
				struct rtw_phl_twt_setup_info *setup_info);

enum rtw_phl_status
rtw_phl_twt_parse_flow_field(u8 *ie_twt_flow, u16 length,
				struct rtw_phl_twt_flow_field *twt_flow);
#endif /* CONFIG_PHL_TWT */

enum rtw_phl_status
rtw_phl_snd_cmd_set_vht_gid(void *phl,
			struct rtw_wifi_role_t *wrole,
			struct rtw_phl_gid_pos_tbl *tbl);

enum rtw_phl_status
rtw_phl_snd_cmd_set_aid(void *phl,
			struct rtw_wifi_role_t *wrole,
			struct rtw_phl_stainfo_t *sta,
			u16 aid);

#ifdef RTW_WKARD_P2PPS_REFINE
#ifdef CONFIG_PHL_P2PPS
enum rtw_phl_status
rtw_phl_p2pps_noa_update(void *phl, struct rtw_phl_noa_desc *in_desc);

void
rtw_phl_p2pps_init_ops(void *phl, struct rtw_phl_p2pps_ops *ops);

void rtw_phl_p2pps_noa_disable_all(void *phl, struct rtw_wifi_role_t *w_role);
#endif
#endif
enum rtw_phl_status
rtw_phl_snd_cmd_set_vht_gid(void *phl,
			struct rtw_wifi_role_t *wrole,
			struct rtw_phl_gid_pos_tbl *tbl);

void rtw_phl_event_notify(void *phl, enum phl_msg_evt_id event,
			struct rtw_wifi_role_t *wrole);
void rtw_phl_notification(void *phl,
                          enum phl_msg_evt_id event,
                          struct rtw_wifi_role_t *wrole);
void rtw_phl_dev_terminate_ntf(void *phl);

enum rtw_phl_status
rtw_phl_cmd_force_usb_switch(void *phl, u32 speed,
				enum phl_band_idx band_idx,
				enum phl_cmd_type cmd_type, u32 cmd_timeout);
enum rtw_phl_status
rtw_phl_cmd_get_usb_speed(void *phl, u32* speed,
				enum phl_band_idx band_idx,
				enum phl_cmd_type cmd_type, u32 cmd_timeout);
enum rtw_phl_status
rtw_phl_cmd_get_usb_support_ability(void *phl, u32* ability,
				enum phl_band_idx band_idx,
				enum phl_cmd_type cmd_type, u32 cmd_timeout);
u8 rtw_phl_get_sta_mgnt_rssi(struct rtw_phl_stainfo_t *psta);

enum rtw_phl_status
rtw_phl_txsts_rpt_config(void *phl, struct rtw_phl_stainfo_t *phl_sta);

#ifdef CONFIG_USB_HCI
/* tx_ok/tx_fail are from release report*/
enum rtw_phl_status
rtw_phl_get_tx_ok_rpt(void *phl, struct rtw_phl_stainfo_t *phl_sta, u32 *tx_ok_cnt,
 enum phl_ac_queue qsel);

enum rtw_phl_status
rtw_phl_get_tx_fail_rpt(void *phl, struct rtw_phl_stainfo_t *phl_sta, u32 *tx_fail_cnt,
 enum phl_ac_queue qsel);

/* tx retry is from ra sts report.*/
enum rtw_phl_status
rtw_phl_get_tx_retry_rpt(void *phl, struct rtw_phl_stainfo_t *phl_sta, u32 *tx_retry_cnt,
 enum phl_ac_queue qsel);
#endif /* CONFIG_USB_HCI */

enum rtw_rx_status rtw_phl_get_rx_status(void *phl);

void rtw_phl_dbg_dump_rx(void *phl, struct rtw_wifi_role_t *wrole);

/******************************************************************************
 *
 * TX power APIs
 *
 *****************************************************************************/
const char *rtw_phl_get_pw_lmt_regu_type_str(void *phl, enum band_type band);

bool rtw_phl_get_pwr_lmt_en(void *phl, u8 band_idx);

enum rtw_phl_status rtw_phl_set_tx_power(void *phl, u8 band_idx);

#endif /*_PHL_API_H_*/

