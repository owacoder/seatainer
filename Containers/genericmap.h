/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef GENERICMAP_H
#define GENERICMAP_H

#include "common.h"

Variant variant_from_genericmap(GenericMap map);
int variant_is_genericmap(Variant var);
int variant_is_variantmap(Variant var);
GenericMap variant_get_genericmap(Variant var);
int variant_set_genericmap(Variant var, const GenericMap map);
int variant_set_genericmap_move(Variant var, GenericMap map);
GenericMap genericmap_create(BinaryCompare key_compare, Compare value_compare, Copier copy, Deleter deleter);
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
int genericmap_compare(GenericMap lhs, GenericMap rhs);
size_t genericmap_size(GenericMap map);
BinaryCompare genericmap_get_key_compare_fn(GenericMap map);
void genericmap_set_key_compare_fn(GenericMap map, BinaryCompare compare);
Compare genericmap_get_value_compare_fn(GenericMap map);
void genericmap_set_value_compare_fn(GenericMap map, Compare compare);
Copier genericmap_get_copier_fn(GenericMap map);
void genericmap_set_copier_fn(GenericMap map, Copier copier);
Deleter genericmap_get_deleter_fn(GenericMap map);
void genericmap_set_deleter_fn(GenericMap map, Deleter deleter);
void genericmap_clear(GenericMap map);
void genericmap_destroy(GenericMap map);
CommonContainerBase *genericmap_get_container_base(GenericMap map);

#endif // GENERICMAP_H
