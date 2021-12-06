/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021-2022, Intel Corporation */

/*
 * executor.hpp -- miniasync-aware coroutines executor.
 */

#include "libminiasync.h"

#include <deque>
#include <memory>
#include <optional>

#include "coroutine_helpers.hpp"
#include "miniasync_operation.hpp"

#ifndef MINIASYNC_EXECUTOR
#define MINIASYNC_EXECUTOR

/* Executor keeps a queue of coroutines to execute. New coroutine can be
 * submitted via submit() function. Run_to_completion will loop until all
 * coroutines are executed. */
struct executor_type {
	executor_type(std::unique_ptr<data_mover_threads, decltype(&data_mover_threads_delete)> &&data_mover) : vdm_(data_mover_threads_get_vdm(data_mover.get())), data_mover(std::move(data_mover))
	{
	}

	void submit(task &&t)
	{
		auto handle = std::move(t).release();
		pending_coro.push_back(handle);
	}

	void submit(miniasync_operation *operation)
	{
		pending_miniasync.push_back(operation);
	}

	void run_to_completion()
	{
		while (true) {
			auto next_miniasync_op = pop_next(pending_miniasync);
			auto next_coro = pop_next(pending_coro);
			if (!next_miniasync_op && !next_coro)
				break;

			if (next_miniasync_op)
				run_pending(next_miniasync_op.value());
			if (next_coro)
				run_pending(next_coro.value());
		}
	}

	vdm *get_mover()
	{
		return vdm_;
	}

 private:
	void run_pending(std::coroutine_handle<> h)
	{
		if (!h.done())
			h.resume();
	}

	void run_pending(miniasync_operation *operation)
	{
		if (operation->ready() && !operation->done()) {
			operation->resume();
		} else {
			/* Operation not ready, yet, put it back to the queue. */
			pending_miniasync.push_back(operation);
		}
	}

	template <typename Deque>
	std::optional<typename Deque::value_type> pop_next(Deque &deque)
	{
		if (deque.empty())
			return std::nullopt;

		auto first = deque.front();
		deque.pop_front();

		return first;
	}

	std::deque<std::coroutine_handle<>> pending_coro;
	std::deque<miniasync_operation *> pending_miniasync;
	struct vdm* vdm_;
	std::unique_ptr<data_mover_threads, decltype(&data_mover_threads_delete)> data_mover;
};

#endif /* MINIASYNC_EXECUTOR */
