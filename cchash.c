#include "cchash.h"

#include "cclnklst.h"

#include "utility.h"

#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KEY_ADDR(block, table) (block)
#define VALUE_ADDR(block, table) ((char *) (block) + MAX(cc_el_metadata_type_size(table->key_meta), cc_el_metadata_type_size(table->value_meta)))

struct HashTable {
    /* bucket_meta contains metadata to be injected into each bucket. The userdata field contains a pointer that points to this hash table,
     * so that element destructors for each bucket can look at the bucket's metadata, then obtain a pointer to this table,
     * then use the buffers to be able to delete the data properly
     */
    HContainerElementMetaData key_meta, value_meta, bucket_meta;
    size_t capacity, size;

    /* table is an allocated array of bucket pointers (linked lists) that each contain a `void *`.
     * The `void *` points to a block of data containing the following:
     *
     *      +--------------+----------------+
     *      | Raw key data | Raw value data |
     *      +--------------+----------------+
     *
     * These blocks are assigned a custom destructor to be freed automatically when the element is destroyed
     *
     * Iterators point to these raw data blocks.
     *
     * TODO: buckets currently have external metadata (thanks to a change to the linked list allowing that),
     *  but still allocate buffers (C99) internally, which are NOT needed!
     */
    HLinkedList *table;

    /* Constructs on the internal storage, but then can be used as a pointer to another storage block
     * The internal storage must be destructed when destroying the class, by setting the storage pointer to NULL
     * To do this, call `cc_el_destroy_reference()` on the buffers
     */
    HElementData key_buffer, value_buffer, bucket_buffer;
};

/* Creates a block pair for the specified table with the specified key and value
 * `key` must be non-NULL.
 * if `value` is NULL, the value is just default-constructed
 */
static void *create_block_pair(HHashTable table, HConstElementData key, HConstElementData value, unsigned flags)
{
    int fully_initialized = 0;
    void *ptr = MALLOC(MAX(cc_el_metadata_type_size(table->key_meta), cc_el_metadata_type_size(table->value_meta)), 2);

    if (!ptr)
        return NULL;

    /* Copy key */
    *cc_el_storage_location_ptr(table->key_buffer) = KEY_ADDR(ptr, table);
    CC_CLEANUP_ON_ERROR(cc_el_call_constructor_in(table->key_meta, table->key_buffer), goto cleanup;);
    ++fully_initialized;

    if (CC_MOVE_SEMANTICS(flags) == CC_MOVE_VALUE)
        CC_CLEANUP_ON_ERROR(cc_el_move_contents(table->key_buffer, key), goto cleanup;);
    else
        CC_CLEANUP_ON_ERROR(cc_el_copy_contents(table->key_buffer, key), goto cleanup;);

    /* Copy value */
    *cc_el_storage_location_ptr(table->value_buffer) = VALUE_ADDR(ptr, table);
    CC_CLEANUP_ON_ERROR(cc_el_call_constructor_in(table->value_meta, table->value_buffer), goto cleanup;);
    ++fully_initialized;

    if (value)
    {
        if (CC_MOVE_SEMANTICS(flags) == CC_MOVE_VALUE)
            CC_CLEANUP_ON_ERROR(cc_el_move_contents(table->value_buffer, value), goto cleanup;);
        else
            CC_CLEANUP_ON_ERROR(cc_el_copy_contents(table->value_buffer, value), goto cleanup;);
    }

    return ptr;

cleanup:
    if (fully_initialized > 1)
    {
        /* Destruct value */
        *cc_el_storage_location_ptr(table->value_buffer) = VALUE_ADDR(ptr, table);
        cc_el_call_destructor_in(table->value_meta, table->value_buffer);
    }

    if (fully_initialized > 0)
    {
        /* Destruct key */
        *cc_el_storage_location_ptr(table->key_buffer) = KEY_ADDR(ptr, table);
        cc_el_call_destructor_in(table->key_meta, table->key_buffer);
    }

    FREE(ptr);

    return NULL;
}

