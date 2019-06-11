#include "net.h"
#include "../utility.h"
#include <string.h>
#include <limits.h>

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

    /** Contains the first character of the path. This is so the path can be reconstructed if the first character must be used as a NUL-terminator for `host` or `port` */
    char path_first_char;

    /** Contains the query of the URL, percent-encoded. Will be `NULL` if no query is specified */
    char *query;

    /** Contains the fragment of the URL, percent-encoded. Will be `NULL` if no query is specified */
    char *fragment;

    /** Contains the entire URL, percent-encoded. However, this buffer is pointed to by all the other URL sections (with the exception of `url`), and so will have NUL characters embedded in it. */
    char *url_buffer;

    /** Contains the entire URL, percent-encoded. */
    char *url;
};

void url_destroy(Url url) {
    free(url->url_buffer);
    free(url->url);
    free(url);
}

char *url_percent_encoded_from_utf8(const char *url) {

}

Url url_from_utf8(const char *url) {
    char *percent_encoded = url_percent_encoded_from_utf8(url);
    if (percent_encoded == NULL)
        return NULL;

    Url result = url_from_percent_encoded(percent_encoded);
    free(percent_encoded);

    return result;
}

Url url_from_percent_encoded(const char *url) {
    Url result = calloc(1, sizeof(*result));
    if (result == NULL)
        return NULL;

    if ((result->url_buffer = strdup(url)) == NULL ||
        (result->url = strdup(url)) == NULL)
        goto cleanup;

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
                result->password = string = colon + 1;
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
    struct {
        const char *scheme;
        unsigned short port;
    } mapping[] = {
        {.scheme = "http", .port = 80}
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
#if LINUX_OS
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define SOCKET int

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#elif WINDOWS_OS
#if MSVC_COMPILER
#pragma comment(lib, "Ws2_32.lib")
#endif
#endif

/* TODO: allow non-blocking connect to allow custom timeout for connecting */
struct SocketInitializationParams {
    const char *host;
    unsigned short port;
    int socketMode;
    enum NetAddressType type;
    int *err;
};

size_t net_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    char *cptr = ptr;
    size_t max = size*count;

    do {
        int transfer = max > INT_MAX? INT_MAX: max;

        transfer = recv((SOCKET) userdata, cptr, transfer, 0);
        if (transfer == SOCKET_ERROR) {
#if WINDOWS_OS
            io_set_error(io, WSAGetLastError()); /* TODO: map WSA errors to errno values? */
#else
            io_set_error(io, errno);
#endif
            return SIZE_MAX;
        }
        else if (transfer == 0)
            return (size*count - max) / size;

        cptr += transfer;
        max -= transfer;
    } while (max);

    return count;
}

size_t net_write(const void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    const char *cptr = ptr;
    size_t max = size*count;

    do {
        int transfer = max > INT_MAX? INT_MAX: max;

        /* TODO: no protection against sending UDP packets that are too large */
        transfer = send((SOCKET) userdata, cptr, transfer, 0);
        if (transfer == SOCKET_ERROR) {
#if WINDOWS_OS
            io_set_error(io, WSAGetLastError()); /* TODO: map WSA errors to errno values? */
#else
            io_set_error(io, errno);
#endif
            return (size*count - max) / size;
        }

        cptr += transfer;
        max -= transfer;
    } while (max);

    return count;
}

void *net_open(void *userdata, IO io) {
    UNUSED(io)
    struct SocketInitializationParams *params = userdata;

    if (params->err)
        *params->err = 0;

#if WINDOWS_OS
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 0), &wsaData)) {
        if (params->err)
            *params->err = WSAGetLastError();
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

    *io_tempdata(io) = params->socketMode != SOCK_STREAM;

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

    SOCKET sock = INVALID_SOCKET;
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
        if (connect(sock, curAddr->ai_addr, curAddr->ai_addrlen)) {
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

    if (sock != INVALID_SOCKET)
        return (void *) sock;

cleanup:
#if WINDOWS_OS
    if (sock != INVALID_SOCKET)
        closesocket(sock);
    WSACleanup();
#elif LINUX_OS
    if (sock != INVALID_SOCKET)
        close(sock);
#endif

    return NULL;
}

int net_close(void *userdata, IO io) {
    UNUSED(io)

#if WINDOWS_OS
    int result = closesocket((SOCKET) userdata);

    WSACleanup();
#elif LINUX_OS
    int result = close((SOCKET) userdata);
#endif

    return result;
}

/* First byte of tempdata is 0 if TCP, 1 if UDP */
static const char *net_what(void *userdata, IO io) {
    UNUSED(userdata)

    switch (*io_tempdata(io)) {
        default: return "unknown_socket";
        case 0: return "tcp_socket";
        case 1: return "udp_socket";
    }
}

static const struct InputOutputDeviceCallbacks net_callbacks = {
    .read = net_read,
    .write = net_write,
    .open = net_open,
    .close = net_close,
    .flush = NULL,
    .tell = NULL,
    .tell64 = NULL,
    .seek = NULL,
    .seek64 = NULL,
    .what = net_what
};

IO io_open_tcp_socket(const char *host, unsigned short port, enum NetAddressType type, const char *mode, int *err) {
    struct SocketInitializationParams params =
    {
        .host = host,
        .port = port,
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
        .socketMode = SOCK_DGRAM,
        .type = type,
        .err = err
    };

    return io_open_custom(&net_callbacks, &params, mode);
}

#include "tee.h"

/* TODO: doesn't currently allow the body to be a sequential device, it must support seeking. This should be fixed later with a HTTP/1.1 chunked request */
int io_make_http_request(IO io, const char *method, Url url, IO data) {
    io_printf(io, "%s %s HTTP/1.0\r\n", method,
              url_get_path_and_query_and_fragment(url)[0] == 0? url_get_path_and_query_and_fragment(url): "/");
    io_printf(io, "Host: %s\r\n", url_get_host_and_port(url));

    if (data) {
        long long size = io_size64(data);

        if (size > 0) {
            io_printf(io, "Content-Length: %llu\r\n", size);

            io_copy(data, io);

            io_puts("\r\n", io);

            return io_error(data) || io_error(io);
        }
    }

    io_puts("\r\n", io);
    io_seek(io, 0, SEEK_CUR); /* Allow reading directly after calling this function */

    return io_error(io);
}

IO io_http_get(Url url, int *err) {
    IO http = io_open_tcp_socket(url_get_host(url), url_get_port_number(url), NetAddressAny, "rwb", err);
    if (http == NULL)
        return NULL;

    if (io_make_http_request(http, "GET", url, NULL)) {
        io_close(http);
        return NULL;
    }

    return http;
}

#endif /* CC_INCLUDE_NETWORK */
