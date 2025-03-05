
#ifndef _wl_iapsta_
#define _wl_iapsta_
typedef enum IFMODE {
	ISTA_MODE = 1,
	IAP_MODE,
	IGO_MODE,
	IGC_MODE,
	IMESH_MODE
} ifmode_t;

enum wl_ext_status {
	WL_EXT_STATUS_PRE_DISCONNECTING = 0,
	WL_EXT_STATUS_DISCONNECTING = 1,
	WL_EXT_STATUS_DISCONNECTED = 2,
	WL_EXT_STATUS_SCAN = 3,
	WL_EXT_STATUS_SCANNING = 4,
	WL_EXT_STATUS_SCAN_COMPLETE = 5,
	WL_EXT_STATUS_CONNECTING = 6,
	WL_EXT_STATUS_RECONNECT = 7,
	WL_EXT_STATUS_CONNECTED = 8,
	WL_EXT_STATUS_ROAMED = 9,
	WL_EXT_STATUS_ADD_KEY = 10,
	WL_EXT_STATUS_AP_ENABLING = 11,
	WL_EXT_STATUS_AP_ENABLED = 12,
	WL_EXT_STATUS_DELETE_STA = 13,
	WL_EXT_STATUS_STA_DISCONNECTED = 14,
	WL_EXT_STATUS_STA_CONNECTED = 15,
	WL_EXT_STATUS_AP_DISABLING = 16,
	WL_EXT_STATUS_AP_DISABLED = 17
};

extern int op_mode;
void wl_ext_update_conn_state(dhd_pub_t *dhd, int ifidx, uint conn_state);
#ifdef EAPOL_RESEND
void wl_ext_backup_eapol_txpkt(dhd_pub_t *dhd, int ifidx, void *pkt);
void wl_ext_release_eapol_txpkt(dhd_pub_t *dhd, int ifidx, bool rx);
#endif /* EAPOL_RESEND */
void wl_ext_iapsta_get_vif_macaddr(struct dhd_pub *dhd, int ifidx, u8 *mac_addr);
#ifdef WLDWDS
int wl_ext_iapsta_attach_dwds_netdev(struct net_device *net, int ifidx, uint8 bssidx);
int wl_ext_iapsta_dettach_dwds_netdev(struct net_device *net, int ifidx, uint8 bssidx);
#endif /* WLDWDS */
int wl_ext_iapsta_attach_netdev(struct net_device *net, int ifidx, uint8 bssidx);
int wl_ext_iapsta_attach_name(struct net_device *net, int ifidx);
int wl_ext_iapsta_dettach_netdev(struct net_device *net, int ifidx);
int wl_ext_iapsta_update_net_device(struct net_device *net, int ifidx);
int wl_ext_iapsta_alive_preinit(struct net_device *dev);
int wl_ext_iapsta_alive_postinit(struct net_device *dev);
int wl_ext_iapsta_attach(struct net_device *net);
void wl_ext_iapsta_dettach(struct net_device *net);
int wl_ext_iapsta_enable(struct net_device *dev, char *command, int total_len);
int wl_ext_iapsta_disable(struct net_device *dev, char *command, int total_len);
int wl_ext_isam_param(struct net_device *dev, char *command, int total_len);
int wl_ext_isam_status(struct net_device *dev, char *command, int total_len);
int wl_ext_isam_init(struct net_device *dev, char *command, int total_len);
int wl_ext_iapsta_config(struct net_device *dev, char *command, int total_len);
void wl_ext_add_remove_pm_enable_work(struct net_device *dev, bool add);
bool wl_ext_iapsta_other_if_enabled(struct net_device *net);
bool wl_ext_sta_connecting(struct net_device *dev);
bool wl_ext_sta_connected(struct net_device *dev);
void wl_ext_get_chan_str(struct net_device *dev, char *chan_str, int total_len);
#ifdef DHD_LOSSLESS_ROAMING
int wl_ext_any_sta_handshaking(struct dhd_pub *dhd);
#endif /* DHD_LOSSLESS_ROAMING */
void wl_iapsta_wait_event_complete(struct dhd_pub *dhd);
int wl_iapsta_suspend_resume(dhd_pub_t *dhd, int suspend);
#ifdef USE_IW
int wl_ext_in4way_sync_wext(struct net_device *dev, uint action,
	enum wl_ext_status status, void *context);
#endif /* USE_IW */
#ifdef WLMESH
int wl_ext_mesh_peer_status(struct net_device *dev, char *data, char *command,
	int total_len);
int wl_ext_isam_peer_path(struct net_device *dev, char *command, int total_len);
#endif
#ifdef WL_CFG80211
int wl_ext_in4way_sync(struct net_device *dev, uint action,
	enum wl_ext_status status, void *context);
void wl_ext_update_extsae_4way(struct net_device *dev,
	const struct ieee80211_mgmt *mgmt, bool tx);
u32 wl_ext_iapsta_update_channel(struct net_device *dev, u32 channel);
void wl_ext_iapsta_update_iftype(struct net_device *net, int wl_iftype);
bool wl_ext_iapsta_iftype_enabled(struct net_device *net, int wl_iftype);
void wl_ext_iapsta_enable_master_if(struct net_device *dev, bool post);
void wl_ext_iapsta_restart_master(struct net_device *dev);
void wl_ext_iapsta_ifadding(struct net_device *net, int ifidx);
bool wl_ext_iapsta_mesh_creating(struct net_device *net);
void wl_ext_fw_reinit_incsa(struct net_device *dev);
void wl_ext_send_event_msg(struct net_device *dev, int event, int status,
	int reason);
#ifdef BTC_WAR
void wl_ext_btc_config(struct net_device *dev, bool enable);
#endif /* BTC_WAR */
#ifdef STA_MGMT
bool wl_ext_del_sta_info(struct net_device *net, u8 *bssid);
bool wl_ext_add_sta_info(struct net_device *net, u8 *bssid);
#endif /* STA_MGMT */
#ifdef SCAN_SUPPRESS
uint16 wl_ext_scan_suppress(struct net_device *dev, void *scan_params, bool scan_v2,
	struct wl_chan_info *chan_info);
void wl_ext_reset_scan_busy(dhd_pub_t *dhd);
#endif /* SCAN_SUPPRESS */
#endif
#ifdef PROPTX_MAXCOUNT
int wl_ext_get_wlfc_maxcount(struct dhd_pub *dhd, int ifidx);
#endif /* PROPTX_MAXCOUNT */
#ifdef TPUT_MONITOR
int32 wl_ext_tput_get(struct dhd_pub *dhd);
#endif /* TPUT_MONITOR */
#endif
