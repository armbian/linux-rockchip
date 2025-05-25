/* SPDX-License-Identifier: GPL-2.0 */

/* Copyright (c) 2024 Rockchip Electronics Co., Ltd. */

#ifndef __RKCE_AKCIPHER_H__
#define __RKCE_AKCIPHER_H__

#include <linux/types.h>

#include "rkce_dev.h"

extern struct rkce_algt rkce_asym_rsa;
extern struct rkce_algt rkce_asym_ecc_p192;
extern struct rkce_algt rkce_asym_ecc_p224;
extern struct rkce_algt rkce_asym_ecc_p256;
extern struct rkce_algt rkce_asym_sm2;

#endif
