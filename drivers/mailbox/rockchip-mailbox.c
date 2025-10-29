// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2024, Rockchip Electronics Co., Ltd.
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/string_helpers.h>
#include <soc/rockchip/rockchip-mailbox.h>

#define MAILBOX_A2B_INTEN		0x00
#define MAILBOX_A2B_STATUS		0x04
#define MAILBOX_A2B_CMD(x)		(0x08 + (x) * 8)
#define MAILBOX_A2B_DAT(x)		(0x0c + (x) * 8)

#define MAILBOX_B2A_INTEN		0x28
#define MAILBOX_B2A_STATUS		0x2C
#define MAILBOX_B2A_CMD(x)		(0x30 + (x) * 8)
#define MAILBOX_B2A_DAT(x)		(0x34 + (x) * 8)

#define MAILBOX_V2_A2B_INTEN		MAILBOX_A2B_INTEN
#define MAILBOX_V2_A2B_STATUS		MAILBOX_A2B_STATUS
#define MAILBOX_V2_A2B_CMD		0x08
#define MAILBOX_V2_A2B_DAT		0x0c
#define MAILBOX_V2_A2B_LOCK		0x20

#define MAILBOX_V2_B2A_INTEN		0x10
#define MAILBOX_V2_B2A_STATUS		0x14
#define MAILBOX_V2_B2A_CMD		0x18
#define MAILBOX_V2_B2A_DAT		0x1c
#define MAILBOX_V2_B2A_LOCK		0x30

#define MAILBOX_V2_VERSION		0x40
#define MAILBOX_V2_VERSION_MASK		GENMASK(31, 16)

#define MAILBOX_V2_TRIGGER_SHIFT	8
#define MAILBOX_V2_TRIGGER_MASK		BIT(8)

#define MAILBOX_V2_INTEN_TX_DONE	BIT(0)
#define MAILBOX_V2_INTEN_RX_DONE	BIT(1)
#define MAILBOX_V2_INTEN_RX_DONE_SHIFT	1

#define MAILBOX_V2_STATUS_TX_DONE	BIT(0)
#define MAILBOX_V2_STATUS_RX_DONE	BIT(1)
#define MAILBOX_V2_STATUS_MASK		GENMASK(1, 0)

#define MAILBOX_VERSION_1_0_0		0x100U
#define MAILBOX_VERSION_2_0_0		0x200U
#define MAILBOX_VERSION_2_1_0		0x210U
#define MAILBOX_VERSION_2_2_0		0x220U

#define MAILBOX_POLLING_MS		5 /* default polling interval 5ms */
#define BIT_WRITEABLE_SHIFT		16

struct rockchip_mbox_reg {
	u32 tx_int;
	u32 tx_sts;
	u32 tx_cmd;
	u32 tx_dat;
	u32 tx_lock;
	u32 rx_int;
	u32 rx_sts;
	u32 rx_cmd;
	u32 rx_dat;
	u32 rx_lock;
};

struct rockchip_mbox_data {
	int num_chans;
	u32 version;
	struct rockchip_mbox_reg reg_a2b;
	struct rockchip_mbox_reg reg_b2a;
	const struct mbox_chan_ops *ops;
	irqreturn_t (*irq_func)(int irq, void *dev_id);
};

struct rockchip_mbox_chan {
	int idx;
	int irq;
};

struct rockchip_mbox {
	struct mbox_controller mbox;
	struct clk *pclk;
	void __iomem *mbox_base;
	spinlock_t cfg_lock; /* Serialise access to the register */
	unsigned char trigger_method; /* 0 = write cmd, 1 = write cmd first, then write data */
	u32 version;
	struct rockchip_mbox_msg *msg;
	const struct rockchip_mbox_reg *reg;

	struct rockchip_mbox_chan *chans;
};

