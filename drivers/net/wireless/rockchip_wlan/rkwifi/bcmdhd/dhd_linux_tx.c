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

#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>
#ifdef SHOW_LOGTRACE
#include <linux/syscalls.h>
#include <event_log.h>
#endif /* SHOW_LOGTRACE */

#ifdef PCIE_FULL_DONGLE
#include <bcmmsgbuf.h>
#endif /* PCIE_FULL_DONGLE */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/rtnetlink.h>
#include <linux/etherdevice.h>
#include <linux/random.h>
#include <linux/spinlock.h>
#include <linux/ethtool.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/ip.h>
#include <linux/reboot.h>
#include <linux/notifier.h>
#include <linux/irq.h>
#if defined(CONFIG_TIZEN)
#include <linux/net_stat_tizen.h>
#endif /* CONFIG_TIZEN */
#include <net/addrconf.h>
#ifdef ENABLE_ADAPTIVE_SCHED
#include <linux/cpufreq.h>
#endif /* ENABLE_ADAPTIVE_SCHED */
#include <linux/rtc.h>
#include <linux/namei.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>
#include <dhd_linux_priv.h>

#include <epivers.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmdevs.h>
#include <bcmdevs_legacy.h>    /* need to still support chips no longer in trunk firmware */
#include <bcmiov.h>
#include <bcmstdlib_s.h>

#include <ethernet.h>
#include <bcmevent.h>
#include <vlan.h>
#include <802.3.h>

#include <dhd_linux_wq.h>
#include <dhd.h>
#include <dhd_linux.h>
#include <dhd_linux_pktdump.h>
#ifdef DHD_WET
#include <dhd_wet.h>
#endif /* DHD_WET */
#ifdef PCIE_FULL_DONGLE
#include <dhd_flowring.h>
#endif
#include <dhd_bus.h>
#include <dhd_proto.h>
#include <dhd_dbg.h>
#include <dhd_dbg_ring.h>
#include <dhd_debug.h>
#if defined(WL_CFG80211)
#include <wl_cfg80211.h>
#ifdef WL_BAM
#include <wl_bam.h>
#endif	/* WL_BAM */
#endif	/* WL_CFG80211 */
#ifdef PNO_SUPPORT
#include <dhd_pno.h>
#endif
#ifdef RTT_SUPPORT
#include <dhd_rtt.h>
#endif

#include <dhd_linux_sock_qos.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#ifdef DHD_WMF
#include <dhd_wmf_linux.h>
#endif /* DHD_WMF */

#ifdef DHD_L2_FILTER
#include <bcmicmp.h>
#include <bcm_l2_filter.h>
#include <dhd_l2_filter.h>
#endif /* DHD_L2_FILTER */

#ifdef DHD_PSTA
#include <dhd_psta.h>
#endif /* DHD_PSTA */

#ifdef AMPDU_VO_ENABLE
/* XXX: Enabling VO AMPDU to reduce FER */
#include <802.1d.h>
#endif /* AMPDU_VO_ENABLE */

#if defined(DHDTCPACK_SUPPRESS) || defined(DHDTCPSYNC_FLOOD_BLK)
#include <dhd_ip.h>
#endif /* DHDTCPACK_SUPPRESS || DHDTCPSYNC_FLOOD_BLK */
#include <dhd_daemon.h>
#ifdef DHD_PKT_LOGGING
#include <dhd_pktlog.h>
#endif /* DHD_PKT_LOGGING */
#ifdef DHD_4WAYM4_FAIL_DISCONNECT
#include <eapol.h>
#endif /* DHD_4WAYM4_FAIL_DISCONNECT */

#ifdef ENABLE_DHD_GRO
#include <net/sch_generic.h>
#endif /* ENABLE_DHD_GRO */

#ifdef FIX_CPU_MIN_CLOCK
#include <linux/pm_qos.h>
#endif /* FIX_CPU_MIN_CLOCK */

#ifdef PROP_TXSTATUS
#include <wlfc_proto.h>
#include <dhd_wlfc.h>
#endif

#if defined(OEM_ANDROID)
#include <wl_android.h>
#endif
#include <dhd_config.h>

#define WME_PRIO2AC(prio)  wme_fifo2ac[prio2fifo[(prio)]]

void
dhd_tx_stop_queues(struct net_device *net)
{
#ifdef DHD_MQ
	netif_tx_stop_all_queues(net);
#else
	netif_stop_queue(net);
#endif
}

void
dhd_tx_start_queues(struct net_device *net)
{
#ifdef DHD_MQ
	netif_tx_wake_all_queues(net);
#else
	netif_wake_queue(net);
#endif
}

