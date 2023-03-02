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

#ifndef _MAC_AX_FW_OFLD_H_
#define _MAC_AX_FW_OFLD_H_

#include "../type.h"
#include "fwcmd.h"
#include "fwofld.h"
#include "trx_desc.h"

#define READ_OFLD_MAX_LEN 2000
#define WRITE_OFLD_MAX_LEN 2000
#define CONF_OFLD_MAX_LEN 2000
#define CMD_OFLD_MAX_LEN 2000

#define CONF_OFLD_RESTORE 0
#define CONF_OFLD_BACKUP 1

#define SCAN_OP_STOP	0
#define SCAN_OP_START	1
#define SCAN_OP_SETPARM	2

#define CMD_OFLD_SIZE sizeof(struct fwcmd_cmd_ofld)

/* Generate 8-bit mask for a 4-byte alignment offset */
#define GET_W8_MSK(offset) \
	(0xFF << ((offset) & 0x3 ? (8 * ((offset) & 0x3)) : 0))

/* Generate 16-bit mask for a 4-byte alignment offset */
#define GET_W16_MSK(offset) \
	(0xFFFF << ((offset) & 0x2 ? 16 : 0))

#define MAC_REG_W8_OFLD(offset, val, lc) \
	write_mac_reg_ofld(adapter, offset & 0xFFFC, GET_W8_MSK(offset), val, lc)

#define MAC_REG_W16_OFLD(offset, val, lc) \
	write_mac_reg_ofld(adapter, offset & 0xFFFD, GET_W16_MSK(offset), val, lc)

#define MAC_REG_W32_OFLD(offset, val, lc) \
	write_mac_reg_ofld(adapter, offset, 0xFFFFFFFF, val, lc)

#define MAC_REG_W_OFLD(offset, mask, val, lc) \
	write_mac_reg_ofld(adapter, offset, mask, val, lc)

#define MAC_REG_P_OFLD(offset, mask, val, lc) \
	poll_mac_reg_ofld(adapter, offset, mask, val, lc)

#define DELAY_OFLD(val, lc) \
	poll_mac_reg_ofld(adapter, val, lc)

/**
 * @enum PKT_OFLD_OP
 *
 * @brief PKT_OFLD_OP
 *
 * @var PKT_OFLD_OP::PKT_OFLD_OP_ADD
 * Please Place Description here.
 * @var PKT_OFLD_OP::PKT_OFLD_OP_DEL
 * Please Place Description here.
 * @var PKT_OFLD_OP::PKT_OFLD_OP_READ
 * Please Place Description here.
 * @var PKT_OFLD_OP::PKT_OFLD_OP_MAX
 * Please Place Description here.
 */
enum PKT_OFLD_OP {
	PKT_OFLD_OP_ADD = 0,
	PKT_OFLD_OP_DEL = 1,
	PKT_OFLD_OP_READ = 2,
	PKT_OFLD_OP_MAX
};

/**
 * @enum FW_OFLD_OP
 *
 * @brief FW_OFLD_OP
 *
 * @var FW_OFLD_OP::FW_OFLD_OP_DUMP_EFUSE
 * Please Place Description here.
 * @var FW_OFLD_OP::FW_OFLD_OP_PACKET_OFLD
 * Please Place Description here.
 * @var FW_OFLD_OP::FW_OFLD_OP_READ_OFLD
 * Please Place Description here.
 * @var FW_OFLD_OP::FW_OFLD_OP_WRITE_OFLD
 * Please Place Description here.
 * @var FW_OFLD_OP::FW_OFLD_OP_CONF_OFLD
 * Please Place Description here.
 * @var FW_OFLD_OP::FW_OFLD_OP_MAX
 * Please Place Description here.
 */
enum FW_OFLD_OP {
	FW_OFLD_OP_DUMP_EFUSE = 0,
	FW_OFLD_OP_PACKET_OFLD = 1,
	FW_OFLD_OP_READ_OFLD = 2,
	FW_OFLD_OP_WRITE_OFLD = 3,
	FW_OFLD_OP_CONF_OFLD = 4,
	FW_OFLD_OP_CH_SWITCH = 5,
	FW_OFLD_OP_MAX
};

