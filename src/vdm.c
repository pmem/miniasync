// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include "libminiasync/vdm.h"
#include "libminiasync/vdm_threads.h"
#include "core/util.h"

struct vdm {
    struct vdm_descriptor *descriptor;
    void *data;
};

/*
 * Returns NULL if failed to allocate memory for struct vdm
 * or vdm_data_init failed
 */
struct vdm *
vdm_new(struct vdm_descriptor *descriptor)
{
	struct vdm *vdm = malloc(sizeof(struct vdm));

	if (!vdm)
		return NULL;

	vdm->descriptor = descriptor;
	vdm->data = NULL;

	if (descriptor->vdm_data_init) {
		descriptor->vdm_data_init(&vdm->data);
		if (!vdm->data) {
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
	util_atomic_store64(&data->complete, 1);
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
	util_atomic_load64(&data->complete, &complete);
	return (complete) ? FUTURE_STATE_COMPLETE : FUTURE_STATE_RUNNING;
}

static enum future_state
vdm_check_async_start(struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	int started, complete;
	util_atomic_load64(&data->started, &started);
	util_atomic_load64(&data->complete, &complete);
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
async_memcpy_threads(void *arg)
{
	memcpy_impl(NULL, arg);

	return NULL;
}

void *
vdm_threads_loop(void *arg)
{
	struct vdm_threads_data *vdm_threads_data = arg;
	struct ringbuf *buf = vdm_threads_data->buf;
	struct future_context *data;

	while (1) {
		/*
		 * Worker thread is trying to dequeue from ringbuffer,
		 * if he fails, he's waiting until something is added to
		 * the ringbuffer.
		 */
		data = ringbuf_dequeue(buf);

		if (!vdm_threads_data->running)
			return NULL;

		async_memcpy_threads(data);
	}
}

static void
memcpy_threads(void *descriptor, struct future_notifier *notifier,
	struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	struct vdm_threads_data *vdm_threads_data = vdm_get_data(data->vdm);

	notifier->notifier_used = FUTURE_NOTIFIER_WAKER;
	if (ringbuf_tryenqueue(vdm_threads_data->buf, context) == 0) {
		util_atomic_store64(&data->started, 1);
	}

}

static void
memcpy_threads_polled(void *descriptor, struct future_notifier *notifier,
	struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	struct vdm_threads_data *vdm_threads_data = vdm_get_data(data->vdm);

	notifier->notifier_used = FUTURE_NOTIFIER_POLLER;
	notifier->poller.ptr_to_monitor = (uint64_t *)&data->complete;

	if (ringbuf_tryenqueue(vdm_threads_data->buf, context) == 0) {
		util_atomic_store64(&data->started, 1);
	}
}

static struct vdm_descriptor threads_descriptor = {
	.memcpy = memcpy_threads,
	.vdm_data_init = vdm_threads_init,
	.vdm_data_fini = vdm_threads_fini,
	.check = vdm_check_async_start,
};

struct vdm_descriptor *
vdm_descriptor_threads(void)
{
	return &threads_descriptor;
}

static struct vdm_descriptor threads_polled_descriptor = {
	.memcpy = memcpy_threads_polled,
	.vdm_data_init = vdm_threads_init,
	.vdm_data_fini = vdm_threads_fini,
	.check = vdm_check_async_start,
};

struct vdm_descriptor *
vdm_descriptor_threads_polled(void)
{
	return &threads_polled_descriptor;
}
