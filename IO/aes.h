#ifndef AES_H
#define AES_H

#include "../io.h"

#ifdef __cplusplus
extern "C" {
#endif

enum AES_Type {
    AES_128,
    AES_192,
    AES_256
};

enum AES_Mode {
    AES_ECB,
    AES_CBC,
    AES_PCBC,

    /* AES_CFB must be the first stream cipher (everything below this must be a stream cipher) */
    AES_CFB,
    AES_OFB,
    AES_CTR /* TODO: CTR mode not implemented yet (defaults back to ECB). Can't find a really good resource to specify the standardized size of the counter (if there is one) */
};

/* Open as "r" only: to decrypt data from IO and obtain the plaintext
 *
 * Open as "w" only: to decrypt the ciphertext written and push the plaintext to the underlying IO device
 *
 * Open as "rw": Both modes allowed
 *
 */
IO io_open_aes_decrypt(IO io, enum AES_Type type, enum AES_Mode cipherMode, const unsigned char *key, unsigned char iv[16], const char *mode);

/* Open as "r" only: to encrypt data from IO and obtain the ciphertext
 *
 * Open as "w" only: to encrypt the ciphertext written and push the ciphertext to the underlying IO device
 *
 * Open as "rw": Both modes allowed
 *
 */
IO io_open_aes_encrypt(IO io, enum AES_Type type, enum AES_Mode cipherMode, const unsigned char *key, unsigned char iv[16], const char *mode);

void test_aes();

#ifdef __cplusplus
}
#endif

#endif // AES_H
