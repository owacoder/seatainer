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

const void *avlnode_key(struct AVLNode *node);
void **avlnode_value(struct AVLNode *node);

Compare avltree_get_key_compare_fn(struct AVLTree *tree);
Compare avltree_get_value_compare_fn(struct AVLTree *tree);
Copier avltree_get_key_copier_fn(struct AVLTree *tree);
Copier avltree_get_value_copier_fn(struct AVLTree *tree);
Deleter avltree_get_key_deleter_fn(struct AVLTree *tree);
Deleter avltree_get_value_deleter_fn(struct AVLTree *tree);
Parser avltree_get_key_parser_fn(struct AVLTree *tree);
Parser avltree_get_value_parser_fn(struct AVLTree *tree);
Serializer avltree_get_key_serializer_fn(struct AVLTree *tree);
Serializer avltree_get_value_serializer_fn(struct AVLTree *tree);
int avltree_set_key_compare_fn(struct AVLTree *tree, Compare compare);
int avltree_set_value_compare_fn(struct AVLTree *tree, Compare compare);
int avltree_set_key_copier_fn(struct AVLTree *tree, Copier copier);
int avltree_set_value_copier_fn(struct AVLTree *tree, Copier copier);
int avltree_set_key_deleter_fn(struct AVLTree *tree, Deleter deleter);
int avltree_set_value_deleter_fn(struct AVLTree *tree, Deleter deleter);
int avltree_set_key_parser_fn(struct AVLTree *tree, Parser parser);
int avltree_set_value_parser_fn(struct AVLTree *tree, Parser parser);
int avltree_set_key_serializer_fn(struct AVLTree *tree, Serializer serializer);
int avltree_set_value_serializer_fn(struct AVLTree *tree, Serializer serializer);

struct AVLTree *avltree_create(const CommonContainerBase *key_base, const CommonContainerBase *value_base);
void avltree_destroy_helper(const struct AVLTree *tree, struct AVLNode *node);
void avltree_destroy(struct AVLTree *tree);
struct AVLTree *avltree_copy(struct AVLTree *other);
struct AVLNode *avltree_min_node(struct AVLTree *tree);
struct AVLNode *avltree_max_node(struct AVLTree *tree);
struct AVLNode *avltree_inorder_previous(struct AVLNode *node);
struct AVLNode *avltree_inorder_next(struct AVLNode *node);
struct AVLNode *avltree_find(struct AVLTree *tree, const void *key);
struct AVLNode *avltree_insert_move_key(struct AVLTree *tree, void *key, void *value);
struct AVLNode *avltree_insert_copy_key(struct AVLTree *tree, const void *key, void *value);
/* Returns node after deleted node, or NULL if the node to be deleted has no successor */
struct AVLNode *avltree_delete_node(struct AVLTree *tree, struct AVLNode *node);
struct AVLNode *avltree_delete(struct AVLTree *tree, const void *key);
int avltree_compare(struct AVLTree *left, struct AVLTree *right);
void avltree_clear(struct AVLTree *tree);
size_t avltree_size(struct AVLTree *tree);
const CommonContainerBase *avltree_get_key_container_base(const struct AVLTree *tree);
const CommonContainerBase *avltree_get_value_container_base(const struct AVLTree *tree);

#endif /* AVL_H */
