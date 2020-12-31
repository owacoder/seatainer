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

#include <time.h>

static const CommonContainerBase container_base_empty_recipe_ = {
    .copier = NULL,
    .compare = NULL,
    .deleter = NULL,
    .parse = NULL,
    .serialize = NULL,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .size = 0,
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
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .size = sizeof(void *),
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_voidptr_recipe(void) {
    return &container_base_voidptr_recipe_;
}

_Bool *container_base_copy_boolean(const _Bool *v) {
    return generic_pod_copy_alloc(v, sizeof(*v));
}

int container_base_compare_boolean(const _Bool *a, const _Bool *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_boolean_recipe_ = {
    .copier = (Copier) container_base_copy_boolean,
    .compare = (Compare) container_base_compare_boolean,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_boolean,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .size = sizeof(_Bool),
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_boolean_recipe(void) {
    return &container_base_boolean_recipe_;
}

signed char *container_base_copy_char(const signed char *v) {
    return generic_pod_copy_alloc(v, sizeof(*v));
}

int container_base_compare_char(const signed char *a, const signed char *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_char_recipe_ = {
    .copier = (Copier) container_base_copy_char,
    .compare = (Compare) container_base_compare_char,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_char,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .size = sizeof(signed char),
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_char_recipe(void) {
    return &container_base_char_recipe_;
}

unsigned char *container_base_copy_uchar(const unsigned char *v) {
    return generic_pod_copy_alloc(v, sizeof(*v));
}

int container_base_compare_uchar(const unsigned char *a, const unsigned char *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_uchar_recipe_ = {
    .copier = (Copier) container_base_copy_uchar,
    .compare = (Compare) container_base_compare_uchar,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_char,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .size = sizeof(unsigned char),
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_uchar_recipe(void) {
    return &container_base_uchar_recipe_;
}

signed short *container_base_copy_short(const signed short *v) {
    return generic_pod_copy_alloc(v, sizeof(*v));
}

int container_base_compare_short(const signed short *a, const signed short *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_short_recipe_ = {
    .copier = (Copier) container_base_copy_short,
    .compare = (Compare) container_base_compare_short,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_short,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .size = sizeof(short),
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_short_recipe(void) {
    return &container_base_short_recipe_;
}

unsigned short *container_base_copy_ushort(const unsigned short *v) {
    return generic_pod_copy_alloc(v, sizeof(*v));
}

int container_base_compare_ushort(const unsigned short *a, const unsigned short *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_ushort_recipe_ = {
    .copier = (Copier) container_base_copy_ushort,
    .compare = (Compare) container_base_compare_ushort,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_ushort,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .size = sizeof(unsigned short),
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_ushort_recipe(void) {
    return &container_base_ushort_recipe_;
}

int *container_base_copy_int(const int *v) {
    return generic_pod_copy_alloc(v, sizeof(*v));
}

int container_base_compare_int(const int *a, const int *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_int_recipe_ = {
    .copier = (Copier) container_base_copy_int,
    .compare = (Compare) container_base_compare_int,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_int,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .size = sizeof(int),
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_int_recipe(void) {
    return &container_base_int_recipe_;
}

int *container_base_copy_uint(const unsigned int *v) {
    return generic_pod_copy_alloc(v, sizeof(*v));
}

int container_base_compare_uint(const unsigned int *a, const unsigned int *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_uint_recipe_ = {
    .copier = (Copier) container_base_copy_uint,
    .compare = (Compare) container_base_compare_uint,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_uint,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .size = sizeof(unsigned int),
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_uint_recipe(void) {
    return &container_base_uint_recipe_;
}

long *container_base_copy_long(const long *v) {
    return generic_pod_copy_alloc(v, sizeof(*v));
}

int container_base_compare_long(const long *a, const long *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_long_recipe_ = {
    .copier = (Copier) container_base_copy_long,
    .compare = (Compare) container_base_compare_long,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_long,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .size = sizeof(long),
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_long_recipe(void) {
    return &container_base_long_recipe_;
}

unsigned long *container_base_copy_ulong(const unsigned long *v) {
    return generic_pod_copy_alloc(v, sizeof(*v));
}

int container_base_compare_ulong(const unsigned long *a, const unsigned long *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_ulong_recipe_ = {
    .copier = (Copier) container_base_copy_ulong,
    .compare = (Compare) container_base_compare_ulong,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_ulong,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .size = sizeof(unsigned long),
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_ulong_recipe(void) {
    return &container_base_ulong_recipe_;
}

long long *container_base_copy_long_long(const long long *v) {
    return generic_pod_copy_alloc(v, sizeof(*v));
}

int container_base_compare_long_long(const long long *a, const long long *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_long_long_recipe_ = {
    .copier = (Copier) container_base_copy_long_long,
    .compare = (Compare) container_base_compare_long_long,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_long_long,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .size = sizeof(long long),
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_long_long_recipe(void) {
    return &container_base_long_long_recipe_;
}

unsigned long long *container_base_copy_ulong_long(const unsigned long long *v) {
    return generic_pod_copy_alloc(v, sizeof(*v));
}

int container_base_compare_ulong_long(const unsigned long long *a, const unsigned long long *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_ulong_long_recipe_ = {
    .copier = (Copier) container_base_copy_ulong_long,
    .compare = (Compare) container_base_compare_ulong_long,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_ulong_long,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .size = sizeof(unsigned long long),
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_ulong_long_recipe(void) {
    return &container_base_ulong_long_recipe_;
}

size_t *container_base_copy_size_t(const size_t *v) {
    return generic_pod_copy_alloc(v, sizeof(*v));
}

int container_base_compare_size_t(const size_t *a, const size_t *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_size_t_recipe_ = {
    .copier = (Copier) container_base_copy_size_t,
    .compare = (Compare) container_base_compare_size_t,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_size_t,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .size = sizeof(size_t),
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_size_t_recipe(void) {
    return &container_base_size_t_recipe_;
}

float *container_base_copy_float(const float *v) {
    return generic_pod_copy_alloc(v, sizeof(*v));
}

int container_base_compare_float(const float *a, const float *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_float_recipe_ = {
    .copier = (Copier) container_base_copy_float,
    .compare = (Compare) container_base_compare_float,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_float,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .size = sizeof(float),
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_float_recipe(void) {
    return &container_base_float_recipe_;
}

double *container_base_copy_double(const double *v) {
    return generic_pod_copy_alloc(v, sizeof(*v));
}

int container_base_compare_double(const double *a, const double *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_double_recipe_ = {
    .copier = (Copier) container_base_copy_double,
    .compare = (Compare) container_base_compare_double,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_double,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .size = sizeof(double),
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_double_recipe(void) {
    return &container_base_double_recipe_;
}

long double *container_base_copy_long_double(const long double *v) {
    return generic_pod_copy_alloc(v, sizeof(*v));
}

int container_base_compare_long_double(const long double *a, const long double *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_long_double_recipe_ = {
    .copier = (Copier) container_base_copy_long_double,
    .compare = (Compare) container_base_compare_long_double,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_long_double,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .size = sizeof(long double),
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_long_double_recipe(void) {
    return &container_base_long_double_recipe_;
}

clock_t *container_base_copy_clock_t(const clock_t *v) {
    return generic_pod_copy_alloc(v, sizeof(*v));
}

int container_base_compare_clock_t(const clock_t *a, const clock_t *b) {
    return (*a > *b) - (*a < *b);
}

static const CommonContainerBase container_base_clock_t_recipe_ = {
    .copier = (Copier) container_base_copy_clock_t,
    .compare = (Compare) container_base_compare_clock_t,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_clock_t,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .size = sizeof(clock_t),
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_clock_t_recipe(void) {
    return &container_base_clock_t_recipe_;
}

struct tm *container_base_copy_tm(const struct tm *v) {
    return generic_pod_copy_alloc(v, sizeof(*v));
}

int container_base_compare_tm(const struct tm *a, const struct tm *b) {
    if (a->tm_year != b->tm_year)
        return (a->tm_year > b->tm_year) - (a->tm_year < b->tm_year);

    if (a->tm_mon != b->tm_mon)
        return (a->tm_mon > b->tm_mon) - (a->tm_mon < b->tm_mon);

    if (a->tm_mday != b->tm_mday)
        return (a->tm_mday > b->tm_mday) - (a->tm_mday < b->tm_mday);

    if (a->tm_hour != b->tm_hour)
        return (a->tm_hour > b->tm_hour) - (a->tm_hour < b->tm_hour);

    if (a->tm_min != b->tm_min)
        return (a->tm_min > b->tm_min) - (a->tm_min < b->tm_min);

    if (a->tm_sec != b->tm_sec)
        return (a->tm_sec > b->tm_sec) - (a->tm_sec < b->tm_sec);

    return 0;
}

static const CommonContainerBase container_base_tm_recipe_ = {
    .copier = (Copier) container_base_copy_tm,
    .compare = (Compare) container_base_compare_tm,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_tm,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .size = sizeof(struct tm),
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_tm_recipe(void) {
    return &container_base_tm_recipe_;
}

static const CommonContainerBase container_base_cstring_recipe_ = {
    .copier = (Copier) strdup_alloc,
    .compare = (Compare) strcmp,
    .deleter = (Deleter) FREE,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_cstring,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
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
    .serialize = (Serializer) io_serialize_binary,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
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
    .serialize = (Serializer) io_serialize_variant,
    .collection_size = NULL,
    .collection_begin = NULL,
    .collection_next = NULL,
    .collection_get_key = NULL,
    .collection_get_value = NULL,
    .collection_find = {0},
    .collection_insert = {0},
    .collection_erase = NULL,
    .collection_replace = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_variant_recipe() {
    return &container_base_variant_recipe_;
}

static Iterator genericlist_collection_erase(GenericList list, Iterator it) {
    if (it == NULL) {
        genericlist_clear(list);
        return NULL;
    }

    return genericlist_remove_at_iterator(list, it);
}

static const CommonContainerBase container_base_genericlist_recipe_ = {
    .copier = (Copier) genericlist_copy,
    .compare = (Compare) genericlist_compare,
    .deleter = (Deleter) genericlist_destroy,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_utf8,
    .collection_size = (CollectionSize) genericlist_size,
    .collection_begin = (CollectionBegin) genericlist_begin,
    .collection_next = (CollectionNext) genericlist_next,
    .collection_get_key = NULL,
    .collection_get_value = (CollectionValue) genericlist_value_of,
    .collection_find.list = (CollectionListFind) genericlist_find_iterator,
    .collection_insert.list = (CollectionListInsertMove) genericlist_insert_move_iterator,
    .collection_erase = (CollectionErase) genericlist_collection_erase,
    .collection_replace = (CollectionReplaceMove) genericlist_replace_move_iterator,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_genericlist_recipe() {
    return &container_base_genericlist_recipe_;
}

static Iterator genericmap_collection_erase(GenericMap map, Iterator it) {
    if (it == NULL) {
        genericmap_clear(map);
        return NULL;
    }

    return genericmap_erase(map, it);
}

static const CommonContainerBase container_base_genericmap_recipe_ = {
    .copier = (Copier) genericmap_copy,
    .compare = (Compare) genericmap_compare,
    .deleter = (Deleter) genericmap_destroy,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_utf8,
    .collection_size = (CollectionSize) genericmap_size,
    .collection_begin = (CollectionBegin) genericmap_begin,
    .collection_next = (CollectionNext) genericmap_next,
    .collection_get_key = (CollectionKey) genericmap_key_of,
    .collection_get_value = (CollectionValue) genericmap_value_of,
    .collection_find.key_value = (CollectionKeyValueFind) genericmap_find,
    .collection_insert.key_value = (CollectionKeyValueInsertMove) genericmap_insert_move_key,
    .collection_erase = (CollectionErase) genericmap_collection_erase,
    .collection_replace = (CollectionReplaceMove) genericmap_replace_move,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_genericmap_recipe() {
    return &container_base_genericmap_recipe_;
}

static int genericset_collection_insert(GenericSet set, void *item, Iterator it) {
    UNUSED(it)

    return genericset_add_move(set, item);
}

static Iterator genericset_collection_erase(GenericSet set, Iterator it) {
    if (it == NULL) {
        genericset_clear(set);
        return NULL;
    }

    return genericset_erase(set, it);
}

static const CommonContainerBase container_base_genericset_recipe_ = {
    .copier = (Copier) genericset_copy,
    .compare = (Compare) genericset_compare,
    .deleter = (Deleter) genericset_destroy,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_utf8,
    .collection_size = (CollectionSize) genericset_size,
    .collection_begin = (CollectionBegin) genericset_begin,
    .collection_next = (CollectionNext) genericset_next,
    .collection_get_key = NULL,
    .collection_get_value = (CollectionValue) genericset_value_of,
    .collection_find.list = (CollectionListFind) genericset_find,
    .collection_insert.list = (CollectionListInsertMove) genericset_collection_insert,
    .collection_erase = (CollectionErase) genericset_collection_erase,
    .collection_replace = NULL,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_genericset_recipe() {
    return &container_base_genericset_recipe_;
}

static int genericlinkedlist_collection_insert(GenericLinkedList list, void *item, Iterator it) {
    return genericlinkedlist_insert_move(list, item, genericlinkedlist_previous(list, it));
}

static Iterator genericlinkedlist_collection_erase(GenericLinkedList list, Iterator it) {
    if (it == NULL) {
        genericlinkedlist_clear(list);
        return NULL;
    }

    Iterator next = genericlinkedlist_next(list, it);
    genericlinkedlist_remove_after(list, genericlinkedlist_previous(list, it));
    return next;
}

static const CommonContainerBase container_base_genericlinkedlist_recipe_ = {
    .copier = (Copier) genericlinkedlist_copy,
    .compare = (Compare) genericlinkedlist_compare,
    .deleter = (Deleter) genericlinkedlist_destroy,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_utf8,
    .collection_size = (CollectionSize) genericlinkedlist_size,
    .collection_begin = (CollectionBegin) genericlinkedlist_begin,
    .collection_next = (CollectionNext) genericlinkedlist_next,
    .collection_get_key = NULL,
    .collection_get_value = (CollectionValue) genericlinkedlist_value_of,
    .collection_find.list = (CollectionListFind) genericlinkedlist_find_from,
    .collection_insert.list = (CollectionListInsertMove) genericlinkedlist_collection_insert,
    .collection_erase = (CollectionErase) genericlinkedlist_collection_erase,
    .collection_replace = (CollectionReplaceMove) genericlinkedlist_replace_move_at,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = NULL
};

const CommonContainerBase *container_base_genericlinkedlist_recipe() {
    return &container_base_genericlinkedlist_recipe_;
}

static const CommonContainerBase container_base_variantlist_recipe_ = {
    .copier = (Copier) genericlist_copy,
    .compare = (Compare) genericlist_compare,
    .deleter = (Deleter) genericlist_destroy,
    .parse = NULL,
    .serialize = (Serializer) io_serialize_utf8,
    .collection_size = (CollectionSize) genericlist_size,
    .collection_begin = (CollectionBegin) genericlist_begin,
    .collection_next = (CollectionNext) genericlist_next,
    .collection_get_key = NULL,
    .collection_get_value = (CollectionValue) genericlist_value_of,
    .collection_find.list = (CollectionListFind) genericlist_find_iterator,
    .collection_insert.list = (CollectionListInsertMove) genericlist_insert_move_iterator,
    .collection_erase = (CollectionErase) genericlist_collection_erase,
    .collection_replace = (CollectionReplaceMove) genericlist_replace_move_iterator,
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
    .serialize = (Serializer) io_serialize_utf8,
    .collection_size = (CollectionSize) genericmap_size,
    .collection_begin = (CollectionBegin) genericmap_begin,
    .collection_next = (CollectionNext) genericmap_next,
    .collection_get_key = (CollectionKey) genericmap_key_of,
    .collection_get_value = (CollectionValue) genericmap_value_of,
    .collection_find.key_value = (CollectionKeyValueFind) genericmap_find,
    .collection_insert.key_value = (CollectionKeyValueInsertMove) genericmap_insert_move_key,
    .collection_erase = (CollectionErase) genericmap_collection_erase,
    .collection_replace = (CollectionReplaceMove) genericmap_replace_move,
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
    .serialize = (Serializer) io_serialize_utf8,
    .collection_size = (CollectionSize) genericset_size,
    .collection_begin = (CollectionBegin) genericset_begin,
    .collection_next = (CollectionNext) genericset_next,
    .collection_get_key = NULL,
    .collection_get_value = (CollectionValue) genericset_value_of,
    .collection_find.list = (CollectionListFind) genericset_find,
    .collection_insert.list = (CollectionListInsertMove) genericset_collection_insert,
    .collection_erase = (CollectionErase) genericset_collection_erase,
    .collection_replace = NULL,
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
    .serialize = (Serializer) io_serialize_utf8,
    .collection_size = (CollectionSize) stringlist_size,
    .collection_begin = (CollectionBegin) stringlist_begin,
    .collection_next = (CollectionNext) stringlist_next,
    .collection_get_key = NULL,
    .collection_get_value = (CollectionValue) stringlist_value_of,
    .collection_find.list = (CollectionListFind) genericlist_find_iterator,
    .collection_insert.list = (CollectionListInsertMove) genericlist_insert_move_iterator,
    .collection_erase = (CollectionErase) genericlist_collection_erase,
    .collection_replace = (CollectionReplaceMove) genericlist_replace_move_iterator,
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
    .serialize = (Serializer) io_serialize_utf8,
    .collection_size = (CollectionSize) stringmap_size,
    .collection_begin = (CollectionBegin) stringmap_begin,
    .collection_next = (CollectionNext) stringmap_next,
    .collection_get_key = (CollectionKey) stringmap_key_of,
    .collection_get_value = (CollectionValue) stringmap_value_of,
    .collection_find.key_value = (CollectionKeyValueFind) genericmap_find,
    .collection_insert.key_value = (CollectionKeyValueInsertMove) genericmap_insert_move_key,
    .collection_erase = (CollectionErase) genericmap_collection_erase,
    .collection_replace = (CollectionReplaceMove) genericmap_replace_move,
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
    .serialize = (Serializer) io_serialize_utf8,
    .collection_size = (CollectionSize) stringset_size,
    .collection_begin = (CollectionBegin) stringset_begin,
    .collection_next = (CollectionNext) stringset_next,
    .collection_get_key = NULL,
    .collection_get_value = (CollectionValue) stringset_value_of,
    .collection_find.list = (CollectionListFind) genericset_find,
    .collection_insert.list = (CollectionListInsertMove) genericset_collection_insert,
    .collection_erase = (CollectionErase) genericset_collection_erase,
    .collection_replace = NULL,
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
    .serialize = (Serializer) io_serialize_utf8,
    .collection_size = (CollectionSize) genericlist_size,
    .collection_begin = (CollectionBegin) genericlist_begin,
    .collection_next = (CollectionNext) genericlist_next,
    .collection_get_key = NULL,
    .collection_get_value = (CollectionValue) genericlist_value_of,
    .collection_find.list = (CollectionListFind) genericlist_find_iterator,
    .collection_insert.list = (CollectionListInsertMove) genericlist_insert_move_iterator,
    .collection_erase = (CollectionErase) genericlist_collection_erase,
    .collection_replace = (CollectionReplaceMove) genericlist_replace_move_iterator,
    .dynamic = 0,
    .key_child = NULL,
    .value_child = (CommonContainerBase *) &container_base_binary_recipe_
};

const CommonContainerBase *container_base_binarylist_recipe() {
    return &container_base_binarylist_recipe_;
}
