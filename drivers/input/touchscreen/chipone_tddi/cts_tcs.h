// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef _CTS_TCS_H_
#define _CTS_TCS_H_

int cts_tcs_read(struct cts_device *cts_dev,
        u16 cmd, u8 *buf, size_t len);
int cts_tcs_write(struct cts_device *cts_dev,
        u16 cmd, u8 *buf, size_t len);

int cts_tcs_get_hwid_info(struct cts_device *cts_dev, u8 *info);
int cts_tcs_get_fw_ver(struct cts_device *cts_dev, u16 *fwver);
int cts_tcs_get_lib_ver(struct cts_device *cts_dev, u16 *libver);
int cts_tcs_get_ddi_ver(struct cts_device *cts_dev, u8 *ddiver);
int cts_tcs_get_res_x(struct cts_device *cts_dev, u16 *res_x);
int cts_tcs_get_res_y(struct cts_device *cts_dev, u16 *res_y);
int cts_tcs_get_rows(struct cts_device *cts_dev, u8 *rows);
int cts_tcs_get_cols(struct cts_device *cts_dev, u8 *cols);
int cts_tcs_get_rx_tx_info(struct cts_device *cts_dev, u8 *info);
int cts_tcs_get_panel_rx_tx_info(struct cts_device *cts_dev, u8 *info);
int cts_tcs_get_flip_x(struct cts_device *cts_dev, bool *flip_x);
int cts_tcs_get_flip_y(struct cts_device *cts_dev, bool *flip_y);
int cts_tcs_get_swap_axes(struct cts_device *cts_dev, bool *swap_axes);
int cts_tcs_clr_gstr_ready_flag(struct cts_device *cts_dev);

int cts_tcs_get_int_mode(struct cts_device *cts_dev, u8 *int_mode);
int cts_tcs_get_int_keep_time(struct cts_device *cts_dev,
        u16 *int_keep_time);
int cts_tcs_get_rawdata_target(struct cts_device *cts_dev,
        u16 *rawdata_target);
int cts_tcs_get_esd_method(struct cts_device *cts_dev, u8 *esd_method);

int cts_tcs_get_touchinfo(struct cts_device *cts_dev,
        struct cts_device_touch_info *touch_info);
int cts_tcs_get_gestureinfo(struct cts_device *cts_dev,
        struct cts_device_gesture_info *gesture_info);
int cts_tcs_get_touch_status(struct cts_device *cts_dev);
int cts_tcs_get_esd_protection(struct cts_device *cts_dev,
        u8 *esd_protection);

int cts_tcs_read_hw_reg(struct cts_device *cts_dev, u32 addr,
        u8 *buf, size_t size);
int cts_tcs_write_hw_reg(struct cts_device *cts_dev, u32 addr,
        u8 *buf, size_t size);
int cts_tcs_read_ddi_reg(struct cts_device *cts_dev, u32 addr,
        u8 *regbuf, size_t size);
int cts_tcs_write_ddi_reg(struct cts_device *cts_dev, u32 addr,
        u8 *regbuf, size_t size);
int cts_tcs_read_reg(struct cts_device *cts_dev, u32 addr,
        u8 *buf, size_t size);
int cts_tcs_write_reg(struct cts_device *cts_dev, u32 addr,
        u8 *buf, size_t size);

int cts_tcs_get_fw_id(struct cts_device *cts_dev, u16 *fwid);
int cts_tcs_get_workmode(struct cts_device *cts_dev, u8 *workmode);
int cts_tcs_set_workmode(struct cts_device *cts_dev, u8 workmode);
int cts_tcs_set_openshort_mode(struct cts_device *cts_dev, u8 mode);
int cts_tcs_get_curr_mode(struct cts_device *cts_dev, u8 *currmode);
int cts_tcs_get_krang_current_workmode(struct cts_device *cts_dev, u8 *workmode);
int cts_tcs_set_krang_workmode(struct cts_device *cts_dev, u8 workmode);

int cts_tcs_set_tx_vol(struct cts_device *cts_dev, u8 txvol);

int cts_tcs_set_short_test_type(struct cts_device *cts_dev, u8 short_type);
int cts_tcs_set_openshort_enable(struct cts_device *cts_dev, u8 enable);
int cts_tcs_is_openshort_enabled(struct cts_device *cts_dev, u8 *enabled);

