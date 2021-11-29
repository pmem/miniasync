/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021, Intel Corporation */

/*
 * mover.h - public definitions for an abstract data mover type.
 *
 * A mover is an abstract type that software can use to generically perform
 * asynchronous memory operations. Libraries can use this to avoid a hard
 * dependency on any specific implementation of hardware offload for
 * memory operations.
 *
 * Data movers implementations can use DMA engines like
 * Intel DSA (Data Streaming Accelerator), plain threads,
 * or synchronous operations in the current working thread.
 *
 * Data movers need to implement the runner interface, and applications can use
 * such implementations to create a concrete mover. Software can then use movers
 * to create more complex generic concurrent futures that use asynchronous
 * memory operations.
 */

#ifndef MOVER_H
#define MOVER_H 1

#include "future.h"

struct mover;

typedef void (*mover_cb_fn)(struct future_context *context);

struct mover_memcpy_data {
	struct future_waker waker;
	_Atomic int complete;
	struct mover *mover;
	void *dest;
	void *src;
	size_t n;
	mover_cb_fn mover_cb;
};

struct mover_memcpy_output {
	void *dest;
};

FUTURE(mover_memcpy_future,
	struct mover_memcpy_data, struct mover_memcpy_output);

struct mover_memcpy_future mover_memcpy(struct mover *mover,
	void *dest, void *src, size_t n);

typedef void (*async_memcpy_fn)(void *runner, struct future_context *context);

struct mover_runner {
	void *runner_data;
	async_memcpy_fn memcpy;
};

struct mover_runner *mover_runner_synchronous(void);
struct mover_runner *mover_runner_pthreads(void);

struct mover *mover_new(struct mover_runner *runner);

void mover_delete(struct mover *mover);

#endif
