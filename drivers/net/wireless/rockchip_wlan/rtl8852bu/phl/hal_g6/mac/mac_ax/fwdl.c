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

#include "fwdl.h"

#define FWDL_WAIT_CNT 400000
#define FWDL_SECTION_MAX_NUM 10
#define FWDL_SECTION_CHKSUM_LEN	8
#define FWDL_SECTION_PER_PKT_LEN 2020
#define FWDL_TRY_CNT 3
#define FWDL_DIGEST_SIZE 0x20

struct fwhdr_section_info {
	u8 redl;
	u8 *addr;
	u32 len;
	u32 dladdr;
};

struct fw_bin_info {
	u8 section_num;
	u32 hdr_len;
	u32 git_idx;
	struct fwhdr_section_info section_info[FWDL_SECTION_MAX_NUM];
};

struct hw_info {
	u8 chip;
	u8 cut;
	u8 category;
};

struct fwld_info {
	u32 len;
	u8 *fw;
};

static inline void fwhdr_section_parser(struct fwhdr_section_t *section,
					struct fwhdr_section_info *info)
{
	u32 hdr_val;
	u32 section_len;

	hdr_val = le32_to_cpu(section->dword1);
	section_len = GET_FIELD(hdr_val, SECTION_INFO_SEC_SIZE);
	if (hdr_val & SECTION_INFO_CHECKSUM)
		section_len += FWDL_SECTION_CHKSUM_LEN;
	info->len = section_len;
	info->redl = (hdr_val & SECTION_INFO_REDL) ? 1 : 0;

	info->dladdr = (GET_FIELD(le32_to_cpu(section->dword0),
		SECTION_INFO_SEC_DL_ADDR)) & 0x1FFFFFFF;
}

static inline void fwhdr_hdr_parser(struct fwhdr_hdr_t *hdr,
				    struct fw_bin_info *info)
{
	u32 hdr_val;

	hdr_val = le32_to_cpu(hdr->dword6);
	info->section_num = GET_FIELD(hdr_val, FWHDR_SEC_NUM);
	info->hdr_len = FWHDR_HDR_LEN + info->section_num * FWHDR_SECTION_LEN;

	/* fill HALMAC information */
	hdr_val = le32_to_cpu(hdr->dword7);
	hdr_val = SET_CLR_WORD(hdr_val, FWDL_SECTION_PER_PKT_LEN,
			       FWHDR_FW_PART_SZ);
	hdr->dword7 = cpu_to_le32(hdr_val);

	hdr_val = le32_to_cpu(hdr->dword2);
	info->git_idx = GET_FIELD(hdr_val, FWHDR_COMMITID);
}

static u32 fwhdr_parser(struct mac_ax_adapter *adapter, u8 *fw, u32 len,
			struct fw_bin_info *info)
{
	u32 i;
	u8 *fw_end = fw + len;
	u8 *bin_ptr;
	struct fwhdr_section_info *cur_section_info;

	if (!info) {
		PLTFM_MSG_ERR("[ERR]%s: *info = NULL\n", __func__);
		return MACNPTR;
	} else if (!fw) {
		PLTFM_MSG_ERR("[ERR]%s: *fw = NULL\n", __func__);
		return MACNOITEM;
	} else if (!len) {
		PLTFM_MSG_ERR("[ERR]%s: len = 0\n", __func__);
		return MACBUFSZ;
	}

	fwhdr_hdr_parser((struct fwhdr_hdr_t *)fw, info);
	bin_ptr = fw + info->hdr_len;

	/* jump to section header */
	fw += FWHDR_HDR_LEN;
	cur_section_info = info->section_info;
	for (i = 0; i < info->section_num; i++) {
		fwhdr_section_parser((struct fwhdr_section_t *)fw,
				     cur_section_info);
		cur_section_info->addr = bin_ptr;
		bin_ptr += cur_section_info->len;
		fw += FWHDR_SECTION_LEN;
		cur_section_info++;
	}

	if (fw_end != bin_ptr) {
		PLTFM_MSG_ERR("[ERR]%s: ", __func__);
		PLTFM_MSG_ERR("fw bin size != fw size in fwhdr\n");
		return MACFWBIN;
	}

	return MACSUCCESS;
}

static inline u32 update_fw_ver(struct mac_ax_adapter *adapter,
				struct fwhdr_hdr_t *hdr)
{
	u32 hdr_val;
	struct mac_ax_fw_info *info = &adapter->fw_info;

	hdr_val = le32_to_cpu(hdr->dword1);
	info->major_ver = GET_FIELD(hdr_val, FWHDR_MAJORVER);
	info->minor_ver = GET_FIELD(hdr_val, FWHDR_MINORVER);
	info->sub_ver = GET_FIELD(hdr_val, FWHDR_SUBVERSION);
	info->sub_idx = GET_FIELD(hdr_val, FWHDR_SUBINDEX);

	hdr_val = le32_to_cpu(hdr->dword5);
	info->build_year = GET_FIELD(hdr_val, FWHDR_YEAR);

	hdr_val = le32_to_cpu(hdr->dword4);
	info->build_mon = GET_FIELD(hdr_val, FWHDR_MONTH);
	info->build_date = GET_FIELD(hdr_val, FWHDR_DATE);
	info->build_hour = GET_FIELD(hdr_val, FWHDR_HOUR);
	info->build_min = GET_FIELD(hdr_val, FWHDR_MIN);

	info->h2c_seq = 0;
	info->rec_seq = 0;

	return MACSUCCESS;
}

