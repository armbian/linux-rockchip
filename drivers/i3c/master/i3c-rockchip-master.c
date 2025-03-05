// SPDX-License-Identifier: (GPL-2.0-only)
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 * Driver for I3C master in Rockchip SoC
 */

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/i3c/master.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/slab.h>

#define I3C_BIT(nr)		(BIT(nr) | BIT(nr + 16))
#define I3C_CLR_BIT(nr)		(BIT(nr + 16))

#define I3C_HIWORD_UPDATE(val, mask, shift) \
	((val) << (shift) | (mask) << ((shift) + 16))

#define MODULE_CTL		0x0000
#define MODULE_CONF		0x0004
#define TOUT_CONF_I3C		0x0008
#define CLKSTALL_CONF_I3C	0x000C
#define SLVSCL_CONF_I2C		0x0010
#define WAIT_THLD_I2C		0x0014
#define CMD			0x0020
#define TXDATA			0x0024
#define RXDATA			0x0028
#define IBIDATA			0x002C
#define FIFO_TRIG_DMA		0x0030
#define TXFIFO_NUM_DMA		0x0034
#define RXFIFO_NUM_DMA		0x0038
#define IBIFIFO_NUM_DMA		0x003C
#define FIFO_TRIG_CPU		0x0040
#define TXFIFO_NUM_CPU		0x0044
#define RXFIFO_NUM_CPU		0x0048
#define IBIFIFO_NUM_CPU		0x004C
#define DMA_CONF		0x0050
#define CLKDIV_OD		0x0054
#define CLKDIV_PP		0x0058
#define CLKDIV_FM		0x005C
#define TIME_CONF_PPOD		0x0060
#define TIME_CONF_FM		0x0064
#define DAA_CONF		0x0068
#define BUS_FREE_CONF		0x006C
#define BUS_FLT_CONF		0x0070
#define REG_READY_CONF		0x0074
#define FLUSH_CTL		0x0078
#define INT_EN			0x0090
#define INT_STATUS		0x0094
#define IBI_CONF		0x009C
#define IBI_MAP1		0x0100
#define IBI_MAP2		0x0104
#define IBI_MAP3		0x0108
#define IBI_MAP4		0x010C
#define IBI_MAP5		0x0110
#define IBI_MAP6		0x0114
#define DEV_ID1_RR0		0x0120
#define DEV_ID1_RR1		0x0124
#define DEV_ID1_RR2		0x0128
#define VERSION			0x0200
#define CMD1_STATUS		0x0230
#define ASYNC_REF_CNT		0x0260
#define FIFO_STATUS		0x0264
#define IBI_STATUS		0x0268
#define DAA_STATUS		0x026C
#define RXDAT_OVER		0x0270
#define IBIRX_OVER		0x0274

#define MODULE_CTL_START_EN_SHIFT	(0)
#define MODULE_CTL_START_EN		(0x1 << MODULE_CTL_START_EN_SHIFT)

#define MODULE_CTL_FM_START_EN_SHIFT	(1)
#define MODULE_CTL_FM_START_EN		(0x1 << MODULE_CTL_FM_START_EN_SHIFT)

#define MODULE_CONF_MODULE_EN_SHIFT	(0)
#define MODULE_CONF_MODULE_EN		I3C_BIT(MODULE_CONF_MODULE_EN_SHIFT)
#define MODULE_CONF_MODULE_DIS		I3C_CLR_BIT(MODULE_CONF_MODULE_EN_SHIFT)

#define MODULE_CONF_BUS_TYPE_SHIFT	(1)
#define MODULE_CONF_PURE_I3C		(I3C_CLR_BIT(1) | I3C_CLR_BIT(2))
#define MODULE_CONF_MIX_FAST_I3C	(I3C_BIT(1) | I3C_CLR_BIT(2))
#define MODULE_CONF_MIX_SLOW_I3C	(I3C_CLR_BIT(1) | I3C_BIT(2))
#define MODULE_CONF_PURE_I2C		(I3C_BIT(1) | I3C_BIT(2))

#define MODULE_CONF_ODDAA_EN_SHIFT	(4)
#define MODULE_CONF_ODDAA_EN		I3C_BIT(MODULE_CONF_ODDAA_EN_SHIFT)

#define MODULE_ADDRACK_PPOD_RST_SHIFT	(6)
#define MODULE_ADDRACK_PPOD_RST		(0x1 << MODULE_ADDRACK_PPOD_RST_SHIFT)

#define CLKSTALL_SHIFT			(0)
#define CLKSTALL_ENABLE		(0x1 << CLKSTALL_SHIFT)
#define CLKSTALL_BEFORE_ACK_SHIFT	(1)
#define CLKSTALL_BEFORE_ACK_ENABLE	(0x1 << CLKSTALL_BEFORE_ACK_SHIFT)
#define CLKSTALL_BEFORE_READ_SHIFT	(3)
#define CLKSTALL_BEFORE_READ_ENABLE	(0x1 << CLKSTALL_BEFORE_READ_SHIFT)
#define CLKSTALL_NUM_SHIFT		(8)

#define CMD_RNW_SHIFT			(0)
#define CMD_RNW				(0x1 << CMD_RNW_SHIFT)

#define CMD_ADDR_SHIFT			(1)
#define CMD_ADDR_MASK			(0x7F << CMD_ADDR_SHIFT)
#define CMD_DEV_ADDR(a)			((a) << CMD_ADDR_SHIFT)

#define CMD_TRAN_MODE_SHIFT		(8)
#define CMD_I3C_SDR_TX_MODE		(0x0 << CMD_TRAN_MODE_SHIFT)
#define CMD_I3C_SDR_RX_MODE		(0x1 << CMD_TRAN_MODE_SHIFT)
#define CMD_I3C_ENTDAA_MODE		(0x2 << CMD_TRAN_MODE_SHIFT)
#define CMD_I3C_ADDR_ONLY_MODE		(0x3 << CMD_TRAN_MODE_SHIFT)
#define CMD_I2C_TX			(0x4 << CMD_TRAN_MODE_SHIFT)
#define CMD_I2C_RX			(0x5 << CMD_TRAN_MODE_SHIFT)

#define CMD_IS_SYNC_DT_TRAN_SHIFT	(11)
#define CMD_IS_SYNC_DT_TRAN		(0x1 << CMD_IS_SYNC_DT_TRAN_SHIFT)

#define CMD_IS_HDREXIT_SHIFT		(12)
#define CMD_HDREXIT_PATTERN		(0x1 << CMD_IS_HDREXIT_SHIFT)

#define CMD_IS_RESET_SHIFT		(13)
#define CMD_TARGET_RESET_PATTERN	(0x1 << CMD_IS_RESET_SHIFT)

#define CMD_I2C_ACK2LAST_SHIFT		(14)
#define CMD_I2C_ACK2LAST		(0x1 << CMD_I2C_ACK2LAST_SHIFT)

#define CMD_I2C_ACK2NAK_SHIFT		(15)
#define CMD_I2C_ACK2NAK			(0x1 << CMD_I2C_ACK2NAK_SHIFT)

#define CMD_I3C_ACK2EARLY_SHIFT		(16)
#define CMD_I3C_ACK2EARLY		(0x1 << CMD_I3C_ACK2EARLY_SHIFT)

#define CMD_I3C_ACK2NAK_SHIFT		(17)
#define CMD_I3C_ACK2NAK_INNORE		(0x1 << CMD_I3C_ACK2NAK_SHIFT)

#define CMD_I3C_END2IBI_SHIFT		(18)
#define CMD_I3C_END2IBI			(0x1 << CMD_I3C_END2IBI_SHIFT)

#define CMD_ACK2NEXT_SHIFT		(19)
#define CMD_ACK2STOP			(0x0 << CMD_ACK2NEXT_SHIFT)
#define CMD_ACK2NEXT			(0x1 << CMD_ACK2NEXT_SHIFT)

#define CMD_BYTE_LEN_SHIFT		(20)
#define CMD_FIFO_PL_LEN(a)		((a) << CMD_BYTE_LEN_SHIFT)

#define REG_READY_CONF_SHIFT		(0)
#define REG_READY_CONF_EN		I3C_BIT(REG_READY_CONF_SHIFT)
#define REG_READY_CONF_DIS		I3C_CLR_BIT(REG_READY_CONF_SHIFT)

#define REG_READY_CONF_IBI_READY_SHIFT	(1)
#define IBI_REG_READY_CONF_EN		I3C_BIT(REG_READY_CONF_IBI_READY_SHIFT)
#define IBI_REG_READY_CONF_DIS		I3C_CLR_BIT(REG_READY_CONF_IBI_READY_SHIFT)

#define FLUSH_CTL_CMDFIFO_FLUSH_SHIFT	(0)
#define FLUSH_CTL_CMDFIFO		(0x1 << FLUSH_CTL_CMDFIFO_FLUSH_SHIFT)

#define FLUSH_CTL_TXFIFO_FLUSH_SHIFT	(1)
#define FLUSH_CTL_TXFIFO		(0x1 << FLUSH_CTL_TXFIFO_FLUSH_SHIFT)

#define FLUSH_CTL_RXFIFO_FLUSH_SHIFT	(2)
#define FLUSH_CTL_RXFIFO		(0x1 << FLUSH_CTL_RXFIFO_FLUSH_SHIFT)

#define FLUSH_CTL_IBIFIFO_FLUSH_SHIFT	(3)
#define FLUSH_CTL_IBIFIFO		(0x1 << FLUSH_CTL_IBIFIFO_FLUSH_SHIFT)

#define FLUSH_CTL_CMDSTATUS_FLUSH_SHIFT	(4)
#define FLUSH_CTL_CMDSTATUS		(0x1 << FLUSH_CTL_CMDSTATUS_FLUSH_SHIFT)

#define FLUSH_CTL_DEVUID_FLUSH_SHIFT	(5)
#define FLUSH_CTL_DEVUID		(0x1 << FLUSH_CTL_DEVUID_FLUSH_SHIFT)

#define FLUSH_CTL_IBIMAP_FLUSH_SHIFT	(6)
#define FLUSH_CTL_IBIMAP		(0x1 << FLUSH_CTL_IBIMAP_FLUSH_SHIFT)

#define FLUSH_CTL_FIFO_NUM_TXRX_SHIFT	(8)
#define FLUSH_CTL_FIFO_NUM_TXRX		(0x1 << FLUSH_CTL_FIFO_NUM_TXRX_SHIFT)

#define FLUSH_CTL_FIFO_NUM_IBI_SHIFT	(9)
#define FLUSH_CTL_FIFO_NUM_IBI		(0x1 << FLUSH_CTL_FIFO_NUM_IBI_SHIFT)

#define FLUSH_CTL_AHB_RTX_STATUS_SHIFT	(10)
#define FLUSH_CTL_AHB_RTX_STATUS	(0x1 << FLUSH_CTL_AHB_RTX_STATUS_SHIFT)

#define FLUSH_CTRL_ALL			(FLUSH_CTL_CMDFIFO | FLUSH_CTL_TXFIFO | \
					FLUSH_CTL_RXFIFO |  FLUSH_CTL_CMDSTATUS | \
					FLUSH_CTL_FIFO_NUM_TXRX | FLUSH_CTL_AHB_RTX_STATUS)

#define FLUSH_IBI_ALL			(FLUSH_CTL_IBIFIFO | FLUSH_CTL_FIFO_NUM_IBI)

#define INT_EN_END_IEN_SHIFT		(0)
#define IRQ_END_IEN			I3C_BIT(INT_EN_END_IEN_SHIFT)
#define IRQ_END_CLR			I3C_CLR_BIT(INT_EN_END_IEN_SHIFT)
#define IRQ_END_STS			(0x1 << INT_EN_END_IEN_SHIFT)

#define INT_EN_SLVREQ_START_IEN_SHIFT	(1)
#define IRQ_SLVREQ_START_IEN		I3C_BIT(INT_EN_SLVREQ_START_IEN_SHIFT)
#define IRQ_SLVREQ_START_DIS		I3C_CLR_BIT(INT_EN_SLVREQ_START_IEN_SHIFT)
#define SLVREQ_START_STS		(0x1 << INT_EN_SLVREQ_START_IEN_SHIFT)

#define INT_EN_OWNIBI_IEN_SHIFT		(2)
#define OWNIBI_IRQPD_IEN		I3C_BIT(INT_EN_OWNIBI_IEN_SHIFT)
#define OWNIBI_IRQPD_DIS		I3C_CLR_BIT(INT_EN_OWNIBI_IEN_SHIFT)
#define OWNIBI_IRQPD_STS		(0x1 << INT_EN_OWNIBI_IEN_SHIFT)

#define INT_EN_DATAERR_IRQPD_SHIFT	(4)
#define INT_DATAERR_IRQPD_STS		(0x1 << INT_EN_DATAERR_IRQPD_SHIFT)

