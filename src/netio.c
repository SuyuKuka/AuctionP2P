#include "netio.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>

int read_nb(int sock, void *buf, size_t n, int timeout) {
    struct pollfd fd = {.fd = sock, .events = POLLIN};

    int poll_ret;
    ssize_t n_read;
    size_t pos = 0;
    while (pos < n && (poll_ret = poll(&fd, 1, timeout))) {
        if (fd.revents & POLLIN) {
            n_read = recv(sock, buf + pos, n - pos, 0);
            if (n_read < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                perror("[ERROR] recv");
                return -1;
            }
            if (n_read == 0) {
                fprintf(stderr, "[WARN] Connection closed by peer\n");
                if (pos == n) return 0;
                return -1;
            }

            pos += n_read;
        }
    }

    if (poll_ret < 0) {
        perror("[ERROR] poll");
        return -1;
    } else if (poll_ret == 0) {
        fprintf(stderr, "[WARN] Timeout on read\n");
        return -1;
    }

    if (pos > n) {
        fprintf(stderr, "[ERROR] Invalid message format received.\n");
        return -1;
    }

    return 0;
}

int write_nb(int sock, void *buf, size_t n, int timeout) {
    struct pollfd fd = {.fd = sock, .events = POLLOUT};

    int poll_ret;
    size_t pos = 0;
    while (pos < n && (poll_ret = poll(&fd, 1, timeout))) {
        if (fd.revents & POLLOUT) {
            ssize_t n_sent = send(sock, buf + pos, n - pos, 0);
            if (n_sent < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                perror("[ERROR] send");
                return -1;
            }
            pos += n_sent;
        }
    }

    if (poll_ret < 0) {
        perror("[ERROR] poll");
        return -1;
    } else if (poll_ret == 0) {
        fprintf(stderr, "[WARN] Timeout on write\n");
        return -1;
    }

    return 0;
}

int set_nonblock(int sock) {
    int flags;

    if ((flags = fcntl(sock, F_GETFL, 0)) < 0) {
        perror("[ERROR] fcntl F_GETFL");
        return -1;
    }

    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("[ERROR] fcntl F_SETFL");
        return -1;
    }

    return 0;
}
