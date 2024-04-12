// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip PCIE3.0 phy driver
 *
 * Copyright (C) 2022 Rockchip Electronics Co., Ltd.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/phy/pcie.h>
#include <linux/phy/phy.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <dt-bindings/phy/phy-snps-pcie3.h>

/* Common definition */
#define RK_PCIE_SRAM_INIT_TIMEOUT		20000

/* Register for RK3568 */
#define GRF_PCIE30PHY_CON1			0x4
#define GRF_PCIE30PHY_CON4			0x10
#define GRF_PCIE30PHY_CON6			0x18
#define GRF_PCIE30PHY_CON9			0x24
#define GRF_PCIE30PHY_STATUS0			0x80
#define SRAM_INIT_DONE(reg)			(reg & BIT(14))

/* Register for RK3588 */
#define PHP_GRF_PCIESEL_CON			0x100
#define RK3588_PCIE3PHY_GRF_CMN_CON0		0x0
#define RK3588_PCIE3PHY_GRF_PHY0_STATUS1	0x904
#define RK3588_PCIE3PHY_GRF_PHY1_STATUS1	0xa04
#define RK3588_PCIE3PHY_GRF_PHY0_LN0_CON1	0x1004
#define RK3588_PCIE3PHY_GRF_PHY0_LN1_CON1	0x1104
#define RK3588_PCIE3PHY_GRF_PHY1_LN0_CON1	0x2004
#define RK3588_PCIE3PHY_GRF_PHY1_LN1_CON1	0x2104
#define RK3588_SRAM_INIT_DONE(reg)		((reg & 0xf) == 0xf)

#define RK3588_BIFURCATION_LANE_0_1		BIT(0)
#define RK3588_BIFURCATION_LANE_2_3		BIT(1)
#define RK3588_LANE_AGGREGATION		BIT(2)
#define RK3588_RX_CMN_REFCLK_MODE_EN		((BIT(7) << 16) |  BIT(7))
#define RK3588_RX_CMN_REFCLK_MODE_DIS		(BIT(7) << 16)
#define RK3588_PCIE1LN_SEL_EN			(GENMASK(1, 0) << 16)
#define RK3588_PCIE30_PHY_MODE_EN		(GENMASK(2, 0) << 16)

struct rockchip_p3phy_ops;

struct rockchip_p3phy_priv {
	const struct rockchip_p3phy_ops *ops;
	void __iomem *mmio;
	/* mode: RC, EP */
	int mode;
	/* pcie30_phymode: Aggregation, Bifurcation */
	int pcie30_phymode;
	struct regmap *phy_grf;
	struct regmap *pipe_grf;
	struct reset_control *p30phy;
	struct phy *phy;
	struct clk_bulk_data *clks;
	int num_clks;
	bool is_bifurcation;
	u32 rx_cmn_refclk_mode[4];
};

struct rockchip_p3phy_ops {
	int (*phy_init)(struct rockchip_p3phy_priv *priv);
	int (*phy_calibrate)(struct rockchip_p3phy_priv *priv);
};

