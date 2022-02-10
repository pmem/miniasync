/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2022, Intel Corporation */

#ifndef VDM_THREADS_H
#define VDM_THREADS_H

#include "future.h"

#ifdef __cplusplus
extern "C" {
#endif

struct vdm *vdm_threads_new(size_t nthreads, size_t ringbuf_size,
	enum future_notifier_type desired_notifier);
void vdm_threads_delete(struct vdm *vdm);

#ifdef __cplusplus
}
#endif
#endif /* VDM_THREADS_H */
