#include "utility.h"
#include "platforms.h"

#include <limits.h>
#include <stdint.h>

#if WINDOWS_OS
#include <windows.h>
#include <winnt.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if WINDOWS_OS
#define ATOMIC_CMPXCHG(op)                                                  \
    Atomic old, current;                                                    \
                                                                            \
    do {                                                                    \
        old = *location;                                                    \
        current = op;                                                       \
    } while (atomic_cmpxchg(location, current, old) != old);                \
                                                                            \
    return old;

#define ATOMICP_CMPXCHG(op)                                                 \
    AtomicPointer old, current;                                             \
                                                                            \
    do {                                                                    \
        old = *location;                                                    \
        current = op;                                                       \
    } while (atomicp_cmpxchg(location, current, old) != old);               \
                                                                            \
    return old;

Atomic atomic_set(volatile Atomic *location, Atomic value)
{
    return InterlockedExchange(location, value);
}

Atomic atomic_add(volatile Atomic *location, Atomic value)
{
    return InterlockedExchangeAdd(location, value);
}

Atomic atomic_sub(volatile Atomic *location, Atomic value)
{
    return InterlockedExchangeAdd(location, -value);
}

Atomic atomic_and(volatile Atomic *location, Atomic value)
{
    ATOMIC_CMPXCHG(old & value);
}

Atomic atomic_or(volatile Atomic *location, Atomic value)
{
    ATOMIC_CMPXCHG(old | value);
}

Atomic atomic_xor(volatile Atomic *location, Atomic value)
{
    ATOMIC_CMPXCHG(old ^ value);
}

Atomic atomic_load(const volatile Atomic *location)
{
    return *location;
    /* Alternately: return atomic_add((volatile Atomic *) location, 0); */
}

Atomic atomic_cmpxchg(volatile Atomic *location, Atomic value, Atomic compare)
{
    return InterlockedCompareExchange(location, value, compare);
}

int atomic_test_bit(const volatile Atomic *location, unsigned bit)
{
    return (atomic_load(location) >> bit) & 1;
}

int atomic_set_bit(volatile Atomic *location, unsigned bit)
{
    ATOMIC_CMPXCHG(old | ((Atomic) 1 << bit));
}

int atomic_clear_bit(volatile Atomic *location, unsigned bit)
{
    ATOMIC_CMPXCHG(old & ~((Atomic) 1 << bit));
}

int atomic_flip_bit(volatile Atomic *location, unsigned bit)
{
    ATOMIC_CMPXCHG(old ^ ((Atomic) 1 << bit));
}

AtomicPointer atomicp_set(volatile AtomicPointer *location, AtomicPointer value)
{
    return InterlockedExchangePointer(location, value);
}

AtomicPointer atomicp_add(volatile AtomicPointer *location, intptr_t value)
{
    ATOMICP_CMPXCHG((char *) old + value);
}

AtomicPointer atomicp_sub(volatile AtomicPointer *location, intptr_t value)
{
    ATOMICP_CMPXCHG((char *) old - value);
}

AtomicPointer atomicp_load(const volatile AtomicPointer *location)
{
    return *location;
}

AtomicPointer atomicp_cmpxchg(volatile AtomicPointer *location, AtomicPointer value, AtomicPointer compare)
{
    return InterlockedCompareExchangePointer(location, value, compare);
}
#elif LINUX_OS
#define ATOMIC_CMPXCHG(op)                                                  \
    Atomic old, current;                                                    \
                                                                            \
    do {                                                                    \
        old = __sync_fetch_and_or(location, 0);                             \
        current = op;                                                       \
    } while (atomic_cmpxchg(location, current, old) != old);                \
                                                                            \
    return old;

#define ATOMICP_CMPXCHG(op)                                                 \
    AtomicPointer old, current;                                             \
                                                                            \
    do {                                                                    \
        old = __sync_fetch_and_or(location, 0);                             \
        current = op;                                                       \
    } while (atomicp_cmpxchg(location, current, old) != old);               \
                                                                            \
    return old;

