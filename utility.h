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
typedef uint32_t Atomic;
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

/** @brief Spinlock type
 *
 * This type implements a spinlock, busy-waiting until the lock becomes available. There is no inherent thread-identification in the spinlock,
 * and should not be copied around by value (i.e. the spinlock should be static or should be a member type, or just use a pointer)
 */
typedef Atomic Spinlock;

/** @brief Initialize a spinlock.
 *
 * The spinlock is left in an unlocked state.
 *
 * @param spinlock The spinlock to initialize
 */
void spinlock_init(volatile Spinlock *spinlock);

/** @brief Lock a spinlock
 *
 * The thread blocks until the spinlock is available, and then locks it.
 * If the current thread already has the spinlock locked, a deadlock condition occurs.
 *
 * @param spinlock The spinlock to lock
 */
void spinlock_lock(volatile Spinlock *spinlock);

/** @brief Detects if a spinlock is locked
 *
 * @param spinlock The spinlock to check the lock state of
 * @return Returns 1 if the spinlock is currently locked, 0 if unlocked
 */
int spinlock_is_locked(volatile Spinlock *spinlock);

/** @brief Attempt to lock a spinlock
 *
 * The thread attempts to lock the spinlock, and returns immediately regardless of whether it could lock or not.
 * If the current thread already has the spinlock locked, 0 is returned.
 *
 * @param spinlock The spinlock to attempt to lock
 * @return 0 if the spinlock could not be locked, either because the current thread or another thread owns the lock, or 1 if a lock could be obtained.
 */
int spinlock_try_lock(volatile Spinlock *spinlock);

/** @brief Unlock a spinlock
 *
 * Unconditionally unlocks a spinlock. WARNING: Even if another thread has locked the spinlock, it will be unlocked after calling this function.
 *
 * @param spinlock The spinlock to unlock
 */
void spinlock_unlock(volatile Spinlock *spinlock);

struct MutexStruct;
typedef struct MutexStruct *Mutex;

/** @brief Creates a new mutex object
 *
 * Mutex objects are not shareable across processes, just threads within a process.
 *
 * @return A new non-recursive mutex object, or NULL if none could be created
 */
Mutex mutex_create();

/** @brief Creates a new mutex object
 *
 * Mutex objects are not shareable across processes, just threads within a process.
 *
 * @return A new recursive mutex object, or NULL if none could be created
 */
Mutex mutex_create_recursive();

/** @brief Checks if a mutex object is recursive
 *
 * @p mutex The mutex to check
 * @return 1 if @p mutex is a recursive mutex, 0 if @p mutex is non-recursive
 */
int mutex_is_recursive(Mutex mutex);

/** @brief Locks a mutex.
 *
 * If the mutex is currently locked by another thread, the current thread will block until it can get ownership of the mutex.
 *
 * If the current thread already has the mutex locked, a deadlock will occur unless the mutex is recursive.
 *
 * If @p mutex is a recursive mutex, the mutex will be relocked and each call to `mutex_lock()` must be accompanied by a call to `mutex_unlock()`.
 *
 * @param mutex The mutex object to lock
 */
void mutex_lock(Mutex mutex);

/** @brief Attempts to lock a mutex.
 *
 * The thread attempts to lock the mutex, and returns immediately regardless of whether it could lock or not.
 * If the current thread already has the mutex locked, the result is 0.
 *
 * @param mutex The mutex object to attempt to lock
 * @return 1 if the mutex was able to be locked, 0 if another thread has currently locked the mutex.
 *         If the current thread locked the mutex, and @p mutex is non-recursive, 0 is returned.
 *         If the current thread locked the mutex, and @p mutex is recursive, 1 is returned.
 */
int mutex_try_lock(Mutex mutex);

/** @brief Unlock a mutex
 *
 * Unlocks a mutex that was locked by the current thread. If @p mutex is currently locked by a thread other than the current thread,
 * the behavior is undefined.
 *
 * @param mutex The mutex object to unlock
 */
void mutex_unlock(Mutex mutex);

/** @brief Destroys a mutex object
 *
 * Destroys the mutex object and frees all resources associated with it.
 * The mutex must be destroyed while in the unlocked state, or the side effects are is undefined.
 *
 * @param mutex The mutex object to destroy
 */
void mutex_destroy(Mutex mutex);

typedef void *Thread;

typedef int (*ThreadStartFn)(void *);
typedef int (*ThreadStartFnNoArgs)(void);

Thread thread_create(ThreadStartFn fn, void *args);
Thread thread_create_no_args(ThreadStartFnNoArgs fn);

int thread_is_current(Thread t);

void thread_yield(void);

int thread_join(Thread t, int *result);

int thread_detach(Thread t);

void thread_close(Thread t);

void thread_exit(int result);

/** @brief Sleeps for @p tm milliseconds (thread_sleep), microseconds (thread_usleep), or nanoseconds (thread_nsleep).
 *
 * Note that the resolution of the system timer may not actually achieve microsecond or nanosecond precision.
 *
 * @param tm The time period to sleep for.
 */
