// SPDX-License-Identifier: GPL-2.0
/*
 * RSA acceleration support for Rockchip crypto v2
 *
 * Copyright (c) 2020 Rockchip Electronics Co., Ltd.
 *
 * Author: Lin Jinhan <troy.lin@rock-chips.com>
 *
 * Some ideas are from marvell/cesa.c and s5p-sss.c driver.
 */
#include <linux/module.h>
#include <linux/asn1_decoder.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>

#include "rk_crypto_core.h"
#include "rk_crypto_v2.h"
#include "rk_crypto_v2_reg.h"
#include "rk_crypto_v2_pka.h"
#include "rk_crypto_ecc.h"
#include "rk_sm2signature.asn1.h"
#include "rk_ecdsasignature.asn1.h"

#define BG_WORDS2BYTES(words)	((words) * sizeof(u32))
#define BG_BYTES2WORDS(bytes)	(((bytes) + sizeof(u32) - 1) / sizeof(u32))

static DEFINE_MUTEX(akcipher_mutex);

static void rk_rsa_adjust_rsa_key(struct rsa_key *key)
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

static void rk_rsa_clear_ctx(struct rk_rsa_ctx *ctx)
{
	/* Free the old key if any */
	rk_bn_free(ctx->n);
	ctx->n = NULL;

	rk_bn_free(ctx->e);
	ctx->e = NULL;

	rk_bn_free(ctx->d);
	ctx->d = NULL;
}

static int rk_rsa_setkey(struct crypto_akcipher *tfm, const void *key,
			 unsigned int keylen, bool private)
{
	struct rk_rsa_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct rsa_key rsa_key;
	int ret = -ENOMEM;

	rk_rsa_clear_ctx(ctx);

	memset(&rsa_key, 0x00, sizeof(rsa_key));

	if (private)
		ret = rsa_parse_priv_key(&rsa_key, key, keylen);
	else
		ret = rsa_parse_pub_key(&rsa_key, key, keylen);

	if (ret < 0)
		goto error;

	rk_rsa_adjust_rsa_key(&rsa_key);

	ctx->n = rk_bn_alloc(rsa_key.n_sz);
	if (!ctx->n)
		goto error;

	ctx->e = rk_bn_alloc(rsa_key.e_sz);
	if (!ctx->e)
		goto error;

	rk_bn_set_data(ctx->n, rsa_key.n, rsa_key.n_sz, RK_BG_BIG_ENDIAN);
	rk_bn_set_data(ctx->e, rsa_key.e, rsa_key.e_sz, RK_BG_BIG_ENDIAN);

	CRYPTO_DUMPHEX("n = ", ctx->n->data, BG_WORDS2BYTES(ctx->n->n_words));
	CRYPTO_DUMPHEX("e = ", ctx->e->data, BG_WORDS2BYTES(ctx->e->n_words));

	if (private) {
		ctx->d = rk_bn_alloc(rsa_key.d_sz);
		if (!ctx->d)
			goto error;

		rk_bn_set_data(ctx->d, rsa_key.d, rsa_key.d_sz, RK_BG_BIG_ENDIAN);

		CRYPTO_DUMPHEX("d = ", ctx->d->data, BG_WORDS2BYTES(ctx->d->n_words));
	}

	return 0;
error:
	rk_rsa_clear_ctx(ctx);
	return ret;
}

static unsigned int rk_rsa_max_size(struct crypto_akcipher *tfm)
{
	struct rk_rsa_ctx *ctx = akcipher_tfm_ctx(tfm);

	CRYPTO_TRACE();

	return rk_bn_get_size(ctx->n);
}

static int rk_rsa_setpubkey(struct crypto_akcipher *tfm, const void *key,
			    unsigned int keylen)
{
	CRYPTO_TRACE();

	return rk_rsa_setkey(tfm, key, keylen, false);
}

static int rk_rsa_setprivkey(struct crypto_akcipher *tfm, const void *key,
			     unsigned int keylen)
{
	CRYPTO_TRACE();

	return rk_rsa_setkey(tfm, key, keylen, true);
}

