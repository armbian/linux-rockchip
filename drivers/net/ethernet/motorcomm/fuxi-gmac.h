/*++

Copyright (c) 2021 Motorcomm, Inc.
Motorcomm Confidential and Proprietary.

This is Motorcomm NIC driver relevant files. Please don't copy, modify,
distribute without commercial permission.

--*/

#ifndef __FUXI_GMAC_H__
#define __FUXI_GMAC_H__

#include "fuxi-os.h"

#ifdef LINUX
#include <linux/dma-mapping.h>
#include <linux/netdevice.h>
#include <linux/workqueue.h>
#include <linux/phy.h>
#include <linux/if_vlan.h>
#include <linux/bitops.h>
#include <linux/timecounter.h>

#ifdef CONFIG_PCI_MSI
//for MSIx, 20210527
#include <linux/pci.h>
#endif
#include <linux/pm_wakeup.h>

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#else
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif

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

#define FXGMAC_TEST_MAC_HEAD_LEN        14

#endif

// For fpga before 20210507
#define FXGMAC_FPGA_VER_B4_0507		0
#define FXGMAC_FPGA_VER_20210507	1

#ifdef UBOOT
#define FXGMAC_DRV_NAME                     "yt6801"
#else
#define FXGMAC_DRV_NAME                     "fuxi-gmac"
#endif

#define FXGMAC_DRV_VERSION                  "1.0.21"

#define FXGMAC_DRV_DESC                     "Motorcomm FUXI GMAC Driver"

#define FXGMAC_TX_HANG_TIMER_EN             0	// 0 - TX hang checking from MPCheckForHang. 1 - TX hang checking from sw timer. This will be more real time.
#define FUXI_MAC_REGS_OFFSET                0x2000

#define FUXI_EPHY_INTERRUPT_D0_OFF          0 //1: in normal D0 state, turn off ephy link change interrupt.
#define FUXI_ALLOC_NEW_RECBUFFER            0 //1:when rec buffer is not enough,to create rbd and rec buffer ,but  the rdb need to be continus with the intialized rdb,so close the feature 

#if defined(_WIN32) || defined(_WIN64) || defined(LINUX)
#define FUXI_PM_WPI_READ_FEATURE_EN         1
#endif

#define PHY_LINK_TIMEOUT                    1500
#define ESD_RESET_MAXIMUM                   0

#ifndef LINUX
#define FUXI_EPHY_AUTO_FORCE_FEATURE_EN     1
#define FUXI_EPHY_REG_ACCESS_FEATURE_EN     1
#define FUXI_MISC_INT_HANDLE_FEATURE_EN     1
#define FUXI_EPHY_LINK_INT_CHECK_EN         1
#define FUXI_EPHY_LOOPBACK_DETECT_EN        1
#endif

#define REGWR_RETRY_MAXIMUM                 2600
#define PCIE_LINKDOWN_VALUE                 0xFFFFFFFF

#if defined(UEFI) || defined(PXE)
#define RSS_Q_COUNT                         1
#define TX_RING_COUNT                       1
#define RX_RING_COUNT                       1
#else
#define RSS_Q_COUNT                         4
#endif

#ifdef FXGMAC_ONE_CHANNEL                   // if use only one channel, you should define it in fuxi-os.h
#define FXGMAC_MSIX_Q_VECTORS       1
#ifdef LINUX
#define FXGMAC_TX_INTERRUPT_EN		0
#endif
#else
#define FXGMAC_MSIX_Q_VECTORS	4
#ifdef LINUX
#define FXGMAC_TX_INTERRUPT_EN		1 //set to 1 for enabling tx interrupt for tx complete checking, 20220210
#endif
#endif

#ifdef LINUX
/* only for debug. for normal run, pls keep them both 0 */
#define FXGMAC_NUM_OF_TX_Q_USED		0 //0: use default tx q; other: specify txq-1: 1 txq; 20210908
#define FXGMAC_DUMMY_TX_DEBUG		0 //for debug. 1 to enable a dummy tx,ie, no tail for gmac; 20210908
#define FXGMAC_TRIGGER_TX_HANG		0 //for debug. 1 to trigger(write reg 0x1000) for sniffer stop, 20210909

/* driver feature configuration */
#if FXGMAC_TX_HANG_TIMER_EN
#define FXGMAC_TX_HANG_CHECH_DIRTY	0 //0: check hw current desc; 1: check software dirty, 20211229
#endif

#define FXGMAC_FULL_TX_CHANNEL		0 // 1:poll tx of 4 channels; 0: since only 1 tx channel supported in this version, poll ch 0 always. */

#ifdef CONFIG_ARM64
#define FUXI_DMA_BIT_MASK       64  // when you want to run this driver on 64bit arm,you should open this,otherwise dma's mask cannot be set successfully.
#endif

#ifdef CONFIG_PCI_MSI
//#undef CONFIG_PCI_MSI 				//undefined for legacy interupt mode
#endif

#ifdef CONFIG_PCI_MSI
//20210526 for MSIx
#define FXGMAC_MAX_MSIX_Q_VECTORS	FXGMAC_MSIX_Q_VECTORS //should be same as FXGMAC_MAX_DMA_CHANNELS
#if FXGMAC_TX_INTERRUPT_EN
#undef FXGMAC_MAX_MSIX_Q_VECTORS
#define FXGMAC_MAX_MSIX_Q_VECTORS	(FXGMAC_MSIX_Q_VECTORS + 1) //should be same as FXGMAC_MAX_DMA_CHANNELS + 1 tx_irq
#endif
#define FXGMAC_MSIX_CH0RXDIS_EN		0 //set to 1 for ch0 unbalance fix, 20221012;
#define FXGMAC_MSIX_INTCTRL_EN      1
#else //for case of no CONFIG_PCI_MSI
#define FXGMAC_MSIX_CH0RXDIS_EN		0 //NO modification needed! for non-MSI, set to 0 always, 20221012;
#define FXGMAC_MSIX_INTCTRL_EN      0
#endif

#if FXGMAC_TX_INTERRUPT_EN
#define FXGMAC_IS_CHANNEL_WITH_TX_IRQ(chId)	(0 == (chId) ? 1 : 0)
#endif

/*added RSS features, 20210608*/
#define FXGMAC_RSS_FEATURE_ENABLED	1 // 1:enable rss ; 0: rss not included.
#define FXGMAC_RSS_HASH_KEY_LINUX	1 // 0:hard to default rss key ; 1: normal hash key process from Linux.

/*added WOL features, 20210622*/
#define FXGMAC_WOL_FEATURE_ENABLED	1 // 1:enable wol ; 0: wol not included.
/*since wol upon link will cause issue, disabled it always. 20220113 */
#define FXGMAC_WOL_UPON_EPHY_LINK	1 // 1:enable ephy link change wol ; 0: ephy link change wol is not supported.

/*added Pause features, 20210727*/
#define FXGMAC_PAUSE_FEATURE_ENABLED	1 // 1:enable flow control/pause framce ; 0: flow control/pause frame not included.

/*added ARP offload engine (AOE), 20210623*/
#define FXGMAC_AOE_FEATURE_ENABLED	1 // 1:enable arp offload engine ; 0: aoe is not included.

/*added NS offload engine, 20220115*/
#define FXGMAC_NS_OFFLOAD_ENABLED	1 // 1:enable NS offload for IPv6 ; 0: NS is not included.

/*added for fpga ver after 20210729, which needs release phy before set of MAC tx/rx , 20210909*/
#define FXGMAC_TXRX_EN_AFTER_PHY_RELEASE	1 // 1:release ephy before mac tx/rx bits are set.

/*added power management features, 20210709*/
#define FXGMAC_PM_FEATURE_ENABLED	1 // 1:enable PM ; 0: PM not included.

/*added sanity check, 20210914 */
#define FXGMAC_SANITY_CHECK_ENABLED	0 // 1:enable health checking ; 


/*added vlan id filter 20220223 */
#define FXGMAC_FILTER_SINGLE_VLAN_ENABLED	1 // 1:enable health checking ;

//2022-04-24 xiaojiang comment
//Linux driver implement VLAN HASH Table feature to support mutliple VLAN feautre
#define FXGMAC_FILTER_MULTIPLE_VLAN_ENABLED 1

//2022-05-06 xiaojiang comment
//Linux driver implement MAC HASH Table feature
#define FUXI_MAC_HASH_TABLE 1

//2022-05-06 xiaojiang comment
//Linux driver implement write multiple mac addr
#define FXGMAC_FILTER_MULTIPLE_MAC_ADDR_ENABLED 1

//2022-05-06 xiaojiang comment
//Linux driver disable MISC Interrupt
#define FUXI_MISC_INT_HANDLE_FEATURE_EN             0
#endif

/* flags for ipv6 NS offload address, local link or Global unicast */
#define FXGMAC_NS_IFA_LOCAL_LINK		1
#define FXGMAC_NS_IFA_GLOBAL_UNICAST	2

