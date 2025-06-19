/*
 * Copyright (C) 2012 Rockchip Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/* Rock-chips rfkill driver for bluetooth
 *
*/

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/rfkill.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/rfkill-bt.h>
#include <linux/rfkill-wlan.h>
#include <linux/wakelock.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <asm/irq.h>
#include <linux/suspend.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <dt-bindings/gpio/gpio.h>
#include <uapi/linux/rfkill.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif
#include <linux/pinctrl/consumer.h>
#include <linux/gpio/consumer.h>
#include "../../drivers/gpio/gpiolib.h"
#include "rfkill.h"

#if 0
#define DBG(x...) pr_info("[BT_RFKILL]: " x)
#else
#define DBG(x...)
#endif

#define LOG(x...) pr_info("[BT_RFKILL]: " x)

#define BT_WAKEUP_TIMEOUT 10000
#define BT_IRQ_WAKELOCK_TIMEOUT (10 * 1000)

#define BT_BLOCKED true
#define BT_UNBLOCK false
#define BT_SLEEP true
#define BT_WAKEUP false

enum {
	IOMUX_FNORMAL = 0,
	IOMUX_FGPIO,
	IOMUX_FMUX,
};

struct rfkill_rk_data {
	struct rfkill_rk_platform_data *pdata;
	struct platform_device *pdev;
	struct rfkill *rfkill_dev;
	struct wake_lock bt_irq_wl;
	struct delayed_work bt_sleep_delay_work;
	int irq_req;
	bool enable_power_key;
};

static struct rfkill_rk_data *g_rfkill = NULL;
static struct input_dev *power_key_dev;

static const char bt_name[] =
#if defined(CONFIG_BCM4330)
#if defined(CONFIG_BT_MODULE_NH660)
	"nh660"
#else
	"bcm4330"
#endif
#elif defined(CONFIG_RK903)
#if defined(CONFIG_RKWIFI_26M)
	"rk903_26M"
#else
	"rk903"
#endif
#elif defined(CONFIG_BCM4329)
	"bcm4329"
#elif defined(CONFIG_MV8787)
	"mv8787"
#elif defined(CONFIG_AP6210)
#if defined(CONFIG_RKWIFI_26M)
	"ap6210"
#else
	"ap6210_24M"
#endif
#elif defined(CONFIG_AP6330)
	"ap6330"
#elif defined(CONFIG_AP6476)
	"ap6476"
#elif defined(CONFIG_AP6493)
	"ap6493"
#elif defined(CONFIG_AP6441)
	"ap6441"
#elif defined(CONFIG_AP6335)
	"ap6335"
#elif defined(CONFIG_GB86302I)
	"gb86302i"
#else
	"bt_default"
#endif
	;

static int rfkill_rk_power_key_up(void)
{
	if (!power_key_dev)
		return -ENODEV;

	input_report_key(power_key_dev, KEY_POWER, 1);
	input_sync(power_key_dev);
	msleep(20);
	input_report_key(power_key_dev, KEY_POWER, 0);
	input_sync(power_key_dev);

	return 0;
}

static irqreturn_t rfkill_rk_wake_host_irq(int irq, void *dev)
{
	struct rfkill_rk_data *rfkill = dev;

	LOG("BT_WAKE_HOST IRQ fired\n");

	DBG("BT IRQ wakeup, request %dms wakelock\n", BT_IRQ_WAKELOCK_TIMEOUT);

	wake_lock_timeout(&rfkill->bt_irq_wl,
			  msecs_to_jiffies(BT_IRQ_WAKELOCK_TIMEOUT));

	if (rfkill->enable_power_key)
		return IRQ_WAKE_THREAD;

	return IRQ_HANDLED;
}

static irqreturn_t rfkill_rk_wake_host_irq_thread(int irq, void *dev)
{
	rfkill_rk_power_key_up();

	return IRQ_HANDLED;
}

