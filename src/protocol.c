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

void free_msg_buffer(msg_buffer msg_buffer) { free(msg_buffer.buf); }

int concat_to_buf(msg_buffer *msg_buf, const void *data, size_t data_size) {
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

    if (flags & ID && concat_to_buf(msg_buf, &msg->id, sizeof(msg->id)) < 0) return -1;

    if (flags & LMESS && concat_to_buf(msg_buf, &msg->lmess, sizeof(msg->lmess)) < 0) return -1;

    if (flags & LSIG && concat_to_buf(msg_buf, &msg->lsig, sizeof(msg->lsig)) < 0) return -1;

    if (flags & MESS && msg->lmess > 0 && msg->mess != NULL &&
        concat_to_buf(msg_buf, msg->mess, msg->lmess) < 0)
        return -1;

    if (flags & SIG && msg->lsig > 0 && msg->sig != NULL &&
        concat_to_buf(msg_buf, msg->sig, msg->lsig) < 0)
        return -1;

    if (flags & IP && concat_to_buf(msg_buf, &msg->ip, sizeof(msg->ip)) < 0) return -1;

    if (flags & PORT && concat_to_buf(msg_buf, &msg->port, sizeof(msg->port)) < 0) return -1;

    if (flags & CLE && concat_to_buf(msg_buf, msg->cle, sizeof(msg->cle)) < 0) return -1;

    if (flags & NUMV && concat_to_buf(msg_buf, &msg->numv, sizeof(msg->numv)) < 0) return -1;

    if (flags & PRIX && concat_to_buf(msg_buf, &msg->prix, sizeof(msg->prix)) < 0) return -1;

    if (flags & NB && concat_to_buf(msg_buf, &msg->nb, sizeof(msg->nb)) < 0) return -1;

    return 0;
}

void buf_to_msg(const char *buf, message *msg, unsigned int flags) {
    int pos = 0;

    if (flags & CODE) msg->code = buf[pos++];

    if (flags & ID) {
        msg->id = *((uint16_t *)(buf + pos));
        pos += sizeof(msg->id);
    }

    if (flags & LMESS) msg->lmess = buf[pos++];

    if (flags & LSIG) msg->lsig = buf[pos++];

    if (flags & MESS) {
        memcpy(msg->mess, buf + pos, msg->lmess);
        pos += msg->lmess;
    }

    if (flags & SIG) {
        memcpy(msg->sig, buf + pos, msg->lsig);
        pos += msg->lsig;
    }

    if (flags & IP) {
        memcpy(msg->ip, buf + pos, sizeof(msg->ip));
        pos += sizeof(msg->ip);
    }

    if (flags & PORT) {
        msg->port = *((uint16_t *)(buf + pos));
        pos += sizeof(msg->port);
    }

    if (flags & CLE) {
        memcpy(msg->cle, buf + pos, sizeof(msg->cle));
        pos += sizeof(msg->cle);
    }

    if (flags & NUMV) {
        msg->numv = *((uint32_t *)(buf + pos));
        pos += sizeof(msg->numv);
    }

    if (flags & PRIX) {
        msg->prix = *((uint32_t *)(buf + pos));
        pos += sizeof(msg->prix);
    }

    if (flags & NB) {
        msg->nb = *((uint16_t *)(buf + pos));
        pos += sizeof(msg->nb);
    }
}
