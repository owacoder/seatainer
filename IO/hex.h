/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef HEX_H
#define HEX_H

#include "io_core.h"

#ifdef __cplusplus
extern "C" {
#endif

IO io_open_hex_encode(IO io, const char *mode);
IO io_open_hex_decode(IO io, const char *mode);

void test_hex();

#ifdef __cplusplus
class HexEncodeIO : public IODevice {
    IODevice *d;

    void closing() {
        d->decrementRef();
    }

public:
    HexEncodeIO() : d(NULL) {}
    HexEncodeIO(IODevice &d, const char *mode = "rwb") : d(NULL) {tryOpen(open(d, mode));}

    int open(IODevice &dev, const char *mode = "rwb") {
        if (isOpen())
            return AlreadyOpen;
        else if (!dev.underlyingDevice())
            return GenericError;

        m_io = io_open_hex_encode(dev.underlyingDevice(), mode);

        if (m_io) {
            this->d = &dev;

            dev.incrementRef();
        }

        return m_io? 0: GenericError;
    }
};

class HexDecodeIO : public IODevice {
    IODevice *d;

    void closing() {
        d->decrementRef();
    }

public:
    HexDecodeIO() : d(NULL) {}
    HexDecodeIO(IODevice &d, const char *mode = "rwb") : d(NULL) {tryOpen(open(d, mode));}

    int open(IODevice &dev, const char *mode = "rwb") {
        if (isOpen())
            return AlreadyOpen;
        else if (!dev.underlyingDevice())
            return GenericError;

        m_io = io_open_hex_decode(dev.underlyingDevice(), mode);

        if (m_io) {
            this->d = &dev;

            dev.incrementRef();
        }

        return m_io? 0: GenericError;
    }
};

}
#endif

#endif // HEX_H
