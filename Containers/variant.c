/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "variant.h"
#include "../utility.h"
#include "recipes.h"

#include <float.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>

struct VariantStructCustomHelper {
    CommonContainerBase *base;
    void *data;
};

struct VariantStructAtomHelper {
    union {
        int boolean;
        long long integer;
        unsigned long long unsigned_integer;
        double floating;
    } d;
    Binary string;
};

struct VariantStruct {
    union {
        struct VariantStructCustomHelper custom;
        struct VariantStructAtomHelper atom;
    } d;
    enum VariantType type;
};

static void variant_clear_to(Variant var, enum VariantType type) {
    if (variant_get_type(var) == VariantCustom) {
        var->d.custom.base->deleter(var->d.custom.data);
        container_base_destroy_if_dynamic(var->d.custom.base);
    } else
        FREE(var->d.atom.string.data);

    var->d.atom.string.data = NULL;
    var->d.atom.string.length = 0;
    var->type = type;
}

Variant variant_create_null() {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantNull;

    return var;
}

Variant variant_create_boolean(int b) {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantBoolean;
    var->d.atom.d.boolean = !!b;

    return var;
}

Variant variant_create_int(int value) {
    return variant_create_int64(value);
}

Variant variant_create_uint(unsigned int value) {
    return variant_create_uint64(value);
}

Variant variant_create_int64(long long value) {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantInteger;
    var->d.atom.d.integer = value;

    return var;
}

Variant variant_create_uint64(unsigned long long value) {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantUnsignedInteger;
    var->d.atom.d.unsigned_integer = value;

    return var;
}

Variant variant_create_float(double value) {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantFloat;
    var->d.atom.d.floating = value;

    return var;
}

Variant variant_create_string_move(char *value) {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantString;
    var->d.atom.string.data = value;

    return var;
}

Variant variant_create_string(const char *value) {
    char *duplicate = strdup(value);
    if (duplicate == NULL)
        return NULL;

    Variant var = variant_create_string_move(duplicate);
    if (var == NULL)
        FREE(duplicate);

    return var;
}

Variant variant_create_binary_string_move(char *value, size_t value_len) {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantBinary;
    var->d.atom.string.data = value;
    var->d.atom.string.length = value_len;

    return var;
}

Variant variant_create_binary_string(const char *value, size_t value_len) {
    char *duplicate = MALLOC(value_len+1);
    if (duplicate == NULL)
        return NULL;

    Variant var = variant_create_binary_string_move(duplicate, value_len);
    if (var == NULL)
        FREE(duplicate);
    else {
        memcpy(duplicate, value, value_len);
        duplicate[value_len] = 0;
    }

    return var;
}

Variant variant_create_binary_string_binary_move(Binary value) {
    return variant_create_binary_string_move(value.data, value.length);
}

Variant variant_create_binary_string_binary(const Binary value) {
    return variant_create_binary_string(value.data, value.length);
}

Variant variant_create_custom_move(void *item, const CommonContainerBase *base) {
    Variant var = CALLOC(sizeof(*var), 1);
    CommonContainerBase *new_base = container_base_copy_if_dynamic(base);
    if (var == NULL || new_base == NULL) {
        FREE(var);
        FREE(new_base);
        return NULL;
    }

    var->type = VariantCustom;
    var->d.custom.data = item;
    var->d.custom.base = new_base;

    return var;
}

Variant variant_create_custom(const void *item, const CommonContainerBase *base) {
    if (base->copier == NULL) /* Needed in order to copy item */
        return NULL;

    void *duplicate = base->copier(item);
    if (duplicate == NULL && item != NULL)
        return NULL;

    Variant var = variant_create_custom_move(duplicate, base);
    if (var == NULL && base->deleter)
        base->deleter(duplicate);

    return var;
}

Variant variant_create_custom_move_adopt(void *item, CommonContainerBase *base) {
    if (base == NULL)
        return NULL;

    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL) {
        container_base_destroy_if_dynamic(base);
        return NULL;
    }

    var->type = VariantCustom;
    var->d.custom.data = item;
    var->d.custom.base = base;

    return var;
}