static u32 __fwhdr_download(struct mac_ax_adapter *adapter,
			    u8 *fw, u32 hdr_len, u8 redl)
{
	u8 *buf;
	u32 ret = 0;
	#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
	#else
	struct h2c_buf *h2cb;
	#endif

	h2cb = h2cb_alloc(adapter, H2CB_CLASS_DATA);
	if (!h2cb) {
		PLTFM_MSG_ERR("[ERR]%s: h2cb_alloc fail\n", __func__);
		return MACNPTR;
	}

	buf = h2cb_put(h2cb, hdr_len);
	if (!buf) {
		PLTFM_MSG_ERR("[ERR]%s: h2cb_put fail\n", __func__);
		ret = MACNOBUF;
		goto fail;
	}

	PLTFM_MEMCPY(buf, fw, hdr_len);

	if (redl) {
		ret = h2c_pkt_set_hdr_fwdl(adapter, h2cb,
					   FWCMD_TYPE_H2C, FWCMD_H2C_CAT_MAC,
					   FWCMD_H2C_CL_FWDL,
					   FWCMD_H2C_FUNC_FWHDR_REDL, 0, 0);
	} else {
		ret = h2c_pkt_set_hdr_fwdl(adapter, h2cb,
					   FWCMD_TYPE_H2C, FWCMD_H2C_CAT_MAC,
					   FWCMD_H2C_CL_FWDL,
					   FWCMD_H2C_FUNC_FWHDR_DL, 0, 0);
	}

	if (ret) {
		PLTFM_MSG_ERR("[ERR]%s: set h2c hdr fail\n", __func__);
		goto fail;
	}

	ret = h2c_pkt_build_txd(adapter, h2cb);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]%s: build h2c txd fail\n", __func__);
		goto fail;
	}

	#if MAC_AX_PHL_H2C
	ret = PLTFM_TX(h2cb);
	#else
	ret = PLTFM_TX(h2cb->data, h2cb->len);
	#endif
	if (ret) {
		PLTFM_MSG_ERR("[ERR]%s: PLTFM_TX fail\n", __func__);
		goto fail;
	}

	h2cb_free(adapter, h2cb);

	return MACSUCCESS;
fail:
	h2cb_free(adapter, h2cb);

	PLTFM_MSG_ERR("[ERR]%s ret: %d\n", __func__, ret);

	return ret;
}

#if MAC_AX_PHL_H2C
static u32 __sections_build_txd(struct mac_ax_adapter *adapter,
				struct rtw_h2c_pkt *h2cb)
{
	u8 *buf;
	u32 ret;
	u32 txd_len;
	struct rtw_t_meta_data info = {0};
	struct mac_ax_ops *ops = adapter_to_mac_ops(adapter);

	info.type = RTW_PHL_PKT_TYPE_FWDL;
	info.pktlen = (u16)h2cb->data_len;
	txd_len = ops->txdesc_len(adapter, &info);

	buf = h2cb_push(h2cb, txd_len);
	if (!buf) {
		PLTFM_MSG_ERR("[ERR]%s: h2cb_push fail\n", __func__);
		return MACNPTR;
	}

	ret = ops->build_txdesc(adapter, &info, buf, txd_len);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]%s: ", __func__);
		PLTFM_MSG_ERR("build_txdesc fail\n");
		return ret;
	}

	return MACSUCCESS;
}

static u32 __sections_push(struct rtw_h2c_pkt *h2cb)
{
#define section_push_len 8
	h2cb->vir_data -= section_push_len;
	h2cb->vir_tail -= section_push_len;

	return MACSUCCESS;
}

#else
static u32 __sections_build_txd(struct mac_ax_adapter *adapter,
				struct h2c_buf *h2cb)
{
	u8 *buf;
	u32 ret;
	u32 txd_len;
	struct rtw_t_meta_data info;
	struct mac_ax_ops *ops = adapter_to_mac_ops(adapter);

	info.type = RTW_PHL_PKT_TYPE_FWDL;
	info.pktlen = (u16)h2cb->len;
	txd_len = ops->txdesc_len(adapter, &info);

	buf = h2cb_push(h2cb, txd_len);
	if (!buf) {
		PLTFM_MSG_ERR("[ERR]%s: h2cb_push fail\n", __func__);
		return MACNPTR;
	}

	ret = ops->build_txdesc(adapter, &info, buf, txd_len);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]%s: ", __func__);
		PLTFM_MSG_ERR("mac_build_txdesc fail\n");
		return ret;
	}

	return MACSUCCESS;
}
#endif
static u32 __sections_download(struct mac_ax_adapter *adapter,
			       struct fwhdr_section_info *info)
{
	u8 *section = info->addr;
	u32 residue_len = info->len;
	u32 pkt_len;
	u8 *buf;
	u32 ret = 0;
	#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
	#else
	struct h2c_buf *h2cb;
	#endif

	while (residue_len) {
		if (residue_len >= FWDL_SECTION_PER_PKT_LEN)
			pkt_len = FWDL_SECTION_PER_PKT_LEN;
		else
			pkt_len = residue_len;

		h2cb = h2cb_alloc(adapter, H2CB_CLASS_LONG_DATA);
		if (!h2cb) {
			PLTFM_MSG_ERR("[ERR]%s: ", __func__);
			PLTFM_MSG_ERR("h2cb_alloc fail\n");
			return MACNPTR;
		}
		#if MAC_AX_PHL_H2C
		__sections_push(h2cb);
		#endif
		buf = h2cb_put(h2cb, pkt_len);
		if (!buf) {
			PLTFM_MSG_ERR("[ERR]%s: ", __func__);
			PLTFM_MSG_ERR("h2cb_put fail\n");
			ret = MACNOBUF;
			goto fail;
		}

		PLTFM_MEMCPY(buf, section, pkt_len);

		ret = __sections_build_txd(adapter, h2cb);
		if (ret) {
			PLTFM_MSG_ERR("[ERR]%s: ", __func__);
			PLTFM_MSG_ERR("__sections_build_txd fail\n");
			goto fail;
		}
		#if MAC_AX_PHL_H2C
		ret = PLTFM_TX(h2cb);
		#else
		ret = PLTFM_TX(h2cb->data, h2cb->len);
		#endif
		if (ret) {
			PLTFM_MSG_ERR("[ERR]%s: PLTFM_TX fail\n", __func__);
			goto fail;
		}

		h2cb_free(adapter, h2cb);

		section += pkt_len;
		residue_len -= pkt_len;
	}

