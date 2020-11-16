/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef STRINGMAP_H
#define STRINGMAP_H

#include "common.h"

Variant variant_from_stringmap(StringMap map);
int variant_is_stringmap(Variant var);
StringMap variant_get_stringmap(Variant var);
int variant_set_stringmap(Variant var, const StringMap map);
int variant_set_stringmap_move(Variant var, StringMap map);
StringMap stringmap_create();
StringMap stringmap_create_custom(BinaryCompare compare, StringCompare value_compare);
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
int stringmap_compare(StringMap lhs, StringMap rhs);
BinaryCompare stringmap_get_key_compare_fn(StringMap map);
void stringmap_set_key_compare_fn(StringMap map, BinaryCompare compare);
StringCompare stringmap_get_value_compare_fn(StringMap map);
void stringmap_set_value_compare_fn(StringMap map, StringCompare compare);
void stringmap_clear(StringMap map);
void stringmap_destroy(StringMap map);
CommonContainerBase *stringmap_get_container_base(StringMap map);

#endif // STRINGMAP_H
