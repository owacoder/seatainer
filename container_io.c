/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "container_io.h"
#include "Containers/sbuffer.h"
#include "Containers/recipes.h"
#include "utility.h"
#include <float.h>
#include <math.h>

/* Serializer basics:
 *
 * Value serializers are used where specified, if they're the same type as the format
 * If the value is a native variant, serialize the variant
 * If the value is a custom variant, attempt to serialize the contained value with the current serializer.
 * If a serializer is specified with a different type, and the type is a collection, serialize the collection and call the current serializer for all elements
 * If a serializer is specified with a different type, and the type is not a collection, attempt to serialize the item with the current serializer if it's a known type
 * If no serializer is specified, and the type is a collection, serialize the collection and call the current serializer for all elements
 * If no serializer is specified, and the type is not a collection, attempt to serialize the item with the current serializer if it's a known type
 * Otherwise, an error occurs and serialization cannot continue
 */

int io_serialize_boolean(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_boolean, 1);

    if (generic_types_compatible_compare(base, container_base_boolean_recipe()) != 0)
        return CC_ENOTSUP;

    return io_puts(*((const _Bool *) data)? "true": "false", output)? io_error(output): 0;
}

int io_serialize_char(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_char, 1);

    if (generic_types_compatible_compare(base, container_base_char_recipe()) != 0)
        return CC_ENOTSUP;

    return io_printf(output, "'%.1s'", data) < 0? io_error(output): 0;
}

int io_serialize_short(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_short, 1);

    if (generic_types_compatible_compare(base, container_base_short_recipe()) != 0)
        return CC_ENOTSUP;

    return io_printf(output, "%hd", *((const short *) data)) < 0? io_error(output): 0;
}

int io_serialize_ushort(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_ushort, 1);

    if (generic_types_compatible_compare(base, container_base_ushort_recipe()) != 0)
        return CC_ENOTSUP;

    return io_printf(output, "%hu", *((const unsigned short *) data)) < 0? io_error(output): 0;
}

int io_serialize_int(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_int, 1);

    if (generic_types_compatible_compare(base, container_base_int_recipe()) != 0)
        return CC_ENOTSUP;

    return io_printf(output, "%d", *((const int *) data)) < 0? io_error(output): 0;
}

int io_serialize_uint(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_uint, 1);

    if (generic_types_compatible_compare(base, container_base_uint_recipe()) != 0)
        return CC_ENOTSUP;

    return io_printf(output, "%u", *((const unsigned int *) data)) < 0? io_error(output): 0;
}

int io_serialize_long(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_long, 1);

    if (generic_types_compatible_compare(base, container_base_long_recipe()) != 0)
        return CC_ENOTSUP;

    return io_printf(output, "%ld", *((const long *) data)) < 0? io_error(output): 0;
}

int io_serialize_ulong(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_ulong, 1);

    if (generic_types_compatible_compare(base, container_base_ulong_recipe()) != 0)
        return CC_ENOTSUP;

    return io_printf(output, "%lu", *((const unsigned long *) data)) < 0? io_error(output): 0;
}

int io_serialize_long_long(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_long_long, 1);

    if (generic_types_compatible_compare(base, container_base_long_long_recipe()) != 0)
        return CC_ENOTSUP;

    return io_printf(output, "%lld", *((const long long *) data)) < 0? io_error(output): 0;
}

int io_serialize_ulong_long(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_ulong_long, 1);

    if (generic_types_compatible_compare(base, container_base_ulong_long_recipe()) != 0)
        return CC_ENOTSUP;

    return io_printf(output, "%llu", *((const unsigned long long *) data)) < 0? io_error(output): 0;
}

int io_serialize_float(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_float, 1);

    if (generic_types_compatible_compare(base, container_base_float_recipe()) != 0)
        return CC_ENOTSUP;

    return io_printf(output, "%.*g", FLT_DIG-1, *((const float *) data)) < 0? io_error(output): 0;
}

int io_serialize_double(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_double, 1);

    if (generic_types_compatible_compare(base, container_base_double_recipe()) != 0)
        return CC_ENOTSUP;

    return io_printf(output, "%.*g", DBL_DIG-1, *((const double *) data)) < 0? io_error(output): 0;
}

