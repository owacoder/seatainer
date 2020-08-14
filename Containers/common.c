/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "common.h"

/* These structs really aren't used at all, they're just present to emit compiler warnings if assigning one container type to another.
 * Custom extensions of the generic types should use the same pattern:
 *
 * container.h:
 *
 *      typedef struct ContainerStruct *Container;
 *
 * container.c:
 *
 *      struct ContainerStruct {}; // Dummy struct just for type-safety warnings
 *
 *      void container_function(Container c) {
 *          generic_function((GenericContainer) c); // Cast to generic container type when it needs to be used
 *      }
 *
 * This prevents the user from unknowningly assigning one "type" of void * to another variable.
 */
#if 0
struct ContainerStruct {};
#endif

int generictypes_compatible_compare(Compare compare_lhs, Compare compare_rhs, Copier copier_lhs, Copier copier_rhs, Deleter deleter_lhs, Deleter deleter_rhs) {
    /* If copiers and deleters are the same, it's the same type */
    if (copier_lhs == copier_rhs && deleter_lhs == deleter_rhs)
        return 0;
    /* If no copier, but compare and deleters are the same, it's the same type */
    else if (copier_lhs == NULL && copier_rhs == NULL && compare_lhs == compare_rhs && deleter_lhs == deleter_rhs)
        return 0;
    /* Otherwise, the types are different */
    else if (copier_lhs || copier_rhs) {
        /* NOTE: this only works on platforms where it's safe to cast function pointers to uintptr_t (which is just about any modern system), but it may fail for other systems */
        uintptr_t lhs = (uintptr_t) copier_lhs;
        uintptr_t rhs = (uintptr_t) copier_rhs;

        return (lhs > rhs) - (lhs < rhs);
    } else if (compare_lhs || compare_rhs) {
        /* NOTE: this only works on platforms where it's safe to cast function pointers to uintptr_t (which is just about any modern system), but it may fail for other systems */
        uintptr_t lhs = (uintptr_t) compare_lhs;
        uintptr_t rhs = (uintptr_t) compare_rhs;

        return (lhs > rhs) - (lhs < rhs);
    } else {
        /* NOTE: this only works on platforms where it's safe to cast function pointers to uintptr_t (which is just about any modern system), but it may fail for other systems */
        uintptr_t lhs = (uintptr_t) deleter_lhs;
        uintptr_t rhs = (uintptr_t) deleter_rhs;

        return (lhs > rhs) - (lhs < rhs);
    }
}

int binary_compare(const Binary *a, const Binary *b) {
    size_t max = MIN(a->length, b->length);
    int cmp = memcmp(a->data, b->data, max);

    if (cmp)
        return cmp;
    else if (a->length < max)
        return -1;
    else if (b->length < max)
        return 1;
    else
        return 0;
}

const char *binstr_search(const char *string, size_t *string_len, const char *token, size_t token_len) {
    if (*string_len < token_len)
        return NULL;
    else {
        for (size_t len = *string_len; len >= token_len; ++string, --len) {
            if (memcmp(string, token, token_len) == 0) {
                *string_len = len;
                return string;
            }
        }
    }

    return NULL;
}
