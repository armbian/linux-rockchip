/******************************************************************************
 *
 * Copyright(c) 2019 - 2020 Realtek Corporation.
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
#ifndef _PLTFM_OPS_LINUX_H_
#define _PLTFM_OPS_LINUX_H_
#include "drv_types.h"

#ifdef CONFIG_PLATFORM_AML_S905
extern struct device * g_pcie_reserved_mem_dev;
#endif

static inline char *_os_strpbrk(const char *s, const char *ct)
{
	return strpbrk(s, ct);
}

static inline char *_os_strsep(char **s, const char *ct)
{
	return strsep(s, ct);
}

#if 1
#define _os_sscanf(buf, fmt, ...) sscanf(buf, fmt, ##__VA_ARGS__)
#else
static inline int _os_sscanf(const char *buf, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsscanf(buf, fmt, args);
	va_end(args);
	return i;
}
#endif
static inline int _os_strcmp(const char *s1, const char *s2)
{
	return strcmp(s1, s2);
}
static inline int _os_strncmp(const char *s1, const char *s2, size_t n)
{
	return strncmp(s1, s2, n);
}
static inline char *_os_strcpy(char *dest, const char *src)
{
	return strcpy(dest, src);
}
static inline char *_os_strncpy(char *dest, const char *src, size_t n)
{
	return strncpy(dest, src, n);
}
#if 1
#define _os_strchr(s, c) strchr(s, c)
#else
static inline char*_os_strchr(const char *s, int c)
{
	return strchr(s, c);
}
#endif

#if 1
#define _os_snprintf(s, sz, fmt, ...) snprintf(s, sz, fmt, ##__VA_ARGS__)
#define _os_vsnprintf(str, size, fmt, args) vsnprintf(str, size, fmt, args)
#else
static int _os_snprintf(char *str, size_t size, const char *fmt, ...)
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vsnprintf(str, size, fmt, args);
	va_end(args);
	if (size > 0)
		str[size - 1] = '\0';
	return ret;
}
#endif

static inline u32 _os_strlen(u8 *buf)
{
	return strlen(buf);
}
static inline void _os_delay_ms(void *d, u32 ms)
{
	rtw_mdelay_os(ms);
}
static inline void _os_delay_us(void *d, u32 us)
{
	rtw_udelay_os(us);
}
static inline void _os_sleep_ms(void *d, u32 ms)
{
	rtw_msleep_os(ms);
}
static inline void _os_sleep_us(void *d, u32 us)
{
	rtw_usleep_os(us);
}
static inline u32 _os_get_cur_time_us(void)
{
	return rtw_systime_to_us(rtw_get_current_time());
}
static inline u32 _os_get_cur_time_ms(void)
{
	return rtw_systime_to_ms(rtw_get_current_time());
}

static inline u64 _os_modular64(u64 x, u64 y)
{
	/*return do_div(x, y);*/
	return rtw_modular64(x, y);
}
static inline u64 _os_division64(u64 x, u64 y)
{
	/*return do_div(x, y);*/
	return rtw_division64(x, y);
}
static inline u32 _os_div_round_up(u32 x, u32 y)
{
	return RTW_DIV_ROUND_UP(x, y);
}

#ifdef CONFIG_PCI_HCI
static inline void _os_cache_inv(void *d, _dma *bus_addr_l,
			_dma *bus_addr_h, u32 buf_sz, u8 direction)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
	PPCI_DATA pci_data = dvobj_to_pci(pobj);
	struct pci_dev *pdev = pci_data->ppcidev;

	pci_cache_inv(pdev, bus_addr_l, buf_sz, direction);
}

static inline void _os_cache_wback(void *d, _dma *bus_addr_l,
			_dma *bus_addr_h, u32 buf_sz, u8 direction)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
	PPCI_DATA pci_data = dvobj_to_pci(pobj);
	struct pci_dev *pdev = pci_data->ppcidev;

	pci_cache_wback(pdev, bus_addr_l, buf_sz, direction);
}

static inline void *_os_dma_pool_create(void *d, char *name, u32 wd_page_sz)
{
	struct dvobj_priv *dvobj = (struct dvobj_priv *)d;

	return pci_create_dma_pool(dvobj->pci_data.ppcidev, name, wd_page_sz);
}

