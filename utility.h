/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef CCUTILITY_H
#define CCUTILITY_H

#include "platforms.h"

#include <memory.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if WINDOWS_OS
typedef LONG Atomic;
typedef LPVOID AtomicPointer;
#elif LINUX_OS
typedef long Atomic;
typedef void *AtomicPointer;
#endif

/* All atomic functions return value of atomic *before* the operation was performed */
/* All atomic types must be aligned on their respective native boundary to be reliable */
Atomic atomic_set(volatile Atomic *location, Atomic value);
Atomic atomic_load(volatile const Atomic *location);
Atomic atomic_add(volatile Atomic *location, Atomic value);
Atomic atomic_sub(volatile Atomic *location, Atomic value);
Atomic atomic_and(volatile Atomic *location, Atomic value);
Atomic atomic_or(volatile Atomic *location, Atomic value);
Atomic atomic_xor(volatile Atomic *location, Atomic value);
Atomic atomic_cmpxchg(volatile Atomic *location, Atomic value, Atomic compare);

int atomic_test_bit(volatile const Atomic *location, unsigned bit);
int atomic_set_bit(volatile Atomic *location, unsigned bit);
int atomic_clear_bit(volatile Atomic *location, unsigned bit);
int atomic_flip_bit(volatile Atomic *location, unsigned bit);

AtomicPointer atomicp_set(volatile AtomicPointer *location, AtomicPointer value);
AtomicPointer atomicp_load(volatile const AtomicPointer *location);
AtomicPointer atomicp_add(volatile AtomicPointer *location, intptr_t value);
AtomicPointer atomicp_sub(volatile AtomicPointer *location, intptr_t value);
AtomicPointer atomicp_cmpxchg(volatile AtomicPointer *location, AtomicPointer value, AtomicPointer compare);

int memswap(void *p, void *q, size_t size);

int memxor(void *dst, void *src, size_t size);

/** @brief Lowercases the string.
 *
 * @param str The string to lowercase.
 * @return Returns @p str.
 */
char *strlower(char *str);

/** @brief Uppercases the string.
 *
 * @param str The string to uppercase.
 * @return Returns @p str.
 */
char *strupper(char *str);

/** @brief Gathers, or concatenates, an array of strings into one allocated string.
 * 
 * @param strings The array of strings to concatenate.
 * @param stringsCount The number of entries in the strings array.
 * @result A newly allocated string that contains the concatenated strings, or `NULL` if out of memory.
 */
char *strgather(char *strings[], size_t stringsCount);

/* Performs a Pearson hash on the specified data, with a pseudo-random shuffle of hash bytes */
unsigned pearson_hash(const char *data, size_t size);

/* Returns 1 if number is prime, 0 otherwise */
int is_prime(size_t number);

/* Returns next prime number greater than `number` */
size_t next_prime(size_t number);

uint32_t rotate_left32(uint32_t v, unsigned amount);
uint32_t rotate_right32(uint32_t v, unsigned amount);
uint64_t rotate_left64(uint64_t v, unsigned amount);
uint64_t rotate_right64(uint64_t v, unsigned amount);

/** @brief Copies @p v safely into @p dst as a little-endian value.
 *
 *  @param dst is the location to store the little-endian representation of @p v in. The buffer pointed to by @p dst must be at least 4 bytes long.
 *  @param v is the 32-bit number to copy.
 */
void u32cpy_le(unsigned char *dst, uint32_t v);

/** @brief Copies @p v safely into @p dst as a big-endian value.
 *
 *  @param dst is the location to store the big-endian representation of @p v in. The buffer pointed to by @p dst must be at least 4 bytes long.
 *  @param v is the 32-bit number to copy.
 */
void u32cpy_be(unsigned char *dst, uint32_t v);

/** @brief Copies @p v safely into @p dst as a little-endian value.
 *
 *  @param dst is the location to store the little-endian representation of @p v in. The buffer pointed to by @p dst must be at least 8 bytes long.
 *  @param v is the 64-bit number to copy.
 */
void u64cpy_le(unsigned char *dst, uint64_t v);

/** @brief Copies @p v safely into @p dst as a big-endian value.
 *
 *  @param dst is the location to store the big-endian representation of @p v in. The buffer pointed to by @p dst must be at least 8 bytes long.
 *  @param v is the 64-bit number to copy.
 */
void u64cpy_be(unsigned char *dst, uint64_t v);

/** @brief Copies @p src safely into @p dst as a little-endian value.
 *
 *  @param dst is the location to store @p src in.
 *  @param src is the little-endian representation of the 32-bit number to copy. The buffer pointed to by @p src must be at least 4 bytes long.
 *  @return The value that was read into @p dst.
 */
uint32_t u32get_le(uint32_t *dst, unsigned char *src);

/** @brief Copies @p src safely into @p dst as a big-endian value.
 *
 *  @param dst is the location to store @p src in.
 *  @param src is the big-endian representation of the 32-bit number to copy. The buffer pointed to by @p src must be at least 4 bytes long.
 *  @return The value that was read into @p dst.
 */
uint32_t u32get_be(uint32_t *dst, unsigned char *src);

/** @brief Copies @p src safely into @p dst as a little-endian value.
 *
 *  @param dst is the location to store @p src in.
 *  @param src is the little-endian representation of the 64-bit number to copy. The buffer pointed to by @p src must be at least 8 bytes long.
 *  @return The value that was read into @p dst.
 */
uint64_t u64get_le(uint64_t *dst, unsigned char *src);

/** @brief Copies @p src safely into @p dst as a big-endian value.
 *
 *  @param dst is the location to store @p src in.
 *  @param src is the big-endian representation of the 64-bit number to copy. The buffer pointed to by @p src must be at least 8 bytes long.
 *  @return The value that was read into @p dst.
 */
uint64_t u64get_be(uint64_t *dst, unsigned char *src);

#if X86_CPU | AMD64_CPU
/** @brief Obtains x86 CPU information.
 *
 *  @param function is the main leaf to extract data from with the `CPUID` function.
 *  @param subfunction is the sub-leaf to extract data from with the `CPUID` function. If the main leaf does not require one, just use 0.
 *  @param dst is the location to store the extracted data in. The indexes 0, 1, 2, and 3 reference EAX, EBX, ECX, and EDX, respectively.
 *  @return 0 on success, non-zero on failure to complete the operation.
 */
/* Requires that `dst` be able to store at least 4 32-bit integers */
/* Returns 0 on success, non-zero on failure */
int x86_cpuid(uint32_t function, uint32_t subfunction, uint32_t dst[4]);
#endif

/** @brief Tests a specific bit number @p bit in the parameter @p x.
 *
 *  @param x is an integral value to test.
 *  @param bit is the bit position to test.
 *  @return Non-zero if the bit at the specified location is set, zero if the bit is cleared.
 */
#define TESTBIT(x, bit) ((x) & (1ull << bit))

/** @brief Performs an overflow-safe multiplication of two size_t values.
 *
 *  @param u The first number to multiply.
 *  @param v The second number to multiply.
 *  @return The product resulting from multiplying u and v, or 0 if overflow would occur.
 */
size_t safe_multiply(size_t u, size_t v);

#if WINDOWS_OS
int str_is_codepage_safe(LPCSTR utf8);
LPWSTR utf8_to_wide_alloc(LPCSTR utf8);
LPSTR wide_to_utf8_alloc(LPCWSTR wide);
#endif

#ifdef __cplusplus
}
#endif

#endif /* CCUTILITY_H */
