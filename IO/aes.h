#ifndef AES_H
#define AES_H

#include "../io.h"

enum AES_Type {
    AES_128,
    AES_192,
    AES_256
};

/* Open as "r" only: to decrypt data from IO and obtain the plaintext
 *
 * Open as "w" only: to encrypt the data written and push the ciphertext to the underlying IO device
 *
 * Open as "rw": not currently allowed
 *
 */
IO io_open_aes(IO io, enum AES_Type type, const unsigned char *key, const char *mode);

void test_aes();

#endif // AES_H
