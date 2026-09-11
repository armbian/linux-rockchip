// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *
 *  Bluetooth HCI UART driver
 *
 *  Copyright (C) 2000-2001  Qualcomm Incorporated
 *  Copyright (C) 2002-2003  Maxim Krasnyansky <maxk@qualcomm.com>
 *  Copyright (C) 2004-2005  Marcel Holtmann <marcel@holtmann.org>
 */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/poll.h>

#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/signal.h>
#include <linux/ioctl.h>
#include <linux/skbuff.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/serdev.h>
#include <asm/unaligned.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include "hci_uart.h"

struct h4_struct {
	struct sk_buff *rx_skb;
	struct sk_buff_head txq;
};

/* Initialize protocol */
static int h4_open(struct hci_uart *hu)
{
	struct h4_struct *h4;

	BT_DBG("hu %p", hu);

	h4 = kzalloc(sizeof(*h4), GFP_KERNEL);
	if (!h4)
		return -ENOMEM;

	skb_queue_head_init(&h4->txq);

	hu->priv = h4;

	/* on the tty path this is hciattach's business */
	if (hu->serdev)
		serdev_device_set_flow_control(hu->serdev, true);

	return 0;
}

/* Flush protocol data */
static int h4_flush(struct hci_uart *hu)
{
	struct h4_struct *h4 = hu->priv;

	BT_DBG("hu %p", hu);

	skb_queue_purge(&h4->txq);

	return 0;
}

/* Close protocol */
static int h4_close(struct hci_uart *hu)
{
	struct h4_struct *h4 = hu->priv;

	BT_DBG("hu %p", hu);

	skb_queue_purge(&h4->txq);

	kfree_skb(h4->rx_skb);

	hu->priv = NULL;
	kfree(h4);

	return 0;
}

/* Enqueue frame for transmission (padding, crc, etc) */
static int h4_enqueue(struct hci_uart *hu, struct sk_buff *skb)
{
	struct h4_struct *h4 = hu->priv;

	BT_DBG("hu %p skb %p", hu, skb);

	/* Prepend skb with frame type */
	memcpy(skb_push(skb, 1), &hci_skb_pkt_type(skb), 1);
	skb_queue_tail(&h4->txq, skb);

	return 0;
}

static const struct h4_recv_pkt h4_recv_pkts[] = {
	{ H4_RECV_ACL,   .recv = hci_recv_frame },
	{ H4_RECV_SCO,   .recv = hci_recv_frame },
	{ H4_RECV_EVENT, .recv = hci_recv_frame },
	{ H4_RECV_ISO,   .recv = hci_recv_frame },
};

/* Recv data */
static int h4_recv(struct hci_uart *hu, const void *data, int count)
{
	struct h4_struct *h4 = hu->priv;

	if (!test_bit(HCI_UART_REGISTERED, &hu->flags))
		return -EUNATCH;

	h4->rx_skb = h4_recv_buf(hu->hdev, h4->rx_skb, data, count,
				 h4_recv_pkts, ARRAY_SIZE(h4_recv_pkts));
	if (IS_ERR(h4->rx_skb)) {
		int err = PTR_ERR(h4->rx_skb);
		bt_dev_err(hu->hdev, "Frame reassembly failed (%d)", err);
		h4->rx_skb = NULL;
		return err;
	}

	return count;
}

static struct sk_buff *h4_dequeue(struct hci_uart *hu)
{
	struct h4_struct *h4 = hu->priv;
	return skb_dequeue(&h4->txq);
}

static const struct hci_uart_proto h4p = {
	.id		= HCI_UART_H4,
	.name		= "H4",
	.open		= h4_open,
	.close		= h4_close,
	.recv		= h4_recv,
	.enqueue	= h4_enqueue,
	.dequeue	= h4_dequeue,
	.flush		= h4_flush,
};

#ifdef CONFIG_BT_HCIUART_SERDEV

/* the controller ignores commands until it has answered one */
#define H4_QUIRK_SYNC_RESET	BIT(0)

struct h4_serdev {
	struct hci_uart hu;
	unsigned long quirks;
};

static int h4_serdev_setup(struct hci_uart *hu)
{
	struct h4_serdev *h4 = serdev_device_get_drvdata(hu->serdev);
	struct sk_buff *skb;
	int i;

	if (!(h4->quirks & H4_QUIRK_SYNC_RESET))
		return 0;

	bt_dev_info(hu->hdev, "syncing controller, a command timeout here is expected");

	for (i = 0; i < 3; i++) {
		skb = __hci_cmd_sync(hu->hdev, HCI_OP_RESET, 0, NULL, HCI_CMD_TIMEOUT);
		if (!IS_ERR(skb)) {
			kfree_skb(skb);
			return 0;
		}
	}

	bt_dev_err(hu->hdev, "no answer to HCI Reset");

	return PTR_ERR(skb);
}

/* not h4p: a ->setup there would suppress the tty path's vendor detect */
static const struct hci_uart_proto h4_serdev_proto = {
	.id		= HCI_UART_H4,
	.name		= "H4",
	.open		= h4_open,
	.close		= h4_close,
	.recv		= h4_recv,
	.enqueue	= h4_enqueue,
	.dequeue	= h4_dequeue,
	.flush		= h4_flush,
	.setup		= h4_serdev_setup,
};

