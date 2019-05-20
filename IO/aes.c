#include "aes.h"
#include "../platforms.h"
#include "../utility.h"
#include <limits.h>

static unsigned char test_aes_plaintext[] = {
    0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
    0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
    0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
    0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
    0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
    0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
    0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
    0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10
};

static unsigned char test_aes_ciphertext128[] = {
    0x3a, 0xd7, 0x7b, 0xb4, 0x0d, 0x7a, 0x36, 0x60,
    0xa8, 0x9e, 0xca, 0xf3, 0x24, 0x66, 0xef, 0x97,
    0xf5, 0xd3, 0xd5, 0x85, 0x03, 0xb9, 0x69, 0x9d,
    0xe7, 0x85, 0x89, 0x5a, 0x96, 0xfd, 0xba, 0xaf,
    0x43, 0xb1, 0xcd, 0x7f, 0x59, 0x8e, 0xce, 0x23,
    0x88, 0x1b, 0x00, 0xe3, 0xed, 0x03, 0x06, 0x88,
    0x7b, 0x0c, 0x78, 0x5e, 0x27, 0xe8, 0xad, 0x3f,
    0x82, 0x23, 0x20, 0x71, 0x04, 0x72, 0x5d, 0xd4
};

static unsigned char test_aes_ciphertext192[] = {
    0xbd, 0x33, 0x4f, 0x1d, 0x6e, 0x45, 0xf2, 0x5f,
    0xf7, 0x12, 0xa2, 0x14, 0x57, 0x1f, 0xa5, 0xcc,
    0x97, 0x41, 0x04, 0x84, 0x6d, 0x0a, 0xd3, 0xad,
    0x77, 0x34, 0xec, 0xb3, 0xec, 0xee, 0x4e, 0xef,
    0xef, 0x7a, 0xfd, 0x22, 0x70, 0xe2, 0xe6, 0x0a,
    0xdc, 0xe0, 0xba, 0x2f, 0xac, 0xe6, 0x44, 0x4e,
    0x9a, 0x4b, 0x41, 0xba, 0x73, 0x8d, 0x6c, 0x72,
    0xfb, 0x16, 0x69, 0x16, 0x03, 0xc1, 0x8e, 0x0e
};

static unsigned char test_aes_ecb_key128[] = {
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};

static unsigned char test_aes_ecb_key192[] = {
    0x8e, 0x73, 0xb0, 0xf7, 0xda, 0x0e, 0x64, 0x52,
    0xc8, 0x10, 0xf3, 0x2b, 0x80, 0x90, 0x79, 0xe5,
    0x62, 0xf8, 0xea, 0xd2, 0x52, 0x2c, 0x6b, 0x7b
};

#if defined(__SSE2__)
#define AES_COMPILE_SUPPORTS_X86_INTRINSICS
#endif

#include "hex.h"
#include "crypto_rand.h"

void test_aes() {
    unsigned char iv[16];

    IO rand = io_open_crypto_rand();
    io_read(iv, 1, 16, rand);
    io_close(rand);

    enum AES_Mode mode = AES_OFB;
    IO input = io_open_cstring("Hello World!    This is a text!!SomeMoreTextForU");
    IO ciphertext = io_open_aes_encrypt(input, AES_192, mode, test_aes_ecb_key192, iv, "rb");
    IO pciphertext = io_open_aes_encrypt(input, AES_192, mode, test_aes_ecb_key192, iv, "rb<");
    IO aes = io_open_aes_decrypt(ciphertext, AES_192, mode, test_aes_ecb_key192, iv, "rb");
    IO paes = io_open_aes_decrypt(pciphertext, AES_192, mode, test_aes_ecb_key192, iv, "rb<");

    while (1) {
        int ch;
        if ((ch = io_getc(aes)) == EOF) {
            if (io_error(aes))
                puts("Error encountered while reading AES");
            else
                puts("End of stream reached");
            break;
        }

        putc(ch, stdout);
    }

    puts("");
    io_rewind(input);

    while (1) {
        int ch;
        if ((ch = io_getc(paes)) == EOF) {
            if (io_error(paes))
                puts("Error encountered while reading AES");
            else
                puts("End of stream reached");
            break;
        }

        putc(ch, stdout);
    }

    io_close(aes);
    io_close(pciphertext);
    io_close(ciphertext);
    io_close(input);

    IO plaintext = io_open_file(stdout);
    IO hex = io_open_hex_encode(plaintext, "wb");
    aes = io_open_aes_encrypt(hex, AES_192, AES_ECB, test_aes_ecb_key192, NULL, "wb");

    printf("Encrypted: ");
    io_putc(test_aes_plaintext[0], aes);
    io_write(test_aes_plaintext + 1, 3, 1, aes);
    io_write(test_aes_plaintext + 4, 12, 1, aes);
    printf("\nEncrypted: ");
    io_write(test_aes_plaintext + 16, 16, 1, aes);
    printf("\nEncrypted: ");
    io_write(test_aes_plaintext + 32, 16, 1, aes);
    printf("\nEncrypted: ");
    io_write(test_aes_plaintext + 48, 16, 1, aes);
    puts("");

    io_close(aes);
    io_close(hex);
    io_close(plaintext);
}

