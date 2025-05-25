// SPDX-License-Identifier: GPL-2.0
/*
 * Crypto acceleration support for Rockchip crypto engine
 *
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Lin Jinhan <troy.lin@rock-chips.com>
 *
 */

#define RKCE_MODULE_TAG		"CIPHER"
#define RKCE_MODULE_OFFSET	6

#include <crypto/scatterwalk.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>

#include "rkce_core.h"
#include "rkce_dev.h"
#include "rkce_debug.h"
#include "rkce_monitor.h"
#include "rkce_skcipher.h"

#define TD_SYNC_TIMEOUT_MS		3000

static void rkce_set_symm_td_sg(struct rkce_symm_td *td_head,
				uint32_t index, uint32_t len,
				const dma_addr_t in,
				const dma_addr_t out);

static inline bool is_algt_aead(struct rkce_algt *algt)
{
	return algt->type == RKCE_ALGO_TYPE_AEAD;
}

static inline struct rkce_cipher_ctx *sk_req2cipher_ctx(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);

	return crypto_skcipher_ctx(tfm);
}

static inline struct rkce_cipher_ctx *asyn_req2cipher_ctx(void *async_req)
{
	struct skcipher_request *areq = container_of(async_req, struct skcipher_request, base);

	return sk_req2cipher_ctx(areq);
}

static uint32_t rkce_get_td_keysize(uint32_t algo, uint32_t mode,  uint32_t key_len)
{
	uint32_t key_size = 0;

	if (mode == RKCE_SYMM_MODE_XTS)
		key_len /= 2;

	if (algo == RKCE_SYMM_ALGO_AES) {
		if (key_len == AES_KEYSIZE_128)
			key_size = RKCE_KEY_AES_128;
		else if (key_len == AES_KEYSIZE_192)
			key_size = RKCE_KEY_AES_192;
		else if (key_len == AES_KEYSIZE_256)
			key_size = RKCE_KEY_AES_256;
		else
			key_size = 0;
	}

	return key_size;
}

static void rkce_reverse_memcpy(void *dst, const void *src, uint32_t size)
{
	char *_dst = (char *)dst, *_src = (char *)src;
	uint32_t i;

	if (!dst || !src || !size)
		return;

	for (i = 0; i < size; ++i)
		_dst[size - i - 1] = _src[i];
}

static int rkce_decrypt_block(void *hardware, uint32_t algo,
			      const uint8_t *key, uint32_t key_len,
			      const uint8_t *input, uint8_t *output)
{
	int ret;
	uint32_t block_size = AES_BLOCK_SIZE;
	uint32_t mode = RKCE_SYMM_MODE_ECB;
	uint8_t *data_buf = NULL;
	struct rkce_symm_td *td = NULL;
	struct rkce_symm_td_buf *td_buf = NULL;

	if (!hardware ||
	    !key ||
	    !input ||
	    !output ||
	    key_len == 0)
		return -EINVAL;

	td_buf = rkce_cma_alloc(sizeof(*td_buf));
	if (!td_buf) {
		rk_debug("rkce_cma_alloc td_buf failed.\n");
		ret = -ENOMEM;
		goto exit;
	}

	td = rkce_cma_alloc(sizeof(*td));
	if (!td) {
		rk_debug("rkce_cma_alloc td failed.\n");
		ret = -ENOMEM;
		goto exit;
	}

	data_buf = rkce_cma_alloc(block_size);
	if (!data_buf) {
		rk_debug("rkce_cma_alloc block failed.\n");
		ret = -ENOMEM;
		goto exit;
	}

	memcpy(td_buf->key1, key, key_len);
	memcpy(data_buf, input, block_size);

	rkce_init_symm_td(td, td_buf);
	rkce_set_symm_td_sg(td, 0, block_size,
			    rkce_cma_virt2phys(data_buf),
			    rkce_cma_virt2phys(data_buf));

	td->ctrl.td_type   = RKCE_TD_TYPE_SYMM;
	td->ctrl.is_dec    = 1;
	td->ctrl.symm_algo = algo;
	td->ctrl.symm_mode = mode;
	td->ctrl.key_size  = rkce_get_td_keysize(algo, mode, key_len);
	td->ctrl.first_pkg = 1;
	td->ctrl.last_pkg  = 1;
	td->ctrl.int_en    = 1;

	ret = rkce_push_td_sync(hardware, td, TD_SYNC_TIMEOUT_MS);
	if (ret == 0)
		memcpy(output, data_buf, block_size);
exit:
	rkce_cma_free(data_buf);
	rkce_cma_free(td);
	rkce_cma_free(td_buf);

	return ret;
}