static inline void _os_dma_pool_destory(void *d, void *pool)
{
	struct dvobj_priv *dvobj = (struct dvobj_priv *)d;

	pci_destory_dma_pool(dvobj->pci_data.ppcidev, (struct dma_pool *)pool);
}

/* txbd, rxbd, wd */
static inline void *_os_shmem_alloc(void *d, void *pool, _dma *bus_addr_l,
				    _dma *bus_addr_h, u32 buf_sz,
				    u8 cache, u8 direction, void **os_rsvd)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
	PPCI_DATA pci_data = dvobj_to_pci(pobj);
	struct pci_dev *pdev = pci_data->ppcidev;

	if (cache == DMA_ADDR)
		return pci_alloc_noncache_mem(pdev, bus_addr_l, buf_sz);
	else if (cache == POOL_ADDR) {
		return pci_zalloc_pool_mem(pdev, (struct dma_pool *)pool, bus_addr_l);
	} else
		return pci_alloc_cache_mem(pdev, bus_addr_l, buf_sz, direction);

	return NULL;
}

static inline void _os_shmem_free(void *d, void *pool, u8 *vir_addr, _dma *bus_addr_l,
				  _dma *bus_addr_h, u32 buf_sz,
				  u8 cache, u8 direction, void *os_rsvd)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
	PPCI_DATA pci_data = dvobj_to_pci(pobj);
	struct pci_dev *pdev = pci_data->ppcidev;

	if (cache == DMA_ADDR)
		return pci_free_noncache_mem(pdev, vir_addr, bus_addr_l, buf_sz);
	else if (cache == POOL_ADDR)
		return pci_free_pool_mem(pdev, (struct dma_pool *)pool, vir_addr, bus_addr_l);
	else
		return pci_free_cache_mem(pdev, vir_addr, bus_addr_l, buf_sz, direction);
}
#endif /*CONFIG_PCI_HCI*/

static inline void *_os_pkt_buf_unmap_rx(void *d, _dma bus_addr_l, _dma bus_addr_h, u32 buf_sz)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
#ifdef CONFIG_PCI_HCI
	PPCI_DATA pci_data = dvobj_to_pci(pobj);
	struct pci_dev *pdev = pci_data->ppcidev;
#endif /*CONFIG_PCI_HCI*/

#ifdef CONFIG_PCI_HCI
#ifdef CONFIG_PLATFORM_AML_S905
	if (g_pcie_reserved_mem_dev)
		pdev->dev.dma_mask = NULL;
#endif
	pci_unmap_single(pdev, bus_addr_l, buf_sz, PCI_DMA_FROMDEVICE);
#endif

#ifdef RTW_CORE_RECORD
	phl_add_record(d, REC_RX_UNMAP, bus_addr_l, buf_sz);
#endif
	return NULL;
}

static inline void *_os_pkt_buf_map_rx(void *d, _dma *bus_addr_l, _dma *bus_addr_h,
					u32 buf_sz, void *os_priv)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
#ifdef CONFIG_PCI_HCI
	PPCI_DATA pci_data = dvobj_to_pci(pobj);
	struct pci_dev *pdev = pci_data->ppcidev;
	struct sk_buff *skb = os_priv;

#ifdef CONFIG_PLATFORM_AML_S905
	if (g_pcie_reserved_mem_dev)
		pdev->dev.dma_mask = NULL;
#endif
	*bus_addr_l = pci_map_single(pdev, skb->data, buf_sz, PCI_DMA_FROMDEVICE);
	/* *bus_addr_h = NULL;*/
#endif /*CONFIG_PCI_HCI*/

	return NULL;
}

#ifdef CONFIG_PCI_HCI
static inline struct sk_buff *_os_alloc_noncashe_skb(struct pci_dev *pdev, u32 buf_sz)
{
	struct sk_buff *skb = NULL;
	unsigned char *data = NULL;

	skb = rtw_zmalloc(sizeof(struct sk_buff));
	if (!skb)
		goto out;

	data = pci_alloc_noncache_mem(pdev, (dma_addr_t *)&skb->cb, buf_sz);

	if (!data)
		goto nodata;

	skb->head = data;
	skb->data = data;
	skb_reset_tail_pointer(skb);
	skb->end = skb->tail + buf_sz;
	skb->len = buf_sz;
out:
	return skb;
nodata:
	_rtw_mfree(skb, sizeof(struct sk_buff));
	skb = NULL;
	goto out;
}

