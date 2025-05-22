#ifndef CONCENSUS_H
#define CONCENSUS_H

#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "protocol.h"

int supervisor_wait_for_validation(int sock, struct sockaddr *addr, size_t nb_peers, message *mess, uint16_t supervisor_id);

int send_validation_to_supervisor(int sock, struct sockaddr *addr, message *msg);

int verify_signature(message msg, char *signature);

#endif