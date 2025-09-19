
/*!
 @file nds03_module.c
 @brief
 @author lull
 @date 2025-06
 @copyright Copyright (c) 2025  Shenzhen Nephotonics  Semiconductor Technology Co., Ltd.
 @license BSD 3-Clause License
          This file is part of the Nephotonics sensor SDK.
          It is licensed under the BSD 3-Clause License.
          A copy of the license can be found in the project root directory, in the file named LICENSE.
 */
#include "nds03.h"
#include "nds03_iio.h"

/* Set default value to 1 to allow to see module insertion debug messages */
int nds03_enable_debug = 0;

static inline NDS03_Platform_t *to_nds03_platform(struct nds03_context *ctx)
{
	return &ctx->g_nds03_device.platform;
}
static inline NDS03_Dev_t * to_nds03_dev(struct nds03_context *ctx)
{
	return &ctx->g_nds03_device;
}

int nds03_interrupt_config(NDS03_Dev_t *pNxDevice, uint8_t is_open)
{
	int8_t retval = 0;

	if (is_open) {
		retval |= NDS03_SetGpio1Config(pNxDevice,
				NDS03_GPIO1_NEW_MEASURE_READY, NDS03_GPIO1_POLARITY_LOW);
		/* 连续模式开启测量 */
		retval |= NDS03_StartContinuousMeasurement(pNxDevice);
	} else {
		/* 连续模式关闭测量 */
		retval |= NDS03_StopContinuousMeasurement(pNxDevice);
		/* 关闭测距中断 */
		retval |= NDS03_SetGpio1Config(pNxDevice,
				NDS03_GPIO1_FUNCTIONALITY_OFF, NDS03_GPIO1_POLARITY_LOW);
	}
	return retval;
}
EXPORT_SYMBOL_GPL(nds03_interrupt_config);

int nds03_sensor_init(struct nds03_context *ctx)
{
	NDS03_Dev_t *pNxDevice = to_nds03_dev(ctx);

	NDS03_Platform_t *pdev = to_nds03_platform(ctx);
	/* 函数指针结构体 */

	if (NDS03_ERROR_NONE != nds03_platform_init(pdev, ctx->client)) {
		nds03_errmsg("nds03_platform init error\n");
		return -1;
	}
	/* 循环等待设备启动, 若模组或者IIC读写函数有问题则会报错 */
	if (NDS03_ERROR_NONE != NDS03_WaitDeviceBootUp(pNxDevice)) {
		nds03_errmsg("NDS03_WaitDeviceBootUp error\r\n");
		return -1;
	}
	/** 判断是否为NDS03设备 */
	if (NDS03_ERROR_NONE != NDS03_IsNDS03(pNxDevice)) {
		nds03_errmsg("The device is not NDS03, please change the device!\n");
		return -2;
	}
	/* 初始化模组设备 */
	if (NDS03_ERROR_NONE != NDS03_InitDevice(pNxDevice)) {
		nds03_errmsg("NDS03_InitDevice error!!\r\n");
		return -3;
	}
	if (atomic_read(&ctx->meas_mode) == 0)
		nds03_interrupt_config(pNxDevice, 1);
	else
		nds03_interrupt_config(pNxDevice, 0);

	return 0;
}
EXPORT_SYMBOL_GPL(nds03_sensor_init);

static void printf_ranging_data(NDS03_Dev_t *pNxDevice)
{
	nds03_info("ranging data start:\r\n");

	nds03_info("dist start:\r\n");
	nds03_info(
		"%d %d %d %d\r\n",
		pNxDevice->ranging_data[0].depth, pNxDevice->ranging_data[1].depth,
		pNxDevice->ranging_data[2].depth, pNxDevice->ranging_data[3].depth);
	nds03_info("dist end\r\n");

	nds03_info("confi start:\r\n");
	nds03_info(
		"%d %d %d %d\r\n",
		pNxDevice->ranging_data[0].confi, pNxDevice->ranging_data[1].confi,
		pNxDevice->ranging_data[2].confi, pNxDevice->ranging_data[3].confi);
	nds03_info("confi end\r\n");

	nds03_info("count start:\r\n");
	nds03_info(
		"%d %d %d %d\r\n",
		pNxDevice->ranging_data[0].count, pNxDevice->ranging_data[1].count,
		pNxDevice->ranging_data[2].count, pNxDevice->ranging_data[3].count);
	nds03_info("count end\r\n");

	nds03_info("crate start:\r\n");
	nds03_info(
		"%d %d %d %d\r\n",
		pNxDevice->ranging_data[0].crate, pNxDevice->ranging_data[1].crate,
		pNxDevice->ranging_data[2].crate, pNxDevice->ranging_data[3].crate);
	nds03_info("crate end\r\n");

	nds03_info("ranging data end\r\n");
}

