// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for ROCKCHIP Integrated FEPHYs
 *
 * Copyright (c) 2025, Rockchip Electronics Co., Ltd.
 *
 * David Wu <david.wu@rock-chips.com>
 */

#include <linux/clk.h>
#include <linux/ethtool.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/netdevice.h>
#include <linux/nvmem-consumer.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/phy.h>
#include <linux/regmap.h>

#define INTERNAL_FEPHY_ID			0x06808101

#define MII_INTERNAL_CTRL_STATUS		17
#define SMI_ADDR_CFGCNTL			20
#define SMI_ADDR_TSTREAD1			21
#define SMI_ADDR_TSTREAD2			22
#define SMI_ADDR_TSTWRITE			23
#define MII_LED_CTRL				25
#define MII_INT_STATUS				29
#define MII_INT_MASK				30
#define MII_SPECIAL_CONTROL_STATUS		31

#define MII_AUTO_MDIX_EN			BIT(7)
#define MII_MDIX_EN				BIT(6)

#define MII_SPEED_10				BIT(2)
#define MII_SPEED_100				BIT(3)

#define CFGCNTL_WRITE_ADDR			0
#define CFGCNTL_READ_ADDR			5
#define CFGCNTL_GROUP_SEL			11
#define CFGCNTL_RD				(BIT(15) | BIT(10))
#define CFGCNTL_WR				(BIT(14) | BIT(10))

#define CFGCNTL_WRITE(group, reg)		(CFGCNTL_WR | ((group) << CFGCNTL_GROUP_SEL) \
						| ((reg) << CFGCNTL_WRITE_ADDR))
#define CFGCNTL_READ(group, reg)		(CFGCNTL_RD | ((group) << CFGCNTL_GROUP_SEL) \
						| ((reg) << CFGCNTL_READ_ADDR))

#define GAIN_PRE				GENMASK(5, 2)
#define WR_ADDR_A7CFG				0x18

#define MDIX_OFFSET_MIN				-5
#define MDI_OFFSET_MAX				3
#define OFFSET_TIMES_MAX			5

#define DEFAULT_TXAMP				0x9

enum {
	GROUP_CFG0 = 0,
	GROUP_WOL,
	GROUP_CFG0_READ,
	GROUP_BIST,
	GROUP_AFE,
	GROUP_CFG1
};

struct rockchip_fephy_priv {
	struct phy_device *phydev;
	unsigned int clk_rate;
	struct clk *pclk;
	int old_link;
	int wol_irq;
	int txamp;
	int mdi_offset;
	int mdix_offset;
	int current_group;
	struct regmap *regs;
};

static int rockchip_fephy_group_read(struct phy_device *phydev, u8 group, u32 reg)
{
	int ret;

	ret = phy_write(phydev, SMI_ADDR_CFGCNTL, CFGCNTL_READ(group, reg));
	if (ret)
		return ret;

	if (group)
		return phy_read(phydev, SMI_ADDR_TSTREAD1);
	else
		return (phy_read(phydev, SMI_ADDR_TSTREAD1) |
			(phy_read(phydev, SMI_ADDR_TSTREAD2) << 16));
}

static int rockchip_fephy_group_write(struct phy_device *phydev, u8 group,
				      u32 reg, u16 val)
{
	int ret;

	ret = phy_write(phydev, SMI_ADDR_TSTWRITE, val);
	if (ret)
		return ret;

	return phy_write(phydev, SMI_ADDR_CFGCNTL, CFGCNTL_WRITE(group, reg));
}

static int rockchip_fephy_get_txamp_from_nvmem(struct phy_device *phydev)
{
	struct rockchip_fephy_priv *priv = phydev->priv;
	unsigned char *buf;
	struct nvmem_cell *cell;
	int txamp_type;
	size_t len;

	cell = nvmem_cell_get(&phydev->mdio.dev, "txamp");
	if (IS_ERR(cell)) {
		phydev_err(phydev, "failed to get txamp cell: %ld, use default\n",
			   PTR_ERR(cell));
	} else {
		buf = nvmem_cell_read(cell, &len);
		nvmem_cell_put(cell);
		if (!IS_ERR(buf)) {
			if (len == 2) {
				priv->txamp = buf[0] & 0x1f;
				txamp_type =  buf[1];
				/* For some cases, if it's an odd number, add 3 */
				if (txamp_type == 0x8 && (priv->txamp & 1))
					priv->txamp += 3;
			}
			kfree(buf);
			return 0;
		}
		phydev_err(phydev, "failed to get nvmem buf, use default\n");
	}

	return -EINVAL;
}

