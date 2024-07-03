/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 */

#ifndef __ROCKCHIP_HPTIMER_H__
#define __ROCKCHIP_HPTIMER_H__

enum rk_hptimer_mode_t {
	RK_HPTIMER_NORM_MODE = 0,
	RK_HPTIMER_HARD_ADJUST_MODE = 1,
	RK_HPTIMER_SOFT_ADJUST_MODE = 2,
};

/* hptimer int */
enum rk_hptimer_v2_int_id_t {
	RK_HPTIMER_V2_INT_REACH = 0,
	RK_HPTIMER_V2_INT_SYNC = 2,
	RK_HPTIMER_V2_INT_32K_REACH = 3,
	RK_HPTIMER_V2_INT_EXTRA_REACH = 4,
};

int rk_hptimer_is_enabled(void __iomem *base);
int rk_hptimer_get_mode(void __iomem *base);
u64 rk_hptimer_get_count(void __iomem *base);

int rk_hptimer_wait_mode(void __iomem *base, enum rk_hptimer_mode_t mode);
void rk_hptimer_do_soft_adjust(void __iomem *base, u32 hf, u32 lf);
void rk_hptimer_do_soft_adjust_no_wait(void __iomem *base, u32 hf, u32 lf);
void rk_hptimer_mode_init(void __iomem *base, enum rk_hptimer_mode_t mode);

void rk_hptimer_v2_clear_int_st(void __iomem *base, enum rk_hptimer_v2_int_id_t id);
void rk_hptimer_v2_enable_int(void __iomem *base, enum rk_hptimer_v2_int_id_t id);
void rk_hptimer_v2_disable_int(void __iomem *base, enum rk_hptimer_v2_int_id_t id);
int rk_hptimer_v2_wait_sync(void __iomem *base);
void rk_hptimer_v2_do_soft_adjust(void __iomem *base, u32 hf, u32 lf);
void rk_hptimer_v2_do_soft_adjust_no_wait(void __iomem *base, u32 hf, u32 lf);
void rk_hptimer_v2_do_hard_adjust(void __iomem *base);
void rk_hptimer_v2_do_hard_adjust_no_wait(void __iomem *base);
void rk_hptimer_v2_config_one_shot_timeout_int(void __iomem *base, u64 delta_cnt);
void rk_hptimer_v2_config_free_timeout_int(void __iomem *base, u32 delta_cnt);
void rk_hptimer_v2_config_sleep_timeout_int(void __iomem *base, u64 delta_cnt);
void rk_hptimer_v2_mode_init(void __iomem *base, enum rk_hptimer_mode_t mode, u32 hf);

#endif
