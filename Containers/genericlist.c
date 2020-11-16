/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "genericlist.h"
#include "../utility.h"

/* For conversions */
#include "variant.h"
#include "genericmap.h"

struct GenericListStruct {
    CommonContainerBase base;
    void **array;
    Compare compare;
    Copier copy;
    Deleter deleter;
    size_t array_size;
    size_t array_capacity;
};

Variant variant_from_genericlist(GenericList list) {
    return variant_create_custom_base(list, (Compare) genericlist_compare, (Copier) genericlist_copy, (Deleter) genericlist_destroy, *genericlist_get_container_base(list));
}

int variant_is_genericlist(Variant var) {
    return variant_get_copier_fn(var) == (Copier) genericlist_copy;
}

int variant_is_variantlist(Variant var) {
    return variant_is_genericlist(var) && genericlist_get_copier_fn((GenericList) variant_get_custom(var)) == (Copier) variant_copy;
}

GenericList variant_get_genericlist(Variant var) {
    if (!variant_is_genericlist(var))
        return NULL;

    return variant_get_custom(var);
}

int variant_set_genericlist_move(Variant var, GenericList list) {
    return variant_set_custom_move_base(var, list, (Compare) genericlist_compare, (Copier) genericlist_copy, (Deleter) genericlist_destroy, *genericlist_get_container_base(list));
}

int variant_set_genericlist(Variant var, const GenericList list) {
    return variant_set_custom_base(var, list, (Compare) genericlist_compare, (Copier) genericlist_copy, (Deleter) genericlist_destroy, *genericlist_get_container_base(list));
}

GenericList genericlist_create(Compare compare, Copier copy, Deleter deleter) {
    return genericlist_create_reserve(0, compare, copy, deleter);
}

GenericList genericlist_create_reserve(size_t reserve, Compare compare, Copier copy, Deleter deleter) {
    const size_t minimum_size = 8;
    GenericList list = CALLOC(1, sizeof(*list));
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
        list->array[0] = NULL;
        list->compare = compare;
        list->copy = copy;
        list->deleter = deleter? deleter: (Deleter) FREE;

        return list;
    }
}

GenericList genericlist_copy(GenericList other) {
    void **array = genericlist_array(other);
    GenericList list = genericlist_create_reserve(other->array_size, other->compare, other->copy, other->deleter);
    if (!list)
        return NULL;

    for (size_t i = 0; i < genericlist_size(other); ++i)
        if (genericlist_append(list, array[i])) {
            genericlist_destroy(list);
            return NULL;
        }

    return list;
}

GenericList genericlist_concatenate(GenericList left, GenericList right) {
    GenericList result = genericlist_create_reserve(genericlist_size(left) + genericlist_size(right),
                                                    genericlist_get_compare_fn(left),
                                                    genericlist_get_copier_fn(left),
                                                    genericlist_get_deleter_fn(left));

    if (result == NULL)
        return NULL;

    void **array = genericlist_array(left);
    for (size_t i = 0; i < genericlist_size(left); ++i)
        if (genericlist_append(result, array[i])) {
            genericlist_destroy(result);
            return NULL;
        }

    array = genericlist_array(right);
    for (size_t i = 0; i < genericlist_size(right); ++i)
        if (genericlist_append(result, array[i])) {
            genericlist_destroy(result);
            return NULL;
        }

    return result;
}

GenericList genericlist_from_genericmap_values(GenericMap other, Compare compare) {
    GenericList list = genericlist_create_reserve(genericmap_size(other),
                                                  compare,
                                                  genericmap_get_copier_fn(other),
                                                  genericmap_get_deleter_fn(other));
    if (!list)
        return NULL;

    for (Iterator it = genericmap_begin(other); it; it = genericmap_next(other, it)) {
        if (genericlist_append(list, genericmap_value_of(other, it))) {
            genericlist_destroy(list);
            return NULL;
        }
    }

    return list;
}

GenericList genericlist_from_array(const void **items, Compare compare, Copier copy, Deleter deleter) {
    const void **ptr = items;
    size_t count = 0;

    for (; *ptr; ++ptr, ++count);

    return genericlist_from_array_n(items, count, compare, copy, deleter);
}

