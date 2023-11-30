/*++

Copyright (c) 2021 Motor-comm Corporation. 
Confidential and Proprietary. All rights reserved.

This is Motor-comm Corporation NIC driver relevant files. Please don't copy, modify,
distribute without commercial permission.

--*/


#include <linux/netdevice.h>
#include <linux/tcp.h>
#include <linux/interrupt.h>
#include <linux/inetdevice.h>
#include <linux/inet.h>
#include <net/addrconf.h>

#include "fuxi-os.h"
#include "fuxi-gmac.h"
#include "fuxi-gmac-reg.h"


static int fxgmac_one_poll_rx(struct napi_struct *, int);
static int fxgmac_one_poll_tx(struct napi_struct *, int);
static int fxgmac_all_poll(struct napi_struct *, int);

unsigned int fxgmac_get_netdev_ip4addr(struct fxgmac_pdata *pdata)
{
    struct net_device *netdev = pdata->netdev;
    struct in_ifaddr *ifa;
    unsigned int ipval = 0xc0a801ca; //here just hard code to 192.168.1.202

    rcu_read_lock();
    /* we only get the first IPv4 addr. */
    ifa = rcu_dereference(netdev->ip_ptr->ifa_list);
    if(ifa) {
        /* binary ipv4 addr with __be */
        ipval = (unsigned int)ifa->ifa_address;

        DPRINTK("%s, netdev %s IPv4 address %pI4, mask: %pI4\n",__FUNCTION__, ifa->ifa_label, &ifa->ifa_address, &ifa->ifa_mask);
    }
    //	ifa = rcu_dereference(ifa->ifa_next); // more ipv4 addr
    rcu_read_unlock();

    return ipval;
}

unsigned char * fxgmac_get_netdev_ip6addr(struct fxgmac_pdata *pdata, unsigned char *ipval, unsigned char *ip6addr_solicited, unsigned int ifa_flag)
{
    struct net_device *netdev = pdata->netdev;
    struct inet6_dev *i6dev;
    struct inet6_ifaddr *ifp;
    unsigned char local_ipval[16] = {0};
    unsigned char solicited_ipval[16] = {0};
    struct in6_addr *addr_ip6 = (struct in6_addr *)local_ipval;
    struct in6_addr *addr_ip6_solicited = (struct in6_addr *)solicited_ipval;
    int err = -EADDRNOTAVAIL;

    //in6_pton("fe80::35c6:dd1b:9745:fc9b", -1, (u8*)ipval, -1, NULL); //here just hard code for default
    if(ipval) {
        addr_ip6 = (struct in6_addr *)ipval;
    }

    if(ip6addr_solicited) {
        addr_ip6_solicited = (struct in6_addr *)ip6addr_solicited;			
    }

    in6_pton("fe80::4808:8ffb:d93e:d753", -1, (u8*)addr_ip6, -1, NULL); //here just hard code for default

    if (ifa_flag & FXGMAC_NS_IFA_GLOBAL_UNICAST)
        DPRINTK ("%s FXGMAC_NS_IFA_GLOBAL_UNICAST is set, %x\n", __FUNCTION__, ifa_flag);

    if (ifa_flag & FXGMAC_NS_IFA_LOCAL_LINK)
        DPRINTK ("%s FXGMAC_NS_IFA_LOCAL_LINK is set, %x\n", __FUNCTION__, ifa_flag);

    rcu_read_lock();
    i6dev = __in6_dev_get(netdev);
    if (i6dev != NULL) {
        read_lock_bh(&i6dev->lock);
        list_for_each_entry(ifp, &i6dev->addr_list, if_list) {

            /* here we need only the ll addr, use scope to filter out it. */
            if (((ifa_flag & FXGMAC_NS_IFA_GLOBAL_UNICAST) && (ifp->scope != IFA_LINK)) || ((ifa_flag & FXGMAC_NS_IFA_LOCAL_LINK) && (ifp->scope == IFA_LINK)/* &&
                !(ifp->flags & IFA_F_TENTATIVE)*/)) {

                memcpy(addr_ip6, &ifp->addr, 16);
                addrconf_addr_solict_mult(addr_ip6, addr_ip6_solicited);
                err = 0;

                //DPRINTK("%s, netdev %s IPv6 local-link address %pI6\n",__FUNCTION__, netdev->name, addr_ip6);
                //DPRINTK("%s, netdev %s IPv6 solicited-node add %pI6\n",__FUNCTION__, netdev->name, addr_ip6_solicited);
                break;
            }
        }
        read_unlock_bh(&i6dev->lock);
    }
    rcu_read_unlock();

    if(err) DPRINTK("%s get ipv6 addr failed, use default.\n", __FUNCTION__);

    //DPRINTK("%s, netdev %s IPv6 local-link address %pI6\n",__FUNCTION__, netdev->name, addr_ip6);
    //DPRINTK("%s, netdev %s IPv6 solicited-node adr %pI6\n",__FUNCTION__, netdev->name, addr_ip6_solicited);

    return (err ? NULL : ipval);
}

static inline unsigned int fxgmac_tx_avail_desc(struct fxgmac_ring *ring)
{
    //return (ring->dma_desc_count - (ring->cur - ring->dirty));
    unsigned int avail;

    if (ring->dirty > ring->cur)
        avail = ring->dirty - ring->cur;
    else
        avail = ring->dma_desc_count - ring->cur + ring->dirty;

    return avail;
}

static inline unsigned int fxgmac_rx_dirty_desc(struct fxgmac_ring *ring)
{
    //return (ring->cur - ring->dirty);
    unsigned int dirty;

    if (ring->dirty <= ring->cur)
        dirty = ring->cur - ring->dirty;
    else
        dirty = ring->dma_desc_count - ring->dirty + ring->cur;

    return dirty;
}

static int fxgmac_maybe_stop_tx_queue(
            struct fxgmac_channel *channel,
            struct fxgmac_ring *ring,
            unsigned int count)
{
    struct fxgmac_pdata *pdata = channel->pdata;

    if (count > fxgmac_tx_avail_desc(ring)) {
        netif_info(pdata, drv, pdata->netdev,
                                "Tx queue stopped, not enough descriptors available\n");
        netif_stop_subqueue(pdata->netdev, channel->queue_index);
        ring->tx.queue_stopped = 1;

        /* If we haven't notified the hardware because of xmit_more
         * support, tell it now
         */
        if (ring->tx.xmit_more)
            pdata->hw_ops.tx_start_xmit(channel, ring);
        if(netif_msg_tx_done(pdata)) DPRINTK("about stop tx q, ret BUSY\n");

        return NETDEV_TX_BUSY;
    }

    return 0;
}

static void fxgmac_prep_vlan(struct sk_buff *skb,
                struct fxgmac_pkt_info *pkt_info)
{
    if (skb_vlan_tag_present(skb))
        pkt_info->vlan_ctag = skb_vlan_tag_get(skb);
}

static int fxgmac_prep_tso(struct sk_buff *skb,
                struct fxgmac_pkt_info *pkt_info)
{
    int ret;

    if (!FXGMAC_GET_REG_BITS(pkt_info->attributes,
                TX_PACKET_ATTRIBUTES_TSO_ENABLE_POS,
                TX_PACKET_ATTRIBUTES_TSO_ENABLE_LEN))
        return 0;

    ret = skb_cow_head(skb, 0);
    if (ret)
        return ret;

    pkt_info->header_len = skb_transport_offset(skb) + tcp_hdrlen(skb);
    pkt_info->tcp_header_len = tcp_hdrlen(skb);
    pkt_info->tcp_payload_len = skb->len - pkt_info->header_len;
    pkt_info->mss = skb_shinfo(skb)->gso_size;

    if(default_msg_level & NETIF_MSG_TX_DONE){
        DPRINTK("header_len=%u\n", pkt_info->header_len);
        DPRINTK("tcp_header_len=%u, tcp_payload_len=%u\n",
                pkt_info->tcp_header_len, pkt_info->tcp_payload_len);
        DPRINTK("mss=%u\n", pkt_info->mss);
    }
    /* Update the number of packets that will ultimately be transmitted
     * along with the extra bytes for each extra packet
     */
    pkt_info->tx_packets = skb_shinfo(skb)->gso_segs;
    pkt_info->tx_bytes += (pkt_info->tx_packets - 1) * pkt_info->header_len;

    return 0;
}

static int fxgmac_is_tso(struct sk_buff *skb)
{
    if (skb->ip_summed != CHECKSUM_PARTIAL)
        return 0;

    if (!skb_is_gso(skb))
        return 0;

    return 1;
}

static void fxgmac_prep_tx_pkt(struct fxgmac_pdata *pdata,
                struct fxgmac_ring *ring,
                struct sk_buff *skb,
                struct fxgmac_pkt_info *pkt_info)
{
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0) )
    skb_frag_t *frag;
#else
    struct skb_frag_struct *frag;
#endif
    unsigned int context_desc;
    unsigned int len;
    unsigned int i;

    pkt_info->skb = skb;

    context_desc = 0;
    pkt_info->desc_count = 0;

    pkt_info->tx_packets = 1;
    pkt_info->tx_bytes = skb->len;
    if(netif_msg_tx_done(pdata)) 
        DPRINTK ("fxgmac_prep_tx_pkt callin,pkt desc cnt=%d,skb len=%d, skbheadlen=%d\n", pkt_info->desc_count, skb->len, skb_headlen(skb));

    if (fxgmac_is_tso(skb)) {
        /* TSO requires an extra descriptor if mss is different */
        if (skb_shinfo(skb)->gso_size != ring->tx.cur_mss) {
            context_desc = 1;
            pkt_info->desc_count++;
        }
        if(netif_msg_tx_done(pdata))
            DPRINTK("fxgmac_is_tso=%d, ip_summed=%d,skb gso=%d\n",((skb->ip_summed == CHECKSUM_PARTIAL) && (skb_is_gso(skb)))?1:0, skb->ip_summed, skb_is_gso(skb)?1:0);

        /* TSO requires an extra descriptor for TSO header */
        pkt_info->desc_count++;

        pkt_info->attributes = FXGMAC_SET_REG_BITS(
                    pkt_info->attributes,
                    TX_PACKET_ATTRIBUTES_TSO_ENABLE_POS,
                    TX_PACKET_ATTRIBUTES_TSO_ENABLE_LEN,
                    1);
        pkt_info->attributes = FXGMAC_SET_REG_BITS(
                    pkt_info->attributes,
                    TX_PACKET_ATTRIBUTES_CSUM_ENABLE_POS,
                    TX_PACKET_ATTRIBUTES_CSUM_ENABLE_LEN,
                    1);
        if(netif_msg_tx_done(pdata)) DPRINTK ("fxgmac_prep_tx_pkt,tso, pkt desc cnt=%d\n", pkt_info->desc_count);
    } else if (skb->ip_summed == CHECKSUM_PARTIAL)
        pkt_info->attributes = FXGMAC_SET_REG_BITS(
                    pkt_info->attributes,
                    TX_PACKET_ATTRIBUTES_CSUM_ENABLE_POS,
                    TX_PACKET_ATTRIBUTES_CSUM_ENABLE_LEN,
                    1);

    if (skb_vlan_tag_present(skb)) {
        /* VLAN requires an extra descriptor if tag is different */
        if (skb_vlan_tag_get(skb) != ring->tx.cur_vlan_ctag)
            /* We can share with the TSO context descriptor */
            if (!context_desc) {
                context_desc = 1;
                pkt_info->desc_count++;
            }

        pkt_info->attributes = FXGMAC_SET_REG_BITS(
                    pkt_info->attributes,
                    TX_PACKET_ATTRIBUTES_VLAN_CTAG_POS,
                    TX_PACKET_ATTRIBUTES_VLAN_CTAG_LEN,
                    1);
        if(netif_msg_tx_done(pdata)) DPRINTK ("fxgmac_prep_tx_pkt,VLAN, pkt desc cnt=%d,vlan=0x%04x\n", pkt_info->desc_count, skb_vlan_tag_get(skb));
    }

    for (len = skb_headlen(skb); len;) {
        pkt_info->desc_count++;
        len -= min_t(unsigned int, len, FXGMAC_TX_MAX_BUF_SIZE);
    }

    for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
        frag = &skb_shinfo(skb)->frags[i];
        for (len = skb_frag_size(frag); len; ) {
            pkt_info->desc_count++;
            len -= min_t(unsigned int, len, FXGMAC_TX_MAX_BUF_SIZE);
        }
    }
    if(netif_msg_tx_done(pdata)) 
        DPRINTK ("fxgmac_prep_tx_pkt callout,pkt desc cnt=%d,skb len=%d, skbheadlen=%d,frags=%d\n", pkt_info->desc_count, skb->len, skb_headlen(skb), skb_shinfo(skb)->nr_frags);
}

