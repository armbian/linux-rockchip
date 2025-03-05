/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */
#ifndef __ROCKCHIP_DSMC_HOST_H
#define __ROCKCHIP_DSMC_HOST_H

#define DSMC_FPGA_WINBOND_X8				0
#define DSMC_FPGA_WINBOND_X16				1
#define DSMC_FPGA_APM_X8				0
#define DSMC_FPGA_APM_X16				0

#define DSMC_FPGA_DMAC_TEST				1

#define DSMC_MAX_SLAVE_NUM				4
#define DSMC_LB_MAX_RGN					4

#define DSMC_MEM_ATTRIBUTE_NO_CACHE			0
#define DSMC_MEM_ATTRIBUTE_CACHE			1
#define DSMC_MEM_ATTRIBUTE_WR_COM			2

#define DSMC_MAP_UNCACHE_SIZE				(128 * 1024)
#define DSMC_MAP_BUFFERED_SIZE				(128 * 1024)

/* DSMC register */
#define DSMC_VER					0x0000
#define DSMC_CSR					0x0008
#define DSMC_TAR					0x0010
#define DSMC_AXICTL					0x0014
#define DSMC_CLK_MD					0x0020
#define DSMC_DLL_DBG_CTRL				0x0028
#define DSMC_DEV_SIZE					0x0030
#define DSMC_INT_EN					0x0040
#define DSMC_INT_STATUS					0x0044
#define DSMC_INT_MASK					0x0048
#define DSMC_DMA_EN					0x0050
#define DSMC_DMA_REQ_NUM(n)				(0x0054 + (0x4 * (n)))
#define DSMC_DMA_MUX					0x005c
#define DSMC_VDMC(n)					(0x1000 * ((n) + 1))
#define DSMC_MCR(n)					(0x1000 * ((n) + 1) + 0x10)
#define DSMC_MTR(n)					(0x1000 * ((n) + 1) + 0x14)
#define DSMC_BDRTCR(n)					(0x1000 * ((n) + 1) + 0x20)
#define DSMC_MRGTCR(n)					(0x1000 * ((n) + 1) + 0x24)
#define DSMC_WRAP2INCR(n)				(0x1000 * ((n) + 1) + 0x28)
#define DSMC_RDS_DLL0_CTL(n)				(0x1000 * ((n) + 1) + 0x30)
#define DSMC_RDS_DLL1_CTL(n)				(0x1000 * ((n) + 1) + 0x34)
#define DSMC_SLV_RGN_DIV(n)				(0x1000 * ((n) + 1) + 0x40)
#define DSMC_RGN0_ATTR(n)				(0x1000 * ((n) + 1) + 0x50)
#define DSMC_RGN1_ATTR(n)				(0x1000 * ((n) + 1) + 0x54)
#define DSMC_RGN2_ATTR(n)				(0x1000 * ((n) + 1) + 0x58)
#define DSMC_RGN3_ATTR(n)				(0x1000 * ((n) + 1) + 0x5c)

#define DSMC_RDS_DLL_CTL(cs, byte)			(0x1000 * ((cs) + 1) + 0x30 + (byte) * 0x4)

/* AXICTL */
#define AXICTL_RD_NO_ERR_SHIFT				8
#define AXICTL_RD_NO_ERR_MASK				0x1

/* INT_EN */
#define INT_EN_SHIFT					0
#define INT_EN_MASK(cs)					(0x1 << (cs))
#define INT_EN(cs)					(0x1 << (cs))

/* INT_STATUS */
#define INT_STATUS_SHIFT				0
#define INT_STATUS_MASK(cs)				(0x1 << (cs))
#define INT_STATUS(cs)					(0x1 << (cs))

/* INT_MASK */
#define INT_MASK(cs)					(0x1 << (cs))
#define INT_UNMASK(cs)					(0x0 << (cs))