static int rockchip_fephy_get_adc_offset_from_nvmem(struct phy_device *phydev)
{
	struct rockchip_fephy_priv *priv = phydev->priv;
	unsigned char *buf;
	struct nvmem_cell *cell;
	size_t len;

	cell = nvmem_cell_get(&phydev->mdio.dev, "adc_offset");
	if (IS_ERR(cell)) {
		phydev_err(phydev, "failed to get offset cell: %ld, use default\n",
			   PTR_ERR(cell));
	} else {
		buf = nvmem_cell_read(cell, &len);
		nvmem_cell_put(cell);
		if (!IS_ERR(buf)) {
			if (len == 2) {
				priv->mdi_offset = buf[0] & 0x7f;
				priv->mdix_offset = buf[1] & 0x7f;
			}
			kfree(buf);
			return 0;
		}
		phydev_err(phydev, "failed to get nvmem buf, use default\n");
	}

	return -EINVAL;
}

static int rockchip_fephy_fix_offset(struct phy_device *phydev)
{
	struct rockchip_fephy_priv *priv = phydev->priv;
	int offset, mdi_offset, mdix_offset, sum;
	int mdi_fix, mdix_fix;
	int ret = 0;

	mdi_offset = (priv->mdi_offset < 0x40) ? priv->mdi_offset : (priv->mdi_offset - 0x80);
	mdix_offset = (priv->mdix_offset < 0x40) ? priv->mdix_offset : (priv->mdix_offset - 0x80);

	sum = mdi_offset + mdix_offset;
	/* Balance to smaller side */
	offset = ((sum >= 0) ? (sum + 1) : sum) / 2;
	offset -= (MDI_OFFSET_MAX + MDIX_OFFSET_MIN) / 2;
	mdi_fix = mdi_offset - offset;
	mdix_fix = mdix_offset - offset;
	if (mdi_fix > MDI_OFFSET_MAX || mdix_fix < MDIX_OFFSET_MIN) {
		int reg;

		reg = phy_read(priv->phydev, MII_INTERNAL_CTRL_STATUS);
		if (reg < 0)
			return reg;

		if ((abs(mdi_offset)) <= abs(mdix_offset)) {
			offset = mdi_offset;
			/* Force MDI */
			reg &= ~(MII_MDIX_EN | MII_AUTO_MDIX_EN);
		} else {
			offset = mdix_offset;
			/* Force MDIX */
			reg &= ~MII_AUTO_MDIX_EN;
			reg |= MII_MDIX_EN;
		}

		ret = phy_write(phydev, MII_INTERNAL_CTRL_STATUS, reg);
		if (ret)
			return ret;
	}

	offset = (offset >= 0) ? offset : (offset + 0x80);
	offset &= 0x7f;
	clk_enable(priv->pclk);
	regmap_write(priv->regs, 0xC0, offset);
	regmap_write(priv->regs, 0xA0, 0xFFFF0008);
	clk_disable(priv->pclk);

	return ret;
}

