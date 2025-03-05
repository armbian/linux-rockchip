// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */
#include <linux/cacheflush.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>

#include "dsmc-host.h"
#include "dsmc-lb-slave.h"

#define MHZ					(1000000)

#define REG_CLRSETBITS(dsmc, offset, clrbits, setbits) \
		dsmc_modify_reg(dsmc, offset, clrbits, setbits)

/* psram id */
enum {
	CYPRESS = 0x1,
	ISSI = 0x3,
	WINBOND = 0x6,
	APM_PSRAM = 0xd,
};

struct dsmc_psram {
	uint16_t id;
	uint16_t protcl;
	uint32_t mtr_timing;
};

/* DSMC psram support list */
static const struct dsmc_psram psram_info[] = {
	/* Only APM is Xccela psram, others are Hyper psram */
	{APM_PSRAM, OPI_XCCELA_PSRAM, MTR_CFG(2, 2, 0, 0, 0, 0, 0, 0)},
	{WINBOND, HYPERBUS_PSRAM, MTR_CFG(2, 2, 0, 0, 0, 0, 2, 2)},
	{CYPRESS, HYPERBUS_PSRAM, MTR_CFG(2, 2, 0, 0, 0, 0, 1, 1)},
	{ISSI, HYPERBUS_PSRAM, MTR_CFG(2, 2, 0, 0, 0, 0, 1, 1)},
};

static inline void xccela_write_mr(struct dsmc_map *map,
				   uint32_t mr_num, uint8_t val)
{
	writew(XCCELA_PSRAM_MR_SET(val), map->virt + XCCELA_PSRAM_MR(mr_num));
}

static inline uint8_t xccela_read_mr(struct dsmc_map *map, uint32_t mr_num)
{
	return XCCELA_PSRAM_MR_GET(readw(map->virt +
				   XCCELA_PSRAM_MR(mr_num)));
}

static inline void hyper_write_mr(struct dsmc_map *map,
				  uint32_t mr_num, uint16_t val)
{
	writew(val, map->virt + mr_num);
}

static inline uint16_t hyper_read_mr(struct dsmc_map *map, uint32_t mr_num)
{
	return readw(map->virt + mr_num);
}

static inline void lb_write_cmn(struct dsmc_map *map,
				 uint32_t cmn_reg, uint32_t val)
{
	writel(val, map->virt + cmn_reg);
}

static inline uint32_t lb_read_cmn(struct dsmc_map *map, uint32_t cmn_reg)
{
	return readl(map->virt + cmn_reg);
}

static inline void dsmc_modify_reg(struct rockchip_dsmc *dsmc, uint32_t offset,
				   uint32_t clrbits, uint32_t setbits)
{
	uint32_t value;

	value = readl(dsmc->regs + offset);
	value &= ~clrbits;
	value |= setbits;
	writel(value, dsmc->regs + offset);
}

static int find_attr_region(struct dsmc_config_cs *cfg, uint32_t attribute)
{
	int region;

	for (region = 0; region < DSMC_LB_MAX_RGN; region++) {
		if ((cfg->slv_rgn[region].status) &&
		    (cfg->slv_rgn[region].attribute == attribute))
			return region;
	}
	return -1;
}

static uint32_t cap_2_dev_size(uint32_t cap)
{
	uint32_t mask = 0x80000000;
	int i;

	for (i = 31; i >= 0; i--) {
		if (cap & mask)
			return i;
		mask >>= 1;
	}
	return 0;
}

static int dsmc_psram_id_detect(struct rockchip_dsmc *dsmc, uint32_t cs)
{
	uint32_t tmp[DSMC_MAX_SLAVE_NUM], i;
	int ret = -1;
	struct dsmc_map *region_map = &dsmc->cs_map[cs].region_map[0];
	struct dsmc_config_cs *cfg = &dsmc->cfg.cs_cfg[cs];

	for (i = 0; i < DSMC_MAX_SLAVE_NUM; i++) {
		tmp[i] = readl(dsmc->regs + DSMC_MCR(i));

		/* config to CR space */
		REG_CLRSETBITS(dsmc, DSMC_MCR(i),
			       (MCR_IOWIDTH_MASK << MCR_IOWIDTH_SHIFT) |
			       (MCR_CRT_MASK << MCR_CRT_SHIFT),
			       (MCR_IOWIDTH_X8 << MCR_IOWIDTH_SHIFT) |
			       (MCR_CRT_CR_SPACE << MCR_CRT_SHIFT));
	}

	if (cfg->protcl == OPI_XCCELA_PSRAM) {
		uint8_t mid;

		/* reset AP memory psram */
		REG_CLRSETBITS(dsmc, DSMC_VDMC(cs),
			       (VDMC_RESET_CMD_MODE_MASK << VDMC_RESET_CMD_MODE_SHIFT),
			       (0x1 << VDMC_RESET_CMD_MODE_SHIFT));
		/* write mr any value to trigger xccela psram reset */
		xccela_write_mr(region_map, 0, 0x0);
		udelay(200);
		REG_CLRSETBITS(dsmc, DSMC_VDMC(cs),
			       (VDMC_RESET_CMD_MODE_MASK << VDMC_RESET_CMD_MODE_SHIFT),
			       (0x0 << VDMC_RESET_CMD_MODE_SHIFT));

		mid = xccela_read_mr(region_map, 1);
		mid &= XCCELA_DEV_ID_MASK;

		if (mid == APM_PSRAM)
			ret = 0;
	} else {
		/* hyper psram get ID */
		uint16_t mid;

		mid = hyper_read_mr(region_map, HYPER_PSRAM_IR0);
		mid &= HYPERBUS_DEV_ID_MASK;
		for (i = 1; i < ARRAY_SIZE(psram_info); i++) {
			if (mid == psram_info[i].id) {
				ret = 0;
				break;
			}
		}
	}

	for (i = 0; i < DSMC_MAX_SLAVE_NUM; i++)
		/* config to memory space */
		writel(tmp[i], dsmc->regs + DSMC_MCR(i));

	return ret;
}