static int rk_rsa_calc(struct akcipher_request *req, bool encypt)
{
	struct crypto_akcipher *tfm = crypto_akcipher_reqtfm(req);
	struct rk_rsa_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct rk_bignum *in = NULL, *out = NULL;
	u32 key_byte_size;
	u8 *tmp_buf = NULL;
	int ret = -ENOMEM;

	CRYPTO_TRACE();

	if (unlikely(!ctx->n || !ctx->e))
		return -EINVAL;

	if (!encypt && !ctx->d)
		return -EINVAL;

	key_byte_size = rk_bn_get_size(ctx->n);

	if (req->dst_len < key_byte_size) {
		req->dst_len = key_byte_size;
		return -EOVERFLOW;
	}

	if (req->src_len > key_byte_size)
		return -EINVAL;

	in = rk_bn_alloc(key_byte_size);
	if (!in)
		goto exit;

	out = rk_bn_alloc(key_byte_size);
	if (!out)
		goto exit;

	tmp_buf = kzalloc(key_byte_size, GFP_KERNEL);
	if (!tmp_buf)
		goto exit;

	if (!sg_copy_to_buffer(req->src, sg_nents(req->src), tmp_buf, req->src_len)) {
		dev_err(ctx->rk_dev->dev, "[%s:%d] sg copy err\n",
			__func__, __LINE__);
		ret =  -EINVAL;
		goto exit;
	}

	ret = rk_bn_set_data(in, tmp_buf, req->src_len, RK_BG_BIG_ENDIAN);
	if (ret)
		goto exit;

	CRYPTO_DUMPHEX("in = ", in->data, BG_WORDS2BYTES(in->n_words));

	mutex_lock(&akcipher_mutex);

	if (encypt)
		ret = rk_pka_expt_mod(in, ctx->e, ctx->n, out);
	else
		ret = rk_pka_expt_mod(in, ctx->d, ctx->n, out);

	mutex_unlock(&akcipher_mutex);

	if (ret)
		goto exit;

	CRYPTO_DUMPHEX("out = ", out->data, BG_WORDS2BYTES(out->n_words));

	ret = rk_bn_get_data(out, tmp_buf, key_byte_size, RK_BG_BIG_ENDIAN);
	if (ret)
		goto exit;

	CRYPTO_DUMPHEX("tmp_buf = ", tmp_buf, key_byte_size);

	if (!sg_copy_from_buffer(req->dst, sg_nents(req->dst), tmp_buf, key_byte_size)) {
		dev_err(ctx->rk_dev->dev, "[%s:%d] sg copy err\n",
			__func__, __LINE__);
		ret =  -EINVAL;
		goto exit;
	}

	req->dst_len = key_byte_size;

	CRYPTO_TRACE("ret = %d", ret);
exit:
	kfree(tmp_buf);

	rk_bn_free(in);
	rk_bn_free(out);

	return ret;
}

static int rk_rsa_enc(struct akcipher_request *req)
{
	CRYPTO_TRACE();

	return rk_rsa_calc(req, true);
}

static int rk_rsa_dec(struct akcipher_request *req)
{
	CRYPTO_TRACE();

	return rk_rsa_calc(req, false);
}

static int rk_rsa_start(struct rk_crypto_dev *rk_dev)
{
	CRYPTO_TRACE();

	return -ENOSYS;
}

static int rk_rsa_crypto_rx(struct rk_crypto_dev *rk_dev)
{
	CRYPTO_TRACE();

	return -ENOSYS;
}

static void rk_rsa_complete(struct crypto_async_request *base, int err)
{
	if (base->complete)
		base->complete(base, err);
}

static int rk_rsa_init_tfm(struct crypto_akcipher *tfm)
{
	struct rk_rsa_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct akcipher_alg *alg = __crypto_akcipher_alg(tfm->base.__crt_alg);
	struct rk_crypto_algt *algt;
	struct rk_crypto_dev *rk_dev;
	struct rk_alg_ctx *alg_ctx = &ctx->algs_ctx;

	CRYPTO_TRACE();

	memset(ctx, 0x00, sizeof(*ctx));

	algt = container_of(alg, struct rk_crypto_algt, alg.asym);
	rk_dev = algt->rk_dev;

	if (!rk_dev->request_crypto)
		return -EFAULT;

	rk_dev->request_crypto(rk_dev, "rsa");

	alg_ctx->align_size     = crypto_tfm_alg_alignmask(&tfm->base) + 1;

	alg_ctx->ops.start      = rk_rsa_start;
	alg_ctx->ops.update     = rk_rsa_crypto_rx;
	alg_ctx->ops.complete   = rk_rsa_complete;

	ctx->rk_dev = rk_dev;

	rk_pka_set_crypto_base(ctx->rk_dev->pka_reg);

	return 0;
}

static void rk_rsa_exit_tfm(struct crypto_akcipher *tfm)
{
	struct rk_rsa_ctx *ctx = akcipher_tfm_ctx(tfm);

	CRYPTO_TRACE();

	rk_rsa_clear_ctx(ctx);

	if (ctx->rk_dev && ctx->rk_dev->release_crypto)
		ctx->rk_dev->release_crypto(ctx->rk_dev, "rsa");

	memset(ctx, 0x00, sizeof(*ctx));
}

