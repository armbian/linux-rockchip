// SPDX-License-Identifier: GPL-2.0
/*
 * Crypto acceleration support for Rockchip crypto engine
 *
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Lin Jinhan <troy.lin@rock-chips.com>
 *
 */

#define RKCE_MODULE_TAG		"CORE"
#define RKCE_MODULE_OFFSET	2

#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "rkce_core.h"
#include "rkce_debug.h"
#include "rkce_error.h"
#include "rkce_reg.h"

struct rkce_chn_info {
	void		*td_virt;
	uint32_t	int_st;
	uint32_t	td_id;
	int		result;

	request_cb_func cb_func;
};

struct rkce_hardware {
	struct RKCE_REG		*rkce_reg;

	struct rkce_chn_info	chn[RKCE_TD_TYPE_MAX];
};

#define RST_TIMEOUT_MS		100
#define TD_PUSH_TIMEOUT_MS	3000

#define IP_VERSION_MASK		(0xfU >> 28)
#define IP_VERSION_RKCE		(0x1U >> 28)
#define GET_IP_VERSION(ver)	((ver) & IP_VERSION_MASK)

#define IS_SYMM_TD(td_type)	((td_type) == RKCE_TD_TYPE_SYMM || \
				 (td_type) == RKCE_TD_TYPE_SYMM_HASH_IN || \
				 (td_type) == RKCE_TD_TYPE_SYMM_HASH_OUT)

#define IS_HASH_TD(td_type)	((td_type) == RKCE_TD_TYPE_HASH)

#define GET_RKCE_REG(hardware) (((struct rkce_hardware *)(hardware))->rkce_reg)
#define CHECK_RKCE_INITED(hardware)   WARN_ON_ONCE(!(hardware) || \
					   !(((struct rkce_hardware *)(hardware))->rkce_reg))
#define WHILE_TIMEOUT(condition, timeout_ms) ({                        \
			int timeout = timeout_ms;                     \
			while ((condition) && timeout--) {           \
				usleep_range(1000, 2000);             \
			}                                             \
			if (timeout < 0)                              \
				rk_err("%s timeout!\n", #condition);  \
			(timeout < 0) ? -RKCE_TIMEOUT : 0;            \
		})

static const uint32_t cipher_mode2bit_mask[] = {
	[RKCE_SYMM_MODE_ECB]       = RKCE_AES_VER_ECB_FLAG_MASK,
	[RKCE_SYMM_MODE_CBC]       = RKCE_AES_VER_CBC_FLAG_MASK,
	[RKCE_SYMM_MODE_CFB]       = RKCE_AES_VER_CFB_FLAG_MASK,
	[RKCE_SYMM_MODE_OFB]       = RKCE_AES_VER_OFB_FLAG_MASK,
	[RKCE_SYMM_MODE_CTR]       = RKCE_AES_VER_CTR_FLAG_MASK,
	[RKCE_SYMM_MODE_XTS]       = RKCE_AES_VER_XTS_FLAG_MASK,
	[RKCE_SYMM_MODE_CTS]       = RKCE_AES_VER_CTS_FLAG_MASK,
	[RKCE_SYMM_MODE_CCM]       = RKCE_AES_VER_CCM_FLAG_MASK,
	[RKCE_SYMM_MODE_GCM]       = RKCE_AES_VER_GCM_FLAG_MASK,
	[RKCE_SYMM_MODE_CMAC]      = RKCE_AES_VER_CMAC_FLAG_MASK,
	[RKCE_SYMM_MODE_CBC_MAC]   = RKCE_AES_VER_CBC_MAC_FLAG_MASK,
};

