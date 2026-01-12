/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 *
 * Author: Algea Cao <algea.cao@rock-chips.com>
 */

#ifndef __DW_HDCP_NOTIFY_H__
#define __DW_HDCP_NOTIFY_H__

#include <linux/notifier.h>

#if IS_REACHABLE(CONFIG_ROCKCHIP_DW_HDCP2)
int dw_hdcp_register_notifier(struct notifier_block *nb);
int dw_hdcp_unregister_notifier(struct notifier_block *nb);
#else
int dw_hdcp_register_notifier(struct notifier_block *nb)
{
	return 0;
}

int dw_hdcp_unregister_notifier(struct notifier_block *nb)
{
	return 0;
}
#endif

#endif
