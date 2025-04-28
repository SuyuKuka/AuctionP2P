#include "linking.h"

#include <arpa/inet.h>
#include <poll.h>
#include <stdint.h>
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
 * @param[in,out]   peers Adresses et clés des pairs.
 *
 * @return `-1` en cas d'erreur, `0` s'il n'y a pas eu de réponse, `1` sinon.
 */
int await_join_req_response(int sock, linked_list *peers);

/**
 * @brief Gère le partage des données personnelles à un pair choisi.
 *
 * @param[in,out]   p_data Données personnelles.
 * @param[in]       peers Données des pairs.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int handle_pers_data_sharing(pers_data *p_data, linked_list peers);
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
 * @brief Gère l'initialisation de l'adresse d'enchères en recevant cette adresse de la part du pair
 * choisi précédemment, ainsi que la réception des infos des autres pairs du système. (`code = 7`).
 *
 * @param[in,out]   auction_addr Adresse d'enchères.
 * @param[in,out]   peers Données des pairs.
 * @param[in]       sock Socket serveur TCP.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int handle_peers_data_sharing(struct sockaddr_in6 *auction_addr, linked_list *peers, int sock);

/**
 * @brief Vérifie que l'ID du pair ne soit pas déjà utilisé. Si c'est le cas, un nouvel ID est
 * renvoyé.
 *
 * @param[in]   peers Données des pairs.
 * @param[in]   id ID reçu.
 *
 * @return `id` si l'ID est unique, sinon un nouvel ID en /!\ big-endian.
 */
int verify_id(linked_list peers, uint16_t id);
/**
 * @brief Envoi du message contenant la validité de l'ID. (`code = 50` ou `code = 51`).
 *
 * @param[in]   sock Socket TCP.
 * @param[in]   id ID reçu ou le nouvel ID en /!\ big-endian.
 * @param[in]   validity Validité de l'id (`0` ou `1`).
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int send_id_validity(int sock, int id, int validity);
/**
 * @brief Envoi du message contenant les infos du dernier pair demandant l'adhésion au système aux
 * autres pairs du système. (`code = 6`).
 *
 * @param[in]   peer Données du pair.
 * @param[in]   peers Liste contenant les infos des pairs.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int send_peer_data_to_all_peers(peer_data peer, linked_list *peers);
/**
 * @brief Envoi de l'en-tête, contenant l'adresse d'enchères, et des infos des autres pairs du
 * système au pair demandant l'adhésion au système. (`code = 7`).
 *
 * @param[in]   auction_ip IP de l'adresse d'enchères.
 * @param[in]   auction_port Port de l'adresse d'enchères.
 * @param[in]   peers Liste contenant les infos des pairs.
 * @param[in]   p_data Données personnelles (seulement l'ID est utilisé).
 * @param[in]   peer Données du pair.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int send_all_peers_data_to_peer(const uint8_t *auction_ip, uint16_t auction_port,
                                linked_list *peers, pers_data p_data, peer_data peer);

/**************************************************************************************************
 *                       Méthode principale qui gère l'adhésion au système.                       *
 **************************************************************************************************/

int handle_linking(pers_data *p_data, linked_list *peers, struct sockaddr_in6 linking_addr,
                   struct sockaddr_in6 *auction_addr) {
    // b) 1. Envoi de la demande + attente de réponse.
    int attempts = 0;
    int res = 0;
    do {
        attempts++;
        printf("Trying to join the auction system... Attempt %d.\n", attempts);
        if (send_join_request(p_data->udp_sock, linking_addr) < 0) continue;
        res = await_join_req_response(p_data->tcp_serv_sock, peers);
    } while (attempts < 3 && res == 0);

    printf("[DEBUG] === Initial response received ===\n");

    // Erreur.
    if (res < 0) return -1;

    // b) 2. Pas de réponse reçue.
    if (attempts == 3 && !res) return 0;

    printf("[DEBUG] === Starting sharing process ===\n");

    // b) 4. Partage de l'adresse personnelle.
    if (handle_pers_data_sharing(p_data, *peers) < 0 ||
        handle_peers_data_sharing(auction_addr, peers, p_data->tcp_serv_sock) < 0)
        return -1;

    return 1;
}