void thread_sleep(unsigned long long tm);
void thread_usleep(unsigned long long tm);
void thread_nsleep(unsigned long long tm);

/** @brief Swaps two memory blocks.
 *
 * Swaps the contents of two memory blocks.
 *
 * @param p One of the arrays to swap.
 * @param q One of the arrays to swap.
 * @param size The number of bytes to swap between @p p and @p q.
 * @return Always returns 0.
 */
int memswap(void *p, void *q, size_t size);

/** @brief Performs an XOR of two memory blocks.
 *
 * Performs an XOR operation for every byte of @p src with @p dst, and stores the result in @p dst.
 *
 * @param dst The destination to XOR with and write to.
 * @param src The source to XOR with @p dst.
 * @param size The number of bytes to operate on.
 * @return Always returns 0.
 */
int memxor(void *dst, void *src, size_t size);

#define UTF8_MAX (0x10ffff)
#define UTF8_MASK (0x1fffff)

/** @brief Gets the size in bytes that a codepoint would require to be encoded in UTF-8.
 *
 * @param codepoint The codepoint to get the encoding size of.
 * @return The number of bytes it would take to encode @p codepoint as UTF-8.
 */
int utf8size(uint32_t codepoint);

/** @brief Gets the length of the UTF-8 string in characters
 *
 * @param utf8 The UTF-8 string to get the length of.
 * @return The length of @p utf8 in UTF-8 characters.
 */
size_t utf8len(const char *utf8);

/** @brief Determines if and where an encoding error appears in a UTF-8 string.
 *
 * @param utf8 The UTF-8 string to check for an error.
 * @return If an error is found, a pointer to the error location is returned. On success, NULL is returned.
 */
const char *utf8error(const char *utf8);

/** @brief Finds a UTF-8 character in a UTF-8 string.
 *
 * @param utf8 The UTF-8 string to search.
 * @param chr The UTF-8 character codepoint to search for.
 * @return Same as `strchr`. If the character is found, a pointer to the first instance of that character is returned, otherwise NULL is returned.
 */
const char *utf8chr(const char *utf8, uint32_t chr);

/** @brief Gets the codepoint of the first character in a UTF-8 string.
 *
 * @param utf8 The UTF-8 string to extract the first character from.
 * @param next A reference to a pointer that stores the position of the character following the one extracted. Can be NULL if unused.
 * @return The codepoint of the first UTF-8 character in @p utf8, or 0x8000fffd on error.
 *    This constant allows detection of error by performing `result > UTF8_MAX`,
 *    and providing the Unicode replacement character 0xfffd if masked with UTF8_MASK.
 */
uint32_t utf8next(const char *utf8, const char **next);

/** @brief Attempts to concatenate a codepoint to a UTF-8 string.
 *
 * @param utf8 The UTF-8 string to append to. This must point to the NUL character at the end of the string.
 * @param codepoint The codepoint to append to @p utf8.
 * @param remainingBytes A pointer to the number of bytes remaining in the buffer after @p utf8.
 *     The NUL that @p utf8 points to must not be included in this available size.
 *     This field is updated with the number of bytes available after the function returns.
 * @return On success, a pointer to the NUL at the end of the string is returned, otherwise NULL is returned and nothing is appended.
 */
char *utf8append(char *utf8, uint32_t codepoint, size_t *remainingBytes);

/** @brief Lowercases the ASCII string.
 *
 * @param str The string to lowercase.
 * @return Returns @p str.
 */
char *strlower(char *str);

/** @brief Uppercases the ASCII string.
 *
 * @param str The string to uppercase.
 * @return Returns @p str.
 */
char *strupper(char *str);

/** @brief Determines whether a string begins with another string
 *
 * @param str The string to compare
 * @param substr The substring to compare to the beginning of @p str
 * @return Returns 1 if @p string starts with @p substr, 0 otherwise
 */
int str_starts_with(const char *str, const char *substr);

/** @brief Compares two case insensitive strings.
 *
 * Behavior is identical to strcmp(), except ASCII characters are compared case insensitively.
 *
 * @param lhs One string to compare.
 * @param rhs The other string to compare.
 * @return Returns < 0 if lhs < rhs lexicographically, > 0 if lhs > rhs lexicographically, and 0 if the strings are equal (case insensitively).
 */
int strcmp_no_case(const char *lhs, const char *rhs);

/** @brief Concatenates an array of strings into one allocated string.
 * 
 * @param strings The array of strings to concatenate.
 * @param stringsCount The number of entries in the strings array.
 * @param separator A string to insert between every gathered element.
 * @return A newly allocated string that contains the concatenated strings, or `NULL` if out of memory.
 */
char *strjoin_alloc(const char *strings[], size_t stringsCount, const char *separator);

/** @brief Allocates a duplicate of a NUL-terminated string.
 *
 * @param str The string to copy. This parameter must not be NULL.
 * @return A newly allocated string that contains a copy of @p str.
 */
