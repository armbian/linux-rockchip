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
#define _PHL_DEBUG_C_
#include "phl_headers.h"

#ifdef CONFIG_RTW_DEBUG
enum _phl_dbg_comp_mode {
	PHL_SET_COMP_BIT = 0x01,
	PHL_CLEAR_COMP_BIT = 0x2,
	PHL_SHOW_COMP = 0x3,
	PHL_SET_COMP_VALUE = 0x4,
};

u32 phl_log_components = COMP_PHL_XMIT |
			 COMP_PHL_WOW |
			/* COMP_PHL_CMDDISP |*/
			 COMP_PHL_RECV |
			 COMP_PHL_MAC |
			#ifdef CONFIG_POWER_SAVE
			 COMP_PHL_PS |
			#endif
			 COMP_PHL_DBG | 0;
u8 phl_log_level = _PHL_INFO_;
struct dbg_mem_ctx debug_memory_ctx;

void debug_dump_mac_address(u8 *mac_addr)
{
	if (mac_addr == NULL) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "[dump mac addr] mac_addr is NULL\n");
		return;
	}

	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "[MAC ADDRESS]\n");
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
		 "%02X-%02X-%02X-%02X-%02X-%02X\n",
		 mac_addr[0], mac_addr[1], mac_addr[2],
		 mac_addr[3], mac_addr[4], mac_addr[5]);
}

void debug_dump_data(u8 *buf, u32 buf_len, const char *prefix)
{
	u32 i;

	if (buf == NULL) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_, "[debug dump] buf is NULL\n");
		return;
	}

	PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_, "[debug dump] %s\n", prefix);
	for (i = 0; i < buf_len; i++) {
		if (!(i % 8))
			PHL_DATA(COMP_PHL_DBG, _PHL_DEBUG_, "\n");
		PHL_DATA(COMP_PHL_DBG, _PHL_DEBUG_, "%02X ", buf[i]);
		}
	PHL_DATA(COMP_PHL_DBG, _PHL_DEBUG_, "\n");
}

void rt_alloc_dbg_buf(void *phl, u8 *buf_ptr, u32 buf_size,
		const u8 *file_name, u32 line_num, const u8 *func_name)
{
	_os_list *list = &debug_memory_ctx.alloc_buf_list;
	_os_lock *lock = &debug_memory_ctx.alloc_buf_list_lock;
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct dbg_alloc_buf *dbg_buf = NULL;
	u32 name_size = 0;

	FUNCIN();

	dbg_buf = _os_mem_alloc(phl_to_drvpriv(phl_info), sizeof(*dbg_buf));

	dbg_buf->buf_ptr = buf_ptr;
		dbg_buf->buf_size = buf_size;

	name_size = (_os_strlen((u8 *)file_name) > DEBUG_MAX_NAME_LEN) ?
		DEBUG_MAX_NAME_LEN : _os_strlen((u8 *)file_name);
	_os_mem_cpy(phl_to_drvpriv(phl_info), dbg_buf->file_name, (u8 *)file_name, name_size);

	dbg_buf->line_num = line_num;

	name_size = (_os_strlen((u8 *)func_name) > DEBUG_MAX_NAME_LEN) ?
		DEBUG_MAX_NAME_LEN : _os_strlen((u8 *)func_name);
	_os_mem_cpy(phl_to_drvpriv(phl_info), dbg_buf->func_name, (u8 *)func_name, name_size);

	_os_spinlock(phl_to_drvpriv(phl_info), lock, _bh, NULL);

	list_add_tail(&(dbg_buf->list), list);
	debug_memory_ctx.alloc_buf_cnt++;

	_os_spinunlock(phl_to_drvpriv(phl_info), lock, _bh, NULL);

	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Allocate Memory: %p, size: %d, File: %s, Line: %d, Function: %s\n",
		 dbg_buf->buf_ptr, dbg_buf->buf_size, dbg_buf->file_name,
		 dbg_buf->line_num, dbg_buf->func_name);

	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Current allocated buffer count: %d",
		 debug_memory_ctx.alloc_buf_cnt);

	FUNCOUT();
}


