// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 * Author: Lingsong Ding <damon.ding@rock-chips.com>
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/pwm-rockchip.h>
#include <linux/syscalls.h>
#include <linux/delay.h>
#include <linux/types.h>

#define PWM_CONTROLLER_NUM_MAX	4
#define PWM_CHANNEL_NUM_MAX	8

#define PWM_WAVE_8BIT_TEST 1

/* 400k pwm_dclk src */
#ifdef PWM_WAVE_8BIT_TEST
#define PWM_TABLE_MAX		256
#define PWM_WIDTH_MODE		PWM_WAVE_TABLE_8BITS_WIDTH
/* in nanoseconds */
#define PWM_WAVE_STEP		2500
#define PWM_WAVE_RPT		30
#else
#define PWM_TABLE_MAX		100
#define PWM_WIDTH_MODE		PWM_WAVE_TABLE_16BITS_WIDTH
/* in nanoseconds */
#define PWM_WAVE_STEP		10000
#define PWM_WAVE_RPT		10
#endif

struct pwm_test_data {
	struct pwm_device *pwm_dev;
	struct device *dev;
};

static struct pwm_test_data *g_pwm_test_data[PWM_CONTROLLER_NUM_MAX][PWM_CHANNEL_NUM_MAX] = {};

struct pwm_device *global_pdev;
static int global_channel_id = -1;

static u64 table[256] = {};

enum pwm_cmd_type {
	UNSUPPORTED_TYPE = -1,
	PWM_SET_ENABLE = 0,
	PWM_SET_CONTINOUS,
	PWM_SET_ONESHOT,
	PWM_SET_CAPTURE,
	PWM_SET_COUNTER,
	PWM_SET_FREQ_METER,
	PWM_SET_BIPHASIC,
	PWM_SET_WAVE,
	PWM_GLOBAL_CTRL,
	PWM_TEST_HELP,
};

static enum pwm_cmd_type pwm_rockchip_test_parse_cmd(char *cmd_type)
{
	if (!strcmp(cmd_type, "enable"))
		return PWM_SET_ENABLE;
	else if (!strcmp(cmd_type, "continuous"))
		return PWM_SET_CONTINOUS;
#ifdef CONFIG_PWM_ROCKCHIP_ONESHOT
	else if (!strcmp(cmd_type, "oneshot"))
		return PWM_SET_ONESHOT;
#endif
	else if (!strcmp(cmd_type, "capture"))
		return PWM_SET_CAPTURE;
	else if (!strcmp(cmd_type, "counter"))
		return PWM_SET_COUNTER;
	else if (!strcmp(cmd_type, "frequency"))
		return PWM_SET_FREQ_METER;
	else if (!strcmp(cmd_type, "biphasic"))
		return PWM_SET_BIPHASIC;
	else if (!strcmp(cmd_type, "wave"))
		return PWM_SET_WAVE;
	else if (!strcmp(cmd_type, "global"))
		return PWM_GLOBAL_CTRL;
	else if (!strcmp(cmd_type, "help"))
		return PWM_TEST_HELP;
	else
		return UNSUPPORTED_TYPE;
}

static int get_biphasic_mode(char *mode)
{
	if (!strcmp(mode, "mode0"))
		return PWM_BIPHASIC_COUNTER_MODE0;
	else if (!strcmp(mode, "mode1"))
		return PWM_BIPHASIC_COUNTER_MODE1;
	else if (!strcmp(mode, "mode2"))
		return PWM_BIPHASIC_COUNTER_MODE2;
	else if (!strcmp(mode, "mode3"))
		return PWM_BIPHASIC_COUNTER_MODE3;
	else if (!strcmp(mode, "mode4"))
		return PWM_BIPHASIC_COUNTER_MODE4;
	else
		return -EINVAL;
}

