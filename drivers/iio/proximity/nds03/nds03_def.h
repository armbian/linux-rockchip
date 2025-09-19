/**
 * @file nds03_def.h
 * @author tongsheng.tang
 * @brief NDS03's Macro definition and data structure
 * @version 2.x.x
 * @date 2025-06
 *
 * @copyright Copyright (c) 2025, Nephotonics Information Technology (Hefei) Co., Ltd.
 *
 * @license BSD 3-Clause License
 *          This file is part of the NDS03 SDK and is licensed under the BSD 3-Clause License.
 *          You may obtain a copy of the license in the project root directory, 
 *          in the file named LICENSE.
 *
 */
#ifndef __NDS03_DEF_H__
#define __NDS03_DEF_H__

#include "nds03_stdint.h"
#if NDS03_PLATFORM == PLATFORM_LINUX_DRIVER
#include <linux/kernel.h>
#else
#include <stdio.h>
#endif
#include "nds03_platform.h"

#ifndef DEBUG_INFO
#define DEBUG_INFO        0 /** 调试信息打印开关 */
#endif

#if NDS03_PLATFORM == PLATFORM_NOT_C51
#include <stdio.h>
#define NX_PRINTF(fmt, ...)   do { if (DEBUG_INFO) printf(fmt, ##__VA_ARGS__); } while(0)

#elif NDS03_PLATFORM == PLATFORM_LINUX_DRIVER
#include <linux/kernel.h>
#define NX_PRINTF(fmt, args...)   do { if (DEBUG_INFO) printk(fmt, ##args); } while(0)

#elif NDS03_PLATFORM == PLATFORM_C51
#define NX_PRINTF(fmt, ...)

#else
#define NX_PRINTF(fmt, ...)
#endif

/** @defgroup NDS03_Global_Define_Group NDS03 Defines
 *  @brief	  NDS03 Defines
 *  @{
 */

/** @defgroup NDS03_Reg_Group NDS03 Register Defines
 * @brief	  NDS03 Register Defines
 *  @{
 */
#define NDS03_REG_DEV_ADDR                      (0x1E)      /** 模组的设备地址 */

#define NDS03_REG_STATE                         (0x20)      /** 模组运行状态寄存器 */
#define NDS03_REG_DATA_CNT                      (0x21)      /** 模组测距次数计数值 */
#define NDS03_REG_FW_VER                        (0x24)      /** 模组固件版本号寄存器 */

#define NDS03_REG_RANGE_STATE                   (0x28)      /** 测距状态码 */
#define NDS03_REG_CALIB_STATE                   (0x29)      /** 标定状态码 */
#define NDS03_REG_THERM                         (0x2A)      /** 温度值 */
#define NDS03_REG_AMBIENT                       (0x2C)      /** 环境光值 */

#define NDS03_REG_DEPTH                         (0x30)      /** 深度值 */
#define NDS03_REG_CONFI                         (0x32)      /** 置信度 */
#define NDS03_REG_COUNT                         (0x34)      /** 计数值 */
#define NDS03_REG_CRATE                         (0x36)      /** 计数率 */
#define NDS03_REG_DEPTH2                        (0x38)      /** 深度值 */
#define NDS03_REG_CONFI2                        (0x3A)      /** 置信度 */
#define NDS03_REG_COUNT2                        (0x3C)      /** 计数值 */
#define NDS03_REG_CRATE2                        (0x3E)      /** 计数率 */
#define NDS03_REG_DEPTH3                        (0x40)      /** 深度值 */
#define NDS03_REG_CONFI3                        (0x42)      /** 置信度 */
#define NDS03_REG_COUNT3                        (0x44)      /** 计数值 */
#define NDS03_REG_CRATE3                        (0x46)      /** 计数率 */
#define NDS03_REG_DEPTH4                        (0x48)      /** 深度值 */
#define NDS03_REG_CONFI4                        (0x4A)      /** 置信度 */
#define NDS03_REG_COUNT4                        (0x4C)      /** 计数值 */
#define NDS03_REG_CRATE4                        (0x4E)      /** 计数率 */

