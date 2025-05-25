// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 *
 * Author: Huang Lee <Putin.li@rock-chips.com>
 */

#include "rga_job.h"
#include "rga_common.h"
#include "rga_hw_config.h"
#include "rga_debugger.h"

#define GET_GCD(n1, n2) \
	({ \
		int i; \
		int gcd = 1; \
		for (i = 1; i <= (n1) && i <= (n2); i++) { \
			if ((n1) % i == 0 && (n2) % i == 0) \
				gcd = i; \
		} \
		gcd; \
	})
#define GET_LCM(n1, n2, gcd) (((n1) * (n2)) / gcd)

static int rga_set_feature(struct rga_req *rga_base)
{
	int feature = 0;

	if (rga_base->render_mode == COLOR_FILL_MODE)
		feature |= RGA_COLOR_FILL;

	if (rga_base->render_mode == COLOR_PALETTE_MODE)
		feature |= RGA_COLOR_PALETTE;

	if (rga_base->color_key_max > 0 || rga_base->color_key_min > 0)
		feature |= RGA_COLOR_KEY;

	if ((rga_base->alpha_rop_flag >> 1) & 1)
		feature |= RGA_ROP_CALCULATE;

	if ((rga_base->alpha_rop_flag >> 8) & 1)
		feature |= RGA_NN_QUANTIZE;

	return feature;
}

static bool rga_check_csc(const struct rga_hw_data *data, struct rga_req *rga_base)
{
	switch (rga_base->yuv2rgb_mode & RGA_Y2R_MASK) {
	case RGA_Y2R_BT601_LIMIT:
		if (!(data->csc_y2r_mode & RGA_MODE_CSC_BT601L))
			return false;

		break;
	case RGA_Y2R_BT601_FULL:
		if (!(data->csc_y2r_mode & RGA_MODE_CSC_BT601F))
			return false;

		break;
	case RGA_Y2R_BT709_LIMIT:
		if (!(data->csc_y2r_mode & RGA_MODE_CSC_BT709))
			return false;

		break;
	default:
		break;
	}

	switch (rga_base->yuv2rgb_mode & RGA_R2Y_MASK) {
	case RGA_R2Y_BT601_LIMIT:
		if (!(data->csc_r2y_mode & RGA_MODE_CSC_BT601L))
			return false;

		break;
	case RGA_R2Y_BT601_FULL:
		if (!(data->csc_r2y_mode & RGA_MODE_CSC_BT601F))
			return false;

		break;
	case RGA_R2Y_BT709_LIMIT:
		if (!(data->csc_r2y_mode & RGA_MODE_CSC_BT709))
			return false;

		break;
	default:
		break;
	}


	if ((rga_base->full_csc.flag & 0x1) && !(data->feature & RGA_FULL_CSC))
		return false;

	return true;
}

static bool rga_check_resolution(const struct rga_rect_range *range, int width, int height)
{
	if (width > range->max.width || height > range->max.height)
		return false;

	if (width < range->min.width || height < range->min.height)
		return false;

	return true;
}

static bool rga_check_format(const struct rga_hw_data *data,
		int rd_mode, int format, int win_num)
{
	int i;
	const uint32_t *formats;
	uint32_t format_count;

	switch (rd_mode) {
	case RGA_RASTER_MODE:
		formats = data->win[win_num].formats[RGA_RASTER_INDEX];
		format_count = data->win[win_num].formats_count[RGA_RASTER_INDEX];
		break;
	case RGA_FBC_MODE:
		formats = data->win[win_num].formats[RGA_AFBC16x16_INDEX];
		format_count = data->win[win_num].formats_count[RGA_AFBC16x16_INDEX];
		break;
	case RGA_TILE_MODE:
		formats = data->win[win_num].formats[RGA_TILE8x8_INDEX];
		format_count = data->win[win_num].formats_count[RGA_TILE8x8_INDEX];
		break;
	case RGA_TILE4x4_MODE:
		formats = data->win[win_num].formats[RGA_TILE4x4_INDEX];
		format_count = data->win[win_num].formats_count[RGA_TILE4x4_INDEX];
		break;
	case RGA_RKFBC_MODE:
		formats = data->win[win_num].formats[RGA_RKFBC64x4_INDEX];
		format_count = data->win[win_num].formats_count[RGA_RKFBC64x4_INDEX];
		break;
	case RGA_AFBC32x8_MODE:
		formats = data->win[win_num].formats[RGA_AFBC32x8_INDEX];
		format_count = data->win[win_num].formats_count[RGA_AFBC32x8_INDEX];
		break;
	default:
		return false;
	}

	if (formats == NULL || format_count == 0)
		return false;

	for (i = 0; i < format_count; i++)
		if (format == formats[i])
			return true;

	return false;
}

