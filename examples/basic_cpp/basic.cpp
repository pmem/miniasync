// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * basic.cpp -- example showing miniasync integration with coroutines
 */

#include "libminiasync.h"

#include <iostream>
#include <queue>

#include "coroutine_helpers.hpp"

/* Container from which executor will take futures to execute */
std::vector<struct vdm_memcpy_future> futures;
/* continuations[x] is a handle to coroutine to be resumed after futures[x] */
std::vector<std::coroutine_handle<>> continuations;

struct memcpy_task
{
	memcpy_task(char *dst, char *src, size_t n) {
		pthread_mover = std::shared_ptr<vdm>(vdm_new(vdm_descriptor_pthreads()), &vdm_delete);
		fut = vdm_memcpy(pthread_mover.get(), (void*)dst, (void*)src, n);
	}

	bool await_ready()
	{
		return false;
	}

	void await_suspend(std::coroutine_handle<> h)
	{
		futures.emplace_back(fut);
		continuations.emplace_back(h);
	}

	void await_resume() {}

	// XXX: change to unique_ptr after fixing when_all_awaitable
	std::shared_ptr<vdm> pthread_mover;
	struct vdm_memcpy_future fut;
};

/* Executor loop */
void wait(struct runtime *r)
{
	/* Inside this loop, we move `futures` and `continuations` to local
	 * variables and execute them. During the execution, coroutines
	 * might put some more work to be done into `futures` and `continuations`. */
	while (futures.size()) {
		auto futures_snapshot = std::move(futures);
		auto continuations_snapshot = std::move(continuations);

		// XXX: can we avoid this loop?
		std::vector<future*> futs;
		for (auto &f : futures_snapshot)
			futs.emplace_back(FUTURE_AS_RUNNABLE(&f));

		runtime_wait_multiple(r, futs.data(), futs.size());
		for (auto &c : continuations_snapshot)
			c.resume();
	}
}

task async_mempcy(char *dst, char *src, size_t n)
{
	std::cout << "Before memcpy" << std::endl;
	co_await memcpy_task{dst, src, n/2};
	std::cout << "After memcpy " << ((char*) dst) << std::endl;
	co_await memcpy_task{dst + n/2, src + n/2, n - n/2};
	std::cout << "After second memcpy " << ((char*) dst) << std::endl;

	auto a1 = memcpy_task{dst, src, 1};
	auto a2 = memcpy_task{dst + 1, src, 1};
	auto a3 = memcpy_task{dst + 2, src, 1};

	co_await when_all(a1, a2, a3);
	std::cout << "After 3 concurrent memcopies " << ((char*) dst) << std::endl;
}

task async_memcpy_print(char *dst, char *src, size_t n, std::string to_print)
{
	auto a1 = async_mempcy(dst, src, n/2);
	auto a2 = async_mempcy(dst + n/2, src + n/2, n - n/2);

	co_await when_all(a1, a2);

	std::cout << to_print << std::endl;
}

int
main(int argc, char *argv[])
{
	auto r = runtime_new();

	static constexpr auto buffer_size = 10;
	static constexpr auto to_copy = "something";
	static constexpr auto to_print = "async print!";

	char buffer[buffer_size] = {0};
	{
		auto future = async_memcpy_print(buffer, std::string(to_copy).data(), buffer_size, to_print);

		std::cout << "inside main" << std::endl;

		future.h.resume();
		wait(r);

		std::cout << buffer << std::endl;
	}

	runtime_delete(r);

	return 0;
}