static int rfkill_rk_setup_wake_irq(struct rfkill_rk_data *rfkill)
{
	int ret = 0;
	struct rfkill_rk_irq *irq = &rfkill->pdata->wake_host_irq;

	if (rfkill->irq_req) {
		rfkill->irq_req = 0;
		free_irq(irq->irq, rfkill);
	}
	if (irq->gpio.io) {
		LOG("Request irq for bt wakeup host\n");
		irq->irq = gpiod_to_irq(irq->gpio.io);
		sprintf(irq->name, "%s_irq", irq->gpio.name);
		ret = request_threaded_irq(irq->irq, rfkill_rk_wake_host_irq,
					   rfkill_rk_wake_host_irq_thread,
					   IRQF_ONESHOT | (gpiod_is_active_low(irq->gpio.io) ?
					   IRQF_TRIGGER_FALLING :
					   IRQF_TRIGGER_RISING),
					   irq->name, rfkill);
		if (ret)
			return ret;
		rfkill->irq_req = 1;
		LOG("** disable irq\n");
		disable_irq(irq->irq);
		/*ret = disable_irq_wake(irq->irq);init irq wake is disabled,no need to disable*/
	}

	return ret;
}

static inline void rfkill_rk_sleep_bt_internal(struct rfkill_rk_data *rfkill,
					       bool sleep)
{
	struct rfkill_rk_gpio *wake = &rfkill->pdata->wake_gpio;

	DBG("*** bt sleep: %d ***\n", sleep);
#ifndef CONFIG_BK3515A_COMBO
	if (wake->io)
		gpiod_direction_output(wake->io, sleep ? 0 : 1);
#else
	if (!sleep) {
		DBG("HOST_UART0_TX pull down 10us\n");
		if (wake->io) {
			gpiod_direction_output(wake->io, 1);
			usleep_range(10, 20);
			gpiod_direction_output(wake->io, 0);
		}
	}
#endif
}

static void rfkill_rk_delay_sleep_bt(struct work_struct *work)
{
	struct rfkill_rk_data *rfkill = NULL;

	DBG("Enter %s\n", __func__);

	rfkill = container_of(work, struct rfkill_rk_data,
			      bt_sleep_delay_work.work);

	rfkill_rk_sleep_bt_internal(rfkill, BT_SLEEP);
}

void rfkill_rk_sleep_bt(bool sleep)
{
	struct rfkill_rk_data *rfkill = g_rfkill;
	struct rfkill_rk_gpio *wake;

	DBG("Enter %s\n", __func__);

	if (!rfkill) {
		LOG("*** RFKILL is empty???\n");
		return;
	}

	wake = &rfkill->pdata->wake_gpio;
	if (!wake->io) {
		DBG("*** Not support bt wakeup and sleep\n");
		return;
	}

	cancel_delayed_work_sync(&rfkill->bt_sleep_delay_work);

	rfkill_rk_sleep_bt_internal(rfkill, sleep);

#ifdef CONFIG_BT_AUTOSLEEP
	if (sleep == BT_WAKEUP) {
		schedule_delayed_work(&rfkill->bt_sleep_delay_work,
				      msecs_to_jiffies(BT_WAKEUP_TIMEOUT));
	}
#endif
}
EXPORT_SYMBOL(rfkill_rk_sleep_bt);

static int bt_power_state = 0;
int rfkill_get_bt_power_state(int *power, bool *toggle)
{
	struct rfkill_rk_data *mrfkill = g_rfkill;

	if (!mrfkill) {
		LOG("%s: rfkill-bt driver has not Successful initialized\n",
		    __func__);
		return -1;
	}

	*toggle = mrfkill->pdata->power_toggle;
	*power = bt_power_state;

	return 0;
}

