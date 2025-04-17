#include "linking.h"

#include <arpa/inet.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "protocol.h"

#define MILLI(seconds) seconds * 1000

/**
 * @brief Envoi le message pour rejoindre le système d'enchères (`code = 3`).
 *
 * @param[in]   sock Socket d'envoi.
 * @param[in]   linking_addr Adresse multicast.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int send_join_request(int sock, struct sockaddr_in6 linking_addr) {
    message msg;
    memset(&msg, 0, sizeof(msg));
    msg.code = JOIN_REQ;

    msg_buffer msg_buf;
    init_msg_buffer(&msg_buf, sizeof(uint8_t));

    concat_msg_to_buf(&msg_buf, &msg, CODE);

    if (sendto(sock, msg_buf.buf, msg_buf.len, 0, (struct sockaddr *)&linking_addr,
               sizeof(linking_addr)) < 0) {
        perror("[ERROR] send");
        return -1;
    }

    return 0;
}

/**
 * @brief Gère les réponses de la part des autres pairs suite à la demande initiale.
 *
 * @param[in]   linking_addr Adresse multicast.
 *
 * @return `-1` en cas d'erreur, `0` s'il n'y a pas eu de réponse, `1` sinon.
 */
int await_join_req_response(struct sockaddr_in6 linking_addr) {
    int linking_subs_sock = init_multicast_socket(linking_addr);
    if (linking_subs_sock < 0) return -1;

    struct pollfd fd = {.fd = linking_subs_sock, .events = POLLIN};

    int poll_ret;
    int got_res = 0;
    while ((poll_ret = poll(&fd, 1, MILLI(5)))) {
        char buf[1024];
        memset(buf, 0, sizeof(buf));
        read(linking_subs_sock, buf, sizeof(buf));

        if (buf[0] == JOIN_REQ_RES) {
            got_res = 1;
            // TODO: b) 3.
            printf("Response received\n");
            break;
        }
    }

    if (poll_ret < 0) {
        perror("[ERROR] poll");
        return -1;
    }

    return got_res;
}

int process_linking(struct sockaddr_in6 linking_addr) {
    // Socket UDP pour l'envoi
    int send_sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if ((send_sock) < 0) {
        perror("[ERROR] socket");
        goto error;
    }

    // b) 1. Envoi de la demande + attente de réponse
    int attempts = 0;
    int res = 0;
    do {
        send_join_request(send_sock, linking_addr);
        attempts++;
        printf("Trying to join the auction system... Attempt %d.\n", attempts);
    } while (attempts < 3 && (res = await_join_req_response(linking_addr)) == 0);

    if (res < 0) goto error;

    // b) 2. Pas de réponse reçue
    if (attempts == 3 && !res) return 0;

    return 1;

error:
    if (send_sock) close(send_sock);
    return -1;
}

int handle_join_requests(struct sockaddr_in6 linking_addr) {
    int linking_subs_sock = init_multicast_socket(linking_addr);
    if (linking_subs_sock < 0) return -1;
    struct pollfd fds = {.fd = linking_subs_sock, .events = POLLIN};

    int ret;
    while ((ret = poll(&fds, 1, -1))) {
        // TODO: b) 3.
        // =================== Test ===================
        char buf[1024];
        memset(buf, 0, sizeof(buf));

        struct sockaddr_in6 addr;
        socklen_t sock_len = sizeof(addr);
        if (recvfrom(linking_subs_sock, buf, sizeof(buf), 0, (struct sockaddr *)&addr, &sock_len) <
            0) {
            perror("[ERROR] recvfrom");
        }

        uint8_t code = buf[0];
        if (code != JOIN_REQ) continue;

        char ip_str[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, &addr.sin6_addr, ip_str, sizeof(ip_str)) == NULL) {
            perror("inet_ntop failed for IPv6");
        }
        printf("Join request from: %s:%d\n", ip_str, ntohs(addr.sin6_port));

        message msg;
        memset(&msg, 0, sizeof(msg));
        msg.code = JOIN_REQ_RES;

        msg_buffer msg_buf;
        init_msg_buffer(&msg_buf, sizeof(uint8_t));

        concat_msg_to_buf(&msg_buf, &msg, CODE);

        if (sendto(linking_subs_sock, msg_buf.buf, msg_buf.len, 0, (struct sockaddr *)&linking_addr,
                   sizeof(linking_addr)) < 0) {
            perror("[ERROR] send");
            return -1;
        }
        printf("Response sent.\n");
        // ============================================
    }

    if (ret < 0) {
        perror("[ERROR] poll");
        close(linking_subs_sock);
        return -1;
    }

    return 0;
}