/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "element.h"
#include "ccvector.h"
#include "cclnklst.h"
#include "ccdbllst.h"
#include "cchash.h"
#include "ccstring.h"

#include "utility.h"

#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *cc_el_error_reason(int error)
{
    switch (error)
    {
        case CC_LESS_THAN: return "Less-than";
        case CC_OK: return "No error";
        case CC_GREATER_THAN: return "Greater-than";
        case CC_FAILURE: return "Operation failed";
        case CC_NO_SUCH_METHOD: return "No valid method found";
        case CC_NO_MEM: return "Out of memory";
        case CC_BAD_PARAM: return "Bad parameter passed to function";
        case CC_TYPE_MISMATCH: return "Type mismatch";
        default: return "Unknown error";
    }
}

Iterator cc_el_null_iterator()
{
    return NULL;
}

ExIterator cc_el_null_ex_iterator()
{
    ExIterator it;
    memset(it.opaque, 0, sizeof(it.opaque));
    return it;
}

ExIterator cc_el_ex_iterator_from_iterator(Iterator iterator)
{
    ExIterator it = cc_el_null_ex_iterator();
    it.opaque[0] = iterator;
    return it;
}

int ExIteratorEquals(ExIterator lhs, ExIterator rhs)
{
    return !memcmp(lhs.opaque, rhs.opaque, sizeof(lhs.opaque));
}

int ExIteratorNonNull(ExIterator it)
{
    return it.opaque[0] != NULL;
}

struct ElementData {
    /* Actual internal data is stored in the union */
    union {
        uintmax_t _uint;
        intmax_t _int;
        double _ldbl;
        void *_ptr;
    } data;
    /* A reference to the actual data location if external, or NULL if using internal data
     * Constructors, copy-constructors, or destructors must _NEVER_ change this parameter via cc_el_storage_location_ptr()
     */
    void *_src;
    /* The metadata used for this element type (externally owned)
     * If `meta` is NULL, then the `type` member is used for type deduction,
     * and standard type rules (construction, copying, and destruction) apply
     * If `meta` is non-NULL, then the `meta` member is used for all type information
     * `meta` may be set to NULL if non-NULL during a cc_el_assign_xxx() function call, changing the effective type
     */
    HContainerElementMetaData meta;
    ContainerElementType type;
};

void *cc_el_storage_location(HElementData data)
{
    return data->_src? data->_src: &data->data;
}

void **cc_el_storage_location_ptr(HElementData data)
{
    return &data->_src;
}

int cc_el_is_external_reference(HConstElementData data)
{
    return data->_src != NULL;
}

void cc_el_clear_external_reference(HElementData data)
{
    data->_src = NULL;
}

struct ContainerElementMetaData
{
    ElementDataCallback el_constructor;
    ElementDualDataCallback el_copy_constructor;
    ElementDataCallback el_destructor;
    ElementDualDataCallback el_compare;
    size_t el_size; /* Size in bytes of contained elements */
    ContainerElementType el_type; /* Type of contained elements */
    void *el_userdata; /* Userdata, allowed to be used for anything */
};

static int cc_el_char_constructor(HElementData data)
{
    char *p = (char *) (cc_el_storage_location(data));
    *p = 0;
    return CC_OK;
}
static int cc_el_signed_char_constructor(HElementData data)
{
    signed char *p = (signed char *) (cc_el_storage_location(data));
    *p = 0;
    return CC_OK;
}
static int cc_el_unsigned_char_constructor(HElementData data)
{
    unsigned char *p = (unsigned char *) (cc_el_storage_location(data));
    *p = 0;
    return CC_OK;
}
static int cc_el_signed_short_constructor(HElementData data)
{
    signed short *p = (signed short *) (cc_el_storage_location(data));
    *p = 0;
    return CC_OK;
}
static int cc_el_unsigned_short_constructor(HElementData data)
{
    unsigned short *p = (unsigned short *) (cc_el_storage_location(data));
    *p = 0;
    return CC_OK;
}
static int cc_el_signed_int_constructor(HElementData data)
{
    signed int *p = (signed int *) (cc_el_storage_location(data));
    *p = 0;
    return CC_OK;
}
static int cc_el_unsigned_int_constructor(HElementData data)
{
    unsigned int *p = (unsigned int *) (cc_el_storage_location(data));
    *p = 0;
    return CC_OK;
}
static int cc_el_signed_long_constructor(HElementData data)
{
    signed long *p = (signed long *) (cc_el_storage_location(data));
    *p = 0;
    return CC_OK;
}
static int cc_el_unsigned_long_constructor(HElementData data)
{
    unsigned long *p = (unsigned long *) (cc_el_storage_location(data));
    *p = 0;
    return CC_OK;
}
static int cc_el_signed_long_long_constructor(HElementData data)
{
    signed long long *p = (signed long long *) (cc_el_storage_location(data));
    *p = 0;
    return CC_OK;
}
static int cc_el_unsigned_long_long_constructor(HElementData data)
{
    unsigned long long *p = (unsigned long long *) (cc_el_storage_location(data));
    *p = 0;
    return CC_OK;
}
static int cc_el_float_constructor(HElementData data)
{
    float *p = (float *) (cc_el_storage_location(data));
    *p = 0;
    return CC_OK;
}
static int cc_el_double_constructor(HElementData data)
{
    double *p = (double *) (cc_el_storage_location(data));
    *p = 0;
    return CC_OK;
}
static int cc_el_voidp_constructor(HElementData data)
{
    void **p = (void **) (cc_el_storage_location(data));
    *p = 0;
    return CC_OK;
}
static int cc_el_string_constructor(HElementData data)
{
    if (data->_src) /* External, means container already exists */
        return cc_s_init_at(data->_src, cc_s_sizeof());

    return cc_el_voidp_constructor(data);
}
static int cc_el_vector_constructor(HElementData data)
{
    if (data->_src) /* External, means container already exists */
        return cc_v_init_at(data->_src, cc_v_sizeof(), El_Null); /* TODO: figure out how to change type of vector data to something different after initial construction? */

    return cc_el_voidp_constructor(data);
}
static int cc_el_linked_list_constructor(HElementData data)
{
    if (data->_src) /* External, means container already exists */
        return cc_ll_init_at(data->_src, cc_ll_sizeof(), El_Null, NULL); /* TODO: figure out how to change type of list data to something different after initial construction? */

    return cc_el_voidp_constructor(data);
}
static int cc_el_doubly_linked_list_constructor(HElementData data)
{
    if (data->_src) /* External, means container already exists */
        return cc_dll_init_at(data->_src, cc_dll_sizeof(), El_Null); /* TODO: figure out how to change type of list data to something different after initial construction? */

    return cc_el_voidp_constructor(data);
}
static int cc_el_hash_table_constructor(HElementData data)
{
    if (data->_src) /* External, means container already exists */
        return cc_ht_init_at(data->_src, cc_ht_sizeof(), El_Null, El_Null); /* TODO: figure out how to change type of vector data to something different after initial construction? */

    return cc_el_voidp_constructor(data);
}

static int cc_el_char_copy_constructor(HElementData lhs, HElementData rhs)
{
    char *l = (char *) (cc_el_storage_location(lhs));
    char *r = (char *) (cc_el_storage_location(rhs));

    *l = *r;
    return CC_OK;
}

