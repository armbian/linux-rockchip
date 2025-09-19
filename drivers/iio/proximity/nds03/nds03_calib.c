/**
 * @file nds03_calib.c
 * @author tongsheng.tang
 * @brief NDS03 Calibration functions
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

#include "nds03_comm.h"
#include "nds03_dev.h"
#include "nds03_data.h"
#include "nds03_calib.h"

/**
 * @brief NDS03 Get Offset Calib Depth MM
 *        获取offset标定距离
 * @param pNxDevice
 * @param calib_depth_mm
 * @return NDS03_Error
 */
NDS03_Error NDS03_GetOffsetCalibDepthMM(NDS03_Dev_t *pNxDevice, uint16_t *calib_depth_mm)
{
	NDS03_Error	ret = NDS03_ERROR_NONE;

	CHECK_RET(NDS03_ReadHalfWord(pNxDevice, NDS03_REG_OFFSET_MM, calib_depth_mm));

	return ret;
}

/**
 * @brief NDS03 Set Offset Calib Depth MM
 *        设置offset标定距离
 * @param pNxDevice
 * @param calib_depth_mm
 * @return NDS03_Error
 */
NDS03_Error NDS03_SetOffsetCalibDepthMM(NDS03_Dev_t *pNxDevice, uint16_t calib_depth_mm)
{
	NDS03_Error	ret = NDS03_ERROR_NONE;

	if (calib_depth_mm == 0)
		CHECK_RET(NDS03_ReadHalfWord(pNxDevice, NDS03_REG_OFFSET_MM, &calib_depth_mm));

	CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_CFG_ENA, NDS03_CMD_ENA_ENABLE));
	CHECK_RET(NDS03_WriteHalfWord(pNxDevice, NDS03_REG_OFFSET_MM, calib_depth_mm));
	CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_CFG_ENA, NDS03_CMD_ENA_DISABLE));

	return ret;
}

/**
 * @brief NDS03 Offset Calibration Check
 *        Offset 标定检查
 * @param pNxDevice
 * @param calib_depth_mm
 * @return NDS03_Error
 */
static NDS03_Error NDS03_OffsetCalibrationCheck(NDS03_Dev_t *pNxDevice)
{
	NDS03_Error	ret = NDS03_ERROR_NONE;
	uint16_t	ref_histo_max, depth[2];
	uint8_t		depth_flag;

	CHECK_RET(NDS03_GetSingleRangingData(pNxDevice));
	CHECK_RET(NDS03_ReadHalfWord(pNxDevice, NDS03_REG_REF_HISTO_MAX, &ref_histo_max));
	if (ret == NDS03_ERROR_NONE && ref_histo_max < NDS03_OFFSET_REF_MAX_COUNT_TH) {
		ret = NDS03_ERROR_VCSEL_ERROR;
		NX_PRINTF("ref_histo_max:%d\r\n", ref_histo_max);
		return ret;
	}
	CHECK_RET(NDS03_ReadByte(pNxDevice, NDS03_REG_DEPTH_FLAG, &depth_flag));
	CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_CFG_ENA, NDS03_CMD_ENA_ENABLE));
	CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_DEPTH_FLAG, 1));
	CHECK_RET(NDS03_GetSingleRangingData(pNxDevice));
	depth[0] = pNxDevice->ranging_data[0].depth;
	CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_DEPTH_FLAG, 2));
	CHECK_RET(NDS03_GetSingleRangingData(pNxDevice));
	depth[1] = pNxDevice->ranging_data[0].depth;
	CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_DEPTH_FLAG, depth_flag));
	CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_CFG_ENA, NDS03_CMD_ENA_DISABLE));
	if (ret == NDS03_ERROR_NONE && ((depth[0] + NDS03_OFFSET_DEPTH_ERROR_TH < depth[1]) ||
		(depth[0] > depth[1] + NDS03_OFFSET_DEPTH_ERROR_TH) ||
		depth[0] == NDS03_DEPTH_INVALID_VALUE || depth[1] == NDS03_DEPTH_INVALID_VALUE)) {
		ret = NDS03_ERROR_OFFSET_ERROR;
		NX_PRINTF("depth[0]:%d depth[1]:%d\r\n", depth[0], depth[1]);
	}
	return ret;
}

