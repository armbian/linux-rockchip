// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for Motorcomm PHYs
 *
 * Author: Peter Geis <pgwipeout@gmail.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of_device.h>
#include <linux/phy.h>

#define PHY_ID_YT8011		0x4f51eb01
#define PHY_ID_YT8511		0x0000010a
#define PHY_ID_YT8512		0x00000118
#define PHY_ID_YT8512B		0x00000128
#define PHY_ID_YT8522		0x4f51e928
#define PHY_ID_YT8531S		0x4f51e91a
#define PHY_ID_YT8531		0x4f51e91b

enum {
	YT8011_RGMII_DVDDIO_1V8 = 1,
	YT8011_RGMII_DVDDIO_2V5,
	YT8011_RGMII_DVDDIO_3V3
};

struct yt8011_priv {
	u8 polling_mode;
	u8 chip_mode;
	u8 vddio;
};

#define YT8011_SPEED_MODE		0xc000
#define YT8011_DUPLEX			0x2000
#define YT8011_SPEED_MODE_BIT		14
#define YT8011_DUPLEX_BIT		13
#define YT8011_LINK_STATUS_BIT		10

#define REG_PHY_SPEC_STATUS		0x11
#define REG_DEBUG_ADDR_OFFSET		0x1e
#define REG_DEBUG_DATA			0x1f
#define REG_MII_MMD_CTRL		0x0D  /* MMD access control register */
#define REG_MII_MMD_DATA		0x0E  /* MMD access data register */

#define YT8511_PAGE_SELECT	0x1e
#define YT8511_PAGE		0x1f
#define YT8511_EXT_CLK_GATE	0x0c
#define YT8511_EXT_DELAY_DRIVE	0x0d
#define YT8511_EXT_SLEEP_CTRL	0x27

/* 2b00 25m from pll
 * 2b01 25m from xtl *default*
 * 2b10 62.m from pll
 * 2b11 125m from pll
 */
#define YT8511_CLK_125M		(BIT(2) | BIT(1))
#define YT8511_PLLON_SLP	BIT(14)

/* RX Delay enabled = 1.8ns 1000T, 8ns 10/100T */
#define YT8511_DELAY_RX		BIT(0)

/* TX Gig-E Delay is bits 7:4, default 0x5
 * TX Fast-E Delay is bits 15:12, default 0xf
 * Delay = 150ps * N - 250ps
 * On = 2000ps, off = 50ps
 */
#define YT8511_DELAY_GE_TX_EN	(0xf << 4)
#define YT8511_DELAY_GE_TX_DIS	(0x2 << 4)
#define YT8511_DELAY_FE_TX_EN	(0xf << 12)
#define YT8511_DELAY_FE_TX_DIS	(0x2 << 12)

#define YT8512_EXTREG_AFE_PLL		0x50
#define YT8512_EXTREG_EXTEND_COMBO	0x4000
#define YT8512_EXTREG_LED0		0x40c0
#define YT8512_EXTREG_LED1		0x40c3

#define YT8512_EXTREG_SLEEP_CONTROL1	0x2027

#define YT_SOFTWARE_RESET		0x8000

#define YT8512_CONFIG_PLL_REFCLK_SEL_EN	0x0040
#define YT8512_CONTROL1_RMII_EN		0x0001
#define YT8512_LED0_ACT_BLK_IND		0x1000
#define YT8512_LED0_DIS_LED_AN_TRY	0x0001
#define YT8512_LED0_BT_BLK_EN		0x0002
#define YT8512_LED0_HT_BLK_EN		0x0004
#define YT8512_LED0_COL_BLK_EN		0x0008
#define YT8512_LED0_BT_ON_EN		0x0010
#define YT8512_LED1_BT_ON_EN		0x0010
#define YT8512_LED1_TXACT_BLK_EN	0x0100
#define YT8512_LED1_RXACT_BLK_EN	0x0200
#define YT8512_SPEED_MODE		0xc000
#define YT8512_DUPLEX			0x2000

#define YT8512_SPEED_MODE_BIT		14
#define YT8512_DUPLEX_BIT		13
#define YT8512_EN_SLEEP_SW_BIT		15

#define YT8522_TX_CLK_DELAY             0x4210
#define YT8522_ANAGLOG_IF_CTRL          0x4008
#define YT8522_DAC_CTRL                 0x2057
#define YT8522_INTERPOLATOR_FILTER_1    0x14
#define YT8522_INTERPOLATOR_FILTER_2    0x15
#define YT8522_EXTENDED_COMBO_CTRL_1    0x4000

#define YTXXXX_SPEED_MODE               0xc000
#define YTXXXX_DUPLEX                   0x2000
#define YTXXXX_SPEED_MODE_BIT           14
#define YTXXXX_DUPLEX_BIT               13
#define YTXXXX_AUTO_NEGOTIATION_BIT     12
#define YTXXXX_ASYMMETRIC_PAUSE_BIT     11
#define YTXXXX_PAUSE_BIT                10
#define YTXXXX_LINK_STATUS_BIT          10

/* if system depends on ethernet packet to restore from sleep,
 * please define this macro to 1 otherwise, define it to 0.
 */
#define SYS_WAKEUP_BASED_ON_ETH_PKT	1

/* to enable system WOL feature of phy, please define this macro to 1
 * otherwise, define it to 0.
 */
#define YTPHY_WOL_FEATURE_ENABLE	0

#if (YTPHY_WOL_FEATURE_ENABLE)
#undef SYS_WAKEUP_BASED_ON_ETH_PKT
#define SYS_WAKEUP_BASED_ON_ETH_PKT	1
#endif

