#include "consensus.h"

#include <poll.h>

#include "netio.h"
#include "protocol.h"

int supervisor_begin_consensus(pers_data p_data, struct sockaddr_in6 mult_addr, linked_list *peers,
                               message *mess, int flags) {
    // Buffer pour le champ mess
    dyn_array msg_buf;
    if (init_dyn_array(&msg_buf, msg_size(flags)) < 0) return -1;
    if (concat_msg_to_buf(&msg_buf, mess, flags) < 0) goto error;

    // Liste des pairs vérifiés
    linked_list checked_list;
    memset(&checked_list, 0, sizeof(checked_list));

    int poll_ret;
    int msg_flags = CODE | ID | LMESS | MESS | LSIG | SIG;
    struct pollfd fd = {.fd = p_data.udp_serv_sock, .events = POLLIN};
    // Le superviseur attend les messages de validations
    printf("[DEBUG] CONSENSUS: Waiting for %zu validation(s)...\n", peers->size);
    while ((poll_ret = poll(&fd, 1, MILLI(5))) > 0) {
        if (fd.revents & POLLIN) {
            message msg;
            memset(&msg, 0, sizeof(msg));
            if (recv_msg_from(&msg, msg_flags, 1024, p_data.udp_serv_sock, NULL, NULL) < 0) {
                fprintf(stderr, "[ERROR] recvfrom\n");
                continue;
            }
            if (msg.code != CONS_VAL) continue;

            printf("[DEBUG] CONSENSUS: Validation message received, verifying signature...\n");

            // Vérification de la signature
            uint16_t peer_id = ntohs(msg.id);
            const node *peer;
            if ((peer = get_node(peer_id, peers)) == NULL) {
                printf("[ERROR] CONSENSUS: Unknown peer id: %d\n", peer_id);
                continue;
            }
            if (verify_signature(msg_buf.array, msg.sig, ((peer_data *)peer->data)->cle) < 0)
                continue;

            // Ajout dans la liste des pairs vérifiés
            node *n = init_node();
            if (!n) {
                printf("[ERROR] CONSENSUS: Failed to add peer to checked list.\n");
                continue;
            }
            append_to_list(n, peer_id, &checked_list);

            printf("[DEBUG] CONSENSUS: Peer %d message validated!\n", peer_id);
        }
    }

    // Vérifie s'il manque des pairs
    for (node *n = peers->head; n != NULL; n = n->next) {
        if (!get_node(n->id, &checked_list)) goto error;
    }

    if (poll_ret < 0) {
        perror("[ERROR] poll");
        goto error;
    } else if (poll_ret == 0 && peers->size != checked_list.size) {
        printf("[DEBUG] CONSENSUS: Supervisor timed out\n");
        goto error;
    }

    // Toutes les validations sont bien arrivées
    printf("[DEBUG] CONSENSUS: All %zu validation(s) received successfully.\n", peers->size);

    // Message de validation.
    int val_flags = CODE | ID | LMESS | MESS | LSIG | SIG | NB;
    message validation_msg;
    memset(&validation_msg, 0, sizeof(validation_msg));
    validation_msg.code = CONS_CONF;
    validation_msg.id = htons(p_data.id);
    validation_msg.lmess = msg_buf.len;
    memcpy(validation_msg.mess, msg_buf.array, validation_msg.lmess);
    validation_msg.lsig = 0;
    memcpy(validation_msg.sig, "", validation_msg.lsig);
    validation_msg.nb = peers->size > 2 ? htons(3) : htons(peers->size);

    free_dyn_array(msg_buf);
    if (init_dyn_array(&msg_buf, msg_size(flags)) < 0) return -1;
    if (concat_msg_to_buf(&msg_buf, &validation_msg, val_flags) < 0) goto error;

    node *n = peers->head;
    int count = 0;
    while (count < 3 && n != NULL) {
        message sig_msg;
        memset(&sig_msg, 0, sizeof(sig_msg));
        sig_msg.id = htons(n->id);
        sig_msg.lsig = 0;
        memcpy(sig_msg.sig, "", sig_msg.lsig);
        if (concat_msg_to_buf(&msg_buf, &sig_msg, ID | LSIG | SIG) < 0) goto error;
        count++;
        n = n->next;
    }

    // Multidiffuse le message avec le code 2
    if (sendto(p_data.udp_sock, msg_buf.array, msg_buf.len, 0, (struct sockaddr *)(&mult_addr),
               sizeof(mult_addr)) < 0) {
        perror("[ERROR] sendto");
        goto error;
    }

    if (peers->size > 3) {
        // Multidiffusion des signatures restantes
        int follow_flags = CODE | ID | NB;
        message following_msg;
        memset(&following_msg, 0, sizeof(following_msg));
        following_msg.code = CONS_CONF_FOLLOW;
        following_msg.id = htons(p_data.id);
        validation_msg.nb = htons(peers->size - 3);

        free_dyn_array(msg_buf);
        if (init_dyn_array(&msg_buf, msg_size(follow_flags)) < 0) return -1;
        if (concat_msg_to_buf(&msg_buf, &following_msg, follow_flags) < 0) goto error;

        while (n != NULL) {
            message sig_msg;
            memset(&sig_msg, 0, sizeof(sig_msg));
            sig_msg.id = htons(n->id);
            sig_msg.lsig = 0;
            memcpy(sig_msg.sig, "", sig_msg.lsig);
            if (concat_msg_to_buf(&msg_buf, &sig_msg, ID | LSIG | SIG) < 0) goto error;
            count++;
            n = n->next;
        }

        // Multidiffuse le message avec le code 20
        if (sendto(p_data.udp_sock, msg_buf.array, msg_buf.len, 0, (struct sockaddr *)(&mult_addr),
                   sizeof(mult_addr)) < 0) {
            perror("[ERROR] sendto");
            goto error;
        }
    }

    // Toutes les validations sont bien arrivées
    printf("[DEBUG] CONSENSUS: Confirmation message sent!\n");

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
                                        int mult_sock, uint16_t supervisor_id) {
    // Buffer pour le champ mess
    dyn_array msg_buf;
    if (init_dyn_array(&msg_buf, msg_size(flags)) < 0) return -1;
    if (concat_msg_to_buf(&msg_buf, msg, flags) < 0) goto error;

    // Message de validation.
    message validation_msg;
    memset(&validation_msg, 0, sizeof(validation_msg));
    validation_msg.code = CONS_VAL;
    validation_msg.id = htons(p_data.id);
    validation_msg.lmess = msg_buf.len;
    memcpy(validation_msg.mess, msg_buf.array, validation_msg.lmess);
    validation_msg.lsig = 0;
    memcpy(validation_msg.sig, "", validation_msg.lsig);

    // Envoi du message via UDP
    if (send_msg_to(validation_msg, CODE | ID | LMESS | MESS | LSIG | SIG, p_data.udp_sock,
                    (struct sockaddr *)&supervisor_addr, sizeof(struct sockaddr_in6)) < 0) {
        perror("[ERROR] send_msg_to");
        goto error;
    }

    printf("[DEBUG] CONSENSUS: Sent validation message to supervisor, awaiting response...\n");

    char buf[1024];
    char following_buf[1024];
    message confirmation_msg;
    message following_msg;
    message sig_msg;
    memset(buf, 0, sizeof(buf));
    memset(following_buf, 0, sizeof(following_buf));
    memset(&confirmation_msg, 0, sizeof(confirmation_msg));
    memset(&following_msg, 0, sizeof(following_msg));
    memset(&sig_msg, 0, sizeof(sig_msg));

    // Recevoir la confirmation du superviseur
    int got_res = 0;
    int poll_ret;
    struct pollfd fd = {.fd = mult_sock, .events = POLLIN};
    while ((poll_ret = poll(&fd, 1, MILLI(10))) > 0) {
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

    buf_to_msg(buf, &confirmation_msg, CODE | ID | LMESS | MESS | LSIG | SIG | NB);
    size_t header_size =
        msg_size(CODE | ID | LMESS | LSIG | NB) + confirmation_msg.lmess + confirmation_msg.lsig;
    size_t following_header_size = msg_size(CODE | ID | NB);
    size_t sig_msg_offset = 0;
    uint16_t confirmation_nb = ntohs(confirmation_msg.nb);

    printf("[DEBUG] CONSENSUS: Supervisor confirmation received, validating signatures...\n");
    // Vérification de la signature de tous les pairs
    if (confirmation_msg.code == CONS_CONF && ntohs(confirmation_msg.id) == supervisor_id) {
        uint16_t following_nb = 0;
        if (confirmation_nb > 3) {
            if (recvfrom(mult_sock, following_buf, sizeof(following_buf), 0, NULL, NULL) < 0) {
                perror("[ERROR] recvfrom");
                goto error;
            }
            if (following_buf[0] == CONS_CONF_FOLLOW &&
                ntohs(confirmation_msg.id) == supervisor_id) {
                buf_to_msg(following_buf, &following_msg, CODE | ID | NB);
                following_nb = ntohs(following_msg.nb);
            }
        }

        for (size_t i = 0; i < confirmation_nb + following_nb; i++) {
            if (i < 3) {
                buf_to_msg(buf + header_size + sig_msg_offset, &sig_msg, ID | LSIG | SIG);
                printf("[DEBUG] CONSENSUS: Checking signature #%zu of peer %d...\n", i + 1,
                       ntohs(sig_msg.id));
            } else {
                if (i == 3) sig_msg_offset = 0;
                printf(
                    "[DEBUG] CONSENSUS: Confirmation message split, checking signature #%zu...\n",
                    i + 1);
                buf_to_msg(following_buf + following_header_size + sig_msg_offset, &sig_msg,
                           ID | LSIG | SIG);
            }
            printf("OFFSET: %zu\n", sig_msg_offset);
            sig_msg_offset += msg_size(ID | LSIG) + sig_msg.lsig;

            const node *peer;
            uint16_t sig_id = ntohs(sig_msg.id);
            if (sig_id != p_data.id) {
                if ((peer = get_node(sig_id, &peers)) == NULL) {
                    printf("[ERROR] CONSENSUS: Unknown peer id: %d\n", sig_id);
                    goto error;
                }
                if (verify_signature(msg_buf.array, sig_msg.sig, ((peer_data *)peer->data)->cle) <
                    0)
                    goto error;
            }
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
