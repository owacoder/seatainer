/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "sha1.h"
#include "../utility.h"

#include <stdlib.h>

#if defined(__SSSE3__) && defined(__SHA__)
#define SHA1_COMPILE_SUPPORTS_X86_INTRINSICS
#endif
#define SHA1_HASH_BYTES 20

struct Sha1 {
    IO io;
    uint32_t state[5];
    uint8_t buffer[64];
    size_t buffer_size;
    uint64_t message_len;
    void (*calculate)(struct Sha1 *sha1);

    /* Number of characters of hash read by sha1_read */
    int read;
};

static void calculate_sha1(struct Sha1 *sha1) {
    uint32_t wbuffer[80] = {0, 0, 0, 0,
                            0, 0, 0, 0,
                            0, 0, 0, 0,
                            0, 0, 0, 0};
    uint32_t mstate[5] = {sha1->state[0], sha1->state[1], sha1->state[2], sha1->state[3], sha1->state[4]};

    for (size_t i = 0; i < 16; ++i)
        u32get_be(&wbuffer[i], sha1->buffer + i*4);

    for (size_t i = 16; i < 80; ++i)
        wbuffer[i] = rotate_left32(wbuffer[i-3] ^ wbuffer[i-8] ^ wbuffer[i-14] ^ wbuffer[i-16], 1);

    for (size_t i = 0; i < 80; ++i)
    {
        uint32_t F, k, t;

        if (i < 20)
        {
            F = (mstate[1] & mstate[2]) | (~mstate[1] & mstate[3]);
            k = 0x5a827999;
        }
        else if (i < 40)
        {
            F = mstate[1] ^ mstate[2] ^ mstate[3];
            k = 0x6ed9eba1;
        }
        else if (i < 60)
        {
            F = (mstate[1] & mstate[2]) | (mstate[1] & mstate[3]) | (mstate[2] & mstate[3]);
            k = 0x8f1bbcdc;
        }
        else
        {
            F = mstate[1] ^ mstate[2] ^ mstate[3];
            k = 0xca62c1d6;
        }

        t = rotate_left32(mstate[0], 5) + F + mstate[4] + k + wbuffer[i];
        mstate[4] = mstate[3];
        mstate[3] = mstate[2];
        mstate[2] = rotate_left32(mstate[1], 30);
        mstate[1] = mstate[0];
        mstate[0] = t;
    }

    for (size_t i = 0; i < 5; ++i)
        sha1->state[i] += mstate[i];

    sha1->buffer_size = 0;
}

