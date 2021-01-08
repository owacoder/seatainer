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

#include <time.h>

int io_parser_nested_parse_and_list_insert(void *input, void *container, const CommonContainerBase *base, Parser nested_parser, struct ParserIdentity *type) {
    if (base->value_child->size > sizeof(void*)) { /* Large POD type */
        if (base->collection_insert.list.copy == NULL)
            return CC_EINVAL;

        char *item = MALLOC(base->size);
        if (item == NULL)
            return CC_ENOMEM;

        int err = nested_parser(input, item, base->value_child, type);
        if (!err)
            err = base->collection_insert.list.copy(container, item, NULL);

        FREE(item);
        if (err)
            return err;
    } else if (base->value_child->size) { /* Small POD type */
        if (base->collection_insert.list.copy == NULL)
            return CC_EINVAL;

        void *item[1];

        int err = io_parse_json(input, item, base->value_child, type);
        if (!err)
            err = base->collection_insert.list.copy(container, item, NULL);

        if (err)
            return err;
    } else { /* Non-POD type */
        if (base->collection_insert.list.move == NULL)
            return CC_EINVAL;

        void *handle = NULL;

        int err = io_parse_json(input, &handle, base->value_child, type);
        if (!err)
            err = base->collection_insert.list.move(container, handle, NULL);

        if (err) {
            if (base->deleter)
                base->deleter(handle);

            return err;
        }
    }

    return 0;
}

StringList io_split_to_stringlist(IO input, const char *separator, int keep_empty) {
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

GenericList io_split_to_binarylist(IO input, const char *separator, size_t separator_len, int keep_empty) {
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

StringList io_divide_to_stringlist(IO input, size_t record_size, int keep_partial) {
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

GenericList io_divide_to_binarylist(IO input, size_t record_size, int keep_partial) {
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

int io_join_stringlist(IO output, StringList list, const char *separator) {
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

int io_join_genericlist(IO output, GenericList list, const char *separator) {
    return io_join_genericlist_n(output, list, separator, strlen(separator));
}

int io_join_genericlist_n(IO output, GenericList list, const char *separator, size_t separator_len) {
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

    const char *str = *((const _Bool *) data)? "true": "false";
    const size_t len = strlen(str);

    type->written = len;

    return io_write(str, 1, len, output) != len? io_error(output): 0;
}

int io_serialize_char(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_char, 1);

    if (generic_types_compatible_compare(base, container_base_char_recipe()) != 0)
        return CC_ENOTSUP;

    int result = io_printf(output, "%.1s", data) < 0;
    if (result < 0)
        return io_error(output);

    type->written = result;
    return 0;
}

int io_serialize_short(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_short, 1);

    if (generic_types_compatible_compare(base, container_base_short_recipe()) != 0)
        return CC_ENOTSUP;

    int result = io_printf(output, "%hd", *((const short *) data));
    if (result < 0)
        return io_error(output);

    type->written = result;
    return 0;
}

int io_serialize_ushort(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_ushort, 1);

    if (generic_types_compatible_compare(base, container_base_ushort_recipe()) != 0)
        return CC_ENOTSUP;

    int result = io_printf(output, "%hu", *((const unsigned short *) data));
    if (result < 0)
        return io_error(output);

    type->written = result;
    return 0;
}

int io_serialize_int(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_int, 1);

    if (generic_types_compatible_compare(base, container_base_int_recipe()) != 0)
        return CC_ENOTSUP;

    int result = io_printf(output, "%d", *((const int *) data));
    if (result < 0)
        return io_error(output);

    type->written = result;
    return 0;
}

int io_serialize_uint(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_uint, 1);

    if (generic_types_compatible_compare(base, container_base_uint_recipe()) != 0)
        return CC_ENOTSUP;

    int result = io_printf(output, "%u", *((const unsigned int *) data));
    if (result < 0)
        return io_error(output);

    type->written = result;
    return 0;
}

int io_serialize_long(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_long, 1);

    if (generic_types_compatible_compare(base, container_base_long_recipe()) != 0)
        return CC_ENOTSUP;

    int result = io_printf(output, "%ld", *((const long *) data));
    if (result < 0)
        return io_error(output);

    type->written = result;
    return 0;
}

int io_serialize_ulong(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_ulong, 1);

    if (generic_types_compatible_compare(base, container_base_ulong_recipe()) != 0)
        return CC_ENOTSUP;

    int result = io_printf(output, "%lu", *((const unsigned long *) data));
    if (result < 0)
        return io_error(output);

    type->written = result;
    return 0;
}

int io_serialize_long_long(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_long_long, 1);

    if (generic_types_compatible_compare(base, container_base_long_long_recipe()) != 0)
        return CC_ENOTSUP;

    int result = io_printf(output, "%lld", *((const long long *) data));
    if (result < 0)
        return io_error(output);

    type->written = result;
    return 0;
}

int io_serialize_ulong_long(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_ulong_long, 1);

    if (generic_types_compatible_compare(base, container_base_ulong_long_recipe()) != 0)
        return CC_ENOTSUP;

    int result = io_printf(output, "%llu", *((const unsigned long long *) data));
    if (result < 0)
        return io_error(output);

    type->written = result;
    return 0;
}

int io_serialize_size_t(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_size_t, 1);

    if (generic_types_compatible_compare(base, container_base_size_t_recipe()) != 0)
        return CC_ENOTSUP;

    int result = io_printf(output, "%zu", *((const size_t *) data));
    if (result < 0)
        return io_error(output);

    type->written = result;
    return 0;
}

