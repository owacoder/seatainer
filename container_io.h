/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef CONTAINER_IO_H
#define CONTAINER_IO_H

#include "ccio.h"
#include "containers.h"

StringList stringlist_split_io(IO input, const char *separator, int keep_empty);
int stringlist_join_io(IO output, StringList list, const char *separator);
int binarylist_join_io(IO output, BinaryList list, const char *separator, size_t separator_len);

#endif // CONTAINER_IO_H
