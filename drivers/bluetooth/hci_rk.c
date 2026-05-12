// SPDX-License-Identifier: GPL-2.0-only
/*
 * Bluetooth HCI UART transport driver for Rockchip RK960/RK962 series
 *
 * Copyright (C) 2026 Rockchip Electronics Co., Ltd.
 *
 * Transport layer only; chip logic lives in btrk.c / btrk.h.
 */

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/firmware.h>
#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/property.h>
#include <linux/serdev.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/unaligned.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include "hci_uart.h"
#include "btrk.h"


/* RKUART bootloader protocol constants */
#define RKUART_READY_STR	"RKUART"
#define RKUART_PKG_SIZE		1024
#define RKUART_BOOT_CMD		0x0471

static const u8 RKUART_REQ_TAG[4] = { 'R', 'E', 'Q', ' ' };
static const u8 RKUART_ACK_TAG[4] = { 'A', 'C', 'K', ' ' };

/* Driver state machine */
enum rk_phase {
	RK_PHASE_RKUART_READY,
	RK_PHASE_RKUART_ACK,
	RK_PHASE_RKUART_DATA,
	RK_PHASE_HCI,
};

struct rk_data {
	struct sk_buff		*rx_skb;
	struct sk_buff_head	txq;
	enum rk_phase		phase;
	u8	boot_rx[32];
	int	boot_rx_len;
	struct completion boot_comp;
	int	boot_comp_status;
};

struct rk_serdev {
	struct hci_uart		serdev_hu;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*shutdown_gpio;
	u32			init_speed;
	u32			oper_speed;
	bool			flow_ctrl;
	bool			lpm_enable;
	u8			sco_path;
	u16			voice_setting;
	struct btrk_dev		bdev;
};

struct rk_setup_ctx {
	struct rk_serdev	*rkdev;
	struct rk_data		*rk;
};

/* RKUART bootloader protocol structures */
struct rk_boot_req {
	__le32 tag;
	__le16 cmd;
	__le32 data_length;
	__le16 package_size;
	__le16 timeout;
	__le32 load_addr;
	__le32 data_hash;
	u8     reserved[6];
	__le32 req_hash;
} __packed;

struct rk_boot_resp {
	__le32 tag;
	u8     reserved[6];
	u8     cause;
	u8     status;
} __packed;

static struct gpio_desc *rk_control_gpio(struct rk_serdev *rkdev)
{
	if (rkdev->reset_gpio)
		return rkdev->reset_gpio;

	return rkdev->shutdown_gpio;
}

static void rk_assert_reset(struct rk_serdev *rkdev)
{
	struct gpio_desc *gpio = rk_control_gpio(rkdev);

	if (gpio)
		gpiod_set_value_cansleep(gpio, 0);
}

static void rk_deassert_reset(struct rk_serdev *rkdev)
{
	struct gpio_desc *gpio = rk_control_gpio(rkdev);

	if (gpio) {
		gpiod_set_value_cansleep(gpio, 1);
		msleep(150);
	}
}

static void rk_power_on(struct rk_serdev *rkdev)
{
	if (rkdev->shutdown_gpio) {
		gpiod_set_value_cansleep(rkdev->shutdown_gpio, 1);
		usleep_range(20000, 25000);
	}
}

static void rk_power_off(struct rk_serdev *rkdev)
{
	if (rkdev->shutdown_gpio)
		gpiod_set_value_cansleep(rkdev->shutdown_gpio, 0);
	if (rkdev->reset_gpio)
		gpiod_set_value_cansleep(rkdev->reset_gpio, 0);
}

static int rk_shutdown(struct hci_dev *hdev)
{
	struct hci_uart *hu = hci_get_drvdata(hdev);
	struct rk_serdev *rkdev = serdev_device_get_drvdata(hu->serdev);

	rk_power_off(rkdev);
	return 0;
}

/* RKUART boot protocol - receive side */
static bool rk_has_ready_token(const u8 *buf, size_t len)
{
	size_t token_len = sizeof(RKUART_READY_STR) - 1;
	size_t offset;

	if (len < token_len)
		return false;
	for (offset = 0; offset <= len - token_len; offset++) {
		if (!memcmp(buf + offset, RKUART_READY_STR, token_len))
			return true;
	}
	return false;
}

