/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */
#ifndef _ROCKCHIP_THUNDERBOOT_H
#define _ROCKCHIP_THUNDERBOOT_H

#ifdef CONFIG_ROCKCHIP_THUNDER_BOOT
int rk_tb_prepare_ramdisk_decompress(struct device *dev);
void rk_tb_ramdisk_compress_done(void);
int rk_tb_wait_ramdisk_compress_done(u32 timeout_ms);
#else
static inline int rk_tb_prepare_ramdisk_decompress(struct device *dev)
{
	return 0;
}
static inline void rk_tb_ramdisk_compress_done(void) {}
static inline int rk_tb_wait_ramdisk_compress_done(u32 timeout_ms)
{
	return timeout_ms;
}
#endif
#endif