int
BCMFASTPATH(__dhd_sendpkt)(dhd_pub_t *dhdp, int ifidx, void *pktbuf)
{
	int ret = BCME_OK;
	dhd_info_t *dhd = (dhd_info_t *)(dhdp->info);
	struct ether_header *eh = NULL;
	uint8 pkt_flow_prio;
	dhd_if_t *ifp;
	unsigned long flags;
	uint datalen;

	DHD_GENERAL_LOCK(dhdp, flags);
	ifp = dhd_get_ifp(dhdp, ifidx);
	if (!ifp || ifp->del_in_progress) {
		DHD_ERROR(("%s: ifp:%p del_in_progress:%d\n",
			__FUNCTION__, ifp, ifp ? ifp->del_in_progress : 0));
		DHD_GENERAL_UNLOCK(dhdp, flags);
		PKTCFREE(dhdp->osh, pktbuf, TRUE);
		return -ENODEV;
	}
	DHD_GENERAL_UNLOCK(dhdp, flags);

	/* Reject if down */
	if (!dhdp->up || (dhdp->busstate == DHD_BUS_DOWN)) {
		/* free the packet here since the caller won't */
		PKTCFREE(dhdp->osh, pktbuf, TRUE);
		return -ENODEV;
	}

#ifdef PCIE_FULL_DONGLE
	if (dhdp->busstate == DHD_BUS_SUSPEND) {
		DHD_ERROR(("%s : pcie is still in suspend state!!\n", __FUNCTION__));
		PKTCFREE(dhdp->osh, pktbuf, TRUE);
		return NETDEV_TX_BUSY;
	}
#endif /* PCIE_FULL_DONGLE */

	/* Reject if pktlen > MAX_MTU_SZ */
	if (PKTLEN(dhdp->osh, pktbuf) > MAX_MTU_SZ) {
		/* free the packet here since the caller won't */
		dhdp->tx_big_packets++;
		PKTCFREE(dhdp->osh, pktbuf, TRUE);
		return BCME_ERROR;
	}

	datalen = PKTLEN(dhdp->osh, pktbuf);

#ifdef DHD_L2_FILTER
	/* if dhcp_unicast is enabled, we need to convert the */
	/* broadcast DHCP ACK/REPLY packets to Unicast. */
	if (ifp->dhcp_unicast) {
	    uint8* mac_addr;
	    uint8* ehptr = NULL;
	    int ret;
	    ret = bcm_l2_filter_get_mac_addr_dhcp_pkt(dhdp->osh, pktbuf, ifidx, &mac_addr);
	    if (ret == BCME_OK) {
		/*  if given mac address having valid entry in sta list
		 *  copy the given mac address, and return with BCME_OK
		*/
		if (dhd_find_sta(dhdp, ifidx, mac_addr)) {
		    ehptr = PKTDATA(dhdp->osh, pktbuf);
		    bcopy(mac_addr, ehptr + ETHER_DEST_OFFSET, ETHER_ADDR_LEN);
		}
	    }
	}

	if (ifp->grat_arp && DHD_IF_ROLE_AP(dhdp, ifidx)) {
	    if (bcm_l2_filter_gratuitous_arp(dhdp->osh, pktbuf) == BCME_OK) {
			PKTCFREE(dhdp->osh, pktbuf, TRUE);
			return BCME_ERROR;
	    }
	}

	if (ifp->parp_enable && DHD_IF_ROLE_AP(dhdp, ifidx)) {
		ret = dhd_l2_filter_pkt_handle(dhdp, ifidx, pktbuf, TRUE);

		/* Drop the packets if l2 filter has processed it already
		 * otherwise continue with the normal path
		 */
		if (ret == BCME_OK) {
			PKTCFREE(dhdp->osh, pktbuf, TRUE);
			return BCME_ERROR;
		}
	}
#endif /* DHD_L2_FILTER */
	/* Update multicast statistic */
	if (PKTLEN(dhdp->osh, pktbuf) >= ETHER_HDR_LEN) {
		uint8 *pktdata = (uint8 *)PKTDATA(dhdp->osh, pktbuf);
		eh = (struct ether_header *)pktdata;

		if (ETHER_ISMULTI(eh->ether_dhost))
			dhdp->tx_multicast++;
		if (ntoh16(eh->ether_type) == ETHER_TYPE_802_1X) {
#ifdef DHD_LOSSLESS_ROAMING
			uint8 prio = (uint8)PKTPRIO(pktbuf);

			/* back up 802.1x's priority */
			dhdp->prio_8021x = prio;
#endif /* DHD_LOSSLESS_ROAMING */
			DBG_EVENT_LOG(dhdp, WIFI_EVENT_DRIVER_EAPOL_FRAME_TRANSMIT_REQUESTED);
			atomic_inc(&dhd->pend_8021x_cnt);
#if defined(WL_CFG80211) && defined(WL_WPS_SYNC)
			wl_handle_wps_states(dhd_idx2net(dhdp, ifidx),
				pktdata, PKTLEN(dhdp->osh, pktbuf), TRUE);
#endif /* WL_CFG80211 && WL_WPS_SYNC */
#ifdef EAPOL_RESEND
			wl_ext_backup_eapol_txpkt(dhdp, ifidx, pktbuf);
#endif /* EAPOL_RESEND */
		}
		dhd_dump_pkt(dhdp, ifidx, pktdata,
			(uint32)PKTLEN(dhdp->osh, pktbuf), TRUE, NULL, NULL);
	} else {
		PKTCFREE(dhdp->osh, pktbuf, TRUE);
		return BCME_ERROR;
	}

#if (defined(BCM_ROUTER_DHD) && defined(QOS_MAP_SET))
	if (ifp->qosmap_up_table_enable) {
		pktsetprio_qms(pktbuf, ifp->qosmap_up_table, FALSE);
	}
	else
#endif
	{
		/* Look into the packet and update the packet priority */
#ifndef PKTPRIO_OVERRIDE
		/* XXX RB:6270 Ignore skb->priority from TCP/IP stack */
		if (PKTPRIO(pktbuf) == 0)
#endif /* !PKTPRIO_OVERRIDE */
		{
#if (!defined(BCM_ROUTER_DHD) && (defined(QOS_MAP_SET) || \
	defined(WL_CUSTOM_MAPPING_OF_DSCP)))
			u8 *up_table = wl_get_up_table(dhdp, ifidx);
			pktsetprio_qms(pktbuf, up_table, FALSE);
			if (PKTPRIO(pktbuf) > MAXPRIO) {
				DHD_ERROR_RLMT(("wrong user prio:%d from qosmap ifidx:%d\n",
					PKTPRIO(pktbuf), ifidx));
				if (up_table) {
					prhex("up_table", up_table, UP_TABLE_MAX);
				}
			}
#else
			/* For LLR, pkt prio will be changed to 7(NC) here */
			pktsetprio(pktbuf, FALSE);
#endif /* QOS_MAP_SET || WL_CUSTOM_MAPPING_OF_DSCP */
		}
#ifndef PKTPRIO_OVERRIDE
		else {
			/* Some protocols like OZMO use priority values from 256..263.
			 * these are magic values to indicate a specific 802.1d priority.
			 * make sure that priority field is in range of 0..7
			 */
			PKTSETPRIO(pktbuf, PKTPRIO(pktbuf) & 0x7);
		}
#endif /* !PKTPRIO_OVERRIDE */
	}

#if defined(BCM_ROUTER_DHD)
	traffic_mgmt_pkt_set_prio(dhdp, pktbuf);

#endif /* BCM_ROUTER_DHD */

	BCM_REFERENCE(pkt_flow_prio);
	/* Intercept and create Socket level statistics */
	/*
	 * TODO: Some how moving this code block above the pktsetprio code
	 * is resetting the priority back to 0, but this does not happen for
	 * packets generated from iperf uisng -S option. Can't understand why.
	 */
	dhd_update_sock_flows(dhd, pktbuf);

#ifdef SUPPORT_SET_TID
	dhd_set_tid_based_on_uid(dhdp, pktbuf);
#endif	/* SUPPORT_SET_TID */

#ifdef PCIE_FULL_DONGLE
	/*
	 * Lkup the per interface hash table, for a matching flowring. If one is not
	 * available, allocate a unique flowid and add a flowring entry.
	 * The found or newly created flowid is placed into the pktbuf's tag.
	 */

#ifdef DHD_TX_PROFILE
	if (dhdp->tx_profile_enab && dhdp->num_profiles > 0 &&
		dhd_protocol_matches_profile(PKTDATA(dhdp->osh, pktbuf),
		PKTLEN(dhdp->osh, pktbuf), dhdp->protocol_filters,
		dhdp->host_sfhllc_supported)) {
		/* we only have support for one tx_profile at the moment */

		/* tagged packets must be put into TID 6 */
		PKTSETPRIO(pktbuf, PRIO_8021D_VO);
	}
