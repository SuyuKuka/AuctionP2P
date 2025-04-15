#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "netio.h"

#define LINKING_IP "ff12::feed:bee"
#define AUCTIONS_IP "ff12::add:beef"
#define LINKING_PORT 57500
#define AUCTIONS_PORT 59500

struct sockaddr_in6 linking_addr;
struct sockaddr_in6 auction_addr;

int init_multicast_addr(struct sockaddr_in6 *addr, const char *ip, int port) {
    memset(addr, 0, sizeof(struct sockaddr_in6));
    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons(port);
    addr->sin6_scope_id = 0;
    int res = inet_pton(AF_INET6, ip, &(addr->sin6_addr));
    if (res == 0) {
        fprintf(stderr, "[ERROR] Invalid ip address format: %s.\n", ip);
        return -1;
    } else if (res < 0) {
        perror("[ERROR] 'inet_pton' during multicast address initialization");
        return -1;
    }
    return 0;
}

int init_multicast_socket(const char *ip, int port) {
    int receiver_sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (receiver_sock < 0) {
        perror("[ERROR] init_multicast_socket socket");
        goto error;
    }

    int reuse = 1;
    if (setsockopt(receiver_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("[ERROR] init_multicast_socket reuseaddr");
        goto error;
    }

    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(port);

    if (bind(receiver_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[ERROR] init_multicast_socket bind");
        goto error;
    }

    struct ipv6_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    inet_pton(AF_INET6, ip, &mreq.ipv6mr_multiaddr.s6_addr);
    mreq.ipv6mr_interface = 0;

    if (setsockopt(receiver_sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) < 0) {
        perror("[ERROR] init_multicast_socket subscription");
        goto error;
    }

    return receiver_sock;

error:
    if (receiver_sock) close(receiver_sock);
    return -1;
}

int main(int argc, char const *argv[]) {
    init_multicast_addr(&linking_addr, LINKING_IP, LINKING_PORT);
    init_multicast_addr(&auction_addr, AUCTIONS_IP, AUCTIONS_PORT);

    int linking_sock = init_multicast_socket(LINKING_IP, LINKING_PORT);

    // Test
    char buf[128];
    memset(buf, 0, sizeof(buf));
    read(linking_sock, buf, sizeof(buf));
    printf("%s\n", buf);

    return EXIT_SUCCESS;
}
