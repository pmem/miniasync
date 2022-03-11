// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "libminiasync.h"
#include "libminiasync/data_mover_threads.h"

/* Avoid compatibility errors */
#ifndef _MSC_VER
#define bool_compare_and_swap __sync_bool_compare_and_swap
#define fetch_and_add __sync_fetch_and_add
#define fetch_and_sub __sync_fetch_and_sub
#else
#include <windows.h>

static __inline int
bool_compare_and_swap_MSVC(volatile LONG *ptr, LONG oldval, LONG newval)
{
	LONG old = InterlockedCompareExchange(ptr, newval, oldval);
	return (old == oldval);
}

#define bool_compare_and_swap(p, o, n)\
	bool_compare_and_swap_MSVC((LONG *)(p), (LONG)(o), (LONG)(n))
#define fetch_and_add(ptr, value)\
	InterlockedExchangeAdd((LONG *)(ptr), value)
#define fetch_and_sub(ptr, value)\
	InterlockedExchangeSubtract((LONG *)(ptr), value)
#endif

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

enum hashmap_entry_state {
	HASHMAP_ENTRY_STATE_UNOCCUPIED,
	HASHMAP_ENTRY_STATE_PROCESSED,
	HASHMAP_ENTRY_STATE_PRESENT,
};

struct hashmap_entry {
	uint64_t key;
	struct {
		void *addr;
		size_t size;
	} value;
	enum hashmap_entry_state state;
};

struct hashmap {
	struct hashmap_entry *entries;
	size_t capacity; /* max stored entries */
	size_t length; /* number of stored entries */
	unsigned lock; /* used for hashmap operations synchronization */
};

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

/* Checks if entry is empty */
static inline int
hashmap_entry_empty(struct hashmap_entry *hme)
{
	return (hme->state == HASHMAP_ENTRY_STATE_UNOCCUPIED && hme->key == 0);
}

/* Checks if entry is deleted */
static inline int
hashmap_entry_deleted(struct hashmap_entry *hme)
{
	return (hme->state == HASHMAP_ENTRY_STATE_UNOCCUPIED && hme->key != 0);
}

/* Checks if hashmap entry is unoccupied */
static inline int
hashmap_entry_unoccupied(struct hashmap_entry *hme)
{
	return hme->state == HASHMAP_ENTRY_STATE_UNOCCUPIED;
}