/**
 * @enum CHSW_BW
 *
 * @brief CHSW_BW
 *
 * @var CHSW_BW::CHSW_BW_20
 * Please Place Description here.
 * @var CHSW_BW::CHSW_BW_40
 * Please Place Description here.
 * @var CHSW_BW::CHSW_BW_80
 * Please Place Description here.
 * @var CHSW_BW::CHSW_BW_160
 * Please Place Description here.
 * @var CHSW_BW::CHSW_BW_80_80
 * Please Place Description here.
 * @var CHSW_BW::CHSW_BW_5
 * Please Place Description here.
 * @var CHSW_BW::CHSW_BW_10
 * Please Place Description here.
 * @var CHSW_BW::CHSW_BW_MAX
 * Please Place Description here.
 */
enum CHSW_BW {
	CHSW_BW_20 = 0,
	CHSW_BW_40 = 1,
	CHSW_BW_80 = 2,
	CHSW_BW_160 = 3,
	CHSW_BW_80_80 = 4,
	CHSW_BW_5 = 5,
	CHSW_BW_10 = 6,
	CHSW_BW_MAX
};

/**
 * @enum CHSW_CHBAND
 *
 * @brief CHSW_CHBAND
 *
 * @var CHSW_CHBAND::CHSW_CHBAND_24G
 * Please Place Description here.
 * @var CHSW_CHBAND::CHSW_CHBAND_5G
 * Please Place Description here.
 * @var CHSW_CHBAND::CHSW_CHBAND_6G
 * Please Place Description here.
 * @var CHSW_CHBAND::CHSW_CHBAND_MAX
 * Please Place Description here.
 */
enum CHSW_CHBAND {
	CHSW_CHBAND_24G = 0,
	CHSW_CHBAND_5G = 1,
	CHSW_CHBAND_6G = 2,
	CHSW_CHBAND_MAX
};

/**
 * @enum CHSW_STATUS_CODE
 *
 * @brief CHSW_STATUS_CODE
 *
 * @var CHSW_STATUS_CODE::CHSW_OK
 * Please Place Description here.
 * @var CHSW_STATUS_CODE::CHSW_NOT_COMPILED_ERR
 * Please Place Description here.
 * @var CHSW_STATUS_CODE::CHSW_STOPSER_FAIL_WARN
 * Please Place Description here.
 * @var CHSW_STATUS_CODE::CHSW_BB_CTRL_BW_CH_FAIL_ERR
 * Please Place Description here.
 * @var CHSW_STATUS_CODE::CHSW_RF_RELOAD_FAIL_WARN
 * Please Place Description here.
 * @var CHSW_STATUS_CODE::CHSW_STARTSER_FAIL_WARN
 * Please Place Description here.
 * @var CHSW_STATUS_CODE::CHSW_MAX
 * Please Place Description here.
 */
enum CHSW_STATUS_CODE {
	CHSW_OK = 0,
	CHSW_NOT_COMPILED_ERR = 1,
	CHSW_STOPSER_FAIL_WARN = 2,
	CHSW_BB_CTRL_BW_CH_FAIL_ERR = 3,
	CHSW_RF_RELOAD_FAIL_WARN = 4,
	CHSW_STARTSER_FAIL_WARN = 5,
	CHSW_MAX
};

/**
 * @struct mac_ax_conf_ofld_hdr
 * @brief mac_ax_conf_ofld_hdr
 *
 * @var mac_ax_conf_ofld_hdr::pattern_count
 * Please Place Description here.
 * @var mac_ax_conf_ofld_hdr::rsvd
 * Please Place Description here.
 */
struct mac_ax_conf_ofld_hdr {
	u16 pattern_count;
	u16 rsvd;
};

/**
 * @struct mac_ax_pkt_ofld_hdr
 * @brief mac_ax_pkt_ofld_hdr
 *
 * @var mac_ax_pkt_ofld_hdr::pkt_idx
 * Please Place Description here.
 * @var mac_ax_pkt_ofld_hdr::pkt_op
 * Please Place Description here.
 * @var mac_ax_pkt_ofld_hdr::rsvd
 * Please Place Description here.
 * @var mac_ax_pkt_ofld_hdr::pkt_len
 * Please Place Description here.
 */
struct mac_ax_pkt_ofld_hdr {
	u8 pkt_idx;
	u8 pkt_op:3;
	u8 rsvd:5;
	u16 pkt_len;
};

/**
 * @struct scanofld_chinfo_node
 * @brief scanofld_chinfo_node
 *
 * @var scanofld_chinfo_node::next
 * Point to next node.
 * @var scanofld_chinfo_node::chinfo
 * Content of this node.
 */
