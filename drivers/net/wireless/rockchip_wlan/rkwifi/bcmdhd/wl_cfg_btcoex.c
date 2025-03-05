/*
 * Linux cfg80211 driver - Dongle Host Driver (DHD) related
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

#include <net/rtnetlink.h>

#include <bcmutils.h>
#include <bcmendian.h>
#include <wldev_common.h>
#include <wl_cfg80211.h>
#include <dhd_cfg80211.h>
#include <dngl_stats.h>
#include <dhd.h>
#include <bcmiov.h>
#include <dhdioctl.h>
#include <wlioctl.h>

#ifdef PKT_FILTER_SUPPORT
extern uint dhd_pkt_filter_enable;
extern uint dhd_master_mode;
extern void dhd_pktfilter_offload_enable(dhd_pub_t * dhd, char *arg, int enable, int master_mode);
#endif

typedef enum wl_cfg_btcx_timer_trig_type {
	BT_DHCP_TIMER_IDLE,
	BT_DHCP_TIMER_TRIGGER_NORMAL,
	BT_DHCP_TIMER_TRIGGER_SCO
} wl_cfg_btcx_timer_trig_type_t;

typedef enum wl_cfg_btcx_dhcp_state {
	BT_DHCP_IDLE,
	BT_DHCP_START,
	BT_DHCP_OPPR_WIN,
	BT_DHCP_FLAG_FORCE_TIMEOUT
} wl_cfg_btcx_dhcp_state_t;

struct btcoex_info {
	timer_list_compat_t timer;
	u32 timer_ms;
	u32 timer_on;
	u32 ts_dhcp_start;				/* ms ts ecord time stats */
	u32 ts_dhcp_ok;					/* ms ts ecord time stats */
	bool dhcp_done;					/* flag, indicates that host done with
							 * dhcp before t1/t2 expiration
							 */
	wl_cfg_btcx_dhcp_state_t bt_state;
	wl_cfg_btcx_timer_trig_type_t timer_trig_type;	/* timer trigger type */
	struct work_struct work;
	struct net_device *dev;
};

#if defined(OEM_ANDROID)
static struct btcoex_info *btcoex_info_loc = NULL;

/* TODO: clean up the BT-Coex code, it still have some legacy ioctl/iovar functions */

/* use New SCO/eSCO smart YG suppression */
#define BT_DHCP_eSCO_FIX
/* this flag boost wifi pkt priority to max, caution: -not fair to sco */
#define BT_DHCP_USE_FLAGS
/* T1 start SCO/ESCo priority suppression */
#define BT_DHCP_OPPR_WIN_TIME	2500
/* T2 turn off SCO/SCO supperesion is (timeout) */
#define BT_DHCP_FLAG_FORCE_TIME 5500

#define	BTCOEXMODE	"BTCOEXMODE"
#define	POWERMODE	"POWERMODE"

/*
 * get named driver variable to uint register value and return error indication
 * calling example: dev_wlc_intvar_get_reg(dev, "btc_params",66, &reg_value)
 */
static int
dev_wlc_intvar_get_reg(struct net_device *dev, char *name,
	uint reg, int *retval)
{
	union {
		char buf[WLC_IOCTL_SMLEN];
		int val;
	} var;
	int error;

	bzero(&var, sizeof(var));
	error = bcm_mkiovar(name, (char *)(&reg), sizeof(reg), (char *)(&var), sizeof(var.buf));
	if (error == 0) {
		return BCME_BUFTOOSHORT;
	}
	error = wldev_ioctl_get(dev, WLC_GET_VAR, (char *)(&var), sizeof(var.buf));

	*retval = dtoh32(var.val);
	return (error);
}

static int
dev_wlc_bufvar_set(struct net_device *dev, char *name, char *buf, int len)
{
	char ioctlbuf_local[WLC_IOCTL_SMLEN];
	int ret;

	ret = bcm_mkiovar(name, buf, len, ioctlbuf_local, sizeof(ioctlbuf_local));
	if (ret == 0)
		return BCME_BUFTOOSHORT;
	return (wldev_ioctl_set(dev, WLC_SET_VAR, ioctlbuf_local, ret));
}

/*
get named driver variable to uint register value and return error indication
calling example: dev_wlc_intvar_set_reg(dev, "btc_params",66, value)
*/
static int
dev_wlc_intvar_set_reg(struct net_device *dev, char *name, char *addr, char * val)
{
	char reg_addr[8];

	bzero(reg_addr, sizeof(reg_addr));
	memcpy((char *)&reg_addr[0], (char *)addr, 4);
	memcpy((char *)&reg_addr[4], (char *)val, 4);

	return (dev_wlc_bufvar_set(dev, name, (char *)&reg_addr[0], sizeof(reg_addr)));
}

