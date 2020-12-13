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
//typedef struct StringLinkedListStruct *StringLinkedList; /* Simple singly-linked list of NUL-terminated strings */
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
 * @param fname A token that contains the name of the serializer function being defined. This is to prevent self-references.
 * @param serializer_is_utf8 Whether the serializer is UTF-8 output (1) or not (0).
 */
#define SERIALIZER_DECLARE(serializer_type, fname, serializer_is_utf8) \
    if (output == NULL) { /* Just looking for the serializer type? */ \
        if (type) { \
            type->type = (serializer_type); \
            type->is_utf8 = (serializer_is_utf8); \
        } \
        return 0; \
    } else if (!base) \
        return CC_EINVAL; \
    else if (base->serialize && base->serialize != (Serializer) fname) { /* Serialize using value serializer if same format */ \
        int err = 0; \
        struct SerializerIdentity identity; \
        if ((err = base->serialize(NULL, NULL, NULL, &identity)) != 0) \
            return err; \
        if (!strcmp((serializer_type), identity.type)) \
            return base->serialize(output, data, base, NULL); \
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
typedef int (*Serializer)(void *output, const void *data, const struct CommonContainerBaseStruct *base, struct SerializerIdentity *type);

typedef enum {
    CompareError = -2,
    CompareLess = -1,
    CompareEqual = 0,
    CompareGreater = 1,
    CompareUnordered = 2
} CompareResult;

/** Compares two items
 *
 * If performing a search for an item inside a collection, the item being searched for should be passed as the first parameter
 */
typedef int (*Compare)(const void *a, const void *b);

/** Returns the beginning of a collection
 *
 * The collection may be unordered, in which case no significance is placed on the beginning of a collection
 */
typedef Iterator (*CollectionBegin)(const void *container);

/** Returns an element following the current iterator in a container, or NULL if the end of the container was reached
 *
 * The collection may be unordered, in which case no significance is placed on the beginning of a collection
 */
typedef Iterator (*CollectionNext)(const void *container, Iterator it);

/** Returns the key being stored at the position represented by the current iterator, or NULL if the container doesn't support keys
 */
typedef const void *(*CollectionKey)(const void *container, Iterator it);

/** Returns the value being stored at the position represented by the current iterator
 */
typedef void *(*CollectionValue)(const void *container, Iterator it);

/** Obtains a container's size
 */
typedef size_t (*CollectionSize)(const void *container);

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
