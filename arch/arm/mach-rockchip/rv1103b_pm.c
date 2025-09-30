// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/suspend.h>
#include <linux/mfd/syscon.h>

#include <asm/cacheflush.h>
#include <asm/fiq_glue.h>
#include <asm/tlbflush.h>
#include <asm/suspend.h>
#include <dt-bindings/suspend/rockchip-rv1103b.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/rockchip/rockchip_pm_config.h>

#include "rkpm_gicv2.h"
#include "rkpm_helpers.h"
#include "rkpm_uart.h"
#include "rockchip_hptimer.h"
#include "rv1103b_pm.h"

#define RV1103B_PM_REG_REGION_MEM_SIZE		SZ_4K
#define SLEEP_RESERVE_MEM_SIZE			SZ_4K

#define BOOTRAM_RSV_SIZE			0x50

enum {
	RV1103B_GPIO_PULL_NONE,
	RV1103B_GPIO_PULL_UP,
	RV1103B_GPIO_PULL_DOWN,
	RV1103B_GPIO_PULL_UP_DOWN,
};

struct rockchip_pm_data {
	const struct platform_suspend_ops *ops;
	int (*init)(struct device_node *np);
};

struct rv1103b_sleep_ddr_data {
	u32 entered_pmu_fsm;

	u32 cru_gate_con[RV1103B_CRU_GATE_CON_NUM];
	u32 pmu0cru_gate_con[RV1103B_PMU0CRU_GATE_CON_NUM];
	u32 pmu1cru_gate_con[RV1103B_PMU1CRU_GATE_CON_NUM];
	u32 pericru_gate_con[RV1103B_PERICRU_GATE_CON_NUM];
	u32 npucru_gate_con[RV1103B_NPUCRU_GATE_CON_NUM];
	u32 venccru_gate_con[RV1103B_VENCCRU_GATE_CON_NUM];
	u32 vicru_gate_con[RV1103B_VICRU_GATE_CON_NUM];
	u32 corecru_gate_con[RV1103B_CORECRU_GATE_CON_NUM];

	u32 ddrgrf_con1, ddrgrf_con5, ddrgrf_con8;
	u32 pmugrf_soc_con0, pmugrf_soc_con4, pmugrf_soc_con5, pmugrf_soc_con6;
	u32 gpio0a_iomux_l, gpio0a_iomux_h, gpio0b_iomux_l, gpio0b_iomux_h;
	u32 gpio0a_pull, gpio0b_pull;
	u32 gpio0_ddr_l, gpio0_ddr_h, gpio0_dr_l, gpio0_dr_h;
	u32 pmu_wkup_int_st, gpio0_int_st;
};

static struct rv1103b_sleep_ddr_data ddr_data;

static struct rk_sleep_config slp_cfg;

static u32 sleep_rsv_mem_phy;
static u32 *sleep_rsv_mem;
static suspend_state_t pm_suspend_state = PM_SUSPEND_MEM;

static void __iomem *pericru_base;
static void __iomem *venccru_base;
static void __iomem *npucru_base;
static void __iomem *vicru_base;
static void __iomem *corecru_base;
static void __iomem *cru_base;
static void __iomem *pmu0cru_base;
static void __iomem *pmu1cru_base;

static void __iomem *npugrf_base;
static void __iomem *coregrf_base;
static void __iomem *ddrc_base;
static void __iomem *ddrgrf_base;
static void __iomem *perigrf_base;
static void __iomem *pmugrf_base;

static void __iomem *ioc3_base;
static void __iomem *ioc47_base;
static void __iomem *ioc6_base;
static void __iomem *ioc0_base;
static void __iomem *ioc1_base;
static void __iomem *gpio_base[3];

static void __iomem *perisgrf_base;
static void __iomem *pmusgrf_base;

static void __iomem *qos_cpu_base;
static void __iomem *qos_crypto_base;
static void __iomem *qos_dcf_base;
static void __iomem *qos_decom_base;
static void __iomem *qos_dma2ddr_base;
static void __iomem *qos_mac_base;
static void __iomem *qos_mcu_base;
static void __iomem *qos_rga2e_rd_base;
static void __iomem *qos_rga2e_wr_base;
static void __iomem *qos_rkdma_base;
static void __iomem *qos_sdmmc1_base;
static void __iomem *qos_usb_base;
static void __iomem *qos_emmc_base;
static void __iomem *qos_fspi_base;
static void __iomem *qos_isp_base;
static void __iomem *qos_sdmmc0_base;
static void __iomem *qos_vicap_base;
static void __iomem *qos_npu_base;
static void __iomem *qos_rkvdec_base;
static void __iomem *qos_fspi_pmu_base;
static void __iomem *qos_lpmcu_base;
static void __iomem *qos_spi2ahb_base;

static void __iomem *gicd_base;
static void __iomem *gicc_base;

static void __iomem *pvtpll_core_base;
static void __iomem *pvtpll_isp_base;
static void __iomem *pvtpll_vepu_base;
static void __iomem *pvtpll_npu_base;

static void __iomem *hptimer_base;
static void __iomem *pmu_base;
static void __iomem *uartdbg_base;
static void __iomem *wdt_ns_base;
static void __iomem *wdt_s_base;
static void __iomem *nstimer_base[6];
static void __iomem *stimer_base[2];
static void __iomem *fw_ddr_base;
static void __iomem *pmusram_base;

#define WMSK_VAL		0xffff0000

static struct reg_region vd_core_reg_rgns[] = {
	/* core_cru */
	{ REG_REGION(0x300, 0x308, 4, &corecru_base, WMSK_VAL)},
	{ REG_REGION(0x800, 0x804, 4, &corecru_base, WMSK_VAL)},
	{ REG_REGION(0xa00, 0xa04, 4, &corecru_base, WMSK_VAL)},
	{ REG_REGION(0xd00, 0xd00, 4, &corecru_base, 0)},
	{ REG_REGION(0xd04, 0xd04, 4, &corecru_base, WMSK_VAL)},

	/* npu_cru */
	{ REG_REGION(0x300, 0x308, 4, &npucru_base, WMSK_VAL)},
	{ REG_REGION(0x800, 0x800, 4, &npucru_base, WMSK_VAL)},
	{ REG_REGION(0xa00, 0xa00, 4, &npucru_base, WMSK_VAL)},

	/* core_grf */
	{ REG_REGION(0x000, 0x000, 4, &coregrf_base, WMSK_VAL)},
	{ REG_REGION(0x004, 0x004, 4, &coregrf_base, 0)},

	/* npu_grf */
	{ REG_REGION(0x000, 0x000, 4, &npugrf_base, 0)},

	/* qos */
	{ REG_REGION(0x08, 0x18, 4, &qos_cpu_base, 0)},
	{ REG_REGION(0x08, 0x18, 4, &qos_npu_base, 0)},
};

static struct reg_region vd_log_reg_rgns[] = {
	/* firewall_ddr */
	{ REG_REGION(0x000, 0x01c, 4, &fw_ddr_base, 0)},
	{ REG_REGION(0x040, 0x060, 4, &fw_ddr_base, 0)},
	{ REG_REGION(0x0f0, 0x0f0, 4, &fw_ddr_base, 0)},

	/* cru */
	{ REG_REGION(0x040, 0x044, 4, &cru_base, WMSK_VAL)},
	{ REG_REGION(0x048, 0x048, 4, &cru_base, 0)},
	{ REG_REGION(0x04c, 0x050, 4, &cru_base, WMSK_VAL)},
	{ REG_REGION(0x060, 0x064, 4, &cru_base, WMSK_VAL)},
	{ REG_REGION(0x068, 0x068, 4, &cru_base, 0)},
	{ REG_REGION(0x06c, 0x070, 4, &cru_base, WMSK_VAL)},
	{ REG_REGION(0x140, 0x1bc, 4, &cru_base, 0)},
	/* { REG_REGION(0x280, 0x280, 4, &cru_base, WMSK_VAL)}, */
	{ REG_REGION(0x300, 0x308, 4, &cru_base, WMSK_VAL)},
	{ REG_REGION(0x314, 0x314, 4, &cru_base, WMSK_VAL)},
	{ REG_REGION(0x328, 0x330, 4, &cru_base, 0)},
	{ REG_REGION(0x350, 0x350, 4, &cru_base, WMSK_VAL)},
	{ REG_REGION(0x354, 0x354, 4, &cru_base, 0)},
	{ REG_REGION(0x378, 0x3a4, 4, &cru_base, WMSK_VAL)},
	{ REG_REGION(0x800, 0x818, 4, &cru_base, WMSK_VAL)},
	{ REG_REGION(0xa00, 0xa00, 4, &cru_base, WMSK_VAL)},
	{ REG_REGION(0xd00, 0xd10, 8, &cru_base, 0)},
	{ REG_REGION(0xd04, 0xd14, 8, &cru_base, WMSK_VAL)},
	{ REG_REGION(0xd18, 0xd20, 4, &cru_base, WMSK_VAL)},
	{ REG_REGION(0xc00, 0xc00, 4, &cru_base, 0)},
	{ REG_REGION(0xc10, 0xc10, 4, &cru_base, 0)},
	{ REG_REGION(0xcc0, 0xcc0, 4, &cru_base, 0)},

	/* peri_cru */
	{ REG_REGION(0x300, 0x30c, 4, &pericru_base, WMSK_VAL)},
	{ REG_REGION(0x800, 0x82c, 4, &pericru_base, WMSK_VAL)},
	{ REG_REGION(0xa00, 0xa2c, 4, &pericru_base, WMSK_VAL)},
	{ REG_REGION(0xc08, 0xc08, 4, &pericru_base, WMSK_VAL)},

	/* peri_grf */
	{ REG_REGION(0x000, 0x00c, 4, &perigrf_base, WMSK_VAL)},
	{ REG_REGION(0x020, 0x034, 4, &perigrf_base, WMSK_VAL)},
	{ REG_REGION(0x050, 0x05c, 4, &perigrf_base, WMSK_VAL)},
	{ REG_REGION(0x070, 0x078, 4, &perigrf_base, WMSK_VAL)},
	{ REG_REGION(0x080, 0x090, 4, &perigrf_base, 0)},
	{ REG_REGION(0x0a0, 0x0a4, 4, &perigrf_base, WMSK_VAL)},
	{ REG_REGION(0x0b0, 0x0b4, 4, &perigrf_base, WMSK_VAL)},
	{ REG_REGION(0x100, 0x108, 4, &perigrf_base, WMSK_VAL)},
	{ REG_REGION(0x110, 0x11c, 4, &perigrf_base, 0)},
	{ REG_REGION(0x200, 0x210, 4, &perigrf_base, 0)},
	{ REG_REGION(0x214, 0x214, 4, &perigrf_base, WMSK_VAL)},

	/* peri_sgrf */
	{ REG_REGION(0x008, 0x00c, 4, &perisgrf_base, WMSK_VAL)},
	{ REG_REGION(0x018, 0x018, 4, &perisgrf_base, WMSK_VAL)},
	{ REG_REGION(0x020, 0x03c, 4, &perisgrf_base, WMSK_VAL)},
	{ REG_REGION(0x080, 0x080, 4, &perisgrf_base, 0)},

	/* vi_cru */
	{ REG_REGION(0x300, 0x300, 4, &vicru_base, WMSK_VAL)},
	{ REG_REGION(0x800, 0x808, 4, &vicru_base, WMSK_VAL)},
	{ REG_REGION(0xa00, 0xa04, 4, &vicru_base, WMSK_VAL)},
	{ REG_REGION(0xc08, 0xc08, 4, &vicru_base, WMSK_VAL)},

	/* vepu_cru */
	{ REG_REGION(0x300, 0x308, 4, &venccru_base, WMSK_VAL)},
	{ REG_REGION(0x800, 0x800, 4, &venccru_base, WMSK_VAL)},
	{ REG_REGION(0xa00, 0xa00, 4, &venccru_base, WMSK_VAL)},