/**************************************************************************************************
 *                     Demande d'adhésion au système + attente d'une réponse.                     *
 **************************************************************************************************/

int send_join_request(int sock, struct sockaddr_in6 linking_addr) {
    message msg;
    memset(&msg, 0, sizeof(msg));
    msg.code = JOIN_REQ;

    return send_msg_to(msg, CODE, sock, (struct sockaddr *)&linking_addr, sizeof(linking_addr));
}

int await_join_req_response(int sock, linked_list *peers) {
    int msg_flags = CODE | ID | IP | PORT;
    size_t m_size = msg_size(msg_flags);
    char buf[m_size];
    int got_res = 0;

    int poll_ret;
    struct pollfd fds = {.fd = sock, .events = POLLIN};
    while ((poll_ret = poll(&fds, 1, MILLI(5)))) {
        // Demande de connexion reçue.
        if (fds.revents & POLLIN) {
            memset(buf, 0, m_size);

            // Accepte la connexion.
            int peer = accept(sock, NULL, NULL);
            if (peer < 0) {
                perror("[ERROR] accept");
                continue;
            }

            if (set_nonblock(peer) < 0) goto error;

            // Lecture du message.
            message msg;
            if (read_msg(peer, &msg, msg_flags, MILLI(5)) < 0) goto error;

            // Code correct.
            if (msg.code == JOIN_REQ_RES) {
                got_res = 1;

                peer_data peer = {.id = msg.id, .port = msg.port};
                memcpy(&peer.ip, &msg.ip, sizeof(peer.ip));
                memset(peer.cle, 0, KEY_SIZE);

                // Ajout des données du pair dans `peers`.
                node *peer_node = init_node();
                if (!peer_node) return -1;
                peer_node->data = &peer;
                append_to_list(peer_node, peers);

                // ===== DEBUG =====
                peer_data *d = get_last_node(peers)->data;
                char ip_str[INET6_ADDRSTRLEN];
                if (inet_ntop(AF_INET6, d->ip, ip_str, sizeof(ip_str)))
                    printf("[DEBUG] Response received from peer %d of ip %s %d.\n", ntohs(d->id),
                           ip_str, ntohs(d->port));
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

int handle_pers_data_sharing(pers_data *p_data, linked_list peers) {
    int ret = -1;
    int sock = socket(PF_INET6, SOCK_STREAM, 0);  // Socket TCP.
    if (sock < 0) {
        perror("[ERROR] socket");
        return -1;
    }

    for (node *n = peers.head; n != NULL; n = n->next) {
        peer_data *peer = n->data;
        printf("[DEBUG] Trying to share address and key to peer %d.\n", ntohs(peer->id));
        ret = send_pers_data_to_peer(*p_data, *peer, sock);
        if (ret == 0) break;
    }

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

    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = peer.port;
    memcpy(&addr.sin6_addr.s6_addr, &peer.ip, sizeof(addr.sin6_addr.s6_addr));

    // ===== DEBUG =====
    char ip_str[INET6_ADDRSTRLEN];
    if (inet_ntop(AF_INET6, &addr.sin6_addr, ip_str, sizeof(ip_str)))
        printf("[DEBUG] Sharing info to peer %d of ip %s %d.\n", ntohs(peer.id), ip_str,
               ntohs(peer.port));
    // ===== DEBUG =====

    // Connexion via TCP sur l'adresse du pair.
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[ERROR] connect");
        return -1;
    }

    if (set_nonblock(sock) < 0) return -1;

    printf("[DEBUG] Sharing address and key...\n");

    // Envoi de la réponse.
    if (write_msg(msg, CODE | ID | IP | PORT | CLE, sock, MILLI(5)) < 0) return -1;

    return 0;
}

int await_pers_data_sharing_res(pers_data *p_data, int sock) {
    char buf[3];
    if (read_nb(sock, &buf, sizeof(buf), MILLI(5)) < 0) return -1;

    if (buf[0] == JOIN_INVALID_ID) {
        printf("[DEBUG] ID %d already used.\n", ntohs(p_data->id));
        memcpy(&p_data->id, buf + 1, sizeof(p_data->id));
        printf("[DEBUG] New ID set: %d.\n", ntohs(p_data->id));
    } else if (buf[0] != JOIN_VALID_ID) {
        printf("[DEBUG] Invalid ID response code received: %d.\n", buf[0]);
        return -1;
    } else {
        printf("[DEBUG] Valid ID response code received: %d.\n", buf[0]);
    }

    return 0;
}

/**************************************************************************************************
 *                   Réception de l'adresse d'enchères + infos des autres pairs. *
 **************************************************************************************************/

int handle_peers_data_sharing(struct sockaddr_in6 *auction_addr, linked_list *peers, int sock) {
    printf("[DEBUG] Awaiting auction address and peers info sharing...\n");

    message msg;
    memset(&msg, 0, sizeof(msg));

    int header_flags = CODE | ID | IP | PORT | NB;
    int info_flags = ID | IP | PORT | CLE;

    // Accepte la connexion, lecture du message d'entête.
    int peer_sock = accept(sock, NULL, NULL);
    if (peer_sock < 0) {
        perror("[ERROR] accept");
        return -1;
    }
    if (set_nonblock(peer_sock) < 0 || read_msg(peer_sock, &msg, header_flags, MILLI(5)) < 0)
        goto error;

    if (msg.code != JOIN_SHARE_ALL_INFO) {
        fprintf(stderr, "[ERROR] Invalid message code received: %d\n", msg.code);
        goto error;
    }

    // Initialisation de l'adresse d'enchères.
    memset(auction_addr, 0, sizeof(*auction_addr));
    auction_addr->sin6_family = AF_INET6;
    memcpy(&auction_addr->sin6_addr.s6_addr, msg.ip, sizeof(auction_addr->sin6_addr.s6_addr));
    auction_addr->sin6_port = msg.port;
    auction_addr->sin6_scope_id = 0;

    // ===== DEBUG =====
    char ip_str[INET6_ADDRSTRLEN];
    if (inet_ntop(AF_INET6, msg.ip, ip_str, sizeof(ip_str)))
        printf("[DEBUG] Auction address received: %s %d\n", ip_str, ntohs(msg.port));
    // ===== DEBUG =====

    uint16_t nb = msg.nb;
    for (uint16_t i = 0; i < nb - 1; i++) {
        memset(&msg, 0, sizeof(msg));

        if (read_msg(peer_sock, &msg, info_flags, MILLI(5)) < 0) goto error;

        // ===== DEBUG =====
        if (inet_ntop(AF_INET6, msg.ip, ip_str, sizeof(ip_str)))
            printf("[DEBUG] Info received: peer %d of ip %s %d\n", ntohs(msg.id), ip_str,
                   ntohs(msg.port));
        // ===== DEBUG =====

        // Ajout dans le tableau.
        peer_data peer = {.id = msg.id, .port = msg.port};
        memcpy(peer.ip, msg.ip, sizeof(peer.ip));
        memcpy(peer.cle, msg.cle, sizeof(peer.cle));

        node *peer_node = init_node();
        if (!peer_node) goto error;
        peer_node->data = &peer;
        append_to_list(peer_node, peers);
    }

    close(peer_sock);
    return 0;

error:
    if (peer_sock) close(peer_sock);
    return -1;
}

/**************************************************************************************************
 *                               Réception de messages côté pair Q. *
 **************************************************************************************************/

int handle_join_request(struct sockaddr_in6 addr, pers_data p_data) {
    printf("[DEBUG] Join request received!\n");

    // Réponse à l'expéditeur.
    message msg;
    memset(&msg, 0, sizeof(msg));
    msg.code = JOIN_REQ_RES;
    msg.id = p_data.id;
    memcpy(msg.ip, p_data.addr.sin6_addr.s6_addr, sizeof(msg.ip));
    msg.port = p_data.addr.sin6_port;

    // Socket TCP.
    int sock = socket(PF_INET6, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[ERROR] socket");
        goto error;
    }

    // Connexion via TCP sur l'adresse de l'expéditeur.
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[ERROR] connect");
        goto error;
    }

    if (set_nonblock(sock) < 0) {
        goto error;
    }

    // Envoi de la réponse.
    if (write_msg(msg, CODE | ID | IP | PORT, sock, MILLI(5)) < 0) goto error;

    printf("[DEBUG] Response sent.\n");

    close(sock);
    return 0;

error:
    if (sock) close(sock);
    return -1;
}

int handle_info_sharing(int peer_sock, peer_data peer, pers_data p_data, linked_list *peers,
                        struct sockaddr_in6 auction_addr) {
    // Vérification de l'ID.
    int new_id = verify_id(*peers, peer.id);

    // Réponse à l'expéditeur :
    if (send_id_validity(peer_sock, peer.id, peer.id == new_id) < 0) return -1;

    // Ajout des données du pair dans `peers`.
    node *peer_node = init_node();
    if (!peer_node) return -1;
    peer_node->data = &peer;
    append_to_list(peer_node, peers);

    // ===== DEBUG =====
    peer_data *d = get_last_node(peers)->data;
    char ip_str[INET6_ADDRSTRLEN];
    if (inet_ntop(AF_INET6, d->ip, ip_str, sizeof(ip_str)))
        printf("[DEBUG] Peer %d shared his ip %s %d.\n", ntohs(d->id), ip_str, ntohs(d->port));
    // ===== DEBUG =====

    if (send_peer_data_to_all_peers(peer, peers) < 0) {
        fprintf(stderr, "[ERROR] Failed to share peer's info to all.\n");
        goto error;
    }

    if (send_all_peers_data_to_peer(auction_addr.sin6_addr.s6_addr, auction_addr.sin6_port, peers,
                                    p_data, peer) < 0) {
        fprintf(stderr, "[ERROR] Failed to share auction address and other peers info to peer.\n");
        goto error;
    }

    return 0;

error:
    remove_from_list(peer_node->id, peers);
    return -1;
}

int handle_info_sharing_to_all(peer_data peer, linked_list *peers) {
    // Ajout des données du pair dans `peers`.
    node *peer_node = init_node();
    if (!peer_node) return -1;
    peer_node->data = &peer;
    append_to_list(peer_node, peers);

    // ===== DEBUG =====
    peer_data *d = get_last_node(peers)->data;
    char ip_str[INET6_ADDRSTRLEN];
    if (inet_ntop(AF_INET6, d->ip, ip_str, sizeof(ip_str)))
        printf("[DEBUG] A peer shared data of peer %d of ip %s %d.\n", ntohs(d->id), ip_str,
               ntohs(d->port));
    // ===== DEBUG =====

    return 0;
}

int verify_id(linked_list peers, uint16_t id) {
    int exists;
    do {
        exists = 0;
        for (node *i = peers.head; i != NULL; i = i->next) {
            if (((peer_data *)i->data)->id == id) exists = 1;
        }
        if (exists) id = htons(rand() % UINT16_MAX);
    } while (exists);
    return id;
}

int send_id_validity(int sock, int id, int validity) {
    message msg;
    memset(&msg, 0, sizeof(msg));
    msg.code = validity ? JOIN_VALID_ID : JOIN_INVALID_ID;
    msg.id = id;

    if (write_msg(msg, CODE | ID, sock, MILLI(5)) < 0) return -1;

    printf("[DEBUG] ID validity response sent!\n");

    return 0;
}

int send_peer_data_to_all_peers(peer_data peer, linked_list *peers) {
    printf("[DEBUG] Sharing peer's info to all...\n");

    message msg;
    memset(&msg, 0, sizeof(msg));
    msg.code = JOIN_SHARE_INFO_TO_ALL;
    msg.id = peer.id;
    memcpy(msg.ip, peer.ip, sizeof(msg.ip));
    msg.port = peer.port;
    memcpy(msg.cle, peer.cle, sizeof(msg.cle));

    int msg_flags = CODE | ID | IP | PORT | CLE;
    size_t m_size = msg_size(msg_flags);
    dyn_array msg_buf;
    if (init_dyn_array(&msg_buf, m_size) < 0) return -1;
    if (concat_msg_to_buf(&msg_buf, &msg, msg_flags) < 0) goto error;

    for (node *n = peers->head; n != NULL; n = n->next) {
        const peer_data *other_peer = n->data;
        if (other_peer->id == peer.id) continue;

        // ===== DEBUG =====
        char ip_str[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, other_peer->ip, ip_str, sizeof(ip_str)))
            printf("[DEBUG] ================= Peer %d, %s %d.\n", ntohs(other_peer->id), ip_str,
                   ntohs(other_peer->port));
        // ===== DEBUG =====

        struct sockaddr_in6 addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        memcpy(&addr.sin6_addr.s6_addr, other_peer->ip, sizeof(addr.sin6_addr.s6_addr));
        addr.sin6_port = other_peer->port;

        // Socket TCP.
        int sock = socket(PF_INET6, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("[ERROR] socket");
            goto loop_err;
        }

        // Connexion via TCP sur l'adresse du pair.
        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("[ERROR] connect");
            goto loop_err;
        }

        if (set_nonblock(sock) < 0) {
            goto loop_err;
        }

        // Envoi de la réponse.
        if (write_nb(sock, msg_buf.array, msg_buf.len, MILLI(5)) < 0) goto loop_err;

        printf("[DEBUG] Shared peer's info to another peer.\n");

    loop_err:
        close(sock);
    }

    free_dyn_array(msg_buf);
    return 0;

error:
    free_dyn_array(msg_buf);
    return -1;
}

