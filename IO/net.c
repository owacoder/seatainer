/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "net.h"
#include "../utility.h"
#include "../seaerror.h"

#include "limiter.h"
#include "../containers.h"

#include <string.h>
#include <limits.h>
#include <ctype.h>

struct UrlStruct {
    /** Contains the scheme of the URL */
    char *scheme;

    /** Contains the username of the URL. Will be `NULL` if no username is specified. */
    char *username;

    /** Contains the password of the URL. Will be `NULL` if no password is specified. If the ':' is present, but no password, this field will be the empty string. */
    char *password;

    /** Contains the host name of the URL. Will be `NULL` if no host is specified. */
    char *host;

    /** Contains the port of the URL, as a string. Will be `NULL` if no port is specified. */
    char *port;

    /** Contains the path of the URL, percent-encoded. Will never be `NULL` but may be empty */
    char *path;

    /** Contains the query of the URL, percent-encoded. Will be `NULL` if no query is specified */
    char *query;

    /** Contains the fragment of the URL, percent-encoded. Will be `NULL` if no query is specified */
    char *fragment;

    /** Contains the first character of the path. This is so the path can be reconstructed if the first character must be used as a NUL-terminator for `host` or `port` */
    char path_first_char;

    /** Contains the entire URL, percent-encoded. However, this buffer is pointed to by all the other URL sections, and so will have NUL characters embedded in it. */
    char url_buffer[];
};

void url_destroy(Url url) {
    FREE(url);
}

char *url_percent_encoded_from_utf8(const char *url) {

}

static int url_verify(const char *portion, const char *acceptableChars) {
    for (; *portion; ++portion) {
        if (*portion == '%' && strchr(acceptableChars, '%')) {
            if (portion[1] == '%')
                ++portion;
            else if (isxdigit(portion[1]) && isxdigit(portion[2]))
                portion += 2;
            else
                return 0;
        }
        else if (!isalnum(*portion) &&
                !strchr(acceptableChars, *portion))
            return 0;
    }

    return 1;
}

static int url_verify_digits(const char *portion) {
    for (; *portion; ++portion) {
        if (!isdigit(*portion))
            return 0;
    }

    return 1;
}

int url_valid(Url url) {
    return url &&
            isalpha((unsigned char) *url_get_scheme(url)) && url_verify(url_get_scheme(url), "+.-") &&
            (!url_get_username(url) || url_verify(url_get_username(url), "%-._~!$&'()*+,;=")) &&
            (!url_get_password(url) || url_verify(url_get_password(url), "%-._~!$&'()*+,;=:")) &&
            /* TODO: doesn't currently handle IPv6 addresses */
            (!url_get_host(url) || url_verify(url_get_host(url), "%-._~!$&'()*+,;=")) &&
            (!url_get_port(url) || url_verify_digits(url_get_port(url))) &&
            (!url_get_path(url) || ((url_get_path(url)[0] == '/'? url_get_path(url)[1] != '/': 1) && url_verify(url_get_path(url), "%-._~!$&'()*+,;=:@"))) &&
            (!url_get_query(url) || url_verify(url_get_query(url), "%-._~!$&'()*+,;=:@/?")) &&
            (!url_get_fragment(url) || url_verify(url_get_fragment(url), "%-._~!$&'()*+,;=:@/?"));
}

Url url_from_utf8(const char *url) {
    char *percent_encoded = url_percent_encoded_from_utf8(url);
    if (percent_encoded == NULL)
        return NULL;

    Url result = url_from_percent_encoded(percent_encoded);
    FREE(percent_encoded);

    return result;
}

Url url_from_percent_encoded(const char *url) {
    Url result = CALLOC(1, sizeof(*result) + strlen(url) + 1);
    if (result == NULL)
        return NULL;

    strcpy(result->url_buffer, url);

    char *pos, *string = result->url_buffer;

    /* Get scheme */
    pos = strchr(string, ':');
    if (pos == NULL || string == pos)
        goto cleanup;

    result->scheme = string;
    *pos = 0;

    string = pos + 1;

    if (string[0] == '/' && string[1] == '/') {
        /* Get userinfo, host and port if available */
        string += 2;

        char *slash = strchr(string, '/');
        char *at = strchr(string, '@');

        if (at != NULL && (slash == NULL || at < slash)) {
            char *colon = strchr(string, ':');
            result->username = string;

            /* Password exists */
            if (colon != NULL && (at == NULL || colon < at)) {
                *colon = 0;
                result->password = colon + 1;
            } else
                result->password = NULL;

            *at = 0;
            string = at + 1;
        } else
            result->username = NULL;

        result->host = string;
        if (*result->host == '[') {
            for (; *string && *string != ']'; ++string);
        }

        char *colon = strchr(string, ':');

        if (colon != NULL && (slash == NULL || colon < slash)) {
            *colon = 0;
            result->port = string = colon + 1;
        } else
            result->port = NULL;

        strlower(result->host);
        string = slash? slash: string + strlen(string);
    } else {
        result->username = result->password = NULL;
        result->host = NULL;
        result->port = NULL;
    }

    result->path_first_char = *string;
    result->path = string;
    pos = strchr(string, '?');

    /* Query exists */
    if (pos != NULL) {
        /* Get query */
        *pos = 0;
        result->query = string = pos + 1;
    } else
        result->query = NULL;

    pos = strchr(string, '#');
    /* Fragment exists */
    if (pos != NULL) {
        *pos = 0;
        result->fragment = pos + 1;
    } else
        result->fragment = NULL;

    /* Lowercase the scheme */
    strlower(result->scheme);

    return result;

cleanup:
    url_destroy(result);
    return NULL;
}

const char *url_get_scheme(Url url) {
    if (url->username)
        url->username[-3] = 0;
    else if (url->host)
        url->host[-3] = 0;
    else
        url->path[-1] = 0;
    return url->scheme;
}

