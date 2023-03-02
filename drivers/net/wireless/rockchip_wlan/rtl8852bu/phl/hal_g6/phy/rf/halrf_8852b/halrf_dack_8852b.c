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

#define t_avg 100


void halrf_afe_init_8852b(struct rf_info *rf)
{
	halrf_wmac32(rf, 0x8040, 0xf);
	halrf_wreg(rf, 0xc0d4, MASKDWORD, 0x4486888c);
	halrf_wreg(rf, 0xc0d8, MASKDWORD, 0xc6ba10e0);
	halrf_wreg(rf, 0xc0dc, MASKDWORD, 0x30c52868);
	halrf_wreg(rf, 0xc0e0, MASKDWORD, 0x05008128);
	halrf_wreg(rf, 0xc0e4, MASKDWORD, 0x0000272b);
	halrf_wreg(rf, 0xc1d4, MASKDWORD, 0x4486888c);
	halrf_wreg(rf, 0xc1d8, MASKDWORD, 0xc6ba10e0);
	halrf_wreg(rf, 0xc1dc, MASKDWORD, 0x30c52868);
	halrf_wreg(rf, 0xc1e0, MASKDWORD, 0x05008128);
	halrf_wreg(rf, 0xc1e4, MASKDWORD, 0x0000272b);
}
void halrf_dack_init_8852b(struct rf_info *rf)
{
}

void halrf_drck_8852b(struct rf_info *rf)
{
	u32 c;
	u32 rck_d;
	RF_DBG(rf, DBG_RF_DACK, "[DACK]Ddie RCK start!!!\n");	
	halrf_wreg(rf, 0xc0cc, BIT(6), 0x1);
	c = 0;
	while (halrf_rreg(rf, 0xc0d0, BIT(3)) == 0) {
		c++;
		halrf_delay_us(rf, 1);
		if (c > 10000) {
			RF_DBG(rf, DBG_RF_DACK, "[DACK]DRCK timeout\n");
			break;
		}
	}

	halrf_wreg(rf, 0xc0cc, BIT(6), 0x0);
	halrf_wreg(rf, 0xc094, BIT(9), 0x1);
	halrf_delay_us(rf, 1);
	halrf_wreg(rf, 0xc094, BIT(9), 0x0);

	/*manual write for LPS*/
	rck_d = halrf_rreg(rf, 0xc0d0, 0xf8000);
	/*RCK_SEL=0*/
	halrf_wreg(rf, 0xc0cc, BIT(9), 0x0);
	halrf_wreg(rf, 0xc0cc, 0x1f, rck_d);
	RF_DBG(rf, DBG_RF_DACK, "[DACK]0xc0cc = 0x%x\n", halrf_rreg(rf, 0xc0cc, MASKDWORD));
}

void halrf_addck_backup_8852b(struct rf_info *rf)
{
	struct halrf_dack_info *dack = &rf->dack;

	halrf_wreg(rf, 0xc0f4, 0x300, 0x0);
	dack->addck_d[0][0] = (u16)halrf_rreg(rf, 0xc0fc,0xffc00) ;
	dack->addck_d[0][1] = (u16)halrf_rreg(rf, 0xc0fc,0x003ff) ;

	halrf_wreg(rf, 0xc1f4, 0x300, 0x0);
	dack->addck_d[1][0] = (u16)halrf_rreg(rf, 0xc1fc,0xffc00) ;
	dack->addck_d[1][1] = (u16)halrf_rreg(rf, 0xc1fc,0x003ff) ;
}

void halrf_addck_reload_8852b(struct rf_info *rf)
{
	struct halrf_dack_info *dack = &rf->dack;
	/*S0*/
	halrf_wreg(rf, 0xc0f0, 0x3ff0000, dack->addck_d[0][0]);
	halrf_wreg(rf, 0xc0f4, 0xf, (dack->addck_d[0][1] >> 6));
	halrf_wreg(rf, 0xc0f0, 0xfc000000, (dack->addck_d[0][1] & 0x3f));
	/*manual*/
	halrf_wreg(rf, 0xc0f4, 0x30, 0x3);
	/*S1*/
	halrf_wreg(rf, 0xc1f0, 0x3ff0000, dack->addck_d[1][0]);
	halrf_wreg(rf, 0xc1f4, 0xf, (dack->addck_d[1][1] >> 6));
	halrf_wreg(rf, 0xc1f0, 0xfc000000, (dack->addck_d[1][1] & 0x3f));
	/*manual*/
	halrf_wreg(rf, 0xc1f4, 0x30, 0x3);
}

