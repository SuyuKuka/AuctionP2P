#ifndef NETIO_H
#define NETIO_H

#include <netinet/in.h>
#include <stdint.h>

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
    NB = 1 << 11
};

enum Codes {
    CONS_VAL = 1,  /* Consensus : message de validation. */
    CONS_CONF = 2, /* Consensus : confirmation de la réception de tous les msg de validation. */
    CONS_CONF_FOLLOW = 20,    /* Consensus: suite du message de confirmation. */
    JOIN_REQ = 3,             /* Join system : demande pour rejoindre le système.  */
    JOIN_REQ_RES = 4,         /* Join system : réponse à la demande initiale. */
    JOIN_SHARE_ID = 5,        /* Join system : partage de l'identité du demandeur à un pair. */
    JOIN_VALID_ID = 50,       /* Join system : identifiant valide. */
    JOIN_INVALID_ID = 51,     /* Join system : identifiant existant. */
    JOIN_SHARE_ID_TO_ALL = 6, /* Join system : partage de l'identité du demandeur aux autres. */
    JOIN_SHARE_ALL_ID = 7,    /* Join system : partage de l'identité de tous au demandeur. */
    AUC_INIT = 8,             /* Auction : initialisation d'une vente. */
    AUC_BID = 9,              /* Auction : enchérir sur une vente. */
    AUC_NOTICE = 11,          /* Auction : première notification de fin. */
    AUC_END = 12,             /* Auction : fin de la vente. */
    AUC_LEAVE = 13,           /* Auction : quitter le système. */
    AUC_CONCUR = 14,          /* Auction : concurrence entre deux enchères. */
    AUC_REJECT = 15,          /* Auction : enchère rejetée car invalide. */
    MISSING_SUP = 16,         /* Superviseur absent. */
    MISSING_SUP_CANCEL = 17,  /* Annulation de la notification d'absence. */
    MISSING_PEER = 18,        /* Pair absent. */
};

/* Structure pour un message quelconque. */
typedef struct message {
    uint8_t code;       /* Type. */
    uint16_t id;        /* Identifiant. */
    uint8_t lmess;      /* Longueur du message. */
    uint8_t lsig;       /* Longueur de la signature. */
    char* mess;         /* Message. */
    char* sig;          /* Signature. */
    struct in6_addr ip; /* Adresse IP. */
    uint16_t port;      /* Port. */
    char cle[113];      /* Clé. */
    uint32_t numv;      /* Num vente. */
    uint32_t prix;      /* Prix vente. */
    uint16_t nb;        /* Nombre d'un type de message concaténé à la suite. */
} message;

/* Structure pour un buffer de taille variable. */
typedef struct msg_buffer {
    char* buf;   /* Buffer. */
    size_t size; /* Taille totale du buffer. */
    size_t len;  /* Taille actuelle du buffer. */
} msg_buffer;

/**
 * @brief Initialise `msg_buf` de taille initiale `size`.
 *
 * Si `malloc` échoue, tous les champs sont mis à `0`.
 *
 * @param[in,out]   msg_buf Buffer.
 * @param[in]       size Taille initiale.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int init_msg_buffer(msg_buffer* msg_buf, size_t size);
/**
 * @brief Réalloue `msg_buf->buf` en doublant sa taille.
 *
 * Si `realloc` échoue, il n'y a aucun changement.
 *
 * @param[in,out]   msg_buf Buffer.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int realloc_msg_buffer(msg_buffer* msg_buf);
/**
 * @brief Ajoute `data` à la suite dans le buffer.
 *
 * La méthode réalloue le buffer si la taille des données est plus grande que la taille restante
 * dans le buffer (`realloc_msg_buffer`).
 *
 * @param[in,out]   msg_buf Buffer.
 * @param[in]       data Données.
 * @param[in]       data_size Taille des données.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int concat_to_buf(msg_buffer* msg_buf, void* data, size_t data_size);
/**
 * @brief Ajoute les données contenues dans `msg` spécifiées par les `flags` à la suite du buffer.
 *
 * Exemple :
 * Si `flags = CODE | ID`, alors les champs `msg->code` et `msg->id` seront concaténé à `msg_buf`.
 *
 * La méthode réalloue le buffer si la taille des données est plus grande que la taille restante
 * dans le buffer (`concat_to_buf`).
 *
 * @param[in,out]   msg_buf Buffer.
 * @param[in]       data Données.
 * @param[in]       data_size Taille des données.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int concat_msg_to_buf(msg_buffer* msg_buf, message* msg, unsigned int flags);

#endif