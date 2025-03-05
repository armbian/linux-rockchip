/*
 * EVENT_LOG system definitions
 *
 * Copyright (C) 2022, Broadcom.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *
 * <<Broadcom-WL-IPTag/Dual:>>
 */

#ifndef _EVENT_LOG_SET_H_
#define _EVENT_LOG_SET_H_

/* Set assignments */
/* set 0 */
#define EVENT_LOG_SET_BUS		(0u)
#ifndef EVENT_LOG_SET_BUS_NUM_BLOCKS
#define EVENT_LOG_SET_BUS_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_BUS_BLOCK_SIZE
#define EVENT_LOG_SET_BUS_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_512B)
#endif

/* set 1 */
#define EVENT_LOG_SET_WL		(1u)
#ifndef EVENT_LOG_SET_WL_NUM_BLOCKS
#define EVENT_LOG_SET_WL_NUM_BLOCKS	(4u)
#endif

#ifndef EVENT_LOG_SET_WL_BLOCK_SIZE
#define EVENT_LOG_SET_WL_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_512B)
#endif

/* Set 2 */
#define EVENT_LOG_SET_PSM		(2u)
#ifndef EVENT_LOG_SET_PSM_NUM_BLOCKS
#define EVENT_LOG_SET_PSM_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_PSM_BLOCK_SIZE
#define EVENT_LOG_SET_PSM_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_512B)
#endif

/* set 3 */
#define EVENT_LOG_SET_ERROR		(3u)
#ifndef EVENT_LOG_SET_ERROR_NUM_BLOCKS
#define EVENT_LOG_SET_ERROR_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_ERROR_BLOCK_SIZE
#define EVENT_LOG_SET_ERROR_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_1K)
#endif

/* Set 4: MSCH logging */
#define EVENT_LOG_SET_MSCH_PROFILER	(4u)
#ifndef EVENT_LOG_SET_MSCH_NUM_BLOCKS
#define EVENT_LOG_SET_MSCH_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_MSCH_BLOCK_SIZE
/* Updated the size as per comment in the msch profiler attach code */
#define EVENT_LOG_SET_MSCH_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_1K)
#endif

/* A particular customer uses sets 5, 6, and 7. There is a request
 * to not name these log sets as that could limit their ability to
 * use different log sets in future.
 * Sets 5, 6, and 7 are instantiated by host
 * In such case, ecounters could be mapped to any set that host
 * configures. They may or may not use set 5.
 */
#define EVENT_LOG_SET_5			(5u)
#define EVENT_LOG_SET_ECOUNTERS		(EVENT_LOG_SET_5)
#define EVENT_LOG_SET_6			(6u)
#define EVENT_LOG_SET_7			(7u)

/* Temporary change to satisfy compilation across branches
 * Will be removed after checkin
 */
#define EVENT_LOG_SET_8			(8u)
#define EVENT_LOG_SET_PRSRV		(EVENT_LOG_SET_8)
#ifndef EVENT_LOG_SET_PRSRV_NUM_BLOCKS
#define EVENT_LOG_SET_PRSRV_NUM_BLOCKS	(3u)
#endif

#ifndef EVENT_LOG_SET_PRSRV_BLOCK_SIZE
#define EVENT_LOG_SET_PRSRV_BLOCK_SIZE	(EVENT_LOG_MAX_BLOCK_SIZE)
#endif

#define EVENT_LOG_SET_9			(9u)
/* General purpose preserve chatty.
 * EVENT_LOG_SET_PRSRV_CHATTY log set should not be used by FW as it is
 * used by customer host. FW should use EVENT_LOG_SET_GP_PRSRV_CHATTY
 * for general purpose preserve chatty logs.
 */
#define EVENT_LOG_SET_GP_PRSRV_CHATTY	(EVENT_LOG_SET_9)
#define EVENT_LOG_SET_PRSRV_CHATTY	(EVENT_LOG_SET_6)
/* Log set 9 */
#ifndef EVENT_LOG_SET_GP_PRSRV_CHATTY_NUM_BLOCKS
#define EVENT_LOG_SET_GP_PRSRV_CHATTY_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_GP_PRSRV_CHATTY_BLOCK_SIZE
#define EVENT_LOG_SET_GP_PRSRV_CHATTY_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_1K)
#endif