GenericList genericlist_from_array_n(const void **items, size_t count, Compare compare, Copier copy, Deleter deleter) {
    GenericList list = genericlist_create_reserve(count, compare, copy, deleter);
    if (!list)
        return NULL;

    for (size_t i = 0; i < count; ++i) {
        if (genericlist_append(list, items[i])) {
            genericlist_destroy(list);
            return NULL;
        }
    }

    return list;
}

GenericList genericlist_create_filled(const void *item, size_t size, Compare compare, Copier copy, Deleter deleter) {
    GenericList list = genericlist_create_reserve(size, compare, copy, deleter);
    if (!list || genericlist_fill(list, item, size)) {
        genericlist_destroy(list);
        return NULL;
    }

    return list;
}

GenericList genericlist_copy_slice(GenericList other, size_t begin_index, size_t length) {
    if (begin_index > genericlist_size(other))
        begin_index = genericlist_size(other);

    if (genericlist_size(other) - begin_index < length)
        length = genericlist_size(other) - begin_index;

    GenericList list = genericlist_create_reserve(length, genericlist_get_compare_fn(other), genericlist_get_copier_fn(other), genericlist_get_deleter_fn(other));
    if (list == NULL)
        return NULL;

    void **array = genericlist_array(other);
    for (size_t i = 0; i < length; ++i) {
        if (genericlist_append(list, array[begin_index+i])) {
            genericlist_destroy(list);
            return NULL;
        }
    }

    return list;
}

static int genericlist_grow(GenericList list, size_t added) {
    if (list->array_size+added >= list->array_capacity) {
        size_t new_capacity = MAX(list->array_capacity + (list->array_capacity / 2), list->array_size + added) + 1;
        size_t new_size = safe_multiply(new_capacity, sizeof(*list->array));
        if (!new_size || new_capacity < list->array_capacity)
            return CC_ENOMEM;

        void **new_array = REALLOC(list->array, new_size);
        if (!new_array)
            return CC_ENOMEM;

        list->array_capacity = new_capacity;
        list->array = new_array;
    }

    return 0;
}

int genericlist_fill(GenericList list, const void *item, size_t size) {
    if (list->copy == NULL)
        return CC_ENOTSUP;

    size_t fill_size = MIN(genericlist_size(list), size);

    int error = genericlist_resize(list, size, item);
    if (error)
        return error;

    for (size_t i = 0; i < fill_size; ++i) {
        void *duplicate = list->copy(item);
        if (duplicate == NULL && item != NULL)
            return CC_ENOMEM;

        list->deleter(list->array[i]);
        list->array[i] = duplicate;
    }

    return 0;
}

int genericlist_resize(GenericList list, size_t size, const void *empty_item) {
    size_t original_size = genericlist_size(list);

    if (size > original_size) {
        if (list->copy == NULL)
            return CC_ENOTSUP;

        int error = genericlist_grow(list, size - original_size);
        if (error)
            return error;

        for (size_t i = original_size; i < size; ++i) {
            void *duplicate = list->copy(empty_item);
            if (duplicate == NULL && empty_item != NULL) {
                list->array_size = i;
                goto cleanup;
            }

            list->array[i] = duplicate;
        }
    } else if (size < original_size) {
        for (size_t i = size; i < original_size; ++i) {
            list->deleter(list->array[i]);
        }
    }

    list->array_size = size;
    list->array[list->array_size] = NULL;

    return 0;

cleanup:
    for (size_t i = original_size; i < list->array_size; ++i) {
        list->deleter(list->array[i]);
    }

    list->array_size = original_size;
    list->array[list->array_size] = NULL;

    return CC_ENOMEM;
}

int genericlist_append_list(GenericList list, GenericList other) {
    int err = 0;
    /* Sizes are stored in variables because if `list` and `other` point to the same list, the sizes will change when items are added */
    size_t original_size = genericlist_size(list);
    size_t other_size = genericlist_size(other);

    for (size_t i = 0; i < other_size; ++i)
        if ((err = genericlist_append(list, genericlist_array(other)[i])) != 0)
            goto cleanup;

    return 0;

cleanup:
    for (size_t i = original_size; i < genericlist_size(list); ++i) {
        list->deleter(list->array[i]);
    }
    list->array_size = original_size;
    list->array[list->array_size] = NULL;
    return err;
}

