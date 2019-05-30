/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef CRYPTO_RAND_H
#define CRYPTO_RAND_H

#include "../io.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opens a read-only cryptographically-secure RNG
 *
 * This device does not require seeding. On Windows, it accesses the `RtlGenRandom` function, and on Linux, it accesses the `/dev/urandom` device.
 *
 * @return A new read-only secure RNG device, or `NULL` if an error occured.
 */
IO io_open_crypto_rand();

#ifdef __cplusplus
}
#endif

#endif // CRYPTO_RAND_H
