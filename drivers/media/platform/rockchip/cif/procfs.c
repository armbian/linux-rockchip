// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) Rockchip Electronics Co., Ltd. */
#include <linux/clk.h>
#include <linux/math64.h>
#include <linux/proc_fs.h>
#include <linux/sem.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/v4l2-mediabus.h>
#include <linux/regmap.h>

#include "dev.h"
#include "procfs.h"
#include "mipi-csi2.h"
#include "../../../../phy/rockchip/phy-rockchip-csi2-dphy-common.h"
#include "../../../../phy/rockchip/phy-rockchip-samsung-dcphy.h"

#ifdef CONFIG_PROC_FS

static const struct {
	const char *name;
	u32 mbus_code;
} mbus_formats[] = {
	/* media bus code */
	{ "RGB444_1X12", MEDIA_BUS_FMT_RGB444_1X12 },
	{ "RGB444_2X8_PADHI_BE", MEDIA_BUS_FMT_RGB444_2X8_PADHI_BE },
	{ "RGB444_2X8_PADHI_LE", MEDIA_BUS_FMT_RGB444_2X8_PADHI_LE },
	{ "RGB555_2X8_PADHI_BE", MEDIA_BUS_FMT_RGB555_2X8_PADHI_BE },
	{ "RGB555_2X8_PADHI_LE", MEDIA_BUS_FMT_RGB555_2X8_PADHI_LE },
	{ "RGB565_1X16", MEDIA_BUS_FMT_RGB565_1X16 },
	{ "BGR565_2X8_BE", MEDIA_BUS_FMT_BGR565_2X8_BE },
	{ "BGR565_2X8_LE", MEDIA_BUS_FMT_BGR565_2X8_LE },
	{ "RGB565_2X8_BE", MEDIA_BUS_FMT_RGB565_2X8_BE },
	{ "RGB565_2X8_LE", MEDIA_BUS_FMT_RGB565_2X8_LE },
	{ "RGB666_1X18", MEDIA_BUS_FMT_RGB666_1X18 },
	{ "RBG888_1X24", MEDIA_BUS_FMT_RBG888_1X24 },
	{ "RGB666_1X24_CPADHI", MEDIA_BUS_FMT_RGB666_1X24_CPADHI },
	{ "RGB666_1X7X3_SPWG", MEDIA_BUS_FMT_RGB666_1X7X3_SPWG },
	{ "BGR888_1X24", MEDIA_BUS_FMT_BGR888_1X24 },
	{ "GBR888_1X24", MEDIA_BUS_FMT_GBR888_1X24 },
	{ "RGB888_1X24", MEDIA_BUS_FMT_RGB888_1X24 },
	{ "RGB888_2X12_BE", MEDIA_BUS_FMT_RGB888_2X12_BE },
	{ "RGB888_2X12_LE", MEDIA_BUS_FMT_RGB888_2X12_LE },
	{ "RGB888_1X7X4_SPWG", MEDIA_BUS_FMT_RGB888_1X7X4_SPWG },
	{ "RGB888_1X7X4_JEIDA", MEDIA_BUS_FMT_RGB888_1X7X4_JEIDA },
	{ "ARGB8888_1X32", MEDIA_BUS_FMT_ARGB8888_1X32 },
	{ "RGB888_1X32_PADHI", MEDIA_BUS_FMT_RGB888_1X32_PADHI },
	{ "RGB101010_1X30", MEDIA_BUS_FMT_RGB101010_1X30 },
	{ "RGB121212_1X36", MEDIA_BUS_FMT_RGB121212_1X36 },
	{ "RGB161616_1X48", MEDIA_BUS_FMT_RGB161616_1X48 },
	{ "Y8_1X8", MEDIA_BUS_FMT_Y8_1X8 },
	{ "UV8_1X8", MEDIA_BUS_FMT_UV8_1X8 },
	{ "UYVY8_1_5X8", MEDIA_BUS_FMT_UYVY8_1_5X8 },
	{ "VYUY8_1_5X8", MEDIA_BUS_FMT_VYUY8_1_5X8 },
	{ "YUYV8_1_5X8", MEDIA_BUS_FMT_YUYV8_1_5X8 },
	{ "YVYU8_1_5X8", MEDIA_BUS_FMT_YVYU8_1_5X8 },
	{ "UYVY8_2X8", MEDIA_BUS_FMT_UYVY8_2X8 },
	{ "VYUY8_2X8", MEDIA_BUS_FMT_VYUY8_2X8 },
	{ "YUYV8_2X8", MEDIA_BUS_FMT_YUYV8_2X8 },
	{ "YVYU8_2X8", MEDIA_BUS_FMT_YVYU8_2X8 },
	{ "Y10_1X10", MEDIA_BUS_FMT_Y10_1X10 },
	{ "Y10_2X8_PADHI_LE", MEDIA_BUS_FMT_Y10_2X8_PADHI_LE },
	{ "UYVY10_2X10", MEDIA_BUS_FMT_UYVY10_2X10 },
	{ "VYUY10_2X10", MEDIA_BUS_FMT_VYUY10_2X10 },
	{ "YUYV10_2X10", MEDIA_BUS_FMT_YUYV10_2X10 },
	{ "YVYU10_2X10", MEDIA_BUS_FMT_YVYU10_2X10 },
	{ "Y12_1X12", MEDIA_BUS_FMT_Y12_1X12 },
	{ "UYVY12_2X12", MEDIA_BUS_FMT_UYVY12_2X12 },
	{ "VYUY12_2X12", MEDIA_BUS_FMT_VYUY12_2X12 },
	{ "YUYV12_2X12", MEDIA_BUS_FMT_YUYV12_2X12 },
	{ "YVYU12_2X12", MEDIA_BUS_FMT_YVYU12_2X12 },
	{ "UYVY8_1X16", MEDIA_BUS_FMT_UYVY8_1X16 },
	{ "VYUY8_1X16", MEDIA_BUS_FMT_VYUY8_1X16 },
	{ "YUYV8_1X16", MEDIA_BUS_FMT_YUYV8_1X16 },
	{ "YVYU8_1X16", MEDIA_BUS_FMT_YVYU8_1X16 },
	{ "YDYUYDYV8_1X16", MEDIA_BUS_FMT_YDYUYDYV8_1X16 },
	{ "UYVY10_1X20", MEDIA_BUS_FMT_UYVY10_1X20 },
	{ "VYUY10_1X20", MEDIA_BUS_FMT_VYUY10_1X20 },
	{ "YUYV10_1X20", MEDIA_BUS_FMT_YUYV10_1X20 },
	{ "YVYU10_1X20", MEDIA_BUS_FMT_YVYU10_1X20 },
	{ "VUY8_1X24", MEDIA_BUS_FMT_VUY8_1X24 },
	{ "YUV8_1X24", MEDIA_BUS_FMT_YUV8_1X24 },
	{ "UYYVYY8_0_5X24", MEDIA_BUS_FMT_UYYVYY8_0_5X24 },
	{ "UYVY12_1X24", MEDIA_BUS_FMT_UYVY12_1X24 },
	{ "VYUY12_1X24", MEDIA_BUS_FMT_VYUY12_1X24 },
	{ "YUYV12_1X24", MEDIA_BUS_FMT_YUYV12_1X24 },
	{ "YVYU12_1X24", MEDIA_BUS_FMT_YVYU12_1X24 },
	{ "YUV10_1X30", MEDIA_BUS_FMT_YUV10_1X30 },
	{ "UYYVYY10_0_5X30", MEDIA_BUS_FMT_UYYVYY10_0_5X30 },
	{ "AYUV8_1X32", MEDIA_BUS_FMT_AYUV8_1X32 },
	{ "UYYVYY12_0_5X36", MEDIA_BUS_FMT_UYYVYY12_0_5X36 },
	{ "YUV12_1X36", MEDIA_BUS_FMT_YUV12_1X36 },
	{ "YUV16_1X48", MEDIA_BUS_FMT_YUV16_1X48 },
	{ "UYYVYY16_0_5X48", MEDIA_BUS_FMT_UYYVYY16_0_5X48 },
	{ "SBGGR8_1X8", MEDIA_BUS_FMT_SBGGR8_1X8 },
	{ "SGBRG8_1X8", MEDIA_BUS_FMT_SGBRG8_1X8 },
	{ "SGRBG8_1X8", MEDIA_BUS_FMT_SGRBG8_1X8 },
	{ "SRGGB8_1X8", MEDIA_BUS_FMT_SRGGB8_1X8 },
	{ "SBGGR10_ALAW8_1X8", MEDIA_BUS_FMT_SBGGR10_ALAW8_1X8 },
	{ "SGBRG10_ALAW8_1X8", MEDIA_BUS_FMT_SGBRG10_ALAW8_1X8 },
	{ "SGRBG10_ALAW8_1X8", MEDIA_BUS_FMT_SGRBG10_ALAW8_1X8 },
	{ "SRGGB10_ALAW8_1X8", MEDIA_BUS_FMT_SRGGB10_ALAW8_1X8 },
	{ "SBGGR10_DPCM8_1X8", MEDIA_BUS_FMT_SBGGR10_DPCM8_1X8 },
	{ "SGBRG10_DPCM8_1X8", MEDIA_BUS_FMT_SGBRG10_DPCM8_1X8 },
	{ "SGRBG10_DPCM8_1X8", MEDIA_BUS_FMT_SGRBG10_DPCM8_1X8 },
	{ "SRGGB10_DPCM8_1X8", MEDIA_BUS_FMT_SRGGB10_DPCM8_1X8 },
	{ "SBGGR10_2X8_PADHI_BE", MEDIA_BUS_FMT_SBGGR10_2X8_PADHI_BE },
	{ "SBGGR10_2X8_PADHI_LE", MEDIA_BUS_FMT_SBGGR10_2X8_PADHI_LE },
	{ "SBGGR10_2X8_PADLO_BE", MEDIA_BUS_FMT_SBGGR10_2X8_PADLO_BE },
	{ "SBGGR10_2X8_PADLO_LE", MEDIA_BUS_FMT_SBGGR10_2X8_PADLO_LE },
	{ "SBGGR10_1X10", MEDIA_BUS_FMT_SBGGR10_1X10 },
	{ "SGBRG10_1X10", MEDIA_BUS_FMT_SGBRG10_1X10 },
	{ "SGRBG10_1X10", MEDIA_BUS_FMT_SGRBG10_1X10 },
	{ "SRGGB10_1X10", MEDIA_BUS_FMT_SRGGB10_1X10 },
	{ "SBGGR12_1X12", MEDIA_BUS_FMT_SBGGR12_1X12 },
	{ "SGBRG12_1X12", MEDIA_BUS_FMT_SGBRG12_1X12 },
	{ "SGRBG12_1X12", MEDIA_BUS_FMT_SGRBG12_1X12 },
	{ "SRGGB12_1X12", MEDIA_BUS_FMT_SRGGB12_1X12 },
	{ "SBGGR14_1X14", MEDIA_BUS_FMT_SBGGR14_1X14 },
	{ "SGBRG14_1X14", MEDIA_BUS_FMT_SGBRG14_1X14 },
	{ "SGRBG14_1X14", MEDIA_BUS_FMT_SGRBG14_1X14 },
	{ "SRGGB14_1X14", MEDIA_BUS_FMT_SRGGB14_1X14 },
	{ "SBGGR16_1X16", MEDIA_BUS_FMT_SBGGR16_1X16 },
	{ "SGBRG16_1X16", MEDIA_BUS_FMT_SGBRG16_1X16 },
	{ "SGRBG16_1X16", MEDIA_BUS_FMT_SGRBG16_1X16 },
	{ "SRGGB16_1X16", MEDIA_BUS_FMT_SRGGB16_1X16 },
	{ "JPEG_1X8", MEDIA_BUS_FMT_JPEG_1X8 },
	{ "S5C_UYVY_JPEG_1X8", MEDIA_BUS_FMT_S5C_UYVY_JPEG_1X8 },
	{ "AHSV8888_1X32", MEDIA_BUS_FMT_AHSV8888_1X32 },
	{ "FIXED", MEDIA_BUS_FMT_FIXED },
	{ "Y8", MEDIA_BUS_FMT_Y8_1X8 },
	{ "Y10", MEDIA_BUS_FMT_Y10_1X10 },
	{ "Y12", MEDIA_BUS_FMT_Y12_1X12 },
	{ "YUYV", MEDIA_BUS_FMT_YUYV8_1X16 },
	{ "YUYV1_5X8", MEDIA_BUS_FMT_YUYV8_1_5X8 },
	{ "YUYV2X8", MEDIA_BUS_FMT_YUYV8_2X8 },
	{ "UYVY", MEDIA_BUS_FMT_UYVY8_1X16 },
	{ "UYVY1_5X8", MEDIA_BUS_FMT_UYVY8_1_5X8 },
	{ "UYVY2X8", MEDIA_BUS_FMT_UYVY8_2X8 },
	{ "VUY24", MEDIA_BUS_FMT_VUY8_1X24 },
	{ "SBGGR8", MEDIA_BUS_FMT_SBGGR8_1X8 },
	{ "SGBRG8", MEDIA_BUS_FMT_SGBRG8_1X8 },
	{ "SGRBG8", MEDIA_BUS_FMT_SGRBG8_1X8 },
	{ "SRGGB8", MEDIA_BUS_FMT_SRGGB8_1X8 },
	{ "SBGGR10", MEDIA_BUS_FMT_SBGGR10_1X10 },
	{ "SGBRG10", MEDIA_BUS_FMT_SGBRG10_1X10 },
	{ "SGRBG10", MEDIA_BUS_FMT_SGRBG10_1X10 },
	{ "SRGGB10", MEDIA_BUS_FMT_SRGGB10_1X10 },
	{ "SBGGR10_DPCM8", MEDIA_BUS_FMT_SBGGR10_DPCM8_1X8 },
	{ "SGBRG10_DPCM8", MEDIA_BUS_FMT_SGBRG10_DPCM8_1X8 },
	{ "SGRBG10_DPCM8", MEDIA_BUS_FMT_SGRBG10_DPCM8_1X8 },
	{ "SRGGB10_DPCM8", MEDIA_BUS_FMT_SRGGB10_DPCM8_1X8 },
	{ "SBGGR12", MEDIA_BUS_FMT_SBGGR12_1X12 },
	{ "SGBRG12", MEDIA_BUS_FMT_SGBRG12_1X12 },
	{ "SGRBG12", MEDIA_BUS_FMT_SGRBG12_1X12 },
	{ "SRGGB12", MEDIA_BUS_FMT_SRGGB12_1X12 },
	{ "AYUV32", MEDIA_BUS_FMT_AYUV8_1X32 },
	{ "RBG24", MEDIA_BUS_FMT_RBG888_1X24 },
	{ "RGB32", MEDIA_BUS_FMT_RGB888_1X32_PADHI },
	{ "ARGB32", MEDIA_BUS_FMT_ARGB8888_1X32 },

	/* v4l2 fourcc code */
	{ "NV16", V4L2_PIX_FMT_NV16},
	{ "NV61", V4L2_PIX_FMT_NV61},
	{ "NV12", V4L2_PIX_FMT_NV12},
	{ "NV21", V4L2_PIX_FMT_NV21},
	{ "YUYV", V4L2_PIX_FMT_YUYV},
	{ "YVYU", V4L2_PIX_FMT_YVYU},
	{ "UYVY", V4L2_PIX_FMT_UYVY},
	{ "VYUY", V4L2_PIX_FMT_VYUY},
	{ "RGB3", V4L2_PIX_FMT_RGB24},
	{ "RGBP", V4L2_PIX_FMT_RGB565},
	{ "BGRH", V4L2_PIX_FMT_BGR666},
	{ "RGGB", V4L2_PIX_FMT_SRGGB8},
	{ "GRBG", V4L2_PIX_FMT_SGRBG8},
	{ "GBRG", V4L2_PIX_FMT_SGBRG8},
	{ "BA81", V4L2_PIX_FMT_SBGGR8},
	{ "RG10", V4L2_PIX_FMT_SRGGB10},
	{ "BA10", V4L2_PIX_FMT_SGRBG10},
	{ "GB10", V4L2_PIX_FMT_SGBRG10},
	{ "BG10", V4L2_PIX_FMT_SBGGR10},
	{ "RG12", V4L2_PIX_FMT_SRGGB12},
	{ "BA12", V4L2_PIX_FMT_SGRBG12},
	{ "GB12", V4L2_PIX_FMT_SGBRG12},
	{ "BG12", V4L2_PIX_FMT_SBGGR12},
	{ "BYR2", V4L2_PIX_FMT_SBGGR16},
	{ "Y16 ", V4L2_PIX_FMT_Y16},
};

