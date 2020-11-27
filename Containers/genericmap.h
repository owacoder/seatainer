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
GenericMap genericmap_create(const CommonContainerBase *key_base, const CommonContainerBase *value_base);
GenericMap genericmap_copy(GenericMap other);
int genericmap_insert_move(GenericMap map, const void *key, void *item);
int genericmap_insert(GenericMap map, const void *key, const void *item);
int genericmap_replace_move(GenericMap map, Iterator it, void *item);
int genericmap_replace(GenericMap map, Iterator it, const void *item);
int genericmap_contains(GenericMap map, const void *key);
void genericmap_remove(GenericMap map, const void *key);
Iterator genericmap_find(GenericMap map, const void *key);
Iterator genericmap_erase(GenericMap map, Iterator it);
Iterator genericmap_begin(GenericMap map);
Iterator genericmap_next(GenericMap map, Iterator it);
const void *genericmap_key_of(GenericMap map, Iterator it);
void *genericmap_value_of(GenericMap map, Iterator it);
void *genericmap_value_of_key(GenericMap map, const void *key);
int genericmap_compare(GenericMap lhs, GenericMap rhs);
size_t genericmap_size(GenericMap map);
void genericmap_clear(GenericMap map);
void genericmap_destroy(GenericMap map);
const CommonContainerBase *genericmap_get_key_container_base(GenericMap map);
const CommonContainerBase *genericmap_get_value_container_base(GenericMap map);
CommonContainerBase *genericmap_build_recipe(GenericMap map);

Compare genericmap_get_key_compare_fn(GenericMap map);
Compare genericmap_get_value_compare_fn(GenericMap map);
Copier genericmap_get_key_copier_fn(GenericMap map);
Copier genericmap_get_value_copier_fn(GenericMap map);
Deleter genericmap_get_key_deleter_fn(GenericMap map);
Deleter genericmap_get_value_deleter_fn(GenericMap map);
Parser genericmap_get_key_parser_fn(GenericMap map);
Parser genericmap_get_value_parser_fn(GenericMap map);
Serializer genericmap_get_key_serializer_fn(GenericMap map);
Serializer genericmap_get_value_serializer_fn(GenericMap map);
int genericmap_set_key_compare_fn(GenericMap map, Compare compare);
int genericmap_set_value_compare_fn(GenericMap map, Compare compare);
int genericmap_set_key_copier_fn(GenericMap map, Copier copier);
int genericmap_set_value_copier_fn(GenericMap map, Copier copier);
int genericmap_set_key_deleter_fn(GenericMap map, Deleter deleter);
int genericmap_set_value_deleter_fn(GenericMap map, Deleter deleter);
int genericmap_set_key_parser_fn(GenericMap map, Parser parser);
int genericmap_set_value_parser_fn(GenericMap map, Parser parser);
int genericmap_set_key_serializer_fn(GenericMap map, Serializer serializer);
int genericmap_set_value_serializer_fn(GenericMap map, Serializer serializer);

#endif // GENERICMAP_H