static void rkce_update_iv(struct rkce_cipher_ctx *ctx, uint8_t *iv)
{
	struct rkce_algt *algt;
	struct rkce_symm_td_buf *td_buf;

	if (!ctx || !ctx->algt || !ctx->td_buf || !iv ||
	    ctx->algt->mode == RKCE_SYMM_MODE_ECB)
		return;

	algt   = ctx->algt;
	td_buf = ctx->td_buf;

	rkce_reverse_memcpy(iv + 0,  td_buf->ctx + 24, 4);
	rkce_reverse_memcpy(iv + 4,  td_buf->ctx + 28, 4);
	rkce_reverse_memcpy(iv + 8,  td_buf->ctx + 0, 4);
	rkce_reverse_memcpy(iv + 12, td_buf->ctx + 4, 4);

	if (algt->mode == RKCE_SYMM_MODE_XTS)
		rkce_decrypt_block(algt->rk_dev->hardware, algt->algo,
				   td_buf->key2, ctx->keylen / 2, iv, iv);

	rkce_dumphex("td_buf->ctx", td_buf->ctx, sizeof(td_buf->ctx));
	rkce_dumphex("updated iv", iv, 16);

	memset(td_buf->ctx, 0x00, sizeof(td_buf->ctx));
}

int rkce_cipher_request_callback(int result, uint32_t td_id, void *td_addr)
{
	struct rkce_symm_td *td = (struct rkce_symm_td *)td_addr;
	struct rkce_symm_td_buf *td_buf =
		container_of(rkce_cma_phys2virt(td->symm_ctx_addr), struct rkce_symm_td_buf, ctx);
	struct rkce_cipher_ctx *ctx = (struct rkce_cipher_ctx *)td_buf->user_data;
	struct crypto_engine *engine = ctx->algt->rk_dev->symm_engine;

	rk_trace("enter.\n");

	if (is_algt_aead(ctx->algt)) {
		struct aead_request *tmp_req = (struct aead_request *)ctx->req;
		struct rkce_cipher_request_ctx *rctx = aead_request_ctx(tmp_req);

		if (result != -ETIMEDOUT)
			rkce_monitor_del(rctx->td_head);

		if (result)
			crypto_finalize_aead_request(engine, ctx->req, result);

		rk_debug("dst = %p, nents %u, tag = %p, authsize = %u,offset = %u\n",
			tmp_req->dst,
			sg_nents(tmp_req->dst),
			td_buf->tag,
			ctx->authsize,
			rctx->assoclen + rctx->cryptlen);

		if (rctx->is_enc) {
			if (!sg_pcopy_from_buffer(tmp_req->dst,
						  sg_nents(tmp_req->dst),
						  td_buf->tag,
						  ctx->authsize,
						  rctx->assoclen + rctx->cryptlen))
				result = -EBADMSG;

		} else {
			uint8_t auth_data[RKCE_TD_TAG_SIZE];

			if (!sg_pcopy_to_buffer(tmp_req->src,
						sg_nents(tmp_req->src),
						auth_data, ctx->authsize,
						rctx->assoclen + rctx->cryptlen) ||
			    crypto_memneq(auth_data, td_buf->tag, ctx->authsize))
				result = -EBADMSG;
		}

		crypto_finalize_aead_request(engine, ctx->req, result);
	} else {
		struct skcipher_request *tmp_req = (struct skcipher_request *)ctx->req;
		struct rkce_cipher_request_ctx *rctx = skcipher_request_ctx(tmp_req);

		if (result != -ETIMEDOUT)
			rkce_monitor_del(rctx->td_head);

		if (result)
			crypto_finalize_skcipher_request(engine, tmp_req, result);

		/* update iv */
		rkce_update_iv(ctx, tmp_req->iv);

		crypto_finalize_skcipher_request(engine, tmp_req, result);
	}

	if (result) {
		rkce_dump_reginfo(ctx->algt->rk_dev->hardware);
		rkce_soft_reset(ctx->algt->rk_dev->hardware, RKCE_RESET_SYMM);
	}

	rk_trace("exit.\n");

	return 0;
}

static int rkce_set_symm_td_buf_key(struct rkce_symm_td_buf *td_buf,
				    uint32_t algo, uint32_t mode,
				    const uint8_t *key, uint32_t key_len)
{
	rk_trace("enter.\n");

	memset(td_buf->key1, 0x00, sizeof(td_buf->key1));
	memset(td_buf->key2, 0x00, sizeof(td_buf->key2));

	if (mode == RKCE_SYMM_MODE_XTS) {
		memcpy(td_buf->key1, key, key_len / 2);
		memcpy(td_buf->key2, key + key_len / 2, key_len / 2);
		rkce_dumphex("key1", td_buf->key1, sizeof(td_buf->key1));
		rkce_dumphex("key2", td_buf->key2, sizeof(td_buf->key2));
	} else {
		memcpy(td_buf->key1, key, key_len);
	}

	if (key_len == DES_KEY_SIZE * 2 &&
	    (algo == RKCE_SYMM_ALGO_DES || algo == RKCE_SYMM_ALGO_TDES))
		memcpy(td_buf->key1 + DES_KEY_SIZE * 2, td_buf->key1, DES_KEY_SIZE);

	rk_trace("exit.\n");

	return 0;
}

