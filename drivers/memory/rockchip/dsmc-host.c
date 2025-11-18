// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>

#include "dsmc-host.h"
#include "dsmc-lb-slave.h"

/* DMA translate state flags */
#define RXDMA					(1 << 0)
#define TXDMA					(1 << 1)

#ifndef FALSE
#define FALSE					0
#endif

#ifndef TRUE
#define TRUE					(1)
#endif

static __maybe_unused int rk3576_dsmc_platform_init(struct platform_device *pdev)
{
	uint32_t i, val;
	struct rockchip_dsmc_device *priv;

	priv = platform_get_drvdata(pdev);

	priv->dsmc.cfg.dma_req_mux_offset = 0x0;

	if (IS_ERR_OR_NULL(priv->dsmc.grf)) {
		dev_err(priv->dsmc.dev, "Missing rockchip,grf property\n");
		return -ENODEV;
	}

	for (i = RK3576_GPIO3A_IOMUX_SEL_H; i <= RK3576_GPIO4A_IOMUX_SEL_L; i += 4) {
		if (i == RK3576_GPIO4A_IOMUX_SEL_L)
			val = RK3576_IOMUX_SEL(0x5, 4) |
			      RK3576_IOMUX_SEL(0x5, 0);
		else
			val = RK3576_IOMUX_SEL(0x5, 12) |
			      RK3576_IOMUX_SEL(0x5, 8) |
			      RK3576_IOMUX_SEL(0x5, 4) |
			      RK3576_IOMUX_SEL(0x5, 0);
		regmap_write(priv->dsmc.grf, i, val);
	}

	return 0;
}

static __maybe_unused int rk3506_dsmc_platform_init(struct platform_device *pdev)
{
	int ret = 0;
	uint32_t i;
	struct device *dev = &pdev->dev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_active;
	struct pinctrl_state *pinctrl_lb_slave;
	const struct rockchip_dsmc_device *priv;

	priv = platform_get_drvdata(pdev);

	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pinctrl)) {
		dev_err(dev, "Failed to get pinctrl\n");
		return PTR_ERR(pinctrl);
	}

	pinctrl_active = pinctrl_lookup_state(pinctrl, "active");
	if (IS_ERR(pinctrl_active)) {
		dev_err(dev, "Failed to lookup active pinctrl state\n");
		return PTR_ERR(pinctrl_active);
	}

	ret = pinctrl_select_state(pinctrl, pinctrl_active);
	if (ret) {
		dev_err(dev, "Failed to select active pinctrl state\n");
		return ret;
	}

	pinctrl_lb_slave = pinctrl_lookup_state(pinctrl, "lb-slave");
	if (IS_ERR(pinctrl_lb_slave)) {
		dev_err(dev, "Failed to lookup lb-slave pinctrl state\n");
		return PTR_ERR(pinctrl_lb_slave);
	}

	for (i = 0; i < DSMC_MAX_SLAVE_NUM; i++) {
		if (priv->dsmc.cfg.cs_cfg[i].device_type == DSMC_LB_DEVICE) {
			ret = pinctrl_select_state(pinctrl, pinctrl_lb_slave);
			if (ret) {
				dev_err(dev, "Failed to select lb-slave pinctrl state\n");
				return ret;
			}
			break;
		}
	}

	return ret;
}

static const struct of_device_id dsmc_of_match[] = {
#if IS_ENABLED(CONFIG_CPU_RK3576)
	{
		.compatible = "rockchip,rk3576-dsmc", .data = rk3576_dsmc_platform_init
	},
#endif
#if IS_ENABLED(CONFIG_CPU_RK3506)
	{
		.compatible = "rockchip,rk3506-dsmc", .data = rk3506_dsmc_platform_init
	},
#endif
#if IS_ENABLED(CONFIG_CPU_RV1126B)
	{
		.compatible = "rockchip,rv1126b-dsmc", .data = rk3506_dsmc_platform_init
	},
#endif
	{},
};
MODULE_DEVICE_TABLE(of, dsmc_of_match);

