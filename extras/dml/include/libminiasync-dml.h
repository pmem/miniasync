/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021, Intel Corporation */

/*
 * libminiasync-dml.h -- definition of miniasync dml implementation
 */

#ifndef MINIASYNC_DML_H
#define MINIASYNC_DML_H 1

#include <stddef.h>
#include <stdio.h>

#include "libminiasync-dml/vdm_dml.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MINIASYNC_DML memory flags */
#define MINIASYNC_DML_F_MEM_DURABLE 		(1U << 0)
#define MINIASYNC_DML_F_MEM_VALID_FLAGS	(MINIASYNC_DML_F_MEM_DURABLE)

#ifdef __cplusplus
}
#endif

#endif /* MINIASYNC_DML_H */