static struct rkce_symm_td *rkce_cipher_td_chain_alloc(uint32_t sg_nents,
						       struct rkce_symm_td_buf *td_buf)
{
	int ret = -ENOMEM;
	uint32_t i, td_nums;
	struct rkce_symm_td *td_head = NULL;

	rk_trace("enter.\n");

	td_nums = DIV_ROUND_UP(sg_nents, RKCE_TD_SG_NUM);

	rk_debug("sg_nents = %u, td_nums = %u\n", sg_nents, td_nums);

	td_head = rkce_cma_alloc(sizeof(*td_head) * td_nums);
	if (!td_head) {
		rk_debug("rkce_cma_alloc %u td failed.\n", td_nums);
		goto error;
	}

	for (i = 0; i < td_nums; i++) {
		ret = rkce_init_symm_td(&td_head[i], td_buf);
		if (ret) {
			rk_debug("rkce_init_symm_td td[%u] failed.\n", i);
			goto error;
		}

		if (i < td_nums - 1)
			td_head[i].next_task = rkce_cma_virt2phys(&td_head[i + 1]);

	}

	rk_trace("exit.\n");

	return td_head;
error:
	rkce_cma_free(td_head);

	rk_trace("exit.\n");

	return NULL;
}

static void rkce_cipher_td_chain_free(struct rkce_symm_td *td_head)
{
	rk_trace("enter.\n");

	rkce_cma_free(td_head);

	rk_trace("exit.\n");
}

static void rkce_set_symm_td_sg(struct rkce_symm_td *td_head,
				uint32_t index, uint32_t len,
				const dma_addr_t in,
				const dma_addr_t out)
{
	struct rkce_symm_td *cur_td = &td_head[index / RKCE_TD_SG_NUM];
	uint32_t sg_idx = index % 8;

	rk_trace("enter.\n");

	memset(&(cur_td->sg[sg_idx]), 0x00, sizeof(struct rkce_sg_info));

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	cur_td->sg[sg_idx].src_addr_h = in >> 32;
#endif

	cur_td->sg[sg_idx].src_addr_l = in & 0xffffffff;
	cur_td->sg[sg_idx].src_size   = len;

	if (out) {
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
		cur_td->sg[sg_idx].dst_addr_h = out >> 32;
#endif

		cur_td->sg[sg_idx].dst_addr_l = out & 0xffffffff;
		cur_td->sg[sg_idx].dst_size   = len;
	}

	rk_trace("exit.\n");
}

static int rkce_cipher_set_td_chain(struct scatterlist *sgs, struct scatterlist *sgd,
				    uint32_t cryptlen, struct rkce_symm_td *td_head,
				    struct rkce_symm_td_ctrl ctrl)
{
	uint32_t last_td_idx = 0;
	uint32_t td_sg_idx = 0;
	uint32_t split_len = 0;
	uint32_t src_left = 0, dst_left = 0;
	dma_addr_t src_dma = 0, dst_dma = 0;
	int first_pkg, last_pkg, int_en;

	rk_trace("enter.\n");

	if (cryptlen == 0)
		return -1;

	/* only set first td*/
	first_pkg      = ctrl.first_pkg;
	ctrl.first_pkg = 0;
	last_pkg       = ctrl.last_pkg;
	ctrl.last_pkg  = 0;

	/* only set last td */
	int_en      = ctrl.int_en;
	ctrl.int_en = 0;

	/* first sg */
	src_left = sg_dma_len(sgs);
	src_dma  = sg_dma_address(sgs);

	if (sgd) {
		dst_left = sg_dma_len(sgd);
		dst_dma  = sg_dma_address(sgd);
	}

	while (cryptlen) {
		rk_debug("cryptlen = %u, src_left = %u, src_dma = %pad, dst_left = %u, dst_dma = %pad\n",
			 cryptlen, src_left, &src_dma, dst_left, &dst_dma);
		if (src_left == 0) {
			sgs = sg_next(sgs);
			if (!sgs) {
				rk_debug("sgs not enough.\n");
				goto error;
			}
			src_left = sg_dma_len(sgs);
			src_dma  = sg_dma_address(sgs);
		}

		if (sgd && dst_left == 0) {
			sgd = sg_next(sgd);
			if (!sgd) {
				rk_debug("sgd not enough.\n");
				goto error;
			}

			dst_left = sg_dma_len(sgd);
			dst_dma  = sg_dma_address(sgd);
		}

		split_len = src_left >= dst_left && sgd ? dst_left : src_left;

		/* in case split length too long */
		if (split_len > cryptlen)
			split_len = cryptlen;

		rk_debug("split_len = %u\n", split_len);
		/* set one td and point to next */
		rkce_set_symm_td_sg(td_head, td_sg_idx, split_len, src_dma, dst_dma);

		if (td_sg_idx % RKCE_TD_SG_NUM == 0)
			memcpy(&td_head[td_sg_idx / RKCE_TD_SG_NUM].ctrl, &ctrl, sizeof(ctrl));

		td_sg_idx++;

		cryptlen -= split_len;
		src_dma  += split_len;
		src_left -= split_len;

		if (sgd) {
			dst_dma  += split_len;
			dst_left -= split_len;
		}
	}

