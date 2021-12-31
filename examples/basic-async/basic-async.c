//
// Created by blazej-smorawski on 21/12/2021.
//

#include <stdlib.h>
#include <string.h>
#include "libminiasync.h"

int
main()
{
	struct runtime *r = runtime_new();

	struct vdm_descriptor *vdm_async_descriptor = vdm_descriptor_pthreads();
	struct vdm *vdm = vdm_new(vdm_async_descriptor);

	for (int i = 0; i < 2; i++) {
		printf("Starting a new memcpy\n");
		int *src = malloc(1 << 10);
		int *dst = malloc(1 << 10);
		int *src2 = malloc(1 << 10);
		int *dst2 = malloc(1 << 10);
		int *src3 = malloc(1 << 10);
		int *dst3 = malloc(1 << 10);
		int *src4 = malloc(1 << 10);
		int *dst4 = malloc(1 << 10);

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

		runtime_wait_multiple(r, futs, (unsigned)rand() % 4 + 1);

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