static int h4_serdev_probe(struct serdev_device *serdev)
{
	struct h4_serdev *h4;
	u32 speed = 0;

	h4 = devm_kzalloc(&serdev->dev, sizeof(*h4), GFP_KERNEL);
	if (!h4)
		return -ENOMEM;

	h4->hu.serdev = serdev;
	h4->quirks = (unsigned long)device_get_match_data(&serdev->dev);
	serdev_device_set_drvdata(serdev, h4);

	device_property_read_u32(&serdev->dev, "max-speed", &speed);
	hci_uart_set_speeds(&h4->hu, speed, speed);

	return hci_uart_register_device(&h4->hu, &h4_serdev_proto);
}

static void h4_serdev_remove(struct serdev_device *serdev)
{
	struct h4_serdev *h4 = serdev_device_get_drvdata(serdev);

	hci_uart_unregister_device(&h4->hu);
}

static const struct of_device_id h4_serdev_of_match[] = {
	{ .compatible = "aicsemi,aic8800-bt",
	  .data = (void *)H4_QUIRK_SYNC_RESET },
	{ }
};
MODULE_DEVICE_TABLE(of, h4_serdev_of_match);

static struct serdev_device_driver h4_serdev_driver = {
	.probe	= h4_serdev_probe,
	.remove	= h4_serdev_remove,
	.driver = {
		.name		= "hci_uart_h4",
		.of_match_table	= h4_serdev_of_match,
	},
};

static void h4_serdev_register(void)
{
	serdev_device_driver_register(&h4_serdev_driver);
}

static void h4_serdev_unregister(void)
{
	serdev_device_driver_unregister(&h4_serdev_driver);
}

#else

static inline void h4_serdev_register(void) { }
static inline void h4_serdev_unregister(void) { }

#endif

int __init h4_init(void)
{
	h4_serdev_register();

	return hci_uart_register_proto(&h4p);
}

int __exit h4_deinit(void)
{
	h4_serdev_unregister();

	return hci_uart_unregister_proto(&h4p);
}

struct sk_buff *h4_recv_buf(struct hci_dev *hdev, struct sk_buff *skb,
			    const unsigned char *buffer, int count,
			    const struct h4_recv_pkt *pkts, int pkts_count)
{
	struct hci_uart *hu = hci_get_drvdata(hdev);
	u8 alignment = hu->alignment ? hu->alignment : 1;

	/* Check for error from previous call */
	if (IS_ERR(skb))
		skb = NULL;

	while (count) {
		int i, len;

		/* remove padding bytes from buffer */
		for (; hu->padding && count > 0; hu->padding--) {
			count--;
			buffer++;
		}
		if (!count)
			break;

		if (!skb) {
			for (i = 0; i < pkts_count; i++) {
				if (buffer[0] != (&pkts[i])->type)
					continue;

				skb = bt_skb_alloc((&pkts[i])->maxlen,
						   GFP_ATOMIC);
				if (!skb)
					return ERR_PTR(-ENOMEM);

				hci_skb_pkt_type(skb) = (&pkts[i])->type;
				hci_skb_expect(skb) = (&pkts[i])->hlen;
				break;
			}

			/* Check for invalid packet type */
			if (!skb)
				return ERR_PTR(-EILSEQ);

			count -= 1;
			buffer += 1;
		}

		len = min_t(uint, hci_skb_expect(skb) - skb->len, count);
		skb_put_data(skb, buffer, len);

		count -= len;
		buffer += len;

		/* Check for partial packet */
		if (skb->len < hci_skb_expect(skb))
			continue;

		for (i = 0; i < pkts_count; i++) {
			if (hci_skb_pkt_type(skb) == (&pkts[i])->type)
				break;
		}

		if (i >= pkts_count) {
			kfree_skb(skb);
			return ERR_PTR(-EILSEQ);
		}

		if (skb->len == (&pkts[i])->hlen) {
			u16 dlen;

			switch ((&pkts[i])->lsize) {
			case 0:
				/* No variable data length */
				dlen = 0;
				break;
			case 1:
				/* Single octet variable length */
				dlen = skb->data[(&pkts[i])->loff];
				hci_skb_expect(skb) += dlen;

				if (skb_tailroom(skb) < dlen) {
					kfree_skb(skb);
					return ERR_PTR(-EMSGSIZE);
				}
				break;
			case 2:
				/* Double octet variable length */
				dlen = get_unaligned_le16(skb->data +
							  (&pkts[i])->loff);
				hci_skb_expect(skb) += dlen;

				if (skb_tailroom(skb) < dlen) {
					kfree_skb(skb);
					return ERR_PTR(-EMSGSIZE);
				}
				break;
			default:
				/* Unsupported variable length */
				kfree_skb(skb);
				return ERR_PTR(-EILSEQ);
			}

			if (!dlen) {
				hu->padding = (skb->len + 1) % alignment;
				hu->padding = (alignment - hu->padding) % alignment;

				/* No more data, complete frame */
				(&pkts[i])->recv(hdev, skb);
				skb = NULL;
			}
		} else {
			hu->padding = (skb->len + 1) % alignment;
			hu->padding = (alignment - hu->padding) % alignment;

			/* Complete frame */
			(&pkts[i])->recv(hdev, skb);
			skb = NULL;
		}
	}

	return skb;
}
EXPORT_SYMBOL_GPL(h4_recv_buf);
