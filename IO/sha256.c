/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "sha256.h"
#include "../utility.h"

#include <stdlib.h>

#if defined(__SSSE3__) && defined(__SHA__)
#define SHA256_COMPILE_SUPPORTS_X86_INTRINSICS
#endif
#define SHA256_HASH_BYTES 32

struct Sha256 {
    IO io;
    uint32_t state[8];
    uint8_t buffer[64];
    size_t buffer_size;
    uint64_t message_len;
    void (*calculate)(struct Sha256 *sha256);

    /* Number of characters of hash read by sha256_read */
    int read;
};

static uint32_t ktable[] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void calculate_sha256(struct Sha256 *sha256) {
    uint32_t wbuffer[80];
    uint32_t mstate[8] = {sha256->state[0], sha256->state[1], sha256->state[2], sha256->state[3],
                          sha256->state[4], sha256->state[5], sha256->state[6], sha256->state[7]};

    for (size_t i = 0; i < 16; ++i)
        u32get_be(&wbuffer[i], sha256->buffer + i*4);

    for (size_t i = 16; i < 80; ++i) {
        uint32_t s0 = rotate_right32(wbuffer[i-15], 7) ^ rotate_right32(wbuffer[i-15], 18) ^ (wbuffer[i-15] >> 3);
        uint32_t s1 = rotate_right32(wbuffer[i-2], 17) ^ rotate_right32(wbuffer[i-2], 19) ^ (wbuffer[i-2] >> 10);
        wbuffer[i] = wbuffer[i-16] + s0 + wbuffer[i-7] + s1;
    }

    for (size_t i = 0; i < 64; ++i)
    {
        uint32_t temp1 = mstate[7] + ktable[i] + wbuffer[i];
        temp1 += rotate_right32(mstate[4], 6) ^ rotate_right32(mstate[4], 11) ^ rotate_right32(mstate[4], 25);
        temp1 += (mstate[4] & mstate[5]) ^ (~mstate[4] & mstate[6]);

        uint32_t temp2 = rotate_right32(mstate[0], 2) ^ rotate_right32(mstate[0], 13) ^ rotate_right32(mstate[0], 22);
        temp2 += (mstate[0] & mstate[1]) ^ (mstate[0] & mstate[2]) ^ (mstate[1] & mstate[2]);

        mstate[7] = mstate[6];
        mstate[6] = mstate[5];
        mstate[5] = mstate[4];
        mstate[4] = mstate[3] + temp1;
        mstate[3] = mstate[2];
        mstate[2] = mstate[1];
        mstate[1] = mstate[0];
        mstate[0] = temp1 + temp2;
    }

    for (size_t i = 0; i < 8; ++i)
        sha256->state[i] += mstate[i];

    sha256->buffer_size = 0;
}