	/* gpio1~2 */
	{ REG_REGION(0x000, 0x00c, 4, &gpio_base[1], WMSK_VAL)},
	{ REG_REGION(0x018, 0x044, 4, &gpio_base[1], WMSK_VAL)},
	{ REG_REGION(0x048, 0x048, 4, &gpio_base[1], 0)},
	{ REG_REGION(0x060, 0x064, 4, &gpio_base[1], WMSK_VAL)},
	{ REG_REGION(0x100, 0x108, 4, &gpio_base[1], WMSK_VAL)},

	{ REG_REGION(0x000, 0x00c, 4, &gpio_base[2], WMSK_VAL)},
	{ REG_REGION(0x018, 0x044, 4, &gpio_base[2], WMSK_VAL)},
	{ REG_REGION(0x048, 0x048, 4, &gpio_base[2], 0)},
	{ REG_REGION(0x060, 0x064, 4, &gpio_base[2], WMSK_VAL)},
	/* restore gpio_virtual_en after gpio_group */
	{ REG_REGION(0x100, 0x108, 4, &gpio_base[2], WMSK_VAL)},

	/* vccio3_ioc */
	{ REG_REGION(0x020, 0x024, 4, &ioc3_base, WMSK_VAL)},
	{ REG_REGION(0x140, 0x148, 4, &ioc3_base, WMSK_VAL)},
	{ REG_REGION(0x210, 0x210, 4, &ioc3_base, WMSK_VAL)},
	{ REG_REGION(0x310, 0x310, 4, &ioc3_base, WMSK_VAL)},
	{ REG_REGION(0x410, 0x410, 4, &ioc3_base, WMSK_VAL)},
	{ REG_REGION(0x510, 0x510, 4, &ioc3_base, WMSK_VAL)},
	{ REG_REGION(0x610, 0x610, 4, &ioc3_base, WMSK_VAL)},
	{ REG_REGION(0x710, 0x710, 4, &ioc3_base, WMSK_VAL)},
	{ REG_REGION(0x800, 0x800, 4, &ioc3_base, WMSK_VAL)},

	/* vccio47_ioc */
	{ REG_REGION(0x024, 0x03c, 4, &ioc47_base, WMSK_VAL)},
	{ REG_REGION(0x14c, 0x160, 4, &ioc47_base, WMSK_VAL)},
	{ REG_REGION(0x210, 0x218, 4, &ioc47_base, WMSK_VAL)},
	{ REG_REGION(0x310, 0x318, 4, &ioc47_base, WMSK_VAL)},
	{ REG_REGION(0x410, 0x418, 4, &ioc47_base, WMSK_VAL)},
	{ REG_REGION(0x510, 0x518, 4, &ioc47_base, WMSK_VAL)},
	{ REG_REGION(0x610, 0x618, 4, &ioc47_base, WMSK_VAL)},
	{ REG_REGION(0x710, 0x718, 4, &ioc47_base, WMSK_VAL)},
	{ REG_REGION(0x800, 0x808, 4, &ioc47_base, WMSK_VAL)},
	{ REG_REGION(0x80c, 0x80c, 4, &ioc47_base, 0)},
	{ REG_REGION(0x810, 0x810, 4, &ioc47_base, WMSK_VAL)},

	/* vccio6_ioc */
	{ REG_REGION(0x040, 0x048, 4, &ioc6_base, WMSK_VAL)},
	{ REG_REGION(0x180, 0x194, 4, &ioc6_base, WMSK_VAL)},
	{ REG_REGION(0x220, 0x224, 4, &ioc6_base, WMSK_VAL)},
	{ REG_REGION(0x320, 0x324, 4, &ioc6_base, WMSK_VAL)},
	{ REG_REGION(0x420, 0x424, 4, &ioc6_base, WMSK_VAL)},
	{ REG_REGION(0x520, 0x524, 4, &ioc6_base, WMSK_VAL)},
	{ REG_REGION(0x620, 0x624, 4, &ioc6_base, WMSK_VAL)},
	{ REG_REGION(0x720, 0x724, 4, &ioc6_base, WMSK_VAL)},
	{ REG_REGION(0x800, 0x804, 4, &ioc6_base, WMSK_VAL)},
	{ REG_REGION(0x80c, 0x810, 4, &ioc6_base, WMSK_VAL)},

	/* gpio1~2 int en */
	{ REG_REGION(0x010, 0x014, 4, &gpio_base[1], WMSK_VAL)},
	{ REG_REGION(0x010, 0x014, 4, &gpio_base[2], WMSK_VAL)},

	/* NS TIMER 6 channel */
	{ REG_REGION(0x00, 0x04, 4, &nstimer_base[0], 0)},
	{ REG_REGION(0x10, 0x10, 4, &nstimer_base[0], 0)},
	{ REG_REGION(0x20, 0x24, 4, &nstimer_base[1], 0)},
	{ REG_REGION(0x30, 0x30, 4, &nstimer_base[1], 0)},
	{ REG_REGION(0x40, 0x44, 4, &nstimer_base[2], 0)},
	{ REG_REGION(0x50, 0x50, 4, &nstimer_base[2], 0)},
	{ REG_REGION(0x60, 0x64, 4, &nstimer_base[3], 0)},
	{ REG_REGION(0x70, 0x70, 4, &nstimer_base[3], 0)},
	{ REG_REGION(0x80, 0x84, 4, &nstimer_base[4], 0)},
	{ REG_REGION(0x90, 0x90, 4, &nstimer_base[4], 0)},
	{ REG_REGION(0xa0, 0xa4, 4, &nstimer_base[5], 0)},
	{ REG_REGION(0xb0, 0xb0, 4, &nstimer_base[5], 0)},

	/* S TIMER0 2 channel */
	{ REG_REGION(0x00, 0x04, 4, &stimer_base[0], 0)},
	{ REG_REGION(0x10, 0x10, 4, &stimer_base[0], 0)},
	{ REG_REGION(0x20, 0x24, 4, &stimer_base[1], 0)},
	{ REG_REGION(0x30, 0x30, 4, &stimer_base[1], 0)},

	/* wdt_ns */
	{ REG_REGION(0x04, 0x04, 4, &wdt_ns_base, 0)},
	{ REG_REGION(0x00, 0x00, 4, &wdt_ns_base, 0)},

	/* wdt_s */
	{ REG_REGION(0x04, 0x04, 4, &wdt_s_base, 0)},
	{ REG_REGION(0x00, 0x00, 4, &wdt_s_base, 0)},

	/* qos */
	{ REG_REGION(0x08, 0x18, 4, &qos_crypto_base, 0)},
	{ REG_REGION(0x08, 0x18, 4, &qos_dcf_base, 0)},
	{ REG_REGION(0x08, 0x18, 4, &qos_decom_base, 0)},
	{ REG_REGION(0x08, 0x18, 4, &qos_dma2ddr_base, 0)},
	{ REG_REGION(0x08, 0x18, 4, &qos_mac_base, 0)},
	{ REG_REGION(0x08, 0x18, 4, &qos_mcu_base, 0)},
	{ REG_REGION(0x08, 0x18, 4, &qos_rga2e_rd_base, 0)},
	{ REG_REGION(0x08, 0x18, 4, &qos_rga2e_wr_base, 0)},
	{ REG_REGION(0x08, 0x18, 4, &qos_rkdma_base, 0)},
	{ REG_REGION(0x08, 0x18, 4, &qos_sdmmc1_base, 0)},
	{ REG_REGION(0x08, 0x18, 4, &qos_usb_base, 0)},
	{ REG_REGION(0x08, 0x18, 4, &qos_emmc_base, 0)},
	{ REG_REGION(0x08, 0x18, 4, &qos_fspi_base, 0)},
	{ REG_REGION(0x08, 0x18, 4, &qos_isp_base, 0)},
	{ REG_REGION(0x08, 0x18, 4, &qos_sdmmc0_base, 0)},
	{ REG_REGION(0x08, 0x18, 4, &qos_vicap_base, 0)},
	{ REG_REGION(0x08, 0x18, 4, &qos_rkvdec_base, 0)},
};

static struct reg_region pd_pmu1_reg_rgns[] = {
	/* pmu1_cru */
	{ REG_REGION(0x300, 0x300, 4, &pmu1cru_base, WMSK_VAL)},
	{ REG_REGION(0x800, 0x804, 4, &pmu1cru_base, WMSK_VAL)},
	{ REG_REGION(0xa00, 0xa04, 4, &pmu1cru_base, WMSK_VAL)},
	{ REG_REGION(0xc08, 0xc08, 4, &pmu1cru_base, WMSK_VAL)},

	/* pmu1_ioc */
	{ REG_REGION(0x008, 0x00c, 4, &ioc1_base, WMSK_VAL)},
	{ REG_REGION(0x110, 0x118, 4, &ioc1_base, WMSK_VAL)},
	{ REG_REGION(0x204, 0x204, 4, &ioc1_base, WMSK_VAL)},
	{ REG_REGION(0x304, 0x304, 4, &ioc1_base, WMSK_VAL)},
	{ REG_REGION(0x404, 0x404, 4, &ioc1_base, WMSK_VAL)},
	{ REG_REGION(0x504, 0x504, 4, &ioc1_base, WMSK_VAL)},
	{ REG_REGION(0x604, 0x604, 4, &ioc1_base, WMSK_VAL)},
	{ REG_REGION(0x704, 0x704, 4, &ioc1_base, WMSK_VAL)},
	{ REG_REGION(0x800, 0x804, 4, &ioc1_base, WMSK_VAL)},
	{ REG_REGION(0x808, 0x808, 4, &ioc1_base, 0)},

	/* qos */
	{ REG_REGION(0x08, 0x18, 4, &qos_fspi_pmu_base, 0)},
	{ REG_REGION(0x08, 0x18, 4, &qos_lpmcu_base, 0)},
	{ REG_REGION(0x08, 0x18, 4, &qos_spi2ahb_base, 0)},
};

static struct reg_region pd_pmu0_reg_rgns[] = {
	/* pmu0_cru */
	{ REG_REGION(0x300, 0x300, 4, &pmu0cru_base, WMSK_VAL)},
	{ REG_REGION(0x304, 0x304, 4, &pmu0cru_base, 0)},
	{ REG_REGION(0x308, 0x308, 4, &pmu0cru_base, WMSK_VAL)},
	{ REG_REGION(0x800, 0x808, 4, &pmu0cru_base, WMSK_VAL)},
	{ REG_REGION(0xd00, 0xd00, 4, &pmu0cru_base, 0)},
	{ REG_REGION(0xd04, 0xd04, 4, &pmu0cru_base, WMSK_VAL)},

	/* pmu_grf */
	{ REG_REGION(0x000, 0x01c, 4, &pmugrf_base, WMSK_VAL)},
	{ REG_REGION(0x020, 0x028, 4, &pmugrf_base, 0)},
	{ REG_REGION(0x040, 0x040, 4, &pmugrf_base, WMSK_VAL)},
	{ REG_REGION(0x050, 0x050, 4, &pmugrf_base, WMSK_VAL)},
	{ REG_REGION(0x060, 0x060, 4, &pmugrf_base, WMSK_VAL)},
	{ REG_REGION(0x200, 0x22c, 4, &pmugrf_base, 0)},

	/* pmu_sgrf */
	{ REG_REGION(0x000, 0x004, 4, &pmusgrf_base, WMSK_VAL)},
	{ REG_REGION(0x020, 0x020, 4, &pmusgrf_base, 0)},

	/* gpio0 */
	{ REG_REGION(0x000, 0x00c, 4, &gpio_base[0], WMSK_VAL)},
	{ REG_REGION(0x018, 0x044, 4, &gpio_base[0], WMSK_VAL)},
	{ REG_REGION(0x048, 0x048, 4, &gpio_base[0], 0)},
	{ REG_REGION(0x060, 0x064, 4, &gpio_base[0], WMSK_VAL)},
	{ REG_REGION(0x080, 0x084, 4, &gpio_base[0], WMSK_VAL)},
	{ REG_REGION(0x110, 0x124, 4, &gpio_base[0], WMSK_VAL)},
	{ REG_REGION(0x100, 0x108, 4, &gpio_base[0], WMSK_VAL)},

