// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip Remote Processors Messaging.
 *
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 * Author: Zain Wang <zain.wang@rock-chips.com>
 */

#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/log2.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/rpmsg.h>
#include <linux/rpmsg/rockchip_rpmsg.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/virtio.h>
#include <linux/workqueue.h>

#include "rockchip_amp_rpmsg_sharedmem.h"
#include "../../rpmsg/rpmsg_internal.h"

#define RKAMP_SHAREMEM_PREFIX	"rpmsg-sharedmem-"
#define RKAMP_ALLOC_ALIGN_SIZE	64

#define ROUND_UP(s, x)	(DIV_ROUND_UP((s), (x)) * x)

struct rkamp_sharedmem {
	struct device *dev;
	void *virt_addr;
	struct gen_pool *pool;
	struct reserved_mem *rmem;
};

struct rkamp_sharedmem_device {
	struct rpmsg_driver *rpmsg_sd;
	struct rkamp_sharedmem sm;
	struct device *dev;
};

enum rpmsg_send_status {
	RPMSG_SEND_IDLE,
	RPMSG_SEND_BUSY,
	RPMSG_SEND_REPLIED,
};

struct rpmsg_sharedmem_device {
	struct rkamp_sharedmem_device *rksm_dev;
	struct device *dev;
	struct device chrdev;

	struct rkamp_sharedmem *sm;
	struct rkamp_sharedmem_node *node[RKAMP_SHAREDMEM_MAX_NODE];

	struct rpmsg_endpoint *ept;
	uint32_t remote_ept_id;

	/* For rpmsg sending protect */
	struct mutex lock;
	/* For sharedmem register protect */
	struct mutex reg_lock;
	/* For sharedmem node manager */
	struct mutex node_lock;
	wait_queue_head_t xfer_waitq;
	enum rpmsg_send_status status;
	struct rkamp_sharedmem_err_msg err_msg;
};

static bool rpmsg_send_check_status(struct rpmsg_sharedmem_device *rsm_dev,
				    enum rpmsg_send_status status)
{
	enum rpmsg_send_status now;

	mutex_lock(&rsm_dev->lock);
	now = rsm_dev->status;
	mutex_unlock(&rsm_dev->lock);

	return !!(now == status);
}

static void rpmsg_send_set_status(struct rpmsg_sharedmem_device *rsm_dev,
				  enum rpmsg_send_status status)
{
	mutex_lock(&rsm_dev->lock);
	rsm_dev->status = status;
	mutex_unlock(&rsm_dev->lock);
}

static int rpmsg_sharedmem_node_connected(struct rpmsg_sharedmem_device *rsm_dev,
					  struct rkamp_sharedmem_node *sm_node)
{
	struct rkamp_sharedmem_msg msg;
	int ret;

	msg.id = RKAMP_SHAREDMEM_MAMANGER_ID;
	msg.mmsg.command = RKAMP_SHAREDMEM_CMD_REGISTER;
	memcpy(msg.mmsg.register_msg.name, sm_node->name, sm_node->name_size);
	msg.mmsg.register_msg.name_size = sm_node->name_size;
	msg.mmsg.register_msg.sm_addr = sm_node->dma_pa;
	msg.mmsg.register_msg.sm_size = sm_node->size;

	mutex_lock(&rsm_dev->lock);
	if (rsm_dev->status != RPMSG_SEND_IDLE) {
		mutex_unlock(&rsm_dev->lock);
		return -EBUSY;
	}

	rsm_dev->status = RPMSG_SEND_BUSY;
	mutex_unlock(&rsm_dev->lock);

	ret = rpmsg_sendto(rsm_dev->ept, &msg, sizeof(msg),
			   rsm_dev->remote_ept_id);
	if (ret) {
		dev_err(rsm_dev->dev, "rpmsg_send failed\n");
		goto out;
	}

