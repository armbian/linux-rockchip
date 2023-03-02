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
#ifndef _RTL8852BE_HAL_H_
#define _RTL8852BE_HAL_H_

/* rtl8852BE_halinit.c */

#define R_AX_HIMR0 0x01A0
#define B_AX_HALT_C2H_INT_EN BIT(21)
#define B_AX_RON_INT_EN BIT(20)
#define B_AX_PDNINT_EN BIT(19)
#define B_AX_SPSANA_OCP_INT_EN BIT(18)
#define B_AX_SPS_OCP_INT_EN BIT(17)
#define B_AX_BTON_STS_UPDATE_INT_EN BIT(16)
#define B_AX_GPIOF_INT_EN BIT(15)
#define B_AX_GPIOE_INT_EN BIT(14)
#define B_AX_GPIOD_INT_EN BIT(13)
#define B_AX_GPIOC_INT_EN BIT(12)
#define B_AX_GPIOB_INT_EN BIT(11)
#define B_AX_GPIOA_INT_EN BIT(10)
#define B_AX_GPIO9_INT_EN BIT(9)
#define B_AX_GPIO8_INT_EN BIT(8)
#define B_AX_GPIO7_INT_EN BIT(7)
#define B_AX_GPIO6_INT_EN BIT(6)
#define B_AX_GPIO5_INT_EN BIT(5)
#define B_AX_GPIO4_INT_EN BIT(4)
#define B_AX_GPIO3_INT_EN BIT(3)
#define B_AX_GPIO2_INT_EN BIT(2)
#define B_AX_GPIO1_INT_EN BIT(1)
#define B_AX_GPIO0_INT_EN BIT(0)

#define R_AX_HISR0 0x01A4
#define B_AX_HALT_C2H_INT BIT(21)
#define B_AX_RON_INT BIT(20)
#define B_AX_PDNINT BIT(19)
#define B_AX_SPSANA_OCP_INT BIT(18)
#define B_AX_SPS_OCP_INT BIT(17)
#define B_AX_BTON_STS_UPDATE_INT BIT(16)
#define B_AX_GPIOF_INT BIT(15)
#define B_AX_GPIOE_INT BIT(14)
#define B_AX_GPIOD_INT BIT(13)
#define B_AX_GPIOC_INT BIT(12)
#define B_AX_GPIOB_INT BIT(11)
#define B_AX_GPIOA_INT BIT(10)
#define B_AX_GPIO9_INT BIT(9)
#define B_AX_GPIO8_INT BIT(8)
#define B_AX_GPIO7_INT BIT(7)
#define B_AX_GPIO6_INT BIT(6)
#define B_AX_GPIO5_INT BIT(5)
#define B_AX_GPIO4_INT BIT(4)
#define B_AX_GPIO3_INT BIT(3)
#define B_AX_GPIO2_INT BIT(2)
#define B_AX_GPIO1_INT BIT(1)
#define B_AX_GPIO0_INT BIT(0)


#define R_AX_PCIE_HIMR00 0x10B0
#define B_AX_HC00ISR_IND_INT_EN BIT(27)
#define B_AX_HD1ISR_IND_INT_EN BIT(26)
#define B_AX_HD0ISR_IND_INT_EN BIT(25)
#define B_AX_HS0ISR_IND_INT_EN BIT(24)
#define B_AX_RETRAIN_INT_EN BIT(21)
#define B_AX_RPQBD_FULL_INT_EN BIT(20)
#define B_AX_RDU_INT_EN BIT(19)
#define B_AX_RXDMA_STUCK_INT_EN BIT(18)
#define B_AX_TXDMA_STUCK_INT_EN BIT(17)
#define B_AX_PCIE_HOTRST_INT_EN BIT(16)
#define B_AX_PCIE_FLR_INT_EN BIT(15)
#define B_AX_PCIE_PERST_INT_EN BIT(14)
#define B_AX_TXDMA_CH12_INT_EN BIT(13)
#define B_AX_TXDMA_CH9_INT_EN BIT(12)
#define B_AX_TXDMA_CH8_INT_EN BIT(11)
#define B_AX_TXDMA_ACH7_INT_EN BIT(10)
#define B_AX_TXDMA_ACH6_INT_EN BIT(9)
#define B_AX_TXDMA_ACH5_INT_EN BIT(8)
#define B_AX_TXDMA_ACH4_INT_EN BIT(7)
#define B_AX_TXDMA_ACH3_INT_EN BIT(6)
#define B_AX_TXDMA_ACH2_INT_EN BIT(5)
#define B_AX_TXDMA_ACH1_INT_EN BIT(4)
#define B_AX_TXDMA_ACH0_INT_EN BIT(3)
#define B_AX_RPQDMA_INT_EN BIT(2)
#define B_AX_RXP1DMA_INT_EN BIT(1)
#define B_AX_RXDMA_INT_EN BIT(0)

