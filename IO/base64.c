/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "base64.h"
#include "../seaerror.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

struct Base64Params {
    const char *alphabet;
    IO io;
    uint32_t state;
    signed char pushed_to_state;
    signed char available_in_state;
    signed char padding_chars;
    signed char done;
    signed char decoding;
};

static void base64_init(struct Base64Params *base64) {
    base64->state = 0;
    base64->pushed_to_state = base64->available_in_state = base64->padding_chars = base64->done = 0;
}

/* FOR DECODING PURPOSES */
static void base64_push_encoded(struct Base64Params *base64, int encoded) {
    if (encoded == 64) {
        ++base64->padding_chars;
        encoded = 0;
    } else {
        ++base64->pushed_to_state;
    }

    base64->state = (base64->state << 6) | encoded;
    if (base64->pushed_to_state + base64->padding_chars == 4)
        base64->available_in_state = base64->pushed_to_state - 1;
}

static void base64_push_encoded_finish(struct Base64Params *base64) {
    if (base64->pushed_to_state) {
        base64->state <<= 6 * (4 - base64->pushed_to_state);
        base64->available_in_state = base64->pushed_to_state - 1;
    }

    base64->done = 1;
}

static char base64_get_decoded(struct Base64Params *base64) {
    if (base64->available_in_state) {
        if (--base64->available_in_state == 0)
            base64->pushed_to_state = 0;

        base64->state <<= 8;
        return (base64->state >> 24) & 0xff;
    }

    return 0;
}

/* FOR ENCODING PURPOSES */
static void base64_push_decoded(struct Base64Params *base64, int decoded) {
    base64->state = (base64->state << 8) | decoded;
    ++base64->pushed_to_state;

    if (base64->pushed_to_state == 3)
        base64->available_in_state = 4;
}

static void base64_push_decoded_finish(struct Base64Params *base64) {
    if (base64->pushed_to_state) {
        base64->state <<= 8 * (3 - base64->pushed_to_state);

        if (base64->alphabet[64]) {
            base64->available_in_state = 4; /* If alphabet includes padding, always pad to 4 chars */
            base64->padding_chars = 3 - base64->pushed_to_state;
        } else
            base64->available_in_state = base64->pushed_to_state + 1;
    }

    base64->done = 1;
}

static char base64_get_encoded(struct Base64Params *base64) {
    if (base64->available_in_state) {
        if (base64->padding_chars == base64->available_in_state) {
            --base64->padding_chars;
            --base64->available_in_state;

            return 64;
        } else {
            if (--base64->available_in_state == 0)
                base64->pushed_to_state = 0;

            base64->state <<= 6;
            return (base64->state >> 24) & 0x3f;
        }
    }

    return 0;
}

static size_t base64_decode_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    size_t max = size*count;
    unsigned char *cptr = ptr;
    struct Base64Params *base64 = (struct Base64Params *) userdata;

    while (max) {
        if (base64->available_in_state) {
            *cptr++ = base64_get_decoded(base64);
            --max;
        } else if (!base64->done) {
            int ch = io_getc(base64->io);
            if (ch == EOF)
                base64_push_encoded_finish(base64);
            else {
                char *sptr = strchr(base64->alphabet, ch);
                if (sptr == NULL) {
                    io_set_error(io, CC_EBADMSG);
                    return (cptr - (unsigned char *) ptr) / size;
                }

                ch = (int) (sptr - base64->alphabet);

                base64_push_encoded(base64, ch);

                if (base64->padding_chars > 2) {
                    io_set_error(io, CC_EBADMSG);
                    return (cptr - (unsigned char *) ptr) / size;
                }
            }
        } else
            break;
    }

    io_set_error(io, io_error(base64->io));
    return (cptr - (unsigned char *) ptr) / size;
}

static size_t base64_encode_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    size_t max = size*count;
    unsigned char *cptr = ptr;
    struct Base64Params *base64 = (struct Base64Params *) userdata;

    while (max) {
        if (base64->available_in_state) {
            *cptr++ = base64->alphabet[(int) base64_get_encoded(base64)];
            --max;
        } else if (!base64->done) {
            int ch = io_getc(base64->io);
            if (ch == EOF)
                base64_push_decoded_finish(base64);
            else {
                base64_push_decoded(base64, ch);
            }
        } else
            break;
    }

    io_set_error(io, io_error(base64->io));
    return (cptr - (unsigned char *) ptr) / size;
}

static size_t base64_decode_write(const void *ptr, size_t size, size_t count, void *userdata, IO io) {
    const unsigned char *cptr = ptr;
    size_t max = size*count;
    struct Base64Params *base64 = (struct Base64Params *) userdata;

    for (; max; --max, ++cptr) {
        char *sptr = strchr(base64->alphabet, *cptr);
        if (sptr == NULL) {
            io_set_error(io, CC_EBADMSG);
            return (cptr - (unsigned char *) ptr) / size;
        }

        int ch = (int) (sptr - base64->alphabet);

        base64_push_encoded(base64, ch);

        while (base64->available_in_state) {
            char c = base64_get_decoded(base64);

            if (io_putc(c, base64->io) == EOF) {
                io_set_error(io, io_error(base64->io));
                return (cptr - (unsigned char *) ptr) / (size);
            }
        }
    }

    io_set_error(io, io_error(base64->io));
    return (cptr - (unsigned char *) ptr) / (size);
}

