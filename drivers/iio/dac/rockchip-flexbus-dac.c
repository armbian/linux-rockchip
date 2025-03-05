// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Driver for Rockchip Flexbus DAC opmode
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/iio/buffer.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>

#include <dt-bindings/mfd/rockchip-flexbus.h>
#include <linux/mfd/rockchip-flexbus.h>

#define FLEXBUS_MAX_TX_RATE	200000000
#define FLEXBUS_DAC_ERR_ISR	(FLEXBUS_DMA_TIMEOUT_ISR | FLEXBUS_DMA_ERR_ISR |	\
				 FLEXBUS_TX_UDF_ISR | FLEXBUS_TX_OVF_ISR)
#define FLEXBUS_DAC_ISR		(FLEXBUS_DAC_ERR_ISR | FLEXBUS_TX_DONE_ISR)
#define FLEXBUS_DAC_TIMEOUT	msecs_to_jiffies(100)
#define FLEXBUS_DAC_REPEAT	0x40

enum flexbus_dac_result {
	DAC_DONE = 0,
	DAC_ERR,
};

struct rockchip_flexbus_dac;

struct rockchip_flexbus_dac_ops {
	int (*write_block)(struct rockchip_flexbus_dac *rkfb_dac, void *src, dma_addr_t src_phys,
			   u32 src_len, u32 num_of_dfs);
};

struct rockchip_flexbus_dac {
	struct device		*dev;
	struct rockchip_flexbus	*rkfb;
	u32			dfs;
	bool			free_sclk;
	bool			cpol;
	bool			cpha;
	struct mutex		lock;
	struct completion	completion;
	enum flexbus_dac_result	result;
	u32			last_val;
	u32			src_buf_len;
	dma_addr_t		src_buf_phys;
	void			*src_buf;
	const struct rockchip_flexbus_dac_ops	*ops;
};

static const struct iio_chan_spec rockchip_flexbus_dac_channel[] = {
	{
		.type = IIO_VOLTAGE,
		.output = 1,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SAMP_FREQ),
		.scan_type = {
			.sign = 'u',
			.realbits = 16,
			.storagebits = 16,
			.shift = 0,
			.repeat = FLEXBUS_DAC_REPEAT,
			.endianness = IIO_CPU,
		},
	},
};

static u16 rockchip_flexbus_get_last_val(struct rockchip_flexbus_dac *rkfb_dac, void *src,
					 u32 num_of_dfs)
{
	u32 index = num_of_dfs - 1;
	u16 val;

	switch (rkfb_dac->dfs) {
	case 4:
		val = readb(src + index * rkfb_dac->dfs / 8);
		if (index % 2)
			val >>= 4;
		val &= 0xf;
		break;
	case 8:
		val = readb(src + index * rkfb_dac->dfs / 8);
		break;
	case 16:
	default:
		val = readw(src + index * rkfb_dac->dfs / 8);
		break;
	}

	return val;
}

static int rockchip_flexbus_dac_write_block(struct rockchip_flexbus_dac *rkfb_dac, void *src,
					    dma_addr_t src_phys, u32 src_len, u32 num_of_dfs)
{
	struct rockchip_flexbus *rkfb = rkfb_dac->rkfb;
	u32 val;
	int ret = 0;

	if (num_of_dfs > 0x08000000) {
		dev_err(rkfb_dac->dev, "num_of_dfs is too large\n");
		return -EINVAL;
	}

	mutex_lock(&rkfb_dac->lock);
	reinit_completion(&rkfb_dac->completion);

	rockchip_flexbus_writel(rkfb, FLEXBUS_TX_NUM, num_of_dfs);

	val = num_of_dfs * rkfb_dac->dfs / 32 / 2;
	if (val < 16)
		val = 16;
	else if (val > rkfb->config->txwat_start_max)
		val = rkfb->config->txwat_start_max;
	rockchip_flexbus_writel(rkfb, FLEXBUS_TXWAT_START, val);

	rockchip_flexbus_writel(rkfb, FLEXBUS_DMA_SRC_ADDR0, (ulong)src_phys >> 2);
	rockchip_flexbus_writel(rkfb, FLEXBUS_DMA_SRC_LEN0, src_len);

	rockchip_flexbus_writel(rkfb, FLEXBUS_ENR, FLEXBUS_TX_ENR);
	if (!wait_for_completion_timeout(&rkfb_dac->completion, FLEXBUS_DAC_TIMEOUT)) {
		ret = -ETIMEDOUT;
		goto end;
	}

	if (rkfb_dac->result != DAC_DONE) {
		ret = -1;
		goto end;
	}

	rkfb_dac->last_val = rockchip_flexbus_get_last_val(rkfb_dac, src, num_of_dfs);

end:
	rockchip_flexbus_writel(rkfb, FLEXBUS_ENR, FLEXBUS_TX_DIS);
	mutex_unlock(&rkfb_dac->lock);

	return ret;
}

