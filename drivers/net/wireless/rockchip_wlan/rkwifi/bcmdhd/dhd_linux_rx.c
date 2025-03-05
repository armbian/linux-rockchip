/*
 * Broadcom Dongle Host Driver (DHD),
 * Linux-specific network interface for receive(rx) path
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
#include <net/ndisc.h>
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

/* RX frame thread priority */
int dhd_rxf_prio = CUSTOM_RXF_PRIO_SETTING;
module_param(dhd_rxf_prio, int, 0);

/* Request scheduling of the bus rx frame */
static void dhd_os_rxflock(dhd_pub_t *pub);
static void dhd_os_rxfunlock(dhd_pub_t *pub);

static int dhd_wl_host_event(dhd_info_t *dhd, int ifidx, void *pktdata, uint16 pktlen,
                wl_event_msg_t *event_ptr, void **data_ptr);

static inline int dhd_rxf_enqueue(dhd_pub_t *dhdp, void* skb)
{
	uint32 store_idx;
	uint32 sent_idx;

	if (!skb) {
		DHD_ERROR(("dhd_rxf_enqueue: NULL skb!!!\n"));
		return BCME_ERROR;
	}

	dhd_os_rxflock(dhdp);
	store_idx = dhdp->store_idx;
	sent_idx = dhdp->sent_idx;
	if (dhdp->skbbuf[store_idx] != NULL) {
		/* Make sure the previous packets are processed */
		dhd_os_rxfunlock(dhdp);
#ifdef RXF_DEQUEUE_ON_BUSY
		DHD_TRACE(("dhd_rxf_enqueue: pktbuf not consumed %p, store idx %d sent idx %d\n",
			skb, store_idx, sent_idx));
		return BCME_BUSY;
#else /* RXF_DEQUEUE_ON_BUSY */
		DHD_ERROR(("dhd_rxf_enqueue: pktbuf not consumed %p, store idx %d sent idx %d\n",
			skb, store_idx, sent_idx));
		/* removed msleep here, should use wait_event_timeout if we
		 * want to give rx frame thread a chance to run
		 */
#if defined(WAIT_DEQUEUE)
		OSL_SLEEP(1);
#endif
		return BCME_ERROR;
#endif /* RXF_DEQUEUE_ON_BUSY */
	}
	DHD_TRACE(("dhd_rxf_enqueue: Store SKB %p. idx %d -> %d\n",
		skb, store_idx, (store_idx + 1) & (MAXSKBPEND - 1)));
	dhdp->skbbuf[store_idx] = skb;
	dhdp->store_idx = (store_idx + 1) & (MAXSKBPEND - 1);
	dhd_os_rxfunlock(dhdp);

	return BCME_OK;
}

static inline void* dhd_rxf_dequeue(dhd_pub_t *dhdp)
{
	uint32 store_idx;
	uint32 sent_idx;
	void *skb;

	dhd_os_rxflock(dhdp);

	store_idx = dhdp->store_idx;
	sent_idx = dhdp->sent_idx;
	skb = dhdp->skbbuf[sent_idx];

	if (skb == NULL) {
		dhd_os_rxfunlock(dhdp);
		DHD_ERROR(("dhd_rxf_dequeue: Dequeued packet is NULL, store idx %d sent idx %d\n",
			store_idx, sent_idx));
		return NULL;
	}

	dhdp->skbbuf[sent_idx] = NULL;
	dhdp->sent_idx = (sent_idx + 1) & (MAXSKBPEND - 1);

	DHD_TRACE(("dhd_rxf_dequeue: netif_rx_ni(%p), sent idx %d\n",
		skb, sent_idx));

	dhd_os_rxfunlock(dhdp);

	return skb;
}

#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
void dhd_rx_wq_wakeup(struct work_struct *ptr)
{
	struct dhd_rx_tx_work *work;
	struct dhd_pub * pub;

	work = container_of(ptr, struct dhd_rx_tx_work, work);

	pub = work->pub;

	DHD_RPM(("%s: ENTER. \n", __FUNCTION__));

	if (atomic_read(&pub->block_bus) || pub->busstate == DHD_BUS_DOWN) {
		return;
	}

	DHD_OS_WAKE_LOCK(pub);
	if (pm_runtime_get_sync(dhd_bus_to_dev(pub->bus)) >= 0) {

		// do nothing but wakeup the bus.
		pm_runtime_mark_last_busy(dhd_bus_to_dev(pub->bus));
		pm_runtime_put_autosuspend(dhd_bus_to_dev(pub->bus));
	}
	DHD_OS_WAKE_UNLOCK(pub);
	kfree(work);
}
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

#ifdef DHD_WMF
bool
dhd_is_rxthread_enabled(dhd_pub_t *dhdp)
{
	dhd_info_t *dhd = dhdp->info;

	return dhd->rxthread_enabled;
}
#endif /* DHD_WMF */

/** Called when a frame is received by the dongle on interface 'ifidx' */
void
dhd_rx_frame(dhd_pub_t *dhdp, int ifidx, void *pktbuf, int numpkt, uint8 chan)
{
	dhd_info_t *dhd = (dhd_info_t *)dhdp->info;
	struct sk_buff *skb;
	uchar *eth;
	uint len;
	void *data, *pnext = NULL;
	int i;
	dhd_if_t *ifp;
	wl_event_msg_t event;
#if defined(OEM_ANDROID)
	int tout_rx = 0;
	int tout_ctrl = 0;
#endif /* OEM_ANDROID */
	void *skbhead = NULL;
	void *skbprev = NULL;
	uint16 protocol;
	unsigned char *dump_data;
#ifdef DHD_MCAST_REGEN
	uint8 interface_role;
	if_flow_lkup_t *if_flow_lkup;
	unsigned long flags;
#endif
#ifdef DHD_WAKE_STATUS
	wake_counts_t *wcp = NULL;
#endif /* DHD_WAKE_STATUS */
	int pkt_wake = 0;
#ifdef ENABLE_DHD_GRO
	bool dhd_gro_enable = TRUE;
	struct Qdisc *qdisc = NULL;
#endif /* ENABLE_DHD_GRO */

	DHD_TRACE(("%s: Enter\n", __FUNCTION__));
	BCM_REFERENCE(dump_data);
	BCM_REFERENCE(pkt_wake);

#ifdef DHD_TPUT_PATCH
	if (dhdp->conf->pktsetsum)
		PKTSETSUMGOOD(pktbuf, TRUE);
#endif

#ifdef ENABLE_DHD_GRO
	if (ifidx < DHD_MAX_IFS) {
		ifp = dhd->iflist[ifidx];
		if (ifp && ifp->net->qdisc) {
			if (ifp->net->qdisc->ops->cl_ops) {
				dhd_gro_enable = FALSE;
				DHD_TRACE(("%s: disable sw gro becasue of"
						" qdisc tx traffic control\n", __FUNCTION__));
			}

			if (dev_ingress_queue(ifp->net)) {
				qdisc = dev_ingress_queue(ifp->net)->qdisc_sleeping;
				if (qdisc != NULL && (qdisc->flags & TCQ_F_INGRESS)) {
					if (
#if defined(CONFIG_NET_XGRESS)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
						(ifp->net->tcx_ingress != NULL) ||
#endif /* LINUX_VERSION >= 6.6.0 */
#elif defined(CONFIG_NET_CLS_ACT)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0))
						(ifp->net->miniq_ingress != NULL) ||
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0))
						(ifp->net->ingress_cl_list != NULL) ||
#endif /* LINUX_VERSION >= 4.2.0 */
#endif /* CONFIG_NET_CLS_ACT */
						0 ) {
						dhd_gro_enable = FALSE;
						DHD_TRACE(("%s: disable sw gro because of"
						" qdisc rx traffic control\n", __FUNCTION__));
					}
				}
			}
		}
	}
#ifdef DHD_GRO_ENABLE_HOST_CTRL
	if (!dhdp->permitted_gro && dhd_gro_enable) {
		dhd_gro_enable = FALSE;
	}
