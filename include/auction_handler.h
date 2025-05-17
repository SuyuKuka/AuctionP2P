#ifndef AUCTION_HANDLER_H
#define AUCTION_HANDLER_H

#include <stdint.h>
#include <netinet/in.h>
#include "protocol.h"
#include "collections.h"

/* Structure d'une enchère */
typedef struct auction_info {
    uint32_t numv;            /* Numéro de vente */
    uint16_t supervisor_id;   /* ID du superviseur */
    uint32_t current_price;   /* Prix actuel */
    uint16_t current_bidder;  /* ID de l'enchérisseur actuel */
    int is_active;            /* Indicateur si l'enchère est active */
} auction_info;

/* Structure pour les paramètres du thread de gestion des enchères */
typedef struct auction_handler_params {
    pers_data *p_data;                /* Données personnelles */
    struct sockaddr_in6 auction_addr; /* Adresse multicast d'enchères */
    dyn_array *active_auctions;       /* Liste des enchères actives */
} auction_handler_params;

/**
 * @brief Initialise une nouvelle enchère
 * 
 * Création d'une nouvelle enchère avec un numéro unique et l'enregistre
 * dans la liste active_auctions. Elle envoie également un message d'initialisation
 * d'enchère sur l'adresse multicast spécifiée.
 * 
 * @param p_data Pointeur vers les données personnelles du pair
 * @param auction_addr Adresse multicast pour les messages d'enchères
 * @param counter Pointeur vers le compteur utilisé pour les numéros d'enchères
 * @param initial_price Prix initial de l'enchère
 * @param active_auctions Tableau dynamique contenant la liste des enchères actives
 * 
 * @return 0 en cas de succès, -1 en cas d'erreur
 */
int init_auction(pers_data *p_data, struct sockaddr_in6 auction_addr, 
                 uint32_t *counter, uint32_t initial_price, dyn_array *active_auctions);

/**
 * @brief Place une enchère sur une vente existante
 * 
 * Envoie un message d'enchère (code 9) sur l'adresse multicast
 * pour participer à une enchère. Le superviseur de l'enchère valide
 * ou non cette offre.
 * 
 * @param p_data Pointeur vers les données personnelles du pair
 * @param auction_addr Adresse multicast pour les messages d'enchères
 * @param auction_num Numéro de l'enchère sur laquelle placer l'offre
 * @param bid_price Prix proposé par le pair
 * 
 * @return 0 en cas de succès, -1 en cas d'erreur.
 */
int place_bid(pers_data *p_data, struct sockaddr_in6 auction_addr,
              uint32_t auction_num, uint32_t bid_price);

/**
 * @brief Affiche la liste des enchères actives
 * 
 * Parcourt le tableau des enchères actives et affiche pour chaque enchère
 * son numéro, le prix actuel et l'ID du dernier enchérisseur.
 * 
 * @param active_auctions Tableau dynamique contenant la liste des enchères actives
 */
void list_auctions(dyn_array *active_auctions);

/**
 * @brief Thread gérant les messages d'enchères
 * 
 * S'exécute dans un thread séparé, responsable de:
 * - L'écoute des messages d'enchères sur l'addr multicast
 * - Traitement des différents types de messages (AUC_INIT, AUC_BID, 10)
 * - Mettre à jour la liste active_auctions
 * - Valider et rediffuser les enchères pour lesquelles ce pair est superviseur
 * 
 * @param params Pointeur vers auction_handler_params
 */
void *handle_auctions(void *params);

#endif