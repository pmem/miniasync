// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libminiasync.h>
#include <libminiasync-dml.h>

int main(int argc, char *argv[]) {
	char *buf_a = strdup("testbuf");
	char *buf_b = strdup("otherbuf");
	size_t copy_size = strlen(buf_a);

	struct runtime *r = runtime_new();

	struct vdm *dml_mover = vdm_new(vdm_descriptor_dml());
	struct vdm_memcpy_future a_to_b = vdm_memcpy(dml_mover, buf_b, buf_a,
			copy_size, 0);

	runtime_wait(r, FUTURE_AS_RUNNABLE(&a_to_b));

	assert(memcmp(buf_a, buf_b, copy_size) == 0);

	char *buf_c = strdup("testbufasync");
	char *buf_d = strdup("differentbuf");
	size_t copy_size_async = strlen(buf_c);

	struct vdm *dml_mover_async = vdm_new(vdm_descriptor_dml_async());
	struct vdm_memcpy_future c_to_d = vdm_memcpy(dml_mover_async, buf_c,
			buf_d, copy_size_async, 0);

	runtime_wait(r, FUTURE_AS_RUNNABLE(&c_to_d));

	assert(memcmp(buf_c, buf_d, copy_size_async) == 0);

	return 0;
}