static int fxgmac_calc_rx_buf_size(struct net_device *netdev, unsigned int mtu)
{
    unsigned int rx_buf_size;

    if (mtu > FXGMAC_JUMBO_PACKET_MTU) {
        netdev_alert(netdev, "MTU exceeds maximum supported value\n");
        return -EINVAL;
    }

    rx_buf_size = mtu + ETH_HLEN + ETH_FCS_LEN + VLAN_HLEN;
    rx_buf_size = clamp_val(rx_buf_size, FXGMAC_RX_MIN_BUF_SIZE, PAGE_SIZE * 4 /* follow yonggang's suggestion */);

    rx_buf_size = (rx_buf_size + FXGMAC_RX_BUF_ALIGN - 1) &
                ~(FXGMAC_RX_BUF_ALIGN - 1);

    return rx_buf_size;
}

static void fxgmac_enable_rx_tx_ints(struct fxgmac_pdata *pdata)
{
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;
    struct fxgmac_channel *channel;
    enum fxgmac_int int_id;
    unsigned int i;

    channel = pdata->channel_head;
    for (i = 0; i < pdata->channel_count; i++, channel++) {
        if (channel->tx_ring && channel->rx_ring)
            int_id = FXGMAC_INT_DMA_CH_SR_TI_RI;
        else if (channel->tx_ring)
            int_id = FXGMAC_INT_DMA_CH_SR_TI;
        else if (channel->rx_ring)
            int_id = FXGMAC_INT_DMA_CH_SR_RI;
        else
            continue;

        hw_ops->enable_int(channel, int_id);
    }
}

static void fxgmac_disable_rx_tx_ints(struct fxgmac_pdata *pdata)
{
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;
    struct fxgmac_channel *channel;
    enum fxgmac_int int_id;
    unsigned int i;

    channel = pdata->channel_head;
    for (i = 0; i < pdata->channel_count; i++, channel++) {
        if (channel->tx_ring && channel->rx_ring)
            int_id = FXGMAC_INT_DMA_CH_SR_TI_RI;
        else if (channel->tx_ring)
            int_id = FXGMAC_INT_DMA_CH_SR_TI;
        else if (channel->rx_ring)
            int_id = FXGMAC_INT_DMA_CH_SR_RI;
        else
            continue;

        hw_ops->disable_int(channel, int_id);
    }
}

static irqreturn_t fxgmac_isr(int irq, void *data)
{
    unsigned int dma_isr, dma_ch_isr, mac_isr;
    struct fxgmac_pdata *pdata = data;
    struct fxgmac_channel *channel;
    struct fxgmac_hw_ops *hw_ops;
    unsigned int i, ti, ri;

#if FXGMAC_FPGA_VER_20210507
    /* below line is added for fpga after 20210507 */
    struct net_device *netdev = pdata->netdev;
    u32 regval, val_mgmt_intcrtl0;
#endif

    hw_ops = &pdata->hw_ops;

    /* The DMA interrupt status register also reports MAC and MTL
     * interrupts. So for polling mode, we just need to check for
     * this register to be non-zero
     */
    dma_isr = readl(pdata->mac_regs + DMA_ISR);

#if FXGMAC_FPGA_VER_B4_0507
    if (!dma_isr)
        return IRQ_HANDLED;
#endif

#if FXGMAC_FPGA_VER_20210507
    //if (netdev->base_addr)
    {
        val_mgmt_intcrtl0 = (u32)(* ((u32 *)(netdev->base_addr + MGMT_INT_CTRL0)));
    }

#if FXGMAC_TX_INTERRUPT_EN
    if (!(val_mgmt_intcrtl0 & MGMT_INT_CTRL0_INT_STATUS_RXTX_MASK/*MGMT_INT_CTRL0_INT_STATUS_MASK*/) && (!dma_isr))
#else
    if (!(val_mgmt_intcrtl0 & MGMT_INT_CTRL0_INT_STATUS_RXCH_MASK/*MGMT_INT_CTRL0_INT_STATUS_MASK*/) && (!dma_isr))
#endif
        return IRQ_HANDLED;

    //disable all management interrupts. we actully use only Rx ch interrupts.
    regval = FXGMAC_SET_REG_BITS(val_mgmt_intcrtl0, MGMT_INT_CTRL0_INT_MASK_POS,
                    MGMT_INT_CTRL0_INT_MASK_LEN,
                    MGMT_INT_CTRL0_INT_MASK_MASK);
    writel(regval, (volatile void *)(netdev->base_addr + MGMT_INT_CTRL0));
    //pdata->stats.mgmt_int_isr=(u64)dma_isr;
    //pdata->stats.mgmt_int_isr=(u64)((pdata->stats.mgmt_int_isr << 32) + val_mgmt_intcrtl0);

#if FXGMAC_TX_INTERRUPT_EN
    regval = FXGMAC_GET_REG_BITS(regval, MGMT_INT_CTRL0_INT_STATUS_RXCH_POS,
                    MGMT_INT_CTRL0_INT_STATUS_RXTX_LEN);
    regval = (regval | dma_isr) & MGMT_INT_CTRL0_INT_STATUS_RXTX_MASK ;
#else
    regval = FXGMAC_GET_REG_BITS(regval, MGMT_INT_CTRL0_INT_STATUS_RXCH_POS,
                    MGMT_INT_CTRL0_INT_STATUS_RXCH_LEN);
    regval = (regval | dma_isr) & MGMT_INT_CTRL0_INT_STATUS_RXCH_MASK ;
#endif

#ifdef CONFIG_PCI_MSI
    //for msi/msix, we use it as debug counter
    pdata->stats.mgmt_int_isr++;
#else
    //pdata->stats.mgmt_int_isr=(u64)regval;
    pdata->stats.mgmt_int_isr++;
#endif
#endif

    netif_dbg(pdata, intr, pdata->netdev, "DMA_ISR=%#010x\n", dma_isr);

    for (i = 0; i < pdata->channel_count; i++) {
#if FXGMAC_FPGA_VER_B4_0507
        if (!(dma_isr & (1 << i)))
#elif FXGMAC_FPGA_VER_20210507
#if FXGMAC_TX_INTERRUPT_EN
        if (FXGMAC_IS_CHANNEL_WITH_TX_IRQ(i) ? (!(regval & (1 << i)) && !(regval & (1 << MGMT_INT_CTRL0_INT_STATUS_TXCH_POS))): (!(regval & (1 << i))))   
#else
        if (!(regval & (1 << i)))
#endif
#endif
            continue;

        channel = pdata->channel_head + i;

        dma_ch_isr = readl(FXGMAC_DMA_REG(channel, DMA_CH_SR));
        netif_dbg(pdata, intr, pdata->netdev, "DMA_CH%u_ISR=%#010x\n",
            i, dma_ch_isr);

        /* The TI or RI interrupt bits may still be set even if using
        * per channel DMA interrupts. Check to be sure those are not
        * enabled before using the private data napi structure.
        */
        ti = FXGMAC_GET_REG_BITS(dma_ch_isr, DMA_CH_SR_TI_POS,
                     DMA_CH_SR_TI_LEN);
        ri = FXGMAC_GET_REG_BITS(dma_ch_isr, DMA_CH_SR_RI_POS,
                     DMA_CH_SR_RI_LEN);
        if (!pdata->per_channel_irq && (ti || ri)) {
            if (napi_schedule_prep(&pdata->napi)) {
                /* Disable Tx and Rx interrupts */
                fxgmac_disable_rx_tx_ints(pdata);

                pdata->stats.napi_poll_isr++;

#if FXGMAC_TX_INTERRUPT_EN
                if (ti || (regval & (1 << MGMT_INT_CTRL0_INT_STATUS_TXCH_POS))) {
                    pdata->stats.msix_ch0_napi_isr_tx++;
                }
#endif

                /* Turn on polling */
                __napi_schedule_irqoff(&pdata->napi);
            }
        }

        if (FXGMAC_GET_REG_BITS(dma_ch_isr, DMA_CH_SR_TPS_POS,
                    DMA_CH_SR_TPS_LEN))
            pdata->stats.tx_process_stopped++;

        if (FXGMAC_GET_REG_BITS(dma_ch_isr, DMA_CH_SR_RPS_POS,
                    DMA_CH_SR_RPS_LEN))
            pdata->stats.rx_process_stopped++;

        if (FXGMAC_GET_REG_BITS(dma_ch_isr, DMA_CH_SR_TBU_POS,
                    DMA_CH_SR_TBU_LEN))
            pdata->stats.tx_buffer_unavailable++;

        if (FXGMAC_GET_REG_BITS(dma_ch_isr, DMA_CH_SR_RBU_POS,
                    DMA_CH_SR_RBU_LEN))
            pdata->stats.rx_buffer_unavailable++;

        /* Restart the device on a Fatal Bus Error */
        if (FXGMAC_GET_REG_BITS(dma_ch_isr, DMA_CH_SR_FBE_POS,
                    DMA_CH_SR_FBE_LEN)) {
            pdata->stats.fatal_bus_error++;
            schedule_work(&pdata->restart_work);
        }

        /* Clear all interrupt signals */
        writel(dma_ch_isr, FXGMAC_DMA_REG(channel, DMA_CH_SR));
    }

    if (FXGMAC_GET_REG_BITS(dma_isr, DMA_ISR_MACIS_POS,
                DMA_ISR_MACIS_LEN)) {
        mac_isr = readl(pdata->mac_regs + MAC_ISR);

        if (FXGMAC_GET_REG_BITS(mac_isr, MAC_ISR_MMCTXIS_POS,
                    MAC_ISR_MMCTXIS_LEN))
            hw_ops->tx_mmc_int(pdata);

        if (FXGMAC_GET_REG_BITS(mac_isr, MAC_ISR_MMCRXIS_POS,
                    MAC_ISR_MMCRXIS_LEN))
            hw_ops->rx_mmc_int(pdata);

        /* Clear all interrupt signals */
        writel(mac_isr, (pdata->mac_regs + MAC_ISR));
    }

    return IRQ_HANDLED;
}

