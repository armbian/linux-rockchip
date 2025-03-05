/*
 * Linux OS Independent Layer
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

#ifndef _linux_osl_h_
#define _linux_osl_h_

#include <typedefs.h>
#include <linuxerrmap.h>

#define DECLSPEC_ALIGN(x)	__attribute__ ((aligned(x)))

/* Linux Kernel: File Operations: start */
static INLINE void * osl_os_open_image(char * filename)
	{ return NULL; }
static INLINE void osl_os_close_image(void * image)
	{ return; }
static INLINE int osl_os_get_image_block(char * buf, int len, void * image)
	{ return 0; }
static INLINE int osl_os_image_size(void *image)
	{ return 0; }
/* Linux Kernel: File Operations: end */

#ifdef BCMDRIVER

/* OSL initialization */
#ifdef SHARED_OSL_CMN
extern osl_t *osl_attach(void *pdev, uint bustype, bool pkttag, void **osh_cmn);
#else
extern osl_t *osl_attach(void *pdev, uint bustype, bool pkttag);
#endif /* SHARED_OSL_CMN */

extern void osl_detach(osl_t *osh);
extern int osl_static_mem_init(osl_t *osh, void *adapter);
extern int osl_static_mem_deinit(osl_t *osh, void *adapter);
extern void osl_set_bus_handle(osl_t *osh, void *bus_handle);
extern void* osl_get_bus_handle(osl_t *osh);
#ifdef DHD_MAP_LOGGING
extern void osl_dma_map_dump(osl_t *osh);
#define OSL_DMA_MAP_DUMP(osh)	osl_dma_map_dump(osh)
#else
#define OSL_DMA_MAP_DUMP(osh)	do {} while (0)
#endif /* DHD_MAP_LOGGING */

/* Global ASSERT type */
extern uint32 g_assert_type;