static void rk_boot_rx_handle(struct rk_data *rk, const u8 *data, int count)
{
	int avail = (int)sizeof(rk->boot_rx) - rk->boot_rx_len;

	if (count > avail) {
		rk->boot_rx_len = 0;
		avail = (int)sizeof(rk->boot_rx);
	}
	count = min(count, avail);
	if (count <= 0)
		return;

	memcpy(rk->boot_rx + rk->boot_rx_len, data, count);
	rk->boot_rx_len += count;

	switch (rk->phase) {
	case RK_PHASE_RKUART_READY:
		if (rk_has_ready_token(rk->boot_rx, rk->boot_rx_len)) {
			rk->boot_rx_len = 0;
			if (!completion_done(&rk->boot_comp)) {
				rk->boot_comp_status = 0;
				complete(&rk->boot_comp);
			}
		}
		break;
	case RK_PHASE_RKUART_ACK:
	case RK_PHASE_RKUART_DATA:
		if (rk->boot_rx_len >= (int)sizeof(struct rk_boot_resp)) {
			struct rk_boot_resp *r = (struct rk_boot_resp *)rk->boot_rx;

			rk->boot_rx_len = 0;
			rk->boot_comp_status =
				(memcmp(&r->tag, RKUART_ACK_TAG, 4) == 0 &&
				 r->status == 0) ? 0 : -EIO;
			complete(&rk->boot_comp);
		}
		break;
	default:
		break;
	}
}

/* RKUART boot protocol - transmit side */
static int rk_serdev_write_all(struct serdev_device *serdev,
			       const u8 *buf, size_t len)
{
	while (len > 0) {
		int w = serdev_device_write_buf(serdev, buf, len);

		if (w < 0)
			return w;
		if (w == 0)
			return -EIO;
		buf += w;
		len -= w;
	}
	return 0;
}

static int rk_boot_upload(struct hci_uart *hu, struct rk_data *rk,
			  const struct firmware *fw, u32 load_addr)
{
	struct hci_dev *hdev = hu->hdev;
	struct rk_boot_req req;
	const u8 *p;
	u32 size, hash;
	unsigned long timeout;
	int ret;

	bt_dev_info(hdev, "RK: uploading bootloader (%zu bytes, load_addr=0x%08x)",
		    fw->size, load_addr);

	hash = 0;
	p    = fw->data;
	size = fw->size;
	while (size > 0) {
		u32 chunk = min_t(u32, size, RKUART_PKG_SIZE);

		hash = btrk_js_hash(hash, p, chunk);
		p    += chunk;
		size -= chunk;
	}

	memset(&req, 0, sizeof(req));
	memcpy(&req.tag, RKUART_REQ_TAG, 4);
	req.cmd          = cpu_to_le16(RKUART_BOOT_CMD);
	req.data_length  = cpu_to_le32(fw->size);
	req.package_size = cpu_to_le16(RKUART_PKG_SIZE);
	req.timeout      = cpu_to_le16(5000);
	req.load_addr    = cpu_to_le32(load_addr);
	req.data_hash    = cpu_to_le32(hash);
	req.req_hash     = cpu_to_le32(btrk_js_hash(0, (const u8 *)&req,
				       sizeof(req) - 4));

	timeout = msecs_to_jiffies(5000);
	if (!wait_for_completion_timeout(&rk->boot_comp, timeout)) {
		bt_dev_err(hdev, "RK: timeout waiting for RKUART ready");
		return -ETIMEDOUT;
	}

	reinit_completion(&rk->boot_comp);
	rk->phase = RK_PHASE_RKUART_ACK;
	ret = rk_serdev_write_all(hu->serdev, (const u8 *)&req, sizeof(req));
	if (ret) {
		bt_dev_err(hdev, "RK: failed to send REQ header (%d)", ret);
		return ret;
	}

	timeout = msecs_to_jiffies(5000);
	if (!wait_for_completion_timeout(&rk->boot_comp, timeout)) {
		bt_dev_err(hdev, "RK: timeout waiting for REQ ACK");
		return -ETIMEDOUT;
	}
	if (rk->boot_comp_status) {
		bt_dev_err(hdev, "RK: bad ACK for REQ header");
		return -EIO;
	}

	p    = fw->data;
	size = fw->size;
	rk->phase = RK_PHASE_RKUART_DATA;

	while (size > 0) {
		u32 chunk = min_t(u32, size, RKUART_PKG_SIZE);

		reinit_completion(&rk->boot_comp);
		ret = rk_serdev_write_all(hu->serdev, p, chunk);
		if (ret) {
			bt_dev_err(hdev, "RK: fw chunk write error (%d)", ret);
			return ret;
		}

		timeout = msecs_to_jiffies(2000);
		if (!wait_for_completion_timeout(&rk->boot_comp, timeout)) {
			bt_dev_err(hdev, "RK: timeout waiting for chunk ACK");
			return -ETIMEDOUT;
		}
		if (rk->boot_comp_status) {
			bt_dev_err(hdev, "RK: bad ACK for fw chunk");
			return -EIO;
		}

		p    += chunk;
		size -= chunk;
	}

	bt_dev_info(hdev, "RK: bootloader upload OK (%zu bytes)", fw->size);
	return 0;
}

