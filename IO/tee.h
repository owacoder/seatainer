/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef TEE_H
#define TEE_H

#include "../ccio.h"

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
class TeeIO : public IODevice {
    IODevice *out1, *out2;

    void closing() {
        out1->decrementRef();
        out2->decrementRef();
    }

public:
    TeeIO() : out1(NULL), out2(NULL) {}
    TeeIO(IODevice &out1, IODevice &out2, const char *mode = "wb") : out1(NULL), out2(NULL) {tryOpen(open(out1, out2, mode));}

    int open(IODevice &out1, IODevice &out2, const char *mode = "wb") {
        if (isOpen())
            return AlreadyOpen;
        else if (!out1.underlyingDevice() || !out2.underlyingDevice())
            return GenericError;

        m_io = io_open_tee(out1.underlyingDevice(), out2.underlyingDevice(), mode);

        if (m_io) {
            this->out1 = &out1;
            this->out2 = &out2;

            out1.incrementRef();
            out2.incrementRef();
        }

        return m_io? 0: GenericError;
    }
};

}
#endif

#endif // TEE_H
