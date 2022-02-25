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
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**miniasync_vdm** - Virtual data mover API for miniasync library


# SYNOPSIS #

```c
#include <libminiasync.h>

struct vdm *vdm_new(struct vdm_descriptor *descriptor);
void vdm_delete(struct vdm *vdm);
struct vdm_memcpy_future vdm_memcpy(struct vdm *vdm, void *dest, void *src, size_t n, uint64_t flags);
```

For general description of miniasync see **miniasync**(7).


# DESCRIPTION #

API for miniasync library forming the basis of various virtual data movers.

For more information about the usage of virtual data mover API, see *examples* directory
in miniasync repository <https://github.com/pmem/miniasync>.

**vdm_new**() creates a new instance of virtual data mover defined by the *descriptor* and performs
additional initialization if specified in the descriptor. For example,
creating instance of vdm with **vdm_descriptor_threads**(3) also calls a procedure that creates
necessary threads and data associated with them.

**vdm_delete**() cleans up all memory allocated by **vdm_new**() of the *vdm* instance and performs
additional finalization if specified in the descriptor of the vdm instance.

**vdm_memcpy**() initializes and returns a new memcpy future based on the virtual data mover
instance *vdm*. The parameters: *dest*, *src*, *n* are standard memcpy parameters. The *flags*
represents mover specific flags. For example, flags for dml mover that describe how the memcpy will
be performed by the mover.


# RETURN VALUE #

The **vdm_new** returns pointer to a *struct vdm* or **NULL** if the allocation of
*struct vdm* failed or the additional initialization failed.


# SEE ALSO #

**miniasync**(7), **miniasync_future**(7), **miniasync_runtime**(3), **miniasync_vdm_synchronous**(3),
**miniasync_vdm_threads**(3), **vdm_descriptor_threads**(3) and **<https://pmem.io>**
