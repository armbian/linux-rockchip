/*++

Copyright (c) 2021 Motor-comm Corporation. 
Confidential and Proprietary. All rights reserved.

This is Motor-comm Corporation NIC driver relevant files. Please don't copy, modify,
distribute without commercial permission.

--*/


#include <linux/ethtool.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>

#include "fuxi-gmac.h"
#include "fuxi-gmac-reg.h"

struct fxgmac_stats_desc {
    char stat_string[ETH_GSTRING_LEN];
    int stat_offset;
};

#define FXGMAC_STAT(str, var)					\
	{							\
		str,						\
		offsetof(struct fxgmac_pdata, stats.var),	\
	}

static const struct fxgmac_stats_desc fxgmac_gstring_stats[] = {
    /* MMC TX counters */
    FXGMAC_STAT("tx_bytes", txoctetcount_gb),
    FXGMAC_STAT("tx_bytes_good", txoctetcount_g),
    FXGMAC_STAT("tx_packets", txframecount_gb),
    FXGMAC_STAT("tx_packets_good", txframecount_g),
    FXGMAC_STAT("tx_unicast_packets", txunicastframes_gb),
    FXGMAC_STAT("tx_broadcast_packets", txbroadcastframes_gb),
    FXGMAC_STAT("tx_broadcast_packets_good", txbroadcastframes_g),
    FXGMAC_STAT("tx_multicast_packets", txmulticastframes_gb),
    FXGMAC_STAT("tx_multicast_packets_good", txmulticastframes_g),
    FXGMAC_STAT("tx_vlan_packets_good", txvlanframes_g),
    FXGMAC_STAT("tx_64_byte_packets", tx64octets_gb),
    FXGMAC_STAT("tx_65_to_127_byte_packets", tx65to127octets_gb),
    FXGMAC_STAT("tx_128_to_255_byte_packets", tx128to255octets_gb),
    FXGMAC_STAT("tx_256_to_511_byte_packets", tx256to511octets_gb),
    FXGMAC_STAT("tx_512_to_1023_byte_packets", tx512to1023octets_gb),
    FXGMAC_STAT("tx_1024_to_max_byte_packets", tx1024tomaxoctets_gb),
    FXGMAC_STAT("tx_underflow_errors", txunderflowerror),
    FXGMAC_STAT("tx_pause_frames", txpauseframes),
    FXGMAC_STAT("tx_single_collision", txsinglecollision_g),
    FXGMAC_STAT("tx_multiple_collision", txmultiplecollision_g),
    FXGMAC_STAT("tx_deferred_frames", txdeferredframes),
    FXGMAC_STAT("tx_late_collision_frames", txlatecollisionframes),
    FXGMAC_STAT("tx_excessive_collision_frames", txexcessivecollisionframes),
    FXGMAC_STAT("tx_carrier_error_frames", txcarriererrorframes),
    FXGMAC_STAT("tx_excessive_deferral_error", txexcessivedeferralerror),
    FXGMAC_STAT("tx_oversize_frames_good", txoversize_g),

    /* MMC RX counters */
    FXGMAC_STAT("rx_bytes", rxoctetcount_gb),
    FXGMAC_STAT("rx_bytes_good", rxoctetcount_g),
    FXGMAC_STAT("rx_packets", rxframecount_gb),
    FXGMAC_STAT("rx_unicast_packets_good", rxunicastframes_g),
    FXGMAC_STAT("rx_broadcast_packets_good", rxbroadcastframes_g),
    FXGMAC_STAT("rx_multicast_packets_good", rxmulticastframes_g),
    FXGMAC_STAT("rx_vlan_packets_mac", rxvlanframes_gb),
    FXGMAC_STAT("rx_64_byte_packets", rx64octets_gb),
    FXGMAC_STAT("rx_65_to_127_byte_packets", rx65to127octets_gb),
    FXGMAC_STAT("rx_128_to_255_byte_packets", rx128to255octets_gb),
    FXGMAC_STAT("rx_256_to_511_byte_packets", rx256to511octets_gb),
    FXGMAC_STAT("rx_512_to_1023_byte_packets", rx512to1023octets_gb),
    FXGMAC_STAT("rx_1024_to_max_byte_packets", rx1024tomaxoctets_gb),
    FXGMAC_STAT("rx_undersize_packets_good", rxundersize_g),
    FXGMAC_STAT("rx_oversize_packets_good", rxoversize_g),
    FXGMAC_STAT("rx_crc_errors", rxcrcerror),
    FXGMAC_STAT("rx_align_error", rxalignerror),
    FXGMAC_STAT("rx_crc_errors_small_packets", rxrunterror),
    FXGMAC_STAT("rx_crc_errors_giant_packets", rxjabbererror),
    FXGMAC_STAT("rx_length_errors", rxlengtherror),
    FXGMAC_STAT("rx_out_of_range_errors", rxoutofrangetype),
    FXGMAC_STAT("rx_fifo_overflow_errors", rxfifooverflow),
    FXGMAC_STAT("rx_watchdog_errors", rxwatchdogerror),
    FXGMAC_STAT("rx_pause_frames", rxpauseframes),
    FXGMAC_STAT("rx_receive_error_frames", rxreceiveerrorframe),
    FXGMAC_STAT("rx_control_frames_good", rxcontrolframe_g),

    /* Extra counters */
    FXGMAC_STAT("tx_tso_packets", tx_tso_packets),
    FXGMAC_STAT("rx_split_header_packets", rx_split_header_packets),
    FXGMAC_STAT("tx_process_stopped", tx_process_stopped),
    FXGMAC_STAT("rx_process_stopped", rx_process_stopped),
    FXGMAC_STAT("tx_buffer_unavailable", tx_buffer_unavailable),
    FXGMAC_STAT("rx_buffer_unavailable", rx_buffer_unavailable),
    FXGMAC_STAT("fatal_bus_error", fatal_bus_error),
    FXGMAC_STAT("tx_vlan_packets_net", tx_vlan_packets),
    FXGMAC_STAT("rx_vlan_packets_net", rx_vlan_packets),
    FXGMAC_STAT("napi_poll_isr", napi_poll_isr),
    FXGMAC_STAT("napi_poll_txtimer", napi_poll_txtimer),
    FXGMAC_STAT("alive_cnt_txtimer", cnt_alive_txtimer),
#if FXGMAC_TX_HANG_TIMER_EN
    FXGMAC_STAT("alive_cnt_tx_hang_timer", cnt_alive_tx_hang_timer),
    FXGMAC_STAT("cnt_tx_hang", cnt_tx_hang),
#endif
    FXGMAC_STAT("ephy_poll_timer", ephy_poll_timer_cnt),
    FXGMAC_STAT("ephy_link_change_cnt", ephy_link_change_cnt),
    FXGMAC_STAT("mgmt_int_isr", mgmt_int_isr),
    FXGMAC_STAT("msix_int_isr", msix_int_isr),
    FXGMAC_STAT("msix_int_isr_cur", msix_int_isr_cur),
    FXGMAC_STAT("msix_ch0_napi_isr", msix_ch0_napi_isr),
    FXGMAC_STAT("msix_ch1_napi_isr", msix_ch1_napi_isr),
    FXGMAC_STAT("msix_ch2_napi_isr", msix_ch2_napi_isr),
    FXGMAC_STAT("msix_ch3_napi_isr", msix_ch3_napi_isr),
#if FXGMAC_TX_INTERRUPT_EN
    FXGMAC_STAT("msix_ch0_napi_isr_tx", msix_ch0_napi_isr_tx),
    FXGMAC_STAT("msix_ch0_napi_napi_tx", msix_ch0_napi_napi_tx),
    FXGMAC_STAT("msix_ch0_napi_sch_tx", msix_ch0_napi_sch_tx),
#endif		

};