Variant variant_create_custom_adopt(const void *item, CommonContainerBase *base) {
    if (base == NULL || base->copier == NULL) /* Needed to copy item */
        goto cleanup;

    void *duplicate = base->copier(item);
    if (duplicate == NULL && item != NULL)
        goto cleanup;

    Variant var = variant_create_custom_move_adopt(duplicate, base);
    if (var == NULL) {
        if (base->deleter)
            base->deleter(duplicate);

        return NULL; /* The variant_create function already cleaned up the base */
    }

    return var;

cleanup:
    container_base_destroy_if_dynamic(base);
    return NULL;
}

Variant variant_copy(Variant other) {
    switch (variant_get_type(other)) {
        default:
        case VariantNull: return variant_create_null();
        case VariantBoolean: return variant_create_boolean(variant_get_boolean(other));
        case VariantInteger: return variant_create_int64(variant_get_int64(other));
        case VariantUnsignedInteger: return variant_create_uint64(variant_get_uint64(other));
        case VariantFloat: return variant_create_float(variant_get_float(other));
        case VariantString: return variant_create_string(variant_get_string(other));
        case VariantBinary: return variant_create_binary_string_binary(variant_get_binary(other));
        case VariantCustom: return variant_create_custom(variant_get_custom(other), variant_get_custom_container_base(other));
    }
}

int variant_compare(Variant lhs, Variant rhs) {
    if (variant_get_type(lhs) < variant_get_type(rhs))
        return -1;
    else if (variant_get_type(lhs) > variant_get_type(rhs))
        return 1;

    switch (variant_get_type(lhs)) {
        default:
        case VariantNull: return 0;
        case VariantBoolean: return (lhs->d.atom.d.boolean > rhs->d.atom.d.boolean) - (lhs->d.atom.d.boolean < rhs->d.atom.d.boolean);
        case VariantInteger: return (lhs->d.atom.d.integer > rhs->d.atom.d.integer) - (lhs->d.atom.d.integer < rhs->d.atom.d.integer);
        case VariantFloat: return (lhs->d.atom.d.floating > rhs->d.atom.d.floating) - (lhs->d.atom.d.floating < rhs->d.atom.d.floating);
        case VariantString: return strcmp(lhs->d.atom.string.data, rhs->d.atom.string.data);
        case VariantBinary: return binary_compare(&lhs->d.atom.string, &rhs->d.atom.string);
        case VariantCustom: {
            int cmp = generic_types_compatible_compare(lhs->d.custom.base, rhs->d.custom.base);
            if (cmp)
                return cmp;

            if (lhs->d.custom.base->compare)
                return lhs->d.custom.base->compare(lhs->d.custom.data, rhs->d.custom.data);
            else if (rhs->d.custom.base->compare)
                return rhs->d.custom.base->compare(lhs->d.custom.data, rhs->d.custom.data);

            return 0; /* Assume equal if no comparison function */
        }
    }
}

enum VariantType variant_get_type(Variant var) {
    return var->type;
}

int variants_are_equal_types(Variant a, Variant b) {
    if (variant_is_custom(a) && variant_is_custom(b))
        return !generic_types_compatible_compare(a->d.custom.base, b->d.custom.base);

    return a->type == b->type;
}

int variant_is_null(Variant var) {
    return variant_get_type(var) == VariantNull;
}

int variant_is_boolean(Variant var) {
    return variant_get_type(var) == VariantBoolean;
}

int variant_is_int(Variant var) {
    if (variant_get_type(var) != VariantInteger)
        return 0;

    return var->d.atom.d.integer >= INT_MIN && var->d.atom.d.integer <= INT_MAX;
}

int variant_is_uint(Variant var) {
    if (variant_get_type(var) != VariantUnsignedInteger)
        return 0;

    return var->d.atom.d.unsigned_integer <= UINT_MAX;
}

int variant_is_int64(Variant var) {
    return variant_get_type(var) == VariantInteger;
}

int variant_is_uint64(Variant var) {
    return variant_get_type(var) == VariantUnsignedInteger;
}

int variant_is_float(Variant var) {
    return variant_get_type(var) == VariantFloat;
}

int variant_is_integral(Variant var) {
    return variant_get_type(var) == VariantInteger || variant_get_type(var) == VariantUnsignedInteger;
}