/* Descriptor related parameters */
#if FXGMAC_TX_HANG_TIMER_EN
#define FXGMAC_TX_DESC_CNT		1024
#else
#define FXGMAC_TX_DESC_CNT		256 //256 to make sure the tx ring is in the 4k range when FXGMAC_TX_HANG_TIMER_EN is 0
#endif
#define FXGMAC_TX_DESC_MIN_FREE		(FXGMAC_TX_DESC_CNT >> 3)
#define FXGMAC_TX_DESC_MAX_PROC		(FXGMAC_TX_DESC_CNT >> 1)
#define FXGMAC_RX_DESC_CNT		1024
#define FXGMAC_RX_DESC_MAX_DIRTY	(FXGMAC_RX_DESC_CNT >> 3)

/* Descriptors required for maximum contiguous TSO/GSO packet */
#define FXGMAC_TX_MAX_SPLIT	((GSO_MAX_SIZE / FXGMAC_TX_MAX_BUF_SIZE) + 1)

/* Maximum possible descriptors needed for a SKB */
#define FXGMAC_TX_MAX_DESC_NR	(MAX_SKB_FRAGS + FXGMAC_TX_MAX_SPLIT + 2)

#define FXGMAC_TX_MAX_BUF_SIZE	(0x3fff & ~(64 - 1))
#define FXGMAC_RX_MIN_BUF_SIZE	(ETH_FRAME_LEN + ETH_FCS_LEN + VLAN_HLEN)
#define FXGMAC_RX_BUF_ALIGN	64

/* Maximum Size for Splitting the Header Data
 * Keep in sync with SKB_ALLOC_SIZE
 * 3'b000: 64 bytes, 3'b001: 128 bytes
 * 3'b010: 256 bytes, 3'b011: 512 bytes
 * 3'b100: 1023 bytes ,   3'b101'3'b111: Reserved
 */
#define FXGMAC_SPH_HDSMS_SIZE		3
#define FXGMAC_SKB_ALLOC_SIZE		512

//2022-04-22 xiaojiang comment
//In Linux Driver, it set MAX_FIFO size 131072, here it uses the same value as windows driver
#define FXGMAC_MAX_FIFO			81920

#define FXGMAC_MAX_DMA_CHANNELS		FXGMAC_MSIX_Q_VECTORS
#define FXGMAC_DMA_STOP_TIMEOUT		5
#define FXGMAC_DMA_INTERRUPT_MASK	0x31c7
#if FXGMAC_TX_INTERRUPT_EN
#define FXGMAC_MAX_DMA_CHANNELS_PLUS_1TX	(FXGMAC_MAX_DMA_CHANNELS + 1)
#endif

/* Default coalescing parameters */
//2022-04-22 xiaojiang comment
//In Windows Driver, the DMA_TX_USECS is not used, here it uses the linux driver value
#define FXGMAC_INIT_DMA_TX_USECS	2000
#define FXGMAC_INIT_DMA_TX_FRAMES	25
#define FXGMAC_INIT_DMA_RX_USECS	30
#define FXGMAC_INIT_DMA_RX_FRAMES	25
#define FXGMAC_MAX_DMA_RIWT		0xff
#define FXGMAC_MIN_DMA_RIWT		0x01

/* Flow control queue count */
#define FXGMAC_MAX_FLOW_CONTROL_QUEUES	8

/* System clock is 125 MHz */
#define FXGMAC_SYSCLOCK			125000000

/* Maximum MAC address hash table size (256 bits = 8 bytes) */
#define FXGMAC_MAC_HASH_TABLE_SIZE      8

/* wol pattern settings */
#define MAX_PATTERN_SIZE                128                             // PATTERN length
#define MAX_PATTERN_COUNT               16                              // pattern count
#define MAX_LPP_ARP_OFFLOAD_COUNT       1
#define MAX_LPP_NS_OFFLOAD_COUNT        2

#define MAX_WPI_LENGTH_SIZE             1536                            // WPI packet.
#define PM_WAKE_PKT_ALIGN               8                               // try use 64 bit boundary...

/* Receive Side Scaling */
#define FXGMAC_RSS_HASH_KEY_SIZE        40
#define FXGMAC_RSS_MAX_TABLE_SIZE       128
#define FXGMAC_RSS_LOOKUP_TABLE_TYPE    0
#define FXGMAC_RSS_HASH_KEY_TYPE        1
#define MAX_MSI_COUNT                   16                              // Max Msi/Msix supported.

#define FXGMAC_STD_PACKET_MTU           1500
#define FXGMAC_JUMBO_PACKET_MTU         9014

#define NIC_MAX_TCP_OFFLOAD_SIZE        7300
#define NIC_MIN_LSO_SEGMENT_COUNT       2

/* power management */
#define FXGMAC_POWER_STATE_DOWN         0
#define FXGMAC_POWER_STATE_UP           1
#if !defined(UEFI) 
#ifndef LINUX
#define ETH_IS_ZEROADDRESS(Address)               \
    ((((PUCHAR)(Address))[0] == ((UCHAR)0x00)) && (((PUCHAR)(Address))[1] == ((UCHAR)0x00)) && (((PUCHAR)(Address))[2] == ((UCHAR)0x00)) && (((PUCHAR)(Address))[3] == ((UCHAR)0x00)) && (((PUCHAR)(Address))[4] == ((UCHAR)0x00)) && (((PUCHAR)(Address))[5] == ((UCHAR)0x00)))
#endif
#endif
#ifdef LINUX
#define HAVE_FXGMAC_DEBUG_FS

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &(((TYPE*)0)->MEMBER))
#endif

#define ETH_IS_ZEROADDRESS(Address)               \
    ((((u8*)(Address))[0] == ((u8)0x00)) && (((u8*)(Address))[1] == ((u8)0x00)) && (((u8*)(Address))[2] == ((u8)0x00)) && (((u8*)(Address))[3] == ((u8)0x00)) && (((u8*)(Address))[4] == ((u8)0x00)) && (((u8*)(Address))[5] == ((u8)0x00)))

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

#endif

// Don't change the member variables or types, this inherits from Windows OS.
struct wol_bitmap_pattern
{
    u32         flags;
    u32         pattern_size;
    u32         mask_size;
    u8          mask_info[MAX_PATTERN_SIZE / 8];       //
    u8          pattern_info[MAX_PATTERN_SIZE];        //
    u8          pattern_offset;
    u16         pattern_crc;
};

struct led_setting
{
    u32 s0_led_setting[5];
    u32 s3_led_setting[5];
    u32 s5_led_setting[5];
    u32 disable_led_setting[5];
};

typedef struct led_setting LED_SETTING;
typedef struct wol_bitmap_pattern WOL_BITMAP_PATTERN;

typedef enum
{
    WAKE_REASON_NONE = 0,
    WAKE_REASON_MAGIC,
    WAKE_REASON_PATTERNMATCH,
    WAKE_REASON_LINK,
    WAKE_REASON_TCPSYNV4,
    WAKE_REASON_TCPSYNV6,
    WAKE_REASON_TBD,	//for wake up method like Link-change, for that, GMAC cannot identify and need more checking.
    WAKE_REASON_HW_ERR,
} WAKE_REASON;		//note, maybe we should refer to NDIS_PM_WAKE_REASON_TYPE to avoid duplication definition....

/* Helper macro for descriptor handling
 *  Always use FXGMAC_GET_DESC_DATA to access the descriptor data
 */
#if 0 //No need to round
#define FXGMAC_GET_DESC_DATA(ring, idx) ({				\
    typeof(ring) _ring = (ring);					\
    ((_ring)->desc_data_head +					\
     ((idx) & ((_ring)->dma_desc_count - 1)));			\
})
#endif

#define FXGMAC_GET_DESC_DATA(ring, idx) ((ring)->desc_data_head + (idx))
#define FXGMAC_GET_ENTRY(x, size) ((x + 1) & (size - 1))
#if defined(LINUX) || defined(UEFI_AARCH64) ||defined(UEFI_LOONGARCH64) || defined(UBOOT)

#if defined(UBOOT)
/* write 32bit register */
#define writereg(pAdapter, val, addr) (writel(val, addr))
/* read 32bit register */
#define readreg(pAdapter, addr) (readl(addr))
/* read from 32bit register via pci config space */
#define cfg_r32(_pdata, reg, pdat) dm_pci_read_config32((_pdata)->pdev, (reg), (u32 *)(pdat));
/* write to 32bit register via pci config space */
#define cfg_w32(_pdata, reg, val) dm_pci_write_config32((_pdata)->pdev, (reg), (u32)(val))
/* usleep */
#define usleep_range_ex(pAdapter, a, b) (udelay(b))
#endif

#if defined(LINUX) 
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
#endif

#define FXGMAC_GET_REG_BITS(var, pos, len) ({				\
    typeof(pos) _pos = (pos);					\
    typeof(len) _len = (len);					\
    ((var) & GENMASK(_pos + _len - 1, _pos)) >> (_pos);		\
})

#define FXGMAC_GET_REG_BITS_LE(var, pos, len) ({			\
    typeof(pos) _pos = (pos);					\
    typeof(len) _len = (len);					\
    typeof(var) _var = le32_to_cpu((var));				\
    ((_var) & GENMASK(_pos + _len - 1, _pos)) >> (_pos);		\
})

