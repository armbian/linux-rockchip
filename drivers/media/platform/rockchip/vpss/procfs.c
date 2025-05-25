// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) Rockchip Electronics Co., Ltd. */

#include "vpss.h"
#include "common.h"
#include "stream.h"
#include "dev.h"
#include "vpss_offline.h"
#include "hw.h"
#include "procfs.h"
#include "regs.h"

#include "version.h"

#ifdef CONFIG_PROC_FS

static void show_hw(struct seq_file *p, struct rkvpss_hw_dev *hw)
{
	int i;
	u32 val, mask;

	if (hw->dev->power.usage_count.counter <= 0) {
		seq_printf(p, "\n%s\n", "HW close");
		return;
	}

	seq_printf(p, "\n%s\n", "HW INFO");

	val = rkvpss_hw_read(hw, RKVPSS_VPSS_CTRL);
	seq_printf(p, "\tmirror:%s(0x%x)\n", (val & RKVPSS_MIR_EN) ? "ON" : "OFF", val);
	val = rkvpss_hw_read(hw, RKVPSS_VPSS_ONLINE);
	seq_printf(p, "\tcmsc:%s(0x%x)\n", (val & RKVPSS_ISP2VPSS_ONLINE2_CMSC_EN) ? "ON" : "OFF", val);

	for (i = 0; i < vpss_outchn_max(hw->vpss_ver); i++) {
		seq_printf(p, "\toutput[%d]", i);
		val = rkvpss_hw_read(hw, RKVPSS_VPSS_ONLINE);
		mask = (RKVPSS_ISP2VPSS_CHN0_SEL(3) << i * 2);
		seq_printf(p, "\tchn_sel:%s(0x%x)", (val & mask) ? "ON" : "OFF", val);
		if (hw->is_ofl_ch[i]) {
			val = rkvpss_hw_read(hw, RKVPSS_CROP0_CTRL);
			mask = RKVPSS_CROP_CHN_EN(i);
			seq_printf(p, "\tcrop:%s(0x%x)", (val & mask) ? "ON" : "OFF", val);
		} else {
			val = rkvpss_hw_read(hw, RKVPSS_CROP1_CTRL);
			mask = RKVPSS_CROP_CHN_EN(i);
			seq_printf(p, "\tcrop:%s(0x%x)", (val & mask) ? "ON" : "OFF", val);
		}
		switch (i) {
		case 0:
			val = rkvpss_hw_read(hw, RKVPSS_RATIO0_CTRL);
			break;
		case 1:
			val = rkvpss_hw_read(hw, RKVPSS_RATIO1_CTRL);
			break;
		case 2:
			val = rkvpss_hw_read(hw, RKVPSS_RATIO2_CTRL);
			break;
		case 3:
			val = rkvpss_hw_read(hw, RKVPSS_RATIO3_CTRL);
			break;
		default:
			break;
		}
		seq_printf(p, "\taspt:%s(0x%x)", (val & 1) ? "ON" : "OFF", val);
		val = rkvpss_hw_read(hw, RKVPSS_MI_WR_VFLIP_CTRL);
		mask = RKVPSS_MI_CHN_V_FLIP(i);
		seq_printf(p, "\tflip:%s(0x%x)\n", (val & mask) ? "ON" : "OFF", val);
	}
}
static int vpss_show(struct seq_file *p, void *v)
{
	struct rkvpss_device *dev = p->private;
	struct rkvpss_hw_dev *hw = dev->hw_dev;
	struct rkvpss_subdev *vpss_sdev = &dev->vpss_sdev;
	struct rkvpss_stream *stream;
	enum rkvpss_state state = dev->vpss_sdev.state;
	int i;
	u32 val;

	seq_printf(p, "%-10s Version:v%02x.%02x.%02x\n",
		   dev->name,
		   RKVPSS_DRIVER_VERSION >> 16,
		   (RKVPSS_DRIVER_VERSION & 0xff00) >> 8,
		   RKVPSS_DRIVER_VERSION & 0x00ff);
	for (i = 0; i < dev->hw_dev->clks_num; i++) {
		seq_printf(p, "%-10s %ld\n",
			   dev->hw_dev->match_data->clks[i],
			   clk_get_rate(dev->hw_dev->clks[i]));
	}
	if (state != VPSS_START)
		return 0;

	seq_printf(p, "%-10s %dx%d\n", "Input size",
		   vpss_sdev->in_fmt.width, vpss_sdev->in_fmt.height);
	seq_printf(p, "is_ofl_cmsc:%d\n", hw->is_ofl_cmsc);

	for (i = 0; i < vpss_outchn_max(hw->vpss_ver); i++) {
		stream = &dev->stream_vdev.stream[i];
		if (hw->is_ofl_ch[i] || !stream->streaming) {
			seq_printf(p, "is_ofl_ch[%d]:%d OFF\n", i, hw->is_ofl_ch[i]);
			continue;
		} else {
			val = rkvpss_hw_read(hw, RKVPSS_MI_CHN0_WR_CTRL + i * 0x100);
			seq_printf(p, "is_ofl_ch[%d]:%d ON(0x%x)\n", i, hw->is_ofl_ch[i], val);
			seq_printf(p, "\tFormat:%c%c%c%c crop_v_offs:%d crop_h_offs:%d crop_width:%d crop_height:%d scl_width:%d scl_height:%d\n",
				   stream->out_fmt.pixelformat,
				   stream->out_fmt.pixelformat >> 8,
				   stream->out_fmt.pixelformat >> 16,
				   stream->out_fmt.pixelformat >> 24,
				   stream->crop.top,
				   stream->crop.left,
				   stream->crop.width,
				   stream->crop.height,
				   stream->out_fmt.width,
				   stream->out_fmt.height);
			seq_printf(p, "\tsequence:%d rate:%dms delay:%dms frameloss:%d buf_cnt:%d\n",
				   stream->dbg.id,
				   stream->dbg.interval / 1000 / 1000,
				   stream->dbg.delay / 1000 / 1000,
				   stream->dbg.frameloss,
				   rkvpss_stream_buf_cnt(stream));
		}
	}

	show_hw(p, hw);

	seq_printf(p, "%-10s Cnt:%d ErrCnt:%d\n",
		   "Interrupt",
		   dev->isr_cnt,
		   dev->isr_err_cnt);
	return 0;
}