void halrf_dack_backup_s0_8852b(struct rf_info *rf)
{
	struct halrf_dack_info *dack = &rf->dack;
	u8 i;
//	u32 temp;

//	halrf_wreg(rf, 0x5e00, BIT(3), 0x1);
//	halrf_wreg(rf, 0x5e50, BIT(3), 0x1);
	halrf_wreg(rf, 0x12b8, BIT(30), 0x1);
	/*MSBK*/
#if 0
	for (i = 0; i < 0x10; i++) {
		/*S0*/
		halrf_wreg(rf, 0xc000, 0x1e, i);
		temp = (u8)halrf_rreg(rf, 0xc04c, 0x7fc0);
		RF_DBG(rf, DBG_RF_DACK, "[DACK]0xc04c %d = 0x%x\n",
		i, temp);
	}

	for (i = 0; i < 0x10; i++) {
		/*S0*/
		halrf_wreg(rf, 0xc020, 0x1e, i);
		temp = (u8)halrf_rreg(rf, 0xc070, 0x7fc0);
		RF_DBG(rf, DBG_RF_DACK, "[DACK]0xc070 %d = 0x%x\n",
		i, temp);
	}
#endif
	for (i = 0; i < 0x10; i++) {
		/*S0*/
		halrf_wreg(rf, 0xc000, 0x1e, i);
		dack->msbk_d[0][0][i] = (u8)halrf_rreg(rf, 0xc05c, 0xff000000);
		halrf_wreg(rf, 0xc020, 0x1e, i);
		dack->msbk_d[0][1][i] = (u8)halrf_rreg(rf, 0xc080, 0xff000000);
	}


	/*biasK*/
	dack->biask_d[0][0] = (u16)halrf_rreg(rf, 0xc048, 0xffc);
	dack->biask_d[0][1] = (u16)halrf_rreg(rf, 0xc06c, 0xffc);
	/*DADCK*/
	dack->dadck_d[0][0] = (u8)halrf_rreg(rf, 0xc060, 0xff000000);
	dack->dadck_d[0][1] = (u8)halrf_rreg(rf, 0xc084, 0xff000000);
}

void halrf_dack_backup_s1_8852b(struct rf_info *rf)
{
	struct halrf_dack_info *dack = &rf->dack;
	u8 i;
//	u32 temp;

//	halrf_wreg(rf, 0x7e00, BIT(3), 0x1);
//	halrf_wreg(rf, 0x7e50, BIT(3), 0x1);
	halrf_wreg(rf, 0x32b8, BIT(30), 0x1);
	/*MSBK*/
#if 0
	for (i = 0; i < 0x10; i++) {
		/*S0*/
		halrf_wreg(rf, 0xc100, 0x1e, i);
		temp = (u8)halrf_rreg(rf, 0xc14c, 0x7fc0);
		RF_DBG(rf, DBG_RF_DACK, "[DACK]0xc14c %d = 0x%x\n",
		i, temp);
	}

	for (i = 0; i < 0x10; i++) {
		/*S0*/
		halrf_wreg(rf, 0xc120, 0x1e, i);
		temp = (u8)halrf_rreg(rf, 0xc170, 0x7fc0);
		RF_DBG(rf, DBG_RF_DACK, "[DACK]0xc170 %d = 0x%x\n",
		i, temp);
	}
#endif

	for (i = 0; i < 0x10; i++) {
		/*S1*/
		halrf_wreg(rf, 0xc100, 0x1e, i);
		dack->msbk_d[1][0][i] = (u8)halrf_rreg(rf, 0xc15c, 0xff000000);
		halrf_wreg(rf, 0xc120, 0x1e, i);
		dack->msbk_d[1][1][i] = (u8)halrf_rreg(rf, 0xc180, 0xff000000);
	}
	/*biasK*/
	dack->biask_d[1][0] = (u16)halrf_rreg(rf, 0xc148, 0xffc);
	dack->biask_d[1][1] = (u16)halrf_rreg(rf, 0xc16c, 0xffc);
	/*DADCK*/
	dack->dadck_d[1][0] = (u8)halrf_rreg(rf, 0xc160, 0xff000000);
	dack->dadck_d[1][1] = (u8)halrf_rreg(rf, 0xc184, 0xff000000);
}

