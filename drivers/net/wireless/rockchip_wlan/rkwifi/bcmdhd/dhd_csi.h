/*
 * Broadcom Dongle Host Driver (DHD), Linux-specific network interface.
 * Basically selected code segments from usb-cdc.c and usb-rndis.c
 *
 * Copyright (C) 2023, Broadcom.
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

#ifndef __DHD_CSI_H__
#define __DHD_CSI_H__

/* definition for structure and API between APPP and driver */

/* **** SYNA CSI header interface with driver **** */

enum syna_csi_data_mode_type {
	SYNA_CSI_DATA_MODE_NONE = 0,
	/* upper layer will fetch data by sysfs */
	SYNA_CSI_DATA_MODE_SYSFS,
	/* upper layer will fetch data by UDP socket */
	SYNA_CSI_DATA_MODE_UDP_SOCKET,
	SYNA_CSI_DATA_MODE_LAST
};

enum syna_csi_format_type {
	SYNA_CSI_FORMAT_UNKNOWN = 0,
	SYNA_CSI_FORMAT_Q8,  /* (9,9,5) Q8 floating point
	                      * BCM4358/4359
	                      */
	SYNA_CSI_FORMAT_Q13, /* (14,14) Q13 fixed point
	                      * BCM4339/43455/43456/43458
	                      */
	SYNA_CSI_FORMAT_Q11, /* (12,12,6) Q11 floating point
	                      * BCM4360/4366
	                      */
	SYNA_CSI_FORMAT_LAST
};
/* (9,9,5) Q8 floating point data format */
#define _Q8_REAL_MA(t)         (0x000ff &((t)>>14))
#define _Q8_REAL_SI(t)         (0x00001 &((t)>>22))
#define _Q8_IMAGE_MA(t)        (0x000ff &((t)>> 5))
#define _Q8_IMAGE_SI(t)        (0x00001 &((t)>>13))
#define SYNA_CSI_DATA_Q8_REAL(t) \
(((0 < _Q8_REAL_SI(t)) \
	? (-1) \
	: (1)) \
	* ((int32)_Q8_REAL_MA(t)) \
)
#define SYNA_CSI_DATA_Q8_IMAGE(t) \
(((0 < _Q8_IMAGE_SI(t)) \
	? (-1) \
	: (1)) \
	* ((int32)_Q8_IMAGE_MA(t)) \
)
#define SYNA_CSI_DATA_Q8_EXP(t) \
(((int8)(0xf8 \
	& ((t) << 3))) \
	>> 3 \
)
/* (14,14) Q13 fixed point data format */
#define SYNA_CSI_DATA_Q13_REAL(t) \
(((int32)(0xfff30000 \
	& ((t) << 4))) \
	>> (14 + 4) \
)
#define SYNA_CSI_DATA_Q13_IMAGE(t) \
(((int32)(0xfff30000 \
	& ((t) << 18))) \
	>> (0 + 18) \
)
/* (12,12,6) Q11 floating point */
#define _Q11_REAL_MA(t)        (0x007ff &((t)>>18))
#define _Q11_REAL_SI(t)        (0x00001 &((t)>>29))
#define _Q11_IMAGE_MA(t)       (0x007ff &((t)>> 6))
#define _Q11_IMAGE_SI(t)       (0x00001 &((t)>>17))
#define SYNA_CSI_DATA_Q11_REAL(t) \
(((0 < _Q11_REAL_SI(t)) \
	? (-1) \
	: (1)) \
	* ((int32)_Q11_REAL_MA(t)) \
)
#define SYNA_CSI_DATA_Q11_IMAGE(t) \
(((0 < _Q11_IMAGE_SI(t)) \
	? (-1) \
	: (1)) \
	* ((int32)_Q11_IMAGE_MA(t)) \
)
#define SYNA_CSI_DATA_Q11_EXP(t) \
(((int8)(0xfc \
	& ((t) << 2))) \
	>> 2 \
)

enum syna_csi_error_status {
	SYNA_CSI_ERROR_NONE,
	SYNA_CSI_ERROR_GENERIC,  /* generic error */
	SYNA_CSI_ERROR_NO_ACK,   /* peer doesn't ACK */
	SYNA_CSI_ERROR_READ,     /* read data error */
	SYNA_CSI_ERROR_PS,       /* peer device under powersaving */
	SYNA_CSI_ERROR_LAST
};

