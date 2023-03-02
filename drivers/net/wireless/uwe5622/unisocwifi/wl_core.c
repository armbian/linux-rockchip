/*
 * Copyright (C) 2016 Spreadtrum Communications Inc.
 *
 * Authors	:
 * star.liu <star.liu@spreadtrum.com>
 * yifei.li <yifei.li@spreadtrum.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/platform_device.h>
#include <linux/utsname.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <marlin_platform.h>
#include <linux/of.h>
#include "sprdwl.h"
#include "wl_intf.h"
#include "wl_core.h"
#include "tx_msg.h"
#include "rx_msg.h"
#include "msg.h"
#include "txrx.h"
#include "debug.h"
#include "dbg_ini_util.h"
#include "tcp_ack.h"

#ifdef WL_CONFIG_DEBUG
int sprdwl_debug_level = L_ERR;
#else
int sprdwl_debug_level = L_NONE;
#endif

struct device *sprdwl_dev;
void adjust_debug_level(char *buf, unsigned char offset)
{
	int level = buf[offset] - '0';

	switch (level) {
	case L_ERR:
		sprdwl_debug_level = L_ERR;
		break;
	case L_WARN:
		sprdwl_debug_level = L_WARN;
		break;
	case L_INFO:
		sprdwl_debug_level = L_INFO;
		break;
	case L_DBG:
		sprdwl_debug_level = L_DBG;
		break;
	default:
		sprdwl_debug_level = L_ERR;
		wl_err("input wrong debug level\n");
	}

	wl_err("set sprdwl_debug_level: %d\n", sprdwl_debug_level);
}
#ifdef WMMAC_WFA_CERTIFICATION
extern unsigned int vo_ratio;
extern unsigned int vi_ratio;
extern unsigned int be_ratio;
extern unsigned int wmmac_ratio;

void adjust_qos_ratio(char *buf, unsigned char offset)
{
	unsigned int qos_ratio =
		(buf[offset + 3] - '0')*10 + (buf[offset + 4] - '0');

	if (buf[offset] == 'v') {
		if (buf[offset + 1] == 'o')
			vo_ratio = qos_ratio;
		else if (buf[offset + 1] == 'i')
			vi_ratio = qos_ratio;
	} else if (buf[offset] == 'b' && buf[offset + 1] == 'e') {
		be_ratio = qos_ratio;
	} else if (buf[offset] == 'a' && buf[offset + 1] == 'm') {
		wmmac_ratio = qos_ratio;
	}

	wl_err("vo ratio:%u, vi ratio:%u, be ratio:%u, wmmac_ratio:%u\n",
		   vo_ratio, vi_ratio, be_ratio, wmmac_ratio);
}
#endif
unsigned int new_threshold;
void adjust_tdls_threshold(char *buf, unsigned char offset)
{
	unsigned int value = 0;
	unsigned int i = 0;
	unsigned int len = strlen(buf) - strlen("tdls_threshold=");

	for (i = 0; i < len; (value *= 10), i++) {
		if ((buf[offset + i] >= '0') &&
		   (buf[offset + i] <= '9')) {
			value += (buf[offset + i] - '0');
		} else {
			value /= 10;
			break;
		}
	}
	new_threshold = value;
	wl_err("%s, change tdls_threshold to %d\n", __func__, value);
}

struct debuginfo_s {
	void (*func)(char *, unsigned char offset);
	char str[30];
} debuginfo[] = {
	{adjust_debug_level, "debug_level="},
#ifdef WMMAC_WFA_CERTIFICATION
	{adjust_qos_ratio, "qos_ratio:"},
#endif
	{adjust_ts_cnt_debug, "debug_info="},
	{enable_tcp_ack_delay, "tcpack_delay_en="},
	{adjust_tcp_ack_delay, "tcpack_delay_cnt="},
	{adjust_tcp_ack_delay_win, "tcpack_delay_win="},
	{adjust_txnum_level, "txnum_level="},
	{adjust_rxnum_level, "rxnum_level="},
	{adjust_tdls_threshold, "tdls_threshold="},
};

/* TODO: Could we use netdev_alloc_frag instead of kmalloc?
 *       So we did not need to distinguish buffer type
 *       Maybe it could speed up alloc process, too
 */