#define FXGMAC_SET_REG_BITS(var, pos, len, val) ({			\
    typeof(var) _var = (var);					\
    typeof(pos) _pos = (pos);					\
    typeof(len) _len = (len);					\
    typeof(val) _val = (val);					\
    _val = (_val << _pos) & GENMASK(_pos + _len - 1, _pos);		\
    _var = (_var & ~GENMASK(_pos + _len - 1, _pos)) | _val;		\
})

#define FXGMAC_SET_REG_BITS_LE(var, pos, len, val) ({			\
    typeof(var) _var = (var);					\
    typeof(pos) _pos = (pos);					\
    typeof(len) _len = (len);					\
    typeof(val) _val = (val);					\
    _val = (_val << _pos) & GENMASK(_pos + _len - 1, _pos);		\
    _var = (_var & ~GENMASK(_pos + _len - 1, _pos)) | _val;		\
    cpu_to_le32(_var);						\
})

#else
__inline u32 FXGMAC_GET_REG_BITS(u32 var, u32 pos, u32 len)
{
    u32 _pos = (pos);
    u32 _len = (len);
    u32 v = ((var)&GENMASK(_pos + _len - 1, _pos)) >> (_pos);

    return v;
}

__inline u32 FXGMAC_GET_REG_BITS_LE(u32 var, u32 pos, u32 len)
{
    u32 _pos = (pos);
    u32 _len = (len);
    u32 _var = le32_to_cpu((var));
    u32 v = ((_var)&GENMASK(_pos + _len - 1, _pos)) >> (_pos);

    return v;
}

__inline u32 FXGMAC_SET_REG_BITS(u32 var, u32 pos, u32 len, u32 val)
{
    u32 _var = (var);
    u32 _pos = (pos);
    u32 _len = (len);
    u32 _val = (val);
    _val = (_val << _pos) & GENMASK(_pos + _len - 1, _pos);
    _var = (_var & ~GENMASK(_pos + _len - 1, _pos)) | _val;

    return _var;
}

__inline u32 FXGMAC_SET_REG_BITS_LE(u32 var, u32 pos, u32 len, u32 val)
{
    u32 _var = (var);
    u32 _pos = (pos);
    u32 _len = (len);
    u32 _val = (val);
    _val = (_val << _pos) & GENMASK(_pos + _len - 1, _pos);
    _var = (_var & ~GENMASK(_pos + _len - 1, _pos)) | _val;

    return cpu_to_le32(_var);
}
#endif

struct fxgmac_pdata;

enum fxgmac_int {
    FXGMAC_INT_DMA_CH_SR_TI,
    FXGMAC_INT_DMA_CH_SR_TPS,
    FXGMAC_INT_DMA_CH_SR_TBU,
    FXGMAC_INT_DMA_CH_SR_RI,
    FXGMAC_INT_DMA_CH_SR_RBU,
    FXGMAC_INT_DMA_CH_SR_RPS,
    FXGMAC_INT_DMA_CH_SR_TI_RI,
    FXGMAC_INT_DMA_CH_SR_FBE,
    FXGMAC_INT_DMA_ALL,
};

struct fxgmac_stats {
    /* MMC TX counters */
    u64 txoctetcount_gb;
    u64 txframecount_gb;
    u64 txbroadcastframes_g;
    u64 txmulticastframes_g;
    u64 tx64octets_gb;
    u64 tx65to127octets_gb;
    u64 tx128to255octets_gb;
    u64 tx256to511octets_gb;
    u64 tx512to1023octets_gb;
    u64 tx1024tomaxoctets_gb;
    u64 txunicastframes_gb;
    u64 txmulticastframes_gb;
    u64 txbroadcastframes_gb;
    u64 txunderflowerror;
    u64 txsinglecollision_g;
    u64 txmultiplecollision_g;
    u64 txdeferredframes;
    u64 txlatecollisionframes;
    u64 txexcessivecollisionframes;
    u64 txcarriererrorframes;
    u64 txoctetcount_g;
    u64 txframecount_g;
    u64 txexcessivedeferralerror;
    u64 txpauseframes;
    u64 txvlanframes_g;
    u64 txoversize_g;

    /* MMC RX counters */
    u64 rxframecount_gb;
    u64 rxoctetcount_gb;
    u64 rxoctetcount_g;
    u64 rxbroadcastframes_g;
    u64 rxmulticastframes_g;
    u64 rxcrcerror;
    u64 rxalignerror;
    u64 rxrunterror;
    u64 rxjabbererror;
    u64 rxundersize_g;
    u64 rxoversize_g;
    u64 rx64octets_gb;
    u64 rx65to127octets_gb;
    u64 rx128to255octets_gb;
    u64 rx256to511octets_gb;
    u64 rx512to1023octets_gb;
    u64 rx1024tomaxoctets_gb;
    u64 rxunicastframes_g;
    u64 rxlengtherror;
    u64 rxoutofrangetype;
    u64 rxpauseframes;
    u64 rxfifooverflow;
    u64 rxvlanframes_gb;
    u64 rxwatchdogerror;
    u64 rxreceiveerrorframe;
    u64 rxcontrolframe_g;

    /* Extra counters */
    u64 tx_tso_packets;
    u64 rx_split_header_packets;
    u64 tx_process_stopped;
    u64 rx_process_stopped;
    u64 tx_buffer_unavailable;
    u64 rx_buffer_unavailable;
    u64 fatal_bus_error;
    u64 tx_vlan_packets;
    u64 rx_vlan_packets;
    u64 napi_poll_isr;
    u64 napi_poll_txtimer;
    u64 cnt_alive_txtimer;
#ifdef LINUX	
#if FXGMAC_TX_HANG_TIMER_EN
    u64 cnt_alive_tx_hang_timer;
    u64 cnt_tx_hang;
#endif
#endif
    u64 ephy_poll_timer_cnt;
    u64 mgmt_int_isr;
#ifdef LINUX	
    u64 ephy_link_change_cnt;
    u64 msix_int_isr;
    u64 msix_int_isr_cur;
    u64 msix_ch0_napi_isr;
    u64 msix_ch1_napi_isr;
    u64 msix_ch2_napi_isr;
    u64 msix_ch3_napi_isr;
#if FXGMAC_TX_INTERRUPT_EN
    u64 msix_ch0_napi_isr_tx;
    u64 msix_ch0_napi_napi_tx;
    u64 msix_ch0_napi_sch_tx;
#endif
#endif
};

struct fxgmac_ring_buf {
    struct sk_buff* skb;
#ifdef LINUX
    dma_addr_t skb_dma;
#endif	
    unsigned int skb_len;
};

/* Common Tx and Rx DMA hardware descriptor */
struct fxgmac_dma_desc {
    __le32 desc0;
    __le32 desc1;
    __le32 desc2;
    __le32 desc3;
};

/* Page allocation related values */
struct fxgmac_page_alloc {
    struct page* pages;
    unsigned int pages_len;
    unsigned int pages_offset;
#ifdef LINUX
    dma_addr_t pages_dma;
#endif	
};

/* Ring entry buffer data */
struct fxgmac_buffer_data {
    struct fxgmac_page_alloc pa;
    struct fxgmac_page_alloc pa_unmap;

#ifdef LINUX
    dma_addr_t dma_base;
#endif		
    unsigned long dma_off;
    unsigned int dma_len;
};

/* Tx-related desc data */
struct fxgmac_tx_desc_data {
    unsigned int packets;		/* BQL packet count */
    unsigned int bytes;		/* BQL byte count */
};

/* Rx-related desc data */
struct fxgmac_rx_desc_data {
    struct fxgmac_buffer_data hdr;	/* Header locations */
    struct fxgmac_buffer_data buf;	/* Payload locations */

    unsigned short hdr_len;		/* Length of received header */
    unsigned short len;		/* Length of received packet */
};

struct fxgmac_pkt_info {
    struct sk_buff* skb;

    unsigned int attributes;

    unsigned int errors;

    /* descriptors needed for this packet */
    unsigned int desc_count;
    unsigned int length;

    unsigned int tx_packets;
    unsigned int tx_bytes;

    unsigned int header_len;
    unsigned int tcp_header_len;
    unsigned int tcp_payload_len;
    unsigned short mss;

    unsigned short vlan_ctag;

    u64 rx_tstamp;

    u32 rss_hash;
    #ifdef LINUX
    enum pkt_hash_types rss_hash_type;
    #endif
};

struct fxgmac_desc_data {
    /* dma_desc: Virtual address of descriptor
     *  dma_desc_addr: DMA address of descriptor
     */
    struct fxgmac_dma_desc* dma_desc;
#ifdef LINUX	
    dma_addr_t dma_desc_addr;
#endif	

    /* skb: Virtual address of SKB
     *  skb_dma: DMA address of SKB data
     *  skb_dma_len: Length of SKB DMA area
     */
    struct sk_buff* skb;
#ifdef LINUX	
    dma_addr_t skb_dma;
#endif	
    unsigned int skb_dma_len;

    /* Tx/Rx -related data */
    struct fxgmac_tx_desc_data tx;
    struct fxgmac_rx_desc_data rx;

    unsigned int mapped_as_page;

