// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/io.h>
#include <linux/math64.h>

#include "rkpm_helpers.h"
#include "rockchip_hptimer.h"

/* hp timer regs */
#define TIMER_HP_REVISION		0x0
#define TIMER_HP_CTRL			0x4
#define TIMER_HP_INT_EN			0x8
#define TIMER_HP_T24_GCD		0xc
#define TIMER_HP_T32_GCD		0x10
#define TIMER_HP_LOAD_COUNT0		0x14
#define TIMER_HP_LOAD_COUNT1		0x18
#define TIMER_HP_T24_DELAT_COUNT0	0x1c
#define TIMER_HP_T24_DELAT_COUNT1	0x20
#define TIMER_HP_CURR_32K_VALUE0	0x24
#define TIMER_HP_CURR_32K_VALUE1	0x28
#define TIMER_HP_CURR_TIMER_VALUE0	0x2c
#define TIMER_HP_CURR_TIMER_VALUE1	0x30
#define TIMER_HP_T24_32BEGIN0		0x34
#define TIMER_HP_T24_32BEGIN1		0x38
#define TIMER_HP_T32_24END0		0x3c
#define TIMER_HP_T32_24END1		0x40
#define TIMER_HP_BEGIN_END_VALID	0x44
#define TIMER_HP_SYNC_REQ		0x48
#define TIMER_HP_INTR_STATUS		0x4c
#define TIMER_HP_CURR_ATTK_32K_VALUE0	0x54
#define TIMER_HP_CURR_ATTK_32K_VALUE1	0x58
#define TIMER_HP_LOAD_32K_COUNT0	0x5c
#define TIMER_HP_LOAD_32K_COUNT1	0x60
#define TIMER_HP_COMP_H_VALUE0		0x64
#define TIMER_HP_COMP_H_VALUE1		0x68
#define TIMER_HP_COMP_L_VALUE0		0x6c
#define TIMER_HP_COMP_L_VALUE1		0x70
#define TIMER_HP_COMP_H_32K_VALUE0	0x74
#define TIMER_HP_COMP_H_32K_VALUE1	0x78
#define TIMER_HP_COMP_L_32K_VALUE0	0x7c
#define TIMER_HP_COMP_L_32K_VALUE1	0x80

/* hptimer ctlr */
enum rk_hptimer_ctlr_reg {
	RK_HPTIMER_CTRL_EN = 0,
	RK_HPTIMER_CTRL_MODE = 1,
	RK_HPTIMER_CTRL_CNT_MODE = 3,
	RK_HPTIMER_ATTK_CNT_CTRL = 4,
	RK_HPTIMER_EXTRA_CNT_CTRL = 5,
	RK_HPTIMER_INIT_MODE = 6,
};

/* hptimer record valid */
enum rk_hptimer_valid_t {
	RK_HPTIMER_VALID_T24_32_BEGIN = 0,
	RK_HPTIMER_VALID_T32_24_END = 1,
	RK_HPTIMER_VALID_COMP = 2,
	RK_HPTIMER_VALID_COMP_32K = 3,
};

/* hptimer req */
enum rk_hptimer_req_t {
	RK_HPTIMER_REQ_SW_SYNC = 0,
	RK_HPTIMER_REQ_HW_SYNC = 1,
	RK_HPTIMER_REQ_COMP_EN = 4,
	RK_HPTIMER_REQ_LP_COMP_EN = 5,
	RK_HPTIMER_REQ_HW_SYNC_COMP_EN = 6,
	RK_HPTIMER_REQ_HW_SYNC_EN = 8,
	RK_HPTIMER_REQ_HW_SYNC_DIS = 9,
};

#define HPTIMER_WAIT_MAX_US	1000000

static u32 get_gcd(u32 a, u32 b)
{
	u32 t;

	while (b != 0) {
		t = a % b;
		a = b;
		b = t;
	}

	return a;
}

static int rk_hptimer_wait_int_st(void __iomem *base,
				  enum rk_hptimer_v2_int_id_t id,
				  u64 wait_us)
{
	if (!wait_us)
		return -1;

	while (!(readl_relaxed(base + TIMER_HP_INTR_STATUS) & BIT(id)) &&
	       --wait_us > 0)
		rkpm_raw_udelay(1);
	dsb();

	if ((readl_relaxed(base + TIMER_HP_INTR_STATUS) & BIT(id)) == 0) {
		rkpm_printstr("can't wait hptimer int:");
		rkpm_printdec(id);
		rkpm_printch('-');
		rkpm_printhex(readl_relaxed(base + TIMER_HP_INTR_STATUS));
		rkpm_printch('\n');
		return -1;
	} else {
		return 0;
	}
}

