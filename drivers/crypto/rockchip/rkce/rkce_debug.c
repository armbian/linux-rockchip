// SPDX-License-Identifier: GPL-2.0
/*
 * Crypto acceleration support for Rockchip crypto engine
 *
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Lin Jinhan <troy.lin@rock-chips.com>
 *
 */

#define RKCE_MODULE_TAG		"DEBUG"
#define RKCE_MODULE_OFFSET	0

#include <linux/kernel.h>

#include "rkce_core.h"
#include "rkce_buf.h"
#include "rkce_debug.h"

ulong rkce_debug_level;

typedef void (*rkce_dump_td_func)(void *td, uint32_t index);

static void rkce_dump_symm_td(void *td, uint32_t index);
static void rkce_dump_hash_td(void *td, uint32_t index);

static const rkce_dump_td_func g_dump_td_funcs[RKCE_TD_TYPE_MAX] = {
	[RKCE_TD_TYPE_SYMM] = rkce_dump_symm_td,
	[RKCE_TD_TYPE_HASH] = rkce_dump_hash_td,
};

static const char *rkce_td_type_str(void *td)
{
	struct rkce_symm_td *symm_td = (struct rkce_symm_td *)td;

	switch (symm_td->ctrl.td_type) {
	case RKCE_TD_TYPE_SYMM:
		return "SYMM";
	case RKCE_TD_TYPE_HASH:
		return "HASH";
	case RKCE_TD_TYPE_SYMM_HASH_IN:
		return "SYMM HASH IN";
	case RKCE_TD_TYPE_SYMM_HASH_OUT:
		return "SYMM HASH OUT";
	default:
		return "UNKNOWN";
	}
}

static const char *rkce_td_symm_algo_str(void *td)
{
	struct rkce_symm_td *symm_td = (struct rkce_symm_td *)td;

	switch (symm_td->ctrl.symm_algo) {
	case RKCE_SYMM_ALGO_AES:
		return "AES";
	case RKCE_SYMM_ALGO_SM4:
		return "SM4";
	case RKCE_SYMM_ALGO_DES:
		return "DES";
	case RKCE_SYMM_ALGO_TDES:
		return "TDES";
	default:
		return "UNKNOWN";
	}
}

static const char *rkce_td_hash_algo_str(void *td)
{
	struct rkce_hash_td *hash_td = (struct rkce_hash_td *)td;

	switch (hash_td->ctrl.hash_algo) {
	case RKCE_HASH_ALGO_SHA1:
		return "SHA1";
	case RKCE_HASH_ALGO_MD5:
		return "MD5";
	case RKCE_HASH_ALGO_SHA256:
		return "SHA256";
	case RKCE_HASH_ALGO_SHA224:
		return "SHA224";
	case RKCE_HASH_ALGO_SM3:
		return "SM3";
	case RKCE_HASH_ALGO_SHA512:
		return "SHA512";
	case RKCE_HASH_ALGO_SHA384:
		return "SHA384";
	case RKCE_HASH_ALGO_SHA512_224:
		return "SHA512_224";
	case RKCE_HASH_ALGO_SHA512_256:
		return "SHA512_256";
	default:
		return "UNKNOWN";
	}
}

static const char *rkce_td_symm_mode_str(void *td)
{
	struct rkce_symm_td *symm_td = (struct rkce_symm_td *)td;

	switch (symm_td->ctrl.symm_mode) {
	case RKCE_SYMM_MODE_ECB:
		return "ECB";
	case RKCE_SYMM_MODE_CBC:
		return "CBC";
	case RKCE_SYMM_MODE_CTS:
		return "CTS";
	case RKCE_SYMM_MODE_CTR:
		return "CTR";
	case RKCE_SYMM_MODE_CFB:
		return "CFB";
	case RKCE_SYMM_MODE_OFB:
		return "OFB";
	case RKCE_SYMM_MODE_XTS:
		return "XTS";
	case RKCE_SYMM_MODE_CCM:
		return "CCM";
	case RKCE_SYMM_MODE_GCM:
		return "GCM";
	case RKCE_SYMM_MODE_CMAC:
		return "CMAC";
	case RKCE_SYMM_MODE_CBC_MAC:
		return "CBC_MAC";
	case RKCE_SYMM_MODE_BYPASS:
		return "BYP";
	default:
		return "UNKNOWN";
	}
}

