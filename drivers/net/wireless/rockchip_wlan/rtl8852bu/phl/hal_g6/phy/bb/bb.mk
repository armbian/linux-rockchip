# All needed files would be added to _HAL_INTFS_FILES, and it would include
# hal_g6/phy/bb and all related files in directory hal_g6/phy/bb/.
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
path_halbb_d1 := $(phl_path)/phy/bb

halbb-y += $(path_halbb_d1)/halbb.o \
			$(path_halbb_d1)/halbb_api.o \
			$(path_halbb_d1)/halbb_rua_tbl.o \
			$(path_halbb_d1)/halbb_auto_dbg.o\
			$(path_halbb_d1)/halbb_cfo_trk.o \
			$(path_halbb_d1)/halbb_ch_info.o \
			$(path_halbb_d1)/halbb_cmn_rpt.o \
			$(path_halbb_d1)/halbb_dbcc.o \
			$(path_halbb_d1)/halbb_dbg.o \
			$(path_halbb_d1)/halbb_dbg_cmd.o \
			$(path_halbb_d1)/halbb_dfs.o \
			$(path_halbb_d1)/halbb_edcca.o \
			$(path_halbb_d1)/halbb_env_mntr.o \
			$(path_halbb_d1)/halbb_hw_cfg.o \
			$(path_halbb_d1)/halbb_init.o \
			$(path_halbb_d1)/halbb_interface.o \
			$(path_halbb_d1)/halbb_la_mode.o \
			$(path_halbb_d1)/halbb_math_lib.o \
			$(path_halbb_d1)/halbb_mp.o \
			$(path_halbb_d1)/halbb_plcp_gen.o \
			$(path_halbb_d1)/halbb_plcp_tx.o \
			$(path_halbb_d1)/halbb_pmac_setting.o \
			$(path_halbb_d1)/halbb_psd.o \
			$(path_halbb_d1)/halbb_physts.o \
			$(path_halbb_d1)/halbb_pwr_ctrl.o \
			$(path_halbb_d1)/halbb_ra.o \
			$(path_halbb_d1)/halbb_statistics.o \
			$(path_halbb_d1)/halbb_ant_div.o \
			$(path_halbb_d1)/halbb_dig.o \
			$(path_halbb_d1)/halbb_fwofld.o \
			$(path_halbb_d1)/halbb_dyn_csi_rsp.o

ifeq ($(CONFIG_RTL8852A), y)
ic := 8852a
# Level 2 directory
path_halbb_8852a := $(path_halbb_d1)/halbb_$(ic)

halbb-y += $(path_halbb_8852a)/halbb_8852a.o \
			$(path_halbb_8852a)/halbb_8852a_api.o \
			$(path_halbb_8852a)/halbb_hwimg_8852a.o \
			$(path_halbb_8852a)/halbb_reg_cfg_8852a.o
endif

ifeq ($(CONFIG_RTL8852A), y)
ic := 8852a_2
# Level 2 directory
path_halbb_8852a_2 := $(path_halbb_d1)/halbb_$(ic)

halbb-y += $(path_halbb_8852a_2)/halbb_8852a_2.o \
			$(path_halbb_8852a_2)/halbb_8852a_2_api.o \
			$(path_halbb_8852a_2)/halbb_hwimg_8852a_2.o \
			$(path_halbb_8852a_2)/halbb_reg_cfg_8852a_2.o
endif

ifeq ($(CONFIG_RTL8852B), y)
ic := 8852b
# Level 2 directory
path_halbb_8852b := $(path_halbb_d1)/halbb_$(ic)

halbb-y += $(path_halbb_8852b)/halbb_8852b.o \
			$(path_halbb_8852b)/halbb_8852b_api.o \
			$(path_halbb_8852b)/halbb_hwimg_8852b.o \
			$(path_halbb_8852b)/halbb_reg_cfg_8852b.o
endif

ifeq ($(CONFIG_RTL8852C), y)
ic := 8852c
# Level 2 directory
path_halbb_8852c := $(path_halbb_d1)/halbb_$(ic)

halbb-y += $(path_halbb_8852c)/halbb_8852c.o \
			$(path_halbb_8852c)/halbb_8852c_api.o \
			$(path_halbb_8852c)/halbb_hwimg_8852c.o \
			$(path_halbb_8852c)/halbb_reg_cfg_8852c.o
endif

_HAL_BB_FILES +=	$(halbb-y)
