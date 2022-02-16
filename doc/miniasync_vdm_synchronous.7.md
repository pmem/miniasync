---
layout: manual
Content-Style: 'text/css'
title: _MP(MINIASYNC_VDM_SYNCHRONOUS, 3)
collection: miniasync
header: MINIASYNC_VDM_SYNCHRONOUS
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (miniasync_vdm_synchronous.3 -- man page for miniasync vdm API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**miniasync_vdm_synchronous** - synchronous implementation of **miniasync**(7)
virtual data mover


# SYNOPSIS #

```c
#include <libminiasync.h>
```

For general description of virtual data mover API, see **miniasync**(7).


# DESCRIPTION #

Synchronous data mover is a synchronous implementation of the virtual data mover
interface.

When the future is polled for the first time the data mover operation will be executed
synchronously on the same thread that polled the future.

To create a new synchronous data mover instance, use **data_mover_sync_new**(3) function.

Synchronous data mover supports following operations:

* **vdm_memcpy**(3) - memory copy operation

Synchronous data mover does not support notifier feature.

For more information about the usage of thread data mover API, see *examples* directory
in miniasync repository <https://github.com/pmem/miniasync>.


# SEE ALSO #

 **data_mover_sync_new**(3), **vdm_memcpy**(3),
 **miniasync**(7), **miniasync_future**(7),
 **miniasync_vdm**(7) and **<https://pmem.io>**
