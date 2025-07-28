// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Rockchip Electronics Co., Ltd.
 *
 * Author: Shunqing Chen <csq@rock-chips.com>
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>

#include "rk628.h"
#include "rk628_combrxphy.h"
#include "rk628_combtxphy.h"
#include "rk628_cru.h"
#include "rk628_csi.h"
#include "rk628_dsi.h"
#include "rk628_hdmirx.h"
#include "rk628_post_process.h"

static const struct regmap_range rk628_cru_readable_ranges[] = {
	regmap_reg_range(CRU_CPLL_CON0, CRU_CPLL_CON4),
	regmap_reg_range(CRU_GPLL_CON0, CRU_GPLL_CON4),
	regmap_reg_range(CRU_APLL_CON0, CRU_APLL_CON4),
	regmap_reg_range(CRU_MODE_CON00, CRU_MODE_CON00),
	regmap_reg_range(CRU_CLKSEL_CON00, CRU_CLKSEL_CON21),
	regmap_reg_range(CRU_GATE_CON00, CRU_GATE_CON05),
	regmap_reg_range(CRU_SOFTRST_CON00, CRU_SOFTRST_CON04),
};

static const struct regmap_access_table rk628_cru_readable_table = {
	.yes_ranges     = rk628_cru_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_cru_readable_ranges),
};

static const struct regmap_range rk628_combrxphy_readable_ranges[] = {
	regmap_reg_range(COMBRX_REG(0x6600), COMBRX_REG(0x665b)),
	regmap_reg_range(COMBRX_REG(0x66a0), COMBRX_REG(0x66db)),
	regmap_reg_range(COMBRX_REG(0x66f0), COMBRX_REG(0x66ff)),
	regmap_reg_range(COMBRX_REG(0x6700), COMBRX_REG(0x6790)),
};

static const struct regmap_access_table rk628_combrxphy_readable_table = {
	.yes_ranges     = rk628_combrxphy_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_combrxphy_readable_ranges),
};

