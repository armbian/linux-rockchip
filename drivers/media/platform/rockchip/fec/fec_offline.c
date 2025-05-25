// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#include <linux/version.h>
#include <linux/rk-video-format.h>

#include "fec_offline.h"
#include "hw.h"
#include "procfs.h"
#include "regs.h"

int rkfec_debug;
module_param_named(debug, rkfec_debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-6)");

static int rkfec_stdfps = 30;
module_param_named(standardfps, rkfec_stdfps, int, 0644);
MODULE_PARM_DESC(standardfps, "standard fps");

static int rkfec_cache_linesize = 2;
module_param_named(cache_linesize, rkfec_cache_linesize, int, 0644);
MODULE_PARM_DESC(cache_linesize, "Cache linesize (0-3)");

static int rkfec_user_debug;
module_param_named(user_debug, rkfec_user_debug, int, 0644);
MODULE_PARM_DESC(user_debug, "Debug level (0-6)");

#if IS_LINUX_VERSION_AT_LEAST_6_1
	#define GET_SG_TABLE(mem_ops, off_buf) mem_ops->cookie(&(off_buf)->vb, (off_buf)->mem)
#else
	#define GET_SG_TABLE(mem_ops, off_buf) mem_ops->cookie((off_buf)->mem)
#endif

static void buf_del(struct file *file, int fd, bool is_all);

static void rkfec_dvfs(struct rkfec_offline_dev *ofl, int width)
{
	int i, ret;
	struct rkfec_hw_dev *hw = ofl->hw;
	const struct fec_clk_info *rate_info = NULL;
	unsigned long target_rate = 0;

	for (i = 0; i < hw->clk_rate_tbl_num; i++) {
		if (width <= hw->clk_rate_tbl[i].refer_data) {
			rate_info = &hw->clk_rate_tbl[i];
			break;
		}
	}

	if (!rate_info)
		rate_info = &hw->clk_rate_tbl[hw->clk_rate_tbl_num - 1];

	target_rate = rate_info->clk_rate * 1000000;

	ret = hw->set_clk(hw->clks[0], target_rate);
	if (ret < 0)
		v4l2_err(&ofl->v4l2_dev, "failed to set aclk rate: %d\n", ret);

	ret = hw->set_clk(hw->clks[2], target_rate);
	if (ret < 0)
		v4l2_err(&ofl->v4l2_dev, "failed to set core clk rate: %d\n", ret);

	v4l2_dbg(4, rkfec_debug, &ofl->v4l2_dev, "set clk rate: %ld\n",
		 target_rate);
}

#if IS_LINUX_VERSION_AT_LEAST_6_1
static void init_vb2(struct rkfec_offline_dev *ofl,
		     struct rkfec_offline_buf *buf)
{
	struct rkfec_hw_dev *hw = ofl->hw;
	unsigned long attrs = DMA_ATTR_NO_KERNEL_MAPPING;

	if (!buf)
		return;
	memset(&buf->vb, 0, sizeof(buf->vb));
	ofl->vb2_queue.gfp_flags = GFP_KERNEL | GFP_DMA32;
	ofl->vb2_queue.dma_dir = DMA_BIDIRECTIONAL;
	if (hw->is_dma_config)
		attrs |= DMA_ATTR_FORCE_CONTIGUOUS;
	ofl->vb2_queue.dma_attrs = attrs;
	buf->vb.vb2_queue = &ofl->vb2_queue;
}
#endif

