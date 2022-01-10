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
 * vdm_dml_translate_flags -- translate miniasync-dml flags
 */
static void
vdm_dml_translate_flags(uint64_t flags, uint64_t *dml_flags, dml_path_t *path)
{
	ASSERTeq((flags & ~MINIASYNC_DML_F_VALID_FLAGS), 0);

	*dml_flags = 0;
	*path = DML_PATH_SW; /* default path */
	for (uint64_t iflag = 1; flags > 0; iflag = iflag << 1) {
		if ((flags & iflag) == 0)
			continue;

		switch (iflag) {
			case MINIASYNC_DML_F_MEM_DURABLE:
				*dml_flags |= DML_FLAG_DST1_DURABLE;
				break;
			case MINIASYNC_DML_F_PATH_HW:
				*path = DML_PATH_HW;
				break;
			default: /* shouldn't be possible */
				ASSERT(0);
		}

		/* remove translated flag from the flags to be translated */
		flags = flags & (~iflag);
	}
}

/*
 * vdm_dml_memcpy_job_new -- create a new memcpy job struct
 */
static dml_job_t *
vdm_dml_memcpy_job_new(void *dest, void *src, size_t n, uint64_t flags,
		dml_path_t path)
{
	dml_status_t status;
	uint32_t job_size;
	dml_job_t *dml_job = NULL;

	status = dml_get_job_size(path, &job_size);
	ASSERTeq(status, DML_STATUS_OK);

	dml_job = (dml_job_t *)malloc(job_size);

	status = dml_init_job(path, dml_job);
	ASSERTeq(status, DML_STATUS_OK);

	dml_job->operation = DML_OP_MEM_MOVE;
	dml_job->source_first_ptr = (uint8_t *)src;
	dml_job->destination_first_ptr = (uint8_t *)dest;
	dml_job->source_length = n;
	dml_job->destination_length = n;
	dml_job->flags = DML_FLAG_COPY_ONLY | flags;

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
 * vdm_dml_check_delete_job -- check status of memcpy job
 */
static enum future_state
vdm_dml_check_delete_job(struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	dml_job_t *dml_job = (dml_job_t *)data->extra;

	dml_status_t status = dml_check_job(dml_job);
	ASSERTne(status, DML_STATUS_JOB_CORRUPTED);

	enum future_state state = (status == DML_STATUS_OK) ?
			FUTURE_STATE_COMPLETE : FUTURE_STATE_RUNNING;

	if (state == FUTURE_STATE_COMPLETE)
		vdm_dml_memcpy_job_delete(&dml_job);

	return state;
}

/*
 * vdm_dml_memcpy -- execute dml memcpy operation
 */
static void
vdm_dml_memcpy(void *runner, struct future_notifier *notifier,
	struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	struct vdm_memcpy_output *output = future_context_get_output(context);

	uint64_t tflags = 0;
	dml_path_t path = 0;
	vdm_dml_translate_flags(data->flags, &tflags, &path);
	dml_job_t *dml_job = vdm_dml_memcpy_job_new(data->dest, data->src,
			data->n, tflags, path);
	data->extra = dml_job;
	output->dest = vdm_dml_memcpy_job_submit(dml_job);
}

/*
 * dml_synchronous_descriptor -- dml memcpy descriptor
 */
static struct vdm_descriptor dml_descriptor = {
	.vdm_data_init = NULL,
	.vdm_data_fini = NULL,
	.memcpy = vdm_dml_memcpy,
	.check = vdm_dml_check_delete_job,
};

/*
 * vdm_descriptor_dml -- return dml memcpy descriptor
 */
struct vdm_descriptor *
vdm_descriptor_dml(void)
{
	return &dml_descriptor;
}
