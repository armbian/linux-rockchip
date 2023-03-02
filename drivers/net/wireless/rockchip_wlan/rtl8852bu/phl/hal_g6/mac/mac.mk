# All needed files would be added to _HAL_INTFS_FILES, and it would include
# hal/hal_halmac.c and all related files in directory hal/halmac/.
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
path_hm := $(phl_path)/mac
# Level 1 directory
path_hm_d1 := $(path_hm)/mac_ax
path_fw_d1 := $(path_hm)/fw_ax

ifeq ($(CONFIG_PCI_HCI), y)
pci := y
endif
ifeq ($(CONFIG_SDIO_HCI), y)
sdio := y
endif
ifeq ($(CONFIG_USB_HCI), y)
usb := y
endif

halmac-y +=		$(path_hm)/mac_ax.o

# Modify level 1 directory if needed
#			$(path_hm_d1)/fwdl.o

halmac-y +=		$(path_hm_d1)/addr_cam.o \
			$(path_hm_d1)/cmac_tx.o \
			$(path_hm_d1)/coex.o \
			$(path_hm_d1)/cpuio.o \
			$(path_hm_d1)/dbcc.o \
			$(path_hm_d1)/dbgpkg.o \
			$(path_hm_d1)/dbgport_hw.o \
			$(path_hm_d1)/dbg_cmd.o \
			$(path_hm_d1)/dle.o \
			$(path_hm_d1)/efuse.o \
			$(path_hm_d1)/fwcmd.o \
			$(path_hm_d1)/fwdl.o \
			$(path_hm_d1)/fwofld.o \
			$(path_hm_d1)/gpio.o \
			$(path_hm_d1)/hci_fc.o \
			$(path_hm_d1)/hdr_conv.o \
			$(path_hm_d1)/hw_seq.o \
			$(path_hm_d1)/h2c_agg.o \
			$(path_hm_d1)/hw.o \
			$(path_hm_d1)/hwamsdu.o \
			$(path_hm_d1)/init.o \
			$(path_hm_d1)/la_mode.o \
			$(path_hm_d1)/mcc.o \
			$(path_hm_d1)/mport.o \
			$(path_hm_d1)/phy_rpt.o \
			$(path_hm_d1)/power_saving.o \
			$(path_hm_d1)/pwr.o \
			$(path_hm_d1)/p2p.o \
			$(path_hm_d1)/role.o \
			$(path_hm_d1)/rx_filter.o \
			$(path_hm_d1)/rx_forwarding.o \
			$(path_hm_d1)/rrsr.o \
			$(path_hm_d1)/ser.o \
			$(path_hm_d1)/security_cam.o \
			$(path_hm_d1)/sounding.o \
			$(path_hm_d1)/status.o \
			$(path_hm_d1)/sta_sch.o \
			$(path_hm_d1)/tblupd.o \
			$(path_hm_d1)/tcpip_checksum_offload.o \
			$(path_hm_d1)/trx_desc.o \
			$(path_hm_d1)/trxcfg.o \
			$(path_hm_d1)/twt.o \
			$(path_hm_d1)/wowlan.o \
			$(path_hm_d1)/flash.o \
			$(path_hm_d1)/spatial_reuse.o \
			$(path_hm_d1)/pwr_seq_func.o \
			$(path_hm_d1)/phy_misc.o \

halmac-$(pci) += 	$(path_hm_d1)/_pcie.o
halmac-$(usb) += 	$(path_hm_d1)/_usb.o
halmac-$(sdio) +=	$(path_hm_d1)/_sdio.o


ifeq ($(CONFIG_RTL8852A), y)
ic := 8852a
endif
ifeq ($(CONFIG_RTL8852B), y)
ic := 8852b
endif
ifeq ($(CONFIG_RTL8852C), y)
ic := 8852c
endif


# Level 2 directory
path_hm2 := $(path_hm_d1)/mac_$(ic)

halmac-y	+=	$(path_hm2)/gpio_$(ic).o \
			$(path_hm2)/init_$(ic).o \
			$(path_hm2)/pwr_seq_$(ic).o
halmac-$(usb) += 	$(path_hm2)/_usb_$(ic).o


ifeq ($(CONFIG_RTL8852B), y)
halmac-y	+=	$(path_hm2)/pwr_seq_func_$(ic).o
endif

ifeq ($(CONFIG_RTL8852C), y)
halmac-y	+=	$(path_hm2)/pwr_seq_func_$(ic).o \
				$(path_hm2)/trx_desc_$(ic).o
endif

# fw files
path_fw := $(path_fw_d1)/rtl$(ic)

halmac-y	+=	$(path_fw)/hal$(ic)_fw.o \
			$(path_fw)/hal$(ic)_fw_log.o
ifeq ($(CONFIG_RTL8852B), y)
halmac-y        +=      $(path_fw)/hal$(ic)_fw_u1.o
endif

_HAL_MAC_FILES +=	$(halmac-y)
