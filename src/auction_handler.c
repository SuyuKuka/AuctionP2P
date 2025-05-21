#include "auction_handler.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "collections.h"
#include "netio.h"
#include "netutils.h"
#include "protocol.h"

int init_auction(pers_data *p_data, struct sockaddr_in6 auction_addr, uint32_t *counter,
                 uint32_t initial_price, dyn_array *active_auctions) {
    (*counter)++;

    // Utilisation directe du compteur comme numéro de vente
    uint32_t auction_num = *counter;

    // Préparation du message d'initialisation de vente (code 8)
    message auction_init_msg;
    memset(&auction_init_msg, 0, sizeof(message));

    auction_init_msg.code = AUC_INIT;              // CODE = 8
    auction_init_msg.id = p_data->id;              // ID du pair
    auction_init_msg.numv = htonl(auction_num);    // Numéro de vente
    auction_init_msg.prix = htonl(initial_price);  // Prix initial

    if (send_msg_to(auction_init_msg, CODE | ID | NUMV | PRIX, p_data->udp_sock,
                    (struct sockaddr *)&auction_addr, sizeof(auction_addr)) < 0) {
        fprintf(stderr, "Failed to send auction initialization\n");
        return -1;
    }

    // Ajout de la vente à la liste
    auction_info new_auction;
    new_auction.numv = auction_num;
    new_auction.supervisor_id = ntohs(p_data->id);
    new_auction.current_price = initial_price;
    new_auction.current_bidder = ntohs(p_data->id);
    new_auction.is_active = 1;
    new_auction.last_activity = time(NULL);
    new_auction.notice_sent = 0;

    if (concat_to_array(active_auctions, &new_auction, sizeof(auction_info)) < 0) {
        return -1;
    }

    printf("Auction #%u initiated with initial price %u\n", auction_num, initial_price);
    return 0;
}

int place_bid(const pers_data *p_data, struct sockaddr_in6 auction_addr, uint32_t auction_num,
              uint32_t bid_price) {
    // Préparation du message d'enchère (code 9)
    message bid_msg;
    memset(&bid_msg, 0, sizeof(message));

    bid_msg.code = AUC_BID;             // CODE = 9
    bid_msg.id = p_data->id;            // ID du pair
    bid_msg.numv = htonl(auction_num);  // Numéro de vente
    bid_msg.prix = htonl(bid_price);    // Prix proposé

    if (send_msg_to(bid_msg, CODE | ID | NUMV | PRIX, p_data->udp_sock,
                    (struct sockaddr *)&auction_addr, sizeof(auction_addr)) < 0) {
        fprintf(stderr, "Failed to send bid\n");
        return -1;
    }

    return 0;
}

void list_auctions(dyn_array *active_auctions) {
    printf(" Active auctions:\n");

    if (active_auctions->len == 0) {
        printf(" No active auctions\n");
        return;
    }

    const auction_info *auctions = (auction_info *)active_auctions->array;
    int count = active_auctions->len / sizeof(auction_info);

    for (int i = 0; i < count; i++) {
        if (auctions[i].is_active) {
            printf("   Auction #%u: current price %u (last bidder: %u)\n", auctions[i].numv,
                   auctions[i].current_price, auctions[i].current_bidder);
        }
    }
}

/**
 * @brief Vérifie si l'enchère existe dans la liste des enchères.
 *
 * @param[in]   active_auctions Buffer.
 * @param[in]   auction_num Message.
 *
 * @return `1` si elle existe, `0` sinon.
 */
int check_auction_exists(dyn_array *active_auctions, uint32_t auction_num) {
    const auction_info *auctions = (auction_info *)active_auctions->array;
    int count = active_auctions->len / sizeof(auction_info);
    for (int i = 0; i < count; i++) {
        if (auctions[i].numv == auction_num) return 1;
    }
    return 0;
}

