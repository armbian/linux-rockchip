# SPDX-License-Identifier: GPL-2.0
#rkwifi packed Makefile
# (gwl)

# The bcmdhd sources in bcmdhd/ are bus-agnostic only in name: the bus is
# picked by -D flags that leak into every shared file, and dhd_sdio.c and
# dhd_pcie.c define the same dhd_bus_*/dhd_prot_* symbols.  So each bus gets
# its own object directory, which compiles the shared sources with its own
# flag set into its own module.  bcmdhd/ itself is never built directly.
obj-$(CONFIG_BCMDHD_SDIO) += bcmdhd_sdio/
obj-$(CONFIG_BCMDHD_PCIE) += bcmdhd_pcie/

.PHONY: clean

clean:
	find . -name '*.o*' -exec rm -f {} \; 
