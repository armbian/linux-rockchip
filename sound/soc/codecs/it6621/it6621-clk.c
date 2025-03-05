// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 * Author: Jason Zhang <jason.zhang@rock-chips.com>
 */

#include "it6621.h"
#include "it6621-clk.h"
#include "it6621-earc.h"
#include "it6621-reg-bank0.h"
#include "it6621-reg-bank1.h"

static int it6621_get_100ms_cnt(struct it6621_priv *priv, u32 *count)
{
	unsigned int val;

	regmap_write(priv->regmap, IT6621_REG_GEN0, IT6621_100MS_CNT_EN);
	msleep(99);
	regmap_write(priv->regmap, IT6621_REG_GEN0, IT6621_100MS_CNT_DIS);
	regmap_read(priv->regmap, IT6621_REG_100MS_CNT_7_0, &val);
	*count = (u32)val;
	regmap_read(priv->regmap, IT6621_REG_100MS_CNT_15_8, &val);
	*count += ((u32)(val) << 8);
	regmap_read(priv->regmap, IT6621_REG_100MS_CNT_23_16, &val);
	*count += ((u32)(val) << 16);

	return 0;
}

static int it6621_get_lcf(struct it6621_priv *priv, unsigned int *lcf)
{
	unsigned int timer_1us_flt;
	unsigned int lcf_high;
	unsigned int lcf_low;
	unsigned int tmp;

	if (priv->fixed_lcf) {
		*lcf = priv->fixed_lcf;
		dev_info(priv->dev, "lcf: %d\n", *lcf);
	} else {
		regmap_update_bits(priv->regmap, 0xa9, 0x0f, 0x00);
		it6621_get_100ms_cnt(priv, &lcf_low);

		regmap_update_bits(priv->regmap, 0xa9, 0x0f, 0x0f);
		it6621_get_100ms_cnt(priv, &lcf_high);

		if (!(lcf_high - lcf_low)) {
			dev_err(priv->dev, "Invalid LCF");
			return -EINVAL;
		}

		/* Get tenths */
		tmp = (1000000 - lcf_low) * 16 * 10 / (lcf_high - lcf_low);
		timer_1us_flt = tmp % 10;
		tmp = tmp / 10;
		if (timer_1us_flt >= 5)
			*lcf = tmp + 1;
		else
			*lcf = tmp;

		dev_info(priv->dev, "lcf low: %d, lcf high: %d, lcf: %d\n",
			 lcf_low, lcf_high, *lcf);
	}

	if (*lcf > 15)
		*lcf = 15;

	return 0;
}

static int it6621_get_refclk(struct it6621_priv *priv, unsigned int *refclk)
{
	if (priv->rclk_sel == IT6621_RCLK_FREQ_REFCLK)
		*refclk = priv->rclk / 2;
	else if (priv->rclk_sel == IT6621_RCLK_FREQ_REFCLK_DIV_2)
		*refclk = priv->rclk;
	else if (priv->rclk_sel == IT6621_RCLK_FREQ_REFCLK_DIV_4)
		*refclk = priv->rclk * 2;
	else if (priv->rclk_sel == IT6621_RCLK_FREQ_REFCLK_DIV_8)
		*refclk = priv->rclk * 4;
	else
		*refclk = 0;

	return 0;
}

int it6621_calc_rclk(struct it6621_priv *priv)
{
	unsigned int timer_1us;
	unsigned int timer_1us_int;
	unsigned int timer_1us_flt;
	unsigned int lcf;
	unsigned int aclk_bnd_num;
	unsigned int aclk_valid_num;
	unsigned int sum;
	int ret;

	ret = it6621_get_lcf(priv, &lcf);
	if (ret)
		return ret;

	regmap_update_bits(priv->regmap, 0xa9, 0x0f, lcf);

	it6621_get_100ms_cnt(priv, &sum);
	priv->rclk = sum / 100;
	dev_dbg(priv->dev, "RCLK: %uMHz\n", priv->rclk / 1000);

	/* Update 1us timer */
	timer_1us = sum / 100000;
	timer_1us_int = timer_1us;
	timer_1us_flt = sum % 100000;
	timer_1us_flt <<= 8;
	timer_1us_flt /= 100000;
	regmap_write(priv->regmap, IT6621_REG_GEN4, timer_1us_int);
	regmap_write(priv->regmap, IT6621_REG_1US_TIME_FLT, timer_1us_flt);

	if (priv->rclk_sel == IT6621_RCLK_FREQ_REFCLK)
		sum /= 2;
	else if (priv->rclk_sel == IT6621_RCLK_FREQ_REFCLK_DIV_4)
		sum *= 2;
	else if (priv->rclk_sel == IT6621_RCLK_FREQ_REFCLK_DIV_8)
		sum *= 4;

	aclk_bnd_num = sum * 128 / 358400;
	aclk_valid_num = sum * 128 / 200000;
	regmap_write(priv->regmap, IT6621_REG_CLK_DET0, aclk_bnd_num & 0xff);
	regmap_write(priv->regmap, IT6621_REG_CLK_DET1,
		     (aclk_bnd_num & 0x100) >> 8);
	regmap_write(priv->regmap, IT6621_REG_CLK_DET2,
		     aclk_valid_num & 0xff);
	regmap_write(priv->regmap, IT6621_REG_CLK_DET3,
		     (aclk_valid_num & 0x300) >> 8);

	return 0;
}