	td_head[0].ctrl.first_pkg = first_pkg;

	/* clear last td */
	last_td_idx = DIV_ROUND_UP(td_sg_idx, RKCE_TD_SG_NUM) - 1;

	td_head[last_td_idx].next_task     = 0;
	td_head[last_td_idx].ctrl.last_pkg = last_pkg;
	td_head[last_td_idx].ctrl.int_en   = int_en;

	rk_trace("exit.\n");

	return 0;
error:
	rk_trace("exit.\n");

	return -EINVAL;
}

static int rkce_cipher_sg_map(struct device *dev,
			      struct scatterlist *sgs, uint32_t sgs_nents,
			      struct scatterlist *sgd, uint32_t sgd_nents)
{
	int ret = 0;

	rk_trace("enter.\n");

	/* flush src & invalid dst */
	if (sgs == sgd) {
		ret = dma_map_sg(dev, sgs, sgs_nents, DMA_BIDIRECTIONAL);
		if (ret <= 0) {
			rk_err("dma_map_sg src dst DMA_BIDIRECTIONAL failed, ret = %d.\n", ret);
			goto exit;
		}
	} else {
		ret = dma_map_sg(dev, sgs, sgs_nents, DMA_TO_DEVICE);
		if (ret <= 0) {
			rk_err("dma_map_sg src DMA_TO_DEVICE failed, ret = %d.\n", ret);
			goto exit;
		}

		ret = dma_map_sg(dev, sgd, sgd_nents, DMA_FROM_DEVICE);
		if (ret <= 0) {
			dma_unmap_sg(dev, sgs, sgs_nents, DMA_TO_DEVICE);
			rk_err("dma_map_sg dst DMA_FROM_DEVICE failed, ret = %d.\n", ret);
			goto exit;
		}
	}

	ret = 0;

exit:
	rk_trace("exit.\n");

	return ret;
}

static void rkce_cipher_sg_unmap(struct device *dev,
				 struct scatterlist *sgs, uint32_t sgs_nents,
				 struct scatterlist *sgd, uint32_t sgd_nents)
{
	rk_trace("enter.\n");

	if (sgs == sgd) {
		dma_unmap_sg(dev, sgs, sgs_nents, DMA_BIDIRECTIONAL);
	} else {
		dma_unmap_sg(dev, sgs, sgs_nents, DMA_TO_DEVICE);
		dma_unmap_sg(dev, sgd, sgd_nents, DMA_FROM_DEVICE);
	}

	rk_trace("exit.\n");
}

static int rkce_common_prepare_req(struct rkce_cipher_ctx *ctx,
				   struct rkce_cipher_request_ctx *rctx,
				   void *req)
{
	int ret = 0;
	struct rkce_symm_td_ctrl ctrl;
	struct device *dev = rctx->dev;
	struct rkce_algt *algt = ctx->algt;

	rk_trace("enter.\n");

	rk_debug("rctx = %p, sgs = %p, sgd = %p\n",
		 rctx, rctx->sgs, rctx->sgd);

	/* check key length */
	if (ctx->keylen == 0) {
		rk_err("Key should set before calculating.\n");
		return -EINVAL;
	}

	rctx->td_head = rkce_cipher_td_chain_alloc(rctx->sgs_nents + rctx->sgd_nents, ctx->td_buf);
	if (!rctx->td_head) {
		ret = -ENOMEM;
		rk_err("rkce_cipher_td_chain_alloc td_head failed ret = %d\n", ret);
		goto exit;
	}

	rk_debug("rctx = %p, sgs = %p, sgd = %p\n",
		 rctx, rctx->sgs, rctx->sgd);

	memset(&ctrl, 0x00, sizeof(ctrl));

	ctrl.td_type   = RKCE_TD_TYPE_SYMM;
	ctrl.is_dec    = !rctx->is_enc;
	ctrl.symm_algo = algt->algo;
	ctrl.symm_mode = algt->mode;
	ctrl.key_size  = rkce_get_td_keysize(algt->algo, algt->mode, ctx->keylen);
	ctrl.iv_len    = ctx->ivlen;
	ctrl.first_pkg = 1;
	ctrl.last_pkg  = 1;
	ctrl.int_en    = 1;
	ctrl.is_aad    = 0;

	rk_debug("rctx = %p, sgs = %p, sgd = %p, sga = %p, is_aead = %d\n",
		 rctx, rctx->sgs, rctx->sgd, rctx->sga, rctx->is_aead);

	if (rctx->is_aead) {
		struct aead_request *tmp_req = (struct aead_request *)req;

		memcpy(ctx->td_buf->iv, tmp_req->iv, ctrl.iv_len);

		if (!rctx->is_dma) {
			ret = rkce_cipher_sg_map(dev,
						 tmp_req->src,
						 sg_nents_for_len(tmp_req->src, rctx->map_total),
						 tmp_req->dst,
						 sg_nents_for_len(tmp_req->dst, rctx->map_total));
			if (ret)
				goto exit;
		}

		rctx->td_aad_head = rkce_cipher_td_chain_alloc(rctx->sga_nents, ctx->td_buf);
		if (!rctx->td_aad_head) {
			ret = -ENOMEM;
			rk_err("rkce_cipher_td_chain_alloc td_aad_head failed ret = %d\n", ret);
			goto exit;
		}

		ctrl.is_aad    = 1;

		ctx->td_buf->gcm_len.aad_len_h = 0;
		ctx->td_buf->gcm_len.aad_len_l = rctx->assoclen;
		ctx->td_buf->gcm_len.pc_len_h  = 0;
		ctx->td_buf->gcm_len.pc_len_l  = rctx->cryptlen;

		/* translate scatter list to td chain */
		ret = rkce_cipher_set_td_chain(rctx->sga, NULL, rctx->assoclen,
					       rctx->td_aad_head, ctrl);
		if (ret)
			goto exit;
	} else {
		struct skcipher_request *tmp_req = (struct skcipher_request *)req;

		memcpy(ctx->td_buf->iv, tmp_req->iv, ctrl.iv_len);

		if (!rctx->is_dma) {
			ret = rkce_cipher_sg_map(dev,
						 rctx->sgs, rctx->sgs_nents,
						 rctx->sgd, rctx->sgd_nents);
			if (ret)
				return ret;
		}
	}

	ctrl.is_aad = 0;

	/* translate scatter list to td chain */
	ret = rkce_cipher_set_td_chain(rctx->sgs, rctx->sgd, rctx->cryptlen, rctx->td_head, ctrl);
	if (ret)
		goto exit;

	rctx->is_mapped = true;

exit:
	rk_trace("exit.\n");

	return ret;
}