/* DMA_EN */
#define DMA_REQ_EN_SHIFT				0
#define DMA_REQ_EN_MASK					0x1
#define DMA_REQ_EN(cs)					(0x1 << (cs))
#define DMA_REQ_DIS(cs)					(0x0 << (cs))

/* DSMC_DMA_MUX */
#define DMA_REQ_MUX_MASK(req)				(0x3 << ((req) * 4))
#define DMA_REQ_MUX(req, n)				(((n) & 0x3) << ((req) * 4))

/* VDMC */
#define VDMC_MID_SHIFT					0
#define VDMC_MID_MASK					0xF
#define VDMC_PROTOCOL_SHIFT				4
#define VDMC_PROTOCOL_MASK				0xF
#define VDMC_RESET_CMD_MODE_SHIFT			8
#define VDMC_RESET_CMD_MODE_MASK			0x1
#define VDMC_LATENCY_FIXED_SHIFT			9
#define VDMC_LATENCY_FIXED_MASK				0x1
#define VDMC_LATENCY_VARIABLE				0
#define VDMC_LATENCY_FIXED				1

#define DSMC_UNKNOWN_DEVICE				0x0
#define OPI_XCCELA_PSRAM				0x1
#define HYPERBUS_PSRAM					0x2
#define DSMC_LB_DEVICE					0x3

/* RDS_DLL0_CTL */
#define RDS_DLL0_CTL_RDS_0_CLK_DELAY_NUM_SHIFT		0
#define RDS_DLL0_CTL_RDS_0_CLK_DELAY_NUM_MASK		0xff
#define RDS_DLL0_CTL_RDS_0_CLK_SMP_SEL_SHIFT		31
/* RDS_DLL1_CTL */
#define RDS_DLL1_CTL_RDS_1_CLK_DELAY_NUM_SHIFT		0
#define RDS_DLL0_CTL_RDS_0_CLK_DELAY_NUM_MASK		0xff
#define RDS_DLL1_CTL_RDS_1_CLK_SMP_SEL_SHIFT		31

/* MCR */
#define MCR_WRAPSIZE_SHIFT				0
#define MCR_WRAPSIZE_MASK				0x3
#define MCR_WRAPSIZE_32_CLK				1
#define MCR_WRAPSIZE_8_CLK				2
#define MCR_WRAPSIZE_16_CLK				3

#define MCR_EXCLUSIVE_DQS_SHIFT				2
#define MCR_EXCLUSIVE_DQS_MASK				0x1
#define MCR_IOWIDTH_SHIFT				3
#define MCR_IOWIDTH_MASK				0x1
#define MCR_DEVTYPE_SHIFT				4
#define MCR_DEVTYPE_MASK				0x1
#define MCR_CRT_SHIFT					5
#define MCR_CRT_MASK					0x1
#define MCR_ACS_SHIFT					16
#define MCR_ACS_MASK					0x1
#define MCR_TCMO_SHIFT					17
#define MCR_TCMO_MASK					0x1
#define MCR_MAXLEN_SHIFT				18
#define MCR_MAXLEN_MASK					0x1FF
#define MCR_MAXEN_SHIFT					31
#define MCR_MAXEN_MASK					0x1

#define MCR_CRT_CR_SPACE				0x1
#define MCR_CRT_MEM_SPACE				0x0
#define MCR_IOWIDTH_X16					0x1
#define MCR_IOWIDTH_X8					0x0
#define MCR_DEVTYPE_HYPERRAM				0x1
#define MCR_MAX_LENGTH_EN				0x1
#define MCR_MAX_LENGTH					0x1ff

/* BDRTCR */
#define BDRTCR_COL_BIT_NUM_SHIFT			0
#define BDRTCR_COL_BIT_NUM_MASK				0x7
#define BDRTCR_WR_BDR_XFER_EN_SHIFT			4
#define BDRTCR_WR_BDR_XFER_EN_MASK			0x1
#define BDRTCR_WR_BDR_XFER_EN				1
#define BDRTCR_RD_BDR_XFER_EN_SHIFT			5
#define BDRTCR_RD_BDR_XFER_EN_MASK			0x1
#define BDRTCR_RD_BDR_XFER_EN				1

