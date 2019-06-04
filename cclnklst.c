/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "cclnklst.h"

#include "utility.h"

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

typedef struct LinkedListNode *HLinkedListNode;

struct LinkedList
{
    HLinkedListNode head, tail;
    size_t size; /* Size stored in upper bits, lowest bit specifies whether metadata is owned (0) or external (1) */

    HContainerElementMetaData metadata;

#ifdef C99
    /* Constructs on the internal storage, but then can be used as a pointer to another storage block
     * The internal storage must be destructed when destroying the class, by setting the storage pointer to NULL
     * To do this, call `cc_el_destroy_reference()` on the buffer
     */
    HElementData buffer;
#endif
};

#ifdef C99
extern inline int cc_ll_push_front(HLinkedList list, unsigned flags, HConstElementData data, ElementDataCallback construct);
extern inline int cc_ll_pop_front(HLinkedList list, ElementDataCallback destruct);
extern inline int cc_ll_push_back(HLinkedList list, unsigned flags, HConstElementData data, ElementDataCallback construct);
#else
int cc_ll_push_front(HLinkedList list, unsigned flags, HConstElementData data, ElementDataCallback construct) {
    return cc_ll_insert_after(list, flags, NULL, data, construct);
}
int cc_ll_pop_front(HLinkedList list, ElementDataCallback destruct) {
    return cc_ll_erase_after(list, NULL, destruct);
}
int cc_ll_push_back(HLinkedList list, unsigned flags, HConstElementData data, ElementDataCallback construct) {
    return cc_ll_insert_after(list, flags, cc_ll_rbegin(list), data, construct);
}
#endif

size_t cc_ll_sizeof()
{
    return sizeof(struct LinkedList);
}

size_t cc_ll_max_capacity()
{
    return SIZE_MAX / 2;
}

static int cc_ll_has_external_metadata(HLinkedList list)
{
    return list->size & 1;
}

static void cc_ll_set_size(HLinkedList list, size_t size)
{
    list->size = (size << 1) | (list->size & 1);
}

HLinkedList cc_ll_init(ContainerElementType type, HContainerElementMetaData externalMeta)
{
    HLinkedList result = MALLOC(cc_ll_sizeof(), 1);

    if (!result || CC_OK != cc_ll_init_at(result, cc_ll_sizeof(), type, externalMeta))
    {
        FREE(result);
        return NULL;
    }

    return result;
}

int cc_ll_init_at(void *buf, size_t buffer_size, ContainerElementType type, HContainerElementMetaData externalMeta)
{
    if (buffer_size < cc_ll_sizeof())
        CC_BAD_PARAM_HANDLER("buffer size too small");

    HLinkedList result = buf;

    result->head = result->tail = NULL;
    result->size = !!externalMeta;
    result->metadata = externalMeta? externalMeta: cc_el_make_metadata(type);

    if (!result->metadata)
        CC_NO_MEM_HANDLER("out of memory");

#ifdef C99
    result->buffer = cc_el_init(type, result->metadata, NULL, NULL);
    if (!result->buffer)
    {
        if (!externalMeta)
            cc_el_kill_metadata(result->metadata);
        CC_NO_MEM_HANDLER("out of memory");
    }
#endif

    return CC_OK;
}

HLinkedList cc_ll_copy(HLinkedList list, HContainerElementMetaData externalMeta, ElementDataCallback construct, ElementDataCallback destruct)
{
    HLinkedList new_list = cc_ll_init(cc_el_metadata_type(list->metadata), externalMeta);
    HLinkedListNode old = list->head, node = NULL;

    if (!new_list)
        return NULL;

    if (!externalMeta)
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

        CC_CLEANUP_ON_ERROR(cc_ll_insert_after(new_list, CC_COPY_VALUE, node, d, construct),
                            cc_ll_destroy(new_list, destruct); return NULL;); /* Failure to insert, now clean up */

        old = old->next;
        node = node == NULL? new_list->head: node->next;
    }

    return new_list;
}

int cc_ll_assign(HLinkedList dst, HLinkedList src)
{
    cc_ll_clear(dst, NULL);

    HLinkedList copy = cc_ll_copy(src, NULL, NULL, NULL);
    if (!copy)
        CC_NO_MEM_HANDLER("out of memory");

    cc_ll_swap(dst, copy);
    cc_ll_destroy(copy, NULL);

    return CC_OK;
}

void cc_ll_swap(HLinkedList lhs, HLinkedList rhs)
{
    struct LinkedList temp = *lhs;
    *lhs = *rhs;
    *rhs = temp;
}