static bool rga_check_align(struct rga_job *job,
			    uint32_t byte_stride_align, uint32_t format, uint16_t w_stride)
{
	int bit_stride, pixel_stride, align, gcd;

	pixel_stride = rga_get_pixel_stride_from_format(format);
	if (pixel_stride <= 0)
		return false;

	bit_stride = pixel_stride * w_stride;

	if (bit_stride % (byte_stride_align * 8) == 0)
		return true;

	if (DEBUGGER_EN(MSG)) {
		gcd = GET_GCD(pixel_stride, byte_stride_align * 8);
		align = GET_LCM(pixel_stride, byte_stride_align * 8, gcd) / pixel_stride;
		rga_job_log(job, "unsupported width stride %d, 0x%x should be %d aligned!",
			w_stride, format, align);
	}

	return false;
}

static bool rga_check_channel(struct rga_job *job, const struct rga_hw_data *data,
			      struct rga_img_info_t *img,
			      const char *name, int input, int swap, int win_num)
{
	int w, h;
	const struct rga_rect_range *range;

	if (input)
		range = &data->input_range;
	else
		range = &data->output_range;

	if (swap) {
		w = img->act_h;
		h = img->act_w;
	} else {
		w = img->act_w;
		h = img->act_h;
	}

	if (!rga_check_resolution(range, w, h)) {
		if (DEBUGGER_EN(MSG))
			rga_job_log(job, "%s resolution check error, input range[%dx%d ~ %dx%d], [w,h] = [%d, %d]\n",
				name,
				data->input_range.min.width, data->input_range.min.height,
				data->input_range.max.width, data->input_range.max.height,
				w, h);

		return false;
	}

	if (data == &rga3_data &&
	    !rga_check_resolution(&data->input_range,
				  w + img->x_offset,
				  h + img->y_offset)) {
		if (DEBUGGER_EN(MSG))
			rga_job_log(job, "%s RGA3 resolution check error, input range[%dx%d ~ %dx%d], [w+x,h+y] = [%d, %d]\n",
				name,
				data->input_range.min.width, data->input_range.min.height,
				data->input_range.max.width, data->input_range.max.height,
				w + img->x_offset,
				h + img->y_offset);
		return false;
	}

	if (!rga_check_format(data, img->rd_mode, img->format, win_num)) {
		if (DEBUGGER_EN(MSG))
			rga_job_log(job, "%s format check error, mode = %#x, format = %#x\n",
				name, img->rd_mode, img->format);
		return false;
	}

	if (!rga_check_align(job, data->byte_stride_align, img->format, img->vir_w)) {
		if (DEBUGGER_EN(MSG))
			rga_job_log(job, "%s align check error, byte_stride_align[%d], format[%#x], vir_w[%d]\n",
				name, data->byte_stride_align, img->format, img->vir_w);
		return false;
	}

	return true;
}

static bool rga_check_scale(struct rga_job *job, const struct rga_hw_data *data,
			    struct rga_req *rga_base)
{
	struct rga_img_info_t *src0 = &rga_base->src;
	struct rga_img_info_t *dst = &rga_base->dst;

	int sw, sh;
	int dw, dh;

	sw = src0->act_w;
	sh = src0->act_h;
	dw = dst->act_w;
	dh = dst->act_h;

	if (sw > dw) {
		if ((sw >> data->max_downscale_factor) > dw)
			goto check_error;
	} else if (sw < dw) {
		if ((sw << data->max_upscale_factor) < dw)
			goto check_error;
	}

	if (sh > dh) {
		if ((sh >> data->max_downscale_factor) > dh)
			goto check_error;
	} else if (sh < dh) {
		if ((sh << data->max_upscale_factor) < dh)
			goto check_error;
	}