static int cc_el_signed_char_copy_constructor(HElementData lhs, HElementData rhs)
{
    signed char *l = (signed char *) (cc_el_storage_location(lhs));
    signed char *r = (signed char *) (cc_el_storage_location(rhs));

    *l = *r;
    return CC_OK;
}

static int cc_el_unsigned_char_copy_constructor(HElementData lhs, HElementData rhs)
{
    unsigned char *l = (unsigned char *) (cc_el_storage_location(lhs));
    unsigned char *r = (unsigned char *) (cc_el_storage_location(rhs));

    *l = *r;
    return CC_OK;
}

static int cc_el_signed_short_copy_constructor(HElementData lhs, HElementData rhs)
{
    signed short *l = (signed short *) (cc_el_storage_location(lhs));
    signed short *r = (signed short *) (cc_el_storage_location(rhs));

    *l = *r;
    return CC_OK;
}

static int cc_el_unsigned_short_copy_constructor(HElementData lhs, HElementData rhs)
{
    unsigned short *l = (unsigned short *) (cc_el_storage_location(lhs));
    unsigned short *r = (unsigned short *) (cc_el_storage_location(rhs));

    *l = *r;
    return CC_OK;
}

static int cc_el_signed_int_copy_constructor(HElementData lhs, HElementData rhs)
{
    signed int *l = (signed int *) (cc_el_storage_location(lhs));
    signed int *r = (signed int *) (cc_el_storage_location(rhs));

    *l = *r;
    return CC_OK;
}

static int cc_el_unsigned_int_copy_constructor(HElementData lhs, HElementData rhs)
{
    unsigned int *l = (unsigned int *) (cc_el_storage_location(lhs));
    unsigned int *r = (unsigned int *) (cc_el_storage_location(rhs));

    *l = *r;
    return CC_OK;
}

static int cc_el_signed_long_copy_constructor(HElementData lhs, HElementData rhs)
{
    signed long *l = (signed long *) (cc_el_storage_location(lhs));
    signed long *r = (signed long *) (cc_el_storage_location(rhs));

    *l = *r;
    return CC_OK;
}

static int cc_el_unsigned_long_copy_constructor(HElementData lhs, HElementData rhs)
{
    unsigned long *l = (unsigned long *) (cc_el_storage_location(lhs));
    unsigned long *r = (unsigned long *) (cc_el_storage_location(rhs));

    *l = *r;
    return CC_OK;
}

static int cc_el_signed_long_long_copy_constructor(HElementData lhs, HElementData rhs)
{
    signed long long *l = (signed long long *) (cc_el_storage_location(lhs));
    signed long long *r = (signed long long *) (cc_el_storage_location(rhs));

    *l = *r;
    return CC_OK;
}

static int cc_el_unsigned_long_long_copy_constructor(HElementData lhs, HElementData rhs)
{
    unsigned long long *l = (unsigned long long *) (cc_el_storage_location(lhs));
    unsigned long long *r = (unsigned long long *) (cc_el_storage_location(rhs));

    *l = *r;
    return CC_OK;
}

static int cc_el_float_copy_constructor(HElementData lhs, HElementData rhs)
{
    float *l = (float *) (cc_el_storage_location(lhs));
    float *r = (float *) (cc_el_storage_location(rhs));

    *l = *r;
    return CC_OK;
}

static int cc_el_double_copy_constructor(HElementData lhs, HElementData rhs)
{
    double *l = (double *) (cc_el_storage_location(lhs));
    double *r = (double *) (cc_el_storage_location(rhs));

    *l = *r;
    return CC_OK;
}

static int cc_el_voidp_copy_constructor(HElementData lhs, HElementData rhs)
{
    void **l = cc_el_storage_location(lhs);
    void **r = cc_el_storage_location(rhs);

    *l = *r;
    return CC_OK;
}

static int cc_el_string_copy_constructor(HElementData lhs, HElementData rhs)
{
    HString *l = cc_el_get_string(lhs);
    HString *r = cc_el_get_string(rhs);

    if (lhs->_src) /* External data, so container must already exist */
        return cc_s_assign_cstring_n(*l, cc_s_raw(*r), cc_s_size(*r));
    else
    {
        if (*l)
            cc_s_destroy(*l);

        *l = cc_s_copy(*r);
        if (!*l)
            CC_NO_MEM_HANDLER("out of memory");
    }

    return CC_OK;
}

static int cc_el_vector_copy_constructor(HElementData lhs, HElementData rhs)
{
    HVector *l = cc_el_get_vector(lhs);
    HVector *r = cc_el_get_vector(rhs);

    if (lhs->_src) /* External data, so container must already exist */
        return cc_v_assign(*l, *r);
    else
    {
        if (*l)
            cc_v_destroy(*l, NULL);

        *l = cc_v_copy(*r, NULL, NULL);
        if (!*l)
            CC_NO_MEM_HANDLER("out of memory");
    }

    return CC_OK;
}

static int cc_el_linked_list_copy_constructor(HElementData lhs, HElementData rhs)
{
    HLinkedList *l = cc_el_get_linked_list(lhs);
    HLinkedList *r = cc_el_get_linked_list(rhs);

    if (lhs->_src) /* External data, so container must already exist */
        return cc_ll_assign(*l, *r);
    else
    {
        if (*l)
            cc_ll_destroy(*l, NULL);

        *l = cc_ll_copy(*r, NULL, NULL, NULL);
        if (!*l)
            CC_NO_MEM_HANDLER("out of memory");
    }

    return CC_OK;
}

static int cc_el_doubly_linked_list_copy_constructor(HElementData lhs, HElementData rhs)
{
    HDoublyLinkedList *l = cc_el_get_doubly_linked_list(lhs);
    HDoublyLinkedList *r = cc_el_get_doubly_linked_list(rhs);

    if (lhs->_src) /* External data, so container must already exist */
        return cc_dll_assign(*l, *r);
    else
    {
        if (*l)
            cc_dll_destroy(*l, NULL);

        *l = cc_dll_copy(*r, NULL, NULL);
        if (!*l)
            CC_NO_MEM_HANDLER("out of memory");
    }

    return CC_OK;
}

static int cc_el_hash_table_copy_constructor(HElementData lhs, HElementData rhs)
{
    HHashTable *l = cc_el_get_hash_table(lhs);
    HHashTable *r = cc_el_get_hash_table(rhs);

    if (lhs->_src) /* External data, so container must already exist */
        return cc_ht_assign(*l, *r);
    else
    {
        if (*l)
            cc_ht_destroy(*l);

        *l = cc_ht_copy(*r);
        if (!*l)
            CC_NO_MEM_HANDLER("out of memory");
    }

    return CC_OK;
}

static int cc_el_string_destructor(HElementData data)
{
    HString *d = cc_el_get_string(data);

    if (data->_src)
        cc_s_destroy_at(*d);
    else if (*d)
        cc_s_destroy(*d);

    return CC_OK;
}

static int cc_el_vector_destructor(HElementData data)
{
    HVector *d = cc_el_get_vector(data);

    if (data->_src)
        cc_v_destroy_at(*d, NULL);
    else if (*d)
        cc_v_destroy(*d, NULL);

    return CC_OK;
}

static int cc_el_linked_list_destructor(HElementData data)
{
    HLinkedList *d = cc_el_get_linked_list(data);

    if (data->_src)
        cc_ll_destroy_at(*d, NULL);
    else if (*d)
        cc_ll_destroy(*d, NULL);

    return CC_OK;
}

