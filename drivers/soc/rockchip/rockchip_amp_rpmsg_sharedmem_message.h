/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 * Author: Zain Wang <zain.wang@rock-chips.com>
 */

#ifndef __ROCKCHIP_AMP_RPMSG_SHAREDMEM_MESSAGE_H__
#define __ROCKCHIP_AMP_RPMSG_SHAREDMEM_MESSAGE_H__

#define RKAMP_SHAREDMEM_MAMANGER_ID	(0)

#define RKAMP_SHAREDMEM_CMD_ERROR	(0)
#define RKAMP_SHAREDMEM_CMD_REGISTER	(1)
#define RKAMP_SHAREDMEM_CMD_UNREGISTER	(2)

#define RKAMP_SHAREDMEM_MSG_DATA_LEN	(492)
#define RKAMP_SHAREDMEM_MMSG_DATA_LEN	(488)
#define RKAMP_SHAREDMEM_EMSG_DATA_LEN	(480)
#define RKAMP_SHAREDMEM_NODE_NAME_LEN	(64)

struct rkamp_sharedmem_register_msg {
	uint32_t name_size;
	uint64_t sm_addr;
	uint64_t sm_size;
	uint8_t name[64];
} __packed;

#define RKAMP_SHM_ERROR_HEAD		(8)
struct rkamp_sharedmem_err_msg {
	int32_t error;
	uint32_t len;
	char msg[RKAMP_SHAREDMEM_EMSG_DATA_LEN];
} __packed;

struct rkamp_sharedmem_manager_msg {
	uint32_t command;
	union {
		uint8_t data[RKAMP_SHAREDMEM_MMSG_DATA_LEN];
		struct rkamp_sharedmem_register_msg register_msg;
		struct rkamp_sharedmem_err_msg err_msg;
	};
} __packed;

struct rkamp_sharedmem_msg {
	uint32_t id;
	union {
		uint8_t data[RKAMP_SHAREDMEM_MSG_DATA_LEN];
		struct rkamp_sharedmem_manager_msg mmsg;
	};
} __packed;

#endif /* __ROCKCHIP_AMP_RPMSG_SHAREDMEM_MESSAGE_H__ */