/* andrey: bt pkt period independant sco/esco session detection algo.  */
static bool btcoex_is_sco_active(struct net_device *dev)
{
	int ioc_res = 0;
	bool res = FALSE;
	int sco_id_cnt = 0;
	int param27;
	int i;

	for (i = 0; i < 12; i++) {

		ioc_res = dev_wlc_intvar_get_reg(dev, "btc_params", 27, &param27);

		WL_TRACE(("sample[%d], btc params: 27:%x\n", i, param27));

		if (ioc_res < 0) {
			WL_ERR(("ioc read btc params error\n"));
			break;
		}

		if ((param27 & 0x6) == 2) { /* count both sco & esco  */
			sco_id_cnt++;
		}

		if (sco_id_cnt > 2) {
			WL_TRACE(("sco/esco detected, pkt id_cnt:%d  samples:%d\n",
				sco_id_cnt, i));
			res = TRUE;
			break;
		}

		OSL_SLEEP(5);
	}

	return res;
}

#if defined(BT_DHCP_eSCO_FIX)
/* Enhanced BT COEX settings for eSCO compatibility during DHCP window */
static int set_btc_esco_params(struct net_device *dev, bool trump_sco)
{
	static bool saved_status = FALSE;

	char buf_reg50va_dhcp_on[8] =
		{ 50, 00, 00, 00, 0x22, 0x80, 0x00, 0x00 };
	char buf_reg51va_dhcp_on[8] =
		{ 51, 00, 00, 00, 0x00, 0x00, 0x00, 0x00 };
	char buf_reg64va_dhcp_on[8] =
		{ 64, 00, 00, 00, 0x00, 0x00, 0x00, 0x00 };
	char buf_reg65va_dhcp_on[8] =
		{ 65, 00, 00, 00, 0x00, 0x00, 0x00, 0x00 };
	char buf_reg71va_dhcp_on[8] =
		{ 71, 00, 00, 00, 0x00, 0x00, 0x00, 0x00 };
	uint32 regaddr;
	static uint32 saved_reg50;
	static uint32 saved_reg51;
	static uint32 saved_reg64;
	static uint32 saved_reg65;
	static uint32 saved_reg71;

	if (trump_sco) {
		/* this should reduce eSCO agressive retransmit
		 * w/o breaking it
		 */

		/* 1st save current */
		WL_TRACE(("Do new SCO/eSCO coex algo {save &"
			  "override}\n"));
		if ((!dev_wlc_intvar_get_reg(dev, "btc_params", 50, &saved_reg50)) &&
			(!dev_wlc_intvar_get_reg(dev, "btc_params", 51, &saved_reg51)) &&
			(!dev_wlc_intvar_get_reg(dev, "btc_params", 64, &saved_reg64)) &&
			(!dev_wlc_intvar_get_reg(dev, "btc_params", 65, &saved_reg65)) &&
			(!dev_wlc_intvar_get_reg(dev, "btc_params", 71, &saved_reg71))) {
			saved_status = TRUE;
			WL_TRACE(("saved bt_params[50,51,64,65,71]:"
				  "0x%x 0x%x 0x%x 0x%x 0x%x\n",
				  saved_reg50, saved_reg51,
				  saved_reg64, saved_reg65, saved_reg71));
		} else {
			WL_ERR((":%s: save btc_params failed\n",
				__FUNCTION__));
			saved_status = FALSE;
			return -1;
		}

		/* pacify the eSco   */
		WL_TRACE(("override with [50,51,64,65,71]:"
			  "0x%x 0x%x 0x%x 0x%x 0x%x\n",
			  *(u32 *)(buf_reg50va_dhcp_on+4),
			  *(u32 *)(buf_reg51va_dhcp_on+4),
			  *(u32 *)(buf_reg64va_dhcp_on+4),
			  *(u32 *)(buf_reg65va_dhcp_on+4),
			  *(u32 *)(buf_reg71va_dhcp_on+4)));

		dev_wlc_bufvar_set(dev, "btc_params",
			(char *)&buf_reg50va_dhcp_on[0], 8);
		dev_wlc_bufvar_set(dev, "btc_params",
			(char *)&buf_reg51va_dhcp_on[0], 8);
		dev_wlc_bufvar_set(dev, "btc_params",
			(char *)&buf_reg64va_dhcp_on[0], 8);
		dev_wlc_bufvar_set(dev, "btc_params",
			(char *)&buf_reg65va_dhcp_on[0], 8);
		dev_wlc_bufvar_set(dev, "btc_params",
			(char *)&buf_reg71va_dhcp_on[0], 8);

		saved_status = TRUE;
	} else if (saved_status) {
		/* restore previously saved bt params */
		WL_TRACE(("Do new SCO/eSCO coex algo {save &"
			  "override}\n"));

		regaddr = 50;
		dev_wlc_intvar_set_reg(dev, "btc_params",
			(char *)&regaddr, (char *)&saved_reg50);
		regaddr = 51;
		dev_wlc_intvar_set_reg(dev, "btc_params",
			(char *)&regaddr, (char *)&saved_reg51);
		regaddr = 64;
		dev_wlc_intvar_set_reg(dev, "btc_params",
			(char *)&regaddr, (char *)&saved_reg64);
		regaddr = 65;
		dev_wlc_intvar_set_reg(dev, "btc_params",
			(char *)&regaddr, (char *)&saved_reg65);
		regaddr = 71;
		dev_wlc_intvar_set_reg(dev, "btc_params",
			(char *)&regaddr, (char *)&saved_reg71);

		WL_TRACE(("restore bt_params[50,51,64,65,71]:"
			"0x%x 0x%x 0x%x 0x%x 0x%x\n",
			saved_reg50, saved_reg51, saved_reg64,
			saved_reg65, saved_reg71));

		saved_status = FALSE;
	} else {
		WL_ERR((":%s att to restore not saved BTCOEX params\n",
			__FUNCTION__));
		return -1;
	}
	return 0;
}
#endif /* BT_DHCP_eSCO_FIX */