/* UART transport helpers */
static int rk_set_speed(struct hci_uart *hu, u16 opcode, u32 speed)
{
	int ret;

	serdev_device_set_baudrate(hu->serdev, 115200);
	ret = btrk_send_speed_cmd(hu->hdev, opcode, speed);
	if (ret)
		return ret;
	serdev_device_set_baudrate(hu->serdev, speed);
	return 0;
}

/* hci_uart_proto callbacks */
static int rk_open(struct hci_uart *hu)
{
	struct rk_data *rk;

	rk = kzalloc(sizeof(*rk), GFP_KERNEL);
	if (!rk)
		return -ENOMEM;

	skb_queue_head_init(&rk->txq);
	init_completion(&rk->boot_comp);
	rk->phase = RK_PHASE_HCI;
	hu->priv = rk;
	return 0;
}

static int rk_close(struct hci_uart *hu)
{
	struct rk_serdev *rkdev = serdev_device_get_drvdata(hu->serdev);
	struct rk_data *rk = hu->priv;

	skb_queue_purge(&rk->txq);
	kfree_skb(rk->rx_skb);
	kfree(rk);
	hu->priv = NULL;
	rk_power_off(rkdev);
	return 0;
}

static int rk_flush(struct hci_uart *hu)
{
	struct rk_data *rk = hu->priv;

	skb_queue_purge(&rk->txq);
	return 0;
}

static const struct h4_recv_pkt rk_recv_pkts[] = {
	{ H4_RECV_ACL,   .recv = hci_recv_frame },
	{ H4_RECV_SCO,   .recv = hci_recv_frame },
	{ H4_RECV_EVENT, .recv = hci_recv_frame },
	{ H4_RECV_ISO,   .recv = hci_recv_frame },
};

static int rk_recv(struct hci_uart *hu, const void *data, int count)
{
	struct rk_data *rk = hu->priv;

	if (rk->phase != RK_PHASE_HCI) {
		rk_boot_rx_handle(rk, data, count);
		return count;
	}

	rk->rx_skb = h4_recv_buf(hu->hdev, rk->rx_skb, data, count,
				 rk_recv_pkts, ARRAY_SIZE(rk_recv_pkts));
	if (IS_ERR(rk->rx_skb)) {
		int err = PTR_ERR(rk->rx_skb);

		bt_dev_err(hu->hdev, "RK: frame reassembly failed (%d)", err);
		rk->rx_skb = NULL;
		return err;
	}
	return count;
}

static int rk_enqueue(struct hci_uart *hu, struct sk_buff *skb)
{
	struct rk_data *rk = hu->priv;

	memcpy(skb_push(skb, 1), &hci_skb_pkt_type(skb), 1);
	skb_queue_tail(&rk->txq, skb);
	return 0;
}

static struct sk_buff *rk_dequeue(struct hci_uart *hu)
{
	struct rk_data *rk = hu->priv;

	return skb_dequeue(&rk->txq);
}

/* Setup phases */
static int rk_setup_power_reset(struct hci_uart *hu,
				struct rk_serdev *rkdev,
				struct rk_data *rk)
{
	struct hci_dev *hdev = hu->hdev;

	bt_dev_info(hdev, "RK: init flow_ctrl=%d lpm=%d oper_speed=%u",
		    rkdev->flow_ctrl, rkdev->lpm_enable, rkdev->oper_speed);

	rk_assert_reset(rkdev);
	rk_power_on(rkdev);