/* MRGTCR */
#define MRGTCR_READ_WRITE_MERGE_EN			0x3

/* MTR */
#define MTR_WLTCY_SHIFT					0
#define MTR_WLTCY_MASK					0xf
#define MTR_RLTCY_SHIFT					4
#define MTR_RLTCY_MASK					0xf
#define MTR_WCSH_SHIFT					8
#define MTR_RCSH_SHIFT					12
#define MTR_WCSS_SHIFT					16
#define MTR_RCSS_SHIFT					20
#define MTR_WCSHI_SHIFT					24
#define MTR_RCSHI_SHIFT					28

/* RGNX_ATTR */
#define RGNX_ATTR_SHIFT					0
#define RGNX_ATTR_MASK					0x3
#define RGNX_ATTR_REG					0x0
#define RGNX_ATTR_DPRA					0x1
#define RGNX_ATTR_NO_MERGE_FIFO				0x2
#define RGNX_ATTR_MERGE_FIFO				0x3
#define RGNX_ATTR_CTRL_SHIFT				4
#define RGNX_ATTR_BE_CTRLED_SHIFT			5
#define RGNX_ATTR_DUM_CLK_EN_SHIFT			6
#define RGNX_ATTR_DUM_CLK_NUM_SHIFT			7
#define RGNX_ATTR_CA_ADDR_MASK				1
#define RGNX_ATTR_32BIT_ADDR_WIDTH			0
#define RGNX_ATTR_16BIT_ADDR_WIDTH			1
#define RGNX_ATTR_ADDR_WIDTH_SHIFT			8

#define RGNX_STATUS_ENABLED				(1)
#define RGNX_STATUS_DISABLED				(0)

#define MTR_CFG(RCSHI, WCSHI, RCSS, WCSS, RCSH, WCSH, RLTCY, WLTCY)	\
	(((RCSHI) << MTR_RCSHI_SHIFT) |					\
	 ((WCSHI) << MTR_WCSHI_SHIFT) |					\
	 ((RCSS) << MTR_RCSS_SHIFT) |					\
	 ((WCSS) << MTR_WCSS_SHIFT) |					\
	 ((RCSH) << MTR_RCSH_SHIFT) |					\
	 ((WCSH) << MTR_WCSH_SHIFT) |					\
	 ((RLTCY) << MTR_RLTCY_SHIFT) |					\
	 ((WLTCY) << MTR_WLTCY_SHIFT))

#define APM_PSRAM_LATENCY_FIXED				0x1
#define APM_PSRAM_LATENCY_VARIABLE			0x0

#define DSMC_BURST_WRAPSIZE_32CLK			0x1
#define DSMC_BURST_WRAPSIZE_8CLK			0x2
#define DSMC_BURST_WRAPSIZE_16CLK			0x3

#define DSMC_DLL_EN					0x1

#define HYPER_PSRAM_IR0					(0x00)
#define HYPER_PSRAM_IR1					(0x02)
#define HYPER_PSRAM_CR0					(0x1000)
#define HYPER_PSRAM_CR1					(0x1002)
#define XCCELA_PSRAM_MR(n)				(2 * (n))
#define XCCELA_PSRAM_MR_GET(n)				(((n) >> 8) & 0xff)
#define XCCELA_PSRAM_MR_SET(n)				(((n) & 0xff) << 8)
/* device id bit mask */
#define HYPERBUS_DEV_ID_MASK				(0xf)
#define IR0_ROW_COUNT_128MBIT				(0xd)
#define IR0_ROW_COUNT_SHIFT				(0x8)
#define IR0_ROW_COUNT_MASK				(0x1f)
#define IR0_COL_COUNT_SHIFT				(0x4)
#define IR0_COL_COUNT_MASK				(0xf)
#define IR1_DEV_IO_WIDTH_SHIFT				(0)
#define IR1_DEV_IO_WIDTH_MASK				(0xf)
#define IR1_DEV_IO_WIDTH_X16				(0x9)

