/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef LIMITER_H
#define LIMITER_H

#include "../ccio.h"

/** @brief Opens a device to limit IO to a specific subset of another device.
 *
 * This device is useful to limit the amount of data read from the middle of another stream.
 *
 * @param io The IO device to read from or write to.
 * @param offset The offset of @p io to start reading from or writing to.
 * @param length The maximum number of characters to read from or write to @p io.
 * @param mode Contains the standard IO device mode specifiers (i.e. "r", "w", "rw").
 * @return A new device allowing limited reading and writing on @p io, or NULL if an allocation error occurred.
 */
IO io_open_limiter(IO io, long long offset, long long length, const char *mode);

#endif // LIMITER_H
