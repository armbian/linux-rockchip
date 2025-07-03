// SPDX-License-Identifier: GPL-2.0
/*
 * MCU read/write files via rpmsg
 *
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 * Author: Jiahang Zheng <jiahang.zheng@rock-chips.com>
 */

#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/types.h>
#include <linux/fdtable.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/string.h>
#include <linux/kref.h>

#define RPMSG_FILE_CONFIG_MAGIC_NUM	0x4B525644
#define RPMSG_FILE_NON_RESPONSE		0x85A9246C

enum file_cmd {
	FILE_CMD_OPEN = 0,
	FILE_CMD_SEEK,
	FILE_CMD_READ,
	FILE_CMD_WRITE,
	FILE_CMD_CLOSE,
	FILE_CMD_FSYNC
};

#define BASE_DIR "/userdata"
#define MAX_FILENAME_LEN 100
#define MAX_DATA_SIZE 400

struct file_io_request {
	u32 cmd;
	u32 id;
	s32 fd;
	int flags;
	int whence;
	s32 offset;
	u32 size;
	u32 non_response;
	char data[MAX_DATA_SIZE];
} __packed;

struct file_io_response {
	u32 cmd;
	u32 id;
	s32 result;
	u32 size;
	char data[MAX_DATA_SIZE];
} __packed;

#define DRV_NAME "rpmsg_file_io"
#define RPMSG_DEVICE_NAME "rpmsg_file_io"

struct rpmsg_file_io {
	struct rpmsg_device *rpdev;
	struct list_head files;
	struct mutex lock;
};

struct rpmsg_file_handle {
	int fd;
	struct file *fp;
	struct kref refcount;
	struct list_head list;
};

static void rpmsg_file_handle_release(struct kref *ref)
{
	struct rpmsg_file_handle *fh = container_of(ref, struct rpmsg_file_handle, refcount);

	kfree(fh);
}

static struct rpmsg_file_handle *find_fd(struct rpmsg_file_io *priv, int fd)
{
	struct rpmsg_file_handle *fh;
	struct rpmsg_file_handle *found = NULL;

	mutex_lock(&priv->lock);
	list_for_each_entry(fh, &priv->files, list) {
		if (fh->fd == fd) {
			kref_get(&fh->refcount);
			found = fh;
			break;
		}
	}
	mutex_unlock(&priv->lock);
	return found;
}

static int sanitize_filename(const char *name)
{
	int i;

	if (!name || name[0] == '\0')
		return -EINVAL;

	if (strnlen(name, MAX_FILENAME_LEN) >= MAX_FILENAME_LEN)
		return -EINVAL;

	for (i = 0; name[i] != '\0'; i++) {
		// Directory separators are not allowed
		if (name[i] == '/' || name[i] == '\\')
			return -EINVAL;

		// Directory traversal is not allowed
		if (i > 0 && name[i] == '.' && name[i-1] == '.')
			return -EINVAL;
	}

	return 0;
}

static int check_allowed_flags(int flags)
{
	// Basic permissible flags
	int allowed = O_RDONLY | O_WRONLY | O_RDWR | O_CREAT | O_EXCL |
		      O_TRUNC | O_APPEND | O_CLOEXEC;

	if (flags & ~allowed)
		return -EINVAL;

	return 0;
}

static int handle_open(struct rpmsg_file_io *priv, struct file_io_request *req,
			struct file_io_response *resp)
{
	struct file *fp;
	struct rpmsg_file_handle *fh;
	int fd = -1;
	struct rpmsg_device *rpdev = priv->rpdev;
	int ret = 0;
	char full_path[MAX_FILENAME_LEN];
	const char *filename = req->data;

	ret = sanitize_filename(filename);
	if (ret < 0) {
		dev_err(&rpdev->dev, "Filename validation failed: %s\n", filename);
		return ret;
	}

	ret = check_allowed_flags(req->flags);
	if (ret < 0) {
		dev_err(&rpdev->dev, "Invalid flags: 0x%x\n", req->flags);
		return ret;
	}

	snprintf(full_path, MAX_FILENAME_LEN, "%s/%s", BASE_DIR, filename);

