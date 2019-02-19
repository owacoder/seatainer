#include "cclnklst.h"

#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct LinkedListNode
{
    struct LinkedListNode *next;
#ifdef C99
    void *data[]; /* Actually is a char[] containing the raw stored type
                   * The actual stored type is e.g. the int stored in the ElementData, not the ElementData itself
                   * (void * is just to align) */
#else
    HElementData data;
#endif
};

struct LinkedList
{
    HLinkedListNode head;
    size_t size;

    HContainerElementMetaData metadata;

#ifdef C99
    /* Constructs on the internal storage, but then can be used as a pointer to another storage block
     * The internal storage must be destructed when destroying the class, by setting the storage pointer to NULL */
    HElementData buffer;
#endif
};

#ifdef C99
extern inline int cc_ll_push_front(HLinkedList list, HConstElementData data, ElementDataCallback construct);
extern inline int cc_ll_pop_front(HLinkedList list, ElementDataCallback destruct);
#else
int cc_ll_push_front(HLinkedList list, HConstElementData data, ElementDataCallback construct) {
    return cc_ll_insert_after(list, NULL, data, construct);
}
int cc_ll_pop_front(HLinkedList list, ElementDataCallback destruct) {
    return cc_ll_erase_after(list, NULL, destruct);
}
#endif

HLinkedList cc_ll_init(ContainerElementType type)
{
    HLinkedList result = MALLOC(sizeof(*result), 1);

    if (!result)
        return NULL;

    result->head = NULL;
    result->size = 0;
    result->metadata = cc_el_make_metadata(type);

    if (!result->metadata)
    {
        FREE(result);
        result = NULL;
    }

#ifdef C99
    result->buffer = cc_el_init(type, result->metadata, NULL, NULL);
    if (!result->buffer)
    {
        cc_el_kill_metadata(result->metadata);
        FREE(result);
        result = NULL;
    }
#endif

    return result;
}

HLinkedList cc_ll_copy(HLinkedList list, ElementDataCallback construct, ElementDataCallback destruct)
{
    HLinkedList new_list = cc_ll_init(cc_el_metadata_type(list->metadata));
    HLinkedListNode old = list->head, node = NULL;

    if (!new_list)
        return NULL;

    cc_el_copy_metadata(new_list->metadata, list->metadata); /* Copy metadata (type and callbacks) from old list to this list */

    while (old)
    {
        HElementData d;
#ifdef C99
        *cc_el_storage_location_ptr(list->buffer) = old->data;

        d = list->buffer;
#else
        d = (HElementData) old->data;
#endif

        CC_CLEANUP_ON_ERROR(cc_ll_insert_after(new_list, node, d, construct),
                            cc_ll_destroy(new_list, destruct); return NULL;); /* Failure to insert, now clean up */

        old = old->next;
        node = node == NULL? new_list->head: node->next;
    }

    return new_list;
}

void cc_ll_swap(HLinkedList lhs, HLinkedList rhs)
{
    struct LinkedList temp = *lhs;
    *lhs = *rhs;
    *rhs = temp;
}

int cc_ll_insert_after(HLinkedList list, Iterator after, HConstElementData data, ElementDataCallback construct)
{
    int err;

    if (data && !cc_el_compatible_metadata_element(list->metadata, data))
        CC_TYPE_MISMATCH_HANDLER("cannot insert element", /*expected*/ cc_el_metadata_type(list->metadata), /*actual*/ cc_el_type(data));

#ifdef C99
    int element_was_constructed = 0;

    /* In C99, we can initialize with a flexible array member */
    HLinkedListNode ptr = MALLOC(sizeof(*ptr) + cc_el_metadata_type_size(list->metadata), 1);
    if (!ptr)
        CC_NO_MEM_HANDLER("out of memory");

    *cc_el_storage_location_ptr(list->buffer) = ptr->data;

    if (construct)
        CC_CLEANUP_ON_ERROR(construct(list->buffer), err = ret; goto cleanup;);
    else
        CC_CLEANUP_ON_ERROR(cc_el_call_constructor_in(list->metadata, list->buffer), err = ret; goto cleanup;);

    element_was_constructed = 1;

    if (data)
        CC_CLEANUP_ON_ERROR(cc_el_copy_contents(list->buffer, data), err = ret; goto cleanup;);
#else
    /* TODO: not implemented yet */
    HLinkedListNode ptr = MALLOC(sizeof(*ptr), 1);
    if (!ptr)
        CC_NO_MEM_HANDLER("out of memory");

    ptr->data = cc_el_init(cc_el_metadata_type(list->metadata), list->metadata, construct, &err);
    if (!ptr->data)
    {
        FREE(ptr);
        return err;
    }

    if (data)
        CC_CLEANUP_ON_ERROR(cc_el_copy_contents(ptr->data, data), err = ret; goto cleanup;);
#endif

    HLinkedListNode ll_after = after;

    if (after) /* Must be in the middle of the list */
    {
        ptr->next = ll_after->next;
        ll_after->next = ptr;
    }
    else /* Insert at beginning of list */
    {
        ptr->next = list->head;
        list->head = ptr;
    }

    list->size += 1;

    return CC_OK;

cleanup:
#ifdef C99
    if (element_was_constructed)
        cc_el_call_destructor_in(list->metadata, list->buffer);
#else
    cc_el_destroy(ptr->data);
#endif
    FREE(ptr);

    return err;
}