static irqreturn_t fxgmac_dma_isr(int irq, void *data)
{
    struct fxgmac_channel *channel = data;
    struct fxgmac_pdata *pdata = channel->pdata;
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;
    u32 regval;
    int message_id;

    if (irq == channel->dma_irq_tx) {
        message_id = MSI_ID_TXQ0;
        hw_ops->disable_msix_one_interrupt(pdata, message_id);
        regval = 0;
        regval = FXGMAC_SET_REG_BITS(regval, DMA_CH_SR_TI_POS, DMA_CH_SR_TI_LEN, 1);
        writereg(pdata->pAdapter, regval, FXGMAC_DMA_REG(channel, DMA_CH_SR));
        if (napi_schedule_prep(&channel->napi_tx)) {
            __napi_schedule_irqoff(&channel->napi_tx);
        }
    } else {
        message_id = channel->queue_index;
        hw_ops->disable_msix_one_interrupt(pdata, message_id);
        regval = 0;
        regval = readreg(pdata->pAdapter, FXGMAC_DMA_REG(channel, DMA_CH_SR));
        regval = FXGMAC_SET_REG_BITS(regval, DMA_CH_SR_RI_POS, DMA_CH_SR_RI_LEN, 1);
        writereg(pdata->pAdapter, regval, FXGMAC_DMA_REG(channel, DMA_CH_SR));
        if (napi_schedule_prep(&channel->napi_rx)) {
            __napi_schedule_irqoff(&channel->napi_rx);
        }
    }

    pdata->stats.msix_int_isr++;
    pdata->stats.msix_int_isr_cur=(u64)channel->dma_irq;

    switch(channel->queue_index){
    case 0:
#if FXGMAC_TX_INTERRUPT_EN
        if(irq == channel->dma_irq) {
            pdata->stats.msix_ch0_napi_isr++;
        }
        if (irq == channel->dma_irq_tx) {
            pdata->stats.msix_ch0_napi_isr_tx++;
            pdata->stats.msix_ch0_napi_sch_tx++;
        }
#else
        pdata->stats.msix_ch0_napi_isr++;
#endif
        break;
    case 1:
        pdata->stats.msix_ch1_napi_isr++;
        break;
    case 2:
        pdata->stats.msix_ch2_napi_isr++;
        break;
    case 3:
        pdata->stats.msix_ch3_napi_isr++;
        break;
    default:
        break;
    }

    return IRQ_HANDLED;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0))
static void fxgmac_tx_timer(struct timer_list *t)
#else
static void fxgmac_tx_timer(unsigned long data)
#endif
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0))
    struct fxgmac_channel *channel = from_timer(channel, t, tx_timer);
#else
    struct fxgmac_channel *channel = (struct fxgmac_channel *)data;
#endif

    //for msix. since this function is defined for MSIx, it is no need to brace by macro CONFIG_PCI_MSI
    struct fxgmac_pdata *pdata = channel->pdata;
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;
    u32 regval;

    if (pdata->per_channel_irq) {
        hw_ops->disable_msix_one_interrupt(pdata, MSI_ID_TXQ0);
        regval = 0;
        regval = FXGMAC_SET_REG_BITS(regval, DMA_CH_SR_TI_POS, DMA_CH_SR_TI_LEN, 1);
        writereg(pdata->pAdapter, regval, FXGMAC_DMA_REG(channel, DMA_CH_SR));
        if (napi_schedule_prep(&channel->napi_tx)) {
            pdata->stats.napi_poll_txtimer++;
            __napi_schedule(&channel->napi_tx);
        }
    } else {
        fxgmac_disable_rx_tx_ints(pdata);
        if (napi_schedule_prep(&pdata->napi)) {
            pdata->stats.napi_poll_txtimer++;
            __napi_schedule(&pdata->napi);
        }
    }

    pdata->stats.cnt_alive_txtimer++;
    channel->tx_timer_active = 0;
}

#if FXGMAC_TX_HANG_TIMER_EN
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0))
static void fxgmac_tx_hang_timer_handler(struct timer_list *t)
#else
static void fxgmac_tx_hang_timer_handler(unsigned long data)
#endif
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0))
    struct fxgmac_channel *channel = from_timer(channel, t, tx_hang_timer);
#else
    struct fxgmac_channel *channel = (struct fxgmac_channel *)data;
#endif

#if FXGMAC_TX_HANG_CHECH_DIRTY
    struct fxgmac_ring *ring = channel->tx_ring;
#endif
    struct fxgmac_pdata *pdata = channel->pdata;
    struct net_device *netdev = pdata->netdev;
    unsigned int hw_reg_cur;
    unsigned int regval;

#if FXGMAC_TX_HANG_CHECH_DIRTY
    hw_reg_cur = ring->dirty;
#else
    hw_reg_cur = readl(FXGMAC_DMA_REG(channel, 0x44/* tx desc curr pointer reg */));
#endif
    if(hw_reg_cur == channel->tx_hang_hw_cur) {

        /* hw current desc still stucked */
        if(!pdata->tx_hang_restart_queuing) {
            pdata->tx_hang_restart_queuing = 1;
            pdata->stats.cnt_tx_hang++;
            DPRINTK("tx_hang_timer_handler: restart scheduled, at desc %u, queuing=%u.\n", channel->tx_hang_hw_cur, pdata->tx_hang_restart_queuing);	

            netif_tx_stop_all_queues(netdev);

            /* Disable MAC Rx */
            regval = readl(pdata->mac_regs + MAC_CR);
            regval = FXGMAC_SET_REG_BITS(regval, MAC_CR_CST_POS,
                             MAC_CR_CST_LEN, 0);
            regval = FXGMAC_SET_REG_BITS(regval, MAC_CR_ACS_POS,
                             MAC_CR_ACS_LEN, 0);
            regval = FXGMAC_SET_REG_BITS(regval, MAC_CR_RE_POS,
                             MAC_CR_RE_LEN, 0);
            writel(regval, pdata->mac_regs + MAC_CR);

            schedule_work(&pdata->restart_work);
        }
    }

    channel->tx_hang_timer_active = 0;
}

static void fxgmac_tx_hang_timer_start(struct fxgmac_channel *channel)
{
    struct fxgmac_pdata *pdata = channel->pdata;

    /* Start the Tx hang timer */
    if (1 && !channel->tx_hang_timer_active) {
        channel->tx_hang_timer_active = 1;

        /* FXGMAC_INIT_DMA_TX_USECS is desc3 polling period, we give 2 more checking period */
        mod_timer(&channel->tx_hang_timer,
                jiffies + usecs_to_jiffies(FXGMAC_INIT_DMA_TX_USECS * 10));

        pdata->stats.cnt_alive_tx_hang_timer++;
    }
}
#endif

static void fxgmac_init_timers(struct fxgmac_pdata *pdata)
{
    struct fxgmac_channel *channel;
    unsigned int i;

    channel = pdata->channel_head;
    for (i = 0; i < pdata->channel_count; i++, channel++) {
        if (!channel->tx_ring)
            break;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0))
    timer_setup(&channel->tx_timer, fxgmac_tx_timer, 0);
#else
    setup_timer(&channel->tx_timer, fxgmac_tx_timer, (unsigned long)channel);
#endif
#if FXGMAC_TX_HANG_TIMER_EN
    channel->tx_hang_timer_active = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0))
    timer_setup(&channel->tx_hang_timer, fxgmac_tx_hang_timer_handler, 0);
#else
    setup_timer(&channel->tx_hang_timer, fxgmac_tx_hang_timer_handler, (unsigned long)channel);
#endif
#endif
    }
}

static void fxgmac_stop_timers(struct fxgmac_pdata *pdata)
{
    struct fxgmac_channel *channel;
    unsigned int i;

    channel = pdata->channel_head;
    if (channel != NULL) {
        for (i = 0; i < pdata->channel_count; i++, channel++) {
            if (!channel->tx_ring)
                break;

            del_timer_sync(&channel->tx_timer);
#if FXGMAC_TX_HANG_TIMER_EN
            del_timer_sync(&channel->tx_hang_timer);
            channel->tx_hang_timer_active = 0;
#endif
        }
    }
}

static void fxgmac_napi_enable(struct fxgmac_pdata *pdata, unsigned int add)
{
    struct fxgmac_channel *channel;
    unsigned int i;

    if (pdata->per_channel_irq) {
        channel = pdata->channel_head;
        for (i = 0; i < pdata->channel_count; i++, channel++) {
            if (add) {
                netif_napi_add(pdata->netdev, &channel->napi_rx,
                           fxgmac_one_poll_rx,
                           NAPI_POLL_WEIGHT);
            }
            napi_enable(&channel->napi_rx);

            if (FXGMAC_IS_CHANNEL_WITH_TX_IRQ(i)) {
                netif_napi_add(pdata->netdev, &channel->napi_tx,
                           fxgmac_one_poll_tx,
                           NAPI_POLL_WEIGHT);
                napi_enable(&channel->napi_tx);
            }
            if(netif_msg_drv(pdata)) DPRINTK("napi_enable, msix ch%d napi enabled done,add=%d\n", i, add);
        }
    } else {
        if (add)
            netif_napi_add(pdata->netdev, &pdata->napi,
                       fxgmac_all_poll, NAPI_POLL_WEIGHT);

        napi_enable(&pdata->napi);
    }
}

static void fxgmac_napi_disable(struct fxgmac_pdata *pdata, unsigned int del)
{
    struct fxgmac_channel *channel;
    unsigned int i;

    if (pdata->per_channel_irq) {
        channel = pdata->channel_head;
        if (channel != NULL) {
            for (i = 0; i < pdata->channel_count; i++, channel++) {
                napi_disable(&channel->napi_rx);

                if (del) {
                    netif_napi_del(&channel->napi_rx);
                }

                if (FXGMAC_IS_CHANNEL_WITH_TX_IRQ(i)) {
                    napi_disable(&channel->napi_tx);
                    netif_napi_del(&channel->napi_tx);
                }
                if(netif_msg_drv(pdata)) DPRINTK("napi_disable, msix ch%d napi disabled done,del=%d\n", i, del);
            }
        }
    } else {
        napi_disable(&pdata->napi);

        if (del)
            netif_napi_del(&pdata->napi);
    }
}