struct yt8xxx_priv {
	u8 polling_mode;
	u8 chip_mode;
};

/* for YT8531 package A xtal init config */
#define YTPHY8531A_XTAL_INIT		0

#define REG_PHY_SPEC_STATUS		0x11
#define REG_DEBUG_ADDR_OFFSET		0x1e
#define REG_DEBUG_DATA			0x1f

#define YT8521_EXTREG_SLEEP_CONTROL1	0x27
#define YT8521_EN_SLEEP_SW_BIT		15

#define YT8521_SPEED_MODE		0xc000
#define YT8521_DUPLEX			0x2000
#define YT8521_SPEED_MODE_BIT		14
#define YT8521_DUPLEX_BIT		13
#define YT8521_LINK_STATUS_BIT		10

/* YT8521 polling mode */
#define YT8521_PHY_MODE_FIBER		1 /* fiber mode only */
#define YT8521_PHY_MODE_UTP		2 /* utp mode only */
#define YT8521_PHY_MODE_POLL		3 /* fiber and utp, poll mode */

static int yt8521_hw_strap_polling(struct phy_device *phydev);
#define YT8521_PHY_MODE_CURR		yt8521_hw_strap_polling(phydev)

static int yt8511_read_page(struct phy_device *phydev)
{
	return __phy_read(phydev, YT8511_PAGE_SELECT);
};

static int yt8511_write_page(struct phy_device *phydev, int page)
{
	return __phy_write(phydev, YT8511_PAGE_SELECT, page);
};

static int yt8511_config_init(struct phy_device *phydev)
{
	int oldpage, ret = 0;
	unsigned int ge, fe;

	oldpage = phy_select_page(phydev, YT8511_EXT_CLK_GATE);
	if (oldpage < 0)
		goto err_restore_page;

	/* set rgmii delay mode */
	switch (phydev->interface) {
	case PHY_INTERFACE_MODE_RGMII:
		ge = YT8511_DELAY_GE_TX_DIS;
		fe = YT8511_DELAY_FE_TX_DIS;
		break;
	case PHY_INTERFACE_MODE_RGMII_RXID:
		ge = YT8511_DELAY_RX | YT8511_DELAY_GE_TX_DIS;
		fe = YT8511_DELAY_FE_TX_DIS;
		break;
	case PHY_INTERFACE_MODE_RGMII_TXID:
		ge = YT8511_DELAY_GE_TX_EN;
		fe = YT8511_DELAY_FE_TX_EN;
		break;
	case PHY_INTERFACE_MODE_RGMII_ID:
		ge = YT8511_DELAY_RX | YT8511_DELAY_GE_TX_EN;
		fe = YT8511_DELAY_FE_TX_EN;
		break;
	default: /* do not support other modes */
		ret = -EOPNOTSUPP;
		goto err_restore_page;
	}

	ret = __phy_modify(phydev, YT8511_PAGE, (YT8511_DELAY_RX | YT8511_DELAY_GE_TX_EN), ge);
	if (ret < 0)
		goto err_restore_page;

	/* set clock mode to 125mhz */
	ret = __phy_modify(phydev, YT8511_PAGE, 0, YT8511_CLK_125M);
	if (ret < 0)
		goto err_restore_page;

	/* fast ethernet delay is in a separate page */
	ret = __phy_write(phydev, YT8511_PAGE_SELECT, YT8511_EXT_DELAY_DRIVE);
	if (ret < 0)
		goto err_restore_page;

	ret = __phy_modify(phydev, YT8511_PAGE, YT8511_DELAY_FE_TX_EN, fe);
	if (ret < 0)
		goto err_restore_page;

	/* leave pll enabled in sleep */
	ret = __phy_write(phydev, YT8511_PAGE_SELECT, YT8511_EXT_SLEEP_CTRL);
	if (ret < 0)
		goto err_restore_page;

	ret = __phy_modify(phydev, YT8511_PAGE, 0, YT8511_PLLON_SLP);
	if (ret < 0)
		goto err_restore_page;

err_restore_page:
	return phy_restore_page(phydev, oldpage, ret);
}

static u32 ytphy_read_ext(struct phy_device *phydev, u32 regnum)
{
	int ret;

	phy_lock_mdio_bus(phydev);
	ret = __phy_write(phydev, REG_DEBUG_ADDR_OFFSET, regnum);
	if (ret < 0)
		goto err_handle;

	ret = __phy_read(phydev, REG_DEBUG_DATA);
	if (ret < 0)
		goto err_handle;

err_handle:
	phy_unlock_mdio_bus(phydev);
	return ret;
}

static int ytphy_write_ext(struct phy_device *phydev, u32 regnum, u16 val)
{
	int ret;

	phy_lock_mdio_bus(phydev);
	ret = __phy_write(phydev, REG_DEBUG_ADDR_OFFSET, regnum);
	if (ret < 0)
		goto err_handle;

	ret = __phy_write(phydev, REG_DEBUG_DATA, val);
	if (ret < 0)
		goto err_handle;

err_handle:
	phy_unlock_mdio_bus(phydev);
	return ret;
}

static int __maybe_unused ytphy_write_mmd(struct phy_device *phydev, u16 device, u16 reg, u16 value)
{
	int ret = 0;

	phy_lock_mdio_bus(phydev);

	__phy_write(phydev, REG_MII_MMD_CTRL, device);
	__phy_write(phydev, REG_MII_MMD_DATA, reg);
	__phy_write(phydev, REG_MII_MMD_CTRL, device | 0x4000);
	__phy_write(phydev, REG_MII_MMD_DATA, value);

	phy_unlock_mdio_bus(phydev);

	return ret;
}

