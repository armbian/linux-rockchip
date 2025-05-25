/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 */

#ifndef _PWM_ROCKCHIP_H_
#define _PWM_ROCKCHIP_H_

#include <linux/pwm.h>

/**
 * enum rockchip_pwm_clk_src_sel - select the clk src of pwm
 * @PWM_SELECT_CLK_PWM: select clk_pwm as clk src
 * @PWM_SELECT_CLK_PWM_OSC: select clk_pwm_osc as clk src
 * @PWM_SELECT_CLK_PWM_RC: select clk_pwm_rc as clk src
 */
enum rockchip_pwm_clk_src_sel {
	PWM_SELECT_CLK_PWM,
	PWM_SELECT_CLK_PWM_OSC,
	PWM_SELECT_CLK_PWM_RC,
};

/**
 * enum rockchip_pwm_global_ctrl_cmd - commands for pwm global ctrl
 * @PWM_GLOBAL_CTRL_JOIN: join the global control group
 * @PWM_GLOBAL_CTRL_EXIT: exit the global control group
 * @PWM_GLOBAL_CTRL_GRANT: obtian the permission of global control
 * @PWM_GLOBAL_CTRL_RECLAIM: reclaim the permission of global control
 * @PWM_GLOBAL_CTRL_UPDATE: update the configs for all channels in group
 * @PWM_GLOBAL_CTRL_ENABLE: enable all channels in group
 * @PWM_GLOBAL_CTRL_DISABLE: disable all channels in group
 */
enum rockchip_pwm_global_ctrl_cmd {
	PWM_GLOBAL_CTRL_JOIN,
	PWM_GLOBAL_CTRL_EXIT,
	PWM_GLOBAL_CTRL_GRANT,
	PWM_GLOBAL_CTRL_RECLAIM,
	PWM_GLOBAL_CTRL_UPDATE,
	PWM_GLOBAL_CTRL_ENABLE,
	PWM_GLOBAL_CTRL_DISABLE,
};

/**
 * enum rockchip_pwm_counter_input_sel - select the input src of counter
 * @PWM_COUNTER_INPUT_FROM_IO: input from PWM IO
 * @PWM_COUNTER_INPUT_FROM_CRU: input from CRU
 */
enum rockchip_pwm_counter_input_sel {
	PWM_COUNTER_INPUT_FROM_IO,
	PWM_COUNTER_INPUT_FROM_CRU,
};

/**
 * enum rockchip_pwm_freq_meter_input_sel - select the input src of frequency meter
 * @PWM_FREQ_METER_INPUT_FROM_IO: input from PWM IO
 * @PWM_FREQ_METER_INPUT_FROM_CRU: input from CRU
 */
enum rockchip_pwm_freq_meter_input_sel {
	PWM_FREQ_METER_INPUT_FROM_IO,
	PWM_FREQ_METER_INPUT_FROM_CRU,
};

/**
 * struct rockchip_pwm_wave_table - wave table config object
 * @offset: the offset of wave table to set
 * @len: the length of wave table to set
 * @table: the values of wave table to set
 * @
 */
struct rockchip_pwm_wave_table {
	u16 offset;
	u16 len;
	u64 *table;
};

/**
 * enum rockchip_pwm_wave_table_width_mode - element width of pwm wave table
 * @PWM_WAVE_TABLE_8BITS_WIDTH: each element in table is 8bits
 * @PWM_WAVE_TABLE_16BITS_WIDTH: each element in table is 16bits
 */
enum rockchip_pwm_wave_table_width_mode {
	PWM_WAVE_TABLE_8BITS_WIDTH,
	PWM_WAVE_TABLE_16BITS_WIDTH,
};

/**
 * enum rockchip_pwm_wave_update_mode - update mode of wave generator
 * @PWM_WAVE_INCREASING:
 *     The wave table address will wrap back to minimum address when increase to
 *     maximum and then increase again.
 * @PWM_WAVE_INCREASING_THEN_DECREASING:
 *     The wave table address will change to decreasing when increasing to the maximum
 *     address. it will return to increasing when decrease to the minimum value.
 */