#define CR0_INITIAL_LATENCY_SHIFT			4
#define CR0_INITIAL_LATENCY_MASK			0xf
#define CR0_FIXED_LATENCY_ENABLE_SHIFT			3
#define CR0_FIXED_LATENCY_ENABLE_MASK			0x1
#define CR0_FIXED_LATENCY_ENABLE_VARIABLE_LATENCY	0x0
#define CR0_FIXED_LATENCY_ENABLE_FIXED_LATENCY		0x1

#define CR0_BURST_LENGTH_SHIFT				0
#define CR0_BURST_LENGTH_MASK				0x3
#define CR0_BURST_LENGTH_64_CLK				0x0
#define CR0_BURST_LENGTH_32_CLK				0x1
#define CR0_BURST_LENGTH_8_CLK				0x2
#define CR0_BURST_LENGTH_16_CLK				0x3

#define CR1_CLOCK_TYPE_SHIFT				6
#define CR1_CLOCK_TYPE_MASK				0x1
#define CR1_CLOCK_TYPE_SINGLE_CLK			0x1
#define CR1_CLOCK_TYPE_DIFF_CLK				0x0

#define XCCELA_DEV_ID_MASK				(0x1f)

#define XCCELA_MR0_RL_SHIFT				(2)
#define XCCELA_MR0_RL_MASK				(0x7)
#define XCCELA_MR0_RL_TYPE_SHIFT			(5)
#define XCCELA_MR0_RL_TYPE_MASK				(0x1)
#define XCCELA_MR0_RL_TYPE_FIXED			(0x1)
#define XCCELA_MR0_RL_TYPE_VARIABLE			(0x0)

#define XCCELA_MR2_DEV_DENSITY_MASK			(0x7)

#define XCCELA_MR3_RBXEN_MASK				(1)
#define XCCELA_MR3_RBXEN_SHIFT				(7)

#define XCCELA_MR4_WL_SHIFT				(5)
#define XCCELA_MR4_WL_MASK				(0x7)
#define XCCELA_MR4_REFRESH_SHIFT			(3)
#define XCCELA_MR4_REFRESH_MASK				(0x3)
#define XCCELA_MR4_0_5_REFRESH_RATE			(0x3)

#define XCCELA_MR8_IO_TYPE_SHIFT			(6)
#define XCCELA_MR8_IO_TYPE_MASK				(0x1)
#define XCCELA_MR8_IO_TYPE_X16				(0x1)
#define XCCELA_MR8_IO_TYPE_X8				(0x0)
#define XCCELA_MR8_RBX_EN_SHIFT				(3)
#define XCCELA_MR8_RBX_EN_MASK				(0x1)
#define XCCELA_MR8_RBX_EN				(0x1)
#define XCCELA_MR8_BL_SHIFT				(0)
#define XCCELA_MR8_BL_MASK				(0x7)
#define XCCELA_MR8_BL_32_CLK				(0x2)
#define XCCELA_MR8_BL_16_CLK				(0x1)
#define XCCELA_MR8_BL_8_CLK				(0x0)

#define PSRAM_SIZE_32MBYTE				(0x02000000)
#define PSRAM_SIZE_16MBYTE				(0x01000000)
#define PSRAM_SIZE_8MBYTE				(0x00800000)
#define PSRAM_SIZE_4MBYTE				(0x00400000)

/* TCSM/TCEM */
#define DSMC_DEV_TCSM_4U				(4000)
#define DSMC_DEV_TCSM_1U				(1000)
#define DSMC_DEC_TCEM_2_5U				(2500)
#define DSMC_DEC_TCEM_3U				(3000)
#define DSMC_DEC_TCEM_0_5U				(500)

