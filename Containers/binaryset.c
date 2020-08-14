/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "binaryset.h"
#include "impl/avl.h"

/* For conversions */
#include "variant.h"
#include "binarylist.h"

/* See common.c for reasoning behind this empty struct */
struct BinarySetStruct {};

Variant variant_from_binaryset(BinarySet set) {
    return variant_create_custom(set, (Compare) binaryset_compare, (Copier) binaryset_copy, (Deleter) binaryset_destroy);
}

int variant_is_binaryset(Variant var) {
    return variant_get_copier_fn(var) == (Copier) binaryset_copy;
}

BinarySet variant_get_binaryset(Variant var) {
    if (!variant_is_binaryset(var))
        return NULL;

    return variant_get_custom(var);
}

int variant_set_binaryset_move(Variant var, BinarySet set) {
    return variant_set_custom_move(var, set, (Compare) binaryset_compare, (Copier) binaryset_copy, (Deleter) binaryset_destroy);
}

int variant_set_binaryset(Variant var, const BinarySet set) {
    return variant_set_custom(var, set, (Compare) binaryset_compare, (Copier) binaryset_copy, (Deleter) binaryset_destroy);
}

BinarySet binaryset_create() {
    return binaryset_create_custom(NULL);
}

BinarySet binaryset_create_custom(BinaryCompare compare) {
    BinarySet set = (BinarySet) avltree_create();
    if (set)
        avltree_set_key_compare_fn((struct AVLTree *) set, compare);

    return set;
}

BinarySet binaryset_from_binarylist(BinaryList list) {
    BinarySet set = binaryset_create_custom(binarylist_get_compare_fn(list));

    if (set) {
        const Binary *array = binarylist_array(list);
        for (size_t i = 0; i < binarylist_size(list); ++i) {
            if (binaryset_add(set, array[i].data, array[i].length)) {
                binaryset_destroy(set);
                return NULL;
            }
        }
    }

    return set;
}

BinarySet binaryset_copy(BinarySet other) {
    return (BinarySet) avltree_copy((struct AVLTree *) other);
}

BinarySet binaryset_union(BinarySet a, BinarySet b) {
    BinaryCompare compare = binaryset_get_compare_fn(a);
    BinarySet result = binaryset_create_custom(compare);
    if (result == NULL)
        return NULL;

    Iterator lhs = binaryset_begin(a);
    Iterator rhs = binaryset_begin(b);
    while (lhs && rhs) {
        const Binary lhs_value = binaryset_value_of(a, lhs);
        const Binary rhs_value = binaryset_value_of(b, rhs);
        int cmp = compare(&lhs_value, &rhs_value);
        int err = 0;

        if (cmp < 0) {
            err = binaryset_add_binary(result, lhs_value);
            lhs = binaryset_next(a, lhs);
        } else if (cmp > 0) {
            err = binaryset_add_binary(result, rhs_value);
            rhs = binaryset_next(b, rhs);
        } else {
            err = binaryset_add_binary(result, lhs_value);
            lhs = binaryset_next(a, lhs);
            rhs = binaryset_next(b, rhs);
        }

        if (err) {
            binaryset_destroy(result);
            return NULL;
        }
    }

    while (lhs) {
        if (binaryset_add_binary(result, binaryset_value_of(a, lhs))) {
            binaryset_destroy(result);
            return NULL;
        }

        lhs = binaryset_next(a, lhs);
    }

    while (rhs) {
        if (binaryset_add_binary(result, binaryset_value_of(b, rhs))) {
            binaryset_destroy(result);
            return NULL;
        }

        rhs = binaryset_next(b, rhs);
    }

    return result;
}

BinarySet binaryset_intersection(BinarySet a, BinarySet b) {
    BinaryCompare compare = binaryset_get_compare_fn(a);
    BinarySet result = binaryset_create_custom(compare);
    if (result == NULL)
        return NULL;

    Iterator lhs = binaryset_begin(a);
    Iterator rhs = binaryset_begin(b);
    while (lhs && rhs) {
        const Binary lhs_value = binaryset_value_of(a, lhs);
        const Binary rhs_value = binaryset_value_of(b, rhs);
        int cmp = compare(&lhs_value, &rhs_value);

        if (cmp < 0) {
            lhs = binaryset_next(a, lhs);
        } else if (cmp > 0) {
            rhs = binaryset_next(b, rhs);
        } else {
            if (binaryset_add_binary(result, lhs_value)) {
                binaryset_destroy(result);
                return NULL;
            }
            lhs = binaryset_next(a, lhs);
            rhs = binaryset_next(b, rhs);
        }
    }

    return result;
}