#define FXGMAC_STATS_COUNT	ARRAY_SIZE(fxgmac_gstring_stats)

static void fxgmac_ethtool_get_drvinfo(struct net_device *netdev,
				       struct ethtool_drvinfo *drvinfo)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    u32 ver = pdata->hw_feat.version;
    u32 sver, devid, userver;

    strlcpy(drvinfo->driver, pdata->drv_name, sizeof(drvinfo->driver));
    strlcpy(drvinfo->version, pdata->drv_ver, sizeof(drvinfo->version));
    strlcpy(drvinfo->bus_info, dev_name(pdata->dev),
    sizeof(drvinfo->bus_info));
    /*
    * D|DEVID: Indicates the Device family
    * U|USERVER: User-defined Version
    */
    sver = FXGMAC_GET_REG_BITS(ver, MAC_VR_SVER_POS,
    		      MAC_VR_SVER_LEN);
    devid = FXGMAC_GET_REG_BITS(ver, MAC_VR_DEVID_POS,
    		    MAC_VR_DEVID_LEN);
    userver = FXGMAC_GET_REG_BITS(ver, MAC_VR_USERVER_POS,
    		      MAC_VR_USERVER_LEN);
    /*DPRINTK("xlgma: No userver (%x) here, sver (%x) should be 0x51\n", userver, sver);*/
    snprintf(drvinfo->fw_version, sizeof(drvinfo->fw_version),
     "S.D.U: %x.%x.%x", sver, devid, userver);
}

static u32 fxgmac_ethtool_get_msglevel(struct net_device *netdev)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);

    return pdata->msg_enable;
}

static void fxgmac_ethtool_set_msglevel(struct net_device *netdev,
					u32 msglevel)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);

    DPRINTK("fxmac, set msglvl from %08x to %08x\n", pdata->msg_enable, msglevel); 
    default_msg_level = msglevel;
    pdata->msg_enable = msglevel;
}

static void fxgmac_ethtool_get_channels(struct net_device *netdev,
					struct ethtool_channels *channel)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
#if (FXGMAC_RSS_FEATURE_ENABLED)
    /* report maximum channels */
    channel->max_combined = FXGMAC_MAX_DMA_CHANNELS;
    channel->max_other = 0;
    channel->other_count = 0;

    /* record RSS queues */
    channel->combined_count = FXGMAC_MAX_DMA_CHANNELS;

    /* nothing else to report if RSS is disabled */
    if (channel->combined_count == 1)
    	return;
    DPRINTK("fxmac rss, get channels max=(combined %d,other %d),count(combined %d,other %d)\n", channel->max_combined, channel->max_other, channel->combined_count, channel->other_count); 
#endif

    channel->max_rx = FXGMAC_MAX_DMA_CHANNELS;
    channel->max_tx = FXGMAC_MAX_DMA_CHANNELS;
    channel->rx_count = pdata->rx_q_count;
    channel->tx_count = pdata->tx_q_count;
    DPRINTK("fxmac, get channels max=(rx %d,tx %d),count(%d,%d)\n", channel->max_rx, channel->max_tx, channel->rx_count, channel->tx_count); 
}

#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0) )
static int fxgmac_ethtool_get_coalesce(struct net_device *netdev, 
                                        struct ethtool_coalesce *ec,
                                        struct kernel_ethtool_coalesce *kernel_coal,
                                        struct netlink_ext_ack *extack)
#else
static int fxgmac_ethtool_get_coalesce(struct net_device *netdev, struct ethtool_coalesce *ec)
#endif
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);

    memset(ec, 0, sizeof(struct ethtool_coalesce));
    ec->rx_coalesce_usecs = pdata->rx_usecs;
    ec->rx_max_coalesced_frames = pdata->rx_frames;
    ec->tx_max_coalesced_frames = pdata->tx_frames;

    DPRINTK("fxmac, get coalesce\n"); 
    return 0;
}

#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0) )
static int fxgmac_ethtool_set_coalesce(struct net_device *netdev, 
                                        struct ethtool_coalesce *ec,
                                        struct kernel_ethtool_coalesce *kernel_coal,
                                        struct netlink_ext_ack *extack)
#else
static int fxgmac_ethtool_set_coalesce(struct net_device *netdev, struct ethtool_coalesce *ec)
#endif
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;
    unsigned int rx_frames, rx_riwt, rx_usecs;
    unsigned int tx_frames;

    /* Check for not supported parameters */
    if ((ec->rx_coalesce_usecs_irq) || (ec->rx_max_coalesced_frames_irq) ||
        (ec->tx_coalesce_usecs) || (ec->tx_coalesce_usecs_high) ||
        (ec->tx_max_coalesced_frames_irq) || (ec->tx_coalesce_usecs_irq) ||
        (ec->stats_block_coalesce_usecs) ||  (ec->pkt_rate_low) ||
        (ec->use_adaptive_rx_coalesce) || (ec->use_adaptive_tx_coalesce) ||
        (ec->rx_max_coalesced_frames_low) || (ec->rx_coalesce_usecs_low) ||
        (ec->tx_coalesce_usecs_low) || (ec->tx_max_coalesced_frames_low) ||
        (ec->pkt_rate_high) || (ec->rx_coalesce_usecs_high) ||
        (ec->rx_max_coalesced_frames_high) ||
        (ec->tx_max_coalesced_frames_high) ||
        (ec->rate_sample_interval))
    	return -EOPNOTSUPP;

    rx_usecs = ec->rx_coalesce_usecs;
    rx_riwt = hw_ops->usec_to_riwt(pdata, rx_usecs);
    rx_frames = ec->rx_max_coalesced_frames;
    tx_frames = ec->tx_max_coalesced_frames;

    if ((rx_riwt > FXGMAC_MAX_DMA_RIWT) ||
        (rx_riwt < FXGMAC_MIN_DMA_RIWT) ||
        (rx_frames > pdata->rx_desc_count))
    	return -EINVAL;

    if (tx_frames > pdata->tx_desc_count)
    	return -EINVAL;

    pdata->rx_riwt = rx_riwt;
    pdata->rx_usecs = rx_usecs;
    pdata->rx_frames = rx_frames;
    hw_ops->config_rx_coalesce(pdata);

    pdata->tx_frames = tx_frames;
    hw_ops->config_tx_coalesce(pdata);

    DPRINTK("fxmac, set coalesce\n"); 
    return 0;
}

#if (FXGMAC_RSS_FEATURE_ENABLED)
static u32 fxgmac_get_rxfh_key_size(struct net_device *netdev)
{
    return FXGMAC_RSS_HASH_KEY_SIZE;
}

static u32 fxgmac_rss_indir_size(struct net_device *netdev)
{
    return FXGMAC_RSS_MAX_TABLE_SIZE;
}

static void fxgmac_get_reta(struct fxgmac_pdata *pdata, u32 *indir)
{
    int i, reta_size = FXGMAC_RSS_MAX_TABLE_SIZE;
    u16 rss_m;
#ifdef FXGMAC_ONE_CHANNLE
    rss_m = FXGMAC_MAX_DMA_CHANNELS;
#else
    rss_m = FXGMAC_MAX_DMA_CHANNELS - 1; //mask for index of channel, 0-3
#endif

    for (i = 0; i < reta_size; i++)
    	indir[i] = pdata->rss_table[i] & rss_m;
}