static const uint32_t hash_algo2bit_mask[] = {
	[RKCE_HASH_ALGO_SHA1]       = RKCE_HASH_VER_SHA1_FLAG_MASK,
	[RKCE_HASH_ALGO_SHA224]     = RKCE_HASH_VER_SHA224_FLAG_MASK,
	[RKCE_HASH_ALGO_SHA256]     = RKCE_HASH_VER_SHA256_FLAG_MASK,
	[RKCE_HASH_ALGO_SHA384]     = RKCE_HASH_VER_SHA384_FLAG_MASK,
	[RKCE_HASH_ALGO_SHA512]     = RKCE_HASH_VER_SHA512_FLAG_MASK,
	[RKCE_HASH_ALGO_SHA512_224] = RKCE_HASH_VER_SHA512_224_FLAG_MASK,
	[RKCE_HASH_ALGO_SHA512_256] = RKCE_HASH_VER_SHA512_256_FLAG_MASK,
	[RKCE_HASH_ALGO_MD5]        = RKCE_HASH_VER_MD5_FLAG_MASK,
	[RKCE_HASH_ALGO_SM3]        = RKCE_HASH_VER_SM3_FLAG_MASK,
};

static const uint32_t hmac_algo2bit_mask[] = {
	[RKCE_HASH_ALGO_SHA1]       = RKCE_HMAC_VER_SHA1_FLAG_MASK,
	[RKCE_HASH_ALGO_SHA256]     = RKCE_HMAC_VER_SHA256_FLAG_MASK,
	[RKCE_HASH_ALGO_SHA512]     = RKCE_HMAC_VER_SHA512_FLAG_MASK,
	[RKCE_HASH_ALGO_MD5]        = RKCE_HMAC_VER_MD5_FLAG_MASK,
	[RKCE_HASH_ALGO_SM3]        = RKCE_HMAC_VER_SM3_FLAG_MASK,
};

static bool rk_is_cipher_support(struct RKCE_REG *rkce_reg,
				 uint32_t algo, uint32_t mode, uint32_t key_len)
{
	uint32_t version = 0;
	uint32_t mask = 0;
	bool key_len_valid = true;

	switch (algo) {
	case RKCE_SYMM_ALGO_DES:
	case RKCE_SYMM_ALGO_TDES:
		version = rkce_reg->DES_VER;

		if (key_len == RKCE_DES_BLOCK_SIZE)
			key_len_valid = true;
		else if (key_len == 2 * RKCE_DES_BLOCK_SIZE ||
			 key_len == 3 * RKCE_DES_BLOCK_SIZE)
			key_len_valid = version & RKCE_DES_VER_TDES_FLAG_MASK;
		else
			key_len_valid = false;
		break;
	case RKCE_SYMM_ALGO_AES:
		version = rkce_reg->AES_VER;

		if (key_len == RKCE_AES_KEYSIZE_128)
			key_len_valid = version & RKCE_AES_VER_AES128_FLAG_MASK;
		else if (key_len == RKCE_AES_KEYSIZE_192)
			key_len_valid = version & RKCE_AES_VER_AES192_FLAG_MASK;
		else if (key_len == RKCE_KEY_AES_256)
			key_len_valid = version & RKCE_AES_VER_AES256_FLAG_MASK;
		else
			key_len_valid = false;
		break;
	case RKCE_SYMM_ALGO_SM4:
		version = rkce_reg->SM4_VER;

		key_len_valid = (key_len == RKCE_SM4_KEYSIZE) ? true : false;
		break;
	default:
		return false;
	}

	mask = cipher_mode2bit_mask[mode];

	if (key_len == 0)
		key_len_valid = true;

	return (version & mask) && key_len_valid;
}

static bool rk_is_hash_support(struct RKCE_REG *rkce_reg, uint32_t algo, uint32_t type)
{
	uint32_t version = 0;
	uint32_t mask = 0;

	if (type == RKCE_ALGO_TYPE_HMAC) {
		version = rkce_reg->HMAC_VER;
		mask    = hmac_algo2bit_mask[algo];
	} else if (type == RKCE_ALGO_TYPE_HASH) {
		version = rkce_reg->HASH_VER;
		mask    = hash_algo2bit_mask[algo];
	} else {
		return false;
	}

	return version & mask;
}