void halrf_dack_reload_by_path_8852b(struct rf_info *rf, enum rf_path path)
{
	struct halrf_dack_info *dack = &rf->dack;
	u8 i;
	u32 oft;

	if (path == RF_PATH_A)
		oft = 0;
	else
		oft = 0x100;
	/*MSBK*/
	for (i = 0; i < 16; i++) {
		halrf_wreg(rf, 0xc000+ oft, 0x1e, i);
		halrf_wreg(rf, 0xc004+ oft, 0x3fc0, dack->msbk_d[path][0][i]);
	}
	for (i = 0; i < 16; i++) {
		halrf_wreg(rf, 0xc020+ oft, 0x1e, i);
		halrf_wreg(rf, 0xc024+ oft, 0x3fc0, dack->msbk_d[path][1][i]);
	}

	/*biask*/
	halrf_wreg(rf, 0xc004+ oft, 0x3ff00000, dack->biask_d[path][0]);
	halrf_wreg(rf, 0xc024+ oft, 0x3ff00000, dack->biask_d[path][1]);
	/*DADCK*/
}

void halrf_dack_reload_8852b(struct rf_info *rf, enum rf_path path)
{
	u32 oft;

	if (path == RF_PATH_A)
		oft = 0;
	else
		oft = 0x100;

	halrf_wreg(rf, 0xc004 + oft, 0x3, 0x2);
	halrf_wreg(rf, 0xc024 + oft, 0x3, 0x2);
	halrf_wreg(rf, 0xc004 + oft, BIT(5), 0x1);
	halrf_wreg(rf, 0xc024 + oft, BIT(5), 0x1);
	halrf_dack_reload_by_path_8852b(rf, path);
	halrf_wreg(rf, 0xc004 + oft, 0x3, 0x0);
	halrf_wreg(rf, 0xc024 + oft, 0x3, 0x0);
}

void halrf_check_addc_8852b(struct rf_info *rf, enum rf_path path)
{
	u32 temp, dc_re, dc_im;
	u32 i, m, p, t;
	u32 re[t_avg], im[t_avg];
#if 1
	halrf_wreg(rf, 0x20f4, BIT(24), 0x0);
	halrf_wreg(rf, 0x20f8, 0x80000000, 0x1);
	halrf_wreg(rf, 0x20f0, 0xff0000, 0x1);
	halrf_wreg(rf, 0x20f0, 0xf00, 0x2);
	halrf_wreg(rf, 0x20f0, 0xf, 0x0);
	if (path  == RF_PATH_A)
		halrf_wreg(rf, 0x20f0, 0xc0, 0x2);
	else
		halrf_wreg(rf, 0x20f0, 0xc0, 0x3);
	for (i = 0; i < t_avg; i++) {
		temp = halrf_rreg(rf, 0x1730, 0xffffffff);
		re[i] = (temp & 0xfff000) >> 12;
		im[i] = temp & 0xfff;
//		RF_DBG(rf, DBG_RF_DACK, "[DACK]S%d,re[i]= 0x%x,im[i] =0x%x\n",
//		path, re[i], im[i]);
	}
#else
	for (i = 0; i < t_avg; i++) {
		if (path  == RF_PATH_A)
			temp = halrf_rreg(rf, 0x1c8c, MASKDWORD);
		else
			temp = halrf_rreg(rf, 0x3c8c, MASKDWORD);

		re[i] = (temp & 0xfff000) >> 12;
		im[i] = temp & 0xfff;
//		RF_DBG(rf, DBG_RF_DACK, "[DACK]S%d,re[i]= 0x%x,im[i] =0x%x\n",
//		path, re[i], im[i]);
	}

#endif
	m = 0;
	p = 0;
	for (i = 0; i < t_avg; i++) {
		if (re[i] > 0x800)
			m = (0x1000 - re[i]) + m;
		else
			p = re[i] + p;
	}

	if (p > m) {
		t = p - m;
		t = t / t_avg;
	} else {
		t = m - p;
		t = t / t_avg;
		if (t != 0x0)
			t = 0x1000 - t;
	}
	dc_re = t;

	m = 0;
	p = 0;
	for (i = 0; i < t_avg; i++) {
		if (im[i] > 0x800)
			m = (0x1000 - im[i]) + m;
		else
			p = im[i] + p;
	}

	if (p > m) {
		t = p - m;
		t = t / t_avg;
	} else {
		t = m - p;
		t = t / t_avg;
		if (t != 0x0)
			t = 0x1000 - t;
	}
	dc_im = t;

	RF_DBG(rf, DBG_RF_DACK, "[DACK]S%d,dc_re = 0x%x,dc_im =0x%x\n",
		path, dc_re, dc_im);
}

