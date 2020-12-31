/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "stringset.h"
#include "genericset.h"
#include "recipes.h"

/* For conversions */
#include "variant.h"
#include "stringlist.h"

/* See common.c for reasoning behind this empty struct */
struct StringSetStruct {char dummy;};

Variant variant_from_stringset(StringSet set) {
    return variant_create_custom_adopt(set, stringset_build_recipe(set));
}

int variant_is_stringset(Variant var) {
    return generic_types_compatible_compare(variant_get_custom_container_base(var), container_base_stringset_recipe()) == 0;
}

StringSet variant_get_stringset(Variant var) {
    if (!variant_is_stringset(var))
        return NULL;

    return variant_get_custom_data(var);
}

int variant_set_stringset_move(Variant var, StringSet set) {
    return variant_set_custom_move_adopt(var, set, stringset_build_recipe(set));
}

int variant_set_stringset(Variant var, StringSet set) {
    return variant_set_custom_adopt(var, set, stringset_build_recipe(set));
}

StringSet stringset_create() {
    return (StringSet) genericset_create();
}

StringSet stringset_create_custom(const CommonContainerBase *base) {
    return (StringSet) genericset_create_custom(base? base: container_base_cstring_recipe());
}

StringSet stringset_from_stringlist(StringList list, const CommonContainerBase *base) {
    StringSet set = stringset_create_custom(base);

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
    return (StringSet) genericset_copy((GenericSet) other);
}

StringSet stringset_union(StringSet a, StringSet b) {
    return (StringSet) genericset_union((GenericSet) a, (GenericSet) b);
}

StringSet stringset_intersection(StringSet a, StringSet b) {
    return (StringSet) genericset_intersection((GenericSet) a, (GenericSet) b);
}

StringSet stringset_subtract(StringSet from, StringSet set_to_subtract) {
    return (StringSet) genericset_subtract((GenericSet) from, (GenericSet) set_to_subtract);
}

StringSet stringset_difference(StringSet a, StringSet b) {
    return (StringSet) genericset_difference((GenericSet) a, (GenericSet) b);
}

int stringset_add(StringSet set, const char *item) {
    return genericset_add((GenericSet) set, item);
}

int stringset_add_n(StringSet set, const char *item, size_t item_len) {
    if (memchr(item, 0, item_len) != NULL)
        return CC_EINVAL;

    return genericset_add((GenericSet) set, item);
}

int stringset_contains(StringSet set, const char *item) {
    return genericset_contains((GenericSet) set, item);
}

void stringset_remove(StringSet set, const char *item) {
    genericset_remove((GenericSet) set, item);
}

Iterator stringset_erase(StringSet set, Iterator it) {
    return genericset_erase((GenericSet) set, it);
}

Iterator stringset_begin(StringSet set) {
    return genericset_begin((GenericSet) set);
}

Iterator stringset_next(StringSet set, Iterator it) {
    return genericset_next((GenericSet) set, it);
}

const char *stringset_value_of(StringSet set, Iterator it) {
    return genericset_value_of((GenericSet) set, it);
}

size_t stringset_size(StringSet set) {
    return genericset_size((GenericSet) set);
}

int stringset_compare(StringSet lhs, StringSet rhs) {
    return genericset_compare((GenericSet) lhs, (GenericSet) rhs);
}

void stringset_clear(StringSet set) {
    genericset_clear((GenericSet) set);
}

void stringset_destroy(StringSet set) {
    genericset_destroy((GenericSet) set);
}

GenericSet stringset_cast_to_genericset(StringSet set) {
    return (GenericSet) set;
}

const CommonContainerBase *stringset_get_container_base(StringSet set) {
    return genericset_get_container_base((GenericSet) set);
}

CommonContainerBase *stringset_build_recipe(StringSet set) {
    return container_base_build_container(stringset_get_container_base(set), container_base_stringset_recipe());
}