enum rockchip_pwm_wave_update_mode {
	PWM_WAVE_INCREASING,
	PWM_WAVE_INCREASING_THEN_DECREASING,
};

/**
 * struct rockchip_pwm_wave_config - wave generator config object
 * @wave_table: the wave table config
 * @clk_src: the clk src selection in wave generator mode
 * @mem_clk_src: the memory clk src selection in wave generator mode
 * @width_mode: the width mode of wave table
 * @update_mode: the update mode of wave generator
 * @enable: enable or disable wave generator
 * @duty_en: to update duty by duty table or not
 * @period_en: to update period by period table or not
 * @clk_rate: the dclk rate in wave generator mode
 * @rpt: the number of repeated effective periods
 * @duty_max: the maximum address of duty table
 * @duty_min: the minimum address of duty table
 * @period_max: the maximum address of period table
 * @period_min: the minimum address of period table
 * @offset: the initial offset address of duty and period
 * @middle: the middle address of duty and period
 * @max_hold: the time to stop at maximum address
 * @min_hold: the time to stop at minimum address
 * @middle_hold: the time to stop at middle address
 */
struct rockchip_pwm_wave_config {
	struct rockchip_pwm_wave_table *wave_table;
	enum rockchip_pwm_clk_src_sel clk_src;
	enum rockchip_pwm_clk_src_sel mem_clk_src;
	enum rockchip_pwm_wave_table_width_mode width_mode;
	enum rockchip_pwm_wave_update_mode update_mode;
	bool enable;
	bool duty_en;
	bool period_en;
	unsigned long clk_rate;
	u16 rpt;
	u32 duty_max;
	u32 duty_min;
	u32 period_max;
	u32 period_min;
	u32 offset;
	u32 middle;
	u32 max_hold;
	u32 min_hold;
	u32 middle_hold;
};

/**
 * enum rockchip_pwm_biphasic_mode - mode of biphasic counter
 * @PWM_BIPHASIC_COUNTER_MODE0: single phase increase mode with A-phase
 * @PWM_BIPHASIC_COUNTER_MODE1: single phase increase/decrease mode with A-phase
 * @PWM_BIPHASIC_COUNTER_MODE2: dual phase with A/B-phase mode
 * @PWM_BIPHASIC_COUNTER_MODE3: dual phase with A/B-phase 2 times frequency mode
 * @PWM_BIPHASIC_COUNTER_MODE4: dual phase with A/B-phase 4 times frequency mode
 */
enum rockchip_pwm_biphasic_mode {
	PWM_BIPHASIC_COUNTER_MODE0,
	PWM_BIPHASIC_COUNTER_MODE1,
	PWM_BIPHASIC_COUNTER_MODE2,
	PWM_BIPHASIC_COUNTER_MODE3,
	PWM_BIPHASIC_COUNTER_MODE4,
	PWM_BIPHASIC_COUNTER_MODE0_FREQ,
};

/**
 * struct rockchip_pwm_biphasic_config - biphasic counter config object
 * @enable: enable: enable or disable biphasic counter
 * @is_continuous: biphascic counter will not stop at the end of timer in continuous mode
 * @mode: the mode of biphasic counter
 * @delay_ms: time to wait, in milliseconds, before getting biphasic counter result
 */
struct rockchip_pwm_biphasic_config {
	bool enable;
	bool is_continuous;
	u8 mode;
	u32 delay_ms;
};

#if IS_REACHABLE(CONFIG_PWM_ROCKCHIP)
/**
 * rockchip_pwm_set_counter() - setup pwm counter mode
 * @pwm: PWM device
 * @enable: enable/disable counter mode
 *
 * Returns: 0 on success or a negative error code on failure.
 */