static int  rkce_common_unprepare_req(struct rkce_cipher_request_ctx *rctx)
{
	struct device *dev = rctx->dev;

	rk_trace("enter.\n");

	if (!rctx->is_dma && rctx->is_mapped)
		rkce_cipher_sg_unmap(dev, rctx->sgs, rctx->sgs_nents, rctx->sgd, rctx->sgd_nents);

	rkce_cipher_td_chain_free(rctx->td_aad_head);
	rkce_cipher_td_chain_free(rctx->td_head);

	memzero_explicit(rctx, sizeof(*rctx));

	rk_trace("exit.\n");

	return 0;
}

static int rkce_cipher_prepare_req(struct crypto_engine *engine, void *areq)
{
	struct skcipher_request *req = container_of(areq, struct skcipher_request, base);
	struct rkce_cipher_request_ctx *rctx = skcipher_request_ctx(req);
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rkce_cipher_ctx *ctx = crypto_skcipher_ctx(tfm);
	struct device *dev = ctx->algt->rk_dev->dev;

	rk_trace("enter.\n");

	ctx->req = req;

	rctx->dev       = dev;
	rctx->cryptlen  = req->cryptlen;
	rctx->sgs       = req->src;
	rctx->sgs_nents = sg_nents_for_len(req->src, req->cryptlen);
	rctx->sgd       = req->dst;
	rctx->sgd_nents = sg_nents_for_len(req->dst, req->cryptlen);
	rctx->map_total = rctx->cryptlen;
	rctx->is_dma    = sg_dma_address(req->src) && sg_dma_address(req->dst);

	rk_debug("rctx = %p, sgs = %p, sgd = %p\n",
		 rctx, rctx->sgs, rctx->sgd);

	return rkce_common_prepare_req(ctx, rctx, req);
}

static int rkce_cipher_unprepare_req(struct crypto_engine *engine, void *areq)
{
	struct skcipher_request *req = container_of(areq, struct skcipher_request, base);
	struct rkce_cipher_request_ctx *rctx = skcipher_request_ctx(req);

	rk_trace("enter.\n");

	return rkce_common_unprepare_req(rctx);
}

static int rkce_cipher_run_req(struct crypto_engine *engine, void *async_req)
{
	struct skcipher_request *req = container_of(async_req, struct skcipher_request, base);
	struct rkce_cipher_request_ctx *rctx = skcipher_request_ctx(req);
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rkce_cipher_ctx *ctx = crypto_skcipher_ctx(tfm);

	rk_trace("enter.\n");

	rkce_monitor_add(rctx->td_head, rkce_cipher_request_callback);

	return rkce_push_td(ctx->algt->rk_dev->hardware, rctx->td_head);
}

