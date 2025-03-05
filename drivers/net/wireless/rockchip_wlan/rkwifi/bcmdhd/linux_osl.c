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

#define LINUX_PORT

#include <typedefs.h>
#include <bcmendian.h>
#include <linuxver.h>
#include <bcmdefs.h>

#if defined(__ARM_ARCH_7A__) && !defined(DHD_USE_COHERENT_MEM_FOR_RING)
#include <asm/cacheflush.h>
#endif /* __ARM_ARCH_7A__ && !DHD_USE_COHERENT_MEM_FOR_RING */

#include <linux/random.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
#include <linux/sched/clock.h>
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0) */

#include <osl.h>
#include <bcmutils.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <pcicfg.h>
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 8, 0))
#include <asm-generic/pci-dma-compat.h>
#endif

#include <linux/fs.h>

#ifdef BCM_OBJECT_TRACE
#include <bcmutils.h>
#endif /* BCM_OBJECT_TRACE */
#include "linux_osl_priv.h"

#define PCI_CFG_RETRY		10	/* PR15065: retry count for pci cfg accesses */

#define DUMPBUFSZ 1024

#if defined(CUSTOMER_HW7_DEBUG)
uint32 g_assert_type = 1; /* By Default not cause Kernel Panic */
#else
uint32 g_assert_type = 0; /* By Default Kernel Panic */
#endif

module_param(g_assert_type, int, 0);

#if defined(BCMSLTGT)
/* !!!make sure htclkratio is not 0!!! */
extern uint htclkratio;
#endif

#ifdef USE_DMA_LOCK
static void osl_dma_lock(osl_t *osh);
static void osl_dma_unlock(osl_t *osh);
static void osl_dma_lock_init(osl_t *osh);

#define DMA_LOCK(osh)		osl_dma_lock(osh)
#define DMA_UNLOCK(osh)		osl_dma_unlock(osh)
#define DMA_LOCK_INIT(osh)	osl_dma_lock_init(osh);
#else
#define DMA_LOCK(osh)		do { /* noop */ } while(0)
#define DMA_UNLOCK(osh)		do { /* noop */ } while(0)
#define DMA_LOCK_INIT(osh)	do { /* noop */ } while(0)
#endif /* USE_DMA_LOCK */

uint lmtest = FALSE;

#ifdef DHD_MAP_LOGGING
#define DHD_MAP_LOG_SIZE 2048

typedef struct dhd_map_item {
	dmaaddr_t pa;		/* DMA address (physical) */
	uint64 ts_nsec;		/* timestamp: nsec */
	uint32 size;		/* mapping size */
	uint8 rsvd[4];		/* reserved for future use */
} dhd_map_item_t;

typedef struct dhd_map_record {
	uint32 items;		/* number of total items */
	uint32 idx;		/* current index of metadata */
	dhd_map_item_t map[0];	/* metadata storage */
} dhd_map_log_t;

void
osl_dma_map_dump(osl_t *osh)
{
	dhd_map_log_t *map_log, *unmap_log;
	uint64 ts_sec, ts_usec;

	map_log = (dhd_map_log_t *)(osh->dhd_map_log);
	unmap_log = (dhd_map_log_t *)(osh->dhd_unmap_log);
	osl_get_localtime(&ts_sec, &ts_usec);

	if (map_log && unmap_log) {
		OSL_PRINT(("%s: map_idx=%d unmap_idx=%d "
			"current time=[%5lu.%06lu]\n", __FUNCTION__,
			map_log->idx, unmap_log->idx, (unsigned long)ts_sec,
			(unsigned long)ts_usec));
		OSL_PRINT(("%s: dhd_map_log(pa)=0x%llx size=%d,"
			" dma_unmap_log(pa)=0x%llx size=%d\n", __FUNCTION__,
			(uint64)__virt_to_phys((ulong)(map_log->map)),
			(uint32)(sizeof(dhd_map_item_t) * map_log->items),
			(uint64)__virt_to_phys((ulong)(unmap_log->map)),
			(uint32)(sizeof(dhd_map_item_t) * unmap_log->items)));
	}
}

static void *
osl_dma_map_log_init(uint32 item_len)
{
	dhd_map_log_t *map_log;
	gfp_t flags;
	uint32 alloc_size = (uint32)(sizeof(dhd_map_log_t) +
		(item_len * sizeof(dhd_map_item_t)));

	flags = CAN_SLEEP() ? GFP_KERNEL : GFP_ATOMIC;
	map_log = (dhd_map_log_t *)kmalloc(alloc_size, flags);
	if (map_log) {
		memset(map_log, 0, alloc_size);
		map_log->items = item_len;
		map_log->idx = 0;
	}

	return (void *)map_log;
}

static void
osl_dma_map_log_deinit(osl_t *osh)
{
	if (osh->dhd_map_log) {
		kfree(osh->dhd_map_log);
		osh->dhd_map_log = NULL;
	}

	if (osh->dhd_unmap_log) {
		kfree(osh->dhd_unmap_log);
		osh->dhd_unmap_log = NULL;
	}
}

static void
osl_dma_map_logging(osl_t *osh, void *handle, dmaaddr_t pa, uint32 len)
{
	dhd_map_log_t *log = (dhd_map_log_t *)handle;
	uint32 idx;

	if (log == NULL) {
		OSL_PRINT(("%s: log is NULL\n", __FUNCTION__));
		return;
	}

	idx = log->idx;
	log->map[idx].ts_nsec = osl_localtime_ns();
	log->map[idx].pa = pa;
	log->map[idx].size = len;
	log->idx = (idx + 1) % log->items;
}
#endif /* DHD_MAP_LOGGING */

/* translate bcmerrors into linux errors */
int
osl_error(int bcmerror)
{
	if (bcmerror > 0)
		bcmerror = 0;
	else if (bcmerror < BCME_LAST)
		bcmerror = BCME_ERROR;

	/* Array bounds covered by ASSERT in osl_attach */
	return linux_get_errmap(bcmerror);
}

osl_t *
osl_attach(void *pdev, uint bustype, bool pkttag
#ifdef SHARED_OSL_CMN
	, void **osl_cmn
#endif /* SHARED_OSL_CMN */
)
{
#ifndef SHARED_OSL_CMN
	void **osl_cmn = NULL;
#endif /* SHARED_OSL_CMN */
	osl_t *osh;
	gfp_t flags;

	flags = CAN_SLEEP() ? GFP_KERNEL: GFP_ATOMIC;
	if (!(osh = kmalloc(sizeof(osl_t), flags)))
		return osh;

	ASSERT_NULL(osh);

	bzero(osh, sizeof(osl_t));

	if (osl_cmn == NULL || *osl_cmn == NULL) {
		if (!(osh->cmn = kmalloc(sizeof(osl_cmn_t), flags))) {
			kfree(osh);
			return NULL;
		}
		bzero(osh->cmn, sizeof(osl_cmn_t));
		if (osl_cmn)
			*osl_cmn = osh->cmn;
		atomic_set(&osh->cmn->malloced, 0);
		osh->cmn->dbgmem_list = NULL;
		spin_lock_init(&(osh->cmn->dbgmem_lock));

#ifdef BCMDBG_PKT
		spin_lock_init(&(osh->cmn->pktlist_lock));
#endif
		spin_lock_init(&(osh->cmn->pktalloc_lock));

	} else {
		osh->cmn = *osl_cmn;
	}
	atomic_add(1, &osh->cmn->refcount);

	bcm_object_trace_init();

	osh->failed = 0;
	osh->pdev = pdev;
	osh->pub.pkttag = pkttag;
	osh->bustype = bustype;
	osh->magic = OS_HANDLE_MAGIC;

	switch (bustype) {
		case PCI_BUS:
		case SI_BUS:
			osh->pub.mmbus = TRUE;
			break;
		case SDIO_BUS:
		case USB_BUS:
		case SPI_BUS:
		case RPC_BUS:
			osh->pub.mmbus = FALSE;
			break;
		default:
			ASSERT(FALSE);
			break;
	}

#ifdef BCMDBG_CTRACE
	spin_lock_init(&osh->ctrace_lock);
	INIT_LIST_HEAD(&osh->ctrace_list);
	osh->ctrace_num = 0;
#endif /* BCMDBG_CTRACE */

	DMA_LOCK_INIT(osh);

#ifdef BCMDBG_ASSERT
	if (pkttag) {
		struct sk_buff *skb;
		BCM_REFERENCE(skb);
		ASSERT(OSL_PKTTAG_SZ <= sizeof(skb->cb));
	}
#endif

#ifdef DHD_MAP_LOGGING
	osh->dhd_map_log = osl_dma_map_log_init(DHD_MAP_LOG_SIZE);
	if (osh->dhd_map_log == NULL) {
		OSL_PRINT(("%s: Failed to alloc dhd_map_log\n", __FUNCTION__));
	}

	osh->dhd_unmap_log = osl_dma_map_log_init(DHD_MAP_LOG_SIZE);
	if (osh->dhd_unmap_log == NULL) {
		OSL_PRINT(("%s: Failed to alloc dhd_unmap_log\n", __FUNCTION__));
	}
#endif /* DHD_MAP_LOGGING */

	return osh;
}