#define INT_EN_I2CIBI_IRQPD_SHIFT	(9)
#define I2CIBI_IRQPD_STS		(0x1 << INT_EN_I2CIBI_IRQPD_SHIFT)

#define INT_EN_CPU_FIFOTRIG_IEN_SHIFT	(15)
#define IRQ_CPU_FIFOTRIG_IEN		I3C_BIT(INT_EN_CPU_FIFOTRIG_IEN_SHIFT)
#define IRQ_CPU_FIFOTRIG_CLR		I3C_CLR_BIT(INT_EN_CPU_FIFOTRIG_IEN_SHIFT)
#define CPU_FIFOTRIG_IRQ_STS		(0x1 << INT_EN_CPU_FIFOTRIG_IEN_SHIFT)

#define INT_STATUS_LASTCMD_ID_SHIFT	(16)
#define FINISHED_CMD_ID_MASK		(0xF << INT_STATUS_LASTCMD_ID_SHIFT)
#define FINISHED_CMD_ID(isr)		(((isr) & FINISHED_CMD_ID_MASK) >> INT_STATUS_LASTCMD_ID_SHIFT)

#define INT_STATUS_ERRCMD_ID_SHIFT	(24)
#define ERR_CMD_ID_MASK			(0xF << INT_STATUS_ERRCMD_ID_SHIFT)
#define ERR_CMD_ID(isr)			(((isr) & ERR_CMD_ID_MASK) >> INT_STATUS_ERRCMD_ID_SHIFT)

#define IBI_CONF_AUTOIBI_EN_SHIFT	(0)
#define IBI_CONF_AUTOIBI_EN		I3C_BIT(IBI_CONF_AUTOIBI_EN_SHIFT)
#define IBI_CONF_AUTOIBI_DIS		I3C_CLR_BIT(IBI_CONF_AUTOIBI_EN_SHIFT)

#define DMA_CONF_NORDMA_RX_EN_SHIFT	(1)
#define DMA_CONF_NORDMA_RX_EN		(0x1 << DMA_CONF_NORDMA_RX_EN_SHIFT)

#define DMA_CONF_NORDMA_IBI_EN_SHIFT	(2)
#define DMA_CONF_NORDMA_IBI_EN		(0x1 << DMA_CONF_NORDMA_IBI_EN_SHIFT)

#define DMA_CONF_NORDMA_TX_EN_SHIFT	(3)
#define DMA_CONF_NORDMA_TX_EN		(0x1 << DMA_CONF_NORDMA_TX_EN_SHIFT)

#define DMA_CONF_RX_RESP_ERR_EN_SHIFT	(4)
#define DMA_CONF_RX_RESP_ERROR_EN	(0x1 << DMA_CONF_RX_RESP_ERR_EN_SHIFT)

#define DMA_CONF_IBI_RESP_ERR_EN_SHIFT	(5)
#define DMA_CONF_IBI_RESP_ERROR_EN	(0x1 << DMA_CONF_IBI_RESP_ERR_EN_SHIFT)

#define DMA_CONF_TX_RESP_ERR_EN_SHIFT	(6)
#define DMA_CONF_TX_RESP_ERROR_EN	(0x1 << DMA_CONF_TX_RESP_ERR_EN_SHIFT)

#define DMA_CONF_RX_HSIZE_ERR_EN_SHIFT	(8)
#define DMA_CONF_RX_HSIZE_ERR_IRQEN	(0x1 << DMA_CONF_RX_HSIZE_ERR_EN_SHIFT)

#define DMA_CONF_IBI_HSIZE_ERR_EN_SHIFT	(9)
#define DMA_CONF_IBI_HSIZE_ERR_IRQEN	(0x1 << DMA_CONF_IBI_HSIZE_ERR_EN_SHIFT)

#define DMA_CONF_TX_HSIZE_ERR_EN_SHIFT	(10)
#define DMA_CONF_TX_HSIZE_ERR_IRQEN	(0x1 << DMA_CONF_TX_HSIZE_ERR_EN_SHIFT)

#define DMA_CONF_RX_PAD_EN_SHIFT	(12)
#define DMA_CONF_RX_PAD_EN		(0x1 << DMA_CONF_RX_PAD_EN_SHIFT)

#define DMA_CONF_ENABLED		(DMA_CONF_NORDMA_RX_EN | DMA_CONF_NORDMA_TX_EN | \
					DMA_CONF_TX_HSIZE_ERR_IRQEN | DMA_CONF_RX_HSIZE_ERR_IRQEN | \
					DMA_CONF_RX_PAD_EN)

#define DMA_TXFIFO_TRIG_SHIFT		(0)
#define DMA_TXFIFO_TRIG_MASK		(0xF << DMA_TXFIFO_TRIG_SHIFT)
#define DMA_RXFIFO_TRIG_SHIFT		(4)
#define DMA_RXFIFO_TRIG_MASK		(0xF << DMA_RXFIFO_TRIG_SHIFT)
#define DMA_TXFIFO_TRIG_MASK_SHIFT	(16)
#define DMA_TXFIFO_TRIG_MASK_MASK	(0xF << DMA_TXFIFO_TRIG_MASK_SHIFT)
#define DMA_RXFIFO_TRIG_MASK_SHIFT	(20)
#define DMA_RXFIFO_TRIG_MASK_MASK	(0xF << DMA_RXFIFO_TRIG_MASK_SHIFT)

#define DMA_TXFIFO_TRIG(trig)		((((trig) - 1) << DMA_TXFIFO_TRIG_SHIFT) | DMA_TXFIFO_TRIG_MASK_MASK)
#define DMA_RXFIFO_TRIG(trig)		((((trig) - 1) << DMA_RXFIFO_TRIG_SHIFT) | DMA_RXFIFO_TRIG_MASK_MASK)

#define CPU_TXFIFO_TRIG_CPU_SHIFT	(0)
#define CPU_TXFIFO_TRIG_CPU_MASK	(0xF << CPU_TXFIFO_TRIG_CPU_SHIFT)
#define CPU_RXFIFO_TRIG_CPU_SHIFT	(4)
#define CPU_RXFIFO_TRIG_CPU_MASK	(0xF << CPU_RXFIFO_TRIG_CPU_SHIFT)
#define CPU_TXFIFO_TRIG_MASK_SHIFT	(16)
#define CPU_TXFIFO_TRIG_MASK_MASK	(0xF << CPU_TXFIFO_TRIG_MASK_SHIFT)
#define CPU_RXFIFO_TRIG_MASK_SHIFT	(20)
#define CPU_RXFIFO_TRIG_MASK_MASK	(0xF << CPU_RXFIFO_TRIG_MASK_SHIFT)

#define IRQ_TXFIFO_TRIG(x)		((((x) - 1) << CPU_TXFIFO_TRIG_CPU_SHIFT) | CPU_TXFIFO_TRIG_MASK_MASK)
#define IRQ_RXFIFO_TRIG(x)		((((x) - 1) << CPU_RXFIFO_TRIG_CPU_SHIFT) | CPU_RXFIFO_TRIG_MASK_MASK)

#define TXFIFO_SPACE2FULL_SHIFT		(8)
#define TXFIFO_SPACE2FULL_MASK		(0x1F << TXFIFO_SPACE2FULL_SHIFT)
#define TXFIFO_SPACE2FULL_NUMWORD(s)	(((s) & TXFIFO_SPACE2FULL_MASK) >> TXFIFO_SPACE2FULL_SHIFT)

#define RXFIFO_SPACE2EMPTY_SHIFT	(16)
#define RXFIFO_SPACE2EMPTY_MASK		(0x1F << RXFIFO_SPACE2EMPTY_SHIFT)
#define RXFIFO_SPACE2EMPTY_NUMWORD(s)	(((s) & RXFIFO_SPACE2EMPTY_MASK) >> RXFIFO_SPACE2EMPTY_SHIFT)

#define DAA_STATUS_ASSIGNED_CNT_SHIFT	(0)
#define DAA_ASSIGNED_CNT_MASK		(0xF << DAA_STATUS_ASSIGNED_CNT_SHIFT)
#define DAANAK_CNT_SHIFT		(4)
#define DAANAK_CNT_MASK			(0x3 << DAA_STATUS_DAANAK_CNT_SHIFT)

#define STATUS_CMD1_ADDRNAK_SHIFT	(0)
#define STATUS_ADDRNAK			(0x1 << STATUS_CMD1_ADDRNAK_SHIFT)

#define STATUS_CMD1_RXEARLY_I3C_SHIFT	(1)
#define STATUS_RXEARLY_I3C		(0x1 << STATUS_CMD1_RXEARLY_I3C_SHIFT)

#define STATUS_CMD1_TXEARLY_I2C_SHIFT	(2)
#define STATUS_TXEARLY_I2C		(0x1 << STATUS_CMD1_TXEARLY_I2C_SHIFT)

#define STATUS_CMD1_FORCE_STOP_SHIFT	(3)
#define STATUS_FORCE_STOP		(0x1 << STATUS_CMD1_FORCE_STOP_SHIFT)

#define STATUS_CMD1_DAANAK_SHIFT	(4)
#define STATUS_DAANAK			(0x1 << STATUS_CMD1_DAANAK_SHIFT)

#define STATUS_CMD1_FINISH_SHIFT	(6)
#define STATUS_FINISH			(0x1 << STATUS_CMD1_FINISH_SHIFT)

#define STATUS_CMD1_RNW_SHIFT		(7)
#define STATUS_RNW			(0x1 << STATUS_CMD1_RNW_SHIFT)

#define STATUS_ERR			GENMASK(STATUS_CMD1_DAANAK_SHIFT + 1, 0)

#define I3C_BUS_THIGH_MAX_NS		41
#define SCL_I3C_TIMING_CNT_MIN		12
#define I3C_BUS_I2C_FM_TLOW_MIN_NS	1300
#define I3C_BUS_I2C_FM_SCL_RATE		400000
#define I3C_BUS_I2C_FMP_TLOW_MIN_NS	500
#define I3C_BUS_I2C_FMP_SCL_RAISE_NS	120

#define SCL_I3C_CLKDIV_HCNT(x)		((x) & GENMASK(15, 0))
#define SCL_I3C_CLKDIV_LCNT(x)		(((x) << 16) & GENMASK(31, 16))
#define SCL_I2C_CLKDIV_HCNT(x)		((x) & GENMASK(15, 0))
#define SCL_I2C_CLKDIV_LCNT(x)		(((x) << 16) & GENMASK(31, 16))

#define I2C_SLV_HOLD_SCL_CONF		0xfffffff0

#define IBI_TYPE_IBI			0
#define IBI_TYPE_HJ			1
#define IBI_TYPE_MR			2
#define IBI_TYPE(x)			((x) & GENMASK(1, 0))
#define IBI_NACK			BIT(2)
#define IBI_RXEARLY			BIT(3)
#define IBI_XFER_BYTES(x)		(((x) & GENMASK(15, 8)) >> 8)
#define IBI_SLVID(x)			(((x) & GENMASK(20, 16)) >> 16)
#define I2C_IBIADDR(x)			(((x) & GENMASK(32, 24)) >> 24)

#define IBI_MAP(d)			(IBI_MAP1 + (((d) - 1) / 2) * 0x4)
#define IBI_MAP_DEV_ADDR(d, addr)	(((addr) & GENMASK(6, 0)) << ((((d) - 1) % 2) * 0x10))
#define IBI_MAP_DEV_PLLEN(d, len)	(((len) & GENMASK(7, 0)) << ((((d) - 1) % 2) * 0x10 + 0x8))

#define INTR_ALL			0xffffffff
#define STATUS_LAST_CMDID		(((x) & GENMASK(19, 16)) >> 16)

#define ROCKCHIP_I3C_MAX_DEVS		12
#define DEVS_CTRL_DEVS_ACTIVE_MASK	GENMASK(11, 0)
#define DEV_ID_RR0(d)			(DEV_ID1_RR0 + (d) * 0x10)
#define DEV_ID_RR0_GET_DEV_ADDR(x)	(((x) >> 1) & GENMASK(6, 0))

#define CMD_FIFO_CCC(x)			((x) & GENMASK(6, 0))
#define CMDR_XFER_BYTES(x)		(((x) & GENMASK(19, 8)) >> 8)

#define DATAFIFO_DEPTH			16
#define CMDFIFO_DEPTH			11
#define IBIFIFO_DEPTH			16
#define CMD_FIFO_PL_LEN_MAX		4096

#define CMDR_NO_ERROR			0
#define CMDR_DDR_PREAMBLE_ERROR		1
#define CMDR_DDR_PARITY_ERROR		2
#define CMDR_DDR_RX_FIFO_OVF		3
#define CMDR_DDR_TX_FIFO_UNF		4
#define CMDR_M0_ERROR			5
#define CMDR_M1_ERROR			6
#define CMDR_M2_ERROR			7
#define CMDR_MST_ABORT			8
#define CMDR_NACK_RESP			9