const char *url_get_authority(Url url) {
    if (!url->host)
        return NULL;

    if (url->password)
        url->password[-1] = ':';

    if (url->username)
        url->host[-1] = '@';

    if (url->port)
        url->port[-1] = ':';

    url->path[0] = 0;

    return url->username? url->username: url->host;
}

const char *url_get_username(Url url) {
    if (!url->username)
        return NULL;

    if (url->password)
        url->password[-1] = 0;
    else
        url->host[-1] = 0;

    return url->username;
}

const char *url_get_password(Url url) {
    if (!url->password)
        return NULL;

    url->host[-1] = 0;

    return url->password;
}

const char *url_get_host_and_port(Url url) {
    if (!url->host)
        return NULL;

    if (url->port)
        url->port[-1] = ':';

    url->path[0] = 0;

    return url->host;
}

const char *url_get_host(Url url) {
    if (url->port)
        url->port[-1] = 0;
    else
        url->path[0] = 0; /* In case the host directly precedes the path */

    return url->host;
}

const char *url_get_port(Url url) {
    url->path[0] = 0;
    return url->port;
}

static unsigned short url_port_from_scheme(Url url) {
    const struct {
        const char *scheme;
        unsigned short port;
    } mapping[] = {
        {.scheme = "http", .port = 80},
        {.scheme = "https", .port = 443}
    };

    const char *scheme = url_get_scheme(url);
    for (size_t i = 0; i < sizeof(mapping)/sizeof(*mapping); ++i) {
        if (!strcmp(scheme, mapping[i].scheme)) {
            return mapping[i].port;
        }
    }

    return 0;
}

unsigned short url_get_port_number(Url url) {
    unsigned short port = 0;
    const char *portStr = url->port;

    if (portStr)
        for (; *portStr && isdigit(*portStr); ++portStr) {
            port = (port * 10) + (*portStr - '0');
        }
    else
        return url_port_from_scheme(url);

    return port;
}

const char *url_get_path(Url url) {
    if (url->query)
        url->query[-1] = 0;
    else if (url->fragment)
        url->fragment[-1] = 0;

    url->path[0] = url->path_first_char;
    return url->path;
}

const char *url_get_query(Url url) {
    if (url->fragment)
        url->fragment[-1] = 0;

    return url->query;
}

const char *url_get_fragment(Url url) {
    return url->fragment;
}

const char *url_get_path_and_query(Url url) {
    if (url->fragment)
        url->fragment[-1] = 0;

    if (url->query)
        url->query[-1] = '?';

    url->path[0] = url->path_first_char;

    return url->path;
}

const char *url_get_path_and_query_and_fragment(Url url) {
    if (url->fragment)
        url->fragment[-1] = '#';

    if (url->query)
        url->query[-1] = '?';

    url->path[0] = url->path_first_char;

    return url->path;
}

const char *url_get_percent_encoded(Url url) {
    if (url->username) {
        url->username[-3] = ':';
        if (url->password)
            url->password[-1] = ':';
        url->host[-1] = '@';
    }
    else if (url->host)
        url->host[-3] = ':';
    else
        url->path[-1] = ':';

    if (url->port)
        url->port[-1] = ':';

    url->path[0] = url->path_first_char;

    if (url->query)
        url->query[-1] = '?';

    if (url->fragment)
        url->fragment[-1] = '#';

    return url->scheme;
}

/**********************************************************
 *                                                        *
 *   BEGIN SOCKET LAYER                                   *
 *                                                        *
 **********************************************************/

#ifdef CC_INCLUDE_NETWORK
#if WINDOWS_OS
#if MSVC_COMPILER
#pragma comment(lib, "Ws2_32.lib")
#endif
#endif

#define CC_SOCKET_TYPE(io) *io_tempdata(io)
#define CC_UDP_SOCKET 0
#define CC_TCP_SOCKET 1
#define CC_SSL_SOCKET 2

#include <signal.h>

#ifdef CC_INCLUDE_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>

struct NetContext {
    SOCKET fd;
    SSL_CTX *ctx;
    SSL *socket;
    char ownsCtx; /* 1 if ctx is owned by this struct, 0 if ctx is external */
};

