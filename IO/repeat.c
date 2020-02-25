/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "repeat.h"
#include "../seaerror.h"

#include <string.h>

static void *repeat_open(void *userdata, IO io) {
    UNUSED(io)

    return userdata;
}

static size_t repeat_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    IO underlyingIO = userdata;
    char *cptr = ptr;
    size_t max = size*count;
    size_t read;
    int at_end = 0;

    while (1) {
        read = io_read(cptr, 1, max, underlyingIO);
        io_set_error(io, io_error(underlyingIO));

        cptr += read;
        max -= read;

        if (max == 0 || io_error(io) || (at_end && read == 0)) /* Everything was read, an error occurred, or the stream is empty */
            return (size*count - max) / size;

        if (io_seek(underlyingIO, 0, SEEK_SET)) {
            io_set_error(io, CC_ESPIPE);
            return (size*count - max) / size;
        }

        at_end = 1;
    }
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
    .flush = NULL,
    .clearerr = NULL,
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