/**
 * @brief    ToF Offset 标定
 * @details  不可以指定标定距离，使用默认距离，如果没有修改，那值默认是500mm
 *
 * @param   pNxDevice       设备模组
 * @return  int8_t
 * @retval  0:  成功
 * @retval  !0: Offset标定失败
 */
NDS03_Error NDS03_OffsetCalibration(NDS03_Dev_t *pNxDevice)
{
	NDS03_Error	ret = NDS03_ERROR_NONE;
	uint16_t	ambient_bg;

	NX_PRINTF("%s Start!\r\n", __func__);
	CHECK_RET(NDS03_GetSingleRangingData(pNxDevice));
	CHECK_RET(NDS03_ReadHalfWord(pNxDevice, NDS03_REG_AMBIENT, &ambient_bg));
	if (ambient_bg > NDS03_AMBIENT_TH) {
		ret = NDS03_ERROR_AMBIENT_HIGH;
		NX_PRINTF("ambient_bg:%d\r\n", ambient_bg);
		return ret;
	}
	// 打开使能
	CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_CMD_ENA, NDS03_CMD_ENA_ENABLE));
	CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_CMD_REQ, NDS03_CMD_OFFSET_CALIB));
	CHECK_RET(NDS03_WaitforCmdVal(pNxDevice, NDS03_CMD_OFFSET_CALIB, 10000));
	CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_CMD_VAL, NDS03_CMD_ENA_DISABLE));
	CHECK_RET(NDS03_ReadByte(pNxDevice, NDS03_REG_CALIB_STATE, (uint8_t*)&ret));
	CHECK_RET(NDS03_OffsetCalibrationCheck(pNxDevice));

	NX_PRINTF("%s End!\r\n", __func__);

	return ret;
}

/**
 * @brief    ToF Offset 标定
 * @details  可以指定标定距离
 *
 * @param   pNxDevice       设备模组
 * @param   calib_depth_mm  标定距离
 * @return  int8_t
 * @retval  0:  成功
 * @retval  !0: Offset标定失败
 */
NDS03_Error NDS03_OffsetCalibrationAtDepth(NDS03_Dev_t *pNxDevice, uint16_t calib_depth_mm)
{
	NDS03_Error	ret = NDS03_ERROR_NONE;
	uint16_t	ambient_bg;

	NX_PRINTF("%s Start!\r\n", __func__);

	CHECK_RET(NDS03_GetSingleRangingData(pNxDevice));
	CHECK_RET(NDS03_ReadHalfWord(pNxDevice, NDS03_REG_AMBIENT, &ambient_bg));
	if (ambient_bg > NDS03_AMBIENT_TH){
		ret = NDS03_ERROR_AMBIENT_HIGH;
		NX_PRINTF("ambient:%d \r\n", ambient_bg);
		return ret;
	}
	CHECK_RET(NDS03_SetOffsetCalibDepthMM(pNxDevice, calib_depth_mm));
	// 打开使能
	CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_CMD_ENA, NDS03_CMD_ENA_ENABLE));
	CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_CMD_REQ, NDS03_CMD_OFFSET_CALIB));
	CHECK_RET(NDS03_WaitforCmdVal(pNxDevice, NDS03_CMD_OFFSET_CALIB, 10000));
	CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_CMD_VAL, NDS03_CMD_ENA_DISABLE));
	CHECK_RET(NDS03_ReadByte(pNxDevice, NDS03_REG_CALIB_STATE, (uint8_t*)&ret));
	CHECK_RET(NDS03_OffsetCalibrationCheck(pNxDevice));

	NX_PRINTF("%s End!\r\n", __func__);

	return ret;
}

/**
 * @brief NDS03 Read Xtalk Data
 *        读取NDS03串扰数据
 * @param pNxDevice     模组设备
 * @param rbuf          数据指针
 * @param size          数据大小
 * @return NDS03_Error
 */