#define NDS03_REG_DAT_REQ                       (0x50)      /** 获取深度请求寄存器 */
#define NDS03_REG_DAT_VAL                       (0x51)      /** 获取数据命令有效寄存器 */
#define NDS03_REG_CMD_REQ                       (0x52)      /** 命令请求寄存器 */
#define NDS03_REG_CMD_VAL                       (0x53)      /** 命令完成寄存器 */
#define NDS03_REG_CMD_ENA                       (0x54)      /** 命令使能寄存器 */
#define NDS03_REG_CFG_ENA                       (0x55)      /** 配置使能，该值为0xA5才能访问其他寄存器 */
#define NDS03_REG_REF_HISTO_MAX                 (0x56)      /** 参考直方图最大值 */

#define NDS03_REG_DEPTH_FLAG                    (0x64)      /** 测距结果标志位 */
#define NDS03_REG_SLEEP_TIME                    (0x66)      /** 睡眠时间(ms) */
#define NDS03_REG_PULSE_NUM                     (0x6C)      /** 发光次数 */

#define NDS03_REG_DEPTH_TH_L                    (0x70)      /** 深度低阈值 */
#define NDS03_REG_DEPTH_TH_H                    (0x72)      /** 深度高阈值 */
#define NDS03_REG_GPIO1_FUNC                    (0x74)      /** 中断功能/方式 */
#define NDS03_REG_SLEEP_MODE                    (0x75)      /** 睡眠方式 */
#define NDS03_REG_CONFI_TH                      (0x76)      /** 置信度阈值 */
#define NDS03_REG_TARGET_NUM                    (0x77)      /** 目标个数 */
#define NDS03_REG_INV_TIME                      (0x7C)      /** 测量间隔时间 */
#define NDS03_REG_OFFSET_MM                     (0x90)      /** offset 标定位置 */

#define NDS03_REG_XTALK_TH                      (0xB0)      /** 串扰阈值 */
#define NDS03_REG_XTALK                         (0xB2)      /** 串扰值 */

#define NDS03_REG_DATA_SIZE                     (0x5C)      /** 数据大小 */
#define NDS03_REG_CACHE_SIZE                    (0x5D)      /** 缓存大小 */
#define NDS03_REG_CACHE_ADDR                    (0x5E)      /** 缓存地址 */
#define NDS03_REG_CACHE_DATA                    (0xC0)      /** 缓存数据 */

/** @} NDS03_Reg_Group */

/** @defgroup NDS03_State_Group NDS03 Data Request Index
 *  @brief	  NDS03 State  (NDS03_REG_STATE)
 *  @{
 */

#define NDS03_STATE_IDLE                        0x00    /** 空闲状态 */
#define NDS03_STATE_SOFT_READY                  0xA5    /** 软件就绪状态 */
#define NDS03_STATE_GOT_DEPTH                   0xA6    /** 已获取深度状态 */

/** @} NDS03_State_Group */

/** @defgroup NDS03_Data_Val_Req_Idx_Group NDS03 Data Request Index
 *  @brief    NDS03 Data Request Mask (NDS03_REG_DAT_VAL_REQ)
 *  @{
 */
/** DATA REQ */
#define NDS03_DATA_REQ_IDLE                     0x00    /** 无效数据命令标志位 */
#define NDS03_DEPTH_DATA_FLAG                   0x10    /** 获取深度数据标志 */
#define NDS03_DEPTH_CONTINUOUS_FLAG             0x13    /** 连续获取深度数据标志 */
#define NDS03_USER_DATA_FLAG                    0x50    /** 读用户数据标志位 */
#define NDS03_GET_MODEL_FLAG                    0x51    /** 获取模组型号标志位 */
#define NDS03_CMD_READ_HGM_DATA                 0xC0    /** 读直方图数据标志位 */
#define NDS03_CMD_READ_HGM_DATA_ENA             0x05    /** 读直方图数据使能标志位 */

/** DATA VAL */
#define NDS03_DATA_VAL_IDLE                     0x00    /** 无效数据标志位 */

/** CMD REQ */
#define NDS03_CMD_OFFSET_CALIB                  0x20    /** offset标定命令 */
#define NDS03_CMD_XTALK_CALIB                   0x24    /** xtalk标定 */
#define NDS03_CMD_WRITE_USER_DATA               0x50    /** 写用户数据区域标志位 */


/** CMD VAL */
#define NDS03_CMD_VAL_IDLE                      0x00    /** 无效命令标志位 */

