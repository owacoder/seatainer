#include "cchash.h"

#include "cclnklst.h"

#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

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
static void *create_block_pair(HHashTable table, HConstElementData key, HConstElementData value)
{
    int fully_initialized = 0;
    void *ptr = MALLOC(MAX(cc_el_metadata_type_size(table->key_meta), cc_el_metadata_type_size(table->value_meta)), 2);

    if (!ptr)
        return NULL;

    /* Copy key */
    *cc_el_storage_location_ptr(table->key_buffer) = ptr;
    CC_CLEANUP_ON_ERROR(cc_el_call_constructor_in(table->key_meta, table->key_buffer), goto cleanup;);
    ++fully_initialized;

    CC_CLEANUP_ON_ERROR(cc_el_copy_contents(table->key_buffer, key), goto cleanup;);

    /* Copy value */
    *cc_el_storage_location_ptr(table->value_buffer) = (char *) ptr + MAX(cc_el_metadata_type_size(table->key_meta), cc_el_metadata_type_size(table->value_meta));
    CC_CLEANUP_ON_ERROR(cc_el_call_constructor_in(table->value_meta, table->value_buffer), goto cleanup;);
    ++fully_initialized;

    if (value)
        CC_CLEANUP_ON_ERROR(cc_el_copy_contents(table->value_buffer, value), goto cleanup;);

    return ptr;

cleanup:
    if (fully_initialized > 1)
    {
        /* Destruct value */
        *cc_el_storage_location_ptr(table->value_buffer) = (char *) ptr + MAX(cc_el_metadata_type_size(table->key_meta), cc_el_metadata_type_size(table->value_meta));
        cc_el_call_destructor_in(table->value_meta, table->value_buffer);
    }

    if (fully_initialized > 0)
    {
        /* Destruct key */
        *cc_el_storage_location_ptr(table->key_buffer) = ptr;
        cc_el_call_destructor_in(table->key_meta, table->key_buffer);
    }

    FREE(ptr);

    return NULL;
}

static int destroy_raw_block_pair(HHashTable table, void *block)
{
    /* Destruct key */
    *cc_el_storage_location_ptr(table->key_buffer) = block;
    cc_el_call_destructor_in(table->key_meta, table->key_buffer);

    /* Destruct value */
    *cc_el_storage_location_ptr(table->value_buffer) = block + cc_el_metadata_type_size(table->key_meta);
    cc_el_call_destructor_in(table->value_meta, table->value_buffer);

    /* Free data block */
    FREE(block);

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

/* Compares `void *` key block data to other `void *` key block data, but they MUST BE IN SEPARATE HASH TABLES!
 * This is because the hash table key buffer is used, so there must be one for each block
 */
static int compare_block_pairs_keys_only(HElementData lhs, HElementData rhs)
{
    void *plhs = *cc_el_get_voidp(lhs);
    void *prhs = *cc_el_get_voidp(rhs);

    HHashTable ltable = cc_el_userdata_in(cc_el_get_metadata(lhs));
    HHashTable rtable = cc_el_userdata_in(cc_el_get_metadata(rhs));

    *cc_el_storage_location_ptr(ltable->key_buffer) = plhs;
    *cc_el_storage_location_ptr(rtable->key_buffer) = prhs;

    return cc_el_call_compare_in(ltable->key_meta, ltable->key_buffer, rtable->key_buffer);
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

    *cc_el_storage_location_ptr(ltable->key_buffer) = plhs;
    *cc_el_storage_location_ptr(rtable->key_buffer) = prhs;

    int result = cc_el_call_compare_in(ltable->key_meta, ltable->key_buffer, rtable->key_buffer);
    if (result != 0)
        return result;

    *cc_el_storage_location_ptr(ltable->value_buffer) = (char *) plhs + MAX(cc_el_metadata_type_size(ltable->key_meta), cc_el_metadata_type_size(ltable->value_meta));
    *cc_el_storage_location_ptr(rtable->value_buffer) = (char *) prhs + MAX(cc_el_metadata_type_size(rtable->key_meta), cc_el_metadata_type_size(rtable->value_meta));

    return cc_el_call_compare_in(ltable->value_meta, ltable->value_buffer, rtable->value_buffer);
}

static int destroy_block_pair(HElementData data)
{
    void *ptr = *cc_el_get_voidp(data);

    HHashTable table = cc_el_userdata_in(cc_el_get_metadata(data));

    return destroy_raw_block_pair(table, ptr);
}

static Iterator find_key_in_bucket(HHashTable table, HLinkedList bucket, Iterator start, HConstElementData key, ElementDualDataCallback compare)
{
    Iterator it;
    for (it = start; it; it = cc_ll_next(bucket, it))
    {
        cc_ll_node_data(bucket, it, table->bucket_buffer);
        int res = compare_block_pair_key(table->bucket_buffer, key, compare);

        if (res == 0)
            return it;
        else if (res < -1 || res > 1)
            break;
    }

    return NULL;
}

static HHashTable cc_ht_init_with_table_size(ContainerElementType keyType, ContainerElementType valueType, size_t table_size)
{
    HHashTable table = CALLOC(sizeof(*table), 1);

    if (!table)
        return NULL;

    if (NULL == (table->table = CALLOC(sizeof(*table->table), table_size)) ||
            NULL == (table->key_meta = cc_el_make_metadata(keyType)) ||
            NULL == (table->value_meta = cc_el_make_metadata(valueType)) ||
            NULL == (table->bucket_meta = cc_el_make_metadata(El_VoidPtr)) ||
            NULL == (table->key_buffer = cc_el_init(keyType, table->key_meta, NULL, NULL)) ||
            NULL == (table->value_buffer = cc_el_init(valueType, table->value_meta, NULL, NULL)) ||
            NULL == (table->bucket_buffer = cc_el_init(El_VoidPtr, table->bucket_meta, NULL, NULL)))
        goto cleanup;

    cc_el_set_compare_in(table->bucket_meta, compare_block_pairs_full);
    cc_el_set_destructor_in(table->bucket_meta, destroy_block_pair);

    table->capacity = table_size;

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
    return cc_ht_init_with_table_size(keyType, valueType, 512);
}

static HHashTable cc_ht_copy_with_table_size(HHashTable table, size_t table_size)
{
    HHashTable new_table = cc_ht_init_with_table_size(cc_el_metadata_type(table->key_meta), cc_el_metadata_type(table->value_meta), table_size);

    if (!new_table)
        return NULL;

    cc_el_copy_metadata(new_table->key_meta, table->key_meta);
    cc_el_copy_metadata(new_table->value_meta, table->value_meta);
}

HHashTable cc_ht_copy(HHashTable table, ElementDataCallback construct, ElementDataCallback destruct)
{
    return NULL;
}

void cc_ht_swap(HHashTable lhs, HHashTable rhs)
{
    struct HashTable table = *lhs;
    *lhs = *rhs;
    *rhs = table;
}

float cc_ht_load_factor(HHashTable table)
{
    return (float) table->size / (float) table->capacity;
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
    Iterator list_iter = NULL;
    if (bucket == NULL)
    {
        bucket = table->table[hash] = cc_ll_init(El_VoidPtr);
        if (!bucket)
            CC_NO_MEM_HANDLER("out of memory");

        /* Imbue metadata on new bucket so deletions work automatically */
        cc_el_copy_metadata(cc_ll_metadata(bucket), table->bucket_meta);
    }
    /* If bucket already exists, look for our key in it */
    else
        list_iter = find_key_in_bucket(table, bucket, cc_ll_begin(bucket), key, compare);

    /* Then create data block itself */
    void *block = create_block_pair(table, key, data);
    if (!block)
        CC_NO_MEM_HANDLER("out of memory");

    /* If multi-value, insert just after first found identical key (for ease of insertion) */
    if (CC_MULTIVALUE(flags) == CC_MULTI_VALUE)
    {
        *cc_el_storage_location_ptr(table->bucket_buffer) = &block;

        CC_CLEANUP_ON_ERROR(cc_ll_insert_after(bucket, list_iter, table->bucket_buffer, NULL), err = ret; goto cleanup;);
    }
    /* Or if single-value, just replace block and delete old block */
    else
    {
        cc_ll_node_data(bucket, list_iter, table->bucket_buffer);

        void *old = *cc_el_get_voidp(table->bucket_buffer);
        *cc_el_get_voidp(table->bucket_buffer) = block;

        destroy_raw_block_pair(table, old);
    }

    return CC_OK;
cleanup:
    destroy_raw_block_pair(table, block);

    return err;
}

int cc_ht_erase(HHashTable table, unsigned flags, HConstElementData data, ElementDataCallback destruct)
{
    return 0;
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
        }
    }

    return cc_el_null_ex_iterator();
}

