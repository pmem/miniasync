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

#ifdef __cplusplus
extern "C" {
#endif

struct vdm;

enum vdm_operation_type {
	VDM_OPERATION_MEMCPY,
	VDM_OPERATION_MEMMOVE,
	VDM_OPERATION_MEMSET,
};

struct vdm_operation_data_memcpy {
	void *dest;
	void *src;
	size_t n;
	uint64_t flags;
};

struct vdm_operation_data_memmove {
	void *dest;
	void *src;
	size_t n;
	uint64_t flags;
};

struct vdm_operation_data_memset {
	void *str;
	int c;
	size_t n;
	uint64_t flags;
};

/* sized so that sizeof(vdm_operation_data) is 64 */
#define VDM_OPERATION_DATA_MAX_SIZE (40)

struct vdm_operation {
	union {
		struct vdm_operation_data_memcpy memcpy;
		struct vdm_operation_data_memmove memmove;
		struct vdm_operation_data_memset memset;
		uint8_t data[VDM_OPERATION_DATA_MAX_SIZE];
	} data;
	enum vdm_operation_type type;
	uint32_t padding;
};

struct vdm_operation_data {
	void *data;
	struct vdm *vdm;
	struct vdm_operation operation;
};

struct vdm_operation_output_memcpy {
	void *dest;
};

struct vdm_operation_output_memmove {
	void *dest;
};

struct vdm_operation_output_memset {
	void *str;
};

struct vdm_operation_output {
	enum vdm_operation_type type; /* XXX: determine if needed */
	union {
		struct vdm_operation_output_memcpy memcpy;
		struct vdm_operation_output_memmove memmove;
		struct vdm_operation_output_memset memset;
	} output;
};

FUTURE(vdm_operation_future,
	struct vdm_operation_data, struct vdm_operation_output);

typedef void *(*vdm_operation_new)
	(struct vdm *vdm, const enum vdm_operation_type type);
typedef int (*vdm_operation_start)(void *data,
	const struct vdm_operation *operation,
	struct future_notifier *n);
typedef enum future_state (*vdm_operation_check)(void *data,
	const struct vdm_operation *operation);
typedef void (*vdm_operation_delete)(void *data,
	const struct vdm_operation *operation,
	struct vdm_operation_output *output);

struct vdm {
	vdm_operation_new op_new;
	vdm_operation_delete op_delete;
	vdm_operation_start op_start;
	vdm_operation_check op_check;
};

struct vdm *vdm_synchronous_new(void);
void vdm_synchronous_delete(struct vdm *vdm);

/*
 * vdm_operation_impl -- the poll implementation for a generic vdm operation
 * The operation lifecycle is as follows:
 *	FUTURE_STATE_IDLE -- op_start() --> FUTURE_STATE_RUNNING
 *	FUTURE_STATE_RUNNING -- op_check() --> FUTURE_STATE_COMPLETE
 *	FUTURE_STATE_COMPLETE --> op_delete()
 */
static inline enum future_state
vdm_operation_impl(struct future_context *context, struct future_notifier *n)
{
	struct vdm_operation_data *fdata =
		(struct vdm_operation_data *)future_context_get_data(context);
	struct vdm *vdm = fdata->vdm;

	if (context->state == FUTURE_STATE_IDLE) {
		if (vdm->op_start(fdata->data, &fdata->operation, n) != 0) {
			return FUTURE_STATE_IDLE;
		}
	}

	enum future_state state = vdm->op_check(fdata->data, &fdata->operation);

	if (state == FUTURE_STATE_COMPLETE) {
		struct vdm_operation_output *output =
			(struct vdm_operation_output *)
				future_context_get_output(context);
		vdm->op_delete(fdata->data, &fdata->operation, output);
		/* variable data is no longer valid! */
	}

	return state;
}

/*
 * vdm_memcpy -- instantiates a new memcpy vdm operation and returns a new
 * future to represent that operation
 */
static inline struct vdm_operation_future
vdm_memcpy(struct vdm *vdm, void *dest, void *src, size_t n, uint64_t flags)
{
	struct vdm_operation_future future = {.data.operation = {
		.type = VDM_OPERATION_MEMCPY,
		.data = {
			.memcpy.dest = dest,
			.memcpy.flags = flags,
			.memcpy.n = n,
			.memcpy.src = src,
		}
	}};
	future.data.data = vdm->op_new(vdm, VDM_OPERATION_MEMCPY);
	future.data.vdm = vdm;
	FUTURE_INIT(&future, vdm_operation_impl);

	return future;
}

/*
 * vdm_memmove -- instantiates a new memmove vdm operation and returns a new
 * future to represent that operation
 */
static inline struct vdm_operation_future
vdm_memmove(struct vdm *vdm, void *dest, void *src, size_t n, uint64_t flags)
{
	struct vdm_operation_future future = {.data.operation = {
		.type = VDM_OPERATION_MEMMOVE,
		.data = {
			.memcpy.dest = dest,
			.memcpy.flags = flags,
			.memcpy.n = n,
			.memcpy.src = src,
		}
	}};

	future.data.data = vdm->op_new(vdm, VDM_OPERATION_MEMMOVE);
	future.data.vdm = vdm;
	FUTURE_INIT(&future, vdm_operation_impl);

	return future;
}

/*
 * vdm_memset -- instantiates a new memset vdm operation and returns a new
 * future to represent that operation
 */
static inline struct vdm_operation_future
vdm_memset(struct vdm *vdm, void *str, int c, size_t n, uint64_t flags)
{
	struct vdm_operation_future future = {.data.operation = {
		.type = VDM_OPERATION_MEMSET,
		.data = {
			.memset.str = str,
			.memset.flags = flags,
			.memset.n = n,
			.memset.c = c,
		}
	}};

	future.data.data = vdm->op_new(vdm, VDM_OPERATION_MEMSET);
	future.data.vdm = vdm;
	FUTURE_INIT(&future, vdm_operation_impl);

	return future;
}

#ifdef __cplusplus
}
#endif
#endif /* VDM_H */