static int fxgmac_get_rxfh(struct net_device *netdev, u32 *indir, u8 *key,
			  u8 *hfunc)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);

    /*
    ETH_RSS_HASH_TOP        __ETH_RSS_HASH(TOP)
    ETH_RSS_HASH_XOR        __ETH_RSS_HASH(XOR)
    ETH_RSS_HASH_CRC32      __ETH_RSS_HASH(CRC32)	
    */
    if (hfunc)
    {
    	//*hfunc = ETH_RSS_HASH_XOR;
    	*hfunc = ETH_RSS_HASH_TOP;
    	DPRINTK("fxmac, get_rxfh for hash function\n"); 
    }

    if (indir)
    {
    	fxgmac_get_reta(pdata, indir);
    	DPRINTK("fxmac, get_rxfh for indirection tab\n"); 
    }

    if (key)
    {
    	memcpy(key, pdata->rss_key, fxgmac_get_rxfh_key_size(netdev));
    	DPRINTK("fxmac, get_rxfh  for hash key\n"); 
    }

    return 0;
}

static int fxgmac_set_rxfh(struct net_device *netdev, const u32 *indir,
			  const u8 *key, const u8 hfunc)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    int i;
    u32 reta_entries = fxgmac_rss_indir_size(netdev);
    int max_queues = FXGMAC_MAX_DMA_CHANNELS;

    DPRINTK("fxmac, set_rxfh callin, indir=%lx, key=%lx, func=%02x\n", (unsigned long)indir, (unsigned long)key, hfunc); 

    if (hfunc)
    	return -EINVAL;

    /* Fill out the redirection table */
    if (indir) {
#if FXGMAC_MSIX_CH0RXDIS_EN
    	max_queues = max_queues; // kill warning
    	reta_entries = reta_entries;
    	i = i;
    	DPRINTK("fxmac, set_rxfh, change of indirect talbe is not supported.\n");
    	return -EINVAL;
#else
    	/* double check user input. */
    	for (i = 0; i < reta_entries; i++)
    	    if (indir[i] >= max_queues)
    	        return -EINVAL;

    	for (i = 0; i < reta_entries; i++)
    	    pdata->rss_table[i] = indir[i];

    	fxgmac_write_rss_lookup_table(pdata);
#endif
    }

    /* Fill out the rss hash key */
    if (FXGMAC_RSS_HASH_KEY_LINUX && key) {
    	memcpy(pdata->rss_key, key, fxgmac_get_rxfh_key_size(netdev));
    	fxgmac_write_rss_hash_key(pdata);
    }

    return 0;
}

static int fxgmac_get_rss_hash_opts(struct fxgmac_pdata *pdata,
				   struct ethtool_rxnfc *cmd)
{
    u32 reg_opt;
    cmd->data = 0;

    reg_opt = fxgmac_read_rss_options(pdata);
    DPRINTK ("fxgmac_get_rss_hash_opts, hw=%02x, %02x\n", reg_opt, pdata->rss_options);

    if(reg_opt != pdata->rss_options)
    {
    	DPRINTK ("fxgmac_get_rss_hash_opts, warning, options are not consistent\n");
    }

    /* Report default options for RSS */
    switch (cmd->flow_type) {
    case TCP_V4_FLOW:
    case UDP_V4_FLOW:
    	if(((TCP_V4_FLOW == (cmd->flow_type)) && (FXGMAC_GET_REG_BITS(pdata->rss_options, MAC_RSSCR_TCP4TE_POS, MAC_RSSCR_TCP4TE_LEN))) ||
    		((UDP_V4_FLOW == (cmd->flow_type)) && (FXGMAC_GET_REG_BITS(pdata->rss_options, MAC_RSSCR_UDP4TE_POS, MAC_RSSCR_UDP4TE_LEN))))
    	{
    		cmd->data |= RXH_L4_B_0_1 | RXH_L4_B_2_3;
    	}
    	/* fall through */
    case SCTP_V4_FLOW:
    case AH_ESP_V4_FLOW:
    case AH_V4_FLOW:
    case ESP_V4_FLOW:
    case IPV4_FLOW:
    	if(((TCP_V4_FLOW == (cmd->flow_type)) && (FXGMAC_GET_REG_BITS(pdata->rss_options, MAC_RSSCR_TCP4TE_POS, MAC_RSSCR_TCP4TE_LEN))) ||
    		((UDP_V4_FLOW == (cmd->flow_type)) && (FXGMAC_GET_REG_BITS(pdata->rss_options, MAC_RSSCR_UDP4TE_POS, MAC_RSSCR_UDP4TE_LEN))) ||
    		(FXGMAC_GET_REG_BITS(pdata->rss_options, MAC_RSSCR_IP4TE_POS, MAC_RSSCR_IP4TE_LEN)))
    	{
    		cmd->data |= RXH_IP_SRC | RXH_IP_DST;
    	}
    	break;
    case TCP_V6_FLOW:
    case UDP_V6_FLOW:
    	if(((TCP_V6_FLOW == (cmd->flow_type)) && (FXGMAC_GET_REG_BITS(pdata->rss_options, MAC_RSSCR_TCP6TE_POS, MAC_RSSCR_TCP6TE_LEN))) ||
    		((UDP_V6_FLOW == (cmd->flow_type)) && (FXGMAC_GET_REG_BITS(pdata->rss_options, MAC_RSSCR_UDP6TE_POS, MAC_RSSCR_UDP6TE_LEN))))
    	{
    		cmd->data |= RXH_L4_B_0_1 | RXH_L4_B_2_3;
    	}
    	/* fall through */
    case SCTP_V6_FLOW:
    case AH_ESP_V6_FLOW:
    case AH_V6_FLOW:
    case ESP_V6_FLOW:
    case IPV6_FLOW:
    	if(((TCP_V6_FLOW == (cmd->flow_type)) && (FXGMAC_GET_REG_BITS(pdata->rss_options, MAC_RSSCR_TCP6TE_POS, MAC_RSSCR_TCP6TE_LEN))) ||
    		((UDP_V6_FLOW == (cmd->flow_type)) && (FXGMAC_GET_REG_BITS(pdata->rss_options, MAC_RSSCR_UDP6TE_POS, MAC_RSSCR_UDP6TE_LEN))) ||
    		(FXGMAC_GET_REG_BITS(pdata->rss_options, MAC_RSSCR_IP6TE_POS, MAC_RSSCR_IP6TE_LEN)))
    	{
    		cmd->data |= RXH_IP_SRC | RXH_IP_DST;
    	}
    	break;
    default:
    	return -EINVAL;
    }

    return 0;
}

static int fxgmac_get_rxnfc(struct net_device *dev, struct ethtool_rxnfc *cmd,
			   u32 *rule_locs)
{
    struct fxgmac_pdata *pdata = netdev_priv(dev);
    int ret = -EOPNOTSUPP;

    switch (cmd->cmd) {
    case ETHTOOL_GRXRINGS:
    	cmd->data = pdata->rx_q_count;
    	ret = 0;
    	DPRINTK("fxmac, get_rxnfc for rx ring cnt\n"); 
    	break;
    case ETHTOOL_GRXCLSRLCNT:
    	cmd->rule_cnt = 0;
    	ret = 0;
    	DPRINTK("fxmac, get_rxnfc for classify rule cnt\n"); 
    	break;
    case ETHTOOL_GRXCLSRULE:
    	DPRINTK("fxmac, get_rxnfc for classify rules\n"); 
    	ret = 0;//ixgbe_get_ethtool_fdir_entry(adapter, cmd);
    	break;
    case ETHTOOL_GRXCLSRLALL:
    	cmd->rule_cnt = 0;
    	ret = 0;
    	/*ret = ixgbe_get_ethtool_fdir_all(adapter, cmd,
    					 (u32 *)rule_locs);
    	*/
    	DPRINTK("fxmac, get_rxnfc for classify both cnt and rules\n"); 
    	break;
    case ETHTOOL_GRXFH:
    	ret = fxgmac_get_rss_hash_opts(pdata, cmd);
    	DPRINTK("fxmac, get_rxnfc for hash options\n"); 
    	break;
    default:
    	break;
    }

    return ret;
}