void sprdwl_free_data(void *data, int buffer_type)
{
	if (buffer_type) { /* Fragment page buffer */
		put_page(virt_to_head_page(data));
	} else { /* Normal buffer */
		kfree(data);
	}
}

void sprdwl_tdls_flow_flush(struct sprdwl_vif *vif, const u8 *peer, u8 oper)
{
	struct sprdwl_intf *intf = vif->priv->hw_priv;
	u8 i;

	if (oper == NL80211_TDLS_SETUP || oper == NL80211_TDLS_ENABLE_LINK) {
		for (i = 0; i < MAX_TDLS_PEER; i++) {
			if (ether_addr_equal(intf->tdls_flow_count[i].da,
						 peer)) {
				memset(&intf->tdls_flow_count[i],
					   0,
					   sizeof(struct tdls_flow_count_para));
				break;
			}
		}
	}
}

void sprdwl_event_tdls_flow_count(struct sprdwl_vif *vif, u8 *data, u16 len)
{
	struct sprdwl_intf *intf = vif->priv->hw_priv;
	u8 i;
	u8 found = 0;
	struct tdls_update_peer_infor *peer_info =
		(struct tdls_update_peer_infor *)data;
	ktime_t kt;

	if (len < sizeof(struct tdls_update_peer_infor)) {
		wl_err("%s, event data len not in range\n", __func__);
		return;
	}
	for (i = 0; i < MAX_TDLS_PEER; i++) {
		if (ether_addr_equal(intf->tdls_flow_count[i].da,
					 peer_info->da)) {
			found = 1;
			break;
		}
	}
	/*0 to delete entry*/
	if (peer_info->valid == 0) {
		if (found == 0) {
			wl_err("%s, invalid da, fail to del\n", __func__);
			return;
		}
		memset(&intf->tdls_flow_count[i],
			   0,
			   sizeof(struct tdls_flow_count_para));

		for (i = 0; i < MAX_TDLS_PEER; i++) {
			if (intf->tdls_flow_count[i].valid == 1)
				found++;
		}
		if (found == 1)
			intf->tdls_flow_count_enable = 0;
	} else if (peer_info->valid == 1) {
		if (found == 0) {
			for (i = 0; i < MAX_TDLS_PEER; i++) {
				if (intf->tdls_flow_count[i].valid == 0) {
					found = 1;
					break;
				}
			}
		}
		if (found == 0) {
			wl_err("%s, no free TDLS entry\n", __func__);
			i = 0;
		}

		intf->tdls_flow_count_enable = 1;
		intf->tdls_flow_count[i].valid = 1;
		ether_addr_copy(intf->tdls_flow_count[i].da, peer_info->da);
		intf->tdls_flow_count[i].threshold = peer_info->txrx_len;
		intf->tdls_flow_count[i].data_len_counted = 0;

		wl_info("%s,%d, tdls_id=%d,threshold=%d, timer=%d, da=(%pM)\n",
			__func__, __LINE__, i,
			intf->tdls_flow_count[i].threshold,
			peer_info->timer, peer_info->da);

		kt = ktime_get();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
		intf->tdls_flow_count[i].start_mstime =
			(u32)(div_u64(kt, NSEC_PER_MSEC));
#else
		intf->tdls_flow_count[i].start_mstime =
			(u32)(div_u64(kt.tv64, NSEC_PER_MSEC));
#endif
		intf->tdls_flow_count[i].timer =
			peer_info->timer;
		wl_info("%s,%d, tdls_id=%d,start_time:%u\n",
			__func__, __LINE__, i,
			intf->tdls_flow_count[i].start_mstime);
	}
}

