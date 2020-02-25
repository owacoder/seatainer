/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "concat.h"
#include "../seaerror.h"

#include <string.h>

/* io_tempdata() guarantees that we'll be able to store at least three pointers, so casting tempdata to this struct is always safe */
struct ConcatInitializationParams {
    IO lhs, rhs;
    char use_lhs;
};

static void *concat_open(void *userdata, IO io) {
    memcpy(io_tempdata(io), userdata, 3*sizeof(void *));

    return io;
}

static size_t concat_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(userdata)

    unsigned char *cptr = ptr;
    size_t read = 0, max = size*count;
    struct ConcatInitializationParams *params = (struct ConcatInitializationParams *) io_tempdata(io);

    if (params->use_lhs) {
        read = io_read(cptr, 1, max, params->lhs);
        io_set_error(io, io_error(params->lhs));

        if (read != max) {
            if (io_error(params->lhs) == 0) /* Simple EOF? */ {
                params->use_lhs = 0;
                read += io_read(cptr + read, 1, max - read, params->rhs);
                io_set_error(io, io_error(params->rhs));
            }
        }

        return read / size;
    } else {
        read = io_read(cptr, 1, max, params->rhs);
        io_set_error(io, io_error(params->rhs));
        return read;
    }
}

static size_t concat_write(const void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(userdata)

    const unsigned char *cptr = ptr;
    size_t written = 0, max = size*count;
    struct ConcatInitializationParams *params = (struct ConcatInitializationParams *) io_tempdata(io);

    if (params->use_lhs) {
        written = io_write(ptr, 1, max, params->lhs);
        io_set_error(io, io_error(params->lhs));

        if (written != max) {
            if (io_error(params->lhs) == CC_ENOBUFS) {
                params->use_lhs = 0;
                written += io_write(cptr + written, 1, max - written, params->rhs);
                io_set_error(io, io_error(params->rhs));
            }
        }

        return written / size;
    } else {
        written = io_write(ptr, size, count, params->rhs);
        io_set_error(io, io_error(params->rhs));
        return written;
    }
}

static int concat_flush(void *userdata, IO io) {
    UNUSED(userdata)

    struct ConcatInitializationParams *params = (struct ConcatInitializationParams *) io_tempdata(io);

    io_flush(params->lhs);
    io_flush(params->rhs);

    if (io_error(params->lhs))
        io_set_error(io, io_error(params->lhs));
    else if (io_error(params->rhs))
        io_set_error(io, io_error(params->rhs));
    else
        return 0;

    return EOF;
}

static void concat_clearerr(void *userdata, IO io) {
    UNUSED(userdata)

    struct ConcatInitializationParams *params = (struct ConcatInitializationParams *) io_tempdata(io);

    io_clearerr(params->lhs);
    io_clearerr(params->rhs);
}

static long long concat_tell64(void *userdata, IO io) {
    UNUSED(userdata)

    struct ConcatInitializationParams *params = (struct ConcatInitializationParams *) io_tempdata(io);

    if (params->use_lhs)
        return io_tell64(params->lhs);

    long long lhs_size = io_size64(params->lhs);
    if (lhs_size < 0)
        return -1;

    long long rhs_pos = io_tell64(params->rhs);
    if (rhs_pos < 0)
        return -1;

    return lhs_size + rhs_pos;
}

static int concat_seek64(void *userdata, long long int offset, int origin, IO io) {
    UNUSED(userdata)

    struct ConcatInitializationParams *params = (struct ConcatInitializationParams *) io_tempdata(io);

    if (offset == 0 && origin == SEEK_SET) {
        if (io_seek(params->lhs, 0, SEEK_SET) < 0 ||
                io_seek(params->rhs, 0, SEEK_SET) < 0)
            return -1;

        params->use_lhs = 1;
        return 0;
    }

    long long lhs_size = io_size64(params->lhs);
    if (lhs_size < 0)
        return -1;

    /* Translate all origins to SEEK_SET for ease of computation */
    switch (origin) {
        case SEEK_END: {
            long long underlyingSize = io_size64(params->rhs);
            if (underlyingSize < 0)
                return -1;

            offset += underlyingSize + lhs_size;
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

    if (offset < 0)
        return -1;

    if (offset < lhs_size) {
        if (io_seek(params->rhs, 0, SEEK_SET) < 0 ||
            io_seek64(params->lhs, offset, SEEK_SET) < 0)
            return -1;

        params->use_lhs = 1;
    } else {
        if (io_seek64(params->rhs, offset - lhs_size, SEEK_SET) < 0)
            return -1;

        params->use_lhs = 0;
    }

    return 0;
}

static const char *concat_what(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    return "concat";
}

static const struct InputOutputDeviceCallbacks concat_callbacks = {
    .open = concat_open,
    .close = NULL,
    .read = concat_read,
    .write = concat_write,
    .flush = concat_flush,
    .clearerr = concat_clearerr,
    .stateSwitch = NULL,
    .seek = NULL,
    .seek64 = concat_seek64,
    .tell = NULL,
    .tell64 = concat_tell64,
    .what = concat_what
};

IO io_open_concat(IO lhs, IO rhs, const char *mode) {
    struct ConcatInitializationParams params = {
        .lhs = lhs,
        .rhs = rhs,
        .use_lhs = 1
    };

    return io_open_custom(&concat_callbacks, &params, mode);
}