struct AES_ctx {
    unsigned char iv[16]; /* Initialization vector */
    unsigned char previous[16]; /* Stores previous-block/IV/data-needed-for-next-iteration */
    unsigned char state[16]; /* Data payload should be put in this buffer before encryption/decryption */
    unsigned char expandedKey[16 * 15]; /* Key schedule; first bytes are original key */
    unsigned char *buffer; /* Data payload should be read out from here after encryption/decryption */
    IO io; /* Underlying IO device */
    void (*cb)(struct AES_ctx *ctx); /* Callback for encryption/decryption; pass pointer to this struct as argument */
    enum AES_Mode mode; /* Encryption mode, handled by AESEncode/AESDecode */
    unsigned char rounds; /* Number of iterations required for current key size (the number of rounds is stored instead of key size) */
    unsigned char pos;
    /* pos contains the number of bytes written from the state when writing (0-15), 16 means flush the state */
    /* pos contains the number of bytes available to read from the state when reading (1-16), 0 means fill the state */
};

static const unsigned char sbox[] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

static const unsigned char inv_sbox[] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d,
};

static const unsigned char Rcon[] = {
    0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d, 0x9a,
    0x2f, 0x5e, 0xbc, 0x63, 0xc6, 0x97, 0x35, 0x6a, 0xd4, 0xb3, 0x7d, 0xfa, 0xef, 0xc5, 0x91, 0x39,
    0x72, 0xe4, 0xd3, 0xbd, 0x61, 0xc2, 0x9f, 0x25, 0x4a, 0x94, 0x33, 0x66, 0xcc, 0x83, 0x1d, 0x3a,
    0x74, 0xe8, 0xcb, 0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8,
    0xab, 0x4d, 0x9a, 0x2f, 0x5e, 0xbc, 0x63, 0xc6, 0x97, 0x35, 0x6a, 0xd4, 0xb3, 0x7d, 0xfa, 0xef,
    0xc5, 0x91, 0x39, 0x72, 0xe4, 0xd3, 0xbd, 0x61, 0xc2, 0x9f, 0x25, 0x4a, 0x94, 0x33, 0x66, 0xcc,
    0x83, 0x1d, 0x3a, 0x74, 0xe8, 0xcb, 0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b,
    0x36, 0x6c, 0xd8, 0xab, 0x4d, 0x9a, 0x2f, 0x5e, 0xbc, 0x63, 0xc6, 0x97, 0x35, 0x6a, 0xd4, 0xb3,
    0x7d, 0xfa, 0xef, 0xc5, 0x91, 0x39, 0x72, 0xe4, 0xd3, 0xbd, 0x61, 0xc2, 0x9f, 0x25, 0x4a, 0x94,
    0x33, 0x66, 0xcc, 0x83, 0x1d, 0x3a, 0x74, 0xe8, 0xcb, 0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20,
    0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d, 0x9a, 0x2f, 0x5e, 0xbc, 0x63, 0xc6, 0x97, 0x35,
    0x6a, 0xd4, 0xb3, 0x7d, 0xfa, 0xef, 0xc5, 0x91, 0x39, 0x72, 0xe4, 0xd3, 0xbd, 0x61, 0xc2, 0x9f,
    0x25, 0x4a, 0x94, 0x33, 0x66, 0xcc, 0x83, 0x1d, 0x3a, 0x74, 0xe8, 0xcb, 0x8d, 0x01, 0x02, 0x04,
    0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d, 0x9a, 0x2f, 0x5e, 0xbc, 0x63,
    0xc6, 0x97, 0x35, 0x6a, 0xd4, 0xb3, 0x7d, 0xfa, 0xef, 0xc5, 0x91, 0x39, 0x72, 0xe4, 0xd3, 0xbd,
    0x61, 0xc2, 0x9f, 0x25, 0x4a, 0x94, 0x33, 0x66, 0xcc, 0x83, 0x1d, 0x3a, 0x74, 0xe8, 0xcb, 0x8d
};