int send_all_peers_data_to_peer(const uint8_t *auction_ip, uint16_t auction_port,
                                linked_list *peers, pers_data p_data, peer_data peer) {
    printf("[DEBUG] Sharing auction address and other peers info to peer...\n");

    message msg;
    memset(&msg, 0, sizeof(msg));
    msg.code = JOIN_SHARE_ALL_INFO;
    msg.id = p_data.id;
    memcpy(msg.ip, auction_ip, sizeof(msg.ip));
    msg.port = auction_port;
    msg.nb = peers->size;

    int msg_flags = CODE | ID | IP | PORT | NB;
    size_t m_size = msg_size(msg_flags);
    dyn_array msg_buf;
    if (init_dyn_array(&msg_buf, m_size) < 0) return -1;
    if (concat_msg_to_buf(&msg_buf, &msg, msg_flags) < 0) return -1;

    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    memcpy(&addr.sin6_addr.s6_addr, peer.ip, sizeof(addr.sin6_addr.s6_addr));
    addr.sin6_port = peer.port;

    // Socket TCP.
    int sock = socket(PF_INET6, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[ERROR] socket");
        goto error;
    }

    // Connexion via TCP sur l'adresse du pair.
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[ERROR] connect");
        goto error;
    }

    if (set_nonblock(sock) < 0) {
        goto error;
    }

    // Envoi de l'en-tête.
    if (write_nb(sock, msg_buf.array, msg_buf.len, MILLI(5)) < 0) goto error;

    printf("[DEBUG] Shared auction address to peer.\n");

    // Envoi des infos des autres pairs.
    printf("[DEBUG] Sharing %zu other peers info to peer...\n", peers->size - 1);
    for (node *n = peers->head; n != NULL; n = n->next) {
        const peer_data *other_peer = n->data;
        if (other_peer->id == peer.id) continue;

        // On vide le message et le buffer.
        memset(&msg, 0, sizeof(msg));
        msg_buf.len = 0;

        msg.id = other_peer->id;
        memcpy(msg.ip, other_peer->ip, sizeof(msg.ip));
        msg.port = other_peer->port;
        memcpy(msg.cle, other_peer->cle, sizeof(msg.cle));

        if (concat_msg_to_buf(&msg_buf, &msg, ID | IP | PORT | CLE) < 0 ||
            write_nb(sock, msg_buf.array, msg_buf.len, MILLI(5)) < 0)
            goto error;

        // ===== DEBUG =====
        char ip_str[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, msg.ip, ip_str, sizeof(ip_str)))
            printf("[DEBUG] Shared peer %d's (%s %d) info to peer.\n", ntohs(msg.id), ip_str,
                   ntohs(msg.port));
        // ===== DEBUG =====
    }

    close(sock);
    free_dyn_array(msg_buf);
    return 0;

error:
    if (sock) close(sock);
    free_dyn_array(msg_buf);
    return -1;
}
