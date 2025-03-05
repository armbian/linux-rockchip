// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Driver for Rockchip Flexbus FSPI mode
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/acpi.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/dma-mapping.h>
#include <linux/iopoll.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/spi/spi-mem.h>

#include <dt-bindings/mfd/rockchip-flexbus.h>
#include <linux/mfd/rockchip-flexbus.h>

#define FLEXBUS_FSPI_ERR_ISR	(FLEXBUS_DMA_TIMEOUT_ISR | FLEXBUS_DMA_ERR_ISR |	\
				 FLEXBUS_TX_UDF_ISR | FLEXBUS_TX_OVF_ISR)

/* Flexbus definition */
#define FLEXBUS_REVISION_V9			(0x9)

#define FLEXBUS_QSPI_CMD_MAX			(0x8)

#define FLEXBUS_MAX_IOSIZE			(0x4000)
#define FLEXBUS_DMA_TIMEOUT_MS			(0x1000)

#define FLEXBUS_MAX_SPEED			(150 * 1000 * 1000)
#define FLEXBUS_DLL_THRESHOLD_RATE		(50 * 1000 * 1000)

#define FLEXBUS_DLL_TRANING_STEP		(10)	/* Training step */
#define FLEXBUS_DLL_TRANING_VALID_WINDOW	(80)	/* Valid DLL winbow */

#define FLEXBUS_DLL_CELLS			(0x7f)

#define FLEXBUS_MAX_CHIPSELECT_NUM		(1)
#define FLEXBUS_TX_WIDTH_4			(4)

struct rk_flexbus_fspi {
	struct device *dev;
	struct rockchip_flexbus *fb;
	u32 speed[FLEXBUS_MAX_CHIPSELECT_NUM];
	u32 cur_speed;
	u32 cur_real_speed;
	u8 *tx_buf;
	u8 *switch_buf;
	u8 *temp_buf;
	dma_addr_t dma_temp_buf;
	struct completion cp;
	u32 version;
	u32 max_iosize;
	u32 dll_cells[FLEXBUS_MAX_CHIPSELECT_NUM];
	struct gpio_desc **cs_gpiods;
	struct spi_controller *master;
};

struct rk_flexbus_fspi_xfer {
	u8 *cmd_buf;
	u32 cmd_cycles;
	dma_addr_t buf_addr;
	u32 buf_cycles;
};

