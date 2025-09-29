// SPDX-License-Identifier: GPL-2.0-only
/*
 * Rockchip Serial Flash Controller Driver
 *
 * Copyright (c) 2017-2021, Rockchip Electronics Co., Ltd.
 * Author: Shawn Lin <shawn.lin@rock-chips.com>
 *	   Chris Morgan <macroalpha82@gmail.com>
 *	   Jon Lin <Jon.lin@rock-chips.com>
 */

#include <linux/acpi.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/dma-mapping.h>
#include <linux/iopoll.h>
#include <linux/interrupt.h>
#include <linux/mfd/syscon.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/spi/spi-mem.h>
#include <linux/of_gpio.h>

/* System control */
#define SFC_CTRL			0x0
#define  SFC_CTRL_PHASE_SEL_NEGETIVE	BIT(1)
#define  SFC_CTRL_DTR_MODE		BIT(2)
#define  SFC_CTRL_CMD_BITS_SHIFT	8
#define  SFC_CTRL_ADDR_BITS_SHIFT	10
#define  SFC_CTRL_DATA_BITS_SHIFT	12
#define  SFC_CTRL_DTR_MODE_BY_DEVICE	BIT(17)
#define  SFC_CTRL_CMD_STR_SHIFT		19
#define  SFC_CTRL_ADDR_STR_SHIFT	20
#define  SFC_CTRL_CMD_CTRL_CMD_EXT	(2 << 27)
#define  SFC_CTRL_WPEN			BIT(29)

/* Interrupt mask */
#define SFC_IMR				0x4
#define  SFC_IMR_RX_FULL		BIT(0)
#define  SFC_IMR_RX_UFLOW		BIT(1)
#define  SFC_IMR_TX_OFLOW		BIT(2)
#define  SFC_IMR_TX_EMPTY		BIT(3)
#define  SFC_IMR_TRAN_FINISH		BIT(4)
#define  SFC_IMR_BUS_ERR		BIT(5)
#define  SFC_IMR_NSPI_ERR		BIT(6)
#define  SFC_IMR_DMA			BIT(7)

/* Interrupt clear */
#define SFC_ICLR			0x8
#define  SFC_ICLR_RX_FULL		BIT(0)
#define  SFC_ICLR_RX_UFLOW		BIT(1)
#define  SFC_ICLR_TX_OFLOW		BIT(2)
#define  SFC_ICLR_TX_EMPTY		BIT(3)
#define  SFC_ICLR_TRAN_FINISH		BIT(4)
#define  SFC_ICLR_BUS_ERR		BIT(5)
#define  SFC_ICLR_NSPI_ERR		BIT(6)
#define  SFC_ICLR_DMA			BIT(7)

/* FIFO threshold level */
#define SFC_FTLR			0xc
#define  SFC_FTLR_TX_SHIFT		0
#define  SFC_FTLR_TX_MASK		0x1f
#define  SFC_FTLR_RX_SHIFT		8
#define  SFC_FTLR_RX_MASK		0x1f

/* Reset FSM and FIFO */
#define SFC_RCVR			0x10
#define  SFC_RCVR_RESET			BIT(0)

/* Enhanced mode */
#define SFC_AX				0x14

/* Address Bit number */
#define SFC_ABIT			0x18

/* Interrupt status */
#define SFC_ISR				0x1c
#define  SFC_ISR_RX_FULL_SHIFT		BIT(0)
#define  SFC_ISR_RX_UFLOW_SHIFT		BIT(1)
#define  SFC_ISR_TX_OFLOW_SHIFT		BIT(2)
#define  SFC_ISR_TX_EMPTY_SHIFT		BIT(3)
#define  SFC_ISR_TX_FINISH_SHIFT	BIT(4)
#define  SFC_ISR_BUS_ERR_SHIFT		BIT(5)
#define  SFC_ISR_NSPI_ERR_SHIFT		BIT(6)
#define  SFC_ISR_DMA_SHIFT		BIT(7)

/* FIFO status */
#define SFC_FSR				0x20
#define  SFC_FSR_TX_IS_FULL		BIT(0)
#define  SFC_FSR_TX_IS_EMPTY		BIT(1)
#define  SFC_FSR_RX_IS_EMPTY		BIT(2)
#define  SFC_FSR_RX_IS_FULL		BIT(3)
#define  SFC_FSR_TXLV_MASK		GENMASK(13, 8)
#define  SFC_FSR_TXLV_SHIFT		8
#define  SFC_FSR_RXLV_MASK		GENMASK(20, 16)
#define  SFC_FSR_RXLV_SHIFT		16

/* FSM status */
#define SFC_SR				0x24
#define  SFC_SR_IS_IDLE			0x0
#define  SFC_SR_IS_BUSY			0x1

/* Raw interrupt status */
#define SFC_RISR			0x28
#define  SFC_RISR_RX_FULL		BIT(0)
#define  SFC_RISR_RX_UNDERFLOW		BIT(1)
#define  SFC_RISR_TX_OVERFLOW		BIT(2)
#define  SFC_RISR_TX_EMPTY		BIT(3)
#define  SFC_RISR_TRAN_FINISH		BIT(4)
#define  SFC_RISR_BUS_ERR		BIT(5)
#define  SFC_RISR_NSPI_ERR		BIT(6)
#define  SFC_RISR_DMA			BIT(7)

/* Version */
#define SFC_VER				0x2C
#define  SFC_VER_3			0x3
#define  SFC_VER_4			0x4
#define  SFC_VER_5			0x5
#define  SFC_VER_6			0x6
#define  SFC_VER_8			0x8
#define  SFC_VER_9			0x9

/* Ext ctrl */
#define SFC_EXT_CTRL			0x34
#define  SFC_SCLK_X2_BYPASS		BIT(24)

/* Delay line controller resiter */
#define SFC_DLL_CTRL0			0x3C
#define SFC_DLL_CTRL0_SCLK_SMP_DLL	BIT(15)
#define SFC_DLL_CTRL0_DLL_MAX_VER4	0xFFU
#define SFC_DLL_CTRL0_DLL_MAX_VER5	0x1FFU

