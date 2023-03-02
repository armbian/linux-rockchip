/******************************************************************************
 *
 * Copyright(c) 2019 Realtek Corporation.
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
#ifndef _PHL_UTIL_H_
#define _PHL_UTIL_H_

#define phlcom_to_drvpriv(_pcom) 	(_pcom->drv_priv)
#define phl_is_mp_mode(_phl_com)	(_phl_com->drv_mode >= RTW_DRV_MODE_MP_SMDL_START && _phl_com->drv_mode <= RTW_DRV_MODE_MP_SMDL_END)

#ifndef is_broadcast_mac_addr
#define is_broadcast_mac_addr(addr) ((((addr[0]) & 0xff) == 0xff) && (((addr[1]) & 0xff) == 0xff) && \
	(((addr[2]) & 0xff) == 0xff) && (((addr[3]) & 0xff) == 0xff) && (((addr[4]) & 0xff) == 0xff) && \
					(((addr[5]) & 0xff) == 0xff))
#endif
#ifndef MIN
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#endif

#ifndef DIFF
#define DIFF(_x_, _y_) ((_x_ >= _y_) ? (_x_ - _y_) : (_y_ - _x_))
#endif

#define SET_STATUS_FLAG(_status,_flags)	\
	((_status) |= (_flags))
#define TEST_STATUS_FLAG(_status,_flags)\
	(((_status) & (_flags))==(_flags))
#define CLEAR_STATUS_FLAG(_status,_flags)\
	((_status) &= ~(_flags))

static inline void _add_bitmap_bit(u8 *bitmap, u8 *arr, u32 len)
{
	u32 k = 0;
	for(k = 0; k < (len); k++)
		bitmap[arr[k] / 8] |= (BIT0 << (arr[k] % 8));
}

static inline void _clr_bitmap_bit(u8 *bitmap, u8 *arr, u32 len)
{
	u32 k = 0;

	for(k = 0; k < (len); k++)
		bitmap[arr[k] / 8] &= ~(BIT0 << (arr[k] % 8));
}

#define _chk_bitmap_bit(_bitmap, _id) \
	((_bitmap)[(_id) / 8] & (BIT0 << ((_id) % 8)))

#define _reset_bitmap(_d, _bitmap ,_len) _os_mem_set(_d, _bitmap, 0, _len)

static inline void _and_bitmaps( u8* ref_bitmap, u8* _bitmap, u32 len)
{
	u32 k = 0;

	for(k = 0; k < len; k++)
		_bitmap[k] &= ref_bitmap[k];
}


/*phl_queue*/
struct phl_queue {
	_os_list queue;
	_os_lock lock;
	int cnt;
};
static inline _os_list *_get_next(_os_list *list)
{
	return list->next;
}

static inline _os_list *_get_prev(_os_list *list)
{
	return list->prev;
}


static inline _os_list *_get_list_head(struct phl_queue *q)
{
	return (&q->queue);
}

void pq_init(void *d, struct phl_queue *q);
void pq_deinit(void *d, struct phl_queue *q);
void pq_reset(void *d, struct phl_queue *q, enum lock_type type);
u8 pq_push(void *d, struct phl_queue *q, _os_list *obj, u8 pos, enum lock_type type);
u8 pq_pop(void *d, struct phl_queue *q, _os_list **obj, u8 pos, enum lock_type type);
u8 pq_get_next(void *d, struct phl_queue *queue, _os_list *cur_obj,
	       _os_list **obj, enum lock_type type);
u8 pq_get_front(void *d, struct phl_queue *queue, _os_list **obj,
		enum lock_type type);
u8 pq_get_tail(void *d, struct phl_queue *q, _os_list **obj, enum lock_type type);
u8 pq_get_prev(void *d, struct phl_queue *queue, _os_list *cur_obj,
	       _os_list **obj, enum lock_type type);
void pq_del_node(void *d, struct phl_queue *q, _os_list *obj, enum lock_type type);

u8 pq_search_node(void *d, struct phl_queue *q, _os_list **obj,
		  enum lock_type type, bool bdel, void *priv,
		  u8 (*search_fun)(void *d, void *obj, void *priv));
u8 pq_insert(void *d, struct phl_queue *q, enum lock_type type, void *priv, _os_list *input,
		  u8 (*pq_predicate)(void *d, void *priv,_os_list *input, _os_list *obj));
u32 phl_get_passing_time_us(u32 start);
u32 phl_get_passing_time_ms(u32 start);

#define rtw_phl_is_ap_category(_type) (_type == PHL_RTYPE_AP ||\
				       _type == PHL_RTYPE_P2P_GO ||\
				       _type == PHL_RTYPE_VAP)
#define rtw_phl_is_client_category(_type) (_type == PHL_RTYPE_STATION ||\
					   _type == PHL_RTYPE_P2P_GC ||\
					   _type == PHL_RTYPE_TDLS)
#define rtw_phl_role_is_ap_category(_wrole) (rtw_phl_is_ap_category(_wrole->type))
#define rtw_phl_role_is_client_category(_wrole) (rtw_phl_is_client_category(_wrole->type))
#endif /*_PHL_UTIL_H_*/

