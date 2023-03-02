/******************************************************************************
 *
 * Copyright(c) 2019 Realtek Corporation.
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
 *****************************************************************************/
#include "../halrf_precomp.h"

#ifdef RF_8852B_SUPPORT

void _halrf_psd_backup_bb_registers_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	u32 *reg,
	u32 *reg_backup,
	u32 reg_num)
{
	u32 i;

	for (i = 0; i < reg_num; i++) {
		reg_backup[i] = halrf_rreg(rf, reg[i], MASKDWORD);

		RF_DBG(rf, DBG_RF_PSD, "[TXGAPK] Backup BB 0x%08x = 0x%08x\n",
		       reg[i], reg_backup[i]);
	}
}

void _halrf_psd_reload_bb_registers_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	u32 *reg,
	u32 *reg_backup,
	u32 reg_num)

{
	u32 i;

	for (i = 0; i < reg_num; i++) {
		halrf_wreg(rf, reg[i], MASKDWORD, reg_backup[i]);

		RF_DBG(rf, DBG_RF_PSD, "[TXGAPK] Reload BB 0x%08x = 0x%08x\n",
		       reg[i], reg_backup[i]);
	}
}

void halrf_psd_init_8852b(struct rf_info *rf, enum phl_phy_idx phy,
			u8 path, u8 iq_path, u32 avg, u32 fft)
{
	struct halrf_psd_data *psd_info = &rf->psd;
	u32 bb_reg[PSD_BACKUP_NUM_8852B] = {
			0x20fc, 0x5864, 0x7864, 0x12b8, 0x32b8,
			0x030c, 0x032c, 0x58c8, 0x78c8,	0x2008,
			0x0c1c, 0x0700,	0x0c70, 0x0c60, 0x0c6c,
			0x58ac, 0x78ac, 0x0c3c, 0x2320, 0x4490,
			0x12a0, 0x32a0, 0x8008, 0x8080, 0x8088,
			0x80d0, 0x8074, 0x81dc, 0x82dc, 0x8120,
			0x8220,	0x8018, 0x8000, 0x800c, 0x81cc,
			0x82cc, 0x802c, 0x8034, 0x80d4, 0x80fc,
			0x801c};

	RF_DBG(rf, DBG_RF_PSD, "======> %s   phy=%d\n", __func__, phy);

	psd_info->psd_progress = 1;

	_halrf_psd_backup_bb_registers_8852b(rf, phy, bb_reg,
			psd_info->psd_reg_backup, PSD_BACKUP_NUM_8852B);

	psd_info->path = path;
	psd_info->iq_path = iq_path;
	psd_info->avg = avg;
	psd_info->fft = fft;

	/*02_BB_AFE_PHY0*/
	halrf_wreg(rf, 0x20fc, 0xffff0000, 0x0303);
	halrf_wreg(rf, 0x5864, 0x18000000, 0x3);
	halrf_wreg(rf, 0x7864, 0x18000000, 0x3);
	halrf_wreg(rf, 0x12b8, 0x40000000, 0x1);
	halrf_wreg(rf, 0x32b8, 0x40000000, 0x1);
	halrf_wreg(rf, 0x030c, 0xff000000, 0x13);
	halrf_wreg(rf, 0x032c, 0xffff0000, 0x0041);
	halrf_wreg(rf, 0x12b8, 0x10000000, 0x1);
	halrf_wreg(rf, 0x58c8, 0x01000000, 0x1);
	halrf_wreg(rf, 0x78c8, 0x01000000, 0x1);
	halrf_wreg(rf, 0x5864, 0xc0000000, 0x3);
	halrf_wreg(rf, 0x7864, 0xc0000000, 0x3);
	halrf_wreg(rf, 0x2008, 0x01ffffff, 0x1ffffff);
	halrf_wreg(rf, 0x0c1c, 0x00000004, 0x1);
	halrf_wreg(rf, 0x0700, 0x08000000, 0x1);
	halrf_wreg(rf, 0x0c70, 0x000003ff, 0x3ff);
	halrf_wreg(rf, 0x0c60, 0x00000003, 0x3);
	halrf_wreg(rf, 0x0c6c, 0x00000001, 0x1);
	halrf_wreg(rf, 0x58ac, 0x08000000, 0x1);
	halrf_wreg(rf, 0x78ac, 0x08000000, 0x1);
	halrf_wreg(rf, 0x0c3c, 0x00000200, 0x1);
	halrf_wreg(rf, 0x2320, 0x00000001, 0x1);
	halrf_wreg(rf, 0x4490, 0x80000000, 0x1);
	halrf_wreg(rf, 0x12a0, 0x00007000, 0x7);
	halrf_wreg(rf, 0x12a0, 0x00008000, 0x1);
	halrf_wreg(rf, 0x12a0, 0x00070000, 0x3);
	halrf_wreg(rf, 0x12a0, 0x00080000, 0x1);
	halrf_wreg(rf, 0x32a0, 0x00070000, 0x3);
	halrf_wreg(rf, 0x32a0, 0x00080000, 0x1);
	halrf_wreg(rf, 0x0700, 0x01000000, 0x1);
	halrf_wreg(rf, 0x0700, 0x06000000, 0x2);
	halrf_wreg(rf, 0x20fc, 0xffff0000, 0x3333);
	/*03_IQK_Preset_Non_DBCC_PHY0*/
	halrf_wrf(rf, RF_PATH_A, 0x5, 0x00001, 0x0);
	halrf_wrf(rf, RF_PATH_B, 0x5, 0x00001, 0x0);
	halrf_wreg(rf, 0x8008, 0xffffffff, 0x00000080);
	halrf_wreg(rf, 0x8080, 0xffffffff, 0x00000000);
	halrf_wreg(rf, 0x8088, 0xffffffff, 0x81ff010a);
	halrf_wreg(rf, 0x80d0, 0xffffffff, 0x00200000);
	halrf_wreg(rf, 0x8074, 0xffffffff, 0x80000000);
	halrf_wreg(rf, 0x81dc, 0xffffffff, 0x00000000);
	halrf_wreg(rf, 0x82dc, 0xffffffff, 0x00000000);

	/*07_CLK_Setting_RxIQK_BW80M_Non_DBCC_PHY0*/				
	halrf_wreg(rf, 0x8120, 0xffffffff, 0x4d000a08);
	halrf_wreg(rf, 0x8220, 0xffffffff, 0x4d000a08);
	halrf_wreg(rf, 0x12a0, 0x00070000, 0x2);
	halrf_wreg(rf, 0x12a0, 0x00080000, 0x1);
	halrf_wreg(rf, 0x32a0, 0x00070000, 0x2);
	halrf_wreg(rf, 0x32a0, 0x00080000, 0x1);
	halrf_wreg(rf, 0x0700, 0x01000000, 0x1);
	halrf_wreg(rf, 0x0700, 0x06000000, 0x1);

	/*windowing*/
	halrf_wreg(rf, 0x8018, 0xfffffffe, 0x20008080);

	if (fft == 160)
		halrf_wreg(rf, 0x801c, 0x00003000, 0x0);
	else if (fft == 320)
		halrf_wreg(rf, 0x801c, 0x00003000, 0x1);
	else if (fft == 640)
		halrf_wreg(rf, 0x801c, 0x00003000, 0x2);
	else /*1280*/
		halrf_wreg(rf, 0x801c, 0x00003000, 0x3);

	if (avg == 1)
		halrf_wreg(rf, 0x801c, 0x000e0000, 0x0);
	else if (avg == 2)
		halrf_wreg(rf, 0x801c, 0x000e0000, 0x1);
	else if (avg == 4)
		halrf_wreg(rf, 0x801c, 0x000e0000, 0x2);
	else if (avg == 8)
		halrf_wreg(rf, 0x801c, 0x000e0000, 0x3);
	else if (avg == 16)
		halrf_wreg(rf, 0x801c, 0x000e0000, 0x4);
	else if (avg == 64)
		halrf_wreg(rf, 0x801c, 0x000e0000, 0x6);
	else if (avg == 128)
		halrf_wreg(rf, 0x801c, 0x000e0000, 0x7);
	else	/*32*/
		halrf_wreg(rf, 0x801c, 0x000e0000, 0x5);

}