	return true;
check_error:
	if (DEBUGGER_EN(MSG))
		rga_job_log(job, "scale check error, scale limit[1/%d ~ %d], src[%d, %d], dst[%d, %d]\n",
			(1 << data->max_downscale_factor), (1 << data->max_upscale_factor),
			sw, sh, dw, dh);

	return false;
}

static bool rga_check_rotate(struct rga_job *job, const struct rga_hw_data *data,
			    struct rga_req *rga_base)
{
	/* rot-90 and rot-270 */
	if (((rga_base->rotate_mode & 0x0f) == 1) &&
	    ((rga_base->sina == 65536 && rga_base->cosa == 0) ||
	     (rga_base->sina == -65536 && rga_base->cosa == 0))) {
		if (data == &rga3_data &&
		    (rga_is_yuv422_packed_format(rga_base->src.format) ||
		     rga_is_yuv422_semi_planar_format(rga_base->src.format))) {
			if (DEBUGGER_EN(MSG))
				rga_job_log(job, "rotate check error, RGA3 unsupported YUV422 rotate 90/270, format[%s(%#x)]\n",
					    rga_get_format_name(rga_base->src.format),
					    rga_base->src.format);
			return false;
		}
	}

	return true;
}

int rga_job_assign(struct rga_job *job)
{
	struct rga_img_info_t *src0 = &job->rga_command_base.src;
	struct rga_img_info_t *src1 = &job->rga_command_base.pat;
	struct rga_img_info_t *dst = &job->rga_command_base.dst;

	struct rga_req *rga_base = &job->rga_command_base;
	const struct rga_hw_data *data;
	struct rga_scheduler_t *scheduler = NULL;

	int feature;
	int core = RGA_NONE_CORE;
	int optional_cores = RGA_NONE_CORE;
	int specified_cores = RGA_NONE_CORE;
	int i;
	int min_of_job_count = -1;
	unsigned long flags;
	int need_swap = false;

	/* assigned by userspace */
	if (rga_base->core > RGA_NONE_CORE) {
		if (rga_base->core > RGA_CORE_MASK) {
			rga_job_err(job, "invalid setting core by user\n");
			return -1;
		} else if (rga_base->core & RGA_CORE_MASK)
			specified_cores = rga_base->core;
	}

	feature = rga_set_feature(rga_base);

	/* function */
	for (i = 0; i < rga_drvdata->num_of_scheduler; i++) {
		data = rga_drvdata->scheduler[i]->data;
		scheduler = rga_drvdata->scheduler[i];

		if ((specified_cores != RGA_NONE_CORE) &&
			(!(scheduler->core & specified_cores)))
			continue;

		if (DEBUGGER_EN(MSG))
			rga_job_log(job, "start policy on %s(%#x)",
				rga_get_core_name(scheduler->core), scheduler->core);

		if (scheduler->data->mmu == RGA_MMU &&
		    job->flags & RGA_JOB_UNSUPPORT_RGA_MMU) {
			if (DEBUGGER_EN(MSG))
				rga_job_log(job, "RGA2 only support under 4G memory!\n");
			continue;
		}

		if (feature > 0) {
			if (!(feature & data->feature)) {
				if (DEBUGGER_EN(MSG))
					rga_job_log(job, "%s(%#x), break on feature\n",
						rga_get_core_name(scheduler->core),
						scheduler->core);
				continue;
			}
		}

		/* some mode rotate 90/270 need swap dst_width/dst_height  */
		if (((rga_base->rotate_mode & 0x0f) == 1) &&
			((rga_base->sina == 65536 && rga_base->cosa == 0) ||
			(rga_base->sina == -65536 && rga_base->cosa == 0))) {
			need_swap = true;
		}

		/* only colorfill need single win (colorpalette?) */
		if (!(feature & 1)) {
			if (src1->yrgb_addr > 0) {
				if (!(src0->rd_mode & data->win[0].rd_mode)) {
					if (DEBUGGER_EN(MSG))
						rga_job_log(job, "%s(%#x), src0 break on %s(%#x)\n",
							rga_get_core_name(scheduler->core),
							scheduler->core,
							rga_get_store_mode_str(src0->rd_mode),
							src0->rd_mode);
					continue;
				}

				if (!(src1->rd_mode & data->win[1].rd_mode)) {
					if (DEBUGGER_EN(MSG))
						rga_job_log(job, "%s(%#x), src1 break on %s(%#x)\n",
							rga_get_core_name(scheduler->core),
							scheduler->core,
							rga_get_store_mode_str(src1->rd_mode),
							src1->rd_mode);
					continue;
				}

				if (!(dst->rd_mode & data->win[2].rd_mode)) {
					if (DEBUGGER_EN(MSG))
						rga_job_log(job, "%s(%#x), dst break on %s(%#x)\n",
							rga_get_core_name(scheduler->core),
							scheduler->core,
							rga_get_store_mode_str(dst->rd_mode),
							dst->rd_mode);
					continue;
				}
			} else {
				if (!(src0->rd_mode & data->win[0].rd_mode)) {
					if (DEBUGGER_EN(MSG))
						rga_job_log(job, "%s(%#x), src break on %s(%#x)\n",
							rga_get_core_name(scheduler->core),
							scheduler->core,
							rga_get_store_mode_str(src0->rd_mode),
							src0->rd_mode);
					continue;
				}

				if (!(dst->rd_mode & data->win[2].rd_mode)) {
					if (DEBUGGER_EN(MSG))
						rga_job_log(job, "%s(%#x), dst break on %s(%#x)\n",
							rga_get_core_name(scheduler->core),
							scheduler->core,
							rga_get_store_mode_str(dst->rd_mode),
							dst->rd_mode);
					continue;
				}
			}

			if (!rga_check_scale(job, data, rga_base)) {
				if (DEBUGGER_EN(MSG))
					rga_job_log(job, "%s(%#x), break on rga_check_scale",
						rga_get_core_name(scheduler->core),
						scheduler->core);
				continue;
			}

			if (!rga_check_channel(job, data, src0, "src0", true, false, 0)) {
				if (DEBUGGER_EN(MSG))
					rga_job_log(job, "%s(%#x), break on src0",
						rga_get_core_name(scheduler->core),
						scheduler->core);
				continue;
			}

			if (src1->yrgb_addr > 0) {
				if (!rga_check_channel(job, data, src1, "src1", true, false, 1)) {
					if (DEBUGGER_EN(MSG))
						rga_job_log(job, "%s(%#x), break on src1",
							rga_get_core_name(scheduler->core),
							scheduler->core);
					continue;
				}
			}
		}

		if (!rga_check_channel(job, data, dst, "dst", false, need_swap, 2)) {
			if (DEBUGGER_EN(MSG))
				rga_job_log(job, "%s(%#x), break on dst",
					rga_get_core_name(scheduler->core),
					scheduler->core);
			continue;
		}

		if (!rga_check_csc(data, rga_base)) {
			if (DEBUGGER_EN(MSG))
				rga_job_log(job, "%s(%#x), break on rga_check_csc",
					rga_get_core_name(scheduler->core),
					scheduler->core);
			continue;
		}

		if (!rga_check_rotate(job, data, rga_base)) {
			if (DEBUGGER_EN(MSG))
				rga_job_log(job, "%s(%#x), break on rga_check_rotate",
					rga_get_core_name(scheduler->core),
					scheduler->core);
			continue;
		}

		optional_cores |= scheduler->core;
	}

	if (optional_cores == 0) {
		rga_job_err(job, "no core match\n");
		return -1;
	}

	for (i = 0; i < rga_drvdata->num_of_scheduler; i++) {
		scheduler = rga_drvdata->scheduler[i];

		if (optional_cores & scheduler->core) {
			spin_lock_irqsave(&scheduler->irq_lock, flags);

			if (scheduler->running_job == NULL) {
				core = scheduler->core;
				job->scheduler = scheduler;
				spin_unlock_irqrestore(&scheduler->irq_lock,
							 flags);
				break;
			} else {
				if ((min_of_job_count == -1) ||
				    (min_of_job_count > scheduler->job_count)) {
					min_of_job_count = scheduler->job_count;
					core = scheduler->core;
					job->scheduler = scheduler;
				}
			}

			spin_unlock_irqrestore(&scheduler->irq_lock, flags);
		}
	}

	/* TODO: need consider full load */
	if (DEBUGGER_EN(MSG))
		rga_job_log(job, "matched cores = %#x, assign core: %s(%#x)\n",
			optional_cores,
			rga_get_core_name(core), core);

	return core;
}