static int yt8011_probe(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	struct yt8011_priv *priv;
	int chip_config;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	phydev->priv = priv;

	/* ext reg 0x9030 bit0
	 * 0 = chip works in RGMII mode; 1 = chip works in SGMII mode
	 */
	chip_config = ytphy_read_ext(phydev, 0x9030);
	priv->chip_mode = chip_config & 0x1;

	return 0;
}

static int yt8011_config_aneg(struct phy_device *phydev)
{
	phydev->speed = SPEED_1000;

	return 0;
}

static int yt8011_config_vddio(struct phy_device *phydev)
{
	struct yt8011_priv *priv = phydev->priv;

	if (!(priv->chip_mode)) { /* rgmii config */
		switch (priv->vddio) {
		case YT8011_RGMII_DVDDIO_2V5:
			dev_info(&phydev->mdio.dev, "config PHY vddio 2v5\n");
			ytphy_write_ext(phydev, 0x9000, 0x8000);
			ytphy_write_ext(phydev, 0x0062, 0x0000);
			ytphy_write_ext(phydev, 0x9000, 0x0000);
			ytphy_write_ext(phydev, 0x9031, 0xb200);
			ytphy_write_ext(phydev, 0x9111, 0x5);
			ytphy_write_ext(phydev, 0x9114, 0x3939);
			ytphy_write_ext(phydev, 0x9112, 0xf);
			ytphy_write_ext(phydev, 0x9110, 0x0);
			ytphy_write_ext(phydev, 0x9113, 0x10);
			ytphy_write_ext(phydev, 0x903d, 0x2);
			break;
		case YT8011_RGMII_DVDDIO_1V8:
			dev_info(&phydev->mdio.dev, "config PHY for 1v8\n");
			ytphy_write_ext(phydev, 0x9000, 0x8000);
			ytphy_write_ext(phydev, 0x0062, 0x0000);
			ytphy_write_ext(phydev, 0x9000, 0x0000);
			ytphy_write_ext(phydev, 0x9031, 0xb200);
			ytphy_write_ext(phydev, 0x9116, 0x6);
			ytphy_write_ext(phydev, 0x9119, 0x3939);
			ytphy_write_ext(phydev, 0x9117, 0xf);
			ytphy_write_ext(phydev, 0x9115, 0x0);
			ytphy_write_ext(phydev, 0x9118, 0x20);
			ytphy_write_ext(phydev, 0x903d, 0x3);
			break;
		case YT8011_RGMII_DVDDIO_3V3:
		default:
			dev_info(&phydev->mdio.dev, "config PHY for 3v3\n");
			ytphy_write_ext(phydev, 0x9000, 0x8000);
			ytphy_write_ext(phydev, 0x0062, 0x0000);
			ytphy_write_ext(phydev, 0x9000, 0x0000);
			ytphy_write_ext(phydev, 0x9031, 0xb200);
			ytphy_write_ext(phydev, 0x903b, 0x0040);
			ytphy_write_ext(phydev, 0x903e, 0x3b3b);
			ytphy_write_ext(phydev, 0x903c, 0xf);
			ytphy_write_ext(phydev, 0x903d, 0x1000);
			ytphy_write_ext(phydev, 0x9038, 0x0000);
			break;
		}
	}
	return 0;
}
static int yt8011_aneg_done(struct phy_device *phydev)
{
	int link_utp = 0;

	/* UTP */
	ytphy_write_ext(phydev, 0x9000, 0);
	link_utp = !!(phy_read(phydev, REG_PHY_SPEC_STATUS) & (BIT(YT8011_LINK_STATUS_BIT)));

	return !!(link_utp);
}

static int yt8011_automotive_adjust_status(struct phy_device *phydev, int val)
{
	int speed_mode;
	int speed = SPEED_UNKNOWN;

	speed_mode = (val & YT8011_SPEED_MODE) >> YT8011_SPEED_MODE_BIT;
	switch (speed_mode) {
	case 1:
		speed = SPEED_100;
		break;
	case 2:
		speed = SPEED_1000;
		break;
	default:
		speed = SPEED_UNKNOWN;
		break;
	}

	phydev->speed = speed;
	phydev->duplex = DUPLEX_FULL;

	return 0;
}

static int yt8011_read_status(struct phy_device *phydev)
{
	int ret;
	int val;
	int link;
	int link_utp = 0;

	/* UTP */
	ret = ytphy_write_ext(phydev, 0x9000, 0x0);
	if (ret < 0)
		return ret;

	val = phy_read(phydev, REG_PHY_SPEC_STATUS);
	if (val < 0)
		return val;

	link = val & (BIT(YT8011_LINK_STATUS_BIT));
	if (link) {
		link_utp = 1;
		yt8011_automotive_adjust_status(phydev, val);
	} else {
		link_utp = 0;
	}

	if (link_utp) {
		if (phydev->link == 0)
			phydev->link = 1;
	} else {
		if (phydev->link == 1)
			phydev->link = 0;
	}

	if (link_utp)
		ytphy_write_ext(phydev, 0x9000, 0x0);

	return 0;
}

static int ytphy_soft_reset(struct phy_device *phydev)
{
	int ret = 0, val = 0;

	val = phy_read(phydev, MII_BMCR);
	if (val < 0)
		return val;

	ret = phy_write(phydev, MII_BMCR, val | BMCR_RESET);
	if (ret < 0)
		return ret;

	return ret;
}

