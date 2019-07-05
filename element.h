/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef CCELEMENT_H
#define CCELEMENT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "platforms.h"

#if C11
#define CC_COPY(container) CC_COPY_EX((container), NULL, NULL)
#define CC_COPY_EX(container, constructor, destructor) \
    (_Generic((container), HVector: cc_v_copy, \
                           HLinkedList: cc_ll_copy, \
                           HDoublyLinkedList: cc_dll_copy)((container), (constructor), (destructor)))

#define CC_FOR_EACH(container, flags, callback) CC_FOR_EACH_EX((container), (flags), (callback), NULL)
#define CC_FOR_EACH_EX(container, flags, callback, userdata) \
    (_Generic((container), HVector: cc_v_iterate, \
                           HLinkedList: cc_ll_iterate \
                           HDoublyLinkedList: cc_dll_iterate)((container), (flags), (callback), (userdata)))

#define CC_R_FOR_EACH(container, flags, callback) CC_FOR_EACH_EX((container), (flags) | CC_BACKWARD, (callback), NULL)
#define CC_R_FOR_EACH_EX(container, flags, callback, userdata) \
    (_Generic((container), HVector: cc_v_iterate, \
                           HDoublyLinkedList: cc_dll_iterate)((container), (flags) | CC_BACKWARD, (callback), (userdata)))

#define CC_DESTROY(item) CC_DESTROY_EX((item), NULL)
#define CC_DESTROY_EX(item, destructor) \
    (_Generic((item), HElementData: cc_el_destroy((HElementData) (item)), \
                      HContainerElementMetaData: cc_el_kill_metadata((HContainerElementMetaData) (item)), \
                      HVector: cc_v_destroy((HVector) (item), (destructor)), \
                      HLinkedList: cc_ll_destroy((HLinkedList) (item), (destructor)), \
                      HDoublyLinkedList: cc_dll_destroy((HDoublyLinkedList) (item), (destructor))))

#define CC_ASSIGN(element, item) \
    (_Generic((item), char: cc_el_assign_char, \
                      signed char: cc_el_assign_signed_char, \
                      unsigned char: cc_el_assign_unsigned_char, \
                      signed short: cc_el_assign_signed_short, \
                      unsigned short: cc_el_assign_unsigned_short, \
                      signed int: cc_el_assign_signed_int, \
                      unsigned int: cc_el_assign_unsigned_int, \
                      signed long: cc_el_assign_signed_long, \
                      unsigned long: cc_el_assign_unsigned_long, \
                      signed long long: cc_el_assign_signed_long_long, \
                      unsigned long long: cc_el_assign_unsigned_long_long, \
                      float: cc_el_assign_float, \
                      double: cc_el_assign_double, \
                      void *: cc_el_assign_voidp, \
                      HVector: cc_el_assign_vector, \
                      HLinkedList: cc_el_assign_linked_list, \
                      HDoublyLinkedList: cc_el_assign_doubly_linked_list)((element), (item)))

#define CC_INSERT_AFTER(container, iterator, data) CC_INSERT_AFTER_EX((container), (iterator), (data), NULL)
#define CC_INSERT_AFTER_EX(container, iterator, data, constructor) \
    (_Generic((container), HVector: cc_v_insert((container), (iterator)+1, (data), (constructor)), \
                           HLinkedList: cc_ll_insert_after((container), (iterator), (data), (constructor)), \
                           HDoublyLinkedList: cc_dll_insert_after((container), (iterator), (data), (constructor))))

#endif

/* Swap */
#define CC_SWAP(type, x, y) {type tmp = (x); (x) = (y); (y) = tmp;}

/* Return values (error codes) */
#define CC_LESS_THAN -1 /* Left < Right */
#define CC_OK 0 /* Success, equality */
#define CC_GREATER_THAN 1 /* Left > Right */
#define CC_FAILURE 2 /* Failure to perform operation */
#define CC_NO_SUCH_METHOD 3 /* No method found to perform operation */
#define CC_NO_MEM 8 /* Out of memory; could not allocate */
#define CC_BAD_PARAM 9 /* Bad parameter provided */
#define CC_TYPE_MISMATCH 10 /* Type mismatch, cannot work with provided element types */

