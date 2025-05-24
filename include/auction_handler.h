#ifndef AUCTION_HANDLER_H
#define AUCTION_HANDLER_H

#include <netinet/in.h>
#include <stdint.h>
#include <time.h>

#include "collections.h"
#include "consensus.h"
#include "protocol.h"

/* Structure d'une enchère */
typedef struct auction_info {
    uint32_t numv;            /* Numéro de vente */
    uint16_t supervisor_id;   /* ID du superviseur */
    uint32_t current_price;   /* Prix actuel */
    uint16_t current_bidder;  /* ID de l'enchérisseur actuel */
    int is_active;            /* Indicateur si l'enchère est active */
    time_t last_activity;     /* Timestamp de la dernière activité */
    time_t first_notice_time; /* Timestamp de la première notification */
    int notice_sent;          /* Si la première notification a été envoyée */
} auction_info;

#define T3_SECONDS 20  // 20 secondes

/* Structure pour les paramètres du thread de gestion des enchères inactives */
typedef struct auction_timer_params {
    pers_data p_data;                 /* Données personnelles */
    struct sockaddr_in6 auction_addr; /* Adresse multicast d'enchères */
    dyn_array *active_auctions;       /* Liste des enchères actives */
    int *running;                     /* Si on doit ou non arrêter le thread */
} auction_timer_params;

/**
 * @brief Initialise une nouvelle enchère.
 *
 * Création d'une nouvelle enchère avec un numéro unique et l'enregistre
 * dans la liste active_auctions. Elle envoie également un message d'initialisation
 * d'enchère sur l'adresse multicast spécifiée.
 *
 * @param[in]       p_data Données personnelles du pair
 * @param[in]       auction_addr Adresse multicast pour les messages d'enchères
 * @param[in,out]   counter Compteur utilisé pour les numéros d'enchères
 * @param[in]       initial_price Prix initial de l'enchère
 * @param[in,out]   active_auctions Liste des enchères actives
 *
 * @return 0 en cas de succès, -1 en cas d'erreur
 */
int send_init_auction(pers_data *p_data, struct sockaddr_in6 auction_addr, uint32_t *counter,
                      uint32_t initial_price, dyn_array *active_auctions);

/**
 * @brief Place une enchère sur une vente existante.
 *
 * Envoie un message d'enchère (code 9) sur l'adresse multicast
 * pour participer à une enchère. Le superviseur de l'enchère valide
 * ou non cette offre.
 *
 * @param[in]   p_data Données personnelles du pair
 * @param[in]   auction_addr Adresse multicast pour les messages d'enchères
 * @param[in]   auction_num Numéro de l'enchère sur laquelle placer l'offre
 * @param[in]   bid_price Prix proposé par le pair
 *
 * @return 0 en cas de succès, -1 en cas d'erreur.
 */
int send_place_bid(const pers_data *p_data, struct sockaddr_in6 auction_addr, uint32_t auction_num,
                   uint32_t bid_price);

/**
 * @brief Affiche la liste des enchères actives.
 *
 * Parcourt le tableau des enchères actives et affiche pour chaque enchère
 * son numéro, le prix actuel et l'ID du dernier enchérisseur.
 *
 * @param[in]   active_auctions Liste des enchères actives
 */
void list_auctions(dyn_array *active_auctions);

/**
 * @brief Initialise une enchère.
 *
 * Ajoute la nouvelle enchère reçue dans la liste si elle n'existe pas déjà.
 *
 * @param[in]       msg Message
 * @param[in,out]   active_auctions Liste des enchères actives
 */
void handle_auction_init(message msg, dyn_array *active_auctions);

/**
 * @brief Gère la réception d'un enchérissement.
 *
 * Ignore le message si l'enchère n'est pas en cours.
 * Sinon un processus de consensus débute.
 *
 * @param[in]   msg Message
 * @param[in]   active_auctions Liste des enchères actives
 * @param[in]   p_data Données personnelles
 * @param[in]   auction_addr Adresse d'enchères
 */
void handle_auction_bid(message msg, dyn_array *active_auctions, pers_data p_data,
                        struct sockaddr_in6 auction_addr);

/**
 * @brief Gère la réception de la rediffusion du message d'enchérissement.
 *
 * Met à jour le prix de l'enchère après validation du consensus.
 *
 * @param[in]       msg Message
 * @param[in,out]   active_auctions Liste des enchères actives
 * @param[in]       peers Liste des pairs
 * @param[in]       p_data Données personnelles
 * @param[in]       auction_addr Adresse d'enchères
 * @param[in]       auction_sock Socket abonnée à l'adresse d'enchères
 */
void handle_rebroadcasted_bid(message msg, dyn_array *active_auctions, linked_list *peers,
                              pers_data p_data, struct sockaddr_in6 auction_addr, int auction_sock);

/**
 * @brief Gère la réception de la première notification de fin d'une enchère.
 *
 * Met à jour l'indicateur de première notification dans la liste des enchères.
 *
 * @param[in]       msg Message
 * @param[in,out]   active_auctions Liste des enchères actives
 */
void handle_auction_first_notice(message msg, dyn_array *active_auctions);

/**
 * @brief Gère la réception de la dernière notification de fin d'une enchère.
 *
 * L'enchère devient inactive.
 *
 * @param[in]       msg Message
 * @param[in,out]   active_auctions Liste des enchères actives
 */
void handle_auction_end_notice(message msg, dyn_array *active_auctions, linked_list *peers,
                              pers_data p_data, struct sockaddr_in6 auction_addr, int auction_sock);

/**
 * @brief Gère un pair qui quitte le système d'enchères.
 *
 * @param[in]   msg Message
 */
void handle_auction_leave(message msg);

/**
 * @brief Gère les messages concurrents.
 *
 * @param[in]   msg Message
 */
void handle_auction_concurrent_mess(message msg);

/**
 * @brief Gère le rejet d'une enchère.
 *
 * @param[in]   msg Message
 */
void handle_auction_bid_rejection(message msg);

/**
 * @brief Gère la disparition d'un superviseur.
 *
 * @param[in]   msg Message
 */
void handle_auction_missing_supervisor(message msg);

/**
 * @brief Thread qui vérifie périodiquement les enchères inactives.
 *
 * Ce thread s'exécute en arrière-plan et vérifie périodiquement
 * si certaines enchères sont inactives depuis trop longtemps.
 *
 * @param[in] params Pointeur vers `auction_timer_params`
 */
void *auction_timer_thread(void *params);

#endif