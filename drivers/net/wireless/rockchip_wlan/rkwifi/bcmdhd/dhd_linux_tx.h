/*
 * Broadcom Dongle Host Driver (DHD),
 * Linux-specific network interface for transmit(tx) path
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
 */

#ifndef _dhd_linux_tx_h_
#define _dhd_linux_tx_h_
void dhd_tx_stop_queues(struct net_device *net);
void dhd_tx_start_queues(struct net_device *net);
int BCMFASTPATH(__dhd_sendpkt)(dhd_pub_t *dhdp, int ifidx, void *pktbuf);
int BCMFASTPATH(dhd_sendpkt)(dhd_pub_t *dhdp, int ifidx, void *pktbuf);
#ifdef DHD_MQ
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
uint16
BCMFASTPATH(dhd_select_queue)(struct net_device *net, struct sk_buff *skb,
       void *accel_priv, select_queue_fallback_t fallback);
#else
uint16
BCMFASTPATH(dhd_select_queue)(struct net_device *net, struct sk_buff *skb);
#endif /* LINUX_VERSION_CODE */
#endif /* DHD_MQ */
netdev_tx_t BCMFASTPATH(dhd_start_xmit)(struct sk_buff *skb, struct net_device *net);
void dhd_txflowcontrol(dhd_pub_t *dhdp, int ifidx, bool state);
void dhd_txcomplete(dhd_pub_t *dhdp, void *txp, bool success);
void dhd_os_tx_completion_wake(dhd_pub_t *dhd);
void dhd_os_sdlock_txq(dhd_pub_t *pub);
void dhd_os_sdunlock_txq(dhd_pub_t *pub);
#endif /* _dhd_linux_tx_h_ */
