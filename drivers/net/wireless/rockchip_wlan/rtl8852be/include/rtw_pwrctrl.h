/******************************************************************************
 *
 * Copyright(c) 2007 - 2019 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/
#ifndef __RTW_PWRCTRL_H_
#define __RTW_PWRCTRL_H_

#define PS_ACTIVE	0
#define PS_LPS		1

#define FW_PWR0	0
#define FW_PWR1	1
#define FW_PWR2	2
#define FW_PWR3	3


#define HW_PWR0	7
#define HW_PWR1	6
#define HW_PWR2	2
#define HW_PWR3	0
#define HW_PWR4	8

#define FW_PWRMSK	0x7


#define XMIT_ALIVE	BIT(0)
#define RECV_ALIVE	BIT(1)
#define CMD_ALIVE	BIT(2)
#define EVT_ALIVE	BIT(3)

enum power_mgnt {
	PM_PS_MODE_ACTIVE	= 0	,
	PM_PS_MODE_MIN			,
	PM_PS_MODE_MAX			,
	PM_PS_MODE_DTIM			,	/* PS_MODE_SELF_DEFINED */
	PM_PS_MODE_VOIP			,
	PM_PS_MODE_UAPSD_WMM	,
	PM_PS_MODE_UAPSD			,
	PM_PS_MODE_IBSS			,
	PM_PS_MODE_WWLAN		,
	PM_RADIO_OFF			,
	PM_CARD_DISABLE		,
	PM_PS_MODE_NUM,
};

enum lps_level {
	LPS_NORMAL = 0,
	LPS_LCLK,
	LPS_PG,
	LPS_LEVEL_MAX,
};


/*
	BIT[2:0] = HW state
	BIT[3] = Protocol PS state,   0: register active state , 1: register sleep state
	BIT[4] = sub-state
*/

#define PS_DPS				BIT(0)
#define PS_LCLK				(PS_DPS)
#define PS_RF_OFF			BIT(1)
#define PS_ALL_ON			BIT(2)
#define PS_ST_ACTIVE		BIT(3)

#define PS_ISR_ENABLE		BIT(4)
#define PS_IMR_ENABLE		BIT(5)
#define PS_ACK				BIT(6)
#define PS_TOGGLE			BIT(7)

#define PS_STATE_MASK		(0x0F)
#define PS_STATE_HW_MASK	(0x07)
#define PS_SEQ_MASK			(0xc0)

#define PS_STATE(x)		(PS_STATE_MASK & (x))
#define PS_STATE_HW(x)	(PS_STATE_HW_MASK & (x))
#define PS_SEQ(x)		(PS_SEQ_MASK & (x))

#define PS_STATE_S0		(PS_DPS)
#define PS_STATE_S1		(PS_LCLK)
#define PS_STATE_S2		(PS_RF_OFF)
#define PS_STATE_S3		(PS_ALL_ON)
#define PS_STATE_S4		((PS_ST_ACTIVE) | (PS_ALL_ON))


#define PS_IS_RF_ON(x)	((x) & (PS_ALL_ON))
#define PS_IS_ACTIVE(x)	((x) & (PS_ST_ACTIVE))
#define CLR_PS_STATE(x)	((x) = ((x) & (0xF0)))


struct reportpwrstate_parm {
	unsigned char mode;
	unsigned char state; /* the CPWM value */
	unsigned short rsvd;
};


typedef _sema _pwrlock;


__inline static void _init_pwrlock(_pwrlock *plock)
{
	_rtw_init_sema(plock, 1);
}

__inline static void _free_pwrlock(_pwrlock *plock)
{
	_rtw_free_sema(plock);
}


__inline static void _enter_pwrlock(_pwrlock *plock)
{
	_rtw_down_sema(plock);
}


__inline static void _exit_pwrlock(_pwrlock *plock)
{
	_rtw_up_sema(plock);
}

#define LPS_DELAY_MS	1000 /* 1 sec */

#define EXE_PWR_NONE	0x01
#define EXE_PWR_IPS		0x02
#define EXE_PWR_LPS		0x04

/* RF state. */
typedef enum _rt_rf_power_state {
	rf_on,		/* RF is on after RFSleep or RFOff */
	rf_sleep,	/* 802.11 Power Save mode */
	rf_off,		/* HW/SW Radio OFF or Inactive Power Save */
	/* =====Add the new RF state above this line===== */
	rf_max
} rt_rf_power_state;

/* ASPM OSC Control bit, added by Roger, 2013.03.29. */
#define	RT_PCI_ASPM_OSC_IGNORE		0	 /* PCI ASPM ignore OSC control in default */
#define	RT_PCI_ASPM_OSC_ENABLE		BIT0 /* PCI ASPM controlled by OS according to ACPI Spec 5.0 */
#define	RT_PCI_ASPM_OSC_DISABLE		BIT1 /* PCI ASPM controlled by driver or BIOS, i.e., force enable ASPM */


