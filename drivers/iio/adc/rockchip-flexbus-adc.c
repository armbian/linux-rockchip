// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Driver for Rockchip Flexbus ADC opmode
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

#define FLEXBUS_MAX_RX_RATE	200000000
#define FLEXBUS_ADC_ERR_ISR	(FLEXBUS_DMA_TIMEOUT_ISR | FLEXBUS_DMA_ERR_ISR |	\
				 FLEXBUS_RX_UDF_ISR | FLEXBUS_RX_OVF_ISR)
#define FLEXBUS_ADC_ISR		(FLEXBUS_ADC_ERR_ISR | FLEXBUS_RX_DONE_ISR)
#define FLEXBUS_ADC_TIMEOUT	msecs_to_jiffies(100)
#define FLEXBUS_ADC_REPEAT	0x40

enum flexbus_adc_result {
	ADC_DONE = 0,
	ADC_ERR,
};

struct rockchip_flexbus_adc;

struct rockchip_flexbus_adc_ops {
	int (*read_block)(struct rockchip_flexbus_adc *rkfb_adc, dma_addr_t dst_phys, u32 dst_len,
			  u32 num_of_dfs);
};

struct rockchip_flexbus_adc {
	struct device		*dev;
	struct rockchip_flexbus	*rkfb;
	struct clk		*ref_clk;	/* ref_clk is only used for slave-mode */
	u32			dfs;
	bool			slave_mode;
	bool			free_sclk;
	bool			auto_pad;
	bool			cpol;
	bool			cpha;
	struct mutex		lock;
	struct completion	completion;
	enum flexbus_adc_result	result;
	u32			dst_buf_len;
	dma_addr_t		dst_buf_phys;
	void			*dst_buf;
	const struct rockchip_flexbus_adc_ops	*ops;
};

static const struct iio_chan_spec rockchip_flexbus_adc_channel[] = {
	{
		.type = IIO_VOLTAGE,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SAMP_FREQ),
		.scan_type = {
			.sign = 'u',
			.realbits = 16,
			.storagebits = 16,
			.shift = 0,
			.repeat = FLEXBUS_ADC_REPEAT,
			.endianness = IIO_CPU,
		},
	},
};

static int rockchip_flexbus_adc_read_block(struct rockchip_flexbus_adc *rkfb_adc,
					   dma_addr_t dst_phys, u32 dst_len, u32 num_of_dfs)
{
	struct rockchip_flexbus *rkfb = rkfb_adc->rkfb;
	int ret = 0;

	if (num_of_dfs > 0x08000000) {
		dev_err(rkfb_adc->dev, "num_of_dfs is too large\n");
		return -EINVAL;
	}

	mutex_lock(&rkfb_adc->lock);
	reinit_completion(&rkfb_adc->completion);

	rockchip_flexbus_writel(rkfb, FLEXBUS_RX_NUM, num_of_dfs);
	rockchip_flexbus_writel(rkfb, FLEXBUS_DMA_DST_ADDR0, (ulong)dst_phys >> 2);
	rockchip_flexbus_writel(rkfb, FLEXBUS_DMA_DST_LEN0, dst_len);

	rockchip_flexbus_writel(rkfb, FLEXBUS_ENR, FLEXBUS_RX_ENR);
	if (!wait_for_completion_timeout(&rkfb_adc->completion, FLEXBUS_ADC_TIMEOUT)) {
		ret = -ETIMEDOUT;
		goto end;
	}

	if (rkfb_adc->result != ADC_DONE)
		ret = -1;

end:
	rockchip_flexbus_writel(rkfb, FLEXBUS_ENR, FLEXBUS_RX_DIS);
	mutex_unlock(&rkfb_adc->lock);

	return ret;
}

