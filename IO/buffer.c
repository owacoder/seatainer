/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "buffer.h"

#include "../seaerror.h"

struct Buffer {
    char *data;
    size_t capacity, pos, endpos; /* pos points to first character in buffer, endpos points past last character in buffer, if equal, buffer is empty */
};

static size_t buffer_size_used(struct Buffer *buf) {
    if (buf->pos <= buf->endpos)
        return buf->endpos - buf->pos;
    else /* buf->pos > buf->endpos */
        return buf->capacity - (buf->pos - buf->endpos);
}

static size_t buffer_size_empty(struct Buffer *buf) {
    return buf->capacity - buffer_size_used(buf);
}

static size_t buffer_contiguous_space_available_at_end(struct Buffer *buf) {
    if (buf->pos <= buf->endpos)
        return buf->capacity - buf->endpos;
    else
        return buf->pos - buf->endpos;
}

static size_t buffer_contiguous_space_used_at_end(struct Buffer *buf) {
    if (buf->pos <= buf->endpos)
        return buf->endpos - buf->pos;
    else
        return buf->capacity - buf->pos;
}

static int buffer_grow(struct Buffer *buf, size_t size_of_data_to_append) {
    size_of_data_to_append += 1;

    if (buffer_size_empty(buf) < size_of_data_to_append) {
        const size_t currently_used = buffer_size_used(buf);
        const size_t new_size = MAX(buf->capacity + (buf->capacity >> 1), currently_used + size_of_data_to_append);

        if (buf->pos != 0) { /* If data is not aligned to beginning already */
            char *new_data = MALLOC(new_size);
            if (new_data == NULL)
                return CC_ENOMEM;

            if (buf->pos <= buf->endpos) {
                memcpy(new_data, buf->data + buf->pos, currently_used);
            } else if (buf->pos > buf->endpos) {
                const size_t offset = buf->capacity - buf->pos;
                memcpy(new_data, buf->data + buf->pos, offset);
                memcpy(new_data + offset, buf->data, buf->endpos);
            }

            FREE(buf->data);
            buf->data = new_data;
            buf->capacity = new_size;
            buf->pos = 0;
            buf->endpos = currently_used;
        } else { /* Data already aligned to start of buffer */
            char *new_data = REALLOC(buf->data, new_size);
            if (new_data == NULL)
                return CC_ENOMEM;

            buf->capacity = new_size;
            buf->data = new_data;
        }
    }

    return 0;
}

/* Buffer must have at least `size` available space for data before this function is called */
static void buffer_append(struct Buffer *buf, const void *data, size_t size) {
    const char *cptr = data;
    const size_t contiguous_to_end = buffer_contiguous_space_available_at_end(buf);

    if (contiguous_to_end >= size) {
        memcpy(buf->data + buf->endpos, cptr, size);
        buf->endpos += size;
        buf->endpos %= buf->capacity;
    } else {
        memcpy(buf->data + buf->endpos, cptr, contiguous_to_end);
        size -= contiguous_to_end;

        memcpy(buf->data, cptr + contiguous_to_end, size);
        buf->endpos = size;
    }
}

static size_t buffer_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    struct Buffer *buffer = userdata;

    char *cptr = ptr;
    size_t max = size*count;
    size_t available = buffer_size_used(buffer);
    const size_t contiguous = buffer_contiguous_space_used_at_end(buffer);

    if (available < max)
        max = available / size * size;

    if (max <= contiguous) {
        memcpy(cptr, buffer->data + buffer->pos, max);
        buffer->pos += max;
        buffer->pos %= buffer->capacity;
    } else {
        memcpy(cptr, buffer->data + buffer->pos, contiguous);
        memcpy(cptr + contiguous, buffer->data, max - contiguous);

        buffer->pos = max - contiguous;
    }

    return max / size;
}

static size_t buffer_write(const void *ptr, size_t size, size_t count, void *userdata, IO io) {
    struct Buffer *buffer = userdata;

    size_t max = size*count;
    int error = buffer_grow(buffer, max);
    io_set_error(io, error);

    if (error)
        return 0;

    buffer_append(buffer, ptr, max);
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

static unsigned long buffer_flags(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    return IO_FLAG_SUPPORTS_NO_STATE_SWITCH;
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
    .flags = buffer_flags,
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