static int rockchip_mbox_send_data(struct mbox_chan *chan, void *data)
{
	struct rockchip_mbox *mb = dev_get_drvdata(chan->mbox->dev);
	struct rockchip_mbox_msg *msg = data;
	struct rockchip_mbox_chan *chans = chan->con_priv;
	u32 status;

	if (!msg)
		return -EINVAL;

	status = readl_relaxed(mb->mbox_base + MAILBOX_A2B_STATUS);
	if (status & (1U << chans->idx)) {
		dev_err(mb->mbox.dev, "The mailbox channel is busy\n");
		return -EBUSY;
	}

	dev_dbg(mb->mbox.dev, "Chan[%d]: A2B message, cmd 0x%08x, data 0x%08x\n",
		chans->idx, msg->cmd, msg->data);

	writel_relaxed(msg->cmd, mb->mbox_base + MAILBOX_A2B_CMD(chans->idx));
	writel_relaxed(msg->data, mb->mbox_base +
		       MAILBOX_A2B_DAT(chans->idx));

	return 0;
}

static int rockchip_mbox_startup(struct mbox_chan *chan)
{
	struct rockchip_mbox *mb = dev_get_drvdata(chan->mbox->dev);
	struct rockchip_mbox_chan *chans = chan->con_priv;
	u32 val = 0U;

	/* Enable the corresponding B2A interrupt */
	spin_lock(&mb->cfg_lock);
	val = readl_relaxed(mb->mbox_base + MAILBOX_B2A_INTEN) |
		(1U << chans->idx);
	writel_relaxed(val, mb->mbox_base + MAILBOX_B2A_INTEN);
	spin_unlock(&mb->cfg_lock);

	return 0;
}

static void rockchip_mbox_shutdown(struct mbox_chan *chan)
{
	struct rockchip_mbox *mb = dev_get_drvdata(chan->mbox->dev);
	struct rockchip_mbox_chan *chans = chan->con_priv;
	u32 val = 0U;

	/* Disable the corresponding B2A interrupt */
	spin_lock(&mb->cfg_lock);
	val = readl_relaxed(mb->mbox_base + MAILBOX_B2A_INTEN) &
		~(1U << chans->idx);
	writel_relaxed(val, mb->mbox_base + MAILBOX_B2A_INTEN);
	spin_unlock(&mb->cfg_lock);
}

static bool rockchip_mbox_last_tx_done(struct mbox_chan *chan)
{
	struct rockchip_mbox *mb = dev_get_drvdata(chan->mbox->dev);
	struct rockchip_mbox_chan *chans = chan->con_priv;
	u32 status;

	status = readl_relaxed(mb->mbox_base + MAILBOX_A2B_STATUS);
	return !(status & (1U << chans->idx));
}

static irqreturn_t rockchip_mbox_irq(int irq, void *dev_id)
{
	int idx;
	struct rockchip_mbox_msg *msg;
	struct rockchip_mbox *mb = (struct rockchip_mbox *)dev_id;
	u32 status = readl_relaxed(mb->mbox_base + MAILBOX_B2A_STATUS);

	for (idx = 0; idx < mb->mbox.num_chans; idx++) {
		if ((status & (1U << idx)) && irq == mb->chans[idx].irq) {
			/* Get cmd/data from the channel of B2A */
			msg = &mb->msg[idx];
			msg->cmd = readl_relaxed(mb->mbox_base +
						 MAILBOX_B2A_CMD(idx));
			msg->data = readl_relaxed(mb->mbox_base +
						  MAILBOX_B2A_DAT(idx));

			dev_dbg(mb->mbox.dev, "Chan[%d]: B2A message, cmd 0x%08x, data 0x%08x\n",
				idx, msg->cmd, msg->data);

			if (mb->mbox.chans[idx].cl)
				mbox_chan_received_data(&mb->mbox.chans[idx], msg);

			/* Clear mbox interrupt */
			writel_relaxed(1U << idx,
				       mb->mbox_base + MAILBOX_B2A_STATUS);
		}
	}

	return IRQ_HANDLED;
}