static int destroy_raw_block_pair(HHashTable table, void *block)
{
    if (block)
    {
        /* Destruct key */
        *cc_el_storage_location_ptr(table->key_buffer) = KEY_ADDR(block, table);
        cc_el_call_destructor_in(table->key_meta, table->key_buffer);

        /* Destruct value */
        *cc_el_storage_location_ptr(table->value_buffer) = VALUE_ADDR(block, table);
        cc_el_call_destructor_in(table->value_meta, table->value_buffer);

        /* Free data block */
        FREE(block);
    }

    return 0;
}

/* Compares `void *` block data to user-specified actual key */
static int compare_block_pair_key(HElementData block_data, HElementData user_key, ElementDualDataCallback compare)
{
    void *pblock_data = *cc_el_get_voidp(block_data);

    HHashTable table = cc_el_userdata_in(cc_el_get_metadata(block_data));

    *cc_el_storage_location_ptr(table->key_buffer) = pblock_data;

    if (compare)
        return compare(table->key_buffer, user_key);
    else
        return cc_el_call_compare_in(table->key_meta, table->key_buffer, user_key);
}

/* Compares `void *` full block data to other `void *` full block data, but they MUST BE IN SEPARATE HASH TABLES!
 * This is because the hash table buffers are used, so there must be one for each block
 */
static int compare_block_pairs_full(HElementData lhs, HElementData rhs)
{
    void *plhs = *cc_el_get_voidp(lhs);
    void *prhs = *cc_el_get_voidp(rhs);

    HHashTable ltable = cc_el_userdata_in(cc_el_get_metadata(lhs));
    HHashTable rtable = cc_el_userdata_in(cc_el_get_metadata(rhs));

    *cc_el_storage_location_ptr(ltable->key_buffer) = KEY_ADDR(plhs, ltable);
    *cc_el_storage_location_ptr(rtable->key_buffer) = KEY_ADDR(prhs, rtable);

    int result = cc_el_call_compare_in(ltable->key_meta, ltable->key_buffer, rtable->key_buffer);
    if (result != 0)
        return result;

    *cc_el_storage_location_ptr(ltable->value_buffer) = VALUE_ADDR(plhs, ltable);
    *cc_el_storage_location_ptr(rtable->value_buffer) = VALUE_ADDR(plhs, ltable);

    return cc_el_call_compare_in(ltable->value_meta, ltable->value_buffer, rtable->value_buffer);
}

static int destroy_block_pair(HElementData data)
{
    void *ptr = *cc_el_get_voidp(data);

    HHashTable table = cc_el_userdata_in(cc_el_get_metadata(data));

    return destroy_raw_block_pair(table, ptr);
}

/* Finds the location of the first key in the bucket with an identical key to `key`, and returns it.
 * The prior element is returned in `*prior`, or `*prior` is NULL if the first element is equal to the key */
static Iterator find_first_key_in_bucket(HHashTable table, HLinkedList bucket, HConstElementData key, ElementDualDataCallback compare, Iterator *prior)
{
    Iterator it, last = NULL;
    for (it = cc_ll_begin(bucket); it; last = it, it = cc_ll_next(bucket, it))
    {
        cc_ll_node_data(bucket, it, table->bucket_buffer);
        int res = compare_block_pair_key(table->bucket_buffer, (HElementData) key, compare);

        if (res == 0)
        {
            if (prior)
                *prior = last;
            return it;
        }
        else if (res < -1 || res > 1)
            break;
    }

    if (prior)
        *prior = last;

    return NULL;
}

/* Counts number of identical keys after the first (designated by `start`) in the bucket */
static size_t count_remaining_keys_in_bucket(HHashTable table, Iterator start, HLinkedList bucket, HConstElementData key, ElementDualDataCallback compare)
{
    size_t count = 0;
    Iterator it;
    for (it = cc_ll_next(bucket, start); it; it = cc_ll_next(bucket, it))
    {
        cc_ll_node_data(bucket, it, table->bucket_buffer);
        int res = compare_block_pair_key(table->bucket_buffer, (HElementData) key, compare);

        if (res == 0)
            ++count;
        else
            break;
    }

    return count;
}

size_t cc_ht_sizeof()
{
    return sizeof(struct HashTable);
}