/* Set 10: BUS preserve */
#define EVENT_LOG_SET_PRSRV_BUS		(10u)
#ifndef EVENT_LOG_SET_PRSRV_BUS_NUM_BLOCKS
#define EVENT_LOG_SET_PRSRV_BUS_NUM_BLOCKS	(3u)
#endif

#ifndef EVENT_LOG_SET_PRSRV_BUS_BLOCK_SIZE
#define EVENT_LOG_SET_PRSRV_BUS_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_512B)
#endif

/* Set 11: WL preserve */
#define EVENT_LOG_SET_PRSRV_WL		(11u)
#ifndef EVENT_LOG_SET_PRSRV_WL_NUM_BLOCKS
#define EVENT_LOG_SET_PRSRV_WL_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_PRSRV_WL_BLOCK_SIZE
#define EVENT_LOG_SET_PRSRV_WL_BLOCK_SIZE	(EVENT_LOG_MAX_BLOCK_SIZE)
#endif

/* Set 12: Slotted BSS set */
#define EVENT_LOG_SET_WL_SLOTTED_BSS    (12u)
#ifndef EVENT_LOG_SET_WL_SLOTTED_BSS_NUM_BLOCKS
#define EVENT_LOG_SET_WL_SLOTTED_BSS_NUM_BLOCKS	(5u)
#endif

#ifndef EVENT_LOG_SET_WL_SLOTTED_BSS_BLOCK_SIZE
#define EVENT_LOG_SET_WL_SLOTTED_BSS_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_512B)
#endif

/* Set 13: PHY entity logging */
#define EVENT_LOG_SET_PHY		(13u)
#ifndef EVENT_LOG_SET_PHY_NUM_BLOCKS
#define EVENT_LOG_SET_PHY_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_PHY_BLOCK_SIZE
#define EVENT_LOG_SET_PHY_BLOCK_SIZE	(EVENT_LOG_MAX_BLOCK_SIZE)
#endif

/* Set 14: PHY preserve */
#define EVENT_LOG_SET_PRSRV_PHY		(14u)
/* PHY periodic */
/* Use the newer name in future */
#define EVENT_LOG_SET_PERIODIC_PHY	(14u)
#ifndef EVENT_LOG_SET_PHY_PERIODIC_NUM_BLOCKS
#define EVENT_LOG_SET_PHY_PERIODIC_NUM_BLOCKS	(4u)
#endif

#ifndef EVENT_LOG_SET_PHY_PERIODIC_BLOCK_SIZE
#define EVENT_LOG_SET_PHY_PERIODIC_BLOCK_SIZE	(EVENT_LOG_MAX_BLOCK_SIZE)
#endif

/* set 15: RTE entity */
#define EVENT_LOG_SET_RTE		(15u)
#ifndef EVENT_LOG_SET_RTE_NUM_BLOCKS
#define EVENT_LOG_SET_RTE_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_RTE_BLOCK_SIZE
#define EVENT_LOG_SET_RTE_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_512B)
#endif

/* set 16: Malloc and free logging */
#define EVENT_LOG_SET_MEM_API		(16u)
#ifndef EVENT_LOG_SET_MEM_API_NUM_BLOCKS
#define EVENT_LOG_SET_MEM_API_NUM_BLOCKS	(7u)
#endif

#ifndef EVENT_LOG_SET_MEM_API_BLOCK_SIZE
#define EVENT_LOG_SET_MEM_API_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_512B)
#endif

/* set 17: summary log set to hold various summaries i.e. channel switch, scan, etc */
#define EVENT_LOG_SET_SUMMARY	(17u)
#ifndef EVENT_LOG_SET_SUMMARY_NUM_BLOCKS
#define EVENT_LOG_SET_SUMMARY_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_SUMMARY_BLOCK_SIZE
#define EVENT_LOG_SET_SUMMARY_BLOCK_SIZE	(EVENT_LOG_MAX_BLOCK_SIZE)
#endif

/* three log sets for general debug purposes */
#define EVENT_LOG_SET_GENERAL_DBG_1	(18u)
#define EVENT_LOG_SET_GENERAL_DBG_2	(19u)
#define EVENT_LOG_SET_GENERAL_DBG_3	(20u)

/* Log sets for capturing power related logs. Note that these sets
 * are to be used across entire system and not just WL.
 */