#endif /* DHD_GRO_ENABLE_HOST_CTRL */
#endif /* ENABLE_DHD_GRO */

	for (i = 0; pktbuf && i < numpkt; i++, pktbuf = pnext) {
		struct ether_header *eh;

		pnext = PKTNEXT(dhdp->osh, pktbuf);
		PKTSETNEXT(dhdp->osh, pktbuf, NULL);

		/* info ring "debug" data, which is not a 802.3 frame, is sent/hacked with a
		 * special ifidx of DHD_DUMMY_INFO_IF.  This is just internal to dhd to get the data
		 * from dhd_msgbuf.c:dhd_prot_infobuf_cmplt_process() to here (dhd_rx_frame).
		 */
		if (ifidx == DHD_DUMMY_INFO_IF) {
			/* Event msg printing is called from dhd_rx_frame which is in Tasklet
			 * context in case of PCIe FD, in case of other bus this will be from
			 * DPC context. If we get bunch of events from Dongle then printing all
			 * of them from Tasklet/DPC context that too in data path is costly.
			 * Also in the new Dongle SW(4359, 4355 onwards) console prints too come as
			 * events with type WLC_E_TRACE.
			 * We'll print this console logs from the WorkQueue context by enqueing SKB
			 * here and Dequeuing will be done in WorkQueue and will be freed only if
			 * logtrace_pkt_sendup is TRUE
			 */
#ifdef SHOW_LOGTRACE
			dhd_event_logtrace_enqueue(dhdp, ifidx, pktbuf);
#else /* !SHOW_LOGTRACE */
			/* If SHOW_LOGTRACE not defined and ifidx is DHD_DUMMY_INFO_IF,
			 * free the PKT here itself
			 */
			PKTFREE_CTRLBUF(dhdp->osh, pktbuf, FALSE);
#endif /* SHOW_LOGTRACE */
			continue;
		}

		eh = (struct ether_header *)PKTDATA(dhdp->osh, pktbuf);

		/* Check if dhd_stop is in progress */
		if (dhdp->stop_in_progress) {
			DHD_ERROR_RLMT(("%s: dhd_stop in progress ignore received packet\n",
				__FUNCTION__));
			RX_PKTFREE(dhdp->osh, eh->ether_type, pktbuf, FALSE);
			continue;
		}

#ifdef DHD_WAKE_STATUS
		/* Get pkt_wake and clear it */
#if defined(BCMPCIE)
		pkt_wake = dhd_bus_set_get_bus_wake_pkt_dump(dhdp, 0);
#elif defined(BCMSDIO)
		pkt_wake = dhd_bus_set_get_bus_wake(dhdp, 0);
#endif /* BCMPCIE */
#ifdef BCMDBUS
		wcp = NULL;
#else
		wcp = dhd_bus_get_wakecount(dhdp);
#endif /* BCMDBUS */
		if (wcp == NULL) {
			/* If wakeinfo count buffer is null do not  update wake count values */
			pkt_wake = 0;
		}
#endif /* DHD_WAKE_STATUS */

		if (dhd->pub.tput_data.tput_test_running &&
			dhd->pub.tput_data.direction == TPUT_DIR_RX &&
			ntoh16(eh->ether_type) == ETHER_TYPE_IP) {
			dhd_tput_test_rx(dhdp, pktbuf);
			PKTFREE(dhd->pub.osh, pktbuf, FALSE);
			continue;
		}

		if (ifidx >= DHD_MAX_IFS) {
			DHD_ERROR(("%s: ifidx(%d) Out of bound. drop packet\n",
				__FUNCTION__, ifidx));
			RX_PKTFREE(dhdp->osh, eh->ether_type, pktbuf, FALSE);
			continue;
		}

		ifp = dhd->iflist[ifidx];
		if (ifp == NULL) {
			DHD_ERROR_RLMT(("%s: ifp is NULL. drop packet\n",
				__FUNCTION__));
			RX_PKTFREE(dhdp->osh, eh->ether_type, pktbuf, FALSE);
			continue;
		}

		/* Dropping only data packets before registering net device to avoid kernel panic */
#ifndef PROP_TXSTATUS_VSDB
		if ((!ifp->net || ifp->net->reg_state != NETREG_REGISTERED) &&
			(ntoh16(eh->ether_type) != ETHER_TYPE_BRCM))
#else
		if ((!ifp->net || ifp->net->reg_state != NETREG_REGISTERED || !dhd->pub.up) &&
			(ntoh16(eh->ether_type) != ETHER_TYPE_BRCM))
#endif /* PROP_TXSTATUS_VSDB */
		{
			DHD_ERROR(("%s: net device is NOT registered yet. drop packet\n",
			__FUNCTION__));
			PKTCFREE(dhdp->osh, pktbuf, FALSE);
			continue;
		}

#ifdef PROP_TXSTATUS
		if (dhd_wlfc_is_header_only_pkt(dhdp, pktbuf)) {
			/* WLFC may send header only packet when
			there is an urgent message but no packet to
			piggy-back on
			*/
			PKTCFREE(dhdp->osh, pktbuf, FALSE);
			continue;
		}
#endif
#ifdef DHD_L2_FILTER
		/* If block_ping is enabled drop the ping packet */
		if (ifp->block_ping) {
			if (bcm_l2_filter_block_ping(dhdp->osh, pktbuf) == BCME_OK) {
				PKTCFREE(dhdp->osh, pktbuf, FALSE);
				continue;
			}
		}
		if (ifp->grat_arp && DHD_IF_ROLE_STA(dhdp, ifidx)) {
		    if (bcm_l2_filter_gratuitous_arp(dhdp->osh, pktbuf) == BCME_OK) {
				PKTCFREE(dhdp->osh, pktbuf, FALSE);
				continue;
		    }
		}
		if (ifp->parp_enable && DHD_IF_ROLE_AP(dhdp, ifidx)) {
			int ret = dhd_l2_filter_pkt_handle(dhdp, ifidx, pktbuf, FALSE);

			/* Drop the packets if l2 filter has processed it already
			 * otherwise continue with the normal path
			 */
			if (ret == BCME_OK) {
				PKTCFREE(dhdp->osh, pktbuf, TRUE);
				continue;
			}
		}
		if (ifp->block_tdls) {
			if (bcm_l2_filter_block_tdls(dhdp->osh, pktbuf) == BCME_OK) {
				PKTCFREE(dhdp->osh, pktbuf, FALSE);
				continue;
			}
		}
#endif /* DHD_L2_FILTER */

#ifdef DHD_MCAST_REGEN
		DHD_FLOWID_LOCK(dhdp->flowid_lock, flags);
		if_flow_lkup = (if_flow_lkup_t *)dhdp->if_flow_lkup;
		ASSERT(if_flow_lkup);

		interface_role = if_flow_lkup[ifidx].role;
		DHD_FLOWID_UNLOCK(dhdp->flowid_lock, flags);

		if (ifp->mcast_regen_bss_enable && (interface_role != WLC_E_IF_ROLE_WDS) &&
				!DHD_IF_ROLE_AP(dhdp, ifidx) &&
				ETHER_ISUCAST(eh->ether_dhost)) {
			if (dhd_mcast_reverse_translation(eh) ==  BCME_OK) {
#ifdef DHD_PSTA
				/* Change bsscfg to primary bsscfg for unicast-multicast packets */
				if ((dhd_get_psta_mode(dhdp) == DHD_MODE_PSTA) ||
						(dhd_get_psta_mode(dhdp) == DHD_MODE_PSR)) {
					if (ifidx != 0) {
						/* Let the primary in PSTA interface handle this
						 * frame after unicast to Multicast conversion
						 */
						ifp = dhd_get_ifp(dhdp, 0);
						ASSERT(ifp);
					}
				}
			}
#endif /* PSTA */
		}
#endif /* MCAST_REGEN */

#ifdef DHD_WMF
		/* WMF processing for multicast packets */
		if (ifp->wmf.wmf_enable && (ETHER_ISMULTI(eh->ether_dhost))) {
			dhd_sta_t *sta;
			int ret;

			sta = dhd_find_sta(dhdp, ifidx, (void *)eh->ether_shost);
			ret = dhd_wmf_packets_handle(dhdp, pktbuf, sta, ifidx, 1);
			switch (ret) {
				case WMF_TAKEN:
					/* The packet is taken by WMF. Continue to next iteration */
					continue;
				case WMF_DROP:
					/* Packet DROP decision by WMF. Toss it */
					DHD_ERROR(("%s: WMF decides to drop packet\n",
						__FUNCTION__));
					PKTCFREE(dhdp->osh, pktbuf, FALSE);
					continue;
				default:
					/* Continue the transmit path */
					break;
			}
		}
#endif /* DHD_WMF */

#ifdef DHDTCPSYNC_FLOOD_BLK
		if (dhd_tcpdata_get_flag(dhdp, pktbuf) == FLAG_SYNC) {
			int delta_sec;
			int delta_sync;
			int sync_per_sec;
			u64 curr_time = DIV_U64_BY_U32(OSL_LOCALTIME_NS(), NSEC_PER_SEC);
			ifp->tsync_rcvd ++;
			delta_sync = ifp->tsync_rcvd - ifp->tsyncack_txed;
			delta_sec = curr_time - ifp->last_sync;
			if (delta_sec > 1) {
				sync_per_sec = delta_sync/delta_sec;
				if (sync_per_sec > TCP_SYNC_FLOOD_LIMIT) {
					schedule_work(&ifp->blk_tsfl_work);
					DHD_ERROR(("ifx %d TCP SYNC Flood attack suspected! "
						"sync recvied %d pkt/sec \n",
						ifidx, sync_per_sec));
					ifp->tsync_per_sec = sync_per_sec;
				}
				dhd_reset_tcpsync_info_by_ifp(ifp);
			}

		}
#endif /* DHDTCPSYNC_FLOOD_BLK */

#ifdef DHDTCPACK_SUPPRESS
		dhd_tcpdata_info_get(dhdp, pktbuf);
#endif
		skb = PKTTONATIVE(dhdp->osh, pktbuf);

		ASSERT(ifp);
		skb->dev = ifp->net;
#ifdef DHD_WET
		/* wet related packet proto manipulation should be done in DHD
		 * since dongle doesn't have complete payload
		 */
		if (WET_ENABLED(&dhd->pub) && (dhd_wet_recv_proc(dhd->pub.wet_info,
				pktbuf) < 0)) {
			DHD_INFO(("%s:%s: wet recv proc failed\n",
				__FUNCTION__, dhd_ifname(dhdp, ifidx)));
		}
#endif /* DHD_WET */

#ifdef DHD_PSTA
		if (PSR_ENABLED(dhdp) &&
#ifdef BCM_ROUTER_DHD
				!(ifp->primsta_dwds) &&
#endif /* BCM_ROUTER_DHD */
				(dhd_psta_proc(dhdp, ifidx, &pktbuf, FALSE) < 0)) {
			DHD_ERROR(("%s:%s: psta recv proc failed\n", __FUNCTION__,
				dhd_ifname(dhdp, ifidx)));
		}
#endif /* DHD_PSTA */

#if defined(BCM_ROUTER_DHD)
		/* XXX Use WOFA for both dhdap and dhdap-atlas router. */
		/* XXX dhd_sendpkt verify pkt accounting (TO/FRM NATIVE) and PKTCFREE */

		if (DHD_IF_ROLE_AP(dhdp, ifidx) && (!ifp->ap_isolate)) {
			eh = (struct ether_header *)PKTDATA(dhdp->osh, pktbuf);
			if (ETHER_ISUCAST(eh->ether_dhost)) {
				if (dhd_find_sta(dhdp, ifidx, (void *)eh->ether_dhost)) {
					dhd_sendpkt(dhdp, ifidx, pktbuf);
					continue;
				}
			} else {
				void *npkt;
#if defined(HNDCTF)
				if (PKTISCHAINED(pktbuf)) { /* XXX WAR */
					DHD_ERROR(("Error: %s():%d Chained non unicast pkt<%p>\n",
						__FUNCTION__, __LINE__, pktbuf));
					PKTFRMNATIVE(dhdp->osh, pktbuf);
					PKTCFREE(dhdp->osh, pktbuf, FALSE);
					continue;
				}
#endif /* HNDCTF */
				if ((ntoh16(eh->ether_type) != ETHER_TYPE_IAPP_L2_UPDATE) &&
					((npkt = PKTDUP(dhdp->osh, pktbuf)) != NULL))
					dhd_sendpkt(dhdp, ifidx, npkt);
			}
		}

#if defined(HNDCTF)
		/* try cut thru' before sending up */
		if (dhd_ctf_forward(dhd, skb, &pnext) == BCME_OK) {
			continue;
		}
#endif /* HNDCTF */

#else /* !BCM_ROUTER_DHD */

#if defined(DBG_PKT_MON) && !defined(PCIE_FULL_DONGLE)
		if (dhd_80211_mon_pkt(dhdp, pktbuf, ifidx)) {
			continue;
		}
#endif

#ifdef PCIE_FULL_DONGLE
		if ((DHD_IF_ROLE_AP(dhdp, ifidx) || DHD_IF_ROLE_P2PGO(dhdp, ifidx)) &&
			(!ifp->ap_isolate)) {
			eh = (struct ether_header *)PKTDATA(dhdp->osh, pktbuf);
			if (ETHER_ISUCAST(eh->ether_dhost)) {
				if (dhd_find_sta(dhdp, ifidx, (void *)eh->ether_dhost)) {
					dhdp->rx_forward++; /* Local count */
					dhd_sendpkt(dhdp, ifidx, pktbuf);
					continue;
				}
			} else {
				if ((ntoh16(eh->ether_type) != ETHER_TYPE_IAPP_L2_UPDATE)) {
					void *npktbuf = NULL;
					/*
					* If host_sfhllc_supported enabled, do skb_copy as SFHLLC
					* header will be inserted during Tx, due to which network
					* stack will not decode the Rx packet.
					* Else PKTDUP(skb_clone) is enough.
					*/
					if (dhdp->host_sfhllc_supported) {
						npktbuf = skb_copy(skb, GFP_ATOMIC);
					} else {
						npktbuf = PKTDUP(dhdp->osh, pktbuf);
					}
					if (npktbuf != NULL) {
						dhdp->rx_forward++; /* Local count */
						dhd_sendpkt(dhdp, ifidx, npktbuf);
					}
				}
			}
		}
#endif /* PCIE_FULL_DONGLE */
#endif /* BCM_ROUTER_DHD */
#ifdef DHD_POST_EAPOL_M1_AFTER_ROAM_EVT
		if ((IS_STA_IFACE(ndev_to_wdev(ifp->net)) || (IS_P2P_GC(ndev_to_wdev(ifp->net)))) &&
			(ifp->recv_reassoc_evt == TRUE) && (ifp->post_roam_evt == FALSE) &&
			(dhd_is_4way_msg((char *)(skb->data)) == EAPOL_4WAY_M1)) {
				DHD_ERROR(("%s: Reassoc is in progress. "
					"Drop EAPOL M1 frame\n", __FUNCTION__));
				PKTFREE(dhdp->osh, pktbuf, FALSE);
				continue;
		}
#endif /* DHD_POST_EAPOL_M1_AFTER_ROAM_EVT */
#ifdef WLEASYMESH
		if ((dhdp->conf->fw_type == FW_TYPE_EZMESH) &&
				(ntoh16(eh->ether_type) != ETHER_TYPE_BRCM)) {
			uint16 * da = (uint16 *)(eh->ether_dhost);
			ASSERT(ISALIGNED(da, 2));

			/* XXX: Special handling for 1905 messages
			 * if DA matches with configured 1905 AL MAC addresses
			 * bypass fwder and foward it to linux stack
			 */
			if (ntoh16(eh->ether_type) == ETHER_TYPE_1905_1) {
				if (!eacmp(da, ifp->_1905_al_ucast) || !eacmp(da, ifp->_1905_al_mcast)) {
					//skb->fwr_flood = 0;
				} else {
					//skb->fwr_flood = 1;
				}
			}
		}
#endif /* WLEASYMESH */
		/* Get the protocol, maintain skb around eth_type_trans()
		 * The main reason for this hack is for the limitation of
		 * Linux 2.4 where 'eth_type_trans' uses the 'net->hard_header_len'
		 * to perform skb_pull inside vs ETH_HLEN. Since to avoid
		 * coping of the packet coming from the network stack to add
		 * BDC, Hardware header etc, during network interface registration
		 * we set the 'net->hard_header_len' to ETH_HLEN + extra space required
		 * for BDC, Hardware header etc. and not just the ETH_HLEN
		 */
		eth = skb->data;
		len = skb->len;
		dump_data = skb->data;
		protocol = (skb->data[12] << 8) | skb->data[13];

		if (protocol == ETHER_TYPE_802_1X) {
			DBG_EVENT_LOG(dhdp, WIFI_EVENT_DRIVER_EAPOL_FRAME_RECEIVED);
#if defined(WL_CFG80211) && defined(WL_WPS_SYNC)
			wl_handle_wps_states(ifp->net, dump_data, len, FALSE);
#endif /* WL_CFG80211 && WL_WPS_SYNC */
#ifdef DHD_4WAYM4_FAIL_DISCONNECT
			if (dhd_is_4way_msg((uint8 *)(skb->data)) == EAPOL_4WAY_M3) {
				OSL_ATOMIC_SET(dhdp->osh, &ifp->m4state, M3_RXED);
			}
#endif /* DHD_4WAYM4_FAIL_DISCONNECT */
#ifdef EAPOL_RESEND
			wl_ext_release_eapol_txpkt(dhdp, ifidx, TRUE);
#endif /* EAPOL_RESEND */
		}
		dhd_handle_pktdata(dhdp, ifidx, skb, dump_data, FALSE,
			len, NULL, NULL, NULL, FALSE, pkt_wake, TRUE);
#if defined(DHD_WAKE_STATUS) && defined(DHD_WAKEPKT_DUMP)
		if (pkt_wake) {
			DHD_ERROR(("##### dhdpcie_host_wake caused by packets\n"));
			dhd_prhex("[wakepkt_dump]", (char*)dump_data, MIN(len, 64), DHD_RPM_VAL);
			DHD_ERROR(("config check in_suspend: %d\n", dhdp->in_suspend));
#ifdef ARP_OFFLOAD_SUPPORT
			DHD_ERROR(("arp hmac_update:%d \n", dhdp->hmac_updated));
#endif /* ARP_OFFLOAD_SUPPORT */
#if defined(DHD_WAKEPKT_SET_MARK) && defined(CONFIG_NF_CONNTRACK_MARK)
			PKTMARK(skb) |= 0x80000000;
#endif /* DHD_WAKEPKT_SET_MARK && CONFIG_NF_CONNTRACK_MARK */
		}
#endif /* DHD_WAKE_STATUS && DHD_WAKEPKT_DUMP */

		skb->protocol = eth_type_trans(skb, skb->dev);

		if (skb->pkt_type == PACKET_MULTICAST) {
			dhd->pub.rx_multicast++;
			ifp->stats.multicast++;
		}

		skb->data = eth;
		skb->len = len;

		/* TODO: XXX: re-look into dropped packets. */
		DHD_DBG_PKT_MON_RX(dhdp, skb, FRAME_TYPE_ETHERNET_II);
		/* Strip header, count, deliver upward */
		skb_pull(skb, ETH_HLEN);

#ifdef ENABLE_WAKEUP_PKT_DUMP
		if (dhd_mmc_wake) {
			DHD_INFO(("wake_pkt %s(%d)\n", __FUNCTION__, __LINE__));
			prhex("wake_pkt", (char*) eth, MIN(len, 64));

			update_wake_pkt_info(skb);
			DHD_ERROR(("wake_pkt %s(%d), raw_info=0x%016llx\n",
				__FUNCTION__, __LINE__, temp_raw))
#ifdef CONFIG_IRQ_HISTORY
			add_irq_history(0, "WIFI");
#endif
			dhd_mmc_wake = FALSE;
		}
#endif /* ENABLE_WAKEUP_PKT_DUMP */

		/* Process special event packets and then discard them */
		/* XXX Decide on a better way to fit this in */
		memset(&event, 0, sizeof(event));

		if (ntoh16(skb->protocol) == ETHER_TYPE_BRCM) {
			bcm_event_msg_u_t evu;
			int ret_event, event_type;
			void *pkt_data = skb_mac_header(skb);

			ret_event = wl_host_event_get_data(pkt_data, len, &evu);

			if (ret_event != BCME_OK) {
				DHD_ERROR(("%s: wl_host_event_get_data err = %d\n",
					__FUNCTION__, ret_event));
				PKTFREE_CTRLBUF(dhdp->osh, pktbuf, FALSE);
				continue;
			}

			memcpy(&event, &evu.event, sizeof(wl_event_msg_t));
			event_type = ntoh32_ua((void *)&event.event_type);
#ifdef SHOW_LOGTRACE
			/* Event msg printing is called from dhd_rx_frame which is in Tasklet
			 * context in case of PCIe FD, in case of other bus this will be from
			 * DPC context. If we get bunch of events from Dongle then printing all
			 * of them from Tasklet/DPC context that too in data path is costly.
			 * Also in the new Dongle SW(4359, 4355 onwards) console prints too come as
			 * events with type WLC_E_TRACE.
			 * We'll print this console logs from the WorkQueue context by enqueing SKB
			 * here and Dequeuing will be done in WorkQueue and will be freed only if
			 * logtrace_pkt_sendup is true
			 */
			if (event_type == WLC_E_TRACE) {
				DHD_TRACE(("%s: WLC_E_TRACE\n", __FUNCTION__));
				dhd_event_logtrace_enqueue(dhdp, ifidx, pktbuf);
				continue;
			}
#endif /* SHOW_LOGTRACE */

			ret_event = dhd_wl_host_event(dhd, ifidx, pkt_data, len, &event, &data);

			wl_event_to_host_order(&event);
#if defined(OEM_ANDROID)
			if (!tout_ctrl)
				tout_ctrl = DHD_PACKET_TIMEOUT_MS;
#endif /* OEM_ANDROID */

#if (defined(OEM_ANDROID) && defined(PNO_SUPPORT))
			if (event_type == WLC_E_PFN_NET_FOUND) {
				/* enforce custom wake lock to garantee that Kernel not suspended */
				tout_ctrl = CUSTOM_PNO_EVENT_LOCK_xTIME * DHD_PACKET_TIMEOUT_MS;
			}
#endif /* PNO_SUPPORT */
			if (numpkt != 1) {
				DHD_TRACE(("%s: Got BRCM event packet in a chained packet.\n",
				__FUNCTION__));
			}

#ifdef DHD_WAKE_STATUS
			if (unlikely(pkt_wake)) {
#ifdef DHD_WAKE_EVENT_STATUS
				if (event.event_type < WLC_E_LAST) {
#ifdef CUSTOM_WAKE_REASON_STATS
					if (wcp->rc_event_idx == MAX_WAKE_REASON_STATS) {
						wcp->rc_event_idx = 0;
					}
					DHD_ERROR(("[pkt_wake] event id %u = %s\n",
						event.event_type,
						bcmevent_get_name(event.event_type)));
					wcp->rc_event[wcp->rc_event_idx] = event.event_type;
					wcp->rc_event_idx++;
#else
					wcp->rc_event[event.event_type]++;
#endif /* CUSTOM_WAKE_REASON_STATS */
					wcp->rcwake++;
					pkt_wake = 0;
				}
#endif /* DHD_WAKE_EVENT_STATUS */
			}
#endif /* DHD_WAKE_STATUS */

			/* For delete virtual interface event, wl_host_event returns positive
			 * i/f index, do not proceed. just free the pkt.
			 */
			if ((event_type == WLC_E_IF) && (ret_event > 0)) {
				DHD_ERROR(("%s: interface is deleted. Free event packet\n",
				__FUNCTION__));
				PKTFREE_CTRLBUF(dhdp->osh, pktbuf, FALSE);
				continue;
			}

			/*
			 * For the event packets, there is a possibility
			 * of ifidx getting modifed.Thus update the ifp
			 * once again.
			 */
			ASSERT(ifidx < DHD_MAX_IFS);
			ifp = dhd->iflist[ifidx];
#ifndef PROP_TXSTATUS_VSDB
			if (!(ifp && ifp->net && (ifp->net->reg_state == NETREG_REGISTERED)))
#else
			if (!(ifp && ifp->net && (ifp->net->reg_state == NETREG_REGISTERED) &&
				dhd->pub.up))
#endif /* PROP_TXSTATUS_VSDB */
			{
				DHD_ERROR(("%s: net device is NOT registered. drop event packet\n",
				__FUNCTION__));
				PKTFREE_CTRLBUF(dhdp->osh, pktbuf, FALSE);
				continue;
			}

#ifdef SENDPROB
			if (dhdp->wl_event_enabled ||
				(dhdp->recv_probereq && (event.event_type == WLC_E_PROBREQ_MSG)))
#else
			if (dhdp->wl_event_enabled)
#endif
			{
#ifdef DHD_USE_STATIC_CTRLBUF
				/* If event bufs are allocated via static buf pool
				 * and wl events are enabled, make a copy, free the
				 * local one and send the copy up.
				 */
				struct sk_buff *nskb = skb_copy(skb, GFP_ATOMIC);
				/* Copy event and send it up */
				PKTFREE_STATIC(dhdp->osh, pktbuf, FALSE);
				if (nskb) {
					skb = nskb;
				} else {
					DHD_ERROR(("skb clone failed. dropping event.\n"));
					continue;
				}
#endif /* DHD_USE_STATIC_CTRLBUF */
			} else {
				/* If event enabled not explictly set, drop events */
				PKTFREE_CTRLBUF(dhdp->osh, pktbuf, FALSE);
				continue;
			}
		} else {
#if defined(OEM_ANDROID)
			tout_rx = DHD_PACKET_TIMEOUT_MS;

			/* Override rx wakelock timeout to give hostapd enough time
			 * to process retry or error handling for handshake
			 */
			if (IS_AP_IFACE(ndev_to_wdev(ifp->net)) &&
				ntoh16(skb->protocol) == ETHER_TYPE_802_1X) {
				if (dhd_is_4way_msg(dump_data) == EAPOL_4WAY_M2) {
					tout_rx = DHD_HANDSHAKE_TIMEOUT_MS;
				}
			}
#endif /* OEM_ANDROID */

#ifdef PROP_TXSTATUS
			dhd_wlfc_save_rxpath_ac_time(dhdp, (uint8)PKTPRIO(skb));
#endif /* PROP_TXSTATUS */

#ifdef DHD_WAKE_STATUS
			if (unlikely(pkt_wake)) {
				wcp->rxwake++;
#ifdef DHD_WAKE_RX_STATUS

				if (ntoh16(skb->protocol) == ETHER_TYPE_ARP) /* ARP */
					wcp->rx_arp++;
				if (ntoh16(skb->protocol) == ETHER_TYPE_IP) {
					struct iphdr *ipheader = NULL;
					ipheader = (struct iphdr*)(skb->data);
					if (ipheader && ipheader->protocol == IPPROTO_ICMP) {
						wcp->rx_icmp++;
					}
				}
				if (dump_data[0] == 0xFF) { /* Broadcast */
					wcp->rx_bcast++;
				} else if (dump_data[0] & 0x01) { /* Multicast */
					wcp->rx_mcast++;
					if (ntoh16(skb->protocol) == ETHER_TYPE_IPV6) {
					    wcp->rx_multi_ipv6++;
					    if ((skb->len > ETHER_ICMP6_HEADER) &&
					        (dump_data[ETHER_ICMP6_HEADER] == IPPROTO_ICMPV6)) {
					        wcp->rx_icmpv6++;
					        if (skb->len > ETHER_ICMPV6_TYPE) {
					            switch (dump_data[ETHER_ICMPV6_TYPE]) {
					            case NDISC_ROUTER_ADVERTISEMENT:
					                wcp->rx_icmpv6_ra++;
					                break;
					            case NDISC_NEIGHBOUR_ADVERTISEMENT:
					                wcp->rx_icmpv6_na++;
					                break;
					            case NDISC_NEIGHBOUR_SOLICITATION:
					                wcp->rx_icmpv6_ns++;
					                break;
					            }
					        }
					    }
					} else if (dump_data[2] == 0x5E) {
						wcp->rx_multi_ipv4++;
					} else {
						wcp->rx_multi_other++;
					}
				} else { /* Unicast */
					wcp->rx_ucast++;
				}
#undef ETHER_ICMP6_HEADER
#undef ETHER_IPV6_SADDR
#undef ETHER_IPV6_DAADR
#undef ETHER_ICMPV6_TYPE
#ifdef DHD_WAKE_STATUS_PRINT
				dhd_dump_wake_status(dhdp, wcp, (struct ether_header *)eth);
#endif /* DHD_WAKE_STATUS_PRINT */
#endif /* DHD_WAKE_RX_STATUS */
				pkt_wake = 0;
			}
#endif /* DHD_WAKE_STATUS */
		}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
		ifp->net->last_rx = jiffies;
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0) */

		if (ntoh16(skb->protocol) != ETHER_TYPE_BRCM) {
			dhdp->dstats.rx_bytes += skb->len;
			dhdp->rx_bytes += skb->len;
			dhdp->rx_packets++; /* Local count */
			ifp->stats.rx_bytes += skb->len;
			ifp->stats.rx_packets++;
		}

		/* XXX WL here makes sure data is 4-byte aligned? */
		if (in_interrupt()) {
			bcm_object_trace_opr(skb, BCM_OBJDBG_REMOVE,
				__FUNCTION__, __LINE__);
#if defined(DHD_LB_RXP)
#ifdef ENABLE_DHD_GRO
			/* The pktlog module clones a skb using skb_clone and
			 * stores the skb point to the ring buffer of the pktlog module.
			 * Once the buffer is full,
			 * the PKTFREE is called for removing the oldest skb.
			 * The kernel panic occurred when the pktlog module free
			 * the rx frame handled by napi_gro_receive().
			 * It is a fix code that DHD don't use napi_gro_receive() for
			 * the packet used in pktlog module.
			 */
			if (dhd_gro_enable && !skb_cloned(skb) &&
				ntoh16(skb->protocol) != ETHER_TYPE_BRCM) {
				napi_gro_receive(&dhd->rx_napi_struct, skb);
			} else {
#if defined(WL_MONITOR) && defined(BCMSDIO)
				if (dhd_monitor_enabled(dhdp, ifidx))
					dhd_rx_mon_pkt_sdio(dhdp, skb, ifidx);
				else
#endif /* WL_MONITOR && BCMSDIO */
				netif_receive_skb(skb);
			}
#else
#if defined(WL_MONITOR) && defined(BCMSDIO)
			if (dhd_monitor_enabled(dhdp, ifidx))
				dhd_rx_mon_pkt_sdio(dhdp, skb, ifidx);
			else
#endif /* WL_MONITOR && BCMSDIO */
			netif_receive_skb(skb);
#endif /* ENABLE_DHD_GRO */
#else /* !defined(DHD_LB_RXP) */
			netif_rx(skb);
#endif /* !defined(DHD_LB_RXP) */
		} else {
			if (dhd->rxthread_enabled) {
				if (!skbhead)
					skbhead = skb;
				else
					PKTSETNEXT(dhdp->osh, skbprev, skb);
				skbprev = skb;
			} else {

				/* If the receive is not processed inside an ISR,
				 * the softirqd must be woken explicitly to service
				 * the NET_RX_SOFTIRQ.	In 2.6 kernels, this is handled
				 * by netif_rx_ni(), but in earlier kernels, we need
				 * to do it manually.
				 */
				bcm_object_trace_opr(skb, BCM_OBJDBG_REMOVE,
					__FUNCTION__, __LINE__);

#if defined(BCMPCIE) && defined(DHDTCPACK_SUPPRESS)
				dhd_tcpack_suppress_set(&dhd->pub, TCPACK_SUP_OFF);
#endif /* BCMPCIE && DHDTCPACK_SUPPRESS */
#if defined(DHD_LB_RXP)
#ifdef ENABLE_DHD_GRO
				if (dhd_gro_enable && !skb_cloned(skb) &&
					ntoh16(skb->protocol) != ETHER_TYPE_BRCM) {
					napi_gro_receive(&dhd->rx_napi_struct, skb);
				} else {
					netif_receive_skb(skb);
				}
#else
				netif_receive_skb(skb);
#endif /* ENABLE_DHD_GRO */
#else /* !defined(DHD_LB_RXP) */
				dhd_netif_rx_ni(skb);
#endif /* !defined(DHD_LB_RXP) */
			}
		}
	}

	if (dhd->rxthread_enabled && skbhead)
		dhd_sched_rxf(dhdp, skbhead);