static void
wl_cfg80211_btcoex_init_handler_status(void)
{
	if (!btcoex_info_loc)
		return;

	btcoex_info_loc->timer_trig_type = BT_DHCP_TIMER_IDLE;
	btcoex_info_loc->bt_state = BT_DHCP_IDLE;
}

static void
wl_cfg80211_bt_setflag(struct net_device *dev, bool set)
{
#if defined(BT_DHCP_USE_FLAGS)
	char buf_flag7_dhcp_on[8] = { 7, 00, 00, 00, 0x1, 0x0, 0x00, 0x00 };
	char buf_flag7_default[8]   = { 7, 00, 00, 00, 0x0, 0x00, 0x00, 0x00};
#endif

#if defined(BT_DHCP_eSCO_FIX)
	/*  ANREY: New Yury's eSco pacifier */
	/* set = 1, save & turn on  0 - off & restore prev settings */
	set_btc_esco_params(dev, set);
#endif

#if defined(BT_DHCP_USE_FLAGS)
/*  ANdrey: old WI-FI priority boost via flags   */
	WL_TRACE(("WI-FI priority boost via bt flags, set:%d\n", set));
	if (set == TRUE)
		/* Forcing bt_flag7  */
		dev_wlc_bufvar_set(dev, "btc_flags",
			(char *)&buf_flag7_dhcp_on[0],
			sizeof(buf_flag7_dhcp_on));
	else
		/* Restoring default bt flag7 */
		dev_wlc_bufvar_set(dev, "btc_flags",
			(char *)&buf_flag7_default[0],
			sizeof(buf_flag7_default));
#endif
}

static void wl_cfg80211_bt_timerfunc(ulong data)
{
	struct btcoex_info *bt_local = (struct btcoex_info *)data;
	WL_TRACE(("Enter\n"));
	bt_local->timer_on = 0;
	schedule_work(&bt_local->work);
}