HHashTable cc_ht_init_with_capacity(ContainerElementType keyType, ContainerElementType valueType, size_t capacity)
{
    HHashTable table = CALLOC(cc_ht_sizeof(), 1);

    if (!table)
        return NULL;

    if (NULL == (table->table = CALLOC(sizeof(*table->table), capacity)) ||
            NULL == (table->key_meta = cc_el_make_metadata(keyType)) ||
            NULL == (table->value_meta = cc_el_make_metadata(valueType)) ||
            NULL == (table->bucket_meta = cc_el_make_metadata(El_VoidPtr)) ||
            NULL == (table->key_buffer = cc_el_init(keyType, table->key_meta, NULL, NULL)) ||
            NULL == (table->value_buffer = cc_el_init(valueType, table->value_meta, NULL, NULL)) ||
            NULL == (table->bucket_buffer = cc_el_init(El_VoidPtr, table->bucket_meta, NULL, NULL)))
        goto cleanup;

    cc_el_set_userdata_in(table->bucket_meta, table);
    cc_el_set_compare_in(table->bucket_meta, compare_block_pairs_full);
    cc_el_set_destructor_in(table->bucket_meta, destroy_block_pair);

    table->capacity = capacity;

    return table;

cleanup:
    cc_el_destroy_reference(table->key_buffer);
    cc_el_destroy_reference(table->value_buffer);
    cc_el_destroy_reference(table->bucket_buffer);
    cc_el_kill_metadata(table->key_meta);
    cc_el_kill_metadata(table->value_meta);
    cc_el_kill_metadata(table->bucket_meta);
    FREE(table->table);
    FREE(table);

    return NULL;
}

HHashTable cc_ht_init(ContainerElementType keyType, ContainerElementType valueType)
{
    return cc_ht_init_with_capacity(keyType, valueType, 31);
}

static HHashTable cc_ht_copy_with_capacity_and_flags(HHashTable table, size_t capacity, unsigned flags)
{
    HHashTable new_table = cc_ht_init_with_capacity(cc_el_metadata_type(table->key_meta), cc_el_metadata_type(table->value_meta), capacity);

    if (!new_table)
        return NULL;

    cc_el_copy_metadata(new_table->key_meta, table->key_meta);
    cc_el_copy_metadata(new_table->value_meta, table->value_meta);

    ExIterator it;
    for (it = cc_ht_begin(table); ExIteratorNonNull(it); it = cc_ht_next(table, it))
    {
        cc_ht_node_key(table, it, table->key_buffer);
        cc_ht_node_data(table, it, table->value_buffer);

        /* TODO: order of elements with identical keys will be backwards; fix? */
        if (CC_OK != cc_ht_insert(new_table, CC_MULTI_VALUE | flags, table->key_buffer, table->value_buffer, NULL))
            goto cleanup;
    }

    new_table->size = table->size;
    return new_table;

cleanup:
    cc_ht_destroy(new_table);
    return NULL;
}

HHashTable cc_ht_copy_with_capacity(HHashTable table, size_t capacity)
{
    return cc_ht_copy_with_capacity_and_flags(table, capacity, 0);
}

HHashTable cc_ht_copy(HHashTable table)
{
    HHashTable new_table = cc_ht_init_with_capacity(cc_el_metadata_type(table->key_meta), cc_el_metadata_type(table->value_meta), table->capacity);

    if (!new_table)
        return NULL;

    cc_el_copy_metadata(new_table->key_meta, table->key_meta);
    cc_el_copy_metadata(new_table->value_meta, table->value_meta);

    size_t idx;
    for (idx = 0; idx < table->capacity; ++idx)
    {
        if (table->table[idx] && NULL == (new_table->table[idx] = cc_ll_copy(table->table[idx], NULL, NULL)))
            goto cleanup;
    }

    new_table->size = table->size;
    return new_table;

cleanup:
    cc_ht_destroy(new_table);
    return NULL;
}

