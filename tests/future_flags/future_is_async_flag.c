// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "libminiasync.h"
#include "test_helpers.h"
#include "core/util.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_MAX_COUNT 20
#define FAKE_MAP_ARG ((void *)((uintptr_t)(0xFEEDCAFE)))

int results[20];
int results_index = 0;

struct countup_data {
	int counter;
	int max_count;
	uint64_t future_id;
};

struct countup_output {
	int result;
};

FUTURE(countup_fut, struct countup_data, struct countup_output);

enum future_state
countup_task(struct future_context *context,
	struct future_notifier *notifier)
{
	struct countup_data *data = future_context_get_data(context);
	data->counter++;
	if (data->counter == data->max_count) {
		struct countup_output *output =
			future_context_get_output(context);
		output->result += 1;
		results[results_index] = data->future_id;
		results_index++;
		return FUTURE_STATE_COMPLETE;
	} else {
		return FUTURE_STATE_RUNNING;
	}
}

struct countup_fut
countup_async(int max_count, uint64_t future_id)
{
	struct countup_fut fut = {.output.result = 0};
	FUTURE_INIT(&fut, countup_task);
	fut.data.counter = 0;
	fut.data.max_count = max_count;
	fut.output.result = 0;
	fut.data.future_id = future_id;

	FUTURE_SET_FLAG(&fut, FUTURE_IS_ASYNC);

	return fut;
}
struct countup_fut
countup_non_async(int max_count, uint64_t future_id)
{
	struct countup_fut fut = {.output.result = 0};
	FUTURE_INIT(&fut, countup_task);
	fut.data.counter = 0;
	fut.data.max_count = max_count;
	fut.output.result = 0;
	fut.data.future_id = future_id;

	return fut;
}

void
test_basic_futures(void)
{
	struct runtime *r = runtime_new();
	struct future **futures = malloc(sizeof(struct future *) * 4);
	struct countup_fut up1 = countup_async(TEST_MAX_COUNT, 1);
	UT_ASSERTeq(FUTURE_STATE(&up1), FUTURE_STATE_IDLE);
	
	if (!FUTURE_HAS_FLAG(&up1, FUTURE_IS_ASYNC)) {
		UT_FATAL("future async flag is not set while it should be");
	}

	struct countup_fut up2 = countup_non_async(TEST_MAX_COUNT, 2);
	UT_ASSERTeq(FUTURE_STATE(&up2), FUTURE_STATE_IDLE);
	
	if (FUTURE_HAS_FLAG(&up2, FUTURE_IS_ASYNC)) {
		UT_FATAL("future async flag is set while it shouldn't be");
	}

	futures[0] = FUTURE_AS_RUNNABLE(&up2);
	futures[1] = FUTURE_AS_RUNNABLE(&up1);

	runtime_wait_multiple(r, futures, 2);

	UT_ASSERTeq(results[0], 1);
	UT_ASSERTeq(results[1], 2);

	free(futures);
	runtime_delete(r);
}

struct chained_up_fut_data {
	FUTURE_CHAIN_ENTRY(struct countup_fut, up1);
	FUTURE_CHAIN_ENTRY(struct countup_fut, up2);
};

struct chained_up_fut_output {
	int result_sum;
};

FUTURE(chained_up_fut, struct chained_up_fut_data, struct chained_up_fut_output);

void
up1_to_up2_map(struct future_context *lhs, struct future_context *rhs,
	void *arg)
{
	struct countup_output *up1_output = future_context_get_data(lhs);
	struct countup_output *up2_output = future_context_get_output(rhs);

	up2_output->result += up1_output->result;
}

void
up2_to_result_map(struct future_context *lhs, struct future_context *rhs,
	void *arg)
{
	struct countup_output *up2_output = future_context_get_output(lhs);
	struct chained_up_fut_output *output = future_context_get_output(rhs);
	output->result_sum = up2_output->result;
}

struct chained_up_fut
countup_chained_sync_async(int count, uint64_t id_fut1, uint64_t id_fut2)
{
	struct chained_up_fut fut = {.output.result_sum = 0};
	FUTURE_CHAIN_ENTRY_INIT(&fut.data.up1, countup_non_async(count, id_fut1),
		up1_to_up2_map, FAKE_MAP_ARG);
	FUTURE_CHAIN_ENTRY_INIT(&fut.data.up2, countup_async(count, id_fut2),
		up2_to_result_map, FAKE_MAP_ARG);
	FUTURE_CHAIN_INIT(&fut);

	return fut;
}

struct chained_up_fut
countup_chained_async_sync(int count, uint64_t id_fut1, uint64_t id_fut2)
{
	struct chained_up_fut fut = {.output.result_sum = 0};
	FUTURE_CHAIN_ENTRY_INIT(&fut.data.up1, countup_async(count, id_fut1),
		up1_to_up2_map, FAKE_MAP_ARG);
	FUTURE_CHAIN_ENTRY_INIT(&fut.data.up2, countup_non_async(count, id_fut2),
		up2_to_result_map, FAKE_MAP_ARG);
	FUTURE_CHAIN_INIT(&fut);

	return fut;
}


void
test_chained_future()
{
	struct runtime *r = runtime_new();
	struct future **futures = malloc(sizeof(struct future *) * 2);
	struct chained_up_fut fut1 = countup_chained_sync_async(TEST_MAX_COUNT, 3, 4);
	struct chained_up_fut fut2 = countup_chained_async_sync(TEST_MAX_COUNT, 5, 6);

	futures[0] = FUTURE_AS_RUNNABLE(&fut1);
	futures[1] = FUTURE_AS_RUNNABLE(&fut2);

	runtime_wait_multiple(r, futures, 2);

	struct chained_up_fut_output *output = FUTURE_OUTPUT(&fut1);
	UT_ASSERTeq(output->result_sum, 21);

	output = FUTURE_OUTPUT(&fut2);
	UT_ASSERTeq(output->result_sum, 21);

	UT_ASSERTeq(results[2], 5);
	UT_ASSERTeq(results[3], 3);
	UT_ASSERTeq(results[4], 4);
	UT_ASSERTeq(results[5], 6);

	free(futures);
	runtime_delete(r);
}


int
main(void)
{
	test_basic_futures();
	test_chained_future();

	return 0;
}
