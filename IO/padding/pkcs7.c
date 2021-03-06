/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "bit.h"
#include "../../seaerror.h"

#include <string.h>
#include <stdint.h>

/* TODO: support seeking with padding devices */

struct Pkcs7Padding {
    IO io;
    size_t block_size;
    int pad_value;
    uint64_t written;
};

struct Pkcs7PaddingDecode {
    IO io;
    size_t block_size;

    unsigned char buffer[256];
    size_t buffer_avail; /* Number of bytes left available to read from buffer */
};

int pkcs7_padding_encode_close(void *userdata, IO io) {
    UNUSED(io)

    struct Pkcs7Padding *padding = userdata;
    int err = 0;

    if (io_just_wrote(io) && padding->written % padding->block_size != 0) {
        size_t rest = padding->block_size - padding->written % padding->block_size;

        for (size_t i = 0; i < rest; ++i)
            if (io_putc((int) rest, padding->io) == EOF) {
                err = io_error(padding->io);
                break;
            }
    }

    FREE(userdata);

    return err;
}

int pkcs7_padding_decode_close(void *userdata, IO io) {
    UNUSED(io)

    struct Pkcs7PaddingDecode *padding = userdata;
    int err = 0;

    if (io_just_wrote(io) && padding->buffer_avail) {
        unsigned padding_char = padding->buffer[padding->buffer_avail - 1];

        if (padding_char > padding->buffer_avail ||
                padding->buffer_avail != padding->block_size) /* Bad padding, specifies value greater than block size */
            err = CC_EBADMSG;

        if (!err) {
            /* Ensure that all padding actually is set to the correct value */
            for (size_t i = padding->buffer_avail - padding_char; i < padding->buffer_avail; ++i)
                if (padding->buffer[i] != padding_char) {
                    err = CC_EBADMSG;
                    break;
                }

            padding->buffer_avail -= padding_char;

            if (io_write(padding->buffer, 1, padding->buffer_avail, padding->io) != padding->buffer_avail) {
                err = io_error(padding->io);
            }
        }
    }

    FREE(userdata);

    return err;
}

size_t pkcs7_padding_encode_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    struct Pkcs7Padding *padding = userdata;

    size_t max = size*count;
    size_t read = io_read(ptr, 1, max, padding->io);

    padding->written += read;

    if (io_error(padding->io)) {
        io_set_error(io, io_error(padding->io));
        return read;
    } else if (read == max) /* Read everything */
        ;
    else if (io_eof(padding->io)) { /* Ran out of data while reading */
        unsigned char *cptr = ptr;

        cptr += read;
        max -= read;

        size_t more = padding->written % padding->block_size;
        if (padding->pad_value < 0) {
            more = padding->block_size - padding->written % padding->block_size;
            padding->pad_value = (int) more;
        }

        more = more > max? max: more;

        if (more) { /* Pad any remaining values necessary */
            memset(cptr, padding->pad_value, more);
            padding->written += more;
            read += more;
        }
    }

    return read / size;
}

size_t pkcs7_padding_decode_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    struct Pkcs7PaddingDecode *padding = userdata;

    size_t max = size*count;
    unsigned char *cptr = ptr;

    do {
        /* Read from existing internal buffer to specified external buffer */
        if (padding->buffer_avail >= max) {
            memcpy(cptr, padding->buffer + padding->block_size - padding->buffer_avail, max);
            padding->buffer_avail -= max;
            return count;
        } else if (padding->buffer_avail) {
            memcpy(cptr, padding->buffer + padding->block_size - padding->buffer_avail, padding->buffer_avail);
            max -= padding->buffer_avail;
            cptr += padding->buffer_avail;
            padding->buffer_avail = 0;
        }

        /* Pump data from input to internal buffer */
        size_t read = io_read(padding->buffer, 1, padding->block_size + 1, padding->io);

        if (read == padding->block_size + 1) {
            if (io_ungetc(padding->buffer[padding->block_size], padding->io) == EOF) {
                io_set_error(io, CC_EREAD);
                return SIZE_MAX;
            }

            padding->buffer_avail = padding->block_size;
        } else if (io_error(padding->io)) {
            io_set_error(io, io_error(padding->io));
            return read;
        } else if (read < padding->block_size) {
            if (read == 0)
                break;

            io_set_error(io, CC_EREAD);
            return SIZE_MAX;
        } else /* read == padding->block_size, no error */ {
            unsigned char pad_value = padding->buffer[padding->block_size - 1];
            int i = 0, pos;

            if (pad_value > padding->block_size) /* Bad padding, specifies value greater than block size */
                goto cleanup;

            /* Ensure that all padding actually is set to the correct value */
            for (pos = (int) padding->block_size - 1; i < pad_value; ++i, --pos) {
                if (padding->buffer[pos] != pad_value)
                    goto cleanup;
            }

            memmove(padding->buffer + pad_value, padding->buffer, padding->block_size - pad_value);
            padding->buffer_avail = padding->block_size - pad_value;
        }
    } while (max);

    return (size*count - max) / size;

