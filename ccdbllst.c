/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "ccdbllst.h"
#include "utility.h"

#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoublyLinkedListNode
{
    struct DoublyLinkedListNode *next, *prev;
#if C99
    void *data[]; /* Actually is a char[] containing the raw stored type
                   * The actual stored type is e.g. the int stored in the ElementData, not the ElementData itself
                   * (void * is just to align) */
#else
    HElementData data;
#endif
};

typedef struct DoublyLinkedListNode *HDoublyLinkedListNode;

struct DoublyLinkedList
{
    HDoublyLinkedListNode head, tail;
    size_t size;

    HContainerElementMetaData metadata;

#if C99
    /* Constructs on the internal storage, but then can be used as a pointer to another storage block
     * The internal storage must be destructed when destroying the class, by setting the storage pointer to NULL
     * To do this, call `cc_el_destroy_reference()` on the buffer
     */
    HElementData buffer;
#endif
};

/* Precondition: list must be non-empty */
static void cc_dll_insert_new_head(HDoublyLinkedList list, HDoublyLinkedListNode node)
{
    node->prev = NULL;
    node->next = list->head;
    list->head->prev = node;
    list->head = node;
}
/* Precondition: list must be non-empty */
static void cc_dll_insert_new_tail(HDoublyLinkedList list, HDoublyLinkedListNode node)
{
    node->next = NULL;
    node->prev = list->tail;
    list->tail->next = node;
    list->tail = node;
}
/* Precondition: list must be non-empty and node must be a part of list */
/* Postcondition: list no longer references node */
static void cc_dll_remove_node(HDoublyLinkedList list, HDoublyLinkedListNode node)
{
    if (node->prev)
        node->prev->next = node->next;

    if (node->next)
        node->next->prev = node->prev;

    if (node == list->head)
        list->head = node->next;

    if (node == list->tail)
        list->tail = node->prev;
}

#if C99
extern inline int cc_dll_push_front(HDoublyLinkedList list, unsigned flags, HConstElementData data, ElementDataCallback construct);
extern inline int cc_dll_pop_front(HDoublyLinkedList list, ElementDataCallback destruct);
extern inline int cc_dll_push_back(HDoublyLinkedList list, unsigned flags, HConstElementData data, ElementDataCallback construct);
extern inline int cc_dll_pop_back(HDoublyLinkedList list, ElementDataCallback destruct);
#else
int cc_dll_push_front(HDoublyLinkedList list, unsigned flags, HConstElementData data, ElementDataCallback construct) {
    return cc_dll_insert_after(list, flags, NULL, data, construct);
}
int cc_dll_pop_front(HDoublyLinkedList list, ElementDataCallback destruct) {
    return cc_dll_erase(list, list->head, destruct);
}
int cc_dll_push_back(HDoublyLinkedList list, unsigned flags, HConstElementData data, ElementDataCallback construct) {
    return cc_dll_insert_after(list, flags, list->tail, data, construct);
}
int cc_dll_pop_back(HDoublyLinkedList list, ElementDataCallback destruct) {
    return cc_dll_erase(list, list->tail, destruct);
}
#endif

size_t cc_dll_sizeof()
{
    return sizeof(struct DoublyLinkedList);
}

size_t cc_dll_max_capacity()
{
    return SIZE_MAX;
}

HDoublyLinkedList cc_dll_init(ContainerElementType type)
{
    HDoublyLinkedList result = MALLOC(cc_dll_sizeof());

    if (!result || CC_OK != cc_dll_init_at(result, cc_dll_sizeof(), type))
    {
        FREE(result);
        return NULL;
    }

    return result;
}

int cc_dll_init_at(void *buf, size_t buffer_size, ContainerElementType type)
{
    if (buffer_size < cc_dll_sizeof())
        CC_BAD_PARAM_HANDLER("buffer size too small");

    HDoublyLinkedList result = buf;

    result->head = result->tail = NULL;
    result->size = 0;
    result->metadata = cc_el_make_metadata(type);

    if (!result->metadata)
        CC_NO_MEM_HANDLER("out of memory");

#if C99
    result->buffer = cc_el_init(type, result->metadata, NULL, NULL);
    if (!result->buffer)
    {
        cc_el_kill_metadata(result->metadata);
        CC_NO_MEM_HANDLER("out of memory");
    }
#endif

    return CC_OK;
}

HDoublyLinkedList cc_dll_copy(HDoublyLinkedList list, ElementDataCallback construct, ElementDataCallback destruct)
{
    HDoublyLinkedList new_list = cc_dll_init(cc_el_metadata_type(list->metadata));
    HDoublyLinkedListNode old = list->head, node = NULL;

    if (!new_list)
        return NULL;

    cc_el_copy_metadata(new_list->metadata, list->metadata); /* Copy metadata (type and callbacks) from old list to this list */

    while (old)
    {
        HElementData d;
#if C99
        *cc_el_storage_location_ptr(list->buffer) = old->data;

        d = list->buffer;
#else
        d = (HElementData) old->data;
#endif

        CC_CLEANUP_ON_ERROR(cc_dll_insert_after(new_list, CC_COPY_VALUE, node, d, construct),
                            cc_dll_destroy(new_list, destruct); return NULL;); /* Failure to insert, now clean up */

        old = old->next;
        node = node == NULL? new_list->head: node->next;
    }

    return new_list;
}

