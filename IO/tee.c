#include "tee.h"
#include <string.h>

/* io_tempdata() guarantees that we'll be able to store at least two pointers, so casting tempdata to this struct is always safe */
struct TeeInitializationParams {
    IO out1, out2;
};

void *tee_open(void *userdata, IO io) {
    memcpy(io_tempdata(io), userdata, 2*sizeof(void *));

    return io;
}

size_t tee_write(const void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(userdata)

    struct TeeInitializationParams *params = (struct TeeInitializationParams *) io_tempdata(io);

    io_write(ptr, size, count, params->out1);
    io_write(ptr, size, count, params->out2);

    if (io_error(params->out1))
        io_set_error(io, io_error(params->out1));
    else if (io_error(params->out2))
        io_set_error(io, io_error(params->out2));
    else
        return count;

    return 0;
}

int tee_flush(void *userdata, IO io) {
    UNUSED(userdata)

    struct TeeInitializationParams *params = (struct TeeInitializationParams *) io_tempdata(io);

    io_flush(params->out1);
    io_flush(params->out2);

    if (io_error(params->out1))
        io_set_error(io, io_error(params->out1));
    else if (io_error(params->out2))
        io_set_error(io, io_error(params->out2));
    else
        return 0;

    return EOF;
}

static const char *tee_what(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    return "tee";
}

static const struct InputOutputDeviceCallbacks tee_callbacks = {
    .open = tee_open,
    .close = NULL,
    .read = NULL,
    .write = tee_write,
    .flush = tee_flush,
    .stateSwitch = NULL,
    .seek = NULL,
    .seek64 = NULL,
    .tell = NULL,
    .tell64 = NULL,
    .what = tee_what
};

IO io_open_tee(IO out1, IO out2, const char *mode) {
    struct TeeInitializationParams params = {
        .out1 = out1,
        .out2 = out2
    };

    return io_open_custom(&tee_callbacks, &params, mode);
}