#ifdef SHA1_COMPILE_SUPPORTS_X86_INTRINSICS
/* Intel SHA extensions using C intrinsics                 */
/*   Written and place in public domain by Jeffrey Walton  */
/*   Based on code from Intel, and by Sean Gulley for      */
/*   the miTLS project.                                    */
/*                                                         */
/* See https://github.com/noloader/SHA-Intrinsics/blob/master/sha1-x86.c */
static void calculate_sha1_x86(struct Sha1 *sha1) {
    __m128i ABCD, ABCD_SAVE, E0, E0_SAVE, E1;
    __m128i MSG0, MSG1, MSG2, MSG3;
    const __m128i MASK = _mm_set_epi64x(0x0001020304050607ULL, 0x08090a0b0c0d0e0fULL);

    /* Load initial values */
    ABCD = _mm_loadu_si128((const __m128i*) sha1->state);
    E0 = _mm_set_epi32(sha1->state[4], 0, 0, 0);
    ABCD = _mm_shuffle_epi32(ABCD, 0x1B);

    /* Save current state  */
    ABCD_SAVE = ABCD;
    E0_SAVE = E0;

    /* Rounds 0-3 */
    MSG0 = _mm_loadu_si128((const __m128i*)(sha1->buffer + 0));
    MSG0 = _mm_shuffle_epi8(MSG0, MASK);
    E0 = _mm_add_epi32(E0, MSG0);
    E1 = ABCD;
    ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 0);

    /* Rounds 4-7 */
    MSG1 = _mm_loadu_si128((const __m128i*)(sha1->buffer + 16));
    MSG1 = _mm_shuffle_epi8(MSG1, MASK);
    E1 = _mm_sha1nexte_epu32(E1, MSG1);
    E0 = ABCD;
    ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 0);
    MSG0 = _mm_sha1msg1_epu32(MSG0, MSG1);

    /* Rounds 8-11 */
    MSG2 = _mm_loadu_si128((const __m128i*)(sha1->buffer + 32));
    MSG2 = _mm_shuffle_epi8(MSG2, MASK);
    E0 = _mm_sha1nexte_epu32(E0, MSG2);
    E1 = ABCD;
    ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 0);
    MSG1 = _mm_sha1msg1_epu32(MSG1, MSG2);
    MSG0 = _mm_xor_si128(MSG0, MSG2);

    /* Rounds 12-15 */
    MSG3 = _mm_loadu_si128((const __m128i*)(sha1->buffer + 48));
    MSG3 = _mm_shuffle_epi8(MSG3, MASK);
    E1 = _mm_sha1nexte_epu32(E1, MSG3);
    E0 = ABCD;
    MSG0 = _mm_sha1msg2_epu32(MSG0, MSG3);
    ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 0);
    MSG2 = _mm_sha1msg1_epu32(MSG2, MSG3);
    MSG1 = _mm_xor_si128(MSG1, MSG3);

    /* Rounds 16-19 */
    E0 = _mm_sha1nexte_epu32(E0, MSG0);
    E1 = ABCD;
    MSG1 = _mm_sha1msg2_epu32(MSG1, MSG0);
    ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 0);
    MSG3 = _mm_sha1msg1_epu32(MSG3, MSG0);
    MSG2 = _mm_xor_si128(MSG2, MSG0);

    /* Rounds 20-23 */
    E1 = _mm_sha1nexte_epu32(E1, MSG1);
    E0 = ABCD;
    MSG2 = _mm_sha1msg2_epu32(MSG2, MSG1);
    ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 1);
    MSG0 = _mm_sha1msg1_epu32(MSG0, MSG1);
    MSG3 = _mm_xor_si128(MSG3, MSG1);

    /* Rounds 24-27 */
    E0 = _mm_sha1nexte_epu32(E0, MSG2);
    E1 = ABCD;
    MSG3 = _mm_sha1msg2_epu32(MSG3, MSG2);
    ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 1);
    MSG1 = _mm_sha1msg1_epu32(MSG1, MSG2);
    MSG0 = _mm_xor_si128(MSG0, MSG2);

    /* Rounds 28-31 */
    E1 = _mm_sha1nexte_epu32(E1, MSG3);
    E0 = ABCD;
    MSG0 = _mm_sha1msg2_epu32(MSG0, MSG3);
    ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 1);
    MSG2 = _mm_sha1msg1_epu32(MSG2, MSG3);
    MSG1 = _mm_xor_si128(MSG1, MSG3);

    /* Rounds 32-35 */
    E0 = _mm_sha1nexte_epu32(E0, MSG0);
    E1 = ABCD;
    MSG1 = _mm_sha1msg2_epu32(MSG1, MSG0);
    ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 1);
    MSG3 = _mm_sha1msg1_epu32(MSG3, MSG0);
    MSG2 = _mm_xor_si128(MSG2, MSG0);

    /* Rounds 36-39 */
    E1 = _mm_sha1nexte_epu32(E1, MSG1);
    E0 = ABCD;
    MSG2 = _mm_sha1msg2_epu32(MSG2, MSG1);
    ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 1);
    MSG0 = _mm_sha1msg1_epu32(MSG0, MSG1);
    MSG3 = _mm_xor_si128(MSG3, MSG1);

    /* Rounds 40-43 */
    E0 = _mm_sha1nexte_epu32(E0, MSG2);
    E1 = ABCD;
    MSG3 = _mm_sha1msg2_epu32(MSG3, MSG2);
    ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 2);
    MSG1 = _mm_sha1msg1_epu32(MSG1, MSG2);
    MSG0 = _mm_xor_si128(MSG0, MSG2);

    /* Rounds 44-47 */
    E1 = _mm_sha1nexte_epu32(E1, MSG3);
    E0 = ABCD;
    MSG0 = _mm_sha1msg2_epu32(MSG0, MSG3);
    ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 2);
    MSG2 = _mm_sha1msg1_epu32(MSG2, MSG3);
    MSG1 = _mm_xor_si128(MSG1, MSG3);

    /* Rounds 48-51 */
    E0 = _mm_sha1nexte_epu32(E0, MSG0);
    E1 = ABCD;
    MSG1 = _mm_sha1msg2_epu32(MSG1, MSG0);
    ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 2);
    MSG3 = _mm_sha1msg1_epu32(MSG3, MSG0);
    MSG2 = _mm_xor_si128(MSG2, MSG0);

    /* Rounds 52-55 */
    E1 = _mm_sha1nexte_epu32(E1, MSG1);
    E0 = ABCD;
    MSG2 = _mm_sha1msg2_epu32(MSG2, MSG1);
    ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 2);
    MSG0 = _mm_sha1msg1_epu32(MSG0, MSG1);
    MSG3 = _mm_xor_si128(MSG3, MSG1);

    /* Rounds 56-59 */
    E0 = _mm_sha1nexte_epu32(E0, MSG2);
    E1 = ABCD;
    MSG3 = _mm_sha1msg2_epu32(MSG3, MSG2);
    ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 2);
    MSG1 = _mm_sha1msg1_epu32(MSG1, MSG2);
    MSG0 = _mm_xor_si128(MSG0, MSG2);

    /* Rounds 60-63 */
    E1 = _mm_sha1nexte_epu32(E1, MSG3);
    E0 = ABCD;
    MSG0 = _mm_sha1msg2_epu32(MSG0, MSG3);
    ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 3);
    MSG2 = _mm_sha1msg1_epu32(MSG2, MSG3);
    MSG1 = _mm_xor_si128(MSG1, MSG3);

    /* Rounds 64-67 */
    E0 = _mm_sha1nexte_epu32(E0, MSG0);
    E1 = ABCD;
    MSG1 = _mm_sha1msg2_epu32(MSG1, MSG0);
    ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 3);
    MSG3 = _mm_sha1msg1_epu32(MSG3, MSG0);
    MSG2 = _mm_xor_si128(MSG2, MSG0);

    /* Rounds 68-71 */
    E1 = _mm_sha1nexte_epu32(E1, MSG1);
    E0 = ABCD;
    MSG2 = _mm_sha1msg2_epu32(MSG2, MSG1);
    ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 3);
    MSG3 = _mm_xor_si128(MSG3, MSG1);

    /* Rounds 72-75 */
    E0 = _mm_sha1nexte_epu32(E0, MSG2);
    E1 = ABCD;
    MSG3 = _mm_sha1msg2_epu32(MSG3, MSG2);
    ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 3);

    /* Rounds 76-79 */
    E1 = _mm_sha1nexte_epu32(E1, MSG3);
    E0 = ABCD;
    ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 3);

    /* Combine state */
    E0 = _mm_sha1nexte_epu32(E0, E0_SAVE);
    ABCD = _mm_add_epi32(ABCD, ABCD_SAVE);

    /* Save state */
    ABCD = _mm_shuffle_epi32(ABCD, 0x1B);
    _mm_storeu_si128((__m128i*) sha1->state, ABCD);
    sha1->state[4] = _mm_extract_epi32(E0, 3);

    sha1->buffer_size = 0;
}
#endif

