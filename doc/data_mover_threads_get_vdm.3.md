---
layout: manual
Content-Style: 'text/css'
title: _MP(DATA_MOVER_THREADS_GET_VDM, 3)
collection: miniasync
header: DATA_MOVER_THREADS_GET_VDM
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (data_mover_threads_get_vdm.3 -- man page for miniasync data_mover_threads_get_vdm operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**data_mover_threads_get_vdm**() - get virtual data mover structure from the thread
data mover structure

# SYNOPSIS #

```c
#include <libminiasync.h>

struct vdm;
struct data_mover_threads;

struct vdm *data_mover_threads_get_vdm(struct data_mover_threads *dmt);
```

For general description of thread data mover API, see **miniasync_vdm_threads**(7).

# DESCRIPTION #

The **data_mover_threads_get_vdm**() function reads the virtual data mover structure
from the thread data mover structure pointed by *dms*. Virtual data mover structure
*struct vdm* is needed by every **miniasync**(7) data mover operation.

Thread data mover implementation supports following operations:

* **vdm_memcpy**(3) - memory copy operation
* **vdm_memmove**(3) - memory move operation
* **vdm_memset**(3) - memory set operation

# RETURN VALUE #

The **data_mover_threads_get_vdm**() function returns a pointer to *struct vdm* structure.

# SEE ALSO #

**vdm_memcpy**(3), **vdm_memmove**(3), **vdm_memset**(3),
**miniasync**(7), **miniasync_vdm_threads**(7) and **<https://pmem.io>**
