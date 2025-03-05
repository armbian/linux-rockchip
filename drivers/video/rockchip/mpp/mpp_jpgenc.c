// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 * author:
 *	Johnson.Ding, johnson.ding@rock-chips.com
 *
 */

#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/regmap.h>
#include <linux/proc_fs.h>
#include <linux/dev_printk.h>

#include "mpp_debug.h"
#include "mpp_common.h"
#include "mpp_iommu.h"

#define JPGENC_DRIVER_NAME		"mpp_jpgenc"

#define JPGENC_SESSION_MAX_BUFFERS	40
#define JPGENC_REG_NUM			79

#define JPGENC_REG_HW_ID_INDEX		0
#define JPGENC_REG_START_INDEX		0
#define JPGENC_REG_END_INDEX		78
#define JPGENC_REG_START_EN_BASE	0x4
#define JPGENC_REG_START_EN_INDEX	1
#define JPGENC_REG_INT_EN_BASE		0x10
#define JPGENC_REG_INT_EN_INDEX		4
#define JPGENC_REG_INT_MASK_BASE	0x14
#define JPGENC_REG_INT_MASK_INDEX	5
#define JPGENC_REG_INT_CLR_BASE		0x18
#define JPGENC_REG_INT_CLR_INDEX	6
#define JPGENC_REG_INT_STATUS_BASE	0x1c
#define JPGENC_REG_INT_STATUS_INDEX	7
#define JPGENC_REG_PERF_WORKING_CNT	0xe4

#define JPGENC_REG_ENC_RSL_INDEX	(29)
#define JPGENC_GET_WIDTH(x)		((((x) & 0x1fff) + 1) << 3)
#define JPGENC_GET_HEIGHT(x)		(((((x) >> 16) & 0x1fff) + 1) << 3)

#define JPGENC_START_EN			BIT(8)
#define JPGENC_INT_ST_ENC_DONE		BIT(0)

#define to_jpgenc_task(task)	\
		container_of(task, struct jpgenc_task, mpp_task)
#define to_jpgenc_dev(dev)	\
		container_of(dev, struct jpgenc_dev, mpp)

enum VEPU_CMD {
	VEPU_CMD_NONE = 0,
	VEPU_CMD_ONE_FRAME,
	VEPU_CMD_MULTI_FRAME_START,
	VEPU_CMD_MULTI_FRAME_UPDATE,
	VEPU_CMD_LINK_TABLE_FORCE_PAUSE,
	VEPU_CMD_LINK_TABLE_RESUME,
	VEPU_CMD_SAFE_CLR,
	VEPU_CMD_FORCE_CLR,
};

enum JPGENC_MODE {
	JPGENC_MODE_ONEFRAME		= 0,
	JPGENC_MODE_LINK_ADD		= 1,
	JPGENC_MODE_LINK_ONEFRAME	= 2,
	JPGENC_MODE_BUTT,
};

struct jpgenc_reg_msg {
	u32 base_s;
	u32 base_e;
	/* class base for link */
	u32 link_s;
	/* class end for link */
	u32 link_e;
	/* class bytes for link */
	u32 link_len;
};

struct jpgenc_task {
	struct mpp_task mpp_task;
	enum MPP_CLOCK_MODE clk_mode;
	u32 reg[JPGENC_REG_NUM];

	struct reg_offset_info off_inf;
	u32 irq_status;
	/* req for current task */
	u32 w_req_cnt;
	struct mpp_request w_reqs[MPP_MAX_MSG_NUM];
	u32 r_req_cnt;
	struct mpp_request r_reqs[MPP_MAX_MSG_NUM];
	/* image info */
	u32 width;
	u32 height;
	u32 pixels;
	struct mpp_dma_buffer *bs_buf;
	u32 offset_bs;
};

struct jpgenc_dev {
	struct mpp_dev mpp;

	struct mpp_clk_info aclk_info;
	struct mpp_clk_info hclk_info;
	struct mpp_clk_info core_clk_info;
#ifdef CONFIG_ROCKCHIP_MPP_PROC_FS
	struct proc_dir_entry *procfs;
#endif
	struct reset_control *rst_a;
	struct reset_control *rst_h;

	// /* for link mode */
	enum JPGENC_MODE link_mode;
	// atomic_t link_task_cnt;
	// u32 link_run;
	// u32 jpeg_wr_addr;
};