void halrf_addck_8852b(struct rf_info *rf)
{
	struct halrf_dack_info *dack = &rf->dack;
	u32 c;
	/*S0*/
#if 1
	/*manual off*/
	halrf_wreg(rf, 0xc0f4, 0x30, 0x0);
	halrf_wreg(rf, 0xc1d4, 0x30, 0x0);
	/*1.ADC & clk enable */
	halrf_wreg(rf, 0x12b8, BIT(30), 0x1);
	halrf_wreg(rf, 0x032c, BIT(30), 0x0);
	/*4.Reset calibration*/
	halrf_wreg(rf, 0x032c, BIT(22), 0x0);	
	halrf_wreg(rf, 0x032c, BIT(22), 0x1);

	halrf_wreg(rf, 0x030c, 0x0f000000, 0xf);
	/*2.ADC input not from RXBB & ADC input short*/
	halrf_wreg(rf, 0x032c, BIT(16), 0x0);	
	halrf_wreg(rf, 0xc0d4, BIT(1), 0x1);
	/*3.release ADC reset*/
	halrf_wreg(rf, 0x030c, 0x0f000000, 0x3);
	RF_DBG(rf, DBG_RF_DACK, "[DACK]before S0 ADDCK\n");
	halrf_check_addc_8852b(rf, RF_PATH_A);
	/*average 128 times*/
//	halrf_wreg(rf, 0xc0f4, BIT(7) | BIT(6), 0x3);
	/*5.trigger dc offset calibration*/
	halrf_wreg(rf, 0xc0f4, BIT(11), 0x1);
	halrf_wreg(rf, 0xc0f4, BIT(11), 0x0);
	halrf_delay_us(rf, 1);
	/*check if cal done*/
	halrf_wreg(rf, 0xc0f4, 0x300, 0x1);
	c = 0;
	while (halrf_rreg(rf, 0xc0fc, BIT(0)) == 0) {
		c++;
		halrf_delay_us(rf, 1);
		if (c > 10000) {
			RF_DBG(rf, DBG_RF_DACK, "[DACK]S0 ADDCK timeout\n");
			dack->addck_timeout[0] = true;
			break;
		}
	}
	RF_DBG(rf, DBG_RF_DACK, "[DACK]ADDCK c = %d\n", c);
	RF_DBG(rf, DBG_RF_DACK, "[DACK]after S0 ADDCK\n");
	halrf_check_addc_8852b(rf, RF_PATH_A);
	/*restore*/
	halrf_wreg(rf, 0xc0d4, BIT(1), 0x0);
	halrf_wreg(rf, 0x032c, BIT(16), 0x1);
	halrf_wreg(rf, 0x030c, 0x0f000000, 0xc);
	halrf_wreg(rf, 0x032c, BIT(30), 0x1);
	halrf_wreg(rf, 0x12b8, BIT(30), 0x0);
#endif

	/*S1*/
#if 1
	/*1.ADC & clk enable */
	halrf_wreg(rf, 0x32b8, BIT(30), 0x1);
	halrf_wreg(rf, 0x032c, BIT(30), 0x0);
	/*4.Reset calibration*/
	halrf_wreg(rf, 0x032c, BIT(22), 0x0);	
	halrf_wreg(rf, 0x032c, BIT(22), 0x1);

	halrf_wreg(rf, 0x030c, 0x0f000000, 0xf);
	/*2.ADC input not from RXBB & ADC input short*/
	halrf_wreg(rf, 0x032c, BIT(16), 0x0);	
	halrf_wreg(rf, 0xc1d4, BIT(1), 0x1);
	/*3.release ADC reset*/
	halrf_wreg(rf, 0x030c, 0x0f000000, 0x3);
	RF_DBG(rf, DBG_RF_DACK, "[DACK]before S1 ADDCK\n");
	halrf_check_addc_8852b(rf, RF_PATH_B);
	/*average 128 times*/
//	halrf_wreg(rf, 0xc1f4, BIT(7) | BIT(6), 0x3);	
	/*5.trigger dc offset calibration*/
	halrf_wreg(rf, 0xc1f4, BIT(11), 0x1);
	halrf_wreg(rf, 0xc1f4, BIT(11), 0x0);
	halrf_delay_us(rf, 1);
	/*check if cal done*/
	halrf_wreg(rf, 0xc1f4, 0x300, 0x1);
	c = 0;
	while (halrf_rreg(rf, 0xc1fc, BIT(0)) == 0) {
		c++;
		halrf_delay_us(rf, 1);
		if (c > 10000) {
			RF_DBG(rf, DBG_RF_DACK, "[DACK]S1 ADDCK timeout\n");
			dack->addck_timeout[1] = true;
			break;
		}
	}
	RF_DBG(rf, DBG_RF_DACK, "[DACK]ADDCK c = %d\n", c);
	RF_DBG(rf, DBG_RF_DACK, "[DACK]after S1 ADDCK\n");
	halrf_check_addc_8852b(rf, RF_PATH_B);
	/*restore*/
	halrf_wreg(rf, 0xc1d4, BIT(1), 0x0);
	halrf_wreg(rf, 0x032c, BIT(16), 0x1);
	halrf_wreg(rf, 0x030c, 0x0f000000, 0xc);
	halrf_wreg(rf, 0x032c, BIT(30), 0x1);
	halrf_wreg(rf, 0x32b8, BIT(30), 0x0);
#endif
}