#ifdef CONFIG_PHYS_ADDR_T_64BIT
#define PRI_FMT_x       "llx"
#define PRI_FMT_X       "llX"
#define PRI_FMT_o       "llo"
#define PRI_FMT_d       "lld"
#else
#define PRI_FMT_x       "x"
#define PRI_FMT_X       "X"
#define PRI_FMT_o       "o"
#define PRI_FMT_d       "d"
#endif /* CONFIG_PHYS_ADDR_T_64BIT */
/* ASSERT */
#ifndef ASSERT
#if (defined(BCMDBG_ASSERT) || defined(BCMASSERT_LOG)) && !defined(BINCMP)
	#define ASSERT(exp) \
		do { if (!(exp)) osl_assert(#exp, __FILE__, __LINE__); } while (0)
extern void osl_assert(const char *exp, const char *file, int line);
#else
#ifdef __GNUC__
	#define GCC_VERSION \
		(__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if GCC_VERSION > 30100
	#define ASSERT(exp)	do {} while (0)
#else
	/* ASSERT could cause segmentation fault on GCC3.1, use empty instead */
	#define ASSERT(exp)
#endif /* GCC_VERSION > 30100 */
#endif /* __GNUC__ */
#endif /* (BCMDBG_ASSERT || BCMASSERT_LOG) && !BINCMP */
#endif /* ASSERT */

#define ASSERT_FP(exp) ASSERT(exp)

/* microsecond delay */
#define	OSL_DELAY(usec)		osl_delay(usec)
extern void osl_delay(uint usec);

#define OSL_SLEEP(ms)			osl_sleep(ms)
extern void osl_sleep(uint ms);

/* PCI configuration space access macros */
#define	OSL_PCI_READ_CONFIG(osh, offset, size) \
	osl_pci_read_config((osh), (offset), (size))
#define	OSL_PCI_WRITE_CONFIG(osh, offset, size, val) \
	osl_pci_write_config((osh), (offset), (size), (val))
extern uint32 osl_pci_read_config(osl_t *osh, uint offset, uint size);
extern void osl_pci_write_config(osl_t *osh, uint offset, uint size, uint val);

#ifdef BCMPCIE
/* PCI device bus # and slot # */
#define OSL_PCI_BUS(osh)	osl_pci_bus(osh)
#define OSL_PCI_SLOT(osh)	osl_pci_slot(osh)
#define OSL_PCIE_DOMAIN(osh)	osl_pcie_domain(osh)
#define OSL_PCIE_BUS(osh)	osl_pcie_bus(osh)
extern uint osl_pci_bus(osl_t *osh);
extern uint osl_pci_slot(osl_t *osh);
extern uint osl_pcie_domain(osl_t *osh);
extern uint osl_pcie_bus(osl_t *osh);
extern struct pci_dev *osl_pci_device(osl_t *osh);
#endif

#define OSL_ACP_COHERENCE		(1<<1L)
#define OSL_FWDERBUF			(1<<2L)

/* Pkttag flag should be part of public information */
typedef struct {
	bool pkttag;
	bool mmbus;		/**< Bus supports memory-mapped register accesses */
	pktfree_cb_fn_t tx_fn;  /**< Callback function for PKTFREE */
	void *tx_ctx;		/**< Context to the callback function */
#ifdef OSLREGOPS
	osl_rreg_fn_t rreg_fn;	/**< Read Register function */
	osl_wreg_fn_t wreg_fn;	/**< Write Register function */
	void *reg_ctx;		/**< Context to the reg callback functions */
#else
	void	*unused[3];	/**< temp fix for USBAP cftpool handle currption */
#endif
	void (*rx_fn)(void *rx_ctx, void *p);
	void *rx_ctx;
} osl_pubinfo_t;

extern void osl_flag_set(osl_t *osh, uint32 mask);
extern void osl_flag_clr(osl_t *osh, uint32 mask);
extern bool osl_is_flag_set(osl_t *osh, uint32 mask);

#define PKTFREESETCB(osh, _tx_fn, _tx_ctx)		\
	do {						\
	   ((osl_pubinfo_t*)osh)->tx_fn = _tx_fn;	\
	   ((osl_pubinfo_t*)osh)->tx_ctx = _tx_ctx;	\
	} while (0)

#define PKTFREESETRXCB(osh, _rx_fn, _rx_ctx)		\
	do {						\
	   ((osl_pubinfo_t*)osh)->rx_fn = _rx_fn;	\
	   ((osl_pubinfo_t*)osh)->rx_ctx = _rx_ctx;	\
	} while (0)

#ifdef OSLREGOPS
#define REGOPSSET(osh, rreg, wreg, ctx)			\
	do {						\
	   ((osl_pubinfo_t*)osh)->rreg_fn = rreg;	\
	   ((osl_pubinfo_t*)osh)->wreg_fn = wreg;	\
	   ((osl_pubinfo_t*)osh)->reg_ctx = ctx;	\
	} while (0)
#endif /* OSLREGOPS */

/* host/bus architecture-specific byte swap */
#define BUS_SWAP32(v)		(v)

#if defined(BCMDBG_MEM) && !defined(BINCMP)
	#define MALLOC(osh, size)	osl_debug_malloc((osh), (size), __LINE__, __FILE__)
	#define MALLOCZ(osh, size)	osl_debug_mallocz((osh), (size), __LINE__, __FILE__)
	#define MFREE(osh, addr, size)	\
	({osl_debug_mfree((osh), ((void *)addr), (size), __LINE__, __FILE__);(addr) = NULL;})
	#define VMALLOC(osh, size)	osl_debug_vmalloc((osh), (size), __LINE__, __FILE__)
	#define VMALLOCZ(osh, size)	osl_debug_vmallocz((osh), (size), __LINE__, __FILE__)
	#define VMFREE(osh, addr, size)	\
	({osl_debug_vmfree((osh), ((void *)addr), (size), __LINE__, __FILE__);(addr) = NULL;})
	/* From the linux kernel 4.12, kvmalloc introduced which tries basically to
	 * allocate even large blocks with kmalloc and falls back to vmalloc if fail.
	 */
	#define KVMALLOC(osh, size)	osl_debug_kvmalloc((osh), (size), __LINE__, __FILE__)
	#define KVMALLOCZ(osh, size)	osl_debug_kvmallocz((osh), (size), __LINE__, __FILE__)
	#define KVMFREE(osh, addr, size)	\
	({osl_debug_kvmfree((osh), ((void *)addr), (size), __LINE__, __FILE__);(addr) = NULL;})
	#define MALLOCED(osh)		osl_malloced((osh))
	#define MEMORY_LEFTOVER(osh) osl_check_memleak(osh)
	#define MALLOC_DUMP(osh, b)	osl_debug_memdump((osh), (b))
	extern void *osl_debug_malloc(osl_t *osh, uint size, int line, const char* file);
	extern void *osl_debug_mallocz(osl_t *osh, uint size, int line, const char* file);
	extern void osl_debug_mfree(osl_t *osh, void *addr, uint size, int line, const char* file);
	extern void *osl_debug_vmalloc(osl_t *osh, uint size, int line, const char* file);
	extern void *osl_debug_vmallocz(osl_t *osh, uint size, int line, const char* file);
	extern void osl_debug_vmfree(osl_t *osh, void *addr, uint size, int line, const char* file);
	extern void *osl_debug_kvmalloc(osl_t *osh, uint size, int line, const char* file);
	extern void *osl_debug_kvmallocz(osl_t *osh, uint size, int line, const char* file);
	extern void osl_debug_kvmfree(osl_t *osh, void *addr, uint size, int line,
		const char* file);
	extern uint osl_malloced(osl_t *osh);
	struct bcmstrbuf;
	extern int osl_debug_memdump(osl_t *osh, struct bcmstrbuf *b);
	extern uint osl_check_memleak(osl_t *osh);
#else /* BCMDBG_MEM && !BINCMP */
	#define MALLOC(osh, size)	osl_malloc((osh), (size))
	#define MALLOCZ(osh, size)	osl_mallocz((osh), (size))
	#define MALLOC_RA(osh, size, callsite)	osl_mallocz((osh), (size))
	#define MFREE(osh, addr, size)	\
	({osl_mfree((osh), ((void *)addr), (size));(addr) = NULL;})
	#define VMALLOC(osh, size)	osl_vmalloc((osh), (size))
	#define VMALLOCZ(osh, size)	osl_vmallocz((osh), (size))
	#define VMFREE(osh, addr, size)	\
	({osl_vmfree((osh), ((void *)addr), (size));(addr) = NULL;})
	/* From the linux kernel 4.12, kvmalloc introduced which tries basically to
	 * allocate even large blocks with kmalloc and falls back to vmalloc if fail.
	 */
	#define KVMALLOC(osh, size)	osl_kvmalloc((osh), (size))
	#define KVMALLOCZ(osh, size)	osl_kvmallocz((osh), (size))
	#define KVMFREE(osh, addr, size)	\
	({osl_kvmfree((osh), ((void *)addr), (size));(addr) = NULL;})
	#define MALLOCED(osh)		osl_malloced((osh))
	#define MEMORY_LEFTOVER(osh) osl_check_memleak(osh)
	extern void *osl_vmalloc(osl_t *osh, uint size);
	extern void *osl_vmallocz(osl_t *osh, uint size);
	extern void osl_vmfree(osl_t *osh, void *addr, uint size);
	extern void *osl_kvmalloc(osl_t *osh, uint size);
	extern void *osl_kvmallocz(osl_t *osh, uint size);
	extern void osl_kvmfree(osl_t *osh, void *addr, uint size);
	extern uint osl_malloced(osl_t *osh);
	extern uint osl_check_memleak(osl_t *osh);
#endif /* BCMDBG_MEM && !BINCMP */

#define DMA_MALLOCZ(osh, size, dmable_size, dmah) osl_dma_mallocz((osh), (size), (dmable_size))
#define DMA_MFREE(osh, addr, size, dmah) osl_dma_mfree((osh), (addr), (size))

extern void *osl_malloc(osl_t *osh, uint size);
extern void *osl_mallocz(osl_t *osh, uint size);
extern void *osl_dma_mallocz(osl_t *osh, uint size, uint *dmable_size);
extern void osl_mfree(osl_t *osh, void *addr, uint size);
extern void osl_dma_mfree(osl_t *osh, void *addr, uint size);
#define MALLOC_NODBG(osh, size)		osl_malloc((osh), (size))
#define MALLOCZ_NODBG(osh, size)	osl_mallocz((osh), (size))
#define MFREE_NODBG(osh, addr, size)	({osl_mfree((osh), ((void *)addr), (size));(addr) = NULL;})

extern int memcpy_s(void *dest, size_t destsz, const void *src, size_t n);
extern int memset_s(void *dest, size_t destsz, int c, size_t n);
#define	MALLOC_FAILED(osh)	osl_malloc_failed((osh))
extern uint osl_malloc_failed(osl_t *osh);

/* allocate/free shared (dma-able) consistent memory */
#define	DMA_CONSISTENT_ALIGN	osl_dma_consistent_align()
#define	DMA_ALLOC_CONSISTENT(osh, size, align, tot, pap, dmah) \
	osl_dma_alloc_consistent((osh), (size), (align), (tot), (pap))
#define	DMA_FREE_CONSISTENT(osh, va, size, pa, dmah) \
	osl_dma_free_consistent((osh), (void*)(va), (size), (pa))

#define	DMA_ALLOC_CONSISTENT_FORCE32(osh, size, align, tot, pap, dmah) \
	osl_dma_alloc_consistent((osh), (size), (align), (tot), (pap))
#define	DMA_FREE_CONSISTENT_FORCE32(osh, va, size, pa, dmah) \
	osl_dma_free_consistent((osh), (void*)(va), (size), (pa))

extern uint osl_dma_consistent_align(void);
extern void *osl_dma_alloc_consistent(osl_t *osh, uint size, uint16 align,
	uint *tot, dmaaddr_t *pap);
extern void osl_dma_free_consistent(osl_t *osh, void *va, uint size, dmaaddr_t pa);

/* map/unmap direction, refer enum dma_data_direction */
#include <linux/dma-direction.h>
#define DMA_RXTX	DMA_BIDIRECTIONAL	/* 0, Bidirectional DMA */
#define DMA_TX		DMA_TO_DEVICE		/* 1, TX direction for DMA */
#define DMA_RX		DMA_FROM_DEVICE		/* 2, RX direction for DMA */
#define DMA_NO		DMA_NONE		/* 3, Used to skip cache op */

/* map/unmap shared (dma-able) memory */
#define	DMA_UNMAP(osh, pa, size, direction, p, dmah) \
	osl_dma_unmap((osh), (pa), (size), (direction))
extern void osl_dma_flush(osl_t *osh, void *va, uint size, int direction, void *p,
	hnddma_seg_map_t *txp_dmah);
extern dmaaddr_t osl_dma_map(osl_t *osh, void *va, uint size, int direction, void *p,
	hnddma_seg_map_t *txp_dmah);
extern void osl_dma_unmap(osl_t *osh, dmaaddr_t pa, uint size, int direction);

#ifndef PHYS_TO_VIRT
#define	PHYS_TO_VIRT(pa)	osl_phys_to_virt(pa)
#endif
#ifndef VIRT_TO_PHYS
#define	VIRT_TO_PHYS(va)	osl_virt_to_phys(va)
#endif
extern void * osl_phys_to_virt(void * pa);
extern void * osl_virt_to_phys(void * va);

/* API for DMA addressing capability */
#define OSL_DMADDRWIDTH(osh, addrwidth) ({BCM_REFERENCE(osh); BCM_REFERENCE(addrwidth);})

#define OSL_MB()		mb()
#define OSL_WMB()		wmb()
#define OSL_RMB()		rmb()
#define OSL_SMP_WMB()		smp_wmb()
#define OSL_SMP_RMB()		smp_rmb()

#if defined(__aarch64__)
#define OSL_ISB()	isb()
#else
#define OSL_ISB()	do { /* noop */ } while (0)
#endif /* __aarch64__ */

/* API for CPU relax */
extern void osl_cpu_relax(void);
#define OSL_CPU_RELAX() osl_cpu_relax()

extern void osl_preempt_disable(osl_t *osh);
extern void osl_preempt_enable(osl_t *osh);
#define OSL_DISABLE_PREEMPTION(osh)	osl_preempt_disable(osh)
#define OSL_ENABLE_PREEMPTION(osh)	osl_preempt_enable(osh)

#if (defined(BCMPCIE) && !defined(DHD_USE_COHERENT_MEM_FOR_RING) && defined(__ARM_ARCH_7A__))
	extern void osl_cache_flush(void *va, uint size);
	extern void osl_cache_inv(void *va, uint size);
	extern void osl_prefetch(const void *ptr);
	#define OSL_CACHE_FLUSH(va, len)	osl_cache_flush((void *)(va), len)
	#define OSL_CACHE_INV(va, len)		osl_cache_inv((void *)(va), len)
	#define OSL_PREFETCH(ptr)			osl_prefetch(ptr)
#else  /* !__ARM_ARCH_7A__ */
	#define OSL_CACHE_FLUSH(va, len)	BCM_REFERENCE(va)
	#define OSL_CACHE_INV(va, len)		BCM_REFERENCE(va)
	#define OSL_PREFETCH(ptr)		BCM_REFERENCE(ptr)
#endif /* !__ARM_ARCH_7A__ */

/* register access macros */
#if defined(BCMSDIO)
	#include <bcmsdh.h>
	#define OSL_WRITE_REG(osh, r, v) (bcmsdh_reg_write(osl_get_bus_handle(osh), \
		(uintptr)(r), sizeof(*(r)), (v)))
	#define OSL_READ_REG(osh, r) (bcmsdh_reg_read(osl_get_bus_handle(osh), \
		(uintptr)(r), sizeof(*(r))))

	#define SELECT_BUS_WRITE(osh, mmap_op, bus_op) do {	\
		if (((osl_pubinfo_t*)(osh))->mmbus) {	\
			mmap_op;	\
		} else {	\
			bus_op;	\
		}	\
	} while(0)
	#define SELECT_BUS_READ(osh, mmap_op, bus_op) (((osl_pubinfo_t*)(osh))->mmbus) ? \
		mmap_op : bus_op