int io_serialize_long_double(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_long_double, 1);

    if (generic_types_compatible_compare(base, container_base_long_double_recipe()) != 0)
        return CC_ENOTSUP;

    return io_printf(output, "%.*Lg", LDBL_DIG-1, *((const long double *) data)) < 0? io_error(output): 0;
}

int io_serialize_cstring(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_cstring, 1);

    if (generic_types_compatible_compare(base, container_base_cstring_recipe()) != 0)
        return CC_ENOTSUP;

    return io_puts(data, output)? io_error(output): 0;
}

int io_serialize_binary(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_binary, 1);

    if (generic_types_compatible_compare(base, container_base_cstring_recipe()) != 0)
        return CC_ENOTSUP;

    const Binary *binary = data;

    for (size_t i = 0; i < binary->length; ++i) {
        unsigned char chr = binary->data[i];

        if (chr > 0x80 || chr < 0x20) {
            if (io_printf(output, "\\x%.2hhx", chr) < 0)
                return io_error(output);
        } else if (io_putc(chr, output) == EOF)
            return io_error(output);
    }

    return 0;
}

int io_serialize_variant(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    while (1) {
        SERIALIZER_DECLARE("UTF-8", io_serialize_variant, 1);

        if (generic_types_compatible_compare(base, container_base_variant_recipe()) != 0)
            return CC_ENOTSUP;

        Variant v = (Variant) data;

        switch (variant_get_type(v)) {
            case VariantCustom:
                data = variant_get_custom(v);
                base = variant_get_custom_container_base(v);
                continue;
            case VariantNull: return io_puts("<null>", output)? io_error(output): 0;
            case VariantBoolean: {
                _Bool bool = variant_get_boolean(v);
                return io_serialize_boolean(output, &bool, container_base_boolean_recipe(), NULL);
            }
            case VariantInteger: {
                long long i = variant_get_int64(v);
                return io_serialize_long_long(output, &i, container_base_long_long_recipe(), NULL);
            }
            case VariantUnsignedInteger: {
                unsigned long long i = variant_get_uint64(v);
                return io_serialize_ulong_long(output, &i, container_base_ulong_long_recipe(), NULL);
            }
            case VariantFloat: {
                double d = variant_get_float(v);
                return io_serialize_double(output, &d, container_base_double_recipe(), NULL);
            }
            case VariantString: {
                const char *s = variant_get_string(v);
                return io_serialize_double(output, s, container_base_cstring_recipe(), NULL);
            }
            case VariantBinary: {
                Binary b = variant_get_binary(v);
                return io_serialize_binary(output, &b, container_base_binary_recipe(), NULL);
            }
            default:
                return CC_ENOTSUP;
        }
    }
}

int io_serialize_container(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_container, 1);

    if (base->collection_begin == NULL ||
            base->collection_next == NULL ||
            base->collection_get_value == NULL)
        return CC_ENOTSUP;

    Serializer key_serialize = base->key_child? base->key_child->serialize: NULL;
    if (key_serialize == NULL)
        key_serialize = io_default_serializer_for_type(base->key_child);

    Serializer value_serialize = base->value_child? base->value_child->serialize: NULL;
    if (value_serialize == NULL)
        value_serialize = io_default_serializer_for_type(base->value_child);

    if (base->collection_get_key) {
        if (key_serialize == NULL || value_serialize == NULL)
            return CC_ENOTSUP;

        if (io_putc('{', output) == EOF)
            return io_error(output);

        Iterator start = base->collection_begin(data);
        for (Iterator it = start; it; it = base->collection_next(data, it)) {
            if (it != start && io_puts(", ", output) == EOF)
                return io_error(output);

            int err = key_serialize(output, base->collection_get_key(data, it), base->key_child, NULL);
            if (err)
                return err;

            if (io_puts(": ", output) == EOF)
                return io_error(output);

            err = value_serialize(output, base->collection_get_value(data, it), base->value_child, NULL);
            if (err)
                return err;
        }

        if (io_putc('}', output) == EOF)
            return io_error(output);
    } else {
        if (value_serialize == NULL)
            return CC_ENOTSUP;

        if (io_putc('[', output) == EOF)
            return io_error(output);

        Iterator start = base->collection_begin(data);
        for (Iterator it = start; it; it = base->collection_next(data, it)) {
            if (it != start && io_puts(", ", output) == EOF)
                return io_error(output);

            int err = value_serialize(output, base->collection_get_value(data, it), base->value_child, NULL);
            if (err)
                return err;
        }

        if (io_putc(']', output) == EOF)
            return io_error(output);
    }

    return 0;
}