	ret = wait_event_interruptible_timeout(
			rsm_dev->xfer_waitq,
			rpmsg_send_check_status(rsm_dev, RPMSG_SEND_REPLIED),
			msecs_to_jiffies(1000));

	if (!ret)
		ret = -ETIMEDOUT;
	else if (ret > 0)
		ret = 0;

	if (ret) {
		dev_err(rsm_dev->dev, "can't get the replied\n");
		goto out;
	}

	ret = rsm_dev->err_msg.error;
	if (!ret)
		sm_node->id = rsm_dev->err_msg.msg[0];
	else
		dev_err(rsm_dev->dev,
			 "%s: failed to connect: %d\n", __func__, ret);
out:
	rpmsg_send_set_status(rsm_dev, RPMSG_SEND_IDLE);

	return ret;
}

static bool rpmsg_is_sharedmem_device(struct device *dev)
{
	if (memcmp(dev_name(dev), RKAMP_SHAREMEM_PREFIX,
		   sizeof(RKAMP_SHAREMEM_PREFIX) - 1)) {
		dev_info(dev, "dev_name(%s) is not rpmsg device: %s\n",
			 dev_name(dev), RKAMP_SHAREMEM_PREFIX);
		return false;
	} else {
		return true;
	}
}

static int rpmsg_sharedmem_get_new_node(struct rpmsg_sharedmem_device *rsm_dev)
{
	int i;
	int ret = -ENOMEM;

	mutex_lock(&rsm_dev->node_lock);
	for (i = 0; i < RKAMP_SHAREDMEM_MAX_NODE; i++) {
		if (!rsm_dev->node[i]) {
			ret = i;
			break;
		}
	}

	mutex_unlock(&rsm_dev->node_lock);
	return ret;
}

static int rpmsg_sharedmem_set_new_node(struct rpmsg_sharedmem_device *rsm_dev,
					struct rkamp_sharedmem_node *node,
					int num)
{
	if (num >= RKAMP_SHAREDMEM_MAX_NODE)
		return -EINVAL;

	mutex_lock(&rsm_dev->node_lock);
	rsm_dev->node[num] = node;
	mutex_unlock(&rsm_dev->node_lock);
	return 0;
}

static bool rpmsg_sharedmem_node_is_valid(struct rkamp_sharedmem_node *node)
{
	int i;
	struct rpmsg_sharedmem_device *rsm_dev = dev_get_drvdata(node->dev);
	bool ret = false;

	mutex_lock(&rsm_dev->node_lock);
	for (i = 0; i < RKAMP_SHAREDMEM_MAX_NODE; i++) {
		if (rsm_dev->node[i] == node) {
			ret = true;
			break;
		}
	}

	mutex_unlock(&rsm_dev->node_lock);
	return ret;
}

static void rpmsg_sharedmem_node_free(struct rpmsg_sharedmem_device *rsm_dev,
				      struct rkamp_sharedmem_node *node)
{
	gen_pool_free(rsm_dev->sm->pool, (unsigned long)node->dma_va,
		      node->size);
	kfree(node);
}
/*************************** SHAREMEM EXPORT *********************************/
struct device *rkamp_sharedmem_get_device(const char *name)
{
	return class_find_device_by_name(rpmsg_class, name);
}
EXPORT_SYMBOL(rkamp_sharedmem_get_device);