void halrf_psd_restore_8852b(struct rf_info *rf, enum phl_phy_idx phy)
{
	struct halrf_psd_data *psd_info = &rf->psd;
	u32 bb_reg[PSD_BACKUP_NUM_8852B] = {
			0x20fc, 0x5864, 0x7864, 0x12b8, 0x32b8,
			0x030c, 0x032c, 0x58c8, 0x78c8,	0x2008,
			0x0c1c, 0x0700,	0x0c70, 0x0c60, 0x0c6c,
			0x58ac, 0x78ac, 0x0c3c, 0x2320, 0x4490,
			0x12a0, 0x32a0, 0x8008, 0x8080, 0x8088,
			0x80d0, 0x8074, 0x81dc, 0x82dc, 0x8120,
			0x8220,	0x8018, 0x8000, 0x800c, 0x81cc,
			0x82cc, 0x802c, 0x8034, 0x80d4, 0x80fc,
			0x801c};

	RF_DBG(rf, DBG_RF_PSD, "======> %s   phy=%d\n", __func__, phy);

	_halrf_psd_reload_bb_registers_8852b(rf, phy, bb_reg,
			psd_info->psd_reg_backup, PSD_BACKUP_NUM_8852B);

	halrf_wrf(rf, RF_PATH_A, 0x5, 0x00001, 0x1);
	halrf_wrf(rf, RF_PATH_B, 0x5, 0x00001, 0x1);

	psd_info->psd_progress = 0;

#if 0
	/*98_IQK_Reg_Non_DBCC_PHY0_path01_Restore*/
	halrf_wreg(rf, 0x8008, 0xffffffff, 0x00000000);
	halrf_wreg(rf, 0x8074, 0xffffffff, 0x00000000);
	halrf_wreg(rf, 0x8088, 0xffffffff, 0x80000000);
	halrf_wreg(rf, 0x80d0, 0xffffffff, 0x00000000);
	halrf_wreg(rf, 0x80e0, 0x00000001, 0x0);
	halrf_wreg(rf, 0x8120, 0xffffffff, 0x10010000);
	halrf_wreg(rf, 0x8140, 0x00000100, 0x0);
	halrf_wreg(rf, 0x8150, 0xffffffff, 0xe4e4e4e4);
	halrf_wreg(rf, 0x8154, 0x00000100, 0x0);
	halrf_wreg(rf, 0x81cc, 0x0000003f, 0x0);
	halrf_wreg(rf, 0x81dc, 0xffffffff, 0x00000002);
	halrf_wreg(rf, 0x8220, 0xffffffff, 0x10010000);
	halrf_wreg(rf, 0x8240, 0x00000100, 0x0);
	halrf_wreg(rf, 0x8250, 0xffffffff, 0xe4e4e4e4);
	halrf_wreg(rf, 0x8254, 0x00000100, 0x0);
	halrf_wreg(rf, 0x82cc, 0x0000003f, 0x0);
	halrf_wreg(rf, 0x82dc, 0xffffffff, 0x00000002);

	/*99_BB_AFE_Non_DBCC_PHY0_path01_restore*/
	halrf_wreg(rf, 0x20fc, 0xffff0000, 0x0303);
	halrf_wreg(rf, 0x12b8, 0x40000000, 0x0);
	halrf_wreg(rf, 0x32b8, 0x40000000, 0x0);
	halrf_wreg(rf, 0x5864, 0xc0000000, 0x0);
	halrf_wreg(rf, 0x7864, 0xc0000000, 0x0);
	halrf_wreg(rf, 0x2008, 0x01ffffff, 0x0000000);
	halrf_wreg(rf, 0x0c1c, 0x00000004, 0x0);
	halrf_wreg(rf, 0x0700, 0x08000000, 0x0);
	halrf_wreg(rf, 0x0c70, 0x0000001f, 0x03);
	halrf_wreg(rf, 0x0c70, 0x000003e0, 0x03);
	halrf_wreg(rf, 0x12a0, 0x000ff000, 0x00);
	halrf_wreg(rf, 0x32a0, 0x000ff000, 0x00);
	halrf_wreg(rf, 0x0700, 0x07000000, 0x0);
	halrf_wreg(rf, 0x0c3c, 0x00000200, 0x0);
	halrf_wreg(rf, 0x2320, 0x00000001, 0x0);
	halrf_wreg(rf, 0x20fc, 0xffff0000, 0x0000);
	halrf_wreg(rf, 0x58c8, 0x01000000, 0x0);
	halrf_wreg(rf, 0x78c8, 0x01000000, 0x0);
#endif
}