static void rockchip_flexbus_adc_isr(struct rockchip_flexbus *rkfb, u32 isr)
{
	struct rockchip_flexbus_adc *rkfb_adc = rkfb->fb1_data;

	if (rkfb->opmode1 != ROCKCHIP_FLEXBUS1_OPMODE_ADC)
		return;

	if (isr & FLEXBUS_RX_DONE_ISR) {
		rkfb_adc->result = ADC_DONE;
		rockchip_flexbus_writel(rkfb, FLEXBUS_ICR, FLEXBUS_RX_DONE_ISR);
	}

	if (isr & FLEXBUS_ADC_ERR_ISR) {
		rkfb_adc->result = ADC_ERR;

		if (isr & FLEXBUS_DMA_TIMEOUT_ISR) {
			dev_err_ratelimited(rkfb_adc->dev, "dma timeout!\n");
			rockchip_flexbus_writel(rkfb, FLEXBUS_ICR, FLEXBUS_DMA_TIMEOUT_ISR);
		}
		if (isr & FLEXBUS_DMA_ERR_ISR) {
			dev_err_ratelimited(rkfb_adc->dev, "dma err!\n");
			rockchip_flexbus_writel(rkfb, FLEXBUS_ICR, FLEXBUS_DMA_ERR_ISR);
		}
		if (isr & FLEXBUS_RX_UDF_ISR) {
			dev_err_ratelimited(rkfb_adc->dev, "rx underflow!\n");
			rockchip_flexbus_writel(rkfb, FLEXBUS_ICR, FLEXBUS_RX_UDF_ISR);
		}
		if (isr & FLEXBUS_RX_OVF_ISR) {
			dev_err_ratelimited(rkfb_adc->dev, "rx overflow!\n");
			rockchip_flexbus_writel(rkfb, FLEXBUS_ICR, FLEXBUS_RX_OVF_ISR);
		}
	}

	complete(&rkfb_adc->completion);
}

static int rockchip_flexbus_adc_read_raw(struct iio_dev *indio_dev,
					 struct iio_chan_spec const *chan, int *val, int *val2,
					 long mask)
{
	struct rockchip_flexbus_adc *rkfb_adc = iio_priv(indio_dev);
	u32 val_mask;
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = rkfb_adc->ops->read_block(rkfb_adc, rkfb_adc->dst_buf_phys,
						rkfb_adc->dst_buf_len, 8);
		if (ret)
			return ret;

		switch (rkfb_adc->dfs) {
		case 4:
			val_mask = 0xf;
			break;
		case 8:
			val_mask = 0xff;
			break;
		case 16:
		default:
			val_mask = 0xffff;
			break;
		}
		dma_rmb();
		*val = readw(rkfb_adc->dst_buf) & val_mask;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SAMP_FREQ:
		if (rkfb_adc->slave_mode && !rkfb_adc->ref_clk) {
			dev_err(rkfb_adc->dev,
				"sample freq depends on clock source of the ADC device\n");
			return -EINVAL;
		}
		if (rkfb_adc->slave_mode)
			*val = clk_get_rate(rkfb_adc->ref_clk);
		else
			*val = clk_get_rate(rkfb_adc->rkfb->clks[1].clk) / 2;
		return IIO_VAL_INT;

	default:
		return -EINVAL;
	}
}

static int rockchip_flexbus_adc_write_raw(struct iio_dev *indio_dev,
					  struct iio_chan_spec const *chan, int val, int val2,
					  long mask)
{
	struct rockchip_flexbus_adc *rkfb_adc = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		if (rkfb_adc->slave_mode && !rkfb_adc->ref_clk) {
			dev_err(rkfb_adc->dev,
				"sample freq depends on clock source of the ADC device\n");
			return -EINVAL;
		}
		mutex_lock(&rkfb_adc->lock);
		if (val > FLEXBUS_MAX_RX_RATE / 2)
			val = FLEXBUS_MAX_RX_RATE / 2;
		if (rkfb_adc->slave_mode)
			ret = clk_set_rate(rkfb_adc->ref_clk, val);
		else
			ret = clk_set_rate(rkfb_adc->rkfb->clks[1].clk, val * 2);
		mutex_unlock(&rkfb_adc->lock);
		break;

	default:
		return -EINVAL;
	}

	return ret;
}

static const struct iio_info rockchip_flexbus_adc_info = {
	.read_raw = rockchip_flexbus_adc_read_raw,
	.write_raw = rockchip_flexbus_adc_write_raw,
};

