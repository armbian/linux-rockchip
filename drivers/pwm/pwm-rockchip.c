// SPDX-License-Identifier: GPL-2.0-only
/*
 * PWM driver for Rockchip SoCs
 *
 * Copyright (C) 2014 Beniamino Galvani <b.galvani@gmail.com>
 * Copyright (C) 2014 Rockchip Electronics Co., Ltd.
 */

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/pwm-rockchip.h>
#include <linux/time.h>
#include <media/rc-core.h>
#include "pwm-rockchip-irq-callbacks.h"

#define PWM_MAX_CHANNEL_NUM	8

#define PWM_IR_TRANSMIT_BUFFER_SIZE	7

/*
 * regs for pwm v1-v3
 */
#define PWM_CTRL_TIMER_EN	(1 << 0)
#define PWM_CTRL_OUTPUT_EN	(1 << 3)

/* PWM_PERIOD_HPR */
#define PWM_PERIOD_HPR		0x4
/* PWM_DUTY_LPR */
#define PWM_DUTY_LPR		0x8
/* PWM_CTRL */
#define PWM_CTRL_V1		0xc
#define PWM_ENABLE		(1 << 0)
#define PWM_MODE_SHIFT		1
#define PWM_MODE_MASK		(0x3 << PWM_MODE_SHIFT)
#define PWM_ONESHOT		(0 << PWM_MODE_SHIFT)
#define PWM_CONTINUOUS		(1 << PWM_MODE_SHIFT)
#define PWM_CAPTURE		(2 << PWM_MODE_SHIFT)
#define PWM_DUTY_POSITIVE	(1 << 3)
#define PWM_DUTY_NEGATIVE	(0 << 3)
#define PWM_INACTIVE_NEGATIVE	(0 << 4)
#define PWM_INACTIVE_POSITIVE	(1 << 4)
#define PWM_POLARITY_MASK	(PWM_DUTY_POSITIVE | PWM_INACTIVE_POSITIVE)
#define PWM_OUTPUT_LEFT		(0 << 5)
#define PWM_OUTPUT_CENTER	(1 << 5)
#define PWM_LOCK_EN		(1 << 6)
#define PWM_LP_DISABLE		(0 << 8)
#define PWM_CLK_SEL_SHIFT	9
#define PWM_CLK_SEL_MASK	(1 << PWM_CLK_SEL_SHIFT)
#define PWM_SEL_NO_SCALED_CLOCK	(0 << PWM_CLK_SEL_SHIFT)
#define PWM_SEL_SCALED_CLOCK	(1 << PWM_CLK_SEL_SHIFT)
#define PWM_PRESCELE_SHIFT	12
#define PWM_PRESCALE_MASK	(0x3 << PWM_PRESCELE_SHIFT)
#define PWM_SCALE_SHIFT		16
#define PWM_SCALE_MASK		(0xff << PWM_SCALE_SHIFT)
#define PWM_ONESHOT_COUNT_SHIFT	24
#define PWM_ONESHOT_COUNT_MASK	(0xff << PWM_ONESHOT_COUNT_SHIFT)

/* PWM_INTSTS */
#define PWM_REG_INTSTS(n)	((3 - (n)) * 0x10 + 0x10)
/* PWM_INT_EN */
#define PWM_REG_INT_EN(n)	((3 - (n)) * 0x10 + 0x14)
#define PWM_CH_INT(n)		BIT(n)

/* PWM_CHANNEL_IO_CTRL */
#define PWM_REG_IO_CTRL(n)	((3 - (n)) * 0x10 + 0xa0)
#define PWM_FILTER_NUMBER_SHIFT	4
#define PWM_FILTER_NUMBER_MASK	(0x1ff << PWM_FILTER_NUMBER_SHIFT)
#define PWM_FILTER_ENABLE_MASK	0xf
#define PWM_FILTER_ENABLE(n)	BIT(n)

/*
 * regs for pwm v4
 */
#define HIWORD_UPDATE(v, l, h)	((((v) << (l)) & GENMASK((h), (l))) | (GENMASK(h, l) << 16))

/* VERSION_ID */
#define VERSION_ID			0x0
#define CHANNEL_NUM_SUPPORT_SHIFT	0
#define CHANNEL_NUM_SUPPORT_MASK	(0xf << CHANNEL_NUM_SUPPORT_SHIFT)
#define CHANNLE_INDEX_SHIFT		4
#define CHANNLE_INDEX_MASK		(0xf << CHANNLE_INDEX_SHIFT)
#define IR_TRANS_SUPPORT		BIT(8)
#define POWER_KEY_SUPPORT		BIT(9)
#define FREQ_METER_SUPPORT		BIT(10)
#define COUNTER_SUPPORT			BIT(11)
#define WAVE_SUPPORT			BIT(12)
#define FILTER_SUPPORT			BIT(13)
#define BIPHASIC_SUPPORT		BIT(14)
#define LEDC_SUPPORT			BIT(15)
#define MINOR_VERSION_SHIFT		16
#define MINOR_VERSION_MASK		(0xff << MINOR_VERSION_SHIFT)
#define MAIN_VERSION_SHIFT		24
#define MAIN_VERSION_MASK		(0xff << MAIN_VERSION_SHIFT)
/* ENABLE */
#define ENABLE				0x4
#define PWM_ENABLE_V4			(0x3 << 0)
#define PWM_CLK_EN(v)			HIWORD_UPDATE(v, 0, 0)
#define PWM_EN(v)			HIWORD_UPDATE(v, 1, 1)
#define PWM_CTRL_UPDATE_EN(v)		HIWORD_UPDATE(v, 2, 2)
#define PWM_GLOBAL_JOIN_EN(v)		HIWORD_UPDATE(v, 4, 4)
/* CLK_CTRL */
#define CLK_CTRL			0x8
#define CLK_PRESCALE(v)			HIWORD_UPDATE(v, 0, 2)
#define CLK_SCALE(v)			HIWORD_UPDATE(v, 4, 12)
#define CLK_SRC_SEL(v)			HIWORD_UPDATE(v, 13, 14)
#define SRC_CLK_PWM			0
#define SRC_CLK_PWM_OSC			1
#define SRC_CLK_PWM_RC			2
#define CLK_GLOBAL_SEL(v)		HIWORD_UPDATE(v, 15, 15)
/* CTRL */
#define CTRL_V4				0xc
#define PWM_MODE(v)			HIWORD_UPDATE(v, 0, 1)
#define ONESHOT_MODE			0
#define CONTINUOUS_MODE			1
#define CAPTURE_MODE			2
#define PWM_POLARITY(v)			HIWORD_UPDATE(v, 2, 3)
#define DUTY_NEGATIVE			(0 << 0)
#define DUTY_POSITIVE			(1 << 0)
#define INACTIVE_NEGATIVE		(0 << 1)
#define INACTIVE_POSITIVE		(1 << 1)
#define PWM_ALIGNED_INVALID(v)		HIWORD_UPDATE(v, 5, 5)
#define PWM_IN_SEL(v)			HIWORD_UPDATE(v, 6, 8)
/* PERIOD */
#define PERIOD				0x10
/* DUTY */
#define DUTY				0x14
/* OFFSET */
#define OFFSET				0x18
/* RPT */
#define RPT				0x1c
#define FIRST_DIMENSIONAL_SHIFT		0
#define SECOND_DIMENSINAL_SHIFT		16
/* FILTER_CTRL */
#define FILTER_CTRL			0x20
#define FILTER_ENABLE(v)		HIWORD_UPDATE(v, 0, 0)
#define FILTER_NUMBER(v)		HIWORD_UPDATE(v, 4, 9)
/* HPC */
#define HPC				0x2c
/* LPC */
#define LPC				0x30
/* BIPHASIC_COUNTER_CTRL0 */
#define BIPHASIC_CTRL0			0x40
#define BIPHASIC_EN(v)			HIWORD_UPDATE(v, 0, 0)
#define BIPHASIC_CONTINOUS_MODE_EN(v)	HIWORD_UPDATE(v, 1, 1)
#define BIPHASIC_MODE(v)		HIWORD_UPDATE(v, 3, 5)
#define BIPHASIC_SYNC_EN(v)		HIWORD_UPDATE(v, 7, 7)
/* BIPHASIC_COUNTER_CTRL1 */
#define BIPHASIC_CTRL1			0x44
/* BIPHASIC_COUNTER_TIMER_VALUE */
#define BIPHASIC_TIMER_VALUE		0x48
/* BIPHASIC_COUNTER_RESULT_VALUE */
#define BIPHASIC_RESULT_VALUE		0x4c
/* BIPHASIC_COUNTER_RESULT_VALUE_SYNC */
#define BIPHASIC_RESULT_VALUE_SYNC	0x50
/* INTSTS*/
#define INTSTS				0x70
#define CAP_LPR_INTSTS_SHIFT		0
#define CAP_HPR_INTSTS_SHIFT		1
#define ONESHOT_END_INTSTS_SHIFT	2
#define RELOAD_INTSTS_SHIFT		3
#define FREQ_INTSTS_SHIFT		4
#define PWR_INTSTS_SHIFT		5
#define IR_TRANS_END_INTSTS_SHIFT	6
#define WAVE_MAX_INTSTS_SHIFT		7
#define WAVE_MIDDLE_INTSTS_SHIFT	8
#define BIPHASIC_INISTS_SHIFT		9
#define CAP_LPR_INT			BIT(CAP_LPR_INTSTS_SHIFT)
#define CAP_HPR_INT			BIT(CAP_HPR_INTSTS_SHIFT)
#define ONESHOT_END_INT			BIT(ONESHOT_END_INTSTS_SHIFT)
#define RELOAD_INT			BIT(RELOAD_INTSTS_SHIFT)
#define FREQ_INT			BIT(FREQ_INTSTS_SHIFT)
#define PWR_INT				BIT(PWR_INTSTS_SHIFT)
#define IR_TRANS_END_INT		BIT(IR_TRANS_END_INTSTS_SHIFT)
#define WAVE_MAX_INT			BIT(WAVE_MAX_INTSTS_SHIFT)
#define WAVE_MIDDLE_INT			BIT(WAVE_MIDDLE_INTSTS_SHIFT)
#define BIPHASIC_INT			BIT(BIPHASIC_INISTS_SHIFT)
/* INT_EN */
#define INT_EN				0x74
#define CAP_LPR_INT_EN(v)		HIWORD_UPDATE(v, 0, 0)
#define CAP_HPR_INT_EN(v)		HIWORD_UPDATE(v, 1, 1)
#define ONESHOT_END_INT_EN(v)		HIWORD_UPDATE(v, 2, 2)
#define RELOAD_INT_EN(v)		HIWORD_UPDATE(v, 3, 3)
#define FREQ_INT_EN(v)			HIWORD_UPDATE(v, 4, 4)
#define PWR_INT_EN(v)			HIWORD_UPDATE(v, 5, 5)
#define IR_TRANS_END_INT_EN(v)		HIWORD_UPDATE(v, 6, 6)
#define WAVE_MAX_INT_EN(v)		HIWORD_UPDATE(v, 7, 7)
#define WAVE_MIDDLE_INT_EN(v)		HIWORD_UPDATE(v, 8, 8)
#define BIPHASIC_INT_EN(v)		HIWORD_UPDATE(v, 9, 9)
/* FEATURE */
#define FEATURE				0x7c
/* WAVE_MEM_ARBITER */
#define WAVE_MEM_ARBITER		0x80
#define WAVE_MEM_GRANT_SHIFT		0
#define WAVE_MEM_READ_LOCK_SHIFT	16
/* WAVE_MEM_STATUS */
#define WAVE_MEM_STATUS			0x84
#define WAVE_MEM_STATUS_SHIFT		0
/* WAVE_CTRL */
#define WAVE_CTRL			0x88
#define WAVE_DUTY_EN(v)			HIWORD_UPDATE(v, 0, 0)
#define WAVE_PERIOD_EN(v)		HIWORD_UPDATE(v, 1, 1)
#define WAVE_WIDTH_MODE(v)		HIWORD_UPDATE(v, 2, 2)
#define WAVE_UPDATE_MODE(v)		HIWORD_UPDATE(v, 3, 3)
#define WAVE_MEM_CLK_SEL(v)		HIWORD_UPDATE(v, 4, 5)
#define WAVE_DUTY_AMPLIFY(v)		HIWORD_UPDATE(v, 6, 10)
#define WAVE_PERIOD_AMPLIFY(v)		HIWORD_UPDATE(v, 11, 15)
/* WAVE_MAX */
#define WAVE_MAX			0x8c
#define WAVE_DUTY_MAX_SHIFT		0
#define WAVE_PERIOD_MAX_SHIFT		16
/* WAVE_MIN */
#define WAVE_MIN			0x90
#define WAVE_DUTY_MIN_SHIFT		0
#define WAVE_PERIOD_MIN_SHIFT		16
/* WAVE_OFFSET */
#define WAVE_OFFSET			0x94
#define WAVE_OFFSET_SHIFT		0
/* WAVE_MIDDLE */
#define WAVE_MIDDLE			0x98
#define WAVE_MIDDLE_SHIFT		0
/* WAVE_HOLD */
#define WAVE_HOLD			0x9c
#define MAX_HOLD_SHIFT			0
#define MIN_HOLD_SHIFT			8
#define MIDDLE_HOLD_SHIFT		16
/* GLOBAL_ARBITER */
#define GLOBAL_ARBITER			0xc0
#define GLOBAL_GRANT_SHIFT		0
#define GLOBAL_READ_LOCK_SHIFT		16
/* GLOBAL_CTRL */
#define GLOBAL_CTRL			0xc4
#define GLOBAL_PWM_EN(v)		HIWORD_UPDATE(v, 0, 0)
#define GLOBAL_PWM_UPDATE_EN(v)		HIWORD_UPDATE(v, 1, 1)
/* IR_TRANS_ARBITER */
#define IR_TRANS_ARBITER		0x180
#define IR_TRANS_GRANT_SHIFT		0
#define IR_TRANS_READ_LOCK_SHIFT	16
/* IR_TRANS_CTRL0 */
#define IR_TRANS_CTRL0			0x184
#define IR_TRANS_OUT_ENABLE(v)		HIWORD_UPDATE(v, 0, 0)
#define IR_TRANS_DUTY_POL(v)		HIWORD_UPDATE(v, 1, 1)
#define IR_TRANS_INACTIVE_POL(v)	HIWORD_UPDATE(v, 2, 2)
#define IR_TRANS_MODE(v)		HIWORD_UPDATE(v, 3, 3)
#define IR_TRANS_FORMAT(v)		HIWORD_UPDATE(v, 4, 7)
#define NEC_WITH_SIMPLE_REPEAT_CODE	0
#define NEC_WITH_FULL_REPEAT_CODE	1
#define TC9012				2
#define SONY				3
/* IR_TRANS_CTRL1 */
#define IR_TRANS_CTRL1			0x188
#define IR_TRANS_RPT(v)			HIWORD_UPDATE(v, 0, 15)
/* IR_TRANS_PRE */
#define IR_TRANS_PRE			0x18c
#define IR_TRANS_OUT_LOW_PRELOAD_SHIFT	0
#define IR_TRANS_OUT_HIGH_PRELOAD_SHIFT	16
/* IR_TRANS_SPRE */
#define IR_TRANS_SPRE			0x190
#define IR_TRANS_OUT_HIGH_SIMPLE_PRELOAD_SHIFT	0
/* IR_TRANS_LD */
#define IR_TRANS_LD			0x194
#define IR_TRANS_OUT_DATA_LOW_PERIOD_SHIFT	0
/* IR_TRANS_HD */
#define IR_TRANS_HD			0x198
#define IR_TRANS_OUT_HIGH_PERIOD_FOR_ZERO_SHIFT	0
#define IR_TRANS_OUT_HIGH_PERIOD_FOR_ONE_SHIFT	16
/* IR_TRANS_BURST_FRAME */
#define IR_TRANS_BURST_FRAME		0x19c
#define IR_TRANS_OUT_FRAME_PERIOD_SHIFT	0
#define IR_TRANS_OUT_FRAME_PERIOD_MASK	(0x3ffff << IR_TRANS_OUT_FRAME_PERIOD_SHIFT)
#define IR_TRANS_OUT_BURST_PERIOD_SHIFT	20
/* IR_TRANS_DATA_VALUE */
#define IR_TRANS_DATA_VALUE		0x1a0
#define IR_TRANS_OUT_VALUE_SHIFT	0
/* FREQ_ARBITER */
#define FREQ_ARBITER			0x1c0
#define FREQ_GRANT_SHIFT		0
#define FREQ_READ_LOCK_SHIFT		16
/* FREQ_CTRL */
#define FREQ_CTRL			0x1c4
#define FREQ_EN(v)			HIWORD_UPDATE(v, 0, 0)
#define FREQ_CLK_SEL(v)			HIWORD_UPDATE(v, 1, 1)
#define FREQ_CHANNEL_SEL(v)		HIWORD_UPDATE(v, 3, 5)
#define FREQ_CLK_SWITCH_MODE(v)		HIWORD_UPDATE(v, 6, 6)
#define FREQ_TIMIER_CLK_SEL(v)		HIWORD_UPDATE(v, 7, 7)
/* FREQ_TIMER_VALUE */
#define FREQ_TIMER_VALUE		0x1c8
/* FREQ_RESULT_VALUE */
#define FREQ_RESULT_VALUE		0x1cc
/* COUNTER_ARBITER */
#define COUNTER_ARBITER			0x200
#define COUNTER_GRANT_SHIFT		0
#define COUNTER_READ_LOCK_SHIFT		16
/* COUNTER_CTRL */
#define COUNTER_CTRL			0x204
#define COUNTER_EN(v)			HIWORD_UPDATE(v, 0, 0)
#define COUNTER_CLK_SEL(v)		HIWORD_UPDATE(v, 1, 2)
#define COUNTER_CHANNEL_SEL(v)		HIWORD_UPDATE(v, 3, 5)
#define COUNTER_CLR(v)			HIWORD_UPDATE(v, 6, 6)
/* COUNTER_LOW */
#define COUNTER_LOW			0x208
/* COUNTER_HIGH */
#define COUNTER_HIGH			0x20c
/* WAVE_MEM */
#define WAVE_MEM			0x400

