// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 *
 * Author: Lingsong Ding <damon.ding@rock-chips.com>
 */

#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include "rk628.h"
#include "rk628_cru.h"
#include "rk628_gpio.h"
#include "rk628_pwm.h"

#define PWM_ENABLE		(1 << 0)
#define PWM_MODE_SHIFT		1
#define PWM_MODE_MASK		(0x3 << PWM_MODE_SHIFT)
#define PWM_ONESHOT		(0 << PWM_MODE_SHIFT)
#define PWM_CONTINUOUS		(1 << PWM_MODE_SHIFT)
#define PWM_DUTY_POSITIVE	(1 << 3)
#define PWM_DUTY_NEGATIVE	(0 << 3)
#define PWM_INACTIVE_NEGATIVE	(0 << 4)
#define PWM_INACTIVE_POSITIVE	(1 << 4)
#define PWM_POLARITY_MASK	(PWM_DUTY_POSITIVE | PWM_INACTIVE_POSITIVE)
#define PWM_OUTPUT_LEFT		(0 << 5)
#define PWM_OUTPUT_CENTER	(1 << 5)
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
#define PWM_ONESHOT_COUNT_MAX	256

#define PWM_EN_CLR_SHIFT	2
#define PWM_EN_CLR		(1 << PWM_EN_CLR_SHIFT)
#define PWM_INT_SET_SHIFT	3

#define PWM_ENABLE_CONF		(PWM_OUTPUT_LEFT | PWM_LP_DISABLE | PWM_ENABLE | PWM_CONTINUOUS)
#define PWM_ENABLE_CONF_MASK	(GENMASK(2, 0) | BIT(5) | BIT(8))

#define PWM_DCLK_RATE		24000000

#define DEBUG_PRINT(args...) \
		do { \
			if (s) \
				seq_printf(s, args); \
			else \
				pr_info(args); \
		} while (0)

static inline struct rk628 *to_rk628(struct rk628_pwm *p)
{
	return container_of(p, struct rk628, pwm);
}

static inline struct rk628_pwm *to_rk628_pwm(struct pwm_chip *pc)
{
	return container_of(pc, struct rk628_pwm, chip);
}

static void rk628_pwm_dclk_enable(struct rk628 *rk628)
{
}

static void rk628_pwm_dclk_disable(struct rk628 *rk628)
{
}

#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
static void rk628_pwm_get_state(struct pwm_chip *chip,
				struct pwm_device *pwm,
				struct pwm_state *state)
#else
static int rk628_pwm_get_state(struct pwm_chip *chip,
			       struct pwm_device *pwm,
			       struct pwm_state *state)
#endif
{
	struct rk628_pwm *p = to_rk628_pwm(chip);
	struct rk628 *rk628 = to_rk628(p);
	u32 enable_conf = PWM_ENABLE_CONF;
	u32 val;
	u32 dclk_div;
	u64 tmp;

	dclk_div = p->oneshot_en ? 2 : 1;

	rk628_i2c_read(rk628, GRF_PWM_PERIOD, &val);
	tmp = val * dclk_div * NSEC_PER_SEC;
	state->period = DIV_ROUND_CLOSEST_ULL(tmp, p->clk_rate);

	rk628_i2c_read(rk628, GRF_PWM_DUTY, &val);
	tmp = val * dclk_div * NSEC_PER_SEC;
	state->duty_cycle =  DIV_ROUND_CLOSEST_ULL(tmp, p->clk_rate);

	rk628_i2c_read(rk628, GRF_PWM_CTRL, &val);
	if (p->oneshot_en)
		enable_conf &= ~PWM_CONTINUOUS;
	state->enabled = (val & enable_conf) == enable_conf;

	if (!(val & PWM_DUTY_POSITIVE))
		state->polarity = PWM_POLARITY_INVERSED;
	else
		state->polarity = PWM_POLARITY_NORMAL;
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
	return 0;
#endif
}