#endif /* defined(DHD_TX_PROFILE) */

	if (PKTPRIO(pktbuf) > MAXPRIO) {
		DHD_ERROR_RLMT(("Wrong user prio:%d ifidx:%d\n", PKTPRIO(pktbuf), ifidx));
		/* non-assert build, print ratelimit error, free packet and exit */
		ASSERT(0);
		PKTCFREE(dhd->pub.osh, pktbuf, TRUE);
		return BCME_ERROR;
	}

	pkt_flow_prio = dhdp->flow_prio_map[(PKTPRIO(pktbuf))];

	ret = dhd_flowid_update(dhdp, ifidx, pkt_flow_prio, pktbuf);
	if (ret != BCME_OK) {
		PKTCFREE(dhd->pub.osh, pktbuf, TRUE);
		return ret;
	}
#endif /* PCIE_FULL_DONGLE */
	/* terence 20150901: Micky add to ajust the 802.1X priority */
	/* Set the 802.1X packet with the highest priority 7 */
	if (dhdp->conf->pktprio8021x >= 0)
		pktset8021xprio(pktbuf, dhdp->conf->pktprio8021x);

#ifdef PROP_TXSTATUS
	if (dhd_wlfc_is_supported(dhdp)) {
		/* store the interface ID */
		DHD_PKTTAG_SETIF(PKTTAG(pktbuf), ifidx);

		/* store destination MAC in the tag as well */
		DHD_PKTTAG_SETDSTN(PKTTAG(pktbuf), eh->ether_dhost);

		/* decide which FIFO this packet belongs to */
		if (ETHER_ISMULTI(eh->ether_dhost))
			/* one additional queue index (highest AC + 1) is used for bc/mc queue */
			DHD_PKTTAG_SETFIFO(PKTTAG(pktbuf), AC_COUNT);
		else
			DHD_PKTTAG_SETFIFO(PKTTAG(pktbuf), WME_PRIO2AC(PKTPRIO(pktbuf)));
	} else
#endif /* PROP_TXSTATUS */
	{
		/* If the protocol uses a data header, apply it */
		dhd_prot_hdrpush(dhdp, ifidx, pktbuf);
	}

	/* Use bus module to send data frame */
#ifdef PROP_TXSTATUS
	{
		if (dhd_wlfc_commit_packets(dhdp, (f_commitpkt_t)dhd_bus_txdata,
			dhdp->bus, pktbuf, TRUE) == WLFC_UNSUPPORTED) {
			/* non-proptxstatus way */
#ifdef BCMPCIE
			ret = dhd_bus_txdata(dhdp->bus, pktbuf, (uint8)ifidx);
#else
			ret = dhd_bus_txdata(dhdp->bus, pktbuf);
#endif /* BCMPCIE */
		}
	}
#else
#ifdef BCMPCIE
	ret = dhd_bus_txdata(dhdp->bus, pktbuf, (uint8)ifidx);
#else
	ret = dhd_bus_txdata(dhdp->bus, pktbuf);
#endif /* BCMPCIE */
#endif /* PROP_TXSTATUS */

	/* Update the packet counters here, as it is called for LB Tx and non-LB Tx too */
	if (ret) {
#ifdef BCMDBUS
		PKTCFREE(dhdp->osh, pktbuf, TRUE);
#endif /* BCMDBUS */
		ifp->stats.tx_dropped++;
		dhdp->tx_dropped++;
	} else {
#ifdef PROP_TXSTATUS
		/* tx_packets counter can counted only when wlfc is disabled */
		if (!dhd_wlfc_is_supported(dhdp))
#endif
		{
			dhdp->tx_packets++;
			dhdp->tx_bytes += datalen;
			ifp->stats.tx_packets++;
			ifp->stats.tx_bytes += datalen;
		}
		dhdp->actual_tx_pkts++;
	}

	return ret;
}

int
BCMFASTPATH(dhd_sendpkt)(dhd_pub_t *dhdp, int ifidx, void *pktbuf)
{
	int ret = 0;
	unsigned long flags;
	dhd_if_t *ifp;

	DHD_GENERAL_LOCK(dhdp, flags);
	ifp = dhd_get_ifp(dhdp, ifidx);
	if (!ifp || ifp->del_in_progress) {
		DHD_ERROR(("%s: ifp:%p del_in_progress:%d\n",
			__FUNCTION__, ifp, ifp ? ifp->del_in_progress : 0));
		DHD_GENERAL_UNLOCK(dhdp, flags);
		PKTCFREE(dhdp->osh, pktbuf, TRUE);
		return -ENODEV;
	}
	if (DHD_BUS_CHECK_DOWN_OR_DOWN_IN_PROGRESS(dhdp)) {
		DHD_ERROR(("%s: returning as busstate=%d\n",
			__FUNCTION__, dhdp->busstate));
		DHD_GENERAL_UNLOCK(dhdp, flags);
		PKTCFREE(dhdp->osh, pktbuf, TRUE);
		return -ENODEV;
	}
	DHD_IF_SET_TX_ACTIVE(ifp, DHD_TX_SEND_PKT);
	DHD_BUS_BUSY_SET_IN_SEND_PKT(dhdp);
	DHD_GENERAL_UNLOCK(dhdp, flags);

	DHD_GENERAL_LOCK(dhdp, flags);
	if (DHD_BUS_CHECK_SUSPEND_OR_SUSPEND_IN_PROGRESS(dhdp)) {
		DHD_ERROR(("%s: bus is in suspend(%d) or suspending(0x%x) state!!\n",
			__FUNCTION__, dhdp->busstate, dhdp->dhd_bus_busy_state));
		DHD_BUS_BUSY_CLEAR_IN_SEND_PKT(dhdp);
		DHD_IF_CLR_TX_ACTIVE(ifp, DHD_TX_SEND_PKT);
		dhd_os_tx_completion_wake(dhdp);
		dhd_os_busbusy_wake(dhdp);
		DHD_GENERAL_UNLOCK(dhdp, flags);
		PKTCFREE(dhdp->osh, pktbuf, TRUE);
		return -ENODEV;
	}
	DHD_GENERAL_UNLOCK(dhdp, flags);

	ret = __dhd_sendpkt(dhdp, ifidx, pktbuf);

	DHD_GENERAL_LOCK(dhdp, flags);
	DHD_BUS_BUSY_CLEAR_IN_SEND_PKT(dhdp);
	DHD_IF_CLR_TX_ACTIVE(ifp, DHD_TX_SEND_PKT);
	dhd_os_tx_completion_wake(dhdp);
	dhd_os_busbusy_wake(dhdp);
	DHD_GENERAL_UNLOCK(dhdp, flags);
	return ret;
}

#ifdef DHD_MQ
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
static uint16
BCMFASTPATH(dhd_select_queue)(struct net_device *net, struct sk_buff *skb,
       void *accel_priv, select_queue_fallback_t fallback)