struct rk_crypto_algt rk_v2_asym_rsa = {
	.name = "rsa",
	.type = ALG_TYPE_ASYM,
	.algo = ASYM_ALGO_RSA,
	.alg.asym = {
		.encrypt = rk_rsa_enc,
		.decrypt = rk_rsa_dec,
		.set_pub_key = rk_rsa_setpubkey,
		.set_priv_key = rk_rsa_setprivkey,
		.max_size = rk_rsa_max_size,
		.init = rk_rsa_init_tfm,
		.exit = rk_rsa_exit_tfm,
		.reqsize = 64,
		.base = {
			.cra_name = "rsa",
			.cra_driver_name = "rsa-rk",
			.cra_priority = RK_CRYPTO_PRIORITY,
			.cra_module = THIS_MODULE,
			.cra_ctxsize = sizeof(struct rk_rsa_ctx),
		},
	},
};

int rk_ecc_get_signature_r(void *context, size_t hdrlen, unsigned char tag,
			   const void *value, size_t vlen)
{
	struct rk_ecp_point *sig = context;
	const uint8_t *tmp_value = value;

	if (!value || !vlen)
		return -EINVAL;

	/* skip first zero */
	if (tmp_value[0] == 0x00) {
		tmp_value += 1;
		vlen -= 1;
	}

	return rk_bn_set_data(sig->x, tmp_value, vlen, RK_BG_BIG_ENDIAN);
}

int rk_ecc_get_signature_s(void *context, size_t hdrlen, unsigned char tag,
			   const void *value, size_t vlen)
{
	struct rk_ecp_point *sig = context;
	const uint8_t *tmp_value = value;

	if (!value || !vlen)
		return -EINVAL;

	/* skip first zero */
	if (tmp_value[0] == 0x00) {
		tmp_value += 1;
		vlen -= 1;
	}

	return rk_bn_set_data(sig->y, tmp_value, vlen, RK_BG_BIG_ENDIAN);
}