static void rk628_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
				const struct pwm_state *state)
{
	struct rk628_pwm *p = to_rk628_pwm(chip);
	struct rk628 *rk628 = to_rk628(p);
	unsigned long period, duty;
	u64 div;
	u32 ctrl;
	u8 dclk_div = 1;

#ifdef CONFIG_PWM_ROCKCHIP_ONESHOT
	if (state->oneshot_count > 0 && state->oneshot_count <= PWM_ONESHOT_COUNT_MAX)
		dclk_div = 2;
#endif

	/*
	 * Since period and duty cycle registers have a width of 32
	 * bits, every possible input period can be obtained using the
	 * default prescaler value for all practical clock rate values.
	 */
	div = (u64)p->clk_rate * state->period;
	period = DIV_ROUND_CLOSEST_ULL(div, dclk_div * NSEC_PER_SEC);

	div = (u64)p->clk_rate * state->duty_cycle;
	duty = DIV_ROUND_CLOSEST_ULL(div, dclk_div * NSEC_PER_SEC);

	/*
	 * Lock the period and duty of previous configuration, then
	 * change the duty and period, that would not be effective.
	 */
	rk628_i2c_read(rk628, GRF_PWM_CTRL, &ctrl);

#ifdef CONFIG_PWM_ROCKCHIP_ONESHOT
	if (state->oneshot_count > 0 && state->oneshot_count <= PWM_ONESHOT_COUNT_MAX) {
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

		p->oneshot_en = true;
		ctrl &= ~PWM_MODE_MASK;
		ctrl |= PWM_ONESHOT;

		ctrl &= ~PWM_ONESHOT_COUNT_MASK;
		ctrl |= (state->oneshot_count - 1) << PWM_ONESHOT_COUNT_SHIFT;

		rk628_i2c_read(rk628, GRF_PWM_STATUS, &int_ctrl);
		int_ctrl |= PWM_EN_CLR;
		rk628_i2c_write(rk628, GRF_PWM_STATUS, int_ctrl);
	} else {
		u32 int_ctrl;

		ctrl &= ~PWM_SCALE_MASK;
		ctrl &= ~PWM_CLK_SEL_MASK;
		ctrl |= PWM_SEL_NO_SCALED_CLOCK;

		if (state->oneshot_count)
			dev_err(chip->dev, "Oneshot_count must be between 1 and 256.\n");

		p->oneshot_en = false;
		ctrl &= ~PWM_MODE_MASK;
		ctrl |= PWM_CONTINUOUS;

		ctrl &= ~PWM_ONESHOT_COUNT_MASK;

		rk628_i2c_read(rk628, GRF_PWM_STATUS, &int_ctrl);
		int_ctrl &= ~PWM_EN_CLR;
		rk628_i2c_write(rk628, GRF_PWM_STATUS, int_ctrl);
	}
#endif

	rk628_i2c_write(rk628, GRF_PWM_PERIOD, period);
	rk628_i2c_write(rk628, GRF_PWM_DUTY, duty);

	ctrl &= ~PWM_POLARITY_MASK;
	if (state->polarity == PWM_POLARITY_INVERSED)
		ctrl |= PWM_DUTY_NEGATIVE | PWM_INACTIVE_POSITIVE;
	else
		ctrl |= PWM_DUTY_POSITIVE | PWM_INACTIVE_NEGATIVE;

	rk628_i2c_write(rk628, GRF_PWM_CTRL, ctrl);
}

static int rk628_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm, bool enable)
{
	struct rk628_pwm *p = to_rk628_pwm(chip);
	struct rk628 *rk628 = to_rk628(p);
	u32 val;

	if (enable)
		rk628_pwm_dclk_enable(rk628);

	rk628_i2c_read(rk628, GRF_PWM_CTRL, &val);
	val &= ~PWM_ENABLE_CONF_MASK;

	if (p->center_aligned)
		val |= PWM_OUTPUT_CENTER;

	if (enable) {
		val |= PWM_ENABLE_CONF;
		if (p->oneshot_en)
			val &= ~PWM_CONTINUOUS;
	} else {
		val &= ~PWM_ENABLE_CONF;
	}

	rk628_i2c_write(rk628, GRF_PWM_CTRL, val);

	if (!enable)
		rk628_pwm_dclk_disable(rk628);

	return 0;
}

static void rk628_pwm_set_iomux(struct pwm_chip *chip)
{
	struct rk628_pwm *p = to_rk628_pwm(chip);
	struct rk628 *rk628 = to_rk628(p);

	/*
	 * The pwm output iomux:
	 * m0: gpio3b4
	 * m1: gpio0a6
	 */
	if (p->is_output_m1) {
		rk628_i2c_write(rk628, GRF_GPIO0AB_SEL_CON, 0x03000300);
		rk628_i2c_write(rk628, RK628_GPIO0_BASE + GPIO_SWPORT_DDR_L, 0x00400040);
	} else {
		rk628_i2c_write(rk628, GRF_GPIO3AB_SEL_CON, 0x30003000);
		rk628_i2c_write(rk628, RK628_GPIO3_BASE + GPIO_SWPORT_DDR_L, 0x10001000);
	}
}

