#include "protocol.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**************************************************************************************************
 *                                        dyn_array methods                                       *
 **************************************************************************************************/

int init_dyn_array(dyn_array *arr, size_t size) {
    arr->len = 0;
    arr->array = malloc(size);
    if (arr->array == NULL) {
        arr->size = 0;
        perror("[ERROR] Failed to malloc");
        return -1;
    }
    arr->size = size;
    return 0;
}

int realloc_dyn_array(dyn_array *arr) {
    size_t new_size = arr->size * 2;
    char *tmp = realloc(arr->array, new_size);
    if (tmp == NULL) {
        perror("[ERROR] Failed to realloc");
        return -1;
    }
    arr->array = tmp;
    arr->size = new_size;
    return 0;
}

void free_dyn_array(dyn_array arr) { free(arr.array); }

int concat_to_array(dyn_array *arr, const void *data, size_t data_size) {
    while (arr->len + data_size >= arr->size) {
        if (realloc_dyn_array(arr) < 0) return -1;
    }

    memcpy(arr->array + arr->len, data, data_size);
    arr->len += data_size;

    return 0;
}

/**************************************************************************************************
 *                                         message methods                                        *
 **************************************************************************************************/

int concat_msg_to_buf(dyn_array *msg_buf, message *msg, unsigned int flags) {
    if (!msg_buf || !msg) return -1;

    if (flags & CODE && concat_to_array(msg_buf, &msg->code, sizeof(msg->code)) < 0) return -1;

    if (flags & ID && concat_to_array(msg_buf, &msg->id, sizeof(msg->id)) < 0) return -1;

    if (flags & LMESS && concat_to_array(msg_buf, &msg->lmess, sizeof(msg->lmess)) < 0) return -1;

    if (flags & LSIG && concat_to_array(msg_buf, &msg->lsig, sizeof(msg->lsig)) < 0) return -1;

    if (flags & MESS && msg->lmess > 0 && msg->mess != NULL &&
        concat_to_array(msg_buf, msg->mess, msg->lmess) < 0)
        return -1;

    if (flags & SIG && msg->lsig > 0 && msg->sig != NULL &&
        concat_to_array(msg_buf, msg->sig, msg->lsig) < 0)
        return -1;

    if (flags & IP && concat_to_array(msg_buf, &msg->ip, sizeof(msg->ip)) < 0) return -1;

    if (flags & PORT && concat_to_array(msg_buf, &msg->port, sizeof(msg->port)) < 0) return -1;

    if (flags & CLE && concat_to_array(msg_buf, msg->cle, sizeof(msg->cle)) < 0) return -1;

    if (flags & NUMV && concat_to_array(msg_buf, &msg->numv, sizeof(msg->numv)) < 0) return -1;

    if (flags & PRIX && concat_to_array(msg_buf, &msg->prix, sizeof(msg->prix)) < 0) return -1;

    if (flags & NB && concat_to_array(msg_buf, &msg->nb, sizeof(msg->nb)) < 0) return -1;

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