static int yt8011_soft_reset(struct phy_device *phydev)
{
	struct yt8011_priv *priv = phydev->priv;

	/* utp */
	ytphy_write_ext(phydev, 0x9000, 0x0);
	ytphy_soft_reset(phydev);

	if (priv->chip_mode) { /* sgmm */
		ytphy_write_ext(phydev, 0x9000, 0x8000);
		ytphy_soft_reset(phydev);

		/* restore utp space */
		ytphy_write_ext(phydev, 0x9000, 0x0);
	}

	return 0;
}

static int yt8512_clk_init(struct phy_device *phydev)
{
	struct device_node *node = phydev->mdio.dev.of_node;
	const char *strings = NULL;
	int ret;
	int val;

	if (node && node->parent && node->parent->parent) {
		ret = of_property_read_string(node->parent->parent,
					      "clock_in_out", &strings);
		if (!ret) {
			if (!strcmp(strings, "input"))
				return 0;
		}
	}

	val = ytphy_read_ext(phydev, YT8512_EXTREG_AFE_PLL);
	if (val < 0)
		return val;

	val |= YT8512_CONFIG_PLL_REFCLK_SEL_EN;

	ret = ytphy_write_ext(phydev, YT8512_EXTREG_AFE_PLL, val);
	if (ret < 0)
		return ret;

	val = ytphy_read_ext(phydev, YT8512_EXTREG_EXTEND_COMBO);
	if (val < 0)
		return val;

	val |= YT8512_CONTROL1_RMII_EN;

	ret = ytphy_write_ext(phydev, YT8512_EXTREG_EXTEND_COMBO, val);
	if (ret < 0)
		return ret;

	val = phy_read(phydev, MII_BMCR);
	if (val < 0)
		return val;

	val |= YT_SOFTWARE_RESET;
	ret = phy_write(phydev, MII_BMCR, val);

	return ret;
}

static int yt8512_led_init(struct phy_device *phydev)
{
	int ret;
	int val;
	int mask;

	val = ytphy_read_ext(phydev, YT8512_EXTREG_LED0);
	if (val < 0)
		return val;

	val |= YT8512_LED0_ACT_BLK_IND;

	mask = YT8512_LED0_DIS_LED_AN_TRY | YT8512_LED0_BT_BLK_EN |
		YT8512_LED0_HT_BLK_EN | YT8512_LED0_COL_BLK_EN |
		YT8512_LED0_BT_ON_EN;
	val &= ~mask;

	ret = ytphy_write_ext(phydev, YT8512_EXTREG_LED0, val);
	if (ret < 0)
		return ret;

	val = ytphy_read_ext(phydev, YT8512_EXTREG_LED1);
	if (val < 0)
		return val;

	val |= YT8512_LED1_BT_ON_EN;

	mask = YT8512_LED1_TXACT_BLK_EN | YT8512_LED1_RXACT_BLK_EN;
	val &= ~mask;

	ret = ytphy_write_ext(phydev, YT8512_LED1_BT_ON_EN, val);

	return ret;
}

static int yt8011_config_init(struct phy_device *phydev)
{
	struct yt8011_priv *priv = phydev->priv;
	struct device_node *np = phydev->mdio.dev.of_node;
	const char *vddio_conf;

	phydev->autoneg = AUTONEG_DISABLE;

	if (!np) {
		dev_err(&phydev->mdio.dev, "Device Tree node is missing\n");
		priv->vddio = YT8011_RGMII_DVDDIO_3V3;
	} else {
		if (of_property_read_string(np, "motorcomm,vddio", &vddio_conf)) {
			dev_err(&phydev->mdio.dev, "Missing 'motorcomm,vddio' property in DTS, using 3v3 default\n");
			priv->vddio = YT8011_RGMII_DVDDIO_3V3;
		} else {
			if (!strcasecmp(vddio_conf, "1v8")) {
				priv->vddio = YT8011_RGMII_DVDDIO_1V8;
			} else if (!strcasecmp(vddio_conf, "2v5")) {
				priv->vddio = YT8011_RGMII_DVDDIO_2V5;
			} else if (!strcasecmp(vddio_conf, "3v3")) {
				priv->vddio = YT8011_RGMII_DVDDIO_3V3;
			} else {
				dev_err(&phydev->mdio.dev, "Invalid 'motorcomm,vddio' value, using 3v3 default\n");
				priv->vddio = YT8011_RGMII_DVDDIO_3V3;
			}
		}
	}
	/* UTP */
	ytphy_write_ext(phydev, 0x9000, 0x0);

	ytphy_write_ext(phydev, 0x1008, 0x2119);
	ytphy_write_ext(phydev, 0x1092, 0x712);
	ytphy_write_ext(phydev, 0x90bc, 0x6661);
	ytphy_write_ext(phydev, 0x90b9, 0x620b);
	ytphy_write_ext(phydev, 0x2001, 0x6418);
	ytphy_write_ext(phydev, 0x1019, 0x3712);
	ytphy_write_ext(phydev, 0x101a, 0x3713);
	ytphy_write_ext(phydev, 0x2015, 0x1012);
	ytphy_write_ext(phydev, 0x2005, 0x810);
	ytphy_write_ext(phydev, 0x2013, 0xff06);
	ytphy_write_ext(phydev, 0x1053, 0xf);
	ytphy_write_ext(phydev, 0x105e, 0xa46c);
	ytphy_write_ext(phydev, 0x1088, 0x002b);
	ytphy_write_ext(phydev, 0x1088, 0x002b);
	ytphy_write_ext(phydev, 0x1088, 0xb);
	ytphy_write_ext(phydev, 0x3008, 0x143);
	ytphy_write_ext(phydev, 0x3009, 0x1918);
	ytphy_write_ext(phydev, 0x9095, 0x1a1a);
	ytphy_write_ext(phydev, 0x9096, 0x1a10);
	ytphy_write_ext(phydev, 0x9097, 0x101a);
	ytphy_write_ext(phydev, 0x9098, 0x01ff);
	yt8011_config_vddio(phydev);

	ytphy_soft_reset(phydev);

	netdev_info(phydev->attached_dev, "%s done, phy addr: %d\n", __func__, phydev->mdio.addr);

	return 0;
}

