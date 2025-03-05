#SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Himax touchscreen drivers.
#
ifneq ($(filter y, $(CONFIG_KALLSYMS_ALL)),)
	ccflags-y += -D__KERNEL_KALLSYMS_ALL_ENABLED__
endif

ifneq ($(filter y, $(CONFIG_FB)),)
	ccflags-y += -DHX_CONFIG_FB
endif

ifneq ($(filter y, $(CONFIG_DRM)),)
	#ccflags-y += -DHX_CONFIG_DRM
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)),)
	himax_mmi-objs += himax_debug.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)),)
	himax_mmi-objs += himax_inspection.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_INCELL)),)
	himax_mmi-objs += himax_ic_incell_core.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83193)),)
  ccflags-y += -D__HIMAX_HX83193_MOD__
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-m += himax_ic_HX83193.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX83193.o
  endif
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83192)),)
  ccflags-y += -D__HIMAX_HX83192_MOD__
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	himax_mmi-objs += himax_ic_HX83192.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX83192.o
  endif
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83191)),)
  ccflags-y += -D__HIMAX_HX83191_MOD__
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-m += himax_ic_HX83191.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX83191.o
  endif
endif

ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
  ccflags-y += -D__HIMAX_MOD__
	himax_mmi-objs += himax_common.o
	himax_mmi-objs += himax_platform.o
ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_EMBEDDED_FIRMWARE)),)
	himax_mmi-objs += Himax_firmware.o
endif
	obj-m += himax_mmi.o
endif
ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	himax_mmi-objs += himax_common.o
	himax_mmi-objs += himax_platform.o
ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_EMBEDDED_FIRMWARE)),)
	himax_mmi-objs += Himax_firmware.o
endif
	obj-y += himax_mmi.o
endif

ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_EMBEDDED_FIRMWARE)),)
ccflags-y += -D__EMBEDDED_FW__

ld_array_start_str = _binary_$(srctree)/$(src)_Himax_firmware_bin_start
ld_array_start_sym = $(subst -,_,$(subst .,_,$(subst /,_,$(ld_array_start_str))))
obj_array_start_sym = _binary___Himax_firmware_bin_start

ld_array_size_str = _binary_$(srctree)/$(src)_Himax_firmware_bin_size
ld_array_size_sym = $(subst -,_,$(subst .,_,$(subst /,_,$(ld_array_size_str))))
obj_array_size_sym = _binary___Himax_firmware_bin_size

ld_array_end_str = _binary_$(srctree)/$(src)_Himax_firmware_bin_end
ld_array_end_sym = $(subst -,_,$(subst .,_,$(subst /,_,$(ld_array_end_str))))
obj_array_end_sym = _binary___Himax_firmware_bin_end

$(src)/Himax_firmware.o: $(src)/Himax_firmware.bin FORCE
	$(LD) $(LDFLAGS) -r -b binary $(srctree)/$(src)/Himax_firmware.bin -o $(objtree)/$(obj)/Himax_firmware.o
	$(OBJCOPY) --redefine-sym $(ld_array_start_sym)=$(obj_array_start_sym) $(objtree)/$(obj)/Himax_firmware.o
	$(OBJCOPY) --redefine-sym $(ld_array_size_sym)=$(obj_array_size_sym) $(objtree)/$(obj)/Himax_firmware.o
	$(OBJCOPY) --redefine-sym $(ld_array_end_sym)=$(obj_array_end_sym) $(objtree)/$(obj)/Himax_firmware.o

endif
