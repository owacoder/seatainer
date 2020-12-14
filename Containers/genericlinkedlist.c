#include "genericlinkedlist.h"
#include "../utility.h"
#include "recipes.h"

/* For conversions */
#include "variant.h"
#include "genericmap.h"

struct GenericLinkedListNode {
    struct GenericLinkedListNode *next;
    unsigned char data[];
};

struct GenericLinkedListStruct {
    CommonContainerBase *base;
    struct GenericLinkedListNode *head, *tail;
    size_t size;
};

Variant variant_from_genericlinkedlist(GenericLinkedList list) {
    return variant_create_custom_adopt(list, genericlinkedlist_build_recipe(list));
}

int variant_is_genericlinkedlist(Variant var) {
    return generic_types_compatible_compare(variant_get_custom_container_base(var), container_base_genericlinkedlist_recipe()) == 0;
}

GenericLinkedList variant_get_genericlinkedlist(Variant var) {
    if (!variant_is_genericlinkedlist(var))
        return NULL;

    return variant_get_custom(var);
}

int variant_set_genericlinkedlist_move(Variant var, GenericLinkedList list) {
    return variant_set_custom_move(var, list, genericlinkedlist_get_container_base(list));
}

int variant_set_genericlinkedlist(Variant var, const GenericLinkedList list) {
    return variant_set_custom(var, list, genericlinkedlist_get_container_base(list));
}

GenericLinkedList genericlinkedlist_create(const CommonContainerBase *base) {
    struct GenericLinkedListStruct *list = CALLOC(1, sizeof(*list));
    CommonContainerBase *new_base = container_base_copy_if_dynamic(base);
    if (list == NULL || new_base == NULL) {
        FREE(list);
        container_base_destroy_if_dynamic(new_base);
        return NULL;
    }

    list->base = new_base;

    return list;
}

GenericLinkedList genericlinkedlist_copy(GenericLinkedList other) {
    GenericLinkedList list = genericlinkedlist_create(genericlinkedlist_get_container_base(other));
    if (list == NULL)
        return NULL;

    for (Iterator it = genericlinkedlist_begin(other); it; it = genericlinkedlist_next(other, it)) {
        if (genericlinkedlist_append(list, genericlinkedlist_value_of(other, it))) {
            genericlinkedlist_destroy(list);
            return NULL;
        }
    }

    return list;
}

GenericLinkedList genericlinkedlist_concatenate(GenericLinkedList left, GenericLinkedList right) {
    if (generic_types_compatible_compare(genericlinkedlist_get_container_base(left), genericlinkedlist_get_container_base(right)) != 0)
        return NULL;

    GenericLinkedList list = genericlinkedlist_create(genericlinkedlist_get_container_base(left));
    if (list == NULL)
        return NULL;

    for (Iterator it = genericlinkedlist_begin(left); it; it = genericlinkedlist_next(left, it)) {
        if (genericlinkedlist_append(list, genericlinkedlist_value_of(left, it))) {
            genericlinkedlist_destroy(list);
            return NULL;
        }
    }

    for (Iterator it = genericlinkedlist_begin(right); it; it = genericlinkedlist_next(right, it)) {
        if (genericlinkedlist_append(list, genericlinkedlist_value_of(right, it))) {
            genericlinkedlist_destroy(list);
            return NULL;
        }
    }

    return list;
}

GenericLinkedList genericlinkedlist_from_genericmap_values(GenericMap other) {
    GenericLinkedList list = genericlinkedlist_create(genericmap_get_value_container_base(other));
    if (!list)
        return NULL;

    for (Iterator it = genericmap_begin(other); it; it = genericmap_next(other, it)) {
        if (genericlinkedlist_append(list, genericmap_value_of(other, it))) {
            genericlinkedlist_destroy(list);
            return NULL;
        }
    }

    return list;
}

GenericLinkedList genericlinkedlist_from_array(const void **items, const CommonContainerBase *base) {
    const void **ptr = items;
    size_t count = 0;

    for (; *ptr; ++ptr, ++count);

    return genericlinkedlist_from_array_n(items, count, base);
}

GenericLinkedList genericlinkedlist_from_array_n(const void **items, size_t count, const CommonContainerBase *base) {
    GenericLinkedList list = genericlinkedlist_create(base);
    if (!list)
        return NULL;

    for (size_t i = 0; i < count; ++i) {
        if (genericlinkedlist_append(list, items[i])) {
            genericlinkedlist_destroy(list);
            return NULL;
        }
    }

    return list;
}

int genericlinkedlist_append_list(GenericLinkedList list, GenericLinkedList other) {

}

