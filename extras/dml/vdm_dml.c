// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include <dml/dml.h>
#include <libminiasync.h>
#include <stdbool.h>
#include <stdlib.h>

#include "libminiasync-dml.h"
#include "core/out.h"
#include "core/util.h"

/*
 * vdm_dml_translate_flags -- translate miniasync-dml flags into dml flags
 */
static uint64_t
vdm_dml_translate_flags(uint64_t flags)
{
	ASSERTeq((flags & ~MINIASYNC_DML_F_MEM_VALID_FLAGS), 0);

	uint64_t tflags = 0;
	for (uint64_t iflag = 1; flags > 0; iflag = iflag << 1) {
		if ((flags & iflag) == 0)
			continue;

		switch (iflag) {
			case MINIASYNC_DML_F_MEM_DURABLE:
				tflags |= DML_FLAG_DST1_DURABLE;
				break;
			default: /* shouldn't be possible */
				ASSERT(0);
		}

		/* remove translated flag from the flags to be translated */
		flags = flags & (~iflag);
	}

	return tflags;
}

/*
 * vdm_dml_memcpy_job_new -- create a new memcpy job struct
 */
static dml_job_t *
vdm_dml_memcpy_job_new(void *dest, void *src, size_t n, uint64_t flags)
{
	dml_status_t status;
	uint32_t job_size;
	dml_job_t *dml_job = NULL;

	status = dml_get_job_size(DML_PATH_HW, &job_size);
	ASSERTeq(status, DML_STATUS_OK);

	dml_job = (dml_job_t *)malloc(job_size);

	status = dml_init_job(DML_PATH_HW, dml_job);
	ASSERTeq(status, DML_STATUS_OK);

	dml_job->operation = DML_OP_MEM_MOVE;
	dml_job->source_first_ptr = (uint8_t *)src;
	dml_job->destination_first_ptr = (uint8_t *)dest;
	dml_job->source_length = n;
	dml_job->destination_length = n;
	dml_job->flags = DML_FLAG_COPY_ONLY | vdm_dml_translate_flags(flags);

	return dml_job;
}

/*
 * vdm_dml_memcpy_job_delete -- delete job struct
 */
static void
vdm_dml_memcpy_job_delete(dml_job_t **dml_job)
{
	dml_finalize_job(*dml_job);
	free(*dml_job);
}

/*
 * vdm_dml_memcpy_job_execute -- execute memcpy job (blocking)
 */
static void *
vdm_dml_memcpy_job_execute(dml_job_t *dml_job)
{
	dml_status_t status;
	status = dml_execute_job(dml_job);
	ASSERTeq(status, DML_STATUS_OK);

	return dml_job->destination_first_ptr;
}

/*
 * vdm_dml_memcpy_job_submit -- submit memcpy job (nonblocking)
 */
static void *
vdm_dml_memcpy_job_submit(dml_job_t *dml_job)
{
	dml_status_t status;
	status = dml_submit_job(dml_job);
	ASSERTeq(status, DML_STATUS_OK);

	return dml_job->destination_first_ptr;
}

/*
 * vdm_dml_operation_new -- create a new DML job
 */
static int64_t
vdm_dml_operation_new(void *vdm_data, const struct vdm_operation *operation)
{
	struct vdm_operation *sync_op = malloc(sizeof(*sync_op));
	*sync_op = *operation;

	switch (operation->type) {
		case VDM_OPERATION_MEMCPY:
		return (int64_t)vdm_dml_memcpy_job_new(operation->memcpy.dest,
			operation->memcpy.src, operation->memcpy.n,
			operation->memcpy.flags);
		default:
		ASSERT(0); /* unreachable */
	}
	return 0;
}

/*
 * vdm_dml_operation_delete -- delete a DML job
 */
static void
vdm_dml_operation_delete(void *vdm_data, int64_t op_id)
{
	dml_job_t *job = (dml_job_t *)op_id;
	vdm_dml_memcpy_job_delete(&job);
}

/*
 * vdm_dml_operation_check -- check the status of a DML job
 */
enum future_state
vdm_dml_operation_check(void *vdm_data, int64_t op_id)
{
	dml_job_t *job = (dml_job_t *)op_id;

	dml_status_t status = dml_check_job(job);
	ASSERTne(status, DML_STATUS_JOB_CORRUPTED);

	return (status == DML_STATUS_OK) ?
		FUTURE_STATE_COMPLETE : FUTURE_STATE_RUNNING;
}

/*
 * vdm_dml_operation_start_sync -- start ('submit') asynchronous dml job
 */
int
vdm_dml_operation_start_async(void *vdm_data, int64_t op_id,
	struct future_notifier *n)
{
	n->notifier_used = FUTURE_NOTIFIER_NONE;

	dml_job_t *job = (dml_job_t *)op_id;

	vdm_dml_memcpy_job_submit(job);

	return 0;
}

/*
 * vdm_dml_operation_start_sync -- start ('execute') synchronous dml job
 */
int
vdm_dml_operation_start_sync(void *vdm_data, int64_t op_id,
	struct future_notifier *n)
{
	n->notifier_used = FUTURE_NOTIFIER_NONE;

	dml_job_t *job = (dml_job_t *)op_id;
	vdm_dml_memcpy_job_execute(job);

	return 0;
}

/*
 * dml_synchronous_descriptor -- dml synchronous memcpy descriptor
 */
static struct vdm_descriptor dml_synchronous_descriptor = {
	.vdm_data_init = NULL,
	.vdm_data_fini = NULL,
	.op_new = vdm_dml_operation_new,
	.op_delete = vdm_dml_operation_delete,
	.op_check = vdm_dml_operation_check,
	.op_start = vdm_dml_operation_start_sync,
};

/*
 * vdm_descriptor_dml -- return dml synchronous memcpy descriptor
 */
struct vdm_descriptor *
vdm_descriptor_dml(void)
{
	return &dml_synchronous_descriptor;
}

/*
 * dml_synchronous_descriptor -- dml asynchronous memcpy descriptor
 */
static struct vdm_descriptor dml_asynchronous_descriptor = {
	.vdm_data_init = NULL,
	.vdm_data_fini = NULL,
	.op_new = vdm_dml_operation_new,
	.op_delete = vdm_dml_operation_delete,
	.op_check = vdm_dml_operation_check,
	.op_start = vdm_dml_operation_start_async,
};

/*
 * vdm_descriptor_dml -- return dml asynchronous memcpy descriptor
 */
struct vdm_descriptor *
vdm_descriptor_dml_async(void)
{
	return &dml_asynchronous_descriptor;
}
