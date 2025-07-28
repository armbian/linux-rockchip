# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for vehicle driver
#

ccflags-y := -std=gnu99 -Wno-declaration-after-statement -DPB_SYSTEM_HEADER=\"pb_system.h\" -I$(srctree)/drivers/misc/vehicle/nanopb-c/

obj-$(CONFIG_VEHICLE_CORE) += vehicle-core.o
vehicle-core-objs = \
	nanopb-c/pb_common.o \
	nanopb-c/pb_decode.o \
	nanopb-c/pb_encode.o \
	vehicle_core.o \
	vehicle_protocol_callback.o \
	vehiclehalproto.pb.o

obj-$(CONFIG_VEHICLE_DUMMY) += \
	vehicle_dummy_hw.o

obj-$(CONFIG_VEHICLE_ADC) += \
	vehicle-adc.o

obj-$(CONFIG_VEHICLE_GPIO) += \
	vehicle-gpio.o

obj-$(CONFIG_VEHICLE_SPI) += \
	vehicle-spi.o \

obj-$(CONFIG_VEHICLE_SPI_PROTOCOL) += \
	vehicle_spi_protocol.o

obj-$(CONFIG_VEHICLE_GPIO_MCU_EXPANDER) += \
	vehicle-gpio-mcu.o
