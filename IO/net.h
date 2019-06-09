#ifndef NET_H
#define NET_H

#include "../io.h"

#ifdef __cplusplus
extern "C" {
#endif

struct UrlStruct;

typedef struct UrlStruct *Url;

/** @brief Creates a copy of a UTF-8 URL, percent-encoded.
 *
 * The URL may not include percent encoding mixed with UTF-8 characters. @p url will be converted to percent encoding as needed.
 *
 * @param url The URL to copy.
 * @return An allocated string that contains the percent-encoded copy of @p url.
 */
char *url_percent_encoded_from_utf8(const char *url);

/** @brief Creates a URL from a UTF-8 string.
 *
 * @param url The URL to convert to a Url object.
 * @return A new Url containing the URL specified by @p url.
 */
Url url_from_utf8(const char *url);

/** @brief Creates a URL from a percent-encoded string.
 *
 * @param url The URL to convert to a Url object.
 * @return A new Url containing the URL specified by @p url.
 */
Url url_from_percent_encoded(const char *url);

const char *url_get_scheme(Url url);
const char *url_get_authority(Url url);
const char *url_get_username(Url url);
const char *url_get_password(Url url);
const char *url_get_host(Url url);
const char *url_get_port(Url url);
const char *url_get_path(Url url);
const char *url_get_query(Url url);
const char *url_get_fragment(Url url);
const char *url_get_path_and_query_and_fragment(Url url);
const char *url_get_percent_encoded(Url url);

const char *url_to_utf8(Url url);
const char *url_to_percent_encoded(Url url);

void url_destroy(Url url);

#ifdef CC_INCLUDE_NETWORK
enum NetAddressType {
    NetAddressAny,
    NetAddressIPv4,
    NetAddressIPv6
};

IO io_open_tcp_socket(const char *host, unsigned short port, enum NetAddressType type, const char *mode, const char **err);
IO io_open_udp_socket(const char *host, unsigned short port, enum NetAddressType type, const char *mode, const char **err);
#endif

#ifdef __cplusplus
}
#endif

#endif // NET_H
