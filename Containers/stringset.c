/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "stringset.h"
#include "binaryset.h"

/* For conversions */
#include "variant.h"
#include "stringlist.h"

/* See common.c for reasoning behind this empty struct */
struct StringSetStruct {char dummy;};

Variant variant_from_stringset(StringSet set) {
    return variant_create_custom_base(set, (Compare) stringset_compare, (Copier) stringset_copy, (Deleter) stringset_destroy, *stringset_get_container_base(set));
}

int variant_is_stringset(Variant var) {
    return variant_get_copier_fn(var) == (Copier) stringset_copy;
}

StringSet variant_get_stringset(Variant var) {
    if (!variant_is_stringset(var))
        return NULL;

    return variant_get_custom(var);
}

int variant_set_stringset_move(Variant var, StringSet set) {
    return variant_set_custom_move_base(var, set, (Compare) stringset_compare, (Copier) stringset_copy, (Deleter) stringset_destroy, *stringset_get_container_base(set));
}

int variant_set_stringset(Variant var, StringSet set) {
    return variant_set_custom_base(var, set, (Compare) stringset_compare, (Copier) stringset_copy, (Deleter) stringset_destroy, *stringset_get_container_base(set));
}

StringSet stringset_create() {
    return (StringSet) binaryset_create();
}

StringSet stringset_create_custom(BinaryCompare compare) {
    return (StringSet) binaryset_create_custom(compare);
}

StringSet stringset_from_stringlist(StringList list, BinaryCompare compare) {
    StringSet set = stringset_create_custom(compare);

    if (set) {
        char **array = stringlist_array(list);
        for (size_t i = 0; i < stringlist_size(list); ++i) {
            if (stringset_add(set, array[i])) {
                stringset_destroy(set);
                return NULL;
            }
        }
    }

    return set;
}

StringSet stringset_copy(StringSet other) {
    return (StringSet) binaryset_copy((BinarySet) other);
}

StringSet stringset_union(StringSet a, StringSet b) {
    return (StringSet) binaryset_union((BinarySet) a, (BinarySet) b);
}

StringSet stringset_intersection(StringSet a, StringSet b) {
    return (StringSet) binaryset_intersection((BinarySet) a, (BinarySet) b);
}

StringSet stringset_subtract(StringSet from, StringSet set_to_subtract) {
    return (StringSet) binaryset_subtract((BinarySet) from, (BinarySet) set_to_subtract);
}

StringSet stringset_difference(StringSet a, StringSet b) {
    return (StringSet) binaryset_difference((BinarySet) a, (BinarySet) b);
}

int stringset_add(StringSet set, const char *item) {
    return binaryset_add((BinarySet) set, item, strlen(item));
}

int stringset_add_n(StringSet set, const char *item, size_t item_len) {
    if (memchr(item, 0, item_len) != NULL)
        return CC_EINVAL;

    return binaryset_add((BinarySet) set, item, item_len);
}

int stringset_contains(StringSet set, const char *item) {
    return binaryset_contains((BinarySet) set, item, strlen(item));
}

void stringset_remove(StringSet set, const char *item) {
    binaryset_remove((BinarySet) set, item, strlen(item));
}

Iterator stringset_erase(StringSet set, Iterator it) {
    return binaryset_erase((BinarySet) set, it);
}

Iterator stringset_begin(StringSet set) {
    return binaryset_begin((BinarySet) set);
}

Iterator stringset_next(StringSet set, Iterator it) {
    return binaryset_next((BinarySet) set, it);
}

const char *stringset_value_of(StringSet set, Iterator it) {
    return binaryset_value_of((BinarySet) set, it).data;
}

size_t stringset_size(StringSet set) {
    return binaryset_size((BinarySet) set);
}

int stringset_compare(StringSet lhs, StringSet rhs) {
    return binaryset_compare((BinarySet) lhs, (BinarySet) rhs);
}

BinaryCompare stringset_get_compare_fn(StringSet set) {
    return binaryset_get_compare_fn((BinarySet) set);
}

void stringset_set_compare_fn(StringSet set, BinaryCompare compare) {
    binaryset_set_compare_fn((BinarySet) set, compare);
}

void stringset_clear(StringSet set) {
    binaryset_clear((BinarySet) set);
}

void stringset_destroy(StringSet set) {
    binaryset_destroy((BinarySet) set);
}

CommonContainerBase *stringset_get_container_base(StringSet set) {
    return binaryset_get_container_base((BinarySet) set);
}
