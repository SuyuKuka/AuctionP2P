#include "handlers.h"

#include <arpa/inet.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "auction_handler.h"
#include "linking.h"
#include "netio.h"
#include "netutils.h"

void *handle_multicast(void *arg) {
    printf("[DEBUG] === Starting multicast thread ===\n");

    multicast_handler_args *args = (multicast_handler_args *)arg;

    struct sockaddr_in6 linking_addr = args->linking_addr;
    struct sockaddr_in6 auction_addr = args->auction_addr;
    pers_data p_data = args->p_data;
    dyn_array *active_auctions = args->active_auctions;
    linked_list *peers = args->peers;
    // Socket abonnée à l'adresse de liaison.
    int linking_subs_sock = -1;
    // Socket abonnée à l'adresse d'enchères.
    int auction_subs_sock = -1;

    linking_subs_sock = make_multicast_socket(linking_addr);
    if (linking_subs_sock < 0) goto error;
    auction_subs_sock = make_multicast_socket(auction_addr);
    if (auction_subs_sock < 0) goto error;

    message msg;
    struct pollfd fds[2];
    fds[0].fd = linking_subs_sock;
    fds[0].events = POLLIN;
    fds[1].fd = auction_subs_sock;
    fds[1].events = POLLIN;
    while (*args->running) {
        int n_fd = poll(fds, 2, MILLI(10));
        if (n_fd <= 0) continue;

        memset(&msg, 0, sizeof(msg));

        if (n_fd > 0) {
            if (fds[0].revents & POLLIN) {
                // Adresse de l'expéditeur.
                struct sockaddr_in6 addr;
                socklen_t addr_len = sizeof(addr);
                if (recv_msg_from(&msg, CODE, 0, linking_subs_sock, (struct sockaddr *)&addr,
                                  &addr_len) < 0)
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
                        break;
                }
            }

            if (fds[1].revents & POLLIN) {
                char buffer[1024];
                memset(buffer, 0, sizeof(buffer));
                if (recv(auction_subs_sock, buffer, sizeof(buffer), 0) < 0) continue;
                switch (buffer[0]) {
                    case AUC_INIT:
                        buf_to_msg(buffer, &msg, CODE | ID | NUMV | PRIX);
                        handle_auction_init(msg, active_auctions);
                        break;

                    case AUC_BID:
                        buf_to_msg(buffer, &msg, CODE | ID | NUMV | PRIX);
                        handle_auction_bid(msg, active_auctions, p_data, auction_addr);
                        break;

                    case AUC_SUP_REDIFF:
                        buf_to_msg(buffer, &msg, CODE | ID | NUMV | PRIX);
                        handle_rebroadcasted_bid(msg, active_auctions, peers, p_data, auction_addr,
                                                 auction_subs_sock);
                        break;

                    case AUC_NOTICE:
                        buf_to_msg(buffer, &msg, CODE | ID | NUMV | PRIX);
                        handle_auction_first_notice(msg, active_auctions);
                        break;

                    case AUC_END:
                        buf_to_msg(buffer, &msg, CODE | ID | NUMV | PRIX);
                        handle_auction_end_notice(msg, active_auctions, peers, p_data, auction_addr,
                                                  auction_subs_sock);
                        break;

                    case AUC_LEAVE:
                        buf_to_msg(buffer, &msg, CODE | ID);
                        handle_auction_leave(msg, peers, p_data, auction_addr, auction_subs_sock,
                                             active_auctions, args->running);
                        break;

                    case AUC_CONCUR:
                        buf_to_msg(buffer, &msg, CODE | ID | NUMV);
                        handle_auction_concurrent_mess(msg);
                        break;

                    case AUC_REJECT:
                        buf_to_msg(buffer, &msg, CODE | ID | NUMV | PRIX);
                        handle_auction_bid_rejection(msg);
                        break;

                    case MISSING_SUP:
                        buf_to_msg(buffer, &msg, CODE | ID | NUMV);
                        handle_auction_missing_supervisor(msg);
                        break;

                    default:
                        break;
                }
            }
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
    struct pollfd fd = {.fd = p_data.tcp_serv_sock, .events = POLLIN};
    while (*args->running) {
        memset(&msg, 0, sizeof(msg));

        int n_fd = poll(&fd, 1, MILLI(10));
        if (n_fd <= 0 || !(fd.revents & POLLIN)) continue;

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

                handle_info_sharing(peer_sock, peer, p_data, peers, auction_addr);
                free(peer);
                break;
            case JOIN_SHARE_INFO_TO_ALL:
                if (read_msg(peer_sock, &msg, ID | IP | PORT | CLE, MILLI(5)) < 0) goto error;
                // Ajout dans la liste.
                append_peer_data_to_list(peers, ntohs(msg.id), msg.ip, ntohs(msg.port), msg.cle);
                free(peer);
                break;

            default:
                free(peer);
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