	fp = filp_open(full_path, req->flags, 0666);
	if (IS_ERR(fp)) {
		dev_err(&rpdev->dev, "open fail: %s, error %ld\n", filename, PTR_ERR(fp));
		return PTR_ERR(fp);
	}

	dev_dbg(&rpdev->dev, "open success: %s\n", full_path);

	fh = kmalloc(sizeof(*fh), GFP_KERNEL);
	if (!fh) {
		ret = -ENOMEM;
		dev_err(&rpdev->dev, "malloc error\n");
		goto err_close_file;
	}

	fd = get_unused_fd_flags(0);
	if (fd < 0) {
		ret = fd;
		dev_err(&rpdev->dev, "get fd error\n");
		goto err_free_fh;
	}
	fd_install(fd, fp);

	fh->fd = fd;
	fh->fp = fp;

	kref_init(&fh->refcount); /* Initialize the reference count to 1 */

	mutex_lock(&priv->lock);
	list_add(&fh->list, &priv->files);
	mutex_unlock(&priv->lock);

	resp->result = fd;
	return 0;

err_free_fh:
	kfree(fh);
err_close_file:
	if (!IS_ERR_OR_NULL(fp))
		filp_close(fp, NULL);

	return ret;
}

static int handle_seek(struct rpmsg_file_io *priv, struct file_io_request *req,
		       struct file_io_response *resp)
{
	struct rpmsg_file_handle *fh;
	loff_t offset;
	loff_t res;

	fh = find_fd(priv, req->fd);
	if (!fh)
		return -EBADF;

	offset = (loff_t)req->offset;

	res = vfs_llseek(fh->fp, offset, req->whence);

	kref_put(&fh->refcount, rpmsg_file_handle_release);

	if (res < 0)
		return (int)res;

	resp->result = res;
	return 0;
}

static int handle_read(struct rpmsg_file_io *priv, struct file_io_request *req,
		       struct file_io_response *resp)
{
	struct rpmsg_file_handle *fh;
	loff_t pos;
	ssize_t ret;
	u32 read_size;

	fh = find_fd(priv, req->fd);
	if (!fh)
		return -EBADF;

	read_size = req->size;
	if (read_size > MAX_DATA_SIZE)
		read_size = MAX_DATA_SIZE;

	pos = fh->fp->f_pos;
	ret = kernel_read(fh->fp, resp->data, read_size, &pos);

	fh->fp->f_pos = pos;
	kref_put(&fh->refcount, rpmsg_file_handle_release);

	if (ret < 0)
		return ret;

	resp->size = ret;
	resp->result = 0;
	return 0;
}

static int handle_write(struct rpmsg_file_io *priv, struct file_io_request *req,
			struct file_io_response *resp)
{
	struct rpmsg_file_handle *fh;
	loff_t pos;
	ssize_t ret;
	u32 write_len;

	fh = find_fd(priv, req->fd);
	if (!fh)
		return -EBADF;

	write_len = req->size;
	if (write_len > MAX_DATA_SIZE)
		write_len = MAX_DATA_SIZE;

	pos = fh->fp->f_pos;
	ret = kernel_write(fh->fp, req->data, write_len, &pos);

	fh->fp->f_pos = pos;
	kref_put(&fh->refcount, rpmsg_file_handle_release);

	if (ret < 0)
		return ret;

	resp->size = ret;
	resp->result = 0;
	return 0;
}

static int handle_close(struct rpmsg_file_io *priv, struct file_io_request *req,
			struct file_io_response *resp)
{
	struct rpmsg_file_handle *fh = NULL;
	struct rpmsg_file_handle *iter;
	int fd = req->fd;
	bool found = false;

	mutex_lock(&priv->lock);
	list_for_each_entry(iter, &priv->files, list) {
		if (iter->fd == fd) {
			fh = iter;
			list_del(&fh->list);
			found = true;
			break;
		}
	}
	mutex_unlock(&priv->lock);

	if (!found)
		return -EBADF;

	close_fd(fd);

	kref_put(&fh->refcount, rpmsg_file_handle_release);

	resp->result = 0;
	return 0;
}

static int handle_fsync(struct rpmsg_file_io *priv, struct file_io_request *req,
			struct file_io_response *resp)
{
	struct rpmsg_file_handle *fh;
	int ret;