/* If this function returns CC_EPIPE, don't use the underlying file descriptor anymore */
static int net_upgrade_to_ssl(const char *host, void *userdata, IO io) {
    struct NetContext *context = userdata;

    if (io_flush(io))
        return io_error(io);

    /* Build a context, if no context is assigned by the user */
    if (context->ctx == NULL) {
        context->ownsCtx = 1;
        context->ctx = SSL_CTX_new(TLS_client_method());
        if (context->ctx == NULL)
            return CC_ENOMEM;

        /* Only allow TLSv1.2 and TLSv1.3 */
        if (SSL_CTX_set_min_proto_version(context->ctx, TLS1_2_VERSION) != 1)
            return CC_EPROTO;

        /* Use system certificates */
        if (ssl_load_system_certificates(context->ctx))
            return CC_EREAD;

        /* Make sure we verify the certificates */
        SSL_CTX_set_verify(context->ctx, SSL_VERIFY_PEER, NULL);
    }

    context->socket = SSL_new(context->ctx);
    if (context->socket == NULL)
        return CC_ENOMEM;

    /* Set up Server Name Indication for client */
    /* See https://stackoverflow.com/questions/5113333/how-to-implement-server-name-indication-sni */
    if (SSL_set_tlsext_host_name(context->socket, host) != 1)
        return CC_EPROTO;

    /* Set up automatic hostname verification (OpenSSL v1.0.2 or newer) */
    /* See https://wiki.openssl.org/index.php/Hostname_validation */
    X509_VERIFY_PARAM *x509_verify = SSL_get0_param(context->socket);

    X509_VERIFY_PARAM_set_hostflags(x509_verify, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    if (!X509_VERIFY_PARAM_set1_host(x509_verify, host, strlen(host)))
        return CC_EPROTO;

    /* Assign actual file descriptor to socket */
    if (SSL_set_fd(context->socket, context->fd) != 1)
        return CC_EBADF;

    /* Actual connection attempt */
    int err = SSL_connect(context->socket);

    /* Then ensure that the hostname verification went well. */
    if (SSL_get_verify_result(context->socket) != X509_V_OK)
        return CC_EPROTO;

    if (err == 1) {
        CC_SOCKET_TYPE(io) = CC_SSL_SOCKET;
        return 0;
    }

    io_set_error(io, err < 0? CC_EPIPE: CC_EPROTO);
    return err < 0? CC_EPIPE: CC_EPROTO;
}

static int net_downgrade_from_ssl(void *userdata, IO io) {
    UNUSED(io)
    struct NetContext *context = userdata;

    if (io_flush(io))
        return io_error(io);

    int result = SSL_shutdown(context->socket);
    if (result == 0)
        result = SSL_shutdown(context->socket);

    if (result < 0)
        return SSL_get_error(context->socket, result) == SSL_ERROR_SYSCALL? CC_EPIPE: CC_EPROTO;

    return 0;
}
#else
struct NetContext {
    SOCKET fd;
};
#endif

/* TODO: allow non-blocking connect to allow custom timeout for connecting */
struct SocketInitializationParams {
    const char *host;
    unsigned short port;
    int socketType; /* One of CC_XXX_SOCKET types */
    int socketMode;
    enum NetAddressType type;
    int *err;

#ifdef CC_INCLUDE_SSL
    SSL_CTX *sslContext;
#endif
};

void io_net_init(void) {
#if !WINDOWS_OS
    signal(SIGPIPE, SIG_IGN);
#endif

#ifdef CC_INCLUDE_SSL
    SSL_load_error_strings();
    SSL_library_init();
#endif
}

void io_net_destroy(void) {

}

static size_t net_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)
    struct NetContext *context = userdata;

    char *cptr = ptr;
    size_t max = size*count;

    do {
        int transfer = max > INT_MAX? INT_MAX: (int) max;

#ifdef CC_INCLUDE_SSL
        if (CC_SOCKET_TYPE(io) != CC_SSL_SOCKET) {
#endif
            transfer = recv(context->fd, cptr, transfer, 0);
            if (transfer == SOCKET_ERROR) {
#if WINDOWS_OS
                io_set_error(io, WSAGetLastError());
#else
                io_set_error(io, errno);
#endif
                return SIZE_MAX;
            }
            else if (transfer == 0)
                return (size*count - max) / size;
#ifdef CC_INCLUDE_SSL
        } else /* SSL transfer */ {
            transfer = SSL_read(context->socket, cptr, transfer);
            if (transfer <= 0) {
                const int err = SSL_get_error(context->socket, transfer);
                const int errStack = ERR_get_error();

                switch (err) {
                    default:
                    case SSL_ERROR_SSL: io_set_error(io, CC_EPROTO); return SIZE_MAX;
                    case SSL_ERROR_SYSCALL:
#if WINDOWS_OS
                        io_set_error(io, WSAGetLastError());
#else
                        io_set_error(io, errno);
#endif
                        if (errStack != 0 || io_error(io) != 0) /* A real error occured */
                            return SIZE_MAX;

                        /* Server closed socket without clean shutdown, set Connection Reset error to signify short read and return */
                        io_set_error(io, CC_ECONNRESET);
                        /* fallthrough */
                    case SSL_ERROR_ZERO_RETURN: return (size*count - max) / size;
                }
            }
        }
#endif

        cptr += transfer;
        max -= transfer;
    } while (max);

    return count;
}

static size_t net_write(const void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)
    struct NetContext *context = userdata;

    const char *cptr = ptr;
    size_t max = size*count;

    do {
        int transfer = max > INT_MAX? INT_MAX: (int) max;

        /* TODO: no protection against sending UDP packets that are too large */
#ifdef CC_INCLUDE_SSL
        if (CC_SOCKET_TYPE(io) != CC_SSL_SOCKET) {
#endif
            transfer = send(context->fd, cptr, transfer, 0);
            if (transfer == SOCKET_ERROR) {
#if WINDOWS_OS
                io_set_error(io, WSAGetLastError());
#else
                io_set_error(io, errno);
#endif
                return (size*count - max) / size;
            }
#ifdef CC_INCLUDE_SSL
        } else /* SSL transfer */ {
            transfer = SSL_write(context->socket, cptr, transfer);
            if (transfer <= 0) {
                int err = SSL_get_error(context->socket, transfer);

                switch (err) {
                    default:
                    case SSL_ERROR_SSL: io_set_error(io, CC_EPROTO); return SIZE_MAX;
#if WINDOWS_OS
                    case SSL_ERROR_SYSCALL: io_set_error(io, WSAGetLastError()); return SIZE_MAX;
#else
                    case SSL_ERROR_SYSCALL: io_set_error(io, errno); return SIZE_MAX;
#endif
                }
            }
        }
#endif

        cptr += transfer;
        max -= transfer;
    } while (max);

    return count;
}

static void *net_open(void *userdata, IO io) {
    UNUSED(io)
    struct SocketInitializationParams *params = userdata;
    SOCKET sock = INVALID_SOCKET;

    struct NetContext *context = CALLOC(1, sizeof(*context));
    if (context == NULL) {
        if (params->err)
            *params->err = CC_ENOMEM;
        return NULL;
    }

    if (params->err)
        *params->err = 0;

#if WINDOWS_OS
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 0), &wsaData)) {
        if (params->err)
            *params->err = WSAGetLastError();

        FREE(context);
        return NULL;
    }
