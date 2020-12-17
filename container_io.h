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

/** Default "UTF-8" serializers for known types */
int io_serialize_boolean(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_char(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
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
int io_serialize_container(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);

int io_serialize_clock_t(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);
int io_serialize_tm(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);

/** Get the default "UTF-8" serializer given a type */
Serializer io_default_serializer_for_type(const CommonContainerBase *base);

/** Generic JSON serializer, supporting Variants and containers, as well as simple types */
int io_serialize_json(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);

StringList stringlist_split_io(IO input, const char *separator, int keep_empty);
GenericList binarylist_split_io(IO input, const char *separator, size_t separator_len, int keep_empty);
StringList stringlist_divide_io(IO input, size_t record_size, int keep_partial);
GenericList binarylist_divide_io(IO input, size_t record_size, int keep_partial);
int stringlist_join_io(IO output, StringList list, const char *separator);
int genericlist_join_io(IO output, GenericList list, const char *separator);
int genericlist_join_io_n(IO output, GenericList list, const char *separator, size_t separator_len);

#endif // CONTAINER_IO_H