int variant_is_number(Variant var) {
    return variant_is_integral(var) || variant_is_float(var);
}

int variant_is_string(Variant var) {
    return variant_get_type(var) == VariantString;
}

int variant_is_binary(Variant var) {
    return variant_get_type(var) == VariantBinary;
}

int variant_is_custom(Variant var) {
    return variant_get_type(var) == VariantCustom;
}

int variant_set_null(Variant var) {
    variant_clear_to(var, VariantNull);
    return 0;
}

int variant_set_boolean(Variant var, int b) {
    variant_clear_to(var, VariantBoolean);

    var->d.atom.d.boolean = !!b;

    return 0;
}

int variant_set_int(Variant var, int value) {
    variant_clear_to(var, VariantInteger);

    var->d.atom.d.integer = value;

    return 0;
}

int variant_set_uint(Variant var, unsigned int value) {
    variant_clear_to(var, VariantUnsignedInteger);

    var->d.atom.d.unsigned_integer = value;

    return 0;
}

int variant_set_int64(Variant var, long long value) {
    variant_clear_to(var, VariantInteger);

    var->d.atom.d.integer = value;

    return 0;
}

int variant_set_uint64(Variant var, unsigned long long value) {
    variant_clear_to(var, VariantUnsignedInteger);

    var->d.atom.d.unsigned_integer = value;

    return 0;
}

int variant_set_float(Variant var, double value) {
    variant_clear_to(var, VariantFloat);

    var->d.atom.d.floating = value;

    return 0;
}

int variant_set_string_move(Variant var, char *value) {
    variant_clear_to(var, VariantString);

    var->d.atom.string.data = value;
    var->d.atom.string.length = 0;

    return 0;
}

int variant_set_string(Variant var, const char *value) {
    char *duplicate = strdup(value);
    if (duplicate == NULL)
        return CC_ENOMEM;

    int err = variant_set_string_move(var, duplicate);
    if (err)
        FREE(duplicate);

    return err;
}

int variant_set_binary_string_move(Variant var, char *value, size_t value_len) {
    variant_clear_to(var, VariantBinary);

    var->d.atom.string.data = value;
    var->d.atom.string.length = value_len;

    return 0;
}

int variant_set_binary_string_binary_move(Variant var, Binary value) {
    return variant_set_binary_string_move(var, value.data, value.length);
}

int variant_set_binary_string(Variant var, const char *value, size_t value_len) {
    char *duplicate = MALLOC(value_len+1);
    if (duplicate == NULL)
        return CC_ENOMEM;

    int err = variant_set_binary_string_move(var, duplicate, value_len);
    if (err)
        FREE(duplicate);
    else {
        memcpy(duplicate, value, value_len);
        duplicate[value_len] = 0;
    }

    return err;
}

int variant_set_binary_string_binary(Variant var, const Binary value) {
    return variant_set_binary_string(var, value.data, value.length);
}

int variant_set_custom_move(Variant var, void *item, const CommonContainerBase *base) {
    CommonContainerBase *new_base = container_base_copy_if_dynamic(base);
    if (new_base == NULL)
        return CC_ENOMEM;

    variant_clear_to(var, VariantCustom);

    var->d.custom.data = item;
    var->d.custom.base = new_base;

    return 0;
}

int variant_set_custom(Variant var, const void *item, const CommonContainerBase *base) {
    if (base == NULL || base->copier == NULL)
        return CC_EINVAL;

    void *duplicate = base->copier(item);
    if (duplicate == NULL && item != NULL)
        return CC_ENOMEM;

    int err = variant_set_custom_move(var, duplicate, base);
    if (err && base->deleter)
        base->deleter(duplicate);

    return err;
}

int variant_set_custom_move_adopt(Variant var, void *item, CommonContainerBase *base) {
    if (base == NULL)
        return CC_EINVAL;

    variant_clear_to(var, VariantCustom);

    var->d.custom.data = item;
    var->d.custom.base = base;

    return 0;
}