static int rockchip_dsmc_platform_init(struct platform_device *pdev)
{
	const struct of_device_id *match;
	int (*init)(struct platform_device *pdev);
	int ret;

	match = of_match_node(dsmc_of_match, pdev->dev.of_node);
	if (match) {
		init = match->data;
		if (init) {
			ret = init(pdev);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static void *rk_dsmc_map_kernel(phys_addr_t start, size_t len, uint32_t mem_attr)
{
	void *vaddr;

	if (mem_attr == DSMC_MEM_ATTRIBUTE_CACHE)
		vaddr = ioremap_cache(start, len);
	else if (mem_attr == DSMC_MEM_ATTRIBUTE_WR_COM)
		vaddr = ioremap_wc(start, len);
	else
		vaddr = ioremap(start, len);

	return vaddr;
}

static void rk_dsmc_unmap_kernel(void *vaddr)
{
	if (vaddr != NULL)
		iounmap(vaddr);
}

static int dsmc_parse_dt_regions(struct platform_device *pdev, struct device_node *lb_np,
				 struct dsmc_config_cs *cfg)
{
	int i;
	int ret = 0;
	char region_name[16];
	struct device_node *region_node, *child_node;
	struct device *dev = &pdev->dev;
	struct regions_config *rgn;
	const char *attribute;

	region_node = of_get_child_by_name(lb_np, "region");
	if (!region_node) {
		dev_err(dev, "Failed to get dsmc_slave node\n");
		return -ENODEV;
	}

	for (i = 0; i < DSMC_LB_MAX_RGN; i++) {
		rgn = &cfg->slv_rgn[i];
		snprintf(region_name, sizeof(region_name), "region%d", i);
		child_node = of_get_child_by_name(region_node, region_name);
		if (child_node) {
			ret = of_property_read_string(child_node, "rockchip,attribute",
						&attribute);
			if (ret) {
				dev_err(dev, "Failed to read region %d rockchip,attribute!\n", i);
				ret = -ENODEV;
				goto release_region_node;
			}
			if (strcmp(attribute, "Merged FIFO") == 0) {
				rgn->attribute = RGNX_ATTR_MERGE_FIFO;
			} else if (strcmp(attribute, "No-Merge FIFO") == 0) {
				rgn->attribute = RGNX_ATTR_NO_MERGE_FIFO;
			} else if (strcmp(attribute, "DPRA") == 0) {
				rgn->attribute = RGNX_ATTR_DPRA;
			} else if (strcmp(attribute, "Register") == 0) {
				rgn->attribute = RGNX_ATTR_REG;
			} else {
				dev_err(dev, "Region %d unknown attribute: %s\n",
					i, attribute);
				ret = -ENODEV;
				of_node_put(child_node);
				goto release_region_node;
			}

			if (of_property_read_u32(child_node, "rockchip,ca-addr-width",
						&rgn->ca_addr_width)) {
				dev_err(dev, "Failed to read rockchip,ca-addr-width!\n");
				ret = -ENODEV;
				of_node_put(child_node);
				goto release_region_node;
			}
			if (of_property_read_u32(child_node, "rockchip,dummy-clk-num",
						&rgn->dummy_clk_num)) {
				dev_err(dev, "Failed to read rockchip,dummy-clk-num!\n");
				ret = -ENODEV;
				of_node_put(child_node);
				goto release_region_node;
			}
			rgn->dummy_clk_num--;

			if (of_property_read_u32(child_node, "rockchip,cs0-be-ctrled",
						&rgn->cs0_be_ctrled)) {
				dev_err(dev, "Failed to read rockchip,cs0-be-ctrled!\n");
				ret = -ENODEV;
				of_node_put(child_node);
				goto release_region_node;
			}

			if (of_property_read_u32(child_node, "rockchip,cs0-ctrl",
						&rgn->cs0_ctrl)) {
				dev_err(dev, "Failed to read rockchip,cs0-ctrl!\n");
				ret = -ENODEV;
				of_node_put(child_node);
				goto release_region_node;
			}
			rgn->status = of_device_is_available(child_node);
			if (rgn->status)
				cfg->rgn_num++;
			of_node_put(child_node);
		}
	}

release_region_node:
	of_node_put(region_node);

	return ret;
}

static int dsmc_reg_remap(struct device *dev, struct dsmc_ctrl_config *cfg,
			  struct rockchip_dsmc *dsmc, uint64_t *mem_ranges,
			  uint32_t *dqs_dll)
{
	int ret = 0;
	uint32_t cs, rgn, rgn_num_max;
	uint32_t num = 0;
	struct dsmc_map *region_map;

	rgn_num_max = 1;
	for (cs = 0; cs < DSMC_MAX_SLAVE_NUM; cs++) {
		if (cfg->cs_cfg[cs].device_type == DSMC_UNKNOWN_DEVICE)
			continue;
		if (cfg->cs_cfg[cs].rgn_num == 3)
			cfg->cs_cfg[cs].rgn_num++;
		rgn_num_max = max_t(uint32_t, rgn_num_max, cfg->cs_cfg[cs].rgn_num);
	}

	for (cs = 0; cs < DSMC_MAX_SLAVE_NUM; cs++) {
		if (cfg->cs_cfg[cs].device_type == OPI_XCCELA_PSRAM) {
			region_map = &dsmc->cs_map[cs].region_map[0];
			region_map->phys = (phys_addr_t)(mem_ranges[0] + cs * mem_ranges[1]);
			region_map->size = (size_t)mem_ranges[1];
			cfg->cap = max_t(uint32_t, cfg->cap, region_map->size);
			region_map->virt = rk_dsmc_map_kernel(region_map->phys,
							      region_map->size,
							      DSMC_MEM_ATTRIBUTE_NO_CACHE);
		} else if (cfg->cs_cfg[cs].device_type == DSMC_LB_DEVICE) {
			num = 0;
			for (rgn = 0; rgn < DSMC_LB_MAX_RGN; rgn++) {
				if (!cfg->cs_cfg[cs].slv_rgn[rgn].status)
					continue;
				region_map = &dsmc->cs_map[cs].region_map[rgn];
				region_map->phys = (phys_addr_t)(mem_ranges[0] +
						   rgn_num_max * mem_ranges[1] * cs +
						   num * mem_ranges[1]);
				region_map->size = (size_t)mem_ranges[1];
				cfg->cap = max_t(uint32_t, cfg->cap, region_map->size);
				num++;
				region_map->virt = rk_dsmc_map_kernel(region_map->phys,
						   region_map->size,
						   DSMC_MEM_ATTRIBUTE_NO_CACHE);
				if (!region_map->virt) {
					dev_err(dev, "Failed to remap slave cs%d memory\n",
						cs);
					ret = -EINVAL;
				}
			}
		}
	}
	cfg->cap *= rgn_num_max;

	return ret;
}

static int dsmc_mem_remap(struct device *dev, struct rockchip_dsmc *dsmc)
{
	int ret = 0;
	uint32_t cs, i;
	uint32_t mem_attr;
	struct dsmc_map *region_map;
	struct dsmc_ctrl_config *cfg = &dsmc->cfg;
	struct regions_config *slv_rgn;

	for (cs = 0; cs < DSMC_MAX_SLAVE_NUM; cs++) {
		if (cfg->cs_cfg[cs].device_type == DSMC_UNKNOWN_DEVICE)
			continue;
		region_map = &dsmc->cs_map[cs].region_map[0];
		/* unremap the register space */
		if (region_map->virt) {
			rk_dsmc_unmap_kernel(region_map->virt);
			region_map->virt = NULL;
		}

		if (cfg->cs_cfg[cs].device_type == DSMC_LB_DEVICE) {
			for (i = 0; i < DSMC_LB_MAX_RGN; i++) {
				region_map = &dsmc->cs_map[cs].region_map[i];
				slv_rgn = &cfg->cs_cfg[cs].slv_rgn[i];
				if (!slv_rgn->status)
					continue;
				if (slv_rgn->attribute == RGNX_ATTR_REG)
					mem_attr = DSMC_MEM_ATTRIBUTE_NO_CACHE;
				else
					mem_attr = DSMC_MEM_ATTRIBUTE_CACHE;

				region_map->virt = rk_dsmc_map_kernel(region_map->phys,
								      region_map->size,
								      mem_attr);
				if (!region_map->virt) {
					dev_err(dev, "Failed to remap slave cs%d memory\n", cs);
					ret = -EINVAL;
					continue;
				}
			}
			if (rockchip_dsmc_register_lb_device(dev, cs)) {
				dev_err(dev, "Failed to register local bus device\n");
				ret = -EINVAL;
				return ret;
			}

		} else {
			region_map = &dsmc->cs_map[cs].region_map[0];
			region_map->virt = rk_dsmc_map_kernel(region_map->phys,
							      region_map->size,
							      DSMC_MEM_ATTRIBUTE_CACHE);
			if (!region_map->virt) {
				dev_err(dev, "Failed to remap psram cs%d memory\n", cs);
				ret = -EINVAL;
			}
		}
	}

	return ret;
}

static int dsmc_parse_dt(struct platform_device *pdev, struct rockchip_dsmc *dsmc)
{
	int ret = 0;
	uint32_t io_width_val;
	uint32_t cs;
	uint32_t psram = 0, lb_slave = 0;
	uint64_t mem_ranges[2];
	uint32_t dqs_dll[2 * DSMC_MAX_SLAVE_NUM];
	uint32_t mtr_timing[8];
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child_node;
	struct device_node *slave_np, *dsmc_slave_np;
	struct device_node *psram_np = NULL, *lb_slave_np = NULL;
	struct dsmc_ctrl_config *cfg = &dsmc->cfg;
	char slave_name[16];

	slave_np = of_get_child_by_name(np, "slave");
	if (!slave_np) {
		dev_err(dev, "Failed to find slave node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32_array(slave_np, "rockchip,dqs-dll", dqs_dll,
					 ARRAY_SIZE(dqs_dll));
	if (ret) {
		dev_err(dev, "Failed to read rockchip,dqs-dll!\n");
		ret = -ENODEV;
		goto release_slave_node;
	}

	for (cs = 0; cs < DSMC_MAX_SLAVE_NUM; cs++) {
		cfg->cs_cfg[cs].dll_num[0] = dqs_dll[2 * cs];
		cfg->cs_cfg[cs].dll_num[1] = dqs_dll[2 * cs + 1];
	}

	ret = of_property_read_u64_array(slave_np, "rockchip,ranges",
					 mem_ranges, ARRAY_SIZE(mem_ranges));
	if (ret) {
		dev_err(dev, "Failed to read rockchip,ranges!\n");
		ret = -ENODEV;
		goto release_slave_node;
	}
	dsmc_slave_np = of_parse_phandle(slave_np, "rockchip,slave-dev", 0);
	if (!dsmc_slave_np) {
		dev_err(dev, "Failed to get rockchip,slave-dev node\n");
		ret = -ENODEV;
		goto release_slave_node;
	}
	if (of_property_read_u32(dsmc_slave_np, "rockchip,clk-mode", &cfg->clk_mode)) {
		dev_err(dev, "Failed to get rockchip,clk-mode\n");
		ret = -ENODEV;
		goto release_dsmc_slave_node;
	}

	psram_np = of_get_child_by_name(dsmc_slave_np, "psram");
	if (psram_np) {
		for (cs = 0; cs < DSMC_MAX_SLAVE_NUM; cs++) {
			snprintf(slave_name, sizeof(slave_name), "psram%d", cs);
			child_node = of_get_child_by_name(psram_np, slave_name);
			if (!child_node)
				continue;
			if (!of_device_is_available(child_node)) {
				of_node_put(child_node);
				continue;
			}
			cfg->cs_cfg[cs].device_type = OPI_XCCELA_PSRAM;
			psram = 1;
			of_node_put(child_node);
		}
	}

	lb_slave_np = of_get_child_by_name(dsmc_slave_np, "lb-slave");
	if (lb_slave_np) {
		for (cs = 0; cs < DSMC_MAX_SLAVE_NUM; cs++) {
			snprintf(slave_name, sizeof(slave_name), "lb-slave%d", cs);
			child_node = of_get_child_by_name(lb_slave_np, slave_name);
			if (!child_node)
				continue;
			if (!of_device_is_available(child_node)) {
				of_node_put(child_node);
				continue;
			}
			cfg->cs_cfg[cs].device_type = DSMC_LB_DEVICE;
			lb_slave = 1;
			ret = of_property_read_u32_array(child_node,
							 "rockchip,mtr-timing",
							 mtr_timing, 8);
			if (ret) {
				dev_err(dev, "Fail to get rockchip,mtr-timing\n");
				of_node_put(child_node);
				goto release_dsmc_slave_node;
			}
			cfg->cs_cfg[cs].rcshi = mtr_timing[0];
			cfg->cs_cfg[cs].wcshi = mtr_timing[1];
			cfg->cs_cfg[cs].rcss = mtr_timing[2];
			cfg->cs_cfg[cs].wcss = mtr_timing[3];
			cfg->cs_cfg[cs].rcsh = mtr_timing[4];
			cfg->cs_cfg[cs].wcsh = mtr_timing[5];
			cfg->cs_cfg[cs].rd_latency = mtr_timing[6];
			cfg->cs_cfg[cs].wr_latency = mtr_timing[7];

			ret = of_property_read_u32(child_node,
						   "rockchip,int-en",
						   &cfg->cs_cfg[cs].int_en);
			if (ret) {
				dev_warn(dev, "used default rockchip,int-en\n");
				cfg->cs_cfg[cs].int_en = cs;
			}
			if (dsmc_parse_dt_regions(pdev, child_node,
						  &cfg->cs_cfg[cs])) {
				ret = -ENODEV;
				of_node_put(child_node);
				goto release_dsmc_slave_node;
			}
			of_node_put(child_node);

			ret = of_property_read_u32(lb_slave_np,
						   "rockchip,io-width",
						   &io_width_val);
			if (ret) {
				dev_warn(dev, "used default rockchip,io-width(x16 mode)\n");
					 cfg->cs_cfg[cs].io_width = MCR_IOWIDTH_X16;
			} else {
				if (io_width_val == 16) {
					cfg->cs_cfg[cs].io_width = MCR_IOWIDTH_X16;
				} else if (io_width_val == 8) {
					cfg->cs_cfg[cs].io_width = MCR_IOWIDTH_X8;
				} else {
					dev_warn(dev, "invalid rockchip,io-width %u, use x16\n",
						 io_width_val);
					cfg->cs_cfg[cs].io_width = MCR_IOWIDTH_X16;
				}
			}
		}
	}

	if (psram && lb_slave) {
		dev_err(dev, "Can't have both psram and lb_slave\n");
		ret = -ENODEV;
		goto release_dsmc_slave_node;
	} else if (!(psram || lb_slave)) {
		dev_err(dev, "psram or lb_slave need open in dts\n");
		ret = -ENODEV;
		goto release_dsmc_slave_node;
	}
	ret = dsmc_reg_remap(dev, cfg, dsmc, mem_ranges, dqs_dll);

release_dsmc_slave_node:
	of_node_put(psram_np);
	of_node_put(lb_slave_np);
	of_node_put(dsmc_slave_np);
release_slave_node:
	of_node_put(slave_np);

	return ret;
}

static int dsmc_read(struct rockchip_dsmc_device *dsmc_dev, uint32_t cs, uint32_t region,
		     unsigned long addr_offset, uint32_t *data)
{
	struct dsmc_map *map = &dsmc_dev->dsmc.cs_map[cs].region_map[region];

	*data = readl_relaxed(map->virt + addr_offset);

	return 0;
}

static int dsmc_write(struct rockchip_dsmc_device *dsmc_dev, uint32_t cs, uint32_t region,
		      unsigned long addr_offset, uint32_t val)
{
	struct dsmc_map *map = &dsmc_dev->dsmc.cs_map[cs].region_map[region];

	writel_relaxed(val, map->virt + addr_offset);

	return 0;
}

static void dsmc_lb_dma_hw_mode_en(struct rockchip_dsmc *dsmc, uint32_t cs)
{
	struct device *dev = dsmc->dev;
	struct dsmc_transfer *xfer = &dsmc->xfer;
	size_t size = xfer->transfer_size;
	uint32_t burst_byte = xfer->brst_len * xfer->brst_size;
	uint32_t dma_req_num;

	dma_req_num = size / burst_byte;
	if (size % burst_byte) {
		dev_warn(dev, "DMA size is unaligned\n");
		dma_req_num++;
	}
	writel(dma_req_num, dsmc->regs + DSMC_DMA_REQ_NUM(cs));

	/* enable dma request */
	writel(DMA_REQ_EN(cs), dsmc->regs + DSMC_DMA_EN);
}

static void rockchip_dsmc_interrupt_mask(struct rockchip_dsmc *dsmc)
{
	uint32_t cs = dsmc->xfer.ops_cs;

	/* mask dsmc interrupt */
	writel(INT_MASK(cs), dsmc->regs + DSMC_INT_MASK);
}

static void rockchip_dsmc_interrupt_unmask(struct rockchip_dsmc *dsmc)
{
	uint32_t cs = dsmc->xfer.ops_cs;

	/* mask dsmc interrupt */
	writel(INT_UNMASK(cs), dsmc->regs + DSMC_INT_MASK);
}

static int dmaengine_config_interleaved(struct dsmc_transfer *xfer,
					struct dma_interleaved_template *xt,
					enum dma_transfer_direction dir,
					int numf)
{
	xt->frame_size = 1;
	xt->sgl[0].size = xfer->brst_size * xfer->brst_len;
	xt->numf = xfer->transfer_size / xt->sgl[0].size;

	xt->dir = dir;

	if (xt->dir == DMA_MEM_TO_DEV) {
		xt->src_start = xfer->src_addr;
		xt->src_inc = true;
		xt->dst_inc = true;
	} else {
		xt->dst_start = xfer->dst_addr;
		xt->src_inc = true;
		xt->dst_inc = true;
	}

	return 0;
}

static void rockchip_dsmc_lb_dma_txcb(void *data)
{
	struct rockchip_dsmc *dsmc = data;

	atomic_fetch_andnot(TXDMA, &dsmc->xfer.state);
	rockchip_dsmc_lb_dma_hw_mode_dis(dsmc);
	rockchip_dsmc_interrupt_unmask(dsmc);
}

static void rockchip_dsmc_lb_dma_rxcb(void *data)
{
	struct rockchip_dsmc *dsmc = data;

	atomic_fetch_andnot(RXDMA, &dsmc->xfer.state);
	rockchip_dsmc_lb_dma_hw_mode_dis(dsmc);
	rockchip_dsmc_interrupt_unmask(dsmc);
}

static int rockchip_dsmc_lb_prepare_tx_dma(struct device *dev,
					   struct rockchip_dsmc *dsmc, uint32_t cs)
{
	struct dma_async_tx_descriptor *txdesc = NULL;
	struct dsmc_transfer *xfer = &dsmc->xfer;
	struct dma_interleaved_template xt;
	unsigned long flags = DMA_CTRL_ACK;
	struct dma_slave_config txconf = {
		.direction = DMA_MEM_TO_DEV,
		.dst_addr = xfer->dst_addr,
		.dst_addr_width = xfer->brst_size,
		.dst_maxburst = xfer->brst_len,
	};

	memset(&xt, 0, sizeof(xt));

	dmaengine_slave_config(dsmc->dma_req[cs], &txconf);

	dmaengine_config_interleaved(xfer, &xt, DMA_MEM_TO_DEV, 1);
	txdesc = dmaengine_prep_interleaved_dma(dsmc->dma_req[cs], &xt,
						flags | DMA_PREP_INTERRUPT);
	if (!txdesc) {
		dev_err(dev, "Not able to get tx desc for DMA xfer\n");
		return -EIO;
	}

	txdesc->callback = rockchip_dsmc_lb_dma_txcb;
	txdesc->callback_param = dsmc;

	atomic_or(TXDMA, &dsmc->xfer.state);
	dmaengine_submit(txdesc);
	dma_async_issue_pending(dsmc->dma_req[cs]);

	/* 1 means the transfer is in progress */
	return 1;
}

static int rockchip_dsmc_lb_prepare_rx_dma(struct device *dev,
					   struct rockchip_dsmc *dsmc, uint32_t cs)
{
	struct dma_async_tx_descriptor *rxdesc = NULL;
	struct dsmc_transfer *xfer = &dsmc->xfer;
	struct dma_interleaved_template xt;
	unsigned long flags = DMA_CTRL_ACK;
	struct dma_slave_config rxconf = {
		.direction = DMA_DEV_TO_MEM,
		.src_addr = xfer->src_addr,
		.src_addr_width = xfer->brst_size,
		.src_maxburst = xfer->brst_len,
	};

	memset(&xt, 0, sizeof(xt));

	dmaengine_slave_config(dsmc->dma_req[cs], &rxconf);

	dmaengine_config_interleaved(xfer, &xt, DMA_DEV_TO_MEM, 1);
	rxdesc = dmaengine_prep_interleaved_dma(dsmc->dma_req[cs], &xt,
						flags | DMA_PREP_INTERRUPT);
	if (!rxdesc) {
		dev_err(dev, "Not able to get rx desc for DMA xfer\n");
		return -EIO;
	}

	rxdesc->callback = rockchip_dsmc_lb_dma_rxcb;
	rxdesc->callback_param = dsmc;

	atomic_or(RXDMA, &dsmc->xfer.state);
	dmaengine_submit(rxdesc);
	dma_async_issue_pending(dsmc->dma_req[cs]);

	/* 1 means the transfer is in progress */
	return 1;

}

static int dsmc_copy_from(struct rockchip_dsmc_device *dsmc_dev, uint32_t cs, uint32_t region,
			  uint32_t from, dma_addr_t dst_phys, size_t size)
{
	struct dsmc_map *map = &dsmc_dev->dsmc.cs_map[cs].region_map[region];
	struct device *dev = dsmc_dev->dsmc.dev;
	struct rockchip_dsmc *dsmc = &dsmc_dev->dsmc;

	if (atomic_read(&dsmc->xfer.state) & (RXDMA | TXDMA)) {
		dev_warn(dev, "copy_from: the transfer is busy!\n");
		return -EBUSY;
	}

	dsmc->xfer.src_addr = map->phys + from;
	dsmc->xfer.dst_addr = dst_phys;
	dsmc->xfer.transfer_size = size;
	dsmc->xfer.ops_cs = cs;

	rockchip_dsmc_interrupt_mask(dsmc);

	rockchip_dsmc_lb_prepare_rx_dma(dev, dsmc, cs);

	dsmc_lb_dma_hw_mode_en(dsmc, cs);

	rockchip_dsmc_lb_dma_trigger_by_host(dsmc, cs);

	return 0;
}

static int dsmc_copy_from_state(struct rockchip_dsmc_device *dsmc_dev)
{
	struct rockchip_dsmc *dsmc = &dsmc_dev->dsmc;

	if (atomic_read(&dsmc->xfer.state) & RXDMA)
		return -EBUSY;
	else
		return 0;
}

static int dsmc_copy_to(struct rockchip_dsmc_device *dsmc_dev, uint32_t cs, uint32_t region,
			dma_addr_t src_phys, uint32_t to, size_t size)
{
	struct dsmc_map *map = &dsmc_dev->dsmc.cs_map[cs].region_map[region];
	struct device *dev = dsmc_dev->dsmc.dev;
	struct rockchip_dsmc *dsmc = &dsmc_dev->dsmc;

	if (atomic_read(&dsmc->xfer.state) & (RXDMA | TXDMA)) {
		dev_warn(dev, "copy_to: the transfer is busy!\n");
		return -EBUSY;
	}

	dsmc->xfer.src_addr = src_phys;
	dsmc->xfer.dst_addr = map->phys + to;
	dsmc->xfer.transfer_size = size;
	dsmc->xfer.ops_cs = cs;

	rockchip_dsmc_interrupt_mask(dsmc);

	rockchip_dsmc_lb_prepare_tx_dma(dev, dsmc, cs);

	dsmc_lb_dma_hw_mode_en(dsmc, cs);

	rockchip_dsmc_lb_dma_trigger_by_host(dsmc, cs);

	return 0;
}

static int dsmc_copy_to_state(struct rockchip_dsmc_device *dsmc_dev)
{
	struct rockchip_dsmc *dsmc = &dsmc_dev->dsmc;

	if (atomic_read(&dsmc->xfer.state) & TXDMA)
		return -EBUSY;
	else
		return 0;
}

static void dsmc_data_init(struct rockchip_dsmc *dsmc)
{
	uint32_t cs;
	struct dsmc_ctrl_config *cfg = &dsmc->cfg;
	struct dsmc_config_cs *cs_cfg;

	dsmc->xfer.brst_len = 16;
	dsmc->xfer.brst_size = DMA_SLAVE_BUSWIDTH_8_BYTES;
	for (cs = 0; cs < DSMC_MAX_SLAVE_NUM; cs++) {
		if (cfg->cs_cfg[cs].device_type == DSMC_UNKNOWN_DEVICE)
			continue;
		cs_cfg = &dsmc->cfg.cs_cfg[cs];

		cs_cfg->exclusive_dqs = 0;
		if (cs_cfg->device_type == OPI_XCCELA_PSRAM) {
			cs_cfg->io_width = MCR_IOWIDTH_X16;
			cs_cfg->wrap_size = DSMC_BURST_WRAPSIZE_32CLK;
			cs_cfg->wrap2incr_en = 0;
			cs_cfg->acs = 1;
			cs_cfg->max_length_en = 1;
			cs_cfg->max_length = 0xff;
			cs_cfg->rd_bdr_xfer_en = 1;
			cs_cfg->wr_bdr_xfer_en = 1;
		} else {
			cs_cfg->wrap_size = DSMC_BURST_WRAPSIZE_16CLK;
			cs_cfg->wrap2incr_en = 1;
			cs_cfg->acs = 1;
			cs_cfg->max_length_en = 0;
			cs_cfg->max_length = 0x0;
		}
	}
}

static int dsmc_check_mult_psram_cap(struct rockchip_dsmc *dsmc)
{
	uint32_t cs;
	uint32_t io_width = 0xffffffff;
	struct device *dev = dsmc->dev;
	struct dsmc_ctrl_config *cfg = &dsmc->cfg;

	for (cs = 0; cs < DSMC_MAX_SLAVE_NUM; cs++) {
		if (cfg->cs_cfg[cs].device_type == DSMC_UNKNOWN_DEVICE)
			continue;
		if (io_width == 0xffffffff) {
			io_width = dsmc->cfg.cs_cfg[cs].io_width;
		} else {
			if (io_width != dsmc->cfg.cs_cfg[cs].io_width) {
				dev_err(dev, "The io width error for mult rank!\n");
				return -1;
			}
		}
	}

	return 0;
}

static void dsmc_reset_ctrl(struct rockchip_dsmc *dsmc)
{
	reset_control_assert(dsmc->areset);
	reset_control_assert(dsmc->preset);
	udelay(20);
	reset_control_deassert(dsmc->areset);
	reset_control_deassert(dsmc->preset);
}

static int dsmc_init(struct rockchip_dsmc *dsmc)
{
	uint32_t cs;
	struct device *dev = dsmc->dev;
	struct dsmc_ctrl_config *cfg = &dsmc->cfg;
	struct dsmc_config_cs *cs_cfg;
	uint32_t ret = 0;

	dsmc_data_init(dsmc);

	for (cs = 0; cs < DSMC_MAX_SLAVE_NUM; cs++) {
		if (cfg->cs_cfg[cs].device_type == DSMC_UNKNOWN_DEVICE)
			continue;
		if (cfg->cs_cfg[cs].device_type == OPI_XCCELA_PSRAM) {
			ret = rockchip_dsmc_device_dectect(dsmc, cs);
			if (ret)
				return ret;
		}
	}

	ret = dsmc_check_mult_psram_cap(dsmc);
	if (ret)
		return ret;

	dsmc_reset_ctrl(dsmc);

	for (cs = 0; cs < DSMC_MAX_SLAVE_NUM; cs++) {
		if (cfg->cs_cfg[cs].device_type == DSMC_UNKNOWN_DEVICE)
			continue;
		cs_cfg = &dsmc->cfg.cs_cfg[cs];
		dev_info(dev, "init cs%d %s device\n",
			cs, (cs_cfg->device_type == DSMC_LB_DEVICE) ? "LB" : "PSRAM");
		rockchip_dsmc_ctrller_init(dsmc, cs);
		if (cs_cfg->device_type == OPI_XCCELA_PSRAM)
			ret = rockchip_dsmc_psram_reinit(dsmc, cs);
		else
			ret = rockchip_dsmc_lb_init(dsmc, cs);

		if (ret)
			break;
	}

	return ret;
}

static struct dsmc_ops rockchip_dsmc_ops = {
	.read = dsmc_read,
	.write = dsmc_write,
	.copy_from = dsmc_copy_from,
	.copy_from_state = dsmc_copy_from_state,
	.copy_to = dsmc_copy_to,
	.copy_to_state = dsmc_copy_to_state,
};

static int rockchip_dsmc_dma_request(struct device *dev, struct rockchip_dsmc *dsmc)
{
	int ret = 0;

	atomic_set(&dsmc->xfer.state, 0);

	dsmc->dma_req[0] = dma_request_chan(dev, "req0");
	if (!dsmc->dma_req[0]) {
		dev_err(dev, "Failed to request DMA dsmc req0 channel!\n");
		return -ENODEV;
	}

	dsmc->dma_req[1] = dma_request_chan(dev, "req1");
	if (!dsmc->dma_req[1]) {
		dev_err(dev, "Failed to request DMA dsmc req1 channel!\n");
		ret = -ENODEV;
		goto err;
	}
	return ret;
err:
	dma_release_channel(dsmc->dma_req[0]);
	return ret;
}

const char *rockchip_dsmc_get_compat(int index)
{
	if (index < 0 || index >= ARRAY_SIZE(dsmc_of_match))
		return NULL;

	return dsmc_of_match[index].compatible;
}
EXPORT_SYMBOL(rockchip_dsmc_get_compat);

static int match_dsmc_device(struct device *dev, const void *data)
{
	const char *compat = data;

	if (!dev->of_node)
		return 0;

	return of_device_is_compatible(dev->of_node, compat);
}

struct rockchip_dsmc_device *rockchip_dsmc_find_device_by_compat(const char *compat)
{
	struct device *dev;
	struct platform_device *pdev;
	const struct rockchip_dsmc_device *priv;

	dev = bus_find_device(&platform_bus_type, NULL, compat, match_dsmc_device);
	if (!dev)
		return NULL;

	pdev = to_platform_device(dev);
	priv = platform_get_drvdata(pdev);

	return (struct rockchip_dsmc_device *)priv;
}
EXPORT_SYMBOL(rockchip_dsmc_find_device_by_compat);

static int rk_dsmc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct rockchip_dsmc *dsmc;
	struct rockchip_dsmc_device *priv;
	struct resource *mem;
	int ret = 0;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	dsmc = &priv->dsmc;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dsmc->regs = devm_ioremap_resource(&pdev->dev, mem);
	if (IS_ERR(dsmc->regs))
		return PTR_ERR(dsmc->regs);

	dsmc->grf = syscon_regmap_lookup_by_phandle(np, "rockchip,grf");
	if (IS_ERR(dsmc->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return ret;
	}

	ret = device_property_read_u32(dev, "clock-frequency", &dsmc->cfg.freq_hz);
	if (ret) {
		dev_err(dev, "Failed to read clock-frequency property!\n");
		return ret;
	}

	dsmc->cfg.ctrl_freq_hz = dsmc->cfg.freq_hz * 2;
	dsmc->cfg.dma_req_mux_offset = DSMC_DMA_MUX;

	if (dsmc_parse_dt(pdev, dsmc)) {
		ret = -ENODEV;
		dev_err(dev, "The dts parameters get fail! ret = %d\n", ret);
		return ret;
	}
	ret = rockchip_dsmc_platform_init(pdev);
	if (ret)
		return ret;

	dsmc->areset = devm_reset_control_get(dev, "dsmc");
	if (IS_ERR(dsmc->areset)) {
		ret = PTR_ERR(dsmc->areset);
		dev_err(dev, "failed to get dsmc areset: %d\n", ret);
		return ret;
	}
	dsmc->preset = devm_reset_control_get(dev, "apb");
	if (IS_ERR(dsmc->preset)) {
		ret = PTR_ERR(dsmc->preset);
		dev_err(dev, "failed to get dsmc preset: %d\n", ret);
		return ret;
	}

	dsmc->clk_sys = devm_clk_get(dev, "clk_sys");
	if (IS_ERR(dsmc->clk_sys)) {
		dev_err(dev, "Can't get clk_sys clk\n");
		return PTR_ERR(dsmc->clk_sys);
	}

	dsmc->aclk = devm_clk_get(dev, "aclk_dsmc");
	if (IS_ERR(dsmc->aclk)) {
		dev_err(dev, "Can't get aclk_dsmc clk\n");
		return PTR_ERR(dsmc->aclk);
	}

	dsmc->pclk = devm_clk_get(dev, "pclk");
	if (IS_ERR(dsmc->pclk)) {
		dev_err(dev, "Can't get pclk clk\n");
		return PTR_ERR(dsmc->pclk);
	}
	dsmc->aclk_root = devm_clk_get(dev, "aclk_root");
	if (IS_ERR(dsmc->aclk_root)) {
		dev_err(dev, "Can't get aclk_root clk\n");
		return PTR_ERR(dsmc->aclk_root);
	}

	ret = clk_prepare_enable(dsmc->aclk_root);
	if (ret) {
		dev_err(dev, "Can't prepare enable dsmc aclk_root: %d\n", ret);
		goto out;
	}
	ret = clk_prepare_enable(dsmc->aclk);
	if (ret) {
		dev_err(dev, "Can't prepare enable dsmc aclk: %d\n", ret);
		goto err_dis_aclk_root;
	}
	ret = clk_prepare_enable(dsmc->pclk);
	if (ret) {
		dev_err(dev, "Can't prepare enable dsmc pclk: %d\n", ret);
		goto err_dis_aclk;
	}
	ret = clk_prepare_enable(dsmc->clk_sys);
	if (ret) {
		dev_err(dev, "Can't prepare enable dsmc clk_sys: %d\n", ret);
		goto err_dis_pclk;
	}

	ret = clk_set_rate(dsmc->aclk_root, dsmc->cfg.freq_hz);
	if (ret) {
		dev_err(dev, "Failed to set dsmc aclk_root rate\n");
		goto err_dis_all_clk;
	}
	ret = clk_set_rate(dsmc->clk_sys, dsmc->cfg.ctrl_freq_hz);
	if (ret) {
		dev_err(dev, "Failed to set dsmc sys rate\n");
		goto err_dis_all_clk;
	}

	ret = rockchip_dsmc_dma_request(dev, dsmc);
	if (ret) {
		dev_err(dev, "Failed to request dma channel\n");
		goto err_dis_all_clk;
	}

	dsmc->dev = dev;
	priv->ops = &rockchip_dsmc_ops;

	if (dsmc_init(dsmc)) {
		ret = -ENODEV;
		dev_err(dev, "DSMC init fail!\n");
		goto err_release_dma;
	}

	if (dsmc_mem_remap(dev, dsmc)) {
		ret = -ENODEV;
		dev_err(dev, "DSMC memory remap fail!\n");
		goto err_release_dma;
	}

	if (rockchip_dsmc_dll_training(priv)) {
		ret = -ENODEV;
		dev_err(dev, "DSMC dll training fail!\n");
		goto err_release_dma;
	}

	if (rockchip_dsmc_status_check(priv)) {
		ret = -ENODEV;
		dev_err(dev, "DSMC status error, please check hardware matched(io, slave etc.)\n");
		goto err_release_dma;
	}

	return 0;

err_release_dma:
	if (dsmc->dma_req[0])
		dma_release_channel(dsmc->dma_req[0]);
	if (dsmc->dma_req[1])
		dma_release_channel(dsmc->dma_req[1]);
err_dis_all_clk:
	clk_disable_unprepare(dsmc->clk_sys);
err_dis_pclk:
	clk_disable_unprepare(dsmc->pclk);
err_dis_aclk:
	clk_disable_unprepare(dsmc->aclk);
err_dis_aclk_root:
	clk_disable_unprepare(dsmc->aclk_root);

out:
	return ret;
}

static void release_dsmc_mem(struct device *dev, struct rockchip_dsmc *dsmc)
{
	int i;
	uint32_t cs;
	struct dsmc_map *region_map;
	struct dsmc_ctrl_config *cfg = &dsmc->cfg;

	for (cs = 0; cs < DSMC_MAX_SLAVE_NUM; cs++) {
		if (cfg->cs_cfg[cs].device_type == DSMC_UNKNOWN_DEVICE)
			continue;
		for (i = 0; i < DSMC_LB_MAX_RGN; i++) {
			region_map = &dsmc->cs_map[cs].region_map[i];
			if (region_map->virt) {
				rk_dsmc_unmap_kernel(region_map->virt);
				region_map->virt = NULL;
			}
		}
		if (dsmc->cfg.cs_cfg[cs].device_type == DSMC_LB_DEVICE)
			rockchip_dsmc_unregister_lb_device(dev, cs);
	}
}

static int rk_dsmc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rockchip_dsmc *dsmc;
	struct rockchip_dsmc_device *priv;

	priv = platform_get_drvdata(pdev);
	dsmc = &priv->dsmc;

	if (dsmc->aclk_root) {
		clk_disable_unprepare(dsmc->aclk_root);
		dsmc->aclk_root = NULL;
	}
	if (dsmc->aclk) {
		clk_disable_unprepare(dsmc->aclk);
		dsmc->aclk = NULL;
	}
	if (dsmc->pclk) {
		clk_disable_unprepare(dsmc->pclk);
		dsmc->pclk = NULL;
	}
	if (dsmc->clk_sys) {
		clk_disable_unprepare(dsmc->clk_sys);
		dsmc->clk_sys = NULL;
	}

	release_dsmc_mem(dev, dsmc);

	if (dsmc->dma_req[0])
		dma_release_channel(dsmc->dma_req[0]);
	if (dsmc->dma_req[1])
		dma_release_channel(dsmc->dma_req[1]);

	return 0;
}

static struct platform_driver rk_dsmc_driver = {
	.probe = rk_dsmc_probe,
	.remove = rk_dsmc_remove,
	.driver = {
		.name = "dsmc",
		.of_match_table = dsmc_of_match,
	},
};

static int __init rk_dsmc_init(void)
{
	int ret;

	ret = rockchip_dsmc_lb_class_create("dsmc");
	if (ret != 0) {
		pr_err("Failed to create DSMC class\n");
		return ret;
	}

	ret = platform_driver_register(&rk_dsmc_driver);
	if (ret != 0) {
		pr_err("Failed to register rockchip dsmc driver\n");
		rockchip_dsmc_lb_class_destroy();
		return ret;
	}

	return 0;
}

static void __exit rk_dsmc_exit(void)
{
	platform_driver_unregister(&rk_dsmc_driver);
	rockchip_dsmc_lb_class_destroy();
}

module_init(rk_dsmc_init);
module_exit(rk_dsmc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhihuan He <huan.he@rock-chips.com>");
MODULE_DESCRIPTION("ROCKCHIP DSMC host driver");
