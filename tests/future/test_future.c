// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "libminiasync/future.h"
#include "test_helpers.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <libminiasync.h>

#define TEST_MAX_COUNT 10
#define FAKE_NOTIFIER ((void *)((uintptr_t)(0xDEADBEEF)))

struct foo_data {
	int counter;
};

struct foo_output {
	int result;
};

FUTURE(foo_fut, struct foo_data, struct foo_output);

enum future_state
foo_task(struct future_context *context,
	struct future_notifier *notifier)
{
	UT_ASSERTeq(notifier, FAKE_NOTIFIER);

	struct foo_data *data = future_context_get_data(context);
	data->counter++;
	if (data->counter == TEST_MAX_COUNT) {
		struct foo_output *output = future_context_get_output(context);
		output->result = 1;
		return FUTURE_STATE_COMPLETE;
	} else {
		return FUTURE_STATE_RUNNING;
	}
}

struct foo_fut
async_foo()
{
	struct foo_fut fut = {0};
	FUTURE_INIT(&fut, foo_task);
	fut.data.counter = 0;
	fut.output.result = 0;

	return fut;
}

int
main(void)
{
	struct foo_fut fut = async_foo();
	UT_ASSERTeq(FUTURE_STATE(&fut), FUTURE_STATE_IDLE);

	struct foo_output *output = FUTURE_OUTPUT(&fut);
	UT_ASSERTeq(output->result, 0);

	struct foo_data *data = FUTURE_DATA(&fut);
	UT_ASSERTeq(data->counter, 0);

	enum future_state state = FUTURE_STATE_RUNNING;

	for (int i = 0; i < TEST_MAX_COUNT; ++i) {
		UT_ASSERTeq(state, FUTURE_STATE_RUNNING);
		UT_ASSERTeq(FUTURE_STATE(&fut), i == 0 ?
			FUTURE_STATE_IDLE : FUTURE_STATE_RUNNING);
		UT_ASSERTeq(data->counter, i);
		UT_ASSERTeq(output->result, 0);

		state = future_poll(FUTURE_AS_RUNNABLE(&fut), FAKE_NOTIFIER);
	}
	UT_ASSERTeq(data->counter, TEST_MAX_COUNT);
	UT_ASSERTeq(output->result, 1);
	UT_ASSERTeq(state, FUTURE_STATE_COMPLETE);

	/* polling on a complete future is a noop */
	state = future_poll(FUTURE_AS_RUNNABLE(&fut), FAKE_NOTIFIER);

	UT_ASSERTeq(data->counter, TEST_MAX_COUNT);
	UT_ASSERTeq(output->result, 1);
	UT_ASSERTeq(state, FUTURE_STATE_COMPLETE);

	return 0;
}
