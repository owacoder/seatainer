/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef COMMON_H
#define COMMON_H

#include "../seaerror.h"

/* Basic container operations needed:
 *  - Creation, initialization, and copying
 *  - Insertion (at end or in middle)
 *  - Deletion (by index or by element)
 *  - Enumeration
 *  - Search (subset of enumeration)
 *  - Comparison
 *  - Size and clear
 *  - Destruction
 */

struct GenericListStruct;
struct BinaryListStruct;
struct StringListStruct;
struct StringSetStruct;
struct BinarySetStruct;
struct GenericMapStruct;
struct StringMapStruct;

typedef struct VariantStruct *Variant; /* Simple variant type that can hold integers, floating point values, strings, or other containers. Simple conversions can be performed */
typedef struct GenericListStruct *GenericList; /* Simple list of generic pointers */
typedef struct StringListStruct *StringList; /* Simple list of NUL-terminated strings */
typedef struct BinaryListStruct *BinaryList; /* Simple list of binary strings or other sized plain-old-data */
typedef struct GenericLinkedListStruct *GenericLinkedList; /* Simple singly-linked list of generic pointers */
typedef struct StringLinkedListStruct *StringLinkedList; /* Simple singly-linked list of NUL-terminated strings */
typedef struct StringSetStruct *StringSet; /* Simple exclusive set of NUL-terminated strings, each element only appears once. Elements are in ascending order */
typedef struct BinarySetStruct *BinarySet; /* Simple exclusive set of binary strings or other sized plain-old-data. Elements are in ascending order */
typedef struct GenericMapStruct *GenericMap; /* Ordered map of binary strings, containing generic pointers. Elements are in ascending order */
typedef struct StringMapStruct *StringMap; /* Simple exclusive map of NUL-terminated string keys with values, each element only appears once. Elements are NUL-terminated strings. Elements are in ascending order */
typedef void *Iterator; /* Simple iterator, does not need to be freed */

/** Returns a new custom datatype from an IO device
 *
 * @param io The IO device to read from and set errors to
 * @return A custom container dynamically allocated from the parsed data, or NULL if an error occurred
 */
typedef void *(*Parser)(void *io);

/** @brief Structure containing the identity of a Serializer function. See `Serializer` for details. */
struct SerializerIdentity {
    const char *type;
    int is_utf8;
};

/** @brief Declares serializer inside of a serializer function.
 *
 * Example usage:
 *
 *     int serializer(IO output, const void *data, struct SerializerIdentity *type) {
 *         SERIALIZER_DECLARE("JSON", 1);
 *
 *         ...
 *     }
 *
 * @param serializer_type String type of serializer to return in the `type` field of the SerializerIdentity.
 * @param serializer_is_utf8 Whether the serializer is UTF-8 output (1) or not (0).
 */
#define SERIALIZER_DECLARE(serializer_type, serializer_is_utf8) \
    if (output == NULL) { \
        if (type) { \
            type->type = (serializer_type); \
            type->is_utf8 = (serializer_is_utf8); \
        } \
        return 0; \
    }

/** Serializes a container or datatype to an IO device
 *
 * The special case with @p output == NULL passed to this function is treated as a request for the serializing function to identify itself.
 * Any implementations of this function *MUST* comply with this convention.
 *
 * @param output The IO device to read from and set errors to. If this parameter is NULL, it is treated as a request for the serializing function to identify itself.
 * @param data The custom container or datatype to serialize
 * @param type A pointer to a `struct SerializerIdentity` that receives the type of this serializer (e.g. {type: 'JSON', is_utf8: 1}) if @p type is not NULL. If an error occurs, the value placed in @p type is undefined.
 * @return An error that occurred while serializing or identifying the serializer, or 0 on success.
 */
typedef int (*Serializer)(void *output, const void *data, struct SerializerIdentity *type);

typedef struct CommonContainerBaseStruct {
    Parser parse;
    Serializer serialize;
    char cvt_expects_variant; /* Whether the parser and serializer expect Variant values (1) or work with raw (possibly custom) data instead (0) */
} CommonContainerBase;

CommonContainerBase build_container_base(Parser parse, Serializer serialize, int cvt_expects_variant);

typedef struct BinaryStruct {
    size_t length;
    char *data;
} Binary;

typedef int (*Compare)(const void *a, const void *b);
typedef int (*BinaryCompare)(const Binary *a, const Binary *b);
typedef int (*StringCompare)(const char *a, const char *b);
typedef void *(*Copier)(const void *p);
typedef void (*Deleter)(void *p);

CommonContainerBase empty_container_base(void);

int generictypes_compatible_compare(Compare compare_lhs, Compare compare_rhs,
                                    Copier copier_lhs, Copier copier_rhs,
                                    Deleter deleter_lhs, Deleter deleter_rhs);

int binary_compare(const Binary *a, const Binary *b);

/** @brief Searches for a binary token inside a binary string
 *
 *  Operates the same as strstr, but for binary strings.
 *
 * @param string The binary data to search in (the haystack)
 * @param string_len A pointer to the length of data to search in (the size of the haystack). This field is updated to a new smaller size, unless NULL is returned.
 * @param token The binary token to search for (the needle)
 * @param token_len The length of data to search for (the size of the needle)
 * @return Returns the location where token was found, or NULL if @p token was not found. If a non-NULL value is returned, the value pointed to by @p string_len is updated with the length to the end of the string from the return value.
 */
const char *binstr_search(const char *string, size_t *string_len, const char *token, size_t token_len);

/** @brief A destructor function that does nothing to its argument
 *
 * Use this function as a destructor when you don't want generic containers to free() their objects
 */
void generic_nofree(void *p);

#endif // COMMON_H
