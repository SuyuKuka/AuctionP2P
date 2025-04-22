#include "linking.h"

#include <arpa/inet.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "netio.h"

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
 * @brief Gère les réponses de la part des autres pairs suite à la demande initiale (`code = 4`).
 *
 * @param[in]       sock Socket serveur TCP.
 * @param[in,out]   peers_data Adresses et clés des pairs.
 *
 * @return `-1` en cas d'erreur, `0` s'il n'y a pas eu de réponse, `1` sinon.
 */
int await_join_req_response(int sock, dyn_array *peers_data);

/**
 * @brief Gère le partage des données personnelles à un pair choisi.
 *
 * @param[in,out]   p_data Données personnelles.
 * @param[in]       peers_data Données des pairs.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int handle_pers_data_sharing(pers_data *p_data, dyn_array peers_data);
/**
 * @brief Envoi du message contenant les données personnelles au pair `peer`. (`code = 5`).
 *
 * @param[in]   p_data Données personnelles.
 * @param[in]   peer Pair choisi.
 * @param[in]   sock Socket TCP.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int send_pers_data_to_peer(pers_data p_data, peer_data peer, int sock);
/**
 * @brief Gère la réponse de la part du pair suite au partage des données personnelles.
 * (`code = 50` ou `code = 51`).
 *
 * @param[in,out]   p_data Données personnelles.
 * @param[in]       sock Socket TCP.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int await_pers_data_sharing_res(pers_data *p_data, int sock);

/**
 * @brief Vérifie que l'ID du pair ne soit pas déjà utilisé. Si c'est le cas, un nouvel ID est
 * renvoyé.
 *
 * @param[in,out]   peers_data Données des pairs.
 * @param[in]       id ID reçu.
 *
 * @return `id` si l'ID est unique, sinon un nouvel ID en /!\ big-endian.
 */
int verify_id(dyn_array peers_data, uint16_t id);
/**
 * @brief Envoi du message contenant la validité de l'ID. (`code = 50` ou `code = 51`).
 *
 * @param[in]   sock Socket TCP.
 * @param[in]   id ID reçu ou le nouvel ID.
 * @param[in]   validity Validité de l'id (`0` ou `1`).
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int send_id_validity(int sock, int id, int validity);

/**************************************************************************************************
 *                       Méthode principale qui gère l'adhésion au système. *
 **************************************************************************************************/

int handle_linking(pers_data *p_data, dyn_array *peers_data, struct sockaddr_in6 linking_addr) {
    // b) 1. Envoi de la demande + attente de réponse.
    int attempts = 0;
    int res = 0;
    do {
        attempts++;
        printf("Trying to join the auction system... Attempt %d.\n", attempts);
        if (send_join_request(p_data->udp_sock, linking_addr) < 0) continue;
        res = await_join_req_response(p_data->tcp_serv_sock, peers_data);
    } while (attempts < 3 && res == 0);

    // Erreur.
    if (res < 0) return -1;

    // b) 2. Pas de réponse reçue.
    if (attempts == 3 && !res) return 0;

    // b) 4. Partage de l'adresse personnelle.
    if (handle_pers_data_sharing(p_data, *peers_data) < 0) return -1;

    return 1;
}

/**************************************************************************************************
 *                     Demande d'adhésion au système + attente d'une réponse.                     *
 **************************************************************************************************/

int send_join_request(int sock, struct sockaddr_in6 linking_addr) {
    message msg;
    memset(&msg, 0, sizeof(msg));
    msg.code = JOIN_REQ;

    dyn_array msg_buf;
    if (init_dyn_array(&msg_buf, sizeof(uint8_t)) < 0) return -1;
    if (concat_msg_to_buf(&msg_buf, &msg, CODE) < 0) goto error;

    if (sendto(sock, msg_buf.array, msg_buf.len, 0, (struct sockaddr *)&linking_addr,
               sizeof(linking_addr)) < 0) {
        perror("[ERROR] send");
        goto error;
    }

    free_dyn_array(msg_buf);
    return 0;

error:
    free_dyn_array(msg_buf);
    return -1;
}

int await_join_req_response(int sock, dyn_array *peers_data) {
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

            if (set_nonblock(peer) < 0) goto error;

            // Lecture du message.
            if (read_nb(peer, buf, msg_size, MILLI(5)) < 0) goto error;

            // Code correct.
            if (buf[0] == JOIN_REQ_RES) {
                got_res = 1;

                // Traduction buffer -> message.
                buf_to_msg(buf, &msg, CODE | ID | IP | PORT);

                // Ajout des données du pair dans `peers_data`.

                peer_data data = {.id = msg.id, .port = msg.port};
                memcpy(&data.ip, &msg.ip, sizeof(msg.ip));
                memset(data.cle, 0, KEY_SIZE);

                if (concat_to_array(peers_data, &data, sizeof(data)) < 0) goto error;

                // ===== DEBUG =====
                peer_data p = ((peer_data *)(peers_data->array))[0];
                char ip_str[INET6_ADDRSTRLEN];
                if (inet_ntop(AF_INET6, p.ip, ip_str, sizeof(ip_str)))
                    printf("[DEBUG] Response received!\n[DEBUG] From peer %d of ip %s %d.\n",
                           ntohs(p.id), ip_str, ntohs(p.port));
                // ===== DEBUG =====
            }

        error:
            close(peer);
        }
    }

    if (poll_ret < 0) {
        perror("[ERROR] poll");
        return -1;
    }

    return got_res;
}