static bool rk_is_asym_support(struct RKCE_REG *rkce_reg, uint32_t algo)
{
	switch (algo) {
	case RKCE_ASYM_ALGO_RSA:
		return !!rkce_reg->PKA_VER;
	case RKCE_ASYM_ALGO_ECC_P192:
	case RKCE_ASYM_ALGO_ECC_P224:
	case RKCE_ASYM_ALGO_ECC_P256:
	case RKCE_ASYM_ALGO_SM2:
		return !!rkce_reg->ECC_MAX_CURVE_WIDE;
	default:
		return false;
	}
}

bool rkce_hw_algo_valid(void *rkce_hw, uint32_t type, uint32_t algo, uint32_t mode)
{
	struct RKCE_REG *rkce_reg;

	CHECK_RKCE_INITED(rkce_hw);


	rkce_reg = GET_RKCE_REG(rkce_hw);

	if (type == RKCE_ALGO_TYPE_CIPHER || type == RKCE_ALGO_TYPE_AEAD) {
		rk_debug("CIPHER");
		return rk_is_cipher_support(rkce_reg, algo, mode, 0);
	} else if (type == RKCE_ALGO_TYPE_HASH || type == RKCE_ALGO_TYPE_HMAC) {
		rk_debug("HASH/HMAC");
		return rk_is_hash_support(rkce_reg, algo, type);
	} else if (type == RKCE_ALGO_TYPE_ASYM) {
		rk_debug("ASYM");
		return rk_is_asym_support(rkce_reg, algo);
	} else {
		return false;
	}
}

uint32_t rkce_get_td_type(void *td)
{
	if (!td)
		return ~((uint32_t)0);

	return ((struct rkce_symm_td *)td)->ctrl.td_type;
}

int rkce_soft_reset(void *rkce_hw, uint32_t reset_sel)
{
	struct RKCE_REG *rkce_reg;
	uint32_t value = 0;

	CHECK_RKCE_INITED(rkce_hw);

	rkce_reg = GET_RKCE_REG(rkce_hw);

	if (reset_sel & RKCE_RESET_SYMM)
		value |= RKCE_RST_CTL_SW_SYMM_RESET_SHIFT;

	if (reset_sel & RKCE_RESET_HASH)
		value |= RKCE_RST_CTL_SW_HASH_RESET_SHIFT;

	if (reset_sel & RKCE_RESET_PKA)
		value |= RKCE_RST_CTL_SW_PKA_RESET_SHIFT;

	rkce_reg->RST_CTL = value | RKCE_WRITE_MASK_ALL;

	return WHILE_TIMEOUT(rkce_reg->RST_CTL, RST_TIMEOUT_MS);
}

static int rkce_check_version(struct RKCE_REG *rkce_reg)
{
	rk_debug("rkce_reg->CE_VER = %08x\n", rkce_reg->CE_VER);

	if (GET_IP_VERSION(rkce_reg->CE_VER) != IP_VERSION_RKCE) {
		rk_err("IP version is %08x not a RKCE module.\n", rkce_reg->CE_VER);
		return -RKCE_FAULT;
	}

	return RKCE_SUCCESS;
}