struct rkamp_sharedmem_node *rkamp_sharedmem_register(struct device *dev,
			int size, char *name, int name_size,
			void (*notify)(void *priv, void *payload, int len),
			void *priv)
{
	struct rpmsg_sharedmem_device *rsm_dev;
	struct rkamp_sharedmem_node *sm_node, *ept;
	struct rkamp_sharedmem *sm;
	int new;

	if (!rpmsg_is_sharedmem_device(dev) || size <= 0 || !name ||
	    name_size <= 0 || name_size > RKAMP_SHAREDMEM_NODE_NAME_LEN)
		return ERR_PTR(-EINVAL);

	rsm_dev = dev_get_drvdata(dev);
	sm = rsm_dev->sm;

	mutex_lock(&rsm_dev->reg_lock);

	new = rpmsg_sharedmem_get_new_node(rsm_dev);
	mutex_unlock(&rsm_dev->node_lock);
	if (new < 0) {
		dev_err(rsm_dev->dev, "No space for new node\n");
		ept = ERR_PTR(-ENOMEM);
		goto err;
	}

	size = ROUND_UP(size, RKAMP_ALLOC_ALIGN_SIZE);
	sm_node = kzalloc(sizeof(*sm_node), GFP_KERNEL);
	if (!sm_node) {
		ept = ERR_PTR(-ENOMEM);
		goto err;
	}

	sm_node->dma_va = (void *)gen_pool_alloc(sm->pool, size);
	if (!sm_node->dma_va) {
		dev_err(dev, "failed to alloc dma memory\n");
		ept = ERR_PTR(-ENOMEM);
		goto err_pool;
	}

	sm_node->dma_pa = gen_pool_virt_to_phys(sm->pool,
						(unsigned long)sm_node->dma_va);

	memcpy(sm_node->name, name, name_size);
	sm_node->name_size = name_size;
	sm_node->notify = notify;
	sm_node->priv = priv;
	sm_node->size = size;
	sm_node->dev = dev;

	if (rpmsg_sharedmem_node_connected(rsm_dev, sm_node)) {
		dev_err(dev, "failed to connect to device\n");
		ept = ERR_PTR(-EPROTO);
		goto err_connect;
	}

	rpmsg_sharedmem_set_new_node(rsm_dev, sm_node, new);
	dev_info(dev, "connected to device: %s\n", name);
	mutex_unlock(&rsm_dev->reg_lock);
	return sm_node;
err_connect:
	gen_pool_free(sm->pool, (unsigned long)sm_node->dma_va, sm_node->size);
err_pool:
	kfree(sm_node);
err:
	mutex_unlock(&rsm_dev->reg_lock);
	return ept;
}
EXPORT_SYMBOL(rkamp_sharedmem_register);

int rkamp_sharedmem_unregister(struct rkamp_sharedmem_node *node)
{
	struct rpmsg_sharedmem_device *rsm_dev;
	int i;

	if (!node)
		return -EINVAL;

	if (!rpmsg_sharedmem_node_is_valid(node)) {
		dev_err(node->dev, "%s: node is not valid\n", __func__);
		return -EINVAL;
	}

	rsm_dev = dev_get_drvdata(node->dev);

	mutex_lock(&rsm_dev->node_lock);
	for (i = 0; i < RKAMP_SHAREDMEM_MAX_NODE; i++) {
		if (rsm_dev->node[i] == node) {
			rsm_dev->node[i] = NULL;
			break;
		}
	}
	rpmsg_sharedmem_node_free(rsm_dev, node);
	mutex_unlock(&rsm_dev->node_lock);

	return 0;
}
EXPORT_SYMBOL(rkamp_sharedmem_unregister);

static void rkamp_sharedmem_dma_sync(struct rkamp_sharedmem_node *node,
				     int offset, int size, bool to_device)
{
	struct rpmsg_sharedmem_device *rsm_dev;
	int len;

	if (size < 0 || !node)
		return;

	if (!rpmsg_sharedmem_node_is_valid(node)) {
		dev_info(node->dev, "%s: node is not valid\n", __func__);
		return;
	}

	rsm_dev = dev_get_drvdata(node->dev);
	if (offset > node->size) {
		dev_info(rsm_dev->dev, "Invalid offset %d greater then size %lld\n",
			 offset, node->size);
		return;
	}

	len = node->size - offset;
	len = size < len ? size : len;
	if (to_device)
		dma_sync_single_for_device(rsm_dev->sm->dev,
					   node->dma_pa + offset,
					   len, DMA_TO_DEVICE);
	else
		dma_sync_single_for_cpu(rsm_dev->sm->dev,
					node->dma_pa + offset,
					len, DMA_FROM_DEVICE);
}

