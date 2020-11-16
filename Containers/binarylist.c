/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "binarylist.h"
#include "../utility.h"

/* For conversions */
#include "variant.h"
#include "stringlist.h"
#include "binaryset.h"
#include "genericmap.h"
#include "stringmap.h"

struct BinaryListStruct {
    CommonContainerBase base;
    Binary *array;
    BinaryCompare compare;
    size_t array_size;
    size_t array_capacity;
};

Variant variant_from_binarylist(BinaryList list) {
    return variant_create_custom_base(list, (Compare) binarylist_compare, (Copier) binarylist_copy, (Deleter) binarylist_destroy, *binarylist_get_container_base(list));
}

int variant_is_binarylist(Variant var) {
    return variant_get_copier_fn(var) == (Copier) binarylist_copy;
}

BinaryList variant_get_binarylist(Variant var) {
    if (!variant_is_binarylist(var))
        return NULL;

    return variant_get_custom(var);
}

int variant_set_binarylist_move(Variant var, BinaryList list) {
    return variant_set_custom_move_base(var, list, (Compare) binarylist_compare, (Copier) binarylist_copy, (Deleter) binarylist_destroy, *binarylist_get_container_base(list));
}

int variant_set_binarylist(Variant var, const BinaryList list) {
    return variant_set_custom_base(var, list, (Compare) binarylist_compare, (Copier) binarylist_copy, (Deleter) binarylist_destroy, *binarylist_get_container_base(list));
}

static int binarylist_grow(BinaryList list, size_t added) {
    if (list->array_size+added >= list->array_capacity) {
        size_t new_capacity = MAX(list->array_capacity + (list->array_capacity / 2), list->array_size + added) + 1;
        size_t new_size = safe_multiply(new_capacity, sizeof(*list->array));
        if (!new_size || new_capacity < list->array_capacity)
            return CC_ENOMEM;

        Binary *new_array = REALLOC(list->array, new_size);
        if (!new_array)
            return CC_ENOMEM;

        list->array_capacity = new_capacity;
        list->array = new_array;
    }

    return 0;
}

BinaryList binarylist_create() {
    return binarylist_create_custom(NULL);
}

BinaryList binarylist_create_custom(BinaryCompare compare) {
    return binarylist_create_reserve(0, compare);
}

BinaryList binarylist_create_reserve(size_t reserve, BinaryCompare compare) {
    const size_t minimum_size = 8;
    BinaryList list = CALLOC(1, sizeof(*list));
    if (!list)
        return NULL;

    list->array_capacity = MAX(minimum_size, reserve);

    while (1) {
        list->array = MALLOC(list->array_capacity * sizeof(*list->array));
        if (list->array == NULL) {
            if (list->array_capacity > minimum_size) {
                list->array_capacity = minimum_size; /* Fall back to minimum size if trying to allocate too much */
                continue;
            }

            FREE(list);
            return NULL;
        }
        list->array[0].data = NULL;
        list->array[0].length = 0;
        list->compare = compare? compare: binary_compare;

        return list;
    }
}

BinaryList binarylist_split(const char *string, size_t string_len, const char *separator, size_t separator_len, int keep_empty) {
    int done = 0;
    BinaryList list = binarylist_create();
    if (!list)
        return NULL;

    if (!separator_len) { /* Empty separator, separate every character */
        for (; string_len; --string_len) {
            if (binarylist_append(list, string, 1))
                goto cleanup;

            ++string;
        }

        return list;
    }

    while (!done) {
        const char *separator_location = binstr_search(string, &string_len, separator, separator_len);
        if (separator_location == NULL) {
            done = 1;
            separator_location = string + string_len;
        }

        size_t item_len = separator_location - string;
        if (item_len || keep_empty) {
            if (binarylist_append(list, string, separator_location - string))
                goto cleanup;
        }

        string = separator_location + separator_len;
    }

    return list;

cleanup:
    binarylist_destroy(list);
    return NULL;
}

