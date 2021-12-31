// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include "libminiasync.h"

int
main()
{
	struct runtime *r = runtime_new();

	struct vdm_descriptor *vdm_async_descriptor = vdm_descriptor_threads();
	struct vdm *vdm = vdm_new(vdm_async_descriptor);

	size_t test_size = 1 << 10;

	for (int i = 0; i < 3; i++) {
		char *src1 = malloc(test_size * sizeof(char));
		char *dst1 = malloc(test_size * sizeof(char));
		char *src2 = malloc(test_size * 2 * sizeof(char));
		char *dst2 = malloc(test_size * 2 * sizeof(char));

		memset(src1, 7, test_size);
		memset(src2, 6, test_size * 2);

		struct vdm_memcpy_future fut = vdm_memcpy(
			vdm, dst1, src1, test_size, 0);
		struct vdm_memcpy_future fut2 = vdm_memcpy(
			vdm, dst2, src2, test_size * 2, 0);

		struct future *futs[] = {FUTURE_AS_RUNNABLE(&fut),
			FUTURE_AS_RUNNABLE(&fut2)
		};

		runtime_wait_multiple(r, futs, 2);

		free(src1);
		free(src2);
		free(dst1);
		free(dst2);
	}

	vdm_delete(vdm);
	runtime_delete(r);
	return 0;
}
