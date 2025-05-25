// SPDX-License-Identifier: GPL-2.0
/*
 * Crypto acceleration support for Rockchip crypto engine
 *
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Lin Jinhan <troy.lin@rock-chips.com>
 *
 */
#define RKCE_MODULE_TAG		"ASYM"
#define RKCE_MODULE_OFFSET	12

#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>

#include "rkce_akcipher.h"
#include "rkce_core.h"
#include "rkce_dev.h"
#include "rkce_debug.h"
#include "rkce_pka.h"
#include "rkce_ecc.h"
#include "rkce_sm2signature.asn1.h"
#include "rkce_ecdsasignature.asn1.h"

static DEFINE_MUTEX(akcipher_mutex);

static void rkce_rsa_adjust_rsa_key(struct rsa_key *key)
{
	if (key->n_sz && key->n && !key->n[0]) {
		key->n++;
		key->n_sz--;
	}

	if (key->e_sz && key->e && !key->e[0]) {
		key->e++;
		key->e_sz--;
	}

	if (key->d_sz && key->d && !key->d[0]) {
		key->d++;
		key->d_sz--;
	}
}

static void rkce_rsa_clear_ctx(struct rkce_rsa_ctx *ctx)
{
	rk_trace("enter.\n");

	/* Free the old key if any */
	rkce_bn_free(ctx->n);
	ctx->n = NULL;

	rkce_bn_free(ctx->e);
	ctx->e = NULL;

	rkce_bn_free(ctx->d);
	ctx->d = NULL;

	rk_trace("exit.\n");
}

static int rkce_rsa_setkey(struct crypto_akcipher *tfm, const void *key,
			   unsigned int keylen, bool private)
{
	struct rkce_rsa_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct rsa_key rsa_key;
	int ret = -ENOMEM;

	rk_trace("enter.\n");

	rk_debug("set %s keylen = %u\n", private ? "private" : "public", keylen);

	rkce_rsa_clear_ctx(ctx);

	memset(&rsa_key, 0x00, sizeof(rsa_key));

	if (private)
		ret = rsa_parse_priv_key(&rsa_key, key, keylen);
	else
		ret = rsa_parse_pub_key(&rsa_key, key, keylen);

	if (ret < 0)
		goto error;

	rkce_rsa_adjust_rsa_key(&rsa_key);

	ctx->n = rkce_bn_alloc(rsa_key.n_sz);
	if (!ctx->n)
		goto error;

	ctx->e = rkce_bn_alloc(rsa_key.e_sz);
	if (!ctx->e)
		goto error;

	rkce_bn_set_data(ctx->n, rsa_key.n, rsa_key.n_sz, RK_BG_BIG_ENDIAN);
	rkce_bn_set_data(ctx->e, rsa_key.e, rsa_key.e_sz, RK_BG_BIG_ENDIAN);

	if (private) {
		ctx->d = rkce_bn_alloc(rsa_key.d_sz);
		if (!ctx->d)
			goto error;

		rkce_bn_set_data(ctx->d, rsa_key.d, rsa_key.d_sz, RK_BG_BIG_ENDIAN);
	}

	rk_trace("exit.\n");

	return 0;
error:
	rkce_rsa_clear_ctx(ctx);

	rk_trace("exit.\n");

	return ret;
}

static unsigned int rkce_rsa_max_size(struct crypto_akcipher *tfm)
{
	struct rkce_rsa_ctx *ctx = akcipher_tfm_ctx(tfm);

	return rkce_bn_get_size(ctx->n);
}

static int rkce_rsa_setpubkey(struct crypto_akcipher *tfm, const void *key,
			      unsigned int keylen)
{
	return rkce_rsa_setkey(tfm, key, keylen, false);
}

static int rkce_rsa_setprivkey(struct crypto_akcipher *tfm, const void *key,
			       unsigned int keylen)
{
	return rkce_rsa_setkey(tfm, key, keylen, true);
}

