// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */
// SPDX-License-Identifier: MIT
/* Copyright (c) Lewis Baker */

#include <coroutine>
#include <utility>
#include <string>
#include <atomic>
#include <functional>
#include <memory>

#ifndef MINIASYNC_COROUTINE_HELPERS
#define MINIASYNC_COROUTINE_HELPERS

/* Helper structures for coroutines, they are heavily inspired by
 * https://github.com/lewissbaker/cppcoro
 */

/* This is a generic task which supports continuation. */
struct task {
  struct promise_type {
      struct final_awaitable
      {
          bool await_ready() const noexcept { return false; }
          void await_resume() noexcept {}

          std::coroutine_handle<> await_suspend(std::coroutine_handle<task::promise_type> h) noexcept {
			  auto &cont = h.promise().cont;
              return cont ? cont : std::noop_coroutine();
          }
      };


    task get_return_object() { return task{std::coroutine_handle<task::promise_type>::from_promise(*this)}; }
    std::suspend_always initial_suspend() { return {}; }
    auto final_suspend() noexcept { return final_awaitable{}; }
    void return_void() {}
    void unhandled_exception() {}

    std::coroutine_handle<> cont;
  };

  void wait() {
    h.resume();
  }

  bool await_ready() { return !h || h.done();}
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> aw) {
      h.promise().cont = aw;
      return h;
    }
    void await_resume() {}

  std::coroutine_handle<task::promise_type> h;
};

namespace detail {
struct when_all_task {
  struct promise_type {
	  std::atomic<int> *counter;
	  std::coroutine_handle<> continuation;

	void start(std::atomic<int>& counter, std::coroutine_handle<> continuation)
  	{
		this->counter = &counter;
		this->continuation = continuation;

		std::coroutine_handle<when_all_task::promise_type>::from_promise(*this).resume();
  	}

      struct final_awaitable
      {
          bool await_ready() const noexcept { return false; }
          void await_resume() noexcept {}

          void await_suspend(std::coroutine_handle<when_all_task::promise_type> h) noexcept {
			  auto cnt = h.promise().counter->fetch_sub(1);
			  if (cnt - 1 == 0) {
				h.promise().continuation.resume();
			  }
          }
      };

    when_all_task get_return_object() { return when_all_task{std::coroutine_handle<when_all_task::promise_type>::from_promise(*this)}; }
    std::suspend_always initial_suspend() { return {}; }
    auto final_suspend() noexcept { return final_awaitable{}; }
    void return_void() {}
    void unhandled_exception() {}
  };

  void start(std::atomic<int>& counter, std::coroutine_handle<> continuation)
  {
	  h.promise().start(counter, continuation);
  }

  std::coroutine_handle<when_all_task::promise_type> h;
};

template <typename Awaitable>
when_all_task make_when_all_task(Awaitable awaitable)
{
	co_await awaitable;
}

template <typename Task>
struct when_all_ready_awaitable
{
	when_all_ready_awaitable(std::vector<Task>&& tasks): counter(tasks.size()), tasks(std::move(tasks))
	{
	}

	bool await_ready()
	{
		return false;
	}

	void await_suspend(std::coroutine_handle<> h)
	{
		for (auto&& task : tasks)
		{
			task.start(counter, h);
		}
	}

	void await_resume() {}

	std::atomic<int> counter = 0;
	std::vector<Task> tasks;
};
}

template <typename... Awaitables>
auto when_all(Awaitables&&... awaitables)
{
	std::vector<detail::when_all_task> tasks;

	for (auto &&a : {awaitables...})
		tasks.emplace_back(detail::make_when_all_task(std::move(a)));

	return detail::when_all_ready_awaitable(std::move(tasks));
}

#endif MINIASYNC_COROUTINE_HELPERS
