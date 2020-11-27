/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "binarytree.h"

struct BinaryTreeStruct {
    struct BinaryTreeStruct *parent, *left, *right, *extra;
    void *data;
};