static bool rockchip_mbox_v2_is_busy(struct rockchip_mbox *mb)
{
	u32 status;

	if (mb->version < MAILBOX_VERSION_2_2_0)
		status = readl_relaxed(mb->mbox_base + mb->reg->tx_sts) & MAILBOX_V2_STATUS_TX_DONE;
	else
		status = readl_relaxed(mb->mbox_base + mb->reg->tx_lock);

	return !!status;
}

static int rockchip_mbox_v2_send_data(struct mbox_chan *chan, void *data)
{
	struct rockchip_mbox *mb = dev_get_drvdata(chan->mbox->dev);
	struct rockchip_mbox_msg *msg = data;

	if (!msg)
		return -EINVAL;

	if (rockchip_mbox_v2_is_busy(mb)) {
		dev_err(mb->mbox.dev, "The mailbox is busy\n");
		return -EBUSY;
	}

	dev_dbg(mb->mbox.dev, "TX: cmd 0x%08x, data 0x%08x\n", msg->cmd, msg->data);

	if (mb->trigger_method) {
		writel_relaxed(msg->cmd, mb->mbox_base + mb->reg->tx_cmd);
		writel_relaxed(msg->data, mb->mbox_base + mb->reg->tx_dat);
	} else {
		writel_relaxed(msg->cmd, mb->mbox_base + mb->reg->tx_cmd);
	}

	return 0;
}

static int rockchip_mbox_v2_startup(struct mbox_chan *chan)
{
	struct rockchip_mbox *mb = dev_get_drvdata(chan->mbox->dev);

	/* Set the TX interrupt trigger method */
	writel_relaxed((1U << (BIT_WRITEABLE_SHIFT + MAILBOX_V2_TRIGGER_SHIFT) |
			mb->trigger_method << MAILBOX_V2_TRIGGER_SHIFT),
			mb->mbox_base + mb->reg->tx_int);

	/* Enable the tx_done interrupt */
	writel_relaxed((1U << BIT_WRITEABLE_SHIFT | MAILBOX_V2_INTEN_TX_DONE),
			mb->mbox_base + mb->reg->rx_int);

	/* Enable the rx_done interrupt */
	if (mb->mbox.txdone_irq)
		writel_relaxed((1U << (BIT_WRITEABLE_SHIFT + MAILBOX_V2_INTEN_RX_DONE_SHIFT) |
				MAILBOX_V2_INTEN_RX_DONE),
				mb->mbox_base + mb->reg->rx_int);

	return 0;
}

static bool rockchip_mbox_v2_last_tx_done(struct mbox_chan *chan)
{
	struct rockchip_mbox *mb = dev_get_drvdata(chan->mbox->dev);
	u32 status;

	status = readl_relaxed(mb->mbox_base + mb->reg->tx_sts);
	return !(status & MAILBOX_V2_STATUS_TX_DONE);
}

static void rockchip_mbox_v2_shutdown(struct mbox_chan *chan)
{
	struct rockchip_mbox *mb = dev_get_drvdata(chan->mbox->dev);

	/* Disable the tx_done interrupt */
	writel_relaxed(1U << BIT_WRITEABLE_SHIFT, mb->mbox_base + mb->reg->rx_int);

	/* Disable the rx_done interrupt */
	if (mb->mbox.txdone_irq)
		writel_relaxed(1U << (BIT_WRITEABLE_SHIFT + MAILBOX_V2_INTEN_RX_DONE_SHIFT),
				mb->mbox_base + mb->reg->rx_int);
}

