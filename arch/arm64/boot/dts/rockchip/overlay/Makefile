# SPDX-License-Identifier: GPL-2.0
dtbo-$(CONFIG_ARCH_ROCKCHIP) += \
	orangepi-5-lcd1.dtbo \
	orangepi-5-lcd2.dtbo \
	orangepi-5-sata.dtbo \
	rock-4ab-raspi-7inch-touchscreen.dtbo \
	rock-4c-plus-radxa-display-10fhd.dtbo \
	rock-4c-plus-radxa-display-10hd.dtbo \
	rock-4c-plus-radxa-display-8hd.dtbo \
	rock-4c-plus-raspi-7inch-touchscreen.dtbo \
	rock-4-okdo-5mp-camera.dtbo \
	rock-4-rpi-camera-v1p3.dtbo \
	rock-4-rpi-camera-v2.dtbo \
	rock-5a-hdmi-8k.dtbo \
	rock-5a-i2c5-rtc-hym8563.dtbo \
	rock-5a-radxa-camera-4k.dtbo \
	rock-5a-radxa-display-10hd.dtbo \
	rock-5a-radxa-display-8hd.dtbo \
	rock-5a-spi-nor-flash.dtbo \
	rock-5a-usb-otg.dtbo \
	rock-5b-hdmi1-8k.dtbo \
	rock-5b-hdmi2-8k.dtbo \
	rock-5b-pd-max-voltage-12v.dtbo \
	rock-5b-radxa-display-10hd.dtbo \
	rock-5b-radxa-display-8hd.dtbo \
	rock-5b-rpi-camera-v2.dtbo \
	rock-5b-radxa-camera-4k.dtbo \
	rock-5b-sata.dtbo \
	rockchip-rk3588-opp-oc-24ghz.dtbo

dtbotxt-$(CONFIG_ARCH_ROCKCHIP) += \
	README.rockchip-overlays

targets += $(dtbo-y) $(dtbotxt-y)

always-y		:= $(dtbo-y) $(dtbotxt-y)
clean-files		:= *.dtbo

