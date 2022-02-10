// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include "core/membuf.h"
#include "core/out.h"
#include "libminiasync/vdm.h"

/*
 * vdm_operation_impl -- the poll implementation for a generic vdm operation
 * The operation lifecycle is as follows:
 *	FUTURE_STATE_IDLE -- op_start() --> FUTURE_STATE_RUNNING
 *	FUTURE_STATE_RUNNING -- op_check() --> FUTURE_STATE_COMPLETE
 *	FUTURE_STATE_COMPLETE --> op_delete()
 */
static enum future_state
vdm_operation_impl(struct future_context *context, struct future_notifier *n)
{
	struct vdm_operation_data *data = future_context_get_data(context);
	struct vdm *vdm = membuf_ptr_user_data(data->op);

	if (context->state == FUTURE_STATE_IDLE) {
		if (vdm->op_start(data->op, n) != 0) {
			return FUTURE_STATE_IDLE;
		}
	}

	enum future_state state = vdm->op_check(data->op);

	if (state == FUTURE_STATE_COMPLETE) {
		struct vdm_operation_output *output =
			future_context_get_output(context);
		vdm->op_delete(data->op, output);
		/* variable data is no longer valid! */
	}

	return state;
}

/*
 * vdm_memcpy -- instantiates a new memcpy vdm operation and returns a new
 * future to represent that operation
 */
struct vdm_operation_future
vdm_memcpy(struct vdm *vdm, void *dest, void *src, size_t n, uint64_t flags)
{
	struct vdm_operation op;
	op.type = VDM_OPERATION_MEMCPY;
	op.memcpy.dest = dest;
	op.memcpy.flags = flags;
	op.memcpy.n = n;
	op.memcpy.src = src;

	struct vdm_operation_future future = {0};
	future.data.op = vdm->op_new(vdm, &op);
	FUTURE_INIT(&future, vdm_operation_impl);

	return future;
}

struct vdm_sync {
	struct vdm base; /* must be first */

	struct membuf *membuf;
};

struct vdm_sync_op {
	struct vdm_operation op;
	int complete;
};

/*
 * sync_operation_check -- always returns COMPLETE because sync mover operations
 * are complete immediately after starting.
 */
static enum future_state
sync_operation_check(void *op)
{
	struct vdm_sync_op *sync_op = op;

	int complete;
	util_atomic_load_explicit32(&sync_op->complete, &complete,
		memory_order_acquire);

	return complete ? FUTURE_STATE_COMPLETE : FUTURE_STATE_IDLE;
}

/*
 * sync_membuf_check -- checks the status of a sync job
 */
static enum membuf_check_result
sync_membuf_check(void *ptr, void *data)
{
	return sync_operation_check(ptr) == FUTURE_STATE_COMPLETE ?
		MEMBUF_PTR_CAN_REUSE : MEMBUF_PTR_IN_USE;
}

/*
 * sync_membuf_size -- returns the size of a sync operation
 */
static size_t
sync_membuf_size(void *ptr, void *data)
{
	return sizeof(struct vdm_sync_op);
}

/*
 * sync_operation_new -- creates a new sync operation
 */
static void *
sync_operation_new(struct vdm *vdm, const struct vdm_operation *operation)
{
	struct vdm_sync *vdm_sync = (struct vdm_sync *)vdm;
	struct vdm_sync_op *sync_op = membuf_alloc(vdm_sync->membuf,
		sizeof(struct vdm_sync_op));
	sync_op->op = *operation;
	sync_op->complete = 0;

	return sync_op;
}

/*
 * sync_operation_delete -- deletes sync operation
 */
static void
sync_operation_delete(void *op, struct vdm_operation_output *output)
{
	struct vdm_sync_op *sync_op = (struct vdm_sync_op *)op;
	switch (sync_op->op.type) {
		case VDM_OPERATION_MEMCPY:
			output->type = VDM_OPERATION_MEMCPY;
			output->memcpy.dest = sync_op->op.memcpy.dest;
			break;
		default:
			ASSERT(0);
	}
}

/*
 * sync_operation_start -- start (and perform) a synchronous memory operation
 */
static int
sync_operation_start(void *op, struct future_notifier *n)
{
	struct vdm_sync_op *sync_op = (struct vdm_sync_op *)op;
	if (n)
		n->notifier_used = FUTURE_NOTIFIER_NONE;
	memcpy(sync_op->op.memcpy.dest, sync_op->op.memcpy.src,
		sync_op->op.memcpy.n);

	util_atomic_store_explicit32(&sync_op->complete,
		1, memory_order_release);

	return 0;
}

static struct vdm vdm_synchronous_base = {
	.op_new = sync_operation_new,
	.op_delete = sync_operation_delete,
	.op_check = sync_operation_check,
	.op_start = sync_operation_start,
};

/*
 * vdm_synchronous_new -- creates a new synchronous data mover
 */
struct vdm *
vdm_synchronous_new(void)
{
	struct vdm_sync *vdm_sync = malloc(sizeof(struct vdm_sync));
	if (vdm_sync == NULL)
		return NULL;

	vdm_sync->base = vdm_synchronous_base;
	vdm_sync->membuf = membuf_new(sync_membuf_check, sync_membuf_size,
		NULL, vdm_sync);
	if (vdm_sync->membuf == NULL)
		goto membuf_failed;

	return &vdm_sync->base;

membuf_failed:
	free(vdm_sync);
	return NULL;
}

/*
 * vdm_synchronous_delete -- deletes a synchronous data mover
 */
void
vdm_synchronous_delete(struct vdm *vdm)
{
	struct vdm_sync *vdm_sync = (struct vdm_sync *)vdm;
	membuf_delete(vdm_sync->membuf);
	free(vdm_sync);
}