#define DMA_TRIG_LEVEL			0x8
#define DMA_RX_MAX_BURST_SIZE		0x4
#define DMA_RX_MAX_BURST_LEN		0x8

#define CLKSTALL_NUM			(0x4 << CLKSTALL_NUM_SHIFT)

#define WAIT_TIMEOUT			1000 /* ms */
#define TX_IDLE_WAIT_TIMEOUT		10 /* ms */

enum rockchip_i3c_xfer_mode {
	ROCKCHIP_I3C_POLL,
	ROCKCHIP_I3C_DMA,
	ROCKCHIP_I2C_IRQ
};

/*
 * If it was direct ccc, daa, or with7e xfer, there are two fifo
 * cmd at least, first fifo cmd's mode is brocast 7E; the next fifo
 * cmd is xfer's cmd.
 */
enum rockchip_i3c_cmd_mode {
	PRIVATE_TRANSFER_WITHOUT7E,
	PRIVATE_TRANSFER_WITH7E,
	BROCAST_CCC,
	DIRECT_CCC,
	DO_DAA
};

struct rockchip_i3c_cmd {
	u32 fifo_cmd;
	u8 target_addr;
	u16 tx_len;
	u8 *tx_buf;
	u16 rx_len;
	u8 *rx_buf;
	u8 error;
};

struct rockchip_i3c_xfer {
	struct list_head node;
	struct completion comp;
	int ret;
	u8 mode;
	u8 ccc_cmd;
	unsigned int ncmds;

	/* total word */
	unsigned int tx_num_word;
	unsigned int rx_num_word;

	/* dma */
	int tx_sg_len;
	int rx_sg_len;
	u8 rx_trig;
	u8 tx_trig;
	u8 rx_burst_size;
	u8 rx_burst_len;
	bool xferred;

	/* irq */
	unsigned int cur_tx_cmd;
	unsigned int cur_tx_len;

	unsigned int cur_rx_cmd;
	unsigned int cur_rx_len;

	enum rockchip_i3c_xfer_mode xfer_mode;
	struct rockchip_i3c_cmd cmds[];
};

struct rockchip_i3c_master_caps {
	u8 cmdfifodepth;
	u8 datafifodepth;
	u8 ibififodepth;
};

struct rockchip_i3c_dat_entry {
	u8 addr;
	struct i3c_dev_desc *ibi_dev;
};

struct rockchip_i3c_i2c_dev_data {
	u16 id;
	s16 ibi;
	struct i3c_generic_ibi_pool *ibi_pool;
};

struct rockchip_i3c_master {
	struct i3c_master_controller base;
	struct device *dev;

	void __iomem *regs;
	struct clk *clk;
	struct clk *hclk;
	struct reset_control *reset;
	struct reset_control *reset_ahb;
	int irq;

	dma_addr_t dma_addr_rx;
	dma_addr_t dma_addr_tx;
	struct dma_chan *dma_rx;
	struct dma_chan *dma_tx;
	struct scatterlist tx_sg[CMDFIFO_DEPTH];
	struct scatterlist rx_sg[CMDFIFO_DEPTH];

	struct {
		struct list_head list;
		struct rockchip_i3c_xfer *cur;
		spinlock_t lock;
	} xferqueue;

	struct {
		unsigned int num_slots;
		struct i3c_dev_desc **slots;
		spinlock_t lock;
	} ibi;
	struct work_struct hj_work;
	struct rockchip_i3c_master_caps caps;

	u32 free_pos;
	bool daa_done;
	unsigned int maxdevs;
	struct rockchip_i3c_dat_entry devs[ROCKCHIP_I3C_MAX_DEVS];

	struct kmem_cache *aligned_cache;
	bool suspended;
};

static void rockchip_i3c_master_start_xfer_locked(struct rockchip_i3c_master *master);
static void rockchip_i3c_master_end_xfer_locked(struct rockchip_i3c_master *master,
						u32 isr);

static inline struct rockchip_i3c_master *
to_rockchip_i3c_master(struct i3c_master_controller *master)
{
	return container_of(master, struct rockchip_i3c_master, base);
}

static unsigned int
rockchip_i3c_master_wr_to_tx_fifo(struct rockchip_i3c_master *master,
				  const u8 *bytes, int nbytes)
{
	writesl(master->regs + TXDATA, bytes, nbytes / 4);
	if (nbytes & 3) {
		u32 tmp = 0;

		memcpy(&tmp, bytes + (nbytes & ~3), nbytes & 3);
		writesl(master->regs + TXDATA, &tmp, 1);
	}

	return DIV_ROUND_UP(nbytes, 4);
}

static void
rockchip_i3c_master_wr_ccc_to_tx_fifo(struct rockchip_i3c_master *master,
				      const u8 *bytes, int nbytes, u8 cmd)
{
	u32 val = 0;
	int i, n;

	val = cmd;
	if (nbytes <= 3) {
		for (i = 0; i < nbytes; i++)
			val |= bytes[i] << (8 * (i + 1));
		writel_relaxed(val, master->regs + TXDATA);
	} else {
		n = nbytes - 3;
		writesl(master->regs + TXDATA, bytes + 3, n / 4);
		if ((nbytes - 3) & 3) {
			u32 tmp = 0;

			memcpy(&tmp, bytes + (n & ~3), n & 3);
			writesl(master->regs + TXDATA, &tmp, 1);
		}
	}
}

static unsigned int
rockchip_i3c_master_rd_from_rx_fifo(struct rockchip_i3c_master *master,
				    u8 *bytes, int nbytes)
{
	readsl(master->regs + RXDATA, bytes, nbytes / 4);
	if (nbytes & 3) {
		u32 tmp;

		readsl(master->regs + RXDATA, &tmp, 1);
		memcpy(bytes + (nbytes & ~3), &tmp, nbytes & 3);
	}

	return DIV_ROUND_UP(nbytes, 4);
}

static bool rockchip_i3c_master_supports_ccc_cmd(struct i3c_master_controller *m,
						 const struct i3c_ccc_cmd *cmd)
{
	if (cmd->ndests > 1)
		return false;

	switch (cmd->id) {
	case I3C_CCC_ENEC(true):
	case I3C_CCC_ENEC(false):
	case I3C_CCC_DISEC(true):
	case I3C_CCC_DISEC(false):
	case I3C_CCC_ENTAS(0, true):
	case I3C_CCC_ENTAS(0, false):
	case I3C_CCC_RSTDAA(true):
	case I3C_CCC_RSTDAA(false):
	case I3C_CCC_ENTDAA:
	case I3C_CCC_SETMWL(true):
	case I3C_CCC_SETMWL(false):
	case I3C_CCC_SETMRL(true):
	case I3C_CCC_SETMRL(false):
	case I3C_CCC_ENTHDR(0):
	case I3C_CCC_SETDASA:
	case I3C_CCC_SETNEWDA:
	case I3C_CCC_GETMWL:
	case I3C_CCC_GETMRL:
	case I3C_CCC_GETPID:
	case I3C_CCC_GETBCR:
	case I3C_CCC_GETDCR:
	case I3C_CCC_GETSTATUS:
	case I3C_CCC_GETMXDS:
	case I3C_CCC_GETHDRCAP:
		return true;
	default:
		return false;
	}
}

static void rockchip_i3c_master_disable(struct rockchip_i3c_master *master)
{
	writel_relaxed(MODULE_CONF_MODULE_DIS, master->regs + MODULE_CONF);
}

static void rockchip_i3c_master_enable(struct rockchip_i3c_master *master)
{
	writel_relaxed(MODULE_CONF_MODULE_EN | MODULE_CONF_ODDAA_EN |
		       MODULE_ADDRACK_PPOD_RST,
		       master->regs + MODULE_CONF);
}

static void rockchip_i3c_master_reset_controller(struct rockchip_i3c_master *i3c)
{
	if (!IS_ERR_OR_NULL(i3c->reset)) {
		reset_control_assert(i3c->reset);
		udelay(10);
		reset_control_deassert(i3c->reset);
	}

	if (!IS_ERR_OR_NULL(i3c->reset_ahb)) {
		reset_control_assert(i3c->reset_ahb);
		udelay(10);
		reset_control_deassert(i3c->reset_ahb);
	}
}

static struct rockchip_i3c_xfer *
rockchip_i3c_master_alloc_xfer(struct rockchip_i3c_master *master, unsigned int ncmds)
{
	struct rockchip_i3c_xfer *xfer;

	xfer = kzalloc(struct_size(xfer, cmds, ncmds), GFP_KERNEL);
	if (!xfer)
		return NULL;

	INIT_LIST_HEAD(&xfer->node);
	xfer->ncmds = ncmds;
	xfer->ret = -ETIMEDOUT;

	return xfer;
}

static void rockchip_i3c_master_free_xfer(struct rockchip_i3c_xfer *xfer)
{
	kfree(xfer);
}

static void rockchip_i3c_master_queue_xfer(struct rockchip_i3c_master *master,
					   struct rockchip_i3c_xfer *xfer)
{
	unsigned long flags;

	init_completion(&xfer->comp);
	spin_lock_irqsave(&master->xferqueue.lock, flags);
	if (master->xferqueue.cur) {
		list_add_tail(&xfer->node, &master->xferqueue.list);
	} else {
		master->xferqueue.cur = xfer;
		rockchip_i3c_master_start_xfer_locked(master);
	}
	spin_unlock_irqrestore(&master->xferqueue.lock, flags);
}

static void rockchip_i3c_master_unqueue_xfer(struct rockchip_i3c_master *master,
					     struct rockchip_i3c_xfer *xfer)
{
	unsigned long flags;

	spin_lock_irqsave(&master->xferqueue.lock, flags);
	if (master->xferqueue.cur == xfer) {
		writel_relaxed(REG_READY_CONF_DIS, master->regs + REG_READY_CONF);
		master->xferqueue.cur = NULL;
		writel_relaxed(IRQ_END_CLR, master->regs + INT_EN);
	} else {
		list_del_init(&xfer->node);
	}
	spin_unlock_irqrestore(&master->xferqueue.lock, flags);
}

static void rockchip_i3c_master_put_dma_safe_buf(struct rockchip_i3c_master *master,
						 u8 *buf, u8 *target, unsigned int len,
						 bool xferred, bool rx)
{
	if (!buf || buf == target)
		return;

	if (xferred && rx)
		memcpy(target, buf, len);

	kmem_cache_free(master->aligned_cache, buf);
}

static u8 *rockchip_i3c_master_get_dma_safe_buf(struct rockchip_i3c_master *master,
						u8 *buf, unsigned int len, bool rx)
{
	u8 *mem;

	if (len == 0)
		return NULL;

	mem = kmem_cache_zalloc(master->aligned_cache, GFP_KERNEL);
	if (!mem)
		return NULL;

	if (!rx)
		memcpy(mem, buf, len);

	return mem;
}

static void rockchip_i3c_maste_cleanup_dma(struct rockchip_i3c_master *master)
{
	struct rockchip_i3c_xfer *xfer = master->xferqueue.cur;
	int i;

	if (xfer->rx_sg_len)
		dmaengine_terminate_all(master->dma_rx);

	if (xfer->tx_sg_len)
		dmaengine_terminate_all(master->dma_tx);

	for (i = 0; i < xfer->rx_sg_len; i++)
		dma_unmap_single(master->dma_rx->device->dev,
				 sg_dma_address(&master->rx_sg[i]),
				 sg_dma_len(&master->rx_sg[i]), DMA_FROM_DEVICE);

	for (i = 0; i < xfer->tx_sg_len; i++)
		dma_unmap_single(master->dma_tx->device->dev,
				 sg_dma_address(&master->tx_sg[i]),
				 sg_dma_len(&master->tx_sg[i]), DMA_TO_DEVICE);

	/* DMA conf */
	writel_relaxed(0x0, master->regs + DMA_CONF);
}

static inline u32
rockchip_i3c_master_wait_for_tx_idle(struct rockchip_i3c_master *master)
{
	u32 status;

	if (!readl_relaxed_poll_timeout(master->regs + INT_STATUS, status,
					status & IRQ_END_STS, 100,
					TX_IDLE_WAIT_TIMEOUT * 1000))
		return status;

	dev_warn(master->dev, "i3c controller is in busy state!\n");
	return status;
}

static void rockchip_i3c_master_dma_irq_callback(void *data)
{
	struct rockchip_i3c_master *master = data;
	struct rockchip_i3c_xfer *xfer;

	rockchip_i3c_maste_cleanup_dma(master);
	xfer = master->xferqueue.cur;
	xfer->xferred = true;
	complete(&xfer->comp);
}