static const struct regmap_range rk628_hdmirx_readable_ranges[] = {
	regmap_reg_range(HDMI_RX_HDMI_SETUP_CTRL, HDMI_RX_HDMI_TIMER_CTRL),
	regmap_reg_range(HDMI_RX_HDMI_PCB_CTRL, HDMI_RX_HDMI_PCB_CTRL),
	regmap_reg_range(HDMI_RX_HDMI_MODE_RECOVER, HDMI_RX_HDMI_ERROR_PROTECT),
	regmap_reg_range(HDMI_RX_HDMI_SYNC_CTRL, HDMI_RX_HDMI_CKM_RESULT),
	regmap_reg_range(HDMI_RX_HDMI_RESMPL_CTRL, HDMI_RX_HDMI_RESMPL_CTRL),
	regmap_reg_range(HDMI_VM_CFG_CH0_1, HDMI_VM_CFG_CH2),
	regmap_reg_range(HDMI_RX_HDCP_CTRL, HDMI_RX_HDCP_SETTINGS),
	regmap_reg_range(HDMI_RX_HDCP_KIDX, HDMI_RX_HDCP_KIDX),
	regmap_reg_range(HDMI_RX_HDCP_DBG, HDMI_RX_HDCP_AN0),
	regmap_reg_range(HDMI_RX_HDCP_STS, HDMI_RX_HDCP_STS),
	regmap_reg_range(HDMI_RX_MD_HCTRL1, HDMI_RX_MD_HACT_PX),
	regmap_reg_range(HDMI_RX_MD_VCTRL, HDMI_RX_MD_VSC),
	regmap_reg_range(HDMI_RX_MD_VOL, HDMI_RX_MD_VTL),
	regmap_reg_range(HDMI_RX_MD_IL_POL, HDMI_RX_MD_STS),
	regmap_reg_range(HDMI_RX_AUD_CTRL, HDMI_RX_AUD_CTRL),
	regmap_reg_range(HDMI_RX_AUD_PLL_CTRL, HDMI_RX_AUD_PLL_CTRL),
	regmap_reg_range(HDMI_RX_AUD_CLK_CTRL, HDMI_RX_AUD_CLK_CTRL),
	regmap_reg_range(HDMI_RX_AUD_FIFO_CTRL, HDMI_RX_AUD_FIFO_TH),
	regmap_reg_range(HDMI_RX_AUD_CHEXTR_CTRL, HDMI_RX_AUD_SPARE),
	regmap_reg_range(HDMI_RX_AUD_FIFO_STS, HDMI_RX_AUD_FIFO_STS),
	regmap_reg_range(HDMI_RX_AUDPLL_GEN_CTS, HDMI_RX_AUDPLL_GEN_N),
	regmap_reg_range(HDMI_RX_I2CM_PHYG3_DATAI, HDMI_RX_I2CM_PHYG3_DATAI),
	regmap_reg_range(HDMI_RX_PDEC_CTRL, HDMI_RX_PDEC_CTRL),
	regmap_reg_range(HDMI_RX_PDEC_AUDIODET_CTRL, HDMI_RX_PDEC_AUDIODET_CTRL),
	regmap_reg_range(HDMI_RX_PDEC_ERR_FILTER, HDMI_RX_PDEC_ASP_CTRL),
	regmap_reg_range(HDMI_RX_PDEC_STS, HDMI_RX_PDEC_STS),
	regmap_reg_range(HDMI_RX_PDEC_GCP_AVMUTE, HDMI_RX_PDEC_GCP_AVMUTE),
	regmap_reg_range(HDMI_RX_PDEC_ACR_CTS, HDMI_RX_PDEC_ACR_N),
	regmap_reg_range(HDMI_RX_PDEC_AVI_HB, HDMI_RX_PDEC_AVI_PB),
	regmap_reg_range(HDMI_RX_PDEC_AIF_CTRL, HDMI_RX_PDEC_AIF_PB0),
	regmap_reg_range(HDMI_RX_HDMI20_CONTROL, HDMI_RX_CHLOCK_CONFIG),
	regmap_reg_range(HDMI_RX_SCDC_REGS0, HDMI_RX_SCDC_REGS2),
	regmap_reg_range(HDMI_RX_SCDC_WRDATA0, HDMI_RX_SCDC_WRDATA0),
	regmap_reg_range(HDMI_RX_HDMI20_STATUS, HDMI_RX_HDMI20_STATUS),
	regmap_reg_range(HDMI_RX_PDEC_ISTS, HDMI_RX_PDEC_IEN),
	regmap_reg_range(HDMI_RX_AUD_CEC_ISTS, HDMI_RX_AUD_CEC_IEN),
	regmap_reg_range(HDMI_RX_AUD_FIFO_ISTS, HDMI_RX_AUD_FIFO_IEN),
	regmap_reg_range(HDMI_RX_MD_ISTS, HDMI_RX_MD_IEN),
	regmap_reg_range(HDMI_RX_HDMI_ISTS, HDMI_RX_HDMI_IEN),
	regmap_reg_range(HDMI_RX_DMI_DISABLE_IF, HDMI_RX_DMI_DISABLE_IF),
	regmap_reg_range(HDMI_RX_CEC_CTRL, HDMI_RX_CEC_WAKEUPCTRL),
};

static const struct regmap_access_table rk628_hdmirx_readable_table = {
	.yes_ranges     = rk628_hdmirx_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_hdmirx_readable_ranges),
};

static const struct regmap_range rk628_key_readable_ranges[] = {
	regmap_reg_range(EDID_BASE, EDID_BASE + 0x400),
};

static const struct regmap_access_table rk628_key_readable_table = {
	.yes_ranges     = rk628_key_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_key_readable_ranges),
};

static const struct regmap_range rk628_combtxphy_readable_ranges[] = {
	regmap_reg_range(COMBTXPHY_BASE, COMBTXPHY_CON10),
};

static const struct regmap_access_table rk628_combtxphy_readable_table = {
	.yes_ranges     = rk628_combtxphy_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_combtxphy_readable_ranges),
};

static const struct regmap_range rk628_csi_readable_ranges[] = {
	regmap_reg_range(CSITX_CONFIG_DONE, CSITX_CSITX_VERSION),
	regmap_reg_range(CSITX_SYS_CTRL0_IMD, CSITX_TIMING_HPW_PADDING_NUM),
	regmap_reg_range(CSITX_VOP_PATH_CTRL, CSITX_VOP_PATH_CTRL),
	regmap_reg_range(CSITX_VOP_FILTER_CTRL, CSITX_VOP_FILTER_CTRL),
	regmap_reg_range(CSITX_VOP_PATH_PKT_CTRL, CSITX_VOP_PATH_PKT_CTRL),
	regmap_reg_range(CSITX_CSITX_STATUS0, CSITX_LPDT_DATA_IMD),
	regmap_reg_range(CSITX_DPHY_CTRL, CSITX_DPHY_CTRL),
};

