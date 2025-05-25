// SPDX-License-Identifier: GPL-2.0
/*
 * Crypto acceleration support for Rockchip RKCE crypto
 *
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Lin Jinhan <troy.lin@rock-chips.com>
 *
 */

#define RKCE_MODULE_TAG		"DEV"
#define RKCE_MODULE_OFFSET	4

#include <linux/clk.h>
#include <linux/crypto.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <crypto/scatterwalk.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "rkce_akcipher.h"
#include "rkce_buf.h"
#include "rkce_core.h"
#include "rkce_debug.h"
#include "rkce_dev.h"
#include "rkce_hash.h"
#include "rkce_skcipher.h"
#include "rkce_monitor.h"
#include "../cryptodev_linux/rk_cryptodev.h"

#define CRYPTO_NAME	"RKCE"

module_param_named(debug_level, rkce_debug_level, ulong, 0644);
MODULE_PARM_DESC(rkce_debug_level, "debug level | DBEUG | CORE | DEV | CIPHER | HASH | BUF | (0-3)");

static irqreturn_t rkce_dev_irq_handle(int irq, void *dev_id)
{
	struct rkce_dev *rk_dev = platform_get_drvdata(dev_id);

	rk_trace("enter.\n");

	if (!rk_dev || !rk_dev->hardware)
		return IRQ_HANDLED;

	rkce_irq_handler(rk_dev->hardware);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t rkce_dev_irq_thread(int irq, void *dev_id)
{
	struct rkce_dev *rk_dev = platform_get_drvdata(dev_id);

	rk_trace("enter.\n");

	if (!rk_dev || !rk_dev->hardware)
		goto exit;

	rkce_irq_thread(rk_dev->hardware);

exit:
	return IRQ_HANDLED;
}

int rkce_enable_clk(struct rkce_dev *rk_dev)
{
	int ret;

	rk_trace("clk_bulk_prepare_enable.\n");

	ret = clk_bulk_prepare_enable(rk_dev->clks_num, rk_dev->clk_bulks);
	if (ret < 0)
		rk_err("failed to enable clks %d\n", ret);

	return ret;
}

void rkce_disable_clk(struct rkce_dev *rk_dev)
{
	rk_trace("clk_bulk_disable_unprepare.\n");

	clk_bulk_disable_unprepare(rk_dev->clks_num, rk_dev->clk_bulks);
}

static struct rkce_algt *g_rkce_algs[] = {
	&rkce_ecb_sm4_alg,
	&rkce_cbc_sm4_alg,
	&rkce_xts_sm4_alg,
	&rkce_cfb_sm4_alg,
	&rkce_ofb_sm4_alg,
	&rkce_ctr_sm4_alg,
	&rkce_gcm_sm4_alg,

	&rkce_ecb_aes_alg,
	&rkce_cbc_aes_alg,
	&rkce_xts_aes_alg,
	&rkce_cfb_aes_alg,
	&rkce_ofb_aes_alg,
	&rkce_ctr_aes_alg,
	&rkce_gcm_aes_alg,

	&rkce_ecb_des_alg,
	&rkce_cbc_des_alg,
	&rkce_cfb_des_alg,
	&rkce_ofb_des_alg,

	&rkce_ecb_des3_ede_alg,
	&rkce_cbc_des3_ede_alg,
	&rkce_cfb_des3_ede_alg,
	&rkce_ofb_des3_ede_alg,

	&rkce_ahash_sha1,
	&rkce_ahash_sha224,
	&rkce_ahash_sha256,
	&rkce_ahash_sha384,
	&rkce_ahash_sha512,
	&rkce_ahash_md5,
	&rkce_ahash_sm3,

	&rkce_hmac_md5,
	&rkce_hmac_sha1,
	&rkce_hmac_sha256,
	&rkce_hmac_sha512,
	&rkce_hmac_sm3,

	&rkce_asym_rsa,
	&rkce_asym_sm2,
	&rkce_asym_ecc_p192,
	&rkce_asym_ecc_p224,
	&rkce_asym_ecc_p256,
};

static int rkce_crypto_register(struct rkce_dev *rk_dev, struct rkce_algt **algts, int algt_num)
{
	unsigned int i, k;
	int err = 0;

	rkce_enable_clk(rk_dev);

	for (i = 0; i < algt_num; i++) {
		struct rkce_algt *cur_algt = algts[i];

		if (!rkce_hw_algo_valid(rk_dev->hardware, cur_algt->type,
					cur_algt->algo, cur_algt->mode))
			continue;

		cur_algt->rk_dev     = rk_dev;
		cur_algt->valid_flag = true;

		if (cur_algt->type == RKCE_ALGO_TYPE_CIPHER) {
			if (cur_algt->mode == RKCE_SYMM_MODE_CTR ||
			    cur_algt->mode == RKCE_SYMM_MODE_CFB ||
			    cur_algt->mode == RKCE_SYMM_MODE_OFB)
				cur_algt->alg.cipher.base.cra_blocksize = 1;

			if (cur_algt->mode == RKCE_SYMM_MODE_ECB)
				cur_algt->alg.cipher.ivsize = 0;

			err = crypto_register_skcipher(&cur_algt->alg.cipher);
		} else if (cur_algt->type == RKCE_ALGO_TYPE_HASH ||
			   cur_algt->type == RKCE_ALGO_TYPE_HMAC) {
			err = crypto_register_ahash(&cur_algt->alg.hash);
		} else if (cur_algt->type == RKCE_ALGO_TYPE_AEAD) {
			err = crypto_register_aead(&cur_algt->alg.aead);
		} else if (cur_algt->type == RKCE_ALGO_TYPE_ASYM) {
			err = crypto_register_akcipher(&cur_algt->alg.asym);
		} else {
			continue;
		}

		if (err) {
			rk_err("crypto register %s failed.\n", cur_algt->name);
			goto err_cipher_algs;
		}

		rk_debug("register algo %s success.\n", cur_algt->name);
	}

	rkce_disable_clk(rk_dev);

	return 0;

err_cipher_algs:
	for (k = 0; k < i; k++) {
		struct rkce_algt *cur_algt = algts[i];

		if (cur_algt->type == RKCE_ALGO_TYPE_CIPHER)
			crypto_unregister_skcipher(&cur_algt->alg.cipher);
		else if (cur_algt->type == RKCE_ALGO_TYPE_HASH ||
			 cur_algt->type == RKCE_ALGO_TYPE_HMAC)
			crypto_unregister_ahash(&cur_algt->alg.hash);
		else if (cur_algt->type == RKCE_ALGO_TYPE_AEAD)
			crypto_unregister_aead(&cur_algt->alg.aead);
		else if (cur_algt->type == RKCE_ALGO_TYPE_ASYM)
			crypto_unregister_akcipher(&cur_algt->alg.asym);
		else
			continue;
	}

	rkce_disable_clk(rk_dev);

	return err;
}

static void rkce_crypto_unregister(struct rkce_dev *rk_dev, struct rkce_algt **algts, int algt_num)
{
	int i;

	rkce_enable_clk(rk_dev);

	for (i = 0; i < algt_num; i++) {
		struct rkce_algt *cur_algt = algts[i];

		if (cur_algt->type == RKCE_ALGO_TYPE_CIPHER)
			crypto_unregister_skcipher(&cur_algt->alg.cipher);
		else if (cur_algt->type == RKCE_ALGO_TYPE_HASH ||
			 cur_algt->type == RKCE_ALGO_TYPE_HMAC)
			crypto_unregister_ahash(&cur_algt->alg.hash);
		else if (cur_algt->type == RKCE_ALGO_TYPE_AEAD)
			crypto_unregister_aead(&cur_algt->alg.aead);
		else if (cur_algt->type == RKCE_ALGO_TYPE_ASYM)
			crypto_unregister_akcipher(&cur_algt->alg.asym);
		else
			continue;
	}

	rkce_disable_clk(rk_dev);
}

static const struct of_device_id rkce_of_id_table[] = {
	/* crypto rkce in below */
	{
		.compatible = "rockchip,crypto-ce",
	},

	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, rkce_of_id_table);

static int rkce_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct rkce_dev *rk_dev;
	int err = 0;

	rk_dev = devm_kzalloc(&pdev->dev, sizeof(*rk_dev), GFP_KERNEL);
	if (!rk_dev)
		return -ENOMEM;

	spin_lock_init(&rk_dev->lock);

	/* get crypto base */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rk_dev->reg = devm_ioremap_resource(dev, res);
	if (IS_ERR(rk_dev->reg)) {
		err = PTR_ERR(rk_dev->reg);
		goto err_crypto;
	}

	rk_dev->clks_num = devm_clk_bulk_get_all(dev, &rk_dev->clk_bulks);
	if (rk_dev->clks_num < 0) {
		err = rk_dev->clks_num;
		rk_err("failed to get clks property\n");
		goto err_crypto;
	}

	rk_dev->irq = platform_get_irq(pdev, 0);
	if (rk_dev->irq < 0) {
		rk_err("control Interrupt is not available.\n");
		err = rk_dev->irq;
		goto err_crypto;
	}

	err = devm_request_threaded_irq(dev, rk_dev->irq, rkce_dev_irq_handle,
					rkce_dev_irq_thread, IRQF_ONESHOT,
					dev_name(dev), pdev);
	if (err) {
		rk_err("irq request failed.\n");
		goto err_crypto;
	}

	disable_irq(rk_dev->irq);

	err = rkce_cma_init(dev);
	if (err) {
		rk_err("rkce_cma_init failed.\n");
		goto err_crypto;
	}

	rk_dev->hardware = rkce_hardware_alloc(rk_dev->reg);
	if (!rk_dev->hardware) {
		err = -EFAULT;
		rk_err("rkce_hardware_alloc failed.\n");
		goto err_crypto;
	}

	rk_dev->dev = dev;

	err = rkce_crypto_register(rk_dev, g_rkce_algs, ARRAY_SIZE(g_rkce_algs));
	if (err) {
		rk_err("rkce_crypto_register failed.\n");
		goto err_crypto;
	}

	platform_set_drvdata(pdev, rk_dev);

	rkce_monitor_init();

	rkce_irq_callback_set(rk_dev->hardware, RKCE_TD_TYPE_SYMM, rkce_cipher_request_callback);
	rkce_irq_callback_set(rk_dev->hardware, RKCE_TD_TYPE_HASH, rkce_hash_request_callback);

	rk_dev->symm_engine = crypto_engine_alloc_init(&pdev->dev, true);
	crypto_engine_start(rk_dev->symm_engine);

	rk_dev->hash_engine = crypto_engine_alloc_init(&pdev->dev, true);
	crypto_engine_start(rk_dev->hash_engine);

	rk_debug("symm_engine = %p hash_engine = %p",
		 rk_dev->symm_engine, rk_dev->hash_engine);

	rk_cryptodev_register_dev(dev, "RKCE multi");

	enable_irq(rk_dev->irq);

	dev_info(dev, "%s Accelerator successfully registered\n", CRYPTO_NAME);

	return 0;

err_crypto:
	rkce_hardware_free(rk_dev->hardware);
	rk_dev->hardware = NULL;
	return err;
}

static int rkce_remove(struct platform_device *pdev)
{
	struct rkce_dev *rk_dev = platform_get_drvdata(pdev);

	if (!rk_dev)
		return 0;

	crypto_engine_exit(rk_dev->symm_engine);
	crypto_engine_exit(rk_dev->hash_engine);

	rkce_monitor_deinit();

	rkce_crypto_unregister(rk_dev, g_rkce_algs, ARRAY_SIZE(g_rkce_algs));

	rkce_hardware_free(rk_dev->hardware);

	rkce_cma_deinit(rk_dev->dev);

	rkce_disable_clk(rk_dev);

	return 0;
}

static struct platform_driver rkce_driver = {
	.probe		= rkce_probe,
	.remove		= rkce_remove,
	.driver		= {
		.name	= CRYPTO_NAME,
		.of_match_table	= rkce_of_id_table,
	},
};

module_platform_driver(rkce_driver);

MODULE_AUTHOR("Lin Jinhan <troy.lin@rock-chips.com>");
MODULE_DESCRIPTION("Support for Rockchip's RKCE cryptographic engine");
MODULE_LICENSE("GPL");