/* Creates a new hashmap instance */
static struct hashmap *
hashmap_new(size_t capacity)
{
	if (capacity <= 0) {
		fprintf(stderr, "hashmap capacity has to be bigger than 0\n");
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

/* Calculates the index based on provided key and hashmap capacity */
static int
hashmap_key_index(struct hashmap *hm, uint64_t key)
{
	return (int)(hash_val(key) % (hm->capacity - 1));
}

/* Looks for index of the entry with provided key */
static int
hashmap_entry_lookup(struct hashmap *hm, uint64_t key,
		enum hashmap_entry_state state)
{
	int index = hashmap_key_index(hm, key);

	switch (state) {
		case HASHMAP_ENTRY_STATE_UNOCCUPIED:
			for (size_t i = 0; i < hm->capacity; i++, index =
					(index + 1) % (int)hm->capacity) {
				struct hashmap_entry *hme = &hm->entries[index];
				if (hashmap_entry_unoccupied(hme)) {
					return index;
				}
			}
			break;
		case HASHMAP_ENTRY_STATE_PRESENT:
			for (size_t i = 0; i < hm->capacity; i++, index =
					(index + 1) % (int)hm->capacity) {
				struct hashmap_entry *hme = &hm->entries[index];
				if (hashmap_entry_deleted(hme)) {
					continue;
				} else if (hashmap_entry_empty(hme)) {
					break;
				}

				if (hme->key == key) {
					return index;
				}
			}
			break;
		default:
			assert(0); /* should not be reachable */
	}

	return -1;
}

/* Sets the state of hashmap entry */
static unsigned
hashmap_entry_set_state_impl(struct hashmap_entry *hme,
		enum hashmap_entry_state old, enum hashmap_entry_state new)
{
	return bool_compare_and_swap(&hme->state, old, new);
}

/*
 * BEGIN of hashmap_entry_set_state_fut
 */
struct hashmap_entry_set_state_data {
	struct hashmap_entry *hme;
	enum hashmap_entry_state old;
	enum hashmap_entry_state new;
};

struct hashmap_entry_set_state_output {
	unsigned changed;
};

FUTURE(hashmap_entry_set_state_fut, struct hashmap_entry_set_state_data,
		struct hashmap_entry_set_state_output);

static enum future_state
hashmap_entry_set_state_wrapped(struct future_context *ctx,
		struct future_notifier *notifier)
{
	struct hashmap_entry_set_state_data *data =
			future_context_get_data(ctx);
	struct hashmap_entry_set_state_output *output =
			future_context_get_output(ctx);

	output->changed = hashmap_entry_set_state_impl(data->hme, data->old,
			data->new);
	return FUTURE_STATE_COMPLETE;
}

static struct hashmap_entry_set_state_fut
hashmap_entry_set_state(struct hashmap_entry *hme, enum hashmap_entry_state old,
		enum hashmap_entry_state new)
{
	struct hashmap_entry_set_state_fut future = {0};
	future.data.hme = hme;
	future.data.new = new;
	future.data.old = old;

	FUTURE_INIT(&future, hashmap_entry_set_state_wrapped);

	return future;
}
/*
 * END of hashmap_entry_set_state_fut
 */

/*
 * BEGIN of memory_alloc_fut
 */
struct memory_alloc_data {
	size_t size;
};

struct memory_alloc_output {
	void *addr;
};

FUTURE(memory_alloc_fut, struct memory_alloc_data, struct memory_alloc_output);

static enum future_state
memory_alloc_impl(struct future_context *ctx, struct future_notifier *notifier)
{
	struct memory_alloc_data *data = future_context_get_data(ctx);
	struct memory_alloc_output *output = future_context_get_output(ctx);

	output->addr = malloc(data->size);

	return FUTURE_STATE_COMPLETE;
}

/*
 * memory_alloc -- allocates memory of given size
 */
static struct memory_alloc_fut
memory_alloc(size_t size)
{
	struct memory_alloc_fut future = {0};
	future.data.size = size;

	FUTURE_INIT(&future, memory_alloc_impl);

	return future;
}
/*
 * END of memory_alloc_fut
 */

/*
 * BEGIN of hashmap_lookup_fut future
 */
struct hashmap_lookup_data {
	struct hashmap *hm;
	uint64_t key;
	enum hashmap_entry_state state;
};

struct hashmap_lookup_output {
	struct hashmap_entry *hme;
};

FUTURE(hashmap_lookup_fut, struct hashmap_lookup_data,
		struct hashmap_lookup_output);

static enum future_state
hashmap_lookup_impl(struct future_context *ctx,
		struct future_notifier *notifier)
{
	struct hashmap_lookup_data *data =
			future_context_get_data(ctx);
	struct hashmap_lookup_output *output =
			future_context_get_output(ctx);
	struct hashmap *hm = data->hm;
	uint64_t key = data->key;
	enum hashmap_entry_state state = data->state;

	struct hashmap_entry *hme = NULL;
	if (key == 0) {
		fprintf(stderr, "invalid key %" PRIu64 "\n", key);
		goto set_output;
	} else if (state == HASHMAP_ENTRY_STATE_UNOCCUPIED &&
			hm->capacity == hm->length) {
		fprintf(stderr, "no space left for key %" PRIu64 "\n", key);
		goto set_output;
	} else if (state == HASHMAP_ENTRY_STATE_UNOCCUPIED &&
			hashmap_entry_lookup(hm, key,
					HASHMAP_ENTRY_STATE_PRESENT) != -1) {
		fprintf(stderr, "key %" PRIu64 " already exists\n", key);
		goto set_output;
	}

	int index = hashmap_entry_lookup(hm, key, state);
	if (index == -1) {
		switch (state) {
			case HASHMAP_ENTRY_STATE_PRESENT:
			/* Entry with given key is not present in the hashmap */
				return FUTURE_STATE_COMPLETE;
			case HASHMAP_ENTRY_STATE_UNOCCUPIED:
			/*
			 * An unoccupied entry wasn't found despite hashmap not
			 * being full. Re-run the lookup future.
			 */
				return FUTURE_STATE_RUNNING;
			default:
				assert(0); /* should not be reachable */
		}
	}

	hme = &hm->entries[index];

set_output:
	output->hme = hme;
	return FUTURE_STATE_COMPLETE;
}

static struct hashmap_lookup_fut
hashmap_lookup(struct hashmap *hm, uint64_t key, enum hashmap_entry_state state)
{
	struct hashmap_lookup_fut future = {0};
	future.data.hm = hm;
	future.data.key = key;
	future.data.state = state;

	FUTURE_INIT(&future, hashmap_lookup_impl);

	return future;
}
/*
 * END of hashmap_find_unoccupiedlookup future
 */

/*
 * BEGIN of hashmap_lookup_lock_entry_fut future
 */
struct hashmap_lookup_lock_entry_data {
	FUTURE_CHAIN_ENTRY(struct hashmap_lookup_fut, lookup);
	FUTURE_CHAIN_ENTRY(struct hashmap_entry_set_state_fut, set_state);
};

struct hashmap_lookup_lock_entry_output {
	struct hashmap_entry *hme;
};

FUTURE(hashmap_lookup_lock_entry_fut, struct hashmap_lookup_lock_entry_data,
		struct hashmap_lookup_lock_entry_output);

static void
lookup_to_set_state_map(struct future_context *lookup_ctx,
		struct future_context *set_state_ctx, void *arg)
{
	struct hashmap_lookup_output *lookup_unoccupied_output =
			future_context_get_output(lookup_ctx);
	struct hashmap_entry_set_state_data *set_state_data =
			future_context_get_data(set_state_ctx);
	struct hashmap_entry *hme = lookup_unoccupied_output->hme;

	if (hme == NULL) {
		/*
		 * Entry lookup failed, no need to lock the entry in
		 * 'processed' state.
		 */
		set_state_ctx->state = FUTURE_STATE_COMPLETE;
	}

	set_state_data->hme = hme;
}

static void
set_state_to_output_map(struct future_context *set_state_ctx,
		struct future_context *lock_entry_ctx, void *arg)
{
	struct hashmap_entry_set_state_data *set_state_data =
			future_context_get_data(set_state_ctx);
	struct hashmap_entry_set_state_output *set_state_output =
			future_context_get_output(set_state_ctx);
	struct hashmap_lookup_lock_entry_output *lock_entry_output =
			future_context_get_output(lock_entry_ctx);

	lock_entry_output->hme = (set_state_output->changed) ?
			set_state_data->hme : NULL;
}

static struct hashmap_lookup_lock_entry_fut
hashmap_lookup_lock_entry(struct hashmap *hm, uint64_t key,
		enum hashmap_entry_state state)
{
	struct hashmap_lookup_lock_entry_fut chain = {0};
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.lookup,
			hashmap_lookup(hm, key, state),
			lookup_to_set_state_map, NULL);
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.set_state,
			hashmap_entry_set_state(NULL, state,
					HASHMAP_ENTRY_STATE_PROCESSED),
			set_state_to_output_map, NULL);

	FUTURE_CHAIN_INIT(&chain);

	return chain;
}
/*
 * END of hashmap_lookup_lock_entry_fut future
 */

