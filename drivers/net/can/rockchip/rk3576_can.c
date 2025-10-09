// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 * Rk3576 CAN driver
 */

#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/pinctrl/consumer.h>
#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/can/dev.h>
#include <linux/can/error.h>
#include <linux/reset.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>

/* registers definition */
enum rk3576_can_reg {
	CAN_MODE = 0x00,
	CAN_CMD = 0x04,
	CAN_STATE = 0x08,
	CAN_INT = 0x0c,
	CAN_INT_MASK = 0x10,
	CAN_NBTP = 0x100,
	CAN_DBTP = 0x104,
	CAN_TDCR = 0x108,
	CAN_DMA_CTRL = 0x11c,
	CAN_TXFIC = 0x200,
	CAN_TXID = 0x204,
	CAN_TXDAT0 = 0x208,
	CAN_TXDAT1 = 0x20c,
	CAN_TXDAT2 = 0x210,
	CAN_TXDAT3 = 0x214,
	CAN_TXDAT4 = 0x218,
	CAN_TXDAT5 = 0x21c,
	CAN_TXDAT6 = 0x220,
	CAN_TXDAT7 = 0x224,
	CAN_TXDAT8 = 0x228,
	CAN_TXDAT9 = 0x22c,
	CAN_TXDAT10 = 0x230,
	CAN_TXDAT11 = 0x234,
	CAN_TXDAT12 = 0x238,
	CAN_TXDAT13 = 0x23c,
	CAN_TXDAT14 = 0x240,
	CAN_TXDAT15 = 0x244,
	CAN_BUF1_TXFIC = 0x280,
	CAN_BUF1_TXID = 0x284,
	CAN_BUF1_TXDAT0 = 0x288,
	CAN_BUF1_TXDAT1 = 0x28c,
	CAN_BUF1_TXDAT2 = 0x290,
	CAN_BUF1_TXDAT3 = 0x294,
	CAN_BUF1_TXDAT4 = 0x298,
	CAN_BUF1_TXDAT5 = 0x29c,
	CAN_BUF1_TXDAT6 = 0x2a0,
	CAN_BUF1_TXDAT7 = 0x2a4,
	CAN_BUF1_TXDAT8 = 0x2a8,
	CAN_BUF1_TXDAT9 = 0x2ac,
	CAN_BUF1_TXDAT10 = 0x2b0,
	CAN_BUF1_TXDAT11 = 0x2b4,
	CAN_BUF1_TXDAT12 = 0x2b8,
	CAN_BUF1_TXDAT13 = 0x2bc,
	CAN_BUF1_TXDAT14 = 0x2c0,
	CAN_BUF1_TXDAT15 = 0x2c4,
	CAN_RXFIC = 0x300,
	CAN_RXID = 0x304,
	CAN_RXTS = 0x308,
	CAN_RXDAT0 = 0x30c,
	CAN_RXDAT1 = 0x310,
	CAN_RXDAT2 = 0x314,
	CAN_RXDAT3 = 0x318,
	CAN_RXDAT4 = 0x31c,
	CAN_RXDAT5 = 0x320,
	CAN_RXDAT6 = 0x324,
	CAN_RXDAT7 = 0x328,
	CAN_RXDAT8 = 0x32c,
	CAN_RXDAT9 = 0x330,
	CAN_RXDAT10 = 0x334,
	CAN_RXDAT11 = 0x338,
	CAN_RXDAT12 = 0x33c,
	CAN_RXDAT13 = 0x340,
	CAN_RXDAT14 = 0x344,
	CAN_RXDAT15 = 0x348,
	CAN_RXFRD = 0x400,
	CAN_STR_CTL = 0x600,
	CAN_STR_STATE = 0x604,
	CAN_STR_TIMEOUT = 0x608,
	CAN_STR_WTM = 0x60c,
	CAN_EXTM_START_ADDR = 0x610,
	CAN_EXTM_SIZE = 0x614,
	CAN_EXTM_WADDR = 0x618,
	CAN_EXTM_RADDR = 0x61c,
	CAN_EXTM_AHB_TXTHR = 0x620,
	CAN_EXTM_LEFT_CNT = 0x624,
	CAN_ATF0 = 0x700,
	CAN_ATF1 = 0x704,
	CAN_ATF2 = 0x708,
	CAN_ATF3 = 0x70c,
	CAN_ATF4 = 0x710,
	CAN_ATFM0 = 0x714,
	CAN_ATFM1 = 0x718,
	CAN_ATFM2 = 0x71c,
	CAN_ATFM3 = 0x720,
	CAN_ATFM4 = 0x724,
	CAN_ATF_DLC = 0x728,
	CAN_ATF_CTL = 0x72c,
	CAN_SPACE_CTRL = 0x800,
	CAN_AUTO_RETX_CFG = 0x808,
	CAN_AUTO_RETX_STATE0 = 0x80c,
	CAN_AUTO_RETX_STATE1 = 0x810,
	CAN_OLF_CFG = 0x814,
	CAN_RXINT_CTRL = 0x818,
	CAN_RXINT_TIMEOUT = 0x81c,
	CAN_OTHER_CFG = 0x820,
	CAN_WAVE_FILTER_CFG = 0x824,
	CAN_RBC_CFG = 0x828,
	CAN_TXCRC_CFG = 0x82c,
	CAN_BUSOFFRCY_CFG = 0x830,
	CAN_BUSOFF_RCY_THR = 0x834,
	CAN_ERROR_CODE = 0x900,
	CAN_ERROR_MASK = 0x904,
	CAN_RXERRORCNT = 0x910,
	CAN_TXERRORCNT = 0x914,
	CAN_RX_RXSRAM_RDATA = 0xc00,
	CAN_RTL_VERSION = 0xf0c,
};

enum {
	ROCKCHIP_RK3576_CAN = 0,
	ROCKCHIP_RV1126B_CAN,
};

#define DATE_LENGTH_12_BYTE	(0x9)
#define DATE_LENGTH_16_BYTE	(0xa)
#define DATE_LENGTH_20_BYTE	(0xb)
#define DATE_LENGTH_24_BYTE	(0xc)
#define DATE_LENGTH_32_BYTE	(0xd)
#define DATE_LENGTH_48_BYTE	(0xe)
#define DATE_LENGTH_64_BYTE	(0xf)

#define CAN_TX0_REQ		BIT(0)
#define CAN_TX1_REQ		BIT(1)
#define CAN_TX_REQ_FULL		((CAN_TX0_REQ) | (CAN_TX1_REQ))