static int32_t nds03_make_measure(struct nds03_context *ctx)
{
	int32_t	ret = 0;
	NDS03_Dev_t *pNxDevice = &ctx->g_nds03_device;

	mutex_lock(&ctx->work_mutex);
	/* 获取测量数据 */
	if (atomic_read(&ctx->meas_mode))
		ret = NDS03_GetSingleRangingData(pNxDevice);
	else
		ret = NDS03_GetInterruptRangingData(pNxDevice);

	if (ret >= 0 && nds03_enable_debug)
		printf_ranging_data(pNxDevice);

	mutex_unlock(&ctx->work_mutex);

	return ret;
}

static int __ctrl_tof_start(struct nds03_context * ctx)
{
	schedule_delayed_work(&ctx->dwork,
		msecs_to_jiffies(atomic_read(&ctx->poll_delay_ms)));
	atomic_set(&ctx->is_meas, 1);

	return 0;
}

static int __ctrl_tof_stop(struct nds03_context * ctx)
{
	atomic_set(&ctx->is_meas, 0);
	return 0;
}

static int __ctrl_tof_reset(struct nds03_context * ctx)
{
	int ret = 0;

	__ctrl_tof_stop(ctx);
	mutex_lock(&ctx->work_mutex);
	ret = nds03_sensor_init(ctx);
	if (ret != 0)
		nds03_errmsg("nds03 sensor init failed\n");

	mutex_unlock(&ctx->work_mutex);
	return ret;
}

static int32_t tof_offset_calib(struct nds03_context *ctx, uint16_t calib_dist)
{
	int32_t     i,  cnt = 20;
	int32_t     depth_sum, depth_aver;
	NDS03_Dev_t *pNxDevice = to_nds03_dev(ctx);
	/* 将设备处于500mm处 */
	NDS03_StopContinuousMeasurement(pNxDevice);
	msleep(100);
	/* 串扰标定 */
	ctx->calib_result = NDS03_XtalkCalibration(pNxDevice);
	if (NDS03_ERROR_NONE != ctx->calib_result) {
		nds03_info("Xtalk calib error: %d\n", ctx->calib_result);
		return -1;
	}
	/* Offset标定 */
	ctx->calib_result = NDS03_OffsetCalibrationAtDepth(pNxDevice, calib_dist);
	if(NDS03_ERROR_NONE != ctx->calib_result) {
		nds03_info("Offset calib error: %d\n", ctx->calib_result);
		return -1;
	}
	/* 获取平均值 */
	NDS03_GetSingleRangingData(pNxDevice);
	NDS03_GetSingleRangingData(pNxDevice);
	depth_sum = 0;
	for (i = 0; i < cnt; i++) {
		if (NDS03_ERROR_NONE != NDS03_GetSingleRangingData(pNxDevice)) {
			nds03_info("NDS03_GetSingleRangingData error!!\r\n");
			return -1;
		}
		depth_sum = depth_sum + (int32_t)(uint32_t)pNxDevice->ranging_data[0].depth;
	}
	depth_aver = depth_sum / cnt;
	if (depth_aver < (calib_dist - 20) || depth_aver > (calib_dist + 20)) {
		ctx->calib_result = NDS03_ERROR_RANGING;
		nds03_info("NDS03 calibration fail!!\r\n");
		return -1;
	}
	nds03_info("NDS03 calibration success\r\n");
	return 0;
}

