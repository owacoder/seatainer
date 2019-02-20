#include "ccvector.h"

#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Vector
{
    char *data; /* Array of `size * cc_el_metadata_type_size()` bytes */
    HContainerElementMetaData metadata;

    size_t size, capacity;

    /* Constructs on the internal storage, but then can be used as a pointer to another storage block
     * The internal storage must be destructed when destroying the class, by setting the storage pointer to NULL
     * To do this, call `cc_el_destroy_reference()` on the buffer
     */
    HElementData buffer;
};

#ifdef C99
extern inline int cc_v_push_back(HVector list, HConstElementData data, ElementDataCallback construct);
extern inline int cc_v_pop_back(HVector list, ElementDataCallback destruct);
#else
int cc_v_push_back(HVector list, HConstElementData data, ElementDataCallback construct)
{
    return cc_v_insert(list, list->size, data, construct);
}

int cc_v_pop_back(HVector list, ElementDataCallback destruct)
{
    return cc_v_erase(list, list->size - 1, destruct);
}
#endif

HVector cc_v_init(ContainerElementType type)
{
    HVector result = MALLOC(sizeof(*result), 1);

    if (!result)
        return NULL;

    result->data = NULL;
    result->size = result->capacity = 0;
    result->metadata = cc_el_make_metadata(type);
    if (!result->metadata)
    {
        FREE(result);
        result = NULL;
    }

    result->buffer = cc_el_init(type, result->metadata, NULL, NULL);
    if (!result->buffer)
    {
        cc_el_kill_metadata(result->metadata);
        FREE(result);
        result = NULL;
    }

    return result;
}

HVector cc_v_grow(HVector list, size_t size)
{
    if (list->capacity >= size)
        return list;

    size_t el_size = cc_el_sizeof();
    if (SIZE_MAX / el_size < size)
        return NULL;

    size_t new_size = MAX(size, list->capacity + (list->capacity >> 1));
    new_size = MAX(8, new_size);

    char *new_data = REALLOC(list->data, new_size, cc_el_metadata_type_size(list->metadata));
    if (new_data == NULL)
        return NULL; /* Leave `list` unchanged on failure to allocate */

    list->data = new_data;
    list->capacity = new_size;

    return list;
}

static void cc_v_move(HVector list, size_t src, size_t dst, size_t count)
{
    if (count)
    {
        size_t element_size = cc_el_metadata_type_size(list->metadata);
        memmove(list->data + dst * element_size, list->data + src * element_size, count * element_size);
    }
}

#if 0
static HVector cc_v_construct(HVector list, size_t last_index, ElementDataCallback construct)
{
    for (size_t i = list->size; i != last_index; ++i)
    {
        if (construct)
            construct(cc_v_node_data(list, i));
        else
            cc_el_call_constructor_in(list->metadata, cc_v_node_data(list, i));
    }

    return list;
}
#endif

HVector cc_v_copy(HVector list, ElementDataCallback construct, ElementDataCallback destruct)
{
    (void) construct;
    (void) destruct;

    HVector new_list = cc_v_init(cc_el_metadata_type(list->metadata));
    if (!new_list)
        return NULL;

    cc_el_copy_metadata(new_list->metadata, list->metadata);

    if (cc_v_grow(new_list, list->size) == NULL)
    {
        cc_v_destroy(new_list, NULL); /* Nothing initialized yet, so don't pass `destruct` */
        return NULL;
    }
    new_list->size = list->size; /* cc_v_grow() doesn't set the size */

    int last_element_was_fully_constructed = 0;
    size_t i, element_size = cc_el_metadata_type_size(list->metadata);
    for (i = 0; i < list->size; ++i)
    {
        /* Set storage location */
        *cc_el_storage_location_ptr(new_list->buffer) = new_list->data + i * element_size;
        *cc_el_storage_location_ptr(list->buffer) = list->data + i * element_size;

        /* Construct new element */
        last_element_was_fully_constructed = 0;
        if (construct)
            CC_CLEANUP_ON_ERROR(construct(new_list->buffer), goto cleanup;);
        else
            CC_CLEANUP_ON_ERROR(cc_el_call_constructor_in(new_list->metadata, new_list->buffer), goto cleanup;);

        last_element_was_fully_constructed = 1;
        /* Copy contents */
        CC_CLEANUP_ON_ERROR(cc_el_copy_contents(new_list->buffer, list->buffer), goto cleanup;);
    }

    return new_list;

cleanup:
    new_list->size = i + last_element_was_fully_constructed; /* Add one if we need to delete the latest element */

    cc_v_destroy(new_list, NULL);
    return NULL;
}