static void dsmc_psram_bw_detect(struct rockchip_dsmc *dsmc, uint32_t cs)
{
	uint32_t i;
	uint32_t tmp[DSMC_MAX_SLAVE_NUM], col;
	uint16_t ir0_ir1;
	struct dsmc_map *region_map = &dsmc->cs_map[cs].region_map[0];
	struct dsmc_config_cs *cfg = &dsmc->cfg.cs_cfg[cs];

	if (cfg->protcl == OPI_XCCELA_PSRAM) {
		col = 10;
		if (dsmc->cfg.cap >= PSRAM_SIZE_16MBYTE)
			cfg->io_width = MCR_IOWIDTH_X16;
		else
			cfg->io_width = MCR_IOWIDTH_X8;
	} else {
		for (i = 0; i < DSMC_MAX_SLAVE_NUM; i++) {
			tmp[i] = readl(dsmc->regs + DSMC_MCR(i));
			/* config to CR space */
			REG_CLRSETBITS(dsmc, DSMC_MCR(i),
				       (MCR_IOWIDTH_MASK << MCR_IOWIDTH_SHIFT) |
				       (MCR_CRT_MASK << MCR_CRT_SHIFT),
				       (MCR_IOWIDTH_X8 << MCR_IOWIDTH_SHIFT) |
				       (MCR_CRT_CR_SPACE << MCR_CRT_SHIFT));
		}

		/* hyper psram get IR0 */
		ir0_ir1 = hyper_read_mr(region_map, HYPER_PSRAM_IR0);
		col = ((ir0_ir1 >> IR0_COL_COUNT_SHIFT) & IR0_COL_COUNT_MASK) + 1;

		ir0_ir1 = hyper_read_mr(region_map, HYPER_PSRAM_IR1);
		if ((ir0_ir1 & IR1_DEV_IO_WIDTH_MASK) == IR1_DEV_IO_WIDTH_X16)
			cfg->io_width = MCR_IOWIDTH_X16;
		else
			cfg->io_width = MCR_IOWIDTH_X8;

		for (i = 0; i < DSMC_MAX_SLAVE_NUM; i++)
			/* config to memory space */
			writel(tmp[i], dsmc->regs + DSMC_MCR(i));

	}
	cfg->col = col;
}

static int dsmc_psram_dectect(struct rockchip_dsmc *dsmc, uint32_t cs)
{
	uint32_t i = 0;
	int ret = -1;
	struct device *dev = dsmc->dev;
	struct dsmc_config_cs *cfg = &dsmc->cfg.cs_cfg[cs];

	/* axi read do not response error */
	REG_CLRSETBITS(dsmc, DSMC_AXICTL,
		       (AXICTL_RD_NO_ERR_MASK << AXICTL_RD_NO_ERR_SHIFT),
		       (0x1 << AXICTL_RD_NO_ERR_SHIFT));
	REG_CLRSETBITS(dsmc, DSMC_MCR(cs),
		       (MCR_DEVTYPE_MASK << MCR_DEVTYPE_SHIFT),
		       (MCR_DEVTYPE_HYPERRAM << MCR_DEVTYPE_SHIFT));

	for (i = 0; i < ARRAY_SIZE(psram_info); i++) {
		REG_CLRSETBITS(dsmc, DSMC_VDMC(cs),
			       (VDMC_MID_MASK << VDMC_MID_SHIFT) |
			       (VDMC_PROTOCOL_MASK << VDMC_PROTOCOL_SHIFT),
			       (psram_info[i].id << VDMC_MID_SHIFT) |
			       (psram_info[i].protcl << VDMC_PROTOCOL_SHIFT));
		writel(psram_info[i].mtr_timing,
		       dsmc->regs + DSMC_MTR(cs));

		cfg->mid = psram_info[i].id;
		cfg->protcl = psram_info[i].protcl;
		cfg->mtr_timing = psram_info[i].mtr_timing;
		if (!dsmc_psram_id_detect(dsmc, cs)) {
			dev_info(dev, "The cs%d %s PSRAM ID: 0x%x\n", cs,
				(cfg->protcl == OPI_XCCELA_PSRAM) ? "XCCELA" : "HYPER",
				psram_info[i].id);
			ret = 0;
			break;
		}
	}
	if (i == ARRAY_SIZE(psram_info)) {
		dev_err(dev, "Unknown PSRAM device\n");
		ret = -1;
	} else {
		dsmc_psram_bw_detect(dsmc, cs);
	}

	/* recovery axi read response */
	REG_CLRSETBITS(dsmc, DSMC_AXICTL,
		       (AXICTL_RD_NO_ERR_MASK << AXICTL_RD_NO_ERR_SHIFT),
		       (0x0 << AXICTL_RD_NO_ERR_SHIFT));

	return ret;
}

static uint32_t calc_ltcy_value(uint32_t latency)
{
	if ((latency >= 5) && (latency <= 10))
		return (latency - 5);
	else
		return (latency + 0xb);
}

static int dsmc_ctrller_cfg_for_lb(struct rockchip_dsmc *dsmc, uint32_t cs)
{
	uint32_t value = 0, i;
	struct regions_config *slv_rgn;
	struct dsmc_config_cs *cfg = &dsmc->cfg.cs_cfg[cs];

	writel(dsmc->cfg.clk_mode, dsmc->regs + DSMC_CLK_MD);
	writel(MTR_CFG(cfg->rcshi, cfg->wcshi, cfg->rcss, cfg->wcss,
		       cfg->rcsh, cfg->wcsh,
		       calc_ltcy_value(cfg->rd_latency),
		       calc_ltcy_value(cfg->wr_latency)),
	       dsmc->regs + DSMC_MTR(cs));
	writel(cfg->rgn_num / 2,
	       dsmc->regs + DSMC_SLV_RGN_DIV(cs));
	for (i = 0; i < DSMC_LB_MAX_RGN; i++) {
		slv_rgn = &cfg->slv_rgn[i];
		if (!slv_rgn->status)
			continue;

		if (slv_rgn->dummy_clk_num >= 2)
			value = (0x1 << RGNX_ATTR_DUM_CLK_EN_SHIFT) |
				(0x1 << RGNX_ATTR_DUM_CLK_NUM_SHIFT);
		else if (slv_rgn->dummy_clk_num >= 1)
			value = (0x1 << RGNX_ATTR_DUM_CLK_EN_SHIFT) |
				(0x0 << RGNX_ATTR_DUM_CLK_NUM_SHIFT);
		else
			value = 0x0 << RGNX_ATTR_DUM_CLK_EN_SHIFT;
		writel((slv_rgn->attribute << RGNX_ATTR_SHIFT) |
		       (slv_rgn->cs0_ctrl << RGNX_ATTR_CTRL_SHIFT) |
		       (slv_rgn->cs0_be_ctrled <<
			RGNX_ATTR_BE_CTRLED_SHIFT) | value |
		       (RGNX_ATTR_32BIT_ADDR_WIDTH <<
			RGNX_ATTR_ADDR_WIDTH_SHIFT),
		       dsmc->regs + DSMC_RGN0_ATTR(cs) + 4 * i);
	}
	/* clear and enable interrupt */
	REG_CLRSETBITS(dsmc, DSMC_INT_STATUS,
		       INT_STATUS_MASK(cfg->int_en),
		       INT_STATUS(cfg->int_en));
	REG_CLRSETBITS(dsmc, DSMC_INT_EN,
		       INT_EN_MASK(cfg->int_en),
		       INT_EN(cfg->int_en));

	if (dsmc->cfg.dma_req_mux_offset && (cs < 2))
		REG_CLRSETBITS(dsmc, dsmc->cfg.dma_req_mux_offset,
			       DMA_REQ_MUX_MASK(cs),
			       DMA_REQ_MUX(cs, dsmc->cfg.cs_cfg[cs].int_en));

	return 0;
}

