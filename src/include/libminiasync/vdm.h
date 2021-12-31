/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021, Intel Corporation */

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

typedef void (*vdm_cb_fn)(struct future_context *context);
typedef int (*vdm_data_fn)(void **vdm_data);

struct vdm_memcpy_data {
	struct future_notifier notifier;
	int32_t started;
	int32_t complete;
	struct vdm *vdm;
	void *dest;
	void *src;
	size_t n;
	vdm_cb_fn vdm_cb;
	uint64_t flags;
	void *extra;
};

struct vdm_memcpy_output {
	void *dest;
};

FUTURE(vdm_memcpy_future,
	struct vdm_memcpy_data, struct vdm_memcpy_output);

struct vdm_memcpy_future vdm_memcpy(struct vdm *vdm, void *dest, void *src,
		size_t n, uint64_t flags);

typedef void (*async_memcpy_fn)(void *descriptor,
	struct future_notifier *notifier, struct future_context *context);

typedef enum future_state (*async_check_fn)(struct future_context *context);

struct vdm_descriptor {
	vdm_data_fn vdm_data_init;
	vdm_data_fn vdm_data_fini;
	async_memcpy_fn memcpy;
	async_check_fn check;
};

struct vdm_descriptor *vdm_descriptor_synchronous(void);

struct vdm *vdm_new(struct vdm_descriptor *descriptor);
void vdm_delete(struct vdm *vdm);
void *vdm_get_data(struct vdm *vdm);
enum future_state vdm_check(struct future_context *context);
enum future_state vdm_check_async_start(struct future_context *context);

#ifdef __cplusplus
}
#endif
#endif /* VDM_H */
