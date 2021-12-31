/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2022, Intel Corporation */

#ifndef VDM_THREAD_H
#define VDM_THREAD_H

#include "future.h"
#include "core/os_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

#define THREADS_COUNT 1
#define RINGBUF_SIZE 128
	
struct vdm_descriptor *vdm_descriptor_threads(void);
struct vdm_descriptor *vdm_descriptor_threads_polled(void);

#ifdef __cplusplus
}
#endif
#endif /* VDM_THREAD_H */
