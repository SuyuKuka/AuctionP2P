#include "concensus.h"
#include "protocol.h"
#include "netio.h"

#include "concensus.h"
#include "protocol.h"
#include "netio.h"

int send_validation_to_supervisor(int sock, struct sockaddr_in6 addr, message msg) {
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

    // Envoi du message via UDP
    if (send_msg_to(validation_msg, CODE | ID | NUMV | PRIX | LMESS | MESS | LSIG | SIG, sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[ERROR] send_msg_to");
        close(sock);
        return -1;
    }

    printf("[DEBUG] Sent validation message to peer %d\n", validation_msg.id);
    return 0;
}

// gere qu'un seul auction, l'adapté pour qu'il puisse en faire plusieurs
// sock udp pour multidiffuser
int supervisor_wait_for_validation(int sock, struct sockaddr_in6 addr ,int total_validation_msg, message mess) {
    int count = 0;
    message msg;
    printf("[INFO] Waiting for %d validation(s)...\n", total_validation_msg);
    while (count < total_validation_msg) {
        // Réinitialisation du message une fois qu'on l'a reçue.
        memset(&msg, 0, sizeof(msg));

        // Attente du message de validation.
        if (recv_msg_from(&msg, CODE | ID | NUMV | PRIX | LMESS | MESS | LSIG | SIG, sock_supervisor, 
            (struct sockaddr *)&addr, &addrlen) < 0) {
            perror("[ERROR] recv_msg_from");
            continue;
        }

        // Vérification de la signature du message. Si la vérification echoue, on ignore la validation.
        if (verify_signature(msg, "a") != 0) {
            printf("[WARN] Invalid signature from peer ID %d. Skipping validation.\n", msg.id);
            continue;
        }

        // Verification du message de validation.
        if (msg.code == 1 && msg.numv == mess.numv) {
            printf("[DEBUG] Received validation from peer ID %d\n", msg.id);
            count++;
        } else {
            printf("[WARN] Wrong message code: code=%d, numv=%d\n", msg.code, msg.numv);
        }
    }

    // Tous les messages de validation ont été reçus.
    printf("[INFO] All %d validation(s) received successfully.\n", total_validation_msg);

    message validation_msg;
    memset(&validation_msg, 0, sizeof(validation_msg));
    validation_msg.code = 2;
    validation_msg.id = msg.id; // l'id du superviseur, pas le pair P
    validation_msg.numv = msg.numv;
    validation_msg.prix = msg.prix;
    validation_msg.lmess = msg.lmess;
    validation_msg.mess = msg.mess;
    validation_msg.lsig = 1;
    validation_msg.sig = "superviseur";
    validation_msg.nb = count;

    if (send_msg_to(validation_msg, CODE | ID | NUMV | PRIX | LMESS | MESS | LSIG | SIG | NB, sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[ERROR] send_msg_to");
        close(sock);
        return -1;
    }
    return 0;
}

int verify_signature(message msg, char *signature) {
   if (strcmp(msg.sig, signature) == 0) {
        return 0;
    } else {
        return -1;
    }
}