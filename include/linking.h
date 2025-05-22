#ifndef LINKING_H
#define LINKING_H

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
 * @param[in,out]   peers Adresses et clés des pairs.
 * @param[in]       linking_addr Adresse de liaison multicast.
 * @param[in,out]   auction_addr Adresse d'enchères multicast.
 *
 * @return `-1` en cas d'erreur, `0` s'il n'y a pas eu de réponse, `1` sinon.
 */
int handle_linking(pers_data *p_data, linked_list *peers, struct sockaddr_in6 linking_addr,
                   struct sockaddr_in6 *auction_addr);
/**
 * @brief Gère une demande pour rejoindre le système d'enchères.
 *
 * @param[in]   addr Adresse du demandeur.
 * @param[in]   p_data Informations personnelles.
 */
int handle_join_request(struct sockaddr_in6 addr, pers_data p_data);
/**
 * @brief Gère la demande de partage d'infos personnelles des pairs du système au pair `peer`.
 *
 * @param[in]       peer_sock Socket du pair.
 * @param[in,out]   peer Informations du pair.
 * @param[in]       p_data Données personnelles.
 * @param[in,out]   peers Liste des pairs.
 * @param[in]       auction_addr Adresse d'enchères.
 */
int handle_info_sharing(int peer_sock, peer_data *peer, pers_data p_data, linked_list *peers,
                        struct sockaddr_in6 auction_addr);
/**
 * @brief Ajoute les données d'un pair à la suite dans la liste des pairs.
 *
 * @param[in]   peers Liste contenant les infos des pairs.
 * @param[in]   id ID du pair.
 * @param[in]   ip IP du pair.
 * @param[in]   port Port du pair.
 * @param[in]   cle Clé du pair.
 *
 * @return `-1` si erreur, `0` sinon.
 */
int append_peer_data_to_list(linked_list *peers, uint16_t id, const uint8_t *ip, uint16_t port,
                             const char *cle);

#endif