Serializer io_default_serializer_for_type(const CommonContainerBase *base) {
    if (generic_types_compatible_compare(base, container_base_boolean_recipe()) == 0)
        return (Serializer) io_serialize_boolean;
    else if (generic_types_compatible_compare(base, container_base_char_recipe()) == 0 ||
             generic_types_compatible_compare(base, container_base_uchar_recipe()) == 0)
        return (Serializer) io_serialize_char;
    else if (generic_types_compatible_compare(base, container_base_short_recipe()) == 0)
        return (Serializer) io_serialize_short;
    else if (generic_types_compatible_compare(base, container_base_ushort_recipe()) == 0)
        return (Serializer) io_serialize_ushort;
    else if (generic_types_compatible_compare(base, container_base_int_recipe()) == 0)
        return (Serializer) io_serialize_int;
    else if (generic_types_compatible_compare(base, container_base_uint_recipe()) == 0)
        return (Serializer) io_serialize_uint;
    else if (generic_types_compatible_compare(base, container_base_long_recipe()) == 0)
        return (Serializer) io_serialize_long;
    else if (generic_types_compatible_compare(base, container_base_ulong_recipe()) == 0)
        return (Serializer) io_serialize_ulong;
    else if (generic_types_compatible_compare(base, container_base_long_long_recipe()) == 0)
        return (Serializer) io_serialize_long_long;
    else if (generic_types_compatible_compare(base, container_base_ulong_long_recipe()) == 0)
        return (Serializer) io_serialize_ulong_long;
    else if (generic_types_compatible_compare(base, container_base_float_recipe()) == 0)
        return (Serializer) io_serialize_float;
    else if (generic_types_compatible_compare(base, container_base_double_recipe()) == 0)
        return (Serializer) io_serialize_double;
    else if (generic_types_compatible_compare(base, container_base_long_double_recipe()) == 0)
        return (Serializer) io_serialize_long_double;
    else if (generic_types_compatible_compare(base, container_base_cstring_recipe()) == 0)
        return (Serializer) io_serialize_cstring;
    else if (generic_types_compatible_compare(base, container_base_binary_recipe()) == 0)
        return (Serializer) io_serialize_binary;
    else if (generic_types_compatible_compare(base, container_base_stringlist_recipe()) == 0 ||
             generic_types_compatible_compare(base, container_base_genericlist_recipe()) == 0 ||
             generic_types_compatible_compare(base, container_base_stringset_recipe()) == 0 ||
             generic_types_compatible_compare(base, container_base_genericset_recipe()) == 0 ||
             generic_types_compatible_compare(base, container_base_stringmap_recipe()) == 0 ||
             generic_types_compatible_compare(base, container_base_genericmap_recipe()) == 0)
        return (Serializer) io_serialize_container;
    else
        return NULL; /* Unknown default serializer type */
}

static size_t serialize_json_stringpart_io(const void *data, size_t size, size_t count, void *userdata, IO io) {
    IO output = (IO) userdata;

    size_t written = 0;
    size_t max = size*count;

    for (size_t i = 0; i < max; ++i) {
        char buffer[8] = {'\\'};
        size_t buffer_size = 1;
        unsigned char c = ((unsigned char *) data)[i];

        switch (c) {
            case '\\':
            case '"': buffer[1] = c; buffer_size = 2; break;
            case '\b': buffer[1] = 'b'; buffer_size = 2; break;
            case '\f': buffer[1] = 'f'; buffer_size = 2; break;
            case '\n': buffer[1] = 'n'; buffer_size = 2; break;
            case '\r': buffer[1] = 'r'; buffer_size = 2; break;
            case '\t': buffer[1] = 't'; buffer_size = 2; break;
            default: {
                if (c < 0x20) {
                    const char alphabet[] = "0123456789ABCDEF";

                    buffer[1] = 'u';
                    buffer[2] = buffer[3] = '0';
                    buffer[4] = alphabet[c >> 4];
                    buffer[5] = alphabet[c & 0xf];
                    buffer_size = 6;
                } else
                    buffer[0] = c;

                break;
            }
        }

        size_t written_this_time = io_write(buffer, 1, buffer_size, output);
        written += written_this_time;
        if (written_this_time != buffer_size) {
            if (io) io_set_error(io, io_error(output));
            return written / size;
        }
    }

    return written / size;
}