static inline u8 rockchip_i3c_calc_rx_burst(u32 data_len, bool size_calced)
{
	u32 i, level;

	/* If burst size calculated, rx burst size is byte, half_word or word.
	 * tx burst size is fixed word, and for burst len calculated, it is
	 * guaranteed to be no larger than 1/2 RX FIFO size.
	 */
	if (size_calced)
		level = 4;
	else
		level = 8;

	/* burst size: 1, 2, 4, or 8 */
	for (i = 1; i < level; i <<= 1) {
		if (data_len & i)
			break;
	}

	return i;
}

static inline u8 rockchip_i3c_master_calc_trig_level(u32 data_len, bool rx)
{
	/* for rx, if data len cannot divisible by 4, best trig is 1 word,
	 * and ensure (burst_size * burst_len) <= (trig * 4).
	 */
	if ((data_len % 4) && rx)
		return 1;

	if (data_len % 4)
		return data_len / 4 + 1;
	else
		return data_len / 4;
}

static int rockchip_i3c_master_prepate_dma_sg(struct rockchip_i3c_master *master,
					      struct rockchip_i3c_xfer *xfer)
{
	struct dma_async_tx_descriptor *rxdesc = NULL, *txdesc = NULL;
	unsigned int rx_num_word = 0, tx_num_word = 0, i;
	struct rockchip_i3c_cmd *cmd;
	dma_cookie_t cookie;
	dma_addr_t dma_addr;

	/* DMA conf */
	writel_relaxed(DMA_CONF_ENABLED, master->regs + DMA_CONF);

	xfer->tx_sg_len = xfer->rx_sg_len = 0;
	for (i = 0; i < xfer->ncmds; i++) {
		cmd = &xfer->cmds[i];
		if (cmd->fifo_cmd & CMD_RNW) {
			u8 rx_burst = xfer->rx_burst_size * xfer->rx_burst_len;

			rx_num_word += DIV_ROUND_UP(cmd->rx_len, 4);
			dma_addr = dma_map_single(master->dma_rx->device->dev,
						  cmd->rx_buf, cmd->rx_len,
						  DMA_FROM_DEVICE);
			if (dma_mapping_error(master->dma_rx->device->dev, dma_addr)) {
				dev_err(master->dev, "dma rx map failed\n");
				return -EINVAL;
			}

			sg_dma_len(&master->rx_sg[xfer->rx_sg_len]) =
				DIV_ROUND_UP(cmd->rx_len, rx_burst) * rx_burst;
			sg_dma_address(&master->rx_sg[xfer->rx_sg_len]) = dma_addr;
			xfer->rx_sg_len++;
		} else {
			tx_num_word += DIV_ROUND_UP(cmd->tx_len, 4);
			dma_addr = dma_map_single(master->dma_tx->device->dev,
						  cmd->tx_buf, cmd->tx_len, DMA_TO_DEVICE);
			if (dma_mapping_error(master->dma_tx->device->dev, dma_addr)) {
				dev_err(master->dev, "dma tx map failed\n");
				return -EINVAL;
			}

			sg_dma_len(&master->tx_sg[xfer->tx_sg_len]) =
				DIV_ROUND_UP(cmd->tx_len, xfer->tx_trig * 4) * 4 * xfer->tx_trig;
			sg_dma_address(&master->tx_sg[xfer->tx_sg_len]) = dma_addr;
			xfer->tx_sg_len++;
		}
		writel_relaxed(cmd->fifo_cmd, master->regs + CMD);
	}

	if (xfer->rx_sg_len > 0) {
		struct dma_slave_config rxconf = {
			.direction = DMA_DEV_TO_MEM,
			.src_addr = master->dma_addr_rx,
			.src_addr_width = xfer->rx_burst_size,
			.src_maxburst = xfer->rx_burst_len,
		};

		dmaengine_slave_config(master->dma_rx, &rxconf);
		writel_relaxed(DMA_RXFIFO_TRIG(xfer->rx_trig),
			       master->regs + FIFO_TRIG_DMA);
		writel_relaxed(rx_num_word, master->regs + RXFIFO_NUM_DMA);

		rxdesc = dmaengine_prep_slave_sg(master->dma_rx, master->rx_sg,
						 xfer->rx_sg_len, DMA_DEV_TO_MEM,
						 DMA_PREP_INTERRUPT);
		if (!rxdesc) {
			if (txdesc)
				dmaengine_terminate_sync(master->dma_tx);
			return -EINVAL;
		}

		/* last cmd gets the callback */
		if (xfer->cmds[xfer->ncmds - 1].fifo_cmd & CMD_RNW)
			rxdesc->callback = rockchip_i3c_master_dma_irq_callback;
		else
			rxdesc->callback = NULL;
		rxdesc->callback_param = master;

		cookie = dmaengine_submit(rxdesc);
		if (dma_submit_error(cookie)) {
			dev_err(master->dev, "submitting dma rx failed\n");
			rockchip_i3c_maste_cleanup_dma(master);
			return -EINVAL;
		}
		dma_async_issue_pending(master->dma_rx);
	}

	if (xfer->tx_sg_len > 0) {
		struct dma_slave_config txconf = {
			.direction = DMA_MEM_TO_DEV,
			.dst_addr = master->dma_addr_tx,
			.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES,
			.dst_maxburst = xfer->tx_trig,
		};

		dmaengine_slave_config(master->dma_tx, &txconf);
		/* should match the DMA burst size */
		writel_relaxed(DMA_TXFIFO_TRIG(xfer->tx_trig),
			       master->regs + FIFO_TRIG_DMA);
		writel_relaxed(tx_num_word, master->regs + TXFIFO_NUM_DMA);

		txdesc = dmaengine_prep_slave_sg(master->dma_tx, master->tx_sg,
						 xfer->tx_sg_len, DMA_MEM_TO_DEV,
						 DMA_PREP_INTERRUPT);
		if (!txdesc) {
			if (rxdesc)
				dmaengine_terminate_sync(master->dma_rx);
			return -EINVAL;
		}

		/* last cmd gets the callback */
		if (!(xfer->cmds[xfer->ncmds - 1].fifo_cmd & CMD_RNW))
			txdesc->callback = rockchip_i3c_master_dma_irq_callback;
		else
			txdesc->callback = NULL;
		txdesc->callback_param = master;

		cookie = dmaengine_submit(txdesc);
		if (dma_submit_error(cookie)) {
			dev_err(master->dev, "submitting dma tx failed\n");
			rockchip_i3c_maste_cleanup_dma(master);
			return -EINVAL;
		}
		dma_async_issue_pending(master->dma_tx);
	}

	return 0;
}

static void rockchip_i3c_master_data_isr_write(struct rockchip_i3c_master *master,
					       struct rockchip_i3c_xfer *xfer,
					       unsigned int tx_num_word)
{
	unsigned int tx_space = tx_num_word;
	struct rockchip_i3c_cmd *cmd;
	int i;

	for (i = xfer->cur_tx_cmd; i < xfer->ncmds; i++) {
		cmd = &xfer->cmds[i];
		if (!(cmd->fifo_cmd & CMD_RNW))	{
			u16 min_tx_len, word;

			/* tx fifo is full or tx data was written to fifo */
			if (tx_space <= 0 || xfer->tx_num_word <= 0)
				break;
			min_tx_len = min(4 * tx_space, cmd->tx_len - xfer->cur_tx_len);
			word = rockchip_i3c_master_wr_to_tx_fifo(master,
				(u8 *)&cmd->tx_buf[xfer->cur_tx_len], min_tx_len);

			tx_space -= word;
			xfer->tx_num_word = xfer->tx_num_word - word;
			xfer->cur_tx_len += min_tx_len;
			if (xfer->cur_tx_len >= cmd->tx_len) {
				xfer->cur_tx_cmd = i + 1;
				xfer->cur_tx_len = 0;
			}
		}
	}
}

static int rockchip_i3c_master_data_isr_read(struct rockchip_i3c_master *master,
					     struct rockchip_i3c_xfer *xfer,
					     unsigned int rx_num_word)
{
	unsigned int rx_left = rx_num_word;
	struct rockchip_i3c_cmd *cmd;
	int i;

	for (i = xfer->cur_rx_cmd; i < xfer->ncmds; i++) {
		cmd = &xfer->cmds[i];
		if (cmd->fifo_cmd & CMD_RNW) {
			u16 min_rx_len, word;

			/* rx fifo is empty or rx data was read to memory */
			if (rx_left <= 0 || xfer->rx_num_word <= 0)
				break;
			min_rx_len = min(4 * rx_left, cmd->rx_len - xfer->cur_rx_len);
			word = rockchip_i3c_master_rd_from_rx_fifo(master,
				(u8 *)&cmd->rx_buf[xfer->cur_rx_len], min_rx_len);
			rx_left -= word;
			xfer->rx_num_word = xfer->rx_num_word - word;
			xfer->cur_rx_len += min_rx_len;
			if (xfer->cur_rx_len >= cmd->rx_len) {
				xfer->cur_rx_cmd = i + 1;
				xfer->cur_rx_len = 0;
			}
		}
	}

	return 0;
}

static int rockchip_i3c_master_data_isr(struct rockchip_i3c_master *master, u32 isr)
{
	unsigned int fifo_status = readl_relaxed(master->regs + FIFO_STATUS);
	unsigned int rx_num_word = RXFIFO_SPACE2EMPTY_NUMWORD(fifo_status);
	unsigned int tx_num_word = TXFIFO_SPACE2FULL_NUMWORD(fifo_status);

	struct rockchip_i3c_xfer *xfer = master->xferqueue.cur;

	if (xfer->rx_num_word > 0 && rx_num_word > 0)
		rockchip_i3c_master_data_isr_read(master, xfer, rx_num_word);

	if (xfer->tx_num_word > 0 && tx_num_word > 0)
		rockchip_i3c_master_data_isr_write(master, xfer, tx_num_word);

	return 0;
}

static int rockchip_i3c_master_prepare_irq(struct rockchip_i3c_master *master,
					   struct rockchip_i3c_xfer *xfer)
{
	/* At first, init global values with 0 */
	xfer->cur_tx_cmd = xfer->cur_rx_cmd = 0;
	xfer->cur_tx_len = xfer->cur_rx_len = 0;

	writel_relaxed(I2C_SLV_HOLD_SCL_CONF, master->regs + SLVSCL_CONF_I2C);
	/* tx fifo trig */
	writel_relaxed(IRQ_TXFIFO_TRIG(DATAFIFO_DEPTH), master->regs + FIFO_TRIG_CPU);
	/* rx fifo trig */
	writel_relaxed(IRQ_RXFIFO_TRIG(DATAFIFO_DEPTH), master->regs + FIFO_TRIG_CPU);

	/* clean DMA conf */
	writel_relaxed(0x0, master->regs + DMA_CONF);

	writel_relaxed(xfer->tx_num_word, master->regs + TXFIFO_NUM_CPU);
	writel_relaxed(xfer->rx_num_word, master->regs + RXFIFO_NUM_CPU);

	if (xfer->tx_num_word > 0)
		rockchip_i3c_master_data_isr_write(master, xfer, DATAFIFO_DEPTH);

	return 0;
}