static int dsmc_slv_cmn_rgn_config(struct rockchip_dsmc *dsmc,
				   struct regions_config *slv_rgn,
				   uint32_t rgn, uint32_t cs)
{
	uint32_t tmp;
	struct device *dev = dsmc->dev;
	struct dsmc_map *region_map = &dsmc->cs_map[cs].region_map[0];
	struct dsmc_config_cs *cfg = &dsmc->cfg.cs_cfg[cs];

	tmp = lb_read_cmn(region_map, RGN_CMN_CON(rgn, 0));
	if (slv_rgn->dummy_clk_num == 0) {
		tmp &= ~(WR_DATA_CYC_EXTENDED_MASK << WR_DATA_CYC_EXTENDED_SHIFT);
	} else if (slv_rgn->dummy_clk_num == 1) {
		tmp |= slv_rgn->dummy_clk_num << WR_DATA_CYC_EXTENDED_SHIFT;
	} else {
		dev_err(dev, "lb slave: dummy clk too large\n");
		return -1;
	}
	tmp &= ~(RD_LATENCY_CYC_MASK << RD_LATENCY_CYC_SHIFT);
	if ((cfg->rd_latency == 1) || (cfg->rd_latency == 2)) {
		tmp |= cfg->rd_latency << RD_LATENCY_CYC_SHIFT;
	} else {
		dev_err(dev, "lb slave: read latency value error\n");
		return -1;
	}
	tmp &= ~(WR_LATENCY_CYC_MASK << WR_LATENCY_CYC_SHIFT);
	if ((cfg->wr_latency == 1) || (cfg->wr_latency == 2)) {
		tmp |= cfg->wr_latency << WR_LATENCY_CYC_SHIFT;
	} else {
		dev_err(dev, "lb slave: write latency value error\n");
		return -1;
	}
	tmp &= ~(CA_CYC_MASK << CA_CYC_SHIFT);
	if (slv_rgn->ca_addr_width == RGNX_ATTR_32BIT_ADDR_WIDTH)
		tmp |= CA_CYC_32BIT << CA_CYC_SHIFT;
	else
		tmp |= CA_CYC_16BIT << CA_CYC_SHIFT;

	lb_write_cmn(region_map, RGN_CMN_CON(rgn, 0), tmp);

	return 0;
}

static int dsmc_slv_cmn_config(struct rockchip_dsmc *dsmc,
			       struct regions_config *slv_rgn, uint32_t cs)
{
	uint32_t tmp;
	struct device *dev = dsmc->dev;
	struct dsmc_map *region_map = &dsmc->cs_map[cs].region_map[0];
	struct dsmc_config_cs *cfg = &dsmc->cfg.cs_cfg[cs];

	tmp = lb_read_cmn(region_map, CMN_CON(3));
	tmp |= 0x1 << RDYN_GEN_CTRL_SHIFT;
	tmp &= ~(DATA_WIDTH_MASK << DATA_WIDTH_SHIFT);
	tmp |= cfg->io_width << DATA_WIDTH_SHIFT;
	lb_write_cmn(region_map, CMN_CON(3), tmp);

	tmp = lb_read_cmn(region_map, CMN_CON(0));
	if (slv_rgn->dummy_clk_num == 0) {
		tmp &= ~(WR_DATA_CYC_EXTENDED_MASK << WR_DATA_CYC_EXTENDED_SHIFT);
	} else if (slv_rgn->dummy_clk_num == 1) {
		tmp |= slv_rgn->dummy_clk_num << WR_DATA_CYC_EXTENDED_SHIFT;
	} else {
		dev_err(dev, "lb slave: dummy clk too large\n");
		return -1;
	}

	tmp &= ~(CA_CYC_MASK << CA_CYC_SHIFT);
	if (slv_rgn->ca_addr_width == RGNX_ATTR_32BIT_ADDR_WIDTH)
		tmp |= CA_CYC_32BIT << CA_CYC_SHIFT;
	else
		tmp |= CA_CYC_16BIT << CA_CYC_SHIFT;

	lb_write_cmn(region_map, CMN_CON(0), tmp);

	return 0;
}