static void pwm_rockchip_test_help_info(void)
{
	pr_info("------------------------------------------------------------------------------------\n");
	pr_info("                                      HELP INFO\n");
	pr_info("------------------------------------------------------------------------------------\n");
	pr_info("pwm test commands format:\n");
	pr_info("echo cmd_type controller_id channel_id para0 para1 ... > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo global cmd_type controller_id channel_id > /dev/pwm_rockchip_misc_test\n");
	pr_info("\n");
	pr_info("continuous mode demo:\n");
	pr_info("echo continuous 0 0 10000 5000 normal > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo enable 0 0 true > /dev/pwm_rockchip_misc_test\n");
	pr_info("\n");
	pr_info("oneshot mode demo:\n");
	pr_info("echo oneshot 0 1 10000 5000 0 normal 10 0 > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo enable 0 1 true > /dev/pwm_rockchip_misc_test\n");
	pr_info("\n");
	pr_info("capture mode demo:\n");
	pr_info("echo capture 1 0 1000 > /dev/pwm_rockchip_misc_test\n");
	pr_info("\n");
	pr_info("counter mode demo:\n");
	pr_info("echo counter 1 0 io 1000 > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo counter 1 1 cru 1000 > /dev/pwm_rockchip_misc_test\n");
	pr_info("\n");
	pr_info("frequency meter mode demo:\n");
	pr_info("echo frequency 1 2 io 1000 > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo frequency 1 3 cru 1000 > /dev/pwm_rockchip_misc_test\n");
	pr_info("\n");
	pr_info("biphasic counter mode demo(for mode0 as frequency meter):\n");
	pr_info("echo biphasic 1 4 freq 1000 > /dev/pwm_rockchip_misc_test\n");
	pr_info("biphasic counter mode demo:\n");
	pr_info("echo biphasic 1 5 mode0 1000 > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo biphasic 1 5 mode1 1000 > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo biphasic 1 5 mode2 1000 > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo biphasic 1 5 mode3 1000 > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo biphasic 1 5 mode4 1000 > /dev/pwm_rockchip_misc_test\n");
	pr_info("\n");
	pr_info("global control demo:\n");
	pr_info("echo global join 0 0 1 2 3 > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo global grant 0 0 > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo oneshot 0 0 10000 1000 2000 normal 10 0 > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo oneshot 0 1 10000 1000 4000 normal 10 0 > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo oneshot 0 2 10000 1000 6000 normal 10 0 > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo oneshot 0 3 10000 1000 8000 normal 10 0 > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo global update 0 0 > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo global enable 0 0 > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo global reclaim 0 0 > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo global exit 0 0 1 2 3 > /dev/pwm_rockchip_misc_test\n");
	pr_info("\n");
	pr_info("wave generator demo:\n");
	pr_info("echo wave 0 1 true > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo continuous 0 1 640000 320000 normal > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo continuous 0 1 1000000 500000 normal > /dev/pwm_rockchip_misc_test\n");
	pr_info("echo enable 0 1 true > /dev/pwm_rockchip_misc_test\n");
	pr_info("------------------------------------------------------------------------------------\n");
}

static struct pwm_device *pwm_rockchip_test_get_pwm_dev(char *controller, char *channel,
							int *controller_id, int *channel_id)
{
	struct pwm_device *pdev = NULL;
	int ret = 0;

	if (!controller || !channel || !controller_id || !channel_id)
		return NULL;

	ret = kstrtoint(controller, 10, controller_id);
	if (ret || *controller_id >= PWM_CONTROLLER_NUM_MAX) {
		pr_err("pwm controller id should be 0 to %d\n", PWM_CONTROLLER_NUM_MAX - 1);
		return NULL;
	}

	ret = kstrtoint(channel, 10, channel_id);
	if (ret || *channel_id >= PWM_CHANNEL_NUM_MAX) {
		pr_err("pwm channel id should be 0 to %d\n", PWM_CHANNEL_NUM_MAX - 1);
		return NULL;
	}

	if (!g_pwm_test_data[*controller_id][*channel_id]) {
		pr_err("pwm%d_%d should be bound first\n", *controller_id, *channel_id);
		return NULL;
	}

	if (g_pwm_test_data[*controller_id][*channel_id]->pwm_dev)
		pdev = g_pwm_test_data[*controller_id][*channel_id]->pwm_dev;

	return pdev;
}

