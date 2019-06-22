/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "aes.h"
#include "../utility.h"
#include <limits.h>
#include <stdlib.h>

#if defined(__SSE2__)
#define AES_COMPILE_SUPPORTS_X86_INTRINSICS
#endif

/** @brief Stores all the information needed for encoding or decoding (but not both) one 16-byte block of AES
 *
 * This structure is not designed to be used by the end-user (nor is it accessible as such), but is designed to be used
 * internally in using an AES IO object:
 *
 * @code
 *     IO out = io_open_file(stdout);
 *     IO aes = io_open_aes_encrypt(out, AES_128, AES_ECB, key, NULL, "w");
 *
 *     ... // Write data to `aes`, since it and the underlying device are opened for writing
 *
 *     io_close(aes);
 *     io_close(out);
 * @endcode
 */
struct AES_ctx {
    /** Stores the 16-byte initialization vector. Does not have to be initialized if mode does not require it. */
    unsigned char iv[16];

    /** Temporary storage for next iteration, depending on current mode. */
    unsigned char previous[16];

    /** Stores the data payload before sending off for encryption or decryption. */
    unsigned char state[16];

    /** Stores the entire expanded Rijndael key. The original key is at the beginning of the array. */
    unsigned char expandedKey[16 * 15];

    /** Where to read the data payload from after encryption or decryption has completed. */
    unsigned char *buffer;

    /** The underlying IO device to read data from or send data to. This device is not closed when the context is destroyed. */
    IO io;

    /** The callback to do the actual encryption or decryption. The function signature for each is identical. The argument `ctx` is a pointer to this struct. */
    void (*cb)(struct AES_ctx *ctx);

    /** Specifies the block-cipher mode of operation, one of `AES_ECB`, `AES_CBC`, `AES_PCBC`, `AES_CFB`, `AES_OFB`, or `AES_CTR` */
    enum AES_Mode mode;

    /** Specifies whether this context points to an encryptor (0) or decryptor (1) */
    unsigned char isDecryptor;

    /** Specifies the number of iterations required for the current key size (the number of rounds is stored instead of key size) */
    unsigned char rounds;

    /** When writing, `pos` contains the number of bytes written to the state (0-15), 16 means flush the state.
     *  When reading, `pos` contains the number of bytes read from the state (1-16), 0 means fill the state.
     */
    unsigned char pos;
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