int genericlist_append_move(GenericList list, void *item) {
    int error = genericlist_grow(list, 1);
    if (error)
        return error;

    list->array[list->array_size++] = item;
    list->array[list->array_size] = NULL;
    return 0;
}

int genericlist_append(GenericList list, const void *item) {
    if (list->copy == NULL)
        return CC_ENOTSUP;

    void *duplicate = list->copy(item);
    if (duplicate == NULL && item != NULL)
        return CC_ENOMEM;

    int err = genericlist_append_move(list, duplicate);
    if (err)
        list->deleter(duplicate);

    return err;
}

int genericlist_insert_list(GenericList list, GenericList other, size_t before_index) {
    if (list->copy == NULL)
        return CC_ENOTSUP;

    if (before_index >= genericlist_size(list))
        before_index = genericlist_size(list);

    const size_t original_size = genericlist_size(list);
    const size_t other_size = genericlist_size(other);
    size_t rollback_location = 0;
    int err = genericlist_grow(list, other_size);
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

            const void *other_item = list->array[source_index];
            void *duplicate = list->copy(other_item);
            if (duplicate == NULL && other_item != NULL) {
                rollback_location = destination_index;
                goto cleanup;
            }

            list->array[destination_index] = duplicate;
        }
    } else {
        for (size_t i = before_index; i < before_index + other_size; ++i) {
            const void *other_item = other->array[i-before_index];
            void *duplicate = list->copy(other_item);
            if (duplicate == NULL && other_item != NULL) {
                rollback_location = i;
                goto cleanup;
            }

            list->array[i] = duplicate;
        }
    }

    list->array_size += other_size;
    list->array[list->array_size] = NULL;

    return 0;

cleanup:
    for (size_t i = before_index; i < rollback_location; ++i) {
        list->deleter(list->array[i]);
    }

    /* One is added at the end of the move so the trailing NULL will be moved back as well */
    memmove(list->array + before_index, list->array + before_index + other_size, sizeof(*list->array) * (original_size - before_index + 1));

    return CC_ENOMEM;
}

int genericlist_insert_move(GenericList list, void *item, size_t before_index) {
    if (before_index >= genericlist_size(list))
        before_index = genericlist_size(list);

    int error = genericlist_grow(list, 1);
    if (error)
        return error;

    /* Make space in list */
    memmove(list->array + before_index + 1, list->array + before_index, (genericlist_size(list) - before_index) * sizeof(*list->array));

    /* Insert into list */
    list->array[before_index] = item;
    list->array[list->array_size] = NULL;
    return 0;
}

int genericlist_insert(GenericList list, const void *item, size_t before_index) {
    if (list->copy == NULL)
        return CC_ENOTSUP;

    void *duplicate = list->copy(item);
    if (duplicate == NULL && item != NULL)
        return CC_ENOMEM;

    int err = genericlist_insert_move(list, duplicate, before_index);
    if (err)
        list->deleter(duplicate);

    return err;
}

int genericlist_replace_move_at(GenericList list, size_t index, void *item) {
    if (index >= list->array_size)
        return CC_EINVAL;

    list->deleter(list->array[index]);
    list->array[index] = item;
    return 0;
}

int genericlist_replace_at(GenericList list, size_t index, const void *item) {
    if (list->copy == NULL)
        return CC_ENOTSUP;

    void *duplicate = list->copy(item);
    if (duplicate == NULL && item != NULL)
        return CC_ENOMEM;

    int err = genericlist_replace_move_at(list, index, duplicate);
    if (err)
        list->deleter(duplicate);

    return err;
}

size_t genericlist_remove_at(GenericList list, size_t index) {
    return genericlist_erase(list, index, index+1);
}

size_t genericlist_remove_one(GenericList list, const void *item) {
    size_t index = genericlist_find(list, item, 0);
    if (index == SIZE_MAX)
        return 0;

    return genericlist_remove_at(list, index);
}

size_t genericlist_remove_all(GenericList list, const void *item) {
    size_t count = 0;
    size_t index = 0;

    while (1) {
        index = genericlist_find(list, item, index);
        if (index == SIZE_MAX)
            return count;

        count += genericlist_remove_at(list, index);
    }
}

