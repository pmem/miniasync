---
layout: manual
Content-Style: 'text/css'
title: _MP(MINIASYNC_VDM_DML, 3)
collection: miniasync_dml
header: MINIASYNC_VDM_DML
secondary_title: miniasync_dml
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (miniasync_vdm_dml.3 -- man page for miniasync-dml vdm API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[EXAMPLE](#example)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**miniasync_vdm_dml** - **DML** implementation of **miniasync**(7) virtual data mover


# SYNOPSIS #

```c
#include <libminiasync.h>
#include <libminiasync-dml.h>
```

For general description of virtual data mover API, see **miniasync**(7).


# DESCRIPTION #

The **vdm_descriptor_dml**() is **miniasync**(7) virtual data mover
implementation of Data Mover Library (**DML**).

`struct vdm_descriptor *vdm_descriptor_dml(void);`

:	Data Mover Library implementation of virtual data mover. It is a non-blocking copy operation.
	By default, this operation executes using software path. Set *MINIASYNC_DML_F_PATH_HW* flag to
	use hardware path. Hardware path enables use of hardware accelerators (e.g. Intel Data Streaming Accelerator)
	for certain computations.
	If you want to make use of hardware path, make sure that DML is installed with ```DML_HW``` option.
	For more information about **DML**, see **<https://github.com/intel/DML>**.

XXX: add description how to use flags

For more information about **miniasync-dml** compilation options, see *extras/dml/README.md* file
in miniasync repository <https://github.com/pmem/miniasync>.

**DML** data mover is an implementation of the virtual data mover based on
the *Data Mover Library*(**DML**). Every **DML** data mover operation
executes under the control of **DML**.

When the future is polled for the first time the data mover operation will be executed
asynchronously under the control of **DML** library. **DML** data mover does not
block the calling thread

To create a new **DML** data mover instance, use **data_mover_dml_new**(3) function.

**DML** data mover provides the following flags:

* **MINIASYNC_DML_F_MEM_DURABLE** - write to destination is identified as write to durable memory

* **MINIASYNC_DML_F_PATH_HW** - operation execution is offloaded to the hardware accelerator

**DML** data mover supports following operations:

* **vdm_memcpy**(3) - memory copy operation

**DML** data mover does not support notifier feature.

For more information about **DML**, see **<https://github.com/intel/DML>**.


# EXAMPLE #

Example usage of **vdm_descriptor_dml**() function with **MINIASYNC_DML_F_MEM_DURABLE** flag:
```c
struct vdm *dml_mover = vdm_new(vdm_descriptor_dml());
struct vdm_memcpy_future memcpy_fut =
		vdm_memcpy(dml_mover, dest, src, copy_size, MINIASYNC_DML_F_MEM_DURABLE);
```


## ERRORS ##

**vdm_descriptor_dml**() function does not return any errors.


# SEE ALSO #

**miniasync**(7), **miniasync_vdm**(3), **vdm_memcpy**(3), **vdm_new**(3),
**<https://github.com/intel/DML>** and **<https://pmem.io>**
