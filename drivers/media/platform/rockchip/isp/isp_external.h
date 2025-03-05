/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2021 Rockchip Electronics Co., Ltd. */

#ifndef _RKISP_EXTERNAL_H
#define _RKISP_EXTERNAL_H

#include <linux/iopoll.h>

#define RKISP_VICAP_CMD_MODE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 0, struct rkisp_vicap_mode)

#define RKISP_VICAP_CMD_INIT_BUF \
	 _IOW('V', BASE_VIDIOC_PRIVATE + 1, struct rkisp_init_buf)

#define RKISP_VICAP_CMD_RX_BUFFER_FREE \
	 _IOW('V', BASE_VIDIOC_PRIVATE + 2, struct rkisp_rx_buf)

#define RKISP_VICAP_CMD_QUICK_STREAM \
	_IOW('V', BASE_VIDIOC_PRIVATE + 3, int)

#define RKISP_VICAP_CMD_SET_RESET \
	 _IOW('V', BASE_VIDIOC_PRIVATE + 4, int)

#define RKISP_VICAP_CMD_SET_STREAM \
	 _IOW('V', BASE_VIDIOC_PRIVATE + 5, int)

#define RKISP_VICAP_CMD_HW_LINK \
	_IOW('V', BASE_VIDIOC_PRIVATE + 6, int)

#define RKISP_VICAP_CMD_SOF \
	_IOW('V', BASE_VIDIOC_PRIVATE + 7, struct rkisp_vicap_sof)

#define RKISP_VICAP_CMD_MULTI_ONLINE \
	_IOW('V', BASE_VIDIOC_PRIVATE + 8, int)

#define RKISP_VICAP_BUF_CNT 3
#define RKISP_VICAP_BUF_CNT_MAX 8
#define RKISP_RX_BUF_POOL_MAX (RKISP_VICAP_BUF_CNT_MAX * 3)

#define rkisp_cond_poll_timeout(cond, sleep_us, timeout_us) \
({ \
	int __val; \
	read_poll_timeout((int), __val, cond, sleep_us, timeout_us, false, 0); \
})

struct rkisp_vicap_input {
	u8 merge_num;
	u8 index;
	u8 multi_sync;
};

enum rkisp_vicap_link {
	RKISP_VICAP_ONLINE,
	RKISP_VICAP_ONLINE_ONE_FRAME,
	RKISP_VICAP_ONLINE_MULTI,
	RKISP_VICAP_ONLINE_UNITE,
	RKISP_VICAP_RDBK_AIQ,
	RKISP_VICAP_RDBK_AUTO,
	RKISP_VICAP_RDBK_AUTO_ONE_FRAME,
};

struct rkisp_vicap_mode {
	/* copy rkisp_device name */
	char name[128];
	enum rkisp_vicap_link rdbk_mode;

	struct rkisp_vicap_input input;
	int dev_id;
};

struct rkisp_init_buf {
	u32 buf_cnt;
	u32 hdr_wrap_line;
};

enum rx_buf_type {
	BUF_SHORT,
	BUF_MIDDLE,
	BUF_LONG,
};

struct rkisp_rx_buf {
	struct list_head list;
	struct dma_buf *dbuf;
	dma_addr_t dma;
	u64 timestamp;
	u32 sequence;
	u32 type;
	u32 runtime_us;

	bool is_init;
	bool is_first;

	bool is_resmem;
	bool is_switch;

	bool is_uncompact;
};

struct rkisp_vicap_sof {
	u64 timestamp;
	u32 sequence;
	u32 exp[3];
	u32 gain[3];
	u32 hts;
	u32 vts;
	u32 pclk;
	__u32 dcg_used;
	__u32 dcg_val[3];
	struct rkmodule_dcg_ratio dcg_ratio;
	struct rkmodule_gain_mode gain_mode;
	bool is_exp_active;
};

#endif