enum _PS_BBRegBackup_ {
	PSBBREG_RF0 = 0,
	PSBBREG_RF1,
	PSBBREG_RF2,
	PSBBREG_AFE0,
	PSBBREG_TOTALCNT
};

enum { /* for ips_mode */
	IPS_NONE = 0,
	IPS_NORMAL,
	IPS_LEVEL_2,
	IPS_NUM
};

/* Design for pwrctrl_priv.ips_deny, 32 bits for 32 reasons at most */
typedef enum _PS_DENY_REASON {
	PS_DENY_DRV_INITIAL = 0,
	PS_DENY_SCAN,
	PS_DENY_JOIN,
	PS_DENY_DISCONNECT,
	PS_DENY_SUSPEND,
	PS_DENY_IOCTL,
	PS_DENY_MGNT_TX,
	PS_DENY_MONITOR_MODE,
	PS_DENY_BEAMFORMING,		/* Beamforming */
	PS_DENY_DRV_REMOVE = 30,
	PS_DENY_OTHERS = 31
} PS_DENY_REASON;

struct rsvd_page_cache_t;

struct pwrctrl_priv {
	_pwrlock	lock;
	_pwrlock	check_32k_lock;
	volatile u8 rpwm; /* requested power state for fw */
	volatile u8 cpwm; /* fw current power state. updated when 1. read from HCPWM 2. driver lowers power level */
	volatile u8 tog; /* toggling */
	volatile u8 cpwm_tog; /* toggling */
	u8 rpwm_retry;

	u8	pwr_mode;
	u8	smart_ps;
	u8	bcn_ant_mode;
	u8	dtim;
#ifdef CONFIG_LPS_CHK_BY_TP
	u8	lps_chk_by_tp;
	u16	lps_tx_tp_th;/*Mbps*/
	u16	lps_rx_tp_th;/*Mbps*/
	u16	lps_bi_tp_th;/*Mbps*//*TRX TP*/
	int	lps_chk_cnt_th;
	int	lps_chk_cnt;
	u32	lps_tx_pkts;
	u32	lps_rx_pkts;

#endif

#ifdef CONFIG_WMMPS_STA
	u8 wmm_smart_ps;
#endif /* CONFIG_WMMPS_STA */	

	u32	alives;
	_workitem cpwm_event;
	_workitem dma_event; /*for handle un-synchronized tx dma*/
#ifdef CONFIG_LPS_RPWM_TIMER
	u8 brpwmtimeout;
	_workitem rpwmtimeoutwi;
	_timer pwr_rpwm_timer;
#endif /* CONFIG_LPS_RPWM_TIMER */
	u8	bpower_saving; /* for LPS/IPS */

	u8	b_hw_radio_off;
	u8	reg_rfoff;
	u8	reg_pdnmode; /* powerdown mode */
	u32	rfoff_reason;

	uint	ips_enter_cnts;
	uint	ips_leave_cnts;
	uint	lps_enter_cnts;
	uint	lps_leave_cnts;

	u8	ips_mode;
	u8	ips_org_mode;
	u8	ips_mode_req; /* used to accept the mode setting request, will update to ipsmode later */
	uint bips_processing;
	systime ips_deny_time; /* will deny IPS when system time is smaller than this */
	u8 pre_ips_type;/* 0: default flow, 1: carddisbale flow */

	/* ps_deny: if 0, power save is free to go; otherwise deny all kinds of power save. */
	/* Use PS_DENY_REASON to decide reason. */
	/* Don't access this variable directly without control function, */
	/* and this variable should be protected by lock. */
	u32 ps_deny;

	u8 ps_processing; /* temporarily used to mark whether in rtw_ps_processor */