void count_tdls_flow(struct sprdwl_vif *vif, u8 *data, u16 len)
{
	u8 i, found = 0;
	u32 msec;
	u8 elapsed_time;
	u8 unit_time;
	ktime_t kt;
	struct sprdwl_intf *intf = (struct sprdwl_intf *)vif->priv->hw_priv;
	int ret = 0;

	for (i = 0; i < MAX_TDLS_PEER; i++) {
		if ((intf->tdls_flow_count[i].valid == 1) &&
			(ether_addr_equal(data, intf->tdls_flow_count[i].da)))
			goto count_it;
	}
	return;

count_it:
	if (new_threshold != 0)
		intf->tdls_flow_count[i].threshold = new_threshold;
	kt = ktime_get();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	msec = (u32)(div_u64(kt, NSEC_PER_MSEC));
#else
	msec = (u32)(div_u64(kt.tv64, NSEC_PER_MSEC));
#endif
	elapsed_time =
		(msec - intf->tdls_flow_count[i].start_mstime) / MSEC_PER_SEC;
	unit_time = elapsed_time / intf->tdls_flow_count[i].timer;
	wl_info("%s,%d, tdls_id=%d, len_counted=%d, len=%d, threshold=%dK\n",
		__func__, __LINE__, i,
		intf->tdls_flow_count[i].data_len_counted, len,
		intf->tdls_flow_count[i].threshold);
	wl_info("currenttime=%u, elapsetime=%d, unit_time=%d\n",
		msec, elapsed_time, unit_time);

	if ((intf->tdls_flow_count[i].data_len_counted == 0 &&
		 len > (intf->tdls_flow_count[i].threshold * 1024)) ||
		(intf->tdls_flow_count[i].data_len_counted > 0 &&
		((intf->tdls_flow_count[i].data_len_counted + len) >
		 intf->tdls_flow_count[i].threshold * 1024 *
		 ((unit_time == 0) ? 1 : unit_time)))) {
		ret = sprdwl_send_tdls_cmd(vif, vif->ctx_id,
					   (u8 *)intf->tdls_flow_count[i].da,
					   SPRDWL_TDLS_CMD_CONNECT);
		memset(&intf->tdls_flow_count[i], 0,
				   sizeof(struct tdls_flow_count_para));
	} else {
		if (intf->tdls_flow_count[i].data_len_counted == 0) {
			intf->tdls_flow_count[i].start_mstime = msec;
			intf->tdls_flow_count[i].data_len_counted += len;
		}
		if ((intf->tdls_flow_count[i].data_len_counted > 0) &&
			unit_time > 1) {
			intf->tdls_flow_count[i].start_mstime = msec;
			intf->tdls_flow_count[i].data_len_counted = len;
		}
		if ((intf->tdls_flow_count[i].data_len_counted > 0) &&
			unit_time <= 1) {
			intf->tdls_flow_count[i].data_len_counted += len;
		}
	}
	for (i = 0; i < MAX_TDLS_PEER; i++) {
		if (intf->tdls_flow_count[i].valid == 1)
			found++;
	}
	if (found == 0)
		intf->tdls_flow_count_enable = 0;
}