#ifdef SHA256_COMPILE_SUPPORTS_X86_INTRINSICS
/* Intel SHA extensions using C intrinsics                 */
/*   Written and place in public domain by Jeffrey Walton  */
/*   Based on code from Intel, and by Sean Gulley for      */
/*   the miTLS project.                                    */
/*                                                         */
/* See https://github.com/noloader/SHA-Intrinsics/blob/master/sha256-x86.c */
static void calculate_sha256_x86(struct Sha256 *sha256) {
#if 0
    __m128i ABCD, ABCD_SAVE, E0, E0_SAVE, E1;
    __m128i MSG0, MSG1, MSG2, MSG3;
    const __m128i MASK = _mm_set_epi64x(0x0001020304050607ULL, 0x08090a0b0c0d0e0fULL);

    /* Load initial values */
    ABCD = _mm_loadu_si128((const __m128i*) sha256->state);
    E0 = _mm_set_epi32(sha256->state[4], 0, 0, 0);
    ABCD = _mm_shuffle_epi32(ABCD, 0x1B);

    /* Save current state  */
    ABCD_SAVE = ABCD;
    E0_SAVE = E0;

    /* Rounds 0-3 */
    MSG0 = _mm_loadu_si128((const __m128i*)(sha256->buffer + 0));
    MSG0 = _mm_shuffle_epi8(MSG0, MASK);
    E0 = _mm_add_epi32(E0, MSG0);
    E1 = ABCD;
    ABCD = _mm_sha256rnds4_epu32(ABCD, E0, 0);

    /* Rounds 4-7 */
    MSG1 = _mm_loadu_si128((const __m128i*)(sha256->buffer + 16));
    MSG1 = _mm_shuffle_epi8(MSG1, MASK);
    E1 = _mm_sha256nexte_epu32(E1, MSG1);
    E0 = ABCD;
    ABCD = _mm_sha256rnds4_epu32(ABCD, E1, 0);
    MSG0 = _mm_sha256msg1_epu32(MSG0, MSG1);

    /* Rounds 8-11 */
    MSG2 = _mm_loadu_si128((const __m128i*)(sha256->buffer + 32));
    MSG2 = _mm_shuffle_epi8(MSG2, MASK);
    E0 = _mm_sha256nexte_epu32(E0, MSG2);
    E1 = ABCD;
    ABCD = _mm_sha256rnds4_epu32(ABCD, E0, 0);
    MSG1 = _mm_sha256msg1_epu32(MSG1, MSG2);
    MSG0 = _mm_xor_si128(MSG0, MSG2);

    /* Rounds 12-15 */
    MSG3 = _mm_loadu_si128((const __m128i*)(sha256->buffer + 48));
    MSG3 = _mm_shuffle_epi8(MSG3, MASK);
    E1 = _mm_sha256nexte_epu32(E1, MSG3);
    E0 = ABCD;
    MSG0 = _mm_sha256msg2_epu32(MSG0, MSG3);
    ABCD = _mm_sha256rnds4_epu32(ABCD, E1, 0);
    MSG2 = _mm_sha256msg1_epu32(MSG2, MSG3);
    MSG1 = _mm_xor_si128(MSG1, MSG3);

    /* Rounds 16-19 */
    E0 = _mm_sha256nexte_epu32(E0, MSG0);
    E1 = ABCD;
    MSG1 = _mm_sha256msg2_epu32(MSG1, MSG0);
    ABCD = _mm_sha256rnds4_epu32(ABCD, E0, 0);
    MSG3 = _mm_sha256msg1_epu32(MSG3, MSG0);
    MSG2 = _mm_xor_si128(MSG2, MSG0);

    /* Rounds 20-23 */
    E1 = _mm_sha256nexte_epu32(E1, MSG1);
    E0 = ABCD;
    MSG2 = _mm_sha256msg2_epu32(MSG2, MSG1);
    ABCD = _mm_sha256rnds4_epu32(ABCD, E1, 1);
    MSG0 = _mm_sha256msg1_epu32(MSG0, MSG1);
    MSG3 = _mm_xor_si128(MSG3, MSG1);

    /* Rounds 24-27 */
    E0 = _mm_sha256nexte_epu32(E0, MSG2);
    E1 = ABCD;
    MSG3 = _mm_sha256msg2_epu32(MSG3, MSG2);
    ABCD = _mm_sha256rnds4_epu32(ABCD, E0, 1);
    MSG1 = _mm_sha256msg1_epu32(MSG1, MSG2);
    MSG0 = _mm_xor_si128(MSG0, MSG2);

    /* Rounds 28-31 */
    E1 = _mm_sha256nexte_epu32(E1, MSG3);
    E0 = ABCD;
    MSG0 = _mm_sha256msg2_epu32(MSG0, MSG3);
    ABCD = _mm_sha256rnds4_epu32(ABCD, E1, 1);
    MSG2 = _mm_sha256msg1_epu32(MSG2, MSG3);
    MSG1 = _mm_xor_si128(MSG1, MSG3);

    /* Rounds 32-35 */
    E0 = _mm_sha256nexte_epu32(E0, MSG0);
    E1 = ABCD;
    MSG1 = _mm_sha256msg2_epu32(MSG1, MSG0);
    ABCD = _mm_sha256rnds4_epu32(ABCD, E0, 1);
    MSG3 = _mm_sha256msg1_epu32(MSG3, MSG0);
    MSG2 = _mm_xor_si128(MSG2, MSG0);

    /* Rounds 36-39 */
    E1 = _mm_sha256nexte_epu32(E1, MSG1);
    E0 = ABCD;
    MSG2 = _mm_sha256msg2_epu32(MSG2, MSG1);
    ABCD = _mm_sha256rnds4_epu32(ABCD, E1, 1);
    MSG0 = _mm_sha256msg1_epu32(MSG0, MSG1);
    MSG3 = _mm_xor_si128(MSG3, MSG1);

    /* Rounds 40-43 */
    E0 = _mm_sha256nexte_epu32(E0, MSG2);
    E1 = ABCD;
    MSG3 = _mm_sha256msg2_epu32(MSG3, MSG2);
    ABCD = _mm_sha256rnds4_epu32(ABCD, E0, 2);
    MSG1 = _mm_sha256msg1_epu32(MSG1, MSG2);
    MSG0 = _mm_xor_si128(MSG0, MSG2);

    /* Rounds 44-47 */
    E1 = _mm_sha256nexte_epu32(E1, MSG3);
    E0 = ABCD;
    MSG0 = _mm_sha256msg2_epu32(MSG0, MSG3);
    ABCD = _mm_sha256rnds4_epu32(ABCD, E1, 2);
    MSG2 = _mm_sha256msg1_epu32(MSG2, MSG3);
    MSG1 = _mm_xor_si128(MSG1, MSG3);

    /* Rounds 48-51 */
    E0 = _mm_sha256nexte_epu32(E0, MSG0);
    E1 = ABCD;
    MSG1 = _mm_sha256msg2_epu32(MSG1, MSG0);
    ABCD = _mm_sha256rnds4_epu32(ABCD, E0, 2);
    MSG3 = _mm_sha256msg1_epu32(MSG3, MSG0);
    MSG2 = _mm_xor_si128(MSG2, MSG0);

    /* Rounds 52-55 */
    E1 = _mm_sha256nexte_epu32(E1, MSG1);
    E0 = ABCD;
    MSG2 = _mm_sha256msg2_epu32(MSG2, MSG1);
    ABCD = _mm_sha256rnds4_epu32(ABCD, E1, 2);
    MSG0 = _mm_sha256msg1_epu32(MSG0, MSG1);
    MSG3 = _mm_xor_si128(MSG3, MSG1);

    /* Rounds 56-59 */
    E0 = _mm_sha256nexte_epu32(E0, MSG2);
    E1 = ABCD;
    MSG3 = _mm_sha256msg2_epu32(MSG3, MSG2);
    ABCD = _mm_sha256rnds4_epu32(ABCD, E0, 2);
    MSG1 = _mm_sha256msg1_epu32(MSG1, MSG2);
    MSG0 = _mm_xor_si128(MSG0, MSG2);

    /* Rounds 60-63 */
    E1 = _mm_sha256nexte_epu32(E1, MSG3);
    E0 = ABCD;
    MSG0 = _mm_sha256msg2_epu32(MSG0, MSG3);
    ABCD = _mm_sha256rnds4_epu32(ABCD, E1, 3);
    MSG2 = _mm_sha256msg1_epu32(MSG2, MSG3);
    MSG1 = _mm_xor_si128(MSG1, MSG3);

    /* Rounds 64-67 */
    E0 = _mm_sha256nexte_epu32(E0, MSG0);
    E1 = ABCD;
    MSG1 = _mm_sha256msg2_epu32(MSG1, MSG0);
    ABCD = _mm_sha256rnds4_epu32(ABCD, E0, 3);
    MSG3 = _mm_sha256msg1_epu32(MSG3, MSG0);
    MSG2 = _mm_xor_si128(MSG2, MSG0);

    /* Rounds 68-71 */
    E1 = _mm_sha256nexte_epu32(E1, MSG1);
    E0 = ABCD;
    MSG2 = _mm_sha256msg2_epu32(MSG2, MSG1);
    ABCD = _mm_sha256rnds4_epu32(ABCD, E1, 3);
    MSG3 = _mm_xor_si128(MSG3, MSG1);

    /* Rounds 72-75 */
    E0 = _mm_sha256nexte_epu32(E0, MSG2);
    E1 = ABCD;
    MSG3 = _mm_sha256msg2_epu32(MSG3, MSG2);
    ABCD = _mm_sha256rnds4_epu32(ABCD, E0, 3);

    /* Rounds 76-79 */
    E1 = _mm_sha256nexte_epu32(E1, MSG3);
    E0 = ABCD;
    ABCD = _mm_sha256rnds4_epu32(ABCD, E1, 3);

    /* Combine state */
    E0 = _mm_sha256nexte_epu32(E0, E0_SAVE);
    ABCD = _mm_add_epi32(ABCD, ABCD_SAVE);

    /* Save state */
    ABCD = _mm_shuffle_epi32(ABCD, 0x1B);
    _mm_storeu_si128((__m128i*) sha256->state, ABCD);
    sha256->state[4] = _mm_extract_epi32(E0, 3);

    sha256->buffer_size = 0;
#endif
}
#endif