static int vpss_open(struct inode *inode, struct file *file)
{
	struct rkvpss_device *data = pde_data(inode);

	return single_open(file, vpss_show, data);
}

static const struct proc_ops ops = {
	.proc_open		= vpss_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

int rkvpss_proc_init(struct rkvpss_device *dev)
{
	dev->procfs = proc_create_data(dev->name, 0, NULL, &ops, dev);
	if (!dev->procfs)
		return -EINVAL;
	return 0;
}

void rkvpss_proc_cleanup(struct rkvpss_device *dev)
{
	if (dev->procfs)
		remove_proc_entry(dev->name, NULL);
	dev->procfs = NULL;
}

/************************offline************************/

static int offline_vpss_show(struct seq_file *p, void *v)
{
	struct rkvpss_offline_dev *ofl = p->private;
	struct rkvpss_hw_dev *hw = ofl->hw;
	struct rkvpss_ofl_cfginfo *cfginfo, *next;
	int i;

	seq_printf(p, "%-10s Version:v%02x.%02x.%02x\n",
		   ofl->v4l2_dev.name,
		   RKVPSS_DRIVER_VERSION >> 16,
		   (RKVPSS_DRIVER_VERSION & 0xff00) >> 8,
		   RKVPSS_DRIVER_VERSION & 0x00ff);
	for (i = 0; i < ofl->hw->clks_num; i++) {
		seq_printf(p, "%-10s %ld\n",
			   ofl->hw->match_data->clks[i],
			   clk_get_rate(ofl->hw->clks[i]));
	}

	seq_printf(p, "is_ofl_cmsc:%d\n", ofl->hw->is_ofl_cmsc);

	mutex_lock(&ofl->ofl_lock);
	list_for_each_entry_safe(cfginfo, next, &ofl->cfginfo_list, list) {
		seq_printf(p, "dev_id:%d sequence:%d\n",
			   cfginfo->dev_id,
			   cfginfo->sequence);
		seq_printf(p, "%-10sbuf_fd:%d Format:%c%c%c%c width:%d height:%d\n",
			   "Input",
			   cfginfo->input.buf_fd,
			   cfginfo->input.format,
			   cfginfo->input.format >> 8,
			   cfginfo->input.format >> 16,
			   cfginfo->input.format >> 24,
			   cfginfo->input.width,
			   cfginfo->input.height);

		seq_printf(p, "%-10s\n", "Output");
		for (i = 0; i < vpss_outchn_max(hw->vpss_ver); i++) {
			if (!ofl->hw->is_ofl_ch[i] || !cfginfo->output[i].enable) {
				seq_printf(p, "\tch[%d] OFF is_ofl_ch[%d]:%d output[%d].enable:%d\n",
					   i, i, ofl->hw->is_ofl_ch[i], i,
					   cfginfo->output[i].enable);
			} else {
				seq_printf(p, "\tch[%d] ON buf_fd:%d Format:%c%c%c%c crop_v_offs:%d crop_h_offs:%d crop_width:%d crop_height:%d scl_width:%d scl_height:%d\n",
					   i,
					   cfginfo->output[i].buf_fd, cfginfo->output[i].format,
					   cfginfo->output[i].format >> 8,
					   cfginfo->output[i].format >> 16,
					   cfginfo->output[i].format >> 24,
					   cfginfo->output[i].crop_v_offs,
					   cfginfo->output[i].crop_h_offs,
					   cfginfo->output[i].crop_width,
					   cfginfo->output[i].crop_height,
					   cfginfo->output[i].scl_width,
					   cfginfo->output[i].scl_height);
			}
		}
	}
	mutex_unlock(&ofl->ofl_lock);

	seq_printf(p, "\n%s\n", "Rate");
	for (i = 0; i < DEV_NUM_MAX; i++) {
		if (ofl->dev_rate[i].in_timestamp == 0)
			continue;
		seq_printf(p, "\tdev_id:%d in_rate:%dms out_rate:%dms sequence:%d delay:%dms\n",
			   i,
			   ofl->dev_rate[i].in_rate / 1000 / 1000,
			   ofl->dev_rate[i].out_rate / 1000 / 1000,
			   ofl->dev_rate[i].sequence,
			   ofl->dev_rate[i].delay / 1000 / 1000);
	}

	show_hw(p, hw);

	return 0;
}

static int offline_vpss_open(struct inode *inode, struct file *file)
{
	struct rkvpss_offline_dev *data = pde_data(inode);

	return single_open(file, offline_vpss_show, data);
}

static const struct proc_ops offline_ops = {
	.proc_open		= offline_vpss_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

int rkvpss_offline_proc_init(struct rkvpss_offline_dev *dev)
{
	dev->procfs = proc_create_data(dev->v4l2_dev.name, 0, NULL, &offline_ops, dev);
	if (!dev->procfs)
		return -EINVAL;
	return 0;
}

void rkvpss_offline_proc_cleanup(struct rkvpss_offline_dev *dev)
{
	if (dev->procfs)
		remove_proc_entry(dev->v4l2_dev.name, NULL);
	dev->procfs = NULL;
}

#endif /* CONFIG_PROC_FS */