#define SPRDWL_SDIO_DEBUG_BUFLEN 128
static ssize_t sprdwl_intf_read_info(struct file *file,
					 char __user *user_buf,
					 size_t count, loff_t *ppos)
{
	size_t ret = 0;
	unsigned int buflen, len;
	unsigned char *buf;
	struct sprdwl_intf *sdev;
	struct sprdwl_tx_msg *tx_msg;

	sdev = (struct sprdwl_intf *)file->private_data;
	tx_msg = (struct sprdwl_tx_msg *)sdev->sprdwl_tx;
	buflen = SPRDWL_SDIO_DEBUG_BUFLEN;
	buf = kzalloc(buflen, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len = 0;
	len += scnprintf(buf, buflen,
			 "net: stop %lu, start %lu\n drop cnt:\n"
			 "cmd %lu, sta %lu, p2p %lu,\n"
			 "ring_ap:%lu ring_cp:%lu red_flow:%u,\n"
			 "green_flow:%u blue_flow:%u white_flow:%u\n",
			 tx_msg->net_stop_cnt, tx_msg->net_start_cnt,
			 tx_msg->drop_cmd_cnt, tx_msg->drop_data1_cnt,
			 tx_msg->drop_data2_cnt,
			 tx_msg->ring_ap, tx_msg->ring_cp,
			 atomic_read(&tx_msg->flow_ctrl[0].flow),
			 atomic_read(&tx_msg->flow_ctrl[1].flow),
			 atomic_read(&tx_msg->flow_ctrl[2].flow),
			 atomic_read(&tx_msg->flow_ctrl[3].flow));
	if (len > buflen)
		len = buflen;

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return ret;
}

static ssize_t sprdwl_intf_write(struct file *file,
				 const char __user *__user_buf,
				 size_t count, loff_t *ppos)
{
	char buf[30];
	struct sprdwl_intf *sdev;
	int type = 0;
	int debug_size = sizeof(debuginfo)/sizeof(struct debuginfo_s);

	sdev = (struct sprdwl_intf *)file->private_data;

	if (!count || count >= sizeof(buf)) {
		wl_err("write len too long:%zu >= %zu\n", count, sizeof(buf));
		return -EINVAL;
	}
	if (copy_from_user(buf, __user_buf, count))
		return -EFAULT;
	buf[count] = '\0';
	wl_debug("write info:%s\n", buf);
	for (type = 0; type < debug_size; type++)
		if (!strncmp(debuginfo[type].str, buf,
				 strlen(debuginfo[type].str))) {
			wl_err("write info:type %d\n", type);
			debuginfo[type].func(buf, strlen(debuginfo[type].str));
			break;
		}

	return count;
}

static const struct file_operations sprdwl_intf_debug_fops = {
	.read = sprdwl_intf_read_info,
	.write = sprdwl_intf_write,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek
};

static int txrx_debug_show(struct seq_file *s, void *p)
{
	unsigned int i = 0;

	for (i = 0; i < MAX_DEBUG_CNT_INDEX; i++)
		debug_cnt_show(s, i);

	for (i = 0; i < MAX_DEBUG_TS_INDEX; i++)
		debug_ts_show(s, i);

	for (i = 0; i < MAX_DEBUG_RECORD_INDEX; i++)
		debug_record_show(s, i);

	return 0;
}

static int txrx_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, txrx_debug_show, inode->i_private);
}

static ssize_t txrx_debug_write(struct file *file,
				const char __user *__user_buf,
				size_t count, loff_t *ppos)
{
	char buf[20] = "debug_info=";
	unsigned char len = strlen(buf);

	if (!count || (count + len) >= sizeof(buf)) {
		wl_err("write len too long:%zu >= %zu\n", count, sizeof(buf));
		return -EINVAL;
	}

	if (copy_from_user((buf + len), __user_buf, count))
		return -EFAULT;

	buf[count + len] = '\0';
	wl_debug("write info:%s\n", buf);

	adjust_ts_cnt_debug(buf, len);

	return count;
}

static const struct file_operations txrx_debug_fops = {
	.owner = THIS_MODULE,
	.open = txrx_debug_open,
	.read = seq_read,
	.write = txrx_debug_write,
	.llseek = seq_lseek,
	.release = single_release,
};

void sprdwl_debugfs(void *spdev, struct dentry *dir)
{
	struct sprdwl_intf *intf;

	intf = (struct sprdwl_intf *)spdev;
	debugfs_create_file("sprdwlinfo", S_IRUSR,
				dir, intf, &sprdwl_intf_debug_fops);
}

static struct dentry *sprdwl_debug_root;