int variant_set_custom_adopt(Variant var, const void *item, CommonContainerBase *base) {
    int err = 0;

    if (base == NULL || base->copier == NULL) {
        err = CC_EINVAL;
        goto cleanup;
    }

    void *duplicate = base->copier(item);
    if (duplicate == NULL && item != NULL) {
        err = CC_ENOMEM;
        goto cleanup;
    }

    err = variant_set_custom_move_adopt(var, duplicate, base);
    if (err && base->deleter)
        base->deleter(duplicate);

    return err;

cleanup:
    container_base_destroy_if_dynamic(base);
    return err;
}

int variant_set_variant(Variant var, const Variant other) {
    Variant temp = variant_copy(other);
    if (temp == NULL && other != NULL)
        return CC_ENOMEM;

    return variant_set_variant_move(var, temp);
}

int variant_set_variant_move(Variant var, Variant other) {
    if (other == NULL)
        return variant_set_null(var);

    struct VariantStruct tempstruct = *var;
    *var = *other;
    *other = tempstruct;

    variant_destroy(other);
    return 0;
}

int variant_get_boolean(Variant var) {
    if (!variant_is_boolean(var))
        return 0;

    return var->d.atom.d.boolean;
}

int variant_get_int(Variant var) {
    if (!variant_is_int(var) || var->d.atom.d.integer < INT_MIN || var->d.atom.d.integer > INT_MAX)
        return 0;

    return (int) var->d.atom.d.integer;
}

long long variant_get_int64(Variant var) {
    if (!variant_is_int(var))
        return 0;

    return var->d.atom.d.integer;
}

unsigned int variant_get_uint(Variant var) {
    if (!variant_is_uint(var) || var->d.atom.d.unsigned_integer > UINT_MAX)
        return 0;

    return (unsigned int) var->d.atom.d.unsigned_integer;
}

unsigned long long variant_get_uint64(Variant var) {
    if (!variant_is_uint64(var))
        return 0;

    return var->d.atom.d.unsigned_integer;
}

double variant_get_float(Variant var) {
    if (!variant_is_float(var))
        return 0.0;

    return var->d.atom.d.floating;
}

char *variant_get_string(Variant var) {
    if (!variant_is_string(var))
        return NULL;

    return var->d.atom.string.data;
}

Binary variant_get_binary(Variant var) {
    if (!variant_is_binary(var)) {
        Binary temp = {.data = NULL, .length = 0};
        return temp;
    }

    return var->d.atom.string;
}

void *variant_get_custom(Variant var) {
    if (!variant_is_custom(var))
        return NULL;

    return var->d.custom.data;
}

int variant_to_boolean(Variant var, int *error) {
    if (error)
        *error = 0;

    switch (variant_get_type(var)) {
        default:
        case VariantNull: return 0;
        case VariantBoolean: return var->d.atom.d.boolean;
        case VariantInteger: return !!var->d.atom.d.integer;
        case VariantUnsignedInteger: return !!var->d.atom.d.unsigned_integer;
        case VariantFloat: return !!var->d.atom.d.floating;
        case VariantString: return var->d.atom.string.data && var->d.atom.string.data[0] != 0;
        case VariantBinary: return var->d.atom.string.length != 0;
        case VariantCustom:
            if (error)
                *error = CC_ENOTSUP;
            return 0;
    }
}

int variant_to_int(Variant var, int *error) {
    if (error)
        *error = 0;

    switch (variant_get_type(var)) {
        default:
        case VariantNull: return 0;
        case VariantBoolean: return var->d.atom.d.boolean;
        case VariantInteger: return variant_get_int(var);
        case VariantUnsignedInteger:
            if (var->d.atom.d.unsigned_integer > INT_MAX) {
                if (error)
                    *error = CC_ERANGE;
                return 0;
            }

            return (int) var->d.atom.d.unsigned_integer;
        case VariantFloat:
            if (var->d.atom.d.floating < INT_MIN || var->d.atom.d.floating > INT_MAX) {
                if (error)
                    *error = CC_ERANGE;
                return 0;
            }

            return (int) trunc(var->d.atom.d.floating);
        case VariantString:
        case VariantBinary: {
            errno = 0;
            long result = strtol(var->d.atom.string.data, NULL, 10);
            if (errno || result < INT_MIN || result > INT_MAX) {
                if (error)
                    *error = CC_ERANGE;
                return 0;
            }

            return result;
        }
        case VariantCustom:
            if (error)
                *error = CC_ENOTSUP;
            return 0;
    }
}