#else
	#define SELECT_BUS_WRITE(osh, mmap_op, bus_op) ({BCM_REFERENCE(osh); mmap_op;})
	#define SELECT_BUS_READ(osh, mmap_op, bus_op) ({BCM_REFERENCE(osh); mmap_op;})
#endif /* defined(BCMSDIO) */

#define OSL_ERROR(bcmerror)	osl_error(bcmerror)
extern int osl_error(int bcmerror);

/* the largest reasonable packet buffer driver uses for ethernet MTU in bytes */
#define	PKTBUFSZ	2048   /* largest reasonable packet buffer, driver uses for ethernet MTU */

#define OSH_NULL   NULL

/*
 * BINOSL selects the slightly slower function-call-based binary compatible osl.
 * Macros expand to calls to functions defined in linux_osl.c .
 */
#ifndef BINOSL
#include <linuxver.h>           /* use current 2.4.x calling conventions */
#include <linux/kernel.h>       /* for vsn/printf's */
#include <linux/string.h>       /* for mem*, str* */
extern uint64 osl_sysuptime_us(void);
#define OSL_SYSUPTIME()		((uint32)jiffies_to_msecs(jiffies))
#define OSL_SYSUPTIME_US()	osl_sysuptime_us()
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
extern uint64 osl_sysuptime_ns(void);
#define OSL_SYSUPTIME_NS()     osl_sysuptime_ns()
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0) */
extern uint64 osl_localtime_ns(void);
extern void osl_get_localtime(uint64 *sec, uint64 *usec);
extern uint64 osl_systztime_us(void);
extern char* osl_get_rtctime(void);
#define OSL_LOCALTIME_NS()	osl_localtime_ns()
#define OSL_GET_LOCALTIME(sec, usec)	osl_get_localtime((sec), (usec))
#define OSL_SYSTZTIME_US()	osl_systztime_us()
#define OSL_GET_RTCTIME()	osl_get_rtctime()
/* RTC format %02d:%02d:%02d.%06lu, LEN including the trailing null space */
#define RTC_TIME_BUF_LEN	16u
#define	printf(fmt, args...)	printk(PERCENT_S DHD_LOG_PREFIXS fmt, PRINTF_SYSTEM_TIME, ## args)
#define	vprintf(fmt, ap)	vprintk(fmt, ap)
#include <linux/kernel.h>	/* for vsn/printf's */
#include <linux/string.h>	/* for mem*, str* */
/* bcopy's: Linux kernel doesn't provide these (anymore) */
#define	bcopy_hw(src, dst, len)		memcpy((dst), (src), (len))
#define	bcopy_hw_async(src, dst, len)	memcpy((dst), (src), (len))
#define	bcopy_hw_poll_for_completion()
#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
#define	bcmp(b1, b2, len)	memcmp((b1), (b2), (len))
#define	bzero(b, len)		memset((b), '\0', (len))

