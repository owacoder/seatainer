/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "variant.h"
#include "../utility.h"

#include <float.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>

struct VariantStructCustomHelper {
    Compare compare;
    Copier copy;
    Deleter deleter;
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
    CommonContainerBase base;
};

static void variant_clear_to(Variant var, enum VariantType type) {
    if (variant_get_type(var) == VariantCustom)
        var->d.custom.deleter(var->d.custom.data);
    else
        FREE(var->d.atom.string.data);

    if (!var->base.cvt_expects_variant && type != var->type)
        var->base = empty_container_base();

    var->d.atom.string.data = NULL;
    var->d.atom.string.length = 0;
    var->type = type;
}

Variant variant_create_null() {return variant_create_null_base(empty_container_base());}

Variant variant_create_null_base(CommonContainerBase base) {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantNull;
    var->base = base;

    return var;
}

Variant variant_create_boolean(int b) {return variant_create_boolean_base(b, empty_container_base());}

Variant variant_create_boolean_base(int b, CommonContainerBase base) {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantBoolean;
    var->d.atom.d.boolean = !!b;
    var->base = base;

    return var;
}

Variant variant_create_int(int value) {return variant_create_int_base(value, empty_container_base());}

Variant variant_create_int_base(int value, CommonContainerBase base) {
    return variant_create_int64_base(value, base);
}

Variant variant_create_uint(unsigned int value) {return variant_create_uint_base(value, empty_container_base());}

Variant variant_create_uint_base(unsigned int value, CommonContainerBase base) {
    return variant_create_uint64_base(value, base);
}

Variant variant_create_int64(long long value) {return variant_create_int64_base(value, empty_container_base());}

Variant variant_create_int64_base(long long value, CommonContainerBase base) {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantInteger;
    var->d.atom.d.integer = value;
    var->base = base;

    return var;
}

Variant variant_create_uint64(unsigned long long value) {return variant_create_uint64_base(value, empty_container_base());}

Variant variant_create_uint64_base(unsigned long long value, CommonContainerBase base) {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantUnsignedInteger;
    var->d.atom.d.unsigned_integer = value;
    var->base = base;

    return var;
}

Variant variant_create_float(double value) {return variant_create_float_base(value, empty_container_base());}

Variant variant_create_float_base(double value, CommonContainerBase base) {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantFloat;
    var->d.atom.d.floating = value;
    var->base = base;

    return var;
}

Variant variant_create_string_move(char *value) {return variant_create_string_move_base(value, empty_container_base());}

Variant variant_create_string_move_base(char *value, CommonContainerBase base) {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantString;
    var->d.atom.string.data = value;
    var->base = base;

    return var;
}

Variant variant_create_string(const char *value) {return variant_create_string_base(value, empty_container_base());}

Variant variant_create_string_base(const char *value, CommonContainerBase base) {
    char *duplicate = strdup(value);
    if (duplicate == NULL)
        return NULL;

    Variant var = variant_create_string_move_base(duplicate, base);
    if (var == NULL)
        FREE(duplicate);

    return var;
}

Variant variant_create_binary_string_move(char *value, size_t value_len) {return variant_create_binary_string_move_base(value, value_len, empty_container_base());}

Variant variant_create_binary_string_move_base(char *value, size_t value_len, CommonContainerBase base) {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantBinary;
    var->d.atom.string.data = value;
    var->d.atom.string.length = value_len;
    var->base = base;

    return var;
}

Variant variant_create_binary_string(const char *value, size_t value_len) {return variant_create_binary_string_base(value, value_len, empty_container_base());}