static int serialize_json_stringpart_internal(IO output, const Binary b) {
    return serialize_json_stringpart_io(b.data, 1, b.length, output, NULL) == b.length? 0: io_error(output);
}

static int serialize_json_string_internal(IO output, const Binary b) {
    if (io_putc('"', output) == EOF ||
        serialize_json_stringpart_internal(output, b) ||
        io_putc('"', output) == EOF)
        return io_error(output);

    return 0;
}

int io_serialize_json(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    while (1) {
        SERIALIZER_DECLARE("JSON", io_serialize_json, 1);

        if (generic_types_compatible_compare(base, container_base_variant_recipe()) == 0) {
            if (variant_is_custom((Variant) data)) {
                /* Variant is custom, just move down a level (to the child value) and restart the serializer */
                base = base->value_child;
                data = variant_get_custom((Variant) data);
                continue;
            } else {
                /* Variant is native, just serialize it */
                switch (variant_get_type((Variant) data)) {
                    case VariantNull: io_puts("null", output); return io_error(output);
                    case VariantBoolean: io_puts(variant_get_boolean((Variant) data)? "true": "false", output); return io_error(output);
                    case VariantInteger: io_printf(output, "%lld", variant_get_int64((Variant) data)); return io_error(output);
                    case VariantUnsignedInteger: io_printf(output, "%llu", variant_get_uint64((Variant) data)); return io_error(output);
                    case VariantFloat: {
                        double flt = variant_get_float((Variant) data);
                        if (fabs(flt) == (double) INFINITY || isnan(flt))
                            return CC_ENOTSUP;

                        io_printf(output, "%.*g", DBL_DIG-1, flt);

                        return io_error(output);
                    }
                    case VariantString: {
                        Binary b = variant_to_binary((Variant) data, NULL);

                        return serialize_json_string_internal(output, b);
                    }
                    case VariantBinary:
                    default:
                        return CC_ENOTSUP;
                }
            }
        }

        if (base->collection_begin && base->collection_next && base->collection_get_value) { /* A collection of some sort */
            if (base->collection_get_key) { /* An object */
                /* Ensure keys are C-strings */
                if (generic_types_compatible_compare(base->key_child, container_base_cstring_recipe()) != 0)
                    return CC_ENOTSUP;

                if (io_putc('{', output) == EOF)
                    return io_error(output);

                Iterator start = base->collection_begin(data);
                for (Iterator it = start; it; it = base->collection_next(data, it)) {
                    if (it != start && io_putc(',', output) == EOF)
                        return io_error(output);

                    const void *key = base->collection_get_key(data, it);
                    const void *value = base->collection_get_value(data, it);

                    int err = io_serialize_json(output, key, base->key_child, NULL);
                    if (err)
                        return err;

                    if (io_putc(':', output) == EOF)
                        return io_error(output);

                    err = io_serialize_json(output, value, base->value_child, NULL);
                    if (err)
                        return err;
                }

                if (io_putc('}', output) == EOF)
                    return io_error(output);
            } else { /* An array */
                if (io_putc('[', output) == EOF)
                    return io_error(output);

                Iterator start = base->collection_begin(data);
                for (Iterator it = start; it; it = base->collection_next(data, it)) {
                    if (it != start && io_putc(',', output) == EOF)
                        return io_error(output);

                    const void *value = base->collection_get_value(data, it);

                    int err = io_serialize_json(output, value, base->value_child, NULL);
                    if (err)
                        return err;
                }

                if (io_putc(']', output) == EOF)
                    return io_error(output);
            }
        } else { /* An atom */
            if (generic_types_compatible_compare(base, container_base_cstring_recipe()) == 0) {
                Binary b = {.data = (char *) data, .length = strlen(data)};
                return serialize_json_string_internal(output, b);
            } else if (generic_types_compatible_compare(base, container_base_boolean_recipe()) == 0) {
                const _Bool value = *((const _Bool *) data);
                io_puts(value? "true": "false", output);
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_char_recipe()) == 0) {
                const signed char value = *((const signed char *) data);
                io_printf(output, "%hhd", value);
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_uchar_recipe()) == 0) {
                const unsigned char value = *((const unsigned char *) data);
                io_printf(output, "%hhu", value);
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_short_recipe()) == 0) {
                const short value = *((const short *) data);
                io_printf(output, "%hd", value);
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_ushort_recipe()) == 0) {
                const unsigned short value = *((const unsigned short *) data);
                io_printf(output, "%hu", value);
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_int_recipe()) == 0) {
                const int value = *((const int *) data);
                io_printf(output, "%d", value);
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_uint_recipe()) == 0) {
                const unsigned int value = *((const unsigned int *) data);
                io_printf(output, "%u", value);
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_long_recipe()) == 0) {
                const long value = *((const long *) data);
                io_printf(output, "%ld", value);
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_ulong_recipe()) == 0) {
                const unsigned long value = *((const unsigned long *) data);
                io_printf(output, "%lu", value);
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_long_long_recipe()) == 0) {
                const long long value = *((const long long *) data);
                io_printf(output, "%lld", value);
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_ulong_long_recipe()) == 0) {
                const unsigned long long value = *((const unsigned long long *) data);
                io_printf(output, "%llu", value);
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_float_recipe()) == 0) {
                const float flt = *((const float *) data);
                if (fabsf(flt) == INFINITY || isnan(flt))
                    return CC_ENOTSUP;

                io_printf(output, "%.*g", FLT_DIG-1, flt);

                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_double_recipe()) == 0) {
                const double flt = *((const double *) data);
                if (fabs(flt) == (double) INFINITY || isnan(flt))
                    return CC_ENOTSUP;

                io_printf(output, "%.*g", DBL_DIG-1, flt);

                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_long_double_recipe()) == 0) {
                const long double flt = *((const long double *) data);
                if (fabsl(flt) == (long double) INFINITY || isnan(flt))
                    return CC_ENOTSUP;

                io_printf(output, "%.*Lg", LDBL_DIG-1, flt);

                return io_error(output);
            } else {
                return CC_ENOTSUP;
            }
        }

        return 0;
    }
}

