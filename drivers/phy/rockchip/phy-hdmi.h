/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Rockchip Electronics Co., Ltd.
 */

#ifndef _PHY_HDMI_H_
#define _PHY_HDMI_H_

/**
 * struct phy_configure_opts_hdmi - HDMI configuration set
 * @tmds_char_rate: HDMI TMDS Character Rate in Hertz.
 *
 * This structure is used to represent the configuration state of a HDMI phy.
 */
struct phy_configure_opts_hdmi {
	unsigned long long tmds_char_rate;
};

#endif /* _PHY_HDMI_H_ */
