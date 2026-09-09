#include "endianness.h"

#include <stdint.h>

enum endianness endianness_get(void)
{
    const int value = 0x0001;
    return (*((uint8_t*)&value) == 0x01) ? LITTLE_E : BIG_E;
}

void endianness_reverse(void* out, const void* in, const size_t size)
{
    if (!in || !out)
        return;

    for (size_t ind = 0; ind < size; ind++) {
        *((uint8_t*)out + size - 1 - ind) = *((uint8_t*)in + ind);
    }
}
