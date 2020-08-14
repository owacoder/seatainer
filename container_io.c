/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "container_io.h"

StringList stringlist_split_io(IO input, const char *separator, int keep_empty) {
    IO buffer = io_open_dynamic_buffer("wb");
    StringList list = stringlist_create();
    if (buffer == NULL || list == NULL) {
        io_close(buffer);
        stringlist_destroy(list);
        return NULL;
    }

    while (1) {

    }
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
