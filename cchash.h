#ifndef CCHASH_H
#define CCHASH_H

#include "element.h"

#ifdef __cplusplus
extern "C" {
#endif
    /* Initializes a new hash table with specified key and value types
     *
     * Returns NULL if allocation failed
     */
    HHashTable cc_ht_init(ContainerElementType keyType, ContainerElementType valueType);

    /* Returns a copy of the hash table
     *
     * The provided callback `construct` is used to construct the new elements
     * The provided callback `destruct` is used to destruct the elements on failure to allocate, if necessary
     *
     * `table` is never modified
     *
     * Returns NULL if allocation failed
     */
    HHashTable cc_ht_copy(HHashTable table, ElementDataCallback construct, ElementDataCallback destruct);

    /* Swaps two hash tables
     *
     * Guaranteed not to fail
     */
    void cc_ht_swap(HHashTable lhs, HHashTable rhs);

    /* Returns the size of the hash table (how many elements are currently in it)
     *
     * Note that this operation is performed in O(1) time
     *
     */
    size_t cc_ht_size_of(HHashTable table);

    /* Returns the capacity of the hash table
     *
     * Note that this operation is performed in O(1) time
     *
     */
    size_t cc_ht_capacity_of(HHashTable table);

    /* Allocates a new element inserted to the table
     *
     * Note that this operation is performed in <= O(n) time
     *
     * The provided callback is used to construct the new element
     *
     * Returns CC_OK on success
     * Returns CC_NO_MEM on failure to allocate
     * Returns CC_TYPE_MISMATCH if there is a type mismatch
     *
     * `*table` will be updated on success
     * `*table` will be unchanged on failure
     */
    int cc_ht_insert(HHashTable table, HConstElementData data, ElementDataCallback construct);

    /* Destroys the specified element in the hash table
     *
     * Note that this operation is performed in <= O(n) time
     *
     * The provided callback is used to destruct the new element
     *
     * Returns CC_OK on success
     * Returns CC_TYPE_MISMATCH if there is a type mismatch
     *
     * `*table` will be updated on success
     * `*table` will be unchanged on failure
     *
     */
    int cc_ht_erase(HHashTable table, HConstElementData data, ElementDataCallback destruct);

    /* Returns an iterator to the beginning of the hash table, or NULL if the hash table is empty
     */
    Iterator cc_ht_begin(HHashTable table);

    /* Returns the iterator after `node`, or NULL if `node` is the last element
     */
    Iterator cc_ht_next(HHashTable table, Iterator node);

    /* Searches the hash table for the specified element, compared using the provided comparator if possible
     *
     * Calls the provided callback (with specified userdata) for each element
     *
     * `direction`: either CC_FORWARD or CC_BACKWARD
     *
     * The parameter `out` will be set if this function returns CC_OK, but may be NULL, indicating the provided element was not found
     *
     * Returns CC_OK on success
     */
    int cc_ht_find(HHashTable table, Iterator start, unsigned flags, HConstElementData data, ElementDualDataCallback compare, Iterator *out);

    /* Iterates through the entire hash table
     *
     * Calls the provided callback (with specified userdata) for each element
     *
     * Returns CC_OK on success
     * Returns CC_BAD_PARAM if `callback` is NULL
     */
    int cc_ht_iterate(HHashTable table, ExtendedElementDualDataCallback callback, void *userdata);

    /* Returns a reference to the value of the specified element in `out`
     *
     * If the element position is out of bounds, CC_BAD_PARAM is returned, otherwise, CC_OK is returned
     *
     * Although technically possible, keys retrieved from this function MUST NOT BE MODIFIED!
     *
     * Note that this operation is performed in O(1) time
     *
     */
    int cc_ht_node_key(HHashTable table, Iterator element, HElementData out);

    /* Returns a reference to the value of the specified element in `out`
     *
     * If the element position is out of bounds, CC_BAD_PARAM is returned, otherwise, CC_OK is returned
     *
     * Note that this operation is performed in O(1) time
     *
     */
    int cc_ht_node_data(HHashTable table, Iterator element, HElementData out);

    /* Returns the metadata (type and callback information) of the hash table keys
     *
     * Note that this operation is performed in O(1) time
     *
     */
    HContainerElementMetaData cc_ht_key_metadata(HHashTable table);

    /* Returns the metadata (type and callback information) of the hash table values
     *
     * Note that this operation is performed in O(1) time
     *
     */
    HContainerElementMetaData cc_ht_value_metadata(HHashTable table);

    /* Compares two hash tables, and returns whether they are equal or not
     *
     * Uses the provided comparison function `cmp` for each pair of elements
     * (expected to return -1, 0, or 1 if less-than, equal, or greater-than, respectively)
     * Uses memcmp() if no comparison function is provided
     *
     * Note that this operation is performed in O(n) time
     *
     * Returns one of: 0 (equal), 1 (not equal), or CC_TYPE_MISMATCH
     *
     * Returns CC_TYPE_MISMATCH if tables don't contain compatible types
     *
     */
    int cc_ht_compare(HHashTable lhs, HHashTable rhs, ElementDualDataCallback cmp);

    /* Clears the hash table
     *
     * The provided callback is used to destroy all elements
     *
     * The size is reset to 0 after this function is called
     *
     * Note that this operation is performed in O(n) time
     */
    void cc_ht_clear(HHashTable table, ElementDataCallback destruct);

    /* Destroys the hash table
     *
     * The provided callback is used to destroy all elements
     *
     * The handle is invalidated after this function is called
     *
     * Note that this operation is performed in O(n) time
     */
    void cc_ht_destroy(HHashTable table, ElementDataCallback destruct);
#ifdef __cplusplus
}
#endif

#endif // CCHASH_H
