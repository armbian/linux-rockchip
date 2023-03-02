/** @file */
/******************************************************************************
 *
 * Copyright(c) 2019 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 ******************************************************************************/

#ifndef _MAC_AX_FWDL_H_
#define _MAC_AX_FWDL_H_

#include "../type.h"
#include "fwcmd.h"
#include "trx_desc.h"
#include "trxcfg.h"
#include "dle.h"
#include "hci_fc.h"
#include "power_saving.h"

#if MAC_AX_PCIE_SUPPORT
#include "_pcie.h"
#endif

#if MAC_AX_8852A_SUPPORT
#include "../fw_ax/rtl8852a/hal8852a_fw.h"
#endif
#if MAC_AX_8852B_SUPPORT
#include "../fw_ax/rtl8852b/hal8852b_fw_u1.h"
#include "../fw_ax/rtl8852b/hal8852b_fw.h"
#endif

#define FWHDR_HDR_LEN (sizeof(struct fwhdr_hdr_t))
#define FWHDR_SECTION_LEN (sizeof(struct fwhdr_section_t))

#define ROMDL_SEG_LEN 0x40000

#define AX_BOOT_REASON_PWR_ON 0
#define AX_BOOT_REASON_WDT 1
#define AX_BOOT_REASON_LPS 2

#define RTL8852A_ID 0x50
#define RTL8852B_ID 0x51
#define RTL8852C_ID 0x52
#define RTL8834A_ID 0x53

#define RTL8852A_ROM_ADDR 0x18900000
#define RTL8852B_ROM_ADDR 0x18900000
#define RTL8852C_ROM_ADDR 0x20000000
#define RTL8192XB_ROM_ADDR 0x20000000

/**
 * @struct fwhdr_hdr_t
 * @brief fwhdr_hdr_t
 *
 * @var fwhdr_hdr_t::dword0
 * Please Place Description here.
 * @var fwhdr_hdr_t::dword1
 * Please Place Description here.
 * @var fwhdr_hdr_t::dword2
 * Please Place Description here.
 * @var fwhdr_hdr_t::dword3
 * Please Place Description here.
 * @var fwhdr_hdr_t::dword4
 * Please Place Description here.
 * @var fwhdr_hdr_t::dword5
 * Please Place Description here.
 * @var fwhdr_hdr_t::dword6
 * Please Place Description here.
 * @var fwhdr_hdr_t::dword7
 * Please Place Description here.
 */
struct fwhdr_hdr_t {
	u32 dword0;
	u32 dword1;
	u32 dword2;
	u32 dword3;
	u32 dword4;
	u32 dword5;
	u32 dword6;
	u32 dword7;
};

/**
 * @struct fwhdr_section_t
 * @brief fwhdr_section_t
 *
 * @var fwhdr_section_t::dword0
 * Please Place Description here.
 * @var fwhdr_section_t::dword1
 * Please Place Description here.
 * @var fwhdr_section_t::dword2
 * Please Place Description here.
 * @var fwhdr_section_t::dword3
 * Please Place Description here.
 */
struct fwhdr_section_t {
	u32 dword0;
	u32 dword1;
	u32 dword2;
	u32 dword3;
};

/**
 * @enum fw_dl_status
 *
 * @brief fw_dl_status
 *
 * @var fw_dl_status::FWDL_INITIAL_STATE
 * Please Place Description here.
 * @var fw_dl_status::FWDL_FWDL_ONGOING
 * Please Place Description here.
 * @var fw_dl_status::FWDL_CHECKSUM_FAIL
 * Please Place Description here.
 * @var fw_dl_status::FWDL_SECURITY_FAIL
 * Please Place Description here.
 * @var fw_dl_status::FWDL_CUT_NOT_MATCH
 * Please Place Description here.
 * @var fw_dl_status::FWDL_RSVD0
 * Please Place Description here.
 * @var fw_dl_status::FWDL_WCPU_FWDL_RDY
 * Please Place Description here.
 * @var fw_dl_status::FWDL_WCPU_FW_INIT_RDY
 * Please Place Description here.
 */
enum fw_dl_status {
	FWDL_INITIAL_STATE = 0,
	FWDL_FWDL_ONGOING = 1,
	FWDL_CHECKSUM_FAIL = 2,
	FWDL_SECURITY_FAIL = 3,
	FWDL_CUT_NOT_MATCH = 4,
	FWDL_RSVD0 = 5,
	FWDL_WCPU_FWDL_RDY = 6,
	FWDL_WCPU_FW_INIT_RDY = 7
};

/**
 * @enum fw_dl_cv
 *
 * @brief fw_dl_cv
 *
 * @var fw_dl_chip_cut::FWDL_CAV
 * Please Place Description here.
 * @var fw_dl_chip_cut::FWDL_CBV
 * Please Place Description here.
 * @var fw_dl_chip_cut::FWDL_CCV
 * Please Place Description here.
 * @var fw_dl_chip_cut::FWDL_CDV
 * Please Place Description here.
 * @var fw_dl_chip_cut::FWDL_CEV
 * Please Place Description here.
 * @var fw_dl_chip_cut::FWDL_CFV
 * Please Place Description here.
 * @var fw_dl_chip_cut::FWDL_CGV
 * Please Place Description here.
 * @var fw_dl_chip_cut::FWDL_CHV
 * Please Place Description here.
 * @var fw_dl_chip_cut::FWDL_CIV
 * Please Place Description here.
 */
enum fw_dl_cv {
	FWDL_CAV = 0,
	FWDL_CBV = 1,
	FWDL_CCV,
	FWDL_CDV,
	FWDL_CEV,
	FWDL_CFV,
	FWDL_CGV,
	FWDL_CHV,
	FWDL_CIV,
};

