#include "seaerror.h"
#include "utility.h"

#include <string.h>
#include <stdlib.h>

char *error_description_alloc(int err) {
#if WINDOWS_OS
    LPWSTR str = NULL;

    if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR) &str, 0, NULL)) {
        LPSTR result = wide_to_utf8_alloc(str);
        LocalFree(str);
        return result;
    }

    return NULL;
#else
    size_t buf_size = 256;
    char *buf = MALLOC(buf_size);

    do {
        if (strerror_r(err, buf, buf_size) == 0)
            break;

        if (errno != ERANGE) {
            FREE(buf);
            return NULL;
        }

        buf_size *= 2;
        char *new_buf = REALLOC(buf, buf_size);
        if (new_buf == NULL) {
            FREE(buf);
            return NULL;
        }

        buf = new_buf;
    } while (1);

    return buf;
#endif
}

const char *error_description(int err) {
#if WINDOWS_OS
    static WCHAR wbuffer[32768];
    static char buffer[sizeof(wbuffer)/sizeof(*wbuffer) * 4 + 1];

    if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), wbuffer, sizeof(wbuffer)/sizeof(*wbuffer), NULL)) {
        WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, buffer, sizeof(buffer), NULL, NULL);
        return buffer;
    }

    return "";
#else
    return strerror(err);
#endif
}