struct rockchip_pwm_chip {
	struct pwm_chip chip;
	struct clk *clk;
	struct clk *pclk;
	struct clk *clk_osc;
	struct pinctrl *pinctrl;
	struct pinctrl_state *active_state;
	struct delayed_work pwm_work;
	const struct rockchip_pwm_data *data;
	const struct rockchip_pwm_biphasic_config *biphasic_config;
	struct resource *res;
	struct dentry *debugfs;
	struct completion ir_trans_completion;
	struct completion freq_meter_completion;
	struct completion biphasic_completion;
	void __iomem *base;
	unsigned long clk_rate;
	unsigned long is_clk_enabled;
	bool vop_pwm_en; /* indicate voppwm mirror register state */
	bool center_aligned;
	bool oneshot_valid;
	bool oneshot_en;
	bool capture_en;
	bool wave_en;
	bool global_ctrl_grant;
	bool ir_trans_support;
	bool freq_meter_support;
	bool counter_support;
	bool wave_support;
	bool filter_support;
	bool biphasic_support;
	bool ledc_support;
	int channel_id;
	int irq;
	u64 filter_window_ns;
	u32 scaler;
	u8 main_version;
	u8 minor_version;
	u8 capture_cnt;
};

struct rockchip_pwm_regs {
	unsigned long duty;
	unsigned long period;
	unsigned long ctrl;
	unsigned long version;
	unsigned long enable;
};

struct rockchip_pwm_funcs {
	int (*enable)(struct pwm_chip *chip, struct pwm_device *pwm, bool enable);
	void (*config)(struct pwm_chip *chip, struct pwm_device *pwm,
		       const struct pwm_state *state);
	void (*set_capture)(struct pwm_chip *chip, struct pwm_device *pwm, bool enable);
	int (*get_capture_result)(struct pwm_chip *chip, struct pwm_device *pwm,
				  struct pwm_capture *catpure_res);
	int (*set_counter)(struct pwm_chip *chip, struct pwm_device *pwm,
			   enum rockchip_pwm_counter_input_sel input_sel, bool enable);
	int (*get_counter_result)(struct pwm_chip *chip, struct pwm_device *pwm,
				  unsigned long *counter_res, bool is_clear);
	int (*set_freq_meter)(struct pwm_chip *chip, struct pwm_device *pwm,
			      unsigned long delay_ms,
			      enum rockchip_pwm_freq_meter_input_sel input_sel,
			      bool enable);
	int (*get_freq_meter_result)(struct pwm_chip *chip, struct pwm_device *pwm,
				     unsigned long delay_ms, unsigned long *freq_hz);
	int (*global_ctrl)(struct pwm_chip *chip, struct pwm_device *pwm,
			   enum rockchip_pwm_global_ctrl_cmd cmd);
	int (*set_wave_table)(struct pwm_chip *chip, struct pwm_device *pwm,
			      struct rockchip_pwm_wave_table *table_config,
			      enum rockchip_pwm_wave_table_width_mode width_mode);
	int (*set_wave)(struct pwm_chip *chip, struct pwm_device *pwm,
			struct rockchip_pwm_wave_config *config);
	int (*set_biphasic)(struct pwm_chip *chip, struct pwm_device *pwm,
			    struct rockchip_pwm_biphasic_config *config);
	int (*get_biphasic_result)(struct pwm_chip *chip, struct pwm_device *pwm,
				   unsigned long *biphasic_res);
	int (*ir_transmit)(struct pwm_chip *chip, unsigned int *txbuf, unsigned int count);
	irqreturn_t (*irq_handler)(int irq, void *data);
	int (*set_filter)(struct pwm_chip *chip, struct pwm_device *pwm, u64 filter_window_ns);
};

struct rockchip_pwm_data {
	struct rockchip_pwm_regs regs;
	struct rockchip_pwm_funcs funcs;
	unsigned int prescaler;
	bool supports_polarity;
	bool supports_lock;
	bool vop_pwm;
	u8 main_version;
	u32 enable_conf;
	u32 enable_conf_mask;
	u32 oneshot_cnt_max;
	u32 oneshot_rpt_max;
	u32 wave_table_max;
};

static inline struct rockchip_pwm_chip *to_rockchip_pwm_chip(struct pwm_chip *c)
{
	return container_of(c, struct rockchip_pwm_chip, chip);
}

static int rockchip_pwm_get_state(struct pwm_chip *chip,
				  struct pwm_device *pwm,
				  struct pwm_state *state)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	u32 enable_conf = pc->data->enable_conf;
	u64 clk_rate_kHz = pc->clk_rate / 1000;
	u64 tmp;
	u32 scaler = pc->scaler ? pc->scaler * 2 : 1;
	u32 val;
	u32 dclk_div = 1;
	int ret;

	if (!pc->oneshot_en) {
		ret = clk_enable(pc->pclk);
		if (ret)
			return 0;
	}

	if (pc->main_version < 4)
		dclk_div = pc->oneshot_en ? 2 : 1;

	tmp = readl_relaxed(pc->base + pc->data->regs.period);
	tmp *= dclk_div * pc->data->prescaler * scaler * USEC_PER_SEC;
	state->period = DIV_ROUND_CLOSEST_ULL(tmp, clk_rate_kHz);

	tmp = readl_relaxed(pc->base + pc->data->regs.duty);
	tmp *= dclk_div * pc->data->prescaler * scaler * USEC_PER_SEC;
	state->duty_cycle = DIV_ROUND_CLOSEST_ULL(tmp, clk_rate_kHz);

	if (pc->main_version >= 4) {
		val = readl_relaxed(pc->base + pc->data->regs.enable);
	} else {
		val = readl_relaxed(pc->base + pc->data->regs.ctrl);
		if (pc->oneshot_en) {
			enable_conf &= ~PWM_MODE_MASK;
			enable_conf |= PWM_ONESHOT;
		} else if (pc->capture_en) {
			enable_conf &= ~PWM_MODE_MASK;
			enable_conf |= PWM_CAPTURE;
		}
	}
	state->enabled = (val & enable_conf) == enable_conf;

	if (pc->data->supports_polarity && !(val & PWM_DUTY_POSITIVE))
		state->polarity = PWM_POLARITY_INVERSED;
	else
		state->polarity = PWM_POLARITY_NORMAL;

	if (!pc->oneshot_en)
		clk_disable(pc->pclk);

	return 0;
}

static irqreturn_t rockchip_pwm_irq_v1(int irq, void *data)
{
	struct rockchip_pwm_chip *pc = data;
	struct pwm_state state;
	u32 int_ctrl;
	unsigned int id = pc->channel_id;
	int val;

	if (id > 3)
		return IRQ_NONE;
	val = readl_relaxed(pc->base + PWM_REG_INTSTS(id));

	if ((val & PWM_CH_INT(id)) == 0)
		return IRQ_NONE;

	writel_relaxed(PWM_CH_INT(id), pc->base + PWM_REG_INTSTS(id));

	if (pc->oneshot_en) {
		/*
		 * Set pwm state to disabled when the oneshot mode finished.
		 */
		pwm_get_state(&pc->chip.pwms[0], &state);
		state.enabled = false;
		pwm_apply_state(&pc->chip.pwms[0], &state);

		rockchip_pwm_oneshot_callback(&pc->chip.pwms[0], &state);
	} else if (pc->capture_en) {
		/*
		 * Capture input waveform:
		 *    _______                 _______
		 *   |       |               |       |
		 * __|       |_______________|       |________
		 *   ^0      ^1              ^2
		 *
		 * At position 0, the interrupt comes, and DUTY_LPR reg shows the
		 * low polarity cycles which should be ignored. The effective high
		 * and low polarity cycles will be calculated in position 1 and
		 * position 2, where the interrupt comes.
		 */
		if (pc->capture_cnt++ > 3) {
			int_ctrl = readl_relaxed(pc->base + PWM_REG_INT_EN(pc->channel_id));
			int_ctrl &= ~PWM_CH_INT(pc->channel_id);
			writel_relaxed(int_ctrl, pc->base + PWM_REG_INT_EN(pc->channel_id));
		}
	}

	return IRQ_HANDLED;
}

