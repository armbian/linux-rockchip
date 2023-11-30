/*++

Copyright (c) 2021 Motor-comm Corporation. 
Confidential and Proprietary. All rights reserved.

This is Motor-comm Corporation NIC driver relevant files. Please don't copy, modify,
distribute without commercial permission.

--*/



#include "fuxi-gmac.h"
#ifdef HAVE_FXGMAC_DEBUG_FS
#include <linux/debugfs.h>
#endif
#include <linux/module.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> 
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fcntl.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/netdevice.h> 
#include <linux/etherdevice.h>
#include <linux/netdevice.h> 
#include <linux/etherdevice.h>
#include <linux/ip.h> 
#include <linux/udp.h>

#define TEST_MAC_HEAD               14
#define TEST_TCP_HEAD_LEN_OFFSET    12
#define TEST_TCP_OFFLOAD_LEN_OFFSET 48
#define TEST_TCP_FIX_HEAD_LEN       24
#define TEST_TCP_MSS_OFFSET         56

#define DF_MAX_NIC_NUM      16

#ifdef HAVE_FXGMAC_DEBUG_FS

//char fxgmac_driver_name[] = "fuxi";
static char fxgmac_dbg_netdev_ops_buf[256] = "";

/**
 * fxgmac_dbg_netdev_ops_read - read for netdev_ops datum
 * @filp: the opened file
 * @buffer: where to write the data for the user to read
 * @count: the size of the user's buffer
 * @ppos: file position offset
 **/
static ssize_t fxgmac_dbg_netdev_ops_read(struct file *filp,
					 char __user *buffer,
					 size_t count, loff_t *ppos)
{
    struct fxgmac_pdata *pdata = filp->private_data;
    char *buf;
    int len;

    /* don't allow partial reads */
    if (*ppos != 0)
    	return 0;

    buf = kasprintf(GFP_KERNEL, "%s: %s\n",
    		pdata->netdev->name,
    		fxgmac_dbg_netdev_ops_buf);
    if (!buf)
    	return -ENOMEM;

    if (count < strlen(buf)) {
    	kfree(buf);
    	return -ENOSPC;
    }

    len = simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));

    kfree(buf);
    return len;
}

/**
 * fxgmac_dbg_netdev_ops_write - write into netdev_ops datum
 * @filp: the opened file
 * @buffer: where to find the user's data
 * @count: the length of the user's data
 * @ppos: file position offset
 **/
static ssize_t fxgmac_dbg_netdev_ops_write(struct file *filp,
					  const char __user *buffer,
					  size_t count, loff_t *ppos)
{
    //struct fxgmac_pdata *pdata = filp->private_data;
    int len;

    /* don't allow partial writes */
    if (*ppos != 0)
    	return 0;
    if (count >= sizeof(fxgmac_dbg_netdev_ops_buf))
    	return -ENOSPC;

    len = simple_write_to_buffer(fxgmac_dbg_netdev_ops_buf,
    			     sizeof(fxgmac_dbg_netdev_ops_buf)-1,
    			     ppos,
    			     buffer,
    			     count);
    if (len < 0)
    	return len;

    fxgmac_dbg_netdev_ops_buf[len] = '\0';

    if (strncmp(fxgmac_dbg_netdev_ops_buf, "tx_timeout", 10) == 0) {
    	DPRINTK("tx_timeout called\n");
    } else {
    	FXGMAC_PR("Unknown command: %s\n", fxgmac_dbg_netdev_ops_buf);
    	FXGMAC_PR("Available commands:\n");
    	FXGMAC_PR("    tx_timeout\n");
    }
    return count;
}
#endif

