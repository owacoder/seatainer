/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef CONTAINER_IO_H
#define CONTAINER_IO_H

#include "ccio.h"
#include "containers.h"

/** @brief Registers a type to be used with the io_printf and io_scanf family of functions.
 *
 * @param name The constant name of the type. This value is not copied, so it's essential the string stays valid.
 * @param base The common container base that describes the type. If dynamic, ownership of the base is adopted by the system.
 * @return Returns 0 on success, or the error that occurred on failure.
 */
int io_register_type(const char *name, CommonContainerBase *base);

/** @brief Finds a registered type by its name.
 *
 * @param name The name of the registered type.
 * @param name_len The length of the string pointed to by name. This can be a substring of another string if necessary.
 * @return Returns the registered container base on success, or NULL if an error occurred or the type was not registered.
 */
const CommonContainerBase *io_get_registered_type(const char *name, size_t name_len);

/** @brief Unregisters a type that was registered with io_register_type()
 *
 * @param name The name of the type to be removed.
 */
void io_unregister_type(const char *name);

/** @brief Registers a type to be used with the io_printf and io_scanf family of functions.
 *
 * @param name The constant name of the type. This value is not copied, so it's essential the string stays valid.
 * @param parser The parser function to be used with the format, or NULL, if no parsing is supported.
 * @param serializer The serializer function to be used with the format, or NULL if no serialization is supported.
 * @return Returns 0 on success, or the error that occurred on failure.
 */
int io_register_format(const char *name, Parser parser, Serializer serializer);

/** @brief Unregisters a format that was registered with io_register_format()
 *
 * @param name The name of the format to be removed.
 */
void io_unregister_format(const char *name);

/** Default "UTF-8" parsers for known types */
int io_parse_boolean(IO input, void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_parse_char(IO input, void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_parse_short(IO input, void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_parse_ushort(IO input, void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_parse_int(IO input, void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_parse_uint(IO input, void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_parse_long(IO input, void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_parse_ulong(IO input, void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_parse_long_long(IO input, void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_parse_ulong_long(IO input, void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_parse_size_t(IO input, void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_parse_float(IO input, void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_parse_double(IO input, void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_parse_long_double(IO input, void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_parse_cstring(IO input, void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_parse_binary(IO input, void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_parse_variant(IO input, void *data, const CommonContainerBase *base, struct SerializerIdentity *type);

/** Default "UTF-8" serializers for known types */
int io_serialize_boolean(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_char(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_uchar(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_short(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_ushort(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_int(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_uint(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_long(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_ulong(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_long_long(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_ulong_long(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_size_t(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_float(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_double(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_long_double(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_cstring(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_binary(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_variant(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);

/* Serializer format specifies the separator with which to join elements. */
int io_serialize_list_join(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_utf8(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);

int io_serialize_clock_t(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_tm(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);

/** Get the default "UTF-8" serializer given a type */
Serializer io_default_serializer_for_type(const CommonContainerBase *base);

/** Generic JSON parser and serializer, supporting Variants and containers, as well as simple types */
int io_parse_json(IO input, void *data, const CommonContainerBase *base, struct ParserIdentity *type);

/** Generic JSON serializer, supporting Variants and containers, as well as simple types
 * The user-specified format can include the following:
 *    "ASCII": Forces all non-ASCII unicode points to be escaped in JSON strings. If this flag is not used, valid UTF-8 is passed through unchanged.
 *             If invalid UTF-8 is encountered in a string or binary value, the invalid byte will be mapped to an escaped Unicode code point
 *             regardless of this flag's existence. This allows simple UTF-8 decoders to decode to the same byte sequence as the source.
 */
int io_serialize_json(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);

/** The following are not serializers, but they are special functions for fast operations on Binary and C-string lists */
StringList stringlist_split_io(IO input, const char *separator, int keep_empty);
GenericList binarylist_split_io(IO input, const char *separator, size_t separator_len, int keep_empty);
StringList stringlist_divide_io(IO input, size_t record_size, int keep_partial);
GenericList binarylist_divide_io(IO input, size_t record_size, int keep_partial);
int io_join_stringlist(IO output, StringList list, const char *separator);
int io_join_genericlist(IO output, GenericList list, const char *separator);
int io_join_genericlist_n(IO output, GenericList list, const char *separator, size_t separator_len);

#endif // CONTAINER_IO_H