static void rockchip_pwm_config_v1(struct pwm_chip *chip, struct pwm_device *pwm,
				   const struct pwm_state *state)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	unsigned long period, duty, delay_ns;
	unsigned long flags;
	u64 div;
	u32 ctrl;
	u8 dclk_div = 1;

#ifdef CONFIG_PWM_ROCKCHIP_ONESHOT
	if (state->oneshot_count > 0 && state->oneshot_count <= pc->data->oneshot_cnt_max)
		dclk_div = 2;
#endif

	/*
	 * Since period and duty cycle registers have a width of 32
	 * bits, every possible input period can be obtained using the
	 * default prescaler value for all practical clock rate values.
	 */
	div = (u64)pc->clk_rate * state->period;
	period = DIV_ROUND_CLOSEST_ULL(div, dclk_div * pc->data->prescaler * NSEC_PER_SEC);

	div = (u64)pc->clk_rate * state->duty_cycle;
	duty = DIV_ROUND_CLOSEST_ULL(div, dclk_div * pc->data->prescaler * NSEC_PER_SEC);

	if (pc->data->supports_lock) {
		div = (u64)10 * NSEC_PER_SEC * dclk_div * pc->data->prescaler;
		delay_ns = DIV_ROUND_UP_ULL(div, pc->clk_rate);
	}

	local_irq_save(flags);

	ctrl = readl_relaxed(pc->base + PWM_CTRL_V1);
	if (pc->data->vop_pwm) {
		if (pc->vop_pwm_en)
			ctrl |= PWM_ENABLE;
		else
			ctrl &= ~PWM_ENABLE;
	}

#ifdef CONFIG_PWM_ROCKCHIP_ONESHOT
	if (state->oneshot_count > 0 && state->oneshot_count <= pc->data->oneshot_cnt_max) {
		u32 int_ctrl;

		/*
		 * This is a workaround, an uncertain waveform will be
		 * generated after oneshot ends. It is needed to enable
		 * the dclk scale function to resolve it. It doesn't
		 * matter what the scale factor is, just make sure the
		 * scale function is turned on, for which we set scale
		 * factor to 2.
		 */
		ctrl &= ~PWM_SCALE_MASK;
		ctrl |= (dclk_div / 2) << PWM_SCALE_SHIFT;
		ctrl &= ~PWM_CLK_SEL_MASK;
		ctrl |= PWM_SEL_SCALED_CLOCK;

		pc->oneshot_valid = true;
		ctrl &= ~PWM_MODE_MASK;
		ctrl |= PWM_ONESHOT;

		ctrl &= ~PWM_ONESHOT_COUNT_MASK;
		ctrl |= (state->oneshot_count - 1) << PWM_ONESHOT_COUNT_SHIFT;

		if (pc->irq >= 0) {
			int_ctrl = readl_relaxed(pc->base + PWM_REG_INT_EN(pc->channel_id));
			int_ctrl |= PWM_CH_INT(pc->channel_id);
			writel_relaxed(int_ctrl, pc->base + PWM_REG_INT_EN(pc->channel_id));
		}
	} else {
		u32 int_ctrl;

		ctrl &= ~PWM_SCALE_MASK;
		ctrl &= ~PWM_CLK_SEL_MASK;
		ctrl |= PWM_SEL_NO_SCALED_CLOCK;

		if (state->oneshot_count)
			dev_err(chip->dev, "Oneshot_count must be between 1 and %d.\n",
				pc->data->oneshot_cnt_max);

		pc->oneshot_valid = false;
		ctrl &= ~PWM_MODE_MASK;
		ctrl |= PWM_CONTINUOUS;

		ctrl &= ~PWM_ONESHOT_COUNT_MASK;

		int_ctrl = readl_relaxed(pc->base + PWM_REG_INT_EN(pc->channel_id));
		int_ctrl &= ~PWM_CH_INT(pc->channel_id);
		writel_relaxed(int_ctrl, pc->base + PWM_REG_INT_EN(pc->channel_id));
	}
#endif

	/*
	 * Lock the period and duty of previous configuration, then
	 * change the duty and period, that would not be effective.
	 */
	if (pc->data->supports_lock) {
		ctrl |= PWM_LOCK_EN;
		writel_relaxed(ctrl, pc->base + PWM_CTRL_V1);
	}

	writel(period, pc->base + PWM_PERIOD_HPR);
	writel(duty, pc->base + PWM_DUTY_LPR);

	if (pc->data->supports_polarity) {
		ctrl &= ~PWM_POLARITY_MASK;
		if (state->polarity == PWM_POLARITY_INVERSED)
			ctrl |= PWM_DUTY_NEGATIVE | PWM_INACTIVE_POSITIVE;
		else
			ctrl |= PWM_DUTY_POSITIVE | PWM_INACTIVE_NEGATIVE;
	}

	/*
	 * Unlock and set polarity at the same time, the configuration of duty,
	 * period and polarity would be effective together at next period. It
	 * takes 10 dclk cycles to make sure lock works before unlocking.
	 */
	if (pc->data->supports_lock) {
		ctrl &= ~PWM_LOCK_EN;
		ndelay(delay_ns);
	}

	writel(ctrl, pc->base + PWM_CTRL_V1);
	local_irq_restore(flags);
}

static int rockchip_pwm_enable_v1(struct pwm_chip *chip, struct pwm_device *pwm, bool enable)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	u32 enable_conf = pc->data->enable_conf;
	int ret;
	u32 val;

	if (enable) {
		ret = clk_enable(pc->clk);
		if (ret)
			return ret;
	}

	val = readl_relaxed(pc->base + PWM_CTRL_V1);
	val &= ~pc->data->enable_conf_mask;

	if (PWM_OUTPUT_CENTER & pc->data->enable_conf_mask) {
		if (pc->center_aligned)
			val |= PWM_OUTPUT_CENTER;
	}

	if (pc->oneshot_valid) {
		enable_conf &= ~PWM_MODE_MASK;
		enable_conf |= PWM_ONESHOT;
	} else if (pc->capture_en) {
		enable_conf &= ~PWM_MODE_MASK;
		enable_conf |= PWM_CAPTURE;
	}

	if (enable) {
		val |= enable_conf;
	} else {
		/*
		 * The PWM io input/output state is controlled by PWM mode
		 * configuration. In order to avoid the antagonistic drive
		 * state between the PWM pin and the external pin, keep the
		 * PWM mode fixed in capture mode although PWM is disabled.
		 */
		if (pc->capture_en)
			val |= PWM_CAPTURE;
	}

	writel_relaxed(val, pc->base + PWM_CTRL_V1);
	if (pc->data->vop_pwm)
		pc->vop_pwm_en = enable;

	if (!enable)
		clk_disable(pc->clk);

	pc->oneshot_en = pc->oneshot_valid ? enable : false;

	return 0;
}

static irqreturn_t rockchip_pwm_irq_v4(int irq, void *data)
{
	struct rockchip_pwm_chip *pc = data;
	int val;
	irqreturn_t ret = IRQ_NONE;

	val = readl_relaxed(pc->base + INTSTS);
#ifdef CONFIG_PWM_ROCKCHIP_ONESHOT
	if (val & ONESHOT_END_INT) {
		struct pwm_state state;

		writel_relaxed(ONESHOT_END_INT, pc->base + INTSTS);

		/*
		 * Set pwm state to disabled when the oneshot mode finished.
		 */
		pwm_get_state(&pc->chip.pwms[0], &state);
		state.enabled = false;
		state.oneshot_count = 0;
		state.oneshot_repeat = 0;
		pwm_apply_state(&pc->chip.pwms[0], &state);

		rockchip_pwm_oneshot_callback(&pc->chip.pwms[0], &state);

		ret = IRQ_HANDLED;
	}
#endif
	if (val & CAP_LPR_INT) {
		writel_relaxed(CAP_LPR_INT, pc->base + INTSTS);
		pc->capture_cnt++;

		ret = IRQ_HANDLED;
	} else if (val & CAP_HPR_INT) {
		writel_relaxed(CAP_HPR_INT, pc->base + INTSTS);
		pc->capture_cnt++;

		ret = IRQ_HANDLED;
	}

	/*
	 * Capture input waveform:
	 *    _______                 _______
	 *   |       |               |       |
	 * __|       |_______________|       |________
	 *   ^0      ^1              ^2
	 *
	 * At position 0, the LPR interrupt comes, and LPR reg shows the
	 * low polarity cycles which should be ignored. The effective high
	 * and low polarity cycles will be calculated in position 1 and
	 * position 2, where the HPR and LPR interrupts come again.
	 */
	if (pc->capture_cnt > 3) {
		writel_relaxed(CAP_LPR_INT | CAP_HPR_INT, pc->base + INTSTS);
		writel_relaxed(CAP_LPR_INT_EN(false) | CAP_HPR_INT_EN(false), pc->base + INT_EN);
	}

	if (val & FREQ_INT) {
		writel_relaxed(FREQ_INT, pc->base + INTSTS);
		complete(&pc->freq_meter_completion);

		ret = IRQ_HANDLED;
	}

	if (val & BIPHASIC_INT) {
		writel_relaxed(BIPHASIC_INT, pc->base + INTSTS);
		complete(&pc->biphasic_completion);

		ret = IRQ_HANDLED;
	}

	if (val & WAVE_MIDDLE_INT) {
		writel_relaxed(WAVE_MIDDLE_INT, pc->base + INTSTS);

		rockchip_pwm_wave_middle_callback(&pc->chip.pwms[0]);

		ret = IRQ_HANDLED;
	}

	if (val & WAVE_MAX_INT) {
		writel_relaxed(WAVE_MAX_INT, pc->base + INTSTS);

		rockchip_pwm_wave_max_callback(&pc->chip.pwms[0]);

		ret = IRQ_HANDLED;
	}

	if (val & IR_TRANS_END_INT) {
		writel_relaxed(IR_TRANS_END_INT, pc->base + INTSTS);
		complete(&pc->ir_trans_completion);

		ret = IRQ_HANDLED;
	}

	return ret;
}

static void rockchip_pwm_config_v4(struct pwm_chip *chip, struct pwm_device *pwm,
				   const struct pwm_state *state)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	unsigned long period, duty;
	u64 clk_rate_kHz = pc->clk_rate / 1000;
	u64 div = 0;
	u64 tmp = 0;
	u32 scaler = pc->scaler ? pc->scaler * 2 : 1;
	u32 rpt = 0;
	u32 offset = 0;

	tmp = (u64)pc->data->prescaler * scaler * USEC_PER_SEC;
	/*
	 * Since period and duty cycle registers have a width of 32
	 * bits, every possible input period can be obtained using the
	 * default prescaler value for all practical clock rate values.
	 */
	div = (u64)clk_rate_kHz * state->period;
	period = DIV_ROUND_CLOSEST_ULL(div, tmp);

	div = (u64)clk_rate_kHz * state->duty_cycle;
	duty = DIV_ROUND_CLOSEST_ULL(div, tmp);

	writel_relaxed(period, pc->base + PERIOD);
	writel_relaxed(duty, pc->base + DUTY);

	if (pc->data->supports_polarity) {
		if (state->polarity == PWM_POLARITY_INVERSED)
			writel_relaxed(PWM_POLARITY(DUTY_NEGATIVE | INACTIVE_POSITIVE),
				       pc->base + CTRL_V4);
		else
			writel_relaxed(PWM_POLARITY(DUTY_POSITIVE | INACTIVE_NEGATIVE),
				       pc->base + CTRL_V4);
	}

#ifdef CONFIG_PWM_ROCKCHIP_ONESHOT
	if ((state->oneshot_count > 0 && state->oneshot_count <= pc->data->oneshot_cnt_max) &&
	    (state->oneshot_repeat <= pc->data->oneshot_rpt_max)) {
		rpt |= (state->oneshot_count - 1) << FIRST_DIMENSIONAL_SHIFT;
		if (state->oneshot_repeat)
			rpt |= (state->oneshot_repeat - 1) << SECOND_DIMENSINAL_SHIFT;

		if (state->duty_offset > 0 &&
		    state->duty_offset <= (state->period - state->duty_cycle)) {
			div = (u64)clk_rate_kHz * state->duty_offset;
			offset = DIV_ROUND_CLOSEST_ULL(div, tmp);
		} else if (state->duty_offset > (state->period - state->duty_cycle)) {
			dev_err(chip->dev, "Duty_offset must be between %lld and %lld.\n",
				state->duty_cycle, state->period);
		}

		pc->oneshot_valid = true;
	} else {
		if (state->oneshot_count)
			dev_err(chip->dev, "Oneshot_count must be between 1 and %d.\n",
				pc->data->oneshot_cnt_max);

		pc->oneshot_valid = false;
	}
