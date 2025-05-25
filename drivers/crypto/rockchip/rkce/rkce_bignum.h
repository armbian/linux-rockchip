/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2020 Rockchip Electronics Co., Ltd. */

#ifndef __RKCE_BIGNUM_H__
#define __RKCE_BIGNUM_H__

enum bignum_endian {
	RK_BG_BIG_ENDIAN,
	RK_BG_LITTILE_ENDIAN
};

/**
 * struct rkce_bignum - crypto bignum struct.
 */
struct rkce_bignum {
	u32 n_words;
	u32 *data;
};

struct rkce_ecp_point {
	struct rkce_bignum *x;     /*!<  the point's X coordinate  */
	struct rkce_bignum *y;     /*!<  the point's Y coordinate  */
};

struct rkce_bignum *rkce_bn_alloc(u32 max_size);
void rkce_bn_free(struct rkce_bignum *bn);
int rkce_bn_set_data(struct rkce_bignum *bn, const u8 *data, u32 size, enum bignum_endian endian);
int rkce_bn_get_data(const struct rkce_bignum *bn, u8 *data, u32 size, enum bignum_endian endian);
u32 rkce_bn_get_size(const struct rkce_bignum *bn);
int rkce_bn_highest_bit(const struct rkce_bignum *src);

struct rkce_ecp_point *rkce_ecc_alloc_point_zero(u32 max_size);
struct rkce_ecp_point *rkce_ecc_alloc_point(const uint8_t *x, uint32_t x_len,
					    const uint8_t *y, uint32_t y_len,
					    enum bignum_endian endian, u32 max_size);
void rkce_ecc_free_point(struct rkce_ecp_point *point);
bool rkce_ecp_point_is_zero(struct rkce_ecp_point *point);

#endif