/* register access macros */
#if defined(OSLREGOPS)
#define R_REG(osh, r) (\
	sizeof(*(r)) == sizeof(uint8) ? osl_readb((osh), (volatile uint8*)(r)) : \
	sizeof(*(r)) == sizeof(uint16) ? osl_readw((osh), (volatile uint16*)(r)) : \
	sizeof(*(r)) == sizeof(uint32) ? osl_readl((osh), (volatile uint32*)(r)) : \
	osl_readq((osh), (volatile uint64*)(r)) \
)

#define W_REG(osh, r, v) do { \
	switch (sizeof(*(r))) { \
	case sizeof(uint8):	osl_writeb((osh), (volatile uint8*)(r), (uint8)(v)); break; \
	case sizeof(uint16):	osl_writew((osh), (volatile uint16*)(r), (uint16)(v)); break; \
	case sizeof(uint32):	osl_writel((osh), (volatile uint32*)(r), (uint32)(v)); break; \
	case sizeof(uint64):	osl_writeq((osh), (volatile uint64*)(r), (uint64)(v)); break; \
	} \
} while (0)

extern uint8 osl_readb(osl_t *osh, volatile uint8 *r);
extern uint16 osl_readw(osl_t *osh, volatile uint16 *r);
extern uint32 osl_readl(osl_t *osh, volatile uint32 *r);
extern uint32 osl_readq(osl_t *osh, volatile uint64 *r);
extern void osl_writeb(osl_t *osh, volatile uint8 *r, uint8 v);
extern void osl_writew(osl_t *osh, volatile uint16 *r, uint16 v);
extern void osl_writel(osl_t *osh, volatile uint32 *r, uint32 v);
extern void osl_writeq(osl_t *osh, volatile uint64 *r, uint64 v);

#else /* OSLREGOPS */

#ifdef DHD_DEBUG_REG_DUMP
extern uint64 regs_addr;

#define DUMP_R_REG_OFFSET(r) \
{ \
	if ((((uint64)r - regs_addr) >> 16) == 0) \
		printf("**** R_REG : 0x%llx\n", (uint64)r - regs_addr); \
}

#define DUMP_W_REG_OFFSET(r, v) \
{ \
	if ((((uint64)r - regs_addr) >> 16) == 0) \
		printf("$$$$ W_REG : 0x%llx %llu\n", (uint64)(r) - regs_addr, (uint64)(v)); \
}
#else /* !DHD_DEBUG_REG_DUMP */
#define DUMP_R_REG_OFFSET(r)
#define DUMP_W_REG_OFFSET(r, v)
#endif /* DHD_DEBUG_REG_DUMP */

extern void dhd_plat_l1_exit_io(void);

#ifndef IL_BIGENDIAN
#ifdef CONFIG_64BIT
/* readq is defined only for 64 bit platform */
#define NO_WIN_CHECK_R_REG(osh, r, addr) (\
	SELECT_BUS_READ(osh, \
		({ \
			__typeof(*(r)) __osl_v = 0; \
			dhd_plat_l1_exit_io(); \
			BCM_REFERENCE(osh);	\
			switch (sizeof(*(r))) { \
				case sizeof(uint8):	__osl_v = \
					readb((volatile uint8*)(addr)); break; \
				case sizeof(uint16):	__osl_v = \
					readw((volatile uint16*)(addr)); break; \
				case sizeof(uint32):	__osl_v = \
					readl((volatile uint32*)(addr)); break; \
				case sizeof(uint64):	__osl_v = \
					readq((volatile uint64*)(addr)); break; \
			} \
			__osl_v; \
		}), \
		OSL_READ_REG(osh, r)) \
)
#else /* !CONFIG_64BIT */
#define R_REG(osh, r) (\
	SELECT_BUS_READ(osh, \
		({ \
			__typeof(*(r)) __osl_v = 0; \
			switch (sizeof(*(r))) { \
				case sizeof(uint8):	__osl_v = \
					readb((volatile uint8*)(r)); break; \
				case sizeof(uint16):	__osl_v = \
					readw((volatile uint16*)(r)); break; \
				case sizeof(uint32):	__osl_v = \
					readl((volatile uint32*)(r)); break; \
			} \
			__osl_v; \
		}), \
		OSL_READ_REG(osh, r)) \
)
#endif /* CONFIG_64BIT */

