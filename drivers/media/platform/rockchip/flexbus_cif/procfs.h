/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Rockchip Flexbus CIF Driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */
#ifndef _FLEXBUS_CIF_PROCFS_H
#define _FLEXBUS_CIF_PROCFS_H

#ifdef CONFIG_PROC_FS

int flexbus_cif_proc_init(struct flexbus_cif_device *dev);
void flexbus_cif_proc_cleanup(struct flexbus_cif_device *dev);

#else

static inline int flexbus_cif_proc_init(struct rkisp_device *dev)
{
	return 0;
}
static inline void flexbus_cif_proc_cleanup(struct rkisp_device *dev)
{

}

#endif

#endif
