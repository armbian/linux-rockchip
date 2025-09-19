
/**
 * @file nds03_data.c
 * @author tongsheng.tang
 * @brief NDS03 get depth data functions
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

#include "nds03_def.h"
#include "nds03_comm.h"
#include "nds03_dev.h"
#include "nds03_data.h"

/**
 * @brief NDS03 Get Ranging Data Ready
 *        检测NDS03测距是否完成
 * @param   pNxDevice 模组设备
 * @return  int8_t
 * @retval  函数执行结果
 * - 0：未完成
 *   1: 完成
 * - ::NDS03_ERROR_I2C:IIC通讯错误/数据异常
 */
NDS03_Error NDS03_GetRangingDataReady(NDS03_Dev_t *pNxDevice)
{
	NDS03_Error ret = NDS03_ERROR_NONE;
	uint8_t     buf_valid_flag = 0x00;

	CHECK_RET(NDS03_ReadByte(pNxDevice, NDS03_REG_DAT_VAL, &buf_valid_flag));

	if (buf_valid_flag == NDS03_DEPTH_DATA_FLAG)
		return 1;

	return ret;
}

/**
 * @brief NDS03 Start Single Measurement
 *        发送开始单次测量信号
 * @param   pNxDevice 模组设备
 * @return  int8_t
 */
NDS03_Error NDS03_StartSingleMeasurement(NDS03_Dev_t *pNxDevice)
{
	NDS03_Error ret = NDS03_ERROR_NONE;

	/* 清除标志位 */
	CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_DAT_VAL, NDS03_DATA_VAL_IDLE));
	/* 发送触发信号 */
	CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_DAT_REQ, NDS03_DEPTH_DATA_FLAG));

	return ret;
}

/**
 * @brief NDS03 Start Continuous Measurement
 *        发送开始连续测量信号
 * @param   pNxDevice 模组设备
 * @return  int8_t
 */
NDS03_Error NDS03_StartContinuousMeasurement(NDS03_Dev_t *pNxDevice)
{
	NDS03_Error ret = NDS03_ERROR_NONE;

	if (pNxDevice->config.continuous_flag == 0) {
		/* 清除标志位 */
		CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_DAT_VAL, NDS03_DATA_VAL_IDLE));
		/* 发送触发信号 */
		CHECK_RET(NDS03_WriteByte(pNxDevice,
				NDS03_REG_DAT_REQ, NDS03_DEPTH_CONTINUOUS_FLAG));
		pNxDevice->config.continuous_flag = 1;
	}

	return ret;
}

/**
 * @brief NDS03 Stop Continuous Measurement
 *        发送结束连续测量信号，用于连续模式
 * @param   pNxDevice 模组设备
 * @return  int8_t
 */
NDS03_Error NDS03_StopContinuousMeasurement(NDS03_Dev_t *pNxDevice)
{
	NDS03_Error ret = NDS03_ERROR_NONE;

	if (pNxDevice->config.continuous_flag != 0) {
		/* 清除标志位 */
		CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_DAT_REQ, NDS03_DATA_REQ_IDLE));
		/* 等待测量完成 */
		CHECK_RET(NDS03_WaitforDataVal(pNxDevice, NDS03_DEPTH_CONTINUOUS_FLAG, 200));
		/* 清除标志位 */
		CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_DAT_VAL, NDS03_DATA_VAL_IDLE));
		pNxDevice->config.continuous_flag = 0;
	}

	return ret;
}

/**
 * @brief NDS03 Clear Data Valid Flag
 *        清除NDS03测量数据的有效位，取完一次数
 *        据做的操作，通知NDS03数据已读取
 * @param   pNxDevice 模组设备
 * @return  NDS03_Error
 */
NDS03_Error NDS03_ClearDataValidFlag(NDS03_Dev_t *pNxDevice)
{
	NDS03_Error ret = NDS03_ERROR_NONE;

	CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_DAT_VAL, NDS03_DATA_VAL_IDLE));

	return ret;
}

/**
 * @brief NDS03 Read Ranging Data
 *        读取NDS03寄存器获取测量数据，数据更新于一次测距完成后
 * @param   pNxDevice 模组设备
 * @return  int32_t
 */
