// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include <stdlib.h>
#include <string.h>

#include "libminiasync/vdm.h"
#include "core/os_thread.h"
#include "core/util.h"

struct vdm {
	struct vdm_descriptor *descriptor;
	void *data;
};

struct vdm *
vdm_new(struct vdm_descriptor *descriptor)
{
	struct vdm *vdm = malloc(sizeof(struct vdm));
	vdm->descriptor = descriptor;
	vdm->data = NULL;

	if (descriptor->vdm_data_init)
		descriptor->vdm_data_init(&vdm->data);

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
	util_atomic_store32(&data->complete, 1);
	if (data->notifier.notifier_used == FUTURE_NOTIFIER_WAKER)
		FUTURE_WAKER_WAKE(&data->notifier.waker);
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
	future.data.complete = 0;
	future.output = (struct vdm_memcpy_output){ NULL };
	future.data.flags = flags;
	FUTURE_INIT(&future, vdm_memcpy_impl);

	return future;
}

static void
memcpy_impl(void *descriptor, struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	struct vdm_memcpy_output *output = future_context_get_output(context);
	output->dest = memcpy(data->dest, data->src, data->n);
	data->vdm_cb(context);
}

static enum future_state
vdm_check(struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);

	int complete;
	util_atomic_load32(&data->complete, &complete);
	return (complete) ? FUTURE_STATE_COMPLETE : FUTURE_STATE_RUNNING;
}

static void
memcpy_sync(void *descriptor, struct future_notifier *notifier,
	struct future_context *context)
{
	notifier->notifier_used = FUTURE_NOTIFIER_NONE;

	memcpy_impl(descriptor, context);
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

static void *
async_memcpy_pthread(void *arg)
{
	memcpy_impl(NULL, arg);

	return NULL;
}

static void
memcpy_pthreads(void *descriptor, struct future_notifier *notifier,
	struct future_context *context)
{
	notifier->notifier_used = FUTURE_NOTIFIER_WAKER;

	/* XXX it causes segmentation fault */
	os_thread_t thread;
	os_thread_create(&thread, NULL, async_memcpy_pthread, context);
}

static void
memcpy_pthreads_polled(void *descriptor, struct future_notifier *notifier,
	struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);

	notifier->notifier_used = FUTURE_NOTIFIER_POLLER;
	notifier->poller.ptr_to_monitor = (uint64_t *)&data->complete;

	/* XXX it causes segmentation fault */
	os_thread_t thread;
	os_thread_create(&thread, NULL, async_memcpy_pthread, context);
}

static struct vdm_descriptor pthreads_descriptor = {
	.memcpy = memcpy_pthreads,
	.vdm_data_init = NULL,
	.vdm_data_fini = NULL,
	.check = vdm_check,
};

struct vdm_descriptor *
vdm_descriptor_pthreads(void)
{
	return &pthreads_descriptor;
}

static struct vdm_descriptor pthreads_polled_descriptor = {
	.memcpy = memcpy_pthreads_polled,
	.vdm_data_init = NULL,
	.vdm_data_fini = NULL,
	.check = vdm_check,
};

struct vdm_descriptor *
vdm_descriptor_pthreads_polled(void)
{
	return &pthreads_polled_descriptor;
}