	serdev_device_set_baudrate(hu->serdev, rkdev->init_speed);
	serdev_device_set_flow_control(hu->serdev, false);
	serdev_device_set_rts(hu->serdev, true);

	reinit_completion(&rk->boot_comp);
	rk->phase = RK_PHASE_RKUART_READY;
	rk->boot_rx_len = 0;

	rk_deassert_reset(rkdev);
	bt_dev_info(hdev, "RK: chip released, listening for RKUART");
	return 0;
}

static int rk_setup_bootloader(struct hci_uart *hu,
			       struct rk_serdev *rkdev,
			       struct rk_data *rk)
{
	struct hci_dev *hdev = hu->hdev;
	const struct firmware *fw = NULL;
	u32 load_addr;
	int ret;

	ret = request_firmware(&fw, BTRK_FW_LOADER, &hdev->dev);
	if (ret) {
		bt_dev_err(hdev, "RK: failed to load bootloader (%d)", ret);
		return ret;
	}

	bt_dev_info(hdev, "RK: chip_rev=%d", rkdev->bdev.loader_type);

	load_addr = rkdev->bdev.loader_load_addr;

	ret = rk_boot_upload(hu, rk, fw, load_addr);
	release_firmware(fw);
	return ret;
}

static int rk_btrk_power_reset(struct hci_uart *hu, void *ctx)
{
	struct rk_setup_ctx *setup = ctx;

	return rk_setup_power_reset(hu, setup->rkdev, setup->rk);
}

static int rk_btrk_setup_bootloader(struct hci_uart *hu, struct btrk_dev *bdev,
				    void *ctx)
{
	struct rk_setup_ctx *setup = ctx;

	setup->rkdev->bdev = *bdev;
	return rk_setup_bootloader(hu, setup->rkdev, setup->rk);
}

static void rk_btrk_set_hci_ready(void *ctx)
{
	struct rk_setup_ctx *setup = ctx;

	setup->rk->phase = RK_PHASE_HCI;
}

static int rk_btrk_set_speed(struct hci_uart *hu, u16 opcode, u32 speed,
			     void *ctx)
{
	(void)ctx;
	return rk_set_speed(hu, opcode, speed);
}

static void rk_btrk_set_baudrate(struct hci_uart *hu, u32 speed, void *ctx)
{
	(void)ctx;
	serdev_device_set_baudrate(hu->serdev, speed);
}

static void rk_btrk_set_flow_control(struct hci_uart *hu, bool enabled,
				     void *ctx)
{
	(void)ctx;
	hci_uart_set_flow_control(hu, enabled);
}

static void rk_btrk_power_off(void *ctx)
{
	struct rk_setup_ctx *setup = ctx;

	rk_power_off(setup->rkdev);
}

static const struct btrk_setup_ops rk_btrk_setup_ops = {
	.power_reset = rk_btrk_power_reset,
	.setup_bootloader = rk_btrk_setup_bootloader,
	.set_hci_ready = rk_btrk_set_hci_ready,
	.set_speed = rk_btrk_set_speed,
	.set_baudrate = rk_btrk_set_baudrate,
	.set_flow_control = rk_btrk_set_flow_control,
	.power_off = rk_btrk_power_off,
};

static int rk_setup(struct hci_uart *hu)
{
	struct rk_serdev *rkdev = serdev_device_get_drvdata(hu->serdev);
	struct rk_data *rk = hu->priv;
	struct rk_setup_ctx setup = {
		.rkdev = rkdev,
		.rk = rk,
	};
	struct btrk_setup_info info = {
		.oper_speed = rkdev->oper_speed,
		.flow_ctrl = rkdev->flow_ctrl,
		.lpm_enable = rkdev->lpm_enable,
		.sco_path = rkdev->sco_path,
		.voice_setting = rkdev->voice_setting,
	};

	return btrk_setup(hu, &rkdev->bdev, &info, &rk_btrk_setup_ops, &setup);
}

/* hci_uart_proto and serdev registration */
static const struct hci_uart_proto rk_proto = {
	.id           = HCI_UART_RK96X,
	.name         = "RK96X",
	.manufacturer = 1737,
	.init_speed   = 115200,
	.open         = rk_open,
	.close        = rk_close,
	.flush        = rk_flush,
	.setup        = rk_setup,
	.recv         = rk_recv,
	.enqueue      = rk_enqueue,
	.dequeue      = rk_dequeue,
};

struct rk_family_data {
	bool is_rk962;
};

