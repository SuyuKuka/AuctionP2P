#include "netio.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>

int read_nb(int sock, void *buf, size_t n, int timeout) {
    struct pollfd fd = {.fd = sock, .events = POLLIN};

    int poll_ret;
    ssize_t n_read;
    size_t pos = 0;
    while (pos < n && (poll_ret = poll(&fd, 1, timeout)) > 0) {
        if (fd.revents & POLLIN) {
            n_read = recv(sock, (char *)buf + pos, n - pos, 0);
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
    while (pos < n && (poll_ret = poll(&fd, 1, timeout)) > 0) {
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

int send_msg_to(message msg, int flags, int sock, struct sockaddr *addr, socklen_t len) {
    dyn_array msg_buf;
    if (init_dyn_array(&msg_buf, sizeof(uint8_t)) < 0) return -1;
    if (concat_msg_to_buf(&msg_buf, &msg, flags) < 0) goto error;

    if (sendto(sock, msg_buf.array, msg_buf.len, 0, addr, len) < 0) {
        perror("[ERROR] sendto");
        goto error;
    }

    free_dyn_array(msg_buf);
    return 0;

error:
    free_dyn_array(msg_buf);
    return -1;
}

int recv_msg_from(message *msg, int flags, size_t additional_size, int sock, struct sockaddr *addr,
                  socklen_t *len) {
    char buf[msg_size(flags) + additional_size];
    memset(buf, 0, sizeof(buf));

    if (recvfrom(sock, buf, sizeof(buf), 0, addr, len) < 0) {
        perror("[ERROR] recvfrom");
        return -1;
    }
    buf_to_msg(buf, msg, flags);

    return 0;
}

int write_msg(message msg, int flags, int sock, int timeout) {
    int ret = 0;

    dyn_array msg_buf;
    if (init_dyn_array(&msg_buf, msg_size(flags)) < 0) return -1;
    if (concat_msg_to_buf(&msg_buf, &msg, flags) < 0 ||
        write_nb(sock, msg_buf.array, msg_buf.len, timeout) < 0)
        ret = -1;

    free_dyn_array(msg_buf);
    return ret;
}

int read_msg(int sock, message *msg, int flags, int timeout) {
    char buf[msg_size(flags)];
    if (read_nb(sock, buf, sizeof(buf), timeout) < 0) return -1;
    buf_to_msg(buf, msg, flags);
    return 0;
}