static const unsigned char bit_reverse_table_256[] = {
	0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0,
	0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
	0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8,
	0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
	0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4,
	0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
	0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC,
	0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
	0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2,
	0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
	0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA,
	0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
	0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6,
	0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
	0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE,
	0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
	0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1,
	0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
	0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9,
	0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
	0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5,
	0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
	0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED,
	0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
	0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3,
	0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
	0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB,
	0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
	0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7,
	0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
	0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF,
	0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

static void flexbus_bit_revert(struct rk_flexbus_fspi *fspi, const u8 *in, u8 *out, u32 len)
{
	int i;

	for (i = 0; i < len; i++)
		out[i] = bit_reverse_table_256[in[i]];
}

static int flexbus_fspi_data_format(struct rk_flexbus_fspi *fspi,
				    const u8 *in, u8 in_lines,
				    u8 *out, u8 out_lines,
				    u32 cycles, u8 *temp_buf)
{
	int in_bits, out_bits, i;

	if (in_lines == 4 && out_lines == 1) {
		in_bits = cycles * 4;
		out_bits = cycles;
		memset(out, 0, (out_bits + 7) / 8);
		for (i = 0; i < in_bits; i += 4)
			if (test_bit(i + 1, (void *)in))
				__set_bit(i / 4, (void *)out);

		return in_bits;
	} else if (in_lines == 1 && out_lines == 4) {
		in_bits = cycles;
		out_bits = in_bits * 4;
		memset(out, 0, (out_bits + 7) / 8);
		flexbus_bit_revert(fspi, in, temp_buf, cycles / 8);
		for (i = 0; i < in_bits; i++)
			if (test_bit(i, (void *)temp_buf))
				__set_bit(i * 4, (void *)out);

		return in_bits;
	}

	return 0;
}

static int flexbus_data_format_order(struct rk_flexbus_fspi *fspi, u8 *in, u8 *out, u32 bytes)
{
	int i, loops, left;
	u32 *in32 = (u32 *)in;
	u32 *out32 = (u32 *)out;

	loops = bytes / 4;
	left = bytes % 4;
	for (i = 0; i < loops; i++)
		out32[i] = __swab32(in32[i]);
	dev_dbg(fspi->fb->dev, "%s %d %d %d\n", __func__, loops, left, i);
	switch (left) {
	case 1:
		out[i * 4] = in[i * 4];
		break;
	case 2:
		out[i * 4] = in[i * 4 + 1];
		out[i * 4 + 1] = in[i * 4];
		break;
	case 3:
		out[i * 4] = in[i * 4 + 2];
		out[i * 4 + 1] = in[i * 4 + 1];
		out[i * 4 + 2] = in[i * 4];
		break;
	default:
		break;
	}

	return 0;
}

static int rk_flexbus_fspi_init(struct rk_flexbus_fspi *fspi)
{
	u32 ctrl;

	fspi->fb->config->grf_config(fspi->fb, false, 0, 0);

	fspi->version = rockchip_flexbus_readl(fspi->fb, FLEXBUS_REVISION) >> 16;
	fspi->max_iosize = FLEXBUS_MAX_IOSIZE;

	rockchip_flexbus_writel(fspi->fb, FLEXBUS_TXWAT_START, 0x10);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_DMA_SRC_LEN0,
				fspi->max_iosize * FLEXBUS_TX_WIDTH_4);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_DMA_DST_LEN0,
				fspi->max_iosize * FLEXBUS_TX_WIDTH_4);

	if (fspi->version == FLEXBUS_REVISION_V9)
		ctrl = FLEXBUS_TX_CTL_MSB | fspi->fb->dfs_reg->dfs_4bit;
	else
		ctrl = FLEXBUS_TX_CTL_UNIT_BYTE | FLEXBUS_TX_CTL_MSB | fspi->fb->dfs_reg->dfs_1bit;
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_TX_CTL, ctrl);

	/* Using internal clk as sample clock */
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_SLAVE_MODE, 0);

	return 0;
}

static u16 rk_flexbus_fspi_get_max_dll_cells(struct rk_flexbus_fspi *fspi)
{
	return FLEXBUS_DLL_CELLS;
}

static void rk_flexbus_fspi_set_delay_lines(struct rk_flexbus_fspi *fspi, u16 cells, u8 cs)
{
	u16 cell_max = 256;

	if (cells > cell_max)
		cells = cell_max;

	if (cells) {
		rockchip_flexbus_writel(fspi->fb, FLEXBUS_DLL_EN, 1);
		rockchip_flexbus_writel(fspi->fb, FLEXBUS_DLL_NUM, cells);
	} else {
		rockchip_flexbus_writel(fspi->fb, FLEXBUS_DLL_EN, 0);
	}
}

static int rk_flexbus_fspi_clk_set_rate(struct rk_flexbus_fspi *fspi, unsigned long speed)
{
	return clk_set_rate(fspi->fb->clks[0].clk, speed * 2);
}

static unsigned long rk_flexbus_fspi_clk_get_rate(struct rk_flexbus_fspi *fspi)
{
	return clk_get_rate(fspi->fb->clks[0].clk) / 2;
}

static int rk_flexbus_fspi_send(struct rk_flexbus_fspi *fspi, struct rk_flexbus_fspi_xfer *cfg)
{
	int ret = 0, timeout_ms = FLEXBUS_DMA_TIMEOUT_MS;

	rockchip_flexbus_writel(fspi->fb, FLEXBUS_CSN_CFG, 0x10001);

	rockchip_flexbus_writel(fspi->fb, FLEXBUS_COM_CTL, FLEXBUS_TX_ONLY);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_IMR, FLEXBUS_TX_DONE_ISR);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_ICR, FLEXBUS_TX_DONE_ISR);

	rockchip_flexbus_writel(fspi->fb, FLEXBUS_TX_CMD_LEN, cfg->cmd_cycles);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_TX_CMD0, ((u32 *)fspi->tx_buf)[0]);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_TX_CMD1, ((u32 *)fspi->tx_buf)[1]);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_TX_NUM, cfg->cmd_cycles + cfg->buf_cycles);

	rockchip_flexbus_writel(fspi->fb, FLEXBUS_RX_CTL, 0);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_RX_NUM, 0);

	init_completion(&fspi->cp);

	rockchip_flexbus_writel(fspi->fb, FLEXBUS_DMA_SRC_ADDR0, cfg->buf_addr >> 2);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_ENR, FLEXBUS_TX_ENR);

	if (!wait_for_completion_timeout(&fspi->cp, msecs_to_jiffies(timeout_ms))) {
		dev_err(fspi->dev, "DMA wait for tx transfer finish timeout\n");
		ret = -ETIMEDOUT;
	}

	rockchip_flexbus_writel(fspi->fb, FLEXBUS_ENR, FLEXBUS_TX_DIS);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_ICR, FLEXBUS_TX_DONE_ISR);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_IMR, 0);

	rockchip_flexbus_writel(fspi->fb, FLEXBUS_CSN_CFG, 0x10000);

	return ret;
}