static void AddOne(unsigned char *ctr) {
    uint8_t carry = 1;
    for (int i = 15; i >= 0; --i) {
        int temp = ctr[i] + carry;
        ctr[i] = temp;
        carry = temp >> 8;
    }
}

static void SubBytes(unsigned char *state) {
    for (int i = 0; i < 16; ++i)
        state[i] = sbox[state[i]];
}

static void InvSubBytes(unsigned char *state) {
    for (int i = 0; i < 16; ++i)
        state[i] = inv_sbox[state[i]];
}

#define AT(column, row) ((row) + ((column) * 4))

static void ShiftRows(unsigned char *state) {
    uint8_t temp;

    temp = state[AT(0, 1)];
    state[AT(0, 1)] = state[AT(1, 1)];
    state[AT(1, 1)] = state[AT(2, 1)];
    state[AT(2, 1)] = state[AT(3, 1)];
    state[AT(3, 1)] = temp;

    temp = state[AT(0, 2)];
    state[AT(0, 2)] = state[AT(2, 2)];
    state[AT(2, 2)] = temp;

    temp = state[AT(1, 2)];
    state[AT(1, 2)] = state[AT(3, 2)];
    state[AT(3, 2)] = temp;

    temp = state[AT(0, 3)];
    state[AT(0, 3)] = state[AT(3, 3)];
    state[AT(3, 3)] = state[AT(2, 3)];
    state[AT(2, 3)] = state[AT(1, 3)];
    state[AT(1, 3)] = temp;
}

static void InvShiftRows(unsigned char *state) {
    uint8_t temp;

    temp = state[AT(0, 1)];
    state[AT(0, 1)] = state[AT(3, 1)];
    state[AT(3, 1)] = state[AT(2, 1)];
    state[AT(2, 1)] = state[AT(1, 1)];
    state[AT(1, 1)] = temp;

    temp = state[AT(0, 2)];
    state[AT(0, 2)] = state[AT(2, 2)];
    state[AT(2, 2)] = temp;

    temp = state[AT(1, 2)];
    state[AT(1, 2)] = state[AT(3, 2)];
    state[AT(3, 2)] = temp;

    temp = state[AT(0, 3)];
    state[AT(0, 3)] = state[AT(1, 3)];
    state[AT(1, 3)] = state[AT(2, 3)];
    state[AT(2, 3)] = state[AT(3, 3)];
    state[AT(3, 3)] = temp;
}

static void AddRoundKey(struct AES_ctx *ctx, int round) {
    memxor(ctx->buffer, ctx->expandedKey + round * 16, 16);
}

static uint8_t xtime(uint8_t v) {
    return (uint8_t) ((v << 1) ^ (((v >> 7) & 1) * 0x1b));
}

static uint8_t GF2(uint8_t b) {
    return xtime(b);
}

static uint8_t GF3(uint8_t b) {
    return xtime(b) ^ b;
}

static uint8_t GF(uint8_t a, uint8_t b) {
    uint8_t result = (a & 1) * b;

    for (a >>= 1; a; a >>= 1) {
        b = xtime(b);
        if (a & 1)
            result ^= b;
    }

    return result;
}

static void MixColumns(struct AES_ctx *ctx) {
    uint8_t i;
    unsigned char state[4][4];

    memcpy(state, ctx->buffer, 16);

    for (i = 0; i < 4; ++i) {
        ctx->buffer[AT(i, 0)] = GF(2, state[i][0]) ^ GF(3, state[i][1]) ^ state[i][2] ^ state[i][3];
        ctx->buffer[AT(i, 1)] = GF(2, state[i][1]) ^ GF(3, state[i][2]) ^ state[i][3] ^ state[i][0];
        ctx->buffer[AT(i, 2)] = GF(2, state[i][2]) ^ GF(3, state[i][3]) ^ state[i][0] ^ state[i][1];
        ctx->buffer[AT(i, 3)] = GF(2, state[i][3]) ^ GF(3, state[i][0]) ^ state[i][1] ^ state[i][2];
    }
}

