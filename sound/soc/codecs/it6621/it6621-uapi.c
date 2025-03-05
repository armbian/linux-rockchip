// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 * Author: Jason Zhang <jason.zhang@rock-chips.com>
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/poll.h>

#include "it6621.h"
#include "it6621-earc.h"
#include "it6621-uapi.h"

#define IT6621_MAX_MSGS			18
#define IT6621_MAX_MSG_LEN		256

struct it6621_msg {
	u8 event;
	u8 data[IT6621_MAX_MSG_LEN];
} __packed;

/* Command */
#define IT6621_EARC_SET_ENABLED		_IOW('a', 0x01, unsigned int)
#define IT6621_EARC_GET_STATE		_IOR('a', 0x02, unsigned int)
#define IT6621_EARC_GET_AUDIO_CAP	_IOR('a', 0x03, u8[IT6621_MAX_MSG_LEN])
#define IT6621_EARC_GET_EVENT		_IOR('a', 0x04, struct it6621_msg)
#define IT6621_EARC_SET_ENTER_ARC	_IOW('a', 0x05, unsigned int)

struct it6621_msg_entry {
	struct list_head list;
	struct it6621_msg msg;
};

struct it6621_fh {
	struct list_head list;
	struct list_head msgs;
	unsigned int msgs_num;
	struct mutex msgs_lock;
	wait_queue_head_t wait;
	struct it6621_priv *priv;
	struct mutex valid_lock;
	bool valid;
};

static int minor;

static bool it6621_fh_get_valid(struct it6621_fh *fh)
{
	bool valid;

	mutex_lock(&fh->valid_lock);
	valid = fh->valid;
	mutex_unlock(&fh->valid_lock);

	return valid;
}

static void it6621_fh_set_valid(struct it6621_fh *fh, bool valid)
{
	mutex_lock(&fh->valid_lock);
	fh->valid = valid;
	mutex_unlock(&fh->valid_lock);
}

static int it6621_fh_set_earc_enabled(struct it6621_fh *fh,
				      unsigned int __user *argp)
{
	struct it6621_priv *priv = fh->priv;
	unsigned int enabled;
	int ret;

	ret = copy_from_user(&enabled, argp, sizeof(enabled));
	if (ret)
		return -EFAULT;

	return it6621_set_earc_enabled(priv, !!enabled);
}

static int it6621_fh_get_earc_state(struct it6621_fh *fh,
				    unsigned int __user *argp)
{
	struct it6621_priv *priv = fh->priv;
	int ret;

	ret = copy_to_user(argp, &priv->state, sizeof(priv->state));
	if (ret)
		return -EFAULT;

	return 0;
}

static int it6621_fh_get_earc_audio_cap(struct it6621_fh *fh, u8 __user *argp)
{
	struct it6621_priv *priv = fh->priv;
	int ret;

	mutex_lock(&priv->rxcap_lock);
	ret = copy_to_user(argp, priv->rxcap, sizeof(priv->rxcap));
	mutex_unlock(&priv->rxcap_lock);

	if (ret)
		return -EFAULT;

	return 0;
}

static int it6621_fh_get_msg(struct it6621_fh *fh,
			     struct it6621_msg __user *argp,
			     bool block)
{
	struct it6621_msg_entry *entry;
	int ret = 0;

	do {
		mutex_lock(&fh->msgs_lock);

		if (fh->msgs_num) {
			entry = list_first_entry(&fh->msgs,
						 struct it6621_msg_entry,
						 list);
			list_del(&entry->list);
			ret = copy_to_user(argp, &entry->msg, sizeof(entry->msg));
			kfree(entry);
			fh->msgs_num--;
			mutex_unlock(&fh->msgs_lock);

			if (ret)
				return -EFAULT;

			return 0;
		}

		mutex_unlock(&fh->msgs_lock);

		if (!block)
			return -EAGAIN;

		ret = wait_event_interruptible(fh->wait, fh->msgs_num);

		if (!it6621_fh_get_valid(fh))
			return -ENXIO;
		/* Exit on error, otherwise loop to get the new message */
	} while (!ret);

	return ret;
}

static int it6621_fh_push_msg(struct it6621_fh *fh, u8 event, void *data,
			      size_t len)
{
	struct it6621_msg_entry *entry;

	mutex_lock(&fh->msgs_lock);

	if (fh->msgs_num <= IT6621_MAX_MSGS) {
		entry = kzalloc(sizeof(*entry), GFP_KERNEL);
		if (!entry) {
			mutex_unlock(&fh->msgs_lock);
			return -ENOMEM;
		}

		entry->msg.event = event;
		memcpy(entry->msg.data, data, len);
		list_add_tail(&entry->list, &fh->msgs);
		fh->msgs_num++;
	} else {
		dev_warn(fh->priv->dev, "queue is full for fh: %p", fh);
	}

	mutex_unlock(&fh->msgs_lock);

	wake_up_interruptible(&fh->wait);

	return 0;
}

static int it6621_fh_set_enter_arc(struct it6621_fh *fh,
				   unsigned int __user *argp)
{
	struct it6621_priv *priv = fh->priv;
	int enabled;
	int ret;

	ret = copy_from_user(&enabled, argp, sizeof(enabled));
	if (ret)
		return -EFAULT;

	return it6621_set_enter_arc(priv, !!enabled);
}

