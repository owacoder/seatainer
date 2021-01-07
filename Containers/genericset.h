/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef GENERICSET_H
#define GENERICSET_H

#include "common.h"

Variant variant_from_genericset(GenericSet set);
int variant_is_genericset(Variant var);
int variant_is_variantset(Variant var);
GenericSet variant_get_genericset(Variant var);
int variant_set_genericset(Variant var, const GenericSet set);
int variant_set_genericset_move(Variant var, GenericSet set);
GenericSet genericset_create(const CommonContainerBase *base);
GenericSet genericset_from_genericlist(GenericList list);
GenericSet genericset_copy(GenericSet other);
GenericSet genericset_union(GenericSet a, GenericSet b);
GenericSet genericset_intersection(GenericSet a, GenericSet b);
GenericSet genericset_subtract(GenericSet from, GenericSet set_to_subtract);
GenericSet genericset_difference(GenericSet a, GenericSet b);
int genericset_add(GenericSet set, const void *item);
int genericset_add_move(GenericSet set, void *item);
Iterator genericset_find(GenericSet set, const void *item);
int genericset_contains(GenericSet set, const void *item);
void genericset_remove(GenericSet set, const void *item);
Iterator genericset_erase(GenericSet set, Iterator it);
Iterator genericset_begin(GenericSet set);
Iterator genericset_next(GenericSet set, Iterator it);
/* Result of `genericset_value_of` must NOT be modified */
const void *genericset_value_of(GenericSet set, Iterator it);
size_t genericset_size(GenericSet set);
int genericset_compare(GenericSet lhs, GenericSet rhs);
void genericset_clear(GenericSet set);
void genericset_destroy(GenericSet set);
const CommonContainerBase *genericset_get_container_base(GenericSet set);
CommonContainerBase *genericset_build_recipe(GenericSet set);

Compare genericset_get_compare_fn(GenericSet set);
Copier genericset_get_copier_fn(GenericSet set);
Deleter genericset_get_deleter_fn(GenericSet set);
Parser genericset_get_parser_fn(GenericSet set);
Serializer genericset_get_serializer_fn(GenericSet set);
int genericset_set_compare_fn(GenericSet set, Compare compare);
int genericset_set_copier_fn(GenericSet set, Copier copier);
int genericset_set_deleter_fn(GenericSet set, Deleter deleter);
int genericset_set_parser_fn(GenericSet set, Parser parser);
int genericset_set_serializer_fn(GenericSet set, Serializer serializer);

#endif // GENERICSET_H