#endif

    /* Initialize hints */
    struct addrinfo hints, *result, *curAddr;
    memset(&hints, 0, sizeof(hints));

    switch (params->type) {
        case NetAddressAny: hints.ai_family = AF_UNSPEC; break;
        case NetAddressIPv4: hints.ai_family = AF_INET; break;
        case NetAddressIPv6: hints.ai_family = AF_INET6; break;
    }
    hints.ai_socktype = params->socketMode;
    hints.ai_protocol = params->socketMode == SOCK_STREAM? IPPROTO_TCP: IPPROTO_UDP;

    CC_SOCKET_TYPE(io) = params->socketType;

    /* Convert port number to NUL-terminated string */
    char portStr[sizeof(unsigned short) * CHAR_BIT];
    char *ptr = portStr + sizeof(portStr) - 1;

    *ptr = 0;
    unsigned short port = params->port;
    while (port) {
        *--ptr = '0' + port % 10;
        port /= 10;
    }

    /* Obtain address information */
    if (getaddrinfo(params->host, ptr, &hints, &result)) {
        if (params->err)
#if WINDOWS_OS
            *params->err = WSAGetLastError();
#else
            *params->err = errno;
#endif
        goto cleanup;
    }

    curAddr = result;

    for (; curAddr; curAddr = curAddr->ai_next) {
        /* Create socket for address */
        sock = socket(curAddr->ai_family, curAddr->ai_socktype, curAddr->ai_protocol);
        if (sock == INVALID_SOCKET) {
            if (params->err)
#if WINDOWS_OS
                *params->err = WSAGetLastError();
#else
                *params->err = errno;
#endif
            continue;
        }

        /* Then attempt to connect */
        if (connect(sock, curAddr->ai_addr, (int) curAddr->ai_addrlen)) {
            if (params->err)
#if WINDOWS_OS
                *params->err = WSAGetLastError();
#else
                *params->err = errno;
#endif

            if (sock != INVALID_SOCKET)
#if WINDOWS_OS
                closesocket(sock);
#elif LINUX_OS
                close(sock);
#endif
            sock = INVALID_SOCKET;
            continue;
        }
    }

    freeaddrinfo(result);

    /* Valid connected socket, now just initialize SSL as needed */
    if (sock != INVALID_SOCKET) {
        context->fd = sock;

#ifdef CC_INCLUDE_SSL
        if (CC_SOCKET_TYPE(io) == CC_SSL_SOCKET) { /* SSL connection specified instead of regular TCP */
            context->ctx = params->sslContext;
            context->ownsCtx = 0;

            CC_SOCKET_TYPE(io) = CC_TCP_SOCKET; /* Downgrade specifier to TCP; it will be upgraded in net_upgrade_to_ssl() */
            int result = net_upgrade_to_ssl(params->host, context, io);
            if (result) {
                printf("Error: %s\n", error_description(result));
                if (params->err)
                    *params->err = result;
                goto cleanup;
            }
        }
#endif

        return context;
    }

cleanup:
#if WINDOWS_OS
    if (sock != INVALID_SOCKET)
        closesocket(sock);
    WSACleanup();
#elif LINUX_OS
    if (sock != INVALID_SOCKET)
        close(sock);
#endif

#ifdef CC_INCLUDE_SSL
    if (context->socket)
        SSL_free(context->socket);

    if (context->ownsCtx && context->ctx)
        SSL_CTX_free(context->ctx);
#endif

    FREE(context);
    return NULL;
}

static int net_close(void *userdata, IO io) {
    struct NetContext *context = userdata;

    int result = 0;

#ifdef CC_INCLUDE_SSL
    if (CC_SOCKET_TYPE(io) == CC_SSL_SOCKET) {
        if (io_error(io) != CC_EPROTO) {
            result = SSL_shutdown(context->socket);

            /* If result == 0, the server didn't send a close_notify yet, but we don't care because we're just going to shutdown anyway */
            if (result < 0)
                result = SSL_get_error(context->socket, result) == SSL_ERROR_SYSCALL? CC_EPIPE: CC_EPROTO;
            else
                result = 0;
        }

        SSL_free(context->socket);
    }

    if (context->ownsCtx)
        SSL_CTX_free(context->ctx);
#endif

#if WINDOWS_OS
    int closeResult = closesocket(context->fd);

    WSACleanup();
#elif LINUX_OS
    int closeResult = close(context->fd);
#endif

    if (result == 0)
        result = closeResult;

    FREE(context);
    return result;
}

static unsigned long net_flags(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    return IO_FLAG_SUPPORTS_NO_STATE_SWITCH;
}

static int net_shutdown(void *userdata, IO io, int how) {
    struct NetContext *context = userdata;

#ifdef CC_INCLUDE_SSL
    if (CC_SOCKET_TYPE(io) == CC_SSL_SOCKET) {
        if (how != IO_SHUTDOWN_READWRITE) {
            io_set_error(io, CC_ENOTSUP);
            return EOF;
        }

        int err = net_downgrade_from_ssl(userdata, io);
        if (err) {
            io_set_error(io, err);
            return EOF;
        }

        SSL_free(context->socket);
        CC_SOCKET_TYPE(io) = CC_TCP_SOCKET;
    }
#endif

    if (shutdown(context->fd, how)) {
#if WINDOWS_OS
        io_set_error(io, WSAGetLastError());
#else
        io_set_error(io, errno);
#endif
        return EOF;
    }

    return 0;
}

static const char *net_what(void *userdata, IO io) {
    UNUSED(userdata)

    switch (CC_SOCKET_TYPE(io)) {
        default: return "unknown_socket";
        case CC_UDP_SOCKET: return "udp_socket";
        case CC_TCP_SOCKET: return "tcp_socket";
        case CC_SSL_SOCKET: return "ssl_socket";
    }
}

