#ifndef CCVECTOR_H
#define CCVECTOR_H

#include "element.h"

#ifdef __cplusplus
extern "C" {
#endif
    /* Initializes a new vector with specified type
     *
     * Returns NULL if allocation failed
     */
    HVector cc_v_init(ContainerElementType type);

    /* Grows the capacity of the vector to at least `size` elements
     *
     * Returns NULL if allocation failed, but `list` will still be valid.
     */
    HVector cc_v_grow(HVector list, size_t size);

    /* Returns a copy of the vector
     *
     * The provided callback `construct` is used to construct the new elements
     * The provided callback `destruct` is used to destruct the elements on failure to allocate, if necessary
     *
     * `list` is never modified
     *
     * Returns NULL if allocation failed
     */
    HVector cc_v_copy(HVector list, ElementDataCallback construct, ElementDataCallback destruct);

    /* Swaps two vectors
     *
     * Guaranteed not to fail
     */
    void cc_v_swap(HVector lhs, HVector rhs);

    /* Returns the size of the vector
     *
     * Note that this operation is performed in O(1) time
     *
     */
    size_t cc_v_size_of(HVector list);

    /* Returns the capacity of the vector
     *
     * Note that this operation is performed in O(1) time
     *
     */
    size_t cc_v_capacity_of(HVector list);

    /* Allocates a new element inserted before the specified vector index
     *
     * If `before` is 0, the new element is inserted at the beginning of the list
     *
     * Note that this operation is performed in <= O(n) time
     *
     * The provided callback is used to construct the new element
     *
     * Returns CC_OK on success
     * Returns CC_NO_MEM on failure to allocate
     * Returns CC_TYPE_MISMATCH if there is a type mismatch
     *
     * `*list` will be updated on success
     * `*list` will be unchanged on failure
     *
     */
    int cc_v_insert(HVector list, size_t before, HConstElementData data, ElementDataCallback construct);

    /* Destroys the specified element in the vector
     *
     * Note that this operation is performed in <= O(n) time
     *
     * The provided callback is used to destruct the element
     *
     * Returns CC_OK on success
     * Returns CC_TYPE_MISMATCH if there is a type mismatch
     *
     * `*list` will be updated on success
     * `*list` will be unchanged on failure
     *
     */
    int cc_v_erase(HVector list, size_t element, ElementDataCallback destruct);

    /* Allocates a new element appended to the vector
     *
     * Note that this operation is performed in amortized O(1) time
     *
     * The provided callback is used to construct the new element
     *
     * Returns 0 on success
     * Returns non-zero on failure to allocate
     *
     * `*list` will be updated on success
     * `*list` will be unchanged on failure
     *
     */
    INLINE int cc_v_push_back(HVector list, HConstElementData data, ElementDataCallback construct) INLINE_DEFINITION({
        return cc_v_insert(list, cc_v_size_of(list), data, construct);
    })

    INLINE int cc_v_pop_back(HVector list, ElementDataCallback destruct) INLINE_DEFINITION({
        return cc_v_erase(list, cc_v_size_of(list) - 1, destruct);
    })

    /* Returns the raw contiguous data pointer
     *
     * WARNING! This function returns a pointer to the raw contiguous memory. Any modification is at your own risk!
     */
    void *cc_v_raw(HVector list);

    /* Returns an iterator to the beginning of the vector, or NULL if the vector is empty
     */
    Iterator cc_v_begin(HVector list);

    /* Returns the iterator after `node`, or NULL if `node` is the last element
     */
    Iterator cc_v_next(HVector list, Iterator node);

    /* Returns a reverse iterator to the reversed beginning of the vector, or NULL if the vector is empty
     *
     * Similar to the C++ construct of the same name
     */
    Iterator cc_v_rbegin(HVector list);

    /* Returns the reverse iterator after `node`, or NULL if `node` is the last element
     */
    Iterator cc_v_rnext(HVector list, Iterator node);

    /* Searches the vector for the specified element, compared using the provided comparator if possible
     *
     * Calls the provided callback (with specified userdata) for each element
     *
     * supported in `flags`:
     *   - direction: either CC_FORWARD or CC_BACKWARD
     *
     * The parameter `out` will be set if this function returns CC_OK, but may be NULL, indicating the provided element was not found
     *
     * Returns CC_OK on success
     */
    int cc_v_find(HVector list, Iterator start, unsigned flags, HConstElementData data, ElementDualDataCallback compare, Iterator *out);

    /* Iterates through the entire vector
     *
     * Calls the provided callback (with specified userdata) for each element
     *
     * supported in `flags`:
     *   - direction: either CC_FORWARD or CC_BACKWARD
     *
     * Returns CC_OK on success
     * Returns CC_BAD_PARAM if `callback` is NULL
     */
    int cc_v_iterate(HVector list, unsigned flags, ExtendedElementDataCallback callback, void *userdata);

    /* Returns a reference to the value of the specified element in `out`
     *
     * If the element position is out of bounds, CC_BAD_PARAM is returned, otherwise, CC_OK is returned
     *
     * Note that this operation is performed in O(1) time
     *
     */
    int cc_v_node_data(HVector list, Iterator element, HElementData out);

    /* Returns the metadata (type and callback information) of the vector
     *
     * Note that this operation is performed in O(1) time
     *
     */
    HContainerElementMetaData cc_v_metadata(HVector list);

    /* Compares two vectors, and returns the lexicographical comparison of both
     *
     * Uses the provided comparison function `cmp` for each pair of elements
     * (expected to return -1, 0, or 1 if less-than, equal, or greater-than, respectively)
     * Uses memcmp() if no comparison function is provided
     *
     * Note that this operation is performed in O(n) time
     *
     * Returns one of: -1, 0, 1, or CC_TYPE_MISMATCH
     *
     * Returns CC_TYPE_MISMATCH if lists don't contain compatible types
     *
     */
    int cc_v_compare(HVector lhs, HVector rhs, ElementDualDataCallback cmp);

    /* Sorts the data in a vector using the provided comparison callback
     *
     * Uses memcmp() if no comparison function is provided
     * The order of equivalent elements is unspecified (i.e. the sort is not stable)
     *
     * Note that this operation is performed in O(n log n) time
     *
     * Returns CC_OK on success
     */
    int cc_v_sort(HVector list, ElementDualDataCallback cmp);

    /* Vector to string
     *
     * Returns NULL if vector is not of type Char, SignedChar, or UnsignedChar
     */
    const char *cc_v_to_string(HVector list);

    /* Assign string to vector
     *
     * Vector must have type Char, SignedChar, or UnsignedChar
     *
     * Returns CC_OK on success
     * Returns CC_TYPE_MISMATCH if there is a type mismatch
     */
    int cc_v_assign_string_n(HVector list, const char *data, size_t len);
    int cc_v_assign_string(HVector list, const char *data);

    /* Clears the vector
     *
     * The provided callback is used to destroy all elements
     *
     * The size is reset to 0 after this function is called
     *
     * Note that this operation is performed in O(n) time
     */
    void cc_v_clear(HVector list, ElementDataCallback destruct);

    /* Destroys the vector
     *
     * The provided callback is used to destroy all elements
     *
     * The handle is invalidated after this function is called
     *
     * Note that this operation is performed in O(n) time
     */
    void cc_v_destroy(HVector list, ElementDataCallback destruct);
#ifdef __cplusplus
}
#endif

#endif /* CCVECTOR_H */