void rkamp_sharedmem_flush(struct rkamp_sharedmem_node *node,
			   int offset, int size)
{
	rkamp_sharedmem_dma_sync(node, offset, size, true);
}
EXPORT_SYMBOL(rkamp_sharedmem_flush);

void rkamp_sharedmem_invalidate(struct rkamp_sharedmem_node *node,
				int offset, int size)
{
	rkamp_sharedmem_dma_sync(node, offset, size, false);
}
EXPORT_SYMBOL(rkamp_sharedmem_invalidate);

int rkamp_sharedmem_send_notify(struct rkamp_sharedmem_node *node,
				void *payload, int len)
{
	struct rkamp_sharedmem_msg msg;
	struct rpmsg_sharedmem_device *rsm_dev;
	int ret;

	if (len <= 0 || !payload || !node || len > RKAMP_SHAREDMEM_MSG_DATA_LEN)
		return -EINVAL;

	if (!rpmsg_sharedmem_node_is_valid(node)) {
		dev_info(node->dev, "%s: node is not valid\n", __func__);
		return -ENODEV;
	}

	rsm_dev = dev_get_drvdata(node->dev);
	if (!node->id) {
		dev_warn(rsm_dev->dev, "node can't be 0 !!!!");
		return -EINVAL;
	}

	msg.id = node->id;
	memcpy(msg.data, payload, len);

	ret = rpmsg_sendto(rsm_dev->ept, &msg, len + 4, rsm_dev->remote_ept_id);
	if (ret) {
		dev_err(rsm_dev->dev, "rpmsg_send failed\n");
		return -EREMOTE;
	}
	return 0;
}
EXPORT_SYMBOL(rkamp_sharedmem_send_notify);

/***************************** RPMSG PROBE ***********************************/
static int rpmsg_sharedmem_probe(struct rpmsg_device *rdev)
{
	struct rpmsg_sharedmem_device *rsm_dev;
	struct device *dev = &rdev->dev;
	int ret;

	rsm_dev = devm_kzalloc(&rdev->dev, sizeof(*rsm_dev), GFP_KERNEL);
	if (!rsm_dev)
		return -ENOMEM;

	rsm_dev->dev = dev;
	rsm_dev->ept = rdev->ept;
	rsm_dev->remote_ept_id = rdev->dst;
	mutex_init(&rsm_dev->lock);
	mutex_init(&rsm_dev->reg_lock);
	mutex_init(&rsm_dev->node_lock);
	init_waitqueue_head(&rsm_dev->xfer_waitq);

	rsm_dev->rksm_dev = (void *)(rdev->id.driver_data);
	rsm_dev->sm = &(rsm_dev->rksm_dev->sm);

	dev_info(dev, "phys_addr %pa virt_addr %p | size = 0x%llx\n",
		      &rsm_dev->sm->rmem->base, rsm_dev->sm->virt_addr,
		      (unsigned long long)rsm_dev->sm->rmem->size);

	rsm_dev->chrdev.parent = dev;
	rsm_dev->chrdev.class = rpmsg_class;
	dev_set_name(&rsm_dev->chrdev, "%s", rdev->id.name);
	ret = device_register(&rsm_dev->chrdev);
	if (ret) {
		dev_err(dev, "failed to register child device\n");
		return -ENODEV;
	}
	dev_set_drvdata(&rsm_dev->chrdev, rsm_dev);
	dev_set_drvdata(dev, rsm_dev);

	rdev->announce = rdev->src != RPMSG_ADDR_ANY;

	dev_info(dev, "rpmsg sharedmem is ready!\n");
	return 0;
}

