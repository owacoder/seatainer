#ifndef STRINGLIST_H
#define STRINGLIST_H

#include "platforms.h"
#include "seaerror.h"

struct GenericListStruct;
struct StringListStruct;
struct BinaryListStruct;
struct StringSetStruct;
struct KeyValueListStruct;

typedef struct GenericListStruct *GenericList; /* Simple list of generic pointers */
typedef struct StringListStruct *StringList; /* Simple list of NUL-terminated strings */
typedef struct BinaryListStruct *BinaryList; /* Simple list of binary strings or other sized plain-old-data */
typedef void *StringSet; /* Simple exclusive set of NUL-terminated strings, each element only appears once. No element order is defined */
typedef void *StringMap; /* Simple exclusive set of NUL-terminated string keys with values, each element only appears once. Elements are strings. No element order is defined */
typedef void *Iterator; /* Simple iterator, does not need to be freed */

typedef struct BinaryStruct {
    char *data;
    size_t length;
} Binary;

/* TODO: compare function for stringlist takes `char *` arguments and binary containers take a `Binary *`
 * Making stringlists use `Binary *` might be an unnecessary performance hit.
 * OTOH, compare functions can't be interchanged between containers as-is.
 */
typedef int (*Compare)(const void *a, const void *b);
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

StringSet stringset_create();
StringSet stringset_create_custom(Compare compare);
StringSet stringset_from_stringlist(StringList list, Compare compare);
StringSet stringset_copy(StringSet other);
int stringset_add(StringSet set, const char *item);
int stringset_add_n(StringSet set, const char *item, size_t item_len);
int stringset_contains(StringSet set, const char *item);
void stringset_remove(StringSet set, const char *item);
Iterator stringset_begin(StringSet set);
Iterator stringset_next(StringSet set, Iterator it);
const char *stringset_value_of(StringSet set, Iterator it);
size_t stringset_size(StringSet set);
Compare stringset_get_compare_fn(StringSet set);
void stringset_set_compare_fn(StringSet set, Compare compare);
void stringset_clear(StringSet set);
void stringset_destroy(StringSet set);

GenericList genericlist_create(Compare compare, Copier copy, Deleter deleter);
GenericList genericlist_create_reserve(size_t items, Compare compare, Copier copy, Deleter deleter);
GenericList genericlist_copy(GenericList other);
GenericList genericlist_concatenate(GenericList left, GenericList right);
int genericlist_append_actual(GenericList list, void *item);
int genericlist_append_copy(GenericList list, const void *item);
int genericlist_insert_actual(GenericList list, void *item, size_t before_index);
int genericlist_insert_copy(GenericList list, const void *item, size_t before_index);
int genericlist_replace_actual_at(GenericList list, size_t index, void *item);
int genericlist_replace_copy_at(GenericList list, size_t index, const void *item);
size_t genericlist_remove_at(GenericList list, size_t index);
size_t genericlist_remove_one(GenericList list, const void *item);
size_t genericlist_remove_all(GenericList list, const void *item);
size_t genericlist_erase(GenericList list, size_t begin_index, size_t end_index);
void **genericlist_array(GenericList list);
int genericlist_contains(GenericList list, const void *item);
size_t genericlist_find(GenericList list, const void *item, size_t begin_index);
size_t genericlist_rfind(GenericList list, const void *item, size_t begin_index);
int genericlist_compare(GenericList list, GenericList other);
GenericList genericlist_stable_sorted(GenericList list, int descending);
int genericlist_stable_sort(GenericList list, int descending);
void genericlist_reserve(GenericList list, size_t size);
size_t genericlist_size(GenericList list);
Compare genericlist_get_compare_fn(GenericList list);
void genericlist_set_compare_fn(GenericList list, Compare compare);
Copier genericlist_get_copier_fn(GenericList list);
void genericlist_set_copier_fn(GenericList list, Copier copier);
Deleter genericlist_get_deleter_fn(GenericList list);
void genericlist_set_deleter_fn(GenericList list, Deleter deleter);
void genericlist_clear(GenericList list);
void genericlist_destroy(GenericList list);

StringList stringlist_create();
StringList stringlist_create_custom(Compare compare);
StringList stringlist_from_stringset(StringSet other);
StringList stringlist_from_binarylist(BinaryList other);
StringList stringlist_split(const char *string, const char *separator, int keep_empty);
StringList stringlist_copy(StringList other);
int stringlist_append(StringList list, const char *item);
int stringlist_append_n(StringList list, const char *item, size_t item_len);
int stringlist_insert(StringList list, const char *item, size_t before_index);
int stringlist_insert_n(StringList list, const char *item, size_t item_len, size_t before_index);
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
size_t stringlist_size(StringList list);
Compare stringlist_get_compare_fn(StringList list);
void stringlist_set_compare_fn(StringList list, Compare compare);
void stringlist_clear(StringList list);
void stringlist_destroy(StringList list);
char *stringlist_joined_alloc(StringList list, const char *separator);

BinaryList binarylist_create();
BinaryList binarylist_create_custom(Compare compare);
BinaryList binarylist_from_stringlist(StringList other);
BinaryList binarylist_split(const char *string, const char *separator, int keep_empty);
BinaryList binarylist_copy(BinaryList other);
int binarylist_append(BinaryList list, const char *item, size_t item_len);
int binarylist_insert(BinaryList list, const char *item, size_t item_len, size_t before_index);
int binarylist_replace_at(BinaryList list, size_t index, const char *item, size_t item_len);
size_t binarylist_remove_at(BinaryList list, size_t index);
size_t binarylist_remove_one(BinaryList list, const char *item, size_t item_len);
size_t binarylist_remove_all(BinaryList list, const char *item, size_t item_len);
size_t binarylist_erase(BinaryList list, size_t begin_index, size_t end_index);
Binary *binarylist_array(BinaryList list);
int binarylist_contains(BinaryList list, const char *item, size_t item_len);
size_t binarylist_find(BinaryList list, const char *item, size_t item_len, size_t begin_index);
size_t binarylist_rfind(BinaryList list, const char *item, size_t item_len, size_t begin_index);
int binarylist_compare(BinaryList list, BinaryList other);
size_t binarylist_size(BinaryList list);
Compare binarylist_get_compare_fn(BinaryList list);
void binarylist_set_compare_fn(BinaryList list, Compare compare);
void binarylist_clear(BinaryList list);
void binarylist_destroy(BinaryList list);

#endif // STRINGLIST_H