	/* pmu0_ioc */
	{ REG_REGION(0x000, 0x004, 4, &ioc0_base, WMSK_VAL)},
	{ REG_REGION(0x100, 0x10c, 4, &ioc0_base, WMSK_VAL)},
	{ REG_REGION(0x300, 0x300, 4, &ioc0_base, WMSK_VAL)},
	{ REG_REGION(0x400, 0x400, 4, &ioc0_base, WMSK_VAL)},
	{ REG_REGION(0x500, 0x500, 4, &ioc0_base, WMSK_VAL)},
	{ REG_REGION(0x600, 0x600, 4, &ioc0_base, WMSK_VAL)},
	{ REG_REGION(0x700, 0x700, 4, &ioc0_base, WMSK_VAL)},
	{ REG_REGION(0x800, 0x804, 4, &ioc0_base, WMSK_VAL)},
	{ REG_REGION(0x808, 0x808, 4, &ioc0_base, 0)},
	{ REG_REGION(0x900, 0x900, 4, &ioc0_base, WMSK_VAL)},

	/* pmu0_ioc pull */
	{ REG_REGION(0x200, 0x200, 4, &ioc0_base, WMSK_VAL)},

	/* gpio0 int en */
	{ REG_REGION(0x010, 0x014, 4, &gpio_base[0], WMSK_VAL)},
};

static struct reg_region pvtpll_core_reg_rgns[] = {
	{ REG_REGION(0x020, 0x024, 4, &pvtpll_core_base, WMSK_VAL)},
	{ REG_REGION(0x020, 0x024, 4, &pvtpll_npu_base, WMSK_VAL)},
};

static struct reg_region pvtpll_logic_reg_rgns[] = {
	{ REG_REGION(0x020, 0x024, 4, &pvtpll_vepu_base, WMSK_VAL)},
	{ REG_REGION(0x020, 0x024, 4, &pvtpll_isp_base, WMSK_VAL)},
};

#define SYS_REG_DEC_BW(n)		(2 >> (((n) >> 2) & 0x3))
#define SYS_REG_DEC_COL(n)		(9 + (((n) >> 9) & 0x3))
#define SYS_REG_DEC_BK(n)		(3 - (((n) >> 8) & 0x1))
#define SYS_REG_DEC_CS0_ROW(os_reg2, os_reg3)		\
		((((((os_reg2) >> 6 & 0x3) |		\
		 ((((os_reg3) >> 5) & 0x1) << 2)) + 1) & 0x7) + 12)

static void init_sleep_reserve_mem(void)
{
	u32 os_reg2, os_reg3;
	u32 bw, col, bk, cs0_row, cap;

	if (sleep_rsv_mem != NULL)
		goto mem_ok;

	os_reg2 = readl_relaxed(pmugrf_base + RV1103B_PMUGRF_OS_REG(2));
	os_reg3 = readl_relaxed(pmugrf_base + RV1103B_PMUGRF_OS_REG(3));
	bw = SYS_REG_DEC_BW(os_reg2);
	col = SYS_REG_DEC_COL(os_reg2);
	bk = SYS_REG_DEC_BK(os_reg2);
	cs0_row = SYS_REG_DEC_CS0_ROW(os_reg2, os_reg3);

	cap = 1u << (bw + col + bk + cs0_row);

	/* last 4k of ddr */
	sleep_rsv_mem_phy = cap - SLEEP_RESERVE_MEM_SIZE;
	rkpm_printstr("sleep_rsv_mem_phy:");
	rkpm_printhex(sleep_rsv_mem_phy);
	rkpm_printstr("\n");

	sleep_rsv_mem = (u32 *)ioremap(sleep_rsv_mem_phy, SLEEP_RESERVE_MEM_SIZE);
	if (sleep_rsv_mem == NULL) {
		rkpm_printstr("can't map sleep_rsv_mem, halt!!!");
		rkpm_printhex(sleep_rsv_mem_phy);
		asm("b .");
	} else {
		rkpm_printstr("map sleep_rsv_mem:");
		rkpm_printhex((u32)sleep_rsv_mem);
		rkpm_printstr("\n");
	}

mem_ok:
	/* save data for ddr.bin */
	sleep_rsv_mem[0] = PMU_SUSPEND_MAGIC;
	sleep_rsv_mem[1] = __pa_symbol(cpu_resume);
	sleep_rsv_mem[2] = readl_relaxed(pmugrf_base + RV1103B_PMUGRF_OS_REG(2));
	sleep_rsv_mem[3] = readl_relaxed(pmugrf_base + RV1103B_PMUGRF_OS_REG(3));
	sleep_rsv_mem[4] = readl_relaxed(pmugrf_base + RV1103B_PMUGRF_OS_REG(4));
}

#define PLL_LOCKED_TIMEOUT		600000U

static void pm_pll_wait_lock(u32 pll_id)
{
	int delay = PLL_LOCKED_TIMEOUT;

	if (readl_relaxed(cru_base + RV1103B_CRU_PLL_CON(pll_id, 1)) & CRU_PLLCON1_PWRDOWN)
		return;

	while (--delay >= 0) {
		if (readl_relaxed(cru_base + RV1103B_CRU_PLL_CON(pll_id, 1)) &
		    CRU_PLLCON1_LOCK_STATUS)
			break;

		rkpm_raw_udelay(1);
	}

	if (delay < 0) {
		rkpm_printstr("Can't wait pll lock: ");
		rkpm_printhex(pll_id);
		rkpm_printch('\n');
	}
}

static struct plat_gicv2_dist_ctx_t gicd_ctx_save;
static struct plat_gicv2_cpu_ctx_t gicc_ctx_save;

static void gic400_save(void)
{
	rkpm_gicv2_cpu_save(gicd_base, gicc_base, &gicc_ctx_save);
	rkpm_gicv2_dist_save(gicd_base, &gicd_ctx_save);
}

static void gic400_restore(void)
{
	rkpm_gicv2_dist_restore(gicd_base, &gicd_ctx_save);
	rkpm_gicv2_cpu_restore(gicd_base, gicc_base, &gicc_ctx_save);
}

static void uart_write_byte(uint8_t byte)
{
	writel_relaxed(byte, uartdbg_base + 0x0);

	while (!(readl_relaxed(uartdbg_base + 0x14) & 0x40))
		;
}

void rkpm_printch(int c)
{
	if (c == '\n')
		uart_write_byte('\r');

	uart_write_byte(c);
}

#define RV1103B_DUMP_GPIO_INTEN(id)							\
	do {										\
		rkpm_printstr("GPIO");							\
		rkpm_printdec(id);							\
		rkpm_printstr(": ");							\
		rkpm_printhex(readl_relaxed(gpio_base[id] + RV1103B_GPIO_INT_EN_L));	\
		rkpm_printch(' ');							\
		rkpm_printhex(readl_relaxed(gpio_base[id] + RV1103B_GPIO_INT_EN_H));	\
		rkpm_printch(' ');							\
		rkpm_printhex(readl_relaxed(gpio_base[id] + RV1103B_GPIO_INT_MASK_L));	\
		rkpm_printch(' ');							\
		rkpm_printhex(readl_relaxed(gpio_base[id] + RV1103B_GPIO_INT_MASK_H));	\
		rkpm_printch(' ');							\
		rkpm_printhex(readl_relaxed(gpio_base[id] + RV1103B_GPIO_INT_STATUS));	\
		rkpm_printch(' ');							\
		rkpm_printhex(readl_relaxed(gpio_base[id] + RV1103B_GPIO_INT_RAWSTATUS));\
		rkpm_printch('\n');							\
	} while (0)

static void  rv1103b_dbg_sleep_enter_info(void)
{
	static u32 sleep_cnt;
	u32 cfg = slp_cfg.mode_config;

	rkpm_printstr("enter:");
	rkpm_printhex(cfg);
	rkpm_printstr(", ");
	rkpm_printhex(slp_cfg.wakeup_config);
	rkpm_printstr(", ");
	rkpm_printdec(++sleep_cnt);
	rkpm_printch('\n');

	switch (pm_suspend_state) {
	case PM_SUSPEND_MEM_ULTRA:
		rkpm_printstr("ultra\n");
		break;
	case PM_SUSPEND_MEM_LITE:
		rkpm_printstr("lite\n");
		break;
	case PM_SUSPEND_MEM:
		rkpm_printstr("deep\n");
		break;
	default:
		break;
	}

	if (cfg & RKPM_SLP_ARMPD)
		rkpm_printstr("armpd\n");
	if (cfg & RKPM_SLP_ARMOFF)
		rkpm_printstr("armoff\n");
	if (cfg & RKPM_SLP_ARMOFF_LOGOFF)
		rkpm_printstr("logoff\n");
	if (cfg & RKPM_SLP_ARMOFF_PMUOFF)
		rkpm_printstr("pmuoff\n");
	if (cfg & RKPM_SLP_PMU_HW_PLLS_PD)
		rkpm_printstr("hw_plls_pd\n");
	if (cfg & RKPM_SLP_PMU_PMUALIVE_32K)
		rkpm_printstr("pmualive_32k\n");
	if (cfg & RKPM_SLP_PMU_DIS_OSC)
		rkpm_printstr("dis_osc\n");
	if (cfg & RKPM_SLP_32K_EXT)
		rkpm_printstr("32k ext\n");
	if (cfg & RKPM_SLP_TIME_OUT_WKUP)
		rkpm_printstr("timeout wkup\n");
	if (cfg & RKPM_SLP_PMU_DBG)
		rkpm_printstr("pmu debug\n");
	if (cfg & RKPM_SLP_LP_PR)
		rkpm_printstr("LP_PR\n");
}

static void rv1103b_dbg_pmu_wkup_src(void)
{
	u32 pmu_int_st = ddr_data.pmu_wkup_int_st;

	rkpm_printstr("wake up status:");
	rkpm_printhex(pmu_int_st);
	rkpm_printch('\n');

	if (pmu_int_st)
		rkpm_printstr("wake up information:\n");

	if (pmu_int_st & BIT(RV1103B_PMU_WAKEUP_GPIO_INT)) {
		rkpm_printstr("GPIO0 wakeup:");
		rkpm_printhex(ddr_data.gpio0_int_st);
		rkpm_printch('\n');
	}

	if (pmu_int_st & BIT(RV1103B_PMU_WAKEUP_SDMMC0))
		rkpm_printstr("SDMMC wakeup\n");

	if (pmu_int_st & BIT(RV1103B_PMU_WAKEUP_SDIO))
		rkpm_printstr("SDIO wakeup\n");

	if (pmu_int_st & BIT(RV1103B_PMU_WAKEUP_USBDEV))
		rkpm_printstr("USBDEV wakeup\n");

	if (pmu_int_st & BIT(RV1103B_PMU_WAKEUP_UART0))
		rkpm_printstr("UART0 wakeup\n");

	if (pmu_int_st & BIT(RV1103B_PMU_WAKEUP_PWM0))
		rkpm_printstr("PWM0 wakeup\n");

	if (pmu_int_st & BIT(RV1103B_PMU_WAKEUP_TIMER))
		rkpm_printstr("TIMER wakeup\n");

	if (pmu_int_st & BIT(RV1103B_PMU_WAKEUP_HPTIMER))
		rkpm_printstr("HPTIMER wakeup\n");

	if (pmu_int_st & BIT(RV1103B_PMU_WAKEUP_SYS_INT))
		rkpm_printstr("SYS_INT wakeup\n");

	if (pmu_int_st & BIT(RV1103B_PMU_WAKEUP_AOV))
		rkpm_printstr("AOV wakeup\n");

	if (pmu_int_st & BIT(RV1103B_PMU_WAKEUP_TIMEOUT))
		rkpm_printstr("TIMEOUT wakeup\n");

	rkpm_printch('\n');
}

static void rv1103b_dbg_irq_prepare(void)
{
	RV1103B_DUMP_GPIO_INTEN(0);
}