static struct mpp_hw_info jpgenc_v1_hw_info = {
	.reg_num = JPGENC_REG_NUM,
	.reg_id = JPGENC_REG_HW_ID_INDEX,
	.reg_start = JPGENC_REG_START_INDEX,
	.reg_end = JPGENC_REG_END_INDEX,
	.reg_en = JPGENC_REG_START_EN_INDEX,
};

static const u16 trans_tbl_jpgenc[] = {
	9,	// link table base
	16,	// address of JPEG Q table
	17,	// top address of JPEG bitstream
	18,	// bottom address of JPEG bitstream
	19,	// read address of JPEG bitstream
	20,	// start address of JPEG bitstream
	21,	// base address of ECS length buffer
	22,	// base address of the 1st storage area for video source buffer
	23,	// base address of the 2nd storage area for video source buffer
	24	// base address of the 3rd storage area for video source buffer
};

#define JPGENC_FMT_DEFAULT 0
static struct mpp_trans_info jpgenc_v1_trans[] = {
	[JPGENC_FMT_DEFAULT] = {
		.count = ARRAY_SIZE(trans_tbl_jpgenc),
		.table = trans_tbl_jpgenc,
	},
};

static int jpgenc_process_reg_fd(struct mpp_session *session, struct jpgenc_task *task)
{
	int ret = 0;

	ret = mpp_translate_reg_address(session, &task->mpp_task,
					JPGENC_FMT_DEFAULT, task->reg, &task->off_inf);

	if (ret)
		return ret;

	mpp_translate_reg_offset_info(&task->mpp_task, &task->off_inf, task->reg);
	return 0;
}

static int jpgenc_extract_task_msg(struct jpgenc_task *task, struct mpp_task_msgs *msgs)
{
	u32 i;
	int ret;
	struct mpp_request *req;
	struct mpp_hw_info *hw_info = task->mpp_task.hw_info;

	for (i = 0; i < msgs->req_cnt; i++) {
		u32 off_s, off_e;

		req = &msgs->reqs[i];
		if (!req->size)
			continue;

		switch (req->cmd) {
		case MPP_CMD_SET_REG_WRITE: {
			off_s = hw_info->reg_start * sizeof(u32);
			off_e = hw_info->reg_end * sizeof(u32);
			ret = mpp_check_req(req, 0, sizeof(task->reg), off_s, off_e);

			if (ret)
				continue;

			if (copy_from_user((u8 *)task->reg + req->offset, req->data, req->size)) {
				mpp_err("copy_from_user reg failed\n");
				return -EIO;
			}

			memcpy(&task->w_reqs[task->w_req_cnt++], req, sizeof(*req));
		} break;
		case MPP_CMD_SET_REG_READ: {
			off_s = hw_info->reg_start * sizeof(u32);
			off_e = hw_info->reg_end * sizeof(u32);
			ret = mpp_check_req(req, 0, sizeof(task->reg), off_s, off_e);

			if (ret)
				continue;

			memcpy(&task->r_reqs[task->r_req_cnt++], req, sizeof(*req));
		} break;
		case MPP_CMD_SET_REG_ADDR_OFFSET: {
			mpp_extract_reg_offset_info(&task->off_inf, req);
		} break;
		default:
			break;
		}
	}
	mpp_debug(DEBUG_TASK_INFO, "w_req_cnt %d, r_req_cnt %d\n",
		  task->w_req_cnt, task->r_req_cnt);

	return 0;
}

static void *jpgenc_alloc_task(struct mpp_session *session, struct mpp_task_msgs *msgs)
{
	int ret;
	struct mpp_task *mpp_task = NULL;
	struct jpgenc_task *task = NULL;
	struct mpp_dev *mpp = session->mpp;

	mpp_debug_enter();

	task = kzalloc(sizeof(*task), GFP_KERNEL);
	if (!task)
		return NULL;

	mpp_task = &task->mpp_task;
	mpp_task_init(session, mpp_task);
	mpp_task->hw_info = mpp->var->hw_info;
	mpp_task->reg = task->reg;
	/* process fd in register */
	ret = jpgenc_extract_task_msg(task, msgs);
	if (ret)
		goto fail;

	if (!(msgs->flags & MPP_FLAGS_REG_FD_NO_TRANS)) {
		ret = jpgenc_process_reg_fd(session, task);
		if (ret)
			goto fail;

	}
	task->clk_mode = CLK_MODE_NORMAL;
	task->width = JPGENC_GET_WIDTH(task->reg[JPGENC_REG_ENC_RSL_INDEX]);
	task->height = JPGENC_GET_HEIGHT(task->reg[JPGENC_REG_ENC_RSL_INDEX]);
	task->pixels = task->width * task->height;
	mpp_debug(DEBUG_TASK_INFO, "width=%d, height=%d\n", task->width, task->height);

	mpp_debug_leave();
	return mpp_task;
fail:
	mpp_task_dump_mem_region(mpp, mpp_task);
	mpp_task_dump_reg(mpp, mpp_task);
	mpp_task_finalize(session, mpp_task);
	kfree(task);
	return NULL;
}