NDS03_Error NDS03_ReadRangingData(NDS03_Dev_t *pNxDevice)
{
	NDS03_Error	ret = NDS03_ERROR_NONE;

#if NDS03_TARGET_MAX_NUM == 1
	CHECK_RET(NDS03_ReadNBytesByHalfWord(pNxDevice,
		NDS03_REG_DEPTH, (uint16_t*)&pNxDevice->ranging_data[0],
		sizeof(pNxDevice->ranging_data[0])));
#endif

#if NDS03_TARGET_MAX_NUM == 2
	CHECK_RET(NDS03_ReadNBytesByHalfWord(pNxDevice,
		NDS03_REG_DEPTH, (uint16_t*)&pNxDevice->ranging_data[0],
		2 * sizeof(pNxDevice->ranging_data[0])));
#endif

#if NDS03_TARGET_MAX_NUM == 3
	CHECK_RET(NDS03_ReadNBytesByHalfWord(pNxDevice,
		NDS03_REG_DEPTH, (uint16_t*)&pNxDevice->ranging_data[0],
		3 * sizeof(pNxDevice->ranging_data[0])));
#endif

#if NDS03_TARGET_MAX_NUM >= 4
	CHECK_RET(NDS03_ReadNBytesByHalfWord(pNxDevice,
		NDS03_REG_DEPTH, (uint16_t*)&pNxDevice->ranging_data[0],
		4 * sizeof(pNxDevice->ranging_data[0])));
#endif

    return ret;
}

/**
 * @brief NDS03 Get Continuous Ranging Data
 *        连续模式下，从NDS03中获取一次深度数据，
 *        需要与NDS03_StartContinuousMeasurement函数搭配
 * @param   pNxDevice 模组设备
 * @param   pData  获取到的深度和幅度数据
 * @return  int32_t
 */
NDS03_Error NDS03_GetContinuousRangingData(NDS03_Dev_t *pNxDevice)
{
	NDS03_Error ret = NDS03_ERROR_NONE;
	uint8_t     data_cnt;
	uint32_t    retry_cnt = 20000;

	/* 等待测量完成 */
	do {
		CHECK_RET(NDS03_ReadByte(pNxDevice, NDS03_REG_DATA_CNT, &data_cnt));
		CHECK_RET(NDS03_Delay10us(pNxDevice, 10));
	} while (data_cnt == pNxDevice->data_cnt && --retry_cnt);
	pNxDevice->data_cnt = data_cnt;
	if (retry_cnt != 0) {
		/* 读取测量数据 */
		CHECK_RET(NDS03_ReadRangingData(pNxDevice));
		/* 清除数据有效标志位 */
		CHECK_RET(NDS03_ClearDataValidFlag(pNxDevice));
	} else {
		ret = NDS03_ERROR_TIMEOUT;
	}

	return ret;
}

/**
 * @brief NDS03 Get Single Ranging Data
 *        NDS03中获取一次深度数据
 *
 * @param   pNxDevice 模组设备
 * @return  int32_t
 */
NDS03_Error NDS03_GetSingleRangingData(NDS03_Dev_t *pNxDevice)
{
	NDS03_Error ret = NDS03_ERROR_NONE;
	int32_t     timeout_ms = pNxDevice->config.range_frame_time_us / 1000;

	/* 清除标志位 */
	CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_DAT_VAL, NDS03_DATA_VAL_IDLE));
	/* 发送触发信号 */
	CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_DAT_REQ, NDS03_DEPTH_DATA_FLAG));
	/* 等待测量完成 */
	timeout_ms = (timeout_ms < 200) ? 200 : timeout_ms;
	CHECK_RET(NDS03_WaitforDataVal(pNxDevice, NDS03_DEPTH_DATA_FLAG, timeout_ms));
	/* 读取测量数据 */
	CHECK_RET(NDS03_ReadRangingData(pNxDevice));
	/* 清除数据有效标志位 */
	CHECK_RET(NDS03_ClearDataValidFlag(pNxDevice));

	return ret;
}

/**
 * @brief NDS03 Get Interrupt Ranging Data
 *        NDS03中获取一次中断深度数据
 *
 * @param   pNxDevice 模组设备
 * @return  int32_t
 */
NDS03_Error NDS03_GetInterruptRangingData(NDS03_Dev_t *pNxDevice)
{
	NDS03_Error ret = NDS03_ERROR_NONE;

	/* 读取测量数据 */
	CHECK_RET(NDS03_ReadRangingData(pNxDevice));
	/* 清除数据有效标志位 */
	CHECK_RET(NDS03_ClearDataValidFlag(pNxDevice));

	return ret;
}