struct scanofld_chinfo_node {
	struct scanofld_chinfo_node *next;
	struct mac_ax_scanofld_chinfo *chinfo;
};

/**
 * @struct scan_chinfo_list
 * @brief scan_chinfo_list
 *
 * @var scan_chinfo_head::head
 * Point to first node.
 * @var scan_chinfo_head::tail
 * Point to last node.
 * @var scan_chinfo_head::size
 * Size of the list
 */
struct scan_chinfo_list {
	struct scanofld_chinfo_node *head;
	struct scanofld_chinfo_node *tail;
	u8 size;
};

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */
/**
 * @brief mac_reset_fwofld_state
 *
 * @param *adapter
 * @param op
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_reset_fwofld_state(struct mac_ax_adapter *adapter, u8 op);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_check_fwofld_done
 *
 * @param *adapter
 * @param op
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_check_fwofld_done(struct mac_ax_adapter *adapter, u8 op);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_clear_write_request
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_clear_write_request(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_add_write_request
 *
 * @param *adapter
 * @param *req
 * @param *value
 * @param *mask
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_add_write_request(struct mac_ax_adapter *adapter,
			  struct mac_ax_write_req *req,
			  u8 *value, u8 *mask);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_write_ofld
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_write_ofld(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_clear_conf_request
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_clear_conf_request(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_add_conf_request
 *
 * @param *adapter
 * @param *req
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_add_conf_request(struct mac_ax_adapter *adapter,
			 struct mac_ax_conf_ofld_req *req);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_conf_ofld
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_conf_ofld(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_read_pkt_ofld
 *
 * @param *adapter
 * @param id
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_read_pkt_ofld(struct mac_ax_adapter *adapter, u8 id);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_del_pkt_ofld
 *
 * @param *adapter
 * @param id
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_del_pkt_ofld(struct mac_ax_adapter *adapter, u8 id);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_add_pkt_ofld
 *
 * @param *adapter
 * @param *pkt
 * @param len
 * @param *id
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_add_pkt_ofld(struct mac_ax_adapter *adapter, u8 *pkt, u16 len, u8 *id);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_pkt_ofld_packet
 *
 * @param *adapter
 * @param **pkt_buf
 * @param *pkt_len
 * @param *pkt_id
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_pkt_ofld_packet(struct mac_ax_adapter *adapter,
			u8 **pkt_buf, u16 *pkt_len, u8 *pkt_id);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_dump_efuse_ofld
 *
 * @param *adapter
 * @param efuse_size
 * @param is_hidden
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_dump_efuse_ofld(struct mac_ax_adapter *adapter, u32 efuse_size,
			bool is_hidden);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_efuse_ofld_map
 *
 * @param *adapter
 * @param *efuse_map
 * @param efuse_size
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_efuse_ofld_map(struct mac_ax_adapter *adapter, u8 *efuse_map,
		       u32 efuse_size);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_clear_read_request
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_clear_read_request(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_add_read_request
 *
 * @param *adapter
 * @param *req
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_add_read_request(struct mac_ax_adapter *adapter,
			 struct mac_ax_read_req *req);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_read_ofld
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_read_ofld(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_read_ofld_value
 *
 * @param *adapter
 * @param **val_buf
 * @param *val_len
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_read_ofld_value(struct mac_ax_adapter *adapter,
			u8 **val_buf, u16 *val_len);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_general_pkt_ids
 *
 * @param *adapter
 * @param *ids
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_general_pkt_ids(struct mac_ax_adapter *adapter,
			struct mac_ax_general_pkt_ids *ids);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_add_cmd_ofld
 *
 * This is the function for FW IO offload.
 * Users could call the function to add write BB/RF/MAC REG command.
 * When the aggregated commands are full or the command is last,
 * FW would receive a H2C containing aggreated IO command.
 *
 * @param *adapter
 * @param *cmd
 * @return 0 for success. Others are fail.
 * @retval u32
 */
u32 mac_add_cmd_ofld(struct mac_ax_adapter *adapter, struct rtw_mac_cmd *cmd);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_get_fw_cap
 *
 * @param *adapter
 * @param *val
 * @return This function would set FW capability in *val.
 * return fail while FW is NOT ready
 * @retval u32
 */
u32 mac_get_fw_cap(struct mac_ax_adapter *adapter, u32 *val);
/**
 * @}
 * @}
 */

u32 write_mac_reg_ofld(struct mac_ax_adapter *adapter,
		       u16 offset, u32 mask, u32 val, u8 lc);

