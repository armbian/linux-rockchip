/* Himax Android Driver Sample Code for debug nodes
 *
 * Copyright (C) 2021 Himax Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef H_HIMAX_DEBUG
#define H_HIMAX_DEBUG

#include "himax_platform.h"
#include "himax_common.h"

int himax_touch_proc_init(struct himax_ts_data *ts);
void himax_touch_proc_deinit(struct himax_ts_data *ts);

#define HIMAX_PROC_VENDOR_FILE "vendor"
#define HIMAX_PROC_PEN_POS_FILE "pen_pos"
#define HIMAX_PROC_DIAG_FOLDER "diag"
#define HIMAX_PROC_STACK_FILE "stack"
#define HIMAX_PROC_DELTA_FILE "delta_s"
#define HIMAX_PROC_DC_FILE "dc_s"
#define HIMAX_PROC_BASELINE_FILE "baseline_s"
#define HIMAX_PROC_DEBUG_FILE "debug"
#define HIMAX_PROC_FLASH_DUMP_FILE "flash_dump"

enum flash_dump_prog {
	START,
	ONGOING,
	FINISHED,
};

extern uint8_t X_NUM4;

#if defined(HX_RST_PIN_FUNC)
extern void himax_ic_reset(uint8_t loadconfig, uint8_t int_off);
#endif

/* Moved from debug.c end */
#define CMD_NUM 16
static const char *dbg_cmd_str[] = {
	"crc_test",
	"fw_debug",
	"attn",
	"layout",
	"senseonoff",
	"debug_level",
	"int_en",
	"register",
	"reset",
	"diag_arr",
	"diag",
	"GMA",
	NULL
};

static int dbg_cmd_flag;
static char *dbg_cmd_par;
static ssize_t (*dbg_func_ptr_r[CMD_NUM])(struct himax_ts_data *ts, char *buf, size_t len);
static ssize_t (*dbg_func_ptr_w[CMD_NUM])(struct himax_ts_data *ts, char *buf, size_t len);
#endif
