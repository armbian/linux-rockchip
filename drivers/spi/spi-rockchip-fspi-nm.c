// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Driver for Rockchip FSPI normal mode
 *
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>

// #define ROCKCHIP_FSPI_VERBOSE

/* System control */
#define FSPI_CTRL			0x0
#define  FSPI_CTRL_SPIM_3		BIT(0)
#define  FSPI_CTRL_PHASE_SEL_NEGETIVE	BIT(1)
#define  FSPI_CTRL_DTR_MODE		BIT(2)
#define  FSPI_CTRL_CMD_BITS_SHIFT	8
#define  FSPI_CTRL_ADDR_BITS_SHIFT	10
#define  FSPI_CTRL_DATA_BITS_SHIFT	12
#define  FSPI_CTRL_DUAL_MODE		((1 << 8) | (1 << 10) | (1 << 12))
#define  FSPI_CTRL_QUAD_MODE		((2 << 8) | (2 << 10) | (2 << 12))
#define  FSPI_CTRL_OCTAL_MODE		((3 << 8) | (3 << 10) | (3 << 12))
#define  FSPI_CTRL_DTR_MODE_BY_DEVICE	BIT(17)
#define  FSPI_CTRL_CMD_STR_SHIFT	19
#define  FSPI_CTRL_ADDR_STR_SHIFT	20
#define  FSPI_CTRL_CMD_CTRL_CMD_EXT	(2 << 27)
#define  FSPI_CTRL_WPEN			BIT(29)

/* Interrupt mask */
#define FSPI_IMR			0x4
#define  FSPI_IMR_RX_FULL		BIT(0)
#define  FSPI_IMR_RX_UFLOW		BIT(1)
#define  FSPI_IMR_TX_OFLOW		BIT(2)
#define  FSPI_IMR_TX_EMPTY		BIT(3)
#define  FSPI_IMR_TRAN_FINISH		BIT(4)
#define  FSPI_IMR_BUS_ERR		BIT(5)
#define  FSPI_IMR_NSPI_ERR		BIT(6)
#define  FSPI_IMR_DMA			BIT(7)

/* Interrupt clear */
#define FSPI_ICLR			0x8
#define  FSPI_ICLR_RX_FULL		BIT(0)
#define  FSPI_ICLR_RX_UFLOW		BIT(1)
#define  FSPI_ICLR_TX_OFLOW		BIT(2)
#define  FSPI_ICLR_TX_EMPTY		BIT(3)
#define  FSPI_ICLR_TRAN_FINISH		BIT(4)
#define  FSPI_ICLR_BUS_ERR		BIT(5)
#define  FSPI_ICLR_NSPI_ERR		BIT(6)
#define  FSPI_ICLR_DMA			BIT(7)

/* FIFO threshold level */
#define FSPI_FTLR			0xc
#define  FSPI_FTLR_TX_SHIFT		0
#define  FSPI_FTLR_TX_MASK		0x1f
#define  FSPI_FTLR_RX_SHIFT		8
#define  FSPI_FTLR_RX_MASK		0x1f

/* Reset FSM and FIFO */
#define FSPI_RCVR			0x10
#define  FSPI_RCVR_RESET		BIT(0)

/* Enhanced mode */
#define FSPI_AX				0x14

/* Address Bit number */
#define FSPI_ABIT			0x18

/* Interrupt status */
#define FSPI_ISR			0x1c
#define  FSPI_ISR_RX_FULL_SHIFT		BIT(0)
#define  FSPI_ISR_RX_UFLOW_SHIFT		BIT(1)
#define  FSPI_ISR_TX_OFLOW_SHIFT		BIT(2)
#define  FSPI_ISR_TX_EMPTY_SHIFT		BIT(3)
#define  FSPI_ISR_TX_FINISH_SHIFT	BIT(4)
#define  FSPI_ISR_BUS_ERR_SHIFT		BIT(5)
#define  FSPI_ISR_NSPI_ERR_SHIFT	BIT(6)
#define  FSPI_ISR_DMA_SHIFT		BIT(7)