static void rockchip_flexbus_dac_isr(struct rockchip_flexbus *rkfb, u32 isr)
{
	struct rockchip_flexbus_dac *rkfb_dac = rkfb->fb0_data;

	if (rkfb->opmode0 != ROCKCHIP_FLEXBUS0_OPMODE_DAC)
		return;

	if (isr & FLEXBUS_TX_DONE_ISR) {
		rkfb_dac->result = DAC_DONE;
		rockchip_flexbus_writel(rkfb, FLEXBUS_ICR, FLEXBUS_TX_DONE_ISR);
	}

	if (isr & FLEXBUS_DAC_ERR_ISR) {
		rkfb_dac->result = DAC_ERR;

		if (isr & FLEXBUS_DMA_TIMEOUT_ISR) {
			dev_err_ratelimited(rkfb_dac->dev, "dma timeout!\n");
			rockchip_flexbus_writel(rkfb, FLEXBUS_ICR, FLEXBUS_DMA_TIMEOUT_ISR);
		}
		if (isr & FLEXBUS_DMA_ERR_ISR) {
			dev_err_ratelimited(rkfb_dac->dev, "dma err!\n");
			rockchip_flexbus_writel(rkfb, FLEXBUS_ICR, FLEXBUS_DMA_ERR_ISR);
		}
		if (isr & FLEXBUS_TX_UDF_ISR) {
			dev_err_ratelimited(rkfb_dac->dev, "tx underflow!\n");
			rockchip_flexbus_writel(rkfb, FLEXBUS_ICR, FLEXBUS_TX_UDF_ISR);
		}
		if (isr & FLEXBUS_TX_OVF_ISR) {
			dev_err_ratelimited(rkfb_dac->dev, "tx overflow!\n");
			rockchip_flexbus_writel(rkfb, FLEXBUS_ICR, FLEXBUS_TX_OVF_ISR);
		}
	}

	complete(&rkfb_dac->completion);
}

static int rockchip_flexbus_dac_read_raw(struct iio_dev *indio_dev,
					 struct iio_chan_spec const *chan, int *val, int *val2,
					 long mask)
{
	struct rockchip_flexbus_dac *rkfb_dac = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		*val = rkfb_dac->last_val;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = clk_get_rate(rkfb_dac->rkfb->clks[0].clk) / 2;
		return IIO_VAL_INT;

	default:
		return -EINVAL;
	}
}

