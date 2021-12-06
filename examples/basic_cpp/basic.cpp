// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * basic.cpp -- example showing miniasync integration with coroutines
 */

#include "libminiasync.h"

#include <iostream>
#include <queue>
#include <unordered_set>
#include <numeric>
#include <cassert>

#include "coroutine_helpers.hpp"

struct miniasync_awaitable_base
{
	bool await_ready();
	void await_resume();

	bool is_done();
	void resume();

protected:
	struct future_notifier notifier;
	std::coroutine_handle<> cont;
};

bool miniasync_awaitable_base::await_ready()
{
	return false;
}

void miniasync_awaitable_base::await_resume()
{
}

bool miniasync_awaitable_base::is_done()
{
	assert(notifier.notifier_used == FUTURE_NOTIFIER_POLLER);
	return *notifier.poller.ptr_to_monitor;
}

void miniasync_awaitable_base::resume()
{
	cont.resume();
}

template <typename Future>
struct miniasync_awaitable : public miniasync_awaitable_base
{
	miniasync_awaitable(Future future);
	void await_suspend(std::coroutine_handle<> cont);

	Future future;
};

/* Global state */
struct vdm* mover = nullptr;
std::vector<miniasync_awaitable_base*> awaitables;

template <typename Future>
miniasync_awaitable<Future>::miniasync_awaitable(Future future): future(future) {
}

template <typename Future>
void miniasync_awaitable<Future>::await_suspend(std::coroutine_handle<> cont)
{
	this->cont = cont;

	future_poll(FUTURE_AS_RUNNABLE(&future), &notifier);

	awaitables.push_back(static_cast<miniasync_awaitable_base*>(this));
}

miniasync_awaitable<vdm_memcpy_future> async_memcpy(void *dst, void *src, size_t n)
{
	return miniasync_awaitable<vdm_memcpy_future>(vdm_memcpy(mover, dst, src, n));
}

/* Executor loop */
void wait(task& t)
{
	t.h.resume();

	while (!t.h.done()) {
		// XXX - optimize this for single future case,
		// it's not optimal to allocate new vector each time
		auto awaitables_snapshot = std::move(awaitables);
		std::vector<bool> done(awaitables_snapshot.size(), false);
		int done_cnt = 0;

		do {
			// XXX: can use umwait
			for (int i = 0; i < awaitables_snapshot.size(); i++) {
				auto &f = awaitables_snapshot[i];
				if (f->is_done() && !done[i]) {
					done_cnt++;
					done[i] = true;
					f->resume();
				}
			}
		} while (done_cnt != awaitables_snapshot.size());
	}
}

task run_async_mempcy(char *dst, char *src, size_t n)
{
	std::cout << "Before memcpy" << std::endl;
	co_await async_memcpy(dst, src, n/2);
	std::cout << "After memcpy " << ((char*) dst) << std::endl;
	co_await async_memcpy(dst + n/2, src + n/2, n - n/2);
	std::cout << "After second memcpy " << ((char*) dst) << std::endl;

	auto a1 = async_memcpy(dst, src, 1);
	auto a2 = async_memcpy(dst + 1, src, 1);
	auto a3 = async_memcpy(dst + 2, src, 1);

	co_await when_all(a1, a2, a3);
	std::cout << "After 3 concurrent memcopies " << ((char*) dst) << std::endl;
}

task async_memcpy_print(char *dst, char *src, size_t n, std::string to_print)
{
	auto a1 = run_async_mempcy(dst, src, n/2);
	auto a2 = run_async_mempcy(dst + n/2, src + n/2, n - n/2);

	co_await when_all(a1, a2);

	std::cout << to_print << std::endl;
}

int
main(int argc, char *argv[])
{
	static constexpr auto buffer_size = 10;
	static constexpr auto to_copy = "something";
	static constexpr auto to_print = "async print!";

	char buffer[buffer_size] = {0};
	{
		mover = vdm_new(vdm_descriptor_pthreads_polled());
		auto future = async_memcpy_print(buffer, std::string(to_copy).data(), buffer_size, to_print);

		std::cout << "inside main" << std::endl;

		wait(future);

		std::cout << buffer << std::endl;
	}

	vdm_delete(mover);

	return 0;
}