static int buf_alloc(struct file *file, struct rkfec_buf *info)
{
	struct rkfec_offline_dev *ofl = video_drvdata(file);
	struct rkfec_hw_dev *hw = ofl->hw;
	const struct vb2_mem_ops *ops = hw->mem_ops;
	struct rkfec_offline_buf *buf;
	struct dma_buf *dbuf;
	int fd, size;
	void *mem;

	info->buf_fd = -1;
	size = PAGE_ALIGN(info->size);
	if (!size)
		return -EINVAL;
	buf = kzalloc(sizeof(struct rkfec_offline_buf), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
#if IS_LINUX_VERSION_AT_LEAST_6_1
	init_vb2(ofl, buf);
	mem = ops->alloc(&buf->vb, hw->dev, size);
#else
	mem = ops->alloc(hw->dev, DMA_ATTR_NO_KERNEL_MAPPING, size,
			 DMA_BIDIRECTIONAL, GFP_KERNEL | GFP_DMA32);
#endif
	if (IS_ERR_OR_NULL(mem)) {
		v4l2_err(&ofl->v4l2_dev, "failed to alloc dmabuf\n");
		goto err_alloc;
	}

#if IS_LINUX_VERSION_AT_LEAST_6_1
	dbuf = ops->get_dmabuf(&buf->vb, mem, O_RDWR);
#else
	dbuf = ops->get_dmabuf(mem, O_RDWR);
#endif
	if (IS_ERR_OR_NULL(dbuf)) {
		v4l2_err(&ofl->v4l2_dev, "failed to get dmabuf\n");
		goto err_dmabuf;
	}

	fd = dma_buf_fd(dbuf, O_CLOEXEC);
	if (fd < 0) {
		v4l2_err(&ofl->v4l2_dev, "failed to get dmabuf fd\n");
		goto err_dmabuf_fd;
	}

	get_dma_buf(dbuf);

	info->buf_fd = fd;
	buf->fd = fd;
	buf->file = file;
	buf->dbuf = dbuf;
	buf->mem = mem;
	buf->memory = RKFEC_MEMORY_MMAP;
	ops->prepare(buf->mem);
	mutex_lock(&hw->dev_lock);
	list_add_tail(&buf->list, &ofl->list);
	mutex_unlock(&hw->dev_lock);
	v4l2_dbg(1, rkfec_debug, &ofl->v4l2_dev, "%s file:%p, fd:%d dbuf:%p size %d\n",
		 __func__, file, fd, dbuf, size);
	return 0;

err_dmabuf_fd:
	dma_buf_put(dbuf);
err_dmabuf:
	ops->put(mem);
err_alloc:
	kfree(buf);
	return -ENOMEM;
}

static struct rkfec_offline_buf *buf_add(struct file *file, int fd, int size)
{
	struct rkfec_offline_dev *ofl = video_drvdata(file);
	struct rkfec_hw_dev *hw = ofl->hw;
	const struct vb2_mem_ops *ops = hw->mem_ops;
	struct rkfec_offline_buf *buf = NULL, *next = NULL;
	struct dma_buf *dbuf;
	void *mem = NULL;
	bool need_add = true;

	dbuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dbuf)) {
		v4l2_err(&ofl->v4l2_dev, "invalid dmabuf fd:%d\n", fd);
		return buf;
	}

	if (size && dbuf->size < size) {
		v4l2_err(&ofl->v4l2_dev,
				"input fd:%d size error:%zu < %u\n",
				fd, dbuf->size, size);
		dma_buf_put(dbuf);
		return buf;
	}

	mutex_lock(&hw->dev_lock);
	list_for_each_entry_safe(buf, next, &ofl->list, list) {
		if (buf->fd == fd && buf->dbuf == dbuf) {
			need_add = false;
			break;
		}
	}

	if (need_add) {
		buf = kzalloc(sizeof(struct rkfec_offline_buf), GFP_KERNEL);
		if (!buf)
			goto err_kzalloc;
#if IS_LINUX_VERSION_AT_LEAST_6_1
		init_vb2(ofl, buf);

		mem = ops->attach_dmabuf(&buf->vb, hw->dev, dbuf, dbuf->size);
#else
		mem = ops->attach_dmabuf(hw->dev, dbuf, dbuf->size,
					   DMA_BIDIRECTIONAL);
#endif
		if (IS_ERR(mem)) {
			v4l2_err(&ofl->v4l2_dev, "failed to attach dmabuf, fd:%d\n", fd);
			goto err_attach;
		}
		if (ops->map_dmabuf(mem)) {
			v4l2_err(&ofl->v4l2_dev, "failed to map, fd:%d\n", fd);
			ops->detach_dmabuf(mem);
			goto err_map;
		}
		buf->fd = fd;
		buf->file = file;
		buf->dbuf = dbuf;
		buf->mem = mem;
		buf->memory = RKFEC_MEMORY_DMABUF;
		list_add_tail(&buf->list, &ofl->list);
		v4l2_dbg(1, rkfec_debug, &ofl->v4l2_dev,
				"%s file:%p fd:%d dbuf:%p size:%d\n",
				__func__, file, fd, dbuf, size);
	} else {
		dma_buf_put(dbuf);
	}

	mutex_unlock(&hw->dev_lock);
	return buf;

