/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Rockchip Samsung mipi dcphy driver
 *
 * Copyright (C) 2020 Rockchip Electronics Co., Ltd.
 */

#ifndef _PHY_ROCKCHIP_SAMSUNG_DCPHY_H_
#define _PHY_ROCKCHIP_SAMSUNG_DCPHY_H_

#define MAX_NUM_CSI2_DPHY	(0x2)

enum hs_drv_res_ohm {
	_30_OHM = 0x8,
	_31_2_OHM,
	_32_5_OHM,
	_34_OHM,
	_35_5_OHM,
	_37_OHM,
	_39_OHM,
	_41_OHM,
	_43_OHM = 0x0,
	_46_OHM,
	_49_OHM,
	_52_OHM,
	_56_OHM,
	_60_OHM,
	_66_OHM,
	_73_OHM,
};

struct hs_drv_res_cfg {
	enum hs_drv_res_ohm clk_hs_drv_up_ohm;
	enum hs_drv_res_ohm clk_hs_drv_down_ohm;
	enum hs_drv_res_ohm data_hs_drv_up_ohm;
	enum hs_drv_res_ohm data_hs_drv_down_ohm;
};

struct samsung_mipi_dcphy_plat_data {
	const struct hs_drv_res_cfg *dphy_hs_drv_res_cfg;
	u32 dphy_tx_max_kbps_per_lane;
	u32 cphy_tx_max_ksps_per_lane;
};

struct samsung_mipi_dcphy {
	struct device *dev;
	struct clk *ref_clk;
	struct clk *pclk;
	struct regmap *regmap;
	struct regmap *grf_regmap;
	struct reset_control *m_phy_rst;
	struct reset_control *s_phy_rst;
	struct reset_control *apb_rst;
	struct reset_control *grf_apb_rst;
	struct mutex mutex;
	struct csi2_dphy *dphy_dev[MAX_NUM_CSI2_DPHY];
	atomic_t stream_cnt;
	int dphy_dev_num;
	bool c_option;

	unsigned int lanes;

	const struct samsung_mipi_dcphy_plat_data *pdata;
	struct {
		unsigned long long rate;
		u8 prediv;
		u16 fbdiv;
		u16 dsm;
		u8 scaler;

		bool ssc_en;
		u8 mfr;
		u8 mrr;
	} pll;

	int (*stream_on)(struct csi2_dphy *dphy, struct v4l2_subdev *sd);
	int (*stream_off)(struct csi2_dphy *dphy, struct v4l2_subdev *sd);
	struct resource *res;
};

#endif
