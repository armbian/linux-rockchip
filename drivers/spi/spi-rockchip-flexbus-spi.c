// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Driver for Rockchip Flexbus FSPI mode
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/iopoll.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>

#include <dt-bindings/mfd/rockchip-flexbus.h>
#include <linux/mfd/rockchip-flexbus.h>

#define FLEXBUS_SPI_ERR_ISR	(FLEXBUS_DMA_TIMEOUT_ISR | FLEXBUS_DMA_ERR_ISR |	\
				 FLEXBUS_TX_UDF_ISR | FLEXBUS_TX_OVF_ISR)

/* Flexbus definition */
#define FLEXBUS_MAX_IOSIZE			(0x4000)
#define FLEXBUS_MAX_SPEED			(150 * 1000 * 1000)
#define FLEXBUS_MAX_CHIPSELECT_NUM		(1)
#define FLEXBUS_MAX_DLL_CELLS			(0xff)

#define FLEXBUS_DMA_TIMEOUT_MS			(0x1000)

struct rk_flexbus_spi {
	struct device *dev;
	struct rockchip_flexbus *fb;
	u32 version;
	u32 cur_speed;
	u32 max_iosize;
	u32 dll_cells;
	u32 dfs;
	u32 dfs_cfg;
	u32 mode; /* spi->mode restore */
	bool irq;
	u8 *temp_buf;
	dma_addr_t dma_temp_buf;
	struct completion cp;
};

static void rk_flexbus_spi_set_delay_lines(struct rk_flexbus_spi *flexbus, u32 cells)
{
	if (cells) {
		rockchip_flexbus_writel(flexbus->fb, FLEXBUS_DLL_EN, 1);
		rockchip_flexbus_writel(flexbus->fb, FLEXBUS_DLL_NUM, cells);
	} else {
		rockchip_flexbus_writel(flexbus->fb, FLEXBUS_DLL_EN, 0);
	}
}

static int rk_flexbus_spi_wait_idle(struct rk_flexbus_spi *flexbus, u32 timeout_ms)
{
	u32 status;
	int ret;

	/*
	 * For short packet data, in order to obtain a better response rate,
	 * the code is busy waiting for a period of time.
	 */
	ret = readl_poll_timeout(flexbus->fb->base + FLEXBUS_RISR, status,
				 status & (FLEXBUS_RX_DONE_ISR | FLEXBUS_TX_DONE_ISR), 0,
				 100);
	if (!ret)
		return 0;

	ret = readl_poll_timeout(flexbus->fb->base + FLEXBUS_RISR, status,
				 status & (FLEXBUS_RX_DONE_ISR | FLEXBUS_TX_DONE_ISR), 20,
				 timeout_ms * 1000);
	if (ret)
		dev_err(flexbus->dev, "DMA failed, risr=%x\n", status);

	return ret;
}

static int rk_flexbus_spi_send_start(struct rk_flexbus_spi *flexbus, struct spi_device *spi,
				     struct spi_transfer *xfer, dma_addr_t addr)
{
	u32 ctrl, len;

	len = xfer->len * 8 / flexbus->dfs;

	ctrl = FLEXBUS_TX_ONLY;
	rockchip_flexbus_writel(flexbus->fb, FLEXBUS_COM_CTL, ctrl);
	if (flexbus->irq)
		rockchip_flexbus_writel(flexbus->fb, FLEXBUS_IMR, FLEXBUS_TX_DONE_ISR);

	rockchip_flexbus_writel(flexbus->fb, FLEXBUS_TX_NUM, len);
	rockchip_flexbus_writel(flexbus->fb, FLEXBUS_DMA_SRC_ADDR0, addr >> 2);
	rockchip_flexbus_writel(flexbus->fb, FLEXBUS_ENR, FLEXBUS_TX_ENR);

	return 0;
}

static int rk_flexbus_spi_recv_start(struct rk_flexbus_spi *flexbus, struct spi_device *spi,
				     struct spi_transfer *xfer, dma_addr_t addr)
{
	u32 ctrl, len;

	len = xfer->len * 8 / flexbus->dfs;

	ctrl = FLEXBUS_RX_ONLY | FLEXBUS_SCLK_SHARE | FLEXBUS_TX_USE_RX;
	rockchip_flexbus_writel(flexbus->fb, FLEXBUS_COM_CTL, ctrl);
	if (flexbus->irq)
		rockchip_flexbus_writel(flexbus->fb, FLEXBUS_IMR, FLEXBUS_RX_DONE_ISR);

