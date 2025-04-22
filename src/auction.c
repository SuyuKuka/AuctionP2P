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

#define LINKING_IP "ff12::feed:bee"
#define AUCTIONS_IP "ff12::add:beef"
#define LINKING_PORT 57500
#define AUCTIONS_PORT 59500

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

    if (init_dyn_array(&peers_data, sizeof(peer_data) * 5) < 0) goto error;

    // Initialisation udp_sock.
    udp_sock = make_udp_socket();
    if (udp_sock < 0) goto error;
    int udp_sock_port = get_socket_port(udp_sock);
    if (udp_sock_port < 0) goto error;

    // Initialisation de l'adresse de liaison et l'adresse personnelle.
    if (init_multicast_addr(&linking_addr, LINKING_IP, LINKING_PORT) < 0 ||
        init_personal_addr(&personal_addr, udp_sock_port) < 0)
        goto error;

    // ===== DEBUG =====
    char ip_str[INET6_ADDRSTRLEN];
    if (inet_ntop(AF_INET6, &personal_addr.sin6_addr, ip_str, sizeof(ip_str)))
        printf("[DEBUG] Personal addr: %s %d\n", ip_str, ntohs(personal_addr.sin6_port));
    // ===== DEBUG =====

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

    // Demande d'adhésion au système d'enchères
    res = handle_linking(&p_data, &peers_data, linking_addr);
    if (res == 0) {
        printf("Nobody connected, awaiting new peers...\n");
    } else if (res < 0) {
        goto error;
    }

    join_req_params join_req_p = {.p_data = p_data, .linking_addr = linking_addr};
    pthread_t join_req_thread;
    if (pthread_create(&join_req_thread, NULL, handle_join_requests, &join_req_p) < 0) {
        goto error;
    }

    data_sharing_params sharing_p = {.p_data = p_data, .peers_data = &peers_data};
    pthread_t d_sharing_thread;
    if (pthread_create(&d_sharing_thread, NULL, handle_data_sharing, &sharing_p) < 0) {
        goto error;
    }

    while (1);
    // TODO: Etape 3

    close(udp_sock);
    close(tcp_serv_sock);
    close(linking_subs_sock);
    return EXIT_SUCCESS;

error:
    if (udp_sock) close(udp_sock);
    if (tcp_serv_sock) close(tcp_serv_sock);
    if (linking_subs_sock) close(linking_subs_sock);
    free_dyn_array(peers_data);
    return EXIT_FAILURE;
}