err_map:
	ops->detach_dmabuf(mem);
err_attach:
	dma_buf_put(dbuf);
	kfree(buf);
err_kzalloc:
	mutex_unlock(&hw->dev_lock);
	return NULL;
}

static void buf_del(struct file *file, int fd, bool is_all)
{
	struct rkfec_offline_dev *ofl = video_drvdata(file);
	struct rkfec_hw_dev *hw = ofl->hw;
	const struct vb2_mem_ops *ops = hw->mem_ops;
	struct rkfec_offline_buf *buf, *next;

	mutex_lock(&hw->dev_lock);
	list_for_each_entry_safe(buf, next, &ofl->list, list) {
		if ((is_all || buf->fd == fd)) {
			v4l2_dbg(1, rkfec_debug, &ofl->v4l2_dev,
					"%s file:%p fd:%d dbuf:%p, memory:%d\n",
					__func__, file, buf->fd, buf->dbuf, buf->memory);
			if (buf->memory == RKFEC_MEMORY_DMABUF) {
				ops->unmap_dmabuf(buf->mem);
				ops->detach_dmabuf(buf->mem);
			} else {
				ops->put(buf->mem);
			}
			dma_buf_put(buf->dbuf);
			buf->file = NULL;
			buf->mem = NULL;
			buf->dbuf = NULL;
			buf->fd = -1;
			list_del(&buf->list);
			kfree(buf);
			if (!is_all)
				break;
		}
	}
	mutex_unlock(&hw->dev_lock);
}