enum syna_csi_flag {
	/* bit 0 ~ bit 15 are for error information */
	SYNA_CSI_FLAG_ERROR_MASK = 0x0FFFF,
	SYNA_CSI_FLAG_ERROR_CHECKSUM = (1 << 0),
	SYNA_CSI_FLAG_ERROR_GENERIC = (1 << SYNA_CSI_ERROR_GENERIC),
	SYNA_CSI_FLAG_ERROR_NO_ACK = (1 << SYNA_CSI_ERROR_NO_ACK),
	SYNA_CSI_FLAG_ERROR_READ = (1 << SYNA_CSI_ERROR_READ),
	SYNA_CSI_FLAG_ERROR_PS = (1 << SYNA_CSI_ERROR_PS),

	/* subcarrier real position along FFT index: (total N=2*M subcarries)
	 * -M, -(M-1), -(M-2), -(M-3),...,-1, 0, 1, 2,...M-1, M
	 *
	 *  Data Output order with positive part first(*default*):
	 * 0, 1, 2,...M-1, M,   -M, -(M-1), -(M-2), -(M-3),...,-1,
	 *
	 *  Data Output order with negative part first:
	 * -M, -(M-1), -(M-2), -(M-3),...,-1, 0, 1, 2,...M-1, M
	 */
	SYNA_CSI_FLAG_FFT_NEGATIVE_FIRST = (1 << 16),
	SYNA_CSI_FLAG_LAST
};

enum syna_csi_band_type {
	SYNA_CSI_BAND_UNKNOWN = 0,
	SYNA_CSI_BAND_2G,
	SYNA_CSI_BAND_5G,
	SYNA_CSI_BAND_6G,
	SYNA_CSI_BAND_LAST
};

enum syna_csi_bandwidth_type {
	SYNA_CSI_BW_UNKNOWN = 0x000,
	SYNA_CSI_BW_5MHz    = 0x001,
	SYNA_CSI_BW_10MHz   = 0x002,
	SYNA_CSI_BW_20MHz   = 0x004,
	SYNA_CSI_BW_40MHz   = 0x008,
	SYNA_CSI_BW_80MHz   = 0x010,
	SYNA_CSI_BW_160MHz  = 0x020,
	SYNA_CSI_BW_320MHz  = 0x040,
	SYNA_CSI_BW_LAST
};

/* 'SYNA' -> 'A'53, 'N'59, 'Y'4e, 'S'41 */
#define CONST_SYNA_CSI_MAGIC_FLAG    0x414E5953

/* CSI API common header structure (8 bytes alignment for 64bit system) */
/* below is version 1 'syna_csi_header' */
#define CONST_SYNA_CSI_COMMON_HEADER_VERSION    0x01
struct syna_csi_header {
	/* byte 0 ~ 3 */
	uint32  magic_flag;       /* flag for protecting and detecting header */

	/* byte 4 ~ 7 */
	uint8  version;           /* header version of this structure */
	uint8  format_type;       /* data format type of current CSI packet,
	                           * enumerated in 'syna_csi_format_type'
	                           */
	uint8  fc;                /* packet frame control type of current
	                           * CSI data for further checking if it
	                           * is correct expected result
	                           */
	uint8  flags;             /* flag may use for extra emendation
	                           * which enumerated as 'syna_csi_flag'
	                           */

	/* byte 8 ~ 19 */
	uint8  client_ea[6];      /* client MAC address */
	uint8  bsscfg_ea[6];      /* BSSCFG address of current interface */

	/* byte 20 ~ 23 */
	uint8  band;              /* enumerated in 'syna_csi_band_type' */
	uint8  bandwidth;         /* enumerated in 'syna_csi_bandwidth_type' */
	uint16 channel;           /* channel number of corresponding band */

	/* byte 24 ~ 31 */
	uint64 report_tsf;        /* current CSI DATA RX timestamp */

	/* byte 32 ~ 35 */
	uint8  num_txstream;      /* peer side TX spatial steams */
	uint8  num_rxchain;       /* number of RX side chain/antenna */
	uint16 num_subcarrier;    /* Number of subcarrier */

	/* byte 36 ~ 39 */
	int8   rssi;              /* average RSSI, and goto check the
	                           * 'rssi_ant' if this 'rssi' is zero
	                           */
	int8   noise;             /* average noise */
	int16  global_id;         /* CSI frame global ID */

	/* byte 40 ~ 47 */
	int8   rssi_ant[8];       /* RSSI of each RX chain/antenna
	                           * (Depends on solution, and some
	                           * chipsets may only provide 'rssi'
	                           * rather than 'rssi_ant')
	                           */

	/* byte 48 ~ 55 */
	uint64 padding_future;    /* reserved */

	/* byte 56 ~ 63 */
	uint16 data_length;       /* the bytes of 'data[]' */
	uint16 remain_length;     /* remain length of not received data */
	uint16 copied_length;     /* the length has been copied to user */
	uint16 checksum;          /* 'data[]' checksum, 0 - not used */

