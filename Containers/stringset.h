/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef STRINGSET_H
#define STRINGSET_H

#include "common.h"

Variant variant_from_stringset(StringSet set);
int variant_is_stringset(Variant var);
StringSet variant_get_stringset(Variant var);
int variant_set_stringset(Variant var, const StringSet set);
int variant_set_stringset_move(Variant var, StringSet set);
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

#endif // STRINGSET_H