static int fec_running(struct file *file, struct rkfec_in_out *buf)
{
	struct rkfec_offline_dev *ofl = video_drvdata(file);
	struct rkfec_hw_dev *hw = ofl->hw;
	const struct vb2_mem_ops *mem_ops = hw->mem_ops;
	struct sg_table *sg_talbe;
	struct rkfec_offline_buf *off_buf;
	u32 in_fmt, out_fmt;
	u32 rd_mode, wr_mode;
	u32 val;
	u32 in_w = buf->in_width, in_h = buf->in_height;
	u32 out_w = buf->out_width, out_h = buf->out_height;
	/* Calculate virtual width in bytes */
	u32 in_stride, out_stride_y, out_stride_uv;
	/* Calculate uv offset in bytes */
	u32 in_uv_offset, out_uv_offset;
	/* Calculate byte offset based on pixel offset */
	u32 in_y_start = 0, in_uv_start = 0, out_y_start = 0, out_uv_start = 0;
	u32 y_base, c_base;
	void __iomem *base = ofl->hw->base_addr;
	int ret = -EINVAL;
	ktime_t t = 0;
	s64 us = 0;

	t = ktime_get();
	v4l2_dbg(3, rkfec_debug, &ofl->v4l2_dev,
		 "%s enter %dx%d->%dx%d format(in:%c%c%c%c out:%c%c%c%c)\n",
		 __func__, in_w, in_h, out_w, out_h,
		 buf->in_fourcc, buf->in_fourcc >> 8,
		 buf->in_fourcc >> 16, buf->in_fourcc >> 24,
		 buf->out_fourcc, buf->out_fourcc >> 8,
		 buf->out_fourcc >> 16, buf->out_fourcc >> 24);

	v4l2_dbg(3, rkfec_debug, &ofl->v4l2_dev,
		 "in: stride %d, offset %d, out: stride %d, offset %d\n",
		 buf->buf_cfg.in_stride, buf->buf_cfg.in_offs,
		 buf->buf_cfg.out_stride, buf->buf_cfg.out_offs);

	if (hw->fec_ver == RKFEC_V20) {
		if (hw->soft_reset)
			hw->soft_reset(hw);
		else
			dev_warn(hw->dev, "soft_reset not implemented\n");
	}

	if (hw->set_clk)
		rkfec_dvfs(ofl, in_w);

	ofl->prev_frame.fs_seq = ofl->curr_frame.fs_seq;
	ofl->prev_frame.fs_timestamp = ofl->curr_frame.fs_timestamp;
	ofl->curr_frame.fs_seq++;
	ofl->curr_frame.fs_timestamp = ktime_get_ns();

	init_completion(&ofl->cmpl);

	switch (buf->in_fourcc) {
	case V4L2_PIX_FMT_NV12:
		in_fmt = SW_FEC_RD_FMT(1);
		rd_mode = SW_FEC_RD_MODE(0);
		in_stride = ALIGN(buf->buf_cfg.in_stride, 16);
		in_uv_offset = in_stride * in_h;
		in_y_start = buf->buf_cfg.in_offs;
		in_uv_start = in_y_start;
		break;
	case V4L2_PIX_FMT_TILE420:
		in_fmt = SW_FEC_RD_FMT(0);
		rd_mode = SW_FEC_RD_MODE(1);
		in_stride = ALIGN(buf->buf_cfg.in_stride * 6, 16);
		in_uv_offset = in_stride * in_h;
		in_y_start = buf->buf_cfg.in_offs * 6;
		in_uv_start = in_y_start;
		break;
	default:
		v4l2_err(&ofl->v4l2_dev,
			 "no support in format:%c%c%c%c\n",
			 buf->in_fourcc, buf->in_fourcc >> 8,
			 buf->in_fourcc >> 16, buf->in_fourcc >> 24);
		return -EINVAL;
	}

	switch (buf->out_fourcc) {
	case V4L2_PIX_FMT_NV12:
		out_fmt = SW_FEC_WR_FMT(1);
		wr_mode = SW_FEC_WR_MODE(0);
		out_stride_y = ALIGN(buf->buf_cfg.out_stride, 16);
		out_stride_uv = out_stride_y;
		out_uv_offset = out_stride_y * out_h;
		out_y_start = buf->buf_cfg.out_offs;
		out_uv_start = out_y_start;
		break;
	case V4L2_PIX_FMT_TILE420:
		out_fmt = SW_FEC_WR_FMT(0);
		wr_mode = SW_FEC_WR_MODE(1);
		out_stride_y = ALIGN(buf->buf_cfg.out_stride * 6, 16);
		out_stride_uv = out_stride_y;
		out_uv_offset = out_stride_y * out_h;
		out_y_start = buf->buf_cfg.out_offs * 6;
		out_uv_start = out_y_start;
		break;
	case V4L2_PIX_FMT_FBC0:
		out_fmt = SW_FEC_WR_FMT(0);
		wr_mode = SW_FEC_WR_MODE(2);
		out_stride_y = (buf->buf_cfg.out_stride + 63) / 64 * 384;
		out_stride_uv = (buf->buf_cfg.out_stride + 63) / 64 * 16;
		// Head stride is c channel
		out_uv_offset = out_stride_uv * out_h / 4;
		out_y_start = buf->buf_cfg.out_offs / 64 * 384;
		out_uv_start = buf->buf_cfg.out_offs / 64 * 16;
		break;
	case V4L2_PIX_FMT_QUAD:
		out_fmt = SW_FEC_WR_FMT(0);
		wr_mode = SW_FEC_WR_MODE(3);
		out_stride_y = ALIGN(buf->buf_cfg.out_stride * 3, 16);
		out_stride_uv = out_stride_y;
		out_uv_offset = out_stride_y * out_h;

		if (buf->buf_cfg.out_offs > 0) {
			v4l2_err(&ofl->v4l2_dev,
				 "Offset is not supported in %c%c%c%c\n",
				 buf->out_fourcc, buf->out_fourcc >> 8,
				 buf->out_fourcc >> 16, buf->out_fourcc >> 24);
			out_y_start = 0;
			out_uv_start = 0;
		}
		break;
	default:
		v4l2_err(&ofl->v4l2_dev, "no support out format:%c%c%c%c\n",
			 buf->out_fourcc, buf->out_fourcc >> 8,
			 buf->out_fourcc >> 16, buf->out_fourcc >> 24);
		return -EINVAL;
	}

	/* input picture buf */
	off_buf = buf_add(file, buf->buf_cfg.in_pic_fd, buf->buf_cfg.in_size);
	if (!off_buf)
		return -ENOMEM;

	sg_talbe = GET_SG_TABLE(mem_ops, off_buf);
	if (!sg_talbe)
		goto free_buf;
	y_base = sg_dma_address(sg_talbe->sgl);
	c_base = y_base + in_uv_offset;
	writel(y_base + in_y_start, base + RKFEC_RD_Y_BASE);
	writel(c_base + in_uv_start, base + RKFEC_RD_C_BASE);

	/* output picture buf */
	off_buf = buf_add(file, buf->buf_cfg.out_pic_fd, buf->buf_cfg.out_size);
	if (!off_buf)
		goto free_buf;

	sg_talbe = GET_SG_TABLE(mem_ops, off_buf);
	if (!sg_talbe)
		goto free_buf;
	if (buf->out_fourcc == V4L2_PIX_FMT_FBC0) {
		c_base = sg_dma_address(sg_talbe->sgl);
		y_base = c_base + out_uv_offset;

		if (buf->buf_cfg.out_offs > 0)
			writel((out_uv_offset + out_y_start)  << 4,
			       base + RKFEC_WR_FBCE_HEAD_OFFSET);
		else
			writel(out_uv_offset << 4, base + RKFEC_WR_FBCE_HEAD_OFFSET);
	} else {
		y_base = sg_dma_address(sg_talbe->sgl);
		c_base = y_base + out_uv_offset;
	}
	writel(y_base + out_y_start, base + RKFEC_WR_Y_BASE);
	writel(c_base + out_uv_start, base + RKFEC_WR_C_BASE);

	/* lut buf */
	off_buf = buf_add(file, buf->buf_cfg.lut_fd, buf->buf_cfg.lut_size);
	if (!off_buf)
		goto free_buf;

	sg_talbe = GET_SG_TABLE(mem_ops, off_buf);
	if (!sg_talbe)
		goto free_buf;
	val = sg_dma_address(sg_talbe->sgl);
	writel(val, base + RKFEC_LUT_BASE);

	//fmt
	val = in_fmt | out_fmt | rd_mode | wr_mode;
	writel(val, base + RKFEC_CTRL);

	//stride
	val = FEC_RD_VIR_STRIDE_Y(in_stride / 4) | FEC_RD_VIR_STRIDE_C(in_stride / 4);
	writel(val, base + RKFEC_RD_VIR_STRIDE);
	val = FEC_WR_VIR_STRIDE_Y(out_stride_y / 4) | FEC_WR_VIR_STRIDE_C(out_stride_uv / 4);
	writel(val, base + RKFEC_WR_VIR_STRIDE);
	//with height lut_size
	val = SW_FEC_SRC_WIDTH(buf->in_width) | Sw_FEC_SRC_HEIGHT(buf->in_height);
	writel(val, base + RKFEC_SRC_SIZE);
	val = SW_FEC_DST_WIDTH(buf->out_width) | Sw_FEC_DST_HEIGHT(buf->out_height);
	writel(val, base + RKFEC_DST_SIZE);
	val = SW_LUT_SIZE(buf->buf_cfg.lut_size);
	writel(val, base + RKFEC_LUT_SIZE);

	//new  bg val
	val = SW_BG_Y_VALUE(buf->bg_val.bg_y) |
	      SW_BG_U_VALUE(buf->bg_val.bg_u) |
	      SW_BG_V_VALUE(buf->bg_val.bg_v);
	writel(val, base + RKFEC_BG_VALUE);

	//core_ctrl
	val = SW_FEC_BIC_MODE(buf->core_ctrl.bic_mode) |
	      SW_LUT_DENSITY(buf->core_ctrl.density) |
	      SW_FEC_BORDER_MODE(buf->core_ctrl.border_mode) |
	      SW_FEC_PBUF_CRS_DIS(buf->core_ctrl.pbuf_crs_dis) |
	      SW_FEC_CRS_BUF_MODE(buf->core_ctrl.buf_mode) |
	      SYS_FEC_ST;
	writel(val, base + RKFEC_CORE_CTRL);

	writel(0, base + RKFEC_CLK_DIS);

	// cache
	writel(0x1c, base + RKFEC_CACHE_MAX_READS);
	val = SW_CACHE_LINESIZE(rkfec_cache_linesize) | 0x7;
	writel(val, base + RKFEC_CACHE_CTRL);

	//update
	writel(SYS_FEC_FORCE_UPD, base + RKFEC_UPD);

	//start
	if (!hw->is_shutdown)
		writel(SYS_FEC_ST, base + RKFEC_STRT);

	ofl->state = RKFEC_START;

	// add info for procfs
	ofl->in_fmt.width = in_w;
	ofl->in_fmt.height = in_h;
	ofl->in_fmt.pixelformat = buf->in_fourcc;
	ofl->in_fmt.bytesperline = in_stride;
	ofl->in_fmt.sizeimage = buf->buf_cfg.in_size;
	ofl->in_fmt.offset = buf->buf_cfg.in_offs;

	ofl->out_fmt.width = out_w;
	ofl->out_fmt.height = out_h;
	ofl->out_fmt.pixelformat = buf->out_fourcc;
	ofl->out_fmt.bytesperline = out_stride_y;
	ofl->out_fmt.sizeimage = buf->buf_cfg.out_size;
	ofl->out_fmt.offset = buf->buf_cfg.out_offs;

	ret = wait_for_completion_timeout(&ofl->cmpl, msecs_to_jiffies(300));
	if (!ret) {
		v4l2_err(&ofl->v4l2_dev, "fec working timeout\n");
		ret = -EAGAIN;
	} else {
		ret = 0;
	}

	us = ktime_us_delta(ktime_get(), t);
	v4l2_dbg(3, rkfec_debug, &ofl->v4l2_dev,
		 "%s exit ret:%d, time:%lldus\n", __func__, ret, us);

	if (rkfec_debug >= 4) {
		pr_cont("FEC_0x200:\n");
		print_hex_dump(KERN_CONT, "", DUMP_PREFIX_OFFSET, 16, 4,
			       base + RKFEC_STRT, 0xc0, false);

		pr_cont("FEC_CACHE:\n");
		print_hex_dump(KERN_CONT, "", DUMP_PREFIX_OFFSET, 16, 4,
			       base + RKFEC_CACHE_STATUS, 0x28, false);

		pr_cont("FEC_MMU:\n");
		print_hex_dump(KERN_CONT, "", DUMP_PREFIX_OFFSET, 16, 4,
			       base + RKFEC_MMU_DTE_ADDR, 0x2c, false);
	}

	ofl->debug.interval = us;
	if (ofl->debug.interval * rkfec_stdfps > USEC_PER_SEC)
		ofl->debug.frame_timeout_cnt++;

	ofl->state = RKFEC_FRAME_END;
	if (!ret) {
		if (ofl->curr_frame.fe_seq > ofl->prev_frame.fe_seq &&
		    ofl->curr_frame.fe_seq - ofl->prev_frame.fe_seq > 1)
			ofl->debug.frameloss += ofl->curr_frame.fe_seq - ofl->prev_frame.fe_seq - 1;

		ofl->prev_frame.fe_seq = ofl->curr_frame.fe_seq;
		ofl->prev_frame.fe_timestamp = ofl->curr_frame.fe_timestamp;
		ofl->curr_frame.fe_seq++;
		ofl->curr_frame.fe_timestamp = ktime_get_ns();
	}

	return ret;

free_buf:
	v4l2_dbg(3, rkfec_debug, &ofl->v4l2_dev,
		 "%s sg_talbe error\n", __func__);
	buf_del(file, 0, true);
	return ret;
}