ExIterator cc_ht_next(HHashTable table, ExIterator node)
{
    size_t idx = (HLinkedList *) node.opaque[1] - table->table;
    HLinkedList list = table->table[idx];

    /* Check remainder of bucket */
    Iterator list_iter = cc_ll_next(list, node.opaque[2]);

    if (list_iter)
    {
        cc_ll_node_data(list, list_iter, table->bucket_buffer);

        node.opaque[0] = *cc_el_get_voidp(table->bucket_buffer);
        node.opaque[1] = list_iter;

        return node;
    }

    /* Bucket is complete, move on to next hash entry */
    for (; idx < table->capacity; ++idx)
    {
        list = table->table[idx];
        if (list && cc_ll_size_of(list))
        {
            list_iter = cc_ll_begin(list);

            cc_ll_node_data(list, list_iter, table->bucket_buffer);

            node.opaque[0] = *cc_el_get_voidp(table->bucket_buffer);
            node.opaque[1] = list_iter;
            node.opaque[2] = table->table + idx;
        }
    }

    return cc_el_null_ex_iterator();
}

int cc_ht_find(HHashTable table, Iterator start, unsigned flags, HConstElementData key, ElementDualDataCallback compare, Iterator *out)
{


    return 0;
}

int cc_ht_iterate(HHashTable table, ExtendedElementDualDataCallback callback, void *userdata)
{
    return 0;
}

int cc_ht_node_key(HHashTable table, ExIterator node, HElementData out)
{
    if (!cc_el_compatible_metadata_element(table->key_meta, out))
        CC_TYPE_MISMATCH_HANDLER("cannot get element", /*expected*/ cc_el_metadata_type(table->key_meta), /*actual*/ cc_el_type(out));

    *cc_el_storage_location_ptr(out) = node.opaque[0];

    return CC_OK;
}

int cc_ht_node_data(HHashTable table, ExIterator node, HElementData out)
{
    if (!cc_el_compatible_metadata_element(table->value_meta, out))
        CC_TYPE_MISMATCH_HANDLER("cannot get element", /*expected*/ cc_el_metadata_type(table->value_meta), /*actual*/ cc_el_type(out));

    *cc_el_storage_location_ptr(out) = (char *) node.opaque[0] + MAX(cc_el_metadata_type_size(table->key_meta), cc_el_metadata_type_size(table->value_meta));

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
    cc_el_set_compare_in(lhs->bucket_meta, compare_block_pairs_full);



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