/* set 21: power related logs */
#define EVENT_LOG_SET_POWER_1		(21u)
#ifndef EVENT_LOG_SET_POWER_1_NUM_BLOCKS
#define EVENT_LOG_SET_POWER_1_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_POWER_1_BLOCK_SIZE
#define EVENT_LOG_SET_POWER_1_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_1K)
#endif

/* set 22: power related logs */
#define EVENT_LOG_SET_POWER_2		(22u)
#ifndef EVENT_LOG_SET_POWER_2_NUM_BLOCKS
#define EVENT_LOG_SET_POWER_2_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_POWER_2_BLOCK_SIZE
#define EVENT_LOG_SET_POWER_2_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_1K)
#endif

/* Used for timestamp plotting, TS_LOG() */
#define EVENT_LOG_SET_TS_LOG		(23u)
#ifndef EVENT_LOG_SET_TS_LOG_NUM_BLOCKS
#define EVENT_LOG_SET_TS_LOG_NUM_BLOCKS	(4u)
#endif

#ifndef EVENT_LOG_SET_TS_LOG_BLOCK_SIZE
#define EVENT_LOG_SET_TS_LOG_BLOCK_SIZE		(EVENT_LOG_BLOCK_SIZE_512B)
#endif

/* set 24: BUS preserve chatty */
#define EVENT_LOG_SET_PRSRV_BUS_CHATTY	(24u)
#ifndef EVENT_LOG_SET_PRSRV_BUS_CHATTY_NUM_BLOCKS
#define EVENT_LOG_SET_PRSRV_BUS_CHATTY_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_PRSRV_BUS_CHATTY_BLOCK_SIZE
#define EVENT_LOG_SET_PRSRV_BUS_CHATTY_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_512B)
#endif

/* PRESERVE_PREIODIC_LOG_SET */
/* flush if host is in D0 at every period */
#define EVENT_LOG_SET_PRSV_PERIODIC	(25u)
#ifndef EVENT_LOG_SET_PRSRV_PERIODIC_NUM_BLOCKS
#define EVENT_LOG_SET_PRSRV_PERIODIC_NUM_BLOCKS		(3u)
#endif

#ifndef EVENT_LOG_SET_PRSRV_PERIODIC_BLOCK_SIZE
#define EVENT_LOG_SET_PRSRV_PERIODIC_BLOCK_SIZE		(EVENT_LOG_BLOCK_SIZE_512B)
#endif

/* set 26: AMT logging and other related information */
#define EVENT_LOG_SET_AMT		(26u)
#ifndef EVENT_LOG_SET_AMT_NUM_BLOCKS
#define EVENT_LOG_SET_AMT_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_AMT_BLOCK_SIZE
#define EVENT_LOG_SET_AMT_BLOCK_SIZE	(EVENT_LOG_MAX_BLOCK_SIZE)
#endif

/* set: 27 State machine logging. Part of preserve logs */
#define EVENT_LOG_SET_FSM		(27u)
#ifndef EVENT_LOG_SET_FSM_NUM_BLOCKS
#define EVENT_LOG_SET_FSM_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_FSM_BLOCK_SIZE
#define EVENT_LOG_SET_FSM_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_1K)
#endif

/* set: 28 wbus related logging */
#define EVENT_LOG_SET_WBUS		(28u)
#ifndef EVENT_LOG_SET_WBUS_NUM_BLOCKS
#define EVENT_LOG_SET_WBUS_NUM_BLOCKS	(3u)
#endif

#ifndef EVENT_LOG_SET_WBUS_BLOCK_SIZE
#define EVENT_LOG_SET_WBUS_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_256B)
#endif

/* set 29: bcm trace logging */
#define EVENT_LOG_SET_BCM_TRACE		(29u)
#ifndef EVENT_LOG_SET_BCM_TRACE_NUM_BLOCKS
#define EVENT_LOG_SET_BCM_TRACE_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_BCM_TRACE_BLOCK_SIZE
#define EVENT_LOG_SET_BCM_TRACE_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_1K)
#endif

/* set 30: For PM alert related logging */
#define EVENT_LOG_SET_WL_PS_LOG		(30u)
#ifndef EVENT_LOG_SET_WL_PS_NUM_BLOCKS
#define EVENT_LOG_SET_WL_PS_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_WL_PS_BLOCK_SIZE
#define EVENT_LOG_SET_WL_PS_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_1K)
#endif

