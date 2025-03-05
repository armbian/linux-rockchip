/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Header providing constants for Rockchip suspend bindings.
 *
 * Copyright (C) 2024, Rockchip Electronics Co., Ltd.
 */

#ifndef __DT_BINDINGS_RK3506_PM_H__
#define __DT_BINDINGS_RK3506_PM_H__

/******************************bits ops************************************/
#ifndef BIT
#define BIT(nr)                          (1 << (nr))
#endif
#define PWREN_VALID_MASK                 0x80 /* bit7 */
#define PWREN_ACT_LOW_MASK               0x40 /* bit6 */
#define PWREN_ACT_HIGH_MASK              0x00 /* bit6 */
#define PIN0(n)                          (((n) | PWREN_VALID_MASK) << 0)
#define PIN8(n)                          (((n) | PWREN_VALID_MASK) << 8)

/*
 *	RK3506 system suspend mode configure definitions.
 *
 * Driver:
 *	These configures are pass to ATF by SMC in:
 *	drivers/soc/rockchip/rockchip_pm_config.c
 *
 * DTS:
 *	rockchip_suspend: rockchip-suspend {
 *		rockchip,sleep-mode-config = <...>;
 *		rockchip,wakeup-config = <...>;
 *		rockchip,apios-suspend = <...>;
 *		rockchip,sleep-debug-en = <...>;
 *	};
 */

/*
 * Suspend mode:
 *	rockchip,sleep-mode-config = <...>;
 */
#define RKPM_ARMPD                       BIT(0)
#define RKPM_ARMOFF                      BIT(1)
#define RKPM_ARMOFF_DDRPD                BIT(2)
#define RKPM_ARMOFF_LOGOFF               BIT(3)
#define RKPM_24M_OSC_DIS                 BIT(4)
#define RKPM_32K_CLK                     BIT(5)
#define RKPM_32K_SRC_OSC                 BIT(6)
#define RKPM_32K_SRC_RC                  BIT(7)
#define RKPM_32K_SRC_24XIN               BIT(8)
#define RKPM_PWM0_CH0_REGULATOR          BIT(9)
#define RKPM_PWM0_SRC_24M_EN             BIT(10)
#define RKPM_PWM0_SRC_PLL_EN             BIT(11)
#define RKPM_LOG_CLK_ALIVE               BIT(12)
#define RKPM_GPLL_ALIVE                  BIT(13)
#define RKPM_V0PLL_ALIVE                 BIT(14)
#define RKPM_V1PLL_ALIVE                 BIT(15)
#define RKPM_GPIO4_IE_DIS                BIT(16)
#define RKPM_PMU_BUS_CLK_HS              BIT(17)
#define RKPM_PWM0_CH0_CORE_PWREN         BIT(18)
/*
 * Wakeup source:
 *	rockchip,wakeup-config = <...>;
 */
#define RKPM_ARM_WAKEUP_EN               BIT(0)
#define RKPM_M0_WAKEUP_EN                BIT(1)
#define RKPM_HPTIMER_WAKEUP_EN           BIT(2)
#define RKPM_GPIO0_WAKEUP_EN             BIT(3)
#define RKPM_GPIO1_SHADOW_WAKEUP_EN      BIT(4)
#define RKPM_GPIO2_WAKEUP_EN             BIT(5)
#define RKPM_GPIO3_WAKEUP_EN             BIT(6)
#define RKPM_GPIO4_WAKEUP_EN             BIT(7)
#define RKPM_PWM0_WAKEUP_EN              BIT(8)
#define RKPM_PWM1_WAKEUP_EN              BIT(9)
#define RKPM_PWM2_WAKEUP_EN              BIT(10)
#define RKPM_UART0_WAKEUP_EN             BIT(11)
#define RKPM_UART1_WAKEUP_EN             BIT(12)
#define RKPM_UART2_WAKEUP_EN             BIT(13)
#define RKPM_UART3_WAKEUP_EN             BIT(14)
#define RKPM_UART4_WAKEUP_EN             BIT(15)
#define RKPM_UART5_WAKEUP_EN             BIT(16)
#define RKPM_USB0_DET_WAKEUP_EN          BIT(17)
#define RKPM_USB1_DET_WAKEUP_EN          BIT(18)
#define RKPM_MAC0_WAKEUP_EN              BIT(19)
#define RKPM_MAC1_WAKEUP_EN              BIT(20)
#define RKPM_TIMER0_WAKEUP_EN            BIT(21)
#define RKPM_TIMER1_WAKEUP_EN            BIT(22)
#define RKPM_CAN0_WAKEUP_EN              BIT(23)
#define RKPM_CAN1_WAKEUP_EN              BIT(24)
#define RKPM_TOUCH_KEY_WAKEUP_EN         BIT(25)
#define RKPM_SARADC_WAKUP_EN             BIT(26)
#define RKPM_ALARM_WAKEUP_EN             BIT(27)
#define RKPM_SECOND_CATCH_WAKEUP_EN      BIT(28)
#define RKPM_PMU_TIMEOUT_EN              BIT(29)