void osl_set_bus_handle(osl_t *osh, void *bus_handle)
{
	osh->bus_handle = bus_handle;
}

void* osl_get_bus_handle(osl_t *osh)
{
	return osh->bus_handle;
}

void
osl_detach(osl_t *osh)
{
	if (osh == NULL)
		return;

#ifdef BCMDBG_MEM
	if (MEMORY_LEFTOVER(osh)) {
		static char dumpbuf[DUMPBUFSZ];
		struct bcmstrbuf b;

		OSL_PRINT(("%s: MEMORY LEAK %d bytes\n", __FUNCTION__, MALLOCED(osh)));
		bcm_binit(&b, dumpbuf, DUMPBUFSZ);
		MALLOC_DUMP(osh, &b);
		OSL_PRINT(("%s", b.origbuf));
	}
#endif

	bcm_object_trace_deinit();

#ifdef DHD_MAP_LOGGING
	osl_dma_map_log_deinit(osh);
#endif /* DHD_MAP_LOGGING */

	ASSERT(osh->magic == OS_HANDLE_MAGIC);
	atomic_sub(1, &osh->cmn->refcount);
	if (atomic_read(&osh->cmn->refcount) == 0) {
			kfree(osh->cmn);
	}
	kfree(osh);
}

/* APIs to set/get specific quirks in OSL layer */
void
BCMFASTPATH(osl_flag_set)(osl_t *osh, uint32 mask)
{
	osh->flags |= mask;
}

void
osl_flag_clr(osl_t *osh, uint32 mask)
{
	osh->flags &= ~mask;
}

bool
osl_is_flag_set(osl_t *osh, uint32 mask)
{
	return (osh->flags & mask);
}

#if (defined(BCMPCIE) && defined(__ARM_ARCH_7A__) && !defined(DHD_USE_COHERENT_MEM_FOR_RING))

inline void
BCMFASTPATH(osl_cache_flush)(void *va, uint size)
{
	if (size > 0)
		dma_sync_single_for_device(OSH_NULL, virt_to_phys(va), size,
			DMA_TO_DEVICE);
}

inline void
BCMFASTPATH(osl_cache_inv)(void *va, uint size)
{
	dma_sync_single_for_cpu(OSH_NULL, virt_to_phys(va), size, DMA_FROM_DEVICE);
}

inline void
BCMFASTPATH(osl_prefetch)(const void *ptr)
{
	__asm__ __volatile__("pld\t%0" :: "o"(*(const char *)ptr) : "cc");
}

#endif /* !__ARM_ARCH_7A__ */

uint32
osl_pci_read_config(osl_t *osh, uint offset, uint size)
{
	uint val = 0;
	uint retry = PCI_CFG_RETRY;	 /* PR15065: faulty cardbus controller bug */

	ASSERT_NULL(osh);
	ASSERT(osh->magic == OS_HANDLE_MAGIC);

	/* only 4byte access supported */
	ASSERT(size == 4);

	do {
		pci_read_config_dword(osh->pdev, offset, &val);
		if (val != 0xffffffff)
			break;
	} while (retry--);

#ifdef BCMDBG
	if (retry < PCI_CFG_RETRY)
		OSL_PRINT(("PCI CONFIG READ access to %d required %d retries\n", offset,
		       (PCI_CFG_RETRY - retry)));
#endif /* BCMDBG */

	return (val);
}

void
osl_pci_write_config(osl_t *osh, uint offset, uint size, uint val)
{
	uint retry = PCI_CFG_RETRY;	 /* PR15065: faulty cardbus controller bug */

	ASSERT_NULL(osh);
	ASSERT(osh->magic == OS_HANDLE_MAGIC);

	/* only 4byte access supported */
	ASSERT(size == 4);
#ifdef DHD_DEBUG_REG_DUMP
	OSL_PRINT(("###### W_CFG : 0x%x 0x%x #######\n", offset, val));
#endif /* DHD_DEBUG_REG_DUMP */
	do {
		pci_write_config_dword(osh->pdev, offset, val);
		/* PR15065: PCI_BAR0_WIN is believed to be the only pci cfg write that can occur
		 * when dma activity is possible
		 */
		if (offset != PCI_BAR0_WIN && offset != PCI_BAR1_WIN)
			break;
		if (osl_pci_read_config(osh, offset, size) == val)
			break;
	} while (retry--);

#ifdef BCMDBG
	if (retry < PCI_CFG_RETRY)
		OSL_PRINT(("PCI CONFIG WRITE access to %d required %d retries\n", offset,
		       (PCI_CFG_RETRY - retry)));
#endif /* BCMDBG */
}

#ifdef BCMPCIE
/* return bus # for the pci device pointed by osh->pdev */
uint
osl_pci_bus(osl_t *osh)
{
	ASSERT_NULL(osh);
	ASSERT(osh->magic == OS_HANDLE_MAGIC)
	ASSERT_NULL(osh->pdev);

#if defined(__ARM_ARCH_7A__)
	return pci_domain_nr(((struct pci_dev *)osh->pdev)->bus);
#else
	return ((struct pci_dev *)osh->pdev)->bus->number;
#endif
}

/* return slot # for the pci device pointed by osh->pdev */
uint
osl_pci_slot(osl_t *osh)
{
	ASSERT_NULL(osh);
	ASSERT(osh->magic == OS_HANDLE_MAGIC);
	ASSERT_NULL(osh->pdev);

#if defined(__ARM_ARCH_7A__)
	return PCI_SLOT(((struct pci_dev *)osh->pdev)->devfn) + 1;
#else
	return PCI_SLOT(((struct pci_dev *)osh->pdev)->devfn);
#endif
}

/* return domain # for the pci device pointed by osh->pdev */
uint
osl_pcie_domain(osl_t *osh)
{
	ASSERT_NULL(osh);
	ASSERT(osh->magic == OS_HANDLE_MAGIC);
	ASSERT_NULL(osh->pdev);

	return pci_domain_nr(((struct pci_dev *)osh->pdev)->bus);
}

/* return bus # for the pci device pointed by osh->pdev */
uint
osl_pcie_bus(osl_t *osh)
{
	ASSERT_NULL(osh);
	ASSERT(osh->magic == OS_HANDLE_MAGIC);
	ASSERT_NULL(osh->pdev);

	return ((struct pci_dev *)osh->pdev)->bus->number;
}

/* return the pci device pointed by osh->pdev */
struct pci_dev *
osl_pci_device(osl_t *osh)
{
	ASSERT_NULL(osh);
	ASSERT(osh->magic == OS_HANDLE_MAGIC);
	ASSERT_NULL(osh->pdev);

	return osh->pdev;
}
#endif

void *
osl_malloc(osl_t *osh, uint size)
{
	void *addr;
	gfp_t flags;

	/* only ASSERT if osh is defined */
	if (osh)
		ASSERT(osh->magic == OS_HANDLE_MAGIC);
#ifdef CONFIG_DHD_USE_STATIC_BUF
	if (bcm_static_buf)
	{
		unsigned long irq_flags;
		int i = 0;
		if ((size >= PAGE_SIZE)&&(size <= STATIC_BUF_SIZE))
		{
			OSL_STATIC_BUF_LOCK(&bcm_static_buf->static_lock, irq_flags);

			for (i = 0; i < STATIC_BUF_MAX_NUM; i++)
			{
				if (bcm_static_buf->buf_use[i] == 0)
					break;
			}

			if (i == STATIC_BUF_MAX_NUM)
			{
				OSL_STATIC_BUF_UNLOCK(&bcm_static_buf->static_lock, irq_flags);
				OSL_PRINT(("all static buff in use!\n"));
				goto original;
			}

			bcm_static_buf->buf_use[i] = 1;
			OSL_STATIC_BUF_UNLOCK(&bcm_static_buf->static_lock, irq_flags);

			bzero(bcm_static_buf->buf_ptr+STATIC_BUF_SIZE*i, size);
			if (osh)
				atomic_add(size, &osh->cmn->malloced);

			return ((void *)(bcm_static_buf->buf_ptr+STATIC_BUF_SIZE*i));
		}
	}
original:
#endif /* CONFIG_DHD_USE_STATIC_BUF */

	flags = CAN_SLEEP() ? GFP_KERNEL: GFP_ATOMIC;
	if ((addr = kmalloc(size, flags)) == NULL) {
		if (osh)
			osh->failed++;
		return (NULL);
	}
	if (osh && osh->cmn)
		atomic_add(size, &osh->cmn->malloced);

	return (addr);
}

