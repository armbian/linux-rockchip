/* SPDX-License-Identifier: GPL-2.0 */

/* Copyright (c) 2024 Rockchip Electronics Co., Ltd. */

#ifndef __RKCE_DEV_H__
#define __RKCE_DEV_H__

#include <crypto/aes.h>
#include <crypto/algapi.h>
#include <crypto/des.h>
#include <crypto/engine.h>
#include <crypto/gcm.h>
#include <crypto/md5.h>
#include <crypto/sha1.h>
#include <crypto/sha2.h>
#include <crypto/skcipher.h>
#include <crypto/sm3.h>
#include <crypto/sm4.h>
#include <crypto/internal/aead.h>
#include <crypto/internal/akcipher.h>
#include <crypto/internal/hash.h>
#include <crypto/internal/des.h>
#include <crypto/internal/rsa.h>
#include <crypto/internal/skcipher.h>

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/reset.h>

#include "rkce_core.h"

#define RKCE_PRIORITY			0

#define DES_MIN_KEY_SIZE		DES_KEY_SIZE
#define DES_MAX_KEY_SIZE		DES_KEY_SIZE
#define DES3_EDE_MIN_KEY_SIZE		DES3_EDE_KEY_SIZE
#define DES3_EDE_MAX_KEY_SIZE		DES3_EDE_KEY_SIZE
#define SM4_MIN_KEY_SIZE		SM4_KEY_SIZE
#define SM4_MAX_KEY_SIZE		SM4_KEY_SIZE

#define MD5_BLOCK_SIZE			SHA1_BLOCK_SIZE

#define sha384_state			sha512_state
#define sha224_state			sha256_state

#define RKCE_SYMM_ALGO_DES3_EDE		RKCE_SYMM_ALGO_TDES
struct rkce_dev {
	struct device			*dev;
	struct reset_control		*rst;
	void __iomem			*reg;
	int				irq;
	int clks_num;
	struct clk_bulk_data		*clk_bulks;

	/* device lock */
	spinlock_t			lock;
	struct crypto_engine		*symm_engine;
	struct crypto_engine		*hash_engine;

	void				*hardware;
};

struct rkce_cipher_ctx {
	struct crypto_engine_ctx	enginectx;
	struct rkce_algt		*algt;
	struct rkce_symm_td_buf		*td_buf;
	void				*req;

	uint8_t				iv[AES_BLOCK_SIZE];
	uint32_t			ivlen;
	uint32_t			keylen;
	uint32_t			authsize;
};

struct rkce_cipher_request_ctx {
	struct rkce_symm_td		*td_head;
	struct rkce_symm_td		*td_aad_head;

	struct device			*dev;
	struct scatterlist		*sgs;
	struct scatterlist		*sgd;
	uint32_t			sgs_nents;
	uint32_t			sgd_nents;
	uint32_t			cryptlen;

	struct scatterlist		src_sg[2];
	struct scatterlist		dst_sg[2];
	struct scatterlist		*sga;
	uint32_t			sga_nents;
	uint32_t			assoclen;

	uint32_t			map_total;
	uint8_t				is_enc;
	uint8_t				is_aead;
	uint8_t				is_mapped;
	uint8_t				is_dma;
};

/* the private variable of hash */
struct rkce_ahash_ctx {
	struct crypto_engine_ctx	enginectx;
	struct rkce_algt		*algt;
	struct rkce_hash_td		*key_td;
	struct rkce_hash_td_buf		*td_buf;
	struct ahash_request		*req;

	uint8_t				*user_key;
	uint32_t			calculated;
	uint8_t				is_hmac;
	uint8_t				is_final;
};

struct rkce_ahash_request_ctx {
	struct rkce_hash_td		*td_head;
	uint32_t			sgs_nents;
	uint8_t				is_mapped;
	uint8_t				*hw_context;
};

struct rkce_rsa_ctx {
	struct crypto_engine_ctx	enginectx;
	struct rkce_algt		*algt;

	struct rkce_bignum		*n;
	struct rkce_bignum		*e;
	struct rkce_bignum		*d;

	uint8_t				is_enc;
};

struct rkce_ecc_ctx {
	struct crypto_engine_ctx	enginectx;
	struct rkce_algt		*algt;

	uint32_t			group_id;
	uint32_t			nbits;
	bool				pub_key_set;
	struct rkce_ecp_point		*point_Q;

	uint8_t				is_sign;
};

struct rkce_asym_request_ctx {
	uint32_t			reserved;
};

struct rkce_algt {
	struct rkce_dev			*rk_dev;
	union {
		struct skcipher_alg	cipher;
		struct ahash_alg	hash;
		struct akcipher_alg	asym;
		struct aead_alg		aead;
	} alg;
	enum rkce_algo_type		type;
	uint32_t			algo;
	uint32_t			mode;
	char				*name;
	bool				valid_flag;
};

