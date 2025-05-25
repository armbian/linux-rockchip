/* SPDX-License-Identifier: GPL-2.0 */

/* Copyright (c) 2022 Rockchip Electronics Co., Ltd. */

#ifndef __RKCE_PKA_H__
#define __RKCE_PKA_H__

#include "rkce_bignum.h"

void rkce_pka_set_crypto_base(void __iomem *base);

int rkce_pka_expt_mod(struct rkce_bignum *in,
		      struct rkce_bignum *e,
		      struct rkce_bignum *n,
		      struct rkce_bignum *out);

#endif