static void end_sha256(struct Sha256 *sha256) {
    sha256->buffer[sha256->buffer_size++] = 0x80;

    if (sha256->buffer_size >= 56) {
        while (sha256->buffer_size < 64)
            sha256->buffer[sha256->buffer_size++] = 0;
        sha256->calculate(sha256);
    }

    while (sha256->buffer_size < 56)
        sha256->buffer[sha256->buffer_size++] = 0;

    u64cpy_le(sha256->buffer + 56, sha256->message_len);
    sha256->buffer_size = 64;

    sha256->calculate(sha256);
}

static void sha256_init_state(struct Sha256 *ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->message_len = 0;
}

static void *sha256_open(void *userdata, IO io) {
    UNUSED(io)

    struct Sha256 *result = CALLOC(1, sizeof(struct Sha256));
    if (result == NULL)
        return NULL;

    result->io = userdata;
    result->calculate = calculate_sha256;
    sha256_init_state(result);

#ifdef SHA256_COMPILE_SUPPORTS_X86_INTRINSICS
#if X86_CPU | AMD64_CPU
#if 0
    /* Detect SHA extensions support */
    uint32_t cpuid[4];
    if (0 == x86_cpuid(7, 0, cpuid) && TESTBIT(cpuid[1], 29))
        result->calculate = calculate_sha256_x86;
#endif
#endif
#endif

    return result;
}