cleanup:
    io_set_error(io, CC_EREAD);
    return SIZE_MAX;
}

size_t pkcs7_padding_encode_write(const void *ptr, size_t size, size_t count, void *userdata, IO io) {
    struct Pkcs7Padding *padding = userdata;

    size_t written = io_write(ptr, size, count, padding->io);

    if (written == size*count)
        padding->written += written;
    else
        io_set_error(io, io_error(padding->io));

    return written;
}

size_t pkcs7_padding_decode_write(const void *ptr, size_t size, size_t count, void *userdata, IO io) {
    struct Pkcs7PaddingDecode *padding = userdata;

    size_t max = size*count, written = 0;
    const unsigned char *cptr = ptr;

    while (max) {
        if (padding->buffer_avail == padding->block_size) { /* Only flush block if there is guaranteed to be more data coming */
            size_t wrote = io_write(padding->buffer, 1, padding->block_size, padding->io);
            written += wrote;
            if (wrote != padding->block_size) {
                io_set_error(io, io_error(padding->io));
                return written / size;
            }
            padding->buffer_avail = 0;
        }

        size_t amount_added = MIN(max, padding->block_size - padding->buffer_avail);
        memcpy(padding->buffer + padding->buffer_avail, cptr, amount_added);

        padding->buffer_avail += amount_added;
        cptr += amount_added;
        max -= amount_added;
    }

    return count;
}

int pkcs7_padding_encode_flush(void *userdata, IO io) {
    struct Pkcs7Padding *padding = userdata;

    int result = io_flush(padding->io);
    io_set_error(io, io_error(padding->io));

    return result;
}

int pkcs7_padding_decode_flush(void *userdata, IO io) {
    struct Pkcs7PaddingDecode *padding = userdata;

    int result = io_flush(padding->io);
    io_set_error(io, io_error(padding->io));

    return result;
}

static void pkcs7_padding_encode_clearerr(void *userdata, IO io) {
    UNUSED(io)

    struct Pkcs7Padding *padding = userdata;

    io_clearerr(padding->io);
}

static void pkcs7_padding_decode_clearerr(void *userdata, IO io) {
    UNUSED(io)

    struct Pkcs7PaddingDecode *padding = userdata;

    io_clearerr(padding->io);
}

static const char *pkcs7_padding_encode_what(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    return "pkcs7_padding_encode";
}

static const char *pkcs7_padding_decode_what(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    return "pkcs7_padding_decode";
}

static const struct InputOutputDeviceCallbacks pkcs7_padding_encode_callbacks = {
    .open = NULL,
    .close = pkcs7_padding_encode_close,
    .read = pkcs7_padding_encode_read,
    .write = pkcs7_padding_encode_write,
    .flush = pkcs7_padding_encode_flush,
    .clearerr = pkcs7_padding_encode_clearerr,
    .state_switch = NULL,
    .seek = NULL,
    .seek64 = NULL,
    .tell = NULL,
    .tell64 = NULL,
    .flags = NULL,
    .what = pkcs7_padding_encode_what
};

static const struct InputOutputDeviceCallbacks pkcs7_padding_decode_callbacks = {
    .open = NULL,
    .close = pkcs7_padding_decode_close,
    .read = pkcs7_padding_decode_read,
    .write = pkcs7_padding_decode_write,
    .flush = pkcs7_padding_decode_flush,
    .clearerr = pkcs7_padding_decode_clearerr,
    .state_switch = NULL,
    .seek = NULL,
    .seek64 = NULL,
    .tell = NULL,
    .tell64 = NULL,
    .flags = NULL,
    .what = pkcs7_padding_decode_what
};

IO io_open_pkcs7_padding_encode(IO io, size_t block_size, const char *mode) {
    if (block_size > 255)
        return NULL;

    struct Pkcs7Padding *padding = CALLOC(1, sizeof(*padding));
    if (padding == NULL)
        return NULL;

    IO result = io_open_custom(&pkcs7_padding_encode_callbacks, padding, mode);
    if (result == NULL) {
        FREE(padding);
        return NULL;
    }

    padding->io = io;
    padding->block_size = block_size? block_size: 1;
    padding->pad_value = -1;

    return result;
}

IO io_open_pkcs7_padding_decode(IO io, size_t block_size, const char *mode) {
    if (block_size > 255)
        return NULL;

    struct Pkcs7PaddingDecode *padding = CALLOC(1, sizeof(*padding));
    if (padding == NULL)
        return NULL;

    IO result = io_open_custom(&pkcs7_padding_decode_callbacks, padding, mode);
    if (result == NULL) {
        FREE(padding);
        return NULL;
    }

    padding->io = io;
    padding->block_size = block_size? block_size: 1;

    return result;
}