static void wl_cfg80211_bt_handler(struct work_struct *work)
{
	struct btcoex_info *btcx_inf;

	GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
	btcx_inf = container_of(work, struct btcoex_info, work);
	GCC_DIAGNOSTIC_POP();

	if (btcx_inf->timer_on) {
		btcx_inf->timer_on = 0;
		del_timer_sync(&btcx_inf->timer);
	}

	switch (btcx_inf->bt_state) {
		case BT_DHCP_START:
			/* DHCP started
			 * provide OPPORTUNITY window to get DHCP address
			 */
			WL_TRACE(("bt_dhcp stm: started \n"));

			btcx_inf->bt_state = BT_DHCP_OPPR_WIN;
			mod_timer(&btcx_inf->timer,
				jiffies + msecs_to_jiffies(BT_DHCP_OPPR_WIN_TIME));
			btcx_inf->timer_on = 1;
			break;

		case BT_DHCP_OPPR_WIN:
			if (btcx_inf->dhcp_done) {
				WL_TRACE(("DHCP Done before T1 expiration\n"));
				goto btc_coex_idle;
			}

			if (btcx_inf->timer_trig_type == BT_DHCP_TIMER_TRIGGER_SCO) {
				/* DHCP is not over yet, start lowering BT priority
				 * enforce btc_params + flags if necessary
				 */
				WL_TRACE(("DHCP T1:%d expired\n", BT_DHCP_OPPR_WIN_TIME));
				if (btcx_inf->dev)
					wl_cfg80211_bt_setflag(btcx_inf->dev, TRUE);

				btcx_inf->bt_state = BT_DHCP_FLAG_FORCE_TIMEOUT;
				mod_timer(&btcx_inf->timer,
					jiffies + msecs_to_jiffies(BT_DHCP_FLAG_FORCE_TIME));
				btcx_inf->timer_on = 1;
			}
			else {
				goto btc_coex_idle;
			}

			break;

		case BT_DHCP_FLAG_FORCE_TIMEOUT:
			if (btcx_inf->dhcp_done) {
				WL_TRACE(("DHCP Done before T2 expiration\n"));
			} else {
				/* Noo dhcp during T1+T2, restore BT priority */
				WL_TRACE(("DHCP wait interval T2:%d msec expired\n",
					BT_DHCP_FLAG_FORCE_TIME));
			}
			/* Pass through */
			fallthrough;

		default:
			if (btcx_inf->bt_state != BT_DHCP_FLAG_FORCE_TIMEOUT) {
				WL_ERR(("Error BT DHCP status=%d!!!\n", btcx_inf->bt_state));
			}

			/* Restoring default bt priority */
			if (btcx_inf->dev &&
				btcx_inf->timer_trig_type == BT_DHCP_TIMER_TRIGGER_SCO) {
				wl_cfg80211_bt_setflag(btcx_inf->dev, FALSE);
			}
btc_coex_idle:
			/* Restore BLE Scan Grant */
			if (btcx_inf->dev) {
				wldev_iovar_setint(btcx_inf->dev, "btc_ble_grants", 1);
			}
			wl_cfg80211_btcoex_init_handler_status();
			btcx_inf->timer_on = 0;
			break;
	}

	/* why we need this? */
	net_os_wake_unlock(btcx_inf->dev);
}

void* wl_cfg80211_btcoex_init(struct net_device *ndev)
{
	struct btcoex_info *btco_inf = NULL;

	btco_inf = kmalloc(sizeof(struct btcoex_info), GFP_KERNEL);
	if (!btco_inf)
		return NULL;

	btco_inf->ts_dhcp_start = 0;
	btco_inf->ts_dhcp_ok = 0;
	/* Set up timer for BT  */
	btco_inf->timer_ms = 10;
	init_timer_compat(&btco_inf->timer, wl_cfg80211_bt_timerfunc, btco_inf);
	wl_cfg80211_btcoex_init_handler_status();

	btco_inf->dev = ndev;

	INIT_WORK(&btco_inf->work, wl_cfg80211_bt_handler);

	btcoex_info_loc = btco_inf;
	return btco_inf;
}

void wl_cfg80211_btcoex_kill_handler(void)
{
	if (!btcoex_info_loc)
		return;

	if (btcoex_info_loc->timer_on) {
		btcoex_info_loc->timer_on = 0;
		del_timer_sync(&btcoex_info_loc->timer);
	}
	cancel_work_sync(&btcoex_info_loc->work);
	wl_cfg80211_btcoex_init_handler_status();
}

void wl_cfg80211_btcoex_deinit(void)
{
	if (!btcoex_info_loc)
		return;

	wl_cfg80211_btcoex_kill_handler();
	kfree(btcoex_info_loc);
}