static int rfkill_rk_set_power(void *data, bool blocked)
{
	struct rfkill_rk_data *rfkill = data;
	struct rfkill_rk_gpio *wake_host = &rfkill->pdata->wake_host_irq.gpio;
	struct rfkill_rk_gpio *poweron = &rfkill->pdata->poweron_gpio;
	struct rfkill_rk_gpio *reset = &rfkill->pdata->reset_gpio;
	struct rfkill_rk_gpio *rts = &rfkill->pdata->rts_gpio;
	struct pinctrl *pinctrl = rfkill->pdata->pinctrl;
	int wifi_power = 0;
	bool toggle = false;

	DBG("Enter %s\n", __func__);

	DBG("Set blocked:%d\n", blocked);

	toggle = rfkill->pdata->power_toggle;


	DBG("%s: toggle = %s\n", __func__, toggle ? "true" : "false");

	if (!blocked) {
		if (toggle) {
			rfkill_set_wifi_bt_power(1);
			msleep(100);
		}

		rfkill_rk_sleep_bt(BT_WAKEUP); // ensure bt is wakeup

		if (wake_host->io) {
			LOG("%s: set bt wake_host high!\n", __func__);
			gpiod_direction_output(wake_host->io, 1);
			msleep(20);
		}
		if (poweron->io) {
			if (gpiod_get_value(poweron->io) == 0) {
				gpiod_direction_output(poweron->io, 0);
				msleep(20);
				gpiod_direction_output(poweron->io, 1);
				msleep(20);
			}
		}

		if (reset->io) {
			if (gpiod_get_value(reset->io) == 0) {
				gpiod_direction_output(reset->io, 0);
				msleep(20);
				gpiod_direction_output(reset->io, 1);
			}
		}

		if (wake_host->io) {
			LOG("%s: set bt wake_host input!\n", __func__);
			gpiod_direction_input(wake_host->io);
		}

		if (pinctrl && rts->io) {
			pinctrl_select_state(pinctrl, rts->gpio_state);
			LOG("ENABLE UART_RTS\n");
			gpiod_direction_output(rts->io, 1);
			msleep(100);
			LOG("DISABLE UART_RTS\n");
			gpiod_direction_output(rts->io, 0);
			pinctrl_select_state(pinctrl, rts->default_state);
		}

		bt_power_state = 1;
		LOG("bt turn on power\n");
		rfkill_rk_setup_wake_irq(rfkill);
	} else {
		if (poweron->io) {
			if (gpiod_get_value(poweron->io) == 1) {
				gpiod_direction_output(poweron->io, 0);
				msleep(20);
			}
		}

		bt_power_state = 0;
		LOG("bt shut off power\n");
		if (reset->io) {
			if (gpiod_get_value(reset->io) == 1) {
				gpiod_direction_output(reset->io, 0);
				msleep(20);
			}
		}
		if (toggle) {
			if (rfkill_get_wifi_power_state(&wifi_power)) {
				LOG("%s: cannot get wifi power state!\n", __func__);
				return -EPERM;
			}
			if (!wifi_power) {
				LOG("%s: bt will set vbat to low\n", __func__);
				rfkill_set_wifi_bt_power(0);
			} else {
				LOG("%s: bt shouldn't control the vbat\n", __func__);
			}
		}
	}

	return 0;
}

static int rfkill_rk_pm_prepare(struct device *dev)
{
	struct rfkill_rk_data *rfkill = g_rfkill;
	struct rfkill_rk_gpio *rts;
	struct rfkill_rk_irq *wake_host_irq;

	DBG("Enter %s\n", __func__);

	if (!rfkill)
		return 0;

	rts = &rfkill->pdata->rts_gpio;
	wake_host_irq = &rfkill->pdata->wake_host_irq;

	//To prevent uart to receive bt data when suspended
	if (rfkill->pdata->pinctrl && rts->io) {
		DBG("Disable UART_RTS\n");
		pinctrl_select_state(rfkill->pdata->pinctrl, rts->gpio_state);
		gpiod_direction_output(rts->io, 0);
	}

#ifdef CONFIG_BT_AUTOSLEEP
	rfkill_rk_sleep_bt(BT_SLEEP);
#endif

	// enable bt wakeup host
	if (wake_host_irq->gpio.io && bt_power_state) {
		DBG("enable irq for bt wakeup host\n");
		enable_irq(wake_host_irq->irq);
		enable_irq_wake(wake_host_irq->irq);
	}

#ifdef CONFIG_RFKILL_RESET
	rfkill_init_sw_state(rfkill->rfkill_dev, BT_BLOCKED);
	rfkill_set_sw_state(rfkill->rfkill_dev, BT_BLOCKED);
	rfkill_set_hw_state(rfkill->rfkill_dev, false);
	rfkill_rk_set_power(rfkill, BT_BLOCKED);
#endif

	return 0;
}

static void rfkill_rk_pm_complete(struct device *dev)
{
	struct rfkill_rk_data *rfkill = g_rfkill;
	struct rfkill_rk_irq *wake_host_irq;
	struct rfkill_rk_gpio *rts;

	DBG("Enter %s\n", __func__);

	if (!rfkill)
		return;

	wake_host_irq = &rfkill->pdata->wake_host_irq;
	rts = &rfkill->pdata->rts_gpio;

	if (wake_host_irq->gpio.io && bt_power_state) {
		LOG("** disable irq\n");
		disable_irq(wake_host_irq->irq);
		disable_irq_wake(wake_host_irq->irq);
	}

	if (rfkill->pdata->pinctrl && rts->io) {
		DBG("Enable UART_RTS\n");
		gpiod_direction_output(rts->io, 1);
		pinctrl_select_state(rfkill->pdata->pinctrl, rts->default_state);
	}
}