#ifdef CONFIG_64BIT
/* writeq is defined only for 64 bit platform */
#define NO_WIN_CHECK_W_REG(osh, r, addr, v) do { \
	SELECT_BUS_WRITE(osh, \
		({ \
			dhd_plat_l1_exit_io(); \
			switch (sizeof(*(r))) { \
				case sizeof(uint8):	writeb((uint8)(v), \
						(volatile uint8*)(addr)); break; \
				case sizeof(uint16):	writew((uint16)(v), \
						(volatile uint16*)(addr)); break; \
				case sizeof(uint32):	writel((uint32)(v), \
						(volatile uint32*)(addr)); break; \
				case sizeof(uint64):	writeq((uint64)(v), \
						(volatile uint64*)(addr)); break; \
			} \
		 }), \
		(OSL_WRITE_REG(osh, r, v))); \
	} while (0)
#else /* !CONFIG_64BIT */
#define W_REG(osh, r, v) do { \
	SELECT_BUS_WRITE(osh, \
		switch (sizeof(*(r))) { \
			case sizeof(uint8):	writeb((uint8)(v), (volatile uint8*)(r)); break; \
			case sizeof(uint16):	writew((uint16)(v), (volatile uint16*)(r)); break; \
			case sizeof(uint32):	writel((uint32)(v), (volatile uint32*)(r)); break; \
		}, \
		(OSL_WRITE_REG(osh, r, v))); \
	} while (0)
#endif /* CONFIG_64BIT */

#else	/* IL_BIGENDIAN */

#ifdef CONFIG_64BIT
/* readq and writeq is defined only for 64 bit platform */
#define R_REG(osh, r) (\
	SELECT_BUS_READ(osh, \
		({ \
			__typeof(*(r)) __osl_v = 0; \
			switch (sizeof(*(r))) { \
				case sizeof(uint8):	__osl_v = \
					readb((volatile uint8*)((uintptr)(r)^3)); break; \
				case sizeof(uint16):	__osl_v = \
					readw((volatile uint16*)((uintptr)(r)^2)); break; \
				case sizeof(uint32):	__osl_v = \
					readl((volatile uint32*)(r)); break; \
				case sizeof(uint64):    __osl_v = \
					readq((volatile uint64*)(r)); break; \
			} \
			__osl_v; \
		}), \
		OSL_READ_REG(osh, r)) \
)
#define W_REG(osh, r, v) do { \
	SELECT_BUS_WRITE(osh, \
		switch (sizeof(*(r))) { \
			case sizeof(uint8):	writeb((uint8)(v), \
					(volatile uint8*)((uintptr)(r)^3)); break; \
			case sizeof(uint16):	writew((uint16)(v), \
					(volatile uint16*)((uintptr)(r)^2)); break; \
			case sizeof(uint32):	writel((uint32)(v), \
					(volatile uint32*)(r)); break; \
			case sizeof(uint64):	writeq((uint64)(v), \
					(volatile uint64*)(r)); break; \
		}, \
		(OSL_WRITE_REG(osh, r, v))); \
	} while (0)

#else /* !CONFIG_64BIT */
#define R_REG(osh, r) (\
	SELECT_BUS_READ(osh, \
		({ \
			__typeof(*(r)) __osl_v = 0; \
			switch (sizeof(*(r))) { \
				case sizeof(uint8):	__osl_v = \
					readb((volatile uint8*)((uintptr)(r)^3)); break; \
				case sizeof(uint16):	__osl_v = \
					readw((volatile uint16*)((uintptr)(r)^2)); break; \
				case sizeof(uint32):	__osl_v = \
					readl((volatile uint32*)(r)); break; \
			} \
			__osl_v; \
		}), \
		OSL_READ_REG(osh, r)) \
)
#define W_REG(osh, r, v) do { \
	SELECT_BUS_WRITE(osh, \
		switch (sizeof(*(r))) { \
			case sizeof(uint8):	writeb((uint8)(v), \
					(volatile uint8*)((uintptr)(r)^3)); break; \
			case sizeof(uint16):	writew((uint16)(v), \
					(volatile uint16*)((uintptr)(r)^2)); break; \
			case sizeof(uint32):	writel((uint32)(v), \
					(volatile uint32*)(r)); break; \
		}, \
		(OSL_WRITE_REG(osh, r, v))); \
	} while (0)
#endif /* CONFIG_64BIT */
#endif /* IL_BIGENDIAN */

#endif /* OSLREGOPS */

/* bcopy, bcmp, and bzero functions */
#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
#define	bcmp(b1, b2, len)	memcmp((b1), (b2), (len))
#define	bzero(b, len)		memset((b), '\0', (len))

/* uncached/cached virtual address */
#define OSL_UNCACHED(va)	((void *)va)
#define OSL_CACHED(va)		((void *)va)

#define OSL_PREF_RANGE_LD(va, sz) BCM_REFERENCE(va)
#define OSL_PREF_RANGE_ST(va, sz) BCM_REFERENCE(va)

/* get processor cycle count */
#if defined(__i386__)
#define	OSL_GETCYCLES(x)	rdtscl((x))
#else
#define OSL_GETCYCLES(x)	((x) = 0)
#endif /* __i386__ */

/* dereference an address that may cause a bus exception */
#define	BUSPROBE(val, addr)	({ (val) = R_REG(NULL, (addr)); 0; })

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
/* 'ioremap_nocache' was deprecated in kernels >= 5.6, so instead we use 'ioremap' which
 * is no-cache by default since kernels 2.6.25.
 */
