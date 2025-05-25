/* SPDX-License-Identifier: GPL-2.0 */

/* Copyright (c) 2024 Rockchip Electronics Co., Ltd. */

#ifndef __RKCE_ERROR_H__
#define __RKCE_ERROR_H__

#include <linux/errno.h>

#define RKCE_SUCCESS		0
#define RKCE_NOMEM		ENOMEM
#define RKCE_FAULT		EFAULT
#define RKCE_INVAL		EINVAL
#define RKCE_TIMEOUT		ETIMEDOUT

#endif
