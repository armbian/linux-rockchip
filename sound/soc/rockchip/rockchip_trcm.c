// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip TRCM Pcm Driver
 *
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 * Author: Sugar Zhang <sugar.zhang@rock-chips.com>
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "rockchip_trcm.h"

#define DMA_GUARD_BUFFER_SIZE		64

static unsigned int prealloc_buffer_size_kbytes = 512;
module_param(prealloc_buffer_size_kbytes, uint, 0444);
MODULE_PARM_DESC(prealloc_buffer_size_kbytes, "Preallocate DMA buffer size (KB).");

struct dmaengine_dma_guard {
	dma_addr_t dma_addr;
	unsigned char *dma_area;
};

struct dmaengine_trcm {
	struct device *dev;
	struct dma_chan *chan[SNDRV_PCM_STREAM_LAST + 1];
	struct dmaengine_dma_guard guard[SNDRV_PCM_STREAM_LAST + 1];
	struct snd_soc_component component;
	bool always_on;
};

struct dmaengine_trcm_runtime_data {
	struct dmaengine_trcm *parent;
	struct dma_chan *dma_chan;
	dma_cookie_t cookie;

	unsigned int frame_bytes;
	unsigned int channels;
	int stream;
};

static inline ssize_t trcm_channels_to_bytes(struct dmaengine_trcm_runtime_data *prtd,
					     int channels)
{
	return (prtd->frame_bytes / prtd->channels) * channels;
}

static inline ssize_t trcm_frames_to_bytes(struct dmaengine_trcm_runtime_data *prtd,
					   snd_pcm_sframes_t size)
{
	return size * prtd->frame_bytes;
}

static inline snd_pcm_sframes_t trcm_bytes_to_frames(struct dmaengine_trcm_runtime_data *prtd,
						     ssize_t size)
{
	return size / prtd->frame_bytes;
}

static inline struct dmaengine_trcm *soc_component_to_trcm(struct snd_soc_component *p)
{
	return container_of(p, struct dmaengine_trcm, component);
}

static inline struct dmaengine_trcm_runtime_data *substream_to_prtd(
	const struct snd_pcm_substream *substream)
{
	if (!substream->runtime)
		return NULL;

	return substream->runtime->private_data;
}

static struct dma_chan *snd_dmaengine_trcm_get_chan(struct snd_pcm_substream *substream)
{
	struct dmaengine_trcm_runtime_data *prtd = substream_to_prtd(substream);

	return prtd->dma_chan;
}

static struct device *dmaengine_dma_dev(struct dmaengine_trcm *trcm,
	struct snd_pcm_substream *substream)
{
	if (!trcm->chan[substream->stream])
		return NULL;

	return trcm->chan[substream->stream]->device->dev;
}

static int dmaengine_trcm_hw_params(struct snd_soc_component *component,
				   struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params)
{
	struct dmaengine_trcm_runtime_data *prtd = substream_to_prtd(substream);
	struct dma_chan *chan = snd_dmaengine_trcm_get_chan(substream);
	struct dma_slave_config slave_config;
	int ret;

	memset(&slave_config, 0, sizeof(slave_config));

	ret = snd_dmaengine_pcm_prepare_slave_config(substream, params, &slave_config);
	if (ret)
		return ret;

	ret = dmaengine_slave_config(chan, &slave_config);
	if (ret)
		return ret;

	prtd->frame_bytes = snd_pcm_format_size(params_format(params),
						params_channels(params));
	prtd->channels = params_channels(params);

	return 0;
}

static int
dmaengine_pcm_set_runtime_hwparams(struct snd_soc_component *component,
				   struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct dmaengine_trcm *trcm = soc_component_to_trcm(component);
	struct device *dma_dev = dmaengine_dma_dev(trcm, substream);
	struct dma_chan *chan = trcm->chan[substream->stream];
	struct snd_dmaengine_dai_dma_data *dma_data;
	struct snd_pcm_hardware hw;

	if (rtd->dai_link->num_cpus > 1) {
		dev_err(rtd->dev,
			"%s doesn't support Multi CPU yet\n", __func__);
		return -EINVAL;
	}

	dma_data = snd_soc_dai_get_dma_data(asoc_rtd_to_cpu(rtd, 0), substream);

	memset(&hw, 0, sizeof(hw));
	hw.info = SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID |
			SNDRV_PCM_INFO_INTERLEAVED;
	hw.periods_min = 2;
	hw.periods_max = UINT_MAX;
	hw.period_bytes_min = 256;
	hw.period_bytes_max = dma_get_max_seg_size(dma_dev);
	hw.buffer_bytes_max = SIZE_MAX;
	hw.fifo_size = dma_data->fifo_size;

	snd_dmaengine_pcm_refine_runtime_hwparams(substream,
						  dma_data,
						  &hw,
						  chan);

	return snd_soc_set_runtime_hwparams(substream, &hw);
}