/**
 * it6621_force_pdiv() - If auto mode error , FW force to correct PLL's setting
 * @priv: The priv device
 *
 * Returns zero if succeed.
 */
int it6621_force_pdiv(struct it6621_priv *priv)
{
	unsigned int tbclk_sel;
	unsigned int xp_pdiv;
	unsigned int xp_gain;
	unsigned int val;
	unsigned int expect;

	regmap_read(priv->regmap, IT6621_REG_CLK_CTRL1, &val);
	tbclk_sel = val & IT6621_TBCLK_SEL;

	regmap_read(priv->regmap, IT6621_REG_TX_AFE1, &val);
	xp_pdiv = val & IT6621_TX_XP_PDIV;
	xp_gain = (val & IT6621_TX_XP_GAIN) >> 2;

	dev_dbg(priv->dev, "ACLK: %d\n", priv->aclk);

	expect = 0xff;
	switch (tbclk_sel) {
	case IT6621_TBCLK_AICLK_MUL_32:
		if (xp_pdiv != 0)
			expect = 0;
		break;
	case IT6621_TBCLK_AICLK_MUL_16:
		if ((priv->aclk < 3584) && (xp_pdiv != 0))
			expect = 0;
		else if ((priv->aclk > 3584) && (priv->aclk < 7168) &&
			 (xp_pdiv != 1))
			expect = 1;
		break;
	case IT6621_TBCLK_AICLK_MUL_8:
		if ((priv->aclk < 3584) && (xp_pdiv != 0))
			expect = 0;
		else if ((priv->aclk > 3584) && (priv->aclk < 7168) &&
			 (xp_pdiv != 1))
			expect = 1;
		else if ((priv->aclk > 7168) && (priv->aclk < 14336) &&
			 (xp_pdiv != 3))
			expect = 3;
		break;
	case IT6621_TBCLK_AICLK_MUL_4:
		if ((priv->aclk < 7168) && xp_pdiv != 0)
			expect = 0;
		else if ((priv->aclk > 7168) && (priv->aclk < 14336) &&
			 (xp_pdiv != 1))
			expect = 1;
		else if ((priv->aclk > 14336) && (priv->aclk < 28672) &&
			 (xp_pdiv != 3))
			expect = 3;
		break;
	case IT6621_TBCLK_AICLK_MUL_2:
		if ((priv->aclk < 14336) && xp_pdiv != 0)
			expect = 0;
		else if ((priv->aclk > 14336) && (priv->aclk < 28672) &&
			 (xp_pdiv != 1))
			expect = 1;
		else if ((priv->aclk > 28672) && (priv->aclk < 57334) &&
			 (xp_pdiv != 3))
			expect = 3;
		break;
	case IT6621_TBCLK_AICLK:
		if ((priv->aclk < 28672) && (xp_pdiv != 0))
			expect = 0;
		else if ((priv->aclk > 28672) && (priv->aclk < 57334) &&
			 (xp_pdiv != 1))
			expect = 1;
		else if ((priv->aclk > 57334) && (priv->aclk < 114668) &&
			 (xp_pdiv != 3))
			expect = 3;
		break;
	default:
		break;
	}

	if (expect != 0xff)
		dev_warn(priv->dev, "Wrong XP PDIV, detect %d, expected %d\n",
			 xp_pdiv, expect);

	expect = 0xff;
	switch (tbclk_sel) {
	case IT6621_TBCLK_AICLK:
		if ((priv->aclk > 3584) && (priv->aclk < 7168) &&
		    (xp_gain != 0))
			expect = 0;
		break;
	case IT6621_TBCLK_AICLK_MUL_2:
		if ((priv->aclk < 3584) && (xp_gain != 0))
			expect = 0;
		break;
	default:
		if (xp_gain != 1)
			expect = 1;
		break;
	}

	if (expect != 0xff)
		dev_warn(priv->dev, "Wrong XP GAIN, detect %d, expected %d\n",
			 xp_gain, expect);

	regmap_read(priv->regmap, IT6621_REG_TX_AFE1, &val);
	xp_pdiv = val & IT6621_TX_XP_PDIV;
	xp_gain = (val & IT6621_TX_XP_GAIN) >> 2;
	dev_dbg(priv->dev, "TBCLK SEL: 0x%02X, XP PDIV: %d, XP GAIN: %d\n",
		tbclk_sel, xp_pdiv, xp_gain);

	usleep_range(10000, 11000);
	regmap_update_bits(priv->regmap, IT6621_REG_DMAC_CTRL1,
			   IT6621_TX_MANUAL_RESET_EN_SEL,
			   IT6621_TX_MANUAL_RESET_EN);
	regmap_update_bits(priv->regmap, IT6621_REG_DMAC_CTRL1,
			   IT6621_TX_MANUAL_RESET_EN_SEL,
			   IT6621_TX_MANUAL_RESET_DIS);

	return 0;
}

