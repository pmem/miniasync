---
layout: manual
Content-Style: 'text/css'
title: _MP(DATA_MOVER_DML_GET_VDM, 3)
collection: miniasync
header: DATA_MOVER_DML_GET_VDM
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (data_mover_dml_get_vdm.3 -- man page for miniasync data_mover_dml_get_vdm operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**data_mover_dml_get_vdm**() - get virtual data mover structure from the **DML**
data mover structure


# SYNOPSIS #

```c
#include <libminiasync.h>

struct vdm {
	vdm_operation_new op_new;
	vdm_operation_delete op_delete;
	vdm_operation_start op_start;
	vdm_operation_check op_check;
};

struct data_mover_dml;

struct vdm *data_mover_dml_get_vdm(struct data_mover_dml *dmd);
```

For general description of **DML** data mover API, see **miniasync_vdm_dml**(7).


# DESCRIPTION #

The **data_mover_dml_get_vdm**() function reads the virtual data mover structure
from the **DML** data mover structure pointed by *dmd*. Virtual data mover
structure *struct vdm* is needed by every **miniasync**(7) data mover operation.

**DML** data mover implementation supports following operations:

* **vdm_memcpy**(3) - memory copy operation


# RETURN VALUE #

The **data_mover_dml_get_vdm**() function returns a pointer to *struct vdm* structure.


# SEE ALSO #

**vdm_memcpy**(3), **miniasync**(7),
**miniasync_vdm_dml**(7) and **<https://pmem.io>**
