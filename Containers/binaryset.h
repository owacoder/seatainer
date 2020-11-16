/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef BINARYSET_H
#define BINARYSET_H

#include "common.h"

Variant variant_from_binaryset(BinarySet set);
int variant_is_binaryset(Variant var);
BinarySet variant_get_binaryset(Variant var);
int variant_set_binaryset(Variant var, const BinarySet set);
int variant_set_binaryset_move(Variant var, BinarySet set);
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
CommonContainerBase *binaryset_get_container_base(BinarySet set);

#endif // BINARYSET_H
