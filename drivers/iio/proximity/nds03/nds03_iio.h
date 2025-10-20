/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __NDS03_IIO_H
#define __NDS03_IIO_H

#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>

struct nds03_iio_dev {
	struct iio_dev *indio_dev;
	struct nds03_context *ctx;
	bool enabled;
	struct work_struct enable_work;
	struct work_struct disable_work;
};

enum nds03_iio_chan {
	NDS03_IIO_CHAN_DISTANCE,
	NDS03_IIO_CHAN_TIMESTAMP,
};

struct nds03_scan {
	__le16 distance;
	s64 ts;
};

int nds03_iio_init(struct nds03_context *ctx);
void nds03_iio_remove(struct nds03_context *ctx);
void nds03_iio_push_data(struct nds03_context *ctx, uint16_t distance_mm);

#endif