static int cc_el_doubly_linked_list_destructor(HElementData data)
{
    HDoublyLinkedList *d = cc_el_get_doubly_linked_list(data);

    if (data->_src)
        cc_dll_destroy_at(*d, NULL);
    else if (*d)
        cc_dll_destroy(*d, NULL);

    return CC_OK;
}

static int cc_el_hash_table_destructor(HElementData data)
{
    HHashTable *d = cc_el_get_hash_table(data);

    if (data->_src)
        cc_ht_destroy_at(*d);
    else if (*d)
        cc_ht_destroy(*d);

    return CC_OK;
}

static int cc_el_char_compare(HElementData lhs, HElementData rhs)
{
    char *l = (char *) (cc_el_storage_location(lhs));
    char *r = (char *) (cc_el_storage_location(rhs));

    return (*l > *r) - (*l < *r);
}

static int cc_el_signed_char_compare(HElementData lhs, HElementData rhs)
{
    signed char *l = (signed char *) (cc_el_storage_location(lhs));
    signed char *r = (signed char *) (cc_el_storage_location(rhs));

    return (*l > *r) - (*l < *r);
}

static int cc_el_unsigned_char_compare(HElementData lhs, HElementData rhs)
{
    unsigned char *l = (unsigned char *) (cc_el_storage_location(lhs));
    unsigned char *r = (unsigned char *) (cc_el_storage_location(rhs));

    return (*l > *r) - (*l < *r);
}

static int cc_el_signed_short_compare(HElementData lhs, HElementData rhs)
{
    signed short *l = (signed short *) (cc_el_storage_location(lhs));
    signed short *r = (signed short *) (cc_el_storage_location(rhs));

    return (*l > *r) - (*l < *r);
}

static int cc_el_unsigned_short_compare(HElementData lhs, HElementData rhs)
{
    unsigned short *l = (unsigned short *) (cc_el_storage_location(lhs));
    unsigned short *r = (unsigned short *) (cc_el_storage_location(rhs));

    return (*l > *r) - (*l < *r);
}

static int cc_el_signed_int_compare(HElementData lhs, HElementData rhs)
{
    signed int *l = (signed int *) (cc_el_storage_location(lhs));
    signed int *r = (signed int *) (cc_el_storage_location(rhs));

    return (*l > *r) - (*l < *r);
}

static int cc_el_unsigned_int_compare(HElementData lhs, HElementData rhs)
{
    unsigned int *l = (unsigned int *) (cc_el_storage_location(lhs));
    unsigned int *r = (unsigned int *) (cc_el_storage_location(rhs));

    return (*l > *r) - (*l < *r);
}

static int cc_el_signed_long_compare(HElementData lhs, HElementData rhs)
{
    signed long *l = (signed long *) (cc_el_storage_location(lhs));
    signed long *r = (signed long *) (cc_el_storage_location(rhs));

    return (*l > *r) - (*l < *r);
}

static int cc_el_unsigned_long_compare(HElementData lhs, HElementData rhs)
{
    unsigned long *l = (unsigned long *) (cc_el_storage_location(lhs));
    unsigned long *r = (unsigned long *) (cc_el_storage_location(rhs));

    return (*l > *r) - (*l < *r);
}

static int cc_el_signed_long_long_compare(HElementData lhs, HElementData rhs)
{
    signed long long *l = (signed long long *) (cc_el_storage_location(lhs));
    signed long long *r = (signed long long *) (cc_el_storage_location(rhs));

    return (*l > *r) - (*l < *r);
}

static int cc_el_unsigned_long_long_compare(HElementData lhs, HElementData rhs)
{
    unsigned long long *l = (unsigned long long *) (cc_el_storage_location(lhs));
    unsigned long long *r = (unsigned long long *) (cc_el_storage_location(rhs));

    return (*l > *r) - (*l < *r);
}

static int cc_el_float_compare(HElementData lhs, HElementData rhs)
{
    float *l = (float *) (cc_el_storage_location(lhs));
    float *r = (float *) (cc_el_storage_location(rhs));

    return (*l > *r) - (*l < *r);
}

static int cc_el_double_compare(HElementData lhs, HElementData rhs)
{
    double *l = (double *) (cc_el_storage_location(lhs));
    double *r = (double *) (cc_el_storage_location(rhs));

    return (*l > *r) - (*l < *r);
}

static int cc_el_voidp_compare(HElementData lhs, HElementData rhs)
{
    void **l = cc_el_storage_location(lhs);
    void **r = cc_el_storage_location(rhs);

    return (*l > *r) - (*l < *r);
}

static int cc_el_string_compare(HElementData lhs, HElementData rhs)
{
    HString *l = cc_el_get_string(lhs);
    HString *r = cc_el_get_string(rhs);

    return cc_s_compare(*l, *r, NULL);
}

static int cc_el_vector_compare(HElementData lhs, HElementData rhs)
{
    HVector *l = cc_el_get_vector(lhs);
    HVector *r = cc_el_get_vector(rhs);

    return cc_v_compare(*l, *r, NULL);
}

static int cc_el_linked_list_compare(HElementData lhs, HElementData rhs)
{
    HLinkedList *l = cc_el_get_linked_list(lhs);
    HLinkedList *r = cc_el_get_linked_list(rhs);

    return cc_ll_compare(*l, *r, NULL);
}

static int cc_el_doubly_linked_list_compare(HElementData lhs, HElementData rhs)
{
    HDoublyLinkedList *l = cc_el_get_doubly_linked_list(lhs);
    HDoublyLinkedList *r = cc_el_get_doubly_linked_list(rhs);

    return cc_dll_compare(*l, *r, NULL);
}

static int cc_el_hash_table_compare(HElementData lhs, HElementData rhs)
{
    HHashTable *l = cc_el_get_hash_table(lhs);
    HHashTable *r = cc_el_get_hash_table(rhs);

    return cc_ht_compare(*l, *r, NULL);
}

const char *cc_el_typename(ContainerElementType type)
{
    const char *types[] = {"null", "char", "signed char", "unsigned char",
                           "signed short", "unsigned short", "signed int",
                           "unsigned int", "signed long", "unsigned long",
                           "signed long long", "unsigned long long", "float",
                           "double", "void *", "string", "vector",
                           "linked list", "doubly linked list", "hash table"};

    if (type >= sizeof(types)/sizeof(types[0]))
        return "unknown type";

    return types[type];
}

ElementDataCallback cc_el_constructor(ContainerElementType type)
{
    switch (type)
    {
        default:
        case El_Null: return NULL;
        case El_Char: return cc_el_char_constructor;
        case El_SignedChar: return cc_el_signed_char_constructor;
        case El_SignedShort: return cc_el_signed_short_constructor;
        case El_SignedInt: return cc_el_signed_int_constructor;
        case El_SignedLong: return cc_el_signed_long_constructor;
        case El_SignedLongLong: return cc_el_signed_long_long_constructor;
        case El_UnsignedChar: return cc_el_unsigned_char_constructor;
        case El_UnsignedShort: return cc_el_unsigned_short_constructor;
        case El_UnsignedInt: return cc_el_unsigned_int_constructor;
        case El_UnsignedLong: return cc_el_unsigned_long_constructor;
        case El_UnsignedLongLong: return cc_el_unsigned_long_long_constructor;
        case El_Float: return cc_el_float_constructor;
        case El_Double: return cc_el_double_constructor;
        case El_VoidPtr: return cc_el_voidp_constructor;
        case El_String: return cc_el_string_constructor;
        case El_Vector: return cc_el_vector_constructor;
        case El_LinkedList: return cc_el_linked_list_constructor;
        case El_DoublyLinkedList: return cc_el_doubly_linked_list_constructor;
        case El_HashTable: return cc_el_hash_table_constructor;
    }
}

