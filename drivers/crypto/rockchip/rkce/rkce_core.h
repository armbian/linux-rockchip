/* SPDX-License-Identifier: GPL-2.0 */

/* Copyright (c) 2024 Rockchip Electronics Co., Ltd. */

#ifndef __RKCE_CORE_H__
#define __RKCE_CORE_H__

#include <linux/bitops.h>
#include <linux/types.h>

#include "rkce_buf.h"
#include "rkce_error.h"
#include "rkce_reg.h"

#define RKCE_TD_SG_NUM			8

#define RKCE_AES_BLOCK_SIZE		16
#define RKCE_AES_KEYSIZE_128		16
#define RKCE_AES_KEYSIZE_192		24
#define RKCE_AES_KEYSIZE_256		32

#define RKCE_SM4_KEYSIZE		16

#define RKCE_DES_BLOCK_SIZE		8
#define RKCE_DES_KEYSIZE		8
#define RKCE_TDES_EDE_KEYSIZE		24

#define RKCE_TD_ALIGINMENT		16
#define RKCE_TD_KEY_SIZE		128
#define RKCE_TD_IV_SIZE			16
#define RKCE_TD_GCM_LEN_SIZE		16
#define RKCE_TD_HASH_CTX_SIZE		RKCE_HASH_CONTEXT_SIZE
#define RKCE_TD_SYMM_CTX_SIZE		RKCE_SYMM_CONTEXT_SIZE
#define RKCE_TD_TAG_SIZE		16
#define RKCE_TD_TAG_SIZE_MIN		8
#define RKCE_TD_TAG_SIZE_MAX		RKCE_TD_TAG_SIZE
#define RKCE_TD_HASH_SIZE		64
#define RKCE_TD_FIFO_DEPTH		8

#define RKCE_RESET_SYMM			BIT(0)
#define RKCE_RESET_HASH			BIT(1)
#define RKCE_RESET_PKA			BIT(2)

#define RKCE_WRITE_MASK_SHIFT		(16)
#define RKCE_WRITE_MASK_ALL		((0xffffu << RKCE_WRITE_MASK_SHIFT))

enum rkce_expand_bit {
	RKCE_EXPAND_BIT_4G = 0,
	RKCE_EXPAND_BIT_8G,
	RKCE_EXPAND_BIT_16G,
	RKCE_EXPAND_BIT_32G,
};

enum rkce_td_type {
	RKCE_TD_TYPE_SYMM = 0,
	RKCE_TD_TYPE_HASH,
	RKCE_TD_TYPE_SYMM_HASH_IN,
	RKCE_TD_TYPE_SYMM_HASH_OUT,
	RKCE_TD_TYPE_MAX,
};

enum rkce_algo_symm_type {
	RKCE_SYMM_ALGO_AES = 0,
	RKCE_SYMM_ALGO_SM4,
	RKCE_SYMM_ALGO_DES,
	RKCE_SYMM_ALGO_TDES,
	RKCE_SYMM_ALGO_MAX,
};

enum rkce_algo_symm_mode {
	RKCE_SYMM_MODE_ECB = 0,
	RKCE_SYMM_MODE_CBC,
	RKCE_SYMM_MODE_CTS,
	RKCE_SYMM_MODE_CTR,
	RKCE_SYMM_MODE_CFB,
	RKCE_SYMM_MODE_OFB,
	RKCE_SYMM_MODE_XTS,
	RKCE_SYMM_MODE_CCM,
	RKCE_SYMM_MODE_GCM,
	RKCE_SYMM_MODE_CMAC,
	RKCE_SYMM_MODE_CBC_MAC,
	RKCE_SYMM_MODE_BYPASS = 0xf,
	RKCE_SYMM_MODE_MAX,
};

enum {
	RKCE_KEY_AES_128 = 0,
	RKCE_KEY_AES_192,
	RKCE_KEY_AES_256,
};

enum rkce_algo_hash_type {
	RKCE_HASH_ALGO_SHA1 = 0,
	RKCE_HASH_ALGO_MD5,
	RKCE_HASH_ALGO_SHA256,
	RKCE_HASH_ALGO_SHA224,
	RKCE_HASH_ALGO_SM3 = 6,
	RKCE_HASH_ALGO_SHA512 = 8,
	RKCE_HASH_ALGO_SHA384 = 9,
	RKCE_HASH_ALGO_SHA512_224,
	RKCE_HASH_ALGO_SHA512_256,
	RKCE_HASH_ALGO_MAX,
};