static int dmaengine_trcm_open(struct snd_soc_component *component,
			      struct snd_pcm_substream *substream)
{
	struct dmaengine_trcm *trcm = soc_component_to_trcm(component);
	struct dma_chan *chan = trcm->chan[substream->stream];
	struct dmaengine_trcm_runtime_data *prtd;
	int ret;

	if (!chan)
		return -ENXIO;

	ret = dmaengine_pcm_set_runtime_hwparams(component, substream);
	if (ret)
		return ret;

	ret = snd_pcm_hw_constraint_integer(substream->runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		return ret;

	prtd = kzalloc(sizeof(*prtd), GFP_KERNEL);
	if (!prtd)
		return -ENOMEM;

	prtd->parent = trcm;
	prtd->stream = substream->stream;
	prtd->dma_chan = chan;

	substream->runtime->private_data = prtd;

	return 0;
}

static int dmaengine_trcm_close(struct snd_soc_component *component,
			       struct snd_pcm_substream *substream)
{
	struct dmaengine_trcm_runtime_data *prtd = substream_to_prtd(substream);

	dmaengine_synchronize(prtd->dma_chan);

	kfree(prtd);

	return 0;
}

static snd_pcm_uframes_t dmaengine_trcm_pointer(
	struct snd_soc_component *component,
	struct snd_pcm_substream *substream)
{
	struct dmaengine_trcm_runtime_data *prtd = substream_to_prtd(substream);
	struct dma_tx_state state;
	unsigned int buf_size;
	unsigned int pos = 0;

	dmaengine_tx_status(prtd->dma_chan, prtd->cookie, &state);
	buf_size = snd_pcm_lib_buffer_bytes(substream);
	if (state.residue > 0 && state.residue <= buf_size)
		pos = buf_size - state.residue;

	return trcm_bytes_to_frames(prtd, pos);
}

static void dmaengine_trcm_dma_complete(void *arg)
{
	struct snd_pcm_substream *substream = arg;

	if (!substream->runtime)
		return;

	snd_pcm_period_elapsed(substream);
}

static int dmaengine_trcm_prepare_and_submit(struct snd_pcm_substream *substream)
{
	struct dmaengine_trcm_runtime_data *prtd = substream_to_prtd(substream);
	struct dma_chan *chan = prtd->dma_chan;
	struct dma_async_tx_descriptor *desc;
	enum dma_transfer_direction direction;
	unsigned long flags = DMA_CTRL_ACK;

	direction = snd_pcm_substream_to_dma_direction(substream);

	if (!substream->runtime->no_period_wakeup)
		flags |= DMA_PREP_INTERRUPT;

	desc = dmaengine_prep_dma_cyclic(chan,
		substream->runtime->dma_addr,
		snd_pcm_lib_buffer_bytes(substream),
		snd_pcm_lib_period_bytes(substream), direction, flags);

	if (!desc)
		return -ENOMEM;

	desc->callback = dmaengine_trcm_dma_complete;
	desc->callback_param = substream;
	prtd->cookie = dmaengine_submit(desc);

	return 0;
}

int dmaengine_trcm_dma_guard_ctrl(struct snd_soc_component *component,
				  int stream, bool en)
{
	struct dmaengine_trcm *trcm = soc_component_to_trcm(component);
	struct dmaengine_dma_guard *guard = &trcm->guard[stream];
	struct dma_chan *chan = trcm->chan[stream];
	struct dma_async_tx_descriptor *desc;
	enum dma_transfer_direction direction;

	if (!chan)
		return 0;

	if (!en)
		return dmaengine_terminate_async(chan);

	direction = stream ? DMA_DEV_TO_MEM : DMA_MEM_TO_DEV;

	desc = dmaengine_prep_dma_cyclic(chan, guard->dma_addr,
					 DMA_GUARD_BUFFER_SIZE,
					 DMA_GUARD_BUFFER_SIZE,
					 direction,
					 DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc) {
		dev_err(component->dev, "Failed to get dma desc\n");
		return -ENOMEM;
	}

	desc->callback = NULL;
	desc->callback_param = NULL;
	dmaengine_submit(desc);
	dma_async_issue_pending(chan);

	return 0;
}
EXPORT_SYMBOL_GPL(dmaengine_trcm_dma_guard_ctrl);

static int dmaengine_trcm_trigger(struct snd_soc_component *component,
				 struct snd_pcm_substream *substream, int cmd)
{
	struct dmaengine_trcm_runtime_data *prtd = substream_to_prtd(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret;

#ifdef TRCM_DEBUG
	ktime_t start_time, stop_time, diff_time;

	start_time = ktime_get();
#endif
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		dmaengine_terminate_async(prtd->dma_chan);
		ret = dmaengine_trcm_prepare_and_submit(substream);
		if (ret)
			return ret;
		dma_async_issue_pending(prtd->dma_chan);
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		dmaengine_resume(prtd->dma_chan);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (runtime->info & SNDRV_PCM_INFO_PAUSE)
			dmaengine_pause(prtd->dma_chan);
		else
			dmaengine_terminate_async(prtd->dma_chan);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		dmaengine_pause(prtd->dma_chan);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		dmaengine_terminate_async(prtd->dma_chan);
		dmaengine_trcm_dma_guard_ctrl(component, substream->stream, 1);
		break;
	default:
		return -EINVAL;
	}

#ifdef TRCM_DEBUG
	stop_time = ktime_get();
	diff_time = ktime_sub(stop_time, start_time);
	dev_dbg(component->dev, "cmd: %d time cost %lld\n",
		cmd, ktime_to_us(diff_time));
#endif