ElementDualDataCallback cc_el_copy_constructor(ContainerElementType type)
{
    switch (type)
    {
        default:
        case El_Null: return NULL;
        case El_Char: return cc_el_char_copy_constructor;
        case El_SignedChar: return cc_el_signed_char_copy_constructor;
        case El_SignedShort: return cc_el_signed_short_copy_constructor;
        case El_SignedInt: return cc_el_signed_int_copy_constructor;
        case El_SignedLong: return cc_el_signed_long_copy_constructor;
        case El_SignedLongLong: return cc_el_signed_long_long_copy_constructor;
        case El_UnsignedChar: return cc_el_unsigned_char_copy_constructor;
        case El_UnsignedShort: return cc_el_unsigned_short_copy_constructor;
        case El_UnsignedInt: return cc_el_unsigned_int_copy_constructor;
        case El_UnsignedLong: return cc_el_unsigned_long_copy_constructor;
        case El_UnsignedLongLong: return cc_el_unsigned_long_long_copy_constructor;
        case El_Float: return cc_el_float_copy_constructor;
        case El_Double: return cc_el_double_copy_constructor;
        case El_VoidPtr: return cc_el_voidp_copy_constructor;
        case El_String: return cc_el_string_copy_constructor;
        case El_Vector: return cc_el_vector_copy_constructor;
        case El_LinkedList: return cc_el_linked_list_copy_constructor;
        case El_DoublyLinkedList: return cc_el_doubly_linked_list_copy_constructor;
        case El_HashTable: return cc_el_hash_table_copy_constructor;
    }
}

ElementDataCallback cc_el_destructor(ContainerElementType type)
{
    switch (type)
    {
        default: return NULL;
        case El_String: return cc_el_string_destructor;
        case El_Vector: return cc_el_vector_destructor;
        case El_LinkedList: return cc_el_linked_list_destructor;
        case El_DoublyLinkedList: return cc_el_doubly_linked_list_destructor;
        case El_HashTable: return cc_el_hash_table_destructor;
    }
    return NULL;
}

ElementDualDataCallback cc_el_compare(ContainerElementType type)
{
    switch (type)
    {
        default:
        case El_Null: return NULL;
        case El_Char: return cc_el_char_compare;
        case El_SignedChar: return cc_el_signed_char_compare;
        case El_SignedShort: return cc_el_signed_short_compare;
        case El_SignedInt: return cc_el_signed_int_compare;
        case El_SignedLong: return cc_el_signed_long_compare;
        case El_SignedLongLong: return cc_el_signed_long_long_compare;
        case El_UnsignedChar: return cc_el_unsigned_char_compare;
        case El_UnsignedShort: return cc_el_unsigned_short_compare;
        case El_UnsignedInt: return cc_el_unsigned_int_compare;
        case El_UnsignedLong: return cc_el_unsigned_long_compare;
        case El_UnsignedLongLong: return cc_el_unsigned_long_long_compare;
        case El_Float: return cc_el_float_compare;
        case El_Double: return cc_el_double_compare;
        case El_VoidPtr: return cc_el_voidp_compare;
        case El_String: return cc_el_string_compare;
        case El_Vector: return cc_el_vector_compare;
        case El_LinkedList: return cc_el_linked_list_compare;
        case El_DoublyLinkedList: return cc_el_doubly_linked_list_compare;
        case El_HashTable: return cc_el_hash_table_compare;
    }
}

void *cc_el_userdata_in(HContainerElementMetaData metadata)
{
    return metadata->el_userdata;
}

void cc_el_set_userdata_in(HContainerElementMetaData metadata, void *userdata)
{
    metadata->el_userdata = userdata;
}

ElementDataCallback cc_el_constructor_in(HContainerElementMetaData metadata)
{
    return metadata->el_constructor;
}

void cc_el_set_constructor_in(HContainerElementMetaData metadata, ElementDataCallback constructor)
{
    metadata->el_constructor = constructor? constructor: cc_el_constructor(cc_el_metadata_type(metadata));
}

ElementDualDataCallback cc_el_copy_constructor_in(HContainerElementMetaData metadata)
{
    return metadata->el_copy_constructor;
}

void cc_el_set_copy_constructor_in(HContainerElementMetaData metadata, ElementDualDataCallback copy_constructor)
{
    metadata->el_copy_constructor = copy_constructor? copy_constructor: cc_el_copy_constructor(cc_el_metadata_type(metadata));
}

ElementDataCallback cc_el_destructor_in(HContainerElementMetaData metadata)
{
    return metadata->el_destructor;
}

void cc_el_set_destructor_in(HContainerElementMetaData metadata, ElementDataCallback destructor)
{
    metadata->el_destructor = destructor? destructor: cc_el_destructor(cc_el_metadata_type(metadata));
}

ElementDualDataCallback cc_el_compare_in(HContainerElementMetaData metadata)
{
    return metadata->el_compare;
}

void cc_el_set_compare_in(HContainerElementMetaData metadata, ElementDualDataCallback compare)
{
    metadata->el_compare = compare? compare: cc_el_compare(cc_el_metadata_type(metadata));
}


int cc_el_call_constructor_in(HContainerElementMetaData metadata, HElementData data)
{
    if (!metadata->el_constructor)
        return CC_NO_SUCH_METHOD;

    return metadata->el_constructor(data);
}

int cc_el_call_copy_constructor_in(HContainerElementMetaData metadata, HElementData dest, HElementData src)
{
    if (!metadata->el_copy_constructor)
        CC_NO_SUCH_METHOD_HANDLER("no copy constructor found");

    return metadata->el_copy_constructor(dest, src);
}

int cc_el_call_destructor_in(HContainerElementMetaData metadata, HElementData data)
{
    if (!metadata->el_destructor)
        return CC_NO_SUCH_METHOD;

    return metadata->el_destructor(data);
}

int cc_el_call_compare_in(HContainerElementMetaData metadata, HElementData lhs, HElementData rhs)
{
    if (!metadata->el_compare)
        CC_NO_SUCH_METHOD_HANDLER("no comparison method found");

    return metadata->el_compare(lhs, rhs);
}

int cc_el_init_at(void *buf, size_t buffer_size, ContainerElementType type, HContainerElementMetaData metadata, ElementDataCallback construct)
{
    if (buffer_size < cc_el_sizeof())
        CC_BAD_PARAM_HANDLER("buffer size too small");

    HElementData d = buf;

    ElementDataCallback cb = NULL;

    d->meta = metadata;
    d->type = type;
    d->_src = NULL;

    if (construct)
        cb = construct;

    if (!cb && metadata)
        cb = cc_el_constructor_in(metadata);

    if (!cb)
        cb = cc_el_constructor(type);

    if (cb)
        return cb(d);

    return CC_OK;
}