	/* byte 64 ~ End */
	uint32 data[0];           /* variable length according to the
	                           * 'data_length', but the minimal
	                           * valid data length will be
	                           * decided by the 'num_txchains'
	                           * 'num_rxchain' 'num_subcarrier'.
	                           * For example, the index quantity
	                           * of 'uint32' in the 11N 80MHz can
	                           * be got via:
	                           * 256 SubCarriers * 2 TXStream * 2 RXChain
	                           * Note: 'data_length' is bytes quantity,
	                           * and the 'data[]' array index range should
	                           * be 0 ~ (data_length/4 - 1)
	                           */
};

#define SYNA_CSI_PKT_TOTAL_LEN(ptr) \
(sizeof(struct syna_csi_header) \
	+ ((struct syna_csi_header *)(ptr))->data_length \
)

/* definition for structure and functions between driver and FW */

/* Maxinum csi file dump size */
#define MAX_CSI_FILESZ		(32 * 1024)
/* Maxinum subcarrier number */
#define MAXIMUM_CFR_DATA	2048
#define CSI_DUMP_PATH		"/sys/bcm-dhd/csi"
#define MAX_EVENT_SIZE		1400
/* maximun csi number stored at dhd */
#define MAX_CSI_NUM		16
#define MAX_CSI_BUF_NUM		1024

/* CSI VERSION */
#define CSI_VERSION_V0		0 /* V0 */
#define CSI_VERSION_V1		1 /* V1 */
#define CSI_VERSION_V2		2 /* V2 */

struct dhd_cfr_header_v0 {
	uint8 status:4;     /* bit3-0 */
	uint8 version:4;    /* bit7-4 */
	/* Peer MAC address */
	uint8 peer_macaddr[6];
	/* Number of Space Time Streams */
	uint8 sts;
	/* Number of RX chain */
	uint8 num_rx;
	/* Number of subcarrier */
	uint16 num_carrier;
	/* Length of the CSI dump */
	uint32 cfr_dump_length;
	/* remain unsend CSI data length */
	uint32 remain_length;
	/* RSSI */
	int8 rssi;
	uint32 data[0];
} __attribute__ ((packed));

struct dhd_cfr_header_v1 {
	uint8 status:4;     /* bit3-0 */
	uint8 version:4;    /* bit7-4 */
	/* Peer MAC address */
	uint8 peer_macaddr[6];
	/* Number of Space Time Streams */
	uint8 sts;
	/* Number of RX chain */
	uint8 num_rx;
	/* Number of subcarrier */
	uint16 num_carrier;
	/* Length of the CSI dump */
	uint32 cfr_dump_length;
	/* remain unsend CSI data length */
	uint32 remain_length;
	/* RSSI */
	int8 rssi;

	/* Chip id. 1 for BCM43456/8 */
	uint8 chip_id;
	/* Frame control field */
	uint8 fc;
	/* Time stamp when CFR capture is taken,
	 * in microseconds since the epoch
	 */
	uint64 cfr_timestamp;

	uint32 data[0];
} __attribute__ ((packed));

struct dhd_cfr_header_v2 { /* 8 bytes aligment for x64 */
	/* byte 0 ~ 7 */
	uint8  status_compat:4;   /* bit3-0, current CSI data status: 0->good */
	uint8  version_compat:4;  /* bit7-4, duplicate but compatible part */
	uint8  padding_magic;
	uint16 magic_flag;        /* flag for protecting and detecting header */

	uint8  version;           /* header version of this structure */
	uint8  format_type;       /* data format type of current CSI packet,
	                           * enumerated in 'syna_csi_format_type'
	                           */
	uint8  fc;                /* packet frame control type of current
	                           * CSI data for further checking if it
	                           * is correct expected result
	                           */
	uint8  flags;             /* flag may use for extra emendation
	                           * which enumerated as 'syna_csi_flag'
	                           */

	/* byte 8 ~ 19 */
	uint8  client_ea[6];      /* client MAC address */
	uint8  bsscfg_ea[6];      /* BSSCFG address of current interface */

	/* byte 20 ~ 23 */
	uint8  band;              /* enumerated in 'syna_csi_band_type' */
	uint8  bandwidth;         /* enumerated in 'syna_csi_bandwidth_type' */
	uint16 channel;           /* channel number of corresponding band */

	/* byte 24 ~ 31 */
	uint64 report_tsf;        /* current CSI DATA RX timestamp */

	/* byte 32 ~ 35 */
	uint8  num_txstream;      /* peer side TX spatial steams */
	uint8  num_rxchain;       /* number of RX side chain/antenna */
	uint16 num_subcarrier;    /* Number of subcarrier */