static int rkce_init(void *rkce_hw)
{
	struct RKCE_REG *rkce_reg = GET_RKCE_REG(rkce_hw);
	uint32_t value = 0;
	int ret;

	ret = rkce_check_version(rkce_reg);
	if (ret)
		goto exit;

	rkce_soft_reset(rkce_hw, RKCE_RESET_SYMM | RKCE_RESET_HASH | RKCE_RESET_PKA);

	/* clear symm interrupt register */
	rkce_reg->SYMM_INT_EN = 0;
	value = rkce_reg->SYMM_INT_ST;
	rkce_reg->SYMM_INT_ST = value;

	ret = WHILE_TIMEOUT(rkce_reg->SYMM_INT_ST, RST_TIMEOUT_MS);
	if (ret)
		goto exit;

	/* clear hash interrupt register */
	rkce_reg->HASH_INT_EN = 0;
	value = rkce_reg->HASH_INT_ST;
	rkce_reg->HASH_INT_ST = value;

	ret = WHILE_TIMEOUT(rkce_reg->HASH_INT_ST, RST_TIMEOUT_MS);
	if (ret)
		goto exit;

	if (rkce_reg->SYMM_CONTEXT_SIZE != RKCE_TD_SYMM_CTX_SIZE) {
		rk_err("rkce symm context size (%u) != %u\n",
		       rkce_reg->SYMM_CONTEXT_SIZE, RKCE_TD_SYMM_CTX_SIZE);
		return -RKCE_INVAL;
	}

	if (rkce_reg->HASH_CONTEXT_SIZE != RKCE_TD_HASH_CTX_SIZE) {
		rk_err("rkce hash context size (%u) != %u\n",
		       rkce_reg->HASH_CONTEXT_SIZE, RKCE_TD_HASH_CTX_SIZE);
		return -RKCE_INVAL;
	}

exit:
	return ret;
}

void *rkce_hardware_alloc(void __iomem *reg_base)
{
	struct rkce_hardware *hardware;

	rk_debug("reg_base = %p", reg_base);

	if (!reg_base)
		return NULL;

	hardware = kzalloc(sizeof(*hardware), GFP_KERNEL);
	if (!hardware)
		return NULL;

	hardware->rkce_reg = reg_base;

	if (rkce_init(hardware) != 0) {
		kfree(hardware);
		return NULL;
	}

	rk_debug("hardware = %p", hardware);

	return hardware;
}

void rkce_hardware_free(void *rkce_hw)
{
	if (!rkce_hw)
		return;

	kfree(rkce_hw);
}

void rkce_dump_reginfo(void *rkce_hw)
{
	struct RKCE_REG *rkce_reg;

	CHECK_RKCE_INITED(rkce_hw);

	rkce_reg = GET_RKCE_REG(rkce_hw);

	rk_info("\n============================== reg info ===========================\n");
	rk_info("FIFO_ST           = %08x\n", rkce_reg->FIFO_ST);
	rk_info("\n");
	rk_info("SYMM_INT_EN       = %08x\n", rkce_reg->SYMM_INT_EN);
	rk_info("SYMM_INT_ST       = %08x\n", rkce_reg->SYMM_INT_ST);
	rk_info("SYMM_TD_ST        = %08x\n", rkce_reg->SYMM_TD_ST);
	rk_info("SYMM_TD_ID        = %08x\n", rkce_reg->SYMM_TD_ID);
	rk_info("SYMM_ST_DBG       = %08x\n", rkce_reg->SYMM_ST_DBG);
	rk_info("SYMM_TD_ADDR_DBG  = %08x\n", rkce_reg->SYMM_TD_ADDR_DBG);
	rk_info("SYMM_TD_GRANT_DBG = %08x\n", rkce_reg->SYMM_TD_GRANT_DBG);
	rk_info("\n");
	rk_info("HASH_INT_EN       = %08x\n", rkce_reg->HASH_INT_EN);
	rk_info("HASH_INT_ST       = %08x\n", rkce_reg->HASH_INT_ST);
	rk_info("HASH_TD_ST        = %08x\n", rkce_reg->HASH_TD_ST);
	rk_info("HASH_TD_ID        = %08x\n", rkce_reg->HASH_TD_ID);
	rk_info("HASH_ST_DBG       = %08x\n", rkce_reg->HASH_ST_DBG);
	rk_info("HASH_TD_ADDR_DBG  = %08x\n", rkce_reg->HASH_TD_ADDR_DBG);
	rk_info("HASH_TD_GRANT_DBG = %08x\n", rkce_reg->HASH_TD_GRANT_DBG);
	rk_info("===================================================================\n");
}

