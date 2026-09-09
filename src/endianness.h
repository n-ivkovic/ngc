#ifndef ENDIANNESS_H
#define ENDIANNESS_H

#include <stddef.h>

enum endianness {
    LITTLE_E,
    BIG_E
};

/**
 * Get the system's endianness.
 *
 * @returns The system's endianness.
 */
enum endianness endianness_get(void);


/**
 * Reverse the endianness of a value.
 *
 * @param out Pointer to store value with reverse endianness.
 * @param in Pointer to value to reverse endianness of.
 * @param size Size of value to reverse endianness of.
 */
void endianness_reverse(void* out, const void* in, const size_t size);

#endif
