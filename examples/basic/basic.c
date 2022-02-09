// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * main.c -- main of async implementation example
 */

#include <assert.h>
#include <emmintrin.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libminiasync.h"

#ifdef _WIN32
#define strdup(x) _strdup(x)
#endif

struct async_print_data {
	void *value;
};

struct async_print_output {
	/* XXX dummy field to avoid empty struct */
	uintptr_t foo;
};

FUTURE(async_print_fut, struct async_print_data, struct async_print_output);

static enum future_state
async_print_impl(struct future_context *ctx, struct future_notifier *notifier)
{
	if (notifier != NULL) {
		notifier->notifier_used = FUTURE_NOTIFIER_NONE;
	}

	struct async_print_data *data = future_context_get_data(ctx);
	printf("async print: %p\n", data->value);

	return FUTURE_STATE_COMPLETE;
}

static struct async_print_fut
async_print(void *value)
{
	struct async_print_fut future = {0};
	future.data.value = value;

	FUTURE_INIT(&future, async_print_impl);

	return future;
}

struct async_memcpy_print_data {
	FUTURE_CHAIN_ENTRY(struct vdm_memcpy_future, memcpy);
	FUTURE_CHAIN_ENTRY(struct async_print_fut, print);
};

struct async_memcpy_print_output {
	/* XXX dummy field to avoid empty struct */
	uintptr_t foo;
};

FUTURE(async_memcpy_print_fut, struct async_memcpy_print_data,
		struct async_memcpy_print_output);

static void
memcpy_to_print_map(struct future_context *memcpy_ctx,
		    struct future_context *print_ctx, void *arg)
{
	struct vdm_memcpy_output *output =
		future_context_get_output(memcpy_ctx);
	struct async_print_data *print = future_context_get_data(print_ctx);

	print->value = output->dest;
	assert(arg == (void *)0xd);
}

static struct async_memcpy_print_fut
async_memcpy_print(struct vdm *vdm, void *dest, void *src, size_t n)
{
	struct async_memcpy_print_fut chain = {0};
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.memcpy,
				vdm_memcpy(vdm, dest, src, n, 0),
				memcpy_to_print_map, (void *)0xd);
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.print, async_print(NULL), NULL,
				NULL);

	FUTURE_CHAIN_INIT(&chain);

	return chain;
}

int
main(void)
{
	size_t testbuf_size = strlen("testbuf");
	char *buf_a = strdup("testbuf");

	char *buf_b = strdup("otherbuf");
	struct runtime *r = runtime_new();

	struct vdm *thread_mover = vdm_new(vdm_descriptor_threads_polled());
	struct vdm_memcpy_future a_to_b =
		vdm_memcpy(thread_mover, buf_b, buf_a, testbuf_size, 0);

	runtime_wait(r, FUTURE_AS_RUNNABLE(&a_to_b));

	struct async_print_fut print_5 = async_print((void *)0x5);
	runtime_wait(r, FUTURE_AS_RUNNABLE(&print_5));

	struct async_memcpy_print_fut memcpy_print =
		async_memcpy_print(thread_mover, buf_b, buf_a, testbuf_size);
	runtime_wait(r, FUTURE_AS_RUNNABLE(&memcpy_print));

	runtime_delete(r);

	struct async_memcpy_print_fut memcpy_print_busy =
		async_memcpy_print(thread_mover, buf_b, buf_a, testbuf_size);
	FUTURE_BUSY_POLL(&memcpy_print_busy);

	vdm_delete(thread_mover);

	printf("%s %s %d\n", buf_a, buf_b, memcmp(buf_a, buf_b, testbuf_size));

	free(buf_a);
	free(buf_b);

	return 0;
}