BinaryList binarylist_divide(const char *binary, size_t binary_len, size_t record_size, int keep_partial) {
    if (binary_len < record_size && !keep_partial)
        return binarylist_create();

    size_t records = record_size? binary_len / record_size: 0;
    size_t partial_size = binary_len - records * record_size;
    BinaryList list = binarylist_create_reserve(records + !!partial_size, binary_compare);
    if (!list)
        return NULL;

    if (record_size == 0 || record_size >= binary_len) { /* Add entire binary as item */
        if (binarylist_append(list, binary, binary_len))
            goto cleanup;

        return list;
    }

    for (size_t i = 0; i < records; ++i) {
        if (binarylist_append(list, binary, record_size))
            goto cleanup;

        binary += record_size;
    }

    if (partial_size && keep_partial) {
        if (binarylist_append(list, binary, partial_size))
            goto cleanup;
    }

    return list;

cleanup:
    binarylist_destroy(list);
    return NULL;
}

BinaryList binarylist_copy(BinaryList other) {
    Binary *array = binarylist_array(other);
    BinaryList list = binarylist_create_reserve(binarylist_size(other), binarylist_get_compare_fn(other));
    if (!list)
        return NULL;

    for (size_t i = 0; i < binarylist_size(other); ++i)
        if (binarylist_append(list, array[i].data, array[i].length)) {
            binarylist_destroy(list);
            return NULL;
        }

    return list;
}

BinaryList binarylist_copy_slice(BinaryList other, size_t begin_index, size_t length) {
    if (begin_index > binarylist_size(other))
        begin_index = binarylist_size(other);

    if (binarylist_size(other) - begin_index < length)
        length = binarylist_size(other) - begin_index;

    BinaryList list = binarylist_create_reserve(length, binarylist_get_compare_fn(other));
    if (list == NULL)
        return NULL;

    const Binary *array = binarylist_array(other);
    for (size_t i = 0; i < length; ++i) {
        if (binarylist_append_binary(list, array[begin_index+i])) {
            binarylist_destroy(list);
            return NULL;
        }
    }

    return list;
}

BinaryList binarylist_concatenate(BinaryList left, BinaryList right) {
    BinaryList result = binarylist_create_reserve(binarylist_size(left) + binarylist_size(right),
                                                  binarylist_get_compare_fn(left));

    if (result == NULL)
        return NULL;

    Binary *array = binarylist_array(left);
    for (size_t i = 0; i < binarylist_size(left); ++i)
        if (binarylist_append_binary(result, array[i])) {
            binarylist_destroy(result);
            return NULL;
        }

    array = binarylist_array(right);
    for (size_t i = 0; i < binarylist_size(right); ++i)
        if (binarylist_append_binary(result, array[i])) {
            binarylist_destroy(result);
            return NULL;
        }

    return result;
}

BinaryList binarylist_from_stringlist(StringList other) {
    char **array = stringlist_array(other);
    BinaryList list = binarylist_create_reserve(stringlist_size(other), NULL);
    if (!list)
        return NULL;

    for (size_t i = 0; i < stringlist_size(other); ++i) {
        if (binarylist_append(list, array[i], strlen(array[i]))) {
            binarylist_destroy(list);
            return NULL;
        }
    }

    return list;
}

BinaryList binarylist_from_binaryset(BinarySet other) {
    BinaryList list = binarylist_create_reserve(binaryset_size(other), binaryset_get_compare_fn(other));
    if (!list)
        return NULL;

    for (Iterator it = binaryset_begin(other); it; it = binaryset_next(other, it)) {
        if (binarylist_append_binary(list, binaryset_value_of(other, it))) {
            binarylist_destroy(list);
            return NULL;
        }
    }

    return list;
}

BinaryList binarylist_from_genericmap_keys(GenericMap other) {
    BinaryList list = binarylist_create_reserve(genericmap_size(other), genericmap_get_key_compare_fn(other));
    if (!list)
        return NULL;

    for (Iterator it = genericmap_begin(other); it; it = genericmap_next(other, it)) {
        if (binarylist_append_binary(list, genericmap_key_of(other, it))) {
            binarylist_destroy(list);
            return NULL;
        }
    }

    return list;
}

BinaryList binarylist_from_stringmap_keys(StringMap other) {
    return binarylist_from_genericmap_keys((GenericMap) other);
}

BinaryList binarylist_from_stringmap_values(StringMap other) {
    BinaryList list = binarylist_create_reserve(stringmap_size(other), stringmap_get_key_compare_fn(other));
    if (!list)
        return NULL;

    for (Iterator it = stringmap_begin(other); it; it = stringmap_next(other, it)) {
        char *value = stringmap_value_of(other, it);
        if (binarylist_append(list, value, strlen(value))) {
            binarylist_destroy(list);
            return NULL;
        }
    }

    return list;
}