Variant variant_create_binary_string_base(const char *value, size_t value_len, CommonContainerBase base) {
    char *duplicate = MALLOC(value_len+1);
    if (duplicate == NULL)
        return NULL;

    Variant var = variant_create_binary_string_move_base(duplicate, value_len, base);
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

Variant variant_create_binary_string_binary_move_base(Binary value, CommonContainerBase base) {
    return variant_create_binary_string_move_base(value.data, value.length, base);
}

Variant variant_create_binary_string_binary(const Binary value) {
    return variant_create_binary_string(value.data, value.length);
}

Variant variant_create_binary_string_binary_base(const Binary value, CommonContainerBase base) {
    return variant_create_binary_string_base(value.data, value.length, base);
}

Variant variant_create_custom_move(void *item, Compare compare, Copier copier, Deleter deleter) {return variant_create_custom_move_base(item, compare, copier, deleter, empty_container_base());}

Variant variant_create_custom_move_base(void *item, Compare compare, Copier copier, Deleter deleter, CommonContainerBase base) {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantCustom;
    var->d.custom.compare = compare;
    var->d.custom.copy = copier;
    var->d.custom.deleter = deleter? deleter: (Deleter) FREE;
    var->d.custom.data = item;
    var->base = base;

    return var;
}

Variant variant_create_custom(const void *item, Compare compare, Copier copier, Deleter deleter) {return variant_create_custom_base(item, compare, copier, deleter, empty_container_base());}

Variant variant_create_custom_base(const void *item, Compare compare, Copier copier, Deleter deleter, CommonContainerBase base) {
    if (copier == NULL)
        return NULL;

    if (deleter == NULL)
        deleter = FREE;

    void *duplicate = copier(item);
    if (duplicate == NULL && item != NULL)
        return NULL;

    Variant var = variant_create_custom_move_base(duplicate, compare, copier, deleter, base);
    if (var == NULL)
        deleter(duplicate);

    return var;
}

Variant variant_copy(Variant other) {
    switch (variant_get_type(other)) {
        default:
        case VariantNull: return variant_create_null_base(*variant_get_container_base(other));
        case VariantBoolean: return variant_create_boolean_base(variant_get_boolean(other), *variant_get_container_base(other));
        case VariantInteger: return variant_create_int64_base(variant_get_int64(other), *variant_get_container_base(other));
        case VariantUnsignedInteger: return variant_create_uint64_base(variant_get_uint64(other), *variant_get_container_base(other));
        case VariantFloat: return variant_create_float_base(variant_get_float(other), *variant_get_container_base(other));
        case VariantString: return variant_create_string_base(variant_get_string(other), *variant_get_container_base(other));
        case VariantBinary: return variant_create_binary_string_binary_base(variant_get_binary(other), *variant_get_container_base(other));
        case VariantCustom: return variant_create_custom_base(variant_get_custom(other),
                                                              variant_get_compare_fn(other),
                                                              variant_get_copier_fn(other),
                                                              variant_get_deleter_fn(other),
                                                              *variant_get_container_base(other));
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
            int cmp = generictypes_compatible_compare(lhs->d.custom.compare, rhs->d.custom.compare,
                                                      lhs->d.custom.copy, rhs->d.custom.copy,
                                                      lhs->d.custom.deleter, rhs->d.custom.deleter);
            if (cmp)
                return cmp;

            if (lhs->d.custom.compare)
                return lhs->d.custom.compare(lhs->d.custom.data, rhs->d.custom.data);
            else if (rhs->d.custom.compare)
                return rhs->d.custom.compare(lhs->d.custom.data, rhs->d.custom.data);

            return 0; /* Assume equal if no comparison function */
        }
    }
}

enum VariantType variant_get_type(Variant var) {
    return var->type;
}

int variants_are_equal_types(Variant a, Variant b) {
    if (variant_is_custom(a) && variant_is_custom(b))
        return !generictypes_compatible_compare(a->d.custom.compare, b->d.custom.compare,
                                                a->d.custom.copy, b->d.custom.copy,
                                                a->d.custom.deleter, b->d.custom.deleter);

    return a->type == b->type;
}

