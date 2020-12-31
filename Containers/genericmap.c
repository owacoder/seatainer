/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "genericmap.h"
#include "impl/avl.h"
#include "recipes.h"

/* For conversions */
#include "variant.h"

/* See common.c for reasoning behind this empty struct */
struct GenericMapStruct {char dummy;};

Variant variant_from_genericmap(GenericMap map) {
    return variant_create_custom_adopt(map, genericmap_build_recipe(map));
}

int variant_is_genericmap(Variant var) {
    return generic_types_compatible_compare(variant_get_custom_container_base(var), container_base_genericmap_recipe()) == 0;
}

int variant_is_variantmap(Variant var) {
    return generic_types_compatible_compare(variant_get_custom_container_base(var), container_base_variantmap_recipe()) == 0;
}

GenericMap variant_get_genericmap(Variant var) {
    if (!variant_is_genericmap(var))
        return NULL;

    return variant_get_custom_data(var);
}

int variant_set_genericmap_move(Variant var, GenericMap map) {
    return variant_set_custom_move_adopt(var, map, genericmap_build_recipe(map));
}

int variant_set_genericmap(Variant var, const GenericMap map) {
    return variant_set_custom_adopt(var, map, genericmap_build_recipe(map));
}

GenericMap genericmap_create(const CommonContainerBase *key_base, const CommonContainerBase *value_base) {
    return (GenericMap) avltree_create(key_base, value_base);
}

GenericMap genericmap_copy(GenericMap other) {
    return (GenericMap) avltree_copy((struct AVLTree *) other);
}

int genericmap_insert_move_key(GenericMap map, void *key, void *item) {
    if (avltree_insert_move_key((struct AVLTree *) map, key, item) == NULL)
        return CC_ENOMEM;

    return 0;
}

int genericmap_insert_move(GenericMap map, const void *key, void *item) {
    if (avltree_insert_copy_key((struct AVLTree *) map, key, item) == NULL)
        return CC_ENOMEM;

    return 0;
}

int genericmap_insert(GenericMap map, const void *key, const void *item) {
    if (avltree_get_value_copier_fn((struct AVLTree *) map) == NULL)
        return CC_ENOTSUP;

    void *duplicate = avltree_get_value_copier_fn((struct AVLTree *) map)(item);
    if (duplicate == NULL && item != NULL)
        return CC_ENOMEM;

    int err = genericmap_insert_move(map, key, duplicate);
    if (err)
        avltree_get_value_deleter_fn((struct AVLTree *) map)(duplicate);

    return err;
}

int genericmap_replace_move(GenericMap map, Iterator it, void *item) {
    struct AVLNode *node = it;

    avltree_get_value_deleter_fn((struct AVLTree *) map)(*avlnode_value(node));
    *avlnode_value(node) = item;
    return 0;
}

int genericmap_replace(GenericMap map, Iterator it, const void *item) {
    if (avltree_get_value_copier_fn((struct AVLTree *) map) == NULL)
        return CC_ENOTSUP;

    void *duplicate = avltree_get_value_copier_fn((struct AVLTree *) map)(item);
    if (duplicate == NULL && item != NULL)
        return CC_ENOMEM;

    int err = genericmap_replace_move(map, it, duplicate);
    if (err)
        avltree_get_value_deleter_fn((struct AVLTree *) map)(duplicate);

    return err;
}

int genericmap_contains(GenericMap map, const void *key) {
    return avltree_find((struct AVLTree *) map, key) != NULL;
}

Iterator genericmap_find(GenericMap map, const void *key) {
    return avltree_find((struct AVLTree *) map, key);
}

void genericmap_remove(GenericMap map, const void *key) {
    avltree_delete((struct AVLTree *) map, key);
}

Iterator genericmap_erase(GenericMap map, Iterator it) {
    return avltree_delete_node((struct AVLTree *) map, (struct AVLNode *) it);
}

Iterator genericmap_begin(GenericMap map) {
    return avltree_min_node((struct AVLTree *) map);
}

Iterator genericmap_next(GenericMap map, Iterator it) {
    UNUSED(map)

    return avltree_inorder_next((struct AVLNode *) it);
}

const void *genericmap_key_of(GenericMap map, Iterator it) {
    UNUSED(map)

    return avlnode_key((struct AVLNode *) it);
}

void *genericmap_value_of(GenericMap map, Iterator it) {
    UNUSED(map)

    return *avlnode_value((struct AVLNode *) it);
}