static const struct rfkill_ops rfkill_rk_ops = {
	.set_block = rfkill_rk_set_power,
};

#define PROC_DIR "bluetooth/sleep"

static struct proc_dir_entry *bluetooth_dir, *sleep_dir;

static ssize_t bluesleep_read_proc_lpm(struct file *file, char __user *buffer,
				       size_t count, loff_t *data)
{
	return sprintf(buffer, "unsupported to read\n");
}

static ssize_t bluesleep_write_proc_lpm(struct file *file,
					const char __user *buffer, size_t count,
					loff_t *data)
{
	return count;
}

static ssize_t bluesleep_read_proc_btwrite(struct file *file,
					   char __user *buffer, size_t count,
					   loff_t *data)
{
	return sprintf(buffer, "unsupported to read\n");
}

static ssize_t bluesleep_write_proc_btwrite(struct file *file,
					    const char __user *buffer,
					    size_t count, loff_t *data)
{
	char b;

	if (count < 1)
		return -EINVAL;

	if (copy_from_user(&b, buffer, 1))
		return -EFAULT;

	DBG("btwrite %c\n", b);
	if (b != '0')
		rfkill_rk_sleep_bt(BT_WAKEUP);
	else
		rfkill_rk_sleep_bt(BT_SLEEP);

	return count;
}

static ssize_t bluesleep_read_proc_powerupkey(struct file *file,
					      char __user *buffer, size_t count,
					      loff_t *data)
{
	struct rfkill_rk_data *rfkill = g_rfkill;
	char src[2];

	if (*data >= 1)
		return 0;

	if (!rfkill)
		return -EFAULT;

	src[0] = rfkill->enable_power_key ? '1' : '0';
	src[1] = '\n';
	if (copy_to_user(buffer, src, 2))
		return -EFAULT;
	*data = 1;

	return 2;
}

static ssize_t bluesleep_write_proc_powerupkey(struct file *file,
					       const char __user *buffer,
					       size_t count, loff_t *data)
{
	char b;
	struct rfkill_rk_data *rfkill = g_rfkill;

	if (!rfkill)
		return -EFAULT;

	if (count < 1)
		return -EINVAL;

	if (copy_from_user(&b, buffer, 1))
		return -EFAULT;

	if (b != '0')
		rfkill->enable_power_key = true;
	else
		rfkill->enable_power_key = false;

	return count;
}

#ifdef CONFIG_OF
static int rkgpiod_request(struct device *dev, const char *con_id,
			   enum gpiod_flags flags, struct rfkill_rk_gpio   *rk_gpio)
{
	struct  gpio_desc	*gpiodesc;
	int error;

	gpiodesc = devm_gpiod_get(dev, con_id, flags);
	error = PTR_ERR_OR_ZERO(gpiodesc);
	if (error) {
		LOG("%s failed to request %s gpio, err %d\n", __func__, con_id, error);
		rk_gpio->io = NULL;
	} else {
		rk_gpio->io = gpiodesc;
		LOG("%s get property: %s flags %lx\n", __func__, con_id, gpiodesc->flags);
	}

	return error;
}

static int bluetooth_platdata_parse_dt(struct device *dev,
				       struct rfkill_rk_platform_data *data)
{
	struct device_node *node = dev->of_node;
	int error;

	if (!node)
		return -ENODEV;

	memset(data, 0, sizeof(*data));

	if (of_find_property(node, "wifi-bt-power-toggle", NULL)) {
		data->power_toggle = true;
		LOG("%s: get property wifi-bt-power-toggle.\n", __func__);
	} else {
		data->power_toggle = false;
	}

	error = rkgpiod_request(dev, "uart_rts", GPIOD_ASIS, &data->rts_gpio);
	if (!error) {
		data->pinctrl = devm_pinctrl_get(dev);
		if (!IS_ERR(data->pinctrl)) {
			data->rts_gpio.default_state =
				pinctrl_lookup_state(data->pinctrl, "default");
			data->rts_gpio.gpio_state =
				pinctrl_lookup_state(data->pinctrl, "rts_gpio");
		} else {
			data->pinctrl = NULL;
			LOG("%s: dts doesn't define the uart rts iomux.\n", __func__);
		}
	}

