#include "concensus.h"
#include "protocol.h"
#include "netio.h"

#include "concensus.h"
#include "protocol.h"
#include "netio.h"

// gere qu'un seul auction, l'adapté pour qu'il puisse en faire plusieurs
int supervisor_wait_for_validation(int sock, struct sockaddr *addr, size_t nb_peers, message mess, uint16_t supervisor_id) {
    size_t count = 0;
    message msg = malloc(sizeof(message));
    printf("[INFO] Waiting for %zu validation(s)...\n", nb_peers);

    struct sockaddr_in6 src_addr;
    socklen_t src_len = sizeof(src_addr);

    while (count < nb_peers) {
        memset(&msg, 0, sizeof(msg));

        // Le superviseur attend les messages de validations
        if (recv_msg_from(&msg, CODE | ID | NUMV | PRIX | LMESS | MESS | LSIG | SIG, sock,
                          (struct sockaddr *)&src_addr, &src_len) < 0) {
            perror("[ERROR] recv_msg_from");
            continue;
        }

        printf("[DEBUG] Received message from peer ID %d\n", ntohs(msg.id));

        // Si c’est une validation pour la bonne enchère
        if (msg.code == 1 && msg.numv == mess.numv) {
            printf("[DEBUG] Received validation from peer ID %d\n", ntohs(msg.id));
            count++;
        }

        // Si c’est une validation pour une autre enchère, on l’ignore proprement
        else if (msg.code == 1 && msg.numv != mess.numv) {
            printf("[INFO] Ignored validation for another auction (numv=%d)\n", ntohl(msg.numv));
        }

        // Sinon, c’est une erreur ou un message inattendu
        else {
            printf("[WARN] Wrong message received: code=%d, numv=%d\n", msg.code, ntohl(msg.numv));
        }
    }

    // Toutes les validations sont bien arrivé
    printf("[INFO] All %zu validation(s) received successfully.\n", nb_peers);

    // Créer le message de validation.
    char sig_buf[] = "a";
    message validation_msg;
    memset(&validation_msg, 0, sizeof(validation_msg));
    validation_msg.code = 2;
    validation_msg.id = htons(supervisor_id);
    validation_msg.numv = mess.numv;
    validation_msg.prix = mess.prix;
    validation_msg.lmess = mess.lmess;
    validation_msg.mess = mess.mess;
    validation_msg.lsig = 1;
    validation_msg.sig = sig_buf;
    validation_msg.nb = count;

    int flags = CODE | ID | NUMV | PRIX | LMESS | MESS | LSIG | SIG | NB;

    // Multidiffuse le message avec le code 2
    if (send_msg_to(validation_msg, flags, sock, addr, sizeof(struct sockaddr_in6)) < 0) {
        perror("[ERROR] send_msg_to");
        return -1;
    }

    return 0;
}

int send_validation_to_supervisor(int sock, struct sockaddr *addr, message msg) {
    // Créer le message de validation.
    message validation_msg;
    memset(&validation_msg, 0, sizeof(validation_msg));
    validation_msg.code = 1;
    validation_msg.id = msg.id;
    validation_msg.numv = msg.numv;
    validation_msg.prix = msg.prix;
    validation_msg.lmess = msg.lmess;
    validation_msg.mess = msg.mess;
    validation_msg.lsig = 1;
    validation_msg.sig = "a";

    int flags = CODE | ID | NUMV | PRIX | LMESS | MESS | LSIG | SIG;

    // Envoi du message via UDP
    if (send_msg_to(validation_msg, flags, sock, addr, sizeof(struct sockaddr_in6)) < 0) {
        perror("[ERROR] send_msg_to");
        return -1;
    }

    printf("[DEBUG] Sent validation message to peer %d\n", ntohs(validation_msg.id));
    return 0;
}

int verify_signature(message msg, char *signature) {
   if (strcmp(msg.sig, signature) == 0) {
        return 0;
    } else {
        return -1;
    }
}