static void rockchip_i3c_master_start_xfer_locked(struct rockchip_i3c_master *master)
{
	struct rockchip_i3c_xfer *xfer = master->xferqueue.cur;
	struct rockchip_i3c_cmd *cmd;
	u32 fifo_cmd;
	int i;

	if (!xfer)
		return;

	writel_relaxed(FLUSH_CTRL_ALL, master->regs + FLUSH_CTL);
	writel_relaxed(INTR_ALL, master->regs + INT_STATUS);

	switch (xfer->mode) {
	case BROCAST_CCC:
		/* only one cmd, rnw = 0, length = cmd->tx_len + 1. */
		for (i = 0; i < xfer->ncmds; i++) {
			cmd = &xfer->cmds[i];
			writel_relaxed(cmd->fifo_cmd, master->regs + CMD);
			rockchip_i3c_master_wr_ccc_to_tx_fifo(master,
				cmd->tx_buf, cmd->tx_len, xfer->ccc_cmd);
		}
		break;
	case DIRECT_CCC:
		/* more than one cmd, first cmd's tx fifo write ccc cmd to tx fifo,
		 * write tx data to txfifo if rnw = 0.
		 */
		writel_relaxed(xfer->ccc_cmd, master->regs + TXDATA);
		fifo_cmd = CMD_DEV_ADDR(0x7e) | CMD_FIFO_PL_LEN(1) | CMD_ACK2NEXT;
		writel_relaxed(fifo_cmd, master->regs + CMD);
		for (i = 0; i < xfer->ncmds; i++) {
			cmd = &xfer->cmds[i];
			rockchip_i3c_master_wr_to_tx_fifo(master, cmd->tx_buf,
							  cmd->tx_len);
			writel_relaxed(cmd->fifo_cmd, master->regs + CMD);
		}
		break;
	case DO_DAA:
		/* Send ENTDAA cmd first, then send is_entdaa 7E/R cmd */
		writel_relaxed(xfer->ccc_cmd, master->regs + TXDATA);
		fifo_cmd = CMD_DEV_ADDR(0x7e) | CMD_FIFO_PL_LEN(1) | CMD_ACK2NEXT;
		writel_relaxed(fifo_cmd, master->regs + CMD);
		for (i = 0; i < xfer->ncmds; i++) {
			cmd = &xfer->cmds[i];
			cmd->fifo_cmd |= CMD_DEV_ADDR(0x7e) | CMD_I3C_ENTDAA_MODE |
					 CMD_RNW | CMD_ACK2NEXT;
			if (i == xfer->ncmds - 1)
				cmd->fifo_cmd &= ~CMD_ACK2NEXT;

			writel_relaxed(cmd->fifo_cmd, master->regs + CMD);
		}
		break;
	case PRIVATE_TRANSFER_WITH7E:
		/* more than one cmd, first cmd, write 0x7e to cmd */
		fifo_cmd = CMD_DEV_ADDR(0x7e) | CMD_I3C_ADDR_ONLY_MODE | CMD_ACK2NEXT;
		writel_relaxed(fifo_cmd, master->regs + CMD);
		fallthrough;
	case PRIVATE_TRANSFER_WITHOUT7E:
		if (xfer->xfer_mode == ROCKCHIP_I3C_DMA) {
			rockchip_i3c_master_prepate_dma_sg(master, xfer);
		} else if (xfer->xfer_mode == ROCKCHIP_I2C_IRQ) {
			rockchip_i3c_master_prepare_irq(master, xfer);
			for (i = 0; i < xfer->ncmds; i++) {
				cmd = &xfer->cmds[i];
				writel_relaxed(cmd->fifo_cmd, master->regs + CMD);
			}
		} else {
			for (i = 0; i < xfer->ncmds; i++) {
				cmd = &xfer->cmds[i];
				rockchip_i3c_master_wr_to_tx_fifo(master, cmd->tx_buf,
								  cmd->tx_len);
				writel_relaxed(cmd->fifo_cmd, master->regs + CMD);
			}
		}

		break;
	default:
		break;
	}

	if (xfer->xfer_mode == ROCKCHIP_I3C_DMA)
		writel_relaxed(IRQ_END_CLR | IRQ_CPU_FIFOTRIG_CLR, master->regs + INT_EN);
	else if (xfer->xfer_mode == ROCKCHIP_I2C_IRQ)
		writel_relaxed(IRQ_END_IEN | IRQ_CPU_FIFOTRIG_IEN, master->regs + INT_EN);
	else
		writel_relaxed(IRQ_END_IEN | IRQ_CPU_FIFOTRIG_CLR, master->regs + INT_EN);

	/* Use FM speed before daa done */
	if (!master->daa_done)
		writel_relaxed(MODULE_CTL_FM_START_EN, master->regs + MODULE_CTL);
	else
		writel_relaxed(MODULE_CTL_START_EN, master->regs + MODULE_CTL);

	writel_relaxed(REG_READY_CONF_EN, master->regs + REG_READY_CONF);
}

static void rockchip_i3c_master_end_xfer_locked(struct rockchip_i3c_master *master,
						u32 isr)
{
	struct rockchip_i3c_xfer *xfer = master->xferqueue.cur;
	u32 finish = 0, cmd_status_offset = 0;
	int i, ret = 0;

	if (!xfer)
		return;

	if (!(isr & IRQ_END_STS)) {
		dev_warn_ratelimited(master->dev, "xfer finish not end.\n");
		return;
	}

	finish = FINISHED_CMD_ID(isr);
	/* last cmd is 0, maybe ibi finish or nack happened for this transfer */
	if (!finish) {
		xfer->cmds[0].error = CMDR_M2_ERROR;
		goto done;
	}

	/* The three modes should do a extra cmd, now we parsed the cmds[0] status
	 * at first.
	 */
	if (xfer->mode == DIRECT_CCC || xfer->mode == PRIVATE_TRANSFER_WITH7E ||
	    xfer->mode == DO_DAA) {
		u32 cmdr;

		cmd_status_offset = 1;
		cmdr = readl_relaxed(master->regs + CMD1_STATUS);
		if (cmdr & STATUS_ADDRNAK) {
			xfer->cmds[0].error = CMDR_M2_ERROR;
			goto done;
		} else if (cmdr & STATUS_TXEARLY_I2C) {
			xfer->cmds[0].error = CMDR_M0_ERROR;
			goto done;
		}
		if (finish > 0)
			finish -= 1;
	}

	/* if it was i2c irq mode, read the left rx data in rx fifo by fifostatus.
	 * if it was i3c, the device "t = 0" should be cared, the following handles
	 * is wrong for this case.
	 */
	if (xfer->xfer_mode == ROCKCHIP_I2C_IRQ) {
		unsigned int fifo_status = readl_relaxed(master->regs + FIFO_STATUS);
		unsigned int rx_num_word = RXFIFO_SPACE2EMPTY_NUMWORD(fifo_status);

		if (rx_num_word > 0)
			rockchip_i3c_master_data_isr_read(master, xfer, rx_num_word);

		writel_relaxed(0, master->regs + TXFIFO_NUM_CPU);
		writel_relaxed(0, master->regs + RXFIFO_NUM_CPU);
	}

	for (i = 0; i < finish; i++) {
		struct rockchip_i3c_cmd *ccmd;
		u32 cmdr, error;

		xfer->cmds[i].error = 0;
		cmdr = readl_relaxed(master->regs + CMD1_STATUS + 0x4 * cmd_status_offset);
		ccmd = &xfer->cmds[i];
		error = (cmdr & STATUS_ADDRNAK) | (cmdr & STATUS_TXEARLY_I2C);

		if (cmdr & STATUS_ERR) {
			if (cmdr & STATUS_TXEARLY_I2C) {
				xfer->cmds[i].error = CMDR_M0_ERROR;
				break;
			} else if (cmdr & STATUS_ADDRNAK) {
				xfer->cmds[i].error = CMDR_M2_ERROR;
				break;
			}
		} else {
			xfer->cmds[i].error = CMDR_NO_ERROR;
		}

		if (isr & INT_DATAERR_IRQPD_STS) {
			xfer->cmds[i].error = CMDR_M1_ERROR;
			break;
		}

		/* rnw = 1 */
		if ((cmdr & STATUS_RNW) && !error) {
			ccmd->rx_len = CMDR_XFER_BYTES(cmdr);
			if (xfer->xfer_mode == ROCKCHIP_I3C_POLL)
				rockchip_i3c_master_rd_from_rx_fifo(master, ccmd->rx_buf,
								    ccmd->rx_len);
		}

		cmd_status_offset++;
	}

done:
	for (i = 0; i < xfer->ncmds; i++) {
		switch (xfer->cmds[i].error) {
		case CMDR_NO_ERROR:
			break;
		case CMDR_M0_ERROR:
		case CMDR_M1_ERROR:
		case CMDR_M2_ERROR:
		case CMDR_MST_ABORT:
		case CMDR_NACK_RESP:
			ret = -EIO;
			break;
		default:
			ret = -EINVAL;
			break;
		}
	}

	xfer->ret = ret;
	if (xfer->xfer_mode != ROCKCHIP_I3C_DMA)
		complete(&xfer->comp);

	xfer = list_first_entry_or_null(&master->xferqueue.list,
					struct rockchip_i3c_xfer, node);
	if (xfer)
		list_del_init(&xfer->node);

	master->xferqueue.cur = xfer;
	rockchip_i3c_master_start_xfer_locked(master);
}

static enum i3c_error_code rockchip_i3c_master_cmd_get_err(struct rockchip_i3c_cmd *cmd)
{
	switch (cmd->error) {
	case CMDR_M0_ERROR:
		return I3C_ERROR_M0;

	case CMDR_M1_ERROR:
		return I3C_ERROR_M1;

	case CMDR_M2_ERROR:
	case CMDR_NACK_RESP:
		return I3C_ERROR_M2;

	default:
		break;
	}

	return I3C_ERROR_UNKNOWN;
}

static int rockchip_i3c_master_send_ccc_cmd(struct i3c_master_controller *m,
					    struct i3c_ccc_cmd *cmd)
{
	struct rockchip_i3c_master *master = to_rockchip_i3c_master(m);
	struct rockchip_i3c_xfer *xfer;
	struct rockchip_i3c_cmd *ccmd;
	int ret;

	xfer = rockchip_i3c_master_alloc_xfer(master, 1);
	if (!xfer)
		return -ENOMEM;

	ccmd = xfer->cmds;
	if (cmd->id & I3C_CCC_DIRECT) {
		xfer->mode = DIRECT_CCC;
		ccmd->fifo_cmd = CMD_FIFO_PL_LEN(cmd->dests[0].payload.len);
		xfer->ccc_cmd = cmd->id;
	} else {
		xfer->mode = BROCAST_CCC;
		ccmd->fifo_cmd = CMD_FIFO_PL_LEN(cmd->dests[0].payload.len + 1);
		xfer->ccc_cmd = cmd->id & GENMASK(6, 0);
	}

	ccmd->fifo_cmd |= CMD_DEV_ADDR(cmd->dests[0].addr);
	if (cmd->rnw) {
		ccmd->fifo_cmd |= CMD_RNW | CMD_I3C_SDR_RX_MODE;
		ccmd->rx_buf = cmd->dests[0].payload.data;
		ccmd->rx_len = cmd->dests[0].payload.len;
	} else {
		ccmd->tx_buf = cmd->dests[0].payload.data;
		ccmd->tx_len = cmd->dests[0].payload.len;
	}

	/* when ibi nack happen, continue repeat start for current transfer */
	ccmd->fifo_cmd |= CMD_I3C_END2IBI;

	rockchip_i3c_master_queue_xfer(master, xfer);
	if (!wait_for_completion_timeout(&xfer->comp, msecs_to_jiffies(WAIT_TIMEOUT)))
		rockchip_i3c_master_unqueue_xfer(master, xfer);

	ret = xfer->ret;
	if (ret != -ETIMEDOUT) {
		cmd->err = rockchip_i3c_master_cmd_get_err(&xfer->cmds[0]);
		if (cmd->rnw)
			cmd->dests[0].payload.len = ccmd->rx_len;
	} else {
		/* timeout error, maybe reset target */
		dev_err(master->dev, "timeout, ipd: 0x%x for ccc cmd\n",
			readl_relaxed(master->regs + INT_STATUS));
	}

	rockchip_i3c_master_free_xfer(xfer);

	return ret;
}

