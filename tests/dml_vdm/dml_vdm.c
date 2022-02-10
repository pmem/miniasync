// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libminiasync.h>
#include <libminiasync-dml.h>

#include "util_dml.h"

static int
dml_memcpy(unsigned flags)
{
	char *buf_a = strdup("testbuf");
	char *buf_b = strdup("otherbuf");
	size_t copy_size = strlen(buf_a);

	struct runtime *r = runtime_new();

	struct vdm *dml_mover_async = vdm_dml_new();
	struct vdm_operation_future a_to_b = vdm_memcpy(dml_mover_async, buf_b,
			buf_a, copy_size, flags);

	runtime_wait(r, FUTURE_AS_RUNNABLE(&a_to_b));

	assert(memcmp(buf_a, buf_b, copy_size) == 0);

	vdm_dml_delete(dml_mover_async);
	runtime_delete(r);
	free(buf_a);
	free(buf_b);

	return 0;
}

static int
test_dml_basic_memcpy()
{
	return dml_memcpy(0);
}

static int
test_dml_durable_flag_memcpy()
{
	return dml_memcpy(MINIASYNC_DML_F_MEM_DURABLE);
}

static int
test_dml_hw_path_flag_memcpy()
{
	return dml_memcpy(MINIASYNC_DML_F_PATH_HW);
}

int
main(int argc, char *argv[])
{
	int ret = test_dml_basic_memcpy();
	if (ret)
		return ret;

	ret = test_dml_durable_flag_memcpy();
	if (ret)
		return ret;

	if (util_dml_check_hw_available() == 0) {
		ret = test_dml_hw_path_flag_memcpy();
		if (ret)
			return ret;
	}

	return 0;
}