enum rkce_algo_asym_type {
	RKCE_ASYM_ALGO_RSA = 0,
	RKCE_ASYM_ALGO_ECC_P192,
	RKCE_ASYM_ALGO_ECC_P224,
	RKCE_ASYM_ALGO_ECC_P256,
	RKCE_ASYM_ALGO_ECC_P384,
	RKCE_ASYM_ALGO_ECC_P521,
	RKCE_ASYM_ALGO_SM2,
	RKCE_ASYM_ALGO_MAX,
};

enum rkce_algo_type {
	RKCE_ALGO_TYPE_HASH,
	RKCE_ALGO_TYPE_HMAC,
	RKCE_ALGO_TYPE_CIPHER,
	RKCE_ALGO_TYPE_ASYM,
	RKCE_ALGO_TYPE_AEAD,
	RKCE_ALGO_TYPE_MAX,
};

struct rkce_ip_info {
	uint32_t	aes_ver;
	uint32_t	des_ver;
	uint32_t	sm4_ver;
	uint32_t	hash_ver;
	uint32_t	hmac_ver;
	uint32_t	pka_ver;
	uint32_t	extra_feature;
	uint32_t	ce_ver;
};

struct rkce_gcm_len {
	uint32_t	pc_len_l;
	uint32_t	pc_len_h;
	uint32_t	aad_len_l;
	uint32_t	aad_len_h;
};

struct rkce_sg_info {
	uint32_t	src_size;
	uint32_t	src_addr_h;
	uint32_t	src_addr_l;

	uint32_t	dst_size;
	uint32_t	dst_addr_h;
	uint32_t	dst_addr_l;
};

/* total = 64 + 16 + 16 + 16 + 32 = 114(Byte) */
struct rkce_symm_td_buf {
	uint8_t			key1[RKCE_AES_KEYSIZE_256];		// offset 0x00
	uint8_t			key2[RKCE_AES_KEYSIZE_256];		// offset 0x20
	uint8_t			iv[RKCE_TD_IV_SIZE];			// offset 0x40
	struct rkce_gcm_len	gcm_len;				// offset 0x50
	uint8_t			tag[RKCE_TD_TAG_SIZE];			// offset 0x60
	uint8_t			ctx[RKCE_SYMM_CONTEXT_SIZE];		// offset 0x70
	void			*user_data;
};

/* total = 128 + 64 + 208 = 360(Byte) */
struct rkce_hash_td_buf {
	uint8_t			key[RKCE_TD_KEY_SIZE];			// offset 0x00
	uint8_t			hash[RKCE_TD_HASH_SIZE];		// offset 0x80
	uint8_t			ctx[RKCE_HASH_CONTEXT_SIZE];		// offset 0xB0
	void			*user_data;
};

struct rkce_symm_hash_td_buf {
	uint8_t			key1[RKCE_AES_KEYSIZE_256];		// offset 0x00
	uint8_t			key2[RKCE_AES_KEYSIZE_256];		// offset 0x20
	uint8_t			key3[RKCE_AES_KEYSIZE_256 * 2];		// offset 0x40
	uint8_t			iv[RKCE_TD_IV_SIZE];			// offset 0x80
	struct rkce_gcm_len	gcm_len;				// offset 0x90
	uint8_t			tag[RKCE_TD_TAG_SIZE];			// offset 0xA0
	uint8_t			hash[RKCE_TD_HASH_SIZE];		// offset 0xB0
	uint8_t			symm_ctx[RKCE_SYMM_CONTEXT_SIZE];	// offset 0xF0
	uint8_t			hash_ctx[RKCE_HASH_CONTEXT_SIZE];	// offset 0x110
	void			*user_data;
};

struct rkce_symm_td_ctrl {
	uint32_t	td_type : 2;
	uint32_t	is_dec : 1;
	uint32_t	is_aad : 1;
	uint32_t	symm_algo : 2;
	uint32_t: 2;
	uint32_t	symm_mode : 4;
	uint32_t	key_size : 2;
	uint32_t	first_pkg : 1;
	uint32_t	last_pkg : 1;
	uint32_t	key_sel : 3;
	uint32_t	iv_len : 5;
	uint32_t: 4;
	uint32_t	is_key_inside : 1;
	uint32_t: 1;
	uint32_t	is_preemptible : 1;
	uint32_t	int_en : 1;
};

