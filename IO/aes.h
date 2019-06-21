/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef AES_H
#define AES_H

#include "../io.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Specifies the length of an AES key. */
enum AES_Type {
    AES_128,
    AES_192,
    AES_256
};

/** Specifies the block-cipher mode for the AES IO device. */
enum AES_Mode {
    AES_ECB,
    AES_CBC,
    AES_PCBC,

    /* AES_CFB must be the first stream cipher (everything below this must be a stream cipher) */
    AES_CFB,
    AES_OFB,
    AES_CTR /* TODO: CTR mode not implemented yet (defaults back to ECB). Can't find a really good resource to specify the standardized size of the counter (if there is one) */
};

IO io_open_aes_decrypt(IO io, enum AES_Type type, enum AES_Mode cipherMode, const unsigned char *key, unsigned char iv[16], const char *mode);
IO io_open_aes_encrypt(IO io, enum AES_Type type, enum AES_Mode cipherMode, const unsigned char *key, unsigned char iv[16], const char *mode);

void test_aes();

#ifdef __cplusplus
class AESEncryptIO : public IODevice {
    IODevice *d;

    void closing() {
        d->decrementRef();
    }

public:
    AESEncryptIO() : d(NULL) {}
    AESEncryptIO(IODevice &d, enum AES_Type type, enum AES_Mode cipherMode, const unsigned char *key, unsigned char iv[16], const char *mode = "rwb") : d(NULL) {tryOpen(open(d, type, cipherMode, key, iv, mode));}

    int open(IODevice &dev, enum AES_Type type, enum AES_Mode cipherMode, const unsigned char *key, unsigned char iv[16], const char *mode = "rwb") {
        if (isOpen())
            return AlreadyOpen;
        else if (!dev.underlyingDevice())
            return GenericError;

        m_io = io_open_aes_encrypt(dev.underlyingDevice(), type, cipherMode, key, iv, mode);

        if (m_io) {
            this->d = &dev;

            dev.incrementRef();
        }

        return m_io? 0: GenericError;
    }
};

class AESDecryptIO : public IODevice {
    IODevice *d;

    void closing() {
        d->decrementRef();
    }

public:
    AESDecryptIO() : d(NULL) {}
    AESDecryptIO(IODevice &d, enum AES_Type type, enum AES_Mode cipherMode, const unsigned char *key, unsigned char iv[16], const char *mode = "rwb") : d(NULL) {tryOpen(open(d, type, cipherMode, key, iv, mode));}

    int open(IODevice &dev, enum AES_Type type, enum AES_Mode cipherMode, const unsigned char *key, unsigned char iv[16], const char *mode = "rwb") {
        if (isOpen())
            return AlreadyOpen;
        else if (!dev.underlyingDevice())
            return GenericError;

        m_io = io_open_aes_decrypt(dev.underlyingDevice(), type, cipherMode, key, iv, mode);

        if (m_io) {
            this->d = &dev;

            dev.incrementRef();
        }

        return m_io? 0: GenericError;
    }
};

}
#endif

#endif // AES_H