static int rkce_ablk_init_tfm(struct crypto_skcipher *tfm)
{
	struct rkce_cipher_ctx *ctx = crypto_skcipher_ctx(tfm);
	struct skcipher_alg *alg = crypto_skcipher_alg(tfm);
	struct rkce_algt *algt = container_of(alg, struct rkce_algt, alg.cipher);

	rk_trace("enter.\n");

	rk_debug("alloc %s\n", algt->name);

	memzero_explicit(ctx, sizeof(*ctx));

	ctx->algt = algt;
	ctx->ivlen = algt->mode == RKCE_SYMM_MODE_ECB ? 0 : crypto_skcipher_ivsize(tfm);

	ctx->enginectx.op.prepare_request   = rkce_cipher_prepare_req;
	ctx->enginectx.op.do_one_request    = rkce_cipher_run_req;
	ctx->enginectx.op.unprepare_request = rkce_cipher_unprepare_req;

	ctx->td_buf = rkce_cma_alloc(sizeof(*(ctx->td_buf)));
	if (!ctx->td_buf) {
		rk_debug("rkce_cma_alloc td_buf failed.\n");
		return -ENOMEM;
	}

	ctx->td_buf->user_data = ctx;

	crypto_skcipher_set_reqsize(tfm, sizeof(struct rkce_cipher_request_ctx));

	rkce_enable_clk(algt->rk_dev);

	rk_trace("exit.\n");

	return 0;
}

static void rkce_ablk_exit_tfm(struct crypto_skcipher *tfm)
{
	struct rkce_cipher_ctx *ctx = crypto_skcipher_ctx(tfm);

	rk_trace("enter.\n");

	rkce_cma_free(ctx->td_buf);

	rkce_disable_clk(ctx->algt->rk_dev);

	memzero_explicit(ctx, sizeof(*ctx));

	rk_trace("exit.\n");
}

static int rkce_common_setkey(struct rkce_cipher_ctx *ctx, const uint8_t *key, unsigned int keylen)
{
	struct rkce_algt *algt = ctx->algt;
	uint32_t key_factor;

	/* The key length of XTS is twice the normal length */
	key_factor = algt->mode == RKCE_SYMM_MODE_XTS ? 2 : 1;

	rk_debug("algo = %x, mode = %x, key_len = %d\n", algt->algo, algt->mode, keylen);

	switch (algt->algo) {
	case RKCE_SYMM_ALGO_DES:
		if (keylen != DES_KEY_SIZE)
			goto error;
		break;
	case RKCE_SYMM_ALGO_TDES:
		if (keylen != DES_KEY_SIZE * 2 &&
		    keylen != DES_KEY_SIZE * 3)
			goto error;
		break;
	case RKCE_SYMM_ALGO_AES:
		if (keylen != (AES_KEYSIZE_128 * key_factor) &&
		    keylen != (AES_KEYSIZE_192 * key_factor) &&
		    keylen != (AES_KEYSIZE_256 * key_factor))
			goto error;
		break;
	case RKCE_SYMM_ALGO_SM4:
		if (keylen != SM4_KEY_SIZE * key_factor)
			goto error;
		break;
	default:
		goto error;
	}

	ctx->keylen = keylen;

	/* set td buf info */
	return rkce_set_symm_td_buf_key(ctx->td_buf, algt->algo, algt->mode, key, keylen);

error:
	return -EINVAL;
}

static int rkce_cipher_setkey(struct crypto_skcipher *cipher,
			      const uint8_t *key, unsigned int keylen)
{
	rk_trace("enter.\n");

	return rkce_common_setkey(crypto_skcipher_ctx(cipher), key, keylen);
}

static int rkce_cipher_handle_req(struct skcipher_request *req, bool is_enc)
{
	struct rkce_cipher_request_ctx *rctx = skcipher_request_ctx(req);
	struct rkce_cipher_ctx *ctx = sk_req2cipher_ctx(req);
	struct rkce_dev *rk_dev = ctx->algt->rk_dev;
	struct crypto_engine *engine = rk_dev->symm_engine;

	rk_trace("enter.\n");

	memzero_explicit(rctx, sizeof(*rctx));

	rctx->is_enc  = is_enc;
	rctx->is_aead = false;

	rk_debug("cryptlen = %u, %s\n", req->cryptlen, is_enc ? "encrypt" : "decrypt");

	return crypto_transfer_skcipher_request_to_engine(engine, req);
}

static int rkce_cipher_encrypt(struct skcipher_request *req)
{
	rk_trace("enter.\n");

	return rkce_cipher_handle_req(req, true);
}

static int rkce_cipher_decrypt(struct skcipher_request *req)
{
	rk_trace("enter.\n");

	return rkce_cipher_handle_req(req, false);
}