size_t genericlist_erase(GenericList list, size_t begin_index, size_t end_index) {
    if (begin_index >= genericlist_size(list))
        return 0;

    if (end_index > genericlist_size(list))
        end_index = genericlist_size(list);

    size_t length = end_index - begin_index;
    for (size_t i = begin_index; i < end_index; ++i) {
        list->deleter(list->array[i]);
    }

    memmove(list->array + begin_index, list->array + end_index, (genericlist_size(list) - end_index) * sizeof(*list->array));
    list->array_size -= length;
    list->array[list->array_size] = NULL;

    return length;
}

void **genericlist_array(GenericList list) {
    return list->array;
}

int genericlist_contains(GenericList list, const void *item) {
    return genericlist_find(list, item, 0) != SIZE_MAX;
}

size_t genericlist_find(GenericList list, const void *item, size_t begin_index) {
    if (list->compare == NULL)
        return SIZE_MAX;

    for (size_t i = begin_index; i < genericlist_size(list); ++i) {
        if (list->compare(genericlist_array(list)[i], item) == 0)
            return i;
    }

    return SIZE_MAX;
}

size_t genericlist_rfind(GenericList list, const void *item, size_t begin_index) {
    if (list->compare == NULL)
        return SIZE_MAX;

    if (begin_index >= genericlist_size(list))
        begin_index = genericlist_size(list)-1;

    for (size_t i = begin_index; i != SIZE_MAX; --i) {
        if (list->compare(genericlist_array(list)[i], item) == 0)
            return i;
    }

    return SIZE_MAX;
}

int genericlist_compare(GenericList list, GenericList other) {
    int cmp = generictypes_compatible_compare(list->compare, other->compare,
                                              list->copy, other->copy,
                                              list->deleter, other->deleter);
    if (cmp)
        return cmp;

    size_t max = MIN(genericlist_size(list), genericlist_size(other));

    if (list->compare != NULL) {
        for (size_t i = 0; i < max; ++i) {
            int cmp = list->compare(genericlist_array(list)[i], genericlist_array(other)[i]);

            if (cmp)
                return cmp;
        }
    }

    if (genericlist_size(other) != max)
        return -1;
    else if (genericlist_size(list) != max)
        return 1;

    return 0;
}

/* descending must be -1 (descending) or 1 (ascending) */
static void genericlist_insertion_sort(void **base, size_t num, int descending, Compare compar) {
    for (size_t i = 1; i < num; ++i) {
        for (size_t j = i; j > 0; --j) {
            if (compar(base[j], base[j-1]) * descending >= 0)
                break;

            void *temp = base[j];
            base[j] = base[j-1];
            base[j-1] = temp;
        }
    }
}

static void **genericlist_heap_parent(void **base, void **start) {
    if (start == base)
        return NULL;

    return base + (start - base - 1) / 2;
}

static void **genericlist_heap_left_child(void **base, void **start, size_t num) {
    size_t location = start - base;
    if (location * 2 + 1 >= num)
        return NULL;

    return base + location * 2 + 1;
}

static void **genericlist_heap_right_child(void **base, void **start, size_t num) {
    size_t location = start - base;
    if (location * 2 + 2 >= num)
        return NULL;

    return base + location * 2 + 2;
}

static void genericlist_heapify_siftdown(void **base, void **start, size_t num, int descending, Compare compar) {
    void **root = start;

    for (void **left = genericlist_heap_left_child(base, root, num); left; left = genericlist_heap_left_child(base, root, num)) {
        void **swap = root;
        void **right = genericlist_heap_right_child(base, root, num);

        if (compar(*swap, *left) * descending < 0)
            swap = left;

        if (right && compar(*swap, *right) * descending < 0)
            swap = right;

        if (swap == root)
            return;

        void *temp = *swap;
        *swap = *root;
        *root = temp;

        root = swap;
    }
}

static void genericlist_heapify_helper(void **base, size_t num, int descending, Compare compar) {
    if (num <= 1)
        return;

    void **start = genericlist_heap_parent(base, base + num - 1);

    while (1) {
        genericlist_heapify_siftdown(base, start, num, descending, compar);

        if (start == base)
            break;

        --start;
    }
}

static void genericlist_heap_sort_helper(void **base, size_t num, int descending, Compare compar) {
    genericlist_heapify_helper(base, num, descending, compar);

    size_t end = num - 1;
    while (end > 0) {
        void *temp = base[end];
        base[end] = base[0];
        base[0] = temp;

        genericlist_heapify_siftdown(base, base, --end, descending, compar);
    }
}