static int rk_hptimer_wait_record_valid(void __iomem *base, u64 wait_us, u32 msk)
{
	if (!wait_us)
		return -1;

	while ((readl_relaxed(base + TIMER_HP_BEGIN_END_VALID) & msk) != msk &&
	       --wait_us > 0)
		rkpm_raw_udelay(1);
	dsb();

	if ((readl_relaxed(base + TIMER_HP_BEGIN_END_VALID) & msk) != msk) {
		rkpm_printstr("can't wait hptimer begin_end valid:");
		rkpm_printhex(readl_relaxed(base + TIMER_HP_BEGIN_END_VALID));
		rkpm_printch('\n');
		return -1;
	} else {
		return 0;
	}
}

static int rk_hptimer_wait_begin_end_valid(void __iomem *base, u64 wait_us)
{
	return rk_hptimer_wait_record_valid(base, wait_us,
					    BIT(RK_HPTIMER_VALID_T24_32_BEGIN) |
					    BIT(RK_HPTIMER_VALID_T32_24_END));
}

static u64 rk_hptimer_get_soft_adjust_delt_cnt(void __iomem *base, u32 hf, u32 lf)
{
	u64 begin, end, delta;
	u32 tmp;

	if (!lf)
		return 0;

	if (rk_hptimer_wait_begin_end_valid(base, HPTIMER_WAIT_MAX_US))
		return 0;

	begin = (u64)readl_relaxed(base + TIMER_HP_T24_32BEGIN0) |
		(u64)readl_relaxed(base + TIMER_HP_T24_32BEGIN1) << 32;
	end = (u64)readl_relaxed(base + TIMER_HP_T32_24END0) |
	      (u64)readl_relaxed(base + TIMER_HP_T32_24END1) << 32;

	delta = (end - begin + 2) * (hf - lf);
	delta = div_u64(delta, lf);
	tmp = (2 * hf + hf / 2) / lf;
	delta = delta + tmp + 2;

	writel_relaxed(0x3, base + TIMER_HP_BEGIN_END_VALID);

	return delta;
}

static void rk_hptimer_soft_adjust_req(void __iomem *base, u64 delta)
{
	if (delta == 0)
		return;

	writel_relaxed(delta & 0xffffffff,
		       base + TIMER_HP_T24_DELAT_COUNT0);
	writel_relaxed((delta >> 32) & 0xffffffff,
		       base + TIMER_HP_T24_DELAT_COUNT1);
	dsb();

	writel_relaxed(BITS_WITH_WMASK(1, 0x1, RK_HPTIMER_REQ_SW_SYNC),
		       base + TIMER_HP_SYNC_REQ);
	dsb();
}

static void rk_hptimer_hard_adjust_req(void __iomem *base)
{
	writel_relaxed(BITS_WITH_WMASK(1, 0x1, RK_HPTIMER_REQ_HW_SYNC),
		       base + TIMER_HP_SYNC_REQ);
	dsb();
}

int rk_hptimer_is_enabled(void __iomem *base)
{
	return !!(readl_relaxed(base + TIMER_HP_CTRL) & BIT(RK_HPTIMER_CTRL_EN));
}

int rk_hptimer_get_mode(void __iomem *base)
{
	return (readl_relaxed(base + TIMER_HP_CTRL) >> RK_HPTIMER_CTRL_MODE) & 0x3;
}

u64 rk_hptimer_get_count(void __iomem *base)
{
	u32 high, low, temp;

	do {
		high = readl_relaxed(base + TIMER_HP_CURR_TIMER_VALUE1);
		low = readl_relaxed(base + TIMER_HP_CURR_TIMER_VALUE0);
		temp = readl_relaxed(base + TIMER_HP_CURR_TIMER_VALUE1);
	} while (high != temp);

	return ((u64)high << 32) | low;
}

void rk_hptimer_v2_clear_int_st(void __iomem *base, enum rk_hptimer_v2_int_id_t id)
{
	writel_relaxed(BIT(id), base + TIMER_HP_INTR_STATUS);
}

void rk_hptimer_v2_enable_int(void __iomem *base, enum rk_hptimer_v2_int_id_t id)
{
	u32 int_en = readl_relaxed(base + TIMER_HP_INT_EN);

	writel_relaxed(int_en | BIT(id), base + TIMER_HP_INT_EN);
}

void rk_hptimer_v2_disable_int(void __iomem *base, enum rk_hptimer_v2_int_id_t id)
{
	u32 int_en = readl_relaxed(base + TIMER_HP_INT_EN);

	writel_relaxed(int_en & ~BIT(id), base + TIMER_HP_INT_EN);
}

int rk_hptimer_v2_wait_sync(void __iomem *base)
{
	if (rk_hptimer_wait_int_st(base, RK_HPTIMER_V2_INT_SYNC,
				   HPTIMER_WAIT_MAX_US))
		return -1;

	rk_hptimer_v2_clear_int_st(base, RK_HPTIMER_V2_INT_SYNC);

	return 0;
}

void rk_hptimer_v2_do_soft_adjust(void __iomem *base, u32 hf, u32 lf)
{
	u64 delta = rk_hptimer_get_soft_adjust_delt_cnt(base, hf, lf);

	rk_hptimer_soft_adjust_req(base, delta);

	rk_hptimer_v2_wait_sync(base);
}