int wl_cfg80211_set_btcoex_dhcp(struct net_device *dev, dhd_pub_t *dhd, char *command)
{

#ifndef OEM_ANDROID
	static int  pm = PM_FAST;
	int  pm_local = PM_OFF;
#endif /* OEM_ANDROID */
	struct btcoex_info *btco_inf = btcoex_info_loc;
	char powermode_val = 0;
	uint8 cmd_len = 0;
	char buf_reg66va_dhcp_on[8] = { 66, 00, 00, 00, 0x10, 0x27, 0x00, 0x00 };
	char buf_reg41va_dhcp_on[8] = { 41, 00, 00, 00, 0x33, 0x00, 0x00, 0x00 };
	char buf_reg68va_dhcp_on[8] = { 68, 00, 00, 00, 0x90, 0x01, 0x00, 0x00 };

	uint32 regaddr;
	static uint32 saved_reg66;
	static uint32 saved_reg41;
	static uint32 saved_reg68;
	static bool saved_status = FALSE;

	char buf_flag7_default[8] =   { 7, 00, 00, 00, 0x0, 0x00, 0x00, 0x00};

	/* Figure out powermode 1 or o command */
#ifdef  OEM_ANDROID
	cmd_len = sizeof(BTCOEXMODE);
#else
	cmd_len = sizeof(POWERMODE);
#endif
	powermode_val = command[cmd_len];

	WL_INFORM_MEM(("BTCOEX MODE: %c\n", powermode_val));
	if (powermode_val == '1') {
		WL_TRACE_HW4(("DHCP session starts\n"));

#if defined(OEM_ANDROID) && defined(DHCP_SCAN_SUPPRESS)
		/* Suppress scan during the DHCP */
		wl_cfg80211_scan_suppress(dev, 1);
#endif /* OEM_ANDROID && DHCP_SCAN_SUPPRESS */

#ifdef PKT_FILTER_SUPPORT
		dhd->dhcp_in_progress = 1;

#if defined(APSTA_BLOCK_ARP_DURING_DHCP)
		if (DHD_OPMODE_STA_SOFTAP_CONCURR(dhd)) {
			/* Block ARP frames while DHCP of STA interface is in
			 * progress in case of STA/SoftAP concurrent mode
			 */
			wl_cfg80211_block_arp(dev, TRUE);
		} else
#endif /* APSTA_BLOCK_ARP_DURING_DHCP */
		if (dhd->early_suspended) {
			WL_TRACE_HW4(("DHCP in progressing , disable packet filter!!!\n"));
			dhd_enable_packet_filter(0, dhd);
		}
#endif /* PKT_FILTER_SUPPORT */

		/* Retrieve and saved orig regs value */
		if ((saved_status == FALSE) &&
#ifndef OEM_ANDROID
			(!dev_wlc_ioctl(dev, WLC_GET_PM, &pm, sizeof(pm))) &&
#endif
			(!dev_wlc_intvar_get_reg(dev, "btc_params", 66,  &saved_reg66)) &&
			(!dev_wlc_intvar_get_reg(dev, "btc_params", 41,  &saved_reg41)) &&
			(!dev_wlc_intvar_get_reg(dev, "btc_params", 68,  &saved_reg68)))   {
				saved_status = TRUE;
				WL_TRACE(("Saved 0x%x 0x%x 0x%x\n",
					saved_reg66, saved_reg41, saved_reg68));

				/* Disable PM mode during dhpc session */
#ifndef OEM_ANDROID
				dev_wlc_ioctl(dev, WLC_SET_PM, &pm_local, sizeof(pm_local));
#endif
				/* Disable BLE Scan Grant during DHCP session */
				wldev_iovar_setint(dev, "btc_ble_grants", 0);
				btco_inf->timer_trig_type = BT_DHCP_TIMER_TRIGGER_NORMAL;

				/* Disable PM mode during dhpc session */
				/* Start  BT timer only for SCO connection */
				if (btcoex_is_sco_active(dev)) {
					/* btc_params 66 */
					dev_wlc_bufvar_set(dev, "btc_params",
						(char *)&buf_reg66va_dhcp_on[0],
						sizeof(buf_reg66va_dhcp_on));
					/* btc_params 41 0x33 */
					dev_wlc_bufvar_set(dev, "btc_params",
						(char *)&buf_reg41va_dhcp_on[0],
						sizeof(buf_reg41va_dhcp_on));
					/* btc_params 68 0x190 */
					dev_wlc_bufvar_set(dev, "btc_params",
						(char *)&buf_reg68va_dhcp_on[0],
						sizeof(buf_reg68va_dhcp_on));

					btco_inf->timer_trig_type = BT_DHCP_TIMER_TRIGGER_SCO;
				}

				btco_inf->bt_state = BT_DHCP_START;
				btco_inf->timer_on = 1;
				mod_timer(&btco_inf->timer, timer_expires(&btco_inf->timer));

				WL_TRACE(("enable BT DHCP Timer\n"));
		}
		else if (saved_status == TRUE) {
			WL_ERR(("was called w/o DHCP OFF. Continue\n"));
		}
	}
#ifdef  OEM_ANDROID
	else if (powermode_val == '2') {
#else
	else if (powermode_val == '0') {
#endif

#if defined(OEM_ANDROID) && defined(DHCP_SCAN_SUPPRESS)
		/* Since DHCP is complete, enable the scan back */
		wl_cfg80211_scan_suppress(dev, 0);
#endif /* OEM_ANDROID */

#ifdef PKT_FILTER_SUPPORT
		dhd->dhcp_in_progress = 0;
		WL_TRACE_HW4(("DHCP is complete \n"));

#if defined(APSTA_BLOCK_ARP_DURING_DHCP)
		if (DHD_OPMODE_STA_SOFTAP_CONCURR(dhd)) {
			/* Unblock ARP frames */
			wl_cfg80211_block_arp(dev, FALSE);
		} else
#endif /* APSTA_BLOCK_ARP_DURING_DHCP */
		if (dhd->early_suspended) {
			/* Enable packet filtering */
			WL_TRACE_HW4(("DHCP is complete , enable packet filter!!!\n"));
			dhd_enable_packet_filter(1, dhd);
		}
#endif /* PKT_FILTER_SUPPORT */

		/* Restoring PM mode */
#ifndef OEM_ANDROID
		dev_wlc_ioctl(dev, WLC_SET_PM, &pm, sizeof(pm));
#endif

		/* Stop any bt timer because DHCP session is done */
		WL_TRACE(("disable BT DHCP Timer\n"));
		if (btco_inf->timer_on) {
			btco_inf->timer_on = 0;
			del_timer_sync(&btco_inf->timer);

			if (btco_inf->bt_state != BT_DHCP_IDLE) {
			/* ANDREY: case when framework signals DHCP end before STM timeout */
			/* need to restore original btc flags & extra btc params */
				WL_TRACE(("bt->bt_state:%d\n", btco_inf->bt_state));
				/* wake up btcoex thread to restore btlags+params  */
				schedule_work(&btco_inf->work);
			}
		}

		if (saved_status == TRUE) {
			/* Restoring btc_flag paramter anyway */
			dev_wlc_bufvar_set(dev, "btc_flags",
				(char *)&buf_flag7_default[0], sizeof(buf_flag7_default));

			/* Restore original values */
			regaddr = 66;
			dev_wlc_intvar_set_reg(dev, "btc_params",
				(char *)&regaddr, (char *)&saved_reg66);
			regaddr = 41;
			dev_wlc_intvar_set_reg(dev, "btc_params",
				(char *)&regaddr, (char *)&saved_reg41);
			regaddr = 68;
			dev_wlc_intvar_set_reg(dev, "btc_params",
				(char *)&regaddr, (char *)&saved_reg68);

			WL_TRACE(("restore regs {66,41,68} <- 0x%x 0x%x 0x%x\n",
				saved_reg66, saved_reg41, saved_reg68));

			/* Enable BLE Scan Grant */
			wldev_iovar_setint(dev, "btc_ble_grants", 1);
			wl_cfg80211_btcoex_init_handler_status();
		}

		saved_status = FALSE;
	}
	else {
		WL_ERR(("Unknown yet power setting, ignored\n"));
	}
	return 0;
}

#ifdef WL_UWB_COEX
const uint16 uwb_6g_chmap[UWB_COEX_CH_MAP_NUM] = {
	1,    5,  9,   13,  17,  21,  25,  29,  33,  37,  41,  45,  49,  53,  57,  61,
	65,  69,  73,  77,  81,  85,  89,  93,  97,  101, 105, 109, 113, 117, 121, 125,
	129, 133, 137, 141, 145, 149, 153, 157, 161, 165, 169, 173, 177, 181, 185, 189,
	193, 197, 201, 205, 209, 213, 217, 221, 225, 229, 233, 0,   0,   0,   0,   0
};

static int
wl_cfg_uwb_coex_get_iovar_status_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	bcm_iov_batch_buf_t *b_resp = (bcm_iov_batch_buf_t *)ctx;
	uint32 status;
	uint16 resp_len;

	/* if all tlvs are parsed, we should not be here */
	if (b_resp->count == 0) {
		return BCME_BADLEN;
	}

	/*  cbfn params may be used in f/w */
	if (len < sizeof(status)) {
		return BCME_BUFTOOSHORT;
	}

	/* first 4 bytes consists status */
	status = dtoh32(*(uint32 *)data);
	resp_len = len - sizeof(status);

	if (status != BCME_OK) {
		WL_ERR(("%s - cmd type %d failed, status: %04x\n",
			__FUNCTION__, type, status));
		return status;
	}
	if (!resp_len) {
		if (b_resp->is_set) {
			/* Set cmd resp may have only status, so len might be zero.
			 * just decrement batch resp count
			 */
			goto counter;
		}
		/* Every response for get command expects some data,
		 * return error if there is no data
		 */
		return BCME_ERROR;
	}
counter:
	if (b_resp->count > 0) {
		b_resp->count--;
	}

	if (!b_resp->count) {
		status = BCME_IOV_LAST_CMD;
	}

	return status;
}

