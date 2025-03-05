// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 * author:
 *	Ding Wei <leo.ding@rock-chips.com>
 */

#include <linux/delay.h>

#include "../mpp_rkvdec2.h"
#include "../mpp_rkvdec2_link.h"

#include "mpp_hack_rk3576.h"

#define RK3576_HACK_DATA_RLC_OFFSET		(0)
#define RK3576_HACK_DATA_PPS_OFFSET		(RK3576_HACK_DATA_RLC_OFFSET + 64)
#define RK3576_HACK_DATA_RPS_OFFSET		(RK3576_HACK_DATA_PPS_OFFSET + 256)
#define RK3576_HACK_DATA_RCB_STRMD_OFFSET	(RK3576_HACK_DATA_RPS_OFFSET + 256)
#define RK3576_HACK_DATA_RCB_INTRA_OFFSET	(RK3576_HACK_DATA_RCB_STRMD_OFFSET + 256)
#define RK3576_HACK_DATA_OUT_OFFSET		(RK3576_HACK_DATA_RCB_INTRA_OFFSET + 512)
#define RK3576_HACK_DATA_COLMV_OFFSET		(RK3576_HACK_DATA_OUT_OFFSET + 1536)

#define RK3576_HACK_REGS_OFFSET			(4096)
#define RK3576_HACK_REG_NODE_OFFSET		(RK3576_HACK_REGS_OFFSET)
#define RK3576_HACK_REG_SEG0_OFFSET		(RK3576_HACK_REGS_OFFSET + 32)
#define RK3576_HACK_REG_SEG1_OFFSET		(RK3576_HACK_REGS_OFFSET + 256)
#define RK3576_HACK_REG_SEG2_OFFSET		(RK3576_HACK_REGS_OFFSET + 512)
#define RK3576_HACK_REG_DEBUG_OFFSET		(RK3576_HACK_REGS_OFFSET + 1024)