int cc_dll_assign(HDoublyLinkedList dst, HDoublyLinkedList src)
{
    cc_dll_clear(dst, NULL);

    HDoublyLinkedList copy = cc_dll_copy(src, NULL, NULL);
    if (!copy)
        CC_NO_MEM_HANDLER("out of memory");

    cc_dll_swap(dst, copy);
    cc_dll_destroy(copy, NULL);

    return CC_OK;
}

void cc_dll_swap(HDoublyLinkedList lhs, HDoublyLinkedList rhs)
{
    struct DoublyLinkedList temp = *lhs;
    *lhs = *rhs;
    *rhs = temp;
}

int cc_dll_insert_after(HDoublyLinkedList list, unsigned flags, Iterator after, HConstElementData data, ElementDataCallback construct)
{
    int err;

    if (data && !cc_el_compatible_metadata_element(list->metadata, data))
        CC_TYPE_MISMATCH_HANDLER("cannot insert element", /*expected*/ cc_el_metadata_type(list->metadata), /*actual*/ cc_el_type(data));

#if C99
    int element_was_constructed = 0;

    /* In C99, we can initialize with a flexible array member */
    HDoublyLinkedListNode ptr = MALLOC(sizeof(*ptr) + cc_el_metadata_type_size(list->metadata));
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
    HDoublyLinkedListNode ptr = MALLOC(sizeof(*ptr));
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

    HDoublyLinkedListNode ll_after = after;

    /* Insert cases:
     * Empty list
     * Inserting at head of list
     * Inserting in middle of list
     * Inserting at tail of list
     */

    if (!list->head) /* Empty list */
    {
        ptr->next = ptr->prev = NULL;
        list->head = list->tail = ptr;
    }
    else if (after == list->tail) /* End of list */
        cc_dll_insert_new_tail(list, ptr);
    else if (after) /* Must be in the middle of the list */
    {
        /* It is guaranteed that the prior and succeeding elements exist, since this must be an insertion between two elements */

        /* Link to next */
        ptr->next = ll_after->next;
        ll_after->next->prev = ptr;

        /* Link to previous */
        ll_after->next = ptr;
        ptr->prev = ll_after;
    }
    else /* Beginning of list */
        cc_dll_insert_new_head(list, ptr);

    list->size += 1;

    return CC_OK;

cleanup:
#if C99
    if (element_was_constructed)
        cc_el_call_destructor_in(list->metadata, list->buffer);
#else
    cc_el_destroy(ptr->data);
#endif
    FREE(ptr);

    return err;
}