/*
 * Flags layout
 *
 * +---...----------------+-----------------------+------------------------+-----------------------+-------------------+--------------+
 * |         31-8: Unused | 7-6: Search semantics | 5: Copy/move-semantics | 4: Single/multi-value | 3-1: Organization | 0: Direction |
 * +---...----------------+-----------------------+------------------------+-----------------------+-------------------+--------------+
 *
 */
/* Direction flags */
#define CC_FORWARD 0
#define CC_BACKWARD 1

/* Linear self-organizing flags */
#define CC_ORGANIZE_NONE (0 << 1)
/* CC_ORGANIZE_MTF = Move found element to front (or back, depending on direction flag) of list */
#define CC_ORGANIZE_MTF (1 << 1)
#define CC_ORGANIZE_TRANSPOSE (2 << 1)
#define CC_ORGANIZE_COUNT (3 << 1)
#define CC_ORGANIZE_AUTO (4 << 1)

/* Multi- or single-value flags */
#define CC_SINGLE_VALUE (0 << 4)
#define CC_MULTI_VALUE (1 << 4)

/* Move-semantics flags */
#define CC_COPY_VALUE (0 << 5)
/* Moving values works by taking ownership of the data STORED in an HElementData and clearing the reference to it, NOT by taking ownership of the HElementData */
#define CC_MOVE_VALUE (1 << 5)

/* Search-semantics flags */
#define CC_SEARCH_LINEAR (0 << 6)
#define CC_SEARCH_BINARY (1 << 6)

/* value extraction from flags */
#define CC_DIRECTION(flags) ((flags) & 1)
#define CC_ORGANIZATION(flags) ((flags) & (7 << 1))
#define CC_MULTIVALUE(flags) ((flags) & (1 << 4))
#define CC_MOVE_SEMANTICS(flags) ((flags) & (1 << 5))
#define CC_SEARCH_SEMANTICS(flags) ((flags) & (3 << 6))

/* Error handlers */
#ifdef CC_TYPE_MISMATCH_ABORT
#define CC_TYPE_MISMATCH_HANDLER(message, expected, got) do {fprintf(stderr, "%s (%s:%d) - expected %s, but %s was provided: %s\n", __FUNCTION__, __FILE__, __LINE__, cc_el_typename((expected)), cc_el_typename((got)), (message)); abort();} while (0)
#else
#define CC_TYPE_MISMATCH_HANDLER(message, expected, got) return CC_TYPE_MISMATCH
#endif

#ifdef CC_NO_MEM_ABORT
#define CC_NO_MEM_HANDLER(message) do {fprintf(stderr, "%s (%s:%d): %s\n", __FUNCTION__, __FILE__, __LINE__, (message)); abort();} while (0)
#else
#define CC_NO_MEM_HANDLER(message) return CC_NO_MEM
#endif

#ifdef CC_BAD_PARAM_ABORT
#define CC_BAD_PARAM_HANDLER(message) do {fprintf(stderr, "%s (%s:%d): %s\n", __FUNCTION__, __FILE__, __LINE__, (message)); abort();} while (0)
#else
#define CC_BAD_PARAM_HANDLER(message) return CC_BAD_PARAM
#endif

#ifdef CC_NO_SUCH_METHOD_ABORT
#define CC_NO_SUCH_METHOD_HANDLER(message) do {fprintf(stderr, "%s (%s:%d): %s\n", __FUNCTION__, __FILE__, __LINE__, (message)); abort();} while (0)
#else
#define CC_NO_SUCH_METHOD_HANDLER(message) return CC_NO_SUCH_METHOD
#endif

/* Internal convenience macros */
#define CC_RETURN_ON_ERROR(x) \
    do { \
        int ret = CC_OK; \
        ret = (x); \
        if (ret != CC_OK) \
            return ret; \
    } while(0)

