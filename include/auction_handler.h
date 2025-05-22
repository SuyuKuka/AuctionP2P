#ifndef AUCTION_HANDLER_H
#define AUCTION_HANDLER_H

#include <netinet/in.h>
#include <stdint.h>
#include <time.h>

#include "collections.h"
#include "protocol.h"

/* Structure d'une enchère */
typedef struct auction_info {
    uint32_t numv;              /* Numéro de vente */
    uint16_t supervisor_id;     /* ID du superviseur */
    uint32_t current_price;     /* Prix actuel */
    uint16_t current_bidder;    /* ID de l'enchérisseur actuel */
    int is_active;              /* Indicateur si l'enchère est active */
    time_t last_activity;       /* Timestamp de la dernière activité */
    time_t first_notice_time;   /* Timestamp de la première notification */
    int notice_sent;            /* Si la première notification a été envoyée */
} auction_info;

#define T3_SECONDS 20  // 20 secondes, pour tests

/* Structure pour les paramètres du thread de gestion des enchères */
typedef struct auction_handler_params {
    pers_data *p_data;                /* Données personnelles */
    struct sockaddr_in6 auction_addr; /* Adresse multicast d'enchères */
    dyn_array *active_auctions;       /* Liste des enchères actives */
    unsigned int *auction_counter;    /* Pointeur vers le compteur d'enchères */
    int *running;                     /* Si on doit ou non arrêter le thread */
} auction_handler_params;

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
int init_auction(pers_data *p_data, struct sockaddr_in6 auction_addr, uint32_t *counter,
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
int place_bid(const pers_data *p_data, struct sockaddr_in6 auction_addr, uint32_t auction_num,
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
 * @brief Vérifie si l'enchère existe dans la liste des enchères.
 *
 * @param[in]   active_auctions Buffer.
 * @param[in]   auction_num Message.
 *
 * @return `1` si elle existe, `0` sinon.
 */
int check_auction_exists(dyn_array *active_auctions, uint32_t auction_num);

/**
 * @brief Envoie une première notification de fin d'enchère
 * 
 * Cette fonction envoie un message avec le code 11 (AUC_NOTICE) pour notifier
 * que le temps d'inactivité a dépassé t_3 secondes et que l'enchère pourrait
 * bientôt se terminer.
 * 
 * @param p_data Pointeur vers les données personnelles du pair
 * @param auction_addr Adresse multicast pour les messages d'enchères
 * @param auction_num Numéro de l'enchère 
 * @param current_price Prix actuel de l'enchère
 * 
 * @return 0 en cas de succès, -1 en cas d'erreur
 */
int send_auction_notice(pers_data *p_data, struct sockaddr_in6 auction_addr,
                       uint32_t auction_num, uint32_t current_price);

/**
 * @brief Envoie la notification finale de fin d'enchère
 * 
 * Cette fonction envoie un message avec le code 12 (AUC_END) pour notifier
 * la fin de l'enchère après un second délai de t_3 secondes sans activité.
 * 
 * @param p_data Pointeur vers les données personnelles du pair
 * @param auction_addr Adresse multicast pour les messages d'enchères
 * @param auction_num Numéro de l'enchère 
 * @param final_price Prix final de l'enchère
 * @param winner_id ID du gagnant de l'enchère
 * 
 * @return 0 en cas de succès, -1 en cas d'erreur
 */
int send_auction_end(pers_data *p_data, struct sockaddr_in6 auction_addr,
                    uint32_t auction_num, uint32_t final_price, uint16_t winner_id);

/**
 * @brief Vérifie les enchères inactives et envoie des notifications
 * 
 * Cette fonction parcourt la liste des enchères actives et vérifie si
 * certaines d'entre elles n'ont pas eu d'activité depuis t_3 secondes.
 * Si oui, elle envoie une notification appropriée. -> rajouter pour d) 2.
 * 
 * @param p_data Pointeur vers les données personnelles du pair
 * @param auction_addr Adresse multicast pour les messages d'enchères
 * @param active_auctions Tableau dynamique contenant la liste des enchères actives
 */
void check_inactive_auctions(pers_data *p_data, struct sockaddr_in6 auction_addr, 
                           dyn_array *active_auctions);

/**
 * @brief Thread qui vérifie périodiquement les enchères inactives
 * 
 * Ce thread s'exécute en arrière-plan et vérifie périodiquement
 * si certaines enchères sont inactives depuis trop longtemps.
 * 
 * @param params Pointeur vers auction_timer_params
 */
void *auction_timer_thread(void *params);

/**
 * @brief Thread gérant les messages d'enchères.
 *
 * S'exécute dans un thread séparé, responsable de :
 *
 * - l'écoute des messages d'enchères sur l'addr multicast;
 *
 * - traiter des différents types de messages (`AUC_INIT`, `AUC_BID`, 'AUC_SUP_REDIFF' et plus à rajouter);
 *
 * - mettre à jour la liste active_auctions;
 *
 * - valider et rediffuser les enchères pour lesquelles ce pair est superviseur.
 *
 * @param[in,out]   params Pointeur vers auction_handler_params
 */
void *handle_auctions(void *params);

#endif