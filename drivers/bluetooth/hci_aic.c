// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Bluetooth HCI UART driver for aic8800 devices
 *
 *  Copyright (c) 2026 FriendlyElec Computer Tech. Co., Ltd.
 *  (http://www.friendlyelec.com)
 *
 * Based on hci_mrvl.c
 *
 *  Copyright (C) 2016  Marvell International Ltd.
 *  Copyright (C) 2016  Intel Corporation
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/skbuff.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/of.h>
#include <linux/serdev.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include "hci_uart.h"

struct aic_serdev {
	struct hci_uart hu;

	struct sk_buff *rx_skb;
	struct sk_buff_head txq;
};

static int aic_open(struct hci_uart *hu)
{
	struct aic_serdev *aic = hu->priv;

	BT_DBG("hu %p", hu);

	if (!hu->serdev || !hci_uart_has_flow_control(hu))
		return -EOPNOTSUPP;

	skb_queue_head_init(&aic->txq);

	return 0;
}

static int aic_close(struct hci_uart *hu)
{
	struct aic_serdev *aic = hu->priv;

	BT_DBG("hu %p", hu);

	skb_queue_purge(&aic->txq);
	kfree_skb(aic->rx_skb);

	return 0;
}

static int aic_flush(struct hci_uart *hu)
{
	struct aic_serdev *aic = hu->priv;

	BT_DBG("hu %p", hu);

	skb_queue_purge(&aic->txq);

	return 0;
}

static int aic_enqueue(struct hci_uart *hu, struct sk_buff *skb)
{
	struct aic_serdev *aic = hu->priv;

	BT_DBG("hu %p skb %p", hu, skb);

	/* Prepend skb with frame type */
	memcpy(skb_push(skb, 1), &hci_skb_pkt_type(skb), 1);
	skb_queue_tail(&aic->txq, skb);

	return 0;
}

static struct sk_buff *aic_dequeue(struct hci_uart *hu)
{
	struct aic_serdev *aic = hu->priv;

	return skb_dequeue(&aic->txq);
}

static const struct h4_recv_pkt aic_recv_pkts[] = {
	{ H4_RECV_ACL,   .recv = hci_recv_frame },
	{ H4_RECV_SCO,   .recv = hci_recv_frame },
	{ H4_RECV_EVENT, .recv = hci_recv_frame },
	{ H4_RECV_ISO,   .recv = hci_recv_frame },
};

static int aic_recv(struct hci_uart *hu, const void *data, int count)
{
	struct aic_serdev *aic = hu->priv;

	if (!test_bit(HCI_UART_REGISTERED, &hu->flags))
		return -EUNATCH;

	aic->rx_skb = h4_recv_buf(hu->hdev, aic->rx_skb, data, count,
				  aic_recv_pkts,
				  ARRAY_SIZE(aic_recv_pkts));
	if (IS_ERR(aic->rx_skb)) {
		int err = PTR_ERR(aic->rx_skb);

		bt_dev_err(hu->hdev, "Frame reassembly failed (%d)", err);
		aic->rx_skb = NULL;
		return err;
	}

	return count;
}

static int aic_setup(struct hci_uart *hu)
{
	int err;

	/* The firmware might be loaded by the Wifi driver over SDIO. We wait
	 * up to 10s for the CTS to go up. Afterward, we know that the firmware
	 * is ready.
	 */
	err = serdev_device_wait_for_cts(hu->serdev, true, 10000);
	if (err) {
		bt_dev_err(hu->hdev, "Wait for CTS failed with %d\n", err);
		return err;
	}

	return 0;
}

static const struct hci_uart_proto aic_proto = {
	.id		= HCI_UART_AIC,
	.name		= "AIC (H4)",
	.init_speed	= 1500000,
	.open		= aic_open,
	.close		= aic_close,
	.flush		= aic_flush,
	.setup		= aic_setup,
	.recv		= aic_recv,
	.enqueue	= aic_enqueue,
	.dequeue	= aic_dequeue,
};

static int aic_serdev_probe(struct serdev_device *serdev)
{
	struct device *dev = &serdev->dev;
	struct aic_serdev *aicdev;

	aicdev = devm_kzalloc(dev, sizeof(*aicdev), GFP_KERNEL);
	if (!aicdev)
		return -ENOMEM;

	aicdev->hu.priv = aicdev;
	aicdev->hu.serdev = serdev;
	serdev_device_set_drvdata(serdev, aicdev);

	return hci_uart_register_device(&aicdev->hu, &aic_proto);
}

static void aic_serdev_remove(struct serdev_device *serdev)
{
	struct aic_serdev *aicdev = serdev_device_get_drvdata(serdev);

	hci_uart_unregister_device(&aicdev->hu);
}

#ifdef CONFIG_OF
static const struct of_device_id aic_bluetooth_of_match[] = {
	{ .compatible = "cvte,aic8800-bt" },
	{ },
};
MODULE_DEVICE_TABLE(of, aic_bluetooth_of_match);
#endif

static struct serdev_device_driver aic_serdev_driver = {
	.probe = aic_serdev_probe,
	.remove = aic_serdev_remove,
	.driver = {
		.name = "hci_uart_aic",
		.of_match_table = of_match_ptr(aic_bluetooth_of_match),
	},
};

int __init aic_init(void)
{
	serdev_device_driver_register(&aic_serdev_driver);

	return hci_uart_register_proto(&aic_proto);
}

int __exit aic_deinit(void)
{
	serdev_device_driver_unregister(&aic_serdev_driver);

	return hci_uart_unregister_proto(&aic_proto);
}
