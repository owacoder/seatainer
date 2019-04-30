#ifndef CCDBLLST_H
#define CCDBLLST_H

#include "element.h"

/*
 * Not the most efficient linked list implementation, but does provide a measure of run-time type safety.
 *
 * It allocates the list, the nodes, and the elements in the nodes (so two allocations per element, even for trivial types)
 *
 */

#ifdef __cplusplus
extern "C" {
#endif
    /* Returns the size of one linked list */
    size_t cc_dll_sizeof();

    /* Returns maximum capacity of one linked list */
    size_t cc_dll_max_capacity();

    /* Initializes a new linked list with specified type
     *
     * Returns NULL if allocation failed
     */
    HDoublyLinkedList cc_dll_init(ContainerElementType type);

    /* Initializes a new linked list at the specified buffer
     * Returns CC_BAD_PARAM if the buffer is not big enough, or a constructor response error code
     * Returns CC_OK if all went well
     */
    int cc_dll_init_at(void *buf, size_t buffer_size, ContainerElementType type);

    /* Returns a copy of the linked list
     *
     * The provided callback `construct` is used to construct the new elements
     * The provided callback `destruct` is used to destruct the elements on failure to allocate, if necessary
     *
     * `list` is never modified
     *
     * Returns NULL if allocation failed
     */
    HDoublyLinkedList cc_dll_copy(HDoublyLinkedList list, ElementDataCallback construct, ElementDataCallback destruct);

    /* Swaps two linked lists
     *
     * Guaranteed not to fail
     */
    void cc_dll_swap(HDoublyLinkedList lhs, HDoublyLinkedList rhs);

    /* Returns the beginning element of the linked list
     *
     * Note that this operation is performed in O(1) time
     *
     */
    Iterator cc_dll_begin(HDoublyLinkedList list);

    /* Returns the beginning reversed element of the linked list
     *
     * Note that this operation is performed in O(1) time
     *
     */
    Iterator cc_dll_rbegin(HDoublyLinkedList list);

    /* Allocates a new element (without ownership taken) inserted after the linked list node
     *
     * If `after` is NULL, the new element is inserted at the beginning of the list
     *
     * Note that this operation is performed in O(1) time
     *
     * supported in flags:
     *   - Move-semantics: CC_MOVE_VALUE or CC_COPY_VALUE
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
    int cc_dll_insert_after(HDoublyLinkedList list, unsigned flags, Iterator after, HConstElementData data, ElementDataCallback construct);

    /* Destroys the element `element` in the linked list
     *
     * Note that this operation is performed in O(1) time
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
    int cc_dll_erase(HDoublyLinkedList list, Iterator element, ElementDataCallback destruct);

    /* Allocates a new element (without ownership taken) prepended to the linked list node
     *
     * Note that this operation is performed in O(1) time
     *
     * supported in flags:
     *   - Move-semantics: CC_MOVE_VALUE or CC_COPY_VALUE
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
    INLINE int cc_dll_push_front(HDoublyLinkedList list, unsigned flags, HConstElementData data, ElementDataCallback construct) INLINE_DEFINITION({
        return cc_dll_insert_after(list, flags, NULL, data, construct);
    })
    INLINE int cc_dll_pop_front(HDoublyLinkedList list, ElementDataCallback destruct) INLINE_DEFINITION({
        return cc_dll_erase(list, cc_dll_begin(list), destruct);
    })

    /* Allocates a new element (without ownership taken) prepended to the linked list node
     *
     * Note that this operation is performed in O(1) time
     *
     * supported in flags:
     *   - Move-semantics: CC_MOVE_VALUE or CC_COPY_VALUE
     * The provided callback is used to construct the new element
     *
     * Returns 0 on success
     * Returns non-zero on failure to allocate
     *
     * `*list` will be updated on success
     * `*list` will be unchanged on failure
     *
     */
    INLINE int cc_dll_push_back(HDoublyLinkedList list, unsigned flags, HConstElementData data, ElementDataCallback construct) INLINE_DEFINITION({
        return cc_dll_insert_after(list, flags, cc_dll_rbegin(list), data, construct);
    })
    INLINE int cc_dll_pop_back(HDoublyLinkedList list, ElementDataCallback destruct) INLINE_DEFINITION({
        return cc_dll_erase(list, cc_dll_rbegin(list), destruct);
    })

    /* Searches the list for the specified element, compared using the provided comparator if possible
     *
     * Calls the provided callback (with specified userdata) for each element
     *
     * supported in `flags`:
     *   - direction: either CC_FORWARD or CC_BACKWARD
     *   - organization: CC_ORGANIZE_xxx
     *
     * The parameter `out` will be set if this function returns CC_OK, but may be NULL, indicating the provided element was not found
     *
     * Returns CC_OK on success
     */
    int cc_dll_find(HDoublyLinkedList list, Iterator start, unsigned flags, HConstElementData data, ElementDualDataCallback compare, Iterator *out);

    /* Iterates through the entire linked list
     *
     * Calls the provided callback (with specified userdata) for each element
     *
     * supported in `flags`:
     *   - direction: either CC_FORWARD or CC_BACKWARD
     *
     * Returns CC_OK on success
     * Returns CC_BAD_PARAM if `callback` is NULL
     */
    int cc_dll_iterate(HDoublyLinkedList list, unsigned flags, ExtendedElementDataCallback callback, void *userdata);

    /* Reverses the order of elements in the linked list
     *
     * This function never fails
     */
    void cc_dll_reverse(HDoublyLinkedList list);

    /* Returns the size of the linked list
     *
     * Note that this operation is performed in O(1) time
     *
     */
    size_t cc_dll_size(HDoublyLinkedList list);

    /* Returns the metadata (type and callback information) of the linked list
     *
     * Note that this operation is performed in O(1) time
     *
     */
    HContainerElementMetaData cc_dll_metadata(HDoublyLinkedList list);

    /* Returns the next element in the linked list, or NULL if no such element exists
     *
     * Note that this operation is performed in O(1) time
     *
     */
    Iterator cc_dll_next(HDoublyLinkedList list, Iterator node);

    /* Returns the next reversed element in the linked list, or NULL if no such element exists
     *
     * Note that this operation is performed in O(1) time
     *
     */
    Iterator cc_dll_rnext(HDoublyLinkedList list, Iterator node);

    /* Returns the value of the specified element in `out`
     *
     * CC_OK is returned
     *
     * Note that this operation is performed in O(1) time
     *
     */
    int cc_dll_node_data(HDoublyLinkedList list, Iterator node, HElementData out);

    /* Compares two linked lists, and returns the lexicographical comparison of both
     *
     * Uses the provided comparison function `cmp` for each pair of elements
     * (expected to return -1, 0, or 1 if less-than, equal, or greater-than, respectively)
     * Uses memcmp() if no comparison function is provided
     *
     * Note that this operation is performed in O(n) time
     *
     * Returns one of: -1, 0, 1, CC_BAD_PARAM, or CC_TYPE_MISMATCH
     *
     * Returns CC_TYPE_MISMATCH if lists don't contain compatible types
     * Returns CC_BAD_PARAM if `cmp` is NULL
     *
     */
    int cc_dll_compare(HDoublyLinkedList lhs, HDoublyLinkedList rhs, ElementDualDataCallback cmp);

    /* Destroys the linked list at the specified location
     *
     * The provided callback is used to destroy all elements
     *
     * The handle is invalidated after this function is called, but NOT freed!
     *
     * Note that this operation is performed in O(n) time
     */
    void cc_dll_destroy_at(HDoublyLinkedList list, ElementDataCallback destruct);

    /* Destroys the linked list
     *
     * The provided callback is used to destroy all elements
     *
     * The handle is invalidated after this function is called
     *
     * Note that this operation is performed in O(n) time
     */
    void cc_dll_destroy(HDoublyLinkedList list, ElementDataCallback destruct);
#ifdef __cplusplus
}
#endif

#endif /* CCDBLLST_H */