#else
static uint16
BCMFASTPATH(dhd_select_queue)(struct net_device *net, struct sk_buff *skb)
#endif /* LINUX_VERSION_CODE */
{
	dhd_info_t *dhd_info = DHD_DEV_INFO(net);
	dhd_pub_t *dhdp = &dhd_info->pub;
	uint16 prio = 0;

	BCM_REFERENCE(dhd_info);
	BCM_REFERENCE(dhdp);
	BCM_REFERENCE(prio);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
	if (mq_select_disable) {
		/* if driver side queue selection is disabled via sysfs, call the kernel
		* supplied fallback function to select the queue, which is usually
		* '__netdev_pick_tx()' in net/core/dev.c
		*/
		return fallback(net, skb);
	}
#endif /* LINUX_VERSION */

	prio = dhdp->flow_prio_map[skb->priority];
	if (prio < AC_COUNT)
		return prio;
	else
		return AC_BK;
}
#endif /* DHD_MQ */

netdev_tx_t
BCMFASTPATH(dhd_start_xmit)(struct sk_buff *skb, struct net_device *net)
{
	int ret;
	void *pktbuf;
	dhd_info_t *dhd = DHD_DEV_INFO(net);
	dhd_if_t *ifp = NULL;
	int ifidx;
	unsigned long flags;
#if !defined(BCM_ROUTER_DHD)
	uint8 htsfdlystat_sz = 0;
#endif /* ! BCM_ROUTER_DHD */
#ifdef DHD_WMF
	struct ether_header *eh;
	uint8 *iph;
#endif /* DHD_WMF */
#if defined(DHD_MQ) && defined(DHD_MQ_STATS)
	int qidx = 0;
	int cpuid = 0;
	int prio = 0;
#endif /* DHD_MQ && DHD_MQ_STATS */
#if defined(WL_CFG80211)
	struct bcm_cfg80211 *cfg = wl_get_cfg(net);
#endif /* defined(WL_CFG80211) */

	DHD_TRACE(("%s: Enter\n", __FUNCTION__));
#if defined(WL_CFG80211)
	UNUSED_PARAMETER(cfg);
#endif /* defined(WL_CFG80211) */

#if defined(DHD_MQ) && defined(DHD_MQ_STATS)
	qidx = skb_get_queue_mapping(skb);
	/* if in a non pre-emptable context, smp_processor_id can be used
	* else get_cpu and put_cpu should be used
	*/
	if (!CAN_SLEEP()) {
		cpuid = smp_processor_id();
	}
	else {
		cpuid = get_cpu();
		put_cpu();
	}
	prio = dhd->pub.flow_prio_map[skb->priority];
	DHD_TRACE(("%s: Q idx = %d, CPU = %d, prio = %d \n", __FUNCTION__,
		qidx, cpuid, prio));
	dhd->pktcnt_qac_histo[qidx][prio]++;
	dhd->pktcnt_per_ac[prio]++;
	dhd->cpu_qstats[qidx][cpuid]++;
#endif /* DHD_MQ && DHD_MQ_STATS */

	if (dhd_query_bus_erros(&dhd->pub)) {
		return -ENODEV;
	}

	DHD_GENERAL_LOCK(&dhd->pub, flags);
	DHD_BUS_BUSY_SET_IN_TX(&dhd->pub);
	DHD_GENERAL_UNLOCK(&dhd->pub, flags);

	DHD_GENERAL_LOCK(&dhd->pub, flags);
#ifdef BCMPCIE
	if (DHD_BUS_CHECK_SUSPEND_OR_SUSPEND_IN_PROGRESS(&dhd->pub)) {
		DHD_ERROR_RLMT(("%s: bus is in suspend(%d) or suspending(0x%x) state!!\n",
			__FUNCTION__, dhd->pub.busstate, dhd->pub.dhd_bus_busy_state));
		DHD_BUS_BUSY_CLEAR_IN_TX(&dhd->pub);
#ifdef PCIE_FULL_DONGLE
		/* Stop tx queues if suspend is in progress or suspended */
		if (DHD_BUS_CHECK_ANY_SUSPEND_IN_PROGRESS(&dhd->pub) ||
			(dhd->pub.busstate == DHD_BUS_SUSPEND &&
			!DHD_BUS_BUSY_CHECK_RESUME_IN_PROGRESS(&dhd->pub))) {
			dhd_bus_stop_queue(dhd->pub.bus);
		}
#endif /* PCIE_FULL_DONGLE */
		dhd_os_busbusy_wake(&dhd->pub);
		DHD_GENERAL_UNLOCK(&dhd->pub, flags);
		return NETDEV_TX_BUSY;
	}
#else
	if (DHD_BUS_CHECK_SUSPEND_OR_SUSPEND_IN_PROGRESS(&dhd->pub)) {
		DHD_ERROR(("%s: bus is in suspend(%d) or suspending(0x%x) state!!\n",
			__FUNCTION__, dhd->pub.busstate, dhd->pub.dhd_bus_busy_state));
	}
#endif

	DHD_OS_WAKE_LOCK(&dhd->pub);

#if defined(DHD_HANG_SEND_UP_TEST)
	if (dhd->pub.req_hang_type == HANG_REASON_BUS_DOWN) {
		DHD_ERROR(("%s: making DHD_BUS_DOWN\n", __FUNCTION__));
		dhd->pub.busstate = DHD_BUS_DOWN;
	}
#endif /* DHD_HANG_SEND_UP_TEST */

	/* Reject if down */
	/* XXX kernel panic issue when first bootup time,
	 *	 rmmod without interface down make unnecessary hang event.
	 */
	if (dhd->pub.hang_was_sent || DHD_BUS_CHECK_DOWN_OR_DOWN_IN_PROGRESS(&dhd->pub)) {
		DHD_ERROR(("%s: xmit rejected pub.up=%d busstate=%d \n",
			__FUNCTION__, dhd->pub.up, dhd->pub.busstate));
		dhd_tx_stop_queues(net);
#if defined(OEM_ANDROID)
		/* Send Event when bus down detected during data session */
		if (dhd->pub.up && !dhd->pub.hang_was_sent && !DHD_BUS_CHECK_REMOVE(&dhd->pub)) {
			DHD_ERROR(("%s: Event HANG sent up\n", __FUNCTION__));
			dhd->pub.hang_reason = HANG_REASON_BUS_DOWN;
			net_os_send_hang_message(net);
		}
#endif /* OEM_ANDROID */
		DHD_BUS_BUSY_CLEAR_IN_TX(&dhd->pub);
		dhd_os_busbusy_wake(&dhd->pub);
		DHD_GENERAL_UNLOCK(&dhd->pub, flags);
		DHD_OS_WAKE_UNLOCK(&dhd->pub);
		return NETDEV_TX_BUSY;
	}

	ifp = DHD_DEV_IFP(net);
	ifidx = DHD_DEV_IFIDX(net);
#ifdef DHD_BUZZZ_LOG_ENABLED
	BUZZZ_LOG(START_XMIT_BGN, 2, (uint32)ifidx, (uintptr)skb);
#endif /* DHD_BUZZZ_LOG_ENABLED */
	if (ifidx == DHD_BAD_IF) {
		DHD_ERROR(("%s: bad ifidx %d\n", __FUNCTION__, ifidx));
		dhd_tx_stop_queues(net);
		DHD_BUS_BUSY_CLEAR_IN_TX(&dhd->pub);
		dhd_os_busbusy_wake(&dhd->pub);
		DHD_GENERAL_UNLOCK(&dhd->pub, flags);
		DHD_OS_WAKE_UNLOCK(&dhd->pub);
		return NETDEV_TX_BUSY;
	}
#ifdef RPM_FAST_TRIGGER
	/* Xmit is running reset RPM fast trigger */
	if (dhd->pub.rpm_fast_trigger) {
		DHD_ERROR(("%s: Reset RPM fast trigger\n", __FUNCTION__));
		dhd->pub.rpm_fast_trigger = FALSE;
	}
#endif /* RPM_FAST_TRIGGER */
	DHD_GENERAL_UNLOCK(&dhd->pub, flags);

	/* If tput test is in progress */
	if (dhd->pub.tput_data.tput_test_running) {
		return NETDEV_TX_BUSY;
	}

	ASSERT(ifidx == dhd_net2idx(dhd, net));
	ASSERT((ifp != NULL) && ((ifidx < DHD_MAX_IFS) && (ifp == dhd->iflist[ifidx])));

	bcm_object_trace_opr(skb, BCM_OBJDBG_ADD_PKT, __FUNCTION__, __LINE__);

	/* re-align socket buffer if "skb->data" is odd address */
	if (((unsigned long)(skb->data)) & 0x1) {
		unsigned char *data = skb->data;
		uint32 length = skb->len;
		PKTPUSH(dhd->pub.osh, skb, 1);
		memmove(skb->data, data, length);
		PKTSETLEN(dhd->pub.osh, skb, length);
	}

#ifdef TPUT_MONITOR
	if (dhd->pub.conf->tput_monitor_ms) {
		dhd_os_sdlock_txq(&dhd->pub);
		dhd->pub.conf->net_len += PKTLEN(dhd->pub.osh, skb);
		dhd_os_sdunlock_txq(&dhd->pub);
		if ((dhd->pub.conf->data_drop_mode == XMIT_DROP) &&
				(PKTLEN(dhd->pub.osh, skb) > 500)) {
			dev_kfree_skb(skb);
			return NETDEV_TX_OK;
		}
	}
#endif
	/* Make sure there's enough room for any header */
#if !defined(BCM_ROUTER_DHD)
	if (skb_cow(skb, (dhd->pub.hdrlen + htsfdlystat_sz))) {
		DHD_ERROR(("%s: skb_cow failed\n",
		           dhd_ifname(&dhd->pub, ifidx)));
		bcm_object_trace_opr(skb, BCM_OBJDBG_REMOVE, __FUNCTION__, __LINE__);
		dev_kfree_skb_any(skb);
		ret = -ENOMEM;
		goto done;
	}
#endif /* !BCM_ROUTER_DHD */

	/* Convert to packet */
	if (!(pktbuf = PKTFRMNATIVE(dhd->pub.osh, skb))) {
		DHD_ERROR(("%s: PKTFRMNATIVE failed\n",
		           dhd_ifname(&dhd->pub, ifidx)));
		bcm_object_trace_opr(skb, BCM_OBJDBG_REMOVE, __FUNCTION__, __LINE__);
		dev_kfree_skb_any(skb);
		ret = -ENOMEM;
		goto done;
	}

#ifdef DHD_WET
	/* wet related packet proto manipulation should be done in DHD
	   since dongle doesn't have complete payload
	 */
	if (WET_ENABLED(&dhd->pub) &&
			(dhd_wet_send_proc(dhd->pub.wet_info, pktbuf, &pktbuf) < 0)) {
		DHD_INFO(("%s:%s: wet send proc failed\n",
				__FUNCTION__, dhd_ifname(&dhd->pub, ifidx)));
		PKTFREE(dhd->pub.osh, pktbuf, FALSE);
		ret =  -EFAULT;
		goto done;
	}
#endif /* DHD_WET */

#ifdef DHD_WMF
	eh = (struct ether_header *)PKTDATA(dhd->pub.osh, pktbuf);
	iph = (uint8 *)eh + ETHER_HDR_LEN;

	/* WMF processing for multicast packets
	 * Only IPv4 packets are handled
	 */
	if (ifp->wmf.wmf_enable && (ntoh16(eh->ether_type) == ETHER_TYPE_IP) &&
		(IP_VER(iph) == IP_VER_4) && (ETHER_ISMULTI(eh->ether_dhost) ||
		((IPV4_PROT(iph) == IP_PROT_IGMP) && dhd->pub.wmf_ucast_igmp))) {
#if defined(DHD_IGMP_UCQUERY) || defined(DHD_UCAST_UPNP)
		void *sdu_clone;
		bool ucast_convert = FALSE;
#ifdef DHD_UCAST_UPNP
		uint32 dest_ip;

		dest_ip = ntoh32(*((uint32 *)(iph + IPV4_DEST_IP_OFFSET)));
		ucast_convert = dhd->pub.wmf_ucast_upnp && MCAST_ADDR_UPNP_SSDP(dest_ip);
#endif /* DHD_UCAST_UPNP */
#ifdef DHD_IGMP_UCQUERY
		ucast_convert |= dhd->pub.wmf_ucast_igmp_query &&
			(IPV4_PROT(iph) == IP_PROT_IGMP) &&
			(*(iph + IPV4_HLEN(iph)) == IGMPV2_HOST_MEMBERSHIP_QUERY);
#endif /* DHD_IGMP_UCQUERY */
		if (ucast_convert) {
			dhd_sta_t *sta;
			unsigned long flags;
			struct list_head snapshot_list;
			struct list_head *wmf_ucforward_list;

			ret = NETDEV_TX_OK;

			/* For non BCM_GMAC3 platform we need a snapshot sta_list to
			 * resolve double DHD_IF_STA_LIST_LOCK call deadlock issue.
			 */
			wmf_ucforward_list = DHD_IF_WMF_UCFORWARD_LOCK(dhd, ifp, &snapshot_list);

			GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
			/* Convert upnp/igmp query to unicast for each assoc STA */
			list_for_each_entry(sta, wmf_ucforward_list, list) {
				GCC_DIAGNOSTIC_POP();
				/* Skip sending to proxy interfaces of proxySTA */
				if (sta->psta_prim != NULL && !ifp->wmf_psta_disable) {
					continue;
				}
				if ((sdu_clone = PKTDUP(dhd->pub.osh, pktbuf)) == NULL) {
					ret = WMF_NOP;
					break;
				}
				dhd_wmf_forward(ifp->wmf.wmfh, sdu_clone, 0, sta, 1);
			}
			DHD_IF_WMF_UCFORWARD_UNLOCK(dhd, wmf_ucforward_list);

			DHD_GENERAL_LOCK(&dhd->pub, flags);
			DHD_BUS_BUSY_CLEAR_IN_TX(&dhd->pub);
			dhd_os_busbusy_wake(&dhd->pub);
			DHD_GENERAL_UNLOCK(&dhd->pub, flags);
			DHD_OS_WAKE_UNLOCK(&dhd->pub);

			if (ret == NETDEV_TX_OK)
				PKTFREE(dhd->pub.osh, pktbuf, TRUE);

			return ret;
		} else
#endif /* defined(DHD_IGMP_UCQUERY) || defined(DHD_UCAST_UPNP) */
		{
			/* There will be no STA info if the packet is coming from LAN host
			 * Pass as NULL
			 */
			ret = dhd_wmf_packets_handle(&dhd->pub, pktbuf, NULL, ifidx, 0);
			switch (ret) {
			case WMF_TAKEN:
			case WMF_DROP:
				/* Either taken by WMF or we should drop it.
				 * Exiting send path
				 */

				DHD_GENERAL_LOCK(&dhd->pub, flags);
				DHD_BUS_BUSY_CLEAR_IN_TX(&dhd->pub);
				dhd_os_busbusy_wake(&dhd->pub);
				DHD_GENERAL_UNLOCK(&dhd->pub, flags);
				DHD_OS_WAKE_UNLOCK(&dhd->pub);
				return NETDEV_TX_OK;
			default:
				/* Continue the transmit path */
				break;
			}
		}
	}
#endif /* DHD_WMF */
#ifdef DHD_PSTA
	/* PSR related packet proto manipulation should be done in DHD
	 * since dongle doesn't have complete payload
	 */
	if (PSR_ENABLED(&dhd->pub) &&
#ifdef BCM_ROUTER_DHD
		!(ifp->primsta_dwds) &&
#endif /* BCM_ROUTER_DHD */
		(dhd_psta_proc(&dhd->pub, ifidx, &pktbuf, TRUE) < 0)) {

			DHD_ERROR(("%s:%s: psta send proc failed\n", __FUNCTION__,
				dhd_ifname(&dhd->pub, ifidx)));
	}
#endif /* DHD_PSTA */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0) && defined(DHD_TCP_PACING_SHIFT)
#ifndef DHD_DEFAULT_TCP_PACING_SHIFT
#define DHD_DEFAULT_TCP_PACING_SHIFT 7
#endif /* DHD_DEFAULT_TCP_PACING_SHIFT */
	if (skb->sk) {
		sk_pacing_shift_update(skb->sk, DHD_DEFAULT_TCP_PACING_SHIFT);
	}
#else
#if defined(BCMPCIE) && defined(DHD_VSDB_SKIP_ORPHAN) && defined(WL_CFG80211)
	if (!cfg->vsdb_mode)
#endif /* (BCMPCIE) && (DHD_VSDB_SKIP_ORPHAN) && defined(WL_CFG80211) */
	skb_orphan(skb);
#endif /* LINUX_VERSION_CODE >= 4.19.0 && DHD_TCP_PACING_SHIFT */

	/* move from dhdsdio_sendfromq(), try to orphan skb early */
	if (dhd->pub.conf->orphan_move == 2)
		PKTORPHAN(skb, dhd->pub.conf->tsq);
	else if (dhd->pub.conf->orphan_move == 3)
		skb_orphan(skb);

#ifdef DHDTCPSYNC_FLOOD_BLK
	if (dhd_tcpdata_get_flag(&dhd->pub, pktbuf) == FLAG_SYNCACK) {
		ifp->tsyncack_txed ++;
	}
#endif /* DHDTCPSYNC_FLOOD_BLK */

#ifdef DHDTCPACK_SUPPRESS
	if (dhd->pub.tcpack_sup_mode == TCPACK_SUP_HOLD) {
		/* If this packet has been hold or got freed, just return */
		if (dhd_tcpack_hold(&dhd->pub, pktbuf, ifidx)) {
			ret = 0;
			goto done;
		}
	} else {
		/* If this packet has replaced another packet and got freed, just return */
		if (dhd_tcpack_suppress(&dhd->pub, pktbuf)) {
			ret = 0;
			goto done;
		}
	}
#endif /* DHDTCPACK_SUPPRESS */

	/*
	 * If Load Balance is enabled queue the packet
	 * else send directly from here.
	 */
#if defined(DHD_LB_TXP)
	ret = dhd_lb_sendpkt(dhd, net, ifidx, pktbuf);
#else
	ret = __dhd_sendpkt(&dhd->pub, ifidx, pktbuf);
#endif
	BCM_REFERENCE(ret);

done:
	DHD_GENERAL_LOCK(&dhd->pub, flags);
	DHD_BUS_BUSY_CLEAR_IN_TX(&dhd->pub);
	DHD_IF_CLR_TX_ACTIVE(ifp, DHD_TX_START_XMIT);
	dhd_os_tx_completion_wake(&dhd->pub);
	dhd_os_busbusy_wake(&dhd->pub);
	DHD_GENERAL_UNLOCK(&dhd->pub, flags);
	DHD_OS_WAKE_UNLOCK(&dhd->pub);
#ifdef DHD_BUZZZ_LOG_ENABLED
	BUZZZ_LOG(START_XMIT_END, 0);
#endif /* DHD_BUZZZ_LOG_ENABLED */
	/* Return ok: we always eat the packet */
	return NETDEV_TX_OK;
}

