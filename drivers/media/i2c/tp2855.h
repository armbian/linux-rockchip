/* SPDX-License-Identifier: GPL-2.0 */
/*
 * tp2855 driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 *
 */
#ifndef _TP2855_H_
#define _TP2855_H_

enum tp2855_support_reso {
	TP2855_CVSTD_720P_60 = 0,
	TP2855_CVSTD_720P_50,
	TP2855_CVSTD_1080P_30,
	TP2855_CVSTD_1080P_25,
	TP2855_CVSTD_720P_30,
	TP2855_CVSTD_720P_25,
	TP2855_CVSTD_SD,
	TP2855_CVSTD_OTHER,
};

int tp2855_sensor_mod_init(void);

#endif
