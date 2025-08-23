/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 */


#ifndef _DT_BINDINGS_ROCKCHIP_VOP_H
#define _DT_BINDINGS_ROCKCHIP_VOP_H

#define	ROCKCHIP_VOP2_SHARED_MODE_PRIMARY	1
#define	ROCKCHIP_VOP2_SHARED_MODE_SECONDARY	2

#define	ROCKCHIP_VOP_VP0	0
#define	ROCKCHIP_VOP_VP1	1
#define	ROCKCHIP_VOP_VP2	2
#define	ROCKCHIP_VOP_VP3	3

#define	ROCKCHIP_VOP2_CLUSTER0	0
#define	ROCKCHIP_VOP2_CLUSTER1	1
#define	ROCKCHIP_VOP2_ESMART0	2
#define	ROCKCHIP_VOP2_ESMART1	3
#define	ROCKCHIP_VOP2_SMART0	4
#define	ROCKCHIP_VOP2_SMART1	5
#define	ROCKCHIP_VOP2_CLUSTER2	6
#define	ROCKCHIP_VOP2_CLUSTER3	7
#define	ROCKCHIP_VOP2_ESMART2	8
#define	ROCKCHIP_VOP2_ESMART3	9
#define	ROCKCHIP_VOP2_CURSOR0	10
#define	ROCKCHIP_VOP2_CURSOR1	11

#define	ROCKCHIP_VOP2_PHY_ID_INVALID	-1

/*
 * FBD: Fast Boot Display
 *
 * ROCKCHIP_DRM_FBD_FROM_UBOOT:
 *     show logo.bmp from uboot and show logo_kernel.bmp after enter kernel;
 * ROCKCHIP_DRM_FBD_FROM_UBOOT_TO_RTOS:
 *     crtc/connector/panel will be init at uboot, and update plane at rtos;
 * ROCKCHIP_DRM_FBD_FROM_RTOS:
 *     crtc/connector/panel will be init at rtos, uboot no need to do any hardware
 *     config, but need to pass the logic state to kernel to ensure pd/clk/drm
 *     state is continuous.
 */
#define ROCKCHIP_DRM_FBD_FROM_UBOOT		0
#define ROCKCHIP_DRM_FBD_FROM_UBOOT_TO_RTOS	1
#define ROCKCHIP_DRM_FBD_FROM_RTOS		2

/* mcu_data[23:0] */
#define ROCKCHIP_MCU_DATA_MAP_DATA_1x24			0
/*
 * { mcu_data[7:3], 3'b0, mcu_data[2:0], mcu_data[7:5], 2'b0, mcu_data[4:0], 3'b0 }
 * That is:
 *     LCDC_D23 -> MCU_D7_M1
 *     LCDC_D22 -> MCU_D6_M1
 *     LCDC_D21 -> MCU_D5_M1
 *     LCDC_D20 -> MCU_D4_M1
 *     LCDC_D19 -> MCU_D3_M1
 *     LCDC_D18 -> N/A
 *     LCDC_D17 -> N/A
 *     LCDC_D16 -> N/A
 *     LCDC_D15 -> MCU_D2_M1
 *     LCDC_D14 -> MCU_D1_M1
 *     LCDC_D13 -> MCU_D0_M1
 *     LCDC_D12 -> MCU_D7_M0
 *     LCDC_D11 -> MCU_D6_M0
 *     LCDC_D10 -> MCU_D5_M0
 *     LCDC_D9  -> N/A
 *     LCDC_D8  -> N/A
 *     LCDC_D7  -> MCU_D4_M0
 *     LCDC_D6  -> MCU_D3_M0
 *     LCDC_D5  -> MCU_D2_M0
 *     LCDC_D4  -> MCU_D1_M0
 *     LCDC_D3  -> MCU_D0_M0
 *     LCDC_D2  -> N/A
 *     LCDC_D1  -> N/A
 *     LCDC_D0  -> N/A
 */
#define ROCKCHIP_MCU_DATA_MAP_DATA_WITH_DUMMY_0		1
/* { 8'd0, BT1120[15:0] } */
#define ROCKCHIP_BT1120_DATA_MAP_DATA_PADLO		0
/* { BT1120[15:0], 8'd0 }*/
#define ROCKCHIP_BT1120_DATA_MAP_DATA_PADHI		1
/*
 * { BT1120[15:11], 3'd0, BT1120[10:5], 2'd0, BT1120[4:0], 3'b0 }
 * That is:
 *     LCDC_D23 -> BT1120_D15
 *     LCDC_D22 -> BT1120_D14
 *     LCDC_D21 -> BT1120_D13
 *     LCDC_D20 -> BT1120_D12
 *     LCDC_D19 -> BT1120_D11
 *     LCDC_D18 -> N/A
 *     LCDC_D17 -> N/A
 *     LCDC_D16 -> N/A
 *     LCDC_D15 -> BT1120_D10
 *     LCDC_D14 -> BT1120_D9
 *     LCDC_D13 -> BT1120_D8
 *     LCDC_D12 -> BT1120_D7
 *     LCDC_D11 -> BT1120_D6
 *     LCDC_D10 -> BT1120_D5
 *     LCDC_D9  -> N/A
 *     LCDC_D8  -> N/A
 *     LCDC_D7  -> BT1120_D4
 *     LCDC_D6  -> BT1120_D3
 *     LCDC_D5  -> BT1120_D2
 *     LCDC_D4  -> BT1120_D1
 *     LCDC_D3  -> BT1120_D0
 *     LCDC_D2  -> N/A
 *     LCDC_D1  -> N/A
 *     LCDC_D0  -> N/A
 */
#define ROCKCHIP_BT1120_DATA_MAP_DATA_WITH_DUMMY_0	3
/* { 16'd0, BT656[7:0] }*/
#define ROCKCHIP_BT656_DATA_MAP_PADLO			0
/* { 8'd0, BT656[7:0], 8'd0 }*/
#define ROCKCHIP_BT656_DATA_MAP_DUMMY_HI_LO		1
/* { BT656[7:0], 16'd0 }*/
#define ROCKCHIP_BT656_DATA_MAP_PADHI			2
/*
 * { BT656[7:3], 3'd0, BT656[2:0], BT656[7:5], 2'd0, BT656[4:0], 3'd0 }
 * That is:
 *     LCDC_D23 -> BT656_D7_M1
 *     LCDC_D22 -> BT656_D6_M1
 *     LCDC_D21 -> BT656_D5_M1
 *     LCDC_D20 -> BT656_D4_M1
 *     LCDC_D19 -> BT656_D3_M1
 *     LCDC_D18 -> N/A
 *     LCDC_D17 -> N/A
 *     LCDC_D16 -> N/A
 *     LCDC_D15 -> BT656_D2_M1
 *     LCDC_D14 -> BT656_D1_M1
 *     LCDC_D13 -> BT656_D0_M1
 *     LCDC_D12 -> BT656_D7_M0
 *     LCDC_D11 -> BT656_D6_M0
 *     LCDC_D10 -> BT656_D5_M0
 *     LCDC_D9  -> N/A
 *     LCDC_D8  -> N/A
 *     LCDC_D7  -> BT656_D4_M0
 *     LCDC_D6  -> BT656_D3_M0
 *     LCDC_D5  -> BT656_D2_M0
 *     LCDC_D4  -> BT656_D1_M0
 *     LCDC_D3  -> BT656_D0_M0
 *     LCDC_D2  -> N/A
 *     LCDC_D1  -> N/A
 *     LCDC_D0  -> N/A
 */
#define ROCKCHIP_BT656_DATA_MAP_DATA_WITH_DUMMY_0	3

#endif