static const struct regmap_access_table rk628_csi_readable_table = {
	.yes_ranges     = rk628_csi_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_csi_readable_ranges),
};

static const struct regmap_range rk628_csi1_readable_ranges[] = {
	regmap_reg_range(CSITX1_CONFIG_DONE, CSITX1_CSITX_VERSION),
	regmap_reg_range(CSITX1_SYS_CTRL0_IMD, CSITX1_TIMING_HPW_PADDING_NUM),
	regmap_reg_range(CSITX1_VOP_PATH_CTRL, CSITX1_VOP_PATH_CTRL),
	regmap_reg_range(CSITX1_VOP_FILTER_CTRL, CSITX1_VOP_FILTER_CTRL),
	regmap_reg_range(CSITX1_VOP_PATH_PKT_CTRL, CSITX1_VOP_PATH_PKT_CTRL),
	regmap_reg_range(CSITX1_CSITX_STATUS0, CSITX1_LPDT_DATA_IMD),
	regmap_reg_range(CSITX1_DPHY_CTRL, CSITX1_DPHY_CTRL),
};

static const struct regmap_access_table rk628_csi1_readable_table = {
	.yes_ranges     = rk628_csi1_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_csi1_readable_ranges),
};

static const struct regmap_range rk628_dsi0_readable_ranges[] = {
	regmap_reg_range(DSI0_BASE, DSI0_BASE + DSI_MAX_REGISTER),
};

static const struct regmap_access_table rk628_dsi0_readable_table = {
	.yes_ranges     = rk628_dsi0_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_dsi0_readable_ranges),
};

static const struct regmap_range rk628_dsi1_readable_ranges[] = {
	regmap_reg_range(DSI1_BASE, DSI1_BASE + DSI_MAX_REGISTER),
};

static const struct regmap_access_table rk628_dsi1_readable_table = {
	.yes_ranges     = rk628_dsi1_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_dsi1_readable_ranges),
};


static const struct regmap_config rk628_regmap_config[RK628_DEV_MAX] = {
	[RK628_DEV_GRF] = {
		.name = "grf",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = GRF_MAX_REGISTER,
		.reg_format_endian = REGMAP_ENDIAN_LITTLE,
		.val_format_endian = REGMAP_ENDIAN_LITTLE,
	},
	[RK628_DEV_CRU] = {
		.name = "cru",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = CRU_MAX_REGISTER,
		.reg_format_endian = REGMAP_ENDIAN_LITTLE,
		.val_format_endian = REGMAP_ENDIAN_LITTLE,
		.rd_table = &rk628_cru_readable_table,
	},
	[RK628_DEV_COMBRXPHY] = {
		.name = "combrxphy",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = COMBRX_REG(0x6790),
		.reg_format_endian = REGMAP_ENDIAN_LITTLE,
		.val_format_endian = REGMAP_ENDIAN_LITTLE,
		.rd_table = &rk628_combrxphy_readable_table,
	},
	[RK628_DEV_DSI0] = {
		.name = "dsi0",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = DSI0_BASE + DSI_MAX_REGISTER,
		.reg_format_endian = REGMAP_ENDIAN_LITTLE,
		.val_format_endian = REGMAP_ENDIAN_LITTLE,
		.rd_table = &rk628_dsi0_readable_table,
	},
	[RK628_DEV_DSI1] = {
		.name = "dsi1",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = DSI1_BASE + DSI_MAX_REGISTER,
		.reg_format_endian = REGMAP_ENDIAN_LITTLE,
		.val_format_endian = REGMAP_ENDIAN_LITTLE,
		.rd_table = &rk628_dsi1_readable_table,
	},
	[RK628_DEV_HDMIRX] = {
		.name = "hdmirx",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = HDMI_RX_MAX_REGISTER,
		.reg_format_endian = REGMAP_ENDIAN_LITTLE,
		.val_format_endian = REGMAP_ENDIAN_LITTLE,
		.rd_table = &rk628_hdmirx_readable_table,
	},
	[RK628_DEV_ADAPTER] = {
		.name = "adapter",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = KEY_MAX_REGISTER,
		.reg_format_endian = REGMAP_ENDIAN_LITTLE,
		.val_format_endian = REGMAP_ENDIAN_LITTLE,
		.rd_table = &rk628_key_readable_table,
	},
	[RK628_DEV_COMBTXPHY] = {
		.name = "combtxphy",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = COMBTXPHY_CON10,
		.reg_format_endian = REGMAP_ENDIAN_LITTLE,
		.val_format_endian = REGMAP_ENDIAN_LITTLE,
		.rd_table = &rk628_combtxphy_readable_table,
	},
	[RK628_DEV_CSI] = {
		.name = "csi",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = CSI_MAX_REGISTER,
		.reg_format_endian = REGMAP_ENDIAN_LITTLE,
		.val_format_endian = REGMAP_ENDIAN_LITTLE,
		.rd_table = &rk628_csi_readable_table,
	},
	[RK628_DEV_CSI1] = {
		.name = "csi1",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = CSI1_MAX_REGISTER,
		.reg_format_endian = REGMAP_ENDIAN_LITTLE,
		.val_format_endian = REGMAP_ENDIAN_LITTLE,
		.rd_table = &rk628_csi1_readable_table,
	},
};

