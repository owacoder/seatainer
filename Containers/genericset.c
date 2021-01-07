/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "genericset.h"
#include "impl/avl.h"
#include "recipes.h"

/* For conversions */
#include "variant.h"

/* See common.c for reasoning behind this empty struct */
struct GenericSetStruct {char dummy;};

Variant variant_from_genericset(GenericSet set) {
    return variant_create_custom_adopt(set, genericset_build_recipe(set));
}

int variant_is_genericset(Variant var) {
    return generic_types_compatible_compare(variant_get_custom_container_base(var), container_base_genericset_recipe()) == 0;
}

int variant_is_variantset(Variant var) {
    return generic_types_compatible_compare(variant_get_custom_container_base(var), container_base_variantset_recipe()) == 0;
}

GenericSet variant_get_genericset(Variant var) {
    if (!variant_is_genericset(var))
        return NULL;

    return variant_get_custom_data(var);
}

int variant_set_genericset_move(Variant var, GenericSet set) {
    return variant_set_custom_move_adopt(var, set, genericset_build_recipe(set));
}

int variant_set_genericset(Variant var, const GenericSet set) {
    return variant_set_custom_adopt(var, set, genericset_build_recipe(set));
}

GenericSet genericset_create(const CommonContainerBase *base) {
    return (GenericSet) avltree_create(base, container_base_empty_recipe());
}

GenericSet genericset_copy(GenericSet other) {
    return (GenericSet) avltree_copy((struct AVLTree *) other);
}

GenericSet genericset_union(GenericSet a, GenericSet b) {
    if (generic_types_compatible_compare(avltree_get_key_container_base((struct AVLTree *) a), avltree_get_key_container_base((struct AVLTree *) b)) != 0)
        return NULL;

    const CommonContainerBase *base = avltree_get_key_container_base((struct AVLTree *) a);
    GenericSet result = genericset_create(base);
    if (result == NULL)
        return NULL;

    Iterator lhs = genericset_begin(a);
    Iterator rhs = genericset_begin(b);
    while (lhs && rhs) {
        const void *lhs_value = genericset_value_of(a, lhs);
        const void *rhs_value = genericset_value_of(b, rhs);
        int cmp = base->compare(lhs_value, rhs_value);
        int err = 0;

        if (cmp < 0) {
            err = genericset_add(result, lhs_value);
            lhs = genericset_next(a, lhs);
        } else if (cmp > 0) {
            err = genericset_add(result, rhs_value);
            rhs = genericset_next(b, rhs);
        } else {
            err = genericset_add(result, lhs_value);
            lhs = genericset_next(a, lhs);
            rhs = genericset_next(b, rhs);
        }

        if (err) {
            genericset_destroy(result);
            return NULL;
        }
    }

    while (lhs) {
        if (genericset_add(result, genericset_value_of(a, lhs))) {
            genericset_destroy(result);
            return NULL;
        }

        lhs = genericset_next(a, lhs);
    }

    while (rhs) {
        if (genericset_add(result, genericset_value_of(b, rhs))) {
            genericset_destroy(result);
            return NULL;
        }

        rhs = genericset_next(b, rhs);
    }

    return result;
}

GenericSet genericset_intersection(GenericSet a, GenericSet b) {
    if (generic_types_compatible_compare(avltree_get_key_container_base((struct AVLTree *) a), avltree_get_key_container_base((struct AVLTree *) b)) != 0)
        return NULL;

    const CommonContainerBase *base = avltree_get_key_container_base((struct AVLTree *) a);
    GenericSet result = genericset_create(base);
    if (result == NULL)
        return NULL;

    Iterator lhs = genericset_begin(a);
    Iterator rhs = genericset_begin(b);
    while (lhs && rhs) {
        const void *lhs_value = genericset_value_of(a, lhs);
        const void *rhs_value = genericset_value_of(b, rhs);
        int cmp = base->compare(lhs_value, rhs_value);

        if (cmp < 0) {
            lhs = genericset_next(a, lhs);
        } else if (cmp > 0) {
            rhs = genericset_next(b, rhs);
        } else {
            if (genericset_add(result, lhs_value)) {
                genericset_destroy(result);
                return NULL;
            }
            lhs = genericset_next(a, lhs);
            rhs = genericset_next(b, rhs);
        }
    }

    return result;
}

GenericSet genericset_subtract(GenericSet from, GenericSet set_to_subtract) {
    if (generic_types_compatible_compare(avltree_get_key_container_base((struct AVLTree *) from), avltree_get_key_container_base((struct AVLTree *) set_to_subtract)) != 0)
        return NULL;

    const CommonContainerBase *base = avltree_get_key_container_base((struct AVLTree *) from);
    GenericSet result = genericset_create(base);
    if (result == NULL)
        return NULL;

    Iterator lhs = genericset_begin(from);
    Iterator rhs = genericset_begin(set_to_subtract);
    while (lhs && rhs) {
        const void *lhs_value = genericset_value_of(from, lhs);
        const void *rhs_value = genericset_value_of(set_to_subtract, rhs);
        int cmp = base->compare(lhs_value, rhs_value);

        if (cmp < 0) {
            if (genericset_add(result, lhs_value)) {
                genericset_destroy(result);
                return NULL;
            }
            lhs = genericset_next(from, lhs);
        } else if (cmp > 0) {
            rhs = genericset_next(set_to_subtract, rhs);
        } else {
            lhs = genericset_next(from, lhs);
            rhs = genericset_next(set_to_subtract, rhs);
        }
    }

    while (lhs) {
        if (genericset_add(result, genericset_value_of(from, lhs))) {
            genericset_destroy(result);
            return NULL;
        }

        lhs = genericset_next(from, lhs);
    }

    return result;
}

