// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "libminiasync.h"
#include "libminiasync/data_mover_threads.h"

#define HASHMAP_VALUE_SIZE 32
#define WAIT_FUTURES_MAX 4

/* Polls 'nfuts' number of futures until they complete, makes us of runtime */
#define WAIT_FUTURES(runtimep, futsp, nfuts)\
do {\
	struct future *tmp_futs[WAIT_FUTURES_MAX];\
	for (int i = 0; i < nfuts; i++) {\
		tmp_futs[i] = FUTURE_AS_RUNNABLE(&(futsp[i]));\
	}\
\
	runtime_wait_multiple(r, tmp_futs, nfuts);\
} while (0)

struct hashmap_entry {
	uint64_t key;
	char value[HASHMAP_VALUE_SIZE];
	unsigned deleted;
};

struct hashmap {
	struct hashmap_entry *entries;
	size_t capacity; /* max stored entries */
	size_t length; /* number of stored entries */
	unsigned lock; /* used for hashmap operations synchronization */
};

static inline int
is_power_of_2(uint64_t val)
{
	return val && !(val & (val - 1));
}

/* Checks if entry was deleted */
static inline int
hashmap_entry_is_deleted(struct hashmap_entry *hme)
{
	return hme->deleted != 0;
}

/* Checks if hashmap is empty */
static inline int
hashmap_entry_is_empty(struct hashmap_entry *hme)
{
	return hme->key == 0 || hashmap_entry_is_deleted(hme);
}

/* Creates a new hashmap instance */
static struct hashmap *
hashmap_new(size_t capacity)
{
	if (!is_power_of_2(capacity)) {
		fprintf(stderr, "hashmap capacity has to be a power of 2\n");
		return NULL;
	}

	struct hashmap *hm = malloc(sizeof(struct hashmap) * capacity);
	if (hm == NULL) {
		return NULL;
	}

	/* allocate zero'd space */
	hm->entries = calloc(capacity, sizeof(struct hashmap_entry));
	if (hm->entries == NULL) {
		goto free_hm;
	}

	hm->capacity = capacity;
	hm->length = 0;

	return hm;

free_hm:
	free(hm);
	return NULL;
}

/* Deletes a hashmap instance */
void
hashmap_delete(struct hashmap *hm)
{
	free(hm->entries);
	free(hm);
}

/* Hash function based on Austin Appleby MurmurHash3 64-bit finalizer */
static size_t
hash_val(uint64_t val)
{
	val ^= val >> 33;
	val *= 0xff51afd7ed558ccd;
	val ^= val >> 33;
	val *= 0xc4ceb9fe1a85ec53;
	val ^= val >> 33;

	return val;
}

/* Calculates the index based on provided key and hashmap capacity */
static int
hashmap_key_index(struct hashmap *hm, uint64_t key)
{
	return (int)(hash_val(key) & (hm->capacity - 1));
}

/* Looks for index of the entry with provided key */
static int
hashmap_lookup(struct hashmap *hm, uint64_t key)
{
	int index = hashmap_key_index(hm, key);

	for (size_t i = 0; i < hm->capacity; i++,
			index = (index + 1) % (int)hm->capacity) {
		if (hashmap_entry_is_deleted(&hm->entries[index])) {
			continue;
		} else if (hm->entries[index].key == 0) {
			break;
		}

		if (hm->entries[index].key == key) {
			return index;
		}
	}

	return -1;
}

/* Acquires the hashmap lock */
static unsigned
hashmap_lock_acquire_impl(struct hashmap *hm)
{
	return __sync_bool_compare_and_swap(&hm->lock, 0, 1);
}

/* Releases the hashmap lock */
static void
hashmap_lock_release_impl(struct hashmap *hm)
{
	/* lock should be releaased only if it was previously acquired */
	assert(__sync_bool_compare_and_swap(&hm->lock, 1, 0));
}

/*
 * BEGIN of hashmap_lock_acquire_fut
 */