/*
 * BEGIN of hashmap_get_fut future
 */
struct hashmap_get_copy_data {
	FUTURE_CHAIN_ENTRY(struct hashmap_lookup_lock_entry_fut,
			lookup_lock_entry);
	FUTURE_CHAIN_ENTRY(struct memory_alloc_fut, memory_alloc);
	FUTURE_CHAIN_ENTRY(struct vdm_operation_future, memcpy_value);
	FUTURE_CHAIN_ENTRY(struct hashmap_entry_set_state_fut, set_entry_state);
};

struct hashmap_get_copy_output {
	void *value;
	size_t size;
};

FUTURE(hashmap_get_copy_fut, struct hashmap_get_copy_data,
		struct hashmap_get_copy_output);

static void
memcpy_value_init(void *future, struct future_context *hashmap_get_copy_ctx,
		void *arg)
{
	struct hashmap_get_copy_data *data =
			future_context_get_data(hashmap_get_copy_ctx);
	struct hashmap_get_copy_output *output =
			future_context_get_output(hashmap_get_copy_ctx);
	struct hashmap_entry *hme = data->lookup_lock_entry.fut.output.hme;

	void *src = hme->value.addr;
	void *dest = data->memory_alloc.fut.output.addr;
	size_t size = hme->value.size;

	struct vdm_operation_future fut;
	if (dest != NULL) {
		/* Data mover passed as an argument */
		struct vdm *vdm = (struct vdm *)arg;
		fut = vdm_memcpy(vdm, dest, src, size, 0);
	} else {
		/* Allocation failed, entry value shouldn't be copied */
		FUTURE_INIT_COMPLETE(&fut);
	}

	memcpy(future, &fut, sizeof(fut));

	/* Set 'hashmap_get_copy_fut' future output as destination address */
	output->value = dest;
	output->size = size;
}

