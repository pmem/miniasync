// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include "libminiasync/vdm.h"
#include "libminiasync/vdm_threads.h"
#include "core/util.h"
#include "core/os_thread.h"
#include "core/ringbuf.h"

#define THREADS_COUNT 12
#define RINGBUF_SIZE 128

struct vdm_threads_data {
    struct ringbuf *buf;
    os_thread_t **threads;
};

int
vdm_threads_init(void **vdm_data)
{
	struct vdm_threads_data *vdm_threads_data =
		malloc(sizeof(struct vdm_threads_data));

	if (!vdm_threads_data) {
		return 1;
	}

	vdm_threads_data->buf = ringbuf_new(RINGBUF_SIZE);

	if (!vdm_threads_data->buf) {
		free(vdm_threads_data);
		return 2;
	}

	vdm_threads_data->threads = malloc(sizeof(os_thread_t) * THREADS_COUNT);

	if (!vdm_threads_data->threads) {
		ringbuf_delete(vdm_threads_data->buf);
		free(vdm_threads_data);
		return 3;
	}

	for (int i = 0; i < THREADS_COUNT; i++) {
		vdm_threads_data->threads[i] = malloc(sizeof(os_thread_t));

		if (!vdm_threads_data->threads[i]) {
			for (int j = 0; j < i; j++) {
				free(vdm_threads_data->threads[j]);
			}
			free(vdm_threads_data->threads);
			ringbuf_delete(vdm_threads_data->buf);
			free(vdm_threads_data);
			return 4;
		}

		os_thread_create(vdm_threads_data->threads[i],
			NULL, vdm_threads_loop, vdm_threads_data);
	}
	*vdm_data = vdm_threads_data;
	return 0;
}

int
vdm_threads_fini(void **vdm_data)
{
	struct vdm_threads_data *vdm_threads_data = *vdm_data;
	ringbuf_stop(vdm_threads_data->buf);
	for (int i = 0; i < THREADS_COUNT; i++) {
		os_thread_join(vdm_threads_data->threads[i], NULL);
		free(vdm_threads_data->threads[i]);
	}
	free(vdm_threads_data->threads);
	ringbuf_delete(vdm_threads_data->buf);
	free(vdm_threads_data);
	vdm_data = NULL;
	return 0;
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

void *
vdm_threads_loop(void *arg)
{
	struct vdm_threads_data *vdm_threads_data = arg;
	struct ringbuf *buf = vdm_threads_data->buf;
	struct future_context *context;

	while (1) {
		/*
		 * Worker thread is trying to dequeue from ringbuffer,
		 * if he fails, he's waiting until something is added to
		 * the ringbuffer.
		 */
		context = ringbuf_dequeue(buf);

		if (context == NULL)
			return NULL;

		struct vdm_memcpy_data *data = future_context_get_data(context);
		data->memcpy_impl(NULL, context);
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
