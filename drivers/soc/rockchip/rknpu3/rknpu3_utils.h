/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 *
 * RKNPU Utility Functions
 */

#ifndef __RKNPU3_UTILS_H__
#define __RKNPU3_UTILS_H__

#include <linux/io.h>
#include <linux/ktime.h>

#include "rknpu3_config.h"
#include "rknpu3_ioctl.h"

/* NPU register access: base is void __iomem *, offset in bytes */
static inline u32 rknpu3_reg_read(void __iomem *base, unsigned int off)
{
	return readl(base + off);
}

static inline void rknpu3_reg_write(void __iomem *base, unsigned int off, u32 val)
{
	writel(val, base + off);
}

/* RKNPU base address mask: bit[0-29]=0x1C00000, bit[34-35]=0x3 */
#define RKNPU3_BASE_MASK 0xC01C00000ULL
/* RKNPU reset address mask: bit[0-29]=0x400A18, bit[34-35]=0x3 */
#define RKNPU3_RESET_MASK 0xC00400A18UL
/* RKNPU hprc address mask: bit[0-29]=0x800000, bit[34-35]=0x3 */
#define RKNPU3_HPRC_MASK 0xC00800000ULL

/* RKNPU core ID bit offset */
#define RKNPU3_CORE_ID_SHIFT 30

#define INT_ADDR_TO_MESH_ADDR(internal_addr, node_id, device) \
	((uint64_t)device << 35 | 1UL << 34 | (uint64_t)(node_id) << 30 | \
	 (uint64_t)(internal_addr))

/**
 * rknpu3_core_to_local_idx() - Convert global core ID to local offset
 * @core_id: Global core ID
 *
 * Return: Local offset, or invalid value if not current node's core
 */
static inline uint32_t rknpu3_core_to_local_idx(uint32_t core_id)
{
	return core_id - RKNPU3_CORE_ID_BASE;
}

/**
 * rknpu3_core_to_global_id() - Convert local offset to global core ID
 * @local_idx: Local offset
 *
 * Return: Global core ID
 */
static inline uint32_t rknpu3_core_to_global_id(uint32_t local_idx)
{
	if (local_idx >= RKNPU3_LOCAL_CORE_NUM)
		return 0xFFFFFFFF; /* Invalid value */

	return RKNPU3_CORE_ID_BASE * RKNPU3_LOCAL_CORE_NUM + local_idx;
}

/**
 * rknpu3_get_time_us() - Get current system time in microseconds
 *
 * Return: Current system time in microseconds
 */
static inline uint64_t rknpu3_get_time_us(void)
{
	return ktime_get_ns() / 1000;
}

/**
 * get_rknpu3_base_addr() - Get RKNPU local base address
 * @core_id: Core ID
 *
 * Return: RKNPU register base address
 */
static inline uint64_t get_rknpu3_base_addr(uint32_t core_id)
{
	return RKNPU3_BASE_MASK | ((uint64_t)core_id << RKNPU3_CORE_ID_SHIFT);
}

/**
 * get_rknpu3_reset_addr() - Get RKNPU reset address
 * @core_id: Core ID
 *
 * Return: RKNPU register reset address
 */
static inline uint64_t get_rknpu3_reset_addr(uint32_t core_id)
{
	return RKNPU3_RESET_MASK | ((uint64_t)core_id << RKNPU3_CORE_ID_SHIFT);
}

/**
 * get_rknpu3_hprc_addr() - Get RKNPU hprc address
 * @core_id: Core ID
 *
 * Return: RKNPU register hprc address
 */
static inline uint64_t get_rknpu3_hprc_addr(uint32_t core_id)
{
	return RKNPU3_HPRC_MASK | ((uint64_t)core_id << RKNPU3_CORE_ID_SHIFT);
}

/**
 * get_dram_local_addr() - Get DRAM local address
 * @addr: Global address
 *
 * Return: Local address
 */
static inline uint32_t get_dram_local_addr(uint64_t addr)
{
	return (uint32_t)(addr & 0xffffffff);
}

/* Time measurement macros */
#define RKNPU3_TIME_START(name) \
	uint64_t _##name##_start = rknpu3_get_time_us()

#define RKNPU3_TIME_END(name)                                         \
	uint64_t _##name##_end = rknpu3_get_time_us();                \
	pr_info(" %s: %lld us\n", #name, _##name##_end - _##name##_start)

/* CPU cycle counting macros */
#ifdef ENABLE_RKNPU3_TIME_PERF
#define RKNPU3_CYCLE_START(name) \
	uint64_t _##name##_cycle_start = __get_MCYCLE()

#define RKNPU3_CYCLE_END(name)                                               \
	uint64_t _##name##_cycle_end = __get_MCYCLE();                      \
	pr_info(" %s cycles: %lld\n", #name,                                \
		_##name##_cycle_end - _##name##_cycle_start)

#else

#define RKNPU3_CYCLE_START(name) ((void)(name))
#define RKNPU3_CYCLE_END(name)   ((void)(name))

#endif

#endif /* __RKNPU3_UTILS_H__ */
