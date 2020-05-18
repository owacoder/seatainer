#include "ccstringlist.h"

#include "utility.h"

struct StringlistStruct {
    char **array;
    size_t array_size;
    size_t array_capacity;
};

Stringlist stringlist_create() {
    Stringlist list = CALLOC(1, sizeof(*list));
    if (!list)
        return NULL;

    list->array_capacity = 8;
    list->array = MALLOC(list->array_capacity * sizeof(char*));
    list->array[0] = NULL;

    return list;
}

Stringlist stringlist_copy(Stringlist other) {
    Stringlist list = stringlist_create();
    if (!list)
        return NULL;

    for (size_t i = 0; i < stringlist_size(other); ++i)
        if (stringlist_append(list, stringlist_array(other)[i])) {
            stringlist_destroy(list);
            return NULL;
        }

    return list;
}

int stringlist_append(Stringlist list, const char *item) {
    if (list->array_size+1 == list->array_capacity) {
        size_t new_capacity = MAX(list->array_capacity + (list->array_capacity / 2), list->array_capacity) + 1;
        size_t new_size = safe_multiply(new_capacity, sizeof(char*));
        if (!new_size)
            return CC_ENOMEM;

        char **new_array = REALLOC(list->array, new_size);
        if (!new_array)
            return CC_ENOMEM;

        list->array_capacity = new_capacity;
        list->array = new_array;
    }

    char *duplicate = strdup(item);
    if (!duplicate)
        return CC_ENOMEM;

    list->array[list->array_size++] = duplicate;
    list->array[list->array_size] = NULL;
    return 0;
}

char **stringlist_array(Stringlist list) {
    return list->array;
}

size_t stringlist_size(Stringlist list) {
    return list->array_size;
}

void stringlist_destroy(Stringlist list) {
    for (size_t i = 0; i < stringlist_size(list); ++i)
        FREE(list->array[i]);

    FREE(list->array);
    FREE(list);
}