static void rv1103b_dbg_irq_finish(void)
{
	rv1103b_dbg_pmu_wkup_src();
}

static inline u32 rv1103b_l2_config(void)
{
	u32 l2ctlr;

	asm("mrc p15, 1, %0, c9, c0, 2" : "=r" (l2ctlr));
	return l2ctlr;
}

static void __init rv1103b_config_bootdata(void)
{
	rkpm_bootdata_cpusp = RV1103B_PMUSRAM_BASE + (SZ_8K - 8);
	rkpm_bootdata_cpu_code = __pa_symbol(cpu_resume);

	rkpm_bootdata_l2ctlr_f = 1;
	rkpm_bootdata_l2ctlr = rv1103b_l2_config();
}

static void clock_suspend(void)
{
	int i;

	for (i = 0; i < RV1103B_CRU_GATE_CON_NUM; i++) {
		ddr_data.cru_gate_con[i] =
			readl_relaxed(cru_base + RV1103B_CRU_GATE_CON(i));
		writel_relaxed(0xffff0000, cru_base + RV1103B_CRU_GATE_CON(i));
	}

	for (i = 0; i < RV1103B_PMU0CRU_GATE_CON_NUM; i++) {
		ddr_data.pmu0cru_gate_con[i] =
			readl_relaxed(pmu0cru_base + RV1103B_PMU0CRU_GATE_CON(i));
		writel_relaxed(0xffff0000, pmu0cru_base + RV1103B_PMU0CRU_GATE_CON(i));
	}

	for (i = 0; i < RV1103B_PMU1CRU_GATE_CON_NUM; i++) {
		ddr_data.pmu1cru_gate_con[i] =
			readl_relaxed(pmu1cru_base + RV1103B_PMU1CRU_GATE_CON(i));
		writel_relaxed(0xffff0000, pmu1cru_base + RV1103B_PMU1CRU_GATE_CON(i));
	}

	for (i = 0; i < RV1103B_PERICRU_GATE_CON_NUM; i++) {
		ddr_data.pericru_gate_con[i] =
			readl_relaxed(pericru_base + RV1103B_PERICRU_GATE_CON(i));
		writel_relaxed(0xffff0000, pericru_base + RV1103B_PERICRU_GATE_CON(i));
	}

	for (i = 0; i < RV1103B_NPUCRU_GATE_CON_NUM; i++) {
		ddr_data.npucru_gate_con[i] =
			readl_relaxed(npucru_base + RV1103B_NPUCRU_GATE_CON(i));
		writel_relaxed(0xffff0000, npucru_base + RV1103B_NPUCRU_GATE_CON(i));
	}

	for (i = 0; i < RV1103B_VENCCRU_GATE_CON_NUM; i++) {
		ddr_data.venccru_gate_con[i] =
			readl_relaxed(venccru_base + RV1103B_VENCCRU_GATE_CON(i));
		writel_relaxed(0xffff0000, venccru_base + RV1103B_VENCCRU_GATE_CON(i));
	}

	for (i = 0; i < RV1103B_VICRU_GATE_CON_NUM; i++) {
		ddr_data.vicru_gate_con[i] =
			readl_relaxed(vicru_base + RV1103B_VICRU_GATE_CON(i));
		writel_relaxed(0xffff0000, vicru_base + RV1103B_VICRU_GATE_CON(i));
	}

	for (i = 0; i < RV1103B_CORECRU_GATE_CON_NUM; i++) {
		ddr_data.corecru_gate_con[i] =
			readl_relaxed(corecru_base + RV1103B_CORECRU_GATE_CON(i));
		writel_relaxed(0xffff0000, corecru_base + RV1103B_CORECRU_GATE_CON(i));
	}
}

static void clock_resume(void)
{
	int i;

	for (i = 0; i < RV1103B_CRU_GATE_CON_NUM; i++)
		writel_relaxed(WITH_16BITS_WMSK(ddr_data.cru_gate_con[i]),
			       cru_base + RV1103B_CRU_GATE_CON(i));

	for (i = 0; i < RV1103B_PMU0CRU_GATE_CON_NUM; i++)
		writel_relaxed(WITH_16BITS_WMSK(ddr_data.pmu0cru_gate_con[i]),
			       pmu0cru_base + RV1103B_PMU0CRU_GATE_CON(i));

	for (i = 0; i < RV1103B_PMU1CRU_GATE_CON_NUM; i++)
		writel_relaxed(WITH_16BITS_WMSK(ddr_data.pmu1cru_gate_con[i]),
			       pmu1cru_base + RV1103B_PMU1CRU_GATE_CON(i));

	for (i = 0; i < RV1103B_PERICRU_GATE_CON_NUM; i++)
		writel_relaxed(WITH_16BITS_WMSK(ddr_data.pericru_gate_con[i]),
			       pericru_base + RV1103B_PERICRU_GATE_CON(i));

	for (i = 0; i < RV1103B_NPUCRU_GATE_CON_NUM; i++)
		writel_relaxed(WITH_16BITS_WMSK(ddr_data.npucru_gate_con[i]),
			       npucru_base + RV1103B_NPUCRU_GATE_CON(i));

	for (i = 0; i < RV1103B_VENCCRU_GATE_CON_NUM; i++)
		writel_relaxed(WITH_16BITS_WMSK(ddr_data.venccru_gate_con[i]),
			       venccru_base + RV1103B_VENCCRU_GATE_CON(i));

	for (i = 0; i < RV1103B_VICRU_GATE_CON_NUM; i++)
		writel_relaxed(WITH_16BITS_WMSK(ddr_data.vicru_gate_con[i]),
			       vicru_base + RV1103B_VICRU_GATE_CON(i));

	for (i = 0; i < RV1103B_CORECRU_GATE_CON_NUM; i++)
		writel_relaxed(WITH_16BITS_WMSK(ddr_data.corecru_gate_con[i]),
			       corecru_base + RV1103B_CORECRU_GATE_CON(i));
}

static void sleep_32k_config(void)
{
	if (slp_cfg.mode_config & RKPM_SLP_32K_EXT) {
		/* deepslow select clk_32k_rtc */
		writel_relaxed(BITS_WITH_WMASK(0x1, 0x3, 0),
			       pmu0cru_base + RV1103B_PMU0CRU_CLKSEL_CON(0));
	} else if ((slp_cfg.mode_config & RKPM_SLP_LP_PR) == 0) {
		/* 125M * (16 / 61035) = 32.768k */
		writel_relaxed(0x0010ee6b, pmu0cru_base + RV1103B_PMU0CRU_CLKSEL_CON(1));
		/* select rc_osc_io */
		writel_relaxed(BITS_WITH_WMASK(0x1, 0x1, 2),
			       pmu0cru_base + RV1103B_PMU0CRU_CLKSEL_CON(0));
		/* deepslow select xin_rc_div */
		writel_relaxed(BITS_WITH_WMASK(0x0, 0x3, 0),
			       pmu0cru_base + RV1103B_PMU0CRU_CLKSEL_CON(0));
		/* enable rc_osc */
		writel_relaxed(BITS_WITH_WMASK(0x1, 0x7, 0),
			       pmugrf_base + RV1103B_PMUGRF_SOC_CON(7));
	}
}

static void sleep_32k_config_restore(void)
{
	/* if no ext-32k, select osc_div_32k */
	if ((slp_cfg.mode_config & RKPM_SLP_32K_EXT) == 0) {
		writel_relaxed(BITS_WITH_WMASK(0x0, 0x1, 2),
			       pmu0cru_base + RV1103B_PMU0CRU_CLKSEL_CON(0));
		writel_relaxed(BITS_WITH_WMASK(0x0, 0x3, 0),
			       pmu0cru_base + RV1103B_PMU0CRU_CLKSEL_CON(0));
	}
}

static void ddr_sleep_config(void)
{
	u32 val;

	ddr_data.ddrgrf_con1 = readl_relaxed(ddrgrf_base + RV1103B_DDRGRF_CON(1));
	ddr_data.ddrgrf_con5 = readl_relaxed(ddrgrf_base + RV1103B_DDRGRF_CON(5));
	ddr_data.ddrgrf_con8 = readl_relaxed(ddrgrf_base + RV1103B_DDRGRF_CON(8));

	ddr_data.pmugrf_soc_con0 = readl_relaxed(pmugrf_base + RV1103B_PMUGRF_SOC_CON(0));

	/* PWRCTL, disable auto powerdown and auto selfref */
	val = readl_relaxed(ddrc_base + 0x30);
	writel_relaxed(val & ~(BIT(0) | BIT(1)), ddrc_base + 0x30);

	/* disable ddrc_aclk_auto_gate and biu_clk_auto_gate */
	writel_relaxed(BITS_WITH_WMASK(0x0, 0x3, 9), ddrgrf_base + RV1103B_DDRGRF_CON(1));
	/* disable ddrc_axi/core/apb/syscreq/pdsrlp_cg_en */
	writel_relaxed(BITS_WITH_WMASK(0x0, 0x1ff, 0), ddrgrf_base + RV1103B_DDRGRF_CON(1));

	/* csysreq_ddrc_pmu, the hardware low power request signal by pmu */
	writel_relaxed(BITS_WITH_WMASK(0x1, 0x1, 3), ddrgrf_base + RV1103B_DDRGRF_CON(5));
	/* csysreq_aclk_cpu/npvd/vi_pmu enable */
	writel_relaxed(BITS_WITH_WMASK(0x7, 0x7, 4), ddrgrf_base + RV1103B_DDRGRF_CON(8));

	/* STAT, waiting operating_mode to Normal */
	while ((readl_relaxed(ddrc_base + 0x4) & 0x7) != 0x1)
		continue;

	/* ddr io_ret and io_hz by pmu */
	writel_relaxed(BITS_WITH_WMASK(0x0, 0xf, 9), pmugrf_base + RV1103B_PMUGRF_SOC_CON(0));
}

static void ddr_sleep_config_restore(void)
{
	writel_relaxed(WITH_16BITS_WMSK(ddr_data.pmugrf_soc_con0),
		       pmugrf_base + RV1103B_PMUGRF_SOC_CON(0));

	writel_relaxed(WITH_16BITS_WMSK(ddr_data.ddrgrf_con1), ddrgrf_base + RV1103B_DDRGRF_CON(1));
	writel_relaxed(WITH_16BITS_WMSK(ddr_data.ddrgrf_con8), ddrgrf_base + RV1103B_DDRGRF_CON(8));
	writel_relaxed(WITH_16BITS_WMSK(ddr_data.ddrgrf_con5), ddrgrf_base + RV1103B_DDRGRF_CON(5));
}

static void suspend_workaround_timeout_wkup(void)
{
	int wkup;
	u64 delta_cnt;

	if (ddr_data.entered_pmu_fsm)
		return;

	wkup = readl_relaxed(pmu_base + RV1103B_PMU1_WAKEUP_INT_CON);
	if ((wkup & BIT(RV1103B_PMU_WAKEUP_TIMEOUT)) == 0)
		return;

	wkup = (wkup & ~BIT(RV1103B_PMU_WAKEUP_TIMEOUT)) |
	       BIT(RV1103B_PMU_WAKEUP_HPTIMER);
	writel_relaxed(wkup, pmu_base + RV1103B_PMU1_WAKEUP_INT_CON);

	delta_cnt = readl_relaxed(pmu_base + RV1103B_PMU1_WAKEUP_TIMEOUT);

	if (slp_cfg.mode_config & RKPM_SLP_PMU_PMUALIVE_32K)
		delta_cnt = delta_cnt * 24000000 / 32768;

	rk_hptimer_v2_config_sleep_timeout_int(hptimer_base, delta_cnt);
}

static void resume_workaround_timeout_wkup(void)
{
	if (ddr_data.entered_pmu_fsm)
		return;

	/* resume from pmu_fsm */
	if (ddr_data.pmu_wkup_int_st)
		ddr_data.entered_pmu_fsm = 1;

	rk_hptimer_v2_disable_int(hptimer_base, RK_HPTIMER_V2_INT_32K_REACH);
	rk_hptimer_v2_clear_int_st(hptimer_base, RK_HPTIMER_V2_INT_32K_REACH);
}

