#include "consensus.h"

#include <poll.h>

#include "netio.h"
#include "protocol.h"

int supervisor_begin_consensus(pers_data p_data, struct sockaddr_in6 mult_addr, linked_list peers,
                               message *mess, int flags) {
    // Buffer pour le champ mess
    dyn_array msg_buf;
    if (init_dyn_array(&msg_buf, msg_size(flags)) < 0) return -1;
    if (concat_msg_to_buf(&msg_buf, mess, flags) < 0) return -1;

    // Liste des pairs vérifiés
    linked_list checked_list;
    memset(&checked_list, 0, sizeof(checked_list));

    int poll_ret;
    int msg_flags = CODE | ID | LMESS | MESS | LSIG | SIG;
    struct pollfd fd = {.fd = p_data.udp_serv_sock, .events = POLLIN};
    // Le superviseur attend les messages de validations
    printf("[DEBUG] CONSENSUS: Waiting for %zu validation(s)...\n", peers.size);
    while ((poll_ret = poll(&fd, 1, MILLI(5))) > 0) {
        if (fd.revents & POLLIN) {
            message msg;
            if (recv_msg_from(&msg, msg_flags, 1024, p_data.udp_serv_sock, NULL, NULL) < 0) {
                fprintf(stderr, "[ERROR] recvfrom\n");
                continue;
            }
            printf("[DEBUG] CONSENSUS: Message received, verifying code...\n");
            if (msg.code != CONS_VAL) continue;
            uint16_t peer_id = ntohs(msg.id);

            printf("[DEBUG] CONSENSUS: Validation message received, verifying signature...\n");

            // Vérification de la signature
            const node *peer;
            if ((peer = get_node(peer_id, &peers)) == NULL) continue;
            if (verify_signature(msg_buf.array, msg.sig, ((peer_data *)peer->data)->cle) < 0)
                continue;

            // Ajout dans la liste des pairs vérifiés
            node *n = init_node();
            if (!n) continue;
            append_to_list(n, peer_id, &checked_list);

            printf("[DEBUG] CONSENSUS: Peer %d message validated!\n", peer_id);
        }
    }

    // Vérifie s'il manque des pairs
    for (node *n = peers.head; n != NULL; n = n->next) {
        if (!get_node(n->id, &checked_list)) goto error;
    }

    if (poll_ret < 0) {
        perror("[ERROR] poll");
        goto error;
    } else if (poll_ret == 0 && peers.size != checked_list.size) {
        printf("[DEBUG] CONSENSUS: Supervisor timed out\n");
        goto error;
    }

    // Message de validation.
    message validation_msg;
    memset(&validation_msg, 0, sizeof(validation_msg));
    validation_msg.code = CONS_CONF;
    validation_msg.id = p_data.id;
    validation_msg.lmess = msg_buf.len;
    validation_msg.mess = msg_buf.array;
    validation_msg.lsig = 0;
    validation_msg.sig = "";

    // Multidiffuse le message avec le code 2
    if (send_msg_to(validation_msg, CODE | ID | LMESS | MESS | LSIG | SIG, p_data.udp_sock,
                    (struct sockaddr *)(&mult_addr), sizeof(mult_addr)) < 0) {
        fprintf(stderr, "[ERROR] send_msg_to");
        goto error;
    }

    // Toutes les validations sont bien arrivées
    printf("[DEBUG] CONSENSUS: All %zu validation(s) received successfully.\n", peers.size);

    free_dyn_array(msg_buf);
    free_list(&checked_list);
    return 0;

error:
    free_dyn_array(msg_buf);
    free_list(&checked_list);
    return -1;
}

int send_await_validation_to_supervisor(struct sockaddr_in6 supervisor_addr, message *msg,
                                        int flags, pers_data p_data, linked_list peers,
                                        int mult_sock) {
    // Buffer pour le champ mess
    dyn_array msg_buf;
    if (init_dyn_array(&msg_buf, msg_size(flags)) < 0) return -1;
    if (concat_msg_to_buf(&msg_buf, msg, flags) < 0) goto error;

    // Message de validation.
    message validation_msg;
    memset(&validation_msg, 0, sizeof(validation_msg));
    validation_msg.code = CONS_VAL;
    validation_msg.id = p_data.id;
    validation_msg.lmess = msg_buf.len;
    validation_msg.mess = msg_buf.array;
    validation_msg.lsig = 0;
    validation_msg.sig = "";

    // Envoi du message via UDP
    if (send_msg_to(validation_msg, CODE | ID | LMESS | MESS | LSIG | SIG, p_data.udp_sock,
                    (struct sockaddr *)&supervisor_addr, sizeof(struct sockaddr_in6)) < 0) {
        perror("[ERROR] send_msg_to");
        goto error;
    }

    printf("[DEBUG] CONSENSUS: Sent validation message to supervisor, awaiting response...\n");

    char buf[1024];
    char sig_buf[1024];
    memset(buf, 0, sizeof(buf));
    memset(sig_buf, 0, sizeof(sig_buf));
    message msg_confirmation;
    message peer_sig;

    // Recevoir la confirmation du superviseur
    int got_res = 0;
    int poll_ret;
    struct pollfd fd = {.fd = mult_sock, .events = POLLIN};
    while ((poll_ret = poll(&fd, 1, MILLI(5))) > 0) {
        if (fd.revents & POLLIN) {
            if (recvfrom(mult_sock, buf, sizeof(buf), 0, NULL, NULL) < 0) {
                perror("[ERROR] recvfrom");
                goto error;
            }
            got_res = 1;
        }
    }

    if (poll_ret < 0 || !got_res) {
        if (poll_ret < 0)
            perror("[ERROR] poll");
        else
            printf("[DEBUG] CONSENSUS: Peer timed out\n");
        goto error;
    }

    buf_to_msg(buf, &msg_confirmation, CODE | ID | LMESS | MESS | LSIG | SIG | NB);
    size_t header_size = msg_size(CODE | ID | LMESS | MESS | LSIG | SIG | NB);
    size_t sig_header_size = msg_size(CODE | ID | NB);
    size_t sig_size = msg_size(ID | LSIG | SIG);

    // Vérification de la signature de tous les pairs
    if (msg_confirmation.code == CONS_CONF) {
        if (msg_confirmation.nb > 3) {
            if (recvfrom(mult_sock, sig_buf, sizeof(sig_buf), 0, NULL, NULL) < 0) {
                perror("[ERROR] recvfrom");
                goto error;
            }
            buf_to_msg(sig_buf, &peer_sig, CODE | ID | NB);
        }

        for (size_t i = 0; i < msg_confirmation.nb; i++) {
            if (i < 3) {
                buf_to_msg(buf + header_size + sig_size * i, &peer_sig, ID | LSIG | SIG);
            } else {
                buf_to_msg(sig_buf + sig_header_size + sig_size * i, &peer_sig, ID | LSIG | SIG);
            }
            const node *peer;
            if ((peer = get_node(peer_sig.id, &peers)) == NULL) goto error;
            if (verify_signature(msg_buf.array, peer_sig.sig, ((peer_data *)peer->data)->cle) < 0)
                goto error;
        }

        free_dyn_array(msg_buf);
        return 0;
    }

error:
    free_dyn_array(msg_buf);
    return -1;
}

int verify_signature(const char *mess, const char *signature, const char *key) {
    // TODO: Check signature
    (void)mess;
    (void)signature;
    (void)key;
    return 0;
}