	u8 fw_psmode_iface_id;
	u8	bLeisurePs;
	u8	LpsIdleCount;
	u8	power_mgnt;
	u8	org_power_mgnt;
	u8	bFwCurrentInPSMode;
	systime	lps_deny_time; /* will deny LPS when system time is smaller than this */
	s32		pnp_current_pwr_state;
	u8		pnp_bstop_trx;
	u8		bInSuspend;
#ifdef CONFIG_BTC
	u8		bAutoResume;
	u8		autopm_cnt;
#endif
	u8		bSupportRemoteWakeup;
	u8		wowlan_wake_reason;
	u8		wowlan_last_wake_reason;
	u8		wowlan_ap_mode;
	u8		wowlan_mode;
	u8		wowlan_p2p_mode;
	u8		wowlan_pno_enable;
	u8		wowlan_in_resume;

#ifdef CONFIG_GPIO_WAKEUP
#endif /* CONFIG_GPIO_WAKEUP */
	u8		hst2dev_high_active;
#ifdef CONFIG_WOWLAN
#ifdef CONFIG_IPV6
	u8		wowlan_ns_offload_en;
#endif /*CONFIG_IPV6*/
	u8		wowlan_txpause_status;
	u8		wowlan_pattern_idx;
	u64		wowlan_fw_iv;
	struct rtl_priv_pattern	patterns[MAX_WKFM_CAM_NUM];
	_mutex	wowlan_pattern_cam_mutex;
	u8		wowlan_aoac_rpt_loc;
	struct aoac_report wowlan_aoac_rpt;
	u8		wowlan_power_mgmt;
	u8		wowlan_lps_level;
	#ifdef CONFIG_LPS_1T1R
	u8		wowlan_lps_1t1r;
	#endif
#endif /* CONFIG_WOWLAN */
	_timer	pwr_state_check_timer;
	int		pwr_state_check_interval;
	u8		pwr_state_check_cnts;


	rt_rf_power_state	rf_pwrstate;/* cur power state, only for IPS */
	/* rt_rf_power_state	current_rfpwrstate; */
	rt_rf_power_state	change_rfpwrstate;
	u8		bkeepfwalive;
	u8		brfoffbyhw;
	unsigned long PS_BBRegBackup[PSBBREG_TOTALCNT];

#ifdef CONFIG_RESUME_IN_WORKQUEUE
	struct workqueue_struct *rtw_workqueue;
	_workitem resume_work;
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
	u8 do_late_resume;
#endif /* CONFIG_HAS_EARLYSUSPEND */

#ifdef CONFIG_ANDROID_POWER
	android_early_suspend_t early_suspend;
	u8 do_late_resume;
#endif

	u8 lps_level_bk;
	u8 lps_level; /*LPS_NORMAL,LPA_CG,LPS_PG*/
#ifdef CONFIG_LPS_1T1R
	u8 lps_1t1r_bk;
	u8 lps_1t1r;
#endif
#ifdef CONFIG_LPS_PG
	struct rsvd_page_cache_t lpspg_info;
#ifdef CONFIG_RTL8822C
	struct rsvd_page_cache_t lpspg_dpk_info;
	struct rsvd_page_cache_t lpspg_iqk_info;
#endif
#endif
	u8 current_lps_hw_port_id;

#ifdef CONFIG_RTW_CFGVENDOR_LLSTATS
	systime radio_on_start_time;
	systime pwr_saving_start_time;
	u32 pwr_saving_time;
	u32 on_time;
	u32 tx_time;
	u32 rx_time;
#endif /* CONFIG_RTW_CFGVENDOR_LLSTATS */

#ifdef CONFIG_LPS_ACK
	struct submit_ctx lps_ack_sctx;
	s8 lps_ack_status;
	_mutex lps_ack_mutex;
#endif /* CONFIG_LPS_ACK */
};

#define rtw_get_ips_mode_req(pwrctl) \
	(pwrctl)->ips_mode_req

#define rtw_ips_mode_req(pwrctl, ips_mode) \
	(pwrctl)->ips_mode_req = (ips_mode)

#define RTW_PWR_STATE_CHK_INTERVAL 2000
#ifdef CONFIG_RTW_IPS
bool rtw_core_set_ips_state(void *drv_priv, enum rtw_rf_state state);
#endif
#ifdef CONFIG_POWER_SAVING
#define _rtw_set_pwr_state_check_timer(pwrctl, ms) \
	do { \
		/*RTW_INFO("%s _rtw_set_pwr_state_check_timer(%p, %d)\n", __FUNCTION__, (pwrctl), (ms));*/ \
		_set_timer(&(pwrctl)->pwr_state_check_timer, (ms)); \
	} while (0)

#define rtw_set_pwr_state_check_timer(pwrctl) \
	_rtw_set_pwr_state_check_timer((pwrctl), (pwrctl)->pwr_state_check_interval)
#endif
extern void rtw_init_pwrctrl_priv(_adapter *adapter);
extern void rtw_free_pwrctrl_priv(_adapter *adapter);

#ifdef CONFIG_LPS_LCLK
s32 rtw_register_task_alive(_adapter *, u32 task);
void rtw_unregister_task_alive(_adapter *, u32 task);
extern s32 rtw_register_tx_alive(_adapter *padapter);
extern void rtw_unregister_tx_alive(_adapter *padapter);
extern s32 rtw_register_rx_alive(_adapter *padapter);
extern void rtw_unregister_rx_alive(_adapter *padapter);