static inline void _os_free_noncashe_skb(struct pci_dev *pdev,
	struct sk_buff *skb, u32 buf_sz)
{
	/* skb buffer */
	pci_free_noncache_mem(pdev, skb->data, (dma_addr_t *)skb->cb, buf_sz);

	/* skb */
	rtw_mfree(skb, sizeof(struct sk_buff));
}
#endif /*CONFIG_PCI_HCI*/

/* rxbuf */
#define PHL_RX_HEADROOM 0
static inline void *_os_pkt_buf_alloc_rx(void *d, _dma *bus_addr_l,
			_dma *bus_addr_h, u32 buf_sz, u8 cache, void **os_priv)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
#ifdef CONFIG_PCI_HCI
	PPCI_DATA pci_data = dvobj_to_pci(pobj);
	struct pci_dev *pdev = pci_data->ppcidev;
#endif /*CONFIG_PCI_HCI*/
	struct sk_buff *skb = NULL;
	u32 rxbuf_size = buf_sz + PHL_RX_HEADROOM;

	if (cache)
		skb = rtw_skb_alloc(rxbuf_size);
#ifdef CONFIG_PCI_HCI
	else
		skb = _os_alloc_noncashe_skb(pdev, rxbuf_size);
#endif
	if (!skb)
		return NULL;

	//skb_pull(skb, PHL_RX_HEADROOM);
#ifdef CONFIG_PCI_HCI
#ifdef CONFIG_PLATFORM_AML_S905
	if (g_pcie_reserved_mem_dev)
		pdev->dev.dma_mask = NULL;
#endif
	if (cache)
		*bus_addr_l = pci_map_single(pdev, skb->data,
			rxbuf_size, PCI_DMA_FROMDEVICE);
	else
		*bus_addr_l = *(dma_addr_t *)skb->cb;
	/* *bus_addr_h = NULL;*/
#endif /*CONFIG_PCI_HCI*/
	*os_priv = skb;

	return skb->data;
}

static inline void _os_pkt_buf_free_rx(void *d, u8 *vir_addr, _dma bus_addr_l,
				_dma bus_addr_h, u32 buf_sz, u8 cache, void *os_priv)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
#ifdef CONFIG_PCI_HCI
	PPCI_DATA pci_data = dvobj_to_pci(pobj);
	struct pci_dev *pdev = pci_data->ppcidev;
#endif /*CONFIG_PCI_HCI*/
	struct sk_buff *skb = (struct sk_buff *)os_priv;

#ifdef CONFIG_PCI_HCI
#ifdef CONFIG_PLATFORM_AML_S905
	if (g_pcie_reserved_mem_dev)
		pdev->dev.dma_mask = NULL;
#endif
	if (cache)
		pci_unmap_single(pdev, bus_addr_l, buf_sz, PCI_DMA_FROMDEVICE);

	if (!cache)
		_os_free_noncashe_skb(pdev, skb, buf_sz);
	else
#endif /*CONFIG_PCI_HCI*/
		rtw_skb_free(skb);
}

/* phl pre-alloc network layer buffer */
static inline void * _os_alloc_netbuf(void *d, u32 buf_sz, void **os_priv)
{
	return _os_pkt_buf_alloc_rx(d, NULL, NULL, buf_sz, true, os_priv);
}

/* Free netbuf for error case. (ex. drop rx-reorder packet) */
static inline void _os_free_netbuf(void *d, u8 *vir_addr, u32 buf_sz, void *os_priv)
{
	_os_pkt_buf_free_rx(d, vir_addr, 0,0, buf_sz, true, os_priv);
}


/*virtually contiguous memory*/
static inline void *_os_mem_alloc(void *d, u32 buf_sz)
{
	#ifdef DBG_PHL_MEM_ALLOC
	struct dvobj_priv *obj = (struct dvobj_priv *)d;

	ATOMIC_ADD_RETURN(&obj->phl_mem, buf_sz);
	#endif

	#ifdef CONFIG_PHL_USE_KMEM_ALLOC
	return rtw_zmalloc(buf_sz);
	#else
	if (in_atomic()) {
		RTW_ERR("Call rtw_zvmalloc in atomic @%s:%u\n",
			__FUNCTION__, __LINE__);
		dump_stack();
	}
	return rtw_zvmalloc(buf_sz);
	#endif
}

