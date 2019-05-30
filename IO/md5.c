/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "md5.h"
#include "../utility.h"

#include <stdlib.h>

#define MD5_HASH_BYTES 16

static const uint32_t md5_table[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static const unsigned char md5_shift[64] = {
    7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
    5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
    4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
    6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
};

struct Md5 {
    IO io;
    uint32_t state[4];
    uint8_t buffer[64];
    size_t buffer_size;
    uint64_t message_len;

    /* Number of characters of hash read by md5_read */
    int read;
};

static void calculate_md5(struct Md5 *md5) {
    uint32_t wbuffer[16] = {0, 0, 0, 0,
                            0, 0, 0, 0,
                            0, 0, 0, 0,
                            0, 0, 0, 0};
    uint32_t mstate[4] = {md5->state[0], md5->state[1], md5->state[2], md5->state[3]};

    for (size_t i = 0; i < 16; ++i)
        u32get_le(&wbuffer[i], md5->buffer + i*4);

    for (size_t i = 0; i < 64; ++i)
    {
        uint32_t F;
        size_t g;

        if (i < 16)
        {
            F = (mstate[1] & mstate[2]) | (~mstate[1] & mstate[3]);
            g = i;
        }
        else if (i < 32)
        {
            F = (mstate[3] & mstate[1]) | (~mstate[3] & mstate[2]);
            g = 5*i + 1;
        }
        else if (i < 48)
        {
            F = mstate[1] ^ mstate[2] ^ mstate[3];
            g = 3*i + 5;
        }
        else
        {
            F = mstate[2] ^ (mstate[1] | ~mstate[3]);
            g = 7*i;
        }

        F += mstate[0] + md5_table[i] + wbuffer[g % 16];
        mstate[0] = mstate[3];
        mstate[3] = mstate[2];
        mstate[2] = mstate[1];
        mstate[1] += rotate_left32(F, md5_shift[i]);
    }

    for (size_t i = 0; i < 4; ++i)
        md5->state[i] += mstate[i];

    md5->buffer_size = 0;
}

static void end_md5(struct Md5 *md5) {
    md5->buffer[md5->buffer_size++] = 0x80;

    if (md5->buffer_size >= 56) {
        while (md5->buffer_size < 64)
            md5->buffer[md5->buffer_size++] = 0;
        calculate_md5(md5);
    }

    while (md5->buffer_size < 56)
        md5->buffer[md5->buffer_size++] = 0;

    u64cpy_le(md5->buffer + 56, md5->message_len);
    md5->buffer_size = 64;

    calculate_md5(md5);
}

static void *md5_open(void *userdata, IO io) {
    UNUSED(io)

    struct Md5 *result = calloc(1, sizeof(struct Md5));
    if (result == NULL)
        return NULL;

    result->io = userdata;
    result->state[0] = 0x67452301;
    result->state[1] = 0xefcdab89;
    result->state[2] = 0x98badcfe;
    result->state[3] = 0x10325476;

    return result;
}

static int md5_close(void *userdata, IO io) {
    int result = 0;
    struct Md5 *md5 = userdata;

    if (!io_readable(io)) {
        end_md5(md5);

        unsigned char state[MD5_HASH_BYTES];

        u32cpy_le(&state[0], md5->state[0]);
        u32cpy_le(&state[4], md5->state[1]);
        u32cpy_le(&state[8], md5->state[2]);
        u32cpy_le(&state[12], md5->state[3]);

        result = io_write(state, 4, 4, md5->io) != 4? -1: 0;
    }

    free(userdata);
    return result;
}

static size_t md5_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    if (size == 0 || count == 0)
        return 0;

    struct Md5 temp_md5;
    struct Md5 *md5 = userdata;
    size_t max = size*count;
    if (max > MD5_HASH_BYTES)
        max = MD5_HASH_BYTES - md5->read;

    if (!io_writable(io) && md5->message_len == 0) /* Using as pull parser; read entire input then hash (if hash was already calculated, message length is non-zero) */ {
        do {
            md5->buffer_size = io_read(md5->buffer, 1, 64, md5->io);
            if (md5->buffer_size == 64)
                calculate_md5(md5);
            else if (io_error(md5->io))
                return SIZE_MAX;

            md5->message_len += 8 * md5->buffer_size;
        } while (md5->buffer_size == 64);
    }

    temp_md5 = *md5;
    end_md5(md5 = &temp_md5);

    unsigned char state[MD5_HASH_BYTES];

    u32cpy_le(&state[0], md5->state[0]);
    u32cpy_le(&state[4], md5->state[1]);
    u32cpy_le(&state[8], md5->state[2]);
    u32cpy_le(&state[12], md5->state[3]);

    memcpy(ptr, state + md5->read, max);
    md5->read += max;

    return max;
}

static size_t md5_write(const void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    if (size == 0 || count == 0)
        return 0;

    struct Md5 *md5 = userdata;
    size_t max = size*count;
    const unsigned char *cptr = ptr;

    md5->message_len += 8 * max;
    md5->read = 0;
    while (max) {
        size_t copy = 64 - md5->buffer_size;
        if (copy > max)
            copy = max;

        memcpy(md5->buffer + md5->buffer_size, cptr, copy);
        md5->buffer_size += copy;

        cptr += copy;
        max -= copy;

        if (md5->buffer_size == 64)
            calculate_md5(md5);
    }

    return count;
}

static int md5_seek(void *userdata, long offset, int origin, IO io) {
    if (!io_readable(io))
        return -1;

    struct Md5 *md5 = userdata;

    switch (origin) {
        default: return -1;
        case SEEK_SET:
            if (offset < 0 || offset > MD5_HASH_BYTES)
                return -1;
            md5->read = offset;
            return 0;
        case SEEK_CUR:
            if (md5->read + offset < 0 || md5->read + offset > MD5_HASH_BYTES)
                return -1;
            md5->read += offset;
            return 0;
        case SEEK_END:
            if (offset > 0 || offset < -MD5_HASH_BYTES)
                return -1;
            md5->read = MD5_HASH_BYTES + offset;
            return 0;
    }
}

static const struct InputOutputDeviceCallbacks md5_callbacks = {
    .read = md5_read,
    .write = md5_write,
    .open = md5_open,
    .close = md5_close,
    .flush = NULL,
    .tell = NULL,
    .tell64 = NULL,
    .seek = md5_seek,
    .seek64 = NULL
};

IO io_open_md5(IO io, const char *mode) {
    return io_open_custom(&md5_callbacks, io, mode);
}
