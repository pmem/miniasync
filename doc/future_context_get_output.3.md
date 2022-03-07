---
layout: manual
Content-Style: 'text/css'
title: _MP(FUTURE_CONTEXT_GET_OUTPUT, 3)
collection: miniasync
header: FUTURE_CONTEXT_GET_OUTPUT
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (future_context_get_output.3 -- man page for miniasync future_context_get_output operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**future_context_get_output**() - get future output from future context

# SYNOPSIS #

```c
#include <libminiasync.h>

struct future_context;
void *future_context_get_output(struct future_context *context);
```

For general description of future API, see **miniasync_future**(7).

# DESCRIPTION #

The **future_context_get_output**() function reads the output from future context *context*.
Output type corresponds to the *\_output_type* parameter that is provided to the
**FUTURE(_name, _data_type, _output_type)** macro during future creation.

For more information about the usage of **future_context_get_output**() function, see *basic*
example in *example* directory in miniasync repository <https://github.com/pmem/miniasync>.

## RETURN VALUE ##

The **future_context_get_output**() function returns pointer to the future output.

# SEE ALSO #

**miniasync**(7), **miniasync_future**(7) and **<https://pmem.io>**