void cc_v_swap(HVector lhs, HVector rhs)
{
    struct Vector temp = *lhs;
    *lhs = *rhs;
    *rhs = temp;
}

int cc_v_insert(HVector list, size_t before, HConstElementData data, ElementDataCallback construct)
{
    int element_was_constructed = 0;
    int err = CC_OK;

    (void) construct;

    if (before > list->size)
        CC_BAD_PARAM_HANDLER("element out-of-bounds");

    if (data && !cc_el_compatible_metadata_element(list->metadata, data))
        CC_TYPE_MISMATCH_HANDLER("cannot insert element", /*expected*/ cc_el_metadata_type(list->metadata), /*actual*/ cc_el_type(data));

    if (cc_v_grow(list, list->size + 1) == NULL)
        CC_NO_MEM_HANDLER("out of memory");

    /* Move vector so we can insert at `before` */
    cc_v_move(list, before, before + 1, list->size - before);
    /* Insert data */
    size_t element_size = cc_el_metadata_type_size(list->metadata);

    /* Set storage location */
    *cc_el_storage_location_ptr(list->buffer) = list->data + before * element_size;

    /* Construct element */
    if (construct)
        CC_CLEANUP_ON_ERROR(construct(list->buffer), err = ret; goto cleanup;);
    else
        CC_CLEANUP_ON_ERROR(cc_el_call_constructor_in(list->metadata, list->buffer), err = ret; goto cleanup;);

    element_was_constructed = 1;

    /* Copy contents */
    if (data)
        CC_CLEANUP_ON_ERROR(cc_el_copy_contents(list->buffer, (HElementData) data), err = ret; goto cleanup;);

    /* Update list size */
    list->size += 1;

    return CC_OK;

cleanup:
    cc_v_move(list, before + 1, before, list->size - before);

    if (element_was_constructed)
        cc_el_call_destructor_in(list->metadata, list->buffer);

    return err;
}

int cc_v_erase(HVector list, size_t element, ElementDataCallback destruct)
{
    if (element >= list->size)
        CC_BAD_PARAM_HANDLER("element out-of-bounds");

    *cc_el_storage_location_ptr(list->buffer) = list->data + element * cc_el_metadata_type_size(list->metadata);

    /* Delete data at element */
    if (destruct)
        destruct(list->buffer);
    else
        cc_el_call_destructor_in(list->metadata, list->buffer);

    /* Move vector over the top of the element we got rid of */
    cc_v_move(list, element + 1, element, list->size - element - 1);
    /* Decrease list size */
    list->size -= 1;

    return CC_OK;
}

int cc_v_find(HVector list, Iterator start, unsigned flags, HConstElementData data, ElementDualDataCallback compare, Iterator *out)
{
    Iterator node = start;

    if (!cc_el_compatible_metadata_element(list->metadata, data))
        CC_TYPE_MISMATCH_HANDLER("cannot find element of different type in list", /*expected*/ cc_el_metadata_type(list->metadata), /*actual*/ cc_el_type(data));

    for (; node; node = CC_DIRECTION(flags) == CC_FORWARD? cc_v_next(list, node): cc_v_rnext(list, node))
    {
        *cc_el_storage_location_ptr(list->buffer) = node;

        int ret;
        if (compare)
            ret = compare(list->buffer, (HElementData) data);
        else
            ret = cc_el_call_compare_in(list->metadata, list->buffer, (HElementData) data);

        if (ret == CC_OK)
            break;
        else if (ret != CC_LESS_THAN && ret != CC_GREATER_THAN) /* If an error code, return immediately */
            return ret;
    }

    *out = node;
    return CC_OK;
}