static void pmu_sleep_config(void)
{
	u32 clk_freq_khz = 32;
	u32 pmu0_pwr_con;
	u32 pmu1_wkup_con, pmu1_pwr_con, pmu1_scu_con, pmu1_ddr_con;
	u32 pmu2_bus_idle_con, pmu1_cru_con[2], pmu1_pll_con;
	u32 cfg = slp_cfg.mode_config;

	ddr_data.pmugrf_soc_con4 = readl_relaxed(pmugrf_base + RV1103B_PMUGRF_SOC_CON(4));
	ddr_data.pmugrf_soc_con5 = readl_relaxed(pmugrf_base + RV1103B_PMUGRF_SOC_CON(5));
	ddr_data.pmugrf_soc_con6 = readl_relaxed(pmugrf_base + RV1103B_PMUGRF_SOC_CON(6));

	pmu0_pwr_con = 0;

	pmu1_wkup_con = slp_cfg.wakeup_config;

	if (readl_relaxed(pmu_base + RV1103B_PMU1_WAKEUP_TIMEOUT) != 0)
		pmu1_wkup_con |= BIT(RV1103B_PMU_WAKEUP_TIMEOUT);

	pmu1_pwr_con =
		BIT(RV1103B_PMU_PWRMODE1_EN) |
		/* BIT(RV1103B_PMU_SCU_BYPASS) | */
		/* BIT(RV1103B_PMU_BUS_BYPASS) | */
		/* BIT(RV1103B_PMU_DDR_BYPASS) | */
		/* BIT(RV1103B_PMU_PWRGT_BYPASS) | */
		/* BIT(RV1103B_PMU_CRU_BYPASS) | */
		BIT(RV1103B_PMU_PDPMU1_BYPASS) |
		/* BIT(RV1103B_PMU_WFI_BYPASS) | */
		BIT(RV1103B_PMU_SLP_CNT_EN) |
		0;

	pmu1_scu_con =
		BIT(RV1103B_PMU_SCU_L2_FLUSH) |
		BIT(RV1103B_PMU_SCU_L2_IDLE) |
		BIT(RV1103B_PMU_SCU_PWRDN) |
		BIT(RV1103B_PMU_SCU_PWROFF) |
		BIT(RV1103B_PMU_CLST_CPU_PD) |
		BIT(RV1103B_PMU_SCU_VOL_GT) |
		BIT(RV1103B_PMU_CLST_CLK_SRC_GT) |
		0;

	pmu2_bus_idle_con =
		BIT(RV1103B_PMU_IDLE_REQ_MSCH) |
		BIT(RV1103B_PMU_IDLE_REQ_DDRC) |
		BIT(RV1103B_PMU_IDLE_REQ_PERI) |
		BIT(RV1103B_PMU_IDLE_REQ_VEPU) |
		BIT(RV1103B_PMU_IDLE_REQ_VI) |
		BIT(RV1103B_PMU_IDLE_REQ_CRU) |
		0;

	pmu1_cru_con[0] =
		BIT(RV1103B_PMU_WAKEUP_RST) |
		BIT(RV1103B_PMU_INPUT_CLAMP) |
		BIT(RV1103B_PMU_ALIVE_OSC_EN) |
		BIT(RV1103B_PMU_POWER_OFF) |
		/* BIT(RV1103B_PMU_PWM_SWITCH) | */
		/* BIT(RV1103B_PMU_GPIO_IOE) | */
		/* BIT(RV1103B_PMU_PWM_SWITCH_IOUT) | */
		BIT(RV1103B_PMU_OFF_IO) |
		/* BIT(RV1103B_PWM_CLK_GT_PLL) | */
		/* BIT(RV1103B_PWM_CLK_GT_OSC) | */
		0;

	pmu1_cru_con[1] =
		/* BIT(RV1103B_PMU_PERI_CLK_SRC_GT) | */
		/* BIT(RV1103B_PMU_VENC_CLK_SRC_GT) | */
		/* BIT(RV1103B_PMU_VI_CLK_SRC_GT) | */
		/* BIT(RV1103B_PMU_NPU_CLK_SRC_GT) | */
		/* BIT(RV1103B_PMU_CORE_CLK_SRC_GT) | */
		/* BIT(RV1103B_PMU_DDR_CLK_SRC_GT) | */
		0;

	pmu1_ddr_con =
		BIT(RV1103B_PMU_DDR_SREF_C) |
		BIT(RV1103B_PMU_DDR_SREF_A) |
		BIT(RV1103B_PMU_DDRIO_RETON_ENTER) |
		/* BIT(RV1103B_PMU_DDRIO_RETON_EXIT) | */
		BIT(RV1103B_PMU_DDRIO_RSTIOV_ENTER) |
		/* BIT(RV1103B_PMU_DDRIO_RSTIOV_EXIT) | */
		BIT(RV1103B_PMU_DDRCTL_A_AUTO_GATING) |
		BIT(RV1103B_PMU_DDRCTL_C_AUTO_GATING) |
		BIT(RV1103B_PMU_DDRPHY_AUTO_GATING) |
		BIT(RV1103B_PMU_DDRIO_HZ_ENTER) |
		/* BIT(RV1103B_PMU_DDRIO_HZ_EXIT) | */
		0;

	pmu1_pll_con =
		BIT(RV1103B_PMU_DPLL_PD) |
		BIT(RV1103B_PMU_GPLL_PD) |
		0;

	if (cfg & RKPM_SLP_PMU_PMUALIVE_32K) {
		pmu1_cru_con[0] |=
			BIT(RV1103B_PMU_ALIVE_32K) |
			0;

		clk_freq_khz = 32;
	} else {
		clk_freq_khz = 24000;
	}

	if (cfg & RKPM_SLP_PMU_DIS_OSC) {
		pmu1_cru_con[0] |=
			BIT(RV1103B_PMU_OSC_DIS) |
			0;
	}

	if (cfg & RKPM_SLP_TIME_OUT_WKUP) {
		writel_relaxed(clk_freq_khz * 1000,
			       pmu_base + RV1103B_PMU1_WAKEUP_TIMEOUT);
		pmu1_wkup_con |= BIT(RV1103B_PMU_WAKEUP_TIMEOUT);
	}

	if (cfg & RKPM_SLP_ARMPD) {
		pmu1_pwr_con &=
			~(BIT(RV1103B_PMU_SLP_CNT_EN) |
			  0);

		pmu1_cru_con[0] &=
			~(BIT(RV1103B_PMU_WAKEUP_RST) |
			  BIT(RV1103B_PMU_INPUT_CLAMP) |
			  BIT(RV1103B_PMU_POWER_OFF) |
			  0);

		pmu1_ddr_con =
			BIT(RV1103B_PMU_DDR_SREF_C) |
			BIT(RV1103B_PMU_DDR_SREF_A) |
			0;

		/* resume from pmusram */
		writel_relaxed(BITS_WITH_WMASK(2, 0x3, 10),
			       pmusgrf_base + RV1103B_PMUSGRF_SOC_CON(1));
	} else if (cfg & RKPM_SLP_ARMOFF) {
		pmu1_cru_con[0] &=
			~(BIT(RV1103B_PMU_WAKEUP_RST) |
			  BIT(RV1103B_PMU_INPUT_CLAMP) |
			  0);

		pmu1_ddr_con =
			BIT(RV1103B_PMU_DDR_SREF_C) |
			BIT(RV1103B_PMU_DDR_SREF_A) |
			0;

		/* resume from pmusram */
		writel_relaxed(BITS_WITH_WMASK(2, 0x3, 10),
			       pmusgrf_base + RV1103B_PMUSGRF_SOC_CON(1));
	} else if ((cfg & RKPM_SLP_ARMOFF_LOGOFF) ||
		   (pm_suspend_state == PM_SUSPEND_MEM_ULTRA)) {
		/* pmu reset hold */
		/* except lpmcu */
		writel_relaxed(0xffff3fff, pmugrf_base + RV1103B_PMUGRF_SOC_CON(4));
		/* except lpmcu */
		writel_relaxed(0x007f007e, pmugrf_base + RV1103B_PMUGRF_SOC_CON(5));
		writel_relaxed(0xffffffff, pmugrf_base + RV1103B_PMUGRF_SOC_CON(6));

		/* resume from bootrom */
		writel_relaxed(BITS_WITH_WMASK(0, 0x3, 10),
			       pmusgrf_base + RV1103B_PMUSGRF_SOC_CON(1));
	} else if (cfg & RKPM_SLP_ARMOFF_PMUOFF) {
		pmu1_pwr_con &=
			~(BIT(RV1103B_PMU_PDPMU1_BYPASS) |
			  0);

		pmu0_pwr_con |=
			BIT(RV1103B_PMU_PWRMODE0_EN) |
			/* BIT(RV1103B_PMU1_PWR_BYPASS) | */
			BIT(RV1103B_PMU1_BUS_BYPASS) |
			BIT(RV1103B_PMU1_PWRGT_EN) |
			BIT(RV1103B_PMU1_BUS_IDLE_EN) |
			BIT(RV1103B_PMU1_BUS_AUTO) |
			0;

		/* pmu reset hold */
		/* except lpmcu */
		writel_relaxed(0xffff3fff, pmugrf_base + RV1103B_PMUGRF_SOC_CON(4));
		/* except lpmcu */
		writel_relaxed(0x007f007e, pmugrf_base + RV1103B_PMUGRF_SOC_CON(5));
		writel_relaxed(0xffff0000, pmugrf_base + RV1103B_PMUGRF_SOC_CON(6));

		/* resume from bootrom */
		writel_relaxed(BITS_WITH_WMASK(0, 0x3, 10),
			       pmusgrf_base + RV1103B_PMUSGRF_SOC_CON(1));
	}

	if (cfg & RKPM_SLP_LP_PR) {
		writel_relaxed(0, pmugrf_base + RV1103B_PMUGRF_OS_REG(2));
		writel_relaxed(0, pmugrf_base + RV1103B_PMUGRF_OS_REG(3));

		writel_relaxed(0xffffffff, pmugrf_base + RV1103B_PMUGRF_SOC_CON(4));
		writel_relaxed(0x00ff00ff, pmugrf_base + RV1103B_PMUGRF_SOC_CON(5));
		writel_relaxed(0xffffffff, pmugrf_base + RV1103B_PMUGRF_SOC_CON(6));

		pmu1_pwr_con &=
			~(BIT(RV1103B_PMU_DDR_BYPASS) |
			  0);
	}

	/* pmu count */
	writel_relaxed(clk_freq_khz * 4, pmu_base + RV1103B_PMU1_OSC_STABLE_CNT);
	writel_relaxed(clk_freq_khz * 6, pmu_base + RV1103B_PMU1_PMIC_STABLE_CNT);
	writel_relaxed(clk_freq_khz * 15, pmu_base + RV1103B_PMU1_SLEEP_CNT);

	/* Pmu's clk has switched to 24M back When pmu FSM counts
	 * the follow counters, so we should use 24M to calculate
	 * these counters.
	 */
	writel_relaxed(0, pmu_base + RV1103B_PMU1_WAKEUP_RST_CLR_CNT);
	writel_relaxed(1200, pmu_base + RV1103B_PMU1_PLL_LOCK_CNT);
	writel_relaxed(24000 * 2, pmu_base + RV1103B_PMU1_PWM_SWITCH_CNT);

	writel_relaxed(0, pmu_base + RV1103B_PMU2_SCU_STABLE_CNT);
	writel_relaxed(0, pmu_base + RV1103B_PMU2_SCU_PWRUP_CNT);
	writel_relaxed(0, pmu_base + RV1103B_PMU2_SCU_PWRDN_CNT);
	writel_relaxed(0, pmu_base + RV1103B_PMU2_SCU_VOLUP_CNT);
	writel_relaxed(0, pmu_base + RV1103B_PMU2_SCU_VOLDN_CNT);

	writel_relaxed(0x00010001, pmu_base + RV1103B_PMU1_INT_MASK_CON);
	writel_relaxed(WITH_16BITS_WMSK(pmu1_scu_con), pmu_base + RV1103B_PMU2_SCU_PWR_CON);
	writel_relaxed(0x003f003f, pmu_base + RV1103B_PMU2_CLUSTER_IDLE_CON);
	writel_relaxed(0xffff0000 | BIT(RV1103B_CPU_AUTO_INT_MSK),
		       pmu_base + RV1103B_PMU2_CPU_AUTO_PWR_CON);
	writel_relaxed(0xffff0000 | BIT(RV1103B_SCU_AUTO_INT_MSK),
		       pmu_base + RV1103B_PMU2_SCU_AUTO_PWR_CON);

	writel_relaxed(WITH_16BITS_WMSK(pmu1_cru_con[0]), pmu_base + RV1103B_PMU1_CRU_PWR_CON(0));
	writel_relaxed(WITH_16BITS_WMSK(pmu1_cru_con[1]), pmu_base + RV1103B_PMU1_CRU_PWR_CON(1));
	writel_relaxed(WITH_16BITS_WMSK(pmu2_bus_idle_con), pmu_base + RV1103B_PMU2_BUS_IDLE_CON);

	writel_relaxed(WITH_16BITS_WMSK(pmu1_ddr_con), pmu_base + RV1103B_PMU1_DDR_PWR_CON);
	writel_relaxed(WITH_16BITS_WMSK(pmu1_pll_con), pmu_base + RV1103B_PMU1_PLLPD_CON);
	writel_relaxed(pmu1_wkup_con, pmu_base + RV1103B_PMU1_WAKEUP_INT_CON);
	writel_relaxed(WITH_16BITS_WMSK(pmu1_pwr_con), pmu_base + RV1103B_PMU1_PWR_CON);

	writel_relaxed(WITH_16BITS_WMSK(pmu0_pwr_con), pmu_base + RV1103B_PMU0_PWR_CON);

#if RV1103B_WAKEUP_TO_SYSTEM_RESET
	writel_relaxed(0, pmugrf_base + RV1103B_PMUGRF_OS_REG(9));
	/* Use PMUGRF_OS_REG10 to save wakeup source */
	writel_relaxed(0, pmugrf_base + RV1103B_PMUGRF_OS_REG(10));
#else
	writel_relaxed(__pa_symbol(cpu_resume),
		       pmugrf_base + RV1103B_PMUGRF_OS_REG(9));
#endif
	suspend_workaround_timeout_wkup();
}

