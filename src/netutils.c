#define _POSIX_C_SOURCE 200112L

#include "netutils.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

int make_udp_receiver_socket(uint16_t port) {
    int sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("[ERROR] socket");
        goto error;
    }

    struct sockaddr_in6 s_addr;
    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin6_family = AF_INET6;
    s_addr.sin6_addr = in6addr_any;
    s_addr.sin6_port = port;

    if (bind(sock, (struct sockaddr *)&s_addr, sizeof(s_addr)) < 0) {
        perror("[ERROR] bind");
        goto error;
    }

    return sock;

error:
    if (sock) close(sock);
    return -1;
}

int make_multicast_socket(struct sockaddr_in6 multicast_addr) {
    int receiver_sock = make_udp_receiver_socket(multicast_addr.sin6_port);
    if (receiver_sock < 0) {
        return -1;
    }

    struct ipv6_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    mreq.ipv6mr_multiaddr = multicast_addr.sin6_addr;
    mreq.ipv6mr_interface = 0;

    if (setsockopt(receiver_sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) < 0) {
        perror("[ERROR] setsockopt IPV6_JOIN_GROUP");
        close(receiver_sock);
        return -1;
    }

    return receiver_sock;
}

int init_personal_addr(struct sockaddr_in6 *personal_addr, int port) {
    char hostname[1024];
    struct addrinfo hints, *res = NULL;

    // Nom de la machine
    memset(hostname, 0, sizeof(hostname));
    if (gethostname(hostname, sizeof(hostname)) < 0) {
        perror("[ERROR] gethostname");
        goto error;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;

    int status;
    if ((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
        fprintf(stderr, "[ERROR] getaddrinfo: %s\n", gai_strerror(status));
        goto error;
    }
    if (res == NULL) goto error;

    memcpy(personal_addr, res->ai_addr, sizeof(*personal_addr));
    personal_addr->sin6_port = port;

    freeaddrinfo(res);
    return 0;

error:
    if (res) freeaddrinfo(res);
    return -1;
}

int make_tcp_server_socket(int port) {
    int sock = socket(PF_INET6, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[ERROR] socket");
        goto error;
    }

    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = port;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[ERROR] bind");
        goto error;
    }

    if (listen(sock, 0) < 0) {
        perror("[ERROR] listen");
        goto error;
    }

    return sock;

error:
    if (sock) close(sock);
    return -1;
}

int make_udp_socket() {
    int sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if ((sock) < 0) {
        perror("[ERROR] socket");
        goto error;
    }

    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = 0;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[ERROR] bind");
        goto error;
    }

    return sock;

error:
    if (sock) close(sock);
    return -1;
}

int get_socket_port(int sock) {
    struct sockaddr_in6 addr;
    socklen_t len = sizeof(addr);

    if (getsockname(sock, (struct sockaddr *)&addr, &len) < 0) {
        perror("[ERROR] getsockname");
        return -1;
    }

    return addr.sin6_port;
}