/* Dummy Cycle Control Register */
#define SFC_DUMM_CTRL			0x74
#define SFC_DUMMY_CTRL_SEL		BIT(0)
#define SFC_DUMMY_CTRL_EXT_SHIFT	1

/* Command Extend Register */
#define SFC_CMD_EXT			0x78

/* Master trigger */
#define SFC_DMA_TRIGGER			0x80
#define SFC_DMA_TRIGGER_START		1

/* Src or Dst addr for master */
#define SFC_DMA_ADDR			0x84

/* Length control register extension 32GB */
#define SFC_LEN_CTRL			0x88
#define SFC_LEN_CTRL_TRB_SEL		1
#define SFC_LEN_EXT			0x8C

/* Command */
#define SFC_CMD				0x100
#define  SFC_CMD_IDX_SHIFT		0
#define  SFC_CMD_DUMMY_SHIFT		8
#define  SFC_CMD_DIR_SHIFT		12
#define  SFC_CMD_DIR_RD			0
#define  SFC_CMD_DIR_WR			1
#define  SFC_CMD_ADDR_SHIFT		14
#define  SFC_CMD_ADDR_0BITS		0
#define  SFC_CMD_ADDR_24BITS		1
#define  SFC_CMD_ADDR_32BITS		2
#define  SFC_CMD_ADDR_XBITS		3
#define  SFC_CMD_TRAN_BYTES_SHIFT	16
#define  SFC_CMD_CS_SHIFT		30

/* Address */
#define SFC_ADDR			0x104

/* Data */
#define SFC_DATA			0x108

#define SFC_CS1_REG_OFFSET		0x200

#define SFC_MAX_CHIPSELECT_NUM		2

#define SFC_MAX_IOSIZE_VER3		(512 * 31)
/* Although up to 4GB, 64KB is enough with less mem reserved */
#define SFC_MAX_IOSIZE_VER4		(0x10000U)

/* DMA is only enabled for large data transmission */
#define SFC_DMA_TRANS_THRETHOLD		(0x40)

/* Maximum clock values from datasheet suggest keeping clock value under
 * 150MHz. No minimum or average value is suggested.
 */
#define SFC_MAX_SPEED		(150 * 1000 * 1000)
#define SFC_DLL_THRESHOLD_RATE	(50 * 1000 * 1000)

#define SFC_DLL_TRANING_STEP		10	/* Training step */
#define SFC_DLL_TRANING_VALID_WINDOW	80	/* Valid DLL winbow */

#define ROCKCHIP_AUTOSUSPEND_DELAY	2000

struct rockchip_sfc_powergood {
	bool	valid;
	u32	grf_offset;
	u8	bits_mask;
};

struct rockchip_sfc_data {
	struct rockchip_sfc_powergood powergood;
};

struct rockchip_sfc {
	struct device *dev;
	void __iomem *regbase;
	struct clk *hclk;
	struct clk *clk;
	u32 speed[SFC_MAX_CHIPSELECT_NUM];
	u32 cur_speed;
	u32 cur_real_speed;
	/* virtual mapped addr for dma_buffer */
	void *buffer;
	dma_addr_t dma_buffer;
	struct completion cp;
	bool use_dma;
	bool sclk_x2_bypass;
	u32 max_iosize;
	u32 max_dll_cells;
	u32 dll_cells[SFC_MAX_CHIPSELECT_NUM];
	u16 version;
	struct gpio_desc *rst_gpio;
	struct gpio_desc **cs_gpiods;
	struct spi_master *master;
	struct regmap *grf;
	struct rockchip_sfc_data *data;
};

static int rockchip_sfc_reset(struct rockchip_sfc *sfc)
{
	int err;
	u32 status;

	writel_relaxed(SFC_RCVR_RESET, sfc->regbase + SFC_RCVR);

	err = readl_poll_timeout(sfc->regbase + SFC_RCVR, status,
				 !(status & SFC_RCVR_RESET), 20,
				 jiffies_to_usecs(HZ));
	if (err)
		dev_err(sfc->dev, "SFC reset never finished\n");

	/* Still need to clear the masked interrupt from RISR */
	writel_relaxed(0xFFFFFFFF, sfc->regbase + SFC_ICLR);

	dev_dbg(sfc->dev, "reset\n");

	return err;
}

static u16 rockchip_sfc_get_version(struct rockchip_sfc *sfc)
{
	return  (u16)(readl(sfc->regbase + SFC_VER) & 0xffff);
}

static u32 rockchip_sfc_get_max_iosize(struct rockchip_sfc *sfc)
{
	if (sfc->version >= SFC_VER_4)
		return SFC_MAX_IOSIZE_VER4;

	return SFC_MAX_IOSIZE_VER3;
}

static u32 rockchip_sfc_get_max_dll_cells(struct rockchip_sfc *sfc)
{
	if (sfc->max_dll_cells)
		return sfc->max_dll_cells;

	if (sfc->version > SFC_VER_4)
		return SFC_DLL_CTRL0_DLL_MAX_VER5;
	else if (sfc->version == SFC_VER_4)
		return SFC_DLL_CTRL0_DLL_MAX_VER4;
	else
		return 0;
}

static void rockchip_sfc_set_delay_lines(struct rockchip_sfc *sfc, u16 cells, u8 cs)
{
	u16 cell_max = (u16)rockchip_sfc_get_max_dll_cells(sfc);
	u32 val = 0;

	if (cells > cell_max)
		cells = cell_max;

	if (cells)
		val = SFC_DLL_CTRL0_SCLK_SMP_DLL | cells;

	writel(val, sfc->regbase + cs * SFC_CS1_REG_OFFSET + SFC_DLL_CTRL0);
}

