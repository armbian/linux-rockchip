// SPDX-License-Identifier: GPL-2.0
/*
 * Crypto acceleration support for Rockchip crypto engine
 *
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Lin Jinhan <troy.lin@rock-chips.com>
 *
 */
#define RKCE_MODULE_TAG		"HASH"
#define RKCE_MODULE_OFFSET	8

#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>

#include "rkce_core.h"
#include "rkce_dev.h"
#include "rkce_debug.h"
#include "rkce_monitor.h"
#include "rkce_hash.h"

static inline struct rkce_ahash_ctx *hash_req2ctx(struct ahash_request *req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);

	return crypto_ahash_ctx(tfm);
}

static inline struct rkce_ahash_ctx *asyn_req2ctx(void *async_req)
{
	struct ahash_request *req = container_of(async_req, struct ahash_request, base);

	return hash_req2ctx(req);
}

static struct rkce_hash_td *rkce_hash_td_chain_alloc(uint32_t sgs_nents,
						     struct rkce_hash_td_buf *td_buf)
{
	int ret = -ENOMEM;
	struct rkce_hash_td *td_head = NULL;
	uint32_t i, td_nums;

	td_nums = DIV_ROUND_UP(sgs_nents, RKCE_TD_SG_NUM);

	rk_debug("sgs_nents = %u, td_nums = %u\n", sgs_nents, td_nums);

	td_head = rkce_cma_alloc(sizeof(*td_head) * td_nums);
	if (!td_head) {
		rk_debug("rkce_cma_alloc %u td failed.\n", td_nums);
		goto error;
	}

	for (i = 0; i < td_nums; i++) {
		ret = rkce_init_hash_td(&td_head[i], td_buf);
		if (ret) {
			rk_debug("rkce_init_symm_td td[%u] failed.\n", i);
			goto error;
		}

		if (i < td_nums - 1)
			td_head[i].next_task = rkce_cma_virt2phys(&td_head[i + 1]);

	}

	return td_head;
error:
	rkce_cma_free(td_head);

	return NULL;
}

static void rkce_hash_td_chain_free(struct rkce_hash_td *td_head)
{
	rkce_cma_free(td_head);
}

static void rkce_set_hash_td_sg(struct rkce_hash_td *td_head,
				uint32_t index, uint32_t len,
				const dma_addr_t in)
{
	struct rkce_hash_td *cur_td = &td_head[index / RKCE_TD_SG_NUM];
	uint32_t sg_idx = index % 8;

	memset(&(cur_td->sg[sg_idx]), 0x00, sizeof(struct rkce_sg_info));

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	cur_td->sg[sg_idx].src_addr_h = in >> 32;
#endif

	cur_td->sg[sg_idx].src_addr_l = in & 0xffffffff;
	cur_td->sg[sg_idx].src_size   = len;
}

static int rkce_hash_set_td_chain(struct scatterlist *sgs,
				  uint32_t data_len, struct rkce_hash_td *td_head,
				  struct rkce_hash_td_ctrl ctrl)
{
	uint32_t last_td_idx = 0;
	uint32_t td_sg_idx = 0;
	uint32_t src_len;
	dma_addr_t src_dma;
	int first_pkg, last_pkg, int_en;

	rk_debug("sgs = %p data_len = %u, td_head = %p", sgs, data_len, td_head);

	/* only set first td*/
	first_pkg      = ctrl.first_pkg;
	ctrl.first_pkg = 0;
	last_pkg       = ctrl.last_pkg;
	ctrl.last_pkg  = 0;

	/* only set last td */
	int_en      = ctrl.int_en;
	ctrl.int_en = 0;

	while (data_len) {
		if (!sgs) {
			rk_err("sgs is empty\n");
			goto error;
		}

		src_len = sg_dma_len(sgs);
		src_dma = sg_dma_address(sgs);

		rk_debug("data_len = %u, src_len = %u, src_dma = %pad, td_sg_idx = %u\n",
			 data_len, src_len, &src_dma, td_sg_idx);

		/* in case src length too long */
		if (src_len > data_len)
			src_len = data_len;

		/* set one td and point to next */
		rkce_set_hash_td_sg(td_head, td_sg_idx, src_len, src_dma);

		if (td_sg_idx % RKCE_TD_SG_NUM == 0)
			memcpy(&(td_head[td_sg_idx / RKCE_TD_SG_NUM].ctrl), &ctrl, sizeof(ctrl));

		td_sg_idx++;
		data_len -= src_len;
		sgs     = sg_next(sgs);
	}