static const char *rkcif_pixelcode_to_string(u32 mbus_code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(mbus_formats); ++i) {
		if (mbus_formats[i].mbus_code == mbus_code)
			return mbus_formats[i].name;
	}

	return "unknown";
}

static const char *rkcif_get_monitor_mode(enum rkcif_monitor_mode monitor_mode)
{
	switch (monitor_mode) {
	case RKCIF_MONITOR_MODE_IDLE:
		return "idle";
	case RKCIF_MONITOR_MODE_CONTINUE:
		return "continue";
	case RKCIF_MONITOR_MODE_TRIGGER:
		return "trigger";
	case RKCIF_MONITOR_MODE_HOTPLUG:
		return "hotplug";
	default:
		return "unknown";
	}
}

static const char *rkcif_get_sync_mode(enum rkmodule_sync_mode sync_mode)
{
	switch (sync_mode) {
	case NO_SYNC_MODE:
		return "no sync";
	case EXTERNAL_MASTER_MODE:
		return "external master";
	case INTERNAL_MASTER_MODE:
		return "internal master";
	case SLAVE_MODE:
		return "slave";
	case SOFT_SYNC_MODE:
		return "soft sync";
	default:
		return "unknown";
	}
}

static void rkcif_show_mixed_info(struct rkcif_device *dev, struct seq_file *f)
{
	enum rkcif_monitor_mode monitor_mode;

	seq_printf(f, "Driver Version:v%02x.%02x.%02x\n",
		   RKCIF_DRIVER_VERSION >> 16,
		   (RKCIF_DRIVER_VERSION & 0xff00) >> 8,
		    RKCIF_DRIVER_VERSION & 0x00ff);
	seq_printf(f, "Work Mode:%s\n",
		   dev->workmode == RKCIF_WORKMODE_ONEFRAME ? "one frame" :
		   dev->workmode == RKCIF_WORKMODE_PINGPONG ? "ping pong" : "line loop");

	monitor_mode = dev->reset_watchdog_timer.monitor_mode;
	seq_printf(f, "Monitor Mode:%s\n",
		   rkcif_get_monitor_mode(monitor_mode));
}

