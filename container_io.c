/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "container_io.h"
#include "Containers/sbuffer.h"
#include "utility.h"
#include <math.h>

static char *json_parse_string_helper(IO input) {
    int escape_next = 0;
    Buffer buffer;

    /* TODO: bad messages should set the CC_EBADMSG error on the IO device */
    buffer_init(&buffer);

    while (1) {
        int ch = io_getc(input);

        if (ch == EOF)
            return NULL;

        if (escape_next) {
            switch (ch) {
                case '"':
                case '\\':
                case '/':
                case 'b':
                case 'f':
                case 'n':
                case 'r':
                case 't':
                case 'u':
                default:
                    return NULL;
            }
            escape_next = 0;
        } else if (ch == '\\') {
            escape_next = 1;
            continue;
        } else if (ch == '"') {
            return buffer.data;
        }
    }

    return NULL;
}

Variant json_parse(IO input) {
    char buffer[8];

    /* TODO: bad messages should set the CC_EBADMSG error on the IO device */

    while (1) {
        int ch = io_getc(input);

        switch (ch) {
            case EOF: return NULL;
            case 'n':
                if (io_read(buffer, 1, 3, input) != 3 || memcmp(buffer, "ull", 3))
                    return NULL;

                return variant_create_null();
            case 't':
                if (io_read(buffer, 1, 3, input) != 3 || memcmp(buffer, "rue", 3))
                    return NULL;

                return variant_create_boolean(1);
            case 'f':
                if (io_read(buffer, 1, 4, input) != 4 || memcmp(buffer, "alse", 4))
                    return NULL;

                return variant_create_boolean(0);
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
            case '9': /* Handle number */ {
                io_ungetc(ch, input);

                double number;

                break;
            }
            case '"': /* Handle string */
            case '[': /* Handle array */
            case '{': /* Handle object */
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                break;
        }
    }

    return NULL;
}

static size_t json_serialize_stringpart_io(const void *data, size_t size, size_t count, void *userdata, IO io) {
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
            return written;
        }
    }

    return written;
}

static const char *json_what(void *userdata, IO io) {
    UNUSED(userdata);
    UNUSED(io);

    return "json";
}

static const struct InputOutputDeviceCallbacks json_callbacks = {
    .read = NULL,
    .write = json_serialize_stringpart_io,
    .open = NULL,
    .close = NULL,
    .flush = NULL,
    .clearerr = NULL,
    .stateSwitch = NULL,
    .tell = NULL,
    .tell64 = NULL,
    .seek = NULL,
    .seek64 = NULL,
    .flags = NULL,
    .what = json_what
};

static int json_serialize_stringpart_internal(IO output, const Binary b) {
    return json_serialize_stringpart_io(b.data, 1, b.length, output, NULL) == b.length? 0: io_error(output);
}

static int json_serialize_string_internal(IO output, const Binary b) {
    if (io_putc('"', output) == EOF ||
        json_serialize_stringpart_internal(output, b) ||
        io_putc('"', output) == EOF)
        return io_error(output);

    return 0;
}

int json_serialize_stringlist(IO output, StringList list, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("JSON", 1);

    int err = 0;
    if (serialize_raw_was_serialized(output,
                                     list,
                                     stringlist_get_container_base(list),
                                     "JSON",
                                     &err,
                                     &json_callbacks,
                                     serialize_is_utf8)) {
        return err;
    }

    if (io_putc('[', output) == EOF)
        return io_error(output);

    for (size_t i = 0; i < stringlist_size(list); ++i) {
        if (i > 0 && io_putc(',', output) == EOF)
            return io_error(output);

        Binary b = {.data = stringlist_array(list)[i], .length = strlen(stringlist_array(list)[i])};

        int err = json_serialize_string_internal(output, b);
        if (err)
            return err;
    }

    if (io_putc(']', output) == EOF)
        return io_error(output);

    return 0;
}

int json_serialize_variantlist(IO output, GenericList list, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("JSON", 1);

    int err = 0;
    if (serialize_raw_was_serialized(output,
                                     list,
                                     genericlist_get_container_base(list),
                                     "JSON",
                                     &err,
                                     &json_callbacks,
                                     serialize_is_utf8)) {
        return err;
    }

    if (io_putc('[', output) == EOF)
        return io_error(output);

    for (size_t i = 0; i < genericlist_size(list); ++i) {
        if (i > 0 && io_putc(',', output) == EOF)
            return io_error(output);

        int err = json_serialize_variant(output, (Variant) genericlist_array(list)[i], type);
        if (err)
            return err;
    }

    if (io_putc(']', output) == EOF)
        return io_error(output);

    return 0;
}