void sprdwl_debugfs_init(struct sprdwl_intf *intf)
{
	/* create debugfs */
	sprdwl_debug_root = debugfs_create_dir("sprdwl_debug", NULL);
	if (IS_ERR(sprdwl_debug_root)) {
		wl_err("%s, create dir fail!\n", __func__);
		sprdwl_debug_root = NULL;
		return;
	}

	if (!debugfs_create_file("log_level", S_IRUSR | S_IWUSR,
		sprdwl_debug_root, intf, &sprdwl_intf_debug_fops))
		wl_err("%s, create file fail!\n", __func__);

	if (!debugfs_create_file("txrx_dbg", S_IRUSR | S_IWUSR,
		sprdwl_debug_root, NULL, &txrx_debug_fops))
		wl_err("%s, %d, create_file fail!\n", __func__, __LINE__);
	else
		debug_ctrl_init();
}

void sprdwl_debugfs_deinit(void)
{
	/* remove debugfs */
	debugfs_remove_recursive(sprdwl_debug_root);
}

static int sprdwl_ini_download_status(void)
{
	/*disable download ini function, just return 1*/
	/*	return 1; */
	/*fw is ready for receive ini file*/
	/* return !is_first_power_on(MARLIN_WIFI); */
	return !cali_ini_need_download(MARLIN_WIFI);
}

static void sprdwl_force_exit(void *spdev)
{
	struct sprdwl_intf *intf;
	struct sprdwl_tx_msg *tx_msg;

	intf = (struct sprdwl_intf *)spdev;
	tx_msg = (struct sprdwl_tx_msg *)intf->sprdwl_tx;
	intf->exit = 1;
}

static int sprdwl_is_exit(void *spdev)
{
	struct sprdwl_intf *intf;

	intf = (struct sprdwl_intf *)spdev;
	return intf->exit;
}

static void sprdwl_tcp_drop_msg(void *spdev, struct sprdwl_msg_buf *msgbuf)
{
	enum sprdwl_mode mode;
	struct sprdwl_msg_list *list;
	struct sprdwl_intf *intf = (struct sprdwl_intf *)spdev;

	if (msgbuf->skb)
		dev_kfree_skb(msgbuf->skb);
	mode = msgbuf->mode;
	list = msgbuf->msglist;
	sprdwl_free_msg_buf(msgbuf, list);
	sprdwl_wake_net_ifneed(intf, list, mode);
}

static struct sprdwl_if_ops sprdwl_core_ops = {
	.get_msg_buf = sprdwl_get_msg_buf,
	.free_msg_buf = sprdwl_tx_free_msg_buf,
#ifdef SPRDWL_TX_SELF
	.tx = sprdwl_tx_self_msg,
#else
	.tx = sprdwl_tx_msg_func,
#endif
	.force_exit = sprdwl_force_exit,
	.is_exit = sprdwl_is_exit,
	.debugfs = sprdwl_debugfs,
	.tcp_drop_msg = sprdwl_tcp_drop_msg,
	.ini_download_status = sprdwl_ini_download_status
};

static struct notifier_block boost_notifier = {
	.notifier_call = sprdwl_notifier_boost,
};

#ifdef CP2_RESET_SUPPORT
extern struct sprdwl_priv *g_sprdwl_priv;
extern void sprdwl_cancel_scan(struct sprdwl_vif *vif);
extern void sprdwl_cancel_sched_scan(struct sprdwl_vif *vif);
extern void sprdwl_flush_all_txlist(struct sprdwl_tx_msg *sprdwl_tx_dev);
extern int sprdwl_cmd_init(void);
extern void sprdwl_cmd_deinit(void);
extern void sprdwl_net_flowcontrl(struct sprdwl_priv *priv,
			   enum sprdwl_mode mode, bool state);
extern void sprdwl_reg_notify(struct wiphy *wiphy, struct regulatory_request *request);
struct work_struct wifi_rst_begin;
struct work_struct wifi_rst_down;
struct completion wifi_reset_ready;
extern struct sprdwl_cmd g_sprdwl_cmd;

