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
    // Utilisation du compteur comme numéro de vente
    uint32_t auction_num = p_data->id + *counter + 1;

    (*counter)++;

    // Préparation du message d'initialisation de vente (code 8)
    message auction_init_msg;
    memset(&auction_init_msg, 0, sizeof(message));

    auction_init_msg.code = AUC_INIT;              // CODE = 8
    auction_init_msg.id = htons(p_data->id);       // ID du pair
    auction_init_msg.numv = htonl(auction_num);    // Numéro de vente
    auction_init_msg.prix = htonl(initial_price);  // Prix initial

    if (send_msg_to(auction_init_msg, CODE | ID | NUMV | PRIX, p_data->udp_sock,
                    (struct sockaddr *)&auction_addr, sizeof(auction_addr)) < 0) {
        fprintf(stderr, "Failed to send auction initialization.\n");
        return -1;
    }

    // Ajout de la vente à la liste
    auction_info new_auction;
    new_auction.numv = auction_num;
    new_auction.supervisor_id = p_data->id;
    new_auction.current_price = initial_price;
    new_auction.current_bidder = p_data->id;
    new_auction.is_active = 1;
    new_auction.last_activity = time(NULL);
    new_auction.first_notice_time = 0;
    new_auction.notice_sent = 0;

    if (concat_to_array(active_auctions, &new_auction, sizeof(auction_info)) < 0) {
        return -1;
    }

    printf("Auction #%u initiated with initial price %u.\n", auction_num, initial_price);
    return 0;
}

int place_bid(const pers_data *p_data, struct sockaddr_in6 auction_addr, uint32_t auction_num,
              uint32_t bid_price) {
    // Préparation du message d'enchère (code 9)
    message bid_msg;
    memset(&bid_msg, 0, sizeof(message));

    bid_msg.code = AUC_BID;             // CODE = 9
    bid_msg.id = htons(p_data->id);     // ID du pair
    bid_msg.numv = htonl(auction_num);  // Numéro de vente
    bid_msg.prix = htonl(bid_price);    // Prix proposé

    if (send_msg_to(bid_msg, CODE | ID | NUMV | PRIX, p_data->udp_sock,
                    (struct sockaddr *)&auction_addr, sizeof(auction_addr)) < 0) {
        fprintf(stderr, "Failed to send bid.\n");
        return -1;
    }

    return 0;
}

void list_auctions(dyn_array *active_auctions) {
    printf("  Available auctions:\n");

    if (active_auctions->len == 0) {
        printf("  No auctions available.\n");
        return;
    }

    const auction_info *auctions = (auction_info *)active_auctions->array;
    int count = active_auctions->len / sizeof(auction_info);

    for (int i = 0; i < count; i++) {
        if (auctions[i].is_active) {
            printf("    [ACTIVE] Auction #%u: current price %u (last bidder: %u).\n",
                   auctions[i].numv, auctions[i].current_price, auctions[i].current_bidder);
        } else {
            printf("    [CLOSED] Auction #%u: final price %u   (winner: %u).\n", auctions[i].numv,
                   auctions[i].current_price, auctions[i].current_bidder);
        }
    }
}

/**
 * @brief Vérifie si l'enchère existe dans la liste des enchères.
 *
 * @param[in]   active_auctions Tableau des enchères.
 * @param[in]   auction_num Numéro de l'enchère.
 * @param[in]   check_active Si `!= 0`, vérifie seulement les enchères actives.
 *
 * @return `1` si elle existe, `0` sinon.
 */
int check_auction_exists(dyn_array *active_auctions, uint32_t auction_num, int check_active) {
    const auction_info *auctions = (auction_info *)active_auctions->array;
    int count = active_auctions->len / sizeof(auction_info);
    for (int i = 0; i < count; i++) {
        if (auctions[i].numv == auction_num && (!check_active || auctions[i].is_active)) return 1;
    }
    return 0;
}

