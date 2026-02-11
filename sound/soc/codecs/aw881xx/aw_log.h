/* SPDX-License-Identifier: GPL-2.0
 * aw_log.h   aw codec driver
 *
 * Copyright (c) 2020 AWINIC Technology CO., LTD
 *
 *  Author: Bruce zhao <zhaolei@awinic.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __AW_LOG_H__
#define __AW_LOG_H__

#define LOG_LEVEL      (0)
#define LOG_LEVEL_ERR  (0)
#define LOG_LEVEL_DBG  (1)
#define LOG_LEVEL_INFO (2)

/********************************************
 * print information control
 *******************************************/
#define aw_dev_err(dev, format, ...) \
	do { \
		if (LOG_LEVEL >= LOG_LEVEL_ERR) \
			pr_err("[Awinic][%s]%s: " format "\n", \
			       dev_name(dev), __func__, ##__VA_ARGS__); \
	} while (0)

#define aw_dev_info(dev, format, ...) \
	do { \
		if (LOG_LEVEL >= LOG_LEVEL_INFO) \
			pr_info("[Awinic][%s]%s: " format "\n", \
				dev_name(dev), __func__, ##__VA_ARGS__); \
	} while (0)

#define aw_dev_dbg(dev, format, ...) \
	do { \
		if (LOG_LEVEL >= LOG_LEVEL_DBG) \
			pr_debug("[Awinic][%s]%s: " format "\n", \
				 dev_name(dev), __func__, ##__VA_ARGS__); \
	} while (0)

#define aw_pr_err(format, ...) \
	do { \
		if (LOG_LEVEL >= LOG_LEVEL_ERR) \
			pr_err("[Awinic]%s: " format "\n", \
			       __func__, ##__VA_ARGS__); \
	} while (0)

#define aw_pr_info(format, ...) \
	do { \
		if (LOG_LEVEL >= LOG_LEVEL_INFO) \
			pr_info("[Awinic]%s: " format "\n", \
				__func__, ##__VA_ARGS__); \
	} while (0)

#define aw_pr_dbg(format, ...) \
	do { \
		if (LOG_LEVEL >= LOG_LEVEL_DBG) \
			pr_debug("[Awinic]%s: " format "\n", \
				 __func__, ##__VA_ARGS__); \
	} while (0)

#endif
