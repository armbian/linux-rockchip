# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_ROCKCHIP_RKNPU3) += rknpu3.o
ccflags-y += -Werror

# ccflags-y += -DDEBUG -g

rknpu3-y := rknpu3_drv.o \
            rknpu3_core.o \
            rknpu3_reset.o \
            rknpu3_task.o \
            rknpu3_memory.o \
            rknpu3_pqueue.o

rknpu3-$(CONFIG_PM_DEVFREQ) += rknpu3_devfreq.o