static int fxgmac_request_irqs(struct fxgmac_pdata *pdata)
{
    struct net_device *netdev = pdata->netdev;
    struct fxgmac_channel *channel;
    unsigned int i;
    int ret;

    if(0 == (pdata->int_flags & FXGMAC_FLAG_MSIX_ENABLED))
    {
        ret = devm_request_irq(pdata->dev, pdata->dev_irq, fxgmac_isr,
                         (pdata->int_flags & FXGMAC_FLAG_MSI_ENABLED) ? 0 : IRQF_SHARED, netdev->name, pdata);	
        if (ret) {
            netdev_alert(netdev, "error requesting irq %d, ret = %d\n", pdata->dev_irq, ret);
            return ret;
        }
    }

    if (!pdata->per_channel_irq)
        return 0;

    channel = pdata->channel_head;
    for (i = 0; i < pdata->channel_count; i++, channel++) {
        snprintf(channel->dma_irq_name,
             sizeof(channel->dma_irq_name) - 1,
                             "%s-ch%d-Rx-%u", netdev_name(netdev), i,
             channel->queue_index);
#if FXGMAC_TX_INTERRUPT_EN
        if(FXGMAC_IS_CHANNEL_WITH_TX_IRQ(i)) {
            snprintf(channel->dma_irq_name_tx,
                 sizeof(channel->dma_irq_name_tx) - 1,
                                 "%s-ch%d-Tx-%u", netdev_name(netdev), i,
                 channel->queue_index);

            ret = devm_request_irq(pdata->dev, channel->dma_irq_tx,
                             fxgmac_dma_isr, 0,
                             channel->dma_irq_name_tx, channel);

            if (ret) {
                DPRINTK("fxgmac_req_irqs, err with MSIx irq request for ch %d tx, ret=%d\n", i, ret);
                /* Using an unsigned int, 'i' will go to UINT_MAX and exit */
                devm_free_irq(pdata->dev, channel->dma_irq_tx, channel);
                devm_free_irq(pdata->dev, pdata->dev_irq, pdata);
                return ret;
            }

            if(netif_msg_drv(pdata)) DPRINTK("fxgmac_req_irqs, MSIx irq_tx request ok, ch=%d, irq=%d,%s\n", i, channel->dma_irq_tx, channel->dma_irq_name_tx);
        }
#endif
        ret = devm_request_irq(pdata->dev, channel->dma_irq,
                           fxgmac_dma_isr, 0,
                           channel->dma_irq_name, channel);
        if (ret) {
            netdev_alert(netdev, "error requesting irq %d\n",
                     channel->dma_irq);
            goto err_irq;
        }
    }

    if(netif_msg_drv(pdata)) DPRINTK("fxgmac_req_irqs, MSIx irq request ok, total=%d,%d~%d\n", i, (pdata->channel_head)[0].dma_irq, (pdata->channel_head)[i-1].dma_irq);
    return 0;

err_irq:
    DPRINTK("fxgmac_req_irqs, err with MSIx irq request at %d, ret=%d\n", i, ret);
    /* Using an unsigned int, 'i' will go to UINT_MAX and exit */
    for (i--, channel--; i < pdata->channel_count; i--, channel--) {
#if FXGMAC_TX_INTERRUPT_EN
        if(FXGMAC_IS_CHANNEL_WITH_TX_IRQ(i)) {
            devm_free_irq(pdata->dev, channel->dma_irq_tx, channel);
        }
#endif
        devm_free_irq(pdata->dev, channel->dma_irq, channel);
    }

    devm_free_irq(pdata->dev, pdata->dev_irq, pdata);
    return ret;
}

static void fxgmac_free_irqs(struct fxgmac_pdata *pdata)
{
    struct fxgmac_channel *channel;
    unsigned int i = 0;

    if(0 == (pdata->int_flags & FXGMAC_FLAG_MSIX_ENABLED)) {
        devm_free_irq(pdata->dev, pdata->dev_irq, pdata);
    }

    if (!pdata->per_channel_irq)
        return;

    channel = pdata->channel_head;
    if (channel != NULL) {
        for (i = 0; i < pdata->channel_count; i++, channel++) {
#if FXGMAC_TX_INTERRUPT_EN
            if(FXGMAC_IS_CHANNEL_WITH_TX_IRQ(i)) {
                devm_free_irq(pdata->dev, channel->dma_irq_tx, channel);
                if(netif_msg_drv(pdata)) DPRINTK("fxgmac_free_irqs, MSIx irq_tx clear done, ch=%d\n", i);
            }
#endif
            devm_free_irq(pdata->dev, channel->dma_irq, channel);
        }
    }
    if(netif_msg_drv(pdata)) DPRINTK("fxgmac_free_irqs, MSIx rx irq clear done, total=%d\n", i);
}

static void fxgmac_free_tx_data(struct fxgmac_pdata *pdata)
{
    struct fxgmac_desc_ops *desc_ops = &pdata->desc_ops;
    struct fxgmac_desc_data *desc_data;
    struct fxgmac_channel *channel;
    struct fxgmac_ring *ring;
    unsigned int i, j;

    channel = pdata->channel_head;
    if (channel != NULL) {
        for (i = 0; i < pdata->channel_count; i++, channel++) {
            ring = channel->tx_ring;
            if (!ring)
                break;

            for (j = 0; j < ring->dma_desc_count; j++) {
                desc_data = FXGMAC_GET_DESC_DATA(ring, j);
                desc_ops->unmap_desc_data(pdata, desc_data);
            }
        }
    }
}

static void fxgmac_free_rx_data(struct fxgmac_pdata *pdata)
{
    struct fxgmac_desc_ops *desc_ops = &pdata->desc_ops;
    struct fxgmac_desc_data *desc_data;
    struct fxgmac_channel *channel;
    struct fxgmac_ring *ring;
    unsigned int i, j;

    channel = pdata->channel_head;
    if (channel != NULL) {
        for (i = 0; i < pdata->channel_count; i++, channel++) {
            ring = channel->rx_ring;
            if (!ring)
                break;

            for (j = 0; j < ring->dma_desc_count; j++) {
                desc_data = FXGMAC_GET_DESC_DATA(ring, j);
                desc_ops->unmap_desc_data(pdata, desc_data);
            }
        }
    }
}

extern int fxgmac_dismiss_all_int(struct fxgmac_pdata *pdata);

static int fxgmac_start(struct fxgmac_pdata *pdata)
{
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;
    struct net_device *netdev = pdata->netdev;
    int ret;
    bool b_linkup = false;
    u32 phy_speed = 2; //default to 1000m
    unsigned int pcie_low_power = 0;
#if FXGMAC_FPGA_VER_20210507
    u32 regval,val_mgmt_intcrtl0;
#endif

    if(netif_msg_drv(pdata)) DPRINTK("fxgmac start callin here.\n");

    /* must reset software again here, to avoid flushing tx queue error caused by the system only run probe
    * when installing driver on the arm platform.
    */
    hw_ops->exit(pdata);
    hw_ops->reset_phy(pdata);
    hw_ops->release_phy(pdata);
    hw_ops->pcie_init(pdata,
        pcie_low_power & PCIE_LP_ASPM_LTR,
        pcie_low_power & PCIE_LP_ASPM_L1SS,
        pcie_low_power & PCIE_LP_ASPM_L1,
        pcie_low_power & PCIE_LP_ASPM_L0S);
    hw_ops->config_power_up(pdata);

#if (FXGMAC_TXRX_EN_AFTER_PHY_RELEASE)
    hw_ops->release_phy(pdata);
#endif

    fxgmac_dismiss_all_int(pdata);
    if(hw_ops->get_xlgmii_phy_status)
        hw_ops->get_xlgmii_phy_status(pdata, (u32*)&phy_speed, (bool *)&b_linkup, 0);
    switch(phy_speed){
    case 2:
        pdata->phy_speed = SPEED_1000;
        break;
    case 1:
        pdata->phy_speed = SPEED_100;
        break;
    case 0:
        pdata->phy_speed = SPEED_10;
        break;
    default:
        pdata->phy_speed = SPEED_1000;
        break;
    }
    pdata->phy_link = b_linkup;

    ret = hw_ops->init(pdata);
    if (ret) {
        printk("fxgmac hw init error.\n");
        return ret;
    }
    fxgmac_napi_enable(pdata, 1);

    ret = fxgmac_request_irqs(pdata);
    if (ret)
        goto err_napi;

    hw_ops->enable_tx(pdata);
    hw_ops->enable_rx(pdata);
#if FXGMAC_FPGA_VER_20210507
    //config interrupt to level signal
    regval = (u32)readl((const volatile void *)(pdata->mac_regs + DMA_MR));
    regval = FXGMAC_SET_REG_BITS(regval, DMA_MR_INTM_POS,
                                     DMA_MR_INTM_LEN, 1);
#if 1
    regval = FXGMAC_SET_REG_BITS(regval, DMA_MR_QUREAD_POS,
                                 DMA_MR_QUREAD_LEN, 1); 	
#endif	
    writel(regval, pdata->mac_regs + DMA_MR);

    //enable management controlled interrupts
    val_mgmt_intcrtl0 = (u32)readl((const volatile void *)(netdev->base_addr + MGMT_INT_CTRL0));

    //enable RX channel interrupts
    val_mgmt_intcrtl0 = FXGMAC_SET_REG_BITS(val_mgmt_intcrtl0, MGMT_INT_CTRL0_INT_MASK_RXCH_POS,
                         MGMT_INT_CTRL0_INT_STATUS_RXCH_LEN,
                         0);
#if FXGMAC_TX_INTERRUPT_EN
    //enable TX channel interrupts
    val_mgmt_intcrtl0 = FXGMAC_SET_REG_BITS(val_mgmt_intcrtl0, MGMT_INT_CTRL0_INT_MASK_TXCH_POS,
                         MGMT_INT_CTRL0_INT_STATUS_TXCH_LEN,
                         0);
#endif
    writel(val_mgmt_intcrtl0, (volatile void *)(netdev->base_addr + MGMT_INT_CTRL0));
#endif

    hw_ops->set_interrupt_moderation(pdata);
    hw_ops->enable_msix_rxtxinterrupt(pdata);
    fxgmac_enable_rx_tx_ints(pdata);

    fxgmac_act_phy_link(pdata);
#if !(FXGMAC_TXRX_EN_AFTER_PHY_RELEASE)
    hw_ops->release_phy(pdata);
#endif
    netif_tx_start_all_queues(netdev);

    hw_ops->led_under_active(pdata);

    return 0;

err_napi:
    fxgmac_napi_disable(pdata, 1);
    hw_ops->exit(pdata);
    DPRINTK("fxgmac start callout with irq err.\n");
    return ret;
}

static void fxgmac_stop(struct fxgmac_pdata *pdata)
{
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;
    struct net_device *netdev = pdata->netdev;
    struct fxgmac_channel *channel;
    struct netdev_queue *txq;
    unsigned int i;
#if FXGMAC_FPGA_VER_20210507
    u32 val_mgmt_intcrtl0;

    //disable management controlled interrupts
    //disable RX channel interrupts
    val_mgmt_intcrtl0 = (u32)readl((const volatile void *)(netdev->base_addr + MGMT_INT_CTRL0));
    val_mgmt_intcrtl0 = FXGMAC_SET_REG_BITS(val_mgmt_intcrtl0, MGMT_INT_CTRL0_INT_MASK_RXCH_POS,
                                             MGMT_INT_CTRL0_INT_STATUS_RXCH_LEN,
                                             MGMT_INT_CTRL0_INT_STATUS_RXCH_MASK);
#if FXGMAC_TX_INTERRUPT_EN
    //disable TX channel interrupts
    val_mgmt_intcrtl0 = FXGMAC_SET_REG_BITS(val_mgmt_intcrtl0, MGMT_INT_CTRL0_INT_MASK_TXCH_POS,
                         MGMT_INT_CTRL0_INT_STATUS_TXCH_LEN,
                         MGMT_INT_CTRL0_INT_STATUS_TXCH_MASK);
#endif
    writel(val_mgmt_intcrtl0, (volatile void *)(netdev->base_addr + MGMT_INT_CTRL0));
#endif

    netif_carrier_off(netdev);//yzhang added, 0324
    netif_tx_stop_all_queues(netdev);
    fxgmac_stop_timers(pdata);
    hw_ops->disable_tx(pdata);
    hw_ops->disable_rx(pdata);
    fxgmac_free_irqs(pdata);
    fxgmac_napi_disable(pdata, 1);

    channel = pdata->channel_head;
    if (channel != NULL) {
        for (i = 0; i < pdata->channel_count; i++, channel++) {
            if (!channel->tx_ring)
                continue;

            txq = netdev_get_tx_queue(netdev, channel->queue_index);
            netdev_tx_reset_queue(txq);
        }
    }

    switch (pdata->current_state) {
        case CURRENT_STATE_SUSPEND:
            hw_ops->led_under_sleep(pdata);
            break;
        case CURRENT_STATE_SHUTDOWN:
        case CURRENT_STATE_RESTART:
            hw_ops->led_under_shutdown(pdata);
            break;
        case CURRENT_STATE_CLOSE:
            //hw_ops->led_under_disable(pdata);
            break;
        default:
            break;
    }
}

