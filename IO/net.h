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
const char *url_get_host_and_port(Url url);
const char *url_get_host(Url url);
const char *url_get_port(Url url);
unsigned short url_get_port_number(Url url);
const char *url_get_path(Url url);
const char *url_get_query(Url url);
const char *url_get_fragment(Url url);
const char *url_get_path_and_query(Url url);
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

void io_net_init();
void io_net_destroy();

IO io_open_tcp_socket(const char *host, unsigned short port, enum NetAddressType type, const char *mode, int *err);
IO io_open_udp_socket(const char *host, unsigned short port, enum NetAddressType type, const char *mode, int *err);

typedef struct HttpStateStruct *HttpState;
typedef void (*HttpHeaderCallback)(const char *header, const char *value);

HttpState http_create_state(IO http, HttpHeaderCallback responseHeaderCb);
HttpState http_create_state_from_url(Url url, HttpHeaderCallback responseHeaderCb, int *err);
void http_destroy_state(HttpState state);

int http_begin_request(HttpState state, const char *method, Url url);
int http_add_header(HttpState state, const char *header, const char *value);
int http_add_body(HttpState state, IO body, const char *mimeType);
int http_add_body_cstr(HttpState state, const char *body, const char *mimeType);
IO http_request_body(HttpState state, const char *mimeType);

int http_begin_response(HttpState state);
IO http_response_body(HttpState state);
int http_end_response(HttpState state);

#ifdef CC_INCLUDE_SSL
#include <openssl/ssl.h>

IO io_open_ssl_socket(const char *host, unsigned short port, enum NetAddressType type, const char *mode, SSL_CTX *ctx, int *err);
#endif
#endif

#ifdef __cplusplus
#ifdef CC_INCLUDE_NETWORK
class NetIO : public IODevice {
    void closing() {}

public:
    NetIO() {}
    NetIO(const char *host, unsigned short port, enum NetAddressType type = NetAddressAny, bool tcp = true, const char *mode = "rwb") {
        tryOpen(tcp? openTcp(host, port, type, mode): openUdp(host, port, type, mode));
    }

    int openTcp(const char *host, unsigned short port, enum NetAddressType type = NetAddressAny, const char *mode = "rwb") {
        if (isOpen())
            return AlreadyOpen;

        int err;
        m_io = io_open_tcp_socket(host, port, type, mode, &err);

        return m_io? 0: err;
    }
    int openUdp(const char *host, unsigned short port, enum NetAddressType type = NetAddressAny, const char *mode = "rwb") {
        if (isOpen())
            return AlreadyOpen;

        int err;
        m_io = io_open_udp_socket(host, port, type, mode, &err);

        return m_io? 0: err;
    }
};
#endif

}
#endif

#endif // NET_H