	rkgpiod_request(dev, "BT,power", GPIOD_OUT_HIGH, &data->poweron_gpio);
	rkgpiod_request(dev, "BT,reset", GPIOD_OUT_HIGH, &data->reset_gpio);
	rkgpiod_request(dev, "BT,wake", GPIOD_OUT_HIGH, &data->wake_gpio);
	error = rkgpiod_request(dev, "BT,wake_host", GPIOD_ASIS, &data->wake_host_irq.gpio);
	if (!error)
		sprintf(data->wake_host_irq.gpio.name, "%s", "BT,wake_host");

	data->ext_clk = devm_clk_get(dev, "ext_clock");
	if (IS_ERR(data->ext_clk)) {
		LOG("%s: clk_get failed!!!.\n", __func__);
	} else {
		clk_prepare_enable(data->ext_clk);
	}
	return 0;
}
#endif //CONFIG_OF

static const struct proc_ops bluesleep_lpm = {
	.proc_read = bluesleep_read_proc_lpm,
	.proc_write = bluesleep_write_proc_lpm,
};

static const struct proc_ops bluesleep_btwrite = {
	.proc_read = bluesleep_read_proc_btwrite,
	.proc_write = bluesleep_write_proc_btwrite,
};

static const struct proc_ops bluesleep_powerupkey = {
	.proc_read = bluesleep_read_proc_powerupkey,
	.proc_write = bluesleep_write_proc_powerupkey,
};

static int rfkill_rk_register_power_key(struct device *dev)
{
	int ret = 0;

	/* register input device */
	power_key_dev = devm_input_allocate_device(dev);
	if (!power_key_dev) {
		LOG("%s: not enough memory for input device\n", __func__);
		return -ENOMEM;
	}

	power_key_dev->name = "bt-powerkey";
	power_key_dev->id.bustype = BUS_HOST;

	power_key_dev->evbit[0] = BIT_MASK(EV_KEY);
	set_bit(KEY_POWER, power_key_dev->keybit);

	ret = input_register_device(power_key_dev);
	if (ret) {
		LOG("%s: register input device exception, exit\n", __func__);
		return -EBUSY;
	}

	return ret;
}