int cc_ll_insert_after(HLinkedList list, unsigned flags, Iterator after, HConstElementData data, ElementDataCallback construct)
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
    {
        if (CC_MOVE_SEMANTICS(flags) == CC_MOVE_VALUE)
            CC_CLEANUP_ON_ERROR(cc_el_move_contents(list->buffer, data), err = ret; goto cleanup;);
        else
            CC_CLEANUP_ON_ERROR(cc_el_copy_contents(list->buffer, data), err = ret; goto cleanup;);
    }
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
    {
        if (CC_MOVE_SEMANTICS(flags) == CC_MOVE_VALUE)
            CC_CLEANUP_ON_ERROR(cc_el_move_contents(ptr->data, data), err = ret; goto cleanup;);
        else
            CC_CLEANUP_ON_ERROR(cc_el_copy_contents(ptr->data, data), err = ret; goto cleanup;);
    }
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

    if (ll_after == list->tail)
        list->tail = ptr;

    cc_ll_set_size(list, cc_ll_size(list) + 1);

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
    cc_ll_set_size(list, cc_ll_size(list) - 1);

    if (list->tail == node)
        list->tail = (HLinkedListNode) after;

#ifdef C99
    *cc_el_storage_location_ptr(list->buffer) = node->data;

    if (destruct)
        destruct(list->buffer);
    else
        cc_el_call_destructor_in(list->metadata, list->buffer);
#else
    cc_el_destroy(node->data);
#endif
    FREE(node);

    return CC_OK;
}

int cc_ll_find(HLinkedList list, Iterator start, unsigned flags, HConstElementData data, ElementDualDataCallback compare, Iterator *out)
{
    HLinkedListNode node = start;

    if (!cc_el_compatible_metadata_element(list->metadata, data))
        CC_TYPE_MISMATCH_HANDLER("cannot find element of different type in list", /*expected*/ cc_el_metadata_type(list->metadata), /*actual*/ cc_el_type(data));

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

    HLinkedListNode swap = NULL;

    switch (CC_ORGANIZATION(flags))
    {
        default: CC_BAD_PARAM_HANDLER("unsupported self-organization flag");
        case CC_ORGANIZE_NONE: break;
        case CC_ORGANIZE_MTF: /* Move to front (or back, if direction is backwards) */
            if (list->head != node)
                swap = list->head;
            break;
        case CC_ORGANIZE_TRANSPOSE: /* Transpose with nearest element */
            if (list->head != node)
                swap = node->next; /* TODO: wrong? it should be the previous node, no? */
            break;
    }

    if (swap)
    {
#ifdef C99
        memswap(node->data, swap->data, cc_el_metadata_type_size(list->metadata));
#else
        HElementData handle = node->data;
        node->data = swap->data;
        swap->data = handle;
#endif
    }

    return CC_OK;
}

void cc_ll_reverse(HLinkedList list)
{
    if (!list->head)
        return;

    HLinkedListNode prev = NULL, node = list->head;
    while (node)
    {
        HLinkedListNode next = node->next;
        node->next = prev;
        prev = node;
        node = next;
    }

    list->head = prev;
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

size_t cc_ll_size(HLinkedList list)
{
    return list->size >> 1;
}

Iterator cc_ll_begin(HLinkedList list)
{
    return list->head;
}

Iterator cc_ll_rbegin(HLinkedList list)
{
    return list->tail;
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

HElementData cc_ll_node_data_easy(HLinkedList list, Iterator node)
{
    if (cc_ll_node_data(list, node, list->buffer) != CC_OK)
        return NULL;

    return list->buffer;
}

int cc_ll_node_data(HLinkedList list, Iterator node, HElementData out)
{
    HLinkedListNode ll_node = node;

    if (!cc_el_compatible_metadata_element(list->metadata, out))
        CC_TYPE_MISMATCH_HANDLER("cannot get element", /*expected*/ cc_el_metadata_type(list->metadata), /*actual*/ cc_el_type(out));

#ifdef C99
    *cc_el_storage_location_ptr(out) = ll_node->data;
#else
    *cc_el_storage_location_ptr(out) = cc_el_storage_location(ll_node->data);
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
        cc_el_destroy(node->data);
#endif

        HLinkedListNode next = node->next;

        FREE(node);
        node = next;
    }

    list->head = NULL;
    cc_ll_set_size(list, 0);
}

void cc_ll_destroy_at(HLinkedList list, ElementDataCallback destruct)
{
    cc_ll_clear(list, destruct);

#ifdef C99
    /* Reinitialize buffer to point to original storage (since we constructed it and now need to destruct it) */
    cc_el_destroy_reference(list->buffer);
#endif
    if (!cc_ll_has_external_metadata(list))
        cc_el_kill_metadata(list->metadata);
}

void cc_ll_destroy(HLinkedList list, ElementDataCallback destruct)
{
    cc_ll_destroy_at(list, destruct);
    FREE(list);
}

#ifdef __cplusplus
}
#endif