void *
osl_mallocz(osl_t *osh, uint size)
{
	void *ptr;

	ptr = osl_malloc(osh, size);

	if (ptr != NULL) {
		bzero(ptr, size);
	}

	return ptr;
}

#if defined(L1_CACHE_BYTES)
#define DMA_PAD (L1_CACHE_BYTES)
#else
#define DMA_PAD (128u)
#endif
void *
osl_dma_mallocz(osl_t *osh, uint size, uint *dmable_size)
{
	void *addr = NULL;
	gfp_t flags = 0;
	uint16 align = 0;
	uint32 dmapad = 0;

	if (osh)
		ASSERT(osh->magic == OS_HANDLE_MAGIC);
	if (!size || !dmable_size)
		return NULL;

	*dmable_size = size;
	align = *dmable_size % DMA_PAD;
	dmapad = align ? (DMA_PAD - align) : 0;
	*dmable_size += dmapad;

	flags = CAN_SLEEP() ? GFP_KERNEL: GFP_ATOMIC;
	flags |= GFP_DMA;
	addr = kmalloc(*dmable_size, flags);
	if (addr != NULL) {
		bzero(addr, *dmable_size);
		if (osh && osh->cmn) {
			atomic_add(*dmable_size, &osh->cmn->malloced);
		}
	} else {
		if (osh) {
			osh->failed++;
		}
	}

	return addr;
}

void
osl_mfree(osl_t *osh, void *addr, uint size)
{
#ifdef CONFIG_DHD_USE_STATIC_BUF
	unsigned long flags;

	if (addr == NULL) {
		return;
	}

	if (bcm_static_buf)
	{
		if ((addr > (void *)bcm_static_buf) && ((unsigned char *)addr
			<= ((unsigned char *)bcm_static_buf + STATIC_BUF_TOTAL_LEN)))
		{
			int buf_idx = 0;

			buf_idx = ((unsigned char *)addr - bcm_static_buf->buf_ptr)/STATIC_BUF_SIZE;

			OSL_STATIC_BUF_LOCK(&bcm_static_buf->static_lock, flags);
			bcm_static_buf->buf_use[buf_idx] = 0;
			OSL_STATIC_BUF_UNLOCK(&bcm_static_buf->static_lock, flags);

			if (osh && osh->cmn) {
				ASSERT(osh->magic == OS_HANDLE_MAGIC);
				atomic_sub(size, &osh->cmn->malloced);
			}
			return;
		}
	}
#endif /* CONFIG_DHD_USE_STATIC_BUF */
	if (osh && osh->cmn) {
		ASSERT(osh->magic == OS_HANDLE_MAGIC);

		ASSERT(size <= osl_malloced(osh));

		atomic_sub(size, &osh->cmn->malloced);
	}
	kfree(addr);
}

void
osl_dma_mfree(osl_t *osh, void *addr, uint size)
{
	if (addr == NULL) {
		return;
	}

	if (osh && osh->cmn) {
		ASSERT(osh->magic == OS_HANDLE_MAGIC);
		ASSERT(size <= osl_malloced(osh));
		atomic_sub(size, &osh->cmn->malloced);
	}

	kfree(addr);
	addr = NULL;
}

#ifdef BCMDBG_MEM
/* In BCMDBG_MEM configurations osl_vmalloc is only used internally in
 * the implementation of osl_debug_vmalloc.  Because we are using the GCC
 * -Wstrict-prototypes compile option, we must always have a prototype
 * for a global/external function.  So make osl_vmalloc static in
 * the BCMDBG_MEM case.
 */
static
#endif
void *
osl_vmalloc(osl_t *osh, uint size)
{
	void *addr;

	/* only ASSERT if osh is defined */
	if (osh)
		ASSERT(osh->magic == OS_HANDLE_MAGIC);
	if ((addr = vmalloc(size)) == NULL) {
		if (osh)
			osh->failed++;
		return (NULL);
	}
	if (osh && osh->cmn)
		atomic_add(size, &osh->cmn->malloced);

	return (addr);
}

#ifndef BCMDBG_MEM
void *
osl_vmallocz(osl_t *osh, uint size)
{
	void *ptr;

	ptr = osl_vmalloc(osh, size);

	if (ptr != NULL) {
		bzero(ptr, size);
	}

	return ptr;
}
#endif

#ifdef BCMDBG_MEM
/* In BCMDBG_MEM configurations osl_vmfree is only used internally in
 * the implementation of osl_debug_vmfree.  Because we are using the GCC
 * -Wstrict-prototypes compile option, we must always have a prototype
 * for a global/external function.  So make osl_vmfree static in
 * the BCMDBG_MEM case.
 */
static
#endif
void
osl_vmfree(osl_t *osh, void *addr, uint size)
{
	if (osh && osh->cmn) {
		ASSERT(osh->magic == OS_HANDLE_MAGIC);

		ASSERT(size <= osl_malloced(osh));

		atomic_sub(size, &osh->cmn->malloced);
	}
	vfree(addr);
}

#ifdef BCMDBG_MEM
/* In BCMDBG_MEM configurations osl_kvmalloc is only used internally in
 * the implementation of osl_debug_kvmalloc.  Because we are using the GCC
 * -Wstrict-prototypes compile option, we must always have a prototype
 * for a global/external function.  So make osl_kvmalloc static in
 * the BCMDBG_MEM case.
 */
static
#endif
void *
osl_kvmalloc(osl_t *osh, uint size)
{
	void *addr;
	gfp_t flags;

	/* only ASSERT if osh is defined */
	if (osh)
		ASSERT(osh->magic == OS_HANDLE_MAGIC);

	flags = CAN_SLEEP() ? GFP_KERNEL: GFP_ATOMIC;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
	if ((addr = kmalloc(size, flags)) == NULL) {
#else
	if ((addr = kvmalloc(size, flags)) == NULL) {
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0) */
		if (osh)
			osh->failed++;
		return (NULL);
	}
	if (osh && osh->cmn)
		atomic_add(size, &osh->cmn->malloced);

	return (addr);
}

#ifndef BCMDBG_MEM
void *
osl_kvmallocz(osl_t *osh, uint size)
{
	void *ptr;

	ptr = osl_kvmalloc(osh, size);

	if (ptr != NULL) {
		bzero(ptr, size);
	}

	return ptr;
}
#endif

#ifdef BCMDBG_MEM
/* In BCMDBG_MEM configurations osl_kvmfree is only used internally in
 * the implementation of osl_debug_kvmfree.  Because we are using the GCC
 * -Wstrict-prototypes compile option, we must always have a prototype
 * for a global/external function.  So make osl_kvmfree static in
 * the BCMDBG_MEM case.
 */
static
#endif
void
osl_kvmfree(osl_t *osh, void *addr, uint size)
{
	if (osh && osh->cmn) {
		ASSERT(osh->magic == OS_HANDLE_MAGIC);

		ASSERT(size <= osl_malloced(osh));

		atomic_sub(size, &osh->cmn->malloced);
	}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
	kfree(addr);
#else
	kvfree(addr);
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0) */
}

uint
osl_check_memleak(osl_t *osh)
{
	ASSERT_NULL(osh);
	ASSERT(osh->magic == OS_HANDLE_MAGIC);
	if (atomic_read(&osh->cmn->refcount) == 1)
		return (atomic_read(&osh->cmn->malloced));
	else
		return 0;
}

uint
osl_malloced(osl_t *osh)
{
	ASSERT_NULL(osh);
	ASSERT(osh->magic == OS_HANDLE_MAGIC);
	return (atomic_read(&osh->cmn->malloced));
}

uint
osl_malloc_failed(osl_t *osh)
{
	ASSERT_NULL(osh);
	ASSERT(osh->magic == OS_HANDLE_MAGIC);
	return (osh->failed);
}

#ifdef BCMDBG_MEM
void *
osl_debug_malloc(osl_t *osh, uint size, int line, const char* file)
{
	bcm_mem_link_t *p;
	const char* basename;
	unsigned long flags = 0;
	if (!size) {
		OSL_PRINT(("%s: allocating zero sized mem at %s line %d\n",
			__FUNCTION__, file, line));
		ASSERT(0);
	}

	if ((p = (bcm_mem_link_t*)osl_malloc(osh, sizeof(bcm_mem_link_t) + size)) == NULL) {
		return (NULL);
	}

	if (osh) {
		OSL_MEMLIST_LOCK(&osh->cmn->dbgmem_lock, flags);
	}

	p->size = size;
	p->line = line;
	p->osh = (void *)osh;

	basename = strrchr(file, '/');
	/* skip the '/' */
	if (basename)
		basename++;

	if (!basename)
		basename = file;

	strlcpy(p->file, basename, sizeof(p->file));

	/* link this block */
	if (osh) {
		p->prev = NULL;
		p->next = osh->cmn->dbgmem_list;
		if (p->next)
			p->next->prev = p;
		osh->cmn->dbgmem_list = p;
		OSL_MEMLIST_UNLOCK(&osh->cmn->dbgmem_lock, flags);
	}

	return p + 1;
}