static void end_sha1(struct Sha1 *sha1) {
    sha1->buffer[sha1->buffer_size++] = 0x80;

    if (sha1->buffer_size >= 56) {
        while (sha1->buffer_size < 64)
            sha1->buffer[sha1->buffer_size++] = 0;
        sha1->calculate(sha1);
    }

    while (sha1->buffer_size < 56)
        sha1->buffer[sha1->buffer_size++] = 0;

    u64cpy_le(sha1->buffer + 56, sha1->message_len);
    sha1->buffer_size = 64;

    sha1->calculate(sha1);
}

static void *sha1_open(void *userdata, IO io) {
    UNUSED(io)

    struct Sha1 *result = CALLOC(1, sizeof(struct Sha1));
    if (result == NULL)
        return NULL;

    result->io = userdata;
    result->calculate = calculate_sha1;
    result->state[0] = 0x67452301;
    result->state[1] = 0xefcdab89;
    result->state[2] = 0x98badcfe;
    result->state[3] = 0x10325476;
    result->state[4] = 0xc3d2e1f0;

#ifdef SHA1_COMPILE_SUPPORTS_X86_INTRINSICS
#if X86_CPU | AMD64_CPU
    /* Detect SHA extensions support */
    uint32_t cpuid[4];
    if (0 == x86_cpuid(7, 0, cpuid) && TESTBIT(cpuid[1], 29))
        result->calculate = calculate_sha1_x86;
#endif
#endif

    return result;
}

