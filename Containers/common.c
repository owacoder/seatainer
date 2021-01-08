/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "common.h"
#include <stdint.h>

/* These structs really aren't used at all, they're just present to emit compiler warnings if assigning one container type to another.
 * Custom extensions of the generic types should use the same pattern:
 *
 * container.h:
 *
 *      typedef struct ContainerStruct *Container;
 *
 * container.c:
 *
 *      struct ContainerStruct {char dummy;}; // Dummy struct just for type-safety warnings
 *
 *      void container_function(Container c) {
 *          generic_function((GenericContainer) c); // Cast to generic container type when it needs to be used
 *      }
 *
 * This prevents the user from unknowningly assigning one "type" of void * to another variable.
 */
#if 0
struct ContainerStruct {char dummy;};
#endif

size_t container_base_element_space_required(const CommonContainerBase *base) {
    if (base == NULL)
        return 0;

    return base->size? base->size: sizeof(void*);
}

static CommonContainerBase *container_base_copy_first_level(const CommonContainerBase *base) {
    if (base == NULL)
        return NULL;

    CommonContainerBase *new_base = MALLOC(sizeof(*base));
    if (new_base == NULL)
        return NULL;

    memcpy(new_base, base, sizeof(*base));
    new_base->dynamic = 1;
    new_base->value_child = new_base->key_child = NULL;

    return new_base;
}

CommonContainerBase *container_base_copy(const CommonContainerBase *base) {
    if (base == NULL)
        return NULL;

    CommonContainerBase *new_base = container_base_copy_first_level(base);
    if (new_base == NULL)
        return NULL;

    if (base->value_child) {
        new_base->value_child = container_base_copy(base->value_child);
        if (new_base->value_child == NULL) {
            container_base_destroy_if_dynamic(new_base);
            return NULL;
        }
    }

    if (base->key_child) {
        new_base->key_child = container_base_copy(base->key_child);
        if (new_base->key_child == NULL) {
            container_base_destroy_if_dynamic(new_base);
            return NULL;
        }
    }

    return new_base;
}

CommonContainerBase *container_base_copy_if_dynamic(const CommonContainerBase *base) {
    if (base == NULL)
        return NULL;

    if (!base->dynamic)
        return (CommonContainerBase *) base;

    CommonContainerBase *new_base = MALLOC(sizeof(*base));
    if (new_base == NULL)
        return NULL;

    memcpy(new_base, base, sizeof(*base));
    new_base->dynamic = 1;

    if (base->value_child) {
        new_base->value_child = container_base_copy_if_dynamic(base->value_child);
        if (new_base->value_child == NULL) {
            container_base_destroy_if_dynamic(new_base);
            return NULL;
        }
    }

    if (base->key_child) {
        new_base->key_child = container_base_copy(base->key_child);
        if (new_base->key_child == NULL) {
            container_base_destroy_if_dynamic(new_base);
            return NULL;
        }
    }

    return new_base;
}

static int container_base_is_static_in_top_levels(const CommonContainerBase *base, size_t levels) {
    if (levels == 0 || base == NULL)
        return 0;

    if (!base->dynamic)
        return 1;

    return container_base_is_static_in_top_levels(base->value_child, levels-1) ||
           container_base_is_static_in_top_levels(base->key_child, levels-1);
}

CommonContainerBase *container_base_copy_if_static(const CommonContainerBase *base, size_t levels) {
    if (container_base_is_static_in_top_levels(base, levels))
        return container_base_copy(base);

    return (CommonContainerBase *) base;
}

CommonContainerBase *container_base_build_container(const CommonContainerBase *elements, const CommonContainerBase *container) {
    if (elements == NULL || container == NULL)
        return NULL;

    if (!elements->dynamic && !container->dynamic && container->value_child == elements)
        return (CommonContainerBase *) container;

    CommonContainerBase *new_elements = container_base_copy_if_dynamic(elements);
    CommonContainerBase *new_container = container_base_copy(container);
    if (new_elements == NULL || new_container == NULL) {
        container_base_destroy_if_dynamic(new_elements);
        container_base_destroy_if_dynamic(new_container);
        return NULL;
    }

    container_base_destroy_if_dynamic(new_container->value_child);
    new_container->value_child = new_elements;

    return new_container;
}

