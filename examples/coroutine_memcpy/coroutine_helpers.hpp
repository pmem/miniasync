/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021-2022, Intel Corporation */
// SPDX-License-Identifier: MIT
/* Copyright (c) Lewis Baker */

#include <atomic>
#include <coroutine>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#ifndef MINIASYNC_COROUTINE_HELPERS
#define MINIASYNC_COROUTINE_HELPERS

/*
 * Helper structures for coroutines, they are heavily inspired by
 * https://github.com/lewissbaker/cppcoro
 */

/* This is a generic task which supports continuation. */
struct task {
	struct promise_type {
		struct final_awaitable {
			bool await_ready() const noexcept
			{
				return false;
			}
			void await_resume() noexcept
			{
			}

			void await_suspend(std::coroutine_handle<task::promise_type> h) noexcept
			{
				auto &cont = h.promise().cont;
				if (cont)
					cont.resume();
			}
		};

		task get_return_object()
		{
			return task{std::coroutine_handle<task::promise_type>::from_promise(*this)};
		}
		std::suspend_always initial_suspend()
		{
			return {};
		}
		auto final_suspend() noexcept
		{
			return final_awaitable{};
		}
		void return_void()
		{
		}
		void unhandled_exception()
		{
		}

		std::coroutine_handle<task::promise_type> cont;
	};

	void wait()
	{
		h.resume();
	}

	std::coroutine_handle<task::promise_type> release() &&
	{
		return std::exchange(h, nullptr);
	}

	bool await_ready()
	{
		return !h || h.done();
	}

	std::coroutine_handle<> await_suspend(std::coroutine_handle<task::promise_type> aw)
	{
		h.promise().cont = aw;
		return h;
	}

	void await_resume()
	{
	}

	std::coroutine_handle<task::promise_type> h;
};

namespace detail
{

template <typename Awaitable>
task when_all_task(Awaitable awaitable, std::atomic<int> &counter, std::coroutine_handle<> h)
{
	co_await awaitable;

	auto cnt = counter.fetch_sub(1);
	if (cnt - 1 == 0) {
		h.resume();
	}
}

template <typename Task>
struct when_all_ready_awaitable {
	when_all_ready_awaitable(std::vector<Task> &&tasks) : counter(tasks.size()), tasks(std::move(tasks))
	{
	}

	bool await_ready()
	{
		return false;
	}

	void await_suspend(std::coroutine_handle<> h)
	{
		for (auto &&task : tasks) {
			when_all_task(std::move(task), counter, h).h.resume();
		}
	}

	void await_resume()
	{
	}

	std::atomic<int> counter = 0;
	std::vector<Task> tasks;
};
} // namespace detail

template <typename A, typename... Awaitables>
auto when_all(A &&aw, Awaitables &&...awaitables)
{
	std::vector<std::remove_reference_t<A>> tasks;
	tasks.emplace_back(std::move<A>(aw));

	for (auto &&a : {awaitables...})
		tasks.emplace_back(std::move(a));

	return detail::when_all_ready_awaitable(std::move(tasks));
}

#endif
