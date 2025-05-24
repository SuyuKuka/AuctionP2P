#include "protocol.h"

#include <string.h>

int concat_msg_to_buf(dyn_array *msg_buf, message *msg, unsigned int flags) {
    if (!msg_buf || !msg) return -1;

    if (flags & CODE && concat_to_array(msg_buf, &msg->code, sizeof(msg->code)) < 0) return -1;

    if (flags & ID && concat_to_array(msg_buf, &msg->id, sizeof(msg->id)) < 0) return -1;

    if (flags & LMESS && concat_to_array(msg_buf, &msg->lmess, sizeof(msg->lmess)) < 0) return -1;

    if (flags & LSIG && concat_to_array(msg_buf, &msg->lsig, sizeof(msg->lsig)) < 0) return -1;

    if (flags & MESS && msg->lmess > 0 && concat_to_array(msg_buf, msg->mess, msg->lmess) < 0)
        return -1;

    if (flags & SIG && msg->lsig > 0 && concat_to_array(msg_buf, msg->sig, msg->lsig) < 0)
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

size_t msg_size(int flags) {
    size_t size = 0;
    message msg;

    if (flags & CODE) size += sizeof(msg.code);
    if (flags & ID) size += sizeof(msg.id);
    if (flags & LMESS) size += sizeof(msg.lmess);
    if (flags & LSIG) size += sizeof(msg.lsig);
    if (flags & MESS) size += sizeof(msg.mess);
    if (flags & SIG) size += sizeof(msg.sig);
    if (flags & IP) size += sizeof(msg.ip);
    if (flags & PORT) size += sizeof(msg.port);
    if (flags & CLE) size += sizeof(msg.cle);
    if (flags & NUMV) size += sizeof(msg.numv);
    if (flags & PRIX) size += sizeof(msg.prix);
    if (flags & NB) size += sizeof(msg.nb);

    return size;
}