static void
__dhd_txflowcontrol(dhd_pub_t *dhdp, struct net_device *net, bool state)
{

	if (net->reg_state != NETREG_REGISTERED) {
		return;
	}

	if (state == ON) {
		if (!netif_queue_stopped(net)) {
			DHD_TXFLOWCTL(("%s: Stop Netif Queue\n", __FUNCTION__));
			netif_stop_queue(net);
		} else {
			DHD_INFO(("%s: Netif Queue already stopped\n", __FUNCTION__));
		}
	}

	if (state == OFF) {
		if (netif_queue_stopped(net)) {
			DHD_TXFLOWCTL(("%s: Start Netif Queue\n", __FUNCTION__));
			netif_wake_queue(net);
		} else {
			DHD_INFO(("%s: Netif Queue already started\n", __FUNCTION__));
		}
	}
}

void
dhd_txflowcontrol(dhd_pub_t *dhdp, int ifidx, bool state)
{
	struct net_device *net;
	dhd_info_t *dhd = dhdp->info;
	unsigned long flags;
	int i;

	DHD_TRACE(("%s: Enter\n", __FUNCTION__));

	ASSERT(dhd);

#ifdef DHD_LOSSLESS_ROAMING
	/* block flowcontrol during roaming */
	if ((dhdp->dequeue_prec_map == (1 << dhdp->flow_prio_map[PRIO_8021D_NC])) && (state == ON))
	{
		DHD_ERROR_RLMT(("%s: Roaming in progress, cannot stop network queue (0x%x:%d)\n",
			__FUNCTION__, dhdp->dequeue_prec_map, dhdp->flow_prio_map[PRIO_8021D_NC]));
		return;
	}
#endif

	flags = dhd_os_sdlock_txoff(&dhd->pub);
	if (ifidx == ALL_INTERFACES) {
		for (i = 0; i < DHD_MAX_IFS; i++) {
			if (dhd->iflist[i]) {
				net = dhd->iflist[i]->net;
				__dhd_txflowcontrol(dhdp, net, state);
			}
		}
	} else {
		if (dhd->iflist[ifidx]) {
			net = dhd->iflist[ifidx]->net;
			__dhd_txflowcontrol(dhdp, net, state);
		}
	}
	dhdp->txoff = state;
	dhd_os_sdunlock_txoff(&dhd->pub, flags);
}

