// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "libminiasync/vdm.h"
#include "libminiasync/vdm_threads.h"
#include "core/util.h"
#include "core/os_thread.h"
#include "core/ringbuf.h"

struct vdm_threads_data {
	struct ringbuf *buf;
	os_thread_t *threads;
};

struct threads_operation_entry {
	struct vdm_threads_data *threads;

	struct vdm_operation operation;
	enum future_notifier_type desired_notifier;
	struct future_notifier notifier;
	uint64_t complete;
	uint64_t started;
};

/*
 * vdm_threads_do_operation -- implementation of the various implementations
 *	supported by this mover implementation
 */
static void
vdm_threads_do_operation(struct threads_operation_entry *op)
{
	switch (op->operation.type) {
		case VDM_OPERATION_MEMCPY: {
			struct vdm_memcpy_data *mdata
				= &op->operation.memcpy;
			memcpy(mdata->dest, mdata->src, mdata->n);
		} break;
		default:
		assert(0); /* unreachable */
		break;
	}

	if (op->desired_notifier == FUTURE_NOTIFIER_WAKER) {
		FUTURE_WAKER_WAKE(&op->notifier.waker);
	}
	util_atomic_store_explicit64(&op->complete, 1, memory_order_release);
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
	struct threads_operation_entry *op;

	while (1) {
		/*
		 * Worker thread is trying to dequeue from ringbuffer,
		 * if he fails, he's waiting until something is added to
		 * the ringbuffer.
		 */
		if ((op = ringbuf_dequeue(buf)) == NULL)
			return NULL;

		vdm_threads_do_operation(op);
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

/*
 * vdm_threads_operation_entry_new -- allocate and initialize a new thread
 * operation structure
 */
static struct threads_operation_entry *
vdm_threads_operation_entry_new(void *vdm_data,
	const struct vdm_operation *operation,
	enum future_notifier_type desired_notifier)
{
	struct threads_operation_entry *op = malloc(sizeof(*op));
	op->complete = 0;
	op->started = 0;
	op->threads = vdm_data;
	op->desired_notifier = desired_notifier;
	op->operation = *operation;

	return op;
}

/*
 * vdm_threads_operation_entry_delete -- delete a thread operation
 */
static void
vdm_threads_operation_entry_delete(struct threads_operation_entry *entry)
{
	free(entry);
}

/*
 * vdm_threads_operation_new -- create a new thread operation that uses
 * wakers
 */
static int64_t
vdm_threads_operation_new(void *vdm_data,
	const struct vdm_operation *operation)
{
	return (int64_t)vdm_threads_operation_entry_new(vdm_data, operation,
		FUTURE_NOTIFIER_WAKER);
}

/*
 * vdm_threads_polled_operation_new -- create a new thread operation that uses
 * polling
 */
static int64_t
vdm_threads_polled_operation_new(void *vdm_data,
	const struct vdm_operation *operation)
{
	return (int64_t)vdm_threads_operation_entry_new(vdm_data, operation,
		FUTURE_NOTIFIER_POLLER);
}

/*
 * vdm_threads_operation_delete -- delete a thread operation
 */
static void
vdm_threads_operation_delete(void *vdm_data, int64_t op_id)
{
	struct threads_operation_entry *op =
		(struct threads_operation_entry *)op_id;
	vdm_threads_operation_entry_delete(op);
}

/*
 * vdm_threads_operation_check -- check the status of a thread operation
 */
static enum future_state
vdm_threads_operation_check(void *vdm_data, int64_t op_id)
{
	struct threads_operation_entry *op =
		(struct threads_operation_entry *)op_id;

	uint64_t complete;
	util_atomic_load_explicit64(&op->complete,
		&complete, memory_order_acquire);
	if (complete)
		return FUTURE_STATE_COMPLETE;

	uint64_t started;
	util_atomic_load_explicit64(&op->started,
		&started, memory_order_acquire);
	if (started)
		return FUTURE_STATE_RUNNING;

	return FUTURE_STATE_IDLE;
}

/*
 * vdm_threads_operation_start -- start a memory operation using threads
 */
static int
vdm_threads_operation_start(void *vdm_data,
	int64_t op_id, struct future_notifier *n)
{
	struct threads_operation_entry *op =
		(struct threads_operation_entry *)op_id;

	if (n) {
		n->notifier_used = op->desired_notifier;
		op->notifier = *n;
		if (op->desired_notifier == FUTURE_NOTIFIER_POLLER) {
			n->poller.ptr_to_monitor = &op->complete;
		}
	} else {
		op->desired_notifier = FUTURE_NOTIFIER_NONE;
	}

	struct vdm_threads_data *vdm_threads_data = vdm_data;

	if (ringbuf_tryenqueue(vdm_threads_data->buf, op) == 0) {
		util_atomic_store_explicit64(&op->started,
			FUTURE_STATE_RUNNING, memory_order_release);
	}

	return 0;
}

static struct vdm_descriptor threads_descriptor = {
	.vdm_data_init = vdm_threads_init,
	.vdm_data_fini = vdm_threads_fini,
	.op_new = vdm_threads_operation_new,
	.op_delete = vdm_threads_operation_delete,
	.op_check = vdm_threads_operation_check,
	.op_start = vdm_threads_operation_start,
};

/*
 * vdm_descriptor_threads -- returns an asynchronous memory mover
 * that uses worker threads to complete operations.
 * Uses a waker notifier.
 */
struct vdm_descriptor *
vdm_descriptor_threads(void)
{
	return &threads_descriptor;
}

static struct vdm_descriptor threads_polled_descriptor = {
	.vdm_data_init = vdm_threads_init,
	.vdm_data_fini = vdm_threads_fini,
	.op_new = vdm_threads_polled_operation_new,
	.op_delete = vdm_threads_operation_delete,
	.op_check = vdm_threads_operation_check,
	.op_start = vdm_threads_operation_start,
};

/*
 * vdm_descriptor_threads_polled -- returns an asynchronous memory mover
 * that uses worker threads to complete operations.
 * Uses polling notifier.
 */
struct vdm_descriptor *
vdm_descriptor_threads_polled(void)
{
	return &threads_polled_descriptor;
}