static void report_meas_event(struct nds03_context * ctx)
{
	int retval = 0;

	retval = nds03_make_measure(ctx);
	if (retval < 0)
		return;

	uint16_t distance = ctx->g_nds03_device.ranging_data[0].depth;
	nds03_iio_push_data(ctx, distance);
}

static irqreturn_t tof_irq_handler_i2c(int vec, void *info)
{
	struct nds03_context *ctx = (struct nds03_context *)info;
	bool is_meas = atomic_read(&ctx->is_meas);

	if (ctx->irq == vec && is_meas)
		schedule_work(&ctx->irq_work);

	return IRQ_HANDLED;
}

static void nds03_work_handler(struct work_struct *work)
{
	struct nds03_context *ctx = container_of(work, struct nds03_context, dwork.work);

	if (atomic_read(&ctx->meas_mode) && atomic_read(&ctx->is_meas)) {
		nds03_make_measure(ctx);
		schedule_delayed_work(&ctx->dwork,
			msecs_to_jiffies(atomic_read(&ctx->poll_delay_ms)));
		report_meas_event(ctx);
	}
}

static void nds03_measure_irq_work(struct work_struct *work)
{
	struct nds03_context *ctx = container_of(work, struct nds03_context, irq_work);

	report_meas_event(ctx);
}

static ssize_t nds03_show_is_meas(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct nds03_context *ctx = dev_get_drvdata(dev);
	return snprintf(buf, 10, "%d\n", atomic_read(&ctx->is_meas));
}

static ssize_t nds03_store_is_meas(struct device *dev,
				struct device_attribute *attr, const char *buf,size_t count)
{
	struct nds03_context *ctx = dev_get_drvdata(dev);
	int ret = count;

	switch (buf[0]) {
	case '0':
		__ctrl_tof_stop(ctx);
		break;
	case '1':
		__ctrl_tof_start(ctx);
		break;
	default:
		nds03_warnmsg("Invalid value\n");
		ret = -EINVAL;
		break;
	}
	return ret ;
}

static DEVICE_ATTR(is_meas, 0660, nds03_show_is_meas, nds03_store_is_meas);

static ssize_t nds03_show_poll_delay_ms(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	uint32_t poll_ms;
	struct nds03_context *ctx = dev_get_drvdata(dev);

	poll_ms = atomic_read(&ctx->poll_delay_ms);
	return snprintf(buf, 10, "%d\n", poll_ms);
}

static ssize_t nds03_store_poll_delay_ms(struct device *dev,
				struct device_attribute *attr, const char *buf,size_t count)
{
	struct nds03_context *ctx = dev_get_drvdata(dev);
	int ret;
	unsigned long delay_ms;

	ret = kstrtoul(buf, 10, &delay_ms);
	if(ret < 0) {
		nds03_warnmsg("Invalid input ctx\n");
		goto store_err;
	}
	atomic_set(&ctx->poll_delay_ms, delay_ms);
	nds03_dbgmsg("Poll delay %lu ms\n", delay_ms);
store_err:
	return ret < 0 ? -EINVAL : count;
}

static DEVICE_ATTR(meas_delay_ms, 0660, nds03_show_poll_delay_ms, nds03_store_poll_delay_ms);

static ssize_t nds03_show_meas_mode(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct nds03_context *ctx = dev_get_drvdata(dev);

	return snprintf(buf, 10, "%d\n",atomic_read(&ctx->meas_mode));
}

static ssize_t nds03_store_meas_mode(struct device *dev,
				struct device_attribute *attr, const char *buf,size_t count)
{
	struct nds03_context *ctx = dev_get_drvdata(dev);
	NDS03_Dev_t *pNxDevice = to_nds03_dev(ctx);
	int ret = count;

	mutex_lock(&ctx->work_mutex);
	switch (buf[0]) {
		case '0':
			if(ctx->irq < 0){
				nds03_warnmsg("No support Interrupt\n");
				ret = -EINVAL;
			} else {
				atomic_set(&ctx->meas_mode, 0);
				nds03_interrupt_config(pNxDevice, 1);
				nds03_dbgmsg("Enter Interrupt Mode\n");
			}
			break;
		case '1':
			atomic_set(&ctx->meas_mode, 1);
			nds03_interrupt_config(pNxDevice, 0);
			nds03_dbgmsg("Enter Poll Mode\n");
			break;
		default:
			nds03_warnmsg("Invalid value\n");
			ret = -EINVAL;
			break;
	}
	mutex_unlock(&ctx->work_mutex);
	return ret ;
}
static DEVICE_ATTR(meas_mode, 0660, nds03_show_meas_mode, nds03_store_meas_mode);