	rockchip_flexbus_writel(flexbus->fb, FLEXBUS_RX_NUM, len);
	rockchip_flexbus_writel(flexbus->fb, FLEXBUS_DMA_DST_ADDR0, addr >> 2);
	rockchip_flexbus_writel(flexbus->fb, FLEXBUS_ENR, FLEXBUS_RX_ENR);

	return 0;
}

static int rk_flexbus_spi_stop(struct rk_flexbus_spi *flexbus)
{
	rockchip_flexbus_writel(flexbus->fb, FLEXBUS_ENR, 0xFFFF0000);
	if (flexbus->irq) {
		rockchip_flexbus_writel(flexbus->fb, FLEXBUS_ICR, 0);
		rockchip_flexbus_writel(flexbus->fb, FLEXBUS_IMR, 0);
	}

	return 0;
}

static int rk_flexbus_spi_set_cs(struct rk_flexbus_spi *flexbus, bool cs_asserted)
{
	if (cs_asserted)
		rockchip_flexbus_writel(flexbus->fb, FLEXBUS_CSN_CFG, 0x30003);
	else
		rockchip_flexbus_writel(flexbus->fb, FLEXBUS_CSN_CFG, 0x30000);

	return 0;
}

static int rk_flexbus_spi_init(struct rk_flexbus_spi *flexbus)
{
	flexbus->fb->config->grf_config(flexbus->fb, false, 0, 0);

	flexbus->version = rockchip_flexbus_readl(flexbus->fb, FLEXBUS_REVISION) >> 16;
	flexbus->max_iosize = FLEXBUS_MAX_IOSIZE;

	rockchip_flexbus_writel(flexbus->fb, FLEXBUS_TXWAT_START, 0x10);
	rockchip_flexbus_writel(flexbus->fb, FLEXBUS_DMA_SRC_LEN0, flexbus->max_iosize);
	rockchip_flexbus_writel(flexbus->fb, FLEXBUS_DMA_DST_LEN0, flexbus->max_iosize);

	/* Using internal clk as sample clock */
	rockchip_flexbus_writel(flexbus->fb, FLEXBUS_SLAVE_MODE, 0);

	return 0;
}

static int rk_flexbus_spi_config(struct rk_flexbus_spi *flexbus, u32 mode)
{
	u32 ctrl;

	/* Din[1] PAD input valid when dfs=1, */
	if (flexbus->dfs == 1)
		rockchip_flexbus_writel(flexbus->fb, FLEXBUS_REMAP, 0x10);
	else
		rockchip_flexbus_writel(flexbus->fb, FLEXBUS_REMAP, 0x0);

	flexbus->fb->config->grf_config(flexbus->fb, false, mode & SPI_CPOL,
					mode & SPI_CPHA);

	ctrl = FLEXBUS_TX_CTL_UNIT_BYTE | flexbus->dfs_cfg;
	ctrl |= (mode & 0x3U) << FLEXBUS_TX_CTL_CPHA_SHIFT;
	if (!(mode & SPI_LSB_FIRST))
		ctrl |= FLEXBUS_TX_CTL_MSB;
	rockchip_flexbus_writel(flexbus->fb, FLEXBUS_TX_CTL, ctrl);

	ctrl = FLEXBUS_RX_CTL_UNIT_BYTE | flexbus->dfs_cfg | FLEXBUS_RXD_DY | FLEXBUS_AUTOPAD;
	ctrl |= (mode & 0x3U) << FLEXBUS_RX_CTL_CPHA_SHIFT;
	if (!(mode & SPI_LSB_FIRST))
		ctrl |= FLEXBUS_RX_CTL_MSB;
	rockchip_flexbus_writel(flexbus->fb, FLEXBUS_RX_CTL, ctrl);

	rk_flexbus_spi_set_delay_lines(flexbus, flexbus->dll_cells);

	return 0;
}