static long rkfec_ofl_ioctl(struct file *file, void *fh,
		bool valid_prio,
		unsigned int cmd, void *arg)
{
	struct rkfec_offline_dev *ofl = video_drvdata(file);
	struct rkfec_offline_buf *buf = NULL;
	long ret = 0;

	ofl->pm_need_wait = true;

	v4l2_dbg(4, rkfec_debug, &ofl->v4l2_dev, "%s cmd:%d", __func__, cmd);

	if (mutex_lock_interruptible(&ofl->ioctl_lock)) {
		return -ERESTARTSYS;
	}

	if (!arg) {
		ret =  -EINVAL;
		goto out;
	}

	switch (cmd) {
	case RKFEC_CMD_IN_OUT:
		ret = fec_running(file, arg);
		break;
	case RKFEC_CMD_BUF_ALLOC:
		buf_alloc(file, arg);
		break;
	case RKFEC_CMD_BUF_ADD:
		buf = buf_add(file, *(int *)arg, 0);
		if (!buf)
			ret = -ENOMEM;
		break;
	case RKFEC_CMD_BUF_DEL:
		buf_del(file, *(int *)arg, false);
		break;
	default:
		ret = -EFAULT;
	}

out:
	/* notify hw suspend */
	if (ofl->hw->is_suspend)
		complete(&ofl->pm_cmpl);

	ofl->pm_need_wait = false;
	mutex_unlock(&ofl->ioctl_lock);
	return ret;
}

