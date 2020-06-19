#ifndef STRINGLIST_H
#define STRINGLIST_H

#include "platforms.h"
#include "seaerror.h"

struct GenericListStruct;
struct BinaryListStruct;

typedef struct VariantStruct *Variant; /* Simple variant type that can hold integers, floating point values, strings, or other containers. Simple conversions can be performed */
typedef struct GenericListStruct *GenericList; /* Simple list of generic pointers */
typedef void *StringList; /* Simple list of NUL-terminated strings */
typedef struct BinaryListStruct *BinaryList; /* Simple list of binary strings or other sized plain-old-data */
typedef void *StringSet; /* Simple exclusive set of NUL-terminated strings, each element only appears once. Elements are in ascending order */
typedef void *BinarySet; /* Simple exclusive set of binary strings or other sized plain-old-data. Elements are in ascending order */
typedef void *GenericMap; /* Ordered map of binary strings, containing generic pointers. Elements are in ascending order */
typedef void *StringMap; /* Simple exclusive map of NUL-terminated string keys with values, each element only appears once. Elements are NUL-terminated strings. Elements are in ascending order */
typedef void *Iterator; /* Simple iterator, does not need to be freed */

typedef struct BinaryStruct {
    char *data;
    size_t length;
} Binary;

typedef int (*Compare)(const void *a, const void *b);
typedef int (*BinaryCompare)(const Binary *a, const Binary *b);
typedef int (*StringCompare)(const char *a, const char *b);
typedef void *(*Copier)(const void *p);
typedef void (*Deleter)(void *p);

/* Basic operations needed:
 *  - Creation, initialization, and copying
 *  - Insertion (at end or in middle)
 *  - Deletion (by index or by element)
 *  - Enumeration
 *  - Search (subset of enumeration)
 *  - Comparison
 *  - Size and clear
 *  - Destruction
 */

enum VariantType {
    VariantNull,
    VariantBoolean,
    VariantInteger,
    VariantFloat,
    VariantString,
    VariantBinary,
    VariantCustom
};

Variant variant_create_null();
Variant variant_create_boolean(int b);
Variant variant_create_int(int value);
Variant variant_create_int64(long long value);
Variant variant_create_float(double value);
Variant variant_create_string(const char *value);
Variant variant_create_string_move(char *value);
/* If binary data is passed to a function that adopts the allocated data (e.g. the `move` functions), the user MUST ensure that data[length] == 0 */
Variant variant_create_binary_string(const char *value, size_t value_len);
Variant variant_create_binary_string_move(char *value, size_t value_len);
Variant variant_create_binary_string_binary(const Binary value);
Variant variant_create_binary_string_binary_move(Binary value);
Variant variant_create_custom(const void *item, Compare compare, Copier copier, Deleter deleter);
Variant variant_create_custom_move(void *item, Compare compare, Copier copier, Deleter deleter);
Variant variant_copy(Variant other);
int variant_compare(Variant lhs, Variant rhs);
int variant_is_null(Variant var);
int variant_is_boolean(Variant var);
int variant_is_int(Variant var);
int variant_is_int64(Variant var);
int variant_is_float(Variant var);
int variant_is_number(Variant var); /* Returns true if int or float */
int variant_is_string(Variant var);
int variant_is_binary(Variant var);
int variant_is_custom(Variant var);
int variant_set_null(Variant var);
int variant_set_boolean(Variant var, int b);
int variant_set_int(Variant var, int value);
int variant_set_int64(Variant var, long long value);
int variant_set_float(Variant var, double value);
int variant_set_string(Variant var, const char *value);
int variant_set_string_move(Variant var, char *value);
int variant_set_binary_string(Variant var, const char *value, size_t value_len);
int variant_set_binary_string_move(Variant var, char *value, size_t value_len);
int variant_set_binary_string_binary(Variant var, const Binary value);
int variant_set_binary_string_binary_move(Variant var, Binary value);
int variant_set_custom_move(Variant var, void *item, Compare compare, Copier copier, Deleter deleter);
int variant_set_custom(Variant var, const void *item, Compare compare, Copier copier, Deleter deleter);
int variant_get_boolean(Variant var);
int variant_get_int(Variant var);
long long variant_get_int64(Variant var);
double variant_get_float(Variant var);
char *variant_get_string(Variant var);
Binary variant_get_binary(Variant var);
void *variant_get_custom(Variant var);
int variant_to_boolean(Variant var, int *error);
int variant_to_int(Variant var, int *error);
long long variant_to_int64(Variant var, int *error);
double variant_to_float(Variant var, int *error);
const char *variant_to_string(Variant var, int *error);
Binary variant_to_binary(Variant var, int *error);
enum VariantType variant_get_type(Variant var);
Compare variant_get_compare_fn(Variant var);
Copier variant_get_copier_fn(Variant var);
Deleter variant_get_deleter_fn(Variant var);
void variant_clear(Variant var);
void variant_destroy(Variant var);

