#include "dynarr.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CAPACITY_INC(capacity) (capacity * 2)

void* dynarr_get(const struct dynarr da, const size_t ind)
{
	if (ind >= da.len)
		return NULL;

	return (uint8_t*)da.vals + (ind * da.val_size);
}

size_t dynarr_alloc(struct dynarr* da, const size_t capacity, const size_t val_size)
{
	if (!da)
		return 0;

	da->vals = calloc(capacity, val_size);
	da->val_size = val_size;
	da->len = 0;

	if (!da->vals)
		da->capacity = 0;
	else
		da->capacity = capacity;

	return da->capacity * da->val_size;
}

/**
 * Allocate additional space for dynamic array.
 *
 * @param da Dynamic array to allocate space for.
 * @param capacity Number of values to allocate space for.
 * @returns Number of bytes allocated for dynamic array. 0 if error.
 */
static size_t dynarr_realloc(struct dynarr* da, size_t capacity)
{
	if (!da)
		return 0;

	// No realloc required - shrinking not required/supported
	if (capacity <= da->capacity)
		return da->capacity * da->val_size;

	void* vals_new = realloc(da->vals, capacity * da->val_size);
	if (!vals_new)
		return 0;

	// Zero out newly available space (as if it had been calloc'd)
	if (!memset((uint8_t*)vals_new + (da->capacity * da->val_size), 0, (capacity - da->capacity) * da->val_size))
		return 0;

	da->vals = vals_new;
	da->capacity = capacity;
	return da->capacity * da->val_size;
}

void* dynarr_push(struct dynarr* da, const void* val, const size_t size)
{
	if (!da || !val)
		return NULL;

	bool da_unalloc = !da->vals;

	// Allocate space for value if not allocated already
	if (da_unalloc && dynarr_alloc(da, 1, size) == 0)
		goto error;

	if (size > da->val_size)
		goto error;

	// Increase capacity to fit value if not enough
	if (da->len >= da->capacity && dynarr_realloc(da, CAPACITY_INC(da->capacity)) == 0)
		goto error;

	void* result = memcpy((uint8_t*)da->vals + (da->len * da->val_size), val, size);
	da->len++;

	return result;

	error:
	if (da_unalloc) dynarr_empty(da);
	return NULL;
}

void* dynarr_set(struct dynarr* da, const size_t ind, const void* vals, const size_t vals_len, const size_t val_size)
{
	if (!da || !vals)
		return NULL;

	if (vals_len == 0)
		return NULL;

	bool da_unalloc = !da->vals;
	size_t len_new = (ind + vals_len > da->len) ? ind + vals_len : da->len;

	// Allocate space for all values if not allocated already
	if (da_unalloc) {
		size_t capacity = 1;
		while (len_new > capacity) {
			capacity = CAPACITY_INC(capacity);
		}

		if (dynarr_alloc(da, capacity, val_size) == 0)
			goto error;
	}

	if (val_size != da->val_size)
		goto error;

	// Increase capacity to fit all values if not enough
	if (len_new > da->capacity) {
		size_t capacity_new = da->capacity;
		do {
			capacity_new = CAPACITY_INC(capacity_new);
		} while (len_new > capacity_new);

		if (dynarr_realloc(da, capacity_new) == 0)
			goto error;

		// If failure happens after realloc, dynamic array will not be shrunk
	}

	void* result = memcpy((uint8_t*)da->vals + (ind * da->val_size), vals, vals_len * val_size);
	if (!result)
		goto error;

	da->len = len_new;
	return result;

	error:
	if (da_unalloc) dynarr_empty(da);
	return NULL;
}

void dynarr_delegate_empty(struct dynarr* da, void (*f)(void*))
{
	if (!da || !f)
		return;

	for (size_t ind = 0; ind < da->len; ind++) {
		f((uint8_t*)da->vals + (ind * da->val_size));
	}

	dynarr_empty(da);
}

void dynarr_empty(struct dynarr* da)
{
	if (!da)
		return;

	if (da->vals) free(da->vals);
	da->vals = NULL;
	da->val_size = 0;
	da->len = 0;
	da->capacity = 0;
}
