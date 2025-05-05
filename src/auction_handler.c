#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "auction_handler.h"
#include "protocol.h"
#include "netio.h"
#include "netutils.h"
#include "darray.h"


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
int init_auction(pers_data *p_data, struct sockaddr_in6 auction_addr, uint32_t *counter, uint32_t initial_price, dyn_array *active_auctions) {
    (*counter)++;
    
    // Créer un num unique de vente: concaténation de l'ID et du compteur
    uint32_t auction_num = (ntohs(p_data->id) << 16) | *counter;
    
    // Préparer le message d'initialisation de vente (code 8)
    message auction_init_msg;
    memset(&auction_init_msg, 0, sizeof(message));
    
    auction_init_msg.code = AUC_INIT;               // CODE = 8
    auction_init_msg.id = p_data->id;               // ID du pair
    auction_init_msg.numv = htonl(auction_num);     // Numéro de vente
    auction_init_msg.prix = htonl(initial_price);   // Prix initial

    // Préparer le buffer pour l'envoi
    dyn_array buf;
    if (init_dyn_array(&buf, 64) < 0) {
        return -1;
    }
    // Concaténer les champs dans le buffer
    if (concat_msg_to_buf(&buf, &auction_init_msg, CODE | ID | NUMV | PRIX) < 0) {
        free_dyn_array(buf);
        return -1;
    }
    
    // Send message sur l'adresse d'enchères
    if (sendto(p_data->udp_sock, buf.array, buf.len, 0, 
               (struct sockaddr *)&auction_addr, sizeof(auction_addr)) < 0) {
        perror("Failed to send auction initialization");
        free_dyn_array(buf);
        return -1;
    }

    // Ajouter vente à la liste des ventes actives
    auction_info new_auction;
    new_auction.numv = auction_num;
    new_auction.supervisor_id = ntohs(p_data->id);
    new_auction.current_price = initial_price;
    new_auction.current_bidder = ntohs(p_data->id);
    new_auction.is_active = 1;
    
    if (concat_to_array(active_auctions, &new_auction, sizeof(auction_info)) < 0) {
        free_dyn_array(buf);
        return -1;
    }
    
    free_dyn_array(buf);
    printf("Auction #%u initiated with initial price %u\n", auction_num, initial_price);
    return 0;
}


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
              uint32_t auction_num, uint32_t bid_price) {
    
    // Préparer le message d'enchère (code 9)
    message bid_msg;
    memset(&bid_msg, 0, sizeof(message));
    
    bid_msg.code = AUC_BID;             // CODE = 9
    bid_msg.id = p_data->id;            // ID du pair
    bid_msg.numv = htonl(auction_num);  // Numéro de vente
    bid_msg.prix = htonl(bid_price);    // Prix proposé
    
    // Préparer le buffer pour l'envoi
    dyn_array buf;
    if (init_dyn_array(&buf, 64) < 0) {
        return -1;
    }
    
    // Concaténer les champs dans le buffer
    if (concat_msg_to_buf(&buf, &bid_msg, CODE | ID | NUMV | PRIX) < 0) {
        free_dyn_array(buf);
        return -1;
    }
    
    // Send message sur l'adresse d'enchères
    if (sendto(p_data->udp_sock, buf.array, buf.len, 0, 
               (struct sockaddr *)&auction_addr, sizeof(auction_addr)) < 0) {
        perror("Failed to send bid");
        free_dyn_array(buf);
        return -1;
    }
    
    free_dyn_array(buf);
    // printf("Bid of %u placed on auction #%u\n", bid_price, auction_num); // déjà géré par le handler
    return 0;
}


/**
 * @brief Affiche la liste des enchères actives
 * 
 * Parcourt le tableau des enchères actives et affiche pour chaque enchère
 * son numéro, le prix actuel et l'ID du dernier enchérisseur.
 * 
 * @param active_auctions Tableau dynamique contenant la liste des enchères actives
 */
