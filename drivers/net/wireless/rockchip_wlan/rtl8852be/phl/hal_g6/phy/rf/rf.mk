# All needed files would be added to _HAL_INTFS_FILES, and it would include
# hal_g6/phy/rf and all related files in directory hal_g6/phy/rf/.
# Before include this makefile, be sure interface (CONFIG_*_HCI) and IC
# (CONFIG_RTL*) setting are all ready!

HAL = hal_g6

ifeq ($(CONFIG_PHL_ARCH), y)
phl_path := phl/hal_g6
phl_path_d1 := $(src)/phl/$(HAL)
else
phl_path := hal_g6
phl_path_d1 := $(src)/$(HAL)
endif

# Base directory
path_halrf_d1 := $(phl_path)/phy/rf

halrf-y += $(path_halrf_d1)/halrf.o \
			$(path_halrf_d1)/halrf_pmac.o \
			$(path_halrf_d1)/halrf_api.o \
			$(path_halrf_d1)/halrf_dbg.o \
			$(path_halrf_d1)/halrf_dbg_cmd.o \
			$(path_halrf_d1)/halrf_ex.o \
			$(path_halrf_d1)/halrf_hw_cfg.o \
			$(path_halrf_d1)/halrf_init.o \
			$(path_halrf_d1)/halrf_interface.o \
			$(path_halrf_d1)/halrf_pwr_table.o \
			$(path_halrf_d1)/halrf_iqk.o \

ifeq ($(CONFIG_RTL8852A), y)
ic := 8852a

# Level 2 directory
path_halrf_8852a := $(path_halrf_d1)/halrf_$(ic)

halrf-y += $(path_halrf_8852a)/halrf_8852a.o \
			$(path_halrf_8852a)/halrf_8852a_api.o \
			$(path_halrf_8852a)/halrf_hwimg_8852a.o\
			$(path_halrf_8852a)/halrf_txgapk_8852a.o\
			$(path_halrf_8852a)/halrf_iqk_8852a.o\
			$(path_halrf_8852a)/halrf_reg_cfg_8852a.o\
			$(path_halrf_8852a)/halrf_dpk_8852a.o\
			$(path_halrf_8852a)/halrf_dack_8852a.o\
			$(path_halrf_8852a)/halrf_set_pwr_table_8852a.o\
			$(path_halrf_8852a)/halrf_efuse_8852a.o\
			$(path_halrf_8852a)/halrf_tssi_8852a.o\
			$(path_halrf_8852a)/halrf_kfree_8852a.o\
			$(path_halrf_8852a)/halrf_psd_8852a.o
endif

ifeq ($(CONFIG_RTL8852B), y)
ic := 8852b

# Level 2 directory
path_halrf_8852b := $(path_halrf_d1)/halrf_$(ic)

halrf-y += $(path_halrf_8852b)/halrf_8852b.o\
			$(path_halrf_8852b)/halrf_8852b_api.o \
			$(path_halrf_8852b)/halrf_hwimg_8852b.o\
			$(path_halrf_8852b)/halrf_txgapk_8852b.o\
			$(path_halrf_8852b)/halrf_iqk_8852b.o\
			$(path_halrf_8852b)/halrf_reg_cfg_8852b.o\
			$(path_halrf_8852b)/halrf_dack_8852b.o \
			$(path_halrf_8852b)/halrf_dpk_8852b.o \
			$(path_halrf_8852b)/halrf_set_pwr_table_8852b.o\
			$(path_halrf_8852b)/halrf_efuse_8852b.o\
			$(path_halrf_8852b)/halrf_tssi_8852b.o\
			$(path_halrf_8852b)/halrf_kfree_8852b.o\
			$(path_halrf_8852b)/halrf_psd_8852b.o
endif

ifeq ($(CONFIG_RTL8852C), y)
ic := 8852c

# Level 2 directory
path_halrf_8852c := $(path_halrf_d1)/halrf_$(ic)

halrf-y += $(path_halrf_8852c)/halrf_8852c.o \
//			$(path_halrf_8852a)/halrf_8852c_api.o \
//			$(path_halrf_8852a)/halrf_hwimg_8852c.o\
//			$(path_halrf_8852a)/halrf_txgapk_8852c.o\
//			$(path_halrf_8852a)/halrf_iqk_8852c.o\
//			$(path_halrf_8852a)/halrf_reg_cfg_8852c.o\
//			$(path_halrf_8852a)/halrf_dpk_8852c.o\
//			$(path_halrf_8852a)/halrf_dack_8852c.o\
//			$(path_halrf_8852a)/halrf_set_pwr_table_8852c.o\
//			$(path_halrf_8852a)/halrf_efuse_8852c.o\
//			$(path_halrf_8852a)/halrf_tssi_8852c.o\
//			$(path_halrf_8852a)/halrf_kfree_8852c.o\
//			$(path_halrf_8852a)/halrf_psd_8852c.o
endif

_HAL_RF_FILES +=	$(halrf-y)
