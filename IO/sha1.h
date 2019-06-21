/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef SHA1_H
#define SHA1_H

#include "../io.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Open as "r" only: to pull all data from IO and obtain the hash
 *
 * Open as "w" only: push data to the hash function and push the hash to the underlying IO device.
 * When the SHA1 IO device is closed, the hash will be written to the underlying IO device. If the write fails, the close fails as well.
 *
 * Open as "rw": push data to the hash function and read the hash back.
 * The hash of the currently submitted data can be obtained at any point by reading 16 bytes.
 * When the SHA1 IO device is closed, nothing will be written to the underlying IO device.
 *
 */
IO io_open_sha1(IO io, const char *mode);

#ifdef __cplusplus
class Sha1IO : public IODevice {
    IODevice *d;

    void closing() {
        d->decrementRef();
    }

public:
    Sha1IO() : d(NULL) {}
    Sha1IO(IODevice &d, const char *mode = "wb") : d(NULL) {open(d, mode);}

    int open(IODevice &dev, const char *mode = "wb") {
        if (isOpen())
            return AlreadyOpen;
        else if (!dev.underlyingDevice())
            return GenericError;

        m_io = io_open_sha1(dev.underlyingDevice(), mode);

        if (m_io) {
            this->d = &dev;

            dev.incrementRef();
        }

        return m_io? 0: GenericError;
    }
};

}
#endif

#endif // SHA1_H
