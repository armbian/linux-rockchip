/* SPDX-License-Identifier: GPL-2.0 */

/* Copyright (c) 2024 Rockchip Electronics Co., Ltd. */

#ifndef __RKCE_DEBUG_H__
#define __RKCE_DEBUG_H__

extern ulong rkce_debug_level;

/* must define marco before rkce_debug.h */
//#define RKCE_MODULE_TAG
//#define RKCE_MODULE_OFFSET

#define RKCE_LOG_LEVEL() (((rkce_debug_level) >> (RKCE_MODULE_OFFSET)) & 0x3)

enum rkce_log_level {
	RKCE_LOG_ERR = 0,
	RKCE_LOG_INFO,
	RKCE_LOG_DEBUG,
	RKCE_LOG_VERBOSE,
	RKCE_LOG_MAX,
};

#define rk_err(fmt, args...)     \
	do { \
		if (RKCE_LOG_LEVEL() >= RKCE_LOG_ERR) \
			pr_err("RKCE-%s: E [%s %d]: " fmt "\n", \
			       RKCE_MODULE_TAG, __func__, __LINE__, ##args); \
	} while (0)

#define rk_info(fmt, args...)     \
	do { \
		if (RKCE_LOG_LEVEL() >= RKCE_LOG_INFO) \
			pr_err(fmt, ##args); \
	} while (0)

#define rk_debug(fmt, args...)     \
	do { \
		if (RKCE_LOG_LEVEL() >= RKCE_LOG_DEBUG) \
			pr_err("RKCE-%s: D [%s %d]: " fmt "\n", \
			       RKCE_MODULE_TAG, __func__, __LINE__, ##args); \
	} while (0)

#define rk_trace(fmt, args...)     \
	do { \
		if (RKCE_LOG_LEVEL() >= RKCE_LOG_VERBOSE) \
			pr_err("RKCE-%s: T [%s %d]: " fmt "\n", \
			       RKCE_MODULE_TAG, __func__, __LINE__, ##args); \
	} while (0)

void rkce_dump_td(void *td);

#if defined(DEBUG)
#define rkce_dumphex(var_name, data, len) print_hex_dump(KERN_CONT, (var_name), \
							 DUMP_PREFIX_OFFSET, \
							 16, 1, (data), (len), false)
#else
#define rkce_dumphex(var_name, data, len) \
	do { \
		if (0) \
			print_hex_dump(KERN_CONT, (var_name), \
				       DUMP_PREFIX_OFFSET, \
				       16, 1, (data), (len), false); \
	} while (0)
#endif

#endif