BinarySet binaryset_subtract(BinarySet from, BinarySet set_to_subtract) {
    BinaryCompare compare = binaryset_get_compare_fn(from);
    BinarySet result = binaryset_create_custom(compare);
    if (result == NULL)
        return NULL;

    Iterator lhs = binaryset_begin(from);
    Iterator rhs = binaryset_begin(set_to_subtract);
    while (lhs && rhs) {
        const Binary lhs_value = binaryset_value_of(from, lhs);
        const Binary rhs_value = binaryset_value_of(set_to_subtract, rhs);
        int cmp = compare(&lhs_value, &rhs_value);

        if (cmp < 0) {
            if (binaryset_add_binary(result, lhs_value)) {
                binaryset_destroy(result);
                return NULL;
            }
            lhs = binaryset_next(from, lhs);
        } else if (cmp > 0) {
            rhs = binaryset_next(set_to_subtract, rhs);
        } else {
            lhs = binaryset_next(from, lhs);
            rhs = binaryset_next(set_to_subtract, rhs);
        }
    }

    while (lhs) {
        if (binaryset_add_binary(result, binaryset_value_of(from, lhs))) {
            binaryset_destroy(result);
            return NULL;
        }

        lhs = binaryset_next(from, lhs);
    }

    return result;
}

BinarySet binaryset_difference(BinarySet a, BinarySet b) {
    BinaryCompare compare = binaryset_get_compare_fn(a);
    BinarySet result = binaryset_create_custom(compare);
    if (result == NULL)
        return NULL;

    Iterator lhs = binaryset_begin(a);
    Iterator rhs = binaryset_begin(b);
    while (lhs && rhs) {
        const Binary lhs_value = binaryset_value_of(a, lhs);
        const Binary rhs_value = binaryset_value_of(b, rhs);
        int cmp = compare(&lhs_value, &rhs_value);
        int err = 0;

        if (cmp < 0) {
            err = binaryset_add_binary(result, lhs_value);
            lhs = binaryset_next(a, lhs);
        } else if (cmp > 0) {
            err = binaryset_add_binary(result, rhs_value);
            rhs = binaryset_next(b, rhs);
        } else {
            lhs = binaryset_next(a, lhs);
            rhs = binaryset_next(b, rhs);
        }

        if (err) {
            binaryset_destroy(result);
            return NULL;
        }
    }

    while (lhs) {
        if (binaryset_add_binary(result, binaryset_value_of(a, lhs))) {
            binaryset_destroy(result);
            return NULL;
        }

        lhs = binaryset_next(a, lhs);
    }

    while (rhs) {
        if (binaryset_add_binary(result, binaryset_value_of(b, rhs))) {
            binaryset_destroy(result);
            return NULL;
        }

        rhs = binaryset_next(b, rhs);
    }

    return result;
}

int binaryset_add(BinarySet set, const char *item, size_t item_len) {
    if (avltree_insert((struct AVLTree *) set, item, item_len, NULL) == NULL)
        return CC_ENOMEM;

    return 0;
}

int binaryset_add_binary(BinarySet set, const Binary item) {
    return binaryset_add(set, item.data, item.length);
}

int binaryset_contains(BinarySet set, const char *item, size_t item_len) {
    return avltree_find((struct AVLTree *) set, item, item_len) != NULL;
}

int binaryset_contains_binary(BinarySet set, const Binary item) {
    return binaryset_contains(set, item.data, item.length);
}

void binaryset_remove(BinarySet set, const char *item, size_t item_len) {
    avltree_delete((struct AVLTree *) set, item, item_len);
}

void binaryset_remove_binary(BinarySet set, const Binary item) {
    binaryset_remove(set, item.data, item.length);
}

Iterator binaryset_erase(BinarySet set, Iterator it) {
    return avltree_delete_node((struct AVLTree *) set, (struct AVLNode *) it);
}

Iterator binaryset_begin(BinarySet set) {
    return avltree_min_node((struct AVLTree *) set);
}

Iterator binaryset_next(BinarySet set, Iterator it) {
    UNUSED(set)

    if (it == NULL)
        return NULL;

    return avltree_inorder_next((struct AVLNode *) it);
}

Binary binaryset_value_of(BinarySet set, Iterator it) {
    UNUSED(set)

    return avlnode_key((struct AVLNode *) it);
}

size_t binaryset_size(BinarySet set) {
    return avltree_size((struct AVLTree *) set);
}

int binaryset_compare(BinarySet lhs, BinarySet rhs) {
    return avltree_compare((struct AVLTree *) lhs, (struct AVLTree *) rhs);
}

BinaryCompare binaryset_get_compare_fn(BinarySet set) {
    return avltree_get_key_compare_fn((struct AVLTree *) set);
}

void binaryset_set_compare_fn(BinarySet set, BinaryCompare compare) {
    avltree_set_key_compare_fn((struct AVLTree *) set, compare);
}

void binaryset_clear(BinarySet set) {
    avltree_clear((struct AVLTree *) set);
}

void binaryset_destroy(BinarySet set) {
    avltree_destroy((struct AVLTree *) set);
}