#define MODE_PASS_ERR		BIT(10)
#define MODE_DIS_PEE		BIT(9)
#define MODE_RETT		BIT(8)
#define MODE_AUTO_BUS_ON	BIT(7)
#define MODE_COVER		BIT(6)
#define MODE_RXSTX		BIT(5)
#define MODE_LBACK		BIT(4)
#define MODE_SILENT		BIT(3)
#define MODE_SELF_TEST		BIT(2)
#define MODE_SLEEP		BIT(1)
#define RESET_MODE		0
#define WORK_MODE		BIT(0)

#define RETX_TIME_LIMIT_CNT_MAX	0xffff
#define RETX_TIME_LIMIT_SHIFT	3
#define RETX_LIMIT_EN		BIT(1)
#define AUTO_RETX_EN		BIT(0)

#define ERR_WARNING_STATE	BIT(3)
#define BUS_OFF_STATE		BIT(4)

#define RX_FINISH_INT		BIT(0)
#define TX_FINISH_INT		BIT(1)
#define ERR_WARN_INT		BIT(2)
#define RX_BUF_OV_INT		BIT(3)
#define PASSIVE_ERR_INT		BIT(4)
#define TX_LOSTARB_INT		BIT(5)
#define BUS_ERR_INT		BIT(6)
#define RX_STR_FULL_INT		BIT(7)
#define RX_STR_OV_INT		BIT(8)
#define BUS_OFF_INT		BIT(9)
#define BUS_OFF_RECOVERY_INT	BIT(10)
#define WAKEUP_INT		BIT(11)
#define AUTO_RETX_FAIL_INT	BIT(12)
#define MFI_INT			BIT(13)
#define MFI_TIMEOUT		BIT(14)
#define RX_STR_TIMEOUT_INT	BIT(15)
#define BUSINT_INT		BIT(16)
#define ISM_WTM_INT		BIT(17)
#define ESM_WTM_INT		BIT(18)
#define BUSOFF_RCY_INT		BIT(19)

#define INT_ENABLE		BIT(0)

#define ERR_PHASE		BIT(28)
#define ARB_PHASE		0
#define DATA_PHASE		1
#define ERR_TYPE_MASK		GENMASK(27, 25)
#define ERR_TYPE_SHIFT		25
#define BIT_ERROR		0
#define BIT_STUFF_ERROR		1
#define FORM_ERROR		2
#define ACK_ERROR		3
#define CRC_ERROR		4
#define TRANSMIT_ACK_EOF	BIT(24)
#define TRANSMIT_CRC		BIT(23)
#define TRANSMIT_STUFF_COUNT	BIT(22)
#define TRANSMIT_DATA		BIT(21)
#define TRANSMIT_SOF_DLC	BIT(20)
#define TRANSMIT_IDLE		BIT(19)
#define RECEIVE_ERROR		BIT(18)
#define RECEIVE_OVERLOAD	BIT(17)
#define RECEIVE_SPACE		BIT(16)
#define RECEIVE_EOF		BIT(15)
#define RECEIVE_ACK_LIM		BIT(14)
#define RECEIVE_ACK_SLOT	BIT(13)
#define RECEIVE_CRC_LIM		BIT(12)
#define RECEIVE_CRC		BIT(11)
#define RECEIVE_STUFF_COUNT	BIT(10)
#define RECEIVE_DATA		BIT(9)
#define RECEIVE_DLC		BIT(8)
#define RECEIVE_ID2_RTR		BIT(4)
#define RECEIVE_SOF_IDE		BIT(3)
#define RECEIVE_BUS_IDLE	BIT(2)
#define RECEIVE_BUS_INT		BIT(1)
#define RECEIVE_STOP		BIT(0)

/* Nominal Bit Timing & Prescaler Register (NBTP) */
#define NBTP_MODE_3_SAMPLES	BIT(31)
#define NBTP_NSJW_SHIFT		24
#define NBTP_NSJW_MASK		(0x7f << NBTP_NSJW_SHIFT)
#define NBTP_NBRP_SHIFT		16
#define NBTP_NBRP_MASK		(0xff << NBTP_NBRP_SHIFT)
#define NBTP_NTSEG2_SHIFT	8
#define NBTP_NTSEG2_MASK	(0x7f << NBTP_NTSEG2_SHIFT)
#define NBTP_NTSEG1_SHIFT	0
#define NBTP_NTSEG1_MASK	(0xff << NBTP_NTSEG1_SHIFT)

/* Data Bit Timing & Prescaler Register (DBTP) */
#define DBTP_MODE_3_SAMPLES	BIT(21)
#define DBTP_DSJW_SHIFT		17
#define DBTP_DSJW_MASK		(0xf << DBTP_DSJW_SHIFT)
#define DBTP_DBRP_SHIFT		9
#define DBTP_DBRP_MASK		(0xff << DBTP_DBRP_SHIFT)
#define DBTP_DTSEG2_SHIFT	5
#define DBTP_DTSEG2_MASK	(0xf << DBTP_DTSEG2_SHIFT)
#define DBTP_DTSEG1_SHIFT	0
#define DBTP_DTSEG1_MASK	(0x1f << DBTP_DTSEG1_SHIFT)

/* Transmitter Delay Compensation Register (TDCR) */
#define TDCR_TDCO_SHIFT		1
#define TDCR_TDCO_MASK		(0x3f << TDCR_TDCO_SHIFT)
#define TDCR_TDC_ENABLE		BIT(0)

#define RX_DMA_ENABLE		BIT(9)

#define TX_FORMAT_SHIFT		7
#define TX_FORMAT_MASK		(0x1 << TX_FORMAT_SHIFT)
#define TX_RTR_SHIFT		6
#define TX_RTR_MASK		(0x1 << TX_RTR_SHIFT)
#define TX_DLC_SHIFT		0
#define TX_DLC_MASK		(0xF << TX_DLC_SHIFT)

#define RX_FORMAT_SHIFT		23
#define RX_FORMAT_MASK		(0x1 << RX_FORMAT_SHIFT)
#define RX_RTR_SHIFT		22
#define RX_RTR_MASK		(0x1 << RX_RTR_SHIFT)
#define RX_DLC_SHIFT		24
#define RX_DLC_MASK		(0xF << RX_DLC_SHIFT)
#define RX_ISM_LEN_SHIFT	8
#define RX_ISM_LEN_MASK		(0xF << RX_ISM_LEN_SHIFT)