int genericlinkedlist_append(GenericLinkedList list, const void *item) {
    return genericlinkedlist_insert(list, item, list->tail);
}

int genericlinkedlist_append_move(GenericLinkedList list, void *item) {
    return genericlinkedlist_insert_move(list, item, list->tail);
}

int genericlinkedlist_prepend_list(GenericLinkedList list, GenericLinkedList other) {

}

int genericlinkedlist_prepend(GenericLinkedList list, const void *item) {
    return genericlinkedlist_insert(list, item, NULL);
}

int genericlinkedlist_prepend_move(GenericLinkedList list, void *item) {
    return genericlinkedlist_insert_move(list, item, NULL);
}

int genericlinkedlist_insert_list(GenericLinkedList list, GenericLinkedList other, Iterator after_it) {

}

int genericlinkedlist_insert(GenericLinkedList list, const void *item, Iterator after_it) {
    if (list->base->size) { /* POD type */
        struct GenericLinkedListNode *new_node = MALLOC(sizeof(*new_node) + list->base->size), *prev_node = after_it;
        if (new_node == NULL)
            return CC_ENOMEM;

        memcpy(new_node->data, item, list->base->size);

        if (prev_node) { /* Inserting after a node */
            new_node->next = prev_node->next;
            prev_node->next = new_node;

            if (prev_node == list->tail) /* May have to update the tail if inserting after it */
                list->tail = new_node;
        } else { /* Insert at start of list */
            if (list->head) /* If there is an element, link to it */
                new_node->next = list->head->next;
            else /* If list is empty, tail should be set too */
                list->tail = new_node;

            list->head = new_node;
        }

        ++list->size;
    } else { /* Non-POD type */
        if (list->base->copier == NULL)
            return CC_ENOTSUP;

        void *duplicate = list->base->copier(item);
        if (duplicate == NULL && item != NULL)
            return CC_ENOMEM;

        int err = genericlinkedlist_insert_move(list, duplicate, after_it);
        if (err && list->base->deleter)
            list->base->deleter(duplicate);
    }

    return 0;
}

int genericlinkedlist_insert_move(GenericLinkedList list, void *item, Iterator after_it) {
    if (list->base->size) { /* POD type */
        int err = genericlinkedlist_insert(list, item, after_it);
        if (err)
            return err;

        FREE(item);
    } else { /* Non-POD type */
        struct GenericLinkedListNode *new_node = MALLOC(sizeof(*new_node)), *prev_node = after_it;
        if (new_node == NULL)
            return CC_ENOMEM;

        *((void **) new_node->data) = item;

        if (prev_node) { /* Inserting after a node */
            new_node->next = prev_node->next;
            prev_node->next = new_node;

            if (prev_node == list->tail) /* May have to update the tail if inserting after it */
                list->tail = new_node;
        } else { /* Insert at start of list */
            if (list->head) /* If there is an element, link to it */
                new_node->next = list->head->next;
            else /* If list is empty, tail should be set too */
                list->tail = new_node;

            list->head = new_node;
        }

        ++list->size;
    }

    return 0;
}

int genericlinkedlist_replace_at(GenericLinkedList list, Iterator it, const void *item) {
    if (list->base->size) { /* POD type */
        struct GenericLinkedListNode *node = it;

        memmove(node->data, item, list->base->size);
    } else { /* Non-POD type */
        if (list->base->copier == NULL)
            return CC_ENOTSUP;

        void *duplicate = list->base->copier(item);
        if (duplicate == NULL && item != NULL)
            return CC_ENOMEM;

        int err = genericlinkedlist_replace_move_at(list, it, duplicate);
        if (err && list->base->deleter)
            list->base->deleter(duplicate);
    }

    return 0;
}

int genericlinkedlist_replace_move_at(GenericLinkedList list, Iterator it, void *item) {
    if (list->base->size) { /* POD type */
        int err = genericlinkedlist_replace_at(list, it, item);
        if (err)
            return err;

        FREE(item);
    } else { /* Non-POD type */
        struct GenericLinkedListNode *node = it;

        if (list->base->deleter)
            list->base->deleter(node->data);

        *((void **) node->data) = item;
    }

    return 0;
}

size_t genericlinkedlist_remove_after(GenericLinkedList list, Iterator it) {
    if (list->head == NULL)
        return 0;

    struct GenericLinkedListNode *node = NULL;

    if (it == NULL) { /* Remove head of list */
        node = list->head;

        if (list->tail == list->head)
            list->head = list->tail = NULL;
        else
            list->head = list->head->next;
    } else {
        struct GenericLinkedListNode *prev = it;
        node = prev->next;

        if (node == NULL) /* Trying to delete after tail */
            return 0;

        if (list->tail == node)
            list->tail = prev;

        prev->next = node->next;
    }

    if (!list->base->size && list->base->deleter)
        list->base->deleter(node->data);

    FREE(node);
    --list->size;

    return 1;
}

