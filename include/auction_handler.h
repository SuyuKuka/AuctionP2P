#ifndef AUCTION_HANDLER_H
#define AUCTION_HANDLER_H

#include <stdint.h>
#include <netinet/in.h>
#include "protocol.h"
#include "darray.h"

// Stocker les infos d'une vente
typedef struct auction_info {
    uint32_t numv;           // Numéro de vente
    uint16_t supervisor_id;  // ID superviseur
    uint32_t current_price;  // Prix actuel
    uint16_t current_bidder; // ID du dernier enchérisseur
    int is_active;           // État de la vente
} auction_info;

// Passer des paramètres au thread d'enchères
typedef struct auction_handler_params {
    pers_data *p_data;
    struct sockaddr_in6 auction_addr;
    dyn_array *active_auctions;
    uint32_t *auction_counter;
} auction_handler_params;

/**
 * @brief Initialise une nouvelle vente aux enchères
 *
 * @param[in] p_data Données personnelles du pair
 * @param[in] auction_addr Adresse multicast pour les enchères
 * @param[in,out] counter Compteur de ventes initiées
 * @param[in] initial_price Prix initial de la vente
 * @param[in,out] active_auctions Tableau des ventes actives
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int init_auction(pers_data *p_data, struct sockaddr_in6 auction_addr, 
                 uint32_t *counter, uint32_t initial_price, 
                 dyn_array *active_auctions);

/**
 * @brief Écoute les messages d'enchères et gère les ventes
 *
 * @param[in] params Structure contenant les paramètres nécessaires
 */
void *handle_auctions(void *params);

/**
 * @brief Place une enchère sur une vente existante
 *
 * @param[in] p_data Données personnelles du pair
 * @param[in] auction_addr Adresse multicast pour les enchères
 * @param[in] auction_num Numéro de la vente
 * @param[in] bid_price Prix proposé
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int place_bid(pers_data *p_data, struct sockaddr_in6 auction_addr,
              uint32_t auction_num, uint32_t bid_price);

/**
 * @brief Affiche la liste des ventes actives
 *
 * @param[in] active_auctions Tableau des ventes actives
 */
void list_auctions(dyn_array *active_auctions);

#endif