static int rockchip_fephy_config_init(struct phy_device *phydev)
{
	struct rockchip_fephy_priv *priv = phydev->priv;
	int ret;

	/* LED Control, default:0x7f */
	ret = phy_write(phydev, MII_LED_CTRL, 0x7aa);
	if (ret)
		return ret;

	/* off-energy level0 threshold */
	ret = rockchip_fephy_group_write(phydev, GROUP_CFG0, 0xa, 0x6664);
	if (ret)
		return ret;

	/* 100M amplitude control */
	ret = rockchip_fephy_group_write(phydev, GROUP_CFG0, 0x18, priv->txamp);
	if (ret)
		return ret;

	/* 10M amplitude control */
	ret = rockchip_fephy_group_write(phydev, GROUP_CFG0, 0x1f, 0x7);
	if (ret)
		return ret;

	if (priv->clk_rate == 24000000) {
		int sel;

		/* pll cp cur sel */
		sel = rockchip_fephy_group_read(phydev, GROUP_AFE, 0x3);
		if (sel < 0)
			return sel;
		ret = rockchip_fephy_group_write(phydev, GROUP_AFE, 0x3, sel | 0x1);
		if (ret)
			return ret;

		/* pll lpf res sel */
		ret = rockchip_fephy_group_write(phydev, GROUP_CFG0, 0x1a, 0x6);
		if (ret)
			return ret;
	}

	return rockchip_fephy_fix_offset(phydev);
}

static int rockchip_fephy_config_aneg(struct phy_device *phydev)
{
	return genphy_config_aneg(phydev);
}

static void rockchip_feph_link_change_notify(struct phy_device *phydev)
{
	struct rockchip_fephy_priv *priv = phydev->priv;
	int ret;

	if (priv->old_link && !phydev->link) {
		priv->old_link = 0;
		ret = rockchip_fephy_group_write(phydev, GROUP_CFG0, 0xa, 0x6664);
		if (ret)
			return;
	} else if (!priv->old_link && phydev->link) {
		int gain;

		priv->old_link = 1;
		/* read gain level */
		gain = rockchip_fephy_group_read(phydev, GROUP_CFG0, 0x0);
		if (gain < 0)
			return;
		if (!(gain & GAIN_PRE)) {
			ret = rockchip_fephy_group_write(phydev, GROUP_CFG0, 0xa, 0x6666);
			if (ret)
				return;
		}
	}
}

static int rockchip_fephy_wol_enable(struct phy_device *phydev)
{
	struct net_device *ndev = phydev->attached_dev;
	int ret;

	ret = rockchip_fephy_group_write(phydev, GROUP_WOL, 0x0,
					 ((u16)ndev->dev_addr[4] << 8) + ndev->dev_addr[5]);
	if (ret)
		return ret;

	ret = rockchip_fephy_group_write(phydev, GROUP_WOL, 0x1,
					 ((u16)ndev->dev_addr[2] << 8) + ndev->dev_addr[3]);
	if (ret)
		return ret;

	ret = rockchip_fephy_group_write(phydev, GROUP_WOL, 0x2,
					 ((u16)ndev->dev_addr[0] << 8) + ndev->dev_addr[1]);
	if (ret)
		return ret;

	ret = rockchip_fephy_group_write(phydev, GROUP_WOL, 0x3, 0xf);
	if (ret)
		return ret;

	/* Enable WOL interrupt */
	ret = phy_write(phydev, MII_INT_MASK, 0xe00);
	if (ret)
		return ret;

	return ret;
}

static int rockchip_fephy_wol_disable(struct phy_device *phydev)
{
	int ret;

	ret = rockchip_fephy_group_write(phydev, GROUP_WOL, 0x3, 0x0);
	if (ret)
		return ret;

	/* Disable WOL interrupt */
	ret = phy_write(phydev, MII_INT_MASK, 0x0);
	if (ret)
		return ret;

	return ret;
}

static irqreturn_t rockchip_fephy_wol_irq_thread(int irq, void *dev_id)
{
	struct rockchip_fephy_priv *priv = (struct rockchip_fephy_priv *)dev_id;

	/* Read status to ack interrupt */
	phy_read(priv->phydev, MII_INT_STATUS);

	return IRQ_HANDLED;
}

static void rockchip_fephy_dump_cfg1_group_regs(struct phy_device *phydev, int group, char *buf)
{
	int reg = 0, val = 0;

	for (reg = 0; reg < 18; reg++) {
		val = rockchip_fephy_group_read(phydev, GROUP_CFG1, reg);
		if (val < 0) {
			pr_err("group%d %2d read error: %d\n", group, reg, val);
			return;
		}
		if (buf)
			sprintf(buf, "%sgroup%d %2d: 0x%x\n", buf, group, reg, val);
		else
			pr_info("group%d reg_%02d: 0x%x\n", group, reg, val);
	}
}

