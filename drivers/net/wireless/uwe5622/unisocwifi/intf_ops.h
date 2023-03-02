/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * Authors	:
 * Keguang Zhang <keguang.zhang@spreadtrum.com>
 * Jingxiang Li <Jingxiang.li@spreadtrum.com>
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

#ifndef __SPRDWL_INTF_OPS_H__
#define __SPRDWL_INTF_OPS_H__

#include <linux/dcache.h>
#include "intf.h"

static
inline struct sprdwl_msg_buf *sprdwl_intf_get_msg_buf(struct sprdwl_priv *priv,
						      enum sprdwl_head_type
						      type,
						      enum sprdwl_mode mode,
						      u8 ctx_id)
{
#if defined(UWE5621_FTR)
	return priv->if_ops->get_msg_buf(priv->hw_priv, type, mode, ctx_id);
#else
	return priv->if_ops->get_msg_buf(priv->hw_priv, type, mode);
#endif
}

static inline void sprdwl_intf_free_msg_buf(struct sprdwl_priv *priv,
					    struct sprdwl_msg_buf *msg)
{
	return priv->if_ops->free_msg_buf(priv->hw_priv, msg);
}

/* return:
 *      0, msg buf freed by the real driver
 *      others, skb need free by the caller,remember not use msg->skb!
 */
static inline int sprdwl_intf_tx(struct sprdwl_priv *priv,
				 struct sprdwl_msg_buf *msg)
{
	return priv->if_ops->tx(priv->hw_priv, msg);
}

static inline void sprdwl_intf_force_exit(struct sprdwl_priv *priv)
{
#if defined(UWE5621_FTR)
	priv->if_ops->force_exit(priv->hw_priv);
#else
	priv->if_ops->force_exit();
#endif
}

static inline int sprdwl_intf_is_exit(struct sprdwl_priv *priv)
{
#if defined(UWE5621_FTR)
	return priv->if_ops->is_exit(priv->hw_priv);
#else
	return priv->if_ops->is_exit();
#endif
}

static inline int sprdwl_intf_suspend(struct sprdwl_priv *priv)
{
	if (priv->if_ops->suspend)
		return priv->if_ops->suspend(priv);

	return 0;
}

static inline int sprdwl_intf_resume(struct sprdwl_priv *priv)
{
	if (priv->if_ops->resume)
		return priv->if_ops->resume(priv);

	return 0;
}

static inline void sprdwl_intf_debugfs(struct sprdwl_priv *priv,
				       struct dentry *dir)
{
#if defined(UWE5621_FTR)
	if (priv->if_ops->debugfs)
		priv->if_ops->debugfs(priv->hw_priv, dir);
#else
	if (priv->if_ops->debugfs)
		priv->if_ops->debugfs(dir);
#endif /* UWE5621_FTR */
}

static inline void sprdwl_intf_tcp_drop_msg(struct sprdwl_priv *priv,
					    struct sprdwl_msg_buf *msg)
{
#if defined(UWE5621_FTR)
	if (priv->if_ops->tcp_drop_msg)
		priv->if_ops->tcp_drop_msg(priv->hw_priv, msg);
#else
	if (priv->if_ops->tcp_drop_msg)
		priv->if_ops->tcp_drop_msg(msg);
#endif
}

static inline int sprdwl_get_ini_status(struct sprdwl_priv *priv)
{
	if (priv->if_ops->ini_download_status)
		return priv->if_ops->ini_download_status();
	return 0;
}

#endif
