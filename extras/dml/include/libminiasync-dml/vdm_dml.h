/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021-2022, Intel Corporation */

#ifndef VDM_DML_H
#define VDM_DML_H 1

#include <dml/dml.h>
#include <libminiasync.h>

#ifdef __cplusplus
extern "C" {
#endif

struct vdm_descriptor *vdm_descriptor_dml_async(void);

#ifdef __cplusplus
}
#endif
#endif /* VDM_DML_H */
