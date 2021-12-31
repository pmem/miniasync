// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "libminiasync/vdm.h"
#include "core/util.h"

#define THREADS_COUNT 10
#define RINGBUF_SIZE 4
#define QUEUE_SIZE 2

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
	util_atomic_store64(&data->complete, 1);
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
	return (complete) ? FUTURE_STATE_COMPLETE :
		((started) ? FUTURE_STATE_RUNNING : FUTURE_STATE_IDLE);
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

void
vdm_pthread_init(void **vdm_data)
{
	struct vdm_pthread_data *pthread_data =
		malloc(sizeof(struct vdm_pthread_data));
	pthread_data->buf = ringbuf_new(RINGBUF_SIZE);

	pthread_cond_init(&pthread_data->added_to_ringbuf, NULL);
	pthread_cond_init(&pthread_data->removed_from_ringbuf, NULL);
	pthread_mutex_init(&pthread_data->lock, NULL);

	pthread_mutex_init(&pthread_data->queue.lock, NULL);
	pthread_data->queue.dequeue = vdm_pthread_dequeue;
	pthread_data->queue.enqueue = vdm_pthread_enqueue;
	pthread_data->queue.size = QUEUE_SIZE;
	pthread_data->queue.count = 0;
	pthread_data->queue.buf =
		malloc(sizeof(void *) * pthread_data->queue.size);
	pthread_data->queue.enqueue_index = 0;
	pthread_data->queue.dequeue_index = 0;

	pthread_create(&pthread_data->queue_thread,
		NULL, vdm_pthread_queue_loop, pthread_data);

	pthread_data->threads = malloc(sizeof(pthread_t) * THREADS_COUNT);
	for (int i = 0; i < THREADS_COUNT; i++) {
		pthread_data->threads[i] = malloc(sizeof(pthread_t));
		pthread_create(pthread_data->threads[i],
			NULL, vdm_pthread_loop, pthread_data);
	}
	*vdm_data = pthread_data;
}

void
vdm_pthread_fini(void **vdm_data)
{
	struct vdm_pthread_data *pthread_data = *vdm_data;
	for (int i = 0; i < THREADS_COUNT; i++) {
		pthread_cancel(*pthread_data->threads[i]);
		pthread_join(*pthread_data->threads[i], NULL);
		free(pthread_data->threads[i]);
	}
	free(pthread_data->threads);
	pthread_cancel(pthread_data->queue_thread);
	pthread_join(pthread_data->queue_thread, NULL);
	ringbuf_delete(pthread_data->buf);
	pthread_mutex_destroy(&pthread_data->queue.lock);
	pthread_mutex_destroy(&pthread_data->lock);
	pthread_cond_destroy(&pthread_data->removed_from_ringbuf);
	pthread_cond_destroy(&pthread_data->added_to_ringbuf);
	free(pthread_data->queue.buf);
	free(pthread_data);
	vdm_data = NULL;
}

/*
 * Enqueue is not locking the data
 */
int
vdm_pthread_enqueue(struct vdm_pthread_queue *queue,
	struct future_context *context)
{
	if (queue->count >= queue->size) {
		pthread_mutex_unlock(&queue->lock);
		return 1;
	}

	queue->buf[queue->enqueue_index] = context;
	queue->enqueue_index = (queue->enqueue_index + 1) % queue->size;
	queue->count++;
	pthread_cond_signal(&queue->added_to_queue);

	return 0;
}

/*
 * Dequeue is not locking the data
 */
struct future_context *
vdm_pthread_dequeue(struct vdm_pthread_queue *queue)
{
	struct future_context *return_value;

	if (queue->count == 0) {
		pthread_mutex_unlock(&queue->lock);
		return NULL;
	}

	return_value = queue->buf[queue->dequeue_index];
	queue->dequeue_index = (queue->dequeue_index + 1) % queue->size;
	queue->count--;

	return return_value;
}

void
pthread_cleanup_handler(void *lock)
{
	pthread_mutex_unlock(lock);
}

static void *
async_memcpy_pthread(void *arg)
{
	memcpy_impl(NULL, arg);

	return NULL;
}

