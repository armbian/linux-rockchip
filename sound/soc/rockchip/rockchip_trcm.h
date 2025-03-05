/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Rockchip TRCM Pcm driver
 *
 * Copyright (C) 2023 Rockchip Electronics Co., Ltd.
 * Author: Sugar Zhang <sugar.zhang@rock-chips.com>
 *
 */

#ifndef _ROCKCHIP_TRCM_H
#define _ROCKCHIP_TRCM_H

#define SND_DMAENGINE_TRCM_DRV_NAME	"snd_dmaengine_trcm"

#if IS_REACHABLE(CONFIG_SND_SOC_ROCKCHIP_TRCM)
int dmaengine_trcm_dma_guard_ctrl(struct snd_soc_component *component,
				  int stream, bool en);
int devm_snd_dmaengine_trcm_register(struct device *dev);
#else
static inline int dmaengine_trcm_dma_guard_ctrl(struct snd_soc_component *component,
						int stream, bool en)
{
	return -ENOSYS;
}

static inline int devm_snd_dmaengine_trcm_register(struct device *dev)
{
	return -ENOSYS;
}
#endif

#endif