#define UDP_RSS_FLAGS (BIT(MAC_RSSCR_UDP4TE_POS) | \
		       BIT(MAC_RSSCR_UDP6TE_POS))
static int fxgmac_set_rss_hash_opt(struct fxgmac_pdata *pdata,
				  struct ethtool_rxnfc *nfc)
{
    u32 rssopt = 0; //pdata->rss_options;

    DPRINTK("fxgmac_set_rss_hash_opt call in,nfc_data=%llx,cur opt=%x\n", nfc->data, pdata->rss_options);

    /*
    * For RSS, it does not support anything other than hashing
    * to queues on src,dst IPs and L4 ports
    */
    if (nfc->data & ~(RXH_IP_SRC | RXH_IP_DST |
    	  RXH_L4_B_0_1 | RXH_L4_B_2_3))
    return -EINVAL;

    switch (nfc->flow_type) {
        case TCP_V4_FLOW:
        case TCP_V6_FLOW:
            /* default to TCP flow and do nothting */
            if (!(nfc->data & RXH_IP_SRC) ||
                !(nfc->data & RXH_IP_DST) ||
                !(nfc->data & RXH_L4_B_0_1) ||
                !(nfc->data & RXH_L4_B_2_3))
            	return -EINVAL;
            if(TCP_V4_FLOW == (nfc->flow_type))
            {
            	rssopt = FXGMAC_SET_REG_BITS(
            		rssopt,
            		MAC_RSSCR_IP4TE_POS,
            		MAC_RSSCR_IP4TE_LEN, 1);
            	rssopt = FXGMAC_SET_REG_BITS(
            		rssopt,
            		MAC_RSSCR_TCP4TE_POS,
            		MAC_RSSCR_TCP4TE_LEN, 1);
            }

            if(TCP_V6_FLOW == (nfc->flow_type))
            {
            	rssopt = FXGMAC_SET_REG_BITS(
            		rssopt,
            		MAC_RSSCR_IP6TE_POS,
            		MAC_RSSCR_IP6TE_LEN, 1);
            	rssopt = FXGMAC_SET_REG_BITS(
            		rssopt,
            		MAC_RSSCR_TCP6TE_POS,
            		MAC_RSSCR_TCP6TE_LEN, 1);
            }
            break;

        case UDP_V4_FLOW:
            if (!(nfc->data & RXH_IP_SRC) ||
                !(nfc->data & RXH_IP_DST))
            	return -EINVAL;
            rssopt = FXGMAC_SET_REG_BITS(
            	rssopt,
            	MAC_RSSCR_IP4TE_POS,
            	MAC_RSSCR_IP4TE_LEN, 1);			
            switch (nfc->data & (RXH_L4_B_0_1 | RXH_L4_B_2_3)) {
                case 0:
                	break;
                case (RXH_L4_B_0_1 | RXH_L4_B_2_3):
                	rssopt = FXGMAC_SET_REG_BITS(
                		rssopt,
                		MAC_RSSCR_UDP4TE_POS,
                		MAC_RSSCR_UDP4TE_LEN, 1);			
                	break;
                default:
                	return -EINVAL;
                }
            break;
        case UDP_V6_FLOW:
            if (!(nfc->data & RXH_IP_SRC) ||
                !(nfc->data & RXH_IP_DST))
            	return -EINVAL;
            rssopt = FXGMAC_SET_REG_BITS(
            	rssopt,
            	MAC_RSSCR_IP6TE_POS,
            	MAC_RSSCR_IP6TE_LEN, 1);			

            switch (nfc->data & (RXH_L4_B_0_1 | RXH_L4_B_2_3)) {
                case 0:
                	break;
                case (RXH_L4_B_0_1 | RXH_L4_B_2_3):
                	rssopt = FXGMAC_SET_REG_BITS(
                		rssopt,
                		MAC_RSSCR_UDP6TE_POS,
                		MAC_RSSCR_UDP6TE_LEN, 1);			
                	break;
                default:
                	return -EINVAL;
            }
            break;
        case AH_ESP_V4_FLOW:
        case AH_V4_FLOW:
        case ESP_V4_FLOW:
        case SCTP_V4_FLOW:
        case AH_ESP_V6_FLOW:
        case AH_V6_FLOW:
        case ESP_V6_FLOW:
        case SCTP_V6_FLOW:
            if (!(nfc->data & RXH_IP_SRC) ||
                !(nfc->data & RXH_IP_DST) ||
                (nfc->data & RXH_L4_B_0_1) ||
                (nfc->data & RXH_L4_B_2_3))
            	return -EINVAL;
            break;
        default:
            return -EINVAL;
    }

    /* if options are changed, then update to hw */
    if (rssopt != pdata->rss_options) {
    if ((rssopt & UDP_RSS_FLAGS) &&
        !(pdata->rss_options & UDP_RSS_FLAGS))
    	DPRINTK("enabling UDP RSS: fragmented packets"
    	       " may arrive out of order to the stack above\n");

    DPRINTK("rss option changed from %x to %x\n", pdata->rss_options, rssopt);
    pdata->rss_options = rssopt;
#if 1
    fxgmac_write_rss_options(pdata);
#else
    /* Perform hash on these packet types */
    mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4
          | IXGBE_MRQC_RSS_FIELD_IPV4_TCP
          | IXGBE_MRQC_RSS_FIELD_IPV6
          | IXGBE_MRQC_RSS_FIELD_IPV6_TCP;

    mrqc &= ~(IXGBE_MRQC_RSS_FIELD_IPV4_UDP |
    	  IXGBE_MRQC_RSS_FIELD_IPV6_UDP);

    if (flags2 & IXGBE_FLAG2_RSS_FIELD_IPV4_UDP)
    	mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4_UDP;

    if (flags2 & IXGBE_FLAG2_RSS_FIELD_IPV6_UDP)
    	mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_UDP;

    if ((hw->mac.type >= ixgbe_mac_X550) &&
        (adapter->flags & IXGBE_FLAG_SRIOV_ENABLED))
    	IXGBE_WRITE_REG(hw, IXGBE_PFVFMRQC(pf_pool), mrqc);
    else
    	IXGBE_WRITE_REG(hw, IXGBE_MRQC, mrqc);
#endif		
    }

    return 0;
}

static int fxgmac_set_rxnfc(struct net_device *dev, struct ethtool_rxnfc *cmd)
{
    struct fxgmac_pdata *pdata = netdev_priv(dev);

    int ret = -EOPNOTSUPP;

    switch (cmd->cmd) {
    case ETHTOOL_SRXCLSRLINS:
    	//no support. rx classifier rule insert
    	//ret = ixgbe_add_ethtool_fdir_entry(adapter, cmd);
    	DPRINTK("set_rxnfc for rx cls rule insert-n\\a\n");
    	break;
    case ETHTOOL_SRXCLSRLDEL:
    	//no support. rx classifier rule delete
    	//ret = ixgbe_del_ethtool_fdir_entry(adapter, cmd);
    	DPRINTK("set_rxnfc for rx cls rule del-n\\a\n");
    	break;
    case ETHTOOL_SRXFH:
    	DPRINTK("set_rxnfc for rx rss option\n");
    	ret = fxgmac_set_rss_hash_opt(pdata, cmd);
    	break;
    default:
    	break;
    }

    return ret;
}