static int rockchip_i3c_master_priv_xfers(struct i3c_dev_desc *dev,
					  struct i3c_priv_xfer *xfers,
					  int nxfers)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct rockchip_i3c_master *master = to_rockchip_i3c_master(m);
	struct rockchip_i3c_xfer *rockchip_xfer;
	int txslots = 0, rxslots = 0, i, ret;
	unsigned long timeleft;

	for (i = 0; i < nxfers; i++) {
		if (xfers[i].len > CMD_FIFO_PL_LEN_MAX)
			return -EOPNOTSUPP;
	}

	if (!nxfers)
		return -EINVAL;

	if (nxfers > master->caps.cmdfifodepth)
		return -EOPNOTSUPP;

	rockchip_xfer = rockchip_i3c_master_alloc_xfer(master, nxfers);
	if (!rockchip_xfer)
		return -ENOMEM;

	for (i = 0; i < nxfers; i++) {
		if (xfers[i].rnw)
			rxslots += DIV_ROUND_UP(xfers[i].len, 4);
		else
			txslots += DIV_ROUND_UP(xfers[i].len, 4);
	}

	if ((rxslots > master->caps.datafifodepth ||
	     txslots > master->caps.datafifodepth) &&
	    (master->dma_rx && master->dma_tx))
		rockchip_xfer->xfer_mode = ROCKCHIP_I3C_DMA;
	else if (rxslots <= master->caps.datafifodepth &&
		 txslots <= master->caps.datafifodepth)
		rockchip_xfer->xfer_mode = ROCKCHIP_I3C_POLL;
	else
		return -EOPNOTSUPP;

	/* Use without 7E defaultly, if use PRIVATE_TRANSFER_WITH7E,
	 * the nxfers should less than "cmdfifodepth - 1".
	 */
	rockchip_xfer->mode = PRIVATE_TRANSFER_WITHOUT7E;
	if (rockchip_xfer->xfer_mode == ROCKCHIP_I3C_DMA) {
		rockchip_xfer->rx_trig = DMA_TRIG_LEVEL;
		rockchip_xfer->tx_trig = DMA_TRIG_LEVEL;
		rockchip_xfer->rx_burst_size = DMA_RX_MAX_BURST_SIZE;
		rockchip_xfer->rx_burst_len = DMA_RX_MAX_BURST_LEN;
	}

	for (i = 0; i < nxfers; i++) {
		struct rockchip_i3c_cmd *ccmd = &rockchip_xfer->cmds[i];
		u32 pl_len;

		pl_len = xfers[i].len;
		ccmd->fifo_cmd = CMD_DEV_ADDR(dev->info.dyn_addr);

		if (xfers[i].rnw) {
			ccmd->fifo_cmd |= CMD_RNW | CMD_I3C_SDR_RX_MODE | CMD_I3C_ACK2EARLY;
			if (rockchip_xfer->xfer_mode == ROCKCHIP_I3C_DMA) {
				u8 rx_trig, rx_burst_size, rx_burst_len, *buf;

				rx_burst_size = rockchip_i3c_calc_rx_burst(xfers[i].len, false);
				if (rockchip_xfer->rx_burst_size > rx_burst_size)
					rockchip_xfer->rx_burst_size = rx_burst_size;

				rx_burst_len = rockchip_i3c_calc_rx_burst(xfers[i].len, true);
				if (rockchip_xfer->rx_burst_len > rx_burst_len)
					rockchip_xfer->rx_burst_len = rx_burst_len;

				rx_trig = rockchip_i3c_master_calc_trig_level(xfers[i].len, true);
				if (rockchip_xfer->rx_trig > rx_trig)
					rockchip_xfer->rx_trig = rx_trig;

				buf = rockchip_i3c_master_get_dma_safe_buf(master, xfers[i].data.in,
									   xfers[i].len, true);
				ccmd->rx_buf = buf;
			} else {
				ccmd->rx_buf = xfers[i].data.in;
			}
			ccmd->rx_len = xfers[i].len;
		} else {
			ccmd->fifo_cmd |= CMD_I3C_SDR_TX_MODE;
			if (rockchip_xfer->xfer_mode == ROCKCHIP_I3C_DMA) {
				u8 tx_trig, *buf;

				tx_trig = rockchip_i3c_master_calc_trig_level(xfers[i].len, false);
				if (rockchip_xfer->tx_trig > tx_trig)
					rockchip_xfer->tx_trig = tx_trig;

				buf = rockchip_i3c_master_get_dma_safe_buf(master,
									   (u8 *)xfers[i].data.out,
									   xfers[i].len, false);
				ccmd->tx_buf = buf;
			} else {
				ccmd->tx_buf = (void *)xfers[i].data.out;
			}
			ccmd->tx_len = xfers[i].len;
		}

		/* when ibi nack happen, continue repeat start for current transfer */
		ccmd->fifo_cmd |= CMD_FIFO_PL_LEN(pl_len) | CMD_I3C_END2IBI;
		if (i < (nxfers - 1))
			ccmd->fifo_cmd |= CMD_ACK2NEXT;
	}

	rockchip_i3c_master_queue_xfer(master, rockchip_xfer);
	timeleft = wait_for_completion_timeout(&rockchip_xfer->comp,
					       msecs_to_jiffies(WAIT_TIMEOUT));
	if (!timeleft) {
		if (rockchip_xfer->xfer_mode == ROCKCHIP_I3C_DMA)
			rockchip_i3c_maste_cleanup_dma(master);
		rockchip_i3c_master_unqueue_xfer(master, rockchip_xfer);
	} else {
		if (rockchip_xfer->xfer_mode == ROCKCHIP_I3C_DMA) {
			unsigned int status;

			/* if the last is tx, make sure tx is completed for controller */
			status = rockchip_i3c_master_wait_for_tx_idle(master);
			if (status & IRQ_END_STS)
				rockchip_i3c_master_end_xfer_locked(master, status);
		}
	}

	for (i = 0; i < nxfers; i++) {
		struct rockchip_i3c_cmd *cmd = &rockchip_xfer->cmds[i];

		if (rockchip_xfer->xfer_mode == ROCKCHIP_I3C_DMA) {
			u8 *buf, *target;

			target = xfers[i].data.in;
			buf = xfers[i].rnw ? cmd->rx_buf : cmd->tx_buf;
			rockchip_i3c_master_put_dma_safe_buf(master, buf, target, xfers[i].len,
							     rockchip_xfer->xferred,
							     xfers[i].rnw);
		}
		xfers[i].err = rockchip_i3c_master_cmd_get_err(&rockchip_xfer->cmds[i]);
		if (xfers[i].rnw)
			xfers[i].len = cmd->rx_len;
	}

	ret = rockchip_xfer->ret;
	if (ret == -ETIMEDOUT)
		/* timeout error, maybe reset target */
		dev_err(master->dev, "timeout, ipd: 0x%x for i3c transfer\n",
			readl_relaxed(master->regs + INT_STATUS));

	rockchip_i3c_master_free_xfer(rockchip_xfer);

	return ret;
}

static int rockchip_i3c_master_i2c_xfers(struct i2c_dev_desc *dev,
					 const struct i2c_msg *xfers,
					 int nxfers)
{
	struct i3c_master_controller *m = i2c_dev_get_master(dev);
	struct rockchip_i3c_master *master = to_rockchip_i3c_master(m);
	unsigned int nrxwords = 0, ntxwords = 0;
	struct rockchip_i3c_xfer *xfer;
	unsigned long timeleft;
	int i, ret = 0;

	if (nxfers > master->caps.cmdfifodepth)
		return -EOPNOTSUPP;

	for (i = 0; i < nxfers; i++) {
		if (xfers[i].len > CMD_FIFO_PL_LEN_MAX)
			return -EOPNOTSUPP;
		if (xfers[i].flags & I2C_M_RD)
			nrxwords += DIV_ROUND_UP(xfers[i].len, 4);
		else
			ntxwords += DIV_ROUND_UP(xfers[i].len, 4);
	}

	xfer = rockchip_i3c_master_alloc_xfer(master, nxfers);
	if (!xfer)
		return -ENOMEM;

	if ((nrxwords > master->caps.datafifodepth ||
	     ntxwords > master->caps.datafifodepth) &&
	    (master->dma_rx && master->dma_tx))
		xfer->xfer_mode = ROCKCHIP_I3C_DMA;
	else if ((nrxwords > master->caps.datafifodepth ||
		  ntxwords > master->caps.datafifodepth) &&
		 (!master->dma_rx || !master->dma_tx))
		xfer->xfer_mode = ROCKCHIP_I2C_IRQ;
	else
		xfer->xfer_mode = ROCKCHIP_I3C_POLL;

	xfer->rx_num_word = nrxwords;
	xfer->tx_num_word = ntxwords;

	/* Use without 7E defaultly, if use PRIVATE_TRANSFER_WITH7E,
	 * the nxfers should less than "cmdfifodepth - 1".
	 */
	xfer->mode = PRIVATE_TRANSFER_WITHOUT7E;
	if (xfer->xfer_mode == ROCKCHIP_I3C_DMA ||
	    xfer->xfer_mode == ROCKCHIP_I2C_IRQ) {
		xfer->rx_trig = DMA_TRIG_LEVEL;
		xfer->tx_trig = DMA_TRIG_LEVEL;
		xfer->rx_burst_size = DMA_RX_MAX_BURST_SIZE;
		xfer->rx_burst_len = DMA_RX_MAX_BURST_LEN;
	}

	for (i = 0; i < nxfers; i++) {
		struct rockchip_i3c_cmd *ccmd = &xfer->cmds[i];

		ccmd->fifo_cmd = CMD_DEV_ADDR(xfers[i].addr) |
				 CMD_FIFO_PL_LEN(xfers[i].len);

		if (xfers[i].flags & I2C_M_TEN)
			return -EOPNOTSUPP;

		if (xfers[i].flags & I2C_M_RD) {
			ccmd->fifo_cmd |= CMD_RNW | CMD_I2C_RX | CMD_I2C_ACK2LAST;
			if (xfer->xfer_mode == ROCKCHIP_I3C_DMA) {
				u8 rx_trig, rx_burst_size, rx_burst_len, *buf;

				rx_burst_size = rockchip_i3c_calc_rx_burst(xfers[i].len, false);
				if (xfer->rx_burst_size > rx_burst_size)
					xfer->rx_burst_size = rx_burst_size;

				rx_burst_len = rockchip_i3c_calc_rx_burst(xfers[i].len, true);
				if (xfer->rx_burst_len > rx_burst_len)
					xfer->rx_burst_len = rx_burst_len;

				rx_trig = rockchip_i3c_master_calc_trig_level(xfers[i].len, true);
				if (xfer->rx_trig > rx_trig)
					xfer->rx_trig = rx_trig;

				buf = rockchip_i3c_master_get_dma_safe_buf(master, xfers[i].buf,
									   xfers[i].len, true);
				ccmd->rx_buf = buf;
			} else {
				ccmd->rx_buf = xfers[i].buf;
			}
			ccmd->rx_len = xfers[i].len;
		} else {
			ccmd->fifo_cmd |= CMD_I2C_TX;
			if (xfer->xfer_mode == ROCKCHIP_I3C_DMA) {
				u8 tx_size, *buf;

				tx_size = rockchip_i3c_master_calc_trig_level(xfers[i].len, false);
				if (xfer->tx_trig > tx_size)
					xfer->tx_trig = tx_size;
				buf = rockchip_i3c_master_get_dma_safe_buf(master,
									   (u8 *)xfers[i].buf,
									   xfers[i].len, false);
				ccmd->tx_buf = buf;
			} else {
				ccmd->tx_buf = xfers[i].buf;
			}
			ccmd->tx_len = xfers[i].len;
		}

		if (i < (nxfers - 1))
			ccmd->fifo_cmd |= CMD_ACK2NEXT;
	}

	rockchip_i3c_master_queue_xfer(master, xfer);
	timeleft = wait_for_completion_timeout(&xfer->comp,
					       msecs_to_jiffies(WAIT_TIMEOUT));
	if (!timeleft) {
		if (xfer->xfer_mode == ROCKCHIP_I3C_DMA)
			rockchip_i3c_maste_cleanup_dma(master);
		rockchip_i3c_master_unqueue_xfer(master, xfer);
	} else {
		if (xfer->xfer_mode == ROCKCHIP_I3C_DMA) {
			unsigned int status;

			/* if the last is tx, make sure tx is completed for controller */
			status = rockchip_i3c_master_wait_for_tx_idle(master);
			if (status & IRQ_END_STS)
				rockchip_i3c_master_end_xfer_locked(master, status);
		}
	}

	for (i = 0; i < nxfers; i++) {
		struct rockchip_i3c_cmd *ccmd = &xfer->cmds[i];

		if (xfer->xfer_mode == ROCKCHIP_I3C_DMA) {
			u8 *buf, *target;

			buf = (xfers[i].flags & I2C_M_RD) ? ccmd->rx_buf : ccmd->tx_buf;
			target = xfers[i].buf;
			rockchip_i3c_master_put_dma_safe_buf(master, buf, target, xfers[i].len,
							     xfer->xferred,
							     xfers[i].flags & I2C_M_RD);
		}
	}

	ret = xfer->ret;
	if (ret == -ETIMEDOUT)
		/* timeout error, maybe reset target */
		dev_err(master->dev, "timeout, ipd: 0x%x for i3c transfer\n",
			readl_relaxed(master->regs + INT_STATUS));

	rockchip_i3c_master_free_xfer(xfer);

	return ret;
}

static inline int rockchip_i3c_master_get_addr_pos(struct rockchip_i3c_master *master,
						   u8 addr)
{
	int pos;

	for (pos = 0; pos < master->maxdevs; pos++) {
		if (addr == master->devs[pos].addr)
			return pos;
	}

	return -EINVAL;
}

static inline int rockchip_i3c_master_get_free_pos(struct rockchip_i3c_master *master)
{
	if (!(master->free_pos & GENMASK(master->maxdevs - 1, 0)))
		return -ENOSPC;

	return ffs(master->free_pos) - 1;
}

static int rockchip_i3c_master_reattach_i3c_dev(struct i3c_dev_desc *dev,
						unsigned char old_dyn_addr)
{
	return 0;
}

static int rockchip_i3c_master_attach_i3c_dev(struct i3c_dev_desc *dev)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct rockchip_i3c_master *master = to_rockchip_i3c_master(m);
	struct rockchip_i3c_i2c_dev_data *data;
	int slot;

	slot = rockchip_i3c_master_get_free_pos(master);
	if (slot < 0)
		return slot;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->ibi = -1;
	data->id = slot;
	master->devs[slot].addr = dev->info.dyn_addr ? : dev->info.static_addr;
	master->free_pos &= ~BIT(slot);
	i3c_dev_set_master_data(dev, data);

	return 0;
}

