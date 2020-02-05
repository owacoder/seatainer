/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "repeat.h"
#include <string.h>

/* TODO: implement repeating device */

static void *repeat_open(void *userdata, IO io) {
    memcpy(io_tempdata(io), userdata, sizeof(void *));

    return io;
}

static size_t repeat_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    IO underlyingIO = userdata;
    const char *cptr = ptr;
    size_t max = size*count;

    size_t read = io_read(cptr, 1, max, underlyingIO);
}

static int repeat_flush(void *userdata, IO io) {
    UNUSED(userdata)

    struct RepeatInitializationParams *params = (struct RepeatInitializationParams *) io_tempdata(io);

    /*
    io_flush(params->out1);
    io_flush(params->out2);

    if (io_error(params->out1))
        io_set_error(io, io_error(params->out1));
    else if (io_error(params->out2))
        io_set_error(io, io_error(params->out2));
    else
        return 0;
        */

    return EOF;
}

static void repeat_clearerr(void *userdata, IO io) {
    UNUSED(userdata)

    struct RepeatInitializationParams *params = (struct RepeatInitializationParams *) io_tempdata(io);

    /*
    io_clearerr(params->out1);
    io_clearerr(params->out2);*/
}

static const char *repeat_what(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    return "repeat";
}

static const struct InputOutputDeviceCallbacks repeat_callbacks = {
    .open = repeat_open,
    .close = NULL,
    .read = repeat_read,
    .write = NULL,
    .flush = repeat_flush,
    .clearerr = repeat_clearerr,
    .stateSwitch = NULL,
    .seek = NULL,
    .seek64 = NULL,
    .tell = NULL,
    .tell64 = NULL,
    .what = repeat_what
};

IO io_open_repeat(IO io, const char *mode) {
    return io_open_custom(&repeat_callbacks, io, mode);
}