    /* Incomplete receive save location.  If the budget is exhausted
     * or the last descriptor (last normal descriptor or a following
     * context descriptor) has not been DMA'd yet the current state
     * of the receive processing needs to be saved.
     */
    unsigned int state_saved;
    struct {
        struct sk_buff* skb;
        unsigned int len;
        unsigned int error;
    } state;
};

struct fxgmac_ring {
    /* Per packet related information */
    struct fxgmac_pkt_info pkt_info;

    /* Virtual/DMA addresses of DMA descriptor list and the total count */
    struct fxgmac_dma_desc* dma_desc_head;
#ifdef LINUX	
    dma_addr_t dma_desc_head_addr;
#endif	
    unsigned int dma_desc_count;

    /* Array of descriptor data corresponding the DMA descriptor
     * (always use the FXGMAC_GET_DESC_DATA macro to access this data)
     */
    struct fxgmac_desc_data* desc_data_head;

    /* Page allocation for RX buffers */
    struct fxgmac_page_alloc rx_hdr_pa;
    struct fxgmac_page_alloc rx_buf_pa;

    /* Ring index values
     *  cur   - Tx: index of descriptor to be used for current transfer
     *          Rx: index of descriptor to check for packet availability
     *  dirty - Tx: index of descriptor to check for transfer complete
     *          Rx: index of descriptor to check for buffer reallocation
     */
    unsigned int cur;
    unsigned int dirty;

    /* Coalesce frame count used for interrupt bit setting */
    unsigned int coalesce_count;

    //union {
        struct {
            unsigned int xmit_more;
            unsigned int queue_stopped;
            unsigned short cur_mss;
            unsigned short cur_vlan_ctag;
        } tx;
    //} ;
} ____cacheline_aligned;

struct fxgmac_channel {
    char name[16];

    /* Address of private data area for device */
    struct fxgmac_pdata* pdata;

    /* Queue index and base address of queue's DMA registers */
    unsigned int queue_index;
    //void __iomem *dma_regs;
    //u8 __iomem* dma_regs;
#if defined(UEFI)
    u32 __iomem dma_regs;
#elif defined(LINUX)
    void __iomem* dma_regs;
#elif defined(_WIN32) || defined(_WIN64)
    u8 __iomem* dma_regs;
#elif defined(UBOOT)
    u8 * dma_regs;
#else
    u32 __iomem dma_regs;
#endif

    /* Per channel interrupt irq number */
    u32 dma_irq;
#ifdef LINUX	
    char dma_irq_name[IFNAMSIZ + 32];
#endif

#if FXGMAC_TX_INTERRUPT_EN
    u32 dma_irq_tx; //for MSIx to match the type of struct msix_entry.vector
    char dma_irq_name_tx[IFNAMSIZ + 32];

    /* Netdev related settings */
    struct napi_struct napi_tx;
#endif	

#ifdef LINUX
    /* Netdev related settings */
    struct napi_struct napi_rx;
    struct timer_list tx_timer;

#ifdef FXGMAC_TX_HANG_TIMER_EN
    /* for tx hang checking. 20211227 */
    unsigned int tx_hang_timer_active;
    struct timer_list tx_hang_timer;
    unsigned int tx_hang_hw_cur;
#endif
#endif	

    unsigned int saved_ier;

    unsigned int tx_timer_active;
    
    struct fxgmac_ring* tx_ring;
    struct fxgmac_ring* rx_ring;
} ____cacheline_aligned;

struct fxphy_ag_adv {
    u8 auto_neg_en : 1;
    u8 full_1000m : 1;
    u8 half_1000m : 1;
    u8 full_100m : 1;
    u8 half_100m : 1;
    u8 full_10m : 1;
    u8 half_10m : 1;
};

struct fxgmac_desc_ops {
    int (*alloc_channles_and_rings)(struct fxgmac_pdata* pdata);
    void (*free_channels_and_rings)(struct fxgmac_pdata* pdata);
    int (*map_tx_skb)(struct fxgmac_channel* channel,
        struct sk_buff* skb);
    int (*map_rx_buffer)(struct fxgmac_pdata* pdata,
        struct fxgmac_ring* ring,
        struct fxgmac_desc_data* desc_data);
    void (*unmap_desc_data)(struct fxgmac_pdata* pdata,
        struct fxgmac_desc_data* desc_data);
    void (*tx_desc_init)(struct fxgmac_pdata* pdata);
    void (*rx_desc_init)(struct fxgmac_pdata* pdata);
};

struct fxgmac_hw_ops {
    int (*init)(struct fxgmac_pdata* pdata);
    int (*exit)(struct fxgmac_pdata* pdata);
    void (*save_nonstick_reg)(struct fxgmac_pdata* pdata);
    void (*restore_nonstick_reg)(struct fxgmac_pdata* pdata);
#if defined(UEFI) || defined(PXE)
    int (*set_gmac_register)(struct fxgmac_pdata* pdata, u32 address, unsigned int data);
    u32 (*get_gmac_register)(struct fxgmac_pdata* pdata, u32 address);
#else
    int (*set_gmac_register)(struct fxgmac_pdata* pdata, u8* address, unsigned int data);
    u32 (*get_gmac_register)(struct fxgmac_pdata* pdata, u8* address);
    void (*esd_restore_pcie_cfg)(struct fxgmac_pdata* pdata);
#endif
    
    int (*tx_complete)(struct fxgmac_dma_desc* dma_desc);

    void (*enable_tx)(struct fxgmac_pdata* pdata);
    void (*disable_tx)(struct fxgmac_pdata* pdata);
    void (*enable_rx)(struct fxgmac_pdata* pdata);
    void (*disable_rx)(struct fxgmac_pdata* pdata);

    int (*enable_int)(struct fxgmac_channel* channel,
        enum fxgmac_int int_id);
    int (*disable_int)(struct fxgmac_channel* channel,
        enum fxgmac_int int_id);
    void (*set_interrupt_moderation)(struct fxgmac_pdata* pdata);
    void (*enable_msix_rxtxinterrupt)(struct fxgmac_pdata* pdata);
    void (*disable_msix_interrupt)(struct fxgmac_pdata* pdata);
    void (*enable_msix_rxtxphyinterrupt)(struct fxgmac_pdata* pdata);
    void (*enable_msix_one_interrupt)(struct fxgmac_pdata* pdata, u32 intid);
    void (*disable_msix_one_interrupt)(struct fxgmac_pdata* pdata, u32 intid);
    bool (*enable_mgm_interrupt)(struct fxgmac_pdata* pdata);
    bool (*disable_mgm_interrupt)(struct fxgmac_pdata* pdata);
    
    void (*dev_xmit)(struct fxgmac_channel* channel);
    int (*dev_read)(struct fxgmac_channel* channel);

    int (*set_mac_address)(struct fxgmac_pdata* pdata, u8* addr);
    int (*set_mac_hash)(struct fxgmac_pdata* pdata);
    int (*config_rx_mode)(struct fxgmac_pdata* pdata);
    int (*enable_rx_csum)(struct fxgmac_pdata* pdata);
    int (*disable_rx_csum)(struct fxgmac_pdata* pdata);

    /* For MII speed configuration */
    int (*config_mac_speed)(struct fxgmac_pdata* pdata);
    int (*set_xlgmii_2500_speed)(struct fxgmac_pdata *pdata);
    int (*set_xlgmii_1000_speed)(struct fxgmac_pdata *pdata);
    int (*set_xlgmii_100_speed)(struct fxgmac_pdata *pdata);
    int (*get_xlgmii_phy_status)(struct fxgmac_pdata *pdata, u32 *speed, bool *link_up, bool link_up_wait_to_complete);

    /* For descriptor related operation */
    void (*tx_desc_init)(struct fxgmac_channel* channel);
    void (*rx_desc_init)(struct fxgmac_channel* channel);
    void (*tx_desc_reset)(struct fxgmac_desc_data* desc_data);
    void (*rx_desc_reset)(struct fxgmac_pdata* pdata,
        struct fxgmac_desc_data* desc_data,
        unsigned int index);
    int (*is_last_desc)(struct fxgmac_dma_desc* dma_desc);
    int (*is_context_desc)(struct fxgmac_dma_desc* dma_desc);
    void (*tx_start_xmit)(struct fxgmac_channel* channel,
        struct fxgmac_ring* ring);

    /* For Flow Control */
    int (*config_tx_flow_control)(struct fxgmac_pdata* pdata);
    int (*config_rx_flow_control)(struct fxgmac_pdata* pdata);

    /* For Jumbo Frames */
    int (*config_mtu)(struct fxgmac_pdata* pdata);
    int (*enable_jumbo)(struct fxgmac_pdata* pdata);

    /* For Vlan related config */
    int (*enable_tx_vlan)(struct fxgmac_pdata* pdata);
    int (*disable_tx_vlan)(struct fxgmac_pdata* pdata);
    int (*enable_rx_vlan_stripping)(struct fxgmac_pdata* pdata);
    int (*disable_rx_vlan_stripping)(struct fxgmac_pdata* pdata);
    int (*enable_rx_vlan_filtering)(struct fxgmac_pdata* pdata);
    int (*disable_rx_vlan_filtering)(struct fxgmac_pdata* pdata);
    int (*update_vlan_hash_table)(struct fxgmac_pdata* pdata);