static void rkcif_show_clks(struct rkcif_device *dev, struct seq_file *f)
{
	int i;
	struct rkcif_hw *hw = dev->hw_dev;

	for (i = 0; i < hw->clk_size; i++) {
		seq_printf(f, "%s:%ld\n",
			   hw->match_data->clks[i],
			   clk_get_rate(hw->clks[i]));
	}
}

static void rkcif_show_toisp_info(struct rkcif_device *dev, struct seq_file *f)
{
	struct sditf_priv *priv = dev->sditf[0];
	char name_strings[32] = {0};

	seq_puts(f, "\nToisp Info:\n");
	seq_printf(f, "\tisp_name: %s\n", priv->mode.name);
	switch (priv->mode.rdbk_mode) {
	case RKISP_VICAP_ONLINE:
		sprintf(name_strings, "%s", "online");
		break;
	case RKISP_VICAP_ONLINE_ONE_FRAME:
		sprintf(name_strings, "%s", "online_one_frame");
		break;
	case RKISP_VICAP_ONLINE_MULTI:
		sprintf(name_strings, "%s", "online_multi");
		break;
	case RKISP_VICAP_ONLINE_UNITE:
		sprintf(name_strings, "%s", "online_unite");
		break;
	case RKISP_VICAP_RDBK_AIQ:
		sprintf(name_strings, "%s", "rdbk_aiq");
		break;
	case RKISP_VICAP_RDBK_AUTO:
		sprintf(name_strings, "%s", "rdbk_auto");
		break;
	case RKISP_VICAP_RDBK_AUTO_ONE_FRAME:
		sprintf(name_strings, "%s", "rdbk_one_frame");
		break;
	default:
		sprintf(name_strings, "%s", "error");
		break;
	}
	seq_printf(f, "\trdbk_mode: %s\n", name_strings);
	if (priv->mode.rdbk_mode > RKISP_VICAP_RDBK_AIQ)
		seq_printf(f, "\treserved buf: %d\n", priv->cif_dev->fb_res_bufs);
	if (priv->hdr_wrap_line)
		seq_printf(f, "\twrap line: %d\n", priv->hdr_wrap_line);
	if (priv->is_combine_mode)
		seq_printf(f, "\tcombine num: %d\n", dev->sditf_cnt);
}