BinaryList binarylist_create_filled(const char *item, size_t item_len, size_t size) {
    BinaryList list = binarylist_create_reserve(size, NULL);
    if (!list || binarylist_fill(list, item, item_len, size)) {
        binarylist_destroy(list);
        return NULL;
    }

    return list;
}

BinaryList binarylist_create_filled_binary(const Binary item, size_t size) {
    return binarylist_create_filled(item.data, item.length, size);
}

int binarylist_fill(BinaryList list, const char *item, size_t item_len, size_t size) {
    size_t fill_size = MIN(binarylist_size(list), size);

    int error = binarylist_resize(list, size, item, item_len);
    if (error)
        return error;

    for (size_t i = 0; i < fill_size; ++i) {
        char *duplicate = MALLOC(item_len+1);
        if (duplicate == NULL && item != NULL)
            return CC_ENOMEM;

        memcpy(duplicate, item, item_len);
        duplicate[item_len] = 0;

        FREE(list->array[i].data);
        list->array[i].data = duplicate;
        list->array[i].length = item_len;
    }

    return 0;
}

int binarylist_fill_binary(BinaryList list, const Binary item, size_t size) {
    return binarylist_fill(list, item.data, item.length, size);
}

int binarylist_resize(BinaryList list, size_t size, const char *empty_item, size_t empty_item_len) {
    size_t original_size = binarylist_size(list);

    if (size > original_size) {
        int error = binarylist_grow(list, size - original_size);
        if (error)
            return error;

        for (size_t i = original_size; i < size; ++i) {
            char *duplicate = MALLOC(empty_item_len+1);
            if (duplicate == NULL && empty_item != NULL) {
                list->array_size = i;
                goto cleanup;
            }

            memcpy(duplicate, empty_item, empty_item_len);
            duplicate[empty_item_len] = 0;

            list->array[i].data = duplicate;
            list->array[i].length = empty_item_len;
        }
    } else if (size < original_size) {
        for (size_t i = size; i < original_size; ++i) {
            FREE(list->array[i].data);
        }
    }

    list->array_size = size;
    list->array[list->array_size].data = NULL;
    list->array[list->array_size].length = 0;

    return 0;

cleanup:
    for (size_t i = original_size; i < list->array_size; ++i) {
        FREE(list->array[i].data);
    }

    list->array_size = original_size;
    list->array[list->array_size].data = NULL;
    list->array[list->array_size].length = 0;

    return CC_ENOMEM;
}

int binarylist_append_list(BinaryList list, BinaryList other) {
    int err = 0;
    /* Sizes are stored in variables because if `list` and `other` point to the same list, the sizes will change when items are added */
    size_t original_size = binarylist_size(list);
    size_t other_size = binarylist_size(other);

    for (size_t i = 0; i < other_size; ++i)
        if ((err = binarylist_append_binary(list, binarylist_array(other)[i])) != 0)
            goto cleanup;

    return 0;

cleanup:
    for (size_t i = original_size; i < binarylist_size(list); ++i) {
        FREE(list->array[i].data);
    }
    list->array_size = original_size;
    list->array[list->array_size].data = NULL;
    list->array[list->array_size].length = 0;
    return err;
}

int binarylist_append_move(BinaryList list, char *item, size_t item_len) {
    int error = binarylist_grow(list, 1);
    if (error)
        return error;

    /* Insert into list */
    list->array[list->array_size].data = item;
    list->array[list->array_size].length = item_len;
    ++list->array_size;
    list->array[list->array_size].data = NULL;
    list->array[list->array_size].length = 0;
    return 0;
}

int binarylist_append(BinaryList list, const char *item, size_t item_len) {
    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    /* Insert into list */
    int err = binarylist_append_move(list, duplicate, item_len);
    if (err)
        FREE(duplicate);
    else {
        /* Copy string */
        memcpy(duplicate, item, item_len);
        duplicate[item_len] = 0;
    }

    return err;
}

int binarylist_append_binary_move(BinaryList list, Binary item) {
    return binarylist_append_move(list, item.data, item.length);
}

int binarylist_append_binary(BinaryList list, const Binary item) {
    return binarylist_append(list, item.data, item.length);
}