StringSet stringset_create();
StringSet stringset_create_custom(BinaryCompare compare);
StringSet stringset_from_stringlist(StringList list, BinaryCompare compare);
StringSet stringset_copy(StringSet other);
StringSet stringset_union(StringSet a, StringSet b);
StringSet stringset_intersection(StringSet a, StringSet b);
StringSet stringset_subtract(StringSet from, StringSet set_to_subtract);
StringSet stringset_difference(StringSet a, StringSet b);
int stringset_add(StringSet set, const char *item);
int stringset_add_n(StringSet set, const char *item, size_t item_len);
int stringset_contains(StringSet set, const char *item);
void stringset_remove(StringSet set, const char *item);
Iterator stringset_erase(StringSet set, Iterator it);
Iterator stringset_begin(StringSet set);
Iterator stringset_next(StringSet set, Iterator it);
const char *stringset_value_of(StringSet set, Iterator it);
size_t stringset_size(StringSet set);
int stringset_compare(StringSet lhs, StringSet rhs);
BinaryCompare stringset_get_compare_fn(StringSet set);
void stringset_set_compare_fn(StringSet set, BinaryCompare compare);
void stringset_clear(StringSet set);
void stringset_destroy(StringSet set);

BinarySet binaryset_create();
BinarySet binaryset_create_custom(BinaryCompare compare);
BinarySet binaryset_from_binarylist(BinaryList list);
BinarySet binaryset_copy(BinarySet other);
BinarySet binaryset_union(BinarySet a, BinarySet b);
BinarySet binaryset_intersection(BinarySet a, BinarySet b);
BinarySet binaryset_subtract(BinarySet from, BinarySet set_to_subtract);
BinarySet binaryset_difference(BinarySet a, BinarySet b);
int binaryset_add(BinarySet set, const char *item, size_t item_len);
int binaryset_add_binary(BinarySet set, const Binary item);
int binaryset_contains(BinarySet set, const char *item, size_t item_len);
int binaryset_contains_binary(BinarySet set, const Binary item);
void binaryset_remove(BinarySet set, const char *item, size_t item_len);
void binaryset_remove_binary(BinarySet set, const Binary item);
Iterator binaryset_erase(BinarySet set, Iterator it);
Iterator binaryset_begin(BinarySet set);
Iterator binaryset_next(BinarySet set, Iterator it);
/* Result of `binaryset_value_of` must NOT be modified */
Binary binaryset_value_of(BinarySet set, Iterator it);
size_t binaryset_size(BinarySet set);
int binaryset_compare(BinarySet lhs, BinarySet rhs);
BinaryCompare binaryset_get_compare_fn(BinarySet set);
void binaryset_set_compare_fn(BinarySet set, BinaryCompare compare);
void binaryset_clear(BinarySet set);
void binaryset_destroy(BinarySet set);

