# SPDX-License-Identifier: GPL-2.0
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

video_rkvpss-$(CONFIG_ROCKCHIP_DVBM) += \
					vpss_dvbm.o \
					vpss_offline_dvbm.o
