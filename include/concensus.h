#ifndef CONCENSUS_H
#define CONCENSUS_H

#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int send_validation_to_peer_supervisor(int sock, struct sockaddr_in6 addr, message msg);

int wait_for_validation(pers_data *p_data, int total_validation_msg, message mess);

#endif