static int dsmc_lb_cmn_config(struct rockchip_dsmc *dsmc, uint32_t cs)
{
	uint32_t tmp[DSMC_MAX_SLAVE_NUM], i;
	struct dsmc_config_cs *cfg = &dsmc->cfg.cs_cfg[cs];
	struct regions_config *slv_rgn;
	int ret = 0;

	for (i = 0; i < DSMC_MAX_SLAVE_NUM; i++) {
		tmp[i] = readl(dsmc->regs + DSMC_MCR(i));
		/* config to CR space */
		REG_CLRSETBITS(dsmc, DSMC_MCR(i),
			       (MCR_IOWIDTH_MASK << MCR_IOWIDTH_SHIFT) |
			       (MCR_CRT_MASK << MCR_CRT_SHIFT),
			       (MCR_IOWIDTH_X8 << MCR_IOWIDTH_SHIFT) |
			       (MCR_CRT_CR_SPACE << MCR_CRT_SHIFT));
	}

	for (i = 0; i < DSMC_LB_MAX_RGN; i++) {
		slv_rgn = &cfg->slv_rgn[i];
		if (!slv_rgn->status)
			continue;
		ret = dsmc_slv_cmn_rgn_config(dsmc, slv_rgn, i, cs);
		if (ret)
			break;
	}

	slv_rgn = &cfg->slv_rgn[0];
	ret = dsmc_slv_cmn_config(dsmc, slv_rgn, cs);

	for (i = 0; i < DSMC_MAX_SLAVE_NUM; i++)
		/* config to memory space */
		writel(tmp[i], dsmc->regs + DSMC_MCR(i));

	for (i = 0; i < DSMC_LB_MAX_RGN; i++) {
		slv_rgn = &cfg->slv_rgn[i];
		if (!slv_rgn->status)
			continue;

		REG_CLRSETBITS(dsmc, DSMC_RGN0_ATTR(cs) + 4 * i,
			       RGNX_ATTR_CA_ADDR_MASK << RGNX_ATTR_ADDR_WIDTH_SHIFT,
			       slv_rgn->ca_addr_width << RGNX_ATTR_ADDR_WIDTH_SHIFT);
	}

	return ret;
}

static void dsmc_lb_csr_config(struct rockchip_dsmc *dsmc, uint32_t cs)
{
	uint32_t mcr_tmp, rgn_attr_tmp;
	struct dsmc_map *region_map = &dsmc->cs_map[cs].region_map[0];

	mcr_tmp = readl(dsmc->regs + DSMC_MCR(cs));
	rgn_attr_tmp = readl(dsmc->regs + DSMC_RGN0_ATTR(cs));
	/* config to slave CSR space */
	REG_CLRSETBITS(dsmc, DSMC_MCR(cs),
		       MCR_CRT_MASK << MCR_CRT_SHIFT,
		       MCR_CRT_MEM_SPACE << MCR_CRT_SHIFT);
	REG_CLRSETBITS(dsmc, DSMC_RGN0_ATTR(cs),
		       RGNX_ATTR_MASK << RGNX_ATTR_SHIFT,
		       RGNX_ATTR_REG << RGNX_ATTR_SHIFT);

	/* enable all s2h interrupt */
	writel(0xffffffff, region_map->virt  + LBC_S2H_INT_STA_EN);
	writel(0xffffffff, region_map->virt  + LBC_S2H_INT_STA_SIG_EN);

	/* clear all s2h interrupt */
	writel(LBC_S2H_INT_STA_MASK << LBC_S2H_INT_STA_SHIFT,
	       region_map->virt  + LBC_S2H_INT_STA);

	/* config to normal memory space */
	writel(mcr_tmp, dsmc->regs + DSMC_MCR(cs));
	writel(rgn_attr_tmp, dsmc->regs + DSMC_RGN0_ATTR(cs));
}

static void dsmc_cfg_latency(uint32_t rd_ltcy, uint32_t wr_ltcy,
			     struct rockchip_dsmc *dsmc, uint32_t cs)
{
	rd_ltcy = calc_ltcy_value(rd_ltcy);
	wr_ltcy = calc_ltcy_value(wr_ltcy);

	REG_CLRSETBITS(dsmc, DSMC_MTR(cs),
		       (MTR_RLTCY_MASK << MTR_RLTCY_SHIFT) |
		       (MTR_WLTCY_MASK << MTR_WLTCY_SHIFT),
		       (rd_ltcy << MTR_RLTCY_SHIFT) |
		       (wr_ltcy << MTR_WLTCY_SHIFT));
}