static void rpmsg_sharedmem_remove(struct rpmsg_device *rdev)
{
	int i;
	struct rpmsg_sharedmem_device *rsm_dev = dev_get_drvdata(&rdev->dev);

	mutex_lock(&rsm_dev->node_lock);
	for (i = 0; i < RKAMP_SHAREDMEM_MAX_NODE; i++) {
		if (rsm_dev->node[i]) {
			rpmsg_sharedmem_node_free(rsm_dev, rsm_dev->node[i]);
			rsm_dev->node[i] = NULL;
		}
	}
	mutex_unlock(&rsm_dev->node_lock);
}

static struct rkamp_sharedmem_node *
rpmsg_sharedmem_find_node_by_id(struct rpmsg_sharedmem_device *rsm_dev, int id)
{
	int i;
	struct rkamp_sharedmem_node *node;

	for (i = 0; i < RKAMP_SHAREDMEM_MAX_NODE; i++) {
		if (!rsm_dev->node[i]) {
			node = NULL;
			continue;
		}
		if (rsm_dev->node[i]->id == id) {
			node = rsm_dev->node[i];
			goto out;
		}
	}

	node = NULL;
out:
	return node;
}

static int rpmsg_sharedmem_recv_cb(struct rpmsg_device *rp, void *payload,
				   int payload_len, void *priv, u32 src)
{
	struct rkamp_sharedmem_msg *msg = payload;
	struct rpmsg_sharedmem_device *rsm_dev = dev_get_drvdata(&rp->dev);
	struct rkamp_sharedmem_node *node;
	uint32_t len;

	if (msg->id) {
		mutex_lock(&rsm_dev->node_lock);
		node = rpmsg_sharedmem_find_node_by_id(rsm_dev, msg->id);
		if (!node) {
			dev_err(rsm_dev->dev, "failed to find node, id = %d",
				msg->id);
			mutex_unlock(&rsm_dev->node_lock);
			return -ENODEV;
		}

		if (node->notify)
			node->notify(node->priv, msg->data, payload_len - 4);
		mutex_unlock(&rsm_dev->node_lock);
		return 0;
	}

	if (msg->mmsg.command == RKAMP_SHAREDMEM_CMD_ERROR) {
		if (rpmsg_send_check_status(rsm_dev, RPMSG_SEND_BUSY)) {
			len = RKAMP_SHM_ERROR_HEAD + msg->mmsg.err_msg.len;
			if (len > sizeof(struct rkamp_sharedmem_err_msg)) {
				dev_warn(rsm_dev->dev, "wrong message length\n");
				len = sizeof(struct rkamp_sharedmem_err_msg);
			}
			memcpy(&rsm_dev->err_msg, &msg->mmsg.err_msg, len);
			rpmsg_send_set_status(rsm_dev, RPMSG_SEND_REPLIED);
			wake_up_interruptible(&rsm_dev->xfer_waitq);
		} else {
			dev_err(rsm_dev->dev, "STATUS is not RKAMP_SEND_REPLIED\n");
			return -EINVAL;
		}

		return 0;
	}

	dev_err(rsm_dev->dev, "unknown manager msg with command = 0x%x\n",
			      msg->mmsg.command);
	return -EINVAL;
}

