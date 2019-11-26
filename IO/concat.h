/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef CONCAT_H
#define CONCAT_H

#include "../ccio.h"

/** @brief Opens a device to concatenate two IO streams to a single IO stream.
 *
 * This device is useful to concatenate two data streams into a single stream, without creating an intermediate buffer.
 *
 * @param lhs The first IO device to start reading from or writing to.
 * @param rhs The second IO device to start reading from or writing to.
 * @param mode Contains the standard IO device mode specifiers (i.e. "r", "w", "rw").
 * @return A new device allowing concatenation of the two streams, or NULL if an allocation error occurred.
 */
IO io_open_concat(IO lhs, IO rhs, const char *mode);

#endif // CONCAT_H