int rkce_push_td(void *rkce_hw, void *td)
{
	int ret = RKCE_SUCCESS;
	struct RKCE_REG *rkce_reg;
	uint32_t td_type;
	struct rkce_hardware *hardware = rkce_hw;

	CHECK_RKCE_INITED(rkce_hw);

	if (!td)
		return -RKCE_INVAL;

	td_type  = rkce_get_td_type(td);
	rkce_reg = GET_RKCE_REG(rkce_hw);

	rkce_dump_td(td);

	if (IS_SYMM_TD(td_type)) {
		rk_debug("rkce symm push td virt(%p), phys(%08x)\n",
			 td, rkce_cma_virt2phys(td));

		WRITE_ONCE(rkce_reg->SYMM_INT_EN, 0x3f);

		/* wait symm fifo valid */
		ret = WHILE_TIMEOUT(rkce_reg->TD_LOAD_CTRL & RKCE_TD_LOAD_CTRL_SYMM_TLR_MASK,
				    TD_PUSH_TIMEOUT_MS);
		if (ret)
			goto exit;

		/* set task desc address */
		rkce_reg->TD_ADDR = rkce_cma_virt2phys(td);
		hardware->chn[RKCE_TD_TYPE_SYMM].td_virt = td;

		/* tell rkce to load task desc address as symm td */
		rkce_reg->TD_LOAD_CTRL = 0xffff0000 | RKCE_TD_LOAD_CTRL_SYMM_TLR_MASK;
	} else if (IS_HASH_TD(td_type)) {
		rk_debug("rkce hash push td virt(%p), phys(%08x)\n",
			 td, rkce_cma_virt2phys(td));

		WRITE_ONCE(rkce_reg->HASH_INT_EN, 0x3f);

		/* wait hash fifo valid */
		ret = WHILE_TIMEOUT(rkce_reg->TD_LOAD_CTRL & RKCE_TD_LOAD_CTRL_HASH_TLR_MASK,
				    TD_PUSH_TIMEOUT_MS);
		if (ret)
			goto exit;

		/* set task desc address */
		rkce_reg->TD_ADDR = rkce_cma_virt2phys(td);
		hardware->chn[RKCE_TD_TYPE_HASH].td_virt = td;

		/* tell rkce to load task desc address as hash td */
		rkce_reg->TD_LOAD_CTRL = 0xffff0000 | RKCE_TD_LOAD_CTRL_HASH_TLR_MASK;
	} else {
		return -RKCE_INVAL;
	}

exit:
	return ret;
}

