/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "sbuffer.h"
#include "utility.h"
#include <limits.h>

void buffer_init(Buffer *buf) {
    memset(buf, 0, sizeof(*buf));
    buf->data = buf->small_data;
    buf->capacity = sizeof(buf->small_data);
}

void buffer_init_reserve(Buffer *buf, size_t size) {
    buffer_init(buf);

    if (size < buf->capacity)
        return;

    char *data = MALLOC(size+1);
    if (data != NULL) {
        buf->capacity = size;
        buf->data = data;
    }
}

void buffer_clear(Buffer *buf) {
    buf->length = 0;
    buf->data[0] = 0;
}

int buffer_append(Buffer *buf, const char *str) {
    return buffer_append_n(buf, str, strlen(str));
}

int buffer_append_chr(Buffer *buf, int chr) {
    if (chr != (chr & UCHAR_MAX))
        return CC_EINVAL;

    char c = chr;
    return buffer_append_n(buf, &c, 1);
}

int buffer_append_n(Buffer *buf, const char *str, size_t len) {
    size_t required_new_capacity = buf->length + len + 1;

    if (required_new_capacity <= buf->capacity) {
        memcpy(buf->data + buf->length, str, len);
        buf->length += len;
        buf->data[buf->length] = 0;
        return 0;
    }

    size_t new_capacity = MAX(16, buf->capacity + (buf->capacity >> 1));

    char *new_data = NULL;
    size_t capacity = new_capacity;
    do {
        new_data = REALLOC(buf->data == buf->small_data? NULL: buf->data, new_capacity);
        if (new_data != NULL) {
            if (buf->data == buf->small_data)
                memcpy(new_data, buf->small_data, buf->length);

            buf->data = new_data;
            memcpy(buf->data + buf->length, str, len);
            buf->capacity = new_capacity;
            buf->length += len;
            buf->data[buf->length] = 0;
            return 0;
        }

        if (capacity == new_capacity)
            capacity = required_new_capacity;
    } while (capacity != required_new_capacity);

    return CC_ENOMEM;
}

char *buffer_take(Buffer *buf) {
    char *buffer = buf->data;

    if (buffer == buf->small_data)
        buffer = strdup(buffer);

    if (buffer)
        buffer_init(buf);

    return buffer;
}

void buffer_destroy(Buffer *buf) {
    if (buf->data != buf->small_data)
        FREE(buf->data);

    buffer_init(buf);
}