/**************************** PLATFORM PROBE **********************************/
static int rkamp_sharedmem_probe(struct platform_device *pdev)
{
	struct rpmsg_driver *rpmsg_sd;
	struct rpmsg_device_id *rpmsg_sd_id;
	struct rkamp_sharedmem_device *rksm_dev;
	struct rkamp_sharedmem *sm;
	struct device *dev = &pdev->dev;
	struct device_node *mnp, *np = dev->of_node;
	const char *channel_name;
	int ret;

	rpmsg_sd_id = devm_kzalloc(dev, sizeof(struct rpmsg_device_id),
				   GFP_KERNEL);
	if (!rpmsg_sd_id)
		return -ENOMEM;

	rpmsg_sd = devm_kzalloc(dev, sizeof(struct rpmsg_driver), GFP_KERNEL);
	if (!rpmsg_sd)
		return -ENOMEM;

	rksm_dev = devm_kzalloc(dev, sizeof(struct rkamp_sharedmem_device),
				GFP_KERNEL);
	if (!rksm_dev)
		return -ENOMEM;

	rpmsg_sd->drv.name = KBUILD_MODNAME;
	rpmsg_sd->probe = rpmsg_sharedmem_probe;
	rpmsg_sd->callback = rpmsg_sharedmem_recv_cb;
	rpmsg_sd->remove = rpmsg_sharedmem_remove;

	ret = of_property_read_string(np, "channel", &channel_name);
	if (ret < 0) {
		dev_err(dev, "failed to read channel name\n");
		return ret;
	}

	ret = snprintf(rpmsg_sd_id->name, RPMSG_NAME_SIZE, "%s%s",
		       RKAMP_SHAREMEM_PREFIX, channel_name);
	if (ret == RPMSG_NAME_SIZE)
		dev_warn(dev, "channel name too long: %s\n",
			 rpmsg_sd_id->name);
	rpmsg_sd_id->driver_data = (kernel_ulong_t)rksm_dev;
	rpmsg_sd->id_table = rpmsg_sd_id;

	rksm_dev->rpmsg_sd = rpmsg_sd;
	sm = &rksm_dev->sm;

	mnp = of_parse_phandle(np, "memory-region", 0);
	if (!mnp) {
		dev_info(dev, "No memory-region found\n");
		return -EINVAL;
	}

	sm->rmem = of_reserved_mem_lookup(mnp);
	if (!sm->rmem) {
		dev_err(dev, "Failed to get reserved memory from DT\n");
		return -ENODEV;
	}

	sm->pool = devm_gen_pool_create(dev, ilog2(RKAMP_ALLOC_ALIGN_SIZE),
					-1, dev_name(dev));
	if (IS_ERR(sm->pool)) {
		dev_err(dev, "Failed to create gen_pool\n");
		return PTR_ERR(sm->pool);
	}

	sm->virt_addr = devm_memremap(dev, sm->rmem->base, sm->rmem->size,
				      MEMREMAP_WB);
	if (IS_ERR(sm->virt_addr)) {
		dev_err(dev, "failed to map memory pool\n");
		return PTR_ERR(sm->virt_addr);
	}

	ret = gen_pool_add_virt(sm->pool, (unsigned long)sm->virt_addr,
				sm->rmem->base, sm->rmem->size, -1);
	if (ret < 0) {
		dev_err(dev, "Failed to add memory to pool: %d\n", ret);
		return ret;
	}

	dev_info(dev, "phys_addr %pa virt_addr %p | size = 0x%llx\n",
		      &sm->rmem->base, sm->virt_addr,
		      (unsigned long long)sm->rmem->size);
	sm->dev = dev;
	rksm_dev->dev = dev;

	platform_set_drvdata(pdev, rksm_dev);

	ret = register_rpmsg_driver(rpmsg_sd);
	if (!ret) {
		dev_info(dev, "registered rpmsg channel: %s\n",
			 rpmsg_sd_id->name);
	} else {
		dev_err(dev, "failed to register rpmsg channel: %s\n",
			rpmsg_sd_id->name);
	}

	return ret;
}

static int rkamp_sharedmem_remove(struct platform_device *pdev)
{
	unregister_rpmsg_driver(platform_get_drvdata(pdev));

	return 0;
}

static const struct of_device_id rkamp_sharedmem_dt_match[] = {
	{ .compatible = "rockchip,amp-sharedmem" },
	{ },
};
MODULE_DEVICE_TABLE(of, rkamp_sharedmem_dt_match);

static struct platform_driver rkamp_sharedmem_driver = {
	.probe		= rkamp_sharedmem_probe,
	.remove		= rkamp_sharedmem_remove,
	.driver		= {
		.name	= "rockchip-amp-sharedmem",
		.of_match_table = rkamp_sharedmem_dt_match,
	},
};

module_platform_driver(rkamp_sharedmem_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zain Wang");
MODULE_DESCRIPTION("Rockchip AMP Shared Memory Manager");