void fxgmac_restart_dev(struct fxgmac_pdata *pdata)
{
    int ret;

    /* If not running, "restart" will happen on open */
    if (!netif_running(pdata->netdev))
        return;

    pdata->current_state = CURRENT_STATE_RESTART;
    fxgmac_stop(pdata);

    fxgmac_free_tx_data(pdata);
    fxgmac_free_rx_data(pdata);

    ret = fxgmac_start(pdata);
    if (ret) {
        printk("fxgmac_restart_dev: fxgmac_start failed.\n");
    }
}

static void fxgmac_restart(struct work_struct *work)
{
    struct fxgmac_pdata *pdata = container_of(work,
                            struct fxgmac_pdata,
                            restart_work);

    rtnl_lock();

    fxgmac_restart_dev(pdata);

    rtnl_unlock();
}

void fxgmac_net_powerup(struct fxgmac_pdata *pdata)
{
    int ret;
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;

    if(netif_msg_drv(pdata)) DPRINTK("fxgmac_net_powerup callin\n");

    /* signal that we are up now */
    pdata->powerstate = 0; //clear all bits as normal now
    if (__test_and_set_bit(FXGMAC_POWER_STATE_UP, &pdata->powerstate)) {
        return; /* do nothing if already up */
    }
#if 0
    hw_ops->exit(pdata);

    /* wake mac to exit lowpower mode by soft or wol */
    hw_ops->config_power_up(pdata);
#endif

    ret = fxgmac_init(pdata, true);
    if (ret) {
        printk("fxgmac_net_powerup: fxgmac_init error\n");
        return;
    }
    ret = fxgmac_start(pdata);
    if (ret) {
        printk("fxgmac_net_powerup: fxgmac_start error\n");
        return;
    }

    // must call it after fxgmac_start,because it will be enable in fxgmac_start
    hw_ops->disable_arp_offload(pdata);

    fxgmac_phy_timer_resume(pdata);

    if(netif_msg_drv(pdata)) {
        DPRINTK("fxgmac_net_powerup callout, powerstate=%ld.\n", pdata->powerstate);
    }
}

void fxgmac_net_powerdown(struct fxgmac_pdata *pdata, unsigned int wol)
{
    struct net_device *netdev = pdata->netdev;
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;

    if(netif_msg_drv(pdata)) DPRINTK("fxgmac_net_powerdown callin here.\n");

    /* signal that we are down to the interrupt handler */
    if (__test_and_set_bit(FXGMAC_POWER_STATE_DOWN, &pdata->powerstate))
        return; /* do nothing if already down */

    if(netif_msg_drv(pdata)) DPRINTK("fxgmac_net_powerdown continue with down process.\n");
    /* phy polling timer should detect the state of fxgmac and stop link status polling accordingly */

    __clear_bit(FXGMAC_POWER_STATE_UP, &pdata->powerstate);

#if 1
    /* Shut off incoming Tx traffic */
    netif_tx_stop_all_queues(netdev);

    /* call carrier off first to avoid false dev_watchdog timeouts */
    netif_carrier_off(netdev);
    netif_tx_disable(netdev);

    /* Disable Rx */
    hw_ops->disable_rx(pdata);

    /* synchronize_rcu() needed for pending XDP buffers to drain */
    //if (adapter->xdp_ring[0]) 20210709
        synchronize_rcu();

    fxgmac_stop(pdata); //some works are redundent in this call
#endif
    // must call it after software reset
    hw_ops->pre_power_down(pdata, false);

    /* set mac to lowpower mode and enable wol accordingly */
    hw_ops->config_power_down(pdata, wol);

#if 1
    //handle vfs if it is envolved

    //similar work as in restart() for that, we do need a resume laterly
    fxgmac_free_tx_data(pdata);
    fxgmac_free_rx_data(pdata);
#endif
    if(netif_msg_drv(pdata)) DPRINTK("fxgmac_net_powerdown callout, powerstate=%ld.\n", pdata->powerstate);
}

static int fxgmac_open(struct net_device *netdev)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    struct fxgmac_desc_ops *desc_ops;
    int ret;
#if 0
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;
    bool b_linkup = false;
    u32 phy_speed = 2; //default to 1000m
#endif
    if(netif_msg_drv(pdata)) DPRINTK("fxgmac_open callin\n");

    desc_ops = &pdata->desc_ops;

    /* TODO: Initialize the phy */

    /* Calculate the Rx buffer size before allocating rings */
    //DPRINTK("fxgmac_open, b4 calc rx buf size, mtu,min,max=%d,%d,%d.\n", netdev->mtu, netdev->min_mtu, netdev->max_mtu);
    ret = fxgmac_calc_rx_buf_size(netdev, netdev->mtu);
    if (ret < 0)
        return ret;
    pdata->rx_buf_size = ret;

    /* Allocate the channels and rings */
    ret = desc_ops->alloc_channles_and_rings(pdata);
    if (ret)
        return ret;

    INIT_WORK(&pdata->restart_work, fxgmac_restart);
    fxgmac_init_timers(pdata);
#if 0 //move to fxgmac_start()
    if(hw_ops->get_xlgmii_phy_status)
        hw_ops->get_xlgmii_phy_status(pdata, (u32*)&phy_speed, (bool *)&b_linkup, 0);
    switch(phy_speed){
    case 2:
        pdata->phy_speed = SPEED_1000;
        break;
    case 1:
        pdata->phy_speed = SPEED_100;
        break;
    default:
        pdata->phy_speed = SPEED_1000;
        break;
    }
    pdata->phy_link = b_linkup;
#endif
    ret = fxgmac_start(pdata);
    if (ret)
        goto err_channels_and_rings;

    //use previous status
    //pdata->hw_ops->get_xlgmii_phy_status(pdata, 0, (bool *)&b_linkup, 0);
    //fxgmac_act_phy_link(pdata);
    fxgmac_phy_timer_init(pdata);

#if 0 /* yzhand debug to disable tx temporaly */
    netif_tx_stop_all_queues(netdev);
    DPRINTK("for debug and disable tx....\n");
#endif
    if(netif_msg_drv(pdata)) DPRINTK("fxgmac_open callout\n");

    return 0;

err_channels_and_rings:	
    desc_ops->free_channels_and_rings(pdata);
    DPRINTK("fxgmac_open callout with channel alloc err\n");
    return ret;
}

static int fxgmac_close(struct net_device *netdev)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    struct fxgmac_desc_ops *desc_ops;

    if(netif_msg_drv(pdata)) DPRINTK("fxgmac_close callin\n");

    desc_ops = &pdata->desc_ops;

    pdata->current_state = (pdata->current_state == CURRENT_STATE_SHUTDOWN) ? 
                        pdata->current_state : CURRENT_STATE_CLOSE;

    /* Stop the device */
    fxgmac_stop(pdata);

    /* Free the channels and rings */
    desc_ops->free_channels_and_rings(pdata);

    pdata->hw_ops.reset_phy(pdata);
    /*clean timer*/
    fxgmac_phy_timer_destroy(pdata);

    if(netif_msg_drv(pdata)) DPRINTK("fxgmac_close callout\n");

    return 0;
}

#if ((LINUX_VERSION_CODE > KERNEL_VERSION(4,0,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0)))
static void fxgmac_tx_timeout(struct net_device *netdev)
#else
static void fxgmac_tx_timeout(struct net_device *netdev, unsigned int unused)
#endif
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);

    netdev_warn(netdev, "tx timeout, device restarting\n");
#if FXGMAC_TX_HANG_TIMER_EN
    if(!pdata->tx_hang_restart_queuing)
        schedule_work(&pdata->restart_work);
#else
    schedule_work(&pdata->restart_work);
#endif
}

u16 fxgmac_cur_tx_skb_q_mapping = 0xff; //invalid value, yzhang
u16 fxgmac_cur_rx_ch_polling = 0xff; //invalid value, yzhang

static int fxgmac_xmit(struct sk_buff *skb, struct net_device *netdev)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    struct fxgmac_pkt_info *tx_pkt_info;
    struct fxgmac_desc_ops *desc_ops;
    struct fxgmac_channel *channel;
    struct fxgmac_hw_ops *hw_ops;
    struct netdev_queue *txq;
    struct fxgmac_ring *ring;
    int ret;

    desc_ops = &pdata->desc_ops;
    hw_ops = &pdata->hw_ops;

    //yzhang disabled 
    if(netif_msg_tx_done(pdata)) DPRINTK("xmit callin, skb->len=%d,q=%d\n", skb->len, skb->queue_mapping);

    fxgmac_cur_tx_skb_q_mapping = skb->queue_mapping;

    channel = pdata->channel_head + skb->queue_mapping;
    txq = netdev_get_tx_queue(netdev, channel->queue_index);
    ring = channel->tx_ring;
    tx_pkt_info = &ring->pkt_info;
#if 0
    if (1/*yzhang dbg skb->len == 0*/) {
        netif_err(pdata, tx_err, netdev,
                "empty skb received from stack\n");
        dev_kfree_skb_any(skb);
        return NETDEV_TX_OK;
    }
#else
    if (skb->len == 0) {
        netif_err(pdata, tx_err, netdev,
                                "empty skb received from stack\n");
        dev_kfree_skb_any(skb);
        return NETDEV_TX_OK;
    }
#endif
    /* Prepare preliminary packet info for TX */
    memset(tx_pkt_info, 0, sizeof(*tx_pkt_info));
    fxgmac_prep_tx_pkt(pdata, ring, skb, tx_pkt_info);

    /* Check that there are enough descriptors available */
    ret = fxgmac_maybe_stop_tx_queue(channel, ring,
                         tx_pkt_info->desc_count);
    if (ret)
    {
        return ret;
    }

    ret = fxgmac_prep_tso(skb, tx_pkt_info);
    if (ret) {
        netif_err(pdata, tx_err, netdev,
                            "error processing TSO packet\n");
        DPRINTK("dev_xmit,tx err for TSO\n");
        dev_kfree_skb_any(skb);
        return ret;
    }
    fxgmac_prep_vlan(skb, tx_pkt_info);

    if (!desc_ops->map_tx_skb(channel, skb)) {
        dev_kfree_skb_any(skb);
        DPRINTK("xmit, map tx skb err\n");
        return NETDEV_TX_OK;
    }

    /* Report on the actual number of bytes (to be) sent */
    netdev_tx_sent_queue(txq, tx_pkt_info->tx_bytes);
    if(netif_msg_tx_done(pdata)) DPRINTK("xmit,before hw_xmit, byte len=%d,txtimeout=%lu\n", tx_pkt_info->tx_bytes, txq->trans_timeout);

    /* Configure required descriptor fields for transmission */
    hw_ops->dev_xmit(channel);
