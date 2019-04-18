#ifndef CCSTRING_H
#define CCSTRING_H

#include "element.h"

#ifdef __cplusplus
extern "C" {
#endif
    /* Returns the size of one linked list */
    size_t cc_s_sizeof();

    /* Initializes a new string
     *
     * Returns NULL if allocation failed
     */
    HString cc_s_init();

    /* Initializes a new element at the specified buffer
     * Returns CC_BAD_PARAM if the buffer is not big enough
     * Returns CC_OK if all went well
     */
    int cc_s_init_at(void *buf, size_t buffer_size);

    /* Grows the capacity of the string to at least `size` elements
     *
     * Returns NULL if allocation failed, but `string` will still be valid.
     */
    HString cc_s_grow(HString string, size_t size);

    /* Returns a copy of the string
     *
     * `string` is never modified
     *
     * Returns NULL if allocation failed
     */
    HString cc_s_copy(HString string);

    /* Swaps two strings
     *
     * Guaranteed not to fail
     */
    void cc_s_swap(HString lhs, HString rhs);

    /* Returns the size of the string
     *
     * Note that this operation is performed in O(1) time
     *
     */
    size_t cc_s_size(HString string);

    /* Returns the capacity of the string
     *
     * Note that this operation is performed in O(1) time
     *
     */
    size_t cc_s_capacity(HString string);

    /* Allocates a new element inserted before the specified string index
     *
     * If `before` is 0, the new element is inserted at the beginning of the string
     *
     * Note that this operation is performed in <= O(n) time
     *
     * Returns CC_OK on success
     * Returns CC_NO_MEM on failure to allocate
     *
     * `*string` will be updated on success
     * `*string` will be unchanged on failure
     *
     */
    int cc_s_insert(HString string, size_t before, char data);
    int cc_s_insert_n(HString string, size_t before, char data, size_t count);
    int cc_s_insert_cstring(HString string, size_t before, const char *data);
    int cc_s_insert_cstring_n(HString string, size_t before, const char *data, size_t count);

    /* Destroys the specified element in the string
     *
     * Note that this operation is performed in <= O(n) time
     *
     * Returns CC_OK on success
     * Returns CC_TYPE_MISMATCH if there is a type mismatch
     *
     * `*string` will be updated on success
     * `*string` will be unchanged on failure
     *
     */
    int cc_s_erase(HString string, size_t element);
    int cc_s_erase_n(HString string, size_t element, size_t count);

    /* Allocates a new element appended to the string
     *
     * Note that this operation is performed in amortized O(1) time
     *
     * The provided callback is used to construct the new element
     *
     * Returns 0 on success
     * Returns non-zero on failure to allocate
     *
     * `*string` will be updated on success
     * `*string` will be unchanged on failure
     *
     */
    INLINE int cc_s_push_back(HString string, char data) INLINE_DEFINITION({
        return cc_s_insert(string, cc_s_size(string), data);
    })
    INLINE int cc_s_push_back_n(HString string, char data, size_t count) INLINE_DEFINITION({
        return cc_s_insert_n(string, cc_s_size(string), data, count);
    })
    INLINE int cc_s_push_back_cstring(HString string, const char *data) INLINE_DEFINITION({
        return cc_s_insert_cstring(string, cc_s_size(string), data);
    })
    INLINE int cc_s_push_back_cstring_n(HString string, const char *data, size_t count) INLINE_DEFINITION({
        return cc_s_insert_cstring_n(string, cc_s_size(string), data, count);
    })

    INLINE int cc_s_pop_back(HString string) INLINE_DEFINITION({
        return cc_s_erase(string, cc_s_size(string) - 1);
    })
    INLINE int cc_s_pop_back_n(HString string, size_t count) INLINE_DEFINITION({
        return cc_s_erase_n(string, cc_s_size(string) - count, count);
    })

    /* Returns the raw contiguous data pointer
     *
     * The returned string is NOT guaranteed to be NUL-terminated
     *
     * WARNING! This function returns a pointer to the raw contiguous memory. Any modification is at your own risk!
     */
    char *cc_s_raw(HString string);

    /* Returns an iterator to the beginning of the string, or NULL if the string is empty
     */
    Iterator cc_s_begin(HString string);

    /* Returns the iterator after `node`, or NULL if `node` is the last element
     */
    Iterator cc_s_next(HString string, Iterator node);

    /* Returns a reverse iterator to the reversed beginning of the string, or NULL if the string is empty
     *
     * Similar to the C++ construct of the same name
     */
    Iterator cc_s_rbegin(HString string);

    /* Returns the reverse iterator after `node`, or NULL if `node` is the last element
     */
    Iterator cc_s_rnext(HString string, Iterator node);

    /* Searches the string for the specified element, compared using the provided comparator if possible
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
    int cc_s_find(HString string, Iterator start, unsigned flags, HConstElementData data, StringCompareCallback compare, Iterator *out);

    /* Reverses all the elements in the string
     *
     * This function never fails
     */
    void cc_s_reverse(HString string);

    /* Returns a reference to the value of the specified element in `out`
     *
     * CC_OK is returned
     *
     * Note that this operation is performed in O(1) time
     *
     */
    int cc_s_node_data(HString string, Iterator element, HElementData out);

    /* Returns the metadata (type and callback information) of the string
     *
     * Note that this operation is performed in O(1) time
     *
     */
    HContainerElementMetaData cc_s_metadata(HString string);

    /* Compares two strings, and returns the lexicographical comparison of both
     *
     * Uses the provided comparison function `cmp` for each pair of elements
     * (expected to return -1, 0, or 1 if less-than, equal, or greater-than, respectively)
     * Uses memcmp() if no comparison function is provided
     *
     * Note that this operation is performed in O(n) time
     *
     * Returns one of: -1, 0, 1
     *
     */
    int cc_s_compare(HString lhs, HString rhs, StringCompareCallback cmp);

    /* String to C-style string
     *
     * Returns the internal string with a trailing NUL character
     *
     * The ownership of the returned string is not relinquished
     *
     * Returns NULL if out of memory (cannot allocate space for NUL character)
     */
    char *cc_s_to_cstring(HString string);

    /* Assign string to string
     *
     * String must have type Char, SignedChar, or UnsignedChar
     *
     * Returns CC_OK on success
     * Returns CC_TYPE_MISMATCH if there is a type mismatch
     */
    int cc_s_assign_cstring_n(HString string, const char *data, size_t len);
    int cc_s_assign_cstring(HString string, const char *data);

    /* Clears the string
     *
     * The size is reset to 0 after this function is called
     *
     * Note that this operation is performed in O(n) time
     */
    void cc_s_clear(HString string);

    /* Destroys the string
     *
     * The handle is invalidated after this function is called, but NOT freed!
     *
     * Note that this operation is performed in O(n) time
     */
    void cc_s_destroy_at(HString string);

    /* Destroys the string
     *
     * The handle is invalidated after this function is called
     *
     * Note that this operation is performed in O(n) time
     */
    void cc_s_destroy(HString string);
#ifdef __cplusplus
}
#endif

#endif /* CCSTRING_H */
