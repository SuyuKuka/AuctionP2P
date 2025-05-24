#ifndef NETIO_H
#define NETIO_H

#include "protocol.h"

#define MILLI(seconds) seconds * 1000

/**
 * @brief Envoie `n` octets de `buf` vers `sock` avec une attente maximale `timeout`.
 *
 * @param[in]   sock Socket.
 * @param[in]   buf Buffer.
 * @param[in]   n Quantité à envoyer.
 * @param[in]   timeout Temps max.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int write_nb(int sock, void *buf, size_t n, int timeout);
/**
 * @brief Lit `n` octets depuis `sock` et les met dans `buf` avec une attente maximale `timeout`.
 *
 * @param[in]   sock Socket.
 * @param[in]   buf Buffer.
 * @param[in]   n Quantité à lire.
 * @param[in]   timeout Temps max.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int read_nb(int sock, void *buf, size_t n, int timeout);
/**
 * @brief Modifie `sock` comme étant non-bloquante.
 *
 * @param[in]   sock Socket.
 * @param[in]   buf Buffer.
 * @param[in]   n Quantité à lire.
 * @param[in]   timeout Temps max.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int set_nonblock(int sock);
/**
 * @brief Envoie le message à l'adresse `addr` depuis la socket `sock`.
 *
 * @param[in]   msg Message.
 * @param[in]   flags Flags du message.
 * @param[in]   sock Socket.
 * @param[in]   addr Adresse.
 * @param[in]   len Taille de l'adresse.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int send_msg_to(message msg, int flags, int sock, struct sockaddr *addr, socklen_t len);
/**
 * @brief Lit la socket `sock` et met le contenu du message dans `msg`, l'adresse dans `addr` et la
 * taille dans `len`.
 *
 * @param[in]   msg Message.
 * @param[in]   flags Flags du message.
 * @param[in]   additional_size Taille additionnelle pour le buffer.
 * @param[in]   sock Socket.
 * @param[in]   addr Adresse.
 * @param[in]   len Taille de l'adresse.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int recv_msg_from(message *msg, int flags, size_t additional_size, int sock, struct sockaddr *addr,
                  socklen_t *len);
/**
 * @brief Envoie le message à la socket `sock`.
 *
 * @param[in]   msg Message.
 * @param[in]   flags Flags du message.
 * @param[in]   sock Socket.
 * @param[in]   timeout Temps maximal d'attente.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int write_msg(message msg, int flags, int sock, int timeout);
/**
 * @brief Lit la socket `sock` et met le contenu dans `msg` selon les `flags`.
 *
 * @param[in]       sock Socket.
 * @param[in,out]   msg Message.
 * @param[in]       flags Flags du message.
 * @param[in]       timeout Temps maximal d'attente.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int read_msg(int sock, message *msg, int flags, int timeout);

#endif