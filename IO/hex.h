#ifndef HEX_H
#define HEX_H

#include "../io.h"

IO io_open_hex_encode(IO io, const char *mode);

IO io_open_hex_decode(IO io, const char *mode);

void test_hex();

#endif // HEX_H