#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0) )
static void fxgmac_get_ringparam(struct net_device *netdev,
             struct ethtool_ringparam *ring,
             struct kernel_ethtool_ringparam *kernel_ring,
             struct netlink_ext_ack *exact)

#else
static void fxgmac_get_ringparam(struct net_device *netdev,
						struct ethtool_ringparam *ring)
#endif
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);

    DPRINTK("fxmac, get_ringparam callin\n"); 

    ring->rx_max_pending = pdata->rx_ring_count;
    ring->tx_max_pending = pdata->tx_ring_count;
    ring->rx_mini_max_pending = 0;
    ring->rx_jumbo_max_pending = 0;
    ring->rx_pending = pdata->rx_ring_count;
    ring->tx_pending = pdata->tx_ring_count;
    ring->rx_mini_pending = 0;
    ring->rx_jumbo_pending = 0;
}

#endif //FXGMAC_RSS_FEATURE_ENABLED

#if FXGMAC_WOL_FEATURE_ENABLED
static void fxgmac_get_wol(struct net_device *netdev,
			  struct ethtool_wolinfo *wol)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);

    /* for further feature implementation
    wol->supported = WAKE_PHY | WAKE_UCAST | WAKE_MCAST |
    		 WAKE_BCAST | WAKE_MAGIC;
    */

    wol->supported = WAKE_UCAST | WAKE_MCAST | WAKE_BCAST | WAKE_MAGIC | WAKE_ARP;
#if FXGMAC_WOL_UPON_EPHY_LINK
    wol->supported |= WAKE_PHY;
#endif

    wol->wolopts = 0;
    if (!(pdata->hw_feat.rwk) || !device_can_wakeup(/*pci_dev_to_dev*/(pdata->dev))) {
    	DPRINTK("fxgmac get_wol, pci does not support wakeup\n"); 
    	return;
    }
#if 0
    wol->wolopts |= WAKE_UCAST;
    wol->wolopts |= WAKE_MCAST;
    wol->wolopts |= WAKE_BCAST;
    wol->wolopts |= WAKE_MAGIC;
#else
    wol->wolopts = pdata->wol;
#endif
    DPRINTK("fxmac, get_wol, 0x%x, 0x%x\n", wol->wolopts, pdata->wol); 
}

static int fxgmac_set_wol(struct net_device *netdev, struct ethtool_wolinfo *wol)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);

    //currently, we do not support these options
#if FXGMAC_WOL_UPON_EPHY_LINK
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,0))
    if (wol->wolopts & (WAKE_MAGICSECURE | WAKE_FILTER)) {
#else
    if (wol->wolopts & WAKE_MAGICSECURE) {
#endif
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,0))
    if (wol->wolopts & (WAKE_PHY | WAKE_MAGICSECURE | WAKE_FILTER)) {
#else
    if (wol->wolopts & (WAKE_PHY | WAKE_MAGICSECURE)) {
#endif
#endif
    	DPRINTK("fxmac, set_wol, not supported wol options, 0x%x\n", wol->wolopts); 
    	return -EOPNOTSUPP;
    }

    if (!(pdata->hw_feat.rwk)) {
    	DPRINTK("fxmac, set_wol, hw wol feature is n/a\n"); 
    	return (wol->wolopts ? -EOPNOTSUPP : 0);
    }

#if 1 /* normally, pls set this to 1.  */
    pdata->wol = 0;
    if (wol->wolopts & WAKE_UCAST)
            pdata->wol |= WAKE_UCAST;

    if (wol->wolopts & WAKE_MCAST)
            pdata->wol |= WAKE_MCAST;

    if (wol->wolopts & WAKE_BCAST)
            pdata->wol |= WAKE_BCAST;

    if (wol->wolopts & WAKE_MAGIC)
            pdata->wol |= WAKE_MAGIC;

    if (wol->wolopts & WAKE_PHY)
            pdata->wol |= WAKE_PHY;

    if (wol->wolopts & WAKE_ARP)
            pdata->wol |= WAKE_ARP;

    fxgmac_set_pattern_data(pdata);

    fxgmac_config_wol(pdata, (!!(pdata->wol)));
#else

    /* for test only... */
    if (wol->wolopts & WAKE_UCAST) {
    	DPRINTK("fxmac, set_wol, ucast\n"); 
    	//fxgmac_net_powerdown(pdata, pdata->wol);
    	//fxgmac_update_aoe_ipv4addr(pdata, (u8 *)NULL);
    	//fxgmac_get_netdev_ip6addr(pdata, NULL, NULL);
    	fxgmac_update_ns_offload_ipv6addr(pdata, FXGMAC_NS_IFA_LOCAL_LINK);
    }

    if (wol->wolopts & WAKE_MAGIC) {
    	DPRINTK("fxmac, set_wol, magic\n");
    	//fxgmac_net_powerup(pdata);
    	//fxgmac_update_aoe_ipv4addr(pdata, (u8 *)"192.168.3.33");
    	fxgmac_update_ns_offload_ipv6addr(pdata, FXGMAC_NS_IFA_GLOBAL_UNICAST);
    }
    /*
    if (wol->wolopts & WAKE_BCAST) {
    	extern int fxgmac_pci_config_modify(struct pci_dev *pdev);
    	DPRINTK("fxmac, set_wol, broadcast\n");
    	fxgmac_pci_config_modify(struct pci_dev *pdev);
    }
    */	
#endif
    DPRINTK("fxmac, set_wol, opt=0x%x, 0x%x\n", wol->wolopts, pdata->wol); 

    return 0;
}
#endif /*FXGMAC_WOL_FEATURE_ENABLED*/

static int fxgmac_get_regs_len(struct net_device __always_unused *netdev)
{
    return FXGMAC_EPHY_REGS_LEN * sizeof(u32);
}

static void fxgmac_get_regs(struct net_device *netdev, struct ethtool_regs *regs,
			   void *p)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;

    u32 *regs_buff = p;
    u8 i;

    memset(p, 0, FXGMAC_EPHY_REGS_LEN * sizeof(u32));
    for (i = REG_MII_BMCR; i < FXGMAC_EPHY_REGS_LEN; i++) {
    	hw_ops->read_ephy_reg(pdata, i, (unsigned int *)&regs_buff[i]);
    }
    regs->version = regs_buff[REG_MII_PHYSID1] << 16 | regs_buff[REG_MII_PHYSID2];

    //fxgmac_ephy_soft_reset(pdata);
}