/*
 * Power control:
 *	rockchip,apios-suspend = <...>;
 */
#define RKPM_PWREN_SLEEP_ACT_LOW         PIN0(PWREN_ACT_LOW_MASK)
#define RKPM_PWREN_SLEEP_ACT_HIGH        PIN0(PWREN_ACT_HIGH_MASK)
#define RKPM_PWREN_SLEEP_GPIO0A0         PIN0(0)
#define RKPM_PWREN_SLEEP_GPIO0A1         PIN0(1)
#define RKPM_PWREN_SLEEP_GPIO0A2         PIN0(2)
#define RKPM_PWREN_SLEEP_GPIO0A3         PIN0(3)
#define RKPM_PWREN_SLEEP_GPIO0A4         PIN0(4)
#define RKPM_PWREN_SLEEP_GPIO0A5         PIN0(5)
#define RKPM_PWREN_SLEEP_GPIO0A6         PIN0(6)
#define RKPM_PWREN_SLEEP_GPIO0A7         PIN0(7)
#define RKPM_PWREN_SLEEP_GPIO0B0         PIN0(8)
#define RKPM_PWREN_SLEEP_GPIO0B1         PIN0(9)
#define RKPM_PWREN_SLEEP_GPIO0B2         PIN0(10)
#define RKPM_PWREN_SLEEP_GPIO0B3         PIN0(11)
#define RKPM_PWREN_SLEEP_GPIO0B4         PIN0(12)
#define RKPM_PWREN_SLEEP_GPIO0B5         PIN0(13)
#define RKPM_PWREN_SLEEP_GPIO0B6         PIN0(14)
#define RKPM_PWREN_SLEEP_GPIO0B7         PIN0(15)
#define RKPM_PWREN_SLEEP_GPIO0C0         PIN0(16)
#define RKPM_PWREN_SLEEP_GPIO0C1         PIN0(17)
#define RKPM_PWREN_SLEEP_GPIO0C2         PIN0(18)
#define RKPM_PWREN_SLEEP_GPIO0C3         PIN0(19)
#define RKPM_PWREN_SLEEP_GPIO0C4         PIN0(20)
#define RKPM_PWREN_SLEEP_GPIO0C5         PIN0(21)
#define RKPM_PWREN_SLEEP_GPIO0C6         PIN0(22)
#define RKPM_PWREN_SLEEP_GPIO0C7         PIN0(23)
#define RKPM_PWREN_SLEEP_GPIO1B1         PIN0(24)
#define RKPM_PWREN_SLEEP_GPIO1B2         PIN0(25)
#define RKPM_PWREN_SLEEP_GPIO1B3         PIN0(26)
#define RKPM_PWREN_SLEEP_GPIO1C2         PIN0(27)
#define RKPM_PWREN_SLEEP_GPIO1C3         PIN0(28)
#define RKPM_PWREN_SLEEP_GPIO1D1         PIN0(29)
#define RKPM_PWREN_SLEEP_GPIO1D2         PIN0(30)
#define RKPM_PWREN_SLEEP_GPIO1D3         PIN0(31)