static ssize_t nds03_store_tof_reset(struct device *dev,
				struct device_attribute *attr, const char *buf,size_t count)
{
	struct nds03_context *ctx = dev_get_drvdata(dev);
	int ret = 0;

	if(buf[0] == '1')
		ret = __ctrl_tof_reset(ctx);

	return ret < 0 ? ret : count;
}

static DEVICE_ATTR(tof_reset, 0660, NULL, nds03_store_tof_reset);

static ssize_t nds03_show_tof_calib(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct nds03_context *ctx = dev_get_drvdata(dev);

	return snprintf(buf, 10, "%d\n",ctx->calib_result);
}

static ssize_t nds03_store_tof_calib(struct device *dev,
				struct device_attribute *attr, const char *buf,size_t count)
{
	struct nds03_context *ctx = dev_get_drvdata(dev);
	int ret;
	unsigned long calib_dis;

	mutex_lock(&ctx->work_mutex);
	ret = kstrtoul(buf, 10, &calib_dis);
	if(ret < 0) {
		nds03_warnmsg("Invalid input ctx\n");
		ret = -EPERM;
		goto store_err;
	}
	nds03_info("offset calib distance: %ld mm\n", calib_dis);
	ret = tof_offset_calib(ctx, calib_dis);
	if (nds03_sensor_init(ctx) != 0)
		nds03_errmsg("nds03 sensor init failed\n");

store_err:
	mutex_unlock(&ctx->work_mutex);
	return ret < 0 ? ret : count;
}

static DEVICE_ATTR(tof_calib, 0660, nds03_show_tof_calib, nds03_store_tof_calib);

static ssize_t nds03_show_enable_debug(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", nds03_enable_debug);
}

static ssize_t nds03_store_enable_debug(struct device *dev,
				struct device_attribute *attr, const char *buf,size_t count)
{
	struct nds03_context *ctx = dev_get_drvdata(dev);
	int32_t ret = 0;

	mutex_lock(&ctx->work_mutex);
	switch (buf[0]) {
		case '0':
			nds03_enable_debug = 0;
			nds03_info("close nds03 debug\n");
			break;
		case '1':
			nds03_enable_debug = 1;
			nds03_info("open nds03 debug\n");
			break;
		default:
			nds03_warnmsg("Invalid value\n");
			ret = -EINVAL;
			break;
	}
	mutex_unlock(&ctx->work_mutex);
	return ret < 0 ? ret : count;
}

static DEVICE_ATTR(enable_debug, 0660, nds03_show_enable_debug, nds03_store_enable_debug);

static ssize_t nds03_store_tof_pulsenum(struct device *dev,
				struct device_attribute *attr, const char *buf,size_t count)
{
	int ret;
	unsigned long pulsenum;
	struct nds03_context *ctx = dev_get_drvdata(dev);
	NDS03_Dev_t *pdev = to_nds03_dev(ctx);

	ret = kstrtoul(buf, 10, &pulsenum);
	if (ret < 0) {
		nds03_errmsg("Invalid input ctx\n");
		return -EPERM;
	}
	mutex_lock(&ctx->work_mutex);
	ret = NDS03_SetPulseNum(pdev, pulsenum);
	mutex_unlock(&ctx->work_mutex);
	nds03_info("set tof pulsenum: %ld, ret: %d\n", pulsenum, ret);
	return ret < 0 ? ret : count;
}

static DEVICE_ATTR(tof_pulsenum, 0660, NULL, nds03_store_tof_pulsenum);