    /* For RX coalescing */
    int (*config_rx_coalesce)(struct fxgmac_pdata* pdata);
    int (*config_tx_coalesce)(struct fxgmac_pdata* pdata);
    unsigned int (*usec_to_riwt)(struct fxgmac_pdata* pdata,
        unsigned int usec);
    unsigned int (*riwt_to_usec)(struct fxgmac_pdata* pdata,
        unsigned int riwt);

    /* For RX and TX threshold config */
    int (*config_rx_threshold)(struct fxgmac_pdata* pdata,
        unsigned int val);
    int (*config_tx_threshold)(struct fxgmac_pdata* pdata,
        unsigned int val);

    /* For RX and TX Store and Forward Mode config */
    int (*config_rsf_mode)(struct fxgmac_pdata* pdata,
        unsigned int val);
    int (*config_tsf_mode)(struct fxgmac_pdata* pdata,
        unsigned int val);

    /* For TX DMA Operate on Second Frame config */
    int (*config_osp_mode)(struct fxgmac_pdata* pdata);

    /* For RX and TX PBL config */
    int (*config_rx_pbl_val)(struct fxgmac_pdata* pdata);
    int (*get_rx_pbl_val)(struct fxgmac_pdata* pdata);
    int (*config_tx_pbl_val)(struct fxgmac_pdata* pdata);
    int (*get_tx_pbl_val)(struct fxgmac_pdata* pdata);
    int (*config_pblx8)(struct fxgmac_pdata* pdata);

    /* For MMC statistics */
    void (*rx_mmc_int)(struct fxgmac_pdata* pdata);
    void (*tx_mmc_int)(struct fxgmac_pdata* pdata);
    void (*read_mmc_stats)(struct fxgmac_pdata* pdata);
    bool (*update_stats_counters)(struct fxgmac_pdata* pdata, bool ephy_check_en);

    /* For Receive Side Scaling */
    int (*enable_rss)(struct fxgmac_pdata* pdata);
    int (*disable_rss)(struct fxgmac_pdata* pdata);
    u32(*get_rss_options)(struct fxgmac_pdata* pdata);
    int (*set_rss_options)(struct fxgmac_pdata* pdata);
    int (*set_rss_hash_key)(struct fxgmac_pdata* pdata, const u8* key);
    int (*set_rss_lookup_table)(struct fxgmac_pdata* pdata, const u32* table);

    /*For Offload*/
#if defined(LINUX) || defined(_WIN64) || defined(_WIN32)
    void (*set_arp_offload)(struct fxgmac_pdata* pdata, unsigned char* ip_addr);//int XlgmacSetArpload(struct fxgmac_pdata* pdata,unsigned char *ip_addr)
    int (*enable_arp_offload)(struct fxgmac_pdata* pdata);//int XlgmacEnableArpload(struct fxgmac_pdata* pdata)
    int (*disable_arp_offload)(struct fxgmac_pdata* pdata);

    /*NS offload*/
    int  (*set_ns_offload)(
        struct fxgmac_pdata* pdata,
        unsigned int index,
        unsigned char* remote_addr,
        unsigned char* solicited_addr,
        unsigned char* target_addr1,
        unsigned char* target_addr2,
        unsigned char* mac_addr);
    int (*enable_ns_offload)(struct fxgmac_pdata* pdata);//int XlgmacEnableArpload(struct fxgmac_pdata* pdata,unsigned char *ip_addr)
    int (*disable_ns_offload)(struct fxgmac_pdata* pdata);

    int (*enable_wake_magic_pattern)(struct fxgmac_pdata* pdata);
    int (*disable_wake_magic_pattern)(struct fxgmac_pdata* pdata);

    int (*enable_wake_link_change)(struct fxgmac_pdata* pdata);
    int (*disable_wake_link_change)(struct fxgmac_pdata* pdata);

    int (*check_wake_pattern_fifo_pointer)(struct fxgmac_pdata* pdata);
    int (*set_wake_pattern)(struct fxgmac_pdata* pdata, struct wol_bitmap_pattern* wol_pattern, u32 pattern_cnt);
    int (*enable_wake_pattern)(struct fxgmac_pdata* pdata);//int XlgmacEnableArpload(struct fxgmac_pdata* pdata,unsigned char *ip_addr)
    int (*disable_wake_pattern)(struct fxgmac_pdata* pdata);
    int (*set_wake_pattern_mask)(struct fxgmac_pdata* pdata, u32 filter_index, u8 register_index, u32 Data);
#if defined(FUXI_PM_WPI_READ_FEATURE_EN) && FUXI_PM_WPI_READ_FEATURE_EN
    void (*get_wake_packet_indication)(struct fxgmac_pdata* pdata, int* wake_reason, u32* wake_pattern_number, u8* wpi_buf, u32 buf_size, u32* packet_size);
    void (*enable_wake_packet_indication)(struct fxgmac_pdata* pdata, int en);
#endif
#endif

    void (*reset_phy)(struct fxgmac_pdata* pdata);
    /*for release phy,phy write and read, and provide clock to GMAC. */
    void (*release_phy)(struct fxgmac_pdata* pdata);
#if !defined(UEFI)
    void (*enable_phy_check)(struct fxgmac_pdata* pdata);
    void (*disable_phy_check)(struct fxgmac_pdata* pdata);
    void (*setup_cable_loopback)(struct fxgmac_pdata* pdata);
    void (*clean_cable_loopback)(struct fxgmac_pdata* pdata);
    void (*disable_phy_sleep)(struct fxgmac_pdata* pdata);
    void (*enable_phy_sleep)(struct fxgmac_pdata* pdata);
    void (*phy_green_ethernet)(struct fxgmac_pdata* pdata);
    void (*phy_eee_feature)(struct fxgmac_pdata* pdata);
#endif
    int (*get_ephy_state)(struct fxgmac_pdata* pdata);
    int (*write_ephy_reg)(struct fxgmac_pdata* pdata, u32 val, u32 data);
    int (*read_ephy_reg)(struct fxgmac_pdata* pdata, u32 val, u32 *data);
    int (*set_ephy_autoneg_advertise)(struct fxgmac_pdata* pdata,  struct fxphy_ag_adv phy_ag_adv);
    int (*phy_config)(struct fxgmac_pdata* pdata);
    void (*close_phy_led)(struct fxgmac_pdata* pdata);
    void (*led_under_active)(struct fxgmac_pdata* pdata);
    void (*led_under_sleep)(struct fxgmac_pdata* pdata);
    void (*led_under_shutdown)(struct fxgmac_pdata* pdata);
    void (*led_under_disable)(struct fxgmac_pdata* pdata);

    /* For power management */
    void (*pre_power_down)(struct fxgmac_pdata* pdata, bool phyloopback);
#if defined(LINUX)
    void (*config_power_down)(struct fxgmac_pdata *pdata, unsigned int wol);
#else
    void (*config_power_down)(struct fxgmac_pdata* pdata, unsigned int offloadcount, bool magic_en, bool remote_pattern_en);
#endif	
    void (*config_power_up)(struct fxgmac_pdata* pdata);
    unsigned char (*set_suspend_int)(void* pdata);
    void(*set_resume_int)(struct fxgmac_pdata* pdata);
    int (*set_suspend_txrx)(struct fxgmac_pdata* pdata);
    void (*set_pwr_clock_gate)(struct fxgmac_pdata* pdata);
    void (*set_pwr_clock_ungate)(struct fxgmac_pdata* pdata);

    /* for multicast address list */
    int (*set_all_multicast_mode)(struct fxgmac_pdata* pdata, unsigned int enable);
    void (*config_multicast_mac_hash_table)(struct fxgmac_pdata* pdata, unsigned char* pmc_mac, int b_add);

    /* for packet filter-promiscuous and broadcast */
    int (*set_promiscuous_mode)(struct fxgmac_pdata* pdata, unsigned int enable);
    int (*enable_rx_broadcast)(struct fxgmac_pdata* pdata, unsigned int enable);

    /* efuse relevant operation. */
    bool (*read_patch_from_efuse)(struct fxgmac_pdata* pdata, u32 offset, u32* value); /* read patch per index. */
    bool (*read_patch_from_efuse_per_index)(struct fxgmac_pdata* pdata, u8 index, u32* offset, u32* value); /* read patch per index. */
    bool (*write_patch_to_efuse)(struct fxgmac_pdata* pdata, u32 offset, u32 value);
    bool (*write_patch_to_efuse_per_index)(struct fxgmac_pdata* pdata, u8 index, u32 offset, u32 value);
    bool (*read_mac_subsys_from_efuse)(struct fxgmac_pdata* pdata, u8* mac_addr, u32* subsys, u32* revid);
    bool (*write_mac_subsys_to_efuse)(struct fxgmac_pdata* pdata, u8* mac_addr, u32* subsys, u32* revid);
    bool (*read_mac_addr_from_efuse)(struct fxgmac_pdata* pdata, u8* mac_addr);
    bool (*write_mac_addr_to_efuse)(struct fxgmac_pdata* pdata, u8* mac_addr);
    bool (*efuse_load)(struct fxgmac_pdata* pdata);
    bool (*read_efuse_data)(struct fxgmac_pdata* pdata, u32 offset, u32* value);
    bool (*write_oob)(struct fxgmac_pdata* pdata);
    bool (*write_led)(struct fxgmac_pdata* pdata, u32 value);
    bool (*read_led_config)(struct fxgmac_pdata* pdata);
    bool (*write_led_config)(struct fxgmac_pdata* pdata);

