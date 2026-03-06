/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Rockchip Electronics Co., Ltd.
 */

#ifndef _PHY_HDMI_H_
#define _PHY_HDMI_H_

#include <linux/types.h>

#define PHY_MODE_HDMI	20

enum phy_hdmi_mode {
	PHY_HDMI_MODE_TMDS,
	PHY_HDMI_MODE_FRL,
};

/**
 * struct phy_configure_opts_hdmi - HDMI configuration set
 * @bpc: Bits per color channel.
 * @tmds_char_rate: HDMI TMDS Character Rate in Hertz.
 * @frl.rate_per_lane: HDMI FRL Rate per Lane in Gbps.
 * @frl.lanes: HDMI FRL lanes count.
 *
 * This structure is used to represent the configuration state of a HDMI phy.
 */
struct phy_configure_opts_hdmi {
	unsigned int bpc;
	union {
		unsigned long long tmds_char_rate;
		struct {
			u8 rate_per_lane;
			u8 lanes;
		} frl;
	};
};

#endif /* _PHY_HDMI_H_ */
