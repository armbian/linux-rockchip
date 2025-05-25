/* SPDX-License-Identifier: GPL-2.0 */

/* Copyright (c) 2024 Rockchip Electronics Co., Ltd. */

#ifndef __RKCE_HASH_H__
#define __RKCE_HASH_H__

#include <linux/types.h>

#include "rkce_dev.h"

extern struct rkce_algt rkce_ahash_sha1;
extern struct rkce_algt rkce_ahash_sha224;
extern struct rkce_algt rkce_ahash_sha256;
extern struct rkce_algt rkce_ahash_sha384;
extern struct rkce_algt rkce_ahash_sha512;
extern struct rkce_algt rkce_ahash_md5;
extern struct rkce_algt rkce_ahash_sm3;

extern struct rkce_algt rkce_hmac_md5;
extern struct rkce_algt rkce_hmac_sha1;
extern struct rkce_algt rkce_hmac_sha256;
extern struct rkce_algt rkce_hmac_sha512;
extern struct rkce_algt rkce_hmac_sm3;

int rkce_hash_request_callback(int result, uint32_t td_id, void *td_addr);

#endif
