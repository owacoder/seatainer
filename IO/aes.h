/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef AES_H
#define AES_H

#include "io_core.h"

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

/** @brief Opens an AES decryption device.
 *
 *  It is not possible to use the resulting device for encryption. To do so, use io_open_aes_encrypt() instead.
 *
 *  The @p mode specifier changes how data flows in the filter:
 *
 *    - Open as "r" only: decrypts the ciphertext read from @p io and obtains the plaintext when read from the filter
 *    - Open as "w" only: decrypts the ciphertext written to the filter and pushes the plaintext to @p io
 *    - Open as "rw": Both modes allowed
 *
 *  Hardware acceleration is supported on x86 devices, and is detected at runtime. To refuse access to acceleration, include a '<' in @p mode.
 *
 *  @param io is the underlying device to read data from and write data to. Must not be `NULL`.
 *  @param type is the size of the AES key to be used.
 *  @param cipherMode is the block-cipher mode of operation for this decryption device.
 *  @param key contains the binary key, size dependent on @p type, either 16-, 24-, or 32-byte key if @p type is `AES_128`, `AES_192`, or `AES_256`, respectively. Must not be `NULL`.
 *  @param iv contains the initialization vector to use for decryption, and may be NULL if @p cipherMode does not require one.
 *  @param mode contains the standard IO device mode specifiers (i.e. "r", "w", "rw"), but has special behavior for each. Must not be `NULL`. See the notes for more info.
 *  @return A new IO device filter that decrypts AES data, or `NULL` if a failure occured.
 */
IO io_open_aes_decrypt(IO io, enum AES_Type type, enum AES_Mode cipherMode, const unsigned char *key, const unsigned char iv[16], const char *mode);

/** @brief Opens an AES encryption device.
 *
 *  It is not possible to use the resulting device for decryption. To do so, use io_open_aes_decrypt() instead.
 *
 *  The @p mode specifier changes how data flows in the filter:
 *
 *    - Open as "r" only: encrypts the plaintext read from @p io and obtains the ciphertext when read from the filter
 *    - Open as "w" only: encrypts the plaintext written to the filter and pushes the ciphertext to @p io
 *    - Open as "rw": Both modes allowed
 *
 *  Hardware acceleration is supported on x86 devices, and is detected at runtime. To refuse access to acceleration, include a '<' in @p mode.
 *
 *  @param io is the underlying device to read data from and write data to. Must not be `NULL`.
 *  @param type is the size of the AES key to be used.
 *  @param cipherMode is the block-cipher mode of operation for this encryption device.
 *  @param key contains the binary key, size dependent on @p type, either 16-, 24-, or 32-byte key if @p type is `AES_128`, `AES_192`, or `AES_256`, respectively. Must not be `NULL`.
 *  @param iv contains the initialization vector to use for encryption, and may be NULL if @p cipherMode does not require one.
 *  @param mode contains the standard IO device mode specifiers (i.e. "r", "w", "rw"), but has special behavior for each. Must not be `NULL`. See the notes for more info.
 *  @return A new IO device filter that encrypts AES data, or `NULL` if a failure occured.
 */
IO io_open_aes_encrypt(IO io, enum AES_Type type, enum AES_Mode cipherMode, const unsigned char *key, const unsigned char iv[16], const char *mode);

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