/* set 31: For SIB co-ex logging */
#define EVENT_LOG_SET_WL_SIB_LOG	(31u)
#ifndef EVENT_LOG_SET_WL_SIB_LOG_NUM_BLOCKS
#define EVENT_LOG_SET_WL_SIB_LOG_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_WL_SIB_LOG_BLOCK_SIZE
#define EVENT_LOG_SET_WL_SIB_LOG_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_512B)
#endif

/* set 32: For EWP HW Init logging */
#define EVENT_LOG_SET_EWP_HW_INIT_LOG	(32u)
#ifndef EVENT_LOG_SET_EWP_HW_INIT_LOG_NUM_BLOCKS
#define EVENT_LOG_SET_EWP_HW_INIT_LOG_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_EWP_HW_INIT_LOG_BLOCK_SIZE
#define EVENT_LOG_SET_EWP_HW_INIT_LOG_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_512B)
#endif

#ifdef COEX_CPU
/* set 35: Shdow log sets for coex cpu */
#define EVENT_LOG_SET_COEX_SHADOW_INFO		(35u)
#ifndef EVENT_LOG_SET_COEX_SHADOW_INFO_NUM_BLOCKS
#define EVENT_LOG_SET_COEX_SHADOW_INFO_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_COEX_SHADOW_INFO_BLOCK_SIZE
#define EVENT_LOG_SET_COEX_SHADOW_INFO_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_512B)
#endif

/* set 36 */
#define EVENT_LOG_SET_COEX_SHADOW_ERR		(36u)
#ifndef EVENT_LOG_SET_COEX_SHADOW_ERR_NUM_BLOCKS
#define EVENT_LOG_SET_COEX_SHADOW_ERR_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_COEX_SHADOW_ERR_BLOCK_SIZE
#define EVENT_LOG_SET_COEX_SHADOW_ERR_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_512B)
#endif

/* TODO: Rename coex shadow set PRSRV to TIMELINE. The old name will be removed once changes in
 * all user components are submitted.
 */
/* set 37 */
#define EVENT_LOG_SET_COEX_SHADOW_PRSRV		(37u)
#define EVENT_LOG_SET_COEX_SHADOW_TIMELINE	(37u)

#ifndef EVENT_LOG_SET_COEX_SHADOW_TIMELINE_NUM_BLOCKS
#define EVENT_LOG_SET_COEX_SHADOW_TIMELINE_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_COEX_SHADOW_TIMELINE_BLOCK_SIZE
#define EVENT_LOG_SET_COEX_SHADOW_TIMELINE_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_512B)
#endif

#endif /* COEX_CPU */

/* set 38: For all BCM HAL logging. */
#define EVENT_LOG_SET_BCMHAL		(38u)
#ifndef EVENT_LOG_SET_BCMHAL_NUM_BLOCKS
#define EVENT_LOG_SET_BCMHAL_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_BCMHAL_BLOCK_SIZE
#define EVENT_LOG_SET_BCMHAL_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_512B)
#endif

#define EVENT_LOG_SET_OBP_TRACE_LOG	(39u)
#ifndef EVENT_LOG_SET_OBP_TRACE_LOG_NUM_BLOCKS
#define EVENT_LOG_SET_OBP_TRACE_LOG_NUM_BLOCKS	(2u)
#endif

#ifndef EVENT_LOG_SET_OBP_TRACE_LOG_BLOCK_SIZE
#define EVENT_LOG_SET_OBP_TRACE_LOG_BLOCK_SIZE	(EVENT_LOG_BLOCK_SIZE_1K)
#endif

#ifndef NUM_EVENT_LOG_SETS
/* Set a maximum number of sets here.  It is not dynamic for
 * efficiency of the EVENT_LOG calls. Old branches could define
 * this to an appropriat enumber in their makefiles to reduce
 * ROM invalidation
 */
#ifdef NUM_EVENT_LOG_SETS_V2
/* for v2, everything has became unsigned */
#define NUM_EVENT_LOG_SETS (40u)
#else /* NUM_EVENT_LOG_SETS_V2 */
#define NUM_EVENT_LOG_SETS (40)
#endif /* NUM_EVENT_LOG_SETS_V2 */
#endif /* NUM_EVENT_LOG_SETS */

/* send delayed logs when >= 50% of buffer is full */
#ifndef ECOUNTERS_DELAYED_FLUSH_PERCENTAGE
#define ECOUNTERS_DELAYED_FLUSH_PERCENTAGE	(50)
#endif

#endif /* _EVENT_LOG_SET_H_ */