static void rockchip_i3c_master_detach_i3c_dev(struct i3c_dev_desc *dev)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct rockchip_i3c_master *master = to_rockchip_i3c_master(m);
	struct rockchip_i3c_i2c_dev_data *data = i3c_dev_get_master_data(dev);

	i3c_dev_set_master_data(dev, NULL);
	master->devs[data->id].addr = 0;
	master->free_pos |= BIT(data->id);
	kfree(data);
}

static int rockchip_i3c_master_attach_i2c_dev(struct i2c_dev_desc *dev)
{
	struct i3c_master_controller *m = i2c_dev_get_master(dev);
	struct rockchip_i3c_master *master = to_rockchip_i3c_master(m);
	struct rockchip_i3c_i2c_dev_data *data;
	int slot;

	slot = rockchip_i3c_master_get_free_pos(master);
	if (slot < 0)
		return slot;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->id = slot;
	master->devs[slot].addr = dev->addr;
	master->free_pos &= ~BIT(slot);

	i2c_dev_set_master_data(dev, data);

	return 0;
}

static void rockchip_i3c_master_detach_i2c_dev(struct i2c_dev_desc *dev)
{
	struct i3c_master_controller *m = i2c_dev_get_master(dev);
	struct rockchip_i3c_master *master = to_rockchip_i3c_master(m);
	struct rockchip_i3c_i2c_dev_data *data = i2c_dev_get_master_data(dev);

	master->devs[data->id].addr = 0;
	master->free_pos |= BIT(data->id);
	i2c_dev_set_master_data(dev, NULL);
	kfree(data);
}

static void rockchip_i3c_master_bus_cleanup(struct i3c_master_controller *m)
{
	struct rockchip_i3c_master *master = to_rockchip_i3c_master(m);

	rockchip_i3c_master_disable(master);
}

static int rockchip_i3c_master_do_daa(struct i3c_master_controller *m)
{
	struct rockchip_i3c_master *master = to_rockchip_i3c_master(m);
	struct rockchip_i3c_xfer *xfer;
	int ret = 0, pos, n = 0, last, i;
	u32 olddevs, newdevs;
	u8 last_addr = 0;
	u32 daa_status;

	olddevs = ~(master->free_pos);
	/* Prepare DAT before launching DAA */
	for (pos = 1; pos < master->maxdevs; pos++) {
		if (olddevs & BIT(pos))
			continue;

		ret = i3c_master_get_free_addr(m, last_addr + 1);
		if (ret < 0)
			return -ENOSPC;

		last_addr = ret;
		master->devs[pos].addr = ret;
		writel_relaxed(ret, master->regs + DEV_ID_RR0(n));
		n++;
	}

	xfer = rockchip_i3c_master_alloc_xfer(master, n);
	if (!xfer)
		return -ENOMEM;

	xfer->mode = DO_DAA;
	xfer->ccc_cmd = I3C_CCC_ENTDAA;

	rockchip_i3c_master_queue_xfer(master, xfer);
	if (!wait_for_completion_timeout(&xfer->comp, msecs_to_jiffies(WAIT_TIMEOUT)))
		rockchip_i3c_master_unqueue_xfer(master, xfer);

	for (i = 0; i < n; i++)
		xfer->cmds[i].error = rockchip_i3c_master_cmd_get_err(&xfer->cmds[i]);

	daa_status = readl_relaxed(master->regs + DAA_STATUS);
	last = daa_status & DAA_ASSIGNED_CNT_MASK;
	if (last >= 1)
		ret = 0;
	else
		ret = xfer->ret;
	if (!ret)
		master->daa_done = true;

	newdevs = GENMASK(master->maxdevs - last, 0);
	newdevs &= ~olddevs;

	for (pos = 1, n = 0; pos < master->maxdevs; pos++) {
		if (n >= last)
			break;

		if (newdevs & BIT(pos)) {
			if (!xfer->cmds[n].error)
				i3c_master_add_i3c_dev_locked(m, master->devs[pos].addr);
			n++;
		}
	}

	rockchip_i3c_master_free_xfer(xfer);

	return ret;
}

static void rockchip_i3c_master_handle_ibi(struct rockchip_i3c_master *master,
					   u32 ibir)
{
	struct rockchip_i3c_i2c_dev_data *data;
	bool data_consumed = false;
	struct i3c_ibi_slot *slot;
	u32 id = IBI_SLVID(ibir);
	struct i3c_dev_desc *dev;
	size_t nbytes;
	u8 *buf;

	if (id >= master->ibi.num_slots || (ibir & IBI_NACK))
		goto out;

	dev = master->ibi.slots[id];
	spin_lock(&master->ibi.lock);

	data = i3c_dev_get_master_data(dev);
	slot = i3c_generic_ibi_get_free_slot(data->ibi_pool);
	if (!slot)
		goto out_unlock;

	buf = slot->data;
	nbytes = IBI_XFER_BYTES(ibir);
	readsl(master->regs + IBIDATA, buf, nbytes / 4);
	if (nbytes % 3) {
		u32 tmp = readl_relaxed(master->regs + IBIDATA);

		memcpy(buf + (nbytes & ~3), &tmp, nbytes & 3);
	}

	slot->len = min_t(unsigned int, IBI_XFER_BYTES(ibir),
			  dev->ibi->max_payload_len);
	i3c_master_queue_ibi(dev, slot);
	data_consumed = true;

out_unlock:
	spin_unlock(&master->ibi.lock);

out:
	/* Consume data from the FIFO if it's not been done already. */
	if (!data_consumed) {
		int i;

		for (i = 0; i < IBI_XFER_BYTES(ibir); i += 4)
			readl_relaxed(master->regs + IBIDATA);
	}
	/* At last, flush ibi status */
	writel_relaxed(FLUSH_IBI_ALL, master->regs + FLUSH_CTL);
}

static void rockchip_i3c_master_demux_ibis(struct rockchip_i3c_master *master,
					   u32 isr)
{
	u32 ibir = readl_relaxed(master->regs + IBI_STATUS);

	if (!(isr & I2CIBI_IRQPD_STS)) {
		switch (IBI_TYPE(ibir)) {
		case IBI_TYPE_IBI:
			rockchip_i3c_master_handle_ibi(master, ibir);
			break;

		case IBI_TYPE_HJ:
			queue_work(master->base.wq, &master->hj_work);
			break;

		case IBI_TYPE_MR:
			break;

		default:
			break;
		}
	} else {
		dev_err(&master->base.dev, "Get IBI request from 0x%lx at i2c transfer\n",
			I2C_IBIADDR(ibir));
	}
}

static irqreturn_t rockchip_i3c_master_interrupt(int irq, void *data)
{
	struct rockchip_i3c_master *master = data;
	u32 status, ien;

	ien = readl_relaxed(master->regs + INT_EN);
	status = readl_relaxed(master->regs + INT_STATUS);
	writel_relaxed(status | ERR_CMD_ID_MASK | FINISHED_CMD_ID_MASK,
		       master->regs + INT_STATUS);
	if (!(status & ien))
		return IRQ_NONE;

	spin_lock(&master->xferqueue.lock);
	if (status & IRQ_END_STS)
		rockchip_i3c_master_end_xfer_locked(master, status);
	else if (status & CPU_FIFOTRIG_IRQ_STS)
		rockchip_i3c_master_data_isr(master, status);

	if ((status & OWNIBI_IRQPD_STS) && (ien & OWNIBI_IRQPD_IEN))
		rockchip_i3c_master_demux_ibis(master, status);
	spin_unlock(&master->xferqueue.lock);

	return IRQ_HANDLED;
}

static int rockchip_i3c_master_request_ibi(struct i3c_dev_desc *dev,
					   const struct i3c_ibi_setup *req)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct rockchip_i3c_master *master = to_rockchip_i3c_master(m);
	struct rockchip_i3c_i2c_dev_data *data = i3c_dev_get_master_data(dev);
	unsigned long flags;

	data->ibi_pool = i3c_generic_ibi_alloc_pool(dev, req);
	if (IS_ERR(data->ibi_pool))
		return PTR_ERR(data->ibi_pool);

	spin_lock_irqsave(&master->ibi.lock, flags);
	data->ibi = data->id;
	master->ibi.slots[data->id] = dev;
	spin_unlock_irqrestore(&master->ibi.lock, flags);

	if (data->ibi < master->ibi.num_slots)
		return 0;

	i3c_generic_ibi_free_pool(data->ibi_pool);
	data->ibi_pool = NULL;

	return -ENOSPC;
}

static void rockchip_i3c_master_free_ibi(struct i3c_dev_desc *dev)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct rockchip_i3c_master *master = to_rockchip_i3c_master(m);
	struct rockchip_i3c_i2c_dev_data *data = i3c_dev_get_master_data(dev);
	unsigned long flags;

	spin_lock_irqsave(&master->ibi.lock, flags);
	master->ibi.slots[data->ibi] = NULL;
	data->ibi = -1;
	spin_unlock_irqrestore(&master->ibi.lock, flags);

	i3c_generic_ibi_free_pool(data->ibi_pool);
}

static int rockchip_i3c_master_disable_ibi(struct i3c_dev_desc *dev)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct rockchip_i3c_master *master = to_rockchip_i3c_master(m);
	unsigned long flags;
	int ret;

	ret = i3c_master_disec_locked(m, dev->info.dyn_addr, I3C_CCC_EVENT_SIR);
	if (ret)
		return ret;

	spin_lock_irqsave(&master->ibi.lock, flags);
	writel_relaxed(IBI_CONF_AUTOIBI_DIS, master->regs + IBI_CONF);
	writel_relaxed(OWNIBI_IRQPD_DIS, master->regs + INT_EN);
	spin_unlock_irqrestore(&master->ibi.lock, flags);

	return ret;
}

static int rockchip_i3c_master_enable_ibi(struct i3c_dev_desc *dev)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct rockchip_i3c_master *master = to_rockchip_i3c_master(m);
	struct rockchip_i3c_i2c_dev_data *data = i3c_dev_get_master_data(dev);
	unsigned long flags;
	unsigned int map;
	int ret;

	spin_lock_irqsave(&master->ibi.lock, flags);
	map = readl_relaxed(master->regs + IBI_MAP(data->id));
	map |= IBI_MAP_DEV_ADDR(data->id, dev->info.dyn_addr) |
	       IBI_MAP_DEV_PLLEN(data->id, dev->info.max_ibi_len);
	writel_relaxed(map, master->regs + IBI_MAP(data->id));

	writel_relaxed(OWNIBI_IRQPD_IEN, master->regs + INT_EN);
	writel_relaxed(IBI_CONF_AUTOIBI_EN, master->regs + IBI_CONF);
	writel_relaxed(IBI_REG_READY_CONF_EN, master->regs + REG_READY_CONF);
	spin_unlock_irqrestore(&master->ibi.lock, flags);

	ret = i3c_master_enec_locked(m, dev->info.dyn_addr, I3C_CCC_EVENT_SIR);
	if (ret) {
		spin_lock_irqsave(&master->ibi.lock, flags);
		writel_relaxed(IBI_CONF_AUTOIBI_DIS, master->regs + IBI_CONF);
		writel_relaxed(OWNIBI_IRQPD_DIS, master->regs + INT_EN);
		spin_unlock_irqrestore(&master->ibi.lock, flags);
	}

	return ret;
}

static void rockchip_i3c_master_recycle_ibi_slot(struct i3c_dev_desc *dev,
						 struct i3c_ibi_slot *slot)
{
	struct rockchip_i3c_i2c_dev_data *data = i3c_dev_get_master_data(dev);

	i3c_generic_ibi_recycle_slot(data->ibi_pool, slot);
}

static int rockchip_i3c_clk_cfg(struct rockchip_i3c_master *master)
{
	unsigned long rate, period;
	u32 scl_timing;
	int hcnt, lcnt, od_lcnt;

	rate = clk_get_rate(master->clk);
	if (!rate)
		return -EINVAL;

	period = DIV_ROUND_UP(1000000000, rate);
	hcnt = DIV_ROUND_UP(I3C_BUS_THIGH_MAX_NS, period);
	if (hcnt < SCL_I3C_TIMING_CNT_MIN)
		hcnt = SCL_I3C_TIMING_CNT_MIN;

	lcnt = DIV_ROUND_UP(rate, master->base.bus.scl_rate.i3c) - hcnt;
	if (lcnt < SCL_I3C_TIMING_CNT_MIN)
		lcnt = SCL_I3C_TIMING_CNT_MIN;

	od_lcnt = lcnt;
	hcnt = DIV_ROUND_UP(hcnt, 4) - 1;
	lcnt = DIV_ROUND_UP(lcnt, 4) - 1;

	scl_timing = SCL_I3C_CLKDIV_HCNT(hcnt) | SCL_I3C_CLKDIV_LCNT(lcnt);
	writel_relaxed(scl_timing, master->regs + CLKDIV_PP);

	od_lcnt = max_t(u8, DIV_ROUND_UP(I3C_BUS_TLOW_OD_MIN_NS * rate, 1000000000),
			od_lcnt);
	/* The scl raise time must be calculated for OD mode, as default
	 * FM+ mode's max raise time is 120ns, can't exceed 1/2 scl_low
	 * time for default.
	 */
	od_lcnt = max_t(u8, DIV_ROUND_UP(2 * I3C_BUS_I2C_FMP_SCL_RAISE_NS * rate, 1000000000),
			od_lcnt);
	od_lcnt = DIV_ROUND_UP(od_lcnt, 4) - 1;
	scl_timing = SCL_I3C_CLKDIV_HCNT(hcnt) | SCL_I3C_CLKDIV_LCNT(od_lcnt);
	writel_relaxed(scl_timing, master->regs + CLKDIV_OD);

	return 0;
}

