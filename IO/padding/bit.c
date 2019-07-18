/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "bit.h"
#include <string.h>
#include <stdint.h>

/* TODO: support seeking with padding devices */

struct BitPadding {
    IO io;
    size_t block_size;
    uint64_t written;
    int wrote_80;
};

int bit_padding_encode_close(void *userdata, IO io) {
    UNUSED(io)

    struct BitPadding *padding = userdata;
    int err = 0;

    if (io_just_wrote(io) && padding->written % padding->block_size != 0) {
        if (io_putc(0x80, padding->io) == EOF)
            err = io_error(padding->io);

        padding->written += 1;
        size_t rest = padding->written % padding->block_size;

        for (size_t i = 0; i < rest; ++i)
            if (io_putc(0, padding->io) == EOF) {
                err = io_error(padding->io);
                break;
            }
    }

    FREE(userdata);

    return err;
}

size_t bit_padding_encode_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    struct BitPadding *padding = userdata;

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

        if (!padding->wrote_80) {
            padding->wrote_80 = 1;

            *cptr++ = 0x80; /* Pad 0x80 since we know there needs to be at least one pad char */
            padding->written += 1;
            read += 1;
            max -= 1;
        }

        size_t more = padding->written % padding->block_size;
        more = more > max? max: more;

        if (more) { /* Pad any remaining zeros necessary */
            memset(cptr, 0, more);
            padding->written += more;
            read += more;
        }
    }

    return read / size;
}

size_t bit_padding_encode_write(const void *ptr, size_t size, size_t count, void *userdata, IO io) {
    struct BitPadding *padding = userdata;

    size_t written = io_write(ptr, size, count, padding->io);

    if (written == size*count)
        padding->written += written;
    else
        io_set_error(io, io_error(padding->io));

    return written;
}

int bit_padding_encode_flush(void *userdata, IO io) {
    struct BitPadding *padding = userdata;

    int result = io_flush(padding->io);
    io_set_error(io, io_error(padding->io));

    return result;
}

static void bit_padding_clearerr(void *userdata, IO io) {
    UNUSED(io)

    struct BitPadding *padding = userdata;

    io_clearerr(padding->io);
}

static const char *bit_padding_encode_what(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    return "bit_padding_encode";
}

static const struct InputOutputDeviceCallbacks bit_padding_encode_callbacks = {
    .open = NULL,
    .close = bit_padding_encode_close,
    .read = bit_padding_encode_read,
    .write = bit_padding_encode_write,
    .flush = bit_padding_encode_flush,
    .clearerr = bit_padding_clearerr,
    .stateSwitch = NULL,
    .seek = NULL,
    .seek64 = NULL,
    .tell = NULL,
    .tell64 = NULL,
    .what = bit_padding_encode_what
};

IO io_open_bit_padding_encode(IO io, size_t block_size, const char *mode) {
    struct BitPadding *padding = CALLOC(1, sizeof(*padding));
    if (padding == NULL)
        return NULL;

    IO result = io_open_custom(&bit_padding_encode_callbacks, padding, mode);
    if (result == NULL) {
        FREE(padding);
        return NULL;
    }

    padding->io = io;
    padding->block_size = block_size? block_size: 1;

    return result;
}
