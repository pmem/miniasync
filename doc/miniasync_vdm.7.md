---
layout: manual
Content-Style: 'text/css'
title: _MP(MINIASYNC_VDM, 7)
collection: miniasync
header: MINIASYNC_VDM
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (miniasync_vdm.7 -- man page for miniasync vdm API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**miniasync_vdm** - virtual data mover API for miniasync library

# SYNOPSIS #

```c
#include <libminiasync.h>

enum vdm_operation_type {
	VDM_OPERATION_MEMCPY,
};
```

For general description of miniasync, see **miniasync**(7).

# DESCRIPTION #

Virtual data mover API forms the basis of various concrete data movers.
It is an abstraction that the concrete data mover implementations should adapt
for compatibility with the **miniasync_future**(7) feature.

Currently, virtual data mover API supports following operation types:

* **VDM_OPERATION_MEMCPY** - a memory copy operation

For more information about concrete data mover implementations, see **miniasync_vdm_threads**(7),
**miniasync_vdm_synchronous**(7) and **miniasync_vdm_dml**(7).

For more information about the usage of virtual data mover API, see *examples* directory
in miniasync repository <https://github.com/pmem/miniasync>.

# SEE ALSO #

**miniasync**(7), **miniasync_future**(7),
**miniasync_vdm_dml**(7), **miniasync_vdm_synchronous**(7),
**miniasync_vdm_threads**(7) and **<https://pmem.io>**