static int
wl_cfg_uwb_coex_proc_resp_buf(bcm_iov_batch_buf_t *resp, uint16 max_len, uint8 is_set)
{
	int ret = BCME_UNSUPPORTED;
	uint16 version;
	uint16 tlvs_len;

	version = dtoh16(*(uint16 *)resp);
	if (version & (BCM_IOV_XTLV_VERSION_0 | BCM_IOV_BATCH_MASK)) {
		if (!resp->count) {
			return BCME_RANGE;
		} else {
			resp->is_set = is_set;
			/* number of tlvs count */
			tlvs_len = max_len - OFFSETOF(bcm_iov_batch_buf_t, cmds[0]);
			/* Extract the tlvs and print their resp in cb fn */
			ret = bcm_unpack_xtlv_buf((void *)resp, (const uint8 *)&resp->cmds[0],
				tlvs_len, BCM_IOV_CMD_OPT_ALIGN32,
				wl_cfg_uwb_coex_get_iovar_status_cbfn);

			if (ret == BCME_IOV_LAST_CMD) {
				ret = BCME_OK;
			}
		}
	}

	return ret;
}

static int
wl_cfg_uwb_coex_execute_cmd(struct net_device *dev, struct bcm_cfg80211 *cfg,
	bcm_iov_batch_buf_t *buf, uint16 buf_sz,
	uint8 *resp_buf, uint16 resp_buf_sz)
{
	int ret = BCME_ERROR;

	char *iov = "uwbcx";
	bcm_iov_batch_buf_t *p_resp = NULL;

	if (buf->is_set) {
		ret = wldev_iovar_setbuf(dev, iov, buf, buf_sz,
			resp_buf, resp_buf_sz, NULL);
		p_resp = (bcm_iov_batch_buf_t *)(resp_buf + strlen(iov) + 1);
		resp_buf_sz -= (strlen(iov) + 1);
	} else {
		ret = wldev_iovar_getbuf(dev, iov, buf, buf_sz,
			resp_buf, resp_buf_sz, NULL);
		p_resp = (bcm_iov_batch_buf_t *)resp_buf;
	}
	if (unlikely(ret)) {
		WL_ERR(("%s - failed to execute uwbcx cmd, err = %d\n", __FUNCTION__, ret));
		goto fail;
	}

	if (ret == BCME_OK && p_resp != NULL) {
		ret = wl_cfg_uwb_coex_proc_resp_buf(p_resp, resp_buf_sz, buf->is_set);
	}
fail:
	return ret;
}