static irqreturn_t rockchip_mbox_v2_irq(int irq, void *dev_id)
{
	struct rockchip_mbox *mb = (struct rockchip_mbox *)dev_id;
	struct rockchip_mbox_msg *msg = mb->msg;
	u32 status;

	status = readl_relaxed(mb->mbox_base + mb->reg->rx_sts);
	if (!(status & MAILBOX_V2_STATUS_MASK))
		return IRQ_NONE;

	if (status & MAILBOX_V2_STATUS_TX_DONE) {
		/* Get cmd/data from the channel */
		msg->cmd = readl_relaxed(mb->mbox_base + mb->reg->rx_cmd);
		msg->data = readl_relaxed(mb->mbox_base + mb->reg->rx_dat);

		dev_dbg(mb->mbox.dev, "RX: cmd 0x%08x, data 0x%08x\n",
			msg->cmd, msg->data);

		/* Clear the tx_done interrupt */
		writel_relaxed(MAILBOX_V2_STATUS_TX_DONE,
			       mb->mbox_base + mb->reg->rx_sts);

		if (mb->mbox.chans[0].cl)
			mbox_chan_received_data(&mb->mbox.chans[0], msg);
	}

	if (status & MAILBOX_V2_STATUS_RX_DONE) {
		if (mb->mbox.txdone_irq)
			mbox_chan_txdone(&mb->mbox.chans[0], 0);

		/* Clear the rx_done interrupt */
		writel_relaxed(MAILBOX_V2_STATUS_RX_DONE,
			       mb->mbox_base + mb->reg->rx_sts);
	}

	return IRQ_HANDLED;
}

static const struct mbox_chan_ops rockchip_mbox_chan_ops = {
	.send_data	= rockchip_mbox_send_data,
	.startup	= rockchip_mbox_startup,
	.shutdown	= rockchip_mbox_shutdown,
	.last_tx_done	= rockchip_mbox_last_tx_done,
};

static const struct mbox_chan_ops rockchip_mbox_v2_chan_ops = {
	.send_data	= rockchip_mbox_v2_send_data,
	.startup	= rockchip_mbox_v2_startup,
	.shutdown	= rockchip_mbox_v2_shutdown,
	.last_tx_done	= rockchip_mbox_v2_last_tx_done,
};

int rockchip_mbox_read_msg(struct mbox_chan *chan,
			   struct rockchip_mbox_msg *msg)
{
	struct rockchip_mbox *mb;
	struct rockchip_mbox_chan *chans;

	if (!chan || !msg)
		return -EINVAL;

	mb = dev_get_drvdata(chan->mbox->dev);
	chans = chan->con_priv;

	msg->cmd  = mb->msg[chans->idx].cmd;
	msg->data = mb->msg[chans->idx].data;

	return 0;
}
EXPORT_SYMBOL_GPL(rockchip_mbox_read_msg);

static const struct rockchip_mbox_data rk3368_drv_data = {
	.num_chans = 4,
	.version = MAILBOX_VERSION_1_0_0,
	.ops = &rockchip_mbox_chan_ops,
	.irq_func = rockchip_mbox_irq,
};

static const struct rockchip_mbox_data rk3576_drv_data = {
	.num_chans = 1,
	.version = MAILBOX_VERSION_2_0_0,
	.reg_a2b = { MAILBOX_V2_A2B_INTEN, MAILBOX_V2_A2B_STATUS,
		     MAILBOX_V2_A2B_CMD, MAILBOX_V2_A2B_DAT,
		     MAILBOX_V2_A2B_LOCK,
		     MAILBOX_V2_B2A_INTEN, MAILBOX_V2_B2A_STATUS,
		     MAILBOX_V2_B2A_CMD, MAILBOX_V2_B2A_DAT,
		     MAILBOX_V2_B2A_LOCK },
	.reg_b2a = { MAILBOX_V2_B2A_INTEN, MAILBOX_V2_B2A_STATUS,
		     MAILBOX_V2_B2A_CMD, MAILBOX_V2_B2A_DAT,
		     MAILBOX_V2_B2A_LOCK,
		     MAILBOX_V2_A2B_INTEN, MAILBOX_V2_A2B_STATUS,
		     MAILBOX_V2_A2B_CMD, MAILBOX_V2_A2B_DAT,
		     MAILBOX_V2_A2B_LOCK },
	.ops = &rockchip_mbox_v2_chan_ops,
	.irq_func = rockchip_mbox_v2_irq,
};