static void
set_entry_state_init_get(void *future, struct future_context *hashmap_get_ctx,
		void *arg)
{
	struct hashmap_get_copy_data *data =
			future_context_get_data(hashmap_get_ctx);

	struct hashmap_entry_set_state_fut fut = {0};
	struct hashmap_entry *hme = data->lookup_lock_entry.fut.output.hme;
	if (hme == NULL) {
		/* Entry wasn't found, entry state shouldn't be changed */
		FUTURE_INIT_COMPLETE(&fut);
	} else {
		/* Entry value was copied, set entry state to 'present' */
		fut = hashmap_entry_set_state(hme,
				HASHMAP_ENTRY_STATE_PROCESSED,
				HASHMAP_ENTRY_STATE_PRESENT);
	}

	memcpy(future, &fut, sizeof(fut));
}

/*
 * hashmap_get_copy -- gets a copy of the value from the hashmap entry
 */
static struct hashmap_get_copy_fut
hashmap_get_copy(struct vdm *vdm, struct hashmap *hm, uint64_t key)
{
	struct hashmap_get_copy_fut chain = {0};
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.lookup_lock_entry,
			hashmap_lookup_lock_entry(hm, key,
					HASHMAP_ENTRY_STATE_PRESENT), NULL,
					NULL);
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.memory_alloc,
			memory_alloc(0), NULL, NULL);
	FUTURE_CHAIN_ENTRY_LAZY_INIT(&chain.data.memcpy_value,
			memcpy_value_init, vdm, NULL, NULL);
	FUTURE_CHAIN_ENTRY_LAZY_INIT(&chain.data.set_entry_state,
			set_entry_state_init_get, NULL, NULL, NULL);

	FUTURE_CHAIN_INIT(&chain);

	return chain;
}
/*
 * END of hashmap_get_fut future
 */

/*
 * BEGIN of hashmap_entry_init_fut future
 */
struct hashmap_entry_init_data {
	struct hashmap *hm;
	struct hashmap_entry *hme;
	uint64_t key;
	size_t size;
};

struct hashmap_entry_init_output {
	struct hashmap_entry *hme;
};

FUTURE(hashmap_entry_init_fut, struct hashmap_entry_init_data,
		struct hashmap_entry_init_output);

static enum future_state
hashmap_entry_init_impl(struct future_context *ctx,
		struct future_notifier *notifier)
{
	struct hashmap_entry_init_data *data = future_context_get_data(ctx);
	struct hashmap_entry_init_output *output =
			future_context_get_output(ctx);
	struct hashmap *hm = data->hm;
	struct hashmap_entry *hme = data->hme;
	output->hme = hme;

	void *addr = malloc(data->size);
	hme->key = data->key;
	hme->value.addr = addr;
	hme->value.size = data->size;

	if (addr == NULL) {
		return FUTURE_STATE_COMPLETE;
	}

	size_t old_val = fetch_and_add(&hm->length, 1);
	assert(old_val != hm->length);

	return FUTURE_STATE_COMPLETE;
}

static struct hashmap_entry_init_fut
hashmap_entry_init(struct hashmap *hm, struct hashmap_entry *hme, uint64_t key,
		size_t size)
{
	struct hashmap_entry_init_fut future = {0};
	future.data.hm = hm;
	future.data.hme = hme;
	future.data.key = key;
	future.data.size = size;

	FUTURE_INIT(&future, hashmap_entry_init_impl);

	return future;
}
/*
 * END of hashmap_entry_init_fut future
 */

/*
 * BEGIN of hashmap_put_fut future
 */
