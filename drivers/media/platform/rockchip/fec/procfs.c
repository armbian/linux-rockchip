// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) Rockchip Electronics Co., Ltd. */

#include <linux/io.h>

#include "fec_offline.h"
#include "hw.h"
#include "procfs.h"
#include "regs.h"
#include "version.h"

#ifdef CONFIG_PROC_FS

/************************offline************************/

static void offline_fec_show_hw(struct seq_file *p, struct rkfec_hw_dev *hw)
{
	u32 val;

	static const char * const wr_mode[] = {
		"rast",
		"til4x4",
		"fbce",
		"quad"
	};

	static const char * const bic_mode[] = {
		"precise",
		"spline",
		"catrom",
		"mitchell"
	};

	static const char * const lut_density[] = {
		"32x16",
		"16x8",
		"4x4"
	};

	static const char * const cacheline[] = {
		"64B",
		"64B",
		"128B",
		"128B"
	};

	if (hw->dev->power.usage_count.counter <= 0) {
		seq_printf(p, "\n%s\n", "HW close");
		return;
	}

	val = readl(hw->base_addr + RKFEC_CTRL);
	seq_printf(p, "%-10s RD_fmt:%s RD_mode:%s WR_fmt:%s WR_mode:%s WR_fbce_unc:%s (0x%x)\n",
		   "CTRL",
		   val & BIT(2) ? "semi" : "interleave",
		   (val >> 4) & 0x3 ? "semi" : "rast",
		   val & BIT(8) ? "semi" : "interleave", wr_mode[val >> 9],
		   val & BIT(13) ? "on" : "off", val);

	val = readl(hw->base_addr + RKFEC_CORE_CTRL);
	seq_printf(p, "%-10s Bic:%s Lut_density:%s, Border_fill:%s, Cross_fill:%s (0x%x)\n",
		   "CORE_CTRL",
		   bic_mode[(val >> 3) & 0x3], lut_density[(val >> 5) & 0x3],
		   val & BIT(7) ? "nearest" : "bg", val & BIT(10) ? "nearest" : "bg", val);

	val = readl(hw->base_addr + RKFEC_RD_VIR_STRIDE);
	seq_printf(p, "%-10s Y:%d C:%d\n", "RD_VIR", (val & 0x3FFF) * 4,
		   ((val >> 16) & 0x3FFF) * 4);

	val = readl(hw->base_addr + RKFEC_WR_VIR_STRIDE);
	seq_printf(p, "%-10s Y:%d C:%d\n", "WR_VIR", (val & 0x3FFF) * 4,
		   ((val >> 16) & 0x3FFF) * 4);

	val = readl(hw->base_addr + RKFEC_BG_VALUE);
	seq_printf(p, "%-10s Y:%d U:%d V:%d\n", "BG_VALUE", val & 0xFF,
		   (val >> 10) & 0xFF, (val >> 20) & 0xFF);

	val = readl(hw->base_addr + RKFEC_LUT_SIZE);
	seq_printf(p, "%-10s Size: %d\n", "LUT", val & 0x3FFFFF);

	val = readl(hw->base_addr + RKFEC_STATUS0);
	seq_printf(p, "%-10s 0x%x\n", "STATUS0", val & 0x3FFFFF);

	val = readl(hw->base_addr + RKFEC_STATUS1);
	seq_printf(p, "%-10s 0x%x\n", "STATUS1", val & 0x3FFFFF);

	val = readl(hw->base_addr + RKFEC_CACHE_CTRL);
	seq_printf(p, "%-10s %s\n", "Cacheline",  cacheline[(val >> 4) & 0x3]);
}

static int offline_fec_show(struct seq_file *p, void *v)
{
	struct rkfec_offline_dev *ofl = p->private;
	struct rkfec_hw_dev *hw = ofl->hw;
	int i;

	seq_printf(p, "%-10s Version:v%02x.%02x.%02x\n", ofl->v4l2_dev.name,
		   RKFEC_DRIVER_VERSION >> 16,
		   (RKFEC_DRIVER_VERSION & 0xff00) >> 8,
		   RKFEC_DRIVER_VERSION & 0xff);
	for (i = 0; i < ofl->hw->clks_num; i++) {
		seq_printf(p, "%-10s %ld\n", ofl->hw->match_data->clks[i],
			   clk_get_rate(ofl->hw->clks[i]));
	}

	seq_printf(p, "%-10s Cnt:%d ErrCnt:%d\n", "Interrupt", ofl->isr_cnt,
		   ofl->err_cnt);

	seq_printf(p, "%-10s Format:%c%c%c%c Size:%dx%d Offset(%d) Sizeimage(%d)\n", "Input",
		   ofl->in_fmt.pixelformat, ofl->in_fmt.pixelformat >> 8,
		   ofl->in_fmt.pixelformat >> 16, ofl->in_fmt.pixelformat >> 24,
		   ofl->in_fmt.width, ofl->in_fmt.height, ofl->in_fmt.offset,
		   ofl->in_fmt.sizeimage);

	seq_printf(p, "%-10s (frame:%d rate:%dms state:%s time:%dms frameloss:%d frm_oversdtim_cnt:%d)\n",
		   "Fec offline",
		   ofl->curr_frame.fs_seq,
		   (u32)(ofl->curr_frame.fs_timestamp - ofl->prev_frame.fs_timestamp) / 1000 / 1000,
		   (ofl->state & RKFEC_FRAME_END) ? "idle" : "working",
		   ofl->debug.interval / 1000,
		   ofl->debug.frameloss,
		   ofl->debug.frame_timeout_cnt);

	seq_printf(p, "%-10s Format:%c%c%c%c Size:%dx%d Offset(%d) Sizeimage(%d) (frame:%d rate:%dms frameloss:%d\n",
		   "Output",
		   ofl->out_fmt.pixelformat,
		   ofl->out_fmt.pixelformat >> 8,
		   ofl->out_fmt.pixelformat >> 16,
		   ofl->out_fmt.pixelformat >> 24,
		   ofl->out_fmt.width,
		   ofl->out_fmt.height,
		   ofl->out_fmt.offset,
		   ofl->out_fmt.sizeimage,
		   ofl->curr_frame.fe_seq,
		   (u32)(ofl->curr_frame.fe_timestamp - ofl->prev_frame.fe_timestamp) / 1000 / 1000,
		   ofl->debug.frameloss);

	offline_fec_show_hw(p, hw);

	return 0;
}

static int offline_fec_open(struct inode *inode, struct file *file)
{
#if IS_LINUX_VERSION_AT_LEAST_6_1
	struct rkfec_offline_dev *data = pde_data(inode);
#else
	struct rkfec_offline_dev *data = PDE_DATA(inode);
#endif

	return single_open(file, offline_fec_show, data);
}

static const struct proc_ops offline_ops = {
	.proc_open = offline_fec_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

int rkfec_offline_proc_init(struct rkfec_offline_dev *dev)
{
	dev->procfs = proc_create_data(dev->v4l2_dev.name, 0, NULL, &offline_ops, dev);
	if (!dev->procfs)
		return -EINVAL;
	return 0;
}

void rkfec_offline_proc_cleanup(struct rkfec_offline_dev *dev)
{
	if (dev->procfs)
		remove_proc_entry(dev->v4l2_dev.name, NULL);
	dev->procfs = NULL;
}
#endif /* CONFIG_PROC_FS */