static const struct InputOutputDeviceCallbacks net_callbacks = {
    .read = net_read,
    .write = net_write,
    .open = net_open,
    .close = net_close,
    .flush = NULL,
    .clearerr = NULL,
    .state_switch = NULL,
    .tell = NULL,
    .tell64 = NULL,
    .seek = NULL,
    .seek64 = NULL,
    .flags = net_flags,
    .shutdown = net_shutdown,
    .what = net_what
};

IO io_open_tcp_socket(const char *host, unsigned short port, enum NetAddressType type, const char *mode, int *err) {
    struct SocketInitializationParams params =
    {
        .host = host,
        .port = port,
        .socketType = CC_TCP_SOCKET,
        .socketMode = SOCK_STREAM,
        .type = type,
        .err = err
    };

    return io_open_custom(&net_callbacks, &params, mode);
}

IO io_open_udp_socket(const char *host, unsigned short port, enum NetAddressType type, const char *mode, int *err) {
    struct SocketInitializationParams params =
    {
        .host = host,
        .port = port,
        .socketType = CC_UDP_SOCKET,
        .socketMode = SOCK_DGRAM,
        .type = type,
        .err = err
    };

    return io_open_custom(&net_callbacks, &params, mode);
}

#ifdef CC_INCLUDE_SSL
IO io_open_ssl_socket(const char *host, unsigned short port, enum NetAddressType type, const char *mode, SSL_CTX *ctx, int *err) {
    struct SocketInitializationParams params =
    {
        .host = host,
        .port = port,
        .socketType = CC_SSL_SOCKET,
        .socketMode = SOCK_STREAM,
        .type = type,
        .err = err,
        .sslContext = ctx
    };

    return io_open_custom(&net_callbacks, &params, mode);
}

#if WINDOWS_OS
int ssl_get_system_certificates(IO out, const char *section) {
    HCERTSTORE hStore;
    PCCERT_CONTEXT pCertContext = NULL;
    int result = 0;

    if (!(hStore = CertOpenStore(
              CERT_STORE_PROV_SYSTEM_A,
              0,
              NULL,
              CERT_SYSTEM_STORE_CURRENT_USER,
              section)))
        return GetLastError();

    do
    {
        if (pCertContext = CertFindCertificateInStore(
                  hStore,
                  X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                  0,
                  CERT_FIND_ANY,
                  NULL,
                  pCertContext))
        {
            SYSTEMTIME systemNow;
            FILETIME fileNow;
            ULARGE_INTEGER diff1, diff2;
            const DWORD flags = CRYPT_STRING_BASE64HEADER;
            DWORD len;

            // Only accept currently valid certificates
            GetSystemTime(&systemNow);

            if (!SystemTimeToFileTime(
                      &systemNow,
                      &fileNow))
                break;

            diff1.LowPart = fileNow.dwLowDateTime;
            diff1.HighPart = fileNow.dwHighDateTime;

            diff2.LowPart = pCertContext->pCertInfo->NotBefore.dwLowDateTime;
            diff2.HighPart = pCertContext->pCertInfo->NotBefore.dwHighDateTime;

            if (MIN(diff1.QuadPart, diff2.QuadPart) < diff2.QuadPart)
                continue;

            diff2.LowPart = pCertContext->pCertInfo->NotAfter.dwLowDateTime;
            diff2.HighPart = pCertContext->pCertInfo->NotAfter.dwHighDateTime;

            if (MAX(diff1.QuadPart, diff2.QuadPart) > diff2.QuadPart)
                continue;

            // Get Base64 string length
            if (!CryptBinaryToStringA(
                      pCertContext->pbCertEncoded,
                      pCertContext->cbCertEncoded,
                      flags,
                      NULL,
                      &len))
                break;

            if (len)
            {
                PCHAR str = MALLOC(len);
                if (str == NULL) {
                    result = CC_ENOMEM;
                    break;
                }

                if (!CryptBinaryToStringA(
                          pCertContext->pbCertEncoded,
                          pCertContext->cbCertEncoded,
                          flags,
                          str,
                          &len))
                {
                    FREE(str);
                    result = GetLastError();
                    break;
                }

                if (io_write(str, 1, len, out) != out ||
                        io_puts("\r\n", out)) {
                    FREE(str);
                    result = io_error(out);
                    break;
                }

                FREE(str);
            }
        }
    } while (pCertContext);

    if (!pCertContext)
        CertFreeCertificateContext(pCertContext);

    CertCloseStore(
              hStore,
              CERT_CLOSE_STORE_FORCE_FLAG);

    return result;
}

int ssl_load_system_certificates(SSL_CTX *ctx) {
    char name[L_tmpnam];

    tmpnam(name);

    /* Write system certificates to temporary file */
    IO tmp = io_open(name, "wb");
    if (tmp == NULL)
        return CC_ENOMEM;

    int result = ssl_get_system_certificates(tmp, "CA");
    if (result)
        goto cleanup;

    io_close(tmp);
    tmp = NULL;

    /* Then load into context and remove file */
    if (!SSL_CTX_load_verify_locations(ctx, name, NULL))
        goto cleanup;

    remove(name);

    return 0;

cleanup:
    io_close(tmp);
    remove(name);
    return result;
}
#else
int ssl_load_system_certificates(SSL_CTX *ctx) {
    const char *locations[] = {
        "/etc/ssl/certs/ca-certificates.crt",   /* Debian, Ubuntu, Gentoo */
        "/etc/pki/tls/certs/ca-bundle.crt",     /* Fedora/RHEL 6 */
        "/etc/ssl/ca-bundle.pem",               /* OpenSUSE */
        "/etc/pki/tls/cacert.pem",              /* OpenELEC */
        "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem", /* CentOS/RHEL 7 */
        NULL
    };

    const char *directories[] = {
        "/etc/ssl/certs",                       /* SLES10/SLES11 */
        "/system/etc/security/cacerts",         /* Android */
        "/usr/local/share/certs",               /* FreeBSD */
        "/etc/pki/tls/certs",                   /* Fedora/RHEL */
        "/etc/openssl/certs",                   /* NetBSD */
        NULL
    };

    const char **location = locations;
    for (; *location; ++location) {
        FILE *f = fopen(*location, "rb");
        if (f == NULL)
            continue;

        fclose(f);

        if (SSL_CTX_load_verify_locations(ctx, *location, NULL))
            return 0;
    }

    const char **directory = directories;
    for (; *directory; ++directory) {
        if (SSL_CTX_load_verify_locations(ctx, NULL, *directory))
            return 0;
    }

    return CC_EREAD;
}
#endif
#endif

