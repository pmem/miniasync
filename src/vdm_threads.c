// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include "core/membuf.h"
#include "core/out.h"
#include "libminiasync/vdm.h"
#include "libminiasync/vdm_threads.h"
#include "core/util.h"
#include "core/os_thread.h"
#include "core/ringbuf.h"

#define VDM_THREADS_DEFAULT_NTHREADS 12
#define VDM_THREADS_DEFAULT_RINGBUF_SIZE 128

struct vdm_threads {
	struct vdm base; /* must be first */

	struct ringbuf *buf;
	size_t nthreads;
	os_thread_t *threads;
	struct membuf *membuf;
	enum future_notifier_type desired_notifier;
};

struct vdm_threads_op {
	struct vdm_operation op;
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
vdm_threads_do_operation(struct vdm_threads_op *op)
{
	switch (op->op.type) {
		case VDM_OPERATION_MEMCPY: {
			struct vdm_operation_data_memcpy *mdata
				= &op->op.memcpy;
			memcpy(mdata->dest, mdata->src, mdata->n);
		} break;
		default:
		ASSERT(0); /* unreachable */
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
static void *
vdm_threads_loop(void *arg)
{
	struct vdm_threads *vdm_threads = arg;
	struct ringbuf *buf = vdm_threads->buf;
	struct vdm_threads_op *op;

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
 * vdm_threads_operation_check -- check the status of a thread operation
 */
static enum future_state
vdm_threads_operation_check(void *op)
{
	struct vdm_threads_op *opt = op;

	uint64_t complete;
	util_atomic_load_explicit64(&opt->complete,
		&complete, memory_order_acquire);
	if (complete)
		return FUTURE_STATE_COMPLETE;

	uint64_t started;
	util_atomic_load_explicit64(&opt->started,
		&started, memory_order_acquire);
	if (started)
		return FUTURE_STATE_RUNNING;

	return FUTURE_STATE_IDLE;
}

/*
 * vdm_threads_membuf_check -- checks the status of a threads job
 */
static enum membuf_check_result
vdm_threads_membuf_check(void *ptr, void *data)
{
	switch (vdm_threads_operation_check(ptr)) {
		case FUTURE_STATE_COMPLETE:
			return MEMBUF_PTR_CAN_REUSE;
		case FUTURE_STATE_RUNNING:
			return MEMBUF_PTR_CAN_WAIT;
		case FUTURE_STATE_IDLE:
			return MEMBUF_PTR_IN_USE;
	}

	ASSERT(0);
	return MEMBUF_PTR_IN_USE;
}

/*
 * vdm_threads_membuf_size -- returns the size of a threads job size
 */
static size_t
vdm_threads_membuf_size(void *ptr, void *data)
{
	return sizeof(struct vdm_threads);
}

/*
 * vdm_threads_operation_new -- create a new thread operation that uses
 * wakers
 */
static void *
vdm_threads_operation_new(struct vdm *vdm,
	const struct vdm_operation *operation)
{
	struct vdm_threads *vdm_threads = (struct vdm_threads *)vdm;

	struct vdm_threads_op *op =
		membuf_alloc(vdm_threads->membuf,
		sizeof(struct vdm_threads_op));

	op->complete = 0;
	op->started = 0;
	op->desired_notifier = vdm_threads->desired_notifier;
	op->op = *operation;

	return op;
}

/*
 * vdm_threads_operation_delete -- delete a thread operation
 */
static void
vdm_threads_operation_delete(void *op, struct vdm_operation_output *output)
{
	struct vdm_threads_op *opt = (struct vdm_threads_op *)op;

	switch (opt->op.type) {
		case VDM_OPERATION_MEMCPY:
			output->type = VDM_OPERATION_MEMCPY;
			output->memcpy.dest = opt->op.memcpy.dest;
			break;
		default:
			ASSERT(0);
	}
}

/*
 * vdm_threads_operation_start -- start a memory operation using threads
 */
static int
vdm_threads_operation_start(void *op, struct future_notifier *n)
{
	struct vdm_threads_op *opt = (struct vdm_threads_op *)op;

	if (n) {
		n->notifier_used = opt->desired_notifier;
		opt->notifier = *n;
		if (opt->desired_notifier == FUTURE_NOTIFIER_POLLER) {
			n->poller.ptr_to_monitor = &opt->complete;
		}
	} else {
		opt->desired_notifier = FUTURE_NOTIFIER_NONE;
	}

	struct vdm_threads *vdm_threads = membuf_ptr_user_data(op);

	if (ringbuf_tryenqueue(vdm_threads->buf, op) == 0) {
		util_atomic_store_explicit64(&opt->started,
			FUTURE_STATE_RUNNING, memory_order_release);
	}

	return 0;
}

static struct vdm vdm_threads_base = {
	.op_new = vdm_threads_operation_new,
	.op_delete = vdm_threads_operation_delete,
	.op_check = vdm_threads_operation_check,
	.op_start = vdm_threads_operation_start,
};

/*
 * vdm_threads_init -- initialize data needed for threads mover to work,
 * returns 1 on failure.
 */
struct vdm *
vdm_threads_new(size_t nthreads, size_t ringbuf_size,
	enum future_notifier_type desired_notifier)
{
	struct vdm_threads *vdm_threads =
		malloc(sizeof(struct vdm_threads));
	if (vdm_threads == NULL)
		goto data_failed;

	vdm_threads->desired_notifier = desired_notifier;
	vdm_threads->base = vdm_threads_base;

	vdm_threads->buf = ringbuf_new(ringbuf_size == 0 ?
		VDM_THREADS_DEFAULT_RINGBUF_SIZE : ringbuf_size);
	if (vdm_threads->buf == NULL)
		goto ringbuf_failed;

	vdm_threads->membuf = membuf_new(vdm_threads_membuf_check,
		vdm_threads_membuf_size, NULL, vdm_threads);
	if (vdm_threads->membuf == NULL)
		goto membuf_failed;

	vdm_threads->nthreads = nthreads <= 0 ?
		VDM_THREADS_DEFAULT_NTHREADS : nthreads;
	vdm_threads->threads = malloc(sizeof(os_thread_t) *
		vdm_threads->nthreads);
	if (vdm_threads->threads == NULL)
		goto threads_array_failed;

	size_t i;
	for (i = 0; i < vdm_threads->nthreads; i++) {
		os_thread_create(&vdm_threads->threads[i],
			NULL, vdm_threads_loop, vdm_threads);
	}

	return &vdm_threads->base;

threads_array_failed:
	membuf_delete(vdm_threads->membuf);

membuf_failed:
	ringbuf_delete(vdm_threads->buf);

ringbuf_failed:
	free(vdm_threads);

data_failed:
	return NULL;
}

/*
 * vdm_threads_fini -- perform necessary cleanup after threads mover.
 * Releases all memory and closes all created threads.
 */
void
vdm_threads_delete(struct vdm *vdm)
{
	struct vdm_threads *vdm_threads = (struct vdm_threads *)vdm;
	ringbuf_stop(vdm_threads->buf);
	for (size_t i = 0; i < vdm_threads->nthreads; i++) {
		os_thread_join(&vdm_threads->threads[i], NULL);
	}
	free(vdm_threads->threads);
	membuf_delete(vdm_threads->membuf);
	ringbuf_delete(vdm_threads->buf);
	free(vdm_threads);
}