void *
osl_debug_mallocz(osl_t *osh, uint size, int line, const char* file)
{
	void *ptr;

	ptr = osl_debug_malloc(osh, size, line, file);

	if (ptr != NULL) {
		bzero(ptr, size);
	}

	return ptr;
}

void
osl_debug_mfree(osl_t *osh, void *addr, uint size, int line, const char* file)
{
	bcm_mem_link_t *p;
	unsigned long flags = 0;

	ASSERT(osh == NULL || osh->magic == OS_HANDLE_MAGIC);

	if (addr == NULL) {
		return;
	}

	p = (bcm_mem_link_t *)((int8*)addr - sizeof(bcm_mem_link_t));
	if (p->size == 0) {
		OSL_PRINT(("osl_debug_mfree: double free on addr %p size %d at line %d file %s\n",
			addr, size, line, file));
		prhex("bcm_mem_link_t", (void *)p, sizeof(*p));
		ASSERT(p->size);
		return;
	}

	if (p->size != size) {
		OSL_PRINT(("%s: dealloca size does not match alloc size\n", __FUNCTION__));
		OSL_PRINT(("Dealloc addr %p size %d at line %d file %s\n", addr, size, line, file));
		OSL_PRINT(("Alloc size %d line %d file %s\n", p->size, p->line, p->file));
		prhex("bcm_mem_link_t", (void *)p, sizeof(*p));
		ASSERT(p->size == size);
		return;
	}

	if (osh && ((osl_t*)p->osh)->cmn != osh->cmn) {
		OSL_PRINT(("osl_debug_mfree: alloc osh %p does not match dealloc osh %p\n",
			((osl_t*)p->osh)->cmn, osh->cmn));
		OSL_PRINT(("Dealloc addr %p size %d at line %d file %s\n", addr, size, line, file));
		OSL_PRINT(("Alloc size %d line %d file %s\n", p->size, p->line, p->file));
		prhex("bcm_mem_link_t", (void *)p, sizeof(*p));
		ASSERT(((osl_t*)p->osh)->cmn == osh->cmn);
		return;
	}

	/* unlink this block */
	if (osh && osh->cmn) {
		OSL_MEMLIST_LOCK(&osh->cmn->dbgmem_lock, flags);
		if (p->prev)
			p->prev->next = p->next;
		if (p->next)
			p->next->prev = p->prev;
		if (osh->cmn->dbgmem_list == p)
			osh->cmn->dbgmem_list = p->next;
		p->next = p->prev = NULL;
	}
	p->size = 0;

	if (osh && osh->cmn) {
		OSL_MEMLIST_UNLOCK(&osh->cmn->dbgmem_lock, flags);
	}
	osl_mfree(osh, p, size + sizeof(bcm_mem_link_t));
}

void *
osl_debug_vmalloc(osl_t *osh, uint size, int line, const char* file)
{
	bcm_mem_link_t *p;
	const char* basename;
	unsigned long flags = 0;
	if (!size) {
		OSL_PRINT(("%s: allocating zero sized mem at %s line %d\n",
			__FUNCTION__, file, line));
		ASSERT(0);
	}

	if ((p = (bcm_mem_link_t*)osl_vmalloc(osh, sizeof(bcm_mem_link_t) + size)) == NULL) {
		return (NULL);
	}

	if (osh) {
		OSL_MEMLIST_LOCK(&osh->cmn->dbgmem_lock, flags);
	}

	p->size = size;
	p->line = line;
	p->osh = (void *)osh;

	basename = strrchr(file, '/');
	/* skip the '/' */
	if (basename)
		basename++;

	if (!basename)
		basename = file;

	strlcpy(p->file, basename, sizeof(p->file));

	/* link this block */
	if (osh) {
		p->prev = NULL;
		p->next = osh->cmn->dbgvmem_list;
		if (p->next)
			p->next->prev = p;
		osh->cmn->dbgvmem_list = p;
		OSL_MEMLIST_UNLOCK(&osh->cmn->dbgmem_lock, flags);
	}

	return p + 1;
}

void *
osl_debug_vmallocz(osl_t *osh, uint size, int line, const char* file)
{
	void *ptr;

	ptr = osl_debug_vmalloc(osh, size, line, file);

	if (ptr != NULL) {
		bzero(ptr, size);
	}

	return ptr;
}

void
osl_debug_vmfree(osl_t *osh, void *addr, uint size, int line, const char* file)
{
	bcm_mem_link_t *p = (bcm_mem_link_t *)((int8*)addr - sizeof(bcm_mem_link_t));
	unsigned long flags = 0;

	ASSERT(osh == NULL || osh->magic == OS_HANDLE_MAGIC);

	if (p->size == 0) {
		OSL_PRINT(("osl_debug_mfree: double free on addr %p size %d at line %d file %s\n",
			addr, size, line, file));
		ASSERT(p->size);
		return;
	}

	if (p->size != size) {
		OSL_PRINT(("%s: dealloca size does not match alloc size\n", __FUNCTION__));
		OSL_PRINT(("Dealloc addr %p size %d at line %d file %s\n", addr, size, line, file));
		OSL_PRINT(("Alloc size %d line %d file %s\n", p->size, p->line, p->file));
		ASSERT(p->size == size);
		return;
	}

	if (osh && ((osl_t*)p->osh)->cmn != osh->cmn) {
		OSL_PRINT(("osl_debug_mfree: alloc osh %p does not match dealloc osh %p\n",
			((osl_t*)p->osh)->cmn, osh->cmn));
		OSL_PRINT(("Dealloc addr %p size %d at line %d file %s\n", addr, size, line, file));
		OSL_PRINT(("Alloc size %d line %d file %s\n", p->size, p->line, p->file));
		ASSERT(((osl_t*)p->osh)->cmn == osh->cmn);
		return;
	}

	/* unlink this block */
	if (osh && osh->cmn) {
		OSL_MEMLIST_LOCK(&osh->cmn->dbgmem_lock, flags);
		if (p->prev)
			p->prev->next = p->next;
		if (p->next)
			p->next->prev = p->prev;
		if (osh->cmn->dbgvmem_list == p)
			osh->cmn->dbgvmem_list = p->next;
		p->next = p->prev = NULL;
	}
	p->size = 0;

	if (osh && osh->cmn) {
		OSL_MEMLIST_UNLOCK(&osh->cmn->dbgmem_lock, flags);
	}
	osl_vmfree(osh, p, size + sizeof(bcm_mem_link_t));
}

void *
osl_debug_kvmalloc(osl_t *osh, uint size, int line, const char* file)
{
	bcm_mem_link_t *p;
	const char* basename;
	unsigned long flags = 0;
	if (!size) {
		OSL_PRINT(("%s: allocating zero sized mem at %s line %d\n",
			__FUNCTION__, file, line));
		ASSERT(0);
	}

	if ((p = (bcm_mem_link_t*)osl_kvmalloc(osh, sizeof(bcm_mem_link_t) + size)) == NULL) {
		return (NULL);
	}

	if (osh) {
		OSL_MEMLIST_LOCK(&osh->cmn->dbgmem_lock, flags);
	}

	p->size = size;
	p->line = line;
	p->osh = (void *)osh;

	basename = strrchr(file, '/');
	/* skip the '/' */
	if (basename)
		basename++;

	if (!basename)
		basename = file;

	strlcpy(p->file, basename, sizeof(p->file));

	/* link this block */
	if (osh) {
		p->prev = NULL;
		p->next = osh->cmn->dbgvmem_list;
		if (p->next)
			p->next->prev = p;
		osh->cmn->dbgvmem_list = p;
		OSL_MEMLIST_UNLOCK(&osh->cmn->dbgmem_lock, flags);
	}

	return p + 1;
}

void *
osl_debug_kvmallocz(osl_t *osh, uint size, int line, const char* file)
{
	void *ptr;

	ptr = osl_debug_kvmalloc(osh, size, line, file);

	if (ptr != NULL) {
		bzero(ptr, size);
	}

	return ptr;
}