int json_serialize_stringmap(IO output, StringMap map, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("JSON", 1);

    int err = 0;
    if (serialize_raw_was_serialized(output,
                                     map,
                                     stringmap_get_container_base(map),
                                     "JSON",
                                     &err,
                                     &json_callbacks,
                                     serialize_is_utf8)) {
        return err;
    }

    Iterator start = stringmap_begin(map);

    if (io_putc('{', output) == EOF)
        return io_error(output);

    for (Iterator it = start; it; it = stringmap_next(map, it)) {
        if (it != start && io_putc(',', output) == EOF)
            return io_error(output);

        int err;
        Binary b;

        b.data = (char *) stringmap_key_of(map, it);
        b.length = strlen(b.data);

        err = json_serialize_string_internal(output, b);
        if (err)
            return err;

        if (io_putc(':', output) == EOF)
            return io_error(output);

        b.data = stringmap_value_of(map, it);
        b.length = strlen(b.data);

        err = json_serialize_string_internal(output, b);
        if (err)
            return err;
    }

    if (io_putc('}', output) == EOF)
        return io_error(output);

    return 0;
}

int json_serialize_variantmap(IO output, GenericMap map, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("JSON", 1);

    int err = 0;
    if (serialize_raw_was_serialized(output,
                                     map,
                                     genericmap_get_container_base(map),
                                     "JSON",
                                     &err,
                                     &json_callbacks,
                                     serialize_is_utf8)) {
        return err;
    }

    Iterator start = genericmap_begin(map);

    if (io_putc('{', output) == EOF)
        return io_error(output);

    for (Iterator it = start; it; it = genericmap_next(map, it)) {
        if (it != start && io_putc(',', output) == EOF)
            return io_error(output);

        int err;

        err = json_serialize_string_internal(output, genericmap_key_of(map, it));
        if (err)
            return err;

        if (io_putc(':', output) == EOF)
            return io_error(output);

        err = json_serialize_variant(output, (Variant) genericmap_value_of(map, it), type);
        if (err)
            return err;
    }

    if (io_putc('}', output) == EOF)
        return io_error(output);

    return 0;
}

int json_serialize_stringset(IO output, StringSet set, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("JSON", 1);

    int err = 0;
    if (serialize_raw_was_serialized(output,
                                     set,
                                     stringset_get_container_base(set),
                                     "JSON",
                                     &err,
                                     &json_callbacks,
                                     serialize_is_utf8)) {
        return err;
    }

    Iterator start = stringset_begin(set);

    if (io_putc('[', output) == EOF)
        return io_error(output);

    for (Iterator it = start; it; it = stringset_next(set, it)) {
        if (it != start && io_putc(',', output) == EOF)
            return io_error(output);

        Binary b;

        b.data = (char *) stringset_value_of(set, it);
        b.length = strlen(b.data);

        int err = json_serialize_stringpart_internal(output, b);
        if (err)
            return err;
    }

    if (io_putc(']', output) == EOF)
        return io_error(output);

    return 0;
}

int json_serialize_variant(IO output, Variant data, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("JSON", 1);

    int err = 0;
    if (serialize_variant_was_serialized(output,
                                         data,
                                         "JSON",
                                         &err,
                                         &json_callbacks,
                                         serialize_is_utf8)) {
        return err;
    }

    switch (variant_get_type(data)) {
        case VariantNull: io_puts("null", output); return io_error(output);
        case VariantBoolean: io_puts(variant_get_boolean(data)? "true": "false", output); return io_error(output);
        case VariantInteger: io_printf(output, "%lld", variant_get_int64(data)); return io_error(output);
        case VariantUnsignedInteger: io_printf(output, "%llu", variant_get_uint64(data)); return io_error(output);
        case VariantFloat: {
            double flt = variant_get_float(data);
            if (fabs(flt) == (double) INFINITY || isnan(flt))
                return CC_ENOTSUP;

            int err;
            const char *str = variant_to_string(data, &err);

            if (err)
                return err;

            io_puts(str, output);

            return io_error(output);
        }
        case VariantString: {
            Binary b = variant_to_binary(data, NULL);

            return json_serialize_string_internal(output, b);
        }
        case VariantBinary: return CC_ENOTSUP;
        default: /* Custom datatypes, no serializer specified */ {
            if (variant_is_stringlist(data))
                return json_serialize_stringlist(output, variant_get_custom(data), type);
            else if (variant_is_stringmap(data))
                return json_serialize_stringmap(output, variant_get_custom(data), type);
            else if (variant_is_variantlist(data))
                return json_serialize_variantlist(output, variant_get_custom(data), type);
            else if (variant_is_variantmap(data))
                return json_serialize_variantmap(output, variant_get_custom(data), type);
            else
                return CC_ENOTSUP;
        }
    }
}