static int rk_flexbus_fspi_send_then_recv_114(struct rk_flexbus_fspi *fspi,
					      struct rk_flexbus_fspi_xfer *cfg,
					      bool trick)
{
	int ret = 0, timeout_ms = FLEXBUS_DMA_TIMEOUT_MS;
	u32 ctrl;

	rockchip_flexbus_writel(fspi->fb, FLEXBUS_CSN_CFG, 0x10001);

	ctrl = FLEXBUS_TX_THEN_RX | FLEXBUS_SCLK_SHARE | FLEXBUS_TX_USE_RX;
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_COM_CTL, ctrl);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_IMR, FLEXBUS_RX_DONE_ISR);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_ICR, FLEXBUS_RX_DONE_ISR);

	rockchip_flexbus_writel(fspi->fb, FLEXBUS_TX_CMD_LEN, cfg->cmd_cycles);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_TX_CMD0, ((u32 *)fspi->tx_buf)[0]);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_TX_CMD1, ((u32 *)fspi->tx_buf)[1]);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_TX_NUM, cfg->cmd_cycles);

	ctrl = FLEXBUS_RXD_DY | FLEXBUS_AUTOPAD | FLEXBUS_RX_CTL_MSB | fspi->fb->dfs_reg->dfs_4bit;
	if (!trick)
		ctrl |= FLEXBUS_RX_CTL_UNIT_BYTE | FLEXBUS_RX_CTL_FILL_DUMMY;
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_RX_CTL, ctrl);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_RX_NUM, cfg->buf_cycles);

	init_completion(&fspi->cp);

	rockchip_flexbus_writel(fspi->fb, FLEXBUS_DMA_DST_ADDR0, cfg->buf_addr >> 2);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_ENR, FLEXBUS_TX_ENR | FLEXBUS_RX_ENR);

	if (!wait_for_completion_timeout(&fspi->cp, msecs_to_jiffies(timeout_ms))) {
		dev_err(fspi->dev, "DMA wait for tr transfer finish timeout\n");
		ret = -ETIMEDOUT;
	}

	rockchip_flexbus_writel(fspi->fb, FLEXBUS_ENR, FLEXBUS_TX_DIS | FLEXBUS_RX_DIS);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_ICR, FLEXBUS_RX_DONE_ISR);
	rockchip_flexbus_writel(fspi->fb, FLEXBUS_IMR, 0);

	rockchip_flexbus_writel(fspi->fb, FLEXBUS_CSN_CFG, 0x10000);

	return ret;
}