#define BUFFER_MODE_ENABLE	BIT(0)
#define EXT_STORAGE_MODE	BIT(1)
#define ISM_SEL_SHIFT		2
#define ISM_SEL_MASK		(0x3 << ISM_SEL_SHIFT)
#define RX_STORAGE_RESET	BIT(4)
#define ESM_SEL_SHIFT		6
#define ESM_SEL_MASK		(0x3 << ESM_SEL_SHIFT)
#define STORAGE_TIMEOUT_MODE	BIT(8)

#define INTM_CNT_SHIFT		17
#define INTM_CNT_MASK		(0x1ff << INTM_CNT_SHIFT)
#define INTM_LEFT_CNT_SHIFT	8
#define INTM_LEFT_CNT_MASK	(0x1ff << INTM_LEFT_CNT_SHIFT)
#define EXTM_FULL		BIT(3)
#define EXTM_EMPTY		BIT(2)
#define INTM_FULL		BIT(1)
#define INTM_EMPTY		BIT(0)

#define EXTTM_LEFT_CNT_SHIFT	0
#define EXTTM_LEFT_CNT_MASK	(0x3fffff << EXTTM_LEFT_CNT_SHIFT)

#define ISM_WATERMASK_CAN	0x6c /* word */
#define ESM_WATERMASK		(0x50 << 8) /* word */

#define BUSOFF_RCY_MODE_EN	BIT(8)
#define BUSOFF_RCY_TIME_CLR	BIT(9)
#define BUSOFF_RCY_CNT_FAST	4
#define BUSOFF_RCY_CNT_SLOW	5
#define BUSOFF_RCY_TIME_FAST	0x3d0900 /* 40ms : cnt * (1 / can_clk) */
#define BUSOFF_RCY_TIME_SLOW	0x1312d00 /* 200ms : cnt * (1 / can_clk) */

#define ATF_MASK		BIT(31)
#define ATF_RTR_EN		BIT(30)
#define ATF_RTR			BIT(29)

#define ATF_DLC_MODE		BIT(5)
#define ATF_DLC_EN		BIT(4)
#define ATF_DLC_SHIFT		0
#define ATF_DLC_MASK		(0xf << ATF_DLC_SHIFT)

#define ATF_DIS(n)		BIT(n)

#define ACK_ERROR_MASK		BIT(4)
#define FORM_ERROR_MASK		BIT(3)
#define CRC_ERROR_MASK		BIT(2)
#define STUFF_ERROR_MASK	BIT(1)
#define BIT_ERROR_MASK		BIT(0)

#define SRAM_MAX_DEPTH		256 /* word */
#define EXT_MEM_SIZE		0x2000 /* 8KByte */

#define CAN_FILTER_MASK		0x1fffffff

#define CAN_FIFO_CNT_MASK	0xff

#define CANBUSOFF_RCY_SLOW	200 /* ms */
#define CANBUSOFF_RCY_FAST	30 /* ms */

#define DRV_NAME		"rk3576_can"

enum rk3576_can_atf_mode {
	CAN_ATF_MASK_MODE = 0,
	CAN_ATF_LIST_MODE,
};

enum rk3576_can_storage_mode {
	CAN_DATA_FLEXIBLE = 0,
	CAN_DATA_CAN_DATE_4_FIXED,
	CAN_DATA_CAN_DATA_18_FIXED,
};

/* rk3576_can private data structure */

struct rk3576_can {
	struct can_priv can;
	struct device *dev;
	struct napi_struct napi;
	struct clk_bulk_data *clks;
	int num_clks;
	struct reset_control *reset;
	void __iomem *base;
	u32 irqstatus;
	unsigned long mode;
	int rx_fifo_shift;
	u32 rx_fifo_mask;
	int rx_fifo_depth;
	int rx_max_data;
	unsigned int auto_retx_cnt;
	bool use_dma;
	u32 dma_size;
	u32 dma_thr;
	int quota;
	struct dma_chan *rxchan;
	u32 *rxbuf;
	dma_addr_t rx_dma_src_addr;
	dma_addr_t rx_dma_dst_addr;
};

static inline u32 rk3576_can_read(const struct rk3576_can *priv,
				  enum rk3576_can_reg reg)
{
	return readl(priv->base + reg);
}

static inline void rk3576_can_write(const struct rk3576_can *priv,
				    enum rk3576_can_reg reg, u32 val)
{
	writel(val, priv->base + reg);
}

static const struct can_bittiming_const rk3576_can_bittiming_const = {
	.name = DRV_NAME,
	.tseg1_min = 1,
	.tseg1_max = 128,
	.tseg2_min = 1,
	.tseg2_max = 128,
	.sjw_max = 128,
	.brp_min = 1,
	.brp_max = 256,
	.brp_inc = 2,
};

static const struct can_bittiming_const rk3576_can_data_bittiming_const = {
	.name = DRV_NAME,
	.tseg1_min = 1,
	.tseg1_max = 32,
	.tseg2_min = 1,
	.tseg2_max = 16,
	.sjw_max = 16,
	.brp_min = 1,
	.brp_max = 256,
	.brp_inc = 2,
};

static int set_reset_mode(struct net_device *ndev)
{
	struct rk3576_can *rcan = netdev_priv(ndev);

	reset_control_assert(rcan->reset);
	udelay(2);
	reset_control_deassert(rcan->reset);

	rk3576_can_write(rcan, CAN_MODE, 0);

	netdev_dbg(ndev, "%s MODE=0x%08x\n", __func__,
		   rk3576_can_read(rcan, CAN_MODE));

	return 0;
}

static int set_normal_mode(struct net_device *ndev)
{
	struct rk3576_can *rcan = netdev_priv(ndev);
	u32 val;

	val = rk3576_can_read(rcan, CAN_MODE);
	val |= WORK_MODE;
	rk3576_can_write(rcan, CAN_MODE, val);

	netdev_dbg(ndev, "%s MODE=0x%08x\n", __func__,
		   rk3576_can_read(rcan, CAN_MODE));
	return 0;
}

