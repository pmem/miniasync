// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include "libminiasync/vdm.h"
#include "core/util.h"

struct vdm {
	struct vdm_descriptor *descriptor;
	void *data;
};

/*
 * vdm_new -- returns NULL if failed to allocate memory
 * for struct vdm or vdm_data_init failed.
 */
struct vdm *
vdm_new(struct vdm_descriptor *descriptor)
{
	struct vdm *vdm = malloc(sizeof(struct vdm));
	if (vdm == NULL)
		return NULL;

	vdm->descriptor = descriptor;
	vdm->data = NULL;

	if (descriptor->vdm_data_init) {
		if (descriptor->vdm_data_init(&vdm->data) != 0) {
			free(vdm);
			return NULL;
		}
	}

	return vdm;
}

/*
 * vdm_delete -- deletes a mover instance
 */
void
vdm_delete(struct vdm *vdm)
{
	struct vdm_descriptor *descriptor = vdm->descriptor;

	if (descriptor->vdm_data_fini)
		descriptor->vdm_data_fini(&vdm->data);

	free(vdm);
}

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
	void *vdata = data->vdm->data;

	if (context->state == FUTURE_STATE_IDLE) {
		if (data->vdm->descriptor->op_start(vdata, data->id, n) != 0) {
			return FUTURE_STATE_IDLE;
		}
	}

	enum future_state state =
		data->vdm->descriptor->op_check(vdata, data->id);

	if (state == FUTURE_STATE_COMPLETE) {
		struct vdm_operation_output *output =
			future_context_get_output(context);
		output->dest = vdata;

		data->vdm->descriptor->op_delete(vdata, data->id);
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
	future.data.id = vdm->descriptor->op_new(vdm->data, &op);
	future.data.vdm = vdm;
	future.output.dest = NULL;
	FUTURE_INIT(&future, vdm_operation_impl);

	return future;
}

/*
 * sync_operation_new -- creates a new sync operation
 */
static int64_t
sync_operation_new(void *vdm_data, const struct vdm_operation *operation)
{
	struct vdm_operation *sync_op = malloc(sizeof(*sync_op));
	*sync_op = *operation;

	return (int64_t)sync_op;
}

/*
 * sync_operation_delete -- deletes sync operation
 */
static void
sync_operation_delete(void *vdm_data, int64_t op_id)
{
	struct vdm_operation *sync_op = (struct vdm_operation *)op_id;
	free(sync_op);
}

/*
 * sync_operation_check -- always returns COMPLETE because sync mover operations
 * are complete immediately after starting.
 */
static enum future_state
sync_operation_check(void *vdm_data, int64_t op_id)
{
	return FUTURE_STATE_COMPLETE;
}

/*
 * sync_operation_start -- start (and perform) a synchronous memory operation
 */
static int
sync_operation_start(void *vdm_data, int64_t op_id, struct future_notifier *n)
{
	struct vdm_operation *sync_op = (struct vdm_operation *)op_id;
	n->notifier_used = FUTURE_NOTIFIER_NONE;
	memcpy(sync_op->memcpy.dest, sync_op->memcpy.src, sync_op->memcpy.n);

	return 0;
}

static struct vdm_descriptor synchronous_descriptor = {
	.vdm_data_init = NULL,
	.vdm_data_fini = NULL,
	.op_new = sync_operation_new,
	.op_delete = sync_operation_delete,
	.op_check = sync_operation_check,
	.op_start = sync_operation_start,
};

/*
 * vdm_descriptor_synchronous -- returns a synchronous memory mover descriptor
 */
struct vdm_descriptor *
vdm_descriptor_synchronous(void)
{
	return &synchronous_descriptor;
}