int cc_ht_adjust_load_factor(HHashTable table, float desired_load_factor)
{
    if (desired_load_factor == 0.0)
        CC_BAD_PARAM_HANDLER("invalid load factor specified");
    else if (table->size == 0)
        return CC_OK;

    return cc_ht_adjust_capacity(table, (float) table->size / desired_load_factor);
}

int cc_ht_adjust_capacity(HHashTable table, size_t capacity)
{
    size_t next = next_prime(capacity);
    HHashTable new_table = cc_ht_copy_with_capacity_and_flags(table, MAX(next, capacity), CC_MOVE_VALUE);
    if (!new_table)
        CC_NO_MEM_HANDLER("out of memory");

    cc_ht_swap(table, new_table);
    cc_ht_destroy(new_table);
    return CC_OK;
}

void cc_ht_swap(HHashTable lhs, HHashTable rhs)
{
    struct HashTable table = *lhs;
    *lhs = *rhs;
    *rhs = table;

    /* Fix self-references in metadata */
    cc_el_set_userdata_in(lhs->bucket_meta, lhs);
    cc_el_set_userdata_in(rhs->bucket_meta, rhs);
}

float cc_ht_load_factor(HHashTable table)
{
    return (float) table->size / (float) table->capacity;
}

size_t cc_ht_total_collisions(HHashTable table)
{
    size_t collisions = 0, idx;
    for (idx = 0; idx < table->capacity; ++idx)
        if (table->table[idx])
        {
            size_t size = cc_ll_size_of(table->table[idx]);
            if (size > 1)
                collisions += size;
        }

    return collisions;
}

size_t cc_ht_max_bucket_collisions(HHashTable table)
{
    size_t max = 0, idx;

    for (idx = 0; idx < table->capacity; ++idx)
        if (table->table[idx] && cc_ll_size_of(table->table[idx]) > max)
            max = cc_ll_size_of(table->table[idx]);

    return max;
}

size_t cc_ht_size_of(HHashTable table)
{
    return table->size;
}

size_t cc_ht_capacity_of(HHashTable table)
{
    return table->capacity;
}

int cc_ht_insert(HHashTable table, unsigned flags, HConstElementData key, HConstElementData data, ElementDualDataCallback compare)
{
    int err;

    if (!key)
        CC_BAD_PARAM_HANDLER("no key specified");

    if (!cc_el_compatible_metadata_element(table->key_meta, key))
        CC_TYPE_MISMATCH_HANDLER("cannot insert element, key is wrong type", /*expected*/ cc_el_metadata_type(table->key_meta), /*actual*/ cc_el_type(data));

    if (data && !cc_el_compatible_metadata_element(table->value_meta, data))
        CC_TYPE_MISMATCH_HANDLER("cannot insert element, value is wrong type", /*expected*/ cc_el_metadata_type(table->value_meta), /*actual*/ cc_el_type(data));

    /* First hash the key */
    unsigned hash;
    CC_RETURN_ON_ERROR(cc_el_hash_default((HElementData) key, &hash));

    /* Then reduce modulo the table capacity (note: this doesn't give even distribution unless the capacity is a power of two) */
    hash %= table->capacity;

    /* Then create the bucket, if it doesn't exist already */
    HLinkedList bucket = table->table[hash];
    Iterator list_iter = NULL, prior = NULL;
    if (bucket == NULL)
    {
        bucket = table->table[hash] = cc_ll_init(El_VoidPtr, table->bucket_meta);
        if (!bucket)
            CC_NO_MEM_HANDLER("out of memory");
    }
    /* If bucket already exists, look for our key in it */
    else
        list_iter = find_first_key_in_bucket(table, bucket, key, compare, &prior);

    /* Then create data block itself */
    void *block = create_block_pair(table, key, data, flags);
    if (!block)
        CC_NO_MEM_HANDLER("out of memory");

    /* If multi-value, insert just before identical key
     * This means for multi-value hashes, all keys will be grouped with identical keys (but in undefined order in relation to each other),
     * and the first key of a multi-value group will always be the most recently inserted.
     */
    if (list_iter == NULL || CC_MULTIVALUE(flags) == CC_MULTI_VALUE)
    {
        *cc_el_storage_location_ptr(table->bucket_buffer) = &block;

        CC_CLEANUP_ON_ERROR(cc_ll_insert_after(bucket, CC_COPY_VALUE, prior, table->bucket_buffer, NULL), err = ret; goto cleanup;);

        ++table->size;
    }
    /* Or if single-value, just replace block and delete old block */
    else
    {
        cc_ll_node_data(bucket, list_iter, table->bucket_buffer);

        void *old = *cc_el_get_voidp(table->bucket_buffer);
        *cc_el_get_voidp(table->bucket_buffer) = block;

        destroy_raw_block_pair(table, old);
    }

    if (CC_ORGANIZATION(flags) == CC_ORGANIZE_AUTO)
    {
        if (cc_ht_load_factor(table) > 0.7) /* Greater than 70%, reduce to 60% */
            return cc_ht_adjust_load_factor(table, 0.5);
    }

    return CC_OK;
cleanup:
    destroy_raw_block_pair(table, block);

    return err;
}