void
osl_debug_kvmfree(osl_t *osh, void *addr, uint size, int line, const char* file)
{
	bcm_mem_link_t *p = (bcm_mem_link_t *)((int8*)addr - sizeof(bcm_mem_link_t));
	unsigned long flags = 0;

	ASSERT(osh == NULL || osh->magic == OS_HANDLE_MAGIC);

	if (p->size == 0) {
		OSL_PRINT(("osl_debug_mfree: double free on addr %p size %d at line %d file %s\n",
			addr, size, line, file));
		ASSERT(p->size);
		return;
	}

	if (p->size != size) {
		OSL_PRINT(("%s: dealloca size does not match alloc size\n", __FUNCTION__));
		OSL_PRINT(("Dealloc addr %p size %d at line %d file %s\n", addr, size, line, file));
		OSL_PRINT(("Alloc size %d line %d file %s\n", p->size, p->line, p->file));
		ASSERT(p->size == size);
		return;
	}

	if (osh && ((osl_t*)p->osh)->cmn != osh->cmn) {
		OSL_PRINT(("osl_debug_mfree: alloc osh %p does not match dealloc osh %p\n",
			((osl_t*)p->osh)->cmn, osh->cmn));
		OSL_PRINT(("Dealloc addr %p size %d at line %d file %s\n", addr, size, line, file));
		OSL_PRINT(("Alloc size %d line %d file %s\n", p->size, p->line, p->file));
		ASSERT(((osl_t*)p->osh)->cmn == osh->cmn);
		return;
	}

	/* unlink this block */
	if (osh && osh->cmn) {
		OSL_MEMLIST_LOCK(&osh->cmn->dbgmem_lock, flags);
		if (p->prev)
			p->prev->next = p->next;
		if (p->next)
			p->next->prev = p->prev;
		if (osh->cmn->dbgvmem_list == p)
			osh->cmn->dbgvmem_list = p->next;
		p->next = p->prev = NULL;
	}
	p->size = 0;

	if (osh && osh->cmn) {
		OSL_MEMLIST_UNLOCK(&osh->cmn->dbgmem_lock, flags);
	}
	osl_kvmfree(osh, p, size + sizeof(bcm_mem_link_t));
}

int
osl_debug_memdump(osl_t *osh, struct bcmstrbuf *b)
{
	bcm_mem_link_t *p;
	unsigned long flags = 0;

	ASSERT_NULL(osh);
	ASSERT(osh->magic == OS_HANDLE_MAGIC);

	OSL_MEMLIST_LOCK(&osh->cmn->dbgmem_lock, flags);

	if (osl_check_memleak(osh) && osh->cmn->dbgmem_list) {
		if (b != NULL)
			bcm_bprintf(b, "   Address   Size File:line\n");
		else
			OSL_PRINT(("   Address   Size File:line\n"));

		for (p = osh->cmn->dbgmem_list; p; p = p->next) {
			if (b != NULL)
				bcm_bprintf(b, "%p %6d %s:%d\n", (char*)p + sizeof(bcm_mem_link_t),
					p->size, p->file, p->line);
			else
				OSL_PRINT(("%p %6d %s:%d\n", (char*)p + sizeof(bcm_mem_link_t),
					p->size, p->file, p->line));

			/* Detects loop-to-self so we don't enter infinite loop */
			if (p == p->next) {
				if (b != NULL)
					bcm_bprintf(b, "WARNING: loop-to-self "
						"p %p p->next %p\n", p, p->next);
				else
					OSL_PRINT(("WARNING: loop-to-self "
						"p %p p->next %p\n", p, p->next));

				break;
			}
		}
	}
	if (osl_check_memleak(osh) && osh->cmn->dbgvmem_list) {
		if (b != NULL)
			bcm_bprintf(b, "Vmem\n   Address   Size File:line\n");
		else
			OSL_PRINT(("Vmem\n   Address   Size File:line\n"));

		for (p = osh->cmn->dbgvmem_list; p; p = p->next) {
			if (b != NULL)
				bcm_bprintf(b, "%p %6d %s:%d\n", (char*)p + sizeof(bcm_mem_link_t),
					p->size, p->file, p->line);
			else
				OSL_PRINT(("%p %6d %s:%d\n", (char*)p + sizeof(bcm_mem_link_t),
					p->size, p->file, p->line));

			/* Detects loop-to-self so we don't enter infinite loop */
			if (p == p->next) {
				if (b != NULL)
					bcm_bprintf(b, "WARNING: loop-to-self "
						"p %p p->next %p\n", p, p->next);
				else
					OSL_PRINT(("WARNING: loop-to-self "
						"p %p p->next %p\n", p, p->next));

				break;
			}
		}
	}

	OSL_MEMLIST_UNLOCK(&osh->cmn->dbgmem_lock, flags);

	return 0;
}

#endif	/* BCMDBG_MEM */

uint
osl_dma_consistent_align(void)
{
	return (PAGE_SIZE);
}

void*
osl_dma_alloc_consistent(osl_t *osh, uint size, uint16 align_bits, uint *alloced, dmaaddr_t *pap)
{
	void *va;
	uint16 align = (1 << align_bits);
	ASSERT_NULL(osh);
	ASSERT(osh->magic == OS_HANDLE_MAGIC);

	if (!ISALIGNED(DMA_CONSISTENT_ALIGN, align))
		size += align;
	*alloced = size;

#if (defined(__ARM_ARCH_7A__) && !defined(DHD_USE_COHERENT_MEM_FOR_RING))
	va = kmalloc(size, GFP_ATOMIC | __GFP_ZERO);
	if (va)
		*pap = (ulong)__virt_to_phys((ulong)va);
#else
	{
		dma_addr_t pap_lin;
		struct pci_dev *hwdev = osh->pdev;
		gfp_t flags;
#ifdef DHD_ALLOC_COHERENT_MEM_FROM_ATOMIC_POOL
		flags = GFP_ATOMIC;
#else
		flags = CAN_SLEEP() ? GFP_KERNEL: GFP_ATOMIC;
#endif /* DHD_ALLOC_COHERENT_MEM_FROM_ATOMIC_POOL */
#ifdef DHD_ALLOC_COHERENT_MEM_WITH_GFP_COMP
		flags |= __GFP_COMP;
#endif /* DHD_ALLOC_COHERENT_MEM_WITH_GFP_COMP */
		va = dma_alloc_coherent(&hwdev->dev, size, &pap_lin, flags);
#ifdef BCMDMA64OSL
		PHYSADDRLOSET(*pap, pap_lin & 0xffffffff);
		PHYSADDRHISET(*pap, (pap_lin >> 32) & 0xffffffff);
#else
		*pap = (dmaaddr_t)pap_lin;
#endif /* BCMDMA64OSL */
	}
#endif /* __ARM_ARCH_7A__ && !DHD_USE_COHERENT_MEM_FOR_RING */

	return va;
}

void
osl_dma_free_consistent(osl_t *osh, void *va, uint size, dmaaddr_t pa)
{
	struct pci_dev *hwdev = NULL;
#ifdef BCMDMA64OSL
	dma_addr_t paddr;
#endif /* BCMDMA64OSL */
	ASSERT_NULL(osh);
	ASSERT(osh->magic == OS_HANDLE_MAGIC);
	UNUSED_PARAMETER(hwdev);

#if (defined(__ARM_ARCH_7A__) && !defined(DHD_USE_COHERENT_MEM_FOR_RING))
	KVFREE(osh, va);
#else /* (defined(__ARM_ARCH_7A__) && !defined(DHD_USE_COHERENT_MEM_FOR_RING)) */
	hwdev = osh->pdev;
#ifdef BCMDMA64OSL
	PHYSADDRTOULONG(pa, paddr);
	dma_free_coherent(&hwdev->dev, size, va, paddr);
#else /* BCMDMA64OSL */
	dma_free_coherent(&hwdev->dev, size, va, (dma_addr_t)pa);
#endif /* BCMDMA64OSL */
#endif /* __ARM_ARCH_7A__ && !DHD_USE_COHERENT_MEM_FOR_RING */
}

void *
osl_virt_to_phys(void *va)
{
	return (void *)(uintptr)virt_to_phys(va);
}

#include <asm/cacheflush.h>
void
BCMFASTPATH(osl_dma_flush)(osl_t *osh, void *va, uint size, int direction, void *p,
	hnddma_seg_map_t *dmah)
{
	return;
}

dmaaddr_t
BCMFASTPATH(osl_dma_map)(osl_t *osh, void *va, uint size, int direction, void *p,
	hnddma_seg_map_t *dmah)
{
	struct pci_dev *hwdev = osh->pdev;
	int dir;
	dmaaddr_t ret_addr;
	dma_addr_t map_addr;
	int ret;

	DMA_LOCK(osh);

	ASSERT_NULL(osh);
	ASSERT(osh->magic == OS_HANDLE_MAGIC);

	/* For Rx buffers, keep direction as bidirectional to handle packet fetch cases */
	dir = (direction == DMA_RX)? DMA_RXTX: direction;

	map_addr = dma_map_single(&hwdev->dev, va, size, dir);

	ret = dma_mapping_error(&hwdev->dev, map_addr);

	if (ret) {
		OSL_PRINT(("%s: Failed to map memory\n", __FUNCTION__));
		PHYSADDRLOSET(ret_addr, 0);
		PHYSADDRHISET(ret_addr, 0);
	} else {
		PHYSADDRLOSET(ret_addr, map_addr & 0xffffffff);
		PHYSADDRHISET(ret_addr, (map_addr >> 32) & 0xffffffff);
	}

#ifdef DHD_MAP_LOGGING
	osl_dma_map_logging(osh, osh->dhd_map_log, ret_addr, size);
#endif /* DHD_MAP_LOGGING */

	DMA_UNLOCK(osh);

	return ret_addr;
}

