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
 * Pre-allocate initial space for dynamic array.
 *
 * @param da Dynamic array to pre-allocate space for.
 * @param len Initial number of values to pre-allocate space for.
 * @param val_size Enforced size of a single value.
 * @returns Pointer to space allocated for dynamic array. NULL if error.
 */
void* dynarr_alloc(struct dynarr* da, const size_t len, const size_t val_size);

/**
 * Set values of dynamic array to copy of values given.
 * Capacity of dynamic array will be increased to fit all values at given index if required.
 * If dynamic array is unallocated, size of a single value will become the enforced size of values added to the dynamic array afterwards.
 *
 * @param da Dynamic array to set values of.
 * @param ind Index of dynamic array to set values from.
 * @param vals Pointer to values to copy.
 * @param vals_len Number of values to copy.
 * @param val_size Size of a single value to copy.
 * @returns Values copied to dynamic array. NULL if error.
 */
void* dynarr_set(struct dynarr* da, const size_t ind, const void* vals, const size_t vals_len, const size_t val_size);

/**
 * Push copy of a single value to end of dynamic array.
 * If dynamic array is unallocated, size of value will become the enforced size of values added to the dynamic array afterwards.
 *
 * @param da Dynamic array to push copy of value to.
 * @param val Pointer to value to copy.
 * @param size Size of value to copy.
 * @returns Value pushed to end of dynamic array. NULL if error.
 */
void* dynarr_push(struct dynarr* da, const void* val, const size_t size);

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