static int rkce_rsa_calc(struct akcipher_request *req, bool encrypt)
{
	struct crypto_akcipher *tfm = crypto_akcipher_reqtfm(req);
	struct rkce_rsa_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct rkce_bignum *in = NULL, *out = NULL;
	uint32_t key_byte_size;
	uint8_t *tmp_buf = NULL;
	int ret = -EINVAL;

	rk_trace("enter.\n");

	if (unlikely(!ctx->n || !ctx->e))
		goto exit;

	if (!encrypt && !ctx->d)
		goto exit;

	key_byte_size = rkce_bn_get_size(ctx->n);

	if (req->dst_len < key_byte_size) {
		req->dst_len = key_byte_size;
		ret = -EOVERFLOW;
		goto exit;
	}

	if (req->src_len > key_byte_size)
		goto exit;

	in = rkce_bn_alloc(key_byte_size);
	if (!in) {
		ret = -ENOMEM;
		goto exit;
	}

	out = rkce_bn_alloc(key_byte_size);
	if (!out) {
		ret = -ENOMEM;
		goto exit;
	}

	tmp_buf = kzalloc(key_byte_size, GFP_KERNEL);
	if (!tmp_buf) {
		ret = -ENOMEM;
		goto exit;
	}

	if (!sg_copy_to_buffer(req->src, sg_nents(req->src), tmp_buf, req->src_len)) {
		rk_err("sg copy err\n");
		ret =  -EINVAL;
		goto exit;
	}

	ret = rkce_bn_set_data(in, tmp_buf, req->src_len, RK_BG_BIG_ENDIAN);
	if (ret)
		goto exit;

	mutex_lock(&akcipher_mutex);

	if (encrypt)
		ret = rkce_pka_expt_mod(in, ctx->e, ctx->n, out);
	else
		ret = rkce_pka_expt_mod(in, ctx->d, ctx->n, out);

	mutex_unlock(&akcipher_mutex);

	if (ret)
		goto exit;

	ret = rkce_bn_get_data(out, tmp_buf, key_byte_size, RK_BG_BIG_ENDIAN);
	if (ret)
		goto exit;

	if (!sg_copy_from_buffer(req->dst, sg_nents(req->dst), tmp_buf, key_byte_size)) {
		rk_err("sg copy err\n");
		ret =  -EINVAL;
		goto exit;
	}

	req->dst_len = key_byte_size;

exit:
	kfree(tmp_buf);

	rkce_bn_free(in);
	rkce_bn_free(out);

	rk_trace("exit.\n");

	return ret;
}

static int rkce_rsa_enc(struct akcipher_request *req)
{
	return rkce_rsa_calc(req, true);
}

static int rkce_rsa_dec(struct akcipher_request *req)
{
	return rkce_rsa_calc(req, false);
}

static int rkce_rsa_init_tfm(struct crypto_akcipher *tfm)
{
	struct rkce_rsa_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct akcipher_alg *alg = __crypto_akcipher_alg(tfm->base.__crt_alg);
	struct rkce_algt *algt = container_of(alg, struct rkce_algt, alg.asym);

	rk_trace("enter.\n");

	rk_debug("alloc %s\n", algt->name);

	memzero_explicit(ctx, sizeof(*ctx));

	ctx->algt = algt;


	rkce_pka_set_crypto_base(algt->rk_dev->reg);

	rkce_enable_clk(algt->rk_dev);

	rk_trace("exit.\n");

	return 0;
}

static void rkce_rsa_exit_tfm(struct crypto_akcipher *tfm)
{
	struct rkce_rsa_ctx *ctx = akcipher_tfm_ctx(tfm);

	rk_trace("enter.\n");

	rkce_rsa_clear_ctx(ctx);

	rkce_disable_clk(ctx->algt->rk_dev);

	memzero_explicit(ctx, sizeof(*ctx));

	rk_trace("exit.\n");
}

