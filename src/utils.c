#define _POSIX_C_SOURCE 200112L

#include "utils.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int read_nb(int sock, char *buf, size_t n, int timeout) {
    struct pollfd fd = {.fd = sock, .events = POLLIN};

    int poll_ret;
    ssize_t n_read;
    size_t pos = 0;
    while (pos < n && (poll_ret = poll(&fd, 1, timeout))) {
        if (fd.revents & POLLIN) {
            n_read = recv(sock, buf + pos, n - pos, 0);
            if (n_read < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                perror("[ERROR] recv");
                return -1;
            }
            if (n_read == 0) {
                perror("[WARN] Connection closed by peer");
                return -1;
            }

            pos += n_read;
        }
    }

    if (poll_ret < 0) {
        perror("[ERROR] poll");
        return -1;
    } else if (poll_ret == 0) {
        perror("[WARN] Timeout on read");
        return -1;
    }

    if (pos > n) {
        fprintf(stderr, "[ERROR] Invalid message format received.\n");
        return -1;
    }

    return 0;
}

int write_nb(int sock, char *buf, size_t n, int timeout) {
    struct pollfd fd = {.fd = sock, .events = POLLOUT};

    int poll_ret;
    size_t pos = 0;
    while (pos < n && (poll_ret = poll(&fd, 1, timeout))) {
        if (fd.revents & POLLOUT) {
            ssize_t n_sent = send(sock, buf + pos, n - pos, 0);
            if (n_sent < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                perror("[ERROR] send");
                return -1;
            }
            pos += n_sent;
        }
    }

    if (poll_ret < 0) {
        perror("[ERROR] poll");
        return -1;
    } else if (poll_ret == 0) {
        perror("[WARN] Timeout on write");
        return -1;
    }

    return 0;
}

int set_nonblock(int sock) {
    int flags;

    if ((flags = fcntl(sock, F_GETFL, 0)) < 0) {
        perror("[ERROR] fcntl F_GETFL");
        return -1;
    }

    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("[ERROR] fcntl F_SETFL");
        return -1;
    }

    return 0;
}

/**************************************************************************************************
 *                                        init addr/socket                                        *
 **************************************************************************************************/

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

int make_multicast_socket(struct sockaddr_in6 multicast_addr) {
    int receiver_sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (receiver_sock < 0) {
        perror("[ERROR] socket");
        goto error;
    }

    int reuse = 1;
    if (setsockopt(receiver_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("[ERROR] setsockopt SO_REUSEADDR");
        goto error;
    }

    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = multicast_addr.sin6_port;

    if (bind(receiver_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[ERROR] bind");
        goto error;
    }

    struct ipv6_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    mreq.ipv6mr_multiaddr = multicast_addr.sin6_addr;
    mreq.ipv6mr_interface = 0;

    if (setsockopt(receiver_sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) < 0) {
        perror("[ERROR] setsockopt IPV6_JOIN_GROUP");
        goto error;
    }

    return receiver_sock;

error:
    if (receiver_sock) close(receiver_sock);
    return -1;
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

    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("[ERROR] setsockopt SO_REUSEADDR");
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

    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("[ERROR] setsockopt SO_REUSEADDR");
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