#endif

	if (pc->oneshot_valid) {
		writel_relaxed(PWM_MODE(ONESHOT_MODE) | PWM_ALIGNED_INVALID(true),
			       pc->base + CTRL_V4);
		writel_relaxed(offset, pc->base + OFFSET);
		writel_relaxed(rpt, pc->base + RPT);
		writel_relaxed(ONESHOT_END_INT_EN(true), pc->base + INT_EN);
	} else {
		writel_relaxed(PWM_MODE(CONTINUOUS_MODE) | PWM_ALIGNED_INVALID(false),
			       pc->base + CTRL_V4);
		writel_relaxed(0, pc->base + OFFSET);
		if (!pc->wave_en)
			writel_relaxed(0, pc->base + RPT);
		writel_relaxed(ONESHOT_END_INT_EN(false), pc->base + INT_EN);
	}

	writel_relaxed(PWM_CTRL_UPDATE_EN(true), pc->base + ENABLE);
}

static int rockchip_pwm_enable_v4(struct pwm_chip *chip, struct pwm_device *pwm, bool enable)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	struct pwm_state curstate;
	unsigned long delay_us;
	int ret;

	if (enable) {
		ret = clk_enable(pc->clk);
		if (ret)
			return ret;
	}

	writel_relaxed(PWM_EN(enable) | PWM_CLK_EN(enable), pc->base + ENABLE);

	/*
	 * For pwm v4, the disable operation of continuous mode, which sets polarity
	 * to inactive state, will not take effect until the end of current period.
	 * Therefore, it makes sense to delay one period before disabling the dclk.
	 */
	if (!enable && !pc->oneshot_en) {
		pwm_get_state(pwm, &curstate);
		delay_us = DIV_ROUND_UP_ULL(curstate.period, NSEC_PER_USEC);
		fsleep(delay_us);
		clk_disable(pc->clk);
	}

	pc->oneshot_en = pc->oneshot_valid ? enable : false;

	return 0;
}

static void rockchip_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
				const struct pwm_state *state)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);

	pc->data->funcs.config(chip, pwm, state);
}

static int rockchip_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm, bool enable)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);

	return pc->data->funcs.enable(chip, pwm, enable);
}

static int rockchip_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			      const struct pwm_state *state)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	struct pwm_state curstate;
	bool enabled;
	int ret = 0;

	if (!pc->oneshot_en) {
		ret = clk_enable(pc->pclk);
		if (ret)
			return ret;
	}

	pwm_get_state(pwm, &curstate);
	enabled = curstate.enabled;

	if (state->polarity != curstate.polarity && enabled &&
	    !pc->data->supports_lock) {
		ret = rockchip_pwm_enable(chip, pwm, false);
		if (ret)
			goto out;
		enabled = false;
	}

	rockchip_pwm_config(chip, pwm, state);
	if (state->enabled != enabled) {
		ret = rockchip_pwm_enable(chip, pwm, state->enabled);
		if (ret)
			goto out;
	}

	if (state->enabled)
		ret = pinctrl_select_state(pc->pinctrl, pc->active_state);
out:
	if (!pc->oneshot_en)
		clk_disable(pc->pclk);

	return ret;
}

static void rockchip_pwm_set_capture_v1(struct pwm_chip *chip, struct pwm_device *pwm,
					bool enable)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	u32 int_ctrl;

	int_ctrl = readl_relaxed(pc->base + PWM_REG_INT_EN(pc->channel_id));
	if (enable)
		int_ctrl |= PWM_CH_INT(pc->channel_id);
	else
		int_ctrl &= ~PWM_CH_INT(pc->channel_id);
	writel_relaxed(int_ctrl, pc->base + PWM_REG_INT_EN(pc->channel_id));

	pc->capture_en = enable;
	pc->capture_cnt = 0;
}

static int rockchip_pwm_get_capture_result_v1(struct pwm_chip *chip, struct pwm_device *pwm,
					      struct pwm_capture *capture_res)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	u64 tmp;

	tmp = readl_relaxed(pc->base + PWM_PERIOD_HPR);
	tmp *= pc->data->prescaler * NSEC_PER_SEC;
	capture_res->duty_cycle = DIV_ROUND_CLOSEST_ULL(tmp, pc->clk_rate);

	tmp = readl_relaxed(pc->base + PWM_DUTY_LPR);
	tmp *= pc->data->prescaler * NSEC_PER_SEC;
	capture_res->period = DIV_ROUND_CLOSEST_ULL(tmp, pc->clk_rate) + capture_res->duty_cycle;

	if (!capture_res->duty_cycle || !capture_res->period)
		return -EINVAL;

	writel_relaxed(0, pc->base + PWM_PERIOD_HPR);
	writel_relaxed(0, pc->base + PWM_DUTY_LPR);

	return 0;
}

static void rockchip_pwm_set_capture_v4(struct pwm_chip *chip, struct pwm_device *pwm,
					bool enable)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	u32 channel_sel = 0;

	if (enable)
		channel_sel = pc->channel_id;

	pc->capture_cnt = 0;

	/*
	 * The PWM io input/output state is controlled by PWM mode
	 * configuration. In order to avoid the antagonistic drive
	 * state between the PWM pin and the external pin, keep the
	 * PWM mode fixed in capture mode although PWM is disabled.
	 */
	writel_relaxed(PWM_MODE(CAPTURE_MODE), pc->base + CTRL_V4);
	writel_relaxed(CAP_LPR_INT_EN(enable) | CAP_HPR_INT_EN(enable) | PWM_IN_SEL(channel_sel),
		       pc->base + INT_EN);
}

static int rockchip_pwm_get_capture_result_v4(struct pwm_chip *chip, struct pwm_device *pwm,
					      struct pwm_capture *capture_res)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	u64 tmp;

	tmp = readl_relaxed(pc->base + HPC);
	tmp *= pc->data->prescaler * NSEC_PER_SEC;
	capture_res->duty_cycle = DIV_ROUND_CLOSEST_ULL(tmp, pc->clk_rate);

	tmp = readl_relaxed(pc->base + LPC);
	tmp *= pc->data->prescaler * NSEC_PER_SEC;
	capture_res->period =  DIV_ROUND_CLOSEST_ULL(tmp, pc->clk_rate) + capture_res->duty_cycle;

	if (!capture_res->duty_cycle || !capture_res->period)
		return -EINVAL;

	writel_relaxed(0, pc->base + HPC);
	writel_relaxed(0, pc->base + LPC);

	return 0;
}

static int rockchip_pwm_capture(struct pwm_chip *chip, struct pwm_device *pwm,
				struct pwm_capture *capture_res, unsigned long timeout_ms)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	struct pwm_state curstate;
	int ret = 0;

	if (!pc->data->funcs.set_capture || !pc->data->funcs.get_capture_result) {
		dev_err(chip->dev, "Unsupported capture mode\n");
		return -EINVAL;
	}

	pwm_get_state(pwm, &curstate);
	if (curstate.enabled) {
		dev_err(chip->dev, "Failed to enable capture mode because PWM%d is busy\n",
			pc->channel_id);
		return -EBUSY;
	}

	ret = clk_enable(pc->pclk);
	if (ret)
		return ret;

	ret = pinctrl_select_state(pc->pinctrl, pc->active_state);
	if (ret) {
		dev_err(chip->dev, "Failed to select pinctrl state\n");
		goto err_disable_pclk;
	}

	pc->data->funcs.set_capture(chip, pwm, true);
	ret = pc->data->funcs.enable(chip, pwm, true);
	if (ret) {
		dev_err(chip->dev, "Failed to enable capture mode\n");
		goto err_disable_pclk;
	}

	usleep_range(timeout_ms * USEC_PER_MSEC, timeout_ms * USEC_PER_MSEC);

	if (pc->capture_cnt > 3) {
		ret = pc->data->funcs.get_capture_result(chip, pwm, capture_res);
		if (ret)
			dev_err(chip->dev, "Failed to get capture result\n");
	} else {
		dev_err(chip->dev, "Failed to wait for LPR/HPR interrupt\n");
		ret = -ETIMEDOUT;
	}

	pc->data->funcs.enable(chip, pwm, false);
	pc->data->funcs.set_capture(chip, pwm, false);

err_disable_pclk:
	clk_disable(pc->pclk);

	return ret;
}

static int rockchip_pwm_set_counter_v4(struct pwm_chip *chip, struct pwm_device *pwm,
				       enum rockchip_pwm_counter_input_sel input_sel,
				       bool enable)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	u32 arbiter = 0;
	u32 channel_sel = 0;
	u32 val;
	int ret;

	if (enable) {
		arbiter = BIT(pc->channel_id) << COUNTER_READ_LOCK_SHIFT |
			  BIT(pc->channel_id) << COUNTER_GRANT_SHIFT,
		channel_sel = pc->channel_id;
	}

	writel_relaxed(arbiter, pc->base + COUNTER_ARBITER);
	if (enable) {
		val = readl_relaxed(pc->base + COUNTER_ARBITER);
		if (!(val & arbiter))
			return -EINVAL;
	}

	if (enable) {
		ret = clk_enable(pc->clk);
		if (ret)
			return ret;
	}

	writel_relaxed(COUNTER_EN(enable) | COUNTER_CLK_SEL(input_sel) |
		       COUNTER_CHANNEL_SEL(channel_sel),
		       pc->base + COUNTER_CTRL);

	if (!enable)
		clk_disable(pc->clk);

	return 0;
}

int rockchip_pwm_set_counter(struct pwm_device *pwm,
			     enum rockchip_pwm_counter_input_sel input_sel,
			     bool enable)
{
	struct pwm_chip *chip;
	struct rockchip_pwm_chip *pc;
	struct pwm_state curstate;
	int ret = 0;

	if (!pwm)
		return -EINVAL;

	chip = pwm->chip;
	pc = to_rockchip_pwm_chip(chip);

	if (!pc->counter_support ||
	    !pc->data->funcs.set_counter || !pc->data->funcs.get_counter_result) {
		dev_err(chip->dev, "Unsupported counter mode\n");
		return -EINVAL;
	}

	pwm_get_state(pwm, &curstate);
	if (curstate.enabled) {
		dev_err(chip->dev, "Failed to enable counter mode because PWM%d is busy\n",
			pc->channel_id);
		return -EBUSY;
	}

	if (enable) {
		ret = clk_enable(pc->pclk);
		if (ret)
			return ret;
	}

	ret = pinctrl_select_state(pc->pinctrl, pc->active_state);
	if (ret) {
		dev_err(chip->dev, "Failed to select pinctrl state\n");
		goto err_disable_pclk;
	}

	ret = pc->data->funcs.set_counter(chip, pwm, input_sel, enable);
	if (ret) {
		dev_err(chip->dev, "Failed to abtain counter arbitration for PWM%d\n",
			pc->channel_id);
		goto err_disable_pclk;
	}

	if (!enable)
		clk_disable(pc->pclk);

	return ret;

err_disable_pclk:
	if (enable)
		clk_disable(pc->pclk);

	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_pwm_set_counter);

static int rockchip_pwm_get_counter_result_v4(struct pwm_chip *chip, struct pwm_device *pwm,
					      unsigned long *counter_res, bool is_clear)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	u64 low, high;

	low = readl_relaxed(pc->base + COUNTER_LOW);
	high = readl_relaxed(pc->base + COUNTER_HIGH);

	*counter_res = (high << 32) | low;
	if (!*counter_res)
		return -EINVAL;

	if (is_clear)
		writel_relaxed(COUNTER_CLR(true), pc->base + COUNTER_CTRL);

	return 0;
}

