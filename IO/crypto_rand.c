/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "crypto_rand.h"
#include <limits.h>

#if WINDOWS_OS
#include <windows.h>

typedef BOOLEAN (WINAPI *RtlGenRandomType)(PVOID, ULONG);

struct CryptoRand {
    HMODULE library;
    BOOLEAN (WINAPI *RtlGenRandom)(PVOID buffer, ULONG bufferLength); /* MSVC coughs if WINAPI is outside `()`?? */
};

static void *crypto_rand_open(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    struct CryptoRand *result = calloc(1, sizeof(struct CryptoRand));
    if (result == NULL)
        return result;

    result->library = LoadLibraryA("advapi32");
    if (result->library == NULL) {
        free(result);
        return NULL;
    }

    result->RtlGenRandom = (RtlGenRandomType) GetProcAddress(result->library, "SystemFunction036");
    if (result->RtlGenRandom == NULL) {
        free(result);
        return NULL;
    }

    return result;
}

static int crypto_rand_close(void *userdata, IO io) {
    UNUSED(io)

    struct CryptoRand *cryptoRand = userdata;

    FreeLibrary(cryptoRand->library);
    free(cryptoRand);

    return 0;
}

static size_t crypto_rand_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    if (size == 0 || count == 0)
        return 0;

    struct CryptoRand *cryptoRand = userdata;

    if (cryptoRand->RtlGenRandom(ptr, size*count))
        return size*count;

#if WINDOWS_OS
    io_set_error(io, ERROR_READ_FAULT);
#else
    io_set_error(io, EIO);
#endif
    return SIZE_MAX;
}

static const char *crypto_rand_what(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    return "crypto_rand";
}

static const struct InputOutputDeviceCallbacks crypto_rand_callbacks = {
    .read = crypto_rand_read,
    .write = NULL,
    .open = crypto_rand_open,
    .close = crypto_rand_close,
    .flush = NULL,
    .stateSwitch = NULL,
    .tell = NULL,
    .tell64 = NULL,
    .seek = NULL,
    .seek64 = NULL,
    .what = crypto_rand_what
};

IO io_open_crypto_rand() {
    return io_open_custom(&crypto_rand_callbacks, NULL, "r");
}
#elif LINUX_OS
IO io_open_crypto_rand() {
    return io_open("/dev/urandom", "r");
}
#endif