/** CMD ENA */
#define NDS03_CMD_WRITE_USER_DATA_ENA           0xAA    /** 用户数据使能标志位 */
#define NDS03_CMD_ENA_ENABLE                    0xA5    /** 配置开启标志位 */
#define NDS03_CMD_ENA_DISABLE                   0x00    /** 配置关闭标志位 */

/** @} NDS03_Data_Val_Req_Idx_Group */

/** @defgroup NDS03_Sleep_Group NDS03 Sleep Group
 *  @brief	  NDS03 Sleep  (NDS03_REG_SLEEP_MODE)
 *  @{
 */

#define     NDS03_MANUAL_SLEEP_TIME_OUT_WEAK_UP            0xA5  /* 手动进入软件睡眠标志位，超时唤醒 */
#define     NDS03_MANUAL_SLEEP_MANUAL_WEAK_UP              0xA4  /* 手动进入软件睡眠标志位，手动唤醒 */
// #define     NDS03_AUTO_SLEEP_MANUAL_WEAK_UP                0xA3  /* 测量结束自动进入软件睡眠标志位，手动唤醒 */
// #define     NDS03_AUTO_SLEEP_TIME_OUT_WEAK_UP              0xA2  /* 测量结束自动进入软件睡眠标志位，超时唤醒 */

/** @} NDS03_Sleep_Group */

/** @enum  NDS03_Status_e
 *  @brief 定义NDS03状态宏
 */
typedef int8_t NDS03_Error;

/** @defgroup NDS03_Error_Group NDS03 Error Group
 *  @brief	  NDS03 Error Group (NDS03_REG_ERROR_FLAG)
 *  @{
 */
#define NDS03_ERROR_NONE                            0   /** 成功 */
#define NDS03_ERROR_API                             -1  /** api接口注册失败 */
#define NDS03_ERROR_TIMEOUT                       	-2  /** 超时错误 */
#define NDS03_ERROR_I2C                             -3  /** IIC通讯错误 */
#define NDS03_ERROR_BOOT                            -4  /** 模组启动错误 */
#define NDS03_ERROR_CALIB                           -5  /** 标定失败错误 */
#define NDS03_ERROR_INIT                            -6  /** 初始化失败 */
#define NDS03_ERROR_RANGING                         -7  /** 测距出错 */
#define NDS03_ERROR_UPGRADE_VER                     -8  /** 升级版本不对 */
#define NDS03_ERROR_UPGRADE                         -9  /** 升级失败 */
#define NDS03_ERROR_AMBIENT_HIGH                    -10 /** 标定时环境光强过大 */
#define NDS03_ERROR_NO_NDS03                        -11 /** 不是NDS03模组 */
#define NDS03_ERROR_VCSEL_ERROR                     -12 /** VCSEL不亮 */
#define NDS03_ERROR_OFFSET_ERROR                    -13 /** OFFSET 标定失败 */

/** @} NDS03_Error_Group */

/** @defgroup NDS03_Calib_State_Group NDS03 Calib State Group
 *  @brief	  NDS03 Calib State Group (NDS03_REG_CALIB_STATE)
 *  @{
 */
#define NDS03_CALIB_ERROR_NONE                      0x00  /** 成功                    */
#define NDS03_CALIB_ERROR_XTALK_OVERFLOW            0x02  /** 串扰溢出                 */
#define NDS03_CALIB_ERROR_XTALK_EXCESSIVE           0x10  /** 串扰过大                 */
#define NDS03_CALIB_ERROR_OFFSET                    0x20  /** offset标定后测距误差过大   */
/** @} NDS03_Calib_State_Group */

/** @defgroup NDS03_GPIO1_Func_Group NDS03 GPIO1 Functions Define
 *  @brief    NDS03 GPIO1 Functions Define Group (NDS03_REG_GPIO1_FUNC)
 *  @{
 */

/** REG_GPIO1_SETTING Mask */
#define NDS03_GPIO1_FUNCTIONALITY_MASK              0x07  /** GPIO1功能配置掩码 */
#define NDS03_GPIO1_POLARITY_MASK                   0x08  /** 极性配置掩码 */

