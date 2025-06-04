/* SPDX-License-Identifier: GPL-2.0 */
/*  ----------------------------------------------------------------------------
 *  File:   rk_ext.h
 *
 *  Desc:   rk_ext_on_mali_ko 中的 通行定义等.
 *
 *  Usage:
 *
 *  Note:
 *
 *  Author: ChenZhen
 *
 *  Log:
 *
 *  ----------------------------------------------------------------------------
 */

#ifndef __RK_EXT_H__
#define __RK_EXT_H__

#include <linux/platform_device.h>
#include "../../common/mali_osk_mali.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/

/** version of rk_ext on mali_ko, aka. rk_ko_ver. */
#define RK_KO_VER   (5)

/*---------------------------------------------------------------------------*/

int mali_platform_device_init(struct platform_device *pdev);

int rk_platform_init_opp_table(struct mali_device *mdev);
void rk_platform_uninit_opp_table(struct mali_device *mdev);

#ifdef __cplusplus
}
#endif

#endif /* __RK_EXT_H__ */

