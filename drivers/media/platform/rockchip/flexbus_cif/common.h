/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Rockchip Flexbus CIF Driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef _FLEXBUS_CIF_COMMON_H
#define _FLEXBUS_CIF_COMMON_H
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/media.h>
#include <media/media-device.h>
#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-mc.h>
#include "dev.h"

int flexbus_cif_alloc_buffer(struct flexbus_cif_device *dev,
		       struct flexbus_cif_dummy_buffer *buf);
void flexbus_cif_free_buffer(struct flexbus_cif_device *dev,
			struct flexbus_cif_dummy_buffer *buf);

#endif /* _FLEXBUS_CIF_COMMON_H */

