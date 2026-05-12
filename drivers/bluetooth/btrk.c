// SPDX-License-Identifier: GPL-2.0-only
/*
 * Bluetooth chip-level support for Rockchip RK960/RK962 series
 *
 * All chip-specific logic: chip-revision detection, firmware selection,
 * firmware download, HCI vendor commands, and post-firmware initialisation.
 * Transport-independent; shared by any transport driver for Rockchip BT chips.
 *
 * Copyright (C) 2026 Rockchip Electronics Co., Ltd.
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/unaligned.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include "btrk.h"
#include "hci_uart.h"

#define FW_RK960_RF_V11		"rockchip/rk960_bt_rf.bin"
#define FW_RK960_BT_V11		"rockchip/rk960_bt.bin"
#define FW_RK962_RF_V11		"rockchip/rk962_bt_rf.bin"
#define FW_RK962_BT_V11		"rockchip/rk962_bt.bin"
#define FW_RK960_RF_V2		"rockchip/rk960_bt_rf_v2.bin"
#define FW_RK960_BT_V2		"rockchip/rk960_bt_v2.bin"
#define FW_RK962_RF_V2		"rockchip/rk962_bt_rf_v2.bin"
#define FW_RK962_BT_V2		"rockchip/rk962_bt_v2.bin"
#define FW_RK962_MCU		"rockchip/rk962_bt_mcu.bin"

static int btrk_loader_mode = BTRK_LOADER_MODE_NORMAL;
module_param(btrk_loader_mode, int, 0644);
MODULE_PARM_DESC(btrk_loader_mode,
		 "1=two-stage RF+BT fw (default) 2=single MCU fw");

#define FW_PACK_SIZE		255
#define BTRK_FW_MAX_SEGS	2
#define BTRK_READY_POLL_MIN_US	10000
#define BTRK_READY_POLL_MAX_US	20000
#define BTRK_READY_CMD_TIMEOUT	msecs_to_jiffies(100)
#define BTRK_POST_BOOT_DELAY_MS	0
#define BTRK_RF_EXEC_DELAY_MS	35
#define BTRK_POST_FW_PRE_FLOW_DELAY_MS	6

struct btrk_nvds_param {
	u8 tag;
	u8 length;
	u8 data[128];
} __packed;

struct btrk_lpm_param {
	u8 sleep_mode;
	u8 host_stack_idle_threshold;
	u8 host_controller_idle_threshold;
	u8 bt_wake_polarity;
	u8 host_wake_polarity;
	u8 allow_host_sleep_during_sco;
	u8 combine_sleep_mode_and_lpm;
	u8 enable_uart_txd_tri_state;
	u8 sleep_guard_time;
	u8 wakeup_guard_time;
	u8 txd_config;
	u8 pulsed_host_wake;
} __packed;

struct btrk_fw_seg {
	u32 vmaddr;
	u32 length;
};

static void btrk_apply_policy(struct btrk_dev *bdev)
{
	bdev->loader_load_addr = 0x00212000u;
	bdev->need_rf_stage = true;
	bdev->need_launch_addr = true;
	bdev->use_loader_baud_cmd = true;
	bdev->supports_board_cfg = bdev->loader_type == RK962_E2;
	bdev->use_bdaddr_property = true;
}

u32 btrk_js_hash(u32 hash, const u8 *buf, u32 len)
{
	u32 i;

	for (i = 0; i < len; i++)
		hash ^= ((hash << 5) + buf[i] + (hash >> 2));
	return hash;
}
EXPORT_SYMBOL_GPL(btrk_js_hash);

static int btrk_parse_flat_fw_segments(struct hci_dev *hdev,
				       const struct firmware *fw,
				       struct btrk_fw_seg **segs_out,
				       u32 *n_segs_out,
				       size_t *data_start)
{
	struct btrk_fw_seg *segs;

	segs = kcalloc(BTRK_FW_MAX_SEGS, sizeof(*segs), GFP_KERNEL);
	if (!segs)
		return -ENOMEM;

	segs[0].vmaddr = BTRK_FW_SRAM_BASE;
	segs[0].length = fw->size / 2;
	segs[1].vmaddr = BTRK_FW_SRAM_BASE + segs[0].length;
	segs[1].length = fw->size - segs[0].length;

	*segs_out = segs;
	*n_segs_out = BTRK_FW_MAX_SEGS;
	*data_start = 0;

	bt_dev_info(hdev, "RK: using flat-image firmware layout (%zu bytes)",
		    fw->size);

	return 0;
}

static int btrk_parse_mcu_fw_segments(const struct firmware *fw,
				      struct btrk_fw_seg **segs_out,
				      u32 *n_segs_out,
				      size_t *data_start,
				      bool *adjusted_last)
{
	struct btrk_fw_seg *segs;
	const u8 *p;
	size_t hdr_len;
	size_t payload_len;
	u32 tail_len;

	hdr_len = sizeof(u32) + BTRK_FW_MAX_SEGS * sizeof(segs[0]);
	if (fw->size < hdr_len)
		return -ENOENT;

	p = fw->data;
	if (get_unaligned_le32(p) != BTRK_FW_MAX_SEGS)
		return -ENOENT;

	segs = kcalloc(BTRK_FW_MAX_SEGS, sizeof(*segs), GFP_KERNEL);
	if (!segs)
		return -ENOMEM;

	p += sizeof(u32);
	segs[0].vmaddr = get_unaligned_le32(p);
	segs[0].length = get_unaligned_le32(p + sizeof(u32));
	segs[1].vmaddr = get_unaligned_le32(p + sizeof(segs[0]));
	segs[1].length = get_unaligned_le32(p + sizeof(segs[0]) +
					    sizeof(u32));

	payload_len = fw->size - hdr_len;
	if (!payload_len || !segs[0].length ||
	    segs[0].length >= payload_len) {
		kfree(segs);
		return -ENOENT;
	}

	tail_len = payload_len - segs[0].length;
	if (segs[1].length != tail_len) {
		segs[1].length = tail_len;
		*adjusted_last = true;
	}

	*segs_out = segs;
	*n_segs_out = BTRK_FW_MAX_SEGS;
	*data_start = hdr_len;
	return 0;
}

static int btrk_parse_fw_segments(struct hci_dev *hdev,
				  const struct firmware *fw,
				  struct btrk_fw_seg **segs_out,
				  u32 *n_segs_out,
				  size_t *data_start,
				  bool *adjusted_last)
{
	int ret;

	*segs_out = NULL;
	*n_segs_out = 0;
	*data_start = 0;
	*adjusted_last = false;

	if (fw->size < sizeof(u32)) {
		bt_dev_err(hdev, "RK: firmware too small for section header (%zu)",
			   fw->size);
		return -EINVAL;
	}

	ret = btrk_parse_mcu_fw_segments(fw, segs_out, n_segs_out,
					 data_start, adjusted_last);
	if (ret != -ENOENT)
		return ret;

	return btrk_parse_flat_fw_segments(hdev, fw, segs_out, n_segs_out,
					   data_start);
}

enum btrk_boot_flow {
	BTRK_BOOT_FLOW_NORMAL = 0,
	BTRK_BOOT_FLOW_STD,
};

struct btrk_boot_plan {
	enum btrk_boot_flow	flow;
	int			loader_mode;
	int			requested_mode;
	bool			run_post_init;
};

static int btrk_hdev_set_bdaddr(struct hci_dev *hdev, const bdaddr_t *bdaddr)
{
	if (!bacmp(bdaddr, BDADDR_ANY))
		return -EINVAL;

	return btrk_hci_cmd(hdev, HCI_OP_RK_WRITE_BD_ADDR, bdaddr, 6);
}

void btrk_select_firmware(struct btrk_dev *bdev)
{
	bool v2 = (bdev->rom_major > 1 ||
		   (bdev->rom_major == 1 && bdev->rom_minor > 1));

	if (!bdev->is_rk962) {
		bdev->fw_rf = v2 ? FW_RK960_RF_V2 : FW_RK960_RF_V11;
		bdev->fw_bt = v2 ? FW_RK960_BT_V2 : FW_RK960_BT_V11;
		bdev->loader_type = v2 ? RK960_E2 : RK960_E1;
	} else {
		bdev->fw_rf = v2 ? FW_RK962_RF_V2 : FW_RK962_RF_V11;
		bdev->fw_bt = v2 ? FW_RK962_BT_V2 : FW_RK962_BT_V11;
		bdev->loader_type = v2 ? RK962_E2 : RK962_E1;
	}

	btrk_apply_policy(bdev);
}
EXPORT_SYMBOL_GPL(btrk_select_firmware);

void btrk_configure_hdev(struct hci_dev *hdev)
{
	hdev->set_bdaddr = btrk_hdev_set_bdaddr;
}
EXPORT_SYMBOL_GPL(btrk_configure_hdev);

static int btrk_build_boot_plan(struct btrk_boot_plan *plan)
{
	int loader_mode;

	loader_mode = btrk_loader_mode;
	plan->requested_mode = loader_mode;

	switch (loader_mode) {
	case BTRK_LOADER_MODE_NORMAL:
		plan->flow = BTRK_BOOT_FLOW_NORMAL;
		plan->run_post_init = true;
		break;
	case BTRK_LOADER_MODE_STD:
		plan->flow = BTRK_BOOT_FLOW_STD;
		plan->run_post_init = false;
		break;
	default:
		return -EINVAL;
	}

	plan->loader_mode = loader_mode;
	return 0;
}

/* HCI vendor command helpers */
static int btrk_read_chip_name_timeout(struct hci_dev *hdev, char *buf,
				       size_t len, unsigned long timeout,
				       bool verbose)
{
	struct sk_buff *skb;
	struct hci_rp_read_local_name *rp;

	skb = __hci_cmd_sync(hdev, HCI_OP_READ_LOCAL_NAME, 0, NULL, timeout);
	if (IS_ERR(skb)) {
		if (verbose)
			bt_dev_err(hdev, "RK: HCI_Read_Local_Name failed (%ld)",
				   PTR_ERR(skb));
		return PTR_ERR(skb);
	}
	if (skb->len != sizeof(*rp)) {
		if (verbose)
			bt_dev_err(hdev,
				   "RK: HCI_Read_Local_Name length mismatch (%u)",
				   skb->len);
		kfree_skb(skb);
		return -EIO;
	}

	rp = (struct hci_rp_read_local_name *)skb->data;
	if (rp->status) {
		if (verbose)
			bt_dev_err(hdev, "RK: HCI_Read_Local_Name status 0x%02x",
				   rp->status);
		kfree_skb(skb);
		return -rp->status;
	}

	if (strscpy(buf, (const char *)rp->name, len) < 0) {
		if (verbose)
			bt_dev_err(hdev, "RK: chip name is truncated");
		kfree_skb(skb);
		return -EILSEQ;
	}

	kfree_skb(skb);
	return 0;
}