static void pmu_sleep_restore(void)
{
	ddr_data.pmu_wkup_int_st = readl_relaxed(pmu_base + RV1103B_PMU1_WAKEUP_INT_ST);
	ddr_data.gpio0_int_st = readl_relaxed(gpio_base[0] + RV1103B_GPIO_INT_STATUS);

	resume_workaround_timeout_wkup();

	if (pm_suspend_state == PM_SUSPEND_MEM_ULTRA)
		sleep_rsv_mem[0] = 0;

	writel_relaxed(0, pmugrf_base + RV1103B_PMUGRF_OS_REG(9));

	writel_relaxed(0xffff0000, pmu_base + RV1103B_PMU0_PWR_CON);
	writel_relaxed(0xffff0000, pmu_base + RV1103B_PMU0_INFO_TX_CON);

	writel_relaxed(0xffff0000, pmu_base + RV1103B_PMU1_INT_MASK_CON);
	writel_relaxed(0xffff0000, pmu_base + RV1103B_PMU2_SCU_PWR_CON);
	writel_relaxed(0xffff0000, pmu_base + RV1103B_PMU2_CLUSTER_IDLE_CON);
	writel_relaxed(0xffff0000, pmu_base + RV1103B_PMU2_CPU_AUTO_PWR_CON);
	writel_relaxed(0xffff0000, pmu_base + RV1103B_PMU2_SCU_AUTO_PWR_CON);

	writel_relaxed(0xffff0000, pmu_base + RV1103B_PMU1_CRU_PWR_CON(0));
	writel_relaxed(0xffff0000, pmu_base + RV1103B_PMU1_CRU_PWR_CON(1));
	writel_relaxed(0xffff0000, pmu_base + RV1103B_PMU2_BUS_IDLE_CON);

	writel_relaxed(0xffff0000, pmu_base + RV1103B_PMU1_DDR_PWR_CON);
	writel_relaxed(0xffff0000, pmu_base + RV1103B_PMU1_PLLPD_CON);
	writel_relaxed(0xffff0000, pmu_base + RV1103B_PMU1_WAKEUP_INT_CON);
	writel_relaxed(0xffff0000, pmu_base + RV1103B_PMU1_PWR_CON);

	/* pmu reset hold */
	writel_relaxed(WITH_16BITS_WMSK(ddr_data.pmugrf_soc_con4),
		       pmugrf_base + RV1103B_PMUGRF_SOC_CON(4));
	writel_relaxed(WITH_16BITS_WMSK(ddr_data.pmugrf_soc_con5),
		       pmugrf_base + RV1103B_PMUGRF_SOC_CON(5));
	writel_relaxed(WITH_16BITS_WMSK(ddr_data.pmugrf_soc_con6),
		       pmugrf_base + RV1103B_PMUGRF_SOC_CON(6));
}

static void soc_sleep_config(void)
{
	rkpm_printch('a');

	if (slp_cfg.mode_config & RKPM_SLP_PMU_PMUALIVE_32K)
		sleep_32k_config();
	rkpm_printch('b');

	ddr_sleep_config();
	rkpm_printch('c');

	pmu_sleep_config();
	rkpm_printch('d');
}

static void soc_sleep_restore(void)
{
	rkpm_printch('d');

	pmu_sleep_restore();
	rkpm_printch('c');

	ddr_sleep_config_restore();
	rkpm_printch('b');

	if (slp_cfg.mode_config & RKPM_SLP_PMU_PMUALIVE_32K)
		sleep_32k_config_restore();
	rkpm_printch('a');
}

static void plls_suspend(void)
{
}

static void plls_resume(void)
{
}

static void gpio0_set_iomux(u32 pin_id, u32 func)
{
	u32 sft = (pin_id % 4) << 2;
	u32 offset = (pin_id / 4) << 2;

	if (pin_id < 8)
		writel_relaxed(BITS_WITH_WMASK(func, 0xf, sft), ioc0_base + offset);
	else if (pin_id < 16)
		writel_relaxed(BITS_WITH_WMASK(func, 0xf, sft), ioc1_base + offset);
}

static void gpio0_set_pull(u32 pin_id, int pull)
{
	u32 sft = (pin_id % 8) << 1;

	if (pin_id < 8)
		writel_relaxed(BITS_WITH_WMASK(pull, 0x3, sft), ioc0_base + 0x200);
	else if (pin_id < 16)
		writel_relaxed(BITS_WITH_WMASK(pull, 0x3, sft), ioc1_base + 0x204);
}

static void gpio0_set_direct(u32 pin_id, int out)
{
	u32 sft = pin_id % 16;

	if (pin_id < 16)
		writel_relaxed(BITS_WITH_WMASK(out, 0x1, sft),
			       gpio_base[0] + RV1103B_GPIO_SWPORT_DDR_L);
	else
		writel_relaxed(BITS_WITH_WMASK(out, 0x1, sft),
			       gpio_base[0] + RV1103B_GPIO_SWPORT_DDR_H);
}

static void gpio0_set_lvl(u32 pin_id, int lvl)
{
	u32 sft = pin_id % 16;

	if (pin_id < 16)
		writel_relaxed(BITS_WITH_WMASK(lvl, 0x1, sft),
			       gpio_base[0] + RV1103B_GPIO_SWPORT_DR_L);
	else
		writel_relaxed(BITS_WITH_WMASK(lvl, 0x1, sft),
			       gpio_base[0] + RV1103B_GPIO_SWPORT_DR_H);
}

static void sleep_pin_config(void)
{
	u32 en_msk = slp_cfg.sleep_pin_config[0] & 0x3;
	u32 act_val = slp_cfg.sleep_pin_config[1] & 0x3;

	/* pwr0 sleep: gpio0_a3 */
	if (en_msk & RKPM_SLEEP_PIN0_EN) {
		writel_relaxed(BITS_WITH_WMASK(0, 0x3, 0),
			       pmugrf_base + RV1103B_PMUGRF_SOC_CON(1));
		writel_relaxed(BITS_WITH_WMASK(act_val & 0x1, 0x1, 4),
			       pmugrf_base + RV1103B_PMUGRF_SOC_CON(1));
		writel_relaxed(BITS_WITH_WMASK(1, 0xf, 12), ioc0_base + 0x0);
		dsb();
		writel_relaxed(BITS_WITH_WMASK(0, 0x3, 6), ioc0_base + 0x200);
	}

	/* pwr1 sleep: gpio_a4 */
	if (en_msk & RKPM_SLEEP_PIN1_EN) {
		writel_relaxed(BITS_WITH_WMASK(0, 0x3, 2),
			       pmugrf_base + RV1103B_PMUGRF_SOC_CON(1));
		writel_relaxed(BITS_WITH_WMASK((act_val >> 1) & 0x1, 0x1, 5),
			       pmugrf_base + RV1103B_PMUGRF_SOC_CON(1));
		writel_relaxed(BITS_WITH_WMASK(1, 0xf, 0), ioc0_base + 0x4);
		dsb();
		writel_relaxed(BITS_WITH_WMASK(0, 0x3, 8), ioc0_base + 0x200);
	}
}

static void gpio_config(void)
{
	u32 iomux, dir, lvl, pull, id;
	u32 cfg, i;

	if (pm_suspend_state == PM_SUSPEND_MEM_ULTRA)
		return;

	ddr_data.gpio0a_iomux_l = readl_relaxed(ioc0_base + 0);
	ddr_data.gpio0a_iomux_h = readl_relaxed(ioc0_base + 0x4);
	ddr_data.gpio0b_iomux_l = readl_relaxed(ioc1_base + 0x8);
	ddr_data.gpio0b_iomux_h = readl_relaxed(ioc1_base + 0xc);
	ddr_data.gpio0a_pull = readl_relaxed(ioc0_base + 0x200);
	ddr_data.gpio0b_pull = readl_relaxed(ioc1_base + 0x204);

	ddr_data.gpio0_ddr_l = readl_relaxed(gpio_base[0] + RV1103B_GPIO_SWPORT_DDR_L);
	ddr_data.gpio0_ddr_h = readl_relaxed(gpio_base[0] + RV1103B_GPIO_SWPORT_DDR_H);
	ddr_data.gpio0_dr_l = readl_relaxed(gpio_base[0] + RV1103B_GPIO_SWPORT_DR_L);
	ddr_data.gpio0_dr_h = readl_relaxed(gpio_base[0] + RV1103B_GPIO_SWPORT_DR_H);

	for (i = 0; i < slp_cfg.sleep_io_config_cnt; i++) {
		cfg = slp_cfg.sleep_io_config[i];
		iomux = RKPM_IO_CFG_GET_IOMUX(cfg);
		dir = RKPM_IO_CFG_GET_GPIO_DIR(cfg);
		lvl = RKPM_IO_CFG_GET_GPIO_LVL(cfg);
		pull = RKPM_IO_CFG_GET_PULL(cfg);
		id = RKPM_IO_CFG_GET_ID(cfg);

		if (iomux == RKPM_IO_CFG_IOMUX_GPIO_VAL) {
			if (dir == RKPM_IO_CFG_GPIO_DIR_OUTPUT_VAL)
				gpio0_set_lvl(id, lvl);

			gpio0_set_direct(id, dir);
		}

		gpio0_set_iomux(id, iomux);
		gpio0_set_pull(id, pull);
	}

	/* pmu_debug */
	if (slp_cfg.mode_config & RKPM_SLP_PMU_DBG) {
		writel_relaxed(0x01ff01ff, pmu_base + RV1103B_PMU0_INFO_TX_CON);
		writel_relaxed(BITS_WITH_WMASK(0x5, 0xf, 4), ioc1_base + 0x8); /* gpio0_b1 */
	}

	sleep_pin_config();
}