void halrf_check_dadc_8852b(struct rf_info *rf, enum rf_path path)
{
	halrf_wreg(rf, 0x032c, BIT(30), 0x0);
	halrf_wreg(rf, 0x030c, 0x0f000000, 0xf);
	halrf_wreg(rf, 0x030c, 0x0f000000, 0x3);
	halrf_wreg(rf, 0x032c, BIT(16), 0x0);
	if (path == RF_PATH_A) {
		halrf_wreg(rf, 0x12dc, BIT(0), 0x1);
		halrf_wreg(rf, 0x12e8, BIT(2), 0x1);
		halrf_wrf(rf, RF_PATH_A, 0x8f, BIT(13), 0x1);
	} else {
		halrf_wreg(rf, 0x32dc, BIT(0), 0x1);
		halrf_wreg(rf, 0x32e8, BIT(2), 0x1);
		halrf_wrf(rf, RF_PATH_B, 0x8f, BIT(13), 0x1);
	}
	halrf_check_addc_8852b(rf, path);
	if (path == RF_PATH_A) {
		halrf_wreg(rf, 0x12dc, BIT(0), 0x0);
		halrf_wreg(rf, 0x12e8, BIT(2), 0x0);
		halrf_wrf(rf, RF_PATH_A, 0x8f, BIT(13), 0x0);
	} else {
		halrf_wreg(rf, 0x32dc, BIT(0), 0x0);
		halrf_wreg(rf, 0x32e8, BIT(2), 0x0);
		halrf_wrf(rf, RF_PATH_B, 0x8f, BIT(13), 0x0);
	}
	halrf_wreg(rf, 0x032c, BIT(16), 0x1);
}

