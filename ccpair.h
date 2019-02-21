#ifndef CCPAIR_H
#define CCPAIR_H

#include "element.h"

#if 0
/*
 *  TODO: HPair type is currently broken.
 */

#ifdef __cplusplus
extern "C" {
#endif
    /* Initializes a new pair with specified key and value types
     *
     * Returns NULL if allocation failed
     */
    HPair cc_p_init(ContainerElementType keyType, ContainerElementType valueType);

    /* Returns a copy of the pair
     *
     * `pair` is never modified
     *
     * Returns NULL if allocation failed
     */
    HPair cc_p_copy(HPair pair);

    /* Swaps two pairs
     *
     * Guaranteed not to fail
     */
    void cc_p_swap(HPair lhs, HPair rhs);

    /* Returns a reference to the value of the specified element in `out`
     *
     * If the element position is out of bounds, CC_BAD_PARAM is returned, otherwise, CC_OK is returned
     *
     * Note that this operation is performed in O(1) time
     *
     */
    int cc_p_node_key(HPair pair, Iterator element, HElementData out);

    /* Returns a reference to the value of the specified element in `out`
     *
     * If the element position is out of bounds, CC_BAD_PARAM is returned, otherwise, CC_OK is returned
     *
     * Note that this operation is performed in O(1) time
     *
     */
    int cc_p_node_data(HPair pair, Iterator element, HElementData out);

    /* Returns the metadata (type and callback information) of the pair key
     *
     * Note that this operation is performed in O(1) time
     *
     */
    HContainerElementMetaData cc_p_key_metadata(HPair pair);

    /* Returns the metadata (type and callback information) of the pair value
     *
     * Note that this operation is performed in O(1) time
     *
     */
    HContainerElementMetaData cc_p_value_metadata(HPair pair);

    /* Compares two pairs, and returns whether they are equal or not
     *
     * Uses the provided comparison function `cmp` for each pair of elements
     * (expected to return -1, 0, or 1 if less-than, equal, or greater-than, respectively)
     * Uses memcmp() if no comparison function is provided
     *
     * Note that this operation is performed in O(n) time
     *
     * Returns one of: 0 (equal), 1 (not equal), or CC_TYPE_MISMATCH
     *
     * Returns CC_TYPE_MISMATCH if pairs don't contain compatible types
     *
     */
    int cc_p_compare(HPair lhs, HPair rhs, ElementDualDataCallback cmp);

    /* Destroys the pair
     *
     * The handle is invalidated after this function is called
     *
     * Note that this operation is performed in O(n) time
     */
    void cc_p_destroy(HPair pair);
#ifdef __cplusplus
}
#endif

#endif

#endif // CCPAIR_H