static void InvMixColumns(struct AES_ctx *ctx) {
    uint8_t i;
    unsigned char state[4][4];

    memcpy(state, ctx->buffer, 16);

    for (i = 0; i < 4; ++i) {
        ctx->buffer[AT(i, 0)] = GF(0xe, state[i][0]) ^ GF(0xb, state[i][1]) ^ GF(0xd, state[i][2]) ^ GF(0x9, state[i][3]);
        ctx->buffer[AT(i, 1)] = GF(0xe, state[i][1]) ^ GF(0xb, state[i][2]) ^ GF(0xd, state[i][3]) ^ GF(0x9, state[i][0]);
        ctx->buffer[AT(i, 2)] = GF(0xe, state[i][2]) ^ GF(0xb, state[i][3]) ^ GF(0xd, state[i][0]) ^ GF(0x9, state[i][1]);
        ctx->buffer[AT(i, 3)] = GF(0xe, state[i][3]) ^ GF(0xb, state[i][0]) ^ GF(0xd, state[i][1]) ^ GF(0x9, state[i][2]);
    }
}

static void ExpandKey(struct AES_ctx *ctx) {
    int i;
    uint8_t tempa[4];

    int Nk = ctx->rounds - 6; /* Number of 32-bit words in a key */

    for (i = Nk; i < 4 * (ctx->rounds + 1); ++i) {
        memcpy(tempa, ctx->expandedKey + ((i-1) * 4), 4);

        if (i % Nk == 0) {
            uint8_t t = tempa[0];
            tempa[0] = sbox[tempa[1]] ^ Rcon[i/Nk];
            tempa[1] = sbox[tempa[2]];
            tempa[2] = sbox[tempa[3]];
            tempa[3] = sbox[t];
        }

        if (Nk > 6 && i % 4 == 0) {
            tempa[0] = sbox[tempa[0]];
            tempa[1] = sbox[tempa[1]];
            tempa[2] = sbox[tempa[2]];
            tempa[3] = sbox[tempa[3]];
        }

        memcpy(ctx->expandedKey + i * 4, ctx->expandedKey + (i - Nk) * 4, 4);
        memxor(ctx->expandedKey + i * 4, tempa, 4);
    }
}

static void AESEncodeInternal(struct AES_ctx *ctx) {
    uint8_t round = 0;

    AddRoundKey(ctx, 0);

    for (round = 1; round < ctx->rounds; ++round) {
        SubBytes(ctx->buffer);
        ShiftRows(ctx->buffer);
        MixColumns(ctx);
        AddRoundKey(ctx, round);
    }

    SubBytes(ctx->buffer);
    ShiftRows(ctx->buffer);
    AddRoundKey(ctx, round);
}

/* See https://en.wikipedia.org/wiki/Block_cipher_mode_of_operation */
static void AESEncode(struct AES_ctx *ctx) {
    switch (ctx->mode) {
        case AES_ECB:
        default: {
            ctx->buffer = ctx->state;
            AESEncodeInternal(ctx);
            break;
        }
        case AES_CBC: {
            memxor(ctx->previous, ctx->state, 16);
            ctx->buffer = ctx->previous;
            AESEncodeInternal(ctx);
            break;
        }
        case AES_PCBC: {
            memswap(ctx->previous, ctx->state, 16);
            memxor(ctx->state, ctx->previous, 16);
            ctx->buffer = ctx->state;
            AESEncodeInternal(ctx);
            memxor(ctx->previous, ctx->state, 16);
            break;
        }
        case AES_CFB: {
            ctx->buffer = ctx->previous;
            AESEncodeInternal(ctx);
            memxor(ctx->previous, ctx->state, 16);
            break;
        }
        case AES_OFB: {
            ctx->buffer = ctx->previous;
            AESEncodeInternal(ctx);
            memxor(ctx->state, ctx->previous, 16);
            ctx->buffer = ctx->state;
            break;
        }
    }
}

