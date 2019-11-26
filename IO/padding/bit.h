/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef BIT_H
#define BIT_H

#include "../../ccio.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opens a device that pads the data read from or written to it to the specified block size
 *
 * This device can be opened for reading or writing.
 *
 * @param io The device to read from or write to.
 * @param block_size The block size to ensure the output is padded to.
 * @param mode The open mode flags for the returned device.
 * @return A new bit-padding device, or `NULL` if an error occured.
 */
IO io_open_bit_padding_encode(IO io, size_t block_size, const char *mode);

#ifdef __cplusplus
class BitPaddingIO : public IODevice {
    IODevice *d;

    void closing() {
        d->decrementRef();
    }

public:
    BitPaddingIO() : d(NULL) {}
    BitPaddingIO(IODevice &device, size_t block_size, const char *mode = "rwb") : d(NULL) {tryOpen(open(device, block_size, mode));}

    int open(IODevice &device, size_t block_size, const char *mode = "rwb") {
        if (isOpen())
            return AlreadyOpen;
        else if (!device.underlyingDevice())
            return GenericError;

        m_io = io_open_bit_padding_encode(device.underlyingDevice(), block_size, mode);

        if (m_io) {
            this->d = &device;

            device.incrementRef();
        }

        return m_io? 0: GenericError;
    }
};

}
#endif

#endif // BIT_H