    int (*pcie_init)(struct fxgmac_pdata* pdata, bool ltr_en, bool aspm_l1ss_en, bool aspm_l1_en, bool aspm_l0s_en);
    void (*trigger_pcie)(struct fxgmac_pdata* pdata, u32 code); // To trigger pcie sniffer for analysis.
};

/* This structure contains flags that indicate what hardware features
 * or configurations are present in the device.
 */
struct fxgmac_hw_features {
    /* HW Version */
    unsigned int version;

    /* HW Feature Register0 */
    unsigned int phyifsel;		/* PHY interface support */
    unsigned int vlhash;		/* VLAN Hash Filter */
    unsigned int sma;		/* SMA(MDIO) Interface */
    unsigned int rwk;		/* PMT remote wake-up packet */
    unsigned int mgk;		/* PMT magic packet */
    unsigned int mmc;		/* RMON module */
    unsigned int aoe;		/* ARP Offload */
    unsigned int ts;		/* IEEE 1588-2008 Advanced Timestamp */
    unsigned int eee;		/* Energy Efficient Ethernet */
    unsigned int tx_coe;		/* Tx Checksum Offload */
    unsigned int rx_coe;		/* Rx Checksum Offload */
    unsigned int addn_mac;		/* Additional MAC Addresses */
    unsigned int ts_src;		/* Timestamp Source */
    unsigned int sa_vlan_ins;	/* Source Address or VLAN Insertion */

    /* HW Feature Register1 */
    unsigned int rx_fifo_size;	/* MTL Receive FIFO Size */
    unsigned int tx_fifo_size;	/* MTL Transmit FIFO Size */
    unsigned int adv_ts_hi;		/* Advance Timestamping High Word */
    unsigned int dma_width;		/* DMA width */
    unsigned int dcb;		/* DCB Feature */
    unsigned int sph;		/* Split Header Feature */
    unsigned int tso;		/* TCP Segmentation Offload */
    unsigned int dma_debug;		/* DMA Debug Registers */
    unsigned int rss;		/* Receive Side Scaling */
    unsigned int tc_cnt;		/* Number of Traffic Classes */
    unsigned int avsel;		/* AV Feature Enable */
    unsigned int ravsel;		/* Rx Side Only AV Feature Enable */
    unsigned int hash_table_size;	/* Hash Table Size */
    unsigned int l3l4_filter_num;	/* Number of L3-L4 Filters */

    /* HW Feature Register2 */
    unsigned int rx_q_cnt;		/* Number of MTL Receive Queues */
    unsigned int tx_q_cnt;		/* Number of MTL Transmit Queues */
    unsigned int rx_ch_cnt;		/* Number of DMA Receive Channels */
    unsigned int tx_ch_cnt;		/* Number of DMA Transmit Channels */
    unsigned int pps_out_num;	/* Number of PPS outputs */
    unsigned int aux_snap_num;	/* Number of Aux snapshot inputs */

    /* HW Feature Register3 */
    u32 hwfr3;
};

struct fxgmac_resources {
#ifdef LINUX	
    void __iomem *addr;
#endif	
    int irq;
};

#ifndef LINUX
struct net_device;
struct device;
struct pci_dev;
#endif

#define FXGAMC_MAX_DATA_SIZE (1024*4+16)
struct per_regisiter_info
{
    unsigned int           size;
    unsigned int           address;
    unsigned int           value;
    unsigned char          data[FXGAMC_MAX_DATA_SIZE];
};

struct ext_command_data {
    u32 val0;
    u32 val1;
};

typedef struct per_regisiter_info   PER_REG_INFO;
typedef struct ext_command_data     CMD_DATA;
#ifdef UBOOT
typedef enum _media_nic_connect_state
{
    NIC_CONNECT_STATE_UNKNOWN,
    NIC_CONNECT_STATE_CONNECTED,
    NIC_CONNECT_STATE_DISCONNECTED
}nic_connect_state;
#endif
struct fxgmac_pdata {
    struct net_device               *netdev;
    struct device                   *dev;
#ifdef UBOOT
    struct udevice                  *pdev;
#else
    struct pci_dev                  *pdev;
#endif
    void                            *pAdapter;
    
#ifdef UBOOT
    const char *                    name;
    u32                             card_num;

    /* pcie info */
    u16                             pci_devid;
    u16                             pci_venid;
    u8                              pci_revid;
    u16                             SubVendorID;
    u16                             SubSystemID;
#endif

    struct fxgmac_hw_ops            hw_ops;
    struct fxgmac_desc_ops          desc_ops;

    /* Device statistics */
    struct fxgmac_stats             stats;

    u32                             msg_enable;
    u32                             reg_nonstick[0x300 >> 2];
#if !defined(UEFI) && !defined(PXE)
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
#endif

    /* MAC registers base */
    //void __iomem *mac_regs;
#if defined(UEFI)
    u32 __iomem                      mac_regs;
    u32 __iomem                      base_mem;
#elif defined(LINUX)
    void __iomem* mac_regs;
    void __iomem* base_mem;	/* for sync with Window driver. */
#elif defined(_WIN32) || defined(_WIN64)
    u8 __iomem* mac_regs;
    u8 __iomem* base_mem;
#elif defined(UBOOT)
    void *                          mac_regs;
    void *                          base_mem; 
#else
    u32 __iomem                      mac_regs;
    u32 __iomem                      base_mem;
#endif

    /* Hardware features of the device */
    struct fxgmac_hw_features       hw_feat;

#ifdef LINUX
    struct work_struct restart_work;
#endif	

    /* Rings for Tx/Rx on a DMA channel */
    struct fxgmac_channel           *channel_head;
    unsigned int                    channel_count;
    unsigned int                    tx_ring_count;
    unsigned int                    rx_ring_count;
    unsigned int                    tx_desc_count;
    unsigned int                    rx_desc_count;
    unsigned int                    tx_q_count;
    unsigned int                    rx_q_count;
    
#ifdef UBOOT    
    /* dma descriptor */
    u32                             tx_tail;
    u32                             rx_tail;
    struct fxgmac_dma_desc *        tx_desc_list; 
    struct fxgmac_dma_desc *        rx_desc_list; 
    u8*                             rx_buffer;
    
    /* Rings for Tx/Rx on a DMA channel */
    struct fxgmac_channel *         tx_channel;
    struct fxgmac_channel *         rx_channel;

    /* link info */
    nic_connect_state               phy_link;
    //uint16_t                        LinkSpeed;     // actual link speed setting
    //uint8_t                         Duplex;        // Duplex set
#endif

    /* Tx/Rx common settings */
    unsigned int                    pblx8;

    /* Tx settings */
    unsigned int                    tx_sf_mode;
    unsigned int                    tx_threshold;
    unsigned int                    tx_pbl;
    unsigned int                    tx_osp_mode;
#if FXGMAC_TX_HANG_TIMER_EN
    /* for tx hang checking. 20211227 */
    unsigned int tx_hang_restart_queuing;
#endif	

    /* Rx settings */
    unsigned int                    rx_sf_mode;
    unsigned int                    rx_threshold;
    unsigned int                    rx_pbl;

    /* Tx coalescing settings */
    unsigned int                    tx_usecs;
    unsigned int                    tx_frames;

    /* Rx coalescing settings */
    unsigned int                    rx_riwt;
    unsigned int                    rx_usecs;
    unsigned int                    rx_frames;

    /* Current Rx buffer size */
    unsigned int                    rx_buf_size;

    /* Flow control settings */
    unsigned int                    tx_pause;
    unsigned int                    rx_pause;

    /* Jumbo frames */
    unsigned int                    mtu;
    unsigned int                    jumbo;

    /* CRC checking */
    unsigned int                    crc_check;

    /* MSIX */
    unsigned int                    msix;

    /* RSS */
    unsigned int                    rss;
   
    /* VlanID */
    unsigned int                    vlan;
    unsigned int                    vlan_exist;
    unsigned int                    vlan_filter;
    unsigned int                    vlan_strip;

    /* Interrupt Moderation */
    unsigned int                    intr_mod;
	unsigned int                    intr_mod_timer;

    /* Device interrupt number */
    int                             dev_irq;
    unsigned int                    per_channel_irq;
#if FXGMAC_TX_INTERRUPT_EN
    u32 channel_irq[FXGMAC_MAX_DMA_CHANNELS_PLUS_1TX]; // change type from int to u32 to match MSIx, p_msix_entry.vector;
#else
    u32 channel_irq[FXGMAC_MAX_DMA_CHANNELS]; // change type from int to u32 to match MSIx, p_msix_entry.vector;
#endif
#ifdef LINUX
    u32 int_flags; //legacy, msi or msix
#ifdef CONFIG_PCI_MSI
    //20210526 for MSIx
    struct msix_entry *p_msix_entries;
    int num_msix_vectors;
#endif

