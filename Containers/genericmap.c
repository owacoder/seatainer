/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "genericmap.h"
#include "impl/avl.h"

/* For conversions */
#include "variant.h"

/* See common.c for reasoning behind this empty struct */
struct GenericMapStruct {};

Variant variant_from_genericmap(GenericMap set) {
    return variant_create_custom(set, (Compare) genericmap_compare, (Copier) genericmap_copy, (Deleter) genericmap_destroy);
}

int variant_is_genericmap(Variant var) {
    return variant_get_copier_fn(var) == (Copier) genericmap_copy;
}

GenericMap variant_get_genericmap(Variant var) {
    if (!variant_is_genericmap(var))
        return NULL;

    return variant_get_custom(var);
}

int variant_set_genericmap_move(Variant var, GenericMap set) {
    return variant_set_custom_move(var, set, (Compare) genericmap_compare, (Copier) genericmap_copy, (Deleter) genericmap_destroy);
}

int variant_set_genericmap(Variant var, const GenericMap set) {
    return variant_set_custom(var, set, (Compare) genericmap_compare, (Copier) genericmap_copy, (Deleter) genericmap_destroy);
}

GenericMap genericmap_create(BinaryCompare key_compare, Compare value_compare, Copier copy, Deleter deleter) {
    struct AVLTree *tree = avltree_create();
    if (tree == NULL)
        return NULL;

    avltree_set_key_compare_fn(tree, key_compare);
    avltree_set_value_compare_fn(tree, value_compare);
    avltree_set_copier_fn(tree, copy);
    avltree_set_deleter_fn(tree, deleter);

    return (GenericMap) tree;
}

GenericMap genericmap_copy(GenericMap other) {
    return (GenericMap) avltree_copy((struct AVLTree *) other);
}

int genericmap_insert_move(GenericMap map, const char *key, size_t key_len, void *item) {
    if (avltree_insert((struct AVLTree *) map, key, key_len, item) == NULL)
        return CC_ENOMEM;

    return 0;
}

int genericmap_insert(GenericMap map, const char *key, size_t key_len, const void *item) {
    if (avltree_get_copier_fn((struct AVLTree *) map) == NULL)
        return CC_ENOTSUP;

    void *duplicate = avltree_get_copier_fn((struct AVLTree *) map)(item);
    if (duplicate == NULL && item != NULL)
        return CC_ENOMEM;

    int err = genericmap_insert_move(map, key, key_len, duplicate);
    if (err)
        avltree_get_deleter_fn((struct AVLTree *) map)(duplicate);

    return err;
}

int genericmap_replace_move(GenericMap map, Iterator it, void *item) {
    struct AVLNode *node = it;

    avltree_get_deleter_fn((struct AVLTree *) map)(*avlnode_value(node));
    *avlnode_value(node) = item;
    return 0;
}

int genericmap_replace(GenericMap map, Iterator it, const void *item) {
    if (avltree_get_copier_fn((struct AVLTree *) map) == NULL)
        return CC_ENOTSUP;

    void *duplicate = avltree_get_copier_fn((struct AVLTree *) map)(item);
    if (duplicate == NULL && item != NULL)
        return CC_ENOMEM;

    int err = genericmap_replace_move(map, it, duplicate);
    if (err)
        avltree_get_deleter_fn((struct AVLTree *) map)(duplicate);

    return err;
}

int genericmap_contains(GenericMap map, const char *key, size_t key_len) {
    return avltree_find((struct AVLTree *) map, key, key_len) != NULL;
}

Iterator genericmap_find(GenericMap map, const char *key, size_t key_len) {
    return avltree_find((struct AVLTree *) map, key, key_len);
}

void genericmap_remove(GenericMap map, const char *key, size_t key_len) {
    avltree_delete((struct AVLTree *) map, key, key_len);
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

Binary genericmap_key_of(GenericMap map, Iterator it) {
    UNUSED(map)

    return avlnode_key((struct AVLNode *) it);
}

void *genericmap_value_of(GenericMap map, Iterator it) {
    UNUSED(map)

    return *avlnode_value((struct AVLNode *) it);
}

void *genericmap_value_of_key(GenericMap map, const char *key, size_t key_len) {
    Iterator it = genericmap_find(map, key, key_len);
    if (it == NULL)
        return NULL;

    return genericmap_value_of(map, it);
}

int genericmap_compare(GenericMap lhs, GenericMap rhs) {
    int cmp = generictypes_compatible_compare(avltree_get_value_compare_fn((struct AVLTree *) lhs), avltree_get_value_compare_fn((struct AVLTree *) rhs),
                                              avltree_get_copier_fn((struct AVLTree *) lhs), avltree_get_copier_fn((struct AVLTree *) rhs),
                                              avltree_get_deleter_fn((struct AVLTree *) lhs), avltree_get_deleter_fn((struct AVLTree *) rhs));
    if (cmp)
        return cmp;

    return avltree_compare((struct AVLTree *) lhs, (struct AVLTree *) rhs);
}

size_t genericmap_size(GenericMap map) {
    return avltree_size((struct AVLTree *) map);
}

BinaryCompare genericmap_get_key_compare_fn(GenericMap map) {
    return avltree_get_key_compare_fn((struct AVLTree *) map);
}

void genericmap_set_key_compare_fn(GenericMap map, BinaryCompare compare) {
    avltree_set_key_compare_fn((struct AVLTree *) map, compare);
}

Compare genericmap_get_value_compare_fn(GenericMap map) {
    return avltree_get_value_compare_fn((struct AVLTree *) map);
}

void genericmap_set_value_compare_fn(GenericMap map, Compare compare) {
    avltree_set_value_compare_fn((struct AVLTree *) map, compare);
}

Copier genericmap_get_copier_fn(GenericMap map) {
    return avltree_get_copier_fn((struct AVLTree *) map);
}

void genericmap_set_copier_fn(GenericMap map, Copier copier) {
    avltree_set_copier_fn((struct AVLTree *) map, copier);
}

Deleter genericmap_get_deleter_fn(GenericMap map) {
    return avltree_get_deleter_fn((struct AVLTree *) map);
}

void genericmap_set_deleter_fn(GenericMap map, Deleter deleter) {
    avltree_set_deleter_fn((struct AVLTree *) map, deleter);
}

void genericmap_clear(GenericMap map) {
    avltree_clear((struct AVLTree *) map);
}

void genericmap_destroy(GenericMap map) {
    avltree_destroy((struct AVLTree *) map);
}
