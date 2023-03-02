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
#ifndef _RTW_TRX_SDIO_H_
#define _RTW_TRX_SDIO_H_

#include <drv_types.h>		/* struct dvobj_priv and etc. */

s32 sdio_dequeue_xmit(_adapter *adapter);

extern struct rtw_intf_ops sdio_ops;

#endif /* _RTW_SDIO_H_ */
