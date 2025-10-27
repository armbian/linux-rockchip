// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip Remote Processors Messaging Test.
 *
 * Copyright (c) 2022 Rockchip Electronics Co., Ltd.
 * Author: Hongming Zou <hongming.zou@rock-chips.com>
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/rpmsg/rockchip_rpmsg.h>
#include <linux/time.h>
#include <linux/virtio.h>

#define LINUX_TEST_MSG "Rockchip rpmsg linux test pingpong!"
#define MSG_LIMIT       10000

/* different processor cores may need to adjust the value of this definition */
#define LINUX_RPMSG_COMPENSATION	(1)	//ms

struct rpmsg_info_t {
	int rx_count;
	struct delayed_work send_work;
	struct rpmsg_device *rp;
	uint32_t remote_ept_id;
};

static int rockchip_rpmsg_test_cb(struct rpmsg_device *rp, void *payload,
				  int payload_len, void *priv, u32 src)
{
	int ret;
	uint32_t remote_ept_id;
	struct rpmsg_info_t *info = dev_get_drvdata(&rp->dev);

	remote_ept_id = src;
	dev_info(&rp->dev, "rx msg %s rx_count %d(remote_ept_id: 0x%x)\n",
			(char *)payload, ++info->rx_count, remote_ept_id);

	/* test should not live forever */
	if (info->rx_count >= MSG_LIMIT) {
		dev_info(&rp->dev, "Rockchip rpmsg test exit!\n");
		return 0;
	}

	mdelay(LINUX_RPMSG_COMPENSATION);
	/* send a new message now */
	ret = rpmsg_sendto(rp->ept, LINUX_TEST_MSG, strlen(LINUX_TEST_MSG), remote_ept_id);
	if (ret)
		dev_err(&rp->dev, "rpmsg_send failed: %d\n", ret);

	return ret;
}

static void rockchip_rpmsg_send_work_handler(struct work_struct *work)
{
	int ret;
	struct rpmsg_info_t *info = container_of(work, struct rpmsg_info_t, send_work.work);

	ret = rpmsg_sendto(info->rp->ept, LINUX_TEST_MSG, strlen(LINUX_TEST_MSG), info->remote_ept_id);
	if (ret)
		dev_err(&info->rp->dev, "rpmsg_send failed: %d\n", ret);
}

static int rockchip_rpmsg_test_probe(struct rpmsg_device *rp)
{
	struct rpmsg_info_t *info;

	dev_info(&rp->dev, "new channel: 0x%x -> 0x%x!\n", rp->src, rp->dst);

	info = devm_kzalloc(&rp->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	dev_set_drvdata(&rp->dev, info);

	/* wo need to announce the new ept to remote */
	rp->announce = rp->src != RPMSG_ADDR_ANY;

	/* Initialize delayed work */
	info->rp = rp;
	info->remote_ept_id = rp->dst;
	INIT_DELAYED_WORK(&info->send_work, rockchip_rpmsg_send_work_handler);

	schedule_delayed_work(&info->send_work, msecs_to_jiffies(10));

	return 0;
}

static void rockchip_rpmsg_test_remove(struct rpmsg_device *rp)
{
	dev_info(&rp->dev, "rockchip rpmsg test is removed\n");
}

static struct rpmsg_device_id rockchip_rpmsg_test_id_table[] = {
	{ .name = "rpmsg-ap3-ch0" },
	{ .name = "rpmsg-mcu0-test" },
	{ /* sentinel */ },
};

static struct rpmsg_driver rockchip_rpmsg_test = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= rockchip_rpmsg_test_id_table,
	.probe		= rockchip_rpmsg_test_probe,
	.callback	= rockchip_rpmsg_test_cb,
	.remove		= rockchip_rpmsg_test_remove,
};

static int __init init(void)
{
	return register_rpmsg_driver(&rockchip_rpmsg_test);
}

static void __exit fini(void)
{
	unregister_rpmsg_driver(&rockchip_rpmsg_test);
}
module_init(init);
module_exit(fini);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Rockchip Remote Processors Messaging Test");
MODULE_AUTHOR("Hongming Zou <hongming.zou@rock-chips.com>");

