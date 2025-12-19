// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 * Author:
 *      Algea Cao <algea.cao@rock-chips.com>
 */
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <drm/drm_edid.h>
#include <drm/bridge/dw_hdmi.h>

#include <media/cec.h>
#include <media/cec-notifier.h>

#include "dw-hdmi-qp-cec.h"

enum {
	CEC_TX_CONTROL		= 0x1000,
	CEC_CTRL_CLEAR		= BIT(0),
	CEC_CTRL_START		= BIT(0),

	CEC_STAT_DONE		= BIT(0),
	CEC_STAT_NACK		= BIT(1),
	CEC_STAT_ARBLOST	= BIT(2),
	CEC_STAT_LINE_ERR	= BIT(3),
	CEC_STAT_RETRANS_FAIL	= BIT(4),
	CEC_STAT_DISCARD	= BIT(5),
	CEC_STAT_TX_BUSY	= BIT(8),
	CEC_STAT_RX_BUSY	= BIT(9),
	CEC_STAT_DRIVE_ERR	= BIT(10),
	CEC_STAT_EOM		= BIT(11),
	CEC_STAT_NOTIFY_ERR	= BIT(12),

	CEC_CONFIG		= 0x1008,
	CEC_ADDR		= 0x100c,
	CEC_TX_CNT		= 0x1020,
	CEC_RX_CNT		= 0x1040,
	CEC_TX_DATA3_0		= 0x1024,
	CEC_RX_DATA3_0		= 0x1044,
	CEC_LOCK_CONTROL	= 0x1054,

	CEC_INT_STATUS		= 0x4000,
	CEC_INT_MASK_N		= 0x4004,
	CEC_INT_CLEAR		= 0x4008,
};

#define CEC_VERSION		0x0000
#define CEC_CTRL		0x0010
#define WAKEUP_EN		BIT(0)
#define CEC_LADR_PAT		0x0018
#define DST_PAT			0xf
#define CEC_IS			0x0020
#define WAKEUP_IS		BIT(2)
#define RCV_ERR_IS		BIT(1)
#define RCV_DONE_IS		BIT(0)
#define CEC_IE			0x0024
#define CEC_RCV			0x0030
#define OPCODE_VLD		BIT(31)
#define HDR_VLD			BIT(15)
#define CEC_PAT0		0x0040
#define CEC_PAT1		0x0044
#define CEC_PAT2		0x0048
#define CEC_PAT3		0x004C
#define CEC_PAT4		0x0050
#define CEC_PAT5		0x0054
#define CEC_PAT6		0x0058
#define CEC_PAT7		0x005C
#define CEC_STARTL_MIN		0x0080
#define CEC_STARTL_MAX		0x0084
#define CEC_BIT0L_MIN		0x0088
#define CEC_BIT0L_MAX		0x008C
#define CEC_BIT1L_MIN		0x0090
#define CEC_BIT1L_MAX		0x0094
#define CEC_BIT0L_NOR		0x0098
#define CEC_BIT0L_TO		0x009C

#define CEC_EN			BIT(0)
#define CEC_WAKE		BIT(1)

struct dw_hdmi_qp_cec {
	struct device *dev;
	struct dw_hdmi_qp *hdmi;
	const struct dw_hdmi_qp_cec_ops *ops;
	void __iomem *cec_wakeup_mem;
	u32 addresses;
	struct cec_adapter *adap;
	struct cec_msg rx_msg;
	unsigned int tx_status;
	bool tx_done;
	bool rx_done;
	bool wake_en;
	bool standby_en;
	bool cec_enable;
	struct cec_notifier *notify;
	int irq;
	int wake_irq;
	struct mutex wake_lock;
	struct input_dev *devinput;
	struct miscdevice misc_dev;
};

static void dw_hdmi_qp_write(struct dw_hdmi_qp_cec *cec, u32 val, int offset)
{
	cec->ops->write(cec->hdmi, val, offset);
}

static u32 dw_hdmi_qp_read(struct dw_hdmi_qp_cec *cec, int offset)
{
	return cec->ops->read(cec->hdmi, offset);
}

static void dw_hdmi_qp_wakeup_write(struct dw_hdmi_qp_cec *cec, u32 val, u32 offset)
{
	writel(val, cec->cec_wakeup_mem + offset);
}

static u32 dw_hdmi_qp_wakeup_read(struct dw_hdmi_qp_cec *cec, u32 offset)
{
	return readl(cec->cec_wakeup_mem + offset);
}

