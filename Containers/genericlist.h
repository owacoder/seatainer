/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef GENERICLIST_H
#define GENERICLIST_H

#include "common.h"

Variant variant_from_genericlist(GenericList list);
int variant_is_genericlist(Variant var);
int variant_is_variantlist(Variant var);
GenericList variant_get_genericlist(Variant var);
int variant_set_genericlist(Variant var, const GenericList list);
int variant_set_genericlist_move(Variant var, GenericList list);
GenericList genericlist_create(const CommonContainerBase *base);
GenericList genericlist_create_reserve(size_t reserve, const CommonContainerBase *base);
GenericList genericlist_copy(GenericList other);
GenericList genericlist_concatenate(GenericList left, GenericList right);
GenericList genericlist_from_genericmap_values(GenericMap other);
GenericList genericlist_from_array(const void **items, const CommonContainerBase *base);
GenericList genericlist_from_array_n(const void **items, size_t count, const CommonContainerBase *base);
GenericList genericlist_create_filled(const void *item, size_t size, const CommonContainerBase *base);
GenericList genericlist_copy_slice(GenericList other, size_t begin_index, size_t length);
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
size_t genericlist_bsearch(GenericList list, const void *item);
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
void genericlist_clear(GenericList list);
void genericlist_destroy(GenericList list);
const CommonContainerBase *genericlist_get_container_base(GenericList list);
CommonContainerBase *genericlist_build_recipe(GenericList list);

Compare genericlist_get_compare_fn(GenericList list);
Copier genericlist_get_copier_fn(GenericList list);
Deleter genericlist_get_deleter_fn(GenericList list);
Parser genericlist_get_parser_fn(GenericList list);
Serializer genericlist_get_serializer_fn(GenericList list);
int genericlist_set_compare_fn(GenericList list, Compare compare);
int genericlist_set_copier_fn(GenericList list, Copier copier);
int genericlist_set_deleter_fn(GenericList list, Deleter deleter);
int genericlist_set_parser_fn(GenericList list, Parser parser);
int genericlist_set_serializer_fn(GenericList list, Serializer serializer);

#endif // GENERICLIST_H
