// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 *
 * Authors:
 *	Dingqiang Lin <jon.lin@rock-chips.com>
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/mtd/spinand.h>

#define SPINAND_MFR_MK			0xF2

#define MK_REG_STATUS2			(0xD0)
#define MK_STATUS2_ECC_MASK		GENMASK(1, 0)
#define MK_STATUS_ECC_HAS_BITFLIPS_LESS	(1 << 4)
#define MK_STATUS_ECC_HAS_BITFLIPS_MORE	(2 << 4)
#define MK_STATUS_ECC_UNCOR_ERROR	(3 << 4)

static SPINAND_OP_VARIANTS(read_cache_variants,
		SPINAND_PAGE_READ_FROM_CACHE_X4_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X2_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(true, 0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(false, 0, 1, NULL, 0));

static SPINAND_OP_VARIANTS(write_cache_variants,
		SPINAND_PROG_LOAD_X4(true, 0, NULL, 0),
		SPINAND_PROG_LOAD(true, 0, NULL, 0));

static SPINAND_OP_VARIANTS(update_cache_variants,
		SPINAND_PROG_LOAD_X4(false, 0, NULL, 0),
		SPINAND_PROG_LOAD(false, 0, NULL, 0));

static int mksv1gcl_ooblayout_ecc(struct mtd_info *mtd, int section,
				  struct mtd_oob_region *region)
{
	if (section > 3)
		return -ERANGE;

	region->offset = (16 * section) + 6;
	region->length = 10;

	return 0;
}

static int mksv1gcl_ooblayout_free(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	if (section > 3)
		return -ERANGE;

	region->offset = (16 * section) + 2;
	region->length = 4;

	return 0;
}

static const struct mtd_ooblayout_ops mksv1gcl_ooblayout = {
	.ecc = mksv1gcl_ooblayout_ecc,
	.free = mksv1gcl_ooblayout_free,
};

/*
 *Table 12-5.ECC Error Bits Descriptions
 *ECC
 *Ability
 *ECCS1 ECCS0 ECCSE1 ECCSE0 Description
 *16
 *0 0 x x No bit errors were detected during the previous read algorithm
 *0 1 0 0 1~2 Bit errors were detected and corrected
 *0 1 0 1 3~4 Bit errors were detected and corrected.
 *0 1 1 0 5~6 Bit errors were detected and corrected.
 *0 1 1 1 7~8 Bit errors were detected and corrected.
 *1 0 0 0 9~10 Bit errors were detected and corrected.
 *1 0 0 1 11~12 Bit errors were detected and corrected.
 *1 0 1 0 13~14 Bit errors were detected and corrected.
 *1 0 1 1 15~16 Bit errors were detected and corrected.
 *1 1 x x Bit errors greater than ECC capability and not corrected
 */
static int mksv1gcl_ecc_get_status(struct spinand_device *spinand,
				   u8 status)
{
	u8 status2;
	struct spi_mem_op op = SPINAND_GET_FEATURE_OP(MK_REG_STATUS2,
						      spinand->scratchbuf);
	u8 ecc_status = status & STATUS_ECC_MASK;
	int ret;

	switch (ecc_status) {
	case STATUS_ECC_NO_BITFLIPS:
		return 0;

	case MK_STATUS_ECC_UNCOR_ERROR:
		return -EBADMSG;

	case MK_STATUS_ECC_HAS_BITFLIPS_LESS:
	case MK_STATUS_ECC_HAS_BITFLIPS_MORE:
		/*
		 * Read status2 register to determine a more fine grained
		 * bit error status
		 */
		ret = spi_mem_exec_op(spinand->spimem, &op);
		if (ret)
			return ret;
		status2 = *(spinand->scratchbuf) & MK_STATUS2_ECC_MASK;
		return (ecc_status == MK_STATUS_ECC_HAS_BITFLIPS_LESS) ?
		       (2 + status2 * 2) : (10 + status2 * 2);
	default:
		break;
	}

	return 0;
}

static const struct spinand_info mk_spinand_table[] = {
	SPINAND_INFO("MKSV1GIL-AE",
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_ADDR, 0x0A),
		     NAND_MEMORG(1, 2048, 64, 64, 1024, 20, 1, 1, 1),
		     NAND_ECCREQ(12, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&mksv1gcl_ooblayout, mksv1gcl_ecc_get_status)),
	SPINAND_INFO("MKSV2GIL-AE",
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_ADDR, 0x0B),
		     NAND_MEMORG(1, 2048, 64, 64, 2048, 40, 1, 1, 1),
		     NAND_ECCREQ(12, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&mksv1gcl_ooblayout, mksv1gcl_ecc_get_status)),
};

static const struct spinand_manufacturer_ops mk_spinand_manuf_ops = {
};

const struct spinand_manufacturer mk_spinand_manufacturer = {
	.id = SPINAND_MFR_MK,
	.name = "MK",
	.chips = mk_spinand_table,
	.nchips = ARRAY_SIZE(mk_spinand_table),
	.ops = &mk_spinand_manuf_ops,
};
