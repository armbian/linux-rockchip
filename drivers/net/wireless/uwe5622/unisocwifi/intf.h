/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Authors:
 * Keguang Zhang <keguang.zhang@spreadtrum.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SPRDWL_INTF_H__
#define __SPRDWL_INTF_H__

#include "msg.h"

struct sprdwl_priv;

struct sprdwl_if_ops {
	struct sprdwl_msg_buf *(*get_msg_buf)(void *sdev,
					      enum sprdwl_head_type type,
#if defined(UWE5621_FTR)
					      enum sprdwl_mode mode,
					      u8 ctx_id);
#else
					      enum sprdwl_mode mode);
#endif
	void (*free_msg_buf)(void *sdev, struct sprdwl_msg_buf *msg);
	int (*tx)(void *spdev, struct sprdwl_msg_buf *msg);

#if defined(UWE5621_FTR)
	void (*force_exit)(void *spdev);
	int (*is_exit)(void *spdev);
#else
	void (*force_exit)(void);
	int (*is_exit)(void);
#endif /* UWE5621_FTR */
	int (*suspend)(struct sprdwl_priv *priv);
	int (*resume)(struct sprdwl_priv *priv);
#if defined(UWE5621_FTR)
	void (*debugfs)(void *spdev, struct dentry *dir);
#else
	void (*debugfs)(struct dentry *dir);
#endif /* UWE5621_FTR */

#if defined(UWE5621_FTR)
	void (*tcp_drop_msg)(void *spdev, struct sprdwl_msg_buf *msg);
#else
	void (*tcp_drop_msg)(struct sprdwl_msg_buf *msg);
#endif /* UWE5621_FTR*/
	int (*ini_download_status)(void);
};

#endif/*__INTF_H__*/
