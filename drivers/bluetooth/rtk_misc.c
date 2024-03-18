/*
 *
 *  Realtek Bluetooth USB download firmware driver
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/skbuff.h>
#include <linux/usb.h>
#include <linux/dcache.h>
#include <linux/in.h>
#include <net/sock.h>
#include <asm/unaligned.h>

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <linux/usb.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/poll.h>

#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
#include <linux/pm_runtime.h>
#endif

#include <linux/firmware.h>
#include <linux/suspend.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include <net/bluetooth/hci.h>

#include "rtk_misc.h"

#include <linux/file.h>
#include <linux/ctype.h>
#define BDADDR_STRING_LEN	17
#define BDADDR_FILE		"/opt/bdaddr"

struct cfg_list_item {
	struct list_head list;
	u16 offset;
	u8 len;
	u8 data[0];
};

static struct list_head list_configs;

#define EXTRA_CONFIG_FILE	"/opt/rtk_btconfig.txt"
static struct list_head list_extracfgs;

#define CMD_CMP_EVT		    0x0e
#define PKT_LEN			    300
#define MSG_TO			    1000	//us
#define PATCH_SEG_MAX	    252
#define DATA_END		    0x80
#define DOWNLOAD_OPCODE	    0xfc20
/* This command is used only for TV patch
 * if host is going to suspend state, it should send this command to
 * Controller. Controller will scan the special advertising packet
 * which indicates Controller to wake up host */
#define STARTSCAN_OPCODE	    0xfc28
#define TRUE			    1
#define FALSE			    0
#define CMD_HDR_LEN		    sizeof(struct hci_command_hdr)
#define EVT_HDR_LEN		    sizeof(struct hci_event_hdr)
#define CMD_CMP_LEN		    sizeof(struct hci_ev_cmd_complete)

#define HCI_CMD_READ_BD_ADDR                0x1009
#define HCI_VENDOR_CHANGE_BDRATE            0xfc17
#define HCI_VENDOR_READ_RTK_ROM_VERISION    0xfc6d
#define HCI_VENDOR_READ_LMP_VERISION        0x1001

#define ROM_LMP_NONE                0x0000
#define ROM_LMP_8723a               0x1200
#define ROM_LMP_8723b               0x8723
#define ROM_LMP_8821a               0X8821
#define ROM_LMP_8761a               0X8761
#define ROM_LMP_8822b               0X8822
#define ROM_LMP_8852a               0x8852

struct rtk_eversion_evt {
	uint8_t status;
	uint8_t version;
} __attribute__ ((packed));

struct rtk_epatch_entry {
	uint16_t chipID;
	uint16_t patch_length;
	uint32_t start_offset;
} __attribute__ ((packed));

struct rtk_epatch {
	uint8_t signature[8];
	uint32_t fw_version;
	uint16_t number_of_total_patch;
	struct rtk_epatch_entry entry[0];
} __attribute__ ((packed));

struct rtk_extension_entry {
	uint8_t opcode;
	uint8_t length;
	uint8_t *data;
} __attribute__ ((packed));

//signature: Realtech
const uint8_t RTK_EPATCH_SIGNATURE[8] =
    { 0x52, 0x65, 0x61, 0x6C, 0x74, 0x65, 0x63, 0x68 };
//Extension Section IGNATURE:0x77FD0451
const uint8_t Extension_Section_SIGNATURE[4] = { 0x51, 0x04, 0xFD, 0x77 };

uint16_t project_id[] = {
	ROM_LMP_8723a,
	ROM_LMP_8723b,
	ROM_LMP_8821a,
	ROM_LMP_8761a,
	ROM_LMP_NONE,
	ROM_LMP_NONE,
	ROM_LMP_NONE,
	ROM_LMP_NONE,
	ROM_LMP_8822b,
	ROM_LMP_8723b, /* RTL8723DU */
	ROM_LMP_8821a, /* RTL8821CU */
	ROM_LMP_NONE,
	ROM_LMP_NONE,
	ROM_LMP_8822b, /* RTL8822CU */
	ROM_LMP_8761a, /* index 14 for 8761BU */
	ROM_LMP_NONE,
	ROM_LMP_NONE,
	ROM_LMP_NONE,
	ROM_LMP_8852a, /* index 18 for 8852AU */
	ROM_LMP_8723b, /* index 19 for 8723FU */
	ROM_LMP_8852a, /* index 20 for 8852BU */
	ROM_LMP_NONE,
	ROM_LMP_NONE,
	ROM_LMP_NONE,
	ROM_LMP_NONE,
	ROM_LMP_8852a, /* index 25 for 8852CU */
};

enum rtk_endpoit {
	CTRL_EP = 0,
	INTR_EP = 1,
	BULK_EP = 2,
	ISOC_EP = 3
};

/* software id */
#define RTLPREVIOUS	0x00
#define RTL8822BU	0x70
#define RTL8723DU	0x71
#define RTL8821CU	0x72
#define RTL8822CU	0x73
#define RTL8761BU	0x74
#define RTL8852AU	0x75
#define RTL8723FU	0x76
#define RTL8852BU	0x77
#define RTL8852CU	0x78

typedef struct {
	uint16_t prod_id;
	uint16_t lmp_sub;
	char *	 mp_patch_name;
	char *	 patch_name;
	char *	 config_name;
	u8       chip_type;
} patch_info;

typedef struct {
	struct list_head list_node;
	struct usb_interface *intf;
	struct usb_device *udev;
	patch_info *patch_entry;
} dev_data;

typedef struct {
	dev_data *dev_entry;
	int pipe_in, pipe_out;
	uint8_t *send_pkt;
	uint8_t *rcv_pkt;
	struct hci_command_hdr *cmd_hdr;
	struct hci_event_hdr *evt_hdr;
	struct hci_ev_cmd_complete *cmd_cmp;
	uint8_t *req_para, *rsp_para;
	uint8_t *fw_data;
	int pkt_len, fw_len;
} xchange_data;

typedef struct {
	uint8_t index;
	uint8_t data[PATCH_SEG_MAX];
} __attribute__ ((packed)) download_cp;

typedef struct {
	uint8_t status;
	uint8_t index;
} __attribute__ ((packed)) download_rp;

#define RTK_VENDOR_CONFIG_MAGIC 0x8723ab55
const u8 cfg_magic[4] = { 0x55, 0xab, 0x23, 0x87 };
struct rtk_bt_vendor_config_entry {
	uint16_t offset;
	uint8_t entry_len;
	uint8_t entry_data[0];
} __attribute__ ((packed));

struct rtk_bt_vendor_config {
	uint32_t signature;
	uint16_t data_len;
	struct rtk_bt_vendor_config_entry entry[0];
} __attribute__ ((packed));
#define BT_CONFIG_HDRLEN		sizeof(struct rtk_bt_vendor_config)

static uint8_t gEVersion = 0xFF;

static dev_data *dev_data_find(struct usb_interface *intf);
static patch_info *get_patch_entry(struct usb_device *udev);
static int load_firmware(dev_data * dev_entry, uint8_t ** buff);
static void init_xdata(xchange_data * xdata, dev_data * dev_entry);
static int check_fw_version(xchange_data * xdata);
static int download_data(xchange_data * xdata);
static int send_hci_cmd(xchange_data * xdata);
static int rcv_hci_evt(xchange_data * xdata);
static uint8_t rtk_get_eversion(dev_data * dev_entry);

