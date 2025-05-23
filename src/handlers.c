#include "handlers.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "linking.h"
#include "netio.h"
#include "netutils.h"

void *handle_multicast(void *arg) {
    printf("[DEBUG] === Starting multicast thread ===\n");

    multicast_handler_args *args = (multicast_handler_args *)arg;

    struct sockaddr_in6 linking_addr = args->linking_addr;
    struct sockaddr_in6 auction_addr = args->auction_addr;
    pers_data p_data = args->p_data;
    // Socket abonnée à l'adresse de liaison.
    int linking_subs_sock = -1;
    // Socket abonnée à l'adresse d'enchères.
    int auction_subs_sock = -1;

    linking_subs_sock = make_multicast_socket(linking_addr);
    if (linking_subs_sock < 0) goto error;
    auction_subs_sock = make_multicast_socket(auction_addr);
    if (auction_subs_sock < 0) goto error;

    message msg;
    while (*args->running) {
        memset(&msg, 0, sizeof(msg));

        // Adresse de l'expéditeur.
        struct sockaddr_in6 addr;
        socklen_t addr_len = sizeof(addr);
        if (recv_msg_from(&msg, ALL, 0, linking_subs_sock, (struct sockaddr *)&addr, &addr_len) < 0)
            continue;

        // ===== DEBUG =====
        char ip_str[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, &addr.sin6_addr, ip_str, sizeof(ip_str)))
            printf("[DEBUG] Multicast message received from: %s %d\n", ip_str,
                   ntohs(addr.sin6_port));
        // ===== DEBUG =====

        switch (msg.code) {
            case JOIN_REQ:
                handle_join_request(addr, p_data);
                break;

            default:
                continue;
        }
    }

error:
    if (linking_subs_sock) close(linking_subs_sock);
    if (auction_subs_sock) close(auction_subs_sock);

    return NULL;
}

void *handle_tcp(void *arg) {
    printf("[DEBUG] === Starting unicast TCP thread ===\n");

    tcp_handler_args *args = (tcp_handler_args *)arg;

    struct sockaddr_in6 auction_addr = args->auction_addr;
    pers_data p_data = args->p_data;
    linked_list *peers = args->peers;

    message msg;

    while (*args->running) {
        memset(&msg, 0, sizeof(msg));

        peer_data *peer = malloc(sizeof(peer_data));
        if (peer == NULL) continue;

        // Accepte la connexion.
        int peer_sock = accept(p_data.tcp_serv_sock, NULL, NULL);
        if (peer_sock < 0) {
            perror("[ERROR] accept");
            goto error;
        }

        printf("[DEBUG] A peer has connected over TCP.\n");

        if (set_nonblock(peer_sock) < 0) goto error;

        // Lecture du code.
        uint8_t code;
        if (read_nb(peer_sock, &code, sizeof(msg.code), MILLI(5)) < 0) goto error;

        switch (code) {
            case JOIN_SHARE_INFO:
                if (read_msg(peer_sock, &msg, ID | IP | PORT | CLE, MILLI(5)) < 0) goto error;
                peer->id = ntohs(msg.id);
                peer->port = ntohs(msg.port);
                memcpy(&peer->ip, &msg.ip, sizeof(peer->ip));
                memcpy(&peer->cle, &msg.cle, sizeof(peer->cle));

                printf("PEER ID: %d\n", peer->id);

                handle_info_sharing(peer_sock, peer, p_data, peers, auction_addr);
                break;
            case JOIN_SHARE_INFO_TO_ALL:
                if (read_msg(peer_sock, &msg, ID | IP | PORT | CLE, MILLI(5)) < 0) goto error;
                // Ajout dans la liste.
                append_peer_data_to_list(peers, ntohs(msg.id), msg.ip, ntohs(msg.port), msg.cle);
                break;

            default:
                break;
        }

        close(peer_sock);
        continue;

    error:
        free(peer);
        close(peer_sock);
    }

    return NULL;
}