int io_serialize_float(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_float, 1);

    if (generic_types_compatible_compare(base, container_base_float_recipe()) != 0)
        return CC_ENOTSUP;

    int result = io_printf(output, "%.*g", FLT_DIG-1, *((const float *) data));
    if (result < 0)
        return io_error(output);

    type->written = result;
    return 0;
}

int io_serialize_double(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_double, 1);

    if (generic_types_compatible_compare(base, container_base_double_recipe()) != 0)
        return CC_ENOTSUP;

    int result = io_printf(output, "%.*g", DBL_DIG-1, *((const double *) data));
    if (result < 0)
        return io_error(output);

    type->written = result;
    return 0;
}

int io_serialize_long_double(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_long_double, 1);

    if (generic_types_compatible_compare(base, container_base_long_double_recipe()) != 0)
        return CC_ENOTSUP;

    int result = io_printf(output, "%.*Lg", LDBL_DIG-1, *((const long double *) data));
    if (result < 0)
        return io_error(output);

    type->written = result;
    return 0;
}

int io_serialize_clock_t(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_clock_t, 1);

    if (generic_types_compatible_compare(base, container_base_clock_t_recipe()) != 0)
        return CC_ENOTSUP;

    int result = io_printf(output, "%.*g", DBL_DIG-1, (clock() - *((const clock_t *) data)) / (double) CLOCKS_PER_SEC);
    if (result < 0)
        return io_error(output);

    type->written = result;
    return 0;
}

int io_serialize_tm(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_tm, 1);

    if (generic_types_compatible_compare(base, container_base_tm_recipe()) != 0)
        return CC_ENOTSUP;

    const struct tm *t = data;
    const char wday[][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char mname[][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    int result = io_printf(output, "%.3s %.3s%3d %.2d:%.2d:%.2d %d",
                             wday[t->tm_wday],
                             mname[t->tm_mon],
                             t->tm_mday,
                             t->tm_hour,
                             t->tm_min,
                             t->tm_sec,
                             t->tm_year + 1900);
    if (result < 0)
        return io_error(output);

    type->written = result;
    return 0;
}

int io_serialize_cstring(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_cstring, 1);

    if (generic_types_compatible_compare(base, container_base_cstring_recipe()) != 0)
        return CC_ENOTSUP;

    size_t len = strlen(data);
    if (io_write(data, 1, len, output) != len)
        return io_error(output);

    type->written = len;
    return 0;
}

int io_serialize_binary(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_binary, 1);

    if (generic_types_compatible_compare(base, container_base_binary_recipe()) != 0)
        return CC_ENOTSUP;

    const Binary *binary = data;

    for (size_t i = 0; i < binary->length; ++i) {
        unsigned char chr = binary->data[i];

        if (chr > 0x80 || chr < 0x20) {
            int result = io_printf(output, "\\x%.2hhx", chr);
            if (result < 0)
                return io_error(output);

            type->written += result;
        } else if (io_putc(chr, output) == EOF) {
            return io_error(output);
        } else {
            ++type->written;
        }
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
                data = variant_get_custom_data(v);
                base = variant_get_custom_container_base(v);
                continue;
            case VariantUndefined:
            case VariantNull: {
                const char *str = variant_get_type(v) == VariantUndefined? "<undefined>": "<null>";
                const size_t len = strlen(str);
                if (io_write(str, 1, len, output) != len)
                    return io_error(output);

                type->written = len;
                return 0;
            }
            case VariantBoolean: {
                _Bool bool = variant_get_boolean(v);
                return io_serialize_boolean(output, &bool, container_base_boolean_recipe(), type);
            }
            case VariantInteger: {
                long long i = variant_get_int64(v);
                return io_serialize_long_long(output, &i, container_base_long_long_recipe(), type);
            }
            case VariantUnsignedInteger: {
                unsigned long long i = variant_get_uint64(v);
                return io_serialize_ulong_long(output, &i, container_base_ulong_long_recipe(), type);
            }
            case VariantFloat: {
                double d = variant_get_float(v);
                return io_serialize_double(output, &d, container_base_double_recipe(), type);
            }
            case VariantString: {
                const char *s = variant_get_string(v);
                return io_serialize_double(output, s, container_base_cstring_recipe(), type);
            }
            case VariantBinary: {
                Binary b = variant_get_binary(v);
                return io_serialize_binary(output, &b, container_base_binary_recipe(), type);
            }
            default:
                return CC_ENOTSUP;
        }
    }
}

int io_serialize_list_join(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    int is_utf8 = 0;

    Serializer value_serialize = base && base->value_child? base->value_child->serialize: NULL;
    if (value_serialize == NULL && base)
        value_serialize = io_default_serializer_for_type(base->value_child);

    if (value_serialize) {
        struct SerializerIdentity subtype = {0};
        value_serialize(NULL, NULL, NULL, &subtype);
        is_utf8 = subtype.is_utf8;
    }

    SERIALIZER_DECLARE("list-join", io_serialize_list_join, is_utf8);
    struct SerializerIdentity subtype = *type;

    if (base->collection_begin == NULL ||
            base->collection_next == NULL ||
            base->collection_get_value == NULL ||
            value_serialize == NULL)
        return CC_ENOTSUP;

    Iterator start = base->collection_begin(data);
    for (Iterator it = start; it; it = base->collection_next(data, it)) {
        if (it != start && type->fmt_length) {
            if (io_write(type->fmt, 1, type->fmt_length, output) != type->fmt_length)
                return io_error(output);
            else
                type->written += type->fmt_length;
        }

        {
            int err = value_serialize(output, base->collection_get_value(data, it), base->value_child, &subtype);
            if (err)
                return err;
            else
                type->written += subtype.written;
        }
    }

    return 0;
}

