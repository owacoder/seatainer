/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "stringlist.h"
#include "genericlist.h"
#include "../utility.h"
#include "recipes.h"

/* For conversions */
#include "variant.h"
#include "stringset.h"
#include "genericmap.h"
#include "stringmap.h"

Variant variant_from_stringlist(StringList list) {
    return variant_create_custom_adopt(list, stringlist_build_recipe(list));
}

int variant_is_stringlist(Variant var) {
    return generic_types_compatible_compare(variant_get_custom_container_base(var), container_base_stringlist_recipe());
}

StringList variant_get_stringlist(Variant var) {
    if (!variant_is_stringlist(var))
        return NULL;

    return variant_get_custom_data(var);
}

int variant_set_stringlist_move(Variant var, StringList list) {
    return variant_set_custom_move(var, list, stringlist_get_container_base(list));
}

int variant_set_stringlist(Variant var, const StringList list) {
    return variant_set_custom(var, list, stringlist_get_container_base(list));
}

StringList stringlist_create() {
    return stringlist_create_custom(NULL);
}

StringList stringlist_create_custom(const CommonContainerBase *base) {
    return stringlist_create_reserve(0, base);
}

StringList stringlist_create_reserve(size_t reserve, const CommonContainerBase *base) {
    return (StringList) genericlist_create_reserve(reserve, base? base: container_base_cstring_recipe());
}

StringList stringlist_concatenate(StringList left, StringList right) {
    return (StringList) genericlist_concatenate((GenericList) left, (GenericList) right);
}

StringList stringlist_split(const char *string, const char *separator, int keep_empty) {
    int done = 0;
    size_t separator_len = strlen(separator);
    StringList list = stringlist_create();
    if (!list)
        return NULL;

    if (!separator_len) { /* Empty separator, separate every character */
        while (*string) {
            if (stringlist_append_n(list, string, 1))
                goto cleanup;

            ++string;
        }

        return list;
    }

    while (!done) {
        const char *separator_location = strstr(string, separator);
        if (separator_location == NULL) {
            done = 1;
            separator_location = string + strlen(string);
        }

        size_t item_len = separator_location - string;
        if (item_len || keep_empty) {
            if (stringlist_append_n(list, string, separator_location - string))
                goto cleanup;
        }

        string = separator_location + separator_len;
    }

    return list;

cleanup:
    stringlist_destroy(list);
    return NULL;
}

StringList stringlist_divide(const char *string, size_t record_size, int keep_partial) {
    size_t len = strlen(string);

    if (len < record_size && !keep_partial)
        return stringlist_create();

    size_t records = record_size? len / record_size: 0;
    size_t partial_size = len - records * record_size;
    StringList list = stringlist_create_reserve(records + !!partial_size, container_base_cstring_recipe());
    if (!list)
        return NULL;

    if (record_size == 0 || record_size >= len) { /* Add entire string as item */
        if (stringlist_append(list, string))
            goto cleanup;

        return list;
    }

    for (size_t i = 0; i < records; ++i) {
        if (stringlist_append_n(list, string, record_size))
            goto cleanup;

        string += record_size;
    }

    if (partial_size && keep_partial) {
        if (stringlist_append_n(list, string, partial_size))
            goto cleanup;
    }

    return list;

cleanup:
    stringlist_destroy(list);
    return NULL;
}

StringList stringlist_copy(StringList other) {
    return (StringList) genericlist_copy((GenericList) other);
}

StringList stringlist_from_array(const char **strings) {
    return (StringList) genericlist_from_array((const void **) strings, container_base_cstring_recipe());
}

StringList stringlist_from_array_n(const char **strings, size_t count) {
    return (StringList) genericlist_from_array_n((const void **) strings, count, container_base_cstring_recipe());
}

StringList stringlist_from_stringset(StringSet other) {
    StringList list = stringlist_create_reserve(stringset_size(other), NULL);
    if (!list)
        return NULL;

    for (Iterator it = stringset_begin(other); it; it = stringset_next(other, it)) {
        if (stringlist_append(list, stringset_value_of(other, it))) {
            stringlist_destroy(list);
            return NULL;
        }
    }

    return list;
}

StringList stringlist_from_stringmap_keys(StringMap other) {
    StringList list = stringlist_create_reserve(stringmap_size(other), NULL);
    if (!list)
        return NULL;

    for (Iterator it = stringmap_begin(other); it; it = stringmap_next(other, it)) {
        if (stringlist_append(list, stringmap_key_of(other, it))) {
            stringlist_destroy(list);
            return NULL;
        }
    }

    return list;
}

StringList stringlist_from_stringmap_values(StringMap other) {
    StringList list = stringlist_create_reserve(stringmap_size(other), NULL);
    if (!list)
        return NULL;

    for (Iterator it = stringmap_begin(other); it; it = stringmap_next(other, it)) {
        if (stringlist_append(list, stringmap_value_of(other, it))) {
            stringlist_destroy(list);
            return NULL;
        }
    }

    return list;
}

StringList stringlist_create_filled(const char *item, size_t size) {
    return (StringList) genericlist_create_filled(item, size, container_base_cstring_recipe());
}

StringList stringlist_copy_slice(StringList other, size_t begin_index, size_t length) {
    return (StringList) genericlist_copy_slice((GenericList) other, begin_index, length);
}

int stringlist_fill(StringList list, const char *item, size_t size) {
    return genericlist_fill((GenericList) list, item, size);
}

int stringlist_resize(StringList list, size_t size, const char *empty_item) {
    return genericlist_resize((GenericList) list, size, empty_item);
}

