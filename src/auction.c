#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "linking.h"
#include "protocol.h"

#define LINKING_IP "ff12::feed:bee"
#define AUCTIONS_IP "ff12::add:beef"
#define LINKING_PORT 57500
#define AUCTIONS_PORT 59500

int main(int argc, char const *argv[]) {
    // TODO: Demander l'adresse et le port ?
    (void)argc;
    (void)argv;

    int res;
    int linking_subs_sock = -1;
    struct sockaddr_in6 linking_addr;
    struct sockaddr_in6 auction_addr;

    // Initialisation de l'adresse de liaison
    if (init_multicast_addr(&linking_addr, LINKING_IP, LINKING_PORT) < 0) goto error;

    res = process_linking(linking_addr);
    if (res == 1) {
        printf("Nobody connected...\n");
    } else if (res < 0) {
        goto error;
    }

    // TODO: A appeler dans un autre thread ?
    handle_join_requests(linking_addr);

    return EXIT_SUCCESS;

error:
    if (linking_subs_sock) close(linking_subs_sock);

    return EXIT_FAILURE;
}