static uint32_t rkce_td_symm_ks(void *td)
{
	struct rkce_symm_td *symm_td = (struct rkce_symm_td *)td;
	uint32_t ks = 0;

	switch (symm_td->ctrl.symm_algo) {
	case RKCE_SYMM_ALGO_AES:
		if (symm_td->ctrl.key_size == RKCE_KEY_AES_128)
			ks = RKCE_AES_KEYSIZE_128;
		else if (symm_td->ctrl.key_size == RKCE_KEY_AES_192)
			ks = RKCE_AES_KEYSIZE_192;
		else if (symm_td->ctrl.key_size == RKCE_KEY_AES_256)
			ks = RKCE_AES_KEYSIZE_256;
		else
			ks = 0;
		break;
	case RKCE_SYMM_ALGO_SM4:
		ks = RKCE_SM4_KEYSIZE;
		break;
	case RKCE_SYMM_ALGO_DES:
		ks = RKCE_DES_KEYSIZE;
		break;
	case RKCE_SYMM_ALGO_TDES:
		ks = RKCE_DES_KEYSIZE * 3;
		break;
	default:
		ks = 0;
	}

	return ks * 8;
}

static void rkce_get_single_td_len(void *td, uint32_t *src_total, uint32_t *dst_total)
{
	uint32_t i;
	uint32_t tmp_src_len = 0;
	uint32_t tmp_dst_len = 0;
	struct rkce_symm_td *symm_td = (struct rkce_symm_td *)td;

	if (!td)
		goto exit;

	for (i = 0; i < ARRAY_SIZE(symm_td->sg); i++) {
		struct rkce_sg_info *sg = &symm_td->sg[i];

		tmp_src_len += sg->src_size;
		tmp_dst_len += sg->dst_size;
	}

exit:
	if (src_total)
		*src_total = tmp_src_len;

	if (dst_total)
		*dst_total = tmp_dst_len;
}

static void rkce_dump_symm_td(void *td, uint32_t index)
{
	uint32_t i;
	struct rkce_symm_td *symm_td = (struct rkce_symm_td *)td;
	uint32_t tmp_src_len = 0, tmp_dst_len = 0;

	rk_debug("\n");
	rk_debug("symm_td(%p) index[%u]:\n", td, index);
	rk_debug("\ttask_id       = %08x\n", symm_td->task_id);
	rk_debug("\tkey_addr      = %08x\n", symm_td->key_addr);
	rk_debug("\tiv_addr       = %08x\n", symm_td->iv_addr);
	rk_debug("\tgcm_len_addr  = %08x\n", symm_td->gcm_len_addr);
	rk_debug("\ttag_addr      = %08x\n", symm_td->tag_addr);
	rk_debug("\tsymm_ctx_addr = %08x\n", symm_td->symm_ctx_addr);

	rk_debug("\tctrl: %s, %s-%u, %s, %s, %s, fpkg(%u), lpkg(%u), ksel(%u), ivl(%u), ki(%u), p(%u), int(%u)\n",
		 rkce_td_type_str(td),
		 rkce_td_symm_algo_str(td),
		 rkce_td_symm_ks(td),
		 rkce_td_symm_mode_str(td),
		 symm_td->ctrl.is_dec ? "DEC" : "ENC",
		 symm_td->ctrl.is_aad ? "AAD" : "PC",
		 symm_td->ctrl.first_pkg, symm_td->ctrl.last_pkg,
		 symm_td->ctrl.key_sel,  symm_td->ctrl.iv_len, symm_td->ctrl.is_key_inside,
		 symm_td->ctrl.is_preemptible, symm_td->ctrl.int_en);

	rkce_get_single_td_len(td, &tmp_src_len, &tmp_dst_len);

	rk_debug("\tsg: src_len = %u, dst_len = %u\n", tmp_src_len, tmp_dst_len);

	for (i = 0; i < ARRAY_SIZE(symm_td->sg); i++) {
		struct rkce_sg_info *sg = &symm_td->sg[i];

		if (sg->src_addr_h ||
		    sg->src_addr_l ||
		    sg->src_size ||
		    sg->dst_addr_h ||
		    sg->dst_addr_l ||
		    sg->dst_size)
			rk_debug("\t\tsg[%u] = 0x%08x%08x(%8u) -> 0x%08x%08x(%8u)\n",
				i, sg->src_addr_h, sg->src_addr_l, sg->src_size,
				sg->dst_addr_h, sg->dst_addr_l, sg->dst_size);
	}
	rk_debug("\tnext_task     = %08x\n", symm_td->next_task);
}

