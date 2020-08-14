/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef STRINGLIST_H
#define STRINGLIST_H

#include "common.h"

/* The Compare function for a StringList is `int (*)(const char *, const char *)` */
Variant variant_from_stringlist(StringList map);
int variant_is_stringlist(Variant var);
StringList variant_get_stringlist(Variant var);
int variant_set_stringlist(Variant var, const StringList set);
int variant_set_stringlist_move(Variant var, StringList set);
StringList stringlist_create();
StringList stringlist_create_custom(StringCompare compare);
StringList stringlist_create_reserve(size_t reserve, StringCompare compare);
StringList stringlist_concatenate(StringList left, StringList right);
StringList stringlist_from_array(const char **strings);
StringList stringlist_from_array_n(const char **strings, size_t count);
StringList stringlist_from_stringset(StringSet other);
StringList stringlist_from_binarylist(BinaryList other);
StringList stringlist_from_genericmap_keys(GenericMap other);
StringList stringlist_from_stringmap_keys(StringMap other);
StringList stringlist_from_stringmap_values(StringMap other);
StringList stringlist_split(const char *string, const char *separator, int keep_empty);
StringList stringlist_divide(const char *string, size_t record_size, int keep_partial);
StringList stringlist_copy(StringList other);
StringList stringlist_create_filled(const char *item, size_t size);
StringList stringlist_copy_slice(StringList other, size_t begin_index, size_t length);
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
char *stringlist_join(StringList list, const char *separator);

#endif // STRINGLIST_H
