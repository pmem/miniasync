// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include <stdlib.h>

#include "libminiasync/runtime.h"
#include "core/os_thread.h"
#include "core/os.h"
#include "core/util.h"

struct runtime_waker_data {
	os_cond_t *cond;
	os_mutex_t *lock;
};

static void
runtime_waker_wake(void *fdata)
{
	struct runtime_waker_data *data = fdata;
	os_mutex_lock(data->lock);
	os_cond_signal(data->cond);
	os_mutex_unlock(data->lock);
}

struct runtime {
	os_cond_t cond;
	os_mutex_t lock;

	uint64_t spins_before_sleep;
	struct timespec cond_wait_time;
};

struct runtime *
runtime_new(void)
{
	struct runtime *runtime = malloc(sizeof(struct runtime));
	if (runtime == NULL)
		return NULL;

	os_cond_init(&runtime->cond);
	os_mutex_init(&runtime->lock);
	runtime->spins_before_sleep = 1000;
	runtime->cond_wait_time = (struct timespec){0, 1000000};

	return runtime;
}

void
runtime_delete(struct runtime *runtime)
{
	free(runtime);
}

static void
runtime_sleep(struct runtime *runtime)
{
	os_mutex_lock(&runtime->lock);
	struct timespec ts;
	os_clock_gettime(CLOCK_REALTIME, &ts);
	static const size_t nsec_in_sec = 1000000000ULL;
	ts.tv_nsec += runtime->cond_wait_time.tv_nsec;
	uint64_t secs = (uint64_t)ts.tv_nsec / nsec_in_sec;
	ts.tv_nsec -= (long)(secs * nsec_in_sec);
	ts.tv_sec += (long)(runtime->cond_wait_time.tv_sec + (long)secs);

	os_cond_timedwait(&runtime->cond, &runtime->lock, &ts);
	os_mutex_unlock(&runtime->lock);
}

void
quicksort_futs_async_property(struct future *futs[],
		int first_index, int last_index)
{
	enum FUTURE_PROPERTY property = FUTURE_PROPERTY_ASYNC;
	int i, j, pivot;
	if (first_index < last_index) {
		pivot = first_index;
		i = first_index;
		j = last_index;

		while (i < j) {
			while (fut_has_property(futs[i], property) >=
					fut_has_property(futs[pivot],
						property) && i < last_index)
				i++;
			while (fut_has_property(futs[j], property) <
					fut_has_property(futs[pivot], property))
				j--;
			if (i < j) {
				struct future *fut = futs[i];
				futs[i] = futs[j];
				futs[j] = fut;
			}
		}

		struct future *fut = futs[pivot];
		futs[pivot] = futs[j];
		futs[j] = fut;
		quicksort_futs_async_property(futs, first_index, j - 1);
		quicksort_futs_async_property(futs, j + 1, last_index);
	}
}

void
runtime_wait_multiple(struct runtime *runtime, struct future *futs[],
						size_t nfuts)
{
	struct runtime_waker_data waker_data;
	waker_data.cond = &runtime->cond;
	waker_data.lock = &runtime->lock;

	struct future_notifier notifier;
	notifier.waker = (struct future_waker){&waker_data, runtime_waker_wake};
	notifier.poller.ptr_to_monitor = NULL;
	size_t ndone = 0;

	for (;;) {
		for (uint64_t i = 0; i < runtime->spins_before_sleep; ++i) {
			quicksort_futs_async_property(futs, 0, (int)nfuts - 1);
			for (uint64_t f = 0; f < nfuts; ++f) {
				struct future *fut = futs[f];
				if (fut->context.state == FUTURE_STATE_COMPLETE)
					continue;

				if (future_poll(fut, &notifier) ==
				    FUTURE_STATE_COMPLETE) {
					ndone++;
				}
				switch (notifier.notifier_used) {
					case FUTURE_NOTIFIER_POLLER:
					/*
					 * TODO: if this is the only future
					 * being polled, use umwait/umonitor
					 * for power-optimized polling.
					 */
					break;
					case FUTURE_NOTIFIER_WAKER:
					case FUTURE_NOTIFIER_NONE:
					/* nothing to do for wakers or none */
					break;
				};
			}

			if (ndone == nfuts)
				return;

			WAIT();
		}
		runtime_sleep(runtime);
	}
}

void
runtime_wait(struct runtime *runtime, struct future *fut)
{
	runtime_wait_multiple(runtime, &fut, 1);
}
