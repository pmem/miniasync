/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021-2022, Intel Corporation */

/*
 * vdm.h - public definitions for an abstract virtual data mover (VDM) type.
 *
 * Virtual mover is an abstract type that software can use to generically
 * perform asynchronous memory operations. Libraries can use this to avoid
 * a hard dependency on any specific implementation of hardware offload for
 * memory operations.
 *
 * Data movers implementations can use DMA engines like
 * Intel DSA (Data Streaming Accelerator), plain threads,
 * or synchronous operations in the current working thread.
 *
 * Data movers need to implement the descriptor interface, and applications can
 * use such implementations to create a concrete mover. Software can then use
 * movers to create more complex generic concurrent futures that use
 * asynchronous memory operations.
 */

#ifndef VDM_H
#define VDM_H 1

#include "future.h"
#include "core/ringbuf.h"
#include "core/os_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

struct vdm;

enum vdm_operation_type {
	VDM_OPERATION_MEMCPY,
};

struct vdm_memcpy_data {
	void *dest;
	void *src;
	size_t n;
	uint64_t flags;
};

struct vdm_operation {
	enum vdm_operation_type type;
	union {
		struct vdm_memcpy_data memcpy;
	};
};

typedef int64_t (*vdm_operation_new)
	(void *vdm_data, const struct vdm_operation *operation);
typedef int (*vdm_operation_start)
	(void *vdm_data, int64_t operation_id, struct future_notifier *n);
typedef enum future_state (*vdm_operation_check)
	(void *vdm_data, int64_t operation_id);
typedef void (*vdm_operation_delete)
	(void *vdm_data, int64_t operation_id);

typedef int (*vdm_data_fn)(void **vdm_data);

struct vdm_operation_data {
	struct vdm *vdm;
	int64_t id;
};

struct vdm_operation_output {
	void *dest;
};

FUTURE(vdm_operation_future,
	struct vdm_operation_data, struct vdm_operation_output);

struct vdm_operation_future vdm_memcpy(struct vdm *vdm, void *dest, void *src,
		size_t n, uint64_t flags);

struct vdm_descriptor {
	vdm_data_fn vdm_data_init;
	vdm_data_fn vdm_data_fini;
	vdm_operation_new op_new;
	vdm_operation_delete op_delete;
	vdm_operation_start op_start;
	vdm_operation_check op_check;
};

struct vdm_descriptor *vdm_descriptor_synchronous(void);

struct vdm *vdm_new(struct vdm_descriptor *descriptor);
void vdm_delete(struct vdm *vdm);

#ifdef __cplusplus
}
#endif
#endif /* VDM_H */
