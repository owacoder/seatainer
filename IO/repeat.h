/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef REPEAT_H
#define REPEAT_H

#include "../ccio.h"

/** @brief Opens a device to repeat an IO stream as an infinite stream.
 *
 * This device repeats an input IO stream endlessly, creating a never-ending stream.
 *
 * @param io The IO device to read from.
 * @param mode Contains the standard IO device mode specifiers (i.e. "r", "w", "rw").
 * @return A new device allowing repeating of the IO stream, or NULL if an allocation error occurred.
 */
IO io_open_repeat(IO io, const char *mode);

#endif // REPEAT_H
