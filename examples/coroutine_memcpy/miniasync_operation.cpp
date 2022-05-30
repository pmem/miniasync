// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * miniasync_operation.cpp - implementation of miniasync_operation
 */

#include "miniasync_operation.hpp"

#include <cassert>

#include "executor.hpp"

void miniasync_operation::await_resume()
{
}

bool miniasync_operation::await_ready()
{
	return ready();
}

void miniasync_operation::await_suspend(std::coroutine_handle<> h)
{
	this->h = h;
	this->executor.submit(this);
}

bool miniasync_operation::ready()
{
	assert(notifier.notifier_used == FUTURE_NOTIFIER_POLLER);
	return *notifier.poller.ptr_to_monitor == 1;
}

bool miniasync_operation::done()
{
	return this->h.done();
}

void miniasync_operation::resume()
{
	this->h.resume();
}