/*virtually contiguous memory*/
static inline void _os_mem_free(void *d, void *buf, u32 buf_sz)
{
	#ifdef DBG_PHL_MEM_ALLOC
	struct dvobj_priv *obj = (struct dvobj_priv *)d;

	ATOMIC_SUB(&obj->phl_mem, buf_sz);
	#endif

	#ifdef CONFIG_PHL_USE_KMEM_ALLOC
	rtw_mfree(buf, buf_sz);
	#else
	if (in_atomic()) {
		RTW_ERR("Call rtw_vmfree in atomic @%s:%u\n",
			__FUNCTION__, __LINE__);
		dump_stack();
	}
	rtw_vmfree(buf, buf_sz);
	#endif
}

/*physically contiguous memory if the buffer will be accessed by a DMA device*/
static inline void *_os_kmem_alloc(void *d, u32 buf_sz)
{
	#ifdef DBG_PHL_MEM_ALLOC
	struct dvobj_priv *obj = (struct dvobj_priv *)d;
	ATOMIC_ADD_RETURN(&obj->phl_mem, buf_sz);
	#endif
	return rtw_zmalloc(buf_sz);
}

/*physically contiguous memory if the buffer will be accessed by a DMA device*/
static inline void _os_kmem_free(void *d, void *buf, u32 buf_sz)
{
	#ifdef DBG_PHL_MEM_ALLOC
	struct dvobj_priv *obj = (struct dvobj_priv *)d;
	ATOMIC_SUB(&obj->phl_mem, buf_sz);
	#endif

	rtw_mfree(buf, buf_sz);
}
static inline void _os_mem_set(void *d, void *buf, s8 value, u32 size)
{
	_rtw_memset(buf, value, size);
}
static inline void _os_mem_cpy(void *d, void *dest, void *src, u32 size)
{
	_rtw_memcpy(dest, src, size);
}
/*Return Value
 * <0 :the first byte that does not match in both memory blocks has a lower value in ptr1 than in ptr2 (if evaluated as unsigned char values)
 * 0   :the contents of both memory blocks are equal
 * >0 :the first byte that does not match in both memory blocks has a greater value in ptr1 than in ptr2 (if evaluated as unsigned char values)
*/
static inline int _os_mem_cmp(void *d, const void *dest, const void *src, size_t size)
{

	return memcmp(dest, src, size);
}
static inline void _os_init_timer(void *d, _os_timer *timer,
		void (*call_back_func)(void *context), void *context,
		const char *sz_id)
{
	_init_timer(timer, call_back_func, context);
}

static inline void _os_set_timer(void *d, _os_timer *timer, u32 ms_delay)
{
	_set_timer(timer, ms_delay);
}

static inline void _os_cancel_timer(void *d, _os_timer *timer)
{
	_cancel_timer_ex(timer);
}

static inline void _os_cancel_timer_async(void *d, _os_timer *timer)
{
	_cancel_timer_async(timer);
}

static inline void _os_release_timer(void *d, _os_timer *timer)
{

}
static inline void _os_mutex_init(void *d, _os_mutex *mutex)
{
	_rtw_mutex_init(mutex);
}

static inline void _os_mutex_deinit(void *d, _os_mutex *mutex)
{
	_rtw_mutex_free(mutex);
}

static inline void _os_mutex_lock(void *d, _os_mutex *mutex)
{
	_rtw_mutex_lock_interruptible(mutex);
}

static inline void _os_mutex_unlock(void *d, _os_mutex *mutex)
{
	_rtw_mutex_unlock(mutex);
}

static inline void _os_sema_init(void *d, _os_sema *sema, int int_cnt)
{
	_rtw_init_sema(sema, int_cnt);
}

static inline void _os_sema_free(void *d, _os_sema *sema)
{
	_rtw_free_sema(sema);
}

static inline void _os_sema_up(void *d, _os_sema *sema)
{
	_rtw_up_sema(sema);
}

static inline u8 _os_sema_down(void *d, _os_sema *sema)
{
	_rtw_down_sema(sema);
	return 0; //success
}

/* event */
static __inline void _os_event_init(void *h, _os_event *event)
{
	init_completion(event);
}

static __inline void _os_event_free(void *h, _os_event *event)
{
}

static __inline void _os_event_reset(void *h, _os_event *event)
{
	/* TODO */
}

static __inline void _os_event_set(void *h, _os_event *event)
{
	complete(event);
}