struct rkce_hash_td_ctrl {
	uint32_t	td_type : 2;
	uint32_t: 5;
	uint32_t	hw_pad_en : 1;
	uint32_t: 6;
	uint32_t	first_pkg : 1;
	uint32_t	last_pkg : 1;
	uint32_t: 8;
	uint32_t	hash_algo: 4;
	uint32_t: 1;
	uint32_t	hmac_en : 1;
	uint32_t	is_preemptible : 1;
	uint32_t	int_en : 1;
};

struct rkce_symm_hash_td_ctrl {
	uint32_t	td_type : 2;
	uint32_t	is_dec : 1;
	uint32_t	is_aad : 1;
	uint32_t	symm_algo : 2;
	uint32_t: 1;
	uint32_t	hw_pad_en : 1;
	uint32_t	symm_mode : 4;
	uint32_t	key_size : 2;
	uint32_t	first_pkg : 1;
	uint32_t	last_pkg : 1;
	uint32_t	key_sel : 3;
	uint32_t	iv_len : 5;
	uint32_t	hash_algo: 4;
	uint32_t	is_key_inside : 1;
	uint32_t	hmac_en : 1;
	uint32_t	is_preemptible : 1;
	uint32_t	int_en : 1;
};

struct rkce_symm_td {
	uint32_t			task_id;
	struct rkce_symm_td_ctrl	ctrl;
	uint32_t			reserve1;
	uint32_t			key_addr;

	uint32_t			iv_addr;
	uint32_t			gcm_len_addr;
	uint32_t			reserve2;
	uint32_t			tag_addr;

	struct rkce_sg_info		sg[RKCE_TD_SG_NUM];

	uint32_t			reserve3;
	uint32_t			symm_ctx_addr;
	uint32_t			reserve4[5];
	uint32_t			next_task;
};

struct rkce_hash_td {
	uint32_t			task_id;
	struct rkce_hash_td_ctrl	ctrl;
	uint32_t			reserve1;
	uint32_t			key_addr;

	uint32_t			reserve2[2];
	uint32_t			hash_addr;
	uint32_t			reserve3;

	struct rkce_sg_info		sg[RKCE_TD_SG_NUM];

	uint32_t			hash_ctx_addr;
	uint32_t			reserve4[6];
	uint32_t			next_task;
};

struct rkce_symm_hash_td {
	uint32_t			task_id;
	struct rkce_symm_hash_td_ctrl	ctrl;
	uint32_t			reserve1;
	uint32_t			key_addr;

	uint32_t			iv_addr;
	uint32_t			gcm_len_addr;
	uint32_t			hash_addr;
	uint32_t			tag_addr;

	struct rkce_sg_info		sg[RKCE_TD_SG_NUM];

	uint32_t			hash_ctx_addr;
	uint32_t			symm_ctx_addr;
	uint32_t			reserve3[5];
	uint32_t			next_task;
};

struct rkce_td {
	union {
		struct rkce_symm_td	 symm;
		struct rkce_hash_td	 hash;
		struct rkce_symm_hash_td symm_hash;
	} td;
};

struct rkce_td_buf {
	union {
		struct rkce_symm_td_buf	     symm;
		struct rkce_hash_td_buf	     hash;
		struct rkce_symm_hash_td_buf symm_hash;
	} td_buf;
};

typedef int (*request_cb_func)(int result, uint32_t td_id, void *td_virt);

void rkce_dump_reginfo(void *rkce_hw);

void *rkce_hardware_alloc(void __iomem *reg_base);

void rkce_hardware_free(void *rkce_hw);

void rkce_irq_handler(void *rkce_hw);

void rkce_irq_thread(void *rkce_hw);

int rkce_irq_callback_set(void *rkce_hw, enum rkce_td_type td_type, request_cb_func cb_func);

int rkce_soft_reset(void *rkce_hw, uint32_t reset_sel);

int rkce_push_td(void *rkce_hw, void *td);

int rkce_push_td_sync(void *rkce_hw, void *td, uint32_t timeout_ms);

uint32_t rkce_get_td_type(void *td_buf);

int rkce_init_symm_td(struct rkce_symm_td *td, struct rkce_symm_td_buf *buf);

int rkce_init_hash_td(struct rkce_hash_td *td, struct rkce_hash_td_buf *buf);

bool rkce_hw_algo_valid(void *rkce_hw, uint32_t type, uint32_t algo, uint32_t mode);

#endif