struct HttpChunkedStruct {
    IO io;

    /** @brief Total number of bytes read so far */
    unsigned long long totalBytes;

    /** @brief Length of last chunk that was read. ULONG_LONG_MAX means no chunk has been read yet. */
    unsigned long long chunkLength;

    /** @brief Number of characters left to read in the current chunk. */
    unsigned long long chunkAvail;
};

static size_t http_chunked_read(void *buf, size_t size, size_t count, void *userdata, IO io) {
    struct HttpChunkedStruct *chunked = userdata;

    size_t max = size*count;
    char *cbuf = buf;

    if (chunked->chunkLength == 0 && chunked->chunkAvail == 0)
        return 0;

    do {
        if (chunked->chunkAvail > max) {
            size_t read = io_read(cbuf, 1, max, chunked->io);
            io_set_error(io, io_error(chunked->io));

            chunked->chunkAvail -= read;
            chunked->totalBytes += read;
            max -= read;
            cbuf += read;
        } else if (chunked->chunkAvail) {
            size_t read = io_read(cbuf, 1, (size_t) chunked->chunkAvail, chunked->io);
            io_set_error(io, io_error(chunked->io));

            if (read != chunked->chunkAvail) {
                if (!io_error(io))
                    io_set_error(io, CC_EBADMSG);
                puts("End of chunk could not be read");
                break;
            }

            chunked->chunkAvail -= read;
            chunked->totalBytes += read;
            max -= read;
            cbuf += read;

            if (io_getc(chunked->io) != '\r' || io_getc(chunked->io) != '\n') {
                puts("Bad end of chunk");
                io_set_error(io, CC_EBADMSG);
                break;
            }
        }

        if (chunked->chunkAvail == 0) {
            /* Read chunk size */
            if (io_scanf(chunked->io, "%llx", &chunked->chunkAvail) != 1 || chunked->chunkAvail == ~0ull) {
                io_set_error(io, CC_EBADMSG);
                break;
            }

            /* Discard chunk extension, if any */
            int ch = io_getc(chunked->io);
            if (ch == ';') {
                int ch = 0;
                while (ch != '\r' && ch != EOF)
                    ch = io_getc(chunked->io);

                if (ch == EOF || io_getc(chunked->io) != '\n') {
                    io_set_error(io, io_error(chunked->io));
                    return SIZE_MAX;
                }
            } else if (ch != '\r' || io_getc(chunked->io) != '\n') {
                io_set_error(io, io_error(chunked->io));
                return SIZE_MAX;
            }

            if (chunked->chunkAvail == 0) /* Must have parsed last chunk */
                break;
        }
    } while (max);

    return (size*count - max) / size;
}

static size_t http_chunked_write(const void *buf, size_t size, size_t count, void *userdata, IO io) {
    struct HttpChunkedStruct *chunked = userdata;

    if (io_printf(chunked->io, "%zx\r\n", size*count) < 0)
        goto cleanup;

    size_t written = io_write(buf, size, count, chunked->io) / size;
    io_set_error(io, io_error(chunked->io));

    if (!io_error(chunked->io) && io_puts("\r\n", chunked->io) != 0)
        goto cleanup;

    return written;

cleanup:
    io_set_error(io, io_error(chunked->io)? io_error(chunked->io): CC_EWRITE);
    return 0;
}

static void http_chunked_clearerr(void *userdata, IO io) {
    UNUSED(io)

    struct HttpChunkedStruct *chunked = userdata;

    io_clearerr(chunked->io);
}

static int http_chunked_flush(void *userdata, IO io) {
    struct HttpChunkedStruct *chunked = userdata;

    int result = io_flush(chunked->io);
    io_set_error(io, io_error(chunked->io));

    return result;
}

static void *http_chunked_open(void *userdata, IO io) {
    UNUSED(io)

    struct HttpChunkedStruct *chunked = CALLOC(1, sizeof(*chunked));
    if (chunked == NULL)
        return NULL;

    chunked->io = userdata;
    chunked->chunkLength = ~0ull;

    return chunked;
}

static int http_chunked_close(void *userdata, IO io) {
    struct HttpChunkedStruct *chunked = userdata;

    if (io_just_wrote(io)) {
        if (io_puts("0\r\n", chunked->io) != 0)
            goto cleanup;
    }

    FREE(chunked);
    return 0;

cleanup:
    io_set_error(io, io_error(chunked->io)? io_error(chunked->io): 0);

    FREE(chunked);
    return io_error(io);
}

static signed long long http_chunked_tell64(void *userdata, IO io) {
    UNUSED(io)
    struct HttpChunkedStruct *chunked = userdata;

    return chunked->totalBytes;
}

static const char *http_chunked_what(void *userdata, IO io) {
    UNUSED(userdata)
    UNUSED(io)

    return "http_chunked";
}

static const struct InputOutputDeviceCallbacks http_chunked_callbacks = {
    .read = http_chunked_read,
    .write = http_chunked_write,
    .open = http_chunked_open,
    .close = http_chunked_close,
    .flush = http_chunked_flush,
    .clearerr = http_chunked_clearerr,
    .state_switch = NULL,
    .tell = NULL,
    .tell64 = http_chunked_tell64,
    .seek = NULL,
    .seek64 = NULL,
    .flags = NULL,
    .what = http_chunked_what
};