static int yt8512_config_init(struct phy_device *phydev)
{
	int ret;
	int val;

	ret = yt8512_clk_init(phydev);
	if (ret < 0)
		return ret;

	ret = yt8512_led_init(phydev);
	if (ret < 0)
		return ret;

	/* disable auto sleep */
	val = ytphy_read_ext(phydev, YT8512_EXTREG_SLEEP_CONTROL1);
	if (val < 0)
		return val;

	val &= (~BIT(YT8512_EN_SLEEP_SW_BIT));

	ret = ytphy_write_ext(phydev, YT8512_EXTREG_SLEEP_CONTROL1, val);
	if (ret < 0)
		return ret;

	return ret;
}

static int yt8512_read_status(struct phy_device *phydev)
{
	int ret;
	int val;
	int speed, speed_mode, duplex;

	ret = genphy_update_link(phydev);
	if (ret)
		return ret;

	val = phy_read(phydev, REG_PHY_SPEC_STATUS);
	if (val < 0)
		return val;

	duplex = (val & YT8512_DUPLEX) >> YT8512_DUPLEX_BIT;
	speed_mode = (val & YT8512_SPEED_MODE) >> YT8512_SPEED_MODE_BIT;
	switch (speed_mode) {
	case 0:
		speed = SPEED_10;
		break;
	case 1:
		speed = SPEED_100;
		break;
	case 2:
	case 3:
	default:
		speed = SPEED_UNKNOWN;
		break;
	}

	phydev->speed = speed;
	phydev->duplex = duplex;

	return 0;
}