static void fxgmac_dbg_tx_pkt(struct fxgmac_pdata *pdata, u8 *pcmd_data)
{
    unsigned int pktLen = 0;
    struct sk_buff *skb;
    pfxgmac_test_packet pPkt;
    u8 * pTx_data = NULL;
    u8 * pSkb_data = NULL;
    u32 offload_len = 0;
    u8 ipHeadLen, tcpHeadLen, headTotalLen;
    static u32 lastGsoSize = 806;//initial default value
    //int i = 0;

    /* get fxgmac_test_packet */
    pPkt = (pfxgmac_test_packet)(pcmd_data + sizeof(struct ext_ioctl_data));
    pktLen = pPkt->length;
    
    /* get pkt data */
    pTx_data = (u8 *)pPkt + sizeof(fxgmac_test_packet);

#if 0 //for debug
    DPRINTK("Send packet len is %d, data:\n", pktLen);
#if 1
    for(i = 0; i < 70 ; i++)
    {
        DPRINTK("%2x ", pTx_data[i]);
    }
#endif
#endif
 
    /* alloc sk_buff */
    skb = alloc_skb(pktLen, GFP_ATOMIC);
    if (!skb){
        DPRINTK("alloc skb fail\n");
        return;
    }

    /* copy data to skb */
    pSkb_data = skb_put(skb, pktLen);
    memset(pSkb_data, 0, pktLen);
    memcpy(pSkb_data, pTx_data, pktLen);
   
    /* set skb parameters */
    skb->dev = pdata->netdev;
    skb->pkt_type = PACKET_OUTGOING;
    skb->protocol = ntohs(ETH_P_IP);
    skb->no_fcs = 1;
    skb->ip_summed = CHECKSUM_PARTIAL; 
    if(skb->len > 1514){
        /* TSO packet */
        /* set tso test flag */
        pdata->fxgmac_test_tso_flag = true;

        /* get protocol head length */
        ipHeadLen = (pSkb_data[TEST_MAC_HEAD] & 0xF) * 4;
        tcpHeadLen = (pSkb_data[TEST_MAC_HEAD + ipHeadLen + TEST_TCP_HEAD_LEN_OFFSET] >> 4 & 0xF) * 4;
        headTotalLen = TEST_MAC_HEAD + ipHeadLen + tcpHeadLen;
        offload_len = (pSkb_data[TEST_TCP_OFFLOAD_LEN_OFFSET] << 8 | 
                        pSkb_data[TEST_TCP_OFFLOAD_LEN_OFFSET + 1]) & 0xFFFF;
        /* set tso skb parameters */
        //skb->ip_summed = CHECKSUM_PARTIAL; 
        skb->transport_header = ipHeadLen + TEST_MAC_HEAD;
        skb->network_header = TEST_MAC_HEAD;
        skb->inner_network_header = TEST_MAC_HEAD;
        skb->mac_len = TEST_MAC_HEAD;

        /* set skb_shinfo parameters */
        if(tcpHeadLen > TEST_TCP_FIX_HEAD_LEN){
            skb_shinfo(skb)->gso_size = (pSkb_data[TEST_TCP_MSS_OFFSET] << 8 | 
                pSkb_data[TEST_TCP_MSS_OFFSET + 1]) & 0xFFFF;
        }else{
            skb_shinfo(skb)->gso_size = 0;
        }
        if(skb_shinfo(skb)->gso_size != 0){
            lastGsoSize = skb_shinfo(skb)->gso_size;
        }else{
            skb_shinfo(skb)->gso_size = lastGsoSize;
        }
        //DPRINTK("offload_len is %d, skb_shinfo(skb)->gso_size is %d", offload_len, skb_shinfo(skb)->gso_size);
        /* get segment size */
        if(offload_len % skb_shinfo(skb)->gso_size == 0){
            skb_shinfo(skb)->gso_segs = offload_len / skb_shinfo(skb)->gso_size;
            pdata->fxgmac_test_last_tso_len = skb_shinfo(skb)->gso_size + headTotalLen;
        }else{
            skb_shinfo(skb)->gso_segs = offload_len / skb_shinfo(skb)->gso_size + 1;
            pdata->fxgmac_test_last_tso_len = offload_len % skb_shinfo(skb)->gso_size + headTotalLen;
        }
        pdata->fxgmac_test_tso_seg_num = skb_shinfo(skb)->gso_segs;

        skb_shinfo(skb)->gso_type = SKB_GSO_TCPV4 ;
        skb_shinfo(skb)->frag_list = NULL;
        skb->csum_start = skb_headroom(skb) + TEST_MAC_HEAD + ipHeadLen;
        skb->csum_offset = skb->len - TEST_MAC_HEAD - ipHeadLen;

        pdata->fxgmac_test_packet_len = skb_shinfo(skb)->gso_size + headTotalLen;
    }else{
        /* set non-TSO packet parameters */
        pdata->fxgmac_test_packet_len = skb->len;
    }

    /* send data */
    if(dev_queue_xmit(skb) != NET_XMIT_SUCCESS){
        DPRINTK("xmit data fail \n");
    }
}