static size_t msgpack_serialize_stringpart_io(const void *data, size_t size, size_t count, void *userdata, IO io) {
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
            return written;
        }
    }

    return written;
}

static const char *msgpack_what(void *userdata, IO io) {
    UNUSED(userdata);
    UNUSED(io);

    return "msgpack";
}

static const struct InputOutputDeviceCallbacks msgpack_callbacks = {
    .read = NULL,
    .write = msgpack_serialize_stringpart_io,
    .open = NULL,
    .close = NULL,
    .flush = NULL,
    .clearerr = NULL,
    .stateSwitch = NULL,
    .tell = NULL,
    .tell64 = NULL,
    .seek = NULL,
    .seek64 = NULL,
    .flags = NULL,
    .what = msgpack_what
};

static int msgpack_serialize_stringpart_internal(IO output, const Binary b) {
    return msgpack_serialize_stringpart_io(b.data, 1, b.length, output, NULL) == b.length? 0: io_error(output);
}

static int msgpack_serialize_string_internal(IO output, const Binary b) {
    if (io_putc('"', output) == EOF ||
        msgpack_serialize_stringpart_internal(output, b) ||
        io_putc('"', output) == EOF)
        return io_error(output);

    return 0;
}

int msgpack_serialize_stringlist(IO output, StringList list, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("MessagePack", 0);

    int err = 0;
    if (serialize_raw_was_serialized(output,
                                     list,
                                     stringlist_get_container_base(list),
                                     "MessagePack",
                                     &err,
                                     &msgpack_callbacks,
                                     serialize_is_utf8)) {
        return err;
    }

    if (io_putc('[', output) == EOF)
        return io_error(output);

    for (size_t i = 0; i < stringlist_size(list); ++i) {
        if (i > 0 && io_putc(',', output) == EOF)
            return io_error(output);

        Binary b = {.data = stringlist_array(list)[i], .length = strlen(stringlist_array(list)[i])};

        int err = msgpack_serialize_string_internal(output, b);
        if (err)
            return err;
    }

    if (io_putc(']', output) == EOF)
        return io_error(output);

    return 0;
}

int msgpack_serialize_variantlist(IO output, GenericList list, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("MessagePack", 0);

    int err = 0;
    if (serialize_raw_was_serialized(output,
                                     list,
                                     genericlist_get_container_base(list),
                                     "MessagePack",
                                     &err,
                                     &msgpack_callbacks,
                                     serialize_is_utf8)) {
        return err;
    }

    if (io_putc('[', output) == EOF)
        return io_error(output);

    for (size_t i = 0; i < genericlist_size(list); ++i) {
        if (i > 0 && io_putc(',', output) == EOF)
            return io_error(output);

        int err = msgpack_serialize_variant(output, (Variant) genericlist_array(list)[i], type);
        if (err)
            return err;
    }

    if (io_putc(']', output) == EOF)
        return io_error(output);

    return 0;
}

int msgpack_serialize_stringmap(IO output, StringMap map, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("MessagePack", 0);

    int err = 0;
    if (serialize_raw_was_serialized(output,
                                     map,
                                     stringmap_get_container_base(map),
                                     "MessagePack",
                                     &err,
                                     &msgpack_callbacks,
                                     serialize_is_utf8)) {
        return err;
    }

    Iterator start = stringmap_begin(map);

    if (io_putc('{', output) == EOF)
        return io_error(output);

    for (Iterator it = start; it; it = stringmap_next(map, it)) {
        if (it != start && io_putc(',', output) == EOF)
            return io_error(output);

        int err;
        Binary b;

        b.data = (char *) stringmap_key_of(map, it);
        b.length = strlen(b.data);

        err = msgpack_serialize_string_internal(output, b);
        if (err)
            return err;

        if (io_putc(':', output) == EOF)
            return io_error(output);

        b.data = stringmap_value_of(map, it);
        b.length = strlen(b.data);

        err = msgpack_serialize_string_internal(output, b);
        if (err)
            return err;
    }

    if (io_putc('}', output) == EOF)
        return io_error(output);

    return 0;
}

