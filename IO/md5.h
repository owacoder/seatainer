#ifndef MD5_H
#define MD5_H

#include "../io.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Open as "r" only: to pull all data from IO and obtain the hash
 *
 * Open as "w" only: push data to the hash function and push the hash to the underlying IO device.
 * When the MD5 IO device is closed, the hash will be written to the underlying IO device. If the write fails, the close fails as well.
 *
 * Open as "rw": push data to the hash function and read the hash back.
 * The hash of the currently submitted data can be obtained at any point by reading 16 bytes.
 * When the MD5 IO device is closed, nothing will be written to the underlying IO device.
 *
 */
IO io_open_md5(IO io, const char *mode);

#ifdef __cplusplus
}
#endif

#endif // MD5_H