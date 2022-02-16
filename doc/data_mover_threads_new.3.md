---
layout: manual
Content-Style: 'text/css'
title: _MP(DATA_MOVER_THREADS_NEW, 3)
collection: miniasync
header: DATA_MOVER_THREADS_NEW
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (data_mover_threads_new.3 -- man page for miniasync data_mover_threads_new operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**data_mover_threads_new**(), **data_mover_threads_delete**(),
**data_mover_threads_default**() - allocate, free or allocate with default parameters
threads data mover structure


# SYNOPSIS #

```c
#include <libminiasync.h>

enum future_notifier_type {
	FUTURE_NOTIFIER_NONE,
	FUTURE_NOTIFIER_WAKER,
	FUTURE_NOTIFIER_POLLER,
};

struct data_mover_threads;

struct data_mover_threads *data_mover_threads_new(size_t nthreads,
	size_t ringbuf_size, enum future_notifier_type desired_notifier);
void data_mover_threads_delete(struct data_mover_threads *dmt);
struct data_mover_threads *data_mover_threads_default();
```

For general description of thread data mover API, see **miniasync_vdm_threads**(7).


# DESCRIPTION #

Thread data mover is a thread-based implementation of the virtual data mover.
Operations submitted to a thread data mover instance are queued and then executed
by one of the working threads. Working threads of each thread data mover instance
are put to sleep until a data mover operation to execute.

The **data_mover_threads_new**() function allocates and initializes a new thread
data mover structure. This function spawns *nthreads* working threads during
initialization. Parameter *desired_notifier* specifies the notifier type.

Currently, thread data mover supports following notifer types:

* **FUTURE_NOTIFIER_NONE** - no notifier
* **FUTURE_NOTIFIER_WAKER** - waker

For more information about notifiers, see **miniasync_future**(7).

The **data_mover_threads_delete**() function frees and finalizes the synchronous
data mover structure pointed by *dms*. Spawned threads are cleaned up during
finalization.


# RETURN VALUE #

The **data_mover_threads_new**() function returns a pointer to *struct data_mover_sync*
structure or **NULL** if the allocation or initialization failed.

The **data_mover_threads_delete**() function does not return any value.


# SEE ALSO #

**miniasync**(7), **miniasync_vdm_threads**(7)
and **<https://pmem.io>**