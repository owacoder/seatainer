/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef PKCS7_H
#define PKCS7_H

#include "../io_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opens a device that pads the data read from or written to it, using PKCS #7 padding, to the specified block size.
 *
 * This device can be opened for reading or writing.
 *
 * @param io The device to read from or write to.
 * @param block_size The block size to ensure the output is padded to.
 * @param mode The open mode flags for the returned device.
 * @return A new PKCS #7 padding device, or `NULL` if an error occured.
 */
IO io_open_pkcs7_padding_encode(IO io, size_t block_size, const char *mode);

/** @brief Opens a device that unpads the PKCS #7-padded data read from or written to it to the specified block size.
 *
 * This device can be opened for reading or writing.
 *
 * @param io The device to read from or write to.
 * @param block_size The block size to ensure the output is padded to.
 * @param mode The open mode flags for the returned device.
 * @return A new PKCS #7 unpadding device, or `NULL` if an error occured.
 */
IO io_open_pkcs7_padding_decode(IO io, size_t block_size, const char *mode);

#ifdef __cplusplus
class Pkcs7PaddingEncodeIO : public IODevice {
    IODevice *d;

    void closing() {
        d->decrementRef();
    }

public:
    Pkcs7PaddingEncodeIO() : d(NULL) {}
    Pkcs7PaddingEncodeIO(IODevice &device, size_t block_size, const char *mode = "rwb") : d(NULL) {tryOpen(open(device, block_size, mode));}

    int open(IODevice &device, size_t block_size, const char *mode = "rwb") {
        if (isOpen())
            return AlreadyOpen;
        else if (!device.underlyingDevice())
            return GenericError;

        m_io = io_open_pkcs7_padding_encode(device.underlyingDevice(), block_size, mode);

        if (m_io) {
            this->d = &device;

            device.incrementRef();
        }

        return m_io? 0: GenericError;
    }
};

class Pkcs7PaddingDecodeIO : public IODevice {
    IODevice *d;

    void closing() {
        d->decrementRef();
    }

public:
    Pkcs7PaddingDecodeIO() : d(NULL) {}
    Pkcs7PaddingDecodeIO(IODevice &device, size_t block_size, const char *mode = "rwb") : d(NULL) {tryOpen(open(device, block_size, mode));}

    int open(IODevice &device, size_t block_size, const char *mode = "rwb") {
        if (isOpen())
            return AlreadyOpen;
        else if (!device.underlyingDevice())
            return GenericError;

        m_io = io_open_pkcs7_padding_decode(device.underlyingDevice(), block_size, mode);

        if (m_io) {
            this->d = &device;

            device.incrementRef();
        }

        return m_io? 0: GenericError;
    }
};

}
#endif

#endif // PKCS7_H