Atomic atomic_set(volatile Atomic *location, Atomic value)
{
    ATOMIC_CMPXCHG(value);
}

Atomic atomic_add(volatile Atomic *location, Atomic value)
{
    return __sync_fetch_and_add(location, value);
}

Atomic atomic_sub(volatile Atomic *location, Atomic value)
{
    return __sync_fetch_and_sub(location, value);
}

Atomic atomic_and(volatile Atomic *location, Atomic value)
{
    return __sync_fetch_and_and(location, value);
}

Atomic atomic_or(volatile Atomic *location, Atomic value)
{
    return __sync_fetch_and_or(location, value);
}

Atomic atomic_xor(volatile Atomic *location, Atomic value)
{
    return __sync_fetch_and_xor(location, value);
}

Atomic atomic_load(const volatile Atomic *location)
{
    return __sync_fetch_and_or((volatile Atomic *) location, 0);
}

Atomic atomic_cmpxchg(volatile Atomic *location, Atomic value, Atomic compare)
{
    return __sync_val_compare_and_swap(location, compare, value);
}

int atomic_test_bit(const volatile Atomic *location, unsigned bit)
{
    return (atomic_load(location) >> bit) & 1;
}

int atomic_set_bit(volatile Atomic *location, unsigned bit)
{
    ATOMIC_CMPXCHG(old | ((Atomic) 1 << bit));
}

int atomic_clear_bit(volatile Atomic *location, unsigned bit)
{
    ATOMIC_CMPXCHG(old & ~((Atomic) 1 << bit));
}

int atomic_flip_bit(volatile Atomic *location, unsigned bit)
{
    ATOMIC_CMPXCHG(old ^ ((Atomic) 1 << bit));
}

AtomicPointer atomicp_set(volatile AtomicPointer *location, AtomicPointer value)
{
    ATOMICP_CMPXCHG(value);
}

AtomicPointer atomicp_add(volatile AtomicPointer *location, intptr_t value)
{
    ATOMICP_CMPXCHG((char *) old + value);
}

AtomicPointer atomicp_sub(volatile AtomicPointer *location, intptr_t value)
{
    ATOMICP_CMPXCHG((char *) old - value);
}

AtomicPointer atomicp_load(const volatile AtomicPointer *location)
{
    return __sync_fetch_and_or((volatile AtomicPointer *) location, 0);
}

AtomicPointer atomicp_cmpxchg(volatile AtomicPointer *location, AtomicPointer value, AtomicPointer compare)
{
    return __sync_val_compare_and_swap(location, compare, value);
}
#endif

int memswap(void *p, void *q, size_t size) {
    char *pchar = p, *qchar = q;

    while (size--)
    {
        char tmp = *pchar;
        *pchar++ = *qchar;
        *qchar++ = tmp;
    }

    return 0;
}

int memxor(void *dst, void *src, size_t size) {
    char *pdst = dst, *psrc = src;

    while (size--)
        *pdst++ ^= *psrc++;

    return 0;
}

/* Adapted from https://en.wikipedia.org/wiki/Modular_exponentiation#Right-to-left_binary_method */
static uint64_t exp_mod(uint64_t base, uint64_t exp, uint64_t mod) {
    uint64_t result = 1;

    for (base %= mod; exp; exp >>= 1)
    {
        if (exp & 1)
            result = (result * base) % mod;
        base = (base * base) % mod;
    }

    return result;
}