	/* for data_len = 0 */
	if (td_sg_idx == 0) {
		memcpy(&td_head[0].ctrl, &ctrl, sizeof(ctrl));
		td_sg_idx = 1;
	}

	td_head[0].ctrl.first_pkg = first_pkg;

	/* clear last td */
	last_td_idx = DIV_ROUND_UP(td_sg_idx, RKCE_TD_SG_NUM) - 1;

	td_head[last_td_idx].next_task     = 0;
	td_head[last_td_idx].ctrl.last_pkg = last_pkg;
	td_head[last_td_idx].ctrl.int_en   = int_en;

	return 0;
error:
	return -1;
}

int rkce_hash_request_callback(int result, uint32_t td_id, void *td_addr)
{
	struct rkce_hash_td *td = (struct rkce_hash_td *)td_addr;
	struct rkce_hash_td_buf *td_buf =
		container_of(rkce_cma_phys2virt(td->hash_ctx_addr), struct rkce_hash_td_buf, ctx);
	struct rkce_ahash_ctx *ctx = (struct rkce_ahash_ctx *)td_buf->user_data;
	struct rkce_ahash_request_ctx *rctx = ahash_request_ctx(ctx->req);

	rk_trace("enter.\n");

	ctx->calculated += ctx->req->nbytes;

	if (result != -ETIMEDOUT)
		rkce_monitor_del(rctx->td_head);

	if (result) {
		rkce_dump_reginfo(ctx->algt->rk_dev->hardware);
		rkce_soft_reset(ctx->algt->rk_dev->hardware, RKCE_RESET_HASH);
	} else {
		if (ctx->is_final && ctx->req->result) {
			memcpy(ctx->req->result, td_buf->hash, ctx->algt->alg.hash.halg.digestsize);
			rkce_dumphex("req->result",
				     ctx->req->result, ctx->algt->alg.hash.halg.digestsize);
		}
	}

	crypto_finalize_hash_request(ctx->algt->rk_dev->hash_engine, ctx->req, result);

	rk_trace("exit.\n");

	return 0;
}

static int rkce_hash_prepare(struct crypto_engine *engine, void *breq)
{
	struct ahash_request *req = container_of(breq, struct ahash_request, base);
	struct rkce_ahash_request_ctx *rctx = ahash_request_ctx(req);
	struct rkce_ahash_ctx *ctx = hash_req2ctx(req);
	struct device *dev = ctx->algt->rk_dev->dev;
	struct rkce_hash_td_ctrl ctrl;
	uint32_t sgs_nents;
	int ret;

	rk_trace("enter.\n");

	memzero_explicit(rctx, sizeof(*rctx));

	if (req->nbytes) {
		sgs_nents = sg_nents_for_len(req->src, req->nbytes);

		if (!sg_dma_address(req->src)) {
			ret = dma_map_sg(dev, req->src, sgs_nents, DMA_TO_DEVICE);
			if (ret <= 0)
				return -EINVAL;

			rctx->is_mapped = true;
		}
	} else {
		sgs_nents = 1;
	}

	rctx->td_head = rkce_hash_td_chain_alloc(sgs_nents, ctx->td_buf);
	if (!rctx->td_head) {
		ret = -ENOMEM;
		rk_err("rkce_hash_td_chain_alloc failed ret = %d\n", ret);
		goto exit;
	}

	ctx->req = req;
	ctx->td_buf->user_data = ctx;
	rctx->sgs_nents = sgs_nents;

	memset(&ctrl, 0x00, sizeof(ctrl));

	ctrl.td_type        = RKCE_TD_TYPE_HASH;
	ctrl.hw_pad_en      = 1;
	ctrl.first_pkg      = !ctx->calculated;
	ctrl.last_pkg       = ctx->is_final;
	ctrl.hash_algo      = ctx->algt->algo;
	ctrl.hmac_en        = ctx->is_hmac;
	ctrl.is_preemptible = 0;
	ctrl.int_en         = 1;

	/* translate scatter list to td chain */
	ret = rkce_hash_set_td_chain(req->src, req->nbytes, rctx->td_head, ctrl);
	if (ret)
		goto exit;

exit:
	rk_trace("exit.\n");

	return ret;
}

static int rkce_hash_unprepare(struct crypto_engine *engine, void *breq)
{
	struct ahash_request *req = container_of(breq, struct ahash_request, base);
	struct rkce_ahash_request_ctx *rctx = ahash_request_ctx(req);
	struct rkce_ahash_ctx *ctx = hash_req2ctx(req);
	struct device *dev = ctx->algt->rk_dev->dev;

	rk_trace("enter.\n");

	if (rctx->is_mapped)
		dma_unmap_sg(dev, req->src, rctx->sgs_nents, DMA_TO_DEVICE);

	rkce_hash_td_chain_free(rctx->td_head);

	memzero_explicit(rctx, sizeof(*rctx));

	rk_trace("exit.\n");

	return 0;
}