static void gpio_restore(void)
{
	if (pm_suspend_state == PM_SUSPEND_MEM_ULTRA)
		return;

	writel_relaxed(WITH_16BITS_WMSK(ddr_data.gpio0_ddr_l),
		       gpio_base[0] + RV1103B_GPIO_SWPORT_DDR_L);
	writel_relaxed(WITH_16BITS_WMSK(ddr_data.gpio0_ddr_h),
		       gpio_base[0] + RV1103B_GPIO_SWPORT_DDR_H);
	writel_relaxed(WITH_16BITS_WMSK(ddr_data.gpio0_dr_l),
		       gpio_base[0] + RV1103B_GPIO_SWPORT_DR_L);
	writel_relaxed(WITH_16BITS_WMSK(ddr_data.gpio0_dr_h),
		       gpio_base[0] + RV1103B_GPIO_SWPORT_DR_H);

	writel_relaxed(WITH_16BITS_WMSK(ddr_data.gpio0a_iomux_l), ioc0_base + 0);
	writel_relaxed(WITH_16BITS_WMSK(ddr_data.gpio0a_iomux_h), ioc0_base + 0x4);
	writel_relaxed(WITH_16BITS_WMSK(ddr_data.gpio0b_iomux_l), ioc1_base + 0x8);
	writel_relaxed(WITH_16BITS_WMSK(ddr_data.gpio0b_iomux_h), ioc1_base + 0xc);
	writel_relaxed(WITH_16BITS_WMSK(ddr_data.gpio0a_pull), ioc0_base + 0x200);
	writel_relaxed(WITH_16BITS_WMSK(ddr_data.gpio0b_pull), ioc1_base + 0x204);
}

static struct uart_debug_ctx debug_port_save;
static u32 cru_mode;

static void pvtpll_core_suspend(void)
{
	rkpm_reg_rgn_save(pvtpll_core_reg_rgns, ARRAY_SIZE(pvtpll_core_reg_rgns));
}

static void pvtpll_core_resume(void)
{
	rkpm_reg_rgn_restore(pvtpll_core_reg_rgns, ARRAY_SIZE(pvtpll_core_reg_rgns));
	rkpm_raw_udelay(1);
}

static void pvtpll_logic_suspend(void)
{
	rkpm_reg_rgn_save(pvtpll_logic_reg_rgns, ARRAY_SIZE(pvtpll_logic_reg_rgns));
}

static void pvtpll_logic_resume(void)
{
	rkpm_reg_rgn_restore(pvtpll_logic_reg_rgns, ARRAY_SIZE(pvtpll_logic_reg_rgns));
	rkpm_raw_udelay(1);
}

static void vd_core_regs_save(void)
{
	rkpm_printch('a');

	pvtpll_core_suspend();
	rkpm_printch('b');

	rkpm_reg_rgn_save(vd_core_reg_rgns, ARRAY_SIZE(vd_core_reg_rgns));
	rkpm_printch('c');

	gic400_save();
	rkpm_printch('d');
}

static void vd_core_regs_restore(void)
{
	u32 mode = readl_relaxed(cru_base + 0x280);

	rkpm_printch('a');

	gic400_restore();
	rkpm_printch('b');

	/* slow mode */
	writel_relaxed(0x00030000, cru_base + 0x280);
	rkpm_printch('c');

	pvtpll_core_resume();
	rkpm_printch('d');

	rkpm_reg_rgn_restore(vd_core_reg_rgns, ARRAY_SIZE(vd_core_reg_rgns));
	rkpm_printch('e');

	/* restore mode */
	writel_relaxed(WITH_16BITS_WMSK(mode), cru_base + 0x280);
	rkpm_printch('f');
}

static void vd_log_regs_save(void)
{
	cru_mode = readl_relaxed(cru_base + 0x280);

	rkpm_printch('a');

	pvtpll_logic_suspend();
	rkpm_printch('b');

	rkpm_reg_rgn_save(vd_log_reg_rgns, ARRAY_SIZE(vd_log_reg_rgns));
	rkpm_printch('c');

	rkpm_uart_debug_save(uartdbg_base, &debug_port_save);
	rkpm_printch('d');
}

static void vd_log_regs_restore(void)
{
	rkpm_printch('a');

	rkpm_uart_debug_restore(uartdbg_base, &debug_port_save);
	rkpm_printch('b');

	/* slow mode */
	writel_relaxed(0x00030000, cru_base + 0x280);
	rkpm_printch('c');

	pvtpll_logic_resume();
	rkpm_printch('d');

	rkpm_reg_rgn_restore(vd_log_reg_rgns, ARRAY_SIZE(vd_log_reg_rgns));
	rkpm_printch('e');

	/* wait lock */
	pm_pll_wait_lock(RV1103B_GPLL_ID);

	/* restore mode */
	writel_relaxed(WITH_16BITS_WMSK(cru_mode), cru_base + 0x280);
	rkpm_printch('f');

	writel_relaxed(0xffff0000, pmugrf_base + RV1103B_PMUGRF_SOC_CON(4));
	writel_relaxed(0xffff0000, pmugrf_base + RV1103B_PMUGRF_SOC_CON(5));
	writel_relaxed(0xffff0000, pmugrf_base + RV1103B_PMUGRF_SOC_CON(6));

	if (readl_relaxed(wdt_ns_base + RV1103B_WDT_CR) & 0x1)
		writel_relaxed(0x76, wdt_ns_base + RV1103B_WDT_CRR);

	if (readl_relaxed(wdt_s_base + RV1103B_WDT_CR) & 0x1)
		writel_relaxed(0x76, wdt_s_base + RV1103B_WDT_CRR);
}

static void pd_pmu1_regs_save(void)
{
	rkpm_printch('a');

	rkpm_reg_rgn_save(pd_pmu1_reg_rgns, ARRAY_SIZE(pd_pmu1_reg_rgns));
	rkpm_printch('b');
}

static void pd_pmu1_regs_restore(void)
{
	rkpm_printch('a');

	rkpm_reg_rgn_restore(pd_pmu1_reg_rgns, ARRAY_SIZE(pd_pmu1_reg_rgns));
	rkpm_printch('b');
}

static void pd_pmu0_regs_save(void)
{
	rkpm_printch('a');

	rkpm_reg_rgn_save(pd_pmu0_reg_rgns, ARRAY_SIZE(pd_pmu0_reg_rgns));
	rkpm_printch('b');
}

static void pd_pmu0_regs_restore(void)
{
	rkpm_printch('a');

	rkpm_bootdata_cpusp = RV1103B_PMUSRAM_BASE + (SZ_8K - 8);
	rkpm_bootdata_cpu_code = __pa_symbol(cpu_resume);
	rkpm_bootdata_l2ctlr_f = 1;
	rkpm_bootdata_l2ctlr = rv1103b_l2_config();

	/* copy resume code and data to pmusram */
	memcpy(pmusram_base, rockchip_slp_cpu_resume,
	       rv1103b_bootram_sz + BOOTRAM_RSV_SIZE);
	rkpm_printch('b');

	rkpm_reg_rgn_restore(pd_pmu0_reg_rgns, ARRAY_SIZE(pd_pmu0_reg_rgns));
	rkpm_printch('c');
}

static void hptimer_init(void)
{
	rk_hptimer_v2_clear_int_st(hptimer_base, RK_HPTIMER_V2_INT_SYNC);

	if (rk_hptimer_get_mode(hptimer_base) == RK_HPTIMER_HARD_ADJUST_MODE)
		return;

	/* deepslow select osc_div_32k */
	writel_relaxed(BITS_WITH_WMASK(0x0, 0x1, 2),
		       pmu0cru_base + RV1103B_PMU0CRU_CLKSEL_CON(0));
	writel_relaxed(BITS_WITH_WMASK(0x0, 0x3, 0),
		       pmu0cru_base + RV1103B_PMU0CRU_CLKSEL_CON(0));

	rk_hptimer_v2_mode_init(hptimer_base, RK_HPTIMER_HARD_ADJUST_MODE, 24000000);
}

static void hptimer_suspend(void)
{
	int mode = rk_hptimer_get_mode(hptimer_base);

	if ((slp_cfg.mode_config & RKPM_SLP_PMU_PMUALIVE_32K) &&
	    mode == RK_HPTIMER_SOFT_ADJUST_MODE)
		writel_relaxed(BITS_WITH_WMASK(0x1, 0x1, 8),
			       pmusgrf_base + RV1103B_PMUSGRF_SOC_CON(0));
}

static void hptimer_resume(void)
{
	int mode = rk_hptimer_get_mode(hptimer_base);

	writel_relaxed(BITS_WITH_WMASK(0x0, 0x1, 8),
		       pmusgrf_base + RV1103B_PMUSGRF_SOC_CON(0));

	if (slp_cfg.mode_config & RKPM_SLP_PMU_PMUALIVE_32K) {
		if (mode == RK_HPTIMER_HARD_ADJUST_MODE)
			rk_hptimer_v2_do_hard_adjust_no_wait(hptimer_base);
		else if (mode == RK_HPTIMER_SOFT_ADJUST_MODE)
			rk_hptimer_v2_do_soft_adjust_no_wait(hptimer_base,
							     24000000,
							     32768);
	}
}

static void rkpm_reg_rgns_init(void)
{
	rkpm_alloc_region_mem(vd_core_reg_rgns, ARRAY_SIZE(vd_core_reg_rgns));
	rkpm_alloc_region_mem(vd_log_reg_rgns, ARRAY_SIZE(vd_log_reg_rgns));
	rkpm_alloc_region_mem(pd_pmu1_reg_rgns, ARRAY_SIZE(pd_pmu1_reg_rgns));
	rkpm_alloc_region_mem(pd_pmu0_reg_rgns, ARRAY_SIZE(pd_pmu0_reg_rgns));
	rkpm_alloc_region_mem(pvtpll_core_reg_rgns, ARRAY_SIZE(pvtpll_core_reg_rgns));
	rkpm_alloc_region_mem(pvtpll_logic_reg_rgns, ARRAY_SIZE(pvtpll_logic_reg_rgns));
}

static void rkpm_regs_rgn_dump(void)
{
	return;

	rkpm_dump_reg_rgns(vd_core_reg_rgns, ARRAY_SIZE(vd_core_reg_rgns));
	rkpm_dump_reg_rgns(vd_log_reg_rgns, ARRAY_SIZE(vd_log_reg_rgns));
	rkpm_dump_reg_rgns(pd_pmu1_reg_rgns, ARRAY_SIZE(pd_pmu1_reg_rgns));
	rkpm_dump_reg_rgns(pd_pmu0_reg_rgns, ARRAY_SIZE(pd_pmu0_reg_rgns));
	rkpm_dump_reg_rgns(pvtpll_core_reg_rgns, ARRAY_SIZE(pvtpll_core_reg_rgns));
	rkpm_dump_reg_rgns(pvtpll_logic_reg_rgns, ARRAY_SIZE(pvtpll_logic_reg_rgns));
}

static int rockchip_lpmode_enter(unsigned long arg)
{
	flush_cache_all();

	cpu_do_idle();

#if RV1103B_WAKEUP_TO_SYSTEM_RESET
	/* If reaches here, it means wakeup source cames before cpu enter wfi.
	 * So we should do system reset if RV1103B_WAKEUP_TO_SYSTEM_RESET.
	 */
	writel_relaxed(0x000c000c, cru_base + RV1103B_CRU_GLB_RST_CON);
	writel_relaxed(0xffff0000, pmugrf_base + RV1103B_PMUGRF_SOC_CON(4));
	writel_relaxed(0xffff0000, pmugrf_base + RV1103B_PMUGRF_SOC_CON(5));
	writel_relaxed(0xffff0000, pmugrf_base + RV1103B_PMUGRF_SOC_CON(6));
	dsb(sy);
	writel_relaxed(0xfdb9, cru_base + RV1103B_CRU_GLB_SRST_FST);
	rkpm_power_down_wfi();
#endif

	rkpm_printstr("Failed to suspend\n");

	return 1;
}

