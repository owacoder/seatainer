/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "avl.h"

typedef struct AVLNode {
    struct AVLNode *left, *right, *parent;
    void *key;
    void *value;
    int balance;
} AVLNode;

typedef struct AVLTree {
    CommonContainerBase *key_base, *value_base;
    struct AVLNode *root;
    size_t size;
} AVLTree;

void *avltree_non_copier(const void *p) {
    UNUSED(p);
    return NULL;
}

void avltree_non_deleter(void *p) {
    UNUSED(p)
}

static void avltree_nodedestroy(const AVLTree *tree, AVLNode *node) {
    if (tree->key_base->deleter) tree->key_base->deleter(node->key);
    if (tree->value_base->deleter) tree->value_base->deleter(node->value);
    FREE(node);
}

const void *avlnode_key(AVLNode *node) {
    return node->key;
}

void **avlnode_value(AVLNode *node) {
    return &node->value;
}

Compare avltree_get_key_compare_fn(AVLTree *tree) {
    return tree->key_base->compare;
}

int avltree_set_key_compare_fn(AVLTree *tree, Compare compare) {
    if (compare == NULL || tree->size)
        return CC_EINVAL;

    CommonContainerBase *base = container_base_copy_if_static(tree->key_base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->compare = compare;
    tree->key_base = base;

    return 0;
}

Compare avltree_get_value_compare_fn(AVLTree *tree) {
    return tree->value_base->compare;
}

int avltree_set_value_compare_fn(AVLTree *tree, Compare compare) {
    CommonContainerBase *base = container_base_copy_if_static(tree->value_base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->compare = compare;
    tree->value_base = base;

    return 0;
}

Copier avltree_get_key_copier_fn(AVLTree *tree) {
    return tree->key_base->copier;
}

int avltree_set_key_copier_fn(AVLTree *tree, Copier copier) {
    CommonContainerBase *base = container_base_copy_if_static(tree->key_base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->copier = copier;
    tree->key_base = base;

    return 0;
}

Copier avltree_get_value_copier_fn(AVLTree *tree) {
    return tree->value_base->copier;
}

int avltree_set_value_copier_fn(AVLTree *tree, Copier copier) {
    CommonContainerBase *base = container_base_copy_if_static(tree->value_base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->copier = copier? copier: avltree_non_copier;
    tree->value_base = base;

    return 0;
}

Deleter avltree_get_key_deleter_fn(AVLTree *tree) {
    return tree->key_base->deleter;
}

int avltree_set_key_deleter_fn(AVLTree *tree, Deleter deleter) {
    CommonContainerBase *base = container_base_copy_if_static(tree->key_base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->deleter = deleter;
    tree->key_base = base;

    return 0;
}

Deleter avltree_get_value_deleter_fn(AVLTree *tree) {
    return tree->value_base->deleter;
}

int avltree_set_value_deleter_fn(AVLTree *tree, Deleter deleter) {
    CommonContainerBase *base = container_base_copy_if_static(tree->value_base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->deleter = deleter? deleter: avltree_non_deleter;
    tree->value_base = base;

    return 0;
}

Parser avltree_get_key_parser_fn(AVLTree *tree) {
    return tree->key_base->parse;
}

int avltree_set_key_parser_fn(AVLTree *tree, Parser parser) {
    CommonContainerBase *base = container_base_copy_if_static(tree->key_base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->parse = parser;
    tree->key_base = base;

    return 0;
}

Parser avltree_get_value_parser_fn(AVLTree *tree) {
    return tree->value_base->parse;
}

int avltree_set_value_parser_fn(AVLTree *tree, Parser parser) {
    CommonContainerBase *base = container_base_copy_if_static(tree->value_base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->parse = parser;
    tree->value_base = base;

    return 0;
}

Serializer avltree_get_key_serializer_fn(AVLTree *tree) {
    return tree->key_base->serialize;
}

int avltree_set_key_serializer_fn(AVLTree *tree, Serializer serializer) {
    CommonContainerBase *base = container_base_copy_if_static(tree->key_base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->serialize = serializer;
    tree->key_base = base;

    return 0;
}

Serializer avltree_get_value_serializer_fn(AVLTree *tree) {
    return tree->value_base->serialize;
}

int avltree_set_value_serializer_fn(AVLTree *tree, Serializer serializer) {
    CommonContainerBase *base = container_base_copy_if_static(tree->value_base, 1);
    if (base == NULL)
        return CC_ENOMEM;

    base->serialize = serializer;
    tree->value_base = base;

    return 0;
}

AVLTree *avltree_create(const CommonContainerBase *key_base, const CommonContainerBase *value_base) {
    if (key_base == NULL || value_base == NULL)
        return NULL;

    AVLTree *tree = CALLOC(1, sizeof(*tree));
    if (tree == NULL)
        goto cleanup;

    tree->key_base = container_base_copy_if_dynamic((CommonContainerBase *) key_base);
    if (tree->key_base == NULL)
        goto cleanup;

    tree->value_base = container_base_copy_if_dynamic((CommonContainerBase *) value_base);
    if (tree->value_base == NULL)
        goto cleanup;

    return tree;

cleanup:
    if (tree) {
        container_base_destroy_if_dynamic(tree->key_base);
        container_base_destroy_if_dynamic(tree->value_base);
    }
    FREE(tree);

    return NULL;
}

void avltree_destroy_helper(const AVLTree *tree, AVLNode *node) {
    if (node == NULL)
        return;

    avltree_destroy_helper(tree, node->left);
    avltree_destroy_helper(tree, node->right);
    avltree_nodedestroy(tree, node);
}

void avltree_destroy(AVLTree *tree) {
    if (tree != NULL) {
        avltree_destroy_helper(tree, tree->root);

        container_base_destroy_if_dynamic(tree->key_base);
        container_base_destroy_if_dynamic(tree->value_base);
    }

    FREE(tree);
}

static AVLNode *avltree_copy_helper(const AVLTree *tree, AVLNode *node, AVLNode *parent) {
    if (node == NULL)
        return NULL;

    AVLNode *new_node = CALLOC(1, sizeof(*new_node));
    if (new_node == NULL)
        return NULL;

    new_node->parent = parent;
    new_node->key = tree->key_base->copier(node->key);
    new_node->value = tree->value_base->copier(node->value);
    new_node->left = avltree_copy_helper(tree, node->left, new_node);
    new_node->right = avltree_copy_helper(tree, node->right, new_node);

    if ((new_node->key == NULL && node->key != NULL) ||
        (new_node->value == NULL && node->value != NULL) ||
        (new_node->left == NULL && node->left != NULL) ||
        (new_node->right == NULL && node->right != NULL)) {
        avltree_destroy_helper(tree, new_node);
        return NULL;
    }

    return new_node;
}

AVLTree *avltree_copy(AVLTree *other) {
    if (other == NULL || other->key_base->copier == NULL || other->value_base->copier == NULL)
        return NULL;

    AVLTree *tree = avltree_create(other->key_base, other->value_base);
    if (tree == NULL)
        return NULL;

    tree->size = other->size;
    tree->root = avltree_copy_helper(other, other->root, NULL);

    if (tree->root == NULL && other->root != NULL) {
        avltree_destroy(tree);
        return NULL;
    }

    return tree;
}

static AVLNode **avltree_find_helper(const AVLTree *tree, AVLNode **node, AVLNode **parent, const void *item_key) {
    *parent = NULL;

    while (1) {
        if (*node == NULL)
            return node;

        int cmp = tree->key_base->compare(item_key, (*node)->key);

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

AVLNode *avltree_min_node(AVLTree *tree) {
    AVLNode *node = tree->root;

    if (node == NULL)
        return NULL;

    while (node->left != NULL) {
        node = node->left;
    }

    return node;
}

AVLNode *avltree_max_node(AVLTree *tree) {
    AVLNode *node = tree->root;

    if (node == NULL)
        return NULL;

    while (node->right != NULL) {
        node = node->right;
    }

    return node;
}

AVLNode *avltree_inorder_previous(AVLNode *node) {
    AVLNode *last = NULL;

    if (node == NULL)
        return NULL;

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

AVLNode *avltree_inorder_next(AVLNode *node) {
    AVLNode *last = NULL;

    if (node == NULL)
        return NULL;

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

AVLNode *avltree_find(AVLTree *tree, const void *key) {
    AVLNode *parent;

    return *avltree_find_helper(tree, &tree->root, &parent, key);
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

AVLNode *avltree_insert_move_key(AVLTree *tree, void *key, void *value) {
    AVLNode *parent, *node, *inserted;
    AVLNode **nodeptr = avltree_find_helper(tree, &tree->root, &parent, key);

    if (*nodeptr != NULL) { /* Key already exists in tree */
        if (tree->value_base->deleter)
            tree->value_base->deleter((*nodeptr)->value);

        (*nodeptr)->value = value;
        return *nodeptr;
    }

    /* Key does not exist in tree yet */

    inserted = node = *nodeptr = CALLOC(1, sizeof(*node));
    if (node == NULL)
        return NULL;

    tree->size += 1;
    node->key = key;
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

AVLNode *avltree_insert_copy_key(AVLTree *tree, const void *key, void *value) {
    if (tree->key_base->copier == NULL)
        return NULL;

    void *new_key = tree->key_base->copier(key);
    if (new_key == NULL)
        return NULL;

    return avltree_insert_move_key(tree, new_key, value);
}

/* Returns node after deleted node, or NULL if the node to be deleted has no successor */
static AVLNode *avltree_delete_node_helper(AVLTree *tree, AVLNode **node) {
    AVLNode *successor = avltree_inorder_next(*node);
    AVLNode save = **node, *node_replacement = NULL;

    if ((*node)->left == NULL && (*node)->right == NULL) {
        avltree_nodedestroy(tree, *node);

        node_replacement = NULL;
    } else if ((*node)->left == NULL) { /* Right child present */
        AVLNode *right = (*node)->right;
        right->parent = (*node)->parent;

        avltree_nodedestroy(tree, *node);

        node_replacement = right;
    } else if ((*node)->right == NULL) { /* Left child present */
        AVLNode *left = (*node)->left;
        left->parent = (*node)->parent;

        avltree_nodedestroy(tree, *node);

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

    AVLNode *n = *node, *parent = save.parent, *sibling = NULL, *grandparent = NULL, *left = NULL;
    if (parent != NULL)
        left = parent->left;
    *node = node_replacement;
    tree->size -= 1;

    int b = 0;

    for (; parent != NULL; left = grandparent != NULL? grandparent->left: NULL, parent = grandparent) {
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

AVLNode *avltree_delete_node(AVLTree *tree, AVLNode *node) {
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

AVLNode *avltree_delete(AVLTree *tree, const void *key) {
    return avltree_delete_node(tree, avltree_find(tree, key));
}

#if 0
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
#endif

int avltree_compare(AVLTree *left, AVLTree *right) {
    int cmp = generic_types_compatible_compare(left->key_base, right->key_base);
    if (cmp)
        return cmp;

    cmp = generic_types_compatible_compare(left->value_base, right->value_base);
    if (cmp)
        return cmp;

    AVLNode *lhs = avltree_min_node(left);
    AVLNode *rhs = avltree_min_node(right);

    while (lhs && rhs) {
        cmp = left->key_base->compare(lhs->key, rhs->key);
        if (cmp)
            return cmp;

        if (left->value_base->compare) {
            cmp = left->value_base->compare(lhs->value, rhs->value);
            if (cmp)
                return cmp;
        }

        lhs = avltree_inorder_next(lhs);
        rhs = avltree_inorder_next(rhs);
    }

    if (lhs)
        return 1;
    else if (rhs)
        return -1;
    else
        return 0;
}

void avltree_clear(AVLTree *tree) {
    avltree_destroy_helper(tree, tree->root);
    tree->root = NULL;
    tree->size = 0;
}

size_t avltree_size(AVLTree *tree) {
    return tree->size;
}

const CommonContainerBase *avltree_get_key_container_base(const AVLTree *tree) {
    return tree->key_base;
}

const CommonContainerBase *avltree_get_value_container_base(const AVLTree *tree) {
    return tree->value_base;
}
