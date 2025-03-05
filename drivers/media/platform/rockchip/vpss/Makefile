# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_VIDEO_ROCKCHIP_VPSS) += video_rkvpss.o

video_rkvpss-objs += hw.o \
		dev.o \
		common.o \
		vpss.o \
		stream.o \
		procfs.o \
		vpss_offline.o

video_rkvpss-$(CONFIG_VIDEO_ROCKCHIP_VPSS_V1) += \
						stream_v1.o \
						vpss_offline_v1.o
