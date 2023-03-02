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
#ifndef _HALRF_PSD_H_
#define _HALRF_PSD_H_

/*@--------------------------Define Parameters-------------------------------*/


/*@-----------------------End Define Parameters-----------------------*/
struct halrf_psd_data {
	u8 path;
	u8 iq_path;
	u32 avg;
	u32 fft;
	u32 point;
	u32 start_point;
	u32 stop_point;
	u32 average;
	u32 buf_size;
	u32 psd_data[450];
	u32 psd_progress;
	bool psd_result_running;
	u32 psd_reg_backup[256];
};


#endif	/*_HALRF_PSD_H_*/