static void rockchip_fephy_dump_afe_group_regs(struct phy_device *phydev, int group, char *buf)
{
	int reg = 0, val = 0;

	for (reg = 0; reg < 32; reg++) {
		val = rockchip_fephy_group_read(phydev, GROUP_AFE, reg);
		if (val < 0) {
			pr_err("group%d %2d read error: %d\n", group, reg, val);
			return;
		}
		if (buf)
			sprintf(buf, "%sgroup%d %2d: 0x%x\n", buf, group, reg, val);
		else
			pr_info("group%d reg_%02d: 0x%x\n", group, reg, val);
	}
}

static void rockchip_fephy_dump_bist_group_regs(struct phy_device *phydev, int group, char *buf)
{
	int reg = 0, val = 0;

	for (reg = 0; reg < 32; reg++) {
		val = rockchip_fephy_group_read(phydev, GROUP_BIST, reg);
		if (val < 0) {
			pr_err("group%d %2d read error: %d\n", group, reg, val);
			return;
		}
		if (buf)
			sprintf(buf, "%sgroup%d %2d: 0x%x\n", buf, group, reg, val);
		else
			pr_info("group%d reg_%02d: 0x%x\n", group, reg, val);
	}
}

static void rockchip_fephy_dump_cfg_read_group_regs(struct phy_device *phydev, int group, char *buf)
{
	int reg = 0, val = 0;

	for (reg = 0; reg < 32; reg++) {
		val = rockchip_fephy_group_read(phydev, GROUP_CFG0_READ, reg);
		if (val < 0) {
			pr_err("group%d %2d read error: %d\n", group, reg, val);
			return;
		}
		if (buf)
			sprintf(buf, "%sgroup%d %2d: 0x%x\n", buf, group, reg, val);
		else
			pr_info("group%d reg_%02d: 0x%x\n", group, reg, val);
	}
}

static void rockchip_fephy_dump_wol_group_regs(struct phy_device *phydev, int group, char *buf)
{
	int reg = 0, val = 0;

	for (reg = 0; reg < 13; reg++) {
		val = rockchip_fephy_group_read(phydev, GROUP_WOL, reg);
		if (val < 0) {
			pr_err("group%d %2d read error: %d\n", group, reg, val);
			return;
		}
		if (buf)
			sprintf(buf, "%sgroup%d %2d: 0x%x\n", buf, group, reg, val);
		else
			pr_info("group%d reg_%02d: 0x%x\n", group, reg, val);
	}
}

static void rockchip_fephy_dump_cfg_group_regs(struct phy_device *phydev, int group, char *buf)
{
	int reg = 0, val = 0;

	for (reg = 0; reg < 32; reg++) {
		val = rockchip_fephy_group_read(phydev, GROUP_CFG0, reg);
		if (val < 0) {
			pr_err("group%d %2d read error: %d\n", group, reg, val);
			return;
		}
		if (buf)
			sprintf(buf, "%sgroup%d %2d: 0x%x\n", buf, group, reg, val);
		else
			pr_info("group%d reg_%02d: 0x%x\n", group, reg, val);
	}
}

