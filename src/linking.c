#include "linking.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "protocol.h"
#include "utils.h"

/**
 * @brief Envoi le message pour rejoindre le système d'enchères (`code = 3`).
 *
 * @param[in]   sock Socket UDP.
 * @param[in]   linking_addr Adresse multicast.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int send_join_request(int sock, struct sockaddr_in6 linking_addr);
/**
 * @brief Gère les réponses de la part des autres pairs suite à la demande initiale.
 *
 * @param[in]   sock Socket serveur TCP.
 *
 * @return `-1` en cas d'erreur, `0` s'il n'y a pas eu de réponse, `1` sinon.
 */
int await_join_req_response(int sock);

int process_linking(pers_sockets socks, struct sockaddr_in6 linking_addr) {
    // b) 1. Envoi de la demande + attente de réponse
    int attempts = 0;
    int res = 0;
    do {
        send_join_request(socks.udp_sock, linking_addr);
        attempts++;
        printf("Trying to join the auction system... Attempt %d.\n", attempts);
    } while (attempts < 3 && (res = await_join_req_response(socks.tcp_serv_sock)) == 0);

    // Erreur
    if (res < 0) return -1;

    // b) 2. Pas de réponse reçue
    if (attempts == 3 && !res) return 0;

    return 1;
}

int send_join_request(int sock, struct sockaddr_in6 linking_addr) {
    message msg;
    memset(&msg, 0, sizeof(msg));
    msg.code = JOIN_REQ;

    msg_buffer msg_buf;
    if (init_msg_buffer(&msg_buf, sizeof(uint8_t)) < 0) return -1;
    if (concat_msg_to_buf(&msg_buf, &msg, CODE) < 0) goto error;

    if (sendto(sock, msg_buf.buf, msg_buf.len, 0, (struct sockaddr *)&linking_addr,
               sizeof(linking_addr)) < 0) {
        perror("[ERROR] send");
        goto error;
    }

    return 0;

error:
    free_msg_buffer(msg_buf);
    return -1;
}

int await_join_req_response(int sock) {
    struct pollfd fds = {.fd = sock, .events = POLLIN};
    int poll_ret;
    int got_res = 0;

    while ((poll_ret = poll(&fds, 1, MILLI(5)))) {
        // Demande de connexion reçue.
        if (fds.revents & POLLIN) {
            message msg;
            size_t msg_size = sizeof(msg.code) + sizeof(msg.id) + sizeof(msg.ip) + sizeof(msg.port);
            char buf[msg_size];
            memset(buf, 0, msg_size);

            // Accepte la connexion.
            int peer = accept(sock, NULL, NULL);
            if (peer < 0) continue;

            if (set_nonblock(peer) < 0) {
                close(peer);
                continue;
            }

            // Lecture du message.
            if (read_nb(peer, buf, msg_size, MILLI(5)) < 0) {
                close(peer);
                continue;
            }

            // Code correct.
            if (buf[0] == JOIN_REQ_RES) {
                got_res = 1;

                // Traduction buffer -> message.
                buf_to_msg(buf, &msg, CODE | ID | IP | PORT);

                // ===== DEBUG =====
                char ip_str[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &msg.ip, ip_str, sizeof(ip_str));
                printf("[DEBUG] Response received!\n[DEBUG] From peer %d of ip %s:%d.\n", msg.id,
                       ip_str, ntohs(msg.port));
                // ===== DEBUG =====
            }

            close(peer);
        }
    }

    if (poll_ret < 0) {
        perror("[ERROR] poll");
        return -1;
    }

    return got_res;
}

int handle_join_requests(pers_sockets socks, struct sockaddr_in6 linking_addr) {
    // Socket abonnée à l'adresse de liaison.
    int linking_subs_sock = make_multicast_socket(linking_addr);
    if (linking_subs_sock < 0) return -1;

    struct pollfd fds = {.fd = linking_subs_sock, .events = POLLIN};
    int ret;
    while ((ret = poll(&fds, 1, -1))) {
        // Message multicast reçue.
        if (fds.revents & POLLIN) {
            char buf[1024];
            memset(buf, 0, sizeof(buf));

            // Adresse de l'expéditeur.
            struct sockaddr_in6 addr;
            socklen_t addr_len = sizeof(addr);
            if (recvfrom(linking_subs_sock, buf, sizeof(buf), 0, (struct sockaddr *)&addr,
                         &addr_len) < 0) {
                perror("[ERROR] recvfrom");
                continue;
            }

            // Mauvais code.
            if (buf[0] != JOIN_REQ) continue;

            // ===== DEBUG =====
            char ip_str[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &addr.sin6_addr, ip_str, sizeof(ip_str));
            printf("[DEBUG] Join request received from: %s:%d\n", ip_str, ntohs(addr.sin6_port));
            // ===== DEBUG =====

            // Réponse à l'expéditeur.
            message msg;
            memset(&msg, 0, sizeof(msg));
            msg.code = JOIN_REQ_RES;
            msg.id = 0;  // TODO: Id
            memcpy(msg.ip, socks.addr.sin6_addr.__in6_u.__u6_addr8, 16);
            msg.port = socks.addr.sin6_port;

            msg_buffer msg_buf;
            if (init_msg_buffer(&msg_buf, sizeof(uint8_t)) < 0) continue;
            if (concat_msg_to_buf(&msg_buf, &msg, CODE | ID | IP | PORT) < 0) {
                free_msg_buffer(msg_buf);
                continue;
            }

            // Socket TCP.
            int sock = socket(AF_INET6, SOCK_STREAM, 0);
            if (sock < 0) {
                perror("[ERROR] socket");
                goto error;
            }

            // Connexion via TCP sur l'adresse de l'expéditeur.
            if (connect(sock, (struct sockaddr *)&addr, addr_len) < 0) {
                perror("[ERROR] connect");
                goto error;
            }

            if (set_nonblock(sock) < 0) {
                goto error;
            }

            // Envoi de la réponse.
            if (write_nb(sock, msg_buf.buf, msg_buf.len, MILLI(5)) < 0) {
                perror("[ERROR] send");
                goto error;
            }

            printf("[DEBUG] Response sent.\n");

        error:
            if (sock) close(sock);
            free_msg_buffer(msg_buf);
        }
    }

    close(linking_subs_sock);

    if (ret < 0) {
        perror("[ERROR] poll");
        return -1;
    }

    return 0;
}