/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Framework for buffer objects that can be shared across devices/subsystems.
 *
 * Copyright(C) 2015 Intel Ltd
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _DMA_BUF_UAPI_H_
#define _DMA_BUF_UAPI_H_

#include <linux/types.h>

/* begin/end dma-buf functions used for userspace mmap. */
struct dma_buf_sync {
	__u64 flags;
};

#define DMA_BUF_SYNC_READ      (1 << 0)
#define DMA_BUF_SYNC_WRITE     (2 << 0)
#define DMA_BUF_SYNC_RW        (DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE)
#define DMA_BUF_SYNC_START     (0 << 2)
#define DMA_BUF_SYNC_END       (1 << 2)
#define DMA_BUF_SYNC_VALID_FLAGS_MASK \
	(DMA_BUF_SYNC_RW | DMA_BUF_SYNC_END)

#define DMA_BUF_NAME_LEN	32

/**
 * struct dma_buf_export_sync_file - Get a sync_file from a dma-buf
 *
 * Userspace can perform a DMA_BUF_IOCTL_EXPORT_SYNC_FILE to retrieve the
 * current set of fences on a dma-buf file descriptor as a sync_file.  CPU
 * waits via poll() or other driver-specific mechanisms typically wait on
 * whatever fences are on the dma-buf at the time the wait begins.  This
 * is similar except that it takes a snapshot of the current fences on the
 * dma-buf for waiting later instead of waiting immediately.  This is
 * useful for modern graphics APIs such as Vulkan which assume an explicit
 * synchronization model but still need to inter-operate with dma-buf.
 *
 * The intended usage pattern is the following:
 *
 *  1. Export a sync_file with flags corresponding to the expected GPU usage
 *     via DMA_BUF_IOCTL_EXPORT_SYNC_FILE.
 *
 *  2. Submit rendering work which uses the dma-buf.  The work should wait on
 *     the exported sync file before rendering and produce another sync_file
 *     when complete.
 *
 *  3. Import the rendering-complete sync_file into the dma-buf with flags
 *     corresponding to the GPU usage via DMA_BUF_IOCTL_IMPORT_SYNC_FILE.
 *
 * Unlike doing implicit synchronization via a GPU kernel driver's exec ioctl,
 * the above is not a single atomic operation.  If userspace wants to ensure
 * ordering via these fences, it is the respnosibility of userspace to use
 * locks or other mechanisms to ensure that no other context adds fences or
 * submits work between steps 1 and 3 above.
 */
struct dma_buf_export_sync_file {
	/**
	 * @flags: Read/write flags
	 *
	 * Must be DMA_BUF_SYNC_READ, DMA_BUF_SYNC_WRITE, or both.
	 *
	 * If DMA_BUF_SYNC_READ is set and DMA_BUF_SYNC_WRITE is not set,
	 * the returned sync file waits on any writers of the dma-buf to
	 * complete.  Waiting on the returned sync file is equivalent to
	 * poll() with POLLIN.
	 *
	 * If DMA_BUF_SYNC_WRITE is set, the returned sync file waits on
	 * any users of the dma-buf (read or write) to complete.  Waiting
	 * on the returned sync file is equivalent to poll() with POLLOUT.
	 * If both DMA_BUF_SYNC_WRITE and DMA_BUF_SYNC_READ are set, this
	 * is equivalent to just DMA_BUF_SYNC_WRITE.
	 */
	__u32 flags;
	/** @fd: Returned sync file descriptor */
	__s32 fd;
};

/**
 * struct dma_buf_import_sync_file - Insert a sync_file into a dma-buf
 *
 * Userspace can perform a DMA_BUF_IOCTL_IMPORT_SYNC_FILE to insert a
 * sync_file into a dma-buf for the purposes of implicit synchronization
 * with other dma-buf consumers.  This allows clients using explicitly
 * synchronized APIs such as Vulkan to inter-op with dma-buf consumers
 * which expect implicit synchronization such as OpenGL or most media
 * drivers/video.
 */
struct dma_buf_import_sync_file {
	/**
	 * @flags: Read/write flags
	 *
	 * Must be DMA_BUF_SYNC_READ, DMA_BUF_SYNC_WRITE, or both.
	 *
	 * If DMA_BUF_SYNC_READ is set and DMA_BUF_SYNC_WRITE is not set,
	 * this inserts the sync_file as a read-only fence.  Any subsequent
	 * implicitly synchronized writes to this dma-buf will wait on this
	 * fence but reads will not.
	 *
	 * If DMA_BUF_SYNC_WRITE is set, this inserts the sync_file as a
	 * write fence.  All subsequent implicitly synchronized access to
	 * this dma-buf will wait on this fence.
	 */
	__u32 flags;
	/** @fd: Sync file descriptor */
	__s32 fd;
};

#define DMA_BUF_BASE		'b'
#define DMA_BUF_IOCTL_SYNC	_IOW(DMA_BUF_BASE, 0, struct dma_buf_sync)

/* 32/64bitness of this uapi was botched in android, there's no difference
 * between them in actual uapi, they're just different numbers.
 */
#define DMA_BUF_SET_NAME	_IOW(DMA_BUF_BASE, 1, const char *)
#define DMA_BUF_SET_NAME_A	_IOW(DMA_BUF_BASE, 1, __u32)
#define DMA_BUF_SET_NAME_B	_IOW(DMA_BUF_BASE, 1, __u64)
#define DMA_BUF_IOCTL_EXPORT_SYNC_FILE	_IOWR(DMA_BUF_BASE, 2, struct dma_buf_export_sync_file)
#define DMA_BUF_IOCTL_IMPORT_SYNC_FILE	_IOW(DMA_BUF_BASE, 3, struct dma_buf_import_sync_file)

struct dma_buf_sync_partial {
	__u64 flags;
	__u32 offset;
	__u32 len;
};

#define DMA_BUF_IOCTL_SYNC_PARTIAL	_IOW(DMA_BUF_BASE, 2, struct dma_buf_sync_partial)

#endif