static void dw_hdmi_qp_wakeup_mod(struct dw_hdmi_qp_cec *cec, u32 val, u32 mask, u32 offset)
{
	u32 value;

	value = readl(cec->cec_wakeup_mem + offset);
	value &= ~mask;
	value |= (mask << 16) | val;

	writel(value, cec->cec_wakeup_mem + offset);
}

static int dw_hdmi_qp_cec_log_addr(struct cec_adapter *adap, u8 logical_addr)
{
	struct dw_hdmi_qp_cec *cec = cec_get_drvdata(adap);

	if (!cec->cec_enable)
		return 0;

	if (logical_addr == CEC_LOG_ADDR_INVALID)
		cec->addresses = 0;
	else
		cec->addresses |= BIT(logical_addr) | BIT(15);

	dw_hdmi_qp_write(cec, cec->addresses, CEC_ADDR);

	return 0;
}

static int dw_hdmi_qp_cec_transmit(struct cec_adapter *adap, u8 attempts,
				   u32 signal_free_time, struct cec_msg *msg)
{
	struct dw_hdmi_qp_cec *cec = cec_get_drvdata(adap);
	unsigned int i;
	u32 val;

	if (!cec->cec_enable)
		return 0;

	for (i = 0; i < msg->len; i++) {
		if (!(i % 4))
			val = msg->msg[i];
		if ((i % 4) == 1)
			val |= msg->msg[i] << 8;
		if ((i % 4) == 2)
			val |= msg->msg[i] << 16;
		if ((i % 4) == 3)
			val |= msg->msg[i] << 24;

		if (i == (msg->len - 1) || (i % 4) == 3)
			dw_hdmi_qp_write(cec, val, CEC_TX_DATA3_0 + (i / 4) * 4);
	}

	dw_hdmi_qp_write(cec, msg->len - 1, CEC_TX_CNT);
	dw_hdmi_qp_write(cec, CEC_CTRL_START, CEC_TX_CONTROL);

	return 0;
}

static irqreturn_t dw_hdmi_qp_cec_hardirq(int irq, void *data)
{
	struct cec_adapter *adap = data;
	struct dw_hdmi_qp_cec *cec = cec_get_drvdata(adap);
	u32 stat = dw_hdmi_qp_read(cec, CEC_INT_STATUS);
	irqreturn_t ret = IRQ_HANDLED;

	if (stat == 0)
		return IRQ_NONE;

	dw_hdmi_qp_write(cec, stat, CEC_INT_CLEAR);

	if (stat & CEC_STAT_LINE_ERR) {
		cec->tx_status = CEC_TX_STATUS_ERROR;
		cec->tx_done = true;
		ret = IRQ_WAKE_THREAD;
	} else if (stat & CEC_STAT_DONE) {
		cec->tx_status = CEC_TX_STATUS_OK;
		cec->tx_done = true;
		ret = IRQ_WAKE_THREAD;
	} else if (stat & CEC_STAT_NACK) {
		cec->tx_status = CEC_TX_STATUS_NACK;
		cec->tx_done = true;
		ret = IRQ_WAKE_THREAD;
	}

	if (stat & CEC_STAT_EOM) {
		unsigned int len, i, val;

		val = dw_hdmi_qp_read(cec, CEC_RX_CNT);
		len = (val & 0xf) + 1;

		if (len > sizeof(cec->rx_msg.msg))
			len = sizeof(cec->rx_msg.msg);

		for (i = 0; i < 4; i++) {
			val = dw_hdmi_qp_read(cec, CEC_RX_DATA3_0 + i * 4);
			cec->rx_msg.msg[i * 4] = val & 0xff;
			cec->rx_msg.msg[i * 4 + 1] = (val >> 8) & 0xff;
			cec->rx_msg.msg[i * 4 + 2] = (val >> 16) & 0xff;
			cec->rx_msg.msg[i * 4 + 3] = (val >> 24) & 0xff;
		}

		dw_hdmi_qp_write(cec, 1, CEC_LOCK_CONTROL);

		cec->rx_msg.len = len;
		cec->rx_done = true;

		ret = IRQ_WAKE_THREAD;
	}

	return ret;
}

static irqreturn_t dw_hdmi_qp_cec_thread(int irq, void *data)
{
	struct cec_adapter *adap = data;
	struct dw_hdmi_qp_cec *cec = cec_get_drvdata(adap);

	if (cec->tx_done) {
		cec->tx_done = false;
		cec_transmit_attempt_done(adap, cec->tx_status);
	}
	if (cec->rx_done) {
		cec->rx_done = false;
		cec_received_msg(adap, &cec->rx_msg);
	}
	return IRQ_HANDLED;
}