Compare variant_get_compare_fn(Variant var) {
    if (!variant_is_custom(var))
        return NULL;

    return var->d.custom.compare;
}

Copier variant_get_copier_fn(Variant var) {
    if (!variant_is_custom(var))
        return NULL;

    return var->d.custom.copy;
}

Deleter variant_get_deleter_fn(Variant var) {
    if (!variant_is_custom(var))
        return NULL;

    return var->d.custom.deleter;
}

VariantParser variant_get_parser_fn(Variant var) {
    return (VariantParser) var->base.parse;
}

VariantSerializer variant_get_serializer_fn(Variant var) {
    return (VariantSerializer) var->base.serialize;
}

void variant_set_parser_fn(Variant var, VariantParser parse) {
    var->base.parse = (Parser) parse;
}

void variant_set_serializer_fn(Variant var, VariantSerializer serialize) {
    var->base.serialize = (Serializer) serialize;
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

int variant_set_custom_move(Variant var, void *item, Compare compare, Copier copier, Deleter deleter) {
    return variant_set_custom_move_base(var, item, compare, copier, deleter, empty_container_base());
}

int variant_set_custom_move_base(Variant var, void *item, Compare compare, Copier copier, Deleter deleter, CommonContainerBase base) {
    if (compare == NULL || copier == NULL)
        return CC_EINVAL;

    variant_clear_to(var, VariantCustom);

    var->d.custom.compare = compare;
    var->d.custom.copy = copier;
    var->d.custom.deleter = deleter? deleter: (Deleter) FREE;
    var->d.custom.data = item;
    var->base = base;

    return 0;
}

int variant_set_custom(Variant var, const void *item, Compare compare, Copier copier, Deleter deleter) {
    return variant_set_custom_base(var, item, compare, copier, deleter, empty_container_base());
}

int variant_set_custom_base(Variant var, const void *item, Compare compare, Copier copier, Deleter deleter, CommonContainerBase base) {
    if (compare == NULL || copier == NULL)
        return CC_EINVAL;

    void *duplicate = copier(item);
    if (duplicate == NULL && item != NULL)
        return CC_ENOMEM;

    variant_clear_to(var, VariantCustom);

    var->d.custom.compare = compare;
    var->d.custom.copy = copier;
    var->d.custom.deleter = deleter? deleter: (Deleter) FREE;
    var->d.custom.data = duplicate;
    var->base = base;

    return 0;
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
            size_t len = snprintf(NULL, 0, "%.*g", DECIMAL_DIG-1, var->d.atom.d.floating);
            string = MALLOC(len+1);
            if (string)
                snprintf(string, len+1, "%.*g", DECIMAL_DIG-1, var->d.atom.d.floating);
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
            var->d.custom.deleter(var->d.custom.data);
        } else {
            FREE(var->d.atom.string.data);
        }

        FREE(var);
    }
}

CommonContainerBase *variant_get_container_base(Variant var) {
    return &var->base;
}

int variant_serialize(void *output, Variant var, struct SerializerIdentity *type) {
    if (var == NULL)
        return CC_EINVAL;

    const CommonContainerBase *base = variant_get_container_base(var);
    Serializer serialize = base->serialize;
    if (serialize == NULL)
        return CC_EINVAL;

    if (base->cvt_expects_variant)
        return serialize(output, var, type);

    switch (var->type) {
        case VariantNull: return serialize(output, NULL, type);
        case VariantBoolean: return serialize(output, &var->d.atom.d.boolean, type);
        case VariantInteger: return serialize(output, &var->d.atom.d.integer, type);
        case VariantUnsignedInteger: return serialize(output, &var->d.atom.d.unsigned_integer, type);
        case VariantFloat: return serialize(output, &var->d.atom.d.floating, type);
        case VariantString: return serialize(output, &var->d.atom.string.data, type);
        case VariantBinary: return serialize(output, &var->d.atom.string, type);
        default: return CC_ENOTSUP;
    }
}