int msgpack_serialize_variantmap(IO output, GenericMap map, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("MessagePack", 0);

    int err = 0;
    if (serialize_raw_was_serialized(output,
                                     map,
                                     genericmap_get_container_base(map),
                                     "MessagePack",
                                     &err,
                                     &msgpack_callbacks,
                                     serialize_is_utf8)) {
        return err;
    }

    Iterator start = genericmap_begin(map);

    if (io_putc('{', output) == EOF)
        return io_error(output);

    for (Iterator it = start; it; it = genericmap_next(map, it)) {
        if (it != start && io_putc(',', output) == EOF)
            return io_error(output);

        int err;

        err = msgpack_serialize_string_internal(output, genericmap_key_of(map, it));
        if (err)
            return err;

        if (io_putc(':', output) == EOF)
            return io_error(output);

        err = msgpack_serialize_variant(output, (Variant) genericmap_value_of(map, it), type);
        if (err)
            return err;
    }

    if (io_putc('}', output) == EOF)
        return io_error(output);

    return 0;
}

int msgpack_serialize_variant(IO output, Variant data, struct SerializerIdentity *type) {
    SERIALIZER_DECLARE("MessagePack", 0);

    int err = 0;
    if (serialize_variant_was_serialized(output,
                                         data,
                                         "MessagePack",
                                         &err,
                                         &msgpack_callbacks,
                                         serialize_is_utf8)) {
        return err;
    }

    switch (variant_get_type(data)) {
        case VariantNull: io_putc(0xc0, output); return io_error(output);
        case VariantBoolean: io_putc(0xc2 + !!variant_get_boolean(data), output); return io_error(output);
        case VariantInteger: io_printf(output, "%lld", variant_get_int64(data)); return io_error(output);
        case VariantUnsignedInteger: io_printf(output, "%llu", variant_get_uint64(data)); return io_error(output);
        case VariantFloat: {
            double flt = variant_get_float(data);
            if (fabs(flt) == INFINITY || isnan(flt))
                return CC_EINVAL;

            int err;
            const char *str = variant_to_string(data, &err);

            if (err)
                return err;

            io_puts(str, output);

            return io_error(output);
        }
        case VariantString: {
            Binary b = variant_to_binary(data, NULL);

            return msgpack_serialize_string_internal(output, b);
        }
        case VariantBinary: return CC_ENOTSUP;
        default: /* Custom datatypes, no serializer specified */ {
            if (variant_is_stringlist(data))
                return msgpack_serialize_stringlist(output, variant_get_custom(data), type);
            else if (variant_is_stringmap(data))
                return msgpack_serialize_stringmap(output, variant_get_custom(data), type);
            else if (variant_is_variantlist(data))
                return msgpack_serialize_variantlist(output, variant_get_custom(data), type);
            else if (variant_is_variantmap(data))
                return msgpack_serialize_variantmap(output, variant_get_custom(data), type);
            else
                return CC_ENOTSUP;
        }
    }
}

int serialize_is_utf8(const struct SerializerIdentity *identity) {
    return identity->is_utf8;
}

int serialize_raw_was_serialized(IO output, const void *data, const CommonContainerBase *base, const char *type, int *err, const struct InputOutputDeviceCallbacks *filter_callbacks, int (*supports_identity)(const struct SerializerIdentity *)) {
    if (base->serialize) {
        IO sub_output = output;
        int error = 0;
        struct SerializerIdentity identity;

        if ((error = base->serialize(NULL, NULL, &identity)) != 0) {
            ;
        } else if (strcmp(type, identity.type)) {
            if (base->cvt_expects_variant) /* We can't do anything if it expects a variant, since we don't have one to work with */
                return 0;

            /* - Expects raw data. Outputs in a different format, so we need to run the serializer */

            if ((supports_identity && !supports_identity(&identity))) {
                if (err) *err = CC_ENOTSUP;
                return 1;
            }

            sub_output = filter_callbacks? io_open_custom(filter_callbacks, output, "wb"): io_open_dynamic_buffer("wb");
            if (sub_output == NULL) {
                if (err) *err = CC_ENOMEM;
                return 1;
            }

            error = base->serialize(sub_output, data, NULL);

            if (!filter_callbacks && !error && /* Using dynamic buffer and no errors so far */
                    io_write(io_underlying_buffer(sub_output), 1, io_underlying_buffer_size(sub_output), output) != io_underlying_buffer_size(sub_output))
                error = io_error(output);

            int close_error = io_close(sub_output);
            if (!error)
                error = close_error;
        } else if (!base->cvt_expects_variant) { /* Serializer expects raw data. Outputs in same format, so we don't need special handling */
            error = base->serialize(output, data, NULL);
        } else
            return 0;

        if (err)
            *err = error;

        return 1;
    }

    return 0;
}