int rockchip_pwm_get_counter_result(struct pwm_device *pwm,
				    unsigned long *counter_res, bool is_clear)
{
	struct pwm_chip *chip;
	struct rockchip_pwm_chip *pc;
	int ret = 0;

	if (!pwm || !counter_res)
		return -EINVAL;

	chip = pwm->chip;
	pc = to_rockchip_pwm_chip(chip);

	if (!pc->counter_support ||
	    !pc->data->funcs.set_counter || !pc->data->funcs.get_counter_result) {
		dev_err(chip->dev, "Unsupported counter mode\n");
		return -EINVAL;
	}

	ret = pc->data->funcs.get_counter_result(chip, pwm, counter_res, is_clear);
	if (ret) {
		dev_err(chip->dev, "Failed to get counter result for PWM%d\n",
			pc->channel_id);
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_pwm_get_counter_result);

static int rockchip_pwm_set_freq_meter_v4(struct pwm_chip *chip, struct pwm_device *pwm,
					  unsigned long delay_ms,
					  enum rockchip_pwm_freq_meter_input_sel input_sel,
					  bool enable)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	u64 div = 0;
	u64 timer_val = 0;
	u32 arbiter = 0;
	u32 channel_sel = 0;
	u32 val;
	int ret;

	if (enable) {
		reinit_completion(&pc->freq_meter_completion);

		arbiter = BIT(pc->channel_id) << FREQ_READ_LOCK_SHIFT |
			  BIT(pc->channel_id) << FREQ_GRANT_SHIFT;
		channel_sel = pc->channel_id;

		div = (u64)pc->clk_rate * delay_ms;
		timer_val = DIV_ROUND_CLOSEST_ULL(div, MSEC_PER_SEC);
	}

	writel_relaxed(arbiter, pc->base + FREQ_ARBITER);
	if (enable) {
		val = readl_relaxed(pc->base + FREQ_ARBITER);
		if (!(val & arbiter))
			return -EINVAL;
	}

	if (enable) {
		ret = clk_enable(pc->clk);
		if (ret)
			return ret;
	}

	writel_relaxed(FREQ_INT_EN(enable), pc->base + INT_EN);
	writel_relaxed(timer_val, pc->base + FREQ_TIMER_VALUE);
	writel_relaxed(FREQ_EN(enable) | FREQ_CLK_SEL(input_sel) | FREQ_CHANNEL_SEL(channel_sel),
		       pc->base + FREQ_CTRL);

	if (!enable)
		clk_disable(pc->clk);

	return 0;
}

static int rockchip_pwm_get_freq_meter_result_v4(struct pwm_chip *chip, struct pwm_device *pwm,
						 unsigned long delay_ms, unsigned long *freq_hz)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	u32 freq_res;
	u32 freq_timer;
	int ret = 0;

	ret = wait_for_completion_timeout(&pc->freq_meter_completion,
					  msecs_to_jiffies(delay_ms * 3 / 2));
	if (!ret) {
		dev_err(chip->dev, "Failed to wait for PWM%d frequency meter result to be valid\n",
			pc->channel_id);
		return -ETIMEDOUT;
	}

	freq_res = readl_relaxed(pc->base + FREQ_RESULT_VALUE);
	freq_timer = readl_relaxed(pc->base + FREQ_TIMER_VALUE);
	*freq_hz = DIV_ROUND_CLOSEST_ULL((u64)pc->clk_rate * freq_res, freq_timer);
	if (!*freq_hz)
		return -EINVAL;

	return 0;
}

int rockchip_pwm_set_freq_meter(struct pwm_device *pwm, unsigned long delay_ms,
				enum rockchip_pwm_freq_meter_input_sel input_sel,
				unsigned long *freq_hz)
{
	struct pwm_chip *chip;
	struct rockchip_pwm_chip *pc;
	struct pwm_state curstate;
	int ret = 0;

	if (!pwm || !freq_hz)
		return -EINVAL;

	chip = pwm->chip;
	pc = to_rockchip_pwm_chip(chip);

	if (!pc->freq_meter_support ||
	    !pc->data->funcs.set_freq_meter || !pc->data->funcs.get_freq_meter_result) {
		dev_err(chip->dev, "Unsupported frequency meter mode\n");
		return -EINVAL;
	}

	pwm_get_state(pwm, &curstate);
	if (curstate.enabled) {
		dev_err(chip->dev, "Failed to enable frequency meter mode because PWM%d is busy\n",
			pc->channel_id);
		return -EBUSY;
	}

	ret = clk_enable(pc->pclk);
	if (ret)
		return ret;

	ret = pinctrl_select_state(pc->pinctrl, pc->active_state);
	if (ret) {
		dev_err(chip->dev, "Failed to select pinctrl state\n");
		goto err_disable_pclk;
	}

	ret = pc->data->funcs.set_freq_meter(chip, pwm, delay_ms, input_sel, true);
	if (ret) {
		dev_err(chip->dev, "Failed to abtain frequency meter arbitration for PWM%d\n",
			pc->channel_id);
	} else {
		ret = pc->data->funcs.get_freq_meter_result(chip, pwm, delay_ms, freq_hz);
		if (ret) {
			dev_err(chip->dev, "Failed to get frequency meter result for PWM%d\n",
				pc->channel_id);
		}
	}
	pc->data->funcs.set_freq_meter(chip, pwm, 0, 0, false);

err_disable_pclk:
	clk_disable(pc->pclk);

	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_pwm_set_freq_meter);

static int rockchip_pwm_global_ctrl_v4(struct pwm_chip *chip, struct pwm_device *pwm,
				       enum rockchip_pwm_global_ctrl_cmd cmd)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	u32 arbiter = 0;
	u32 val = 0;
	int ret = 0;

	switch (cmd) {
	case PWM_GLOBAL_CTRL_JOIN:
		writel_relaxed(PWM_GLOBAL_JOIN_EN(true), pc->base + ENABLE);
		writel_relaxed(CLK_GLOBAL_SEL(true), pc->base + CLK_CTRL);
		break;
	case PWM_GLOBAL_CTRL_EXIT:
		writel_relaxed(PWM_GLOBAL_JOIN_EN(false), pc->base + ENABLE);
		writel_relaxed(CLK_GLOBAL_SEL(false), pc->base + CLK_CTRL);
		break;
	case PWM_GLOBAL_CTRL_GRANT:
		arbiter = BIT(pc->channel_id) << GLOBAL_READ_LOCK_SHIFT |
			  BIT(pc->channel_id) << GLOBAL_GRANT_SHIFT;

		writel_relaxed(arbiter, pc->base + GLOBAL_ARBITER);
		val = readl_relaxed(pc->base + GLOBAL_ARBITER);
		if (!(val & arbiter)) {
			dev_err(chip->dev, "Failed to abtain global ctrl arbitration for PWM%d\n",
				pc->channel_id);
			return -EINVAL;
		}

		pc->global_ctrl_grant = true;
		break;
	case PWM_GLOBAL_CTRL_RECLAIM:
		writel_relaxed(0, pc->base + GLOBAL_ARBITER);

		pc->global_ctrl_grant = false;
		break;
	case PWM_GLOBAL_CTRL_UPDATE:
		if (!pc->global_ctrl_grant) {
			dev_err(chip->dev, "CMD %d: get global ctrl arbitration first for PWM%d\n",
				cmd, pc->channel_id);
			return -EINVAL;
		}

		writel_relaxed(GLOBAL_PWM_UPDATE_EN(true), pc->base + GLOBAL_CTRL);
		break;
	case PWM_GLOBAL_CTRL_ENABLE:
		if (!pc->global_ctrl_grant) {
			dev_err(chip->dev, "CMD %d: get global ctrl arbitration first for PWM%d\n",
				cmd, pc->channel_id);
			return -EINVAL;
		}

		if (!test_and_set_bit(0, &pc->is_clk_enabled)) {
			ret = clk_enable(pc->clk);
			if (ret)
				return ret;
		}

		writel_relaxed(PWM_CLK_EN(true), pc->base + ENABLE);
		writel_relaxed(GLOBAL_PWM_EN(true), pc->base + GLOBAL_CTRL);
		break;
	case PWM_GLOBAL_CTRL_DISABLE:
		if (!pc->global_ctrl_grant) {
			dev_err(chip->dev, "CMD %d: get global ctrl arbitration first for PWM%d\n",
				cmd, pc->channel_id);
			return -EINVAL;
		}

		writel_relaxed(PWM_CLK_EN(false), pc->base + ENABLE);
		writel_relaxed(GLOBAL_PWM_EN(false), pc->base + GLOBAL_CTRL);

		if (test_and_clear_bit(0, &pc->is_clk_enabled))
			clk_disable(pc->clk);

		break;
	default:
		dev_err(chip->dev, "Unsupported global ctrl cmd %d\n", cmd);
		return -EINVAL;
	}

	return 0;
}

int rockchip_pwm_global_ctrl(struct pwm_device *pwm, enum rockchip_pwm_global_ctrl_cmd cmd)
{
	struct pwm_chip *chip;
	struct rockchip_pwm_chip *pc;
	struct pwm_state curstate;
	int ret = 0;

	if (!pwm)
		return -EINVAL;

	chip = pwm->chip;
	pc = to_rockchip_pwm_chip(chip);

	if (!pc->data->funcs.global_ctrl) {
		dev_err(chip->dev, "Unsupported global control\n");
		return -EINVAL;
	}

	pwm_get_state(pwm, &curstate);
	if (curstate.enabled) {
		dev_err(chip->dev, "Failed to execute global ctrl cmd %d because PWM%d is busy\n",
			cmd, pc->channel_id);
		return -EBUSY;
	}

	ret = clk_enable(pc->pclk);
	if (ret)
		return ret;

	ret = pinctrl_select_state(pc->pinctrl, pc->active_state);
	if (ret) {
		dev_err(chip->dev, "Failed to select pinctrl state\n");
		goto err_disable_pclk;
	}

	ret = pc->data->funcs.global_ctrl(chip, pwm, cmd);
	if (ret) {
		dev_err(chip->dev, "Failed to execute global ctrl cmd %d for PWM%d\n",
			cmd, pc->channel_id);
		goto err_disable_pclk;
	}

err_disable_pclk:
	clk_disable(pc->pclk);

	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_pwm_global_ctrl);

static int rockchip_pwm_set_wave_table_v4(struct pwm_chip *chip, struct pwm_device *pwm,
					  struct rockchip_pwm_wave_table *table_config,
					  enum rockchip_pwm_wave_table_width_mode width_mode)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	u64 table_val = 0;
	u64 clk_rate_kHz = pc->clk_rate / 1000;
	u64 div = 0;
	u64 tmp = 0;
	u32 scaler = pc->scaler ? pc->scaler * 2 : 1;
	u32 arbiter = 0;
	u32 val;
	u16 table_max;
	int i;

	if (width_mode == PWM_WAVE_TABLE_16BITS_WIDTH)
		table_max = pc->data->wave_table_max / 2;
	else
		table_max = pc->data->wave_table_max;

	if (!table_config->table ||
	    table_config->offset > table_max || table_config->len > table_max) {
		dev_err(chip->dev, "The wave table to set is out of range for PWM%d\n",
			pc->channel_id);
		return -EINVAL;
	}

	arbiter = BIT(pc->channel_id) << WAVE_MEM_GRANT_SHIFT |
		  BIT(pc->channel_id) << WAVE_MEM_READ_LOCK_SHIFT;
	writel_relaxed(arbiter, pc->base + WAVE_MEM_ARBITER);

	val = readl_relaxed(pc->base + WAVE_MEM_ARBITER);
	if (!(val & arbiter)) {
		dev_err(chip->dev, "Failed to abtain wave memory arbitration for PWM%d\n",
			pc->channel_id);
		return -EINVAL;
	}

	if (width_mode == PWM_WAVE_TABLE_16BITS_WIDTH) {
		for (i = 0; i < table_config->len; i++) {
			div = (u64)clk_rate_kHz * table_config->table[i];
			tmp = (u64)pc->data->prescaler * scaler * USEC_PER_SEC;
			table_val = DIV_ROUND_CLOSEST_ULL(div, tmp);
			writel_relaxed(table_val & 0xff,
				       pc->base + WAVE_MEM + (table_config->offset + i) * 2 * 4);
			if (readl_poll_timeout(pc->base + WAVE_MEM_STATUS,
					       val, (val & BIT(WAVE_MEM_STATUS_SHIFT)),
					       1000, 10 * 1000)) {
				dev_err(chip->dev,
					"Wait for wave mem(offset 0x%08x) to update failed\n",
					(table_config->offset + i) * 2 * 4);
				return -ETIMEDOUT;
			}

			writel_relaxed((table_val >> 8) & 0xff,
				       pc->base + WAVE_MEM +
				       ((table_config->offset + i) * 2 + 1) * 4);
			if (readl_poll_timeout(pc->base + WAVE_MEM_STATUS,
					       val, (val & BIT(WAVE_MEM_STATUS_SHIFT)),
					       1000, 10 * 1000)) {
				dev_err(chip->dev,
					"Wait for wave mem(offset 0x%08x) to update failed\n",
					((table_config->offset + i) * 2 + 1) * 4);
				return -ETIMEDOUT;
			}
		}
	} else {
		for (i = 0; i < table_config->len; i++) {
			div = (u64)clk_rate_kHz * table_config->table[i];
			tmp = (u64)pc->data->prescaler * scaler * USEC_PER_SEC;
			table_val = DIV_ROUND_CLOSEST_ULL(div, tmp);
			writel_relaxed(table_val,
				       pc->base + WAVE_MEM + (table_config->offset + i) * 4);
			if (readl_poll_timeout(pc->base + WAVE_MEM_STATUS,
					       val, (val & BIT(WAVE_MEM_STATUS_SHIFT)),
					       1000, 10 * 1000)) {
				dev_err(chip->dev,
					"Wait for wave mem(offset 0x%08x) to update failed\n",
					(table_config->offset + i) * 4);
				return -ETIMEDOUT;
			}
		}
	}

	writel_relaxed(0, pc->base + WAVE_MEM_ARBITER);

	return 0;
}