static void wifi_reset_wq(struct work_struct *work)
{
	struct sprdwl_vif *vif, *tmp_vif;
	struct sprdwl_intf *intf = NULL;
	struct sprdwl_tx_msg *tx_msg = NULL;
	struct sprdwl_rx_if *rx_if = NULL;

	intf = (struct sprdwl_intf *)g_sprdwl_priv->hw_priv;
	tx_msg = (void *)intf->sprdwl_tx;
	rx_if = (struct sprdwl_rx_if *)intf->sprdwl_rx;

	reinit_completion(&wifi_reset_ready);

	wl_err("cp2 reset begin..........\n");
	g_sprdwl_priv->sync.scan_not_allowed = true;
	g_sprdwl_priv->sync.cmd_not_allowed = true;
	intf->cp_asserted = 1;
	sprdwl_reorder_init(&rx_if->ba_entry);
	sprdwl_net_flowcontrl(g_sprdwl_priv, SPRDWL_MODE_NONE, false);
	if (tx_msg->tx_thread)
		tx_up(tx_msg);

	sprdwl_flush_all_txlist(tx_msg);
	flush_workqueue(rx_if->rx_queue);
	list_for_each_entry_safe(vif, tmp_vif, &g_sprdwl_priv->vif_list, vif_node) {
		g_sprdwl_priv->sync.fw_stat[vif->mode] =  g_sprdwl_priv->fw_stat[vif->mode];
		g_sprdwl_priv->fw_stat[vif->mode] = SPRDWL_INTF_CLOSE;
		sprdwl_report_disconnection(vif, true);
		if (g_sprdwl_priv->scan_vif)
			sprdwl_cancel_scan(g_sprdwl_priv->scan_vif);
		if (g_sprdwl_priv->sched_scan_vif) {
			sprdwl_sched_scan_done(g_sprdwl_priv->sched_scan_vif, true);
			sprdwl_cancel_sched_scan(g_sprdwl_priv->sched_scan_vif);
		}
	}

	sprdwl_vendor_deinit(g_sprdwl_priv->wiphy);
	sprdwl_cmd_wake_upall();
	sprdwl_tcp_ack_deinit(g_sprdwl_priv);
	sprdwl_intf_deinit(intf);
	// sprdwl_cmd_deinit();
	complete(&wifi_reset_ready);
	wl_err("cp2 reset finish..........\n");

}

static void wifi_resume_wq(struct work_struct *work)
{
	struct sprdwl_vif *vif, *tmp_vif;
	struct sprdwl_intf *intf = NULL;
	struct sprdwl_rx_if *rx_if = NULL;
	wl_err("cp2 resume begin...............\n");

	intf = (struct sprdwl_intf *)g_sprdwl_priv->hw_priv;
	rx_if = (struct sprdwl_rx_if *)intf->sprdwl_rx;

	wait_for_completion(&wifi_reset_ready);

	sprdwl_intf_init(g_sprdwl_priv, intf);
	// sprdwl_cmd_init();
	wl_err("sprdwl cmd init finish.\n");
	g_sprdwl_priv->sync.cmd_not_allowed = false;
	intf->cp_asserted = 0;
	sprdwl_net_flowcontrl(g_sprdwl_priv, SPRDWL_MODE_NONE, true);
	sprdwl_reorder_init(&rx_if->ba_entry);
	sprdwl_sync_version(g_sprdwl_priv);
	sprdwl_download_ini(g_sprdwl_priv);
	sprdwl_tcp_ack_init(g_sprdwl_priv);
	sprdwl_get_fw_info(g_sprdwl_priv);
	sprdwl_setup_wiphy(g_sprdwl_priv->wiphy, g_sprdwl_priv);
	sprdwl_vendor_init(g_sprdwl_priv->wiphy);

	sprdwl_reg_notify(g_sprdwl_priv->wiphy, &g_sprdwl_priv->sync.request);

	list_for_each_entry_safe(vif, tmp_vif, &g_sprdwl_priv->vif_list, vif_node) {
		if (SPRDWL_INTF_OPEN == g_sprdwl_priv->sync.fw_stat[vif->mode]) {
			vif->mode = SPRDWL_MODE_NONE;
			sprdwl_init_fw(vif);
		}
	}
	g_sprdwl_priv->sync.scan_not_allowed = false;
	wl_err("cp2 resume complete...............\n");
}

