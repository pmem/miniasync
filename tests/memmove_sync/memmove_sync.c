// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include "libminiasync.h"
#include "test_helpers.h"

int
main(void)
{
	int ret = 0;

	size_t buf_a_size = strlen("teststring1");
	size_t buf_b_size = strlen("teststring2");

	char *buf_a = malloc(buf_a_size + 1);
	char *buf_b = malloc(buf_b_size + 1);

	memcpy(buf_a, "teststring1", buf_a_size + 1);
	memcpy(buf_b, "teststring2", buf_b_size + 1);

	UT_ASSERTne(strcmp(buf_a, buf_b), 0);

	struct data_mover_sync *dms = data_mover_sync_new();
	struct vdm *sync_mover = data_mover_sync_get_vdm(dms);

	struct vdm_operation_future test_memmove_fut =
		vdm_memmove(sync_mover, buf_a, buf_b, buf_a_size, 0);

	FUTURE_BUSY_POLL(&test_memmove_fut);

	ret = strcmp(buf_a, buf_b);

	data_mover_sync_delete(dms);
	free(buf_a);
	free(buf_b);

	return ret;
}
