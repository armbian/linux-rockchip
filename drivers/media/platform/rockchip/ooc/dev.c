// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip driver for IRFPA
 *
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X00 first version.
 */
#include <linux/io.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/rk-camera-module.h>
#include <media/videobuf2-cma-sg.h>
#include "rkooc-externel.h"
#include "dev.h"
#include "regs.h"

#define RKOOC_MODULE_NAME "rkooc"
#define DRIVER_VERSION	KERNEL_VERSION(0, 0x01, 0x00)

static irqreturn_t rkooc_irq_handler(int irq, void *ctx)
{
	u32 value, status;
	unsigned long flags;
	struct v4l2_subdev *ooc_sd = dev_get_drvdata(ctx);
	struct rkooc_dev *dev = container_of(ooc_sd, struct rkooc_dev, ooc_sd);

	status = rkooc_read_reg(dev, OOC_INTR_STATUS);

	if (status & (1 << WIN1_EMPTY_INTR_STS_OFF))
		v4l2_err(&dev->v4l2_dev, "win1 empty!\n");
	if (status & (1 << PDAF_EMPTY_INTR_STS_OFF))
		v4l2_err(&dev->v4l2_dev, "pdaf empty!\n");

	if (status & (1 << VP_INTR_STS_OFF)) {
		dev->ooctx_num++;

		spin_lock_irqsave(&dev->slock, flags);
		if (!list_empty(&dev->vid_out_active)) {
			struct rkooc_buffer *buf =
			    list_entry(dev->vid_out_active.next,
				       struct rkooc_buffer,
				       list);

			list_del(&buf->list);
			spin_unlock_irqrestore(&dev->slock, flags);

			if (dev->cur_buf) {
				vb2_buffer_done(&dev->cur_buf->vb.vb2_buf,
						VB2_BUF_STATE_DONE);
			}

			dev->cur_buf = buf;
			rkooc_hw_update_win_addr(dev, (u32) buf->dma_addr);
		} else {
			spin_unlock_irqrestore(&dev->slock, flags);
		}
	}

	value = status | 0xffff0000;
	rkooc_write_reg(dev, OOC_INTR_CLEAR, value);

	return IRQ_HANDLED;
}

static void rkooc_dev_release(struct v4l2_device *v4l2_dev)
{
	struct rkooc_dev *dev =
	    container_of(v4l2_dev, struct rkooc_dev, v4l2_dev);
	v4l2_device_unregister(&dev->v4l2_dev);
#ifdef CONFIG_MEDIA_CONTROLLER
	media_device_cleanup(&dev->mdev);
#endif
}

static int rkooc_drv_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct rkooc_dev *dev = NULL;
	struct resource *res;

	dev_info(&pdev->dev, "rkooc driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8, DRIVER_VERSION & 0x00ff);
	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	/* use rk cma for buffer memory alloc */
	dev->mem_ops = &vb2_cma_sg_memops;
	dev->have_dummy = false;

	/* set drvdata to struct rkooc_dev */
	platform_set_drvdata(pdev, &dev->ooc_sd);

	/* get ooc register resource */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "get resource failed\n");
		return -EINVAL;
	}

	/* remap ooc register base */
	dev->ooc_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dev->ooc_base)) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = PTR_ERR(dev->ooc_base);
		return ret;
	}

	/* initialize locks */
	spin_lock_init(&dev->slock);
	spin_lock_init(&dev->irfpa_lock);
	mutex_init(&dev->mutex);

	/* setup initial size */
	dev->irfpatx_fourcc = V4L2_PIX_FMT_NV12;

#ifdef CONFIG_MEDIA_CONTROLLER
	dev->v4l2_dev.mdev = &dev->mdev;

	/* Initialize media device */
	strscpy(dev->mdev.model, RKOOC_MODULE_NAME, sizeof(dev->mdev.model));
	snprintf(dev->mdev.bus_info, sizeof(dev->mdev.bus_info),
		 "platform:%s", RKOOC_MODULE_NAME);
	dev->mdev.dev = &pdev->dev;
	media_device_init(&dev->mdev);
	//dev->mdev.ops = &vivid_media_ops;