u32 halrf_psd_get_point_data_8852b(struct rf_info *rf,
			enum phl_phy_idx phy, s32 point)
{
	struct halrf_psd_data *psd_info = &rf->psd;
	u32 val_tmp, val, data;

	halrf_wreg(rf, 0x8088, 0xffffffff, 0x81ff0109);

	if (psd_info->path == RF_PATH_A)
		halrf_wreg(rf, 0x8000, 0xffffffff, 0x00000008);
	else
		halrf_wreg(rf, 0x8000, 0xffffffff, 0x0000000a);

	halrf_wreg(rf, 0x800c, 0xffffffff, 0x00000c00);
	halrf_wreg(rf, 0x8018, 0xfffffffe, 0x20008080);
	halrf_wreg(rf, 0x80d0, 0xffffffff, 0x00300000);

	if (psd_info->path == RF_PATH_A)
		halrf_wreg(rf, 0x81cc, 0x0000003f, 0x3f);
	else
		halrf_wreg(rf, 0x82cc, 0x0000003f, 0x3f);

	halrf_wreg(rf, 0x802c, 0x0fff0000, (point & 0xfff));
	halrf_wreg(rf, 0x8034, 0x00000001, 0x1);
	halrf_wreg(rf, 0x8034, 0x00000001, 0x0);
	halrf_delay_ms(rf, 1);