static int rk_ecc_verify(struct akcipher_request *req)
{
	struct crypto_akcipher *tfm = crypto_akcipher_reqtfm(req);
	struct rk_ecc_ctx *ctx = akcipher_tfm_ctx(tfm);
	size_t keylen = ctx->nbits / 8;
	struct rk_ecp_point *sig_point = NULL;
	u8 rawhash[RK_ECP_MAX_BYTES];
	unsigned char *buffer;
	ssize_t diff;
	int ret;

	if (unlikely(!ctx->pub_key_set))
		return -EINVAL;

	buffer = kmalloc(req->src_len + req->dst_len, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	sig_point = rk_ecc_alloc_point_zero(RK_ECP_MAX_BYTES);
	if (!sig_point) {
		ret = -ENOMEM;
		goto exit;
	}

	sg_pcopy_to_buffer(req->src, sg_nents_for_len(req->src, req->src_len + req->dst_len),
			   buffer, req->src_len + req->dst_len, 0);

	CRYPTO_DUMPHEX("total signture:", buffer, req->src_len);

	if (ctx->group_id == RK_ECP_DP_SM2P256V1)
		ret = asn1_ber_decoder(&rk_sm2signature_decoder, sig_point, buffer, req->src_len);
	else
		ret = asn1_ber_decoder(&rk_ecdsasignature_decoder, sig_point, buffer, req->src_len);
	if (ret < 0)
		goto exit;

	CRYPTO_DUMPHEX("bn r value = ", sig_point->x->data, BG_WORDS2BYTES(sig_point->x->n_words));
	CRYPTO_DUMPHEX("bn s value = ", sig_point->y->data, BG_WORDS2BYTES(sig_point->y->n_words));

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

	CRYPTO_DUMPHEX("rawhash:", rawhash, sizeof(rawhash));

	mutex_lock(&akcipher_mutex);

	ret = rockchip_ecc_verify(ctx->group_id, rawhash, keylen, ctx->point_Q, sig_point);

	mutex_unlock(&akcipher_mutex);
exit:
	kfree(buffer);
	rk_ecc_free_point(sig_point);

	CRYPTO_TRACE("ret = %d\n", ret);

	return ret;
}

/*
 * Set the public key given the raw uncompressed key data from an X509
 * certificate. The key data contain the concatenated X and Y coordinates of
 * the public key.
 */
static int rk_ecc_set_pub_key(struct crypto_akcipher *tfm, const void *key, unsigned int keylen)
{
	struct rk_ecc_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct rk_ecp_point *pub_Q = ctx->point_Q;
	const unsigned char *d = key;
	uint32_t nbytes;

	CRYPTO_TRACE();

	CRYPTO_DUMPHEX("key = ", key, keylen);

	if (keylen < 1 || (((keylen - 1) >> 1) % sizeof(u32)) != 0)
		return -EINVAL;

	/* we only accept uncompressed format indicated by '4' */
	if (d[0] != 4)
		return -EINVAL;

	keylen--;
	d++;

	nbytes = keylen / 2;

	CRYPTO_TRACE("keylen = %u, nbytes = %u, group_id = %u, curve_byte = %u\n",
		     keylen, nbytes, ctx->group_id,
		     rockchip_ecc_get_curve_nbits(ctx->group_id) / 8);

	if (nbytes != rockchip_ecc_get_curve_nbits(ctx->group_id) / 8)
		return -EINVAL;

	rk_bn_set_data(pub_Q->x, d, nbytes, RK_BG_BIG_ENDIAN);
	rk_bn_set_data(pub_Q->y, d + nbytes, nbytes, RK_BG_BIG_ENDIAN);

	CRYPTO_DUMPHEX("Qx = ", pub_Q->x->data, BG_WORDS2BYTES(pub_Q->x->n_words));
	CRYPTO_DUMPHEX("Qy = ", pub_Q->y->data, BG_WORDS2BYTES(pub_Q->y->n_words));

	if (rk_ecp_point_is_zero(pub_Q))
		return -EINVAL;

	ctx->pub_key_set = true;

	return 0;
}

static unsigned int rk_ecc_max_size(struct crypto_akcipher *tfm)
{
	CRYPTO_TRACE();

	return rockchip_ecc_get_max_size();
}

static int rk_ecc_init_tfm(struct crypto_akcipher *tfm)
{
	struct rk_ecc_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct akcipher_alg *alg = __crypto_akcipher_alg(tfm->base.__crt_alg);
	struct rk_crypto_algt *algt;
	struct rk_crypto_dev *rk_dev;

	CRYPTO_TRACE();

	if (!ctx)
		return -EINVAL;

	memset(ctx, 0x00, sizeof(*ctx));

	algt = container_of(alg, struct rk_crypto_algt, alg.asym);
	rk_dev = algt->rk_dev;

	if (!rk_dev->request_crypto)
		return -EFAULT;

	rk_dev->request_crypto(rk_dev, algt->name);

	ctx->rk_dev   = rk_dev;
	ctx->group_id = rockchip_ecc_get_group_id(algt->algo);
	ctx->nbits    = rockchip_ecc_get_curve_nbits(ctx->group_id);
	ctx->point_Q  = rk_ecc_alloc_point_zero(RK_ECP_MAX_BYTES);

	rockchip_ecc_init(ctx->rk_dev->pka_reg);

	return 0;
}

static void rk_ecc_exit_tfm(struct crypto_akcipher *tfm)
{
	struct rk_ecc_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct akcipher_alg *alg = __crypto_akcipher_alg(tfm->base.__crt_alg);
	struct rk_crypto_algt *algt;

	CRYPTO_TRACE();

	if (!ctx)
		return;

	rk_ecc_free_point(ctx->point_Q);

	rockchip_ecc_deinit();

	algt = container_of(alg, struct rk_crypto_algt, alg.asym);

	if (ctx->rk_dev && ctx->rk_dev->release_crypto)
		ctx->rk_dev->release_crypto(ctx->rk_dev, algt->name);

	memset(ctx, 0x00, sizeof(*ctx));
}

struct rk_crypto_algt rk_asym_ecc_p192 = RK_ASYM_ECC_INIT(192);
struct rk_crypto_algt rk_asym_ecc_p224 = RK_ASYM_ECC_INIT(224);
struct rk_crypto_algt rk_asym_ecc_p256 = RK_ASYM_ECC_INIT(256);

struct rk_crypto_algt rk_asym_sm2 = {
	.name = "sm2",
	.type = ALG_TYPE_ASYM,
	.algo = ASYM_ALGO_SM2,
	.alg.asym = {
		.verify = rk_ecc_verify,
		.set_pub_key = rk_ecc_set_pub_key,
		.max_size = rk_ecc_max_size,
		.init = rk_ecc_init_tfm,
		.exit = rk_ecc_exit_tfm,
		.reqsize = 64,
		.base = {
			.cra_name = "sm2",
			.cra_driver_name = "sm2-rk",
			.cra_priority = RK_CRYPTO_PRIORITY,
			.cra_module = THIS_MODULE,
			.cra_ctxsize = sizeof(struct rk_ecc_ctx),
		},
	}
};
