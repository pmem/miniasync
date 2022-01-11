// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include "libminiasync/vdm.h"
#include "libminiasync/vdm_threads.h"
#include "core/os_thread.h"
#include "core/ringbuf.h"

#define THREADS_COUNT 12
#define RINGBUF_SIZE 4

void
vdm_threads_init(void **vdm_data)
{
	struct vdm_threads_data *vdm_threads_data =
		malloc(sizeof(struct vdm_threads_data));

	if (!vdm_threads_data) {
		return;
	}

	vdm_threads_data->buf = ringbuf_new(RINGBUF_SIZE);

	if (!vdm_threads_data->buf) {
		free(vdm_threads_data);
		return;
	}

	vdm_threads_data->running = 1;
	vdm_threads_data->threads = malloc(sizeof(os_thread_t) * THREADS_COUNT);

	if (!vdm_threads_data->threads) {
		ringbuf_delete(vdm_threads_data->buf);
		free(vdm_threads_data);
		return;
	}

	for (int i = 0; i < THREADS_COUNT; i++) {
		vdm_threads_data->threads[i] = malloc(sizeof(os_thread_t));

		if (!vdm_threads_data->threads[i]) {
			for (int j = 0; j < i; j++) {
				free(vdm_threads_data->threads[j]);
			}
			free(vdm_threads_data->threads);
			ringbuf_delete(vdm_threads_data->buf);
			free(vdm_threads_data);
			return;
		}

		os_thread_create(vdm_threads_data->threads[i],
			NULL, vdm_threads_loop, vdm_threads_data);
	}
	*vdm_data = vdm_threads_data;
}

void
vdm_threads_fini(void **vdm_data)
{
	struct vdm_threads_data *vdm_threads_data = *vdm_data;
	vdm_threads_data->running = 0;
	ringbuf_stop(vdm_threads_data->buf);
	for (int i = 0; i < THREADS_COUNT; i++) {
		os_thread_join(vdm_threads_data->threads[i], NULL);
		free(vdm_threads_data->threads[i]);
	}
	free(vdm_threads_data->threads);
	ringbuf_delete(vdm_threads_data->buf);
	free(vdm_threads_data);
	vdm_data = NULL;
}
