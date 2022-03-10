// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "libminiasync/future.h"
#include "libminiasync/vdm.h"
#include "test_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libminiasync.h>

struct alloc_data {
	size_t n;
};

struct alloc_output {
	void *ptr;
};

FUTURE(alloc_fut, struct alloc_data, struct alloc_output);

enum future_state
alloc_impl(struct future_context *context, struct future_notifier *notifier)
{
	struct alloc_data *data = future_context_get_data(context);
	struct alloc_output *output = future_context_get_output(context);

	output->ptr = malloc(data->n);
	UT_ASSERTne(output->ptr, NULL);

	return FUTURE_STATE_COMPLETE;
}

struct alloc_fut
async_alloc(size_t size)
{
	struct alloc_fut fut = {0};
	fut.data.n = size;
	FUTURE_INIT(&fut, alloc_impl);
	return fut;
};

struct strdup_data {
	FUTURE_CHAIN_ENTRY(struct alloc_fut, alloc);
	FUTURE_CHAIN_ENTRY(struct vdm_operation_future, copy);
};

struct strdup_output {
	void *ptr;
	size_t length;
};

FUTURE(strdup_fut, struct strdup_data, struct strdup_output);

void
strdup_map_alloc_to_copy(struct future_context *lhs,
	struct future_context *rhs, void *arg)
{
	struct alloc_output *alloc = future_context_get_output(lhs);
	struct vdm_operation_data *copy = future_context_get_data(rhs);
	copy->operation.data.memcpy.dest = alloc->ptr;
}

void
strdup_map_copy_to_output(struct future_context *lhs,
	struct future_context *rhs, void *arg)
{
	struct vdm_operation_data *copy = future_context_get_data(lhs);
	struct strdup_output *strdup = future_context_get_output(rhs);
	strdup->ptr = copy->operation.data.memcpy.dest;
	strdup->length = copy->operation.data.memcpy.n;
}

struct strdup_fut
async_strdup(struct vdm *vdm, char *s)
{
	struct strdup_fut fut = {0};

	size_t len = strlen(s) + 1;
	FUTURE_CHAIN_ENTRY_INIT(&fut.data.alloc, async_alloc(len),
		strdup_map_alloc_to_copy, NULL);
	FUTURE_CHAIN_ENTRY_INIT(&fut.data.copy,
		vdm_memcpy(vdm, NULL, s, len, 0),
		strdup_map_copy_to_output, NULL);
	FUTURE_CHAIN_INIT(&fut);

	return fut;
}

int
main(void)
{
	struct data_mover_sync *sync = data_mover_sync_new();
	struct vdm *vdm = data_mover_sync_get_vdm(sync);

	static char *hello_world = "Hello World!";

	struct strdup_fut fut = async_strdup(vdm, hello_world);
	FUTURE_BUSY_POLL(&fut);

	struct strdup_output *output = FUTURE_OUTPUT(&fut);
	UT_ASSERTeq(strcmp(hello_world, output->ptr), 0);
	UT_ASSERTeq(strlen(hello_world) + 1, output->length);

	free(output->ptr);

	data_mover_sync_delete(sync);

	return 0;
}
