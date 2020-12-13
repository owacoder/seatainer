/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "genericlist.h"
#include "../utility.h"
#include "recipes.h"

/* For conversions */
#include "variant.h"
#include "genericmap.h"

#define MERGE_SORT_INSERTION_SORT_CUTOFF 8

struct GenericListStruct {
    CommonContainerBase *base;
    size_t array_size; /* Number of elements in array (not bytes) */
    size_t array_capacity; /* Capacity in elements of array (not bytes) */
    void *array;
};

static inline size_t genericlist_stored_element_size(const CommonContainerBase *base) {
    return base->size? MIN(base->size, sizeof(void*)): sizeof(void*);
}

static int genericlist_grow(GenericList list, size_t added) {
    if (list->array_size+added >= list->array_capacity) {
        size_t new_capacity = MAX(list->array_capacity + (list->array_capacity / 2), list->array_size + added) + 1;
        size_t new_size = safe_multiply(new_capacity, genericlist_stored_element_size(list->base));
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

Variant variant_from_genericlist(GenericList list) {
    return variant_create_custom_adopt(list, genericlist_build_recipe(list));
}

int variant_is_genericlist(Variant var) {
    return generic_types_compatible_compare(variant_get_custom_container_base(var), container_base_genericlist_recipe()) == 0;
}

int variant_is_variantlist(Variant var) {
    return generic_types_compatible_compare(variant_get_custom_container_base(var), container_base_variantlist_recipe()) == 0;
}

GenericList variant_get_genericlist(Variant var) {
    if (!variant_is_genericlist(var))
        return NULL;

    return variant_get_custom(var);
}

int variant_set_genericlist_move(Variant var, GenericList list) {
    return variant_set_custom_move(var, list, genericlist_get_container_base(list));
}

int variant_set_genericlist(Variant var, const GenericList list) {
    return variant_set_custom(var, list, genericlist_get_container_base(list));
}

GenericList genericlist_create(const CommonContainerBase *base) {
    return genericlist_create_reserve(0, base);
}

GenericList genericlist_create_reserve(size_t reserve, const CommonContainerBase *base) {
    if (base == NULL)
        return NULL;

    GenericList list = CALLOC(1, sizeof(*list));
    CommonContainerBase *new_base = container_base_copy_if_dynamic(base);
    if (!list || !new_base)
        goto cleanup;

    const size_t element_size = genericlist_stored_element_size(base);
    const size_t minimum_size = 8;

    list->array_capacity = MAX(minimum_size, reserve);
    while (1) {
        const size_t malloc_size = safe_multiply(list->array_capacity, element_size);
        if (!malloc_size)
            goto cleanup;

        list->array = MALLOC(malloc_size);
        if (list->array == NULL) {
            if (list->array_capacity > minimum_size) {
                list->array_capacity = minimum_size; /* Fall back to minimum size if trying to allocate too much */
                continue;
            }

            goto cleanup;
        }

        memset(list->array, 0, element_size);
        list->base = new_base;

        return list;
    }

cleanup:
    FREE(list);
    container_base_destroy_if_dynamic(new_base);
    return NULL;
}

GenericList genericlist_copy(GenericList other) {
    GenericList list = genericlist_create_reserve(other->array_size, other->base);
    if (!list)
        return NULL;

    for (size_t i = 0; i < genericlist_size(other); ++i)
        if (genericlist_append(list, genericlist_value_at(other, i))) {
            genericlist_destroy(list);
            return NULL;
        }

    return list;
}

GenericList genericlist_concatenate(GenericList left, GenericList right) {
    if (generic_types_compatible_compare(genericlist_get_container_base(left), genericlist_get_container_base(right)) != 0)
        return NULL;

    GenericList result = genericlist_create_reserve(genericlist_size(left) + genericlist_size(right), genericlist_get_container_base(left));

    if (result == NULL)
        return NULL;

    for (size_t i = 0; i < genericlist_size(left); ++i)
        if (genericlist_append(result, genericlist_value_at(left, i))) {
            genericlist_destroy(result);
            return NULL;
        }

    for (size_t i = 0; i < genericlist_size(right); ++i)
        if (genericlist_append(result, genericlist_value_at(right, i))) {
            genericlist_destroy(result);
            return NULL;
        }

    return result;
}

GenericList genericlist_from_genericmap_values(GenericMap other) {
    GenericList list = genericlist_create_reserve(genericmap_size(other), genericmap_get_value_container_base(other));
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

GenericList genericlist_from_array(const void **items, const CommonContainerBase *base) {
    const void **ptr = items;
    size_t count = 0;

    for (; *ptr; ++ptr, ++count);

    return genericlist_from_array_n(items, count, base);
}

GenericList genericlist_from_array_n(const void **items, size_t count, const CommonContainerBase *base) {
    GenericList list = genericlist_create_reserve(count, base);
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

GenericList genericlist_create_filled(const void *item, size_t size, const CommonContainerBase *base) {
    GenericList list = genericlist_create_reserve(size, base);
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

    GenericList list = genericlist_create_reserve(length, genericlist_get_container_base(other));
    if (list == NULL)
        return NULL;

    if (list->base->size && list->base->size <= sizeof(void*)) {
        if (genericlist_grow(list, length)) {
            genericlist_destroy(list);
            return NULL;
        }

        const size_t element_size = list->base->size;

        memcpy(list->array, other->array, length * element_size);
        memset((char *) list->array + length * element_size, 0, element_size);
        list->array_size = length;
    } else {
        for (size_t i = 0; i < length; ++i) {
            if (genericlist_append(list, genericlist_value_at(other, begin_index+i))) {
                genericlist_destroy(list);
                return NULL;
            }
        }
    }

    return list;
}

int genericlist_fill(GenericList list, const void *item, size_t size) {
    size_t fill_size = MIN(genericlist_size(list), size);

    int error = genericlist_resize(list, size, item);
    if (error)
        return error;

    if (list->base->size > sizeof(void*)) { /* Large POD */
        const size_t element_size = list->base->size;
        void **array = (void **) list->array;

        for (size_t i = 0; i < fill_size; ++i) {
            void *duplicate = generic_pod_copy_alloc(item, element_size);
            if (duplicate == NULL && item != NULL)
                return CC_ENOMEM;

            FREE(array[i]);

            array[i] = duplicate;
        }
    } else if (list->base->size) { /* Small POD type */
        const size_t element_size = list->base->size;

        if (element_size == 1) {
            memset(list->array, *((unsigned char *) item), size);
        } else {
            for (size_t i = 0; i < fill_size; ++i) {
                memcpy((char*) list->array + (i * element_size), item, element_size);
            }
        }
    } else { /* Non-POD type */
        if (list->base->copier == NULL)
            return CC_ENOTSUP;

        void **array = (void **) list->array;

        for (size_t i = 0; i < fill_size; ++i) {
            void *duplicate = list->base->copier(item);
            if (duplicate == NULL && item != NULL)
                return CC_ENOMEM;

            if (list->base->deleter)
                list->base->deleter(array[i]);

            array[i] = duplicate;
        }
    }

    return 0;
}

int genericlist_resize(GenericList list, size_t size, const void *empty_item) {
    size_t original_size = genericlist_size(list);

    int error = genericlist_grow(list, size - original_size);
    if (error)
        return error;

    if (size > original_size) {
        if (list->base->size > sizeof(void*)) { /* Large POD */
            const size_t element_size = list->base->size;
            void **array = (void **) list->array;

            for (size_t i = original_size; i < size; ++i) {
                void *duplicate = generic_pod_copy_alloc(empty_item, element_size);
                if (duplicate == NULL && empty_item != NULL) {
                    list->array_size = i;
                    goto cleanup;
                }

                array[i] = duplicate;
            }

            array[size] = NULL;
        } else if (list->base->size) { /* Small POD type */
            const size_t element_size = list->base->size;

            if (element_size == 1) {
                memset((char *) list->array + original_size, *((unsigned char *) empty_item), element_size);
            } else {
                for (size_t i = original_size; i < size; ++i) {
                    memcpy((char *) list->array + (i * element_size), empty_item, element_size);
                }
            }

            memset((char *) list->array + (size * element_size), 0, element_size);
        } else {
            if (list->base->copier == NULL)
                return CC_ENOTSUP;

            void **array = (void **) list->array;

            for (size_t i = original_size; i < size; ++i) {
                void *duplicate = list->base->copier(empty_item);
                if (duplicate == NULL && empty_item != NULL) {
                    list->array_size = i;
                    goto cleanup;
                }

                array[i] = duplicate;
            }

            array[size] = NULL;
        }
    } else if (size < original_size) {
        if (list->base->size && list->base->size <= sizeof(void*)) { /* Small POD type */
            const size_t element_size = list->base->size;

            memset((char *) list->array + (size * element_size), 0, element_size);
        } else { /* Large POD or non-POD type */
            void **array = (void **) list->array;
            Deleter d = list->base->size?
                                          FREE: /* Large POD */
                                          list->base->deleter; /* Non-POD */

            if (d) {
                for (size_t i = size; i < original_size; ++i) {
                    d(array[i]);
                }
            }

            array[size] = NULL;
        }
    }

    list->array_size = size;

    return 0;

cleanup:
    genericlist_erase(list, original_size, list->array_size);

    return CC_ENOMEM;
}

int genericlist_append_list(GenericList list, GenericList other) {
    int err = 0;
    /* Sizes are stored in variables because if `list` and `other` point to the same list, the sizes will change when items are added */
    size_t original_size = genericlist_size(list);
    size_t other_size = genericlist_size(other);

    for (size_t i = 0; i < other_size; ++i)
        if ((err = genericlist_append(list, genericlist_value_at(other, i))) != 0)
            goto cleanup;

    return 0;

cleanup:
    genericlist_erase(list, original_size, list->array_size);

    return err;
}

int genericlist_append_move(GenericList list, void *item) {
    return genericlist_insert_move(list, item, list->array_size);
}

int genericlist_append(GenericList list, const void *item) {
    return genericlist_insert(list, item, list->array_size);
}

int genericlist_insert_list(GenericList list, GenericList other, size_t before_index) {
    if (generic_types_compatible_compare(genericlist_get_container_base(list), genericlist_get_container_base(other)) != 0)
        return CC_EINVAL;

    if (before_index >= genericlist_size(list))
        before_index = genericlist_size(list);

    const size_t original_size = genericlist_size(list);
    const size_t other_size = genericlist_size(other);
    int err = genericlist_grow(list, other_size);
    if (err)
        return err;

    if (list->base->size && list->base->size <= sizeof(void*)) {
        const size_t element_size = list->base->size;

        /* One is added at the end of the move so the trailing 0-element will be moved as well */
        memmove((char *) list->array + ((before_index + other_size) * element_size),
                (char *) list->array + ((before_index             ) * element_size),
                (original_size - before_index + 1) * element_size);

        if (list == other) { /* Inserting list into itself */
            memcpy((char *) list->array + (before_index * element_size),
                   list->array,
                   before_index * element_size);

            memcpy((char *) list->array + (before_index*2 * element_size),
                   (char *) list->array + ((before_index + original_size) * element_size),
                   (original_size - before_index) * element_size);
        } else {
            memcpy((char *) list->array + (before_index * element_size), other->array, other_size * element_size);
        }

        list->array_size += other_size;
    } else {
        size_t rollback_location = 0;
        void **array = (void **) list->array;
        void **other_array = (void **) other->array;

        if (list->base->copier == NULL)
            return CC_ENOTSUP;

        /* One is added at the end of the move so the trailing NULL will be moved as well */
        memmove(array + before_index + other_size, array + before_index, sizeof(*array) * (original_size - before_index + 1));

        if (list == other) { /* Inserting list into itself */
            for (size_t i = 0; i < original_size; ++i) {
                size_t source_index = i;
                size_t destination_index = before_index + i;

                if (source_index >= before_index)
                    source_index += original_size;

                const void *other_item = array[source_index];
                void *duplicate = list->base->copier(other_item);
                if (duplicate == NULL && other_item != NULL) {
                    rollback_location = destination_index;
                    goto non_pod_cleanup;
                }

                array[destination_index] = duplicate;
            }
        } else {
            for (size_t i = before_index; i < before_index + other_size; ++i) {
                const void *other_item = other_array[i-before_index];
                void *duplicate = list->base->copier(other_item);
                if (duplicate == NULL && other_item != NULL) {
                    rollback_location = i;
                    goto non_pod_cleanup;
                }

                array[i] = duplicate;
            }
        }

        list->array_size += other_size;

non_pod_cleanup: ;
        Deleter d = list->base->size? FREE: list->base->deleter;

        if (d) {
            for (size_t i = before_index; i < rollback_location; ++i) {
                d(array[i]);
            }
        }

        /* One is added at the end of the move so the trailing NULL will be moved back as well */
        memmove(array + before_index, array + before_index + other_size, sizeof(*array) * (original_size - before_index + 1));

        return CC_ENOMEM;
    }

    return 0;
}

int genericlist_insert_move(GenericList list, void *item, size_t before_index) {
    if (list->base->size && list->base->size <= sizeof(void*)) { /* Small POD type */
        int err = genericlist_insert(list, item, before_index);
        if (err)
            return err;

        FREE(item);

        return 0;
    } else { /* Non-POD type */
        if (before_index >= genericlist_size(list))
            before_index = genericlist_size(list);

        int error = genericlist_grow(list, 1);
        if (error)
            return error;

        void **array = (void **) list->array;

        /* Make space in list */
        memmove(array + before_index + 1, array + before_index, (genericlist_size(list) - before_index) * sizeof(*array));

        array[before_index] = item;
        array[++list->array_size] = NULL;
    }

    return 0;
}

int genericlist_insert(GenericList list, const void *item, size_t before_index) {
    if (list->base->size > sizeof(void*)) { /* Large POD type */
        const size_t element_size = list->base->size;

        void *duplicate = generic_pod_copy_alloc(item, element_size);
        if (duplicate == NULL && item != NULL)
            return CC_ENOMEM;

        int err = genericlist_insert_move(list, duplicate, before_index);
        if (err)
            FREE(duplicate);

        return err;
    } else if (list->base->size) { /* Small POD type */
        unsigned char temp_buffer[sizeof(void*)]; /* Allows us to save the data of the specified item,
                                                     in case it's a reference to an element in this list
                                                     that will possibly get invalidated if we grow it. */
        const size_t element_size = list->base->size;

        if (before_index >= genericlist_size(list))
            before_index = genericlist_size(list);

        memcpy(temp_buffer, item, element_size);

        int error = genericlist_grow(list, 1);
        if (error)
            return error;

        /* Make space in list */
        memmove((char *) list->array + ((before_index+1) * element_size),
                (char *) list->array + ((before_index+0) * element_size),
                (genericlist_size(list) - before_index)  * element_size);

        /* Insert into list */
        memcpy((char *) list->array + (before_index * element_size), temp_buffer, element_size);
        ++list->array_size;

        /* Terminate list */
        memset((char *) list->array + (list->array_size * element_size), 0, element_size);

        return 0;
    } else {
        if (list->base->copier == NULL)
            return CC_ENOTSUP;

        void *duplicate = list->base->copier(item);
        if (duplicate == NULL && item != NULL)
            return CC_ENOMEM;

        int err = genericlist_insert_move(list, duplicate, before_index);
        if (err && list->base->deleter)
            list->base->deleter(duplicate);

        return err;
    }
}

int genericlist_replace_move_at(GenericList list, size_t index, void *item) {
    if (list->base->size > sizeof(void*)) { /* Large POD type */
        void **array = (void **) list->array;

        FREE(array[index]);

        array[index] = item;
    } else if (list->base->size) { /* Small POD type */
        const size_t element_size = list->base->size;

        memcpy((char *) list->array + (index * element_size), item, element_size);

        FREE(item);
    } else { /* Non-POD type */
        void **array = (void **) list->array;

        if (list->base->deleter)
            list->base->deleter(array[index]);

        array[index] = item;
    }

    return 0;
}

int genericlist_replace_at(GenericList list, size_t index, const void *item) {
    if (list->base->size > sizeof(void*)) { /* Large POD type */
        const size_t element_size = list->base->size;
        void **array = (void **) list->array;

        memmove(array[index], item, element_size);
    } else if (list->base->size) { /* Small POD type */
        const size_t element_size = list->base->size;

        memmove((char *) list->array + (index * element_size), item, element_size);
    } else { /* Non-POD type */
        if (list->base->copier == NULL)
            return CC_ENOTSUP;

        void *duplicate = list->base->copier(item);
        if (duplicate == NULL && item != NULL)
            return CC_ENOMEM;

        int err = genericlist_replace_move_at(list, index, duplicate);
        if (err && list->base->deleter)
            list->base->deleter(duplicate);

        return err;
    }

    return 0;
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
    if (begin_index >= MIN(genericlist_size(list), end_index))
        return 0;

    if (end_index > genericlist_size(list))
        end_index = genericlist_size(list);

    const size_t element_size = genericlist_stored_element_size(list->base);
    Deleter deleter = list->base->size? list->base->size <= sizeof(void*)? NULL: FREE: list->base->deleter;

    size_t length = end_index - begin_index;
    if (deleter) {
        void **array = (void **) list->array;

        for (size_t i = begin_index; i < end_index; ++i) {
            deleter(array[i]);
        }
    }

    memmove((char *) list->array + (begin_index  * element_size),
            (char *) list->array + (end_index    * element_size),
            (genericlist_size(list) - end_index) * element_size);

    list->array_size -= length;
    memset((char *) list->array + (list->array_size * element_size), 0, element_size);

    return length;
}

void *genericlist_data(GenericList list) {
    return list->array;
}

int genericlist_contains(GenericList list, const void *item) {
    return genericlist_find(list, item, 0) != SIZE_MAX;
}

size_t genericlist_bsearch(GenericList list, const void *item) {
    if (list->base->compare == NULL || genericlist_size(list) == 0)
        return SIZE_MAX;

    const size_t highest = genericlist_size(list) - 1;
    size_t hi = highest;
    size_t lo = 0;

    if (list->base->size && list->base->size <= sizeof(void*)) {
        const size_t element_size = list->base->size;

        while (lo <= hi) {
            const size_t mid = lo + ((hi - lo) / 2);

            int cmp = list->base->compare(item, (char *) list->array + (mid * element_size));
            if (cmp == 0)
                return mid;

            if (cmp < 0) {
                if (mid == 0)
                    break;

                hi = mid-1;
            } else {
                if (mid == highest)
                    break;

                lo = mid+1;
            }
        }
    } else {
        void **array = (void **) list->array;

        while (lo <= hi) {
            const size_t mid = lo + ((hi - lo) / 2);

            int cmp = list->base->compare(item, array[mid]);
            if (cmp == 0)
                return mid;

            if (cmp < 0) { /* Less than the compared element */
                if (mid == 0)
                    break;

                hi = mid-1;
            } else { /* Greater than the compared element */
                if (mid == highest)
                    break;

                lo = mid+1;
            }
        }
    }

    return SIZE_MAX;
}

size_t genericlist_find(GenericList list, const void *item, size_t begin_index) {
    if (list->base->compare == NULL)
        return SIZE_MAX;

    for (size_t i = begin_index; i < genericlist_size(list); ++i) {
        if (list->base->compare(item, genericlist_value_at(list, i)) == 0)
            return i;
    }

    return SIZE_MAX;
}

size_t genericlist_rfind(GenericList list, const void *item, size_t begin_index) {
    if (list->base->compare == NULL)
        return SIZE_MAX;

    if (begin_index >= genericlist_size(list))
        begin_index = genericlist_size(list)-1;

    for (size_t i = begin_index; i != SIZE_MAX; --i) {
        if (list->base->compare(item, genericlist_value_at(list, i)) == 0)
            return i;
    }

    return SIZE_MAX;
}

int genericlist_compare(GenericList list, GenericList other) {
    int cmp = generic_types_compatible_compare(genericlist_get_container_base(list), genericlist_get_container_base(other));
    if (cmp)
        return cmp;

    size_t max = MIN(genericlist_size(list), genericlist_size(other));

    if (list->base->compare != NULL) {
        for (size_t i = 0; i < max; ++i) {
            int cmp = list->base->compare(genericlist_value_at(list, i), genericlist_value_at(other, i));

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
static void genericlist_pod_insertion_sort(void *base, size_t num, size_t element_size, int descending, Compare compar) {
    for (size_t i = 1; i < num; ++i) {
        for (size_t j = i; j > 0; --j) {
            char * left = (char *) base + (j  )*element_size;
            char *right = (char *) base + (j-1)*element_size;

            if (compar(left, right) * descending >= 0)
                break;

            memswap(left, right, element_size);
        }
    }
}

/* descending must be -1 (descending) or 1 (ascending) */
static void genericlist_ptr_insertion_sort(void **base, size_t num, int descending, Compare compar) {
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

static void *genericlist_pod_heap_parent(void *base, void *start, size_t element_size) {
    size_t location = ((char *) start - (char *) base) / element_size;
    if (start == base)
        return NULL;

    return (char *) base + ((location - 1) / 2) * element_size;
}

static void *genericlist_pod_heap_left_child(void *base, void *start, size_t num, size_t element_size) {
    size_t location = ((char *) start - (char *) base) / element_size;
    if (location * 2 + 1 >= num)
        return NULL;

    return (char *) base + (location * 2 + 1) * element_size;
}

static void *genericlist_pod_heap_right_child(void *base, void *start, size_t num, size_t element_size) {
    size_t location = ((char *) start - (char *) base) / element_size;
    if (location * 2 + 2 >= num)
        return NULL;

    return (char *) base + (location * 2 + 2) * element_size;
}

static void genericlist_pod_heapify_siftdown(void *base, void *start, size_t num, size_t element_size, int descending, Compare compar) {
    void *root = start;

    for (void *left = genericlist_pod_heap_left_child(base, root, num, element_size);
         left;
         left = genericlist_pod_heap_left_child(base, root, num, element_size)) {
        void *swap = root;
        void *right = genericlist_pod_heap_right_child(base, root, num, element_size);

        if (compar(swap, left) * descending < 0)
            swap = left;

        if (right && compar(swap, right) * descending < 0)
            swap = right;

        if (swap == root)
            return;

        memswap(swap, root, element_size);

        root = swap;
    }
}

static void genericlist_pod_heapify_helper(void **base, size_t num, size_t element_size, int descending, Compare compar) {
    if (num <= 1)
        return;

    void *start = genericlist_pod_heap_parent(base, base + num - 1, element_size);

    while (start) {
        genericlist_pod_heapify_siftdown(base, start, num, element_size, descending, compar);

        if (start == base)
            break;

        start = (char *) start - element_size;
    }
}

static void genericlist_pod_heap_sort(void *base, size_t num, size_t element_size, int descending, Compare compar) {
    genericlist_pod_heapify_helper(base, num, element_size, descending, compar);

    size_t end = num - 1;
    while (end > 0) {
        memswap((char *) base,
                (char *) base + end * element_size,
                element_size);

        genericlist_pod_heapify_siftdown(base, base, --end, element_size, descending, compar);
    }
}

static void **genericlist_ptr_heap_parent(void **base, void **start) {
    if (start == base)
        return NULL;

    return base + (start - base - 1) / 2;
}

static void **genericlist_ptr_heap_left_child(void **base, void **start, size_t num) {
    size_t location = start - base;
    if (location * 2 + 1 >= num)
        return NULL;

    return base + location * 2 + 1;
}

static void **genericlist_ptr_heap_right_child(void **base, void **start, size_t num) {
    size_t location = start - base;
    if (location * 2 + 2 >= num)
        return NULL;

    return base + location * 2 + 2;
}

static void genericlist_ptr_heapify_siftdown(void **base, void **start, size_t num, int descending, Compare compar) {
    void **root = start;

    for (void **left = genericlist_ptr_heap_left_child(base, root, num); left; left = genericlist_ptr_heap_left_child(base, root, num)) {
        void **swap = root;
        void **right = genericlist_ptr_heap_right_child(base, root, num);

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

static void genericlist_ptr_heapify_helper(void **base, size_t num, int descending, Compare compar) {
    if (num <= 1)
        return;

    void **start = genericlist_ptr_heap_parent(base, base + num - 1);

    while (start) {
        genericlist_ptr_heapify_siftdown(base, start, num, descending, compar);

        if (start == base)
            break;

        --start;
    }
}

static void genericlist_ptr_heap_sort(void **base, size_t num, int descending, Compare compar) {
    genericlist_ptr_heapify_helper(base, num, descending, compar);

    size_t end = num - 1;
    while (end > 0) {
        void *temp = base[end];
        base[end] = base[0];
        base[0] = temp;

        genericlist_ptr_heapify_siftdown(base, base, --end, descending, compar);
    }
}

/* descending must be -1 (descending) or 1 (ascending) */
static void genericlist_pod_merge_helper(void *result, void *base, size_t begin, size_t pivot, size_t end, size_t element_size, int descending, Compare compar) {
    size_t left = begin, right = pivot;

    for (size_t i = begin; i < end; ++i) {
        char * left_element = (char *) base +  left * element_size;
        char *right_element = (char *) base + right * element_size;

        if (left < pivot && (right >= end || compar(left_element, right_element) * descending <= 0)) {
            memcpy((char *) result + i * element_size, left_element, element_size);
            ++left;
        } else {
            memcpy((char *) result + i * element_size, right_element, element_size);
            ++right;
        }
    }
}

static void genericlist_pod_merge_sort(void *result, void *base, size_t begin, size_t end, size_t element_size, int descending, Compare compar) {
    if (end - begin <= MERGE_SORT_INSERTION_SORT_CUTOFF) {
        genericlist_pod_insertion_sort((char *) base + (begin * element_size), end - begin, element_size, descending, compar);
        return;
    }

    const size_t pivot = begin + (end - begin) / 2;

    genericlist_pod_merge_sort(base, result, begin, pivot, element_size, descending, compar);
    genericlist_pod_merge_sort(base, result, pivot, end, element_size, descending, compar);
    genericlist_pod_merge_helper(base, result, begin, pivot, end, element_size, descending, compar);
}

/* descending must be -1 (descending) or 1 (ascending) */
static void genericlist_ptr_merge_helper(void **result, void **base, size_t begin, size_t pivot, size_t end, int descending, Compare compar) {
    size_t left = begin, right = pivot;

    for (size_t i = begin; i < end; ++i) {
        if (left < pivot && (right >= end || compar(base[left], base[right]) * descending <= 0))
            result[i] = base[left++];
        else
            result[i] = base[right++];
    }
}

static void genericlist_ptr_merge_sort(void **result, void **base, size_t begin, size_t end, int descending, Compare compar) {
    if (end - begin <= MERGE_SORT_INSERTION_SORT_CUTOFF) {
        genericlist_ptr_insertion_sort(base + begin, end - begin, descending, compar);
        return;
    }

    const size_t pivot = begin + (end - begin) / 2;

    genericlist_ptr_merge_sort(base, result, begin, pivot, descending, compar);
    genericlist_ptr_merge_sort(base, result, pivot, end, descending, compar);
    genericlist_ptr_merge_helper(base, result, begin, pivot, end, descending, compar);
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
    if (list->base->compare == NULL)
        return CC_ENOTSUP;

    if (list->base->size && list->base->size <= sizeof(void*))
        /* Could (technically) also use builtin qsort here, but it would require possibly worse than O(n log n) complexity and wouldn't allow compare reversal */
        genericlist_pod_heap_sort(list->array, genericlist_size(list), list->base->size, descending? -1: 1, list->base->compare);
    else
        genericlist_ptr_heap_sort(list->array, genericlist_size(list), descending? -1: 1, list->base->compare);

    return 0;
}

int genericlist_stable_sort(GenericList list, int descending) {
    if (list->base->compare == NULL)
        return CC_ENOTSUP;

    const size_t element_size = genericlist_stored_element_size(list->base);
    void **temp = genericlist_size(list)? MALLOC(genericlist_size(list) * element_size): NULL;
    if (temp == NULL)
        return CC_ENOMEM;

    memcpy(temp, list->array, genericlist_size(list) * element_size);
    if (list->base->size && list->base->size <= sizeof(void*))
        genericlist_pod_merge_sort(temp, list->array, 0, genericlist_size(list), element_size, descending? -1: 1, list->base->compare);
    else
        genericlist_ptr_merge_sort(temp, list->array, 0, genericlist_size(list), descending? -1: 1, list->base->compare);

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
    if (it == NULL || genericlist_size(list) == 0)
        return NULL;

    if (list->base->size && list->base->size <= sizeof(void*)) {
        const size_t element_size = list->base->size;

        if ((char *) it == (char *) list->array + ((list->array_size-1) * element_size))
            return NULL;

        return (char *) it + element_size;
    } else {
        void **array = list->array;

        if (it == array[list->array_size-1])
            return NULL;

        return (void **) it + 1;
    }
}

void *genericlist_value_of(GenericList list, Iterator it) {
    UNUSED(list)

    if (list->base->size && list->base->size <= sizeof(void*))
        return it;
    else
        return *((void **) it);
}

void *genericlist_value_at(GenericList list, size_t index) {
    if (list->base->size && list->base->size <= sizeof(void*)) {
        const size_t element_size = list->base->size;

        return (char *) list->array + (index * element_size);
    } else {
        void **array = (void **) list->array;

        return array[index];
    }
}

size_t genericlist_size(GenericList list) {
    return list->array_size;
}

Compare genericlist_get_compare_fn(GenericList list) {
    return list->base->compare;
}

Copier genericlist_get_copier_fn(GenericList list) {
    return list->base->copier;
}

Deleter genericlist_get_deleter_fn(GenericList list) {
    return list->base->deleter;
}

Parser genericlist_get_parser_fn(GenericList list) {
    return list->base->parse;
}

Serializer genericlist_get_serializer_fn(GenericList list) {
    return list->base->serialize;
}

int genericlist_set_compare_fn(GenericList list, Compare compare) {
    CommonContainerBase *base = container_base_copy_if_static(list->base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->compare = compare;
    list->base = base;

    return 0;
}

int genericlist_set_copier_fn(GenericList list, Copier copier) {
    CommonContainerBase *base = container_base_copy_if_static(list->base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->copier = copier;
    list->base = base;

    return 0;
}

int genericlist_set_deleter_fn(GenericList list, Deleter deleter) {
    CommonContainerBase *base = container_base_copy_if_static(list->base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->deleter = deleter;
    list->base = base;

    return 0;
}

int genericlist_set_parser_fn(GenericList list, Parser parser) {
    CommonContainerBase *base = container_base_copy_if_static(list->base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->parse = parser;
    list->base = base;

    return 0;
}

int genericlist_set_serializer_fn(GenericList list, Serializer serializer) {
    CommonContainerBase *base = container_base_copy_if_static(list->base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->serialize = serializer;
    list->base = base;

    return 0;
}

void genericlist_clear(GenericList list) {
    genericlist_erase(list, 0, list->array_size);
}

void genericlist_destroy(GenericList list) {
    if (list) {
        Deleter deleter = list->base->size? list->base->size <= sizeof(void*)? NULL: FREE: list->base->deleter;

        if (deleter) {
            void **array = (void **) list->array;

            for (size_t i = 0; i < genericlist_size(list); ++i)
                deleter(array[i]);
        }

        container_base_destroy_if_dynamic(list->base);
        FREE(list->array);
        FREE(list);
    }
}

const CommonContainerBase *genericlist_get_container_base(GenericList list) {
    return list->base;
}

CommonContainerBase *genericlist_build_recipe(GenericList list) {
    return container_base_build_container(genericlist_get_container_base(list),
                                          container_base_genericlist_recipe());
}