int rk628_media_i2c_write(struct rk628 *rk628, u32 reg, u32 val)
{
	int region = (reg >> 16) & 0xff;
	int ret = 0;

	if (region >= RK628_DEV_MAX) {
		dev_err(rk628->dev,
			"%s: i2c err: invalid arguments, out of register range\n", __func__);
		return -EINVAL;
	}

	if (region == RK628_DEV_HDMIRX) {
		mutex_lock(&rk628->rst_lock);
		ret = regmap_write(rk628->regmap[region], reg, val);
		mutex_unlock(&rk628->rst_lock);
	} else {
		ret = regmap_write(rk628->regmap[region], reg, val);
	}
	if (ret < 0)
		dev_err(rk628->dev,
			"%s: i2c err reg=0x%x, val=0x%x, ret=%d\n", __func__, reg, val, ret);

	return ret;
}
EXPORT_SYMBOL(rk628_media_i2c_write);

int rk628_media_i2c_read(struct rk628 *rk628, u32 reg, u32 *val)
{
	int region = (reg >> 16) & 0xff;
	int ret = 0;

	if (region >= RK628_DEV_MAX) {
		dev_err(rk628->dev,
			"%s: i2c err: invalid arguments, out of register range\n", __func__);
		return -EINVAL;
	}

	if (region == RK628_DEV_HDMIRX) {
		mutex_lock(&rk628->rst_lock);
		ret = regmap_read(rk628->regmap[region], reg, val);
		mutex_unlock(&rk628->rst_lock);
	} else {
		ret = regmap_read(rk628->regmap[region], reg, val);
	}
	if (ret < 0)
		dev_err(rk628->dev,
			"%s: i2c err reg=0x%x, val=0x%x ret=%d\n", __func__, reg, *val, ret);

	return ret;
}
EXPORT_SYMBOL(rk628_media_i2c_read);

int rk628_media_i2c_update_bits(struct rk628 *rk628, u32 reg, u32 mask,
					u32 val)
{
	int region = (reg >> 16) & 0xff;
	int ret;

	if (region >= RK628_DEV_MAX) {
		dev_err(rk628->dev,
			"%s: i2c err: invalid arguments, out of register range\n", __func__);
		return -EINVAL;
	}

	if (region == RK628_DEV_HDMIRX) {
		mutex_lock(&rk628->rst_lock);
		ret = regmap_update_bits(rk628->regmap[region], reg, mask, val);
		mutex_unlock(&rk628->rst_lock);
	} else {
		ret = regmap_update_bits(rk628->regmap[region], reg, mask, val);
	}

	return ret;
}
EXPORT_SYMBOL(rk628_media_i2c_update_bits);

