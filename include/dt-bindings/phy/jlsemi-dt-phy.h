/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _DT_BINDINGS_JLSEMI_PHY_H
#define _DT_BINDINGS_JLSEMI_PHY_H

#define JL1XXX_LED0_100_LINK		(1 << 4)
#define JL1XXX_LED0_10_LINK		(1 << 5)
#define JL1XXX_LED1_100_ACTIVITY	(1 << 10)
#define JL1XXX_LED1_10_ACTIVITY		(1 << 11)
#define JL1XXX_LED_STATIC_OP_EN		(1 << 0)
#define JL1XXX_LED_MODE_EN		(1 << 1)

#define JL2XXX_LED1_LINK10		(1 << 5)
#define JL2XXX_LED1_LINK100		(1 << 6)
#define JL2XXX_LED1_LINK1000		(1 << 8)
#define JL2XXX_LED2_LINK10		(1 << 10)
#define JL2XXX_LED2_LINK100		(1 << 11)
#define JL2XXX_LED2_LINK1000		(1 << 13)
#define JL2XXX_LED2_ACTIVITY		(1 << 14)
#define JL2XXX_LED_STATIC_OP_EN		(1 << 0)

#endif
