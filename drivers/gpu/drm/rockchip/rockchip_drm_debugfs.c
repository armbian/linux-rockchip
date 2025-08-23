// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 * Author: Sandy Huang <hjc@rock-chips.com>
 */

#include <drm/drm_atomic_uapi.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>

#include <linux/file.h>

#include "rockchip_drm_drv.h"
#include "rockchip_drm_debugfs.h"
#include "rockchip_drm_fb.h"

#if defined(CONFIG_NO_GKI)
#define DUMP_BUF_PATH		"/data"

#define to_rockchip_crtc(x) container_of(x, struct rockchip_crtc, crtc)

/**
 * struct vop_dump_info - vop dump plane info structure
 *
 * Store plane info used to write display data to /data/vop_buf/
 *
 */
struct vop_dump_info {
	/* @win_name human readable vop win name */
	const char *win_name;
	/* @fb: DRM frame buffer */
	struct drm_framebuffer *fb;
	/* @src: source coordinates of the plane (in 16.16)*/
	struct drm_rect *src;
};

static int temp_pow(int sum, int n)
{
	int i;
	int temp = sum;

	if (n < 1)
		return 1;
	for (i = 1; i < n ; i++)
		sum *= temp;
	return sum;
}

static int rockchip_drm_dump_plane_buffer(struct vop_dump_info *dump_info, int frame_count)
{
	struct iosys_map map[DRM_FORMAT_MAX_PLANES];
	struct iosys_map data[DRM_FORMAT_MAX_PLANES];
	struct drm_framebuffer *fb = dump_info->fb;
	int ret;
	int flags;
	const char *ptr;
	char file_name[128];
	char format_name[5];
	void *kvaddr;
	struct file *file;
	loff_t pos = 0;
	struct drm_gem_object *obj = dump_info->fb->obj[0];
	struct rockchip_gem_object *rk_obj = to_rockchip_obj(obj);

	snprintf(file_name, sizeof(file_name), "%p4cc", &dump_info->fb->format->format);
	strscpy(format_name, file_name, 5);

	flags = O_RDWR | O_CREAT;
	snprintf(file_name, 100, "%s/%s_fb-%dx%d_stride-%d_offset-%dx%d_act-%dx%d_%s%s_%d.bin",
		 DUMP_BUF_PATH, dump_info->win_name, dump_info->fb->width, dump_info->fb->height,
		 dump_info->fb->pitches[0], dump_info->src->x1 >> 16, dump_info->src->y1 >> 16,
		 drm_rect_width(dump_info->src) >> 16, drm_rect_height(dump_info->src) >> 16,
		 format_name, rockchip_drm_modifier_to_string(dump_info->fb->modifier), frame_count);

	ret = drm_gem_fb_begin_cpu_access(fb, DMA_FROM_DEVICE);
	if (ret)
		return ret;

	ret = drm_gem_fb_vmap(fb, map, data);
	if (ret) {
		DRM_ERROR("Failed to vmap() buffer\n");
		goto out_drm_gem_fb_end_cpu_access;
	}

	kvaddr = data[0].vaddr;
	ptr = file_name;
	file = filp_open(ptr, flags, 0644);
	if (!IS_ERR(file)) {
		kernel_write(file, kvaddr, rk_obj->size, &pos);
		DRM_INFO("dump file name is:%s\n", file_name);
		fput(file);
	} else {
		DRM_INFO("open %s failed\n", ptr);
	}

	drm_gem_fb_vunmap(fb, map);
out_drm_gem_fb_end_cpu_access:
	drm_gem_fb_end_cpu_access(fb, DMA_FROM_DEVICE);

	return 0;
}

int rockchip_drm_crtc_dump_plane_buffer(struct drm_crtc *crtc)
{
	struct rockchip_crtc *rockchip_crtc = container_of(crtc, struct rockchip_crtc, crtc);
	struct drm_plane *plane;
	struct drm_plane_state *pstate;
	struct drm_framebuffer *fb;
	struct vop_dump_info dump_info;

	drm_atomic_crtc_for_each_plane(plane, crtc) {
		pstate = plane->state;
		fb = pstate->fb;
		if (!fb)
			continue;

		dump_info.win_name = plane->name;
		dump_info.fb = fb;
		dump_info.src = &pstate->src;

		rockchip_drm_dump_plane_buffer(&dump_info, rockchip_crtc->vop_dump_frame_count);
	}
	rockchip_crtc->vop_dump_frame_count++;

	return 0;
}

