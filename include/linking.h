#ifndef LINKING_H
#define LINKING_H

#include <netinet/in.h>

#include "netutils.h"
#include "protocol.h"

typedef struct join_req_params {
    struct sockaddr_in6 linking_addr;  // Adresse de liaison.
    pers_data p_data;
} join_req_params;

typedef struct data_sharing_params {
    dyn_array *peers_data;
    pers_data p_data;
    uint8_t auction_ip[16];
    uint16_t auction_port;
} data_sharing_params;

/**
 * @brief Gère le processus pour rejoindre le système d'enchères.
 *
 * @param[in,out]   p_data Données personnelles.
 * @param[in,out]   peers_data Adresses et clés des pairs.
 * @param[in]       linking_addr Adresse de liaison multicast.
 * @param[in,out]   auction_addr Adresse d'enchères multicast.
 *
 * @return `-1` en cas d'erreur, `0` s'il n'y a pas eu de réponse, `1` sinon.
 */
int handle_linking(pers_data *p_data, dyn_array *peers_data, struct sockaddr_in6 linking_addr,
                   struct sockaddr_in6 *auction_addr);
/**
 * @brief Gère les demandes pour rejoindre le système d'enchères.
 *
 * @param[in]   params Structure `join_req_params`.
 */
void *handle_join_requests(void *params);
/**
 * @brief Gère les demandes de partage d'infos personnelles.
 *
 * @param[in]   params Structure `data_sharing_params`.
 */
void *handle_data_sharing(void *params);

#endif