int btrk_read_chip_name(struct hci_dev *hdev, char *buf, size_t len)
{
	return btrk_read_chip_name_timeout(hdev, buf, len, HCI_INIT_TIMEOUT,
					   true);
}
EXPORT_SYMBOL_GPL(btrk_read_chip_name);

int btrk_parse_chip_name(struct btrk_dev *bdev, struct hci_dev *hdev,
			 const char *name)
{
	const char *p;

	if (!strncasecmp(name, "RK962", 5))
		bdev->is_rk962 = true;
	else if (!strncasecmp(name, "RK960", 5))
		bdev->is_rk962 = false;
	else
		bt_dev_warn(hdev, "RK: unknown chip prefix in '%s'", name);

	bdev->rom_major = 0;
	bdev->rom_minor = 0;

	p = strchr(name, 'V');
	if (!p)
		p = strchr(name, 'v');
	if (p) {
		p++;
		while (*p && *p != '.') {
			if (*p < '0' || *p > '9')
				break;
			bdev->rom_major = bdev->rom_major * 10 + (*p - '0');
			p++;
		}

		if (*p == '.') {
			p++;
			while (*p) {
				if (*p < '0' || *p > '9')
					break;
				bdev->rom_minor = bdev->rom_minor * 10 + (*p - '0');
				p++;
			}
		}
	}

	bt_dev_info(hdev, "RK: chip is_rk962=%d rom=%d.%d",
		    bdev->is_rk962, bdev->rom_major, bdev->rom_minor);

	btrk_select_firmware(bdev);
	return 0;
}
EXPORT_SYMBOL_GPL(btrk_parse_chip_name);

