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
struct GenericSetStruct;
struct StringSetStruct;
struct BinarySetStruct;
struct GenericMapStruct;
struct StringMapStruct;
struct GenericLinkedListStruct;

typedef struct VariantStruct *Variant; /* Simple variant type that can hold integers, floating point values, strings, or other containers. Simple conversions can be performed */
typedef struct GenericListStruct *GenericList; /* Simple list of generic pointers */
typedef struct StringListStruct *StringList; /* Simple list of NUL-terminated strings */
typedef struct GenericLinkedListStruct *GenericLinkedList; /* Simple singly-linked list of generic pointers */
typedef struct GenericSetStruct *GenericSet; /* Simple exclusive set of generic items, each element only appears once. Elements are in ascending order */
typedef struct StringSetStruct *StringSet; /* Simple exclusive set of NUL-terminated strings, each element only appears once. Elements are in ascending order */
typedef struct GenericMapStruct *GenericMap; /* Ordered map of items, containing generic pointers. Elements are in ascending order */
typedef struct StringMapStruct *StringMap; /* Simple exclusive map of NUL-terminated string keys with values, each element only appears once. Elements are NUL-terminated strings. Elements are in ascending order */

struct CommonContainerBaseStruct;
typedef void *Iterator; /* Simple iterator, does not need to be freed */

typedef struct BinaryStruct {
    size_t length;
    char *data;
} Binary;

Binary *binary_alloc(const char *str);
Binary *binary_alloc_n(const char *str, size_t str_length);
Binary *binary_copy(const Binary *other);
int binary_compare(const Binary *a, const Binary *b);
void binary_destroy(Binary *b);

/** @brief Structure containing the identity of a Parser function. See `Parser` for details. */
struct ParserIdentity {
    const char *fmt; /* IN, specifies extra user data containing parser-specific formatting information. */
    size_t fmt_length; /* IN, specifies length of extra data containing parser-specific formatting information. */

    const char *type; /* OUT, specifies type of parser if called with no input */
    int is_utf8; /* OUT, specifies whether format is UTF-8 compatible or not */
};

/** @brief Declares parser inside of a parser function.
 *
 * Example usage:
 *
 *     int parser(IO input, void *data, const CommonContainerBase *base, struct ParserIdentity *type) {
 *         PARSER_DECLARE("JSON", 1);
 *
 *         ...
 *     }
 *
 * @param parser_type String type of parser to return in the `type` field of the ParserIdentity.
 * @param fname A token that contains the name of the parser function being defined. This is to prevent self-references.
 * @param parser_is_utf8 Whether the parser is UTF-8 output (1) or not (0).
 */
#define PARSER_DECLARE(parser_type, fname, parser_is_utf8) \
    if (data == NULL || base == NULL || type == NULL) { /* Not passing a recipient object or ParserIdentity struct is an error */ \
        return CC_EINVAL; \
    } else if (input == NULL) { /* Just looking for the parser type? */ \
        if (type) { \
            type->type = (parser_type); \
            type->is_utf8 = (parser_is_utf8); \
        } \
        return 0; \
    } else if (base->parse && base->parse != (Parser) fname) { /* Serialize using value parser if same format */ \
        int err = 0; \
        struct ParserIdentity identity_ = *type; \
        if ((err = base->parse(NULL, NULL, NULL, &identity_)) != 0) \
            return err; \
        if (!strcmp((parser_type), identity_.type)) \
            return base->parse(input, data, base, type); \
    }

/** Parses a container or datatype from an IO device
 *
 * WARNING: The special case with @p input == NULL passed to this function is treated as a request for the parsing function to identify itself.
 * The parser function must identify itself with the `type` and `is_utf8` fields of @p type if @p input == NULL. These fields do not need to be set if @p input != NULL.
 * Any implementations of this function *MUST* comply with this convention, and using the `PARSER_DECLARE` macro will do this automatically.
 *
 * Extra parameters to the parser function can be passed in as a string with the `fmt` and `fmt_length` fields of the `type` parameter.
 * Strings passed as an argument in this manner should not be assumed to be NUL-terminated.
 *
 * PARSE ANYTHING INTO A VARIANT:
 *   Variant v = NULL;
 *   parse(io, &v, container_base_variant_recipe(), type);
 *
 * PARSE ONLY INTEGERS INTO A VARIANT:
 *   Variant v = variant_create_int(0);
 *   parse(io, &v, container_base_variant_recipe(), type);
 *
 * PARSE ONLY INTS INTO AN INT:
 *   int v;
 *   parse(io, &v, container_base_int_recipe(), type);
 *
 * PARSE ONLY INTS INTO A LIST OF INTS:
 *   GenericList v = genericlist_create(container_base_int_recipe());
 *   CommonContainerBase *base = genericlist_build_recipe(v);
 *   parse(io, &v, base, type);
 *   container_base_destroy_if_dynamic(base);
 *
 * @param input The IO device to read from and set errors to. If this parameter is NULL, it is treated as a request for the parsing function to identify itself, and all other parameters except for @p type can be NULL.
 * @param data A container or item containing value restrictions on what can be parsed.
 * @param base The type of the custom container or datatype to parse.
 * @param type A pointer to a `struct ParserIdentity` that contains extra information regarding the input and output of the parser. Required to always be non-NULL. If an error occurs, the output values placed in @p type are undefined.
 * @return An error that occurred while parsing or identifying the parser, or 0 on success. If an error occurs, the value in @p data is unspecified.
 */