StringList stringlist_split_io(IO input, const char *separator, int keep_empty) {
    Buffer data_buffer;
    size_t separator_matched = 0, separator_len = strlen(separator); /* If non-zero, contains number of separator characters matched */
    StringList list = stringlist_create();
    if (list == NULL) {
        stringlist_destroy(list);
        return NULL;
    }

    buffer_init(&data_buffer);

    while (1) {
        int ch = io_getc(input);
        if (ch == EOF)
            break;

        char c = ch;

try_match_again:
        if (separator[separator_matched] == c) {
            ++separator_matched;

            if (separator_matched == separator_len) {
                if ((keep_empty || data_buffer.length) && stringlist_append(list, data_buffer.data))
                    goto cleanup;

                buffer_clear(&data_buffer);
                separator_matched = 0;
            }

            continue;
        } else if (separator_matched) {
            if (buffer_append_n(&data_buffer, separator, separator_matched))
                goto cleanup;

            separator_matched = 0;
            goto try_match_again;
        }

        if (buffer_append_n(&data_buffer, &c, 1))
            goto cleanup;
    }

    if (separator_matched && buffer_append_n(&data_buffer, separator, separator_matched))
        goto cleanup;

    if (keep_empty || data_buffer.length) {
        if (stringlist_append(list, data_buffer.data))
            goto cleanup;
    }

    buffer_destroy(&data_buffer);
    return list;

cleanup:
    buffer_destroy(&data_buffer);
    stringlist_destroy(list);
    return NULL;
}