#if defined(OEM_ANDROID)
	DHD_OS_WAKE_LOCK_RX_TIMEOUT_ENABLE(dhdp, tout_rx);
	DHD_OS_WAKE_LOCK_CTRL_TIMEOUT_ENABLE(dhdp, tout_ctrl);

	/* To immediately notify the host that timeout is enabled */
	DHD_OS_WAKE_LOCK_TIMEOUT(dhdp);
#endif /* OEM_ANDROID */
}

int
dhd_rxf_thread(void *data)
{
	tsk_ctl_t *tsk = (tsk_ctl_t *)data;
	dhd_info_t *dhd = (dhd_info_t *)tsk->parent;
#if defined(WAIT_DEQUEUE)
#define RXF_WATCHDOG_TIME 250 /* BARK_TIME(1000) /  */
	ulong watchdogTime = OSL_SYSUPTIME(); /* msec */
#endif
	dhd_pub_t *pub = &dhd->pub;

	/* This thread doesn't need any user-level access,
	 * so get rid of all our resources
	 */
	if (dhd_rxf_prio > 0)
	{
		struct sched_param param;
		param.sched_priority = (dhd_rxf_prio < MAX_RT_PRIO)?dhd_rxf_prio:(MAX_RT_PRIO-1);
		setScheduler(current, SCHED_FIFO, &param);
	}

#ifdef CUSTOM_SET_CPUCORE
	dhd->pub.current_rxf = current;
#endif /* CUSTOM_SET_CPUCORE */
	/* Run until signal received */
	while (1) {
		if (dhd->pub.conf->rxf_cpucore >= 0) {
			printf("%s: set rxf_cpucore %d\n", __FUNCTION__, dhd->pub.conf->rxf_cpucore);
			set_cpus_allowed_ptr(current, cpumask_of(dhd->pub.conf->rxf_cpucore));
			dhd->pub.conf->rxf_cpucore = -1;
		}
		if (down_interruptible(&tsk->sema) == 0) {
			void *skb;
#ifdef ENABLE_ADAPTIVE_SCHED
			dhd_sched_policy(dhd_rxf_prio);
#endif /* ENABLE_ADAPTIVE_SCHED */

			SMP_RD_BARRIER_DEPENDS();

			if (tsk->terminated) {
				DHD_OS_WAKE_UNLOCK(pub);
				break;
			}
			skb = dhd_rxf_dequeue(pub);

			if (skb == NULL) {
				continue;
			}
			while (skb) {
				void *skbnext = PKTNEXT(pub->osh, skb);
				PKTSETNEXT(pub->osh, skb, NULL);
				bcm_object_trace_opr(skb, BCM_OBJDBG_REMOVE,
					__FUNCTION__, __LINE__);
#if defined(WL_MONITOR) && defined(BCMSDIO)
				if (dhd_monitor_enabled(pub, 0))
					dhd_rx_mon_pkt_sdio(pub, skb, 0);
				else
#endif /* WL_MONITOR && BCMSDIO */
				dhd_netif_rx_ni(skb);
				skb = skbnext;
			}
#if defined(WAIT_DEQUEUE)
			if (OSL_SYSUPTIME() - watchdogTime > RXF_WATCHDOG_TIME) {
				OSL_SLEEP(1);
				watchdogTime = OSL_SYSUPTIME();
			}
#endif

			DHD_OS_WAKE_UNLOCK(pub);
		} else {
			break;
		}
	}
	complete_and_exit(&tsk->completed, 0);
}

