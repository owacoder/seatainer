#include "element.h"
#include "ccvector.h"
#include "cclnklst.h"
#include "ccdbllst.h"

#include <memory.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Currently, one element takes up 16 bytes. That's too many for a vector of chars! */
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

struct ContainerElementMetaData
{
    ElementDataCallback el_constructor;
    ElementDualDataCallback el_copy_constructor;
    ElementDataCallback el_destructor;
    ElementDualDataCallback el_compare;
    size_t el_size; /* Size in bytes of contained elements */
    ContainerElementType el_type; /* Type of contained elements */
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

static int cc_el_vector_copy_constructor(HElementData lhs, HElementData rhs)
{
    HVector *l = cc_el_storage_location(lhs);
    HVector *r = cc_el_storage_location(rhs);

    if (*l)
        cc_v_destroy(*l, NULL);

    *l = cc_v_copy(*r, NULL, NULL);
    if (!*l)
        CC_NO_MEM_HANDLER("out of memory");

    return CC_OK;
}

static int cc_el_linked_list_copy_constructor(HElementData lhs, HElementData rhs)
{
    HLinkedList *l = cc_el_storage_location(lhs);
    HLinkedList *r = cc_el_storage_location(rhs);

    if (*l)
        cc_ll_destroy(*l, NULL);

    *l = cc_ll_copy(*r, NULL, NULL);
    if (!*l)
        CC_NO_MEM_HANDLER("out of memory");

    return CC_OK;
}

static int cc_el_doubly_linked_list_copy_constructor(HElementData lhs, HElementData rhs)
{
    HDoublyLinkedList *l = cc_el_storage_location(lhs);
    HDoublyLinkedList *r = cc_el_storage_location(rhs);

    if (*l)
        cc_dll_destroy(*l, NULL);

    *l = cc_dll_copy(*r, NULL, NULL);
    if (!*l)
        CC_NO_MEM_HANDLER("out of memory");

    return CC_OK;
}

static int cc_el_vector_destructor(HElementData data)
{
    HVector *d = cc_el_storage_location(data);

    if (*d)
        cc_v_destroy(*d, NULL);

    return CC_OK;
}

static int cc_el_linked_list_destructor(HElementData data)
{
    HLinkedList *d = cc_el_storage_location(data);

    if (*d)
        cc_ll_destroy(*d, NULL);

    return CC_OK;
}

static int cc_el_doubly_linked_list_destructor(HElementData data)
{
    HDoublyLinkedList *d = cc_el_storage_location(data);

    if (*d)
        cc_dll_destroy(*d, NULL);

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

static int cc_el_vector_compare(HElementData lhs, HElementData rhs)
{
    HVector *l = cc_el_storage_location(lhs);
    HVector *r = cc_el_storage_location(rhs);

    return cc_v_compare(*l, *r, NULL);
}

static int cc_el_linked_list_compare(HElementData lhs, HElementData rhs)
{
    HLinkedList *l = cc_el_storage_location(lhs);
    HLinkedList *r = cc_el_storage_location(rhs);

    return cc_ll_compare(*l, *r, NULL);
}

static int cc_el_doubly_linked_list_compare(HElementData lhs, HElementData rhs)
{
    HDoublyLinkedList *l = cc_el_storage_location(lhs);
    HDoublyLinkedList *r = cc_el_storage_location(rhs);

    return cc_dll_compare(*l, *r, NULL);
}

const char *cc_el_typename(ContainerElementType type)
{
    const char *types[] = {"null", "char", "signed char", "unsigned char",
                           "signed short", "unsigned short", "signed int",
                           "unsigned int", "signed long", "unsigned long",
                           "signed long long", "unsigned long long", "float",
                           "double", "const char *", "void *", "vector", "linked list", "doubly linked list"};

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
        case El_CString: return NULL;
        case El_VoidPtr:
        case El_Vector:
        case El_LinkedList:
        case El_DoublyLinkedList:
            return cc_el_voidp_constructor;
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
        case El_CString: return NULL;
        case El_VoidPtr: return cc_el_voidp_copy_constructor;
        case El_Vector: return cc_el_vector_copy_constructor;
        case El_LinkedList: return cc_el_linked_list_copy_constructor;
        case El_DoublyLinkedList: return cc_el_doubly_linked_list_copy_constructor;
    }
}

ElementDataCallback cc_el_destructor(ContainerElementType type)
{
    switch (type)
    {
        default: return NULL;
        case El_Vector: return cc_el_vector_destructor;
        case El_LinkedList: return cc_el_linked_list_destructor;
        case El_DoublyLinkedList: return cc_el_doubly_linked_list_destructor;
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
        case El_CString: return NULL;
        case El_VoidPtr: return cc_el_voidp_compare;
        case El_Vector: return cc_el_vector_compare;
        case El_LinkedList: return cc_el_linked_list_compare;
        case El_DoublyLinkedList: return cc_el_doubly_linked_list_compare;
    }
}

ElementDataCallback cc_el_constructor_in(HContainerElementMetaData metadata)
{
    return metadata->el_constructor;
}

ElementDualDataCallback cc_el_copy_constructor_in(HContainerElementMetaData metadata)
{
    return metadata->el_copy_constructor;
}

ElementDataCallback cc_el_destructor_in(HContainerElementMetaData metadata)
{
    return metadata->el_destructor;
}

ElementDualDataCallback cc_el_compare_in(HContainerElementMetaData metadata)
{
    return metadata->el_compare;
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
    if (buffer_size < sizeof(struct ElementData))
        return CC_BAD_PARAM;

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
    HElementData result = MALLOC(sizeof(*result), 1);
    if (!result)
        return NULL;

    CC_CLEANUP_ON_ERROR(cc_el_init_at(result, sizeof(*result), type, metadata, construct),
                        if (err)
                            *err = ret;
                        FREE(result);
                        return NULL);

    return result;
}

int cc_el_move(HElementData dest, HElementData src)
{
    /* Destruct destination */
    ElementDataCallback cb = NULL;

    if (dest->meta)
        cb = cc_el_destructor_in(dest->meta);

    if (cb == NULL)
        cb = cc_el_destructor(dest->type);

    if (cb)
        cb(dest);

    /* Raw copy */
    *dest = *src;

    /* Initialize source again */
    if (src->_src == NULL)
    {
        ElementDataCallback cb = NULL;
        if (dest->meta)
            cb = cc_el_constructor_in(dest->meta);

        if (!cb)
            cb = cc_el_constructor(dest->type);

        if (cb)
            return cb(src);
    }
    else
        src->_src = NULL;

    return CC_OK;
}

int cc_el_copy_contents(HElementData dest, HConstElementData src)
{
    if (dest->type != src->type)
        return CC_BAD_PARAM;

    ElementDualDataCallback cb = NULL;
    if (dest->meta)
        cb = cc_el_copy_constructor_in(dest->meta);

    if (cb == NULL)
        cb = cc_el_copy_constructor(dest->type);

    if (cb)
        return cb(dest, (HElementData) src);

    return CC_OK;
}

int cc_el_destroy(HElementData data)
{
    ElementDataCallback cb = NULL;
    if (data->meta)
        cb = cc_el_destructor_in(data->meta);

    if (cb == NULL)
        cb = cc_el_destructor(data->type);

    if (cb)
        cb(data);

    FREE(data);
    return CC_OK;
}

#ifdef C99
extern inline int cc_el_destroy_reference(HElementData data);
#else
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

/* Returns CC_BAD_PARAM if there is an error, or CC_OK if successful */
static int cc_el_update_type_information(HElementData data, ContainerElementType new_)
{
    if (data->type != new_ && data->_src != NULL)
    {
        /* Proof of an external storage location. Since we don't want to store external information of a different type,
           the program should return an error message. Also, since we don't know the size limit of the external storage
           location, we shouldn't try to put anything there that might cause a larger error */
            return CC_BAD_PARAM;
    }

    ElementDataCallback cb = NULL;
    if (data->meta)
        cb = cc_el_destructor_in(data->meta);

    if (cb == NULL)
        cb = cc_el_destructor(data->type);

    if (cb)
        CC_RETURN_ON_ERROR(cb(data));

    data->type = new_;

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
int cc_el_assign_vector(HElementData data, HVector d)
{
    CC_RETURN_ON_ERROR(cc_el_update_type_information(data, El_Vector));
    HVector *ptr = cc_el_storage_location(data);
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
    HLinkedList *ptr = cc_el_storage_location(data);
    if (d)
    {
        *ptr = cc_ll_copy(d, NULL, NULL);
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
    HDoublyLinkedList *ptr = cc_el_storage_location(data);
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
HVector *cc_el_get_vector(HElementData data) {return data->type == El_Vector? (HVector *) (cc_el_storage_location(data)): NULL;}
HLinkedList *cc_el_get_linked_list(HElementData data) {return data->type == El_LinkedList? (HLinkedList *) (cc_el_storage_location(data)): NULL;}
HDoublyLinkedList *cc_el_get_doubly_linked_list(HElementData data) {return data->type == El_DoublyLinkedList? (HDoublyLinkedList *) (cc_el_storage_location(data)): NULL;}

size_t cc_el_size_of_type(ContainerElementType type)
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
        case El_CString: return sizeof(const char *);
        case El_VoidPtr: return sizeof(void *);
        case El_Vector: return sizeof(HVector *);
        case El_LinkedList: return sizeof(HLinkedList *);
        case El_DoublyLinkedList: return sizeof(HDoublyLinkedList *);
    }
}

HContainerElementMetaData cc_el_make_metadata(ContainerElementType type)
{
    HContainerElementMetaData result = MALLOC(sizeof(*result), 1);

    if (!result)
        return NULL;

    result->el_constructor = cc_el_constructor(type);
    result->el_destructor = cc_el_destructor(type);
    result->el_copy_constructor = cc_el_copy_constructor(type);
    result->el_compare = cc_el_compare(type);
    result->el_type = type;
    result->el_size = cc_el_size_of_type(type);

    return result;
}

void cc_el_copy_metadata(HContainerElementMetaData dest, const HContainerElementMetaData src)
{
    *dest = *src;
}

void cc_el_kill_metadata(HContainerElementMetaData metadata)
{
    FREE(metadata);
}

ContainerElementType cc_el_type(HConstElementData data)
{
    return data->type;
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

#ifdef __cplusplus
}
#endif