static int rfkill_rk_probe(struct platform_device *pdev)
{
	struct rfkill_rk_data *rfkill;
	struct rfkill_rk_platform_data *pdata = pdev->dev.platform_data;
	int ret = 0;
	struct proc_dir_entry *ent;

	DBG("Enter %s\n", __func__);

	if (!pdata) {
#ifdef CONFIG_OF
		pdata = devm_kzalloc(&pdev->dev,
				     sizeof(struct rfkill_rk_platform_data),
				     GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;

		ret = bluetooth_platdata_parse_dt(&pdev->dev, pdata);
		if (ret < 0) {
#endif
			LOG("%s: No platform data specified\n", __func__);
			return ret;
#ifdef CONFIG_OF
		}
#endif
	}

	pdata->name = (char *)bt_name;
	pdata->type = RFKILL_TYPE_BLUETOOTH;

	rfkill = devm_kzalloc(&pdev->dev, sizeof(*rfkill), GFP_KERNEL);
	if (!rfkill)
		return -ENOMEM;

	rfkill->pdata = pdata;
	rfkill->pdev = pdev;
	g_rfkill = rfkill;

	bluetooth_dir = proc_mkdir("bluetooth", NULL);
	if (!bluetooth_dir) {
		LOG("Unable to create /proc/bluetooth directory");
		return -ENOMEM;
	}

	sleep_dir = proc_mkdir("sleep", bluetooth_dir);
	if (!sleep_dir) {
		LOG("Unable to create /proc/%s directory", PROC_DIR);
		return -ENOMEM;
	}

	/* read/write proc entries */
	ent = proc_create("lpm", 0444, sleep_dir, &bluesleep_lpm);
	if (!ent) {
		LOG("Unable to create /proc/%s/lpm entry", PROC_DIR);
		ret = -ENOMEM;
		goto fail_alloc;
	}

	/* read/write proc entries */
	ent = proc_create("btwrite", 0444, sleep_dir, &bluesleep_btwrite);
	if (!ent) {
		LOG("Unable to create /proc/%s/btwrite entry", PROC_DIR);
		ret = -ENOMEM;
		goto fail_alloc;
	}

	/* read/write proc entries */
	ent = proc_create("powerupkey", 0444, sleep_dir, &bluesleep_powerupkey);
	if (!ent) {
		LOG("Unable to create /proc/%s/powerupkey entry", PROC_DIR);
		ret = -ENOMEM;
		goto fail_alloc;
	}

	wake_lock_init(&rfkill->bt_irq_wl, WAKE_LOCK_SUSPEND,
		       "rfkill_rk_irq_wl");

	DBG("setup rfkill\n");
	rfkill->rfkill_dev = rfkill_alloc(pdata->name, &pdev->dev, pdata->type,
					  &rfkill_rk_ops, rfkill);
	if (!rfkill->rfkill_dev) {
		ret = -ENOMEM;
		goto fail_alloc;
	}

	rfkill_init_sw_state(rfkill->rfkill_dev, BT_BLOCKED);
	rfkill_set_sw_state(rfkill->rfkill_dev, BT_BLOCKED);
	rfkill_set_hw_state(rfkill->rfkill_dev, false);
	ret = rfkill_register(rfkill->rfkill_dev);
	if (ret < 0)
		goto fail_rfkill;

	INIT_DELAYED_WORK(&rfkill->bt_sleep_delay_work,
			  rfkill_rk_delay_sleep_bt);

	//rfkill_rk_set_power(rfkill, BT_BLOCKED);
	// bt turn off power
	if (pdata->poweron_gpio.io) {
		gpiod_direction_output(pdata->poweron_gpio.io, 0);
	}
	if (pdata->reset_gpio.io) {
		gpiod_direction_output(pdata->reset_gpio.io, 0);
	}

	platform_set_drvdata(pdev, rfkill);

	LOG("%s device registered.\n", pdata->name);

	if (rfkill_rk_register_power_key(&pdev->dev) != 0)
		goto fail_rfkill;

	return 0;

fail_rfkill:
	rfkill_destroy(rfkill->rfkill_dev);
fail_alloc:
	remove_proc_subtree("bluetooth/sleep", NULL);
	wake_lock_destroy(&rfkill->bt_irq_wl);
	g_rfkill = NULL;
	return ret;
}

static void rfkill_rk_remove(struct platform_device *pdev)
{
	struct rfkill_rk_data *rfkill = platform_get_drvdata(pdev);

	LOG("Enter %s\n", __func__);

	rfkill_unregister(rfkill->rfkill_dev);
	rfkill_destroy(rfkill->rfkill_dev);
	remove_proc_subtree("bluetooth/sleep", NULL);

	cancel_delayed_work_sync(&rfkill->bt_sleep_delay_work);

	clk_disable_unprepare(rfkill->pdata->ext_clk);
	wake_lock_destroy(&rfkill->bt_irq_wl);
	g_rfkill = NULL;
}

static const struct dev_pm_ops rfkill_rk_pm_ops = {
	.prepare = rfkill_rk_pm_prepare,
	.complete = rfkill_rk_pm_complete,
};

#ifdef CONFIG_OF
static struct of_device_id bt_platdata_of_match[] = {
	{ .compatible = "bluetooth-platdata" },
	{}
};
MODULE_DEVICE_TABLE(of, bt_platdata_of_match);
#endif //CONFIG_OF

static struct platform_driver rfkill_rk_driver = {
	.probe = rfkill_rk_probe,
	.remove = rfkill_rk_remove,
	.driver = {
		.name = "rfkill_bt",
		.owner = THIS_MODULE,
		.pm = &rfkill_rk_pm_ops,
        .of_match_table = of_match_ptr(bt_platdata_of_match),
	},
};

static int __init rfkill_rk_init(void)
{
	int err;

	LOG("Enter %s\n", __func__);
	err = rfkill_wlan_init();
	if (err)
		return err;
	return platform_driver_register(&rfkill_rk_driver);
}

static void __exit rfkill_rk_exit(void)
{
	LOG("Enter %s\n", __func__);
	platform_driver_unregister(&rfkill_rk_driver);
	rfkill_wlan_exit();
}

module_init(rfkill_rk_init);
module_exit(rfkill_rk_exit);

MODULE_DESCRIPTION("rock-chips rfkill for Bluetooth v0.3");
MODULE_AUTHOR("cmy@rock-chips.com, gwl@rock-chips.com");
MODULE_LICENSE("GPL");