/* Adapted from https://en.wikipedia.org/wiki/Miller%E2%80%93Rabin_primality_test#Deterministic_variants */
int is_prime(size_t number) {
    size_t i, j;
    static int divisors[] = {3, 5, 7, 11, 13};

    if (number % 2 == 0)
        return 0;

    for (i = 0; i < sizeof(divisors)/sizeof(*divisors); ++i)
        if (number % divisors[i] == 0)
            return 0;

    static int tests[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
    size_t n;
    size_t r = 0;

    for (n = number - 1; (n & 1) == 0; n >>= 1)
        ++r;

    for (i = 0; i < sizeof(tests)/sizeof(*tests); ++i)
    {
        uint64_t x = exp_mod(tests[i], n, number);

        if (x == 1 || x == (number - 1))
            continue;

        for (j = 0; j < r; ++j)
        {
            x = (x * x) % number;
            if (x == (number - 1))
                goto next;
        }

        return 0;
next:;
    }

    return 1;
}

size_t next_prime(size_t number) {
    number |= 1;
    while (!is_prime(number) && number < 0xffffffffu)
        number += 2;

    return number < 0xffffffffu? number: 0;
}

static unsigned char *pearson_lookup_table() {
    /* A table of randomly shuffled values between 0-255 */
    /* Each value appears only once, but in a random position */
    static unsigned char lookup[256] = {
        0xF5, 0x97, 0x5D, 0xF0, 0xFD, 0xB2, 0x0D, 0x48,
        0xA7, 0x28, 0x72, 0x4B, 0xF1, 0x64, 0x68, 0x57,
        0x05, 0x29, 0x2E, 0x3E, 0xE3, 0x4D, 0x93, 0x85,
        0x19, 0x2C, 0xDC, 0x0C, 0x53, 0xB5, 0xAE, 0x2A,
        0xBB, 0x46, 0xD0, 0x6B, 0xD4, 0xC7, 0xA0, 0x83,
        0x96, 0xFE, 0x6C, 0xFB, 0xA6, 0xE7, 0x42, 0x81,
        0xF2, 0xF4, 0xE0, 0x7D, 0x76, 0x50, 0x4C, 0x8F,
        0x26, 0x77, 0xE5, 0xC3, 0xCF, 0x41, 0x84, 0x87,
        0xBD, 0x3A, 0x08, 0xED, 0xAB, 0xBC, 0x89, 0x18,
        0x12, 0xFA, 0x61, 0xDF, 0xD8, 0x31, 0xD5, 0x1E,
        0x4E, 0x1C, 0x40, 0xBE, 0x82, 0x6F, 0xF6, 0x65,
        0x62, 0x37, 0x11, 0x1B, 0xD9, 0x30, 0x33, 0xCB,
        0x3C, 0xF9, 0x98, 0xDB, 0x1F, 0xEE, 0x49, 0x58,
        0x45, 0xC5, 0xCC, 0x99, 0x9E, 0x4F, 0x34, 0x8A,
        0x7A, 0x3D, 0xE9, 0x5B, 0x60, 0x35, 0x8C, 0xF8,
        0x69, 0x1A, 0xA2, 0xE6, 0x0E, 0x55, 0x6A, 0x0F,
        0xF7, 0x1D, 0x27, 0x8E, 0x7C, 0x6E, 0xA9, 0x7F,
        0x38, 0x8D, 0xAC, 0xE4, 0xB8, 0xB1, 0xC1, 0x0B,
        0x17, 0x71, 0x44, 0xB9, 0xD6, 0x6D, 0xDA, 0x52,
        0x5A, 0xA4, 0x21, 0xBA, 0x16, 0xB0, 0xEF, 0x86,
        0xC9, 0xA8, 0xB7, 0xFF, 0x20, 0x56, 0xC6, 0xCE,
        0x91, 0x25, 0x24, 0x32, 0x5F, 0xB3, 0x74, 0x02,
        0xD1, 0x9D, 0x75, 0x95, 0xCA, 0x39, 0xA1, 0xC0,
        0x9A, 0xDD, 0xC4, 0x9F, 0x80, 0xAA, 0xCD, 0x13,
        0x09, 0x22, 0xB4, 0x3F, 0x2F, 0xB6, 0x9C, 0x79,
        0x7E, 0xD3, 0x2D, 0x07, 0x06, 0x88, 0x0A, 0x59,
        0x90, 0x94, 0x70, 0xAD, 0x9B, 0x63, 0x04, 0x5E,
        0xE8, 0xE1, 0xEA, 0x23, 0x66, 0x54, 0x4A, 0xC8,
        0x03, 0x3B, 0x8B, 0xDE, 0x47, 0x2B, 0xFC, 0x15,
        0x73, 0xA5, 0xD2, 0xE2, 0xA3, 0x5C, 0xEB, 0xF3,
        0xEC, 0x7B, 0x01, 0x36, 0x92, 0x00, 0x78, 0xBF,
        0x51, 0x67, 0xC2, 0x43, 0x10, 0xAF, 0x14, 0xD7
    };

    return lookup;
}

unsigned pearson_hash(const char *data, size_t size)
{
    const unsigned char *lookup = pearson_lookup_table();

    const size_t chunks = sizeof(unsigned int);
    unsigned char hash[sizeof(unsigned int)];
    size_t i, j;
    for (j = 0; j < chunks; ++j)
    {
        hash[j] = size & 0xff;
        hash[j] = lookup[(hash[j] ^ (data[0] + j)) & 0xff];
        for (i = 0; i < size; ++i)
            hash[j] = lookup[(hash[j] ^ data[i]) & 0xff];
    }

    unsigned result = 0;
    for (i = 0; i < chunks; ++i)
    {
        result <<= CHAR_BIT;
        result |= hash[i];
    }

    return result;
}

uint32_t rotate_left32(uint32_t v, unsigned amount) {
    return (v << amount) | (v >> (-amount & 0x1f));
}

uint32_t rotate_right32(uint32_t v, unsigned amount) {
    return (v >> amount) | (v << (-amount & 0x1f));
}

uint64_t rotate_left64(uint64_t v, unsigned amount) {
    return (v << amount) | (v >> (-amount & 0x3f));
}

uint64_t rotate_right64(uint64_t v, unsigned amount) {
    return (v >> amount) | (v << (-amount & 0x3f));
}

/* PUT FUNCTIONS */

/* NOTE: the big- and little-endian raw cast functions ONLY WORK
 * if unaligned access is allowed on the platform. Use the safe version otherwise */

/* Little-endian? */
#if (X86_CPU | AMD64_CPU) && CHAR_BIT == 8
void u32cpy_le(unsigned char *dst, uint32_t v) {
    memcpy(dst, &v, 4);
}
#else
void u32cpy_le(unsigned char *dst, uint32_t v) {
    dst[0] = v & 0xff;
    dst[1] = (v >> 8) & 0xff;
    dst[2] = (v >> 16) & 0xff;
    dst[3] = v >> 24;
}
#endif

/* Big-endian? */
#if 0
void u32cpy_be(unsigned char *dst, uint32_t v) {
    memcpy(dst, &v, 4);
}
#else
void u32cpy_be(unsigned char *dst, uint32_t v) {
    dst[0] = v >> 24;
    dst[1] = (v >> 16) & 0xff;
    dst[2] = (v >> 8) & 0xff;
    dst[3] = v & 0xff;
}
#endif

/* Little-endian? */
#if (X86_CPU | AMD64_CPU) && CHAR_BIT == 8
void u64cpy_le(unsigned char *dst, uint64_t v) {
    memcpy(dst, &v, 8);
}
#else
void u64cpy_le(unsigned char *dst, uint64_t v) {
    dst[0] = v & 0xff;
    dst[1] = (v >> 8) & 0xff;
    dst[2] = (v >> 16) & 0xff;
    dst[3] = (v >> 24) & 0xff;
    dst[4] = (v >> 32) & 0xff;
    dst[5] = (v >> 40) & 0xff;
    dst[6] = (v >> 48) & 0xff;
    dst[7] = v >> 56;
}
#endif

/* Big-endian? */
#if 0
void u64cpy_be(unsigned char *dst, uint64_t v) {
    memcpy(dst, &v, 8);
}
#else
void u64cpy_be(unsigned char *dst, uint64_t v) {
    dst[0] = v >> 56;
    dst[1] = (v >> 48) & 0xff;
    dst[2] = (v >> 40) & 0xff;
    dst[3] = (v >> 32) & 0xff;
    dst[4] = (v >> 24) & 0xff;
    dst[5] = (v >> 16) & 0xff;
    dst[6] = (v >> 8) & 0xff;
    dst[7] = v & 0xff;
}
#endif

/* GET FUNCTIONS */

/* Little-endian? */
#if (X86_CPU | AMD64_CPU) && CHAR_BIT == 8
uint32_t u32get_le(uint32_t *dst, unsigned char *src) {
    memcpy(dst, src, 4);
    return *dst;
}
#else
uint32_t u32get_le(uint32_t *dst, unsigned char *src) {
    return *dst = src[0] | ((uint32_t) src[1] << 8) | ((uint32_t) src[2] << 16) | ((uint32_t) src[3] << 24);
}
#endif

/* Big-endian? */
#if 0
uint32_t u32get_be(uint32_t *dst, unsigned char *src) {
    memcpy(dst, src, 4);
    return *dst;
}
#else
uint32_t u32get_be(uint32_t *dst, unsigned char *src) {
    return *dst = ((uint32_t) src[0] << 24) | ((uint32_t) src[1] << 16) | ((uint32_t) src[2] << 8) | src[3];
}
#endif

/* Little-endian? */
#if (X86_CPU | AMD64_CPU) && CHAR_BIT == 8
uint64_t u64get_le(uint64_t *dst, unsigned char *src) {
    memcpy(dst, src, 8);
    return *dst;
}
#else
uint64_t u64get_le(uint64_t *dst, unsigned char *src) {
    return *dst = src[0] | ((uint64_t) src[1] << 8) | ((uint64_t) src[2] << 16) | ((uint64_t) src[3] << 24) |
           ((uint64_t) src[4] << 32) | ((uint64_t) src[5] << 40) | ((uint64_t) src[6] << 48) | ((uint64_t) src[7] << 56);
}
#endif

/* Big-endian? */
#if 0
uint64_t u64get_be(uint64_t *dst, unsigned char *src) {
    memcpy(dst, src, 8);
    return *dst;
}
#else
uint64_t u64get_be(uint64_t *dst, unsigned char *src) {
    return *dst = ((uint64_t) src[0] << 56) | ((uint64_t) src[1] << 48) | ((uint64_t) src[2] << 40) | ((uint64_t) src[3] << 32) |
            ((uint64_t) src[4] << 24) | ((uint64_t) src[5] << 16) | ((uint64_t) src[6] << 8) | src[7];
}
#endif

#if X86_CPU | AMD64_CPU
int x86_cpuid(uint32_t function, uint32_t subfunction, uint32_t dst[4]) {
#if MSVC_COMPILER
    if (function > 0)
    {
        __cpuid((int *) (dst), function & 0x80000000);

        if (dst[0] < function)
        {
            memset(dst, 0, 16);
            return -1;
        }
    }

    __cpuidex((int *) (dst), function, subfunction);

    return 0;
#elif CLANG_COMPILER | GCC_COMPILER
    unsigned int _eax, _ebx, _ecx, _edx;

    if (function > 0)
    {
        __cpuid(function & 0x80000000, _eax, _ebx, _ecx, _edx);

        if (_eax < function)
        {
            memset(dst, 0, 16);
            return -1;
        }
    }

    __cpuid_count(function, subfunction, dst[0], dst[1], dst[2], dst[3]);

    return 0;
#else
    UNUSED(function)
    UNUSED(subfunction)
    UNUSED(dst)

    return -1;
#endif
}
#endif

#ifdef __cplusplus
}
#endif