struct rkce_algt rkce_asym_rsa = {
	.name = "rsa",
	.type = RKCE_ALGO_TYPE_ASYM,
	.algo = RKCE_ASYM_ALGO_RSA,
	.alg.asym = {
		.encrypt      = rkce_rsa_enc,
		.decrypt      = rkce_rsa_dec,
		.set_pub_key  = rkce_rsa_setpubkey,
		.set_priv_key = rkce_rsa_setprivkey,
		.max_size     = rkce_rsa_max_size,
		.init         = rkce_rsa_init_tfm,
		.exit         = rkce_rsa_exit_tfm,
		.reqsize      = sizeof(struct rkce_asym_request_ctx),
		.base = {
			.cra_name = "rsa",
			.cra_driver_name = "rsa-rk",
			.cra_priority = RKCE_PRIORITY,
			.cra_module = THIS_MODULE,
			.cra_ctxsize = sizeof(struct rkce_rsa_ctx),
		},
	},
};

int rkce_ecc_get_signature_r(void *context, size_t hdrlen, unsigned char tag,
			     const void *value, size_t vlen)
{
	struct rkce_ecp_point *sig = context;
	const uint8_t *tmp_value = value;

	if (!value || !vlen)
		return -EINVAL;

	/* skip first zero */
	if (tmp_value[0] == 0x00) {
		tmp_value += 1;
		vlen -= 1;
	}

	return rkce_bn_set_data(sig->x, tmp_value, vlen, RK_BG_BIG_ENDIAN);
}

int rkce_ecc_get_signature_s(void *context, size_t hdrlen, unsigned char tag,
			     const void *value, size_t vlen)
{
	struct rkce_ecp_point *sig = context;
	const uint8_t *tmp_value = value;

	if (!value || !vlen)
		return -EINVAL;

	/* skip first zero */
	if (tmp_value[0] == 0x00) {
		tmp_value += 1;
		vlen -= 1;
	}

	return rkce_bn_set_data(sig->y, tmp_value, vlen, RK_BG_BIG_ENDIAN);
}