struct hashmap_lock_acquire_data {
	struct hashmap *hm;
};

struct hashmap_lock_acquire_output {
	uintptr_t foo; /* avoid empty struct warning */
};

FUTURE(hashmap_lock_acquire_fut, struct hashmap_lock_acquire_data,
		struct hashmap_lock_acquire_output);

static enum future_state
hashmap_lock_acquire_wrapped(struct future_context *ctx,
		struct future_notifier *notifier)
{
	struct hashmap_lock_acquire_data *data = future_context_get_data(ctx);

	unsigned acquired = hashmap_lock_acquire_impl(data->hm);
	return (acquired) ? FUTURE_STATE_COMPLETE : FUTURE_STATE_RUNNING;
}

static struct hashmap_lock_acquire_fut
hashmap_lock_acquire(struct hashmap *hm)
{
	struct hashmap_lock_acquire_fut future = {0};
	future.data.hm = hm;

	FUTURE_INIT(&future, hashmap_lock_acquire_wrapped);

	return future;
}
/*
 * END of hashmap_lock_acquire_fut
 */

/*
 * BEGIN of hashmap_lock_release_fut
 */
struct hashmap_lock_release_data {
	struct hashmap *hm;
};

struct hashmap_lock_release_output {
	char *return_value; /* XXX: Find different way to pass the output */
};

FUTURE(hashmap_lock_release_fut, struct hashmap_lock_release_data,
		struct hashmap_lock_release_output);

static enum future_state
hashmap_lock_release_wrapped(struct future_context *ctx,
		struct future_notifier *notifier)
{
	struct hashmap_lock_release_data *data = future_context_get_data(ctx);

	hashmap_lock_release_impl(data->hm);
	return FUTURE_STATE_COMPLETE;
}

static struct hashmap_lock_release_fut
hashmap_lock_release(struct hashmap *hm)
{
	struct hashmap_lock_release_fut future = {0};
	future.data.hm = hm;

	FUTURE_INIT(&future, hashmap_lock_release_wrapped);

	return future;
}
/*
 * END of hashmap_lock_release_fut
 */

/*
 * BEGIN of hashmap_get_fut future
 */
struct hashmap_get_data {
	struct hashmap *hm;
	uint64_t key;
};

struct hashmap_get_output {
	char *value;
};

FUTURE(hashmap_get_fut, struct hashmap_get_data, struct hashmap_get_output);

static enum future_state
hashmap_get_impl(struct future_context *ctx, struct future_notifier *notifier)
{
	struct hashmap_get_data *data = future_context_get_data(ctx);
	struct hashmap_get_output *output = future_context_get_output(ctx);
	struct hashmap *hm = data->hm;
	uint64_t key = data->key;

	char *out_value;
	if (key == 0) {
		fprintf(stderr, "invalid key\n");
		out_value = NULL;
		goto set_output;
	}

	int index = hashmap_lookup(hm, key);
	out_value = (index >= 0) ? hm->entries[index].value : NULL;

set_output:
	output->value = out_value;
	return FUTURE_STATE_COMPLETE;
}

/*
 * hashmap_get -- gets a value from the hashmap
 */
static struct hashmap_get_fut
hashmap_get(struct hashmap *hm, uint64_t key)
{
	struct hashmap_get_fut future = {0};
	future.data.hm = hm;
	future.data.key = key;

	FUTURE_INIT(&future, hashmap_get_impl);

	return future;
}
/*
 * END of hashmap_get_fut future
 */

/*
 * BEGIN of hashmap_put_key_fut future
 */
struct hashmap_put_key_data {
	struct hashmap *hm;
	uint64_t key;
	char *value;
};

struct hashmap_put_key_output {
	int index;
};

FUTURE(hashmap_put_key_fut, struct hashmap_put_key_data,
		struct hashmap_put_key_output);