int btrk_hci_cmd(struct hci_dev *hdev, u16 opcode,
		 const void *param, u8 plen)
{
	struct sk_buff *skb;

	skb = __hci_cmd_sync(hdev, opcode, plen, param, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		bt_dev_err(hdev, "RK: HCI cmd 0x%04x failed (%ld)",
			   opcode, PTR_ERR(skb));
		return PTR_ERR(skb);
	}
	kfree_skb(skb);
	return 0;
}
EXPORT_SYMBOL_GPL(btrk_hci_cmd);

static int btrk_write_nvds_param(struct hci_dev *hdev, u8 tag,
				 const void *data, u8 len)
{
	struct btrk_nvds_param nvds = {
		.tag    = tag,
		.length = len,
	};

	if (len > sizeof(nvds.data))
		return -EINVAL;

	memcpy(nvds.data, data, len);
	return btrk_hci_cmd(hdev, HCI_OP_RK_WRITE_NVDS, &nvds, sizeof(nvds));
}

int btrk_set_launch_addr(struct hci_dev *hdev, u32 addr)
{
	__le32 le_addr = cpu_to_le32(addr);

	return btrk_hci_cmd(hdev, HCI_OP_RK_SET_LAUNCH_ADDR,
			    &le_addr, sizeof(le_addr));
}
EXPORT_SYMBOL_GPL(btrk_set_launch_addr);