int cc_dll_erase(HDoublyLinkedList list, Iterator element, ElementDataCallback destruct)
{
    HDoublyLinkedListNode node = element;

    cc_dll_remove_node(list, node);

    list->size -= 1;

#if C99
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

int cc_dll_find(HDoublyLinkedList list, Iterator start, unsigned flags, HConstElementData data, ElementDualDataCallback compare, Iterator *out)
{
    HDoublyLinkedListNode node = start;

    if (!cc_el_compatible_metadata_element(list->metadata, data))
        CC_TYPE_MISMATCH_HANDLER("cannot find element of different type in list", /*expected*/ cc_el_metadata_type(list->metadata), /*actual*/ cc_el_type(data));

    for (; node; node = CC_DIRECTION(flags) == CC_FORWARD? node->next: node->prev)
    {
#if C99
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

    HDoublyLinkedListNode *handle_location = CC_DIRECTION(flags) == CC_FORWARD? &list->head: &list->tail;
    HDoublyLinkedListNode swap = NULL;

    switch (CC_ORGANIZATION(flags))
    {
        default: CC_BAD_PARAM_HANDLER("unsupported self-organization flag");
        case CC_ORGANIZE_NONE: break;
        case CC_ORGANIZE_MTF: /* Move to front (or back, if direction is backwards) */
            if (*handle_location != node)
            {
                if (CC_DIRECTION(flags) == CC_FORWARD)
                    swap = list->head;
                else
                    swap = list->tail;
            }
            break;
        case CC_ORGANIZE_TRANSPOSE: /* Transpose with nearest element */
            if (*handle_location != node)
            {
                if (CC_DIRECTION(flags) == CC_FORWARD)
                    swap = node->prev;
                else
                    swap = node->next;
            }
            break;
    }

    if (swap)
    {
#if C99
        memswap(node->data, swap->data, cc_el_metadata_type_size(list->metadata));
#else
        HElementData handle = node->data;
        node->data = swap->data;
        swap->data = handle;
#endif
    }

    return CC_OK;
}

int cc_dll_iterate(HDoublyLinkedList list, unsigned flags, ExtendedElementDataCallback callback, void *userdata)
{
    int direction = CC_DIRECTION(flags);
    HDoublyLinkedListNode node = direction == CC_FORWARD? list->head: list->tail;

    if (!callback)
        CC_BAD_PARAM_HANDLER("callback must be callable");

    for (; node; node = direction == CC_FORWARD? node->next: node->prev)
    {
#if C99
        *cc_el_storage_location_ptr(list->buffer) = node->data;

        CC_RETURN_ON_ERROR(callback(list->buffer, userdata));
#else
        CC_RETURN_ON_ERROR(callback(node->data, userdata));
#endif
    }

    return CC_OK;
}

void cc_dll_reverse(HDoublyLinkedList list)
{
    if (!list->head)
        return;

    /* Reverse forward pointers */
    HDoublyLinkedListNode prev = NULL, node = list->head;
    while (node)
    {
        HDoublyLinkedListNode next = node->next;
        node->next = prev;
        prev = node;
        node = next;
    }

    list->head = prev;

    /* Then reverse backward pointers */
    prev = NULL;
    node = list->tail;
    while (node)
    {
        HDoublyLinkedListNode next = node->prev;
        node->prev = prev;
        prev = node;
        node = next;
    }

    list->tail = prev;
}

size_t cc_dll_size(HDoublyLinkedList list)
{
    return list->size;
}

Iterator cc_dll_begin(HDoublyLinkedList list)
{
    return list->head;
}

Iterator cc_dll_rbegin(HDoublyLinkedList list)
{
    return list->tail;
}

HContainerElementMetaData cc_dll_metadata(HDoublyLinkedList list)
{
    return list->metadata;
}

Iterator cc_dll_next(HDoublyLinkedList list, Iterator node)
{
    (void) list; /* Not needed for a linked list! */
    return ((HDoublyLinkedListNode) node)->next;
}

Iterator cc_dll_rnext(HDoublyLinkedList list, Iterator node)
{
    (void) list; /* Not needed for a linked list! */
    return ((HDoublyLinkedListNode) node)->prev;
}

HElementData cc_dll_node_data_easy(HDoublyLinkedList list, Iterator node)
{
    if (cc_dll_node_data(list, node, list->buffer) != CC_OK)
        return NULL;

    return list->buffer;
}

int cc_dll_node_data(HDoublyLinkedList list, Iterator node, HElementData out)
{
    HDoublyLinkedListNode ll_node = node;

    if (!cc_el_compatible_metadata_element(list->metadata, out))
        CC_TYPE_MISMATCH_HANDLER("cannot get element", /*expected*/ cc_el_metadata_type(list->metadata), /*actual*/ cc_el_type(out));

#if C99
    *cc_el_storage_location_ptr(out) = ll_node->data;
#else
    *cc_el_storage_location_ptr(out) = *cc_el_storage_location_ptr(ll_node->data);
#endif

    return CC_OK;
}

int cc_dll_compare(HDoublyLinkedList lhs, HDoublyLinkedList rhs, ElementDualDataCallback cmp)
{
    int comparison;

    if (!cc_el_compatible_metadata(lhs->metadata, rhs->metadata))
        CC_TYPE_MISMATCH_HANDLER("cannot compare elements", /*expected*/ cc_el_metadata_type(lhs->metadata), /*actual*/ cc_el_metadata_type(rhs->metadata));

    HDoublyLinkedListNode lnode = lhs->head, rnode = rhs->head;

    while (lnode && rnode)
    {
#if C99
        cc_dll_node_data(lhs, lnode, lhs->buffer);
        cc_dll_node_data(rhs, rnode, rhs->buffer);

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

void cc_dll_clear(HDoublyLinkedList list, ElementDataCallback destruct)
{
    HDoublyLinkedListNode node = list->head;

    while (node)
    {
#if C99
        *cc_el_storage_location_ptr(list->buffer) = node->data;

        if (destruct)
            destruct(list->buffer);
        else
            cc_el_call_destructor_in(list->metadata, list->buffer);
#else
        cc_el_destroy(node->data);
#endif

        HDoublyLinkedListNode next = node->next;

        FREE(node);
        node = next;
    }

    list->head = NULL;
    list->size = 0;
}

void cc_dll_destroy_at(HDoublyLinkedList list, ElementDataCallback destruct)
{
    cc_dll_clear(list, destruct);

#if C99
    /* Reinitialize buffer to point to original storage (since we constructed it and now need to destruct it) */
    cc_el_destroy_reference(list->buffer);
#endif
    cc_el_kill_metadata(list->metadata);
}

void cc_dll_destroy(HDoublyLinkedList list, ElementDataCallback destruct)
{
    cc_dll_destroy_at(list, destruct);
    FREE(list);
}

#ifdef __cplusplus
}
#endif