static int rkcif_get_csi_offset(struct rkcif_device *dev, int csi_host_idx)
{
	int csi_offset = 0;

	if (dev->chip_id == CHIP_RK3588_CIF) {
		csi_offset = csi_host_idx * 0x100;
	} else if (dev->chip_id == CHIP_RV1106_CIF ||
		   dev->chip_id == CHIP_RV1103B_CIF) {
		csi_offset = csi_host_idx * 0x200;
	} else if (dev->chip_id == CHIP_RK3562_CIF) {
		if (csi_host_idx < 3)
			csi_offset = csi_host_idx * 0x200;
		else
			csi_offset = 0x500;
	} else if (dev->chip_id == CHIP_RK3576_CIF) {
		if (csi_host_idx < 2)
			csi_offset = csi_host_idx * 0x200;
		else
			csi_offset = 0x100 + csi_host_idx * 0x100;
	}
	csi_offset += 0x100;
	return csi_offset;
}

static void rkcif_show_reg_vicap(struct rkcif_device *dev, struct seq_file *f)
{
	void *buf = dev->sw_reg;
	struct sditf_priv *priv = dev->sditf[0];
	u32 *reg = NULL;
	int i, j;
	int csi_offset = 0;

	seq_puts(f, "\nVicap reg:\n");
	memcpy_fromio(buf, dev->hw_dev->base_addr, RKCIF_REG_MAX);
	if (dev->reg_dbg == RKCIF_REG_DBG_PART) {
		if (dev->inf_id == RKCIF_MIPI_LVDS) {
			for (j = 0; j < dev->csi_info.csi_num; j++) {
				seq_printf(f, "\nmipi%d reg:\n", dev->csi_info.csi_idx[j]);
				csi_offset = rkcif_get_csi_offset(dev, dev->csi_info.csi_idx[j]);
				reg = buf + csi_offset;
				for (i = 0; i < 0x100 / 16; i++)
					seq_printf(f, "0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
						   (u32)(dev->hw_dev->res->start + csi_offset + i * 16),
						   *(reg + i * 4), *(reg + i * 4 + 1),
						   *(reg + i * 4 + 2), *(reg + i  * 4 + 3));
			}
		} else {
			reg = buf;
			for (i = 0; i < 0x100 / 16; i++)
				seq_printf(f, "0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
					   (u32)(dev->hw_dev->res->start + i * 16),
					   *(reg + i * 4), *(reg + i * 4 + 1),
					   *(reg + i * 4 + 2), *(reg + i  * 4 + 3));
		}
		if (priv && priv->mode.rdbk_mode < RKISP_VICAP_RDBK_AIQ) {
			seq_puts(f, "\ntoisp reg:\n");
			reg = buf + 0x780;
			for (i = 0; i < 0x30 / 16; i++)
				seq_printf(f, "0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
					   (u32)(dev->hw_dev->res->start + 0x780 + i * 16),
					   *(reg + i * 4), *(reg + i * 4 + 1),
					   *(reg + i * 4 + 2), *(reg + i  * 4 + 3));
			reg = buf;
			seq_puts(f, "\nglobal reg:\n");
			for (i = 0; i < 0x10 / 16; i++)
				seq_printf(f, "0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
					   (u32)(dev->hw_dev->res->start + i * 16),
					   *(reg + i * 4), *(reg + i * 4 + 1),
					   *(reg + i * 4 + 2), *(reg + i  * 4 + 3));
		}
	} else {
		reg = buf;
		for (i = 0; i < RKCIF_REG_MAX / 16; i++)
			seq_printf(f, "0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				   (u32)(dev->hw_dev->res->start + i * 16),
				   *(reg + i * 4), *(reg + i * 4 + 1),
				   *(reg + i * 4 + 2), *(reg + i  * 4 + 3));
	}
}