int btrk_set_sco_path(struct hci_dev *hdev, u8 path)
{
	u8 sync_cfg[2] = { path, path };

	return btrk_write_nvds_param(hdev, PARAM_ID_SYNC_CFG,
				     sync_cfg, sizeof(sync_cfg));
}
EXPORT_SYMBOL_GPL(btrk_set_sco_path);

int btrk_send_speed_cmd(struct hci_dev *hdev, u16 opcode, u32 speed)
{
	u8 param[6] = { 0, 0 };

	put_unaligned_le32(speed, param + 2);
	return btrk_hci_cmd(hdev, opcode, param, sizeof(param));
}
EXPORT_SYMBOL_GPL(btrk_send_speed_cmd);

int btrk_set_board_cfg(struct hci_dev *hdev, bool flow_ctrl, bool lpm_enable)
{
	u8 cfg[sizeof(u64) * 2] = { 0 };

	cfg[1] |= BIT(1);
	cfg[2] |= BIT(5);

	if (flow_ctrl)
		cfg[9] |= BIT(1);
	if (lpm_enable)
		cfg[10] |= BIT(5);

	return btrk_hci_cmd(hdev, HCI_OP_RK_SET_BOARD_CFG, cfg, sizeof(cfg));
}
EXPORT_SYMBOL_GPL(btrk_set_board_cfg);

/* Firmware download */
int btrk_fw_probe(struct hci_dev *hdev, const struct firmware *fw,
		  size_t *data_start)
{
	struct btrk_fw_seg *segs;
	const u8 *data;
	size_t data_len;
	size_t cmd_len;
	u32 n_segs;
	u32 hash = 0;
	u8 *cmd;
	u8 *cp;
	bool adjusted_last;
	int ret;
	u32 i;

	ret = btrk_parse_fw_segments(hdev, fw, &segs, &n_segs,
				     data_start, &adjusted_last);
	if (ret)
		return ret;

	if (adjusted_last)
		bt_dev_warn(hdev,
			    "RK: firmware header size mismatch, adjusted last section to %u bytes",
			    segs[n_segs - 1].length);

	data = fw->data + *data_start;
	data_len = fw->size - *data_start;
	for (i = 0; i < data_len; i += FW_PACK_SIZE) {
		u32 chunk = min_t(u32, data_len - i, FW_PACK_SIZE);

		hash = btrk_js_hash(hash, data + i, chunk);
	}

	cmd_len = 8 + (size_t)n_segs * sizeof(*segs);
	cmd = kmalloc(cmd_len, GFP_KERNEL);
	if (!cmd) {
		kfree(segs);
		return -ENOMEM;
	}

	cp = cmd;
	put_unaligned_le32(n_segs, cp);
	cp += sizeof(u32);
	for (i = 0; i < n_segs; i++) {
		put_unaligned_le32(segs[i].vmaddr, cp);
		cp += sizeof(u32);
		put_unaligned_le32(segs[i].length, cp);
		cp += sizeof(u32);
	}
	put_unaligned_le32(hash, cp);

	ret = btrk_hci_cmd(hdev, HCI_OP_RK_FW_PROBE, cmd, cmd_len);
	kfree(cmd);
	kfree(segs);
	return ret;
}
EXPORT_SYMBOL_GPL(btrk_fw_probe);