static const struct of_device_id rockchip_mbox_of_match[] = {
	{ .compatible = "rockchip,rk3368-mailbox", .data = &rk3368_drv_data},
	{ .compatible = "rockchip,rk3576-mailbox", .data = &rk3576_drv_data},
	{ },
};
MODULE_DEVICE_TABLE(of, rockchip_mbox_of_match);

static void rockchip_mbox_get_properties(struct rockchip_mbox *mb,
					 const struct rockchip_mbox_data *drv_data)
{
	struct device *dev = mb->mbox.dev;
	u32 txpoll_period;
	int ret;

	if (device_property_present(dev, "rockchip,tx-direction-b2a"))
		mb->reg = &drv_data->reg_b2a;
	else
		mb->reg = &drv_data->reg_a2b;

	/*
	 * rockchip,txdone-ack: the mailbox client uses its own ACK to check
	 *     TX_DONE, and call mbox_client_txdone() API to schedule tx_tick.
	 * rockchip,txdone-irq: the feature only support from RK3506, the ISR
	 *     function call mbox_chan_txdone() API to schedule tx_tick.
	 * txdone_poll is default for all the platform, it cooperates with
	 *     "rockchip,txpoll-period-ms" or "rockchip,txpoll-period-us"
	 *     periodically call last_tx_done() to check TX_DONE by the hrtimer
	 *     in mailbox framework.
	 */
	if (device_property_present(dev, "rockchip,txdone-ack")) {
		mb->mbox.txdone_irq = false;
		mb->mbox.txdone_poll = false;
	} else if (device_property_present(dev, "rockchip,txdone-irq")) {
		mb->mbox.txdone_irq = true;
	} else {
		mb->mbox.txdone_poll = true;
		if (IS_REACHABLE(CONFIG_MAILBOX_POLL_PERIOD_US)) {
			ret = device_property_read_u32(dev,
						       "rockchip,txpoll-period-us",
						       &txpoll_period);
			if (!ret) {
				mb->mbox.txpoll_period = txpoll_period;
			} else {
				ret = device_property_read_u32(dev,
							       "rockchip,txpoll-period-ms",
							       &txpoll_period);
				mb->mbox.txpoll_period = !ret ? txpoll_period : MAILBOX_POLLING_MS;
				mb->mbox.txpoll_period *= 1000U; /* Convert to us */
			}
		} else {
			ret = device_property_read_u32(dev,
						       "rockchip,txpoll-period-ms",
						       &txpoll_period);
			mb->mbox.txpoll_period = !ret ? txpoll_period : MAILBOX_POLLING_MS;
		}
	}

	if (device_property_present(dev, "rockchip,enable-cmd-trigger"))
		mb->trigger_method = 0;
	else
		mb->trigger_method = 1;

	if (drv_data->version != MAILBOX_VERSION_1_0_0)
		mb->version = FIELD_GET(MAILBOX_V2_VERSION_MASK,
					readl_relaxed(mb->mbox_base + MAILBOX_V2_VERSION));

	if (!mb->version)
		mb->version = drv_data->version;
}

