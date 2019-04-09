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

/* Swaps a number of bytes in p and q, specified by the size parameter
 *
 * p and q must not overlap
 */
int memswap(void *p, void *q, size_t size);

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

void u32cpy_le(unsigned char *dst, uint32_t v);
void u32cpy_be(unsigned char *dst, uint32_t v);
void u64cpy_le(unsigned char *dst, uint64_t v);
void u64cpy_be(unsigned char *dst, uint64_t v);

void u32get_le(uint32_t *dst, unsigned char *src);
void u32get_be(uint32_t *dst, unsigned char *src);
void u64get_le(uint64_t *dst, unsigned char *src);
void u64get_be(uint64_t *dst, unsigned char *src);

#if X86_CPU | AMD64_CPU
/* Requires that `dst` be able to store at least 4 32-bit integers */
/* Returns 0 on success, non-zero on failure */
int x86_cpuid(uint32_t function, uint32_t subfunction, uint32_t *dst);
#endif

#define TESTBIT(x, bit) ((x) & (1ull << bit))

#ifdef __cplusplus
}
#endif

#endif /* CCUTILITY_H */
