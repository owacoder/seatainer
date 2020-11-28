/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef RECIPES_H
#define RECIPES_H

#include "common.h"

const CommonContainerBase *container_base_empty_recipe(void);
const CommonContainerBase *container_base_voidptr_recipe(void);
const CommonContainerBase *container_base_boolean_recipe(void);
const CommonContainerBase *container_base_char_recipe(void);
const CommonContainerBase *container_base_short_recipe(void);
const CommonContainerBase *container_base_int_recipe(void);
const CommonContainerBase *container_base_long_recipe(void);
const CommonContainerBase *container_base_long_long_recipe(void);
const CommonContainerBase *container_base_uchar_recipe(void);
const CommonContainerBase *container_base_ushort_recipe(void);
const CommonContainerBase *container_base_uint_recipe(void);
const CommonContainerBase *container_base_ulong_recipe(void);
const CommonContainerBase *container_base_ulong_long_recipe(void);
const CommonContainerBase *container_base_float_recipe(void);
const CommonContainerBase *container_base_double_recipe(void);
const CommonContainerBase *container_base_long_double_recipe(void);
const CommonContainerBase *container_base_cstring_recipe(void);
const CommonContainerBase *container_base_binary_recipe(void);
const CommonContainerBase *container_base_variant_recipe(void);

const CommonContainerBase *container_base_genericlist_recipe();
const CommonContainerBase *container_base_genericmap_recipe();
const CommonContainerBase *container_base_genericset_recipe();

const CommonContainerBase *container_base_stringlist_recipe();
const CommonContainerBase *container_base_stringmap_recipe();
const CommonContainerBase *container_base_stringset_recipe();

const CommonContainerBase *container_base_binarylist_recipe();

const CommonContainerBase *container_base_variantmap_recipe();
const CommonContainerBase *container_base_variantlist_recipe();
const CommonContainerBase *container_base_variantset_recipe();

#endif // RECIPES_H