/* FIFO status */
#define FSPI_FSR			0x20
#define  FSPI_FSR_TX_IS_FULL		BIT(0)
#define  FSPI_FSR_TX_IS_EMPTY		BIT(1)
#define  FSPI_FSR_RX_IS_EMPTY		BIT(2)
#define  FSPI_FSR_RX_IS_FULL		BIT(3)
#define  FSPI_FSR_TXLV_MASK		GENMASK(13, 8)
#define  FSPI_FSR_TXLV_SHIFT		8
#define  FSPI_FSR_RXLV_MASK		GENMASK(20, 16)
#define  FSPI_FSR_RXLV_SHIFT		16

/* FSM status */
#define FSPI_SR				0x24
#define  FSPI_SR_IS_IDLE		0x0
#define  FSPI_SR_IS_BUSY		0x1

/* Raw interrupt status */
#define FSPI_RISR			0x28
#define  FSPI_RISR_RX_FULL		BIT(0)
#define  FSPI_RISR_RX_UNDERFLOW		BIT(1)
#define  FSPI_RISR_TX_OVERFLOW		BIT(2)
#define  FSPI_RISR_TX_EMPTY		BIT(3)
#define  FSPI_RISR_TRAN_FINISH		BIT(4)
#define  FSPI_RISR_BUS_ERR		BIT(5)
#define  FSPI_RISR_NSPI_ERR		BIT(6)
#define  FSPI_RISR_DMA			BIT(7)

/* Version */
#define FSPI_VER			0x2C
#define  FSPI_VER_3			0x3
#define  FSPI_VER_4			0x4
#define  FSPI_VER_5			0x5
#define  FSPI_VER_6			0x6
#define  FSPI_VER_8			0x8
#define  FSPI_VER_9			0x9
#define  FSPI_VER_13			0x13
#define  FSPI_CAP_X8			BIT(18)

/* Ext ctrl */
#define FSPI_EXT_CTRL			0x34
#define  FSPI_SCLK_X2_BYPASS		BIT(24)

/* Delay line controller resiter */
#define FSPI_DLL_CTRL0			0x3C
#define FSPI_DLL_CTRL0_SCLK_SMP_DLL	BIT(15)
#define FSPI_DLL_CTRL0_DLL_MAX_VER4	0xFFU
#define FSPI_DLL_CTRL0_DLL_MAX_VER5	0x1FFU

/* Dummy Cycle Control Register */
#define FSPI_DUMM_CTRL			0x74
#define FSPI_DUMMY_CTRL_SEL		BIT(0)
#define FSPI_DUMMY_CTRL_EXT_SHIFT	1

/* Command Extend Register */
#define FSPI_CMD_EXT			0x78

/* Master trigger */
#define FSPI_DMA_TRIGGER		0x80
#define FSPI_DMA_TRIGGER_START		1

/* Src or Dst addr for host */
#define FSPI_DMA_ADDR			0x84

/* Length control register extension 32GB */
#define FSPI_LEN_CTRL			0x88
#define FSPI_LEN_CTRL_TRB_SEL		1
#define FSPI_LEN_EXT			0x8C

/* Spi Mode */
#define FSPI_SPI_MODE			0xd4

/* Command */
#define FSPI_CMD			0x100
#define  FSPI_CMD_IDX_SHIFT		0
#define  FSPI_CMD_DUMMY_SHIFT		8
#define  FSPI_CMD_DIR_SHIFT		12
#define  FSPI_CMD_DIR_RD		0
#define  FSPI_CMD_DIR_WR		1
#define  FSPI_CMD_ADDR_SHIFT		14
#define  FSPI_CMD_ADDR_0BITS		0
#define  FSPI_CMD_ADDR_24BITS		1
#define  FSPI_CMD_ADDR_32BITS		2
#define  FSPI_CMD_ADDR_XBITS		3
#define  FSPI_CMD_TRAN_BYTES_SHIFT	16
#define  FSPI_CMD_CS_SHIFT		30

/* Address */
#define FSPI_ADDR			0x104

/* Data */
#define FSPI_DATA			0x108

