// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include "libminiasync/vdm.h"
#include "core/util.h"

struct vdm {
	struct vdm_descriptor *descriptor;
	void *data;
};

/*
 * vdm_new -- returns NULL if failed to allocate memory for struct vdm
 * or vdm_data_init failed
 */
struct vdm *
vdm_new(struct vdm_descriptor *descriptor)
{
	struct vdm *vdm = malloc(sizeof(struct vdm));
	if (vdm == NULL || descriptor == NULL)
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

void
vdm_delete(struct vdm *vdm)
{
	struct vdm_descriptor *descriptor = vdm->descriptor;

	if (descriptor->vdm_data_fini)
		descriptor->vdm_data_fini(&vdm->data);

	free(vdm);
}

void *
vdm_get_data(struct vdm *vdm)
{
	return vdm->data;
}

static void
vdm_memcpy_cb(struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	if (data->notifier.notifier_used == FUTURE_NOTIFIER_WAKER)
		FUTURE_WAKER_WAKE(&data->notifier.waker);
	/*
	 * Setting the complete flag has to be done after calling
	 * the notifier because if the flag is set and runtime
	 * spins just after that, it is possible that the runtime
	 * will finish its work (because all complete flags
	 * will be set) and when the worker thread calls
	 * the notifier it will refer to main thread's stack address
	 * that is no longer relevant and cause segmentation fault.
	 */
	util_atomic_store32(&data->complete, 1);
}

static enum future_state
vdm_memcpy_impl(struct future_context *context, struct future_notifier *n)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	if (context->state == FUTURE_STATE_IDLE) {
		if (n) {
			data->notifier = *n;
		} else {
			data->notifier.notifier_used = FUTURE_NOTIFIER_NONE;
		}

		data->vdm_cb = vdm_memcpy_cb;
		data->vdm->descriptor->memcpy(data->vdm->descriptor,
			&data->notifier, context);
		if (n) {
			*n = data->notifier;
		}
	}

	return data->vdm->descriptor->check(context);
}

struct vdm_memcpy_future
vdm_memcpy(struct vdm *vdm, void *dest, void *src, size_t n, uint64_t flags)
{
	struct vdm_memcpy_future future;
	future.data.vdm = vdm;
	future.data.dest = dest;
	future.data.src = src;
	future.data.n = n;
	future.data.started = 0;
	future.data.complete = 0;
	future.output = (struct vdm_memcpy_output){NULL};
	future.data.flags = flags;
	FUTURE_INIT(&future, vdm_memcpy_impl);
	return future;
}

enum future_state
vdm_check(struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);

	int complete;
	util_atomic_load32(&data->complete, &complete);
	return (complete) ? FUTURE_STATE_COMPLETE : FUTURE_STATE_RUNNING;
}

enum future_state
vdm_check_async_start(struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	int started, complete;
	util_atomic_load32(&data->started, &started);
	util_atomic_load32(&data->complete, &complete);
	if (complete)
		return FUTURE_STATE_COMPLETE;
	if (started)
		return FUTURE_STATE_RUNNING;
	return FUTURE_STATE_IDLE;
}

static void
memcpy_sync(void *descriptor, struct future_notifier *notifier,
	struct future_context *context)
{
	notifier->notifier_used = FUTURE_NOTIFIER_NONE;

	struct vdm_memcpy_data *data = future_context_get_data(context);
	struct vdm_memcpy_output *output = future_context_get_output(context);
	output->dest = memcpy(data->dest, data->src, data->n);
	util_atomic_store32(&data->complete, 1);
}

static struct vdm_descriptor synchronous_descriptor = {
	.memcpy = memcpy_sync,
	.vdm_data_init = NULL,
	.vdm_data_fini = NULL,
	.check = vdm_check,
};

struct vdm_descriptor *
vdm_descriptor_synchronous(void)
{
	return &synchronous_descriptor;
}
