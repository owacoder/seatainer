/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef GENERICTREE_H
#define GENERICTREE_H

struct TreeStruct;

typedef struct TreeStruct *Tree;

Tree tree_create();

void tree_destroy(Tree tree);

#endif // GENERICTREE_H