static int rockchip_flexbus_dac_write_raw(struct iio_dev *indio_dev,
					  struct iio_chan_spec const *chan, int val, int val2,
					  long mask)
{
	struct rockchip_flexbus_dac *rkfb_dac = iio_priv(indio_dev);
	u16 *buf;
	u32 clk_rate;
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		buf = rkfb_dac->src_buf;
		switch (rkfb_dac->dfs) {
		case 4:
			buf[0] = val & 0xf;
			break;
		case 8:
			buf[0] = val & 0xff;
			break;
		case 16:
		default:
			buf[0] = val & 0xffff;
			break;
		}
		dma_wmb();

		ret = rkfb_dac->ops->write_block(rkfb_dac, rkfb_dac->src_buf,
						 rkfb_dac->src_buf_phys, rkfb_dac->src_buf_len, 1);
		break;

	case IIO_CHAN_INFO_SAMP_FREQ:
		mutex_lock(&rkfb_dac->lock);
		clk_rate = val * 2;
		if (clk_rate > FLEXBUS_MAX_TX_RATE)
			clk_rate = FLEXBUS_MAX_TX_RATE;
		ret = clk_set_rate(rkfb_dac->rkfb->clks[0].clk, clk_rate);
		mutex_unlock(&rkfb_dac->lock);
		break;

	default:
		return -EINVAL;
	}

	return ret;
}

static const struct iio_info rockchip_flexbus_dac_info = {
	.read_raw = rockchip_flexbus_dac_read_raw,
	.write_raw = rockchip_flexbus_dac_write_raw,
};

static irqreturn_t rockchip_flexbus_dac_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct iio_buffer *buf = indio_dev->buffer;
	struct rockchip_flexbus_dac *rkfb_dac = iio_priv(indio_dev);
	int ret;

	ret = iio_pop_from_buffer(buf, rkfb_dac->src_buf);
	if (ret) {
		dev_err(rkfb_dac->dev, "failed to pop_from_buffer\n");
		goto end;
	}

	rkfb_dac->ops->write_block(rkfb_dac, rkfb_dac->src_buf, rkfb_dac->src_buf_phys,
				   rkfb_dac->src_buf_len, indio_dev->channels->scan_type.repeat);

end:
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static int rockchip_flexbus_dac_init(struct rockchip_flexbus_dac *rkfb_dac)
{
	struct rockchip_flexbus *rkfb = rkfb_dac->rkfb;
	u32 val;

	if (rkfb_dac->free_sclk)
		rockchip_flexbus_writel(rkfb, FLEXBUS_FREE_SCLK, FLEXBUS_TX_FREE_MODE);

	switch (rkfb_dac->dfs) {
	case 4:
		val = rkfb->dfs_reg->dfs_4bit;
		break;
	case 8:
		val = rkfb->dfs_reg->dfs_8bit;
		break;
	case 16:
		val = rkfb->dfs_reg->dfs_16bit;
		break;
	default:
		return -EINVAL;
	}
	if (rkfb_dac->cpol)
		val |= FLEXBUS_CPOL;
	if (rkfb_dac->cpha)
		val |= FLEXBUS_CPHA;
	rockchip_flexbus_writel(rkfb, FLEXBUS_TX_CTL, val);

	rockchip_flexbus_setbits(rkfb, FLEXBUS_IMR, FLEXBUS_DAC_ISR);

	return 0;
}

static int rockchip_flexbus_dac_parse_dt(struct rockchip_flexbus_dac *rkfb_dac)
{
	rkfb_dac->cpol = device_property_read_bool(rkfb_dac->dev, "rockchip,cpol");
	rkfb_dac->cpha = device_property_read_bool(rkfb_dac->dev, "rockchip,cpha");
	rkfb_dac->free_sclk = device_property_read_bool(rkfb_dac->dev, "rockchip,free-sclk");

	if (device_property_read_u32(rkfb_dac->dev, "rockchip,dfs", &rkfb_dac->dfs)) {
		dev_err(rkfb_dac->dev, "failed to get dfs\n");
		return -EINVAL;
	}
	if (rkfb_dac->dfs != 4 && rkfb_dac->dfs != 8 && rkfb_dac->dfs != 16) {
		dev_err(rkfb_dac->dev, "invalid dfs\n");
		return -EINVAL;
	}

	return 0;
}

static const struct rockchip_flexbus_dac_ops rockchip_flexbus_dac_ops = {
	.write_block = rockchip_flexbus_dac_write_block,
};

