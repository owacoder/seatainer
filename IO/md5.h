/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef MD5_H
#define MD5_H

#include "../ccio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Open as "r" only: to pull all data from IO and obtain the hash
 *
 * Open as "w" only: push data to the hash function and push the hash to the underlying IO device.
 * When the MD5 IO device is closed, the hash will be written to the underlying IO device. If the write fails, the close fails as well.
 *
 * Open as "rw": push data to the hash function and read the result back.
 * Once data is read, a new hash is begun with the next write to the device. TODO: this means the empty hash can never be computed again. Fix?
 * When the MD5 IO device is closed, nothing will be written to the underlying IO device.
 *
 * Open as "rw+": push data to the hash function and read the intermediate hash back.
 * The hash of the currently submitted data can be obtained at any point by reading 16 bytes.
 * There is no way to reset the device to start a new hash.
 * When the MD5 IO device is closed, nothing will be written to the underlying IO device.
 *
 */
IO io_open_md5(IO io, const char *mode);

#ifdef __cplusplus
class Md5IO : public IODevice {
    IODevice *d;

    void closing() {
        d->decrementRef();
    }

public:
    Md5IO() : d(NULL) {}
    Md5IO(IODevice &d, const char *mode = "wb") : d(NULL) {tryOpen(open(d, mode));}

    int open(IODevice &dev, const char *mode = "wb") {
        if (isOpen())
            return AlreadyOpen;
        else if (!dev.underlyingDevice())
            return GenericError;

        m_io = io_open_md5(dev.underlyingDevice(), mode);

        if (m_io) {
            this->d = &dev;

            dev.incrementRef();
        }

        return m_io? 0: GenericError;
    }
};

}
#endif

#endif // MD5_H