static enum future_state
hashmap_put_key_impl(struct future_context *ctx,
		struct future_notifier *notifier)
{
	struct hashmap_put_key_data *data = future_context_get_data(ctx);
	struct hashmap_put_key_output *output = future_context_get_output(ctx);
	struct hashmap *hm = data->hm;
	uint64_t key = data->key;
	char *value = data->value;

	int index = -1;
	if (key == 0) {
		fprintf(stderr, "invalid key %" PRIu64 "\n", key);
		goto set_output;
	} else if (value == NULL) {
		fprintf(stderr, "invalid, NULL value\n");
		goto set_output;
	} else if (hashmap_lookup(hm, key) != -1) {
		fprintf(stderr, "key %" PRIu64 " already exists\n", key);
		goto set_output;
	} else if (hm->capacity == hm->length) {
		fprintf(stderr, "no space left for key %" PRIu64 "\n", key);
		goto set_output;
	}

	index = hashmap_key_index(hm, key);
	while (!hashmap_entry_is_empty(&hm->entries[index])) {
		index = (index + 1) % (int)hm->capacity;
	}

	hm->entries[index].key = key;
	hm->entries[index].deleted = 0;
	hm->length++;

set_output:
	output->index = index;
	return FUTURE_STATE_COMPLETE;
}

static struct hashmap_put_key_fut
hashmap_put_key(struct hashmap *hm, uint64_t key, char *value)
{
	struct hashmap_put_key_fut future = {0};
	future.data.hm = hm;
	future.data.key = key;
	future.data.value = value;

	FUTURE_INIT(&future, hashmap_put_key_impl);

	return future;
}
/*
 * END of hashmap_put_key_fut future
 */

/*
 * BEGIN of hashmap_put_fut future
 */
struct hashmap_put_data {
	FUTURE_CHAIN_ENTRY(struct hashmap_lock_acquire_fut, lock_acquire);
	FUTURE_CHAIN_ENTRY(struct hashmap_put_key_fut, put_key);
	FUTURE_CHAIN_ENTRY(struct vdm_operation_future, memcpy_val);
	FUTURE_CHAIN_ENTRY(struct hashmap_lock_release_fut, lock_release);
};

struct hashmap_put_output {
	char *return_value;
};

FUTURE(hashmap_put_fut, struct hashmap_put_data, struct hashmap_put_output);

static void
put_key_to_memcpy_val_map(struct future_context *put_key_ctx,
		    struct future_context *memcpy_val_ctx, void *arg)
{
	struct hashmap_put_key_data *put_key_data =
			future_context_get_data(put_key_ctx);
	struct hashmap_put_key_output *put_key_output =
			future_context_get_output(put_key_ctx);
	struct vdm_operation_data *memcpy_val_data =
			future_context_get_data(memcpy_val_ctx);

	struct hashmap *hm = put_key_data->hm;
	int index = put_key_output->index;

	struct vdm_operation *memcpy_op = &memcpy_val_data->operation;
	assert(memcpy_op->type == VDM_OPERATION_MEMCPY);
	if (index == -1) {
		/* Inserting key failed, no need for value copy */
		memcpy_val_ctx->state = FUTURE_STATE_COMPLETE;
		return;
	}

	memcpy_op->data.memcpy.dest = hm->entries[index].value;
}

static void
memcpy_val_to_lock_release(struct future_context *memcpy_val_ctx,
	struct future_context *lock_release_ctx, void *arg)
{
	struct vdm_operation_output *memcpy_val_output =
			future_context_get_output(memcpy_val_ctx);
	struct hashmap_lock_release_output *lock_release_output =
			future_context_get_output(lock_release_ctx);
	lock_release_output->return_value =
			memcpy_val_output->output.memcpy.dest;
}

static void
lock_release_to_output(struct future_context *lock_release_ctx,
	struct future_context *hashmap_put_ctx, void *arg)
{
	struct hashmap_lock_release_output *lock_release_output =
			future_context_get_output(lock_release_ctx);
	struct hashmap_put_output *hashmap_put_output =
			future_context_get_output(hashmap_put_ctx);
	hashmap_put_output->return_value = lock_release_output->return_value;
}