static int rockchip_drm_dump_buffer_show(struct seq_file *m, void *data)
{
	seq_puts(m, "VOP dump buffer version: v2.0.0\n");
	seq_puts(m, "  echo dump    > Immediately dump the current frame\n");
	seq_puts(m, "  echo dumpon  > dump to start vop keep dumping\n");
	seq_puts(m, "  echo dumpoff > Disable dump feature and stop keep dumping\n");
	seq_puts(m, "  echo dumpn   > dump n is the number of dump times\n");
	seq_puts(m, "  dump path is /data\n");

	return 0;
}

static int rockchip_drm_dump_buffer_open(struct inode *inode, struct file *file)
{
	struct drm_crtc *crtc = inode->i_private;

	return single_open(file, rockchip_drm_dump_buffer_show, crtc);
}

static ssize_t
rockchip_drm_dump_buffer_write(struct file *file, const char __user *ubuf,
			       size_t len, loff_t *offp)
{
	struct seq_file *m = file->private_data;
	struct drm_crtc *crtc = m->private;
	char buf[14] = {};
	int dump_times = 0;
	int i = 0;
	struct rockchip_crtc *rockchip_crtc = to_rockchip_crtc(crtc);

	if (len > sizeof(buf) - 1)
		return -EINVAL;
	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;
	buf[len - 1] = '\0';
	if (strncmp(buf, "dumpon", 6) == 0) {
		rockchip_crtc->vop_dump_status = DUMP_KEEP;
		DRM_INFO("keep dumping\n");
	} else if (strncmp(buf, "dumpoff", 7) == 0) {
		rockchip_crtc->vop_dump_status = DUMP_DISABLE;
		DRM_INFO("close keep dumping\n");
	} else if (strncmp(buf, "dump", 4) == 0) {
		if (isdigit(buf[4])) {
			for (i = 4; i < strlen(buf); i++) {
				dump_times += temp_pow(10, (strlen(buf)
						       - i - 1))
						       * (buf[i] - '0');
		}
			rockchip_crtc->vop_dump_times = dump_times;
		} else {
			drm_modeset_lock_all(crtc->dev);
			rockchip_drm_crtc_dump_plane_buffer(crtc);
			drm_modeset_unlock_all(crtc->dev);
		}
	} else {
		return -EINVAL;
	}

	return len;
}

static const struct file_operations rockchip_drm_dump_buffer_fops = {
	.owner = THIS_MODULE,
	.open = rockchip_drm_dump_buffer_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = rockchip_drm_dump_buffer_write,
};

int rockchip_drm_add_dump_buffer(struct drm_crtc *crtc, struct dentry *root)
{
	struct dentry *vop_dump_root;
	struct dentry *ent;
	struct rockchip_crtc *rockchip_crtc = to_rockchip_crtc(crtc);

	vop_dump_root = debugfs_create_dir("vop_dump", root);
	rockchip_crtc->vop_dump_status = DUMP_DISABLE;
	rockchip_crtc->vop_dump_times = 0;
	rockchip_crtc->vop_dump_frame_count = 0;
	ent = debugfs_create_file("dump", 0644, vop_dump_root,
				  crtc, &rockchip_drm_dump_buffer_fops);
	if (!ent) {
		DRM_ERROR("create vop_plane_dump err\n");
		debugfs_remove_recursive(vop_dump_root);
	}

	return 0;
}
#endif