static void wifi_reset_init(void)
{
	INIT_WORK(&wifi_rst_begin, wifi_reset_wq);
	INIT_WORK(&wifi_rst_down, wifi_resume_wq);
	init_completion(&wifi_reset_ready);
	return;
}

int wifi_reset_callback(struct notifier_block *nb, unsigned long event, void *v)
{
	wl_info("%s[%d]: %s %d\n", __func__, __LINE__, (char *)v, (int)event);
	switch (event) {
	case 1:
		schedule_work(&wifi_rst_begin);
		break;
	case 0:
		schedule_work(&wifi_rst_down);
		break;
	}

	return NOTIFY_OK;
}
static struct notifier_block wifi_reset_notifier = {
	.notifier_call = wifi_reset_callback,
};
#endif

static int sprdwl_probe(struct platform_device *pdev)
{
	struct sprdwl_intf *intf;
	struct sprdwl_priv *priv;
	int ret;
	u8 i;

#ifdef CP2_RESET_SUPPORT
	wifi_reset_init();
	marlin_reset_callback_register(MARLIN_WIFI, &wifi_reset_notifier);
#endif

	if (start_marlin(MARLIN_WIFI)) {
		wl_err("%s power on chipset failed\n", __func__);
		return -ENODEV;
	}

	intf = kzalloc(sizeof(*intf), GFP_ATOMIC);
	if (!intf) {
		ret = -ENOMEM;
		wl_err("%s alloc intf fail: %d\n", __func__, ret);
		goto err;
	}

	platform_set_drvdata(pdev, intf);
	intf->pdev = pdev;
	sprdwl_dev = &pdev->dev;

	for (i = 0; i < MAX_LUT_NUM; i++)
		intf->peer_entry[i].ctx_id = 0xff;

	dbg_util_init(&intf->ini_cfg);
#ifdef STA_SOFTAP_SCC_MODE
	intf->sta_home_channel = 0;
#endif
	priv = sprdwl_core_create(get_hwintf_type(),
				  &sprdwl_core_ops);
	if (!priv) {
		wl_err("%s core create fail\n", __func__);
		ret = -ENXIO;
		goto err_core_create;
	}
	memcpy(priv->wl_ver.kernel_ver, utsname()->release,
			strlen(utsname()->release));
	memcpy(priv->wl_ver.drv_ver, SPRDWL_DRIVER_VERSION,
			strlen(SPRDWL_DRIVER_VERSION));
	memcpy(priv->wl_ver.update, SPRDWL_UPDATE, strlen(SPRDWL_UPDATE));
	memcpy(priv->wl_ver.reserve, SPRDWL_RESERVE, strlen(SPRDWL_RESERVE));
	wl_info("Spreadtrum WLAN Version:");
	wl_info("Kernel:%s,Driver:%s,update:%s,reserved:%s\n",
			 utsname()->release, SPRDWL_DRIVER_VERSION,
			 SPRDWL_UPDATE, SPRDWL_RESERVE);

	if (priv->hw_type == SPRDWL_HW_SDIO) {
		intf->hif_offset = sizeof(struct sdiohal_puh);
		intf->rx_cmd_port = SDIO_RX_CMD_PORT;
		intf->rx_data_port = SDIO_RX_DATA_PORT;
		intf->tx_cmd_port = SDIO_TX_CMD_PORT;
		intf->tx_data_port = SDIO_TX_DATA_PORT;
	} else if (priv->hw_type == SPRDWL_HW_PCIE) {
		intf->rx_cmd_port = PCIE_RX_CMD_PORT;
		intf->rx_data_port = PCIE_RX_DATA_PORT;
		intf->tx_cmd_port = PCIE_TX_CMD_PORT;
		intf->tx_data_port = PCIE_TX_DATA_PORT;
	} else if (priv->hw_type == SPRDWL_HW_USB) {
		intf->hif_offset = 0;
		intf->rx_cmd_port = USB_RX_CMD_PORT;
		intf->rx_data_port = USB_RX_DATA_PORT;
		intf->tx_cmd_port = USB_TX_CMD_PORT;
		intf->tx_data_port = USB_TX_DATA_PORT;
	}

	ret = sprdwl_intf_init(priv, intf);
	if (ret) {
		wl_err("%s intf init failed: %d\n", __func__, ret);
		goto err_if_init;
	}

	ret = sprdwl_rx_init(intf);
	if (ret) {
		wl_err("%s rx init failed: %d\n", __func__, ret);
		goto err_rx_init;
	}

	ret = sprdwl_tx_init(intf);
	if (ret) {
		wl_err("%s tx_list init failed\n", __func__);
		goto err_tx_init;
	}

	ret = sprdwl_core_init(&pdev->dev, priv);
	if (ret)
		goto err_core_init;

#if defined FPGA_LOOPBACK_TEST
	intf->loopback_n = 0;
	sprdwl_intf_tx_data_fpga_test(intf, NULL, 0);
#endif

	sprdwl_debugfs_init(intf);
	cpufreq_register_notifier(&boost_notifier, CPUFREQ_POLICY_NOTIFIER);

	return ret;

err_core_init:
	sprdwl_bus_deinit();
	sprdwl_tx_deinit(intf);
err_tx_init:
	sprdwl_rx_deinit(intf);
err_rx_init:
	sprdwl_intf_deinit(intf);
err_if_init:
	sprdwl_core_free((struct sprdwl_priv *)intf->priv);
err_core_create:
	kfree(intf);
err:
	return ret;
}