static int rockchip_p3phy_set_mode(struct phy *phy, enum phy_mode mode, int submode)
{
	struct rockchip_p3phy_priv *priv = phy_get_drvdata(phy);

	/* Actually We don't care EP/RC mode, but just record it */
	switch (submode) {
	case PHY_MODE_PCIE_RC:
		priv->mode = PHY_MODE_PCIE_RC;
		break;
	case PHY_MODE_PCIE_EP:
		priv->mode = PHY_MODE_PCIE_EP;
		break;
	case PHY_MODE_PCIE_BIFURCATION:
		priv->is_bifurcation = true;
		break;
	default:
		dev_err(&phy->dev, "%s, invalid mode\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static const u16 phy_fw[] = {
	#include "phy-rockchip-snps-pcie3.fw"
};

static int rockchip_p3phy_rk3568_init(struct rockchip_p3phy_priv *priv)
{
	int i;
	int ret = 0;
	u32 reg;

	/* Deassert PCIe PMA output clamp mode */
	regmap_write(priv->phy_grf, GRF_PCIE30PHY_CON9,
		     (0x1 << 15) | (0x1 << 31));
	/* Set bifurcation if needed, and it doesn't care RC/EP */
	if (priv->is_bifurcation) {
		regmap_write(priv->phy_grf, GRF_PCIE30PHY_CON6,
			     0x1 | (0xf << 16));
		regmap_write(priv->phy_grf, GRF_PCIE30PHY_CON1,
			     (0x1 << 15) | (0x1 << 31));
	}
	regmap_write(priv->phy_grf, GRF_PCIE30PHY_CON4,
		     (0x0 << 14) | (0x1 << (14 + 16))); //sdram_ld_done
	regmap_write(priv->phy_grf, GRF_PCIE30PHY_CON4,
		     (0x0 << 13) | (0x1 << (13 + 16))); //sdram_bypass

	reset_control_deassert(priv->p30phy);

	ret = regmap_read_poll_timeout(priv->phy_grf,
				       GRF_PCIE30PHY_STATUS0,
				       reg, SRAM_INIT_DONE(reg),
				       0, 500);
	if (ret) {
		dev_err(&priv->phy->dev, "%s: lock failed 0x%x, check input refclk and power supply\n",
		       __func__, reg);
		goto out;
	}

	regmap_write(priv->phy_grf, GRF_PCIE30PHY_CON9,
		     (0x3 << 8) | (0x3 << (8 + 16))); //map to access sram
	for (i = 0; i < ARRAY_SIZE(phy_fw); i++)
		writel(phy_fw[i], priv->mmio + (i<<2));
	pr_info("snps pcie3phy FW update! size %ld\n", ARRAY_SIZE(phy_fw));

	regmap_write(priv->phy_grf, GRF_PCIE30PHY_CON9,
		     (0x0 << 8) | (0x3 << (8 + 16)));
	regmap_write(priv->phy_grf, GRF_PCIE30PHY_CON4,
		     (0x1 << 14) | (0x1 << (14 + 16))); //sdram_ld_done

out:
	return ret;
}

static int rockchip_p3phy_rk3568_calibrate(struct rockchip_p3phy_priv *priv)
{
	int ret;
	u32 reg;

	ret = regmap_read_poll_timeout(priv->phy_grf,
				       GRF_PCIE30PHY_STATUS0,
				       reg, SRAM_INIT_DONE(reg),
				       100, RK_PCIE_SRAM_INIT_TIMEOUT);
	if (ret)
		pr_err("%s: lock failed 0x%x, check input refclk and power supply\n",
		       __func__, reg);

	return ret;
}

static const struct rockchip_p3phy_ops rk3568_ops = {
	.phy_init = rockchip_p3phy_rk3568_init,
	.phy_calibrate = rockchip_p3phy_rk3568_calibrate,
};

static int rockchip_p3phy_rk3588_init(struct rockchip_p3phy_priv *priv)
{
	regmap_write(priv->phy_grf, RK3588_PCIE3PHY_GRF_PHY0_LN0_CON1,
		     priv->rx_cmn_refclk_mode[0] ? RK3588_RX_CMN_REFCLK_MODE_EN :
		     RK3588_RX_CMN_REFCLK_MODE_DIS);
	regmap_write(priv->phy_grf, RK3588_PCIE3PHY_GRF_PHY0_LN1_CON1,
		     priv->rx_cmn_refclk_mode[1] ? RK3588_RX_CMN_REFCLK_MODE_EN :
		     RK3588_RX_CMN_REFCLK_MODE_DIS);
	regmap_write(priv->phy_grf, RK3588_PCIE3PHY_GRF_PHY1_LN0_CON1,
		     priv->rx_cmn_refclk_mode[2] ? RK3588_RX_CMN_REFCLK_MODE_EN :
		     RK3588_RX_CMN_REFCLK_MODE_DIS);
	regmap_write(priv->phy_grf, RK3588_PCIE3PHY_GRF_PHY1_LN1_CON1,
		     priv->rx_cmn_refclk_mode[3] ? RK3588_RX_CMN_REFCLK_MODE_EN :
		     RK3588_RX_CMN_REFCLK_MODE_DIS);

	/* Deassert PCIe PMA output clamp mode */
	regmap_write(priv->phy_grf, RK3588_PCIE3PHY_GRF_CMN_CON0, BIT(8) | BIT(24));

	reset_control_deassert(priv->p30phy);

	return 0;
}

static int rockchip_p3phy_rk3588_calibrate(struct rockchip_p3phy_priv *priv)
{
	u32 phy0_status, phy1_status;
	int i, sleep_us = 100;
	bool check_both = (priv->pcie30_phymode == PHY_MODE_PCIE_AGGREGATION);

	for (i = 0; i < RK_PCIE_SRAM_INIT_TIMEOUT; i += sleep_us) {
		regmap_read(priv->phy_grf, RK3588_PCIE3PHY_GRF_PHY0_STATUS1, &phy0_status);
		regmap_read(priv->phy_grf, RK3588_PCIE3PHY_GRF_PHY1_STATUS1, &phy1_status);

		if (check_both) {
			if (RK3588_SRAM_INIT_DONE(phy0_status) && RK3588_SRAM_INIT_DONE(phy1_status))
				return 0;
		} else {
			if (RK3588_SRAM_INIT_DONE(phy0_status) || RK3588_SRAM_INIT_DONE(phy1_status))
				return 0;
		}

		usleep_range(sleep_us, sleep_us + 10);
	}

	pr_err("%s: lock failed p0=0x%x p1=0x%x, check input refclk and power supply\n",
	       __func__, phy0_status, phy1_status);

	return -ETIMEDOUT;
}

static const struct rockchip_p3phy_ops rk3588_ops = {
	.phy_init = rockchip_p3phy_rk3588_init,
	.phy_calibrate = rockchip_p3phy_rk3588_calibrate,
};

static int rockchip_p3phy_init(struct phy *phy)
{
	struct rockchip_p3phy_priv *priv = phy_get_drvdata(phy);
	int ret;

	ret = clk_bulk_prepare_enable(priv->num_clks, priv->clks);
	if (ret) {
		dev_err(&priv->phy->dev, "failed to enable PCIe bulk clks %d\n", ret);
		return ret;
	}

	reset_control_assert(priv->p30phy);
	udelay(1);

	if (priv->ops->phy_init) {
		ret = priv->ops->phy_init(priv);
		if (ret)
			clk_bulk_disable_unprepare(priv->num_clks, priv->clks);
	}

	return ret;
}

static int rockchip_p3phy_exit(struct phy *phy)
{
	struct rockchip_p3phy_priv *priv = phy_get_drvdata(phy);

	clk_bulk_disable_unprepare(priv->num_clks, priv->clks);
	reset_control_assert(priv->p30phy);
	return 0;
}

static const struct phy_ops rockchip_p3phy_ops = {
	.init = rockchip_p3phy_init,
	.exit = rockchip_p3phy_exit,
	.set_mode = rockchip_p3phy_set_mode,
	.owner = THIS_MODULE,
};

static int rockchip_p3phy_probe(struct platform_device *pdev)
{
	struct phy_provider *phy_provider;
	struct device *dev = &pdev->dev;
	struct rockchip_p3phy_priv *priv;
	struct device_node *np = dev->of_node;
	int ret;
	u32 val, reg;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->mmio = devm_platform_get_and_ioremap_resource(pdev, 0, NULL);
	if (IS_ERR(priv->mmio)) {
		ret = PTR_ERR(priv->mmio);
		return ret;
	}

	priv->ops = of_device_get_match_data(&pdev->dev);
	if (!priv->ops) {
		dev_err(dev, "no of match data provided\n");
		return -EINVAL;
	}

	priv->num_clks = devm_clk_bulk_get_all(dev, &priv->clks);
	if (priv->num_clks < 1)
		return -ENODEV;

	priv->phy_grf = syscon_regmap_lookup_by_phandle(np, "rockchip,phy-grf");
	if (IS_ERR(priv->phy_grf)) {
		dev_err(dev, "failed to find rockchip,phy_grf regmap\n");
		return PTR_ERR(priv->phy_grf);
	}

	priv->pipe_grf = syscon_regmap_lookup_by_phandle(dev->of_node,
							 "rockchip,pipe-grf");
	if (IS_ERR(priv->pipe_grf))
		dev_info(dev, "failed to find rockchip,pipe_grf regmap\n");

	/* Configuring grf with clk enabled. */
	ret = clk_bulk_prepare_enable(priv->num_clks, priv->clks);
	if (ret) {
		pr_err("failed to enable PCIe bulk clks %d\n", ret);
		return ret;
	}

	ret = device_property_read_u32(dev, "rockchip,pcie30-phymode", &val);
	if (!ret) {
		priv->pcie30_phymode = val;
		if (priv->pcie30_phymode > 4)
			priv->pcie30_phymode = PHY_MODE_PCIE_AGGREGATION;
		regmap_write(priv->phy_grf, RK3588_PCIE3PHY_GRF_CMN_CON0,
			     (0x7<<16) | priv->pcie30_phymode);
	} else {
		priv->pcie30_phymode = PHY_MODE_PCIE_AGGREGATION;
	}

	/* Set pcie1ln_sel in PHP_GRF_PCIESEL_CON */
	if (!IS_ERR(priv->pipe_grf)) {
		reg = priv->pcie30_phymode & 3;
		if (reg)
			regmap_write(priv->pipe_grf, PHP_GRF_PCIESEL_CON,
				     (reg << 16) | reg);
	};

	clk_bulk_disable_unprepare(priv->num_clks, priv->clks);


	ret = of_property_read_variable_u32_array(dev->of_node,
						  "rockchip,rx-common-refclk-mode",
						  priv->rx_cmn_refclk_mode, 1,
						  ARRAY_SIZE(priv->rx_cmn_refclk_mode));
	/*
	 * if no rockchip,rx-common-refclk-mode, assume enabled for all lanes in
	 * order to be DT backwards compatible. (Since HW reset val is enabled.)
	 */
	if (ret == -EINVAL) {
		for (int i = 0; i < ARRAY_SIZE(priv->rx_cmn_refclk_mode); i++)
			priv->rx_cmn_refclk_mode[i] = 1;
	} else if (ret < 0) {
		dev_err(dev, "failed to read rockchip,rx-common-refclk-mode property %d\n",
			ret);
		return ret;
	}

	priv->phy = devm_phy_create(dev, NULL, &rockchip_p3phy_ops);
	if (IS_ERR(priv->phy)) {
		dev_err(dev, "failed to create combphy\n");
		return PTR_ERR(priv->phy);
	}

	priv->p30phy = devm_reset_control_get(dev, "phy");
	if (IS_ERR(priv->p30phy)) {
		dev_warn(dev, "no phy reset control specified\n");
		priv->p30phy = NULL;
	}

	dev_set_drvdata(dev, priv);
	phy_set_drvdata(priv->phy, priv);
	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	return PTR_ERR_OR_ZERO(phy_provider);
}

static const struct of_device_id rockchip_p3phy_of_match[] = {
	{ .compatible = "rockchip,rk3568-pcie3-phy", .data = &rk3568_ops },
	{ .compatible = "rockchip,rk3588-pcie3-phy", .data = &rk3588_ops },
	{ },
};
MODULE_DEVICE_TABLE(of, rockchip_p3phy_of_match);

static struct platform_driver rockchip_p3phy_driver = {
	.probe	= rockchip_p3phy_probe,
	.driver = {
		.name = "rockchip-snps-pcie3-phy",
		.of_match_table = rockchip_p3phy_of_match,
	},
};
module_platform_driver(rockchip_p3phy_driver);
MODULE_DESCRIPTION("Rockchip Synopsys PCIe 3.0 PHY driver");
MODULE_LICENSE("GPL");
