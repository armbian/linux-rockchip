ifeq ($(CONFIG_PLATFORM_AML_S905), y)
EXTRA_CFLAGS += -DCONFIG_PLATFORM_AML_S905
EXTRA_CFLAGS += -DCONFIG_LITTLE_ENDIAN
EXTRA_CFLAGS += -DCONFIG_IOCTL_CFG80211 -DRTW_USE_CFG80211_STA_EVENT
EXTRA_CFLAGS += -DCONFIG_RADIO_WORK
EXTRA_CFLAGS += -DCONFIG_CONCURRENT_MODE
ifeq ($(shell test $(CONFIG_RTW_ANDROID) -ge 11; echo $$?), 0)
EXTRA_CFLAGS += -DCONFIG_IFACE_NUMBER=3
EXTRA_CFLAGS += -DCONFIG_SEL_P2P_IFACE=1
endif

# default setting for Android
# config CONFIG_RTW_ANDROID in main Makefile

ARCH ?= arm64
CROSS_COMPILE ?= /home/Jimmy/amlogic_905x4+8852AS/skw-rtk/cross_compile_toolchain/gcc-linaro-6.3.1-2017.02-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
ifndef KSRC
KSRC := /home/Jimmy/amlogic_905x4+8852AS/skw-rtk/kernel
# To locate output files in a separate directory.
KSRC += O=/home/Jimmy/amlogic_905x4+8852AS/skw-rtk/kernel_obj
endif

ifeq ($(CONFIG_PCI_HCI), y)
EXTRA_CFLAGS += -DCONFIG_PLATFORM_OPS
_PLATFORM_FILES := platform/platform_linux_pc_pci.o
OBJS += $(_PLATFORM_FILES)
# Core Config
EXTRA_CFLAGS += -DCONFIG_RXBUF_NUM_1024
EXTRA_CFLAGS += -DCONFIG_TX_SKB_ORPHAN
# PHL Config
EXTRA_CFLAGS += -DRTW_WKARD_98D_RXTAG
endif

ifeq ($(CONFIG_RTL8852A), y)
ifeq ($(CONFIG_SDIO_HCI), y)
CONFIG_RTL8852AS ?= m
USER_MODULE_NAME := 8852as
endif
ifeq ($(CONFIG_PCI_HCI), y)
CONFIG_RTL8852AE ?= m
USER_MODULE_NAME := 8852ae
endif
endif

endif