static void rkcif_show_reg_csi2(struct rkcif_device *dev, struct seq_file *f)
{
	struct csi2_dev *csi2 = container_of(dev->active_sensor->sd, struct csi2_dev, sd);
	struct csi2_hw *csi2_hw = NULL;
	int i, j;
	int csi_idx = 0;
	u32 buf[24];

	for (j = 0; j < csi2->csi_info.csi_num; j++) {
		csi_idx = csi2->csi_info.csi_idx[j];
		csi2_hw = csi2->csi2_hw[csi_idx];
		seq_printf(f, "\nmipi%d csi2 reg:\n", csi_idx);
		memcpy_fromio(buf, csi2_hw->base, 0x60);
		for (i = 0; i < 0x60 / 16; i++)
			seq_printf(f, "0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				   (u32)(csi2_hw->res->start + i * 16),
				   *(buf + i * 4), *(buf + i * 4 + 1),
				   *(buf + i * 4 + 2), *(buf + i  * 4 + 3));
	}
}

static void rkcif_show_reg_dcphy(struct samsung_mipi_dcphy *dcphy, int index, struct seq_file *f)
{
	u32 bias_reg[4];
	char *dcphy_reg = NULL;
	u32 *buf = NULL;
	int i = 0;

	seq_printf(f, "\ndcphy%d reg:\n", index);
	regmap_bulk_read(dcphy->regmap, 0, bias_reg, 4);
	seq_printf(f, "0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
		   (u32)dcphy->res->start,
		   bias_reg[0], bias_reg[1],
		   bias_reg[2], bias_reg[3]);

	dcphy_reg = kzalloc(0x500, GFP_KERNEL);
	if (!dcphy_reg)
		return;
	regmap_bulk_read(dcphy->regmap, 0xb00, dcphy_reg, 0x500 / 4);
	buf = (u32 *)dcphy_reg;
	for (i = 0; i < 0x500 / 16; i++)
		seq_printf(f, "0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			   (u32)(dcphy->res->start + 0xb00 + i * 16),
			   *(buf + i * 4), *(buf + i * 4 + 1),
			   *(buf + i * 4 + 2), *(buf + i  * 4 + 3));
	kfree(dcphy_reg);
}

