#include <assert.h>
#include <dml/dml.h>
#include <libminiasync.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>

#include "libminiasync-dml/dml_mover.h"

static dml_job_t *
dml_memcpy_job_new(void *dest, void *src, size_t n)
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
	dml_job->flags = DML_FLAG_COPY_ONLY;

	return dml_job;
}

static void
dml_memcpy_job_delete(dml_job_t **dml_job)
{
	dml_finalize_job(*dml_job);
	free(*dml_job);
}

static void *
dml_memcopy_execute(dml_job_t *dml_job)
{
	dml_status_t status;
	status = dml_execute_job(dml_job);
	assert(status == DML_STATUS_OK);

	return dml_job->destination_first_ptr;
}

static void *
dml_memcopy_submit(dml_job_t *dml_job)
{
	dml_status_t status;
	status = dml_submit_job(dml_job);
	assert(status == DML_STATUS_OK);

	return dml_job->destination_first_ptr;
}

static enum future_state
dml_check(struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);

	return atomic_load(&data->complete) ? FUTURE_STATE_COMPLETE
					    : FUTURE_STATE_RUNNING;
}

static enum future_state
dml_check_job_finalize(struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	dml_job_t *dml_job = (dml_job_t *)data->extra;

	dml_status_t status = dml_check_job(dml_job);
	assert(status != DML_STATUS_JOB_CORRUPTED);

	enum future_state state = (status == DML_STATUS_OK) ?
			FUTURE_STATE_COMPLETE : FUTURE_STATE_RUNNING;

	if (state == FUTURE_STATE_COMPLETE)
		dml_finalize_job(dml_job);

	return state;
}

static void
dml_memcopy_sync(void *runner, struct future_notifier *notifier,
	struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	struct vdm_memcpy_output *output = future_context_get_output(context);
	struct vdm_descriptor *dsc = (struct vdm_descriptor *)runner;

	dml_job_t *dml_job = dml_memcpy_job_new(data->dest, data->src, data->n);
	output->dest = dml_memcopy_execute(dml_job);
	dml_memcpy_job_delete(&dml_job);
	data->vdm_cb(context);
}

static struct vdm_descriptor dml_synchronous_descriptor = {
	.vdm_data = NULL,
	.memcpy = dml_memcopy_sync,
	.check = dml_check,
};

struct vdm_descriptor *
vdm_descriptor_dml(void)
{
	return &dml_synchronous_descriptor;
}

static void
dml_memcopy_async(void *runner, struct future_notifier *notifier,
	struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	struct vdm_memcpy_output *output = future_context_get_output(context);
	struct vdm_descriptor *dsc = (struct vdm_descriptor *)runner;

	dml_job_t *dml_job = dml_memcpy_job_new(data->dest, data->src, data->n);
	data->extra = dml_job;
	output->dest = dml_memcopy_submit(dml_job);
}

static struct vdm_descriptor dml_asynchronous_descriptor = {
	.vdm_data = NULL,
	.memcpy = dml_memcopy_async,
	.check = dml_check_job_finalize,
};

struct vdm_descriptor *
vdm_descriptor_dml_async(void)
{
	return &dml_asynchronous_descriptor;
}