    /* power management and wol, 20210622 */
    u32 wol; //wol options
    unsigned long powerstate; //20210708, power state
    unsigned int ns_offload_tab_idx; //20220120, for ns-offload table. 2 entries supported.
    CURRENT_STATE current_state;
#endif

    /* Netdev related settings */
    unsigned char                   mac_addr[ETH_ALEN];
#ifdef LINUX	
    netdev_features_t netdev_features;
    struct napi_struct            napi;
    struct timer_list phy_poll_tm;
#endif	

    /* Filtering support */
#if FXGMAC_FILTER_MULTIPLE_VLAN_ENABLED	
    unsigned long                 active_vlans[BITS_TO_LONGS(VLAN_N_VID)];
#endif	

    /* Device clocks */
    unsigned long                   sysclk_rate;

    /* RSS addressing mutex */
#ifdef LINUX	
    struct mutex rss_mutex;
    spinlock_t txpoll_lock;
#endif

    /* Receive Side Scaling settings */
    u8                              rss_key[FXGMAC_RSS_HASH_KEY_SIZE];
    u32                             rss_table[FXGMAC_RSS_MAX_TABLE_SIZE];
    u32                             rss_options;

#ifdef LINUX
    bool                          phy_link;
#endif
    int                             phy_speed;
    int                             phy_duplex;
    int                             phy_autoeng;

    char                            drv_name[32];
    char                            drv_ver[32];

    struct wol_bitmap_pattern   pattern[MAX_PATTERN_COUNT];

    struct led_setting          led;
    struct led_setting          ledconfig;

#ifdef LINUX
    bool            fxgmac_test_tso_flag;
    u32             fxgmac_test_tso_seg_num;
    u32             fxgmac_test_last_tso_len;
    u32             fxgmac_test_packet_len;
    volatile u32    fxgmac_test_skb_arr_in_index;
    volatile u32    fxgmac_test_skb_arr_out_index;
    CMD_DATA        ex_cmd_data; 
    PER_REG_INFO    per_reg_data;
    struct sk_buff  *fxgmac_test_skb_array[FXGMAC_MAX_DBG_TEST_PKT];
#ifdef HAVE_FXGMAC_DEBUG_FS
    struct dentry   *dbg_adapter;
    struct dentry   *fxgmac_dbg_root;
#endif
#endif
};

//#ifdef CONFIG_PCI_MSI
#if 1
#define FXGMAC_FLAG_MSI_CAPABLE                  (u32)(1 << 0)
#define FXGMAC_FLAG_MSI_ENABLED                  (u32)(1 << 1)
#define FXGMAC_FLAG_MSIX_CAPABLE                 (u32)(1 << 2)
#define FXGMAC_FLAG_MSIX_ENABLED                 (u32)(1 << 3)
#endif

/* yzhang added 0425 for ethtool msglvl control
 *
 * below are defined in netdevice.h
 
#define netif_msg_is_on(pdata)	((pdata)->msg_enable)
#define netif_msg_drv(pdata)	((pdata)->msg_enable & NETIF_MSG_DRV)
#define netif_msg_probe(pdata)	((pdata)->msg_enable & NETIF_MSG_PROBE)
#define netif_msg_link(pdata)	((pdata)->msg_enable & NETIF_MSG_LINK)
#define netif_msg_timer(pdata)	((pdata)->msg_enable & NETIF_MSG_TIMER)
#define netif_msg_ifdown(pdata)	((pdata)->msg_enable & NETIF_MSG_IFDOWN)
#define netif_msg_ifup(pdata)	((pdata)->msg_enable & NETIF_MSG_IFUP)
#define netif_msg_rx_err(pdata)	((pdata)->msg_enable & NETIF_MSG_RX_ERR)
#define netif_msg_tx_err(pdata)	((pdata)->msg_enable & NETIF_MSG_TX_ERR)
#define netif_msg_tx_queued(pdata)	((pdata)->msg_enable & NETIF_MSG_TX_QUEUED)
#define netif_msg_intr(pdata)	((pdata)->msg_enable & NETIF_MSG_INTR)
#define netif_msg_tx_done(pdata)	((pdata)->msg_enable & NETIF_MSG_TX_DONE)
#define netif_msg_rx_status(pdata)	((pdata)->msg_enable & NETIF_MSG_RX_STATUS)
#define netif_msg_pktdata(pdata)	((pdata)->msg_enable & NETIF_MSG_PKTDATA)

NETIF_MSG_DRV		= 0x0001,
NETIF_MSG_PROBE 	= 0x0002,
NETIF_MSG_LINK		= 0x0004,
NETIF_MSG_TIMER 	= 0x0008,
NETIF_MSG_IFDOWN	= 0x0010,
NETIF_MSG_IFUP		= 0x0020,
NETIF_MSG_RX_ERR	= 0x0040,
NETIF_MSG_TX_ERR	= 0x0080,
NETIF_MSG_TX_QUEUED = 0x0100,
NETIF_MSG_INTR		= 0x0200,
NETIF_MSG_TX_DONE	= 0x0400,
NETIF_MSG_RX_STATUS = 0x0800,
NETIF_MSG_PKTDATA	= 0x1000,
NETIF_MSG_HW		= 0x2000,
NETIF_MSG_WOL		= 0x4000,
*/

#ifdef LINUX
extern u32 default_msg_level;
#ifdef CONFIG_PCI_MSI
extern u32 pcidev_int_mode; // for msi or msix

//for MSIx, 20210526
extern struct msix_entry *msix_entries;
extern int req_vectors;
#endif
#endif

void fxgmac_init_desc_ops(struct fxgmac_desc_ops *desc_ops);
void fxgmac_init_hw_ops(struct fxgmac_hw_ops *hw_ops);
const struct net_device_ops *fxgmac_get_netdev_ops(void);
const struct ethtool_ops *fxgmac_get_ethtool_ops(void);
void fxgmac_dump_tx_desc(struct fxgmac_pdata *pdata,
    struct fxgmac_ring *ring,
    unsigned int idx,
    unsigned int count,
    unsigned int flag);
void fxgmac_dump_rx_desc(struct fxgmac_pdata *pdata,
    struct fxgmac_ring *ring,
    unsigned int idx);
void fxgmac_dbg_pkt(struct net_device *netdev,
    struct sk_buff *skb, bool tx_rx);
void fxgmac_get_all_hw_features(struct fxgmac_pdata *pdata);
void fxgmac_print_all_hw_features(struct fxgmac_pdata *pdata);
int fxgmac_drv_probe(struct device *dev,
    struct fxgmac_resources *res);
int fxgmac_drv_remove(struct device *dev);

#ifdef LINUX
int  fxgmac_init(struct fxgmac_pdata *pdata, bool save_private_reg);
void fxgmac_set_pattern_data(struct fxgmac_pdata *pdata);
/* for phy interface */
int  fxgmac_config_mac_speed(struct fxgmac_pdata *pdata);
void fxgmac_act_phy_link(struct fxgmac_pdata *pdata);
int  fxgmac_phy_timer_init(struct fxgmac_pdata *pdata);
void fxgmac_phy_timer_destroy(struct fxgmac_pdata *pdata);
void fxgmac_phy_timer_resume(struct fxgmac_pdata *pdata);
int  fxgmac_ephy_autoneg_ability_get(struct fxgmac_pdata *pdata, unsigned int *cap_mask);
int  fxgmac_ephy_status_get(/*u32 lport*/struct fxgmac_pdata *pdata, int* speed, int* duplex, int* ret_link, int *media);
int  fxgmac_ephy_soft_reset(struct fxgmac_pdata *pdata);
void fxgmac_phy_force_speed(struct fxgmac_pdata *pdata, int speed);
void fxgmac_phy_force_duplex(struct fxgmac_pdata *pdata, int duplex);
void fxgmac_phy_force_autoneg(struct fxgmac_pdata *pdata, int autoneg);

unsigned int    fxgmac_get_netdev_ip4addr(struct fxgmac_pdata *pdata);
unsigned char * fxgmac_get_netdev_ip6addr(struct fxgmac_pdata *pdata, unsigned char *ipval, unsigned char *ip6addr_solicited, unsigned int ifa_flag);
void fxgmac_update_aoe_ipv4addr(struct fxgmac_pdata *pdata, u8 *ip_addr);
void fxgmac_update_ns_offload_ipv6addr(struct fxgmac_pdata *pdata, unsigned int param);