/*
 * m_sec
 * 	== 0 : wait for completion
 * 	>  0 : wait for timeout or completion
 * return value
 * 	0:timeout
 * 	otherwise:success
 */
static __inline int _os_event_wait(void *h, _os_event *event, u32 m_sec)
{
	unsigned long expire;

	if (m_sec) {
		expire = msecs_to_jiffies(m_sec);

		if (expire > MAX_SCHEDULE_TIMEOUT)
			expire = MAX_SCHEDULE_TIMEOUT;
	}
	else {
		expire = MAX_SCHEDULE_TIMEOUT;
	}

	expire = wait_for_completion_timeout(event, expire);

	if (expire == 0)
		return 0; /* timeout */

	return jiffies_to_msecs(expire); /* success */
}

/* spinlock */

static inline void _os_spinlock_init(void *d, _os_lock *plock)
{
	_rtw_spinlock_init(plock);
}
static inline void _os_spinlock_free(void *d, _os_lock *plock)
{
	_rtw_spinlock_free(plock);
}

static inline void _os_spinlock(void *d, _os_lock *plock,
					enum lock_type type, _os_spinlockfg *flags
)
{
	if(type == _irq)
	{
		if(flags==NULL)
			RTW_ERR("_os_spinlock_irq: flags=NULL @%s:%u\n",
				__FUNCTION__, __LINE__);
		_rtw_spinlock_irq(plock, flags);
	}
	else if(type == _bh)
		_rtw_spinlock_bh(plock);
	else if(type == _ps)
		_rtw_spinlock(plock);
}
static inline void _os_spinunlock(void *d, _os_lock *plock,
					enum lock_type type, _os_spinlockfg *flags
)
{
	if(type == _irq)
	{
		if(flags==NULL)
			RTW_ERR("_os_spinunlock_irq: flags=NULL @%s:%u\n",
				__FUNCTION__, __LINE__);
		_rtw_spinunlock_irq(plock, flags);
	}
	else if(type == _bh)
		_rtw_spinunlock_bh(plock);
	else if(type == _ps)
		_rtw_spinunlock(plock);
}
static inline int _os_test_and_clear_bit(int nr, unsigned long *addr)
{
	return rtw_test_and_clear_bit(nr, addr);
}
static inline int _os_test_and_set_bit(int nr, unsigned long *addr)
{
	return rtw_test_and_set_bit(nr, addr);
}
/* Atomic integer operations */
static inline void _os_atomic_set(void *d, _os_atomic *v, int i)
{
	ATOMIC_SET(v, i);
}

static inline int _os_atomic_read(void *d, _os_atomic *v)
{
	return ATOMIC_READ(v);
}

static inline void _os_atomic_add(void *d, _os_atomic *v, int i)
{
	ATOMIC_ADD(v, i);
}
static inline void _os_atomic_sub(void *d, _os_atomic *v, int i)
{
	ATOMIC_SUB(v, i);
}

static inline void _os_atomic_inc(void *d, _os_atomic *v)
{
	ATOMIC_INC(v);
}

static inline void _os_atomic_dec(void *d, _os_atomic *v)
{
	ATOMIC_DEC(v);
}

static inline int _os_atomic_add_return(void *d, _os_atomic *v, int i)
{
	return ATOMIC_ADD_RETURN(v, i);
}

static inline int _os_atomic_sub_return(void *d, _os_atomic *v, int i)
{
	return ATOMIC_SUB_RETURN(v, i);
}

static inline int _os_atomic_inc_return(void *d, _os_atomic *v)
{
	return ATOMIC_INC_RETURN(v);
}

static inline int _os_atomic_dec_return(void *d, _os_atomic *v)
{
	return ATOMIC_DEC_RETURN(v);
}
/*
static inline bool _os_atomic_inc_unless(void *d, _os_atomic *v, int u)
{
	return ATOMIC_INC_UNLESS(v, 1, u);
}
*/

static inline u8 _os_tasklet_init(void *drv_priv, _os_tasklet *task,
	void (*call_back_func)(void* context), void *context)
{
	rtw_tasklet_init(task,
			 (void(*)(unsigned long))call_back_func,
			 (unsigned long)task);
	return 0;
}
static inline u8 _os_tasklet_deinit(void *drv_priv, _os_tasklet *task)
{
	rtw_tasklet_kill(task);
	return 0;
}
static inline u8 _os_tasklet_schedule(void *drv_priv, _os_tasklet *task)
{
	#if 1
	rtw_tasklet_hi_schedule(task);
	#else
	rtw_tasklet_schedule(task);
	#endif
	return 0;
}