/* descending must be -1 (descending) or 1 (ascending) */
static void genericlist_merge_helper(void **result, void **base, size_t begin, size_t pivot, size_t end, int descending, Compare compar) {
    size_t left = begin, right = pivot;

    for (size_t i = begin; i < end; ++i) {
        if (left < pivot && (right >= end || compar(base[left], base[right]) * descending <= 0))
            result[i] = base[left++];
        else
            result[i] = base[right++];
    }
}

static void genericlist_merge_sort_helper(void **result, void **base, size_t begin, size_t end, int descending, Compare compar) {
    if (end - begin <= 4) {
        genericlist_insertion_sort(base + begin, end - begin, descending, compar);
        return;
    }

    size_t pivot = begin + (end - begin) / 2;

    genericlist_merge_sort_helper(base, result, begin, pivot, descending, compar);
    genericlist_merge_sort_helper(base, result, pivot, end, descending, compar);
    genericlist_merge_helper(base, result, begin, pivot, end, descending, compar);
}

GenericList genericlist_sorted(GenericList list, int descending) {
    GenericList copy = genericlist_copy(list);
    if (copy == NULL || genericlist_sort(copy, descending)) {
        genericlist_destroy(copy);
        return NULL;
    }

    return copy;
}

GenericList genericlist_stable_sorted(GenericList list, int descending) {
    GenericList copy = genericlist_copy(list);
    if (copy == NULL || genericlist_stable_sort(copy, descending)) {
        genericlist_destroy(copy);
        return NULL;
    }

    return copy;
}

int genericlist_sort(GenericList list, int descending) {
    if (list->compare == NULL)
        return CC_ENOTSUP;

    genericlist_heap_sort_helper(list->array, genericlist_size(list), descending? -1: 1, list->compare);
    return 0;
}

int genericlist_stable_sort(GenericList list, int descending) {
    if (list->compare == NULL)
        return CC_ENOTSUP;

    void **temp = MALLOC(genericlist_size(list) * sizeof(*temp));
    if (temp == NULL)
        return CC_ENOMEM;

    memcpy(temp, list->array, genericlist_size(list) * sizeof(*temp));
    genericlist_merge_sort_helper(temp, list->array, 0, genericlist_size(list), descending? -1: 1, list->compare);

    FREE(temp);

    return 0;
}

void genericlist_reserve(GenericList list, size_t size) {
    if (size <= genericlist_size(list))
        return;

    genericlist_grow(list, size - genericlist_size(list));
}

Iterator genericlist_begin(GenericList list) {
    return list->array;
}

Iterator genericlist_next(GenericList list, Iterator it) {
    if (it == NULL || genericlist_size(list) == 0 || it == list->array + genericlist_size(list) - 1)
        return NULL;

    return ((void **) it) + 1;
}

void *genericlist_value_of(GenericList list, Iterator it) {
    UNUSED(list)

    return *((void **) it);
}

size_t genericlist_size(GenericList list) {
    return list->array_size;
}

Compare genericlist_get_compare_fn(GenericList list) {
    return list->compare;
}

void genericlist_set_compare_fn(GenericList list, Compare compare) {
    list->compare = compare;
}

Copier genericlist_get_copier_fn(GenericList list) {
    return list->copy;
}

void genericlist_set_copier_fn(GenericList list, Copier copier) {
    list->copy = copier;
}

Deleter genericlist_get_deleter_fn(GenericList list) {
    return list->deleter;
}

void genericlist_set_deleter_fn(GenericList list, Deleter deleter) {
    if (deleter != NULL)
        list->deleter = deleter;
}

void genericlist_clear(GenericList list) {
    for (size_t i = 0; i < genericlist_size(list); ++i)
        list->deleter(list->array[i]);

    list->array_size = 0;
    list->array[0] = NULL;
}

void genericlist_destroy(GenericList list) {
    if (list) {
        for (size_t i = 0; i < genericlist_size(list); ++i)
            list->deleter(list->array[i]);

        FREE(list->array);
        FREE(list);
    }
}

CommonContainerBase *genericlist_get_container_base(GenericList list) {
    return &list->base;
}