int cts_tcs_set_esd_enable(struct cts_device *cts_dev, u8 enable);
int cts_tcs_set_cneg_enable(struct cts_device *cts_dev, u8 enable);
int cts_tcs_set_mnt_enable(struct cts_device *cts_dev, u8 enable);
int cts_tcs_is_display_on(struct cts_device *cts_dev, u8 *display_on);
int cts_tcs_set_display_on(struct cts_device *cts_dev, u8 display_on);
int cts_tcs_is_cneg_enabled(struct cts_device *cts_dev, u8 *enabled);
int cts_tcs_is_mnt_enabled(struct cts_device *cts_dev, u8 *enabled);
int cts_tcs_set_pwr_mode(struct cts_device *cts_dev, u8 pwr_mode);
int cts_tcs_get_has_int_data(struct cts_device *cts_dev, bool *has);
int cts_tcs_get_int_data_types(struct cts_device *cts_dev, u16 *type);
int cts_tcs_set_int_data_types(struct cts_device *cts_dev, u16 type);
int cts_tcs_get_int_data_method(struct cts_device *cts_dev, u8 *method);
int cts_tcs_set_int_data_method(struct cts_device *cts_dev, u8 method);
int cts_tcs_calc_int_data_size(struct cts_device *cts_dev);

int cts_tcs_polling_data(struct cts_device *cts_dev,u8 *buf, size_t size);
int cts_polling_test_data(struct cts_device *cts_dev,
        u8 *buf, size_t size,  enum int_data_type type);

int cts_tcs_tool_get_rawdata(struct cts_device *cts_dev, u8 *buf, u16 data_source);
int cts_tcs_tool_get_manual_diff(struct cts_device *cts_dev, u8 *buf, u16 data_source);
int cts_tcs_tool_get_real_diff(struct cts_device *cts_dev, u8 *buf, u16 data_source);
int cts_tcs_tool_get_basedata(struct cts_device *cts_dev, u8 *buf, u16 data_source);

int cts_tcs_top_get_rawdata(struct cts_device *cts_dev, u8 *buf, size_t size,
	u16 data_source);
int cts_tcs_top_get_manual_diff(struct cts_device *cts_dev, u8 *buf,
        size_t size, u16 data_source);
int cts_tcs_top_get_real_diff(struct cts_device *cts_dev, u8 *buf, size_t size,
	u16 data_source);
int cts_tcs_top_get_noise_diff(struct cts_device *cts_dev, u8 *buf,size_t size,
	u16 data_source);
int cts_tcs_top_get_basedata(struct cts_device *cts_dev, u8 *buf, size_t size,
	u16 data_source);
int cts_tcs_top_get_cnegdata(struct cts_device *cts_dev, u8 *buf, size_t size,
	u16 data_source);
int cts_tcs_reset_device(struct cts_device *cts_dev);
int cts_tcs_set_int_test(struct cts_device *cts_dev, u8 enable);
int cts_tcs_set_int_pin(struct cts_device *cts_dev, u8 high);
int cts_tcs_get_module_id(struct cts_device *cts_dev, u32 *modId);

int cts_tcs_tool_xtrans(struct cts_device *cts_dev, u8 *tx, size_t txlen,
        u8 *rx, size_t rxlen);

int cts_tcs_set_charger_plug(struct cts_device *cts_dev, u8 set);
int cts_tcs_get_charger_plug(struct cts_device *cts_dev, u8 *isset);
int cts_tcs_set_earjack_plug(struct cts_device *cts_dev, u8 set);
int cts_tcs_get_earjack_plug(struct cts_device *cts_dev, u8 *isset);
int cts_tcs_set_panel_direction(struct cts_device *cts_dev, u8 direction);
int cts_tcs_get_panel_direction(struct cts_device *cts_dev, u8 *direction);
int cts_tcs_set_game_mode(struct cts_device *cts_dev, u8 enable);
int cts_tcs_get_game_mode(struct cts_device *cts_dev, u8 *enabled);
int cts_tcs_set_proximity_mode(struct cts_device *cts_dev, u8 enable);
void cts_tcs_reinit_fw_status(struct cts_device *cts_dev);

int cts_tcs_set_product_en(struct cts_device *cts_dev, u8 enable);
int cts_tcs_wait_krang_stop(struct cts_device *cts_dev);

extern int cts_tcs_set_glove_mode(struct cts_device *cts_dev, u8 enable);
#endif /* _CTS_TCS_H_ */
