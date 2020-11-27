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

Variant json_parse(IO input);
int json_serialize_stringlist(IO output, StringList list, struct SerializerIdentity *type);
int json_serialize_stringmap(IO output, StringMap map, struct SerializerIdentity *type);
int json_serialize_stringset(IO output, StringSet set, struct SerializerIdentity *type);
int json_serialize_variantlist(IO output, GenericList variantlist, struct SerializerIdentity *type);
int json_serialize_variantmap(IO output, GenericMap variantmap, struct SerializerIdentity *type);

int json_serialize_variant(IO output, Variant data, struct SerializerIdentity *type);
int json_serialize(IO output, const void *data, const CommonContainerBase *base, struct SerializerIdentity *type);

StringList stringlist_split_io(IO input, const char *separator, int keep_empty);
GenericList binarylist_split_io(IO input, const char *separator, size_t separator_len, int keep_empty);
StringList stringlist_divide_io(IO input, size_t record_size, int keep_partial);
GenericList binarylist_divide_io(IO input, size_t record_size, int keep_partial);
int stringlist_join_io(IO output, StringList list, const char *separator);
int genericlist_join_io(IO output, GenericList list, const char *separator);
int genericlist_join_io_n(IO output, GenericList list, const char *separator, size_t separator_len);

#endif // CONTAINER_IO_H