/* bittiming is called in reset_mode only */
static int rk3576_can_set_bittiming(struct net_device *ndev)
{
	struct rk3576_can *rcan = netdev_priv(ndev);
	const struct can_bittiming *bt = &rcan->can.bittiming;
	u16 brp, sjw, tseg1, tseg2;
	u32 reg_btp;

	brp = (bt->brp >> 1) - 1;
	sjw = bt->sjw - 1;
	tseg1 = bt->prop_seg + bt->phase_seg1 - 1;
	tseg2 = bt->phase_seg2 - 1;
	reg_btp = (brp << NBTP_NBRP_SHIFT) | (sjw << NBTP_NSJW_SHIFT) |
		  (tseg1 << NBTP_NTSEG1_SHIFT) |
		  (tseg2 << NBTP_NTSEG2_SHIFT);

	if (rcan->can.ctrlmode & CAN_CTRLMODE_3_SAMPLES)
		reg_btp |= NBTP_MODE_3_SAMPLES;

	rk3576_can_write(rcan, CAN_NBTP, reg_btp);

	netdev_dbg(ndev, "%s NBTP=0x%08x, DBTP=0x%08x, TDCR=0x%08x\n", __func__,
		   rk3576_can_read(rcan, CAN_NBTP),
		   rk3576_can_read(rcan, CAN_DBTP),
		   rk3576_can_read(rcan, CAN_TDCR));
	return 0;
}

static int rk3576_can_get_berr_counter(const struct net_device *ndev,
				       struct can_berr_counter *bec)
{
	struct rk3576_can *rcan = netdev_priv(ndev);
	int err;

	err = pm_runtime_get_sync(rcan->dev);
	if (err < 0) {
		netdev_err(ndev, "%s: pm_runtime_get failed(%d)\n",
			   __func__, err);
		return err;
	}

	bec->rxerr = rk3576_can_read(rcan, CAN_RXERRORCNT);
	bec->txerr = rk3576_can_read(rcan, CAN_TXERRORCNT);

	pm_runtime_put(rcan->dev);

	netdev_dbg(ndev, "%s RX_ERR_CNT=0x%08x, TX_ERR_CNT=0x%08x\n", __func__,
		   rk3576_can_read(rcan, CAN_RXERRORCNT),
		   rk3576_can_read(rcan, CAN_TXERRORCNT));

	return 0;
}

static int rk3576_can_atf_config(const struct net_device *ndev, int mode)
{
	struct rk3576_can *rcan = netdev_priv(ndev);
	u32 id[10] = {0};
	u32 dlc = 0, dlc_over = 0;

	switch (mode) {
	case CAN_ATF_MASK_MODE:
		rk3576_can_write(rcan, CAN_ATF0, id[0]);
		rk3576_can_write(rcan, CAN_ATF1, id[1]);
		rk3576_can_write(rcan, CAN_ATF2, id[2]);
		rk3576_can_write(rcan, CAN_ATF3, id[3]);
		rk3576_can_write(rcan, CAN_ATF4, id[4]);
		rk3576_can_write(rcan, CAN_ATFM0, 0x7fff);
		rk3576_can_write(rcan, CAN_ATFM1, 0x7fff);
		rk3576_can_write(rcan, CAN_ATFM2, 0x7fff);
		rk3576_can_write(rcan, CAN_ATFM3, 0x7fff);
		rk3576_can_write(rcan, CAN_ATFM4, 0x7fff);
		break;
	case CAN_ATF_LIST_MODE:
		rk3576_can_write(rcan, CAN_ATF0, id[0]);
		rk3576_can_write(rcan, CAN_ATF1, id[1]);
		rk3576_can_write(rcan, CAN_ATF2, id[2]);
		rk3576_can_write(rcan, CAN_ATF3, id[3]);
		rk3576_can_write(rcan, CAN_ATF4, id[4]);
		rk3576_can_write(rcan, CAN_ATFM0, id[5] | (1 << 31));
		rk3576_can_write(rcan, CAN_ATFM1, id[6] | (1 << 31));
		rk3576_can_write(rcan, CAN_ATFM2, id[7] | (1 << 31));
		rk3576_can_write(rcan, CAN_ATFM3, id[8] | (1 << 31));
		rk3576_can_write(rcan, CAN_ATFM4, id[9] | (1 << 31));
		break;
	default:
		rk3576_can_write(rcan, CAN_ATF_CTL, 0xffff);
		return -EINVAL;
	}

	if (dlc) {
		if (dlc_over)
			rk3576_can_write(rcan, CAN_ATF_DLC, dlc | (1 << 4));
		else
			rk3576_can_write(rcan, CAN_ATF_DLC, dlc | (1 << 4) | (1 << 5));
	}
	rk3576_can_write(rcan, CAN_ATF_CTL, 0);

	return 0;
}

static int rk3576_can_start(struct net_device *ndev)
{
	struct rk3576_can *rcan = netdev_priv(ndev);
	u32 val, ism = 0, water = 0;

	/* we need to enter the reset mode */
	set_reset_mode(ndev);

	rk3576_can_write(rcan, CAN_INT_MASK, INT_ENABLE);
	rk3576_can_atf_config(ndev, CAN_ATF_MASK_MODE);

	/* set mode */
	val = rk3576_can_read(rcan, CAN_MODE);

	if (rcan->rx_max_data > 4) {
		ism = CAN_DATA_CAN_DATA_18_FIXED;
		water = ISM_WATERMASK_CAN;
	} else {
		ism = CAN_DATA_CAN_DATE_4_FIXED;
		water = ISM_WATERMASK_CAN;
	}

	/* internal sram mode */
	rk3576_can_write(rcan, CAN_STR_CTL,
			 (ism << ISM_SEL_SHIFT) | STORAGE_TIMEOUT_MODE);
	rk3576_can_write(rcan, CAN_STR_WTM, water);

	/* Loopback Mode */
	if (rcan->can.ctrlmode & CAN_CTRLMODE_LOOPBACK) {
		val |= MODE_LBACK;
		rk3576_can_write(rcan, CAN_ERROR_MASK, ACK_ERROR_MASK);
	} else if (rcan->can.ctrlmode & CAN_CTRLMODE_LISTENONLY) {
		val |= MODE_SILENT;
		rk3576_can_write(rcan, CAN_ERROR_MASK, ACK_ERROR_MASK);
	} else {
		rk3576_can_write(rcan, CAN_ERROR_MASK, 0);
	}

	if (rcan->auto_retx_cnt)
		rk3576_can_write(rcan, CAN_AUTO_RETX_CFG,
				 AUTO_RETX_EN | RETX_LIMIT_EN |
				 (rcan->auto_retx_cnt << RETX_TIME_LIMIT_SHIFT));
	else
		rk3576_can_write(rcan, CAN_AUTO_RETX_CFG, AUTO_RETX_EN);

	rk3576_can_write(rcan, CAN_MODE, val);
	if (rcan->use_dma)
		rk3576_can_write(rcan, CAN_DMA_CTRL, RX_DMA_ENABLE | rcan->dma_thr);

	rk3576_can_write(rcan, CAN_BUSOFFRCY_CFG, BUSOFF_RCY_MODE_EN | BUSOFF_RCY_CNT_FAST);
	rk3576_can_write(rcan, CAN_BUSOFF_RCY_THR, BUSOFF_RCY_TIME_FAST);

	rk3576_can_set_bittiming(ndev);

	set_normal_mode(ndev);

	rcan->can.state = CAN_STATE_ERROR_ACTIVE;

	netdev_dbg(ndev, "%s MODE=0x%08x, INT_MASK=0x%08x\n", __func__,
		   rk3576_can_read(rcan, CAN_MODE),
		   rk3576_can_read(rcan, CAN_INT_MASK));

	return 0;
}