void rk_hptimer_v2_do_soft_adjust_no_wait(void __iomem *base, u32 hf, u32 lf)
{
	u64 delta = rk_hptimer_get_soft_adjust_delt_cnt(base, hf, lf);

	rk_hptimer_soft_adjust_req(base, delta);
}

void rk_hptimer_v2_do_hard_adjust(void __iomem *base)
{
	rk_hptimer_hard_adjust_req(base);

	rk_hptimer_v2_wait_sync(base);
}

void rk_hptimer_v2_do_hard_adjust_no_wait(void __iomem *base)
{
	rk_hptimer_hard_adjust_req(base);
}

void rk_hptimer_v2_config_one_shot_timeout_int(void __iomem *base, u64 delta_cnt)
{
	u64 cnt = rk_hptimer_get_count(base);

	cnt += delta_cnt;

	writel_relaxed(cnt & 0xffffffff, base + TIMER_HP_LOAD_COUNT0);
	writel_relaxed((cnt >> 32) & 0xffffffff, base + TIMER_HP_LOAD_COUNT1);

	rk_hptimer_v2_enable_int(base, RK_HPTIMER_V2_INT_REACH);
}

void rk_hptimer_v2_config_free_timeout_int(void __iomem *base, u32 delta_cnt)
{
	writel_relaxed(BITS_WITH_WMASK(0, 0x1, RK_HPTIMER_EXTRA_CNT_CTRL),
		       base + TIMER_HP_CTRL);

	/* In this mode, only loadcount0 is valid */
	writel_relaxed(delta_cnt, base + TIMER_HP_LOAD_COUNT0);
	writel_relaxed(0, base + TIMER_HP_LOAD_COUNT1);

	rk_hptimer_v2_enable_int(base, RK_HPTIMER_V2_INT_EXTRA_REACH);
	dsb();
	writel_relaxed(BITS_WITH_WMASK(1, 0x1, RK_HPTIMER_EXTRA_CNT_CTRL),
		       base + TIMER_HP_CTRL);
}

void rk_hptimer_v2_config_sleep_timeout_int(void __iomem *base, u64 delta_cnt)
{
	u64 cnt = rk_hptimer_get_count(base);

	cnt += delta_cnt;

	writel_relaxed(cnt & 0xffffffff, base + TIMER_HP_LOAD_32K_COUNT0);
	writel_relaxed((cnt >> 32) & 0xffffffff, base + TIMER_HP_LOAD_32K_COUNT1);

	rk_hptimer_v2_enable_int(base, RK_HPTIMER_V2_INT_32K_REACH);
}

void rk_hptimer_v2_mode_init(void __iomem *base, enum rk_hptimer_mode_t mode, u32 hf)
{
	u64 old_cnt = rk_hptimer_get_count(base);
	u32 gcd = 0;

	writel_relaxed(0xffff0000, base + TIMER_HP_CTRL);
	writel_relaxed(0x0, base + TIMER_HP_INT_EN);
	writel_relaxed(0xffffffff, base + TIMER_HP_INTR_STATUS);
	writel_relaxed(0x3, base + TIMER_HP_BEGIN_END_VALID);
	writel_relaxed(0xffffffff, base + TIMER_HP_LOAD_COUNT0);
	writel_relaxed(0xffffffff, base + TIMER_HP_LOAD_COUNT1);

	/* config T24/T32 GCD if hard_adjust_mode */
	if (mode == RK_HPTIMER_HARD_ADJUST_MODE) {
		gcd = get_gcd(hf, 32768);

		writel_relaxed(hf / gcd, base + TIMER_HP_T24_GCD);
		writel_relaxed(32768 / gcd, base + TIMER_HP_T32_GCD);
	}
	dsb();

	if (mode != RK_HPTIMER_NORM_MODE)
		writel_relaxed(BIT(RK_HPTIMER_V2_INT_SYNC), base + TIMER_HP_INT_EN);

	writel_relaxed(BITS_WITH_WMASK(mode, 0x3, RK_HPTIMER_CTRL_MODE) |
		       BITS_WITH_WMASK(1, 0x1, RK_HPTIMER_INIT_MODE),
		       base + TIMER_HP_CTRL);
	dsb();

	writel_relaxed(BITS_WITH_WMASK(1, 0x1, RK_HPTIMER_CTRL_EN),
		       base + TIMER_HP_CTRL);
	dsb();

	if (mode == RK_HPTIMER_HARD_ADJUST_MODE) {
		rk_hptimer_v2_do_hard_adjust(base);
	} else if (mode == RK_HPTIMER_SOFT_ADJUST_MODE) {
		/* compensate old_cnt to hptimer if soft_adjust_mode */
		rk_hptimer_soft_adjust_req(base, old_cnt);
	}
}