void *genericmap_value_of_key(GenericMap map, const void *key) {
    Iterator it = genericmap_find(map, key);
    if (it == NULL)
        return NULL;

    return genericmap_value_of(map, it);
}

int genericmap_compare(GenericMap lhs, GenericMap rhs) {
    return avltree_compare((struct AVLTree *) lhs, (struct AVLTree *) rhs);
}

size_t genericmap_size(GenericMap map) {
    return avltree_size((struct AVLTree *) map);
}

void genericmap_clear(GenericMap map) {
    avltree_clear((struct AVLTree *) map);
}

void genericmap_destroy(GenericMap map) {
    avltree_destroy((struct AVLTree *) map);
}

const CommonContainerBase *genericmap_get_key_container_base(GenericMap map) {
    return avltree_get_key_container_base((struct AVLTree *) map);
}

const CommonContainerBase *genericmap_get_value_container_base(GenericMap map) {
    return avltree_get_value_container_base((struct AVLTree *) map);
}

CommonContainerBase *genericmap_build_recipe(GenericMap map) {
    return container_base_build_key_value_container(genericmap_get_key_container_base(map),
                                                    genericmap_get_value_container_base(map),
                                                    container_base_genericmap_recipe());
}

Compare genericmap_get_key_compare_fn(GenericMap map) {
    return avltree_get_key_compare_fn((struct AVLTree *) map);
}

int genericmap_set_key_compare_fn(GenericMap map, Compare compare) {
    return avltree_set_key_compare_fn((struct AVLTree *) map, compare);
}

Compare genericmap_get_value_compare_fn(GenericMap map) {
    return avltree_get_value_compare_fn((struct AVLTree *) map);
}

int genericmap_set_value_compare_fn(GenericMap map, Compare compare) {
    return avltree_set_value_compare_fn((struct AVLTree *) map, compare);
}

Copier genericmap_get_key_copier_fn(GenericMap map) {
    return avltree_get_key_copier_fn((struct AVLTree *) map);
}

int genericmap_set_key_copier_fn(GenericMap map, Copier copier) {
    return avltree_set_key_copier_fn((struct AVLTree *) map, copier);
}

Copier genericmap_get_value_copier_fn(GenericMap map) {
    return avltree_get_value_copier_fn((struct AVLTree *) map);
}

int genericmap_set_value_copier_fn(GenericMap map, Copier copier) {
    return avltree_set_value_copier_fn((struct AVLTree *) map, copier);
}

Deleter genericmap_get_key_deleter_fn(GenericMap map) {
    return avltree_get_key_deleter_fn((struct AVLTree *) map);
}

int genericmap_set_key_deleter_fn(GenericMap map, Deleter deleter) {
    return avltree_set_key_deleter_fn((struct AVLTree *) map, deleter);
}

Deleter genericmap_get_value_deleter_fn(GenericMap map) {
    return avltree_get_value_deleter_fn((struct AVLTree *) map);
}

int genericmap_set_value_deleter_fn(GenericMap map, Deleter deleter) {
    return avltree_set_value_deleter_fn((struct AVLTree *) map, deleter);
}

Parser genericmap_get_key_parser_fn(GenericMap map) {
    return avltree_get_key_parser_fn((struct AVLTree *) map);
}

int genericmap_set_key_parser_fn(GenericMap map, Parser parser) {
    return avltree_set_key_parser_fn((struct AVLTree *) map, parser);
}

Parser genericmap_get_value_parser_fn(GenericMap map) {
    return avltree_get_value_parser_fn((struct AVLTree *) map);
}

int genericmap_set_value_parser_fn(GenericMap map, Parser parser) {
    return avltree_set_value_parser_fn((struct AVLTree *) map, parser);
}

Serializer genericmap_get_key_serializer_fn(GenericMap map) {
    return avltree_get_key_serializer_fn((struct AVLTree *) map);
}

int genericmap_set_key_serializer_fn(GenericMap map, Serializer serializer) {
    return avltree_set_key_serializer_fn((struct AVLTree *) map, serializer);
}

Serializer genericmap_get_value_serializer_fn(GenericMap map) {
    return avltree_get_value_serializer_fn((struct AVLTree *) map);
}

int genericmap_set_value_serializer_fn(GenericMap map, Serializer serializer) {
    return avltree_set_value_serializer_fn((struct AVLTree *) map, serializer);
}