#define  RK_AEAD_ALGO_INIT(cipher_algo, cipher_mode, algo_name, driver_name) {\
	.name = #algo_name,\
	.type = RKCE_ALGO_TYPE_AEAD,\
	.algo = RKCE_SYMM_ALGO_##cipher_algo,\
	.mode = RKCE_SYMM_MODE_##cipher_mode,\
	.alg.aead = {\
		.base.cra_name		= #algo_name,\
		.base.cra_driver_name	= #driver_name,\
		.base.cra_priority	= RKCE_PRIORITY,\
		.base.cra_flags		= CRYPTO_ALG_TYPE_AEAD |\
					  CRYPTO_ALG_KERN_DRIVER_ONLY |\
					  CRYPTO_ALG_ASYNC |\
					  CRYPTO_ALG_NEED_FALLBACK |\
					  CRYPTO_ALG_INTERNAL,\
		.base.cra_blocksize	= 1,\
		.base.cra_ctxsize	= sizeof(struct rkce_cipher_ctx),\
		.base.cra_alignmask	= 0x07,\
		.base.cra_module	= THIS_MODULE,\
		.init		= rkce_aead_init_tfm,\
		.exit		= rkce_aead_exit_tfm,\
		.ivsize		= GCM_AES_IV_SIZE,\
		.chunksize      = cipher_algo##_BLOCK_SIZE,\
		.maxauthsize    = AES_BLOCK_SIZE,\
		.setkey		= rkce_aead_setkey,\
		.setauthsize	= rkce_aead_setauthsize,\
		.encrypt	= rkce_aead_encrypt,\
		.decrypt	= rkce_aead_decrypt,\
	} \
}

#define  RK_CIPHER_ALGO_INIT(cipher_algo, cipher_mode, algo_name, driver_name) {\
	.name = #algo_name,\
	.type = RKCE_ALGO_TYPE_CIPHER,\
	.algo = RKCE_SYMM_ALGO_##cipher_algo,\
	.mode = RKCE_SYMM_MODE_##cipher_mode,\
	.alg.cipher = {\
		.base.cra_name		= #algo_name,\
		.base.cra_driver_name	= #driver_name,\
		.base.cra_priority	= RKCE_PRIORITY,\
		.base.cra_flags		= CRYPTO_ALG_KERN_DRIVER_ONLY |\
					  CRYPTO_ALG_ASYNC |\
					  CRYPTO_ALG_NEED_FALLBACK |\
					  CRYPTO_ALG_INTERNAL,\
		.base.cra_blocksize	= cipher_algo##_BLOCK_SIZE,\
		.base.cra_ctxsize	= sizeof(struct rkce_cipher_ctx),\
		.base.cra_alignmask	= 0x07,\
		.base.cra_module	= THIS_MODULE,\
		.init		= rkce_ablk_init_tfm,\
		.exit		= rkce_ablk_exit_tfm,\
		.min_keysize	= cipher_algo##_MIN_KEY_SIZE,\
		.max_keysize	= cipher_algo##_MAX_KEY_SIZE,\
		.ivsize		= cipher_algo##_BLOCK_SIZE,\
		.chunksize      = cipher_algo##_BLOCK_SIZE,\
		.setkey		= rkce_cipher_setkey,\
		.encrypt	= rkce_cipher_encrypt,\
		.decrypt	= rkce_cipher_decrypt,\
	} \
}

#define  RK_CIPHER_ALGO_XTS_INIT(cipher_algo, algo_name, driver_name) {\
	.name = #algo_name,\
	.type = RKCE_ALGO_TYPE_CIPHER,\
	.algo = RKCE_SYMM_ALGO_##cipher_algo,\
	.mode = RKCE_SYMM_MODE_XTS,\
	.alg.cipher = {\
		.base.cra_name		= #algo_name,\
		.base.cra_driver_name	= #driver_name,\
		.base.cra_priority	= RKCE_PRIORITY,\
		.base.cra_flags		= CRYPTO_ALG_KERN_DRIVER_ONLY |\
					  CRYPTO_ALG_ASYNC |\
					  CRYPTO_ALG_NEED_FALLBACK |\
					  CRYPTO_ALG_INTERNAL,\
		.base.cra_blocksize	= cipher_algo##_BLOCK_SIZE,\
		.base.cra_ctxsize	= sizeof(struct rkce_cipher_ctx),\
		.base.cra_alignmask	= 0x07,\
		.base.cra_module	= THIS_MODULE,\
		.init		= rkce_ablk_init_tfm,\
		.exit		= rkce_ablk_exit_tfm,\
		.min_keysize	= cipher_algo##_MAX_KEY_SIZE,\
		.max_keysize	= cipher_algo##_MAX_KEY_SIZE * 2,\
		.ivsize		= cipher_algo##_BLOCK_SIZE,\
		.chunksize      = cipher_algo##_BLOCK_SIZE,\
		.setkey		= rkce_cipher_setkey,\
		.encrypt	= rkce_cipher_encrypt,\
		.decrypt	= rkce_cipher_decrypt,\
	} \
}