#define R_AX_PCIE_HISR00 0x10B4
#define B_AX_HC00ISR_IND_INT BIT(27)
#define B_AX_HD1ISR_IND_INT BIT(26)
#define B_AX_HD0ISR_IND_INT BIT(25)
#define B_AX_HS0ISR_IND_INT BIT(24)
#define B_AX_RETRAIN_INT BIT(21)
#define B_AX_RPQBD_FULL_INT BIT(20)
#define B_AX_RDU_INT BIT(19)
#define B_AX_RXDMA_STUCK_INT BIT(18)
#define B_AX_TXDMA_STUCK_INT BIT(17)
#define B_AX_PCIE_HOTRST_INT BIT(16)
#define B_AX_PCIE_FLR_INT BIT(15)
#define B_AX_PCIE_PERST_INT BIT(14)
#define B_AX_TXDMA_CH12_INT BIT(13)
#define B_AX_TXDMA_CH9_INT BIT(12)
#define B_AX_TXDMA_CH8_INT BIT(11)
#define B_AX_TXDMA_ACH7_INT BIT(10)
#define B_AX_TXDMA_ACH6_INT BIT(9)
#define B_AX_TXDMA_ACH5_INT BIT(8)
#define B_AX_TXDMA_ACH4_INT BIT(7)
#define B_AX_TXDMA_ACH3_INT BIT(6)
#define B_AX_TXDMA_ACH2_INT BIT(5)
#define B_AX_TXDMA_ACH1_INT BIT(4)
#define B_AX_TXDMA_ACH0_INT BIT(3)
#define B_AX_RPQDMA_INT BIT(2)
#define B_AX_RXP1DMA_INT BIT(1)
#define B_AX_RXDMA_INT BIT(0)

#define R_AX_PCIE_HIMR10 0x13B0
#define B_AX_HC10ISR_IND_INT_EN BIT(28)
#define B_AX_TXDMA_CH11_INT_EN BIT(12)
#define B_AX_TXDMA_CH10_INT_EN BIT(11)

#define R_AX_PCIE_HISR10 0x13B4
#define B_AX_HC10ISR_IND_INT BIT(28)
#define B_AX_TXDMA_CH11_INT BIT(12)
#define B_AX_TXDMA_CH10_INT BIT(11)

enum rtw_hal_status hal_get_efuse_8852be(struct rtw_phl_com_t *phl_com,
					 struct hal_info_t *hal);

enum rtw_hal_status hal_init_8852be(struct rtw_phl_com_t *phl_com,
				    struct hal_info_t *hal);

void hal_deinit_8852be(struct rtw_phl_com_t *phl_com,
		       struct hal_info_t *hal);

enum rtw_hal_status hal_start_8852be(struct rtw_phl_com_t *phl_com,
				     struct hal_info_t *hal);

enum rtw_hal_status hal_stop_8852be(struct rtw_phl_com_t *phl_com,
				    struct hal_info_t *hal);
#ifdef CONFIG_WOWLAN
enum rtw_hal_status hal_wow_init_8852be(struct rtw_phl_com_t *phl_com, struct hal_info_t *hal, struct rtw_phl_stainfo_t *sta);
enum rtw_hal_status hal_wow_deinit_8852be(struct rtw_phl_com_t *phl_com, struct hal_info_t *hal, struct rtw_phl_stainfo_t *sta);
#endif /* CONFIG_WOWLAN */

enum rtw_hal_status hal_mp_init_8852be(struct rtw_phl_com_t *phl_com, struct hal_info_t *hal);
enum rtw_hal_status hal_mp_deinit_8852be(struct rtw_phl_com_t *phl_com, struct hal_info_t *hal);

void hal_init_default_value_8852be(struct hal_info_t *hal, struct hal_intr_mask_cfg *cfg);
u32 hal_hci_cfg_8852be(struct rtw_phl_com_t *phl_com,
		       struct hal_info_t *hal,
				struct rtw_ic_info *ic_info);

void init_hal_spec_8852be(struct rtw_phl_com_t *phl_com,
					struct hal_info_t *hal);
void hal_enable_int_8852be(struct hal_info_t *hal);
void hal_disable_int_8852be(struct hal_info_t *hal);
bool hal_recognize_int_8852be(struct hal_info_t *hal);
void hal_clear_int_8852be(struct hal_info_t *hal);
void hal_clear_int_mask_8852be(struct hal_info_t *hal);
void hal_restore_int_8852be(struct hal_info_t *hal);
u32 hal_int_hdler_8852be(struct hal_info_t *hal);
void hal_rx_int_restore_8852be(struct hal_info_t *hal);

/* rtl8852BE_ops.c */

#endif /* _RTL8852BE_HAL_H_ */