void halrf_dack_8852b_s0(struct rf_info *rf)
{
	struct halrf_dack_info *dack = &rf->dack;
	u32 c = 0;

	/*step 1*/
	halrf_wreg(rf, 0x12a0, BIT(15), 0x1);
	halrf_wreg(rf, 0x12a0, 0x7000, 0x3);
	/*step 2*/
	halrf_wreg(rf, 0x12b8, BIT(30), 0x1);
	halrf_wreg(rf, 0x030c, BIT(28), 0x1);
	halrf_wreg(rf, 0x032c, 0x80000000, 0x0);
	/*step 3*/
	halrf_wreg(rf, 0xc0d8, BIT(16), 0x1);
	/*step 4*/
	halrf_wreg(rf, 0xc0dc, BIT(27) | BIT(26), 0x3);
	/*step 5*/
	halrf_wreg(rf, 0xc004, BIT(30), 0x0);
	halrf_wreg(rf, 0xc024, BIT(30), 0x0);
	/*step 6*/
	halrf_wreg(rf, 0xc004, 0x3ff00000, 0x30);
//	halrf_wreg(rf, 0xc024, 0x3ff00000, 0x30);
	/*step 7*/
	halrf_wreg(rf, 0xc004, BIT(31) | BIT(30), 0x0);
//	halrf_wreg(rf, 0xc024, BIT(31) | BIT(30), 0x0);
	/*step 8*/
	halrf_wreg(rf, 0xc004, BIT(17), 0x1);
	halrf_wreg(rf, 0xc024, BIT(17), 0x1);
	halrf_wreg(rf, 0xc00c, BIT(2), 0x0);
	halrf_wreg(rf, 0xc02c, BIT(2), 0x0);
	/*step 9*/ /*auto mode*/
	halrf_wreg(rf, 0xc004, BIT(0), 0x1);
	halrf_wreg(rf, 0xc024, BIT(0), 0x1);
	halrf_delay_us(rf, 1);
	/*step 10*/
	c = 0x0;
	while ((halrf_rreg(rf, 0xc040, BIT(31)) == 0) || (halrf_rreg(rf, 0xc064, BIT(31)) == 0)) {
		c++;
		halrf_delay_us(rf, 1);
		if (c > 10000) {
			RF_DBG(rf, DBG_RF_DACK, "[DACK]S0 MSBK timeout\n");
			dack->msbk_timeout[0] = true;
			break;
		}
	}
	RF_DBG(rf, DBG_RF_DACK, "[DACK]DACK c = %d\n", c);
	/*step 11*/
	halrf_wreg(rf, 0xc0dc, BIT(27) | BIT(26), 0x0);
	/*step 12*/
	halrf_wreg(rf, 0xc00c, BIT(2), 0x1);
	halrf_wreg(rf, 0xc02c, BIT(2), 0x1);
	/*step 13*/
	c = 0x0;
	while ((halrf_rreg(rf, 0xc05c, BIT(2)) == 0) || (halrf_rreg(rf, 0xc080, BIT(2)) == 0)) {
		c++;
		halrf_delay_us(rf, 1);
		if (c > 10000) {
			RF_DBG(rf, DBG_RF_DACK, "[DACK]S0 DADCK timeout\n");
			dack->dadck_timeout[0] = true;
			break;
		}
	}
	/*step 14*/ /*auto mode off*/
	halrf_wreg(rf, 0xc004, BIT(0), 0x0);
	halrf_wreg(rf, 0xc024, BIT(0), 0x0);
	RF_DBG(rf, DBG_RF_DACK, "[DACK]DACK c = %d\n", c);
	/*step 15*/
	halrf_wreg(rf, 0xc0d8, BIT(16), 0x0);
	/*step 16*/
	halrf_wreg(rf, 0x12a0, BIT(15), 0x0);
	halrf_wreg(rf, 0x12a0, 0x7000, 0x7);

	RF_DBG(rf, DBG_RF_DACK, "[DACK]after S0 DADCK\n");
//	halrf_check_dadc_8852b(rf, RF_PATH_A);

	/*backup here*/
	halrf_dack_backup_s0_8852b(rf);
//	halrf_dack_reload_8852b(rf, RF_PATH_A);
	/*step 17*/
	halrf_wreg(rf, 0x12b8, BIT(30), 0x0);
}