static IO io_open_http_chunked(IO io, const char *mode) {
    return io_open_custom(&http_chunked_callbacks, io, mode);
}

#define IO_HTTP_STATUS_CODE 0x3ff
#define IO_HTTP_NO_MESSAGE_BODY_ALLOWED 0x400
#define IO_HTTP_CONTENT_ENCODING_GZIP 0x800
#define IO_HTTP_CHUNKED_BODY 0x1000
#define IO_HTTP_DEFINED_BODY 0x2000 /* DEFINED_BODY and CHUNKED_BODY are mutually exclusive. This defines whether Content-Length is user-set or not. */
#define IO_HTTP_BODY_SENT 0x4000
#define IO_HTTP_CLOSING 0x8000 /* Set if Connection: close was specified in server response. */

struct HttpStateStruct {
    IO io; /* Owned by this struct only if ownsIO is non-zero */
    IO body; /* Either chunked encoding IO device for request or response, or a limiter to read the correct number of bytes in a Content-Length response. Owned by this struct. */
    StringList headers; /* Headers received from last HTTP request */
    void *userdata; /* Userdata, to be passed to header callback function to identify this struct */
    int ownsIO;
    unsigned long flags;
    unsigned long long contentLength; /* Contains user-defined content length if specified. Irrelevant if `flags & IO_HTTP_DEFINED_BODY` is 0. */
};

static HttpState http_alloc_state() {
    HttpState state = CALLOC(1, sizeof(*state));
    if (state == NULL)
        return NULL;

    state->headers = stringlist_create();
    if (state->headers == NULL) {
        FREE(state);
        return NULL;
    }

    return state;
}

HttpState http_create_state(IO http, void *userdata) {
    HttpState state = http_alloc_state();
    if (state == NULL)
        return NULL;

    state->io = http;
    state->userdata = userdata;

    return state;
}

HttpState http_create_state_from_url(Url url, void *userdata, int *err, void *ssl_ctx) {
    UNUSED(ssl_ctx)

    HttpState state = http_alloc_state();
    if (state == NULL)
        return NULL;

#ifdef CC_INCLUDE_SSL
    if (!strcmp(url_get_scheme(url), "https")) {
        state->io = io_open_ssl_socket(url_get_host(url), url_get_port_number(url), NetAddressAny, "rwb", ssl_ctx, err);
    } else
#endif
    if (!strcmp(url_get_scheme(url), "http"))
        state->io = io_open_tcp_socket(url_get_host(url), url_get_port_number(url), NetAddressAny, "rwb", err);
    else {
        http_destroy_state(state);
        if (err)
            *err = CC_EINVAL;
        return NULL;
    }

    if (state->io == NULL) {
        http_destroy_state(state);
        return NULL;
    }

    state->ownsIO = 1;
    state->userdata = userdata;

    return state;
}

void http_destroy_state(HttpState state) {
    stringlist_destroy(state->headers);
    io_close(state->body);
    if (state->ownsIO)
        io_close(state->io);

    FREE(state);
}

int http_begin_request(HttpState state, const char *method, Url url) {
    if (state->flags & IO_HTTP_CLOSING)
        return CC_EPIPE;

    io_seek(state->io, 0, SEEK_CUR);
    state->flags = 0;
    io_close(state->body);
    state->body = NULL;

    if (io_printf(state->io, "%s %s HTTP/1.1\r\n", method, url_get_path_and_query(url)[0]?
                  url_get_path_and_query(url): "/") < 0 ||
        io_printf(state->io, "Host: %s\r\n", url_get_host_and_port(url)) < 0)
        goto cleanup;

    return 0;

cleanup:
    return io_error(state->io)? io_error(state->io): CC_EINVAL;
}

int http_add_header(HttpState state, const char *header, const char *value) {
    if ((state->flags & IO_HTTP_BODY_SENT) &&
            (!strcmp_no_case(header, "Transfer-Encoding") ||
             !strcmp_no_case(header, "Content-Length") ||
             !strcmp_no_case(header, "Trailer")))
        return CC_EINVAL;

    if (!strcmp_no_case(header, "Transfer-Encoding") && strstr(value, "chunked")) {
        if (state->flags & IO_HTTP_DEFINED_BODY)
            return CC_EINVAL;
        state->flags |= IO_HTTP_CHUNKED_BODY;
    } else if (!strcmp_no_case(header, "Content-Length")) {
        if ((state->flags & IO_HTTP_CHUNKED_BODY))
            return CC_EINVAL;
        state->contentLength = strtoull(value, NULL, 10);
        state->flags |= IO_HTTP_DEFINED_BODY;
    }

    if (io_printf(state->io, "%s: %s\r\n", header, value) < 0)
        goto cleanup;

    return 0;

cleanup:
    return io_error(state->io)? io_error(state->io): CC_EINVAL;
}

int http_add_body_cstr(HttpState state, const char *body, const char *mimeType) {
    IO io = io_open_cstring(body, "rb");
    if (io == NULL)
        return CC_ENOMEM;

    int result = http_add_body(state, io, mimeType);
    io_close(io);

    return result;
}

int http_add_body(HttpState state, IO body, const char *mimeType) {
    int result = mimeType && strlen(mimeType)? http_add_header(state, "Content-Type", mimeType): 0;

    if (result)
        return result;

    long long size = state->flags & IO_HTTP_CHUNKED_BODY? -1: io_size64(body);
    if (size >= 0) {
        if (state->flags & IO_HTTP_DEFINED_BODY) {
            if ((unsigned long long) size != state->contentLength)
                return CC_EINVAL;
        } else if (io_printf(state->io, "Content-Length: %lld\r\n", size) < 0)
            goto cleanup;
    } else if (!(state->flags & IO_HTTP_CHUNKED_BODY)) {
        if (io_puts("Transfer-Encoding: chunked\r\n", state->io))
            goto cleanup;
    }

    if (io_puts("\r\n", state->io))
        goto cleanup;

    if (state->flags & IO_HTTP_CHUNKED_BODY) {
        IO chunked = io_open_http_chunked(state->io, "wb");
        if (chunked == NULL)
            return CC_ENOMEM;

        if (!io_copy(body, chunked) || io_close(chunked)) {
            io_close(chunked);
            goto cleanup;
        }
    } else if (io_copy(body, state->io))
        goto cleanup;

    state->flags |= IO_HTTP_BODY_SENT;
    return 0;

cleanup:
    return io_error(state->io)? io_error(state->io): CC_EWRITE;
}

