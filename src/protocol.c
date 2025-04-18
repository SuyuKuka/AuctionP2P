#include "protocol.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**************************************************************************************************
 *                                       msg_buffer methods                                       *
 **************************************************************************************************/

int init_msg_buffer(msg_buffer *msg_buf, size_t size) {
    msg_buf->len = 0;
    msg_buf->buf = malloc(size);
    if (msg_buf->buf == NULL) {
        msg_buf->size = 0;
        perror("[ERROR] Failed to malloc");
        return -1;
    }
    msg_buf->size = size;
    return 0;
}

int realloc_msg_buffer(msg_buffer *msg_buf) {
    size_t new_size = msg_buf->size * 2;
    char *tmp = realloc(msg_buf->buf, new_size);
    if (tmp == NULL) {
        perror("[ERROR] Failed to realloc");
        return -1;
    }
    msg_buf->buf = tmp;
    msg_buf->size = new_size;
    return 0;
}

int concat_to_buf(msg_buffer *msg_buf, void *data, size_t data_size) {
    while (msg_buf->len + data_size >= msg_buf->size) {
        if (realloc_msg_buffer(msg_buf) < 0) return -1;
    }

    memcpy(msg_buf->buf + msg_buf->len, data, data_size);
    msg_buf->len += data_size;

    return 0;
}

/**************************************************************************************************
 *                                         message methods                                        *
 **************************************************************************************************/

int concat_msg_to_buf(msg_buffer *msg_buf, message *msg, unsigned int flags) {
    if (!msg_buf || !msg) return -1;

    if (flags & CODE && concat_to_buf(msg_buf, &msg->code, sizeof(msg->code)) < 0) return -1;

    if (flags & ID) {
        uint16_t net_id = htons(msg->id);
        if (concat_to_buf(msg_buf, &net_id, sizeof(net_id)) < 0) return -1;
    }

    if (flags & LMESS && concat_to_buf(msg_buf, &msg->lmess, sizeof(msg->lmess)) < 0) return -1;

    if (flags & LSIG && concat_to_buf(msg_buf, &msg->lsig, sizeof(msg->lsig)) < 0) return -1;

    if (flags & MESS && msg->lmess > 0 && msg->mess != NULL &&
        concat_to_buf(msg_buf, msg->mess, msg->lmess) < 0)
        return -1;

    if (flags & SIG && msg->lsig > 0 && msg->sig != NULL &&
        concat_to_buf(msg_buf, msg->sig, msg->lsig) < 0)
        return -1;

    if (flags & IP && concat_to_buf(msg_buf, &msg->ip.__in6_u.__u6_addr8, sizeof(uint8_t) * 16) < 0)
        return -1;

    if (flags & PORT) {
        uint16_t net_port = htons(msg->port);
        if (concat_to_buf(msg_buf, &net_port, sizeof(net_port)) < 0) return -1;
    }

    if (flags & CLE && concat_to_buf(msg_buf, msg->cle, sizeof(msg->cle)) < 0) return -1;

    if (flags & NUMV) {
        uint32_t net_numv = htonl(msg->numv);
        if (concat_to_buf(msg_buf, &net_numv, sizeof(net_numv)) < 0) return -1;
    }

    if (flags & PRIX) {
        uint32_t net_prix = htonl(msg->prix);
        if (concat_to_buf(msg_buf, &net_prix, sizeof(net_prix)) < 0) return -1;
    }

    if (flags & NB) {
        uint16_t net_nb = htons(msg->nb);
        if (concat_to_buf(msg_buf, &net_nb, sizeof(net_nb)) < 0) return -1;
    }

    return 0;
}

/**************************************************************************************************
 *                                   init multicast addr/socket *
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

int init_multicast_socket(struct sockaddr_in6 multicast_addr) {
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
    addr.sin6_port = multicast_addr.sin6_port;

    if (bind(receiver_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[ERROR] init_multicast_socket bind");
        goto error;
    }

    struct ipv6_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    mreq.ipv6mr_multiaddr = multicast_addr.sin6_addr;
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