#include "net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../config.h"

int recv_all(int sock, void *buffer, size_t size) {
    size_t total = 0;
    char *p = (char *)buffer;
    while (total < size) {
        ssize_t n = recv(sock, p + total, size - total, 0);
        if (n <= 0) {
            return -1;
        }
        total += (size_t)n;
    }
    return 0;
}

int send_all(int sock, const void *buffer, size_t size) {
    size_t total = 0;
    const char *p = (const char *)buffer;
    while (total < size) {
        ssize_t n = send(sock, p + total, size - total, 0);
        if (n <= 0) {
            return -1;
        }
        total += (size_t)n;
    }
    return 0;
}

int create_client_listener(int *out_port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(sock);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(0);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return -1;
    }

    socklen_t len = sizeof(addr);
    if (getsockname(sock, (struct sockaddr *)&addr, &len) < 0) {
        perror("getsockname");
        close(sock);
        return -1;
    }

    if (listen(sock, MAX_PLAYERS) < 0) {
        perror("listen");
        close(sock);
        return -1;
    }

    *out_port = ntohs(addr.sin_port);
    return sock;
}

int connect_to_server(const char *server_ip, int listen_port, const char *name, JoinResponse *out, int *out_server_sock) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, server_ip, &addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server IP: %s\n", server_ip);
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    JoinRequest req;
    req.listen_port = listen_port;
    if (name) {
        snprintf(req.name, sizeof(req.name), "%s", name);
    } else {
        req.name[0] = '\0';
    }
    if (send_all(sock, &req, sizeof(req)) != 0) {
        fprintf(stderr, "Failed to send JoinRequest\n");
        close(sock);
        return -1;
    }

    if (recv_all(sock, out, sizeof(*out)) != 0) {
        fprintf(stderr, "Failed to receive JoinResponse\n");
        close(sock);
        return -1;
    }

    if (out_server_sock) {
        *out_server_sock = sock;
    } else {
        close(sock);
    }
    return 0;
}