char *strdup_alloc(const char *str);
#ifndef strdup
#define strdup strdup_alloc
#endif

/** @brief Performs a Pearson hash on specified data.
 *
 * Performs a Pearson hash on an arbitrary amount of data, using a pseudo-randomly shuffled hash table.
 * The output is uniformly-distributed if the input is uniformly-distributed too.
 *
 * @param data points to the data to hash.
 * @param size is the size in bytes of the data to hash.
 * @return The Pearson hash of the data.
 */
unsigned pearson_hash(const char *data, size_t size);

/** @brief Detects whether a number is prime.
 *
 * This function is deterministic, i.e. the number is definitely prime if this function says so.
 * `is_prime()` is specifically designed for use for testing hash table sizes for primality.
 *
 * @param number is the number to test for primality.
 * @return 1 if @p number is prime, 0 if @p number is composite.
 */
int is_prime(size_t number);

/** @brief Attempts to find the lowest prime number not less than @p number.
 *
 * @param number is the number to begin testing for primality from.
 * @return If the result would be greater than 32 bits wide, @p number itself is returned.
 *         Otherwise, the lowest prime not less than @p number is returned.
 */
size_t next_prime(size_t number);

/** @brief Rotates @p v left by @p amount bits.
 *
 *  @param v is the 32-bit number to rotate.
 *  @param amount is the number of bits to rotate @p v by. @p amount @bold must be limited to the range [0, 32)
 *  @return @p v, rotated left (toward the MSB) by @p amount bits
 */
uint32_t rotate_left32(uint32_t v, unsigned amount);

/** @brief Rotates @p v right by @p amount bits.
 *
 *  @param v is the 32-bit number to rotate.
 *  @param amount is the number of bits to rotate @p v by. @p amount @bold must be limited to the range [0, 32)
 *  @return @p v, rotated right (toward the LSB) by @p amount bits
 */
uint32_t rotate_right32(uint32_t v, unsigned amount);

/** @brief Rotates @p v left by @p amount bits.
 *
 *  @param v is the 64-bit number to rotate.
 *  @param amount is the number of bits to rotate @p v by. @p amount @bold must be limited to the range [0, 64)
 *  @return @p v, rotated left (toward the MSB) by @p amount bits
 */
uint64_t rotate_left64(uint64_t v, unsigned amount);

/** @brief Rotates @p v right by @p amount bits.
 *
 *  @param v is the 64-bit number to rotate.
 *  @param amount is the number of bits to rotate @p v by. @p amount @bold must be limited to the range [0, 64)
 *  @return @p v, rotated right (toward the LSB) by @p amount bits
 */
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
 * Requires that `dst` be able to store at least 4 32-bit integers.
 *
 *  @param function is the main leaf to extract data from with the `CPUID` function.
 *  @param subfunction is the sub-leaf to extract data from with the `CPUID` function. If the main leaf does not require one, just use 0.
 *  @param dst is the location to store the extracted data in. The indexes 0, 1, 2, and 3 reference EAX, EBX, ECX, and EDX, respectively.
 *  @return 0 on success, non-zero on failure to complete the operation.
 */
int x86_cpuid(uint32_t function, uint32_t subfunction, uint32_t dst[4]);
#elif ARM64_CPU
#define CPU_FLAG_SUPPORTS_AES HWCAP_AES
#define CPU_FLAG_SUPPORTS_CRC32 HWCAP_CRC32
#define CPU_FLAG_SUPPORTS_PMULL HWCAP_PMULL
#define CPU_FLAG_SUPPORTS_SHA1 HWCAP_SHA1
#define CPU_FLAG_SUPPORTS_SHA2 HWCAP_SHA2

unsigned long arm_cpuid(void);
#elif ARM_CPU
#define CPU_FLAG_SUPPORTS_AES HWCAP2_AES
#define CPU_FLAG_SUPPORTS_CRC32 HWCAP2_CRC32
#define CPU_FLAG_SUPPORTS_PMULL HWCAP2_PMULL
#define CPU_FLAG_SUPPORTS_SHA1 HWCAP2_SHA1
#define CPU_FLAG_SUPPORTS_SHA2 HWCAP2_SHA2

unsigned long arm_cpuid(void);
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

/** @brief Performs an overflow-safe addition of two size_t values.
 *
 * @param u The first number to add.
 * @param v The second number to add.
 * @return The sum resulting from adding u and v, or 0 if overflow would occur.
 */
size_t safe_add(size_t u, size_t v);

#if WINDOWS_OS
int str_is_codepage_safe(LPCSTR utf8);
LPWSTR utf8_to_wide_alloc(LPCSTR utf8);
LPWSTR utf8_to_wide_alloc_additional(LPCSTR utf8, size_t additional);
LPSTR wide_to_utf8_alloc(LPCWSTR wide);
#endif

#ifdef __cplusplus
}
#endif

#endif /* CCUTILITY_H */