Iterator genericlinkedlist_find(GenericLinkedList list, const void *item, Iterator after, Iterator *prior) {
    if (list->base->compare == NULL)
        return NULL;

    Iterator last = after;
    for (Iterator it = after? genericlinkedlist_next(list, after): genericlinkedlist_begin(list); it; it = genericlinkedlist_next(list, it)) {
        if (list->base->compare(item, genericlinkedlist_value_of(list, it)) == 0) {
            if (prior)
                *prior = last;

            return it;
        }

        last = it;
    }

    if (prior)
        *prior = NULL;

    return NULL;
}

size_t genericlinkedlist_remove_one(GenericLinkedList list, const void *item) {
    Iterator prior;
    Iterator found = genericlinkedlist_find(list, item, NULL, &prior);
    if (found == NULL)
        return 0;

    return genericlinkedlist_remove_after(list, prior);
}

size_t genericlinkedlist_remove_all(GenericLinkedList list, const void *item) {
    size_t count = 0;
    Iterator last = NULL;

    while (1) {
        Iterator it = genericlinkedlist_find(list, item, last, &last);
        if (it == NULL)
            return count;

        count += genericlinkedlist_remove_after(list, last);
    }
}

int genericlinkedlist_contains(GenericLinkedList list, const void *item) {
    return genericlinkedlist_find(list, item, NULL, NULL) != NULL;
}

int genericlinkedlist_compare(GenericLinkedList list, GenericLinkedList other) {
    int cmp = generic_types_compatible_compare(genericlinkedlist_get_container_base(list), genericlinkedlist_get_container_base(other));
    if (cmp)
        return cmp;

    const size_t max = MIN(genericlinkedlist_size(list), genericlinkedlist_size(other));

    if (list->base->compare != NULL) {
        Iterator  left = genericlinkedlist_begin(list);
        Iterator right = genericlinkedlist_begin(other);

        for (; left && right; left = genericlinkedlist_next(list, left), right = genericlinkedlist_next(other, right)) {
            int cmp = list->base->compare(genericlinkedlist_value_of(list, left), genericlinkedlist_value_of(other, right));

            if (cmp)
                return cmp;
        }
    }

    if (genericlinkedlist_size(other) != max)
        return -1;
    else if (genericlinkedlist_size(list) != max)
        return 1;

    return 0;
}

GenericLinkedList genericlinkedlist_sorted(GenericLinkedList list, int descending) {
    return genericlinkedlist_stable_sorted(list, descending);
}

GenericLinkedList genericlinkedlist_stable_sorted(GenericLinkedList list, int descending) {
    GenericLinkedList copy = genericlinkedlist_copy(list);
    if (copy == NULL || genericlinkedlist_stable_sort(copy, descending)) {
        genericlinkedlist_destroy(copy);
        return NULL;
    }

    return copy;
}

int genericlinkedlist_sort(GenericLinkedList list, int descending) {
    return genericlinkedlist_stable_sort(list, descending);
}

/* Both left and right lists are guaranteed to have at least one element */
static struct GenericLinkedListNode *genericlinkedlist_merge_sort_helper(GenericLinkedList list,
                                                                         struct GenericLinkedListNode *left,
                                                                         struct GenericLinkedListNode *right,
                                                                         Compare compare, int descending,
                                                                         struct GenericLinkedListNode **tailptr,
                                                                         int *err) {
    struct GenericLinkedListNode *head = NULL, *tail = NULL;

    while (left && right) {
        struct GenericLinkedListNode *temp = NULL;
        int cmp = compare(genericlinkedlist_value_of(list, left),
                          genericlinkedlist_value_of(list, right));
        if (cmp == CompareUnordered && err)
            *err = CC_ENOTSUP;

        cmp *= descending;

        if (cmp <= 0) {
            temp = left;
            left = left->next;
        } else {
            temp = right;
            right = right->next;
        }

        if (head == NULL) {
            head = tail = temp;
        } else {
            tail->next = temp;
            tail = temp;
        }
    }

    /* Tail is guaranteed to not be NULL because both lists have at least one element and the previous loop will have added something to `head` */
    for (; left; left = left->next) {
        tail->next = left;
        tail = left;
    }

    for (; right; right = right->next) {
        tail->next = right;
        tail = right;
    }

    if (tailptr)
        *tailptr = tail;

    return head;
}

