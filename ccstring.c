#include "ccstring.h"

#include <string.h>
#include <stdlib.h>

#include "utility.h"

#ifdef __cplusplus
extern "C" {
#endif

struct StringHelper
{
    char *data; /* Array of `capacity` bytes */
    size_t capacity;
};

struct String
{
    union {
        struct StringHelper helper;
        char tight[sizeof(struct StringHelper)];
    } d;

    size_t size; /* Size stored in upper bits, lowest bit specifies whether string is stored in `tight` (0) or `helper` (1) */
};

#ifdef C99
extern inline int cc_s_push_back(HString string, char data);
extern inline int cc_s_push_back_n(HString string, char data, size_t count);
extern inline int cc_s_push_back_cstring(HString string, const char *data);
extern inline int cc_s_push_back_cstring_n(HString string, const char *data, size_t count);
#else
int cc_s_push_back(HString string, char data) {
    return cc_s_insert(string, cc_s_size_of(string), data);
}
int cc_s_push_back_n(HString string, char data, size_t count) {
    return cc_s_insert_n(string, cc_s_size_of(string), data, count);
}
int cc_s_push_back_cstring(HString string, const char *data) {
    return cc_s_insert_cstring(string, cc_s_size_of(string), data);
}
int cc_s_push_back_cstring_n(HString string, const char *data, size_t count) {
    return cc_s_insert_cstring_n(string, cc_s_size_of(string), data, count);
}

int cc_s_pop_back(HString string) {
    return cc_s_erase(string, cc_s_size_of(string) - 1);
}
int cc_s_pop_back_n(HString string, size_t count) {
    return cc_s_erase_n(string, cc_s_size_of(string) - count, count);
}
#endif

size_t cc_s_sizeof()
{
    return sizeof(struct String);
}

HString cc_s_init()
{
    return CALLOC(cc_s_sizeof(), 1);
}

int cc_s_init_at(void *buf, size_t buffer_size)
{
    if (buffer_size < cc_s_sizeof())
        CC_BAD_PARAM_HANDLER("buffer size too small");

    memset(buf, 0, buffer_size);
    return CC_OK;
}

static int cc_s_is_tight(HString str)
{
    return ~str->size & 1;
}

static void cc_s_set_tight(HString str, int tight)
{
    if (tight)
        str->size &= SIZE_MAX << 1;
    else
        str->size |= 1;
}

static void cc_s_set_size(HString str, size_t size)
{
    str->size = (size << 1) | (str->size & 1);
}

static int cc_s_valid_type(HElementData el)
{
    return cc_el_type(el) == El_Char ||
            cc_el_type(el) == El_SignedChar ||
            cc_el_type(el) == El_UnsignedChar;
}

HString cc_s_grow(HString str, size_t size)
{
    size_t capacity = cc_s_capacity_of(str);
    /* Do we really need to grow? */
    if (capacity >= size)
        return str;

    size_t new_size = MAX(size, capacity + (capacity >> 1));

    /* Allocate storage */
    char *old_data = cc_s_is_tight(str)? NULL: str->d.helper.data;
    char *new_data = REALLOC(old_data, new_size, 1);
    if (new_data == NULL)
        return NULL; /* Leave `str` unchanged on failure to allocate */

    /* Then copy from small-string-optimization if necessary */
    if (cc_s_is_tight(str))
        memcpy(new_data, str->d.tight, cc_s_size_of(str));

    str->d.helper.data = new_data;
    str->d.helper.capacity = new_size;
    cc_s_set_tight(str, 0);

    return str;
}

static void cc_s_move(HString str, size_t src, size_t dst, size_t count)
{
    if (count)
    {
        char *raw = cc_s_raw(str);
        memmove(raw + dst, raw + src, count);
    }
}

HString cc_s_copy(HString str)
{
    HString new_str = cc_s_init();
    if (!new_str)
        return NULL;

    if (cc_s_grow(new_str, cc_s_size_of(str)) == NULL)
    {
        cc_s_destroy(new_str); /* Nothing initialized yet, so don't pass `destruct` */
        return NULL;
    }
    cc_s_set_size(new_str, cc_s_size_of(str)); /* cc_s_grow() doesn't set the size */

    memcpy(cc_s_raw(new_str), cc_s_raw(str), cc_s_size_of(str));

    return new_str;
}

void cc_s_swap(HString lhs, HString rhs)
{
    struct String temp = *lhs;
    *lhs = *rhs;
    *rhs = temp;
}

int cc_s_insert_n(HString str, size_t before, char data, size_t len)
{
    if (before > cc_s_size_of(str))
        CC_BAD_PARAM_HANDLER("element out-of-bounds");

    if (cc_s_grow(str, cc_s_size_of(str) + len) == NULL)
        CC_NO_MEM_HANDLER("out of memory");

    if (len == 0)
        return CC_OK;

    /* Move string so we can insert at `before` */
    cc_s_move(str, before, before + len, cc_s_size_of(str) - before);

    /* Copy */
    memset(cc_s_raw(str) + before, data, len);

    /* Update str size */
    cc_s_set_size(str, cc_s_size_of(str) + len);

    return CC_OK;
}

int cc_s_insert(HString str, size_t before, char data) {
    return cc_s_insert_n(str, before, data, 1);
}

int cc_s_insert_cstring_n(HString str, size_t before, const char *data, size_t len)
{
    if (before > cc_s_size_of(str))
        CC_BAD_PARAM_HANDLER("element out-of-bounds");

    if (cc_s_grow(str, cc_s_size_of(str) + len) == NULL)
        CC_NO_MEM_HANDLER("out of memory");

    if (len == 0)
        return CC_OK;

    /* Move string so we can insert at `before` */
    cc_s_move(str, before, before + len, cc_s_size_of(str) - before);

    /* Copy */
    memcpy(cc_s_raw(str) + before, data, len);

    /* Update str size */
    cc_s_set_size(str, cc_s_size_of(str) + len);

    return CC_OK;
}

int cc_s_insert_cstring(HString str, size_t before, const char *data) {
    return cc_s_insert_cstring_n(str, before, data, strlen(data));
}

int cc_s_erase(HString string, size_t element) {
    return cc_s_erase_n(string, element, 1);
}

int cc_s_erase_n(HString str, size_t element, size_t count)
{
    if (element >= cc_s_size_of(str))
        CC_BAD_PARAM_HANDLER("element out-of-bounds");

    count = MIN(count, cc_s_size_of(str) - element);

    if (count)
    {
        /* Move string over the top of the element we got rid of */
        cc_s_move(str, element + count, element, cc_s_size_of(str) - element - count);
        /* Decrease str size */
        cc_s_set_size(str, cc_s_size_of(str) - count);
    }

    return CC_OK;
}

/* TODO */
int cc_s_find(HString str, Iterator start, unsigned flags, HConstElementData data, StringCompareCallback compare, Iterator *out)
{
    return CC_OK;
}

void cc_s_reverse(HString str)
{
    if (cc_s_size_of(str) == 0)
        return;

    size_t min = 0, max = cc_s_size_of(str) - 1;

    while (min < max)
    {
        memswap(cc_s_raw(str) + min, cc_s_raw(str) + max, 1);

        ++min;
        --max;
    }
}

size_t cc_s_size_of(HString str)
{
    return str->size >> 1;
}

size_t cc_s_capacity_of(HString str)
{
    return cc_s_is_tight(str)? sizeof(struct StringHelper): str->d.helper.capacity;
}

char *cc_s_raw(HString str)
{
    return cc_s_is_tight(str)? str->d.tight: str->d.helper.data;
}

Iterator cc_s_begin(HString str)
{
    return cc_s_size_of(str) == 0? NULL: cc_s_raw(str);
}

Iterator cc_s_rbegin(HString str)
{
    return cc_s_size_of(str) == 0? NULL: cc_s_raw(str) + (cc_s_size_of(str) - 1);
}

Iterator cc_s_next(HString str, Iterator node)
{
    if (node == NULL || node == cc_s_raw(str) + (cc_s_size_of(str) - 1))
        return NULL;

    return (char *) node + 1;
}

Iterator cc_s_rnext(HString str, Iterator node)
{
    if (node == NULL || node == cc_s_raw(str))
        return NULL;

    return (char *) node - 1;
}

int cc_s_node_data(HString str, Iterator element, HElementData out)
{
    (void) str;

    if (cc_s_valid_type(out))
        CC_TYPE_MISMATCH_HANDLER("cannot get element", /*expected*/ El_Char, /*actual*/ cc_el_type(out));

    *cc_el_storage_location_ptr(out) = element;
    return CC_OK;
}

int cc_s_compare(HString lhs, HString rhs, StringCompareCallback cmp)
{
    int comparison;

    if (cmp)
    {
        Iterator lit = cc_s_begin(lhs);
        Iterator rit = cc_s_begin(rhs);

        for (; lit && rit; lit = cc_s_next(lhs, lit), rit = cc_s_next(rhs, rit))
        {
            comparison = cmp(*((char *) lit), *((char *) rit));
            if (comparison != 0)
                return comparison;
        }
    }
    else
    {
        comparison = memcmp(cc_s_raw(lhs), cc_s_raw(rhs), MIN(cc_s_size_of(lhs), cc_s_size_of(rhs)));
        if (comparison != 0)
            return comparison;
    }

    if (cc_s_size_of(rhs) > cc_s_size_of(lhs))
        comparison = -1; /* lhs is shorter */
    else if (cc_s_size_of(lhs) > cc_s_size_of(rhs))
        comparison = 1; /* rhs is shorter */
    else
        comparison = 0;

    return comparison;
}

void cc_s_clear(HString str)
{
    cc_s_set_size(str, 0);
}

void cc_s_destroy_at(HString str)
{
    if (!cc_s_is_tight(str))
        FREE(str->d.helper.data);
}

void cc_s_destroy(HString str)
{
    cc_s_destroy_at(str);
    FREE(str);
}

char *cc_s_to_cstring(HString str)
{
    if (cc_s_size_of(str) == 0)
        return "";

    if (!cc_s_grow(str, cc_s_size_of(str) + 1))
        return NULL;

    cc_s_raw(str)[cc_s_size_of(str)] = 0; /* Set last character to NUL */

    return cc_s_raw(str);
}

int cc_s_assign_cstring_n(HString str, const char *data, size_t len)
{
    if (!cc_s_grow(str, len))
        CC_NO_MEM_HANDLER("out of memory");

    cc_s_set_size(str, len);
    memcpy(cc_s_raw(str), data, len);

    return CC_OK;
}

int cc_s_assign_cstring(HString str, const char *data)
{
    return cc_s_assign_cstring_n(str, data, strlen(data));
}

#ifdef __cplusplus
}
#endif
