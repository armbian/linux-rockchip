/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Header providing constants for Rockchip suspend bindings.
 *
 * Copyright (C) 2025, Rockchip Electronics Co., Ltd.
 * Author: ShengFei.Xu
 */

#ifndef __DT_BINDINGS_SUSPEND_ROCKCHIP_RK3538_H__
#define __DT_BINDINGS_SUSPEND_ROCKCHIP_RK3538_H__
/******************************bits ops************************************/

#ifndef BIT
#define BIT(nr)				(1 << (nr))
#endif

#define RKPM_SLP_NORMAL_MODE		BIT(0)
#define RKPM_SLP_DEEP1_MODE		BIT(1)
#define RKPM_SLP_DEEP2_MODE		BIT(2)
#define RKPM_SLP_ULTRA_MODE		BIT(3)
#define RKPM_SLP_FROM_UBOOT		BIT(4)
#define RKPM_SLP_PMIC_LP		BIT(5)
#define RKPM_SLP_HW_PLLS_OFF		BIT(6)
#define RKPM_SLP_PMUALIVE_32K		BIT(7)
#define RKPM_SLP_OSC_DIS		BIT(8)
#define RKPM_SLP_32K_EXT		BIT(9)
#define RKPM_SLP_32K_INNER		BIT(10)

/* the wake up source */
#define WAKEUP_CPU0_INT			BIT(0)
#define WAKEUP_CPU1_INT			BIT(1)
#define WAKEUP_CPU2_INT			BIT(2)
#define WAKEUP_CPU3_INT			BIT(3)
#define WAKEUP_GPIO0_INT		BIT(4)
#define WAKEUP_SDMMC0_INT		BIT(5)
#define WAKEUP_SDMMC1_INT		BIT(6)
#define WAKEUP_SDIO_INT			BIT(7)
#define WAKEUP_USB_INT			BIT(8)
#define WAKEUP_I2C0_INT			BIT(9)
#define WAKEUP_UART0_INT		BIT(10)
#define WAKEUP_PWM0_CH0_INT		BIT(11)
#define WAKEUP_PWM0_CH1_INT		BIT(12)
#define WAKEUP_PWM0_CH2_INT		BIT(13)
#define WAKEUP_PWM0_CH3_INT		BIT(14)
#define WAKEUP_TIMER_INT		BIT(15)
#define WAKEUP_LPTIMER_INT		BIT(16)
#define WAKEUP_SYS_INT			BIT(17)
#define WAKEUP_PMU_CEC_DET		BIT(18)
#define WAKEUP_PMU_HDMI_HP		BIT(19)
#define WAKEUP_HDMI_CEC			BIT(20)
#define WAKEUP_GMAC			BIT(21)
#define WAKEUP_TIMEOUT			BIT(22)

/* the pwm regulator */
#define RKPM_PWM0_REGULATOR_EN		BIT(0)

/* sleep pin */
#define RKPM_SLEEP_PIN0_EN		BIT(0) /* GPIO0_A2 */
#define RKPM_SLEEP_PIN1_EN		BIT(1) /* GPIO0_A3 */
#define RKPM_SLEEP_PIN2_EN		BIT(2) /* GPIO0_A4 */

#define RKPM_SLEEP_PIN0_ACT_LOW		BIT(0) /* GPIO0_A2 */
#define RKPM_SLEEP_PIN1_ACT_LOW		BIT(1) /* GPIO0_A3 */
#define RKPM_SLEEP_PIN2_ACT_LOW		BIT(2) /* GPIO0_A4 */

#endif