static ssize_t nds03_show_tof_xtalk(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret, count;
	uint16_t xtalk_value;
	struct nds03_context *ctx = dev_get_drvdata(dev);
	NDS03_Dev_t *pdev = to_nds03_dev(ctx);

	mutex_lock(&ctx->work_mutex);
	ret = NDS03_GetXTalkValue(pdev, &xtalk_value);
	if (ret < 0)
		nds03_errmsg("get tof xtalk_value error\n");
	mutex_unlock(&ctx->work_mutex);
	count = snprintf(buf, 10, "%d\n",xtalk_value);
	return ret < 0 ? ret : count;
}

static DEVICE_ATTR(tof_xtalk, 0660, nds03_show_tof_xtalk, NULL);

static ssize_t nds03_store_xtalk_calibration(struct device *dev,
				struct device_attribute *attr, const char *buf,size_t count)
{
	int32_t ret = 0;
	struct nds03_context *ctx = dev_get_drvdata(dev);
	NDS03_Dev_t *pdev = to_nds03_dev(ctx);

	if (sysfs_streq(buf, "1") || sysfs_streq(buf, "on") ||
					sysfs_streq(buf, "start")) {
		mutex_lock(&ctx->work_mutex);
		NDS03_StopContinuousMeasurement(pdev);
		ret = NDS03_XtalkCalibration(pdev);
		if (nds03_sensor_init(ctx) != 0)
			nds03_errmsg("nds03 sensor init failed\n");

		mutex_unlock(&ctx->work_mutex);
		if(ret < 0) {
			nds03_errmsg("NDS03_XtalkCalibration error, ret = %d\n", ret);
			return -EIO;
		}
		return count;
	} else {
		return -EINVAL;
	}
}

static DEVICE_ATTR(xtalk_calibration, 0660, NULL, nds03_store_xtalk_calibration);

static ssize_t nds03_store_offset_calibration(struct device *dev,
				struct device_attribute *attr, const char *buf,size_t count)
{
	int ret;
	struct nds03_context *ctx = dev_get_drvdata(dev);
	NDS03_Dev_t *pdev = to_nds03_dev(ctx);
	int32_t calib_depth_mm_tmp = 0;

	ret = kstrtoint(buf, 10, &calib_depth_mm_tmp);
	if (ret)
		return -EINVAL;

	mutex_lock(&ctx->work_mutex);
	NDS03_StopContinuousMeasurement(pdev);
	ret = NDS03_OffsetCalibrationAtDepth(pdev, calib_depth_mm_tmp);
	if (nds03_sensor_init(ctx) != 0)
		nds03_errmsg("nds03 sensor init failed\n");

	mutex_unlock(&ctx->work_mutex);
	if (ret < 0) {
		nds03_errmsg("NDS03_OffsetCalibrationAtDepth error, ret = %d\n", ret);
		return -EIO;
	}

	return count;
}

static DEVICE_ATTR(offset_calibration, 0660, NULL, nds03_store_offset_calibration);

static struct attribute *nds03_attributes[] = {
	&dev_attr_is_meas.attr,
	&dev_attr_meas_delay_ms.attr,
	&dev_attr_meas_mode.attr,
	&dev_attr_enable_debug.attr,
	&dev_attr_tof_reset.attr,
	&dev_attr_tof_calib.attr,
	&dev_attr_tof_pulsenum.attr,
	&dev_attr_tof_xtalk.attr,
	&dev_attr_xtalk_calibration.attr,
	&dev_attr_offset_calibration.attr,
	NULL
};

static const struct attribute_group nds03_sysfs_groups = {
	.attrs = nds03_attributes,
};