static int dw_hdmi_qp_cec_enable(struct cec_adapter *adap, bool enable)
{
	struct dw_hdmi_qp_cec *cec = cec_get_drvdata(adap);

	if (!enable) {
		if (!cec->cec_enable)
			return 0;
		dw_hdmi_qp_write(cec, 0, CEC_INT_MASK_N);
		dw_hdmi_qp_write(cec, ~0, CEC_INT_CLEAR);
		if (cec->wake_irq > 0 && cec->wake_en && cec->standby_en) {
			cec->ops->set_wakeup(cec->hdmi, true);
			dw_hdmi_qp_wakeup_mod(cec, __ffs(cec->addresses), DST_PAT, CEC_LADR_PAT);
			dw_hdmi_qp_wakeup_write(cec, 0xffffffff, CEC_IS);
			dw_hdmi_qp_wakeup_write(cec, (WAKEUP_IS << 16) | WAKEUP_IS, CEC_IE);
			dw_hdmi_qp_wakeup_mod(cec, OPCODE_VLD | HDR_VLD, OPCODE_VLD | HDR_VLD,
					      CEC_RCV);
			/* wake up opcode, refer to dw-hdmi 2.0 controller */
			dw_hdmi_qp_wakeup_write(cec, 0x80828086, CEC_PAT0);
			dw_hdmi_qp_wakeup_write(cec, 0x80448070, CEC_PAT1);
			dw_hdmi_qp_wakeup_write(cec, 0x80428041, CEC_PAT2);
			dw_hdmi_qp_wakeup_write(cec, 0x8004800d, CEC_PAT3);
			dw_hdmi_qp_wakeup_mod(cec, WAKEUP_EN, WAKEUP_EN, CEC_CTRL);
		} else {
			cec->addresses = 0;
			dw_hdmi_qp_write(cec, cec->addresses, CEC_ADDR);
			cec->ops->disable(cec->hdmi);
			cec->cec_enable = false;
		}
	} else {
		unsigned int irqs;

		if (cec->cec_enable)
			return 0;
		if (cec->wake_irq > 0) {
			cec->ops->set_wakeup(cec->hdmi, false);
			dw_hdmi_qp_wakeup_write(cec, 0xffff0000, CEC_IE);
			dw_hdmi_qp_wakeup_write(cec, 0xffffffff, CEC_IS);
			dw_hdmi_qp_wakeup_mod(cec, 0, WAKEUP_EN, CEC_CTRL);
		}

		cec->ops->enable(cec->hdmi);
		cec->cec_enable = true;

		dw_hdmi_qp_write(cec, ~0, CEC_INT_CLEAR);
		dw_hdmi_qp_write(cec, 1, CEC_LOCK_CONTROL);

		dw_hdmi_qp_cec_log_addr(cec->adap, CEC_LOG_ADDR_INVALID);

		irqs = CEC_STAT_LINE_ERR | CEC_STAT_NACK | CEC_STAT_EOM |
		       CEC_STAT_DONE;
		dw_hdmi_qp_write(cec, ~0, CEC_INT_CLEAR);
		dw_hdmi_qp_write(cec, irqs, CEC_INT_MASK_N);
	}
	return 0;
}

static const struct cec_adap_ops dw_hdmi_qp_cec_ops = {
	.adap_enable = dw_hdmi_qp_cec_enable,
	.adap_log_addr = dw_hdmi_qp_cec_log_addr,
	.adap_transmit = dw_hdmi_qp_cec_transmit,
};

static void dw_hdmi_qp_cec_del(void *data)
{
	struct dw_hdmi_qp_cec *cec = data;

	cec_delete_adapter(cec->adap);
}

static irqreturn_t dw_hdmi_qp_cec_wake_irq(int irq, void *data)
{
	struct cec_adapter *adap = data;
	struct dw_hdmi_qp_cec *cec = cec_get_drvdata(adap);
	u32 cec_int;

	cec_int = dw_hdmi_qp_wakeup_read(cec, CEC_IS);
	if (!cec_int)
		return IRQ_NONE;

	if (!(cec_int & WAKEUP_IS))
		return IRQ_HANDLED;

	dw_hdmi_qp_wakeup_mod(cec, cec_int, cec_int, CEC_IS);
	dw_hdmi_qp_wakeup_write(cec, 0xffff0000, CEC_IE);

	if (!cec->wake_en)
		return IRQ_HANDLED;

	return IRQ_WAKE_THREAD;
}