	return MACSUCCESS;
fail:
	h2cb_free(adapter, h2cb);

	PLTFM_MSG_ERR("[ERR]%s ret: %d\n", __func__, ret);

	return ret;
}

static u32 __write_memory(struct mac_ax_adapter *adapter,
			  u8 *buffer, u32 addr, u32 len)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u8 *content = NULL;
	u32 dl_size;
	u32 target_addr, write_addr;
	u32 seg_size, seg_bytes;
	u32 val32;
	u32 index = 0;
	u32 ret = MACSUCCESS;

	PLTFM_MSG_WARN("%s ind access start\n", __func__);
	PLTFM_MUTEX_LOCK(&adapter->hw_info->ind_access_lock);
	adapter->hw_info->ind_aces_cnt++;

	MAC_REG_W32(R_AX_FILTER_MODEL_ADDR, addr);
	MAC_REG_W32(R_AX_INDIR_ACCESS_ENTRY, 0xAAAAAAAA);
	MAC_REG_W32(R_AX_INDIR_ACCESS_ENTRY + 4, 0xBBBBBBBB);

	val32 = MAC_REG_R32(R_AX_INDIR_ACCESS_ENTRY);
	if (val32 != 0xAAAAAAAA) {
		ret = MACMEMRO;
		goto ind_aces_end;
	}

	val32 = MAC_REG_R32(R_AX_INDIR_ACCESS_ENTRY + 4);
	if (val32 != 0xBBBBBBBB) {
		ret = MACMEMRO;
		goto ind_aces_end;
	}

ind_aces_end:
	adapter->hw_info->ind_aces_cnt--;
	PLTFM_MUTEX_UNLOCK(&adapter->hw_info->ind_access_lock);
	PLTFM_MSG_WARN("%s ind access end\n", __func__);
	if (ret != MACSUCCESS)
		return ret;

	content = (u8 *)PLTFM_MALLOC(len);
	if (!content) {
		PLTFM_MSG_ERR("[ERR]%s: malloc fail\n", __func__);
		return MACNOBUF;
	}

	PLTFM_MEMCPY(content, buffer, len);

	dl_size = len;
	target_addr = addr;

	PLTFM_MSG_WARN("%s ind access trg 0x%X start\n", __func__, target_addr);
	PLTFM_MUTEX_LOCK(&adapter->hw_info->ind_access_lock);
	adapter->hw_info->ind_aces_cnt++;
	while (dl_size != 0) {
		MAC_REG_W32(R_AX_FILTER_MODEL_ADDR, target_addr);
		write_addr = R_AX_INDIR_ACCESS_ENTRY;

		if (dl_size >= ROMDL_SEG_LEN)
			seg_size = ROMDL_SEG_LEN;
		else
			seg_size = dl_size;

		seg_bytes = seg_size;

		while (seg_bytes != 0) {
			val32 = *((u32 *)(content + index));
			MAC_REG_W32(write_addr,
				    cpu_to_le32(val32));

			seg_bytes -= 4;
			write_addr += 4;
			index += 4;
		}

		target_addr += seg_size;
		dl_size -= seg_size;
	}
	adapter->hw_info->ind_aces_cnt--;
	PLTFM_MUTEX_UNLOCK(&adapter->hw_info->ind_access_lock);
	PLTFM_MSG_WARN("%s ind access trg 0x%X end\n", __func__, target_addr);

	PLTFM_FREE(content, len);

	return MACSUCCESS;
}

static u32 fwdl_phase0(struct mac_ax_adapter *adapter)
{
	u32 cnt = FWDL_WAIT_CNT;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (adapter->sm.fwdl != MAC_AX_FWDL_CPU_ON) {
		PLTFM_MSG_ERR("[ERR]%s: state != CPU_ON\n", __func__);
		return MACPROCERR;
	}

	while (--cnt) {
		if (MAC_REG_R8(R_AX_WCPU_FW_CTRL) & B_AX_H2C_PATH_RDY)
			break;
		PLTFM_DELAY_US(1);
	}

	if (!cnt) {
		PLTFM_MSG_ERR("[ERR]%s: poll 0x1E0[1] = 1 fail\n", __func__);
		return MACPOLLTO;
	}

	adapter->sm.fwdl = MAC_AX_FWDL_H2C_PATH_RDY;

	return MACSUCCESS;
}