#define CC_CLEANUP_ON_ERROR(x, cleanup) \
    do { \
        int ret = CC_OK; \
        ret = (x); \
        if (ret != CC_OK) \
        { \
            cleanup; \
        } \
    } while(0)

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum
    {
        El_Null,
        El_Char,
        El_SignedChar,
        El_UnsignedChar,
        El_SignedShort,
        El_UnsignedShort,
        El_SignedInt,
        El_UnsignedInt,
        El_SignedLong,
        El_UnsignedLong,
        El_SignedLongLong,
        El_UnsignedLongLong,
        El_Float,
        El_Double,
        El_VoidPtr,
        El_String,
        El_Vector,
        El_LinkedList,
        El_DoublyLinkedList,
        El_HashTable
    } ContainerElementType;

    typedef void *Iterator;

    typedef struct {
        void *opaque[4];
    } ExIterator;

    Iterator cc_el_null_iterator();
    ExIterator cc_el_null_ex_iterator();
    ExIterator cc_el_ex_iterator_from_iterator(Iterator iterator);

    /* Returns 1 if equal, 0 if not equal */
    int ExIteratorEquals(ExIterator lhs, ExIterator rhs);
    int ExIteratorNonNull(ExIterator it);

    struct String;

    /** A generic 8-bit character string. */
    typedef struct String *HString;

    struct Vector;
    /** A generic vector.
      *
      * Vectors are a dynamic array that contains the underlying data consecutively without space overhead per element.
      */
    typedef struct Vector *HVector;

    struct LinkedList;
    /** A generic linked list.
     *
     * Linked lists are a dynamic list that contains each element linked to the next with some space overhead per element.
     */
    typedef struct LinkedList *HLinkedList;

    struct DoublyLinkedList;
    /** A generic doubly linked list.
     *
     * Doubly linked lists are a dynamic list that contains each element linked to the next and the previous with some space overhead per element.
     */
    typedef struct DoublyLinkedList *HDoublyLinkedList;

    struct HashTable;
    /** A generic hash table.
     *
     * Hash tables are a collection that allows for fast lookup operations for a specific element. Insertion performance is comparable to a linked list.
     */
    typedef struct HashTable *HHashTable;

    struct ElementData;

    /** A generic element type.
     *
     * Elements store data of any type, but also can be references to external data. This allows modification of existing containers by using a reference element.
     */
    typedef struct ElementData *HElementData;

    /** A const generic element type.
     *
     * Identical to `HElementData`, just made `const` so no editing is possible.
     */
    typedef const struct ElementData *HConstElementData;

    struct ContainerElementMetaData;
    /** A generic element metadata type.
     *
     * Metadata, referenced by an element or container, allows constructors, destructors, and comparison operations to be defined per type.
     */
    typedef struct ContainerElementMetaData *HContainerElementMetaData;

    /* Regarding callback invariants:
     *
     *     Constructors: may allocate and fail, but must _NOT_ adjust the storage pointer
     *     Copy constructors: may allocate and fail, but must _NOT_ modify the source and must _NOT_ adjust the destination's storage pointer
     *     Compare: may allocate and fail, but must _NOT_ modify either parameter
     *     Destructors: no failure reporting mechanism is available, so failure _MUST_ terminate the program (or provide some other means of working around the error)
     *
     */
    typedef int (*ElementDataCallback)(HElementData data);
    typedef int (*ExtendedElementDataCallback)(HElementData data, void *userdata);
    typedef int (*ExtendedElementDualDataCallback)(HElementData lhs, HElementData rhs, void *userdata);
    typedef int (*ElementDualDataCallback)(HElementData lhs, HElementData rhs);
    typedef int (*StringCompareCallback)(char lhs, char rhs);

    /** @brief Returns a Seatainer error description as a human-readable string.
      *
      * @param error An error code returned by a Seatainer container function.
      * @return A human-readable string containing a description of the response code.
      */
    const char *cc_el_error_reason(int error);

    /* Returns the (either internal or external) storage location pointing to the actual data */
    /** @brief Returns the storage location pointing to the actual data.
      *
      * In the case of primitive types, the returned location points to the actual data storage. In the case of containers, the returned location points to the handle to the container.
      *
      * @param data An element to get the storage location of. Must not be `NULL`.
      * @return The storage location pointing to the data.
      */
    void *cc_el_storage_location(HElementData data);

    /** @brief Returns a reference to the pointer pointing to the actual data.
     *
     * @p data can be used as a reference to the actual data site, allowing modification (cc_el_assign_xxx() family of functions).
     * If the storage pointer is NULL, an internal memory block will be used to store the value (that memory is only accessible through `cc_el_storage_location()`).
     *
     * **DO NOT CALL** in a user-defined callback (constructor, copy-constructor, or destructor).
     *
     * @param data An element to get the pointer to the internal storage of. Must not be `NULL`.
     * @return A pointer to the external storage pointer.
     */
    void **cc_el_storage_location_ptr(HElementData data);

    /** @brief Returns whether @p data references external data or owns a copy.
     *
     * @param data The element to test.
     * @return 1 if @p data references external data, or 0 if @p data owns the data
     */
    int cc_el_is_external_reference(HConstElementData data);

    /** @brief Clears a reference to external data, reverting to using the internal data.
     *
     * Identical to `*cc_el_storage_location_ptr(data) = NULL;`.
     *
     * @param data The element to clear the reference of.
     */
    void cc_el_clear_external_reference(HElementData data);

    /* Initializes new element handle. Does _NOT_ take ownership of the metadata!
     * If the return value is NULL, returns the error in `*err`, if provided. If the return value is non-null, *err is not modified
     * The element is also constructed, using `construct` if non-null, or the metadata constructor otherwise
     */
    HElementData cc_el_init(ContainerElementType type, HContainerElementMetaData metadata, ElementDataCallback construct, int *err);
    /* Initializes a new element at the specified buffer
     * Returns CC_BAD_PARAM if the buffer is not big enough, or a constructor response error code
     * Returns CC_OK if all went well
     */
    int cc_el_init_at(void *buf, size_t buffer_size, ContainerElementType type, HContainerElementMetaData metadata, ElementDataCallback construct);
    /* Performs a move (shallow copy) of the data from src to dest. `src` is then cleared to the default-constructed type and should still be freed with cc_el_destroy()
     * Returns the constructor response error code
     * Returns CC_OK if all went well
     */
    int cc_el_move_contents(HElementData dest, HConstElementData src);
    /* Performs a copy of the data from src to dest
     * Returns the constructor response error code
     * Returns CC_OK if all went well
     */
    int cc_el_copy_contents(HElementData dest, HConstElementData src);
    /* Destroys an element at a specified location. The handle should no longer be used, but is not freed
     * Returns CC_OK */
    int cc_el_destroy_at(HElementData data);
    /* Destroys an element and invalidates the specified handle. The handle should no longer be used
     * Returns CC_OK */
    int cc_el_destroy(HElementData data);
    /* Destroys an element that was used as a reference and invalidates the specified handle, but does NOT free! The handle should no longer be used
     * Returns CC_OK */
    INLINE int cc_el_destroy_reference_at(HElementData data) INLINE_DEFINITION({
        *cc_el_storage_location_ptr(data) = NULL;
        return cc_el_destroy_at(data);
    })
    /* Destroys an element that was used as a reference and invalidates the specified handle. The handle should no longer be used
     * Returns CC_OK */
    INLINE int cc_el_destroy_reference(HElementData data) INLINE_DEFINITION({
        *cc_el_storage_location_ptr(data) = NULL;
        return cc_el_destroy(data);
    })
    /* Returns the size of one element */
    size_t cc_el_sizeof();

    /* Returns to assign data to the specified element
     *
     * Returns CC_BAD_PARAM if the element is a reference to an external location
     *  and the new type is not the same as the currently stored type
     * Returns CC_OK on success
     *
     * The metadata reference is cleared to NULL if the stored type changes
     */
    int cc_el_assign_char(HElementData data, char d);
    int cc_el_assign_signed_char(HElementData data, signed char d);
    int cc_el_assign_unsigned_char(HElementData data, unsigned char d);
    int cc_el_assign_signed_short(HElementData data, signed short d);
    int cc_el_assign_unsigned_short(HElementData data, unsigned short d);
    int cc_el_assign_signed_int(HElementData data, signed int d);
    int cc_el_assign_unsigned_int(HElementData data, unsigned int d);
    int cc_el_assign_signed_long(HElementData data, signed long d);
    int cc_el_assign_unsigned_long(HElementData data, unsigned long d);
    int cc_el_assign_signed_long_long(HElementData data, signed long long d);
    int cc_el_assign_unsigned_long_long(HElementData data, unsigned long long d);
    int cc_el_assign_float(HElementData data, float d);
    int cc_el_assign_double(HElementData data, double d);
    int cc_el_assign_voidp(HElementData data, void *p);
    int cc_el_assign_string(HElementData, HString p);
    int cc_el_assign_cstring(HElementData data, const char *c);
    int cc_el_assign_cstring_n(HElementData data, const char *c, size_t len);
    int cc_el_assign_vector(HElementData, HVector p);
    int cc_el_assign_linked_list(HElementData data, HLinkedList d);
    int cc_el_assign_doubly_linked_list(HElementData data, HDoublyLinkedList d);
    int cc_el_assign_hash_table(HElementData data, HHashTable d);

    /* Functions to extract data from the specified element
     */
    char *cc_el_get_char(HElementData data);
    signed char *cc_el_get_signed_char(HElementData data);
    unsigned char *cc_el_get_unsigned_char(HElementData data);
    signed short *cc_el_get_signed_short(HElementData data);
    unsigned short *cc_el_get_unsigned_short(HElementData data);
    signed int *cc_el_get_signed_int(HElementData data);
    unsigned int *cc_el_get_unsigned_int(HElementData data);
    signed long *cc_el_get_signed_long(HElementData data);
    unsigned long *cc_el_get_unsigned_long(HElementData data);
    signed long long *cc_el_get_signed_long_long(HElementData data);
    unsigned long long *cc_el_get_unsigned_long_long(HElementData data);
    float *cc_el_get_float(HElementData data);
    double *cc_el_get_double(HElementData data);
    const char **cc_el_get_cstring(HElementData data);
    void **cc_el_get_voidp(HElementData data);
    HString *cc_el_get_string(HElementData data);
    HVector *cc_el_get_vector(HElementData data);
    HLinkedList *cc_el_get_linked_list(HElementData data);
    HDoublyLinkedList *cc_el_get_doubly_linked_list(HElementData data);
    HHashTable *cc_el_get_hash_table(HElementData);

    /** @brief Pretty-prints an element or container
     *
     * @param out Where to print the data to.
     * @param data The element to print. Either containers or primitive types are accepted.
     * @param flags One or more of the following:
     *
     *     0x01 - Print type information along with the value itself
     */
    void cc_el_pretty_print(FILE *out, HElementData data, unsigned flags);

    /* Set the metadata block for the specified element
     *
     * Returns CC_TYPE_MISMATCH if the metadata type does not match the current element type
     * Returns CC_OK on success
     *
     * The element does NOT take ownership of the metadata
     */
    int cc_el_set_metadata(HElementData data, HContainerElementMetaData meta);

    /* Get the metadata block for the specified element
     *
     * Returns NULL if no metadata block is specified.
     *
     * The block must not be killed
     */
    HContainerElementMetaData cc_el_get_metadata(HConstElementData data);

    /* Returns the typename associated with the container type
     */
    const char *cc_el_typename(ContainerElementType type);

    /* Returns the default construction callback for the specified type
     */
    ElementDataCallback cc_el_constructor(ContainerElementType type);
    /* Returns the default copy-construction callback for the specified type
     */
    ElementDualDataCallback cc_el_copy_constructor(ContainerElementType type);
    /* Returns the default destruction callback for the specified type
     */
    ElementDataCallback cc_el_destructor(ContainerElementType type);
    /* Returns the default comparison callback for the specified type
     */
    ElementDualDataCallback cc_el_compare(ContainerElementType type);

    /* Returns the userdata specified in the metadata block
     */
    void *cc_el_userdata_in(HContainerElementMetaData metadata);
    /* Returns the specified construction callback for the metadata block
     */
    ElementDataCallback cc_el_constructor_in(HContainerElementMetaData metadata);
    /* Returns the specified copy-construction callback for the metadata block
     */
    ElementDualDataCallback cc_el_copy_constructor_in(HContainerElementMetaData metadata);
    /* Returns the specified destruction callback for the metadata block
     */
    ElementDataCallback cc_el_destructor_in(HContainerElementMetaData metadata);
    /* Returns the specified comparison callback for the metadata block
     */
    ElementDualDataCallback cc_el_compare_in(HContainerElementMetaData metadata);

    /* Sets the userdata in the metadata block
     */
    void cc_el_set_userdata_in(HContainerElementMetaData metadata, void *userdata);
    /* Sets the specified construction callback for the metadata block
     */
    void cc_el_set_constructor_in(HContainerElementMetaData metadata, ElementDataCallback constructor);
    /* Sets the specified copy construction callback for the metadata block
     */
    void cc_el_set_copy_constructor_in(HContainerElementMetaData metadata, ElementDualDataCallback copy_constructor);
    /* Sets the specified destruction callback for the metadata block
     */
    void cc_el_set_destructor_in(HContainerElementMetaData metadata, ElementDataCallback destructor);
    /* Sets the specified comparison callback for the metadata block
     */
    void cc_el_set_compare_in(HContainerElementMetaData metadata, ElementDualDataCallback compare);

    /* Call the specified function in the metadata block for the given data, if it exists, otherwise return 0
     */
    int cc_el_call_constructor_in(HContainerElementMetaData metadata, HElementData data);
    int cc_el_call_copy_constructor_in(HContainerElementMetaData metadata, HElementData dest, HElementData src);
    int cc_el_call_destructor_in(HContainerElementMetaData metadata, HElementData data);
    int cc_el_call_compare_in(HContainerElementMetaData metadata, HElementData lhs, HElementData rhs);

    /** @brief Obtains the container-repair function for the specified type
     *
     * @param type The type to get the container-repair function for.
     * @return The container-repair function, or `NULL` if no such function exists.
     */
    typedef void (*ContainerRepairCallback)(void *container);
    ContainerRepairCallback cc_el_container_repair_for_type(ContainerElementType type);

    /* Returns the size of the data type specified
     */
    size_t cc_el_size_type(ContainerElementType type);

    /* Returns the size of a metadata block */
    size_t cc_el_metadata_sizeof();

    /* Makes the default metadata block for the specified type
     *
     * Returns NULL if allocation failed
     */
    HContainerElementMetaData cc_el_make_metadata(ContainerElementType type);

    /* Makes the default metadata block for the specified type at the specified location
     *
     * Returns error code on failure, or CC_OK on success
     */
    int cc_el_make_metadata_at(void *buf, size_t buffer_size, ContainerElementType type);

    /* Copies the metadata block to another metadata block
     */
    void cc_el_copy_metadata(HContainerElementMetaData dest, const HContainerElementMetaData src);

    /* Destroys the metadata block at the specified location without freeing it
     */
    void cc_el_kill_metadata_at(HContainerElementMetaData metadata);

    /* Destroys the metadata block
     */
    void cc_el_kill_metadata(HContainerElementMetaData metadata);

    /* Returns the type specified in the element block
     *
     * No NULL checks are performed
     */
    ContainerElementType cc_el_type(HConstElementData data);

    /* Returns the metadata block specifying the contained keys' type information, if the current type is a container.
     *
     * This function returns NULL if the current type is not a key/value container
     *
     * The metadata block must not be killed.
     *
     * No NULL checks are performed
     */
    HContainerElementMetaData cc_el_contained_key_metadata(HElementData data);

    /* Returns the metadata block specifying the contained values' type information, if the current type is a container.
     *
     * This function returns NULL if the current type is not a container
     *
     * The metadata block must not be killed.
     *
     * No NULL checks are performed
     */
    HContainerElementMetaData cc_el_contained_value_metadata(HElementData data);

    /* Returns the type specified in the metadata block
     *
     * No NULL checks are performed
     */
    ContainerElementType cc_el_metadata_type(const HContainerElementMetaData metadata);

    /* Returns the size of the type specified in the metadata block
     *
     * No NULL checks are performed
     */
    size_t cc_el_metadata_type_size(const HContainerElementMetaData metadata);

    /* Returns non-zero if both metadata blocks have compatible (equal) types
     *
     * No NULL checks are performed
     */
    int cc_el_compatible_metadata(const HContainerElementMetaData lhs,
                                  const HContainerElementMetaData rhs);

    /* Returns non-zero if the element matches the metadata block's type
     *
     * No NULL checks are performed
     */
    int cc_el_compatible_metadata_element(const HContainerElementMetaData lhs,
                                          HConstElementData data);

    typedef unsigned (*ElementDataHashFunction)(HConstElementData);

    /* Retrieves hash of data in specified output location "hash"
     *
     * The data is not modified in any way.
     *
     * No NULL checks are performed
     *
     * Returns CC_BAD_PARAM if unsupported datatype
     * Returns CC_OK on success
     */
    int cc_el_hash_default(HConstElementData element, unsigned *hash);

#ifdef __cplusplus
}
#endif

#endif /* CCELEMENT_H */