int serialize_variant_was_serialized(IO output, const Variant data, const char *type, int *err, const struct InputOutputDeviceCallbacks *filter_callbacks, int (*supports_identity)(const struct SerializerIdentity *)) {
    if (variant_get_container_base(data)->serialize) {
        IO sub_output = output;
        int error = 0;
        struct SerializerIdentity identity;

        if ((error = variant_get_container_base(data)->serialize(NULL, NULL, &identity)) != 0) {
            ;
        } else if (strcmp(type, identity.type)) {
            if (supports_identity && !supports_identity(&identity)) {
                if (err) *err = CC_ENOTSUP;
                return 1;
            }

            sub_output = filter_callbacks? io_open_custom(filter_callbacks, output, "wb"): io_open_dynamic_buffer("wb");
            if (sub_output == NULL) {
                if (err) *err = CC_ENOMEM;
                return 1;
            }

            /* - Expects a variant, but outputs a different format. We need to run the serializer anyway, or */
            /* - Expects raw data from variant. Outputs in a different format, so we need to run the serializer */
            error = variant_serialize(sub_output, data, NULL);

            if (!filter_callbacks && !error && /* Using dynamic buffer and no errors so far */
                    io_write(io_underlying_buffer(sub_output), 1, io_underlying_buffer_size(sub_output), output) != io_underlying_buffer_size(sub_output))
                error = io_error(output);

            int close_error = io_close(sub_output);
            if (!error)
                error = close_error;
        } else if (!variant_get_container_base(data)->cvt_expects_variant) { /* Serializer expects raw data from variant. Outputs in same format, so we don't need special handling */
            error = variant_serialize(output, data, NULL);
        } else
            return 0;

        if (err)
            *err = error;

        return 1;
    }

    return 0;
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

BinaryList binarylist_split_io(IO input, const char *separator, size_t separator_len, int keep_empty) {
    Buffer data_buffer;
    size_t separator_matched = 0; /* If non-zero, contains number of separator characters matched */
    BinaryList list = binarylist_create();
    if (list == NULL) {
        binarylist_destroy(list);
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
                if ((keep_empty || data_buffer.length) && binarylist_append(list, data_buffer.data, data_buffer.length))
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
        if (binarylist_append(list, data_buffer.data, data_buffer.length))
            goto cleanup;
    }

    buffer_destroy(&data_buffer);
    return list;

cleanup:
    buffer_destroy(&data_buffer);
    binarylist_destroy(list);
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

BinaryList binarylist_divide_io(IO input, size_t record_size, int keep_partial) {
    char *buffer = MALLOC(record_size);
    BinaryList list = binarylist_create();
    if (buffer == NULL || list == NULL)
        goto cleanup;

    size_t read = 0;
    do {
        read = io_read(buffer, 1, record_size, input);

        if (read == record_size) { /* Full record */
            if (binarylist_append(list, buffer, read))
                goto cleanup;
        } else if (io_error(input)) {
            goto cleanup;
        } else if (read > 0 && keep_partial) {
            if (binarylist_append(list, buffer, read))
                goto cleanup;
        }
    } while (read == record_size);

    FREE(buffer);

    return list;

cleanup:
    FREE(buffer);
    binarylist_destroy(list);
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

int binarylist_join_io(IO output, BinaryList list, const char *separator, size_t separator_len) {
    Iterator it = binarylist_begin(list);

    while (it) {
        Binary item = binarylist_value_of(list, it);

        if (io_write(item.data, 1, item.length, output) != item.length)
            return io_error(output);

        it = binarylist_next(list, it);

        if (it && io_write(separator, 1, separator_len, output) != separator_len)
            return io_error(output);
    }

    return 0;
}