static int dsmc_psram_cfg(struct rockchip_dsmc *dsmc, uint32_t cs)
{
	uint32_t i;
	uint32_t latency, mcr[DSMC_MAX_SLAVE_NUM], tmp;
	struct dsmc_map *region_map = &dsmc->cs_map[cs].region_map[0];
	struct dsmc_config_cs *cs_cfg = &dsmc->cfg.cs_cfg[cs];

	for (i = 0; i < DSMC_MAX_SLAVE_NUM; i++) {
		mcr[i] = readl(dsmc->regs + DSMC_MCR(i));
		/* config to CR space */
		REG_CLRSETBITS(dsmc, DSMC_MCR(i),
			       (MCR_IOWIDTH_MASK << MCR_IOWIDTH_SHIFT) |
			       (MCR_CRT_MASK << MCR_CRT_SHIFT),
			       (MCR_IOWIDTH_X8 << MCR_IOWIDTH_SHIFT) |
			       (MCR_CRT_CR_SPACE << MCR_CRT_SHIFT));
	}
	if (cs_cfg->protcl == OPI_XCCELA_PSRAM) {
		/* Xccela psram init */
		uint8_t mr_tmp, rbxen;

		mr_tmp = xccela_read_mr(region_map, 0);
		tmp = cs_cfg->rd_latency - 3;
		mr_tmp = (mr_tmp & (~(XCCELA_MR0_RL_MASK << XCCELA_MR0_RL_SHIFT))) |
			 (tmp << XCCELA_MR0_RL_SHIFT);
		mr_tmp |= XCCELA_MR0_RL_TYPE_VARIABLE << XCCELA_MR0_RL_TYPE_SHIFT;
		xccela_write_mr(region_map, 0, mr_tmp);

		mr_tmp = xccela_read_mr(region_map, 4);
		latency = cs_cfg->wr_latency;
		if (latency == 3)
			tmp = 0;
		else if (latency == 5)
			tmp = 2;
		else if (latency == 7)
			tmp = 1;
		else
			tmp = latency;

		mr_tmp = (mr_tmp & (~(XCCELA_MR4_WL_MASK << XCCELA_MR4_WL_SHIFT))) |
			 (tmp << XCCELA_MR4_WL_SHIFT);
		/* set 0.5x refresh rate allow */
		mr_tmp = (mr_tmp & (~(XCCELA_MR4_REFRESH_MASK << XCCELA_MR4_REFRESH_SHIFT))) |
			 (XCCELA_MR4_0_5_REFRESH_RATE << XCCELA_MR4_REFRESH_SHIFT);

		xccela_write_mr(region_map, 4, mr_tmp);

		dsmc_cfg_latency(cs_cfg->rd_latency, cs_cfg->wr_latency, dsmc, cs);

		mr_tmp = xccela_read_mr(region_map, 3);
		if ((mr_tmp >> XCCELA_MR3_RBXEN_SHIFT) & XCCELA_MR3_RBXEN_MASK) {
			rbxen = 1;
			cs_cfg->rd_bdr_xfer_en = 0;
		} else {
			rbxen = 0;
		}

		mr_tmp = xccela_read_mr(region_map, 8);

		if (cs_cfg->io_width == MCR_IOWIDTH_X16) {
			mr_tmp |= XCCELA_MR8_IO_TYPE_X16 << XCCELA_MR8_IO_TYPE_SHIFT;
		} else {
			mr_tmp &= (~(XCCELA_MR8_IO_TYPE_MASK << XCCELA_MR8_IO_TYPE_SHIFT));
			mr_tmp |= XCCELA_MR8_IO_TYPE_X8 << XCCELA_MR8_IO_TYPE_SHIFT;
		}
		mr_tmp &= (~(XCCELA_MR8_BL_MASK << XCCELA_MR8_BL_SHIFT));
		if (cs_cfg->wrap_size == MCR_WRAPSIZE_8_CLK)
			mr_tmp |= (XCCELA_MR8_BL_8_CLK << XCCELA_MR8_BL_SHIFT);
		else if (cs_cfg->wrap_size == MCR_WRAPSIZE_16_CLK)
			mr_tmp |= (XCCELA_MR8_BL_16_CLK << XCCELA_MR8_BL_SHIFT);
		else if (cs_cfg->wrap_size == MCR_WRAPSIZE_32_CLK)
			mr_tmp |= (XCCELA_MR8_BL_32_CLK << XCCELA_MR8_BL_SHIFT);

		mr_tmp |= rbxen << XCCELA_MR8_RBX_EN_SHIFT;

		xccela_write_mr(region_map, 8, mr_tmp);
	} else {
		/* Hyper psram init */
		uint16_t cr_tmp;

		cr_tmp = hyper_read_mr(region_map, HYPER_PSRAM_IR0);
		if (((cr_tmp >> IR0_ROW_COUNT_SHIFT) & IR0_ROW_COUNT_MASK) ==
		    IR0_ROW_COUNT_128MBIT) {
			cs_cfg->rd_bdr_xfer_en = 1;
			cs_cfg->wr_bdr_xfer_en = 1;
		} else {
			cs_cfg->rd_bdr_xfer_en = 0;
			cs_cfg->wr_bdr_xfer_en = 0;
		}

		cr_tmp = hyper_read_mr(region_map, HYPER_PSRAM_CR0);

		latency = cs_cfg->wr_latency;
		if (latency == 3)
			tmp = 0xe;
		else if (latency == 4)
			tmp = 0xf;
		else
			tmp = latency - 5;
		cr_tmp = (cr_tmp & (~(CR0_INITIAL_LATENCY_MASK << CR0_INITIAL_LATENCY_SHIFT))) |
			 (tmp << CR0_INITIAL_LATENCY_SHIFT);

		cr_tmp = (cr_tmp & (~(CR0_BURST_LENGTH_MASK << CR0_BURST_LENGTH_SHIFT)));

		if (cs_cfg->wrap_size == MCR_WRAPSIZE_8_CLK)
			cr_tmp |= (CR0_BURST_LENGTH_8_CLK << CR0_BURST_LENGTH_SHIFT);
		else if (cs_cfg->wrap_size == MCR_WRAPSIZE_16_CLK)
			cr_tmp |= (CR0_BURST_LENGTH_16_CLK << CR0_BURST_LENGTH_SHIFT);
		else if (cs_cfg->wrap_size == MCR_WRAPSIZE_32_CLK)
			cr_tmp |= (CR0_BURST_LENGTH_32_CLK << CR0_BURST_LENGTH_SHIFT);

		hyper_write_mr(region_map, HYPER_PSRAM_CR0, cr_tmp);

		dsmc_cfg_latency(latency, latency, dsmc, cs);

		cr_tmp = hyper_read_mr(region_map, HYPER_PSRAM_CR1);
		cr_tmp = (cr_tmp & (~(CR1_CLOCK_TYPE_MASK << CR1_CLOCK_TYPE_SHIFT))) |
			 (CR1_CLOCK_TYPE_DIFF_CLK << CR1_CLOCK_TYPE_SHIFT);
		hyper_write_mr(region_map, HYPER_PSRAM_CR1, cr_tmp);
	}
	for (i = 0; i < DSMC_MAX_SLAVE_NUM; i++)
		/* config to memory space */
		writel(mcr[i], dsmc->regs + DSMC_MCR(i));

	return 0;
}


static int dsmc_psram_init(struct rockchip_dsmc *dsmc, uint32_t cs)
{
	uint32_t latency;
	struct device *dev = dsmc->dev;
	struct dsmc_config_cs *cs_cfg = &dsmc->cfg.cs_cfg[cs];
	uint32_t mhz = dsmc->cfg.freq_hz / MHZ;

	if (mhz <= 66) {
		latency = 3;
	} else if (mhz <= 100) {
		latency = 4;
	} else if (mhz <= 133) {
		latency = 5;
	} else if (mhz <= 166) {
		latency = 6;
	} else if (mhz <= 200) {
		latency = 7;
	} else {
		dev_err(dev, "PSRAM frequency do not support!\n");
		return -1;
	}

	cs_cfg->rd_latency = cs_cfg->wr_latency = latency;

	dsmc_psram_cfg(dsmc, cs);

	return 0;
}