static irqreturn_t rockchip_flexbus_adc_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct rockchip_flexbus_adc *rkfb_adc = iio_priv(indio_dev);
	int ret;

	ret = rkfb_adc->ops->read_block(rkfb_adc, rkfb_adc->dst_buf_phys, rkfb_adc->dst_buf_len,
					indio_dev->channels->scan_type.repeat);
	if (!ret)
		iio_push_to_buffers_with_timestamp(indio_dev, rkfb_adc->dst_buf,
						   iio_get_time_ns(indio_dev));

	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static int rockchip_flexbus_adc_init(struct rockchip_flexbus_adc *rkfb_adc)
{
	struct rockchip_flexbus *rkfb = rkfb_adc->rkfb;
	u32 val;

	if (!rkfb_adc->slave_mode && rkfb_adc->free_sclk)
		rockchip_flexbus_writel(rkfb, FLEXBUS_FREE_SCLK, FLEXBUS_RX_FREE_MODE);

	val = 0x0;
	if (rkfb_adc->slave_mode)
		val |= FLEXBUS_CLK1_IN;
	rockchip_flexbus_writel(rkfb, FLEXBUS_SLAVE_MODE, val);

	switch (rkfb_adc->dfs) {
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
	if (rkfb_adc->auto_pad)
		val |= FLEXBUS_AUTOPAD;
	if (rkfb_adc->cpol)
		val |= FLEXBUS_CPOL;
	if (rkfb_adc->cpha)
		val |= FLEXBUS_CPHA;
	rockchip_flexbus_writel(rkfb, FLEXBUS_RX_CTL, val);

	rockchip_flexbus_setbits(rkfb, FLEXBUS_IMR, FLEXBUS_ADC_ISR);

	rkfb->config->grf_config(rkfb, rkfb_adc->slave_mode, rkfb_adc->cpol, rkfb_adc->cpha);

	return 0;
}

static int rockchip_flexbus_adc_parse_dt(struct rockchip_flexbus_adc *rkfb_adc)
{
	rkfb_adc->slave_mode = device_property_read_bool(rkfb_adc->dev, "rockchip,slave-mode");
	rkfb_adc->auto_pad = device_property_read_bool(rkfb_adc->dev, "rockchip,auto-pad");
	rkfb_adc->cpol = device_property_read_bool(rkfb_adc->dev, "rockchip,cpol");
	rkfb_adc->cpha = device_property_read_bool(rkfb_adc->dev, "rockchip,cpha");
	rkfb_adc->free_sclk = device_property_read_bool(rkfb_adc->dev, "rockchip,free-sclk");

	if (device_property_read_u32(rkfb_adc->dev, "rockchip,dfs", &rkfb_adc->dfs)) {
		dev_err(rkfb_adc->dev, "failed to get dfs\n");
		return -EINVAL;
	}
	if (rkfb_adc->dfs != 4 && rkfb_adc->dfs != 8 && rkfb_adc->dfs != 16) {
		dev_err(rkfb_adc->dev, "invalid dfs\n");
		return -EINVAL;
	}

	return 0;
}

static const struct rockchip_flexbus_adc_ops rockchip_flexbus_adc_ops = {
	.read_block = rockchip_flexbus_adc_read_block,
};

static const struct of_device_id rockchip_flexbus_adc_of_match[] = {
	{ .compatible = "rockchip,flexbus-adc" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rockchip_flexbus_adc_of_match);

static int rockchip_flexbus_adc_probe(struct platform_device *pdev)
{
	struct rockchip_flexbus *rkfb = dev_get_drvdata(pdev->dev.parent);
	struct rockchip_flexbus_adc *rkfb_adc = NULL;
	struct iio_dev *indio_dev = NULL;
	int ret = 0;

	if (rkfb->opmode1 != ROCKCHIP_FLEXBUS1_OPMODE_ADC) {
		dev_err(&pdev->dev, "flexbus1 opmode mismatch!\n");
		return -ENODEV;
	}

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*rkfb_adc));
	if (!indio_dev)
		return -ENOMEM;
	rkfb_adc = iio_priv(indio_dev);

	platform_set_drvdata(pdev, indio_dev);
	rkfb_adc->dev = &pdev->dev;
	rkfb_adc->rkfb = rkfb;

	ret = rockchip_flexbus_adc_parse_dt(rkfb_adc);
	if (ret)
		return ret;

	if (rkfb_adc->slave_mode) {
		rkfb_adc->ref_clk = devm_clk_get(&pdev->dev, "ref_clk");
		if (IS_ERR(rkfb_adc->ref_clk)) {
			dev_warn(rkfb_adc->dev,
				 "failed to get ref_clk in slave-mode. Please make sure the ADC device has clock source.\n");
			rkfb_adc->ref_clk = NULL;
		} else {
			ret = clk_prepare_enable(rkfb_adc->ref_clk);
			if (ret) {
				dev_err(rkfb_adc->dev, "failed to enable ref_clk.\n");
				return ret;
			}
		}
	}

	mutex_init(&rkfb_adc->lock);
	init_completion(&rkfb_adc->completion);

	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->info = &rockchip_flexbus_adc_info;
	indio_dev->modes = INDIO_DIRECT_MODE;

	indio_dev->channels = rockchip_flexbus_adc_channel;
	indio_dev->num_channels = 1;

	ret = rockchip_flexbus_adc_init(rkfb_adc);
	if (ret)
		goto err_mutex_destroy;
	rkfb_adc->ops = &rockchip_flexbus_adc_ops;

	rkfb_adc->dst_buf_len = DIV_ROUND_UP(indio_dev->channels->scan_type.repeat * rkfb_adc->dfs,
					     8);
	rkfb_adc->dst_buf_len = round_up(rkfb_adc->dst_buf_len, 0x40);
	if (rkfb_adc->dst_buf_len < 0x40 || rkfb_adc->dst_buf_len > 0x0fffffc0) {
		dev_err(rkfb_adc->dev, "buf_len should >= 0x40 and <= 0x0fffffc0\n");
		ret = -EINVAL;
		goto err_mutex_destroy;
	}
	rkfb_adc->dst_buf = dmam_alloc_coherent(rkfb_adc->dev, rkfb_adc->dst_buf_len,
						&rkfb_adc->dst_buf_phys, GFP_KERNEL | __GFP_ZERO);
	if (!rkfb_adc->dst_buf) {
		ret = -ENOMEM;
		goto err_mutex_destroy;
	}

	ret = devm_iio_triggered_buffer_setup(&pdev->dev, indio_dev, &iio_pollfunc_store_time,
					      &rockchip_flexbus_adc_trigger_handler, NULL);
	if (ret)
		goto err_mutex_destroy;

	ret = devm_iio_device_register(&pdev->dev, indio_dev);
	if (ret)
		goto err_mutex_destroy;

	rockchip_flexbus_set_fb1(rkfb, rkfb_adc, rockchip_flexbus_adc_isr);

	return ret;

err_mutex_destroy:
	mutex_destroy(&rkfb_adc->lock);
	if (rkfb_adc->ref_clk)
		clk_disable_unprepare(rkfb_adc->ref_clk);

	return ret;
}

static int rockchip_flexbus_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct rockchip_flexbus_adc *rkfb_adc = iio_priv(indio_dev);
	struct rockchip_flexbus *rkfb = rkfb_adc->rkfb;

	rockchip_flexbus_set_fb1(rkfb, NULL, NULL);
	mutex_destroy(&rkfb_adc->lock);
	if (rkfb_adc->ref_clk)
		clk_disable_unprepare(rkfb_adc->ref_clk);

	return 0;
}

static struct platform_driver rockchip_flexbus_adc_driver = {
	.probe	= rockchip_flexbus_adc_probe,
	.remove	= rockchip_flexbus_adc_remove,
	.driver	= {
		.name		= "rockchip_flexbus_adc",
		.of_match_table = rockchip_flexbus_adc_of_match,
	},
};

module_platform_driver(rockchip_flexbus_adc_driver);

MODULE_DESCRIPTION("Rockchip Flexbus ADC driver");
MODULE_LICENSE("GPL");