static int jpgenc_run(struct mpp_dev *mpp, struct mpp_task *mpp_task)
{
	u32 i;
	u32 reg_en;
	struct jpgenc_task *task = to_jpgenc_task(mpp_task);
	u32 timing_en = mpp->srv->timing_en;

	mpp_debug_enter();

	/* set registers for hardware */
	reg_en = mpp_task->hw_info->reg_en;
	task->reg[JPGENC_REG_INT_EN_INDEX] = 0xff;
	task->reg[JPGENC_REG_INT_MASK_INDEX] = 0xff;

	for (i = 0; i < task->w_req_cnt; i++) {
		struct mpp_request *req = &task->w_reqs[i];
		u32 s = req->offset / sizeof(u32);
		u32 e = s + req->size / sizeof(u32);

		mpp_write_req(mpp, task->reg, s, e, reg_en);
	}

	/* flush tlb before starting hardware */
	mpp_iommu_flush_tlb(mpp->iommu_info);

	/* init current task */
	mpp->cur_task = mpp_task;

	mpp_task_run_begin(mpp_task, timing_en, MPP_WORK_TIMEOUT_DELAY);

	/* Flush the register before starting device */
	wmb();
	mpp_write(mpp, JPGENC_REG_START_EN_BASE, task->reg[reg_en] | JPGENC_START_EN);

	mpp_task_run_end(mpp_task, timing_en);

	mpp_debug_leave();
	return 0;
}

static int jpgenc_finish(struct mpp_dev *mpp, struct mpp_task *mpp_task)
{
	u32 i;
	u32 s, e;
	struct mpp_request *req;
	struct jpgenc_task *task = to_jpgenc_task(mpp_task);

	mpp_debug_enter();

	for (i = 0; i < task->r_req_cnt; i++) {
		req = &task->r_reqs[i];
		s = req->offset / sizeof(u32);
		e = s + req->size / sizeof(u32);
		mpp_read_req(mpp, task->reg, s, e);
	}

	task->reg[JPGENC_REG_INT_STATUS_INDEX] = task->irq_status;

	mpp_debug_leave();
	return 0;
}

static int jpgenc_result(struct mpp_dev *mpp, struct mpp_task *mpp_task, struct mpp_task_msgs *msgs)
{
	u32 i;
	struct mpp_request *req;
	struct jpgenc_task *task = to_jpgenc_task(mpp_task);

	mpp_debug_enter();

	for (i = 0; i < task->r_req_cnt; i++) {
		req = &task->r_reqs[i];
		if (copy_to_user(req->data,
				(u8 *)task->reg + req->offset,
				req->size)) {
			mpp_err("copy_to_user reg fail\n");
			mpp_debug_leave();
			return -EIO;
		}
	}

	mpp_debug_leave();
	return 0;
}

static int jpgenc_free_task(struct mpp_session *session, struct mpp_task *mpp_task)
{
	struct jpgenc_task *task = to_jpgenc_task(mpp_task);

	mpp_debug_enter();

	mpp_task_finalize(session, mpp_task);
	kfree(task);

	mpp_debug_leave();
	return 0;
}

#ifdef CONFIG_ROCKCHIP_MPP_PROC_FS
static int jpgenc_procfs_remove(struct mpp_dev *mpp)
{
	struct jpgenc_dev *enc = to_jpgenc_dev(mpp);

	mpp_debug_enter();

	if (enc->procfs) {
		proc_remove(enc->procfs);
		enc->procfs = NULL;
	}

	mpp_debug_leave();
	return 0;
}

static int jpgenc_show_session_info(struct seq_file *seq, void *offset)
{
	return 0;
}