unsigned int variant_to_uint(Variant var, int *error) {
    if (error)
        *error = 0;

    switch (variant_get_type(var)) {
        default:
        case VariantNull: return 0;
        case VariantBoolean: return var->d.atom.d.boolean;
        case VariantInteger:
            if (var->d.atom.d.integer < 0 || var->d.atom.d.integer > UINT_MAX) {
                if (error)
                    *error = CC_ERANGE;
                return 0;
            }

            return (unsigned int) var->d.atom.d.integer;
        case VariantUnsignedInteger: return variant_get_uint(var);
        case VariantFloat:
            if (var->d.atom.d.floating < 0 || var->d.atom.d.floating > UINT_MAX) {
                if (error)
                    *error = CC_ERANGE;
                return 0;
            }

            return (unsigned int) trunc(var->d.atom.d.floating);
        case VariantString:
        case VariantBinary: {
            errno = 0;
            unsigned long result = strtoul(var->d.atom.string.data, NULL, 10);
            if (errno || result > UINT_MAX) {
                if (error)
                    *error = CC_ERANGE;
                return 0;
            }

            return result;
        }
        case VariantCustom:
            if (error)
                *error = CC_ENOTSUP;
            return 0;
    }
}

long long variant_to_int64(Variant var, int *error) {
    if (error)
        *error = 0;

    switch (variant_get_type(var)) {
        default:
        case VariantNull: return 0;
        case VariantBoolean: return var->d.atom.d.boolean;
        case VariantInteger: return var->d.atom.d.integer;
        case VariantUnsignedInteger:
            if (var->d.atom.d.unsigned_integer > LLONG_MAX) {
                if (error)
                    *error = CC_ERANGE;
                return 0;
            }

            return (long long int) var->d.atom.d.unsigned_integer;
        case VariantFloat: return (long long int) trunc(var->d.atom.d.floating);
        case VariantString:
        case VariantBinary: {
            errno = 0;
            long long result = strtoll(var->d.atom.string.data, NULL, 10);
            if (errno) {
                if (error)
                    *error = CC_ERANGE;
                return 0;
            }

            return result;
        }
        case VariantCustom:
            if (error)
                *error = CC_ENOTSUP;
            return 0;
    }
}

unsigned long long variant_to_uint64(Variant var, int *error) {
    if (error)
        *error = 0;

    switch (variant_get_type(var)) {
        default:
        case VariantNull: return 0;
        case VariantBoolean: return var->d.atom.d.boolean;
        case VariantInteger:
            if (var->d.atom.d.integer < 0) {
                if (error)
                    *error = CC_ERANGE;
                return 0;
            }

            return (unsigned long long) var->d.atom.d.integer;
        case VariantUnsignedInteger: return var->d.atom.d.unsigned_integer;
        case VariantFloat: return (unsigned long long) trunc(var->d.atom.d.floating);
        case VariantString:
        case VariantBinary: {
            errno = 0;
            unsigned long long result = strtoull(var->d.atom.string.data, NULL, 10);
            if (errno) {
                if (error)
                    *error = CC_ERANGE;
                return 0;
            }

            return result;
        }
        case VariantCustom:
            if (error)
                *error = CC_ENOTSUP;
            return 0;
    }
}

double variant_to_float(Variant var, int *error) {
    if (error)
        *error = 0;

    switch (variant_get_type(var)) {
        default:
        case VariantNull: return 0;
        case VariantBoolean: return var->d.atom.d.boolean;
        case VariantInteger:
            if ((long long) (double) var->d.atom.d.integer != var->d.atom.d.integer) {
                if (error)
                    *error = CC_ERANGE;
                return 0.0;
            }

            return (double) var->d.atom.d.integer;
        case VariantUnsignedInteger:
            if ((unsigned long long) (double) var->d.atom.d.unsigned_integer != var->d.atom.d.unsigned_integer) {
                if (error)
                    *error = CC_ERANGE;
                return 0.0;
            }

            return (double) var->d.atom.d.unsigned_integer;
        case VariantFloat: return var->d.atom.d.floating;
        case VariantString:
        case VariantBinary: {
            errno = 0;
            double result = strtod(var->d.atom.string.data, NULL);
            if (errno) {
                if (error)
                    *error = CC_ERANGE;
                return 0.0;
            }

            return result;
        }
        case VariantCustom:
            if (error)
                *error = CC_ENOTSUP;
            return 0;
    }
}