int binarylist_insert_list(BinaryList list, BinaryList other, size_t before_index) {
    if (before_index >= binarylist_size(list))
        before_index = binarylist_size(list);

    const size_t original_size = binarylist_size(list);
    const size_t other_size = binarylist_size(other);
    size_t rollback_location = 0;
    int err = binarylist_grow(list, other_size);
    if (err)
        return err;

    /* One is added at the end of the move so the trailing NULL will be moved as well */
    memmove(list->array + before_index + other_size, list->array + before_index, sizeof(*list->array) * (original_size - before_index + 1));

    if (list == other) { /* Inserting list into itself */
        for (size_t i = 0; i < original_size; ++i) {
            size_t source_index = i;
            size_t destination_index = before_index + i;

            if (source_index >= before_index)
                source_index += original_size;

            const Binary other_item = list->array[source_index];
            void *duplicate = MALLOC(other_item.length+1);
            if (duplicate == NULL) {
                rollback_location = destination_index;
                goto cleanup;
            }

            list->array[destination_index].data = duplicate;
            list->array[destination_index].length = other_item.length;
        }
    } else {
        for (size_t i = before_index; i < before_index + other_size; ++i) {
            const Binary other_item = other->array[i-before_index];
            void *duplicate = MALLOC(other_item.length+1);
            if (duplicate == NULL) {
                rollback_location = i;
                goto cleanup;
            }

            list->array[i].data = duplicate;
            list->array[i].length = other_item.length;
        }
    }

    list->array_size += other_size;
    list->array[list->array_size].data = NULL;
    list->array[list->array_size].length = 0;

    return 0;

cleanup:
    for (size_t i = before_index; i < rollback_location; ++i) {
        FREE(list->array[i].data);
    }

    /* One is added at the end of the move so the trailing NULL will be moved back as well */
    memmove(list->array + before_index, list->array + before_index + other_size, sizeof(*list->array) * (original_size - before_index + 1));

    return CC_ENOMEM;
}

int binarylist_insert_move(BinaryList list, char *item, size_t item_len, size_t before_index) {
    if (before_index > binarylist_size(list))
        before_index = binarylist_size(list);

    int error = binarylist_grow(list, 1);
    if (error)
        return error;

    /* Make space in list */
    memmove(list->array + before_index + 1, list->array + before_index, (binarylist_size(list) - before_index) * sizeof(*list->array));

    /* Insert into list */
    list->array[before_index].data = item;
    list->array[before_index].length = item_len;
    list->array[list->array_size].data = NULL;
    list->array[list->array_size].length = 0;
    return 0;
}

int binarylist_insert(BinaryList list, const char *item, size_t item_len, size_t before_index) {
    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    int err = binarylist_insert_move(list, duplicate, item_len, before_index);
    if (err)
        FREE(duplicate);
    else {
        /* Copy binary */
        memcpy(duplicate, item, item_len);
        duplicate[item_len] = 0;
    }

    return err;
}

int binarylist_insert_binary_move(BinaryList list, Binary item, size_t before_index) {
    return binarylist_insert_move(list, item.data, item.length, before_index);
}

int binarylist_insert_binary(BinaryList list, const Binary item, size_t before_index) {
    return binarylist_insert(list, item.data, item.length, before_index);
}

int binarylist_replace_move_at(BinaryList list, size_t index, char *item, size_t item_len) {
    if (index >= list->array_size)
        return CC_EINVAL;

    FREE(list->array[index].data);
    list->array[index].data = item;
    list->array[index].length = item_len;
    return 0;
}

int binarylist_replace_at(BinaryList list, size_t index, const char *item, size_t item_len) {
    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    int err = binarylist_replace_move_at(list, index, duplicate, item_len);
    if (err)
        FREE(duplicate);
    else {
        /* Copy string */
        memcpy(duplicate, item, item_len);
        duplicate[item_len] = 0;
    }

    return err;
}

int binarylist_replace_binary_move_at(BinaryList list, size_t index, Binary item) {
    return binarylist_replace_move_at(list, index, item.data, item.length);
}

int binarylist_replace_binary_at(BinaryList list, size_t index, Binary item) {
    return binarylist_replace_at(list, index, item.data, item.length);
}

size_t binarylist_remove_at(BinaryList list, size_t index) {
    return binarylist_erase(list, index, index+1);
}

size_t binarylist_remove_one(BinaryList list, const char *item, size_t item_len) {
    size_t index = binarylist_find(list, item, item_len, 0);
    if (index == SIZE_MAX)
        return 0;

    return binarylist_remove_at(list, index);
}