void *
vdm_pthread_loop(void *arg)
{
	struct vdm_pthread_data *pthread_data = arg;
	struct ringbuf *buf = pthread_data->buf;
	struct future_context *data;

	pthread_cleanup_push(pthread_cleanup_handler, &pthread_data->lock)

	while (1) {
		/*
		 * Worker thread is trying to dequeue from ringbuffer,
		 * if he fails, he's waiting until something is added to
		 * the ringbuffer.
		 */
		pthread_mutex_lock(&pthread_data->lock);
		while ((data = ringbuf_trydequeue(buf)) ==
			NULL) {
			pthread_cond_wait(
				&pthread_data->added_to_ringbuf,
				&pthread_data->lock);
		}
		pthread_mutex_unlock(&pthread_data->lock);

		async_memcpy_pthread(data);

	}

	pthread_cleanup_pop(0);
}

void *
vdm_pthread_queue_loop(void *arg)
{
	struct vdm_pthread_data *pthread_data = arg;
	struct ringbuf *buf = pthread_data->buf;
	struct vdm_pthread_queue *queue = &pthread_data->queue;
	struct future_context *data;

	pthread_cleanup_push(pthread_cleanup_handler, &queue->lock)

	while (1) {
		/*
		 * ringbuf_enqueue will block if the buffer is full,
		 * if the queue to the buffer is empty we wait on
		 * until something is added into queue, most likely by
		 * main thread.
		 */
		pthread_mutex_lock(&queue->lock);
		while ((data = queue->dequeue(queue)) == NULL) {
			pthread_cond_wait(
				&queue->added_to_queue,
				&queue->lock);
		}
		pthread_mutex_unlock(&queue->lock);

		ringbuf_enqueue(buf, data);
		/*
		 * We should lock sending signal because it could be
		 * missed by a thread that is currently trying to
		 * dequeue from ringbuf.
		 * This error could look like that:
		 * ringbuf_trydequeue(failed)--->\added_to_ringbuf\
		 * --->pthread_cond_wait
		 */
		pthread_mutex_lock(&pthread_data->lock);
		pthread_cond_signal(
			&pthread_data->added_to_ringbuf);
		pthread_mutex_unlock(&pthread_data->lock);
	}
	pthread_cleanup_pop(0);
}

static void
memcpy_pthreads(void *descriptor, struct future_notifier *notifier,
	struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	struct vdm_pthread_data *pthread_data = vdm_get_data(data->vdm);

	notifier->notifier_used = FUTURE_NOTIFIER_WAKER;

	pthread_mutex_unlock(&pthread_data->queue.lock);
	if (pthread_data->queue.enqueue(&pthread_data->queue, context) == 0) {
		util_atomic_store64(&data->started, 1);
	}
	pthread_mutex_unlock(&pthread_data->queue.lock);
}

static void
memcpy_pthreads_polled(void *descriptor, struct future_notifier *notifier,
	struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	struct vdm_pthread_data *pthread_data = vdm_get_data(data->vdm);

	notifier->notifier_used = FUTURE_NOTIFIER_POLLER;
	notifier->poller.ptr_to_monitor = (uint64_t *)&data->complete;

	pthread_mutex_unlock(&pthread_data->queue.lock);
	if (pthread_data->queue.enqueue(&pthread_data->queue, context) == 0) {
		util_atomic_store64(&data->started, 1);
	}
	pthread_mutex_unlock(&pthread_data->queue.lock);
}

static struct vdm_descriptor pthreads_descriptor = {
	.memcpy = memcpy_pthreads,
	.vdm_data_init = vdm_pthread_init,
	.vdm_data_fini = vdm_pthread_fini,
	.check = vdm_check_async_start,
};

struct vdm_descriptor *
vdm_descriptor_pthreads(void)
{
	return &pthreads_descriptor;
}

static struct vdm_descriptor pthreads_polled_descriptor = {
	.memcpy = memcpy_pthreads_polled,
	.vdm_data_init = vdm_pthread_init,
	.vdm_data_fini = vdm_pthread_fini,
	.check = vdm_check_async_start,
};

struct vdm_descriptor *
vdm_descriptor_pthreads_polled(void)
{
	return &pthreads_polled_descriptor;
}
