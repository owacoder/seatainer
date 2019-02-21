#ifndef CCLNKLST_H
#define CCLNKLST_H

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
    struct LinkedList;

    /* The type to be used for referring to a linked list
     *
     * This type should be used for linked list operations
     */
    typedef struct LinkedList *HLinkedList;

    /* Initializes a new linked list with specified type
     *
     * Returns NULL if allocation failed
     */
    HLinkedList cc_ll_init(ContainerElementType type);

    /* Returns a copy of the linked list
     *
     * The provided callback `construct` is used to construct the new elements
     * The provided callback `destruct` is used to destruct the elements on failure to allocate, if necessary
     *
     * `list` is never modified
     *
     * Returns NULL if allocation failed
     */
    HLinkedList cc_ll_copy(HLinkedList list, ElementDataCallback construct, ElementDataCallback destruct);

    /* Swaps two linked lists
     *
     * Guaranteed not to fail
     */
    void cc_ll_swap(HLinkedList lhs, HLinkedList rhs);

    /* Returns the beginning element of the linked list
     *
     * Note that this operation is performed in O(1) time
     *
     */
    Iterator cc_ll_begin(HLinkedList list);

    /* Returns the end element of the linked list
     *
     * Note that this operation is performed in O(1) time
     *
     */
    Iterator cc_ll_rbegin(HLinkedList list);

    /* Allocates a new element (without ownership taken) inserted after the linked list node
     *
     * If `after` is NULL, the new element is inserted at the beginning of the list
     *
     * Note that this operation is performed in O(1) time
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
    int cc_ll_insert_after(HLinkedList list, Iterator after, HConstElementData data, ElementDataCallback construct);

    /* Destroys the element after `after` in the linked list
     *
     * If `after` is NULL, the element at the beginning of the list is removed
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
    int cc_ll_erase_after(HLinkedList list, Iterator after, ElementDataCallback destruct);

    /* Allocates a new element (without ownership taken) prepended to the linked list node
     *
     * Note that this operation is performed in O(1) time
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
    INLINE int cc_ll_push_front(HLinkedList list, HConstElementData data, ElementDataCallback construct) INLINE_DEFINITION({
        return cc_ll_insert_after(list, NULL, data, construct);
    })
    INLINE int cc_ll_pop_front(HLinkedList list, ElementDataCallback destruct) INLINE_DEFINITION({
        return cc_ll_erase_after(list, NULL, destruct);
    })

    /* Allocates a new element (without ownership taken) appended to the linked list node
     *
     * Note that this operation is performed in O(1) time
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
    INLINE int cc_ll_push_back(HLinkedList list, HConstElementData data, ElementDataCallback construct) INLINE_DEFINITION({
        return cc_ll_insert_after(list, cc_ll_rbegin(list), data, construct);
    })

    /* Searches the list for the specified element, compared using the provided comparator if possible
     *
     * Calls the provided callback (with specified userdata) for each element
     *
     * supported in `flags`:
     *   - organization: CC_ORGANIZE_NONE, CC_ORGANIZE_MTF
     *
     * The parameter `out` will be set if this function returns CC_OK, but may be NULL, indicating the provided element was not found
     *
     * Returns CC_OK on success
     */
    int cc_ll_find(HLinkedList list, Iterator start, unsigned flags, HConstElementData data, ElementDualDataCallback compare, Iterator *out);

    /* Forward iterates through the entire linked list
     *
     * Calls the provided callback (with specified userdata) for each element
     *
     * Returns CC_OK on success
     * Returns CC_BAD_PARAM if `callback` is NULL
     */
    int cc_ll_iterate(HLinkedList list, ExtendedElementDataCallback callback, void *userdata);

    /* Returns the size of the linked list
     *
     * Note that this operation is performed in O(1) time
     *
     */
    size_t cc_ll_size_of(HLinkedList list);

    /* Returns the metadata (type and callback information) of the linked list
     *
     * Note that this operation is performed in O(1) time
     *
     */
    HContainerElementMetaData cc_ll_metadata(HLinkedList list);

    /* Returns the next element in the linked list, or NULL if no such element exists
     *
     * Note that this operation is performed in O(1) time
     *
     */
    Iterator cc_ll_next(HLinkedList list, Iterator node);

    /* Returns the value of the specified element in `out`
     *
     * CC_OK is returned
     *
     * Note that this operation is performed in O(1) time
     *
     */
    int cc_ll_node_data(HLinkedList list, Iterator node, HElementData out);

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
    int cc_ll_compare(HLinkedList lhs, HLinkedList rhs, ElementDualDataCallback cmp);

    /* Destroys the linked list
     *
     * The provided callback is used to destroy all elements
     *
     * The handle is invalidated after this function is called
     *
     * Note that this operation is performed in O(n) time
     */
    void cc_ll_destroy(HLinkedList list, ElementDataCallback destruct);
#ifdef __cplusplus
}
#endif

#endif /* CCLNKLST_H */
