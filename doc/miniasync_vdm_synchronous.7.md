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
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**miniasync_vdm_synchronous** - Synchronous virtual data mover for miniasync


# SYNOPSIS #

```c
#include <libminiasync.h>

struct vdm_descriptor *vdm_descriptor_synchronous(void);
```

For general description of miniasync, see **miniasync**(7).


# DESCRIPTION #

In order to use synchronous memcpy performed on the main thread, use following
descriptor:
```c
struct vdm_descriptor *vdm_descriptor_synchronous(void);
```
Then, create a new virtual data mover instance using this descriptor and a new
future:
```c
struct vdm *vdm = vdm_new(vdm_descriptor_synchronous);
struct vdm_memcpy_future fut = vdm_memcpy(vdm, dst, src, size, flags);
```

Now, when the future is polled for the first time in **runtime_wait**(3)
the memcpy operation will be performed synchronously on the same thread as
**runtime_wait**(3).

# RETURN VALUE #

The **vdm_descriptor_synchronous** returns a pointer to `struct vdm_descriptor`
describing synchronous virtual data mover.

# SEE ALSO #

**miniasync**(7), **miniasync_future**(3), **miniasync_runtime**(3),**miniasync_vdm**(3),
**runtime_wait**(3), **vdm_memcpy**(3), **vdm_new**(3) and **<https://pmem.io>**
