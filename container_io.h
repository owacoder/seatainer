/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef CONTAINER_IO_H
#define CONTAINER_IO_H

#include "ccio.h"
#include "containers.h"

int serialize_is_utf8(const struct SerializerIdentity *identity);
int serialize_raw_was_serialized(IO output,
                                     const void *data,
                                     const CommonContainerBase *base,
                                     const char *type,
                                     int *err,
                                     const struct InputOutputDeviceCallbacks *filter_callbacks,
                                     int (*supports_identity)(const struct SerializerIdentity *identity));
int serialize_variant_was_serialized(IO output,
                                     const Variant data,
                                     const char *type,
                                     int *err,
                                     const struct InputOutputDeviceCallbacks *filter_callbacks,
                                     int (*supports_identity)(const struct SerializerIdentity *identity));

Variant json_parse(IO input);
int json_serialize_stringlist(IO output, StringList list, struct SerializerIdentity *type);
int json_serialize_stringmap(IO output, StringMap map, struct SerializerIdentity *type);
int json_serialize_stringset(IO output, StringSet set, struct SerializerIdentity *type);
int json_serialize_variantlist(IO output, GenericList variantlist, struct SerializerIdentity *type);
int json_serialize_variantmap(IO output, GenericMap variantmap, struct SerializerIdentity *type);
int json_serialize_variant(IO output, Variant data, struct SerializerIdentity *type);

StringList stringlist_split_io(IO input, const char *separator, int keep_empty);
BinaryList binarylist_split_io(IO input, const char *separator, size_t separator_len, int keep_empty);
StringList stringlist_divide_io(IO input, size_t record_size, int keep_partial);
BinaryList binarylist_divide_io(IO input, size_t record_size, int keep_partial);
int stringlist_join_io(IO output, StringList list, const char *separator);
int binarylist_join_io(IO output, BinaryList list, const char *separator, size_t separator_len);

#endif // CONTAINER_IO_H