size_t binarylist_remove_all(BinaryList list, const char *item, size_t item_len) {
    size_t count = 0;
    size_t index = 0;

    while (1) {
        index = binarylist_find(list, item, item_len, index);
        if (index == SIZE_MAX)
            return count;

        count += binarylist_remove_at(list, index);
    }
}

size_t binarylist_erase(BinaryList list, size_t begin_index, size_t end_index) {
    if (begin_index >= binarylist_size(list))
        return 0;

    if (end_index > binarylist_size(list))
        end_index = binarylist_size(list);

    size_t length = end_index - begin_index;
    for (size_t i = begin_index; i < end_index; ++i) {
        FREE(list->array[i].data);
    }

    memmove(list->array + begin_index, list->array + end_index, (binarylist_size(list) - end_index) * sizeof(*list->array));
    list->array_size -= length;
    list->array[list->array_size].data = NULL;
    list->array[list->array_size].length = 0;

    return length;
}

Binary *binarylist_array(BinaryList list) {
    return list->array;
}

int binarylist_contains(BinaryList list, const char *item, size_t item_len) {
    return binarylist_find(list, item, item_len, 0) != SIZE_MAX;
}

size_t binarylist_find(BinaryList list, const char *item, size_t item_len, size_t begin_index) {
    Binary b = {.data = (char *) item, .length = item_len};

    for (size_t i = begin_index; i < binarylist_size(list); ++i) {
        if (list->compare(&binarylist_array(list)[i], &b) == 0)
            return i;
    }

    return SIZE_MAX;
}

size_t binarylist_rfind(BinaryList list, const char *item, size_t item_len, size_t begin_index) {
    Binary b = {.data = (char *) item, .length = item_len};

    if (begin_index >= binarylist_size(list))
        begin_index = binarylist_size(list)-1;

    for (size_t i = begin_index; i != SIZE_MAX; --i) {
        if (list->compare(&binarylist_array(list)[i], &b) == 0)
            return i;
    }

    return SIZE_MAX;
}

int binarylist_compare(BinaryList list, BinaryList other) {
    size_t max = MIN(binarylist_size(list), binarylist_size(other));

    for (size_t i = 0; i < max; ++i) {
        int cmp = list->compare(&binarylist_array(list)[i], &binarylist_array(other)[i]);

        if (cmp)
            return cmp;
    }

    if (binarylist_size(other) != max)
        return -1;
    else if (binarylist_size(list) != max)
        return 1;

    return 0;
}

/* descending must be -1 (descending) or 1 (ascending) */
static void binarylist_insertion_sort(Binary *base, size_t num, int descending, BinaryCompare compar) {
    for (size_t i = 1; i < num; ++i) {
        for (size_t j = i; j > 0; --j) {
            if (compar(&base[j], &base[j-1]) * descending >= 0)
                break;

            Binary temp = base[j];
            base[j] = base[j-1];
            base[j-1] = temp;
        }
    }
}

static Binary *binarylist_heap_parent(Binary *base, Binary *start) {
    if (start == base)
        return NULL;

    return base + (start - base - 1) / 2;
}

static Binary *binarylist_heap_left_child(Binary *base, Binary *start, size_t num) {
    size_t location = start - base;
    if (location * 2 + 1 >= num)
        return NULL;

    return base + location * 2 + 1;
}

static Binary *binarylist_heap_right_child(Binary *base, Binary *start, size_t num) {
    size_t location = start - base;
    if (location * 2 + 2 >= num)
        return NULL;

    return base + location * 2 + 2;
}

static void binarylist_heapify_siftdown(Binary *base, Binary *start, size_t num, int descending, BinaryCompare compar) {
    Binary *root = start;

    for (Binary *left = binarylist_heap_left_child(base, root, num); left; left = binarylist_heap_left_child(base, root, num)) {
        Binary *swap = root;
        Binary *right = binarylist_heap_right_child(base, root, num);

        if (compar(swap, left) * descending < 0)
            swap = left;

        if (right && compar(swap, right) * descending < 0)
            swap = right;

        if (swap == root)
            return;

        Binary temp = *swap;
        *swap = *root;
        *root = temp;

        root = swap;
    }
}

static void binarylist_heapify_helper(Binary *base, size_t num, int descending, BinaryCompare compar) {
    if (num <= 1)
        return;

    Binary *start = binarylist_heap_parent(base, base + num - 1);

    while (1) {
        binarylist_heapify_siftdown(base, start, num, descending, compar);

        if (start == base)
            break;

        --start;
    }
}