void
dhd_sched_rxf(dhd_pub_t *dhdp, void *skb)
{
	dhd_info_t *dhd = (dhd_info_t *)dhdp->info;
#ifdef RXF_DEQUEUE_ON_BUSY
	int ret = BCME_OK;
	int retry = 2;
#endif /* RXF_DEQUEUE_ON_BUSY */

	DHD_OS_WAKE_LOCK(dhdp);

	DHD_TRACE(("dhd_sched_rxf: Enter\n"));
#ifdef RXF_DEQUEUE_ON_BUSY
	do {
		ret = dhd_rxf_enqueue(dhdp, skb);
		if (ret == BCME_OK || ret == BCME_ERROR)
			break;
		else
			OSL_SLEEP(50); /* waiting for dequeueing */
	} while (retry-- > 0);

	if (retry <= 0 && ret == BCME_BUSY) {
		void *skbp = skb;

		while (skbp) {
			void *skbnext = PKTNEXT(dhdp->osh, skbp);
			PKTSETNEXT(dhdp->osh, skbp, NULL);
			bcm_object_trace_opr(skb, BCM_OBJDBG_REMOVE,
				__FUNCTION__, __LINE__);
			dhd_netif_rx_ni(skbp);
			skbp = skbnext;
		}
		DHD_ERROR(("send skb to kernel backlog without rxf_thread\n"));
	} else {
		if (dhd->thr_rxf_ctl.thr_pid >= 0) {
			up(&dhd->thr_rxf_ctl.sema);
		}
	}
#else /* RXF_DEQUEUE_ON_BUSY */
	do {
		if (dhd_rxf_enqueue(dhdp, skb) == BCME_OK)
			break;
	} while (1);
	if (dhd->thr_rxf_ctl.thr_pid >= 0) {
		up(&dhd->thr_rxf_ctl.sema);
	} else {
		DHD_OS_WAKE_UNLOCK(dhdp);
	}
	return;
#endif /* RXF_DEQUEUE_ON_BUSY */
}