static int jpgenc_procfs_init(struct mpp_dev *mpp)
{
	struct jpgenc_dev *enc = to_jpgenc_dev(mpp);

	mpp_debug_enter();

	enc->procfs = proc_mkdir(mpp->dev->of_node->name, mpp->srv->procfs);

	if (IS_ERR_OR_NULL(enc->procfs)) {
		mpp_err("failed on open procfs\n");
		enc->procfs = NULL;
		return -EIO;
	}

	mpp_procfs_create_common(enc->procfs, mpp);
	mpp_procfs_create_u32("aclk", 0644, enc->procfs, &enc->aclk_info.debug_rate_hz);
	mpp_procfs_create_u32("hclk", 0644, enc->procfs, &enc->hclk_info.debug_rate_hz);
	mpp_procfs_create_u32("session_buffers", 0644, enc->procfs, &mpp->session_max_buffers);
	mpp_procfs_create_u32("link_mode", 0644, enc->procfs, &enc->link_mode);
	proc_create_single_data("sessions-info", 0444, enc->procfs, jpgenc_show_session_info, mpp);

	mpp_debug_leave();
	return 0;
}
#else
static inline int jpgenc_procfs_remove(struct mpp_dev *mpp)
{
	return 0;
}

static inline int jpgenc_procfs_init(struct mpp_dev *mpp)
{
	return 0;
}
#endif

static int jpgenc_init(struct mpp_dev *mpp)
{
	int ret;
	struct jpgenc_dev *enc = to_jpgenc_dev(mpp);

	mpp_debug_enter();

	ret = mpp_get_clk_info(mpp, &enc->aclk_info, "aclk_vcodec");
	if (ret)
		mpp_err("failed on clk_get aclk_vcodec\n");

	ret = mpp_get_clk_info(mpp, &enc->hclk_info, "hclk_vcodec");
	if (ret)
		mpp_err("failed on clk_get hclk_vcodec\n");

	mpp_set_clk_info_rate_hz(&enc->aclk_info, CLK_MODE_DEFAULT, 700 * MHZ);

	enc->rst_a = mpp_reset_control_get(mpp, RST_TYPE_A, "video_a");
	if (!enc->rst_a)
		mpp_err("No aclk reset resource define\n");

	enc->rst_h = mpp_reset_control_get(mpp, RST_TYPE_H, "video_h");
	if (!enc->rst_h)
		mpp_err("No hclk reset resource define\n");

	mpp_debug_leave();
	return 0;
}

static int jpgenc_clk_on(struct mpp_dev *mpp)
{
	struct jpgenc_dev *enc = to_jpgenc_dev(mpp);

	mpp_clk_safe_enable(enc->aclk_info.clk);
	mpp_clk_safe_enable(enc->hclk_info.clk);

	return 0;
}

static int jpgenc_clk_off(struct mpp_dev *mpp)
{
	struct jpgenc_dev *enc = to_jpgenc_dev(mpp);

	mpp_clk_safe_disable(enc->aclk_info.clk);
	mpp_clk_safe_disable(enc->hclk_info.clk);

	return 0;
}

static int jpgenc_set_freq(struct mpp_dev *mpp, struct mpp_task *mpp_task)
{
	struct jpgenc_dev *enc = to_jpgenc_dev(mpp);
	struct jpgenc_task *task = to_jpgenc_task(mpp_task);

	mpp_clk_set_rate(&enc->aclk_info, task->clk_mode);

	return 0;
}

static int jpgenc_irq(struct mpp_dev *mpp)
{
	mpp->irq_status = mpp_read(mpp, JPGENC_REG_INT_STATUS_BASE);
	mpp_write(mpp, JPGENC_REG_INT_CLR_BASE, mpp->irq_status);
	if (!(mpp->irq_status & JPGENC_INT_ST_ENC_DONE))
		return IRQ_NONE;

	mpp_write(mpp, JPGENC_REG_START_EN_BASE, 0);
	return IRQ_WAKE_THREAD;
}

static int jpgenc_isr(struct mpp_dev *mpp)
{
	int err_mask = 0x340;
	struct jpgenc_task *task = NULL;
	struct mpp_task *mpp_task = mpp->cur_task;
	struct jpgenc_dev *enc = to_jpgenc_dev(mpp);

	mpp_debug_enter();

	if (!mpp_task) {
		dev_err(mpp->dev, "no current task\n");
		return IRQ_HANDLED;
	}

	mpp_task->hw_cycles = mpp_read(mpp, JPGENC_REG_PERF_WORKING_CNT);
	mpp_time_diff_with_hw_time(mpp_task, enc->aclk_info.real_rate_hz);
	mpp->cur_task = NULL;
	task = to_jpgenc_task(mpp_task);
	task->irq_status = mpp->irq_status;
	mpp_debug(DEBUG_IRQ_STATUS, "irq_status: %08x\n", task->irq_status);

	if (err_mask & task->irq_status)
		atomic_inc(&mpp->reset_request);

	mpp_task_finish(mpp_task->session, mpp_task);

	mpp_debug_leave();

	return IRQ_HANDLED;
}

