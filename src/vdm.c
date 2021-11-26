// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2021, Intel Corporation */

#include "vdm.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

struct vdm {
	struct vdm_descriptor *descriptor;
};

struct vdm *
vdm_new(struct vdm_descriptor *descriptor)
{
	struct vdm *vdm = malloc(sizeof(struct vdm));
	vdm->descriptor = descriptor;

	return vdm;
}

void
vdm_delete(struct vdm *vdm)
{
	free(vdm);
}

static void
vdm_memcpy_cb(struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	atomic_store(&data->complete, 1);
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
			n, context);
	}
	return atomic_load(&data->complete) ? FUTURE_STATE_COMPLETE
					    : FUTURE_STATE_RUNNING;
}

struct vdm_memcpy_future
vdm_memcpy(struct vdm *vdm, void *dest, void *src, size_t n)
{
	struct vdm_memcpy_future future;
	future.data.vdm = vdm;
	future.data.dest = dest;
	future.data.src = src;
	future.data.n = n;
	future.data.complete = 0;
	FUTURE_INIT(&future, vdm_memcpy_impl);

	return future;
}

static void
memcpy_impl(void *runner, struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	struct vdm_memcpy_output *output = future_context_get_output(context);
	output->dest = memcpy(data->dest, data->src, data->n);
	data->vdm_cb(context);
}

static void
memcpy_sync(void *runner, struct future_notifier *notifier,
	struct future_context *context)
{
	notifier->notifier_used = FUTURE_NOTIFIER_NONE;

	memcpy_impl(runner, context);
}

static struct vdm_descriptor synchronous_descriptor = {
	.vdm_data = NULL,
	.memcpy = memcpy_sync,
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
memcpy_pthreads(void *runner, struct future_notifier *notifier,
	struct future_context *context)
{
	notifier->notifier_used = FUTURE_NOTIFIER_WAKER;

	pthread_t thread;
	pthread_create(&thread, NULL, async_memcpy_pthread, context);
}

static void
memcpy_pthreads_polled(void *runner, struct future_notifier *notifier,
	struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);

	notifier->notifier_used = FUTURE_NOTIFIER_POLLER;
	notifier->poller.ptr_to_monitor = (uint64_t *)&data->complete;

	pthread_t thread;
	pthread_create(&thread, NULL, async_memcpy_pthread, context);
}

static struct vdm_descriptor pthreads_descriptor = {
	.vdm_data = NULL,
	.memcpy = memcpy_pthreads,
};

struct vdm_descriptor *
vdm_descriptor_pthreads(void)
{
	return &pthreads_descriptor;
}

static struct vdm_descriptor pthreads_polled_descriptor = {
	.vdm_data = NULL,
	.memcpy = memcpy_pthreads_polled,
};

struct vdm_descriptor *
vdm_descriptor_pthreads_polled(void)
{
	return &pthreads_polled_descriptor;
}
