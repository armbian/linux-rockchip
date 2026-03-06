/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Rockchip Vehicle driver
 *
 * Copyright (C) 2026 Rockchip Electronics Co., Ltd.
 */
#ifndef __VEHICLE_AD_TP2860_H__
#define __VEHICLE_AD_TP2860_H__

int tp2860_ad_init(struct vehicle_ad_dev *ad);
int tp2860_ad_deinit(void);
int tp2860_ad_get_cfg(struct vehicle_cfg **cfg);
void tp2860_ad_check_cif_error(struct vehicle_ad_dev *ad, int last_line);
int tp2860_check_id(struct vehicle_ad_dev *ad);
int tp2860_stream(struct vehicle_ad_dev *ad, int enable);
void tp2860_channel_set(struct vehicle_ad_dev *ad, int channel);
int tp2860_configure_regulators(struct vehicle_ad_dev *ad, struct device_node *cp);

#endif