static int dsmc_ctrller_cfg_for_psram(struct rockchip_dsmc *dsmc, uint32_t cs)
{
	int ret = 0;
	struct dsmc_config_cs *cs_cfg = &dsmc->cfg.cs_cfg[cs];

	REG_CLRSETBITS(dsmc, DSMC_MCR(cs),
		       MCR_DEVTYPE_MASK << MCR_DEVTYPE_SHIFT,
		       MCR_DEVTYPE_HYPERRAM << MCR_DEVTYPE_SHIFT);

	REG_CLRSETBITS(dsmc, DSMC_VDMC(cs),
		       (VDMC_MID_MASK << VDMC_MID_SHIFT) |
		       (VDMC_PROTOCOL_MASK << VDMC_PROTOCOL_SHIFT),
		       (cs_cfg->mid << VDMC_MID_SHIFT) |
		       (cs_cfg->protcl << VDMC_PROTOCOL_SHIFT));
	writel(cs_cfg->mtr_timing,
	       dsmc->regs + DSMC_MTR(cs));

	ret = dsmc_psram_init(dsmc, cs);

	REG_CLRSETBITS(dsmc, DSMC_MCR(cs),
		       (MCR_WRAPSIZE_MASK << MCR_WRAPSIZE_SHIFT),
		       (cs_cfg->wrap_size << MCR_WRAPSIZE_SHIFT));

	return ret;
}

static void dsmc_psram_remodify_timing(struct rockchip_dsmc *dsmc, uint32_t cs)
{
	uint32_t i;
	uint32_t max_length = 511, tcmd = 3;
	uint32_t tcsm, tmp;
	uint32_t mhz = dsmc->cfg.freq_hz / MHZ;
	struct dsmc_map *region_map = &dsmc->cs_map[cs].region_map[0];
	struct dsmc_config_cs *cs_cfg = &dsmc->cfg.cs_cfg[cs];

	if (cs_cfg->mid == APM_PSRAM) {
		/* for extended temp */
		if (region_map->size <= 0x400000)
			tcsm = DSMC_DEC_TCEM_2_5U;
		else if (region_map->size <= 0x1000000)
			tcsm = DSMC_DEC_TCEM_3U;
		else
			tcsm = DSMC_DEC_TCEM_0_5U;
	} else {
		tcsm = DSMC_DEV_TCSM_1U;
	}

	tmp = (tcsm * mhz + 999) / 1000;
	tmp = tmp - tcmd - 2 * cs_cfg->wr_latency - 4;

	if (tmp > max_length)
		tmp = max_length;

	for (i = 0; i < DSMC_MAX_SLAVE_NUM; i++)
		REG_CLRSETBITS(dsmc, DSMC_MCR(i),
			       MCR_IOWIDTH_MASK << MCR_IOWIDTH_SHIFT,
			       cs_cfg->io_width << MCR_IOWIDTH_SHIFT);

	REG_CLRSETBITS(dsmc, DSMC_MCR(cs),
		       (MCR_MAXEN_MASK << MCR_MAXEN_SHIFT) |
		       (MCR_MAXLEN_MASK << MCR_MAXLEN_SHIFT),
		       (MCR_MAX_LENGTH_EN << MCR_MAXEN_SHIFT) |
		       (tmp << MCR_MAXLEN_SHIFT));

	if (cs_cfg->io_width == MCR_IOWIDTH_X16)
		tmp = cs_cfg->col - 2;
	else
		tmp = cs_cfg->col - 1;
	REG_CLRSETBITS(dsmc, DSMC_BDRTCR(cs),
		       (BDRTCR_COL_BIT_NUM_MASK << BDRTCR_COL_BIT_NUM_SHIFT) |
		       (BDRTCR_WR_BDR_XFER_EN_MASK << BDRTCR_WR_BDR_XFER_EN_SHIFT) |
		       (BDRTCR_RD_BDR_XFER_EN_MASK << BDRTCR_RD_BDR_XFER_EN_SHIFT),
		       ((tmp - 6) << BDRTCR_COL_BIT_NUM_SHIFT) |
		       (cs_cfg->wr_bdr_xfer_en << BDRTCR_WR_BDR_XFER_EN_SHIFT) |
		       (cs_cfg->rd_bdr_xfer_en << BDRTCR_RD_BDR_XFER_EN_SHIFT));
}

static void dsmc_lb_dma_clear_s2h_intrupt(struct rockchip_dsmc *dsmc, uint32_t cs)
{
	int region, manual = 0;
	uint32_t mcr_tmp, rgn_attr_tmp;
	struct dsmc_map *map;
	struct dsmc_config_cs *cfg = &dsmc->cfg.cs_cfg[cs];

	region = find_attr_region(cfg, RGNX_ATTR_REG);
	if (region < 0) {
		manual = -1;
		region = 0;
	}

	if (manual) {
		mcr_tmp = readl(dsmc->regs + DSMC_MCR(cs));
		rgn_attr_tmp = readl(dsmc->regs + DSMC_RGN0_ATTR(cs));
		/* config to slave CSR space */
		REG_CLRSETBITS(dsmc, DSMC_MCR(cs),
			       MCR_CRT_MASK << MCR_CRT_SHIFT,
			       MCR_CRT_MEM_SPACE << MCR_CRT_SHIFT);
		REG_CLRSETBITS(dsmc, DSMC_RGN0_ATTR(cs),
			       RGNX_ATTR_MASK << RGNX_ATTR_SHIFT,
			       RGNX_ATTR_REG << RGNX_ATTR_SHIFT);
	}

	map = &dsmc->cs_map[cs].region_map[region];

	/* clear all s2h interrupt */
	writel(0x1 << S2H_INT_FOR_DMA_NUM,
	       map->virt + LBC_S2H_INT_STA);

	if (manual) {
		/* config to normal memory space */
		writel(mcr_tmp, dsmc->regs + DSMC_MCR(cs));
		writel(rgn_attr_tmp, dsmc->regs + DSMC_RGN0_ATTR(cs));
	}
}

static void data_cpu_to_dsmc_io(uint32_t *data)
{
	uint32_t byte0, byte3;

	byte0 = (*data >> 0) & 0xFF;
	byte3 = (*data >> 24) & 0xFF;

	*data &= 0x00FFFF00;

	*data |= (byte3 << 0);
	*data |= (byte0 << 24);
}