static void AESDecodeInternal(struct AES_ctx *ctx) {
    uint8_t round = 0;

    AddRoundKey(ctx, ctx->rounds);

    for (round = ctx->rounds-1; round > 0; --round) {
        InvShiftRows(ctx->buffer);
        InvSubBytes(ctx->buffer);
        AddRoundKey(ctx, round);
        InvMixColumns(ctx);
    }

    InvShiftRows(ctx->buffer);
    InvSubBytes(ctx->buffer);
    AddRoundKey(ctx, 0);
}

static void AESDecode(struct AES_ctx *ctx) {
    switch (ctx->mode) {
        case AES_ECB:
        default: {
            ctx->buffer = ctx->state;
            AESDecodeInternal(ctx);
            break;
        }
        case AES_CBC: {
            unsigned char ciphertext[16];
            memcpy(ciphertext, ctx->state, 16);
            ctx->buffer = ctx->state;
            AESDecodeInternal(ctx);
            memxor(ctx->state, ctx->previous, 16);
            memcpy(ctx->previous, ciphertext, 16);
            break;
        }
        case AES_PCBC: {
            unsigned char ciphertext[16];
            memcpy(ciphertext, ctx->state, 16);
            ctx->buffer = ctx->state;
            AESDecodeInternal(ctx);
            memxor(ctx->state, ctx->previous, 16);
            memcpy(ctx->previous, ctx->state, 16);
            memxor(ctx->previous, ciphertext, 16);
            break;
        }
        case AES_CFB: {
            memswap(ctx->state, ctx->previous, 16);
            ctx->buffer = ctx->state;
            AESEncodeInternal(ctx); /* sic, not decoding */
            memxor(ctx->state, ctx->previous, 16);
            ctx->buffer = ctx->state;
            break;
        }
        case AES_OFB: {
            ctx->buffer = ctx->previous;
            AESEncodeInternal(ctx); /* sic, not decoding */
            memxor(ctx->state, ctx->previous, 16);
            ctx->buffer = ctx->state;
            break;
        }
    }
}

#ifdef AES_COMPILE_SUPPORTS_X86_INTRINSICS
static __m128i AESEncodeInternal_x86(struct AES_ctx *ctx, __m128i state) {
    state = _mm_xor_si128(state, _mm_loadu_si128(((__m128i *) ctx->expandedKey) + 0));

    for (int i = 1; i < ctx->rounds; ++i)
        state = _mm_aesenc_si128(state, _mm_loadu_si128(((__m128i *) ctx->expandedKey) + i));

    return _mm_aesenclast_si128(state, _mm_loadu_si128(((__m128i *) ctx->expandedKey) + ctx->rounds));
}

static __m128i AESDecodeInternal_x86(struct AES_ctx *ctx, __m128i state) {
    state = _mm_xor_si128(state, _mm_loadu_si128(((__m128i *) ctx->expandedKey) + ctx->rounds));

    for (int i = ctx->rounds-1; i > 0; --i)
        state = _mm_aesdec_si128(state, _mm_loadu_si128(((__m128i *) ctx->expandedKey) + i));

    return _mm_aesdeclast_si128(state, _mm_loadu_si128(((__m128i *) ctx->expandedKey) + 0));
}

static void AESEncode_x86(struct AES_ctx *ctx) {
    __m128i state = _mm_loadu_si128((__m128i *) ctx->state);

    switch (ctx->mode) {
        case AES_ECB:
        default: {
            state = AESEncodeInternal_x86(ctx, state);
            _mm_storeu_si128((__m128i *) ctx->state, state);
            ctx->buffer = ctx->state;
            break;
        }
        case AES_CBC: {
            __m128i previous = _mm_loadu_si128((__m128i *) ctx->previous);
            previous = _mm_xor_si128(previous, state);
            previous = AESEncodeInternal_x86(ctx, previous);
            _mm_storeu_si128((__m128i *) ctx->previous, previous);
            ctx->buffer = ctx->previous;
            break;
        }
        case AES_PCBC: {
            __m128i plaintext = state;
            __m128i previous = _mm_loadu_si128((__m128i *) ctx->previous);
            state = _mm_xor_si128(state, previous);
            state = AESEncodeInternal_x86(ctx, state);
            _mm_storeu_si128((__m128i *) ctx->state, state);
            previous = _mm_xor_si128(plaintext, state);
            _mm_storeu_si128((__m128i *) ctx->previous, previous);
            ctx->buffer = ctx->state;
            break;
        }
        case AES_CFB: {
            __m128i previous = _mm_loadu_si128((__m128i *) ctx->previous);
            previous = AESEncodeInternal_x86(ctx, previous);
            previous = _mm_xor_si128(previous, state);
            _mm_storeu_si128((__m128i *) ctx->previous, previous);
            ctx->buffer = ctx->previous;
            break;
        }
        case AES_OFB: {
            __m128i previous = _mm_loadu_si128((__m128i *) ctx->previous);
            previous = AESEncodeInternal_x86(ctx, previous);
            state = _mm_xor_si128(previous, state);
            _mm_storeu_si128((__m128i *) ctx->previous, previous);
            _mm_storeu_si128((__m128i *) ctx->state, state);
            ctx->buffer = ctx->state;
            break;
        }
    }
}