#if FXGMAC_DUMMY_TX_DEBUG
    DPRINTK("tx hw_ops->dev_xmit ok\n");
#endif
    if (netif_msg_pktdata(pdata))
        fxgmac_dbg_pkt(netdev, skb, true);

    /* Stop the queue in advance if there may not be enough descriptors */
    fxgmac_maybe_stop_tx_queue(channel, ring, FXGMAC_TX_MAX_DESC_NR);

    return NETDEV_TX_OK;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0))
static void fxgmac_get_stats64(struct net_device *netdev,
                    struct rtnl_link_stats64 *s)
#else
static struct rtnl_link_stats64 * fxgmac_get_stats64(struct net_device *netdev,
                    struct rtnl_link_stats64 *s)
#endif
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    struct fxgmac_stats *pstats = &pdata->stats;

#if FXGMAC_PM_FEATURE_ENABLED
    /* 20210709 for net power down */
    if(!test_bit(FXGMAC_POWER_STATE_DOWN, &pdata->powerstate))
#endif
    {
        //DPRINTK("get_stats64, ndo op, callin\n");
        pdata->hw_ops.read_mmc_stats(pdata);
    }
    s->rx_packets = pstats->rxframecount_gb;
    s->rx_bytes = pstats->rxoctetcount_gb;
    s->rx_errors = pstats->rxframecount_gb -
               pstats->rxbroadcastframes_g -
               pstats->rxmulticastframes_g -
               pstats->rxunicastframes_g;
    s->multicast = pstats->rxmulticastframes_g;
    s->rx_length_errors = pstats->rxlengtherror;
    s->rx_crc_errors = pstats->rxcrcerror;
    s->rx_fifo_errors = pstats->rxfifooverflow;

    s->tx_packets = pstats->txframecount_gb;
    s->tx_bytes = pstats->txoctetcount_gb;
    s->tx_errors = pstats->txframecount_gb - pstats->txframecount_g;
    s->tx_dropped = netdev->stats.tx_dropped;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,12,0))
    return s;
#endif
}

static int fxgmac_set_mac_address(struct net_device *netdev, void *addr)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;
    struct sockaddr *saddr = addr;

    if (!is_valid_ether_addr(saddr->sa_data))
        return -EADDRNOTAVAIL;

    memcpy(netdev->dev_addr, saddr->sa_data, netdev->addr_len);
    memcpy(pdata->mac_addr, saddr->sa_data, netdev->addr_len);

    hw_ops->set_mac_address(pdata, netdev->dev_addr);
    hw_ops->set_mac_hash(pdata);

    DPRINTK("fxgmac,set mac addr to %02x:%02x:%02x:%02x:%02x:%02x\n",netdev->dev_addr[0], netdev->dev_addr[1], netdev->dev_addr[2],
            netdev->dev_addr[3], netdev->dev_addr[4], netdev->dev_addr[5]);
    return 0;
}

// cmd = [0x89F0, 0x89FF]
static int fxgmac_ioctl(struct net_device *netdev, 
            struct ifreq *ifr, int cmd)
{
    struct file f;
    int ret = FXGMAC_SUCCESS;
    struct fxgmac_pdata *pdata = netdev_priv(netdev);

    if (!netif_running(netdev))
        return -ENODEV;

    f.private_data = pdata;

    switch (cmd) {
        case FXGMAC_DEV_CMD:
            ret = fxgmac_dbg_netdev_ops_ioctl(&f, FXGMAC_IOCTL_DFS_COMMAND, (unsigned long)(ifr->ifr_data));
            break;
        default:
            ret = -EINVAL;
            break;
    }

    return ret;
}

static int fxgmac_change_mtu(struct net_device *netdev, int mtu)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    int ret;
#ifdef FXGMAC_DEBUG
    int old_mtu = netdev->mtu;
#endif

    pdata->jumbo = mtu > ETH_DATA_LEN ? 1 : 0;

    ret = fxgmac_calc_rx_buf_size(netdev, mtu);
    if (ret < 0)
        return ret;

    pdata->rx_buf_size = ret;
    netdev->mtu = mtu;

    if (netif_running(netdev))
        fxgmac_restart_dev(pdata);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0))
    DPRINTK("fxgmac,set MTU from %d to %d. min, max=(%d,%d)\n",old_mtu, netdev->mtu, netdev->min_mtu, netdev->max_mtu);
#else
    DPRINTK("fxgmac,set MTU from %d to %d.\n",old_mtu, netdev->mtu);
#endif

    return 0;
}

static int fxgmac_vlan_rx_add_vid(struct net_device *netdev,
                 __be16 proto,
                 u16 vid)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;

    set_bit(vid, pdata->active_vlans);
#if FXGMAC_FILTER_SINGLE_VLAN_ENABLED
    pdata->vlan = vid;
    hw_ops->enable_rx_vlan_filtering(pdata);
#else
    hw_ops->update_vlan_hash_table(pdata);
#endif
    DPRINTK("fxgmac,add rx vlan %d\n", vid);
    return 0;
}

static int fxgmac_vlan_rx_kill_vid(struct net_device *netdev,
                 __be16 proto,
                 u16 vid)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;

    clear_bit(vid, pdata->active_vlans);
#if FXGMAC_FILTER_SINGLE_VLAN_ENABLED
    pdata->vlan = 0;
    hw_ops->disable_rx_vlan_filtering(pdata);
#else
    hw_ops->update_vlan_hash_table(pdata);
#endif

    DPRINTK("fxgmac,del rx vlan %d\n", vid);
    return 0;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void fxgmac_poll_controller(struct net_device *netdev)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    struct fxgmac_channel *channel;
    unsigned int i;

    if (pdata->per_channel_irq) {
        channel = pdata->channel_head;
        for (i = 0; i < pdata->channel_count; i++, channel++)
            fxgmac_dma_isr(channel->dma_irq, channel);
    } else {
        disable_irq(pdata->dev_irq);
        fxgmac_isr(pdata->dev_irq, pdata);
        enable_irq(pdata->dev_irq);
    }
}
#endif /* CONFIG_NET_POLL_CONTROLLER */

static int fxgmac_set_features(struct net_device *netdev,
                 netdev_features_t features)
{
    netdev_features_t rxhash, rxcsum, rxvlan, rxvlan_filter;
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;
    int ret = 0;

    rxhash = pdata->netdev_features & NETIF_F_RXHASH;
    rxcsum = pdata->netdev_features & NETIF_F_RXCSUM;
    rxvlan = pdata->netdev_features & NETIF_F_HW_VLAN_CTAG_RX;
    rxvlan_filter = pdata->netdev_features & NETIF_F_HW_VLAN_CTAG_FILTER;

    if ((features & NETIF_F_RXHASH) && !rxhash)
        ret = hw_ops->enable_rss(pdata);
    else if (!(features & NETIF_F_RXHASH) && rxhash)
        ret = hw_ops->disable_rss(pdata);
    if (ret)
        return ret;

    if ((features & NETIF_F_RXCSUM) && !rxcsum)
        hw_ops->enable_rx_csum(pdata);
    else if (!(features & NETIF_F_RXCSUM) && rxcsum)
        hw_ops->disable_rx_csum(pdata);

    if ((features & NETIF_F_HW_VLAN_CTAG_RX) && !rxvlan)
        hw_ops->enable_rx_vlan_stripping(pdata);
    else if (!(features & NETIF_F_HW_VLAN_CTAG_RX) && rxvlan)
        hw_ops->disable_rx_vlan_stripping(pdata);

    if ((features & NETIF_F_HW_VLAN_CTAG_FILTER) && !rxvlan_filter)
        hw_ops->enable_rx_vlan_filtering(pdata);
    else if (!(features & NETIF_F_HW_VLAN_CTAG_FILTER) && rxvlan_filter)
        hw_ops->disable_rx_vlan_filtering(pdata);

    pdata->netdev_features = features;

    DPRINTK("fxgmac,set features done,%llx\n", (u64)features);
    return 0;
}

static void fxgmac_set_rx_mode(struct net_device *netdev)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;

    hw_ops->config_rx_mode(pdata);
}

static const struct net_device_ops fxgmac_netdev_ops = {
    .ndo_open               = fxgmac_open,
    .ndo_stop               = fxgmac_close,
    .ndo_start_xmit         = fxgmac_xmit,
    .ndo_tx_timeout         = fxgmac_tx_timeout,
    .ndo_get_stats64        = fxgmac_get_stats64,
    .ndo_change_mtu         = fxgmac_change_mtu,
    .ndo_set_mac_address    = fxgmac_set_mac_address,
    .ndo_validate_addr      = eth_validate_addr,
    .ndo_do_ioctl           = fxgmac_ioctl,
    .ndo_vlan_rx_add_vid    = fxgmac_vlan_rx_add_vid,
    .ndo_vlan_rx_kill_vid   = fxgmac_vlan_rx_kill_vid,
#ifdef CONFIG_NET_POLL_CONTROLLER
    .ndo_poll_controller    = fxgmac_poll_controller,
#endif
    .ndo_set_features       = fxgmac_set_features,
    .ndo_set_rx_mode        = fxgmac_set_rx_mode,
};

const struct net_device_ops *fxgmac_get_netdev_ops(void)
{
    return &fxgmac_netdev_ops;
}

static void fxgmac_rx_refresh(struct fxgmac_channel *channel)
{
    struct fxgmac_pdata *pdata = channel->pdata;
    struct fxgmac_ring *ring = channel->rx_ring;
    struct fxgmac_desc_data *desc_data;
#if 0
    struct fxgmac_desc_ops *desc_ops = &pdata->desc_ops;
#endif
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;

    while (ring->dirty != ring->cur) {
        desc_data = FXGMAC_GET_DESC_DATA(ring, ring->dirty);
#if 0
        /* Reset desc_data values */
        desc_ops->unmap_desc_data(pdata, desc_data);

        if (desc_ops->map_rx_buffer(pdata, ring, desc_data))
            break;
#endif
        hw_ops->rx_desc_reset(pdata, desc_data, ring->dirty);
        ring->dirty = FXGMAC_GET_ENTRY(ring->dirty, ring->dma_desc_count);
    }

    /* Make sure everything is written before the register write */
    wmb();

    /* Update the Rx Tail Pointer Register with address of
     * the last cleaned entry
     */
    desc_data = FXGMAC_GET_DESC_DATA(ring, (ring->dirty - 1) & (ring->dma_desc_count - 1));
    writel(lower_32_bits(desc_data->dma_desc_addr), FXGMAC_DMA_REG(channel, DMA_CH_RDTR_LO));
}