static void rk_flexbus_spi_irq_handler(struct rockchip_flexbus *rkfb, u32 isr)
{
	struct rk_flexbus_spi *flexbus = (struct rk_flexbus_spi *)rkfb->fb0_data;

	// dev_err(rkfb->dev, "isr=%x\n", isr);
	rockchip_flexbus_writel(rkfb, FLEXBUS_ICR, isr);
	if (isr & FLEXBUS_SPI_ERR_ISR) {
		if (isr & FLEXBUS_DMA_TIMEOUT_ISR) {
			dev_err_ratelimited(rkfb->dev, "dma timeout!\n");
			rockchip_flexbus_writel(rkfb, FLEXBUS_ICR, FLEXBUS_DMA_TIMEOUT_ISR);
		}
		if (isr & FLEXBUS_DMA_ERR_ISR) {
			dev_err_ratelimited(rkfb->dev, "dma err!\n");
			rockchip_flexbus_writel(rkfb, FLEXBUS_ICR, FLEXBUS_DMA_ERR_ISR);
		}
		if (isr & FLEXBUS_TX_UDF_ISR) {
			dev_err_ratelimited(rkfb->dev, "tx underflow!\n");
			rockchip_flexbus_writel(rkfb, FLEXBUS_ICR, FLEXBUS_TX_UDF_ISR);
		}
		if (isr & FLEXBUS_TX_OVF_ISR) {
			dev_err_ratelimited(rkfb->dev, "tx overflow!\n");
			rockchip_flexbus_writel(rkfb, FLEXBUS_ICR, FLEXBUS_TX_OVF_ISR);
		}
	}

	if ((isr & FLEXBUS_TX_DONE_ISR) || (isr & FLEXBUS_RX_DONE_ISR))
		complete(&flexbus->cp);
}

static int rockchip_flexbus_spi_transfer_one(struct spi_controller *ctlr,
					     struct spi_device *spi,
					     struct spi_transfer *xfer)
{
	struct rk_flexbus_spi *flexbus = spi_controller_get_devdata(ctlr);
	int ret;

	dev_dbg(flexbus->dev, "xfer dfs=%d-%d-%d tx=%p rx=%p len=0x%x\n",
		flexbus->dfs, xfer->tx_nbits, xfer->rx_nbits,
		xfer->tx_buf, xfer->rx_buf, xfer->len);

	if (xfer->len & 1 && flexbus->dfs == 16) {
		dev_err(flexbus->dev, "xfer->len aligned failed\n");
		return -EINVAL;
	}

	if (xfer->speed_hz != flexbus->cur_speed) {
		flexbus->cur_speed = xfer->speed_hz;
		clk_set_rate(flexbus->fb->clks[0].clk, flexbus->cur_speed * 2);
	}

	if (xfer->tx_buf) {
		memcpy(flexbus->temp_buf, xfer->tx_buf, xfer->len);
		dma_sync_single_for_device(flexbus->fb->dev, flexbus->dma_temp_buf,
					   xfer->len, DMA_TO_DEVICE);
	}

	if (flexbus->irq)
		init_completion(&flexbus->cp);
	if (xfer->tx_buf)
		ret = rk_flexbus_spi_send_start(flexbus, spi, xfer, flexbus->dma_temp_buf);
	else
		ret = rk_flexbus_spi_recv_start(flexbus, spi, xfer, flexbus->dma_temp_buf);
	if (flexbus->irq) {
		if (!wait_for_completion_timeout(&flexbus->cp,
						 msecs_to_jiffies(FLEXBUS_DMA_TIMEOUT_MS))) {
			dev_err(flexbus->dev, "DMA wait for rx transfer finish timeout\n");
			ret = -ETIMEDOUT;
		}
	} else {
		ret = rk_flexbus_spi_wait_idle(flexbus, FLEXBUS_DMA_TIMEOUT_MS);
	}

	if (xfer->rx_buf) {
		dma_sync_single_for_cpu(flexbus->fb->dev, flexbus->dma_temp_buf,
					xfer->len, DMA_FROM_DEVICE);
		memcpy(xfer->rx_buf, flexbus->temp_buf, xfer->len);
	}

	rk_flexbus_spi_stop(flexbus);

	return ret;
}

static void rockchip_flexbus_spi_set_cs(struct spi_device *spi, bool enable)
{
	struct rk_flexbus_spi *flexbus = spi_controller_get_devdata(spi->controller);

	rk_flexbus_spi_set_cs(flexbus, !enable);
}

static int rockchip_flexbus_spi_setup(struct spi_device *spi)
{
	struct rk_flexbus_spi *flexbus = spi_controller_get_devdata(spi->controller);

	flexbus->mode = spi->mode;

	return rk_flexbus_spi_config(flexbus, spi->mode);
}