/* Flexbus definition */
#define FSPI_MAX_IOSIZE			(0x4000)
#define FSPI_MAX_SPEED			(150 * 1000 * 1000)
#define FSPI_MAX_CHIPSELECT_NUM		(1)
#define FSPI_MAX_DLL_CELLS		(0xff)

#define FSPI_DMA_TIMEOUT_MS		(0x1000)

#define FSPI_DMA_TRANS_THRETHOLD	(0x40)
#define FSPI_TX_CMD			(0x3C)
#define FSPI_RX_CMD			(0x6B)

struct fspi_nm {
	struct device *dev;
	/* feature */
	u16 version;
	bool sclk_x2_bypass;
	bool force_cmd;
	bool support_octa;
	u32 max_dll_cells;

	/* io resource */
	void __iomem *regbase;
	struct clk *hclk;
	struct clk *clk;

	/* configuration */
	u32 speed;
	u32 dll_cells;
	bool use_dma;
	u32 tx_ctrl;
	u32 rx_ctrl;

	/* dma */
	void *buffer;
	dma_addr_t dma_buffer;
	struct completion cp;
};

static int fspi_nm_reset(struct fspi_nm *fspi)
{
	int err;
	u32 status;

	writel(FSPI_RCVR_RESET, fspi->regbase + FSPI_RCVR);

	err = readl_poll_timeout(fspi->regbase + FSPI_RCVR, status,
				 !(status & FSPI_RCVR_RESET), 20,
				 jiffies_to_usecs(HZ));
	if (err)
		dev_err(fspi->dev, "FSPI reset never finished\n");

	/* Still need to clear the masked interrupt from RISR */
	writel(0xFFFFFFFF, fspi->regbase + FSPI_ICLR);

	dev_dbg(fspi->dev, "reset\n");

	return err;
}

static void fspi_nm_set_delay_lines(struct fspi_nm *fspi, u32 cells)
{
	u32 val = 0;

	if (cells)
		val = FSPI_DLL_CTRL0_SCLK_SMP_DLL | cells;

	writel(val, fspi->regbase + FSPI_DLL_CTRL0);
}

static int fspi_nm_clk_set_rate(struct fspi_nm *fspi, unsigned long speed)
{
	if (fspi->sclk_x2_bypass)
		return clk_set_rate(fspi->clk, speed);
	else
		return clk_set_rate(fspi->clk, speed * 2);
}

static void fspi_nm_irq_unmask(struct fspi_nm *fspi, u32 mask)
{
	u32 reg;

	/* Enable transfer complete interrupt */
	reg = readl(fspi->regbase + FSPI_IMR);
	reg &= ~mask;
	writel(reg, fspi->regbase + FSPI_IMR);
}

static void fspi_nm_xfer_setup(struct fspi_nm *fspi, struct spi_transfer *xfer)
{
	writel(xfer->len, fspi->regbase + FSPI_LEN_EXT);
	if (xfer->tx_buf) {
		writel(fspi->tx_ctrl, fspi->regbase + FSPI_CTRL);
		writel(FSPI_TX_CMD | (FSPI_CMD_DIR_WR << FSPI_CMD_DIR_SHIFT),
		       fspi->regbase + FSPI_CMD);
	} else {
		writel(fspi->rx_ctrl, fspi->regbase + FSPI_CTRL);
		writel(FSPI_RX_CMD | (FSPI_CMD_DIR_RD << FSPI_CMD_DIR_SHIFT),
		       fspi->regbase + FSPI_CMD);
	}
}

static int fspi_nm_wait_txfifo_ready(struct fspi_nm *fspi, u32 timeout_us)
{
	int ret = 0;
	u32 status;

	ret = readl_poll_timeout(fspi->regbase + FSPI_FSR, status,
				 status & FSPI_FSR_TXLV_MASK, 0,
				 timeout_us);
	if (ret) {
		print_hex_dump(KERN_WARNING, "fspi", DUMP_PREFIX_OFFSET, 4,
			       4, fspi->regbase, 0x104, 0);
		dev_dbg(fspi->dev, "fspi wait tx fifo timeout\n");

		return -ETIMEDOUT;
	}

	return (status & FSPI_FSR_TXLV_MASK) >> FSPI_FSR_TXLV_SHIFT;
}

