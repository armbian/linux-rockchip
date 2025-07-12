// SPDX-License-Identifier: GPL-2.0
/*
 * MCU Log RPMSG driver
 * Receives logs from remote MCU processor via RPMSG and saves to /userdata/mcu.log
 * Each received message is acknowledged.
 *
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/of_address.h>
#include <linux/dma-mapping.h>

#define MCU_LOG_RPMSG_EPT_NAME "rpmsg-mculog"
#define LOG_PATH "/userdata/mcu.log"

struct muclog_entry {
	u32 mem_base;
	u32 read_index;
	u32 size;
	u32 mem_max;
};

struct mculog_dev {
	struct rpmsg_device *rpdev;
	spinlock_t lock;
	struct work_struct log_work;
	struct muclog_entry log_entry;
	struct file *filp;
	loff_t pos;
	char *buf;
};

static void log_work_handler(struct work_struct *work)
{
	struct mculog_dev *mdev = container_of(work, struct mculog_dev, log_work);
	struct muclog_entry e;
	u32 ack[2];
	u32 max_size;
	ssize_t written;
	unsigned long flags;

	spin_lock_irqsave(&mdev->lock, flags);
	e = mdev->log_entry;
	spin_unlock_irqrestore(&mdev->lock, flags);

	max_size = e.mem_max - e.mem_base;

	if (e.mem_base + e.read_index >= e.mem_max ||
	    e.size > max_size) {
		dev_err(&mdev->rpdev->dev, "Bad mcu log entry, read_index: %d, size: %d\n",
			e.read_index, e.size);
		goto out;
	}

	if (IS_ERR_OR_NULL(mdev->filp)) {
		mdev->filp = filp_open(LOG_PATH, O_RDWR | O_APPEND, 0666);
		if (IS_ERR(mdev->filp)) {
			dev_err(&mdev->rpdev->dev, "Open %s failed\n", LOG_PATH);
			goto out;
		}
	}

	if (!mdev->buf) {
		/* Use MEMREMAP_WC to avoid cache coherency issues with MCU shared memory */
		mdev->buf = (__force char *) memremap(e.mem_base, max_size, MEMREMAP_WC);
		if (mdev->buf == NULL) {
			dev_err(&mdev->rpdev->dev, "memremap 0x%08x failed\n", e.mem_base);
			goto out;
		}
	}

	dev_dbg(&mdev->rpdev->dev, "Base 0x%08x -> vir %p, read from %d, size %d\n",
		e.mem_base, mdev->buf, e.read_index, e.size);

	if (e.read_index + e.size < max_size) {
		written = kernel_write(mdev->filp, mdev->buf + e.read_index, e.size, &mdev->pos);
	} else {
		u32 first = max_size - e.read_index;

		written = kernel_write(mdev->filp, mdev->buf + e.read_index, first, &mdev->pos);

		written += kernel_write(mdev->filp, mdev->buf, e.size - first, &mdev->pos);
	}

	if (written != e.size)
		dev_dbg(&mdev->rpdev->dev, "write count %ld\n", written);

out:
	ack[0] = e.read_index;
	ack[1] = e.size;
	rpmsg_send(mdev->rpdev->ept, ack, sizeof(ack));
}

static int mculog_rpmsg_cb(struct rpmsg_device *rpdev, void *data, int len,
			   void *priv, u32 src)
{
	struct mculog_dev *mdev = dev_get_drvdata(&rpdev->dev);
	unsigned long flags;

	spin_lock_irqsave(&mdev->lock, flags);

	mdev->log_entry = *(struct muclog_entry *)data;

	spin_unlock_irqrestore(&mdev->lock, flags);

	schedule_work(&mdev->log_work);

	return 0;
}

static int mculog_rpmsg_probe(struct rpmsg_device *rpdev)
{
	struct mculog_dev *mdev;
	int ret;

	dev_info(&rpdev->dev, "MCU Log RPMSG driver probe enter\n");
	mdev = devm_kzalloc(&rpdev->dev, sizeof(*mdev), GFP_KERNEL);
	if (!mdev)
		return -ENOMEM;

	mdev->rpdev = rpdev;
	spin_lock_init(&mdev->lock);
	INIT_WORK(&mdev->log_work, log_work_handler);

	dev_set_drvdata(&rpdev->dev, mdev);

	dev_info(&rpdev->dev, "MCU Log RPMSG driver probed, src[%d] -> dts[%d]\n",
		 rpdev->src, rpdev->dst);

	rpdev->announce = rpdev->src != RPMSG_ADDR_ANY;

	ret = rpmsg_trysend(rpdev->ept, (void *)&ret, sizeof(ret));
	if (ret) {
		dev_err(&rpdev->dev, "rpmsg_send failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static void mculog_rpmsg_remove(struct rpmsg_device *rpdev)
{
	struct mculog_dev *mdev = dev_get_drvdata(&rpdev->dev);

	cancel_work_sync(&mdev->log_work);
	dev_info(&rpdev->dev, "MCU Log RPMSG driver removed\n");

	if (!IS_ERR(mdev->filp))
		filp_close(mdev->filp, NULL);

	if (mdev->buf)
		memunmap(mdev->buf);
}

static struct rpmsg_device_id mculog_rpmsg_id_table[] = {
	{ .name	= MCU_LOG_RPMSG_EPT_NAME },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, mculog_rpmsg_id_table);

static struct rpmsg_driver mculog_rpmsg_driver = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= mculog_rpmsg_id_table,
	.probe		= mculog_rpmsg_probe,
	.callback	= mculog_rpmsg_cb,
	.remove		= mculog_rpmsg_remove,
};
module_rpmsg_driver(mculog_rpmsg_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MCU Log RPMSG Driver");