static int
wl_cfg_uwb_coex_fill_ioctl_data(bcm_iov_batch_buf_t *b_buf, const uint8 is_set,
	const uint16 id, void *data, uint16 data_len)
{
	uint16 len;

	bcm_iov_batch_subcmd_t *sub_cmd;

	/* Fill the header */
	b_buf->version = htol16(BCM_IOV_XTLV_VERSION_0 | BCM_IOV_BATCH_MASK);
	b_buf->count = 1;
	b_buf->is_set = is_set;
	len = OFFSETOF(bcm_iov_batch_buf_t, cmds[0]);

	/* Fill the sub command */
	sub_cmd = (bcm_iov_batch_subcmd_t *)(uint8 *)(&b_buf->cmds[0]);
	sub_cmd->id = htod16(id);
	sub_cmd->len = sizeof(sub_cmd->u.options) + data_len;
	sub_cmd->u.options = htol32(BCM_XTLV_OPTION_ALIGN32);
	len += OFFSETOF(bcm_iov_batch_subcmd_t, data);

	if (data) {
		(void)memcpy_s(sub_cmd->data, data_len, (uint8 *)data, data_len);
		len += ALIGN_SIZE(data_len, 4);
	}

	return len;
}

static int
wl_cfg_uwb_coex_get_ch_idx(const int ch)
{
	int i;
	bool found = FALSE;

	if (ch < UWB_COEX_CH_MIN || ch > UWB_COEX_CH_MAX) {
		return BCME_UNSUPPORTED;
	}

	for (i = 0; i < UWB_COEX_CH_MAP_NUM; i++) {
		if (uwb_6g_chmap[i] == ch) {
			found = TRUE;
			break;
		}
	}

	return found ? i : BCME_UNSUPPORTED;
}

static void
wl_cfg_uwb_coex_make_coex_bitmap(int start_ch_idx, int end_ch_idx,
	uwbcx_coex_bitmap_v2_t *coex_bitmap_cfg)
{
	int i;
	uwbcx_coex_bitmap_t *coex_bitmap = (uwbcx_coex_bitmap_t *) (&coex_bitmap_cfg->coex_bitmap);

	for (i = start_ch_idx; i <= end_ch_idx; i++) {
		if (i < 16u) {
			coex_bitmap->low_bitmap |= (1 << i);
		} else if (i >= 16u && i < 32u) {
			coex_bitmap->mid_low_bitmap |= (1 << (i - 16u));
		} else if (i >= 32u && i < 48u) {
			coex_bitmap->mid_high_bitmap |= (1 << (i - 32u));
		} else if (i >= 48u) {
			coex_bitmap->high_bitmap |= (1 << (i - 48u));
		}
	}
}