static __inline u8 _os_thread_init(	void *drv_priv, _os_thread *thread,
					int (*call_back_func)(void * context),
					void *context,
					const char namefmt[])
{
	thread->thread_handler = rtw_thread_start((int(*)(void*))call_back_func, context, namefmt);
	if (thread->thread_handler) {
		RST_THREAD_STATUS(thread);
		SET_THREAD_STATUS(thread, THREAD_STATUS_STARTED);
		return RTW_PHL_STATUS_SUCCESS;
	}

	return RTW_PHL_STATUS_FAILURE;
}
static __inline u8 _os_thread_deinit(void *drv_priv, _os_thread *thread)
{
	if (CHK_THREAD_STATUS(thread, THREAD_STATUS_STARTED)) {
		CLR_THREAD_STATUS(thread, THREAD_STATUS_STARTED);
		return rtw_thread_stop(thread->thread_handler);
	}

	return RTW_PHL_STATUS_SUCCESS;
}
static __inline enum rtw_phl_status _os_thread_schedule(void *drv_priv, _os_thread *thread)
{
	return RTW_PHL_STATUS_SUCCESS;
}
static inline void _os_thread_stop(void *drv_priv, _os_thread *thread)
{
	SET_THREAD_STATUS(thread, THREAD_STATUS_STOPPED);
}
static inline int _os_thread_check_stop(void *drv_priv, _os_thread *thread)
{
	return CHK_THREAD_STATUS(thread, THREAD_STATUS_STOPPED);
}

static inline int _os_thread_wait_stop(void *drv_priv, _os_thread *thread)
{
	rtw_thread_wait_stop();
	return RTW_PHL_STATUS_SUCCESS;
}

#if 0
static inline _os_thread _os_thread_start(int (*threadfn)(void *data),
	void *data, const char namefmt[])
{
	return rtw_thread_start(threadfn, data, namefmt);
}
static inline bool _os_thread_stop(_os_thread th)
{

	return rtw_thread_stop(th);
}
static inline void _os_thread_wait_stop(void)
{
	rtw_thread_wait_stop();
}
static inline int _os_thread_should_stop(void)
{
	return kthread_should_stop();
}
#endif

#ifdef CONFIG_PHL_CPU_BALANCE
static inline u8 _os_workitem_config_cpu(void *drv_priv, _os_workitem *workitem,
			char *work_name, u8 cpu_id)
{
	_config_workitem_cpu(workitem, work_name, cpu_id);
	return 0;
}
#endif


static inline u8 _os_workitem_init(void *drv_priv, _os_workitem *workitem,
			void (*call_back_func)(void* context), void *context)
{
#ifdef CONFIG_PHL_CPU_BALANCE
	_init_workitem_cpu(workitem, call_back_func, context);
#else
	_init_workitem(workitem, call_back_func, context);
#endif
	return 0;
}
static inline u8 _os_workitem_schedule(void *drv_priv, _os_workitem *workitem)
{
#ifdef CONFIG_PHL_CPU_BALANCE
	_set_workitem_cpu(workitem);
#else
	_set_workitem(workitem);
#endif
	return 0;
}
static inline u8 _os_workitem_deinit(void *drv_priv, _os_workitem *workitem)
{
#ifdef CONFIG_PHL_CPU_BALANCE
	_cancel_workitem_sync_cpu(workitem);
#else
	_cancel_workitem_sync(workitem);
#endif
	return 0;
}

/* File Operation */
static inline u32 _os_read_file(const char *path, u8 *buf, u32 sz)
{
	return (u32)rtw_retrieve_from_file(path, buf, sz);
}

/*BUS*/
#ifdef CONFIG_PCI_HCI
#include <pci_ops_linux.h>
static inline u8 _os_read8_pcie(void *d, u32 addr)
{
	return os_pci_read8((struct dvobj_priv *)d, addr);
}
static inline u16 _os_read16_pcie(void *d, u32 addr)
{
	return os_pci_read16((struct dvobj_priv *)d, addr);

}
static inline u32 _os_read32_pcie(void *d, u32 addr)
{
	return os_pci_read32((struct dvobj_priv *)d, addr);
}