static int rockchip_drm_debugfs_color_bar_show(struct seq_file *s, void *data)
{
	seq_puts(s, "  Enable horizontal color bar:\n");
	seq_puts(s, "      echo 1 > /sys/kernel/debug/dri/0/video_port0/color_bar\n");
	seq_puts(s, "  Enable vertical color bar:\n");
	seq_puts(s, "      echo 2 > /sys/kernel/debug/dri/0/video_port0/color_bar\n");
	seq_puts(s, "  Enable horizontal color gradient:\n");
	seq_puts(s, "      echo 3 > /sys/kernel/debug/dri/0/video_port0/color_bar\n");
	seq_puts(s, "  Enable vertical color gradient:\n");
	seq_puts(s, "      echo 4 > /sys/kernel/debug/dri/0/video_port0/color_bar\n");
	seq_puts(s, "  Enable mutant color:\n");
	seq_puts(s, "      echo 5 > /sys/kernel/debug/dri/0/video_port0/color_bar\n");
	seq_puts(s, "  Enable fix black color:\n");
	seq_puts(s, "      echo 6 > /sys/kernel/debug/dri/0/video_port0/color_bar\n");
	seq_puts(s, "  Enable fix white color:\n");
	seq_puts(s, "      echo 7 > /sys/kernel/debug/dri/0/video_port0/color_bar\n");
	seq_puts(s, "  Disable color bar:\n");
	seq_puts(s, "      echo 0 > /sys/kernel/debug/dri/0/video_port0/color_bar\n");

	return 0;
}

static int rockchip_drm_debugfs_color_bar_open(struct inode *inode, struct file *file)
{
	struct drm_crtc *crtc = inode->i_private;

	return single_open(file, rockchip_drm_debugfs_color_bar_show, crtc);
}

static ssize_t rockchip_drm_debugfs_color_bar_write(struct file *file, const char __user *ubuf,
						    size_t len, loff_t *offp)
{
	struct seq_file *s = file->private_data;
	struct drm_crtc *crtc = s->private;
	struct rockchip_drm_private *priv = crtc->dev->dev_private;
	int pipe = drm_crtc_index(crtc);
	u8 mode;

	if (len != 2) {
		DRM_INFO("Unsupported color bar mode\n");
		return -EINVAL;
	}

	if (kstrtou8_from_user(ubuf, len, 0, &mode))
		return -EFAULT;

	if (priv->crtc_funcs[pipe]->crtc_set_color_bar) {
		if (priv->crtc_funcs[pipe]->crtc_set_color_bar(crtc, mode))
			return -EINVAL;
	}

	return len;
}

static const struct file_operations rockchip_drm_debugfs_color_bar_fops = {
	.owner = THIS_MODULE,
	.open = rockchip_drm_debugfs_color_bar_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = rockchip_drm_debugfs_color_bar_write,
};

int rockchip_drm_debugfs_add_color_bar(struct drm_crtc *crtc, struct dentry *root)
{
	struct dentry *ent;

	ent = debugfs_create_file("color_bar", 0644, root, crtc,
				  &rockchip_drm_debugfs_color_bar_fops);
	if (!ent)
		DRM_ERROR("Failed to add color_bar for debugfs\n");

	return 0;
}

static int rockchip_drm_debugfs_regs_write_show(struct seq_file *s, void *data)
{
	seq_puts(s, "  Write VOP regs:\n");
	seq_puts(s, "    echo address val > /sys/kernel/debug/dri/0/video_portx/regs_write\n\n");
	seq_puts(s, "    video_portx is depend on hardware config, you can get this info from the cmd:\n");
	seq_puts(s, "    cat /sys/kernel/debug/dri/0/summary\n\n");
	seq_puts(s, "    Example:\n");
	seq_puts(s, "    echo 0x27d00000 0x1 > /sys/kernel/debug/dri/0/video_portx/regs_write\n\n");

	return 0;
}

static int rockchip_drm_debugfs_regs_write_open(struct inode *inode, struct file *file)
{
	struct drm_crtc *crtc = inode->i_private;

	return single_open(file, rockchip_drm_debugfs_regs_write_show, crtc);
}

static ssize_t rockchip_drm_debugfs_regs_write(struct file *file, const char __user *ubuf,
					       size_t len, loff_t *offp)
{
	struct seq_file *s = file->private_data;
	struct drm_crtc *crtc = s->private;
	struct rockchip_drm_private *priv = crtc->dev->dev_private;
	int ret = 0, pipe = drm_crtc_index(crtc);
	unsigned long address = 0;
	u32 val = 0;
	char kbuf[32];

	len = min_t(size_t, len, (sizeof(kbuf) - 1));
	if (copy_from_user(kbuf, ubuf, len))
		return -EINVAL;

	kbuf[len] = 0;
	if (sscanf(kbuf, "%lx %x", &address, &val) == -1)
		return -EFAULT;

	if (priv->crtc_funcs[pipe]->regs_write)
		ret = priv->crtc_funcs[pipe]->regs_write(crtc, address, val);
	if (ret)
		return ret;

	return len;
}