static struct GenericLinkedListNode *genericlinkedlist_merge_sort(GenericLinkedList list,
                                                                  struct GenericLinkedListNode *left,
                                                                  size_t length,
                                                                  Compare compare,
                                                                  int descending,
                                                                  struct GenericLinkedListNode **tailptr, int *err) {
    struct GenericLinkedListNode *right = left, *last = left;

    if (length <= 1) {
        if (tailptr)
            *tailptr = left;

        if (left)
            left->next = NULL;

        return left;
    }

    size_t half = length / 2;
    for (size_t i = half; i; --i) {
        last = right;
        right = right->next;
    }

    last->next = NULL; /* Split left list off right list for merge function */
    left = genericlinkedlist_merge_sort(list, left, half, compare, descending, NULL, err);
    right = genericlinkedlist_merge_sort(list, right, length - half, compare, descending, NULL, err);

    return genericlinkedlist_merge_sort_helper(list, left, right, compare, descending, tailptr, err);
}

int genericlinkedlist_stable_sort(GenericLinkedList list, int descending) {
    if (list->base->compare == NULL)
        return CC_ENOTSUP;

    int result = 0;
    list->head = genericlinkedlist_merge_sort(list, list->head, list->size, list->base->compare, descending? -1: 1, &list->tail, &result);

    return result;
}

Iterator genericlinkedlist_begin(GenericLinkedList list) {
    return list->head;
}

Iterator genericlinkedlist_next(GenericLinkedList list, Iterator it) {
    UNUSED(list)

    return it? ((struct GenericLinkedListNode *) it)->next: NULL;
}

void *genericlinkedlist_value_of(GenericLinkedList list, Iterator it) {
    struct GenericLinkedListNode *node = it;

    if (list->base->size)
        return node->data;
    else
        return *((void **) node->data);
}

size_t genericlinkedlist_size(GenericLinkedList list) {
    return list->size;
}

void genericlinkedlist_clear(GenericLinkedList list) {
    struct GenericLinkedListNode *node = list->head, *temp;
    while (node) {
        temp = node;
        node = node->next;

        if (!list->base->size && list->base->deleter)
            list->base->deleter(temp->data);

        FREE(temp);
    }

    list->size = 0;
    list->head = list->tail = NULL;
}

void genericlinkedlist_destroy(GenericLinkedList list) {
    if (list != NULL) {
        struct GenericLinkedListNode *node = list->head, *temp;
        while (node) {
            temp = node;
            node = node->next;

            if (!list->base->size && list->base->deleter)
                list->base->deleter(temp->data);

            FREE(temp);
        }

        container_base_destroy_if_dynamic(list->base);
        FREE(list);
    }
}

Copier genericlinkedlist_get_copier_fn(GenericLinkedList list) {
    return list->base->copier;
}

Deleter genericlinkedlist_get_deleter_fn(GenericLinkedList list) {
    return list->base->deleter;
}

Compare genericlinkedlist_get_compare_fn(GenericLinkedList list) {
    return list->base->compare;
}

Parser genericlinkedlist_get_parser_fn(GenericLinkedList list) {
    return list->base->parse;
}

Serializer genericlinkedlist_get_serializer_fn(GenericLinkedList list) {
    return list->base->serialize;
}

int genericlinkedlist_set_copier_fn(GenericLinkedList list, Copier copier) {
    CommonContainerBase *base = container_base_copy_if_static(list->base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->copier = copier;
    list->base = base;

    return 0;
}

int genericlinkedlist_set_deleter_fn(GenericLinkedList list, Deleter deleter) {
    CommonContainerBase *base = container_base_copy_if_static(list->base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->deleter = deleter;
    list->base = base;

    return 0;
}

int genericlinkedlist_set_compare_fn(GenericLinkedList list, Compare compare) {
    CommonContainerBase *base = container_base_copy_if_static(list->base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->compare = compare;
    list->base = base;

    return 0;
}

int genericlinkedlist_set_parser_fn(GenericLinkedList list, Parser parser) {
    CommonContainerBase *base = container_base_copy_if_static(list->base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->parse = parser;
    list->base = base;

    return 0;
}

int genericlinkedlist_set_serializer_fn(GenericLinkedList list, Serializer serializer) {
    CommonContainerBase *base = container_base_copy_if_static(list->base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->serialize = serializer;
    list->base = base;

    return 0;
}

const CommonContainerBase *genericlinkedlist_get_container_base(GenericLinkedList list) {
    return list->base;
}

CommonContainerBase *genericlinkedlist_build_recipe(GenericLinkedList list) {
    return container_base_build_container(list->base, container_base_genericlinkedlist_recipe());
}