int cc_ht_erase(HHashTable table, unsigned flags, HConstElementData key, ElementDualDataCallback compare)
{
    if (!key)
        CC_BAD_PARAM_HANDLER("no key specified");

    if (!cc_el_compatible_metadata_element(table->key_meta, key))
        CC_TYPE_MISMATCH_HANDLER("cannot erase element, key is wrong type", /*expected*/ cc_el_metadata_type(table->key_meta), /*actual*/ cc_el_type(key));

    /* First hash the key */
    unsigned hash;
    CC_RETURN_ON_ERROR(cc_el_hash_default((HElementData) key, &hash));

    /* Then reduce modulo the table capacity (note: this doesn't give even distribution unless the capacity is a power of two) */
    hash %= table->capacity;

    /* Then find the bucket (if it doesn't exist, the element is not present) */
    HLinkedList bucket = table->table[hash];
    Iterator list_iter = NULL, prior = NULL;
    if (bucket == NULL)
        return CC_OK;
    /* If bucket already exists, look for our key in it */
    else
        list_iter = find_first_key_in_bucket(table, bucket, key, compare, &prior);

    if (list_iter == NULL)
        return CC_OK;

    if (CC_MULTIVALUE(flags) == CC_MULTI_VALUE)
    {
        size_t remaining = count_remaining_keys_in_bucket(table, list_iter, bucket, key, compare) + 1;

        table->size -= remaining;
        while (remaining--)
            cc_ll_erase_after(bucket, prior, NULL);
    }
    else
    {
        cc_ll_erase_after(bucket, prior, NULL);
        --table->size;
    }

    return CC_OK;
}

/* Iterator format
 *
 *      opaque[0] points to block pair itself
 *      opaque[1] is linked list iterator of block pair
 *      opaque[2] points to HLinkedList entry in table
 */
ExIterator cc_ht_begin(HHashTable table)
{
    size_t idx;

    for (idx = 0; idx < table->capacity; ++idx)
    {
        HLinkedList list = table->table[idx];
        if (list && cc_ll_size_of(list))
        {
            ExIterator result = cc_el_null_ex_iterator();
            Iterator list_begin = cc_ll_begin(list);

            cc_ll_node_data(list, list_begin, table->bucket_buffer);

            result.opaque[0] = *cc_el_get_voidp(table->bucket_buffer);
            result.opaque[1] = list_begin;
            result.opaque[2] = table->table + idx;

            return result;
        }
    }

    return cc_el_null_ex_iterator();
}

ExIterator cc_ht_next(HHashTable table, ExIterator node)
{
    size_t idx = (HLinkedList *) node.opaque[2] - table->table;
    HLinkedList list = table->table[idx];

    /* Check remainder of bucket */
    Iterator list_iter = cc_ll_next(list, node.opaque[1]);

    if (list_iter)
    {
        cc_ll_node_data(list, list_iter, table->bucket_buffer);

        node.opaque[0] = *cc_el_get_voidp(table->bucket_buffer);
        node.opaque[1] = list_iter;

        return node;
    }

    /* Bucket is complete, move on to next hash entry */
    for (++idx; idx < table->capacity; ++idx)
    {
        list = table->table[idx];
        if (list && cc_ll_size_of(list))
        {
            list_iter = cc_ll_begin(list);

            cc_ll_node_data(list, list_iter, table->bucket_buffer);

            node.opaque[0] = *cc_el_get_voidp(table->bucket_buffer);
            node.opaque[1] = list_iter;
            node.opaque[2] = table->table + idx;

            return node;
        }
    }

    return cc_el_null_ex_iterator();
}

