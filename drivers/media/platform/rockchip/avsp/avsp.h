/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2025 Rockchip Electronics Co., Ltd. */

#ifndef __RKAVSP_H__
#define __RKAVSP_H__

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <media/videobuf2-cma-sg.h>
#include <media/videobuf2-dma-sg.h>
#include <linux/iommu.h>
#include <soc/rockchip/rockchip_iommu.h>

extern int rkavsp_log_level;

#define RKAVSP_LEVEL_ERR	0
#define RKAVSP_LEVEL_INFO	1
#define RKAVSP_LEVEL_DBG	2

#define RKAVSP_PRINT(level, fmt, args...) \
	do { \
		if (rkavsp_log_level >= level) { \
			if (level == RKAVSP_LEVEL_ERR) \
				dev_err(avsp->dev, "%s:%d " fmt, __func__, __LINE__, ##args); \
			else if (level == RKAVSP_LEVEL_INFO) \
				dev_info(avsp->dev, "%s:%d " fmt, __func__, __LINE__, ##args); \
			else if (level == RKAVSP_LEVEL_DBG) \
				dev_dbg(avsp->dev, "%s:%d " fmt, __func__, __LINE__, ##args); \
		} \
	} while (0)

#define RKAVSP_ERR(fmt,  args...)   RKAVSP_PRINT(RKAVSP_LEVEL_ERR,  fmt, ##args)
#define RKAVSP_INFO(fmt, args...)   RKAVSP_PRINT(RKAVSP_LEVEL_INFO, fmt, ##args)
#define RKAVSP_DBG(fmt,  args...)   RKAVSP_PRINT(RKAVSP_LEVEL_DBG,  fmt, ##args)

#define RKAVSP_CMD_DCP  \
	_IOW('V', 192 + 20, struct rkavsp_dcp_in_out)

#define RKAVSP_CMD_RCS  \
	_IOW('V', 192 + 21, struct rkavsp_rcs_in_out)

#define AVSP_NAME                "rockchip_avsp"
#define AVSP_MAX_BUS_CLK          3
#define RKAVSP_API_VERSION        KERNEL_VERSION(0, 1, 0)
#define RKAVSP_PRY_MAX            6

struct rkavsp_buf {
	int fd;
	struct file *file;
	struct list_head list;
	struct vb2_buffer vb;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *dba;
	void *mem;
	bool alloc;
};

struct avsp_match_data {
	int clks_num;
	const char * const *clks;
	int clk_rate_tbl_num;
	const struct avsp_clk_info *clk_rate_tbl;
	struct irqs_data *irqs;
	int num_irqs;
};

struct rkavsp_pix_format {
	u32 bandnum;
	u32 width;
	u32 height;
	u32 mode;
	u32 offset;
	u32 stride_y;
	u32 stride_c;
};

struct rkavsp_pix_format_pry {
	u32 width[RKAVSP_PRY_MAX];
	u32 height[RKAVSP_PRY_MAX];
	u32 mode;
	u32 offset;
	u32 bytesperline;
};

struct rkavsp_frame_info {
	u32 fs_seq;
	u64 fs_timestamp;
};

struct rkavsp_debug_info {
	u32 interval;
	u32 frame_timeout_cnt;
};

enum rkfavsp_state {
	RKAVSP_DCP_FRAME_END = BIT(1),
	RKAVSP_DCP_STOP = BIT(16),
	RKAVSP_DCP_START = BIT(17),
	RKAVSP_DCP_ERROR = BIT(18),
	RKAVSP_RCS_FRAME_END = BIT(1),
	RKAVSP_RCS_STOP = BIT(16),
	RKAVSP_RCS_START = BIT(17),
	RKAVSP_RCS_ERROR = BIT(18),
};

struct rkavsp_dev {
	struct device *dev;
	struct regmap *grf;
	struct completion dcp_cmpl;
	struct completion rcs_cmpl;
	struct list_head list;
	const struct vb2_mem_ops *mem_ops;
	struct vb2_queue vb2_queue;
	void __iomem *base;
	struct reset_control *reset;
	const struct avsp_match_data *match_data;
	const struct avsp_clk_info *clk_rate_tbl;
	struct clk *clks[AVSP_MAX_BUS_CLK];
	int clk_rate_tbl_num;
	int clks_num;

	struct mutex dev_lock;
	struct mutex dcp_lock;
	struct mutex rcs_lock;
	struct miscdevice mdev;
	bool is_dma_config;
	struct proc_dir_entry *procfs;
	unsigned int dcp_isr_cnt;
	unsigned int dcp_err_cnt;
	unsigned int rcs_isr_cnt;
	unsigned int rcs_err_cnt;
	unsigned int in_seq;
	unsigned int out_seq;
	unsigned int dcp_state;
	unsigned int rcs_state;
	struct rkavsp_frame_info dcp_prev_frame;
	struct rkavsp_frame_info dcp_curr_frame;
	struct rkavsp_debug_info dcp_debug;
	struct rkavsp_pix_format dcp_in_fmt;
	struct rkavsp_pix_format_pry dcp_out_fmt;
	struct rkavsp_frame_info rcs_prev_frame;
	struct rkavsp_frame_info rcs_curr_frame;
	struct rkavsp_debug_info rcs_debug;
	struct rkavsp_pix_format rcs_in_fmt;
	struct rkavsp_pix_format rcs_out_fmt;

};

struct rkavsp_dcp_in_out {
	int in_width;
	int in_height;
	int bandnum;
	int dcp_rd_mode;
	int dcp_wr_mode;

	int dcp_rd_stride_y;
	int dcp_rd_stride_c;
	int dcp_wr_stride_y[6];
	int dcp_wr_stride_c[6];

	int in_pic_fd;
	int out_pry_fd[6];
};

struct rkavsp_rcs_in_out {
	int in_width;
	int in_height;
	int bandnum;

	int rcs_wr_mode;
	int rcs_wr_stride_y;
	int rcs_wr_stride_c;

	int in_pry0_fd[6];
	int in_pry1_fd[6];
	int dt_pry_fd[6];
	int out_pic_fd;
	int rcs_out_start_offset;
};

struct avsp_clk_info {
	u32 clk_rate;
	u32 refer_data;
};

struct irqs_data {
	const char *name;
	irqreturn_t (*irq_hdl)(int irq, void *ctx);
};

#ifndef IS_LINUX_VERSION_AT_LEAST_6_1
#define IS_LINUX_VERSION_AT_LEAST_6_1 (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
#endif

#endif
