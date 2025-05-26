// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip AMP RPMSG timesync.
 *
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 * Author: Zain Wang <zain.wang@rock-chips.com>
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/time.h>
#include <linux/virtio.h>
#include <linux/workqueue.h>
#include "rockchip_amp_rpmsg_timesync.h"

struct rkamp_timesync_device {
	struct delayed_work dwork;
	struct rk_timer_info timer_info;
	struct rpmsg_device *rdev;
	void __iomem *timer_regs;
};

static struct rkamp_timesync_device *gp_ts_dev;

static void rkamp_timesync_work(struct work_struct *work)
{
	struct rkamp_timesync_device *ts_dev =
			container_of(work, struct rkamp_timesync_device, dwork.work);
	struct rpmsg_device *rdev = ts_dev->rdev;
	struct rkamp_timesync_msg msg = { 0 };
	int ret;

	msg.op_flag = RK_TIMESYNC_OP_START;
	ret = rpmsg_sendto(rdev->ept, &msg, sizeof(msg), rdev->dst);
	if (ret)
		dev_err(&rdev->dev, "rpmsg_send failed: %d\n", ret);
}

static uint64_t rkamp_timer_get_count(struct rkamp_timesync_device *ts_dev,
				      void __iomem *timer_regs)
{
	struct rk_timer_info *timer = &ts_dev->timer_info;
	uint32_t high, low, temp;

	if (timer->version != RK_TIMER_VERSION) {
		dev_err(&ts_dev->rdev->dev, "Bad Timer version\n");
		return 0;
	}

	/* Sync from Hal */
	do {
		high = readl(timer_regs + 4);
		low = readl(timer_regs);
		temp = readl(timer_regs + 4);
	} while (high != temp);

	return ((uint64_t)high << 32) | low;
}

static int rkamp_timesync_callback(struct rpmsg_device *rdev, void *payload,
				   int payload_len, void *priv, u32 src)
{
	struct rkamp_timesync_msg *msg = payload;
	struct rkamp_timesync_msg reply = { 0 };
	struct rkamp_timesync_device *ts_dev = dev_get_drvdata(&rdev->dev);
	s64 kernel_ns, mcu_cnt, kernel_cnt;
	int ret;
	unsigned long flags;
	void __iomem *timer_regs;

	if (msg->op_flag != RK_TIMESYNC_OP_TIMEINFO) {
		dev_warn(&rdev->dev, "Bad OP_FLAG 0x%x\n", msg->op_flag);
		return -EINVAL;
	}

	memcpy(&ts_dev->timer_info, &msg->timer_info, sizeof(struct rk_timer_info));
	timer_regs = ioremap(msg->timer_info.addr, 8);
	if (!timer_regs) {
		dev_err(&rdev->dev, "failed to ioremap reg (0x%x) for timer\n",
			msg->timer_info.addr);
		return -EINVAL;
	}

	local_irq_save(flags);
	kernel_ns = ktime_get_ns();
	mcu_cnt = rkamp_timer_get_count(ts_dev, timer_regs);
	local_irq_restore(flags);

	iounmap(timer_regs);
	/* FIMXE: OVERRUN ? */
	reply.op_flag = RK_TIMESYNC_OP_CYCLE_BASE;
	kernel_cnt = div64_u64(kernel_ns * msg->timer_info.clk_rate, 1000);
	reply.cycle_base = kernel_cnt - mcu_cnt;

	ret = rpmsg_sendto(rdev->ept, &reply, sizeof(reply), rdev->dst);
	if (ret)
		dev_err(&rdev->dev, "rpmsg_send failed: %d\n", ret);

	return 0;
}

static int rkamp_timesync_probe(struct rpmsg_device *rdev)
{
	struct rkamp_timesync_device *ts_dev;

	if (gp_ts_dev) {
		dev_info(&rdev->dev, "Timesync rpmsg module has be registered\n");
		return -EINVAL;
	}

	ts_dev = devm_kzalloc(&rdev->dev, sizeof(*ts_dev), GFP_KERNEL);
	if (!ts_dev)
		return -ENOMEM;

	ts_dev->rdev = rdev;
	INIT_DELAYED_WORK(&ts_dev->dwork, rkamp_timesync_work);
	dev_set_drvdata(&rdev->dev, ts_dev);
	gp_ts_dev = ts_dev;

	/* wo need to announce the new ept to remote */
	rdev->announce = rdev->src != RPMSG_ADDR_ANY;

	schedule_delayed_work(&ts_dev->dwork, msecs_to_jiffies(10));
	return 0;
}

static void rkamp_timesync_remove(struct rpmsg_device *rdev)
{
	struct rkamp_timesync_device *ts_dev = dev_get_drvdata(&rdev->dev);

	cancel_delayed_work_sync(&ts_dev->dwork);
	gp_ts_dev = NULL;
}

static struct rpmsg_device_id rkamp_timesync_id_table[] = {
	{ .name = "rkamp-timesync" },
	{ /* sentinel */ },
};

static struct rpmsg_driver rkamp_timesync_driver = {
	.drv.name	= KBUILD_MODNAME,
	.id_table	= rkamp_timesync_id_table,
	.probe		= rkamp_timesync_probe,
	.callback	= rkamp_timesync_callback,
	.remove		= rkamp_timesync_remove,
};

module_rpmsg_driver(rkamp_timesync_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Rockchip RPMSG AMP Time Sync");
MODULE_AUTHOR("Zain Wang <zain.wang@rock-chips.com>");
