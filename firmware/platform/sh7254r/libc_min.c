/* Freestanding replacements for the few C library functions the project uses
 * (the target links without a C library). */
#include <stddef.h>
#include <stdint.h>

void *memset(void *d, int c, size_t n)
{
    uint8_t *p = d;
    while (n-- != 0u) {
        *p++ = (uint8_t)c;
    }
    return d;
}

void *memcpy(void *d, const void *s, size_t n)
{
    uint8_t *p = d;
    const uint8_t *q = s;
    while (n-- != 0u) {
        *p++ = *q++;
    }
    return d;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *p = a, *q = b;
    for (; n != 0u; n--, p++, q++) {
        if (*p != *q) {
            return *p < *q ? -1 : 1;
        }
    }
    return 0;
}