static inline int _os_write8_pcie(void *d, u32 addr, u8 val)
{
	return os_pci_write8((struct dvobj_priv *)d, addr, val);
}
static inline int _os_write16_pcie(void *d, u32 addr, u16 val)
{
	return os_pci_write16((struct dvobj_priv *)d, addr, val);
}
static inline int _os_write32_pcie(void *d, u32 addr, u32 val)
{
	return os_pci_write32((struct dvobj_priv *)d, addr, val);
}
#endif/*#ifdef CONFIG_PCI_HCI*/

#ifdef CONFIG_USB_HCI
#include <usb_ops_linux.h>
static inline int _os_usbctrl_vendorreq(void *d, u8 request, u16 value,
				u16 index, void *pdata, u16 len, u8 requesttype)
{
	return usbctrl_vendorreq((struct dvobj_priv *)d, request, value,
				index, pdata, len, requesttype);
}
static __inline u8 os_out_token_alloc(void *drv_priv)
{
	return 0; // RTW_PHL_STATUS_SUCCESS
}

static __inline void os_out_token_free(void *drv_priv)
{
}

static inline int os_usb_tx(void *d, u8 *tx_buf_ptr,
			u8 bulk_id, u32 len, u8 *pkt_data_buf)
{
	return rtw_usb_write_port((struct dvobj_priv *)d, tx_buf_ptr,
			bulk_id, len, pkt_data_buf);
}

static __inline void os_enable_usb_out_pipes(void *drv_priv)
{
}

static __inline void os_disable_usb_out_pipes(void *drv_priv)
{
	/* Free bulkout urb */
	rtw_usb_write_port_cancel(drv_priv);
}

static __inline u8 os_in_token_alloc(void *drv_priv)
{
	// Allocate in token (pUrb) list
	return 0;
}

static __inline void os_in_token_free(void *drv_priv)
{
	// free in token memory
	/*rtw_usb_read_port_free(drv_priv);*/
}


static __inline u8 os_send_usb_in_token(void *drv_priv, void *rxobj, u8 *inbuf, u32 inbuf_len, u8 pipe_idx, u8 minLen)
{
	return rtw_usb_read_port(drv_priv, rxobj, inbuf, inbuf_len, pipe_idx, minLen);
}

static __inline void os_enable_usb_in_pipes(void *drv_priv)
{
}

static __inline void os_disable_usb_in_pipes(void *drv_priv)
{
	// Cancel Pending IN IRPs.
	rtw_usb_read_port_cancel(drv_priv);
}


#endif /*CONFIG_USB_HCI*/

#ifdef CONFIG_SDIO_HCI
#include <rtw_sdio.h>
#include <sdio_ops_linux.h>
#include <rtw_debug.h>

static inline u8 _os_sdio_cmd52_r8(void *d, u32 offset)
{
	u8 val = SDIO_ERR_VAL8;

	if (rtw_sdio_read_cmd52((struct dvobj_priv *)d, offset, &val, 1) == _FAIL)
		RTW_ERR("%s: I/O FAIL!\n", __FUNCTION__);

	return val;
}

static inline u8 _os_sdio_cmd53_r8(void *d, u32 offset)
{
	u8 val = SDIO_ERR_VAL8;


	if (rtw_sdio_read_cmd53((struct dvobj_priv *)d, offset, &val, 1) == _FAIL)
		RTW_ERR("%s: I/O FAIL!\n", __FUNCTION__);

	return val;
}

static inline u16 _os_sdio_cmd53_r16(void *d, u32 offset)
{
	u16 val = SDIO_ERR_VAL16;


	if (rtw_sdio_read_cmd53((struct dvobj_priv *)d, offset, &val, 2) == _FAIL) {
		RTW_ERR("%s: I/O FAIL!\n", __FUNCTION__);
		goto exit;
	}
	val = le16_to_cpu(val);

exit:
	return val;
}

static inline u32 _os_sdio_cmd53_r32(void *d, u32 offset)
{
	u32 val = SDIO_ERR_VAL32;


	if (rtw_sdio_read_cmd53((struct dvobj_priv *)d, offset, &val, 4) == _FAIL) {
		RTW_ERR("%s: I/O FAIL!\n", __FUNCTION__);
		goto exit;
	}
	val = le32_to_cpu(val);

exit:
	return val;
}