static struct hashmap_put_fut
hashmap_put(struct vdm *vdm, struct hashmap *hm, uint64_t key, void *value)
{
	struct hashmap_put_fut chain = {0};
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.lock_acquire,
			hashmap_lock_acquire(hm), NULL, NULL);
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.put_key,
			hashmap_put_key(hm, key, value),
			put_key_to_memcpy_val_map, NULL);
	/* memcpy destination is determined in 'hashmap_put_key' */
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.memcpy_val,
			vdm_memcpy(vdm, NULL, value, HASHMAP_VALUE_SIZE, 0),
			memcpy_val_to_lock_release, NULL);
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.lock_release,
			hashmap_lock_release(hm), lock_release_to_output, NULL);

	FUTURE_CHAIN_INIT(&chain);

	return chain;
}
/*
 * END of hashmap_put_fut future
 */

/*
 * BEGIN of hashmap_remove_fut future
 */
struct hashmap_remove_data {
	struct hashmap *hm;
	uint64_t key;
};

struct hashmap_remove_output {
	uint64_t return_key;
};

FUTURE(hashmap_remove_fut, struct hashmap_remove_data,
		struct hashmap_remove_output);

static enum future_state
hashmap_remove_impl(struct future_context *ctx,
		struct future_notifier *notifier)
{
	struct hashmap_remove_data *data = future_context_get_data(ctx);
	struct hashmap_remove_output *output = future_context_get_output(ctx);

	struct hashmap *hm = data->hm;
	uint64_t key = data->key;

	if (key == 0) {
		fprintf(stderr, "invalid key %" PRIu64 "\n", key);
		goto set_output;
	}

	/* Try to acquire a hashmap lock */
	if (!hashmap_lock_acquire_impl(hm)) {
		return FUTURE_STATE_RUNNING;
	}

	int index = hashmap_lookup(hm, key);
	if (index == -1) {
		fprintf(stderr, "no entry found for key %" PRIu64 "\n", key);
		key = 0;
		hashmap_lock_release_impl(hm);
		goto set_output;
	}

	hm->entries[index].deleted = 1;
	hm->length--;
	hashmap_lock_release_impl(hm);

set_output:
	output->return_key = key;
	return FUTURE_STATE_COMPLETE;
}

/*
 * hashmap_remove -- removes entry from a hashmap
 */
static struct hashmap_remove_fut
hashmap_remove(struct hashmap *hm, uint64_t key)
{
	struct hashmap_remove_fut future = {0};
	future.data.hm = hm;
	future.data.key = key;

	FUTURE_INIT(&future, hashmap_remove_impl);

	return future;
}
/*
 * END of hashmap_remove_fut future
 */

typedef void (*hashmap_cb)(uint64_t key, void *value, void *arg);

/* Executes callback function for each entry stored in hashmap */
static void
hashmap_foreach(struct hashmap *hm, hashmap_cb cb, void *arg)
{
	uint64_t key;
	void *value;
	for (size_t i = 0; i < hm->capacity; i++) {
		if (hashmap_entry_is_empty(&hm->entries[i])) {
			continue;
		}

		key = hm->entries[i].key;
		value = hm->entries[i].value;

		cb(key, value, arg);
	}
}

/* Hashmap callback, prints key, value pair */
static void
print_entry(uint64_t key, void *value, void *arg)
{
	printf("key: %" PRIu64 ", value: %s\n", key, (char *)value);
}