static struct sk_buff *fxgmac_create_skb(struct fxgmac_pdata *pdata,
                     struct napi_struct *napi,
                     struct fxgmac_desc_data *desc_data,
                     unsigned int len)
{
#if 0
    unsigned int copy_len;
    struct sk_buff *skb;
    u8 *packet;

    skb = napi_alloc_skb(napi, desc_data->rx.hdr.dma_len);
    if (!skb)
        return NULL;

    /* Start with the header buffer which may contain just the header
     * or the header plus data
     */
    dma_sync_single_range_for_cpu(pdata->dev, desc_data->rx.hdr.dma_base,
                        desc_data->rx.hdr.dma_off,
                        desc_data->rx.hdr.dma_len,
                        DMA_FROM_DEVICE);

    packet = page_address(desc_data->rx.hdr.pa.pages) +
         desc_data->rx.hdr.pa.pages_offset;
    copy_len = (desc_data->rx.hdr_len) ? desc_data->rx.hdr_len : len;
    copy_len = min(desc_data->rx.hdr.dma_len, copy_len);
    skb_copy_to_linear_data(skb, packet, copy_len);
    skb_put(skb, copy_len);

    len -= copy_len;
    if (len) {
        /* Add the remaining data as a frag */
        dma_sync_single_range_for_cpu(pdata->dev,
                          desc_data->rx.buf.dma_base,
                          desc_data->rx.buf.dma_off,
                          desc_data->rx.buf.dma_len,
                          DMA_FROM_DEVICE);

        skb_add_rx_frag(skb, skb_shinfo(skb)->nr_frags,
                desc_data->rx.buf.pa.pages,
                desc_data->rx.buf.pa.pages_offset,
                len, desc_data->rx.buf.dma_len);
        desc_data->rx.buf.pa.pages = NULL;
    }

    return skb;
#endif
    struct sk_buff *skb;
    skb = __netdev_alloc_skb_ip_align(pdata->netdev, len, GFP_KERNEL);
    if (!skb) {
        netdev_err(pdata->netdev, "%s: Rx init fails; skb is NULL\n", __func__);
        return NULL;
    }

    dma_sync_single_for_cpu(pdata->dev, desc_data->rx.buf.dma_base, len, DMA_FROM_DEVICE);
	skb_copy_to_linear_data(skb, desc_data->skb->data, len);
	skb_put(skb, len);
    dma_sync_single_for_device(pdata->dev, desc_data->rx.buf.dma_base, len, DMA_FROM_DEVICE);

    return skb;
}

static int fxgmac_tx_poll(struct fxgmac_channel *channel)
{
    struct fxgmac_pdata *pdata = channel->pdata;
    struct fxgmac_ring *ring = channel->tx_ring;
    struct net_device *netdev = pdata->netdev;
    unsigned int tx_packets = 0, tx_bytes = 0;
    struct fxgmac_desc_data *desc_data;
    struct fxgmac_dma_desc *dma_desc;
    struct fxgmac_desc_ops *desc_ops;
    struct fxgmac_hw_ops *hw_ops;
    struct netdev_queue *txq;
    int processed = 0;
    unsigned int cur;

    static int fxgmac_restart_need = 0;
    static u32 change_cnt = 0;
    static u32 reg_cur_pre = 0xffffffff;

#if FXGMAC_TX_HANG_TIMER_EN
    static u32 reg_cur = 0;
#endif

    desc_ops = &pdata->desc_ops;
    hw_ops = &pdata->hw_ops;

    /* Nothing to do if there isn't a Tx ring for this channel */
    if (!ring){
        if(netif_msg_tx_done(pdata) && (channel->queue_index < pdata->tx_q_count)) DPRINTK("tx_poll, null point to ring %d\n", channel->queue_index);
        return 0;
    }
    if((ring->cur != ring->dirty) && (netif_msg_tx_done(pdata)))
        DPRINTK("tx_poll callin, ring_cur=%d,ring_dirty=%d,qIdx=%d\n", ring->cur, ring->dirty, channel->queue_index);

    cur = ring->cur;

    /* Be sure we get ring->cur before accessing descriptor data */
    smp_rmb();

    txq = netdev_get_tx_queue(netdev, channel->queue_index);

    while (ring->dirty != cur) {
        desc_data = FXGMAC_GET_DESC_DATA(ring, ring->dirty);
        dma_desc = desc_data->dma_desc;

        if (!hw_ops->tx_complete(dma_desc)) {
#if FXGMAC_TRIGGER_TX_HANG
            struct net_device *netdev = pdata->netdev;
            #define FXGMAC_HANG_THRESHOLD 	1
            //static u32 reg_tail = 0, reg_tail_pre = 0xffffffff;

            reg_cur = readl(FXGMAC_DMA_REG(channel, 0x44/* tx desc curr pointer reg */));

            if(reg_cur != reg_cur_pre){
                reg_cur_pre = reg_cur;
                change_cnt = 0;
            } else {
                change_cnt++;
            }

            if (change_cnt > 2)
            {
                //change_cnt = 0;

                DPRINTK("after complete check, cur=%d, dirty=%d,qIdx=%d, hw desc cur=%#x, pre=%#x\n", ring->cur, ring->dirty, channel->queue_index, 
                    reg_cur, reg_cur_pre);

                if((ring->cur > ring->dirty) && ((ring->cur - ring->dirty) > FXGMAC_HANG_THRESHOLD) ) {
                    DPRINTK("after complete check warning..., too many TBD occupied by HW, 0xdbbb, %d.\n", (ring->cur - ring->dirty));
                    (* ((u32 *)(netdev->base_addr + 0x1000))) = 0xdbbb;

                    if(!fxgmac_restart_need ) {
                        schedule_work(&pdata->restart_work);
                        fxgmac_restart_need = 1;
                        change_cnt = 0;
                    }
                }else if((ring->cur < ring->dirty) && ((ring->cur + (ring->dma_desc_count - ring->dirty)) > FXGMAC_HANG_THRESHOLD) ) {
                    DPRINTK("after complete check warning..., too many TBD occupied by HW, 0xdb00, %d.\n", (ring->cur + (ring->dma_desc_count - ring->dirty)));	
                    (* ((u32 *)(netdev->base_addr + 0x1000))) = 0xdb00;

                    if(!fxgmac_restart_need ) {
                        schedule_work(&pdata->restart_work);
                        fxgmac_restart_need = 1;
                        change_cnt = 0;
                    }
                }
            }
#endif
#if FXGMAC_TX_HANG_TIMER_EN
            if((!pdata->tx_hang_restart_queuing) && (!channel->tx_hang_timer_active)) {
                reg_cur = ring->dirty;
                if(reg_cur_pre != reg_cur) {
                    reg_cur_pre = reg_cur;
                    change_cnt = 0;
                }else {
                    change_cnt++;
                }

                if (change_cnt > 4)
                {
#if FXGMAC_TX_HANG_CHECH_DIRTY
                    channel->tx_hang_hw_cur = ring->dirty;
#else
                    channel->tx_hang_hw_cur = readl(FXGMAC_DMA_REG(channel, 0x44/* tx desc curr pointer reg */));
#endif
                    /* double check for race conditione */
                    if ((!pdata->tx_hang_restart_queuing) && (!channel->tx_hang_timer_active)) {
                    DPRINTK("tx_hang polling: start timer at desc %u, timer act=%u, queuing=%u, qidx=%u.\n", reg_cur, channel->tx_hang_timer_active, pdata->tx_hang_restart_queuing, channel->queue_index);
                    fxgmac_tx_hang_timer_start(channel);
                    }
                }
            }else if (pdata->tx_hang_restart_queuing) {
                //if(netif_msg_drv(pdata)) DPRINTK("tx_hang_timer_handler: restart scheduled.\n");	
            }
#endif

            break;
        }

        reg_cur_pre = 0xffffffff;
        fxgmac_restart_need = 0;
        change_cnt = 0;

        /* Make sure descriptor fields are read after reading
         * the OWN bit
         */
        dma_rmb();

        if (netif_msg_tx_done(pdata))
            fxgmac_dump_tx_desc(pdata, ring, ring->dirty, 1, 0);

        if (hw_ops->is_last_desc(dma_desc)) {
            tx_packets += desc_data->tx.packets;
            tx_bytes += desc_data->tx.bytes;
        }

        /* Free the SKB and reset the descriptor for re-use */
        desc_ops->unmap_desc_data(pdata, desc_data);
        hw_ops->tx_desc_reset(desc_data);

        processed++;
        //ring->dirty++;
        ring->dirty = FXGMAC_GET_ENTRY(ring->dirty, ring->dma_desc_count);
    }

    if (!processed)
        return 0;

    netdev_tx_completed_queue(txq, tx_packets, tx_bytes);

    if ((ring->tx.queue_stopped == 1) &&
        (fxgmac_tx_avail_desc(ring) > FXGMAC_TX_DESC_MIN_FREE)) {
        ring->tx.queue_stopped = 0;
        netif_tx_wake_queue(txq);
    }

    //yzhang comment out to reduce print
    if(netif_msg_tx_done(pdata)){
        DPRINTK("tx_poll callout, processed=%d\n", processed);
    }

    return processed;
}
extern void fxgmac_print_pkt(struct net_device *netdev,
                      struct sk_buff *skb, bool tx_rx);

