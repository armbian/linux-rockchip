/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 *
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM rockchipdrm

#if !defined(_ROCKCHIP_DRM_TRACE_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _ROCKCHIP_DRM_TRACE_H_

#include <linux/tracepoint.h>

#define ROCKCHIP_DRM_MAX_MSG_LEN	256

DECLARE_EVENT_CLASS(rockchipdrm_dbg,

	    TP_PROTO(struct va_format *vaf),

	    TP_ARGS(vaf),

	    TP_STRUCT__entry(
		    __dynamic_array(char, msg, ROCKCHIP_DRM_MAX_MSG_LEN)
	    ),

	    TP_fast_assign(
		    WARN_ON_ONCE(vsnprintf(__get_dynamic_array(msg),
					   ROCKCHIP_DRM_MAX_MSG_LEN,
					   vaf->fmt,
					   *vaf->va) >= ROCKCHIP_DRM_MAX_MSG_LEN);
	    ),

	    TP_printk("%s", __get_str(msg))
);

DEFINE_EVENT(rockchipdrm_dbg, rockchip_drm_dbg_common,
	     TP_PROTO(struct va_format *vaf),
	     TP_ARGS(vaf)
);

DEFINE_EVENT(rockchipdrm_dbg, rockchip_drm_dbg_iommu,
	     TP_PROTO(struct va_format *vaf),
	     TP_ARGS(vaf)
);

DEFINE_EVENT(rockchipdrm_dbg, rockchip_drm_dbg_vsync,
	     TP_PROTO(struct va_format *vaf),
	     TP_ARGS(vaf)
);

#endif /* _ROCKCHIP_DRM_TRACE_H_ || TRACE_HEADER_MULTI_READ */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/gpu/drm/rockchip
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE rockchip_drm_trace

/* This part must be outside protection */
#include <trace/define_trace.h>