static int rockchip_pwm_set_wave_v4(struct pwm_chip *chip, struct pwm_device *pwm,
				    struct rockchip_pwm_wave_config *config)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	u32 ctrl = 0;
	u32 max_val = 0;
	u32 min_val = 0;
	u32 offset = 0;
	u32 middle = 0;
	u32 rpt = 0;
	u8 factor = 0;

	if (config->enable) {
		/*
		 * If the width mode is 16-bits mode, two 8-bits table units
		 * are combined into one 16-bits unit.
		 */
		if (config->width_mode == PWM_WAVE_TABLE_16BITS_WIDTH)
			factor = 2;
		else
			factor = 1;

		ctrl = WAVE_DUTY_EN(config->duty_en) |
		       WAVE_PERIOD_EN(config->period_en) |
		       WAVE_WIDTH_MODE(config->width_mode) |
		       WAVE_UPDATE_MODE(config->update_mode) |
		       WAVE_MEM_CLK_SEL(config->mem_clk_src);
		max_val = config->duty_max * factor << WAVE_DUTY_MAX_SHIFT |
			  config->period_max * factor << WAVE_PERIOD_MAX_SHIFT;
		min_val = config->duty_min * factor << WAVE_DUTY_MIN_SHIFT |
			  config->period_min * factor << WAVE_PERIOD_MIN_SHIFT;
		offset = config->offset * factor << WAVE_OFFSET_SHIFT;
		middle = config->middle * factor << WAVE_MIDDLE_SHIFT;

		rpt = config->rpt << FIRST_DIMENSIONAL_SHIFT;
	} else {
		pc->scaler = 0;
		ctrl = WAVE_DUTY_EN(false) | WAVE_PERIOD_EN(false);
	}

	writel_relaxed(CLK_SCALE(pc->scaler) | CLK_SRC_SEL(config->clk_src), pc->base + CLK_CTRL);
	writel_relaxed(ctrl, pc->base + WAVE_CTRL);
	writel_relaxed(max_val, pc->base + WAVE_MAX);
	writel_relaxed(min_val, pc->base + WAVE_MIN);
	writel_relaxed(offset, pc->base + WAVE_OFFSET);
	writel_relaxed(middle, pc->base + WAVE_MIDDLE);

	writel_relaxed(rpt, pc->base + RPT);

	pc->wave_en = config->enable;

	return 0;
}

int rockchip_pwm_set_wave(struct pwm_device *pwm, struct rockchip_pwm_wave_config *config)
{
	struct pwm_chip *chip;
	struct rockchip_pwm_chip *pc;
	int ret = 0;

	if (!pwm || !config)
		return -EINVAL;

	chip = pwm->chip;
	pc = to_rockchip_pwm_chip(chip);

	if (!pc->wave_support ||
	    !pc->data->funcs.set_wave_table || !pc->data->funcs.set_wave) {
		dev_err(chip->dev, "Unsupported wave generator mode\n");
		return -EINVAL;
	}

	if (!config->clk_rate) {
		dev_err(chip->dev, "clk rate can not be 0\n");
		return -EINVAL;
	}

	pc->scaler = DIV_ROUND_CLOSEST_ULL(pc->clk_rate, config->clk_rate) / 2;
	if (pc->scaler > 256) {
		dev_err(chip->dev, "Unsupported scale factor %d(max: 512) for PWM%d\n",
			pc->scaler * 2, pc->channel_id);
		return -EINVAL;
	}

	ret = clk_enable(pc->pclk);
	if (ret)
		return ret;

	if (config->enable) {
		ret = clk_enable(pc->clk_osc);
		if (ret) {
			dev_err(chip->dev, "Failed to enable OSC clk for PWM%d\n", pc->channel_id);
			goto err_disable_pclk;
		}
	}

	if (config->wave_table) {
		ret = pc->data->funcs.set_wave_table(chip, pwm, config->wave_table,
						     config->width_mode);
		if (ret) {
			dev_err(chip->dev, "Failed to set wave period table for PWM%d\n",
				pc->channel_id);
			goto err_disable_clk_osc;
		}
	}

	ret = pc->data->funcs.set_wave(chip, pwm, config);
	if (ret) {
		dev_err(chip->dev, "Failed to set wave generator for PWM%d\n", pc->channel_id);
		goto err_disable_clk_osc;
	}

	if (!config->enable)
		clk_disable(pc->clk_osc);

	clk_disable(pc->pclk);

	return ret;

err_disable_clk_osc:
	if (config->enable)
		clk_disable(pc->clk_osc);
err_disable_pclk:
	clk_disable(pc->pclk);

	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_pwm_set_wave);

static int rockchip_pwm_set_biphasic_v4(struct pwm_chip *chip, struct pwm_device *pwm,
					struct rockchip_pwm_biphasic_config *config)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	u64 div = 0;
	u32 ctrl = 0;
	u32 timer_val = 0;
	int ret = 0;

	if (config->enable) {
		if (!config->is_continuous && !config->delay_ms) {
			dev_err(chip->dev, "The delay_ms can not be 0 in normal mode for PWM%d\n",
				pc->channel_id);
			return -EINVAL;
		}

		ret = clk_enable(pc->clk);
		if (ret)
			return ret;

		if (!config->is_continuous)
			reinit_completion(&pc->biphasic_completion);

		ctrl = BIPHASIC_EN(true) |
		       BIPHASIC_CONTINOUS_MODE_EN(config->is_continuous) |
		       BIPHASIC_MODE(config->mode == PWM_BIPHASIC_COUNTER_MODE0_FREQ ?
				     PWM_BIPHASIC_COUNTER_MODE0 : config->mode) |
		       BIPHASIC_SYNC_EN(config->is_continuous);

		div = (u64)pc->clk_rate * config->delay_ms;
		timer_val = DIV_ROUND_CLOSEST_ULL(div, MSEC_PER_SEC);

		pc->biphasic_config = config;
	} else {
		ctrl = BIPHASIC_EN(false);

		pc->biphasic_config = NULL;
	}

	writel_relaxed(BIPHASIC_INT_EN(config->enable), pc->base + INT_EN);
	writel_relaxed(ctrl, pc->base + BIPHASIC_CTRL0);
	writel_relaxed(timer_val, pc->base + BIPHASIC_TIMER_VALUE);

	if (!config->enable)
		clk_disable(pc->clk);

	return 0;
}

int rockchip_pwm_set_biphasic(struct pwm_device *pwm, struct rockchip_pwm_biphasic_config *config,
			      unsigned long *biphasic_res)
{
	struct pwm_chip *chip;
	struct rockchip_pwm_chip *pc;
	struct pwm_state curstate;
	int ret = 0;

	if (!pwm)
		return -EINVAL;

	chip = pwm->chip;
	pc = to_rockchip_pwm_chip(chip);

	if (!pc->biphasic_support ||
	    !pc->data->funcs.set_biphasic || !pc->data->funcs.get_biphasic_result) {
		dev_err(chip->dev, "Unsupported biphasic counter mode\n");
		return -EINVAL;
	}

	pwm_get_state(pwm, &curstate);
	if (curstate.enabled) {
		dev_err(chip->dev, "Failed to enable biphasic counter mode because PWM%d is busy\n",
			pc->channel_id);
		return -EBUSY;
	}

	ret = clk_enable(pc->pclk);
	if (ret)
		return ret;

	ret = pinctrl_select_state(pc->pinctrl, pc->active_state);
	if (ret) {
		dev_err(chip->dev, "Failed to select pinctrl state\n");
		goto err_disable_pclk;
	}

	ret = pc->data->funcs.set_biphasic(chip, pwm, config);
	if (ret) {
		dev_err(chip->dev, "Failed to setup biphasic counter mode for PWM%d\n",
			pc->channel_id);
	} else {
		if (pc->biphasic_config && pc->biphasic_config->enable && !config->is_continuous) {
			ret = pc->data->funcs.get_biphasic_result(chip, pwm, biphasic_res);
			if (ret) {
				dev_err(chip->dev,
					"Failed to get biphasic counter result for PWM%d\n",
					pc->channel_id);
			}
			config->enable = false;
			pc->data->funcs.set_biphasic(chip, pwm, config);
		}
	}

err_disable_pclk:
	clk_disable(pc->pclk);

	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_pwm_set_biphasic);

static int rockchip_pwm_get_biphasic_result_v4(struct pwm_chip *chip, struct pwm_device *pwm,
					       unsigned long *biphasic_res)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	const struct rockchip_pwm_biphasic_config *config = pc->biphasic_config;
	u32 val;
	u32 biphasic_timer;
	int ret = 0;

	if (!config->is_continuous) {
		ret = wait_for_completion_timeout(&pc->biphasic_completion,
						  msecs_to_jiffies(config->delay_ms * 3 / 2));
		if (!ret) {
			dev_err(chip->dev,
				"Failed to wait for PWM%d biphasic counter result to be valid\n",
				pc->channel_id);
			return -ETIMEDOUT;
		}

		*biphasic_res = readl_relaxed(pc->base + BIPHASIC_RESULT_VALUE);
		if (!*biphasic_res)
			return -EINVAL;

		if (pc->biphasic_config->mode == PWM_BIPHASIC_COUNTER_MODE0_FREQ) {
			val = *biphasic_res;
			biphasic_timer = readl_relaxed(pc->base + BIPHASIC_TIMER_VALUE);
			*biphasic_res = DIV_ROUND_CLOSEST_ULL((u64)pc->clk_rate * val,
							      biphasic_timer);
		}
	} else {
		*biphasic_res = readl_relaxed(pc->base + BIPHASIC_RESULT_VALUE_SYNC);
	}

	return 0;
}

int rockchip_pwm_get_biphasic_result(struct pwm_device *pwm, unsigned long *biphasic_res)
{
	struct pwm_chip *chip;
	struct rockchip_pwm_chip *pc;
	int ret = 0;

	if (!pwm)
		return -EINVAL;

	chip = pwm->chip;
	pc = to_rockchip_pwm_chip(chip);

	if (!pc->biphasic_support ||
	    !pc->data->funcs.set_biphasic || !pc->data->funcs.get_biphasic_result) {
		dev_err(chip->dev, "Unsupported biphasic counter mode\n");
		return -EINVAL;
	}

	if (!pc->biphasic_config) {
		dev_err(chip->dev, "Failed to parse biphasic counter config\n");
		return -EINVAL;
	}

	if (!pc->biphasic_config->is_continuous || !pc->biphasic_config->enable) {
		dev_err(chip->dev, "Unsupported to get result in real time in normal mode\n");
		return -EINVAL;
	}

	ret = clk_enable(pc->pclk);
	if (ret)
		return ret;

	ret = pc->data->funcs.get_biphasic_result(chip, pwm, biphasic_res);
	if (ret)
		dev_err(chip->dev, "Failed to get biphasic counter result for PWM%d\n",
			pc->channel_id);

	clk_disable(pc->pclk);

	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_pwm_get_biphasic_result);

static int rockchip_pwm_set_filter_v1(struct pwm_chip *chip, struct pwm_device *pwm,
				      u64 filter_window_ns)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	u64 filter;
	u64 div;
	u32 mask;
	u32 ctrl;

	ctrl = readl_relaxed(pc->base + PWM_REG_IO_CTRL(pc->channel_id));
	if (filter_window_ns) {
		mask = PWM_FILTER_ENABLE_MASK & (~PWM_FILTER_ENABLE(pc->channel_id));
		if (ctrl & mask) {
			dev_err(chip->dev,
				"Failed to set filter for PWM%d: only single-channel filtering supported\n",
				pc->channel_id);
			return -EBUSY;
		}

		div = (u64)pc->clk_rate * filter_window_ns;
		filter = DIV_ROUND_CLOSEST_ULL(div, pc->data->prescaler * NSEC_PER_SEC);

		ctrl &= ~PWM_FILTER_NUMBER_MASK;
		ctrl |= filter << PWM_FILTER_NUMBER_SHIFT;
		ctrl |= PWM_FILTER_ENABLE(pc->channel_id);
	} else {
		ctrl &= ~PWM_FILTER_NUMBER_MASK;
		ctrl &= ~PWM_FILTER_ENABLE(pc->channel_id);
	}
	writel_relaxed(ctrl, pc->base + PWM_REG_IO_CTRL(pc->channel_id));

	return 0;
}

static int rockchip_pwm_set_filter_v4(struct pwm_chip *chip, struct pwm_device *pwm,
				      u64 filter_window_ns)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	u64 filter = 0;
	u64 clk_rate_kHz = pc->clk_rate / 1000;
	u64 div = 0;
	u64 tmp = 0;
	u32 scaler = pc->scaler ? pc->scaler * 2 : 1;

	if (filter_window_ns) {
		div = (u64)clk_rate_kHz * filter_window_ns;
		tmp = (u64)pc->data->prescaler * scaler * USEC_PER_SEC;
		filter = DIV_ROUND_CLOSEST_ULL(div, tmp);
	}

	writel_relaxed(FILTER_ENABLE(filter_window_ns ? true : false) | FILTER_NUMBER(filter),
		       pc->base + FILTER_CTRL);

	return 0;
}