u32 poll_mac_reg_ofld(struct mac_ax_adapter *adapter,
		      u16 offset, u32 mask, u32 val, u8 lc);

u32 delay_ofld(struct mac_ax_adapter *adapter, u32 val);
/**
 * @brief mac_ccxrpt_parsing
 *
 * @param *adapter
 * @param *buf
 * @param *info
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_ccxrpt_parsing(struct mac_ax_adapter *adapter,
		       u8 *buf, struct mac_ax_ccxrpt *info);

u32 get_ccxrpt_event(struct mac_ax_adapter *adapter,
		     struct rtw_c2h_info *c2h,
		     enum phl_msg_evt_id *id, u8 *c2h_info);

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_scanofld_ch_list_clear
 *
 * Clear the scan list

 * @param *adapter
 * @param *list
 * @return void
 * @retval void
 */
void mac_scanofld_ch_list_clear(struct mac_ax_adapter *adapter,
				struct scan_chinfo_list *list);

/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_add_scanofld_ch
 *
 * Add a chinfo to scanlist.
 * Note that the user-allocated chinfo must not be free.
 * halmac will handle the free process
 *
 * @param *adapter
 * @param *chinfo
 * @param send_h2C send scanlist to fw after adding or not
 * @param clear_after_send clear halmac scanlist after sending or not(available when sendH2C is set)
 * @return 0 for success. Others are fail.
 * @retval u32
 */
u32 mac_add_scanofld_ch(struct mac_ax_adapter *adapter, struct mac_ax_scanofld_chinfo *chinfo,
			u8 send_h2C, u8 clear_after_send);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_scanofld
 *
 * Start scanofld
 *
 * @param *adapter
 * @param *scanParam
 * @return 0 for success. Others are fail.
 * @retval u32
 */
void mac_scanofld_reset_state(struct mac_ax_adapter *adapter);

/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_scanofld
 *
 * Start scanofld
 *
 * @param *adapter
 * @param *scanParam
 * @return 0 for success. Others are fail.
 * @retval u32
 */
u32 mac_scanofld(struct mac_ax_adapter *adapter, struct mac_ax_scanofld_param *scanParam);

/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_scanofld_fw_busy
 *
 * Check whether FW is scanning or not
 *
 * @param *adapter
 * @return 0 for idle. Others are busy.
 * @retval u32
 */
u32 mac_scanofld_fw_busy(struct mac_ax_adapter *adapter);

/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_scanofld_chlist_busy
 *
 * check whether halmac chlist or fw chlist are busy or not
 *
 * @param *adapter
 * @return 0 for idle. Others are busy.
 * @retval u32
 */
u32 mac_scanofld_chlist_busy(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief ch_switch_ofld
 *
 * ch switch offload
 *
 * @param *adapter
 * @param parm
 * @return 0 for success.
 * @retval ch switch offload h2c status
 */
u32 mac_ch_switch_ofld(struct mac_ax_adapter *adapter, struct mac_ax_ch_switch_parm parm);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_get_ch_switch_rpt
 *
 * get channel switch offload report
 *
 * @param *adapter
 * @param parm
 * @return 0 for success.
 * @retval ch switch offload h2c status
 */
u32 mac_get_ch_switch_rpt(struct mac_ax_adapter *adapter, struct mac_ax_ch_switch_rpt *rpt);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_cfg_bcn_filter
 *
 * config bcn filter
 *
 * @param *adapter
 * @param cfg
 * @return 0 for success.
 * @retval
 */
u32 mac_cfg_bcn_filter(struct mac_ax_adapter *adapter, struct mac_ax_bcn_fltr cfg);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_bcn_filter_rssi
 *
 * offload rssi to fw for bcn filter
 *
 * @param *adapter
 * @param macid
 * @param size
 * @param rssi
 * @return 0 for success.
 * @retval
 */
u32 mac_bcn_filter_rssi(struct mac_ax_adapter *adapter, u8 macid, u8 size, u8 *rssi);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Firmware
 * @{
 * @addtogroup FW_Offload
 * @{
 */

/**
 * @brief mac_bcn_filter_tp
 *
 * offload tp to fw for bcn filter
 *
 * @param *adapter
 * @param macid
 * @param tx
 * @param rx
 * @return 0 for success.
 * @retval
 */
u32 mac_bcn_filter_tp(struct mac_ax_adapter *adapter, u8 macid, u16 tx, u16 rx);
/**
 * @}
 * @}
 */
#endif