static void rockchip_fephy_phy_read_priv_reg(struct phy_device *phydev, int group, int reg)
{
	int val = 0;

	switch (group) {
	case GROUP_CFG0: /* CFG0 register group */
		val = rockchip_fephy_group_read(phydev, GROUP_CFG0, reg);
		if (val < 0) {
			pr_err("group%d %2d read error: %d\n", group, reg, val);
			return;
		}
		pr_info("read group%d reg_%02d: 0x%x\n", group, reg, val);
		break;
	case GROUP_WOL: /* WOL register group */
		val = rockchip_fephy_group_read(phydev, GROUP_WOL, reg);
		if (val < 0) {
			pr_err("group%d %2d read error: %d\n", group, reg, val);
			return;
		}
		pr_info("read group%d reg_%02d: 0x%x\n", group, reg, val);
		break;
	case GROUP_CFG0_READ: /* CFG0_read register group */
		val = rockchip_fephy_group_read(phydev, GROUP_CFG0_READ, reg);
		if (val < 0) {
			pr_err("group%d %2d read error: %d\n", group, reg, val);
			return;
		}
		pr_info("read group%d reg_%02d: 0x%x\n", group, reg, val);
		break;
	case GROUP_BIST: /* BIST register group */
		val = rockchip_fephy_group_read(phydev, GROUP_BIST, reg);
		if (val < 0) {
			pr_err("group%d %2d read error: %d\n", group, reg, val);
			return;
		}
		pr_info("read group%d reg_%02d: 0x%x\n", group, reg, val);
		break;
	case GROUP_AFE: /* AFE register group */
		val = rockchip_fephy_group_read(phydev, GROUP_AFE, reg);
		if (val < 0) {
			pr_err("group%d %2d read error: %d\n", group, reg, val);
			return;
		}
		pr_info("read group%d reg_%02d: 0x%x\n", group, reg, val);
		break;
	case GROUP_CFG1: /* CFG1 register group */
		val = rockchip_fephy_group_read(phydev, GROUP_CFG1, reg);
		if (val < 0) {
			pr_err("group%d %2d read error: %d\n", group, reg, val);
			return;
		}
		pr_info("read group%d reg_%02d: 0x%x\n", group, reg, val);
		break;
	default:
		pr_err("error group num: %d\n", group);
		break;
	}
}

static void
rockchip_fephy_phy_write_priv_reg(struct phy_device *phydev, int group, int reg, int rval)
{
	int val = 0;

	switch (group) {
	case GROUP_CFG0: /* CFG0 register group */
		val = rockchip_fephy_group_write(phydev, GROUP_CFG0, reg, rval);
		if (val) {
			pr_err("group%d %2d write error: %d\n", group, reg, val);
			return;
		}
		pr_info("write group%d reg_%02d: 0x%x\n", group, reg, val);
		break;
	case GROUP_WOL: /* WOL register group */
		val = rockchip_fephy_group_write(phydev, GROUP_WOL, reg, rval);
		if (val) {
			pr_err("group%d %2d write error: %d\n", group, reg, val);
			return;
		}
		pr_info("write group%d reg_%02d: 0x%x\n", group, reg, val);
		break;
	case GROUP_CFG0_READ: /* CFG0_read register group */
		val = rockchip_fephy_group_write(phydev, GROUP_CFG0_READ, reg, rval);
		if (val) {
			pr_err("group%d %2d write error: %d\n", group, reg, val);
			return;
		}
		pr_info("write group%d reg_%02d: 0x%x\n", group, reg, val);
		break;
	case GROUP_BIST: /* BIST register group */
		val = rockchip_fephy_group_write(phydev, GROUP_BIST, reg, rval);
		if (val) {
			pr_err("group%d %2d write error: %d\n", group, reg, val);
			return;
		}
		pr_info("write group%d reg_%02d: 0x%x\n", group, reg, val);
		break;
	case GROUP_AFE: /* AFE register group */
		val = rockchip_fephy_group_write(phydev, GROUP_AFE, reg, rval);
		if (val) {
			pr_err("group%d %2d write error: %d\n", group, reg, val);
			return;
		}
		pr_info("write group%d reg_%02d: 0x%x\n", group, reg, val);
		break;
	case GROUP_CFG1: /* CFG1 register group */
		val = rockchip_fephy_group_write(phydev, GROUP_CFG1, reg, rval);
		if (val) {
			pr_err("group%d %2d write error: %d\n", group, reg, val);
			return;
		}
		pr_info("write group%d reg_%02d: 0x%x\n", group, reg, val);
		break;
	default:
		pr_err("error group num: %d\n", group);
		break;
	}
}