int send_auction_notice(pers_data *p_data, struct sockaddr_in6 auction_addr,
                       uint32_t auction_num, uint32_t current_price) {
    // Préparation du message de notification (code 11)
    message notice_msg;
    memset(&notice_msg, 0, sizeof(message));
    
    notice_msg.code = AUC_NOTICE;           // CODE = 11
    notice_msg.id = p_data->id;             // ID du pair (superviseur)
    notice_msg.numv = htonl(auction_num);   // Numéro de vente
    notice_msg.prix = htonl(current_price); // Prix actuel
    
    if (send_msg_to(notice_msg, CODE | ID | NUMV | PRIX, 
                  p_data->udp_sock, (struct sockaddr *)&auction_addr, 
                  sizeof(auction_addr)) < 0) {
        perror("Failed to send auction notice");
        return -1;
    }
    
    printf(" Sent first notice for auction #%u with current price %u\n", 
           auction_num, current_price);
    return 0;
}

void check_inactive_auctions(pers_data *p_data, struct sockaddr_in6 auction_addr, 
                           dyn_array *active_auctions) {
    time_t current_time = time(NULL);
    auction_info *auctions = (auction_info *)active_auctions->array;
    int count = active_auctions->len / sizeof(auction_info);
    
    for (int i = 0; i < count; i++) {
        // Vérifier uniquement les enchères actives dont ce pair est le superviseur
        if (auctions[i].is_active && auctions[i].supervisor_id == ntohs(p_data->id)) {
            time_t elapsed = current_time - auctions[i].last_activity;
            
            // Si t_3 secondes se sont écoulées depuis la dernière activité
            if (elapsed >= T3_SECONDS) {
                if (!auctions[i].notice_sent) {
                    // Première notification (code 11)
                    if (send_auction_notice(p_data, auction_addr, auctions[i].numv, 
                                           auctions[i].current_price) == 0) {
                        auctions[i].notice_sent = 1;
                        auctions[i].last_activity = current_time;
                    }
                } else {
                    // Deuxième notification et fin (code 12)
                    // TODO: d) 2.
                    printf("Oulala vérification pour la 2ème notification !\n");
                }
            }
        }
    }
}

void *auction_timer_thread(void *params) {
    auction_handler_params *p = (auction_handler_params *)params;
    pers_data *p_data = p->p_data;
    struct sockaddr_in6 auction_addr = p->auction_addr;
    dyn_array *active_auctions = p->active_auctions;
    
    while (1) {
        // Vérification des enchères inactives
        check_inactive_auctions(p_data, auction_addr, active_auctions);
        sleep(5); // Vérification toutes les 5 secondes
    }
    
    return NULL;
}