static int sha1_close(void *userdata, IO io) {
    int result = 0;
    struct Sha1 *sha1 = userdata;

    if (!io_readable(io)) {
        end_sha1(sha1);

        unsigned char state[SHA1_HASH_BYTES];

        u32cpy_be(&state[0], sha1->state[0]);
        u32cpy_be(&state[4], sha1->state[1]);
        u32cpy_be(&state[8], sha1->state[2]);
        u32cpy_be(&state[12], sha1->state[3]);
        u32cpy_be(&state[16], sha1->state[4]);

        result = io_write(state, 4, 5, sha1->io) != 5? io_error(sha1->io): 0;
    }

    FREE(userdata);
    return result;
}

static size_t sha1_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    if (size == 0 || count == 0)
        return 0;

    struct Sha1 temp_sha1;
    struct Sha1 *sha1 = userdata;
    size_t max = size*count;
    if (max > SHA1_HASH_BYTES)
        max = SHA1_HASH_BYTES - sha1->read;

    if (!io_writable(io) && sha1->message_len == 0) /* Using as pull parser; read entire input then hash (if hash was already calculated, message length is non-zero) */ {
        do {
            sha1->buffer_size = io_read(sha1->buffer, 1, 64, sha1->io);
            if (sha1->buffer_size == 64)
                sha1->calculate(sha1);
            else if (io_error(sha1->io)) {
                io_set_error(io, io_error(sha1->io));
                return SIZE_MAX;
            }

            sha1->message_len += 8 * sha1->buffer_size;
        } while (sha1->buffer_size == 64);
    }

    temp_sha1 = *sha1;
    end_sha1(sha1 = &temp_sha1);

    unsigned char state[SHA1_HASH_BYTES];

    u32cpy_be(&state[0], sha1->state[0]);
    u32cpy_be(&state[4], sha1->state[1]);
    u32cpy_be(&state[8], sha1->state[2]);
    u32cpy_be(&state[12], sha1->state[3]);
    u32cpy_be(&state[16], sha1->state[4]);

    memcpy(ptr, state + sha1->read, max);
    sha1->read += max;

    return max;
}

static size_t sha1_write(const void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    if (size == 0 || count == 0)
        return 0;

    struct Sha1 *sha1 = userdata;
    size_t max = size*count;
    const unsigned char *cptr = ptr;

    sha1->message_len += 8 * max;
    sha1->read = 0;
    while (max) {
        size_t copy = 64 - sha1->buffer_size;
        if (copy > max)
            copy = max;

        memcpy(sha1->buffer + sha1->buffer_size, cptr, copy);
        sha1->buffer_size += copy;

        cptr += copy;
        max -= copy;

        if (sha1->buffer_size == 64)
            sha1->calculate(sha1);
    }

    return count;
}

static int sha1_seek(void *userdata, long offset, int origin, IO io) {
    if (!io_readable(io))
        return -1;

    struct Sha1 *sha1 = userdata;

    switch (origin) {
        default: return -1;
        case SEEK_SET:
            if (offset < 0 || offset > SHA1_HASH_BYTES)
                return -1;
            sha1->read = offset;
            return 0;
        case SEEK_CUR:
            if (sha1->read + offset < 0 || sha1->read + offset > SHA1_HASH_BYTES)
                return -1;
            sha1->read += offset;
            return 0;
        case SEEK_END:
            if (offset > 0 || offset < -SHA1_HASH_BYTES)
                return -1;
            sha1->read = SHA1_HASH_BYTES + offset;
            return 0;
    }
}

static void sha1_clearerr(void *userdata, IO io) {
    UNUSED(io)

    struct Sha1 *sha1 = userdata;

    io_clearerr(sha1->io);
}

static const char *sha1_what(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    return "sha1";
}

static const struct InputOutputDeviceCallbacks sha1_callbacks = {
    .read = sha1_read,
    .write = sha1_write,
    .open = sha1_open,
    .close = sha1_close,
    .flush = NULL,
    .clearerr = sha1_clearerr,
    .stateSwitch = NULL,
    .tell = NULL,
    .tell64 = NULL,
    .seek = sha1_seek,
    .seek64 = NULL,
    .what = sha1_what
};

IO io_open_sha1(IO io, const char *mode) {
    return io_open_custom(&sha1_callbacks, io, mode);
}
