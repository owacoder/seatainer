#ifndef NET_H
#define NET_H

#include "../io.h"

#ifdef __cplusplus
extern "C" {
#endif

enum NetAddressType {
    NetAddressAny,
    NetAddressIPv4,
    NetAddressIPv6
};

IO io_open_tcp_socket(const char *host, unsigned short port, enum NetAddressType type, const char *mode, const char **err);
IO io_open_udp_socket(const char *host, unsigned short port, enum NetAddressType type, const char *mode, const char **err);

#ifdef __cplusplus
}
#endif

#endif // NET_H