void *handle_auctions(void *params) {
    printf("[DEBUG] === Starting auction thread ===\n");

    auction_handler_params *p = (auction_handler_params *)params;
    pers_data *p_data = p->p_data;
    struct sockaddr_in6 auction_addr = p->auction_addr;
    dyn_array *active_auctions = p->active_auctions;
    linked_list *peers = p->peers;

    // Création socket, listen les messages d'enchères
    int listen_sock = make_multicast_socket(auction_addr);
    if (listen_sock < 0) {
        fprintf(stderr, "[ERROR] Failed to create multicast socket for auctions.\n");
        return NULL;
    }

    char buffer[1024];
    while (*p->running) {
        memset(buffer, 0, sizeof(buffer));

        // Recevoir les messages
        int recv_len = recvfrom(listen_sock, buffer, sizeof(buffer), 0, NULL, NULL);
        if (recv_len <= 0) {
            continue;
        }

        // Analyser le message reçu
        message msg;
        memset(&msg, 0, sizeof(message));

        // Déterminer le type de message en lisant le 1er octet (code)
        uint8_t code = buffer[0];

        switch (code) {
            case CONS_VAL: { break; }

            case CONS_CONF: {
                int flags = CODE | ID | LMESS | MESS | LSIG | SIG | NB;
                buf_to_msg(buffer, &msg, flags);

                uint16_t supervisor_id = ntohs(msg.id);
                printf("  Consensus confirmation received from supervisor %u.\n", supervisor_id);

                // Extraction du message original depuis MESS
                message original_msg;
                memset(&original_msg, 0, sizeof(original_msg));
                memcpy(&original_msg, msg.mess, msg.lmess);
                
                switch (original_msg.code) {
                    case AUC_END: {
                        uint32_t auction_num = ntohl(original_msg.numv);
                        uint32_t final_price = ntohl(original_msg.prix);
                        uint16_t winner_id = ntohs(original_msg.id);
                        
                        printf("  Consensus validated: Auction #%u ended, won by peer %u with final price %u.\n", 
                            auction_num, winner_id, final_price);
                        
                        // Marquer l'enchère comme finie dans la liste
                        auction_info *auctions = (auction_info *)active_auctions->array;
                        int count = active_auctions->len / sizeof(auction_info);
                        
                        for (int i = 0; i < count; i++) {
                            if (auctions[i].numv == auction_num) {
                                auctions[i].is_active = 0;
                                auctions[i].current_price = final_price;
                                auctions[i].current_bidder = winner_id;
                                break;
                            }
                        }
                        break;
                    }
                    case AUC_SUP_REDIFF: {
                        // Consensus pour une enchère validée
                        uint32_t auction_num = ntohl(original_msg.numv);
                        uint32_t bid_price = ntohl(original_msg.prix);
                        uint16_t bidder_id = ntohs(original_msg.id);
                        
                        printf("  Consensus validated: Bid of %u on auction #%u by peer %u.\n", 
                            bid_price, auction_num, bidder_id);
                        
                        // Maj du prix actuel (déjà fait normalement mais sécu cas où)
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
                }
                break;
            }

            case CONS_CONF_FOLLOW: { break; }

            case AUC_INIT: {
                buf_to_msg(buffer, &msg, CODE | ID | NUMV | PRIX);

                uint32_t auction_num = ntohl(msg.numv);
                uint32_t price = ntohl(msg.prix);
                uint16_t peer_id = ntohs(msg.id);

                // Si la vente n'existe pas, l'ajouter à la liste
                if (check_auction_exists(active_auctions, auction_num, 0)) break;

                auction_info new_auction;
                new_auction.numv = auction_num;
                new_auction.supervisor_id = peer_id;
                new_auction.current_price = price;
                new_auction.current_bidder = peer_id;
                new_auction.first_notice_time = 0;
                new_auction.is_active = 1;

                if (concat_to_array(active_auctions, &new_auction, sizeof(auction_info)) < 0) {
                    fprintf(stderr, "  Failed to initialize auction by peer %u.\n", peer_id);
                    break;
                }

                printf("  New auction #%u initiated by peer %u with current price %u.\n",
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
                if (!check_auction_exists(active_auctions, auction_num, 1)) {
                    printf("  Ignoring bid for non-existent or finished auction #%u.\n",
                           auction_num);
                    break;
                }

                printf("  Bid of %u on auction #%u by peer %u.\n", bid_price, auction_num,
                       bidder_id);

                // Si ce pair est superviseur de la vente, rediff l'enchère
                const auction_info *auctions = (auction_info *)active_auctions->array;
                int count = active_auctions->len / sizeof(auction_info);

                for (int i = 0; i < count; i++) {
                    if (auctions[i].numv == auction_num &&
                        auctions[i].supervisor_id == p_data->id && auctions[i].is_active) {
                        // Check si l'enchère est valide
                        if (bid_price > auctions[i].current_price) {
                            // Rediff l'enchère avec le code AUC_SUP_REDIFF (rediffusion)
                            message relay_msg = msg;
                            relay_msg.code = AUC_SUP_REDIFF;

                            if (send_msg_to(relay_msg, flags, p_data->udp_sock,
                                            (struct sockaddr *)&auction_addr,
                                            sizeof(auction_addr)) < 0) {
                                fprintf(stderr, "Failed to send bid rediffusion.\n");
                            }
                        } else {
                            printf("  Rejected invalid bid on auction #%u (price too low).\n",
                                   auction_num);

                            // Envoi d'un message de rejet (AUC_REJECT = 15)
                            message reject_msg = msg;
                            reject_msg.code = AUC_REJECT;

                            if (send_msg_to(reject_msg, flags, p_data->udp_sock,
                                            (struct sockaddr *)&auction_addr,
                                            sizeof(auction_addr)) < 0) {
                                fprintf(stderr, "Failed to send bid rejection.\n");
                            }
                        }
                    }
                }
                break;
            }

            case AUC_SUP_REDIFF: {  // Rediff de l'enchère par le superviseur (10)
                int flags = CODE | ID | NUMV | PRIX;
                buf_to_msg(buffer, &msg, CODE | ID | NUMV | PRIX);
                uint32_t auction_num = ntohl(msg.numv);
                uint32_t bid_price = ntohl(msg.prix);
                uint16_t bidder_id = ntohs(msg.id);
                int supervisor_id = -1;
                peer_data *supervisor_peer = NULL;

                // ID du superviseur
                auction_info *auctions = (auction_info *)active_auctions->array;
                int count = active_auctions->len / sizeof(auction_info);
                for (int i = 0; i < count; i++) {
                    if (auctions[i].numv == auction_num) supervisor_id = auctions[i].supervisor_id;
                }
                if (supervisor_id == -1) {
                    printf("[ERROR] Unknown supervisor for auction #%u.\n", auction_num);
                    break;
                }

                // Infos du superviseur
                for (node *n = peers->head; n != NULL; n = n->next) {
                    if (n->id == supervisor_id) supervisor_peer = (peer_data *)n->data;
                }
                if (supervisor_id != p_data->id && supervisor_peer == NULL) {
                    printf("[ERROR] Supervisor for auction #%u not found in peers list.\n",
                           auction_num);
                    break;
                }

                // Si superviseur : début du consensus
                if (supervisor_id == p_data->id) {
                    if (supervisor_begin_consensus(*p_data, auction_addr, peers, &msg, flags) < 0) {
                        printf("  Consensus failed for bid of %u on auction #%u.\n", bid_price,
                               auction_num);
                        break;
                    }
                } else {  // Envoi du message de consensus au superviseur
                    // Adresse du superviseur
                    struct sockaddr_in6 addr;
                    memset(&addr, 0, sizeof(addr));
                    addr.sin6_family = AF_INET6;
                    memcpy(&addr.sin6_addr.s6_addr, supervisor_peer->ip,
                           sizeof(addr.sin6_addr.s6_addr));
                    addr.sin6_port = htons(supervisor_peer->port);

                    // ===== DEBUG =====
                    char ip_str[INET6_ADDRSTRLEN];
                    if (inet_ntop(AF_INET6, &addr.sin6_addr, ip_str, sizeof(ip_str)))
                        printf("[DEBUG] CONSENSUS: Supervisor addr: %s %d\n", ip_str,
                               ntohs(addr.sin6_port));
                    // ===== DEBUG =====

                    // Envoi de la validation au superviseur
                    if (send_await_validation_to_supervisor(addr, &msg, flags, *p_data, *peers,
                                                            listen_sock, supervisor_id) < 0) {
                        printf("  Consensus failed for bid of %u on auction #%u.\n", bid_price,
                               auction_num);
                        break;
                    }
                }

                // Màj prix actuel dans la liste de ventes
                for (int i = 0; i < count; i++) {
                    if (auctions[i].numv == auction_num && auctions[i].is_active) {
                        auctions[i].current_price = bid_price;
                        auctions[i].current_bidder = bidder_id;
                        auctions[i].last_activity = time(NULL);
                        auctions[i].notice_sent = 0;
                        printf("  Bid of %u on auction #%u by peer %u validated.\n", bid_price,
                               auction_num, bidder_id);
                        break;
                    }
                }
                break;
            }

            case AUC_NOTICE: {  // Première notification de prix final (11)
                buf_to_msg(buffer, &msg, CODE | ID | NUMV | PRIX);
                uint32_t auction_num = ntohl(msg.numv);
                uint16_t supervisor_id = ntohs(msg.id);
                uint32_t current_price = ntohl(msg.prix);

                printf(
                    "  First notice of ending auction #%u from supervisor %u with current "
                    "price at "
                    "%u.\n",
                    auction_num, supervisor_id, current_price);

                // Mise à jour de l'état de l'enchère
                auction_info *auctions = (auction_info *)active_auctions->array;
                int count = active_auctions->len / sizeof(auction_info);

                for (int i = 0; i < count; i++) {
                    if (auctions[i].numv == auction_num && auctions[i].is_active) {
                        // Activation du flag de la première notification
                        auctions[i].notice_sent = 1;
                        break;
                    }
                }
                break;
            }

            case AUC_END: {  // Deuxième notification et fin de vente (12)
                int flags = CODE | ID | NUMV | PRIX;
                buf_to_msg(buffer, &msg, CODE | ID | NUMV | PRIX);
                uint32_t auction_num = ntohl(msg.numv);
                uint32_t final_price = ntohl(msg.prix);
                uint16_t winner_id = ntohs(msg.id);

                printf("  Auction #%u end proposal, winner being peer %u with final price %u.\n", auction_num,
                       winner_id, final_price);

                // ID du superviseur
                int supervisor_id = -1;
                auction_info *auctions = (auction_info *)active_auctions->array;
                int count = active_auctions->len / sizeof(auction_info);
                
                for (int i = 0; i < count; i++) {
                    if (auctions[i].numv == auction_num) {
                        supervisor_id = auctions[i].supervisor_id;
                        break;
                    }
                }
                if (supervisor_id == -1) {
                    printf("[ERROR] Unknown supervisor for auction #%u.\n", auction_num);
                    break;
                }

                // Si ce pair est le superviseur, start le consensus
                if (supervisor_id == p_data->id) {
                    printf("  Starting consensus as supervisor for auction end #%u...\n", auction_num);
                    if (supervisor_begin_consensus(*p_data, auction_addr, peers, &msg, flags) < 0) {
                        printf("  Consensus failed for auction end #%u.\n", auction_num);
                        break;
                    }
                    printf("  Consensus reached for auction end #%u.\n", auction_num);
                    
                    // Marquer l'enchère comme finie
                    for (int i = 0; i < count; i++) {
                        if (auctions[i].numv == auction_num) {
                            auctions[i].is_active = 0;
                            break;
                        }
                    }
                } else {
                    // Si il n'est pas superviseur, send la validation
                    peer_data *supervisor_peer = NULL;
                    for (node *n = peers->head; n != NULL; n = n->next) {
                        if (n->id == supervisor_id) {
                            supervisor_peer = (peer_data *)n->data;
                            break;
                        }
                    }
                    if (supervisor_peer == NULL) {
                        printf("[ERROR] Supervisor for auction #%u not found in peers list.\n", auction_num);
                        break;
                    }
                    
                    // Adresse du superviseur
                    struct sockaddr_in6 addr;
                    memset(&addr, 0, sizeof(addr));
                    addr.sin6_family = AF_INET6;
                    memcpy(&addr.sin6_addr.s6_addr, supervisor_peer->ip, sizeof(addr.sin6_addr.s6_addr));
                    addr.sin6_port = htons(supervisor_peer->port);

                    // ===== DEBUG =====
                    char ip_str[INET6_ADDRSTRLEN];
                    if (inet_ntop(AF_INET6, &addr.sin6_addr, ip_str, sizeof(ip_str)))
                        printf("[DEBUG] CONSENSUS: Supervisor addr: %s %d\n", ip_str,
                               ntohs(addr.sin6_port));
                    // ===== DEBUG =====

                    // Envoi de la validation au superviseur
                    if (send_await_validation_to_supervisor(addr, &msg, flags, *p_data, *peers,
                                                            listen_sock, supervisor_id) < 0) {
                        printf("  Consensus failed for auction end #%u.\n", auction_num);
                        break;
                    }
                    printf("  Validation sent for auction end #%u.\n", auction_num);
                }
                break;
            }

            case AUC_LEAVE: {  // Quitter le système (13)
                buf_to_msg(buffer, &msg, CODE | ID);
                uint16_t leaving_peer_id = ntohs(msg.id);

                printf("  Peer %u is leaving the auction system.\n", leaving_peer_id);
                // TODO: Implémenter la gestion d'un pair quittant le système
                break;
            }

            case AUC_CONCUR: {  // Concurrence entre deux enchères (14)
                buf_to_msg(buffer, &msg, CODE | ID | NUMV);
                uint32_t auction_num = ntohl(msg.numv);
                uint16_t supervisor_id = ntohs(msg.id);

                printf("  Concurrency issue detected with auction #%u from supervisor %u.\n",
                       auction_num, supervisor_id);
                // TODO: Implémenter la gestion de concurrence
                break;
            }

            case AUC_REJECT: {  // Enchère rejetée car invalide (15)
                buf_to_msg(buffer, &msg, CODE | ID | NUMV | PRIX);
                uint32_t auction_num = ntohl(msg.numv);
                uint32_t rejected_price = ntohl(msg.prix);
                uint16_t bidder_id = ntohs(msg.id);

                printf("  Bid of %u on auction #%u by peer %u was rejected.\n", rejected_price,
                       auction_num, bidder_id);
                break;
            }

            case MISSING_SUP: {  // Superviseur absent (16)
                buf_to_msg(buffer, &msg, CODE | ID | NUMV);
                uint32_t auction_num = ntohl(msg.numv);
                uint16_t reporting_peer_id = ntohs(msg.id);

                printf("  Supervisor for auction #%u is reported missing by peer %u.\n",
                       auction_num, reporting_peer_id);
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

/**************************************************************************************************
 *                                     Gestion fin de vente.                                      *
 **************************************************************************************************/

/**
 * @brief Envoie une première notification de fin d'enchère si `isFinalNotice == 0`.
 * Sinon, envoie la notification de fin d'enchère.
 *
 * Cette fonction envoie un message avec le code 11 ou 12 (`AUC_NOTICE` ou `AUC_END`)
 * pour notifier que le temps d'inactivité a dépassé `t_3` secondes et que l'enchère
 * pourrait bientôt se terminer.
 *
 * @param[in]   p_data Pointeur vers les données personnelles du pair
 * @param[in]   auction_addr Adresse multicast pour les messages d'enchères
 * @param[in]   auction_num Numéro de l'enchère
 * @param[in]   current_price Prix actuel de l'enchère
 * @param[in]   is_final_notice Si c'est la dernière notification de fin
 * @param[in]   winner_id ID du gagnant de l'enchère
 * @param[in]   peers Liste des pairs
 *
 * @return 0 en cas de succès, -1 en cas d'erreur
 */
int send_auction_end_notice(const pers_data *p_data, struct sockaddr_in6 auction_addr,
                            uint32_t auction_num, uint32_t current_price, int is_final_notice,
                            uint16_t winner_id, linked_list *peers) {
    // Préparation du message de notification (code 11 ou 12)
    message notice_msg;
    memset(&notice_msg, 0, sizeof(message));

    notice_msg.numv = htonl(auction_num);    // Numéro de vente
    notice_msg.prix = htonl(current_price);  // Prix actuel

    if (is_final_notice) {
        notice_msg.code = AUC_END;         // CODE = 12
        notice_msg.id = htons(winner_id);  // ID du gagnant

        printf("  Starting consensus for auction's #%u end...\n", auction_num);
        if (supervisor_begin_consensus(*p_data, auction_addr, peers, &notice_msg, 
                                     CODE | ID | NUMV | PRIX) < 0) {
            fprintf(stderr, "Failed to reach consensus for auction end\n");
            return -1;
        }
        printf("  Consensus reached for auction end #%u.\n", auction_num);
    } else {
        notice_msg.code = AUC_NOTICE;       // CODE = 11
        notice_msg.id = htons(p_data->id);  // ID du pair (superviseur)
    }

    if (send_msg_to(notice_msg, CODE | ID | NUMV | PRIX, p_data->udp_sock,
                    (struct sockaddr *)&auction_addr, sizeof(auction_addr)) < 0) {
        fprintf(stderr, "Failed to send auction notice\n");
        return -1;
    }

    return 0;
}

/**
 * @brief Vérifie les enchères inactives et envoie des notifications.
 *
 * Cette fonction parcourt la liste des enchères actives et vérifie si
 * certaines d'entre elles n'ont pas eu d'activité depuis t_3 secondes.
 * Si oui, elle envoie une notification appropriée.
 * Une demande d'initialisation d'enchère est envoyée pour les pairs
 * arrivés en cours d'enchères.
 *
 * @param[in]   p_data Pointeur vers les données personnelles du pair
 * @param[in]   auction_addr Adresse multicast pour les messages d'enchères
 * @param[in]   active_auctions Tableau dynamique contenant la liste des enchères actives
 * @param[in]   peers Liste des pairs
 */
void check_inactive_auctions(pers_data *p_data, struct sockaddr_in6 auction_addr,
                             dyn_array *active_auctions, linked_list *peers) {
    time_t current_time = time(NULL);
    auction_info *auctions = (auction_info *)active_auctions->array;
    int count = active_auctions->len / sizeof(auction_info);

    for (int i = 0; i < count; i++) {
        auction_info *auction = &auctions[i];

        // Vérifier uniquement les enchères actives dont ce pair est le superviseur
        if (!auction->is_active || auction->supervisor_id != p_data->id) continue;

        // Vérifie si c'est le moment d'envoyer la première/dernière notice de fin
        time_t elapsed = current_time - auctions[i].last_activity;
        if (elapsed < T3_SECONDS) continue;

        // Vérifie si c'est le moment d'envoyer la notice de fin
        if (auction->notice_sent) {
            time_t elapsed_since_notice = current_time - auctions[i].first_notice_time;
            if (elapsed_since_notice < T3_SECONDS) continue;
        }

        // Envoi de la notification (code 11 ou 12)
        if (send_auction_end_notice(p_data, auction_addr, auction->numv, auction->current_price,
                                    auction->notice_sent, auction->current_bidder, peers) == 0) {
            if (auction->notice_sent == 0) {
                auction->notice_sent = 1;
                auction->first_notice_time = current_time;

                // Envoi d'un message d'initialisation d'enchère pour les pairs
                // arrivés en cours de route.
                message init_msg;
                memset(&init_msg, 0, sizeof(message));
                init_msg.code = AUC_INIT;
                init_msg.id = htons(p_data->id);
                init_msg.numv = htonl(auction->numv);
                init_msg.prix = htonl(auction->current_price);

                if (send_msg_to(init_msg, CODE | ID | NUMV | PRIX, p_data->udp_sock,
                                (struct sockaddr *)&auction_addr, sizeof(auction_addr)) < 0) {
                    fprintf(stderr, "Failed to send auction reinitialization message.\n");
                }
            } else {
                auction->is_active = 0;  // Enchère marquée comme terminée
            }
        }
    }
}

void *auction_timer_thread(void *params) {
    printf("[DEBUG] === Starting auction timer thread with consensus ===\n");

    auction_handler_params *p = (auction_handler_params *)params;
    pers_data *p_data = p->p_data;
    struct sockaddr_in6 auction_addr = p->auction_addr;
    dyn_array *active_auctions = p->active_auctions;
    linked_list *peers = p->peers;

    while (p->running) {
        // Vérification des enchères inactives
        check_inactive_auctions(p_data, auction_addr, active_auctions, peers);
        sleep(5);  // Vérification toutes les 5 secondes
    }

    return NULL;
}
