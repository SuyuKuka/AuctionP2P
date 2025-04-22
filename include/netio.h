#ifndef NETIO_H
#define NETIO_H

#include <stdlib.h>

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

#endif