/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef BINARYLIST_H
#define BINARYLIST_H

#include "common.h"

/* If binary data is passed to a function that adopts the allocated data (e.g. the `move` functions), the user MUST ensure that data[length] == 0 */
Variant variant_from_binarylist(BinaryList map);
int variant_is_binarylist(Variant var);
BinaryList variant_get_binarylist(Variant var);
int variant_set_binarylist(Variant var, const BinaryList set);
int variant_set_binarylist_move(Variant var, BinaryList set);
BinaryList binarylist_create();
BinaryList binarylist_create_custom(BinaryCompare compare);
BinaryList binarylist_create_reserve(size_t reserve, BinaryCompare compare);
BinaryList binarylist_concatenate(BinaryList left, BinaryList right);
BinaryList binarylist_from_stringlist(StringList other);
BinaryList binarylist_from_binaryset(BinarySet other);
BinaryList binarylist_from_genericmap_keys(GenericMap other);
BinaryList binarylist_from_stringmap_keys(StringMap other);
BinaryList binarylist_from_stringmap_values(StringMap other);
BinaryList binarylist_split(const char *string, size_t string_len, const char *separator, size_t separator_len, int keep_empty);
BinaryList binarylist_divide(const char *binary, size_t binary_len, size_t record_size, int keep_partial);
BinaryList binarylist_create_filled(const char *item, size_t item_len, size_t size);
BinaryList binarylist_create_filled_binary(const Binary item, size_t size);
BinaryList binarylist_copy(BinaryList other);
BinaryList binarylist_copy_slice(BinaryList other, size_t begin_index, size_t length);
int binarylist_fill(BinaryList list, const char *item, size_t item_len, size_t size);
int binarylist_fill_binary(BinaryList list, const Binary item, size_t size);
int binarylist_resize(BinaryList list, size_t size, const char *empty_item, size_t empty_item_len);
int binarylist_append_list(BinaryList list, BinaryList other);
int binarylist_append_move(BinaryList list, char *item, size_t item_len);
int binarylist_append(BinaryList list, const char *item, size_t item_len);
int binarylist_append_binary_move(BinaryList list, Binary item);
int binarylist_append_binary(BinaryList list, const Binary item);
int binarylist_insert_list(BinaryList list, BinaryList other, size_t before_index);
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
BinaryList binarylist_sorted(BinaryList list, int descending);
BinaryList binarylist_stable_sorted(BinaryList list, int descending);
int binarylist_sort(BinaryList list, int descending);
int binarylist_stable_sort(BinaryList list, int descending);
void binarylist_reserve(BinaryList list, size_t size);
Iterator binarylist_begin(BinaryList list);
Iterator binarylist_next(BinaryList list, Iterator it);
Binary binarylist_value_of(BinaryList list, Iterator it);
size_t binarylist_size(BinaryList list);
BinaryCompare binarylist_get_compare_fn(BinaryList list);
void binarylist_set_compare_fn(BinaryList list, BinaryCompare compare);
void binarylist_clear(BinaryList list);
void binarylist_destroy(BinaryList list);

#endif // BINARYLIST_H