static int rkce_ec_verify(struct akcipher_request *req)
{
	struct crypto_akcipher *tfm = crypto_akcipher_reqtfm(req);
	struct rkce_ecc_ctx *ctx = akcipher_tfm_ctx(tfm);
	size_t keylen = ctx->nbits / 8;
	struct rkce_ecp_point *sig_point = NULL;
	uint8_t rawhash[RK_ECP_MAX_BYTES];
	unsigned char *buffer;
	ssize_t diff;
	int ret;

	if (unlikely(!ctx->pub_key_set))
		return -EINVAL;

	buffer = kmalloc(req->src_len + req->dst_len, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	sig_point = rkce_ecc_alloc_point_zero(RK_ECP_MAX_BYTES);
	if (!sig_point) {
		ret = -ENOMEM;
		goto exit;
	}

	sg_pcopy_to_buffer(req->src, sg_nents_for_len(req->src, req->src_len + req->dst_len),
			   buffer, req->src_len + req->dst_len, 0);

	if (ctx->group_id == RK_ECP_DP_SM2P256V1)
		ret = asn1_ber_decoder(&rkce_sm2signature_decoder,
				       sig_point, buffer, req->src_len);
	else
		ret = asn1_ber_decoder(&rkce_ecdsasignature_decoder,
				       sig_point, buffer, req->src_len);
	if (ret < 0)
		goto exit;

	/* if the hash is shorter then we will add leading zeros to fit to ndigits */
	memset(rawhash, 0x00, sizeof(rawhash));

	diff = keylen - req->dst_len;
	if (diff >= 0) {
		if (diff)
			memset(rawhash, 0, diff);

		memcpy(&rawhash[diff], buffer + req->src_len, req->dst_len);
	} else if (diff < 0) {
		/* given hash is longer, we take the left-most bytes */
		memcpy(&rawhash, buffer + req->src_len, keylen);
	}

	mutex_lock(&akcipher_mutex);

	ret = rkce_ecc_verify(ctx->group_id, rawhash, keylen, ctx->point_Q, sig_point);
	mutex_unlock(&akcipher_mutex);
exit:
	kfree(buffer);
	rkce_ecc_free_point(sig_point);

	rk_trace("ret = %d\n", ret);

	return ret;
}

/*
 * Set the public key given the raw uncompressed key data from an X509
 * certificate. The key data contain the concatenated X and Y coordinates of
 * the public key.
 */
static int rkce_ec_set_pub_key(struct crypto_akcipher *tfm, const void *key, unsigned int keylen)
{
	struct rkce_ecc_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct rkce_ecp_point *pub_Q = ctx->point_Q;
	const unsigned char *d = key;
	uint32_t nbytes;

	rk_trace("enter.\n");

	if (keylen < 1 || (((keylen - 1) >> 1) % sizeof(uint32_t)) != 0)
		return -EINVAL;

	/* we only accept uncompressed format indicated by '4' */
	if (d[0] != 4)
		return -EINVAL;

	keylen--;
	d++;

	nbytes = keylen / 2;

	rk_debug("keylen = %u, nbytes = %u, group_id = %u, curve_byte = %u\n",
		 keylen, nbytes, ctx->group_id,
		 rkce_ecc_get_curve_nbits(ctx->group_id) / 8);

	if (nbytes != rkce_ecc_get_curve_nbits(ctx->group_id) / 8)
		return -EINVAL;

	rkce_bn_set_data(pub_Q->x, d, nbytes, RK_BG_BIG_ENDIAN);
	rkce_bn_set_data(pub_Q->y, d + nbytes, nbytes, RK_BG_BIG_ENDIAN);

	if (rkce_ecp_point_is_zero(pub_Q))
		return -EINVAL;

	ctx->pub_key_set = true;

	return 0;
}

static unsigned int rkce_ec_max_size(struct crypto_akcipher *tfm)
{
	rk_trace("enter.\n");

	return rkce_ecc_get_max_size();
}

static int rkce_ec_init_tfm(struct crypto_akcipher *tfm)
{
	struct rkce_ecc_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct akcipher_alg *alg = __crypto_akcipher_alg(tfm->base.__crt_alg);
	struct rkce_algt *algt = container_of(alg, struct rkce_algt, alg.asym);

	rk_trace("enter.\n");

	rk_debug("alloc %s\n", algt->name);

	memzero_explicit(ctx, sizeof(*ctx));

	ctx->algt = algt;

	ctx->group_id = rkce_ecc_get_group_id(algt->algo);
	ctx->nbits    = rkce_ecc_get_curve_nbits(ctx->group_id);
	ctx->point_Q  = rkce_ecc_alloc_point_zero(RK_ECP_MAX_BYTES);

	rkce_enable_clk(ctx->algt->rk_dev);

	rkce_ecc_init(algt->rk_dev->reg);

	return 0;
}

static void rkce_ec_exit_tfm(struct crypto_akcipher *tfm)
{
	struct rkce_ecc_ctx *ctx = akcipher_tfm_ctx(tfm);

	rk_trace("enter.\n");

	if (!ctx)
		return;

	rkce_ecc_free_point(ctx->point_Q);

	rkce_ecc_deinit();

	rkce_disable_clk(ctx->algt->rk_dev);

	memzero_explicit(ctx, sizeof(*ctx));

	rk_trace("exit.\n");
}

struct rkce_algt rkce_asym_ecc_p192 = RK_ASYM_ECC_INIT(192);
struct rkce_algt rkce_asym_ecc_p224 = RK_ASYM_ECC_INIT(224);
struct rkce_algt rkce_asym_ecc_p256 = RK_ASYM_ECC_INIT(256);

struct rkce_algt rkce_asym_sm2 = {
	.name = "sm2",
	.type = RKCE_ALGO_TYPE_ASYM,
	.algo = RKCE_ASYM_ALGO_SM2,
	.alg.asym = {
		.verify      = rkce_ec_verify,
		.set_pub_key = rkce_ec_set_pub_key,
		.max_size    = rkce_ec_max_size,
		.init        = rkce_ec_init_tfm,
		.exit        = rkce_ec_exit_tfm,
		.reqsize     = sizeof(struct rkce_asym_request_ctx),
		.base = {
			.cra_name        = "sm2",
			.cra_driver_name = "sm2-rk",
			.cra_priority    = RKCE_PRIORITY,
			.cra_module      = THIS_MODULE,
			.cra_ctxsize     = sizeof(struct rkce_ecc_ctx),
		},
	}
};