static int fspi_nm_wait_rxfifo_ready(struct fspi_nm *fspi, u32 timeout_us)
{
	int ret = 0;
	u32 status;

	ret = readl_poll_timeout(fspi->regbase + FSPI_FSR, status,
				 status & FSPI_FSR_RXLV_MASK, 0,
				 timeout_us);
	if (ret) {
		print_hex_dump(KERN_WARNING, "fspi", DUMP_PREFIX_OFFSET, 4,
			       4, fspi->regbase, 0x104, 0);
		dev_err(fspi->dev, "fspi wait rx fifo timeout\n");

		return -ETIMEDOUT;
	}

	return (status & FSPI_FSR_RXLV_MASK) >> FSPI_FSR_RXLV_SHIFT;
}

static int fspi_nm_write_fifo(struct fspi_nm *fspi, const u8 *buf, int len)
{
	u8 bytes = len & 0x3;
	u32 dwords;
	int tx_level;
	u32 write_words;
	u32 tmp = 0;

	dwords = len >> 2;
	while (dwords) {
		tx_level = fspi_nm_wait_txfifo_ready(fspi, 1000);
		if (tx_level < 0)
			return tx_level;
		write_words = min_t(u32, tx_level, dwords);
		iowrite32_rep(fspi->regbase + FSPI_DATA, buf, write_words);
		buf += write_words << 2;
		dwords -= write_words;
	}

	/* write the rest non word aligned bytes */
	if (bytes) {
		tx_level = fspi_nm_wait_txfifo_ready(fspi, 1000);
		if (tx_level < 0)
			return tx_level;
		memcpy(&tmp, buf, bytes);
		writel(tmp, fspi->regbase + FSPI_DATA);
	}

	return 0;
}

static int fspi_nm_read_fifo(struct fspi_nm *fspi, u8 *buf, int len)
{
	u8 bytes = len & 0x3;
	u32 dwords;
	u8 read_words;
	int rx_level;
	int tmp;

	/* word aligned access only */
	dwords = len >> 2;
	while (dwords) {
		rx_level = fspi_nm_wait_rxfifo_ready(fspi, 1000);
		if (rx_level < 0)
			return rx_level;
		read_words = min_t(u32, rx_level, dwords);
		ioread32_rep(fspi->regbase + FSPI_DATA, buf, read_words);
		buf += read_words << 2;
		dwords -= read_words;
	}

	/* read the rest non word aligned bytes */
	if (bytes) {
		rx_level = fspi_nm_wait_rxfifo_ready(fspi, 1000);
		if (rx_level < 0)
			return rx_level;
		tmp = readl(fspi->regbase + FSPI_DATA);
		memcpy(buf, &tmp, bytes);
	}

	return 0;
}

static int fspi_nm_fifo_transfer_dma(struct fspi_nm *fspi, dma_addr_t dma_buf, size_t len)
{
	writel(0xFFFFFFFF, fspi->regbase + FSPI_ICLR);
	writel((u32)dma_buf, fspi->regbase + FSPI_DMA_ADDR);
	writel(FSPI_DMA_TRIGGER_START, fspi->regbase + FSPI_DMA_TRIGGER);

	return 0;
}

static int fspi_nm_xfer_data_poll(struct fspi_nm *fspi, struct spi_transfer *xfer)
{
	dev_dbg(fspi->dev, "fspi xfer_poll len=%x\n", xfer->len);

	if (xfer->tx_buf)
		return fspi_nm_write_fifo(fspi, xfer->tx_buf, xfer->len);
	else
		return fspi_nm_read_fifo(fspi, xfer->rx_buf, xfer->len);
}