struct hashmap_put_data {
	FUTURE_CHAIN_ENTRY(struct hashmap_lookup_lock_entry_fut,
			lookup_lock_entry);
	FUTURE_CHAIN_ENTRY(struct hashmap_entry_init_fut, init_entry);
	FUTURE_CHAIN_ENTRY(struct vdm_operation_future, memcpy_value);
	FUTURE_CHAIN_ENTRY(struct hashmap_entry_set_state_fut, set_entry_state);
};

struct hashmap_put_output {
	char *value;
};

FUTURE(hashmap_put_fut, struct hashmap_put_data, struct hashmap_put_output);

static void
lookup_lock_entry_to_entry_init_map(
		struct future_context *lookup_lock_entry_ctx,
		struct future_context *init_entry_ctx, void *arg)
{
	struct hashmap_lookup_lock_entry_output *lookup_lock_output =
			future_context_get_output(lookup_lock_entry_ctx);
	struct hashmap_entry_init_data *init_data =
			future_context_get_data(init_entry_ctx);
	struct hashmap_entry *hme = lookup_lock_output->hme;

	if (hme == NULL) {
		/* Entry wasn't found, no need for entry initialization */
		init_entry_ctx->state = FUTURE_STATE_COMPLETE;
	}

	init_data->hme = hme;
}

static void
init_entry_to_memcpy_value_map(struct future_context *init_entry_ctx,
		    struct future_context *memcpy_value_ctx, void *arg)
{
	struct hashmap_entry_init_output *init_entry_output =
			future_context_get_output(init_entry_ctx);
	struct vdm_operation_data *memcpy_value_data =
			future_context_get_data(memcpy_value_ctx);

	struct hashmap_entry *hme = init_entry_output->hme;
	if (hme == NULL || hme->value.addr == NULL) {
		/*
		 * Either entry wasn't found or it wasn't initialized, no
		 * need for value copy.
		 */
		memcpy_value_ctx->state = FUTURE_STATE_COMPLETE;
		return;
	}

	struct vdm_operation *memcpy_op = &memcpy_value_data->operation;
	memcpy_op->data.memcpy.dest = hme->value.addr;
}

static void
set_entry_state_to_output_map_put(struct future_context *entry_set_state_ctx,
		    struct future_context *put_ctx, void *arg)
{
	struct hashmap_entry_set_state_data *entry_set_state_data =
			future_context_get_data(entry_set_state_ctx);
	struct hashmap_put_output *put_output =
			future_context_get_output(put_ctx);
	struct hashmap_entry *hme = entry_set_state_data->hme;
	put_output->value = (hme) ? hme->value.addr : NULL;
}

static void
set_entry_state_init_put(void *future, struct future_context *hashmap_put_ctx,
		void *arg)
{
	struct hashmap_put_data *data =
			future_context_get_data(hashmap_put_ctx);
	struct hashmap_entry_set_state_fut fut = {0};
	struct hashmap_entry *hme = data->init_entry.fut.output.hme;

	/* Entry wasn't found, entry state shouldn't be changed */
	if (hme == NULL) {
		FUTURE_INIT_COMPLETE(&fut);
		fut.data.hme = NULL;
		goto copy_fut;
	}

	/*
	 * Entry state should be set to 'unoccupied' when initialization
	 * failed or 'present' when it was successful.
	 */
	enum hashmap_entry_state state = (hme->value.addr != NULL) ?
		HASHMAP_ENTRY_STATE_PRESENT : HASHMAP_ENTRY_STATE_UNOCCUPIED;

	fut = hashmap_entry_set_state(hme, HASHMAP_ENTRY_STATE_PROCESSED,
			state);

copy_fut:
	memcpy(future, &fut, sizeof(fut));
}

static struct hashmap_put_fut
hashmap_put(struct vdm *vdm, struct hashmap *hm, uint64_t key, void *value,
		size_t size)
{
	struct hashmap_put_fut chain = {0};
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.lookup_lock_entry,
			hashmap_lookup_lock_entry(hm, key,
					HASHMAP_ENTRY_STATE_UNOCCUPIED),
			lookup_lock_entry_to_entry_init_map, NULL);
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.init_entry,
			hashmap_entry_init(hm, NULL, key, size),
			init_entry_to_memcpy_value_map, NULL);
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.memcpy_value,
			vdm_memcpy(vdm, NULL, value, size, 0), NULL, NULL);
	FUTURE_CHAIN_ENTRY_LAZY_INIT(&chain.data.set_entry_state,
			set_entry_state_init_put, NULL,
			set_entry_state_to_output_map_put, NULL);

	FUTURE_CHAIN_INIT(&chain);

	return chain;
}
/*
 * END of hashmap_put_fut future
 */

