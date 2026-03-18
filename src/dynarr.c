#include "dynarr.h"

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

	// Allocate space for 1 value if not allocated already
	if (!da->vals && dynarr_alloc(da, 1, size) == 0)
		return NULL;

	if (size > da->val_size)
		return NULL;

	// Increase capacity if not enough
	if (da->len >= da->capacity && dynarr_realloc(da, CAPACITY_INC(da->capacity)) == 0)
		return NULL;

	void* result = memcpy((uint8_t*)da->vals + (da->len * da->val_size), val, size);
	da->len++;

	return result;
}

size_t dynarr_copy(struct dynarr* dst, const struct dynarr src)
{
	if (!dst)
		return 0;

	if (src.len == 0)
		return 0;

	// Copy source as-is to destination if destination is unallocated
	if (!dst->vals) {
		if (!dynarr_alloc(dst, src.capacity, src.val_size))
			return 0;

		if (!memcpy(dst->vals, src.vals, src.len * src.val_size)) {
			dynarr_empty(dst);
			return 0;
		}

		dst->len = src.len;
		return dst->len;
	}

	if (dst->val_size != src.val_size)
		return 0;

	size_t len_new = dst->len + src.len;

	// Realloc destination to fit all values
	if (len_new > dst->capacity) {
		size_t capacity_new = dst->capacity;
		do {
			capacity_new = CAPACITY_INC(capacity_new);
		} while (len_new > capacity_new);

		if (dynarr_realloc(dst, capacity_new) == 0)
			return 0;

		// If failure happens after realloc, destination will not be shrunk
	}

	if (!memcpy((uint8_t*)dst->vals + (dst->len * dst->val_size), src.vals, src.len * src.val_size))
		return 0;

	dst->len = len_new;
	return len_new;
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

	free(da->vals);
	da->vals = NULL;
	da->val_size = 0;
	da->len = 0;
	da->capacity = 0;
}