static void rkcif_show_reg_dphy(struct csi2_dphy_hw *dphy_hw, int index, struct seq_file *f)
{
	char *dphy_reg = NULL;
	u32 *buf = NULL;
	int i = 0;

	seq_printf(f, "\ndphy%d reg:\n", index);
	dphy_reg = kzalloc(0x900, GFP_KERNEL);
	if (!dphy_reg)
		return;
	memcpy_fromio(dphy_reg, dphy_hw->hw_base_addr, 0x900);
	buf = (u32 *)dphy_reg;
	for (i = 0; i < 0x900 / 16; i++)
		seq_printf(f, "0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			   (u32)(dphy_hw->res->start + i * 16),
			   *(buf + i * 4), *(buf + i * 4 + 1),
			   *(buf + i * 4 + 2), *(buf + i  * 4 + 3));
	kfree(dphy_reg);
}

static void rkcif_show_reg_dphys(struct rkcif_device *dev, struct seq_file *f)
{
	struct csi2_dphy *dphy = NULL;
	struct csi2_dphy_hw *dphy_hw = NULL;
	struct samsung_mipi_dcphy *dcphy_hw = NULL;
	struct rkcif_pipeline *p = NULL;
	struct v4l2_subdev *sd = NULL;
	int j;
	int csi_idx = 0;

	p = &dev->pipe;
	for (j = 0; j < p->num_subdevs; j++) {
		if (p->subdevs[j] != dev->terminal_sensor.sd &&
		    p->subdevs[j] != dev->active_sensor->sd) {
			sd = p->subdevs[j];
			break;
		}
	}
	if (!sd)
		return;
	dphy = container_of(sd, struct csi2_dphy, sd);

	for (j = 0; j < dev->csi_info.csi_num; j++) {
		csi_idx = dev->csi_info.csi_idx[j];
		if (dphy->drv_data->chip_id == CHIP_ID_RK3568 ||
		    dphy->drv_data->chip_id == CHIP_ID_RV1106) {
			dphy_hw = dphy->dphy_hw_group[0];
			if (dphy_hw)
				rkcif_show_reg_dphy(dphy_hw, 0, f);
		} else if (dphy->drv_data->chip_id == CHIP_ID_RK3588) {
			if (csi_idx < 2) {
				dcphy_hw = dphy->samsung_phy_group[csi_idx];
				if (dcphy_hw)
					rkcif_show_reg_dcphy(dcphy_hw, csi_idx, f);
			} else {
				dphy_hw = dphy->dphy_hw_group[(csi_idx - 2) / 2];
				if (dphy_hw)
					rkcif_show_reg_dphy(dphy_hw, (csi_idx - 2) / 2, f);
			}
		} else if (dphy->drv_data->chip_id == CHIP_ID_RK3576) {
			if (csi_idx < 1) {
				dcphy_hw = dphy->samsung_phy_group[csi_idx];
				if (dcphy_hw)
					rkcif_show_reg_dcphy(dcphy_hw, csi_idx, f);
			} else {
				dphy_hw = dphy->dphy_hw_group[(csi_idx - 1) / 2];
				if (dphy_hw)
					rkcif_show_reg_dphy(dphy_hw, (csi_idx - 1) / 2, f);
			}
		} else {
			dphy_hw = dphy->dphy_hw_group[csi_idx / 2];
			if (dphy_hw)
				rkcif_show_reg_dphy(dphy_hw, csi_idx / 2, f);
		}
	}
}

static void rkcif_show_reg_dbg(struct rkcif_device *dev, struct seq_file *f)
{
	int i;

	rkcif_show_reg_vicap(dev, f);
	if (dev->inf_id == RKCIF_MIPI_LVDS) {
		if (dev->active_sensor->mbus.type == V4L2_MBUS_CSI2_DPHY ||
		    dev->active_sensor->mbus.type == V4L2_MBUS_CSI2_CPHY) {
			for (i = 0; i < 10; i++) {
				rkcif_show_reg_csi2(dev, f);
				usleep_range(2000, 4000);
			}
		}
		rkcif_show_reg_dphys(dev, f);
	}
}