static int rk_flexbus_fspi_exec_mem(struct rk_flexbus_fspi *fspi, const struct spi_mem_op *op)
{
	struct rk_flexbus_fspi_xfer cfg = { 0 };
	u32 cmd_cycles, data_cycles;
	int ret;
	bool format_trick;

	/* format cmd_addr_dummy */
	switch (op->addr.nbytes) {
	case 0:
		fspi->tx_buf[3] = op->cmd.opcode;
		fspi->tx_buf[2] = 0xFF;
		cmd_cycles = 8 + op->dummy.nbytes * 8;
		break;
	case 3:
		fspi->tx_buf[3] = op->cmd.opcode;
		fspi->tx_buf[2] = op->addr.val >> 16 & 0xFF;
		fspi->tx_buf[1] = op->addr.val >> 8 & 0xFF;
		fspi->tx_buf[0] = op->addr.val >> 0 & 0xFF;
		fspi->tx_buf[7] = 0xFF;
		fspi->tx_buf[6] = 0xFF;
		cmd_cycles = 32 + op->dummy.nbytes * 8;
		break;
	case 4:
		fspi->tx_buf[3] = op->cmd.opcode;
		fspi->tx_buf[2] = op->addr.val >> 24 & 0xFF;
		fspi->tx_buf[1] = op->addr.val >> 16 & 0xFF;
		fspi->tx_buf[0] = op->addr.val >> 8 & 0xFF;
		if (op->dummy.nbytes)
			fspi->tx_buf[7] = op->addr.val >> 0 & 0xFF;
		else
			fspi->tx_buf[4] = op->addr.val >> 0 & 0xFF;
		fspi->tx_buf[6] = 0xFF;
		cmd_cycles = 40 + op->dummy.nbytes * 8;
		break;
	default:
		dev_err(fspi->fb->dev, "op->addr.nbytes %d not support!\n", op->addr.nbytes);
		return -EINVAL;
	}
	data_cycles = op->data.nbytes * 8 / op->data.buswidth;

	/*
	 * format data:
	 *   V9:
	 *     rx protocol 111 send_then_recv_114(trick!), then re-format data 4-to-1(trick!)
	 *     rx protocol 114 send_then_recv_114, then re-order data 4-to-4(trick!)
	 *     tx protocol 111 re-format data 1-to-4, then send(114 trick!)
	 *   New version:
	 *     rx protocol 111 send_then_recv_114(trick!), then re-format data 4-to-1(trick!)
	 *     rx protocol 114 send_then_recv_114
	 *     tx protocol 111 send(111)
	 */
	if ((fspi->version == FLEXBUS_REVISION_V9) ||
	    (op->data.dir == SPI_MEM_DATA_IN && op->data.buswidth == 1))
		format_trick = true;
	else
		format_trick = false;

	if (op->data.dir == SPI_MEM_DATA_OUT) {
		if (format_trick) {
			flexbus_fspi_data_format(fspi, op->data.buf.out, 1, fspi->temp_buf, 4,
						 data_cycles, fspi->switch_buf);
			dma_sync_single_for_device(fspi->fb->dev, fspi->dma_temp_buf,
						   op->data.nbytes * 4, DMA_TO_DEVICE);
		} else {
			memcpy(fspi->temp_buf, op->data.buf.out, op->data.nbytes);
			dma_sync_single_for_device(fspi->fb->dev, fspi->dma_temp_buf,
						   op->data.nbytes, DMA_TO_DEVICE);
		}
	}

	cfg.cmd_cycles = cmd_cycles;
	cfg.cmd_buf = fspi->tx_buf;
	cfg.buf_cycles = data_cycles;
	cfg.buf_addr = fspi->dma_temp_buf;

	if (op->data.dir == SPI_MEM_DATA_IN)
		ret = rk_flexbus_fspi_send_then_recv_114(fspi, &cfg, format_trick);
	else
		ret = rk_flexbus_fspi_send(fspi, &cfg);
	if (ret) {
		dev_err(fspi->fb->dev, "cmd=%x data=%d\n", op->cmd.opcode, op->data.nbytes);
		print_hex_dump(KERN_WARNING, "regs:", DUMP_PREFIX_OFFSET, 4, 4, fspi->fb->base, 0x200, 0);
		return ret;
	}

	if (op->data.dir == SPI_MEM_DATA_IN) {
		if (op->data.buswidth == 4) {
			dma_sync_single_for_cpu(fspi->fb->dev, fspi->dma_temp_buf,
						op->data.nbytes, DMA_FROM_DEVICE);
			if (format_trick)
				flexbus_data_format_order(fspi, fspi->temp_buf,
							  op->data.buf.in, op->data.nbytes);
			else
				memcpy(op->data.buf.in, fspi->temp_buf, op->data.nbytes);
		} else {
			dma_sync_single_for_cpu(fspi->fb->dev, fspi->dma_temp_buf,
						op->data.nbytes * 4, DMA_FROM_DEVICE);
			flexbus_fspi_data_format(fspi, fspi->temp_buf, 4, op->data.buf.in, 1,
						 data_cycles, fspi->switch_buf);
		}
	}
	dev_dbg(fspi->fb->dev, "cmd=%x addr=%llx nbytes=%x\n", op->cmd.opcode, op->addr.val, op->data.nbytes);

	return ret;
}

