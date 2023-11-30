# SUBARCH tells the usermode build what the underlying arch is.  That is set
# first, and if a usermode build is happening, the "ARCH=um" on the command
# line overrides the setting of ARCH below.  If a native build is happening,
# then ARCH is assigned, getting whatever value it gets normally, and
# SUBARCH is subsequently ignored.
# eg:
# x86(make modules or make)
# arm(make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules)
SUBARCH := $(shell uname -m | sed -e s/i.86/x86/ -e s/x86_64/x86/ -e s/sun4u/sparc64/ -e s/arm.*/arm/ -e s/sa110/arm/ -e s/s390x/s390/ -e s/parisc64/parisc/ -e s/ppc.*/powerpc/ -e s/mips.*/mips/ -e s/sh[234].*/sh/ -e s/aarch64.*/arm64/ -e s/riscv.*/riscv/)
CURARCH ?= $(SUBARCH)
ARCH ?= $(SUBARCH)
CROSS_COMPILE ?= 
CONFIG_MODULE_SIG=n
EXTRA_CFLAGS = -Wall -g -I$(CURDIR) -I$(subst fuxi-linux-release-package/module_fuxi/src,common,$(PWD)) -I$(PWD)
EXTRA_CFLAGS += -DFXGMAC_DEBUG
KSRC = /lib/modules/`uname -r`/build
KDST = /lib/modules/`uname -r`/kernel/drivers/net/ethernet/motorcomm/
KFILE = yt6801.ko
ifneq ($(filter arm arm64, $(ARCH)),)
	ifeq ($(CURARCH), x86)
		# if you want to compile driver with linux kernel source code,you should specify the source path(KSRC), such as:
		# KSRC = /home/mxl/ti-processor-sdk-linux-am57xx-evm-06.03.00.106/board-support/linux-4.19.94+gitAUTOINC+be5389fd85-gbe5389fd85
	endif
endif
yt6801-objs :=  fuxi-gmac-common.o fuxi-gmac-desc.o fuxi-gmac-ethtool.o fuxi-gmac-hw.o fuxi-gmac-net.o fuxi-gmac-pci.o fuxi-gmac-phy.o fuxi-efuse.o fuxi-dbg.o fuxi-gmac-debugfs.o
obj-m += yt6801.o
modules:
	make -C $(KSRC) M=$(PWD) modules
install:
	sudo install -d $(KDST)
	sudo install $(KFILE) $(KDST)
	sudo depmod -a
	@file $(KDST)$(KFILE)
	@echo install done.
uninstall:
	sudo rm $(KDST)$(KFILE)
	sudo ls -l $(KDST)
clean:
	make -C $(KSRC) M=$(PWD) clean
.PHONY:modules install uninstall clean