static int rkce_hash_handle_req(struct ahash_request *req, bool is_final)
{
	struct rkce_ahash_ctx *ctx = hash_req2ctx(req);
	struct crypto_engine *engine = ctx->algt->rk_dev->hash_engine;

	rk_trace("enter.\n");

	ctx->is_final = is_final;

	rk_debug("handle req %u bytes, %s\n", req->nbytes, is_final ? "final" : "update");

	return crypto_transfer_hash_request_to_engine(engine, req);
}

static int rkce_hash_run(struct crypto_engine *engine, void *breq)
{
	struct ahash_request *req = container_of(breq, struct ahash_request, base);
	struct rkce_ahash_request_ctx *rctx = ahash_request_ctx(req);
	struct rkce_ahash_ctx *ctx = hash_req2ctx(req);

	rk_trace("enter.\n");

	rkce_monitor_add(rctx->td_head, rkce_hash_request_callback);

	return rkce_push_td(ctx->algt->rk_dev->hardware, rctx->td_head);
}

static int rkce_ahash_hmac_setkey(struct crypto_ahash *tfm, const uint8_t *key, unsigned int keylen)
{
	unsigned int blocksize = crypto_tfm_alg_blocksize(crypto_ahash_tfm(tfm));
	struct rkce_ahash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct rkce_algt *algt = ctx->algt;
	int ret = 0;

	rk_trace("enter.\n");

	if (algt->algo >= RKCE_HASH_ALGO_MAX) {
		rk_err("hash algo %d invalid\n", algt->algo);
		return -EINVAL;
	}

	if (keylen > PAGE_SIZE) {
		rk_err("keylen %u > %lu invalid\n", keylen, PAGE_SIZE);
		return -EINVAL;
	}

	if (keylen <= blocksize) {
		memcpy(ctx->td_buf->key, key, keylen);
		goto exit;
	}

	/* calculate key hash as hmac key */
	ctx->user_key = rkce_cma_alloc(keylen);
	if (!ctx->user_key) {
		rk_err("rkce_cma_alloc key_td failed.\n");
		ret = -ENOMEM;
		goto exit;
	}

	memcpy(ctx->user_key, key, keylen);

	ctx->key_td = rkce_cma_alloc(sizeof(*(ctx->key_td)));
	if (!ctx->key_td) {
		rk_err("rkce_cma_alloc key_td failed.\n");
		ret = -ENOMEM;
		goto exit;
	}

	ret = rkce_init_hash_td(ctx->key_td, ctx->td_buf);
	if (ret) {
		rk_debug("rkce_init_symm_td key_td failed.\n");
		goto exit;
	}

	/* key hash as hmac key */
	ctx->key_td->hash_addr = ctx->key_td->key_addr;

	rkce_set_hash_td_sg(ctx->key_td, 0, keylen, rkce_cma_virt2phys(ctx->user_key));

	ctx->key_td->ctrl.td_type   = RKCE_TD_TYPE_HASH;
	ctx->key_td->ctrl.hw_pad_en = 1;
	ctx->key_td->ctrl.first_pkg = 1;
	ctx->key_td->ctrl.last_pkg  = 1;
	ctx->key_td->ctrl.hash_algo = ctx->algt->algo;

	ret = rkce_push_td(ctx->algt->rk_dev->hardware, ctx->key_td);
exit:
	rk_trace("exit.\n");

	return ret;
}

static int rkce_ahash_init(struct ahash_request *req)
{
	struct rkce_ahash_ctx *ctx = hash_req2ctx(req);

	rk_trace("enter.\n");

	ctx->req         = NULL;
	ctx->calculated  = 0;
	ctx->is_final    = 0;

	return 0;
}

static int rkce_ahash_update(struct ahash_request *req)
{
	rk_trace("enter.\n");

	return rkce_hash_handle_req(req, false);
}

static int rkce_ahash_final(struct ahash_request *req)
{
	rk_trace("enter.\n");

	return rkce_hash_handle_req(req, true);
}

static int rkce_ahash_finup(struct ahash_request *req)
{
	rk_trace("enter.\n");

	return rkce_ahash_final(req);
}

static int rkce_ahash_digest(struct ahash_request *req)
{
	rk_trace("enter.\n");

	return rkce_ahash_final(req);
}

