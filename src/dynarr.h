#ifndef DYNARR_H
#define DYNARR_H

#include <stddef.h>

/**
 * Dynamic array.
 */
struct dynarr {
	void* vals;
	size_t val_size;
	size_t capacity;
	size_t len;
};

/**
 * Get value at index of dynamic array.
 *
 * @param da Dynamic array to get value from.
 * @param ind Index of value to get.
 * @returns Value within dynamic array. NULL if not found.
 */
void* dynarr_get(const struct dynarr da, const size_t ind);

/**
 * Allocate initial space for dynamic array.
 *
 * @param da Dynamic array to allocate space for.
 * @param capacity Initial number of values to allocate space for.
 * @param val_size Enforced maximum size of values.
 * @returns Number of bytes allocated for dynamic array. 0 if error.
 */
size_t dynarr_alloc(struct dynarr* da, const size_t capacity, const size_t val_size);

/**
 * Add copy of value to dynamic array.
 * If dynamic array is unallocated, size of value will become the enforced maximum size of all values.
 *
 * @param da Dynamic array to add value to.
 * @param val Pointer to value to copy.
 * @param size Size of value to copy.
 * @returns Value added to dynamic array. NULL if error.
 */
void* dynarr_push(struct dynarr* da, const void* val, const size_t size);

/**
 * Copy values from one dynamic array to another.
 *
 * @param dst Dynamic array to copy values to.
 * @param src Dynamic array to copy values from.
 * @returns Number of values copied.
 */
size_t dynarr_copy(struct dynarr* dst, const struct dynarr src);

/**
 * Free values within dynamic array using delegate function.
 * Dynamic array will be in unallocated state once values are freed.
 *
 * @param da Dynamic array to free values of.
 * @param f Delegate function used to empty values.
 */
void dynarr_delegate_empty(struct dynarr* da, void (*f)(void*));

/**
 * Free values within dynamic array.
 * Dynamic array will be in unallocated state once values are freed.
 *
 * @param da Dynamic array to free values of.
 */
void dynarr_empty(struct dynarr* da);

#endif