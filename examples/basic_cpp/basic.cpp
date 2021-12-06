// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * basic.cpp -- example showing miniasync integration with coroutines
 */

#include <coroutine>
#include <utility>
#include <string>
#include <iostream>

#include "libminiasync.h"

struct simple_future {
	struct promise_type;
	using handle_type = std::coroutine_handle<promise_type>;

	simple_future(handle_type h): h(h) {}
	~simple_future();

	handle_type h;

	void wait(struct runtime *r);
};

struct coroutine_data {
	std::coroutine_handle<simple_future::promise_type> handle;
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

struct simple_future::promise_type {
	simple_future get_return_object() { return simple_future(handle_type::from_promise(*this)); }
	std::suspend_never initial_suspend() { return {}; }
	std::suspend_never final_suspend() noexcept { return {}; }
	void return_void() {}
	void unhandled_exception() {}

	struct vdm *pthread_mover;
	struct async_memcpy_resume_fut fut;
};

void simple_future::wait(struct runtime *r)
{
	runtime_wait(r, FUTURE_AS_RUNNABLE(&h.promise().fut));
}

simple_future::~simple_future() { 
	if (h) { 
		vdm_delete(h.promise().pthread_mover);
		h.destroy();
	}
}

static enum future_state
resume_impl(struct future_context *ctx, struct future_waker waker)
{
	struct coroutine_data *data = reinterpret_cast<struct coroutine_data *>(future_context_get_data(ctx));

	// Resume coroutine
	data->handle();

	return FUTURE_STATE_COMPLETE;
}

static struct coroutine_future
resume_coroutine(std::coroutine_handle<simple_future::promise_type> h)
{
	struct coroutine_future fut;
	fut.data.handle = h;

	FUTURE_INIT(&fut, resume_impl);

	return fut;
}

auto async_memcpy(void *dst, void *src, size_t n)
{
	struct awaitable {
		awaitable(void *dst, void *src, size_t n): dst(dst), src(src), n(n)
		{
		}

		bool await_ready() { return false; /* always suspend (call await_suspend) */ }
		void await_suspend(std::coroutine_handle<simple_future::promise_type> h) {
			auto pthread_mover = vdm_new(vdm_descriptor_pthreads());
			auto &chain = h.promise().fut;

			h.promise().pthread_mover = pthread_mover;

			FUTURE_CHAIN_ENTRY_INIT(&chain.data.memcpy,
						vdm_memcpy(pthread_mover, dst, src, n),
						NULL, NULL);
			FUTURE_CHAIN_ENTRY_INIT(&chain.data.resume, resume_coroutine(h),
						NULL, NULL);

			FUTURE_CHAIN_INIT(&chain);
		}

		void await_resume() {}

		void *dst;
		void *src;
		size_t n;
	};

	return awaitable(dst, src, n);
}

simple_future async_memcpy_print(std::string to_copy, char *buffer, const std::string &to_print)
{
	co_await async_memcpy(reinterpret_cast<void*>(buffer), reinterpret_cast<void*>(to_copy.data()), to_copy.size());
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
		auto future = async_memcpy_print(to_copy, buffer, to_print);

		std::cout << "inside main" << std::endl;

		// actually executes future on runtime r
		future.wait(r);
	}

	runtime_delete(r);

	return 0;
}