int btrk_fw_transfer(struct hci_dev *hdev, const struct firmware *fw,
		     size_t data_start)
{
	const u8 *p = fw->data + data_start;
	size_t left = fw->size - data_start;

	while (left > 0) {
		u32 chunk = min_t(u32, left, FW_PACK_SIZE);
		int ret;

		ret = btrk_hci_cmd(hdev, HCI_OP_RK_FW_XFER, p, chunk);
		if (ret)
			return ret;

		p += chunk;
		left -= chunk;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(btrk_fw_transfer);

int btrk_fw_download(struct hci_dev *hdev, const char *name)
{
	const struct firmware *fw;
	size_t data_start;
	int ret;

	bt_dev_info(hdev, "RK: loading %s", name);
	ret = request_firmware(&fw, name, &hdev->dev);
	if (ret) {
		bt_dev_err(hdev, "RK: failed to load %s (%d)", name, ret);
		return ret;
	}
	bt_dev_info(hdev, "RK: firmware loaded (%zu bytes)", fw->size);

	ret = btrk_fw_probe(hdev, fw, &data_start);
	if (ret)
		goto out;

	ret = btrk_fw_transfer(hdev, fw, data_start);
out:
	release_firmware(fw);
	return ret;
}
EXPORT_SYMBOL_GPL(btrk_fw_download);

/* Post-firmware HCI initialisation */
static int btrk_wait_hci_reset(struct hci_dev *hdev, unsigned long timeout)
{
	unsigned long deadline;
	int ret = -ETIMEDOUT;

	if (!timeout)
		timeout = 1;

	deadline = jiffies + timeout;

	do {
		struct sk_buff *skb;
		unsigned long remaining = deadline - jiffies;
		unsigned long cmd_timeout;

		if (!remaining)
			remaining = 1;

		cmd_timeout = min_t(unsigned long, remaining,
				    BTRK_READY_CMD_TIMEOUT);
		skb = __hci_cmd_sync(hdev, HCI_OP_RESET, 0, NULL, cmd_timeout);
		if (!IS_ERR(skb)) {
			kfree_skb(skb);
			return 0;
		}

		ret = PTR_ERR(skb);
		if (time_after_eq(jiffies, deadline))
			break;

		usleep_range(BTRK_READY_POLL_MIN_US, BTRK_READY_POLL_MAX_US);
	} while (1);

	bt_dev_err(hdev, "RK: HCI_Reset not ready within %u ms (%d)",
		   jiffies_to_msecs(timeout), ret);
	return ret;
}

int btrk_post_init(struct hci_dev *hdev, u16 voice_setting, bool lpm_enable)
{
	__le16 vs = cpu_to_le16(voice_setting);
	struct btrk_lpm_param lpm = {
		.sleep_mode                     = RK_LPM_SLEEP_MODE,
		.host_stack_idle_threshold      = 1,
		.host_controller_idle_threshold = 1,
		.bt_wake_polarity               = 1,
		.host_wake_polarity             = 1,
		.allow_host_sleep_during_sco    = 1,
		.combine_sleep_mode_and_lpm     = 1,
	};
	int ret;

	ret = btrk_wait_hci_reset(hdev, HCI_CMD_TIMEOUT);
	if (ret)
		return ret;

	ret = btrk_hci_cmd(hdev, HCI_OP_WRITE_VOICE_SETTING, &vs, sizeof(vs));
	if (ret)
		return ret;

	if (!lpm_enable) {
		struct btrk_lpm_param disabled_lpm = {};

		btrk_hci_cmd(hdev, HCI_OP_RK_LPM_ENABLE,
			     &disabled_lpm, sizeof(disabled_lpm));
		return 0;
	}

	return btrk_hci_cmd(hdev, HCI_OP_RK_LPM_ENABLE, &lpm, sizeof(lpm));
}
EXPORT_SYMBOL_GPL(btrk_post_init);

static void btrk_stage_delay_ms(unsigned int delay_ms)
{
	if (!delay_ms)
		return;

	if (delay_ms < 20)
		usleep_range(delay_ms * 1000, delay_ms * 1000 + 1000);
	else
		msleep(delay_ms);
}

static int btrk_setup_fw_normal(struct hci_uart *hu, struct btrk_dev *bdev,
				const struct btrk_setup_info *info,
				const struct btrk_setup_ops *ops, void *ctx)
{
	struct hci_dev *hdev = hu->hdev;
	char chip_name[HCI_MAX_NAME_LENGTH + 1];
	int ret;

	ret = btrk_set_sco_path(hdev, info->sco_path);
	if (ret)
		return ret;

	if (bdev->need_rf_stage) {
		if (info->oper_speed && info->oper_speed != 115200) {
			ret = ops->set_speed(hu, HCI_OP_RK_CHG_LOADER_BAUD,
					    info->oper_speed, ctx);
			if (ret)
				return ret;
		}

		if (bdev->need_launch_addr) {
			ret = btrk_set_launch_addr(hdev, BTRK_FW_SRAM_BASE);
			if (ret)
				return ret;
		}

		ret = btrk_read_chip_name(hdev, chip_name, sizeof(chip_name));
		if (!ret) {
			bt_dev_info(hdev, "RK: chip '%s'", chip_name);
			btrk_parse_chip_name(bdev, hdev, chip_name);
		} else {
			bt_dev_warn(hdev,
				    "RK: chip name unavailable in loader (%d), using %s/%s",
				    ret, bdev->fw_rf, bdev->fw_bt);
		}

		ret = btrk_fw_download(hdev, bdev->fw_rf);
		if (ret)
			return ret;

		btrk_stage_delay_ms(BTRK_RF_EXEC_DELAY_MS);
	}

	if (info->oper_speed && info->oper_speed != 115200) {
		if (bdev->use_loader_baud_cmd) {
			ret = ops->set_speed(hu, HCI_OP_RK_CHG_LOADER_BAUD,
					     info->oper_speed, ctx);
		} else {
			ops->set_baudrate(hu, info->oper_speed, ctx);
			ret = 0;
		}
		if (ret)
			return ret;
	}

	if (bdev->need_launch_addr) {
		ret = btrk_set_launch_addr(hdev, BTRK_FW_SRAM_BASE);
		if (ret)
			return ret;
	}

	if (bdev->supports_board_cfg) {
		ret = btrk_set_board_cfg(hdev, info->flow_ctrl, info->lpm_enable);
		if (ret)
			return ret;
	}

	ret = btrk_read_chip_name(hdev, chip_name, sizeof(chip_name));
	if (!ret)
		bt_dev_info(hdev, "RK: loader ready, chip '%s'", chip_name);

	return btrk_fw_download(hdev, bdev->fw_bt);
}

static int btrk_setup_fw_std(struct hci_uart *hu, struct btrk_dev *bdev,
			     const struct btrk_setup_info *info,
			     const struct btrk_setup_ops *ops, void *ctx)
{
	struct hci_dev *hdev = hu->hdev;
	char chip_name[HCI_MAX_NAME_LENGTH + 1];
	int ret;

	bt_dev_info(hdev, "RK: loader_mode=2, MCU fw '%s'", FW_RK962_MCU);

	if (info->oper_speed && info->oper_speed != 115200) {
		ret = ops->set_speed(hu, HCI_OP_RK_CHG_LOADER_BAUD,
				    info->oper_speed, ctx);
		if (ret)
			return ret;
	}

	if (bdev->need_launch_addr) {
		ret = btrk_set_launch_addr(hdev, BTRK_FW_SRAM_BASE);
		if (ret)
			return ret;
	}

	ret = btrk_read_chip_name(hdev, chip_name, sizeof(chip_name));
	if (!ret)
		bt_dev_info(hdev, "RK: chip '%s'", chip_name);

	return btrk_fw_download(hdev, FW_RK962_MCU);
}

static int btrk_setup_post_fw(struct hci_uart *hu,
			      const struct btrk_setup_info *info,
			      const struct btrk_setup_ops *ops, void *ctx)
{
	int ret;

	btrk_stage_delay_ms(BTRK_POST_FW_PRE_FLOW_DELAY_MS);

	if (info->flow_ctrl)
		ops->set_flow_control(hu, false, ctx);

	if (info->oper_speed && info->oper_speed != 115200) {
		ret = ops->set_speed(hu, HCI_OP_RK_CHG_FW_BAUD,
				    info->oper_speed, ctx);
		if (ret)
			return ret;
	}

	return btrk_post_init(hu->hdev, info->voice_setting, info->lpm_enable);
}

int btrk_setup(struct hci_uart *hu, struct btrk_dev *bdev,
	       const struct btrk_setup_info *info,
	       const struct btrk_setup_ops *ops, void *ctx)
{
	struct hci_dev *hdev = hu->hdev;
	struct btrk_boot_plan plan;
	int ret;

	btrk_select_firmware(bdev);
	if (bdev->use_bdaddr_property)
		set_bit(HCI_QUIRK_USE_BDADDR_PROPERTY, &hdev->quirks);
	else
		clear_bit(HCI_QUIRK_USE_BDADDR_PROPERTY, &hdev->quirks);

	ret = btrk_build_boot_plan(&plan);
	if (ret) {
		bt_dev_err(hdev, "RK: unsupported loader_mode=%d",
			   plan.requested_mode);
		return ret;
	}

	ret = ops->power_reset(hu, ctx);
	if (ret)
		return ret;

	ret = ops->setup_bootloader(hu, bdev, ctx);
	if (ret)
		goto err_power;

	btrk_stage_delay_ms(BTRK_POST_BOOT_DELAY_MS);
	ops->set_hci_ready(ctx);

	bt_dev_info(hdev, "RK: loader_mode=%d (requested=%d)",
		    plan.loader_mode, plan.requested_mode);

	if (plan.flow == BTRK_BOOT_FLOW_STD) {
		ret = btrk_setup_fw_std(hu, bdev, info, ops, ctx);
	} else {
		ret = btrk_setup_fw_normal(hu, bdev, info, ops, ctx);
		if (!ret && plan.run_post_init)
			ret = btrk_setup_post_fw(hu, info, ops, ctx);
	}
	if (ret)
		goto err_power;

	bt_dev_info(hdev, "RK: initialisation complete");
	return 0;

err_power:
	ops->power_off(ctx);
	return ret;
}
EXPORT_SYMBOL_GPL(btrk_setup);

MODULE_AUTHOR("Xiao Yao <xiaoyao@rock-chips.com>");
MODULE_DESCRIPTION("Bluetooth support for Rockchip devices");
MODULE_LICENSE("GPL");
MODULE_FIRMWARE(BTRK_FW_LOADER);
MODULE_FIRMWARE(FW_RK960_RF_V11);
MODULE_FIRMWARE(FW_RK960_BT_V11);
MODULE_FIRMWARE(FW_RK962_RF_V11);
MODULE_FIRMWARE(FW_RK962_BT_V11);
MODULE_FIRMWARE(FW_RK960_RF_V2);
MODULE_FIRMWARE(FW_RK960_BT_V2);
MODULE_FIRMWARE(FW_RK962_RF_V2);
MODULE_FIRMWARE(FW_RK962_BT_V2);
MODULE_FIRMWARE(FW_RK962_MCU);