static int rv1103b_suspend_enter(suspend_state_t state)
{
	const struct rk_sleep_config *config = rockchip_get_cur_sleep_config();
	u32 mode_cfg;

	if (config != NULL)
		memcpy(&slp_cfg, config, sizeof(slp_cfg));

	pm_suspend_state = mem_sleep_current;
	if (pm_suspend_state == PM_SUSPEND_MEM_ULTRA) {
		init_sleep_reserve_mem();
		slp_cfg.mode_config &= ~0x1f;
		writel_relaxed(0, pmu_base + RV1103B_PMU1_WAKEUP_TIMEOUT);
	}

	mode_cfg = slp_cfg.mode_config;

	rv1103b_dbg_sleep_enter_info();

	local_fiq_disable();

	rv1103b_dbg_irq_prepare();

	rkpm_printch('-');

	clock_suspend();
	rkpm_printch('0');

	soc_sleep_config();
	rkpm_printch('1');

	plls_suspend();
	rkpm_printch('2');

	gpio_config();
	rkpm_printch('3');

	vd_core_regs_save();
	rkpm_printch('4');

	if ((mode_cfg & (RKPM_SLP_ARMOFF_LOGOFF | RKPM_SLP_ARMOFF_PMUOFF)) ||
	    (pm_suspend_state == PM_SUSPEND_MEM_ULTRA))
		vd_log_regs_save();
	rkpm_printch('5');

	if ((mode_cfg & RKPM_SLP_ARMOFF_PMUOFF) ||
	    (pm_suspend_state == PM_SUSPEND_MEM_ULTRA))
		pd_pmu1_regs_save();
	rkpm_printch('6');

	hptimer_suspend();
	rkpm_printch('7');

	if (pm_suspend_state == PM_SUSPEND_MEM_ULTRA)
		pd_pmu0_regs_save();

	rkpm_regs_rgn_dump();

	rkpm_printstr("-WFI-");
	cpu_suspend(0, rockchip_lpmode_enter);

	if (pm_suspend_state == PM_SUSPEND_MEM_ULTRA)
		pd_pmu0_regs_restore();
	rkpm_printch('7');

	hptimer_resume();
	rkpm_printch('6');

	if ((mode_cfg & RKPM_SLP_ARMOFF_PMUOFF) ||
	    (pm_suspend_state == PM_SUSPEND_MEM_ULTRA))
		pd_pmu1_regs_restore();
	rkpm_printch('5');

	if ((mode_cfg & (RKPM_SLP_ARMOFF_LOGOFF | RKPM_SLP_ARMOFF_PMUOFF)) ||
	    (pm_suspend_state == PM_SUSPEND_MEM_ULTRA))
		vd_log_regs_restore();
	rkpm_printch('4');

	vd_core_regs_restore();
	rkpm_printch('3');
	rkpm_regs_rgn_dump();

	gpio_restore();
	rkpm_printch('2');

	plls_resume();
	rkpm_printch('1');

	soc_sleep_restore();
	rkpm_printch('0');

	if ((rk_hptimer_get_mode(hptimer_base) != RK_HPTIMER_NORM_MODE) &&
	    (slp_cfg.mode_config & RKPM_SLP_PMU_PMUALIVE_32K))
		rk_hptimer_v2_wait_sync(hptimer_base);

	clock_resume();
	rkpm_printch('-');

	fiq_glue_resume();

	rv1103b_dbg_irq_finish();

	local_fiq_enable();
	rkpm_printstr("exit sleep\n");

	return 0;
}

static void rv1103b_set_sleep_mode_default(void)
{
	slp_cfg.mode_config =
		RKPM_SLP_ARMOFF_LOGOFF |
		RKPM_SLP_32K_EXT |
		RKPM_SLP_PMU_PMUALIVE_32K |
		RKPM_SLP_PMU_DIS_OSC |
		/* RKPM_SLP_TIME_OUT_WKUP | */
		RKPM_SLP_PMU_DBG |
		0;

	slp_cfg.sleep_debug_en = 1;
	slp_cfg.wakeup_config =
		RKPM_GPIO0_WKUP_EN |
		/* RKPM_SYS_INT_WKUP_EN | */
		/* RKPM_AOV_WKUP_EN | */
		0;
}

static int __init rv1103b_suspend_init(void)
{
	void __iomem *dev_reg_base;

	dev_reg_base = ioremap(RV1103B_DEV_REG_BASE, RV1103B_DEV_REG_SIZE);
	if (dev_reg_base) {
		pr_info("%s map dev_reg 0x%x -> 0x%x\n",
			__func__, RV1103B_DEV_REG_BASE, (u32)dev_reg_base);
	} else {
		pr_err("%s: can't map dev_reg(0x%x)\n", __func__, RV1103B_DEV_REG_BASE);
		return -ENOMEM;
	}

	pericru_base = dev_reg_base + RV1103B_PERICRU_OFFSET;
	venccru_base = dev_reg_base + RV1103B_VENCCRU_OFFSET;
	npucru_base = dev_reg_base + RV1103B_NPUCRU_OFFSET;
	vicru_base = dev_reg_base + RV1103B_VICRU_OFFSET;
	corecru_base = dev_reg_base + RV1103B_CORECRU_OFFSET;
	cru_base = dev_reg_base + RV1103B_TOPCRU_OFFSET;
	pmu0cru_base = dev_reg_base + RV1103B_PMU0CRU_OFFSET;
	pmu1cru_base = dev_reg_base + RV1103B_PMU1CRU_OFFSET;

	npugrf_base = dev_reg_base + RV1103B_NPUGRF_OFFSET;
	coregrf_base = dev_reg_base + RV1103B_COREGRF_OFFSET;
	ddrc_base = dev_reg_base + RV1103B_DDRC_OFFSET;
	ddrgrf_base = dev_reg_base + RV1103B_DDRGRF_OFFSET;
	perigrf_base = dev_reg_base + RV1103B_PERIGRF_OFFSET;
	pmugrf_base = dev_reg_base + RV1103B_PMUGRF_OFFSET;

	ioc3_base = dev_reg_base + RV1103B_IOC3_OFFSET;
	ioc47_base = dev_reg_base + RV1103B_IOC47_OFFSET;
	ioc6_base = dev_reg_base + RV1103B_IOC6_OFFSET;
	ioc0_base = dev_reg_base + RV1103B_IOC0_OFFSET;
	ioc1_base = dev_reg_base + RV1103B_IOC1_OFFSET;

	gpio_base[0] = dev_reg_base + RV1103B_GPIO0_OFFSET;
	gpio_base[1] = dev_reg_base + RV1103B_GPIO1_OFFSET;
	gpio_base[2] = dev_reg_base + RV1103B_GPIO2_OFFSET;

	perisgrf_base = dev_reg_base + RV1103B_PERISGRF_OFFSET;
	pmusgrf_base = dev_reg_base + RV1103B_PMUSGRF_OFFSET;

	qos_cpu_base = dev_reg_base + 0x310000;
	qos_crypto_base = dev_reg_base + 0x320000;
	qos_dcf_base = dev_reg_base + 0x320100;
	qos_decom_base = dev_reg_base + 0x320200;
	qos_dma2ddr_base = dev_reg_base + 0x320300;
	qos_mac_base = dev_reg_base + 0x320400;
	qos_mcu_base = dev_reg_base + 0x320500;
	qos_rga2e_rd_base = dev_reg_base + 0x320600;
	qos_rga2e_wr_base = dev_reg_base + 0x320700;
	qos_rkdma_base = dev_reg_base + 0x320800;
	qos_sdmmc1_base = dev_reg_base + 0x320900;
	qos_usb_base = dev_reg_base + 0x320a00;
	qos_emmc_base = dev_reg_base + 0x330000;
	qos_fspi_base = dev_reg_base + 0x330100;
	qos_isp_base = dev_reg_base + 0x330200;
	qos_sdmmc0_base = dev_reg_base + 0x330300;
	qos_vicap_base = dev_reg_base + 0x330400;
	qos_npu_base = dev_reg_base + 0x340000;
	qos_rkvdec_base = dev_reg_base + 0x350000;
	qos_fspi_pmu_base = dev_reg_base + 0x360000;
	qos_lpmcu_base = dev_reg_base + 0x360100;
	qos_spi2ahb_base = dev_reg_base + 0x360200;

	gicd_base = dev_reg_base + RV1103B_GIC_OFFSET + 0x1000;
	gicc_base = dev_reg_base + RV1103B_GIC_OFFSET + 0x2000;

	pvtpll_core_base = dev_reg_base + RV1103B_PVTPLL_CORE_OFFSET;
	pvtpll_isp_base = dev_reg_base + RV1103B_PVTPLL_ISP_OFFSET;
	pvtpll_vepu_base = dev_reg_base + RV1103B_PVTPLL_VEPU_OFFSET;
	pvtpll_npu_base = dev_reg_base + RV1103B_PVTPLL_NPU_OFFSET;

	hptimer_base = dev_reg_base + RV1103B_HPTIMER_OFFSET;
	pmu_base = dev_reg_base + RV1103B_PMU_OFFSET;
	uartdbg_base = dev_reg_base + RV1103B_UART0_OFFSET;

	wdt_ns_base = dev_reg_base + RV1103B_WDTNS_OFFSET;
	wdt_s_base = dev_reg_base + RV1103B_WDTS_OFFSET;

	nstimer_base[0] = dev_reg_base + RV1103B_NSTIMER_OFFSET;
	nstimer_base[1] = dev_reg_base + RV1103B_NSTIMER_OFFSET + 0x1000;
	nstimer_base[2] = dev_reg_base + RV1103B_NSTIMER_OFFSET + 0x2000;
	nstimer_base[3] = dev_reg_base + RV1103B_NSTIMER_OFFSET + 0x3000;
	nstimer_base[4] = dev_reg_base + RV1103B_NSTIMER_OFFSET + 0x4000;
	nstimer_base[5] = dev_reg_base + RV1103B_NSTIMER_OFFSET + 0x5000;

	stimer_base[0] = dev_reg_base + RV1103B_STIMER_OFFSET;
	stimer_base[1] = dev_reg_base + RV1103B_STIMER_OFFSET + 0x1000;

	fw_ddr_base = dev_reg_base + RV1103B_FW_DDR_OFFSET;
	pmusram_base = dev_reg_base + RV1103B_PMUSRAM_OFFSET;

	hptimer_init();

	rv1103b_set_sleep_mode_default();

	rv1103b_config_bootdata();

	/* copy resume code and data to pmusram */
	memcpy(pmusram_base, rockchip_slp_cpu_resume,
	       rv1103b_bootram_sz + BOOTRAM_RSV_SIZE);

	/* biu auto con */
	writel_relaxed(0x003f003f, pmu_base + RV1103B_PMU2_NOC_AUTO_CON);

	/* gpio0_a3 activelow, gpio0_a4 active high, select sleep func */
	writel_relaxed(BITS_WITH_WMASK(0x10, 0x3f, 0),
		       pmugrf_base + RV1103B_PMUGRF_SOC_CON(1));

	/* PMU_WAKEUP_TIMEOUT_CNT = 0, disable TIMEOUT_WAKEUP by default */
	writel_relaxed(0x0, pmu_base + RV1103B_PMU1_WAKEUP_TIMEOUT);

	rkpm_region_mem_init(RV1103B_PM_REG_REGION_MEM_SIZE);
	rkpm_reg_rgns_init();

	return 0;
}

static const struct platform_suspend_ops rv1103b_suspend_ops = {
	.enter   = rv1103b_suspend_enter,
	.valid   = suspend_valid_only_mem,
};

void __init rockchip_suspend_init(void)
{
	rv1103b_suspend_init();

	suspend_set_ops(&rv1103b_suspend_ops);
}
