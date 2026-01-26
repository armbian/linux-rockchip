/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 *
 * RKNPU Boot Parameters
 */

#ifndef __RKNRV_BOOT_PARAMS_H__
#define __RKNRV_BOOT_PARAMS_H__

#define RKNRV_BOOT_FLAG_SOFT_INT             (1 << 0)
#define RKNRV_BOOT_FLAG_EXTERNAL_EXEC_PARAMS (1 << 1)
#define RKNRV_BOOT_FLAG_EXTERNAL_STACK       (1 << 2)

#define RKNRV_BOOT_FLAG_SET(params, flag)   ((params)->flags |= (flag))
#define RKNRV_BOOT_FLAG_CLEAR(params, flag) ((params)->flags &= ~(flag))
#define RKNRV_BOOT_FLAG_TEST(params, flag)  ((params)->flags & (flag))

/*
 * Boot parameters structure shared with NPU firmware.
 */
struct rknrv_boot_params {
	const uint32_t magic;
	const uint32_t version;
	const uint32_t entry_point;
	uint32_t error_code;
	uint32_t exit_status;
	uint32_t exec_type;
	uint32_t exec_params;
	uint32_t flags;
	uint32_t kernel_lib_addr;
	uint32_t next_entry_point;
	uint32_t stack_addr;
	uint32_t stack_size;
};

#define RKNRV_EXEC_TYPE_UNDEFINED 0
#define RKNRV_EXEC_TYPE_MODEL     1
#define RKNRV_EXEC_TYPE_KERNEL    2

#endif
