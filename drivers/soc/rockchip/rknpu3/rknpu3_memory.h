/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 *
 * RKNPU Memory Management Header
 */

#ifndef __RKNPU3_MEMORY_H__
#define __RKNPU3_MEMORY_H__

#include "rknpu3_types.h"
#include "rknpu3_ioctl.h"

int rknpu3_memory_module_init(struct rknpu3_device *dev);
void rknpu3_memory_module_deinit(struct rknpu3_device *dev);
int rknpu3_memory_alloc(struct rknpu3_device *dev, struct rknpu3_mem_create *mem_create);
int rknpu3_memory_free(struct rknpu3_device *dev, struct rknpu3_mem_create *mem_create);
int rknpu3_memory_sync(struct rknpu3_session *session, struct rknpu3_mem_sync *mem_sync);
int rknpu3_memory_get_all_core_usage(struct rknpu3_device *dev,
				     struct rknpu3_mem_usage *usage);
int rknpu3_memory_import_dmabuf(struct rknpu3_session *session,
				struct rknpu3_mem_import *mem_import);
int rknpu3_memory_release_imported(struct rknpu3_session *session, uint64_t handle);
void rknpu3_session_release_all_imports(struct rknpu3_session *session);
void *rknpu3_memory_get_kernel_addr(int dma_buf_fd);

#endif /* __RKNPU3_MEMORY_H__ */
