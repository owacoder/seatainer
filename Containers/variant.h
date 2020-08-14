/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef VARIANT_H
#define VARIANT_H

#include "common.h"

enum VariantType {
    VariantNull,
    VariantBoolean,
    VariantInteger,
    VariantUnsignedInteger,
    VariantFloat,
    VariantString,
    VariantBinary,
    VariantCustom
};

Variant variant_create_null();
Variant variant_create_boolean(int b);
Variant variant_create_int(int value);
Variant variant_create_uint(unsigned int value);
Variant variant_create_int64(long long value);
Variant variant_create_uint64(unsigned long long value);
Variant variant_create_float(double value);
Variant variant_create_string(const char *value);
Variant variant_create_string_move(char *value);
/* If binary data is passed to a function that adopts the allocated data (e.g. the `move` functions), the user MUST ensure that data[length] == 0 */
Variant variant_create_binary_string(const char *value, size_t value_len);
Variant variant_create_binary_string_move(char *value, size_t value_len);
Variant variant_create_binary_string_binary(const Binary value);
Variant variant_create_binary_string_binary_move(Binary value);
Variant variant_create_custom(const void *item, Compare compare, Copier copier, Deleter deleter);
Variant variant_create_custom_move(void *item, Compare compare, Copier copier, Deleter deleter);
Variant variant_copy(Variant other);
int variant_compare(Variant lhs, Variant rhs);
int variant_is_null(Variant var);
int variant_is_boolean(Variant var);
int variant_is_int(Variant var);
int variant_is_uint(Variant var);
int variant_is_int64(Variant var);
int variant_is_uint64(Variant var);
int variant_is_float(Variant var);
int variant_is_integral(Variant var); /* Returns true if any integral type */
int variant_is_number(Variant var); /* Returns true if any integral type or float */
int variant_is_string(Variant var);
int variant_is_binary(Variant var);
int variant_is_custom(Variant var);
int variant_set_null(Variant var);
int variant_set_boolean(Variant var, int b);
int variant_set_int(Variant var, int value);
int variant_set_uint(Variant var, unsigned int value);
int variant_set_int64(Variant var, long long value);
int variant_set_uint64(Variant var, unsigned long long value);
int variant_set_float(Variant var, double value);
int variant_set_string(Variant var, const char *value);
int variant_set_string_move(Variant var, char *value);
int variant_set_binary_string(Variant var, const char *value, size_t value_len);
int variant_set_binary_string_move(Variant var, char *value, size_t value_len);
int variant_set_binary_string_binary(Variant var, const Binary value);
int variant_set_binary_string_binary_move(Variant var, Binary value);
int variant_set_custom_move(Variant var, void *item, Compare compare, Copier copier, Deleter deleter);
int variant_set_custom(Variant var, const void *item, Compare compare, Copier copier, Deleter deleter);
int variant_get_boolean(Variant var);
int variant_get_int(Variant var);
unsigned int variant_get_uint(Variant var);
long long variant_get_int64(Variant var);
unsigned long long variant_get_uint64(Variant var);
double variant_get_float(Variant var);
char *variant_get_string(Variant var);
Binary variant_get_binary(Variant var);
void *variant_get_custom(Variant var);
int variant_to_boolean(Variant var, int *error);
int variant_to_int(Variant var, int *error);
unsigned int variant_to_uint(Variant var, int *error);
long long variant_to_int64(Variant var, int *error);
unsigned long long variant_to_uint64(Variant var, int *error);
double variant_to_float(Variant var, int *error);
const char *variant_to_string(Variant var, int *error);
Binary variant_to_binary(Variant var, int *error);
enum VariantType variant_get_type(Variant var);
int variants_are_equal_types(Variant a, Variant b);
Compare variant_get_compare_fn(Variant var);
Copier variant_get_copier_fn(Variant var);
Deleter variant_get_deleter_fn(Variant var);
void variant_clear(Variant var);
void variant_destroy(Variant var);

#endif // VARIANT_H