int rkce_push_td_sync(void *rkce_hw, void *td, uint32_t timeout_ms)
{
	int ret = RKCE_SUCCESS;
	struct RKCE_REG *rkce_reg;
	uint32_t td_type;
	uint32_t value, mask;

	CHECK_RKCE_INITED(rkce_hw);

	if (!td)
		return -RKCE_INVAL;

	td_type  = rkce_get_td_type(td);
	rkce_reg = GET_RKCE_REG(rkce_hw);

	rkce_dump_td(td);

	if (IS_SYMM_TD(td_type)) {
		rk_debug("rkce symm push td virt(%p), phys(%08x)\n",
			 td, rkce_cma_virt2phys(td));

		WRITE_ONCE(rkce_reg->SYMM_INT_EN, 0x00);

		/* wait symm fifo valid */
		ret = WHILE_TIMEOUT(rkce_reg->TD_LOAD_CTRL & RKCE_TD_LOAD_CTRL_SYMM_TLR_MASK,
				    timeout_ms);
		if (ret)
			goto exit;

		/* set task desc address */
		rkce_reg->TD_ADDR = rkce_cma_virt2phys(td);

		/* tell rkce to load task desc address as symm td */
		rkce_reg->TD_LOAD_CTRL = 0xffff0000 | RKCE_TD_LOAD_CTRL_SYMM_TLR_MASK;

		/* wait symm done */
		ret = WHILE_TIMEOUT(!(rkce_reg->SYMM_INT_ST), timeout_ms);
		mask  = RKCE_SYMM_INT_ST_TD_DONE_MASK;
		value = READ_ONCE(rkce_reg->SYMM_INT_ST);
		WRITE_ONCE(rkce_reg->SYMM_INT_ST, value);
		rk_debug("symm ret = %d, value = %08x, IN_ST = %08x\n",
			 ret, value, READ_ONCE(rkce_reg->SYMM_INT_ST));
	} else if (IS_HASH_TD(td_type)) {
		rk_debug("rkce hash push td virt(%p), phys(%08x)\n",
			 td, rkce_cma_virt2phys(td));

		WRITE_ONCE(rkce_reg->HASH_INT_EN, 0x00);

		/* wait hash fifo valid */
		ret = WHILE_TIMEOUT(rkce_reg->TD_LOAD_CTRL & RKCE_TD_LOAD_CTRL_HASH_TLR_MASK,
				    timeout_ms);
		if (ret)
			goto exit;

		/* set task desc address */
		rkce_reg->TD_ADDR = rkce_cma_virt2phys(td);

		/* tell rkce to load task desc address as hash td */
		rkce_reg->TD_LOAD_CTRL = 0xffff0000 | RKCE_TD_LOAD_CTRL_HASH_TLR_MASK;

		/* wait hash done */
		ret = WHILE_TIMEOUT(!(rkce_reg->HASH_INT_ST), timeout_ms);
		mask  = RKCE_HASH_INT_ST_TD_DONE_MASK;
		value = READ_ONCE(rkce_reg->HASH_INT_ST);
		WRITE_ONCE(rkce_reg->HASH_INT_ST, value);
		rk_debug("hash ret = %d, value = %08x, INT_ST = %08x\n",
			 ret, value, READ_ONCE(rkce_reg->HASH_INT_ST));
	} else {
		rk_debug("unknown td_type = %u\n", td_type);
		return -RKCE_INVAL;
	}

	if (ret)
		goto exit;

	ret = (value == mask) ? 0 : -RKCE_FAULT;
exit:
	return ret;
}

int rkce_init_symm_td(struct rkce_symm_td *td, struct rkce_symm_td_buf *buf)
{
	if (!td ||
	    !buf ||
	    !rkce_cma_virt2phys(td) ||
	    !rkce_cma_virt2phys(buf)) {
		rk_debug("td = %p buf = %p", td, buf);
		return -RKCE_INVAL;
	}

	memset(td, 0x00, sizeof(*td));

	td->ctrl.td_type  = RKCE_TD_TYPE_SYMM;
	td->task_id       = rkce_cma_virt2phys(buf);
	td->key_addr      = rkce_cma_virt2phys(buf->key1);
	td->iv_addr       = rkce_cma_virt2phys(buf->iv);
	td->gcm_len_addr  = rkce_cma_virt2phys(&buf->gcm_len);
	td->tag_addr      = rkce_cma_virt2phys(buf->tag);
	td->symm_ctx_addr = rkce_cma_virt2phys(buf->ctx);

	return RKCE_SUCCESS;
}

int rkce_init_hash_td(struct rkce_hash_td *td, struct rkce_hash_td_buf *buf)
{
	if (!td ||
	    !buf ||
	    !rkce_cma_virt2phys(td) ||
	    !rkce_cma_virt2phys(buf)) {
		rk_debug("td = %p buf = %p", td, buf);
		return -RKCE_INVAL;
	}

	memset(td, 0x00, sizeof(*td));

	td->ctrl.td_type  = RKCE_TD_TYPE_HASH;
	td->task_id       = rkce_cma_virt2phys(buf);
	td->key_addr      = rkce_cma_virt2phys(buf->key);
	td->hash_addr     = rkce_cma_virt2phys(buf->hash);
	td->hash_ctx_addr = rkce_cma_virt2phys(buf->ctx);

	return RKCE_SUCCESS;
}