void *handle_auctions(void *params) {
    auction_handler_params *p = (auction_handler_params *)params;
    pers_data *p_data = p->p_data;
    struct sockaddr_in6 auction_addr = p->auction_addr;
    dyn_array *active_auctions = p->active_auctions;
    uint32_t *auction_counter = p->auction_counter;

    // Création socket, listen les messages d'enchères
    int listen_sock = make_multicast_socket(auction_addr);
    if (listen_sock < 0) {
        fprintf(stderr, "Failed to create multicast socket for auctions\n");
        return NULL;
    }

    char buffer[1024];
    struct sockaddr_in6 sender_addr;
    socklen_t sender_len = sizeof(sender_addr);

    printf("Listening for auction messages...\n");

    while (*p->running) {
        memset(buffer, 0, sizeof(buffer));

        // Recevoir les messages
        int recv_len = recvfrom(listen_sock, buffer, sizeof(buffer), 0,
                                (struct sockaddr *)&sender_addr, &sender_len);
        if (recv_len <= 0) {
            continue;
        }

        // Analyser le message reçu
        message msg;
        memset(&msg, 0, sizeof(message));

        // Déterminer le type de message en lisant le 1er octet (code)
        uint8_t code = buffer[0];

        switch (code) {
            case AUC_INIT: {
                buf_to_msg(buffer, &msg, CODE | ID | NUMV | PRIX);

                uint32_t auction_num = ntohl(msg.numv);
                uint32_t price = ntohl(msg.prix);
                uint16_t peer_id = ntohs(msg.id);

                // Si la vente n'existe pas, l'ajouter à la liste
                if (check_auction_exists(active_auctions, auction_num)) break;

                // Mise à jour du compteur pour éviter conflits
                if (auction_num > *auction_counter) {
                    *auction_counter = auction_num;
                }

                auction_info new_auction;
                new_auction.numv = auction_num;
                new_auction.supervisor_id = peer_id;
                new_auction.current_price = price;
                new_auction.current_bidder = peer_id;
                new_auction.is_active = 1;

                if (concat_to_array(active_auctions, &new_auction, sizeof(auction_info)) < 0) {
                    fprintf(stderr, " Failed to initialize auction by peer %u.\n", peer_id);
                    break;
                }

                printf(" New auction #%u initiated by peer %u with current price %u.\n",
                       auction_num, peer_id, price);
                break;
            }

            case AUC_BID: {
                int flags = CODE | ID | NUMV | PRIX;
                buf_to_msg(buffer, &msg, flags);
                uint32_t auction_num = ntohl(msg.numv);
                uint32_t bid_price = ntohl(msg.prix);
                uint16_t bidder_id = ntohs(msg.id);

                // Si l'enchère n'existe pas, on ignore le message.
                if (!check_auction_exists(active_auctions, auction_num)) {
                    printf(" Ignoring bid for non-existent auction #%u\n", auction_num);
                    break;
                }

                printf(" Bid of %u on auction #%u by peer %u\n", bid_price, auction_num, bidder_id);

                // Si ce pair est superviseur de la vente, rediff l'enchère
                auction_info *auctions = (auction_info *)active_auctions->array;
                int count = active_auctions->len / sizeof(auction_info);

                for (int i = 0; i < count; i++) {
                    if (auctions[i].numv == auction_num &&
                        auctions[i].supervisor_id == ntohs(p_data->id) && auctions[i].is_active) {
                        // Check si l'enchère est valide
                        if (bid_price > auctions[i].current_price) {
                            // Maj prix actuel et l'enchérisseur
                            auctions[i].current_price = bid_price;
                            auctions[i].current_bidder = bidder_id;
                            auctions[i].last_activity = time(NULL);
                            auctions[i].notice_sent = 0; // Réinitialisation car nouvelle activité

                            // Rediff l'enchère avec le code AUC_SUP_REDIFF (validation)
                            message relay_msg = msg;
                            relay_msg.code = AUC_SUP_REDIFF;

                            if (send_msg_to(relay_msg, flags, p_data->udp_sock,
                                            (struct sockaddr *)&auction_addr,
                                            sizeof(auction_addr)) < 0) {
                                fprintf(stderr, "Failed to send bid validation\n");
                            } else {
                                printf(" Validated new bid on auction #%u\n", auction_num);
                            }
                        } else {
                            printf(" Rejected invalid bid on auction #%u (price too low)\n",
                                   auction_num);

                            // Envoi d'un message de rejet (AUC_REJECT = 15)
                            message reject_msg = msg;
                            reject_msg.code = AUC_REJECT;

                            if (send_msg_to(reject_msg, flags, p_data->udp_sock,
                                            (struct sockaddr *)&auction_addr,
                                            sizeof(auction_addr)) < 0) {
                                fprintf(stderr, "Failed to send bid rejection\n");
                            }
                        }

                        // Envoi d'un message d'initialisation d'enchère pour les pairs
                        // arrivés en cours de route.
                        message init_msg = {0};
                        init_msg.code = AUC_INIT;
                        init_msg.id = p_data->id;
                        init_msg.numv = htonl(auction_num);
                        init_msg.prix = htonl(auctions[i].current_price);

                        if (send_msg_to(init_msg, flags, p_data->udp_sock,
                                        (struct sockaddr *)&auction_addr,
                                        sizeof(auction_addr)) < 0) {
                            fprintf(stderr,
                                    "[ERROR] Failed to send auction reinitialization message.\n");
                        }
                        break;
                    }
                }
                break;
            }

            case AUC_SUP_REDIFF: {  // Rediff de l'enchère par le superviseur
                buf_to_msg(buffer, &msg, CODE | ID | NUMV | PRIX);
                uint32_t auction_num = ntohl(msg.numv);
                uint32_t bid_price = ntohl(msg.prix);
                uint16_t bidder_id = ntohs(msg.id);

                printf(" Supervisor validated bid of %u on auction #%u by peer %u\n", bid_price,
                       auction_num, bidder_id);

                // TODO: processus de consensus ici, post code 10

                // Maj prix actuel dans la liste de ventes
                auction_info *auctions = (auction_info *)active_auctions->array;
                int count = active_auctions->len / sizeof(auction_info);

                for (int i = 0; i < count; i++) {
                    if (auctions[i].numv == auction_num && auctions[i].is_active) {
                        auctions[i].current_price = bid_price;
                        auctions[i].current_bidder = bidder_id;
                        auctions[i].last_activity = time(NULL);
                        auctions[i].notice_sent = 0; 
                        break;
                    }
                }
                break;
            }

            case AUC_NOTICE: {  // TODO: Première notification de fin (11)
                buf_to_msg(buffer, &msg, CODE | ID | NUMV);
                uint32_t auction_num = ntohl(msg.numv);
                uint16_t supervisor_id = ntohs(msg.id);
                uint32_t current_price = ntohl(msg.prix);

                printf(" Notice of ending auction #%u from supervisor %u with current price at %u\n", 
                        auction_num, supervisor_id, current_price);

                // Mise à jour de l'état de l'enchère
                auction_info *auctions = (auction_info *)active_auctions->array;
                int count = active_auctions->len / sizeof(auction_info);
                
                for (int i = 0; i < count; i++) {
                    if (auctions[i].numv == auction_num && auctions[i].is_active) {
                        // On active le flag de la première notification
                        auctions[i].notice_sent = 1;
                        break;
                    }
                }
                break;
            }

            case AUC_END: {  // TODO: Fin de la vente (12)
                buf_to_msg(buffer, &msg, CODE | ID | NUMV | PRIX);
                uint32_t auction_num = ntohl(msg.numv);
                uint32_t final_price = ntohl(msg.prix);
                uint16_t winner_id = ntohs(msg.id);

                printf(" Auction #%u ended, won by peer %u with final price %u\n", auction_num,
                       winner_id, final_price);

                // Marquer l'enchère comme finie dans la liste
                auction_info *auctions = (auction_info *)active_auctions->array;
                int count = active_auctions->len / sizeof(auction_info);

                for (int i = 0; i < count; i++) {
                    if (auctions[i].numv == auction_num) {
                        auctions[i].is_active = 0;
                        break;
                    }
                }
                break;
            }

            case AUC_LEAVE: {  // Quitter le système (13)
                buf_to_msg(buffer, &msg, CODE | ID);
                uint16_t leaving_peer_id = ntohs(msg.id);

                printf(" Peer %u is leaving the auction system\n", leaving_peer_id);
                // TODO: Implémenter la gestion d'un pair quittant le système
                break;
            }

            case AUC_CONCUR: {  // Concurrence entre deux enchères (14)
                buf_to_msg(buffer, &msg, CODE | ID | NUMV);
                uint32_t auction_num = ntohl(msg.numv);
                uint16_t supervisor_id = ntohs(msg.id);

                printf(" Concurrency issue detected with auction #%u from supervisor %u\n",
                       auction_num, supervisor_id);
                // TODO: Implémenter la gestion de concurrence
                break;
            }

            case AUC_REJECT: {  // Enchère rejetée car invalide (15)
                buf_to_msg(buffer, &msg, CODE | ID | NUMV | PRIX);
                uint32_t auction_num = ntohl(msg.numv);
                uint32_t rejected_price = ntohl(msg.prix);
                uint16_t bidder_id = ntohs(msg.id);

                printf(" Bid of %u on auction #%u by peer %u was rejected\n", rejected_price,
                       auction_num, bidder_id);
                break;
            }

            case MISSING_SUP: {  // Superviseur absent (16)
                buf_to_msg(buffer, &msg, CODE | ID | NUMV);
                uint32_t auction_num = ntohl(msg.numv);
                uint16_t reporting_peer_id = ntohs(msg.id);

                printf(" Supervisor for auction #%u is reported missing by peer %u\n", auction_num,
                       reporting_peer_id);
                // TODO: Implémentation de la gestion d'absence du superviseur
                break;
            }

            default:
                break;
        }
    }

    close(listen_sock);
    return NULL;
}