static int rockchip_sfc_clk_set_rate(struct rockchip_sfc *sfc, unsigned long  speed)
{
	if (sfc->version < SFC_VER_8 || sfc->sclk_x2_bypass)
		return clk_set_rate(sfc->clk, speed);
	else
		return clk_set_rate(sfc->clk, speed * 2);
}

static unsigned long rockchip_sfc_clk_get_rate(struct rockchip_sfc *sfc)
{
	if (sfc->version < SFC_VER_8 || sfc->sclk_x2_bypass)
		return clk_get_rate(sfc->clk);
	else
		return clk_get_rate(sfc->clk) / 2;
}

static void rockchip_sfc_irq_unmask(struct rockchip_sfc *sfc, u32 mask)
{
	u32 reg;

	/* Enable transfer complete interrupt */
	reg = readl(sfc->regbase + SFC_IMR);
	reg &= ~mask;
	writel(reg, sfc->regbase + SFC_IMR);
}

static void rockchip_sfc_irq_mask(struct rockchip_sfc *sfc, u32 mask)
{
	u32 reg;

	/* Disable transfer finish interrupt */
	reg = readl(sfc->regbase + SFC_IMR);
	reg |= mask;
	writel(reg, sfc->regbase + SFC_IMR);
}

static int rockchip_sfc_init(struct rockchip_sfc *sfc)
{
	u32 reg;

	writel(0, sfc->regbase + SFC_CTRL);
	writel(0xFFFFFFFF, sfc->regbase + SFC_ICLR);
	rockchip_sfc_irq_mask(sfc, 0xFFFFFFFF);
	if (rockchip_sfc_get_version(sfc) >= SFC_VER_4)
		writel(SFC_LEN_CTRL_TRB_SEL, sfc->regbase + SFC_LEN_CTRL);
	if (rockchip_sfc_get_version(sfc) >= SFC_VER_8 && sfc->sclk_x2_bypass) {
		reg = readl(sfc->regbase + SFC_EXT_CTRL);
		reg |= SFC_SCLK_X2_BYPASS;
		writel(reg, sfc->regbase + SFC_EXT_CTRL);
	}

	return 0;
}

static int rockchip_sfc_wait_txfifo_ready(struct rockchip_sfc *sfc, u32 timeout_us)
{
	int ret = 0;
	u32 status;

	ret = readl_poll_timeout(sfc->regbase + SFC_FSR, status,
				 status & SFC_FSR_TXLV_MASK, 0,
				 timeout_us);
	if (ret) {
		dev_dbg(sfc->dev, "sfc wait tx fifo timeout\n");

		return -ETIMEDOUT;
	}

	return (status & SFC_FSR_TXLV_MASK) >> SFC_FSR_TXLV_SHIFT;
}

static int rockchip_sfc_wait_rxfifo_ready(struct rockchip_sfc *sfc, u32 timeout_us)
{
	int ret = 0;
	u32 status;

	ret = readl_poll_timeout(sfc->regbase + SFC_FSR, status,
				 status & SFC_FSR_RXLV_MASK, 0,
				 timeout_us);
	if (ret) {
		print_hex_dump(KERN_WARNING, "sfc", DUMP_PREFIX_OFFSET, 4, 4, sfc->regbase, 0x104, 0);
		dev_err(sfc->dev, "sfc wait rx fifo timeout\n");

		return -ETIMEDOUT;
	}

	return (status & SFC_FSR_RXLV_MASK) >> SFC_FSR_RXLV_SHIFT;
}

static void rockchip_sfc_adjust_op_work(struct spi_mem_op *op)
{
	if (unlikely(op->dummy.nbytes && !op->addr.nbytes)) {
		/*
		 * SFC not support output DUMMY cycles right after CMD cycles, so
		 * treat it as ADDR cycles.
		 */
		op->addr.nbytes = op->dummy.nbytes;
		op->addr.buswidth = op->dummy.buswidth;
		op->addr.val = 0xFFFFFFFFF;

		op->dummy.nbytes = 0;
	}
}