static int it6621_uapi_open(struct inode *inode, struct file *file)
{
	struct miscdevice *mdev = file->private_data;
	struct it6621_priv *priv = container_of(mdev, struct it6621_priv, mdev);
	struct it6621_fh *fh;

	if (!priv->uapi_registered)
		return -ENXIO;

	fh = kzalloc(sizeof(*fh), GFP_KERNEL);
	if (!fh)
		return -ENOMEM;

	fh->priv = priv;
	fh->valid = true;
	fh->msgs_num = 0;
	INIT_LIST_HEAD(&fh->msgs);
	mutex_init(&fh->msgs_lock);
	mutex_init(&fh->valid_lock);
	init_waitqueue_head(&fh->wait);

	mutex_lock(&priv->fhs_lock);
	list_add(&fh->list, &priv->fhs);
	mutex_unlock(&priv->fhs_lock);

	file->private_data = fh;

	return 0;
}

static long it6621_uapi_ioctl(struct file *file, unsigned int cmd,
			      unsigned long arg)
{
	struct it6621_fh *fh = file->private_data;
	bool block = !(file->f_flags & O_NONBLOCK);
	void __user *argp = (void __user *)arg;

	if (!it6621_fh_get_valid(fh))
		return -ENODEV;

	switch (cmd) {
	case IT6621_EARC_SET_ENABLED:
		return it6621_fh_set_earc_enabled(fh, argp);
	case IT6621_EARC_GET_STATE:
		return it6621_fh_get_earc_state(fh, argp);
	case IT6621_EARC_GET_AUDIO_CAP:
		return it6621_fh_get_earc_audio_cap(fh, argp);
	case IT6621_EARC_GET_EVENT:
		return it6621_fh_get_msg(fh, argp, block);
	case IT6621_EARC_SET_ENTER_ARC:
		return it6621_fh_set_enter_arc(fh, argp);
	default:
		return -ENOTTY;
	}
}

static int it6621_uapi_release(struct inode *inode, struct file *file)
{
	struct it6621_fh *fh = file->private_data;
	struct it6621_priv *priv = fh->priv;
	struct it6621_msg_entry *entry;

	if (it6621_fh_get_valid(fh)) {
		mutex_lock(&priv->fhs_lock);
		list_del(&fh->list);
		mutex_unlock(&priv->fhs_lock);
	}

	mutex_lock(&fh->msgs_lock);

	while (!list_empty(&fh->msgs)) {
		entry = list_first_entry(&fh->msgs, struct it6621_msg_entry,
					 list);
		list_del(&entry->list);
		kfree(entry);
	}

	mutex_unlock(&fh->msgs_lock);

	kfree(fh);
	file->private_data = NULL;

	return 0;
}

static __poll_t it6621_uapi_poll(struct file *file,
				 struct poll_table_struct *poll)
{
	struct it6621_fh *fh = file->private_data;
	__poll_t ret = 0;

	poll_wait(file, &fh->wait, poll);
	if (!it6621_fh_get_valid(fh))
		ret = EPOLLERR | EPOLLHUP;
	else
		ret = EPOLLIN | EPOLLRDNORM;

	return ret;
}

const struct file_operations it6621_uapi_fops = {
	.owner = THIS_MODULE,
	.open = it6621_uapi_open,
	.unlocked_ioctl = it6621_uapi_ioctl,
	.compat_ioctl = it6621_uapi_ioctl,
	.release = it6621_uapi_release,
	.poll = it6621_uapi_poll,
	.llseek = no_llseek,
};

int it6621_uapi_init(struct it6621_priv *priv)
{
	char *name;
	int ret;

	name = kzalloc(NAME_MAX, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	snprintf(name, NAME_MAX, "ite-earc%d", minor++);
	priv->mdev.minor = MISC_DYNAMIC_MINOR;
	priv->mdev.name = name;
	priv->mdev.fops = &it6621_uapi_fops;

	ret = misc_register(&priv->mdev);
	if (ret)
		return ret;

	priv->uapi_registered = true;

	return 0;
}

void it6621_uapi_remove(struct it6621_priv *priv)
{
	struct it6621_fh *fh;

	mutex_lock(&priv->fhs_lock);

	priv->uapi_registered = false;

	list_for_each_entry(fh, &priv->fhs, list)
		it6621_fh_set_valid(fh, false);

	list_for_each_entry(fh, &priv->fhs, list)
		wake_up_interruptible(&fh->wait);

	mutex_unlock(&priv->fhs_lock);

	misc_deregister(&priv->mdev);
	kfree(priv->mdev.name);
}

int it6621_uapi_msg(struct it6621_priv *priv, u8 event, void *data, size_t len)
{
	struct it6621_fh *fh;
	int ret = 0;

	if (!priv->uapi_registered)
		return 0;

	mutex_lock(&priv->fhs_lock);

	list_for_each_entry(fh, &priv->fhs, list) {
		ret = it6621_fh_push_msg(fh, event, data, len);
		if (ret)
			break;
	}

	mutex_unlock(&priv->fhs_lock);

	return ret;
}
