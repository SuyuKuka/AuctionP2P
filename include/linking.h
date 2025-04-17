#ifndef LINKING_H
#define LINKING_H

#include <netinet/in.h>

/**
 * @brief Gère le processus pour rejoindre le système d'enchères.
 *
 * @param[in]   linking_addr Adresse multicast.
 *
 * @return `-1` en cas d'erreur, `0` s'il n'y a pas eu de réponse, `1` sinon.
 */
int process_linking(struct sockaddr_in6 linking_addr);
/**
 * @brief Gère les demandes pour rejoindre le système d'enchères.
 *
 * @param[in]   linking_addr Adresse multicast.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int handle_join_requests(struct sockaddr_in6 linking_addr);

#endif