#define IOREMAP_NO_CACHE(address, size) ioremap(address, size)
#else /* KERNEL_VERSION < 2.6.25 */
#define IOREMAP_NO_CACHE(address, size) ioremap_nocache(address, size)
#endif

/* map/unmap physical to virtual I/O */
#if !defined(CONFIG_MMC_MSM7X00A)
#define	REG_MAP(pa, size)	IOREMAP_NO_CACHE((unsigned long)(pa), (unsigned long)(size))
#else
#define REG_MAP(pa, size)       (void *)(0)
#endif /* !defined(CONFIG_MMC_MSM7X00A */
#define	REG_UNMAP(va)		iounmap((va))

/* shared (dma-able) memory access macros */
#define	R_SM(r)			*(r)
#define	W_SM(r, v)		(*(r) = (v))
#define	OR_SM(r, v)		(*(r) |= (v))
#define	BZERO_SM(r, len)	memset((r), '\0', (len))

/* Because the non BINOSL implemenation of the PKT OSL routines are macros (for
 * performance reasons),  we need the Linux headers.
 */
#include <linuxver.h>		/* use current 2.4.x calling conventions */

#else	/* BINOSL */

/* Where to get the declarations for mem, str, printf, bcopy's? Two basic approaches.
 *
 * First, use the Linux header files and the C standard library replacmenent versions
 * built-in to the kernel.  Use this approach when compiling non hybrid code or compling
 * the OS port files.  The second approach is to use our own defines/prototypes and
 * functions we have provided in the Linux OSL, i.e. linux_osl.c.  Use this approach when
 * compiling the files that make up the hybrid binary.  We are ensuring we
 * don't directly link to the kernel replacement routines from the hybrid binary.
 *
 * NOTE: The issue we are trying to avoid is any questioning of whether the
 * hybrid binary is derived from Linux.  The wireless common code (wlc) is designed
 * to be OS independent through the use of the OSL API and thus the hybrid binary doesn't
 * derive from the Linux kernel at all.  But since we defined our OSL API to include
 * a small collection of standard C library routines and these routines are provided in
 * the kernel we want to avoid even the appearance of deriving at all even though clearly
 * usage of a C standard library API doesn't represent a derivation from Linux.  Lastly
 * note at the time of this checkin 4 references to memcpy/memset could not be eliminated
 * from the binary because they are created internally by GCC as part of things like
 * structure assignment.  I don't think the compiler should be doing this but there is
 * no options to disable it on Intel architectures (there is for MIPS so somebody must
 * agree with me).  I may be able to even remove these references eventually with
 * a GNU binutil such as objcopy via a symbol rename (i.e. memcpy to osl_memcpy).
 */
	#define	printf(fmt, args...)	printk(fmt , ## args)
	#define	vprintf(fmt, ap)	vprintk(fmt, ap)
	#include <linux/kernel.h>	/* for vsn/printf's */
	#include <linux/string.h>	/* for mem*, str* */
	/* bcopy's: Linux kernel doesn't provide these (anymore) */
	#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
	#define	bcmp(b1, b2, len)	memcmp((b1), (b2), (len))
	#define	bzero(b, len)		memset((b), '\0', (len))

	/* These are provided only because when compiling linux_osl.c there
	 * must be an explicit prototype (separate from the definition) because
	 * we are compiling with GCC option -Wstrict-prototypes.  Conversely
	 * these could be placed directly in linux_osl.c.
	 */
	extern int osl_printf(const char *format, ...);
	extern int osl_sprintf(char *buf, const char *format, ...);
	extern int osl_snprintf(char *buf, size_t n, const char *format, ...);
	extern int osl_vsprintf(char *buf, const char *format, va_list ap);
	extern int osl_vsnprintf(char *buf, size_t n, const char *format, va_list ap);
	extern int osl_strcmp(const char *s1, const char *s2);
	extern int osl_strncmp(const char *s1, const char *s2, uint n);
	extern int osl_strlen(const char *s);
	extern char* osl_strcpy(char *d, const char *s);
	extern char* osl_strncpy(char *d, const char *s, uint n);
	extern char* osl_strchr(const char *s, int c);
	extern char* osl_strrchr(const char *s, int c);
	extern void *osl_memset(void *d, int c, size_t n);
	extern void *osl_memcpy(void *d, const void *s, size_t n);
	extern void *osl_memmove(void *d, const void *s, size_t n);
	extern int osl_memcmp(const void *s1, const void *s2, size_t n);

/* register access macros */
#if !defined(BCMSDIO)
#define R_REG(osh, r) \
	({ \
	 BCM_REFERENCE(osh); \
	 sizeof(*(r)) == sizeof(uint8) ? osl_readb((volatile uint8*)(r)) : \
	 sizeof(*(r)) == sizeof(uint16) ? osl_readw((volatile uint16*)(r)) : \
	 sizeof(*(r)) == sizeof(uint32) ? osl_readl((volatile uint32*)(r)) : \
	 osl_readq((volatile uint64*)(r)); \
	 })
#define W_REG(osh, r, v) do { \
	BCM_REFERENCE(osh); \
	switch (sizeof(*(r))) { \
	case sizeof(uint8):	osl_writeb((uint8)(v), (volatile uint8*)(r)); break; \
	case sizeof(uint16):	osl_writew((uint16)(v), (volatile uint16*)(r)); break; \
	case sizeof(uint32):	osl_writel((uint32)(v), (volatile uint32*)(r)); break; \
	case sizeof(uint64):	osl_writeq((uint64)(v), (volatile uint64*)(r)); break; \
	} \
} while (0)

#else
#define R_REG(osh, r) OSL_READ_REG(osh, r)
#define W_REG(osh, r, v) do { OSL_WRITE_REG(osh, r, v); } while (0)
#endif /* !defined(BCMSDIO) */

extern uint8 osl_readb(volatile uint8 *r);
extern uint16 osl_readw(volatile uint16 *r);
extern uint32 osl_readl(volatile uint32 *r);
extern uint64 osl_readq(volatile uint64 *r);
extern void osl_writeb(uint8 v, volatile uint8 *r);
extern void osl_writew(uint16 v, volatile uint16 *r);
extern void osl_writel(uint32 v, volatile uint32 *r);
extern void osl_writeq(uint64 v, volatile uint64 *r);

