/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2022, Intel Corporation */

#ifndef VDM_THREAD_H
#define VDM_THREAD_H

#include "future.h"
#include "core/os_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

struct vdm_threads_data {
    struct ringbuf *buf;
    os_thread_t **threads;
    int running;
};

void vdm_threads_init(void **vdm_data);
void vdm_threads_fini(void **vdm_data);

#ifdef __cplusplus
}
#endif
#endif /* VDM_THREAD_H */
