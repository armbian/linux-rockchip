// SPDX-License-Identifier: GPL-2.0
/*
 * Crypto acceleration support for Rockchip crypto engine
 *
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Lin Jinhan <troy.lin@rock-chips.com>
 *
 */

#define RKCE_MODULE_TAG		"BUF"
#define RKCE_MODULE_OFFSET	10

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include "rkce_buf.h"
#include "rkce_debug.h"
#include "rkce_error.h"

struct rkce_cma_buf_data {
	void		*virt;
	uint32_t	phys;
	uint32_t	size;
	uint32_t	reserved;
	struct list_head list;
};

static struct device *g_dev;
static LIST_HEAD(g_buf_list);
static DEFINE_MUTEX(g_buf_lock);

int rkce_cma_init(void *device)
{
	int err;

	g_dev = device;

	err = dma_set_mask_and_coherent(g_dev, DMA_BIT_MASK(32));
	if (err)
		dev_err(g_dev, "No suitable DMA available.\n");

	return err;
}

void rkce_cma_deinit(void *device)
{
	struct rkce_cma_buf_data *cma_buf = NULL;
	struct list_head *pos = NULL, *q = NULL;

	if (!device || device != g_dev)
		return;

	mutex_lock(&g_buf_lock);

	list_for_each_safe(pos, q, &g_buf_list) {
		cma_buf = list_entry(pos, struct rkce_cma_buf_data, list);

		list_del(&cma_buf->list);

		dma_free_coherent(g_dev, cma_buf->size, cma_buf->virt, cma_buf->phys);
		memset(cma_buf, 0x00, sizeof(*cma_buf));
		kfree(cma_buf);
	}

	g_dev = NULL;

	mutex_unlock(&g_buf_lock);
}

void *rkce_cma_alloc(uint32_t size)
{
	struct rkce_cma_buf_data *cma_buf;
	dma_addr_t dma_handle = 0;

	mutex_lock(&g_buf_lock);

	cma_buf = kzalloc(sizeof(*cma_buf), GFP_KERNEL);
	if (!cma_buf)
		goto error;

	cma_buf->virt = dma_alloc_coherent(g_dev, size, &dma_handle, GFP_KERNEL);
	if (!cma_buf->virt) {
		kfree(cma_buf);
		cma_buf = NULL;
		goto error;
	}

	cma_buf->phys = (uint32_t)dma_handle;
	cma_buf->size = size;

	list_add(&cma_buf->list, &g_buf_list);

	mutex_unlock(&g_buf_lock);

	rk_debug("++++++ alloc cma buff: virt(%p), phys(%08x), size(%u)\n",
		 cma_buf->virt, cma_buf->phys, cma_buf->size);

	return cma_buf->virt;
error:
	mutex_unlock(&g_buf_lock);

	return NULL;
}

void rkce_cma_free(void *buf)
{
	struct rkce_cma_buf_data *cma_buf = NULL;
	struct list_head *pos = NULL, *q = NULL;
	bool virt_match = false;

	if (!buf)
		return;

	mutex_lock(&g_buf_lock);

	list_for_each_safe(pos, q, &g_buf_list) {
		cma_buf = list_entry(pos, struct rkce_cma_buf_data, list);
		if (cma_buf->virt == buf) {
			virt_match = true;
			list_del(&cma_buf->list);
			break;
		}
	}

	if (virt_match) {
		rk_debug("------ free cma buff: virt(%p), phys(%08x), size(%u)\n",
			 cma_buf->virt, cma_buf->phys, cma_buf->size);

		dma_free_coherent(g_dev, cma_buf->size, cma_buf->virt, cma_buf->phys);
		memset(cma_buf, 0x00, sizeof(*cma_buf));
		kfree(cma_buf);
	}

	mutex_unlock(&g_buf_lock);
}

uint32_t rkce_cma_virt2phys(void *buf)
{
	struct rkce_cma_buf_data *cma_buf = NULL;
	struct list_head *pos = NULL, *q = NULL;
	uint32_t phys = 0;

	if (!buf)
		goto exit;

	mutex_lock(&g_buf_lock);

	list_for_each_safe(pos, q, &g_buf_list) {
		cma_buf = list_entry(pos, struct rkce_cma_buf_data, list);
		if (cma_buf->virt <= buf && buf <= cma_buf->virt + cma_buf->size) {
			phys = cma_buf->phys + (buf - cma_buf->virt);
			goto exit;
		}
	}
exit:
	mutex_unlock(&g_buf_lock);

	rk_debug("virt(%p) -> phys(%08x)\n", buf, phys);

	return phys;
}

void *rkce_cma_phys2virt(uint32_t phys)
{
	struct rkce_cma_buf_data *cma_buf = NULL;
	struct list_head *pos = NULL, *q = NULL;
	void *virt = NULL;

	if (!phys)
		goto exit;

	mutex_lock(&g_buf_lock);

	list_for_each_safe(pos, q, &g_buf_list) {
		cma_buf = list_entry(pos, struct rkce_cma_buf_data, list);
		rk_debug("phys = %x, [%x, %x]\n ",
			 phys, cma_buf->phys, cma_buf->phys + cma_buf->size);
		if (cma_buf->phys <= phys && phys <= cma_buf->phys + cma_buf->size) {
			rk_debug("cma_buf->virt = %p phys = %x, cma_buf->phys = %x, diff = %x\n",
				cma_buf->virt, phys, cma_buf->phys, (phys - cma_buf->phys));
			virt = cma_buf->virt + (phys - cma_buf->phys);
			goto exit;
		}
	}

exit:
	mutex_unlock(&g_buf_lock);

	rk_debug("phys(%08x)-> virt(%p)\n", phys, virt);

	return virt;
}
