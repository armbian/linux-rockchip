// SPDX-License-Identifier: GPL-2.0
/*
 * rk2116-spi.c  --  RK2116 ALSA SoC Audio driver
 *
 */

// #define DEBUG 1
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/spi/spi.h>
#include <sound/soc.h>

#include "rk2116.h"

static int rk2116_spi_bus_write(void *context, const void *data, size_t count)
{
	int ret = 0;
	struct device *dev = context;
	struct spi_device *spi = to_spi_device(dev);
	struct spi_message m;
	struct spi_transfer write_cmd_packet = {
		.tx_buf	= data,
		.len	= count,
	};

	spi_message_init(&m);
	spi_message_add_tail(&write_cmd_packet, &m);
	ret = spi_sync(spi, &m);
	return ret;
}

static int rk2116_spi_bus_read(void *context,
				const void *reg,
				size_t reg_size,
				void *val,
				size_t val_size)
{
	return -EIO;
}

static const struct regmap_bus rk2116_regmap_bus_spi = {
	.write = rk2116_spi_bus_write,
	.read = rk2116_spi_bus_read,
	.reg_format_endian_default = REGMAP_ENDIAN_NATIVE,
	.val_format_endian_default = REGMAP_ENDIAN_NATIVE,
};

static const struct of_device_id rk2116_of_match[] = {
	{ .compatible = "rockchip,rk2116", },
	{ }
};
MODULE_DEVICE_TABLE(of, rk2116_of_match);

static int rk2116_spi_probe(struct spi_device *spi)
{
	struct regmap *regmap;

	regmap = devm_regmap_init(&spi->dev,
				&rk2116_regmap_bus_spi,
				&spi->dev,
				&rk2116_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	return rk2116_probe(&spi->dev, regmap);
}

static void rk2116_spi_remove(struct spi_device *spi)
{
	rk2116_remove(&spi->dev);
}

static const struct spi_device_id rk2116_spi_id_table[] = {
	{ "rk2116", 0 },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(spi, rk2116_spi_id_table);

static struct spi_driver rk2116_spi_driver = {
	.probe      = rk2116_spi_probe,
	.remove     = rk2116_spi_remove,
	.id_table   = rk2116_spi_id_table,
	.driver = {
		.name		= "rk2116",
		.of_match_table	= rk2116_of_match,
	},
};
module_spi_driver(rk2116_spi_driver);

MODULE_DESCRIPTION("ASoC RK2116 audio CODEC SPI driver");
MODULE_AUTHOR("Looper Zheng <looper.zheng@rockchip.com>");
MODULE_LICENSE("GPL");