int rockchip_pwm_set_filter(struct pwm_device *pwm, u64 filter_window_ns)
{
	struct pwm_chip *chip;
	struct rockchip_pwm_chip *pc;
	int ret = 0;

	if (!pwm)
		return -EINVAL;

	chip = pwm->chip;
	pc = to_rockchip_pwm_chip(chip);

	if ((pc->main_version > 4 && !pc->filter_support) || !pc->data->funcs.set_filter) {
		dev_err(chip->dev, "Unsupported filter configuration\n");
		return -EINVAL;
	}

	ret = clk_enable(pc->pclk);
	if (ret)
		return ret;

	ret = pc->data->funcs.set_filter(chip, pwm, filter_window_ns);
	if (ret)
		dev_err(chip->dev, "Failed to get biphasic counter result for PWM%d\n",
			pc->channel_id);

	clk_disable(pc->pclk);

	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_pwm_set_filter);

#ifdef CONFIG_RC_CORE
static int rockchip_pwm_ir_transmit_v4(struct pwm_chip *chip, unsigned int *txbuf,
				       unsigned int count)
{
	struct rockchip_pwm_chip *pc = to_rockchip_pwm_chip(chip);
	u32 arbiter;
	u32 preload, spreload;
	u32 low_period, high_period;
	u32 tx_value;
	u32 timeout_ms;
	u32 val;
	int ret = 0;

	if (count != PWM_IR_TRANSMIT_BUFFER_SIZE) {
		dev_err(chip->dev, "Unsupported ir transmit buf size: %d\n", count);
		return -EINVAL;
	}

	ret = clk_enable(pc->clk);
	if (ret)
		return ret;

	arbiter = BIT(pc->channel_id) << IR_TRANS_READ_LOCK_SHIFT |
		  BIT(pc->channel_id) << IR_TRANS_GRANT_SHIFT;
	writel_relaxed(arbiter, pc->base + IR_TRANS_ARBITER);
	val = readl_relaxed(pc->base + IR_TRANS_ARBITER);
	if (!(val & arbiter)) {
		dev_err(chip->dev, "Failed to abtain ir transmit arbitration for PWM%d\n",
			pc->channel_id);
		ret = -EINVAL;
		goto err_clk;
	}

	reinit_completion(&pc->ir_trans_completion);

	/*
	 * Each value in the txbuf[] is in microseconds(us).
	 * txbuf[0]: the low duration of NEC leader code.
	 * txbuf[1]: the high duration of NEC leader code.
	 * txbuf[2]: the high duration of NEC repeat code.
	 * txbuf[3]: the low duration of NEC logic '0' and '1'.
	 * txbuf[4]: the high duration of NEC logic '0'.
	 * txbuf[5]: the high duration of NEC logic '1'.
	 * txbuf[6]:
	 * For 8-bit address code:
	 *   bit[31:24]             bit[23:16]    bit[15:8]              bit[7:0]
	 *   command inverted code  command code  address inverted code  address code
	 *
	 * For 16-bit address code:
	 *   bit[31:24]             bit[23:16]    bit[15:8]              bit[7:0]
	 *   command inverted code  command code  address code bit[15:8] address code bit[7:0]
	 */
	preload = txbuf[0] << IR_TRANS_OUT_LOW_PRELOAD_SHIFT |
		  txbuf[1] << IR_TRANS_OUT_HIGH_PRELOAD_SHIFT;
	spreload = txbuf[2] << IR_TRANS_OUT_HIGH_SIMPLE_PRELOAD_SHIFT;
	low_period = txbuf[3] << IR_TRANS_OUT_DATA_LOW_PERIOD_SHIFT;
	high_period = txbuf[4] << IR_TRANS_OUT_HIGH_PERIOD_FOR_ZERO_SHIFT |
		      txbuf[5] << IR_TRANS_OUT_HIGH_PERIOD_FOR_ONE_SHIFT;
	tx_value = txbuf[6] << IR_TRANS_OUT_VALUE_SHIFT;

	/* Set the dclk to 1M */
	writel_relaxed(CLK_SCALE(0x32), pc->base + CLK_CTRL);
	writel_relaxed(PWM_CLK_EN(true), pc->base + ENABLE);
	writel_relaxed(IR_TRANS_END_INT_EN(true), pc->base + INT_EN);

	writel_relaxed(preload, pc->base + IR_TRANS_PRE);
	writel_relaxed(spreload, pc->base + IR_TRANS_SPRE);
	writel_relaxed(low_period, pc->base + IR_TRANS_LD);
	writel_relaxed(high_period, pc->base + IR_TRANS_HD);
	writel_relaxed(tx_value, pc->base + IR_TRANS_DATA_VALUE);

	val = readl_relaxed(pc->base + IR_TRANS_BURST_FRAME);
	timeout_ms = ((val & IR_TRANS_OUT_FRAME_PERIOD_MASK) >>
		      IR_TRANS_OUT_FRAME_PERIOD_SHIFT) / 1000;

	writel_relaxed(IR_TRANS_INACTIVE_POL(true) | IR_TRANS_OUT_ENABLE(true),
		       pc->base + IR_TRANS_CTRL0);

	ret = wait_for_completion_timeout(&pc->ir_trans_completion,
					  msecs_to_jiffies(timeout_ms * 3 / 2));
	if (!ret) {
		dev_err(chip->dev, "Failed to wait for PWM%d ir transmit to complete\n",
			pc->channel_id);
		ret = -ETIMEDOUT;
	}

	writel_relaxed(IR_TRANS_OUT_ENABLE(false), pc->base + IR_TRANS_CTRL0);
	writel_relaxed(IR_TRANS_END_INT_EN(false), pc->base + INT_EN);
	writel_relaxed(PWM_CLK_EN(false), pc->base + ENABLE);
	writel_relaxed(0, pc->base + IR_TRANS_ARBITER);

err_clk:
	clk_disable(pc->clk);

	return ret ? ret : count;
}

static int rockchip_pwm_ir_transmit(struct rc_dev *dev, unsigned int *txbuf, unsigned int count)
{
	struct rockchip_pwm_chip *pc = dev->priv;
	struct pwm_chip *chip = &pc->chip;
	struct pwm_state curstate;
	int ret;

	if (!pc->data->funcs.ir_transmit) {
		dev_err(chip->dev, "Unsupported ir transmit mode\n");
		return -EINVAL;
	}

	pwm_get_state(&pc->chip.pwms[0], &curstate);
	if (curstate.enabled) {
		dev_err(chip->dev, "Failed to enable ir transmit mode because PWM%d is busy\n",
			pc->channel_id);
		return -EBUSY;
	}

	ret = pinctrl_select_state(pc->pinctrl, pc->active_state);
	if (ret) {
		dev_err(chip->dev, "Failed to select pinctrl state\n");
		return ret;
	}

	ret = clk_enable(pc->pclk);
	if (ret)
		return ret;

	ret = pc->data->funcs.ir_transmit(chip, txbuf, count);
	if (ret < 0)
		dev_err(chip->dev, "Failed to transmit ir buf\n");

	clk_disable(pc->pclk);

	return ret;
}
#else
static int rockchip_pwm_ir_transmit_v4(struct pwm_chip *chip, unsigned int *txbuf,
				       unsigned int count)
{
	return count;
}
#endif

#ifdef CONFIG_DEBUG_FS
static int rockchip_pwm_debugfs_show(struct seq_file *s, void *data)
{
	struct rockchip_pwm_chip *pc = s->private;
	u32 regs_start;
	int i;
	int ret = 0;

	if (!pc->oneshot_en) {
		ret = clk_enable(pc->pclk);
		if (ret)
			return ret;
	}

	if (pc->main_version >= 4) {
		regs_start = (u32)pc->res->start;
		for (i = 0; i < 0x90; i += 4) {
			seq_printf(s, "%08x:  %08x %08x %08x %08x\n", regs_start + i * 4,
				   readl_relaxed(pc->base + (4 * i)),
				   readl_relaxed(pc->base + (4 * (i + 1))),
				   readl_relaxed(pc->base + (4 * (i + 2))),
				   readl_relaxed(pc->base + (4 * (i + 3))));
		}
	} else {
		regs_start = (u32)pc->res->start - pc->channel_id * 0x10;
		for (i = 0; i < 0x40; i += 4) {
			seq_printf(s, "%08x:  %08x %08x %08x %08x\n", regs_start + i * 4,
				   readl_relaxed(pc->base + (4 * i)),
				   readl_relaxed(pc->base + (4 * (i + 1))),
				   readl_relaxed(pc->base + (4 * (i + 2))),
				   readl_relaxed(pc->base + (4 * (i + 3))));
		}
	}

	if (!pc->oneshot_en)
		clk_disable(pc->pclk);

	return ret;
}
DEFINE_SHOW_ATTRIBUTE(rockchip_pwm_debugfs);

static inline void rockchip_pwm_debugfs_init(struct rockchip_pwm_chip *pc)
{
	pc->debugfs = debugfs_create_file(dev_name(pc->chip.dev),
					  S_IFREG | 0444, NULL, pc,
					  &rockchip_pwm_debugfs_fops);
}

static inline void rockchip_pwm_debugfs_deinit(struct rockchip_pwm_chip *pc)
{
	debugfs_remove(pc->debugfs);
}
#else
static inline void rockchip_pwm_debugfs_init(struct rockchip_pwm_chip *pc)
{
}

static inline void rockchip_pwm_debugfs_deinit(struct rockchip_pwm_chip *pc)
{
}
#endif

static const struct pwm_ops rockchip_pwm_ops = {
	.capture = rockchip_pwm_capture,
	.apply = rockchip_pwm_apply,
	.get_state = rockchip_pwm_get_state,
	.owner = THIS_MODULE,
};

static const struct rockchip_pwm_data pwm_data_v1 = {
	.main_version = 0x01,
	.regs = {
		.version = 0x5c,
		.duty = 0x04,
		.period = 0x08,
		.ctrl = 0x0c,
	},
	.prescaler = 2,
	.supports_polarity = false,
	.supports_lock = false,
	.vop_pwm = false,
	.enable_conf = PWM_CTRL_OUTPUT_EN | PWM_CTRL_TIMER_EN,
	.enable_conf_mask = BIT(1) | BIT(3),
	.oneshot_cnt_max = 0x100,
	.funcs = {
		.enable = rockchip_pwm_enable_v1,
		.config = rockchip_pwm_config_v1,
	},
};

static const struct rockchip_pwm_data pwm_data_v2 = {
	.main_version = 0x02,
	.regs = {
		.version = 0x5c,
		.duty = 0x08,
		.period = 0x04,
		.ctrl = 0x0c,
	},
	.prescaler = 1,
	.supports_polarity = true,
	.supports_lock = false,
	.vop_pwm = false,
	.enable_conf = PWM_OUTPUT_LEFT | PWM_LP_DISABLE | PWM_ENABLE |
		       PWM_CONTINUOUS,
	.enable_conf_mask = GENMASK(2, 0) | BIT(5) | BIT(8),
	.oneshot_cnt_max = 0x100,
	.funcs = {
		.enable = rockchip_pwm_enable_v1,
		.config = rockchip_pwm_config_v1,
	},
};

static const struct rockchip_pwm_data pwm_data_vop = {
	.main_version = 0x02,
	.regs = {
		.version = 0x5c,
		.duty = 0x08,
		.period = 0x04,
		.ctrl = 0x00,
	},
	.prescaler = 1,
	.supports_polarity = true,
	.supports_lock = false,
	.vop_pwm = true,
	.enable_conf = PWM_OUTPUT_LEFT | PWM_LP_DISABLE | PWM_ENABLE |
		       PWM_CONTINUOUS,
	.enable_conf_mask = GENMASK(2, 0) | BIT(5) | BIT(8),
	.oneshot_cnt_max = 0x100,
	.funcs = {
		.enable = rockchip_pwm_enable_v1,
		.config = rockchip_pwm_config_v1,
	},
};

static const struct rockchip_pwm_data pwm_data_v3 = {
	.main_version = 0x03,
	.regs = {
		.version = 0x5c,
		.duty = 0x08,
		.period = 0x04,
		.ctrl = 0x0c,
	},
	.prescaler = 1,
	.supports_polarity = true,
	.supports_lock = true,
	.vop_pwm = false,
	.enable_conf = PWM_OUTPUT_LEFT | PWM_LP_DISABLE | PWM_ENABLE |
		       PWM_CONTINUOUS,
	.enable_conf_mask = GENMASK(2, 0) | BIT(5) | BIT(8),
	.oneshot_cnt_max = 0x100,
	.funcs = {
		.enable = rockchip_pwm_enable_v1,
		.config = rockchip_pwm_config_v1,
		.set_capture = rockchip_pwm_set_capture_v1,
		.get_capture_result = rockchip_pwm_get_capture_result_v1,
		.irq_handler = rockchip_pwm_irq_v1,
		.set_filter = rockchip_pwm_set_filter_v1,
	},
};