#ifdef WL_MONITOR
#ifdef BCMSDIO
void
dhd_rx_mon_pkt_sdio(dhd_pub_t *dhdp, void *pkt, int ifidx)
{
	dhd_info_t *dhd = (dhd_info_t *)dhdp->info;

	if (!dhd->monitor_skb) {
		if ((dhd->monitor_skb = PKTTONATIVE(dhdp->osh, pkt)) == NULL)
			return;
	}

	if (dhd->monitor_type && dhd->monitor_dev)
		dhd->monitor_skb->dev = dhd->monitor_dev;
	else {
		PKTFREE(dhdp->osh, pkt, FALSE);
		dhd->monitor_skb = NULL;
		return;
	}

	dhd->monitor_skb->protocol =
		eth_type_trans(dhd->monitor_skb, dhd->monitor_skb->dev);
	dhd->monitor_len = 0;

	netif_rx_ni(dhd->monitor_skb);

	dhd->monitor_skb = NULL;
}
#elif defined(BCMPCIE)
void
dhd_rx_mon_pkt(dhd_pub_t *dhdp, host_rxbuf_cmpl_t* msg, void *pkt, int ifidx)
{
	dhd_info_t *dhd = (dhd_info_t *)dhdp->info;
#ifdef HOST_RADIOTAP_CONV
	if (dhd->host_radiotap_conv) {
		uint16 len = 0, offset = 0;
		monitor_pkt_info_t pkt_info;

		memcpy(&pkt_info.marker, &msg->marker, sizeof(msg->marker));
		memcpy(&pkt_info.ts, &msg->ts, sizeof(monitor_pkt_ts_t));

		if (!dhd->monitor_skb) {
			if ((dhd->monitor_skb = dev_alloc_skb(MAX_MON_PKT_SIZE)) == NULL)
				return;
		}

		len = bcmwifi_monitor(dhd->monitor_info, &pkt_info, PKTDATA(dhdp->osh, pkt),
			PKTLEN(dhdp->osh, pkt), PKTDATA(dhdp->osh, dhd->monitor_skb), &offset);

		if (dhd->monitor_type && dhd->monitor_dev)
			dhd->monitor_skb->dev = dhd->monitor_dev;
		else {
			PKTFREE(dhdp->osh, pkt, FALSE);
			dev_kfree_skb(dhd->monitor_skb);
			return;
		}

		PKTFREE(dhdp->osh, pkt, FALSE);

		if (!len) {
			return;
		}

		skb_put(dhd->monitor_skb, len);
		skb_pull(dhd->monitor_skb, offset);

		dhd->monitor_skb->protocol = eth_type_trans(dhd->monitor_skb,
			dhd->monitor_skb->dev);
	}
	else
#endif /* HOST_RADIOTAP_CONV */
	{
		uint8 amsdu_flag = (msg->flags & BCMPCIE_PKT_FLAGS_MONITOR_MASK) >>
			BCMPCIE_PKT_FLAGS_MONITOR_SHIFT;
		switch (amsdu_flag) {
			case BCMPCIE_PKT_FLAGS_MONITOR_NO_AMSDU:
			default:
				if (!dhd->monitor_skb) {
					if ((dhd->monitor_skb = PKTTONATIVE(dhdp->osh, pkt))
						== NULL)
						return;
				}
				if (dhd->monitor_type && dhd->monitor_dev)
					dhd->monitor_skb->dev = dhd->monitor_dev;
				else {
					PKTFREE(dhdp->osh, pkt, FALSE);
					dhd->monitor_skb = NULL;
					return;
				}
				dhd->monitor_skb->protocol =
					eth_type_trans(dhd->monitor_skb, dhd->monitor_skb->dev);
				dhd->monitor_len = 0;
				break;

			case BCMPCIE_PKT_FLAGS_MONITOR_FIRST_PKT:
				if (!dhd->monitor_skb) {
					if ((dhd->monitor_skb = dev_alloc_skb(MAX_MON_PKT_SIZE))
						== NULL)
						return;
					dhd->monitor_len = 0;
				}
				if (dhd->monitor_type && dhd->monitor_dev)
					dhd->monitor_skb->dev = dhd->monitor_dev;
				else {
					PKTFREE(dhdp->osh, pkt, FALSE);
					dev_kfree_skb(dhd->monitor_skb);
					return;
				}
				memcpy(PKTDATA(dhdp->osh, dhd->monitor_skb),
				PKTDATA(dhdp->osh, pkt), PKTLEN(dhdp->osh, pkt));
				dhd->monitor_len = PKTLEN(dhdp->osh, pkt);
				PKTFREE(dhdp->osh, pkt, FALSE);
				return;

			case BCMPCIE_PKT_FLAGS_MONITOR_INTER_PKT:
				memcpy(PKTDATA(dhdp->osh, dhd->monitor_skb) + dhd->monitor_len,
				PKTDATA(dhdp->osh, pkt), PKTLEN(dhdp->osh, pkt));
				dhd->monitor_len += PKTLEN(dhdp->osh, pkt);
				PKTFREE(dhdp->osh, pkt, FALSE);
				return;

			case BCMPCIE_PKT_FLAGS_MONITOR_LAST_PKT:
				memcpy(PKTDATA(dhdp->osh, dhd->monitor_skb) + dhd->monitor_len,
				PKTDATA(dhdp->osh, pkt), PKTLEN(dhdp->osh, pkt));
				dhd->monitor_len += PKTLEN(dhdp->osh, pkt);
				PKTFREE(dhdp->osh, pkt, FALSE);
				skb_put(dhd->monitor_skb, dhd->monitor_len);
				dhd->monitor_skb->protocol =
					eth_type_trans(dhd->monitor_skb, dhd->monitor_skb->dev);
				dhd->monitor_len = 0;
				break;
		}
	}

	if (skb_headroom(dhd->monitor_skb) < ETHER_HDR_LEN) {
		struct sk_buff *skb2;

		DHD_INFO(("%s: insufficient headroom\n",
		          dhd_ifname(&dhd->pub, ifidx)));

		skb2 = skb_realloc_headroom(dhd->monitor_skb, ETHER_HDR_LEN);

		dev_kfree_skb(dhd->monitor_skb);
		if ((dhd->monitor_skb = skb2) == NULL) {
			DHD_ERROR(("%s: skb_realloc_headroom failed\n",
			           dhd_ifname(&dhd->pub, ifidx)));
			return;
		}
	}
	PKTPUSH(dhd->pub.osh, dhd->monitor_skb, ETHER_HDR_LEN);

	/* XXX WL here makes sure data is 4-byte aligned? */
	if (in_interrupt()) {
		bcm_object_trace_opr(skb, BCM_OBJDBG_REMOVE,
			__FUNCTION__, __LINE__);
		netif_rx(dhd->monitor_skb);
	} else {
		/* If the receive is not processed inside an ISR,
		 * the softirqd must be woken explicitly to service
		 * the NET_RX_SOFTIRQ.	In 2.6 kernels, this is handled
		 * by netif_rx_ni(), but in earlier kernels, we need
		 * to do it manually.
		 */
		bcm_object_trace_opr(dhd->monitor_skb, BCM_OBJDBG_REMOVE,
			__FUNCTION__, __LINE__);

		dhd_netif_rx_ni(dhd->monitor_skb);
	}

	dhd->monitor_skb = NULL;

#if defined(OEM_ANDROID)
	DHD_OS_WAKE_LOCK_RX_TIMEOUT_ENABLE(dhdp, DHD_MONITOR_TIMEOUT_MS);
	DHD_OS_WAKE_LOCK_TIMEOUT(dhdp);
#endif /* OEM_ANDROID */
}
#endif /* PCIE_FULL_DONGLE */
#endif /* WL_MONITOR */