static const struct rk_family_data rk960_family_data = { .is_rk962 = false };
static const struct rk_family_data rk962_family_data = { .is_rk962 = true  };

static int rk_serdev_probe(struct serdev_device *serdev)
{
	const struct rk_family_data *data;
	struct hci_dev *hdev;
	struct rk_serdev *rkdev;
	int ret;

	rkdev = devm_kzalloc(&serdev->dev, sizeof(*rkdev), GFP_KERNEL);
	if (!rkdev)
		return -ENOMEM;

	rkdev->serdev_hu.serdev = serdev;
	serdev_device_set_drvdata(serdev, rkdev);

	data = device_get_match_data(&serdev->dev);
	if (!data)
		return -ENODEV;

	rkdev->reset_gpio = devm_gpiod_get_optional(&serdev->dev, "reset",
						    GPIOD_OUT_LOW);
	if (IS_ERR(rkdev->reset_gpio))
		return dev_err_probe(&serdev->dev, PTR_ERR(rkdev->reset_gpio),
				     "Failed to get reset GPIO\n");

	rkdev->shutdown_gpio = devm_gpiod_get_optional(&serdev->dev, "shutdown",
						       GPIOD_OUT_LOW);
	if (IS_ERR(rkdev->shutdown_gpio))
		return dev_err_probe(&serdev->dev, PTR_ERR(rkdev->shutdown_gpio),
				     "Failed to get shutdown GPIO\n");

	device_property_read_u32(&serdev->dev, "max-speed", &rkdev->oper_speed);
	if (!rkdev->oper_speed)
		rkdev->oper_speed = 1500000;

	device_property_read_u32(&serdev->dev, "rockchip,init-speed",
				 &rkdev->init_speed);
	if (!rkdev->init_speed)
		rkdev->init_speed = 115200;

	rkdev->flow_ctrl = device_property_read_bool(&serdev->ctrl->dev,
						     "uart-has-rtscts");
	rkdev->lpm_enable = device_property_read_bool(&serdev->dev,
						      "rockchip,enable-lpm");

	device_property_read_u8(&serdev->dev, "rockchip,sco-data-path",
				&rkdev->sco_path);

	if (device_property_read_u16(&serdev->dev, "rockchip,sco-voice-setting",
				     &rkdev->voice_setting))
		rkdev->voice_setting = RK_VOICE_SETTING;

	rkdev->bdev.is_rk962  = data->is_rk962;
	rkdev->bdev.rom_major = 1;
	rkdev->bdev.rom_minor = 1;
	btrk_select_firmware(&rkdev->bdev);

	ret = hci_uart_register_device(&rkdev->serdev_hu, &rk_proto);
	if (ret)
		return dev_err_probe(&serdev->dev, ret,
				     "Failed to register HCI UART device\n");

	hdev = rkdev->serdev_hu.hdev;
	set_bit(HCI_QUIRK_NON_PERSISTENT_SETUP, &hdev->quirks);
	btrk_configure_hdev(hdev);
	hdev->shutdown = rk_shutdown;

	return 0;
}

static void rk_serdev_remove(struct serdev_device *serdev)
{
	struct rk_serdev *rkdev = serdev_device_get_drvdata(serdev);

	hci_uart_unregister_device(&rkdev->serdev_hu);
}

static const struct of_device_id rk_bluetooth_of_match[] = {
	{ .compatible = "rockchip,rk960-bt", .data = &rk960_family_data },
	{ .compatible = "rockchip,rk962-bt", .data = &rk962_family_data },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, rk_bluetooth_of_match);

static struct serdev_device_driver rk_serdev_driver = {
	.probe  = rk_serdev_probe,
	.remove = rk_serdev_remove,
	.driver = {
		.name           = "hci_uart_rk",
		.of_match_table = of_match_ptr(rk_bluetooth_of_match),
	},
};

int __init rkbt_init(void)
{
	int ret;

	ret = serdev_device_driver_register(&rk_serdev_driver);
	if (ret)
		return ret;

	ret = hci_uart_register_proto(&rk_proto);
	if (ret)
		serdev_device_driver_unregister(&rk_serdev_driver);

	return ret;
}

int __exit rkbt_deinit(void)
{
	serdev_device_driver_unregister(&rk_serdev_driver);
	return hci_uart_unregister_proto(&rk_proto);
}
