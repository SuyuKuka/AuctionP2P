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

void *handle_auctions(void *params) {
    auction_handler_params *p = (auction_handler_params *)params;
    pers_data *p_data = p->p_data;
    struct sockaddr_in6 auction_addr = p->auction_addr;
    dyn_array *active_auctions = p->active_auctions;
    uint32_t *auction_counter = p->auction_counter;
    linked_list *peers = p->peers;

    printf("Starting auction handler with %zu peers\n", peers->size);

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
                if (!check_auction_exists(active_auctions, auction_num)) break;

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

                            // Rediff l'enchère avec le code 10 (validation)
                            message relay_msg = msg;
                            relay_msg.code = AUC_SUP_REDIFF;

                            // Si c'est un superviseur, attendre la validation sinon rediffuse le message
                            printf("%u is supervisor and %u is id\n",auctions[i].supervisor_id, ntohs(p_data->id));
                            if (send_msg_to(relay_msg, flags, p_data->udp_sock,
                                        (struct sockaddr *)&auction_addr,
                                        sizeof(auction_addr)) < 0) {
                                fprintf(stderr, "Failed to send bid validation\n");
                            } else {
                                if (auctions[i].supervisor_id == ntohs(p_data->id)) {
                                    printf("[INFO] %u is waiting for new validation bid on auction\n", auctions[i].supervisor_id);
                                    supervisor_wait_for_validation(p_data->udp_serv_sock,
                                                                (struct sockaddr *)&auction_addr,
                                                                peers->size,
                                                                relay_msg,
                                                                auctions[i].supervisor_id);

                                    printf(" Validated new bid on auction #%u\n", auction_num);
                                }
                                printf("[Necessaire???] Validated new bid on auction #%u\n", auction_num);
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
                uint16_t supervisor_id;
                peer_data *supervisor_peer;

                auction_info *auctions = (auction_info *)active_auctions->array;
                int count = active_auctions->len / sizeof(auction_info);

                // Récupérer l'id et le peer du superviseur 
                printf("Peer id : %u\n", ntohs(p_data->id));
                printf("Auction num msg : %u\n", auction_num);
                printf("Count : %d\n", count);

                for (int i = 0; i < count; i++) {
                    printf("Supervisor id: %u\n", auctions[i].supervisor_id);
                    printf("Auction num: %u\n", auctions[i].numv);
                    if (auctions[i].numv == auction_num) {
                        for (node *n = peers->head; n != NULL; n = n->next) {
                            printf("Peer id boucle: %u\n", ntohs(n->id));
                            if (ntohs(n->id) == auctions[i].supervisor_id) {
                                supervisor_id = auctions[i].supervisor_id;
                                supervisor_peer = n->data;
                                break;
                            }
                        }
                    }
                }

                printf("Supervisor is %u\n", supervisor_id);

                printf("%hhn:%u\n" ,supervisor_peer->ip, supervisor_peer->port);

                // Créer l'addresse du superviseur
                // struct sockaddr_in6 addr;
                // memset(&addr, 0, sizeof(addr));
                // addr.sin6_family = AF_INET6;
                // memcpy(&addr.sin6_addr.s6_addr, supervisor_peer->ip, sizeof(addr.sin6_addr.s6_addr));
                // addr.sin6_port = supervisor_peer->port;

                // Envoi de la validation au superviseur
                printf("[INFO] %u is sending a new validation bid on auction\n", ntohs(p_data->id));
                // send_validation_to_supervisor(p_data->udp_sock, (struct sockaddr *)&addr, msg);

                // Recevoir la confirmation du superviseur
                message msg_confirmation;
                struct sockaddr_in src_addr;
                socklen_t len = sizeof(src_addr);
                recv_msg_from(&msg_confirmation, CODE | ID | NUMV | PRIX | LMESS | MESS | LSIG | SIG, p_data->udp_serv_sock,
                          (struct sockaddr *)&src_addr, &len);
                
                // Si c’est une validation pour la bonne enchère
                if (msg_confirmation.code == 2 && msg_confirmation.numv == msg.numv) {
                    printf(" Supervisor validated bid of %u on auction #%u by peer %u\n", bid_price,
                       auction_num, bidder_id);

                    // Maj prix actuel dans la liste de ventes
                    auction_info *auctions = (auction_info *)active_auctions->array;
                    int count = active_auctions->len / sizeof(auction_info);

                    for (int i = 0; i < count; i++) {
                        if (auctions[i].numv == auction_num && auctions[i].is_active) {
                            auctions[i].current_price = bid_price;
                            auctions[i].current_bidder = bidder_id;
                            break;
                        }
                    }
                }
                break;
            }

            case AUC_NOTICE: {  // TODO: Première notification de fin (11)
                buf_to_msg(buffer, &msg, CODE | ID | NUMV);
                uint32_t auction_num = ntohl(msg.numv);
                uint16_t supervisor_id = ntohs(msg.id);

                printf(" Notice of ending auction #%u from supervisor %u\n", auction_num,
                       supervisor_id);
                // TODO: Implémenter la gestion de fin d'enchère
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