static int
dhd_wl_host_event(dhd_info_t *dhd, int ifidx, void *pktdata, uint16 pktlen,
	wl_event_msg_t *event, void **data)
{
	int bcmerror = 0;
#ifdef WL_CFG80211
	unsigned long flags = 0;
#endif /* WL_CFG80211 */
	ASSERT(dhd != NULL);

#ifdef SHOW_LOGTRACE
	bcmerror = wl_process_host_event(&dhd->pub, &ifidx, pktdata, pktlen, event, data,
		&dhd->event_data);
#else
	bcmerror = wl_process_host_event(&dhd->pub, &ifidx, pktdata, pktlen, event, data,
		NULL);
#endif /* SHOW_LOGTRACE */
	if (unlikely(bcmerror != BCME_OK)) {
		return bcmerror;
	}

	if (ntoh32(event->event_type) == WLC_E_IF) {
		/* WLC_E_IF event types are consumed by wl_process_host_event.
		 * For ifadd/del ops, the netdev ptr may not be valid at this
		 * point. so return before invoking cfg80211/wext handlers.
		 */
		return BCME_OK;
	}

#ifdef WL_EVENT
	wl_ext_event_send(dhd->pub.event_params, event, *data);
#endif

#ifdef WL_CFG80211
	if (dhd->iflist[ifidx]->net) {
		DHD_UP_LOCK(&dhd->pub.up_lock, flags);
		if (dhd->pub.up) {
			wl_cfg80211_event(dhd->iflist[ifidx]->net, event, *data);
		}
		DHD_UP_UNLOCK(&dhd->pub.up_lock, flags);
	}
#endif /* defined(WL_CFG80211) */

	return (bcmerror);
}