#if FXGMAC_PAUSE_FEATURE_ENABLED
static int fxgmac_get_link_ksettings(struct net_device *netdev,
				    struct ethtool_link_ksettings *cmd)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;
    u32 duplex, regval, link_status;
    u32 adv = 0xFFFFFFFF;

    regval = fxgmac_ephy_autoneg_ability_get(pdata, &adv);
    if (regval)
        return -ETIMEDOUT;

    ethtool_link_ksettings_zero_link_mode(cmd, supported);
    ethtool_link_ksettings_zero_link_mode(cmd, advertising);

    /* set the supported link speeds */
    ethtool_link_ksettings_add_link_mode(cmd, supported, 1000baseT_Full);
    ethtool_link_ksettings_add_link_mode(cmd, supported, 100baseT_Full);
    ethtool_link_ksettings_add_link_mode(cmd, supported, 100baseT_Half);
    ethtool_link_ksettings_add_link_mode(cmd, supported, 10baseT_Full);
    ethtool_link_ksettings_add_link_mode(cmd, supported, 10baseT_Half);

    /* Indicate pause support */
    ethtool_link_ksettings_add_link_mode(cmd, supported, Pause);
    ethtool_link_ksettings_add_link_mode(cmd, supported, Asym_Pause);
    ethtool_link_ksettings_add_link_mode(cmd, advertising, Pause);
    ethtool_link_ksettings_add_link_mode(cmd, advertising, Asym_Pause);

    ethtool_link_ksettings_add_link_mode(cmd, supported, MII);
    cmd->base.port = PORT_MII;

    ethtool_link_ksettings_add_link_mode(cmd, supported, Autoneg);
    hw_ops->read_ephy_reg(pdata, REG_MII_BMCR, &regval);
    regval = FXGMAC_GET_REG_BITS(regval, PHY_CR_AUTOENG_POS, PHY_CR_AUTOENG_LEN);
    if (regval) {
        ethtool_link_ksettings_add_link_mode(cmd, advertising, Autoneg);
        if (adv & FXGMAC_ADVERTISE_10HALF)
            ethtool_link_ksettings_add_link_mode(cmd, advertising, 10baseT_Half);
        if (adv & FXGMAC_ADVERTISE_10FULL)
            ethtool_link_ksettings_add_link_mode(cmd, advertising, 10baseT_Full);
        if (adv & FXGMAC_ADVERTISE_100HALF)
            ethtool_link_ksettings_add_link_mode(cmd, advertising, 100baseT_Half);
        if (adv & FXGMAC_ADVERTISE_100FULL)
            ethtool_link_ksettings_add_link_mode(cmd, advertising, 100baseT_Full);
        if (adv & FXGMAC_ADVERTISE_1000FULL)
            ethtool_link_ksettings_add_link_mode(cmd, advertising, 1000baseT_Full);
    }
    else {
        clear_bit(ETHTOOL_LINK_MODE_Autoneg_BIT, cmd->link_modes.advertising);
        switch (pdata->phy_speed) {
            case SPEED_1000M:
                if (pdata->phy_duplex)
                    ethtool_link_ksettings_add_link_mode(cmd, advertising, 1000baseT_Full);
                else
                    ethtool_link_ksettings_add_link_mode(cmd, advertising, 1000baseT_Half);
                break;
            case SPEED_100M:
                if (pdata->phy_duplex)
                    ethtool_link_ksettings_add_link_mode(cmd, advertising, 100baseT_Full);
                else
                    ethtool_link_ksettings_add_link_mode(cmd, advertising, 100baseT_Half);
                break;
            case SPEED_10M:
                if (pdata->phy_duplex)
                    ethtool_link_ksettings_add_link_mode(cmd, advertising, 10baseT_Full);
                else
                    ethtool_link_ksettings_add_link_mode(cmd, advertising, 10baseT_Half);
                break;
            default:
                break;
        }
    }
    cmd->base.autoneg = regval;

    hw_ops->read_ephy_reg(pdata, REG_MII_SPEC_STATUS, &regval);
    link_status = regval & (BIT(FUXI_EPHY_LINK_STATUS_BIT));
    if (link_status) {
        duplex = FXGMAC_GET_REG_BITS(regval, PHY_MII_SPEC_DUPLEX_POS, PHY_MII_SPEC_DUPLEX_LEN);
        cmd->base.duplex = duplex;
        cmd->base.speed = pdata->phy_speed;
    }else {
        cmd->base.duplex = DUPLEX_UNKNOWN;
        cmd->base.speed = SPEED_UNKNOWN;
    }

    return 0;
}

static int fxgmac_set_link_ksettings(struct net_device *netdev,
				    const struct ethtool_link_ksettings *cmd)
{
    u32 advertising, support, adv;
    int ret;
    struct fxphy_ag_adv;
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;

    pdata->phy_autoeng = cmd->base.autoneg;

    ethtool_convert_link_mode_to_legacy_u32(&advertising, cmd->link_modes.advertising);
    ethtool_convert_link_mode_to_legacy_u32(&support, cmd->link_modes.supported);
    advertising &= support;

    if (pdata->phy_autoeng){
        ret = hw_ops->read_ephy_reg(pdata, REG_MII_ADVERTISE, &adv);
        if (ret < 0)
            return -ETIMEDOUT;
        adv &= ~REG_BIT_ADVERTISE_100_10_CAP;
        adv |= ethtool_adv_to_mii_adv_t(advertising);
        ret = hw_ops->write_ephy_reg(pdata, REG_MII_ADVERTISE, adv);
        if (ret < 0)
            return -ETIMEDOUT;
        ret = hw_ops->read_ephy_reg(pdata, REG_MII_CTRL1000, &adv);
        if (ret < 0)
            return -ETIMEDOUT;
        adv &= ~REG_BIT_ADVERTISE_1000_CAP;
        adv |= ethtool_adv_to_mii_ctrl1000_t(advertising);
        ret = hw_ops->write_ephy_reg(pdata, REG_MII_CTRL1000, adv);
        if (ret < 0)
            return -ETIMEDOUT;

        ret = hw_ops->read_ephy_reg(pdata, REG_MII_BMCR, &adv);
        if (ret < 0)
            return -ETIMEDOUT;
        adv = FXGMAC_SET_REG_BITS(adv, PHY_CR_AUTOENG_POS, PHY_CR_AUTOENG_LEN, 1);
        ret = hw_ops->write_ephy_reg(pdata, REG_MII_BMCR, adv);
        if (ret < 0)
            return -ETIMEDOUT;
    } else {
        pdata->phy_duplex = cmd->base.duplex;
        pdata->phy_speed = cmd->base.speed;
        fxgmac_phy_force_speed(pdata, pdata->phy_speed);
        fxgmac_phy_force_duplex(pdata, pdata->phy_duplex);
        fxgmac_phy_force_autoneg(pdata, pdata->phy_autoeng);
        fxgmac_config_mac_speed(pdata);
    }

    ret = fxgmac_ephy_soft_reset(pdata);
    if (ret) {
        printk("%s: ephy soft reset timeout.\n", __func__);
        return -ETIMEDOUT;
    }

    return 0;
}

static void fxgmac_get_pauseparam(struct net_device *netdev,
				 struct ethtool_pauseparam *pause)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);

    pause->autoneg = 1;
    pause->rx_pause = pdata->rx_pause;
    pause->tx_pause = pdata->tx_pause;

    DPRINTK("fxmac get_pauseparam done, rx=%d, tx=%d\n", pdata->rx_pause, pdata->tx_pause); 
}

static int fxgmac_set_pauseparam(struct net_device *netdev,
				struct ethtool_pauseparam *pause)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    struct fxgmac_hw_ops *hw_ops = &pdata->hw_ops;
    unsigned int pre_rx_pause = pdata->rx_pause;
    unsigned int pre_tx_pause = pdata->tx_pause;

    //if (pause->autoneg == AUTONEG_ENABLE)
    //DPRINTK("fxgmac set pause parameter, autoneg=%d\n", pause->autoneg);

    pdata->rx_pause = pause->rx_pause;
    pdata->tx_pause = pause->tx_pause;

    if(pre_rx_pause != pdata->rx_pause) {
    	hw_ops->config_rx_flow_control(pdata);
    	DPRINTK("fxgmac set pause parameter, rx from %d to %d\n", pre_rx_pause, pdata->rx_pause);
    }
    if(pre_tx_pause != pdata->tx_pause) {
    	hw_ops->config_tx_flow_control(pdata);
    	DPRINTK("fxgmac set pause parameter, tx from %d to %d\n", pre_tx_pause, pdata->tx_pause);
    }

    /*		
    if (netif_running(netdev))
    	fxgmac_restart_dev(pdata);
    */
    DPRINTK("fxgmac set pause parameter, autoneg=%d, rx=%d, tx=%d\n", pause->autoneg, pause->rx_pause, pause->tx_pause);

    return 0;
}
#endif /*FXGMAC_PAUSE_FEATURE_ENABLED*/


