// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libminiasync.h>
#include <libminiasync-dml.h>

int
dml_basic_memcpy()
{
	char *buf_a = strdup("testbuf");
	char *buf_b = strdup("otherbuf");
	size_t copy_size = strlen(buf_a);

	struct runtime *r = runtime_new();

	struct vdm *dml_mover_async = vdm_new(vdm_descriptor_dml_async());
	struct vdm_memcpy_future a_to_b = vdm_memcpy(dml_mover_async, buf_b,
			buf_a, copy_size, 0);

	runtime_wait(r, FUTURE_AS_RUNNABLE(&a_to_b));

	assert(memcmp(buf_a, buf_b, copy_size) == 0);

	vdm_delete(dml_mover_async);
	runtime_delete(r);
	free(buf_a);
	free(buf_b);

	return 0;
}

int
main(int argc, char *argv[])
{
	int ret = dml_basic_memcpy();
	if (ret)
		return ret;

	return 0;
}