static int sha256_close(void *userdata, IO io) {
    int result = 0;
    struct Sha256 *sha256 = userdata;

    if (!io_readable(io)) {
        end_sha256(sha256);

        unsigned char state[SHA256_HASH_BYTES];

        u32cpy_be(&state[0], sha256->state[0]);
        u32cpy_be(&state[4], sha256->state[1]);
        u32cpy_be(&state[8], sha256->state[2]);
        u32cpy_be(&state[12], sha256->state[3]);
        u32cpy_be(&state[16], sha256->state[4]);
        u32cpy_be(&state[20], sha256->state[5]);
        u32cpy_be(&state[24], sha256->state[6]);
        u32cpy_be(&state[28], sha256->state[7]);

        result = io_write(state, 4, 5, sha256->io) != 5? io_error(sha256->io): 0;
    }

    FREE(userdata);
    return result;
}

static size_t sha256_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    if (size == 0 || count == 0)
        return 0;

    struct Sha256 temp_sha256;
    struct Sha256 *sha256 = userdata, *usersha256 = userdata;
    size_t max = MIN(size*count, (size_t) SHA256_HASH_BYTES - sha256->read);

    if (!io_writable(io) && sha256->message_len == 0) /* Using as pull parser; read entire input then hash (if hash was already calculated, message length is non-zero) */ {
        do {
            sha256->buffer_size = io_read(sha256->buffer, 1, 64, sha256->io);
            if (sha256->buffer_size == 64)
                sha256->calculate(sha256);
            else if (io_error(sha256->io)) {
                io_set_error(io, io_error(sha256->io));
                return SIZE_MAX;
            }

            sha256->message_len += 8 * sha256->buffer_size;
        } while (sha256->buffer_size == 64);
    }

    temp_sha256 = *sha256;
    end_sha256(sha256 = &temp_sha256);

    unsigned char state[SHA256_HASH_BYTES];

    u32cpy_be(&state[0], sha256->state[0]);
    u32cpy_be(&state[4], sha256->state[1]);
    u32cpy_be(&state[8], sha256->state[2]);
    u32cpy_be(&state[12], sha256->state[3]);
    u32cpy_be(&state[16], sha256->state[4]);
    u32cpy_be(&state[20], sha256->state[5]);
    u32cpy_be(&state[24], sha256->state[6]);
    u32cpy_be(&state[28], sha256->state[7]);

    memcpy(ptr, state + sha256->read, max);
    usersha256->read += max;

    return max / size;
}