void
dhd_os_sdlock_rxq(dhd_pub_t *pub)
{
}

void
dhd_os_sdunlock_rxq(dhd_pub_t *pub)
{
}

static void
dhd_os_rxflock(dhd_pub_t *pub)
{
	dhd_info_t *dhd;

	dhd = (dhd_info_t *)(pub->info);
	spin_lock_bh(&dhd->rxf_lock);

}

static void
dhd_os_rxfunlock(dhd_pub_t *pub)
{
	dhd_info_t *dhd;

	dhd = (dhd_info_t *)(pub->info);
	spin_unlock_bh(&dhd->rxf_lock);
}

#ifdef PKT_FILTER_SUPPORT
int net_os_rxfilter_add_remove(struct net_device *dev, int add_remove, int num)
{
	int ret = 0;

#ifndef GAN_LITE_NAT_KEEPALIVE_FILTER
	dhd_info_t *dhd = DHD_DEV_INFO(dev);

	if (!dhd_master_mode)
		add_remove = !add_remove;
	DHD_INFO(("%s: add_remove = %d, num = %d\n", __FUNCTION__, add_remove, num));
	if (!dhd || (num == DHD_UNICAST_FILTER_NUM)) {
		return 0;
	}

#ifdef BLOCK_IPV6_PACKET
	/* customer want to use NO IPV6 packets only */
	if (num == DHD_MULTICAST6_FILTER_NUM) {
		return 0;
	}
#endif /* BLOCK_IPV6_PACKET */

	if (num >= dhd->pub.pktfilter_count) {
		return -EINVAL;
	}

	ret = dhd_packet_filter_add_remove(&dhd->pub, add_remove, num);
#endif /* !GAN_LITE_NAT_KEEPALIVE_FILTER */

	return ret;
}
#endif /* PKT_FILTER_SUPPORT */