/* === FW header === */
/* dword0 */
#define FWHDR_CUTID_SH 0
#define FWHDR_CUTID_MSK 0xff
#define FWHDR_CHIPID_SH 8
#define FWHDR_CHIPID_MSK 0xffffff

/* dword1 */
#define FWHDR_MAJORVER_SH 0
#define FWHDR_MAJORVER_MSK 0xff
#define FWHDR_MINORVER_SH 8
#define FWHDR_MINORVER_MSK 0xff
#define FWHDR_SUBVERSION_SH 16
#define FWHDR_SUBVERSION_MSK 0xff
#define FWHDR_SUBINDEX_SH 24
#define FWHDR_SUBINDEX_MSK 0xff

/* dword2 */
#define FWHDR_COMMITID_SH 0
#define FWHDR_COMMITID_MSK 0xffffffff

/* dword3 */
#define FWHDR_SEC_HDR_OFFSET_SH 0
#define FWHDR_SEC_HDR_OFFSET_MSK 0xff
#define FWHDR_SEC_HDR_SZ_SH 8
#define FWHDR_SEC_HDR_SZ_MSK 0xff
#define FWHDR_FWHDR_SZ_SH 16
#define FWHDR_FWHDR_SZ_MSK 0xff
#define FWHDR_FWHDR_VER_SH 24
#define FWHDR_FWHDR_VER_MSK 0xff

/* dword4 */
#define FWHDR_MONTH_SH 0
#define FWHDR_MONTH_MSK 0xff
#define FWHDR_DATE_SH 8
#define FWHDR_DATE_MSK 0xff
#define FWHDR_HOUR_SH 16
#define FWHDR_HOUR_MSK 0xff
#define FWHDR_MIN_SH 24
#define FWHDR_MIN_MSK 0xff

/* dword5 */
#define FWHDR_YEAR_SH 0
#define FWHDR_YEAR_MSK 0xffff

/* dword6 */
#define FWHDR_IMAGEFROM_SH 0
#define FWHDR_IMAGEFROM_MSK 0x3
#define FWHDR_BOOTFROM_SH 4
#define FWHDR_BOOTFROM_MSK 0x3
#define FWHDR_ROM_ONLY BIT(6)
#define FWHDR_FW_TYPE BIT(7)
#define FWHDR_SEC_NUM_SH 8
#define FWHDR_SEC_NUM_MSK 0xff
#define FWHDR_HCI_TYPE_SH 16
#define FWHDR_HCI_TYPE_MSK 0xf
#define FWHDR_NET_TYPE_SH 20
#define FWHDR_NET_TYPE_MSK 0xf

/* dword7 */
#define FWHDR_FW_PART_SZ_SH 0
#define FWHDR_FW_PART_SZ_MSK 0xffff
#define FWHDR_CMD_VER_SH 24
#define FWHDR_CMD_VER_MSK 0xff

/* === Section header === */
/* dword0 */
#define SECTION_INFO_SEC_DL_ADDR_SH 0
#define SECTION_INFO_SEC_DL_ADDR_MSK 0xffffffff

/* dword1 */
#define SECTION_INFO_SEC_SIZE_SH 0
#define SECTION_INFO_SEC_SIZE_MSK 0xffffff
#define SECTION_INFO_SECTIONTYPE_SH 24
#define SECTION_INFO_SECTIONTYPE_MSK 0xf
#define SECTION_INFO_CHECKSUM BIT(28)
#define SECTION_INFO_REDL BIT(29)

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Download
 * @{
 */
/**
 * @brief disable_fw_watchdog
 *
 * @param *adapter
 * @param *fw
 * @param len
 * @return Please Place Description here.
 * @retval u32
 */
u32 disable_fw_watchdog(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Download
 * @{
 */
/**
 * @brief mac_fwredl
 *
 * @param *adapter
 * @param *fw
 * @param len
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_fwredl(struct mac_ax_adapter *adapter, u8 *fw, u32 len);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Download
 * @{
 */

/**
 * @brief mac_fwdl
 *
 * @param *adapter
 * @param *fw
 * @param len
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_fwdl(struct mac_ax_adapter *adapter, u8 *fw, u32 len);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Download
 * @{
 */

/**
 * @brief mac_enable_cpu
 *
 * @param *adapter
 * @param boot_reason
 * @param dlfw
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_enable_cpu(struct mac_ax_adapter *adapter, u8 boot_reason, u8 dlfw);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Download
 * @{
 */

/**
 * @brief mac_disable_cpu
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_disable_cpu(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Download
 * @{
 */

/**
 * @brief mac_romdl
 *
 * @param *adapter
 * @param *rom
 * @param romaddr
 * @param len
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_romdl(struct mac_ax_adapter *adapter, u8 *rom, u32 romaddr, u32 len);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Download
 * @{
 */

/**
 * @brief mac_ram_boot
 *
 * @param *adapter
 * @param *fw
 * @param len
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_ram_boot(struct mac_ax_adapter *adapter, u8 *fw, u32 len);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Download
 * @{
 */

/**
 * @brief mac_enable_fw
 *
 * @param *adapter
 * @param cat
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_enable_fw(struct mac_ax_adapter *adapter, enum rtw_fw_type cat);
/**
 * @}
 * @}
 */

/**
 * @brief mac_query_fw_buff
 *
 * @param *adapter
 * @param cat
 * @param **fw
 * @param *fw_len
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_query_fw_buff(struct mac_ax_adapter *adapter, enum rtw_fw_type cat, u8 **fw, u32 *fw_len);
/**
 * @}
 * @}
 */
#endif
