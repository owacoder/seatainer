/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "recipes.h"
#include "ccio.h"
#include "utility.h"

#include "../containers.h"
#include "../container_io.h"

static const CommonContainerBase container_base_empty_recipe_ = {
    .copier = NULL,
    .compare = NULL,
    .deleter = NULL,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_empty_recipe(void) {
    return &container_base_empty_recipe_;
}

static int container_base_voidptr_compare(const void *a, const void *b) {
    uintptr_t ia = (uintptr_t) a;
    uintptr_t ib = (uintptr_t) b;

    return (ia > ib) - (ia < ib);
}

static const CommonContainerBase container_base_voidptr_recipe_ = {
    .copier = generic_identitycopy,
    .compare = container_base_voidptr_compare,
    .deleter = NULL,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_voidptr_recipe(void) {
    return &container_base_voidptr_recipe_;
}

_Bool *container_base_copy_boolean(const _Bool *v) {
    return generic_pod_alloc(v, sizeof(*v));
}

int container_base_compare_boolean(const _Bool *a, const _Bool *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_boolean_recipe_ = {
    .copier = (Copier) container_base_copy_boolean,
    .compare = (Compare) container_base_compare_boolean,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_boolean_recipe(void) {
    return &container_base_boolean_recipe_;
}

signed char *container_base_copy_char(const signed char *v) {
    return generic_pod_alloc(v, sizeof(*v));
}

int container_base_compare_char(const signed char *a, const signed char *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_char_recipe_ = {
    .copier = (Copier) container_base_copy_char,
    .compare = (Compare) container_base_compare_char,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_char_recipe(void) {
    return &container_base_char_recipe_;
}

unsigned char *container_base_copy_uchar(const unsigned char *v) {
    return generic_pod_alloc(v, sizeof(*v));
}

int container_base_compare_uchar(const unsigned char *a, const unsigned char *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_uchar_recipe_ = {
    .copier = (Copier) container_base_copy_uchar,
    .compare = (Compare) container_base_compare_uchar,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_uchar_recipe(void) {
    return &container_base_uchar_recipe_;
}

signed short *container_base_copy_short(const signed short *v) {
    return generic_pod_alloc(v, sizeof(*v));
}

int container_base_compare_short(const signed short *a, const signed short *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_short_recipe_ = {
    .copier = (Copier) container_base_copy_short,
    .compare = (Compare) container_base_compare_short,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_short_recipe(void) {
    return &container_base_short_recipe_;
}

unsigned short *container_base_copy_ushort(const unsigned short *v) {
    return generic_pod_alloc(v, sizeof(*v));
}

int container_base_compare_ushort(const unsigned short *a, const unsigned short *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_ushort_recipe_ = {
    .copier = (Copier) container_base_copy_ushort,
    .compare = (Compare) container_base_compare_ushort,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_ushort_recipe(void) {
    return &container_base_ushort_recipe_;
}

int *container_base_copy_int(const int *v) {
    return generic_pod_alloc(v, sizeof(*v));
}

int container_base_compare_int(const int *a, const int *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_int_recipe_ = {
    .copier = (Copier) container_base_copy_int,
    .compare = (Compare) container_base_compare_int,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_int_recipe(void) {
    return &container_base_int_recipe_;
}

int *container_base_copy_uint(const unsigned int *v) {
    return generic_pod_alloc(v, sizeof(*v));
}

int container_base_compare_uint(const unsigned int *a, const unsigned int *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_uint_recipe_ = {
    .copier = (Copier) container_base_copy_uint,
    .compare = (Compare) container_base_compare_uint,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_uint_recipe(void) {
    return &container_base_uint_recipe_;
}

long *container_base_copy_long(const long *v) {
    return generic_pod_alloc(v, sizeof(*v));
}

int container_base_compare_long(const long *a, const long *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_long_recipe_ = {
    .copier = (Copier) container_base_copy_long,
    .compare = (Compare) container_base_compare_long,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_long_recipe(void) {
    return &container_base_long_recipe_;
}

unsigned long *container_base_copy_ulong(const unsigned long *v) {
    return generic_pod_alloc(v, sizeof(*v));
}

int container_base_compare_ulong(const unsigned long *a, const unsigned long *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_ulong_recipe_ = {
    .copier = (Copier) container_base_copy_ulong,
    .compare = (Compare) container_base_compare_ulong,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_ulong_recipe(void) {
    return &container_base_ulong_recipe_;
}

long long *container_base_copy_long_long(const long long *v) {
    return generic_pod_alloc(v, sizeof(*v));
}

int container_base_compare_long_long(const long long *a, const long long *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_long_long_recipe_ = {
    .copier = (Copier) container_base_copy_long_long,
    .compare = (Compare) container_base_compare_long_long,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_long_long_recipe(void) {
    return &container_base_long_long_recipe_;
}

unsigned long long *container_base_copy_ulong_long(const unsigned long long *v) {
    return generic_pod_alloc(v, sizeof(*v));
}

int container_base_compare_ulong_long(const unsigned long long *a, const unsigned long long *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_ulong_long_recipe_ = {
    .copier = (Copier) container_base_copy_ulong_long,
    .compare = (Compare) container_base_compare_ulong_long,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_ulong_long_recipe(void) {
    return &container_base_ulong_long_recipe_;
}

float *container_base_copy_float(const float *v) {
    return generic_pod_alloc(v, sizeof(*v));
}

int container_base_compare_float(const float *a, const float *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_float_recipe_ = {
    .copier = (Copier) container_base_copy_float,
    .compare = (Compare) container_base_compare_float,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_float_recipe(void) {
    return &container_base_float_recipe_;
}

double *container_base_copy_double(const double *v) {
    return generic_pod_alloc(v, sizeof(*v));
}

int container_base_compare_double(const double *a, const double *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_double_recipe_ = {
    .copier = (Copier) container_base_copy_double,
    .compare = (Compare) container_base_compare_double,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_double_recipe(void) {
    return &container_base_double_recipe_;
}

long double *container_base_copy_long_double(const long double *v) {
    return generic_pod_alloc(v, sizeof(*v));
}

int container_base_compare_long_double(const long double *a, const long double *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_long_double_recipe_ = {
    .copier = (Copier) container_base_copy_long_double,
    .compare = (Compare) container_base_compare_long_double,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_long_double_recipe(void) {
    return &container_base_long_double_recipe_;
}

static const CommonContainerBase container_base_cstring_recipe_ = {
    .copier = (Copier) strdup_alloc,
    .compare = (Compare) strcmp,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_cstring_recipe(void) {
    return &container_base_cstring_recipe_;
}

static const CommonContainerBase container_base_binary_recipe_ = {
    .copier = (Copier) binary_copy,
    .compare = (Compare) binary_compare,
    .deleter = (Deleter) binary_destroy,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_binary_recipe(void) {
    return &container_base_binary_recipe_;
}

static const CommonContainerBase container_base_variant_recipe_ = {
    .copier = (Copier) variant_copy,
    .compare = (Compare) variant_compare,
    .deleter = (Deleter) variant_destroy,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_variant_recipe() {
    return &container_base_variant_recipe_;
}

static const CommonContainerBase container_base_genericlist_recipe_ = {
    .copier = (Copier) genericlist_copy,
    .compare = (Compare) genericlist_compare,
    .deleter = (Deleter) genericlist_destroy,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = (IteratorBegin) genericlist_begin,
    .collection_next = (IteratorNext) genericlist_next,
    .collection_get_key = NULL,
    .collection_get_value = (IteratorValue) genericlist_value_of,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_genericlist_recipe() {
    return &container_base_genericlist_recipe_;
}

static const CommonContainerBase container_base_genericmap_recipe_ = {
    .copier = (Copier) genericmap_copy,
    .compare = (Compare) genericmap_compare,
    .deleter = (Deleter) genericmap_destroy,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = (IteratorBegin) genericmap_begin,
    .collection_next = (IteratorNext) genericmap_next,
    .collection_get_key = (IteratorKey) genericmap_key_of,
    .collection_get_value = (IteratorValue) genericmap_value_of,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_genericmap_recipe() {
    return &container_base_genericmap_recipe_;
}

static const CommonContainerBase container_base_genericset_recipe_ = {
    .copier = (Copier) genericset_copy,
    .compare = (Compare) genericset_compare,
    .deleter = (Deleter) genericset_destroy,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = (IteratorBegin) genericset_begin,
    .collection_next = (IteratorNext) genericset_next,
    .collection_get_key = NULL,
    .collection_get_value = (IteratorValue) genericset_value_of,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_genericset_recipe() {
    return &container_base_genericset_recipe_;
}

static const CommonContainerBase container_base_variantlist_recipe_ = {
    .copier = (Copier) genericlist_copy,
    .compare = (Compare) genericlist_compare,
    .deleter = (Deleter) genericlist_destroy,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = (IteratorBegin) genericlist_begin,
    .collection_next = (IteratorNext) genericlist_next,
    .collection_get_key = NULL,
    .collection_get_value = (IteratorValue) genericlist_value_of,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = (CommonContainerBase *) &container_base_variant_recipe_
};

const CommonContainerBase *container_base_variantlist_recipe() {
    return &container_base_variantlist_recipe_;
}

static const CommonContainerBase container_base_variantmap_recipe_ = {
    .copier = (Copier) genericmap_copy,
    .compare = (Compare) genericmap_compare,
    .deleter = (Deleter) genericmap_destroy,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = (IteratorBegin) genericmap_begin,
    .collection_next = (IteratorNext) genericmap_next,
    .collection_get_key = (IteratorKey) genericmap_key_of,
    .collection_get_value = (IteratorValue) genericmap_value_of,
    .dynamic = 0,
    .key_child = (CommonContainerBase *) &container_base_cstring_recipe_,
    .value_child = (CommonContainerBase *) &container_base_variant_recipe_
};

const CommonContainerBase *container_base_variantmap_recipe() {
    return &container_base_variantmap_recipe_;
}

static const CommonContainerBase container_base_variantset_recipe_ = {
    .copier = (Copier) genericset_copy,
    .compare = (Compare) genericset_compare,
    .deleter = (Deleter) genericset_destroy,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = (IteratorBegin) genericset_begin,
    .collection_next = (IteratorNext) genericset_next,
    .collection_get_key = NULL,
    .collection_get_value = (IteratorValue) genericset_value_of,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = (CommonContainerBase *) &container_base_variant_recipe_
};

const CommonContainerBase *container_base_variantset_recipe() {
    return &container_base_variantset_recipe_;
}

static const CommonContainerBase container_base_stringlist_recipe_ = {
    .copier = (Copier) stringlist_copy,
    .compare = (Compare) stringlist_compare,
    .deleter = (Deleter) stringlist_destroy,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = (IteratorBegin) stringlist_begin,
    .collection_next = (IteratorNext) stringlist_next,
    .collection_get_key = NULL,
    .collection_get_value = (IteratorValue) stringlist_value_of,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = (CommonContainerBase *) &container_base_cstring_recipe_
};

const CommonContainerBase *container_base_stringlist_recipe() {
    return &container_base_stringlist_recipe_;
}

static const CommonContainerBase container_base_stringmap_recipe_ = {
    .copier = (Copier) stringmap_copy,
    .compare = (Compare) stringmap_compare,
    .deleter = (Deleter) stringmap_destroy,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = (IteratorBegin) stringmap_begin,
    .collection_next = (IteratorNext) stringmap_next,
    .collection_get_key = (IteratorKey) stringmap_key_of,
    .collection_get_value = (IteratorValue) stringmap_value_of,
    .dynamic = 0,
    .key_child = (CommonContainerBase *) &container_base_cstring_recipe_,
    .value_child = (CommonContainerBase *) &container_base_cstring_recipe_
};

const CommonContainerBase *container_base_stringmap_recipe() {
    return &container_base_stringmap_recipe_;
}

static const CommonContainerBase container_base_stringset_recipe_ = {
    .copier = (Copier) stringset_copy,
    .compare = (Compare) stringset_compare,
    .deleter = (Deleter) stringset_destroy,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = (IteratorBegin) stringset_begin,
    .collection_next = (IteratorNext) stringset_next,
    .collection_get_key = NULL,
    .collection_get_value = (IteratorValue) stringset_value_of,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = (CommonContainerBase *) &container_base_cstring_recipe_
};

const CommonContainerBase *container_base_stringset_recipe() {
    return &container_base_stringset_recipe_;
}

static const CommonContainerBase container_base_binarylist_recipe_ = {
    .copier = (Copier) genericlist_copy,
    .compare = (Compare) genericlist_compare,
    .deleter = (Deleter) genericlist_destroy,
    .parse = NULL,
    .serialize = NULL,
    .collection_begin = (IteratorBegin) genericlist_begin,
    .collection_next = (IteratorNext) genericlist_next,
    .collection_get_key = NULL,
    .collection_get_value = (IteratorValue) genericlist_value_of,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = (CommonContainerBase *) &container_base_binary_recipe_
};

const CommonContainerBase *container_base_binarylist_recipe() {
    return &container_base_binarylist_recipe_;
}