static unsigned int channel_is_onpolling = 0;
static int fxgmac_rx_poll(struct fxgmac_channel *channel, int budget)
{
    struct fxgmac_pdata *pdata = channel->pdata;
    struct fxgmac_ring *ring = channel->rx_ring;
    struct net_device *netdev = pdata->netdev;
    unsigned int len;
    unsigned int context_next, context;
    struct fxgmac_desc_data *desc_data;
    struct fxgmac_pkt_info *pkt_info;
    unsigned int incomplete;
    struct fxgmac_hw_ops *hw_ops;
    struct napi_struct *napi;
    struct sk_buff *skb;
    int packet_count = 0;
    u32 ipce,iphe;

    hw_ops = &pdata->hw_ops;

    /* Nothing to do if there isn't a Rx ring for this channel */
    if (!ring)
        return 0;

    incomplete = 0;
    context_next = 0;

    napi = (pdata->per_channel_irq) ? &channel->napi_rx : &pdata->napi;

    desc_data = FXGMAC_GET_DESC_DATA(ring, ring->cur);
    pkt_info = &ring->pkt_info;

    while (packet_count < budget) {
        memset(pkt_info, 0, sizeof(*pkt_info));
        skb = NULL;
        len = 0;

    read_again:
        desc_data = FXGMAC_GET_DESC_DATA(ring, ring->cur);

        if (fxgmac_rx_dirty_desc(ring) > FXGMAC_RX_DESC_MAX_DIRTY)
            fxgmac_rx_refresh(channel);

        if (hw_ops->dev_read(channel))
            break;

        ring->cur = FXGMAC_GET_ENTRY(ring->cur, ring->dma_desc_count);

        incomplete = FXGMAC_GET_REG_BITS(
                    pkt_info->attributes,
                    RX_PACKET_ATTRIBUTES_INCOMPLETE_POS,
                    RX_PACKET_ATTRIBUTES_INCOMPLETE_LEN);
        context_next = FXGMAC_GET_REG_BITS(
                    pkt_info->attributes,
                    RX_PACKET_ATTRIBUTES_CONTEXT_NEXT_POS,
                    RX_PACKET_ATTRIBUTES_CONTEXT_NEXT_LEN);
        context = FXGMAC_GET_REG_BITS(
                    pkt_info->attributes,
                    RX_PACKET_ATTRIBUTES_CONTEXT_POS,
                    RX_PACKET_ATTRIBUTES_CONTEXT_LEN);

        if (incomplete || context_next)
            goto read_again;

        if (pkt_info->errors) {
            netif_err(pdata, rx_err, netdev,  "error in received packet\n");
            dev_kfree_skb(skb);
            goto next_packet;
        }

        if (!context) {
            len = desc_data->rx.len;
            if (len > pdata->rx_buf_size) {
                if (net_ratelimit())
                    netdev_err(pdata->netdev,"len %d larger than size (%d)\n", len, pdata->rx_buf_size);
                pdata->netdev->stats.rx_dropped++;
                goto next_packet;
            }

            if (len == 0) {
                if (net_ratelimit())
                    netdev_err(pdata->netdev,"A packet of length 0 was received\n");
                goto next_packet;
            }

            if (len && !skb) {
                skb = fxgmac_create_skb(pdata, napi, desc_data, len);
                if (unlikely(!skb)) {
                    if (net_ratelimit())
                        netdev_warn(pdata->netdev, "packet dropped\n");
                    pdata->netdev->stats.rx_length_errors++;
                    goto next_packet;
                }
            }
        }

        if (!skb)
            goto next_packet;

        skb_checksum_none_assert(skb);
        if (netdev->features & NETIF_F_RXCSUM)
        {
            ipce = FXGMAC_GET_REG_BITS_LE(desc_data->dma_desc->desc1,
                                        RX_NORMAL_DESC1_WB_IPCE_POS,
                                        RX_NORMAL_DESC1_WB_IPCE_LEN);
            iphe = FXGMAC_GET_REG_BITS_LE(desc_data->dma_desc->desc1,
                                        RX_NORMAL_DESC1_WB_IPHE_POS,
                                        RX_NORMAL_DESC1_WB_IPHE_LEN);
            /* if csum error,let the stack verify checksum errors.otherwise don't verify */
            if (!ipce && !iphe && FXGMAC_GET_REG_BITS(pkt_info->attributes,
                                        RX_PACKET_ATTRIBUTES_CSUM_DONE_POS,
                                        RX_PACKET_ATTRIBUTES_CSUM_DONE_LEN))
                skb->ip_summed = CHECKSUM_UNNECESSARY;
        }

        if (FXGMAC_GET_REG_BITS(pkt_info->attributes,
                    RX_PACKET_ATTRIBUTES_VLAN_CTAG_POS,
                    RX_PACKET_ATTRIBUTES_VLAN_CTAG_LEN)) {
            __vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q),
                           pkt_info->vlan_ctag);
            pdata->stats.rx_vlan_packets++;
        }

        if (FXGMAC_GET_REG_BITS(pkt_info->attributes,
                    RX_PACKET_ATTRIBUTES_RSS_HASH_POS,
                    RX_PACKET_ATTRIBUTES_RSS_HASH_LEN))
            skb_set_hash(skb, pkt_info->rss_hash,
                     pkt_info->rss_hash_type);

        skb->dev = netdev;
        skb->protocol = eth_type_trans(skb, netdev);
        skb_record_rx_queue(skb, channel->queue_index);

        if(pdata->fxgmac_test_tso_flag)
        {
            /* tso test */
            if(pdata->fxgmac_test_tso_seg_num == 1)
            {
                /* last segment */
                if(pdata->fxgmac_test_last_tso_len == skb->len + FXGMAC_TEST_MAC_HEAD_LEN)
                {
                    /* receive last segment, reset flag */
                    pdata->fxgmac_test_tso_flag = false;
                    pdata->fxgmac_test_tso_seg_num = 0;
                    pdata->fxgmac_test_packet_len = 0;
                    pdata->fxgmac_test_last_tso_len = 0;

                    /* process packet */
                    if((pdata->fxgmac_test_skb_arr_in_index + 1) % FXGMAC_MAX_DBG_TEST_PKT != pdata->fxgmac_test_skb_arr_out_index){
                        struct sk_buff *tmpskb = skb_copy(skb, GFP_ATOMIC);
                        skb_push(tmpskb, FXGMAC_TEST_MAC_HEAD_LEN);

                        pdata->fxgmac_test_skb_array[pdata->fxgmac_test_skb_arr_in_index] = tmpskb;
                        pdata->fxgmac_test_skb_arr_in_index = (pdata->fxgmac_test_skb_arr_in_index + 1) % FXGMAC_MAX_DBG_TEST_PKT;
                    }
                    else{
                        DPRINTK("loopback test buffer is full.");
                    }
                }
            }
            else /* non last segment */
            {
                if(pdata->fxgmac_test_packet_len == skb->len + FXGMAC_TEST_MAC_HEAD_LEN){
                    /* receive a segment */
                    pdata->fxgmac_test_tso_seg_num--;

                    /* process packet */
                    if((pdata->fxgmac_test_skb_arr_in_index + 1) % FXGMAC_MAX_DBG_TEST_PKT != pdata->fxgmac_test_skb_arr_out_index){
                        struct sk_buff *tmpskb = skb_copy(skb, GFP_ATOMIC);
                        skb_push(tmpskb, FXGMAC_TEST_MAC_HEAD_LEN);

                        pdata->fxgmac_test_skb_array[pdata->fxgmac_test_skb_arr_in_index] = tmpskb;
                        pdata->fxgmac_test_skb_arr_in_index = (pdata->fxgmac_test_skb_arr_in_index + 1) % FXGMAC_MAX_DBG_TEST_PKT;
                    }
                    else{
                        DPRINTK("loopback test buffer is full.");
                    }
                }
            }
        }
        else if(pdata->fxgmac_test_packet_len != 0)
        {
            /* xsum and phy loopback test */
            if(pdata->fxgmac_test_packet_len == skb->len + FXGMAC_TEST_MAC_HEAD_LEN)
            {
                /* reset fxg_packet_len */
                pdata->fxgmac_test_packet_len = 0;

                if((pdata->fxgmac_test_skb_arr_in_index + 1) % FXGMAC_MAX_DBG_TEST_PKT != pdata->fxgmac_test_skb_arr_out_index){
                    struct sk_buff *tmpskb = skb_copy(skb, GFP_ATOMIC);
                    skb_push(tmpskb, FXGMAC_TEST_MAC_HEAD_LEN); 

                    pdata->fxgmac_test_skb_array[pdata->fxgmac_test_skb_arr_in_index] = tmpskb;
                    pdata->fxgmac_test_skb_arr_in_index = (pdata->fxgmac_test_skb_arr_in_index + 1) % FXGMAC_MAX_DBG_TEST_PKT;
                }
                else{
                    DPRINTK("loopback test buffer is full.");
                }
            }
        }
#if 1
        napi_gro_receive(napi, skb);
#else
        netif_receive_skb(skb);
#endif

next_packet:
        packet_count++;

        pdata->netdev->stats.rx_packets++;
        pdata->netdev->stats.rx_bytes += len;
    }

    fxgmac_rx_refresh(channel);

    return packet_count;
}

static int fxgmac_one_poll_tx(struct napi_struct *napi, int budget)
{
    struct fxgmac_channel *channel = container_of(napi,
                        struct fxgmac_channel,
                        napi_tx);
    int ret = 0;
    struct fxgmac_pdata *pdata = channel->pdata;
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;

    pdata->stats.msix_ch0_napi_napi_tx++;

    ret = fxgmac_tx_poll(channel);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0))
    if (napi_complete_done(napi, 0)) {
        hw_ops->enable_msix_one_interrupt(pdata, MSI_ID_TXQ0);
    }
#else
    napi_complete(napi);
    hw_ops->enable_msix_one_interrupt(pdata, MSI_ID_TXQ0);
#endif
    return 0;

}

static int fxgmac_one_poll_rx(struct napi_struct *napi, int budget)
{
    struct fxgmac_channel *channel = container_of(napi,
                        struct fxgmac_channel,
                        napi_rx);
    int processed = 0;
    struct fxgmac_pdata *pdata = channel->pdata;
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;

    processed = fxgmac_rx_poll(channel, budget);
    if (processed < budget) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0))
        /* if there no interrupt occured when this interrupt running,struct napi's state is NAPIF_STATE_SCHED,
        * napi_complete_done return true and we can enable irq,it will not cause unbalanced iqr issure.
        * if there more interrupt occured when this interrupt running,struct napi's state is NAPIF_STATE_SCHED | NAPIF_STATE_MISSED
        * because napi_schedule_prep will make it. At this time napi_complete_done will return false and 
        * schedule poll again because of NAPIF_STATE_MISSED,it will cause unbalanced irq issure.
        */
        if (napi_complete_done(napi, processed)) {
            hw_ops->enable_msix_one_interrupt(pdata, channel->queue_index);
        }
#else
        napi_complete(napi);
        hw_ops->enable_msix_one_interrupt(pdata, channel->queue_index);
#endif
    }
    channel_is_onpolling = channel->queue_index; 

    return processed;
}

static int fxgmac_all_poll(struct napi_struct *napi, int budget)
{
    struct fxgmac_pdata *pdata = container_of(napi,
                             struct fxgmac_pdata,
                             napi);
    struct fxgmac_channel *channel;
    int processed, last_processed;
    int ring_budget;
    unsigned int i;

#if FXGMAC_FPGA_VER_20210507
    /* below line is added for fpga after 20210507 */
    struct net_device *netdev = pdata->netdev;
    u32 val_mgmt_intcrtl0;
#endif

    //yzhang comment out
    if(netif_msg_rx_status(pdata)){
        DPRINTK("rx all_poll callin budget=%d\n", budget);
    }

    processed = 0;
    ring_budget = budget / pdata->rx_ring_count;
    do {
        last_processed = processed;

        channel = pdata->channel_head;
        for (i = 0; i < pdata->channel_count; i++, channel++) {
            fxgmac_cur_rx_ch_polling = i;
            /* Cleanup Tx ring first */
#if FXGMAC_FULL_TX_CHANNEL
            fxgmac_tx_poll(channel);
#else
            /*since only 1 tx channel supported in this version, poll ch 0 always. */
            fxgmac_tx_poll((struct fxgmac_channel *)(pdata->channel_head + (i % pdata->tx_q_count)));
#endif
            /* Process Rx ring next */
            if (ring_budget > (budget - processed))
                ring_budget = budget - processed;

            channel_is_onpolling = i; 
            processed += fxgmac_rx_poll(channel, ring_budget);

#if FXGMAC_TX_INTERRUPT_EN
            if (FXGMAC_IS_CHANNEL_WITH_TX_IRQ(i))
                pdata->stats.msix_ch0_napi_napi_tx++;
#endif
        }
    } while ((processed < budget) && (processed != last_processed));

    /* If we processed everything, we are done */
    if (processed < budget) {
        /* Turn off polling */
        napi_complete_done(napi, processed);

        /* Enable Tx and Rx interrupts */
        fxgmac_enable_rx_tx_ints(pdata);

#if FXGMAC_FPGA_VER_20210507
        val_mgmt_intcrtl0 = (u32)readl((const volatile void *)(netdev->base_addr + MGMT_INT_CTRL0));

        //enable RX channel interrupts
        val_mgmt_intcrtl0 = FXGMAC_SET_REG_BITS(val_mgmt_intcrtl0, MGMT_INT_CTRL0_INT_MASK_RXCH_POS,
                             MGMT_INT_CTRL0_INT_STATUS_RXCH_LEN,
                             0);
#if FXGMAC_TX_INTERRUPT_EN
        //enable TX channel interrupts
        val_mgmt_intcrtl0 = FXGMAC_SET_REG_BITS(val_mgmt_intcrtl0, MGMT_INT_CTRL0_INT_MASK_TXCH_POS,
                             MGMT_INT_CTRL0_INT_STATUS_TXCH_LEN,
                             0);
#endif

        writel(val_mgmt_intcrtl0, (volatile void *)(netdev->base_addr + MGMT_INT_CTRL0));
#endif
    }

    if((processed) && (netif_msg_rx_status(pdata))) { //yzhang for debug
        DPRINTK("rx all_poll callout received = %d\n", processed);
    }

    return processed;
}
