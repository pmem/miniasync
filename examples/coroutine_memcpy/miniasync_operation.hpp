/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021-2022, Intel Corporation */

/*
 * miniasync_operation.hpp - awaitable wrapper around miniasync operations
 */

#include "libminiasync.h"

#include <coroutine>
#include <utility>

#ifndef MINIASYNC_OPERATION
#define MINIASYNC_OPERATION

struct executor_type;

struct miniasync_operation {
	template <typename Executor, typename Operation, typename... Args>
	miniasync_operation(Executor &executor, Operation &&operation, Args &&...args) : executor(executor), future(operation(executor.get_mover(), std::forward<Args>(args)...))
	{
		future_poll(FUTURE_AS_RUNNABLE(&future), &notifier);
	}

	void await_resume();
	bool await_ready();
	void await_suspend(std::coroutine_handle<> h);

	bool done();
	bool ready();
	void resume();

 private:
	executor_type &executor;
	std::coroutine_handle<> h;
	struct future_notifier notifier;
	vdm_operation_future future;
};

static inline auto async_memcpy(executor_type &executor, void *dst, void *src, size_t n)
{
	return miniasync_operation(executor, vdm_memcpy, dst, src, n, 0);
}

#endif /* MINIASYNC_OPERATION */