static irqreturn_t dw_hdmi_qp_cec_wake_thread(int irq, void *data)
{
	struct cec_adapter *adap = data;
	struct dw_hdmi_qp_cec *cec = cec_get_drvdata(adap);

	mutex_lock(&cec->wake_lock);

	if (!cec->standby_en) {
		mutex_unlock(&cec->wake_lock);
		return IRQ_HANDLED;
	}
	cec->standby_en = false;

	input_event(cec->devinput, EV_KEY, KEY_POWER, 1);
	input_sync(cec->devinput);
	input_event(cec->devinput, EV_KEY, KEY_POWER, 0);
	input_sync(cec->devinput);
	mutex_unlock(&cec->wake_lock);

	return IRQ_HANDLED;
}

static int rockchip_hdmi_cec_input_init(struct dw_hdmi_qp_cec *cec)
{
	int err;

	cec->devinput = devm_input_allocate_device(cec->dev);
	if (!cec->devinput)
		return -EPERM;

	cec->devinput->name = "hdmi_cec_key";
	cec->devinput->phys = "hdmi_cec_key/input0";
	cec->devinput->id.bustype = BUS_HOST;
	cec->devinput->id.vendor = 0x0001;
	cec->devinput->id.product = 0x0001;
	cec->devinput->id.version = 0x0100;

	err = input_register_device(cec->devinput);
	if (err < 0)
		return err;

	input_set_capability(cec->devinput, EV_KEY, KEY_POWER);

	return 0;
}

static long cec_standby(struct cec_adapter *adap, __u8 __user *parg)
{
	u8 en;
	int ret;
	struct dw_hdmi_qp_cec *cec = cec_get_drvdata(adap);

	if (copy_from_user(&en, parg, sizeof(en)))
		return -EFAULT;

	mutex_lock(&cec->wake_lock);
	cec->standby_en = !en;
	ret = adap->ops->adap_enable(adap, en);
	mutex_unlock(&cec->wake_lock);

	return ret;
}

static long cec_func_en(struct dw_hdmi_qp_cec *cec, int __user *parg)
{
	int en_mask;

	if (copy_from_user(&en_mask, parg, sizeof(en_mask)))
		return -EFAULT;

	cec->wake_en = (en_mask & CEC_EN) && (en_mask & CEC_WAKE);

	return 0;
}

static long dw_hdmi_qp_cec_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct dw_hdmi_qp_cec *cec;
	struct miscdevice *misc_dev;
	void __user *data;

	if (!f)
		return -EFAULT;

	misc_dev = f->private_data;
	cec = container_of(misc_dev, struct dw_hdmi_qp_cec, misc_dev);
	data = (void __user *)arg;

	switch (cmd) {
	case CEC_STANDBY:
		return cec_standby(cec->adap, data);
	case CEC_FUNC_EN:
		return cec_func_en(cec, data);
	default:
		return -EINVAL;
	}

	return -ENOTTY;
}

static int dw_hdmi_qp_cec_open(struct inode *inode, struct file *f)
{
	return 0;
}

static int dw_hdmi_qp_cec_release(struct inode *inode, struct file *f)
{
	return 0;
}

static const struct file_operations dw_hdmi_qp_cec_file_operations = {
	.compat_ioctl = dw_hdmi_qp_cec_ioctl,
	.unlocked_ioctl = dw_hdmi_qp_cec_ioctl,
	.open = dw_hdmi_qp_cec_open,
	.release = dw_hdmi_qp_cec_release,
	.owner = THIS_MODULE,
};