static void rkcif_show_format(struct rkcif_device *dev, struct seq_file *f)
{
	struct rkcif_stream *stream = &dev->stream[0];
	struct rkcif_pipeline *pipe = &dev->pipe;
	struct rkcif_sensor_info *sensor = &dev->terminal_sensor;
	struct v4l2_rect *rect = &sensor->raw_rect;
	struct v4l2_subdev_frame_interval *interval = &sensor->src_fi;
	struct v4l2_subdev_selection *sel = &sensor->selection;
	u32 mbus_flags;
	u64 fps, timestamp0, timestamp1;
	unsigned long flags;
	u32 time_val = 0;
	u32 remainder = 0;

	if (atomic_read(&pipe->stream_cnt) < 1)
		return;

	if (sensor) {
		seq_puts(f, "Input Info:\n");

		seq_printf(f, "\tsrc subdev:%s\n", sensor->sd->name);
		if (sensor->mbus.type == V4L2_MBUS_PARALLEL ||
		    sensor->mbus.type == V4L2_MBUS_BT656) {
			mbus_flags = sensor->mbus.bus.parallel.flags;
			seq_printf(f, "\tinterface:%s\n",
				   sensor->mbus.type == V4L2_MBUS_PARALLEL ? "BT601" : "BT656/BT1120");
			seq_printf(f, "\thref_pol:%s\n",
				   mbus_flags & V4L2_MBUS_HSYNC_ACTIVE_HIGH ? "high active" : "low active");
			seq_printf(f, "\tvsync_pol:%s\n",
				   mbus_flags & V4L2_MBUS_VSYNC_ACTIVE_HIGH ? "high active" : "low active");
		} else {
			seq_printf(f, "\tinterface:%s\n",
				   sensor->mbus.type == V4L2_MBUS_CSI2_DPHY ? "mipi csi2 dphy" :
				   sensor->mbus.type == V4L2_MBUS_CSI2_CPHY ? "mipi csi2 cphy" :
				   sensor->mbus.type == V4L2_MBUS_CCP2 ? "lvds" : "unknown");
			seq_printf(f, "\tlanes:%d\n", sensor->lanes);
		}

		seq_printf(f, "\thdr mode: %s\n",
			   dev->hdr.hdr_mode == NO_HDR ? "normal" :
			   dev->hdr.hdr_mode == HDR_COMPR ? "hdr_compr" :
			   dev->hdr.hdr_mode == HDR_X2 ? "hdr_x2" : "hdr_x3");

		seq_printf(f, "\tformat:%s/%ux%u@%d\n",
			   rkcif_pixelcode_to_string(stream->cif_fmt_in->mbus_code),
			   rect->width, rect->height,
			   interval->interval.denominator / interval->interval.numerator);

		seq_printf(f, "\tcrop.bounds:(%u, %u)/%ux%u\n",
			   sel->r.left, sel->r.top,
			   sel->r.width, sel->r.height);

		spin_lock_irqsave(&stream->fps_lock, flags);
		timestamp0 = stream->fps_stats.frm0_timestamp;
		timestamp1 = stream->fps_stats.frm1_timestamp;
		spin_unlock_irqrestore(&stream->fps_lock, flags);
		if (dev->sditf[0] && dev->sditf[0]->mode.rdbk_mode < RKISP_VICAP_RDBK_AIQ)
			fps = dev->stream[0].readout.total_time;
		else
			fps = timestamp0 > timestamp1 ?
			      timestamp0 - timestamp1 : timestamp1 - timestamp0;
		fps = div_u64(fps, 1000);

		seq_puts(f, "Output Info:\n");
		seq_printf(f, "\tformat:%s/%ux%u(%u,%u)\n",
			   rkcif_pixelcode_to_string(stream->cif_fmt_out->fourcc),
			   dev->channels[0].width, dev->channels[0].height,
			   dev->channels[0].crop_st_x, dev->channels[0].crop_st_y);
		seq_printf(f, "\tcompact:%s\n", stream->is_compact ? "enable" : "disabled");
		seq_printf(f, "\tframe amount:%d\n", stream->frame_idx - 1);
		if (dev->inf_id == RKCIF_MIPI_LVDS) {
			time_val = div_u64(stream->readout.early_time, 1000);
			time_val = div_u64_rem(time_val, 1000, &remainder);
			seq_printf(f, "\tearly:%u.%u ms\n", time_val, remainder);
			if (dev->hdr.hdr_mode == NO_HDR ||
			    dev->hdr.hdr_mode == HDR_COMPR) {
				time_val = div_u64(stream->readout.readout_time, 1000);
				time_val = div_u64_rem(time_val, 1000, &remainder);
				seq_printf(f, "\tsingle readout:%u.%u ms\n", time_val, remainder);
			} else {
				time_val = div_u64(stream->readout.readout_time, 1000);
				time_val = div_u64_rem(time_val, 1000, &remainder);
				seq_printf(f, "\tsingle readout:%u.%u ms\n", time_val, remainder);
				time_val = div_u64(stream->readout.total_time, 1000);
				time_val = div_u64_rem(time_val, 1000, &remainder);
				seq_printf(f, "\ttotal readout:%u.%u ms\n", time_val, remainder);

			}
		}
		time_val = div_u64_rem(fps, 1000, &remainder);
		seq_printf(f, "\trate:%u.%u ms\n", time_val, remainder);
		fps = div_u64(1000000, fps);
		seq_printf(f, "\tfps:%llu\n", fps);
		seq_puts(f, "\tirq statistics:\n");
		seq_printf(f, "\t\t\ttotal:%llu\n",
			   dev->irq_stats.frm_end_cnt[0] +
			   dev->irq_stats.frm_end_cnt[1] +
			   dev->irq_stats.frm_end_cnt[2] +
			   dev->irq_stats.frm_end_cnt[3] +
			   dev->irq_stats.all_err_cnt);
		if (sensor->mbus.type == V4L2_MBUS_PARALLEL ||
		    sensor->mbus.type == V4L2_MBUS_BT656) {
			seq_printf(f, "\t\t\tdvp bus err:%llu\n", dev->irq_stats.dvp_bus_err_cnt);
			seq_printf(f, "\t\t\tdvp pix err:%llu\n", dev->irq_stats.dvp_pix_err_cnt);
			seq_printf(f, "\t\t\tdvp line err:%llu\n", dev->irq_stats.dvp_line_err_cnt);
			seq_printf(f, "\t\t\tdvp over flow:%llu\n", dev->irq_stats.dvp_overflow_cnt);
			seq_printf(f, "\t\t\tdvp bandwidth lack:%llu\n",
				   dev->irq_stats.dvp_bwidth_lack_cnt);
			seq_printf(f, "\t\t\tdvp size err:%llu\n", dev->irq_stats.dvp_size_err_cnt);
		} else {
			seq_printf(f, "\t\t\tcsi over flow:%llu\n", dev->irq_stats.csi_overflow_cnt);
			seq_printf(f, "\t\t\tcsi bandwidth lack:%llu\n",
				   dev->irq_stats.csi_bwidth_lack_cnt);
			seq_printf(f, "\t\t\tcsi size err:%llu\n", dev->irq_stats.csi_size_err_cnt);
		}
		seq_printf(f, "\t\t\tnot active buf cnt:%llu %llu %llu %llu\n",
			   dev->irq_stats.not_active_buf_cnt[0],
			   dev->irq_stats.not_active_buf_cnt[1],
			   dev->irq_stats.not_active_buf_cnt[2],
			   dev->irq_stats.not_active_buf_cnt[3]);
		seq_printf(f, "\t\t\tall err count:%llu\n", dev->irq_stats.all_err_cnt);
		seq_printf(f, "\t\t\tframe dma end:%llu %llu %llu %llu\n",
			   dev->irq_stats.frm_end_cnt[0],
			   dev->irq_stats.frm_end_cnt[1],
			   dev->irq_stats.frm_end_cnt[2],
			   dev->irq_stats.frm_end_cnt[3]);
		seq_printf(f, "irq time: %llu ns\n", dev->hw_dev->irq_time);
		seq_printf(f, "dma enable: 0x%x 0x%x 0x%x 0x%x\n",
			   dev->stream[0].dma_en, dev->stream[1].dma_en,
			   dev->stream[2].dma_en, dev->stream[3].dma_en);
		seq_printf(f, "buf_cnt in drv: %d %d %d %d\n",
			   atomic_read(&dev->stream[0].buf_cnt),
			   atomic_read(&dev->stream[1].buf_cnt),
			   atomic_read(&dev->stream[2].buf_cnt),
			   atomic_read(&dev->stream[3].buf_cnt));
		seq_printf(f, "total buf_cnt: %d %d %d %d\n",
			   dev->stream[0].total_buf_num,
			   dev->stream[1].total_buf_num,
			   dev->stream[2].total_buf_num,
			   dev->stream[3].total_buf_num);
		if (dev->chip_id >= CHIP_RK3576_CIF)
			seq_printf(f, "frame loss: %d %d %d %d\n",
				   dev->stream[0].frame_loss,
				   dev->stream[1].frame_loss,
				   dev->stream[2].frame_loss,
				   dev->stream[3].frame_loss);
		if (dev->sditf[0])
			rkcif_show_toisp_info(dev, f);
		if (dev->reg_dbg)
			rkcif_show_reg_dbg(dev, f);
	}
}