const char *variant_to_string(Variant var, int *error) {
    if (variant_is_custom(var)) {
        if (error)
            *error = CC_ENOTSUP;
        return NULL;
    }

    if (var->d.atom.string.data)
        return var->d.atom.string.data;

    char *string = NULL;
    switch (variant_get_type(var)) {
        default:
        case VariantNull: string = strdup(""); break;
        case VariantBoolean: string = strdup(var->d.atom.d.boolean? "true": "false"); break;
        case VariantInteger: {
            /* Silence some warnings with using the wrong (standard!) format */
#if WINDOWS_OS && (MSVC_COMPILER | CLANG_COMPILER | GCC_COMPILER)
# define VARIANT_TO_STRING_FP_FORMAT "%I64d"
#else
# define VARIANT_TO_STRING_FP_FORMAT "%lld"
#endif
            size_t len = snprintf(NULL, 0, VARIANT_TO_STRING_FP_FORMAT, var->d.atom.d.integer);
            string = MALLOC(len+1);
            if (string)
                snprintf(string, len+1, VARIANT_TO_STRING_FP_FORMAT, var->d.atom.d.integer);
            break;
        }
        case VariantUnsignedInteger: {
            /* Silence some warnings with using the wrong (standard!) format */
#if WINDOWS_OS && (MSVC_COMPILER | CLANG_COMPILER | GCC_COMPILER)
# define VARIANT_UNSIGNED_TO_STRING_FP_FORMAT "%I64u"
#else
# define VARIANT_UNSIGNED_TO_STRING_FP_FORMAT "%llu"
#endif
            size_t len = snprintf(NULL, 0, VARIANT_UNSIGNED_TO_STRING_FP_FORMAT, var->d.atom.d.integer);
            string = MALLOC(len+1);
            if (string)
                snprintf(string, len+1, VARIANT_UNSIGNED_TO_STRING_FP_FORMAT, var->d.atom.d.integer);
            break;
        }
        case VariantFloat: {
            size_t len = snprintf(NULL, 0, "%.*g", DBL_DIG-1, var->d.atom.d.floating);
            string = MALLOC(len+1);
            if (string)
                snprintf(string, len+1, "%.*g", DBL_DIG-1, var->d.atom.d.floating);
            break;
        }
    }

    if (error && string == NULL)
        *error = CC_ENOMEM;

    var->d.atom.string.length = string? strlen(string): 0;
    return var->d.atom.string.data = string;
}

Binary variant_to_binary(Variant var, int *error) {
    if (variant_is_custom(var)) {
        if (error)
            *error = CC_ENOTSUP;
        Binary temp = {.data = NULL, .length = 0};
        return temp;
    }

    if (variant_is_string(var)) {
        if (var->d.atom.string.length == 0)
            var->d.atom.string.length = strlen(var->d.atom.string.data);
        return var->d.atom.string;
    }

    if (var->d.atom.string.data)
        return var->d.atom.string;

    variant_to_string(var, error);

    return var->d.atom.string;
}

void variant_clear(Variant var) {
    variant_clear_to(var, VariantNull);
}

void variant_destroy(Variant var) {
    if (var) {
        if (variant_get_type(var) == VariantCustom) {
            if (var->d.custom.base->deleter)
                var->d.custom.base->deleter(var->d.custom.data);

            container_base_destroy_if_dynamic(var->d.custom.base);
        } else {
            FREE(var->d.atom.string.data);
        }

        FREE(var);
    }
}

const CommonContainerBase *variant_get_custom_container_base(Variant var) {
    if (variant_is_custom(var))
        return var->d.custom.base;

    return NULL;
}

CommonContainerBase *variant_build_recipe(Variant variant) {
    if (variant_is_custom(variant))
        return container_base_build_container(variant_get_custom_container_base(variant), container_base_variant_recipe());

    return (CommonContainerBase *) container_base_variant_recipe();
}