uint16
wl_cfg_uwb_coex_get_ch_val(const int idx)
{
	return uwb_6g_chmap[idx];
}

int
wl_cfg_uwb_coex_execute_ioctl(struct net_device *dev, struct bcm_cfg80211 *cfg,
	const uint8 is_set, uint16 id, void *data, uint16 data_len,
	uint8 *resp_buf, uint16 resp_buf_sz)
{
	int ret;

	uint8 *buf = NULL;
	bcm_iov_batch_buf_t *b_buf;

	uint16 iov_len;

	buf = (uint8 *)MALLOCZ(cfg->osh, WLC_IOCTL_SMLEN);
	if (unlikely(!buf)) {
		WL_ERR(("%s - Failed to alloc mem\n", __FUNCTION__));
		ret = BCME_NOMEM;
		goto exit;
	}
	b_buf = (bcm_iov_batch_buf_t *)buf;

	if (is_set) {
		iov_len = wl_cfg_uwb_coex_fill_ioctl_data(b_buf, TRUE,
			id, data, data_len);
	} else {
		iov_len = wl_cfg_uwb_coex_fill_ioctl_data(b_buf, FALSE,
			id, NULL, 0);
	}

	ret = wl_cfg_uwb_coex_execute_cmd(dev, cfg, b_buf, iov_len,
		(void *)resp_buf, resp_buf_sz);
	if (unlikely(ret)) {
		WL_ERR(("%s - Failed to execute uwb coex ioctl, ret = %d\n",
			__FUNCTION__, ret));
	}
exit:
	if (buf) {
		MFREE(cfg->osh, buf, WLC_IOCTL_SMLEN);
	}

	return ret;
}

int
wl_cfg_uwb_coex_enable(struct net_device *dev, int enable, int start_ch, int end_ch)
{
	int ret = BCME_OK;

	int start_ch_idx;
	int end_ch_idx;

	uint8 *resp_buf = NULL;

	uwbcx_coex_bitmap_v2_t coex_bitmap_cfg;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);

	resp_buf = (uint8 *)MALLOCZ(cfg->osh, WLC_IOCTL_SMLEN);
	if (unlikely(!resp_buf)) {
		WL_ERR(("%s - Failed to alloc mem\n", __FUNCTION__));
		ret = BCME_NOMEM;
		goto exit;
	}

	bzero(&coex_bitmap_cfg, sizeof(uwbcx_coex_bitmap_v2_t));
	coex_bitmap_cfg.version = UWBCX_COEX_BITMAP_VERSION_V2;
	coex_bitmap_cfg.len = sizeof(coex_bitmap_cfg);
	coex_bitmap_cfg.band = UWBCX_BAND_6G;

	/* Validate UWB Coex channel in case of turnning on */
	if (enable && (((start_ch_idx = wl_cfg_uwb_coex_get_ch_idx(start_ch)) < 0) ||
		((end_ch_idx = wl_cfg_uwb_coex_get_ch_idx(end_ch)) < 0))) {
		WL_ERR(("%s - Unsupported ch.%d, ch.%d\n", __FUNCTION__, start_ch, end_ch));
		ret = BCME_UNSUPPORTED;
		goto exit;
	}

	if (enable) {
		wl_cfg_uwb_coex_make_coex_bitmap(start_ch_idx, end_ch_idx, &coex_bitmap_cfg);
	}

	ret = wl_cfg_uwb_coex_execute_ioctl(dev, cfg, TRUE, WL_UWBCX_CMD_COEX_BITMAP,
		&coex_bitmap_cfg, (uint16)sizeof(coex_bitmap_cfg),
		resp_buf, WLC_IOCTL_SMLEN);
	WL_ERR(("%s - UWB Coex %s %s - Ch. [%d/%d] (ret = %d)\n", __FUNCTION__,
	       enable ? "On" : "Off", !ret ? "Success" : "Fail",
	       start_ch, end_ch, ret));
exit:
	if (resp_buf) {
		MFREE(cfg->osh, resp_buf, WLC_IOCTL_SMLEN);
	}

	return ret;
}
#endif /* WL_UWB_COEX */
#endif /* defined(OEM_ANDROID) */