	halrf_wreg(rf, 0x80d4, 0xffffffff, 0x002d0000);
	val_tmp = halrf_rreg(rf, 0x80fc, 0x007f0000);
	halrf_wreg(rf, 0x80d4, 0xffffffff, 0x002e0000);
	val = halrf_rreg(rf, 0x80fc, 0xffffffff);

	data = (val_tmp << 25) | (val >> 7);

	RF_DBG(rf, DBG_RF_PSD, "======> %s   phy=%d   point=0x%x   data=0x%08x\n",
		__func__, phy, point, data);

	return data;
}

void halrf_psd_query_8852b(struct rf_info *rf, enum phl_phy_idx phy,
			u32 point, u32 start_point, u32 stop_point, u32 *outbuf)
{
	struct halrf_psd_data *psd_info = &rf->psd;
	struct rtw_hal_com_t *hal = rf->hal_com;
	u32 i = 0, j = 0;
	s32 point_temp;

	RF_DBG(rf, DBG_RF_PSD, "======> %s phy=%d point=%d start_point=%d stop_point=%d\n",
		__func__, phy, point, start_point, stop_point);

	if (psd_info->psd_result_running == true) {
		RF_DBG(rf, DBG_RF_PSD, "======> %s PSD Running Return !!!\n", __func__);
		return;
	}

	psd_info->psd_result_running = true;

	hal_mem_set(hal, psd_info->psd_data, 0, sizeof(psd_info->psd_data));

	i = start_point;
	while (i < stop_point) {
		if (i >= point)
			point_temp = i - point;
		else
		{
			point_temp = i - point;
			point_temp = point_temp & 0xfff;
		}

		psd_info->psd_data[j] = halrf_psd_get_point_data_8852b(rf, phy, point_temp);

		i++;
		j++;
	}

	RF_DBG(rf, DBG_RF_PSD, "psd_info->psd_data\n");

	for (i = 0; i < 320; i = i + 10) {
		RF_DBG(rf, DBG_RF_PSD, "%d  %d  %d  %d  %d  %d  %d  %d  %d  %d\n",
			psd_info->psd_data[i], psd_info->psd_data[i + 1],
			psd_info->psd_data[i + 2], psd_info->psd_data[i + 3],
			psd_info->psd_data[i + 4], psd_info->psd_data[i + 5],
			psd_info->psd_data[i + 6], psd_info->psd_data[i + 7],
			psd_info->psd_data[i + 8], psd_info->psd_data[i + 9]);
	}

	hal_mem_cpy(hal, outbuf, psd_info->psd_data,
		sizeof(psd_info->psd_data));

	RF_DBG(rf, DBG_RF_PSD, "======> %s PSD End !!!\n", __func__);

	psd_info->psd_result_running = false;

}

#endif	/*RF_8852B_SUPPORT*/