typedef int (*Parser)(void *input, void *data, const struct CommonContainerBase *base, struct ParserIdentity *type);

/** @brief Structure containing the identity of a Serializer function. See `Serializer` for details. */
struct SerializerIdentity {
    const char *fmt; /* IN, specifies extra user data containing serializer-specific formatting information. */
    size_t fmt_length; /* IN, specifies length of extra data containing serializer-specific formatting information. */

    const char *type; /* OUT, specifies type of serializer if called with no output */
    int is_utf8; /* OUT, specifies whether format is UTF-8 compatible or not */
    size_t written; /* OUT, specifies how many characters were written with the current input */
};

/** @brief Declares serializer inside of a serializer function.
 *
 * Example usage:
 *
 *     int serializer(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
 *         SERIALIZER_DECLARE("JSON", 1);
 *
 *         ...
 *     }
 *
 * @param serializer_type String type of serializer to return in the `type` field of the SerializerIdentity.
 * @param fname A token that contains the name of the serializer function being defined. This is to prevent self-references.
 * @param serializer_is_utf8 Whether the serializer is UTF-8 output (1) or not (0).
 */
#define SERIALIZER_DECLARE(serializer_type, fname, serializer_is_utf8) \
    if (type == NULL) { /* Not passing a SerializerIdentity struct is an error */ \
        return CC_EINVAL; \
    } else if (output == NULL) { /* Just looking for the serializer type? */ \
        if (type) { \
            type->type = (serializer_type); \
            type->is_utf8 = (serializer_is_utf8); \
        } \
        return 0; \
    } else if (!base) { \
        return CC_EINVAL; \
    } else if (base->serialize && base->serialize != (Serializer) fname) { /* Serialize using value serializer if same format */ \
        int err = 0; \
        struct SerializerIdentity identity_ = *type; \
        if ((err = base->serialize(NULL, data, base, &identity_)) != 0) \
            return err; \
        if (!strcmp((serializer_type), identity_.type)) \
            return base->serialize(output, data, base, type); \
    } \
    type->written = 0;


/** Serializes a container or datatype to an IO device
 *
 * WARNING: The special case with @p output == NULL passed to this function is treated as a request for the serializing function to identify itself.
 * The serializer function must identify itself with the `type` and `is_utf8` fields of @p type if @p output == NULL. These fields do not need to be set if @p output != NULL.
 * Any implementations of this function *MUST* comply with this convention, and using the `SERIALIZER_DECLARE` macro will do this automatically.
 *
 * WARNING: In *ALL* cases, if the function returns 0, `type->written` *MUST* be set properly with the number of bytes this function wrote to the serializer.
 * Any implementations of this function *MUST* comply with this convention.
 *
 * Extra parameters to the serializer function can be passed in as a string with the `fmt` and `fmt_length` fields of the `type` parameter.
 * Strings passed as an argument in this manner should not be assumed to be NUL-terminated.
 *
 * @param output The IO device to read from and set errors to. If this parameter is NULL, it is treated as a request for the serializing function to identify itself, and all other parameters except for @p type can be NULL.
 * @param data The custom container or datatype to serialize. Simple types, like doubles or integers, should be passed as a pointer to the actual value.
 * @param base The type of the custom container or datatype to serialize.
 * @param type A pointer to a `struct SerializerIdentity` that contains extra information regarding the input and output of the serializer. Required to always be non-NULL. If an error occurs, the output values placed in @p type are undefined.
 * @return An error that occurred while serializing or identifying the serializer, or 0 on success.
 */
typedef int (*Serializer)(void *output, const void *data, const struct CommonContainerBaseStruct *base, struct SerializerIdentity *type);

typedef enum {
    CompareLess = -1,
    CompareEqual = 0,
    CompareGreater = 1,
    CompareUnordered = INT_MAX
} CompareResult;

