# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the touchscreen drivers.
#

# Each configuration option enables a list of files.
GCOV_PROFILE := y

obj-$(CONFIG_TOUCHSCREEN_PARADE)      += pt.o
pt-y := pt_core.o pt_mt_common.o
pt-$(CONFIG_TOUCHSCREEN_PARADE_MT_A) += pt_mta.o
pt-$(CONFIG_TOUCHSCREEN_PARADE_MT_B) += pt_mtb.o
pt-$(CONFIG_TOUCHSCREEN_PARADE_BUTTON) += pt_btn.o
pt-$(CONFIG_TOUCHSCREEN_PARADE_PROXIMITY) += pt_proximity.o
pt-$(CONFIG_TOUCHSCREEN_PARADE_DEVICETREE_SUPPORT) += pt_devtree.o
pt-y += pt_platform.o
obj-$(CONFIG_TOUCHSCREEN_PARADE_I2C)  += pt_i2c.o
obj-$(CONFIG_TOUCHSCREEN_PARADE_SPI)  += pt_spi.o
obj-$(CONFIG_TOUCHSCREEN_PARADE_DEBUG_MDL)    += pt_debug.o
obj-$(CONFIG_TOUCHSCREEN_PARADE_LOADER)       += pt_loader.o
obj-$(CONFIG_TOUCHSCREEN_PARADE_DEVICE_ACCESS)          += pt_device_access.o

ifeq ($(CONFIG_TOUCHSCREEN_PARADE_DEBUG),y)
CFLAGS_pt_core.o += -DDEBUG
CFLAGS_pt_i2c.o += -DDEBUG
CFLAGS_pt_spi.o += -DDEBUG
CFLAGS_pt_mta.o += -DDEBUG
CFLAGS_pt_mtb.o += -DDEBUG
CFLAGS_pt_mt_common.o += -DDEBUG
CFLAGS_pt_btn.o += -DDEBUG
CFLAGS_pt_proximity.o += -DDEBUG
CFLAGS_pt_device_access.o += -DDEBUG
CFLAGS_pt_loader.o += -DDEBUG
CFLAGS_pt_debug.o += -DDEBUG
CFLAGS_pt_devtree.o += -DDEBUG
CFLAGS_pt_platform.o += -DDEBUG
endif

ifeq ($(CONFIG_TOUCHSCREEN_PARADE_VDEBUG),y)
CFLAGS_pt_core.o += -DVERBOSE_DEBUG
CFLAGS_pt_i2c.o += -DVERBOSE_DEBUG
CFLAGS_pt_spi.o += -DVERBOSE_DEBUG
CFLAGS_pt_mta.o += -DVERBOSE_DEBUG
CFLAGS_pt_mtb.o += -DVERBOSE_DEBUG
CFLAGS_pt_mt_common.o += -DVERBOSE_DEBUG
CFLAGS_pt_btn.o += -DVERBOSE_DEBUG
CFLAGS_pt_proximity.o += -DVERBOSE_DEBUG
CFLAGS_pt_device_access.o += -DVERBOSE_DEBUG
CFLAGS_pt_loader.o += -DVERBOSE_DEBUG
CFLAGS_pt_debug.o += -DVERBOSE_DEBUG
CFLAGS_pt_devtree.o += -DVERBOSE_DEBUG
CFLAGS_pt_platform.o += -DVERBOSE_DEBUG
endif