/** GPIO1 Functionality */
typedef uint8_t NDS03_Gpio1Func_t;
#define NDS03_GPIO1_FUNCTIONALITY_OFF               ((NDS03_Gpio1Func_t)0x00) /** 无触发中断 */
#define NDS03_GPIO1_THRESHOLD_LOW                   ((NDS03_Gpio1Func_t)0x01) /** 低深度触发中断 (value < NDS03_REG_DEPTH_TH_L) */
#define NDS03_GPIO1_THRESHOLD_HIGH                  ((NDS03_Gpio1Func_t)0x02) /** 高深度触发中断 (value > NDS03_REG_DEPTH_TH_H) */
#define NDS03_GPIO1_THRESHOLD_DOMAIN_OUT            ((NDS03_Gpio1Func_t)0x03) /** 低深度或高深度触发中断 (value < NDS03_REG_DEPTH_TH_L 或 value > NDS03_REG_DEPTH_TH_H) */
#define NDS03_GPIO1_NEW_MEASURE_READY               ((NDS03_Gpio1Func_t)0x04) /** 新深度数据就绪中断 */

/** GPIO1 polarity */
typedef uint8_t NDS03_Gpio1Polar_t;
#define NDS03_GPIO1_POLARITY_LOW                      ((NDS03_Gpio1Polar_t)0x00) /** 负极性, 低电平有效 */
#define NDS03_GPIO1_POLARITY_HIGH                     ((NDS03_Gpio1Polar_t)0x01) /** 正极性, 高电平有效 */

/** @} NDS03_GPIO1_Func_Group */

#define NDS03_TARGET_MAX_NUM                        4
#define NDS03_AMBIENT_TH                            1000    /** 标定时环境光强阈值 */
#define NDS03_OFFSET_DEPTH_ERROR_TH                 10      /** OFFSET 标定深度误差阈值 */
#define NDS03_OFFSET_REF_MAX_COUNT_TH               500     /** OFFSET 标定深度误差阈值 */
#define NDS03_DEPTH_INVALID_VALUE                   65300   /** 深度无效值 */

/** @enum  NDS03_Status_e
 *  @brief 定义NDS03状态宏
 */
typedef enum{
	NDS03_DISABLE = 0,      ///< 关闭状态
	NDS03_ENABLE  = 1       ///< 使能状态
} NDS03_Status_e;

/**
 * @struct NDS03_RangingData_t
 *
 * @brief NDS03测量结果结构体 \n
 * 定义存储NDS03的深度、置信度信息
 */
typedef struct{
	uint16_t    depth;              ///< 测量距离
	uint16_t    confi;              ///< 测量置信度
	uint16_t    count;              ///< 计数值
	uint16_t    crate;              ///< 计数率
} NDS03_RangingData_t;


/**
  * @struct NDS03_ChipInfo_t
  *
  * @brief NDS03模组生产信息\n
  */
typedef struct {
	uint32_t            fw_version;             ///< NDS03固件版本
} NDS03_ChipInfo_t;

/**
 * @struct NDS03_DevConfig_t
 *
 * @brief NDS03模组配置数据\n
 */
typedef struct {
	uint32_t            range_frame_time_us;    //< 模组取图帧间隔时间配置
	uint8_t             continuous_flag;        ///< 连续模式标志位
	uint8_t             target_num;             ///< 目标个数
} NDS03_DevConfig_t;

/**
 * @struct NDS03_Dev_t
 *
 * @brief 设备类型结构体\n
 */
typedef struct {
	uint8_t             dev_pwr_state;          ///< 设备的当前状态, 就绪模式或者休眠模式
	NDS03_DevConfig_t   config;                 ///< 模组配置信息
	NDS03_ChipInfo_t    chip_info;              ///< 模组设备信息
	uint8_t             data_cnt;               ///< 数据获取次数
	NDS03_RangingData_t ranging_data[NDS03_TARGET_MAX_NUM];     ///< 测距数据结果
	NDS03_Platform_t    platform;
} NDS03_Dev_t;

#define NDS03_DEFAULT_SLAVE_ADDR  0x5C

#if NDS03_PLATFORM == PLATFORM_C51
#define CHECK_RET(func)                  \
	ret = func;                          \
	if(ret != NDS03_ERROR_NONE)          \
		return ret;
#else
#define CHECK_RET(func)  do {                                            \
	ret = func;                                                          \
	if(ret != NDS03_ERROR_NONE) {                                        \
		NX_PRINTF("%s I2c Error, ret: %d\r\n", #func, ret);              \
		return ret;                                                      \
	}                                                                    \
} while (0)
#endif

#endif