int
main(void)
{
	/* Set up the data, create a hashmap instance */
	char val_1[32] = "Foo";
	char val_2[32] = "Bar";
	char val_3[32] = "Fizz";
	char val_4[32] = "Buzz";
	char other_val[32] = "Coffee";

	struct hashmap *hm = hashmap_new(4);

	/* Create a runtime instance for efficient future polling */
	struct runtime *r = runtime_new();

	/* Create a thread mover to be used for data move operations */
	struct data_mover_threads *dmt = data_mover_threads_default();
	if (dmt == NULL) {
		fprintf(stderr, "failed to allocate data mover.\n");
		return 1;
	}

	struct vdm *tmover = data_mover_threads_get_vdm(dmt);

	/*
	 * Populate the hashmap. Create four 'hashmap_put_fut' futures and wait
	 * for their completion. 'hashmap_put' future implementation uses data
	 * mover for data copying.
	 */
	struct hashmap_put_fut put_futs[4];
	put_futs[0] = hashmap_put(tmover, hm, 1, val_1);
	put_futs[1] = hashmap_put(tmover, hm, 2, val_2);
	put_futs[2] = hashmap_put(tmover, hm, 3, val_3);
	put_futs[3] = hashmap_put(tmover, hm, 4, val_4);

	WAIT_FUTURES(r, put_futs, 4);
	/*
	 * At this moment hashmap 'hm' stores four entries with the following
	 * key, value pairs: (1, "Foo"), (2, "Bar"), (3, "Fizz"), (4, "Buzz").
	 */

	/*
	 * Successful put operation outputs the address of stored value. Use
	 * 'FUTURE_OUTPUT' macro to extract each future output and assert that
	 * none failed.
	 */
	struct hashmap_put_output *put_output;
	for (int i = 0; i < 4; i++) {
		put_output = FUTURE_OUTPUT(&put_futs[i]);
		assert(put_output->return_value != NULL);
	}

	/* Insert another entry into the hashmap, exceeding hashmap capacity */
	put_futs[0] = hashmap_put(tmover, hm, 404, other_val);

	WAIT_FUTURES(r, put_futs, 1);

	/* Failed insert outputs 'NULL' */
	put_output = FUTURE_OUTPUT(&put_futs[0]);
	assert(put_output->return_value == NULL);

	/*
	 * Make space in the hashmap. Create two 'hashmap_remove_fut` futures
	 * and wait for their completion.
	 */
	struct hashmap_remove_fut remove_futs[2];
	remove_futs[0] = hashmap_remove(hm, 2);
	remove_futs[1] = hashmap_remove(hm, 3);

	WAIT_FUTURES(r, remove_futs, 2);
	/*
	 * Currently, hashmap 'hm' stores two entries with the following
	 * key, value pairs: (1, "Foo"), (4, "Buzz").
	 */

	/* Successful remove operation outputs key of the removed entry */
	struct hashmap_remove_output *remove_output;
	for (int i = 0; i < 2; i++) {
		remove_output = FUTURE_OUTPUT(&remove_futs[i]);
		assert(remove_output->return_key != 0);
	}

	/* Insert two entries with keys already present in the hashmap */
	put_futs[0] = hashmap_put(tmover, hm, 1, other_val);
	put_futs[1] = hashmap_put(tmover, hm, 4, other_val);

	WAIT_FUTURES(r, put_futs, 2);

	/* Hashmap cannot store entires with duplicated keys */
	for (int i = 0; i < 2; i++) {
		put_output = FUTURE_OUTPUT(&put_futs[i]);
		assert(put_output->return_value == NULL);
	}

	/*
	 * Get value of the entry with '4' key. Create a 'hashmap_get_fut'
	 * future and wait for its execution.
	 */
	struct hashmap_get_fut get_futs[1];
	get_futs[0] = hashmap_get(hm, 4);

	WAIT_FUTURES(r, get_futs, 1);

	/* Entry with '4' key should store value 'Buzz' */
	struct hashmap_get_output *get_output = FUTURE_OUTPUT(&get_futs[0]);
	printf("read value: %s\n", get_output->value);

	/* Print key, value pairs of every entry stored in the hashmap */
	hashmap_foreach(hm, print_entry, NULL);

	runtime_delete(r);

	/* avoid unused variable warning */
	(void) put_output;
	(void) remove_output;

	return 0;
}
