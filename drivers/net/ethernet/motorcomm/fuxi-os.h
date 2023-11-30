/*++

Copyright (c) 2021 Motor-comm Corporation. 
Confidential and Proprietary. All rights reserved.

This is Motor-comm Corporation NIC driver relevant files. 
Please don't copy, modify,distribute without commercial permission.

--*/


#ifndef __FUXI_OS_H__
#define __FUXI_OS_H__

#include <linux/dma-mapping.h>
#include <linux/netdevice.h>
#include <linux/workqueue.h>
#include <linux/phy.h>
#include <linux/if_vlan.h>
#include <linux/bitops.h>
#include <linux/timecounter.h>

#ifdef CONFIG_PCI_MSI
#include <linux/pci.h>
#endif

#include <linux/pm_wakeup.h>
#include <linux/mdio.h>
#include <linux/clk.h>
#include <linux/bitrev.h>
#include <linux/crc32.h>
#include <linux/dcbnl.h>
#include <linux/inet.h>

#define LINUX

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#else
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,0))
#include <linux/crc32poly.h>
#endif

#include "fuxi-dbg.h"

struct fxgmac_ring;
struct fxgmac_pdata;

#define FXGMAC_DRV_VERSION                  "1.0.27"

#ifdef CONFIG_PCI_MSI
//#undef CONFIG_PCI_MSI 				//undefined for legacy interupt mode
#endif

#define PCIE_LP_ASPM_L0S                    1
#define PCIE_LP_ASPM_L1                     2
#define PCIE_LP_ASPM_L1SS                   4
#define PCIE_LP_ASPM_LTR                    8

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,19,0))
/*
 * There are multiple 16-bit CRC polynomials in common use, but this is
 * *the* standard CRC-32 polynomial, first popularized by Ethernet.
 * x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x^1+x^0
 */
#define CRC32_POLY_LE 0xedb88320
#define CRC32_POLY_BE 0x04c11db7

/*
 * This is the CRC32c polynomial, as outlined by Castagnoli.
 * x^32+x^28+x^27+x^26+x^25+x^23+x^22+x^20+x^19+x^18+x^14+x^13+x^11+x^10+x^9+
 * x^8+x^6+x^0
 */
#define CRC32C_POLY_LE 0x82F63B78
#endif

#define FXGMAC_FAIL    -1
#define FXGMAC_SUCCESS     0
#define FXGMAC_DEV_CMD      (SIOCDEVPRIVATE + 1)
#define FXGMAC_IOCTL_DFS_COMMAND    _IOWR('M', 0x80, struct ext_ioctl_data)

#define FXGMAC_MAX_DBG_TEST_PKT     150
#define FXGMAC_MAX_DBG_BUF_LEN      64000
#define FXGMAC_MAX_DBG_RX_DATA      1600
#define FXGMAC_NETDEV_OPS_BUF_LEN   256

#define FXGMAC_TEST_MAC_HEAD_LEN        14

#define FUXI_PM_WPI_READ_FEATURE_EN         1

#define RSS_Q_COUNT                         4

//#define FXGMAC_TX_INTERRUPT_EN		1
#define FXGMAC_TX_HANG_TIMER_EN             0
/* only for debug. for normal run, pls keep them both 0 */
//0: use default tx q; other: specify txq-1: 1 txq;
#define FXGMAC_NUM_OF_TX_Q_USED		0
//1 to enable a dummy tx,ie, no tail for gmac;
#define FXGMAC_DUMMY_TX_DEBUG		0
//1 to trigger(write reg 0x1000) for sniffer stop
#define FXGMAC_TRIGGER_TX_HANG		0

/* driver feature configuration */
#if FXGMAC_TX_HANG_TIMER_EN
//0: check hw current desc; 1: check software dirty
#define FXGMAC_TX_HANG_CHECH_DIRTY	0
#endif

// 1:poll tx of 4 channels; 0: since only 1 tx channel supported in this 
// version, poll ch 0 always.
#define FXGMAC_FULL_TX_CHANNEL		0

