/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 */

#ifndef __RKNPU3_IOCTL_H__
#define __RKNPU3_IOCTL_H__

#include <linux/types.h>

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/errno.h>
#else
#include <sys/ioctl.h>
#include <errno.h>
#endif

/* NPU alignment and CPU cacheline alignment */
#define RKNPU3_ALIGN_SIZE (64)
#define RKNPU3_CHECK_ALIGN(size) ((size) & (RKNPU3_ALIGN_SIZE - 1))

/* NPU command stack size */
#define RKNPU3_TASK_STACK_SIZE (64 * 1024)

#define RKNPU3_STR_HELPER(x) #x

#define RKNPU3_GET_DRV_VERSION_STRING(MAJOR, MINOR, PATCHLEVEL) \
	RKNPU3_STR_HELPER(MAJOR)                                \
	"." RKNPU3_STR_HELPER(MINOR) "." RKNPU3_STR_HELPER(PATCHLEVEL)
#define RKNPU3_GET_DRV_VERSION_CODE(MAJOR, MINOR, PATCHLEVEL) \
	(MAJOR * 10000 + MINOR * 100 + PATCHLEVEL)
#define RKNPU3_GET_DRV_VERSION_MAJOR(CODE) (CODE / 10000)
#define RKNPU3_GET_DRV_VERSION_MINOR(CODE) ((CODE % 10000) / 100)
#define RKNPU3_GET_DRV_VERSION_PATCHLEVEL(CODE) (CODE % 100)

/* Maximum core count */
#define RKNPU3_MAX_CORES 8

#define RKNPU3_AXI_PORT_MAX_NUM 4

/* Core ID base */
#define RKNPU3_CORE_ID_BASE 0
#define RKNPU3_LOCAL_CORE_NUM 8

/* Memory sync mode */
#define RKNPU3_MEM_SYNC_TO_DEVICE   (1 << 0)
#define RKNPU3_MEM_SYNC_FROM_DEVICE (1 << 1)
#define RKNPU3_MEM_SYNC_MASK        (RKNPU3_MEM_SYNC_TO_DEVICE | RKNPU3_MEM_SYNC_FROM_DEVICE)

/* Memory type */
#define RKNPU3_MEM_TYPE_DEFAULT    (0 << 0)
#define RKNPU3_MEM_TYPE_CACHEABLE  (0 << 1)
#define RKNPU3_MEM_TYPE_CONTIGUOUS (1 << 0)
#define RKNPU3_MEM_TYPE_UNCACHED   (1 << 1)

/* Task state */
#define RKNPU3_TASK_STATE_IDLE      0
#define RKNPU3_TASK_STATE_READY     1
#define RKNPU3_TASK_STATE_RUNNING   2
#define RKNPU3_TASK_STATE_COMPLETED 3
#define RKNPU3_TASK_STATE_ERROR     4

/* Error code definitions */
#define RKNPU3_SUCCESS                0             /* Success */
#define RKNPU3_ERROR_INVALID_PARAM    (-EINVAL)     /* Invalid parameter */
#define RKNPU3_ERROR_TIMEOUT          (-ETIMEDOUT)  /* Timeout */
#define RKNPU3_ERROR_BUSY             (-EBUSY)      /* Device busy */
#define RKNPU3_ERROR_NO_MEMORY        (-ENOMEM)     /* Out of memory */
#define RKNPU3_ERROR_OPERATION_FAILED (-EIO)        /* Operation failed */
#define RKNPU3_ERROR_CORE_BUSY        (-EBUSY)      /* Core busy */
#define RKNPU3_ERROR_NOT_INITIALIZED  (-ENODEV)     /* Not initialized */
#define RKNPU3_ERROR_TASK_FULL        (-ENOSPC)     /* Task queue full */
#define RKNPU3_ERROR_QUEUE_FULL       (-ENOSPC)     /* Priority queue full */
#define RKNPU3_ERROR_INVALID_TASK     (-EINVAL)     /* Invalid task */
#define RKNPU3_ERROR_UNSUPPORTED      (-EOPNOTSUPP) /* Unsupported operation */

/**
 * struct rknpu3_mem_create - Memory create descriptor
 * @core_id: Core ID
 * @flags: Memory flags
 * @size: Memory size
 * @dma_buf_fd: DMA-BUF file descriptor for userspace mmap
 * @virt_kaddr: Virtual kernel address
 * @dma_addr: DMA address
 * @reserved: Reserved
 */
struct rknpu3_mem_create {
	__u32 core_id;
	__u32 flags;
	__u64 size;
	__s32 dma_buf_fd;
	__u64 virt_kaddr;
	__u64 dma_addr;
	__u32 reserved;
} __packed;

/**
 * struct rknpu3_mem_sync - Memory sync descriptor
 * @dma_addr: DMA address
 * @offset: Offset
 * @size: Size
 * @flags: Sync flags
 * @reserved: Reserved
 */