static ssize_t
phy_param_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct phy_device *phydev = to_phy_device(dev);
	struct rockchip_fephy_priv *priv = phydev->priv;

	switch (priv->current_group) {
	case GROUP_CFG0:
		rockchip_fephy_dump_cfg_group_regs(phydev, GROUP_CFG0, buf);
		break;
	case GROUP_WOL:
		rockchip_fephy_dump_wol_group_regs(phydev, GROUP_WOL, buf);
		break;
	case GROUP_CFG0_READ:
		 rockchip_fephy_dump_cfg_read_group_regs(phydev, GROUP_CFG0_READ, buf);
		break;
	case GROUP_BIST:
		rockchip_fephy_dump_bist_group_regs(phydev, GROUP_BIST, buf);
		break;
	case GROUP_AFE:
		rockchip_fephy_dump_afe_group_regs(phydev, GROUP_AFE, buf);
		break;
	case GROUP_CFG1:
		rockchip_fephy_dump_cfg1_group_regs(phydev, GROUP_CFG1, buf);
		break;
	default:
		pr_err("error group num: %d\n", priv->current_group);
		break;
	}

	return strlen(buf);
}

static ssize_t phy_param_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct phy_device *phydev = to_phy_device(dev);
	struct rockchip_fephy_priv *priv = phydev->priv;
	int arg1 = 0, arg2 = 0, arg3 = 0, ret;
	char *buff, *p, *para;
	char *argv[4];
	int argc;
	char cmd;

	buff = kstrdup(buf, GFP_KERNEL);
	if (!buff)
		return -EINVAL;

	p = buff;
	for (argc = 0; argc < 4; argc++) {
		para = strsep(&p, " ");
		if (!para) {
			argv[argc] = NULL;
			continue;
		}
		argv[argc] = para;
	}
	if (argc < 1 || argc > 4)
		goto end;

	if (argv[1]) {
		ret = kstrtoint(argv[1], 0, &arg1);
		if (ret)
			pr_err("kstrtoint failed\n");
	}
	if (argv[2]) {
		ret = kstrtoint(argv[2], 0, &arg2);
		if (ret)
			pr_err("kstrtoint failed\n");
	}
	if (argv[3]) {
		ret = kstrtoint(argv[3], 0, &arg3);
		if (ret)
			pr_err("kstrtoint failed\n");
	}

	cmd = argv[0][0];
	switch (cmd) {
	case 'R':
		rockchip_fephy_phy_read_priv_reg(phydev, arg1, arg2);
		priv->current_group = arg1;
		break;
	case 'W':
		rockchip_fephy_phy_write_priv_reg(phydev, arg1, arg2, arg3);
		priv->current_group = arg1;
		break;
	case 'd':
		priv->current_group = GROUP_CFG0;
		rockchip_fephy_dump_cfg_group_regs(phydev, GROUP_CFG0, NULL);
		break;
	case 'w':
		priv->current_group = GROUP_WOL;
		rockchip_fephy_dump_wol_group_regs(phydev, GROUP_WOL, NULL);
		break;
	case 'p':
		priv->current_group = GROUP_CFG0_READ;
		rockchip_fephy_dump_cfg_read_group_regs(phydev, GROUP_CFG0_READ, NULL);
		break;
	case 'b':
		priv->current_group = GROUP_BIST;
		rockchip_fephy_dump_bist_group_regs(phydev, GROUP_BIST, NULL);
		break;
	case 'a':
		priv->current_group = GROUP_AFE;
		rockchip_fephy_dump_afe_group_regs(phydev, GROUP_AFE, NULL);
		break;
	case 's':
		priv->current_group = GROUP_CFG1;
		rockchip_fephy_dump_cfg1_group_regs(phydev, GROUP_CFG1, NULL);
		break;
	case 'r':
		priv->current_group = GROUP_CFG0;
		if (phydev && phydev->drv->soft_reset)
			phydev->drv->soft_reset(phydev);
		break;
	default:
		goto end;
	}

	return count;

end:
	kfree(buff);
	return 0;
}

static DEVICE_ATTR_RW(phy_param);

