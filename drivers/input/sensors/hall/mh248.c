// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 Rockchip Electronics Co., Ltd.
 *
 * Author: Bin Yang <yangbin@rock-chips.com>
 */

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/ebc.h>
#include <linux/fb.h>
#include <linux/freezer.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/notifier.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/rk_keys.h>
#include <linux/sensor-dev.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

struct mh248_para {
	struct device *dev;
	struct notifier_block fb_notif;
	struct notifier_block ebc_notif;
	struct mutex ops_lock;
	struct input_dev *hall_input;
	int is_suspend;
	int gpio_pin;
	int irq;
	int active_value;
#ifdef CONFIG_ROCKCHIP_LITE_ULTRA_SUSPEND
	void __iomem *pmu_gpio_regs;
	u32 pmu_gpio_regs_base;
	u32 pmu_gpio_regs_size;
	u32 pmu_gpio_int_reg;
	u32 pmu_gpio_rtc_int_mask;
	u32 pmu_gpio_pmic_int_mask;
	int is_hall_wakeup;
#endif
};

#ifdef CONFIG_ROCKCHIP_LITE_ULTRA_SUSPEND
static struct mh248_para *g_mh248 = NULL;
#endif

static int hall_ebc_notifier_callback(struct notifier_block *self,
				     unsigned long action, void *data)
{
	struct mh248_para *mh248;

	mh248 = container_of(self, struct mh248_para, ebc_notif);

	mutex_lock(&mh248->ops_lock);

	if (action == EBC_FB_BLANK)
		mh248->is_suspend = 1;
	else if (action == EBC_FB_UNBLANK)
		mh248->is_suspend = 0;

	mutex_unlock(&mh248->ops_lock);

	return NOTIFY_OK;
}

static int hall_fb_notifier_callback(struct notifier_block *self,
				     unsigned long action, void *data)
{
	struct mh248_para *mh248;
	struct fb_event *event = data;

	mh248 = container_of(self, struct mh248_para, fb_notif);

	if (action != FB_EVENT_BLANK)
		return NOTIFY_DONE;

	mutex_lock(&mh248->ops_lock);
	switch (*((int *)event->data)) {
	case FB_BLANK_UNBLANK:
		mh248->is_suspend = 0;
		break;
	default:
		mh248->is_suspend = 1;
		break;
	}
	mutex_unlock(&mh248->ops_lock);

	return NOTIFY_OK;
}

static irqreturn_t hall_mh248_interrupt(int irq, void *dev_id)
{
	struct mh248_para *mh248 = (struct mh248_para *)dev_id;
	int gpio_value = 0;

	gpio_value = gpio_get_value(mh248->gpio_pin);
	pr_info("%s: gpio value =  %d\n", __func__, gpio_value);

	if ((gpio_value != mh248->active_value) &&
	    (mh248->is_suspend == 0)) {
		input_report_key(mh248->hall_input, KEY_POWER, 1);
		input_sync(mh248->hall_input);
		input_report_key(mh248->hall_input, KEY_POWER, 0);
		input_sync(mh248->hall_input);
	} else if ((gpio_value == mh248->active_value) &&
		   (mh248->is_suspend == 1)) {
		input_report_key(mh248->hall_input, KEY_WAKEUP, 1);
		input_sync(mh248->hall_input);
		input_report_key(mh248->hall_input, KEY_WAKEUP, 0);
		input_sync(mh248->hall_input);
	}

	return IRQ_HANDLED;
}

#ifdef CONFIG_ROCKCHIP_LITE_ULTRA_SUSPEND
static void mh248_syscore_resume(void)
{
	u32 intr_status;

	g_mh248->is_hall_wakeup = 1;
	intr_status = readl_relaxed(g_mh248->pmu_gpio_regs + g_mh248->pmu_gpio_int_reg);
	if (intr_status & g_mh248->pmu_gpio_rtc_int_mask)
		g_mh248->is_hall_wakeup = 0;
	else if (intr_status & g_mh248->pmu_gpio_pmic_int_mask)
		g_mh248->is_hall_wakeup = 0;

	pr_info("%s: GPIO0 INT status =  %x, is_hall_wakeup = %d\n", __func__, intr_status, g_mh248->is_hall_wakeup);
}