/* for efuse */
bool fxgmac_read_patch_from_efuse(struct fxgmac_pdata* pdata, u32 offset, u32* value); /* read patch per register offset. */
bool fxgmac_read_patch_from_efuse_per_index(struct fxgmac_pdata* pdata, u8 index, u32* offset, u32* value); /* read patch per 0-based index. */
bool fxgmac_write_patch_to_efuse(struct fxgmac_pdata* pdata, u32 offset, u32 value);
bool fxgmac_write_patch_to_efuse_per_index(struct fxgmac_pdata* pdata, u8 index, u32 offset, u32 value);
bool fxgmac_read_mac_subsys_from_efuse(struct fxgmac_pdata* pdata, u8* mac_addr, u32* subsys, u32* revid);
bool fxgmac_write_mac_addr_to_efuse(struct fxgmac_pdata* pdata, u8* mac_addr);
bool fxgmac_read_subsys_from_efuse(struct fxgmac_pdata* pdata, u32* subsys, u32* revid);
bool fxgmac_write_subsys_to_efuse(struct fxgmac_pdata* pdata, u32* subsys, u32* revid);
bool fxgmac_efuse_load(struct fxgmac_pdata* pdata);


#if (FXGMAC_RSS_FEATURE_ENABLED)
u32 fxgmac_read_rss_options(struct fxgmac_pdata *pdata);
int fxgmac_write_rss_options(struct fxgmac_pdata *pdata);
int fxgmac_read_rss_hash_key(struct fxgmac_pdata *pdata, u8 *key_buf);
int fxgmac_write_rss_hash_key(struct fxgmac_pdata *pdata);
int fxgmac_write_rss_lookup_table(struct fxgmac_pdata *pdata);
#endif

#if FXGMAC_WOL_FEATURE_ENABLED
void fxgmac_config_wol(struct fxgmac_pdata *pdata, int en);
#endif

#if FXGMAC_PM_FEATURE_ENABLED
void fxgmac_net_powerdown(struct fxgmac_pdata *pdata, unsigned int wol);
void fxgmac_net_powerup(struct fxgmac_pdata *pdata);

#endif

#if FXGMAC_SANITY_CHECK_ENABLED
int fxgmac_diag_sanity_check(struct fxgmac_pdata *pdata);
#endif
#else
int  fxgmac_init(struct fxgmac_pdata* pdata, unsigned int rx_desc_count, unsigned int tx_desc_count, bool save_nonstick_reg);
int  fxgmac_open(struct fxgmac_pdata* pdata);
int  fxgmac_close(struct fxgmac_pdata* pdata);
void nic_enable_rx_tx_ints(struct fxgmac_pdata* pdata);
void fxgmac_disable_rx_tx_ints(struct fxgmac_pdata* pdata);

int  dma_start(struct fxgmac_pdata* pdata);
int  fxgmac_start(struct fxgmac_pdata* pdata);
void fxgmac_stop(struct fxgmac_pdata* pdata);
void fxgmac_suspend(struct fxgmac_pdata* pdata);	
#endif

#if defined(UEFI)

#define FXGMAC_DEBUG

/* For debug prints */
#ifdef FXGMAC_DEBUG
/*#define FXGMAC_PR(fmt, args...) \
    pr_alert("[%s,%d]:" fmt, __func__, __LINE__, ## args)*/

#define FXGMAC_PR(fmt, ...) \
    DBGPRINT(MP_TRACE, ("[%a,%d]:" fmt, __func__, __LINE__, __VA_ARGS__))

#else
#define FXGMAC_PR(x...)		do { } while (0)
#endif
#define netif_msg_drv(pdata) FALSE
#define printk(x,...) do { } while (0)
#define DPRINTK(x,...) do { } while (0)

#define netif_dbg(a, b, c, fmt, ...) \
    DBGPRINT(MP_TRACE, ("[%a,%d]:" fmt, __func__, __LINE__, __VA_ARGS__))

#define netif_info(a, b, c, fmt, ...) \
    DBGPRINT(MP_INFO, ("[%a,%d]:" fmt, __func__, __LINE__, __VA_ARGS__))

#define netdev_dbg(a, fmt, ...) \
    DBGPRINT(MP_TRACE, ("[%a,%d]:" fmt, __func__, __LINE__, __VA_ARGS__))

#define netdev_err(a, fmt, ...) \
    DBGPRINT(MP_ERROR, ("[%a,%d]:" fmt, __func__, __LINE__, __VA_ARGS__))

#define netdev_alert(a, fmt, ...) \
    DBGPRINT(MP_WARN, ("[%a,%d]:" fmt, __func__, __LINE__, __VA_ARGS__))

#elif defined(_WIN32) || defined(_WIN64)

#define FXGMAC_DEBUG

/* For debug prints */
#ifdef FXGMAC_DEBUG
/*#define FXGMAC_PR(fmt, args...) \
    pr_alert("[%s,%d]:" fmt, __func__, __LINE__, ## args)*/

#define FXGMAC_PR(fmt, ...) \
    DBGPRINT(MP_TRACE, ("[%s,%d]:" fmt, __func__, __LINE__, __VA_ARGS__))

#else
#define FXGMAC_PR(x...)		do { } while (0)
#endif

#define netif_msg_drv(pdata) FALSE
#define printk(x,...) do { } while (0)
#define DPRINTK(x,...) do { } while (0)//DBGPRINT(MP_LOUD,(x,__VA_ARGS__))

#define netif_dbg(a, b, c, fmt, ...) \
    DBGPRINT(MP_TRACE, ("[%s,%d]:" fmt, __func__, __LINE__, __VA_ARGS__))

#define netif_info(a, b, c, fmt, ...) \
    DBGPRINT(MP_INFO, ("[%s,%d]:" fmt, __func__, __LINE__, __VA_ARGS__))

#define netdev_dbg(a, fmt, ...) \
    DBGPRINT(MP_TRACE, ("[%s,%d]:" fmt, __func__, __LINE__, __VA_ARGS__))

#define netdev_err(a, fmt, ...) \
    DBGPRINT(MP_ERROR, ("[%s,%d]:" fmt, __func__, __LINE__, __VA_ARGS__))

#define netdev_alert(a, fmt, ...) \
    DBGPRINT(MP_WARN, ("[%s,%d]:" fmt, __func__, __LINE__, __VA_ARGS__))

#elif defined(LINUX)

/* For debug prints */
#ifdef FXGMAC_DEBUG
#define FXGMAC_PR(fmt, args...) \
    pr_alert("[%s,%d]:" fmt, __func__, __LINE__, ## args)

#define DPRINTK printk
#else
#define FXGMAC_PR(x...)		do { } while (0)
#define DPRINTK(x...)
#endif

#elif defined(UBOOT)

#define FXGMAC_ERR(NIC, fmt, args...) \
	printf("YT6801: %s: ERROR: " fmt, (NIC)->name ,##args)
#define DEBUGOUT(fmt, args...)	printf(fmt ,##args)

#ifdef DBG
#define FXGMAC_DEBUG(NIC, fmt, args...) \
	printf("YT6801: %s: DEBUG: " fmt, (NIC)->name ,##args)
#define DEBUGFUNC()		printf("%s\n", __func__);
#define DPRINTK(fmt, args...) printf(fmt ,##args)
#define DBGPRINT(Level, Fmt) printf Fmt
#define FXGMAC_PR(fmt, args...) DBGPRINT(MP_TRACE, (fmt,##args))
#else
#define FXGMAC_DEBUG(HW, args...)	do { } while (0)
#define DEBUGFUNC()		do { } while (0)
#define FXGMAC_PR(fmt, ...) do { } while (0)
#define DPRINTK(fmt, args...) do {} while (0)
#define DBGPRINT(Level, Fmt)
#endif

#define netif_dbg(a, b, c, fmt, ...) 
#define netif_info(a, b, c, fmt, ...) 
#define netdev_dbg(a, fmt, ...) 
#define netdev_err(a, fmt, ...) 
#define netdev_alert(a, fmt, ...) 

#define netif_msg_drv(pdata) false
//#define DbgPrintF(level,  fmt, ...)

#else
//PXE
#define FXGMAC_DEBUG

#define FXGMAC_PR(foo)

#define netif_dbg(foo)

#define netif_info(foo)

#define netdev_dbg(foo)

#define netdev_err(foo)

#define netdev_alert(foo)

#define printk(fmt)
#endif

#ifdef UEFI
#elif defined(_WIN32) || defined(_WIN64)
extern bool gFxLpTestFlag;
#endif

#ifdef LINUX
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
#if 0
    union {
        struct ext_command_buf cmdu_buf;
        struct ext_command_data cmdu_data;
        struct ext_command_mac  cmdu_mac;
        struct ext_command_mii  cmdu_mii;
    }cmd_cmdu;
#endif
};

typedef struct _fxgmac_test_buf {
    u8* addr;
    u32 offset;
    u32 length;
} fxgmac_test_buf, *pfxgmac_test_buf;

#define MAX_PKT_BUF                 1

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

#ifdef HAVE_FXGMAC_DEBUG_FS
void fxgmac_dbg_adapter_init(struct fxgmac_pdata *pdata);
void fxgmac_dbg_adapter_exit(struct fxgmac_pdata *pdata);
void fxgmac_dbg_init(struct fxgmac_pdata *pdata);
void fxgmac_dbg_exit(struct fxgmac_pdata *pdata);
#endif /* HAVE_FXGMAC_DEBUG_FS */

void fxgmac_restart_dev(struct fxgmac_pdata *pdata);
long fxgmac_dbg_netdev_ops_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#endif 


#endif /* __FUXI_GMAC_H__ */
