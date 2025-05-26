/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 * Author: Zain Wang <zain.wang@rock-chips.com>
 */

#ifndef __ROCKCHIP_AMP_RPMSG_SHAREDMEM_H__
#define __ROCKCHIP_AMP_RPMSG_SHAREDMEM_H__

#include "rockchip_amp_rpmsg_sharedmem_message.h"
/*  Kernel                                    |    MCU
 *--------------------------------------------|--------------------------------
 *                                            |   register device_node
 * *  -- function: rkamp_sharedmem_register   |
 *  1. alloc sharedmem                        |
 *  2. register (notify) callback function    |
 *  2. connected: send device information   --|-> device_node evaluation,
 *                                            |   start thread to handle rpmsg,
 *                                            |   and return connection result.
 *  3. return connection result               |
 * -------------------------------------------|--------------------------------
 * -- function: rkamp_sharedmem_send_notify   |
 * Send message to MCU device_node.         --|->  handle rpmsg (notify),
 * Lower bandwidth than Sharedmem,            |
 * send some controller command.              |
 * -------------------------------------------|--------------------------------
 * -- function: (notify)                    <-|-- Send rpmsg command.
 * -- register by rkamp_sharedmem_register
 * Handle RPMSG.
 */

#define RKAMP_SHAREDMEM_MAX_NODE       (16)
#define RKAMP_SHAREDMEM_MAX_NAME_LEN   (64)
struct rkamp_sharedmem_node {
	char name[RKAMP_SHAREDMEM_MAX_NAME_LEN];
	int name_size;
	bool enable;
	int id;
	// uint64_t addr_offset;
	dma_addr_t dma_pa;
	void *dma_va;
	uint64_t size;
	void (*notify)(void *priv, void *payload, int len);
	void *priv;
	struct device *dev;
};

struct device *rkamp_sharedmem_get_device(const char *name);
struct rkamp_sharedmem_node *rkamp_sharedmem_register(struct device *dev,
				int size, char *name, int name_size,
				void (*notify)(void *priv, void *payload, int len),
				void *priv);
int rkamp_sharedmem_unregister(struct rkamp_sharedmem_node *node);
void rkamp_sharedmem_flush(struct rkamp_sharedmem_node *node,
			   int offset, int size);
void rkamp_sharedmem_invalidate(struct rkamp_sharedmem_node *node,
				int offset, int size);
int rkamp_sharedmem_send_notify(struct rkamp_sharedmem_node *node, void *payload, int len);

#endif /* __ROCKCHIP_AMP_RPMSG_SHAREDMEM_H__ */
