/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef AVL_H
#define AVL_H

#include "../common.h"

struct AVLNode;
struct AVLTree;

void *avltree_copier(const void *p);
void avltree_deleter(void *p);

Binary avlnode_key(struct AVLNode *node);
void **avlnode_value(struct AVLNode *node);

BinaryCompare avltree_get_key_compare_fn(struct AVLTree *tree);
void avltree_set_key_compare_fn(struct AVLTree *tree, BinaryCompare compare);
Compare avltree_get_value_compare_fn(struct AVLTree *tree);
void avltree_set_value_compare_fn(struct AVLTree *tree, Compare compare);
Copier avltree_get_copier_fn(struct AVLTree *tree);
void avltree_set_copier_fn(struct AVLTree *tree, Copier copier);
Deleter avltree_get_deleter_fn(struct AVLTree *tree);
void avltree_set_deleter_fn(struct AVLTree *tree, Deleter deleter);
struct AVLTree *avltree_create();
void avltree_destroy_helper(struct AVLNode *node, Deleter deleter);
void avltree_destroy(struct AVLTree *tree);
struct AVLTree *avltree_copy(struct AVLTree *other);
struct AVLNode *avltree_min_node(struct AVLTree *tree);
struct AVLNode *avltree_max_node(struct AVLTree *tree);
struct AVLNode *avltree_inorder_previous(struct AVLNode *node);
struct AVLNode *avltree_inorder_next(struct AVLNode *node);
struct AVLNode *avltree_find(struct AVLTree *tree, const char *key, size_t key_len);
struct AVLNode *avltree_insert(struct AVLTree *tree, const char *key, size_t key_len, void *value);
/* Returns node after deleted node, or NULL if the node to be deleted has no successor */
struct AVLNode *avltree_delete_node(struct AVLTree *tree, struct AVLNode *node);
struct AVLNode *avltree_delete(struct AVLTree *tree, const char *key, size_t key_len);
int avltree_compare(struct AVLTree *left, struct AVLTree *right);
void avltree_clear(struct AVLTree *tree);
size_t avltree_size(struct AVLTree *tree);
CommonContainerBase *avltree_get_container_base(struct AVLTree *tree);

#endif /* AVL_H */
