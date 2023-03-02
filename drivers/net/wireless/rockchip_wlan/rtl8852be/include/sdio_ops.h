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
#ifndef __SDIO_OPS_H__
#define __SDIO_OPS_H__


#ifdef PLATFORM_LINUX
#include <sdio_ops_linux.h>
#endif

void dump_sdio_card_info(void *sel, struct dvobj_priv *dvobj);

u32 rtw_sdio_init(struct dvobj_priv *dvobj);
void rtw_sdio_deinit(struct dvobj_priv *dvobj);
int rtw_sdio_alloc_irq(struct dvobj_priv *dvobj);
void rtw_sdio_free_irq(struct dvobj_priv *dvobj);
u8 rtw_sdio_get_num_of_func(struct dvobj_priv *dvobj);


/**
 * rtw_sdio_get_block_size() - Get block size of SDIO transfer
 * @d		struct dvobj_priv*
 *
 * The unit of return value is byte.
 */
static inline u32 rtw_sdio_get_block_size(struct dvobj_priv *d)
{
	return dvobj_to_sdio(d)->block_transfer_len;
}
#endif /* !__SDIO_OPS_H__ */