HElementData cc_el_init(ContainerElementType type, HContainerElementMetaData metadata, ElementDataCallback construct, int *err)
{
    HElementData result = MALLOC(cc_el_sizeof());
    if (!result)
        return NULL;

    CC_CLEANUP_ON_ERROR(cc_el_init_at(result, cc_el_sizeof(), type, metadata, construct),
                        if (err)
                            *err = ret;
                        FREE(result);
                        return NULL);

    return result;
}

int cc_el_move_contents(HElementData dest, HConstElementData src)
{
    if (dest->type != src->type)
        CC_BAD_PARAM_HANDLER("incompatible types");

    /* Destruct destination */
    ElementDataCallback cb = NULL;

    if (dest->meta)
        cb = cc_el_destructor_in(dest->meta);

    if (cb == NULL)
        cb = cc_el_destructor(dest->type);

    if (cb)
        cb(dest);

    /* Copy from source to destination */
    size_t size_to_copy = 0;

    if (dest->meta)
        size_to_copy = cc_el_metadata_type_size(dest->meta);
    else
        size_to_copy = cc_el_size_type(dest->type);

    memcpy(cc_el_storage_location(dest), cc_el_storage_location((HElementData) src), size_to_copy);

    /* Repair destination as needed */
    ContainerRepairCallback repair = cc_el_container_repair_for_type(dest->type);
    if (repair)
        repair(dest->_src? cc_el_storage_location_ptr(dest): *((void **) cc_el_storage_location(dest)));

    /* Initialize source again */
    cb = NULL;
    if (dest->meta)
        cb = cc_el_constructor_in(dest->meta);

    if (!cb)
        cb = cc_el_constructor(dest->type);

    if (cb)
        return cb((HElementData) src);

    return CC_OK;
}

int cc_el_copy_contents(HElementData dest, HConstElementData src)
{
    if (dest->type != src->type)
        CC_BAD_PARAM_HANDLER("incompatible types");

    ElementDualDataCallback cb = NULL;
    if (dest->meta)
        cb = cc_el_copy_constructor_in(dest->meta);

    if (cb == NULL)
        cb = cc_el_copy_constructor(dest->type);

    if (cb)
        return cb(dest, (HElementData) src);

    return CC_OK;
}

int cc_el_destroy_at(HElementData data)
{
    ElementDataCallback cb = NULL;
    if (data->meta)
        cb = cc_el_destructor_in(data->meta);

    if (cb == NULL)
        cb = cc_el_destructor(data->type);

    if (cb)
        cb(data);

    return CC_OK;
}

int cc_el_destroy(HElementData data)
{
    cc_el_destroy_at(data);
    FREE(data);
    return CC_OK;
}

#if C99
extern inline int cc_el_destroy_reference_at(HElementData data);
extern inline int cc_el_destroy_reference(HElementData data);
#else
int cc_el_destroy_reference_at(HElementData data)
{
    *cc_el_storage_location_ptr(data) = NULL;
    return cc_el_destroy_at(data);
}
int cc_el_destroy_reference(HElementData data)
{
    *cc_el_storage_location_ptr(data) = NULL;
    return cc_el_destroy(data);
}
#endif

size_t cc_el_sizeof()
{
    return sizeof(struct ElementData);
}

void cc_el_pretty_print(FILE *out, HElementData data, unsigned flags)
{
    const unsigned int FLAG_PRINT_TYPE = 0x01;
    const unsigned int FLAG_SUPPRESS_TYPE = 0x02;

    if ((flags & FLAG_PRINT_TYPE) && !(flags & FLAG_SUPPRESS_TYPE))
        fprintf(out, "<%s> ", cc_el_typename(data->type));

    flags &= ~FLAG_SUPPRESS_TYPE;

    switch (data->type)
    {
        default: fputs("<?>", out); break;
        case El_Char: fprintf(out, "%c (%d)", *cc_el_get_char(data), *cc_el_get_char(data)); break;
        case El_SignedChar: fprintf(out, "%c (%hd)", *cc_el_get_signed_char(data), *cc_el_get_signed_char(data)); break;
        case El_UnsignedChar: fprintf(out, "%c (%hd)", *cc_el_get_unsigned_char(data), *cc_el_get_unsigned_char(data)); break;
        case El_SignedShort: fprintf(out, "%hhd (%#hhx)", *cc_el_get_signed_short(data), *cc_el_get_signed_short(data)); break;
        case El_UnsignedShort: fprintf(out, "%hhd (%#hhx)", *cc_el_get_unsigned_short(data), *cc_el_get_unsigned_short(data)); break;
        case El_SignedInt: fprintf(out, "%d (%#x)", *cc_el_get_signed_int(data), *cc_el_get_signed_int(data)); break;
        case El_UnsignedInt: fprintf(out, "%d (%#x)", *cc_el_get_unsigned_int(data), *cc_el_get_unsigned_int(data)); break;
        case El_SignedLong: fprintf(out, "%ld (%lx)", *cc_el_get_signed_long(data), *cc_el_get_signed_long(data)); break;
        case El_UnsignedLong: fprintf(out, "%ld (%lx)", *cc_el_get_unsigned_long(data), *cc_el_get_unsigned_long(data)); break;
        case El_SignedLongLong: fprintf(out, "%lld (%llx)", *cc_el_get_signed_long_long(data), *cc_el_get_signed_long_long(data)); break;
        case El_UnsignedLongLong: fprintf(out, "%lld (%llx)", *cc_el_get_unsigned_long_long(data), *cc_el_get_unsigned_long_long(data)); break;
        case El_Float: fprintf(out, "%.8g", *cc_el_get_float(data)); break;
        case El_Double: fprintf(out, "%.17g", *cc_el_get_double(data)); break;
        case El_VoidPtr: fprintf(out, "%p", *cc_el_get_voidp(data)); break;
        case El_String: fprintf(out, "%.*s", (int) cc_s_size(*cc_el_get_string(data)), cc_s_raw(*cc_el_get_string(data))); break;
        case El_Vector:
        {
            HVector vector = *cc_el_get_vector(data);
            Iterator it, begin = cc_v_begin(vector);

            if (flags & FLAG_PRINT_TYPE)
                fprintf(out, "(%u, <%s>)[ ", (unsigned) cc_v_size(vector),
                        cc_el_typename(cc_el_metadata_type(cc_v_metadata(vector))));
            else
                fprintf(out, "(%u)[", (unsigned) cc_v_size(vector));

            for (it = begin; it; it = cc_v_next(vector, it)) {
                if (it != begin)
                    fputc(' ', out);
                cc_el_pretty_print(out, cc_v_node_data_easy(vector, it), flags);
            }

            fputs(" ]", out);

            break;
        }
        case El_LinkedList:
        {
            HLinkedList list = *cc_el_get_linked_list(data);
            Iterator it, begin = cc_ll_begin(list);

            if (flags & FLAG_PRINT_TYPE)
                fprintf(out, "(%u, <%s>)[ ", (unsigned) cc_ll_size(list),
                        cc_el_typename(cc_el_metadata_type(cc_ll_metadata(list))));
            else
                fprintf(out, "(%u)[ ", (unsigned) cc_ll_size(list));

            for (it = begin; it; it = cc_ll_next(list, it)) {
                if (it != begin)
                    fputc(' ', out);
                cc_el_pretty_print(out, cc_ll_node_data_easy(list, it), flags);
            }

            fputs(" ]", out);

            break;
        }
        case El_DoublyLinkedList:
        {
            HDoublyLinkedList list = *cc_el_get_doubly_linked_list(data);
            Iterator it, begin = cc_dll_begin(list);

            if (flags & FLAG_PRINT_TYPE)
                fprintf(out, "(%u, <%s>)[ ", (unsigned) cc_dll_size(list),
                        cc_el_typename(cc_el_metadata_type(cc_dll_metadata(list))));
            else
                fprintf(out, "(%u)[ ", (unsigned) cc_dll_size(list));

            for (it = begin; it; it = cc_dll_next(list, it)) {
                if (it != begin)
                    fputc(' ', out);
                cc_el_pretty_print(out, cc_dll_node_data_easy(list, it), flags);
            }

            fputs(" ]", out);

            break;
        }
        case El_HashTable:
        {
            HHashTable list = *cc_el_get_hash_table(data);
            ExIterator it, begin = cc_ht_begin(list);

            if (flags & FLAG_PRINT_TYPE)
                fprintf(out, "(%u, <%s, %s>)[ ", (unsigned) cc_ht_size(list),
                        cc_el_typename(cc_el_metadata_type(cc_ht_key_metadata(list))),
                        cc_el_typename(cc_el_metadata_type(cc_ht_value_metadata(list))));
            else
                fprintf(out, "(%u)[ ", (unsigned) cc_ht_size(list));

            for (it = begin; ExIteratorNonNull(it); it = cc_ht_next(list, it)) {
                if (!ExIteratorEquals(it, begin))
                    fputc(' ', out);
                fputc('{', out);
                cc_el_pretty_print(out, cc_ht_node_key_easy(list, it), flags | FLAG_SUPPRESS_TYPE);
                fputs(": ", out);
                cc_el_pretty_print(out, cc_ht_node_data_easy(list, it), flags | FLAG_SUPPRESS_TYPE);
                fputc('}', out);
            }

            fputs(" ]", out);

            break;
        }
    }
}

