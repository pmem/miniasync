/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2022, Intel Corporation */

#ifndef VDM_THREAD_H
#define VDM_THREAD_H

#include "future.h"
#include "core/os_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

int vdm_threads_init(void **vdm_data);
int vdm_threads_fini(void **vdm_data);
struct vdm_descriptor *vdm_descriptor_threads(void);
struct vdm_descriptor *vdm_descriptor_threads_polled(void);

void *vdm_threads_loop(void *arg);
#ifdef __cplusplus
}
#endif
#endif /* VDM_THREAD_H */
