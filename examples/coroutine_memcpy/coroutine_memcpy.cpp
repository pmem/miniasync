// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * coroutine_memcpy.cpp -- example showing miniasync integration with coroutines
 */

#include "libminiasync.h"

#include <cassert>
#include <deque>
#include <iostream>
#include <numeric>
#include <optional>
#include <queue>
#include <unordered_set>

#include "coroutine_helpers.hpp"
#include "executor.hpp"
#include "miniasync_operation.hpp"

task run_async_memcpy(executor_type &executor, char *dst, char *src, size_t n)
{
	std::cout << "Before memcpy" << std::endl;
	co_await async_memcpy(executor, dst, src, n / 2);
	std::cout << "After memcpy " << ((char *)dst) << std::endl;
	co_await async_memcpy(executor, dst + n / 2, src + n / 2, n - n / 2);
	std::cout << "After second memcpy " << ((char *)dst) << std::endl;

	auto a1 = async_memcpy(executor, dst, src, 1);
	auto a2 = async_memcpy(executor, dst + 1, src, 1);
	auto a3 = async_memcpy(executor, dst + 2, src, 1);

	co_await when_all(a1, a2, a3);
	std::cout << "After 3 concurrent memcopies " << ((char *)dst) << std::endl;
}

task async_memcpy_print(executor_type &executor, char *dst, char *src, size_t n, std::string to_print)
{
	auto a1 = run_async_memcpy(executor, dst, src, n / 2);
	auto a2 = run_async_memcpy(executor, dst + n / 2, src + n / 2, n - n / 2);

	co_await when_all(a1, a2);

	std::cout << to_print << std::endl;
}

int main(int argc, char *argv[])
{
	static constexpr size_t nthreads = 3;
	static constexpr size_t ringbuf_size = 1024;
	executor_type executor(
		std::unique_ptr<data_mover_threads, decltype(&data_mover_threads_delete)>(data_mover_threads_new(nthreads, ringbuf_size, FUTURE_NOTIFIER_POLLER), &data_mover_threads_delete));

	static constexpr auto buffer_size = 10;
	static constexpr auto to_copy = "something";
	static constexpr auto to_print = "async print!";

	char buffer[buffer_size] = {0};
	auto t = async_memcpy_print(executor, buffer, std::string(to_copy).data(), buffer_size, to_print);
	executor.submit(std::move(t));

	std::cout << "inside main" << std::endl;

	executor.run_to_completion();

	std::cout << buffer << std::endl;

	return 0;
}
