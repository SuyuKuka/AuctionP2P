#ifndef HANDLERS_H
#define HANDLERS_H

#include "protocol.h"

/* Args pour la fonction `handle_multicast`. */
typedef struct {
    struct sockaddr_in6 linking_addr;
    struct sockaddr_in6 auction_addr;
    pers_data p_data;
    int *running;
} multicast_handler_args;

/* Args pour la fonction `handle_tcp`. */
typedef struct {
    struct sockaddr_in6 auction_addr;
    pers_data p_data;
    linked_list *peers;
    int *running;
} tcp_handler_args;

/**
 * @brief Gère la réception des messages multicast.
 *
 * @param[in] arg Structure `multicast_handler_args`.
 */
void *handle_multicast(void *arg);
/**
 * @brief Gère la réception des messages unicast TCP.
 *
 * @param[in,out] arg Structure `tcp_handler_args`.
 */
void *handle_tcp(void *arg);

#endif