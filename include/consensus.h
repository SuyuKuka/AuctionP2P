#ifndef CONSENSUS_H
#define CONSENSUS_H

#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "protocol.h"

/**
 * @brief Débute un consensus sur le message `mess` en tant que superviseur.
 *
 * La fonction attend les messages de validation (code 1) de tous les pairs du système, vérifie leur
 * signature, puis envoie le message de confirmation (code 2 et 20).
 *
 * @param[in]   p_data Données personnelles.
 * @param[in]   mult_addr Adresse multicast.
 * @param[in]   peers Liste des pairs du système.
 * @param[in]   mess Message à vérifier.
 * @param[in]   flags Champs du message.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int supervisor_begin_consensus(pers_data p_data, struct sockaddr_in6 mult_addr, linked_list *peers,
                               message *mess, int flags);

/**
 * @brief Envoi le message de validation, attend la réponse du superviseur et vérifie les
 * signatures.
 *
 * La fonction envoie le message de validation (code 1) et attend les messages de confirmation (code
 * 2 et 20) du superviseur.
 * Elle vérifie ensuite la validité des signatures.
 *
 * @param[in]   supervisor_addr Adresse du superviseur.
 * @param[in]   msg Message à vérifier.
 * @param[in]   flags Champs du message.
 * @param[in]   p_data Données personnelles.
 * @param[in]   peers Liste des pairs du système.
 * @param[in]   mult_sock Socket multicast.
 * @param[in]   supervisor_id ID du superviseur.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int send_await_validation_to_supervisor(struct sockaddr_in6 supervisor_addr, message *msg,
                                        int flags, pers_data p_data, linked_list peers,
                                        int mult_sock, uint16_t supervisor_id);

/**
 * @brief Vérifie la signature du message signé avec la clé donnée.
 *
 * Vérifie si le message signé avec la clé est identique à la signature donnée.
 *
 * @param[in]   mess Message.
 * @param[in]   signature Signature à vérifier.
 * @param[in]   key Clé.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int verify_signature(const char *mess, const char *signature, const char *key);

#endif