void it6621_get_aclk(struct it6621_priv *priv)
{
	unsigned int aclk_avg;
	unsigned int aclk_pred2;
	unsigned int aclk_pred4;
	unsigned int aclk_valid;
	unsigned int aclk_stb;
	unsigned int refclk;
	unsigned int tmp;

	it6621_get_refclk(priv, &refclk);

	regmap_read(priv->regmap, IT6621_REG_CLK_DET8, &aclk_avg);
	regmap_read(priv->regmap, IT6621_REG_CLK_DET9, &tmp);
	aclk_avg += ((tmp & IT6621_DET_ACLK_AVG_HIGH) << 8);
	aclk_pred2 = (tmp & IT6621_DET_ACLK_PRE_DIV_2) >> 4;
	aclk_pred4 = (tmp & IT6621_DET_ACLK_PRE_DIV_4) >> 5;
	aclk_valid = (tmp & IT6621_DET_ACLK_FREQ_VALID) >> 6;
	aclk_stb = (tmp & IT6621_DET_ACLK_FREQ_STB) >> 7;

	priv->aclk = refclk * 128 / aclk_avg;

	if (aclk_pred4)
		priv->aclk *= 4;
	else if (aclk_pred2)
		priv->aclk *= 2;

	dev_dbg(priv->dev, "ACLK: %uMHz, valid: %d, stable: %d\n",
		priv->aclk / 1000, aclk_valid, aclk_stb);
}

void it6621_get_bclk(struct it6621_priv *priv)
{
	unsigned int bclk_avg;
	unsigned int bclk_pred2;
	unsigned int bclk_pred4;
	unsigned int bclk_valid;
	unsigned int bclk_stb;
	unsigned int refclk;
	unsigned int tmp;

	it6621_get_refclk(priv, &refclk);

	regmap_read(priv->regmap, IT6621_REG_CLK_DET10, &bclk_avg);
	regmap_read(priv->regmap, IT6621_REG_CLK_DET11, &tmp);
	bclk_avg += ((tmp & IT6621_DET_BCLK_AVG_HIGH) << 8);
	bclk_pred2 = (tmp & IT6621_DET_BCLK_PRE_DIV_2) >> 4;
	bclk_pred4 = (tmp & IT6621_DET_BCLK_PRE_DIV_4) >> 5;
	bclk_valid = (tmp & IT6621_DET_BCLK_FREQ_VALID) >> 6;
	bclk_stb = (tmp & IT6621_DET_BCLK_FREQ_STB) >> 7;

	priv->bclk = refclk * 128 / bclk_avg;

	if (bclk_pred4)
		priv->bclk *= 4;
	else if (bclk_pred2)
		priv->bclk *= 2;

	dev_dbg(priv->dev, "BCLK: %uMHz, valid: %d, stable: %d\n",
		priv->bclk / 1000, bclk_valid, bclk_stb);
}
