// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * basic.c -- example of creating and running various futures
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libminiasync.h"
#include "libminiasync/data_mover_threads.h"

/* Definitions of futures, their data and output structs */

/*
 * BEGIN of async_print future
 */
struct async_print_data {
	void *value;
};

struct async_print_output {
	int error_code; /* XXX: change from long to int leads to an error */
};

FUTURE(async_print_fut, struct async_print_data, struct async_print_output);

static enum future_state
async_print_impl(struct future_context *ctx, struct future_notifier *notifier)
{
	if (notifier) notifier->notifier_used = FUTURE_NOTIFIER_NONE;

	struct async_print_data *data = future_context_get_data(ctx);
	struct async_print_output *output = future_context_get_output(ctx);

	int ret = printf("async print: %p\n", data->value);
	output->error_code = ret;

	return FUTURE_STATE_COMPLETE;
}

/* It defines how to create 'async_print_fut' future */
static struct async_print_fut
async_print(void *value)
{
	struct async_print_fut future = {0};
	future.data.value = value;

	FUTURE_INIT(&future, async_print_impl);

	return future;
}
/*
 * END of async_print future
 */

/*
 * BEGIN of async_memcpy_print future
 */
struct async_memcpy_print_data {
	FUTURE_CHAIN_ENTRY(struct vdm_operation_future, memcpy);
	FUTURE_CHAIN_ENTRY(struct async_print_fut, print);
};

struct async_memcpy_print_output {
	int error_code;
};

FUTURE(async_memcpy_print_fut, struct async_memcpy_print_data,
		struct async_memcpy_print_output);

static void
memcpy_to_print_map(struct future_context *memcpy_ctx,
		    struct future_context *print_ctx, void *arg)
{
	struct vdm_operation_output *output =
		future_context_get_output(memcpy_ctx);
	struct async_print_data *print = future_context_get_data(print_ctx);

	assert(output->type == VDM_OPERATION_MEMCPY);
	print->value = output->output.memcpy.dest;
	assert(arg == (void *)0xd);
}

static void
print_to_output_map(struct future_context *print_ctx,
		    struct future_context *chained_ctx, void *arg)
{
	struct async_print_output *print = future_context_get_output(print_ctx);
	struct async_memcpy_print_output *chained = future_context_get_output(chained_ctx);

	chained->error_code = print->error_code;
}

/* It defines how to create 'async_memcpy_print_fut' future */
static struct async_memcpy_print_fut
async_memcpy_print(struct vdm *vdm, void *dest, void *src, size_t n)
{
	struct async_memcpy_print_fut chain = {0};
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.memcpy,
				vdm_memcpy(vdm, dest, src, n, 0),
				memcpy_to_print_map, (void *)0xd);
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.print, async_print(NULL),
				print_to_output_map, NULL);

	FUTURE_CHAIN_INIT(&chain);

	return chain;
}
/*
 * END of async_memcpy_print future
 */

/* Main - creates instances and executes the futures */
int
main(void)
{
	/* Set up the data, create runtime and desired mover */
	size_t testbuf_size = strlen("testbuf");
	size_t otherbuf_size = strlen("otherbuf");

	char *buf_a = malloc(testbuf_size + 1);
	if (buf_a == NULL) {
		fprintf(stderr, "Failed to create buf_a\n");
		return 1;
	}
	char *buf_b = malloc(otherbuf_size + 1);
	if (buf_b == NULL) {
		fprintf(stderr, "Failed to create buf_b\n");
		free(buf_a);
		return 1;
	}

	memcpy(buf_a, "testbuf", testbuf_size + 1);
	memcpy(buf_b, "otherbuf", otherbuf_size + 1);

	struct runtime *r = runtime_new();

	struct data_mover_threads *dmt = data_mover_threads_default();
	if (dmt == NULL) {
		fprintf(stderr, "Failed to allocate data mover.\n");
		free(buf_a);
		free(buf_b);
		runtime_delete(r);
		return 1;
	}
	struct vdm *thread_mover = data_mover_threads_get_vdm(dmt);

	struct async_memcpy_print_fut memcpy_print_busy =
		async_memcpy_print(thread_mover, buf_b, buf_a, testbuf_size);
	FUTURE_BUSY_POLL(&memcpy_print_busy);
	struct async_memcpy_print_output *out = FUTURE_OUTPUT(&memcpy_print_busy);
	printf("async memcpy print return value: %d\n", out->error_code);

	/* At the end we require cleanup and we just print the buffers */
	data_mover_threads_delete(dmt);

	printf("%s %s %d\n", buf_a, buf_b, memcmp(buf_a, buf_b, testbuf_size));

	free(buf_a);
	free(buf_b);

	return 0;
}