static int rockchip_mbox_probe(struct platform_device *pdev)
{
	struct rockchip_mbox *mb;
	const struct of_device_id *match;
	const struct rockchip_mbox_data *drv_data;
	struct resource *res;
	int ret, irq, i;

	if (!pdev->dev.of_node)
		return -ENODEV;

	match = of_match_node(rockchip_mbox_of_match, pdev->dev.of_node);
	drv_data = (const struct rockchip_mbox_data *)match->data;

	mb = devm_kzalloc(&pdev->dev, sizeof(*mb), GFP_KERNEL);
	if (!mb)
		return -ENOMEM;

	mb->msg = devm_kcalloc(&pdev->dev, drv_data->num_chans,
			       sizeof(*mb->msg), GFP_KERNEL);
	if (!mb->msg)
		return -ENOMEM;

	mb->chans = devm_kcalloc(&pdev->dev, drv_data->num_chans,
				 sizeof(*mb->chans), GFP_KERNEL);
	if (!mb->chans)
		return -ENOMEM;

	mb->mbox.chans = devm_kcalloc(&pdev->dev, drv_data->num_chans,
				      sizeof(*mb->mbox.chans), GFP_KERNEL);
	if (!mb->mbox.chans)
		return -ENOMEM;

	platform_set_drvdata(pdev, mb);

	mb->mbox.dev = &pdev->dev;
	mb->mbox.num_chans = drv_data->num_chans;
	mb->mbox.ops = drv_data->ops;
	spin_lock_init(&mb->cfg_lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	mb->mbox_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mb->mbox_base))
		return PTR_ERR(mb->mbox_base);

	rockchip_mbox_get_properties(mb, drv_data);

	mb->pclk = devm_clk_get(&pdev->dev, "pclk_mailbox");
	if (IS_ERR(mb->pclk)) {
		ret = PTR_ERR(mb->pclk);
		dev_err(&pdev->dev, "failed to get pclk_mailbox clock: %d\n",
			ret);
		return ret;
	}

	ret = clk_prepare_enable(mb->pclk);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable pclk: %d\n", ret);
		return ret;
	}

	for (i = 0; i < mb->mbox.num_chans; i++) {
		irq = platform_get_irq(pdev, i);
		if (irq < 0) {
			/* For shared irq case, only could be got one time */
			if (i > 0 && irq == -ENXIO) {
				mb->chans[i].irq = mb->chans[0].irq;
			} else {
				ret = irq;
				goto disable_clk;
			}
		} else {
			mb->chans[i].irq = irq;
		}

		mb->chans[i].idx = i;
		mb->mbox.chans[i].con_priv = &mb->chans[i];
	}

	ret = devm_mbox_controller_register(&pdev->dev, &mb->mbox);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register mailbox: %d\n", ret);
		goto disable_clk;
	}

	for (i = 0; i < mb->mbox.num_chans; i++) {
		/* For shared irq case, only request irq thread one time */
		if (i > 0 && mb->chans[i].irq == mb->chans[0].irq)
			break;

		ret = devm_request_threaded_irq(&pdev->dev, mb->chans[i].irq,
						NULL,
						drv_data->irq_func,
						IRQF_ONESHOT,
						dev_name(&pdev->dev),
						mb);
		if (ret < 0)
			goto disable_clk;

		if (device_property_present(&pdev->dev, "wakeup-source"))
			enable_irq_wake(mb->chans[i].irq);
	}

	dev_info(&pdev->dev, "version: 0x%04x, tx_dir: %s, tx_done: %s, poll_period: %u, fast_mode: %s\n",
		 mb->version,
		 mb->reg == &drv_data->reg_b2a ? "B2A" : "A2B",
		 mb->mbox.txdone_irq ? "irq" : mb->mbox.txdone_poll ? "poll" : "ack",
		 mb->mbox.txpoll_period,
		 str_yes_no(!mb->trigger_method));

	return 0;

disable_clk:
	clk_disable_unprepare(mb->pclk);
	return ret;
}

static struct platform_driver rockchip_mbox_driver = {
	.probe	= rockchip_mbox_probe,
	.driver = {
		.name = "rockchip-mailbox",
		.of_match_table = of_match_ptr(rockchip_mbox_of_match),
	},
};

#if defined(CONFIG_ROCKCHIP_THUNDER_BOOT)
static int __init rockchip_mbox_driver_init(void)
{
	return platform_driver_register(&rockchip_mbox_driver);
}
core_initcall(rockchip_mbox_driver_init);
#else
module_platform_driver(rockchip_mbox_driver);
#endif

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Rockchip mailbox: communicate between CPU cores and MCU");
MODULE_AUTHOR("Addy Ke <addy.ke@rock-chips.com>");
MODULE_AUTHOR("Caesar Wang <wxt@rock-chips.com>");
