/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "avl.h"

typedef struct AVLNode {
    struct AVLNode *left, *right, *parent;
    Binary key;
    void *value;
    int balance;
} AVLNode;

typedef struct AVLTree {
    CommonContainerBase base;
    struct AVLNode *root;
    BinaryCompare key_compare;
    Compare value_compare;
    Copier copy;
    Deleter deleter;
    size_t size;
} AVLTree;

void *avltree_copier(const void *p) {
    return (void *) p;
}

void avltree_deleter(void *p) {
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

Binary avlnode_key(AVLNode *node) {
    return node->key;
}

void **avlnode_value(AVLNode *node) {
    return &node->value;
}

BinaryCompare avltree_get_key_compare_fn(AVLTree *tree) {
    return tree->key_compare;
}

void avltree_set_key_compare_fn(AVLTree *tree, BinaryCompare compare) {
    tree->key_compare = compare? compare: binary_compare;
}

Compare avltree_get_value_compare_fn(AVLTree *tree) {
    return tree->value_compare;
}

void avltree_set_value_compare_fn(AVLTree *tree, Compare compare) {
    tree->value_compare = compare;
}

Copier avltree_get_copier_fn(AVLTree *tree) {
    return tree->copy;
}

void avltree_set_copier_fn(AVLTree *tree, Copier copier) {
    tree->copy = copier;
}

Deleter avltree_get_deleter_fn(AVLTree *tree) {
    return tree->deleter;
}

void avltree_set_deleter_fn(AVLTree *tree, Deleter deleter) {
    tree->deleter = deleter? deleter: avltree_deleter;
}

AVLTree *avltree_create() {
    AVLTree *tree = CALLOC(1, sizeof(*tree));
    if (tree == NULL)
        return NULL;

    tree->key_compare = binary_compare;
    tree->value_compare = NULL;
    tree->copy = avltree_copier;
    tree->deleter = avltree_deleter;

    return tree;
}

void avltree_destroy_helper(AVLNode *node, Deleter deleter) {
    if (node == NULL)
        return;

    avltree_destroy_helper(node->left, deleter);
    avltree_destroy_helper(node->right, deleter);
    avltree_nodedestroy(node, deleter);
}

void avltree_destroy(AVLTree *tree) {
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

AVLTree *avltree_copy(AVLTree *other) {
    if (other->copy == NULL)
        return NULL;

    AVLTree *tree = avltree_create();
    if (tree == NULL)
        return NULL;

    tree->key_compare = other->key_compare;
    tree->value_compare = other->value_compare;
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

static AVLNode **avltree_find_helper(AVLNode **node, AVLNode **parent, BinaryCompare compare, Binary item) {
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

AVLNode *avltree_find(AVLTree *tree, const char *key, size_t key_len) {
    Binary b = {.data = (char *) key, .length = key_len};
    AVLNode *parent;

    return *avltree_find_helper(&tree->root, &parent, tree->key_compare, b);
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

AVLNode *avltree_insert(AVLTree *tree, const char *key, size_t key_len, void *value) {
    Binary b = {.data = (char *) key, .length = key_len};
    AVLNode *parent, *node, *inserted;
    AVLNode **nodeptr = avltree_find_helper(&tree->root, &parent, tree->key_compare, b);

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

AVLNode *avltree_delete(AVLTree *tree, const char *key, size_t key_len) {
    return avltree_delete_node(tree, avltree_find(tree, key, key_len));
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
    AVLNode *lhs = avltree_min_node(left);
    AVLNode *rhs = avltree_min_node(right);

    while (lhs && rhs) {
        int cmp = left->key_compare(&lhs->key, &rhs->key);
        if (cmp)
            return cmp;

        if (left->value_compare) {
            cmp = left->value_compare(lhs->value, rhs->value);
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
    avltree_destroy_helper(tree->root, tree->deleter);
    tree->root = NULL;
    tree->size = 0;
}

size_t avltree_size(AVLTree *tree) {
    return tree->size;
}

CommonContainerBase *avltree_get_container_base(AVLTree *tree) {
    return &tree->base;
}