static void fxgmac_dbg_rx_pkt(struct fxgmac_pdata *pdata, u8 *pcmd_data)
{
    unsigned int totalLen = 0;
    struct sk_buff *rx_skb;
    struct ext_ioctl_data *pcmd;
    fxgmac_test_packet pkt;
    void* addr = 0;
    u8 *rx_data = (u8*)kzalloc(FXGMAC_MAX_DBG_RX_DATA, GFP_KERNEL);
    if (!rx_data)
        return;
    //int i;

    /* initial dest data region */
    pcmd = (struct ext_ioctl_data *)pcmd_data;
    addr = pcmd->cmd_buf.buf;
    //DPRINTK("FXG:fxgmac_test_skb_arr_in_index=%d, fxgmac_test_skb_arr_out_index=%d", fxgmac_test_skb_arr_in_index, fxgmac_test_skb_arr_out_index);
    while(pdata->fxgmac_test_skb_arr_in_index != pdata->fxgmac_test_skb_arr_out_index){
        /* get received skb data */
        rx_skb = pdata->fxgmac_test_skb_array[pdata->fxgmac_test_skb_arr_out_index];

        if(rx_skb->len + sizeof(fxgmac_test_packet) + totalLen < 64000){
            pkt.length = rx_skb->len;
            pkt.type = 0x80;
            pkt.buf[0].offset = totalLen + sizeof(fxgmac_test_packet);
            pkt.buf[0].length = rx_skb->len;

            /* get data from skb */
            //DPRINTK("FXG:rx_skb->len=%d", rx_skb->len);
            memcpy(rx_data, rx_skb->data, rx_skb->len);

	        /* update next pointer */
	        if((pdata->fxgmac_test_skb_arr_out_index + 1) % FXGMAC_MAX_DBG_TEST_PKT == pdata->fxgmac_test_skb_arr_in_index)
	        {
		        pkt.next = NULL;
	        }
	        else
	        {
            	pkt.next = (pfxgmac_test_packet)(addr + totalLen + sizeof(fxgmac_test_packet) + pkt.length);
	        }

            /* copy data to user space */
            if(copy_to_user((void *)(addr + totalLen), (void*)(&pkt), sizeof(fxgmac_test_packet)))
            {
                DPRINTK("cppy pkt data to user fail...");
            }
            //FXGMAC_PR("FXG:rx_skb->len=%d", rx_skb->len);
            if(copy_to_user((void *)(addr + totalLen + sizeof(fxgmac_test_packet)), (void*)rx_data, rx_skb->len))
            {
                DPRINTK("cppy data to user fail...");
            }

            /* update total length */
            totalLen += (sizeof(fxgmac_test_packet) + rx_skb->len);

            /* free skb */
            kfree_skb(rx_skb);
            pdata->fxgmac_test_skb_array[pdata->fxgmac_test_skb_arr_out_index] = NULL;

            /* update gCurSkbOutIndex */
            pdata->fxgmac_test_skb_arr_out_index = (pdata->fxgmac_test_skb_arr_out_index + 1) % FXGMAC_MAX_DBG_TEST_PKT;
       }else{
            DPRINTK("receive data more receive buffer... \n");
            break;
       }
    }

    if (rx_data)
        kfree(rx_data);
#if 0
    pPkt = (pfxgmac_test_packet)buf;
    DPRINTK("FXG: pPkt->Length is %d", pPkt->length);
    DPRINTK("FXG: pPkt->Length is %d", pPkt->length);
    DPRINTK("pPkt: %p, buf is %lx",pPkt, pcmd->cmd_buf.buf);
    for(i = 0; i < 30; i++){
        DPRINTK("%x",*(((u8*)pPkt + sizeof(fxgmac_test_packet)) + i));
    }
#endif
}