int cc_ll_erase_after(HLinkedList list, Iterator after, ElementDataCallback destruct)
{
    HLinkedListNode *begin = after? &((HLinkedListNode) after)->next: &list->head;
    HLinkedListNode node = *begin;

    if (!node) /* Nothing to delete, trying to delete end of list */
        return CC_OK;

    *begin = node->next;
    list->size -= 1;

#ifdef C99
    *cc_el_storage_location_ptr(list->buffer) = node->data;

    if (destruct)
        destruct(list->buffer);
    else
        cc_el_call_destructor_in(list->metadata, list->buffer);
#else
    if (destruct)
        destruct(node->data);
    else
        cc_el_call_destructor_in(list->metadata, node->data);

    cc_el_destroy(node->data);
#endif
    FREE(node);

    return CC_OK;
}

int cc_ll_find(HLinkedList list, Iterator start, HConstElementData data, ElementDualDataCallback compare, Iterator *out)
{
    HLinkedListNode node = start;

    for (; node; node = node->next)
    {
#ifdef C99
        *cc_el_storage_location_ptr(list->buffer) = node->data;

        int ret;
        if (compare)
            ret = compare(list->buffer, (HElementData) data);
        else
            ret = cc_el_call_compare_in(list->metadata, list->buffer, (HElementData) data);
#else
        int ret;
        if (compare)
            ret = compare(node->data, (HElementData) data);
        else
            ret = cc_el_call_compare_in(list->metadata, node->data, (HElementData) data);
#endif

        if (ret == CC_OK)
            break;
        else if (ret != CC_LESS_THAN && ret != CC_GREATER_THAN) /* If an error code, return immediately */
            return ret;
    }

    *out = node;
    return CC_OK;
}

int cc_ll_iterate(HLinkedList list, ExtendedElementDataCallback callback, void *userdata)
{
    HLinkedListNode node = list->head;

    if (!callback)
        CC_BAD_PARAM_HANDLER("callback must be callable");

    for (; node; node = node->next)
    {
#ifdef C99
        *cc_el_storage_location_ptr(list->buffer) = node->data;

        CC_RETURN_ON_ERROR(callback(list->buffer, userdata));
#else
        CC_RETURN_ON_ERROR(callback(node->data, userdata));
#endif
    }

    return CC_OK;
}

size_t cc_ll_size_of(HLinkedList list)
{
    return list->size;
}

Iterator cc_ll_begin(HLinkedList list)
{
    return list->head;
}

HContainerElementMetaData cc_ll_metadata(HLinkedList list)
{
    return list->metadata;
}

Iterator cc_ll_next(HLinkedList list, Iterator node)
{
    (void) list; /* Not needed for a linked list! */
    return ((HLinkedListNode) node)->next;
}

int cc_ll_node_data(HLinkedList list, Iterator node, HElementData out)
{
    HLinkedListNode ll_node = node;

    if (!cc_el_compatible_metadata_element(list->metadata, out))
        CC_TYPE_MISMATCH_HANDLER("cannot get element", /*expected*/ cc_el_metadata_type(list->metadata), /*actual*/ cc_el_type(out));

#ifdef C99
    *cc_el_storage_location_ptr(out) = ll_node->data;
#else
    if (!cc_el_compatible_metadata_element(list->metadata, out))
        CC_TYPE_MISMATCH_HANDLER("cannot get element", /*expected*/ cc_el_metadata_type(list->metadata), /*actual*/ cc_el_type(out));

    *cc_el_storage_location_ptr(out) = *cc_el_storage_location_ptr(ll_node->data);
#endif

    return CC_OK;
}

int cc_ll_compare(HLinkedList lhs, HLinkedList rhs, ElementDualDataCallback cmp)
{
    int comparison;

    if (!cc_el_compatible_metadata(lhs->metadata, rhs->metadata))
        CC_TYPE_MISMATCH_HANDLER("cannot compare elements", /*expected*/ cc_el_metadata_type(lhs->metadata), /*actual*/ cc_el_metadata_type(rhs->metadata));

    HLinkedListNode lnode = lhs->head, rnode = rhs->head;

    while (lnode && rnode)
    {
#ifdef C99
        cc_ll_node_data(lhs, lnode, lhs->buffer);
        cc_ll_node_data(rhs, rnode, rhs->buffer);

        if (cmp)
            comparison = cmp(lhs->buffer, rhs->buffer);
        else
            comparison = cc_el_call_compare_in(lhs->metadata, lhs->buffer, rhs->buffer);
#else
        if (cmp)
            comparison = cmp((HElementData) lnode->data, (HElementData) rnode->data);
        else
            comparison = cc_el_call_compare_in(lhs->metadata, lnode->data, rnode->data);
#endif

        if (comparison != 0)
            return comparison;

        lnode = lnode->next;
        rnode = rnode->next;
    }

    if (rnode)
        return -1; /* lhs is shorter */
    else if (lnode)
        return 1; /* rhs is shorter */

    return 0;
}

void cc_ll_clear(HLinkedList list, ElementDataCallback destruct)
{
    HLinkedListNode node = list->head;

    while (node)
    {
#ifdef C99
        *cc_el_storage_location_ptr(list->buffer) = node->data;

        if (destruct)
            destruct(list->buffer);
        else
            cc_el_call_destructor_in(list->metadata, list->buffer);
#else
        if (destruct)
            destruct(node->data);
        else
            cc_el_call_destructor_in(list->metadata, node->data);

        cc_el_destroy(node->data);
#endif

        HLinkedListNode next = node->next;

        FREE(node);
        node = next;
    }

    list->head = NULL;
    list->size = 0;
}

void cc_ll_destroy(HLinkedList list, ElementDataCallback destruct)
{
    cc_ll_clear(list, destruct);

#ifdef C99
    /* Reinitialize buffer to point to original storage (since we constructed it and now need to destruct it) */
    cc_el_destroy_reference(list->buffer);
#endif
    cc_el_kill_metadata(list->metadata);
    FREE(list);
}

#ifdef __cplusplus
}
#endif