/* Returns CC_BAD_PARAM if there is an error, or CC_OK if successful */
static int cc_el_update_type_information(HElementData data, ContainerElementType new_)
{
    if (data->_src != NULL) {
        if (data->type != new_)
        {
            /* Proof of an external storage location. Since we don't want to store external information of a different type,
               the program should return an error message. Also, since we don't know the size limit of the external storage
               location, we shouldn't try to put anything there that might cause a larger error */
                CC_BAD_PARAM_HANDLER("attempted to assign value of different type to external data reference");
        } else { /* Type is not changing, we're just changing the value, so clear it if it's a container */
            switch (new_) {
                default: break;
                case El_String: cc_s_clear(*cc_el_get_string(data)); break;
                case El_Vector: cc_v_clear(*cc_el_get_vector(data), NULL); break;
                case El_LinkedList: cc_ll_clear(*cc_el_get_linked_list(data), NULL); break;
                case El_DoublyLinkedList: cc_dll_clear(*cc_el_get_doubly_linked_list(data), NULL); break;
                case El_HashTable: cc_ht_clear(*cc_el_get_hash_table(data)); break;
            }

            return CC_OK;
        }
    }

    ElementDataCallback cb = NULL;
    if (data->meta)
        cb = cc_el_destructor_in(data->meta);

    if (cb == NULL)
        cb = cc_el_destructor(data->type);

    if (cb)
        CC_RETURN_ON_ERROR(cb(data));

    data->type = new_;

    if (data->type != new_)
        data->meta = NULL;

    return CC_OK;
}

int cc_el_assign_char(HElementData data, char d)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_Char));
    *((char *) cc_el_storage_location(data)) = d;
    return CC_OK;
}
int cc_el_assign_signed_char(HElementData data, signed char d)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_SignedChar));
    *((signed char *) cc_el_storage_location(data)) = d;
    return CC_OK;
}
int cc_el_assign_unsigned_char(HElementData data, unsigned char d)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_UnsignedChar));
    *((unsigned char *) cc_el_storage_location(data)) = d;
    return CC_OK;
}
int cc_el_assign_signed_short(HElementData data, signed short d)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_SignedShort));
    *((signed short *) cc_el_storage_location(data)) = d;
    return CC_OK;
}
int cc_el_assign_unsigned_short(HElementData data, unsigned short d)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_UnsignedShort));
    *((unsigned short *) cc_el_storage_location(data)) = d;
    return CC_OK;
}
int cc_el_assign_signed_int(HElementData data, signed int d)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_SignedInt));
    *((signed int *) cc_el_storage_location(data)) = d;
    return CC_OK;
}
int cc_el_assign_unsigned_int(HElementData data, unsigned int d)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_UnsignedInt));
    *((unsigned int *) cc_el_storage_location(data)) = d;
    return CC_OK;
}
int cc_el_assign_signed_long(HElementData data, signed long d)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_SignedLong));
    *((signed long *) cc_el_storage_location(data)) = d;
    return CC_OK;
}
int cc_el_assign_unsigned_long(HElementData data, unsigned long d)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_UnsignedLong));
    *((unsigned long *) cc_el_storage_location(data)) = d;
    return CC_OK;
}
int cc_el_assign_signed_long_long(HElementData data, signed long long d)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_SignedLongLong));
    *((signed long long *) cc_el_storage_location(data)) = d;
    return CC_OK;
}
int cc_el_assign_unsigned_long_long(HElementData data, unsigned long long d)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_UnsignedLongLong));
    *((unsigned long long *) cc_el_storage_location(data)) = d;
    return CC_OK;
}
int cc_el_assign_float(HElementData data, float d)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_Float));
    *((float *) cc_el_storage_location(data)) = d;
    return CC_OK;
}
int cc_el_assign_double(HElementData data, double d)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_Double));
    *((double *) cc_el_storage_location(data)) = d;
    return CC_OK;
}
int cc_el_assign_voidp(HElementData data, void *p)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_VoidPtr));
    *((void **) cc_el_storage_location(data)) = p;
    return CC_OK;
}
int cc_el_assign_string(HElementData data, HString d)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_String));

    HString *ptr = cc_el_get_string(data);
    if (data->_src) /* External storage, container already exists */
    {
        if (d)
            return cc_s_assign_cstring_n(*ptr, cc_s_raw(d), cc_s_size(d));

        cc_s_clear(*ptr);
        return CC_OK;
    }

    if (d)
    {
        *ptr = cc_s_copy(d);
        if (!*ptr)
            CC_NO_MEM_HANDLER("out of memory");

        return CC_OK;
    }
    else
        *ptr = NULL;
    return CC_OK;
}
int cc_el_assign_cstring(HElementData data, const char *p)
{
    return cc_el_assign_cstring_n(data, p, strlen(p));
}
int cc_el_assign_cstring_n(HElementData data, const char *c, size_t len)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_String));
    HString *ptr = cc_el_get_string(data);
    if (data->_src) /* External storage, container already exists */
    {
        if (c)
            return cc_s_assign_cstring_n(*ptr, c, len);

        cc_s_clear(*ptr);
        return CC_OK;
    }

    if (c)
    {
        *ptr = cc_s_init();
        if (!*ptr)
            CC_NO_MEM_HANDLER("out of memory");

        return cc_s_assign_cstring_n(*ptr, c, len);
    }
    else
        *ptr = NULL;
    return CC_OK;
}
int cc_el_assign_vector(HElementData data, HVector d)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_Vector));
    HVector *ptr = cc_el_get_vector(data);
    if (data->_src) /* External storage, container already exists */
    {
        if (d)
            return cc_v_assign(*ptr, d);

        cc_v_clear(*ptr, NULL);
        return CC_OK;
    }

    if (d)
    {
        *ptr = cc_v_copy(d, NULL, NULL);
        if (!*ptr)
            CC_NO_MEM_HANDLER("out of memory");

        return CC_OK;
    }
    else
        *ptr = NULL;
    return CC_OK;
}
int cc_el_assign_linked_list(HElementData data, HLinkedList d)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_LinkedList));
    HLinkedList *ptr = cc_el_get_linked_list(data);
    if (data->_src) /* External storage, container already exists */
    {
        if (d)
            return cc_ll_assign(*ptr, d);

        cc_ll_clear(*ptr, NULL);
        return CC_OK;
    }

    if (d)
    {
        *ptr = cc_ll_copy(d, NULL, NULL, NULL);
        if (!*ptr)
            CC_NO_MEM_HANDLER("out of memory");

        return CC_OK;
    }
    else
        *ptr = NULL;
    return CC_OK;
}
int cc_el_assign_doubly_linked_list(HElementData data, HDoublyLinkedList d)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_DoublyLinkedList));
    HDoublyLinkedList *ptr = cc_el_get_doubly_linked_list(data);
    if (data->_src) /* External storage, container already exists */
    {
        if (d)
            return cc_dll_assign(*ptr, d);

        cc_dll_clear(*ptr, NULL);
        return CC_OK;
    }

    if (d)
    {
        *ptr = cc_dll_copy(d, NULL, NULL);
        if (!*ptr)
            CC_NO_MEM_HANDLER("out of memory");

        return CC_OK;
    }
    else
        *ptr = NULL;
    return CC_OK;
}
int cc_el_assign_hash_table(HElementData data, HHashTable d)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_HashTable));
    HHashTable *ptr = cc_el_get_hash_table(data);
    if (data->_src) /* External storage, container already exists */
    {
        if (d)
            return cc_ht_assign(*ptr, d);

        cc_ht_clear(*ptr);
        return CC_OK;
    }

    if (d)
    {
        *ptr = cc_ht_copy(d);
        if (!*ptr)
            CC_NO_MEM_HANDLER("out of memory");

        return CC_OK;
    }
    else
        *ptr = NULL;
    return CC_OK;
}