static int rk3576_can_stop(struct net_device *ndev)
{
	struct rk3576_can *rcan = netdev_priv(ndev);

	rcan->can.state = CAN_STATE_STOPPED;
	/* we need to enter reset mode */
	set_reset_mode(ndev);

	/* disable all interrupts */
	rk3576_can_write(rcan, CAN_INT_MASK, 0xffffffff);

	netdev_dbg(ndev, "%s MODE=0x%08x, INT_MASK=0x%08x\n", __func__,
		   rk3576_can_read(rcan, CAN_MODE),
		   rk3576_can_read(rcan, CAN_INT_MASK));
	return 0;
}

static int rk3576_can_set_mode(struct net_device *ndev,
			       enum can_mode mode)
{
	int err;

	switch (mode) {
	case CAN_MODE_START:
		err = rk3576_can_start(ndev);
		if (err) {
			netdev_err(ndev, "starting CAN controller failed!\n");
			return err;
		}
		if (netif_queue_stopped(ndev))
			netif_wake_queue(ndev);
		break;

	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

/* transmit a CAN message
 * message layout in the sk_buff should be like this:
 * xx xx xx xx ff ll 00 11 22 33 44 55 66 77
 * [ can_id ] [flags] [len] [can data (up to 8 bytes]
 */
static netdev_tx_t rk3576_can_start_xmit(struct sk_buff *skb,
					 struct net_device *ndev)
{
	struct rk3576_can *rcan = netdev_priv(ndev);
	struct can_frame *cf = (struct can_frame *)skb->data;
	u32 id, dlc;
	u32 cmd = CAN_TX0_REQ;
	u32 tx_fifo = CAN_TXFIC, tx_id = CAN_TXID, tx_data = CAN_TXDAT0;
	int i;

	if (can_dropped_invalid_skb(ndev, skb))
		return NETDEV_TX_OK;

	netif_stop_queue(ndev);

	if (rk3576_can_read(rcan, CAN_CMD) & CAN_TX0_REQ) {
		cmd = CAN_TX1_REQ;
		if (rcan->mode == ROCKCHIP_RV1126B_CAN) {
			tx_fifo = CAN_BUF1_TXFIC;
			tx_id = CAN_BUF1_TXID;
			tx_data = CAN_BUF1_TXDAT0;
		}
	}

	/* Watch carefully on the bit sequence */
	if (cf->can_id & CAN_EFF_FLAG) {
		/* Extended CAN ID format */
		id = cf->can_id & CAN_EFF_MASK;
		dlc = cf->can_dlc & TX_DLC_MASK;
		dlc |= TX_FORMAT_MASK;

		/* Extended frames remote TX request */
		if (cf->can_id & CAN_RTR_FLAG)
			dlc |= TX_RTR_MASK;
	} else {
		/* Standard CAN ID format */
		id = cf->can_id & CAN_SFF_MASK;
		dlc = cf->can_dlc & TX_DLC_MASK;

		/* Standard frames remote TX request */
		if (cf->can_id & CAN_RTR_FLAG)
			dlc |= TX_RTR_MASK;
	}

	rk3576_can_write(rcan, tx_id, id);
	rk3576_can_write(rcan, tx_fifo, dlc);

	for (i = 0; i < can_cc_dlc2len(cf->can_dlc & TX_DLC_MASK); i += 4)
		rk3576_can_write(rcan, tx_data + i,
				 *(u32 *)(cf->data + i));

	can_put_echo_skb(skb, ndev, 0, 0);

	rk3576_can_write(rcan, CAN_CMD, cmd);

	return NETDEV_TX_OK;
}

static int rk3576_can_rx(struct net_device *ndev, u32 addr)
{
	struct rk3576_can *rcan = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	struct can_frame *cf;
	struct sk_buff *skb;
	u32 id_rk3576_can, dlc;
	int i = 0;
	u32 __maybe_unused ts, ret;
	u32 data[16] = {0};

	if (rcan->use_dma) {
		dlc = readl(rcan->rxbuf + addr * rcan->rx_max_data);
		id_rk3576_can = readl(rcan->rxbuf + 1 + addr * rcan->rx_max_data);
		for (i = 0; i < (rcan->rx_max_data - 2); i++)
			data[i] = readl(rcan->rxbuf + 2 + i + addr * rcan->rx_max_data);
	} else {
		dlc = rk3576_can_read(rcan, addr);
		id_rk3576_can = rk3576_can_read(rcan, addr);
		for (i = 0; i < (rcan->rx_max_data - 2); i++)
			data[i] = rk3576_can_read(rcan, addr);
	}

	/* create zero'ed CAN frame buffer */
	skb = alloc_can_skb(ndev, (struct can_frame **)&cf);
	if (!skb) {
		stats->rx_dropped++;
		return 1;
	}

	/* Change CAN data length format to SocketCAN data format */
	cf->can_dlc = can_cc_dlc2len((dlc & RX_DLC_MASK) >> RX_DLC_SHIFT);

	/* Change CAN ID format to SocketCAN ID format */
	if (dlc & RX_FORMAT_MASK) {
		/* The received frame is an Extended format frame */
		cf->can_id = id_rk3576_can;
		cf->can_id |= CAN_EFF_FLAG;
		if (dlc & RX_RTR_MASK)
			cf->can_id |= CAN_RTR_FLAG;
	} else {
		/* The received frame is a standard format frame */
		cf->can_id = id_rk3576_can;
		if (dlc & RX_RTR_MASK)
			cf->can_id |= CAN_RTR_FLAG;
	}

	if (!(cf->can_id & CAN_RTR_FLAG)) {
		/* Change CAN data format to SocketCAN data format */
		for (i = 0; i < cf->can_dlc; i += 4)
			*(u32 *)(cf->data + i) = data[i / 4];
	}

	stats->rx_packets++;
	stats->rx_bytes += cf->can_dlc;
	netif_rx(skb);

	return 1;
}

/* rk3576_can_rx_poll - Poll routine for rx packets (NAPI)
 * @napi:	napi structure pointer
 * @quota:	Max number of rx packets to be processed.
 *
 * This is the poll routine for rx part.
 * It will process the packets maximux quota value.
 *
 * Return: number of packets received
 */
static int rk3576_can_rx_poll(struct napi_struct *napi, int quota)
{
	struct net_device *ndev = napi->dev;
	struct rk3576_can *rcan = netdev_priv(ndev);
	int work_done = 0, cnt = 0;

	if (rcan->use_dma) {
		while (work_done < rcan->quota)
			work_done += rk3576_can_rx(ndev, work_done);

		if (work_done <= rcan->rx_fifo_depth) {
			napi_complete_done(napi, work_done);
			rk3576_can_write(rcan, CAN_INT_MASK, INT_ENABLE);
		}
	} else {
		quota = (rk3576_can_read(rcan, CAN_STR_STATE) & rcan->rx_fifo_mask) >>
			rcan->rx_fifo_shift;
		quota = quota / rcan->rx_max_data;
		cnt = (rk3576_can_read(rcan, CAN_STR_STATE) & INTM_CNT_MASK) >> INTM_CNT_SHIFT;
		if (quota != cnt)
			quota = ((rk3576_can_read(rcan, CAN_STR_STATE) & rcan->rx_fifo_mask) >>
				rcan->rx_fifo_shift) / rcan->rx_max_data;

		while (work_done < quota)
			work_done += rk3576_can_rx(ndev, CAN_RXFRD);

		if (work_done <= rcan->rx_fifo_depth) {
			napi_complete_done(napi, work_done);
			rk3576_can_write(rcan, CAN_INT_MASK, INT_ENABLE);
		}
	}
	return work_done;
}

static void rk3576_can_rx_dma_callback(void *data)
{
	struct rk3576_can *rcan = data;

	napi_schedule(&rcan->napi);
}

static int rk3576_can_rx_dma(struct rk3576_can *rcan)
{
	struct dma_async_tx_descriptor *rxdesc = NULL;
	int quota = 0;

	quota = (rk3576_can_read(rcan, CAN_STR_STATE) & rcan->rx_fifo_mask) >>
		rcan->rx_fifo_shift;
	rcan->quota = DIV_ROUND_UP(quota, rcan->rx_max_data);
	if (rcan->quota == 0) {
		rk3576_can_write(rcan, CAN_INT_MASK, INT_ENABLE);
		return 1;
	}

	rxdesc = dmaengine_prep_slave_single(rcan->rxchan, rcan->rx_dma_dst_addr,
					     rcan->dma_size * rcan->quota, DMA_DEV_TO_MEM, 0);
	if (!rxdesc)
		return -EBUSY;

	rxdesc->callback = rk3576_can_rx_dma_callback;
	rxdesc->callback_param = rcan;

	dmaengine_submit(rxdesc);
	dma_async_issue_pending(rcan->rxchan);
	return 1;
}

static int rk3576_can_err(struct net_device *ndev, u32 isr)
{
	struct rk3576_can *rcan = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	struct can_frame *cf;
	struct sk_buff *skb;
	unsigned int rxerr, txerr;
	u32 sta_reg;

	skb = alloc_can_err_skb(ndev, &cf);

	rxerr = rk3576_can_read(rcan, CAN_RXERRORCNT);
	txerr = rk3576_can_read(rcan, CAN_TXERRORCNT);
	sta_reg = rk3576_can_read(rcan, CAN_STATE);

	if (skb) {
		cf->data[6] = txerr;
		cf->data[7] = rxerr;
	}

	if (isr & BUS_OFF_INT) {
		rcan->can.state = CAN_STATE_BUS_OFF;
		rcan->can.can_stats.bus_off++;
		cf->can_id |= CAN_ERR_BUSOFF;
	} else if (isr & PASSIVE_ERR_INT) {
		rcan->can.can_stats.error_passive++;
		rcan->can.state = CAN_STATE_ERROR_PASSIVE;
		/* error passive state */
		cf->can_id |= CAN_ERR_CRTL;
		cf->data[1] = (txerr > rxerr) ?
					CAN_ERR_CRTL_TX_WARNING :
					CAN_ERR_CRTL_RX_WARNING;
		cf->data[6] = txerr;
		cf->data[7] = rxerr;
	}
	if (sta_reg & ERR_WARNING_STATE) {
		rcan->can.can_stats.error_warning++;
		rcan->can.state = CAN_STATE_ERROR_WARNING;
		/* error warning state */
		if (likely(skb)) {
			cf->can_id |= CAN_ERR_CRTL;
			cf->data[1] = (txerr > rxerr) ?
				CAN_ERR_CRTL_TX_WARNING :
				CAN_ERR_CRTL_RX_WARNING;
			cf->data[6] = txerr;
			cf->data[7] = rxerr;
		}
	}

	if (isr & BUSOFF_RCY_INT) {
		rk3576_can_write(rcan, CAN_INT_MASK, 0xffff);
		rk3576_can_write(rcan, CAN_INT, isr);
		napi_schedule(&rcan->napi);
		rk3576_can_write(rcan, CAN_BUSOFFRCY_CFG, BUSOFF_RCY_TIME_CLR);
		rk3576_can_write(rcan, CAN_BUSOFF_RCY_THR, BUSOFF_RCY_TIME_SLOW);
		rk3576_can_write(rcan, CAN_BUSOFFRCY_CFG,
				 BUSOFF_RCY_MODE_EN | BUSOFF_RCY_CNT_SLOW);
		rk3576_can_write(rcan, CAN_INT_MASK, INT_ENABLE);
		netif_stop_queue(ndev);
		can_free_echo_skb(ndev, 0, NULL);
		netif_start_queue(ndev);
	}
	stats->rx_packets++;
	stats->rx_bytes += cf->can_dlc;
	netif_rx(skb);

	return 0;
}

static irqreturn_t rk3576_can_interrupt(int irq, void *dev_id)
{
	struct net_device *ndev = (struct net_device *)dev_id;
	struct rk3576_can *rcan = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	u32 err_int = ERR_WARN_INT | RX_BUF_OV_INT | PASSIVE_ERR_INT |
		      BUS_ERR_INT | BUS_OFF_INT | BUSOFF_RCY_INT |
		      BUS_OFF_RECOVERY_INT | RX_STR_FULL_INT;
	u32 isr;

	isr = rk3576_can_read(rcan, CAN_INT);
	if ((isr & RX_STR_TIMEOUT_INT) || (isr & ISM_WTM_INT) || (isr & RX_STR_FULL_INT)) {
		rk3576_can_write(rcan, CAN_INT_MASK,
				 ISM_WTM_INT | RX_STR_TIMEOUT_INT |
				 RX_FINISH_INT);
		if (rcan->use_dma)
			rk3576_can_rx_dma(rcan);
		else
			napi_schedule(&rcan->napi);
	}

	if (isr & TX_FINISH_INT) {
		rk3576_can_write(rcan, CAN_CMD, 0);
		stats->tx_bytes += can_get_echo_skb(ndev, 0, NULL);
		stats->tx_packets++;
		netif_wake_queue(ndev);
	}

	if (isr & err_int) {
		/* error interrupt */
		if (rk3576_can_err(ndev, isr))
			netdev_err(ndev, "can't allocate buffer - clearing pending interrupts\n");
	}

	rk3576_can_write(rcan, CAN_INT, isr);
	return IRQ_HANDLED;
}

static int rk3576_can_open(struct net_device *ndev)
{
	struct rk3576_can *rcan = netdev_priv(ndev);
	int err;

	/* common open */
	err = open_candev(ndev);
	if (err)
		return err;

	err = pm_runtime_get_sync(rcan->dev);
	if (err < 0) {
		netdev_err(ndev, "%s: pm_runtime_get failed(%d)\n",
			   __func__, err);
		goto exit;
	}

	err = rk3576_can_start(ndev);
	if (err) {
		netdev_err(ndev, "could not start CAN peripheral\n");
		goto exit_can_start;
	}

	napi_enable(&rcan->napi);
	netif_start_queue(ndev);

	netdev_dbg(ndev, "%s\n", __func__);
	return 0;

exit_can_start:
	pm_runtime_put(rcan->dev);
exit:
	close_candev(ndev);
	return err;
}

static int rk3576_can_close(struct net_device *ndev)
{
	struct rk3576_can *rcan = netdev_priv(ndev);

	netif_stop_queue(ndev);
	napi_disable(&rcan->napi);
	rk3576_can_stop(ndev);
	close_candev(ndev);
	pm_runtime_put(rcan->dev);

	netdev_dbg(ndev, "%s\n", __func__);
	return 0;
}

static const struct net_device_ops rk3576_can_netdev_ops = {
	.ndo_open = rk3576_can_open,
	.ndo_stop = rk3576_can_close,
	.ndo_start_xmit = rk3576_can_start_xmit,
	.ndo_change_mtu = can_change_mtu,
};

/**
 * rk3576_can_suspend - Suspend method for the driver
 * @dev:	Address of the device structure
 *
 * Put the driver into low power mode.
 * Return: 0 on success and failure value on error
 */
static int __maybe_unused rk3576_can_suspend(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);

	if (netif_running(ndev)) {
		netif_stop_queue(ndev);
		netif_device_detach(ndev);
		rk3576_can_stop(ndev);
	}

	return pm_runtime_force_suspend(dev);
}

/**
 * rk3576_can_resume - Resume from suspend
 * @dev:	Address of the device structure
 *
 * Resume operation after suspend.
 * Return: 0 on success and failure value on error
 */
static int __maybe_unused rk3576_can_resume(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	int ret;

	ret = pm_runtime_force_resume(dev);
	if (ret) {
		dev_err(dev, "pm_runtime_force_resume failed on resume\n");
		return ret;
	}

	if (netif_running(ndev)) {
		ret = rk3576_can_start(ndev);
		if (ret) {
			dev_err(dev, "rk3576_can_chip_start failed on resume\n");
			return ret;
		}

		netif_device_attach(ndev);
		netif_start_queue(ndev);
	}

	return 0;
}

/**
 * rk3576_can_runtime_suspend - Runtime suspend method for the driver
 * @dev:	Address of the device structure
 *
 * Put the driver into low power mode.
 * Return: 0 always
 */
static int __maybe_unused rk3576_can_runtime_suspend(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct rk3576_can *rcan = netdev_priv(ndev);

	clk_bulk_disable_unprepare(rcan->num_clks, rcan->clks);

	return 0;
}

/**
 * rk3576_can_runtime_resume - Runtime resume from suspend
 * @dev:	Address of the device structure
 *
 * Resume operation after suspend.
 * Return: 0 on success and failure value on error
 */
static int __maybe_unused rk3576_can_runtime_resume(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct rk3576_can *rcan = netdev_priv(ndev);
	int ret;

	ret = clk_bulk_prepare_enable(rcan->num_clks, rcan->clks);
	if (ret) {
		dev_err(dev, "Cannot enable clock.\n");
		return ret;
	}

	return 0;
}

static const struct dev_pm_ops rk3576_can_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(rk3576_can_suspend, rk3576_can_resume)
	SET_RUNTIME_PM_OPS(rk3576_can_runtime_suspend,
			   rk3576_can_runtime_resume, NULL)
};

