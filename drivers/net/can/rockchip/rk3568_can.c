// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 Rockchip Electronics Co. Ltd.
 * RK3568 CAN driver
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
#include <linux/rockchip/cpu.h>

/* registers definition */
enum rk3568_can_reg {
	CAN_MODE = 0x00,
	CAN_CMD = 0x04,
	CAN_STATE = 0x08,
	CAN_INT = 0x0c,
	CAN_INT_MASK = 0x10,
	CAN_LOSTARB_CODE = 0x28,
	CAN_ERR_CODE = 0x2c,
	CAN_RX_ERR_CNT = 0x34,
	CAN_TX_ERR_CNT = 0x38,
	CAN_IDCODE = 0x3c,
	CAN_IDMASK = 0x40,
	CAN_TX_CHECK_FIC = 0x50,
	CAN_NBTP = 0x100,
	CAN_DBTP = 0x104,
	CAN_TDCR = 0x108,
	CAN_TSCC = 0x10c,
	CAN_TSCV = 0x110,
	CAN_TXEFC = 0x114,
	CAN_RXFC = 0x118,
	CAN_AFC = 0x11c,
	CAN_IDCODE0 = 0x120,
	CAN_IDMASK0 = 0x124,
	CAN_IDCODE1 = 0x128,
	CAN_IDMASK1 = 0x12c,
	CAN_IDCODE2 = 0x130,
	CAN_IDMASK2 = 0x134,
	CAN_IDCODE3 = 0x138,
	CAN_IDMASK3 = 0x13c,
	CAN_IDCODE4 = 0x140,
	CAN_IDMASK4 = 0x144,
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
	CAN_TXEFRD = 0x500,
};