static int rockchip_i2c_clk_cfg(struct rockchip_i3c_master *master)
{
	unsigned long rate, period;
	int hcnt, lcnt;
	u32 scl_timing;

	rate = clk_get_rate(master->clk);
	if (!rate)
		return -EINVAL;

	period = DIV_ROUND_UP(1000000000, rate);
	lcnt = DIV_ROUND_UP(DIV_ROUND_UP(I3C_BUS_I2C_FM_TLOW_MIN_NS, period), 4) - 1;
	/* Use FM mode for default */
	hcnt = DIV_ROUND_UP(DIV_ROUND_UP(rate, I3C_BUS_I2C_FM_SCL_RATE), 4) - lcnt - 1;
	scl_timing = SCL_I2C_CLKDIV_HCNT(hcnt) | SCL_I2C_CLKDIV_LCNT(lcnt);
	writel_relaxed(scl_timing, master->regs + CLKDIV_FM);

	return 0;
}

static int rockchip_i3c_master_bus_init(struct i3c_master_controller *m)
{
	struct rockchip_i3c_master *master = to_rockchip_i3c_master(m);
	struct i3c_bus *bus = i3c_master_get_bus(m);
	struct i3c_device_info info;
	int ret;

	switch (bus->mode) {
	case I3C_BUS_MODE_MIXED_FAST:
		writel_relaxed(MODULE_CONF_MIX_FAST_I3C, master->regs + MODULE_CONF);
		ret = rockchip_i2c_clk_cfg(master);
		if (ret)
			return ret;
		ret = rockchip_i3c_clk_cfg(master);
		if (ret)
			return ret;
		break;
	case I3C_BUS_MODE_MIXED_LIMITED:
	case I3C_BUS_MODE_MIXED_SLOW:
		writel_relaxed(MODULE_CONF_MIX_SLOW_I3C, master->regs + MODULE_CONF);
		ret = rockchip_i2c_clk_cfg(master);
		if (ret)
			return ret;
		break;
	case I3C_BUS_MODE_PURE:
		writel_relaxed(MODULE_CONF_PURE_I3C, master->regs + MODULE_CONF);
		/* FM Start will be used, so still need to configure i2c clk cfg. */
		ret = rockchip_i2c_clk_cfg(master);
		if (ret)
			return ret;
		ret = rockchip_i3c_clk_cfg(master);
		if (ret)
			return ret;
		break;
	default:
		writel_relaxed(MODULE_CONF_PURE_I2C, master->regs + MODULE_CONF);
		ret = rockchip_i2c_clk_cfg(master);
		if (ret)
			return ret;
		break;
	}

	if (!master->suspended) {
		/* Get an address for the master. */
		ret = i3c_master_get_free_addr(m, 0);
		if (ret < 0)
			return ret;

		memset(&info, 0, sizeof(info));
		info.dyn_addr = ret;

		ret = i3c_master_set_info(&master->base, &info);
		if (ret)
			return ret;
	}

	/* Give max timing for controller holding scl by default */
	writel_relaxed(0xffffffff, master->regs + TOUT_CONF_I3C);
	writel_relaxed(0xffffffff, master->regs + WAIT_THLD_I2C);

	/* Workaround to Give 4 clock cycles delay before ack and T bit for 12.5M */
	writel_relaxed(CLKSTALL_ENABLE | CLKSTALL_BEFORE_ACK_ENABLE |
		       CLKSTALL_BEFORE_READ_ENABLE | CLKSTALL_NUM,
		       master->regs + CLKSTALL_CONF_I3C);

	rockchip_i3c_master_enable(master);

	return 0;
}

static const struct i3c_master_controller_ops rockchip_i3c_master_ops = {
	.bus_init = rockchip_i3c_master_bus_init,
	.bus_cleanup = rockchip_i3c_master_bus_cleanup,
	.attach_i3c_dev = rockchip_i3c_master_attach_i3c_dev,
	.reattach_i3c_dev = rockchip_i3c_master_reattach_i3c_dev,
	.detach_i3c_dev = rockchip_i3c_master_detach_i3c_dev,
	.do_daa = rockchip_i3c_master_do_daa,
	.supports_ccc_cmd = rockchip_i3c_master_supports_ccc_cmd,
	.send_ccc_cmd = rockchip_i3c_master_send_ccc_cmd,
	.priv_xfers = rockchip_i3c_master_priv_xfers,
	.attach_i2c_dev = rockchip_i3c_master_attach_i2c_dev,
	.detach_i2c_dev = rockchip_i3c_master_detach_i2c_dev,
	.i2c_xfers = rockchip_i3c_master_i2c_xfers,
	.request_ibi = rockchip_i3c_master_request_ibi,
	.free_ibi = rockchip_i3c_master_free_ibi,
	.recycle_ibi_slot = rockchip_i3c_master_recycle_ibi_slot,
	.enable_ibi = rockchip_i3c_master_enable_ibi,
	.disable_ibi = rockchip_i3c_master_disable_ibi,
};

static void rockchip_i3c_master_hj(struct work_struct *work)
{
	struct rockchip_i3c_master *master =
		container_of(work, struct rockchip_i3c_master, hj_work);

	i3c_master_do_daa(&master->base);
}

static int rockchip_i3c_probe(struct platform_device *pdev)
{
	struct rockchip_i3c_master *i3c;
	struct resource *res;
	int ret, irq;

	i3c = devm_kzalloc(&pdev->dev, sizeof(*i3c), GFP_KERNEL);
	if (!i3c)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	i3c->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(i3c->regs))
		return PTR_ERR(i3c->regs);

	irq = platform_get_irq(pdev, 0);
	if (i3c->irq < 0)
		return i3c->irq;

	i3c->hclk = devm_clk_get(&pdev->dev, "hclk");
	if (IS_ERR(i3c->hclk))
		return PTR_ERR(i3c->hclk);

	i3c->clk = devm_clk_get(&pdev->dev, "i3c");
	if (IS_ERR(i3c->clk))
		return PTR_ERR(i3c->clk);

	ret = clk_prepare_enable(i3c->hclk);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't prepare i3c hclk: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(i3c->clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't prepare i3c bus clock: %d\n", ret);
		goto err_hclk;
	}

	i3c->dma_tx = dma_request_chan(&pdev->dev, "tx");
	if (IS_ERR(i3c->dma_tx)) {
		/* Check tx to see if we need defer probing driver */
		if (PTR_ERR(i3c->dma_tx) == -EPROBE_DEFER) {
			ret = -EPROBE_DEFER;
			goto err_clk;
		}
		dev_warn(&pdev->dev, "Failed to request TX DMA channel\n");
		i3c->dma_tx = NULL;
	}

	i3c->dma_rx = dma_request_chan(&pdev->dev, "rx");
	if (IS_ERR(i3c->dma_rx)) {
		if (PTR_ERR(i3c->dma_rx) == -EPROBE_DEFER) {
			ret = -EPROBE_DEFER;
			goto err_free_dma_tx;
		}
		dev_warn(&pdev->dev, "Failed to request RX DMA channel\n");
		i3c->dma_rx = NULL;
	}

	if (i3c->dma_tx)
		i3c->dma_addr_tx = res->start + TXDATA;
	if (i3c->dma_rx)
		i3c->dma_addr_rx = res->start + RXDATA;

	i3c->reset = devm_reset_control_get(&pdev->dev, "i3c");
	i3c->reset_ahb = devm_reset_control_get(&pdev->dev, "ahb");
	rockchip_i3c_master_reset_controller(i3c);

	spin_lock_init(&i3c->xferqueue.lock);
	INIT_LIST_HEAD(&i3c->xferqueue.list);

	/* Information regarding the FIFOs depth */
	i3c->caps.cmdfifodepth = CMDFIFO_DEPTH;
	i3c->caps.datafifodepth = DATAFIFO_DEPTH;
	i3c->caps.ibififodepth = IBIFIFO_DEPTH;

	/* Device ID0 is reserved to describe this master. */
	i3c->maxdevs = ROCKCHIP_I3C_MAX_DEVS;
	i3c->free_pos = GENMASK(ROCKCHIP_I3C_MAX_DEVS, 1);

	i3c->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, i3c);

	INIT_WORK(&i3c->hj_work, rockchip_i3c_master_hj);
	spin_lock_init(&i3c->ibi.lock);

	i3c->ibi.num_slots = ROCKCHIP_I3C_MAX_DEVS;
	i3c->ibi.slots = devm_kcalloc(&pdev->dev, i3c->ibi.num_slots,
				      sizeof(*i3c->ibi.slots),
				      GFP_KERNEL);
	if (!i3c->ibi.slots) {
		ret = -ENOMEM;
		goto err_free_dma_rx;
	}

	ret = devm_request_irq(&pdev->dev, irq,
			       rockchip_i3c_master_interrupt, 0,
			       dev_name(&pdev->dev), i3c);
	if (ret)
		goto err_free_dma_rx;

	i3c->aligned_cache = kmem_cache_create("rockchip-i3c", CMD_FIFO_PL_LEN_MAX, 0,
					       SLAB_CACHE_DMA | SLAB_HWCACHE_ALIGN | SLAB_CACHE_DMA32,
					       NULL);
	if (!i3c->aligned_cache) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "unable to create rockchip i3c aligned cache\n");
		goto err_free_dma_rx;
	}

	ret = i3c_master_register(&i3c->base, &pdev->dev,
				  &rockchip_i3c_master_ops, false);
	if (ret)
		goto err_destroy_cache;

	return 0;

err_destroy_cache:
	kmem_cache_destroy(i3c->aligned_cache);

err_free_dma_rx:
	if (i3c->dma_rx)
		dma_release_channel(i3c->dma_rx);
err_free_dma_tx:
	if (i3c->dma_tx)
		dma_release_channel(i3c->dma_tx);
err_clk:
	clk_disable_unprepare(i3c->clk);
err_hclk:
	clk_disable_unprepare(i3c->hclk);
	return ret;
}

static int rockchip_i3c_remove(struct platform_device *pdev)
{
	struct rockchip_i3c_master *i3c = dev_get_drvdata(&pdev->dev);

	if (i3c->dma_rx)
		dma_release_channel(i3c->dma_rx);
	if (i3c->dma_tx)
		dma_release_channel(i3c->dma_tx);

	kmem_cache_destroy(i3c->aligned_cache);
	i3c_master_unregister(&i3c->base);
	clk_disable_unprepare(i3c->clk);
	clk_disable_unprepare(i3c->hclk);

	return 0;
}

static __maybe_unused int rockchip_i3c_suspend_noirq(struct device *dev)
{
	struct rockchip_i3c_master *i3c = dev_get_drvdata(dev);

	i3c->suspended = true;
	rockchip_i3c_master_bus_cleanup(&i3c->base);
	clk_disable_unprepare(i3c->clk);
	clk_disable_unprepare(i3c->hclk);

	return 0;
}

static __maybe_unused int rockchip_i3c_resume_noirq(struct device *dev)
{
	struct rockchip_i3c_master *i3c = dev_get_drvdata(dev);

	clk_prepare_enable(i3c->clk);
	clk_prepare_enable(i3c->hclk);
	rockchip_i3c_master_bus_init(&i3c->base);
	i3c->suspended = false;

	return 0;
}

static const struct dev_pm_ops rockchip_i3c_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(rockchip_i3c_suspend_noirq,
				      rockchip_i3c_resume_noirq)
};

static const struct of_device_id rockchip_i3c_of_match[] = {
	{ .compatible = "rockchip,i3c-master" },
	{ }
};
MODULE_DEVICE_TABLE(of, rockchip_i3c_of_match);

static struct platform_driver rockchip_i3c_driver = {
	.probe = rockchip_i3c_probe,
	.remove = rockchip_i3c_remove,
	.driver = {
		.name = "rockchip-i3c",
		.of_match_table = rockchip_i3c_of_match,
		.pm = &rockchip_i3c_pm_ops,
	},
};

module_platform_driver(rockchip_i3c_driver);

MODULE_AUTHOR("David Wu <david.wu@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip I3C Master Controller Driver");
MODULE_LICENSE("GPL");
