---
layout: manual
Content-Style: 'text/css'
title: _MP(VDM_FLUSH, 3)
collection: miniasync
header: VDM_FLUSH
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (vdm_flush.3 -- man page for miniasync vdm_flush operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**vdm_flush**() - create a new flush virtual data mover operation structure

# SYNOPSIS #

```c
#include <libminiasync.h>

struct vdm_operation_output_flush {
	void *dest;
};

FUTURE(vdm_operation_future,
	struct vdm_operation_data, struct vdm_operation_output);

struct vdm_operation_future vdm_flush(struct vdm *vdm, void *dest, size_t n, uint64_t flags);
```

For general description of virtual data mover API, see **miniasync_vdm**(7).

# DESCRIPTION #

**vdm_flush**() initializes and returns a new flush future based on the virtual data mover
implementation instance *vdm*. The parameters: *dest*, *n* are standard flush parameters.
The *flags* represents data mover specific flags. The **VDM_F_DONT_INVALIDATE_CACHE** specifies
that the affected cache lines should not be evicted from the caches during the flush operation,
as they are by default. The **flush** operation is implemented only for the DML data mover and
cannot be used with synchronous and thread data movers.

Flush future obtained using **vdm_flush**() will attempt to flush the *n* bytes of the processor
caches at the *dest* address when its polled.

## RETURN VALUE ##

The **vdm_flush**() function returns an initialized *struct vdm_operation_future* flush future.

# SEE ALSO #

**vdm_memcpy**(3), **vdm_memmove**(3), **vdm_memset**(3), **miniasync**(7), **miniasync_vdm**(7),
**miniasync_vdm_dml**(7) and **<https://pmem.io>**
