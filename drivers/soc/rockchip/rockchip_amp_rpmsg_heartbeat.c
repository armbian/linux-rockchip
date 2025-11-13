// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip rpmsg heartbeat driver.
 *
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 * Author: Jiahang Zheng <jiahang.zheng@rock-chips.com>
 */

/* #define DEBUG */
#include <linux/rpmsg.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/remoteproc.h>

#define HEARTBEAT_DEVICE_NAME	"rpmsg_heartbeat"
#define HEART_RATE		(3 * HZ)

extern struct class *rpmsg_class;

struct rpmsg_heartbeat {
	struct rpmsg_device *rpdev;
	uint32_t tick;
	atomic_t sleep;

	dev_t devno;
	struct cdev cdev;
	struct device *device;
	wait_queue_head_t wait_queue;
	atomic_t event_flag;

	struct timer_list rpmsg_heartbeat_timer;
};

static int heartbeat_open(struct inode *inode, struct file *filp)
{
	struct rpmsg_heartbeat *chip = container_of(inode->i_cdev,
						    struct rpmsg_heartbeat,
						    cdev);
	filp->private_data = chip;
	return 0;
}

static int heartbeat_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t heartbeat_read(struct file *filp, char __user *buf,
			      size_t count, loff_t *f_pos)
{
	struct rpmsg_heartbeat *chip = filp->private_data;
	uint8_t event;
	int ret;

	if (count < sizeof(event))
		return -EINVAL;

	event = (uint8_t)atomic_read(&chip->event_flag);

	ret = copy_to_user(buf, &event, sizeof(event));
	if (ret) {
		dev_err(&chip->rpdev->dev, "Heartbeat read ret %d\n", ret);
		return -EFAULT;
	}

	return sizeof(event);
}