static int rkce_aead_prepare_req(struct crypto_engine *engine, void *areq)
{
	struct aead_request *req = container_of(areq, struct aead_request, base);
	struct rkce_cipher_request_ctx *rctx = aead_request_ctx(req);
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct rkce_cipher_ctx *ctx = crypto_aead_ctx(tfm);
	struct device *dev = ctx->algt->rk_dev->dev;

	rk_trace("enter.\n");

	ctx->req      = req;
	ctx->authsize = crypto_aead_authsize(tfm);

	rctx->dev       = dev;
	rctx->sga       = req->src;
	rctx->sga_nents = sg_nents_for_len(req->src, req->assoclen);
	rctx->assoclen  = req->assoclen;
	rctx->cryptlen  = rctx->is_enc ? req->cryptlen : req->cryptlen - ctx->authsize;
	rctx->map_total = rctx->assoclen + rctx->cryptlen;
	rctx->is_dma    = sg_dma_address(req->src) && sg_dma_address(req->dst);

	rk_debug("assoclen = %u, cryptlen = %u, authsize = %u, is_dma = %u\n",
		 req->assoclen, req->cryptlen, ctx->authsize, rctx->is_dma);

	/* point sg_src and sg_dst skip assoc data */
	rctx->sgs       = scatterwalk_ffwd(rctx->src_sg, req->src, req->assoclen);
	rctx->sgs_nents = sg_nents_for_len(rctx->sgs, rctx->cryptlen);

	if (req->src == req->dst) {
		rctx->sgd = rctx->sgs;
		rctx->sgd_nents = rctx->sgs_nents;
	} else {
		rctx->sgd       = scatterwalk_ffwd(rctx->dst_sg, req->dst, req->assoclen);
		rctx->sgd_nents = sg_nents_for_len(rctx->sgd, rctx->cryptlen);
	}

	return rkce_common_prepare_req(ctx, rctx, req);
}

static int rkce_aead_unprepare_req(struct crypto_engine *engine, void *areq)
{
	struct aead_request *req = container_of(areq, struct aead_request, base);
	struct rkce_cipher_request_ctx *rctx = aead_request_ctx(req);

	rk_trace("enter.\n");

	return rkce_common_unprepare_req(rctx);
}

static int rkce_aead_run_req(struct crypto_engine *engine, void *async_req)
{
	int ret = 0;
	struct aead_request *req = container_of(async_req, struct aead_request, base);
	struct rkce_cipher_request_ctx *rctx = aead_request_ctx(req);
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct rkce_cipher_ctx *ctx = crypto_aead_ctx(tfm);

	rk_trace("enter.\n");

	ret = rkce_push_td_sync(ctx->algt->rk_dev->hardware, rctx->td_aad_head, TD_SYNC_TIMEOUT_MS);
	if (ret) {
		rk_debug("calc aad data error.\n");
		goto exit;
	}

	ret = rkce_push_td(ctx->algt->rk_dev->hardware, rctx->td_head);
	if (ret) {
		rk_debug("calc data error.\n");
		goto exit;
	}

	rkce_monitor_add(rctx->td_head, rkce_cipher_request_callback);
exit:
	return ret;
}

static int rkce_aead_handle_req(struct aead_request *req, bool is_enc)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct rkce_cipher_ctx *ctx = crypto_aead_ctx(tfm);
	struct rkce_cipher_request_ctx *rctx = aead_request_ctx(req);
	struct rkce_dev *rk_dev = ctx->algt->rk_dev;
	struct crypto_engine *engine = rk_dev->symm_engine;

	rk_trace("enter.\n");

	memzero_explicit(rctx, sizeof(*rctx));

	rctx->is_enc  = is_enc;
	rctx->is_aead = true;

	rk_debug("assoclen = %u, cryptlen = %u, %s\n",
		 req->assoclen, req->cryptlen,
		 is_enc ? "encrypt" : "decrypt");

	return crypto_transfer_aead_request_to_engine(engine, req);
}

static int rkce_aead_init_tfm(struct crypto_aead *tfm)
{
	struct rkce_cipher_ctx *ctx = crypto_aead_ctx(tfm);
	struct aead_alg *alg = crypto_aead_alg(tfm);
	struct rkce_algt *algt = container_of(alg, struct rkce_algt, alg.aead);

	rk_trace("enter.\n");

	rk_debug("alloc %s\n", algt->name);

	memzero_explicit(ctx, sizeof(*ctx));

	ctx->algt  = algt;
	ctx->ivlen = crypto_aead_ivsize(tfm);

	ctx->enginectx.op.prepare_request   = rkce_aead_prepare_req;
	ctx->enginectx.op.do_one_request    = rkce_aead_run_req;
	ctx->enginectx.op.unprepare_request = rkce_aead_unprepare_req;

	ctx->td_buf = rkce_cma_alloc(sizeof(*(ctx->td_buf)));
	if (!ctx->td_buf) {
		rk_debug("rkce_cma_alloc td_buf failed.\n");
		return -ENOMEM;
	}

	crypto_aead_set_reqsize(tfm, sizeof(struct rkce_cipher_request_ctx));

	ctx->td_buf->user_data = ctx;

	rkce_enable_clk(algt->rk_dev);

	rk_trace("exit.\n");

	return 0;
}

static void rkce_aead_exit_tfm(struct crypto_aead *tfm)
{
	struct rkce_cipher_ctx *ctx = crypto_aead_ctx(tfm);

	rk_trace("enter.\n");

	rkce_cma_free(ctx->td_buf);

	rkce_disable_clk(ctx->algt->rk_dev);

	memzero_explicit(ctx, sizeof(*ctx));

	rk_trace("exit.\n");
}

