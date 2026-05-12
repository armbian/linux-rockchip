/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Bluetooth chip-level support for Rockchip RK960/RK962 series
 *
 * Copyright (C) 2026 Rockchip Electronics Co., Ltd.
 */

#ifndef __BTRK_H
#define __BTRK_H

#include <linux/firmware.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

struct hci_uart;
struct btrk_dev;

/* Supported chip-revision identifiers */
enum rk_loader_type {
	RK960_E1 = 0,
	RK962_E1,
	RK960_E2,
	RK962_E2,
};

/* Firmware loading policies */
enum btrk_loader_mode {
	BTRK_LOADER_MODE_NORMAL = 1,
	BTRK_LOADER_MODE_STD,
};

struct btrk_setup_info {
	u32	oper_speed;
	bool	flow_ctrl;
	bool	lpm_enable;
	u8	sco_path;
	u16	voice_setting;
};

struct btrk_setup_ops {
	int  (*power_reset)(struct hci_uart *hu, void *ctx);
	int  (*setup_bootloader)(struct hci_uart *hu, struct btrk_dev *bdev,
				 void *ctx);
	void (*set_hci_ready)(void *ctx);
	int  (*set_speed)(struct hci_uart *hu, u16 opcode, u32 speed, void *ctx);
	void (*set_baudrate)(struct hci_uart *hu, u32 speed, void *ctx);
	void (*set_flow_control)(struct hci_uart *hu, bool enabled, void *ctx);
	void (*power_off)(void *ctx);
};

/**
 * struct btrk_dev - per-chip state embedded in the transport device struct
 * @loader_type: resolved chip revision used for firmware and init policy
 * @is_rk962:   true for RK962 family, false for RK960
 * @rom_major:  major ROM version reported by the chip
 * @rom_minor:  minor ROM version
 * @fw_rf:      filename of the RF firmware image
 * @fw_bt:      filename of the BT firmware image
 * @loader_load_addr: resolved RKUART bootloader load address
 * @need_rf_stage:    chip uses the split RF + BT firmware flow
 * @need_launch_addr: chip requires the launch-address vendor command
 * @use_loader_baud_cmd: chip uses the loader-stage baud-change command
 * @supports_board_cfg:  chip accepts the board-config vendor command
 * @use_bdaddr_property: use the core BDADDR property quirk
 */
struct btrk_dev {
	enum rk_loader_type	loader_type;
	bool			is_rk962;
	u8			rom_major;
	u8			rom_minor;
	const char		*fw_rf;
	const char		*fw_bt;
	u32			loader_load_addr;
	bool			need_rf_stage;
	bool			need_launch_addr;
	bool			use_loader_baud_cmd;
	bool			supports_board_cfg;
	bool			use_bdaddr_property;
};

/* Shared firmware names */
#define BTRK_FW_LOADER			"rockchip/rk96x_bt_loader.bin"

/* HCI vendor opcode definitions */
#define HCI_OP_RK_WRITE_NVDS		0xFC0A
#define HCI_OP_RK_WRITE_BD_ADDR		0xFC80
#define HCI_OP_RK_SET_LAUNCH_ADDR	0xFC85
#define HCI_OP_RK_FW_PROBE		0xFC2D
#define HCI_OP_RK_FW_XFER		0xFC2E
#define HCI_OP_RK_CHG_LOADER_BAUD	0xFC18
#define HCI_OP_RK_CHG_FW_BAUD		0xFC84
#define HCI_OP_RK_SET_BOARD_CFG		0xFC97
#define HCI_OP_RK_LPM_ENABLE		0xFC83

/* Firmware load address for RF/BT stages */
#define BTRK_FW_SRAM_BASE		0x00200000u

/* NVDS parameter IDs */
#define PARAM_ID_SYNC_CFG		0x2C

/* Post-firmware HCI default values */
#define RK_LPM_SLEEP_MODE		1
#define RK_VOICE_SETTING		0x0060

/* Firmware helper */
u32  btrk_js_hash(u32 hash, const u8 *buf, u32 len);

/* Chip policy helpers */
void btrk_select_firmware(struct btrk_dev *bdev);
void btrk_configure_hdev(struct hci_dev *hdev);
int  btrk_setup(struct hci_uart *hu, struct btrk_dev *bdev,
		const struct btrk_setup_info *info,
		const struct btrk_setup_ops *ops, void *ctx);

/* HCI helper API */
int  btrk_read_chip_name(struct hci_dev *hdev, char *buf, size_t len);
int  btrk_parse_chip_name(struct btrk_dev *bdev, struct hci_dev *hdev,
			  const char *name);
int  btrk_hci_cmd(struct hci_dev *hdev, u16 opcode,
		  const void *param, u8 plen);
int  btrk_set_launch_addr(struct hci_dev *hdev, u32 addr);
int  btrk_set_sco_path(struct hci_dev *hdev, u8 path);
int  btrk_send_speed_cmd(struct hci_dev *hdev, u16 opcode, u32 speed);
int  btrk_set_board_cfg(struct hci_dev *hdev, bool flow_ctrl, bool lpm_enable);
int  btrk_fw_probe(struct hci_dev *hdev, const struct firmware *fw,
		   size_t *data_start);
int  btrk_fw_transfer(struct hci_dev *hdev, const struct firmware *fw,
		      size_t data_start);
int  btrk_fw_download(struct hci_dev *hdev, const char *name);
int  btrk_post_init(struct hci_dev *hdev, u16 voice_setting, bool lpm_enable);

#endif /* __BTRK_H */