	return 0;
}

static int dmaengine_trcm_dma_guard_new(struct snd_soc_component *component,
					struct snd_soc_pcm_runtime *rtd)
{
	struct dmaengine_trcm *trcm = soc_component_to_trcm(component);
	struct snd_dmaengine_dai_dma_data *dma_data;
	struct snd_pcm_substream *substream;
	struct snd_soc_dai *dai;
	struct dma_chan *chan;
	struct dma_slave_config slave_config;
	struct device *dev;
	dma_addr_t dma_addr;
	unsigned char *dma_area;
	unsigned int i;
	int ret;

	for_each_pcm_streams(i) {
		substream = rtd->pcm->streams[i].substream;
		if (!substream)
			continue;
		dev = dmaengine_dma_dev(trcm, substream);
		chan = trcm->chan[i];

		dma_area = dma_alloc_coherent(dev, DMA_GUARD_BUFFER_SIZE,
					      &dma_addr, GFP_KERNEL);
		if (!dma_area)
			return -ENOMEM;

		memset(dma_area, 0x0, DMA_GUARD_BUFFER_SIZE);

		trcm->guard[i].dma_addr = dma_addr;
		trcm->guard[i].dma_area = dma_area;

		memset(&slave_config, 0, sizeof(slave_config));

		dma_data = snd_soc_dai_get_dma_data(asoc_rtd_to_cpu(rtd, 0),
						    substream);
		snd_dmaengine_pcm_set_config_from_dai_data(substream, dma_data,
							   &slave_config);

		/*
		 * Use the max-16w to cover all 2^n cases, maybe better
		 * per channels and fmt, at the moment, we use the simple
		 * way.
		 */
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			slave_config.direction = DMA_MEM_TO_DEV;
			slave_config.dst_maxburst = 16;
		} else {
			slave_config.direction = DMA_DEV_TO_MEM;
			slave_config.src_maxburst = 16;
		}

		ret = dmaengine_slave_config(chan, &slave_config);
		if (ret)
			return ret;
	}