static int dsmc_dll_training_method(struct rockchip_dsmc_device *dsmc_dev, uint32_t cs,
				    uint32_t byte, uint32_t dll_num)
{
	uint32_t i, j;
	uint32_t data;
	uint32_t size = 0x100;
	const struct dsmc_ops *ops = dsmc_dev->ops;
	struct dsmc_config_cs *cfg = &dsmc_dev->dsmc.cfg.cs_cfg[cs];
	struct rockchip_dsmc *dsmc = &dsmc_dev->dsmc;
	struct dsmc_map *map = &dsmc_dev->dsmc.cs_map[cs].region_map[0];
	uint32_t pattern[] = {0x5aa5f00f, 0xffff0000};
	uint32_t mask;

	if (cfg->io_width == MCR_IOWIDTH_X8) {
		mask = 0xffffffff;
	} else {
		if (byte == 0)
			mask = 0x0000ffff;
		else
			mask = 0xffff0000;
	}

	REG_CLRSETBITS(dsmc, DSMC_RDS_DLL_CTL(cs, byte),
		       RDS_DLL0_CTL_RDS_0_CLK_DELAY_NUM_MASK,
		       dll_num << RDS_DLL0_CTL_RDS_0_CLK_DELAY_NUM_SHIFT);
	for (j = 0; j < ARRAY_SIZE(pattern); j++) {
		data_cpu_to_dsmc_io(&pattern[j]);
		for (i = 0; i < size; i += 4)
			ops->write(dsmc_dev, cs, 0, i, (pattern[j] + i) & mask);

#ifdef CONFIG_ARM64
		dcache_clean_inval_poc((unsigned long)map->virt, (unsigned long)(map->virt + size));
#else
		dmac_flush_range(map->virt, map->virt + size);
#endif

		for (i = 0; i < size; i += 4) {
			ops->read(dsmc_dev, cs, 0, i, &data);
			if (data != ((pattern[j] + i) & mask))
				return -EINVAL;
		}
	}
	return 0;
}

int rockchip_dsmc_dll_training(struct rockchip_dsmc_device *priv)
{
	uint32_t cs, byte, dir;
	uint32_t dll_max = 0xff, dll_min = 0;
	int dll, dll_step = 10;
	struct dsmc_config_cs *cfg;
	struct rockchip_dsmc *dsmc = &priv->dsmc;
	struct device *dev = dsmc->dev;
	int ret;

	for (cs = 0; cs < DSMC_MAX_SLAVE_NUM; cs++) {
		cfg = &priv->dsmc.cfg.cs_cfg[cs];
		if (cfg->device_type == DSMC_UNKNOWN_DEVICE)
			continue;
		for (byte = MCR_IOWIDTH_X8; byte <= cfg->io_width; byte++) {
			dir = 0;
			dll = cfg->dll_num[byte];
			while ((dll >= 0x0 && dll <= 0xff)) {
				ret = dsmc_dll_training_method(priv, cs, byte, dll);
				if (ret) {
					if (dir)
						dll_max = dll - dll_step;
					else
						dll_min = dll + dll_step;
					dll = cfg->dll_num[byte];
					dir++;
				} else if (((dll + dll_step) > 0xff) || ((dll - dll_step) < 0)) {
					if (dir)
						dll_max = dll;
					else
						dll_min = dll;
					dll = cfg->dll_num[byte];
					dir++;
				}
				if (dir > 1)
					break;
				if (dir)
					dll += dll_step;
				else
					dll -= dll_step;
			}
			dll = (dll_max + dll_min) / 2;
			if ((dll >= 0xff) || (dll <= 0)) {
				dev_err(dev, "DSMC: cs%d byte%d dll training error(0x%x)\n",
				       cs, byte, dll);
				return -1;
			}
			dev_info(dev, "cs%d byte%d dll delay line result 0x%x\n", cs, byte, dll);
			REG_CLRSETBITS(dsmc, DSMC_RDS_DLL_CTL(cs, byte),
				       RDS_DLL0_CTL_RDS_0_CLK_DELAY_NUM_MASK,
				       dll << RDS_DLL0_CTL_RDS_0_CLK_DELAY_NUM_SHIFT);
		}
	}

	return 0;
}
EXPORT_SYMBOL(rockchip_dsmc_dll_training);

void rockchip_dsmc_lb_dma_hw_mode_dis(struct rockchip_dsmc *dsmc)
{
	uint32_t cs = dsmc->xfer.ops_cs;

	/* clear dsmc interrupt */
	writel(INT_STATUS(cs), dsmc->regs + DSMC_INT_STATUS);
	/* disable dma request */
	writel(DMA_REQ_DIS(cs), dsmc->regs + DSMC_DMA_EN);

	dsmc_lb_dma_clear_s2h_intrupt(dsmc, cs);
}
EXPORT_SYMBOL(rockchip_dsmc_lb_dma_hw_mode_dis);

int rockchip_dsmc_lb_dma_trigger_by_host(struct rockchip_dsmc *dsmc, uint32_t cs)
{
	int region, manual = 0;
	uint32_t mcr_tmp, rgn_attr_tmp, flag_tmp;
	struct dsmc_map *map;
	struct dsmc_config_cs *cfg = &dsmc->cfg.cs_cfg[cs];

	region = find_attr_region(cfg, RGNX_ATTR_REG);
	if (region < 0) {
		manual = -1;
		region = 0;
	}

	if (manual) {
		mcr_tmp = readl(dsmc->regs + DSMC_MCR(cs));
		rgn_attr_tmp = readl(dsmc->regs + DSMC_RGN0_ATTR(cs));
		/* config to slave CSR space */
		REG_CLRSETBITS(dsmc, DSMC_MCR(cs),
			       (MCR_CRT_MASK << MCR_CRT_SHIFT),
			       (MCR_CRT_MEM_SPACE << MCR_CRT_SHIFT));
		REG_CLRSETBITS(dsmc, DSMC_RGN0_ATTR(cs),
			       RGNX_ATTR_MASK << RGNX_ATTR_SHIFT,
			       RGNX_ATTR_REG << RGNX_ATTR_SHIFT);
	}
	map = &dsmc->cs_map[cs].region_map[region];
	/*
	 * write (readl(LBC_CON(15)) + 1) to LBC_CON15 to slave which will
	 * write APP_CON(S2H_INT_FOR_DMA_NUM) trigger a slave to host interrupt
	 */
	flag_tmp = readl(map->virt + LBC_CON(15));

	writel(flag_tmp + 1, map->virt + LBC_CON(15));

	if (manual) {
		/* config to normal memory space */
		writel(mcr_tmp, dsmc->regs + DSMC_MCR(cs));
		writel(rgn_attr_tmp, dsmc->regs + DSMC_RGN0_ATTR(cs));
	}

	return 0;
}
EXPORT_SYMBOL(rockchip_dsmc_lb_dma_trigger_by_host);