long fxgmac_dbg_netdev_ops_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = FXGMAC_SUCCESS;
    struct fxgmac_pdata *pdata = file->private_data;
    struct ext_ioctl_data * pcmd = NULL;
    int in_total_size, in_data_size, out_total_size;
    int ioctl_cmd_size = sizeof(struct ext_ioctl_data);
    struct sk_buff *tmpskb;
    u8* data = NULL;
    u8 *buf = (u8*)kzalloc(FXGMAC_MAX_DBG_BUF_LEN, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    if (!arg) {
        DPRINTK("[%s] command arg is %lx !\n", __func__, arg);
        goto err; 
    }

    /* check device type */
    if (_IOC_TYPE(cmd) != IOC_MAGIC) {
        DPRINTK("[%s] command type [%c] error!\n", __func__, _IOC_TYPE(cmd));
        goto err; 
    }

    /* check command number*/
    if (_IOC_NR(cmd) > IOC_MAXNR) { 
        DPRINTK("[%s] command numer [%d] exceeded!\n", __func__, _IOC_NR(cmd));
        goto err; 
    }

    if(arg != 0 && copy_from_user((void*)buf, (void*)arg, ioctl_cmd_size))
    {
        DPRINTK("copy data from user fail... \n");
        goto err; 
    }

    pcmd = (struct ext_ioctl_data *)buf;
    in_total_size = pcmd->cmd_buf.size_in;
    in_data_size = in_total_size - ioctl_cmd_size;
    out_total_size = pcmd->cmd_buf.size_out;
    data = (u8*)arg + ioctl_cmd_size;
#if 0
    /* check R/W */
    if (_IOC_DIR(cmd) & _IOC_READ || _IOC_DIR(cmd) & _IOC_WRITE)
        ret= access_ok((void __user *)arg, _IOC_SIZE(cmd));    
    if (!ret)
        return -EFAULT;
#endif

    if(arg != 0) {
        switch(pcmd->cmd_type) {
        /* ioctl diag begin */
        case FUXI_DFS_IOCTL_DIAG_BEGIN:
            DPRINTK("Debugfs received diag begin command.\n");
            if (netif_running(pdata->netdev)){
                fxgmac_restart_dev(pdata);
            }

            /* release last loopback test abnormal exit buffer */
            while(pdata->fxgmac_test_skb_arr_in_index != 
                    pdata->fxgmac_test_skb_arr_out_index)
            {
                tmpskb = pdata->fxgmac_test_skb_array[pdata->fxgmac_test_skb_arr_out_index];
                if(tmpskb)
                {
                    kfree_skb(tmpskb);
                    pdata->fxgmac_test_skb_array[pdata->fxgmac_test_skb_arr_out_index] = NULL;
                }

                pdata->fxgmac_test_skb_arr_out_index = (pdata->fxgmac_test_skb_arr_out_index + 1) % FXGMAC_MAX_DBG_TEST_PKT;
            }

            /* init loopback test parameters */
            pdata->fxgmac_test_skb_arr_in_index = 0;
            pdata->fxgmac_test_skb_arr_out_index = 0;
            pdata->fxgmac_test_tso_flag = false;
            pdata->fxgmac_test_tso_seg_num = 0;
            pdata->fxgmac_test_last_tso_len = 0;
            pdata->fxgmac_test_packet_len = 0;
           break;

        /* ioctl diag end */
        case FUXI_DFS_IOCTL_DIAG_END:
            DPRINTK("Debugfs received diag end command.\n");
            if (netif_running(pdata->netdev)){
                fxgmac_restart_dev(pdata);
            }
            break;

        /* ioctl diag tx pkt */
        case FUXI_DFS_IOCTL_DIAG_TX_PKT:
            //FXGMAC_PR("Debugfs received diag tx pkt command.\n");
            if(copy_from_user((void *)buf, (void *)arg, in_total_size))
            {
                DPRINTK("copy data from user fail... \n");
            }
            fxgmac_dbg_tx_pkt(pdata, buf);
            //fxgmac_dbg_tx_pkt(pdata, (u8 *)arg);
            break;

        /* ioctl diag rx pkt */
        case FUXI_DFS_IOCTL_DIAG_RX_PKT:
            //DPRINTK("Debugfs received diag rx pkt command.\n")
            fxgmac_dbg_rx_pkt(pdata, buf);
            //fxgmac_dbg_rx_pkt(pdata, (u8 *)arg);
            break;

        /* ioctl device reset */
        case FUXI_DFS_IOCTL_DEVICE_RESET:
            DPRINTK("Debugfs received device reset command.\n");
            if (netif_running(pdata->netdev)){
                fxgmac_restart_dev(pdata);
            }
            break;

        case FXGMAC_EFUSE_LED_TEST:
            DPRINTK("Debugfs received device led test command.\n");
            if (copy_from_user(&pdata->led, data, in_data_size))
                goto err;
            fxgmac_restart_dev(pdata);
            break;

        case FXGMAC_EFUSE_UPDATE_LED_CFG:
            DPRINTK("Debugfs received device led update command.\n");
            if (copy_from_user(&pdata->ledconfig, data, in_data_size))
                goto err;
            ret = pdata->hw_ops.write_led_config(pdata) ? FXGMAC_SUCCESS : FXGMAC_FAIL;
            pdata->hw_ops.read_led_config(pdata);
            pdata->hw_ops.led_under_active(pdata);
            break;

        case FXGMAC_EFUSE_WRITE_LED:
            if (copy_from_user(&pdata->ex_cmd_data, data, in_data_size))
                goto err;
            DPRINTK("FXGMAC_EFUSE_WRITE_LED, val = 0x%x\n", pdata->ex_cmd_data.val0);
            ret = pdata->hw_ops.write_led(pdata, pdata->ex_cmd_data.val0) ? FXGMAC_SUCCESS : FXGMAC_FAIL;
            break;

        case FXGMAC_EFUSE_WRITE_OOB:
            DPRINTK("FXGMAC_EFUSE_WRITE_OOB.\n");
            ret = pdata->hw_ops.write_oob(pdata) ? FXGMAC_SUCCESS : FXGMAC_FAIL;
            break;

        case FXGMAC_EFUSE_READ_REGIONABC:
            if (copy_from_user((void*)buf, (void*)arg, in_total_size))
                goto err;
            memcpy(&pdata->ex_cmd_data, (void*)buf + ioctl_cmd_size, in_data_size);
            ret = pdata->hw_ops.read_efuse_data(pdata, pdata->ex_cmd_data.val0, &pdata->ex_cmd_data.val1) ? FXGMAC_SUCCESS : FXGMAC_FAIL;
            DPRINTK("FXGMAC_EFUSE_READ_REGIONABC, address = 0x%x, val = 0x%x\n", 
                pdata->ex_cmd_data.val0, 
                pdata->ex_cmd_data.val1);
            if (ret == FXGMAC_SUCCESS) {
                memcpy((void*)(buf + ioctl_cmd_size), &pdata->ex_cmd_data, sizeof(CMD_DATA));
                out_total_size = ioctl_cmd_size + sizeof(CMD_DATA);
                if (copy_to_user((void*)arg, (void*)buf, out_total_size))
                    goto err;
            }
            break;

        case FXGMAC_EFUSE_WRITE_PATCH_REG:
            if (copy_from_user(&pdata->ex_cmd_data, data, in_data_size))
                goto err;
            DPRINTK("FXGMAC_EFUSE_WRITE_PATCH_REG, address = 0x%x, val = 0x%x\n", 
            pdata->ex_cmd_data.val0, 
                pdata->ex_cmd_data.val1);
            ret = pdata->hw_ops.write_patch_to_efuse(pdata, pdata->ex_cmd_data.val0, pdata->ex_cmd_data.val1) ? FXGMAC_SUCCESS : FXGMAC_FAIL;
            break;

        case FXGMAC_EFUSE_READ_PATCH_REG:
            if (copy_from_user((void*)buf, (void*)arg, in_total_size))
                goto err;
            memcpy(&pdata->ex_cmd_data, (void*)buf + ioctl_cmd_size, in_data_size);
            ret = pdata->hw_ops.read_patch_from_efuse(pdata, pdata->ex_cmd_data.val0, &pdata->ex_cmd_data.val1) ? FXGMAC_SUCCESS : FXGMAC_FAIL;
            DPRINTK("FXGMAC_EFUSE_READ_PATCH_REG, address = 0x%x, val = 0x%x\n", 
                pdata->ex_cmd_data.val0, 
                pdata->ex_cmd_data.val1);
            if (ret == FXGMAC_SUCCESS) {
                memcpy((void*)(buf + ioctl_cmd_size), &pdata->ex_cmd_data, sizeof(CMD_DATA));
                out_total_size = ioctl_cmd_size + sizeof(CMD_DATA);
                if (copy_to_user((void*)arg, (void*)buf, out_total_size))
                    goto err;
            }
            break;

        case FXGMAC_EFUSE_WRITE_PATCH_PER_INDEX:
            if (copy_from_user(&pdata->per_reg_data, data, in_data_size))
                goto err;
            ret = pdata->hw_ops.write_patch_to_efuse_per_index(pdata, pdata->per_reg_data.data[0], 
                                                        pdata->per_reg_data.address, 
                                                        pdata->per_reg_data.value) ? FXGMAC_SUCCESS : FXGMAC_FAIL;
            DPRINTK("FXGMAC_EFUSE_WRITE_PATCH_PER_INDEX, index = %d, address = 0x%x, val = 0x%x\n", 
                pdata->per_reg_data.data[0],
                pdata->per_reg_data.address, 
                pdata->per_reg_data.value);
            break;

        case FXGMAC_EFUSE_READ_PATCH_PER_INDEX:
            if (copy_from_user((void*)buf, (void*)arg, in_total_size))
                goto err;
            memcpy(&pdata->per_reg_data, (void*)buf + ioctl_cmd_size, in_data_size);
            ret = pdata->hw_ops.read_patch_from_efuse_per_index(pdata, pdata->per_reg_data.data[0], 
                                                        &pdata->per_reg_data.address, 
                                                        &pdata->per_reg_data.value) ? FXGMAC_SUCCESS : FXGMAC_FAIL;
            DPRINTK("FXGMAC_EFUSE_READ_PATCH_PER_INDEX, address = 0x%x, val = 0x%x\n", 
                pdata->per_reg_data.address, 
                pdata->per_reg_data.value);
            if (ret == FXGMAC_SUCCESS) {
                memcpy((void*)(buf + ioctl_cmd_size), &pdata->per_reg_data, sizeof(PER_REG_INFO));
                out_total_size = ioctl_cmd_size + sizeof(PER_REG_INFO);
                if (copy_to_user((void*)arg, (void*)buf, out_total_size))
                    goto err;
            }
            break;

        case FXGMAC_EFUSE_LOAD:
            DPRINTK("FXGMAC_EFUSE_LOAD.\n");
            ret = pdata->hw_ops.efuse_load(pdata) ? FXGMAC_SUCCESS : FXGMAC_FAIL;
            break;

        default:
            DPRINTK("Debugfs received invalid command: %x.\n", pcmd->cmd_type);
            ret = -ENOTTY;
            break;
        }   
    }

    if (buf)
        kfree(buf);
    return ret;

err:
    if (buf)
        kfree(buf);
    return FXGMAC_FAIL;
}