static int yt8521_soft_reset(struct phy_device *phydev)
{
	int ret = 0, val;

	if (YT8521_PHY_MODE_CURR == YT8521_PHY_MODE_UTP) {
		ytphy_write_ext(phydev, 0xa000, 0);
		ret = ytphy_soft_reset(phydev);
		if (ret < 0)
			return ret;
	}

	if (YT8521_PHY_MODE_CURR == YT8521_PHY_MODE_FIBER) {
		ytphy_write_ext(phydev, 0xa000, 2);
		ret = ytphy_soft_reset(phydev);
		if (ret < 0)
			return ret;

		ytphy_write_ext(phydev, 0xa000, 0);
	}

	if (YT8521_PHY_MODE_CURR == YT8521_PHY_MODE_POLL) {
		val = ytphy_read_ext(phydev, 0xa001);
		ytphy_write_ext(phydev, 0xa001, (val & ~0x8000));

		ytphy_write_ext(phydev, 0xa000, 0);
		ret = ytphy_soft_reset(phydev);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int yt8521_hw_strap_polling(struct phy_device *phydev)
{
	int val = 0;

	val = ytphy_read_ext(phydev, 0xa001) & 0x7;
	switch (val) {
	case 1:
	case 4:
	case 5:
		return YT8521_PHY_MODE_FIBER;
	case 2:
	case 6:
	case 7:
		return YT8521_PHY_MODE_POLL;
	case 3:
	case 0:
	default:
		return YT8521_PHY_MODE_UTP;
	}
}

static int yt8521_config_init(struct phy_device *phydev)
{
	int ret, hw_strap_mode;
	int val;

#if (YTPHY_WOL_FEATURE_ENABLE)
	struct ethtool_wolinfo wol;

	/* set phy wol enable */
	memset(&wol, 0x0, sizeof(struct ethtool_wolinfo));
	wol.wolopts |= WAKE_MAGIC;
	ytphy_wol_feature_set(phydev, &wol);
#endif

	phydev->irq = PHY_POLL;
	/* NOTE: this function should not be called more than one for each chip. */
	hw_strap_mode = ytphy_read_ext(phydev, 0xa001) & 0x7;

	ytphy_write_ext(phydev, 0xa000, 0);

	/* disable auto sleep */
	val = ytphy_read_ext(phydev, YT8521_EXTREG_SLEEP_CONTROL1);
	if (val < 0)
		return val;

	val &= (~BIT(YT8521_EN_SLEEP_SW_BIT));
	ret = ytphy_write_ext(phydev, YT8521_EXTREG_SLEEP_CONTROL1, val);
	if (ret < 0)
		return ret;

	/* enable RXC clock when no wire plug */
	val = ytphy_read_ext(phydev, 0xc);
	if (val < 0)
		return val;
	val &= ~(1 << 12);
	ret = ytphy_write_ext(phydev, 0xc, val);
	if (ret < 0)
		return ret;

	netdev_info(phydev->attached_dev, "%s done, phy addr: %d, strap mode = %d, polling mode = %d\n",
		    __func__, phydev->mdio.addr, hw_strap_mode, yt8521_hw_strap_polling(phydev));

	return ret;
}

/* for fiber mode, there is no 10M speed mode and
 * this function is for this purpose.
 */
static int yt8521_adjust_status(struct phy_device *phydev, int val, int is_utp)
{
	int speed = SPEED_UNKNOWN;
	int speed_mode, duplex;

	if (is_utp)
		duplex = (val & YT8521_DUPLEX) >> YT8521_DUPLEX_BIT;
	else
		duplex = 1;
	speed_mode = (val & YT8521_SPEED_MODE) >> YT8521_SPEED_MODE_BIT;
	switch (speed_mode) {
	case 0:
		if (is_utp)
			speed = SPEED_10;
		break;
	case 1:
		speed = SPEED_100;
		break;
	case 2:
		speed = SPEED_1000;
		break;
	case 3:
		break;
	default:
		speed = SPEED_UNKNOWN;
		break;
	}

	phydev->speed = speed;
	phydev->duplex = duplex;

	return 0;
}

/* for fiber mode, when speed is 100M, there is no definition for
 * autonegotiation, and this function handles this case and return
 * 1 per linux kernel's polling.
 */
static int yt8521_aneg_done(struct phy_device *phydev)
{
	int link_fiber = 0, link_utp = 0;

	/* reading Fiber */
	ytphy_write_ext(phydev, 0xa000, 2);
	link_fiber = !!(phy_read(phydev, REG_PHY_SPEC_STATUS) & (BIT(YT8521_LINK_STATUS_BIT)));

	/* reading UTP */
	ytphy_write_ext(phydev, 0xa000, 0);
	if (!link_fiber)
		link_utp = !!(phy_read(phydev, REG_PHY_SPEC_STATUS) & (BIT(YT8521_LINK_STATUS_BIT)));

	netdev_info(phydev->attached_dev, "%s, phy addr: %d, link_fiber: %d, link_utp: %d\n",
		    __func__, phydev->mdio.addr, link_fiber, link_utp);
	return !!(link_fiber | link_utp);
}

static int yt8521_read_status(struct phy_device *phydev)
{
	int link_utp = 0, link_fiber = 0;
	int yt8521_fiber_latch_val;
	int yt8521_fiber_curr_val;
	int link, ret;
	int val;

	if (YT8521_PHY_MODE_CURR != YT8521_PHY_MODE_FIBER) {
		/* reading UTP */
		ret = ytphy_write_ext(phydev, 0xa000, 0);
		if (ret < 0)
			return ret;

		val = phy_read(phydev, REG_PHY_SPEC_STATUS);
		if (val < 0)
			return val;

		link = val & (BIT(YT8521_LINK_STATUS_BIT));
		if (link) {
			link_utp = 1;
			yt8521_adjust_status(phydev, val, 1);
		} else {
			link_utp = 0;
		}
	}

	if (YT8521_PHY_MODE_CURR != YT8521_PHY_MODE_UTP) {
		/* reading Fiber */
		ret = ytphy_write_ext(phydev, 0xa000, 2);
		if (ret < 0)
			return ret;

		val = phy_read(phydev, REG_PHY_SPEC_STATUS);
		if (val < 0)
			return val;

		/* note: below debug information is used to check multiple PHy ports. */

		/* for fiber, from 1000m to 100m, there is not link down from 0x11,
		 * and check reg 1 to identify such case this is important for Linux
		 * kernel for that, missing linkdown event will cause problem.
		 */
		yt8521_fiber_latch_val = phy_read(phydev, MII_BMSR);
		yt8521_fiber_curr_val = phy_read(phydev, MII_BMSR);
		link = val & (BIT(YT8521_LINK_STATUS_BIT));
		if (link && yt8521_fiber_latch_val != yt8521_fiber_curr_val) {
			link = 0;
			netdev_info(phydev->attached_dev, "%s, phy addr: %d, fiber link down detect, latch = %04x, curr = %04x\n",
				    __func__, phydev->mdio.addr, yt8521_fiber_latch_val,
				    yt8521_fiber_curr_val);
		}

		if (link) {
			link_fiber = 1;
			yt8521_adjust_status(phydev, val, 0);
		} else {
			link_fiber = 0;
		}
	}

	if (link_utp || link_fiber) {
		if (phydev->link == 0)
			netdev_info(phydev->attached_dev, "%s, phy addr: %d, link up, media: %s, mii reg 0x11 = 0x%x\n",
				    __func__, phydev->mdio.addr,
				    (link_utp && link_fiber) ? "UNKNOWN MEDIA" : (link_utp ? "UTP" : "Fiber"),
				    (unsigned int)val);
		phydev->link = 1;
	} else {
		if (phydev->link == 1)
			netdev_info(phydev->attached_dev, "%s, phy addr: %d, link down\n",
				    __func__, phydev->mdio.addr);
		phydev->link = 0;
	}

	/* utp or combo */
	if (YT8521_PHY_MODE_CURR != YT8521_PHY_MODE_FIBER) {
		if (link_fiber)
			ytphy_write_ext(phydev, 0xa000, 2);
		if (link_utp)
			ytphy_write_ext(phydev, 0xa000, 0);
	}

	return 0;
}

static int yt8521_suspend(struct phy_device *phydev)
{
#if !(SYS_WAKEUP_BASED_ON_ETH_PKT)
	int value;

	ytphy_write_ext(phydev, 0xa000, 0);
	value = phy_read(phydev, MII_BMCR);
	phy_write(phydev, MII_BMCR, value | BMCR_PDOWN);

	ytphy_write_ext(phydev, 0xa000, 2);
	value = phy_read(phydev, MII_BMCR);
	phy_write(phydev, MII_BMCR, value | BMCR_PDOWN);

	ytphy_write_ext(phydev, 0xa000, 0);
#endif /*!(SYS_WAKEUP_BASED_ON_ETH_PKT)*/

	return 0;
}

static int yt8521_resume(struct phy_device *phydev)
{
	int value, ret;

	/* disable auto sleep */
	value = ytphy_read_ext(phydev, YT8521_EXTREG_SLEEP_CONTROL1);
	if (value < 0)
		return value;

	value &= (~BIT(YT8521_EN_SLEEP_SW_BIT));

	ret = ytphy_write_ext(phydev, YT8521_EXTREG_SLEEP_CONTROL1, value);
	if (ret < 0)
		return ret;

#if !(SYS_WAKEUP_BASED_ON_ETH_PKT)
	if (YT8521_PHY_MODE_CURR != YT8521_PHY_MODE_FIBER) {
		ytphy_write_ext(phydev, 0xa000, 0);
		value = phy_read(phydev, MII_BMCR);
		phy_write(phydev, MII_BMCR, value & ~BMCR_PDOWN);
	}

	if (YT8521_PHY_MODE_CURR != YT8521_PHY_MODE_UTP) {
		ytphy_write_ext(phydev, 0xa000, 2);
		value = phy_read(phydev, MII_BMCR);
		phy_write(phydev, MII_BMCR, value & ~BMCR_PDOWN);

		ytphy_write_ext(phydev, 0xa000, 0);
	}
#endif /*!(SYS_WAKEUP_BASED_ON_ETH_PKT)*/

	return 0;
}

static int yt8522_read_status(struct phy_device *phydev)
{
	int speed, speed_mode, duplex, val;

	genphy_read_status(phydev);
	val = phy_read(phydev, REG_PHY_SPEC_STATUS);
	if (val < 0)
		return val;

	/* link up */
	if ((val & BIT(10)) >> YTXXXX_LINK_STATUS_BIT) {
		duplex = (val & BIT(13)) >> YTXXXX_DUPLEX_BIT;
		speed_mode = (val & (BIT(15) | BIT(14))) >> YTXXXX_SPEED_MODE_BIT;
		switch (speed_mode) {
		case 0:
			speed = SPEED_10;
			break;
		case 1:
			speed = SPEED_100;
			break;
		case 2:
		case 3:
		default:
			speed = SPEED_UNKNOWN;
			break;
		}

		phydev->link = 1;
		phydev->speed = speed;
		phydev->duplex = duplex;

		return 0;
	}

	phydev->link = 0;

	return 0;
}

static int yt8522_probe(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	struct yt8xxx_priv *priv;
	int chip_config;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	phydev->priv = priv;

	chip_config = ytphy_read_ext(phydev, YT8522_EXTENDED_COMBO_CTRL_1);

	priv->chip_mode = ((chip_config & BIT(3)) >> 3);

	return 0;
}

static int yt8522_config_init(struct phy_device *phydev)
{
	struct yt8xxx_priv *priv = phydev->priv;
	int ret;
	int val;

	/* UTP */
	if (!priv->chip_mode) {
		val = ytphy_write_ext(phydev, YT8522_TX_CLK_DELAY, 0);
		if (val < 0)
			return val;

		val = ytphy_write_ext(phydev, YT8522_ANAGLOG_IF_CTRL, 0xbf2a);
		if (val < 0)
			return val;

		val = ytphy_write_ext(phydev, YT8522_DAC_CTRL, 0x297f);
		if (val < 0)
			return val;

		val = ytphy_write_ext(phydev, YT8522_INTERPOLATOR_FILTER_1, 0x1FE);
		if (val < 0)
			return val;

		val = ytphy_write_ext(phydev, YT8522_INTERPOLATOR_FILTER_2, 0x1FE);
		if (val < 0)
			return val;

		/* disable auto sleep */
		val = ytphy_read_ext(phydev, YT8512_EXTREG_SLEEP_CONTROL1);
		if (val < 0)
			return val;

		val &= (~BIT(YT8512_EN_SLEEP_SW_BIT));

		ret = ytphy_write_ext(phydev, YT8512_EXTREG_SLEEP_CONTROL1, val);
		if (ret < 0)
			return ret;

		ytphy_soft_reset(phydev);
	}

	return 0;
}

static int yt8531_rxclk_duty_init(struct phy_device *phydev)
{
	unsigned int value = 0x9696;
	int ret = 0;

	ret = ytphy_write_ext(phydev, 0xa040, 0xffff);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xa041, 0xff);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xa039, 0xbf00);
	if (ret < 0)
		return ret;

	/* nodelay duty = 0x9696 (default)
	 * fixed delay duty = 0x4040
	 * step delay 0xf duty = 0x4041
	 */
	if (phydev->interface == PHY_INTERFACE_MODE_RGMII_ID ||
	    phydev->interface == PHY_INTERFACE_MODE_RGMII_RXID)
		value = 0x4040;

	ret = ytphy_write_ext(phydev, 0xa03a, value);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext(phydev, 0xa03b, value);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext(phydev, 0xa03c, value);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext(phydev, 0xa03d, value);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext(phydev, 0xa03e, value);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext(phydev, 0xa03f, value);
	if (ret < 0)
		return ret;

	return ret;
}

