#ifndef TINYMALLOC_H
#define TINYMALLOC_H

#include <string.h>

#ifndef NULL
#define NULL ((void *) 0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

void *tiny_malloc(size_t size);
void *tiny_calloc(size_t size, size_t count);
void *tiny_realloc(void *ptr, size_t size);
void tiny_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif // TINYMALLOC_H