static void nds03_parse_device_tree(struct nds03_context *ctx)
{
	int32_t ret;
	NDS03_Platform_t *pdev = to_nds03_platform(ctx);

	/* Initialize xshut gpio */
	pdev->xshut_gpio = devm_gpiod_get_optional(&ctx->client->dev, "xshut", GPIOD_OUT_HIGH);
	if (IS_ERR(pdev->xshut_gpio)) {
		ret = PTR_ERR(pdev->xshut_gpio);
		nds03_warnmsg( "no xshut pin available, error = %d\n", ret);
		return;
	} else {
		nds03_dbgmsg("get xshut pin success\n");
	}

	/* Initialize irq gpio*/
	atomic_set(&ctx->meas_mode, 1);  //Default to polling mode
	ctx->irq = -1;
	pdev->intr_gpio = devm_gpiod_get_optional(&ctx->client->dev, "intr", GPIOD_IN);
	if (IS_ERR(pdev->intr_gpio)) {
		ret = PTR_ERR(pdev->intr_gpio);
		nds03_warnmsg( "no intr pin available, error = %d\n", ret);
		return;
	}
	nds03_dbgmsg("get intr pin success\n");
	ctx->irq = ctx->client->irq;
	if (ctx->irq) {
		unsigned long default_trigger = irqd_get_trigger_type(irq_get_irq_data(ctx->irq));
		ret = devm_request_threaded_irq(&ctx->client->dev,
					ctx->irq, NULL,
					tof_irq_handler_i2c,
					default_trigger|IRQF_ONESHOT,
					"nds03_interrupt",
					(void *)ctx);
			if (ret) {
				nds03_errmsg("fail to req threaded irq rc=%d\n", ret);
			} else {
				nds03_info(
					"request irq success, irq mode use, irq num: %d, type: %lu",
					ctx->irq, default_trigger);
				atomic_set(&ctx->meas_mode, 0);  //Configure in interrupt mode
			}
	} else {
		void *poll_interval_dt = NULL;
		uint32_t delay_ms;
		nds03_info("no irq number specified, polling mode is used\n");
		poll_interval_dt = (void *)of_get_property(ctx->client->dev.of_node,
									"nds03_poll_interval",
									NULL);
		delay_ms = poll_interval_dt ? be32_to_cpup(poll_interval_dt) : 0;

		nds03_dbgmsg("poll delay ms:%d\n", delay_ms);

		atomic_set(&ctx->poll_delay_ms, delay_ms);
	}
}

int nds03_common_probe(struct nds03_context * ctx)
{
	int ret = 0;
	u32	i2c_freq  = 0;
	// struct input_dev *input;

	/* Initialize mutex */
	mutex_init(&ctx->work_mutex);

	/* init work handler */
	INIT_DELAYED_WORK(&ctx->dwork, nds03_work_handler);

	INIT_WORK(&ctx->irq_work, nds03_measure_irq_work);

	atomic_set(&ctx->is_meas, 0);

	/* Parse the device tree for NDS03 sensor configuration. */
	nds03_parse_device_tree(ctx);
	of_property_read_u32(ctx->client->adapter->dev.of_node, "clock-frequency", &i2c_freq);
	pr_info("I2C bus number is %d, bus speed: %u Hz\n", ctx->client->adapter->nr, i2c_freq);

	ret = nds03_iio_init(ctx);
	if (ret) {
		nds03_errmsg("IIO init failed: %d\n", ret);
		goto err_iio;
	}
	ctx->fd_open_count = 0;

	nds03_dbgmsg("create sysfs group successfully\n");

	/* Initialize nds03 sensor */
	ret = nds03_sensor_init(ctx);
	if (ret != 0 ) {
		nds03_errmsg("Failed to init nds03 sensor error:%d\n", ret);
		goto err_iio;
	}
	nds03_dbgmsg("init nds03 sensor successfully\n");

	ret = sysfs_create_group(&ctx->client->dev.kobj, &nds03_sysfs_groups);
	if (ret) {
		nds03_errmsg("Failed to create sysfs group error:%d\n", ret);
		goto exit_err;
	}
	ctx->remove_flag = false;
	nds03_dbgmsg("register chardev successfully\n");

	nds03_info("nds03 module registered successfully\n");
	nds03_info( "NDS03 Driver version: %s \n", DRIVER_VERSION);
	return 0;

err_iio:
	nds03_iio_remove(ctx);

exit_err:
	return ret;
}
EXPORT_SYMBOL_GPL(nds03_common_probe);

int nds03_common_remove(struct nds03_context * ctx)
{
	nds03_dbgmsg("Enter %s \n", __FUNCTION__);
	cancel_delayed_work(&ctx->dwork);
	ctx->remove_flag = true;
	sysfs_remove_group(&ctx->client->dev.kobj, &nds03_sysfs_groups);
	nds03_iio_remove(ctx);

	return 0;
}
EXPORT_SYMBOL_GPL(nds03_common_remove);