static int fspi_nm_xfer_data_dma(struct fspi_nm *fspi, struct spi_transfer *xfer)
{
	int ret;
	u32 len = xfer->len;
#ifdef ROCKCHIP_FSPI_VERBOSE
	ktime_t start_time;
	ktime_t end_time;
	unsigned long us = 0;
#endif

	dev_dbg(fspi->dev, "fspi xfer_dma len=%x\n", len);

	if (xfer->tx_buf) {
		memcpy(fspi->buffer, xfer->tx_buf, len);
		dma_sync_single_for_device(fspi->dev, fspi->dma_buffer, len, DMA_TO_DEVICE);
	}

#ifdef ROCKCHIP_FSPI_VERBOSE
	start_time = ktime_get();
#endif
	ret = fspi_nm_fifo_transfer_dma(fspi, fspi->dma_buffer, len);
	if (!wait_for_completion_timeout(&fspi->cp, msecs_to_jiffies(2000))) {
		dev_err(fspi->dev, "DMA wait for transfer finish timeout\n");
		ret = -ETIMEDOUT;
	}
#ifdef ROCKCHIP_FSPI_VERBOSE
	end_time = ktime_get();
	us = ktime_to_us(ktime_sub(end_time, start_time));
	dev_err(fspi->dev, "io %dB %ldus %ldKB/S\n", len, us, len * 1000 / us);
#endif

#ifdef ROCKCHIP_FSPI_VERBOSE
	start_time = ktime_get();
#endif
	if (xfer->rx_buf) {
		dma_sync_single_for_cpu(fspi->dev, fspi->dma_buffer, len, DMA_FROM_DEVICE);
		memcpy(xfer->rx_buf, fspi->buffer, len);
	}
#ifdef ROCKCHIP_FSPI_VERBOSE
	end_time = ktime_get();
	us = ktime_to_us(ktime_sub(end_time, start_time));
	dev_err(fspi->dev, "cp %dB %ldus %ldKB/S\n", len, us, len * 1000 / us);
#endif

	return ret;
}

static int fspi_nm_xfer_done(struct fspi_nm *fspi, u32 timeout_us)
{
	int ret = 0;
	u32 status;

	/*
	 * There is very little data left in fifo, and the controller will
	 * complete the transmission in a short period of time.
	 */
	ret = readl_poll_timeout(fspi->regbase + FSPI_SR, status,
				 !(status & FSPI_SR_IS_BUSY),
				 0, 10);
	if (!ret)
		return 0;

	ret = readl_poll_timeout(fspi->regbase + FSPI_SR, status,
				 !(status & FSPI_SR_IS_BUSY),
				 20, timeout_us);
	if (ret) {
		dev_err(fspi->dev, "wait fspi idle timeout\n");
		print_hex_dump(KERN_WARNING, "fspi", DUMP_PREFIX_OFFSET, 4,
			       4, fspi->regbase, 0x104, 0);
		fspi_nm_reset(fspi);

		ret = -EIO;
	}

	return ret;
}

static int fspi_nm_init(struct fspi_nm *fspi)
{
	u32 reg;

	fspi->version = readl(fspi->regbase + FSPI_VER) & 0xffff;
	writel(0, fspi->regbase + FSPI_CTRL);
	writel(0xFFFFFFFF, fspi->regbase + FSPI_ICLR);
	writel(0xFFFFFFFF, fspi->regbase + FSPI_IMR);
	writel(FSPI_LEN_CTRL_TRB_SEL, fspi->regbase + FSPI_LEN_CTRL);
	if (fspi->version >= FSPI_VER_8 && fspi->sclk_x2_bypass) {
		reg = readl(fspi->regbase + FSPI_EXT_CTRL);
		reg |= FSPI_SCLK_X2_BYPASS;
		writel(reg, fspi->regbase + FSPI_EXT_CTRL);
	} else {
		fspi->sclk_x2_bypass = false;
	}
	if (fspi->version < FSPI_VER_13)
		fspi->force_cmd = true;
	else
		writel(0x1, fspi->regbase + FSPI_SPI_MODE);
	if (readl(fspi->regbase + FSPI_VER) & FSPI_CAP_X8)
		fspi->support_octa = true;
	if (fspi->use_dma)
		fspi_nm_irq_unmask(fspi, FSPI_IMR_DMA);

	fspi_nm_set_delay_lines(fspi, fspi->dll_cells);

	return 0;
}

