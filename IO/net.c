#include "net.h"
#include <string.h>
#include <limits.h>

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
#include <winsock2.h>
#include <ws2tcpip.h>

#if MSVC_COMPILER
#pragma comment(lib, "Ws2_32.lib")
#endif
#endif

struct SocketInitializationParams {
    const char *host;
    unsigned short port;
    int socketMode;
    enum NetAddressType type;
    const char **err;
};

size_t net_read(void *ptr, size_t size, size_t count, void *userdata, IO io) {
    UNUSED(io)

    char *cptr = ptr;
    size_t max = size*count;

    do {
        int transfer = max > INT_MAX? INT_MAX: max;

        transfer = recv((SOCKET) userdata, cptr, transfer, 0);
        if (transfer == SOCKET_ERROR)
            return SIZE_MAX;
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
        if (transfer == SOCKET_ERROR)
            return (size*count - max) / size;

        cptr += transfer;
        max -= transfer;
    } while (max);

    return count;
}

void *net_open(void *userdata, IO io) {
    UNUSED(io)
    struct SocketInitializationParams *params = userdata;

#if WINDOWS_OS
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 0), &wsaData)) {
        if (params->err)
            *params->err = "Windows Sockets failed to initialize";
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
            *params->err = "Failed to get address information for host";
        goto cleanup;
    }

    SOCKET sock = INVALID_SOCKET;
    curAddr = result;

    for (; curAddr; curAddr = curAddr->ai_next) {
        /* Create socket for address */
        sock = socket(curAddr->ai_family, curAddr->ai_socktype, curAddr->ai_protocol);
        if (sock == INVALID_SOCKET) {
            if (params->err)
                *params->err = "Could not create socket";
            continue;
        }

        /* Then attempt to connect */
        if (connect(sock, curAddr->ai_addr, curAddr->ai_addrlen)) {
            if (params->err)
                *params->err = "Could not connect to host";

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

static const struct InputOutputDeviceCallbacks net_callbacks = {
    .read = net_read,
    .write = net_write,
    .open = net_open,
    .close = net_close,
    .flush = NULL,
    .tell = NULL,
    .tell64 = NULL,
    .seek = NULL,
    .seek64 = NULL
};

IO io_open_tcp_socket(const char *host, unsigned short port, enum NetAddressType type, const char *mode, const char **err) {
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

IO io_open_udp_socket(const char *host, unsigned short port, enum NetAddressType type, const char *mode, const char **err) {
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

/* TODO: doesn't currently allow the body to be a sequential device, it must support seeking. This should be fixed later with a HTTP/1.1 chunked request */
int io_make_http_request(IO io, const char *method, const char *url, IO data) {
    io_printf(io, "%s %s HTTP/1.0\r\n", method, url);
    io_printf(io, "Host: %s\r\n", method, url);
    long long size = io_size64(data);

    if (data && size > 0) {
        io_printf(io, "Content-Length: %llu\r\n", size);

        io_copy(data, io);

        io_puts("\r\n", io);

        return io_error(data) || io_error(io);
    }

    io_puts("\r\n", io);

    return io_error(io);
}
