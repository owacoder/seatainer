/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "hex.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

/* The first character of io_tempdata() stores the previous nibble when decoding, and the next nibble when encoding
 * If this value is 16, then no value is present
 */

static void *hex_open(void *userdata, IO io) {
    *io_tempdata(io) = 16;

    return userdata;
}

static size_t hex_decode_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    size_t max = size*count;
    unsigned char *cptr = ptr;
    const char alpha[] = "0123456789abcdef";

    unsigned char *nibble = io_tempdata(io);

    while (max) {
        int ch = io_getc((IO) userdata);
        if (ch == EOF)
            break;

        char *sptr = strchr(alpha, tolower(ch));
        if (sptr == NULL) {
#if WINDOWS_OS
            io_set_error(io, ERROR_INVALID_DATA);
#else
            io_set_error(io, EBADMSG);
#endif
            return SIZE_MAX;
        }

        ch = sptr - alpha;

        if (*nibble == 16)
            *nibble = ch;
        else {
            *cptr++ = (*nibble << 4) | ch;
            *nibble = 16;
            --max;
        }
    }

    io_set_error(io, io_error((IO) userdata));
    return io_error((IO) userdata)? SIZE_MAX: (cptr - (unsigned char *) ptr) / size;
}

static size_t hex_encode_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    size_t max = size*count;
    unsigned char *cptr = ptr;
    const char alpha[] = "0123456789abcdef";

    unsigned char *nibble = io_tempdata(io);

    for (; max; --max, ++cptr) {
        if (*nibble != 16) {
            *cptr = alpha[*nibble];
            *nibble = 16;
        } else {
            int ch = io_getc((IO) userdata);
            if (ch == EOF)
                break;

            *nibble = ch & 0xf;
            *cptr = alpha[ch >> 4];
        }
    }

    io_set_error(io, io_error((IO) userdata));
    return (cptr - (unsigned char *) ptr) / size;
}

static size_t hex_decode_write(const void *ptr, size_t size, size_t count, void *userdata, IO io) {
    const char alpha[] = "0123456789abcdef";
    const unsigned char *cptr = ptr;
    size_t max = size*count;

    unsigned char *nibble = io_tempdata(io);

    for (; max; --max, ++cptr) {
        char *sptr = strchr(alpha, *cptr);
        if (sptr == NULL)
            break;

        if (*nibble == 16)
            *nibble = sptr - alpha;
        else {
            if (io_putc((*nibble << 4) | (sptr - alpha), (IO) userdata) == EOF)
                break;
            *nibble = 16;
        }
    }

    io_set_error(io, io_error((IO) userdata));
    return (cptr - (unsigned char *) ptr) / (size);
}

static size_t hex_encode_write(const void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    const char alpha[] = "0123456789abcdef";
    const unsigned char *cptr = ptr;
    size_t max = size*count;

    for (; max; --max, ++cptr) {
        if (io_putc(alpha[*cptr >> 4], (IO) userdata) == EOF ||
            io_putc(alpha[*cptr & 0xf], (IO) userdata) == EOF)
            break;
    }

    io_set_error(io, io_error((IO) userdata));
    return (cptr - (unsigned char *) ptr) / size;
}

static int hex_flush(void *userdata, IO io) {
    UNUSED(io)

    return io_flush((IO) userdata);
}

static int hex_encode_seek64(void *userdata, long long int offset, int origin, IO io) {
    /* Translate all origins to SEEK_SET for ease of computation */
    switch (origin) {
        case SEEK_END: {
            long long underlyingSize = io_size64((IO) userdata) * 2;
            if (underlyingSize < 0)
                return -1;

            offset += underlyingSize;
            break;
        }
        case SEEK_CUR: {
            long long current = io_tell64(io);
            if (current < 0)
                return -1;

            offset += current;
            break;
        }
    }

    if (io_seek64((IO) userdata, offset / 2, SEEK_SET) != 0)
        return -1;

    *io_tempdata(io) = 16;

    if (offset & 1) /* Odd offset means read one character */
        if (io_getc(io) == EOF)
            return -1;

    return 0;
}

static int hex_decode_seek64(void *userdata, long long int offset, int origin, IO io) {
    /* Translate all origins to SEEK_SET for ease of computation */
    switch (origin) {
        case SEEK_END: {
            long long underlyingSize = io_size64((IO) userdata) / 2;
            if (underlyingSize < 0)
                return -1;

            offset += underlyingSize;
            break;
        }
        case SEEK_CUR: {
            long long current = io_tell64(io);
            if (current < 0)
                return -1;

            offset += current;
            break;
        }
    }

    if (io_seek64((IO) userdata, offset * 2, SEEK_SET) != 0)
        return -1;

    *io_tempdata(io) = 16;

    return 0;
}

static long long hex_encode_tell64(void *userdata, IO io) {
    UNUSED(io)

    long long value = io_tell64((IO) userdata);
    return value > 0? value * 2 - (*io_tempdata(io) < 16): value;
}

static long long hex_decode_tell64(void *userdata, IO io) {
    UNUSED(io)

    long long value = io_tell64((IO) userdata);
    return value > 0? value / 2 + (*io_tempdata(io) < 16): value;
}

static const char *hex_encode_what(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    return "hex_encode";
}

static const char *hex_decode_what(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    return "hex_decode";
}

static const struct InputOutputDeviceCallbacks hex_encode_callbacks = {
    .read = hex_encode_read,
    .write = hex_encode_write,
    .open = hex_open,
    .close = NULL,
    .flush = hex_flush,
    .stateSwitch = NULL,
    .tell = NULL,
    .tell64 = hex_encode_tell64,
    .seek = NULL,
    .seek64 = hex_encode_seek64,
    .what = hex_encode_what
};

static const struct InputOutputDeviceCallbacks hex_decode_callbacks = {
    .read = hex_decode_read,
    .write = hex_decode_write,
    .open = hex_open,
    .close = NULL,
    .flush = hex_flush,
    .stateSwitch = NULL,
    .tell = NULL,
    .tell64 = hex_decode_tell64,
    .seek = NULL,
    .seek64 = hex_decode_seek64,
    .what = hex_decode_what
};

IO io_open_hex_encode(IO io, const char *mode) {
    return io_open_custom(&hex_encode_callbacks, io, mode);
}

IO io_open_hex_decode(IO io, const char *mode) {
    return io_open_custom(&hex_decode_callbacks, io, mode);
}

void test_hex() {
    IO buffer = io_open_cstring("A string to try encoding", "r");
    IO encode = io_open_hex_encode(buffer, "r");
    IO decode = io_open_hex_decode(encode, "r");
    IO out = io_open_file(stdout);

    if (io_copy(decode, out) != 0)
        puts("Some sort of error occured when copying streams");
    puts("");

    decode = io_open_hex_decode(out, "w");
    encode = io_open_hex_encode(decode, "w");

    io_rewind(buffer);
    if (io_copy(buffer, encode) != 0)
        puts("Some sort of error occured when copying streams");
    puts("");
}
