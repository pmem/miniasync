---
layout: manual
Content-Style: 'text/css'
title: _MP(MINIASYNC_VDM_THREADS, 3)
collection: miniasync
header: MINIASYNC_VDM_THREADS
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (miniasync_vdm_threads.3 -- man page for miniasync vdm threads mover API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**miniasync_vdm_threads** - virtual data mover implementation for miniasync using
system threads


# SYNOPSIS #

```c
#include <libminiasync.h>
```

For general description of virtual data mover API, see **miniasync**(7).


# DESCRIPTION #

Thread data mover is a thread-based implementation of the virtual data mover.
Operations submitted to a thread data mover instance are queued and then executed
by one of the working threads that has taken operation off the queue. Working threads
of each thread data mover instance are put to sleep until there's a data mover operation
to execute.

To create a new thread data mover instance, use **data_mover_threads_new**(3) function.

Thread data mover supports following operations:

* **vdm_memcpy**(3) - memory copy operation

Thread data mover supports following notifer types:

* **FUTURE_NOTIFIER_NONE** - no notifier
* **FUTURE_NOTIFIER_WAKER** - waker

For more information about notifiers, see **miniasync_future**(7).


# SEE ALSO #

**data_mover_threads_new**(3),
**miniasync**(7), **miniasync_future**(7),
**miniasync_vdm**(7) and **<https://pmem.io>**