static int dw_hdmi_qp_cec_probe(struct platform_device *pdev)
{
	struct dw_hdmi_qp_cec_data *data = dev_get_platdata(&pdev->dev);
	struct dw_hdmi_qp_cec *cec;
	int ret;

	if (!data) {
		dev_err(&pdev->dev, "can't get data\n");
		return -ENXIO;
	}

	/*
	 * Our device is just a convenience - we want to link to the real
	 * hardware device here, so that userspace can see the association
	 * between the HDMI hardware and its associated CEC chardev.
	 */
	cec = devm_kzalloc(&pdev->dev, sizeof(*cec), GFP_KERNEL);
	if (!cec)
		return -ENOMEM;

	cec->dev = &pdev->dev;
	cec->ops = data->ops;
	cec->hdmi = data->hdmi;
	cec->irq = data->irq;
	cec->wake_irq = data->wake_irq;
	cec->cec_wakeup_mem = data->cec_wakeup_mem;
	mutex_init(&cec->wake_lock);

	platform_set_drvdata(pdev, cec);

	dw_hdmi_qp_write(cec, 0, CEC_TX_CNT);
	dw_hdmi_qp_write(cec, ~0, CEC_INT_CLEAR);
	dw_hdmi_qp_write(cec, 0, CEC_INT_MASK_N);

	cec->adap = cec_allocate_adapter(&dw_hdmi_qp_cec_ops, cec, "dw_hdmi_qp",
					 CEC_CAP_LOG_ADDRS | CEC_CAP_TRANSMIT |
					 CEC_CAP_RC | CEC_CAP_PASSTHROUGH,
					 CEC_MAX_LOG_ADDRS);
	if (IS_ERR(cec->adap)) {
		dev_err(&pdev->dev, "cec allocate adapter failed\n");
		return PTR_ERR(cec->adap);
	}

	dw_hdmi_qp_set_cec_adap(cec->hdmi, cec->adap);

	/* override the module pointer */
	cec->adap->owner = THIS_MODULE;

	ret = devm_add_action(&pdev->dev, dw_hdmi_qp_cec_del, cec);
	if (ret) {
		dev_err(&pdev->dev, "cec add action failed\n");
		cec_delete_adapter(cec->adap);
		return ret;
	}

	if (cec->irq < 0) {
		ret = cec->irq;
		dev_err(&pdev->dev, "cec get irq failed\n");
		return ret;
	}

	ret = devm_request_threaded_irq(&pdev->dev, cec->irq,
					dw_hdmi_qp_cec_hardirq,
					dw_hdmi_qp_cec_thread, IRQF_SHARED,
					"dw-hdmi-qp-cec", cec->adap);
	if (ret < 0) {
		dev_err(&pdev->dev, "cec request irq thread failed\n");
		return ret;
	}

	cec->notify = cec_notifier_cec_adap_register(pdev->dev.parent,
						     NULL, cec->adap);
	if (!cec->notify) {
		dev_err(&pdev->dev, "cec notifier adap register failed\n");
		return -ENOMEM;
	}

	ret = cec_register_adapter(cec->adap, pdev->dev.parent);
	if (ret < 0) {
		dev_err(&pdev->dev, "cec adap register failed\n");
		cec_notifier_cec_adap_unregister(cec->notify, cec->adap);
		return ret;
	}

	/*
	 * CEC documentation says we must not call cec_delete_adapter
	 * after a successful call to cec_register_adapter().
	 */
	devm_remove_action(&pdev->dev, dw_hdmi_qp_cec_del, cec);

	if (cec->wake_irq > 0) {
		ret = devm_request_threaded_irq(&pdev->dev, cec->wake_irq,
						dw_hdmi_qp_cec_wake_irq,
						dw_hdmi_qp_cec_wake_thread,
						IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
						"cec-wakeup", cec->adap);
		if (ret) {
			dev_err(&pdev->dev,
				"hdmi_cec request_irq failed (%d).\n",
				ret);
			return ret;
		}
		device_init_wakeup(&pdev->dev, 1);
		enable_irq_wake(cec->wake_irq);
	} else {
		return 0;
	}

	rockchip_hdmi_cec_input_init(cec);

	cec->misc_dev.name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "rk_cec");
	if (!cec->misc_dev.name)
		return -ENOMEM;
	cec->misc_dev.minor = MISC_DYNAMIC_MINOR;
	cec->misc_dev.fops = &dw_hdmi_qp_cec_file_operations;
	cec->misc_dev.mode = 0666;

	ret = misc_register(&cec->misc_dev);

	return ret;
}

static int dw_hdmi_qp_cec_remove(struct platform_device *pdev)
{
	struct dw_hdmi_qp_cec *cec = platform_get_drvdata(pdev);

	cec_notifier_cec_adap_unregister(cec->notify, cec->adap);
	cec_unregister_adapter(cec->adap);
	if (cec->wake_irq > 0)
		misc_deregister(&cec->misc_dev);

	return 0;
}

static struct platform_driver dw_hdmi_qp_cec_driver = {
	.probe	= dw_hdmi_qp_cec_probe,
	.remove	= dw_hdmi_qp_cec_remove,
	.driver = {
		.name = "dw-hdmi-qp-cec",
	},
};
module_platform_driver(dw_hdmi_qp_cec_driver);

MODULE_AUTHOR("Algea Cao <algea.cao@rock-chips.com>");
MODULE_DESCRIPTION("Synopsys Designware HDMI QP CEC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS(PLATFORM_MODULE_PREFIX "dw-hdmi-qp-cec");