static u32 fwdl_phase1(struct mac_ax_adapter *adapter,
		       u8 *fw, u32 hdr_len, u8 redl)
{
	u32 ret;
	u32 cnt = FWDL_WAIT_CNT;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (adapter->sm.fwdl != MAC_AX_FWDL_H2C_PATH_RDY) {
		PLTFM_MSG_ERR("[ERR]%s: state != H2C_PATH_RDY\n", __func__);
		return MACPROCERR;
	}

	ret = __fwhdr_download(adapter, fw, hdr_len, redl);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]%s: __fwhdr_download fail\n", __func__);
		return ret;
	}

	while (--cnt) {
		if (MAC_REG_R8(R_AX_WCPU_FW_CTRL) & B_AX_FWDL_PATH_RDY)
			break;
		PLTFM_DELAY_US(1);
	}

	if (!cnt) {
		PLTFM_MSG_ERR("[ERR]%s: poll 0x1E0[2] = 1 fail\n", __func__);
		return MACPOLLTO;
	}

	MAC_REG_W32(R_AX_HALT_H2C_CTRL, 0);
	MAC_REG_W32(R_AX_HALT_C2H_CTRL, 0);

	adapter->sm.fwdl = MAC_AX_FWDL_PATH_RDY;

	return MACSUCCESS;
}

static u32 check_fw_rdy(struct mac_ax_adapter *adapter)
{
	u32 val8 = FWDL_INITIAL_STATE;
	u32 cnt = FWDL_WAIT_CNT;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	while (--cnt) {
		val8 = GET_FIELD(MAC_REG_R8(R_AX_WCPU_FW_CTRL),
				 B_AX_WCPU_FWDL_STS);
		if (val8 == FWDL_WCPU_FW_INIT_RDY)
			break;
		PLTFM_DELAY_US(1);
	}

	if (!cnt) {
		PLTFM_MSG_ERR("[ERR]%s: poll 0x1E0[7:5] = 7 fail\n", __func__);

		switch (val8) {
		case FWDL_CHECKSUM_FAIL:
			return MACFWCHKSUM;

		case FWDL_SECURITY_FAIL:
			return MACFWSECBOOT;

		case FWDL_CUT_NOT_MATCH:
			return MACFWCUT;

		default:
			return MACPOLLTO;
		}
	}

	adapter->sm.fwdl = MAC_AX_FWDL_INIT_RDY;

	return MACSUCCESS;
}

static u32 fwdl_phase2(struct mac_ax_adapter *adapter, u8 *fw,
		       struct fw_bin_info *info, u8 redl)
{
	u32 ret;
	u32 section_num = info->section_num;
	struct fwhdr_section_info *section_info = info->section_info;

	if (adapter->sm.fwdl != MAC_AX_FWDL_PATH_RDY) {
		PLTFM_MSG_ERR("[ERR]%s: state != FWDL_PATH_RDY\n", __func__);
		return MACPROCERR;
	}

	while (section_num > 0) {
		if (!redl) {
			ret = __sections_download(adapter, section_info);
			if (ret) {
				PLTFM_MSG_ERR("[ERR]%s: ", __func__);
				PLTFM_MSG_ERR("__sections_download fail\n");
				return ret;
			}
		} else {
			if (section_info->redl) {
				ret = __sections_download(adapter,
							  section_info);
				if (ret) {
					PLTFM_MSG_ERR("[ERR]%s: ", __func__);
					PLTFM_MSG_ERR("__sections_download ");
					PLTFM_MSG_ERR("fail\n");
					return ret;
				}
			}
		}
		section_info++;
		section_num--;
	}

	PLTFM_DELAY_MS(5);

	ret = check_fw_rdy(adapter);
	if (ret) {
		PLTFM_MSG_ERR("%s: check_fw_rdy fail\n", __func__);
		return ret;
	}

	return MACSUCCESS;
}

static void fwdl_fail_dump(struct mac_ax_adapter *adapter,
			   struct fw_bin_info *info, u32 ret)
{
	u32 val32, digest_addr, digest;
	u16 index;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
#if MAC_AX_FEATURE_DBGPKG
	struct mac_ax_ops *mac_ops = adapter_to_mac_ops(adapter);
	struct mac_ax_dbgpkg dbg_val = {0};
	struct mac_ax_dbgpkg_en dbg_en = {0};
#endif

	PLTFM_MSG_ERR("[ERR]fwdl ret = %d\n", ret);
	val32 = MAC_REG_R32(R_AX_WCPU_FW_CTRL);
	PLTFM_MSG_ERR("[ERR]fwdl 0x1E0 = 0x%x\n", val32);

	val32 = MAC_REG_R32(R_AX_BOOT_DBG);
	PLTFM_MSG_ERR("[ERR]fwdl 0x83F0 = 0x%x\n", val32);

	val32 = MAC_REG_R32(R_AX_UDM3);
	PLTFM_MSG_ERR("[ERR]fwdl 0x1FC = 0x%x\n", val32);

	val32 = info->git_idx;
	PLTFM_MSG_ERR("[ERR]fw git idx = 0x%x\n", val32);

	PLTFM_MUTEX_LOCK(&adapter->hw_info->dbg_port_lock);
	adapter->hw_info->dbg_port_cnt++;
	if (adapter->hw_info->dbg_port_cnt != 1) {
		PLTFM_MSG_ERR("[ERR]fwdl fail dump lock cnt %d\n",
			      adapter->hw_info->dbg_port_cnt);
		goto end;
	}

	MAC_REG_W32(R_AX_DBG_CTRL, 0xf200f2);
	val32 = MAC_REG_R32(R_AX_SYS_STATUS1);
	val32 = SET_CLR_WORD(val32, 0x1, B_AX_SEL_0XC0);
	MAC_REG_W32(R_AX_SYS_STATUS1, val32);

	for (index = 0; index < 15; index++) {
		val32 = MAC_REG_R32(R_AX_DBG_PORT_SEL);
		PLTFM_MSG_ERR("[ERR]fw PC = 0x%x\n", val32);
		PLTFM_DELAY_US(10);
	}

