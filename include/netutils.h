#ifndef NETUTILS_H
#define NETUTILS_H

#include <netdb.h>

#define MILLI(seconds) seconds * 1000

/**
 * @brief Initialise l'adresse `addr` de type multicast.
 *
 * @param[in,out]   addr Pointeur sur la structure à initialiser.
 * @param[in]       ip Adresse IP au format présentation.
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
 * @brief Renvoie le port auquel la socket est reliée.
 *
 * @param[in]   sock Socket.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int get_socket_port(int sock);

#endif