GenericMap genericmap_create(BinaryCompare compare, Copier copy, Deleter deleter);
GenericMap genericmap_copy(GenericMap other);
int genericmap_insert_move(GenericMap map, const char *key, size_t key_len, void *item);
int genericmap_insert(GenericMap map, const char *key, size_t key_len, const void *item);
int genericmap_replace_move(GenericMap map, Iterator it, void *item);
int genericmap_replace(GenericMap map, Iterator it, const void *item);
int genericmap_contains(GenericMap map, const char *key, size_t key_len);
void genericmap_remove(GenericMap map, const char *key, size_t key_len);
Iterator genericmap_find(GenericMap map, const char *key, size_t key_len);
Iterator genericmap_erase(GenericMap map, Iterator it);
Iterator genericmap_begin(GenericMap map);
Iterator genericmap_next(GenericMap map, Iterator it);
/* Result of `genericmap_key_of` must NOT be modified */
Binary genericmap_key_of(GenericMap map, Iterator it);
void *genericmap_value_of(GenericMap map, Iterator it);
void *genericmap_value_of_key(GenericMap map, const char *key, size_t key_len);
int genericmap_compare(GenericMap lhs, GenericMap rhs, Compare value_compare);
size_t genericmap_size(GenericMap map);
BinaryCompare genericmap_get_compare_fn(GenericMap map);
void genericmap_set_compare_fn(GenericMap map, BinaryCompare compare);
Copier genericmap_get_copier_fn(GenericMap map);
void genericmap_set_copier_fn(GenericMap map, Copier copier);
Deleter genericmap_get_deleter_fn(GenericMap map);
void genericmap_set_deleter_fn(GenericMap map, Deleter deleter);
void genericmap_clear(GenericMap map);
void genericmap_destroy(GenericMap map);

StringMap stringmap_create();
StringMap stringmap_create_custom(BinaryCompare compare);
StringMap stringmap_copy(StringMap other);
int stringmap_insert_move(StringMap map, const char *key, char *item);
int stringmap_insert(StringMap map, const char *key, const char *item);
int stringmap_insert_n(StringMap map, const char *key, const char *item, size_t item_len);
int stringmap_replace_move(StringMap map, Iterator it, char *item);
int stringmap_replace(StringMap map, Iterator it, const char *item);
int stringmap_replace_n(StringMap map, Iterator it, const char *item, size_t item_len);
int stringmap_contains(StringMap map, const char *key);
void stringmap_remove(StringMap map, const char *key);
Iterator stringmap_find(StringMap map, const char *key);
Iterator stringmap_erase(StringMap map, Iterator it);
Iterator stringmap_begin(StringMap map);
Iterator stringmap_next(StringMap map, Iterator it);
const char *stringmap_key_of(StringMap map, Iterator it);
char *stringmap_value_of(StringMap map, Iterator it);
char *stringmap_value_of_key(StringMap map, const char *key);
size_t stringmap_size(StringMap map);
int stringmap_compare(StringMap lhs, StringMap rhs, StringCompare value_compare);
BinaryCompare stringmap_get_compare_fn(StringMap map);
void stringmap_set_compare_fn(StringMap map, BinaryCompare compare);
void stringmap_clear(StringMap map);
void stringmap_destroy(StringMap map);