#ifdef HAVE_FXGMAC_DEBUG_FS

static struct file_operations fxgmac_dbg_netdev_ops_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = fxgmac_dbg_netdev_ops_read,
	.write = fxgmac_dbg_netdev_ops_write,
    .unlocked_ioctl = fxgmac_dbg_netdev_ops_ioctl,
};

/**
 * fxgmac_dbg_adapter_init - setup the debugfs directory for the adapter
 * @adapter: the adapter that is starting up
 **/
void fxgmac_dbg_adapter_init(struct fxgmac_pdata *pdata)
{
    const char *name = pdata->drv_name;
    struct dentry *pfile;

    pdata->dbg_adapter = debugfs_create_dir(name, pdata->fxgmac_dbg_root);
    if (pdata->dbg_adapter) {
        pfile = debugfs_create_file("netdev_ops", 0600,
    	    		    pdata->dbg_adapter, pdata,
    	    		    &fxgmac_dbg_netdev_ops_fops);
        if (!pfile)
        	DPRINTK("debugfs netdev_ops for %s failed\n", name);
    } else {
        DPRINTK("debugfs entry for %s failed\n", name);
    }
}

/**
 * fxgmac_dbg_adapter_exit - clear out the adapter's debugfs entries
 * @adapter: board private structure
 **/
