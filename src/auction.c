#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "linking.h"
#include "netutils.h"
#include "protocol.h"
#include "auction_handler.h"

#define LINKING_IP "ff12::feed:bee"
#define AUCTIONS_IP "ff12::add:beef"
#define LINKING_PORT 57500
#define AUCTIONS_PORT 59500

void print_help() {
    printf("Available commands:\n");
    printf("  init <initial_price> - Initialize a new auction\n");
    printf("  bid <auction_num> <price> - Place a bid on an auction\n");
    printf("  list - List active auctions\n");
    printf("  help - Display this help message\n");
    printf("  quit - Exit the program\n");
}

int main(int argc, char const *argv[]) {
    // TODO: Demander l'adresse et le port ?
    (void)argc;
    (void)argv;

    int res;
    int udp_sock = -1;                  // Socket UDP pour l'envoi de messages.
    int tcp_serv_sock = -1;             // Socket TCP pour la réception de messages.
    int linking_subs_sock = -1;         // Socket UDP abonnée à l'adresse de liaison.
    struct sockaddr_in6 linking_addr;   // Adresse de liaison.
    struct sockaddr_in6 auction_addr;   // Adresse d'enchères.
    struct sockaddr_in6 personal_addr;  // Adresse personnelle.
    dyn_array peers_data;               // Adresses et clés des pairs.
    pers_data p_data;
    //
    uint32_t auction_counter = 0;       // Compteur des ventes initiées
    dyn_array active_auctions;          // Liste des ventes actives

    if (init_dyn_array(&peers_data, sizeof(peer_data) * 5) < 0) goto error;
    if (init_dyn_array(&active_auctions, sizeof(auction_info) * 5) < 0) goto error; //

    // Initialisation udp_sock.
    udp_sock = make_udp_socket();
    if (udp_sock < 0) goto error;
    int udp_sock_port = get_socket_port(udp_sock);
    if (udp_sock_port < 0) goto error;

    // Initialisation de l'adresse de liaison et l'adresse personnelle.
    if (init_multicast_addr(&linking_addr, LINKING_IP, LINKING_PORT) < 0 ||
        init_personal_addr(&personal_addr, udp_sock_port) < 0)
        goto error;

    // Initialisation tcp_serv_sock.
    tcp_serv_sock = make_tcp_server_socket(udp_sock_port);
    if (tcp_serv_sock < 0) goto error;

    // Initialisation p_data.
    srand(time(NULL));
    p_data.addr = personal_addr;
    p_data.udp_sock = udp_sock;
    p_data.tcp_serv_sock = tcp_serv_sock;
    p_data.id = htons(rand() % UINT16_MAX);
    memset(&p_data.cle, 0, sizeof(p_data.cle));

    // ===== DEBUG =====
    char ip_str[INET6_ADDRSTRLEN];
    if (inet_ntop(AF_INET6, &personal_addr.sin6_addr, ip_str, sizeof(ip_str)))
        printf("[DEBUG] Personal addr: %s %d\n[DEBUG] Personal ID: %d\n", ip_str,
               ntohs(personal_addr.sin6_port), ntohs(p_data.id));
    // ===== DEBUG =====

    // Demande d'adhésion au système d'enchères
    res = handle_linking(&p_data, &peers_data, linking_addr, &auction_addr);
    if (res == 0) {
        // Initialisation de l'adresse de d'enchères.
        printf("Nobody connected, awaiting new peers...\n");
        if (init_multicast_addr(&auction_addr, AUCTIONS_IP, AUCTIONS_PORT) < 0) goto error;
    } else if (res < 0) {
        goto error;
    }

    join_req_params join_req_p = {.p_data = p_data, .linking_addr = linking_addr};
    pthread_t join_req_thread;
    if (pthread_create(&join_req_thread, NULL, handle_join_requests, &join_req_p) < 0) {
        goto error;
    }

    data_sharing_params sharing_p = {
        .p_data = p_data, .peers_data = &peers_data, .auction_port = auction_addr.sin6_port};
    memcpy(sharing_p.auction_ip, &auction_addr.sin6_addr.s6_addr, sizeof(sharing_p.auction_ip));
    pthread_t d_sharing_thread;
    if (pthread_create(&d_sharing_thread, NULL, handle_data_sharing, &sharing_p) < 0) {
        goto error;
    }

    pthread_t auction_thread;
    auction_handler_params *auction_params = malloc(sizeof(auction_handler_params));
    auction_params->p_data = &p_data;
    auction_params->auction_addr = auction_addr;
    auction_params->active_auctions = &active_auctions;
    auction_params->auction_counter = &auction_counter;

    if (pthread_create(&auction_thread, NULL, handle_auctions, auction_params) < 0) {
        free(auction_params);
        goto error;
    }

    printf("\nAuction system initialized. Type 'help' for available commands.\n");


    // Interface utilisateur pour gérer les enchères
    char input[256];
    char cmd[64];

    while (1) {
        printf("> ");
        fflush(stdout);
        
        // Obtenir la cmd de l'user
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // Extraction de la cmd
        if (sscanf(input, "%s", cmd) != 1) {
            continue;
        }
        
        if (strcmp(cmd, "init") == 0) {         // Init une nouvelle vente
            uint32_t initial_price;
            if (sscanf(input, "%*s %u", &initial_price) == 1) {
                if (init_auction(&p_data, auction_addr, &auction_counter, initial_price, &active_auctions) < 0) {
                    printf("Failed to initialize auction\n");
                }
            } else {
                printf("Usage: init <initial_price>\n");
            }
        }
         else if (strcmp(cmd, "bid") == 0) {    // Placer une enchère
            uint32_t auction_num, bid_price;
            if (sscanf(input, "%*s %u %u", &auction_num, &bid_price) == 2) {
                if (place_bid(&p_data, auction_addr, auction_num, bid_price) < 0) {
                    printf(" Failed to place bid\n");
                }
            } else {
                printf("Usage: bid <auction_number> <bid_price>\n");
            }
        }
        else if (strcmp(cmd, "list") == 0) {
            list_auctions(&active_auctions);    // Lister les ventes actives
        }
        else if (strcmp(cmd, "help") == 0) {    // Afficher l'aide
            print_help();
        }
        else if (strcmp(cmd, "quit") == 0) {    // Quit
            break;
        }
        else {
            printf("Unknown command. Type 'help' for available commands.\n");
        }
    }
    // while (1);
    // TODO: Etape 3
    /* Initialiser le thread des enchères ici ? */

    // Nettoyage des ressources
    free(auction_params);
    pthread_cancel(auction_thread);
    pthread_join(auction_thread, NULL);
    pthread_cancel(join_req_thread);
    pthread_join(join_req_thread, NULL);
    pthread_cancel(d_sharing_thread);
    pthread_join(d_sharing_thread, NULL);
    //

    close(udp_sock);
    close(tcp_serv_sock);
    close(linking_subs_sock);
    return EXIT_SUCCESS;

error:
    if (udp_sock) close(udp_sock);
    if (tcp_serv_sock) close(tcp_serv_sock);
    if (linking_subs_sock) close(linking_subs_sock);
    free_dyn_array(peers_data);
    free_dyn_array(active_auctions); //
    return EXIT_FAILURE;
}