static int rk628_reg_show(struct seq_file *s, void *v)
{
	const struct regmap_config *reg;
	struct rk628 *rk628 = s->private;
	unsigned int i, j;
	u32 val = 0;

	seq_printf(s, "rk628_%s:\n", file_dentry(s->file)->d_iname);

	for (i = 0; i < ARRAY_SIZE(rk628_regmap_config); i++) {
		reg = &rk628_regmap_config[i];
		if (!reg->name)
			continue;
		if (!strcmp(reg->name, file_dentry(s->file)->d_iname))
			break;
	}

	if (i == ARRAY_SIZE(rk628_regmap_config))
		return -ENODEV;

	/* grf */
	if (!reg->rd_table) {
		for (i = 0; i <= reg->max_register; i += 4) {
			rk628_i2c_read(rk628, i, &val);
			if (i % 16 == 0)
				seq_printf(s, "\n0x%04x:", i);
			seq_printf(s, " %08x", val);
		}
	} else {
		const struct regmap_range *range_list = reg->rd_table->yes_ranges;
		const struct regmap_range *range;
		int range_list_len = reg->rd_table->n_yes_ranges;

		for (i = 0; i < range_list_len; i++) {
			range = &range_list[i];
			for (j = range->range_min; j <= range->range_max; j += 4) {
				rk628_i2c_read(rk628, j, &val);
				if (j % 16 == 0 || j == range->range_min)
					seq_printf(s, "\n0x%04x:", j);
				seq_printf(s, " %08x", val);
			}
		}

		seq_puts(s, "\n");
	}

	return 0;
}

static ssize_t rk628_reg_write(struct file *file, const char __user *buf,
			       size_t count, loff_t *ppos)
{
	struct rk628 *rk628 = file->f_path.dentry->d_inode->i_private;
	u32 addr;
	u32 val;
	char kbuf[25];
	int ret;

	if (count >= sizeof(kbuf))
		return -ENOSPC;

	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;

	kbuf[count] = '\0';

	ret = sscanf(kbuf, "%x%x", &addr, &val);
	if (ret != 2)
		return -EINVAL;

	rk628_i2c_write(rk628, addr, val);

	return count;
}

static int rk628_reg_open(struct inode *inode, struct file *file)
{
	struct rk628 *rk628 = inode->i_private;

	return single_open(file, rk628_reg_show, rk628);
}

