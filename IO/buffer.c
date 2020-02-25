/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "buffer.h"

#include "../seaerror.h"

struct Buffer {
    char *data;
    size_t capacity, pos, endpos;
};

static size_t buffer_size_available(struct Buffer *buf) {
    if (buf->pos <= buf->endpos)
        return buf->capacity - buf->endpos + buf->pos; /* Space available at end and beginning */
    else
        return buf->pos - buf->endpos; /* Space available in middle */
}

static int buffer_grow(struct Buffer *buf, size_t size_of_data_to_append) {
    if (buffer_size_available(buf) < size_of_data_to_append + 1) {
        size_t new_size = MAX(buf->capacity + (buf->capacity >> 1), buf->capacity - buffer_size_available(buf) + size_of_data_to_append + 1);

        if (buf->pos != 0) { /* If data is not aligned to beginning already */
            char *new_data = MALLOC(new_size);
            if (new_data == NULL)
                return CC_ENOMEM;

            if (buf->pos > buf->endpos) {
                size_t initial_size = buf->capacity - buf->pos;
                memcpy(new_data, buf->data + buf->pos, initial_size);
                memcpy(new_data + initial_size, buf->data, buf->endpos);

                FREE(buf->data);
                buf->data = new_data;
                buf->capacity = new_size;
                buf->endpos += initial_size;
                buf->pos = 0;
            } else {
                memcpy(new_data, buf->data + buf->pos, buf->endpos - buf->pos);

                FREE(buf->data);
                buf->data = new_data;
                buf->capacity = new_size;
                buf->endpos -= buf->pos;
                buf->pos = 0;
            }
        } else { /* Data already aligned to beginning */
            char *new_data = REALLOC(buf->data, new_size);
            if (new_data == NULL)
                return CC_ENOMEM;

            buf->data = new_data;
            buf->capacity = new_size;
        }
    }

    return 0;
}

/* Buffer must have at least `size` available space for data before this function is called */
static void buffer_append(struct Buffer *buf, const void *data, size_t size) {
    const char *cptr = data;

    if (buf->pos < buf->endpos) { /* Space available at both ends */
        size_t available_at_end = buf->capacity - buf->endpos;
        size_t amount_to_copy = MIN(available_at_end, size);

        memcpy(buf->data + buf->endpos, cptr, amount_to_copy);

        if (amount_to_copy < size) {
            buf->endpos = size - amount_to_copy;
            memcpy(buf->data, cptr + amount_to_copy, buf->endpos);
        } else
            buf->endpos += amount_to_copy;
    } else { /* Space available in the middle */
        memcpy(buf->data + buf->endpos, cptr, size);
        buf->endpos += size;
    }
}

static size_t buffer_read(void *buf, size_t size, size_t count, void *userdata, IO io) {
    struct Buffer *buffer = userdata;

    char *cptr = buf;
    size_t max = size*count;

    if (buffer->pos > buffer->endpos) {
        size_t remaining_to_end = buffer->capacity - buffer->pos;
        size_t initial_size = MIN(remaining_to_end, max);

        memcpy(cptr, buffer->data + buffer->pos, initial_size);
        buffer->pos += initial_size;
        max -= initial_size;

        if (max) {
            size_t remaining = MIN(buffer->endpos, max);
            memcpy(cptr + initial_size, buffer->data, remaining);
            buffer->pos = (buffer->pos + remaining) % buffer->capacity;
            initial_size += remaining;
        }

        return initial_size / size;
    } else {
        size_t initial_size = MIN(buffer->endpos - buffer->pos, max);

        memcpy(cptr, buffer->data + buffer->pos, initial_size);
        buffer->pos += initial_size / size * size;

        return initial_size / size;
    }
}

static size_t buffer_write(const void *buf, size_t size, size_t count, void *userdata, IO io) {
    struct Buffer *buffer = userdata;

    size_t max = size*count;
    int error = buffer_grow(buffer, max);
    io_set_error(io, error);

    if (error)
        return 0;

    buffer_append(buffer, buf, max);
    return count;
}

static int buffer_close(void *userdata, IO io) {
    UNUSED(io)

    FREE(userdata);

    return 0;
}

static const char *buffer_what(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    return "thread_buffer";
}

static const struct InputOutputDeviceCallbacks buffer_callbacks = {
    .read = buffer_read,
    .write = buffer_write,
    .open = NULL,
    .close = buffer_close,
    .flush = NULL,
    .clearerr = NULL,
    .stateSwitch = NULL,
    .tell = NULL,
    .tell64 = NULL,
    .seek = NULL,
    .seek64 = NULL,
    .what = buffer_what
};

IO io_open_thread_buffer() {
    struct Buffer *buffer = CALLOC(1, sizeof(*buffer));
    if (buffer == NULL)
        return NULL;

    buffer->capacity = 8;
    buffer->data = MALLOC(buffer->capacity);
    if (buffer->data == NULL) {
        FREE(buffer);
        return NULL;
    }

    IO result = io_open_custom(&buffer_callbacks, buffer, "rwb");
    if (result == NULL) {
        FREE(buffer);
        return NULL;
    }

    return result;
}
