// SPDX-License-Identifier: GPL-2.0
/*
 * bignum support for Rockchip crypto
 *
 * Copyright (c) 2020 Rockchip Electronics Co., Ltd.
 *
 * Author: Lin Jinhan <troy.lin@rock-chips.com>
 *
 */
#include <linux/slab.h>

#include "rk_crypto_bignum.h"

#define DEFAULT_ENDIAN		RK_BG_LITTILE_ENDIAN

#define BYTES2WORDS(bytes)	(round_up((bytes), sizeof(u32)) / sizeof(u32))
#define WORDS2BYTES(words)	((words) * sizeof(u32))
#define RK_WORD_SIZE		32

static void rk_reverse_memcpy(void *dst, const void *src, u32 size)
{
	char *_dst = (char *)dst, *_src = (char *)src;
	u32 i;

	if (!dst || !src || !size)
		return;

	for (i = 0; i < size; ++i)
		_dst[size - i - 1] = _src[i];
}

struct rk_bignum *rk_bn_alloc(u32 max_size)
{
	struct rk_bignum *bn;

	bn = kzalloc(sizeof(*bn), GFP_KERNEL);
	if (!bn)
		return NULL;

	bn->data = kzalloc(round_up(max_size, sizeof(u32)), GFP_KERNEL);
	if (!bn->data) {
		kfree(bn);
		return NULL;
	}

	bn->n_words = BYTES2WORDS(max_size);

	return bn;
}

void rk_bn_free(struct rk_bignum *bn)
{
	if (!bn)
		return;

	if (bn->data) {
		memset(bn->data, 0x00, WORDS2BYTES(bn->n_words));
		kfree(bn->data);
	}

	kfree(bn);
}

int rk_bn_set_data(struct rk_bignum *bn, const u8 *data, u32 size, enum bignum_endian endian)
{
	if (!bn || !data)
		return -EINVAL;

	if (BYTES2WORDS(size) > bn->n_words)
		return -EINVAL;

	if (endian == DEFAULT_ENDIAN)
		memcpy(bn->data, data, size);
	else
		rk_reverse_memcpy(bn->data, data, size);

	return 0;
}

int rk_bn_get_data(const struct rk_bignum *bn, u8 *data, u32 size, enum bignum_endian endian)
{
	if (!bn || !data)
		return -EINVAL;

	if (size < WORDS2BYTES(bn->n_words))
		return -EINVAL;

	memset(data, 0x00, size);

	if (endian == DEFAULT_ENDIAN)
		memcpy(data + size - WORDS2BYTES(bn->n_words), bn->data, bn->n_words);
	else
		rk_reverse_memcpy(data + size - WORDS2BYTES(bn->n_words),
				  bn->data, WORDS2BYTES(bn->n_words));

	return 0;
}

u32 rk_bn_get_size(const struct rk_bignum *bn)
{
	if (!bn)
		return 0;

	return WORDS2BYTES(bn->n_words);
}

/*
 * @brief  Returns the index of the highest 1 in |bn|.
 * @param  bn: the point of input data bignum.
 * @return The index starts at 0 for the least significant bit.
 *         If src == zero, it will return -1
 */
int rk_bn_highest_bit(const struct rk_bignum *bn)
{
	u32 w;
	u32 b;

	if (!bn || !bn->data || !bn->n_words)
		return -1;

	w = bn->data[bn->n_words - 1];

	for (b = 0; b < RK_WORD_SIZE; b++) {
		w >>= 1;
		if (w == 0)
			break;
	}

	return (int)(bn->n_words - 1) * RK_WORD_SIZE + b;
}

void rk_ecc_free_point(struct rk_ecp_point *point)
{
	if (!point)
		return;

	rk_bn_free(point->x);
	rk_bn_free(point->y);

	kfree(point);
}

struct rk_ecp_point *rk_ecc_alloc_point_zero(u32 max_size)
{
	struct rk_ecp_point *point = NULL;

	point = kzalloc(sizeof(*point), GFP_KERNEL);
	if (!point)
		return NULL;

	point->x = rk_bn_alloc(max_size);
	if (!point->x)
		goto error;

	point->y = rk_bn_alloc(max_size);
	if (!point->y)
		goto error;

	return point;

error:
	rk_ecc_free_point(point);

	return NULL;
}

struct rk_ecp_point *rk_ecc_alloc_point(const uint8_t *x, uint32_t x_len,
					const uint8_t *y, uint32_t y_len,
					enum bignum_endian endian, u32 max_size)
{
	struct rk_ecp_point *point = NULL;

	point = kzalloc(sizeof(*point), GFP_KERNEL);
	if (!point)
		return NULL;

	point->x = rk_bn_alloc(max_size);
	if (!point->x)
		goto error;

	if (rk_bn_set_data(point->x, x, x_len, endian) != 0)
		goto error;

	point->y = rk_bn_alloc(max_size);
	if (!point->y)
		goto error;

	if (rk_bn_set_data(point->y, y, y_len, endian) != 0)
		goto error;

	return point;

error:
	rk_ecc_free_point(point);

	return NULL;
}

bool rk_ecp_point_is_zero(struct rk_ecp_point *point)
{
	uint32_t i;
	bool ret = true;

	if (!point || !point->x || !point->y)
		return false;

	for (i = 0; i < point->x->n_words; i++) {
		if (point->x->data[i] != 0)
			ret = false;
	}

	for (i = 0; i < point->y->n_words; i++) {
		if (point->y->data[i] != 0)
			ret = false;
	}

	return ret;
}
