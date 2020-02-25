/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef BUFFER_H
#define BUFFER_H

#include "../ccio.h"

/** @brief Opens a device to buffer information, even between threads.
 *
 * This device is useful to buffer data that needs to be written immediately but read at a later time, possibly from a different thread.
 * All data written to the buffer will be readable from the device. Simultaneous read and write calls are individually atomic.
 * Reading from the device when no data is available will not block until data is available.
 *
 * No mode option is specified because the mode must be "rwb".
 *
 * @return A new device allowing buffering of data written to it, or NULL if an allocation error occurred.
 */
IO io_open_thread_buffer();

#endif // BUFFER_H