#define RK3506_GRF_SOC_CON(n)				(0x4 * (n))
#define GRF_DSMC_REQ0_SEL(n)				((0x1 << (15 + 16)) | ((n) << 15))
#define GRF_DSMC_REQ1_SEL(n)				((0x1 << (14 + 16)) | ((n) << 14))
#define GRF_DMAC0_CH10_SEL(n)				((0x1 << (7 + 16)) | ((n) << 7))
#define GRF_DMAC0_CH8_SEL(n)				((0x1 << (6 + 16)) | ((n) << 6))
#define GRF_DMAC0_CH3_SEL(n)				((0x1 << (3 + 16)) | ((n) << 3))
#define GRF_DMAC0_CH2_SEL(n)				((0x1 << (2 + 16)) | ((n) << 2))

#define RK3576_TPO_IOC_OFFSET				(0x4000)
#define RK3576_GPIO3A_IOMUX_SEL_H			(RK3576_TPO_IOC_OFFSET + 0x64)
#define RK3576_GPIO3B_IOMUX_SEL_L			(RK3576_TPO_IOC_OFFSET + 0x68)
#define RK3576_GPIO3B_IOMUX_SEL_H			(RK3576_TPO_IOC_OFFSET + 0x6c)
#define RK3576_GPIO3C_IOMUX_SEL_L			(RK3576_TPO_IOC_OFFSET + 0x70)
#define RK3576_GPIO3C_IOMUX_SEL_H			(RK3576_TPO_IOC_OFFSET + 0x74)
#define RK3576_GPIO3D_IOMUX_SEL_L			(RK3576_TPO_IOC_OFFSET + 0x78)
#define RK3576_GPIO3D_IOMUX_SEL_H			(RK3576_TPO_IOC_OFFSET + 0x7c)
#define RK3576_GPIO4A_IOMUX_SEL_L			(RK3576_TPO_IOC_OFFSET + 0x80)

#define RK3576_IOMUX_SEL(v, s)				(((v) << (s)) | (0xf << ((s) + 16)))

#define RK3506_GPIO1A_IOMUX_SEL_0_OFFSET		(0x20)
#define RK3506_GPIO1A_IOMUX_SEL_1_OFFSET		(0x24)
#define RK3506_GPIO1B_IOMUX_SEL_0_OFFSET		(0x28)
#define RK3506_GPIO1B_IOMUX_SEL_1_OFFSET		(0x2c)
#define RK3506_GPIO1C_IOMUX_SEL_0_OFFSET		(0x30)
#define RK3506_GPIO1C_IOMUX_SEL_1_OFFSET		(0x34)
#define RK3506_GPIO1D_IOMUX_SEL_0_OFFSET		(0x38)

#define RK3506_IOMUX_SEL(v, s)				(((v) << (s)) | (0xf << ((s) + 16)))

struct regions_config {
	uint32_t attribute;
	uint32_t ca_addr_width;
	uint32_t dummy_clk_num;
	uint32_t cs0_be_ctrled;
	uint32_t cs0_ctrl;
	uint32_t offset_range[2];
	uint32_t status;
};

struct dsmc_config_cs {
	uint16_t mid;
	uint16_t protcl;
	uint32_t device_type;
	uint32_t mtr_timing;
	uint32_t acs;
	uint32_t exclusive_dqs;
	uint32_t io_width;
	uint32_t wrap_size;
	uint32_t rcshi;
	uint32_t wcshi;
	uint32_t rcss;
	uint32_t wcss;
	uint32_t rcsh;
	uint32_t wcsh;
	uint32_t rd_latency;
	uint32_t wr_latency;
	uint32_t col;
	uint32_t wrap2incr_en;
	uint32_t max_length_en;
	uint32_t max_length;
	uint32_t rgn_num;
	uint32_t dll_num[2];
	uint32_t rd_bdr_xfer_en;
	uint32_t wr_bdr_xfer_en;
	uint32_t int_en;
	struct regions_config slv_rgn[DSMC_LB_MAX_RGN];
};