static void rk_flexbus_fspi_set_cs_gpio(struct rk_flexbus_fspi *fspi, u8 cs, bool enable)
{
	if (fspi->cs_gpiods) {
		if (has_acpi_companion(fspi->dev))
			gpiod_set_value_cansleep(fspi->cs_gpiods[cs], !enable);
		else
			/* Polarity handled by GPIO library */
			gpiod_set_value_cansleep(fspi->cs_gpiods[cs], enable);
	}
}

static int rk_flexbus_fspi_exec_op_bypass(struct rk_flexbus_fspi *fspi,
					  struct spi_mem *mem,
					  const struct spi_mem_op *op)
{
	u8 cs = mem->spi->chip_select;
	u32 ret;

	rk_flexbus_fspi_set_cs_gpio(fspi, cs, true);

	ret = rk_flexbus_fspi_exec_mem(fspi, op);
	if (ret) {
		dev_err(fspi->dev, "Failed to exec mem, ret=%d\n", ret);
		return ret;
	}
	rk_flexbus_fspi_set_cs_gpio(fspi, cs, false);

	return ret;
}

static void rk_flexbus_fspi_delay_lines_tuning(struct rk_flexbus_fspi *fspi, struct spi_mem *mem)
{
	struct spi_mem_op op = SPI_MEM_OP(SPI_MEM_OP_CMD(0x9F, 1),
						SPI_MEM_OP_NO_ADDR,
						SPI_MEM_OP_NO_DUMMY,
						SPI_MEM_OP_DATA_IN(4, NULL, 1));
	u8 id[4] = { 0 }, id_temp[4] = { 0 };
	u16 cell_max = (u16)rk_flexbus_fspi_get_max_dll_cells(fspi);
	u16 right, left = 0;
	u16 step = FLEXBUS_DLL_TRANING_STEP;
	bool dll_valid = false;
	u8 cs = mem->spi->chip_select;

	rk_flexbus_fspi_clk_set_rate(fspi, FLEXBUS_DLL_THRESHOLD_RATE);
	op.data.buf.in = &id;
	rk_flexbus_fspi_exec_op_bypass(fspi, mem, &op);
	if ((0xFF == id[0] && 0xFF == id[1]) ||
	    (0x00 == id[0] && 0x00 == id[1])) {
		dev_dbg(fspi->fb->dev, "no dev, dll by pass\n");
		rk_flexbus_fspi_clk_set_rate(fspi, fspi->speed[cs]);
		fspi->speed[cs] = FLEXBUS_DLL_THRESHOLD_RATE;

		return;
	}

	rk_flexbus_fspi_clk_set_rate(fspi, fspi->speed[cs]);
	op.data.buf.in = &id_temp;
	for (right = 0; right <= cell_max; right += step) {
		int ret;

		rk_flexbus_fspi_set_delay_lines(fspi, right, cs);
		rk_flexbus_fspi_exec_op_bypass(fspi, mem, &op);
		dev_dbg(fspi->fb->dev, "dll read flash id:%x %x %x\n",
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

	if (dll_valid && (right - left) >= FLEXBUS_DLL_TRANING_VALID_WINDOW) {
		if (left == 0 && right < cell_max)
			fspi->dll_cells[cs] = left + (right - left) * 2 / 5;
		else
			fspi->dll_cells[cs] = left + (right - left) / 2;
	} else {
		fspi->dll_cells[cs] = 0;
	}

	if (fspi->dll_cells[cs]) {
		dev_dbg(fspi->fb->dev, "%d %d %d dll training pass, %dMHz max_cells=%u ver=%d\n",
			left, right, fspi->dll_cells[cs], fspi->speed[cs],
			rk_flexbus_fspi_get_max_dll_cells(fspi), fspi->version);
		rk_flexbus_fspi_set_delay_lines(fspi, (u16)fspi->dll_cells[cs], cs);
	} else {
		dev_err(fspi->fb->dev, "%d %d dll training failed, %dMHz, reduce the frequency\n",
			left, right, fspi->speed[cs]);
		rk_flexbus_fspi_set_delay_lines(fspi, 0, cs);
		rk_flexbus_fspi_clk_set_rate(fspi, FLEXBUS_DLL_THRESHOLD_RATE);
		mem->spi->max_speed_hz = FLEXBUS_DLL_THRESHOLD_RATE;
		fspi->cur_speed = FLEXBUS_DLL_THRESHOLD_RATE;
		fspi->cur_real_speed = rk_flexbus_fspi_clk_get_rate(fspi);
		fspi->speed[cs] = FLEXBUS_DLL_THRESHOLD_RATE;
	}
}

static int rk_flexbus_fspi_exec_mem_op(struct spi_mem *mem, const struct spi_mem_op *op)
{
	struct rk_flexbus_fspi *fspi = spi_controller_get_devdata(mem->spi->master);
	int ret;
	u8 cs = mem->spi->chip_select;

	if (unlikely(mem->spi->max_speed_hz != fspi->speed[cs]) &&
	    !has_acpi_companion(fspi->dev)) {
		ret = rk_flexbus_fspi_clk_set_rate(fspi, mem->spi->max_speed_hz);
		if (ret)
			goto out;
		fspi->speed[cs] = mem->spi->max_speed_hz;
		fspi->cur_speed = mem->spi->max_speed_hz;
		fspi->cur_real_speed = rk_flexbus_fspi_clk_get_rate(fspi);
		if (fspi->cur_real_speed > FLEXBUS_DLL_THRESHOLD_RATE && 1)
			rk_flexbus_fspi_delay_lines_tuning(fspi, mem);

		dev_dbg(fspi->dev, "set_freq=%dHz real_freq=%ldHz\n",
			fspi->speed[cs], rk_flexbus_fspi_clk_get_rate(fspi));
	}

	rk_flexbus_fspi_set_cs_gpio(fspi, cs, true);

	ret = rk_flexbus_fspi_exec_mem(fspi, op);
	if (ret) {
		dev_err(fspi->dev, "Failed to exec mem, ret=%d\n", ret);
		return ret;
	}
out:
	rk_flexbus_fspi_set_cs_gpio(fspi, cs, false);

	return ret;
}

static int rk_flexbus_fspi_adjust_op_size(struct spi_mem *mem, struct spi_mem_op *op)
{
	struct rk_flexbus_fspi *fspi = spi_controller_get_devdata(mem->spi->master);

	op->data.nbytes = min(op->data.nbytes, fspi->max_iosize);

	return 0;
}

static bool rk_flexbus_fspi_supports_op(struct spi_mem *mem, const struct spi_mem_op *op)
{
	if (op->cmd.buswidth > 1 || op->addr.buswidth > 1 ||
	    op->dummy.buswidth > 1 || op->data.buswidth > 4)
		return false;

	return spi_mem_default_supports_op(mem, op);
}

static const struct spi_controller_mem_ops rk_flexbus_fspi_mem_ops = {
	.exec_op = rk_flexbus_fspi_exec_mem_op,
	.adjust_op_size = rk_flexbus_fspi_adjust_op_size,
	.supports_op = rk_flexbus_fspi_supports_op,
};

static void rk_flexbus_fspi_irq_handler(struct rockchip_flexbus *rkfb, u32 isr)
{
	struct rk_flexbus_fspi *fspi = (struct rk_flexbus_fspi *)rkfb->fb0_data;

	// dev_err(rkfb->dev, "isr=%x\n", isr);
	rockchip_flexbus_writel(rkfb, FLEXBUS_ICR, isr);
	if (isr & FLEXBUS_FSPI_ERR_ISR) {
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
		complete(&fspi->cp);
}

static int rk_flexbus_fspi_get_gpio_descs(struct spi_controller *ctlr, struct rk_flexbus_fspi *fspi)
{
	int nb, i;
	struct gpio_desc **cs;
	struct device *dev = &ctlr->dev;
	unsigned int num_cs_gpios = 0;

	nb = gpiod_count(dev, "fspi-cs");
	ctlr->num_chipselect = max_t(int, nb, ctlr->num_chipselect);

	if (nb == 0 || nb == -ENOENT)
		return 0;
	else if (nb < 0)
		return nb;

	cs = devm_kcalloc(dev, ctlr->num_chipselect, sizeof(*cs),
			  GFP_KERNEL);
	if (!cs)
		return -ENOMEM;
	fspi->cs_gpiods = cs;

	for (i = 0; i < nb; i++) {
		cs[i] = devm_gpiod_get_index_optional(dev, "fspi-cs", i,
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

static int rk_flexbus_fspi_probe(struct platform_device *pdev)
{
	struct rockchip_flexbus *flexbus_dev = dev_get_drvdata(pdev->dev.parent);
	struct device *dev = &pdev->dev;
	struct spi_controller *master;
	struct rk_flexbus_fspi *fspi;
	int ret;
	u32 i, val;

	if (flexbus_dev->opmode0 != ROCKCHIP_FLEXBUS0_OPMODE_SPI ||
	    flexbus_dev->opmode1 != ROCKCHIP_FLEXBUS1_OPMODE_NULL) {
		dev_err(&pdev->dev, "flexbus opmode mismatch!, fb0=%d fb1=%d\n",
			flexbus_dev->opmode0, flexbus_dev->opmode1);
		return -ENODEV;
	}

	master = devm_spi_alloc_master(&pdev->dev, sizeof(*fspi));
	if (!master)
		return -ENOMEM;

	master->flags = SPI_MASTER_HALF_DUPLEX;
	master->mem_ops = &rk_flexbus_fspi_mem_ops;
	master->dev.of_node = pdev->dev.of_node;
	master->max_speed_hz = FLEXBUS_MAX_SPEED;
	master->num_chipselect = FLEXBUS_MAX_CHIPSELECT_NUM;
	master->mode_bits = SPI_RX_QUAD;

	fspi = spi_controller_get_devdata(master);
	fspi->dev = dev;
	fspi->master = master;
	fspi->fb = flexbus_dev;

	rockchip_flexbus_set_fb0(flexbus_dev, fspi, rk_flexbus_fspi_irq_handler);

	if (has_acpi_companion(&pdev->dev)) {
		ret = device_property_read_u32(&pdev->dev, "clock-frequency", &val);
		if (ret) {
			dev_err(&pdev->dev, "Failed to find clock-frequency in ACPI\n");
			return ret;
		}
		for (i = 0; i < FLEXBUS_MAX_CHIPSELECT_NUM; i++)
			fspi->speed[i] = val;
	}

	ret = rk_flexbus_fspi_get_gpio_descs(master, fspi);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get gpio_descs\n");
		return ret;
	}

	platform_set_drvdata(pdev, fspi);

	rk_flexbus_fspi_init(fspi);

	fspi->tx_buf = devm_kmalloc(dev, FLEXBUS_QSPI_CMD_MAX, GFP_KERNEL);
	fspi->switch_buf = devm_kmalloc(dev, fspi->max_iosize * FLEXBUS_TX_WIDTH_4, GFP_KERNEL);
	fspi->temp_buf = (u8 *)devm_get_free_pages(dev, GFP_KERNEL | GFP_DMA32,
						   get_order(fspi->max_iosize *
							     FLEXBUS_TX_WIDTH_4));
	if (!fspi->temp_buf)
		return -ENOMEM;
	fspi->dma_temp_buf = virt_to_phys(fspi->temp_buf);

	ret = devm_spi_register_controller(dev, master);
	if (ret)
		return -ENOMEM;

	return 0;
}

static int __maybe_unused rk_flexbus_fspi_resume(struct device *dev)
{
	struct rk_flexbus_fspi *fspi = dev_get_drvdata(dev);
	int i;

	rk_flexbus_fspi_init(fspi);
	for (i = 0; i < FLEXBUS_MAX_CHIPSELECT_NUM; i++) {
		if (fspi->dll_cells[i])
			rk_flexbus_fspi_set_delay_lines(fspi, (u16)fspi->dll_cells[i], i);
	}

	return 0;
}

static const struct dev_pm_ops rk_flexbus_fspi_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(NULL, rk_flexbus_fspi_resume)
};

static const struct of_device_id rk_flexbus_fspi_dt_ids[] = {
	{ .compatible = "rockchip,flexbus-fspi"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rk_flexbus_fspi_dt_ids);

static struct platform_driver rk_flexbus_fspi_driver = {
	.driver = {
		.name	= "rockchip-flexbus-fspi",
		.of_match_table = rk_flexbus_fspi_dt_ids,
		.pm = &rk_flexbus_fspi_pm_ops,
	},
	.probe	= rk_flexbus_fspi_probe,
};
module_platform_driver(rk_flexbus_fspi_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Rockchip Flexbus Controller Under SPI Transmission Protocol Driver");
MODULE_AUTHOR("Jon Lin <Jon.lin@rock-chips.com>");
