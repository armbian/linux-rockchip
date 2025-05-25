/* SPDX-License-Identifier: GPL-2.0 */

/* Copyright (c) 2024 Rockchip Electronics Co., Ltd. */

#ifndef __RKCE_SCKIPHER_H__
#define __RKCE_SCKIPHER_H__

#include <linux/types.h>

#include "rkce_dev.h"

extern struct rkce_algt rkce_ecb_sm4_alg;
extern struct rkce_algt rkce_cbc_sm4_alg;
extern struct rkce_algt rkce_xts_sm4_alg;
extern struct rkce_algt rkce_cfb_sm4_alg;
extern struct rkce_algt rkce_ofb_sm4_alg;
extern struct rkce_algt rkce_ctr_sm4_alg;
extern struct rkce_algt rkce_gcm_sm4_alg;

extern struct rkce_algt rkce_ecb_aes_alg;
extern struct rkce_algt rkce_cbc_aes_alg;
extern struct rkce_algt rkce_xts_aes_alg;
extern struct rkce_algt rkce_cfb_aes_alg;
extern struct rkce_algt rkce_ofb_aes_alg;
extern struct rkce_algt rkce_ctr_aes_alg;
extern struct rkce_algt rkce_gcm_aes_alg;

extern struct rkce_algt rkce_ecb_des_alg;
extern struct rkce_algt rkce_cbc_des_alg;
extern struct rkce_algt rkce_cfb_des_alg;
extern struct rkce_algt rkce_ofb_des_alg;

extern struct rkce_algt rkce_ecb_des3_ede_alg;
extern struct rkce_algt rkce_cbc_des3_ede_alg;
extern struct rkce_algt rkce_cfb_des3_ede_alg;
extern struct rkce_algt rkce_ofb_des3_ede_alg;

int rkce_cipher_request_callback(int result, uint32_t td_id, void *td_addr);

#endif
