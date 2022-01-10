// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include "libminiasync.h"

int
main()
{
	struct runtime *r = runtime_new();

	struct vdm_descriptor *vdm_async_descriptor = vdm_descriptor_pthreads();
	struct vdm *vdm = vdm_new(vdm_async_descriptor);

	for (int i = 0; i < 10; i++) {
		char *src = malloc(1 << 10 * sizeof(char));
		char *dst = malloc(1 << 10 * sizeof(char));
		char *src2 = malloc(1 << 10 * sizeof(char));
		char *dst2 = malloc(1 << 10 * sizeof(char));
		char *src3 = malloc(1 << 10 * sizeof(char));
		char *dst3 = malloc(1 << 10 * sizeof(char));
		char *src4 = malloc(1 << 10 * sizeof(char));
		char *dst4 = malloc(1 << 10 * sizeof(char));

		memset(src, 7, 1 << 10);
		memset(src2, 6, 1 << 10);
		memset(src3, 5, 1 << 10);
		memset(src4, 4, 1 << 10);

		struct vdm_memcpy_future fut = vdm_memcpy(
			vdm, dst, src, 1 << 10, 0);
		struct vdm_memcpy_future fut2 = vdm_memcpy(
			vdm, dst2, src2, 1 << 10, 0);
		struct vdm_memcpy_future fut3 = vdm_memcpy(
			vdm, dst3, src3, 1 << 10, 0);
		struct vdm_memcpy_future fut4 = vdm_memcpy(
			vdm, dst4, src4, 1 << 10, 0);

		struct future *futs[] = {FUTURE_AS_RUNNABLE(&fut),
			FUTURE_AS_RUNNABLE(&fut2),
			FUTURE_AS_RUNNABLE(&fut3),
			FUTURE_AS_RUNNABLE(&fut4)
		};

		runtime_wait_multiple(r, futs, 4);

		free(src);
		free(src2);
		free(src3);
		free(src4);
		free(dst);
		free(dst2);
		free(dst3);
		free(dst4);
	}

	vdm_delete(vdm);
	runtime_delete(r);
	return 0;
}