static const struct v4l2_ioctl_ops offline_ioctl_ops = {
	.vidioc_default = rkfec_ofl_ioctl,
};

static int ofl_open(struct file *file)
{
	struct rkfec_offline_dev *ofl = video_drvdata(file);
	int ret;

	ret = v4l2_fh_open(file);
	if (ret)
		goto end;

	mutex_lock(&ofl->hw->dev_lock);
	ret = pm_runtime_get_sync(ofl->hw->dev);
	mutex_unlock(&ofl->hw->dev_lock);
	if (ret < 0)
		v4l2_fh_release(file);
end:
	v4l2_dbg(1, rkfec_debug, &ofl->v4l2_dev, "%s ret:%d\n", __func__, ret);
	return (ret > 0) ? 0 : ret;
}

static int ofl_release(struct file *file)
{
	struct rkfec_offline_dev *ofl = video_drvdata(file);
	int ret;

	v4l2_dbg(1, rkfec_debug, &ofl->v4l2_dev, "%s\n", __func__);

	ret = v4l2_fh_release(file);
	if (!ret) {
		buf_del(file, 0, true);
		mutex_lock(&ofl->hw->dev_lock);
		pm_runtime_put_sync(ofl->hw->dev);
		mutex_unlock(&ofl->hw->dev_lock);
	}
	return 0;
}

