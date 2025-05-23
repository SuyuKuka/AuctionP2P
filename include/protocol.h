#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <netinet/in.h>

#include "collections.h"

#define KEY_SIZE 113

/* Flags pour identifier les champs utilisés dans un message. */
enum Flags {
    CODE = 1,
    ID = 1 << 1,
    LMESS = 1 << 2,
    LSIG = 1 << 3,
    MESS = 1 << 4,
    SIG = 1 << 5,
    IP = 1 << 6,
    PORT = 1 << 7,
    CLE = 1 << 8,
    NUMV = 1 << 9,
    PRIX = 1 << 10,
    NB = 1 << 11,
    ALL = CODE | ID | LMESS | LSIG | MESS | SIG | IP | PORT | CLE | NUMV | PRIX | NB
};

enum Codes {
    CONS_VAL = 1,               /* Consensus : message de validation. */
    CONS_CONF = 2,              /* Consensus : confirmation de la réception de tous les msg de
                                   validation. */
    CONS_CONF_FOLLOW = 20,      /* Consensus: suite du message de confirmation. */
    JOIN_REQ = 3,               /* Join system : demande pour rejoindre le système.  */
    JOIN_REQ_RES = 4,           /* Join system : réponse à la demande initiale. */
    JOIN_SHARE_INFO = 5,        /* Join system : partage de l'identité du demandeur à un pair. */
    JOIN_VALID_ID = 50,         /* Join system : identifiant valide. */
    JOIN_INVALID_ID = 51,       /* Join system : identifiant existant. */
    JOIN_SHARE_INFO_TO_ALL = 6, /* Join system : partage de l'identité du demandeur aux autres. */
    JOIN_SHARE_ALL_INFO = 7,    /* Join system : partage de l'identité de tous au demandeur. */
    AUC_INIT = 8,               /* Auction : initialisation d'une vente. */
    AUC_BID = 9,                /* Auction : enchérir sur une vente. */
    AUC_SUP_REDIFF = 10,        /* Auction : le superviseur rediffuse une enchère. */
    AUC_NOTICE = 11,            /* Auction : première notification de fin. */
    AUC_END = 12,               /* Auction : fin de la vente. */
    AUC_LEAVE = 13,             /* Auction : quitter le système. */
    AUC_CONCUR = 14,            /* Auction : concurrence entre deux enchères. */
    AUC_REJECT = 15,            /* Auction : enchère rejetée car invalide. */
    MISSING_SUP = 16,           /* Superviseur absent. */
    MISSING_SUP_CANCEL = 17,    /* Annulation de la notification d'absence. */
    MISSING_PEER = 18,          /* Pair absent. */
};

/* Structure pour un message quelconque.
   /!\ Ne pas oublier de convertir les nombres en big-endian. /!\ */
typedef struct message {
    uint8_t code;       /* Type. */
    uint16_t id;        /* Identifiant. */
    uint8_t lmess;      /* Longueur du message. */
    uint8_t lsig;       /* Longueur de la signature. */
    char mess[512];     /* Message. */
    char sig[512];      /* Signature. */
    uint8_t ip[16];     /* Adresse IP. */
    uint16_t port;      /* Port. */
    char cle[KEY_SIZE]; /* Clé. */
    uint32_t numv;      /* Num vente. */
    uint32_t prix;      /* Prix vente. */
    uint16_t nb;        /* Nombre d'un type de message concaténé à la suite. */
} message;

/* Structure contenant l'ID, l'adresse et la clé d'un pair.
   /!\ ID, IP et port en big-endian. /!\ */
typedef struct peer_data {
    uint16_t id;        /* Identifiant. */
    uint8_t ip[16];     /* Adresse IP. */
    uint16_t port;      /* Port. */
    char cle[KEY_SIZE]; /* Clé. */
} peer_data;

/* Structure pour stocker toutes les données personnelles. */
typedef struct pers_data {
    struct sockaddr_in6 addr;
    int udp_sock;       /* Socket UDP pour les envois. */
    int udp_serv_sock;  /* Socket UDP pour la réception unicast. */
    int tcp_serv_sock;  /* Socket serveur TCP. */
    uint16_t id;        /* Identifiant. */
    char cle[KEY_SIZE]; /* Clé. */
} pers_data;

/**
 * @brief Ajoute les données contenues dans `msg` spécifiées par les `flags` à
 * la suite du tableau.
 *
 * Exemple :
 * Si `flags = CODE | ID`, alors les champs `msg->code` et `msg->id` seront
 * concaténé à `arr`.
 *
 * La méthode réalloue le tableau si la taille des données est plus grande que la
 * taille restante dans le tableau (`concat_to_array`).
 *
 * @param[in,out]   arr Buffer.
 * @param[in]       msg Message.
 * @param[in]       flags Champs à mettre dans le buffer.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int concat_msg_to_buf(dyn_array *arr, message *msg, unsigned int flags);
/**
 * @brief Ajoute les données contenues dans `buf` spécifiées par les `flags` dans `msg`.
 *
 * @param[in]       buf Buffer.
 * @param[in,out]   msg Message.
 * @param[in]       flags Champs à mettre dans le message.
 */
void buf_to_msg(const char *buf, message *msg, unsigned int flags);
/**
 * @brief Renvoie la taille des données d'un message spécifiées par les `flags`.
 *
 * Ne prend pas en compte la taille de `MESS` et `SIG`.
 *
 * @param[in]   flags Flags.
 */
size_t msg_size(int flags);

#endif