void
BCMFASTPATH(osl_dma_unmap)(osl_t *osh, dmaaddr_t pa, uint size, int direction)
{
	int dir;
#ifdef BCMDMA64OSL
	dma_addr_t paddr;
#endif /* BCMDMA64OSL */
	struct pci_dev *hwdev = osh->pdev;

	ASSERT_NULL(osh);
	ASSERT(osh->magic == OS_HANDLE_MAGIC);

	DMA_LOCK(osh);

#ifdef DHD_MAP_LOGGING
	osl_dma_map_logging(osh, osh->dhd_unmap_log, pa, size);
#endif /* DHD_MAP_LOGGING */

	/* For Rx buffers, keep direction as bidirectional to handle packet fetch cases */
	dir = (direction == DMA_RX)? DMA_RXTX: direction;

#ifdef BCMDMA64OSL
	PHYSADDRTOULONG(pa, paddr);
	dma_unmap_single(&hwdev->dev, paddr, size, dir);
#else /* BCMDMA64OSL */
	dma_unmap_single(&hwdev->dev, (uint32)pa, size, dir);
#endif /* BCMDMA64OSL */

	DMA_UNLOCK(osh);
}

/* OSL function for CPU relax */
inline void
BCMFASTPATH(osl_cpu_relax)(void)
{
	cpu_relax();
}

extern void osl_preempt_disable(osl_t *osh)
{
	preempt_disable();
}

extern void osl_preempt_enable(osl_t *osh)
{
	preempt_enable();
}

#if defined(BCMDBG_ASSERT) || defined(BCMASSERT_LOG)
void
osl_assert(const char *exp, const char *file, int line)
{
	char tempbuf[256];
	const char *basename;

	basename = strrchr(file, '/');
	/* skip the '/' */
	if (basename)
		basename++;

	if (!basename)
		basename = file;

#ifdef BCMASSERT_LOG
	snprintf(tempbuf, 64, "\"%s\": file \"%s\", line %d\n",
		exp, basename, line);
#endif /* BCMASSERT_LOG */

#ifdef BCMDBG_ASSERT
	snprintf(tempbuf, 256, "assertion \"%s\" failed: file \"%s\", line %d\n",
		exp, basename, line);

	/* Print assert message and give it time to be written to /var/log/messages */
	if (!in_interrupt() && g_assert_type != 1 && g_assert_type != 3) {
		const int delay = 3;
		printk("%s", tempbuf);
		printk("panic in %d seconds\n", delay);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(delay * HZ);
	}
#endif /* BCMDBG_ASSERT */

	switch (g_assert_type) {
	case 0:
		printk("%s", tempbuf);
		BUG();
		break;
	case 1:
		/* fall through */
	case 3:
		printk("%s", tempbuf);
		break;
	case 2:
		printk("%s", tempbuf);
		BUG();
		break;
	default:
		break;
	}
}
#endif /* BCMDBG_ASSERT || BCMASSERT_LOG */

void
osl_delay(uint usec)
{
	uint d;

#ifdef BCMSLTGT
	usec *= htclkratio;
#endif

	while (usec > 0) {
		d = MIN(usec, 1000);
		udelay(d);
		usec -= d;
	}
}

void
osl_sleep(uint ms)
{
#ifdef BCMSLTGT
	ms *= htclkratio;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	if (ms < 20)
		usleep_range(ms*1000, ms*1000 + 1000);
	else
#endif
		msleep(ms);
}

uint64
osl_sysuptime_us(void)
{
	struct osl_timespec ts;
	uint64 usec;

	osl_do_gettimeofday(&ts);
	/* tv_nsec content is fraction of a second */
	usec = (uint64)ts.tv_sec * USEC_PER_SEC + (ts.tv_nsec / NSEC_PER_USEC);
#ifdef BCMSLTGT
	/* scale down the time to match the slow target roughly */
	usec /= htclkratio;
#endif
	return usec;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
uint64
osl_sysuptime_ns(void)
{
	return ktime_get_real_ns();
}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0) */

uint64
osl_localtime_ns(void)
{
	uint64 ts_nsec = 0;

#ifdef BCMDONGLEHOST
	/* Some Linux based platform cannot use local_clock()
	 * since it is defined by EXPORT_SYMBOL_GPL().
	 * GPL-incompatible module (NIC builds wl.ko)
	 * cannnot use the GPL-only symbol.
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	ts_nsec = local_clock();
#else
	ts_nsec = cpu_clock(smp_processor_id());
#endif
#endif /* BCMDONGLEHOST */
	return ts_nsec;
}

void
osl_get_localtime(uint64 *sec, uint64 *usec)
{
	uint64 ts_nsec = 0;
	unsigned long rem_nsec = 0;

#ifdef BCMDONGLEHOST
	/* Some Linux based platform cannot use local_clock()
	 * since it is defined by EXPORT_SYMBOL_GPL().
	 * GPL-incompatible module (NIC builds wl.ko) can
	 * not use the GPL-only symbol.
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	ts_nsec = local_clock();
#else
	ts_nsec = cpu_clock(smp_processor_id());
#endif
	rem_nsec = do_div(ts_nsec, NSEC_PER_SEC);
#endif /* BCMDONGLEHOST */
	*sec = (uint64)ts_nsec;
	*usec = (uint64)(rem_nsec / MSEC_PER_SEC);
}

uint64
osl_systztime_us(void)
{
	struct osl_timespec ts;
	uint64 tzusec;

	osl_do_gettimeofday(&ts);
	/* apply timezone */
	tzusec = (uint64)((ts.tv_sec - (sys_tz.tz_minuteswest * 60L)) * USEC_PER_SEC);
	tzusec += ts.tv_nsec / NSEC_PER_USEC;

	return tzusec;
}

char *
osl_get_rtctime(void)
{
	static char timebuf[RTC_TIME_BUF_LEN];
	struct timespec64 ts;
	struct rtc_time tm;

	memset_s(timebuf, RTC_TIME_BUF_LEN, 0, RTC_TIME_BUF_LEN);
	ktime_get_real_ts64(&ts);
	rtc_time_to_tm(ts.tv_sec - (sys_tz.tz_minuteswest * 60L), &tm);
	scnprintf(timebuf, RTC_TIME_BUF_LEN,
			"%02d:%02d:%02d.%06lu",
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec/NSEC_PER_USEC);
	return timebuf;
}

/*
 * OSLREGOPS specifies the use of osl_XXX routines to be used for register access
 */
#ifdef OSLREGOPS
uint8
osl_readb(osl_t *osh, volatile uint8 *r)
{
	osl_rreg_fn_t rreg	= ((osl_pubinfo_t*)osh)->rreg_fn;
	void *ctx		= ((osl_pubinfo_t*)osh)->reg_ctx;

	return (uint8)((rreg)(ctx, (volatile void*)r, sizeof(uint8)));
}

uint16
osl_readw(osl_t *osh, volatile uint16 *r)
{
	osl_rreg_fn_t rreg	= ((osl_pubinfo_t*)osh)->rreg_fn;
	void *ctx		= ((osl_pubinfo_t*)osh)->reg_ctx;

	return (uint16)((rreg)(ctx, (volatile void*)r, sizeof(uint16)));
}

uint32
osl_readl(osl_t *osh, volatile uint32 *r)
{
	osl_rreg_fn_t rreg	= ((osl_pubinfo_t*)osh)->rreg_fn;
	void *ctx		= ((osl_pubinfo_t*)osh)->reg_ctx;

	return (uint32)((rreg)(ctx, (volatile void*)r, sizeof(uint32)));
}

void
osl_writeb(osl_t *osh, volatile uint8 *r, uint8 v)
{
	osl_wreg_fn_t wreg	= ((osl_pubinfo_t*)osh)->wreg_fn;
	void *ctx		= ((osl_pubinfo_t*)osh)->reg_ctx;

	((wreg)(ctx, (volatile void*)r, v, sizeof(uint8)));
}

void
osl_writew(osl_t *osh, volatile uint16 *r, uint16 v)
{
	osl_wreg_fn_t wreg	= ((osl_pubinfo_t*)osh)->wreg_fn;
	void *ctx		= ((osl_pubinfo_t*)osh)->reg_ctx;

	((wreg)(ctx, (volatile void*)r, v, sizeof(uint16)));
}

