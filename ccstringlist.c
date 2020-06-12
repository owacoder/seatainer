#include "ccstringlist.h"

#include "utility.h"

typedef struct CompareContext {
    Compare compare; /* Takes `void *` or `Binary *` arguments */
    Compare strcompare; /* Takes `char *` arguments */
} CompareContext;

typedef struct AVLNode {
    struct AVLNode *left, *right, *parent;
    Binary key;
    void *value;
    int balance;
} AVLNode;

typedef struct AVLTree {
    struct AVLNode *root;
    Compare compare;
    Copier copy;
    Deleter deleter;
    size_t size;
} AVLTree;

static int binary_compare(const Binary *a, const Binary *b) {
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

static void *avltree_copier(const void *p) {
    return (void *) p;
}

static void avltree_deleter(void *p) {
    UNUSED(p)
}

static AVLNode *avltree_nodealloc(const char *key, size_t key_len) {
    AVLNode *node = CALLOC(1, sizeof(*node));
    if (node == NULL)
        return NULL;

    node->key.data = MALLOC(key_len + 1);
    if (node->key.data == NULL) {
        FREE(node);
        return NULL;
    }

    memcpy(node->key.data, key, key_len);
    node->key.data[key_len] = 0;
    node->key.length = key_len;

    return node;
}

static void avltree_nodedestroy(AVLNode *node, Deleter deleter) {
    deleter(node->value);
    FREE(node->key.data);
    FREE(node);
}

static Compare avltree_get_compare_fn(AVLTree *tree) {
    return tree->compare;
}

static void avltree_set_compare_fn(AVLTree *tree, Compare compare) {
    tree->compare = compare? compare: (Compare) binary_compare;
}

static Copier avltree_get_copier_fn(AVLTree *tree) {
    return tree->copy;
}

static void avltree_set_copier_fn(AVLTree *tree, Copier copier) {
    tree->copy = copier? copier: (Copier) avltree_copier;
}

static Deleter avltree_get_deleter_fn(AVLTree *tree) {
    return tree->deleter;
}

static void avltree_set_deleter_fn(AVLTree *tree, Deleter deleter) {
    tree->deleter = deleter? deleter: avltree_deleter;
}

static AVLTree *avltree_create() {
    AVLTree *tree = CALLOC(1, sizeof(*tree));
    if (tree == NULL)
        return NULL;

    tree->compare = (Compare) binary_compare;
    tree->copy = avltree_copier;
    tree->deleter = avltree_deleter;

    return tree;
}

static void avltree_destroy_helper(AVLNode *node, Deleter deleter) {
    if (node == NULL)
        return;

    avltree_destroy_helper(node->left, deleter);
    avltree_destroy_helper(node->right, deleter);
    avltree_nodedestroy(node, deleter);
}

static void avltree_destroy(AVLTree *tree) {
    if (tree != NULL)
        avltree_destroy_helper(tree->root, tree->deleter);

    FREE(tree);
}

static AVLNode *avltree_copy_helper(AVLNode *node, AVLNode *parent, Copier copier, Deleter deleter) {
    if (node == NULL)
        return NULL;

    AVLNode *new_node = avltree_nodealloc(node->key.data, node->key.length);
    if (new_node == NULL)
        return NULL;

    new_node->parent = parent;
    new_node->value = copier(node->value);
    new_node->left = avltree_copy_helper(node->left, new_node, copier, deleter);
    new_node->right = avltree_copy_helper(node->right, new_node, copier, deleter);

    if ((new_node->value == NULL && node->value != NULL) ||
        (new_node->left == NULL && node->left != NULL) ||
        (new_node->right == NULL && node->right != NULL)) {
        avltree_destroy_helper(new_node, deleter);
        return NULL;
    }

    return new_node;
}

static AVLTree *avltree_copy(AVLTree *other) {
    AVLTree *tree = avltree_create();
    if (tree == NULL)
        return NULL;

    tree->compare = other->compare;
    tree->copy = other->copy;
    tree->deleter = other->deleter;
    tree->size = other->size;
    tree->root = avltree_copy_helper(other->root, NULL, other->copy, other->deleter);

    if (tree->root == NULL && other->root != NULL) {
        avltree_destroy(tree);
        return NULL;
    }

    return tree;
}

static AVLNode **avltree_find_helper(AVLNode **node, AVLNode **parent, Compare compare, Binary item) {
    *parent = NULL;

    while (1) {
        if (*node == NULL)
            return node;

        int cmp = compare(&item, &((*node)->key));

        if (cmp == 0)
            return node;
        else if (cmp < 0) {
            *parent = *node;
            node = &((*node)->left);
        }
        else if (cmp > 0) {
            *parent = *node;
            node = &((*node)->right);
        }
    }
}

static AVLNode *avltree_min_node(AVLTree *tree) {
    AVLNode *node = tree->root;

    if (node == NULL)
        return NULL;

    while (node->left != NULL) {
        node = node->left;
    }

    return node;
}

static AVLNode *avltree_max_node(AVLTree *tree) {
    AVLNode *node = tree->root;

    if (node == NULL)
        return NULL;

    while (node->right != NULL) {
        node = node->right;
    }

    return node;
}

static AVLNode *avltree_inorder_previous(AVLNode *node) {
    AVLNode *last = NULL;

    if (node->left != NULL) {
        node = node->left;

        while (node->right != NULL)
            node = node->right;

        return node;
    }

    while (node->left == last) {
        last = node;
        node = node->parent;

        if (node == NULL)
            return NULL;
    }

    return node;
}

static AVLNode *avltree_inorder_next(AVLNode *node) {
    AVLNode *last = NULL;

    if (node->right != NULL) {
        node = node->right;

        while (node->left != NULL)
            node = node->left;

        return node;
    }

    while (node->right == last) {
        last = node;
        node = node->parent;

        if (node == NULL)
            return NULL;
    }

    return node;
}

static AVLNode *avltree_find(AVLTree *tree, const char *key, size_t key_len) {
    Binary b = {.data = (char *) key, .length = key_len};
    AVLNode *parent;

    return *avltree_find_helper(&tree->root, &parent, tree->compare, b);
}

static AVLNode *avltree_rotate_left(AVLNode *parent, AVLNode *child) {
    AVLNode *temp = child->left;
    child->left = parent;
    parent->parent = child;
    parent->right = temp;

    if (temp != NULL)
        temp->parent = parent;

    if (child->balance == 0) { /* Child already a balanced tree, will become left heavy */
        parent->balance = +1;
        child->balance = -1;
    } else {
        parent->balance = child->balance = 0;
    }

    return child;
}

static AVLNode *avltree_rotate_right(AVLNode *parent, AVLNode *child) {
    AVLNode *temp = child->right;
    child->right = parent;
    parent->parent = child;
    parent->left = temp;

    if (temp != NULL)
        temp->parent = parent;

    if (child->balance == 0) { /* Child already a balanced tree, will become right heavy */
        parent->balance = -1;
        child->balance = +1;
    } else {
        parent->balance = child->balance = 0;
    }

    return child;
}

static AVLNode *avltree_rotate_rightleft(AVLNode *parent, AVLNode *child) {
    AVLNode *grandchild = child->left;

    child->left = grandchild->right;
    grandchild->right = child;
    if (child->left != NULL)
        child->left->parent = child;
    child->parent = grandchild;

    parent->right = grandchild->left;
    grandchild->left = parent;
    parent->parent = grandchild;
    if (parent->right != NULL)
        parent->right->parent = parent;

    if (grandchild->balance < 0) {
        parent->balance = 0;
        child->balance = +1;
    } else if (grandchild->balance > 0) {
        parent->balance = -1;
        child->balance = 0;
    } else {
        parent->balance = child->balance = 0;
    }
    grandchild->balance = 0;

    return grandchild;
}

static AVLNode *avltree_rotate_leftright(AVLNode *parent, AVLNode *child) {
    AVLNode *grandchild = child->right;

    child->right = grandchild->left;
    grandchild->left = child;
    if (child->right != NULL)
        child->right->parent = child;
    child->parent = grandchild;

    parent->left = grandchild->right;
    grandchild->right = parent;
    parent->parent = grandchild;
    if (parent->left != NULL)
        parent->left->parent = parent;

    if (grandchild->balance < 0) {
        parent->balance = +1;
        child->balance = 0;
    } else if (grandchild->balance > 0) {
        parent->balance = 0;
        child->balance = -1;
    } else {
        parent->balance = child->balance = 0;
    }
    grandchild->balance = 0;

    return grandchild;
}

static AVLNode *avltree_insert(AVLTree *tree, const char *key, size_t key_len, void *value) {
    Binary b = {.data = (char *) key, .length = key_len};
    AVLNode *parent, *node, *inserted;
    AVLNode **nodeptr = avltree_find_helper(&tree->root, &parent, tree->compare, b);

    if (*nodeptr != NULL) {
        tree->deleter((*nodeptr)->value);
        (*nodeptr)->value = value;
        return *nodeptr;
    }

    inserted = node = *nodeptr = avltree_nodealloc(key, key_len);
    if (node == NULL)
        return NULL;

    tree->size += 1;
    node->value = value;
    node->parent = parent;

    for (; parent != NULL; parent = node->parent) {
        AVLNode *temp, *grandparent;

        if (node == parent->left) { /* Inserted as left child */
            if (parent->balance < 0) { /* Already a left-heavy tree */
                grandparent = parent->parent;
                if (node->balance > 0) /* Double rotation needed */
                    temp = avltree_rotate_leftright(parent, node);
                else
                    temp = avltree_rotate_right(parent, node);
            } else { /* Already a right-heavy or balanced tree */
                if (parent->balance > 0) {
                    parent->balance = 0;
                    break;
                }

                parent->balance = -1;
                node = parent; /* Skip node we just adjusted */
                continue;
            }
        } else { /* Inserted as right child */
            if (parent->balance > 0) { /* Already a right-heavy tree */
                grandparent = parent->parent;
                if (node->balance < 0) /* Double rotation needed */
                    temp = avltree_rotate_rightleft(parent, node);
                else
                    temp = avltree_rotate_left(parent, node);
            } else { /* Already a left-heavy or balanced tree */
                if (parent->balance < 0) {
                    parent->balance = 0;
                    break;
                }

                parent->balance = +1;
                node = parent; /* Skip node we just adjusted */
                continue;
            }
        }

        /* Could also be adapted in the double-rotation routines, but it saves code here */
        temp->parent = grandparent;
        if (grandparent != NULL) {
            if (grandparent->left == parent)
                grandparent->left = temp;
            else
                grandparent->right = temp;
        } else {
            tree->root = temp;
        }

        break;
    }

    return inserted;
}

/* Returns node after deleted node, or NULL if the node to be deleted has no successor */
static AVLNode *avltree_delete_node_helper(AVLTree *tree, AVLNode **node) {
    AVLNode *successor = avltree_inorder_next(*node);
    AVLNode save = **node, *node_replacement = NULL;

    if ((*node)->left == NULL && (*node)->right == NULL) {
        avltree_nodedestroy(*node, tree->deleter);

        node_replacement = NULL;
    } else if ((*node)->left == NULL) { /* Right child present */
        AVLNode *right = (*node)->right;
        right->parent = (*node)->parent;

        avltree_nodedestroy(*node, tree->deleter);

        node_replacement = right;
    } else if ((*node)->right == NULL) { /* Left child present */
        AVLNode *left = (*node)->left;
        left->parent = (*node)->parent;

        avltree_nodedestroy(*node, tree->deleter);

        node_replacement = left;
    } else { /* Both children present, successor will never be parent of node */
        /* This routine just swaps out the successor (which must have at most one child) with the node to be deleted
         * and queues the successor to be deleted instead */
        (*node)->value = successor->value;
        (*node)->key = successor->key;
        successor->value = save.value;
        successor->key = save.key;

        if (successor == successor->parent->left)
            node = &successor->parent->left;
        else
            node = &successor->parent->right;

        return avltree_delete_node_helper(tree, node);
    }

    AVLNode *n = *node, *parent = save.parent, *sibling = NULL, *grandparent = NULL, *left = NULL, *right = NULL;
    if (parent != NULL) {
        left = parent->left;
        right = parent->right;
    }
    *node = node_replacement;
    tree->size -= 1;

    int b = 0;

    for (; parent != NULL; left = grandparent != NULL? grandparent->left: NULL,
                           right = grandparent != NULL? grandparent->right: NULL, parent = grandparent) {
        grandparent = parent->parent;

        if (n == left) { /* Left child is shrinking */
            if (parent->balance > 0) { /* Weighted too far to the right, rebalance */
                sibling = parent->right;
                b = sibling->balance;
                if (b < 0) /* Sibling weighted to the left, double rotate */
                    n = avltree_rotate_rightleft(parent, sibling);
                else
                    n = avltree_rotate_left(parent, sibling);
            } else {
                if (parent->balance == 0) {
                    parent->balance = +1;
                    break;
                }

                n = parent;
                n->balance = 0;
                continue;
            }
        } else { /* Right child is shrinking */
            if (parent->balance < 0) { /* Weighted too far to the left, rebalance */
                sibling = parent->left;
                b = sibling->balance;
                if (b > 0) /* Sibling weighted to the right, double rotate */
                    n = avltree_rotate_leftright(parent, sibling);
                else
                    n = avltree_rotate_right(parent, sibling);
            } else {
                if (parent->balance == 0) {
                    parent->balance = -1;
                    break;
                }

                n = parent;
                n->balance = 0;
                continue;
            }
        }

        n->parent = grandparent;
        if (grandparent != NULL) {
            if (grandparent->left == parent)
                grandparent->left = n;
            else
                grandparent->right = n;
        } else {
            tree->root = n;
        }

        if (b == 0)
            break;
    }

    return successor;
}

static AVLNode *avltree_delete_node(AVLTree *tree, AVLNode *node) {
    if (node == NULL)
        return NULL;

    AVLNode **p = &tree->root;

    if (node->parent != NULL) {
        if (node->parent->left == node)
            p = &node->parent->left;
        else
            p = &node->parent->right;
    }

    return avltree_delete_node_helper(tree, p);
}

static AVLNode *avltree_delete(AVLTree *tree, const char *key, size_t key_len) {
    return avltree_delete_node(tree, avltree_find(tree, key, key_len));
}

static void avltree_print_helper(AVLNode *node) {
    if (node == NULL)
        return;

    printf("Item: %s\n", node->key.data);
    printf(" Balance: %+d\n", node->balance);
    printf(" Parent: %s\n", node->parent? node->parent->key.data: "<not present>");
    printf(" Left child: %-20s  Right child: %s\n", node->left? node->left->key.data: "<not present>",
                                                    node->right? node->right->key.data: "<not present>");
    printf("\n");

    avltree_print_helper(node->left);
    avltree_print_helper(node->right);
}

void avltree_print(AVLTree *tree) {
    avltree_print_helper(tree->root);
}

static void avltree_is_avl_helper(AVLNode *node, int level, int *shallowest_leaf, int *deepest_leaf) {
    if (node == NULL)
        return;

    if (node->left == NULL && node->right == NULL) { /* Leaf */
        if (*shallowest_leaf < 0 || level < *shallowest_leaf)
            *shallowest_leaf = level;

        if (*deepest_leaf < level)
            *deepest_leaf = level;
    }

    avltree_is_avl_helper(node->left, level+1, shallowest_leaf, deepest_leaf);
    avltree_is_avl_helper(node->right, level+1, shallowest_leaf, deepest_leaf);
}

int avltree_is_avl(AVLTree *tree) {
    int shallow = -1, deep = -1;

    avltree_is_avl_helper(tree->root, 0, &shallow, &deep);

    return abs(deep - shallow) <= 1;
}

static void avltree_clear(AVLTree *tree) {
    avltree_destroy_helper(tree->root, tree->deleter);
    tree->root = NULL;
    tree->size = 0;
}

static size_t avltree_size(AVLTree *tree) {
    return tree->size;
}

StringSet stringset_create() {
    return (StringSet) avltree_create();
}

StringSet stringset_create_custom(Compare compare) {
    StringSet set = stringset_create();
    if (set)
        avltree_set_compare_fn((AVLTree *) set, compare);

    return set;
}

StringSet stringset_from_stringlist(StringList list, Compare compare) {
    StringSet set = stringset_create();

    if (set) {
        stringset_set_compare_fn(set, compare);

        const char **array = stringlist_array(list);
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
    return (StringSet) avltree_copy((AVLTree *) other);
}

int stringset_add(StringSet set, const char *item) {
    if (avltree_insert((AVLTree *) set, item, strlen(item), NULL) == NULL)
        return CC_ENOMEM;

    return 0;
}

int stringset_add_n(StringSet set, const char *item, size_t item_len) {
    if (memchr(item, 0, item_len) != NULL)
        return CC_EINVAL;

    if (avltree_insert((AVLTree *) set, item, item_len, NULL) != NULL)
        return CC_ENOMEM;

    return 0;
}

int stringset_contains(StringSet set, const char *item) {
    return avltree_find((AVLTree *) set, item, strlen(item)) != NULL;
}

void stringset_remove(StringSet set, const char *item) {
    avltree_delete((AVLTree *) set, item, strlen(item));
}

Iterator stringset_begin(StringSet set) {
    return avltree_min_node((AVLTree *) set);
}

Iterator stringset_next(StringSet set, Iterator it) {
    UNUSED(set)

    if (it == NULL)
        return NULL;

    return avltree_inorder_next((AVLNode *) it);
}

const char *stringset_value_of(StringSet set, Iterator it) {
    UNUSED(set)

    return ((AVLNode *) it)->key.data;
}

size_t stringset_size(StringSet set) {
    return avltree_size((AVLTree *) set);
}

Compare stringset_get_compare_fn(StringSet set) {
    return avltree_get_compare_fn((AVLTree *) set);
}

void stringset_set_compare_fn(StringSet set, Compare compare) {
    avltree_set_compare_fn((AVLTree *) set, compare);
}

void stringset_clear(StringSet set) {
    avltree_clear((AVLTree *) set);
}

void stringset_destroy(StringSet set) {
    avltree_destroy((AVLTree *) set);
}

struct GenericListStruct {
    void **array;
    Compare compare;
    Copier copy;
    Deleter deleter;
    size_t array_size;
    size_t array_capacity;
};

struct StringListStruct {
    char **array;
    Compare compare;
    size_t array_size;
    size_t array_capacity;
};

struct BinaryListStruct {
    Binary *array;
    Compare compare;
    size_t array_size;
    size_t array_capacity;
};

static void generic_deleter(void *p) {
    FREE(p);
}

GenericList genericlist_create(Compare compare, Copier copy, Deleter deleter) {
    return genericlist_create_reserve(0, compare, copy, deleter);
}

GenericList genericlist_create_reserve(size_t reserve, Compare compare, Copier copy, Deleter deleter) {
    if (compare == NULL || copy == NULL)
        return NULL;

    GenericList list = CALLOC(1, sizeof(*list));
    if (!list)
        return NULL;

    list->array_capacity = MAX(8, reserve);
    list->array = MALLOC(list->array_capacity * sizeof(*list->array));
    list->array[0] = NULL;
    list->compare = compare;
    list->copy = copy;
    list->deleter = deleter? deleter: generic_deleter;

    return list;
}

GenericList genericlist_copy(GenericList other) {
    void **array = genericlist_array(other);
    GenericList list = genericlist_create(other->compare, other->copy, other->deleter);
    if (!list)
        return NULL;

    genericlist_reserve(list, genericlist_size(other));

    for (size_t i = 0; i < genericlist_size(other); ++i)
        if (genericlist_append_copy(list, array[i])) {
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
        if (genericlist_append_copy(result, array[i])) {
            genericlist_destroy(result);
            return NULL;
        }

    array = genericlist_array(right);
    for (size_t i = 0; i < genericlist_size(right); ++i)
        if (genericlist_append_copy(result, array[i])) {
            genericlist_destroy(result);
            return NULL;
        }

    return result;
}

static int genericlist_grow(GenericList list, size_t added) {
    if (list->array_size+added >= list->array_capacity) {
        size_t new_capacity = MAX(list->array_capacity + (list->array_capacity / 2), list->array_capacity) + added + 1;
        size_t new_size = safe_multiply(new_capacity, sizeof(*list->array));
        if (!new_size)
            return CC_ENOMEM;

        void **new_array = REALLOC(list->array, new_size);
        if (!new_array)
            return CC_ENOMEM;

        list->array_capacity = new_capacity;
        list->array = new_array;
    }

    return 0;
}

int genericlist_append_actual(GenericList list, void *item) {
    int error = genericlist_grow(list, 1);
    if (error)
        return error;

    list->array[list->array_size++] = item;
    list->array[list->array_size] = NULL;
    return 0;
}

int genericlist_append_copy(GenericList list, const void *item) {
    void *duplicate = list->copy(item);
    if (!duplicate)
        return CC_ENOMEM;

    int err = genericlist_append_actual(list, duplicate);
    if (err)
        list->deleter(duplicate);

    return err;
}

int genericlist_insert_actual(GenericList list, void *item, size_t before_index) {
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

int genericlist_insert_copy(GenericList list, const void *item, size_t before_index) {
    void *duplicate = list->copy(item);
    if (!duplicate)
        return CC_ENOMEM;

    int err = genericlist_insert_actual(list, duplicate, before_index);
    if (err)
        list->deleter(duplicate);

    return err;
}

int genericlist_replace_actual_at(GenericList list, size_t index, void *item) {
    if (index >= list->array_size)
        return CC_EINVAL;

    list->deleter(list->array[index]);
    list->array[index] = item;
    return 0;
}

int genericlist_replace_copy_at(GenericList list, size_t index, const void *item) {
    void *duplicate = list->copy(item);
    if (!duplicate)
        return CC_ENOMEM;

    int err = genericlist_replace_actual_at(list, index, duplicate);
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

    return length;
}

void **genericlist_array(GenericList list) {
    return list->array;
}

int genericlist_contains(GenericList list, const void *item) {
    return genericlist_find(list, item, 0) != SIZE_MAX;
}

size_t genericlist_find(GenericList list, const void *item, size_t begin_index) {
    for (size_t i = begin_index; i < genericlist_size(list); ++i) {
        if (list->compare(genericlist_array(list)[i], item) == 0)
            return i;
    }

    return SIZE_MAX;
}

size_t genericlist_rfind(GenericList list, const void *item, size_t begin_index) {
    if (begin_index >= genericlist_size(list))
        begin_index = genericlist_size(list)-1;

    for (size_t i = begin_index; i != SIZE_MAX; --i) {
        if (list->compare(genericlist_array(list)[i], item) == 0)
            return i;
    }

    return SIZE_MAX;
}

int genericlist_compare(GenericList list, GenericList other) {
    size_t max = MIN(genericlist_size(list), genericlist_size(other));

    for (size_t i = 0; i < max; ++i) {
        int cmp = list->compare(genericlist_array(list)[i], genericlist_array(other)[i]);

        if (cmp)
            return cmp;
    }

    if (genericlist_size(other) != max)
        return -1;
    else if (genericlist_size(list) != max)
        return 1;

    return 0;
}

/* descending must be -1 (descending) or 1 (ascending) */
static void genericlist_insertion_sort(void **base, size_t num, int descending, int (*compar)(const void *, const void *)) {
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

/* descending must be -1 (descending) or 1 (ascending) */
static void genericlist_merge_helper(void **result, void **base, size_t begin, size_t pivot, size_t end, int descending, int (*compar)(const void *, const void *)) {
    size_t left = begin, right = pivot;

    for (size_t i = begin; i < end; ++i) {
        if (left < pivot && (right >= end || compar(base[left], base[right]) * descending <= 0))
            result[i] = base[left++];
        else
            result[i] = base[right++];
    }
}

static void genericlist_merge_sort_helper(void **result, void **base, size_t begin, size_t end, int descending, int (*compar)(const void *, const void *)) {
    if (end - begin <= 4) {
        genericlist_insertion_sort(base + begin, end - begin, descending, compar);
        return;
    }

    size_t pivot = begin + (end - begin) / 2;

    genericlist_merge_sort_helper(base, result, begin, pivot, descending, compar);
    genericlist_merge_sort_helper(base, result, pivot, end, descending, compar);
    genericlist_merge_helper(base, result, begin, pivot, end, descending, compar);
}

GenericList genericlist_stable_sorted(GenericList list, int descending) {
    GenericList copy = genericlist_copy(list);
    if (copy == NULL || genericlist_stable_sort(copy, descending)) {
        genericlist_destroy(copy);
        return NULL;
    }

    return copy;
}

int genericlist_stable_sort(GenericList list, int descending) {
    void **temp = MALLOC(genericlist_size(list) * sizeof(*temp));
    if (temp == NULL) {
        FREE(temp);
        return CC_ENOMEM;
    }

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

size_t genericlist_size(GenericList list) {
    return list->array_size;
}

Compare genericlist_get_compare_fn(GenericList list) {
    return list->compare;
}

void genericlist_set_compare_fn(GenericList list, Compare compare) {
    if (compare != NULL)
        list->compare = compare;
}

Copier genericlist_get_copier_fn(GenericList list) {
    return list->copy;
}

void genericlist_set_copier_fn(GenericList list, Copier copier) {
    if (copier != NULL)
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

StringList stringlist_create() {
    return (StringList) genericlist_create((Compare) strcmp, (Copier) strdup, (Deleter) FREE);
}

StringList stringlist_create_custom(Compare compare) {
    StringList list = stringlist_create();
    if (list && compare)
        list->compare = compare;

    return list;
}

StringList stringlist_split(const char *string, const char *separator, int keep_empty) {
    int done = 0;
    size_t separatorLen = strlen(separator);
    StringList list = stringlist_create();
    if (!list)
        return NULL;

    if (!separatorLen) { /* Empty separator, separate every character */
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

        string = separator_location + separatorLen;
    }

    return list;

cleanup:
    stringlist_destroy(list);
    return NULL;
}

StringList stringlist_copy(StringList other) {
    return (StringList) genericlist_copy((GenericList) other);
}

StringList stringlist_from_stringset(StringSet other) {
    StringList list = stringlist_create();
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

StringList stringlist_from_binarylist(BinaryList other) {
    Binary *array = binarylist_array(other);
    StringList list = stringlist_create();
    if (!list)
        return NULL;

    for (size_t i = 0; i < binarylist_size(other); ++i) {
        if (memchr(array[i].data, 0, array[i].length) != NULL || stringlist_append(list, array[i].data))
            goto cleanup;
    }

    return list;

cleanup:
    stringlist_destroy(list);
    return NULL;
}

int stringlist_append(StringList list, const char *item) {
    return genericlist_append_copy((GenericList) list, item);
}

int stringlist_append_n(StringList list, const char *item, size_t item_len) {
    if (memchr(item, 0, item_len) != NULL)
        return CC_EINVAL;

    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    /* Copy string */
    memcpy(duplicate, item, item_len);
    duplicate[item_len] = 0;

    int err = genericlist_append_actual((GenericList) list, duplicate);
    if (err)
        FREE(duplicate);

    return err;
}

int stringlist_insert(StringList list, const char *item, size_t before_index) {
    return genericlist_insert_copy((GenericList) list, item, before_index);
}

int stringlist_insert_n(StringList list, const char *item, size_t item_len, size_t before_index) {
    if (memchr(item, 0, item_len) != NULL)
        return CC_EINVAL;

    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    /* Copy string */
    memcpy(duplicate, item, item_len);
    duplicate[item_len] = 0;

    int err = genericlist_insert_actual((GenericList) list, duplicate, before_index);
    if (err)
        FREE(duplicate);

    return err;
}

int stringlist_replace_at(StringList list, size_t index, const char *item) {
    return genericlist_replace_copy_at((GenericList) list, index, item);
}

int stringlist_replace_at_n(StringList list, size_t index, const char *item, size_t item_len) {
    if (memchr(item, 0, item_len) != NULL)
        return CC_EINVAL;

    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    /* Copy string */
    memcpy(duplicate, item, item_len);
    duplicate[item_len] = 0;

    int err = genericlist_replace_actual_at((GenericList) list, index, duplicate);
    if (err)
        FREE(duplicate);

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
    return (char **) genericlist_array((GenericList) list);
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

char *stringlist_joined_alloc(StringList list, const char *separator) {
    return strjoin_alloc((const char **) stringlist_array(list), stringlist_size(list), separator);
}

size_t stringlist_size(StringList list) {
    return genericlist_size((GenericList) list);
}

Compare stringlist_get_compare_fn(StringList list) {
    return genericlist_get_compare_fn((GenericList) list);
}

void stringlist_set_compare_fn(StringList list, Compare compare) {
    genericlist_set_compare_fn((GenericList) list, compare);
}

void stringlist_clear(StringList list) {
    genericlist_clear((GenericList) list);
}

void stringlist_destroy(StringList list) {
    genericlist_destroy((GenericList) list);
}

BinaryList binarylist_create() {
    BinaryList list = CALLOC(1, sizeof(*list));
    if (!list)
        return NULL;

    list->array_capacity = 8;
    list->array = MALLOC(list->array_capacity * sizeof(*list->array));
    list->array[0].data = NULL;
    list->array[0].length = 0;
    list->compare = (Compare) binary_compare;

    return list;
}

BinaryList binarylist_create_custom(Compare compare) {
    BinaryList list = binarylist_create();
    if (list)
        list->compare = compare;

    return list;
}

BinaryList binarylist_split(const char *string, const char *separator, int keep_empty) {
    int done = 0;
    size_t separatorLen = strlen(separator);
    BinaryList list = binarylist_create();
    if (!list)
        return NULL;

    if (!separatorLen) { /* Empty separator, separate every character */
        while (*string) {
            if (binarylist_append(list, string, 1))
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
            if (binarylist_append(list, string, separator_location - string))
                goto cleanup;
        }

        string = separator_location + separatorLen;
    }

    return list;

cleanup:
    binarylist_destroy(list);
    return NULL;
}

BinaryList binarylist_copy(BinaryList other) {
    Binary *array = binarylist_array(other);
    BinaryList list = binarylist_create();
    if (!list)
        return NULL;

    list->compare = other->compare;

    for (size_t i = 0; i < binarylist_size(other); ++i)
        if (binarylist_append(list, array[i].data, array[i].length)) {
            binarylist_destroy(list);
            return NULL;
        }

    return list;
}

BinaryList binarylist_from_stringlist(StringList other) {
    char **array = stringlist_array(other);
    BinaryList list = binarylist_create();
    if (!list)
        return NULL;

    for (size_t i = 0; i < stringlist_size(other); ++i) {
        if (binarylist_append(list, array[i], strlen(array[i])))
            goto cleanup;
    }

    return list;

cleanup:
    binarylist_destroy(list);
    return NULL;
}

static int binarylist_grow(BinaryList list) {
    if (list->array_size+1 == list->array_capacity) {
        size_t new_capacity = MAX(list->array_capacity + (list->array_capacity / 2), list->array_capacity) + 1;
        size_t new_size = safe_multiply(new_capacity, sizeof(*list->array));
        if (!new_size)
            return CC_ENOMEM;

        Binary *new_array = REALLOC(list->array, new_size);
        if (!new_array)
            return CC_ENOMEM;

        list->array_capacity = new_capacity;
        list->array = new_array;
    }

    return 0;
}

int binarylist_append(BinaryList list, const char *item, size_t item_len) {
    int error = binarylist_grow(list);
    if (error)
        return error;

    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    /* Copy string */
    memcpy(duplicate, item, item_len);
    duplicate[item_len] = 0;

    /* Insert into list */
    list->array[list->array_size].data = duplicate;
    list->array[list->array_size].length = item_len;
    ++list->array_size;
    list->array[list->array_size].data = NULL;
    list->array[list->array_size].length = 0;
    return 0;
}

int binarylist_insert(BinaryList list, const char *item, size_t item_len, size_t before_index) {
    if (before_index > binarylist_size(list))
        before_index = binarylist_size(list);

    int error = binarylist_grow(list);
    if (error)
        return error;

    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    /* Copy string */
    memcpy(duplicate, item, item_len);
    duplicate[item_len] = 0;

    /* Make space in list */
    memmove(list->array + before_index + 1, list->array + before_index, (binarylist_size(list) - before_index) * sizeof(*list->array));

    /* Insert into list */
    list->array[before_index].data = duplicate;
    list->array[before_index].length = item_len;
    list->array[list->array_size].data = NULL;
    list->array[list->array_size].length = 0;
    return 0;
}

int binarylist_replace_at(BinaryList list, size_t index, const char *item, size_t item_len) {
    if (index >= list->array_size)
        return CC_EINVAL;

    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    /* Copy string */
    memcpy(duplicate, item, item_len);
    duplicate[item_len] = 0;

    FREE(list->array[index].data);
    list->array[index].data = duplicate;
    list->array[index].length = item_len;
    return 0;
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

size_t binarylist_size(BinaryList list) {
    return list->array_size;
}

Compare binarylist_get_compare_fn(BinaryList list) {
    return list->compare;
}

void binarylist_set_compare_fn(BinaryList list, Compare compare) {
    list->compare = compare? compare: (Compare) binary_compare;
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