static const struct file_operations rk628_reg_fops = {
	.owner          = THIS_MODULE,
	.open           = rk628_reg_open,
	.read           = seq_read,
	.write          = rk628_reg_write,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static void rk628_debugfs_register_create(struct rk628 *rk628)
{
	const struct regmap_config *reg;
	struct dentry *dir;
	int i;

	dir = debugfs_create_dir("registers", rk628->debug_dir);
	if (IS_ERR(dir))
		return;

	for (i = 0; i < ARRAY_SIZE(rk628_regmap_config); i++) {
		reg = &rk628_regmap_config[i];
		if (!reg->name)
			continue;
		debugfs_create_file(reg->name, 0600, dir, rk628, &rk628_reg_fops);
	}
	rk628_hdmirx_phy_debugfs_register_create(rk628, dir);
}

static int rk628_dbg_en_show(struct seq_file *s, void *v)
{
	struct rk628 *rk628 = s->private;

	seq_printf(s, "%d\n", rk628->dbg_en);

	return 0;
}

static ssize_t rk628_dbg_en_write(struct file *file, const char __user *buf,
				  size_t count, loff_t *ppos)
{
	struct rk628 *rk628 = file->f_path.dentry->d_inode->i_private;
	char kbuf[25];
	int enable;

	if (!rk628)
		return -EINVAL;

	if (count >= sizeof(kbuf))
		return -ENOSPC;

	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;

	kbuf[count] = '\0';

	if (kstrtoint(kbuf, 10, &enable))
		return -EINVAL;

	rk628->dbg_en = enable;

	return count;
}

static int rk628_dbg_en_open(struct inode *inode, struct file *file)
{
	struct rk628 *rk628 = inode->i_private;

	return single_open(file, rk628_dbg_en_show, rk628);
}

static const struct file_operations rk628_dbg_en_fops = {
	.owner          = THIS_MODULE,
	.open           = rk628_dbg_en_open,
	.read           = seq_read,
	.write          = rk628_dbg_en_write,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static void rk628_dbg_en_node(struct rk628 *rk628)
{
	debugfs_create_file("debug", 0600, rk628->debug_dir, rk628, &rk628_dbg_en_fops);
}

void rk628_debugfs_create(struct rk628 *rk628)
{
	struct dentry *debugfs, *debugfs_tmp = debugfs_lookup("rk628", NULL);

	debugfs = debugfs_tmp;
	if (!debugfs)
		debugfs = debugfs_create_dir("rk628", NULL);
	dput(debugfs_tmp);
	rk628->debug_dir = debugfs_create_dir(dev_name(rk628->dev), debugfs);
	if (IS_ERR(rk628->debug_dir))
		return;

	rk628_debugfs_register_create(rk628);
	rk628_dbg_en_node(rk628);
	rk628_post_process_pattern_node(rk628);
}
EXPORT_SYMBOL(rk628_debugfs_create);

void rk628_debugfs_remove(struct rk628 *rk628)
{
	debugfs_remove_recursive(rk628->debug_dir);
}
EXPORT_SYMBOL(rk628_debugfs_remove);

struct rk628 *rk628_i2c_register(struct i2c_client *client)
{
	struct rk628 *rk628;
	int i, ret;
	struct device *dev = &client->dev;

	rk628 = devm_kzalloc(dev, sizeof(*rk628), GFP_KERNEL);
	if (!rk628)
		return NULL;

	rk628->client = client;
	rk628->dev = dev;
	for (i = 0; i < RK628_DEV_MAX; i++) {
		const struct regmap_config *config = &rk628_regmap_config[i];

		if (!config->name)
			continue;

		rk628->regmap[i] = devm_regmap_init_i2c(client, config);
		if (IS_ERR(rk628->regmap[i])) {
			ret = PTR_ERR(rk628->regmap[i]);
			dev_err(dev, "failed to allocate register map %d: %d\n",
				i, ret);
			return NULL;
		}
	}
	mutex_init(&rk628->rst_lock);

	return rk628;
}
EXPORT_SYMBOL(rk628_i2c_register);

static void calc_dsp_frm_hst_vst(const struct videomode *src,
				 const struct videomode *dst,
				 u32 *dsp_frame_hst, u32 *dsp_frame_vst)
{
	u32 bp_in, bp_out;
	u32 v_scale_ratio;
	u64 t_frm_st;
	u64 t_bp_in, t_bp_out, t_delta, tin;
	u32 src_pixclock, dst_pixclock;
	u32 dsp_htotal, src_htotal, src_vtotal;

	src_pixclock = div_u64(1000000000000llu, src->pixelclock);
	dst_pixclock = div_u64(1000000000000llu, dst->pixelclock);

	src_htotal = src->hsync_len + src->hback_porch + src->hactive +
		     src->hfront_porch;
	src_vtotal = src->vsync_len + src->vback_porch + src->vactive +
		     src->vfront_porch;
	dsp_htotal = dst->hsync_len + dst->hback_porch + dst->hactive +
		     dst->hfront_porch;

	bp_in = (src->vback_porch + src->vsync_len) * src_htotal +
		src->hsync_len + src->hback_porch;
	bp_out = (dst->vback_porch + dst->vsync_len) * dsp_htotal +
		 dst->hsync_len + dst->hback_porch;

	t_bp_in = bp_in * src_pixclock;
	t_bp_out = bp_out * dst_pixclock;
	tin = src_vtotal * src_htotal * src_pixclock;

	v_scale_ratio = src->vactive / dst->vactive;
	if (v_scale_ratio <= 2)
		t_delta = 5 * src_htotal * src_pixclock;
	else
		t_delta = 12 * src_htotal * src_pixclock;

	if (t_bp_in + t_delta > t_bp_out)
		t_frm_st = (t_bp_in + t_delta - t_bp_out);
	else
		t_frm_st = tin - (t_bp_out - (t_bp_in + t_delta));

	do_div(t_frm_st, src_pixclock);
	*dsp_frame_hst = do_div(t_frm_st, src_htotal);
	if (src->vfront_porch < t_frm_st)
		t_frm_st = src->vfront_porch;
	*dsp_frame_vst = t_frm_st;
}

static void rk628_post_process_scaler_init(struct rk628 *rk628,
					   const struct videomode *src,
					   const struct videomode *dst)
{
	u32 dsp_frame_hst, dsp_frame_vst;
	u32 scl_hor_mode, scl_ver_mode;
	u32 scl_v_factor, scl_h_factor;
	u32 dsp_htotal, dsp_hs_end, dsp_hact_st, dsp_hact_end;
	u32 dsp_vtotal, dsp_vs_end, dsp_vact_st, dsp_vact_end;
	u32 dsp_hbor_end, dsp_hbor_st, dsp_vbor_end, dsp_vbor_st;
	u16 bor_right = 0, bor_left = 0, bor_up = 0, bor_down = 0;
	u8 hor_down_mode = 0, ver_down_mode = 0;

	dsp_htotal = dst->hsync_len + dst->hback_porch + dst->hactive +
		     dst->hfront_porch;
	dsp_vtotal = dst->vsync_len + dst->vback_porch + dst->vactive +
		     dst->vfront_porch;
	dsp_hs_end = dst->hsync_len;
	dsp_vs_end = dst->vsync_len;
	dsp_hbor_end = dst->hsync_len + dst->hback_porch + dst->hactive;
	dsp_hbor_st = dst->hsync_len + dst->hback_porch;
	dsp_vbor_end = dst->vsync_len + dst->vback_porch + dst->vactive;
	dsp_vbor_st = dst->vsync_len + dst->vback_porch;
	dsp_hact_st = dsp_hbor_st + bor_left;
	dsp_hact_end = dsp_hbor_end - bor_right;
	dsp_vact_st = dsp_vbor_st + bor_up;
	dsp_vact_end = dsp_vbor_end - bor_down;

	calc_dsp_frm_hst_vst(src, dst, &dsp_frame_hst, &dsp_frame_vst);
	dev_dbg(rk628->dev, "dsp_frame_vst=%d, dsp_frame_hst=%d\n",
			dsp_frame_vst, dsp_frame_hst);

	if (src->hactive > dst->hactive) {
		scl_hor_mode = 2;

		if (hor_down_mode == 0) {
			if ((src->hactive - 1) / (dst->hactive - 1) > 2)
				scl_h_factor = ((src->hactive - 1) << 14) /
					       (dst->hactive - 1);
			else
				scl_h_factor = ((src->hactive - 2) << 14) /
					       (dst->hactive - 1);
		} else {
			scl_h_factor = (dst->hactive << 16) /
				       (src->hactive - 1);
		}

		dev_dbg(rk628->dev, "horizontal scale down\n");
	} else if (src->hactive == dst->hactive) {
		scl_hor_mode = 0;
		scl_h_factor = 0;

		dev_dbg(rk628->dev, "horizontal no scale\n");
	} else {
		scl_hor_mode = 1;
		scl_h_factor = ((src->hactive - 1) << 16) / (dst->hactive - 1);

		dev_dbg(rk628->dev, "horizontal scale up\n");
	}

	if (src->vactive > dst->vactive) {
		scl_ver_mode = 2;

		if (ver_down_mode == 0) {
			if ((src->vactive - 1) / (dst->vactive - 1) > 2)
				scl_v_factor = ((src->vactive - 1) << 14) /
					       (dst->vactive - 1);
			else
				scl_v_factor = ((src->vactive - 2) << 14) /
					       (dst->vactive - 1);
		} else {
			scl_v_factor = (dst->vactive << 16) /
				       (src->vactive - 1);
		}

		dev_dbg(rk628->dev, "vertical scale down\n");
	} else if (src->vactive == dst->vactive) {
		scl_ver_mode = 0;
		scl_v_factor = 0;

		dev_dbg(rk628->dev, "vertical no scale\n");
	} else {
		scl_ver_mode = 1;
		scl_v_factor = ((src->vactive - 1) << 16) / (dst->vactive - 1);

		dev_dbg(rk628->dev, "vertical scale up\n");
	}

	rk628_i2c_update_bits(rk628, GRF_RGB_DEC_CON0,
			      SW_HRES_MASK, SW_HRES(src->hactive));
	rk628_i2c_write(rk628, GRF_SCALER_CON0,
			SCL_VER_DOWN_MODE(ver_down_mode) |
			SCL_HOR_DOWN_MODE(hor_down_mode) |
			SCL_VER_MODE(scl_ver_mode) |
			SCL_HOR_MODE(scl_hor_mode) |
			SCL_EN(1));
	rk628_i2c_write(rk628, GRF_SCALER_CON1,
			SCL_V_FACTOR(scl_v_factor) |
			SCL_H_FACTOR(scl_h_factor));
	rk628_i2c_write(rk628, GRF_SCALER_CON2,
			DSP_FRAME_VST(dsp_frame_vst) |
			DSP_FRAME_HST(dsp_frame_hst));
	rk628_i2c_write(rk628, GRF_SCALER_CON3,
			DSP_HS_END(dsp_hs_end) |
			DSP_HTOTAL(dsp_htotal));
	rk628_i2c_write(rk628, GRF_SCALER_CON4,
			DSP_HACT_END(dsp_hact_end) |
			DSP_HACT_ST(dsp_hact_st));
	rk628_i2c_write(rk628, GRF_SCALER_CON5,
			DSP_VS_END(dsp_vs_end) |
			DSP_VTOTAL(dsp_vtotal));
	rk628_i2c_write(rk628, GRF_SCALER_CON6,
			DSP_VACT_END(dsp_vact_end) |
			DSP_VACT_ST(dsp_vact_st));
	rk628_i2c_write(rk628, GRF_SCALER_CON7,
			DSP_HBOR_END(dsp_hbor_end) |
			DSP_HBOR_ST(dsp_hbor_st));
	rk628_i2c_write(rk628, GRF_SCALER_CON8,
			DSP_VBOR_END(dsp_vbor_end) |
			DSP_VBOR_ST(dsp_vbor_st));
}

void rk628_post_process_en(struct rk628 *rk628,
			   struct videomode *src,
			   struct videomode *dst,
			   u64 *dst_pclk)
{
	u64 dst_rate, src_rate;
	u64 dst_htotal, dst_vtotal, src_htotal, src_vtotal;

	src_rate = src->pixelclock;
	dst_htotal = dst->hactive + dst->hfront_porch + dst->hsync_len + dst->hback_porch;
	dst_vtotal = dst->vactive + dst->vfront_porch + dst->vsync_len + dst->vback_porch;
	dst_rate = src_rate * dst_vtotal * dst_htotal;
	src_htotal = src->hactive + src->hfront_porch + src->hsync_len + src->hback_porch;
	src_vtotal = src->vactive + src->vfront_porch + src->vsync_len + src->vback_porch;
	do_div(dst_rate, (src_vtotal * src_htotal));
	dst->pixelclock = dst_rate;
	*dst_pclk = dst->pixelclock;

	dev_info(rk628->dev, "src %dx%d clock:%lu\n",
		 src->hactive, src->vactive, src->pixelclock);
	dev_info(rk628->dev, "dst %dx%d clock:%lu\n",
		 dst->hactive, dst->vactive, dst->pixelclock);
	dst->flags = 0;

	rk628_control_assert(rk628, RGU_DECODER);
	udelay(10);
	rk628_control_deassert(rk628, RGU_DECODER);
	udelay(10);

	rk628_clk_set_rate(rk628, CGU_CLK_RX_READ, src->pixelclock);
	rk628_control_assert(rk628, RGU_CLK_RX);
	udelay(10);
	rk628_control_deassert(rk628, RGU_CLK_RX);
	udelay(10);

	rk628_clk_set_rate(rk628, CGU_SCLK_VOP, dst->pixelclock);
	rk628_control_assert(rk628, RGU_VOP);
	udelay(10);
	rk628_control_deassert(rk628, RGU_VOP);
	udelay(10);

	rk628_post_process_scaler_init(rk628, src, dst);
}
EXPORT_SYMBOL(rk628_post_process_en);

static const char * const rk628_version[] = {
	"UNKNOWN",
	"RK628D",
	"RK628F/H",
};

void rk628_version_parse(struct rk628 *rk628)
{
	u32 version;

	rk628_i2c_read(rk628, GRF_SOC_VERSION, &version);
	if (version == 0x20200326)
		rk628->version = RK628D_VERSION;
	else if (version == 0x20230321)
		rk628->version = RK628F_VERSION;
	else
		rk628->version = RK628_UNKNOWN;

	dev_info(rk628->dev, "rk628 version is: %s (%x)\n",
		 rk628_version[rk628->version], version);
}
EXPORT_SYMBOL(rk628_version_parse);

MODULE_AUTHOR("Shunqing Chen <csq@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip RK628 driver");
MODULE_LICENSE("GPL");