IO http_request_body(HttpState state, const char *mimeType) {
    if (mimeType && strlen(mimeType) && http_add_header(state, "Content-Type", mimeType))
        return NULL;

    if (state->flags & IO_HTTP_DEFINED_BODY) /* Won't work since we don't know how many bytes will be sent yet. */
        return NULL;
    else if (!(state->flags & IO_HTTP_CHUNKED_BODY)) {
        if (io_puts("Transfer-Encoding: chunked\r\n", state->io))
            goto cleanup;
    }

    if (io_puts("\r\n", state->io))
        goto cleanup;

    state->flags |= IO_HTTP_BODY_SENT;
    return state->body = io_open_http_chunked(state->io, "wb");

cleanup:
    return NULL;
}

static int http_read_headers(HttpState state) {
    IO headerString = io_open_dynamic_buffer("wb");
    if (headerString == NULL)
        return CC_ENOMEM;

    stringlist_clear(state->headers);

    /* Read headers, allow body initialization */
    while (1) {
        int ch = io_getc(state->io);
        if (ch == '\r' || ch == EOF) {
            if (io_getc(state->io) != '\n')
                goto cleanup;

            break; /* Done with headers if empty line */
        }
        io_ungetc(ch, state->io);

        /* Get header line into single string */
        io_rewind(headerString);
        while (1) {
            while (ch != '\r') {
                ch = io_getc(state->io);
                if (ch == EOF || io_putc(ch, headerString) != ch)
                    goto cleanup;
            }
            if (io_getc(state->io) != '\n')
                goto cleanup;

            /* Handle folded headers */
            ch = io_getc(state->io);
            if (ch == ' ' || ch == '\t') {
                while (ch == ' ' || ch == '\t')
                    ch = io_getc(state->io);
                io_ungetc(ch, state->io);
            } else {
                io_ungetc(ch, state->io);
                break;
            }
        }

        /* NUL-terminate header string */
        char *header = io_underlying_buffer(headerString), *value;
        header[io_tell64(headerString)] = 0;

        /* Separate into header and value */
        if ((value = strchr(header, ':')) == NULL)
            goto cleanup;

        /* Let the user know what the header was too (TODO: this is not split into key and value) */
        if (stringlist_append(state->headers, header))
            goto cleanup;

        *value = 0;
        while (isspace(*++value));

        printf("%s: %s\n", header, value);

        /* Special-case some headers */
        if (!strcmp_no_case(header, "Connection") && !strcmp_no_case(value, "close"))
            state->flags |= IO_HTTP_CLOSING;
        if (!strcmp_no_case(header, "Transfer-Encoding") && strstr(value, "chunked"))
            state->flags |= IO_HTTP_CHUNKED_BODY;
        else if (!strcmp_no_case(header, "Content-Length")) {
            state->contentLength = strtoull(value, NULL, 10);
            state->flags |= IO_HTTP_DEFINED_BODY;
        }
    }

    io_close(headerString);

    return 0;

cleanup:
    io_close(headerString);
    return io_error(state->io)? io_error(state->io): CC_EREAD;
}

int http_begin_response(HttpState state) {
    if (!(state->flags & IO_HTTP_DEFINED_BODY)) {
        int result = io_close(state->body);
        state->body = NULL;

        if (result)
            return result;

        if (io_puts("\r\n", state->io))
            return io_error(state->io)? io_error(state->io): CC_EWRITE;
    }

    io_seek(state->io, 0, SEEK_CUR);
    state->flags = 0;

    unsigned int httpMajor, httpMinor, status = 0;
    if (io_scanf(state->io, "HTTP/%u.%u %3u %*[^\r]", &httpMajor, &httpMinor, &status) != 4 ||
            io_getc(state->io) != '\r' ||
            io_getc(state->io) != '\n')
        goto cleanup;

    if (httpMajor > 1 || (httpMajor == 1 && httpMinor > 1))
        return CC_EBADMSG;

    state->flags |= status;
    if (status / 100 == 1 || status == 204 || status == 304)
        state->flags |= IO_HTTP_NO_MESSAGE_BODY_ALLOWED;

    printf("Status: %d\n", status);

    return http_read_headers(state);

cleanup:
    return io_error(state->io)? io_error(state->io): CC_EREAD;
}

IO http_response_body(HttpState state) {
    printf("Getting response body...\n");

    if (state->flags & IO_HTTP_NO_MESSAGE_BODY_ALLOWED)
        return state->body = io_open_empty();
    else if (state->flags & IO_HTTP_CHUNKED_BODY) {
        return state->body = io_open_http_chunked(state->io, "rb");
    } else {
        return state->body = io_open_limiter(state->io, 0, state->contentLength, "rb");
    }
}

int http_end_response(HttpState state) {
    if (state->body) {
        char dummy[256];
        while (io_read(dummy, 1, sizeof(dummy), state->body) == sizeof(dummy))
            ;

        if (io_error(state->body))
            return io_error(state->body);
    }

    int result = io_close(state->body);
    state->body = NULL;

    if (result)
        return result;

    if (state->flags & IO_HTTP_CHUNKED_BODY) {
        int result = http_read_headers(state);
        if (result)
            return result;
    }

    return 0;
}

#endif /* CC_INCLUDE_NETWORK */