int stringlist_append_list(StringList list, StringList other) {
    return genericlist_append_list((GenericList) list, (GenericList) other);
}

int stringlist_append_move(StringList list, char *item) {
    return genericlist_append_move((GenericList) list, item);
}

int stringlist_append(StringList list, const char *item) {
    return genericlist_append((GenericList) list, item);
}

int stringlist_append_n(StringList list, const char *item, size_t item_len) {
    if (memchr(item, 0, item_len) != NULL)
        return CC_EINVAL;

    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    int err = genericlist_append_move((GenericList) list, duplicate);
    if (err)
        FREE(duplicate);
    else {
        /* Copy string */
        memcpy(duplicate, item, item_len);
        duplicate[item_len] = 0;
    }

    return err;
}

int stringlist_insert_list(StringList list, StringList other, size_t before_index) {
    return genericlist_insert_list((GenericList) list, (GenericList) other, before_index);
}

int stringlist_insert_move(StringList list, char *item, size_t before_index) {
    return genericlist_insert_move((GenericList) list, item, before_index);
}

int stringlist_insert(StringList list, const char *item, size_t before_index) {
    return genericlist_insert((GenericList) list, item, before_index);
}

int stringlist_insert_n(StringList list, const char *item, size_t item_len, size_t before_index) {
    if (memchr(item, 0, item_len) != NULL)
        return CC_EINVAL;

    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    int err = genericlist_insert_move((GenericList) list, duplicate, before_index);
    if (err)
        FREE(duplicate);
    else {
        /* Copy string */
        memcpy(duplicate, item, item_len);
        duplicate[item_len] = 0;
    }

    return err;
}

int stringlist_replace_move_at(StringList list, size_t index, char *item) {
    return genericlist_replace_move_at((GenericList) list, index, item);
}

int stringlist_replace_at(StringList list, size_t index, const char *item) {
    return genericlist_replace_at((GenericList) list, index, item);
}

int stringlist_replace_at_n(StringList list, size_t index, const char *item, size_t item_len) {
    if (memchr(item, 0, item_len) != NULL)
        return CC_EINVAL;

    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    int err = genericlist_replace_move_at((GenericList) list, index, duplicate);
    if (err)
        FREE(duplicate);
    else {
        /* Copy string */
        memcpy(duplicate, item, item_len);
        duplicate[item_len] = 0;
    }

    return err;
}

size_t stringlist_remove_at(StringList list, size_t index) {
    return genericlist_remove_at((GenericList) list, index);
}

size_t stringlist_remove_one(StringList list, const char *item) {
    return genericlist_remove_one((GenericList) list, item);
}

size_t stringlist_remove_all(StringList list, const char *item) {
    return genericlist_remove_all((GenericList) list, item);
}

size_t stringlist_erase(StringList list, size_t begin_index, size_t end_index) {
    return genericlist_erase((GenericList) list, begin_index, end_index);
}

char **stringlist_array(StringList list) {
    /* This is possible because any type greater than or equal to sizeof(void*) will be stored directly as a pointer by genericlist
     * Since a char * is the same size as a void *, it is guaranteed that we hold a reference to a (NULL-terminated) (char **) by taking genericlist_data().
     *
     * In contrast, storing a short in a genericlist would require us to cast genericlist_data to a `short *` to get the 0-terminated list of shorts
     */
    return (char **) genericlist_data((GenericList) list);
}

int stringlist_contains(StringList list, const char *item) {
    return stringlist_find(list, item, 0) != SIZE_MAX;
}

size_t stringlist_find(StringList list, const char *item, size_t begin_index) {
    return genericlist_find((GenericList) list, item, begin_index);
}

size_t stringlist_rfind(StringList list, const char *item, size_t begin_index) {
    return genericlist_rfind((GenericList) list, item, begin_index);
}

int stringlist_compare(StringList list, StringList other) {
    return genericlist_compare((GenericList) list, (GenericList) other);
}

char *stringlist_join(StringList list, const char *separator) {
    return strjoin_alloc((const char **) stringlist_array(list), stringlist_size(list), separator);
}

StringList stringlist_sorted(StringList list, int descending) {
    return (StringList) genericlist_sorted((GenericList) list, descending);
}

StringList stringlist_stable_sorted(StringList list, int descending) {
    return (StringList) genericlist_stable_sorted((GenericList) list, descending);
}

int stringlist_sort(StringList list, int descending) {
    return genericlist_sort((GenericList) list, descending);
}

int stringlist_stable_sort(StringList list, int descending) {
    return genericlist_stable_sort((GenericList) list, descending);
}

void stringlist_reserve(StringList list, size_t size) {
    genericlist_reserve((GenericList) list, size);
}

Iterator stringlist_begin(StringList list) {
    return genericlist_begin((GenericList) list);
}

Iterator stringlist_next(StringList list, Iterator it) {
    return genericlist_next((GenericList) list, it);
}

char *stringlist_value_of(StringList list, Iterator it) {
    return (char *) genericlist_value_of((GenericList) list, it);
}

size_t stringlist_size(StringList list) {
    return genericlist_size((GenericList) list);
}

void stringlist_clear(StringList list) {
    genericlist_clear((GenericList) list);
}

void stringlist_destroy(StringList list) {
    genericlist_destroy((GenericList) list);
}

GenericList stringlist_cast_to_genericlist(StringList list) {
    return (GenericList) list;
}

const CommonContainerBase *stringlist_get_container_base(StringList list) {
    return genericlist_get_container_base((GenericList) list);
}

CommonContainerBase *stringlist_build_recipe(StringList list) {
    return container_base_build_container(stringlist_get_container_base(list),
                                          container_base_stringlist_recipe());
}
