/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "utility.h"
#include "platforms.h"

#include <limits.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>

#if LINUX_OS
#include <pthread.h>
#endif

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

static Atomic atomic_lock_acquire(const volatile Atomic *location)
{
    return atomic_cmpxchg(location, 1, 0);
}

static void atomic_lock_release(volatile Atomic *location)
{
    atomic_set(location, 0);
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

static Atomic atomic_lock_acquire(volatile Atomic *location)
{
    return __sync_lock_test_and_set(location, 1);
}

static void atomic_lock_release(volatile Atomic *location)
{
    __sync_lock_release(location);
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

void spinlock_init(volatile Spinlock *spinlock) {
    atomic_set((volatile Atomic *) spinlock, 0);
}

void spinlock_lock(volatile Spinlock *spinlock) {
    while (atomic_lock_acquire((volatile Atomic *) spinlock)) {
#if X86_CPU | AMD64_CPU
        /* Special implementation for x86/amd64, see https://wiki.osdev.org/Spinlock */
        while (*spinlock)
            __builtin_ia32_pause();
#endif
    }
}

int spinlock_try_lock(volatile Spinlock *spinlock) {
    return !atomic_lock_acquire((volatile Atomic *) spinlock);
}

void spinlock_unlock(volatile Spinlock *spinlock) {
    atomic_lock_release((volatile Atomic *) spinlock);
}

Mutex *mutex_create() {
#if LINUX_OS
    pthread_mutex_t *mutex = MALLOC(sizeof(pthread_mutex_t));
    if (mutex == NULL)
        return NULL;

    if (pthread_mutex_init(mutex, NULL) != 0) {
        FREE(mutex);
        return NULL;
    }

    return (Mutex *) mutex;
#elif WINDOWS_OS
    CRITICAL_SECTION *mutex = MALLOC(sizeof(CRITICAL_SECTION));
    if (mutex == NULL)
        return NULL;

    if (InitializeCriticalSectionAndSpinCount(mutex, 0x400) == 0) {
        FREE(mutex);
        return NULL;
    }

    return (Mutex *) mutex;
#else
    return CALLOC(sizeof(Spinlock), 1);
#endif
}

void mutex_lock(Mutex *mutex) {
#if LINUX_OS
    pthread_mutex_lock((pthread_mutex_t *) mutex);
#elif WINDOWS_OS
    EnterCriticalSection((CRITICAL_SECTION *) mutex);
#else
    return spinlock_lock((Spinlock *) mutex);
#endif
}

int mutex_try_lock(Mutex *mutex) {
#if LINUX_OS
    return pthread_mutex_trylock((pthread_mutex_t *) mutex) == 0;
#elif WINDOWS_OS
    return TryEnterCriticalSection((CRITICAL_SECTION *) mutex);
#else
    return spinlock_try_lock((Spinlock *) mutex);
#endif
}

void mutex_unlock(Mutex *mutex) {
#if LINUX_OS
    pthread_mutex_unlock((pthread_mutex_t *) mutex);
#elif WINDOWS_OS
    LeaveCriticalSection((CRITICAL_SECTION *) mutex);
#else
    return spinlock_unlock((Spinlock *) mutex);
#endif
}

void mutex_destroy(Mutex *mutex) {
#if LINUX_OS
    pthread_mutex_destroy((pthread_mutex_t *) mutex);
#elif WINDOWS_OS
    DeleteCriticalSection((CRITICAL_SECTION *) mutex);
#endif

    FREE(mutex);
}

/**
 * @brief Swaps a number of bytes in @p p and @p q.
 *
 * The memory spans must not overlap.
 *
 * @param p is a pointer to a memory block to swap. Must not be `NULL`.
 * @param q is a pointer to a second memory block to swap. Must not be `NULL`.
 * @param size is the number of the bytes to swap.
 * @return This function always returns 0.
 */
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

/**
 * @brief XORs a number of bytes in @p p and @p q.
 *
 * The memory spans must not overlap.
 *
 * @param dst is a pointer to the destination memory block to XOR. Must not be `NULL`.
 * @param src is a pointer to the source memory block to XOR. Must not be `NULL`.
 * @param size is the number of the bytes to XOR.
 * @return This function always returns 0.
 */
int memxor(void *dst, void *src, size_t size) {
    char *pdst = dst, *psrc = src;

    for (; size; --size)
        *pdst++ ^= *psrc++;

    return 0;
}

void thread_sleep(unsigned long milliseconds) {
#if LINUX_OS
    usleep(milliseconds * 1000);
#elif WINDOWS_OS
    Sleep(milliseconds);
#endif
}

int utf8size(uint32_t codepoint) {
    if (codepoint < 0x80) return 1;
    else if (codepoint < 0x800) return 2;
    else if (codepoint < 0x10000) return 3;
    else if (codepoint < 0x110000) return 4;
    else return 0;
}

const char *utf8error(const char *utf8) {
    while (*utf8) {
        const char *current = utf8;
        if (utf8next(utf8, &utf8) > UTF8_MAX)
            return current;
    }

    return NULL;
}

const char *utf8chr(const char *utf8, uint32_t chr) {
    while (*utf8) {
        const char *current = utf8;
        if (utf8next(utf8, &utf8) == chr)
            return current;
    }

    return NULL;
}

size_t utf8len(const char *utf8) {
    size_t len = 0;

    while (*utf8) {
        utf8next(utf8, &utf8);
    }

    return len;
}

uint32_t utf8next(const char *utf8, const char **next) {
    static const unsigned char high5BitsToByteCount[32] = {
        1, /* 00000, valid single byte */
        1, /* 00001, valid single byte */
        1, /* 00010, valid single byte */
        1, /* 00011, valid single byte */
        1, /* 00100, valid single byte */
        1, /* 00101, valid single byte */
        1, /* 00110, valid single byte */
        1, /* 00111, valid single byte */
        1, /* 01000, valid single byte */
        1, /* 01001, valid single byte */
        1, /* 01010, valid single byte */
        1, /* 01011, valid single byte */
        1, /* 01100, valid single byte */
        1, /* 01101, valid single byte */
        1, /* 01110, valid single byte */
        1, /* 01111, valid single byte */
        0, /* 10000, invalid continuation byte */
        0, /* 10001, invalid continuation byte */
        0, /* 10010, invalid continuation byte */
        0, /* 10011, invalid continuation byte */
        0, /* 10100, invalid continuation byte */
        0, /* 10101, invalid continuation byte */
        0, /* 10110, invalid continuation byte */
        0, /* 10111, invalid continuation byte */
        2, /* 11000, 2-byte code */
        2, /* 11001, 2-byte code */
        2, /* 11010, 2-byte code */
        2, /* 11011, 2-byte code */
        3, /* 11100, 3-byte code */
        3, /* 11101, 3-byte code */
        4, /* 11110, 4-byte code */
        0, /* 11111, invalid byte (should never be seen) */
    };

    const uint32_t error = 0x8000fffdul;
    int bytesInCode = high5BitsToByteCount[(unsigned char) *utf8 >> 3];
    uint32_t codepoint = 0;

    if (next)
        *next = utf8 + 1;

    if (bytesInCode == 0) /* Some sort of error */
        return error;
    else if (bytesInCode == 1) /* Shortcut for single byte, since there's no way to fail */
        return *utf8;

    /* Shift out high byte-length bits and begin result */
    codepoint = (unsigned char) *utf8 & (0xff >> bytesInCode);

    /* Obtain continuation bytes */
    for (int i = 1; i < bytesInCode; ++i) {
        if ((utf8[i] & 0xC0) != 0x80) /* Invalid continuation byte (note this handles a terminating NUL just fine) */
            return error;

        codepoint = (codepoint << 6) | (utf8[i] & 0x3f);
    }

    /* Syntax is good, now check for overlong encoding and invalid values */
    if (utf8size(codepoint) != bytesInCode || /* Overlong encoding */
            (codepoint >= 0xd800 && codepoint <= 0xdfff) || /* Not supposed to allow UTF-16 surrogates */
            codepoint > UTF8_MAX) /* Too large of a codepoint */
        return error;

    /* Finished without error */
    if (next)
        *next = utf8 + bytesInCode;
    return codepoint;
}

char *utf8append(char *utf8, uint32_t codepoint, size_t *remainingBytes) {
    static const unsigned char headerForCodepointSize[5] = {
        0x80, /* 10000000 for continuation byte */
        0x00, /* 00000000 for single byte */
        0xC0, /* 11000000 for 2-byte */
        0xE0, /* 11100000 for 3-byte */
        0xF0, /* 11110000 for 4-byte */
    };
    const size_t bytesInCode = utf8size(codepoint);
    const size_t continuationBytesInCode = bytesInCode - 1;

    if (bytesInCode == 0 || *remainingBytes < bytesInCode ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff)) /* Invalid codepoint or no room for encoding */
        return NULL;

    *utf8++ = headerForCodepointSize[bytesInCode] | (unsigned char) (codepoint >> (continuationBytesInCode * 6));

    for (size_t i = continuationBytesInCode; i > 0; --i) {
        *utf8++ = 0x80 | (0x3f & (codepoint >> ((i-1) * 6)));
    }

    *remainingBytes -= bytesInCode;
    *utf8 = 0;
    return utf8;
}

char *strlower(char *str) {
    char *ptr = str;

    for (; *ptr; ++ptr)
        *ptr = tolower(*ptr & UCHAR_MAX);

    return str;
}

char *strupper(char *str) {
    char *ptr = str;

    for (; *ptr; ++ptr)
        *ptr = toupper(*ptr & UCHAR_MAX);

    return str;
}

int strcmp_no_case(const char *lhs, const char *rhs) {
    while (*lhs && *rhs) {
        int l = tolower(*lhs++);
        int r = tolower(*rhs++);

        if (l < r)
            return -1;
        else if (r < l)
            return 1;
    }

    if (*lhs)
        return 1;
    else if (*rhs)
        return -1;

    return 0;
}

char *strgather(const char *strings[], size_t stringsCount) {
    size_t totalLen = 0;
    
    for (size_t i = 0; i < stringsCount; ++i) {
        totalLen += strlen(strings[i]);
    }
    
    char *result = MALLOC(totalLen + 1), *ptr = result;
    if (result == NULL)
        return NULL;
        
    for (size_t i = 0; i < stringsCount; ++i) {
        size_t len = strlen(strings[i]);
        memcpy(ptr, strings[i], len);
        ptr += len;
    }
    
    *ptr = 0;
    return result;
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

/* The following is adapted from https://en.wikipedia.org/wiki/Miller%E2%80%93Rabin_primality_test#Deterministic_variants */
/**
 * @brief Detects whether a number is prime.
 *
 * This function is deterministic, i.e. the number is definitely prime if this function says so.
 * `is_prime()` is specifically designed for use for testing hash table sizes for primality.
 *
 * @param number is the number to test for primality.
 * @return 1 if @p number is prime, 0 if @p number is composite.
 */
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

/**
 * @brief Attempts to find the lowest prime number not less than @p number.
 *
 * @param number is the number to begin testing for primality from.
 * @return If the result would be greater than 32 bits wide, @p number itself is returned.
 *         Otherwise, the lowest prime not less than @p number is returned.
 */
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

/**
 * @brief Performs a Pearson hash on specified data.
 *
 * Performs a Pearson hash on an arbitrary amount of data, using a pseudo-randomly shuffled hash table.
 * The output is uniformly-distributed if the input is uniformly-distributed too.
 *
 * @param data points to the data to hash.
 * @param size is the size in bytes of the data to hash.
 * @return The Pearson hash of the data.
 */
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

/** @brief Rotates @p v left by @p amount bits.
 *
 *  @param v is the 32-bit number to rotate.
 *  @param amount is the number of bits to rotate @p v by. @p amount @bold must be limited to the range [0, 32)
 *  @return @p v, rotated left (toward the MSB) by @p amount bits
 */
uint32_t rotate_left32(uint32_t v, unsigned amount) {
    return (v << amount) | (v >> ((~amount + 1) & 0x1f));
}

/** @brief Rotates @p v right by @p amount bits.
 *
 *  @param v is the 32-bit number to rotate.
 *  @param amount is the number of bits to rotate @p v by. @p amount @bold must be limited to the range [0, 32)
 *  @return @p v, rotated right (toward the LSB) by @p amount bits
 */
uint32_t rotate_right32(uint32_t v, unsigned amount) {
    return (v >> amount) | (v << ((~amount + 1) & 0x1f));
}

/** @brief Rotates @p v left by @p amount bits.
 *
 *  @param v is the 64-bit number to rotate.
 *  @param amount is the number of bits to rotate @p v by. @p amount @bold must be limited to the range [0, 64)
 *  @return @p v, rotated left (toward the MSB) by @p amount bits
 */
uint64_t rotate_left64(uint64_t v, unsigned amount) {
    return (v << amount) | (v >> ((~amount + 1) & 0x3f));
}

/** @brief Rotates @p v right by @p amount bits.
 *
 *  @param v is the 64-bit number to rotate.
 *  @param amount is the number of bits to rotate @p v by. @p amount @bold must be limited to the range [0, 64)
 *  @return @p v, rotated right (toward the LSB) by @p amount bits
 */
uint64_t rotate_right64(uint64_t v, unsigned amount) {
    return (v >> amount) | (v << ((~amount + 1) & 0x3f));
}

/* PUT FUNCTIONS */

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

size_t safe_multiply(size_t u, size_t v) {
#if SIZE_MAX == UINT32_MAX
    unsigned long long result = (unsigned long long) u * (unsigned long long) v;

    return result >> 32? 0: result;
#elif CLANG_COMPILER | GCC_COMPILER
    __uint128_t result = (__uint128_t) u * (__uint128_t) v;

    return result >> 64? 0: result;
#else
    size_t result = u * v;

    if (u != 0 && result / u != v)
        return 0;

    return result;
#endif
}

#if WINDOWS_OS
int str_is_codepage_safe(LPCSTR utf8) {
    if (utf8 == NULL)
        return 1;

    for (; *utf8; ++utf8)
        if ((unsigned char) *utf8 >= 0x80)
            return 0;

    return 1;
}

LPWSTR utf8_to_wide_alloc(LPCSTR utf8) {
    if (utf8 == NULL)
        return NULL;

    int chars = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, utf8, -1, NULL, 0);

    LPWSTR result = MALLOC(sizeof(*result) * chars);
    if (!result)
        return NULL;

    MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, utf8, -1, result, chars);

    return result;
}

LPSTR wide_to_utf8_alloc(LPCWSTR wide) {
    if (wide == NULL)
        return NULL;

    int bytes = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);

    LPSTR result = MALLOC(bytes);
    if (!result)
        return NULL;

    WideCharToMultiByte(CP_UTF8, 0, wide, -1, result, bytes, NULL, NULL);

    return result;
}
#endif

#ifdef __cplusplus
}
#endif