            if (io_write(aes->buffer, 16, 1, aes->io) != 1) {
                io_set_error(io, io_error(aes->io));
                return (size * count - max) / size;
            }
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
            if (io_read(aes->state, 1, 16, aes->io) != 16) {
                io_set_error(io, io_error(aes->io));
                return io_error(aes->io)? SIZE_MAX: (size * count - max) / size;
            }

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

static int aes_flush(void *userdata, IO io) {
    UNUSED(io)

    struct AES_ctx *aes = userdata;

    return io_flush(aes->io);
}

static int aes_close(void *userdata, IO io) {
    UNUSED(io)

    memset(userdata, 0, sizeof(struct AES_ctx));

    free(userdata);
    return 0;
}

static int aes_state_switch(void *userdata, IO io) {
    UNUSED(io)

    struct AES_ctx *aes = userdata;

    return aes->pos = 0;
}

static int aes_seek64(void *userdata, long long int offset, int origin, IO io) {
    UNUSED(io)

    struct AES_ctx *aes = userdata;

    /* Translate all origins to SEEK_SET for ease of computation */
    switch (origin) {
        case SEEK_END: {
            long long underlyingSize = io_size64(aes->io);
            if (underlyingSize < 0)
                return -1;

            offset += underlyingSize;
            break;
        }
        case SEEK_CUR: {
            long long current = io_tell64(io);
            if (current < 0)
                return -1;

            offset += current;
            break;
        }
    }

    /* If opened as a readable-only device, seeks to any location are permitted, and permitted modes are ECB, CTR, CBC, and CFB.
     * If opened as a readable-writable device, only 16-byte aligned seeks are permitted, and permitted modes are ECB, CTR, CBC, and CFB.
     * If opened as writable-only, only 16-byte aligned seeks and only ECB and CTR modes are allowed.
     *
     * However, if using CBC or CFB modes with a writable device, it is necessary to be extra careful about where to seek.
     * Both modes allow seeking anywhere in the output, but if there is more existing output than what is written afterward,
     * the data will be corrupted.
     *
     *         +-----------------------------------+
     *         | DATA BLOCK                        |
     *         +-----------------------------------+
     *
     *         +------------+----------------------+
     *         | DATA BLOCK | < SEEK LOCATION      |
     *         +------------+----------------------+
     *
     *         +------------+--------------------+-+
     *         | DATA BLOCK | END OF OVERWRITE > | < NOW GARBAGE
     *         +------------+--------------------+-+
     *
     * The already existing block(s) after the end of overwriting will now be garbage. To prevent:
     *
     *    - Write more output than already exists in the output device, or
     *    - Write the same data that already existed in the output device at the current location, or
     *    - Use a mode that doesn't have this issue, like ECB or CTR modes
     */

    /* Ensure writable device seeks are on an aligned boundary */
    if (io_writable(io) && offset % 16 != 0)
        return -1;

    /* Ensure non-readable device is not CBC or CFB */
    if (!io_readable(io) && (aes->mode == AES_CBC || aes->mode == AES_CFB))
        return -1;

    /* Ensure OFB or PCBC seeks are only to offset 0 */
    if ((aes->mode == AES_OFB || aes->mode == AES_PCBC) && offset != 0)
        return -1;

    long long blockAddr = offset - offset % 16;

    if (io_seek64(aes->io, blockAddr, SEEK_SET) < 0)
        return -1;

    switch (aes->mode) {
        default: break;
        case AES_CBC:
        case AES_PCBC:
        case AES_CFB:
        case AES_OFB:
            if (blockAddr == 0) /* Use IV for first block */ {
                memcpy(aes->previous, aes->iv, 16);
            } else {
                char buf[16];

                /* The previous seek on the underlying device ensures the requested block exists.
                 * If it does, and the following previous-block seek succeeds, the following read should never fail. */
                if (io_seek64(aes->io, blockAddr - 16, SEEK_SET) < 0 ||
                    io_read(buf, 1, 16, aes->io) != 16)
                    return -1;

                memcpy(aes->previous, buf, 16);
            }
            break;
    }

    aes->pos = 0;
    offset %= 16;
    if (offset) {
        char dummy[16];
        if (io_read(dummy, 1, offset, io) != offset)
            return -1;
    }

    return 0;
}

static long aes_tell(void *userdata, IO io) {
    UNUSED(io)

    struct AES_ctx *aes = userdata;

    long result = io_tell(aes->io);
    return result < 0? result: io_just_read(io)? result - aes->pos: result + aes->pos;
}

static long long aes_tell64(void *userdata, IO io) {
    UNUSED(io)

    struct AES_ctx *aes = userdata;

    long long result = io_tell64(aes->io);
    return result < 0? result: io_just_read(io)? result - aes->pos: result + aes->pos;
}

static const char *aes_what(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    struct AES_ctx *aes = userdata;

    return aes->isDecryptor? "aes_decode": "aes_encode";
}

static const struct InputOutputDeviceCallbacks aes_callbacks = {
    .read = aes_read,
    .write = aes_write,
    .open = NULL,
    .close = aes_close,
    .flush = aes_flush,
    .stateSwitch = aes_state_switch,
    .tell = aes_tell,
    .tell64 = aes_tell64,
    .seek = NULL,
    .seek64 = aes_seek64,
    .what = aes_what
};

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
    ctx->isDecryptor = 0;

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
    if (strchr(mode, '<') == NULL && 0 == x86_cpuid(1, 0, cpuid) && TESTBIT(cpuid[2], 25))
        ctx->cb = AESEncode_x86;
#endif
#endif

    return result;
}

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
    ctx->isDecryptor = 1;

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