static void AESDecode_x86(struct AES_ctx *ctx) {
    __m128i state = _mm_loadu_si128((__m128i *) ctx->state);

    switch (ctx->mode) {
        case AES_ECB:
        default: {
            state = AESDecodeInternal_x86(ctx, state);
            _mm_storeu_si128((__m128i *) ctx->state, state);
            ctx->buffer = ctx->state;
            break;
        }
        case AES_CBC: {
            __m128i ciphertext = state;
            __m128i previous = _mm_loadu_si128((__m128i *) ctx->previous);
            state = AESDecodeInternal_x86(ctx, state);
            state = _mm_xor_si128(state, previous);
            _mm_storeu_si128((__m128i *) ctx->state, state);
            _mm_storeu_si128((__m128i *) ctx->previous, ciphertext);
            ctx->buffer = ctx->state;
            break;
        }
        case AES_PCBC: {
            __m128i ciphertext = state;
            __m128i previous = _mm_loadu_si128((__m128i *) ctx->previous);
            state = AESDecodeInternal_x86(ctx, state);
            state = _mm_xor_si128(state, previous);
            _mm_storeu_si128((__m128i *) ctx->state, state);
            ciphertext = _mm_xor_si128(ciphertext, state);
            _mm_storeu_si128((__m128i *) ctx->previous, ciphertext);
            ctx->buffer = ctx->state;
            break;
        }
        case AES_CFB: {
            __m128i previous = _mm_loadu_si128((__m128i *) ctx->previous);
            previous = AESEncodeInternal_x86(ctx, previous); /* sic, not decoding */
            previous = _mm_xor_si128(previous, state);
            _mm_storeu_si128((__m128i *) ctx->state, previous);
            _mm_storeu_si128((__m128i *) ctx->previous, state);
            ctx->buffer = ctx->state;
            break;
        }
        case AES_OFB: {
            __m128i previous = _mm_loadu_si128((__m128i *) ctx->previous);
            previous = AESEncodeInternal_x86(ctx, previous); /* sic, not decoding */
            state = _mm_xor_si128(previous, state);
            _mm_storeu_si128((__m128i *) ctx->previous, previous);
            _mm_storeu_si128((__m128i *) ctx->state, state);
            ctx->buffer = ctx->state;
            break;
        }
    }
}
#endif

static size_t aes_write(const void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    const unsigned char *cptr = ptr;
    struct AES_ctx *aes = userdata;
    size_t max = size*count, blocks = 0;

    while (max) {
        size_t add = max;
        if (add > 16 - (size_t) aes->pos)
            add = 16 - aes->pos;

        memcpy(aes->state + aes->pos, cptr, add);
        aes->pos += add;
        cptr += add;
        max -= add;

        if (aes->pos == 16) {
            aes->cb(aes);

            aes->pos = 0;

            if (io_write(aes->buffer, 16, 1, aes->io) != 1)
                return (size * count - max) / size;
            ++blocks;
        }
    }

    return count;
}

static size_t aes_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    unsigned char *cptr = ptr;
    struct AES_ctx *aes = userdata;
    size_t max = size*count;

    while (max) {
        if (aes->pos == 0) {
            if (io_read(aes->state, 1, 16, aes->io) != 16)
                return io_error(aes->io)? SIZE_MAX: (size * count - max) / size;

            aes->cb(aes);

            aes->pos = 16;
        }

        size_t use = max;
        if (use > aes->pos)
            use = aes->pos;

        memcpy(cptr, aes->buffer + 16 - aes->pos, use);
        aes->pos -= use;
        cptr += use;
        max -= use;
    }

    return count;
}