#ifdef CONFIG_ARM64
// when you want to run this driver on 64bit arm,you should open this,otherwise 
// dma's mask cannot be set successfully.
#define FUXI_DMA_BIT_MASK       64
#endif

#ifdef CONFIG_PCI_MSI
//should be same as FXGMAC_MAX_DMA_CHANNELS + 1 tx_irq
#define FXGMAC_MAX_MSIX_Q_VECTORS	(FXGMAC_MSIX_Q_VECTORS + 1)
#define FXGMAC_MSIX_CH0RXDIS_EN		0 //set to 1 for ch0 unbalance fix;
#define FXGMAC_MSIX_INTCTRL_EN      1

#define FXGMAC_PHY_INT_NUM          1
#define FXGMAC_MSIX_INT_NUMS (FXGMAC_MAX_MSIX_Q_VECTORS + FXGMAC_PHY_INT_NUM)
#else //for case of no CONFIG_PCI_MSI
#define FXGMAC_MSIX_CH0RXDIS_EN		0   //NO modification needed! for non-MSI, set to 0 always
#define FXGMAC_MSIX_INTCTRL_EN      0
#endif

/*RSS features*/
#ifdef FXGMAC_ONE_CHANNEL
#define FXGMAC_RSS_FEATURE_ENABLED	0 // 1:enable rss ; 0: rss not included.
#else
#define FXGMAC_RSS_FEATURE_ENABLED	1 // 1:enable rss ; 0: rss not included.
#endif
#define FXGMAC_RSS_HASH_KEY_LINUX	1 // 0:hard to default rss key ;1: normal hash key process from Linux.

/*WOL features*/
#define FXGMAC_WOL_FEATURE_ENABLED	1 // 1:enable wol ; 0: wol not included.
/*since wol upon link will cause issue, disabled it always. */
#define FXGMAC_WOL_UPON_EPHY_LINK	1 // 1:enable ephy link change wol ; 0: ephy link change wol is not supported.

/*Pause features*/
#define FXGMAC_PAUSE_FEATURE_ENABLED	1 // 1:enable flow control/pause framce ; 0: flow control/pause frame not included.

/*ARP offload engine (AOE)*/
#define FXGMAC_AOE_FEATURE_ENABLED	1 // 1:enable arp offload engine ; 0: aoe is not included.

/*NS offload engine*/
#define FXGMAC_NS_OFFLOAD_ENABLED	1 // 1:enable NS offload for IPv6 ; 0: NS is not included.

/*for fpga ver after, which needs release phy before set of MAC tx/rx */
#define FXGMAC_TXRX_EN_AFTER_PHY_RELEASE	1 // 1:release ephy before mac tx/rx bits are set.

/*power management features*/
#define FXGMAC_PM_FEATURE_ENABLED	1 // 1:enable PM ; 0: PM not included.

/*sanity check*/
#define FXGMAC_SANITY_CHECK_ENABLED	0 // 1:enable health checking ; 

/*vlan id filter*/
#define FXGMAC_FILTER_SINGLE_VLAN_ENABLED	1 // 1:enable health checking ;

//Linux driver implement VLAN HASH Table feature to support mutliple VLAN feautre
#define FXGMAC_FILTER_MULTIPLE_VLAN_ENABLED 1

//Linux driver implement MAC HASH Table feature
#define FUXI_MAC_HASH_TABLE 1

//Linux driver implement write multiple mac addr
#define FXGMAC_FILTER_MULTIPLE_MAC_ADDR_ENABLED 1

//Linux driver disable MISC Interrupt
#define FUXI_MISC_INT_HANDLE_FEATURE_EN             1

#define HAVE_FXGMAC_DEBUG_FS

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &(((TYPE*)0)->MEMBER))
#endif

#define ETH_IS_ZEROADDRESS(Address)               \
    ((((u8*)(Address))[0] == ((u8)0x00))            \
    && (((u8*)(Address))[1] == ((u8)0x00))          \
    && (((u8*)(Address))[2] == ((u8)0x00))          \
    && (((u8*)(Address))[3] == ((u8)0x00))          \
    && (((u8*)(Address))[4] == ((u8)0x00))          \
    && (((u8*)(Address))[5] == ((u8)0x00)))

 /* read from 8bit register via pci config space */