void
dhd_txcomplete(dhd_pub_t *dhdp, void *txp, bool success)
{
	dhd_info_t *dhd = (dhd_info_t *)(dhdp->info);
	struct ether_header *eh;
	uint16 type;

	if (dhdp->tput_data.tput_test_running) {

		dhdp->batch_tx_pkts_cmpl++;

		/* don't count the stop pkt */
		if (success &&
			dhdp->batch_tx_pkts_cmpl <= dhdp->batch_tx_num_pkts)
			dhdp->tput_data.pkts_good++;
		else if (!success)
			dhdp->tput_data.pkts_bad++;

		/* we dont care for the stop packet in tput test */
		if (dhdp->batch_tx_pkts_cmpl == dhdp->batch_tx_num_pkts) {
			dhdp->tput_stop_ts = OSL_SYSUPTIME_US();
			dhdp->tput_data.pkts_cmpl += dhdp->batch_tx_pkts_cmpl;
			dhdp->tput_data.num_pkts += dhdp->batch_tx_num_pkts;
			dhd_os_tput_test_wake(dhdp);
		}
	}
	/* XXX where does this stuff belong to? */
	dhd_prot_hdrpull(dhdp, NULL, txp, NULL, NULL);

	/* XXX Use packet tag when it is available to identify its type */

	eh = (struct ether_header *)PKTDATA(dhdp->osh, txp);
	type  = ntoh16(eh->ether_type);

	if (type == ETHER_TYPE_802_1X) {
		atomic_dec(&dhd->pend_8021x_cnt);
	}

#ifdef PROP_TXSTATUS
	if (dhdp->wlfc_state && (dhdp->proptxstatus_mode != WLFC_FCMODE_NONE)) {
		dhd_if_t *ifp = dhd->iflist[DHD_PKTTAG_IF(PKTTAG(txp))];
		uint datalen  = PKTLEN(dhd->pub.osh, txp);
		if (ifp != NULL) {
			if (success) {
				dhd->pub.tx_packets++;
				dhd->pub.tx_bytes += datalen;
				ifp->stats.tx_packets++;
				ifp->stats.tx_bytes += datalen;
			} else {
				ifp->stats.tx_dropped++;
			}
		}
	}
#endif
	if (success) {
		dhd->pub.tot_txcpl++;
	}
}