static void rkce_dump_hash_td(void *td, uint32_t index)
{
	uint32_t i;
	struct rkce_hash_td *hash_td = (struct rkce_hash_td *)td;
	uint32_t tmp_src_len = 0;

	rk_debug("\n");
	rk_debug("hash_td(%p) index[%u]:\n", td, index);
	rk_debug("\ttask_id        = %08x\n", hash_td->task_id);
	rk_debug("\tkey_addr       = %08x\n", hash_td->key_addr);
	rk_debug("\thash_addr      = %08x\n", hash_td->hash_addr);
	rk_debug("\thash_ctx_addr  = %08x\n", hash_td->hash_ctx_addr);

	rk_debug("\tctrl: %s, %s:%s, hw_pad(%u), fpkg(%u), lpkg(%u), p(%u), int(%u)\n",
		 rkce_td_type_str(td),
		 hash_td->ctrl.hmac_en ? "HMAC" : "HASH",
		 rkce_td_hash_algo_str(td),
		 hash_td->ctrl.hw_pad_en,
		 hash_td->ctrl.first_pkg, hash_td->ctrl.last_pkg,
		 hash_td->ctrl.is_preemptible, hash_td->ctrl.int_en);

	rkce_get_single_td_len(td, &tmp_src_len, NULL);

	rk_debug("\tsg: src_len = %u\n", tmp_src_len);

	for (i = 0; i < ARRAY_SIZE(hash_td->sg); i++) {
		struct rkce_sg_info *sg = &hash_td->sg[i];

		if (sg->src_addr_h ||
		    sg->src_addr_l ||
		    sg->src_size)
			rk_debug("sg[%u] = 0x%08x%08x(%-8u)\n",
				i, sg->src_addr_h, sg->src_addr_l, sg->src_size);
	}

	rk_debug("\tnext_task     = %08x\n", hash_td->next_task);
}

void rkce_dump_td(void *td)
{
	uint32_t i;
	uint32_t td_type;
	rkce_dump_td_func dump_func;
	uint32_t src_total = 0, dst_total = 0;
	struct rkce_symm_td *tmp_td = (struct rkce_symm_td *)td;

	if (!td) {
		rk_info("empty td\n");
		goto exit;
	}

	td_type = rkce_get_td_type(td);
	if (td_type >= RKCE_TD_TYPE_MAX) {
		rk_err("td_type(%u) >= %u\n", td_type, RKCE_TD_TYPE_MAX);
		goto exit;
	}

	dump_func = g_dump_td_funcs[td_type];
	if (!dump_func)
		return;

	rk_info("==============================================================================\n");

	for (i = 0; i < 1024; i++) {
		uint32_t tmp_src_len, tmp_dst_len;

		rkce_get_single_td_len(tmp_td, &tmp_src_len, &tmp_dst_len);

		src_total += tmp_src_len;
		dst_total += tmp_dst_len;

		dump_func(tmp_td, i);

		tmp_td = rkce_cma_phys2virt(tmp_td->next_task);
		if (!tmp_td)
			break;
	}

	rk_info("=================== td chain src_total = %u, dst_total = %u ===================\n",
		src_total, dst_total);
exit:
	return;
}
