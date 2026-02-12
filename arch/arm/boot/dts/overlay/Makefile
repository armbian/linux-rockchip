# SPDX-License-Identifier: GPL-2.0
dtbo-$(CONFIG_ARCH_ROCKCHIP) += \
	rockchip-rk3506-disable-ethernet0.dtbo \
	rockchip-rk3506-disable-ethernet1.dtbo

dtbotxt-$(CONFIG_ARCH_ROCKCHIP) += \
	README.rockchip-overlays

targets += $(dtbo-y) $(dtbotxt-y)

always-y		:= $(dtbo-y) $(dtbotxt-y)
clean-files		:= *.dtbo

