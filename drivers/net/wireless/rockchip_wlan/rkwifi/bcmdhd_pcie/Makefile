# SPDX-License-Identifier: GPL-2.0
#
# PCIe instance of the shared bcmdhd sources.
#
# bcmdhd selects its bus with -D flags that reach into every shared file, and
# dhd_sdio.c and dhd_pcie.c define the same dhd_bus_*/dhd_prot_* symbols, so a
# single module can only ever serve one bus.  To let one kernel serve both
# kinds of board, the sources in ../bcmdhd are compiled once per bus, with the
# objects landing in this directory so the two flag sets cannot collide.

BCMDHD_ROOT := $(src)/../bcmdhd

# Use the vendor's multi-driver naming (dhdsdio.ko / dhdpcie.ko, plus a
# per-bus log prefix and sysfs name) only when both back-ends are actually
# being built.  A single-bus config keeps producing plain bcmdhd.ko, exactly
# as it did before.
ifeq ($(CONFIG_BCMDHD_SDIO)$(CONFIG_BCMDHD_PCIE),yy)
CONFIG_BCMDHD_MULTIPLE_DRIVER := y
endif

# Pin this instance to a single bus, whatever the .config selected.
CONFIG_BCMDHD_SDIO :=
CONFIG_BCMDHD_PCIE := y
CONFIG_BCMDHD_USB  :=

include $(srctree)/$(BCMDHD_ROOT)/Makefile

# The sources live in ../bcmdhd, so kbuild's built-in $(obj)/%.o: $(src)/%.c
# rule cannot find them.
$(obj)/%.o: $(srctree)/$(BCMDHD_ROOT)/%.c FORCE
	$(call if_changed_rule,cc_o_c)