static __poll_t heartbeat_poll(struct file *filp, poll_table *wait)
{
	struct rpmsg_heartbeat *chip = filp->private_data;
	__poll_t mask = 0;

	poll_wait(filp, &chip->wait_queue, wait);

	if (atomic_read(&chip->event_flag))
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static const struct file_operations heartbeat_fops = {
	.owner = THIS_MODULE,
	.open = heartbeat_open,
	.release = heartbeat_release,
	.read = heartbeat_read,
	.poll = heartbeat_poll,
};

static void set_heartbeat_event(struct rpmsg_heartbeat *chip, unsigned int event)
{
	atomic_or(event, &chip->event_flag);

	wake_up_interruptible(&chip->wait_queue);
}

static void heartbeat_timeout_handler(struct timer_list *t)
{
	struct rpmsg_heartbeat *chip = from_timer(chip, t, rpmsg_heartbeat_timer);

	dev_err(&chip->rpdev->dev, "Heartbeat timeout\n");

	set_heartbeat_event(chip, 0x01);
}

static int rpmsg_heartbeat_cb(struct rpmsg_device *rpdev, void *data, int len,
			      void *priv, u32 src)
{
	struct rpmsg_heartbeat *chip = dev_get_drvdata(&rpdev->dev);

	if (!atomic_read(&chip->sleep))
		mod_timer(&chip->rpmsg_heartbeat_timer, jiffies + HEART_RATE);

	atomic_set(&chip->event_flag, 0);

	rpmsg_sendto(rpdev->ept, HEARTBEAT_DEVICE_NAME, strlen(HEARTBEAT_DEVICE_NAME), src);

	return 0;
}

static int rpmsg_heartbeat_probe(struct rpmsg_device *rpdev)
{
	struct rpmsg_heartbeat *chip;
	int ret;
	dev_t devno;

	chip = devm_kzalloc(&rpdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	init_waitqueue_head(&chip->wait_queue);
	atomic_set(&chip->event_flag, 0);
	atomic_set(&chip->sleep, 0);

	ret = alloc_chrdev_region(&devno, 0, 1, HEARTBEAT_DEVICE_NAME);
	if (ret < 0) {
		dev_err(&rpdev->dev, "Failed to allocate chrdev region\n");
		return ret;
	}

	chip->devno = devno;

	cdev_init(&chip->cdev, &heartbeat_fops);
	chip->cdev.owner = THIS_MODULE;

	ret = cdev_add(&chip->cdev, chip->devno, 1);
	if (ret) {
		dev_err(&rpdev->dev, "Failed to add cdev\n");
		goto err_cdev;
	}

	if (!rpmsg_class) {
		dev_err(&rpdev->dev, "rpmsg_class not available\n");
		ret = -ENODEV;
		goto err_device;
	}

	chip->device = device_create(rpmsg_class, NULL,
				     chip->devno, NULL,
				     "heartbeat");
	if (IS_ERR(chip->device)) {
		ret = PTR_ERR(chip->device);
		dev_err(&rpdev->dev, "Failed to create device\n");
		goto err_device;
	}

	timer_setup(&chip->rpmsg_heartbeat_timer, heartbeat_timeout_handler, 0);

	chip->rpdev = rpdev;
	chip->tick = 0;

	dev_set_drvdata(&rpdev->dev, chip);
	/* we need to announce the new ept to remote */
	rpdev->announce = rpdev->src != RPMSG_ADDR_ANY;

	rpmsg_sendto(rpdev->ept, HEARTBEAT_DEVICE_NAME, strlen(HEARTBEAT_DEVICE_NAME), rpdev->dst);

	dev_info(&rpdev->dev, "Heartbeat device registered at /dev/heartbeat\n");

	return 0;

err_device:
	cdev_del(&chip->cdev);
err_cdev:
	unregister_chrdev_region(chip->devno, 1);
	return ret;
}

static void rpmsg_heartbeat_remove(struct rpmsg_device *rpdev)
{
	struct rpmsg_heartbeat *chip = dev_get_drvdata(&rpdev->dev);

	if (!chip)
		return;

	dev_info(&rpdev->dev, "%s is being removed\n", dev_name(&rpdev->dev));

	/* Mark as sleeping to prevent timer restart */
	atomic_set(&chip->sleep, 1);

	del_timer_sync(&chip->rpmsg_heartbeat_timer);

	if (chip->device) {
		device_destroy(rpmsg_class, chip->devno);
		chip->device = NULL;
	}

	if (chip->cdev.owner)
		cdev_del(&chip->cdev);

	if (chip->devno) {
		unregister_chrdev_region(chip->devno, 1);
		chip->devno = 0;
	}

	wake_up_interruptible(&chip->wait_queue);

	dev_set_drvdata(&rpdev->dev, NULL);

	dev_info(&rpdev->dev, "Heartbeat device completely removed\n");
}

#ifdef CONFIG_PM_SLEEP
static int rk_rpmsg_heartbeat_suspend(struct device *dev)
{
	struct rpmsg_heartbeat *chip = NULL;

	chip = dev_get_drvdata(dev);

	atomic_set(&chip->sleep, 1);
	del_timer_sync(&chip->rpmsg_heartbeat_timer);

	return 0;
}

static void rk_rpmsg_heartbeat_resume(struct device *dev)
{
	struct rpmsg_heartbeat *chip = NULL;

	chip = dev_get_drvdata(dev);

	atomic_set(&chip->sleep, 0);
	mod_timer(&chip->rpmsg_heartbeat_timer, jiffies + HEART_RATE);
}

static const struct dev_pm_ops rk_rpmsg_heartbeat_pm_ops = {
	.prepare = rk_rpmsg_heartbeat_suspend,
	.complete = rk_rpmsg_heartbeat_resume,
};
#endif

static struct rpmsg_device_id rpmsg_driver_heartbeat_id_table[] = {
	{ .name	= "rpmsg_heartbeat" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, rpmsg_driver_heartbeat_id_table);

static struct rpmsg_driver rpmsg_sample_client = {
	.drv = {
		.name	= KBUILD_MODNAME,
#ifdef CONFIG_PM_SLEEP
		.pm = &rk_rpmsg_heartbeat_pm_ops,
#endif
	},
	.id_table	= rpmsg_driver_heartbeat_id_table,
	.probe		= rpmsg_heartbeat_probe,
	.callback	= rpmsg_heartbeat_cb,
	.remove		= rpmsg_heartbeat_remove,
};
module_rpmsg_driver(rpmsg_sample_client);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Rockchip Remote Processor Messaging Heartbeat Driver");
MODULE_AUTHOR("Jiahang Zheng <jiahang.zheng@rock-chips.com>");