void list_auctions(dyn_array *active_auctions) {
    printf(" Active auctions:\n");
    
    if (active_auctions->len == 0) {
        printf(" No active auctions\n");
        return;
    }
    
    auction_info *auctions = (auction_info *)active_auctions->array;
    int count = active_auctions->len / sizeof(auction_info);
    
    for (int i = 0; i < count; i++) {
        if (auctions[i].is_active) {
            printf("   Auction #%u: current price %u (last bidder: %u)\n", 
                   auctions[i].numv, auctions[i].current_price, auctions[i].current_bidder);
        }
    }
}


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
void *handle_auctions(void *params) {
    auction_handler_params *p = (auction_handler_params *)params;
    pers_data *p_data = p->p_data;
    struct sockaddr_in6 auction_addr = p->auction_addr;
    dyn_array *active_auctions = p->active_auctions;
    
    // Créer socket, listen les messages d'enchères
    int listen_sock = make_multicast_socket(auction_addr);
    if (listen_sock < 0) {
        perror("Failed to create multicast socket for auctions");
        return NULL;
    }
    
    char buffer[1024];
    struct sockaddr_in6 sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    
    printf("Listening for auction messages...\n");

    while (1) {
        // Recevoir les messages
        int recv_len = recvfrom(listen_sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&sender_addr, &sender_len);
        
        if (recv_len <= 0) {
            continue;
        }

        // Analyser le message reçu
        message msg;
        memset(&msg, 0, sizeof(message));
        
        // Déterminer le type de message en lisant le 1er octet (code)
        uint8_t code = buffer[0];

        switch(code) {
            case AUC_INIT : {
                buf_to_msg(buffer, &msg, CODE | ID | NUMV | PRIX);
                
                uint32_t auction_num = ntohl(msg.numv);
                uint32_t price = ntohl(msg.prix);
                uint16_t peer_id = ntohs(msg.id);
                
                printf(" New auction #%u initiated by peer %u with initial price %u\n", 
                       auction_num, peer_id, price);
                
                // Check si cette vente existe déjà
                int exists = 0;
                auction_info *auctions = (auction_info *)active_auctions->array;
                int count = active_auctions->len / sizeof(auction_info);
                for (int i = 0; i < count; i++) {
                    if (auctions[i].numv == auction_num) {
                        exists = 1;
                        break;
                    }
                }
                
                if (!exists) {   // Si la vente n'existe pas, l'ajouter à la liste
                    auction_info new_auction;
                    new_auction.numv = auction_num;
                    new_auction.supervisor_id = peer_id;
                    new_auction.current_price = price;
                    new_auction.current_bidder = peer_id;
                    new_auction.is_active = 1;
                    
                    concat_to_array(active_auctions, &new_auction, sizeof(auction_info));
                }
                break;
            }

            case AUC_BID : {
                buf_to_msg(buffer, &msg, CODE | ID | NUMV | PRIX);
                uint32_t auction_num = ntohl(msg.numv);
                uint32_t bid_price = ntohl(msg.prix);
                uint16_t bidder_id = ntohs(msg.id);
                
                printf(" Bid of %u on auction #%u by peer %u\n", bid_price, auction_num, bidder_id);
                
                // Si ce pair est superviseur de la vente, rediff l'enchère
                auction_info *auctions = (auction_info *)active_auctions->array;
                int count = active_auctions->len / sizeof(auction_info);

                for (int i = 0; i < count; i++) {
                    if (auctions[i].numv == auction_num && 
                        auctions[i].supervisor_id == ntohs(p_data->id) && 
                        auctions[i].is_active) {
                        
                        // Check si l'enchère est valide (prix > prix actuel)
                        if (bid_price > auctions[i].current_price) {
                            // Maj prix actuel et l'enchérisseur
                            auctions[i].current_price = bid_price;
                            auctions[i].current_bidder = bidder_id;
                            
                            // Rediff l'enchère avec le code 10
                            message relay_msg = msg;
                            relay_msg.code = 10;  // Code pour rediff d'enchère
                            
                            dyn_array buf;
                            if (init_dyn_array(&buf, 64) >= 0) {
                                if (concat_msg_to_buf(&buf, &relay_msg, CODE | ID | NUMV | PRIX) >= 0) {
                                    sendto(p_data->udp_sock, buf.array, buf.len, 0, 
                                           (struct sockaddr *)&auction_addr, sizeof(auction_addr));
                                }
                                free_dyn_array(buf);
                            }
                            printf(" Validated new bid on auction #%u\n", auction_num);
                        } else {
                            printf(" Rejected invalid bid on auction #%u (price too low)\n", auction_num);
                        }
                        break;
                    }
                }
                break;
            }

            case 10 : {     // Rediff de l'enchère par le superviseur
                buf_to_msg(buffer, &msg, CODE | ID | NUMV | PRIX);
                uint32_t auction_num = ntohl(msg.numv);
                uint32_t bid_price = ntohl(msg.prix);
                uint16_t bidder_id = ntohs(msg.id);
                
                printf(" Supervisor validated bid of %u on auction #%u by peer %u\n", bid_price, auction_num, bidder_id);

                // TODO: processus de consensus ici, post code 10
                
                // Maj prix actuel dans la liste de ventes
                auction_info *auctions = (auction_info *)active_auctions->array;
                int count = active_auctions->len / sizeof(auction_info);
                
                int auction_exists = 0;
                for (int i = 0; i < count; i++) {
                    if (auctions[i].numv == auction_num && auctions[i].is_active) {
                        auctions[i].current_price = bid_price;
                        auctions[i].current_bidder = bidder_id;
                        auction_exists = 1;
                        break;
                    }
                }
                
                // Si l'enchère n'existe pas encore dans la liste, l'add
                if (!auction_exists) {
                    // Détermination du superviseur -> ID dans message 10
                    uint16_t supervisor_id = 0;
                    struct in6_addr *sender_ip = &(sender_addr.sin6_addr);
                    char ip_str[INET6_ADDRSTRLEN];
                    inet_ntop(AF_INET6, sender_ip, ip_str, INET6_ADDRSTRLEN);
                    
                    // Détermination du superviseur à partir de l'IP d'envoi, ou utilisation de l'ID du message
                    supervisor_id = ntohs(msg.id); // ID du message
                    
                    auction_info new_auction;
                    new_auction.numv = auction_num;
                    new_auction.supervisor_id = supervisor_id;
                    new_auction.current_price = bid_price;
                    new_auction.current_bidder = bidder_id;
                    new_auction.is_active = 1;
                    
                    concat_to_array(active_auctions, &new_auction, sizeof(auction_info));
                    printf(" Added new auction #%u to local list with current price %u\n", auction_num, bid_price);
                }
                break;
            }

            default :
                break;
        }
    }

    close(listen_sock);
    return NULL;
}