static const struct of_device_id rk3576_can_of_match[] = {
	{
		.compatible = "rockchip,rk3576-can",
		.data = (void *)ROCKCHIP_RK3576_CAN
	},
	{
		.compatible = "rockchip,rv1126b-can",
		.data = (void *)ROCKCHIP_RV1126B_CAN
	},
	{},
};
MODULE_DEVICE_TABLE(of, rk3576_can_of_match);

static void rk3576_can_dma_init(struct rk3576_can *rcan)
{
	struct dma_slave_config rxconf = {
		.direction = DMA_DEV_TO_MEM,
		.src_addr = rcan->rx_dma_src_addr,
		.src_addr_width = 4,
		.dst_addr_width = 4,
		.src_maxburst = 16,
	};

	if (rcan->rx_max_data == 18)
		rxconf.src_maxburst = 9;
	else
		rxconf.src_maxburst = 4;

	rcan->dma_thr = rxconf.src_maxburst - 1;
	rcan->rxbuf = dma_alloc_coherent(rcan->dev, rcan->dma_size * rcan->rx_fifo_depth,
					 &rcan->rx_dma_dst_addr, GFP_KERNEL);
	if (!rcan->rxbuf) {
		rcan->use_dma = 0;
		return;
	}
	dmaengine_slave_config(rcan->rxchan, &rxconf);
}

