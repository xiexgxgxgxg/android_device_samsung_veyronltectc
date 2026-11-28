/* Android 6.0.1 libc exported these two accelerated fills; Android 11 dropped
 * them, but the stock 6.0.1 libpixelflinger (used by Samsung's LPM) still
 * references them.  size is in bytes. */
#include <stdint.h>
#include <stddef.h>

void android_memset32(void *dst, uint32_t value, size_t size)
{
    uint32_t *p = (uint32_t *)dst;
    size_t n = size / 4;
    while (n--)
        *p++ = value;
}

void android_memset16(void *dst, uint16_t value, size_t size)
{
    uint16_t *p = (uint16_t *)dst;
    size_t n = size / 2;
    while (n--)
        *p++ = value;
}