GenericList genericlist_create(Compare compare, Copier copy, Deleter deleter);
GenericList genericlist_create_reserve(size_t reserve, Compare compare, Copier copy, Deleter deleter);
GenericList genericlist_copy(GenericList other);
GenericList genericlist_concatenate(GenericList left, GenericList right);
GenericList genericlist_from_genericmap_values(GenericMap other, Compare compare);
GenericList genericlist_create_filled(const void *item, size_t size, Compare compare, Copier copy, Deleter deleter);
int genericlist_fill(GenericList list, const void *item, size_t size);
int genericlist_resize(GenericList list, size_t size, const void *empty_item);
int genericlist_append_list(GenericList list, GenericList other);
int genericlist_append_move(GenericList list, void *item);
int genericlist_append(GenericList list, const void *item);
int genericlist_insert_list(GenericList list, GenericList other, size_t before_index);
int genericlist_insert_move(GenericList list, void *item, size_t before_index);
int genericlist_insert(GenericList list, const void *item, size_t before_index);
int genericlist_replace_move_at(GenericList list, size_t index, void *item);
int genericlist_replace_at(GenericList list, size_t index, const void *item);
size_t genericlist_remove_at(GenericList list, size_t index);
size_t genericlist_remove_one(GenericList list, const void *item);
size_t genericlist_remove_all(GenericList list, const void *item);
size_t genericlist_erase(GenericList list, size_t begin_index, size_t end_index);
void **genericlist_array(GenericList list);
int genericlist_contains(GenericList list, const void *item);
size_t genericlist_find(GenericList list, const void *item, size_t begin_index);
size_t genericlist_rfind(GenericList list, const void *item, size_t begin_index);
int genericlist_compare(GenericList list, GenericList other);
GenericList genericlist_sorted(GenericList list, int descending);
GenericList genericlist_stable_sorted(GenericList list, int descending);
int genericlist_sort(GenericList list, int descending);
int genericlist_stable_sort(GenericList list, int descending);
void genericlist_reserve(GenericList list, size_t size);
Iterator genericlist_begin(GenericList list);
Iterator genericlist_next(GenericList list, Iterator it);
void *genericlist_value_of(GenericList list, Iterator it);
size_t genericlist_size(GenericList list);
Compare genericlist_get_compare_fn(GenericList list);
void genericlist_set_compare_fn(GenericList list, Compare compare);
Copier genericlist_get_copier_fn(GenericList list);
void genericlist_set_copier_fn(GenericList list, Copier copier);
Deleter genericlist_get_deleter_fn(GenericList list);
void genericlist_set_deleter_fn(GenericList list, Deleter deleter);
void genericlist_clear(GenericList list);
void genericlist_destroy(GenericList list);

/* The Compare function for a StringList is `int (*)(const char *, const char *)` */
StringList stringlist_create();
StringList stringlist_create_custom(StringCompare compare);
StringList stringlist_create_reserve(size_t reserve, StringCompare compare);
StringList stringlist_concatenate(StringList left, StringList right);
StringList stringlist_from_stringset(StringSet other);
StringList stringlist_from_binarylist(BinaryList other);
StringList stringlist_from_genericmap_keys(GenericMap other);
StringList stringlist_from_stringmap_keys(StringMap other);
StringList stringlist_from_stringmap_values(StringMap other);
StringList stringlist_split(const char *string, const char *separator, int keep_empty);
StringList stringlist_copy(StringList other);
StringList stringlist_create_filled(const char *item, size_t size);
int stringlist_fill(StringList list, const char *item, size_t size);
int stringlist_resize(StringList list, size_t size, const char *empty_item);
int stringlist_append_list(StringList list, StringList other);
int stringlist_append_move(StringList list, char *item);
int stringlist_append(StringList list, const char *item);
int stringlist_append_n(StringList list, const char *item, size_t item_len);
int stringlist_insert_list(StringList list, StringList other, size_t before_index);
int stringlist_insert_move(StringList list, char *item, size_t before_index);
int stringlist_insert(StringList list, const char *item, size_t before_index);
int stringlist_insert_n(StringList list, const char *item, size_t item_len, size_t before_index);
int stringlist_replace_move_at(StringList list, size_t index, char *item);
int stringlist_replace_at(StringList list, size_t index, const char *item);
int stringlist_replace_at_n(StringList list, size_t index, const char *item, size_t item_len);
size_t stringlist_remove_at(StringList list, size_t index);
size_t stringlist_remove_one(StringList list, const char *item);
size_t stringlist_remove_all(StringList list, const char *item);
size_t stringlist_erase(StringList list, size_t begin_index, size_t end_index);
char **stringlist_array(StringList list);
int stringlist_contains(StringList list, const char *item);
size_t stringlist_find(StringList list, const char *item, size_t begin_index);
size_t stringlist_rfind(StringList list, const char *item, size_t begin_index);
int stringlist_compare(StringList list, StringList other);
StringList stringlist_sorted(StringList list, int descending);
StringList stringlist_stable_sorted(StringList list, int descending);
int stringlist_sort(StringList list, int descending);
int stringlist_stable_sort(StringList list, int descending);
void stringlist_reserve(StringList list, size_t size);
Iterator stringlist_begin(StringList list);
Iterator stringlist_next(StringList list, Iterator it);
char *stringlist_value_of(StringList list, Iterator it);
size_t stringlist_size(StringList list);
StringCompare stringlist_get_compare_fn(StringList list);
void stringlist_set_compare_fn(StringList list, StringCompare compare);
void stringlist_clear(StringList list);
void stringlist_destroy(StringList list);
char *stringlist_joined_alloc(StringList list, const char *separator);