int rkce_irq_callback_set(void *rkce_hw, enum rkce_td_type td_type, request_cb_func cb_func)
{
	struct rkce_hardware *hardware = rkce_hw;

	CHECK_RKCE_INITED(rkce_hw);

	if (!cb_func)
		return -RKCE_INVAL;

	if (td_type == RKCE_TD_TYPE_SYMM)
		hardware->chn[RKCE_TD_TYPE_SYMM].cb_func = cb_func;
	else if (td_type == RKCE_TD_TYPE_HASH)
		hardware->chn[RKCE_TD_TYPE_HASH].cb_func = cb_func;
	else
		return -RKCE_INVAL;

	return RKCE_SUCCESS;
}

void rkce_irq_handler(void *rkce_hw)
{
	struct rkce_chn_info *cur_chn;
	struct RKCE_REG *rkce_reg;
	struct rkce_hardware *hardware = rkce_hw;

	CHECK_RKCE_INITED(rkce_hw);

	rkce_reg = GET_RKCE_REG(rkce_hw);

	if (rkce_reg->SYMM_INT_ST) {
		cur_chn = &(hardware->chn[RKCE_TD_TYPE_SYMM]);
		cur_chn->int_st =  READ_ONCE(rkce_reg->SYMM_INT_ST);
		cur_chn->td_id  = rkce_reg->SYMM_TD_ID;

		/* clear symm int */
		WRITE_ONCE(rkce_reg->SYMM_INT_ST, cur_chn->int_st);

		cur_chn->result = (cur_chn->int_st == RKCE_SYMM_INT_ST_TD_DONE_MASK) ?
				  RKCE_SUCCESS : cur_chn->int_st;
	}

	if (rkce_reg->HASH_INT_ST) {
		cur_chn = &(hardware->chn[RKCE_TD_TYPE_HASH]);
		cur_chn->int_st = READ_ONCE(rkce_reg->HASH_INT_ST);
		cur_chn->td_id  = rkce_reg->HASH_TD_ID;

		/* clear hash int */
		WRITE_ONCE(rkce_reg->HASH_INT_ST, cur_chn->int_st);

		cur_chn->result = (cur_chn->int_st == RKCE_HASH_INT_ST_TD_DONE_MASK) ?
				  RKCE_SUCCESS : cur_chn->int_st;
	}
}

void rkce_irq_thread(void *rkce_hw)
{
	uint32_t i;
	bool is_fault = false;
	struct rkce_hardware *hardware = rkce_hw;

	CHECK_RKCE_INITED(rkce_hw);

	for (i = 0; i < ARRAY_SIZE(hardware->chn); i++) {
		struct rkce_chn_info *cur_chn = &(hardware->chn[i]);

		if (cur_chn->result) {
			is_fault = true;
			rk_err("td_type = %u, wrong SISR = %08x, td_id = %08x, td_virt = %p\n",
				i, cur_chn->int_st, cur_chn->td_id, cur_chn->td_virt);
		}

		if (cur_chn->int_st == 0 || !(cur_chn->cb_func))
			continue;

		rk_debug("##################### finalize td %p, result = %d\n",
			 cur_chn->td_virt, cur_chn->result);

		if (cur_chn->cb_func && cur_chn->td_virt)
			cur_chn->cb_func(cur_chn->result, cur_chn->td_id, cur_chn->td_virt);

		cur_chn->result  = 0;
		cur_chn->int_st  = 0;
		cur_chn->td_id   = 0;
		cur_chn->td_virt = NULL;
	}

	if (is_fault)
		rkce_dump_reginfo(hardware);
}