/* system up time in ms */
#define OSL_SYSUPTIME()		osl_sysuptime()
extern uint32 osl_sysuptime(void);

/* uncached/cached virtual address */
#define OSL_UNCACHED(va)	osl_uncached((va))
extern void *osl_uncached(void *va);
#define OSL_CACHED(va)		osl_cached((va))
extern void *osl_cached(void *va);

#define OSL_PREF_RANGE_LD(va, sz)
#define OSL_PREF_RANGE_ST(va, sz)

/* get processor cycle count */
#define OSL_GETCYCLES(x)	((x) = osl_getcycles())
extern uint osl_getcycles(void);

/* dereference an address that may target abort */
#define	BUSPROBE(val, addr)	osl_busprobe(&(val), (addr))
extern int osl_busprobe(uint32 *val, uint32 addr);

/* map/unmap physical to virtual */
#define	REG_MAP(pa, size)	osl_reg_map((pa), (size))
#define	REG_UNMAP(va)		osl_reg_unmap((va))
extern void *osl_reg_map(uint32 pa, uint size);
extern void osl_reg_unmap(void *va);

/* shared (dma-able) memory access macros */
#define	R_SM(r)			*(r)
#define	W_SM(r, v)		(*(r) = (v))
#define	OR_SM(r, v)		(*(r) |= (v))
#define	BZERO_SM(r, len)	bzero((r), (len))

#endif	/* BINOSL */

#define OSL_RAND()		osl_rand()
extern uint32 osl_rand(void);

#define	DMA_FLUSH(osh, va, size, direction, p, dmah) \
	osl_dma_flush((osh), (va), (size), (direction), (p), (dmah))
#define	DMA_MAP(osh, va, size, direction, p, dmah) \
	osl_dma_map((osh), (va), (size), (direction), (p), (dmah))

#else /* ! BCMDRIVER */

/* ASSERT */
#ifdef BCMDBG_ASSERT
	#include <assert.h>
	#define ASSERT assert
#else /* BCMDBG_ASSERT */
	#define ASSERT(exp)	do {} while (0)
#endif /* BCMDBG_ASSERT */

#define ASSERT_FP(exp) ASSERT(exp)

/* MALLOC and MFREE */
#define MALLOC(o, l) malloc(l)
#define MFREE(o, p, l) free(p)
#include <stdlib.h>

/* str* and mem* functions */
#include <string.h>

/* *printf functions */
#include <stdio.h>

/* bcopy, bcmp, and bzero */
extern void bcopy(const void *src, void *dst, size_t len);
extern int bcmp(const void *b1, const void *b2, size_t len);
extern void bzero(void *b, size_t len);
#endif /* ! BCMDRIVER */

#if defined(NIC_REG_ACCESS_LEGACY) || defined(NIC_REG_ACCESS_LEGACY_DBG)
/* TODO: For now check only for NICBUILD flag. Need to find PCIE + NIC equivalent flag */
#if !defined(NICBUILD)
#error NIC_REG_ACCESS_LEGACY is only compatible with NICBUILD
#endif /* defined(NICBUILD) */

typedef struct osl_pcie_window {
	void *bp_access_lock;
	unsigned long window_offset;
	unsigned long bp_addr;
	volatile void *bar_addr;
} osl_pcie_window_t;

extern osl_pcie_window_t osl_reg_access_pcie_window;

/**
 * Initialize osl_reg_access_pcie_window.
 * @param[in]  osh                      OS handle.
 * @param[in]  bp_access_lock           Lock for restricting backplane access.
 * @param[in]  window_offset            Offset of the window to be managed.
 * @param[in]  bar_addr                 ioremap address of this window.
 */
void osl_reg_access_pcie_window_init(osl_t *osh, void *bp_access_lock, unsigned long window_offset,
	volatile void *bar_addr);

/**
 * Check that the osl_reg_access_pcie_window is configured to access the reg_addr and return the
 * window address. If the window is not properly configured, then it will be reconfigured.
 * @param[in]  osh        OS handle.
 * @param[in]  reg_addr   Address that the window should be able to access after this function call.
 * @return                Address of the pcie bar window that is configured to access the provided
 *                        address.
 */
volatile void *osl_update_pcie_win(osl_t *osh, volatile void *reg_addr);

#define BAR0_WINDOW_OFFSET_MASK		0xFFFu
#define BAR0_WINDOW_ADDRESS_MASK	~BAR0_WINDOW_OFFSET_MASK

#define WIN_CHECK_R_REG(osh, r) ({ \
	unsigned long _lock_flags_r_ = osl_spin_lock(osl_reg_access_pcie_window.bp_access_lock); \
	volatile void *_bar_addr_r_ = osl_update_pcie_win(osh, (volatile void *)r); \
	typeof(*r) _retval_; \
	_retval_ = (typeof(*r))NO_WIN_CHECK_R_REG(osh, r, _bar_addr_r_); \
	osl_spin_unlock(osl_reg_access_pcie_window.bp_access_lock, _lock_flags_r_); \
	_retval_; \
})

#define WIN_CHECK_W_REG(osh, r, v) ({ \
	unsigned long _lock_flags_w_ = osl_spin_lock(osl_reg_access_pcie_window.bp_access_lock); \
	volatile void *_bar_addr_w_ = osl_update_pcie_win(osh, (volatile void *)r); \
	NO_WIN_CHECK_W_REG(osh, r, _bar_addr_w_, v); \
	osl_spin_unlock(osl_reg_access_pcie_window.bp_access_lock, _lock_flags_w_); \
})

#endif /* defined(NIC_REG_ACCESS_LEGACY) || defined(NIC_REG_ACCESS_LEGACY_DBG) */

#ifdef NIC_REG_ACCESS_LEGACY
#define R_REG(osh, r)		WIN_CHECK_R_REG(osh, r)
#define W_REG(osh, r, v)	WIN_CHECK_W_REG(osh, r, v)
#else /* NIC_REG_ACCESS_LEGACY */

#if !defined(IL_BIGENDIAN) && defined(CONFIG_64BIT)