static struct syscore_ops mh248_syscore_ops = {
	.resume = mh248_syscore_resume,
};
#endif

static int hall_mh248_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct mh248_para *mh248;
	enum of_gpio_flags irq_flags;
	int hallactive = 0;
	int ret = 0;

	mh248 = devm_kzalloc(&pdev->dev, sizeof(*mh248), GFP_KERNEL);
	if (!mh248)
		return -ENOMEM;

	mh248->dev = &pdev->dev;

#ifdef CONFIG_ROCKCHIP_LITE_ULTRA_SUSPEND
	ret = of_property_read_u32(np, "pmu-gpio-regs-base", &mh248->pmu_gpio_regs_base);
	if (ret) {
		dev_err(mh248->dev, "Can not read property pmu-gpio-regs-base\n");
		return ret;
	}
	dev_info(mh248->dev, "pmu_gpio_regs_base = 0x%x\n", mh248->pmu_gpio_regs_base);
	ret = of_property_read_u32(np, "pmu-gpio-regs-size", &mh248->pmu_gpio_regs_size);
	if (ret) {
		dev_err(mh248->dev, "Can not read property pmu-gpio-reg-size\n");
		return ret;
	}
	dev_info(mh248->dev, "pmu_gpio_regs_size = 0x%x\n", mh248->pmu_gpio_regs_size);
	ret = of_property_read_u32(np, "pmu-gpio-int-reg", &mh248->pmu_gpio_int_reg);
	if (ret) {
		dev_err(mh248->dev, "Can not read property pmu-gpio-int-reg\n");
		return ret;
	}
	dev_info(mh248->dev, "pmu_gpio_int_reg = 0x%x\n", mh248->pmu_gpio_int_reg);
	ret = of_property_read_u32(np, "pmu-gpio-rtc-int-mask", &mh248->pmu_gpio_rtc_int_mask);
	if (ret) {
		dev_err(mh248->dev, "Can not read property pmu-gpio-rtc-int-mask\n");
		return ret;
	}
	dev_info(mh248->dev, "pmu_gpio_rtc_int_mask = 0x%x\n", mh248->pmu_gpio_rtc_int_mask);
	ret = of_property_read_u32(np, "pmu-gpio-pmic-int-mask", &mh248->pmu_gpio_pmic_int_mask);
	if (ret) {
		dev_err(mh248->dev, "Can not read property pmu-gpio-pmic-int-mask\n");
		return ret;
	}
	dev_info(mh248->dev, "pmu_gpio_pmic_int_mask = 0x%x\n", mh248->pmu_gpio_pmic_int_mask);
#endif

	mh248->gpio_pin = of_get_named_gpio_flags(np, "irq-gpio",
						  0, &irq_flags);
	if (!gpio_is_valid(mh248->gpio_pin)) {
		dev_err(mh248->dev, "Can not read property irq-gpio\n");
		return mh248->gpio_pin;
	}
	mh248->irq = gpio_to_irq(mh248->gpio_pin);

	of_property_read_u32(np, "hall-active", &hallactive);
	mh248->active_value = hallactive;
	mh248->is_suspend = 0;
	mutex_init(&mh248->ops_lock);

	ret = devm_gpio_request_one(mh248->dev, mh248->gpio_pin,
				    GPIOF_DIR_IN, "hall_mh248");
	if (ret < 0) {
		dev_err(mh248->dev, "fail to request gpio:%d\n", mh248->gpio_pin);
		return ret;
	}

	ret = devm_request_threaded_irq(mh248->dev, mh248->irq,
					NULL, hall_mh248_interrupt,
					irq_flags | IRQF_NO_SUSPEND | IRQF_ONESHOT,
					"hall_mh248", mh248);
	if (ret < 0) {
		dev_err(mh248->dev, "request irq(%d) failed, ret=%d\n",
			mh248->irq, ret);
		return ret;
	}

	mh248->hall_input = devm_input_allocate_device(&pdev->dev);
	if (!mh248->hall_input) {
		dev_err(&pdev->dev, "Can't allocate hall input dev\n");
		return -ENOMEM;
	}
	mh248->hall_input->name = "hall wake key";
	input_set_capability(mh248->hall_input, EV_KEY, KEY_POWER);
	input_set_capability(mh248->hall_input, EV_KEY, KEY_WAKEUP);

	ret = input_register_device(mh248->hall_input);
	if (ret) {
		dev_err(&pdev->dev, "Unable to register input device, error: %d\n", ret);
		return ret;
	}

	enable_irq_wake(mh248->irq);
	mh248->fb_notif.notifier_call = hall_fb_notifier_callback;
	fb_register_client(&mh248->fb_notif);

	mh248->ebc_notif.notifier_call = hall_ebc_notifier_callback;
	ebc_register_notifier(&mh248->ebc_notif);