/* yzhang added for debug sake. descriptors status checking
 * 2021.03.29
 */
#define FXGMAC_ETH_GSTRING_LEN 32 
static char fxgmac_gstrings_test[1024*7][FXGMAC_ETH_GSTRING_LEN]= {
};

static char fxgmac_gstrings_header[7][10]= {
    "Channel:", "curDesc:",
    "baseMem:", "desc0  :",
    "desc1  :", "desc2  :",
    "desc3  :"
};

#define FXGMAC_TEST_LEN	(sizeof(fxgmac_gstrings_test) / FXGMAC_ETH_GSTRING_LEN)
u16 fxgmac_ethtool_test_flag = 0xff; //invalid value, yzhang

#define DBG_ETHTOOL_CHECK_NUM_OF_DESC 5
static u16 fxgmac_ethtool_test_len = DBG_ETHTOOL_CHECK_NUM_OF_DESC; //default value, yzhang

static void fxgmac_ethtool_get_strings(struct net_device *netdev,
				       u32 stringset, u8 *data)
{
    int i;

    switch (stringset) {
    case ETH_SS_TEST:
    	//netdev_warn(netdev, "Xlgmac diag, get string here, ...\n");
    	memcpy(data, *fxgmac_gstrings_test,
    		   /*FXGMAC_TEST_LEN*/fxgmac_ethtool_test_len * 7 * FXGMAC_ETH_GSTRING_LEN);
    	break;
    	
    case ETH_SS_STATS:
    	for (i = 0; i < FXGMAC_STATS_COUNT; i++) {
    		memcpy(data, fxgmac_gstring_stats[i].stat_string,
    		       ETH_GSTRING_LEN);
    		data += ETH_GSTRING_LEN;
    	}
    	break;
    default:
    	WARN_ON(1);
    	break;
    }
}

static int fxgmac_ethtool_get_sset_count(struct net_device *netdev,
					 int stringset)
{
    int ret;

    switch (stringset) {
    case ETH_SS_TEST:
    	ret = /*FXGMAC_TEST_LEN*/fxgmac_ethtool_test_len * 7;
    	//DPRINTK("ethtool sset cnt ret=%d\n",ret);
    	break;
    case ETH_SS_STATS:
    	ret = FXGMAC_STATS_COUNT;
    	break;

    default:
    	ret = -EOPNOTSUPP;
    }

    return ret;
}

static void fxgmac_ethtool_get_ethtool_stats(struct net_device *netdev,
					     struct ethtool_stats *stats,
					     u64 *data)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    u8 *stat;
    int i;

#if FXGMAC_PM_FEATURE_ENABLED
    /* 20210709 for net power down */
    if(!test_bit(FXGMAC_POWER_STATE_DOWN, &pdata->powerstate))
#endif
    {
    	//DPRINTK("fxgmac_ethtool_get_ethtool_stats, here\n");
    	pdata->hw_ops.read_mmc_stats(pdata);
    }

    for (i = 0; i < FXGMAC_STATS_COUNT; i++) {
    	stat = (u8 *)pdata + fxgmac_gstring_stats[i].stat_offset;
    	*data++ = *(u64 *)stat;
    }
}

static inline bool fxgmac_removed(void __iomem *addr)
{
    return unlikely(!addr);
}
#define FXGMAC_REMOVED(a) fxgmac_removed(a)
extern u16 fxgmac_cur_tx_skb_q_mapping;
extern u16 fxgmac_cur_rx_ch_polling;
extern u16 fxgmac_cur_tx_desc_start_idx;
static bool fxgmac_dbg_ethtool_tx_desc_access = 0;
static bool fxgmac_dbg_ethtool_rx_desc_access = 0;

bool fxgmac_diag_tx_test_is_ongoing ( void )
{
    return fxgmac_dbg_ethtool_tx_desc_access;
}

bool fxgmac_diag_rx_test_is_ongoing ( void )
{
    return fxgmac_dbg_ethtool_rx_desc_access;
}

static void fxgmac_diag_test(struct net_device *netdev,
			    struct ethtool_test *eth_test, u64 *data)
{
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    //bool if_running = netif_running(netdev);
    struct fxgmac_desc_data *desc_data;
    struct fxgmac_dma_desc *dma_desc;
    struct fxgmac_channel *channel;
    struct fxgmac_ring *ring;
    unsigned int ring_idx;
    unsigned int i,j, cur_desc;
    u32 temp_desc;

    if (FXGMAC_REMOVED(pdata->mac_addr)) {
    	netdev_warn(netdev, "Xlgmac diag, adapter removed - test blocked\n");
    	for(i = 0; i < fxgmac_ethtool_test_len; i++){
                data[i*7+0] = 0;
                data[i*7+1] = 0;
                data[i*7+2] = 0;
                data[i*7+3] = 0;
                data[i*7+4] = 0;
                data[i*7+5] = 0;
                data[i*7+6] = 0;
            }

    	eth_test->flags |= ETH_TEST_FL_FAILED;
    	return;
    }

    //netdev_info(netdev, "Xlgmac diag test - flag=%08x, %s here\n", eth_test->flags, ((eth_test->flags == ETH_TEST_FL_OFFLINE)?"TX":"RX"));
    fxgmac_ethtool_test_flag = eth_test->flags;