/*
 * BEGIN of hashmap_entry_fini_fut future
 */
struct hashmap_entry_fini_data {
	struct hashmap *hm;
	struct hashmap_entry *hme;
};

struct hashmap_entry_fini_output {
	struct hashmap_entry *hme;
};

FUTURE(hashmap_entry_fini_fut, struct hashmap_entry_fini_data,
		struct hashmap_entry_fini_output);

static enum future_state
hashmap_entry_fini_impl(struct future_context *ctx,
		struct future_notifier *notifier)
{
	struct hashmap_entry_fini_data *data = future_context_get_data(ctx);
	struct hashmap_entry_fini_output *output =
			future_context_get_output(ctx);
	struct hashmap *hm = data->hm;
	struct hashmap_entry *hme = data->hme;
	output->hme = hme;

	free(hme->value.addr);

	size_t old_val = fetch_and_sub(&hm->length, 1);
	assert(old_val != hm->length);

	return FUTURE_STATE_COMPLETE;
}

static struct hashmap_entry_fini_fut
hashmap_entry_fini(struct hashmap *hm, struct hashmap_entry *hme)
{
	struct hashmap_entry_fini_fut future = {0};
	future.data.hm = hm;
	future.data.hme = hme;

	FUTURE_INIT(&future, hashmap_entry_fini_impl);

	return future;
}
/*
 * END of hashmap_entry_fini_fut future
 */

/*
 * BEGIN of hashmap_remove_fut future
 */
struct hashmap_remove_data {
	FUTURE_CHAIN_ENTRY(struct hashmap_lookup_lock_entry_fut,
			lookup_lock_entry);
	FUTURE_CHAIN_ENTRY(struct hashmap_entry_fini_fut, fini_entry);
	FUTURE_CHAIN_ENTRY(struct hashmap_entry_set_state_fut, set_entry_state);
};

struct hashmap_remove_output {
	uint64_t key;
};

FUTURE(hashmap_remove_fut, struct hashmap_remove_data,
		struct hashmap_remove_output);

static void
lookup_lock_entry_to_entry_fini_map(
		struct future_context *lookup_lock_entry_ctx,
		struct future_context *fini_entry_ctx, void *arg)
{
	struct hashmap_lookup_lock_entry_output *lookup_lock_output =
			future_context_get_output(lookup_lock_entry_ctx);
	struct hashmap_entry_fini_data *fini_data =
			future_context_get_data(fini_entry_ctx);
	struct hashmap_entry *hme = lookup_lock_output->hme;

	if (hme == NULL) {
		/* Entry wasn't found, no need for entry finalziation */
		fini_entry_ctx->state = FUTURE_STATE_COMPLETE;
	}

	fini_data->hme = hme;
}

static void
set_entry_state_to_output_map_remove(struct future_context *entry_set_state_ctx,
		    struct future_context *remove_ctx, void *arg)
{
	struct hashmap_entry_set_state_data *entry_set_state_data =
			future_context_get_data(entry_set_state_ctx);
	struct hashmap_remove_output *remove_output =
			future_context_get_output(remove_ctx);
	struct hashmap_entry *hme = entry_set_state_data->hme;
	remove_output->key = (hme) ? hme->key : 0;
}

static void
set_entry_state_init_remove(void *future,
		struct future_context *hashmap_remove_ctx, void *arg)
{
	struct hashmap_remove_data *data =
			future_context_get_data(hashmap_remove_ctx);
	struct hashmap_entry_set_state_fut fut = {0};
	struct hashmap_entry *hme = data->fini_entry.fut.output.hme;

	if (hme == NULL) {
		/* Entry wasn't found, entry state shouldn't be changed */
		FUTURE_INIT_COMPLETE(&fut);
		fut.data.hme = NULL;
	} else {
		/* Entry was removed, set entry state to 'unoccupied' */
		fut = hashmap_entry_set_state(hme,
				HASHMAP_ENTRY_STATE_PROCESSED,
				HASHMAP_ENTRY_STATE_UNOCCUPIED);
	}