static patch_info fw_patch_table[] = {
/* { pid, lmp_sub, mp_fw_name, fw_name, config_name, chip_type } */
	{0x1724, 0x1200, "mp_rtl8723a_fw", "rtl8723a_fw", "rtl8723a_config", RTLPREVIOUS},	/* RTL8723A */
	{0x8723, 0x1200, "mp_rtl8723a_fw", "rtl8723a_fw", "rtl8723a_config", RTLPREVIOUS},	/* 8723AE */
	{0xA723, 0x1200, "mp_rtl8723a_fw", "rtl8723a_fw", "rtl8723a_config", RTLPREVIOUS},	/* 8723AE for LI */
	{0x0723, 0x1200, "mp_rtl8723a_fw", "rtl8723a_fw", "rtl8723a_config", RTLPREVIOUS},	/* 8723AE */
	{0x3394, 0x1200, "mp_rtl8723a_fw", "rtl8723a_fw", "rtl8723a_config", RTLPREVIOUS},	/* 8723AE for Azurewave */

	{0x0724, 0x1200, "mp_rtl8723a_fw", "rtl8723a_fw", "rtl8723a_config", RTLPREVIOUS},	/* 8723AU */
	{0x8725, 0x1200, "mp_rtl8723a_fw", "rtl8723a_fw", "rtl8723a_config", RTLPREVIOUS},	/* 8723AU */
	{0x872A, 0x1200, "mp_rtl8723a_fw", "rtl8723a_fw", "rtl8723a_config", RTLPREVIOUS},	/* 8723AU */
	{0x872B, 0x1200, "mp_rtl8723a_fw", "rtl8723a_fw", "rtl8723a_config", RTLPREVIOUS},	/* 8723AU */

	{0xb720, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", RTLPREVIOUS},	/* RTL8723BU */
	{0xb72A, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", RTLPREVIOUS},	/* RTL8723BU */
	{0xb728, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", RTLPREVIOUS},	/* RTL8723BE for LC */
	{0xb723, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", RTLPREVIOUS},	/* RTL8723BE */
	{0xb72B, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", RTLPREVIOUS},	/* RTL8723BE */
	{0xb001, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", RTLPREVIOUS},	/* RTL8723BE for HP */
	{0xb002, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", RTLPREVIOUS},	/* RTL8723BE */
	{0xb003, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", RTLPREVIOUS},	/* RTL8723BE */
	{0xb004, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", RTLPREVIOUS},	/* RTL8723BE */
	{0xb005, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", RTLPREVIOUS},	/* RTL8723BE */

	{0x3410, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", RTLPREVIOUS},	/* RTL8723BE for Azurewave */
	{0x3416, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", RTLPREVIOUS},	/* RTL8723BE for Azurewave */
	{0x3459, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", RTLPREVIOUS},	/* RTL8723BE for Azurewave */
	{0xE085, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", RTLPREVIOUS},	/* RTL8723BE for Foxconn */
	{0xE08B, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", RTLPREVIOUS},	/* RTL8723BE for Foxconn */
	{0xE09E, 0x8723, "mp_rtl8723b_fw", "rtl8723b_fw", "rtl8723b_config", RTLPREVIOUS},	/* RTL8723BE for Foxconn */

	{0xA761, 0x8761, "mp_rtl8761a_fw", "rtl8761au_fw", "rtl8761a_config", RTLPREVIOUS},	/* RTL8761AU only */
	{0x818B, 0x8761, "mp_rtl8761a_fw", "rtl8761aw_fw", "rtl8761aw_config", RTLPREVIOUS},	/* RTL8761AW + 8192EU */
	{0x818C, 0x8761, "mp_rtl8761a_fw", "rtl8761aw_fw", "rtl8761aw_config", RTLPREVIOUS},	/* RTL8761AW + 8192EU */
	{0x8760, 0x8761, "mp_rtl8761a_fw", "rtl8761au8192ee_fw", "rtl8761a_config", RTLPREVIOUS},	/* RTL8761AU + 8192EE */
	{0xB761, 0x8761, "mp_rtl8761a_fw", "rtl8761au_fw", "rtl8761a_config", RTLPREVIOUS},	/* RTL8761AUV only */
	{0x8761, 0x8761, "mp_rtl8761a_fw", "rtl8761au8192ee_fw", "rtl8761a_config", RTLPREVIOUS},	/* RTL8761AU + 8192EE for LI */
	{0x8A60, 0x8761, "mp_rtl8761a_fw", "rtl8761au8812ae_fw", "rtl8761a_config", RTLPREVIOUS},	/* RTL8761AU + 8812AE */
	{0x3527, 0x8761, "mp_rtl8761a_fw", "rtl8761au8192ee_fw", "rtl8761a_config", RTLPREVIOUS},	/* RTL8761AU + 8814AE */

	{0x8821, 0x8821, "mp_rtl8821a_fw", "rtl8821a_fw", "rtl8821a_config", RTLPREVIOUS},	/* RTL8821AE */
	{0x0821, 0x8821, "mp_rtl8821a_fw", "rtl8821a_fw", "rtl8821a_config", RTLPREVIOUS},	/* RTL8821AE */
	{0x0823, 0x8821, "mp_rtl8821a_fw", "rtl8821a_fw", "rtl8821a_config", RTLPREVIOUS},	/* RTL8821AU */
	{0x3414, 0x8821, "mp_rtl8821a_fw", "rtl8821a_fw", "rtl8821a_config", RTLPREVIOUS},	/* RTL8821AE */
	{0x3458, 0x8821, "mp_rtl8821a_fw", "rtl8821a_fw", "rtl8821a_config", RTLPREVIOUS},	/* RTL8821AE */
	{0x3461, 0x8821, "mp_rtl8821a_fw", "rtl8821a_fw", "rtl8821a_config", RTLPREVIOUS},	/* RTL8821AE */
	{0x3462, 0x8821, "mp_rtl8821a_fw", "rtl8821a_fw", "rtl8821a_config", RTLPREVIOUS},	/* RTL8821AE */

	{0xb82c, 0x8822, "mp_rtl8822bu_fw", "rtl8822bu_fw", "rtl8822bu_config", RTL8822BU}, /* RTL8822BU */

	{0xd720, 0x8723, "mp_rtl8723du_fw", "rtl8723du_fw", "rtl8723du_config", RTL8723DU}, /* RTL8723DU */
	{0xd723, 0x8723, "mp_rtl8723du_fw", "rtl8723du_fw", "rtl8723du_config", RTL8723DU}, /* RTL8723DU */
	{0xd739, 0x8723, "mp_rtl8723du_fw", "rtl8723du_fw", "rtl8723du_config", RTL8723DU}, /* RTL8723DU */
	{0xb009, 0x8723, "mp_rtl8723du_fw", "rtl8723du_fw", "rtl8723du_config", RTL8723DU}, /* RTL8723DU */
	{0x0231, 0x8723, "mp_rtl8723du_fw", "rtl8723du_fw", "rtl8723du_config", RTL8723DU}, /* RTL8723DU for LiteOn */

	{0xb820, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CU */
	{0xc820, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CU */
	{0xc821, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE */
	{0xc823, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE */
	{0xc824, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE */
	{0xc825, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE */
	{0xc827, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE */
	{0xc025, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE */
	{0xc024, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE */
	{0xc030, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE */
	{0xb00a, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE */
	{0xb00e, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE */
	{0xc032, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE */
	{0x4000, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE for LiteOn */
	{0x4001, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE for LiteOn */
	{0x3529, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE for Azurewave */
	{0x3530, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE for Azurewave */
	{0x3532, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE for Azurewave */
	{0x3533, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE for Azurewave */
	{0x3538, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE for Azurewave */
	{0x3539, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE for Azurewave */
	{0x3558, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE for Azurewave */
	{0x3559, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE for Azurewave */
	{0x3581, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE for Azurewave */
	{0x3540, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE */
	{0x3541, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE for GSD */
	{0x3543, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CE for GSD */
	{0xc80c, 0x8821, "mp_rtl8821cu_fw", "rtl8821cu_fw", "rtl8821cu_config", RTL8821CU}, /* RTL8821CUH */

	{0xc82c, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CU */
	{0xc82e, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CU */
	{0xc81d, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CU */
	{0xd820, 0x8822, "mp_rtl8821du_fw", "rtl8821du_fw", "rtl8821du_config", RTL8822CU}, /* RTL8821DU */

	{0xc822, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE */
	{0xc82b, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE */
	{0xb00c, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE */
	{0xb00d, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE */
	{0xc123, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE */
	{0xc126, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE */
	{0xc127, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE */
	{0xc128, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE */
	{0xc129, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE */
	{0xc131, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE */
	{0xc136, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE */
	{0x3549, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE for Azurewave */
	{0x3548, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE for Azurewave */
	{0xc125, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE */
	{0x4005, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE for LiteOn */
	{0x3051, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE for LiteOn */
	{0x18ef, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE */
	{0x161f, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE */
	{0x3053, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE */
	{0xc547, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE */
	{0x3553, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE */
	{0x3555, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE */
	{0xc82f, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE-VS */
	{0xc02f, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE-VS */
	{0xc03f, 0x8822, "mp_rtl8822cu_fw", "rtl8822cu_fw", "rtl8822cu_config", RTL8822CU}, /* RTL8822CE-VS */

	{0x8771, 0x8761, "mp_rtl8761b_fw", "rtl8761bu_fw", "rtl8761bu_config", RTL8761BU}, /* RTL8761BU only */
	{0xa725, 0x8761, "mp_rtl8761b_fw", "rtl8725au_fw", "rtl8725au_config", RTL8761BU}, /* RTL8725AU */
	{0xa72A, 0x8761, "mp_rtl8761b_fw", "rtl8725au_fw", "rtl8725au_config", RTL8761BU}, /* RTL8725AU BT only */

	{0x885a, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AU */
	{0x8852, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */
	{0xa852, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */
	{0x2852, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */
	{0x385a, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */
	{0x3852, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */
	{0x1852, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */
	{0x4852, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */
	{0x4006, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */
	{0x3561, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */
	{0x3562, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */
	{0x588a, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */
	{0x589a, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */
	{0x590a, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */
	{0xc125, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */
	{0xe852, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */
	//{0xb852, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */
	{0xc852, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */
	{0xc549, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */
	{0xc127, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */
	{0x3565, 0x8852, "mp_rtl8852au_fw", "rtl8852au_fw", "rtl8852au_config", RTL8852AU}, /* RTL8852AE */

	{0xb733, 0x8723, "mp_rtl8723fu_fw", "rtl8723fu_fw", "rtl8723fu_config", RTL8723FU}, /* RTL8723FU */
	{0xb73a, 0x8723, "mp_rtl8723fu_fw", "rtl8723fu_fw", "rtl8723fu_config", RTL8723FU}, /* RTL8723FU */
	{0xf72b, 0x8723, "mp_rtl8723fu_fw", "rtl8723fu_fw", "rtl8723fu_config", RTL8723FU}, /* RTL8723FU */

	{0x8851, 0x8852, "mp_rtl8851au_fw", "rtl8851au_fw", "rtl8851au_config", RTL8852BU}, /* RTL8851AU */
	{0xa85b, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", RTL8852BU}, /* RTL8852BU */
	{0xb852, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", RTL8852BU}, /* RTL8852BU */
	{0xb85b, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", RTL8852BU}, /* RTL8852BE */
	{0xb85c, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", RTL8852BU}, /* RTL8852BE */
	{0x3571, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", RTL8852BU}, /* RTL8852BE */
	{0x3570, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", RTL8852BU}, /* RTL8852BE */
	{0x3572, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", RTL8852BU}, /* RTL8852BE */
	{0x4b06, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", RTL8852BU}, /* RTL8852BE */
	{0x885b, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", RTL8852BU}, /* RTL8852BE */
	{0x886b, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", RTL8852BU}, /* RTL8852BE */
	{0x887b, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", RTL8852BU}, /* RTL8852BE */
	{0xc559, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", RTL8852BU}, /* RTL8852BE */
	{0xb052, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", RTL8852BU}, /* RTL8852BE */
	{0xb152, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", RTL8852BU}, /* RTL8852BE */
	{0xb252, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", RTL8852BU}, /* RTL8852BE */
	{0x4853, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", RTL8852BU}, /* RTL8852BE */
	{0x1670, 0x8852, "mp_rtl8852bu_fw", "rtl8852bu_fw", "rtl8852bu_config", RTL8852BU}, /* RTL8852BE */

	{0xc85a, 0x8852, "mp_rtl8852cu_fw", "rtl8852cu_fw", "rtl8852cu_config", RTL8852CU}, /* RTL8852CU */
	{0x0852, 0x8852, "mp_rtl8852cu_fw", "rtl8852cu_fw", "rtl8852cu_config", RTL8852CU}, /* RTL8852CE */
	{0x5852, 0x8852, "mp_rtl8852cu_fw", "rtl8852cu_fw", "rtl8852cu_config", RTL8852CU}, /* RTL8852CE */
	{0xc85c, 0x8852, "mp_rtl8852cu_fw", "rtl8852cu_fw", "rtl8852cu_config", RTL8852CU}, /* RTL8852CE */
	{0x885c, 0x8852, "mp_rtl8852cu_fw", "rtl8852cu_fw", "rtl8852cu_config", RTL8852CU}, /* RTL8852CE */
	{0x886c, 0x8852, "mp_rtl8852cu_fw", "rtl8852cu_fw", "rtl8852cu_config", RTL8852CU}, /* RTL8852CE */
	{0x887c, 0x8852, "mp_rtl8852cu_fw", "rtl8852cu_fw", "rtl8852cu_config", RTL8852CU}, /* RTL8852CE */
	{0x4007, 0x8852, "mp_rtl8852cu_fw", "rtl8852cu_fw", "rtl8852cu_config", RTL8852CU}, /* RTL8852CE */

/* NOTE: must append patch entries above the null entry */
	{0, 0, NULL, NULL, NULL, 0}
};

static LIST_HEAD(dev_data_list);

void util_hexdump(const u8 *buf, size_t len)
{
	static const char hexdigits[] = "0123456789abcdef";
	char str[16 * 3];
	size_t i;

	if (!buf || !len)
		return;

	for (i = 0; i < len; i++) {
		str[((i % 16) * 3)] = hexdigits[buf[i] >> 4];
		str[((i % 16) * 3) + 1] = hexdigits[buf[i] & 0xf];
		str[((i % 16) * 3) + 2] = ' ';
		if ((i + 1) % 16 == 0) {
			str[16 * 3 - 1] = '\0';
			RTKBT_DBG("%s", str);
		}
	}

	if (i % 16 > 0) {
		str[(i % 16) * 3 - 1] = '\0';
		RTKBT_DBG("%s", str);
	}
}

#if defined RTKBT_SWITCH_PATCH || defined RTKBT_TV_POWERON_WHITELIST
int __rtk_send_hci_cmd(struct usb_device *udev, u8 *buf, u16 size)
{
	int result;
	unsigned int pipe = usb_sndctrlpipe(udev, 0);

	result = usb_control_msg(udev, pipe, 0, USB_TYPE_CLASS, 0, 0,
				 buf, size, 1000); /* 1000 msecs */

	if (result < 0)
		RTKBT_ERR("%s: Couldn't send hci cmd, err %d",
			  __func__, result);

	return result;
}

int __rtk_recv_hci_evt(struct usb_device *udev, u8 *buf, u8 len, u16 opcode)
{
	int recv_length = 0;
	int result = 0;
	int i;
	unsigned int pipe = usb_rcvintpipe(udev, 1);
	struct hci_event_hdr *hdr;
	struct hci_ev_cmd_complete *cmd_cmpl;

	if (len < sizeof(*hdr) + sizeof(*cmd_cmpl)) {
		RTKBT_ERR("%s: Invalid buf length %u", __func__, len);
		return -1;
	}

	while (1) {
		for (i = 0; i < 5; i++) {
			result = usb_interrupt_msg(udev, pipe,
					      (void *)buf, PKT_LEN,
					      &recv_length, MSG_TO);
			if (result >= 0)
				break;
		}

		if (result < 0) {
			RTKBT_ERR("%s; Couldn't receive HCI event, err %d",
				  __func__, result);
			return result;
		}

		/* Ignore the event which is not command complete event */
		if (recv_length < sizeof(*hdr) + sizeof(*cmd_cmpl))
			continue;

		hdr = (struct hci_event_hdr *)buf;
		cmd_cmpl = (struct hci_ev_cmd_complete *)(buf + sizeof(*hdr));
		if (hdr->evt == 0x0e) {
			if (opcode == cmd_cmpl->opcode)
				return recv_length;
		}
	}
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0)
static inline struct inode *file_inode(const struct file *f)
{
	return f->f_path.dentry->d_inode;
}
#endif

static int config_lists_init(void)
{
	INIT_LIST_HEAD(&list_configs);
	INIT_LIST_HEAD(&list_extracfgs);

	return 0;
}

static void config_lists_free(void)
{
	struct list_head *iter;
	struct list_head *tmp;
	struct list_head *head;
	struct cfg_list_item *n;

	if (!list_empty(&list_extracfgs))
		list_splice_tail(&list_extracfgs, &list_configs);
	head = &list_configs;
	list_for_each_safe(iter, tmp, head) {
		n = list_entry(iter, struct cfg_list_item, list);
		if (n) {
			list_del(&n->list);
			kfree(n);
		}
	}

	INIT_LIST_HEAD(&list_configs);
	INIT_LIST_HEAD(&list_extracfgs);
}

static void line_process(char *buf, int len)
{
	char *argv[32];
	int argc = 0;
	unsigned long offset;
	u8 l;
	u8 i = 0;
	char *ptr = buf;
	char *head = buf;
	struct cfg_list_item *item;

	while ((ptr = strsep(&head, ", \t")) != NULL) {
		if (!ptr[0])
			continue;
		argv[argc++] = ptr;
		if (argc >= 32) {
			RTKBT_WARN("%s: Config item is too long", __func__);
			break;
		}
	}

	if (argc < 4) {
		RTKBT_WARN("%s: Invalid Config item, ignore", __func__);
		return;
	}

	offset = simple_strtoul(argv[0], NULL, 16);
	offset = offset | (simple_strtoul(argv[1], NULL, 16) << 8);
	l = (u8)simple_strtoul(argv[2], NULL, 16);
	if (l != (u8)(argc - 3)) {
		RTKBT_ERR("invalid len %u", l);
		return;
	}

	item = kzalloc(sizeof(*item) + l, GFP_KERNEL);
	if (!item) {
		RTKBT_WARN("%s: Cannot alloc mem for item, %04lx, %u", __func__,
			   offset, l);
		return;
	}

	item->offset = (u16)offset;
	item->len = l;
	for (i = 0; i < l; i++)
		item->data[i] = (u8)simple_strtoul(argv[3 + i], NULL, 16);
	list_add_tail(&item->list, &list_extracfgs);
}

static void config_process(u8 *buff, int len)
{
	char *head = (void *)buff;
	char *ptr = (void *)buff;

	while ((ptr = strsep(&head, "\n\r")) != NULL) {
		if (!ptr[0])
			continue;
		line_process(ptr, strlen(ptr) + 1);
	}
}

static void config_file_proc(const char *path)
{
	int size;
	int rc;
	struct file *file;
	u8 tbuf[256];
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	loff_t pos = 0;
#endif

	file = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(file))
		return;

	if (!S_ISREG(file_inode(file)->i_mode))
		return;
	size = i_size_read(file_inode(file));
	if (size <= 0)
		return;

	memset(tbuf, 0, sizeof(tbuf));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	rc = kernel_read(file, tbuf, size, &pos);
#else
	rc = kernel_read(file, 0, tbuf, size);
#endif
	fput(file);
	if (rc != size) {
		if (rc >= 0)
			rc = -EIO;
		return;
	}

	tbuf[rc++] = '\n';
	tbuf[rc++] = '\0';
	config_process(tbuf, rc);
}

int patch_add(struct usb_interface *intf)
{
	dev_data *dev_entry;
	struct usb_device *udev;

	RTKBT_DBG("patch_add");
	dev_entry = dev_data_find(intf);
	if (NULL != dev_entry) {
		return -1;
	}

	udev = interface_to_usbdev(intf);
#ifdef BTUSB_RPM
	RTKBT_DBG("auto suspend is enabled");
	usb_enable_autosuspend(udev);
	pm_runtime_set_autosuspend_delay(&(udev->dev), 2000);
#else
	RTKBT_DBG("auto suspend is disabled");
	usb_disable_autosuspend(udev);
#endif

	dev_entry = kzalloc(sizeof(dev_data), GFP_KERNEL);
	dev_entry->intf = intf;
	dev_entry->udev = udev;
	dev_entry->patch_entry = get_patch_entry(udev);
	if (NULL == dev_entry->patch_entry) {
		kfree(dev_entry);
		return -1;
	}
	list_add(&dev_entry->list_node, &dev_data_list);

	/* Should reset the gEVersion to 0xff, otherwise the stored gEVersion
	 * would cause rtk_get_eversion() returning previous gEVersion if
	 * change to different ECO chip.
	 * This would cause downloading wrong patch, and the controller can't
	 * work. */
	RTKBT_DBG("%s: Reset gEVersion to 0xff", __func__);
	gEVersion = 0xff;

	return 0;
}

void patch_remove(struct usb_interface *intf)
{
	dev_data *dev_entry;
	struct usb_device *udev;

	udev = interface_to_usbdev(intf);
#ifdef BTUSB_RPM
	usb_disable_autosuspend(udev);
#endif

	dev_entry = dev_data_find(intf);
	if (NULL == dev_entry) {
		return;
	}

	RTKBT_DBG("patch_remove");
	list_del(&dev_entry->list_node);
	kfree(dev_entry);
}

static int send_reset_command(xchange_data *xdata)
{
	int ret_val;

	RTKBT_DBG("HCI reset.");

	xdata->cmd_hdr->opcode = cpu_to_le16(HCI_OP_RESET);
	xdata->cmd_hdr->plen = 0;
	xdata->pkt_len = CMD_HDR_LEN;

	ret_val = send_hci_cmd(xdata);
	if (ret_val < 0) {
		RTKBT_ERR("failed to send hci cmd.");
		return ret_val;
	}

	ret_val = rcv_hci_evt(xdata);
	if (ret_val < 0) {
		RTKBT_ERR("failed to recv hci event.");
		return ret_val;
	}

	return 0;
}

static inline int get_max_patch_size(u8 chip_type)
{
	int max_patch_size = 0;

	switch (chip_type) {
	case RTLPREVIOUS:
		max_patch_size = 24 * 1024;
		break;
	case RTL8822BU:
		max_patch_size = 25 * 1024;
		break;
	case RTL8723DU:
	case RTL8822CU:
	case RTL8761BU:
	case RTL8821CU:
		max_patch_size = 40 * 1024;
		break;
	case RTL8852AU:
		max_patch_size = 0x114D0 + 529; /* 69.2KB */
		break;
	case RTL8723FU:
		max_patch_size = 0xC4Cf + 529; /* 49.2KB */
		break;
	case RTL8852BU:
		max_patch_size = 0x104D0 + 529;  /* 65KB */
		break;
	case RTL8852CU:
		max_patch_size = 0x130D0 + 529; /* 76.2KB */
		break;
	default:
		max_patch_size = 40 * 1024;
		break;
	}

	return max_patch_size;
}

int download_patch(struct usb_interface *intf)
{
	dev_data *dev_entry;
	patch_info *pinfo;
	xchange_data *xdata = NULL;
	uint8_t *fw_buf;
	int ret_val;
	int max_patch_size = 0;

	RTKBT_DBG("download_patch start");
	dev_entry = dev_data_find(intf);
	if (NULL == dev_entry) {
		ret_val = -1;
		RTKBT_ERR("NULL == dev_entry");
		goto patch_end;
	}

	xdata = kzalloc(sizeof(xchange_data), GFP_KERNEL);
	if (NULL == xdata) {
		ret_val = -1;
		RTKBT_DBG("NULL == xdata");
		goto patch_end;
	}

	init_xdata(xdata, dev_entry);

	ret_val = check_fw_version(xdata);
	if (ret_val < 0) {
		RTKBT_ERR("Failed to get Local Version Information");
		goto patch_end;

	} else if (ret_val > 0) {
		RTKBT_DBG("Firmware already exists");
		/* Patch alread exists, just return */
		if (gEVersion == 0xff) {
			RTKBT_DBG("global_version is not set, get it!");
			gEVersion = rtk_get_eversion(dev_entry);
		}
		goto patch_end;
	}

	xdata->fw_len = load_firmware(dev_entry, &xdata->fw_data);
	if (xdata->fw_len <= 0) {
		RTKBT_ERR("load firmware failed!");
		ret_val = -1;
		goto patch_end;
	}

	fw_buf = xdata->fw_data;

	pinfo = dev_entry->patch_entry;
	if (!pinfo) {
		RTKBT_ERR("%s: No patch entry", __func__);
		ret_val = -1;
		goto patch_fail;
	}
	max_patch_size = get_max_patch_size(pinfo->chip_type);
	if (xdata->fw_len > max_patch_size) {
		RTKBT_ERR("FW/CONFIG total length larger than allowed %d",
			  max_patch_size);
		ret_val = -1;
		goto patch_fail;
	}

	ret_val = download_data(xdata);
	if (ret_val < 0) {
		RTKBT_ERR("download_data failed, err %d", ret_val);
		goto patch_fail;
	}

	ret_val = check_fw_version(xdata);
	if (ret_val <= 0) {
		RTKBT_ERR("%s: Read Local Version Info failure after download",
			  __func__);
		ret_val = -1;
		goto patch_fail;
	}

	ret_val = 0;
patch_fail:
	kfree(fw_buf);
patch_end:
	if (xdata != NULL) {
		if (xdata->send_pkt)
			kfree(xdata->send_pkt);
		if (xdata->rcv_pkt)
			kfree(xdata->rcv_pkt);
		kfree(xdata);
	}
	RTKBT_DBG("Rtk patch end %d", ret_val);
	return ret_val;
}

#ifdef RTKBT_SWITCH_PATCH
/* @return:
 * -1: error
 * 0: download patch successfully
 * >0: patch already exists  */
int download_lps_patch(struct usb_interface *intf)
{
	dev_data *dev_entry;
	xchange_data *xdata = NULL;
	uint8_t *fw_buf;
	int result;
	char name1[64];
	char *origin_name1;
	char name2[64];
	char *origin_name2;

	RTKBT_DBG("Download LPS Patch start");
	dev_entry = dev_data_find(intf);
	if (!dev_entry) {
		RTKBT_ERR("No Patch found");
		return -1;
	}

	xdata = kzalloc(sizeof(xchange_data), GFP_KERNEL);
	if (!xdata) {
		RTKBT_ERR("Couldn't alloc xdata");
		return -1;
	}

	init_xdata(xdata, dev_entry);

	result = check_fw_version(xdata);
	if (result < 0) {
		RTKBT_ERR("Failed to get Local Version Information");
		goto patch_end;

	} else if (result > 0) {
		RTKBT_DBG("Firmware already exists");
		/* Patch alread exists, just return */
		if (gEVersion == 0xff) {
			RTKBT_DBG("global_version is not set, get it!");
			gEVersion = rtk_get_eversion(dev_entry);
		}
		goto patch_end;
	}

	origin_name1 = dev_entry->patch_entry->patch_name;
	origin_name2 = dev_entry->patch_entry->config_name;
	snprintf(name1, sizeof(name1), "lps_%s", origin_name1);
	snprintf(name2, sizeof(name2), "lps_%s", origin_name2);
	dev_entry->patch_entry->patch_name = name1;
	dev_entry->patch_entry->config_name = name2;
	RTKBT_INFO("Loading %s and %s", name1, name2);
	xdata->fw_len = load_firmware(dev_entry, &xdata->fw_data);
	dev_entry->patch_entry->patch_name = origin_name1;
	dev_entry->patch_entry->config_name = origin_name2;
	if (xdata->fw_len <= 0) {
		result = -1;
		RTKBT_ERR("load firmware failed!");
		goto patch_end;
	}

	fw_buf = xdata->fw_data;

	pinfo = dev_entry->patch_entry;
	if (!pinfo) {
		RTKBT_ERR("%s: No patch entry", __func__);
		result = -1;
		goto patch_fail;
	}
	max_patch_size = get_max_patch_size(pinfo->chip_type);
	if (xdata->fw_len > max_patch_size) {
		result = -1;
		RTKBT_ERR("FW/CONFIG total length larger than allowed %d",
			  max_patch_size);
		goto patch_fail;
	}

	result = download_data(xdata);
	if (result < 0) {
		RTKBT_ERR("download_data failed, err %d", result);
		goto patch_fail;
	}

	result = check_fw_version(xdata);
	if (result <= 0) {
		RTKBT_ERR("%s: Read Local Version Info failure after download",
			  __func__);
		result = -1;
		goto patch_fail;
	}

	result = 0;

patch_fail:
	kfree(fw_buf);
patch_end:
	if (xdata->send_pkt)
		kfree(xdata->send_pkt);
	if (xdata->rcv_pkt)
		kfree(xdata->rcv_pkt);
	kfree(xdata);
	RTKBT_DBG("Download LPS Patch end %d", result);

	return result;
}
#endif

int set_scan(struct usb_interface *intf)
{
	dev_data *dev_entry;
	xchange_data *xdata = NULL;
	int result;

	RTKBT_DBG("%s", __func__);
	dev_entry = dev_data_find(intf);
	if (!dev_entry)
		return -1;

	xdata = kzalloc(sizeof(xchange_data), GFP_KERNEL);
	if (!xdata) {
		RTKBT_ERR("Could not alloc xdata");
		return -1;
	}

	init_xdata(xdata, dev_entry);

	if ( !xdata->send_pkt || !xdata->rcv_pkt ){
		result = -1;
		goto end;
	}

	xdata->cmd_hdr->opcode = cpu_to_le16(STARTSCAN_OPCODE);
	xdata->cmd_hdr->plen = 1;
	xdata->pkt_len = CMD_HDR_LEN + 1;
	xdata->send_pkt[CMD_HDR_LEN] = 1;

	result = send_hci_cmd(xdata);
	if (result < 0)
		goto end;

	result = rcv_hci_evt(xdata);
end:
	kfree(xdata->send_pkt);
	kfree(xdata->rcv_pkt);
	kfree(xdata);

	RTKBT_DBG("%s done", __func__);

	return result;
}

dev_data *dev_data_find(struct usb_interface * intf)
{
	dev_data *dev_entry;

	list_for_each_entry(dev_entry, &dev_data_list, list_node) {
		if (dev_entry->intf == intf) {
			patch_info *patch = dev_entry->patch_entry;
			if (!patch)
				return NULL;

			RTKBT_INFO("chip type value: 0x%02x", patch->chip_type);
			return dev_entry;
		}
	}

	return NULL;
}

patch_info *get_patch_entry(struct usb_device * udev)
{
	patch_info *patch_entry;
	uint16_t pid;

	patch_entry = fw_patch_table;
	pid = le16_to_cpu(udev->descriptor.idProduct);
	RTKBT_DBG("pid = 0x%x", pid);
	while (pid != patch_entry->prod_id) {
		if (0 == patch_entry->prod_id) {
			RTKBT_DBG
			    ("get_patch_entry =NULL, can not find device pid in patch_table");
			return NULL;	//break;
		}
		patch_entry++;
	}

	return patch_entry;
}

static int is_mac(u8 chip_type, u16 offset)
{
	int result = 0;

	switch (chip_type) {
	case RTL8822BU:
	case RTL8723DU:
	case RTL8821CU:
		if (offset == 0x0044)
			return 1;
		break;
	case RTL8822CU:
	case RTL8761BU:
	case RTL8852AU:
	case RTL8723FU:
	case RTL8852BU:
	case RTL8852CU:
		if (offset == 0x0030)
			return 1;
		break;
	case RTLPREVIOUS:
		if (offset == 0x003c)
			return 1;
		break;
	}

	return result;
}

static uint16_t get_mac_offset(u8 chip_type)
{
	switch (chip_type) {
	case RTL8822BU:
	case RTL8723DU:
	case RTL8821CU:
		return 0x0044;
	case RTL8822CU:
	case RTL8761BU:
	case RTL8852AU:
	case RTL8723FU:
	case RTL8852BU:
	case RTL8852CU:
		return 0x0030;
	case RTLPREVIOUS:
		return 0x003c;
	default:
		return 0x003c;
	}
}

static void merge_configs(struct list_head *head, struct list_head *head2)
{
	struct list_head *epos, *enext;
	struct list_head *pos, *next;
	struct cfg_list_item *n;
	struct cfg_list_item *extra;

	if (!head || !head2)
		return;

	if (list_empty(head2))
		return;

	if (list_empty(head)) {
		list_splice_tail(head2, head);
		INIT_LIST_HEAD(head2);
		return;
	}

	/* Add or update & replace */
	list_for_each_safe(epos, enext, head2) {
		extra = list_entry(epos, struct cfg_list_item, list);

		list_for_each_safe(pos, next, head) {
			n = list_entry(pos, struct cfg_list_item, list);
			if (extra->offset == n->offset) {
				if (extra->len < n->len) {
					/* Update the cfg data */
					RTKBT_INFO("Update cfg: ofs %04x len %u",
						   n->offset, n->len);
					memcpy(n->data, extra->data,
					       extra->len);
					list_del(epos);
					kfree(extra);
					break;
				} else {
					/* Replace the item */
					list_del(epos);
					list_replace_init(pos, epos);
					/* free the old item */
					kfree(n);
				}
			}

		}

	}

	if (list_empty(head2))
		return;
	list_for_each_safe(epos, enext, head2) {
		extra = list_entry(epos, struct cfg_list_item, list);
		RTKBT_INFO("Add new cfg: ofs %04x, len %u", extra->offset,
			   extra->len);
		/* Add the item to list */
		list_del(epos);
		list_add_tail(epos, head);
	}
}

int rtk_parse_config_file(u8 *config_buf, int filelen)
{
	struct rtk_bt_vendor_config *config = (void *)config_buf;
	u16 config_len = 0, temp = 0;
	struct rtk_bt_vendor_config_entry *entry = NULL;
	u32 i = 0;
	struct cfg_list_item *item;

	if (!config_buf)
		return -EINVAL;

	config_len = le16_to_cpu(config->data_len);
	entry = config->entry;

	if (le32_to_cpu(config->signature) != RTK_VENDOR_CONFIG_MAGIC) {
		RTKBT_ERR("sig magic num %08x,  not rtk vendor magic %08x",
			  config->signature, RTK_VENDOR_CONFIG_MAGIC);
		return -1;
	}

	if (config_len != filelen - BT_CONFIG_HDRLEN) {
		RTKBT_ERR("config length %u is not right %u", config_len,
			  (u16)(filelen - BT_CONFIG_HDRLEN));
		return -1;
	}

	for (i = 0; i < config_len;) {
		/* Add config item to list */
		item = kzalloc(sizeof(*item) + entry->entry_len, GFP_KERNEL);
		if (item) {
			item->offset = le16_to_cpu(entry->offset);
			item->len = entry->entry_len;
			memcpy(item->data, entry->entry_data, item->len);
			list_add_tail(&item->list, &list_configs);
		} else {
			RTKBT_ERR("Cannot alloc mem for entry %04x, %u",
				  entry->offset, entry->entry_len);
			break;
		}

		temp = entry->entry_len +
			sizeof(struct rtk_bt_vendor_config_entry);
		i += temp;
		entry =
		    (struct rtk_bt_vendor_config_entry *)((uint8_t *) entry +
							  temp);
	}

	return 0;;
}

uint8_t rtk_get_fw_project_id(uint8_t * p_buf)
{
	uint8_t opcode;
	uint8_t len;
	uint8_t data = 0;

	do {
		opcode = *p_buf;
		len = *(p_buf - 1);
		if (opcode == 0x00) {
			if (len == 1) {
				data = *(p_buf - 2);
				RTKBT_DBG
				    ("rtk_get_fw_project_id: opcode %d, len %d, data %d",
				     opcode, len, data);
				break;
			} else {
				RTKBT_ERR
				    ("rtk_get_fw_project_id: invalid len %d",
				     len);
			}
		}
		p_buf -= len + 2;
	} while (*p_buf != 0xFF);

	return data;
}

static void rtk_get_patch_entry(uint8_t * epatch_buf,
				struct rtk_epatch_entry *entry)
{
	uint32_t svn_ver;
	uint32_t coex_ver;
	uint32_t tmp;
	uint16_t i;
	struct rtk_epatch *epatch_info = (struct rtk_epatch *)epatch_buf;

	epatch_info->number_of_total_patch =
	    le16_to_cpu(epatch_info->number_of_total_patch);
	RTKBT_DBG("fw_version = 0x%x", le32_to_cpu(epatch_info->fw_version));
	RTKBT_DBG("number_of_total_patch = %d",
		  epatch_info->number_of_total_patch);

	/* get right epatch entry */
	for (i = 0; i < epatch_info->number_of_total_patch; i++) {
		if (get_unaligned_le16(epatch_buf + 14 + 2 * i) ==
		    gEVersion + 1) {
			entry->chipID = gEVersion + 1;
			entry->patch_length = get_unaligned_le16(epatch_buf +
					14 +
					2 * epatch_info->number_of_total_patch +
					2 * i);
			entry->start_offset = get_unaligned_le32(epatch_buf +
					14 +
					4 * epatch_info-> number_of_total_patch +
					4 * i);
			break;
		}
	}

	if (i >= epatch_info->number_of_total_patch) {
		entry->patch_length = 0;
		entry->start_offset = 0;
		RTKBT_ERR("No corresponding patch found\n");
		return;
	}

	svn_ver = get_unaligned_le32(epatch_buf +
				entry->start_offset +
				entry->patch_length - 8);
	coex_ver = get_unaligned_le32(epatch_buf +
				entry->start_offset +
				entry->patch_length - 12);

	RTKBT_DBG("chipID %d", entry->chipID);
	RTKBT_DBG("patch_length 0x%04x", entry->patch_length);
	RTKBT_DBG("start_offset 0x%08x", entry->start_offset);

	RTKBT_DBG("Svn version: %8d", svn_ver);
	tmp = ((coex_ver >> 16) & 0x7ff) + (coex_ver >> 27) * 10000;
	RTKBT_DBG("Coexistence: BTCOEX_20%06d-%04x",
		  tmp, (coex_ver & 0xffff));
}

int bachk(const char *str)
{
	if (!str)
		return -1;

	if (strlen(str) != 17)
		return -1;

	while (*str) {
		if (!isxdigit(*str++))
			return -1;

		if (!isxdigit(*str++))
			return -1;

		if (*str == 0)
			break;

		if (*str++ != ':')
			return -1;
	}

	return 0;
}

static int request_bdaddr(u8 *buf)
{
	int size;
	int rc;
	struct file *file;
	u8 tbuf[BDADDR_STRING_LEN + 1];
	char *str;
	int i;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	loff_t pos = 0;
#endif

	if (!buf)
		return -EINVAL;

	file = filp_open(BDADDR_FILE, O_RDONLY, 0);
	if (IS_ERR(file))
		return -ENOENT;

	if (!S_ISREG(file_inode(file)->i_mode))
		return -EINVAL;
	size = i_size_read(file_inode(file));
	if (size <= 0)
		return -EINVAL;

	if (size > BDADDR_STRING_LEN)
		size = BDADDR_STRING_LEN;

	memset(tbuf, 0, sizeof(tbuf));
	RTKBT_INFO("size = %d", size);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	rc = kernel_read(file, tbuf, size, &pos);
#else
	rc = kernel_read(file, 0, tbuf, size);
#endif
	fput(file);
	if (rc != size) {
		if (rc >= 0)
			rc = -EIO;
		goto fail;
	}

	if (bachk(tbuf) < 0) {
		rc = -EINVAL;
		goto fail;
	}

	str = tbuf;
	for (i = 5; i >= 0; i--) {
		buf[i] = simple_strtol(str, NULL, 16);
		str += 3;
	}

	return size;
fail:
	return rc;
}

static u8 *load_config(dev_data *dev_entry, int *length)
{
	patch_info *patch_entry;
	const char *config_name;
	const struct firmware *fw;
	struct usb_device *udev;
	int result;
	u8 *buf;
	u8 *p;
	u16 config_len;
	u16 dlen;
	u8 tmp_buf[32];
	int file_sz;
	struct cfg_list_item *n;
	struct list_head *pos, *next;
	u8 chip_type;

	config_lists_init();
	patch_entry = dev_entry->patch_entry;
	config_name = patch_entry->config_name;
	udev = dev_entry->udev;
	chip_type = patch_entry->chip_type;

	RTKBT_INFO("config filename %s", config_name);
	result = request_firmware(&fw, config_name, &udev->dev);
	if (result < 0)
		return 0;

	file_sz = fw->size;
	buf = (u8 *)fw->data;

	/* Load extra configs */
	config_file_proc(EXTRA_CONFIG_FILE);
	list_for_each_safe(pos, next, &list_extracfgs) {
		n = list_entry(pos, struct cfg_list_item, list);
		RTKBT_INFO("extra cfg: ofs %04x, len %u", n->offset, n->len);
	}

	/* Load extra bdaddr config */
	memset(tmp_buf, 0, sizeof(tmp_buf));
	result = request_bdaddr(tmp_buf);
	if (result > 0) {
		n = kzalloc(sizeof(*n) + 6, GFP_KERNEL);
		if (n) {
			n->offset = get_mac_offset(patch_entry->chip_type);
			n->len = 6;
			memcpy(n->data, tmp_buf, 6);
			list_add_tail(&n->list, &list_extracfgs);
		} else {
			RTKBT_WARN("Couldn't alloc mem for bdaddr");
		}
	} else {
		if (result == -ENOENT)
			RTKBT_WARN("no bdaddr file %s", BDADDR_FILE);
		else
			RTKBT_WARN("invalid customer bdaddr %d", result);
	}

	RTKBT_INFO("Origin cfg len %u", (u16)file_sz);
	util_hexdump((const u8 *)buf, file_sz);

	result = rtk_parse_config_file(buf, file_sz);
	if (result < 0) {
		RTKBT_ERR("Parse config file error");
		buf = NULL;
		goto done;
	}

	merge_configs(&list_configs, &list_extracfgs);

	/* Calculate the config_len */
	config_len = 4; /* magic word length */
	config_len += 2; /* data length field */
	dlen = 0;
	list_for_each_safe(pos, next, &list_configs) {
		n = list_entry(pos, struct cfg_list_item, list);
		switch (n->offset) {
		case 0x003c:
		case 0x0030:
		case 0x0044:
			if (is_mac(chip_type, n->offset) && n->len == 6) {
				char s[18];
				sprintf(s, "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
					n->data[5], n->data[4],
					n->data[3], n->data[2],
					n->data[1], n->data[0]);
				RTKBT_INFO("bdaddr ofs %04x, %s", n->offset, s);
			}
			break;
		default:
			break;
		}

		config_len += (3 + n->len);
		dlen += (3 + n->len);
	}


	buf = kzalloc(config_len, GFP_KERNEL);
	if (!buf) {
		RTKBT_ERR("Couldn't alloc buf for configs");
		goto done;
	}

	/* Save configs to a buffer */
	memcpy(buf, cfg_magic, 4);
	buf[4] = dlen & 0xff;
	buf[5] = (dlen >> 8) & 0xff;
	p = buf + 6;
	list_for_each_safe(pos, next, &list_configs) {
		n = list_entry(pos, struct cfg_list_item, list);
		p[0] = n->offset & 0xff;
		p[1] = (n->offset >> 8) & 0xff;
		p[2] = n->len;
		memcpy(p + 3, n->data, n->len);
		p += (3 + n->len);
	}

	RTKBT_INFO("New cfg len %u", config_len);
	util_hexdump((const u8 *)buf, config_len);

	*length = config_len;

done:
	config_lists_free();
	release_firmware(fw);

	return buf;
}

int load_firmware(dev_data * dev_entry, uint8_t ** buff)
{
	const struct firmware *fw;
	struct usb_device *udev;
	patch_info *patch_entry;
	char *fw_name;
	int fw_len = 0, ret_val = 0, config_len = 0, buf_len = -1;
	uint8_t *buf = NULL, *config_file_buf = NULL, *epatch_buf = NULL;
	uint8_t proj_id = 0;
	uint8_t need_download_fw = 1;
	uint16_t lmp_version;
	struct rtk_epatch_entry current_entry = { 0 };

	RTKBT_DBG("load_firmware start");
	udev = dev_entry->udev;
	patch_entry = dev_entry->patch_entry;
	lmp_version = patch_entry->lmp_sub;
	RTKBT_DBG("lmp_version = 0x%04x", lmp_version);

	config_file_buf = load_config(dev_entry, &config_len);

	fw_name = patch_entry->patch_name;
	RTKBT_ERR("fw name is  %s", fw_name);
	ret_val = request_firmware(&fw, fw_name, &udev->dev);
	if (ret_val < 0) {
		fw_len = 0;
		kfree(config_file_buf);
		config_file_buf = NULL;
		goto fw_fail;
	}

	epatch_buf = kzalloc(fw->size, GFP_KERNEL);
	if (NULL == epatch_buf)
		goto alloc_fail;

	memcpy(epatch_buf, fw->data, fw->size);
	buf_len = fw->size + config_len;

	if (lmp_version == ROM_LMP_8723a) {
		RTKBT_DBG("This is 8723a, use old patch style!");

		if (memcmp(epatch_buf, RTK_EPATCH_SIGNATURE, 8) == 0) {
			RTKBT_ERR("8723a Check signature error!");
			need_download_fw = 0;
		} else {
			if (!(buf = kzalloc(buf_len, GFP_KERNEL))) {
				RTKBT_ERR("Can't alloc memory for fw&config");
				buf_len = -1;
			} else {
				RTKBT_DBG("8723a, fw copy direct");
				memcpy(buf, epatch_buf, fw->size);
				if (config_len) {
					memcpy(&buf[buf_len - config_len],
					       config_file_buf, config_len);
				}
			}
		}
	} else {
		RTKBT_ERR("This is not 8723a, use new patch style!");

		/* Get version from ROM */
		gEVersion = rtk_get_eversion(dev_entry);
		RTKBT_DBG("%s: New gEVersion %d", __func__, gEVersion);
		if (gEVersion == 0xFE) {
			RTKBT_ERR("%s: Read ROM version failure", __func__);
			need_download_fw = 0;
			fw_len = 0;
			goto alloc_fail;
		}

		/* check Signature and Extension Section Field */
		if ((memcmp(epatch_buf, RTK_EPATCH_SIGNATURE, 8) != 0) ||
		    memcmp(epatch_buf + buf_len - config_len - 4,
			   Extension_Section_SIGNATURE, 4) != 0) {
			RTKBT_ERR("Check SIGNATURE error! do not download fw");
			need_download_fw = 0;
		} else {
			proj_id =
			    rtk_get_fw_project_id(epatch_buf + buf_len -
						  config_len - 5);

			if (lmp_version != project_id[proj_id]) {
				RTKBT_ERR
				    ("lmp_version is %x, project_id is %x, does not match!!!",
				     lmp_version, project_id[proj_id]);
				need_download_fw = 0;
			} else {
				RTKBT_DBG
				    ("lmp_version is %x, project_id is %x, match!",
				     lmp_version, project_id[proj_id]);
				rtk_get_patch_entry(epatch_buf, &current_entry);

				if (current_entry.patch_length == 0)
					goto alloc_fail;

				buf_len =
				    current_entry.patch_length + config_len;
				RTKBT_DBG("buf_len = 0x%x", buf_len);

				if (!(buf = kzalloc(buf_len, GFP_KERNEL))) {
					RTKBT_ERR
					    ("Can't alloc memory for multi fw&config");
					buf_len = -1;
				} else {
					memcpy(buf,
					       epatch_buf +
					       current_entry.start_offset,
					       current_entry.patch_length);
					memcpy(buf + current_entry.patch_length - 4, epatch_buf + 8, 4);	/*fw version */
					if (config_len) {
						memcpy(&buf
						       [buf_len - config_len],
						       config_file_buf,
						       config_len);
					}
				}
			}
		}
	}

	RTKBT_DBG("fw:%s exists, config file:%s exists",
		  (buf_len > 0) ? "" : "not", (config_len > 0) ? "" : "not");
	if (buf && (buf_len > 0) && (need_download_fw)) {
		fw_len = buf_len;
		*buff = buf;
	}

	RTKBT_DBG("load_firmware done");

alloc_fail:
	release_firmware(fw);

	if (epatch_buf)
		kfree(epatch_buf);

	if (config_file_buf)
		kfree(config_file_buf);
fw_fail:
	if (fw_len == 0)
		kfree(buf);

	return fw_len;
}

void init_xdata(xchange_data * xdata, dev_data * dev_entry)
{
	memset(xdata, 0, sizeof(xchange_data));
	xdata->dev_entry = dev_entry;
	xdata->pipe_in = usb_rcvintpipe(dev_entry->udev, INTR_EP);
	xdata->pipe_out = usb_sndctrlpipe(dev_entry->udev, CTRL_EP);
	xdata->send_pkt = kzalloc(PKT_LEN, GFP_KERNEL);
	xdata->rcv_pkt = kzalloc(PKT_LEN, GFP_KERNEL);
	xdata->cmd_hdr = (struct hci_command_hdr *)(xdata->send_pkt);
	xdata->evt_hdr = (struct hci_event_hdr *)(xdata->rcv_pkt);
	xdata->cmd_cmp =
	    (struct hci_ev_cmd_complete *)(xdata->rcv_pkt + EVT_HDR_LEN);
	xdata->req_para = xdata->send_pkt + CMD_HDR_LEN;
	xdata->rsp_para = xdata->rcv_pkt + EVT_HDR_LEN + CMD_CMP_LEN;
}

int check_fw_version(xchange_data * xdata)
{
	struct hci_rp_read_local_version *read_ver_rsp;
	patch_info *patch_entry;
	int ret_val;
	int retry = 0;

	/* Ensure that the first cmd is hci reset after system suspend
	 * or system reboot */
	send_reset_command(xdata);

get_ver:
	xdata->cmd_hdr->opcode = cpu_to_le16(HCI_OP_READ_LOCAL_VERSION);
	xdata->cmd_hdr->plen = 0;
	xdata->pkt_len = CMD_HDR_LEN;

	ret_val = send_hci_cmd(xdata);
	if (ret_val < 0) {
		RTKBT_ERR("%s: Failed to send HCI command.", __func__);
		goto version_end;
	}

	ret_val = rcv_hci_evt(xdata);
	if (ret_val < 0) {
		RTKBT_ERR("%s: Failed to receive HCI event.", __func__);
		goto version_end;
	}

	patch_entry = xdata->dev_entry->patch_entry;
	read_ver_rsp = (struct hci_rp_read_local_version *)(xdata->rsp_para);
	read_ver_rsp->lmp_subver = le16_to_cpu(read_ver_rsp->lmp_subver);
	read_ver_rsp->hci_rev = le16_to_cpu(read_ver_rsp->hci_rev);
	read_ver_rsp->manufacturer = le16_to_cpu(read_ver_rsp->manufacturer);

	RTKBT_DBG("read_ver_rsp->lmp_subver = 0x%x", read_ver_rsp->lmp_subver);
	RTKBT_DBG("read_ver_rsp->hci_rev = 0x%x", read_ver_rsp->hci_rev);
	RTKBT_DBG("patch_entry->lmp_sub = 0x%x", patch_entry->lmp_sub);
	if (patch_entry->lmp_sub != read_ver_rsp->lmp_subver) {
		return 1;
	}

	ret_val = 0;
version_end:
	if (ret_val) {
		send_reset_command(xdata);
		retry++;
		if (retry < 2)
			goto get_ver;
	}

	return ret_val;
}

uint8_t rtk_get_eversion(dev_data * dev_entry)
{
	struct rtk_eversion_evt *eversion;
	patch_info *patch_entry;
	int ret_val = 0;
	xchange_data *xdata = NULL;

	RTKBT_DBG("%s: gEVersion %d", __func__, gEVersion);
	if (gEVersion != 0xFF && gEVersion != 0xFE) {
		RTKBT_DBG("gEVersion != 0xFF, return it directly!");
		return gEVersion;
	}

	xdata = kzalloc(sizeof(xchange_data), GFP_KERNEL);
	if (NULL == xdata) {
		ret_val = 0xFE;
		RTKBT_DBG("NULL == xdata");
		return ret_val;
	}

	init_xdata(xdata, dev_entry);

	xdata->cmd_hdr->opcode = cpu_to_le16(HCI_VENDOR_READ_RTK_ROM_VERISION);
	xdata->cmd_hdr->plen = 0;
	xdata->pkt_len = CMD_HDR_LEN;

	ret_val = send_hci_cmd(xdata);
	if (ret_val < 0) {
		RTKBT_ERR("Failed to send read RTK rom version cmd.");
		ret_val = 0xFE;
		goto version_end;
	}

	ret_val = rcv_hci_evt(xdata);
	if (ret_val < 0) {
		RTKBT_ERR("Failed to receive HCI event for rom version.");
		ret_val = 0xFE;
		goto version_end;
	}

	patch_entry = xdata->dev_entry->patch_entry;
	eversion = (struct rtk_eversion_evt *)(xdata->rsp_para);
	RTKBT_DBG("eversion->status = 0x%x, eversion->version = 0x%x",
		  eversion->status, eversion->version);
	if (eversion->status) {
		ret_val = 0;
		//global_eversion = 0;
	} else {
		ret_val = eversion->version;
		//global_eversion = eversion->version;
	}

version_end:
	if (xdata != NULL) {
		if (xdata->send_pkt)
			kfree(xdata->send_pkt);
		if (xdata->rcv_pkt)
			kfree(xdata->rcv_pkt);
		kfree(xdata);
	}
	return ret_val;
}

int download_data(xchange_data * xdata)
{
	download_cp *cmd_para;
	download_rp *evt_para;
	uint8_t *pcur;
	int pkt_len, frag_num, frag_len;
	int i, ret_val;
	int j;

	RTKBT_DBG("download_data start");

	cmd_para = (download_cp *) xdata->req_para;
	evt_para = (download_rp *) xdata->rsp_para;
	pcur = xdata->fw_data;
	pkt_len = CMD_HDR_LEN + sizeof(download_cp);
	frag_num = xdata->fw_len / PATCH_SEG_MAX + 1;
	frag_len = PATCH_SEG_MAX;

	for (i = 0; i < frag_num; i++) {
		if (i > 0x7f)
			j = (i & 0x7f) + 1;
		else
			j = i;

		cmd_para->index = j;
		if (i == (frag_num - 1)) {
			cmd_para->index |= DATA_END;
			frag_len = xdata->fw_len % PATCH_SEG_MAX;
			pkt_len -= (PATCH_SEG_MAX - frag_len);
		}
		xdata->cmd_hdr->opcode = cpu_to_le16(DOWNLOAD_OPCODE);
		xdata->cmd_hdr->plen = sizeof(uint8_t) + frag_len;
		xdata->pkt_len = pkt_len;
		memcpy(cmd_para->data, pcur, frag_len);

		ret_val = send_hci_cmd(xdata);
		if (ret_val < 0) {
			return ret_val;
		}

		ret_val = rcv_hci_evt(xdata);
		if (ret_val < 0) {
			return ret_val;
		}

		if (0 != evt_para->status) {
			return -1;
		}

		pcur += PATCH_SEG_MAX;
	}

	RTKBT_DBG("download_data done");
	return xdata->fw_len;
}

int send_hci_cmd(xchange_data * xdata)
{
	int ret_val;

	ret_val = usb_control_msg(xdata->dev_entry->udev, xdata->pipe_out,
				  0, USB_TYPE_CLASS, 0, 0,
				  (void *)(xdata->send_pkt),
				  xdata->pkt_len, MSG_TO);

	if (ret_val < 0)
		RTKBT_ERR("%s; failed to send ctl msg for hci cmd, err %d",
			  __func__, ret_val);

	return ret_val;
}

int rcv_hci_evt(xchange_data * xdata)
{
	int ret_len = 0, ret_val = 0;
	int i;			// Added by Realtek

	while (1) {
		// **************************** Modifed by Realtek (begin)
		for (i = 0; i < 5; i++)	// Try to send USB interrupt message 5 times.
		{
			ret_val =
			    usb_interrupt_msg(xdata->dev_entry->udev,
					      xdata->pipe_in,
					      (void *)(xdata->rcv_pkt), PKT_LEN,
					      &ret_len, MSG_TO);
			if (ret_val >= 0)
				break;
		}
		// **************************** Modifed by Realtek (end)

		if (ret_val < 0) {
			RTKBT_ERR("%s; no usb intr msg for hci event, err %d",
				  __func__, ret_val);
			return ret_val;
		}

		if (CMD_CMP_EVT == xdata->evt_hdr->evt) {
			if (xdata->cmd_hdr->opcode == xdata->cmd_cmp->opcode)
				return ret_len;
		}
	}
}

void print_acl(struct sk_buff *skb, int dataOut)
{
#if PRINT_ACL_DATA
	uint wlength = skb->len;
	uint icount = 0;
	u16 *handle = (u16 *) (skb->data);
	u16 dataLen = *(handle + 1);
	u8 *acl_data = (u8 *) (skb->data);
//if (0==dataOut)
	printk("%d handle:%04x,len:%d,", dataOut, *handle, dataLen);
//else
//      printk("In handle:%04x,len:%d,",*handle,dataLen);
/*	for(icount=4;(icount<wlength)&&(icount<32);icount++)
		{
			printk("%02x ",*(acl_data+icount) );
		}
	printk("\n");
*/
#endif
}

void print_command(struct sk_buff *skb)
{
#if PRINT_CMD_EVENT
	uint wlength = skb->len;
	uint icount = 0;
	u16 *opcode = (u16 *) (skb->data);
	u8 *cmd_data = (u8 *) (skb->data);
	u8 paramLen = *(cmd_data + 2);

	switch (*opcode) {
	case HCI_OP_INQUIRY:
		printk("HCI_OP_INQUIRY");
		break;
	case HCI_OP_INQUIRY_CANCEL:
		printk("HCI_OP_INQUIRY_CANCEL");
		break;
	case HCI_OP_EXIT_PERIODIC_INQ:
		printk("HCI_OP_EXIT_PERIODIC_INQ");
		break;
	case HCI_OP_CREATE_CONN:
		printk("HCI_OP_CREATE_CONN");
		break;
	case HCI_OP_DISCONNECT:
		printk("HCI_OP_DISCONNECT");
		break;
	case HCI_OP_CREATE_CONN_CANCEL:
		printk("HCI_OP_CREATE_CONN_CANCEL");
		break;
	case HCI_OP_ACCEPT_CONN_REQ:
		printk("HCI_OP_ACCEPT_CONN_REQ");
		break;
	case HCI_OP_REJECT_CONN_REQ:
		printk("HCI_OP_REJECT_CONN_REQ");
		break;
	case HCI_OP_AUTH_REQUESTED:
		printk("HCI_OP_AUTH_REQUESTED");
		break;
	case HCI_OP_SET_CONN_ENCRYPT:
		printk("HCI_OP_SET_CONN_ENCRYPT");
		break;
	case HCI_OP_REMOTE_NAME_REQ:
		printk("HCI_OP_REMOTE_NAME_REQ");
		break;
	case HCI_OP_READ_REMOTE_FEATURES:
		printk("HCI_OP_READ_REMOTE_FEATURES");
		break;
	case HCI_OP_SNIFF_MODE:
		printk("HCI_OP_SNIFF_MODE");
		break;
	case HCI_OP_EXIT_SNIFF_MODE:
		printk("HCI_OP_EXIT_SNIFF_MODE");
		break;
	case HCI_OP_SWITCH_ROLE:
		printk("HCI_OP_SWITCH_ROLE");
		break;
	case HCI_OP_SNIFF_SUBRATE:
		printk("HCI_OP_SNIFF_SUBRATE");
		break;
	case HCI_OP_RESET:
		printk("HCI_OP_RESET");
		break;
	default:
		printk("CMD");
		break;
	}
	printk(":%04x,len:%d,", *opcode, paramLen);
	for (icount = 3; (icount < wlength) && (icount < 24); icount++) {
		printk("%02x ", *(cmd_data + icount));
	}
	printk("\n");

#endif
}

void print_event(struct sk_buff *skb)
{
#if PRINT_CMD_EVENT
	uint wlength = skb->len;
	uint icount = 0;
	u8 *opcode = (u8 *) (skb->data);
	u8 paramLen = *(opcode + 1);

	switch (*opcode) {
	case HCI_EV_INQUIRY_COMPLETE:
		printk("HCI_EV_INQUIRY_COMPLETE");
		break;
	case HCI_EV_INQUIRY_RESULT:
		printk("HCI_EV_INQUIRY_RESULT");
		break;
	case HCI_EV_CONN_COMPLETE:
		printk("HCI_EV_CONN_COMPLETE");
		break;
	case HCI_EV_CONN_REQUEST:
		printk("HCI_EV_CONN_REQUEST");
		break;
	case HCI_EV_DISCONN_COMPLETE:
		printk("HCI_EV_DISCONN_COMPLETE");
		break;
	case HCI_EV_AUTH_COMPLETE:
		printk("HCI_EV_AUTH_COMPLETE");
		break;
	case HCI_EV_REMOTE_NAME:
		printk("HCI_EV_REMOTE_NAME");
		break;
	case HCI_EV_ENCRYPT_CHANGE:
		printk("HCI_EV_ENCRYPT_CHANGE");
		break;
	case HCI_EV_CHANGE_LINK_KEY_COMPLETE:
		printk("HCI_EV_CHANGE_LINK_KEY_COMPLETE");
		break;
	case HCI_EV_REMOTE_FEATURES:
		printk("HCI_EV_REMOTE_FEATURES");
		break;
	case HCI_EV_REMOTE_VERSION:
		printk("HCI_EV_REMOTE_VERSION");
		break;
	case HCI_EV_QOS_SETUP_COMPLETE:
		printk("HCI_EV_QOS_SETUP_COMPLETE");
		break;
	case HCI_EV_CMD_COMPLETE:
		printk("HCI_EV_CMD_COMPLETE");
		break;
	case HCI_EV_CMD_STATUS:
		printk("HCI_EV_CMD_STATUS");
		break;
	case HCI_EV_ROLE_CHANGE:
		printk("HCI_EV_ROLE_CHANGE");
		break;
	case HCI_EV_NUM_COMP_PKTS:
		printk("HCI_EV_NUM_COMP_PKTS");
		break;
	case HCI_EV_MODE_CHANGE:
		printk("HCI_EV_MODE_CHANGE");
		break;
	case HCI_EV_PIN_CODE_REQ:
		printk("HCI_EV_PIN_CODE_REQ");
		break;
	case HCI_EV_LINK_KEY_REQ:
		printk("HCI_EV_LINK_KEY_REQ");
		break;
	case HCI_EV_LINK_KEY_NOTIFY:
		printk("HCI_EV_LINK_KEY_NOTIFY");
		break;
	case HCI_EV_CLOCK_OFFSET:
		printk("HCI_EV_CLOCK_OFFSET");
		break;
	case HCI_EV_PKT_TYPE_CHANGE:
		printk("HCI_EV_PKT_TYPE_CHANGE");
		break;
	case HCI_EV_PSCAN_REP_MODE:
		printk("HCI_EV_PSCAN_REP_MODE");
		break;
	case HCI_EV_INQUIRY_RESULT_WITH_RSSI:
		printk("HCI_EV_INQUIRY_RESULT_WITH_RSSI");
		break;
	case HCI_EV_REMOTE_EXT_FEATURES:
		printk("HCI_EV_REMOTE_EXT_FEATURES");
		break;
	case HCI_EV_SYNC_CONN_COMPLETE:
		printk("HCI_EV_SYNC_CONN_COMPLETE");
		break;
	case HCI_EV_SYNC_CONN_CHANGED:
		printk("HCI_EV_SYNC_CONN_CHANGED");
		break;
	case HCI_EV_SNIFF_SUBRATE:
		printk("HCI_EV_SNIFF_SUBRATE");
		break;
	case HCI_EV_EXTENDED_INQUIRY_RESULT:
		printk("HCI_EV_EXTENDED_INQUIRY_RESULT");
		break;
	case HCI_EV_IO_CAPA_REQUEST:
		printk("HCI_EV_IO_CAPA_REQUEST");
		break;
	case HCI_EV_SIMPLE_PAIR_COMPLETE:
		printk("HCI_EV_SIMPLE_PAIR_COMPLETE");
		break;
	case HCI_EV_REMOTE_HOST_FEATURES:
		printk("HCI_EV_REMOTE_HOST_FEATURES");
		break;
	default:
		printk("event");
		break;
	}
	printk(":%02x,len:%d,", *opcode, paramLen);
	for (icount = 2; (icount < wlength) && (icount < 24); icount++) {
		printk("%02x ", *(opcode + icount));
	}
	printk("\n");

#endif
}