    //set_bit(__IXGBE_TESTING, &adapter->state);
    if (eth_test->flags == ETH_TEST_FL_OFFLINE/*1, tx here*/) {
    	fxgmac_dbg_ethtool_tx_desc_access = 1; //yzhang prevent access to desc both sides

    	if(0xff != fxgmac_cur_tx_skb_q_mapping){
    		channel = pdata->channel_head + fxgmac_cur_tx_skb_q_mapping;
    		ring = channel->tx_ring;
    		ring_idx = fxgmac_cur_tx_skb_q_mapping;
    		cur_desc = (ring->cur & (ring->dma_desc_count-1));
    		if(fxgmac_cur_tx_desc_start_idx <= cur_desc){
    			fxgmac_ethtool_test_len = cur_desc - fxgmac_cur_tx_desc_start_idx;
    		}
    		else {
    			fxgmac_ethtool_test_len = (1024 - fxgmac_cur_tx_desc_start_idx) + cur_desc;
    		}
    	}else{
    		netdev_warn(netdev, "Xlgmac diag, tx is not started yet\n");
    		for(i = 0; i < fxgmac_ethtool_test_len; i++){
                	data[i*7+0] = 0;
                	data[i*7+1] = 0;
                	data[i*7+2] = 0;
                	data[i*7+3] = 0;
                	data[i*7+4] = 0;
                	data[i*7+5] = 0;
                	data[i*7+6] = 0;
    	        }

    		eth_test->flags |= ETH_TEST_FL_FAILED;
    		fxgmac_dbg_ethtool_tx_desc_access = 0; //yzhang prevent access to desc both sides

    		return;
    	}

    	//DPRINTK("ethtool dbg:tx ring desc,num=%d,cur=%d\n", fxgmac_ethtool_test_len, cur_desc);
    	if(cur_desc >= DBG_ETHTOOL_CHECK_NUM_OF_DESC) i = cur_desc - DBG_ETHTOOL_CHECK_NUM_OF_DESC ;
    	else i = 0;

    	for(j=0/*, i = fxgmac_cur_tx_desc_start_idx*/; i < fxgmac_ethtool_test_len; i++){ 
    		desc_data = FXGMAC_GET_DESC_DATA(ring, i);
    		dma_desc = desc_data->dma_desc;

    		temp_desc = dma_desc->desc0 | dma_desc->desc1 | dma_desc->desc2 | dma_desc->desc3;
    		if(0 == temp_desc){
    			j++;
    			continue;
    		}
    		//DPRINTK("ethtool dbg:desc val of %d=%u\n", i, temp_desc);
    		sprintf(&fxgmac_gstrings_test[j*7+0][0], "%s%s, %4u ",fxgmac_gstrings_header[0], ((eth_test->flags == ETH_TEST_FL_OFFLINE)?"TX":"RX"),ring_idx);
    		sprintf(&fxgmac_gstrings_test[j*7+1][0], "%s%8u ", fxgmac_gstrings_header[1], i & (ring->dma_desc_count-1));
    		sprintf(&fxgmac_gstrings_test[j*7+2][0], "%s%08x ",fxgmac_gstrings_header[2], (u32)desc_data->dma_desc_addr);
    		sprintf(&fxgmac_gstrings_test[j*7+3][0], "%s%08x ",fxgmac_gstrings_header[3], (u32)dma_desc->desc0);
    		sprintf(&fxgmac_gstrings_test[j*7+4][0], "%s%08x ",fxgmac_gstrings_header[4], (u32)dma_desc->desc1);
    		sprintf(&fxgmac_gstrings_test[j*7+5][0], "%s%08x ",fxgmac_gstrings_header[5], (u32)dma_desc->desc2);
    		sprintf(&fxgmac_gstrings_test[j*7+6][0], "%s%08x ",fxgmac_gstrings_header[6], (u32)dma_desc->desc3);
    		//DPRINTK("ethtool dbg:tx ring desc %d, %s\n", i, &fxgmac_gstrings_test[i][0]);
    		j++;
    	}
    	fxgmac_dbg_ethtool_tx_desc_access = 0; //yzhang prevent access to desc both sides

    	fxgmac_ethtool_test_len = j;
    	if(0 == fxgmac_ethtool_test_len) fxgmac_ethtool_test_len = 1;
    	

    } else {
                if(0xff != fxgmac_cur_rx_ch_polling){
                        channel = pdata->channel_head + fxgmac_cur_rx_ch_polling;
                        ring = channel->rx_ring;
                        ring_idx = fxgmac_cur_rx_ch_polling;
                }else{
                        netdev_warn(netdev, "Xlgmac diag, rx is not started yet\n");
                        data[0] = 2;
                        data[1] = 3;
                        data[2] = 4;
                        data[3] = 5;
                        data[4] = 6;
                        data[5] = 0;
                        data[6] = 1;
                        eth_test->flags |= ETH_TEST_FL_FAILED;
                        return;
                }


    }
    for(i = 0; i < fxgmac_ethtool_test_len; i++){
        	data[i*7+0] = 0;
        	data[i*7+1] = 0;
        	data[i*7+2] = 0;
        	data[i*7+3] = 0;
        	data[i*7+4] = 0;
        	data[i*7+5] = 0;
        	data[i*7+6] = 0;
    }
    //skip_ol_tests:
    //msleep_interruptible(4 * 1000);
}


void fxgmac_diag_get_rx_info(struct fxgmac_channel *channel)
{
    struct fxgmac_desc_data *desc_data;
    struct fxgmac_dma_desc *dma_desc;
    struct fxgmac_ring *ring;
    unsigned int ring_idx;

    ring = channel->rx_ring;
    ring_idx = fxgmac_cur_rx_ch_polling;

    desc_data = FXGMAC_GET_DESC_DATA(ring, ring->cur);
    dma_desc = desc_data->dma_desc;
    sprintf(&fxgmac_gstrings_test[0][8], "%s, %u", "RX",ring_idx);
    sprintf(&fxgmac_gstrings_test[1][8], "%u", ring->cur & (ring->dma_desc_count-1));
    sprintf(&fxgmac_gstrings_test[2][8], "%08x", (u32)desc_data->dma_desc_addr);
    sprintf(&fxgmac_gstrings_test[3][8], "%08x", (u32)dma_desc->desc0);
    sprintf(&fxgmac_gstrings_test[4][8], "%08x", (u32)dma_desc->desc1);
    sprintf(&fxgmac_gstrings_test[5][8], "%08x", (u32)dma_desc->desc2);
    sprintf(&fxgmac_gstrings_test[6][8], "%08x", (u32)dma_desc->desc3);
    //DPRINTK("dev_read call in,diag rx info, ring=%u,desc0=%#x 1=%#x 2=%#x 3=%#x\n",ring->cur,dma_desc->desc0, dma_desc->desc1, dma_desc->desc2, dma_desc->desc3);

}


static const struct ethtool_ops fxgmac_ethtool_ops = {
    .get_drvinfo = fxgmac_ethtool_get_drvinfo,
    .get_link = ethtool_op_get_link,
    .get_msglevel = fxgmac_ethtool_get_msglevel,
    .set_msglevel = fxgmac_ethtool_set_msglevel,
    .self_test		= fxgmac_diag_test,
    .get_channels = fxgmac_ethtool_get_channels,
    .get_coalesce = fxgmac_ethtool_get_coalesce,
    .set_coalesce = fxgmac_ethtool_set_coalesce,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,10,0))
#ifdef ETHTOOL_COALESCE_USECS
    .supported_coalesce_params = ETHTOOL_COALESCE_USECS,
#endif
#endif
    .get_strings = fxgmac_ethtool_get_strings,
    .get_sset_count = fxgmac_ethtool_get_sset_count,
    .get_ethtool_stats = fxgmac_ethtool_get_ethtool_stats,
    .get_regs_len	= fxgmac_get_regs_len,
    .get_regs		= fxgmac_get_regs,
#if (FXGMAC_RSS_FEATURE_ENABLED)
    .get_rxnfc		= fxgmac_get_rxnfc,
    .set_rxnfc		= fxgmac_set_rxnfc,
    .get_ringparam	= fxgmac_get_ringparam,
    .get_rxfh_indir_size	= fxgmac_rss_indir_size,
    .get_rxfh_key_size	= fxgmac_get_rxfh_key_size,
    .get_rxfh		= fxgmac_get_rxfh,
    .set_rxfh		= fxgmac_set_rxfh,
#endif
#if (FXGMAC_WOL_FEATURE_ENABLED)
    .get_wol		= fxgmac_get_wol,
    .set_wol		= fxgmac_set_wol,
#endif
#if (FXGMAC_PAUSE_FEATURE_ENABLED)
#ifdef ETHTOOL_GLINKSETTINGS
    .get_link_ksettings = fxgmac_get_link_ksettings,
    .set_link_ksettings = fxgmac_set_link_ksettings,
#endif /* ETHTOOL_GLINKSETTINGS */
    .get_pauseparam = fxgmac_get_pauseparam,
    .set_pauseparam = fxgmac_set_pauseparam,
#endif
};

const struct ethtool_ops *fxgmac_get_ethtool_ops(void)
{
    return &fxgmac_ethtool_ops;
}
