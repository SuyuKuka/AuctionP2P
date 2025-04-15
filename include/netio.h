#ifndef NETIO_H
#define NETIO_H

#include <netinet/in.h>
#include <stdint.h>

typedef struct {
    uint8_t code;
    uint16_t id;
    uint8_t lmess;
    uint8_t lsig;
    char* mess;
    char* sig;
    struct in6_addr ip;
    uint16_t port;
    char cle[113];
    uint32_t numv;
    uint32_t prix;
    uint16_t nb;
} message;

#endif