	fh = find_fd(priv, req->fd);
	if (!fh)
		return -EBADF;

	ret = vfs_fsync(fh->fp, 0);

	kref_put(&fh->refcount, rpmsg_file_handle_release);

	if (ret < 0)
		return ret;

	resp->result = 0;
	return 0;
}

static int rpmsg_file_io_cb(struct rpmsg_device *rpdev, void *data, int len,
			    void *priv, u32 src)
{
	struct rpmsg_file_io *rfi = dev_get_drvdata(&rpdev->dev);
	struct file_io_request *req = data;
	struct file_io_response resp = { 0 };
	int ret = 0;

	resp.cmd = req->cmd;
	resp.id = req->id;
	resp.result = -ENOTTY;

	switch (req->cmd) {
	case FILE_CMD_OPEN:
		ret = handle_open(rfi, req, &resp);
		break;
	case FILE_CMD_SEEK:
		ret = handle_seek(rfi, req, &resp);
		break;
	case FILE_CMD_READ:
		ret = handle_read(rfi, req, &resp);
		break;
	case FILE_CMD_WRITE:
		ret = handle_write(rfi, req, &resp);
		break;
	case FILE_CMD_CLOSE:
		ret = handle_close(rfi, req, &resp);
		break;
	case FILE_CMD_FSYNC:
		ret = handle_fsync(rfi, req, &resp);
		break;
	default:
		resp.result = -EINVAL;
		break;
	}

	if (ret < 0 && resp.result == -ENOTTY)
		resp.result = ret;

	// response
	if (req->non_response != RPMSG_FILE_NON_RESPONSE)
		rpmsg_send(rpdev->ept, &resp, sizeof(resp));

	return 0;
}

static int rpmsg_file_io_probe(struct rpmsg_device *rpdev)
{
	struct rpmsg_file_io *rfi;
	struct file_io_response resp = { 0 };
	int ret;

	rfi = devm_kzalloc(&rpdev->dev, sizeof(*rfi), GFP_KERNEL);
	if (!rfi)
		return -ENOMEM;

	rfi->rpdev = rpdev;
	INIT_LIST_HEAD(&rfi->files);
	mutex_init(&rfi->lock);

	dev_set_drvdata(&rpdev->dev, rfi);

	rpdev->announce = rpdev->src != RPMSG_ADDR_ANY;

	resp.cmd = RPMSG_FILE_CONFIG_MAGIC_NUM;
	resp.id = rpdev->src;
	ret = rpmsg_sendto(rpdev->ept, &resp, sizeof(resp), rpdev->dst);
	if (ret)
		dev_err(&rpdev->dev, "rpmsg_send failed: %d\n", ret);

	dev_info(&rpdev->dev, "RPMSG File IO driver probed\n");
	return 0;
}

static void rpmsg_file_io_remove(struct rpmsg_device *rpdev)
{
	struct rpmsg_file_io *rfi = dev_get_drvdata(&rpdev->dev);
	struct rpmsg_file_handle *fh, *tmp;

	mutex_lock(&rfi->lock);
	list_for_each_entry_safe(fh, tmp, &rfi->files, list) {
		list_del(&fh->list);
		close_fd(fh->fd);
		kref_put(&fh->refcount, rpmsg_file_handle_release);
	}
	mutex_unlock(&rfi->lock);

	dev_info(&rpdev->dev, "RPMSG File IO driver removed\n");
}

static const struct rpmsg_device_id rpmsg_file_io_id_table[] = {
	{ .name = RPMSG_DEVICE_NAME },
	{},
};
MODULE_DEVICE_TABLE(rpmsg, rpmsg_file_io_id_table);

static struct rpmsg_driver rpmsg_file_io_driver = {
	.drv.name = KBUILD_MODNAME,
	.drv.owner = THIS_MODULE,
	.id_table = rpmsg_file_io_id_table,
	.probe = rpmsg_file_io_probe,
	.callback = rpmsg_file_io_cb,
	.remove = rpmsg_file_io_remove,
};

module_rpmsg_driver(rpmsg_file_io_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RPMSG File I/O Driver");
MODULE_AUTHOR("Jiahang Zheng <jiahang.zheng@rock-chips.com>");
