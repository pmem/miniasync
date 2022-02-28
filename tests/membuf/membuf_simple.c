// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "core/membuf.h"

#define TEST_USER_DATA (void *)(0xC0FFEE)

#define TEST_ENTRY_PADDING (1 << 11)
#define MAX_TEST_ENTRIES (100000)

struct test_entry {
	char padding[TEST_ENTRY_PADDING];
};

int
main(int argc, char *argv[])
{
	struct membuf *mbuf = membuf_new(TEST_USER_DATA);

	struct test_entry **entries =
		malloc(sizeof(struct test_entry *) * MAX_TEST_ENTRIES);
	assert(entries != NULL);

	int i;
	for (i = 0; i < MAX_TEST_ENTRIES; ++i) {
		struct test_entry *entry =
			membuf_alloc(mbuf, sizeof(struct test_entry));
		if (entry == NULL)
			break;
		assert(membuf_ptr_user_data(entry) == TEST_USER_DATA);

		entries[i] = entry;
	}

	/* if this triggers, increase MAX_TEST_ENTRIES */
	assert(i != MAX_TEST_ENTRIES);

	int entries_max = i;

	for (i = 0; i < entries_max / 2; ++i) {
		struct test_entry *entry = entries[i];
		membuf_free(entry);
	}

	int allocated = 0;
	for (i = 0; i < MAX_TEST_ENTRIES; ++i) {
		struct test_entry *entry =
			membuf_alloc(mbuf, sizeof(struct test_entry));
		if (entry == NULL)
			break;
		assert(membuf_ptr_user_data(entry) == TEST_USER_DATA);
	}
	allocated += i;
	assert(i == entries_max / 2);

	for (i = entries_max / 2; i < entries_max; ++i) {
		struct test_entry *entry = entries[i];
		membuf_free(entry);
	}

	for (i = 0; i < MAX_TEST_ENTRIES; ++i) {
		struct test_entry *entry =
			membuf_alloc(mbuf, sizeof(struct test_entry));
		if (entry == NULL)
			break;
		assert(membuf_ptr_user_data(entry) == TEST_USER_DATA);
	}
	allocated += i;
	assert(allocated == entries_max);

	membuf_delete(mbuf);
	free(entries);

	return 0;
}
