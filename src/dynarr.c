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

void* dynarr_alloc(struct dynarr* da, const size_t len, const size_t val_size)
{
	if (!da)
		return NULL;

	// Increment capacity to fit all values
	size_t capacity = 1;
	while (len > capacity) {
		capacity = CAPACITY_INC(capacity);
	}

	da->vals = calloc(capacity, val_size);
	da->val_size = val_size;
	da->len = 0;

	if (!da->vals)
		da->capacity = 0;
	else
		da->capacity = capacity;

	return da->vals;
}

/**
 * Allocate additional space for dynamic array.
 *
 * @param da Dynamic array to allocate space for.
 * @param len Number of values to allocate space for.
 * @returns Pointer to space reallocated for dynamic array. NULL if error.
 */
static void* dynarr_realloc(struct dynarr* da, const size_t len)
{
	if (!da)
		return NULL;

	// Shrinking not required/supported - no realloc required
	if (len <= da->capacity)
		return da->vals;

	// Increment capacity to fit all values
	size_t capacity = da->capacity;
	while (len > capacity) {
		capacity = CAPACITY_INC(capacity);
	}

	void* vals_new = realloc(da->vals, capacity * da->val_size);
	if (!vals_new)
		return NULL;

	// Zero out newly available space (as if it had been calloc'd)
	// Failure to zero out space is non-critical - not checking return result
	memset((uint8_t*)vals_new + (da->capacity * da->val_size), 0, (capacity - da->capacity) * da->val_size);

	da->vals = vals_new;
	da->capacity = capacity;
	return da->vals;
}

void* dynarr_set(struct dynarr* da, const size_t ind, const void* vals, const size_t vals_len, const size_t val_size)
{
	if (!da || !vals)
		return NULL;

	bool da_unalloc = !da->vals;
	size_t len_new = (ind + vals_len > da->len) ? ind + vals_len : da->len;

	// Allocate space for all values if not allocated already
	if (da_unalloc && !dynarr_alloc(da, len_new, val_size))
		goto error;

	// Enforce size of values
	if (val_size != da->val_size)
		goto error;

	// Increase capacity to fit all values if not enough
	if (len_new > da->capacity && !dynarr_realloc(da, len_new))
		goto error;

	// Copy values to given index
	void* result = memcpy((uint8_t*)da->vals + (ind * da->val_size), vals, vals_len * val_size);
	if (!result)
		goto error;

	da->len = len_new;
	return result;

	error:
	if (da_unalloc) dynarr_empty(da);
	return NULL;
}

void* dynarr_push(struct dynarr* da, const void* val, const size_t size)
{
	if (!da)
		return NULL;

	return dynarr_set(da, da->len, val, 1, size);
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