/** Compares two items
 *
 * If performing a search for an item inside a collection, the item being searched for should be passed as the first parameter
 */
typedef int (*Compare)(const void *a, const void *b);

/** Returns the beginning of a collection
 *
 * The collection may be unordered, in which case no significance is placed on the beginning of a collection
 *
 * @param container The container to obtain the starting iterator from.
 * @return Returns an iterator pointing to the first element in @p container, or NULL if @p container is empty.
 */
typedef Iterator (*CollectionBegin)(const void *container);

/** Returns an element following the current iterator in a container, or NULL if the end of the container was reached
 *
 * The collection may be unordered, in which case no significance is placed on the beginning of a collection
 *
 * @param container The container to obtain the next iterator from.
 * @param it A valid iterator pointing to an item in the container.
 * @return Returns an iterator pointing to the element following @p it, or NULL if @p it points to the last element or key/value pair in the container.
 */
typedef Iterator (*CollectionNext)(const void *container, Iterator it);

/** Returns the key being stored at the position represented by the current iterator.
 *
 *  If the container is not a key/value store, this function should not be present and a function pointer with this signature should be NULL.
 *
 *  @param container The container to obtain the key from.
 *  @param it A valid iterator pointing to an item in the container.
 *  @return A pointer to the key requested.
 */
typedef const void *(*CollectionKey)(const void *container, Iterator it);

/** Returns the value being stored at the position represented by the current iterator
 *
 *  @param container The container to obtain the value from.
 *  @param it A valid iterator pointing to an item in the container.
 *  @return A pointer to the value requested.
 */
typedef void *(*CollectionValue)(const void *container, Iterator it);

/** Obtains a container's size
 *
 *  @param container The container to obtain the size of.
 *  @return Returns the size of the container; for key/value containers, the number of key/value pairs, or for list containers, the number of elements in the list.
 */
typedef size_t (*CollectionSize)(const void *container);

/** Finds an item in a container
 *
 *  For key/value containers, @p item is the key to search for.
 *  For list containers, @p item is an item to look for in the list.
 *
 *  @param container The container to search through.
 *  @param item For key/value containers, the key to look for. For list containers, the item to look for, starting from @p begin.
 *  @param begin For list containers only, the point at which to begin the search.
 *  @return Returns an iterator pointing to the found element, or NULL if the element could not be found.
 */
typedef Iterator (*CollectionKeyValueFind)(const void *container, const void *key);
typedef Iterator (*CollectionListFind)(const void *container, const void *item, Iterator begin);

/** Erases an element from a container
 *
 *  @param container The container to remove an element from.
 *  @param it A valid iterator pointing to an element in @p container to remove. If @p is NULL, the container will be cleared, i.e. all elements will be removed.
 *  @return An iterator to the element following @p it, or NULL if @p it points to the last element.
 */
typedef Iterator (*CollectionErase)(void *container, Iterator it);

/** Inserts an element into a key/value container
 *
 *  @param container The container to insert a key/value pair into.
 *  @param key The key to insert into the container. This value is moved, i.e. taken as is without copying.
 *  @param value The value to insert into the container. This value is moved, i.e. taken as is without copying.
 *  @return 0 on success, or an error that occurred on failure.
 */
typedef int (*CollectionKeyValueInsertMove)(void *container, void *key, void *value);

/** Inserts an element into a list container
 *
 *  @param container The container to insert a value into.
 *  @param value The value to insert into the container. This value is moved, i.e. taken as is without copying.
 *  @param it An iterator pointing to the position that the desired value should be placed in. If NULL, the new item is placed at the end of the list. The iterator may be invalidated after this function is called.
 *  @return 0 on success, or an error that occurred on failure.
 */
typedef int (*CollectionListInsertMove)(void *container, void *value, Iterator it);

/** Replaces an element in a container
 *
 *  For key/value containers, only the value can be replaced, the key is immutable.
 *
 *  @param container The container to replace an element in.
 *  @param it An iterator pointing to the existing element that should be replaced. The iterator will point to the same element after this function is called.
 *  @param value The value to insert into the container. This value is moved, i.e. tkaen as is without copying.
 *  @return 0 on success, or an error that occurred on failure.
 */
typedef int (*CollectionReplaceMove)(void *container, Iterator it, void *value);

/** Copies an item
 *
 * If the result is NULL and the parameter `p` is not NULL, an error is assumed to have occurred
 */
typedef void *(*Copier)(const void *p);

/** Deletes an item
 *
 * There is no way to signify an error occurring as deletions should never fail
 */
typedef void (*Deleter)(void *p);