static int rockchip_fephy_probe(struct phy_device *phydev)
{
	struct rockchip_fephy_priv *priv;
	int ret;

	priv = devm_kzalloc(&phydev->mdio.dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->regs = syscon_regmap_lookup_by_phandle(phydev->mdio.dev.of_node, "rockchip,macphy");
	if (IS_ERR_OR_NULL(priv->regs)) {
		phydev_err(phydev, "no macphy regmap found\n");
		return -EINVAL;
	}

	phydev->priv = priv;
	if (device_property_read_u32(&phydev->mdio.dev, "clock-frequency", &priv->clk_rate))
		priv->clk_rate = 24000000;

	priv->wol_irq = platform_get_irq_byname_optional(to_platform_device(&phydev->mdio.dev),
							 "wol_irq");
	if (priv->wol_irq == -EPROBE_DEFER)
		return priv->wol_irq;

	if (priv->wol_irq > 0) {
		ret = devm_request_threaded_irq(&phydev->mdio.dev, priv->wol_irq,
						NULL, rockchip_fephy_wol_irq_thread,
						IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_NO_AUTOEN,
						"rockchip_fephy_wol_irq", priv);
		if (ret) {
			phydev_err(phydev, "request wol_irq failed: %d\n", ret);
			return ret;
		}
		enable_irq_wake(priv->wol_irq);
	}

	priv->phydev = phydev;

	priv->pclk = devm_clk_get(&phydev->mdio.dev, "pclk");
	if (IS_ERR(priv->pclk))
		return PTR_ERR(priv->pclk);

	ret = clk_prepare(priv->pclk);
	if (ret)
		return ret;

	ret = rockchip_fephy_get_txamp_from_nvmem(phydev);
	if (ret)
		priv->txamp = DEFAULT_TXAMP;

	ret = rockchip_fephy_get_adc_offset_from_nvmem(phydev);
	if (ret) {
		priv->mdi_offset = 0;
		priv->mdix_offset = 0;
	}

	ret = device_create_file(&phydev->mdio.dev, &dev_attr_phy_param);
	if (ret) {
		clk_unprepare(priv->pclk);
		return ret;
	}

	return 0;
}

static void rockchip_fephy_remove(struct phy_device *phydev)
{
	struct rockchip_fephy_priv *priv = phydev->priv;

	clk_unprepare(priv->pclk);
	device_remove_file(&phydev->mdio.dev, &dev_attr_phy_param);
}

static int rockchip_fephy_suspend(struct phy_device *phydev)
{
	struct rockchip_fephy_priv *priv = phydev->priv;

	if (priv->wol_irq > 0) {
		rockchip_fephy_wol_enable(phydev);
		enable_irq(priv->wol_irq);
	}

	return genphy_suspend(phydev);
}

static int rockchip_fephy_resume(struct phy_device *phydev)
{
	struct rockchip_fephy_priv *priv = phydev->priv;

	if (priv->wol_irq > 0) {
		rockchip_fephy_wol_disable(phydev);
		disable_irq(priv->wol_irq);
	}

	return genphy_resume(phydev);
}

static struct phy_driver rockchip_fephy_driver[] = {
{
	.phy_id			= INTERNAL_FEPHY_ID,
	.phy_id_mask		= 0xffffffff,
	.name			= "Rockchip integrated FEPHY",
	/* PHY_BASIC_FEATURES */
	.features		= PHY_BASIC_FEATURES,
	.flags			= 0,
	.link_change_notify	= rockchip_feph_link_change_notify,
	.soft_reset		= genphy_soft_reset,
	.config_init		= rockchip_fephy_config_init,
	.config_aneg		= rockchip_fephy_config_aneg,
	.probe			= rockchip_fephy_probe,
	.remove			= rockchip_fephy_remove,
	.suspend		= rockchip_fephy_suspend,
	.resume			= rockchip_fephy_resume,
},
};

module_phy_driver(rockchip_fephy_driver);

static struct mdio_device_id __maybe_unused rockchip_fephy_tbl[] = {
	{ INTERNAL_FEPHY_ID, 0xffffffff },
	{ }
};

MODULE_DEVICE_TABLE(mdio, rockchip_fephy_tbl);

MODULE_AUTHOR("David Wu <david.wu@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip integrated FEPHYs driver");
MODULE_LICENSE("GPL");