static size_t sha256_write(const void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    if (size == 0 || count == 0)
        return 0;

    struct Sha256 *sha256 = userdata;
    size_t max = size*count;
    const unsigned char *cptr = ptr;

    if (io_just_read(io) && !io_opened_for_update(io))
        sha256_init_state(sha256);

    sha256->message_len += 8 * max;
    sha256->read = 0;
    while (max) {
        size_t copy = 64 - sha256->buffer_size;
        if (copy > max)
            copy = max;

        memcpy(sha256->buffer + sha256->buffer_size, cptr, copy);
        sha256->buffer_size += copy;

        cptr += copy;
        max -= copy;

        if (sha256->buffer_size == 64)
            sha256->calculate(sha256);
    }

    return count;
}

static int sha256_state_switch(void *userdata, IO io) {
    UNUSED(io)

    struct Sha256 *sha256 = userdata;
    sha256_init_state(sha256);

    return 0;
}

static long sha256_tell(void *userdata, IO io) {
    UNUSED(io)

    struct Sha256 *sha256 = userdata;

    return sha256->read;
}

static int sha256_seek(void *userdata, long offset, int origin, IO io) {
    if (!io_readable(io))
        return -1;

    struct Sha256 *sha256 = userdata;

    switch (origin) {
        default: return -1;
        case SEEK_SET:
            if (offset < 0 || offset > SHA256_HASH_BYTES)
                return -1;
            sha256->read = offset;
            return 0;
        case SEEK_CUR:
            if (sha256->read + offset < 0 || sha256->read + offset > SHA256_HASH_BYTES)
                return -1;
            sha256->read += offset;
            return 0;
        case SEEK_END:
            if (offset > 0 || offset < -SHA256_HASH_BYTES)
                return -1;
            sha256->read = SHA256_HASH_BYTES + offset;
            return 0;
    }
}

static void sha256_clearerr(void *userdata, IO io) {
    UNUSED(io)

    struct Sha256 *sha256 = userdata;

    io_clearerr(sha256->io);
}

static const char *sha256_what(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    return "sha256";
}

static const struct InputOutputDeviceCallbacks sha256_callbacks = {
    .read = sha256_read,
    .write = sha256_write,
    .open = sha256_open,
    .close = sha256_close,
    .flush = NULL,
    .clearerr = sha256_clearerr,
    .stateSwitch = sha256_state_switch,
    .tell = sha256_tell,
    .tell64 = NULL,
    .seek = sha256_seek,
    .seek64 = NULL,
    .flags = NULL,
    .what = sha256_what
};

IO io_open_sha256(IO io, const char *mode) {
    IO result = io_open_custom(&sha256_callbacks, io, mode);

    if (result != NULL && strchr(mode, '<') != NULL)
        ((struct Sha256 *) io_userdata(result))->calculate = calculate_sha256;

    return result;
}
