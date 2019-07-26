#include "zlib_io.h"
#include "../seaerror.h"

#include <stdint.h>

#ifdef CC_INCLUDE_ZLIB
struct ZlibParameters {
    IO io;
    int windowBits;
    int deflateLevel;
    int memLevel;
    int strategy;
    int deflating;
    int *err;
};

struct ZlibState {
    IO io;
    z_stream zlib;
    unsigned char buffer[4096];
    int deflating;
};

static void *zalloc(void *unused, uInt size, uInt count) {
    UNUSED(unused)

    return CALLOC(size, count);
}

static void zfree(void *unused, void *ptr) {
    UNUSED(unused)

    return FREE(ptr);
}

static void *zlib_open(void *userdata, IO io) {
    UNUSED(io)

    struct ZlibState *state = CALLOC(1, sizeof(*state));
    if (state == NULL)
        return NULL;

    struct ZlibParameters *params = userdata;

    int windowBits = params->windowBits;

    state->deflating = params->deflating;
    state->io = params->io;
    state->zlib.zalloc = zalloc;
    state->zlib.zfree = zfree;
    state->zlib.opaque = NULL;
    state->zlib.next_in = state->zlib.next_out = NULL;
    state->zlib.avail_in = state->zlib.avail_out = 0;

    if (params->err)
        *params->err = 0;

    int result;

    if (state->deflating)
        result = deflateInit2(&state->zlib, params->deflateLevel, Z_DEFLATED, windowBits, params->memLevel, params->strategy);
    else
        result = inflateInit2(&state->zlib, windowBits);

    if (params->err) {
        int *err = params->err;

        switch (result) {
            case Z_MEM_ERROR: *err = CC_ENOMEM; goto cleanup;
            case Z_STREAM_ERROR: *err = CC_EINVAL; goto cleanup;
            case Z_VERSION_ERROR: *err = CC_EIO; goto cleanup;
            case Z_OK: break;
        }
    }

    return state;

cleanup:
    FREE(state);
    return NULL;
}

static size_t zlib_read(void *buf, size_t size, size_t count, void *userdata, IO io) {
    struct ZlibState *state = userdata;

    size_t max = size*count;

    state->zlib.avail_out = max;
    state->zlib.next_out = buf;

    int flush = io_eof(state->io)? Z_FINISH: Z_NO_FLUSH;
    int result;

    do {
        if (state->zlib.avail_in == 0) {
            state->zlib.avail_in = io_read(state->buffer, 1, sizeof(state->buffer), state->io);

            if (flush == Z_FINISH && state->zlib.avail_in == 0 && io_error(state->io)) {
                io_set_error(io, io_error(state->io));
                break;
            }

            if (state->zlib.avail_in != sizeof(state->buffer)) {
                flush = Z_FINISH;
            }

            state->zlib.next_in = state->buffer;
        }

        if (state->deflating)
            result = deflate(&state->zlib, flush);
        else
            result = inflate(&state->zlib, flush);

        switch (result) {
            case Z_STREAM_ERROR:
            case Z_NEED_DICT: io_set_error(io, io_error(state->io)? io_error(state->io): CC_EIO); return SIZE_MAX;
            case Z_DATA_ERROR: io_set_error(io, io_error(state->io)? io_error(state->io): CC_EBADMSG); return SIZE_MAX;
            case Z_MEM_ERROR: io_set_error(io, io_error(state->io)? io_error(state->io): CC_ENOMEM); return SIZE_MAX;
            case Z_STREAM_END: goto done;
        }
    } while (state->zlib.avail_out != 0);

done:
    return (max - state->zlib.avail_out) / size;
}