static size_t base64_encode_write(const void *ptr, size_t size, size_t count, void *userdata, IO io) {
    const unsigned char *cptr = ptr;
    size_t max = size*count;
    struct Base64Params *base64 = (struct Base64Params *) userdata;

    for (; max; --max, ++cptr) {
        base64_push_decoded(base64, *cptr);

        while (base64->available_in_state) {
            if (io_putc(base64->alphabet[(int) base64_get_encoded(base64)], base64->io) == EOF) {
                io_set_error(io, io_error(base64->io));
                return (cptr - (unsigned char *) ptr) / (size);
            }
        }
    }

    io_set_error(io, io_error(base64->io));
    return (cptr - (unsigned char *) ptr) / (size);
}

static int base64_close(void *userdata, IO io) {
    int error = 0;
    struct Base64Params *base64 = (struct Base64Params *) userdata;

    if (io_just_wrote(io) && base64->pushed_to_state) {
        if (base64->decoding) {
            base64_push_encoded_finish(base64);

            while (base64->available_in_state)
                if (io_putc(base64_get_decoded(base64), base64->io) == EOF)
                    error = io_error(base64->io);
        } else {
            base64_push_decoded_finish(base64);

            while (base64->available_in_state) {
                char c = base64->alphabet[(int) base64_get_encoded(base64)];

                if (io_putc(c, base64->io) == EOF)
                    error = io_error(base64->io);
            }
        }
    }

    FREE(base64);

    return error;
}

static int base64_flush(void *userdata, IO io) {
    struct Base64Params *base64 = (struct Base64Params *) userdata;

    int result = io_flush(base64->io);
    io_set_error(io, io_error(base64->io));

    return result;
}

static void base64_clearerr(void *userdata, IO io) {
    UNUSED(io)
    struct Base64Params *base64 = (struct Base64Params *) userdata;

    io_clearerr(base64->io);
}

/* TODO: seeking and writing are not currently implemented */
static int base64_encode_seek64(void *userdata, long long int offset, int origin, IO io) {
    struct Base64Params *base64 = (struct Base64Params *) userdata;

    /* Translate all origins to SEEK_SET for ease of computation */
    switch (origin) {
        case SEEK_END: {
            long long underlyingSize = io_size64(base64->io);
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

static int base64_decode_seek64(void *userdata, long long int offset, int origin, IO io) {
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

static long long base64_encode_tell64(void *userdata, IO io) {
    UNUSED(io)

    long long value = io_tell64((IO) userdata);
    return value > 0? value * 2 - (*io_tempdata(io) < 16): value;
}

static long long base64_decode_tell64(void *userdata, IO io) {
    UNUSED(io)

    long long value = io_tell64((IO) userdata);
    return value > 0? value / 2 + (*io_tempdata(io) < 16): value;
}

static const char *base64_encode_what(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    return "base64_encode";
}

static const char *base64_decode_what(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    return "base64_decode";
}

static const struct InputOutputDeviceCallbacks base64_encode_callbacks = {
    .read = base64_encode_read,
    .write = base64_encode_write,
    .open = NULL,
    .close = base64_close,
    .flush = base64_flush,
    .clearerr = base64_clearerr,
    .stateSwitch = NULL,
    .tell = NULL,
    .tell64 = base64_encode_tell64,
    .seek = NULL,
    .seek64 = base64_encode_seek64,
    .flags = NULL,
    .what = base64_encode_what
};

static const struct InputOutputDeviceCallbacks base64_decode_callbacks = {
    .read = base64_decode_read,
    .write = base64_decode_write,
    .open = NULL,
    .close = base64_close,
    .flush = base64_flush,
    .clearerr = base64_clearerr,
    .stateSwitch = NULL,
    .tell = NULL,
    .tell64 = base64_decode_tell64,
    .seek = NULL,
    .seek64 = base64_decode_seek64,
    .flags = NULL,
    .what = base64_decode_what
};

IO io_open_base64_custom_encode(IO io, const char *alphabet, const char *mode) {
    if (strlen(alphabet) < 64)
        return NULL;

    struct Base64Params *params = CALLOC(1, sizeof(*params));
    if (params == NULL)
        return NULL;

    params->alphabet = alphabet;
    params->io = io;

    return io_open_custom(&base64_encode_callbacks, params, mode);
}

IO io_open_base64_custom_decode(IO io, const char *alphabet, const char *mode) {
    if (strlen(alphabet) < 64)
        return NULL;

    struct Base64Params *params = CALLOC(1, sizeof(*params));
    if (params == NULL)
        return NULL;

    params->alphabet = alphabet;
    params->io = io;
    params->decoding = 1;

    return io_open_custom(&base64_decode_callbacks, params, mode);
}

IO io_open_base64_url_encode(IO io, const char *mode) {
    return io_open_base64_custom_encode(io, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_=", mode);
}

IO io_open_base64_url_decode(IO io, const char *mode) {
    return io_open_base64_custom_decode(io, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_=", mode);
}

IO io_open_base64_encode(IO io, const char *mode) {
    return io_open_base64_custom_encode(io, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=", mode);
}

IO io_open_base64_decode(IO io, const char *mode) {
    return io_open_base64_custom_decode(io, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=", mode);
}

void test_base64() {
    IO buffer = io_open_cstring("A string to try encoding", "r");
    IO encode = io_open_base64_encode(buffer, "r");
    IO decode = io_open_base64_decode(encode, "r");
    IO out = io_open_file(stdout);

    if (io_copy(decode, out) != 0)
        puts("Some sort of error occured when copying streams");
    puts("");

    decode = io_open_base64_decode(out, "w");
    encode = io_open_base64_encode(decode, "w");

    io_rewind(buffer);
    if (io_copy(buffer, encode) != 0)
        puts("Some sort of error occured when copying streams");
    puts("");
}
