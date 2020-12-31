/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "stringmap.h"
#include "genericmap.h"
#include "recipes.h"

/* For strdup */
#include "../utility.h"

/* For conversions */
#include "variant.h"

/* See common.c for reasoning behind this empty struct */
struct StringMapStruct {char dummy;};

Variant variant_from_stringmap(StringMap map) {
    return variant_create_custom_adopt(map, stringmap_build_recipe(map));
}

int variant_is_stringmap(Variant var) {
    return generic_types_compatible_compare(variant_get_custom_container_base(var), container_base_stringmap_recipe()) == 0;
}

StringMap variant_get_stringmap(Variant var) {
    if (!variant_is_stringmap(var))
        return NULL;

    return variant_get_custom_data(var);
}

int variant_set_stringmap_move(Variant var, StringMap map) {
    return variant_set_custom_move_adopt(var, map, stringmap_build_recipe(map));
}

int variant_set_stringmap(Variant var, const StringMap map) {
    return variant_set_custom_adopt(var, map, stringmap_build_recipe(map));
}

StringMap stringmap_create() {
    return stringmap_create_custom(NULL, NULL);
}

StringMap stringmap_create_custom(const CommonContainerBase *key_base, const CommonContainerBase *value_base) {
    return (StringMap) genericmap_create(key_base? key_base: container_base_cstring_recipe(), value_base? value_base: container_base_cstring_recipe());
}

StringMap stringmap_copy(StringMap other) {
    return (StringMap) genericmap_copy((GenericMap) other);
}

int stringmap_insert_move(StringMap map, const char *key, char *item) {
    return genericmap_insert_move((GenericMap) map, key, item);
}

int stringmap_insert(StringMap map, const char *key, const char *item) {
    return genericmap_insert((GenericMap) map, key, item);
}

int stringmap_insert_n(StringMap map, const char *key, const char *item, size_t item_len) {
    if (memchr(item, 0, item_len) != NULL)
        return CC_EINVAL;

    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    int err = stringmap_insert_move(map, key, duplicate);
    if (err)
        FREE(duplicate);
    else {
        /* Copy string */
        memcpy(duplicate, item, item_len);
        duplicate[item_len] = 0;
    }

    return err;
}

int stringmap_replace_move(StringMap map, Iterator it, char *item) {
    return genericmap_replace_move((GenericMap) map, it, item);
}

int stringmap_replace(StringMap map, Iterator it, const char *item) {
    return genericmap_replace((GenericMap) map, it, item);
}

int stringmap_replace_n(StringMap map, Iterator it, const char *item, size_t item_len) {
    if (memchr(item, 0, item_len) != NULL)
        return CC_EINVAL;

    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    int err = stringmap_replace_move(map, it, duplicate);
    if (err)
        FREE(duplicate);
    else {
        /* Copy string */
        memcpy(duplicate, item, item_len);
        duplicate[item_len] = 0;
    }

    return err;
}

int stringmap_contains(StringMap map, const char *key) {
    return genericmap_contains((GenericMap) map, key);
}

void stringmap_remove(StringMap map, const char *key) {
    genericmap_remove((GenericMap) map, key);
}

Iterator stringmap_find(StringMap map, const char *key) {
    return genericmap_find((GenericMap) map, key);
}

Iterator stringmap_erase(StringMap map, Iterator it) {
    return genericmap_erase((GenericMap) map, it);
}

Iterator stringmap_begin(StringMap map) {
    return genericmap_begin((GenericMap) map);
}

Iterator stringmap_next(StringMap map, Iterator it) {
    return genericmap_next((GenericMap) map, it);
}

const char *stringmap_key_of(StringMap map, Iterator it) {
    return genericmap_key_of((GenericMap) map, it);
}

char *stringmap_value_of(StringMap map, Iterator it) {
    return genericmap_value_of((GenericMap) map, it);
}

char *stringmap_value_of_key(StringMap map, const char *key) {
    return genericmap_value_of_key((GenericMap) map, key);
}

size_t stringmap_size(StringMap map) {
    return genericmap_size((GenericMap) map);
}

int stringmap_compare(StringMap lhs, StringMap rhs) {
    return genericmap_compare((GenericMap) lhs, (GenericMap) rhs);
}

Compare stringmap_get_key_compare_fn(StringMap map) {
    return genericmap_get_key_compare_fn((GenericMap) map);
}

void stringmap_set_key_compare_fn(StringMap map, Compare compare) {
    genericmap_set_key_compare_fn((GenericMap) map, compare);
}

Compare stringmap_get_value_compare_fn(StringMap map) {
    return genericmap_get_value_compare_fn((GenericMap) map);
}

void stringmap_set_value_compare_fn(StringMap map, Compare compare) {
    genericmap_set_value_compare_fn((GenericMap) map, (Compare) compare);
}

void stringmap_clear(StringMap map) {
    genericmap_clear((GenericMap) map);
}

void stringmap_destroy(StringMap map) {
    genericmap_destroy((GenericMap) map);
}

GenericMap stringmap_cast_to_genericmap(StringMap map) {
    return (GenericMap) map;
}

const CommonContainerBase *stringmap_get_key_container_base(StringMap map) {
    return genericmap_get_key_container_base((GenericMap) map);
}

const CommonContainerBase *stringmap_get_value_container_base(StringMap map) {
    return genericmap_get_value_container_base((GenericMap) map);
}

CommonContainerBase *stringmap_build_recipe(StringMap map) {
    return container_base_build_key_value_container(stringmap_get_key_container_base(map),
                                                    stringmap_get_value_container_base(map),
                                                    container_base_stringmap_recipe());
}
