/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef HEX_H
#define HEX_H

#include "../io.h"

#ifdef __cplusplus
extern "C" {
#endif

IO io_open_hex_encode(IO io, const char *mode);
IO io_open_hex_decode(IO io, const char *mode);

void test_hex();

#ifdef __cplusplus
}
#endif

#endif // HEX_H