static int sprdwl_remove(struct platform_device *pdev)
{
	struct sprdwl_intf *intf = platform_get_drvdata(pdev);
	struct sprdwl_priv *priv = intf->priv;

#ifdef CP2_RESET_SUPPORT
	marlin_reset_callback_unregister(MARLIN_WIFI, &wifi_reset_notifier);
#endif

	cpufreq_unregister_notifier(&boost_notifier, CPUFREQ_POLICY_NOTIFIER);
	sprdwl_debugfs_deinit();
	sprdwl_core_deinit(priv);
	sprdwl_bus_deinit();
	sprdwl_tx_deinit(intf);
	sprdwl_rx_deinit(intf);
	sprdwl_intf_deinit(intf);
	sprdwl_core_free(priv);
	kfree(intf);
	stop_marlin(MARLIN_WIFI);
	wl_info("%s\n", __func__);

	return 0;
}

static const struct of_device_id sprdwl_of_match[] = {
	{.compatible = "sprd,unisoc-wifi",},
	{}
};
MODULE_DEVICE_TABLE(of, sprdwl_of_match);

static struct platform_driver sprdwl_driver = {
	.probe = sprdwl_probe,
	.remove = sprdwl_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "unisoc_wifi",
		.of_match_table = sprdwl_of_match,
	}
};

#ifdef OTT_UWE
static struct platform_device *unisoc_pdev;
static int __init unisoc_wlan_init(void)
{
	int ret;

	ret = platform_driver_register(&sprdwl_driver);
	if (!ret) {
		unisoc_pdev = platform_device_alloc("unisoc_wifi", -1);
		if (platform_device_add(unisoc_pdev) != 0)
			wl_err("register platform device unisoc wifi failed\n");
	}

	return ret;
}

static void __exit unisoc_wlan_exit(void)
{
	platform_driver_unregister(&sprdwl_driver);
	platform_device_del(unisoc_pdev);
}

module_init(unisoc_wlan_init);
module_exit(unisoc_wlan_exit);
#else
module_platform_driver(sprdwl_driver);
#endif

MODULE_DESCRIPTION("Spreadtrum Wireless LAN Driver");
MODULE_AUTHOR("Spreadtrum WCN Division");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