static int rockchip_sfc_xfer_setup(struct rockchip_sfc *sfc,
				   struct spi_mem *mem,
				   const struct spi_mem_op *op,
				   u32 len)
{
	u32 ctrl = 0, cmd = 0, cmd_ext = 0, dummy_ext = 0;
	u8 cs = mem->spi->chip_select;
	u32 voltage;

	/* set CMD */
	if (op->cmd.nbytes == 2) {
		cmd_ext = op->cmd.opcode;
		ctrl |= SFC_CTRL_CMD_CTRL_CMD_EXT;
	} else {
		cmd = op->cmd.opcode;
	}

	ctrl |= (find_first_bit((unsigned long *)&op->cmd.buswidth, 8) << SFC_CTRL_CMD_BITS_SHIFT);
	ctrl |= (!op->cmd.dtr) << SFC_CTRL_CMD_STR_SHIFT;

	/* set ADDR */
	if (op->addr.nbytes) {
		if (op->addr.nbytes == 4) {
			cmd |= SFC_CMD_ADDR_32BITS << SFC_CMD_ADDR_SHIFT;
		} else if (op->addr.nbytes == 3) {
			cmd |= SFC_CMD_ADDR_24BITS << SFC_CMD_ADDR_SHIFT;
		} else {
			cmd |= SFC_CMD_ADDR_XBITS << SFC_CMD_ADDR_SHIFT;
			writel(op->addr.nbytes * 8 - 1, sfc->regbase + cs * SFC_CS1_REG_OFFSET + SFC_ABIT);
			dev_dbg(sfc->dev, "sfc addrxbits=%d\n", op->addr.nbytes * 8 - 1);
		}

		ctrl |= (find_first_bit((unsigned long *)&op->addr.buswidth, 8) << SFC_CTRL_ADDR_BITS_SHIFT);
	}
	ctrl |= (!op->addr.dtr) << SFC_CTRL_ADDR_STR_SHIFT;

	/* set DUMMY */
	if (op->dummy.nbytes) {
		if (op->dummy.buswidth == 8)
			dummy_ext |= ((op->dummy.nbytes / 2) << SFC_DUMMY_CTRL_EXT_SHIFT | SFC_DUMMY_CTRL_SEL); /* dtr */
		else if (op->dummy.buswidth == 4)
			cmd |= op->dummy.nbytes * 2 << SFC_CMD_DUMMY_SHIFT;
		else if (op->dummy.buswidth == 2)
			cmd |= op->dummy.nbytes * 4 << SFC_CMD_DUMMY_SHIFT;
		else
			cmd |= op->dummy.nbytes * 8 << SFC_CMD_DUMMY_SHIFT;
	}

	/* set DATA */
	if (sfc->version >= SFC_VER_4) /* Clear it if no data to transfer */
		writel(len, sfc->regbase + SFC_LEN_EXT);
	else
		cmd |= len << SFC_CMD_TRAN_BYTES_SHIFT;

	if ((len && op->data.dir == SPI_MEM_DATA_OUT) || (!len && op->addr.nbytes))
		cmd |= SFC_CMD_DIR_WR << SFC_CMD_DIR_SHIFT;
	if (len)
		ctrl |= (find_first_bit((unsigned long *)&op->data.buswidth, 8) << SFC_CTRL_DATA_BITS_SHIFT);

	/* set the Controller */
	ctrl |= SFC_CTRL_PHASE_SEL_NEGETIVE | SFC_CTRL_WPEN;
	if (op->cmd.buswidth > 1)
		ctrl |= SFC_CTRL_WPEN;

	/* Workaround, binding dqs with buswidth 8 */
	if (op->cmd.buswidth == 8)
		ctrl |= (SFC_CTRL_DTR_MODE | SFC_CTRL_DTR_MODE_BY_DEVICE);

	cmd |= cs << SFC_CMD_CS_SHIFT;

	dev_dbg(sfc->dev, "sfc cmd.nbytes=%x(x%d) addr.nbytes=%x(x%d) dummy.nbytes=%x(x%d)\n",
		op->cmd.nbytes, op->cmd.buswidth,
		op->addr.nbytes, op->addr.buswidth,
		op->dummy.nbytes, op->dummy.buswidth);
	dev_dbg(sfc->dev, "sfc ctrl=%x cmd=%x cmd_ext=%x addr=%llx dummy_ext=%x len=%x cs=%d\n",
		ctrl, cmd, cmd_ext, op->addr.val, cmd_ext, len, cs);

	if (sfc->data && sfc->data->powergood.valid) {
		if (regmap_read_poll_timeout(sfc->grf, sfc->data->powergood.grf_offset,
					     voltage, voltage & sfc->data->powergood.bits_mask,
					     1000, jiffies_to_usecs(HZ))) {
			dev_err(sfc->dev, "wait for powergood failed\n");
			return -EIO;
		}
	}

	if (cmd_ext)
		writel(cmd_ext, sfc->regbase + SFC_CMD_EXT);
	if (sfc->version >= SFC_VER_8)
		writel(dummy_ext, sfc->regbase + SFC_DUMM_CTRL);
	writel(ctrl, sfc->regbase + cs * SFC_CS1_REG_OFFSET + SFC_CTRL);
	writel(cmd, sfc->regbase + SFC_CMD);
	if (op->addr.nbytes)
		writel(op->addr.val, sfc->regbase + SFC_ADDR);

	return 0;
}

static int rockchip_sfc_write_fifo(struct rockchip_sfc *sfc, const u8 *buf, int len)
{
	u8 bytes = len & 0x3;
	u32 dwords;
	int tx_level;
	u32 write_words;
	u32 tmp = 0;

	dwords = len >> 2;
	while (dwords) {
		tx_level = rockchip_sfc_wait_txfifo_ready(sfc, 1000);
		if (tx_level < 0)
			return tx_level;
		write_words = min_t(u32, tx_level, dwords);
		iowrite32_rep(sfc->regbase + SFC_DATA, buf, write_words);
		buf += write_words << 2;
		dwords -= write_words;
	}

	/* write the rest non word aligned bytes */
	if (bytes) {
		tx_level = rockchip_sfc_wait_txfifo_ready(sfc, 1000);
		if (tx_level < 0)
			return tx_level;
		memcpy(&tmp, buf, bytes);
		writel(tmp, sfc->regbase + SFC_DATA);
	}

	return len;
}

static int rockchip_sfc_read_fifo(struct rockchip_sfc *sfc, u8 *buf, int len)
{
	u8 bytes = len & 0x3;
	u32 dwords;
	u8 read_words;
	int rx_level;
	int tmp;

	/* word aligned access only */
	dwords = len >> 2;
	while (dwords) {
		rx_level = rockchip_sfc_wait_rxfifo_ready(sfc, 1000);
		if (rx_level < 0)
			return rx_level;
		read_words = min_t(u32, rx_level, dwords);
		ioread32_rep(sfc->regbase + SFC_DATA, buf, read_words);
		buf += read_words << 2;
		dwords -= read_words;
	}

	/* read the rest non word aligned bytes */
	if (bytes) {
		rx_level = rockchip_sfc_wait_rxfifo_ready(sfc, 1000);
		if (rx_level < 0)
			return rx_level;
		tmp = readl(sfc->regbase + SFC_DATA);
		memcpy(buf, &tmp, bytes);
	}

	return len;
}

static int rockchip_sfc_fifo_transfer_dma(struct rockchip_sfc *sfc, dma_addr_t dma_buf, size_t len)
{
	writel(0xFFFFFFFF, sfc->regbase + SFC_ICLR);
	writel((u32)dma_buf, sfc->regbase + SFC_DMA_ADDR);
	writel(SFC_DMA_TRIGGER_START, sfc->regbase + SFC_DMA_TRIGGER);

	return len;
}

static int rockchip_sfc_xfer_data_poll(struct rockchip_sfc *sfc,
				       const struct spi_mem_op *op, u32 len)
{
	dev_dbg(sfc->dev, "sfc xfer_poll len=%x\n", len);

	if (op->data.dir == SPI_MEM_DATA_OUT)
		return rockchip_sfc_write_fifo(sfc, op->data.buf.out, len);
	else
		return rockchip_sfc_read_fifo(sfc, op->data.buf.in, len);
}

