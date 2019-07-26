#include "net.h"
#include "../utility.h"

#include "../seaerror.h"

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

    /** Contains the first character of the path. This is so the path can be reconstructed if the first character must be used as a NUL-terminator for `host` or `port` */
    char path_first_char;

    /** Contains the query of the URL, percent-encoded. Will be `NULL` if no query is specified */
    char *query;

    /** Contains the fragment of the URL, percent-encoded. Will be `NULL` if no query is specified */
    char *fragment;

    /** Contains the entire URL, percent-encoded. However, this buffer is pointed to by all the other URL sections (with the exception of `url`), and so will have NUL characters embedded in it. */
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
    const struct {
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
#if WINDOWS_OS
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

#include <signal.h>

void io_net_init() {
#if !WINDOWS_OS
    signal(SIGPIPE, SIG_IGN);
#endif
}

void io_net_destroy() {

}

static size_t net_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    char *cptr = ptr;
    size_t max = size*count;

    do {
        int transfer = max > INT_MAX? INT_MAX: (int) max;

        transfer = recv((SOCKET) (uintptr_t) userdata, cptr, transfer, 0);
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

        cptr += transfer;
        max -= transfer;
    } while (max);

    return count;
}

static size_t net_write(const void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    const char *cptr = ptr;
    size_t max = size*count;

    do {
        int transfer = max > INT_MAX? INT_MAX: (int) max;

        /* TODO: no protection against sending UDP packets that are too large */
        transfer = send((SOCKET) (uintptr_t) userdata, cptr, transfer, 0);
        if (transfer == SOCKET_ERROR) {
#if WINDOWS_OS
            io_set_error(io, WSAGetLastError());
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

static void *net_open(void *userdata, IO io) {
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

    if (sock != INVALID_SOCKET)
        return (void *) (uintptr_t) sock;

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

static int net_close(void *userdata, IO io) {
    UNUSED(io)

#if WINDOWS_OS
    int result = closesocket((SOCKET) userdata);

    WSACleanup();
#elif LINUX_OS
    int result = close((SOCKET) (uintptr_t) userdata);
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
    .clearerr = NULL,
    .stateSwitch = NULL,
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

struct HttpChunkedStruct {
    IO io;

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
    .stateSwitch = NULL,
    .tell = NULL,
    .tell64 = NULL,
    .seek = NULL,
    .seek64 = NULL,
    .what = http_chunked_what
};

static IO io_open_http_chunked(IO io, const char *mode) {
    return io_open_custom(&http_chunked_callbacks, io, mode);
}

#include "limiter.h"

#define IO_HTTP_STATUS_CODE 0x3ff
#define IO_HTTP_NO_MESSAGE_BODY_ALLOWED 0x400
#define IO_HTTP_CONTENT_ENCODING_GZIP 0x800
#define IO_HTTP_CHUNKED_BODY 0x1000
#define IO_HTTP_DEFINED_BODY 0x2000 /* DEFINED_BODY and CHUNKED_BODY are mutually exclusive. This defines whether Content-Length is user-set or not. */
#define IO_HTTP_BODY_SENT 0x4000
#define IO_HTTP_CLOSING 0x8000 /* Set if Connection: close was specified in server response. */

struct HttpStateStruct {
    IO io; /* Owned by this struct only if ownsIO is non-zero */
    int ownsIO;
    IO body; /* Either chunked encoding IO device for request or response, or a limiter to read the correct number of bytes in a Content-Length response. Owned by this struct. */
    HttpHeaderCallback responseHeaderCb;
    unsigned long flags;
    unsigned long long contentLength; /* Contains user-defined content length if specified. Irrelevant if `flags & IO_HTTP_DEFINED_BODY` is 0. */
};

HttpState http_create_state(IO http, HttpHeaderCallback responseHeaderCb) {
    HttpState state = CALLOC(1, sizeof(*state));
    if (state == NULL)
        return NULL;

    state->io = http;
    state->responseHeaderCb = responseHeaderCb;

    return state;
}

HttpState http_create_state_from_url(Url url, HttpHeaderCallback responseHeaderCb, int *err) {
    HttpState state = CALLOC(1, sizeof(*state));
    if (state == NULL) {
        if (err)
            *err = CC_ENOMEM;
        return NULL;
    }

    state->io = io_open_tcp_socket(url_get_host(url), url_get_port_number(url), NetAddressAny, "rwb", err);
    if (state->io == NULL) {
        FREE(state);
        return NULL;
    }
    state->ownsIO = 1;
    state->responseHeaderCb = responseHeaderCb;

    return state;
}

void http_destroy_state(HttpState state) {
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

    if (io_printf(state->io, "%s %s HTTP/1.1\r\n", method, url_get_path_and_query_and_fragment(url)[0]?
                  url_get_path_and_query_and_fragment(url): "/") < 0 ||
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
        if ((state->flags & IO_HTTP_CHUNKED_BODY) || sscanf(value, "%llu", &state->contentLength) != 1)
            return CC_EINVAL;
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

        if (!io_copy(body, chunked) || io_close(chunked))
            goto cleanup;
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

        *value = 0;
        while (isspace(*++value));

        printf("%s: %s\n", header, value);

        /* Special-case some headers */
        if (!strcmp_no_case(header, "Connection") && !strcmp_no_case(value, "close"))
            state->flags |= IO_HTTP_CLOSING;
        if (!strcmp_no_case(header, "Transfer-Encoding") && strstr(value, "chunked"))
            state->flags |= IO_HTTP_CHUNKED_BODY;
        else if (!strcmp_no_case(header, "Content-Length")) {
            if (sscanf(value, "%llu", &state->contentLength) != 1)
                goto cleanup;
            state->flags |= IO_HTTP_DEFINED_BODY;
        }

        /* Let the user know what the header was too */
        if (state->responseHeaderCb)
            state->responseHeaderCb(header, value);
    }

    return 0;

cleanup:
    FREE(io_underlying_buffer(headerString));
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