CommonContainerBase *container_base_build_key_value_container(const CommonContainerBase *keys, const CommonContainerBase *elements, const CommonContainerBase *container) {
    if (keys == NULL || elements == NULL || container == NULL)
        return NULL;

    if (!elements->dynamic && !container->dynamic && !keys->dynamic &&
            container->value_child == elements && container->key_child == keys)
        return (CommonContainerBase *) container;

    CommonContainerBase *new_keys = container_base_copy_if_dynamic(keys);
    CommonContainerBase *new_elements = container_base_copy_if_dynamic(elements);
    CommonContainerBase *new_container = container_base_copy_first_level(container);
    if (new_keys == NULL || new_elements == NULL || new_container == NULL) {
        container_base_destroy_if_dynamic(new_keys);
        container_base_destroy_if_dynamic(new_elements);
        container_base_destroy_if_dynamic(new_container);
        return NULL;
    }

    new_container->value_child = new_elements;
    new_container->key_child = new_keys;

    return new_container;
}

void container_base_destroy_if_dynamic(CommonContainerBase *base) {
    if (base && base->dynamic) {
        container_base_destroy_if_dynamic(base->key_child);
        container_base_destroy_if_dynamic(base->value_child);

        FREE(base);
    }
}

AllocatedSpace allocated_space_for_type(const CommonContainerBase *base) {
    if (base == NULL)
        return NULL;
    else if (base->size)
        return CALLOC(1, base->size);
    else {
        void **p = MALLOC(sizeof(void*));
        if (p == NULL)
            return NULL;

        *p = NULL;
        return p;
    }
}

void *allocated_space_get_object(AllocatedSpace space, const CommonContainerBase *base) {
    if (space == NULL)
        return NULL;
    else if (base->size)
        return space;
    else
        return *((void **) space);
}

void allocated_space_destroy_after_object_move(AllocatedSpace space, const CommonContainerBase *base) {
    if (!base->size)
        FREE(space);
}

void allocated_space_destroy(AllocatedSpace space, const CommonContainerBase *base) {
    if (!base->size && space && base->deleter)
        base->deleter(*((void **) space));

    FREE(space);
}

int generic_types_compatible_compare(const CommonContainerBase *lhs, const CommonContainerBase *rhs) {
    if (lhs == rhs || (lhs == NULL && rhs == NULL))
        return 0;
    else if (lhs == NULL || rhs == NULL)
        return CompareUnordered;

    /* If copiers and deleters are the same, it's the same type */
    if (lhs->copier == rhs->copier && lhs->deleter == rhs->deleter)
        ;
    /* If no copier, but compare and deleters are the same, it's the same type */
    else if (lhs->copier == NULL && rhs->copier == NULL && lhs->compare == rhs->compare && lhs->deleter == rhs->deleter)
        ;
    /* Otherwise, the types are different, and unordered */
    else
        return CompareUnordered;

    int cmp = generic_types_compatible_compare(lhs->key_child, rhs->key_child);
    if (cmp)
        return cmp;

    return generic_types_compatible_compare(lhs->value_child, rhs->value_child);
}

Binary *binary_alloc(const char *str) {
    return binary_alloc_n(str, strlen(str));
}

Binary *binary_alloc_n(const char *str, size_t str_length) {
    Binary *b = MALLOC(sizeof(*b));
    char *data = MALLOC(str_length+1);
    if (b == NULL || data == NULL) {
        FREE(b);
        FREE(data);
        return NULL;
    }

    memcpy(data, str, str_length);
    data[str_length] = 0;

    b->data = data;
    b->length = str_length;

    return b;
}

Binary *binary_copy(const Binary *other) {
    return binary_alloc_n(other->data, other->length);
}

int binary_compare(const Binary *a, const Binary *b) {
    size_t max = MIN(a->length, b->length);
    int cmp = memcmp(a->data, b->data, max);

    if (cmp)
        return cmp;
    else if (a->length < max)
        return -1;
    else if (b->length < max)
        return 1;
    else
        return 0;
}

void binary_destroy(Binary *b) {
    if (b) {
        FREE(b->data);
        FREE(b);
    }
}

void *generic_pod_copy_alloc(const void *p, size_t bytes) {
    if (p == NULL)
        return NULL;

    void *new_p = MALLOC(bytes);
    if (new_p == NULL)
        return NULL;

    memcpy(new_p, p, bytes);

    return new_p;
}

void *generic_identitycopy(const void *p) {
    return (void *) p;
}