static int yt8531S_config_init(struct phy_device *phydev)
{
#if (YTPHY8531A_XTAL_INIT)
	int ret = 0;

	ret = yt8531a_xtal_init(phydev);
	if (ret < 0)
		return ret;
#endif

	return yt8521_config_init(phydev);
}

static int yt8531_config_init(struct phy_device *phydev)
{
	int ret = 0, val;

#if (YTPHY8531A_XTAL_INIT)
	ret = yt8531a_xtal_init(phydev);
	if (ret < 0)
		return ret;
#endif

	/* PHY_CLK_OUT 125M enabled (default) */
	ret = ytphy_write_ext(phydev, 0xa012, 0xd0);
	if (ret < 0)
		return ret;

	ret = yt8531_rxclk_duty_init(phydev);
	if (ret < 0)
		return ret;

	/* RXC, PHY_CLK_OUT and RXData Drive strength:
	 * Drive strength of RXC = 6, PHY_CLK_OUT = 3, RXD0 = 4 (default 1.8v)
	 * If the io voltage is 3.3v, PHY_CLK_OUT = 2, set 0xa010 = 0xdacf
	 */
	ret = ytphy_write_ext(phydev, 0xa010, 0xdbcf);
	if (ret < 0)
		return ret;

	/* Change 100M default BGS voltage from 0x294c to 0x274c */
	val = ytphy_read_ext(phydev, 0x57);
	val = (val & ~(0xf << 8)) | (7 << 8);
	ret = ytphy_write_ext(phydev, 0x57, val);
	if (ret < 0)
		return ret;

	return ret;
}