#define cfg_r8(_pdata, reg, pdat) pci_read_config_byte((_pdata)->pdev, (reg), (u8 *)(pdat))

 /* read from 16bit register via pci config space */
#define cfg_r16(_pdata, reg, pdat) pci_read_config_word((_pdata)->pdev, (reg), (u16 *)(pdat))

 /* read from 32bit register via pci config space */
#define cfg_r32(_pdata, reg, pdat) pci_read_config_dword((_pdata)->pdev, (reg), (u32 *)(pdat))

/* write to 8bit register via pci config space */
#define cfg_w8(_pdata, reg, val) pci_write_config_byte((_pdata)->pdev, (reg), (u8)(val))

/* write to 16bit register via pci config space */
#define cfg_w16(_pdata, reg, val) pci_write_config_word((_pdata)->pdev, (reg), (u16)(val))

/* write to 32bit register via pci config space */
#define cfg_w32(_pdata, reg, val) pci_write_config_dword((_pdata)->pdev, (reg), (u32)(val))

#define readreg(pAdapter, addr) (readl(addr))
#define writereg(pAdapter, val, addr) (writel(val, addr))
#define usleep_range_ex(pAdapter, a, b) (usleep_range(a, b))
#define _CR(Record, TYPE, Field)  ((TYPE *) ((char *) (Record) - (char *) &(((TYPE *) 0)->Field)))

#define FXGMAC_GET_REG_BITS(var, pos, len) ({				\
    typeof(pos) _pos = (pos);					            \
    typeof(len) _len = (len);					            \
    ((var) & GENMASK(_pos + _len - 1, _pos)) >> (_pos);		\
})

#define FXGMAC_GET_REG_BITS_LE(var, pos, len) ({			\
    typeof(pos) _pos = (pos);					            \
    typeof(len) _len = (len);					            \
    typeof(var) _var = le32_to_cpu((var));				    \
    ((_var) & GENMASK(_pos + _len - 1, _pos)) >> (_pos);	\
})

#define FXGMAC_SET_REG_BITS(var, pos, len, val) ({			\
    typeof(var) _var = (var);					            \
    typeof(pos) _pos = (pos);					            \
    typeof(len) _len = (len);					            \
    typeof(val) _val = (val);					            \
    _val = (_val << _pos) & GENMASK(_pos + _len - 1, _pos);	\
    _var = (_var & ~GENMASK(_pos + _len - 1, _pos)) | _val;	\
})

#define FXGMAC_SET_REG_BITS_LE(var, pos, len, val) ({		\
    typeof(var) _var = (var);					            \
    typeof(pos) _pos = (pos);					            \
    typeof(len) _len = (len);					            \
    typeof(val) _val = (val);					            \
    _val = (_val << _pos) & GENMASK(_pos + _len - 1, _pos);	\
    _var = (_var & ~GENMASK(_pos + _len - 1, _pos)) | _val;	\
    cpu_to_le32(_var);						                \
})

#define STR_FORMAT      "%s"

#define DbgPrintF(level,  fmt, ...)
#define DBGPRINT(Level, Fmt)
#define DBGPRINT_RAW(Level, Fmt)
#define DBGPRINT_S(Status, Fmt)
#define DBGPRINT_UNICODE(Level, UString)
#define Dump(p,cb,fAddress,ulGroup)

#undef ASSERT
#define ASSERT(x)

#define DbgPrintOidName(_Oid)
#define DbgPrintAddress(_pAddress)

#define fxgmac_dump_buffer(_skb, _len, _tx_rx)
#define DumpLine(_p, _cbLine, _fAddress, _ulGroup )

#ifndef FXGMAC_DEBUG
#define FXGMAC_DEBUG
#endif

