// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CTS_FIRMWARE_H
#define CTS_FIRMWARE_H

#include "cts_config.h"
#include <linux/firmware.h>
#include <linux/vmalloc.h>



#define FIRMWARE_VERSION_OFFSET     0x100
#define FIRMWARE_VERSION(firmware)  \
    get_unaligned_le16((firmware)->data + FIRMWARE_VERSION_OFFSET)

struct cts_device;
extern int cts_request_newer_firmware_from_fs(struct cts_device *cts_dev,
        const char *filepath, u16 curr_version);
extern bool cts_is_firmware_updating(struct cts_device *cts_dev);
extern int cts_request_firmware(struct cts_device *cts_dev,
        u32 hwid, u16 fwid, u16 device_fw_ver);
extern int cts_update_firmware(struct cts_device *cts_dev);

#ifdef CFG_CTS_FIRMWARE_IN_FS
extern int cts_update_firmware_from_file(struct cts_device *cts_dev,
    const char *filepath);

#endif /* CFG_CTS_FIRMWARE_IN_FS */

extern u16 cts_crc16(const u8 *data, size_t len);
extern u32 cts_crc32(const u8 *data, size_t len);

#endif /* CTS_FIRMWARE_H */