static const char rk3576_hack_h264_rlc_data[] = {
	0x00, 0x00, 0x01, 0x65,
	0x88, 0x82, 0x0b, 0x01,
	0x2f, 0x08, 0xc5, 0x00,
	0x01, 0x51, 0x78, 0xe0,
	0x00, 0x24, 0xf7, 0x1c,
	0x00, 0x04, 0xcc, 0xeb,
	0x89, 0xd7, 0x80, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

static const char rk3576_hack_h264_pps_data[] = {
	0x40, 0x26, 0x00, 0x10,
	0x04, 0x08, 0x00, 0x08,
	0x80, 0x01, 0x00, 0x00,
	0x00, 0x40, 0x01, 0xd8,
	0x07, 0x7c, 0x7a, 0x00,
	0x00, 0x00, 0x00, 0x04,
	0x00, 0x00, 0x00, 0x00,
};

static bool rk3576_hack_flag;

static bool is_rk3576(struct device *dev)
{
	int ret;

	ret = device_property_match_string(dev, "compatible", "rockchip,rkv-decoder-rk3576");

	return (ret < 0) ? false : true;
}

int rk3576_workaround_init(struct mpp_dev *mpp)
{
	struct rkvdec2_dev *dec = to_rkvdec2_dev(mpp);
	u32 *reg;

	/* check current soc is rk3576 */
	rk3576_hack_flag = is_rk3576(mpp->dev);
	if (!rk3576_hack_flag)
		return 0;

	/* alloc buffer for node */
	dec->fix = mpp_dma_alloc(mpp->dev, 2 * PAGE_SIZE);
	if (!dec->fix) {
		dev_err(mpp->dev, "failed to create buffer for hack\n");
		return -ENOMEM;
	}
	memset(dec->fix->vaddr, 0, dec->fix->size);

	/* set rlc_in */
	memcpy(dec->fix->vaddr + RK3576_HACK_DATA_RLC_OFFSET,
		rk3576_hack_h264_rlc_data, sizeof(rk3576_hack_h264_rlc_data));

	/* set sps_pps */
	memcpy(dec->fix->vaddr + RK3576_HACK_DATA_PPS_OFFSET,
		rk3576_hack_h264_pps_data, sizeof(rk3576_hack_h264_pps_data));

	/* set core registers */
	reg = dec->fix->vaddr + RK3576_HACK_REGS_OFFSET;
	reg[8]  = 0x00000001; // dec_mode
	reg[13] = 0x0000ffff; // core time out
	reg[16] = 0x00000101; // disable error proc
	reg[20] = 0xffffffff; // cabac error low bits
	reg[21] = 0x3ff3ffff; // cabac error high bits

	reg[64] = RK3576_HACK_MAGIC; // magic number
	reg[65] = 0x00000000; // stream start bit
	reg[66] = 0x00000020; // stream_len
	reg[67] = 0x000000a8; // global_len
	reg[68] = 0x00000002; // hor_virstride
	reg[69] = 0x00000002; // ver_virstride
	reg[70] = 0x00000040; // y_virstride

	reg[128] = dec->fix->iova + RK3576_HACK_DATA_RLC_OFFSET; // rlc_base
	reg[129] = dec->fix->iova + RK3576_HACK_DATA_RPS_OFFSET; // rps_base
	reg[131] = dec->fix->iova + RK3576_HACK_DATA_PPS_OFFSET; // pps_base
	reg[140] = dec->fix->iova + RK3576_HACK_DATA_RCB_STRMD_OFFSET; // streamd_rcb
	reg[141] = 0x000000c0;
	reg[148] = dec->fix->iova + RK3576_HACK_DATA_RCB_INTRA_OFFSET; // intra_rcb
	reg[149] = 0x00000200;
	reg[168] = dec->fix->iova + RK3576_HACK_DATA_OUT_OFFSET; // decout_base
	reg[216] = dec->fix->iova + RK3576_HACK_DATA_COLMV_OFFSET; // colmv_base

	/* set link node */
	reg = dec->fix->vaddr + RK3576_HACK_REG_NODE_OFFSET;
	reg[0] = dec->fix->iova + RK3576_HACK_DATA_OUT_OFFSET; // next link node
	reg[1] = 0x76543212; // table info
	reg[2] = dec->fix->iova + RK3576_HACK_REG_DEBUG_OFFSET; // debug regs
	reg[3] = dec->fix->iova + RK3576_HACK_REG_SEG0_OFFSET; // seg0 regs
	reg[4] = dec->fix->iova + RK3576_HACK_REG_SEG1_OFFSET; // seg1 regs
	reg[5] = dec->fix->iova + RK3576_HACK_REG_SEG2_OFFSET; // seg2 regs

	return 0;
}

int rk3576_workaround_exit(struct mpp_dev *mpp)
{
	struct rkvdec2_dev *dec = to_rkvdec2_dev(mpp);

	/* check current soc is rk3576 */
	if (!rk3576_hack_flag)
		return 0;

	if (dec->fix) {
		mpp_dma_free(dec->fix);
		dec->fix = NULL;
	}

	return 0;
}

int rk3576_workaround_run(struct mpp_dev *mpp)
{
	struct rkvdec2_dev *dec = to_rkvdec2_dev(mpp);
	struct rkvdec_link_dev *link = dec->link_dec;
	int ret;
	u32 irq_val, status;

	/* check current soc is rk3576 */
	if (!rk3576_hack_flag || !dec->fix || !link)
		return 0;

	/* disable hardware irq */
	writel_relaxed(0x00008000, link->reg_base + 0x58);

	/* set link registers */
	writel_relaxed(0x0007ffff, link->reg_base + 0x54); // set ip time out
	writel_relaxed(0x00010001, link->reg_base + 0x00); // ccu mode
	writel_relaxed(dec->fix->iova + RK3576_HACK_REG_NODE_OFFSET, link->reg_base + 0x4); // addr
	writel_relaxed(0x00000001, link->reg_base + 0x08); // link mode
	writel_relaxed(0x00000001, link->reg_base + 0x18); // link en

	/* enable hardware */
	wmb();
	writel(0x01, link->reg_base + 0x0c); // config link

	/* wait hardware */
	ret = readl_relaxed_poll_timeout_atomic(link->reg_base + 0x48, irq_val, irq_val, 1, 500);
	if (ret == -ETIMEDOUT) {
		pr_err("%s timeout.\n", __func__);
		// return ret;
	} else {
		status = readl(link->reg_base + 0x4c);
		if (status & 0x3fe)
			pr_err("%s not ready, status %08x.\n", __func__, status);
	}
	/* clear irq and status */
	writel_relaxed(0xffff0000, link->reg_base + 0x48);
	writel_relaxed(0xffff0000, link->reg_base + 0x4c);

	/* reset register */
	writel_relaxed(0x00000000, link->reg_base + 0x00); // ccu mode
	writel_relaxed(0x00000000, link->reg_base + 0x08); // link mode
	writel_relaxed(0x00000000, link->reg_base + 0x18); // link en

	/* enable irq */
	writel(0x00000000, link->reg_base + 0x58);
	udelay(5);

	return ret;
}
