/*
 * log_dump - debugability support for dumping logs to file - header file
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

#ifndef __DHD_LOG_DUMP_H__
#define __DHD_LOG_DUMP_H__

#ifdef DHD_LOG_DUMP

#include <dhd.h>

#define LOG_DUMP_MAGIC 0xDEB3DEB3
#ifdef EWP_ECNTRS_LOGGING
#define ECNTR_RING_ID 0xECDB
#define	ECNTR_RING_NAME	"ewp_ecntr_ring"
#endif /* EWP_ECNTRS_LOGGING */

#ifdef EWP_RTT_LOGGING
#define	RTT_RING_ID 0xADCD
#define	RTT_RING_NAME	"ewp_rtt_ring"
#endif /* EWP_ECNTRS_LOGGING */

#ifdef EWP_BCM_TRACE
#define	BCM_TRACE_RING_ID 0xBCBC
#define	BCM_TRACE_RING_NAME "ewp_bcm_trace_ring"
#endif /* EWP_BCM_TRACE */

#define DHD_LOG_DUMP_MAX_TEMP_BUFFER_SIZE	512
#define DHD_LOG_DUMP_MAX_TAIL_FLUSH_SIZE (80 * 1024)
#define DHD_LOG_DUMP_TS_MULTIPLIER_VALUE    60
#define DHD_LOG_DUMP_TS_FMT_YYMMDDHHMMSSMSMS    "%02d%02d%02d%02d%02d%02d%04lu"
#define DHD_LOG_DUMP_TS_FMT_YYMMDDHHMMSS        "%02d%02d%02d%02d%02d%02d"
#define DHD_DEBUG_DUMP_TYPE		"debug_dump"
#define DEBUG_DUMP_TRIGGER_INTERVAL_SEC	4

