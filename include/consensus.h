#ifndef CONSENSUS_H
#define CONSENSUS_H

#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "protocol.h"

int supervisor_begin_consensus(pers_data p_data, struct sockaddr_in6 mult_addr, linked_list peers,
                               message *mess, int flags);

int send_await_validation_to_supervisor(struct sockaddr_in6 supervisor_addr, message *msg,
                                        int flags, pers_data p_data, linked_list peers,
                                        int mult_sock);

int verify_signature(const char *mess, const char *signature, const char *key);

#endif