int aes_close(void *userdata, IO io) {
    UNUSED(io)

    free(userdata);
    return 0;
}

int aes_seek64(void *userdata, long long int offset, int origin, IO io) {
    UNUSED(io)

    struct AES_ctx *aes = userdata;

    return 0;
}

static const struct InputOutputDeviceCallbacks aes_callbacks = {
    .read = aes_read,
    .write = aes_write,
    .open = NULL,
    .close = aes_close,
    .flush = NULL,
    .tell = NULL,
    .tell64 = NULL,
    .seek = NULL,
    .seek64 = NULL
};

IO io_open_aes_encrypt(IO io, enum AES_Type type, enum AES_Mode cipherMode, const unsigned char *key, unsigned char iv[16], const char *mode) {
    struct AES_ctx *ctx = calloc(1, sizeof(struct AES_ctx));
    if (ctx == NULL)
        return NULL;

    IO result = io_open_custom(&aes_callbacks, ctx, mode);
    if (result == NULL) {
        free(ctx);
        return NULL;
    }

    if (iv != NULL) {
        memcpy(ctx->iv, iv, 16);
        memcpy(ctx->previous, iv, 16);
    }

    ctx->io = io;
    ctx->cb = AESEncode;
    ctx->mode = cipherMode;

    switch (type) {
        case AES_128: ctx->rounds = 10; memcpy(ctx->expandedKey, key, 16); break;
        case AES_192: ctx->rounds = 12; memcpy(ctx->expandedKey, key, 24); break;
        case AES_256: ctx->rounds = 14; memcpy(ctx->expandedKey, key, 32); break;
    }

    ExpandKey(ctx);

#ifdef AES_COMPILE_SUPPORTS_X86_INTRINSICS
#if X86_CPU | AMD64_CPU
    /* Detect AES extensions support */
    uint32_t cpuid[4];
    /* TODO: hardware acceleration can be prevented by adding '<' in the open mode; this should be taken out later */
    if (strchr(mode, '<') == NULL && 0 == x86_cpuid(1, 0, cpuid) && TESTBIT(cpuid[2], 25))
        ctx->cb = AESEncode_x86;
#endif
#endif

    return result;
}

IO io_open_aes_decrypt(IO io, enum AES_Type type, enum AES_Mode cipherMode, const unsigned char *key, unsigned char iv[16], const char *mode) {
    struct AES_ctx *ctx = calloc(1, sizeof(struct AES_ctx));
    if (ctx == NULL)
        return NULL;

    IO result = io_open_custom(&aes_callbacks, ctx, mode);
    if (result == NULL) {
        free(ctx);
        return NULL;
    }

    if (iv != NULL) {
        memcpy(ctx->iv, iv, 16);
        memcpy(ctx->previous, iv, 16);
    }

    ctx->io = io;
    ctx->cb = AESDecode;
    ctx->mode = cipherMode;

    switch (type) {
        case AES_128: ctx->rounds = 10; memcpy(ctx->expandedKey, key, 16); break;
        case AES_192: ctx->rounds = 12; memcpy(ctx->expandedKey, key, 24); break;
        case AES_256: ctx->rounds = 14; memcpy(ctx->expandedKey, key, 32); break;
    }

    ExpandKey(ctx);

#ifdef AES_COMPILE_SUPPORTS_X86_INTRINSICS
#if X86_CPU | AMD64_CPU
    /* Detect AES extensions support */
    uint32_t cpuid[4];
    if (strchr(mode, '<') == NULL && 0 == x86_cpuid(1, 0, cpuid) && TESTBIT(cpuid[2], 25)) {
        ctx->cb = AESDecode_x86;

        if (cipherMode < AES_CFB) {
            /* AESIMC instruction needed for implementation reasons for the central keys in the schedule */
            for (int i = 1; i < ctx->rounds; ++i)
                _mm_storeu_si128((((__m128i *) ctx->expandedKey) + i), _mm_aesimc_si128(_mm_loadu_si128(((__m128i *) ctx->expandedKey) + i)));
        }
    }
#endif
#endif

    return result;
}