char *cc_el_get_char(HElementData data) {return data->type == El_Char? (char *) (cc_el_storage_location(data)): NULL;}
signed char *cc_el_get_signed_char(HElementData data) {return data->type == El_SignedChar? (signed char *) (cc_el_storage_location(data)): NULL;}
unsigned char *cc_el_get_unsigned_char(HElementData data) {return data->type == El_UnsignedChar? (unsigned char *) (cc_el_storage_location(data)): NULL;}
signed short *cc_el_get_signed_short(HElementData data) {return data->type == El_SignedShort? (signed short *) (cc_el_storage_location(data)): NULL;}
unsigned short *cc_el_get_unsigned_short(HElementData data) {return data->type == El_UnsignedShort? (unsigned short *) (cc_el_storage_location(data)): NULL;}
signed int *cc_el_get_signed_int(HElementData data) {return data->type == El_SignedInt? (signed int *) (cc_el_storage_location(data)): NULL;}
unsigned int *cc_el_get_unsigned_int(HElementData data) {return data->type == El_UnsignedInt? (unsigned int *) (cc_el_storage_location(data)): NULL;}
signed long *cc_el_get_signed_long(HElementData data) {return data->type == El_SignedLong? (signed long *) (cc_el_storage_location(data)): NULL;}
unsigned long *cc_el_get_unsigned_long(HElementData data) {return data->type == El_UnsignedLong? (unsigned long *) (cc_el_storage_location(data)): NULL;}
signed long long *cc_el_get_signed_long_long(HElementData data) {return data->type == El_SignedLongLong? (signed long long *) (cc_el_storage_location(data)): NULL;}
unsigned long long *cc_el_get_unsigned_long_long(HElementData data) {return data->type == El_UnsignedLongLong? (unsigned long long *) (cc_el_storage_location(data)): NULL;}
float *cc_el_get_float(HElementData data) {return data->type == El_Float? (float *) (cc_el_storage_location(data)): NULL;}
double *cc_el_get_double(HElementData data) {return data->type == El_Double? (double *) (cc_el_storage_location(data)): NULL;}
void **cc_el_get_voidp(HElementData data) {return data->type == El_VoidPtr? (void **) (cc_el_storage_location(data)): NULL;}
HString *cc_el_get_string(HElementData data) {return data->type == El_String? data->_src? (HString *) cc_el_storage_location_ptr(data): (HString *) (cc_el_storage_location(data)): NULL;}
HVector *cc_el_get_vector(HElementData data) {return data->type == El_Vector? data->_src? (HVector *) cc_el_storage_location_ptr(data): (HVector *) (cc_el_storage_location(data)): NULL;}
HLinkedList *cc_el_get_linked_list(HElementData data) {return data->type == El_LinkedList? data->_src? (HLinkedList *) cc_el_storage_location_ptr(data): (HLinkedList *) (cc_el_storage_location(data)): NULL;}
HDoublyLinkedList *cc_el_get_doubly_linked_list(HElementData data) {return data->type == El_DoublyLinkedList? data->_src? (HDoublyLinkedList *) cc_el_storage_location_ptr(data): (HDoublyLinkedList *) (cc_el_storage_location(data)): NULL;}
HHashTable *cc_el_get_hash_table(HElementData data) {return data->type == El_HashTable? data->_src? (HHashTable *) cc_el_storage_location_ptr(data): (HHashTable *) (cc_el_storage_location(data)): NULL;}

int cc_el_set_metadata(HElementData data, HContainerElementMetaData meta)
{
    if (!cc_el_compatible_metadata_element(meta, data))
        CC_TYPE_MISMATCH_HANDLER("cannot set metadata with specified type", /*expected*/ cc_el_type(data), /*actual*/ cc_el_metadata_type(meta));
    data->meta = meta;
    return CC_OK;
}

HContainerElementMetaData cc_el_get_metadata(HConstElementData data)
{
    return data->meta;
}

ContainerRepairCallback cc_el_container_repair_for_type(ContainerElementType type)
{
    switch (type)
    {
        default: return NULL;
        case El_HashTable: return (ContainerRepairCallback) cc_ht_repair;
    }
}

size_t cc_el_size_type(ContainerElementType type)
{
    switch (type)
    {
        default:
        case El_Null: return 0;
        case El_Char: return sizeof(char);
        case El_SignedChar: return sizeof(signed char);
        case El_UnsignedChar: return sizeof(unsigned char);
        case El_SignedShort: return sizeof(signed short);
        case El_UnsignedShort: return sizeof(unsigned short);
        case El_SignedInt: return sizeof(signed int);
        case El_UnsignedInt: return sizeof(unsigned int);
        case El_SignedLong: return sizeof(signed long);
        case El_UnsignedLong: return sizeof(unsigned long);
        case El_SignedLongLong: return sizeof(signed long long);
        case El_UnsignedLongLong: return sizeof(unsigned long long);
        case El_Float: return sizeof(float);
        case El_Double: return sizeof(double);
        case El_VoidPtr: return sizeof(void *);
        case El_String: return cc_s_sizeof();
        case El_Vector: return cc_v_sizeof();
        case El_LinkedList: return cc_ll_sizeof();
        case El_DoublyLinkedList: return cc_dll_sizeof();
        case El_HashTable: return cc_ht_sizeof();
    }
}

