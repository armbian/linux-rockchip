/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 *
 * Author:
 *  Cerf Yu <cerf.yu@rock-chips.com>
 */

#ifndef __LINUX_RKRGA_COMMON_H_
#define __LINUX_RKRGA_COMMON_H_

#include "rga_drv.h"
#include "rga_hw_config.h"

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "%s: " fmt, "rga"

#define rga_log(fmt, args...) \
	pr_info("%-6d %-6d: " fmt, get_current()->tgid, get_current()->pid, ##args)
#define rga_err(fmt, args...) \
	pr_err("%-6d %-6d: " fmt, get_current()->tgid, get_current()->pid, ##args)

#define rga_dev_log(dev, fmt, args...) \
	dev_info(dev, "%-6d %-6d: " fmt, current->tgid, current->pid, ##args)
#define rga_dev_err(dev, fmt, args...) \
	dev_err(dev, "%-6d %-6d: " fmt, current->tgid, current->pid, ##args)

#define rga_job_log(job, fmt, args...) \
	pr_info("%-6d %-6d: ID[%d]: " fmt, job->session->tgid, job->pid, job->request_id, ##args)
#define rga_job_err(job, fmt, args...) \
	pr_err("%-6d %-6d: ID[%d]: " fmt, job->session->tgid, job->pid, job->request_id, ##args)

#define rga_req_log(request, fmt, args...) \
	pr_info("%-6d %-6d: ID[%d]: " fmt, \
	request->session->tgid, request->pid, request->id, ##args)
#define rga_req_err(request, fmt, args...) \
	pr_err("%-6d %-6d: ID[%d]: " fmt, request->session->tgid, request->pid, request->id, ##args)

#define rga_buf_log(buf, fmt, args...) \
	pr_info("%-6d %-6d: handle[%d]: " fmt, \
	buf->session->tgid, current->pid, buf->handle, ##args)
#define rga_buf_err(buf, fmt, args...) \
	pr_err("%-6d %-6d: handle[%d]: " fmt, buf->session->tgid, current->pid, buf->handle, ##args)

#define RGA_GET_PAGE_COUNT(size) (((size) >> PAGE_SHIFT) + (((size) & (~PAGE_MASK)) ? 1 : 0))

bool rga_is_rgb_format(uint32_t format);
bool rga_is_yuv_format(uint32_t format);
bool rga_is_alpha_format(uint32_t format);
bool rga_is_yuv420_packed_format(uint32_t format);
bool rga_is_yuv420_planar_format(uint32_t format);
bool rga_is_yuv420_semi_planar_format(uint32_t format);
bool rga_is_yuv422_packed_format(uint32_t format);
bool rga_is_yuv422_planar_format(uint32_t format);
bool rga_is_yuv422_semi_planar_format(uint32_t format);
bool rga_is_yuv8bit_format(uint32_t format);
bool rga_is_yuv10bit_format(uint32_t format);
bool rga_is_yuv422p_format(uint32_t format);
bool rga_is_only_y_format(uint32_t format);

int rga_get_format_bits(uint32_t format);
int rga_get_pixel_stride_from_format(uint32_t format);

const char *rga_get_format_name(uint32_t format);
const char *rga_get_render_mode_str(uint8_t mode);
const char *rga_get_store_mode_str(uint32_t mode);
const char *rga_get_interp_str(uint8_t interp);
const char *rga_get_rotate_mode_str(uint8_t mode);
const char *rga_get_blend_mode_str(enum rga_alpha_blend_mode mode);
const char *rga_get_memory_type_str(uint8_t type);
const char *rga_get_mmu_type_str(enum rga_mmu mmu_type);
const char *rga_get_dma_data_direction_str(enum dma_data_direction dir);
const char *rga_get_core_name(enum RGA_SCHEDULER_CORE core);

void rga_convert_addr(struct rga_img_info_t *img, bool before_vir_get_channel);
void rga_swap_pd_mode(struct rga_req *req_rga);
int rga_image_size_cal(int w, int h, int format,
		       int *yrgb_size, int *uv_size, int *v_size);
void rga_dump_memory_parm(struct rga_memory_parm *parm);
void rga_dump_external_buffer(struct rga_external_buffer *buffer);
void rga_dump_req(struct rga_request *request, struct rga_req *req);

unsigned long rga_get_free_pages(gfp_t gfp_mask, unsigned int *order, unsigned long size);

#endif