GenericList binarylist_split_io(IO input, const char *separator, size_t separator_len, int keep_empty) {
    Buffer data_buffer;
    size_t separator_matched = 0; /* If non-zero, contains number of separator characters matched */
    GenericList list = genericlist_create(container_base_binary_recipe());
    if (list == NULL) {
        genericlist_destroy(list);
        return NULL;
    }

    buffer_init(&data_buffer);

    while (1) {
        int ch = io_getc(input);
        if (ch == EOF)
            break;

        char c = ch;

try_match_again:
        if (separator[separator_matched] == c) {
            ++separator_matched;

            if (separator_matched == separator_len) {
                Binary b = {.data = data_buffer.data, .length = data_buffer.length};
                if ((keep_empty || data_buffer.length) && genericlist_append(list, &b))
                    goto cleanup;

                buffer_clear(&data_buffer);
                separator_matched = 0;
            }

            continue;
        } else if (separator_matched) {
            if (buffer_append_n(&data_buffer, separator, separator_matched))
                goto cleanup;

            separator_matched = 0;
            goto try_match_again;
        }

        if (buffer_append_n(&data_buffer, &c, 1))
            goto cleanup;
    }

    if (separator_matched && buffer_append_n(&data_buffer, separator, separator_matched))
        goto cleanup;

    if (keep_empty || data_buffer.length) {
        Binary b = {.data = data_buffer.data, .length = data_buffer.length};
        if (genericlist_append(list, &b))
            goto cleanup;
    }

    buffer_destroy(&data_buffer);
    return list;

cleanup:
    buffer_destroy(&data_buffer);
    genericlist_destroy(list);
    return NULL;
}

StringList stringlist_divide_io(IO input, size_t record_size, int keep_partial) {
    char *buffer = MALLOC(record_size+1);
    StringList list = stringlist_create();
    if (buffer == NULL || list == NULL)
        goto cleanup;

    size_t read = 0;
    do {
        read = io_read(buffer, 1, record_size, input);
        buffer[read] = 0;

        if (read == record_size) { /* Full record */
            if (strlen(buffer) != record_size) {
                io_set_error(input, CC_EBADMSG);
                goto cleanup;
            }

            if (stringlist_append(list, buffer))
                goto cleanup;
        } else if (io_error(input)) {
            goto cleanup;
        } else if (read > 0 && keep_partial) {
            if (stringlist_append(list, buffer))
                goto cleanup;
        }
    } while (read == record_size);

    FREE(buffer);

    return list;

cleanup:
    FREE(buffer);
    stringlist_destroy(list);
    return NULL;
}

GenericList binarylist_divide_io(IO input, size_t record_size, int keep_partial) {
    char *buffer = MALLOC(record_size);
    GenericList list = genericlist_create(container_base_binary_recipe());
    if (buffer == NULL || list == NULL)
        goto cleanup;

    size_t read = 0;
    do {
        read = io_read(buffer, 1, record_size, input);
        Binary b = {.data = buffer, .length = read};

        if (read == record_size) { /* Full record */
            if (genericlist_append(list, &b))
                goto cleanup;
        } else if (io_error(input)) {
            goto cleanup;
        } else if (read > 0 && keep_partial) {
            if (genericlist_append(list, &b))
                goto cleanup;
        }
    } while (read == record_size);

    FREE(buffer);

    return list;

cleanup:
    FREE(buffer);
    genericlist_destroy(list);
    return NULL;
}

int stringlist_join_io(IO output, StringList list, const char *separator) {
    Iterator it = stringlist_begin(list);

    while (it) {
        if (io_puts(stringlist_value_of(list, it), output))
            return io_error(output);

        it = stringlist_next(list, it);

        if (it && io_puts(separator, output))
            return io_error(output);
    }

    return 0;
}

int genericlist_join_io(IO output, GenericList list, const char *separator) {
    return genericlist_join_io_n(output, list, separator, strlen(separator));
}

int genericlist_join_io_n(IO output, GenericList list, const char *separator, size_t separator_len) {
    if (generic_types_compatible_compare(genericlist_get_container_base(list), container_base_cstring_recipe()) == 0) {
        Iterator it = genericlist_begin(list);

        while (it) {
            if (io_puts(genericlist_value_of(list, it), output))
                return io_error(output);

            it = genericlist_next(list, it);

            if (it && io_write(separator, 1, separator_len, output) != separator_len)
                return io_error(output);
        }

        return 0;
    } else if (generic_types_compatible_compare(genericlist_get_container_base(list), container_base_binary_recipe()) == 0) {
        Iterator it = genericlist_begin(list);

        while (it) {
            Binary item = *((Binary *) genericlist_value_of(list, it));

            if (io_write(item.data, 1, item.length, output) != item.length)
                return io_error(output);

            it = genericlist_next(list, it);

            if (it && io_write(separator, 1, separator_len, output) != separator_len)
                return io_error(output);
        }

        return 0;
    }

    return CC_EINVAL;
}