/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Rockchip Vehicle driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */
#ifndef __VEHICLE_AD_TP2855_H__
#define __VEHICLE_AD_TP2855_H__

int tp2855_ad_init(struct vehicle_ad_dev *ad);
int tp2855_ad_deinit(void);
int tp2855_ad_get_cfg(struct vehicle_cfg **cfg);
void tp2855_ad_check_cif_error(struct vehicle_ad_dev *ad, int last_line);
int tp2855_check_id(struct vehicle_ad_dev *ad);
int tp2855_stream(struct vehicle_ad_dev *ad, int enable);
void tp2855_channel_set(struct vehicle_ad_dev *ad, int channel);

#endif
