#include "seaerror.h"

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
    char *error_string = strerror(err);
    size_t error_string_len = strlen(error_string);

    char *new_string = MALLOC(error_string_len + 1);
    if (new_string != NULL)
        memcpy(new_string, error_string, error_string_len + 1);

    return new_string;
#endif
}
