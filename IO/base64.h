/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef BASE64_H
#define BASE64_H

#include "../ccio.h"

#ifdef __cplusplus
extern "C" {
#endif

IO io_open_base64_custom_encode(IO io, const char *alphabet, const char *mode);
IO io_open_base64_custom_decode(IO io, const char *alphabet, const char *mode);

IO io_open_base64_encode(IO io, const char *mode);
IO io_open_base64_decode(IO io, const char *mode);

IO io_open_base64_url_encode(IO io, const char *mode);
IO io_open_base64_url_decode(IO io, const char *mode);

void test_hex();

#ifdef __cplusplus
}
#endif

#endif // BASE64_H