#define RK_HASH_ALGO_INIT(hash_algo, algo_name) {\
	.name = #algo_name,\
	.type = RKCE_ALGO_TYPE_HASH,\
	.algo = RKCE_HASH_ALGO_##hash_algo,\
	.alg.hash = {\
		.init   = rkce_ahash_init,\
		.update = rkce_ahash_update,\
		.final  = rkce_ahash_final,\
		.finup  = rkce_ahash_finup,\
		.export = rkce_ahash_export,\
		.import = rkce_ahash_import,\
		.digest = rkce_ahash_digest,\
		.halg = {\
			.digestsize = hash_algo##_DIGEST_SIZE,\
			.statesize = sizeof(struct rkce_ahash_request_ctx),\
			.base = {\
				.cra_name = #algo_name,\
				.cra_driver_name = #algo_name"-rk",\
				.cra_priority = RKCE_PRIORITY,\
				.cra_flags = CRYPTO_ALG_KERN_DRIVER_ONLY |\
					     CRYPTO_ALG_ASYNC |\
					     CRYPTO_ALG_NEED_FALLBACK |\
					     CRYPTO_ALG_INTERNAL,\
				.cra_blocksize = hash_algo##_BLOCK_SIZE,\
				.cra_ctxsize   = sizeof(struct rkce_ahash_ctx),\
				.cra_alignmask = 0,\
				.cra_init      = rkce_cra_hash_init,\
				.cra_exit      = rkce_cra_hash_exit,\
				.cra_module    = THIS_MODULE,\
			} \
		} \
	} \
}

#define RK_HMAC_ALGO_INIT(hash_algo, algo_name) {\
	.name = "hmac(" #algo_name ")",\
	.type = RKCE_ALGO_TYPE_HMAC,\
	.algo = RKCE_HASH_ALGO_##hash_algo,\
	.alg.hash = {\
		.init   = rkce_ahash_init,\
		.update = rkce_ahash_update,\
		.final  = rkce_ahash_final,\
		.finup  = rkce_ahash_finup,\
		.export = rkce_ahash_export,\
		.import = rkce_ahash_import,\
		.digest = rkce_ahash_digest,\
		.setkey = rkce_ahash_hmac_setkey,\
		.halg = {\
			.digestsize = hash_algo##_DIGEST_SIZE,\
			.statesize = sizeof(struct rkce_ahash_request_ctx),\
			.base = {\
				.cra_name = "hmac(" #algo_name ")",\
				.cra_driver_name = "hmac-" #algo_name "-rk",\
				.cra_priority = RKCE_PRIORITY,\
				.cra_flags = CRYPTO_ALG_KERN_DRIVER_ONLY |\
					     CRYPTO_ALG_ASYNC |\
					     CRYPTO_ALG_NEED_FALLBACK |\
					     CRYPTO_ALG_INTERNAL,\
				.cra_blocksize = hash_algo##_BLOCK_SIZE,\
				.cra_ctxsize   = sizeof(struct rkce_ahash_ctx),\
				.cra_alignmask = 0,\
				.cra_init      = rkce_cra_hash_init,\
				.cra_exit      = rkce_cra_hash_exit,\
				.cra_module    = THIS_MODULE,\
			} \
		} \
	} \
}

#define RK_ASYM_ECC_INIT(key_bits) {\
	.name = "ecc-" #key_bits, \
	.type = RKCE_ALGO_TYPE_ASYM, \
	.algo = RKCE_ASYM_ALGO_ECC_P##key_bits, \
	.alg.asym = { \
		.verify      = rkce_ec_verify, \
		.set_pub_key = rkce_ec_set_pub_key, \
		.max_size    = rkce_ec_max_size, \
		.init        = rkce_ec_init_tfm, \
		.exit        = rkce_ec_exit_tfm, \
		.reqsize     = sizeof(struct rkce_asym_request_ctx), \
		.base = { \
			.cra_name        = "ecdsa-nist-p" #key_bits, \
			.cra_driver_name = "ecdsa-nist-p" #key_bits "-rk", \
			.cra_priority    = RKCE_PRIORITY, \
			.cra_module      = THIS_MODULE, \
			.cra_ctxsize     = sizeof(struct rkce_ecc_ctx), \
		},\
	} \
}

int rkce_enable_clk(struct rkce_dev *rk_dev);
void rkce_disable_clk(struct rkce_dev *rk_dev);

#endif
