// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CTS_SFCTRL_H
#define CTS_SFCTRL_H

#include "cts_core.h"
#include "cts_spi_flash.h"

struct cts_device;

struct cts_sfctrl_ops {
    int (*rdid)(struct cts_device *cts_dev, u32 *id);
    int (*se)(struct cts_device *cts_dev, u32 sector_addr);
    int (*be)(struct cts_device *cts_dev, u32 sector_addr);
    int (*read)(struct cts_device *cts_dev,
            u32 flash_addr, void *dst, size_t len);
    int (*read_to_sram)(struct cts_device *cts_dev,
            u32 flash_addr, u32 sram_addr, size_t len);
    int (*program)(struct cts_device *cts_dev,
            u32 flash_addr, const void *src, size_t len);
    int (*program_from_sram)(struct cts_device *cts_dev,
            u32 flash_addr, u32 sram_addr, size_t len);
    int (*calc_sram_crc)(struct cts_device *cts_dev,
            u32 sram_addr, size_t len, u32 *crc);
    int (*calc_flash_crc)(struct cts_device *cts_dev,
            u32 flash_addr, size_t len, u32 *crc);
};

struct cts_sfctrl {
    u32 reg_base;
    u32 xchg_sram_base;
    size_t xchg_sram_size;
    const struct cts_sfctrl_ops *ops;
};

extern const struct cts_sfctrl_ops cts_sfctrlv2_ops;

#endif /* CTS_SFCTRL_H */