static int rk_flexbus_spi_probe(struct platform_device *pdev)
{
	struct rockchip_flexbus *flexbus_dev = dev_get_drvdata(pdev->dev.parent);
	struct device *dev = &pdev->dev;
	struct spi_controller *master;
	struct rk_flexbus_spi *flexbus;
	int ret;

	if (flexbus_dev->opmode0 != ROCKCHIP_FLEXBUS0_OPMODE_SPI ||
	    flexbus_dev->opmode1 != ROCKCHIP_FLEXBUS1_OPMODE_NULL) {
		dev_err(&pdev->dev, "flexbus opmode mismatch!, fb0=%d fb1=%d\n",
			flexbus_dev->opmode0, flexbus_dev->opmode1);
		return -ENODEV;
	}

	master = devm_spi_alloc_master(&pdev->dev, sizeof(*flexbus));
	if (!master)
		return -ENOMEM;

	master->flags = SPI_MASTER_HALF_DUPLEX;
	master->dev.of_node = pdev->dev.of_node;
	master->max_speed_hz = FLEXBUS_MAX_SPEED;
	master->num_chipselect = FLEXBUS_MAX_CHIPSELECT_NUM;
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_LSB_FIRST;
	master->transfer_one = rockchip_flexbus_spi_transfer_one;
	master->setup = rockchip_flexbus_spi_setup;
	master->set_cs = rockchip_flexbus_spi_set_cs;

	flexbus = spi_controller_get_devdata(master);
	flexbus->dev = dev;
	flexbus->fb = flexbus_dev;

	device_property_read_u32(&pdev->dev, "rockchip,dll-cells", &flexbus->dll_cells);
	if (flexbus->dll_cells > FLEXBUS_MAX_DLL_CELLS)
		flexbus->dll_cells = FLEXBUS_MAX_DLL_CELLS;

	flexbus->irq = !device_property_read_bool(&pdev->dev, "rockchip,poll-only");

	if (device_property_read_u32(&pdev->dev, "rockchip,dfs", &flexbus->dfs)) {
		dev_err(&pdev->dev, "failed to get dfs\n");
		return -EINVAL;
	}
	switch (flexbus->dfs) {
	case 1:
		flexbus->dfs_cfg = flexbus->fb->dfs_reg->dfs_1bit;
		break;
	case 2:
		flexbus->dfs_cfg = flexbus->fb->dfs_reg->dfs_2bit;
		break;
	case 4:
		flexbus->dfs_cfg = flexbus->fb->dfs_reg->dfs_4bit;
		break;
	case 8:
		flexbus->dfs_cfg = flexbus->fb->dfs_reg->dfs_8bit;
		break;
	case 16:
		flexbus->dfs_cfg = flexbus->fb->dfs_reg->dfs_16bit;
		break;
	default:
		dev_err(&pdev->dev, "invalid dfs=%d\n", flexbus->dfs);
		return -EINVAL;
	}

	flexbus->temp_buf = (u8 *)devm_get_free_pages(dev, GFP_KERNEL | GFP_DMA32,
						   get_order(FLEXBUS_MAX_IOSIZE));
	if (!flexbus->temp_buf)
		return -ENOMEM;
	flexbus->dma_temp_buf = virt_to_phys(flexbus->temp_buf);

	platform_set_drvdata(pdev, flexbus);

	rockchip_flexbus_set_fb0(flexbus_dev, flexbus, rk_flexbus_spi_irq_handler);
	rk_flexbus_spi_init(flexbus);

	ret = devm_spi_register_controller(dev, master);
	if (ret)
		return -ENOMEM;

	dev_info(&pdev->dev, "dfs=%d dll=%d irq=%d\n", flexbus->dfs,
		 flexbus->dll_cells, flexbus->irq);

	return 0;
}

static int rk_flexbus_spi_resume(struct device *dev)
{
	struct rk_flexbus_spi *flexbus = dev_get_drvdata(dev);

	rk_flexbus_spi_init(flexbus);
	rk_flexbus_spi_config(flexbus, flexbus->mode);
	if (flexbus->dll_cells)
		rk_flexbus_spi_set_delay_lines(flexbus, flexbus->dll_cells);

	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(rk_flexbus_spi_pm_ops, NULL, rk_flexbus_spi_resume);

static const struct of_device_id rk_flexbus_spi_dt_ids[] = {
	{ .compatible = "rockchip,flexbus-spi"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rk_flexbus_spi_dt_ids);

static struct platform_driver rk_flexbus_spi_driver = {
	.driver = {
		.name	= "rockchip-flexbus-spi",
		.of_match_table = rk_flexbus_spi_dt_ids,
		.pm = pm_sleep_ptr(&rk_flexbus_spi_pm_ops),
	},
	.probe	= rk_flexbus_spi_probe,
};
module_platform_driver(rk_flexbus_spi_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Rockchip Flexbus Controller Under SPI Transmission Protocol Driver");
MODULE_AUTHOR("Jon Lin <Jon.lin@rock-chips.com>");