static const struct v4l2_file_operations offline_fops = {
	.owner = THIS_MODULE,
	.open = ofl_open,
	.release = ofl_release,
	.unlocked_ioctl = video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = video_ioctl2,
#endif
};

static const struct video_device offline_videodev = {
	.name = "rkfec_offline",
	.vfl_dir = VFL_DIR_RX,
	.fops = &offline_fops,
	.ioctl_ops = &offline_ioctl_ops,
	.minor = -1,
	.release = video_device_release_empty,
};

void rkfec_offline_irq(struct rkfec_hw_dev *hw, u32 irq)
{
	struct rkfec_offline_dev *ofl = &hw->ofl_dev;

	v4l2_dbg(3, rkfec_debug, &ofl->v4l2_dev, "%s 0x%x\n", __func__, irq);

	if (!completion_done(&ofl->cmpl))
		complete(&ofl->cmpl);
}

int rkfec_register_offline(struct rkfec_hw_dev *hw)
{
	struct rkfec_offline_dev *ofl = &hw->ofl_dev;
	struct v4l2_device *v4l2_dev;
	struct video_device *vfd;
	int ret;

	ofl->hw = hw;
	v4l2_dev = &ofl->v4l2_dev;
	strscpy(v4l2_dev->name, offline_videodev.name, sizeof(v4l2_dev->name));
	ret = v4l2_device_register(hw->dev, v4l2_dev);
	if (ret)
		return ret;

	mutex_init(&ofl->ioctl_lock);
	ofl->vfd = offline_videodev;
	vfd = &ofl->vfd;
	vfd->device_caps = V4L2_CAP_STREAMING;
	vfd->v4l2_dev = v4l2_dev;
	ret = video_register_device(vfd, VFL_TYPE_VIDEO, 0);
	if (ret) {
		v4l2_err(v4l2_dev, "Failed to register video device\n");
		goto unreg_v4l2;
	}
	video_set_drvdata(vfd, ofl);
	INIT_LIST_HEAD(&ofl->list);
	rkfec_offline_proc_init(ofl);
	ofl->state = RKFEC_STOP;
	//todo
	init_completion(&ofl->pm_cmpl);

	memset(&ofl->vb2_queue, 0, sizeof(ofl->vb2_queue));
	memset(&ofl->curr_frame, 0, sizeof(ofl->curr_frame));
	memset(&ofl->prev_frame, 0, sizeof(ofl->prev_frame));

	v4l2_info(&ofl->v4l2_dev, "%s success\n", __func__);
	return 0;
unreg_v4l2:
	mutex_destroy(&ofl->ioctl_lock);
	v4l2_device_unregister(v4l2_dev);
	return ret;
}

void rkfec_unregister_offline(struct rkfec_hw_dev *hw)
{
	struct rkfec_offline_dev *ofl = &hw->ofl_dev;

	rkfec_offline_proc_cleanup(&hw->ofl_dev);
	mutex_destroy(&ofl->ioctl_lock);
	video_unregister_device(&ofl->vfd);
	v4l2_device_unregister(&ofl->v4l2_dev);
}

MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(DMA_BUF);
