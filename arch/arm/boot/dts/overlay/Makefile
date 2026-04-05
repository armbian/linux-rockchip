# SPDX-License-Identifier: GPL-2.0
dtbo-$(CONFIG_ARCH_ROCKCHIP) += \
	rockchip-ebyte-ecb41-pge-spi0-1cs-spidev.dtbo \
	rockchip-ebyte-ecb41-pge-spi0-2cs-spidev.dtbo \
	rockchip-ebyte-ecb41-pge-spi1-1cs-spidev.dtbo \
	rockchip-forlinx-ok3506-s12-spi0-1cs-spidev.dtbo \
	rockchip-forlinx-ok3506-s12-spi0-2cs-spidev.dtbo \
	rockchip-forlinx-ok3506-s12-spi1-1cs-spidev.dtbo \
	rockchip-forlinx-ok3506-s12-spi1-2cs-spidev.dtbo \
	rockchip-luckfox-lyra-plus-spi0-1cs_rmio13-spidev.dtbo \
	rockchip-luckfox-lyra-plus-spi0-1cs-spidev.dtbo \
	rockchip-luckfox-lyra-plus-spi0-2cs-spidev.dtbo \
	rockchip-luckfox-lyra-ultra-w-spi0-1cs-spidev.dtbo \
	rockchip-luckfox-lyra-zero-w-ipex-antenna.dtbo \
	rockchip-luckfox-lyra-zero-w-spi0-1cs-spidev.dtbo \
	rockchip-luckfox-lyra-zero-w-spi0-2cs-spidev.dtbo \
	rockchip-luckfox-lyra-zero-w-spi1-1cs-spidev.dtbo \
	rockchip-luckfox-lyra-zero-w-spi1-2cs-spidev.dtbo \
	rockchip-rk3506-disable-ethernet0.dtbo \
	rockchip-rk3506-disable-ethernet1.dtbo

dtbotxt-$(CONFIG_ARCH_ROCKCHIP) += \
	README.rockchip-overlays

targets += $(dtbo-y) $(dtbotxt-y)

always-y		:= $(dtbo-y) $(dtbotxt-y)
clean-files		:= *.dtbo