void
osl_writel(osl_t *osh, volatile uint32 *r, uint32 v)
{
	osl_wreg_fn_t wreg	= ((osl_pubinfo_t*)osh)->wreg_fn;
	void *ctx		= ((osl_pubinfo_t*)osh)->reg_ctx;

	((wreg)(ctx, (volatile void*)r, v, sizeof(uint32)));
}
#endif /* OSLREGOPS */

/*
 * BINOSL selects the slightly slower function-call-based binary compatible osl.
 */
#ifdef BINOSL

uint32
osl_sysuptime(void)
{
	uint32 msec = ((uint32)jiffies * (1000 / HZ));
#ifdef BCMSLTGT
	/* scale down the time to match the slow target roughly */
	msec /= htclkratio;
#endif
	return msec;
}

int
osl_printf(const char *format, ...)
{
	va_list args;
	static char printbuf[1024];
	int len;

	/* sprintf into a local buffer because there *is* no "vprintk()".. */
	va_start(args, format);
	len = vsnprintf(printbuf, 1024, format, args);
	va_end(args);

	if (len > sizeof(printbuf)) {
		printk("osl_printf: buffer overrun\n");
		return (0);
	}

	return (printk("%s", printbuf));
}

int
osl_sprintf(char *buf, const char *format, ...)
{
	va_list args;
	int rc;

	va_start(args, format);
	rc = vsprintf(buf, format, args);
	va_end(args);
	return (rc);
}

int
osl_snprintf(char *buf, size_t n, const char *format, ...)
{
	va_list args;
	int rc;

	va_start(args, format);
	rc = vsnprintf(buf, n, format, args);
	va_end(args);
	return (rc);
}

int
osl_vsprintf(char *buf, const char *format, va_list ap)
{
	return (vsprintf(buf, format, ap));
}

int
osl_vsnprintf(char *buf, size_t n, const char *format, va_list ap)
{
	return (vsnprintf(buf, n, format, ap));
}

int
osl_strcmp(const char *s1, const char *s2)
{
	return (strcmp(s1, s2));
}

int
osl_strncmp(const char *s1, const char *s2, uint n)
{
	return (strncmp(s1, s2, n));
}

int
osl_strlen(const char *s)
{
	return (strlen(s));
}

char*
osl_strcpy(char *d, const char *s)
{
	return (strcpy(d, s));
}

char*
osl_strncpy(char *d, const char *s, uint n)
{
	return (strlcpy(d, s, n));
}

char*
osl_strchr(const char *s, int c)
{
	return (strchr(s, c));
}

char*
osl_strrchr(const char *s, int c)
{
	return (strrchr(s, c));
}

void*
osl_memset(void *d, int c, size_t n)
{
	return memset(d, c, n);
}

void*
osl_memcpy(void *d, const void *s, size_t n)
{
	return memcpy(d, s, n);
}

void*
osl_memmove(void *d, const void *s, size_t n)
{
	return memmove(d, s, n);
}

int
osl_memcmp(const void *s1, const void *s2, size_t n)
{
	return memcmp(s1, s2, n);
}

uint32
osl_readl(volatile uint32 *r)
{
	return (readl(r));
}

uint16
osl_readw(volatile uint16 *r)
{
	return (readw(r));
}

uint8
osl_readb(volatile uint8 *r)
{
	return (readb(r));
}

void
osl_writel(uint32 v, volatile uint32 *r)
{
	writel(v, r);
}

void
osl_writew(uint16 v, volatile uint16 *r)
{
	writew(v, r);
}

void
osl_writeb(uint8 v, volatile uint8 *r)
{
	writeb(v, r);
}

void *
osl_uncached(void *va)
{
	return ((void*)va);
}

void *
osl_cached(void *va)
{
	return ((void*)va);
}

uint
osl_getcycles(void)
{
	uint cycles;

#if defined(__i386__)
	rdtscl(cycles);
#else
	cycles = 0;
#endif /* __i386__ */
	return cycles;
}

void *
osl_reg_map(uint32 pa, uint size)
{
	return (ioremap_nocache((unsigned long)pa, (unsigned long)size));
}

void
osl_reg_unmap(void *va)
{
	iounmap(va);
}

int
osl_busprobe(uint32 *val, uint32 addr)
{
	*val = readl((uint32 *)(uintptr)addr);

	return 0;
}
#endif	/* BINOSL */

uint32
osl_rand(void)
{
	uint32 rand;

	get_random_bytes(&rand, sizeof(rand));

	return rand;
}

/* Linux Kernel: File Operations: end */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
void
timer_cb_compat(struct timer_list *tl)
{
	timer_list_compat_t *t = container_of(tl, timer_list_compat_t, timer);
	t->callback((ulong)t->arg);
}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0) */

/* timer apis */
/* Note: All timer api's are thread unsafe and should be protected with locks by caller */

osl_timer_t *
osl_timer_init(osl_t *osh, const char *name, void (*fn)(ulong arg), void *arg)
{
	osl_timer_t *t;
	BCM_REFERENCE(fn);
	if ((t = MALLOCZ(NULL, sizeof(osl_timer_t))) == NULL) {
		OSL_PRINT(("osl_timer_init: out of memory, malloced %d bytes\n",
			(int)sizeof(osl_timer_t)));
		return (NULL);
	}
	bzero(t, sizeof(osl_timer_t));
	if ((t->timer = MALLOCZ(NULL, sizeof(timer_list_compat_t))) == NULL) {
		OSL_PRINT(("osl_timer_init: malloc failed\n"));
		MFREE(NULL, t, sizeof(osl_timer_t));
		return (NULL);
	}

	t->set = TRUE;
#ifdef BCMDBG
	if ((t->name = MALLOCZ(NULL, strlen(name) + 1)) != NULL) {
		strcpy(t->name, name);
	}
#endif

	init_timer_compat(t->timer, (linux_timer_fn)fn, arg);

	return (t);
}

void
osl_timer_add(osl_t *osh, osl_timer_t *t, uint32 ms, bool periodic)
{
	if (t == NULL) {
		OSL_PRINT(("%s: Timer handle is NULL\n", __FUNCTION__));
		return;
	}
	ASSERT(!t->set);

	t->set = TRUE;
	if (periodic) {
		OSL_PRINT(("Periodic timers are not supported by Linux timer apis\n"));
	}
#if defined(BCMSLTGT)
	timer_expires(t->timer) = jiffies + ms*HZ/1000*htclkratio;
#else
	timer_expires(t->timer) = jiffies + ms*HZ/1000;
#endif /* defined(BCMSLTGT) */

	add_timer(t->timer);

	return;
}

void
osl_timer_update(osl_t *osh, osl_timer_t *t, uint32 ms, bool periodic)
{
	if (t == NULL) {
		OSL_PRINT(("%s: Timer handle is NULL\n", __FUNCTION__));
		return;
	}
	if (periodic) {
		OSL_PRINT(("Periodic timers are not supported by Linux timer apis\n"));
	}
	t->set = TRUE;
#if defined(BCMSLTGT)
	timer_expires(t->timer) = jiffies + ms*HZ/1000*htclkratio;
#else
	timer_expires(t->timer) = jiffies + ms*HZ/1000;
#endif /* defined(BCMSLTGT) */

	mod_timer(t->timer, timer_expires(t->timer));

	return;
}

/*
 * Return TRUE if timer successfully deleted, FALSE if still pending
 */
bool
osl_timer_del(osl_t *osh, osl_timer_t *t)
{
	if (t == NULL) {
		OSL_PRINT(("%s: Timer handle is NULL\n", __FUNCTION__));
		return (FALSE);
	}
	if (t->set) {
		t->set = FALSE;
		if (t->timer) {
			del_timer(t->timer);
			MFREE(NULL, t->timer, sizeof(timer_list_compat_t));
		}
#ifdef BCMDBG
		if (t->name) {
			MFREE(NULL, t->name, strlen(t->name) + 1);
		}
#endif
		MFREE(NULL, t, sizeof(osl_timer_t));
	}
	return (TRUE);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
int
kernel_read_compat(struct file *file, loff_t offset, char *addr, unsigned long count)
{
#ifdef DHD_SUPPORT_VFS_CALL
	return (int)kernel_read(file, addr, (size_t)count, &offset);
#else
	return 0;
#endif /* DHD_SUPPORT_VFS_CALL */
}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)) */

/* Linux specific multipurpose spinlock API */
void *
osl_spin_lock_init(osl_t *osh)
{
	/* Adding 4 bytes since the sizeof(spinlock_t) could be 0 */
	/* if CONFIG_SMP and CONFIG_DEBUG_SPINLOCK are not defined */
	/* and this results in kernel asserts in internal builds */
	spinlock_t * lock = MALLOC(osh, sizeof(spinlock_t) + 4);
	if (lock)
		spin_lock_init(lock);
	return ((void *)lock);
}