void fxgmac_dbg_adapter_exit(struct fxgmac_pdata *pdata)
{
    if (pdata->dbg_adapter)
    	debugfs_remove_recursive(pdata->dbg_adapter);
    pdata->dbg_adapter = NULL;
}

/**
 * fxgmac_dbg_init - start up debugfs for the driver
 **/
void fxgmac_dbg_init(struct fxgmac_pdata *pdata)
{
    unsigned int i;
    char num[3];
    const char debug_path[] = "/sys/kernel/debug/";
    const char file_prefix[] = "fuxi_";
    char file_path[50];
    char file_name[8];
    
    /* init file_path */
    memset(file_path, '\0', sizeof(file_path));
    memcpy(file_path, debug_path, sizeof(debug_path));

	for(i = 0; i < DF_MAX_NIC_NUM; i++)
	{
        /* init num and filename */
        memset(num, '\0', sizeof(num));
        memset(file_name, '\0', sizeof(file_name));

        /* int to string */
        sprintf(num, "%d", i);

        /* file name */
        memcpy(file_name, file_prefix, sizeof(file_prefix));
        memcpy(file_name + strlen(file_prefix), num, sizeof(num));

        /* file path */
        memcpy(file_path + sizeof(debug_path) - 1, file_name, sizeof(file_name));
        //DPRINTK("FXG: file_path is %s", file_path);

        /* whether file exist */
        pdata->fxgmac_dbg_root = debugfs_lookup(file_name, NULL);
        if (!pdata->fxgmac_dbg_root)
        {
            /* create file */
	        pdata->fxgmac_dbg_root = debugfs_create_dir(file_name, NULL);
            if (IS_ERR(pdata->fxgmac_dbg_root))
                DPRINTK("fxgmac init of debugfs failed\n");

            break;
        }
	}
}

/**
 * fxgmac_dbg_exit - clean out the driver's debugfs entries
 **/
void fxgmac_dbg_exit(struct fxgmac_pdata *pdata)
{
    if (pdata->fxgmac_dbg_root)
        debugfs_remove_recursive(pdata->fxgmac_dbg_root);
}

#endif /* HAVE_XLGMAC_DEBUG_FS */