/**************************************************************************************************
 *                                Partage des infos personnelles.                                 *
 **************************************************************************************************/

int handle_pers_data_sharing(pers_data *p_data, dyn_array peers_data) {
    int ret;
    peer_data peer;
    size_t n_peer = 0;
    // Socket TCP.
    int sock = socket(PF_INET6, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[ERROR] socket");
        return -1;
    }

    do {
        peer = ((peer_data *)peers_data.array)[n_peer++];
        printf("[DEBUG] Trying to share address and key to peer %d.\n", ntohs(peer.id));
    } while ((ret = send_pers_data_to_peer(*p_data, peer, sock)) < 0 &&
             n_peer < peers_data.len % sizeof(peer_data));

    if (ret == 0) ret = await_pers_data_sharing_res(p_data, sock);

    close(sock);
    return ret;
}

int send_pers_data_to_peer(pers_data p_data, peer_data peer, int sock) {
    message msg;
    memset(&msg, 0, sizeof(msg));
    msg.code = JOIN_SHARE_INFO;
    msg.id = p_data.id;
    memcpy(&msg.ip, &p_data.addr.sin6_addr, sizeof(msg.ip));
    msg.port = p_data.addr.sin6_port;
    memcpy(&msg.cle, &p_data.cle, sizeof(msg.cle));

    size_t msg_size =
        sizeof(msg.code) + sizeof(msg.id) + sizeof(msg.ip) + sizeof(msg.port) + sizeof(msg.cle);
    dyn_array msg_buf;
    if (init_dyn_array(&msg_buf, msg_size) < 0) return -1;
    if (concat_msg_to_buf(&msg_buf, &msg, CODE | ID | IP | PORT | CLE) < 0) goto error;

    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = peer.port;
    memcpy(&addr.sin6_addr.s6_addr, &peer.ip, sizeof(peer.ip));

    // ===== DEBUG =====
    char ip_str[INET6_ADDRSTRLEN];
    if (inet_ntop(AF_INET6, &addr.sin6_addr, ip_str, sizeof(ip_str)))
        printf("[DEBUG] Sharing info to peer %d of ip %s %d.\n", ntohs(peer.id), ip_str,
               ntohs(peer.port));
    // ===== DEBUG =====

    // Connexion via TCP sur l'adresse du pair.
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[ERROR] connect");
        goto error;
    }

    if (set_nonblock(sock) < 0) {
        goto error;
    }

    printf("[DEBUG] Sharing address and key...\n");

    // Envoi de la réponse.
    if (write_nb(sock, msg_buf.array, msg_buf.len, MILLI(5)) < 0) goto error;

    free_dyn_array(msg_buf);
    return 0;

error:
    free_dyn_array(msg_buf);
    return -1;
}

int await_pers_data_sharing_res(pers_data *p_data, int sock) {
    char buf[3];
    if (read_nb(sock, &buf, sizeof(buf), MILLI(5)) < 0) return -1;

    if (buf[0] == JOIN_INVALID_ID) {
        printf("[DEBUG] ID %d already used.\n", p_data->id);
        memcpy(&p_data->id, buf + 1, sizeof(p_data->id));
        printf("[DEBUG] New ID set: %d.\n", p_data->id);
    } else if (buf[0] != JOIN_VALID_ID) {
        printf("[DEBUG] Invalid ID response code received: %d.\n", buf[0]);
        return -1;
    }

    return 0;
}

/**************************************************************************************************
 *                               Réception de messages côté pair Q.                               *
 **************************************************************************************************/