static int jpgenc_reset(struct mpp_dev *mpp)
{
	struct jpgenc_dev *enc = to_jpgenc_dev(mpp);

	mpp_debug_enter();

	if (enc->rst_a && enc->rst_h) {
		mpp_debug(DEBUG_RESET, "reset n\n");

		mpp_pmu_idle_request(mpp, true);
		mpp_safe_reset(enc->rst_a);
		mpp_safe_reset(enc->rst_h);
		udelay(5);
		mpp_safe_unreset(enc->rst_a);
		mpp_safe_unreset(enc->rst_h);
		mpp_pmu_idle_request(mpp, false);

		mpp_debug(DEBUG_RESET, "reset out\n");
	}
	mpp_write(mpp, JPGENC_REG_INT_EN_BASE, 0);

	mpp_debug_leave();
	return 0;
}

static struct mpp_hw_ops jpgenc_v1_hw_ops = {
	.init = jpgenc_init,
	.clk_on = jpgenc_clk_on,
	.clk_off = jpgenc_clk_off,
	.set_freq = jpgenc_set_freq,
	.reset = jpgenc_reset,
};

static struct mpp_dev_ops jpgenc_v1_dev_ops = {
	.alloc_task = jpgenc_alloc_task,
	.prepare = NULL,
	.run = jpgenc_run,
	.irq = jpgenc_irq,
	.isr = jpgenc_isr,
	.finish = jpgenc_finish,
	.result = jpgenc_result,
	.free_task = jpgenc_free_task,
	.ioctl = NULL,
	.init_session = NULL,
	.free_session = NULL,
	.dump_session = NULL,
};

static const struct mpp_dev_var jpgenc_v1_data = {
	.device_type = MPP_DEVICE_RKJPEGE,
	.hw_info = &jpgenc_v1_hw_info,
	.trans_info = jpgenc_v1_trans,
	.hw_ops = &jpgenc_v1_hw_ops,
	.dev_ops = &jpgenc_v1_dev_ops,
};

static const struct of_device_id mpp_jpgenc_dt_match[] = {
	{
		.compatible = "rockchip,rkv-jpeg-encoder-v1",
		.data = &jpgenc_v1_data,
	},
	{},
};

static int jpgenc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct jpgenc_dev *enc = NULL;
	struct mpp_dev *mpp = NULL;
	const struct of_device_id *match = NULL;
	int ret = 0;

	dev_info(dev, "probe device\n");
	enc = devm_kzalloc(dev, sizeof(struct jpgenc_dev), GFP_KERNEL);
	if (!enc)
		return -ENOMEM;

	mpp = &enc->mpp;
	platform_set_drvdata(pdev, mpp);

	if (pdev->dev.of_node) {
		match = of_match_node(mpp_jpgenc_dt_match, pdev->dev.of_node);
		if (match)
			mpp->var = (struct mpp_dev_var *) match->data;
	}

	ret = mpp_dev_probe(mpp, pdev);

	if (ret) {
		dev_err(dev, "probe sub driver failed\n");
		return -EINVAL;
	}

	ret = devm_request_threaded_irq(dev, mpp->irq, mpp_dev_irq, NULL,
					IRQF_SHARED, dev_name(dev), mpp);

	if (ret) {
		dev_err(dev, "register interrupt runtime failed\n");
		return -EINVAL;
	}

	mpp->session_max_buffers = JPGENC_SESSION_MAX_BUFFERS;
	jpgenc_procfs_init(mpp);
	/* register current device to mpp service */
	mpp_dev_register_srv(mpp, mpp->srv);
	dev_info(dev, "probe finish");
	return 0;
}

static int jpgenc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mpp_dev *mpp = dev_get_drvdata(dev);

	dev_info(dev, "remove device\n");
	mpp_dev_remove(mpp);
	jpgenc_procfs_remove(mpp);
	return 0;
}

struct platform_driver rockchip_jpgenc_driver = {
	.probe = jpgenc_probe,
	.remove = jpgenc_remove,
	.shutdown = mpp_dev_shutdown,
	.driver = {
		.name = JPGENC_DRIVER_NAME,
		.of_match_table = of_match_ptr(mpp_jpgenc_dt_match),
	},
};
EXPORT_SYMBOL(rockchip_jpgenc_driver);