static void rkcif_show_group_info(struct rkcif_device *dev, struct seq_file *f)
{
	seq_puts(f, "\nGroup Info:\n");
	seq_printf(f, "\tSync Mode:%s\n",
		   rkcif_get_sync_mode(dev->sync_cfg.type));
	if (dev->sync_cfg.type != NO_SYNC_MODE)
		seq_printf(f, "\tGroup Id:%d\n", dev->sync_cfg.group);
}

static int rkcif_proc_show(struct seq_file *f, void *v)
{
	struct rkcif_device *dev = f->private;

	if (dev) {
		rkcif_show_mixed_info(dev, f);
		rkcif_show_clks(dev, f);
		rkcif_show_format(dev, f);
		rkcif_show_group_info(dev, f);
	} else {
		seq_puts(f, "dev null\n");
	}

	return 0;
}

static int rkcif_proc_open(struct inode *inode, struct file *file)
{
	struct rkcif_device *data = pde_data(inode);

	return single_open(file, rkcif_proc_show, data);
}

static const struct proc_ops rkcif_proc_fops = {
	.proc_open = rkcif_proc_open,
	.proc_release = single_release,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
};

int rkcif_proc_init(struct rkcif_device *dev)
{

	dev->proc_dir = proc_create_data(dev_name(dev->dev), 0444,
					 NULL, &rkcif_proc_fops,
					 dev);
	if (!dev->proc_dir) {
		dev_err(dev->dev, "create proc/%s failed!\n",
			dev_name(dev->dev));
		return -ENODEV;
	}

	return 0;
}

void rkcif_proc_cleanup(struct rkcif_device *dev)
{
	remove_proc_entry(dev_name(dev->dev), NULL);
}

#endif