	if (trcm->always_on) {
		/* Start the first one will auto trigger bstream guard. */
		for_each_pcm_streams(i) {
			substream = rtd->pcm->streams[i].substream;
			if (!substream)
				continue;
			dai = asoc_rtd_to_cpu(rtd, 0);
			if (!dai)
				continue;

			dmaengine_trcm_dma_guard_ctrl(component, substream->stream, 1);
			ret = dai->driver->ops->trigger(substream,
							SNDRV_PCM_TRIGGER_START,
							dai);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int dmaengine_trcm_new(struct snd_soc_component *component,
			     struct snd_soc_pcm_runtime *rtd)
{
	struct dmaengine_trcm *trcm = soc_component_to_trcm(component);
	struct snd_pcm_substream *substream;
	size_t prealloc_buffer_size;
	size_t max_buffer_size;
	unsigned int i;
	int ret;

	prealloc_buffer_size = prealloc_buffer_size_kbytes * 1024;
	max_buffer_size = SIZE_MAX;

	ret = dmaengine_trcm_dma_guard_new(component, rtd);
	if (ret)
		return ret;

	for_each_pcm_streams(i) {
		substream = rtd->pcm->streams[i].substream;
		if (!substream)
			continue;

		if (!trcm->chan[i]) {
			dev_err(component->dev,
				"Missing dma channel for stream: %d\n", i);
			return -EINVAL;
		}

		snd_pcm_set_managed_buffer(substream,
				SNDRV_DMA_TYPE_DEV_IRAM,
				dmaengine_dma_dev(trcm, substream),
				prealloc_buffer_size,
				max_buffer_size);

		if (rtd->pcm->streams[i].pcm->name[0] == '\0') {
			strscpy_pad(rtd->pcm->streams[i].pcm->name,
				    rtd->pcm->streams[i].pcm->id,
				    sizeof(rtd->pcm->streams[i].pcm->name));
		}
	}

	return 0;
}

static const struct snd_soc_component_driver dmaengine_trcm_component = {
	.name		= SND_DMAENGINE_TRCM_DRV_NAME,
	.probe_order	= SND_SOC_COMP_ORDER_LATE,
	.open		= dmaengine_trcm_open,
	.close		= dmaengine_trcm_close,
	.hw_params	= dmaengine_trcm_hw_params,
	.trigger	= dmaengine_trcm_trigger,
	.pointer	= dmaengine_trcm_pointer,
	.pcm_construct	= dmaengine_trcm_new,
};

static const char * const dmaengine_pcm_dma_channel_names[] = {
	[SNDRV_PCM_STREAM_PLAYBACK] = "tx",
	[SNDRV_PCM_STREAM_CAPTURE] = "rx",
};

static int dmaengine_pcm_request_chan_of(struct dmaengine_trcm *trcm,
	struct device *dev, const struct snd_dmaengine_pcm_config *config)
{
	unsigned int i;
	const char *name;
	struct dma_chan *chan;

	for_each_pcm_streams(i) {
		name = dmaengine_pcm_dma_channel_names[i];
		chan = dma_request_chan(dev, name);
		if (IS_ERR(chan)) {
			/*
			 * Only report probe deferral errors, channels
			 * might not be present for devices that
			 * support only TX or only RX.
			 */
			if (PTR_ERR(chan) == -EPROBE_DEFER)
				return -EPROBE_DEFER;
			trcm->chan[i] = NULL;
		} else {
			trcm->chan[i] = chan;
		}
	}

	return 0;
}

static void dmaengine_pcm_release_chan(struct dmaengine_trcm *trcm)
{
	unsigned int i;

	for_each_pcm_streams(i) {
		if (!trcm->chan[i])
			continue;
		dma_release_channel(trcm->chan[i]);
	}
}

/**
 * snd_dmaengine_trcm_register - Register a dmaengine based TRCM device
 * @dev: The parent device for the TRCM device
 */
static int snd_dmaengine_trcm_register(struct device *dev)
{
	const struct snd_soc_component_driver *driver;
	struct dmaengine_trcm *trcm;
	int ret;

	trcm = kzalloc(sizeof(*trcm), GFP_KERNEL);
	if (!trcm)
		return -ENOMEM;

	trcm->dev = dev;

	trcm->always_on = device_property_read_bool(dev, "rockchip,always-on");

#ifdef CONFIG_DEBUG_FS
	trcm->component.debugfs_prefix = "dma";
#endif
	ret = dmaengine_pcm_request_chan_of(trcm, dev, NULL);
	if (ret)
		goto err_free_dma;

	driver = &dmaengine_trcm_component;

	ret = snd_soc_component_initialize(&trcm->component, driver, dev);
	if (ret)
		goto err_free_dma;

	ret = snd_soc_add_component(&trcm->component, NULL, 0);
	if (ret)
		goto err_free_dma;

	dev_info(dev, "Register PCM for TRCM mode\n");

	return 0;

err_free_dma:
	dmaengine_pcm_release_chan(trcm);
	kfree(trcm);
	return ret;
}

/**
 * snd_dmaengine_trcm_unregister - Removes a dmaengine based TRCM device
 * @dev: Parent device the TRCM was register with
 *
 * Removes a dmaengine based TRCM device previously registered with
 * snd_dmaengine_trcm_register.
 */
static void snd_dmaengine_trcm_unregister(struct device *dev)
{
	struct snd_soc_component *component;
	struct dmaengine_trcm *trcm;

	component = snd_soc_lookup_component(dev, SND_DMAENGINE_TRCM_DRV_NAME);
	if (!component)
		return;

	trcm = soc_component_to_trcm(component);

	snd_soc_unregister_component_by_driver(dev, component->driver);
	dmaengine_pcm_release_chan(trcm);
	kfree(trcm);
}

static void devm_dmaengine_trcm_release(struct device *dev, void *res)
{
	snd_dmaengine_trcm_unregister(*(struct device **)res);
}

/**
 * devm_snd_dmaengine_trcm_register - resource managed dmaengine TRCM registration
 * @dev: The parent device for the TRCM device
 *
 * Register a dmaengine based TRCM device with automatic unregistration when the
 * device is unregistered.
 */
int devm_snd_dmaengine_trcm_register(struct device *dev)
{
	struct device **ptr;
	int ret;

	ptr = devres_alloc(devm_dmaengine_trcm_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	ret = snd_dmaengine_trcm_register(dev);
	if (ret == 0) {
		*ptr = dev;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(devm_snd_dmaengine_trcm_register);

MODULE_LICENSE("GPL");
