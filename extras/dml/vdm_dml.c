// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include <assert.h>
#include <dml/dml.h>
#include <libminiasync.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>

#include "libminiasync-dml.h"

static uint64_t
vdm_dml_translate_flags(uint64_t flags)
{
	assert((flags & ~MINIASYNC_DML_F_MEM_VALID_FLAGS) == 0);

	uint64_t tflags = 0;
	for (uint64_t iflag = 1; flags > 0; iflag << 1) {
		if (flags & iflag == 0)
			continue;

		switch (iflag) {
			case MINIASYNC_DML_F_MEM_DURABLE:
				tflags |= DML_FLAG_DST1_DURABLE;
				break;
			default: /* shouldn't be possible */
				assert(0);
		}

		/* remove translated flag from the flags to be translated */
		flags = flags & (~iflag);
	}

	return tflags;
}

static dml_job_t *
vdm_dml_memcpy_job_new(void *dest, void *src, size_t n, uint64_t flags)
{
	dml_status_t status;
	uint32_t job_size;
	dml_job_t *dml_job = NULL;

	status = dml_get_job_size(DML_PATH_HW, &job_size);
	assert(status == DML_STATUS_OK);

	dml_job = (dml_job_t *)malloc(job_size);

	status = dml_init_job(DML_PATH_HW, dml_job);
	assert(status == DML_STATUS_OK);

	dml_job->operation = DML_OP_MEM_MOVE;
	dml_job->source_first_ptr = (uint8_t *)src;
	dml_job->destination_first_ptr = (uint8_t *)dest;
	dml_job->source_length = n;
	dml_job->destination_length = n;
	dml_job->flags = DML_FLAG_COPY_ONLY | flags;

	return dml_job;
}

static void
vdm_dml_memcpy_job_delete(dml_job_t **dml_job)
{
	dml_finalize_job(*dml_job);
	free(*dml_job);
}

static void *
vdm_dml_memcpy_job_execute(dml_job_t *dml_job)
{
	dml_status_t status;
	status = dml_execute_job(dml_job);
	assert(status == DML_STATUS_OK);

	return dml_job->destination_first_ptr;
}

static void *
vdm_dml_memcpy_job_submit(dml_job_t *dml_job)
{
	dml_status_t status;
	status = dml_submit_job(dml_job);
	assert(status == DML_STATUS_OK);

	return dml_job->destination_first_ptr;
}

static enum future_state
vdm_dml_check(struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);

	return atomic_load(&data->complete) ? FUTURE_STATE_COMPLETE
					    : FUTURE_STATE_RUNNING;
}

static enum future_state
vdm_dml_check_delete_job(struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	dml_job_t *dml_job = (dml_job_t *)data->extra;

	dml_status_t status = dml_check_job(dml_job);
	assert(status != DML_STATUS_JOB_CORRUPTED);

	enum future_state state = (status == DML_STATUS_OK) ?
			FUTURE_STATE_COMPLETE : FUTURE_STATE_RUNNING;

	if (state == FUTURE_STATE_COMPLETE)
		vdm_dml_memcpy_job_delete(&dml_job);

	return state;
}

static void
vdm_dml_memcpy_sync(void *runner, struct future_notifier *notifier,
	struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	struct vdm_memcpy_output *output = future_context_get_output(context);
	struct vdm_descriptor *dsc = (struct vdm_descriptor *)runner;

	uint64_t tflags = vdm_dml_translate_flags(data->flags);
	dml_job_t *dml_job = vdm_dml_memcpy_job_new(data->dest, data->src,
			data->n, tflags);
	output->dest = vdm_dml_memcpy_job_execute(dml_job);
	vdm_dml_memcpy_job_delete(&dml_job);
	data->vdm_cb(context);
}

static struct vdm_descriptor dml_synchronous_descriptor = {
	.vdm_data = NULL,
	.memcpy = vdm_dml_memcpy_sync,
	.check = vdm_dml_check,
};

struct vdm_descriptor *
vdm_descriptor_dml(void)
{
	return &dml_synchronous_descriptor;
}

static void
vdm_dml_memcpy_async(void *runner, struct future_notifier *notifier,
	struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	struct vdm_memcpy_output *output = future_context_get_output(context);
	struct vdm_descriptor *dsc = (struct vdm_descriptor *)runner;

	uint64_t tflags = vdm_dml_translate_flags(data->flags);
	dml_job_t *dml_job = vdm_dml_memcpy_job_new(data->dest, data->src,
			data->n, tflags);
	data->extra = dml_job;
	output->dest = vdm_dml_memcpy_job_submit(dml_job);
}

static struct vdm_descriptor dml_asynchronous_descriptor = {
	.vdm_data = NULL,
	.memcpy = vdm_dml_memcpy_async,
	.check = vdm_dml_check_delete_job,
};

struct vdm_descriptor *
vdm_descriptor_dml_async(void)
{
	return &dml_asynchronous_descriptor;
}
