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

struct vdm;

typedef void (*vdm_cb_fn)(struct future_context *context);
typedef void (*vdm_data_fn)(void **vdm_data);

struct vdm_memcpy_data {
	struct future_notifier notifier;
	int complete;
	struct vdm *vdm;
	void *dest;
	void *src;
	size_t n;
	vdm_cb_fn vdm_cb;
	void *extra;
};

struct vdm_memcpy_output {
	void *dest;
};

FUTURE(vdm_memcpy_future,
	struct vdm_memcpy_data, struct vdm_memcpy_output);

struct vdm_memcpy_future vdm_memcpy(struct vdm *vdm,
	void *dest, void *src, size_t n);

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
struct vdm_descriptor *vdm_descriptor_pthreads(void);
struct vdm_descriptor *vdm_descriptor_pthreads_polled(void);
struct vdm *vdm_new(struct vdm_descriptor *descriptor);
void vdm_delete(struct vdm *vdm);
void *vdm_get_data(struct vdm *vdm);

#endif
