/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021-2022, Intel Corporation */

#ifndef VDM_DML_H
#define VDM_DML_H 1

#include <dml/dml.h>
#include <libminiasync.h>

#ifdef __cplusplus
extern "C" {
#endif

struct vdm *vdm_dml_new();
void vdm_dml_delete(struct vdm *vdm);

#ifdef __cplusplus
}
#endif
#endif /* VDM_DML_H */
