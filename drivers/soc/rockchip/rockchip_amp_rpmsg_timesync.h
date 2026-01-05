/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Rockchip AMP RPMSG timesync.
 *
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 * Author: Zain Wang <zain.wang@rock-chips.com>
 */

#ifndef _ROCKCHIP_AMP_RPMSG_TIMESYNC_
#define _ROCKCHIP_AMP_RPMSG_TIMESYNC_

#define RK_TIMER_VERSION		(0)

#define RK_TIMESYNC_OP_START		(1)
#define RK_TIMESYNC_OP_TIMEINFO		(2)
#define RK_TIMESYNC_OP_CYCLE_BASE	(3)

struct rk_timer_info {
	uint32_t addr;
	uint32_t version;
	uint32_t clk_rate;
	/* Unit MHz */
} __packed;

struct rkamp_timesync_msg {
	uint32_t op_flag;
	union {
		struct rk_timer_info timer_info;
		int64_t cycle_base;
	};
} __packed;

#endif