int dhd_os_wake_lock_rx_timeout_enable(dhd_pub_t *pub, int val)
{
	dhd_info_t *dhd = (dhd_info_t *)(pub->info);
	unsigned long flags;

	if (dhd && (dhd->dhd_state & DHD_ATTACH_STATE_WAKELOCKS_INIT)) {
		DHD_WAKE_SPIN_LOCK(&dhd->wakelock_spinlock, flags);
		if (val > dhd->wakelock_rx_timeout_enable)
			dhd->wakelock_rx_timeout_enable = val;
		DHD_WAKE_SPIN_UNLOCK(&dhd->wakelock_spinlock, flags);
	}
	return 0;
}

int net_os_wake_lock_rx_timeout_enable(struct net_device *dev, int val)
{
	dhd_info_t *dhd = DHD_DEV_INFO(dev);
	int ret = 0;

	if (dhd)
		ret = dhd_os_wake_lock_rx_timeout_enable(&dhd->pub, val);
	return ret;
}

#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
void
dhd_flush_rx_tx_wq(dhd_pub_t *dhdp)
{
	dhd_info_t * dhd;

	if (dhdp) {
		dhd = dhdp->info;
		if (dhd) {
			flush_workqueue(dhd->tx_wq);
			flush_workqueue(dhd->rx_wq);
		}
	}

	return;
}
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

#ifdef RX_PKT_POOL
void *
BCMFASTPATH(dhd_rxpool_pktget)(osl_t *osh, dhd_info_t *dhd, uint16 len)
{
	void *p = NULL;
	pkt_pool_t *rx_pool = &dhd->rx_pkt_pool;

	if (dhd->rx_pktpool_thread.thr_pid >= 0) {
		if (rx_pool->rxbuf_sz == len) {
			p = skb_dequeue(&rx_pool->skb_q);
			/* kick rx buffer mgmt thread to alloc more rx
			 * buffers into the pool
			 */
			binary_sema_up(&dhd->rx_pktpool_thread);
		} else {
			DHD_ERROR_RLMT(("%s: PKTGET_RX_POOL is called with length %u, "
				"but pkt pool created with length : %u\n", __FUNCTION__,
				len, rx_pool->rxbuf_sz));
		}
	} else {
		DHD_ERROR_RLMT(("%s: rx_pktpool_thread(%ld) not inited\n", __FUNCTION__,
			dhd->rx_pktpool_thread.thr_pid));
	}

	return p;
}

static int
dhd_rx_pktpool_thread(void *data)
{
	tsk_ctl_t *tsk = (tsk_ctl_t *)data;
	dhd_info_t *dhd = (dhd_info_t *)tsk->parent;
	dhd_pub_t *dhdp = (dhd_pub_t *)&dhd->pub;
	pkt_pool_t *rx_pool = &dhd->rx_pkt_pool;
	void *p = NULL;
	int qlen = 0;
	int num_attempts = 0;

	DHD_TRACE(("%s: STARTED...\n", __FUNCTION__));
	while (1) {
		if (!binary_sema_down(tsk)) {
			SMP_RD_BARRIER_DEPENDS();
			/* Check terminated before processing the items */
			if (tsk->terminated) {
				DHD_ERROR(("%s: task terminated\n", __FUNCTION__));
				goto exit;
			}
			/* Allocate rx buffer only after dongle
			 * handshake the actual rxbuf size.
			 */
			if (rx_pool->rxbuf_sz) {
				qlen = skb_queue_len(&rx_pool->skb_q);
				DHD_TRACE(("%s: before alloc - skb_q len=%u, max_size=%u "
					"rxbuf_sz : %u\n", __FUNCTION__, qlen, rx_pool->max_size,
					rx_pool->rxbuf_sz));
				num_attempts = 0;
				while (qlen < rx_pool->max_size) {
					p = PKTGET(dhdp->osh, rx_pool->rxbuf_sz, FALSE);
					if (!p) {
						DHD_ERROR_RLMT(("%s: pktget fails, resched...\n",
							__FUNCTION__));
						/* retry after some time to fetch packets
						 * if maximum attempts hit, stop
						 */
						num_attempts++;
						if (num_attempts >= RX_PKTPOOL_FETCH_MAX_ATTEMPTS) {
							DHD_ERROR_RLMT(("%s: max attempts to fetch"
								" exceeded.\n", __FUNCTION__));
							break;
						}
						OSL_SLEEP(RX_PKTPOOL_RESCHED_DELAY_MS);
					} else {
						skb_queue_tail(&rx_pool->skb_q, p);
					}
					qlen = skb_queue_len(&rx_pool->skb_q);
				}
				DHD_TRACE(("%s: after alloc - skb_q len=%u, max_size=%u \n",
					__FUNCTION__, qlen, rx_pool->max_size));
			}
		} else {
			DHD_ERROR_RLMT(("%s: unexpected break\n", __FUNCTION__));
			break;
		}
	}
exit:
	DHD_TRACE(("%s: EXITED...\n", __FUNCTION__));
	complete_and_exit(&tsk->completed, 0);
}

void
dhd_rx_pktpool_create(dhd_info_t *dhd, uint16 rxbuf_sz)
{
	pkt_pool_t *rx_pool = &dhd->rx_pkt_pool;
	rx_pool->rxbuf_sz = rxbuf_sz;
	binary_sema_up(&dhd->rx_pktpool_thread);
}

void
dhd_rx_pktpool_init(dhd_info_t *dhd)
{
	pkt_pool_t *rx_pool = &dhd->rx_pkt_pool;

	skb_queue_head_init(&rx_pool->skb_q);
	rx_pool->max_size = MAX_RX_PKT_POOL;
	rx_pool->rxbuf_sz = 0;

	PROC_START(dhd_rx_pktpool_thread, dhd, &dhd->rx_pktpool_thread, 0, "dhd_rx_pktpool_thread");
}

void
dhd_rx_pktpool_deinit(dhd_info_t *dhd)
{
	pkt_pool_t *rx_pool = &dhd->rx_pkt_pool;
	tsk_ctl_t *tsk = &dhd->rx_pktpool_thread;

	if (tsk->parent && tsk->thr_pid >= 0) {
		PROC_STOP_USING_BINARY_SEMA(tsk);
	} else {
		DHD_ERROR(("%s: rx_pktpool_thread(%ld) not inited\n",
			__FUNCTION__, tsk->thr_pid));
	}
	/* skb list manipulation functions use internal
	 * spin lock, so no need to take separate lock
	 */
	skb_queue_purge(&rx_pool->skb_q);
	rx_pool->max_size = 0;
	DHD_ERROR(("%s: de-alloc'd rx buffers in pool \n",
		__FUNCTION__));
}
#endif /* RX_PKT_POOL */