static const struct file_operations rockchip_drm_debugfs_regs_write_ops = {
	.owner = THIS_MODULE,
	.open = rockchip_drm_debugfs_regs_write_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = rockchip_drm_debugfs_regs_write,
};

int rockchip_drm_debugfs_add_regs_write(struct drm_crtc *crtc, struct dentry *root)
{
	struct dentry *ent;

	ent = debugfs_create_file("regs_write", 0644, root, crtc,
				  &rockchip_drm_debugfs_regs_write_ops);
	if (!ent)
		DRM_ERROR("Failed to add regs_write for debugfs\n");

	return 0;
}

static int rockchip_drm_debugfs_dclk_rate_show(struct seq_file *s, void *data)
{
	struct drm_crtc *crtc = s->private;
	struct rockchip_drm_private *priv = crtc->dev->dev_private;
	int pipe = drm_crtc_index(crtc);
	unsigned long rate;

	if (!priv->crtc_funcs[pipe]->crtc_get_dclk_rate) {
		seq_puts(s, "Not support get rate\n");
		return 0;
	}

	rate = priv->crtc_funcs[pipe]->crtc_get_dclk_rate(crtc);

	seq_printf(s, "%lu\n", rate);
	return 0;
}

static int rockchip_drm_debugfs_dclk_rate_open(struct inode *inode, struct file *file)
{
	struct drm_crtc *crtc = inode->i_private;

	return single_open(file, rockchip_drm_debugfs_dclk_rate_show, crtc);
}

static const struct file_operations rockchip_drm_debugfs_dclk_rate_ops = {
	.owner = THIS_MODULE,
	.open = rockchip_drm_debugfs_dclk_rate_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int rockchip_drm_debugfs_add_dclk_rate(struct drm_crtc *crtc, struct dentry *root)
{
	struct dentry *ent;

	ent = debugfs_create_file("calculated_dclk_rate", 0644, root, crtc,
				  &rockchip_drm_debugfs_dclk_rate_ops);
	if (!ent)
		DRM_ERROR("Failed to add dclk_rate for debugfs\n");

	return 0;
}

static int rockchip_drm_debugfs_dovi_mode_show(struct seq_file *s, void *data)
{
	seq_puts(s, "  Mode0: normal mode\n");
	seq_puts(s, "      echo 0 > /sys/kernel/debug/dri/0/video_port0/dovi_mode\n");
	seq_puts(s, "  Mode1: disable aclk reset when enter dovi\n");
	seq_puts(s, "      echo 1 > /sys/kernel/debug/dri/0/video_port0/dovi_mode\n");

	return 0;
}

static int rockchip_drm_debugfs_dovi_mode_open(struct inode *inode, struct file *file)
{
	struct drm_crtc *crtc = inode->i_private;

	return single_open(file, rockchip_drm_debugfs_dovi_mode_show, crtc);
}

static ssize_t rockchip_drm_debugfs_dovi_mode_write(struct file *file, const char __user *ubuf,
						    size_t len, loff_t *offp)
{
	struct seq_file *s = file->private_data;
	struct drm_crtc *crtc = s->private;
	struct rockchip_drm_private *private = crtc->dev->dev_private;
	u8 mode;

	if (len != 2) {
		DRM_INFO("Unsupported dovi mode\n");
		return -EINVAL;
	}

	if (kstrtou8_from_user(ubuf, len, 0, &mode))
		return -EFAULT;

	private->dovi_mode = mode;

	return len;
}

static const struct file_operations rockchip_drm_debugfs_dovi_mode_fops = {
	.owner = THIS_MODULE,
	.open = rockchip_drm_debugfs_dovi_mode_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = rockchip_drm_debugfs_dovi_mode_write,
};

int rockchip_drm_debugfs_add_dovi_mode(struct drm_crtc *crtc, struct dentry *root)
{
	struct dentry *ent;

	ent = debugfs_create_file("dovi_mode", 0644, root, crtc,
				  &rockchip_drm_debugfs_dovi_mode_fops);
	if (!ent)
		DRM_ERROR("Failed to add dovi_mode for debugfs\n");

	return 0;
}