	memcpy(future, &fut, sizeof(fut));
}

static struct hashmap_remove_fut
hashmap_remove(struct hashmap *hm, uint64_t key)
{
	struct hashmap_remove_fut chain = {0};
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.lookup_lock_entry,
			hashmap_lookup_lock_entry(hm, key,
					HASHMAP_ENTRY_STATE_PRESENT),
			lookup_lock_entry_to_entry_fini_map, NULL);
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.fini_entry,
			hashmap_entry_fini(hm, NULL), NULL, NULL);
	FUTURE_CHAIN_ENTRY_LAZY_INIT(&chain.data.set_entry_state,
			set_entry_state_init_remove, NULL,
			set_entry_state_to_output_map_remove, NULL);

	FUTURE_CHAIN_INIT(&chain);

	return chain;
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
		if (hashmap_entry_unoccupied(&hm->entries[i])) {
			continue;
		}

		key = hm->entries[i].key;
		value = hm->entries[i].value.addr;

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
	char val_1[] = "Foo";
	char val_2[] = "Bar";
	char val_3[] = "Fizz";
	char val_4[] = "Buzz";
	char other_val[] = "Coffee";

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
	put_futs[0] = hashmap_put(tmover, hm, 1, val_1, strlen(val_1) + 1);
	put_futs[1] = hashmap_put(tmover, hm, 2, val_2, strlen(val_2) + 1);
	put_futs[2] = hashmap_put(tmover, hm, 3, val_3, strlen(val_3) + 1);
	put_futs[3] = hashmap_put(tmover, hm, 4, val_4, strlen(val_4) + 1);

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
		assert(put_output->value != NULL);
	}

	/* Insert another entry into the hashmap, exceeding hashmap capacity */
	put_futs[0] = hashmap_put(tmover, hm, 404, other_val,
			strlen(other_val) + 1);

	WAIT_FUTURES(r, put_futs, 1);

	/* Failed insert outputs 'NULL' */
	put_output = FUTURE_OUTPUT(&put_futs[0]);
	assert(put_output->value == NULL);

	/*
	 * Make space in the hashmap. Create two 'hashmap_remove_fut` futures
	 * and wait for their completion.
	 */
	struct hashmap_remove_fut remove_futs[2];
	remove_futs[0] = hashmap_remove(hm, 2);
	remove_futs[1] = hashmap_remove(hm, 3);
	// volatile int x = 1; while (x);
	WAIT_FUTURES(r, remove_futs, 2);

	/*
	 * Currently, hashmap 'hm' stores two entries with the following
	 * key, value pairs: (1, "Foo"), (4, "Buzz").
	 */

	/* Successful remove operation outputs key of the removed entry */
	struct hashmap_remove_output *remove_output;
	for (int i = 0; i < 2; i++) {
		remove_output = FUTURE_OUTPUT(&remove_futs[i]);
		assert(remove_output->key != 0);
	}

	/* Insert two entries with keys already present in the hashmap */
	put_futs[0] = hashmap_put(tmover, hm, 1, other_val,
			strlen(other_val) + 1);
	put_futs[1] = hashmap_put(tmover, hm, 4, other_val,
			strlen(other_val) + 1);

	WAIT_FUTURES(r, put_futs, 2);

	/* Hashmap cannot store entry with duplicated key */
	for (int i = 0; i < 2; i++) {
		put_output = FUTURE_OUTPUT(&put_futs[i]);
		assert(put_output->value == NULL);
	}

	/*
	 * Get value of the entry with '4' key. Create a 'hashmap_get_fut'
	 * future and wait for its execution.
	 */
	struct hashmap_get_copy_fut get_futs[1];
	get_futs[0] = hashmap_get_copy(tmover, hm, 4);

	WAIT_FUTURES(r, get_futs, 1);

	/* Entry with '4' key should store value 'Buzz' */
	struct hashmap_get_copy_output *get_copy_output =
			FUTURE_OUTPUT(&get_futs[0]);
	printf("copied value: %s\n", (char *)get_copy_output->value);
	free(get_copy_output->value);

	/* Print key, value pairs of every entry stored in the hashmap */
	hashmap_foreach(hm, print_entry, NULL);

	runtime_delete(r);

	/* avoid unused variable warning */
	(void) put_output;
	(void) remove_output;

	return 0;
}