static const struct of_device_id rockchip_flexbus_dac_of_match[] = {
	{ .compatible = "rockchip,flexbus-dac" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rockchip_flexbus_dac_of_match);

static int rockchip_flexbus_dac_probe(struct platform_device *pdev)
{
	struct rockchip_flexbus *rkfb = dev_get_drvdata(pdev->dev.parent);
	struct rockchip_flexbus_dac *rkfb_dac = NULL;
	struct iio_dev *indio_dev = NULL;
	int ret = 0;

	if (rkfb->opmode0 != ROCKCHIP_FLEXBUS0_OPMODE_DAC) {
		dev_err(&pdev->dev, "flexbus0 opmode mismatch!\n");
		return -ENODEV;
	}

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*rkfb_dac));
	if (!indio_dev)
		return -ENOMEM;
	rkfb_dac = iio_priv(indio_dev);

	platform_set_drvdata(pdev, indio_dev);
	rkfb_dac->dev = &pdev->dev;
	rkfb_dac->rkfb = rkfb;

	ret = rockchip_flexbus_dac_parse_dt(rkfb_dac);
	if (ret)
		return ret;

	mutex_init(&rkfb_dac->lock);
	init_completion(&rkfb_dac->completion);

	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->info = &rockchip_flexbus_dac_info;
	indio_dev->modes = INDIO_DIRECT_MODE;

	indio_dev->channels = rockchip_flexbus_dac_channel;
	indio_dev->num_channels = 1;

	ret = rockchip_flexbus_dac_init(rkfb_dac);
	if (ret)
		goto err_mutex_destroy;
	rkfb_dac->ops = &rockchip_flexbus_dac_ops;

	rkfb_dac->src_buf_len = DIV_ROUND_UP(indio_dev->channels->scan_type.repeat * rkfb_dac->dfs,
					     8);
	rkfb_dac->src_buf_len = round_up(rkfb_dac->src_buf_len, 0x40);
	if (rkfb_dac->src_buf_len < 0x40 || rkfb_dac->src_buf_len > 0x0fffffc0) {
		dev_err(rkfb_dac->dev, "buf_len should >= 0x40 and <= 0x0fffffc0\n");
		ret = -EINVAL;
		goto err_mutex_destroy;
	}

	rkfb_dac->src_buf = dmam_alloc_coherent(rkfb_dac->dev, rkfb_dac->src_buf_len,
						&rkfb_dac->src_buf_phys, GFP_KERNEL | __GFP_ZERO);
	if (!rkfb_dac->src_buf) {
		ret = -ENOMEM;
		goto err_mutex_destroy;
	}

	ret = devm_iio_triggered_buffer_setup_ext(&pdev->dev, indio_dev, NULL,
						  &rockchip_flexbus_dac_trigger_handler,
						  IIO_BUFFER_DIRECTION_OUT, NULL, NULL);
	if (ret)
		goto err_mutex_destroy;

	ret = devm_iio_device_register(&pdev->dev, indio_dev);
	if (ret)
		goto err_mutex_destroy;

	rockchip_flexbus_set_fb0(rkfb, rkfb_dac, rockchip_flexbus_dac_isr);

	return ret;

err_mutex_destroy:
	mutex_destroy(&rkfb_dac->lock);

	return ret;
}

static int rockchip_flexbus_dac_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct rockchip_flexbus_dac *rkfb_dac = iio_priv(indio_dev);
	struct rockchip_flexbus *rkfb = rkfb_dac->rkfb;

	rockchip_flexbus_set_fb0(rkfb, NULL, NULL);
	mutex_destroy(&rkfb_dac->lock);

	return 0;
}

static struct platform_driver rockchip_flexbus_dac_driver = {
	.probe	= rockchip_flexbus_dac_probe,
	.remove	= rockchip_flexbus_dac_remove,
	.driver	= {
		.name		= "rockchip_flexbus_dac",
		.of_match_table = rockchip_flexbus_dac_of_match,
	},
};

module_platform_driver(rockchip_flexbus_dac_driver);

MODULE_DESCRIPTION("Rockchip Flexbus DAC driver");
MODULE_LICENSE("GPL");