size_t cc_el_metadata_sizeof()
{
    return sizeof(struct ContainerElementMetaData);
}

HContainerElementMetaData cc_el_make_metadata(ContainerElementType type)
{
    HContainerElementMetaData result = MALLOC(sizeof(*result));

    if (!result || CC_OK != cc_el_make_metadata_at(result, cc_el_metadata_sizeof(), type))
        return NULL;

    return result;
}

int cc_el_make_metadata_at(void *buf, size_t buffer_size, ContainerElementType type)
{
    if (buffer_size < cc_el_metadata_sizeof())
        CC_BAD_PARAM_HANDLER("buffer size too small");

    HContainerElementMetaData result = buf;

    result->el_constructor = cc_el_constructor(type);
    result->el_destructor = cc_el_destructor(type);
    result->el_copy_constructor = cc_el_copy_constructor(type);
    result->el_compare = cc_el_compare(type);
    result->el_type = type;
    result->el_size = cc_el_size_type(type);
    result->el_userdata = NULL;

    return CC_OK;
}

void cc_el_copy_metadata(HContainerElementMetaData dest, const HContainerElementMetaData src)
{
    *dest = *src;
}

void cc_el_kill_metadata_at(HContainerElementMetaData metadata)
{
    (void) metadata;
}

void cc_el_kill_metadata(HContainerElementMetaData metadata)
{
    cc_el_kill_metadata_at(metadata);
    FREE(metadata);
}

ContainerElementType cc_el_type(HConstElementData data)
{
    return data->type;
}

HContainerElementMetaData cc_el_contained_key_metadata(HElementData data)
{
    switch (cc_el_type(data))
    {
        default: return NULL;
        case El_HashTable: return cc_ht_key_metadata(*((HHashTable *) cc_el_storage_location(data)));
    }
}

HContainerElementMetaData cc_el_contained_value_metadata(HElementData data)
{
    switch (cc_el_type(data))
    {
        default: return NULL;
        case El_Vector: return cc_v_metadata(*((HVector *) cc_el_storage_location(data)));
        case El_LinkedList: return cc_ll_metadata(*((HLinkedList *) cc_el_storage_location(data)));
        case El_DoublyLinkedList: return cc_dll_metadata(*((HDoublyLinkedList *) cc_el_storage_location(data)));
        case El_HashTable: return cc_ht_value_metadata(*((HHashTable *) cc_el_storage_location(data)));
    }
}

ContainerElementType cc_el_metadata_type(const HContainerElementMetaData metadata)
{
    return metadata->el_type;
}

size_t cc_el_metadata_type_size(const HContainerElementMetaData metadata)
{
    return metadata->el_size;
}

int cc_el_compatible_metadata(const HContainerElementMetaData lhs,
                              const HContainerElementMetaData rhs)
{
    return lhs->el_type == rhs->el_type;
}

int cc_el_compatible_metadata_element(const HContainerElementMetaData meta,
                                      HConstElementData data)
{
    return meta->el_type == data->type;
}

int cc_el_hash_default(HConstElementData element, unsigned *hash)
{
    HElementData data = (HElementData) element;

    switch (cc_el_type(data))
    {
        default:
        case El_Null: CC_BAD_PARAM_HANDLER("cannot perform hash on non-hashable type");
        case El_Char:
        case El_SignedChar:
        case El_UnsignedChar:
            *hash = *((unsigned char *) cc_el_storage_location(data));
            break;
        case El_SignedShort:
        case El_UnsignedShort:
            *hash = *((unsigned short *) cc_el_storage_location(data));
            break;
        case El_SignedInt:
        case El_UnsignedInt:
            *hash = *((unsigned int *) cc_el_storage_location(data));
            break;
        case El_SignedLong:
        case El_UnsignedLong:
            if (sizeof(int) == sizeof(long))
                *hash = *((unsigned int *) cc_el_storage_location(data));
            else
                *hash = pearson_hash(cc_el_storage_location(data), sizeof(unsigned long));
        case El_SignedLongLong:
        case El_UnsignedLongLong:
            if (sizeof(int) == sizeof(long long))
                *hash = *((unsigned int *) cc_el_storage_location(data));
            else
                *hash = pearson_hash(cc_el_storage_location(data), sizeof(unsigned long long));
            break;
        case El_Float:
            if (sizeof(int) == sizeof(float))
                *hash = *((unsigned int *) cc_el_storage_location(data));
            else if (sizeof(short) == sizeof(float))
                *hash = *((unsigned short *) cc_el_storage_location(data));
            else if (sizeof(char) == sizeof(float))
                *hash = *((unsigned char *) cc_el_storage_location(data));
            else
                *hash = pearson_hash(cc_el_storage_location(data), sizeof(float));
            break;
        case El_Double:
            if (sizeof(int) == sizeof(double))
                *hash = *((unsigned int *) cc_el_storage_location(data));
            else if (sizeof(short) == sizeof(double))
                *hash = *((unsigned short *) cc_el_storage_location(data));
            else if (sizeof(char) == sizeof(double))
                *hash = *((unsigned char *) cc_el_storage_location(data));
            else
                *hash = pearson_hash(cc_el_storage_location(data), sizeof(double));
            break;
        case El_VoidPtr:
            if (sizeof(int) == sizeof(void*))
                *hash = *((unsigned int *) cc_el_storage_location(data));
            else if (sizeof(short) == sizeof(void*))
                *hash = *((unsigned short *) cc_el_storage_location(data));
            else if (sizeof(char) == sizeof(void*))
                *hash = *((unsigned char *) cc_el_storage_location(data));
            else
                *hash = pearson_hash(cc_el_storage_location(data), sizeof(void *));
            break;
        case El_String:
        {
            HString string = *cc_el_get_string(data);

            if (cc_s_size(string))
                *hash = pearson_hash(cc_s_raw(string), cc_s_size(string));
            else
                *hash = 0;

            break;
        }
        case El_Vector:
        {
            HVector vector = *cc_el_get_vector(data);

            switch (cc_el_metadata_type(cc_el_contained_value_metadata(data)))
            {
                default: CC_BAD_PARAM_HANDLER("cannot hash vector of non-primitive type");
                case El_Char:
                case El_SignedChar:
                case El_UnsignedChar:
                case El_SignedShort:
                case El_UnsignedShort:
                case El_SignedInt:
                case El_UnsignedInt:
                case El_SignedLong:
                case El_UnsignedLong:
                case El_SignedLongLong:
                case El_UnsignedLongLong:
                case El_Float:
                case El_Double:
                case El_VoidPtr:
                    if (cc_v_size(vector))
                        *hash = pearson_hash(cc_v_raw(vector), cc_v_size(vector) * cc_el_metadata_type_size(cc_v_metadata(vector)));
                    else
                        *hash = 0;
                    break;
            }
            break;
        }
    }

    return CC_OK;
}

#ifdef __cplusplus
}
#endif
