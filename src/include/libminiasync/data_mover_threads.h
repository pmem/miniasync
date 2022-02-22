/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2022, Intel Corporation */

#ifndef DATA_MOVER_THREADS_H
#define DATA_MOVER_THREADS_H

#include "vdm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*memcpy_fn)(void *dst, const void *src,
				size_t n, unsigned flags);

/*
 * Do not change order nor delete variables in this struct,
 * because it may cause older software compiled with
 * an older version of libminiasync to stop working with
 * a newer version of libminiasync used in runtime.
 */
struct data_mover_threads_op_fns {
	memcpy_fn op_memcpy;
};

struct data_mover_threads;
struct data_mover_threads *data_mover_threads_new(size_t nthreads,
	size_t ringbuf_size, struct data_mover_threads_op_fns *ops,
	size_t ops_size, enum future_notifier_type desired_notifier);
struct data_mover_threads *data_mover_threads_default();
struct vdm *data_mover_threads_get_vdm(struct data_mover_threads *dmt);
void data_mover_threads_delete(struct data_mover_threads *dmt);

#ifdef __cplusplus
}
#endif
#endif /* DATA_MOVER_THREADS_H */