static int rk3576_can_probe(struct platform_device *pdev)
{
	struct net_device *ndev;
	struct rk3576_can *rcan;
	struct resource *res;
	void __iomem *addr;
	int err, irq;
	u32 val = 0;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "could not get a valid irq\n");
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(addr))
		return -EBUSY;

	ndev = alloc_candev(sizeof(struct rk3576_can), 1);
	if (!ndev) {
		dev_err(&pdev->dev, "could not allocate memory for CAN device\n");
		return -ENOMEM;
	}
	rcan = netdev_priv(ndev);

	rcan->reset = devm_reset_control_array_get(&pdev->dev, false, false);
	if (IS_ERR(rcan->reset)) {
		if (PTR_ERR(rcan->reset) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "failed to get can reset lines\n");
		err = PTR_ERR(rcan->reset);
		goto err_free;
	}
	rcan->num_clks = devm_clk_bulk_get_all(&pdev->dev, &rcan->clks);
	if (rcan->num_clks < 1) {
		err = rcan->num_clks;
		goto err_free;
	}

	rcan->mode = (unsigned long)of_device_get_match_data(&pdev->dev);

	rcan->base = addr;
	rcan->can.clock.freq = clk_get_rate(rcan->clks[0].clk);
	rcan->dev = &pdev->dev;
	rcan->can.state = CAN_STATE_STOPPED;

	rcan->can.bittiming_const = &rk3576_can_bittiming_const;
	rcan->can.data_bittiming_const = &rk3576_can_data_bittiming_const;
	rcan->can.do_set_mode = rk3576_can_set_mode;
	rcan->can.do_get_berr_counter = rk3576_can_get_berr_counter;
	rcan->can.do_set_bittiming = rk3576_can_set_bittiming;
	rcan->can.do_set_data_bittiming = rk3576_can_set_bittiming;
	rcan->can.ctrlmode_supported = CAN_CTRLMODE_LOOPBACK |
				       CAN_CTRLMODE_LISTENONLY;

	rcan->rx_fifo_shift = INTM_LEFT_CNT_SHIFT;
	rcan->rx_fifo_mask = INTM_LEFT_CNT_MASK;

	if (device_property_read_u32(&pdev->dev, "rockchip,auto-retx-cnt", &val))
		rcan->auto_retx_cnt = 0;
	else
		rcan->auto_retx_cnt = val;
	if (rcan->auto_retx_cnt > RETX_TIME_LIMIT_CNT_MAX)
		rcan->auto_retx_cnt = RETX_TIME_LIMIT_CNT_MAX;

	/* rx-max-data only 4 Words or 18 words are supported */
	if (device_property_read_u32_array(&pdev->dev, "rockchip,rx-max-data", &val, 1))
		rcan->rx_max_data = 18;
	else
		rcan->rx_max_data = val;

	if (rcan->rx_max_data != 4 && rcan->rx_max_data != 18) {
		rcan->rx_max_data = 18;
		dev_warn(&pdev->dev, "rx_max_data is invalid, set to 18 words!\n");
	}
	rcan->rx_fifo_depth = SRAM_MAX_DEPTH / rcan->rx_max_data;

	rcan->rxchan = dma_request_chan(&pdev->dev, "rx");
	if (IS_ERR(rcan->rxchan)) {
		dev_warn(&pdev->dev, "Failed to request rxchan\n");
		rcan->rxchan = NULL;
		rcan->use_dma = 0;
	} else {
		rcan->rx_dma_src_addr = res->start + CAN_RXFRD;
		rcan->dma_size = rcan->rx_max_data * 4;
		rcan->use_dma = 1;
	}
	if (rcan->use_dma)
		rk3576_can_dma_init(rcan);

	ndev->netdev_ops = &rk3576_can_netdev_ops;
	ndev->irq = irq;
	ndev->flags |= IFF_ECHO;

	platform_set_drvdata(pdev, ndev);
	SET_NETDEV_DEV(ndev, &pdev->dev);

	pm_runtime_enable(&pdev->dev);
	err = pm_runtime_get_sync(&pdev->dev);
	if (err < 0) {
		dev_err(&pdev->dev, "%s: pm_runtime_get failed(%d)\n",
			__func__, err);
		goto err_pmdisable;
	}
	netif_napi_add(ndev, &rcan->napi, rk3576_can_rx_poll);
	err = register_candev(ndev);
	if (err) {
		dev_err(&pdev->dev, "registering %s failed (err=%d)\n",
			DRV_NAME, err);
		goto err_disableclks;
	}

	/* register interrupt handler */
	err = devm_request_irq(&pdev->dev, irq, rk3576_can_interrupt,
			       0, ndev->name, ndev);
	if (err) {
		dev_err(&pdev->dev, "request_irq err: %d\n", err);
		goto err_unregister;
	}
	dev_info(&pdev->dev, "CAN info: use_dma = %d, rx_max_data = %d, fifo_depth = %d\n",
		 rcan->use_dma, rcan->rx_max_data, rcan->rx_fifo_depth);
	return 0;

