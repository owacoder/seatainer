#ifndef ZLIB_IO_H
#define ZLIB_IO_H

#include "../io.h"

#ifdef CC_INCLUDE_ZLIB
#include <zlib.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ZlibType {
    GzipDeflate,
    ZlibDeflate,
    RawDeflate,
    GzipOnlyInflate,
    ZlibOnlyInflate,
    GzipOrZlibInflate,
    RawInflate
};

IO io_open_zlib_deflate(IO io, int level, int windowBits, const char *mode);
IO io_open_zlib_deflate2(IO io, int level, int windowBits, int memLevel, int strategy, const char *mode);
IO io_open_zlib_inflate(IO io, int windowBits, const char *mode);

IO io_open_zlib_deflate_easy(IO io, enum ZlibType type, const char *mode);
IO io_open_zlib_inflate_easy(IO io, enum ZlibType type, const char *mode);

#ifdef __cplusplus
}
#endif

#endif

#endif // ZLIB_IO_H