#define R_REG(osh, r)		NO_WIN_CHECK_R_REG(osh, r, r)
#define W_REG(osh, r, v)	NO_WIN_CHECK_W_REG(osh, r, r, v)

#endif /* !defined(IL_BIGENDIAN) && defined(CONFIG_64BIT) */
#endif /* NIC_REG_ACCESS_LEGACY */

#define	AND_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) & (v))
#define	OR_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) | (v))

typedef struct sk_buff_head PKT_LIST;
#define PKTLIST_INIT(x)		skb_queue_head_init((x))
#define PKTLIST_ENQ(x, y)	skb_queue_head((struct sk_buff_head *)(x), (struct sk_buff *)(y))
#define PKTLIST_DEQ(x)		skb_dequeue((struct sk_buff_head *)(x))
#define PKTLIST_UNLINK(x, y)	skb_unlink((struct sk_buff *)(y), (struct sk_buff_head *)(x))
#define PKTLIST_FINI(x)		skb_queue_purge((struct sk_buff_head *)(x))

#ifndef _linuxver_h_
typedef struct timer_list_compat timer_list_compat_t;
#endif /* _linuxver_h_ */
typedef struct osl_timer {
	timer_list_compat_t *timer;
	bool   set;
#ifdef BCMDBG
	char    *name;          /* Desription of the timer */
#endif
} osl_timer_t;

typedef void (*linux_timer_fn)(ulong arg);

extern osl_timer_t * osl_timer_init(osl_t *osh, const char *name, void (*fn)(ulong arg), void *arg);
extern void osl_timer_add(osl_t *osh, osl_timer_t *t, uint32 ms, bool periodic);
extern void osl_timer_update(osl_t *osh, osl_timer_t *t, uint32 ms, bool periodic);
extern bool osl_timer_del(osl_t *osh, osl_timer_t *t);

#ifdef BCMDRIVER
typedef atomic_t osl_atomic_t;
#define OSL_ATOMIC_SET(osh, v, x)	atomic_set(v, x)
#define OSL_ATOMIC_INIT(osh, v)		atomic_set(v, 0)
#define OSL_ATOMIC_INC(osh, v)		atomic_inc(v)
#define OSL_ATOMIC_INC_RETURN(osh, v)	atomic_inc_return(v)
#define OSL_ATOMIC_INC_AND_TEST(osh, v)	atomic_inc_and_test(v)
#define OSL_ATOMIC_DEC(osh, v)		atomic_dec(v)
#define OSL_ATOMIC_DEC_RETURN(osh, v)	atomic_dec_return(v)
#define OSL_ATOMIC_READ(osh, v)		atomic_read(v)
#define OSL_ATOMIC_ADD(osh, v, x)	atomic_add(v, x)

#ifndef atomic_set_mask
#define OSL_ATOMIC_OR(osh, v, x)	atomic_or(x, v)
#define OSL_ATOMIC_AND(osh, v, x)	atomic_and(x, v)
#else
#define OSL_ATOMIC_OR(osh, v, x)	atomic_set_mask(x, v)
#define OSL_ATOMIC_AND(osh, v, x)	atomic_clear_mask(~x, v)
#endif
#endif /* BCMDRIVER */

/* kvmalloc was added from kernel ver 4.12 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
#define KVZALLOC(size, flags)	kzalloc(size, flags)
#define KVFREE(osh, addr)	kfree(addr)
#else
#define KVZALLOC(size, flags)	kvzalloc(size, flags)
#define KVFREE(osh, addr)	kvfree(addr)
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0) */

extern void *osl_spin_lock_init(osl_t *osh);
extern void osl_spin_lock_deinit(osl_t *osh, void *lock);
extern unsigned long osl_spin_lock(void *lock);
extern void osl_spin_unlock(void *lock, unsigned long flags);
extern unsigned long osl_spin_lock_irq(void *lock);
extern void osl_spin_unlock_irq(void *lock, unsigned long flags);
extern unsigned long osl_spin_lock_bh(void *lock);
extern void osl_spin_unlock_bh(void *lock, unsigned long flags);

extern void *osl_mutex_lock_init(osl_t *osh);
extern void osl_mutex_lock_deinit(osl_t *osh, void *lock);
extern unsigned long osl_mutex_lock(void *lock);
void osl_mutex_unlock(void *lock, unsigned long flags);

/* -Wimplicit-fallthrough=5 requires attribute to be used
 * Use the attribute when defined and rely on FALLTHROUGH
 * comment otherwisely.
 */
#ifndef fallthrough
#if defined(__GNUC__)
#if (__GNUC__ >= 7)
#define fallthrough __attribute__ ((__fallthrough__))
#else
#define fallthrough  do {} while (0)  /* FALLTHROUGH */
#endif /* GCC >= 7 */
#else /* !GCC */
#if __has_attribute(__fallthrough__)
#define fallthrough __attribute__ ((__fallthrough__))
#else
#define fallthrough  do {} while (0)  /* FALLTHROUGH */
#endif /* __has_attribute */
#endif /* GCC */
#endif /* fallthrough */

typedef struct osl_timespec {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
	__kernel_old_time_t	tv_sec;			/* seconds */
#else
	__kernel_time_t	tv_sec;			/* seconds */
#endif
	__kernel_suseconds_t	tv_usec;	/* microseconds */
	long		tv_nsec;		/* nanoseconds */
} osl_timespec_t;
extern void osl_do_gettimeofday(struct osl_timespec *ts);
extern void osl_get_monotonic_boottime(struct osl_timespec *ts);
extern uint32 osl_do_gettimediff(struct osl_timespec *cur_ts, struct osl_timespec *old_ts);

#ifndef CUSTOM_PREFIX
#define OSL_PRINT(args)	\
do {			\
	printf args;	\
} while (0)
#else
#define OSL_PRINT_PREFIX "[%s]"CUSTOM_PREFIX, OSL_GET_RTCTIME()
#define OSL_PRINT(args)			\
do {					\
	pr_cont(OSL_PRINT_PREFIX);	\
	pr_cont args;			\
} while (0)
#endif /* CUSTOM_PREFIX */
#endif	/* _linux_osl_h_ */