err_unregister:
	unregister_candev(ndev);
err_disableclks:
	netif_napi_del(&rcan->napi);
	pm_runtime_put(&pdev->dev);
err_pmdisable:
	if (rcan->rxbuf) {
		dma_free_coherent(rcan->dev, rcan->dma_size * rcan->rx_fifo_depth, rcan->rxbuf,
				  rcan->rx_dma_dst_addr);
		rcan->rxbuf = NULL;
	}
	if (rcan->rxchan)
		dma_release_channel(rcan->rxchan);
	pm_runtime_disable(&pdev->dev);
err_free:
	free_candev(ndev);

	return err;
}

static int rk3576_can_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct rk3576_can *rcan = netdev_priv(ndev);

	if (rcan->rxbuf) {
		dma_free_coherent(rcan->dev, rcan->dma_size * rcan->rx_fifo_depth, rcan->rxbuf,
				  rcan->rx_dma_dst_addr);
		rcan->rxbuf = NULL;
	}

	if (rcan->rxchan)
		dma_release_channel(rcan->rxchan);

	unregister_netdev(ndev);
	pm_runtime_disable(&pdev->dev);
	netif_napi_del(&rcan->napi);
	free_candev(ndev);

	return 0;
}

static struct platform_driver rk3576_can_driver = {
	.driver = {
		.name = DRV_NAME,
		.pm = &rk3576_can_dev_pm_ops,
		.of_match_table = rk3576_can_of_match,
	},
	.probe = rk3576_can_probe,
	.remove = rk3576_can_remove,
};
module_platform_driver(rk3576_can_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Elaine Zhang <zhangqing@rock-chips.com>");
MODULE_DESCRIPTION("RK3576 CAN Drivers");
