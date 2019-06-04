#include "net.h"
#include <string.h>

#if LINUX_OS
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

static IO io_open_socket(const char *host, unsigned short port, int type, const char *mode, const char **err) {
    struct sockaddr_in serverAddress;
    struct hostent *server;
    int sockfd;

    sockfd = socket(AF_INET, type, 0);
    if (sockfd < 0) {
        if (err)
            *err = "Opening socket failed";
        return NULL;
    }

    /* TODO: not reentrant */
    server = gethostbyname(host);
    if (server == NULL) {
        if (err)
            *err = "Could not resolve host name";
        return NULL;
    }

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    memcpy(&serverAddress.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    serverAddress.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
        if (err)
            *err = "Could not connect to host";
        return NULL;
    }

    IO io = io_open_native_file(sockfd, mode);
    if (io)
        io_grab_file(io);
    return io;
}

IO io_open_tcp_socket(const char *host, unsigned short port, const char *mode, const char **err) {
    return io_open_socket(host, port, SOCK_STREAM, mode, err);
}

IO io_open_udp_socket(const char *host, unsigned short port, const char *mode, const char **err) {
    return io_open_socket(host, port, SOCK_DGRAM, mode, err);
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
#else

#endif
