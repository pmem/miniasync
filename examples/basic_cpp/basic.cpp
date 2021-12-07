// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licensed under MIT license.
///////////////////////////////////////////////////////////////////////////////

/*
 * basic.cpp -- example showing miniasync integration with coroutines
 */

#include <coroutine>
#include <utility>
#include <string>
#include <iostream>

#include "libminiasync.h"

/* Similar to https://github.com/lewissbaker/cppcoro/blob/master/include/cppcoro/task.hpp */
struct task {
  struct promise_type {
      struct final_awaitable
      {
          bool await_ready() const noexcept { return false; }
          void await_resume() noexcept {}

          std::coroutine_handle<> await_suspend(std::coroutine_handle<task::promise_type> h) noexcept {
              return h.promise().cont;
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

struct coroutine_data {
	std::coroutine_handle<> handle;
};

struct coroutine_output {
};

FUTURE(coroutine_future, struct coroutine_data, struct coroutine_output);

struct async_memcpy_resume_data {
	FUTURE_CHAIN_ENTRY(struct vdm_memcpy_future, memcpy);
	FUTURE_CHAIN_ENTRY(struct coroutine_future, resume);
};

struct async_memcpy_resume_output {
};

FUTURE(async_memcpy_resume_fut, struct async_memcpy_resume_data,
		struct async_memcpy_resume_output);

static enum future_state
resume_impl(struct future_context *ctx, struct future_waker waker)
{
	struct coroutine_data *data = reinterpret_cast<struct coroutine_data *>(future_context_get_data(ctx));

	// Resume coroutine
	data->handle();

	return FUTURE_STATE_COMPLETE;
}

static struct coroutine_future
resume_coroutine(std::coroutine_handle<> h)
{
	struct coroutine_future fut;
	fut.data.handle = h;

	FUTURE_INIT(&fut, resume_impl);

	return fut;
}

struct memcpy_task
{
	memcpy_task(void *dst, void *src, size_t n, struct async_memcpy_resume_fut *fut): fut(fut) {
		auto *pthread_mover = vdm_new(vdm_descriptor_pthreads()); // XXX - lifetime
		FUTURE_CHAIN_ENTRY_INIT(&fut->data.memcpy,
			vdm_memcpy(pthread_mover, dst, src, n),
			NULL, NULL);
	}

	bool await_ready()
	{
		return false;
	}

	void await_suspend(std::coroutine_handle<> h)
	{
		FUTURE_CHAIN_ENTRY_INIT(&fut->data.resume, resume_coroutine(h),
			NULL, NULL);
		FUTURE_CHAIN_INIT(fut);
	}

	void await_resume() {}

	struct async_memcpy_resume_fut *fut;
};

task async_mempcy(void *dst, void *src, size_t n, struct async_memcpy_resume_fut *fut)
{
	std::cout << "Before memcpy" << std::endl;
	co_await memcpy_task{dst, src, n, fut};
	std::cout << "After memcpy" << std::endl;
}

task async_memcpy_print(std::string to_copy, char *buffer, const std::string &to_print, struct async_memcpy_resume_fut *fut)
{
	co_await async_mempcy(reinterpret_cast<void*>(buffer), reinterpret_cast<void*>(to_copy.data()), to_copy.size(), fut);
	std::cout << to_print << std::endl;
}

int
main(int argc, char *argv[])
{
	auto r = runtime_new();

	static constexpr auto buffer_size = 10;
	static constexpr auto to_copy = "something";
	static constexpr auto to_print = "async print!";

	char buffer[buffer_size];
	for (auto &c : buffer)
		c = 0;

	{
		struct async_memcpy_resume_fut fut;
		auto future = async_memcpy_print(to_copy, buffer, to_print, &fut);
		// auto future = async_mempcy(buffer, std::string(to_copy).data(), 4);

		std::cout << "inside main" << std::endl;

		// actually executes future on runtime r
		// XXX - make it a single function
		future.wait();
		runtime_wait(r, FUTURE_AS_RUNNABLE(&fut));

		std::cout << buffer << std::endl;
	}

	runtime_delete(r);

	return 0;
}