int cc_ht_find(HHashTable table, HConstElementData key, ElementDualDataCallback compare, ExIterator *out)
{
    /* Take hash of key */
    unsigned hash;
    CC_RETURN_ON_ERROR(cc_el_hash_default((HElementData) key, &hash));

    /* Modulo key by table capacity */
    hash %= table->capacity;

    /* Then find location in bucket */
    HLinkedList bucket = table->table[hash];
    if (!bucket || !cc_ll_size_of(bucket)) /* Bucket doesn't exist or is empty */
        goto null;

    Iterator prior;
    Iterator list_iter = find_first_key_in_bucket(table, bucket, key, compare, &prior);

    if (!list_iter)
        goto null;

    /* Then build iterator */
    cc_ll_node_data(bucket, list_iter, table->bucket_buffer);

    out->opaque[0] = *cc_el_get_voidp(table->bucket_buffer);
    out->opaque[1] = list_iter;
    out->opaque[2] = table->table + hash;

    return CC_OK;

null:
    *out = cc_el_null_ex_iterator();
    return CC_OK;
}

int cc_ht_iterate(HHashTable table, ExtendedElementDualDataCallback callback, void *userdata)
{
    ExIterator it;
    for (it = cc_ht_begin(table); ExIteratorNonNull(it); it = cc_ht_next(table, it))
    {
        cc_ht_node_key(table, it, table->key_buffer);
        cc_ht_node_data(table, it, table->value_buffer);

        CC_RETURN_ON_ERROR(callback(table->key_buffer, table->value_buffer, userdata));
    }

    return CC_OK;
}

int cc_ht_node_key(HHashTable table, ExIterator node, HElementData out)
{
    if (!cc_el_compatible_metadata_element(table->key_meta, out))
        CC_TYPE_MISMATCH_HANDLER("cannot get element", /*expected*/ cc_el_metadata_type(table->key_meta), /*actual*/ cc_el_type(out));

    *cc_el_storage_location_ptr(out) = KEY_ADDR(node.opaque[0], table);

    return CC_OK;
}

int cc_ht_node_data(HHashTable table, ExIterator node, HElementData out)
{
    if (!cc_el_compatible_metadata_element(table->value_meta, out))
        CC_TYPE_MISMATCH_HANDLER("cannot get element", /*expected*/ cc_el_metadata_type(table->value_meta), /*actual*/ cc_el_type(out));

    *cc_el_storage_location_ptr(out) = VALUE_ADDR(node.opaque[0], table);

    return CC_OK;
}

HContainerElementMetaData cc_ht_key_metadata(HHashTable table)
{
    return table->key_meta;
}

HContainerElementMetaData cc_ht_value_metadata(HHashTable table)
{
    return table->value_meta;
}

int cc_ht_compare(HHashTable lhs, HHashTable rhs, ElementDualDataCallback cmp)
{


    return 0;
}

void cc_ht_clear(HHashTable table)
{
    size_t idx;

    for (idx = 0; idx < table->capacity; ++idx)
    {
        if (table->table[idx])
        {
            cc_ll_destroy(table->table[idx], NULL);
            table->table[idx] = NULL;
        }
    }

    table->size = 0;
}

void cc_ht_destroy(HHashTable table)
{
    cc_ht_clear(table);

    cc_el_destroy_reference(table->key_buffer);
    cc_el_destroy_reference(table->value_buffer);
    cc_el_destroy_reference(table->bucket_buffer);
    cc_el_kill_metadata(table->key_meta);
    cc_el_kill_metadata(table->value_meta);
    cc_el_kill_metadata(table->bucket_meta);
    FREE(table->table);
    FREE(table);
}

#ifdef __cplusplus
}
#endif
