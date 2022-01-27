// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include "libminiasync/vdm.h"
#include "libminiasync/vdm_threads.h"
#include "core/util.h"
#include "core/os_thread.h"
#include "core/ringbuf.h"

struct vdm_threads_data {
    struct ringbuf *buf;
    os_thread_t *threads;
};

/*
 * memcpy_threads -- function that is called on future_poll of futures
 * created with vdm associated with vdm_descriptor_threads.
 */
static void
memcpy_threads(void *descriptor, struct future_notifier *notifier,
	struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	struct vdm_threads_data *vdm_threads_data = vdm_get_data(data->vdm);

	notifier->notifier_used = FUTURE_NOTIFIER_WAKER;
	if (ringbuf_tryenqueue(vdm_threads_data->buf, context) == 0) {
		util_atomic_store32(&data->started, 1);
	}
}

/*
 * memcpy_threads_polled -- function that is called on future_poll of
 * futures created with vdm associated with vdm_descriptor_threads_polled.
 */
static void
memcpy_threads_polled(void *descriptor, struct future_notifier *notifier,
	struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	struct vdm_threads_data *vdm_threads_data = vdm_get_data(data->vdm);

	notifier->notifier_used = FUTURE_NOTIFIER_POLLER;
	notifier->poller.ptr_to_monitor = (uint64_t *)&data->complete;

	if (ringbuf_tryenqueue(vdm_threads_data->buf, context) == 0) {
		/*
		 * Memcpy future is seen as started only after adding
		 * its context into ringbuf.
		 */
		util_atomic_store32(&data->started, 1);
	}
}

/*
 * memcpy_impl -- implementation of memcpy for vdm memcpy futures.
 */
static void
memcpy_impl(void *descriptor, struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	struct vdm_memcpy_output *output = future_context_get_output(context);

	output->dest = memcpy(data->dest, data->src, data->n);
	data->vdm_cb(context);
}

/*
 * vdm_threads_fini -- loop that is executed by every worker
 * thread of the mover
 */
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

		memcpy_impl(NULL, context);
	}
}

/*
 * vdm_threads_init -- initialize data needed for threads mover to work,
 * returns 1 on failure.
 */
int
vdm_threads_init(void **vdm_data)
{
	struct vdm_threads_data *vdm_threads_data =
		malloc(sizeof(struct vdm_threads_data));
	if (vdm_threads_data == NULL)
		goto data_failed;

	vdm_threads_data->buf = ringbuf_new(RINGBUF_SIZE);
	if (vdm_threads_data->buf == NULL)
		goto ringbuf_failed;

	vdm_threads_data->threads = malloc(sizeof(os_thread_t) * THREADS_COUNT);
	if (vdm_threads_data->threads == NULL)
		goto threads_array_failed;

	int i;
	for (i = 0; i < THREADS_COUNT; i++) {
		os_thread_create(&vdm_threads_data->threads[i],
			NULL, vdm_threads_loop, vdm_threads_data);
	}
	*vdm_data = vdm_threads_data;
	return 0;

threads_array_failed:
	ringbuf_delete(vdm_threads_data->buf);

ringbuf_failed:
	free(vdm_threads_data);

data_failed:
	return 1;
}

/*
 * vdm_threads_fini -- perform necessary cleanup after threads mover.
 * Releases all memory and closes all created threads.
 */
int
vdm_threads_fini(void **vdm_data)
{
	struct vdm_threads_data *vdm_threads_data = *vdm_data;
	ringbuf_stop(vdm_threads_data->buf);
	for (int i = 0; i < THREADS_COUNT; i++) {
		os_thread_join(&vdm_threads_data->threads[i], NULL);
	}
	free(vdm_threads_data->threads);
	ringbuf_delete(vdm_threads_data->buf);
	free(vdm_threads_data);
	vdm_data = NULL;
	return 0;
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
