/******************************************************************************
 *
 * Copyright(c) 2007 - 2019 Realtek Corporation.
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
#ifndef __SDIO_OPS_LINUX_H__
#define __SDIO_OPS_LINUX_H__

#define SDIO_ERR_VAL8	0xFF
#define SDIO_ERR_VAL16	0xFFFF
#define SDIO_ERR_VAL32	0xFFFFFFFF


bool rtw_is_sdio30(struct dvobj_priv *d);
u32 rtw_sdio_get_clock(struct dvobj_priv *d);


s32 _sd_read(struct dvobj_priv *d, u32 addr, u32 cnt, void *pdata);
s32 sd_read(struct dvobj_priv *d, u32 addr, u32 cnt, void *pdata);
s32 _sd_write(struct dvobj_priv *d, u32 addr, u32 cnt, void *pdata);
s32 sd_write(struct dvobj_priv *d, u32 addr, u32 cnt, void *pdata);

void rtw_sdio_set_irq_thd(struct dvobj_priv *d, _thread_hdl_ thd_hdl);
int __must_check rtw_sdio_raw_read(struct dvobj_priv *d, unsigned int addr,
				void *buf, size_t len, bool fixed);
int __must_check rtw_sdio_raw_write(struct dvobj_priv *d, unsigned int addr,
				void *buf, size_t len, bool fixed);

#endif /* __SDIO_OPS_LINUX_H__ */

