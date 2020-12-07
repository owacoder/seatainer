/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef GENERICLINKEDLIST_H
#define GENERICLINKEDLIST_H

#include "common.h"

Variant variant_from_genericlinkedlist(GenericLinkedList list);
int variant_is_genericlinkedlist(Variant var);
int variant_is_variantlist(Variant var);
GenericLinkedList variant_get_genericlinkedlist(Variant var);
int variant_set_genericlinkedlist(Variant var, const GenericLinkedList list);
int variant_set_genericlinkedlist_move(Variant var, GenericLinkedList list);
GenericLinkedList genericlinkedlist_create(const CommonContainerBase *base);
GenericLinkedList genericlinkedlist_copy(GenericLinkedList other);
GenericLinkedList genericlinkedlist_concatenate(GenericLinkedList left, GenericLinkedList right);
GenericLinkedList genericlinkedlist_from_genericmap_values(GenericMap other);
GenericLinkedList genericlinkedlist_from_array(const void **items, const CommonContainerBase *base);
GenericLinkedList genericlinkedlist_from_array_n(const void **items, size_t count, const CommonContainerBase *base);
int genericlinkedlist_append_list(GenericLinkedList list, GenericLinkedList other);
int genericlinkedlist_append_move(GenericLinkedList list, void *item);
int genericlinkedlist_append(GenericLinkedList list, const void *item);
int genericlinkedlist_prepend_move(GenericLinkedList list, void *item);
int genericlinkedlist_prepend(GenericLinkedList list, const void *item);
int genericlinkedlist_insert_list(GenericLinkedList list, GenericLinkedList other, Iterator after_it);
int genericlinkedlist_insert_move(GenericLinkedList list, void *item, Iterator after_it);
int genericlinkedlist_insert(GenericLinkedList list, const void *item, Iterator after_it);
int genericlinkedlist_replace_move_at(GenericLinkedList list, Iterator it, void *item);
int genericlinkedlist_replace_at(GenericLinkedList list, Iterator it, const void *item);
size_t genericlinkedlist_remove_after(GenericLinkedList list, Iterator it);
size_t genericlinkedlist_remove_one(GenericLinkedList list, const void *item);
size_t genericlinkedlist_remove_all(GenericLinkedList list, const void *item);
int genericlinkedlist_contains(GenericLinkedList list, const void *item);
Iterator genericlinkedlist_find(GenericLinkedList list, const void *item, Iterator after, Iterator *prior);
int genericlinkedlist_compare(GenericLinkedList list, GenericLinkedList other);
GenericLinkedList genericlinkedlist_sorted(GenericLinkedList list, int descending);
GenericLinkedList genericlinkedlist_stable_sorted(GenericLinkedList list, int descending);
int genericlinkedlist_sort(GenericLinkedList list, int descending);
int genericlinkedlist_stable_sort(GenericLinkedList list, int descending);
Iterator genericlinkedlist_begin(GenericLinkedList list);
Iterator genericlinkedlist_next(GenericLinkedList list, Iterator it);
void *genericlinkedlist_value_of(GenericLinkedList list, Iterator it);
size_t genericlinkedlist_size(GenericLinkedList list);
void genericlinkedlist_clear(GenericLinkedList list);
void genericlinkedlist_destroy(GenericLinkedList list);
const CommonContainerBase *genericlinkedlist_get_container_base(GenericLinkedList list);
CommonContainerBase *genericlinkedlist_build_recipe(GenericLinkedList list);

Compare genericlinkedlist_get_compare_fn(GenericLinkedList list);
Copier genericlinkedlist_get_copier_fn(GenericLinkedList list);
Deleter genericlinkedlist_get_deleter_fn(GenericLinkedList list);
Parser genericlinkedlist_get_parser_fn(GenericLinkedList list);
Serializer genericlinkedlist_get_serializer_fn(GenericLinkedList list);
int genericlinkedlist_set_compare_fn(GenericLinkedList list, Compare compare);
int genericlinkedlist_set_copier_fn(GenericLinkedList list, Copier copier);
int genericlinkedlist_set_deleter_fn(GenericLinkedList list, Deleter deleter);
int genericlinkedlist_set_parser_fn(GenericLinkedList list, Parser parser);
int genericlinkedlist_set_serializer_fn(GenericLinkedList list, Serializer serializer);

#endif // GENERICLINKEDLIST_H
