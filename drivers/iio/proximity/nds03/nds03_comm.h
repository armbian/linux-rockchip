/**
 * @file nds03_comm.h
 * @author tongsheng.tang
 * @brief NDS03 communication functions
 * @version 2.x.x
 * @date 2025-06
 *
 * @copyright Copyright (c) 2025, Nephotonics Information Technology (Hefei) Co., Ltd.
 *
 * @license BSD 3-Clause License
 *          This file is part of the NDS03 SDK and is licensed under the BSD 3-Clause License.
 *          You may obtain a copy of the license in the project root directory,
 *          in the file named LICENSE.
 */
#ifndef __NDS03_COMM_H__
#define __NDS03_COMM_H__

#include "nds03_def.h"

/** @defgroup NDS03_Communication_Group NDS03 Communication Functions
 *  @brief NDS03 Communication Functions
 *  @{
 */

/** 延时时间（ms） */
NDS03_Error NDS03_Delay1ms(NDS03_Dev_t *pNxDevice, uint32_t ms);

/** 延时时间（10us） */
NDS03_Error NDS03_Delay10us(NDS03_Dev_t *pNxDevice, uint32_t us);

/** 设置xshut引脚的电平 */
NDS03_Error NDS03_SetXShutPinLevel(NDS03_Dev_t *pNxDevice, int8_t level);

/** NDS03设置i2c频率 */
NDS03_Error NDS03_SetI2cFreq(NDS03_Dev_t *pNxDevice, uint32_t freq);
/** NDS03获取i2c频率 */
NDS03_Error NDS03_GetI2cFreq(NDS03_Dev_t *pNxDevice, uint32_t *freq);
/** 对NDS03寄存器写N个字节 */
NDS03_Error NDS03_WriteNBytes(NDS03_Dev_t *pNxDevice, uint8_t addr, void *wdata, uint16_t size);
/** 对NDS03寄存器读N个字节 */
NDS03_Error NDS03_ReadNBytes(NDS03_Dev_t *pNxDevice, uint8_t addr, void *rdata, uint16_t size);
/** 对NDS03寄存器写1个字节 */
NDS03_Error NDS03_WriteByte(NDS03_Dev_t *pNxDevice, uint8_t addr, uint8_t wdata);
/** 对NDS03寄存器写1个字节 */
NDS03_Error NDS03_ReadByte(NDS03_Dev_t *pNxDevice, uint8_t addr, uint8_t *rdata);
/** 对NDS03寄存器写N个字节，使用半字方式 */
NDS03_Error NDS03_WriteNBytesByHalfWord(NDS03_Dev_t *pNxDevice,
                                uint8_t addr, uint16_t *wdata, uint16_t size);
/** 对NDS03寄存器读N个字节，使用半字方式 */
NDS03_Error NDS03_ReadNBytesByHalfWord(NDS03_Dev_t *pNxDevice,
                                uint8_t addr, uint16_t *rdata, uint16_t size);
/** 对NDS03寄存器写1个字 */
NDS03_Error NDS03_WriteHalfWord(NDS03_Dev_t *pNxDevice, uint8_t addr, uint16_t wdata);
/** 对NDS03寄存器读1个字 */
NDS03_Error NDS03_ReadHalfWord(NDS03_Dev_t *pNxDevice, uint8_t addr, uint16_t *rdata);
/** 对NDS03寄存器写N个字节,使用字方式 */
NDS03_Error NDS03_WriteNBytesByWord(NDS03_Dev_t *pNxDevice,
                                uint8_t addr, uint32_t *wdata, uint16_t size);
/** 对NDS03寄存器读N个字节,使用字方式 */
NDS03_Error NDS03_ReadNBytesByWord(NDS03_Dev_t *pNxDevice,
                                uint8_t addr, uint32_t *rdata, uint16_t size);
/** 对NDS03寄存器写1个字 */
NDS03_Error NDS03_WriteWord(NDS03_Dev_t *pNxDevice, uint8_t addr, uint32_t wdata);
/** 对NDS03寄存器读1个字 */
NDS03_Error NDS03_ReadWord(NDS03_Dev_t *pNxDevice, uint8_t addr, uint32_t *rdata);
/** 获取系统时钟时间（ms） */
NDS03_Error NDS03_GetSystemClkMs(NDS03_Dev_t *pNxDevice,int32_t *time_ms);

/** @} NDS03_Communication_Group */

#endif