static ssize_t pwm_rockchip_test_write(struct file *file, const char __user *buf,
				       size_t n, loff_t *offset)
{
	struct pwm_device *pdev;
	struct pwm_state state;
	struct pwm_capture cap_res;
	struct rockchip_pwm_wave_table duty_table;
	struct rockchip_pwm_wave_config wave_config;
	struct rockchip_pwm_biphasic_config biphasic_config;
	enum rockchip_pwm_freq_meter_input_sel freq_input_sel;
	enum rockchip_pwm_counter_input_sel counter_input_sel;
	enum rockchip_pwm_biphasic_mode biphasic_mode;
	enum pwm_cmd_type cmd_type;
	enum pwm_polarity polarity;
	unsigned long timeout_ms;
	unsigned long freq_hz;
	unsigned long counter_res;
	unsigned long biphasic_res;
	int controller_id, channel_id;
	int period, duty;
#ifdef CONFIG_PWM_ROCKCHIP_ONESHOT
	int duty_offset, rpt_first, rpt_second;
#endif
	int argc = 0;
	int i = 0;
	int ret = 0;
	char tmp[256] = {};
	char *argv[16] = {};
	char *cmd, *data;
	bool enable;

	memset(tmp, 0, sizeof(tmp));
	if (copy_from_user(tmp, buf, n))
		return -EFAULT;
	cmd = tmp;
	data = tmp;

	while (data < (tmp + n)) {
		data = strstr(data, " ");
		if (!data)
			break;
		*data = 0;
		argv[argc] = ++data;
		argc++;
		if (argc >= 16)
			break;
	}
	tmp[n - 1] = 0;

	if (!cmd) {
		ret = -EINVAL;
		goto exit;
	}
	cmd_type = pwm_rockchip_test_parse_cmd(cmd);

	switch (cmd_type) {
	case PWM_SET_ENABLE:
		pdev = pwm_rockchip_test_get_pwm_dev(argv[0], argv[1],
						     &controller_id, &channel_id);
		if (!pdev) {
			pr_err("failed to get pwm device\n");
			ret = -EINVAL;
			goto exit;
		}
		if (argv[2]) {
			if (!strcmp(argv[2], "true")) {
				enable = true;
			} else if (!strcmp(argv[2], "false")) {
				enable = false;
			} else {
				pr_err("enable state should be true or false\n");
				ret = -EINVAL;
				goto exit;
			}
		} else {
			pr_err("failed to parse enable for pwm%d_%d in %s mode\n",
			       controller_id, channel_id, cmd);
			goto exit;
		}

		pwm_get_state(pdev, &state);
		state.enabled = enable;

		ret = pwm_apply_state(pdev, &state);
		if (ret) {
			pr_err("failed to %s pwm%d_%d\n",
			       enable ? "enable" : "disable", controller_id, channel_id);
			ret = -EINVAL;
			goto exit;
		}

		pr_info("%s pwm%d_%d\n", enable ? "enable" : "disable", controller_id, channel_id);
		break;
	case PWM_SET_CONTINOUS:
		pdev = pwm_rockchip_test_get_pwm_dev(argv[0], argv[1],
						     &controller_id, &channel_id);
		if (!pdev) {
			pr_err("failed to get pwm device\n");
			ret = -EINVAL;
			goto exit;
		}
		ret = kstrtoint(argv[2], 10, &period);
		if (ret) {
			pr_err("failed to parse period for pwm%d_%d in %s mode\n",
			       controller_id, channel_id, cmd);
			ret = -EINVAL;
			goto exit;
		}
		ret = kstrtoint(argv[3], 10, &duty);
		if (ret) {
			pr_err("failed to parse duty for pwm%d_%d in %s mode\n",
			       controller_id, channel_id, cmd);
			ret = -EINVAL;
			goto exit;
		}
		if (argv[4]) {
			if (!strcmp(argv[4], "inversed")) {
				polarity = PWM_POLARITY_INVERSED;
			} else if (!strcmp(argv[4], "normal")) {
				polarity = PWM_POLARITY_NORMAL;
			} else {
				pr_err("polarity should be inversed or normal\n");
				ret = -EINVAL;
				goto exit;
			}
		} else {
			pr_err("failed to parse polarity for pwm%d_%d in %s mode\n",
			       controller_id, channel_id, cmd);
			ret = -EINVAL;
			goto exit;
		}

		pwm_get_state(pdev, &state);
		state.period = period;
		state.duty_cycle = duty;
		state.polarity = polarity;

		ret = pwm_apply_state(pdev, &state);
		if (ret) {
			pr_err("failed to set %s mode for pwm%d_%d\n",
			       cmd, controller_id, channel_id);
			return -EINVAL;
		}

		pr_info("set %s mode for pwm%d_%d: period = %dns, duty = %dns, polarity = %s\n",
			cmd, controller_id, channel_id,
			period, duty, polarity ? "inversed" : "normal");
		break;
#ifdef CONFIG_PWM_ROCKCHIP_ONESHOT
	case PWM_SET_ONESHOT:
		pdev = pwm_rockchip_test_get_pwm_dev(argv[0], argv[1],
						     &controller_id, &channel_id);
		if (!pdev) {
			pr_err("failed to get pwm device\n");
			ret = -EINVAL;
			goto exit;
		}
		ret = kstrtoint(argv[2], 10, &period);
		if (ret) {
			pr_err("failed to parse period for pwm%d_%d in %s mode\n",
			       controller_id, channel_id, cmd);
			ret = -EINVAL;
			goto exit;
		}
		ret = kstrtoint(argv[3], 10, &duty);
		if (ret) {
			pr_err("failed to parse duty for pwm%d_%d in %s mode\n",
			       controller_id, channel_id, cmd);
			ret = -EINVAL;
			goto exit;
		}
		ret = kstrtoint(argv[4], 10, &duty_offset);
		if (ret) {
			pr_err("failed to parse offset for pwm%d_%d in %s mode\n",
			       controller_id, channel_id, cmd);
			ret = -EINVAL;
			goto exit;
		}
		if (argv[5]) {
			if (!strcmp(argv[5], "inversed")) {
				polarity = PWM_POLARITY_INVERSED;
			} else if (!strcmp(argv[5], "normal")) {
				polarity = PWM_POLARITY_NORMAL;
			} else {
				pr_err("polarity should be inversed or normal\n");
				ret = -EINVAL;
				goto exit;
			}
		} else {
			pr_err("failed to parse polarity for pwm%d_%d in %s mode\n",
			       controller_id, channel_id, cmd);
			ret = -EINVAL;
			goto exit;
		}
		ret = kstrtoint(argv[6], 10, &rpt_first);
		if (ret) {
			pr_err("failed to parse rpt_first for pwm%d_%d in %s mode\n",
			       controller_id, channel_id, cmd);
			ret = -EINVAL;
			goto exit;
		}
		ret = kstrtoint(argv[7], 10, &rpt_second);
		if (ret) {
			pr_err("failed to parse rpt_second for pwm%d_%d in %s mode\n",
			       controller_id, channel_id, cmd);
			ret = -EINVAL;
			goto exit;
		}

		pwm_get_state(pdev, &state);
		state.period = period;
		state.duty_cycle = duty;
		state.duty_offset = duty_offset;
		state.polarity = polarity;
		state.oneshot_count = rpt_first;
		state.oneshot_repeat = rpt_second;

		ret = pwm_apply_state(pdev, &state);
		if (ret) {
			pr_err("failed to set %s mode for pwm%d_%d\n",
			       cmd, controller_id, channel_id);
			return -EINVAL;
		}

		pr_info("set %s mode for pwm%d_%d: period = %dns, duty = %dns, offset = %dns, polarity = %s rpt = (%d, %d)\n",
			cmd, controller_id, channel_id, period, duty, duty_offset, polarity ? "inversed" : "normal",
			rpt_first, rpt_second);
		break;
#endif
	case PWM_SET_CAPTURE:
		pdev = pwm_rockchip_test_get_pwm_dev(argv[0], argv[1],
						     &controller_id, &channel_id);
		if (!pdev) {
			pr_err("failed to get pwm device\n");
			ret = -EINVAL;
			goto exit;
		}
		ret = kstrtoul(argv[2], 10, &timeout_ms);
		if (ret) {
			pr_err("failed to parse timeout_ms for pwm%d_%d in %s mode\n",
			       controller_id, channel_id, cmd);
			ret = -EINVAL;
			goto exit;
		}

		ret = pwm_capture(pdev, &cap_res, timeout_ms);
		if (ret) {
			pr_err("failed to set %s mode for pwm%d_%d\n",
			       cmd, controller_id, channel_id);
			return -EINVAL;
		}

		pr_info("set %s mode for pwm%d_%d: timeout_ms = %ld, result: period = %dns, duty = %dns\n",
			cmd, controller_id, channel_id, timeout_ms, cap_res.period, cap_res.duty_cycle);
		break;
	case PWM_SET_COUNTER:
		pdev = pwm_rockchip_test_get_pwm_dev(argv[0], argv[1],
						     &controller_id, &channel_id);
		if (!pdev) {
			pr_err("failed to get pwm device\n");
			ret = -EINVAL;
			goto exit;
		}
		if (argv[2]) {
			if (!strcmp(argv[2], "cru")) {
				counter_input_sel = PWM_COUNTER_INPUT_FROM_CRU;
			} else if (!strcmp(argv[2], "io")) {
				counter_input_sel = PWM_COUNTER_INPUT_FROM_IO;
			} else {
				pr_err("input_sel should be cru or io\n");
				ret = -EINVAL;
				goto exit;
			}
		} else {
			pr_err("failed to parse input_sel for pwm%d_%d in %s mode\n",
			       controller_id, channel_id, cmd);
			ret = -EINVAL;
			goto exit;
		}
		ret = kstrtoul(argv[3], 10, &timeout_ms);
		if (ret) {
			pr_err("failed to parse timeout_ms for pwm%d_%d in %s mode\n",
			       controller_id, channel_id, cmd);
			ret = -EINVAL;
			goto exit;
		}

		ret = rockchip_pwm_set_counter(pdev, counter_input_sel, true);
		if (ret) {
			pr_err("failed to enable %s mode for pwm%d_%d\n",
			       cmd, controller_id, channel_id);
			return -EINVAL;
		}

		msleep(timeout_ms);

		ret = rockchip_pwm_get_counter_result(pdev, &counter_res, true);
		if (ret) {
			pr_err("failed to get %s mode result for pwm%d_%d\n",
			       cmd, controller_id, channel_id);
			return -EINVAL;
		}

		ret = rockchip_pwm_set_counter(pdev, 0, false);
		if (ret) {
			pr_err("failed to disable %s mode for pwm%d_%d\n",
			       cmd, controller_id, channel_id);
			return -EINVAL;
		}

		pr_info("set %s mode for pwm%d_%d: timeout_ms = %ld, result: counter_res = %ld\n",
			cmd, controller_id, channel_id, timeout_ms, counter_res);
		break;
	case PWM_SET_FREQ_METER:
		pdev = pwm_rockchip_test_get_pwm_dev(argv[0], argv[1],
						     &controller_id, &channel_id);
		if (!pdev) {
			pr_err("failed to get pwm device\n");
			ret = -EINVAL;
			goto exit;
		}
		if (argv[2]) {
			if (!strcmp(argv[2], "cru")) {
				freq_input_sel = PWM_FREQ_METER_INPUT_FROM_CRU;
			} else if (!strcmp(argv[2], "io")) {
				freq_input_sel = PWM_FREQ_METER_INPUT_FROM_IO;
			} else {
				pr_err("input_sel should be cru or io\n");
				ret = -EINVAL;
				goto exit;
			}
		} else {
			pr_err("failed to parse input_sel for pwm%d_%d in %s mode\n",
			       controller_id, channel_id, cmd);
			ret = -EINVAL;
			goto exit;
		}
		ret = kstrtoul(argv[3], 10, &timeout_ms);
		if (ret) {
			pr_err("failed to parse timeout_ms for pwm%d_%d in %s mode\n",
			       controller_id, channel_id, cmd);
			ret = -EINVAL;
			goto exit;
		}

		ret = rockchip_pwm_set_freq_meter(pdev, timeout_ms, freq_input_sel, &freq_hz);
		if (ret) {
			pr_err("failed to enable %s mode for pwm%d_%d\n",
			       cmd, controller_id, channel_id);
			return -EINVAL;
		}

		pr_info("set %s mode for pwm%d_%d: timeout_ms = %ld, result: frequency = %ldHz\n",
			cmd, controller_id, channel_id, timeout_ms, freq_hz);
		break;
	case PWM_SET_BIPHASIC:
		pdev = pwm_rockchip_test_get_pwm_dev(argv[0], argv[1],
						     &controller_id, &channel_id);
		if (!pdev) {
			pr_err("failed to get pwm device\n");
			ret = -EINVAL;
			goto exit;
		}
		if (argv[2]) {
			if (!strcmp(argv[2], "freq")) {
				biphasic_mode = PWM_BIPHASIC_COUNTER_MODE0_FREQ;
			} else {
				ret = get_biphasic_mode(argv[2]);
				if (ret < 0) {
					pr_err("unsupported biphasic counter mode\n");
					ret = -EINVAL;
					goto exit;
				}
				biphasic_mode = ret;
			}
		} else {
			pr_err("failed to parse biphasic_mode for pwm%d_%d in %s mode\n",
			       controller_id, channel_id, cmd);
			ret = -EINVAL;
			goto exit;
		}
		ret = kstrtoul(argv[3], 10, &timeout_ms);
		if (ret) {
			pr_err("failed to parse timeout_ms for pwm%d_%d in %s mode\n",
			       controller_id, channel_id, cmd);
			ret = -EINVAL;
			goto exit;
		}

		biphasic_config.enable = true;
		biphasic_config.is_continuous = false;
		biphasic_config.mode = biphasic_mode;
		biphasic_config.delay_ms = timeout_ms;
		if (biphasic_mode != PWM_BIPHASIC_COUNTER_MODE0_FREQ && timeout_ms == 0)
			biphasic_config.is_continuous = true;

		ret = rockchip_pwm_set_biphasic(pdev, &biphasic_config, &biphasic_res);
		if (ret) {
			pr_err("failed to enable %s mode for pwm%d_%d\n",
			cmd, controller_id, channel_id);
			return -EINVAL;
		}

		pr_info("set %s mode for pwm%d_%d: timeout_ms = %ld, result%s = %ld%s\n",
			cmd, controller_id, channel_id, timeout_ms,
			biphasic_mode == PWM_BIPHASIC_COUNTER_MODE0_FREQ ? "(frequency)" : "",
			biphasic_res,
			biphasic_mode == PWM_BIPHASIC_COUNTER_MODE0_FREQ ? "Hz" : "");
		break;
	case PWM_SET_WAVE:
		pdev = pwm_rockchip_test_get_pwm_dev(argv[0], argv[1],
						     &controller_id, &channel_id);
		if (!pdev) {
			pr_err("failed to get pwm device\n");
			ret = -EINVAL;
			goto exit;
		}
		if (argv[2]) {
			if (!strcmp(argv[2], "true")) {
				enable = true;
			} else if (!strcmp(argv[2], "false")) {
				enable = false;
			} else {
				pr_err("enable state should be true or false\n");
				ret = -EINVAL;
				goto exit;
			}
		} else {
			pr_err("failed to parse enable for pwm%d_%d in %s mode\n",
			       controller_id, channel_id, cmd);
			goto exit;
		}

		for (i = 0; i < PWM_TABLE_MAX; i++)
			table[i] = i * PWM_WAVE_STEP;
		duty_table.table = table;
		duty_table.offset = (channel_id % 3) * PWM_TABLE_MAX;
		duty_table.len = PWM_TABLE_MAX;

		wave_config.duty_table = &duty_table;
		wave_config.period_table = NULL;
		wave_config.clk_src = PWM_SELECT_CLK_PWM;
		wave_config.mem_clk_src = PWM_SELECT_CLK_PWM_OSC;
		wave_config.width_mode = PWM_WIDTH_MODE;
		wave_config.update_mode = PWM_WAVE_INCREASING_THEN_DECREASING;
		wave_config.enable = enable;
		wave_config.duty_en = true;
		wave_config.period_en = false;
		wave_config.clk_rate = 400000;
		wave_config.rpt = PWM_WAVE_RPT;
		wave_config.duty_max = (channel_id % 3 + 1) * PWM_TABLE_MAX - 1;
		wave_config.duty_min = (channel_id % 3) * PWM_TABLE_MAX;
		wave_config.period_max = 0;
		wave_config.period_min = 0;
		wave_config.offset = 0;
		wave_config.middle = PWM_TABLE_MAX / 2;
		wave_config.max_hold = 3;
		wave_config.min_hold = 0;
		wave_config.middle_hold = 2;

		ret = rockchip_pwm_set_wave(pdev, &wave_config);
		if (ret) {
			pr_err("failed to %s pwm%d_%d\n",
			enable ? "enable" : "disable", controller_id, channel_id);
			ret = -EINVAL;
			goto exit;
		}

		pr_info("%s %s mode for pwm%d_%d: table_len = %d, table_step = %d\n",
			argv[2], cmd, controller_id, channel_id, PWM_TABLE_MAX, PWM_WAVE_STEP);
		break;
	case PWM_GLOBAL_CTRL:
		if (!argv[0]) {
			ret = -EINVAL;
			goto exit;
		}

		if (!strcmp(argv[0], "join")) {
			i = 2;
			while (argv[i]) {
				pdev = pwm_rockchip_test_get_pwm_dev(argv[1], argv[i],
								     &controller_id, &channel_id);
				if (!pdev) {
					pr_err("failed to get pwm device\n");
					ret = -EINVAL;
					goto exit;
				}
				rockchip_pwm_global_ctrl(pdev, PWM_GLOBAL_CTRL_JOIN);
				pr_info("pwm%d_%d join global control group\n",
					controller_id, channel_id);
				i++;
			}
		} else if (!strcmp(argv[0], "exit")) {
			i = 2;
			while (argv[i]) {
				pdev = pwm_rockchip_test_get_pwm_dev(argv[1], argv[i],
								     &controller_id, &channel_id);
				if (!pdev) {
					pr_err("failed to get pwm device\n");
					ret = -EINVAL;
					goto exit;
				}
				rockchip_pwm_global_ctrl(pdev, PWM_GLOBAL_CTRL_EXIT);
				pr_info("pwm%d_%d exit global control group\n",
					controller_id, channel_id);
				i++;
			}
		} else if (!strcmp(argv[0], "grant")) {
			pdev = pwm_rockchip_test_get_pwm_dev(argv[1], argv[2],
							     &controller_id, &channel_id);
			if (!pdev) {
				pr_err("failed to get pwm device\n");
				ret = -EINVAL;
				goto exit;
			}
			ret = rockchip_pwm_global_ctrl(pdev, PWM_GLOBAL_CTRL_GRANT);
			if (ret) {
				ret = -EINVAL;
				goto exit;
			}
			global_channel_id = channel_id;
			global_pdev = pdev;
			pr_info("pwm%d_%d is granted global control\n",
				controller_id, global_channel_id);
		} else if (!strcmp(argv[0], "reclaim")) {
			pdev = pwm_rockchip_test_get_pwm_dev(argv[1], argv[2],
							     &controller_id, &channel_id);
			if (!pdev) {
				pr_err("failed to get pwm device\n");
				ret = -EINVAL;
				goto exit;
			}
			rockchip_pwm_global_ctrl(pdev, PWM_GLOBAL_CTRL_RECLAIM);
			pr_info("pwm%d_%d is reclaimed global control\n",
				controller_id, global_channel_id);
			global_pdev = NULL;
			global_channel_id = -1;
		} else if (!strcmp(argv[0], "update")) {
			pdev = pwm_rockchip_test_get_pwm_dev(argv[1], argv[2],
							     &controller_id, &channel_id);
			if (!pdev) {
				pr_err("failed to get pwm device\n");
				ret = -EINVAL;
				goto exit;
			}
			rockchip_pwm_global_ctrl(pdev, PWM_GLOBAL_CTRL_UPDATE);
			pr_info("pwm%d_%d executes global update\n",
				controller_id, global_channel_id);
		} else if (!strcmp(argv[0], "enable")) {
			pdev = pwm_rockchip_test_get_pwm_dev(argv[1], argv[2],
							     &controller_id, &channel_id);
			if (!pdev) {
				pr_err("failed to get pwm device\n");
				ret = -EINVAL;
				goto exit;
			}
			rockchip_pwm_global_ctrl(pdev, PWM_GLOBAL_CTRL_ENABLE);
			pr_info("pwm%d_%d executes global enable\n",
				controller_id, global_channel_id);
		} else if (!strcmp(argv[0], "disable")) {
			pdev = pwm_rockchip_test_get_pwm_dev(argv[1], argv[2],
							     &controller_id, &channel_id);
			if (!pdev) {
				pr_err("failed to get pwm device\n");
				ret = -EINVAL;
				goto exit;
			}
			rockchip_pwm_global_ctrl(pdev, PWM_GLOBAL_CTRL_DISABLE);
			pr_info("pwm%d_%d executes global disable\n",
				controller_id, global_channel_id);
		} else {
			pr_err("unsupported global control command\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case PWM_TEST_HELP:
	default:
		pwm_rockchip_test_help_info();
		break;
	}

	return n;

exit:
	pwm_rockchip_test_help_info();
	return ret;
}

static const struct file_operations pwm_rockchip_test_fops = {
	.write = pwm_rockchip_test_write,
};

static struct miscdevice pwm_rockchip_test_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "pwm_rockchip_misc_test",
	.fops = &pwm_rockchip_test_fops,
};

static int pwm_rockchip_test_probe(struct platform_device *pdev)
{
	struct pwm_test_data *test_data = NULL;
	struct pwm_device *pwm_dev = NULL;
	char pwm_name[64] = {};
	int i, j;

	for (i = 0; i < PWM_CONTROLLER_NUM_MAX; i++) {
		for (j = 0; j < PWM_CHANNEL_NUM_MAX; j++) {
			sprintf(pwm_name, "%s%d_%d", "pwm", i, j);
			pwm_dev = devm_pwm_get(&pdev->dev, pwm_name);
			if (IS_ERR(pwm_dev)) {
				dev_dbg(&pdev->dev, "failed to bind %s\n", pwm_name);
				continue;
			}

			test_data = devm_kzalloc(&pdev->dev, sizeof(*test_data), GFP_KERNEL);
			if (!test_data)
				return -ENOMEM;

			test_data->pwm_dev = pwm_dev;
			g_pwm_test_data[i][j] = test_data;
			g_pwm_test_data[i][j]->pwm_dev = pwm_dev;
			dev_warn(&pdev->dev, "%s bind %s\n",
				 pwm_name,
				 g_pwm_test_data[i][j]->pwm_dev->chip->dev->of_node->full_name);
		}
	}

	misc_register(&pwm_rockchip_test_misc);

	return 0;
}

static int pwm_rockchip_test_remove(struct platform_device *pdev)
{
	misc_deregister(&pwm_rockchip_test_misc);

	return 0;
}

static const struct of_device_id pwm_rockchip_test_of_match[] = {
	{ .compatible = "pwm-rockchip-test" },
	{ }
};
MODULE_DEVICE_TABLE(of, pwm_rockchip_test_of_match);

static struct platform_driver pwm_rockchip_test_driver = {
	.driver = {
		.name = "pwm-rockchip-test",
		.of_match_table	= of_match_ptr(pwm_rockchip_test_of_match),
	},
	.probe = pwm_rockchip_test_probe,
	.remove = pwm_rockchip_test_remove,
};
module_platform_driver(pwm_rockchip_test_driver);

MODULE_AUTHOR("Lingsong Ding <damon.ding@rock-chips.com>");
MODULE_DESCRIPTION("ROCKCHIP PWM TEST Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("pwm:pwm_test");