int cc_v_iterate(HVector list, unsigned flags, ExtendedElementDataCallback callback, void *userdata)
{
    if (!callback)
        CC_BAD_PARAM_HANDLER("callback must be callable");

    int direction = CC_DIRECTION(flags);
    Iterator i;
    for (i = direction == CC_FORWARD? cc_v_begin(list): cc_v_rbegin(list); i != NULL; i = direction == CC_FORWARD? cc_v_next(list, i): cc_v_rnext(list, i))
    {
        *cc_el_storage_location_ptr(list->buffer) = i;
        CC_RETURN_ON_ERROR(callback(list->buffer, userdata));
    }

    return CC_OK;
}

size_t cc_v_size_of(HVector list)
{
    return list->size;
}

size_t cc_v_capacity_of(HVector list)
{
    return list->capacity;
}

HContainerElementMetaData cc_v_metadata(HVector list)
{
    return list->metadata;
}

Iterator cc_v_begin(HVector list)
{
    return list->size == 0? NULL: list->data;
}

Iterator cc_v_rbegin(HVector list)
{
    return list->size == 0? NULL: list->data + (list->size - 1) * cc_el_metadata_type_size(list->metadata);
}

Iterator cc_v_next(HVector list, Iterator node)
{
    size_t sz = cc_el_metadata_type_size(list->metadata);

    if (node == NULL || node == list->data + (list->size - 1) * sz)
        return NULL;

    return (char *) node + sz;
}

Iterator cc_v_rnext(HVector list, Iterator node)
{
    if (node == NULL || node == list->data)
        return NULL;

    return (char *) node - cc_el_metadata_type_size(list->metadata);
}

int cc_v_node_data(HVector list, Iterator element, HElementData out)
{
    size_t el_size = cc_el_metadata_type_size(list->metadata);

    if (element == NULL || (char *) element >= list->data + list->size * el_size)
        CC_BAD_PARAM_HANDLER("element out-of-bounds");

    if (!cc_el_compatible_metadata_element(list->metadata, out))
        CC_TYPE_MISMATCH_HANDLER("cannot get element", /*expected*/ cc_el_metadata_type(list->metadata), /*actual*/ cc_el_type(out));

    *cc_el_storage_location_ptr(out) = element;
    return CC_OK;
}

int cc_v_compare(HVector lhs, HVector rhs, ElementDualDataCallback cmp)
{
    if (!cc_el_compatible_metadata(lhs->metadata, rhs->metadata))
        CC_TYPE_MISMATCH_HANDLER("cannot compare elements", /*expected*/ cc_el_metadata_type(lhs->metadata), /*actual*/ cc_el_metadata_type(rhs->metadata));

    int comparison;

    Iterator lit = cc_v_begin(lhs);
    Iterator rit = cc_v_begin(rhs);

    for (; lit && rit; lit = cc_v_next(lhs, lit), rit = cc_v_next(rhs, rit))
    {
        cc_v_node_data(lhs, lit, lhs->buffer);
        cc_v_node_data(rhs, rit, rhs->buffer);

        if (cmp)
            comparison = cmp(lhs->buffer, rhs->buffer);
        else
            comparison = cc_el_call_compare_in(lhs->metadata, lhs->buffer, rhs->buffer);

        if (comparison != 0)
            return comparison;
    }

    if (rit)
        comparison = -1; /* lhs is shorter */
    else if (lit)
        comparison = 1; /* rhs is shorter */
    else
        comparison = 0;

    return comparison;
}

void cc_v_clear(HVector list, ElementDataCallback destruct)
{
    HElementData el = list->buffer;

    if (destruct || cc_el_destructor_in(list->metadata))
    {
        Iterator i;
        for (i = cc_v_begin(list); i != NULL; i = cc_v_next(list, i))
        {
            cc_v_node_data(list, i, el);
            if (destruct)
                destruct(el);
            else
                cc_el_call_destructor_in(list->metadata, el);
        }
    }

    list->size = 0;
}

void cc_v_destroy(HVector list, ElementDataCallback destruct)
{
    cc_v_clear(list, destruct);

    /* Reinitialize buffer to point to original storage (since we constructed it and now need to destruct it) */
    cc_el_destroy_reference(list->buffer);
    cc_el_kill_metadata(list->metadata);
    FREE(list->data);
    FREE(list);
}

#ifdef __cplusplus
}
#endif