	PLTFM_MSG_WARN("%s ind access FW digest start\n", __func__);
	PLTFM_MUTEX_LOCK(&adapter->hw_info->ind_access_lock);
	adapter->hw_info->ind_aces_cnt++;
	digest_addr = (0xB8E17C00 + 64) & ~0xA0000000;

	MAC_REG_W32(R_AX_FILTER_MODEL_ADDR, digest_addr);
	PLTFM_MSG_ERR("IRAM digest:");
	for (index = 0; index < FWDL_DIGEST_SIZE; index += 4) {
		digest = MAC_REG_R32(R_AX_INDIR_ACCESS_ENTRY + index);
		PLTFM_MSG_ERR("0x%x\t", digest);
	}

	PLTFM_MSG_ERR("DMEM digest:");
	for (index = 0; index < FWDL_DIGEST_SIZE; index += 4) {
		digest = MAC_REG_R32(R_AX_INDIR_ACCESS_ENTRY +
				     FWDL_DIGEST_SIZE + index);
		PLTFM_MSG_ERR("0x%x\t", digest);
	}
	adapter->hw_info->ind_aces_cnt--;
	PLTFM_MUTEX_UNLOCK(&adapter->hw_info->ind_access_lock);
	PLTFM_MSG_WARN("%s ind access FW digest end\n", __func__);

	pltfm_dbg_dump(adapter);

	mac_ops->dump_ple_dbg_page(adapter, 0);

end:
#if MAC_AX_FEATURE_DBGPKG
	dbg_en.ss_dbg = 0;
	dbg_en.dle_dbg = 1;
	dbg_en.dmac_dbg = 1;
	dbg_en.cmac_dbg = 1;
	dbg_en.mac_dbg_port = 1;
	dbg_en.plersvd_dbg = 1;
	mac_ops->dbg_status_dump(adapter, &dbg_val, &dbg_en);
#endif
	adapter->hw_info->dbg_port_cnt--;
	PLTFM_MUTEX_UNLOCK(&adapter->hw_info->dbg_port_lock);
}

u32 disable_fw_watchdog(struct mac_ax_adapter *adapter)
{
	u32 val32, ret;

	ret = mac_sram_dbg_write(adapter, R_AX_WDT_CTRL,
				 B_AX_WDT_CTRL_ALL_DIS, CPU_LOCAL_SEL);
	if (ret)
		return ret;

	val32 = mac_sram_dbg_read(adapter, R_AX_WDT_STATUS, CPU_LOCAL_SEL);
	val32 = val32 | B_AX_FS_WDT_INT;
	val32 = val32 & (~B_AX_FS_WDT_INT_MSK);
	ret = mac_sram_dbg_write(adapter, R_AX_WDT_STATUS, val32, CPU_LOCAL_SEL);
	if (ret)
		return ret;

	return MACSUCCESS;
}

u32 mac_fwredl(struct mac_ax_adapter *adapter, u8 *fw, u32 len)
{
	u32 val32;
	u32 ret;
	struct fw_bin_info info;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	val32 = MAC_REG_R32(R_AX_WCPU_FW_CTRL);
	val32 &= ~(B_AX_WCPU_FWDL_EN | B_AX_H2C_PATH_RDY | B_AX_FWDL_PATH_RDY);
	val32 = SET_CLR_WORD(val32, FWDL_INITIAL_STATE,
			     B_AX_WCPU_FWDL_STS);

	MAC_REG_W32(R_AX_WCPU_FW_CTRL, val32);

	ret = fwhdr_parser(adapter, fw, len, &info);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]%s: fwhdr_parser fail\n", __func__);
		goto fwdl_err;
	}

	ret = update_fw_ver(adapter, (struct fwhdr_hdr_t *)fw);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]%s: update_fw_ver fail\n", __func__);
		goto fwdl_err;
	}

	adapter->sm.fwdl = MAC_AX_FWDL_H2C_PATH_RDY;

	ret = fwdl_phase1(adapter, fw, info.hdr_len, 1);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]%s: fwdl_phase1 fail\n", __func__);
		goto fwdl_err;
	}

	ret = fwdl_phase2(adapter, fw, &info, 1);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]%s: fwdl_phase2 fail\n", __func__);
		goto fwdl_err;
	}

	mac_scanofld_reset_state(adapter);
	return MACSUCCESS;

fwdl_err:
	fwdl_fail_dump(adapter, &info, ret);

	return ret;
}