static const struct rockchip_pwm_data pwm_data_v4 = {
	.main_version = 0x04,
	.regs = {
		.version = 0x0,
		.enable = 0x4,
		.ctrl = 0xc,
		.period = 0x10,
		.duty = 0x14,
	},
	.prescaler = 1,
	.supports_polarity = true,
	.supports_lock = true,
	.vop_pwm = false,
	.oneshot_cnt_max = 0x10000,
	.oneshot_rpt_max = 0x10000,
	.wave_table_max = 0x300,
	.enable_conf = PWM_ENABLE_V4,
	.funcs = {
		.enable = rockchip_pwm_enable_v4,
		.config = rockchip_pwm_config_v4,
		.set_capture = rockchip_pwm_set_capture_v4,
		.get_capture_result = rockchip_pwm_get_capture_result_v4,
		.set_counter = rockchip_pwm_set_counter_v4,
		.get_counter_result = rockchip_pwm_get_counter_result_v4,
		.set_freq_meter = rockchip_pwm_set_freq_meter_v4,
		.get_freq_meter_result = rockchip_pwm_get_freq_meter_result_v4,
		.global_ctrl = rockchip_pwm_global_ctrl_v4,
		.set_wave_table = rockchip_pwm_set_wave_table_v4,
		.set_wave = rockchip_pwm_set_wave_v4,
		.set_biphasic = rockchip_pwm_set_biphasic_v4,
		.get_biphasic_result = rockchip_pwm_get_biphasic_result_v4,
		.ir_transmit = rockchip_pwm_ir_transmit_v4,
		.irq_handler = rockchip_pwm_irq_v4,
		.set_filter = rockchip_pwm_set_filter_v4,
	},
};

static const struct of_device_id rockchip_pwm_dt_ids[] = {
	{ .compatible = "rockchip,rk2928-pwm", .data = &pwm_data_v1},
	{ .compatible = "rockchip,rk3288-pwm", .data = &pwm_data_v2},
	{ .compatible = "rockchip,vop-pwm", .data = &pwm_data_vop},
	{ .compatible = "rockchip,rk3328-pwm", .data = &pwm_data_v3},
	{ .compatible = "rockchip,rk3576-pwm", .data = &pwm_data_v4},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rockchip_pwm_dt_ids);

static int rockchip_pwm_get_channel_id(const char *name)
{
	int len = strlen(name);

	return name[len - 2] - '0';
}

static u32 rockchip_pwm_get_minor_version(struct rockchip_pwm_chip *pc)
{
	return (readl_relaxed(pc->base + pc->data->regs.version) & MINOR_VERSION_MASK) >>
	       MINOR_VERSION_SHIFT;
}

static int rockchip_pwm_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;
	struct rockchip_pwm_chip *pc;
	struct resource *r;
	unsigned long irq_flags;
	u32 enable_conf, ctrl, feature;
	bool enabled;
	int ret, count;

	id = of_match_device(rockchip_pwm_dt_ids, &pdev->dev);
	if (!id)
		return -EINVAL;

	pc = devm_kzalloc(&pdev->dev, sizeof(*pc), GFP_KERNEL);
	if (!pc)
		return -ENOMEM;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		dev_err(&pdev->dev, "Failed to get pwm register\n");
		return -EINVAL;
	}
	pc->res = r;

	pc->base = devm_ioremap(&pdev->dev, pc->res->start,
				resource_size(pc->res));
	if (IS_ERR(pc->base))
		return PTR_ERR(pc->base);

	pc->clk = devm_clk_get(&pdev->dev, "pwm");
	if (IS_ERR(pc->clk)) {
		pc->clk = devm_clk_get(&pdev->dev, NULL);
		if (IS_ERR(pc->clk))
			return dev_err_probe(&pdev->dev, PTR_ERR(pc->clk),
					     "Can't get PWM clk\n");
	}

	count = of_count_phandle_with_args(pdev->dev.of_node,
					   "clocks", "#clock-cells");
	if (count >= 2) {
		pc->pclk = devm_clk_get(&pdev->dev, "pclk");
		pc->clk_osc = devm_clk_get_optional(&pdev->dev, "osc");
	} else {
		pc->pclk = pc->clk;
	}

	if (IS_ERR(pc->pclk))
		return dev_err_probe(&pdev->dev, PTR_ERR(pc->pclk), "Can't get APB clk\n");

	ret = clk_prepare_enable(pc->clk);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "Can't prepare enable PWM clk\n");

	ret = clk_prepare_enable(pc->pclk);
	if (ret) {
		dev_err_probe(&pdev->dev, ret, "Can't prepare enable APB clk\n");
		goto err_clk;
	}

	pc->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pc->pinctrl)) {
		dev_err(&pdev->dev, "Get pinctrl failed!\n");
		ret = PTR_ERR(pc->pinctrl);
		goto err_pclk;
	}

	pc->active_state = pinctrl_lookup_state(pc->pinctrl, "active");
	if (IS_ERR(pc->active_state)) {
		dev_err(&pdev->dev, "No active pinctrl state\n");
		ret = PTR_ERR(pc->active_state);
		goto err_pclk;
	}

	platform_set_drvdata(pdev, pc);

	pc->data = id->data;
	pc->chip.dev = &pdev->dev;
	pc->chip.ops = &rockchip_pwm_ops;
	pc->chip.base = of_alias_get_id(pdev->dev.of_node, "pwm");
	pc->chip.npwm = 1;
	pc->clk_rate = clk_get_rate(pc->clk);
	pc->main_version = pc->data->main_version;
	if (pc->main_version < 4) {
		pc->channel_id = rockchip_pwm_get_channel_id(pdev->dev.of_node->full_name);
	} else if (pc->main_version == 4 && rockchip_pwm_get_minor_version(pc) < 1) {
		feature = readl_relaxed(pc->base + pc->data->regs.version);
		pc->channel_id = (feature & CHANNLE_INDEX_MASK) >> CHANNLE_INDEX_SHIFT;
		pc->ir_trans_support = !!(feature & IR_TRANS_SUPPORT);
		pc->freq_meter_support = !!(feature & FREQ_METER_SUPPORT);
		pc->counter_support = !!(feature & COUNTER_SUPPORT);
		pc->wave_support = !!(feature & WAVE_SUPPORT);
		pc->filter_support = !!(feature & FILTER_SUPPORT);
		pc->biphasic_support = !!(feature & BIPHASIC_SUPPORT);
	} else {
		feature = readl_relaxed(pc->base + FEATURE);
		pc->channel_id = (feature & CHANNLE_INDEX_MASK) >> CHANNLE_INDEX_SHIFT;
		pc->ir_trans_support = !!(feature & IR_TRANS_SUPPORT);
		pc->freq_meter_support = !!(feature & FREQ_METER_SUPPORT);
		pc->counter_support = !!(feature & COUNTER_SUPPORT);
		pc->wave_support = !!(feature & WAVE_SUPPORT);
		pc->filter_support = !!(feature & FILTER_SUPPORT);
		pc->biphasic_support = !!(feature & BIPHASIC_SUPPORT);
		pc->ledc_support = !!(feature & LEDC_SUPPORT);
	}
	if (pc->channel_id < 0 || pc->channel_id >= PWM_MAX_CHANNEL_NUM) {
		dev_err(&pdev->dev, "Channel id is out of range: %d\n", pc->channel_id);
		ret = -EINVAL;
		goto err_pclk;
	}

	if (pc->freq_meter_support)
		init_completion(&pc->freq_meter_completion);

	if (pc->biphasic_support)
		init_completion(&pc->biphasic_completion);

	if (pc->data->funcs.irq_handler) {
		/*
		 * For pwm v1-v3, the older platform may not support interrupt, and
		 * common continuous mode can still work well without irq.
		 *
		 * For pwm v4, each channel of every controller supports independent
		 * interrupt and the 'interrupts' property is confirmed to be set
		 * for each pwm node.
		 */
		pc->irq = platform_get_irq(pdev, 0);
		if (pc->irq > 0) {
			irq_flags = pc->main_version >= 4 ? IRQF_NO_SUSPEND :
							    IRQF_NO_SUSPEND | IRQF_SHARED;

			ret = devm_request_irq(&pdev->dev, pc->irq, pc->data->funcs.irq_handler,
					       irq_flags, "rk_pwm_irq", pc);
			if (ret) {
				dev_err(&pdev->dev, "Claim IRQ failed\n");
				goto err_pclk;
			}
		}
	}

	enable_conf = pc->data->enable_conf;
	if (pc->main_version >= 4)
		ctrl = readl_relaxed(pc->base + pc->data->regs.enable);
	else
		ctrl = readl_relaxed(pc->base + pc->data->regs.ctrl);
	enabled = (ctrl & enable_conf) == enable_conf;

	pc->center_aligned =
		device_property_read_bool(&pdev->dev, "center-aligned");

	ret = devm_pwmchip_add(&pdev->dev, &pc->chip);
	if (ret < 0) {
		dev_err_probe(&pdev->dev, ret, "pwmchip_add() failed\n");
		goto err_pclk;
	}

	if (pc->wave_support) {
		if (!pc->clk_osc) {
			dev_err(&pdev->dev, "Can't find OSC clk for wave generator mode\n");
			ret = -EINVAL;
			goto err_pclk;
		}

		ret = clk_prepare(pc->clk_osc);
		if (ret) {
			dev_err(&pdev->dev, "Can't prepare OSC clk for wave generator mode\n");
			goto err_pclk;
		}
	}

#ifdef CONFIG_RC_CORE
	if (pc->ir_trans_support &&
	    device_property_present(&pdev->dev, "rockchip,pwm-ir-transmit")) {
		struct rc_dev *rcdev;

		init_completion(&pc->ir_trans_completion);

		rcdev = devm_rc_allocate_device(&pdev->dev, RC_DRIVER_IR_RAW_TX);
		if (!rcdev)
			goto err_pclk;

		rcdev->priv = pc;
		rcdev->driver_name = "rockchip-pwm-ir-tx";
		rcdev->device_name = "Rockchip IR TX";
		rcdev->tx_ir = rockchip_pwm_ir_transmit;
		ret = devm_rc_register_device(&pdev->dev, rcdev);
		if (ret < 0)
			goto err_pclk;
	}
#endif

	rockchip_pwm_debugfs_init(pc);

	/* Keep the PWM clk enabled if the PWM appears to be up and running. */
	if (!enabled)
		clk_disable(pc->clk);

	clk_disable(pc->pclk);

	return 0;

err_pclk:
	clk_disable_unprepare(pc->pclk);
err_clk:
	clk_disable_unprepare(pc->clk);

	return ret;
}

static int rockchip_pwm_remove(struct platform_device *pdev)
{
	struct rockchip_pwm_chip *pc = platform_get_drvdata(pdev);
	struct pwm_state state;
	u32 val;

	rockchip_pwm_debugfs_deinit(pc);

	/*
	 * For oneshot mode, it is needed to wait for bit PWM_ENABLE
	 * to 0, which is automatic if all periods have been sent.
	 */
	pwm_get_state(&pc->chip.pwms[0], &state);
	if (state.enabled) {
		if (pc->oneshot_en) {
			if (readl_poll_timeout(pc->base + pc->data->regs.ctrl,
					       val, !(val & PWM_ENABLE), 1000, 10 * 1000))
				dev_err(&pdev->dev, "Wait for oneshot to complete failed\n");
		} else {
			state.enabled = false;
			pwm_apply_state(&pc->chip.pwms[0], &state);
		}
	}

	if (pc->oneshot_en)
		clk_disable(pc->pclk);
	clk_unprepare(pc->clk_osc);
	clk_unprepare(pc->pclk);
	clk_unprepare(pc->clk);

	return 0;
}

static struct platform_driver rockchip_pwm_driver = {
	.driver = {
		.name = "rockchip-pwm",
		.of_match_table = rockchip_pwm_dt_ids,
	},
	.probe = rockchip_pwm_probe,
	.remove = rockchip_pwm_remove,
};
#ifdef CONFIG_ROCKCHIP_THUNDER_BOOT
static int __init rockchip_pwm_driver_init(void)
{
	return platform_driver_register(&rockchip_pwm_driver);
}
subsys_initcall(rockchip_pwm_driver_init);

static void __exit rockchip_pwm_driver_exit(void)
{
	platform_driver_unregister(&rockchip_pwm_driver);
}
module_exit(rockchip_pwm_driver_exit);
#else
module_platform_driver(rockchip_pwm_driver);
#endif

MODULE_AUTHOR("Beniamino Galvani <b.galvani@gmail.com>");
MODULE_DESCRIPTION("Rockchip SoC PWM driver");
MODULE_LICENSE("GPL v2");