typedef struct CommonContainerBaseStruct CommonContainerBase;

struct CommonContainerBaseStruct {
    Copier copier;
    Compare compare;
    Deleter deleter;
    Parser parse;
    Serializer serialize;
    CollectionSize collection_size;
    CollectionBegin collection_begin;
    CollectionNext collection_next;
    CollectionKey collection_get_key;
    CollectionValue collection_get_value;
    union {
        CollectionListFind list;
        CollectionKeyValueFind key_value;
    } collection_find;
    union {
        CollectionListInsertMove list;
        CollectionKeyValueInsertMove key_value;
    } collection_insert;
    CollectionErase collection_erase;
    CollectionReplaceMove collection_replace;
    CommonContainerBase *value_child; /* Used for child value type, if it exists */
    CommonContainerBase *key_child; /* References key type of children, if it exists */
    size_t size; /* Specifies element size in bytes in memory if a POD type (non-zero).
                    If not a POD type (zero), this type will be stored in a `void *`.
                    If a POD type (non-zero), the copier and deleter functions are irrelevant and will not be used */
    char dynamic; /* Specifies whether traits should be dynamically copied and stored (1) instead of statically referenced (0) */
};

/* Copies a container base and makes the newly allocated one dynamic
 *
 * This function deep copies all levels dynamically.
 *
 * Changes to the existing container base will not affect the newly allocated one.
 */
CommonContainerBase *container_base_copy(const CommonContainerBase *base);

/* Copies a container base only if the existing container base is dynamic
 *
 * This function shallow copies until the first static level and then shares the static container bases.
 *
 * This is useful for keeping a single static instance, or copying to a new dynamic instance if the existing one is dynamic.
 * For example, when creating a new container, this function should be used to initialize the container's base.
 */
CommonContainerBase *container_base_copy_if_dynamic(const CommonContainerBase *base);

/* Copies a container base only if the existing container is static
 *
 * This function deep copies only if some of the top `levels` levels of the current base are static
 * (there can't be a static parent of a dynamic child for obvious reasons).
 *
 * Settings `levels` == 0 will simply return the existing base. Setting `levels` == 1 will deep copy only if the top level is static,
 * and setting `levels` == 2 will deep copy if the top level or the child level are static.
 *
 * This is useful for "detaching" a base from the existing base instance for updating on a specific container instance.
 * For example, when editing a field in the base, this function should be called to ensure the edit is not performed on a static instance
 */
CommonContainerBase *container_base_copy_if_static(const CommonContainerBase *base, size_t levels);

/* Builds a container base from an element definition and a container definition.
 *
 * The `elements` container base will be added into the `container` container base as a subdefinition.
 * Container bases are kept static where possible (i.e. if `elements` is static, it will stay static,
 * and if `container` is static and `elements` is static and `container` already references `elements`, it will stay static as well)
 */
CommonContainerBase *container_base_build_container(const CommonContainerBase *elements, const CommonContainerBase *container);

/* Builds a container base from an element definition and a container definition.
 *
 * The `elements` container base will be added into the `container` container base as a subdefinition.
 * Container bases are kept static where possible (i.e. if `elements` is static, it will stay static,
 * and if `container` is static and `elements` is static and `container` already references `elements`, it will stay static as well)
 */
CommonContainerBase *container_base_build_key_value_container(const CommonContainerBase *keys, const CommonContainerBase *elements, const CommonContainerBase *container);

/* Destroys a container base instance
 *
 * There is no other destroy function for containers as static container bases need no destruction;
 * this one is called this solely to keep its operation unambiguous
 */
void container_base_destroy_if_dynamic(CommonContainerBase *base);

int generic_types_compatible_compare(const CommonContainerBase *lhs, const CommonContainerBase *rhs);

/** @brief A utility copier that copies a POD type to a newly allocated block of memory and returns it
 *
 * This function cannot be used as a Copier parameter.
 */
void *generic_pod_copy_alloc(const void *p, size_t bytes);

/** @brief A copier function that always returns NULL
 *
 * Use this function as a copier when you don't want generic containers to copy objects
 */
void *generic_nocopy(const void *p);

/** @brief A copier function that simply returns its argument
 *
 * Use this function as a copier when you want generic containers to treat objects as atomic values
 */
void *generic_identitycopy(const void *p);

/** @brief A destructor function that does nothing to its argument
 *
 * Use this function as a destructor when you don't want generic containers to free() their objects
 */
void generic_nofree(void *p);

#define REFERENCE(type, value) ((void *) ((type [1]) {(value)}))
#define VALUE(type, generic) (*((type *) (generic)))

#endif // COMMON_H