	/* byte 36 ~ 39 */
	int8   rssi;              /* average RSSI, and goto check the
	                           * 'rssi_ant' if this 'rssi' is zero
	                           */
	int8   noise;             /* average noise */
	int16  global_id;         /* CSI frame global ID */

	/* byte 40 ~ 47 */
	int8   rssi_ant[8];       /* RSSI of each RX chain/antenna
	                           * (Depends on solution, and some
	                           * chipsets may only provide 'rssi'
	                           * rather than 'rssi_ant')
	                           */

	/* byte 48 ~ 55 */
	uint64 padding_future;    /* reserved */

	/* byte 56 ~ 63 */
	uint16 data_length;       /* the data length of 'data[]' */
	uint16 remain_length;     /* the remain unsent length of 'data[]' */
	uint16 padding_length;    /* reserved */
	uint16 checksum;          /* 'data[]' checksum, 0 - not used */

	/* byte 64 ~ End */
	uint32 data[0];           /* variable according to 'data_length', but
	                           * the minial valid data length will be
	                           * decided by 'num_txchains', 'num_rxchain'
	                           * 'num_subcarrier'. For example, the
	                           * quantity of uint32(4 bytes) in the
	                           * 11N 80MHz can be got via:
	                           * 256 SubCarriers * 2 TXStream * 2 RXChain
	                           */
};

union dhd_cfr_header {
	struct dhd_cfr_header_v0  header_v0;
	struct dhd_cfr_header_v1  header_v1;
	struct dhd_cfr_header_v2  header_v2;
};
#define CSI_CFR_TOTAL_LENGTH(data_len) \
	(sizeof(union dhd_cfr_header) + data_len)

/* BW80 2x2 => 4bytes*256subcarries*2txstream*2rxchain */
#define CONST_CSI_MAXIMUM_DATA_BYTES	(CSI_CFR_TOTAL_LENGTH(256 *2 *2 *4))

struct csi_cfr_node {
	struct list_head  list;
	void  *pNode; /* recording current node pointer
	               * especially for UDP data manner
	               */
	uint32 total_size;

	struct syna_csi_header entry;
};

#define SYNA_CSI_CFR_NODE_LEN		sizeof(struct csi_cfr_node)

#define SYNA_CSI_CFR_NODE_TOTAL_LEN(ptr, data_length) \
(\
	SYNA_CSI_CFR_NODE_LEN + data_length \
)

#define SYNA_CSI_CFR_NODE_FREE_LEN(ptr) \
(\
	((struct csi_cfr_node *)(ptr))->total_size \
		- SYNA_CSI_CFR_NODE_LEN \
)

extern int dhd_csi_config(dhd_pub_t *dhdp, char *buf, uint length, bool is_set);

/* Function: change the upper layer fetching data manner
 * Input:  uint  is_set        indicate it's set or get action
 *         uint  type          the expected fetch type(ignore when get)
 *         uint  param         extra param when set(UDP socket port)
 * Output: <  0        error encounter
 *         == 0        successful
 *         >  0        corresponding 'fetch type' when get
 */
extern int dhd_csi_data_fetch_type_access(uint is_set, uint type, uint param);

/* Function: parse the CSI event and convert to 'dhd_csi_header' format,
 *           and queue the item into the common 'csi_cfr_queue'
 * Input:  dhd_pub_t    *dhdp     context for operation
 *         char         *buf      buffer for storing csi data
 *         uint          count    total bytes available in 'buf'
 * Output: <  0        error encounter
 *         == 0        successful but no csi data in queue
 *         >  0        bytes write into 'buf'
 */
extern int dhd_csi_retrieve_queue_data(dhd_pub_t *dhdp, char *buf, uint count);

/* Function: parse the CSI event and convert to 'dhd_csi_header' format,
 *           and queue the item into the common 'csi_cfr_queue'
 * Input:  dhd_pub_t      *dhdp          context for operation
 *         wl_event_msg_t *event         wl event message header
 *         void           *event_data    extra event data
 * Output: <  0        error encounter
 *         == 0        successful
 */
extern int dhd_csi_event_handler(dhd_pub_t *dhdp,
	const wl_event_msg_t *event, void *event_data);

/* Function: Initialize the CSI module
 * Input:  dhd_pub_t      *dhdp          context for operation
 * Output: <  0        error encounter
 *         == 0        successful
 */
extern int dhd_csi_init(dhd_pub_t *dhdp);

/* Function: Deinitialize the CSI module
 * Input:  dhd_pub_t      *dhdp          context for operation
 * Output: <  0        error encounter
 *         == 0        successful
 */
extern int dhd_csi_deinit(dhd_pub_t *dhdp);

#endif /* __DHD_CSI_H__ */