GenericSet genericset_difference(GenericSet a, GenericSet b) {
    if (generic_types_compatible_compare(avltree_get_key_container_base((struct AVLTree *) a), avltree_get_key_container_base((struct AVLTree *) b)) != 0)
        return NULL;

    const CommonContainerBase *base = avltree_get_key_container_base((struct AVLTree *) a);
    GenericSet result = genericset_create(base);
    if (result == NULL)
        return NULL;

    Iterator lhs = genericset_begin(a);
    Iterator rhs = genericset_begin(b);
    while (lhs && rhs) {
        const void *lhs_value = genericset_value_of(a, lhs);
        const void *rhs_value = genericset_value_of(b, rhs);
        int cmp = base->compare(lhs_value, rhs_value);
        int err = 0;

        if (cmp < 0) {
            err = genericset_add(result, lhs_value);
            lhs = genericset_next(a, lhs);
        } else if (cmp > 0) {
            err = genericset_add(result, rhs_value);
            rhs = genericset_next(b, rhs);
        } else {
            lhs = genericset_next(a, lhs);
            rhs = genericset_next(b, rhs);
        }

        if (err) {
            genericset_destroy(result);
            return NULL;
        }
    }

    while (lhs) {
        if (genericset_add(result, genericset_value_of(a, lhs))) {
            genericset_destroy(result);
            return NULL;
        }

        lhs = genericset_next(a, lhs);
    }

    while (rhs) {
        if (genericset_add(result, genericset_value_of(b, rhs))) {
            genericset_destroy(result);
            return NULL;
        }

        rhs = genericset_next(b, rhs);
    }

    return result;
}

int genericset_add(GenericSet set, const void *item) {
    if (avltree_insert_copy_key((struct AVLTree *) set, item, NULL) == NULL)
        return CC_ENOMEM;

    return 0;
}

int genericset_add_move(GenericSet set, void *item) {
    if (avltree_insert_move_key((struct AVLTree *) set, item, NULL) == NULL)
        return CC_ENOMEM;

    return 0;
}

Iterator genericset_find(GenericSet set, const void *item) {
    return avltree_find((struct AVLTree *) set, item);
}

int genericset_contains(GenericSet set, const void *item) {
    return genericset_find(set, item) != NULL;
}

void genericset_remove(GenericSet set, const void *item) {
    avltree_delete((struct AVLTree *) set, item);
}

Iterator genericset_erase(GenericSet set, Iterator it) {
    return avltree_delete_node((struct AVLTree *) set, (struct AVLNode *) it);
}

Iterator genericset_begin(GenericSet set) {
    return avltree_min_node((struct AVLTree *) set);
}

Iterator genericset_next(GenericSet set, Iterator it) {
    UNUSED(set)

    if (it == NULL)
        return NULL;

    return avltree_inorder_next((struct AVLNode *) it);
}

const void *genericset_value_of(GenericSet set, Iterator it) {
    UNUSED(set)

    return avlnode_key((struct AVLNode *) it);
}

size_t genericset_size(GenericSet set) {
    return avltree_size((struct AVLTree *) set);
}

int genericset_compare(GenericSet lhs, GenericSet rhs) {
    return avltree_compare((struct AVLTree *) lhs, (struct AVLTree *) rhs);
}

void genericset_clear(GenericSet set) {
    avltree_clear((struct AVLTree *) set);
}

void genericset_destroy(GenericSet set) {
    avltree_destroy((struct AVLTree *) set);
}

const CommonContainerBase *genericset_get_container_base(GenericSet set) {
    return avltree_get_key_container_base((struct AVLTree *) set);
}

CommonContainerBase *genericset_build_recipe(GenericSet set) {
    return container_base_build_container(genericset_get_container_base(set), container_base_genericset_recipe());
}

Compare genericset_get_compare_fn(GenericSet set) {
    return avltree_get_key_compare_fn((struct AVLTree *) set);
}

int genericset_set_compare_fn(GenericSet set, Compare compare) {
    return avltree_set_key_compare_fn((struct AVLTree *) set, compare);
}

Copier genericset_get_copier_fn(GenericSet set) {
    return avltree_get_key_copier_fn((struct AVLTree *) set);
}

int genericset_set_copier_fn(GenericSet set, Copier copier) {
    return avltree_set_key_copier_fn((struct AVLTree *) set, copier);
}

Deleter genericset_get_deleter_fn(GenericSet set) {
    return avltree_get_key_deleter_fn((struct AVLTree *) set);
}

int genericset_set_deleter_fn(GenericSet set, Deleter deleter) {
    return avltree_set_key_deleter_fn((struct AVLTree *) set, deleter);
}

Parser genericset_get_parser_fn(GenericSet set) {
    return avltree_get_key_parser_fn((struct AVLTree *) set);
}

int genericset_set_parser_fn(GenericSet set, Parser parser) {
    return avltree_set_key_parser_fn((struct AVLTree *) set, parser);
}

Serializer genericset_get_serializer_fn(GenericSet set) {
    return avltree_get_key_serializer_fn((struct AVLTree *) set);
}

int genericset_set_serializer_fn(GenericSet set, Serializer serializer) {
    return avltree_set_key_serializer_fn((struct AVLTree *) set, serializer);
}