struct dsmc_ctrl_config {
	uint32_t clk_mode;
	uint32_t freq_hz;
	uint32_t ctrl_freq_hz;
	uint32_t cap;
	uint32_t dma_req_mux_offset;
	struct dsmc_config_cs cs_cfg[DSMC_MAX_SLAVE_NUM];
};

struct dsmc_map {
	void *virt;
	phys_addr_t phys;
	size_t size;
};

struct dsmc_cs_map {
	struct dsmc_map region_map[DSMC_LB_MAX_RGN];
};

struct dsmc_transfer {
	uint32_t ops_cs;
	struct dma_chan *dma_chan;
	dma_addr_t src_addr;
	dma_addr_t dst_addr;
	size_t transfer_size;
	u8 brst_size;
	u8 brst_len;
	atomic_t state;
};

struct rockchip_dsmc {
	/* Hardware resources */
	void __iomem *regs;
	struct regmap *grf;
	struct clk *aclk_root;
	struct clk *aclk;
	struct clk *pclk;
	struct clk *clk_sys;
	struct device *dev;

	struct dma_chan *dma_req[DSMC_MAX_SLAVE_NUM];

	struct dsmc_transfer xfer;

	struct reset_control *areset;
	struct reset_control *preset;

	struct dsmc_cs_map cs_map[DSMC_MAX_SLAVE_NUM];
	struct dsmc_ctrl_config cfg;
};

struct rockchip_dsmc_device {
	struct dsmc_ops *ops;
	struct rockchip_dsmc dsmc;
};

struct dsmc_ops {
	int (*read)(struct rockchip_dsmc_device *dsmc_dev,
		    uint32_t cs, uint32_t region,
		    unsigned long addr, uint32_t *data);
	int (*write)(struct rockchip_dsmc_device *dsmc_dev,
		     uint32_t cs, uint32_t region,
		     unsigned long addr, uint32_t val);

	int (*copy_from)(struct rockchip_dsmc_device *dsmc_dev,
			 uint32_t cs, uint32_t region, uint32_t from,
			 dma_addr_t dst_phys, size_t size);
	int (*copy_to)(struct rockchip_dsmc_device *dsmc_dev,
		       uint32_t cs, uint32_t region, dma_addr_t src_phys,
		       uint32_t to, size_t size);
	int (*copy_from_state)(struct rockchip_dsmc_device *dsmc_dev);
	int (*copy_to_state)(struct rockchip_dsmc_device *dsmc_dev);
};

int rockchip_dsmc_ctrller_init(struct rockchip_dsmc *dsmc, uint32_t cs);
int rockchip_dsmc_device_dectect(struct rockchip_dsmc *dsmc, uint32_t cs);
int rockchip_dsmc_dll_training(struct rockchip_dsmc_device *priv);
struct rockchip_dsmc_device *rockchip_dsmc_find_device_by_compat(const char *compat);
const char *rockchip_dsmc_get_compat(int index);
int rockchip_dsmc_lb_class_create(const char *name);
int rockchip_dsmc_lb_class_destroy(void);
void rockchip_dsmc_lb_dma_hw_mode_dis(struct rockchip_dsmc *dsmc);
int rockchip_dsmc_lb_dma_trigger_by_host(struct rockchip_dsmc *dsmc, uint32_t cs);
int rockchip_dsmc_lb_init(struct rockchip_dsmc *dsmc, uint32_t cs);
int rockchip_dsmc_psram_reinit(struct rockchip_dsmc *dsmc, uint32_t cs);
int rockchip_dsmc_register_lb_device(struct device *dev, uint32_t cs);
int rockchip_dsmc_unregister_lb_device(struct device *dev, uint32_t cs);

#endif /* __BUS_ROCKCHIP_ROCKCHIP_DSMC_HOST_H */
