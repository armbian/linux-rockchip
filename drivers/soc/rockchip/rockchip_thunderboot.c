// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 */
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/soc/rockchip/rockchip_decompress.h>
#include <linux/soc/rockchip/rockchip_thunderboot.h>
#include <linux/soc/rockchip/rockchip_thunderboot_crypto.h>

static DECLARE_WAIT_QUEUE_HEAD(wait_ramdiskc);
static atomic_t ramdiskc_done = ATOMIC_INIT(0);

int rk_tb_prepare_ramdisk_decompress(struct device *dev)
{
	int ret = 0;
	struct resource src, dst;
	struct device_node *rds, *rdd;

	rds = of_parse_phandle(dev->of_node, "memory-region-src", 0);
	if (!rds) {
		dev_err(dev, "missing \"memory-region-src\" property\n");
		return -ENODEV;
	}

	rdd = of_parse_phandle(dev->of_node, "memory-region-dst", 0);
	if (!rdd) {
		dev_err(dev, "missing \"memory-region-dst\" property\n");
		of_node_put(rds);
		return -ENODEV;
	}

	/* Parse ramdisk addr and help start decompressing */
	if (of_address_to_resource(rds, 0, &src) >= 0 &&
	    of_address_to_resource(rdd, 0, &dst) >= 0) {
		if (IS_ENABLED(CONFIG_ROCKCHIP_THUNDER_BOOT_CRYPTO)) {
			const u32 *digest_org;
			u32 rdk_size = 0;

			of_property_read_u32(rds, "size", &rdk_size);
			digest_org = of_get_property(rds->child, "value", NULL);
			if (digest_org && rdk_size)
				rk_tb_sha256((dma_addr_t)src.start, rdk_size,
					     (void *)digest_org);
		}
		/*
		 * Decompress HW driver will free reserved area of
		 * memory-region-src.
		 */
		ret = rk_decom_start(GZIP_MOD, src.start,
				     dst.start,
				     resource_size(&dst));
		if (ret < 0)
			dev_err(dev, "failed to start decom\n");
	}

	of_node_put(rds);
	of_node_put(rdd);

	return ret;
}

void rk_tb_ramdisk_compress_done(void)
{
	atomic_set(&ramdiskc_done, 1);
	wake_up(&wait_ramdiskc);
}
EXPORT_SYMBOL(rk_tb_ramdisk_compress_done);

int rk_tb_wait_ramdisk_compress_done(u32 timeout_ms)
{
	return wait_event_timeout(wait_ramdiskc, atomic_read(&ramdiskc_done),
				  msecs_to_jiffies(timeout_ms));
}
EXPORT_SYMBOL(rk_tb_wait_ramdisk_compress_done);