#define RKPM_PWREN_CORE_ACT_LOW          PIN8(PWREN_ACT_LOW_MASK)
#define RKPM_PWREN_CORE_ACT_HIGH         PIN8(PWREN_ACT_HIGH_MASK)
#define RKPM_PWREN_CORE_GPIO0A0          PIN8(0)
#define RKPM_PWREN_CORE_GPIO0A1          PIN8(1)
#define RKPM_PWREN_CORE_GPIO0A2          PIN8(2)
#define RKPM_PWREN_CORE_GPIO0A3          PIN8(3)
#define RKPM_PWREN_CORE_GPIO0A4          PIN8(4)
#define RKPM_PWREN_CORE_GPIO0A5          PIN8(5)
#define RKPM_PWREN_CORE_GPIO0A6          PIN8(6)
#define RKPM_PWREN_CORE_GPIO0A7          PIN8(7)
#define RKPM_PWREN_CORE_GPIO0B0          PIN8(8)
#define RKPM_PWREN_CORE_GPIO0B1          PIN8(9)
#define RKPM_PWREN_CORE_GPIO0B2          PIN8(10)
#define RKPM_PWREN_CORE_GPIO0B3          PIN8(11)
#define RKPM_PWREN_CORE_GPIO0B4          PIN8(12)
#define RKPM_PWREN_CORE_GPIO0B5          PIN8(13)
#define RKPM_PWREN_CORE_GPIO0B6          PIN8(14)
#define RKPM_PWREN_CORE_GPIO0B7          PIN8(15)
#define RKPM_PWREN_CORE_GPIO0C0          PIN8(16)
#define RKPM_PWREN_CORE_GPIO0C1          PIN8(17)
#define RKPM_PWREN_CORE_GPIO0C2          PIN8(18)
#define RKPM_PWREN_CORE_GPIO0C3          PIN8(19)
#define RKPM_PWREN_CORE_GPIO0C4          PIN8(20)
#define RKPM_PWREN_CORE_GPIO0C5          PIN8(21)
#define RKPM_PWREN_CORE_GPIO0C6          PIN8(22)
#define RKPM_PWREN_CORE_GPIO0C7          PIN8(23)
#define RKPM_PWREN_CORE_GPIO1B1          PIN8(24)
#define RKPM_PWREN_CORE_GPIO1B2          PIN8(25)
#define RKPM_PWREN_CORE_GPIO1B3          PIN8(26)
#define RKPM_PWREN_CORE_GPIO1C2          PIN8(27)
#define RKPM_PWREN_CORE_GPIO1C3          PIN8(28)
#define RKPM_PWREN_CORE_GPIO1D1          PIN8(29)
#define RKPM_PWREN_CORE_GPIO1D2          PIN8(30)
#define RKPM_PWREN_CORE_GPIO1D3          PIN8(31)

/*
 * Debug control:
 *	rockchip,sleep-debug-en = <...>;
 */
#define RKPM_DBG_CLK_UNGATE              BIT(0)
#define RKPM_DBG_PLLS_ALIVE              BIT(1)
#define RKPM_DBG_PMU_SOUT                BIT(2)
#define RKPM_DBG_REG1                    BIT(3)
#define RKPM_DBG_REG2                    BIT(4)
#define RKPM_DBG_DEEP_JTAG               BIT(5)
#define RKPM_DBG_JTAG                    BIT(6)
#define RKPM_DBG_TIMEOUT_S(i)            (((i) & 0xf) << 7)
#define RKPM_DBG_DUMMY_WFI               BIT(11)
#define RKPM_DBG_DUMMY_DELAY1            BIT(12)
#define RKPM_DBG_DUMMY_DELAY2            BIT(13)
#define RKPM_DBG_PERI_REG                BIT(14)

#endif
