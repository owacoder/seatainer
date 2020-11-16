/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef CONTAINER_SBUFFER_H
#define CONTAINER_SBUFFER_H

#include "common.h"

typedef struct Buffer {
    char small_data[32];
    size_t capacity;
    size_t length;
    char *data;
} Buffer;

void buffer_init(Buffer *buf);
void buffer_init_reserve(Buffer *buf, size_t size);
void buffer_clear(Buffer *buf);
int buffer_append(Buffer *buf, const char *str);
int buffer_append_chr(Buffer *buf, int chr);
int buffer_append_n(Buffer *buf, const char *str, size_t len);
char *buffer_take(Buffer *buf);
void buffer_destroy(Buffer *buf);

#endif // CONTAINER_SBUFFER_H
