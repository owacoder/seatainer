#include "utility.h"

#include <limits.h>

int memswap(void *p, void *q, size_t size)
{
    char *pchar = p, *qchar = q;

    while (size--)
    {
        char tmp = *pchar;
        *pchar++ = *qchar;
        *qchar++ = tmp;
    }

    return 0;
}

static unsigned char *pearson_lookup_table()
{
    /* A table of randomly shuffled values between 0-255 */
    /* Each value appears only once, but in a random position */
    static unsigned char lookup[256] = {
        0xF5, 0x97, 0x5D, 0xF0, 0xFD, 0xB2, 0x0D, 0x48,
        0xA7, 0x28, 0x72, 0x4B, 0xF1, 0x64, 0x68, 0x57,
        0x05, 0x29, 0x2E, 0x3E, 0xE3, 0x4D, 0x93, 0x85,
        0x19, 0x2C, 0xDC, 0x0C, 0x53, 0xB5, 0xAE, 0x2A,
        0xBB, 0x46, 0xD0, 0x6B, 0xD4, 0xC7, 0xA0, 0x83,
        0x96, 0xFE, 0x6C, 0xFB, 0xA6, 0xE7, 0x42, 0x81,
        0xF2, 0xF4, 0xE0, 0x7D, 0x76, 0x50, 0x4C, 0x8F,
        0x26, 0x77, 0xE5, 0xC3, 0xCF, 0x41, 0x84, 0x87,
        0xBD, 0x3A, 0x08, 0xED, 0xAB, 0xBC, 0x89, 0x18,
        0x12, 0xFA, 0x61, 0xDF, 0xD8, 0x31, 0xD5, 0x1E,
        0x4E, 0x1C, 0x40, 0xBE, 0x82, 0x6F, 0xF6, 0x65,
        0x62, 0x37, 0x11, 0x1B, 0xD9, 0x30, 0x33, 0xCB,
        0x3C, 0xF9, 0x98, 0xDB, 0x1F, 0xEE, 0x49, 0x58,
        0x45, 0xC5, 0xCC, 0x99, 0x9E, 0x4F, 0x34, 0x8A,
        0x7A, 0x3D, 0xE9, 0x5B, 0x60, 0x35, 0x8C, 0xF8,
        0x69, 0x1A, 0xA2, 0xE6, 0x0E, 0x55, 0x6A, 0x0F,
        0xF7, 0x1D, 0x27, 0x8E, 0x7C, 0x6E, 0xA9, 0x7F,
        0x38, 0x8D, 0xAC, 0xE4, 0xB8, 0xB1, 0xC1, 0x0B,
        0x17, 0x71, 0x44, 0xB9, 0xD6, 0x6D, 0xDA, 0x52,
        0x5A, 0xA4, 0x21, 0xBA, 0x16, 0xB0, 0xEF, 0x86,
        0xC9, 0xA8, 0xB7, 0xFF, 0x20, 0x56, 0xC6, 0xCE,
        0x91, 0x25, 0x24, 0x32, 0x5F, 0xB3, 0x74, 0x02,
        0xD1, 0x9D, 0x75, 0x95, 0xCA, 0x39, 0xA1, 0xC0,
        0x9A, 0xDD, 0xC4, 0x9F, 0x80, 0xAA, 0xCD, 0x13,
        0x09, 0x22, 0xB4, 0x3F, 0x2F, 0xB6, 0x9C, 0x79,
        0x7E, 0xD3, 0x2D, 0x07, 0x06, 0x88, 0x0A, 0x59,
        0x90, 0x94, 0x70, 0xAD, 0x9B, 0x63, 0x04, 0x5E,
        0xE8, 0xE1, 0xEA, 0x23, 0x66, 0x54, 0x4A, 0xC8,
        0x03, 0x3B, 0x8B, 0xDE, 0x47, 0x2B, 0xFC, 0x15,
        0x73, 0xA5, 0xD2, 0xE2, 0xA3, 0x5C, 0xEB, 0xF3,
        0xEC, 0x7B, 0x01, 0x36, 0x92, 0x00, 0x78, 0xBF,
        0x51, 0x67, 0xC2, 0x43, 0x10, 0xAF, 0x14, 0xD7
    };

    return lookup;
}

unsigned pearson_hash(const char *data, size_t size)
{
    const unsigned char *lookup = pearson_lookup_table();

    const int chunks = sizeof(unsigned int);
    unsigned char hash[sizeof(unsigned int)];
    size_t i, j;
    for (j = 0; j < chunks; ++j)
    {
        hash[j] = size & 0xff;
        hash[j] = lookup[(hash[j] ^ (data[0] + j)) & 0xff];
        for (i = 0; i < size; ++i)
            hash[j] = lookup[(hash[j] ^ data[i]) & 0xff];
    }

    unsigned result = 0;
    for (i = 0; i < chunks; ++i)
    {
        result <<= CHAR_BIT;
        result |= hash[i];
    }

    return result;
}