static inline u8 _os_sdio_cmd53_rn(void *d, u32 offset, u32 size, u8 *data)
{
	struct dvobj_priv *dv = d;
	struct sdio_data *sdio = dvobj_to_sdio(dv);
	u8 *pbuf = data;
	u32 sdio_read_size;

	if (!data)
		return _FAIL;

	sdio_read_size = RND4(size);
	sdio_read_size = rtw_sdio_cmd53_align_size(dv, sdio_read_size);

	if (sdio_read_size > sdio->tmpbuf_sz) {
		pbuf = rtw_malloc(sdio_read_size);
		if (!pbuf)
			return _FAIL;
	}

	if (rtw_sdio_read_cmd53(dv, offset, pbuf, sdio_read_size) == _FAIL) {
		RTW_ERR("%s: I/O FAIL!\n", __FUNCTION__);
		goto exit;
	}

	if (pbuf != data)
		_rtw_memcpy(data, pbuf, size);

exit:
	if (pbuf != data)
		rtw_mfree(pbuf, sdio_read_size);

	return _SUCCESS;
}

static inline u8 _os_sdio_cmd53_r(void *d, u32 offset, u32 size, u8 *data)
{
	u8 ret;

	ret = rtw_sdio_read_cmd53((struct dvobj_priv *)d, offset, data, size);
	if (ret == _FAIL) {
		RTW_ERR("%s: I/O FAIL!\n", __FUNCTION__);
		return _FAIL;
	}

	return _SUCCESS;
}

static inline void _os_sdio_cmd52_w8(void *d, u32 offset, u8 val)
{
	if (rtw_sdio_write_cmd52((struct dvobj_priv *)d, offset, &val, 1) == _FAIL)
		RTW_ERR("%s: I/O FAIL!\n", __FUNCTION__);
}

static inline void _os_sdio_cmd53_w8(void *d, u32 offset, u8 val)
{
	if (rtw_sdio_write_cmd53((struct dvobj_priv *)d, offset, &val, 1) == _FAIL)
		RTW_ERR("%s: I/O FAIL!\n", __FUNCTION__);
}

static inline void _os_sdio_cmd53_w16(void *d, u32 offset, u16 val)
{
	val = cpu_to_le16(val);
	if (rtw_sdio_write_cmd53((struct dvobj_priv *)d, offset, &val, 2) == _FAIL)
		RTW_ERR("%s: I/O FAIL!\n", __FUNCTION__);
}

static inline void _os_sdio_cmd53_w32(void *d, u32 offset, u32 val)
{
	val = cpu_to_le32(val);
	if (rtw_sdio_write_cmd53((struct dvobj_priv *)d, offset, &val, 4) == _FAIL)
		RTW_ERR("%s: I/O FAIL!\n", __FUNCTION__);
}

static inline void _os_sdio_cmd53_wn(void *d, u32 offset, u32 size, u8 *data)
{
	struct dvobj_priv *dv = d;
	struct sdio_data *sdio = dvobj_to_sdio(dv);
	u8 *pbuf = data;


	if (size > sdio->tmpbuf_sz) {
		pbuf = rtw_malloc(size);
		if (!pbuf)
			return;
		_rtw_memcpy(pbuf, data, size);
	}

	if (rtw_sdio_write_cmd53(dv, offset, pbuf, size) == _FAIL)
		RTW_ERR("%s: I/O FAIL!\n", __FUNCTION__);

	if (pbuf != data)
		rtw_mfree(pbuf, size);
}

static inline void _os_sdio_cmd53_w(void *d, u32 offset, u32 size, u8 *data)
{
	u8 ret;

	ret = rtw_sdio_write_cmd53((struct dvobj_priv *)d, offset, data, size);
	if (ret == _FAIL)
		RTW_ERR("%s: I/O FAIL!\n", __FUNCTION__);
}

static inline u8 _os_sdio_f0_read(void *d, u32 addr, void *buf, size_t len)
{
	return rtw_sdio_f0_read((struct dvobj_priv *)d, addr, buf, len);
}

static inline u8 _os_sdio_read_cia_r8(void *d, u32 addr)
{
	u8 data = 0;

	if (rtw_sdio_f0_read((struct dvobj_priv *)d, addr, &data, 1) == _FAIL)
		RTW_ERR("%s: read sdio cia FAIL!\n", __FUNCTION__);

	return data;
}

#endif /*CONFIG_SDIO_HCI*/
#endif /*_PLTFM_OPS_LINUX_H_*/
