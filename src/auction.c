#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "handlers.h"
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
    int udp_serv_sock = -1;             // Socket UDP pour la réception de messages en unicast.
    int tcp_serv_sock = -1;             // Socket TCP pour la réception de messages.
    int linking_subs_sock = -1;         // Socket UDP abonnée à l'adresse de liaison.
    struct sockaddr_in6 linking_addr;   // Adresse de liaison.
    struct sockaddr_in6 auction_addr;   // Adresse d'enchères.
    struct sockaddr_in6 personal_addr;  // Adresse personnelle.
    linked_list peers;                  // Adresses et clés des pairs.
    pers_data p_data;                   // Données personnelles.
    int running = 1;                    // Pour savoir si le pair a quitté ou non le système.
    uint32_t auction_counter = 0;       // Compteur des ventes initiées
    dyn_array active_auctions;          // Liste des ventes actives

    memset(&peers, 0, sizeof(peers));
    memset(&p_data, 0, sizeof(p_data));
    init_dyn_array(&active_auctions, sizeof(auction_info));

    // Initialisation udp_sock.
    udp_sock = make_udp_socket();
    if (udp_sock < 0) goto error;
    int udp_sock_port = get_socket_port(udp_sock);
    if (udp_sock_port < 0) goto error;

    // Initialisation de l'adresse de liaison et l'adresse personnelle.
    if (init_multicast_addr(&linking_addr, LINKING_IP, LINKING_PORT) < 0 ||
        init_personal_addr(&personal_addr, udp_sock_port) < 0)
        goto error;

    // Initialisation udp_serv_sock.
    udp_serv_sock = make_udp_receiver_socket(personal_addr.sin6_port);
    if (udp_serv_sock < 0) goto error;

    // Initialisation tcp_serv_sock.
    tcp_serv_sock = make_tcp_server_socket(udp_sock_port);
    if (tcp_serv_sock < 0) goto error;

    // Initialisation p_data.
    srand(time(NULL));
    p_data.addr = personal_addr;
    p_data.udp_sock = udp_sock;
    p_data.udp_serv_sock = udp_serv_sock;
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
    res = handle_linking(&p_data, &peers, linking_addr, &auction_addr);
    if (res == 0) {
        // Initialisation de l'adresse de d'enchères.
        printf("Nobody connected, awaiting new peers...\n");
        if (init_multicast_addr(&auction_addr, AUCTIONS_IP, AUCTIONS_PORT) < 0) goto error;
    } else if (res < 0) {
        goto error;
    }

    printf("Successfully joined the auction system!\n");

    multicast_handler_args multicast_args = {.linking_addr = linking_addr,
                                             .auction_addr = auction_addr,
                                             .p_data = p_data,
                                             .running = &running};
    tcp_handler_args tcp_args = {
        .auction_addr = auction_addr, .p_data = p_data, .peers = &peers, .running = &running};
    pthread_t join_req_thread;
    if (pthread_create(&join_req_thread, NULL, handle_multicast, &multicast_args) < 0) {
        goto error;
    }
    pthread_t d_sharing_thread;
    if (pthread_create(&d_sharing_thread, NULL, handle_tcp, &tcp_args) < 0) {
        goto error;
    }

   // Initialisation du thread de gestion des enchères
    pthread_t auction_thread;
    auction_handler_params *auction_params = malloc(sizeof(auction_handler_params));
    if (auction_params == NULL) {
        goto error;
    }
    auction_params->p_data = &p_data;
    auction_params->auction_addr = auction_addr;
    auction_params->active_auctions = &active_auctions;

    if (pthread_create(&auction_thread, NULL, handle_auctions, auction_params) < 0) {
        free(auction_params);
        goto error;
    }

    printf("\nAuction system initialized. Type 'help' for available commands.\n");


    // Interface user pour la gestion des enchères
    char input[256];
    char cmd[64];

    while (running) {
        fflush(stdout);
        printf("> ");
        
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
        else if (strcmp(cmd, "quit") == 0) {    // Quitter
            running = 0;
            break;
        }
        else {
            printf("Unknown command. Type 'help' for available commands.\n");
        }
    }
    // TODO: Etape 3

    // Nettoyage des ressources
    free(auction_params);
    pthread_cancel(auction_thread);
    pthread_join(auction_thread, NULL);
    pthread_cancel(join_req_thread);
    pthread_join(join_req_thread, NULL);
    pthread_cancel(d_sharing_thread);
    pthread_join(d_sharing_thread, NULL);

    close(udp_sock);
    close(udp_serv_sock);
    close(tcp_serv_sock);
    close(linking_subs_sock);
    free_list(&peers);
    free_dyn_array(active_auctions);
    return EXIT_SUCCESS;

error:
    if (udp_sock) close(udp_sock);
    if (udp_serv_sock) close(udp_serv_sock);
    if (tcp_serv_sock) close(tcp_serv_sock);
    if (linking_subs_sock) close(linking_subs_sock);
    free_list(&peers);
    free_dyn_array(active_auctions);
    return EXIT_FAILURE;
}