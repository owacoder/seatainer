#ifndef TEE_H
#define TEE_H

#include "../io.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opens a device that duplicates the data pushed to it.
 *
 * This device cannot be opened for reading.
 *
 * @return A new write-only tee device, or `NULL` if an error occured.
 */
IO io_open_tee(IO out1, IO out2, const char *mode);

#ifdef __cplusplus
}
#endif

#endif // TEE_H
