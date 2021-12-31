// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "libminiasync.h"

int
test_threads_memcpy_multiple_same_char(unsigned memcpy_count,
	unsigned n, size_t test_size)
{
	struct runtime *r = runtime_new();
	struct vdm_descriptor *vdm_async_descriptor = vdm_descriptor_threads();
	struct vdm *vdm = vdm_new(vdm_async_descriptor);

	if (!vdm) {
		fprintf(stderr, "Failed to create VDM\n");
		return 1;
	}

	char **sources = malloc(memcpy_count * sizeof(char *) * n);
	char **destinations = malloc(memcpy_count * sizeof(char *) * n);
	char *values = malloc(memcpy_count * sizeof(char) * n);
	size_t *sizes = malloc(memcpy_count * sizeof(size_t) * n);

	struct future **futures = malloc(
		memcpy_count * sizeof(struct future *) * n);
	struct vdm_memcpy_future *memcpy_futures =
		malloc(memcpy_count * sizeof(struct vdm_memcpy_future) * n);

	unsigned index = 0;

	for (unsigned iter = 0; iter < n; iter++) {
		for (unsigned i = index; i < index + memcpy_count; i++) {
			values[i] = (char)((i % 26) + 'A');
			if (test_size) {
				sizes[i] = test_size;
			} else {
				sizes[i] = (size_t)rand() % (1 << 20) + 1;
			}
			sources[i] = malloc(sizes[i] * sizeof(char));
			destinations[i] = malloc(sizes[i] * sizeof(char));
			memset(sources[i], values[i], sizes[i]);
			memcpy_futures[i] = vdm_memcpy(vdm, destinations[i],
				sources[i], sizes[i], 0);
			futures[i] =
				FUTURE_AS_RUNNABLE(&memcpy_futures[i]);
		}

		runtime_wait_multiple(r, futures + iter * memcpy_count,
			memcpy_count);
		index += memcpy_count;
	}

	/* Verification */

	for (unsigned i = 0; i < memcpy_count * n; i++) {
		if (memcmp(sources[i], destinations[i], sizes[i]) !=
			0) {
			fprintf(stderr,
				"Memcpy nr. %u result is wrong! "
				"Returning\n", i);
			return 1;
		}
		printf("Memcpy nr. %u from [%p] to [%p] n=%lu "
			"content='%c' is correct\n", i, sources[i],
			destinations[i], sizes[i], values[i]);
	}

	/* Cleanup */

	for (unsigned i = 0; i < memcpy_count * n; i++) {
		free(sources[i]);
		free(destinations[i]);
	}
	free(sources);
	free(destinations);
	free(values);
	free(sizes);
	free(futures);
	free(memcpy_futures);

	runtime_delete(r);
	vdm_delete(vdm);
	return 0;
}

int
test_threads_memcpy_multiple_sequence(unsigned memcpy_count,
	unsigned n, size_t test_size)
{
	struct runtime *r = runtime_new();
	struct vdm_descriptor *vdm_async_descriptor = vdm_descriptor_threads();
	struct vdm *vdm = vdm_new(vdm_async_descriptor);

	char **sources = malloc(memcpy_count * sizeof(char *) * n);
	char **destinations = malloc(memcpy_count * sizeof(char *) * n);
	size_t *sizes = malloc(memcpy_count * sizeof(size_t) * n);

	struct future **futures = malloc(
		memcpy_count * sizeof(struct future *) * n);
	struct vdm_memcpy_future *memcpy_futures =
		malloc(memcpy_count * sizeof(struct vdm_memcpy_future) * n);

	unsigned index = 0;

	for (unsigned iter = 0; iter < n; iter++) {
		for (unsigned i = index; i < index + memcpy_count; i++) {
			if (test_size) {
				sizes[i] = test_size;
			} else {
				sizes[i] = (size_t)rand() % (1 << 20) + 1;
			}
			sources[i] = malloc(sizes[i] * sizeof(char));
			destinations[i] = malloc(sizes[i] * sizeof(char));

			char value = 0;
			for (unsigned j = 0; j < sizes[i]; j++) {
				sources[i][j] = value++;
			}

			memcpy_futures[i] = vdm_memcpy(vdm, destinations[i],
				sources[i], sizes[i], 0);
			futures[i] =
				FUTURE_AS_RUNNABLE(&memcpy_futures[i]);
		}

		runtime_wait_multiple(r, futures + iter * memcpy_count,
			memcpy_count);
		index += memcpy_count;
	}

	/* Verification */

	for (unsigned i = 0; i < memcpy_count * n; i++) {
		if (memcmp(sources[i], destinations[i], sizes[i]) !=
			0) {
			fprintf(stderr,
				"Memcpy nr. %u result is wrong! "
				"Returning\n", i);
			return 1;
		}
		printf("Memcpy nr. %u from [%p] to [%p] n=%lu "
			"content=sequence is correct\n", i, sources[i],
			destinations[i], sizes[i]);
	}

	/* Cleanup */

	for (unsigned i = 0; i < memcpy_count * n; i++) {
		free(sources[i]);
		free(destinations[i]);
	}
	free(sources);
	free(destinations);
	free(sizes);
	free(futures);
	free(memcpy_futures);

	runtime_delete(r);
	vdm_delete(vdm);
	return 0;
}

int
main(int argc, char *argv[])
{
	srand(time(NULL));
	return
		test_threads_memcpy_multiple_same_char(100, 10, 10) ||
		test_threads_memcpy_multiple_same_char(100, 10, 1 << 10) ||
		test_threads_memcpy_multiple_sequence(100, 10, 128) ||
		test_threads_memcpy_multiple_sequence(100, 10, 7) ||
		test_threads_memcpy_multiple_sequence(100, 10, 0);
}