struct rknpu3_mem_sync {
	__u64 dma_addr;
	__u64 offset;
	__u64 size;
	__u32 flags;
	__s32 dma_buf_fd;
	__u32 reserved;
} __packed;

/**
 * struct rknpu3_mem_import - DMA-BUF import descriptor
 * @dma_buf_fd: Input: DMA-BUF file descriptor to import
 * @flags: Reserved flags
 * @size: Output: Buffer size
 * @dma_addr: Output: DMA physical address
 * @virt_kaddr: Output: Kernel virtual address
 * @handle: Output: Internal handle for subsequent operations
 */
struct rknpu3_mem_import {
	__s32 dma_buf_fd;
	__u32 flags;
	__u64 size;
	__u64 dma_addr;
	__u64 virt_kaddr;
	__u64 handle;
} __packed;

/**
 * struct rknpu3_task - Task descriptor
 * @task_id: Task ID (kernel allocated)
 * @core_id: Core ID (userspace set)
 * @bin_size: Binary buffer size (userspace set)
 * @priority: Priority (userspace set)
 * @state: Task state (kernel updated)
 * @bin_kaddr: Binary buffer kernel virtual address (userspace set)
 * @bin_dma_addr: Binary buffer local DMA address (userspace set)
 * @enable_cycle_count: Whether to enable cycle counting (kernel set)
 * @reserved: Reserved fields for alignment
 */
struct rknpu3_task {
	__u32 task_id;
	__u32 core_id;
	__u32 bin_size;
	__u32 priority;
	__u32 state;
	__u64 bin_kaddr;
	__u32 bin_dma_addr;
	__u32 enable_cycle_count;
	__u32 reserved[7];
} __packed;

/**
 * struct rknpu3_task_submit - Task submit descriptor
 * @task_start: Task array start index (userspace set)
 * @task_count: Task count (userspace set)
 * @timeout_ms: Timeout in milliseconds (userspace set)
 * @enable_cycle_count: Whether to enable cycle counting (userspace set)
 * @task_array_kaddr: Task array kernel virtual address (userspace set)
 * @start_time_us: Start time in microseconds (kernel filled)
 * @exec_time_us: Execution time in microseconds (kernel filled)
 * @reserved: Reserved fields for alignment
 */
struct rknpu3_task_submit {
	__u32 task_start;
	__u32 task_count;
	__u32 timeout_ms;
	__u32 enable_cycle_count;
	__u64 task_array_kaddr;
	__u64 start_time_us;
	__u64 exec_time_us;
	__u32 reserved[6];
} __packed;

/**
 * struct rknpu3_core_mem_usage - Per-core memory usage
 * @core_id: Core ID
 * @total_size: Total memory size
 * @used_size: Used memory size
 */
struct rknpu3_core_mem_usage {
	__u32 core_id;
	__u64 total_size;
	__u64 used_size;
} __packed;

/**
 * struct rknpu3_mem_usage - All cores memory usage
 * @actual_core_count: Actual core count
 * @core_usage: Per-core memory usage array
 */
struct rknpu3_mem_usage {
	__u32 actual_core_count;
	struct rknpu3_core_mem_usage core_usage[RKNPU3_MAX_CORES];
} __packed;

/**
 * struct rknpu3_core_load - Single core load info
 * @core_id: Core ID
 * @load: Load percentage * 100 (0-10000)
 */
struct rknpu3_core_load {
	__u32 core_id;
	__u32 load;
} __packed;

/**
 * struct rknpu3_load - All cores load info
 * @actual_core_count: Actual core count
 * @core_load: Per-core load array
 */
struct rknpu3_load {
	__u32 actual_core_count;
	struct rknpu3_core_load core_load[RKNPU3_MAX_CORES];
} __packed;

/**
 * struct rknpu3_port_data - Single bus port data info
 * @fd_write: FD write data amount
 * @fd_read: FD read data amount
 * @wt_read: WT read data amount
 */
struct rknpu3_port_data {
	__u32 fd_write;
	__u32 fd_read;
	__u32 wt_read;
} __packed;

/**
 * struct rknpu3_core_bw - Single core bandwidth info
 * @core_id: Core ID
 * @port: Port data array
 */
struct rknpu3_core_bw {
	__u32 core_id;
	struct rknpu3_port_data port[RKNPU3_AXI_PORT_MAX_NUM];
} __packed;

/**
 * struct rknpu3_bw_info - All cores bandwidth info
 * @actual_core_count: Actual core count
 * @actual_port_count: Actual port count
 * @core_bw: Per-core bandwidth info
 */
struct rknpu3_bw_info {
	__u32 actual_core_count;
	__u32 actual_port_count;
	struct rknpu3_core_bw core_bw[RKNPU3_MAX_CORES];
} __packed;

/**
 * struct rknpu3_core_cycle - Single core cycle count
 * @core_id: Core ID
 * @cycle_count: Cycle count value
 */