int io_serialize_utf8(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("UTF-8", io_serialize_utf8, 1);
    struct SerializerIdentity subtype = *type;

    if (base->collection_begin == NULL ||
            base->collection_next == NULL ||
            base->collection_get_value == NULL) {
        Serializer serialize = io_default_serializer_for_type(base);
        if (serialize == NULL)
            return CC_ENOTSUP;

        return serialize(output, data, base, type);
    }

    if (base->collection_get_key) {
        if (io_putc('{', output) == EOF)
            return io_error(output);
        else
            ++type->written;

        Iterator start = base->collection_begin(data);
        for (Iterator it = start; it; it = base->collection_next(data, it)) {
            /* Write pair separator if necessary */
            if (it != start) {
                const char *sep = ", ";
                const size_t len = strlen(sep);
                if (io_write(sep, 1, len, output) != len)
                    return io_error(output);
                else
                    type->written += len;
            }

            /* Write key */
            {
                int err = io_serialize_utf8(output, base->collection_get_key(data, it), base->key_child, &subtype);
                if (err)
                    return err;
                else
                    type->written += subtype.written;
            }

            /* Write separator */
            {
                const char *key_sep = ", ";
                const size_t len = strlen(key_sep);
                if (io_write(key_sep, 1, len, output) != len)
                    return io_error(output);
                else
                    type->written += len;
            }

            /* Write value */
            {
                int err = io_serialize_utf8(output, base->collection_get_value(data, it), base->value_child, &subtype);
                if (err)
                    return err;
                else
                    type->written += subtype.written;
            }
        }

        if (io_putc('}', output) == EOF)
            return io_error(output);
        else
            ++type->written;
    } else {
        if (io_putc('[', output) == EOF)
            return io_error(output);
        else
            ++type->written;

        Iterator start = base->collection_begin(data);
        for (Iterator it = start; it; it = base->collection_next(data, it)) {
            if (it != start) {
                const char *sep = ", ";
                const size_t len = strlen(sep);
                if (io_write(sep, 1, len, output) != len)
                    return io_error(output);
                else
                    type->written += len;
            }

            {
                int err = io_serialize_utf8(output, base->collection_get_value(data, it), base->value_child, &subtype);
                if (err)
                    return err;
                else
                    type->written += subtype.written;
            }
        }

        if (io_putc(']', output) == EOF)
            return io_error(output);
        else
            ++type->written;
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
        return (Serializer) io_serialize_utf8;
    else
        return NULL; /* Unknown default serializer type */
}

int io_parse_json_unicode_escape(IO input, unsigned long *codepoint) {
    const char *alphabet = "0123456789abcdef";
    unsigned char buf[16];

    *codepoint = 0;

    if (io_read(buf, 1, 4, input) != 4)
        return io_error(input);

    for (size_t i = 0; i < 4; ++i) {
        char *pos = strchr(alphabet, tolower(buf[i]));
        if (!pos)
            return CC_EBADMSG;

        *codepoint = (*codepoint << 4) + (unsigned int) (pos - alphabet);
    }

    return 0;
}

/** Assumes no part of the number has been parsed. If integral, the double is set to NAN */
int io_parse_json_number(IO input, double *d, signed long long *i, unsigned long long *u) {
    Buffer buffer;
    int err = 0;
    int negative = 0;

    buffer_init(&buffer);

    int ch = io_getc(input);
    if (!isdigit(ch) && ch != '-')
        goto cleanup;

    negative = ch == '-';
    if (negative) {
        if ((err = buffer_append_chr(&buffer, ch)) != 0)
            goto cleanup;

        ch = io_getc(input);
        if (!isdigit(ch))
            goto cleanup;
    }

    if (ch == '0') { /* JSON spec (https://www.json.org/json-en.html) says only one leading zero ever, with a possible decimal point */
        if ((err = buffer_append_chr(&buffer, ch)) != 0)
            goto cleanup;

        ch = io_getc(input);
        if (isdigit(ch))
            goto cleanup;
    } else {
        do {
            if ((err = buffer_append_chr(&buffer, ch)) != 0)
                goto cleanup;

            ch = io_getc(input);
        } while (isdigit(ch));
    }

    if (ch == 'e' || ch == 'E')
        goto parse_exponent;
    else if (ch == '.') {
        do {
            if ((err = buffer_append_chr(&buffer, ch)) != 0)
                goto cleanup;

            ch = io_getc(input);
        } while (isdigit(ch));

        if (ch == 'e' || ch == 'E') {
parse_exponent:
            if ((err = buffer_append_chr(&buffer, ch)) != 0)
                goto cleanup;

            ch = io_getc(input);
            if (ch != '+' && ch != '-' && !isdigit(ch))
                goto cleanup;

            do {
                if ((err = buffer_append_chr(&buffer, ch)) != 0)
                    goto cleanup;

                ch = io_getc(input);
            } while (isdigit(ch));
            io_ungetc(ch, input);
        } else {
            io_ungetc(ch, input);
        }
    } else {
        io_ungetc(ch, input);

        if (negative) {
            /* Parse as integer */
            char *endptr = NULL;

            errno = 0;
            *d = NAN;
            *i = strtoll(buffer.data, &endptr, 10);
            *u = 0;
            if (!errno && !*endptr)
                goto finish;
        } else {
            /* Parse as integer */
            char *endptr = NULL;

            errno = 0;
            *d = NAN;
            *i = 0;
            *u = strtoull(buffer.data, &endptr, 10);
            if (!errno && !*endptr)
                goto finish;
        }
    }

    /* Parse as double (parsing as integer will fallback to this) */
    char *endptr = NULL;

    *i = 0;
    *u = 0;
    *d = strtod(buffer.data, &endptr);
    if (*endptr) {
        goto cleanup;
    }

    /* Fall back to integral if within range */
    if (trunc(*d) == *d) {
        if (!negative && *d <= ULLONG_MAX) {
            *i = 0;
            *u = (unsigned long long) *d;
            *d = NAN;
        } else if (negative && *d >= LLONG_MIN) {
            *i = (long long) *d;
            *u = 0;
            *d = NAN;
        }
    }

finish:
    buffer_destroy(&buffer);

    return 0;

cleanup:
    buffer_destroy(&buffer);
    return err? err: CC_EBADMSG;
}

/* Assumes initial '"' has already been parsed */
int io_parse_json_string(IO input, Binary *b) {
    unsigned int unicode_surrogate = 0; /* Stores the unicode surrogate we decoded last round */
    Buffer buffer;
    int err = 0;

    buffer_init(&buffer);

    int ch;
    while (1) {
        ch = io_getc(input);
        if (ch == EOF)
            goto cleanup;

        if (ch == '"')
            break;
        else if (ch == '\\') {
            ch = io_getc(input);
            if (ch == EOF)
                goto cleanup;

            switch (ch) {
                case '"':
                case '\\':
                case '/': break;
                case 'b': ch = '\b'; break;
                case 'f': ch = '\f'; break;
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                case 'u': {
                    unsigned long codepoint = 0;
                    unsigned char buf[16];

                    err = io_parse_json_unicode_escape(input, &codepoint);
                    if (err)
                        goto cleanup;

                    if (utf16surrogate(codepoint)) {
                        if (!unicode_surrogate) {
                            unicode_surrogate = codepoint;
                            continue;
                        }

                        codepoint = utf16codepoint(unicode_surrogate, codepoint);
                        if (codepoint > UTF8_MAX)
                            goto cleanup;

                        unicode_surrogate = 0;
                    }

                    size_t len = sizeof(buf);
                    utf8append(buf, codepoint & UTF8_MASK, &len);
                    err = buffer_append(&buffer, buf);
                    if (err)
                        goto cleanup;

                    continue;
                }
            }
        }

        if (unicode_surrogate)
            goto cleanup; /* Unmatched surrogate characters not supported */

        err = buffer_append_chr(&buffer, ch);
        if (err)
            goto cleanup;
    }

    FREE(b->data);
    b->length = buffer.length;
    b->data = buffer_take(&buffer);
    buffer_destroy(&buffer);

    return 0;

cleanup:
    buffer_destroy(&buffer);
    return err? err: io_error(input)? io_error(input): CC_EBADMSG;
}

int io_parse_json(IO input, void *data, const CommonContainerBase *base, struct ParserIdentity *type) {
    PARSER_DECLARE("JSON", io_parse_json, 1);

    int ch;

    if (generic_types_compatible_compare(base, container_base_variant_recipe()) == 0) {
        Variant v = *((Variant *) data);

        if (v == NULL) {
            Variant new_v = variant_create_undefined();
            if (new_v == NULL)
                return CC_ENOMEM;

            int err = io_parse_json(input, &new_v, base, type);
            if (!err)
                *((Variant *) data) = new_v;
            else
                variant_destroy(new_v);
            return err;
        }

        do {
            ch = io_getc(input);
            if (ch == EOF)
                goto cleanup;
        } while (strchr(" \n\r\t", ch));

        enum {
            JsonNull,
            JsonTrue,
            JsonFalse,
            JsonNumber,
            JsonString,
            JsonArray,
            JsonObject
        } json_type = JsonNull;

        /* Detect type of element */
        switch (ch) {
            case 'n': json_type = JsonNull; break;
            case 't': json_type = JsonTrue; break;
            case 'f': json_type = JsonFalse; break;
            case '{': json_type = JsonObject; break;
            case '[': json_type = JsonArray; break;
            case '"': json_type = JsonString; break;
            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9': json_type = JsonNumber; break;
            default: goto cleanup;
        }
        io_ungetc(ch, input);

        if (variant_is_undefined(v)) { /* No expected type */
            int err = 0;

            /* Then parse element type */
            switch (json_type) {
                case JsonNull: {
                    int dummy = 0;
                    err = io_parse_json(input, &dummy, container_base_empty_recipe(), type);
                    if (!err)
                        variant_set_null(v);
                    break;
                }
                case JsonTrue:
                case JsonFalse: {
                    _Bool b = 0;
                    err = io_parse_json(input, &b, container_base_boolean_recipe(), type);
                    if (!err)
                        variant_set_boolean(v, b);
                    break;
                }
                case JsonNumber: {
                    int negative = 0;
                    double d = 0.0;
                    signed long long i = 0;
                    unsigned long long u = 0;
                    err = io_parse_json_number(input, &d, &i, &u);
                    if (!err) {
                        if (!isnan(d))
                            variant_set_float(v, d);
                        else if (i)
                            variant_set_int64(v, i);
                        else
                            variant_set_uint64(v, u);
                    }
                    break;
                }
                case JsonString: {
                    Binary b = {.data = NULL, .length = 0};
                    err = io_parse_json(input, &b, container_base_binary_recipe(), type);
                    if (!err) {
                        if (memchr(b.data, 0, b.length))
                            variant_set_binary_string_binary_move(v, b);
                        else
                            variant_set_string_move(v, b.data);
                    } else
                        FREE(b.data);
                    break;
                }
                case JsonArray: {
                    GenericList gl = genericlist_create(container_base_variant_recipe());
                    if (gl == NULL)
                        return CC_ENOMEM;

                    err = io_parse_json(input, &gl, container_base_variantlist_recipe(), type);
                    if (!err)
                        variant_set_custom_move(v, gl, container_base_variantlist_recipe());
                    else
                        genericlist_destroy(gl);

                    break;
                }
                case JsonObject: {
                    GenericMap gm = genericmap_create(container_base_cstring_recipe(),
                                                      container_base_variant_recipe());
                    if (gm == NULL)
                        return CC_ENOMEM;

                    err = io_parse_json(input, &gm, container_base_variantmap_recipe(), type);
                    if (!err)
                        variant_set_custom_move(v, gm, container_base_variantmap_recipe());
                    else
                        genericmap_destroy(gm);

                    break;
                }
            }

            return err;
        } else { /* Expected type is type currently contained in Variant */
            switch (variant_get_type(v)) {
                default: return CC_EINVAL;
                case VariantNull: {
                    int dummy = 0;
                    int err = io_parse_json(input, &dummy, container_base_empty_recipe(), type);
                    return err;
                }
                case VariantBoolean: {
                    _Bool b = 0;
                    int err = io_parse_json(input, &b, container_base_boolean_recipe(), type);
                    if (!err)
                        variant_set_boolean(v, b);
                    return err;
                }
                case VariantInteger: {
                    long long i = 0;
                    int err = io_parse_json(input, &i, container_base_long_long_recipe(), type);
                    if (!err)
                        variant_set_int64(v, i);
                    return err;
                }
                case VariantUnsignedInteger: {
                    unsigned long long i = 0;
                    int err = io_parse_json(input, &i, container_base_ulong_long_recipe(), type);
                    if (!err)
                        variant_set_uint64(v, i);
                    return err;
                }
                case VariantFloat: {
                    double d = 0.0;
                    int err = io_parse_json(input, &d, container_base_double_recipe(), type);
                    if (!err)
                        variant_set_float(v, d);
                    return err;
                }
                case VariantString: {
                    char *str = NULL;
                    int err = io_parse_json(input, &str, container_base_cstring_recipe(), type);
                    if (!err)
                        variant_set_string_move(v, str);
                    else
                        FREE(str);
                    return err;
                }
                case VariantBinary: {
                    Binary b = {.data = NULL, .length = 0};
                    int err = io_parse_json(input, &b, container_base_binary_recipe(), type);
                    if (!err)
                        variant_set_binary_string_binary_move(v, b);
                    else
                        FREE(b.data);
                    return err;
                }
                case VariantCustom: {
                    void *custom_data = NULL;
                    CommonContainerBase *base_copy = container_base_copy_if_dynamic(variant_get_custom_container_base(v));
                    if (base_copy == NULL)
                        return CC_ENOMEM;

                    int err = io_parse_json(input, &custom_data, base_copy, type);
                    if (!err)
                        variant_set_custom_move_adopt(v, custom_data, base_copy);
                    else
                        container_base_destroy_if_dynamic(base_copy);

                    return err;
                }
            }
        }
    }

    do {
        ch = io_getc(input);
        if (ch == EOF)
            goto cleanup;
    } while (strchr(" \n\r\t", ch));

    if (base->collection_get_key != NULL) { /* Object type */
        int has_item = 0;

        if (ch != '{')
            goto cleanup;

        void *container = *((void **) data);

        if (base->key_child == NULL || base->value_child == NULL)
            return CC_EINVAL;

        if (container == NULL) {
            io_ungetc(ch, input);

            if (base->collection_create.key_value == NULL)
                return CC_EINVAL;

            container = base->collection_create.key_value(base->key_child, base->value_child);
            if (container == NULL)
                return CC_ENOMEM;

            int err = io_parse_json(input, &container, base, type);
            if (!err)
                *((void **) data) = container;
            else if (base->deleter)
                base->deleter(container);

            return err;
        }

        while (1) {
            do {
                ch = io_getc(input);
                if (ch == EOF)
                    goto cleanup;
            } while (strchr(" \n\r\t", ch));

            if (ch == '}')
                break;
            else if (has_item) {
                if (ch != ',')
                    goto cleanup;

                do {
                    ch = io_getc(input);
                    if (ch == EOF)
                        goto cleanup;
                } while (strchr(" \n\r\t", ch));
            }

            if (ch != '"')
                goto cleanup;

            io_ungetc(ch, input);
            has_item = 1;

            /* Parse key of pair */
            int err = 0;
            AllocatedSpace key = allocated_space_for_type(base->key_child);
            AllocatedSpace value = allocated_space_for_type(base->value_child);

            if (!key || !value)
                goto object_cleanup;

            err = io_parse_json(input, key, base->key_child, type);
            if (err)
                goto object_cleanup;

            do {
                ch = io_getc(input);
                if (ch == EOF)
                    goto cleanup;
            } while (strchr(" \n\r\t", ch));

            if (ch != ':')
                goto cleanup;

            err = io_parse_json(input, value, base->value_child, type);
            if (err)
                goto object_cleanup;

            err = base->collection_insert.key_value(container,
                                                    allocated_space_get_object(key, base->key_child),
                                                    allocated_space_get_object(value, base->value_child));
            if (err)
                goto object_cleanup;

            /* Then destroy just the allocated space */
            allocated_space_destroy_after_object_move(key, base->key_child);
            allocated_space_destroy_after_object_move(value, base->value_child);

            continue;
object_cleanup:
            allocated_space_destroy(key, base->key_child);
            allocated_space_destroy(value, base->value_child);
            return err? err: CC_EBADMSG;
        }

        /* Parse object into recipient type */
    } else if (base->collection_get_value != NULL) { /* Array type */
        int has_item = 0;

        if (ch != '[')
            goto cleanup;

        void *container = *((void **) data);

        if (container == NULL) {
            io_ungetc(ch, input);

            if (base->collection_create.list == NULL)
                return CC_EINVAL;

            container = base->collection_create.list(base->value_child);
            if (container == NULL)
                return CC_ENOMEM;

            int err = io_parse_json(input, &container, base, type);
            if (!err)
                *((void **) data) = container;
            else if (base->deleter)
                base->deleter(container);

            return err;
        }

        while (1) {
            do {
                ch = io_getc(input);
                if (ch == EOF)
                    goto cleanup;
            } while (strchr(" \n\r\t", ch));

            if (ch == ']')
                break;
            else if (has_item) {
                if (ch != ',')
                    goto cleanup;
            } else {
                io_ungetc(ch, input);
            }
            has_item = 1;

            int err = io_parser_nested_parse_and_list_insert(input, container, base, io_parse_json, type);
            if (err)
                return err;
        }
    } else if (generic_types_compatible_compare(base, container_base_cstring_recipe()) == 0) {
        if (ch != '"')
            goto cleanup;

        Binary b = {.data = NULL, .length = 0};
        int err = io_parse_json_string(input, &b);
        if (err || memchr(b.data, 0, b.length))
            goto cleanup;

        char **cstr = (char **) data;

        FREE(*cstr);
        *cstr = b.data;

        return 0;
    } else if (generic_types_compatible_compare(base, container_base_binary_recipe()) == 0) {
        if (ch != '"')
            goto cleanup;

        return io_parse_json_string(input, data);
    } else if (generic_types_compatible_compare(base, container_base_boolean_recipe()) == 0) {
        if (ch != 't' && ch != 'f')
            goto cleanup;

        if (io_match(input, ch == 't'? "rue": "alse"))
            goto cleanup;

        _Bool *b = (_Bool *) data;
        *b = ch == 't';
    } else if (generic_types_compatible_compare(base, container_base_short_recipe()) == 0) {
        if (ch != '-' && !isdigit(ch))
            goto cleanup;
        io_ungetc(ch, input);

        if (io_scanf(input, "%hd", data) != 1)
            goto cleanup;
    } else if (generic_types_compatible_compare(base, container_base_ushort_recipe()) == 0) {
        if (ch != '-' && !isdigit(ch))
            goto cleanup;
        io_ungetc(ch, input);

        if (io_scanf(input, "%hu", data) != 1)
            goto cleanup;
    } else if (generic_types_compatible_compare(base, container_base_int_recipe()) == 0) {
        if (ch != '-' && !isdigit(ch))
            goto cleanup;
        io_ungetc(ch, input);

        if (io_scanf(input, "%d", data) != 1)
            goto cleanup;
    } else if (generic_types_compatible_compare(base, container_base_uint_recipe()) == 0) {
        if (ch != '-' && !isdigit(ch))
            goto cleanup;
        io_ungetc(ch, input);

        if (io_scanf(input, "%u", data) != 1)
            goto cleanup;
    } else if (generic_types_compatible_compare(base, container_base_long_recipe()) == 0) {
        if (ch != '-' && !isdigit(ch))
            goto cleanup;
        io_ungetc(ch, input);

        if (io_scanf(input, "%ld", data) != 1)
            goto cleanup;
    } else if (generic_types_compatible_compare(base, container_base_ulong_recipe()) == 0) {
        if (ch != '-' && !isdigit(ch))
            goto cleanup;
        io_ungetc(ch, input);

        if (io_scanf(input, "%lu", data) != 1)
            goto cleanup;
    } else if (generic_types_compatible_compare(base, container_base_long_long_recipe()) == 0) {
        if (ch != '-' && !isdigit(ch))
            goto cleanup;
        io_ungetc(ch, input);

        if (io_scanf(input, "%lld", data) != 1)
            goto cleanup;
    } else if (generic_types_compatible_compare(base, container_base_ulong_long_recipe()) == 0) {
        if (ch != '-' && !isdigit(ch))
            goto cleanup;
        io_ungetc(ch, input);

        if (io_scanf(input, "%llu", data) != 1)
            goto cleanup;
    } else if (generic_types_compatible_compare(base, container_base_float_recipe()) == 0) {
        if (ch != '-' && !isdigit(ch))
            goto cleanup;
        io_ungetc(ch, input);

        if (io_scanf(input, "%g", data) != 1)
            goto cleanup;
    } else if (generic_types_compatible_compare(base, container_base_double_recipe()) == 0) {
        if (ch != '-' && !isdigit(ch))
            goto cleanup;
        io_ungetc(ch, input);

        if (io_scanf(input, "%lg", data) != 1)
            goto cleanup;
    } else if (generic_types_compatible_compare(base, container_base_long_double_recipe()) == 0) {
        if (ch != '-' && !isdigit(ch))
            goto cleanup;
        io_ungetc(ch, input);

        if (io_scanf(input, "%Lg", data) != 1)
            goto cleanup;
    } else if (generic_types_compatible_compare(base, container_base_empty_recipe()) == 0) {
        if (ch != 'n' || io_match(input, "ull"))
            goto cleanup;
    }

    return 0;

cleanup:
    return io_error(input)? io_error(input): CC_EBADMSG;
}

static size_t serialize_json_stringpart(int ascii_only, const char *data, size_t length, IO io) {
    char buffer[16];
    size_t written = 0;

    while (length) {
        buffer[0] = '\\';
        size_t buffer_size = 1;
        unsigned char c = *data;

        switch (c) {
            case '"':  buffer[1] =  c ; buffer_size = 2; ++data; --length; break;
            case '\b': buffer[1] = 'b'; buffer_size = 2; ++data; --length; break;
            case '\f': buffer[1] = 'f'; buffer_size = 2; ++data; --length; break;
            case '\n': buffer[1] = 'n'; buffer_size = 2; ++data; --length; break;
            case '\r': buffer[1] = 'r'; buffer_size = 2; ++data; --length; break;
            case '\t': buffer[1] = 't'; buffer_size = 2; ++data; --length; break;
            default: {
                if (c < 0x20 || c >= 0x80) {
                    const char alphabet[] = "0123456789ABCDEF";
                    const char *data_save = data;
                    unsigned long codepoint = utf8next_n(data, &length, &data);

                    if (codepoint > UTF8_MAX) { /* TODO: shouldn't map to a unicode escape since that will be decoded to more than one byte */
                        buffer[1] = 'u';
                        buffer[2] = '0';
                        buffer[3] = '0';
                        buffer[4] = alphabet[c >> 4];
                        buffer[5] = alphabet[c & 0xf];
                        buffer_size = 6;
                    } else if (ascii_only) {
                        if (codepoint > 0xffff) {
                            unsigned int high, low;
                            utf16surrogates(codepoint, &high, &low);

                            buffer[1] = 'u';
                            buffer[2] = alphabet[(high >> 12) & 0xf];
                            buffer[3] = alphabet[(high >>  8) & 0xf];
                            buffer[4] = alphabet[(high >>  4) & 0xf];
                            buffer[5] = alphabet[(high >>  0) & 0xf];
                            buffer[6] = '\\';
                            buffer[7] = 'u';
                            buffer[8] = alphabet[(low >> 12) & 0xf];
                            buffer[9] = alphabet[(low >>  8) & 0xf];
                            buffer[10] = alphabet[(low >>  4) & 0xf];
                            buffer[11] = alphabet[(low >>  0) & 0xf];
                            buffer_size = 12;
                        } else {
                            buffer[1] = 'u';
                            buffer[2] = alphabet[(codepoint >> 12) & 0xf];
                            buffer[3] = alphabet[(codepoint >>  8) & 0xf];
                            buffer[4] = alphabet[(codepoint >>  4) & 0xf];
                            buffer[5] = alphabet[(codepoint >>  0) & 0xf];
                            buffer_size = 6;
                        }
                    } else {
                        buffer_size = data - data_save;
                        memcpy(buffer, data_save, buffer_size);
                    }
                } else {
                    buffer[0] = c;
                    ++data;
                    --length;
                }

                break;
            }
        }

        size_t written_this_time = io_write(buffer, 1, buffer_size, io);
        written += written_this_time;
        if (written_this_time != buffer_size) {
            return written;
        }
    }

    return written;
}

static size_t serialize_json_stringpart_internal(int ascii_only, IO output, const Binary b) {
    return serialize_json_stringpart(ascii_only, b.data, b.length, output);
}

static size_t serialize_json_string_internal(int ascii_only, IO output, const Binary b) {
    size_t written = 1;

    if (io_putc('"', output) == EOF)
        return 0;

    written += serialize_json_stringpart_internal(ascii_only, output, b);
    if (io_error(output))
        return written;

    if (io_putc('"', output) == EOF)
        return written;

    return written + 1;
}

int io_serialize_json(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type) {
    while (1) {
        SERIALIZER_DECLARE("JSON", io_serialize_json, 1);

        const int ascii_only = memstr(type->fmt, type->fmt_length, "ASCII") != NULL;

        if (generic_types_compatible_compare(base, container_base_variant_recipe()) == 0) {
            Variant v = (Variant) data;

            if (variant_is_custom(v)) {
                /* Variant is custom, just move down a level (to the child value) and restart the serializer */
                data = variant_get_custom_data(v);
                base = variant_get_custom_container_base(v);
                continue;
            } else {
                /* Variant is native, just serialize it */
                switch (variant_get_type(v)) {
                    case VariantUndefined:
                    case VariantNull: io_puts("null", output); type->written = 4; return io_error(output);
                    case VariantBoolean: io_puts(variant_get_boolean(v)? "true": "false", output); type->written = 4 + !variant_get_boolean(v); return io_error(output);
                    case VariantInteger: type->written = abs(io_printf(output, "%lld", variant_get_int64(v))); return io_error(output);
                    case VariantUnsignedInteger: type->written = abs(io_printf(output, "%llu", variant_get_uint64(v))); return io_error(output);
                    case VariantFloat: {
                        double flt = variant_get_float(v);
                        if (fabs(flt) == (double) INFINITY || isnan(flt))
                            return CC_ENOTSUP;

                        type->written = abs(io_printf(output, "%.*g", DBL_DIG-1, flt));

                        return io_error(output);
                    }
                    case VariantString:
                    case VariantBinary: {
                        Binary b = variant_to_binary(v, NULL);

                        type->written = serialize_json_string_internal(ascii_only, output, b);

                        return io_error(output);
                    }
                    default:
                        return CC_ENOTSUP;
                }
            }
        }

        if (base->collection_begin && base->collection_next && base->collection_get_value) { /* A collection of some sort */
            if (base->collection_get_key) { /* An object */
                /* Ensure keys are C-strings or Binary */
                if (generic_types_compatible_compare(base->key_child, container_base_cstring_recipe()) != 0 &&
                    generic_types_compatible_compare(base->key_child, container_base_binary_recipe()) != 0)
                    return CC_ENOTSUP;

                const int values_are_variants = generic_types_compatible_compare(base->value_child, container_base_variant_recipe()) == 0;

                if (io_putc('{', output) == EOF)
                    return io_error(output);
                ++type->written;

                Iterator start = base->collection_begin(data);
                for (Iterator it = start; it; it = base->collection_next(data, it)) {
                    const void *key = base->collection_get_key(data, it);
                    const void *value = base->collection_get_value(data, it);

                    if (values_are_variants && variant_is_undefined((Variant) value)) /* If value is undefined, omit it from the JSON output */
                        continue;

                    if (it != start) {
                        if (io_putc(',', output) == EOF)
                            return io_error(output);
                        ++type->written;
                    }

                    struct SerializerIdentity identity_subtype = *type;
                    int err = io_serialize_json(output, key, base->key_child, &identity_subtype);
                    if (err)
                        return err;
                    type->written += identity_subtype.written;

                    if (io_putc(':', output) == EOF)
                        return io_error(output);
                    ++type->written;

                    err = io_serialize_json(output, value, base->value_child, &identity_subtype);
                    if (err)
                        return err;
                    type->written += identity_subtype.written;
                }

                if (io_putc('}', output) == EOF)
                    return io_error(output);
                ++type->written;
            } else { /* An array */
                if (io_putc('[', output) == EOF)
                    return io_error(output);
                ++type->written;

                Iterator start = base->collection_begin(data);
                for (Iterator it = start; it; it = base->collection_next(data, it)) {
                    if (it != start) {
                        if (io_putc(',', output) == EOF)
                            return io_error(output);
                        ++type->written;
                    }

                    const void *value = base->collection_get_value(data, it);

                    struct SerializerIdentity identity_subtype = *type;

                    int err = io_serialize_json(output, value, base->value_child, &identity_subtype);
                    if (err)
                        return err;
                    type->written += identity_subtype.written;
                }

                if (io_putc(']', output) == EOF)
                    return io_error(output);
                ++type->written;
            }
        } else { /* An atom */
            if (generic_types_compatible_compare(base, container_base_cstring_recipe()) == 0) {
                Binary b = {.data = (char *) data, .length = strlen(data)};
                type->written = serialize_json_string_internal(ascii_only, output, b);
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_binary_recipe()) == 0) {
                type->written = serialize_json_string_internal(ascii_only, output, *((Binary *) data));
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_boolean_recipe()) == 0) {
                const _Bool value = *((const _Bool *) data);
                io_puts(value? "true": "false", output);
                type->written = 5 - value;
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_char_recipe()) == 0) {
                const signed char value = *((const signed char *) data);
                type->written = abs(io_printf(output, "%hhd", value));
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_uchar_recipe()) == 0) {
                const unsigned char value = *((const unsigned char *) data);
                type->written = abs(io_printf(output, "%hhu", value));
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_short_recipe()) == 0) {
                const short value = *((const short *) data);
                type->written = abs(io_printf(output, "%hd", value));
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_ushort_recipe()) == 0) {
                const unsigned short value = *((const unsigned short *) data);
                type->written = abs(io_printf(output, "%hu", value));
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_int_recipe()) == 0) {
                const int value = *((const int *) data);
                type->written = abs(io_printf(output, "%d", value));
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_uint_recipe()) == 0) {
                const unsigned int value = *((const unsigned int *) data);
                type->written = abs(io_printf(output, "%u", value));
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_long_recipe()) == 0) {
                const long value = *((const long *) data);
                type->written = abs(io_printf(output, "%ld", value));
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_ulong_recipe()) == 0) {
                const unsigned long value = *((const unsigned long *) data);
                type->written = abs(io_printf(output, "%lu", value));
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_long_long_recipe()) == 0) {
                const long long value = *((const long long *) data);
                type->written = abs(io_printf(output, "%lld", value));
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_ulong_long_recipe()) == 0) {
                const unsigned long long value = *((const unsigned long long *) data);
                type->written = abs(io_printf(output, "%llu", value));
                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_float_recipe()) == 0) {
                const float flt = *((const float *) data);
                if (fabsf(flt) == INFINITY || isnan(flt))
                    return CC_ENOTSUP;

                type->written = abs(io_printf(output, "%.*g", FLT_DIG-1, flt));

                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_double_recipe()) == 0) {
                const double flt = *((const double *) data);
                if (fabs(flt) == (double) INFINITY || isnan(flt))
                    return CC_ENOTSUP;

                type->written = abs(io_printf(output, "%.*g", DBL_DIG-1, flt));

                return io_error(output);
            } else if (generic_types_compatible_compare(base, container_base_long_double_recipe()) == 0) {
                const long double flt = *((const long double *) data);
                if (fabsl(flt) == (long double) INFINITY || isnan(flt))
                    return CC_ENOTSUP;

                type->written = abs(io_printf(output, "%.*Lg", LDBL_DIG-1, flt));

                return io_error(output);
            } else {
                return CC_ENOTSUP;
            }
        }

        return 0;
    }
}
