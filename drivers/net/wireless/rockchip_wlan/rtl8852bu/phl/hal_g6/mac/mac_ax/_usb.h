/** @file */
/******************************************************************************
 *
 * Copyright(c) 2019 Realtek Corporation. All rights reserved.
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
 ******************************************************************************/

#ifndef _MAC_AX_USB_H_
#define _MAC_AX_USB_H_

#include "../type.h"

#define phyoffset            0x20
#define USB11                0x1
#define USB2                 0x2
#define USB3                 0x3
#define U3SWITCHU2           0x17
#define U2SWITCHU3           0xB
#define USB_SWITCH_DELAY     0xF
#define SWITCHMODE           0x2
#define FORCEUSB3MODE        0x1
#define FORCEUSB2MODE        0x0
#define USBEPMAPERR          0xFF
#define USB11_BULKSIZE       0x2
#define USB2_BULKSIZE        0x1
#define USB3_BULKSIZE        0x0
#define EP4                  0x4
#define EP5                  0x5
#define EP6                  0x6
#define EP7                  0x7
#define EP8                  0x8
#define EP9                  0x9
#define EP10                 0xA
#define EP11                 0xB
#define EP12                 0xC
#define NUMP                 0x1
#define BULKOUTID0           0x0
#define BULKOUTID1           0x1
#define BULKOUTID2           0x2
#define BULKOUTID3           0x3
#define BULKOUTID4           0x4
#define BULKOUTID5           0x5
#define BULKOUTID6           0x6
#define USBPHYOFFSET         0x100
#define DISABLE              0x0
#define ENABLE               0x1
#define RXAGGSIZE            0x5
#define RXAGGTO              0x20
#define EFUSESIZE            0x1
#define CMAC_CLK_ALLEN       0xFFFFFFFF
#define SINGLE_MSDU_SIZE     0x600
#define SEC_FCS_SIZE         0x80
#define PLE_PAGE_SIZE        0x80
#define PINGPONG             0x2
#define RX_POLLING_PERIOD    0x40
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup USB
 * @{
 */
/**
 * @brief usb_flush_mode
 *
 * @param *adapter
 * @param mode
 * @return Please Place Description here.
 * @retval u32
 */
u32 usb_flush_mode(struct mac_ax_adapter *adapter, u8 mode);

/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup USB
 * @{
 */

/**
 * @brief get_usb_mode_8852a
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u32
 */
u32 get_usb_mode(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @brief usb_autok_counter_avg
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u32
 */
u32 usb_autok_counter_avg(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

#endif
