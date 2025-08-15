// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) Rockchip Electronics Co., Ltd. */

#include <linux/io.h>

#include "avsp.h"
#include "procfs.h"
#include "regs.h"
#include "version.h"

#ifdef CONFIG_PROC_FS

static const char * const mode[] = {
	"raster(NV12)",
	"til4x4",
	"fbce",
	"quad"
};

static void rkavsp_show_hw(struct seq_file *p, struct rkavsp_dev *hw)
{
	u32 val;
	int i, bandnum;

	if (hw->dev->power.usage_count.counter <= 0) {
		seq_printf(p, "\n%s\n", "HW close");
		return;
	}

	// DCP
	val = readl(hw->base + AVSP_DCP_CTRL);
	bandnum = (val & 0x7);
	seq_printf(p, "%-15s Bandnum: %d RD_mode: %s WR_mode: %s By_pass: %d\n",
		   "DCP_CTRL",
		   (val & 0x7),
		   mode[(val >> 3) & 0x3],
		   mode[(val >> 5) & 0x3],
		   (val >> 7) & 0x1);

	val = readl(hw->base + AVSP_DCP_SIZE);
	seq_printf(p, "%-15s Width: %d Height: %d\n", "DCP_SIZE",
		   (val & 0x07FF), (val >> 16) & 0x1FFF);

	val = readl(hw->base + AVSP_DCP_RD_VIR_SIZE);
	seq_printf(p, "%-15s Y:%d C:%d\n", "DCP_RD_VIR",
		   (val & 0x3FFF), (val >> 16) & 0x3FFF);

	for (i = 0; i < bandnum; i++) {
		val = readl(hw->base + AVSP_DCP_WR_LV0_VIR_SIZE + i * 4);
		seq_printf(p, "%-15s Band: %d Y: %d C: %d\n", "DCP_WD_VIR",
			   i, (val & 0x3FFF), (val >> 16) & 0x3FFF);
	}

	for (i = 0; i < 5; i++) {
		val = readl(hw->base + AVSP_DCP_STATUS0 + i * 4);
		seq_printf(p, "%-15s %d  0x%x\n", "DCP_STATUS", i, val);
	}

	// RCS
	val = readl(hw->base + AVSP_RCS_CTRL);
	seq_printf(p, "%-15s Bandnum: %d RD_mode: %s WR_mode: %s FBCE_CTL: %x\n",
		   "RCS_CTRL",
		   (val & 0x7),
		   mode[(val >> 3) & 0x3],
		   mode[(val >> 5) & 0x3],
		   (val >> 9) & 0xF);

	val = readl(hw->base + AVSP_RCS_SIZE);
	seq_printf(p, "%-15s Width: %d Height: %d\n", "RCS_SIZE",
		   (val & 0x07FF), (val >> 16) & 0x1FFF);

	val = readl(hw->base + AVSP_DCP_RD_VIR_SIZE);
	seq_printf(p, "%-15s Y: %d C: %d\n", "RCS_WR_VIR",
		   (val & 0x3FFF), (val >> 16) & 0x3FFF);

	for (i = 0; i < 5; i++) {
		val = readl(hw->base + AVSP_RCS_STATUS0 + i * 4);
		seq_printf(p, "%-15s %d  0x%x\n", "RCS_STATUS", i, val);
	}

}

