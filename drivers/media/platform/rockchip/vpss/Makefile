# SPDX-License-Identifier: GPL-2.0

GIT_LOG_RAW := $(shell git log -1 --no-decorate --pretty=format:"%h %s" -- $(src) 2>/dev/null || \
	echo "unavailable")
GIT_LOG_VPSS_CFG_RAW := $(shell git log -1 --no-decorate --pretty=format:"%h" -- $(srctree)/include/uapi/linux/rk-vpss-config.h 2>/dev/null || \
	echo "unavailable")

GIT_LOG_0 := $(subst \,\\\,$(GIT_LOG_RAW))
GIT_LOG := $(subst ",\\\",$(GIT_LOG_0))

GIT_LOG_VPSS_CFG_0 := $(subst \,\\\,$(GIT_LOG_VPSS_CFG_RAW))
GIT_LOG_VPSS_CFG := $(subst ",\\\",$(GIT_LOG_VPSS_CFG_0))

ccflags-y += -DGIT_LOG=\""$(GIT_LOG)"\"
ccflags-y += -DGIT_LOG_VPSS_CFG=\""$(GIT_LOG_VPSS_CFG)"\"

obj-$(CONFIG_VIDEO_ROCKCHIP_VPSS) += video_rkvpss.o

video_rkvpss-objs += hw.o \
		dev.o \
		common.o \
		vpss.o \
		stream.o \
		procfs.o \
		vpss_offline.o

video_rkvpss-$(CONFIG_VIDEO_ROCKCHIP_VPSS_V10) += \
						stream_v10.o \
						vpss_offline_v10.o

video_rkvpss-$(CONFIG_VIDEO_ROCKCHIP_VPSS_V20) += \
						stream_v20.o \
						vpss_offline_v20.o \
						vpss_offline_rockit.o \
						vpss_rockit.o

video_rkvpss-$(CONFIG_VIDEO_ROCKCHIP_VPSS_V21) += \
						stream_v21.o \
						vpss_offline_v21.o

video_rkvpss-$(CONFIG_ROCKCHIP_DVBM) += \
					vpss_dvbm.o \
					vpss_offline_dvbm.o