static int rkce_aead_setkey(struct crypto_aead *cipher, const uint8_t *key, unsigned int keylen)
{
	rk_trace("enter.\n");

	return rkce_common_setkey(crypto_aead_ctx(cipher), key, keylen);
}

static int rkce_aead_setauthsize(struct crypto_aead *tfm, unsigned int authsize)
{
	struct rkce_cipher_ctx *ctx = crypto_aead_ctx(tfm);

	if (authsize < RKCE_TD_TAG_SIZE_MIN ||
	    authsize > RKCE_TD_TAG_SIZE_MAX)
		return -EINVAL;

	ctx->authsize = authsize;

	return 0;
}

static int rkce_aead_encrypt(struct aead_request *req)
{
	rk_trace("enter.\n");

	return rkce_aead_handle_req(req, true);
}

static int rkce_aead_decrypt(struct aead_request *req)
{
	rk_trace("enter.\n");

	return rkce_aead_handle_req(req, false);
}

struct rkce_algt rkce_ecb_sm4_alg =
	RK_CIPHER_ALGO_INIT(SM4, ECB, ecb(sm4), ecb-sm4-rk);

struct rkce_algt rkce_cbc_sm4_alg =
	RK_CIPHER_ALGO_INIT(SM4, CBC, cbc(sm4), cbc-sm4-rk);

struct rkce_algt rkce_xts_sm4_alg =
	RK_CIPHER_ALGO_XTS_INIT(SM4, xts(sm4), xts-sm4-rk);

struct rkce_algt rkce_cfb_sm4_alg =
	RK_CIPHER_ALGO_INIT(SM4, CFB, cfb(sm4), cfb-sm4-rk);

struct rkce_algt rkce_ofb_sm4_alg =
	RK_CIPHER_ALGO_INIT(SM4, OFB, ofb(sm4), ofb-sm4-rk);

struct rkce_algt rkce_ctr_sm4_alg =
	RK_CIPHER_ALGO_INIT(SM4, CTR, ctr(sm4), ctr-sm4-rk);

struct rkce_algt rkce_gcm_sm4_alg =
	RK_AEAD_ALGO_INIT(SM4, GCM, gcm(sm4), gcm-sm4-rk);

struct rkce_algt rkce_ecb_aes_alg =
	RK_CIPHER_ALGO_INIT(AES, ECB, ecb(aes), ecb-aes-rk);

struct rkce_algt rkce_cbc_aes_alg =
	RK_CIPHER_ALGO_INIT(AES, CBC, cbc(aes), cbc-aes-rk);

struct rkce_algt rkce_xts_aes_alg =
	RK_CIPHER_ALGO_XTS_INIT(AES, xts(aes), xts-aes-rk);

struct rkce_algt rkce_cfb_aes_alg =
	RK_CIPHER_ALGO_INIT(AES, CFB, cfb(aes), cfb-aes-rk);

struct rkce_algt rkce_ofb_aes_alg =
	RK_CIPHER_ALGO_INIT(AES, OFB, ofb(aes), ofb-aes-rk);

struct rkce_algt rkce_ctr_aes_alg =
	RK_CIPHER_ALGO_INIT(AES, CTR, ctr(aes), ctr-aes-rk);

struct rkce_algt rkce_gcm_aes_alg =
	RK_AEAD_ALGO_INIT(AES, GCM, gcm(aes), gcm-aes-rk);

struct rkce_algt rkce_ecb_des_alg =
	RK_CIPHER_ALGO_INIT(DES, ECB, ecb(des), ecb-des-rk);

struct rkce_algt rkce_cbc_des_alg =
	RK_CIPHER_ALGO_INIT(DES, CBC, cbc(des), cbc-des-rk);

struct rkce_algt rkce_cfb_des_alg =
	RK_CIPHER_ALGO_INIT(DES, CFB, cfb(des), cfb-des-rk);

struct rkce_algt rkce_ofb_des_alg =
	RK_CIPHER_ALGO_INIT(DES, OFB, ofb(des), ofb-des-rk);

struct rkce_algt rkce_ecb_des3_ede_alg =
	RK_CIPHER_ALGO_INIT(DES3_EDE, ECB, ecb(des3_ede), ecb-des3_ede-rk);

struct rkce_algt rkce_cbc_des3_ede_alg =
	RK_CIPHER_ALGO_INIT(DES3_EDE, CBC, cbc(des3_ede), cbc-des3_ede-rk);

struct rkce_algt rkce_cfb_des3_ede_alg =
	RK_CIPHER_ALGO_INIT(DES3_EDE, CFB, cfb(des3_ede), cfb-des3_ede-rk);

struct rkce_algt rkce_ofb_des3_ede_alg =
	RK_CIPHER_ALGO_INIT(DES3_EDE, OFB, ofb(des3_ede), ofb-des3_ede-rk);