static int rkavsp_show(struct seq_file *p, void *v)
{
	struct rkavsp_dev *ofl = p->private;
	int i;

	seq_printf(p, "%-20s Version:v%02x.%02x.%02x\n", AVSP_NAME,
		   RKAVSP_DRIVER_VERSION >> 16,
		   (RKAVSP_DRIVER_VERSION & 0xff00) >> 8,
		   RKAVSP_DRIVER_VERSION & 0xff);
	for (i = 0; i < ofl->clks_num; i++) {
		seq_printf(p, "%-15s %ld\n", ofl->match_data->clks[i],
			   clk_get_rate(ofl->clks[i]));
	}

	seq_printf(p, "%-15s DCP_Cnt:%d ErrCnt:%d\n", "DCP_INT", ofl->dcp_isr_cnt, ofl->dcp_err_cnt);
	seq_printf(p, "%-15s RCS_Cnt:%d ErrCnt:%d\n", "RCS_INT", ofl->rcs_isr_cnt, ofl->rcs_err_cnt);

	// DCP
	seq_printf(p, "%-15s Bandnum: %d Rdmode: %x(%s) Size:%dx%d\n",
		   "DCP:Input",
		   ofl->dcp_in_fmt.bandnum, ofl->dcp_in_fmt.mode, mode[ofl->dcp_in_fmt.mode],
		   ofl->dcp_in_fmt.width, ofl->dcp_in_fmt.height);

	seq_printf(p, "%-15s In_offset:%d(Byte) Stride_y: %d Stride_c: %d(Word)\n",
		   "DCP:Input",
		   ofl->dcp_in_fmt.offset, ofl->dcp_in_fmt.stride_y, ofl->dcp_in_fmt.stride_c);

	seq_printf(p, "%-15s Wrmode: %x(%s)\n",
		   "DCP:Output", ofl->dcp_out_fmt.mode, mode[ofl->dcp_out_fmt.mode]);

	for (i = 0; i < ofl->dcp_in_fmt.bandnum; i++) {
		seq_printf(p, "%-15s band: %d Stride and Hgt: %d(Word) x %d(Byte)\n",
			   "DCP:Output", i, ofl->dcp_out_fmt.width[i], ofl->dcp_out_fmt.height[i]);
	}

	seq_printf(p, "%-15s (frame:%d rate:%dms state:%s time:%dms frm_timeout_cnt:%d)\n\n",
		   "AVSP_DCP",
		   ofl->dcp_curr_frame.fs_seq,
		   (u32)(ofl->dcp_curr_frame.fs_timestamp - ofl->dcp_prev_frame.fs_timestamp) /
			 1000 / 1000,
		   (ofl->rcs_state & RKAVSP_DCP_FRAME_END) ? "idle" : "working",
		   ofl->dcp_debug.interval / 1000,
		   ofl->dcp_debug.frame_timeout_cnt);

	// RCS
	seq_printf(p, "%-15s Bandnum: %d Rdmode: %x(%s) Size:%dx%d\n", "RCS:Input",
		   ofl->rcs_in_fmt.bandnum, ofl->rcs_in_fmt.mode, mode[ofl->rcs_in_fmt.mode],
		   ofl->rcs_in_fmt.width, ofl->rcs_in_fmt.height);

	seq_printf(p, "%-15s Wrmode: %x(%s) Offsets %d(Byte) Stride_y: %d Stride_c: %d(Word)\n",
		   "RCS:Output",
		   ofl->rcs_out_fmt.mode, mode[ofl->rcs_out_fmt.mode], ofl->rcs_out_fmt.offset,
		   ofl->rcs_out_fmt.stride_y, ofl->rcs_out_fmt.stride_c);

	seq_printf(p, "%-15s (frame:%d rate:%dms state:%s time:%dms frm_timeout_cnt:%d)\n\n",
		   "AVSP_RCS",
		   ofl->rcs_curr_frame.fs_seq,
		   (u32)(ofl->rcs_curr_frame.fs_timestamp - ofl->rcs_prev_frame.fs_timestamp) /
			 1000 / 1000,
		   (ofl->rcs_state & RKAVSP_RCS_FRAME_END) ? "idle" : "working",
		   ofl->rcs_debug.interval / 1000,
		   ofl->rcs_debug.frame_timeout_cnt);

	rkavsp_show_hw(p, ofl);

	return 0;
}

static int rkavsp_open(struct inode *inode, struct file *file)
{
#if IS_LINUX_VERSION_AT_LEAST_6_1
	struct rkavsp_dev *data = pde_data(inode);
#else
	struct rkavsp_dev *data = PDE_DATA(inode);
#endif

	return single_open(file, rkavsp_show, data);
}

static const struct proc_ops rkavsp_ops = {
	.proc_open = rkavsp_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

int rkavsp_proc_init(struct rkavsp_dev *dev)
{
	dev->procfs = proc_create_data(AVSP_NAME, 0, NULL, &rkavsp_ops, dev);
	if (!dev->procfs)
		return -EINVAL;
	return 0;
}

void rkavsp_proc_cleanup(struct rkavsp_dev *dev)
{
	if (dev->procfs)
		remove_proc_entry(AVSP_NAME, NULL);
	dev->procfs = NULL;
}
#endif /* CONFIG_PROC_FS */