u32 mac_fwdl(struct mac_ax_adapter *adapter, u8 *fw, u32 len)
{
	u8 retry_cnt;
	u32 ret;
	struct fw_bin_info info;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	ret = 0;
	retry_cnt = 0;
	MAC_REG_W32(R_AX_UDM1, 0);
	// FWDL retry, for 025 temp workaround
	while (retry_cnt < FWDL_TRY_CNT) {
		if (!fw) {
			PLTFM_MSG_ERR("[ERR]%s: no fw\n", __func__);
			ret = MACNOFW;
			PLTFM_MEMSET(&info, 0, sizeof(struct fw_bin_info));
			goto fwdl_err;
		}

		ret = fwhdr_parser(adapter, fw, len, &info);
		if (ret) {
			PLTFM_MSG_ERR("[ERR]%s: fwhdr_parser fail\n", __func__);
			goto fwdl_err;
		}

		ret = update_fw_ver(adapter, (struct fwhdr_hdr_t *)fw);
		if (ret)
			goto fwdl_err;

		ret = fwdl_phase0(adapter);
		if (ret) {
			PLTFM_MSG_ERR("[ERR]%s: fwdl_phase0 fail\n", __func__);
			goto fwdl_err;
		}

		ret = fwdl_phase1(adapter, fw, info.hdr_len, 0);
		if (ret) {
			PLTFM_MSG_ERR("[ERR]%s: fwdl_phase1 fail\n", __func__);
			goto fwdl_err;
		}

		ret = fwdl_phase2(adapter, fw, &info, 0);
		if (ret) {
			PLTFM_MSG_ERR("[ERR]%s: fwdl_phase2 fail\n", __func__);
			goto fwdl_err;
		}

		return MACSUCCESS;

fwdl_err:
		retry_cnt++;
		PLTFM_MSG_ERR("[ERR]%s: Retry FWDL count %d\n", __func__, retry_cnt);
		// At most retry 2 times
		if (retry_cnt < FWDL_TRY_CNT) {
			ret = mac_disable_cpu(adapter);
			if (ret != MACSUCCESS) {
				PLTFM_MSG_ERR("[ERR]%s: mac_disable_cpu fail\n", __func__);
				fwdl_fail_dump(adapter, &info, ret);
				return ret;
			}

			ret = mac_enable_cpu(adapter, AX_BOOT_REASON_PWR_ON, 1);
			if (ret != MACSUCCESS) {
				PLTFM_MSG_ERR("[ERR]%s: mac_enable_cpu fail\n", __func__);
				fwdl_fail_dump(adapter, &info, ret);
				return ret;
			}
			MAC_REG_W32(R_AX_UDM1, retry_cnt);
		} else {
			break;
		}
	}

	fwdl_fail_dump(adapter, &info, ret);

	return ret;
}

u32 mac_enable_cpu(struct mac_ax_adapter *adapter, u8 boot_reason, u8 dlfw)
{
	u32 val32, ret;
	u16 val16;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (MAC_REG_R32(R_AX_PLATFORM_ENABLE) & B_AX_WCPU_EN)
		return MACCPUSTATE;

	if (adapter->sm.fwdl != MAC_AX_FWDL_IDLE) {
		PLTFM_MSG_ERR("[ERR]%s: state != FWDL_IDLE\n", __func__);
		return MACPROCERR;
	}

	//FW cannot support too much log. Reset R_AX_LDM for FW debug config
	MAC_REG_W32(R_AX_LDM, 0);

	MAC_REG_W32(R_AX_HALT_H2C_CTRL, 0);
	MAC_REG_W32(R_AX_HALT_C2H_CTRL, 0);

	val32 = MAC_REG_R32(R_AX_UDM0);
	val32 &= ~(B_AX_UDM0_DBG_MODE_CTRL | B_AX_UDM0_TRAP_LOOP_CTRL);
	MAC_REG_W32(R_AX_UDM0, val32);

	MAC_REG_W32(R_AX_SYS_CLK_CTRL,
		    MAC_REG_R32(R_AX_SYS_CLK_CTRL) | B_AX_CPU_CLK_EN);

	val32 = MAC_REG_R32(R_AX_WCPU_FW_CTRL);
	val32 &= ~(B_AX_WCPU_FWDL_EN | B_AX_H2C_PATH_RDY | B_AX_FWDL_PATH_RDY);
	val32 = SET_CLR_WORD(val32, FWDL_INITIAL_STATE,
			     B_AX_WCPU_FWDL_STS);

	if (dlfw)
		val32 |= B_AX_WCPU_FWDL_EN;

	MAC_REG_W32(R_AX_WCPU_FW_CTRL, val32);

	val16 = MAC_REG_R16(R_AX_BOOT_REASON);
	val16 = SET_CLR_WORD(val16, boot_reason, B_AX_BOOT_REASON);
	MAC_REG_W16(R_AX_BOOT_REASON, val16);

	val32 = MAC_REG_R32(R_AX_PLATFORM_ENABLE);
	MAC_REG_W32(R_AX_PLATFORM_ENABLE, val32 | B_AX_WCPU_EN);

	adapter->sm.fwdl = MAC_AX_FWDL_CPU_ON;

	if (!dlfw) {
		PLTFM_DELAY_MS(5);

		ret = check_fw_rdy(adapter);
		if (ret) {
			PLTFM_MSG_ERR("[ERR]%s: ", __func__);
			PLTFM_MSG_ERR("check_fw_rdy fail\n");
			return ret;
		}
	}

    // Prevent sequence number in HALMAC and FW mismatching
	reset_lps_seq_num(adapter);

	return MACSUCCESS;
}

u32 mac_disable_cpu(struct mac_ax_adapter *adapter)
{
	u32 val32, ret;

	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	adapter->sm.fwdl = MAC_AX_FWDL_IDLE;
	//todo: need to check cpu in safe state before reset CPU

	val32 = MAC_REG_R32(R_AX_PLATFORM_ENABLE);
	MAC_REG_W32(R_AX_PLATFORM_ENABLE, val32 & ~B_AX_WCPU_EN);

	val32 = MAC_REG_R32(R_AX_UDM0);
	val32 &= ~(B_AX_UDM0_DBG_MODE_CTRL | B_AX_UDM0_TRAP_LOOP_CTRL);
	MAC_REG_W32(R_AX_UDM0, val32);

	val32 = MAC_REG_R32(R_AX_WCPU_FW_CTRL);
	val32 &= ~(B_AX_WCPU_FWDL_EN | B_AX_H2C_PATH_RDY | B_AX_FWDL_PATH_RDY);
	MAC_REG_W32(R_AX_WCPU_FW_CTRL, val32);

	val32 = MAC_REG_R32(R_AX_SYS_CLK_CTRL);
	MAC_REG_W32(R_AX_SYS_CLK_CTRL, val32 & ~B_AX_CPU_CLK_EN);

	ret = disable_fw_watchdog(adapter);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]%s: disable_fw_watchdog fail\n", __func__);
		return ret;
	}

	adapter->sm.plat = MAC_AX_PLAT_OFF;

	val32 = MAC_REG_R32(R_AX_PLATFORM_ENABLE);
	MAC_REG_W32(R_AX_PLATFORM_ENABLE, val32 & ~B_AX_PLATFORM_EN);

	val32 = MAC_REG_R32(R_AX_PLATFORM_ENABLE);
	MAC_REG_W32(R_AX_PLATFORM_ENABLE, val32 | B_AX_PLATFORM_EN);

	adapter->sm.plat = MAC_AX_PLAT_ON;

	return MACSUCCESS;
}