void halrf_dack_8852b_s1(struct rf_info *rf)
{
	struct halrf_dack_info *dack = &rf->dack;
	u32 c = 0;

	/*step 1*/
	halrf_wreg(rf, 0x32a0, BIT(15), 0x1);
	halrf_wreg(rf, 0x32a0, 0x7000, 0x3);
	/*step 2*/
	halrf_wreg(rf, 0x32b8, BIT(30), 0x1);
	halrf_wreg(rf, 0x030c, BIT(28), 0x1);
	halrf_wreg(rf, 0x032c, 0x80000000, 0x0);
	/*step 3*/
	halrf_wreg(rf, 0xc1d8, BIT(16), 0x1);
	/*step 4*/
	halrf_wreg(rf, 0xc1dc, BIT(27) | BIT(26), 0x3);
	/*step 5*/
	halrf_wreg(rf, 0xc104, BIT(30), 0x0);
	halrf_wreg(rf, 0xc124, BIT(30), 0x0);
	/*step 6*/
	halrf_wreg(rf, 0xc104, 0x3ff00000, 0x30);
//	halrf_wreg(rf, 0xc024, 0x3ff00000, 0x30);
	/*step 7*/
	halrf_wreg(rf, 0xc104, BIT(31) | BIT(30), 0x0);
//	halrf_wreg(rf, 0xc124, BIT(31) | BIT(30), 0x0);
	/*step 8*/
	halrf_wreg(rf, 0xc104, BIT(17), 0x1);
	halrf_wreg(rf, 0xc124, BIT(17), 0x1);
	halrf_wreg(rf, 0xc10c, BIT(2), 0x0);
	halrf_wreg(rf, 0xc12c, BIT(2), 0x0);
	/*step 9*/ /*auto mode*/
	halrf_wreg(rf, 0xc104, BIT(0), 0x1);
	halrf_wreg(rf, 0xc124, BIT(0), 0x1);
	halrf_delay_us(rf, 1);
	/*step 10*/
	c = 0x0;
	while((halrf_rreg(rf, 0xc140, BIT(31)) == 0) && (halrf_rreg(rf, 0xc164, BIT(31)) == 0)) {
		c++;
		halrf_delay_us(rf, 1);
		if (c > 10000) {
			RF_DBG(rf, DBG_RF_DACK, "[DACK]S1 MSBK timeout\n");
			dack->msbk_timeout[1] = true;
			break;
		}
	}
	RF_DBG(rf, DBG_RF_DACK, "[DACK]DACK c = %d\n", c);
	/*step 11*/
	halrf_wreg(rf, 0xc1dc, BIT(27) | BIT(26), 0x0);
	/*step 12*/
	halrf_wreg(rf, 0xc10c, BIT(2), 0x1);
	halrf_wreg(rf, 0xc12c, BIT(2), 0x1);
	halrf_delay_us(rf, 1);
	/*step 13*/
	c = 0x0;
	while(halrf_rreg(rf, 0xc15c, BIT(2)) == 0 && halrf_rreg(rf, 0xc180, BIT(2)) == 0) {
		c++;
		halrf_delay_us(rf, 1);
		if (c > 10000) {
			RF_DBG(rf, DBG_RF_DACK, "[DACK]S1 DADCK timeout\n");
			dack->dadck_timeout[1] = true;
			break;
		}
	}
	/*step 14*/ /*auto mode off*/
	halrf_wreg(rf, 0xc104, BIT(0), 0x0);
	halrf_wreg(rf, 0xc124, BIT(0), 0x0);
	RF_DBG(rf, DBG_RF_DACK, "[DACK]DACK c = %d\n", c);
	/*step 15*/
	halrf_wreg(rf, 0xc1d8, BIT(16), 0x0);
	/*step 16*/
	halrf_wreg(rf, 0x32a0, BIT(15), 0x0);
	halrf_wreg(rf, 0x32a0, 0x7000, 0x7);

	RF_DBG(rf, DBG_RF_DACK, "[DACK]after S1 DADCK\n");
	halrf_check_dadc_8852b(rf, RF_PATH_B);
	/*backup here*/
	halrf_dack_backup_s1_8852b(rf);
//	halrf_dack_reload_8852b(rf, RF_PATH_B);
	/*step 17*/
	halrf_wreg(rf, 0x32b8, BIT(30), 0x0);
}

void halrf_dack_8852b(struct rf_info *rf)
{
	halrf_dack_8852b_s0(rf);
	halrf_dack_8852b_s1(rf);
}

void halrf_dack_dump_8852b(struct rf_info *rf)
{
	struct halrf_dack_info *dack = &rf->dack;
	u8 i;
	u8 t;

	RF_DBG(rf, DBG_RF_DACK, "[DACK]S0 ADC_DCK ic = 0x%x, qc = 0x%x\n",
	                    dack->addck_d[0][0], dack->addck_d[0][1] );
	RF_DBG(rf, DBG_RF_DACK, "[DACK]S1 ADC_DCK ic = 0x%x, qc = 0x%x\n",
	                    dack->addck_d[1][0], dack->addck_d[1][1] );
	RF_DBG(rf, DBG_RF_DACK, "[DACK]S0 DAC_DCK ic = 0x%x, qc = 0x%x\n",
	       dack->dadck_d[0][0], dack->dadck_d[0][1] );
	RF_DBG(rf, DBG_RF_DACK, "[DACK]S1 DAC_DCK ic = 0x%x, qc = 0x%x\n",
	       dack->dadck_d[1][0], dack->dadck_d[1][1] );

	RF_DBG(rf, DBG_RF_DACK, "[DACK]S0 biask ic = 0x%x, qc = 0x%x\n",
	       dack->biask_d[0][0], dack->biask_d[0][1] );
	RF_DBG(rf, DBG_RF_DACK, "[DACK]S1 biask ic = 0x%x, qc = 0x%x\n",
	       dack->biask_d[1][0], dack->biask_d[1][1] );

	RF_DBG(rf, DBG_RF_DACK, "[DACK]S0 MSBK ic:\n");
	for (i = 0; i < 0x10; i++) {
		t = dack->msbk_d[0][0][i];
		RF_DBG(rf, DBG_RF_DACK, "[DACK]0x%x\n", t);
	}
	RF_DBG(rf, DBG_RF_DACK, "[DACK]S0 MSBK qc:\n");
	for (i = 0; i < 0x10; i++) {
		t = dack->msbk_d[0][1][i];
		RF_DBG(rf, DBG_RF_DACK, "[DACK]0x%x\n", t);
	}
	RF_DBG(rf, DBG_RF_DACK, "[DACK]S1 MSBK ic:\n");
	for (i = 0; i < 0x10; i++) {
		t = dack->msbk_d[1][0][i];
		RF_DBG(rf, DBG_RF_DACK, "[DACK]0x%x\n", t);
	}
	RF_DBG(rf, DBG_RF_DACK, "[DACK]S1 MSBK qc:\n");
	for (i = 0; i < 0x10; i++) {
		t = dack->msbk_d[1][1][i];
		RF_DBG(rf, DBG_RF_DACK, "[DACK]0x%x\n", t);
	}
}