static NDS03_Error NDS03_ReadXtalkData(NDS03_Dev_t *pNxDevice)
{
	NDS03_Error	ret = NDS03_ERROR_NONE;
	uint32_t	rsize, _size;
	uint8_t		one_size;
	uint16_t	addr;
	uint16_t	rbuf[240];
	uint8_t		*rbuf_ptr = (uint8_t *)rbuf;
	uint32_t	size = 240 * 2;

	CHECK_RET(NDS03_ReadByte(pNxDevice, NDS03_REG_CACHE_SIZE, &one_size));
	CHECK_RET(NDS03_ReadHalfWord(pNxDevice, 0xEC, &addr));
	_size = (uint32_t)one_size;
	rsize = 0;
	while ((rsize < size) && (ret == NDS03_ERROR_NONE)) {
		CHECK_RET(NDS03_WriteHalfWord(pNxDevice, NDS03_REG_CACHE_ADDR, addr));
		CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_CMD_ENA, 0x05));
		CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_DAT_REQ, 0xC0));
		CHECK_RET(NDS03_WaitforDataVal(pNxDevice, 0xC0, 200));
		CHECK_RET(NDS03_ReadNBytes(pNxDevice, NDS03_REG_CACHE_DATA, rbuf_ptr,
						((size-rsize)>_size) ? _size:(size-rsize)));
		CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_DAT_VAL, NDS03_DATA_VAL_IDLE));
		rbuf_ptr += _size;
		addr += _size;
		rsize += _size;
	}
	for (int i = 80; i < 240; i++) {
		if (rbuf[i] > 10000)
			return -14;
	}

	return ret;
}

/**
 * @brief   NDS03 Xtalk Calibration
 *          NDS03串扰/盖板标定
 * @param   pNxDevice       设备模组
 * @return  int8_t
 * @retval  0:  成功
 * @retval  !0: xtalk标定失败
 */
NDS03_Error NDS03_XtalkCalibration(NDS03_Dev_t *pNxDevice)
{
	NDS03_Error	ret = NDS03_ERROR_NONE, calib_state = NDS03_ERROR_NONE;
	uint16_t	ambient_bg,ref_histo_max;
	uint8_t		cnt = 2;
	int8_t		check_xtalk_state = 0;

	NX_PRINTF("%s Start!\r\n", __func__);
	do {
		CHECK_RET(NDS03_GetSingleRangingData(pNxDevice));
		CHECK_RET(NDS03_ReadHalfWord(pNxDevice, NDS03_REG_AMBIENT, &ambient_bg));
		CHECK_RET(NDS03_ReadHalfWord(pNxDevice, NDS03_REG_REF_HISTO_MAX, &ref_histo_max));
		if (ambient_bg > NDS03_AMBIENT_TH) {
			ret = NDS03_ERROR_AMBIENT_HIGH;
			NX_PRINTF("ambient:%d\r\n", ambient_bg);
			return ret;
		}
		if (ret == NDS03_ERROR_NONE && ref_histo_max < NDS03_OFFSET_REF_MAX_COUNT_TH) {
			ret = NDS03_ERROR_VCSEL_ERROR;
			NX_PRINTF("ref_histo_max:%d\r\n", ref_histo_max);
			return ret;
		}
		// 打开使能
		CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_CMD_ENA, NDS03_CMD_ENA_ENABLE));
		CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_CMD_REQ, NDS03_CMD_XTALK_CALIB));
		CHECK_RET(NDS03_WaitforCmdVal(pNxDevice, NDS03_CMD_XTALK_CALIB, 5000));
		CHECK_RET(NDS03_WriteByte(pNxDevice, NDS03_REG_CMD_VAL, NDS03_CMD_ENA_DISABLE));
		CHECK_RET(NDS03_GetFirmwareVersion(pNxDevice));
		if (pNxDevice->chip_info.fw_version == 0x10203) {
			CHECK_RET(NDS03_GetSingleRangingData(pNxDevice));
			check_xtalk_state = NDS03_ReadXtalkData(pNxDevice);
		}

	} while (cnt-- && check_xtalk_state != 0);
	CHECK_RET(NDS03_ReadByte(pNxDevice, NDS03_REG_CALIB_STATE, (uint8_t*)&calib_state));
	ret = check_xtalk_state ? check_xtalk_state : (ret | calib_state) &
			(NDS03_CALIB_ERROR_XTALK_OVERFLOW | NDS03_CALIB_ERROR_XTALK_EXCESSIVE);
	NX_PRINTF("%s End!\r\n", __func__);

	return ret;
}

/**
 * @brief NDS03 Get Xtalk Value
 *        获取标定串扰值
 * @param pNxDevice
 * @return NDS03_Error
 */
NDS03_Error NDS03_GetXTalkValue(NDS03_Dev_t *pNxDevice, uint16_t* xtalk_value)
{
	NDS03_Error ret = NDS03_ERROR_NONE;

	CHECK_RET(NDS03_ReadHalfWord(pNxDevice, NDS03_REG_XTALK, xtalk_value));

	return ret;
}
