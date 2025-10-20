// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/ktime.h>
#include "nds03_iio.h"
#include "nds03.h"

#define NDS03_DISTANCE_CHANNEL {				\
	.type = IIO_DISTANCE,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |		\
				BIT(IIO_CHAN_INFO_SCALE) |	\
				BIT(IIO_CHAN_INFO_ENABLE),	\
	.info_mask_separate_available = BIT(IIO_CHAN_INFO_SCALE),\
	.scan_index = NDS03_IIO_CHAN_DISTANCE,			\
	.scan_type = {						\
		.sign = 'u',					\
		.realbits = 16,					\
		.storagebits = 16,				\
		.endianness = IIO_LE,				\
	},							\
}

static const int nds03_scales[] = { 1 };

static const struct iio_chan_spec nds03_channels[] = {
	NDS03_DISTANCE_CHANNEL,
	IIO_CHAN_SOFT_TIMESTAMP(NDS03_IIO_CHAN_TIMESTAMP),
};

static int nds03_read_avail(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			const int **vals, int *type, int *length,
			long mask)
{
	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		*type = IIO_VAL_INT;
		*vals = nds03_scales;
		*length = ARRAY_SIZE(nds03_scales);
		return IIO_AVAIL_LIST;
	default:
		return -EINVAL;
	}
}

/* === read_raw === */
static int nds03_read_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int *val, int *val2, long mask)
{
	struct nds03_iio_dev *iio = iio_device_get_drvdata(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		if (chan->scan_index == NDS03_IIO_CHAN_DISTANCE) {
			*val = iio->ctx->g_nds03_device.ranging_data[0].depth;
			return IIO_VAL_INT;
		}
		break;
	case IIO_CHAN_INFO_SCALE:
		*val = nds03_scales[0];
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_ENABLE:
		*val = iio->enabled;
		return IIO_VAL_INT;

	default:
		return -EINVAL;
	}

	return -EINVAL;
}

static int nds03_write_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int val, int val2, long mask)
{
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		return -EINVAL;
	case IIO_CHAN_INFO_SCALE:
		/* NOTE: Default to 1 */
		return 0;
	case IIO_CHAN_INFO_ENABLE:
		return -EINVAL;
	default:
		return -EINVAL;
	}
}

static const struct iio_info nds03_iio_info = {
	.read_raw = nds03_read_raw,
	.write_raw = nds03_write_raw,
	.read_avail= nds03_read_avail,
};

/* === buffer setup === */
static void nds03_enable_work(struct work_struct *work)
{
	struct nds03_iio_dev *iio = container_of(work, struct nds03_iio_dev, enable_work);
	struct nds03_context *ctx = iio->ctx;
	NDS03_Dev_t *dev = &ctx->g_nds03_device;
	int ret;

	mutex_lock(&ctx->work_mutex);
	if (iio->enabled)
		goto out;

	schedule_delayed_work(&ctx->dwork,
			msecs_to_jiffies(atomic_read(&ctx->poll_delay_ms)));
	atomic_set(&ctx->is_meas, 1);
	ret = nds03_sensor_init(ctx);
	if(ret != 0) {
		dev_err(&ctx->client->dev, "nds03 sensor init failed %d\n", ret);
		goto out;
	}
	ret = NDS03_StartContinuousMeasurement(dev);
	if (ret) {
		dev_err(&ctx->client->dev, "StartContinuous fail %d\n", ret);
		goto out;
	}
	iio->enabled = true;
	dev_info(&ctx->client->dev, "NDS03 sensor started\n");
	goto out;

out:
	mutex_unlock(&ctx->work_mutex);
}

static void nds03_disable_work(struct work_struct *work)
{
	struct nds03_iio_dev *iio = container_of(work, struct nds03_iio_dev, disable_work);
	struct nds03_context *ctx = iio->ctx;
	NDS03_Dev_t *dev = &ctx->g_nds03_device;

	mutex_lock(&ctx->work_mutex);
	if (!iio->enabled)
		goto out;

	atomic_set(&ctx->is_meas, 0);
	NDS03_StopContinuousMeasurement(dev);
	iio->enabled = false;
	dev_info(&ctx->client->dev, "NDS03 sensor stopped\n");
out:
	mutex_unlock(&ctx->work_mutex);
}

static int nds03_buffer_preenable(struct iio_dev *indio_dev)
{
	struct nds03_iio_dev *iio = iio_device_get_drvdata(indio_dev);

	schedule_work(&iio->enable_work);
	return 0;
}

static int nds03_buffer_predisable(struct iio_dev *indio_dev)
{
	struct nds03_iio_dev *iio = iio_device_get_drvdata(indio_dev);

	schedule_work(&iio->disable_work);
	return 0;
}

static const struct iio_buffer_setup_ops nds03_buffer_setup_ops = {
	.preenable  = nds03_buffer_preenable,
	.predisable = nds03_buffer_predisable,
};

int nds03_iio_init(struct nds03_context *ctx)
{
	struct device *dev = &ctx->client->dev;
	struct nds03_iio_dev *iio;
	int ret;

	iio = devm_kzalloc(dev, sizeof(*iio), GFP_KERNEL);
	if (!iio) {
		nds03_errmsg("devm_kzalloc error\n");
		return -ENOMEM;
	}

	iio->indio_dev = devm_iio_device_alloc(dev, 0);
	if (!iio->indio_dev) {
		nds03_errmsg("devm_iio_device_alloc error\n");
		return -ENOMEM;
	}

	iio->ctx = ctx;
	iio->enabled = false;
	INIT_WORK(&iio->enable_work, nds03_enable_work);
	INIT_WORK(&iio->disable_work, nds03_disable_work);

	iio->indio_dev->name = "nds03";
	iio->indio_dev->channels = nds03_channels;
	iio->indio_dev->num_channels = ARRAY_SIZE(nds03_channels);
	iio->indio_dev->info = &nds03_iio_info;
	iio->indio_dev->modes = INDIO_DIRECT_MODE | INDIO_BUFFER_SOFTWARE;
	iio_device_set_drvdata(iio->indio_dev, iio);

	/* kfifo buffer + setup_ops */
	ret = devm_iio_kfifo_buffer_setup(dev, iio->indio_dev, &nds03_buffer_setup_ops);
	if (ret)
		return ret;

	ret = devm_iio_device_register(dev, iio->indio_dev);
	if (ret)
		return ret;

	ctx->iio = iio;

	return 0;
}
EXPORT_SYMBOL_GPL(nds03_iio_init);

void nds03_iio_remove(struct nds03_context *ctx)
{
	return;
}
EXPORT_SYMBOL_GPL(nds03_iio_remove);

void nds03_iio_push_data(struct nds03_context *ctx, uint16_t distance_mm)
{
	struct nds03_iio_dev *iio = ctx->iio;
	struct nds03_scan scan;

	if (!iio || !iio->enabled)
		return;

	scan.distance = distance_mm;
	if (iio_buffer_enabled(iio->indio_dev))
		iio_push_to_buffers_with_timestamp(iio->indio_dev, &scan,
						ktime_get_boottime_ns());
}
EXPORT_SYMBOL_GPL(nds03_iio_push_data);
