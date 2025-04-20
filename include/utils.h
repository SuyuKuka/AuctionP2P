#ifndef UTILS_H
#define UTILS_H

#include <netdb.h>

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
int write_nb(int sock, char *buf, size_t n, int timeout);
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
int read_nb(int sock, char *buf, size_t n, int timeout);
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
 * @brief Initialise l'adresse `addr` de type multicast.
 *
 * @param[in,out]   addr Pointeur sur la structure à initialiser.
 * @param[in]       ip Adresse IP.
 * @param[in]       port Port.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int init_multicast_addr(struct sockaddr_in6 *addr, const char *ip, int port);
/**
 * @brief Renvoie une socket abonnée à l'adresse `multicast_addr`.
 *
 * @param[in]   multicast_addr Adresse multicast.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int make_multicast_socket(struct sockaddr_in6 multicast_addr);
/**
 * @brief Initialise l'adresse personnelle `personal_addr`.
 *
 * @param[in,out]   personal_addr Adresse.
 * @param[in]       port Port spécifique.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int init_personal_addr(struct sockaddr_in6 *personal_addr, int port);
/**
 * @brief Renvoie une socket serveur TCP.
 *
 * @param[in]   port Port.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int make_tcp_server_socket(int port);
/**
 * @brief Renvoie une socket UDP.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int make_udp_socket();
/**
 * @brief Renvoie le port sur auquel la socket est reliée.
 *
 * @param[in]   sock Socket.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int get_socket_port(int sock);

#endif