#ifndef _DHD_LOG_DUMP_DEFINITIONS_
#define _DHD_LOG_DUMP_DEFINITIONS_
#define GENERAL_LOG_HDR "\n-------------------- General log ---------------------------\n"
#define PRESERVE_LOG_HDR "\n-------------------- Preserve log ---------------------------\n"
#define SPECIAL_LOG_HDR "\n-------------------- Special log ---------------------------\n"
#define DHD_DUMP_LOG_HDR "\n-------------------- 'dhd dump' log -----------------------\n"
#define EXT_TRAP_LOG_HDR "\n-------------------- Extended trap data -------------------\n"
#define HEALTH_CHK_LOG_HDR "\n-------------------- Health check data --------------------\n"
#ifdef DHD_DUMP_PCIE_RINGS
#define RING_DUMP_HDR "\n-------------------- Ring dump --------------------\n"
#endif /* DHD_DUMP_PCIE_RINGS */
#define DHD_LOG_DUMP_DLD(fmt, ...) \
	dhd_log_dump_write(DLD_BUF_TYPE_GENERAL, NULL, 0, fmt, ##__VA_ARGS__)
#define DHD_LOG_DUMP_DLD_EX(fmt, ...) \
	dhd_log_dump_write(DLD_BUF_TYPE_SPECIAL, NULL, 0, fmt, ##__VA_ARGS__)
#define DHD_LOG_DUMP_DLD_PRSRV(fmt, ...) \
	dhd_log_dump_write(DLD_BUF_TYPE_PRESERVE, NULL, 0, fmt, ##__VA_ARGS__)
#endif /* !_DHD_LOG_DUMP_DEFINITIONS_ */

#ifndef DHD_LOG_DUMP_RING_DEFINITIONS
#define DHD_LOG_DUMP_RING_DEFINITIONS
#ifdef DHD_DEBUGABILITY_LOG_DUMP_RING
/* Enabled DHD_DEBUGABILITY_LOG_DUMP_RING */
#define DHD_DBG_RING(fmt, ...) \
	dhd_dbg_ring_write(DRIVER_LOG_RING_ID, NULL, 0, fmt, ##__VA_ARGS__)
#define DHD_DBG_RING_EX(fmt, ...) \
	dhd_dbg_ring_write(FW_VERBOSE_RING_ID, NULL, 0, fmt, ##__VA_ARGS__)
#define DHD_DBG_RING_ROAM(fmt, ...) \
	dhd_dbg_ring_write(ROAM_STATS_RING_ID, NULL, 0, fmt, ##__VA_ARGS__)

#define DHD_LOG_DUMP_WRITE		DHD_DBG_RING
#define DHD_LOG_DUMP_WRITE_EX		DHD_DBG_RING_EX
#define DHD_LOG_DUMP_WRITE_PRSRV	DHD_DBG_RING_EX
#define DHD_LOG_DUMP_WRITE_ROAM		DHD_DBG_RING_ROAM

#define DHD_PREFIX_TS "[%s][%s] ",	\
	OSL_GET_RTCTIME(), dhd_log_dump_get_timestamp()
#define DHD_PREFIX_TS_FN "[%s][%s] %s: ",	\
	OSL_GET_RTCTIME(), dhd_log_dump_get_timestamp(), __func__
#define DHD_LOG_DUMP_WRITE_TS		DHD_DBG_RING(DHD_PREFIX_TS)
#define DHD_LOG_DUMP_WRITE_TS_FN	DHD_DBG_RING(DHD_PREFIX_TS_FN)
#define DHD_LOG_DUMP_WRITE_EX_TS	DHD_DBG_RING_EX(DHD_PREFIX_TS)
#define DHD_LOG_DUMP_WRITE_EX_TS_FN	DHD_DBG_RING_EX(DHD_PREFIX_TS_FN)
#define DHD_LOG_DUMP_WRITE_PRSRV_TS	DHD_DBG_RING_EX(DHD_PREFIX_TS)
#define DHD_LOG_DUMP_WRITE_PRSRV_TS_FN	DHD_DBG_RING_EX(DHD_PREFIX_TS_FN)
#define DHD_LOG_DUMP_WRITE_ROAM_TS	DHD_DBG_RING_ROAM(DHD_PREFIX_TS)
#define DHD_LOG_DUMP_WRITE_ROAM_TS_FN	DHD_DBG_RING_ROAM(DHD_PREFIX_TS_FN)
#else
/* Not enabled DHD_DEBUGABILITY_LOG_DUMP_RING */
#define DHD_LOG_DUMP_WRITE             DHD_LOG_DUMP_DLD
#define DHD_LOG_DUMP_WRITE_EX          DHD_LOG_DUMP_DLD_EX
#define DHD_LOG_DUMP_WRITE_PRSRV       DHD_LOG_DUMP_DLD_PRSRV
#define DHD_LOG_DUMP_WRITE_ROAM                DHD_LOG_DUMP_DLD

#define DHD_PREFIX_TS "[%s]: ", dhd_log_dump_get_timestamp()
#define DHD_PREFIX_TS_FN "[%s] %s: ", dhd_log_dump_get_timestamp(), __func__
#define DHD_LOG_DUMP_WRITE_TS          DHD_LOG_DUMP_DLD(DHD_PREFIX_TS)
#define DHD_LOG_DUMP_WRITE_TS_FN       DHD_LOG_DUMP_DLD(DHD_PREFIX_TS_FN)
#define DHD_LOG_DUMP_WRITE_EX_TS       DHD_LOG_DUMP_DLD_EX(DHD_PREFIX_TS)
#define DHD_LOG_DUMP_WRITE_EX_TS_FN    DHD_LOG_DUMP_DLD_EX(DHD_PREFIX_TS_FN)
#define DHD_LOG_DUMP_WRITE_PRSRV_TS    DHD_LOG_DUMP_DLD_PRSRV(DHD_PREFIX_TS)
#define DHD_LOG_DUMP_WRITE_PRSRV_TS_FN DHD_LOG_DUMP_DLD_PRSRV(DHD_PREFIX_TS_FN)
#define DHD_LOG_DUMP_WRITE_ROAM_TS     DHD_LOG_DUMP_DLD(DHD_PREFIX_TS)
#define DHD_LOG_DUMP_WRITE_ROAM_TS_FN  DHD_LOG_DUMP_DLD(DHD_PREFIX_TS_FN)
#endif /* DHD_DEBUGABILITY_LOG_DUMP_RING */
#endif /* DHD_LOG_DUMP_RING_DEFINITIONS */

#define CONCISE_DUMP_BUFLEN 32 * 1024
#define ECNTRS_LOG_HDR "\n-------------------- Ecounters log --------------------------\n"
#ifdef DHD_STATUS_LOGGING
#define STATUS_LOG_HDR "\n-------------------- Status log -----------------------\n"
#endif /* DHD_STATUS_LOGGING */
#define RTT_LOG_HDR "\n-------------------- RTT log --------------------------\n"
#define BCM_TRACE_LOG_HDR "\n-------------------- BCM Trace log --------------------------\n"
#define COOKIE_LOG_HDR "\n-------------------- Cookie List ----------------------------\n"
#define DHD_PKTID_MAP_LOG_HDR "\n---------------- PKTID MAP log -----------------------\n"
#define DHD_PKTID_UNMAP_LOG_HDR "\n------------------ PKTID UNMAP log -----------------------\n"
#define PKTID_LOG_DUMP_FMT \
	"\nIndex \t\tTimestamp \tPktaddr(PA) \tPktid \tSize \tPkttype\n(Current=%d)\n"

/* 0: DLD_BUF_TYPE_GENERAL, 1: DLD_BUF_TYPE_PRESERVE
* 2: DLD_BUF_TYPE_SPECIAL
*/
#define DLD_BUFFER_NUM 3

#ifndef CUSTOM_LOG_DUMP_BUFSIZE_MB
#define CUSTOM_LOG_DUMP_BUFSIZE_MB	4 /* DHD_LOG_DUMP_BUF_SIZE 4 MB static memory in kernel */
#endif /* CUSTOM_LOG_DUMP_BUFSIZE_MB */

#define LOG_DUMP_TOTAL_BUFSIZE (1024 * 1024 * CUSTOM_LOG_DUMP_BUFSIZE_MB)

/*
 * Below are different sections that use the prealloced buffer
 * and sum of the sizes of these should not cross LOG_DUMP_TOTAL_BUFSIZE
 */
#ifdef EWP_BCM_TRACE
#define LOG_DUMP_GENERAL_MAX_BUFSIZE (192 * 1024 * CUSTOM_LOG_DUMP_BUFSIZE_MB)
#define LOG_DUMP_BCM_TRACE_MAX_BUFSIZE (64 * 1024 * CUSTOM_LOG_DUMP_BUFSIZE_MB)
#else
#define LOG_DUMP_GENERAL_MAX_BUFSIZE (256 * 1024 * CUSTOM_LOG_DUMP_BUFSIZE_MB)
#define LOG_DUMP_BCM_TRACE_MAX_BUFSIZE 0
#endif /* EWP_BCM_TRACE */
#define LOG_DUMP_PRESERVE_MAX_BUFSIZE (128 * 1024 * CUSTOM_LOG_DUMP_BUFSIZE_MB)
#define LOG_DUMP_ECNTRS_MAX_BUFSIZE (256 * 1024 * CUSTOM_LOG_DUMP_BUFSIZE_MB)
#define LOG_DUMP_RTT_MAX_BUFSIZE (256 * 1024 * CUSTOM_LOG_DUMP_BUFSIZE_MB)
#define LOG_DUMP_FILTER_MAX_BUFSIZE (128 * 1024 * CUSTOM_LOG_DUMP_BUFSIZE_MB)

#if LOG_DUMP_TOTAL_BUFSIZE < (LOG_DUMP_GENERAL_MAX_BUFSIZE + \
	LOG_DUMP_PRESERVE_MAX_BUFSIZE + LOG_DUMP_ECNTRS_MAX_BUFSIZE + LOG_DUMP_RTT_MAX_BUFSIZE \
	+ LOG_DUMP_BCM_TRACE_MAX_BUFSIZE + LOG_DUMP_FILTER_MAX_BUFSIZE)
#error "LOG_DUMP_TOTAL_BUFSIZE is lesser than sum of all rings"
#endif

/* Special buffer is allocated as separately in prealloc */
#define LOG_DUMP_SPECIAL_MAX_BUFSIZE (8 * 1024)

#define LOG_DUMP_MAX_FILESIZE (8 *1024 * 1024) /* 8 MB default */

#ifdef CONFIG_LOG_BUF_SHIFT
/* 15% of kernel log buf size, if for example klog buf size is 512KB
* 15% of 512KB ~= 80KB
*/
#define LOG_DUMP_KERNEL_TAIL_FLUSH_SIZE \
	(15 * ((1 << CONFIG_LOG_BUF_SHIFT)/100))
#endif /* CONFIG_LOG_BUF_SHIFT */

#define LOG_DUMP_COOKIE_BUFSIZE	1024u
#define DHD_PRINT_BUF_NAME_LEN 30

#ifdef DHD_SSSR_DUMP
#define DUMP_SSSR_ATTR_START	2
#define DUMP_SSSR_ATTR_COUNT	10

typedef enum {
	SSSR_C0_D11_BEFORE = 0,
	SSSR_C0_D11_AFTER = 1,
	SSSR_C1_D11_BEFORE = 2,
	SSSR_C1_D11_AFTER = 3,
	SSSR_C2_D11_BEFORE = 4,
	SSSR_C2_D11_AFTER = 5,
	SSSR_DIG_BEFORE = 6,
	SSSR_DIG_AFTER = 7
} EWP_SSSR_DUMP;
#endif /* DHD_SSSR_DUMP */

typedef enum {
	DLD_BUF_TYPE_GENERAL = 0,
	DLD_BUF_TYPE_PRESERVE = 1,
	DLD_BUF_TYPE_SPECIAL = 2,
	DLD_BUF_TYPE_ECNTRS = 3,
	DLD_BUF_TYPE_FILTER = 4,
	DLD_BUF_TYPE_ALL = 5
} log_dump_type_t;

/*
 * XXX: Always add new enums at the end to compatible with parser,
 * also add new section in split_ret of EWP_config.py
 */
typedef enum {
	LOG_DUMP_SECTION_GENERAL = 0,
	LOG_DUMP_SECTION_ECNTRS,
	LOG_DUMP_SECTION_SPECIAL,
	LOG_DUMP_SECTION_DHD_DUMP,
	LOG_DUMP_SECTION_EXT_TRAP,
	LOG_DUMP_SECTION_HEALTH_CHK,
	LOG_DUMP_SECTION_PRESERVE,
	LOG_DUMP_SECTION_COOKIE,
	LOG_DUMP_SECTION_RING,
	LOG_DUMP_SECTION_STATUS,
	LOG_DUMP_SECTION_RTT,
	LOG_DUMP_SECTION_BCM_TRACE,
	LOG_DUMP_SECTION_PKTID_MAP_LOG,
	LOG_DUMP_SECTION_PKTID_UNMAP_LOG,
	LOG_DUMP_SECTION_TIMESTAMP,
	LOG_DUMP_SECTION_MAX
} log_dump_section_type_t;

/* Each section in the debug_dump log file shall begin with a header */
typedef struct {
	uint32 magic;  /* 0xDEB3DEB3 */
	uint32 type;   /* of type log_dump_section_type_t */
	uint64 timestamp;
	uint32 length;  /* length of the section that follows */
} log_dump_section_hdr_t;

#ifdef DHD_DEBUGABILITY_LOG_DUMP_RING
struct dhd_dbg_ring_buf
{
	void *dhd_pub;
};
extern struct dhd_dbg_ring_buf g_ring_buf;
#endif /* DHD_DEBUGABILITY_LOG_DUMP_RING */

typedef struct dhd_debug_dump_ring_entry {
	log_dump_section_type_t type;
	uint32 debug_dump_ring;
} dhd_debug_dump_ring_entry_t;

/* below structure describe ring buffer. */
struct dhd_log_dump_buf
{
#if defined(LINUX) || defined(linux) || defined(ANDROID) || defined(OEM_ANDROID)
	spinlock_t lock;
#endif
	void *dhd_pub;
	unsigned int enable;
	unsigned int wraparound;
	unsigned long max;
	unsigned int remain;
	char* present;
	char* front;
	char* buffer;
};

typedef struct {
	char *hdr_str;
	log_dump_section_type_t sec_type;
} dld_hdr_t;

extern void dhd_log_dump_write(int type, char *binary_data,
		int binary_len, const char *fmt, ...);
void dhd_schedule_log_dump(dhd_pub_t *dhdp, void *type);
void dhd_log_dump_trigger(dhd_pub_t *dhdp, int subcmd);
void dhd_log_dump_vendor_trigger(dhd_pub_t *dhd_pub);

#ifdef DHD_DEBUGABILITY_DEBUG_DUMP
int dhd_debug_dump_get_ring_num(int sec_type);
#endif /* DHD_DEBUGABILITY_DEBUG_DUMP */
int dhd_log_dump_ring_to_file(dhd_pub_t *dhdp, void *ring_ptr, void *file,
		unsigned long *file_posn, log_dump_section_hdr_t *sec_hdr, char *text_hdr,
		uint32 sec_type);
int dhd_dump_debug_ring(dhd_pub_t *dhdp, void *ring_ptr, const void *user_buf,
		log_dump_section_hdr_t *sec_hdr, char *text_hdr, int buflen, uint32 sec_type);
int dhd_log_dump_cookie_to_file(dhd_pub_t *dhdp, void *fp,
	const void *user_buf, unsigned long *f_pos);
int dhd_log_dump_cookie(dhd_pub_t *dhdp, const void *user_buf);
uint32 dhd_log_dump_cookie_len(dhd_pub_t *dhdp);
int dhd_logdump_cookie_init(dhd_pub_t *dhdp, uint8 *buf, uint32 buf_size);
void dhd_logdump_cookie_deinit(dhd_pub_t *dhdp);
void dhd_logdump_cookie_save(dhd_pub_t *dhdp, char *cookie, char *type);
int dhd_logdump_cookie_get(dhd_pub_t *dhdp, char *ret_cookie, uint32 buf_size);
int dhd_logdump_cookie_count(dhd_pub_t *dhdp);
int dhd_get_dld_log_dump(void *dev, dhd_pub_t *dhdp, const void *user_buf, void *fp,
	uint32 len, int type, void *pos);
#if defined(BCMPCIE)
int dhd_print_ext_trap_data(void *dev, dhd_pub_t *dhdp, const void *user_buf,
	void *fp, uint32 len, void *pos);
uint32 dhd_get_ext_trap_len(void *ndev, dhd_pub_t *dhdp);
#endif /* BCMPCIE */
int dhd_print_dump_data(void *dev, dhd_pub_t *dhdp, const void *user_buf,
	void *fp, uint32 len, void *pos);
int dhd_print_cookie_data(void *dev, dhd_pub_t *dhdp, const void *user_buf,
	void *fp, uint32 len, void *pos);
int dhd_print_health_chk_data(void *dev, dhd_pub_t *dhdp, const void *user_buf,
	void *fp, uint32 len, void *pos);
int dhd_print_time_str(const void *user_buf, void *fp, uint32 len, void *pos);
#ifdef DHD_DUMP_PCIE_RINGS
int dhd_print_flowring_data(void *dev, dhd_pub_t *dhdp, const void *user_buf,
	void *fp, uint32 len, void *pos);
uint32 dhd_get_flowring_len(void *ndev, dhd_pub_t *dhdp);
#endif /* DHD_DUMP_PCIE_RINGS */
#ifdef DHD_STATUS_LOGGING
extern int dhd_print_status_log_data(void *dev, dhd_pub_t *dhdp,
	const void *user_buf, void *fp, uint32 len, void *pos);
extern uint32 dhd_get_status_log_len(void *ndev, dhd_pub_t *dhdp);
#endif /* DHD_STATUS_LOGGING */
#ifdef DHD_MAP_PKTID_LOGGING
extern uint32 dhd_pktid_buf_len(dhd_pub_t *dhd, bool is_map);
extern int dhd_print_pktid_map_log_data(void *dev, dhd_pub_t *dhdp,
	const void *user_buf, void *fp, uint32 len, void *pos, bool is_map);
extern int dhd_write_pktid_log_dump(dhd_pub_t *dhdp, const void *user_buf,
	void *fp, uint32 len, unsigned long *pos, bool is_map);
extern uint32 dhd_get_pktid_map_logging_len(void *ndev, dhd_pub_t *dhdp, bool is_map);
#endif /* DHD_MAP_PKTID_LOGGING */
int dhd_print_ecntrs_data(void *dev, dhd_pub_t *dhdp, const void *user_buf,
	void *fp, uint32 len, void *pos);
int dhd_print_rtt_data(void *dev, dhd_pub_t *dhdp, const void *user_buf,
	void *fp, uint32 len, void *pos);
int dhd_get_debug_dump_file_name(void *dev, dhd_pub_t *dhdp,
	char *dump_path, int size);
uint32 dhd_get_time_str_len(void);
uint32 dhd_get_health_chk_len(void *ndev, dhd_pub_t *dhdp);
uint32 dhd_get_dhd_dump_len(void *ndev, dhd_pub_t *dhdp);
uint32 dhd_get_cookie_log_len(void *ndev, dhd_pub_t *dhdp);
uint32 dhd_get_ecntrs_len(void *ndev, dhd_pub_t *dhdp);
uint32 dhd_get_rtt_len(void *ndev, dhd_pub_t *dhdp);
uint32 dhd_get_dld_len(int log_type);
void dhd_init_sec_hdr(log_dump_section_hdr_t *sec_hdr);
extern char *dhd_log_dump_get_timestamp(void);
bool dhd_log_dump_ecntr_enabled(void);
bool dhd_log_dump_rtt_enabled(void);
void dhd_nla_put_sssr_dump_len(void *ndev, uint32 *arr_len);
int dhd_get_debug_dump(void *dev, const void *user_buf, uint32 len, int type);
#ifdef DHD_SSSR_DUMP_BEFORE_SR
int
dhd_sssr_dump_d11_buf_before(void *dev, const void *user_buf, uint32 len, int core);
int
dhd_sssr_dump_dig_buf_before(void *dev, const void *user_buf, uint32 len);
#endif /* DHD_SSSR_DUMP_BEFORE_SR */
int
dhd_sssr_dump_d11_buf_after(void *dev, const void *user_buf, uint32 len, int core);
int
dhd_sssr_dump_dig_buf_after(void *dev, const void *user_buf, uint32 len);
#ifdef DHD_PKT_LOGGING
extern int dhd_os_get_pktlog_dump(void *dev, const void *user_buf, uint32 len);
extern spinlock_t* dhd_os_get_pktlog_lock(dhd_pub_t *dhdp);
extern uint32 dhd_os_get_pktlog_dump_size(struct net_device *dev);
extern void dhd_os_get_pktlogdump_filename(struct net_device *dev, char *dump_path, int len);
#endif /* DHD_PKT_LOGGING */

#ifdef DNGL_AXI_ERROR_LOGGING
extern int dhd_os_get_axi_error_dump(void *dev, const void *user_buf, uint32 len);
extern int dhd_os_get_axi_error_dump_size(struct net_device *dev);
extern void dhd_os_get_axi_error_filename(struct net_device *dev, char *dump_path, int len);
#endif /*  DNGL_AXI_ERROR_LOGGING */

#ifdef DHD_DEBUGABILITY_DEBUG_DUMP
extern int dhd_debug_dump_to_ring(dhd_pub_t *dhdp);
#endif /* DHD_DEBUGABILITY_DEBUG_DUMP */

extern char *dhd_log_dump_get_timestamp(void);
extern void dhd_dbg_ring_write(int type, char *binary_data,
	int binary_len, const char *fmt, ...);

void dhd_log_dump_init(dhd_pub_t *dhd);
void dhd_log_dump_deinit(dhd_pub_t *dhd);
void dhd_log_dump(void *handle, void *event_info, uint8 event);
int do_dhd_log_dump(dhd_pub_t *dhdp, log_dump_type_t *type);
void dhd_print_buf_addr(dhd_pub_t *dhdp, char *name, void *buf, unsigned int size);
void dhd_log_dump_buf_addr(dhd_pub_t *dhdp, log_dump_type_t *type);
int dhd_log_flush(dhd_pub_t *dhdp, log_dump_type_t *type);

extern void get_debug_dump_time(char *str);
extern void clear_debug_dump_time(char *str);
#if defined(WL_CFGVENDOR_SEND_HANG_EVENT) || defined(DHD_PKT_LOGGING)
extern void copy_debug_dump_time(char *dest, char *src);
#endif

void dhd_get_debug_dump_len(void *handle, struct sk_buff *skb, void *event_info, uint8 event);
void cfgvendor_log_dump_len(dhd_pub_t *dhdp, log_dump_type_t *type, struct sk_buff *skb);
#ifdef DHD_IOVAR_LOG_FILTER_DUMP
bool dhd_iovar_log_dump_check(dhd_pub_t *dhd_pub, uint32 cmd, char *msg);
#endif /* DHD_IOVAR_LOG_FILTER_DUMP */
#endif /* DHD_LOG_DUMP */

#endif /* !__DHD_LOG_DUMP_H__ */