enum {
	ROCKCHIP_CAN_MODE = 0,
	ROCKCHIP_RK3568_CAN_MODE,
	ROCKCHIP_RK3568_CAN_MODE_V2,
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

#define MODE_FDOE		BIT(15)
#define MODE_SPACE_RX		BIT(12)
#define MODE_AUTO_RETX		BIT(10)
#define MODE_RXSORT		BIT(7)
#define MODE_TXORDER		BIT(6)
#define MODE_RXSTX		BIT(5)
#define MODE_LBACK		BIT(4)
#define MODE_SILENT		BIT(3)
#define MODE_SELF_TEST		BIT(2)
#define MODE_SLEEP		BIT(1)
#define RESET_MODE		0
#define WORK_MODE		BIT(0)

#define RX_FINISH_INT		BIT(0)
#define TX_FINISH_INT		BIT(1)
#define ERR_WARN_INT		BIT(2)
#define RX_BUF_OV_INT		BIT(3)
#define PASSIVE_ERR_INT		BIT(4)
#define TX_LOSTARB_INT		BIT(5)
#define BUS_ERR_INT		BIT(6)
#define RX_FIFO_FULL_INT	BIT(7)
#define RX_FIFO_OV_INT		BIT(8)
#define BUS_OFF_INT		BIT(9)
#define BUS_OFF_RECOVERY_INT	BIT(10)
#define TSC_OV_INT		BIT(11)
#define TXE_FIFO_OV_INT		BIT(12)
#define TXE_FIFO_FULL_INT	BIT(13)
#define WAKEUP_INT		BIT(14)

#define ERR_TYPE_MASK		GENMASK(28, 26)
#define ERR_TYPE_SHIFT		26
#define BIT_ERR			0
#define STUFF_ERR		1
#define FORM_ERR		2
#define ACK_ERR			3
#define CRC_ERR			4
#define ERR_DIR_RX		BIT(25)
#define ERR_LOC_MASK		GENMASK(15, 0)

/* Nominal Bit Timing & Prescaler Register (NBTP) */
#define NBTP_MODE_3_SAMPLES	BIT(31)
#define NBTP_NSJW_SHIFT		24
#define NBTP_NSJW_MASK		(0x7f << NBTP_NSJW_SHIFT)
#define NBTP_NBRP_SHIFT		16
#define NBTP_NBRP_MASK		(0xff << NBTP_NBRP_SHIFT)
#define NBTP_NTSEG2_SHIFT	8
#define NBTP_NTSEG2_MASK	(0x7f << NBTP_NTSEG2_SHIFT)
#define NBTP_NTSEG1_SHIFT	0
#define NBTP_NTSEG1_MASK	(0x7f << NBTP_NTSEG1_SHIFT)

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

#define FIFO_ENABLE		BIT(0)
#define RX_FIFO_CNT0_SHIFT	4
#define RX_FIFO_CNT0_MASK	(0x7 << RX_FIFO_CNT0_SHIFT)
#define RX_FIFO_CNT1_SHIFT	5
#define RX_FIFO_CNT1_MASK	(0x7 << RX_FIFO_CNT1_SHIFT)

#define FORMAT_SHIFT		7
#define FORMAT_MASK		(0x1 << FORMAT_SHIFT)
#define RTR_SHIFT		6
#define RTR_MASK		(0x1 << RTR_SHIFT)
#define DLC_SHIFT		0
#define DLC_MASK		(0xF << DLC_SHIFT)

#define CAN_RF_SIZE		0x48
#define CAN_TEF_SIZE		0x8
#define CAN_TXEFRD_OFFSET(n)	(CAN_TXEFRD + CAN_TEF_SIZE * (n))
#define CAN_RXFRD_OFFSET(n)	(CAN_RXFRD + CAN_RF_SIZE * (n))

#define CAN_RX_FILTER_MASK	0x1fffffff
#define NOACK_ERR_FLAG		0xc200800
#define CAN_BUSOFF_FLAG		0x20

#define DRV_NAME	"rk3568_can"

/* rk3568_can private data structure */

struct rk3568_can {
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
	bool txtorx;
	u32 tx_invalid[4];
	struct delayed_work tx_err_work;
	u32 delay_time_ms;
};

static inline u32 rk3568_can_read(const struct rk3568_can *priv,
				  enum rk3568_can_reg reg)
{
	return readl(priv->base + reg);
}

static inline void rk3568_can_write(const struct rk3568_can *priv,
				    enum rk3568_can_reg reg, u32 val)
{
	writel(val, priv->base + reg);
}

static const struct can_bittiming_const rk3568_can_bittiming_const = {
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

static int set_reset_mode(struct net_device *ndev)
{
	struct rk3568_can *rcan = netdev_priv(ndev);

	reset_control_assert(rcan->reset);
	udelay(2);
	reset_control_deassert(rcan->reset);

	rk3568_can_write(rcan, CAN_MODE, 0);

	netdev_dbg(ndev, "%s MODE=0x%08x\n", __func__,
		   rk3568_can_read(rcan, CAN_MODE));

	return 0;
}

static int set_normal_mode(struct net_device *ndev)
{
	struct rk3568_can *rcan = netdev_priv(ndev);
	u32 val;

	val = rk3568_can_read(rcan, CAN_MODE);
	val |= WORK_MODE;
	rk3568_can_write(rcan, CAN_MODE, val);

	netdev_dbg(ndev, "%s MODE=0x%08x\n", __func__,
		   rk3568_can_read(rcan, CAN_MODE));
	return 0;
}

/* bittiming is called in reset_mode only */
static int rk3568_can_set_bittiming(struct net_device *ndev)
{
	struct rk3568_can *rcan = netdev_priv(ndev);
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

	rk3568_can_write(rcan, CAN_NBTP, reg_btp);

	if (bt->bitrate > 200000)
		rcan->delay_time_ms = 1;
	else if (bt->bitrate > 50000)
		rcan->delay_time_ms = 5;
	else
		rcan->delay_time_ms = 20;

	netdev_dbg(ndev, "%s NBTP=0x%08x, DBTP=0x%08x, TDCR=0x%08x\n", __func__,
		   rk3568_can_read(rcan, CAN_NBTP),
		   rk3568_can_read(rcan, CAN_DBTP),
		   rk3568_can_read(rcan, CAN_TDCR));
	return 0;
}

static int rk3568_can_get_berr_counter(const struct net_device *ndev,
				       struct can_berr_counter *bec)
{
	struct rk3568_can *rcan = netdev_priv(ndev);
	int err;

	err = pm_runtime_get_sync(rcan->dev);
	if (err < 0) {
		netdev_err(ndev, "%s: pm_runtime_get failed(%d)\n",
			   __func__, err);
		return err;
	}

	bec->rxerr = rk3568_can_read(rcan, CAN_RX_ERR_CNT);
	bec->txerr = rk3568_can_read(rcan, CAN_TX_ERR_CNT);

	pm_runtime_put(rcan->dev);

	netdev_dbg(ndev, "%s RX_ERR_CNT=0x%08x, TX_ERR_CNT=0x%08x\n", __func__,
		   rk3568_can_read(rcan, CAN_RX_ERR_CNT),
		   rk3568_can_read(rcan, CAN_TX_ERR_CNT));

	return 0;
}

static int rk3568_can_start(struct net_device *ndev)
{
	struct rk3568_can *rcan = netdev_priv(ndev);
	u32 val;

	/* we need to enter the reset mode */
	set_reset_mode(ndev);

	rk3568_can_write(rcan, CAN_INT_MASK, 0);

	/* RECEIVING FILTER, accept all */
	rk3568_can_write(rcan, CAN_IDCODE, 0);
	rk3568_can_write(rcan, CAN_IDMASK, CAN_RX_FILTER_MASK);
	rk3568_can_write(rcan, CAN_IDCODE0, 0);
	rk3568_can_write(rcan, CAN_IDMASK0, CAN_RX_FILTER_MASK);
	rk3568_can_write(rcan, CAN_IDCODE1, 0);
	rk3568_can_write(rcan, CAN_IDMASK1, CAN_RX_FILTER_MASK);
	rk3568_can_write(rcan, CAN_IDCODE2, 0);
	rk3568_can_write(rcan, CAN_IDMASK2, CAN_RX_FILTER_MASK);
	rk3568_can_write(rcan, CAN_IDCODE3, 0);
	rk3568_can_write(rcan, CAN_IDMASK3, CAN_RX_FILTER_MASK);
	rk3568_can_write(rcan, CAN_IDCODE4, 0);
	rk3568_can_write(rcan, CAN_IDMASK4, CAN_RX_FILTER_MASK);

	/* set mode */
	val = rk3568_can_read(rcan, CAN_MODE);

	/* rx fifo enable */
	rk3568_can_write(rcan, CAN_RXFC,
			 rk3568_can_read(rcan, CAN_RXFC) | FIFO_ENABLE);

	/* Mode */
	val |= MODE_FDOE;

	/* Loopback Mode */
	if (rcan->can.ctrlmode & CAN_CTRLMODE_LOOPBACK)
		val |= MODE_SELF_TEST | MODE_LBACK;

	/* Listen-only mode */
	if (rcan->can.ctrlmode & CAN_CTRLMODE_LISTENONLY)
		val |= MODE_SILENT;

	rk3568_can_write(rcan, CAN_MODE, val);

	rk3568_can_set_bittiming(ndev);

	set_normal_mode(ndev);

	rcan->can.state = CAN_STATE_ERROR_ACTIVE;

	netdev_dbg(ndev, "%s MODE=0x%08x, INT_MASK=0x%08x\n", __func__,
		   rk3568_can_read(rcan, CAN_MODE),
		   rk3568_can_read(rcan, CAN_INT_MASK));

	return 0;
}

static int rk3568_can_stop(struct net_device *ndev)
{
	struct rk3568_can *rcan = netdev_priv(ndev);

	rcan->can.state = CAN_STATE_STOPPED;
	/* we need to enter reset mode */
	set_reset_mode(ndev);

	/* disable all interrupts */
	rk3568_can_write(rcan, CAN_INT_MASK, 0xffff);

	netdev_dbg(ndev, "%s MODE=0x%08x, INT_MASK=0x%08x\n", __func__,
		   rk3568_can_read(rcan, CAN_MODE),
		   rk3568_can_read(rcan, CAN_INT_MASK));
	return 0;
}

static int rk3568_can_set_mode(struct net_device *ndev,
			       enum can_mode mode)
{
	int err;

	switch (mode) {
	case CAN_MODE_START:
		err = rk3568_can_start(ndev);
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

static void rk3568_can_tx_err_delay_work(struct work_struct *work)
{
	struct rk3568_can *rcan =
		container_of(work, struct rk3568_can, tx_err_work.work);
	u32 mode, err_code;

	mode = rk3568_can_read(rcan, CAN_MODE);
	err_code = rk3568_can_read(rcan, CAN_ERR_CODE);
	if ((err_code & NOACK_ERR_FLAG) == NOACK_ERR_FLAG) {
		rk3568_can_write(rcan, CAN_MODE,
				 rk3568_can_read(rcan, CAN_MODE) | MODE_SPACE_RX);
		rk3568_can_write(rcan, CAN_CMD, CAN_TX0_REQ);
		rk3568_can_write(rcan, CAN_MODE,
				 rk3568_can_read(rcan, CAN_MODE) & (~MODE_SPACE_RX));
		schedule_delayed_work(&rcan->tx_err_work, msecs_to_jiffies(rcan->delay_time_ms));
	} else {
		rk3568_can_write(rcan, CAN_MODE, 0);
		rk3568_can_write(rcan, CAN_MODE, mode);
		rk3568_can_write(rcan, CAN_MODE,
				 rk3568_can_read(rcan, CAN_MODE) | MODE_SPACE_RX);
		rk3568_can_write(rcan, CAN_CMD, CAN_TX0_REQ);
		rk3568_can_write(rcan, CAN_MODE,
				 rk3568_can_read(rcan, CAN_MODE) & (~MODE_SPACE_RX));
		schedule_delayed_work(&rcan->tx_err_work, msecs_to_jiffies(rcan->delay_time_ms));
	}
}

/* transmit a CAN message
 * message layout in the sk_buff should be like this:
 * xx xx xx xx         ff         ll 00 11 22 33 44 55 66 77
 * [ can_id ] [flags] [len] [can data (up to 8 bytes]
 */
static netdev_tx_t rk3568_can_start_xmit(struct sk_buff *skb,
					 struct net_device *ndev)
{
	struct rk3568_can *rcan = netdev_priv(ndev);
	struct can_frame *cf = (struct can_frame *)skb->data;
	u32 id, dlc;
	u32 cmd = CAN_TX0_REQ;
	int i;
	unsigned long flags;

	if (can_dropped_invalid_skb(ndev, skb))
		return NETDEV_TX_OK;

	netif_stop_queue(ndev);

	if (rk3568_can_read(rcan, CAN_CMD) & CAN_TX0_REQ)
		cmd = CAN_TX1_REQ;

	/* Watch carefully on the bit sequence */
	if (cf->can_id & CAN_EFF_FLAG) {
		/* Extended CAN ID format */
		id = cf->can_id & CAN_EFF_MASK;
		dlc = cf->can_dlc & DLC_MASK;
		dlc |= FORMAT_MASK;

		/* Extended frames remote TX request */
		if (cf->can_id & CAN_RTR_FLAG)
			dlc |= RTR_MASK;
	} else {
		/* Standard CAN ID format */
		id = cf->can_id & CAN_SFF_MASK;
		dlc = cf->can_dlc & DLC_MASK;

		/* Standard frames remote TX request */
		if (cf->can_id & CAN_RTR_FLAG)
			dlc |= RTR_MASK;
	}

	if (rcan->txtorx && rcan->mode <= ROCKCHIP_RK3568_CAN_MODE && cf->can_id & CAN_EFF_FLAG)
		rk3568_can_write(rcan, CAN_MODE,
				 rk3568_can_read(rcan, CAN_MODE) | MODE_RXSTX);
	else
		rk3568_can_write(rcan, CAN_MODE,
				 rk3568_can_read(rcan, CAN_MODE) & (~MODE_RXSTX));

	if (!rcan->txtorx && rcan->mode <= ROCKCHIP_RK3568_CAN_MODE && cf->can_id & CAN_EFF_FLAG) {
		/* Two frames are sent consecutively.
		 * Before the first frame is tx finished,
		 * the register of the second frame is configured.
		 * Don't be interrupted in the middle.
		 */
		local_irq_save(flags);
		rk3568_can_write(rcan, CAN_TXID, rcan->tx_invalid[1]);
		rk3568_can_write(rcan, CAN_TXFIC, rcan->tx_invalid[0]);
		rk3568_can_write(rcan, CAN_TXDAT0, rcan->tx_invalid[2]);
		rk3568_can_write(rcan, CAN_TXDAT1, rcan->tx_invalid[3]);
		rk3568_can_write(rcan, CAN_CMD, CAN_TX0_REQ);
		rk3568_can_write(rcan, CAN_TXID, id);
		rk3568_can_write(rcan, CAN_TXFIC, dlc);
		for (i = 0; i < can_cc_dlc2len(cf->can_dlc & DLC_MASK); i += 4)
			rk3568_can_write(rcan, CAN_TXDAT0 + i,
					 *(u32 *)(cf->data + i));
		can_put_echo_skb(skb, ndev, 0, 0);
		rk3568_can_write(rcan, CAN_CMD, CAN_TX1_REQ);
		local_irq_restore(flags);
		return NETDEV_TX_OK;
	}

	rk3568_can_write(rcan, CAN_TXID, id);
	rk3568_can_write(rcan, CAN_TXFIC, dlc);

	for (i = 0; i < can_cc_dlc2len(cf->can_dlc & DLC_MASK); i += 4)
		rk3568_can_write(rcan, CAN_TXDAT0 + i,
				 *(u32 *)(cf->data + i));

	can_put_echo_skb(skb, ndev, 0, 0);
	rk3568_can_write(rcan, CAN_MODE,
			 rk3568_can_read(rcan, CAN_MODE) | MODE_SPACE_RX);
	rk3568_can_write(rcan, CAN_CMD, cmd);
	rk3568_can_write(rcan, CAN_MODE,
			 rk3568_can_read(rcan, CAN_MODE) & (~MODE_SPACE_RX));
	schedule_delayed_work(&rcan->tx_err_work, msecs_to_jiffies(rcan->delay_time_ms));
	return NETDEV_TX_OK;
}

static int rk3568_can_rx(struct net_device *ndev)
{
	struct rk3568_can *rcan = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	struct can_frame *cf;
	struct sk_buff *skb;
	u32 id_rk3568_can, dlc;
	int i = 0;
	u32 __maybe_unused ts, ret;
	u32 data[16];

	dlc = rk3568_can_read(rcan, CAN_RXFRD);
	id_rk3568_can = rk3568_can_read(rcan, CAN_RXFRD);
	ts = rk3568_can_read(rcan, CAN_RXFRD);
	for (i = 0; i < ARRAY_SIZE(data); i++)
		data[i] = rk3568_can_read(rcan, CAN_RXFRD);

	if (rcan->mode <= ROCKCHIP_RK3568_CAN_MODE) {
		/* may be an empty frame */
		if (!dlc && !id_rk3568_can)
			return 1;

		if (rcan->txtorx) {
			if (rk3568_can_read(rcan, CAN_TX_CHECK_FIC) & FORMAT_MASK) {
				ret = rk3568_can_read(rcan, CAN_TXID) & CAN_SFF_MASK;
				if (id_rk3568_can == ret && !(dlc & FORMAT_MASK))
					rk3568_can_write(rcan, CAN_TX_CHECK_FIC,
							 ts | CAN_TX0_REQ);
				return 1;
			}
		}
	}

	/* create zero'ed CAN frame buffer */
	skb = alloc_can_skb(ndev, (struct can_frame **)&cf);
	if (!skb) {
		stats->rx_dropped++;
		return 1;
	}

	/* Change CAN data length format to socketCAN data format */
	cf->can_dlc = can_cc_dlc2len(dlc & DLC_MASK);

	/* Change CAN ID format to socketCAN ID format */
	if (dlc & FORMAT_MASK) {
		/* The received frame is an Extended format frame */
		cf->can_id = id_rk3568_can;
		cf->can_id |= CAN_EFF_FLAG;
		if (dlc & RTR_MASK)
			cf->can_id |= CAN_RTR_FLAG;
	} else {
		/* The received frame is a standard format frame */
		cf->can_id = id_rk3568_can;
		if (dlc & RTR_MASK)
			cf->can_id |= CAN_RTR_FLAG;
	}

	if (!(cf->can_id & CAN_RTR_FLAG)) {
		/* Change CAN data format to socketCAN data format */
		for (i = 0; i < cf->can_dlc; i += 4)
			*(u32 *)(cf->data + i) = data[i / 4];
	}

	stats->rx_packets++;
	stats->rx_bytes += cf->can_dlc;
	netif_rx(skb);

	return 1;
}

static int rk3568_can_get_rx_fifo_cnt(struct net_device *ndev)
{
	struct rk3568_can *rcan = netdev_priv(ndev);
	int quota = 0;

	if (read_poll_timeout_atomic(rk3568_can_read, quota,
				     (quota & rcan->rx_fifo_mask) >> rcan->rx_fifo_shift,
				     0, 500000, false, rcan, CAN_RXFC))
		netdev_dbg(ndev, "Warning: get fifo cnt failed\n");

	quota = (quota & rcan->rx_fifo_mask) >> rcan->rx_fifo_shift;

	return quota;
}

/* rk3568_can_rx_poll - Poll routine for rx packets (NAPI)
 * @napi:	napi structure pointer
 * @quota:	Max number of rx packets to be processed.
 *
 * This is the poll routine for rx part.
 * It will process the packets maximux quota value.
 *
 * Return: number of packets received
 */
static int rk3568_can_rx_poll(struct napi_struct *napi, int quota)
{
	struct net_device *ndev = napi->dev;
	struct rk3568_can *rcan = netdev_priv(ndev);
	int work_done = 0;

	quota = rk3568_can_get_rx_fifo_cnt(ndev);
	if (quota) {
		while (work_done < quota)
			work_done += rk3568_can_rx(ndev);
	}

	if (work_done < 6) {
		napi_complete_done(napi, work_done);
		rk3568_can_write(rcan, CAN_INT_MASK, 0);
	}

	return work_done;
}

static int rk3568_can_err(struct net_device *ndev, u32 isr)
{
	struct rk3568_can *rcan = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	struct can_frame *cf;
	struct sk_buff *skb;
	unsigned int rxerr, txerr;
	u32 sta_reg;

	skb = alloc_can_err_skb(ndev, &cf);

	rxerr = rk3568_can_read(rcan, CAN_RX_ERR_CNT);
	txerr = rk3568_can_read(rcan, CAN_TX_ERR_CNT);
	sta_reg = rk3568_can_read(rcan, CAN_STATE);

	if (skb) {
		cf->data[6] = txerr;
		cf->data[7] = rxerr;
	}

	if (isr & BUS_OFF_INT) {
		rcan->can.state = CAN_STATE_BUS_OFF;
		rcan->can.can_stats.bus_off++;
		cf->can_id |= CAN_ERR_BUSOFF;
	} else if (isr & ERR_WARN_INT) {
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

	if (rcan->can.state >= CAN_STATE_BUS_OFF ||
	    ((sta_reg & CAN_BUSOFF_FLAG) == CAN_BUSOFF_FLAG)) {
		cancel_delayed_work(&rcan->tx_err_work);
		netif_stop_queue(ndev);
		rk3568_can_stop(ndev);
		can_free_echo_skb(ndev, 0, NULL);
		rk3568_can_start(ndev);
		netif_start_queue(ndev);
	}

	stats->rx_packets++;
	stats->rx_bytes += cf->can_dlc;
	netif_rx(skb);

	return 0;
}

static irqreturn_t rk3568_can_interrupt(int irq, void *dev_id)
{
	struct net_device *ndev = (struct net_device *)dev_id;
	struct rk3568_can *rcan = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	u32 err_int = ERR_WARN_INT | RX_BUF_OV_INT | PASSIVE_ERR_INT |
		      BUS_ERR_INT | BUS_OFF_INT;
	u32 isr;
	u32 dlc = 0;
	u32 quota, work_done = 0;

	isr = rk3568_can_read(rcan, CAN_INT);
	if (isr & TX_FINISH_INT) {
		cancel_delayed_work(&rcan->tx_err_work);
		dlc = rk3568_can_read(rcan, CAN_TXFIC);
		/* transmission complete interrupt */
		stats->tx_bytes += (dlc & DLC_MASK);
		stats->tx_packets++;
		if (rcan->txtorx && rcan->mode <= ROCKCHIP_RK3568_CAN_MODE && dlc & FORMAT_MASK) {
			rk3568_can_write(rcan, CAN_TX_CHECK_FIC, FORMAT_MASK);
			quota = rk3568_can_get_rx_fifo_cnt(ndev);
			if (quota) {
				while (work_done < quota)
					work_done += rk3568_can_rx(ndev);
			}
			if (rk3568_can_read(rcan, CAN_TX_CHECK_FIC) & CAN_TX0_REQ)
				rk3568_can_write(rcan, CAN_CMD, CAN_TX1_REQ);
			rk3568_can_write(rcan, CAN_TX_CHECK_FIC, 0);
		}
		if (read_poll_timeout_atomic(rk3568_can_read, quota,
					     !(quota & 0x3),
					     0, 5000000, false, rcan, CAN_CMD))
			netdev_err(ndev, "Warning: wait tx req timeout!\n");
		rk3568_can_write(rcan, CAN_CMD, 0);
		stats->tx_bytes += can_get_echo_skb(ndev, 0, NULL);
		stats->tx_packets++;
		netif_wake_queue(ndev);
	}

	if (isr & RX_FINISH_INT) {
		if (rcan->mode == ROCKCHIP_RK3568_CAN_MODE_V2) {
			rk3568_can_write(rcan, CAN_INT_MASK, 0x1);
			napi_schedule(&rcan->napi);
		} else {
			work_done = 0;
			quota = (rk3568_can_read(rcan, CAN_RXFC) &
				 rcan->rx_fifo_mask) >>
				rcan->rx_fifo_shift;
			if (quota) {
				while (work_done < quota)
					work_done += rk3568_can_rx(ndev);
			}
		}
	}

	if (isr & err_int) {
		/* error interrupt */
		if (rk3568_can_err(ndev, isr))
			netdev_err(ndev, "can't allocate buffer - clearing pending interrupts\n");
	}

	rk3568_can_write(rcan, CAN_INT, isr);
	return IRQ_HANDLED;
}

static int rk3568_can_open(struct net_device *ndev)
{
	struct rk3568_can *rcan = netdev_priv(ndev);
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

	err = rk3568_can_start(ndev);
	if (err) {
		netdev_err(ndev, "could not start CAN peripheral\n");
		goto exit_can_start;
	}

	if (rcan->mode == ROCKCHIP_RK3568_CAN_MODE_V2)
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

static int rk3568_can_close(struct net_device *ndev)
{
	struct rk3568_can *rcan = netdev_priv(ndev);

	netif_stop_queue(ndev);
	if (rcan->mode == ROCKCHIP_RK3568_CAN_MODE_V2)
		napi_disable(&rcan->napi);
	rk3568_can_stop(ndev);
	close_candev(ndev);
	pm_runtime_put(rcan->dev);
	cancel_delayed_work_sync(&rcan->tx_err_work);

	netdev_dbg(ndev, "%s\n", __func__);
	return 0;
}

static const struct net_device_ops rk3568_can_netdev_ops = {
	.ndo_open = rk3568_can_open,
	.ndo_stop = rk3568_can_close,
	.ndo_start_xmit = rk3568_can_start_xmit,
	.ndo_change_mtu = can_change_mtu,
};

/**
 * rk3568_can_suspend - Suspend method for the driver
 * @dev:	Address of the device structure
 *
 * Put the driver into low power mode.
 * Return: 0 on success and failure value on error
 */
static int __maybe_unused rk3568_can_suspend(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);

	if (netif_running(ndev)) {
		netif_stop_queue(ndev);
		netif_device_detach(ndev);
		rk3568_can_stop(ndev);
	}

	return pm_runtime_force_suspend(dev);
}

/**
 * rk3568_can_resume - Resume from suspend
 * @dev:	Address of the device structure
 *
 * Resume operation after suspend.
 * Return: 0 on success and failure value on error
 */
static int __maybe_unused rk3568_can_resume(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	int ret;

	ret = pm_runtime_force_resume(dev);
	if (ret) {
		dev_err(dev, "pm_runtime_force_resume failed on resume\n");
		return ret;
	}

	if (netif_running(ndev)) {
		ret = rk3568_can_start(ndev);
		if (ret) {
			dev_err(dev, "rk3568_can_chip_start failed on resume\n");
			return ret;
		}

		netif_device_attach(ndev);
		netif_start_queue(ndev);
	}

	return 0;
}

/**
 * rk3568_can_runtime_suspend - Runtime suspend method for the driver
 * @dev:	Address of the device structure
 *
 * Put the driver into low power mode.
 * Return: 0 always
 */
static int __maybe_unused rk3568_can_runtime_suspend(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct rk3568_can *rcan = netdev_priv(ndev);

	clk_bulk_disable_unprepare(rcan->num_clks, rcan->clks);

	return 0;
}

/**
 * rk3568_can_runtime_resume - Runtime resume from suspend
 * @dev:	Address of the device structure
 *
 * Resume operation after suspend.
 * Return: 0 on success and failure value on error
 */
static int __maybe_unused rk3568_can_runtime_resume(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct rk3568_can *rcan = netdev_priv(ndev);
	int ret;

	ret = clk_bulk_prepare_enable(rcan->num_clks, rcan->clks);
	if (ret) {
		dev_err(dev, "Cannot enable clock.\n");
		return ret;
	}

	return 0;
}

static const struct dev_pm_ops rk3568_can_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(rk3568_can_suspend, rk3568_can_resume)
	SET_RUNTIME_PM_OPS(rk3568_can_runtime_suspend,
			   rk3568_can_runtime_resume, NULL)
};

static const struct of_device_id rk3568_can_of_match[] = {
	{
		.compatible = "rockchip,can-2.0",
		.data = (void *)ROCKCHIP_CAN_MODE
	},
	{
		.compatible = "rockchip,rk3568-can-2.0",
		.data = (void *)ROCKCHIP_RK3568_CAN_MODE
	},
	{},
};
MODULE_DEVICE_TABLE(of, rk3568_can_of_match);

static int rk3568_can_probe(struct platform_device *pdev)
{
	struct net_device *ndev;
	struct rk3568_can *rcan;
	struct resource *res;
	void __iomem *addr;
	int err, irq;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "could not get a valid irq\n");
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(addr))
		return -EBUSY;

	ndev = alloc_candev(sizeof(struct rk3568_can), 1);
	if (!ndev) {
		dev_err(&pdev->dev, "could not allocate memory for CAN device\n");
		return -ENOMEM;
	}
	rcan = netdev_priv(ndev);

	/* register interrupt handler */
	err = devm_request_irq(&pdev->dev, irq, rk3568_can_interrupt,
			       0, ndev->name, ndev);
	if (err) {
		dev_err(&pdev->dev, "request_irq err: %d\n", err);
		return err;
	}

	rcan->reset = devm_reset_control_array_get(&pdev->dev, false, false);
	if (IS_ERR(rcan->reset)) {
		if (PTR_ERR(rcan->reset) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "failed to get can reset lines\n");
		return PTR_ERR(rcan->reset);
	}
	rcan->num_clks = devm_clk_bulk_get_all(&pdev->dev, &rcan->clks);
	if (rcan->num_clks < 1)
		return -ENODEV;

	rcan->mode = (unsigned long)of_device_get_match_data(&pdev->dev);

	if ((cpu_is_rk3566() || cpu_is_rk3568()) && (rockchip_get_cpu_version() == 3))
		rcan->mode = ROCKCHIP_RK3568_CAN_MODE_V2;

	rcan->base = addr;
	rcan->can.clock.freq = clk_get_rate(rcan->clks[0].clk);
	rcan->dev = &pdev->dev;
	rcan->can.state = CAN_STATE_STOPPED;
	switch (rcan->mode) {
	case ROCKCHIP_CAN_MODE:
	case ROCKCHIP_RK3568_CAN_MODE:
	case ROCKCHIP_RK3568_CAN_MODE_V2:
		rcan->can.bittiming_const = &rk3568_can_bittiming_const;
		rcan->can.do_set_mode = rk3568_can_set_mode;
		rcan->can.do_get_berr_counter = rk3568_can_get_berr_counter;
		rcan->can.ctrlmode_supported = CAN_CTRLMODE_BERR_REPORTING |
					       CAN_CTRLMODE_LISTENONLY |
					       CAN_CTRLMODE_LOOPBACK |
					       CAN_CTRLMODE_3_SAMPLES;
		rcan->rx_fifo_shift = RX_FIFO_CNT0_SHIFT;
		rcan->rx_fifo_mask = RX_FIFO_CNT0_MASK;
		break;
	default:
		return -EINVAL;
	}

	if (rcan->mode == ROCKCHIP_CAN_MODE) {
		rcan->rx_fifo_shift = RX_FIFO_CNT1_SHIFT;
		rcan->rx_fifo_mask = RX_FIFO_CNT1_MASK;
	}

	if (device_property_read_u32_array(&pdev->dev,
					   "rockchip,tx-invalid-info",
					   rcan->tx_invalid, 4))
		rcan->txtorx = 1;

	if (rcan->mode == ROCKCHIP_RK3568_CAN_MODE_V2) {
		rcan->txtorx = 0;
		netif_napi_add(ndev, &rcan->napi, rk3568_can_rx_poll);
	}

	ndev->netdev_ops = &rk3568_can_netdev_ops;
	ndev->irq = irq;
	ndev->flags |= IFF_ECHO;
	rcan->can.restart_ms = 1;

	irq_set_affinity_hint(irq, get_cpu_mask(num_online_cpus() - 1));

	INIT_DELAYED_WORK(&rcan->tx_err_work, rk3568_can_tx_err_delay_work);

	platform_set_drvdata(pdev, ndev);
	SET_NETDEV_DEV(ndev, &pdev->dev);

	pm_runtime_enable(&pdev->dev);
	err = pm_runtime_get_sync(&pdev->dev);
	if (err < 0) {
		dev_err(&pdev->dev, "%s: pm_runtime_get failed(%d)\n",
			__func__, err);
		goto err_pmdisable;
	}

	err = register_candev(ndev);
	if (err) {
		dev_err(&pdev->dev, "registering %s failed (err=%d)\n",
			DRV_NAME, err);
		goto err_disableclks;
	}

	return 0;

err_disableclks:
	pm_runtime_put(&pdev->dev);
err_pmdisable:
	pm_runtime_disable(&pdev->dev);
	free_candev(ndev);

	return err;
}

static int rk3568_can_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct rk3568_can *rcan = netdev_priv(ndev);

	unregister_netdev(ndev);
	pm_runtime_disable(&pdev->dev);
	if (rcan->mode == ROCKCHIP_RK3568_CAN_MODE_V2)
		netif_napi_del(&rcan->napi);
	free_candev(ndev);

	return 0;
}

static struct platform_driver rk3568_can_driver = {
	.driver = {
		.name = DRV_NAME,
		.pm = &rk3568_can_dev_pm_ops,
		.of_match_table = rk3568_can_of_match,
	},
	.probe = rk3568_can_probe,
	.remove = rk3568_can_remove,
};
module_platform_driver(rk3568_can_driver);

MODULE_AUTHOR("Elaine Zhang <zhangqing@rock-chips.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RK3568 CAN Drivers");