static irqreturn_t rockchip_fspi_nm_irq_handler(int irq, void *dev_id)
{
	struct fspi_nm *fspi = dev_id;
	u32 reg;

	reg = readl(fspi->regbase + FSPI_RISR);

	/* Clear interrupt */
	writel(reg, fspi->regbase + FSPI_ICLR);

	if (reg & FSPI_RISR_DMA) {
		complete(&fspi->cp);

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static int rockchip_fspi_nm_transfer_one(struct spi_controller *ctlr,
					     struct spi_device *spi,
					     struct spi_transfer *xfer)
{
	struct fspi_nm *fspi = spi_controller_get_devdata(ctlr);
	u32 len = xfer->len;
	int ret;

	if (xfer->speed_hz != fspi->speed) {
		fspi->speed = xfer->speed_hz;
		fspi_nm_clk_set_rate(fspi, fspi->speed);
	}

	fspi_nm_xfer_setup(fspi, xfer);
	if (fspi->use_dma && len >= FSPI_DMA_TRANS_THRETHOLD && !(len & 0x3)) {
		init_completion(&fspi->cp);
		ret = fspi_nm_xfer_data_dma(fspi, xfer);
	} else {
		ret = fspi_nm_xfer_data_poll(fspi, xfer);
	}

	if (ret) {
		dev_err(fspi->dev, "xfer data failed ret %d\n", ret);
		fspi_nm_reset(fspi);
		return -EIO;
	}

	return fspi_nm_xfer_done(fspi, 100000);
}

static int rockchip_fspi_nm_setup(struct spi_device *spi)
{
	struct fspi_nm *fspi = spi_controller_get_devdata(spi->controller);
	u32 tx_ctrl = 0, rx_ctrl = 0, com_ctrl = 0;
	u32 mode = spi->mode;

	if ((mode & 0x3) == 1 || (mode & 0x3) == 2) {
		dev_err(fspi->dev, "unsupported mode %d\n", mode & 0x3);
		return -EINVAL;
	}

	if (mode & 0x3)
		com_ctrl = FSPI_CTRL_SPIM_3;

	if (mode & SPI_TX_DUAL)
		tx_ctrl |= FSPI_CTRL_DUAL_MODE;
	else if (mode & SPI_TX_QUAD)
		tx_ctrl |= FSPI_CTRL_QUAD_MODE;
	else if (mode & SPI_TX_OCTAL)
		tx_ctrl |= FSPI_CTRL_OCTAL_MODE;

	if (mode & SPI_RX_DUAL)
		rx_ctrl |= FSPI_CTRL_DUAL_MODE;
	else if (mode & SPI_RX_QUAD)
		rx_ctrl |= FSPI_CTRL_QUAD_MODE;
	else if (mode & SPI_RX_OCTAL)
		rx_ctrl |= FSPI_CTRL_OCTAL_MODE;

	fspi->tx_ctrl = tx_ctrl | com_ctrl;
	fspi->rx_ctrl = rx_ctrl | com_ctrl;

	fspi_nm_set_delay_lines(fspi, fspi->dll_cells);

	return 0;
}

static size_t rockchip_fspi_nm_max_transfer_size(struct spi_device *spi)
{
	return FSPI_MAX_IOSIZE;
}

static int fspi_nm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct spi_controller *host;
	struct resource *res;
	struct fspi_nm *fspi;
	int ret;

	host = devm_spi_alloc_master(&pdev->dev, sizeof(*fspi));
	if (!host)
		return -ENOMEM;

	fspi = spi_controller_get_devdata(host);
	fspi->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	fspi->regbase = devm_ioremap_resource(dev, res);
	if (IS_ERR(fspi->regbase))
		return PTR_ERR(fspi->regbase);

	if (!has_acpi_companion(&pdev->dev))
		fspi->clk = devm_clk_get(&pdev->dev, "clk_sfc");
	if (IS_ERR(fspi->clk)) {
		dev_err(&pdev->dev, "Failed to get fspi interface clk\n");
		return PTR_ERR(fspi->clk);
	}

	if (!has_acpi_companion(&pdev->dev))
		fspi->hclk = devm_clk_get(&pdev->dev, "hclk_sfc");
	if (IS_ERR(fspi->hclk)) {
		dev_err(&pdev->dev, "Failed to get fspi ahb clk\n");
		return PTR_ERR(fspi->hclk);
	}

	fspi->sclk_x2_bypass = of_property_read_bool(fspi->dev->of_node, "rockchip,sclk-x2-bypass");

	device_property_read_u32(&pdev->dev, "rockchip,max-dll", &fspi->max_dll_cells);
	if (fspi->max_dll_cells > FSPI_MAX_DLL_CELLS)
		fspi->max_dll_cells = FSPI_MAX_DLL_CELLS;

	device_property_read_u32(&pdev->dev, "rockchip,dll-cells", &fspi->dll_cells);
	if (fspi->dll_cells > fspi->max_dll_cells)
		fspi->dll_cells = fspi->max_dll_cells;

	fspi->use_dma = !of_property_read_bool(fspi->dev->of_node, "rockchip,fspi-no-dma");
	if (fspi->use_dma) {
		fspi->buffer = (u8 *)devm_get_free_pages(dev, GFP_KERNEL | GFP_DMA32,
							get_order(FSPI_MAX_IOSIZE));
		if (!fspi->buffer)
			return -ENOMEM;
		fspi->dma_buffer = virt_to_phys(fspi->buffer);
	}

	platform_set_drvdata(pdev, fspi);

	ret = clk_prepare_enable(fspi->hclk);
	if (ret) {
		dev_err(&pdev->dev, "Failed to enable ahb clk\n");
		return ret;
	}

	ret = clk_prepare_enable(fspi->clk);
	if (ret) {
		dev_err(&pdev->dev, "Failed to enable interface clk\n");
		goto err_clk;
	}

	ret = platform_get_irq(pdev, 0);
	if (ret < 0)
		goto err_irq;

	ret = devm_request_irq(dev, ret, rockchip_fspi_nm_irq_handler, 0, pdev->name, fspi);
	if (ret)
		goto err_irq;

	fspi_nm_init(fspi);

	host->flags = SPI_CONTROLLER_HALF_DUPLEX;
	host->dev.of_node = pdev->dev.of_node;
	host->max_speed_hz = FSPI_MAX_SPEED;
	host->num_chipselect = FSPI_MAX_CHIPSELECT_NUM;
	host->mode_bits = SPI_MODE_0 | SPI_MODE_3 | SPI_TX_DUAL | SPI_RX_DUAL |
		SPI_TX_QUAD | SPI_RX_QUAD;
	if (fspi->support_octa)
		host->mode_bits |= SPI_TX_OCTAL | SPI_RX_OCTAL;
	host->transfer_one = rockchip_fspi_nm_transfer_one;
	host->setup = rockchip_fspi_nm_setup;
	host->max_transfer_size = rockchip_fspi_nm_max_transfer_size;

	ret = devm_spi_register_controller(dev, host);
	if (ret)
		goto err_irq;

	dev_info(&pdev->dev, "dll=%d\n", fspi->dll_cells);
	if (fspi->force_cmd)
		dev_warn(&pdev->dev, "only support spi memory protocol, the first byte is cmd\n");

	return 0;
err_irq:
	clk_disable_unprepare(fspi->clk);
err_clk:
	clk_disable_unprepare(fspi->hclk);
	return ret;
}

static int fspi_nm_resume(struct device *dev)
{
	struct fspi_nm *fspi = dev_get_drvdata(dev);

	fspi_nm_init(fspi);

	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(fspi_nm_pm_ops, NULL, fspi_nm_resume);

static const struct of_device_id fspi_nm_dt_ids[] = {
	{ .compatible = "rockchip,fspi-nm" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, fspi_nm_dt_ids);

static struct platform_driver fspi_nm_driver = {
	.driver = {
		.name	= "rockchip-fspi-nm",
		.of_match_table = fspi_nm_dt_ids,
		.pm = pm_sleep_ptr(&fspi_nm_pm_ops),
	},
	.probe	= fspi_nm_probe,
};
module_platform_driver(fspi_nm_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Rockchip FSPI Controller Under SPI Transmission Protocol Driver");
MODULE_AUTHOR("Jon Lin <Jon.lin@rock-chips.com>");