int rockchip_dsmc_device_dectect(struct rockchip_dsmc *dsmc, uint32_t cs)
{
	int ret = 0;

	rockchip_dsmc_ctrller_init(dsmc, cs);
	ret = dsmc_psram_dectect(dsmc, cs);
	if (ret)
		return ret;

	return ret;
}
EXPORT_SYMBOL(rockchip_dsmc_device_dectect);

static void xccela_psram_reset(struct rockchip_dsmc *dsmc, uint32_t cs)
{
	struct dsmc_map *region_map = &dsmc->cs_map[cs].region_map[0];

	REG_CLRSETBITS(dsmc, DSMC_VDMC(cs),
		       (VDMC_RESET_CMD_MODE_MASK << VDMC_RESET_CMD_MODE_SHIFT),
		       (0x1 << VDMC_RESET_CMD_MODE_SHIFT));
	xccela_write_mr(region_map, XCCELA_PSRAM_MR(0), XCCELA_PSRAM_MR_SET(0x0));
	udelay(200);
	REG_CLRSETBITS(dsmc, DSMC_VDMC(cs),
		       (VDMC_RESET_CMD_MODE_MASK << VDMC_RESET_CMD_MODE_SHIFT),
		       (0x0 << VDMC_RESET_CMD_MODE_SHIFT));
}

int rockchip_dsmc_psram_reinit(struct rockchip_dsmc *dsmc, uint32_t cs)
{
	int ret = 0;

	if (dsmc->cfg.cs_cfg[cs].protcl == OPI_XCCELA_PSRAM)
		xccela_psram_reset(dsmc, cs);
	ret = dsmc_ctrller_cfg_for_psram(dsmc, cs);
	dsmc_psram_remodify_timing(dsmc, cs);

	return ret;
}
EXPORT_SYMBOL(rockchip_dsmc_psram_reinit);

int rockchip_dsmc_ctrller_init(struct rockchip_dsmc *dsmc, uint32_t cs)
{
	uint32_t i;
	struct dsmc_config_cs *cfg = &dsmc->cfg.cs_cfg[cs];

	writel(MRGTCR_READ_WRITE_MERGE_EN,
	       dsmc->regs + DSMC_MRGTCR(cs));
	writel((0x1 << RDS_DLL0_CTL_RDS_0_CLK_SMP_SEL_SHIFT) |
	       (cfg->dll_num[0] << RDS_DLL0_CTL_RDS_0_CLK_DELAY_NUM_SHIFT),
	       dsmc->regs + DSMC_RDS_DLL0_CTL(cs));
	writel((0x1 << RDS_DLL1_CTL_RDS_1_CLK_SMP_SEL_SHIFT) |
	       (cfg->dll_num[1] << RDS_DLL1_CTL_RDS_1_CLK_DELAY_NUM_SHIFT),
	       dsmc->regs + DSMC_RDS_DLL1_CTL(cs));

	/* all io_width should set the same value in diff cs */
	for (i = 0; i < DSMC_MAX_SLAVE_NUM; i++)
		REG_CLRSETBITS(dsmc, DSMC_MCR(cs),
			       MCR_IOWIDTH_MASK << MCR_IOWIDTH_SHIFT,
			       cfg->io_width << MCR_IOWIDTH_SHIFT);

	REG_CLRSETBITS(dsmc, DSMC_MCR(cs),
		       (MCR_ACS_MASK << MCR_ACS_SHIFT) |
		       (MCR_DEVTYPE_MASK << MCR_DEVTYPE_SHIFT) |
		       (MCR_EXCLUSIVE_DQS_MASK << MCR_EXCLUSIVE_DQS_SHIFT) |
		       (MCR_WRAPSIZE_MASK << MCR_WRAPSIZE_SHIFT) |
		       (MCR_MAXEN_MASK << MCR_MAXEN_SHIFT) |
		       (MCR_MAXLEN_MASK << MCR_MAXLEN_SHIFT),
		       (cfg->acs << MCR_ACS_SHIFT) |
		       (MCR_DEVTYPE_HYPERRAM << MCR_DEVTYPE_SHIFT) |
		       (cfg->exclusive_dqs << MCR_EXCLUSIVE_DQS_SHIFT) |
		       (cfg->wrap_size << MCR_WRAPSIZE_SHIFT) |
		       (cfg->max_length_en << MCR_MAXEN_SHIFT) |
		       (cfg->max_length << MCR_MAXLEN_SHIFT));

	writel(cfg->wrap2incr_en, dsmc->regs + DSMC_WRAP2INCR(cs));

	REG_CLRSETBITS(dsmc, DSMC_VDMC(cs),
		       (VDMC_LATENCY_FIXED_MASK << VDMC_LATENCY_FIXED_SHIFT) |
		       (VDMC_PROTOCOL_MASK << VDMC_PROTOCOL_SHIFT),
		       (VDMC_LATENCY_VARIABLE << VDMC_LATENCY_FIXED_SHIFT) |
		       (cfg->device_type << VDMC_PROTOCOL_SHIFT));
	writel(cap_2_dev_size(dsmc->cfg.cap), dsmc->regs + DSMC_DEV_SIZE);

	return 0;
}
EXPORT_SYMBOL(rockchip_dsmc_ctrller_init);

int rockchip_dsmc_lb_init(struct rockchip_dsmc *dsmc, uint32_t cs)
{
	int ret = 0;

	dsmc_ctrller_cfg_for_lb(dsmc, cs);
	ret = dsmc_lb_cmn_config(dsmc, cs);
	if (ret)
		return ret;
	dsmc_lb_csr_config(dsmc, cs);

	return ret;
}
EXPORT_SYMBOL(rockchip_dsmc_lb_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhihuan He <huan.he@rock-chips.com>");
MODULE_DESCRIPTION("ROCKCHIP DSMC controller driver");