void halrf_dac_cal_8852b(struct rf_info *rf, bool force)
{
	struct halrf_dack_info *dack = &rf->dack;
	u32 rf0_0, rf1_0;
	u8 phy_map;

	phy_map = (BIT(HW_PHY_0) << 4) | RF_AB;
#if 0
	if (dack->dack_en) {
		if (!force) {
			halrf_dack_reload_8852a(rf);
			RF_DBG(rf, DBG_RF_DACK, "[DACK]reload dack value\n");
			return;
		}
	} else {
		dack->dack_en = true;
	}
#endif
	dack->dack_done = false;
	RF_DBG(rf, DBG_RF_DACK, "[DACK]DACK 0x1\n");
	RF_DBG(rf, DBG_RF_DACK, "[DACK]DACK start!!!\n");	
	rf0_0 = halrf_rrf(rf,RF_PATH_A, 0x0, MASKRF);
	rf1_0 = halrf_rrf(rf,RF_PATH_B, 0x0, MASKRF);
#if 1	
	halrf_afe_init_8852b(rf);
	halrf_drck_8852b(rf);
	halrf_wrf(rf, RF_PATH_A, 0x5, BIT(0), 0x0);
	halrf_wrf(rf, RF_PATH_B, 0x5, BIT(0), 0x0);
	halrf_wrf(rf, RF_PATH_A, 0x0, MASKRF, 0x337e1);
	halrf_wrf(rf, RF_PATH_B, 0x0, MASKRF, 0x337e1);
//	halrf_btc_rfk_ntfy(rf, phy_map, RF_BTC_DACK, RFK_ONESHOT_START);
	halrf_addck_8852b(rf);
//	halrf_btc_rfk_ntfy(rf, phy_map, RF_BTC_DACK, RFK_ONESHOT_STOP);
	halrf_addck_backup_8852b(rf);
	halrf_addck_reload_8852b(rf);
//	halrf_wrf(rf, RF_PATH_A, 0x0, MASKRF, 0x40001);
//	halrf_wrf(rf, RF_PATH_B, 0x0, MASKRF, 0x40001);
	halrf_wrf(rf, RF_PATH_A, 0x1, MASKRF, 0x0);
	halrf_wrf(rf, RF_PATH_B, 0x1, MASKRF, 0x0);
//	halrf_btc_rfk_ntfy(rf, phy_map, RF_BTC_DACK, RFK_ONESHOT_START);
	halrf_dack_8852b(rf);
//	halrf_btc_rfk_ntfy(rf, phy_map, RF_BTC_DACK, RFK_ONESHOT_STOP);
	halrf_dack_dump_8852b(rf);
	dack->dack_done = true;
	halrf_wrf(rf, RF_PATH_A, 0x0, MASKRF, rf0_0);
	halrf_wrf(rf, RF_PATH_B, 0x0, MASKRF, rf1_0);
	halrf_wrf(rf, RF_PATH_A, 0x5, BIT(0), 0x1);
	halrf_wrf(rf, RF_PATH_B, 0x5, BIT(0), 0x1);
#endif
	dack->dack_cnt++;
	RF_DBG(rf, DBG_RF_DACK, "[DACK]DACK finish!!!\n");
}
#endif