#ifdef CONFIG_ROCKCHIP_LITE_ULTRA_SUSPEND
	mh248->pmu_gpio_regs = devm_ioremap(mh248->dev, mh248->pmu_gpio_regs_base, mh248->pmu_gpio_regs_size);
	register_syscore_ops(&mh248_syscore_ops);
	g_mh248 = mh248;
#endif

	dev_info(mh248->dev, "hall_mh248_probe success.\n");

	return 0;
}

#ifdef CONFIG_ROCKCHIP_LITE_ULTRA_SUSPEND
static int hall_mh248_remove(struct platform_device *pdev)
{
	unregister_syscore_ops(&mh248_syscore_ops);

	return 0;
}

static int mh248_suspend(struct device *dev)
{
	if (mem_sleep_current == PM_SUSPEND_MEM_ULTRA) {
		disable_irq_wake(g_mh248->irq);
		disable_irq(g_mh248->irq);
	}

	return 0;
}

static int mh248_resume(struct device *dev)
{
	int gpio_value = 0;

	if (mem_sleep_current == PM_SUSPEND_MEM_ULTRA) {
		if (g_mh248->is_hall_wakeup) {
			g_mh248->is_hall_wakeup = 0;
			gpio_value = gpio_get_value(g_mh248->gpio_pin);
			if ((gpio_value == g_mh248->active_value) &&
				(g_mh248->is_suspend == 1)) {
				input_report_key(g_mh248->hall_input, KEY_WAKEUP, 1);
				input_sync(g_mh248->hall_input);
				input_report_key(g_mh248->hall_input, KEY_WAKEUP, 0);
				input_sync(g_mh248->hall_input);
				pr_info("%s: send wakeup key\n", __func__);
			}
		}
		enable_irq(g_mh248->irq);
		enable_irq_wake(g_mh248->irq);
	}
	return 0;
}

static const struct dev_pm_ops mh248_pm = {
	.resume = mh248_resume,
	.suspend = mh248_suspend,
};
#endif

static const struct of_device_id hall_mh248_match[] = {
	{ .compatible = "hall-mh248" },
	{ /* Sentinel */ }
};

static struct platform_driver hall_mh248_driver = {
	.probe = hall_mh248_probe,
#ifdef CONFIG_ROCKCHIP_LITE_ULTRA_SUSPEND
	.remove = hall_mh248_remove,
#endif
	.driver = {
		.name = "mh248",
		.owner = THIS_MODULE,
		.of_match_table	= hall_mh248_match,
#ifdef CONFIG_ROCKCHIP_LITE_ULTRA_SUSPEND
		.pm = &mh248_pm,
#endif
	},
};

module_platform_driver(hall_mh248_driver);

MODULE_ALIAS("platform:mh248");
MODULE_AUTHOR("Bin Yang <yangbin@rock-chips.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Hall Sensor MH248 driver");