int rockchip_pwm_set_counter(struct pwm_device *pwm,
			     enum rockchip_pwm_counter_input_sel input_sel,
			     bool enable);

/**
 * rockchip_pwm_get_counter_result() - get counter result
 * @pwm: PWM device
 * @counter_res: number of input waveforms
 * @is_clear: clear counter result or not
 *
 * Returns: 0 on success or a negative error code on failure.
 */
int rockchip_pwm_get_counter_result(struct pwm_device *pwm,
				    unsigned long *counter_res, bool is_clear);

/**
 * rockchip_pwm_set_freq_meter() - setup pwm frequency meter mode
 * @pwm: PWM device
 * @delay_ms: time to wait, in milliseconds, before getting frequency meter result
 * @input_sel: selec the input of frequency meter
 * @freq_hz: parameter in Hz to fill with frequency meter result
 *
 * Returns: 0 on success or a negative error code on failure.
 */
int rockchip_pwm_set_freq_meter(struct pwm_device *pwm, unsigned long delay_ms,
				enum rockchip_pwm_freq_meter_input_sel input_sel,
				unsigned long *freq_hz);

/**
 * rockchip_pwm_global_ctrl() - execute global control commands
 * @pwm: PWM device
 * @cmd: command type to execute
 *
 * Returns: 0 on success or a negative error code on failure.
 */
int rockchip_pwm_global_ctrl(struct pwm_device *pwm, enum rockchip_pwm_global_ctrl_cmd cmd);

/**
 * rockchip_pwm_set_wave() - setup pwm wave generator mode
 * @pwm: PWM device
 * @config: configs of wave generator mode
 *
 * Returns: 0 on success or a negative error code on failure.
 */
int rockchip_pwm_set_wave(struct pwm_device *pwm, struct rockchip_pwm_wave_config *config);

/**
 * rockchip_pwm_set_biphasic() - setup biphasic counter mode
 * @pwm: PWM device
 * @config: config of biphasic counter mode
 * @biphasic_res: biphasic counter result
 */
int rockchip_pwm_set_biphasic(struct pwm_device *pwm, struct rockchip_pwm_biphasic_config *config,
			      unsigned long *biphasic_res);

/**
 * rockchip_pwm_get_biphasic_result() - get biphasic counter result (valid only in continuous mode)
 * @pwm: PWM device
 * @biphasic_res: biphasic counter result
 */
int rockchip_pwm_get_biphasic_result(struct pwm_device *pwm, unsigned long *biphasic_res);
#else
static inline int rockchip_pwm_set_counter(struct pwm_device *pwm,
					   enum rockchip_pwm_counter_input_sel input_sel,
					   bool enable);
{
	return 0;
}

static inline int rockchip_pwm_get_counter_result(struct pwm_device *pwm,
						  unsigned long *counter_res, bool is_clear)
{
	return 0;
}

static inline int rockchip_pwm_set_freq_meter(struct pwm_device *pwm, unsigned long delay_ms,
					      enum rockchip_pwm_freq_meter_input_sel input_sel,
					      unsigned long *freq_hz)
{
	return 0;
}

static inline  int rockchip_pwm_global_ctrl(struct pwm_device *pwm,
					    enum rockchip_pwm_global_ctrl_cmd cmd)
{
	return 0;
}

static inline int rockchip_pwm_set_wave(struct pwm_device *pwm,
					struct rockchip_pwm_wave_config *config)
{
	return 0;
}

static inline int rockchip_pwm_set_biphasic(struct pwm_device *pwm,
					    struct rockchip_pwm_biphasic_config *config,
					    unsigned long *biphasic_res)
{
	return 0;
}

static inline int rockchip_pwm_get_biphasic_result(struct pwm_device *pwm,
						   unsigned long *biphasic_res)
{
	return 0;
}
#endif

#endif /* _PWM_ROCKCHIP_H_ */
