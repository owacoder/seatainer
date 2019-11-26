#include "limiter.h"

#include "../seaerror.h"

struct Limiter {
    IO io;
    long long offset, length, pos;
};

static size_t limiter_read(void *buf, size_t size, size_t count, void *userdata, IO io) {
    struct Limiter *limiter = userdata;

    size_t max = size*count;
    long long available = limiter->length - io_tell64(io);
    if (available <= 0) /* Overshot end of limited sequence, EOF */
        return 0;

    unsigned long long uavailable = available;

    if (max > uavailable)
        max = uavailable;

    size_t read = io_read(buf, 1, max, limiter->io) / size;
    io_set_error(io, io_error(limiter->io));

    limiter->pos += read;
    return read;
}

static size_t limiter_write(const void *buf, size_t size, size_t count, void *userdata, IO io) {
    struct Limiter *limiter = userdata;

    size_t max = size*count;
    long long available = limiter->length - io_tell64(io);
    if (available <= 0) /* No space to write */ {
        io_set_error(io, CC_ENOBUFS);
        return 0;
    }

    unsigned long long uavailable = available;

    if (max > uavailable)
        max = uavailable;

    size_t written = io_write(buf, 1, max, limiter->io) / size;
    io_set_error(io, io_error(limiter->io));

    if (!io_error(io) && written < max)
        io_set_error(io, CC_ENOBUFS);

    limiter->pos += written;
    return written;
}

static int limiter_close(void *userdata, IO io) {
    UNUSED(io)

    FREE(userdata);

    return 0;
}

static int limiter_flush(void *userdata, IO io) {
    UNUSED(io)

    struct Limiter *limiter = userdata;

    int result = io_flush(limiter->io);
    io_set_error(io, io_error(limiter->io));

    return result;
}

static void limiter_clearerr(void *userdata, IO io) {
    UNUSED(io)

    struct Limiter *limiter = userdata;

    io_clearerr(limiter->io);
}

static long long limiter_tell64(void *userdata, IO io) {
    UNUSED(io)

    struct Limiter *limiter = userdata;

    long long pos = io_tell64(limiter->io);
    if (pos >= 0) {
        limiter->pos = pos -= limiter->offset;
    } else {
        pos = limiter->pos;
    }

    return pos;
}

static int limiter_seek64(void *userdata, long long int offset, int origin, IO io) {
    struct Limiter *limiter = userdata;

    /* Translate all origins to SEEK_SET for ease of computation */
    switch (origin) {
        case SEEK_END: {
            long long underlyingSize = io_size64(limiter->io);
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

    if (offset < 0 || offset > limiter->length)
        return -1;

    int result = io_seek64(limiter->io, offset + limiter->offset, SEEK_SET);
    if (result == 0)
        limiter->pos = offset;

    return result;
}

static const char *limiter_what(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    return "limiter";
}

static const struct InputOutputDeviceCallbacks limiter_callbacks = {
    .read = limiter_read,
    .write = limiter_write,
    .open = NULL,
    .close = limiter_close,
    .flush = limiter_flush,
    .clearerr = limiter_clearerr,
    .stateSwitch = NULL,
    .tell = NULL,
    .tell64 = limiter_tell64,
    .seek = NULL,
    .seek64 = limiter_seek64,
    .what = limiter_what
};

IO io_open_limiter(IO io, long long offset, long long length, const char *mode) {
    struct Limiter *limiter = MALLOC(sizeof(*limiter));
    if (limiter == NULL)
        return NULL;

    limiter->offset = offset < 0? io_tell64(io): offset;
    limiter->length = length;
    limiter->pos = 0;
    limiter->io = io;

    IO result = io_open_custom(&limiter_callbacks, limiter, mode);
    if (result == NULL) {
        FREE(limiter);
        return NULL;
    }

    return result;
}
