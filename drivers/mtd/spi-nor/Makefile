# SPDX-License-Identifier: GPL-2.0

spi-nor-objs			:= core.o sfdp.o swp.o otp.o sysfs.o
spi-nor-$(CONFIG_MTD_SPI_NOR_ATMEL)		+= atmel.o
spi-nor-$(CONFIG_MTD_SPI_NOR_BOYA)		+= boya.o
spi-nor-$(CONFIG_MTD_SPI_NOR_CATALYST)		+= catalyst.o
spi-nor-$(CONFIG_MTD_SPI_NOR_DOSILICON)		+= dosilicon.o
spi-nor-$(CONFIG_MTD_SPI_NOR_EON)		+= eon.o
spi-nor-$(CONFIG_MTD_SPI_NOR_ESMT)		+= esmt.o
spi-nor-$(CONFIG_MTD_SPI_NOR_EVERSPIN)		+= everspin.o
spi-nor-$(CONFIG_MTD_SPI_NOR_FMSH)		+= fmsh.o
spi-nor-$(CONFIG_MTD_SPI_NOR_FUJITSU)		+= fujitsu.o
spi-nor-$(CONFIG_MTD_SPI_NOR_GIGADEVICE)	+= gigadevice.o
spi-nor-$(CONFIG_MTD_SPI_NOR_INTEL)		+= intel.o
spi-nor-$(CONFIG_MTD_SPI_NOR_ISSI)		+= issi.o
spi-nor-$(CONFIG_MTD_SPI_NOR_MACRONIX)		+= macronix.o
spi-nor-$(CONFIG_MTD_SPI_NOR_STMICRO)		+= micron-st.o
spi-nor-$(CONFIG_MTD_SPI_NOR_NORMEM)		+= normem.o
spi-nor-$(CONFIG_MTD_SPI_NOR_PUYA)		+= puya.o
spi-nor-$(CONFIG_MTD_SPI_NOR_SPANSION)		+= spansion.o
spi-nor-$(CONFIG_MTD_SPI_NOR_SST)		+= sst.o
spi-nor-$(CONFIG_MTD_SPI_NOR_WINBOND)		+= winbond.o
spi-nor-$(CONFIG_MTD_SPI_NOR_XILINX)		+= xilinx.o
spi-nor-$(CONFIG_MTD_SPI_NOR_XMC)		+= xmc.o
spi-nor-$(CONFIG_MTD_SPI_NOR_XTX)		+= xtx.o
spi-nor-$(CONFIG_MTD_SPI_NOR_ZBIT)		+= zbit.o
spi-nor-$(CONFIG_DEBUG_FS)	+= debugfs.o
obj-$(CONFIG_MTD_SPI_NOR)	+= spi-nor.o

obj-$(CONFIG_MTD_SPI_NOR)	+= controllers/