static size_t zlib_write(const void *buf, size_t size, size_t count, void *userdata, IO io) {
    struct ZlibState *state = userdata;

    state->zlib.avail_in = size*count;
    state->zlib.next_in = (Bytef *) buf;

    int flush = state->zlib.avail_in == 0? Z_FINISH: Z_NO_FLUSH;
    int result;

    do {
        state->zlib.avail_out = sizeof(state->buffer);
        state->zlib.next_out = state->buffer;

        if (state->deflating)
            result = deflate(&state->zlib, flush);
        else
            result = inflate(&state->zlib, flush);

        switch (result) {
            case Z_STREAM_ERROR:
            case Z_NEED_DICT: io_set_error(io, CC_EIO); return 0;
            case Z_DATA_ERROR: io_set_error(io, CC_EBADMSG); return 0;
            case Z_MEM_ERROR: io_set_error(io, CC_ENOMEM); return 0;
        }

        size_t used = sizeof(state->buffer) - state->zlib.avail_out;
        if (io_write(state->buffer, 1, used, state->io) != used) {
            io_set_error(io, io_error(state->io));
            return 0;
        }
    } while (state->zlib.avail_out == 0);

    return count;
}

static int zlib_close(void *userdata, IO io) {
    UNUSED(io)

    struct ZlibState *state = userdata;
    int result = 0;

    if (io_just_wrote(io)) {
        zlib_write("", 0, 0, userdata, io);
        result = io_error(io);
    }

    if (state->deflating)
        result = deflateEnd(&state->zlib) != Z_OK? result? result: CC_EIO: 0;
    else
        result = inflateEnd(&state->zlib) != Z_OK? result? result: CC_EIO: 0;

    FREE(state);
    return result;
}

static int zlib_flush(void *userdata, IO io) {
    struct ZlibState *state = userdata;

    int result = io_flush(state->io);
    io_set_error(io, io_error(state->io));

    return result;
}

static void zlib_clearerr(void *userdata, IO io) {
    UNUSED(io)

    struct ZlibState *state = userdata;

    io_clearerr(state->io);
}

static const char *zlib_what(void *userdata, IO io) {
    UNUSED(io)

    struct ZlibState *state = userdata;

    return state->deflating? "zlib_deflate": "zlib_inflate";
}

static const struct InputOutputDeviceCallbacks zlib_callbacks = {
    .open = zlib_open,
    .close = zlib_close,
    .read = zlib_read,
    .write = zlib_write,
    .flush = zlib_flush,
    .clearerr = zlib_clearerr,
    .stateSwitch = NULL,
    .seek = NULL,
    .seek64 = NULL,
    .tell = NULL,
    .tell64 = NULL,
    .what = zlib_what
};

IO io_open_zlib_deflate(IO io, int level, int windowBits, const char *mode) {
    return io_open_zlib_deflate2(io, level, windowBits, 8, Z_DEFAULT_STRATEGY, mode);
}

IO io_open_zlib_deflate2(IO io, int level, int windowBits, int memLevel, int strategy, const char *mode) {
    struct ZlibParameters params = {
        .io = io,
        .deflateLevel = level,
        .windowBits = windowBits,
        .memLevel = memLevel,
        .strategy = strategy,
        .deflating = 1
    };

    return io_open_custom(&zlib_callbacks, &params, mode);
}

IO io_open_zlib_deflate_easy(IO io, enum ZlibType type, const char *mode) {
    switch (type) {
        case ZlibDeflate: return io_open_zlib_deflate(io, Z_DEFAULT_COMPRESSION, 15, mode);
        case GzipDeflate: return io_open_zlib_deflate(io, Z_DEFAULT_COMPRESSION, 15 + 16, mode);
        case RawDeflate: return io_open_zlib_deflate(io, Z_DEFAULT_COMPRESSION, -15, mode);
        default: return NULL;
    }
}

IO io_open_zlib_inflate(IO io, int windowBits, const char *mode) {
    struct ZlibParameters params = {
        .io = io,
        .deflateLevel = Z_DEFAULT_COMPRESSION,
        .windowBits = windowBits,
        .memLevel = 8,
        .strategy = Z_DEFAULT_STRATEGY,
        .deflating = 0
    };

    return io_open_custom(&zlib_callbacks, &params, mode);
}

IO io_open_zlib_inflate_easy(IO io, enum ZlibType type, const char *mode) {
    switch (type) {
        case ZlibOnlyInflate: return io_open_zlib_inflate(io, 15, mode);
        case GzipOnlyInflate: return io_open_zlib_inflate(io, 15 + 16, mode);
        case GzipOrZlibInflate: return io_open_zlib_inflate(io, 15 + 32, mode);
        case RawInflate: return io_open_zlib_inflate(io, -15, mode);
        default: return NULL;
    }
}

#endif
