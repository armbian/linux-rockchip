/*
 * drivers/input/touchscreen/hyn_driver_release/hyn_core.h
 *
 * hynitron TouchScreen driver.
 *
 * Copyright (c) 2025  hynitron
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
#ifndef HYNITRON_CORE_H
#define HYNITRON_CORE_H

#include <linux/syscalls.h>
#include <linux/gpio.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/dma-mapping.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
//#include <linux/rtpm_prio.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/fb.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

// #include <linux/regulator/driver.h>
#include <linux/sysfs.h>
#include <linux/fcntl.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/moduleparam.h>
#include "hyn_cfg.h"

#ifdef CONFIG_BUS_SPI
#include "mt_spi.h"
#endif

#if (HYN_WAKE_LOCK_EN == 1)
#include <linux/wakelock.h>
#endif

#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
// #elif defined(CONFIG_DRM)
// #include <linux/msm_drm_notify.h>
// #if defined(CONFIG_DRM_PANEL)
// #include <drm/drm_panel.h>
// #endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

// #define __noscs __attribute__((__no_sanitize__("shadow-call-stack")))

extern u8 hyn_log_level;
#define HYN_INFO(fmt, args...) printk(KERN_INFO "[HYN]"fmt"\n", ##args)
#define HYN_INFO2(fmt, args...) if (hyn_log_level > 0) printk(KERN_INFO "[HYN]"fmt"\n", ##args)
#define HYN_INFO3(fmt, args...) if (hyn_log_level > 1) printk(KERN_INFO "[HYN]"fmt"\n", ##args)
#define HYN_INFO4(fmt, args...) if (hyn_log_level > 2) printk(KERN_INFO "[HYN]"fmt"\n", ##args)
#define HYN_ERROR(fmt, args...) printk(KERN_ERR "[HYN][Error]%s:"fmt"\n", __func__, ##args)
#define HYN_ENTER() HYN_INFO2("[enter]%s\n", __func__)

#if HYN_GKI_VER
// MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif

// #define IS_ERR_OR_NULL(x)  (x <= 0)
#define U8TO16(x1,x2) ((((x1)&0xFF)<<8)|((x2)&0xFF))
#define U8TO32(x1,x2,x3,x4) ((((x1)&0xFF)<<24)|(((x2)&0xFF)<<16)|(((x3)&0xFF)<<8)|((x4)&0xFF))
#define U16REV(x)  ((((x)<<8)&0xFF00)|(((x)>>8)&0x00FF))

#undef NULL
#undef FALSE
#undef TRUE
#undef DISABLE
#undef ENABLE
#define NULL	((void*)0)
#define FALSE	(-1)
#define TRUE	(0)
#define DISABLE	(0)
#define ENABLE	(1)

#define PS_FAR_AWAY	1
#define PS_NEAR		0

#define MULTI_OPEN_TEST	 (0x80)
#define MULTI_SHORT_TEST (0x01)
#define MULTI_SCAP_TEST  (0x02)

#if HYN_GKI_VER
#define hyn_fs_write(...)
#define hyn_fs_read(...)
#else
#define hyn_fs_write kernel_write
#define hyn_fs_read kernel_read
#endif

enum work_mode{
	NOMAL_MODE = 0,
	GESTURE_MODE = 1,
	LP_MODE = 2,
	DEEPSLEEP = 3,
	DIFF_MODE = 4,
	RAWDATA_MODE = 5,
	BASELINE_MODE = 6,
	CALIBRATE_MODE = 7,
	FAC_TEST_MODE = 8,
	GLOVE_EXIT = 0x10,
	GLOVE_ENTER = 0x11,
	CHARGE_EXIT = 0x12,
	CHARGE_ENTER = 0x13,
	ENTER_BOOT_MODE = 0xCA,
};

enum report_typ{
	REPORT_NONE = 0,
	REPORT_POS = 0x01,
	REPORT_KEY = 0x02,
	REPORT_GES = 0x04,
	REPORT_PROX = 0x08
};

enum fac_test_ero{
	FAC_TEST_PASS = 0,
	FAC_GET_DATA_FAIL = -1,
	FAC_GET_CFG_FAIL = -4,
	FAC_TEST_OPENL_FAIL = -5,
	FAC_TEST_OPENH_FAIL = -7,
	FAC_TEST_SHORT_FAIL = -6,
	FAC_TEST_SCAP_FAIL = -8,
};

enum ges_idx{
	IDX_U = 0,
	IDX_UP,
	IDX_DOWN,
	IDX_LEFT,
	IDX_RIGHT,
	IDX_O,
	IDX_e,
	IDX_M,
	IDX_L,
	IDX_W,
	IDX_S,
	IDX_V,
	IDX_C,
	IDX_Z,
	IDX_POWER,
	IDX_NULL = 0xFF,
};


struct hyn_plat_data {
	struct regulator *vdd_ana;
	struct regulator *vdd_i2c;

	struct gpio_desc *reset_gpio;
	struct gpio_desc *irq_gpio;
	unsigned long irq_flags;

	struct pinctrl *pinctl;
	struct pinctrl_state *pin_active;
	struct pinctrl_state *pin_suspend;

	u32 x_resolution;
	u32 y_resolution;
	int swap_xy;
	int reverse_x;
	int reverse_y;

	int max_touch_num;
	int key_num;
	u32  key_x_coords[8]; // max support 8 keys
	u32  key_y_coords;
	u32  key_code[8];
};

struct hyn_chip_series {
	u32 part_no;
	u32 moudle_id;
	u8* chip_name;
	u8* fw_bin;
};

#define MAX_POINTS_REPORT	 (10)
struct ts_frame {
	u8 rep_num;
	enum report_typ report_need;
	u8 key_id;
	u8 key_state;
	struct {
		u8 pos_id;
		u8 event;
		u16 pos_x;
		u16 pos_y;
		u16 pres_z;
	} pos_info[MAX_POINTS_REPORT];
};

struct tp_info {
	u8  fw_sensor_txnum;
	u8  fw_sensor_rxnum;
	u8  fw_key_num;
	u8  reserve;
	u16 fw_res_y;
	u16 fw_res_x;
	u32 fw_boot_time;
	u32 fw_project_id;
	u32 fw_chip_type;
	u32 fw_ver;
	u32 ic_fw_checksum;
	u32 fw_module_id;
};


struct hyn_ts_data {
	u16 bus_type;
#ifdef I2C_PORT
	struct i2c_client *client;
#else
	struct spi_device *client;
#endif
	struct device *dev;
	struct input_dev  *input_dev;
	struct workqueue_struct *hyn_workqueue;
	int gpio_irq;
	int esd_fail_cnt;
	u32 esd_last_value;
	int esd_block_cnt;
	int report_id_flg;
	enum work_mode work_mode;

	int power_is_on;
	atomic_t irq_is_disable;
	atomic_t hyn_irq_flg;
	// spinlock_t irq_lock;
	struct mutex mutex_report;
	struct mutex mutex_bus;
	struct mutex mutex_fs;
#if (HYN_WAKE_LOCK_EN==1)
	struct wake_lock tp_wakelock;
#endif

#if (I2C_USE_DMA==2)
	u8 *dma_buff_va;
	dma_addr_t dma_buff_pa;
#endif
	struct hyn_plat_data plat_data;
	struct tp_info hw_info;
	struct ts_frame rp_buf;

	struct work_struct  work_report;
	struct work_struct  work_resume;
	struct delayed_work esdcheck_work;

	struct work_struct  work_updata_fw;
	int boot_is_pass;
	int need_updata_fw;
	u8 fw_file_name[128];
	u8 *fw_updata_addr;
	int fw_updata_len;
	int fw_dump_state;
	u8 fw_updata_process;
	u8 host_cmd_save[16];
	wait_queue_head_t wait_irq;

	u8 log_level;
	u8 prox_is_enable;
	u8 prox_state;

	u8 gesture_is_enable;
	u8 gesture_id;
	const void *hyn_fuc_used;

	u8 charge_is_enable;
	u8 glove_is_enable;

#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
	int old_fb_state;
// #elif defined(CONFIG_DRM)
//	 struct notifier_block fb_notif;
//	 int old_fb_state;
// #if defined(CONFIG_DRM_PANEL)
//	 struct drm_panel *active_panel;
// #endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	struct kobject *sys_node;
};

struct hyn_ts_fuc{
	void (*tp_rest)(void);
	int (*tp_report)(void);
	int (*tp_supend)(void);
	int (*tp_resum)(void);
	int (*tp_chip_init)(struct hyn_ts_data *ts_data);
	int (*tp_updata_fw)(u8 *bin_addr, u16 len);
	int (*tp_set_workmode)(enum work_mode mode,u8 enable);
	u32 (*tp_check_esd)(void);
	int (*tp_prox_handle)(u8 cmd);
	int (*tp_get_dbg_data)(u8 *buf,u16 len);
	int (*tp_get_test_result)(u8 *buf,u16 len);
};

int hyn_write_data(struct hyn_ts_data *ts_data, u8 *buf, u8 reg_len, u16 len);
int hyn_read_data(struct hyn_ts_data *ts_data,u8 *buf, u16 len);
int hyn_wr_reg(struct hyn_ts_data *ts_data, u32 reg_addr, u8 reg_len, u8 *rbuf, u16 rlen);

void hyn_irq_set(struct hyn_ts_data *ts_data, u8 value);
void hyn_esdcheck_switch(struct hyn_ts_data *ts_data, u8 enable);
int hyn_copy_for_updata(struct hyn_ts_data *ts_data,u8 *buf,u32 offset,u16 len);
int hyn_get_word(u8 **sc_str, u8* ds_str);
void hyn_set_i2c_addr(struct hyn_ts_data *ts_data,u8 addr);

int hyn_proc_fs_int(struct hyn_ts_data *ts_data);
void hyn_proc_fs_exit(void);
int hyn_tool_fs_int(struct hyn_ts_data *ts_data);
void hyn_tool_fs_exit(void);

int hyn_create_sysfs(struct hyn_ts_data *ts_data);
void hyn_release_sysfs(struct hyn_ts_data *ts_data);
int hyn_gesture_init(struct hyn_ts_data *ts_data);
void hyn_gesture_report(struct hyn_ts_data *ts_data);
void hyn_gesture_exit(struct hyn_ts_data *ts_data);

int hyn_proximity_exit(void);
int hyn_proximity_int(struct hyn_ts_data *ts_data);
int hyn_proximity_report(u8 proximity_value);

int hyn_wait_irq_timeout(struct hyn_ts_data *ts_data,int msec);
int hyn_factory_multitest(struct hyn_ts_data *ts_data ,char *cfg_path, u8 *data,s16 *test_th,u8 test_item);
int hyn_fac_test_log_save(char *log_name,struct hyn_ts_data *ts_data,s16 *test_data, int test_ret, u8 test_item);
int hyn_str_2_num(char *str,u8 type);
u16 hyn_sum16(int val, u8* buf,u16 len);
u32 hyn_sum32(int val, u32* buf,u16 len);

int hyn_power_source_ctrl(struct hyn_ts_data *ts_data, int enable);

//ic type
extern const struct hyn_ts_fuc hyn_cst3xx_fuc;
extern const struct hyn_ts_fuc hyn_cst66xx_fuc;
extern const struct hyn_ts_fuc hyn_cst7xx_fuc;
extern const struct hyn_ts_fuc hyn_cst8xxT_fuc;
extern const struct hyn_ts_fuc hyn_cst92xx_fuc;
extern const struct hyn_ts_fuc hyn_cst3240_fuc;
extern const struct hyn_ts_fuc hyn_cst226se_fuc;
extern const struct hyn_ts_fuc hyn_cst36xxes_fuc;

#endif