#if 0 /*#ifdef CONFIG_CORE_CMD_THREAD*/
extern s32 rtw_register_cmd_alive(_adapter *padapter);
extern void rtw_unregister_cmd_alive(_adapter *padapter);
#endif

extern void cpwm_int_hdl(_adapter *padapter, struct reportpwrstate_parm *preportpwrstate);
extern void LPS_Leave_check(_adapter *padapter);
#endif

extern void LeaveAllPowerSaveMode(_adapter *adapter);
extern void LeaveAllPowerSaveModeDirect(_adapter *adapter);
#ifdef CONFIG_IPS
void _ips_enter(_adapter *padapter);
void ips_enter(_adapter *padapter);
int _ips_leave(_adapter *padapter);
int ips_leave(_adapter *padapter);
#endif

#ifdef CONFIG_POWER_SAVING
void rtw_ps_processor(_adapter *padapter);
#endif

#ifdef DBG_CHECK_FW_PS_STATE
int rtw_fw_ps_state(_adapter *padapter);
#endif

#ifdef CONFIG_LPS
extern const char * const LPS_CTRL_PHYDM;
void LPS_Enter(_adapter *padapter, const char *msg);
void LPS_Leave(_adapter *padapter, const char *msg);
void rtw_leave_lps_and_chk(_adapter *padapter, u8 ps_mode);
#ifdef CONFIG_CHECK_LEAVE_LPS
#ifdef CONFIG_LPS_CHK_BY_TP
void traffic_check_for_leave_lps_by_tp(_adapter *padapter, u8 tx, struct sta_info *sta);
#endif
void traffic_check_for_leave_lps(_adapter *padapter, u8 tx, u32 tx_packets);
#endif /*CONFIG_CHECK_LEAVE_LPS*/
void rtw_set_ps_mode(_adapter *padapter, u8 ps_mode, u8 smart_ps, u8 bcn_ant_mode, const char *msg);

u8 rtw_set_rpwm(_adapter *padapter, u8 val8);
#endif /* CONFIG_LPS */

#ifdef CONFIG_RESUME_IN_WORKQUEUE
void rtw_resume_in_workqueue(struct pwrctrl_priv *pwrpriv);
#endif /* CONFIG_RESUME_IN_WORKQUEUE */

#if defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_ANDROID_POWER)
bool rtw_is_earlysuspend_registered(struct pwrctrl_priv *pwrpriv);
bool rtw_is_do_late_resume(struct pwrctrl_priv *pwrpriv);
void rtw_set_do_late_resume(struct pwrctrl_priv *pwrpriv, bool enable);
void rtw_register_early_suspend(struct pwrctrl_priv *pwrpriv);
void rtw_unregister_early_suspend(struct pwrctrl_priv *pwrpriv);
#else
#define rtw_is_earlysuspend_registered(pwrpriv) _FALSE
#define rtw_is_do_late_resume(pwrpriv) _FALSE
#define rtw_set_do_late_resume(pwrpriv, enable) do {} while (0)
#define rtw_register_early_suspend(pwrpriv) do {} while (0)
#define rtw_unregister_early_suspend(pwrpriv) do {} while (0)
#endif /* CONFIG_HAS_EARLYSUSPEND || CONFIG_ANDROID_POWER */

void rtw_set_ips_deny(_adapter *padapter, u32 ms);
int _rtw_pwr_wakeup(_adapter *padapter, u32 ips_deffer_ms, const char *caller);
#define rtw_pwr_wakeup(adapter) _rtw_pwr_wakeup(adapter, RTW_PWR_STATE_CHK_INTERVAL, __FUNCTION__)
#define rtw_pwr_wakeup_ex(adapter, ips_deffer_ms) _rtw_pwr_wakeup(adapter, ips_deffer_ms, __FUNCTION__)
int rtw_pm_set_ips(_adapter *padapter, u8 mode);
int rtw_pm_set_lps(_adapter *padapter, u8 mode);
int rtw_pm_set_lps_level(_adapter *padapter, u8 level);
#ifdef CONFIG_LPS_1T1R
int rtw_pm_set_lps_1t1r(_adapter *padapter, u8 en);
#endif
void rtw_set_lps_deny(_adapter *adapter, u32 ms);

void rtw_ps_deny(_adapter *padapter, PS_DENY_REASON reason);
void rtw_ps_deny_cancel(_adapter *padapter, PS_DENY_REASON reason);
u32 rtw_ps_deny_get(_adapter *padapter);

void rtw_ssmps_enter(_adapter *adapter, struct sta_info *sta);
void rtw_ssmps_leave(_adapter *adapter, struct sta_info *sta);
#endif /* __RTL871X_PWRCTRL_H_ */
