---
layout: manual
Content-Style: 'text/css'
title: _MP(FUTURE_POLL, 3)
collection: miniasync
header: FUTURE_POLL
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (future_poll.3 -- man page for miniasync future_poll operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**future_poll**() - poll the future


# SYNOPSIS #

```c
#include <libminiasync.h>

typedef void (*future_waker_wake_fn)(void *data);

enum future_state {
	FUTURE_STATE_IDLE,
	FUTURE_STATE_COMPLETE,
	FUTURE_STATE_RUNNING,
};

struct future_waker {
	void *data;
	future_waker_wake_fn wake;
};

struct future_poller {
	uint64_t *ptr_to_monitor;
};

enum future_notifier_type {
	FUTURE_NOTIFIER_NONE,
	FUTURE_NOTIFIER_WAKER,
	FUTURE_NOTIFIER_POLLER,
};

struct future_notifier {
	struct future_waker waker;
	struct future_poller poller;
	enum future_notifier_type notifier_used;
	uint32_t padding;
};

struct future;
enum future_state future_poll(struct future *fut,
		struct future_notifier *notifier);
```

For general description of future API, see **miniasync_future**(7).

# DESCRIPTION #

The **future_poll**() function makes implementation-defined operation towards
completion of the task associated with the future pointed by *fut*.

Additionally, the **future_poll**() function can accept future notifier

Future notifier *notifier* is an optional parameter that is passed to the future
task function. Future notifiers can be used to notify the caller that some progress
can be made and the future should be polled again. This can be used to avoid busy
polling.


## RETURN VALUE ##

The **future_poll**() function returns current state of the future.

Future can be in one of the following states:

* **FUTURE_STATE_IDLE** - future task has yet to begin execution

* **FUTURE_STATE_RUNNING** - future task is in progress

* **FUTURE_STATE_COMPLETE** - future task was completed


# SEE ALSO #

**miniasync**(7), **miniasync_future**(7) and **<https://pmem.io>**