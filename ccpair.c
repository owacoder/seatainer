#include "ccpair.h"

#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 0

struct Pair {
    HContainerElementMetaData key_meta, value_meta;

    char *data; /* Contains the raw key data immediately followed by the raw value data */

    /* Constructs on the internal storage, but then can be used as a pointer to another storage block
     * The internal storage must be destructed when destroying the class, by setting the storage pointer to NULL
     * To do this, call `cc_el_destroy_reference()` on the buffer
     */
    HElementData buffer;
};

HPair cc_p_init(ContainerElementType keyType, ContainerElementType valueType)
{
    HPair result = MALLOC(sizeof(*result), 1);

    if (!result)
        return NULL;

    result->key_meta = result->value_meta = NULL;

    result->buffer = cc_el_init(El_Null, NULL, NULL, NULL);
    if (!result->buffer) goto cleanup;

    result->key_meta = cc_el_make_metadata(keyType);
    if (!result->key_meta) goto cleanup;

    result->value_meta = cc_el_make_metadata(valueType);
    if (!result->value_meta) goto cleanup;

    result->data = NULL;

    return result;

cleanup:
    cc_el_destroy(result->buffer);
    cc_el_kill_metadata(result->key_meta);
    cc_el_kill_metadata(result->value_meta);
    FREE(result);
    return NULL;
}

HPair cc_p_copy(HPair pair)
{
    int fully_constructed = 0;

    HPair new_pair = cc_p_init(cc_el_metadata_type(pair->key_meta), cc_el_metadata_type(pair->value_meta));
    if (!new_pair)
        return NULL;

    cc_el_copy_metadata(new_pair->key_meta, pair->key_meta);
    cc_el_copy_metadata(new_pair->value_meta, pair->value_meta);

    if (pair->data == NULL)
        return new_pair;
    else
    {
        pair->data = MALLOC(cc_el_metadata_type_size(pair->key_meta) + cc_el_metadata_type_size(pair->value_meta), 1);
        if (pair->data == NULL)
            goto cleanup;
    }

    /* Set storage location (key) */
    *cc_el_storage_location_ptr(new_pair->buffer) = new_pair->data;
    *cc_el_storage_location_ptr(pair->buffer) = pair->data;

    /* Construct new element (key) */
    CC_CLEANUP_ON_ERROR(cc_el_call_constructor_in(new_pair->key_meta, new_pair->buffer), goto cleanup;);
    ++fully_constructed;

    /* Copy contents (key) */
    CC_CLEANUP_ON_ERROR(cc_el_copy_contents(new_pair->buffer, pair->buffer), goto cleanup;);

    /* Set storage location (value) */
    *cc_el_storage_location_ptr(new_pair->buffer) = new_pair->data + cc_el_metadata_type_size(new_pair->key_meta);
    *cc_el_storage_location_ptr(pair->buffer) = pair->data + cc_el_metadata_type_size(pair->key_meta);

    /* Construct new element (value) */
    CC_CLEANUP_ON_ERROR(cc_el_call_constructor_in(new_pair->key_meta, new_pair->buffer), goto cleanup;);
    ++fully_constructed;

    /* Copy contents (value) */
    CC_CLEANUP_ON_ERROR(cc_el_copy_contents(new_pair->buffer, pair->buffer), goto cleanup;);

    return new_pair;

cleanup:
    /* Destroy value */
    if (fully_constructed > 1)
    {
        *cc_el_storage_location_ptr(new_pair->buffer) = pair->data + cc_el_metadata_type_size(new_pair->key_meta);
        if (cc_el_destructor_in(new_pair->value_meta))
            cc_el_call_destructor_in(new_pair->value_meta, new_pair->buffer);
    }

    /* Destroy key */
    if (fully_constructed > 0)
    {
        *cc_el_storage_location_ptr(new_pair->buffer) = pair->data;
        if (cc_el_destructor_in(new_pair->key_meta))
            cc_el_call_destructor_in(new_pair->key_meta, new_pair->buffer);
    }

    cc_p_destroy(new_pair);
    return NULL;
}

#endif

#ifdef __cplusplus
}
#endif
