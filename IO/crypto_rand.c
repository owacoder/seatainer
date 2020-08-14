/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "crypto_rand.h"
#include "../seaerror.h"

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

    struct CryptoRand *result = CALLOC(1, sizeof(struct CryptoRand));
    if (result == NULL)
        return result;

    result->library = LoadLibraryA("advapi32");
    if (result->library == NULL) {
        FREE(result);
        return NULL;
    }

    /* void cast is required to prevent GCC from complaining about incompatible function signatures */
    result->RtlGenRandom = (RtlGenRandomType) (void *) GetProcAddress(result->library, "SystemFunction036");
    if (result->RtlGenRandom == NULL) {
        FREE(result);
        return NULL;
    }

    return result;
}

static int crypto_rand_close(void *userdata, IO io) {
    UNUSED(io)

    struct CryptoRand *cryptoRand = userdata;

    FreeLibrary(cryptoRand->library);
    FREE(cryptoRand);

    return 0;
}

static size_t crypto_rand_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    if (size == 0 || count == 0)
        return 0;

    struct CryptoRand *cryptoRand = userdata;

    size_t max = size*count;

    do {
        ULONG read = max > ULONG_MAX? ULONG_MAX: (ULONG) max;
        if (!cryptoRand->RtlGenRandom(ptr, read)) {
            io_set_error(io, CC_EREAD);
            return SIZE_MAX;
        }

        max -= read;
    } while (max);

    return count;
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
    .clearerr = NULL,
    .stateSwitch = NULL,
    .tell = NULL,
    .tell64 = NULL,
    .seek = NULL,
    .seek64 = NULL,
    .flags = NULL,
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