void
osl_spin_lock_deinit(osl_t *osh, void *lock)
{
	if (lock)
		MFREE(osh, lock, sizeof(spinlock_t) + 4);
}

unsigned long
osl_spin_lock(void *lock)
{
	unsigned long flags = 0;

	if (lock) {
#ifdef DHD_USE_SPIN_LOCK_BH
		/* Calling spin_lock_bh with both irq and non-irq context will lead to deadlock */
		ASSERT(!in_irq());
		spin_lock_bh((spinlock_t *)lock);
#else
		spin_lock_irqsave((spinlock_t *)lock, flags);
#endif /* DHD_USE_SPIN_LOCK_BH */
	}

	return flags;
}

void
osl_spin_unlock(void *lock, unsigned long flags)
{
	if (lock) {
#ifdef DHD_USE_SPIN_LOCK_BH
		/* Calling spin_lock_bh with both irq and non-irq context will lead to deadlock */
		ASSERT(!in_irq());
		spin_unlock_bh((spinlock_t *)lock);
#else
		spin_unlock_irqrestore((spinlock_t *)lock, flags);
#endif /* DHD_USE_SPIN_LOCK_BH */
	}
}

unsigned long
osl_spin_lock_irq(void *lock)
{
	unsigned long flags = 0;

	if (lock)
		spin_lock_irqsave((spinlock_t *)lock, flags);

	return flags;
}

void
osl_spin_unlock_irq(void *lock, unsigned long flags)
{
	if (lock)
		spin_unlock_irqrestore((spinlock_t *)lock, flags);
}

unsigned long
osl_spin_lock_bh(void *lock)
{
	unsigned long flags = 0;

	if (lock) {
		/* Calling spin_lock_bh with both irq and non-irq context will lead to deadlock */
		ASSERT(!in_irq());
		spin_lock_bh((spinlock_t *)lock);
	}

	return flags;
}

void
osl_spin_unlock_bh(void *lock, unsigned long flags)
{
	if (lock) {
		/* Calling spin_lock_bh with both irq and non-irq context will lead to deadlock */
		ASSERT(!in_irq());
		spin_unlock_bh((spinlock_t *)lock);
	}
}

void *
osl_mutex_lock_init(osl_t *osh)
{
	struct mutex *mtx = NULL;

	mtx = MALLOCZ(osh, sizeof(*mtx));
	if (mtx)
		mutex_init(mtx);

	return mtx;
}

void
osl_mutex_lock_deinit(osl_t *osh, void *mtx)
{
	if (mtx) {
		mutex_destroy(mtx);
		MFREE(osh, mtx, sizeof(struct mutex));
	}
}

/* For mutex lock/unlock unsigned long flags is used,
 * this is to keep in sync with spin lock apis, so that
 * locks can be easily interchanged based on contexts
 */
unsigned long
osl_mutex_lock(void *lock)
{
	if (lock)
		mutex_lock((struct mutex *)lock);

	return 0;
}

void
osl_mutex_unlock(void *lock, unsigned long flags)
{
	if (lock)
		mutex_unlock((struct mutex *)lock);
	return;
}

#ifdef USE_DMA_LOCK
static void
osl_dma_lock(osl_t *osh)
{
	/* The conditional check is to avoid the scheduling bug.
	 * If the spin_lock_bh is used under the spin_lock_irqsave,
	 * Kernel triggered the warning message as the spin_lock_irqsave
	 * disables the interrupt and the spin_lock_bh doesn't use in case
	 * interrupt is disabled.
	 * Please refer to the __local_bh_enable_ip() function
	 * in kernel/softirq.c to understand the condtion.
	 */
	if (likely(in_irq() || irqs_disabled())) {
		spin_lock(&osh->dma_lock);
	} else {
		spin_lock_bh(&osh->dma_lock);
		osh->dma_lock_bh = TRUE;
	}
}

static void
osl_dma_unlock(osl_t *osh)
{
	if (unlikely(osh->dma_lock_bh)) {
		osh->dma_lock_bh = FALSE;
		spin_unlock_bh(&osh->dma_lock);
	} else {
		spin_unlock(&osh->dma_lock);
	}
}

static void
osl_dma_lock_init(osl_t *osh)
{
	spin_lock_init(&osh->dma_lock);
	osh->dma_lock_bh = FALSE;
}
#endif /* USE_DMA_LOCK */

#if defined(NIC_REG_ACCESS_LEGACY) || defined(NIC_REG_ACCESS_LEGACY_DBG)
osl_pcie_window_t osl_reg_access_pcie_window;
/**
 * Initialize osl_reg_access_pcie_window.
 * @param[in]  osh                      OS handle.
 * @param[in]  bp_access_lock           Lock for restricting backplane access.
 * @param[in]  window_offset            Config space window base register offset.
 * @param[in]  bar_addr                 ioremap address of this window.
 */
void
osl_reg_access_pcie_window_init(osl_t *osh, void *bp_access_lock, unsigned long window_offset,
		volatile void *bar_addr)
{
	osl_reg_access_pcie_window.bp_access_lock = bp_access_lock;
	osl_reg_access_pcie_window.window_offset = window_offset;
	osl_reg_access_pcie_window.bar_addr = bar_addr;
	osl_reg_access_pcie_window.bp_addr = OSL_PCI_READ_CONFIG(osh,
		osl_reg_access_pcie_window.window_offset, sizeof(uint32));
}

/**
 * Check that the osl_reg_access_pcie_window is configured to access the reg_addr and return the
 * window address. If the window is not properly configured, then it will be reconfigured.
 * @param[in]  osh        OS handle.
 * @param[in]  reg_addr   Address that the window should be able to access after this function call.
 * @return                Address of the pcie bar window that is configured to access the provided
 *                        address.
 */
volatile void *
osl_update_pcie_win(osl_t *osh, volatile void *reg_addr)
{
	unsigned long r_addr = (unsigned long)reg_addr;
	unsigned long base_addr = (r_addr & BAR0_WINDOW_ADDRESS_MASK);
	if (base_addr != osl_reg_access_pcie_window.bp_addr) {
		osl_reg_access_pcie_window.bp_addr = base_addr;
		OSL_PCI_WRITE_CONFIG(osh, osl_reg_access_pcie_window.window_offset, sizeof(uint32),
			osl_reg_access_pcie_window.bp_addr);
	}
	return (volatile void *)(((volatile uint8 *)osl_reg_access_pcie_window.bar_addr) +
		(r_addr & BAR0_WINDOW_OFFSET_MASK));
}
#endif /* defined(NIC_REG_ACCESS_LEGACY) || defined(NIC_REG_ACCESS_LEGACY_DBG) */

void
osl_do_gettimeofday(struct osl_timespec *ts)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
	struct timespec64 curtime;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
	struct timespec curtime;
#else
	struct timeval curtime;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
	ktime_get_real_ts64(&curtime);
	ts->tv_nsec = curtime.tv_nsec;
	ts->tv_usec	= curtime.tv_nsec / 1000;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
	getnstimeofday(&curtime);
	ts->tv_nsec = curtime.tv_nsec;
	ts->tv_usec = curtime.tv_nsec / 1000;
#else
	do_gettimeofday(&curtime);
	ts->tv_usec = curtime.tv_usec;
	ts->tv_nsec = curtime.tv_usec * 1000;
#endif
	ts->tv_sec = curtime.tv_sec;
}

uint32
osl_do_gettimediff(struct osl_timespec *cur_ts, struct osl_timespec *old_ts)
{
	uint32 diff_s, diff_us, total_diff_us;
	bool pgc_g = FALSE;

	diff_s = (uint32)cur_ts->tv_sec - (uint32)old_ts->tv_sec;
	pgc_g = (cur_ts->tv_usec > old_ts->tv_usec) ? TRUE : FALSE;
	diff_us = pgc_g ? (cur_ts->tv_usec - old_ts->tv_usec) : (old_ts->tv_usec - cur_ts->tv_usec);
	total_diff_us = pgc_g ? (diff_s * 1000000 + diff_us) : (diff_s * 1000000 - diff_us);
	return total_diff_us;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)
void
osl_get_monotonic_boottime(struct osl_timespec *ts)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
	struct timespec64 curtime;
#else
	struct timespec curtime;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
	curtime = ktime_to_timespec64(ktime_get_boottime());
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0)
	curtime = ktime_to_timespec(ktime_get_boottime());
#else
	get_monotonic_boottime(&curtime);
#endif
	ts->tv_sec = curtime.tv_sec;
	ts->tv_nsec = curtime.tv_nsec;
	ts->tv_usec = curtime.tv_nsec / 1000;
}
#endif