static int rkce_ahash_import(struct ahash_request *req, const void *in)
{
	struct rkce_ahash_request_ctx *rctx = ahash_request_ctx(req);
	struct rkce_ahash_ctx *ctx = hash_req2ctx(req);

	rk_trace("enter.\n");

	if (!ctx->td_buf)
		return -EFAULT;

	memcpy(rctx, in, sizeof(*rctx));

	memcpy(ctx->td_buf->ctx, rctx->hw_context, RKCE_TD_HASH_CTX_SIZE);

	kfree(rctx->hw_context);

	rk_trace("exit.\n");

	return 0;
}

static int rkce_ahash_export(struct ahash_request *req, void *out)
{
	struct rkce_ahash_request_ctx *rctx = ahash_request_ctx(req);
	struct rkce_ahash_ctx *ctx = hash_req2ctx(req);

	rk_trace("enter.\n");

	if (!ctx->td_buf)
		return -EFAULT;

	rctx->hw_context = kmalloc(RKCE_TD_HASH_CTX_SIZE, GFP_KERNEL);

	memcpy(rctx->hw_context, ctx->td_buf->ctx, RKCE_TD_HASH_CTX_SIZE);

	memcpy(out, rctx, sizeof(*rctx));

	rk_trace("exit.\n");

	return 0;
}

static int rkce_cra_hash_init(struct crypto_tfm *tfm)
{
	struct rkce_ahash_ctx *ctx = crypto_tfm_ctx(tfm);
	struct rkce_algt *algt;
	struct ahash_alg *alg = __crypto_ahash_alg(tfm->__crt_alg);

	rk_trace("enter.\n");

	algt = container_of(alg, struct rkce_algt, alg.hash);

	rk_debug("alloc %s\n", algt->name);

	memzero_explicit(ctx, sizeof(*ctx));

	ctx->algt = algt;

	ctx->enginectx.op.do_one_request    = rkce_hash_run;
	ctx->enginectx.op.prepare_request   = rkce_hash_prepare;
	ctx->enginectx.op.unprepare_request = rkce_hash_unprepare;

	ctx->td_buf = rkce_cma_alloc(sizeof(*(ctx->td_buf)));
	if (!ctx->td_buf) {
		rk_err("rkce_cma_alloc td_buf failed.\n");
		return -ENOMEM;
	}

	ctx->is_hmac = algt->type == RKCE_ALGO_TYPE_HMAC;

	rkce_enable_clk(algt->rk_dev);

	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
				 sizeof(struct rkce_ahash_request_ctx));

	rk_trace("exit.\n");

	return 0;
}

static void rkce_cra_hash_exit(struct crypto_tfm *tfm)
{
	struct rkce_ahash_ctx *ctx = crypto_tfm_ctx(tfm);

	rk_trace("enter.\n");

	rkce_cma_free(ctx->td_buf);
	rkce_cma_free(ctx->key_td);
	rkce_cma_free(ctx->user_key);

	rkce_disable_clk(ctx->algt->rk_dev);

	memzero_explicit(ctx, sizeof(*ctx));

	rk_trace("exit.\n");
}

struct rkce_algt rkce_ahash_md5    = RK_HASH_ALGO_INIT(MD5, md5);
struct rkce_algt rkce_ahash_sha1   = RK_HASH_ALGO_INIT(SHA1, sha1);
struct rkce_algt rkce_ahash_sha224 = RK_HASH_ALGO_INIT(SHA224, sha224);
struct rkce_algt rkce_ahash_sha256 = RK_HASH_ALGO_INIT(SHA256, sha256);
struct rkce_algt rkce_ahash_sha384 = RK_HASH_ALGO_INIT(SHA384, sha384);
struct rkce_algt rkce_ahash_sha512 = RK_HASH_ALGO_INIT(SHA512, sha512);
struct rkce_algt rkce_ahash_sm3    = RK_HASH_ALGO_INIT(SM3, sm3);

struct rkce_algt rkce_hmac_md5     = RK_HMAC_ALGO_INIT(MD5, md5);
struct rkce_algt rkce_hmac_sha1    = RK_HMAC_ALGO_INIT(SHA1, sha1);
struct rkce_algt rkce_hmac_sha256  = RK_HMAC_ALGO_INIT(SHA256, sha256);
struct rkce_algt rkce_hmac_sha512  = RK_HMAC_ALGO_INIT(SHA512, sha512);
struct rkce_algt rkce_hmac_sm3     = RK_HMAC_ALGO_INIT(SM3, sm3);