static int rockchip_sfc_xfer_data_dma(struct rockchip_sfc *sfc,
				      const struct spi_mem_op *op, u32 len)
{
	int ret;
#ifdef ROCKCHIP_SFC_VERBOSE
	ktime_t start_time;
	ktime_t end_time;
	unsigned long us = 0;
#endif

	dev_dbg(sfc->dev, "sfc xfer_dma len=%x\n", len);

	if (op->data.dir == SPI_MEM_DATA_OUT) {
		memcpy(sfc->buffer, op->data.buf.out, len);
		dma_sync_single_for_device(sfc->dev, sfc->dma_buffer, len, DMA_TO_DEVICE);
	}

#ifdef ROCKCHIP_SFC_VERBOSE
	start_time = ktime_get();
#endif
	ret = rockchip_sfc_fifo_transfer_dma(sfc, sfc->dma_buffer, len);
	if (!wait_for_completion_timeout(&sfc->cp, msecs_to_jiffies(2000))) {
		dev_err(sfc->dev, "DMA wait for transfer finish timeout\n");
		ret = -ETIMEDOUT;
	}
#ifdef ROCKCHIP_SFC_VERBOSE
	end_time = ktime_get();
	us = ktime_to_us(ktime_sub(end_time, start_time));
	dev_err(sfc->dev, "io %dB %ldus %ldKB/S\n", len, us, len * 1000 / us);
#endif
	rockchip_sfc_irq_mask(sfc, SFC_IMR_DMA);

#ifdef ROCKCHIP_SFC_VERBOSE
	start_time = ktime_get();
#endif
	if (op->data.dir == SPI_MEM_DATA_IN) {
		dma_sync_single_for_cpu(sfc->dev, sfc->dma_buffer, len, DMA_FROM_DEVICE);
		memcpy(op->data.buf.in, sfc->buffer, len);
	}
#ifdef ROCKCHIP_SFC_VERBOSE
	end_time = ktime_get();
	us = ktime_to_us(ktime_sub(end_time, start_time));
	dev_err(sfc->dev, "cp %dB %ldus %ldKB/S\n", len, us, len * 1000 / us);
#endif

	return ret;
}

static int rockchip_sfc_xfer_done(struct rockchip_sfc *sfc, u32 timeout_us)
{
	int ret = 0;
	u32 status;

	/*
	 * There is very little data left in fifo, and the controller will
	 * complete the transmission in a short period of time.
	 */
	ret = readl_poll_timeout(sfc->regbase + SFC_SR, status,
				 !(status & SFC_SR_IS_BUSY),
				 0, 10);
	if (!ret)
		return 0;

	ret = readl_poll_timeout(sfc->regbase + SFC_SR, status,
				 !(status & SFC_SR_IS_BUSY),
				 20, timeout_us);
	if (ret) {
		dev_err(sfc->dev, "wait sfc idle timeout\n");
		rockchip_sfc_reset(sfc);

		ret = -EIO;
	}

	return ret;
}

static void rockchip_sfc_set_cs_gpio(struct rockchip_sfc *sfc, u8 cs, bool enable)
{
	if (sfc->cs_gpiods) {
		if (has_acpi_companion(sfc->dev))
			gpiod_set_value_cansleep(sfc->cs_gpiods[cs], !enable);
		else
			/* Polarity handled by GPIO library */
			gpiod_set_value_cansleep(sfc->cs_gpiods[cs], enable);
	}
}

static int rockchip_sfc_exec_op_bypass(struct rockchip_sfc *sfc,
				       struct spi_mem *mem,
				       const struct spi_mem_op *op)
{
	u32 len = min_t(u32, op->data.nbytes, sfc->max_iosize);
	u8 cs = mem->spi->chip_select;
	u32 ret;

	rockchip_sfc_adjust_op_work((struct spi_mem_op *)op);
	rockchip_sfc_set_cs_gpio(sfc, cs, true);
	rockchip_sfc_xfer_setup(sfc, mem, op, len);
	ret = rockchip_sfc_xfer_data_poll(sfc, op, len);
	if (ret != len) {
		dev_err(sfc->dev, "xfer data failed ret %d\n", ret);

		return -EIO;
	}

	ret = rockchip_sfc_xfer_done(sfc, 100000);
	rockchip_sfc_set_cs_gpio(sfc, cs, false);

	return ret;
}