static int rk628_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			   const struct pwm_state *state)
{
	struct pwm_state curstate;
	bool enabled;
	int ret = 0;

	pwm_get_state(pwm, &curstate);
	enabled = curstate.enabled;

	if (state->polarity != curstate.polarity && enabled) {
		ret = rk628_pwm_enable(chip, pwm, false);
		if (ret)
			goto out;
		enabled = false;
	}

	rk628_pwm_config(chip, pwm, state);
	if (state->enabled != enabled) {
		ret = rk628_pwm_enable(chip, pwm, state->enabled);
		if (ret)
			goto out;
	}

	if (state->enabled)
		rk628_pwm_set_iomux(chip);

out:
	return ret;
}

static const struct pwm_ops rk628_pwm_ops = {
	.get_state = rk628_pwm_get_state,
	.apply = rk628_pwm_apply,
	.owner = THIS_MODULE,
};

int rk628_pwm_probe(struct rk628 *rk628, struct device_node *pwm_np)
{
	struct pwm_chip *chip = &rk628->pwm.chip;
	struct platform_device *pd;
	int ret;

	pd = platform_device_alloc("rk628-pwm", -1);
	if (!pd) {
		dev_err(rk628->dev, "failed to alloc pwm platform device\n");
		return -EINVAL;
	}

	ret = platform_device_add(pd);
	if (ret) {
		dev_err(rk628->dev, "failed to add pwm platform device\n");
		return -EINVAL;
	}

	chip->dev = &pd->dev;
	chip->dev->of_node = pwm_np;
	chip->ops = &rk628_pwm_ops;
	chip->base = -1;
	chip->npwm = 1;
	chip->of_xlate = of_pwm_xlate_with_flags;
	chip->of_pwm_n_cells = 3;

	rk628->pwm.clk_rate = PWM_DCLK_RATE;

	rk628->pwm.center_aligned = of_property_read_bool(pwm_np, "pwm,center-aligned");
	rk628->pwm.is_output_m1 = of_property_read_bool(pwm_np, "pwm,output-m1");
	rk628->pwm_bl_en = of_property_read_bool(pwm_np, "pwm,backlight");

	ret = pwmchip_add(chip);
	if (ret < 0) {
		dev_err(rk628->dev, "pwmchip_add() failed: %d\n", ret);
		return ret;
	}

	/*
	 * To avoid warning in devm_pwm_get() process.
	 */
	pd->dev.links.status = DL_DEV_PROBING;

	return 0;
}

void rk628_pwm_init(struct rk628 *rk628)
{
	u32 ctrl;
	bool enabled;

	rk628_i2c_read(rk628, GRF_PWM_CTRL, &ctrl);
	enabled = (ctrl & PWM_ENABLE_CONF) == PWM_ENABLE_CONF;
	if (!enabled)
		rk628_pwm_dclk_disable(rk628);
}

static int rk628_debugfs_pwm_regs_dump(struct seq_file *s, void *data)
{
	struct rk628 *rk628 = s->private;
	u32 val;

	DEBUG_PRINT("pwm regs:\n");
	rk628_i2c_read(rk628, GRF_PWM_PERIOD, &val);
	DEBUG_PRINT("period: 0x%08x\n", val);
	rk628_i2c_read(rk628, GRF_PWM_DUTY, &val);
	DEBUG_PRINT("duty:   0x%08x\n", val);
	rk628_i2c_read(rk628, GRF_PWM_CTRL, &val);
	DEBUG_PRINT("ctrl:   0x%08x\n", val);
	rk628_i2c_read(rk628, GRF_PWM_CH_CNT, &val);
	DEBUG_PRINT("ch_cnt: 0x%08x\n", val);
	rk628_i2c_read(rk628, GRF_PWM_STATUS, &val);
	DEBUG_PRINT("status: 0x%08x\n", val);

	return 0;
}

static int rk628_debugfs_pwm_regs_open(struct inode *inode, struct file *file)
{
	struct rk628 *rk628 = inode->i_private;

	return single_open(file, rk628_debugfs_pwm_regs_dump, rk628);
}

static const struct file_operations rk628_debugfs_pwm_regs_fops = {
	.owner = THIS_MODULE,
	.open = rk628_debugfs_pwm_regs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void rk628_pwm_create_debugfs_file(struct rk628 *rk628)
{
	debugfs_create_file("pwm_regs", 0400, rk628->debug_dir, rk628,
			    &rk628_debugfs_pwm_regs_fops);
}
