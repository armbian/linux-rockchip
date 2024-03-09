CONFIG_AIC8800_BTLPM_SUPPORT = m

obj-$(CONFIG_AIC8800_BTLPM_SUPPORT) := aic8800_btlpm.o

ccflags-y += -I$(srctree)/$(src)/../aic8800_bsp

# Platform support list
CONFIG_PLATFORM_ROCKCHIP ?= n
CONFIG_PLATFORM_ROCKCHIP2 ?= n
CONFIG_PLATFORM_ALLWINNER ?= n
CONFIG_PLATFORM_AMLOGIC ?= n
CONFIG_PLATFORM_UBUNTU ?= y


CONFIG_SUPPORT_LPM = y
CONFIG_AUTO_PM ?= n

aic8800_btlpm-y := \
	aic_bluetooth_main.o \
	rfkill.o \

ifeq ($(CONFIG_PLATFORM_UBUNTU), n)
	aic8800_btlpm-$(CONFIG_SUPPORT_LPM) += lpm.o
endif

ccflags-y += -DAIC_TRACE_INCLUDE_PATH=$(src)

ccflags-$(CONFIG_AUTO_PM) += -DCONFIG_AUTO_PM