/* For debug prints */
#ifdef FXGMAC_DEBUG
#define FXGMAC_PR(fmt, args...) \
    pr_alert("[%s,%d]:" fmt, __func__, __LINE__, ## args)

#define DPRINTK printk
#else
#define FXGMAC_PR(x...)		do { } while (0)
#define DPRINTK(x...)
#endif

#define IOC_MAGIC 'M'
#define IOC_MAXNR (0x80 + 5)

#define FUXI_DFS_IOCTL_DEVICE_INACTIVE   0x10001
#define FUXI_DFS_IOCTL_DEVICE_RESET      0x10002
#define FUXI_DFS_IOCTL_DIAG_BEGIN        0x10003
#define FUXI_DFS_IOCTL_DIAG_END          0x10004
#define FUXI_DFS_IOCTL_DIAG_TX_PKT       0x10005
#define FUXI_DFS_IOCTL_DIAG_RX_PKT       0x10006

#define FXGMAC_EFUSE_UPDATE_LED_CFG                  0x10007
#define FXGMAC_EFUSE_WRITE_LED                       0x10008
#define FXGMAC_EFUSE_WRITE_PATCH_REG                 0x10009
#define FXGMAC_EFUSE_WRITE_PATCH_PER_INDEX           0x1000A
#define FXGMAC_EFUSE_WRITE_OOB                       0x1000B
#define FXGMAC_EFUSE_LOAD                            0x1000C
#define FXGMAC_EFUSE_READ_REGIONABC                  0x1000D
#define FXGMAC_EFUSE_READ_PATCH_REG                  0x1000E
#define FXGMAC_EFUSE_READ_PATCH_PER_INDEX            0x1000F
#define FXGMAC_EFUSE_LED_TEST                        0x10010

#define FXGMAC_GET_MAC_DATA                         0x10011
#define FXGMAC_SET_MAC_DATA                         0x10012
#define FXGMAC_GET_SUBSYS_ID                        0x10013
#define FXGMAC_SET_SUBSYS_ID                        0x10014
#define FXGMAC_GET_GMAC_REG                         0x10015
#define FXGMAC_SET_GMAC_REG                         0x10016
#define FXGMAC_GET_PHY_REG                          0x10017
#define FXGMAC_SET_PHY_REG                          0x10018
#define FXGMAC_EPHYSTATISTICS                       0x10019
#define FXGMAC_GET_STATISTICS                       0x1001A
#define FXGMAC_GET_PCIE_LOCATION                    0x1001B

#define FXGMAC_GET_GSO_SIZE                         0x1001C
#define FXGMAC_SET_GSO_SIZE                         0x1001D
#define FXGMAC_SET_RX_MODERATION                    0x1001E
#define FXGMAC_SET_TX_MODERATION                    0x1001F
#define FXGMAC_GET_TXRX_MODERATION                  0x10020

#define MAX_PKT_BUF                 1
#define FXGAMC_MAX_DATA_SIZE (1024 * 4 + 16)

#ifndef PCI_CAP_ID_MSI
#define PCI_CAP_ID_MSI     0x05    /* Message Signalled Interrupts */
#endif

#ifndef PCI_CAP_ID_MSIX
#define PCI_CAP_ID_MSIX     0x11    /* MSI-X */
#endif

#define PCI_CAP_ID_MSI_ENABLE_POS   0x10
#define PCI_CAP_ID_MSI_ENABLE_LEN   0x1
#define PCI_CAP_ID_MSIX_ENABLE_POS   0x1F
#define PCI_CAP_ID_MSIX_ENABLE_LEN   0x1

#ifndef fallthrough
#if __has_attribute(__fallthrough__)
# define fallthrough                    __attribute__((__fallthrough__))
#else
# define fallthrough                    do {} while (0)  /* fallthrough */
#endif
#endif


#pragma pack(1)
// it's better to make this struct's size to 128byte.
struct pattern_packet{
    u8                  ether_daddr[ETH_ALEN];
    u8                  ether_saddr[ETH_ALEN];
    u16                 ether_type;

    __be16		        ar_hrd;		/* format of hardware address	*/
    __be16		        ar_pro;		/* format of protocol 	*/
    unsigned char       ar_hln;		/* length of hardware address	*/
    unsigned char       ar_pln;		/* length of protocol address	*/
    __be16		        ar_op;		/* ARP opcode (command)		*/
    unsigned char       ar_sha[ETH_ALEN];	/* sender hardware address  */
    unsigned char       ar_sip[4];		/* sender IP address        */
    unsigned char       ar_tha[ETH_ALEN];	/* target hardware address  */
    unsigned char       ar_tip[4];		/* target IP address        */

    u8                  reverse[86];
};
#pragma pack()

typedef enum
{
    CURRENT_STATE_SHUTDOWN = 0,
    CURRENT_STATE_RESUME = 1,
    CURRENT_STATE_INIT = 2,
    CURRENT_STATE_SUSPEND = 3,
    CURRENT_STATE_CLOSE = 4,
    CURRENT_STATE_OPEN = 5,
    CURRENT_STATE_RESTART = 6,
    CURRENT_STATE_REMOVE = 7,
}CURRENT_STATE;

typedef dma_addr_t                  DMA_ADDR_T;
typedef enum pkt_hash_types         RSS_HASH_TYPE;
typedef void __iomem*               IOMEM;
typedef struct pci_dev              PCI_DEV;

struct ext_command_buf {
    void* buf;
    u32 size_in;
    u32 size_out;
};

struct ext_command_mac {
    u32  num;
    union {
        u32  val32;
        u16  val16;
        u8   val8;
    };
};

struct ext_command_mii {
    u16  dev;
    u16  num;
    u16  val;
};

struct ext_ioctl_data {
    u32	cmd_type;
    struct ext_command_buf cmd_buf;
};

typedef struct _fxgmac_test_buf {
    u8* addr;
    u32 offset;
    u32 length;
} fxgmac_test_buf, *pfxgmac_test_buf;

typedef struct _fxgmac_test_packet {
    struct _fxgmac_test_packet  * next;
    u32                      length;             /* total length of the packet(buffers) */
    u32                      type;               /* packet type, vlan, ip checksum, TSO, etc. */

    fxgmac_test_buf          buf[MAX_PKT_BUF];
    fxgmac_test_buf          sGList[MAX_PKT_BUF];
    u16                      vlanID;                                                         
    u16                      mss;                                                            
    u32                      hash;                                                           
    u16                      cpuNum;                                                         
    u16                      xsum;               /* rx, ip-payload checksum */
    u16                      csumStart;          /* custom checksum offset to the mac-header */
    u16                      csumPos;            /* custom checksom position (to the mac_header) */                                          
    void*                    upLevelReserved[4];                                                         
    void*                    lowLevelReserved[4];                                                            
} fxgmac_test_packet, *pfxgmac_test_packet;

typedef struct fxgmac_channel_of_platform
{
    char dma_irq_name[IFNAMSIZ + 32];

    u32 dma_irq_tx; //for MSIx to match the type of struct msix_entry.vector
    char dma_irq_name_tx[IFNAMSIZ + 32];

    /* Netdev related settings */
    struct napi_struct napi_tx;

    /* Netdev related settings */
    struct napi_struct napi_rx;
    struct timer_list tx_timer;

#if FXGMAC_TX_HANG_TIMER_EN
    unsigned int tx_hang_timer_active;
    struct timer_list tx_hang_timer;
    unsigned int tx_hang_hw_cur;
#endif    
}FXGMAC_CHANNEL_OF_PLATFORM;

typedef struct per_regisiter_info
{
    unsigned int           size;
    unsigned int           address;
    unsigned int           value;
    unsigned char          data[FXGAMC_MAX_DATA_SIZE];
}PER_REG_INFO;

// for FXGMAC_EFUSE_WRITE_PATCH_PER_INDEX,val0 is index, val1 is offset,
// val2 is value
typedef struct ext_command_data {
    u32 val0;
    u32 val1;
    u32 val2;
}CMD_DATA;

typedef struct fxgmac_pdata_of_platform
{
    u32                             cfg_pci_cmd;
    u32                             cfg_cache_line_size;
    u32                             cfg_mem_base;
    u32                             cfg_mem_base_hi;
    u32                             cfg_io_base;
    u32                             cfg_int_line;
    u32                             cfg_device_ctrl1;
    u32                             cfg_pci_link_ctrl;
    u32                             cfg_device_ctrl2;
    u32                             cfg_msix_capability;

    struct  work_struct             restart_work;
    u32                             int_flags; //legacy, msi or msix
    int                             phy_irq;
#ifdef CONFIG_PCI_MSI
    struct msix_entry               *msix_entries;
#endif

    /* power management and wol*/
    u32                             wol; //wol options
    unsigned long                   powerstate; //power state
    unsigned int                    ns_offload_tab_idx; //for ns-offload table. 2 entries supported.
    CURRENT_STATE                   current_state;
    netdev_features_t               netdev_features;
    struct napi_struct              napi;
    struct napi_struct              napi_phy;
    u32                             mgm_intctrl_val;
    bool                            phy_link;
    bool                            fxgmac_test_tso_flag;
    u32                             fxgmac_test_tso_seg_num;
    u32                             fxgmac_test_last_tso_len;
    u32                             fxgmac_test_packet_len;
    volatile u32                    fxgmac_test_skb_arr_in_index;
    volatile u32                    fxgmac_test_skb_arr_out_index;
    //CMD_DATA                        ex_cmd_data; 
    //PER_REG_INFO                    per_reg_data;
    struct sk_buff              *fxgmac_test_skb_array[FXGMAC_MAX_DBG_TEST_PKT];
#ifdef HAVE_FXGMAC_DEBUG_FS
    struct dentry               *dbg_adapter;
    struct dentry               *fxgmac_dbg_root;
    char                        fxgmac_dbg_netdev_ops_buf[FXGMAC_NETDEV_OPS_BUF_LEN];
#endif
}FXGMAC_PDATA_OF_PLATFORM;


#ifdef HAVE_FXGMAC_DEBUG_FS
void fxgmac_dbg_adapter_init(struct fxgmac_pdata *pdata);
void fxgmac_dbg_adapter_exit(struct fxgmac_pdata *pdata);
void fxgmac_dbg_init(struct fxgmac_pdata *pdata);
void fxgmac_dbg_exit(struct fxgmac_pdata *pdata);
#endif /* HAVE_FXGMAC_DEBUG_FS */

void fxgmac_restart_dev(struct fxgmac_pdata *pdata);
long fxgmac_dbg_netdev_ops_ioctl(struct file *file, unsigned int cmd, 
                                unsigned long arg);

int  fxgmac_init(struct fxgmac_pdata *pdata, bool save_private_reg);
/* for phy interface */
int  fxgmac_ephy_autoneg_ability_get(struct fxgmac_pdata *pdata, 
                                        unsigned int *cap_mask);
int  fxgmac_ephy_status_get(struct fxgmac_pdata *pdata, int* speed,
                                int* duplex, int* ret_link, int *media);
int  fxgmac_ephy_soft_reset(struct fxgmac_pdata *pdata);
void fxgmac_phy_force_speed(struct fxgmac_pdata *pdata, int speed);
void fxgmac_phy_force_duplex(struct fxgmac_pdata *pdata, int duplex);
void fxgmac_phy_force_autoneg(struct fxgmac_pdata *pdata, int autoneg);

unsigned int    fxgmac_get_netdev_ip4addr(struct fxgmac_pdata *pdata);
unsigned char * fxgmac_get_netdev_ip6addr(struct fxgmac_pdata *pdata, 
                                            unsigned char *ipval, 
                                            unsigned char *ip6addr_solicited, 
                                            unsigned int ifa_flag);

#if FXGMAC_PM_FEATURE_ENABLED
void fxgmac_net_powerdown(struct fxgmac_pdata *pdata, unsigned int wol);
void fxgmac_net_powerup(struct fxgmac_pdata *pdata);
#endif

inline unsigned int fxgmac_tx_avail_desc(struct fxgmac_ring *ring);
inline unsigned int fxgmac_rx_dirty_desc(struct fxgmac_ring *ring);
int fxgmac_start(struct fxgmac_pdata *pdata);
void fxgmac_stop(struct fxgmac_pdata *pdata);
void fxgmac_free_rx_data(struct fxgmac_pdata *pdata);
void fxgmac_free_tx_data(struct fxgmac_pdata *pdata);

#endif //__FUXI_OS_H__