static void rockchip_sfc_delay_lines_tuning(struct rockchip_sfc *sfc, struct spi_mem *mem)
{
	struct spi_mem_op op = SPI_MEM_OP(SPI_MEM_OP_CMD(0x9F, 1),
						SPI_MEM_OP_NO_ADDR,
						SPI_MEM_OP_NO_DUMMY,
						SPI_MEM_OP_DATA_IN(3, NULL, 1));
	u8 id[3], id_temp[3];
	u16 cell_max = (u16)rockchip_sfc_get_max_dll_cells(sfc);
	u16 right, left = 0;
	u16 step = SFC_DLL_TRANING_STEP;
	bool dll_valid = false;
	u8 cs = mem->spi->chip_select;

	rockchip_sfc_clk_set_rate(sfc, SFC_DLL_THRESHOLD_RATE);
	op.data.buf.in = &id;
	rockchip_sfc_exec_op_bypass(sfc, mem, &op);
	if ((0xFF == id[0] && 0xFF == id[1]) ||
	    (0x00 == id[0] && 0x00 == id[1])) {
		dev_dbg(sfc->dev, "no dev, dll by pass\n");
		rockchip_sfc_clk_set_rate(sfc, sfc->speed[cs]);
		sfc->speed[cs] = SFC_DLL_THRESHOLD_RATE;

		return;
	}

	rockchip_sfc_clk_set_rate(sfc, sfc->speed[cs]);
	op.data.buf.in = &id_temp;
	for (right = 0; right <= cell_max; right += step) {
		int ret;

		rockchip_sfc_set_delay_lines(sfc, right, cs);
		rockchip_sfc_exec_op_bypass(sfc, mem, &op);
		dev_dbg(sfc->dev, "dll read flash id:%x %x %x\n",
			id_temp[0], id_temp[1], id_temp[2]);

		ret = memcmp(&id, &id_temp, 3);
		if (dll_valid && ret) {
			right -= step;

			break;
		}
		if (!dll_valid && !ret)
			left = right;

		if (!ret)
			dll_valid = true;

		/* Add cell_max to loop */
		if (right == cell_max)
			break;
		if (right + step > cell_max)
			right = cell_max - step;
	}

	if (dll_valid && (right - left) >= SFC_DLL_TRANING_VALID_WINDOW) {
		if (left == 0 && right < cell_max)
			sfc->dll_cells[cs] = left + (right - left) * 2 / 5;
		else
			sfc->dll_cells[cs] = left + (right - left) / 2;
	} else {
		sfc->dll_cells[cs] = 0;
	}

	if (sfc->dll_cells[cs]) {
		dev_dbg(sfc->dev, "%d %d %d dll training success in %dMHz max_cells=%u sfc_ver=%d\n",
			left, right, sfc->dll_cells[cs], sfc->speed[cs],
			rockchip_sfc_get_max_dll_cells(sfc), rockchip_sfc_get_version(sfc));
		rockchip_sfc_set_delay_lines(sfc, (u16)sfc->dll_cells[cs], cs);
	} else {
		dev_err(sfc->dev, "%d %d dll training failed in %dMHz, reduce the frequency\n",
			left, right, sfc->speed[cs]);
		rockchip_sfc_set_delay_lines(sfc, 0, cs);
		rockchip_sfc_clk_set_rate(sfc, SFC_DLL_THRESHOLD_RATE);
		mem->spi->max_speed_hz = SFC_DLL_THRESHOLD_RATE;
		sfc->cur_speed = SFC_DLL_THRESHOLD_RATE;
		sfc->cur_real_speed = rockchip_sfc_clk_get_rate(sfc);
		sfc->speed[cs] = SFC_DLL_THRESHOLD_RATE;
	}
}

static int rockchip_sfc_exec_mem_op(struct spi_mem *mem, const struct spi_mem_op *op)
{
	struct rockchip_sfc *sfc = spi_master_get_devdata(mem->spi->master);
	u32 len = op->data.nbytes;
	int ret;
	u8 cs = mem->spi->chip_select;

	ret = pm_runtime_get_sync(sfc->dev);
	if (ret < 0) {
		pm_runtime_put_noidle(sfc->dev);
		return ret;
	}

	if (unlikely(mem->spi->max_speed_hz != sfc->speed[cs]) &&
	    !has_acpi_companion(sfc->dev)) {
		ret = rockchip_sfc_clk_set_rate(sfc, mem->spi->max_speed_hz);
		if (ret)
			goto out;
		sfc->speed[cs] = mem->spi->max_speed_hz;
		sfc->cur_speed = mem->spi->max_speed_hz;
		sfc->cur_real_speed = rockchip_sfc_clk_get_rate(sfc);
		if (rockchip_sfc_get_version(sfc) >= SFC_VER_4) {
			if (sfc->cur_real_speed > SFC_DLL_THRESHOLD_RATE)
				rockchip_sfc_delay_lines_tuning(sfc, mem);
			else
				rockchip_sfc_set_delay_lines(sfc, 0, cs);
		}

		dev_dbg(sfc->dev, "set_freq=%dHz real_freq=%ldHz\n",
			sfc->speed[cs], rockchip_sfc_clk_get_rate(sfc));
	}

	rockchip_sfc_adjust_op_work((struct spi_mem_op *)op);
	rockchip_sfc_set_cs_gpio(sfc, cs, true);
	rockchip_sfc_xfer_setup(sfc, mem, op, len);
	if (len) {
		if (likely(sfc->use_dma) && len >= SFC_DMA_TRANS_THRETHOLD && !(len & 0x3)) {
			init_completion(&sfc->cp);
			rockchip_sfc_irq_unmask(sfc, SFC_IMR_DMA);
			ret = rockchip_sfc_xfer_data_dma(sfc, op, len);
		} else {
			ret = rockchip_sfc_xfer_data_poll(sfc, op, len);
		}

		if (ret != len) {
			dev_err(sfc->dev, "xfer data failed ret %d dir %d\n", ret, op->data.dir);
			rockchip_sfc_reset(sfc);
			ret = -EIO;
			goto out;
		}
	}

	ret = rockchip_sfc_xfer_done(sfc, 100000);
out:
	rockchip_sfc_set_cs_gpio(sfc, cs, false);
	pm_runtime_mark_last_busy(sfc->dev);
	pm_runtime_put_autosuspend(sfc->dev);

	return ret;
}

static int rockchip_sfc_adjust_op_size(struct spi_mem *mem, struct spi_mem_op *op)
{
	struct rockchip_sfc *sfc = spi_master_get_devdata(mem->spi->master);

	op->data.nbytes = min(op->data.nbytes, sfc->max_iosize);

	return 0;
}

static bool rockchip_sfc_supports_op(struct spi_mem *mem, const struct spi_mem_op *op)
{
	/*
	 * The number of address bytes should be equal to or less than 4 bytes.
	 */
	if (op->addr.nbytes > 4)
		return false;

	return spi_mem_default_supports_op(mem, op);
}

static const struct spi_controller_mem_ops rockchip_sfc_mem_ops = {
	.exec_op = rockchip_sfc_exec_mem_op,
	.adjust_op_size = rockchip_sfc_adjust_op_size,
	.supports_op = rockchip_sfc_supports_op,
};