void *handle_join_requests(void *params) {
    join_req_params p = *(join_req_params *)params;
    // Socket abonnée à l'adresse de liaison.
    int linking_subs_sock = make_multicast_socket(p.linking_addr);
    if (linking_subs_sock < 0) return NULL;

    char buf[1024];
    while (1) {
        memset(buf, 0, sizeof(buf));

        // Adresse de l'expéditeur.
        struct sockaddr_in6 addr;
        socklen_t addr_len = sizeof(addr);
        if (recvfrom(linking_subs_sock, buf, sizeof(buf), 0, (struct sockaddr *)&addr, &addr_len) <
            0) {
            perror("[ERROR] recvfrom");
            continue;
        }

        // Mauvais code.
        if (buf[0] != JOIN_REQ) continue;

        // ===== DEBUG =====
        char ip_str[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, &addr.sin6_addr, ip_str, sizeof(ip_str)))
            printf("[DEBUG] Join request received from: %s %d\n", ip_str, ntohs(addr.sin6_port));
        // ===== DEBUG =====

        // Réponse à l'expéditeur.
        message msg;
        memset(&msg, 0, sizeof(msg));
        msg.code = JOIN_REQ_RES;
        msg.id = p.p_data.id;
        memcpy(msg.ip, p.p_data.addr.sin6_addr.s6_addr, 16);
        msg.port = p.p_data.addr.sin6_port;

        dyn_array msg_buf;
        if (init_dyn_array(&msg_buf, sizeof(uint8_t)) < 0) continue;
        if (concat_msg_to_buf(&msg_buf, &msg, CODE | ID | IP | PORT) < 0) {
            free_dyn_array(msg_buf);
            continue;
        }

        // Socket TCP.
        int sock = socket(PF_INET6, SOCK_STREAM, 0);
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
        if (write_nb(sock, msg_buf.array, msg_buf.len, MILLI(5)) < 0) {
            perror("[ERROR] send");
            goto error;
        }

        printf("[DEBUG] Response sent.\n");

    error:
        if (sock) close(sock);
        free_dyn_array(msg_buf);
    }

    close(linking_subs_sock);
}

void *handle_data_sharing(void *params) {
    printf("[DEBUG] Handling data sharing...\n");

    data_sharing_params p = *(data_sharing_params *)params;
    while (1) {
        message msg;
        size_t msg_size =
            sizeof(msg.code) + sizeof(msg.id) + sizeof(msg.ip) + sizeof(msg.port) + sizeof(msg.cle);
        char buf[msg_size];
        memset(buf, 0, msg_size);

        // Accepte la connexion.
        int peer = accept(p.p_data.tcp_serv_sock, NULL, NULL);
        if (peer < 0) continue;

        printf("[DEBUG] A peer shared their data.\n");

        if (set_nonblock(peer) < 0) goto error;

        // Lecture du message.
        if (read_nb(peer, buf, msg_size, MILLI(5)) < 0) goto error;

        // Code correct.
        if (buf[0] == JOIN_SHARE_INFO) {
            // Traduction buffer -> message.
            buf_to_msg(buf, &msg, CODE | ID | IP | PORT | CLE);

            peer_data data = {.port = msg.port};
            memcpy(&data.ip, &msg.ip, sizeof(msg.ip));
            memcpy(&data.cle, &msg.cle, KEY_SIZE);

            // Vérification de l'ID.
            data.id = verify_id(*p.peers_data, msg.id);

            // Réponse à l'expéditeur :
            if (send_id_validity(peer, data.id, data.id == msg.id) == 0) {
                // Ajout des données du pair dans `peers_data`.
                if (concat_to_array(p.peers_data, &data, sizeof(data)) < 0) goto error;

                // ===== DEBUG =====
                peer_data peer =
                    ((peer_data *)(p.peers_data->array))[p.peers_data->len % sizeof(peer_data *)];
                char ip_str[INET6_ADDRSTRLEN];
                if (inet_ntop(AF_INET6, peer.ip, ip_str, sizeof(ip_str)))
                    printf("[DEBUG] Address shared! From peer %d of ip %s %d.\n", ntohs(peer.id),
                           ip_str, ntohs(peer.port));
                // ===== DEBUG =====
            }
        }

    error:
        close(peer);
    }
}

int verify_id(dyn_array peers_data, uint16_t id) {
    int exists;
    do {
        exists = 0;
        for (size_t i = 0; i < peers_data.len; i++) {
            if (((peer_data *)peers_data.array)[i].id == id) exists = 1;
        }
        if (exists) id = htons(rand() % UINT16_MAX);
    } while (exists);
    return id;
}

int send_id_validity(int sock, int id, int validity) {
    message msg;
    memset(&msg, 0, sizeof(msg));
    msg.code = validity ? JOIN_VALID_ID : JOIN_INVALID_ID;
    msg.id = htons(id);

    int flags = CODE | (validity ? 0 : ID);

    dyn_array msg_buf;
    if (init_dyn_array(&msg_buf, sizeof(uint8_t)) < 0) return -1;
    if (concat_msg_to_buf(&msg_buf, &msg, flags) < 0) goto error;

    if (send(sock, msg_buf.array, msg_buf.len, 0) < 0) {
        perror("[ERROR] send");
        goto error;
    }

    printf("[DEBUG] ID validity response sent!\n");

    free_dyn_array(msg_buf);
    return 0;

error:
    free_dyn_array(msg_buf);
    return -1;
}
