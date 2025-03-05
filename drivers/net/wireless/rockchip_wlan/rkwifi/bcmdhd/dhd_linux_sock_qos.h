/*
 * Header file for DHD TPA (Traffic Pattern Analyzer)
 *
 * Provides type definitions and function prototypes to call into
 * DHD's QOS on Socket Flow module.
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
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id$
 *
 */

#ifndef _DHD_LINUX_TPA_H_
#define _DHD_LINUX_TPA_H_

struct dhd_sock_flow_info;

/* Feature Disabled dummy implementations */

static inline int
dhd_init_sock_flows_buf(dhd_info_t *dhd, uint watchdog_ms)
{
		BCM_REFERENCE(dhd);
		return BCME_UNSUPPORTED;
}

static inline int
dhd_deinit_sock_flows_buf(dhd_info_t *dhd)
{
		BCM_REFERENCE(dhd);
		return BCME_UNSUPPORTED;
}

static inline void
dhd_update_sock_flows(dhd_info_t *dhd, struct sk_buff *skb)
{
		BCM_REFERENCE(dhd);
		BCM_REFERENCE(skb);
		return;
}

static inline void
dhd_analyze_sock_flows(dhd_info_t *dhd, uint32 watchdog_ms)
{
	BCM_REFERENCE(dhd);
	BCM_REFERENCE(dhd_watchdog_ms);
	return;
}

static inline void
dhd_sock_qos_update_bus_flowid(dhd_info_t *dhd, void *pktbuf,
	uint32 bus_flow_id)
{
	BCM_REFERENCE(dhd);
	BCM_REFERENCE(pktbuf);
	BCM_REFERENCE(bus_flow_id);
}

#endif /* _DHD_LINUX_TPA_H_ */
