#include "cchash.h"

#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Hash table layout:
 *
 *    MAIN      LINKED
 *   VECTOR      LIST
 *               CHAIN
 *      |          |
 *      v          v
 *
 *   +-----+    +-----+    +-----+
 *   |     |    |     |    |     |
 *   |LIST*| -> |VALUE| -> |VALUE|
 *   |     |    |     |    |     |
 *   +-----+    +-----+    +-----+
 *   |     |    |     |    |     |
 *   |LIST*| -> |VALUE| -> |VALUE|
 *   |     |    |     |    |     |
 *   +-----+    +-----+    +-----+
 *   |     |    |     |    |     |
 *   |LIST*| -> |VALUE| -> |VALUE|
 *   |     |    |     |    |     |
 *   +-----+    +-----+    +-----+
 *   |     |    |     |    |     |
 *   |LIST*| -> |VALUE| -> |VALUE|
 *   |     |    |     |    |     |
 *   +-----+    +-----+    +-----+
 */

struct HashTable {
    size_t capacity;


    /* Constructs on the internal storage, but then can be used as a pointer to another storage block
     * The internal storage must be destructed when destroying the class, by setting the storage pointer to NULL
     * To do this, call `cc_el_destroy_reference()` on the buffers
     */
    HElementData key_buffer, data_buffer;
};

HHashTable cc_ht_init(ContainerElementType keyType, ContainerElementType valueType)
{
    return NULL;
}

HHashTable cc_ht_copy(HHashTable table, ElementDataCallback construct, ElementDataCallback destruct)
{
    return NULL;
}

void cc_ht_swap(HHashTable lhs, HHashTable rhs)
{

}

size_t cc_ht_size_of(HHashTable table)
{
    return 0;
}

size_t cc_ht_capacity_of(HHashTable table)
{
    return 0;
}

int cc_ht_insert(HHashTable table, HConstElementData data, ElementDataCallback construct)
{
    return 0;
}

int cc_ht_erase(HHashTable table, HConstElementData data, ElementDataCallback destruct)
{
    return 0;
}

Iterator cc_ht_begin(HHashTable table)
{
    return NULL;
}

Iterator cc_ht_next(HHashTable table, Iterator node)
{
    return NULL;
}

int cc_ht_find(HHashTable table, Iterator start, unsigned flags, HConstElementData data, ElementDualDataCallback compare, Iterator *out)
{
    return 0;
}

int cc_ht_iterate(HHashTable table, ExtendedElementDualDataCallback callback, void *userdata)
{
    return 0;
}

int cc_ht_node_key(HHashTable table, Iterator element, HElementData out)
{
    return 0;
}

int cc_ht_node_data(HHashTable table, Iterator element, HElementData out)
{
    return 0;
}

HContainerElementMetaData cc_ht_key_metadata(HHashTable table)
{
    return NULL;
}

HContainerElementMetaData cc_ht_value_metadata(HHashTable table)
{
    return NULL;
}

int cc_ht_compare(HHashTable lhs, HHashTable rhs, ElementDualDataCallback cmp)
{
    return 0;
}

void cc_ht_clear(HHashTable table, ElementDataCallback destruct)
{

}

void cc_ht_destroy(HHashTable table, ElementDataCallback destruct)
{

}

#ifdef __cplusplus
}
#endif