void
dhd_os_tx_completion_wake(dhd_pub_t *dhd)
{
	/* Call wmb() to make sure before waking up the other event value gets updated */
	OSL_SMP_WMB();
	wake_up(&dhd->tx_completion_wait);
}

void
dhd_os_sdlock_txq(dhd_pub_t *pub)
{
	dhd_info_t *dhd;

	dhd = (dhd_info_t *)(pub->info);
#ifdef BCMDBUS
	spin_lock_irqsave(&dhd->txqlock, dhd->txqlock_flags);
#else
	spin_lock_bh(&dhd->txqlock);
#endif
}

void
dhd_os_sdunlock_txq(dhd_pub_t *pub)
{
	dhd_info_t *dhd;

	dhd = (dhd_info_t *)(pub->info);
#ifdef BCMDBUS
	spin_unlock_irqrestore(&dhd->txqlock, dhd->txqlock_flags);
#else
	spin_unlock_bh(&dhd->txqlock);
#endif
}

unsigned long
dhd_os_sdlock_txoff(dhd_pub_t *pub)
{
	dhd_info_t *dhd;
	unsigned long flags = 0;

	dhd = (dhd_info_t *)(pub->info);
	spin_lock_irqsave(&dhd->txoff_lock, flags);

	return flags;
}

void
dhd_os_sdunlock_txoff(dhd_pub_t *pub, unsigned long flags)
{
	dhd_info_t *dhd;

	dhd = (dhd_info_t *)(pub->info);
	spin_unlock_irqrestore(&dhd->txoff_lock, flags);
}

void
dhd_txfl_wake_lock_timeout(dhd_pub_t *pub, int val)
{
#ifdef CONFIG_HAS_WAKELOCK
	dhd_info_t *dhd = (dhd_info_t *)(pub->info);

	if (dhd) {
		dhd_wake_lock_timeout(dhd->wl_txflwake, msecs_to_jiffies(val));
	}
#endif /* CONFIG_HAS_WAKE_LOCK */
}

#ifdef DHD_4WAYM4_FAIL_DISCONNECT
void
dhd_eap_txcomplete(dhd_pub_t *dhdp, void *txp, bool success, int ifidx)
{
	dhd_info_t *dhd = (dhd_info_t *)(dhdp->info);
	struct ether_header *eh;
	uint16 type;

	/* XXX where does this stuff belong to? */
	dhd_prot_hdrpull(dhdp, NULL, txp, NULL, NULL);

	/* XXX Use packet tag when it is available to identify its type */
	eh = (struct ether_header *)PKTDATA(dhdp->osh, txp);
	type  = ntoh16(eh->ether_type);
	if (type == ETHER_TYPE_802_1X) {
		if (dhd_is_4way_msg((uint8 *)eh) == EAPOL_4WAY_M4) {
			dhd_if_t *ifp = NULL;
			ifp = dhd->iflist[ifidx];
			if (!ifp || !ifp->net) {
				return;
			}
			if (!success) {
				DHD_INFO(("%s: M4 TX failed on %d.\n",
						__FUNCTION__, ifidx));

				OSL_ATOMIC_SET(dhdp->osh, &ifp->m4state, M4_TXFAILED);
				schedule_delayed_work(&ifp->m4state_work,
						msecs_to_jiffies(MAX_4WAY_TIMEOUT_MS));
			} else {
				cancel_delayed_work(&ifp->m4state_work);
			}
		}
	}
}
#endif /* DHD_4WAYM4_FAIL_DISCONNECT */