/*
 * stmmac hard-resets the PHY on every resume (stmmac_mdio_reset() in
 * stmmac_resume()) and, because it uses MAC-managed PM, phylib never
 * calls phy_init_hw() again: the generic resume only clears BMCR_PDOWN and
 * the extended registers stay at their power-on defaults (measured on an
 * Orange Pi 5B: 0xa010 = 0x6bff, 0xa012 = 0xc8 after every rtcwake -m mem
 * instead of what config_init wrote).  Redo the init before resuming.
 */
static int yt8531_resume(struct phy_device *phydev)
{
	int ret;

	ret = yt8531_config_init(phydev);
	if (ret < 0)
		return ret;

	return genphy_resume(phydev);
}

static struct phy_driver motorcomm_phy_drvs[] = {
	{
		PHY_ID_MATCH_EXACT(PHY_ID_YT8011),
		.name		= "YT8011 Automotive Gigabit Ethernet",
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_POLL,
		.probe		= yt8011_probe,
		.config_aneg	= yt8011_config_aneg,
		.aneg_done	= yt8011_aneg_done,
		.config_init	= yt8011_config_init,
		.read_status	= yt8011_read_status,
		.soft_reset	= yt8011_soft_reset,
	}, {
		PHY_ID_MATCH_EXACT(PHY_ID_YT8511),
		.name		= "YT8511 Gigabit Ethernet",
		.config_init	= yt8511_config_init,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
		.read_page	= yt8511_read_page,
		.write_page	= yt8511_write_page,
	}, {
		PHY_ID_MATCH_EXACT(PHY_ID_YT8512),
		.name		= "YT8512 Ethernet",
		.config_init	= yt8512_config_init,
		.read_status	= yt8512_read_status,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
	}, {
		PHY_ID_MATCH_EXACT(PHY_ID_YT8512B),
		.name		= "YT8512B Ethernet",
		.config_init	= yt8512_config_init,
		.read_status	= yt8512_read_status,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
	}, {
		PHY_ID_MATCH_EXACT(PHY_ID_YT8522),
		.name           = "YT8522 100M Ethernet",
		.features       = PHY_BASIC_FEATURES,
		.probe          = yt8522_probe,
		.soft_reset     = ytphy_soft_reset,
		.config_aneg    = genphy_config_aneg,
		.config_init    = yt8522_config_init,
		.read_status    = yt8522_read_status,
		.suspend        = genphy_suspend,
		.resume         = genphy_resume,
	},  {
		/* same as 8521 */
		PHY_ID_MATCH_EXACT(PHY_ID_YT8531S),
		.name          = "YT8531S Gigabit Ethernet",
		.features      = PHY_GBIT_FEATURES,
		.soft_reset    = yt8521_soft_reset,
		.aneg_done     = yt8521_aneg_done,
		.config_init   = yt8531S_config_init,
		.read_status   = yt8521_read_status,
		.suspend       = yt8521_suspend,
		.resume        = yt8521_resume,
#if (YTPHY_WOL_FEATURE_ENABLE)
		.get_wol       = &ytphy_wol_feature_get,
		.set_wol       = &ytphy_wol_feature_set,
#endif
	}, {
		/* same as 8511 */
		PHY_ID_MATCH_EXACT(PHY_ID_YT8531),
		.name          = "YT8531 Gigabit Ethernet",
		.features      = PHY_GBIT_FEATURES,
		.config_init   = yt8531_config_init,
		.suspend       = genphy_suspend,
		.resume        = yt8531_resume,
#if (YTPHY_WOL_FEATURE_ENABLE)
		.get_wol       = &ytphy_wol_feature_get,
		.set_wol       = &ytphy_wol_feature_set,
#endif
	},
};

module_phy_driver(motorcomm_phy_drvs);

MODULE_DESCRIPTION("Motorcomm PHY driver");
MODULE_AUTHOR("Peter Geis");
MODULE_LICENSE("GPL");

static const struct mdio_device_id __maybe_unused motorcomm_tbl[] = {
	{ PHY_ID_MATCH_EXACT(PHY_ID_YT8511) },
	{ PHY_ID_MATCH_EXACT(PHY_ID_YT8512) },
	{ PHY_ID_MATCH_EXACT(PHY_ID_YT8512B) },
	{ PHY_ID_MATCH_EXACT(PHY_ID_YT8522) },
	{ PHY_ID_MATCH_EXACT(PHY_ID_YT8531S) },
	{ PHY_ID_MATCH_EXACT(PHY_ID_YT8531) },
	{ /* sentinal */ }
};

MODULE_DEVICE_TABLE(mdio, motorcomm_tbl);