struct rknpu3_core_cycle {
	__u32 core_id;
	__u64 cycle_count;
} __packed;

/**
 * struct rknpu3_cycle_info - All cores cycle count info
 * @actual_core_count: Actual core count
 * @core_cycle: Per-core cycle info
 */
struct rknpu3_cycle_info {
	__u32 actual_core_count;
	struct rknpu3_core_cycle core_cycle[RKNPU3_MAX_CORES];
} __packed;

/* IOCTL command number definitions */
#define RKNPU3_IOCTL_CMD_SUBMIT_TASK       0
#define RKNPU3_IOCTL_CMD_WAIT_TASK         1
#define RKNPU3_IOCTL_CMD_MEM_CREATE        2
#define RKNPU3_IOCTL_CMD_MEM_DESTROY       3
#define RKNPU3_IOCTL_CMD_GET_MEM_USAGE     4
#define RKNPU3_IOCTL_CMD_RESET_CORE        5
#define RKNPU3_IOCTL_CMD_GET_DRV_VERSION   6
#define RKNPU3_IOCTL_CMD_GET_CORE_LOAD     7
#define RKNPU3_IOCTL_CMD_MEM_SYNC          8
#define RKNPU3_IOCTL_CMD_MEM_IMPORT        9
#define RKNPU3_IOCTL_CMD_MEM_RELEASE       10
#define RKNPU3_IOCTL_CMD_GET_HW_VERSION    11
#define RKNPU3_IOCTL_CMD_GET_BW_INFO       12
#define RKNPU3_IOCTL_CMD_GET_CYCLE_INFO    13

/* IOCTL macro definitions */
#define RKNPU3_IOC_MAGIC 'r'
#define RKNPU3_IO(nr)         _IO(RKNPU3_IOC_MAGIC, nr)
#define RKNPU3_IOW(nr, type)  _IOW(RKNPU3_IOC_MAGIC, nr, type)
#define RKNPU3_IOR(nr, type)  _IOR(RKNPU3_IOC_MAGIC, nr, type)
#define RKNPU3_IOWR(nr, type) _IOWR(RKNPU3_IOC_MAGIC, nr, type)

/* IOCTL command macros */
#define IOCTL_RKNPU3_SUBMIT_TASK    RKNPU3_IOWR(RKNPU3_IOCTL_CMD_SUBMIT_TASK, struct rknpu3_task_submit)
#define IOCTL_RKNPU3_WAIT_TASK      RKNPU3_IOWR(RKNPU3_IOCTL_CMD_WAIT_TASK, struct rknpu3_task_submit)
#define IOCTL_RKNPU3_MEM_CREATE     RKNPU3_IOWR(RKNPU3_IOCTL_CMD_MEM_CREATE, struct rknpu3_mem_create)
#define IOCTL_RKNPU3_MEM_DESTROY    RKNPU3_IOWR(RKNPU3_IOCTL_CMD_MEM_DESTROY, struct rknpu3_mem_create)
#define IOCTL_RKNPU3_GET_MEM_USAGE  RKNPU3_IOR(RKNPU3_IOCTL_CMD_GET_MEM_USAGE, struct rknpu3_mem_usage)
#define IOCTL_RKNPU3_RESET_CORE     RKNPU3_IOW(RKNPU3_IOCTL_CMD_RESET_CORE, __u32)
#define IOCTL_RKNPU3_GET_DRV_VERSION RKNPU3_IOR(RKNPU3_IOCTL_CMD_GET_DRV_VERSION, __u32)
#define IOCTL_RKNPU3_GET_CORE_LOAD  RKNPU3_IOR(RKNPU3_IOCTL_CMD_GET_CORE_LOAD, struct rknpu3_load)
#define IOCTL_RKNPU3_MEM_SYNC       RKNPU3_IOW(RKNPU3_IOCTL_CMD_MEM_SYNC, struct rknpu3_mem_sync)
#define IOCTL_RKNPU3_MEM_IMPORT     RKNPU3_IOWR(RKNPU3_IOCTL_CMD_MEM_IMPORT, struct rknpu3_mem_import)
#define IOCTL_RKNPU3_MEM_RELEASE    RKNPU3_IOW(RKNPU3_IOCTL_CMD_MEM_RELEASE, __u64)
#define IOCTL_RKNPU3_GET_HW_VERSION RKNPU3_IOR(RKNPU3_IOCTL_CMD_GET_HW_VERSION, __u64)
#define IOCTL_RKNPU3_GET_BW_INFO    RKNPU3_IOR(RKNPU3_IOCTL_CMD_GET_BW_INFO, struct rknpu3_bw_info)
#define IOCTL_RKNPU3_GET_CYCLE_INFO RKNPU3_IOR(RKNPU3_IOCTL_CMD_GET_CYCLE_INFO, struct rknpu3_cycle_info)

#endif /* __RKNPU3_IOCTL_H__ */