static void binarylist_heap_sort_helper(Binary *base, size_t num, int descending, BinaryCompare compar) {
    binarylist_heapify_helper(base, num, descending, compar);

    size_t end = num - 1;
    while (end > 0) {
        Binary temp = base[end];
        base[end] = base[0];
        base[0] = temp;

        binarylist_heapify_siftdown(base, base, --end, descending, compar);
    }
}

/* descending must be -1 (descending) or 1 (ascending) */
static void binarylist_merge_helper(Binary *result, Binary *base, size_t begin, size_t pivot, size_t end, int descending, BinaryCompare compar) {
    size_t left = begin, right = pivot;

    for (size_t i = begin; i < end; ++i) {
        if (left < pivot && (right >= end || compar(&base[left], &base[right]) * descending <= 0))
            result[i] = base[left++];
        else
            result[i] = base[right++];
    }
}

static void binarylist_merge_sort_helper(Binary *result, Binary *base, size_t begin, size_t end, int descending, BinaryCompare compar) {
    if (end - begin <= 4) {
        binarylist_insertion_sort(base + begin, end - begin, descending, compar);
        return;
    }

    size_t pivot = begin + (end - begin) / 2;

    binarylist_merge_sort_helper(base, result, begin, pivot, descending, compar);
    binarylist_merge_sort_helper(base, result, pivot, end, descending, compar);
    binarylist_merge_helper(base, result, begin, pivot, end, descending, compar);
}

BinaryList binarylist_sorted(BinaryList list, int descending) {
    BinaryList copy = binarylist_copy(list);
    if (copy == NULL || binarylist_sort(copy, descending)) {
        binarylist_destroy(copy);
        return NULL;
    }

    return copy;
}

BinaryList binarylist_stable_sorted(BinaryList list, int descending) {
    BinaryList copy = binarylist_copy(list);
    if (copy == NULL || binarylist_stable_sort(copy, descending)) {
        binarylist_destroy(copy);
        return NULL;
    }

    return copy;
}

int binarylist_sort(BinaryList list, int descending) {
    if (list->compare == NULL)
        return CC_ENOTSUP;

    binarylist_heap_sort_helper(list->array, binarylist_size(list), descending? -1: 1, list->compare);
    return 0;
}

int binarylist_stable_sort(BinaryList list, int descending) {
    if (list->compare == NULL)
        return CC_ENOTSUP;

    Binary *temp = MALLOC(binarylist_size(list) * sizeof(*temp));
    if (temp == NULL)
        return CC_ENOMEM;

    memcpy(temp, list->array, binarylist_size(list) * sizeof(*temp));
    binarylist_merge_sort_helper(temp, list->array, 0, binarylist_size(list), descending? -1: 1, list->compare);

    FREE(temp);

    return 0;
}

void binarylist_reserve(BinaryList list, size_t size) {
    if (size <= binarylist_size(list))
        return;

    binarylist_grow(list, size - binarylist_size(list));
}

Iterator binarylist_begin(BinaryList list) {
    return list->array;
}

Iterator binarylist_next(BinaryList list, Iterator it) {
    if (it == NULL || binarylist_size(list) == 0 || it == list->array + binarylist_size(list) - 1)
        return NULL;

    return ((Binary *) it) + 1;
}

Binary binarylist_value_of(BinaryList list, Iterator it) {
    UNUSED(list)

    return *((Binary *) it);
}

size_t binarylist_size(BinaryList list) {
    return list->array_size;
}

BinaryCompare binarylist_get_compare_fn(BinaryList list) {
    return list->compare;
}

void binarylist_set_compare_fn(BinaryList list, BinaryCompare compare) {
    list->compare = compare? compare: binary_compare;
}

void binarylist_clear(BinaryList list) {
    for (size_t i = 0; i < binarylist_size(list); ++i)
        FREE(list->array[i].data);

    list->array_size = 0;
    list->array[0].data = NULL;
    list->array[0].length = 0;
}

void binarylist_destroy(BinaryList list) {
    if (list) {
        for (size_t i = 0; i < binarylist_size(list); ++i)
            FREE(list->array[i].data);

        FREE(list->array);
        FREE(list);
    }
}

CommonContainerBase *binarylist_get_container_base(BinaryList list) {
    return &list->base;
}