u32 mac_romdl(struct mac_ax_adapter *adapter, u8 *ROM, u32 ROM_addr, u32 len)
{
	u8 *content = NULL;
	u32 val32, ret;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	ret = mac_disable_cpu(adapter);
	if (ret)
		return ret;

	if (!ROM)
		return MACNOITEM;

	val32 = MAC_REG_R32(R_AX_SEC_CTRL);

	if (val32 & BIT(0)) {
		ret = __write_memory(adapter, ROM, ROM_addr, len);
		if (ret)
			return ret;
	} else {
		PLTFM_MSG_ERR("[ERR]%s: __write_memory fail\n", __func__);
		return MACSECUREON;
	}

	PLTFM_FREE(content, len);

	return MACSUCCESS;
}

u32 mac_ram_boot(struct mac_ax_adapter *adapter, u8 *fw, u32 len)
{
	u32 addr;
	u32 ret, section_num;
	struct fw_bin_info info;
	struct fwhdr_section_info *section_info;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	ret = mac_disable_cpu(adapter);
	if (ret)
		goto fwdl_err;

	ret = fwhdr_parser(adapter, fw, len, &info);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]%s: fwhdr_parser fail\n", __func__);
		goto fwdl_err;
	}

	ret = update_fw_ver(adapter, (struct fwhdr_hdr_t *)fw);
	if (ret)
		goto fwdl_err;

	section_num = info.section_num;
	section_info = info.section_info;

	while (section_num > 0) {
		ret = __write_memory(adapter, section_info->addr,
				     section_info->dladdr, section_info->len);
		if (ret)
			goto fwdl_err;

		section_info++;
		section_num--;
	}

	addr = (0xb8003000 + R_AX_CPU_BOOT_ADDR) & 0x1FFFFFFF;
	PLTFM_MSG_WARN("%s ind access 0x%X start\n", __func__, addr);
	PLTFM_MUTEX_LOCK(&adapter->hw_info->ind_access_lock);
	adapter->hw_info->ind_aces_cnt++;
	MAC_REG_W32(R_AX_FILTER_MODEL_ADDR, addr);
	MAC_REG_W32(R_AX_INDIR_ACCESS_ENTRY, (info.section_info[0].dladdr) |
					      0xA0000000);
	adapter->hw_info->ind_aces_cnt--;
	PLTFM_MUTEX_UNLOCK(&adapter->hw_info->ind_access_lock);
	PLTFM_MSG_WARN("%s ind access 0x%X end\n", __func__, addr);

	ret = mac_enable_cpu(adapter, AX_BOOT_REASON_PWR_ON, 0);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]%s: mac_enable_cpu fail\n", __func__);
		goto fwdl_err;
	}

	PLTFM_DELAY_MS(10);

	ret = check_fw_rdy(adapter);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]%s: check_fw_rdy fail\n", __func__);
		goto fwdl_err;
	}
	return MACSUCCESS;

fwdl_err:
	fwdl_fail_dump(adapter, &info, ret);

	return ret;
}