void rt_free_dbg_buf(void *phl, u8 *buf_ptr, u32 buf_size,
		const u8 *file_name, u32 line_num, const u8 *func_name)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	_os_list *list = &debug_memory_ctx.alloc_buf_list;
	_os_lock *lock = &debug_memory_ctx.alloc_buf_list_lock;
	struct dbg_alloc_buf *dbg_buf = NULL , *t;
	u8 found = false;

	FUNCIN();

	_os_spinlock(phl_to_drvpriv(phl_info), lock, _bh, NULL);

	phl_list_for_loop_safe(dbg_buf, t, struct dbg_alloc_buf, list, list) {
		if (dbg_buf->buf_ptr == buf_ptr) {
			list_del(&dbg_buf->list);
			found = true;
			break;
		}
	}
	_os_spinunlock(phl_to_drvpriv(phl_info), lock, _bh, NULL);

	if (true == found) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Free Memory: %p, size: %d, File: %s, Line: %d, Function: %s\n",
			 dbg_buf->buf_ptr, dbg_buf->buf_size, file_name,
			 line_num, func_name);
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Current allocated buffer count: %d",
			 debug_memory_ctx.alloc_buf_cnt);
		_os_mem_free(phl_to_drvpriv(phl_info), dbg_buf, sizeof(*dbg_buf));
		debug_memory_ctx.alloc_buf_cnt--;
	} else {
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
			"WARNING, can not find allocated buffer in list\n");
	}

	FUNCOUT();
}


void rt_mem_dbg_init(void *phl)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	_os_list *list = &debug_memory_ctx.alloc_buf_list;
	_os_lock *lock = &debug_memory_ctx.alloc_buf_list_lock;

	FUNCIN();
	debug_memory_ctx.alloc_buf_cnt = 0;
	INIT_LIST_HEAD(list);
	_os_spinlock_init(phl_to_drvpriv(phl_info), lock);
	FUNCOUT();
}

void rt_mem_dbg_deinit(void *phl)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	_os_list *list = &debug_memory_ctx.alloc_buf_list;
	_os_lock *lock = &debug_memory_ctx.alloc_buf_list_lock;
	struct dbg_alloc_buf *dbg_buf = NULL;

	FUNCIN();
	while (true != list_empty(list) ||
		0 != debug_memory_ctx.alloc_buf_cnt) {

		_os_spinlock(phl_to_drvpriv(phl_info), lock, _bh, NULL);
		dbg_buf = list_first_entry(list, struct dbg_alloc_buf, list);
		list_del(&dbg_buf->list);
		_os_spinunlock(phl_to_drvpriv(phl_info), lock, _bh, NULL);

		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "FOUND non-freed Memory: %p, size: %d, File: %s, Line: %d, Function: %s\n",
			dbg_buf->buf_ptr, dbg_buf->buf_size,
			dbg_buf->file_name, dbg_buf->line_num,
			dbg_buf->func_name);
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Current allocated buffer count: %d",
			debug_memory_ctx.alloc_buf_cnt);
		_os_mem_free(phl_to_drvpriv(phl_info), dbg_buf, sizeof(*dbg_buf));
		debug_memory_ctx.alloc_buf_cnt--;
	}
	_os_spinlock_init(phl_to_drvpriv(phl_info), lock);

	FUNCOUT();
}


void phl_dbg_show_log_comp(u32 para_dbg)
{
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "[DBG] phl_log_components = 0x%x \n", phl_log_components);
}

void phl_dbg_set_log_comp(u32 para_dbg)
{
	phl_log_components |= BIT(para_dbg);
}

void phl_dbg_clear_log_comp(u32 para_dbg)
{
	phl_log_components &= (~BIT(para_dbg));
}

void phl_dbg_set_log_comp_value(u32 para_dbg)
{
	phl_log_components = para_dbg;
}

u32 rtw_phl_dbg_ctrl_comp(u8 ctrl, u32 para_dbg)
{
	switch(ctrl){
	case PHL_SET_COMP_BIT:
		if (para_dbg > 31)
			PHL_PRINT( "[DBG] bit num is illegal\n");
		else
			phl_dbg_set_log_comp(para_dbg);
		break;
	case PHL_CLEAR_COMP_BIT:
		if (para_dbg > 31)
			PHL_PRINT("[DBG] bit num is illegal\n");
		else
			phl_dbg_clear_log_comp(para_dbg);
		break;
	case PHL_SET_COMP_VALUE:
		phl_dbg_set_log_comp_value(para_dbg);
		break;
	default:
		break;
	}

	phl_dbg_show_log_comp(para_dbg);

	return phl_log_components;
}
#endif	/* #ifdef CONFIG_PHL_DEBUG */

