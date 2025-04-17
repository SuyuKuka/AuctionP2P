#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**************************************************************************************************
 *                                       msg_buffer methods                                       *
 **************************************************************************************************/

int init_msg_buffer(msg_buffer* msg_buf, size_t size) {
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

int realloc_msg_buffer(msg_buffer* msg_buf) {
    size_t new_size = msg_buf->size * 2;
    char* tmp = realloc(msg_buf->buf, new_size);
    if (tmp == NULL) {
        perror("[ERROR] Failed to realloc");
        return -1;
    }
    msg_buf->buf = tmp;
    msg_buf->size = new_size;
    return 0;
}

int concat_to_buf(msg_buffer* msg_buf, void* data, size_t data_size) {
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

int concat_msg_to_buf(msg_buffer* msg_buf, message* msg, unsigned int flags) {
    if (!msg_buf || !msg) return -1;

    printf("code: %d\n", msg->code);

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