u32 mac_enable_fw(struct mac_ax_adapter *adapter, enum rtw_fw_type cat)
{
	u32 ret = MACSUCCESS;
#if defined(PHL_FEATURE_AP) || defined(PHL_FEATURE_NIC)
	u32 chip_id, cv;
	u32 fw_len = 0;
	u8 *fw = NULL;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	chip_id = GET_FIELD(MAC_REG_R32(R_AX_SYS_CHIPINFO), B_AX_HW_ID);
	cv = GET_FIELD(MAC_REG_R32(R_AX_SYS_CFG1), B_AX_CHIP_VER);

	switch (chip_id) {
#ifdef CONFIG_RTL8852A
	case RTL8852A_ID:
		switch (cv) {
#ifdef MAC_FW_8852A_U2
		case FWDL_CBV:
			switch (cat) {
#ifdef PHL_FEATURE_AP
			case RTW_FW_AP:
				fw_len = array_length_8852a_u2_ap;
				fw = array_8852a_u2_ap;
				break;
#endif /*PHL_FEATURE_AP*/
#ifdef PHL_FEATURE_NIC
			case RTW_FW_NIC:
				fw_len = array_length_8852a_u2_nic;
				fw = array_8852a_u2_nic;
				break;
#ifdef CONFIG_WOWLAN
			case RTW_FW_WOWLAN:
				fw_len = array_length_8852a_u2_wowlan;
				fw = array_8852a_u2_wowlan;
				break;
#endif /*CONFIG_WOWLAN*/
#endif /*PHL_FEATURE_NIC*/
			default:
				PLTFM_MSG_ERR("[ERR]%s: no cat\n", __func__);
				fw_len = 0;
				fw = 0;
				break;
			}
			break;
#endif /*MAC_FW_8852A_U2*/
#ifdef MAC_FW_8852A_U3
		case FWDL_CCV:
			switch (cat) {
#ifdef PHL_FEATURE_AP
			case RTW_FW_AP:
				fw_len = array_length_8852a_u3_ap;
				fw = array_8852a_u3_ap;
				break;
#endif /*PHL_FEATURE_AP*/
#ifdef PHL_FEATURE_NIC
			case RTW_FW_NIC:
				fw_len = array_length_8852a_u3_nic;
				fw = array_8852a_u3_nic;
				break;
#ifdef CONFIG_WOWLAN
			case RTW_FW_WOWLAN:
				fw_len = array_length_8852a_u3_wowlan;
				fw = array_8852a_u3_wowlan;
				break;
#endif /*CONFIG_WOWLAN*/
#endif /*PHL_FEATURE_NIC*/
			default:
				PLTFM_MSG_ERR("[ERR]%s: no cat\n", __func__);
				fw_len = 0;
				fw = 0;
				break;
			}
			break;
#endif /*MAC_FW_8852A_U3*/
		default:
			PLTFM_MSG_ERR("[ERR]%s: invalid cut\n", __func__);
			fw_len = 0;
			fw = 0;
			break;
		}
		break;
#endif /*CONFIG_RTL8852A*/
#ifdef CONFIG_RTL8852B
	case RTL8852B_ID:
		switch (cv) {
#ifdef MAC_FW_8852B_U1
		case FWDL_CAV:
			switch (cat) {
#ifdef PHL_FEATURE_AP
			case RTW_FW_AP:
				fw_len = array_length_8852b_u1_ap;
				fw = array_8852b_u1_ap;
				break;
#endif /*PHL_FEATURE_AP*/
#ifdef PHL_FEATURE_NIC
			case RTW_FW_NIC:
				fw_len = array_length_8852b_u1_nic;
				fw = array_8852b_u1_nic;
				break;
#ifdef CONFIG_WOWLAN
			case RTW_FW_WOWLAN:
				fw_len = array_length_8852b_u1_wowlan;
				fw = array_8852b_u1_wowlan;
				break;
#endif /*CONFIG_WOWLAN*/
#endif /*PHL_FEATURE_NIC*/
			default:
				PLTFM_MSG_ERR("[ERR]%s: no cat\n", __func__);
				fw_len = 0;
				fw = 0;
				break;
			}
			break;
#endif /*MAC_FW_8852B_U1*/
#ifdef MAC_FW_8852B_U2
		case FWDL_CBV:
			switch (cat) {
#ifdef PHL_FEATURE_NIC
			case RTW_FW_NIC:
				fw_len = array_length_8852b_u2_nic;
				fw = array_8852b_u2_nic;
				break;
#ifdef CONFIG_WOWLAN
			case RTW_FW_WOWLAN:
				fw_len = array_length_8852b_u2_wowlan;
				fw = array_8852b_u2_wowlan;
				break;
#endif /*CONFIG_WOWLAN*/
#endif /*PHL_FEATURE_NIC*/
			default:
				PLTFM_MSG_ERR("[ERR]%s: no cat\n", __func__);
				fw_len = 0;
				fw = 0;
				break;
			}
			break;
#endif /*MAC_FW_8852B_U2*/
		default:
			PLTFM_MSG_ERR("[ERR]%s: invalid cut\n", __func__);
			fw_len = 0;
			fw = 0;
			break;
		}
		break;
#endif /*CONFIG_RTL8852B*/
#ifdef CONFIG_RTL8852C
	case RTL8852C_ID:
		switch (cv) {
#ifdef MAC_FW_8852C_U1
		case FWDL_CAV:
			switch (cat) {
#ifdef PHL_FEATURE_AP
			case RTW_FW_AP:
				fw_len = array_length_8852c_u1_ap;
				fw = array_8852c_u1_ap;
				break;
#endif /*PHL_FEATURE_AP*/
#ifdef PHL_FEATURE_NIC
			case RTW_FW_NIC:
				fw_len = array_length_8852c_u1_nic;
				fw = array_8852c_u1_nic;
				break;
#ifdef CONFIG_WOWLAN
			case RTW_FW_WOWLAN:
				fw_len = array_length_8852c_u1_wowlan;
				fw = array_8852c_u1_wowlan;
				break;
#endif /*CONFIG_WOWLAN*/
#endif /*PHL_FEATURE_NIC*/
			default:
				PLTFM_MSG_ERR("[ERR]%s: no cat\n", __func__);
				fw_len = 0;
				fw = 0;
				break;
				}
				break;
#endif /*MAC_FW_8852C_U1*/
		default:
			PLTFM_MSG_ERR("[ERR]%s: invalid cut\n", __func__);
			fw_len = 0;
			fw = 0;
			break;
		}
		break;
#endif /*CONFIG_RTL8852C*/

	default:
		PLTFM_MSG_ERR("[ERR]%s: invalid chip\n", __func__);
		fw_len = 0;
		fw = 0;
		break;
	}

	ret = mac_disable_cpu(adapter);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]%s: mac_disable_cpu fail\n", __func__);
		return ret;
	}

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8852C)) {
		ret = mac_check_OTP(adapter, 1);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]%s: mac_check_OTP fail\n",
				      __func__);
			return ret;
		}
	}

	ret = mac_enable_cpu(adapter, AX_BOOT_REASON_PWR_ON, 1);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]%s: mac_enable_cpu fail\n", __func__);
		return ret;
	}

	ret = mac_fwdl(adapter, fw, fw_len);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]%s: mac_enable_cpu fail\n", __func__);
		return ret;
	}

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8852C)) {
		ret = mac_check_OTP(adapter, 0);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]%s: mac_check_OTP fail\n",
				      __func__);
			return ret;
		}
	}

#endif /* #if defined(PHL_FEATURE_AP) || defined(PHL_FEATURE_NIC) */
	return ret;
}
