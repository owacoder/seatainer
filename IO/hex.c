#include "hex.h"
#include "platforms.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct Hex {
    IO io;
    unsigned char nibble; // 16 if no nibble available
};

static void *hex_open(void *userdata, IO io) {
    UNUSED(io)

    struct Hex *result = calloc(1, sizeof(struct Hex));
    if (result == NULL)
        return NULL;

    result->io = userdata;
    result->nibble = 16;

    return result;
}

static int hex_close(void *userdata, IO io) {
    UNUSED(io)

    free(userdata);
    return 0;
}

static size_t hex_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    if (size == 0 || count == 0)
        return 0;

    size_t max = size*count;
    unsigned char *cptr = ptr;
    struct Hex *hex = userdata;
    const char alpha[] = "0123456789abcdef";

    while (max) {
        int ch = io_getc(hex->io);
        if (ch == EOF)
            break;

        char *sptr = strchr(alpha, tolower(ch));
        if (sptr == NULL)
            continue;

        ch = sptr - alpha;

        if (hex->nibble == 16)
            hex->nibble = ch;
        else {
            *cptr++ = (hex->nibble << 4) | ch;
            hex->nibble = 16;
            --max;
        }
    }

    return io_error(hex->io)? SIZE_MAX: (cptr - (unsigned char *) ptr) / size;
}

static size_t hex_write(const void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    if (size == 0 || count == 0)
        return 0;

    const char alpha[] = "0123456789abcdef";
    const unsigned char *cptr = ptr;
    struct Hex *hex = userdata;
    size_t max = size*count;

    for (; max; --max, ++cptr) {
        if (io_putc(alpha[*cptr >> 4], hex->io) == EOF ||
            io_putc(alpha[*cptr & 0xf], hex->io) == EOF)
            break;
    }

    return (cptr - (unsigned char *) ptr) / size;
}

static int hex_flush(void *userdata, IO io) {
    UNUSED(io)

    struct Hex *hex = userdata;
    return io_flush(hex->io);
}

static int hex_seek(void *userdata, long int offset, int origin, IO io) {
    UNUSED(io)

    struct Hex *hex = userdata;
    int result = io_seek(hex->io, offset, origin);
    if (result == 0)
        hex->nibble = 16;
    return result;
}

static int hex_seek64(void *userdata, long long int offset, int origin, IO io) {
    UNUSED(io)

    struct Hex *hex = userdata;
    int result = io_seek64(hex->io, offset, origin);
    if (result == 0)
        hex->nibble = 16;
    return result;
}

static long hex_tell(void *userdata, IO io) {
    UNUSED(io)

    struct Hex *hex = userdata;
    return io_tell(hex->io);
}

static long long hex_tell64(void *userdata, IO io) {
    UNUSED(io)

    struct Hex *hex = userdata;
    return io_tell64(hex->io);
}

static const struct InputOutputDeviceCallbacks hex_callbacks = {
    .read = hex_read,
    .write = hex_write,
    .open = hex_open,
    .close = hex_close,
    .flush = hex_flush,
    .tell = hex_tell,
    .tell64 = hex_tell64,
    .seek = hex_seek,
    .seek64 = hex_seek64
};

IO io_open_hex_filter(IO io, const char *mode) {
    return io_open_custom(&hex_callbacks, io, mode);
}