#endif

	dev->pmclk = devm_clk_get(&pdev->dev, "pmclk");
	if (IS_ERR(dev->pmclk)) {
		dev_err(&pdev->dev, "Failed to get pmclk\n");
		return -EINVAL;
	}
	dev_info(&pdev->dev, "pmclk rate %lu", clk_get_rate(dev->pmclk));

	dev->hclk = devm_clk_get_enabled(&pdev->dev, "hclk");
	if (IS_ERR(dev->hclk))
		dev_err(&pdev->dev, "enable hclk failed");

	dev->aclk = devm_clk_get_enabled(&pdev->dev, "aclk");
	if (IS_ERR(dev->aclk))
		dev_err(&pdev->dev, "enable aclk failed");

	dev_info(&pdev->dev, "rkooc hw version %x",
		 readl(dev->ooc_base + OOC_VERSION));

	/* initialize ooc registers */
	rkooc_hw_init(dev);

	dev->irq = platform_get_irq(pdev, 0);
	if (dev->irq < 0)
		return dev->irq;

	ret = devm_request_irq(&pdev->dev, dev->irq, rkooc_irq_handler,
			       IRQF_SHARED, dev_driver_string(&pdev->dev),
			       &pdev->dev);
	if (ret < 0)
		return ret;

	/* register v4l2_device */
	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name),
		 "%s", RKOOC_MODULE_NAME);
	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (ret)
		return ret;

	dev->v4l2_dev.release = rkooc_dev_release;

	/* create all devs */
	ret = rkooc_create_ooc_dev(dev);
	if (ret)
		goto err_unreg_v4l2_dev;
	ret = rkooc_create_cap_dev(dev);
	if (ret)
		goto err_remove_ooc_dev;
	ret = rkooc_create_rx_dev(dev);
	if (ret)
		goto err_remove_cap_dev;
	ret = rkooc_register_ooc_subdev(dev);
	if (ret)
		goto err_remove_rx_dev;
	ret = v4l2_device_register_subdev_nodes(&dev->v4l2_dev);
	if (ret)
		goto err_unregister_ooc_subdev;

#ifdef CONFIG_MEDIA_CONTROLLER
	/* Register the media device */
	ret = media_device_register(&dev->mdev);
	if (ret) {
		dev_err(dev->mdev.dev,
			"media device register failed (err=%d)\n", ret);
		goto err_unregister_ooc_subdev;
	}
#endif
	return 0;

err_unregister_ooc_subdev:
	rkooc_unregister_ooc_subdev(dev);
err_remove_rx_dev:
	rkooc_remove_rx_dev(dev);
err_remove_cap_dev:
	rkooc_remove_cap_dev(dev);
err_remove_ooc_dev:
	rkooc_remove_ooc_dev(dev);
err_unreg_v4l2_dev:
	v4l2_device_unregister(&dev->v4l2_dev);
	return ret;
}

static int rkooc_drv_remove(struct platform_device *pdev)
{
	struct v4l2_subdev *ooc_sd = platform_get_drvdata(pdev);
	struct rkooc_dev *dev = container_of(ooc_sd, struct rkooc_dev, ooc_sd);

#ifdef CONFIG_MEDIA_CONTROLLER
	media_device_unregister(&dev->mdev);
#endif
	rkooc_hw_pmclk_disable(dev);
	rkooc_hw_deinit(dev);

	rkooc_remove_cap_dev(dev);
	rkooc_remove_ooc_dev(dev);
	rkooc_remove_rx_dev(dev);
	rkooc_unregister_ooc_subdev(dev);
	v4l2_device_unregister(&dev->v4l2_dev);
	return 0;
}

static const struct of_device_id rkooc_of_table[] = {
	{.compatible = "rockchip,rkooc-tx" },
	{ },
};

MODULE_DEVICE_TABLE(of, rkooc_of_table);

static struct platform_driver rkooc_driver = {
	.driver = {
		   .name = RKOOC_MODULE_NAME,
		   .of_match_table = of_match_ptr(rkooc_of_table),
		    },
	.probe = rkooc_drv_probe,
	.remove = rkooc_drv_remove,
};

module_platform_driver(rkooc_driver);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rockchip-rkooc");
MODULE_AUTHOR("ROCKCHIP");