static irqreturn_t rockchip_sfc_irq_handler(int irq, void *dev_id)
{
	struct rockchip_sfc *sfc = dev_id;
	u32 reg;

	reg = readl(sfc->regbase + SFC_RISR);

	/* Clear interrupt */
	writel_relaxed(reg, sfc->regbase + SFC_ICLR);

	if (reg & SFC_RISR_DMA) {
		complete(&sfc->cp);

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static int rockchip_sfc_get_gpio_descs(struct spi_controller *ctlr, struct rockchip_sfc *sfc)
{
	int nb, i;
	struct gpio_desc **cs;
	struct device *dev = &ctlr->dev;
	unsigned int num_cs_gpios = 0;

	nb = gpiod_count(dev, "sfc-cs");
	ctlr->num_chipselect = max_t(int, nb, ctlr->num_chipselect);

	if (nb == 0 || nb == -ENOENT)
		return 0;
	else if (nb < 0)
		return nb;

	cs = devm_kcalloc(dev, ctlr->num_chipselect, sizeof(*cs),
			  GFP_KERNEL);
	if (!cs)
		return -ENOMEM;
	sfc->cs_gpiods = cs;

	for (i = 0; i < nb; i++) {
		cs[i] = devm_gpiod_get_index_optional(dev, "sfc-cs", i,
						      GPIOD_OUT_LOW);
		if (IS_ERR(cs[i]))
			return PTR_ERR(cs[i]);

		if (cs[i]) {
			/*
			 * If we find a CS GPIO, name it after the device and
			 * chip select line.
			 */
			char *gpioname;

			gpioname = devm_kasprintf(dev, GFP_KERNEL, "%s CS%d",
						  dev_name(dev), i);
			if (!gpioname)
				return -ENOMEM;
			gpiod_set_consumer_name(cs[i], gpioname);
			num_cs_gpios++;
			continue;
		}
	}

	return 0;
}

static const struct rockchip_sfc_data rk3506_fspi_data = {
	.powergood = {
		.valid = true,
		.grf_offset = 0x100,
		.bits_mask = BIT(0),
	},
};

static const struct rockchip_sfc_data rv1126b_fspi_data = {
	.powergood = {
		.valid = true,
		.grf_offset = 0x30170,
		.bits_mask = BIT(0),
	},
};

static const struct of_device_id rockchip_sfc_dt_ids[] = {
	{ .compatible = "rockchip,fspi",},
	{ .compatible = "rockchip,rk3506-fspi", .data = &rk3506_fspi_data},
	{ .compatible = "rockchip,rv1126b-fspi", .data = &rv1126b_fspi_data},
	{ .compatible = "rockchip,sfc"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rockchip_sfc_dt_ids);

static int rockchip_sfc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct spi_master *master;
	struct resource *res;
	struct rockchip_sfc *sfc;
	int ret;
	u32 i, val;

	master = devm_spi_alloc_master(&pdev->dev, sizeof(*sfc));
	if (!master)
		return -ENOMEM;

	master->flags = SPI_MASTER_HALF_DUPLEX;
	master->mem_ops = &rockchip_sfc_mem_ops;
	master->dev.of_node = pdev->dev.of_node;
	master->max_speed_hz = SFC_MAX_SPEED;
	master->num_chipselect = SFC_MAX_CHIPSELECT_NUM;

	sfc = spi_master_get_devdata(master);
	sfc->dev = dev;
	sfc->master = master;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sfc->regbase = devm_ioremap_resource(dev, res);
	if (IS_ERR(sfc->regbase))
		return PTR_ERR(sfc->regbase);

	if (!has_acpi_companion(&pdev->dev))
		sfc->clk = devm_clk_get(&pdev->dev, "clk_sfc");
	if (IS_ERR(sfc->clk)) {
		dev_err(&pdev->dev, "Failed to get sfc interface clk\n");
		return PTR_ERR(sfc->clk);
	}

	if (!has_acpi_companion(&pdev->dev))
		sfc->hclk = devm_clk_get(&pdev->dev, "hclk_sfc");
	if (IS_ERR(sfc->hclk)) {
		dev_err(&pdev->dev, "Failed to get sfc ahb clk\n");
		return PTR_ERR(sfc->hclk);
	}

	if (has_acpi_companion(&pdev->dev)) {
		ret = device_property_read_u32(&pdev->dev, "clock-frequency", &val);
		if (ret) {
			dev_err(&pdev->dev, "Failed to find clock-frequency in ACPI\n");
			return ret;
		}
		for (i = 0; i < SFC_MAX_CHIPSELECT_NUM; i++)
			sfc->speed[i] = val;
	}

	sfc->use_dma = !of_property_read_bool(sfc->dev->of_node,
					      "rockchip,sfc-no-dma");
	sfc->sclk_x2_bypass = of_property_read_bool(sfc->dev->of_node,
						    "rockchip,sclk-x2-bypass");

	device_property_read_u32(&pdev->dev, "rockchip,max-dll", &sfc->max_dll_cells);
	if (sfc->max_dll_cells > SFC_DLL_CTRL0_DLL_MAX_VER5)
		sfc->max_dll_cells = SFC_DLL_CTRL0_DLL_MAX_VER5;

	ret = clk_prepare_enable(sfc->hclk);
	if (ret) {
		dev_err(&pdev->dev, "Failed to enable ahb clk\n");
		goto err_hclk;
	}

	ret = clk_prepare_enable(sfc->clk);
	if (ret) {
		dev_err(&pdev->dev, "Failed to enable interface clk\n");
		goto err_clk;
	}

	/* Find the irq */
	ret = platform_get_irq(pdev, 0);
	if (ret < 0)
		goto err_irq;

	ret = devm_request_irq(dev, ret, rockchip_sfc_irq_handler,
			       0, pdev->name, sfc);
	if (ret) {
		dev_err(dev, "Failed to request irq\n");

		goto err_irq;
	}

	sfc->data = (struct rockchip_sfc_data *)device_get_match_data(&pdev->dev);
	if (sfc->data) {
		sfc->grf = syscon_regmap_lookup_by_phandle_optional(dev->of_node, "rockchip,grf");
		if (IS_ERR_OR_NULL(sfc->grf)) {
			ret = -EINVAL;
			dev_err(dev, "Failed to find grf\n");

			goto err_irq;
		}
	}

	platform_set_drvdata(pdev, sfc);

	if (IS_ENABLED(CONFIG_ROCKCHIP_THUNDER_BOOT)) {
		u32 status;

		if (readl_poll_timeout(sfc->regbase + SFC_SR, status,
				       !(status & SFC_SR_IS_BUSY), 10,
				       5000 * USEC_PER_MSEC))
			dev_err(dev, "Wait for SFC idle timeout!\n");
	}

	ret = rockchip_sfc_get_gpio_descs(master, sfc);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get gpio_descs\n");
		goto err_irq;
	}

	ret = rockchip_sfc_init(sfc);
	if (ret)
		goto err_irq;

	sfc->version = rockchip_sfc_get_version(sfc);
	if (sfc->version == SFC_VER_9)
		sfc->version = SFC_VER_6;
	sfc->max_iosize = rockchip_sfc_get_max_iosize(sfc);

	master->mode_bits = SPI_TX_QUAD | SPI_TX_DUAL | SPI_RX_QUAD | SPI_RX_DUAL;
	if (sfc->version >= SFC_VER_8)
		master->mode_bits |= SPI_TX_OCTAL | SPI_RX_OCTAL;

	pm_runtime_set_autosuspend_delay(dev, ROCKCHIP_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_get_noresume(dev);

	if (sfc->use_dma) {
		sfc->buffer = (u8 *)__get_free_pages(GFP_KERNEL | GFP_DMA32, get_order(sfc->max_iosize));
		if (!sfc->buffer) {
			ret = -ENOMEM;
			goto err_dma;
		}
		sfc->dma_buffer = virt_to_phys(sfc->buffer);
	}

	sfc->rst_gpio = devm_gpiod_get_optional(&pdev->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(sfc->rst_gpio)) {
		dev_err(&pdev->dev, "invalid reset-gpios property in node\n");
		ret = PTR_ERR(sfc->rst_gpio);
		goto err_dma;
	} else if (sfc->rst_gpio) {
		dev_info(&pdev->dev, "reset OCTA Flash at first\n");
		gpiod_set_value_cansleep(sfc->rst_gpio, 1);
		mdelay(1);
		gpiod_set_value_cansleep(sfc->rst_gpio, 0);
		mdelay(1);
	}

	ret = spi_register_master(master);
	if (ret)
		goto err_register;

	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return 0;

err_register:
	free_pages((unsigned long)sfc->buffer, get_order(sfc->max_iosize));
err_dma:
	pm_runtime_disable(sfc->dev);
	pm_runtime_set_suspended(sfc->dev);
	pm_runtime_dont_use_autosuspend(sfc->dev);
err_irq:
	clk_disable_unprepare(sfc->clk);
err_clk:
	clk_disable_unprepare(sfc->hclk);
err_hclk:
	return ret;
}

static int rockchip_sfc_remove(struct platform_device *pdev)
{
	struct rockchip_sfc *sfc = platform_get_drvdata(pdev);
	struct spi_master *master = sfc->master;

	free_pages((unsigned long)sfc->buffer, get_order(sfc->max_iosize));
	spi_unregister_master(master);

	clk_disable_unprepare(sfc->clk);
	clk_disable_unprepare(sfc->hclk);

	return 0;
}

static int __maybe_unused rockchip_sfc_runtime_suspend(struct device *dev)
{
	struct rockchip_sfc *sfc = dev_get_drvdata(dev);

	clk_disable_unprepare(sfc->clk);
	clk_disable_unprepare(sfc->hclk);

	return 0;
}

static int __maybe_unused rockchip_sfc_runtime_resume(struct device *dev)
{
	struct rockchip_sfc *sfc = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(sfc->hclk);
	if (ret < 0)
		return ret;

	ret = clk_prepare_enable(sfc->clk);
	if (ret < 0)
		clk_disable_unprepare(sfc->hclk);

	return ret;
}

static int __maybe_unused rockchip_sfc_suspend(struct device *dev)
{
	pinctrl_pm_select_sleep_state(dev);

	return pm_runtime_force_suspend(dev);
}

static int __maybe_unused rockchip_sfc_resume(struct device *dev)
{
	struct rockchip_sfc *sfc = dev_get_drvdata(dev);
	int ret, i;

	ret = pm_runtime_force_resume(dev);
	if (ret < 0)
		return ret;

	pinctrl_pm_select_default_state(dev);

	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		pm_runtime_put_noidle(dev);
		return ret;
	}

	rockchip_sfc_init(sfc);
	for (i = 0; i < SFC_MAX_CHIPSELECT_NUM; i++) {
		if (sfc->dll_cells[i])
			rockchip_sfc_set_delay_lines(sfc, (u16)sfc->dll_cells[i], i);
	}

	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return 0;
}

static const struct dev_pm_ops rockchip_sfc_pm_ops = {
	SET_RUNTIME_PM_OPS(rockchip_sfc_runtime_suspend,
			   rockchip_sfc_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(rockchip_sfc_suspend, rockchip_sfc_resume)
};

static struct platform_driver rockchip_sfc_driver = {
	.driver = {
		.name	= "rockchip-sfc",
		.of_match_table = rockchip_sfc_dt_ids,
		.pm = &rockchip_sfc_pm_ops,
#ifdef CONFIG_ROCKCHIP_THUNDER_BOOT
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
#endif
	},
	.probe	= rockchip_sfc_probe,
	.remove	= rockchip_sfc_remove,
};
module_platform_driver(rockchip_sfc_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Rockchip Serial Flash Controller Driver");
MODULE_AUTHOR("Shawn Lin <shawn.lin@rock-chips.com>");
MODULE_AUTHOR("Chris Morgan <macromorgan@hotmail.com>");
MODULE_AUTHOR("Jon Lin <Jon.lin@rock-chips.com>");