void
dhd_handle_pktdata(dhd_pub_t *dhdp, int ifidx, void *pkt, uint8 *pktdata, uint32 pktid,
	uint32 pktlen, uint16 *pktfate, uint8 *dhd_udr, uint8 *dhd_igmp,
	bool tx, int pkt_wake, bool pkt_log)
{
	struct ether_header *eh;
	uint16 ether_type;
	uint32 pkthash;
	uint8 pkt_type = PKT_TYPE_DATA;
#ifdef DHD_PKT_LOGGING_DBGRING
	bool verbose_logging = FALSE;
	dhd_dbg_ring_t *ring;
	ring = &dhdp->dbg->dbg_rings[PACKET_LOG_RING_ID];
	UNUSED_PARAMETER(verbose_logging);
#endif /* DHD_PKT_LOGGING_DBGRING */

	if (!pktdata || pktlen < ETHER_HDR_LEN) {
		return;
	}

	eh = (struct ether_header *)pktdata;
	ether_type = ntoh16(eh->ether_type);

	/* Check packet type */
	if (dhd_check_ip_prot(pktdata, ether_type)) {
		if (dhd_check_dhcp(pktdata)) {
			pkt_type = PKT_TYPE_DHCP;
		} else if (dhd_check_icmp(pktdata)) {
			pkt_type = PKT_TYPE_ICMP;
		} else if (dhd_check_dns(pktdata)) {
			pkt_type = PKT_TYPE_DNS;
		}
	}
	else if (ether_type == ETHER_TYPE_IPV6) {
		if (dhd_check_icmpv6(pktdata, pktlen)) {
			pkt_type = PKT_TYPE_ICMPV6;
		}
#ifdef DHD_IPV6_DUMP
		else if (dhd_check_dhcp6(pktdata, pktlen)) {
			pkt_type = PKT_TYPE_DHCP6;
		} else if (dhd_check_dns6(pktdata, pktlen)) {
			pkt_type = PKT_TYPE_DNS6;
		}
#endif
	}
	else if (dhd_check_arp(pktdata, ether_type)) {
		pkt_type = PKT_TYPE_ARP;
	}
	else if (ether_type == ETHER_TYPE_802_1X) {
		pkt_type = PKT_TYPE_EAP;
	}
#ifdef DHD_PKT_LOGGING_DBGRING
	do {
		if (!OSL_ATOMIC_READ(dhdp->osh, &dhdp->pktlog->enable)) {
			struct dhd_pktlog_ring *pktlog_ring;

			pktlog_ring = dhdp->pktlog->pktlog_ring;
			if (pktlog_ring->pktcount <= DHD_PACKET_LOG_RING_RESUME_THRESHOLD) {
				dhd_pktlog_resume(dhdp);
			} else {
				/* If pktlog disabled(suspeneded), only allowed TXS update */
				if (tx && pktfate) {
					DHD_PKTLOG_TXS(dhdp, pkt, pktdata, pktid, *pktfate);
					pkthash = __dhd_dbg_pkt_hash((uintptr_t)pkt, pktid);
				}
				dhd_os_dbg_urgent_pullreq(dhdp->dbg->private,
					PACKET_LOG_RING_ID);
				break;
			}
		}

		/* Allow logging for all packets without pktlog filter */
		if (ring->log_level == RING_LOG_LEVEL_EXCESSIVE) {
			verbose_logging = TRUE;
			pkt_log = TRUE;
		/* Not allow logging for any packets */
		} else if (ring->log_level == RING_LOG_LEVEL_NONE) {
			verbose_logging = FALSE;
			pkt_log = FALSE;
		}
#endif /* DHD_PKT_LOGGING_DBGRING */
#ifdef DHD_SBN
		/* Set UDR based on packet type */
		if (dhd_udr && (pkt_type == PKT_TYPE_DHCP ||
			pkt_type == PKT_TYPE_DNS ||
			pkt_type == PKT_TYPE_ARP)) {
			*dhd_udr = TRUE;
		}
#endif /* DHD_SBN */

#ifdef DHD_PKT_LOGGING
#ifdef DHD_SKIP_PKTLOGGING_FOR_DATA_PKTS
		if (pkt_type != PKT_TYPE_DATA)
#else
#ifdef DHD_PKT_LOGGING_DBGRING
		if ((verbose_logging == TRUE) || (pkt_type != PKT_TYPE_DATA))
#endif /* DHD_PKT_LOGGING_DBGRING */
#endif /* DHD_PKT_LOGGING */
		{
			if (pkt_log) {
				if (tx) {
					if (pktfate) {
						/* Tx status */
						DHD_PKTLOG_TXS(dhdp, pkt, pktdata, pktid, *pktfate);
					} else {
						/* Tx packet */
						DHD_PKTLOG_TX(dhdp, pkt, pktdata, pktid);
					}
					pkthash = __dhd_dbg_pkt_hash((uintptr_t)pkt, pktid);
				} else {
					struct sk_buff *skb = (struct sk_buff *)pkt;
					if (pkt_wake) {
						DHD_PKTLOG_WAKERX(dhdp, skb, pktdata);
					} else {
						DHD_PKTLOG_RX(dhdp, skb, pktdata);
					}
				}
			}
		}
#endif /* DHD_PKT_LOGGING */
#ifdef DHD_PKT_LOGGING_DBGRING
	} while (FALSE);
#endif /* DHD_PKT_LOGGING_DBGRING */

	/* Dump packet data */
	if (!tx)
		dhd_dump_pkt(dhdp, ifidx, pktdata, pktlen, FALSE, &pkthash, NULL);
	switch (pkt_type) {
		case PKT_TYPE_DHCP:
			dhd_send_supp_dhcp(dhdp, ifidx, pktdata, tx, pktfate);
			break;
		case PKT_TYPE_EAP:
			dhd_send_supp_eap(dhdp, ifidx, pktdata, pktlen, tx, pktfate);
			break;
#ifdef DHD_IPV6_DUMP
		case PKT_TYPE_ICMPV6:
			dhd_icmpv6_dump(dhdp, ifidx, pktdata, tx, &pkthash, pktfate);
			break;
		case PKT_TYPE_DHCP6:
			dhd_dhcp6_dump(dhdp, ifidx, pktdata, tx, &pkthash, pktfate);
			break;
		case PKT_TYPE_DNS6:
			dhd_dns6_dump(dhdp, ifidx, pktdata, tx, &pkthash, pktfate);
			break;
#endif
		default:
			break;
	}
}