/* If binary data is passed to a function that adopts the allocated data (e.g. the `move` functions), the user MUST ensure that data[length] == 0 */
BinaryList binarylist_create();
BinaryList binarylist_create_custom(BinaryCompare compare);
BinaryList binarylist_create_reserve(size_t reserve, BinaryCompare compare);
BinaryList binarylist_concatenate(BinaryList left, BinaryList right);
BinaryList binarylist_from_stringlist(StringList other);
BinaryList binarylist_from_binaryset(BinarySet other);
BinaryList binarylist_from_genericmap_keys(GenericMap other);
BinaryList binarylist_from_stringmap_keys(StringMap other);
BinaryList binarylist_from_stringmap_values(StringMap other);
BinaryList binarylist_split(const char *string, const char *separator, size_t separator_len, int keep_empty);
BinaryList binarylist_copy(BinaryList other);
BinaryList binarylist_create_filled(const char *item, size_t item_len, size_t size);
BinaryList binarylist_create_filled_binary(const Binary item, size_t size);
int binarylist_fill(BinaryList list, const char *item, size_t item_len, size_t size);
int binarylist_fill_binary(BinaryList list, const Binary item, size_t size);
int binarylist_resize(BinaryList list, size_t size, const char *empty_item, size_t empty_item_len);
int binarylist_append_list(BinaryList list, BinaryList other);
int binarylist_append_move(BinaryList list, char *item, size_t item_len);
int binarylist_append(BinaryList list, const char *item, size_t item_len);
int binarylist_append_binary_move(BinaryList list, Binary item);
int binarylist_append_binary(BinaryList list, const Binary item);
/* TODO: insert list into BinaryList */
int binarylist_insert_move(BinaryList list, char *item, size_t item_len, size_t before_index);
int binarylist_insert(BinaryList list, const char *item, size_t item_len, size_t before_index);
int binarylist_insert_binary_move(BinaryList list, Binary item, size_t before_index);
int binarylist_insert_binary(BinaryList list, const Binary item, size_t before_index);
int binarylist_replace_move_at(BinaryList list, size_t index, char *item, size_t item_len);
int binarylist_replace_at(BinaryList list, size_t index, const char *item, size_t item_len);
int binarylist_replace_binary_move_at(BinaryList list, size_t index, Binary item);
int binarylist_replace_binary_at(BinaryList list, size_t index, Binary item);
size_t binarylist_remove_at(BinaryList list, size_t index);
size_t binarylist_remove_one(BinaryList list, const char *item, size_t item_len);
size_t binarylist_remove_all(BinaryList list, const char *item, size_t item_len);
size_t binarylist_erase(BinaryList list, size_t begin_index, size_t end_index);
Binary *binarylist_array(BinaryList list);
int binarylist_contains(BinaryList list, const char *item, size_t item_len);
size_t binarylist_find(BinaryList list, const char *item, size_t item_len, size_t begin_index);
size_t binarylist_rfind(BinaryList list, const char *item, size_t item_len, size_t begin_index);
int binarylist_compare(BinaryList list, BinaryList other);
/* TODO: sorting for BinaryList objects */
void binarylist_reserve(BinaryList list, size_t size);
Iterator binarylist_begin(BinaryList list);
Iterator binarylist_next(BinaryList list, Iterator it);
Binary binarylist_value_of(BinaryList list, Iterator it);
size_t binarylist_size(BinaryList list);
BinaryCompare binarylist_get_compare_fn(BinaryList list);
void binarylist_set_compare_fn(BinaryList list, BinaryCompare compare);
void binarylist_clear(BinaryList list);
void binarylist_destroy(BinaryList list);

#endif // STRINGLIST_H
