#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../common/protocol.h"
#include "../common/logging.h"

typedef struct {
    int sock;
    struct sockaddr_in addr;
    JoinRequest join_req;
} ClientConn;

static int send_all(int sock, const void *buffer, size_t size);

static int all_connected_ready(const int *connected, const int *ready) {
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (connected[i] && !ready[i]) {
            return 0;
        }
    }
    return 1;
}

static int broadcast_lobby_state(ClientConn *clients, int connected_players, const int *connected, const int *ready) {
    LobbyPacket state;
    memset(&state, 0, sizeof(state));
    state.msg_type = LOBBY_MSG_STATE;
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        state.connected[i] = connected[i] ? 1 : 0;
        state.ready[i] = ready[i] ? 1 : 0;
    }

    for (int i = 0; i < connected_players; ++i) {
        if (send_all(clients[i].sock, &state, sizeof(state)) != 0) {
            log_error("Failed sending lobby state to client %d: %s", i, strerror(errno));
            return -1;
        }
    }
    return 0;
}

static void broadcast_start(ClientConn *clients, int connected_players) {
    LobbyPacket start;
    memset(&start, 0, sizeof(start));
    start.msg_type = LOBBY_MSG_START;

    for (int i = 0; i < connected_players; ++i) {
        if (send_all(clients[i].sock, &start, sizeof(start)) != 0) {
            log_error("Failed sending start to client %d: %s", i, strerror(errno));
        }
    }
}

static int recv_all(int sock, void *buffer, size_t size) {
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

static int send_all(int sock, const void *buffer, size_t size) {
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

static int create_listen_socket(void) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        log_error("Socket creation failed: %s", strerror(errno));
        return -1;
    }

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_error("setsockopt failed: %s", strerror(errno));
        close(sock);
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(SERVER_PORT);

    if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        log_error("Bind failed: %s", strerror(errno));
        close(sock);
        return -1;
    }

    if (listen(sock, MAX_PLAYERS) < 0) {
        log_error("Listen failed: %s", strerror(errno));
        close(sock);
        return -1;
    }

    return sock;
}

int run_server(void) {
    int listen_sock = create_listen_socket();
    if (listen_sock < 0) {
        log_error("Failed to create listen socket");
        return 1;
    }

    log_info("Server listening on port %d (waiting for %d players)", SERVER_PORT, MIN_PLAYERS_TO_START);

    ClientConn clients[MAX_PLAYERS];
    memset(clients, 0, sizeof(clients));
    JoinResponse join_payload;
    memset(&join_payload, 0, sizeof(join_payload));

    /* Accept connections until we have minimum players */
    int connected_players = 0;
    for (int i = 0; i < MAX_PLAYERS && connected_players < MIN_PLAYERS_TO_START; ++i) {
        socklen_t len = sizeof(clients[i].addr);
        log_info("Waiting for player %d...", connected_players + 1);
        
        clients[i].sock = accept(listen_sock, (struct sockaddr *)&clients[i].addr, &len);
        if (clients[i].sock < 0) {
            log_error("Accept failed: %s", strerror(errno));
            close(listen_sock);
            return 1;
        }

        char ipbuf[64] = {0};
        inet_ntop(AF_INET, &clients[i].addr.sin_addr, ipbuf, sizeof(ipbuf));
        log_info("Client connected from %s", ipbuf);

        if (recv_all(clients[i].sock, &clients[i].join_req, sizeof(clients[i].join_req)) != 0) {
            log_error("Failed receiving JoinRequest from client %d: %s", i, strerror(errno));
            close(clients[i].sock);
            close(listen_sock);
            return 1;
        }

        join_payload.players[i].player_id = i;
        snprintf(join_payload.players[i].ip, sizeof(join_payload.players[i].ip), "%s", ipbuf);
        join_payload.players[i].port = clients[i].join_req.listen_port;

        log_info("Assigned Player %d (listen port: %d)", i, clients[i].join_req.listen_port);
        connected_players++;
    }

    log_info("Minimum players connected (%d). Entering ready lobby...", MIN_PLAYERS_TO_START);
    
    /* Send matchmaking info to all connected players */
    for (int i = 0; i < connected_players; ++i) {
        JoinResponse out = join_payload;
        out.assigned_id = i;
        if (send_all(clients[i].sock, &out, sizeof(out)) != 0) {
            log_error("Failed sending JoinResponse to client %d: %s", i, strerror(errno));
        }
    }

    int connected[MAX_PLAYERS] = {0};
    int ready[MAX_PLAYERS] = {0};
    for (int i = 0; i < connected_players; ++i) {
        connected[i] = 1;
    }

    if (broadcast_lobby_state(clients, connected_players, connected, ready) != 0) {
        for (int i = 0; i < connected_players; ++i) {
            close(clients[i].sock);
        }
        close(listen_sock);
        return 1;
    }

    while (!all_connected_ready(connected, ready)) {
        fd_set set;
        FD_ZERO(&set);
        int maxfd = -1;

        for (int i = 0; i < connected_players; ++i) {
            FD_SET(clients[i].sock, &set);
            if (clients[i].sock > maxfd) {
                maxfd = clients[i].sock;
            }
        }

        int ready_count = select(maxfd + 1, &set, NULL, NULL, NULL);
        if (ready_count < 0) {
            if (errno == EINTR) {
                continue;
            }
            log_error("Lobby select failed: %s", strerror(errno));
            break;
        }

        for (int i = 0; i < connected_players; ++i) {
            if (!FD_ISSET(clients[i].sock, &set)) {
                continue;
            }

            LobbyPacket pkt;
            if (recv_all(clients[i].sock, &pkt, sizeof(pkt)) != 0) {
                log_error("Client %d disconnected during lobby", i);
                connected[i] = 0;
                ready[i] = 0;
                continue;
            }

            if (pkt.msg_type == LOBBY_MSG_READY_TOGGLE && pkt.player_id == i) {
                ready[i] = pkt.is_ready ? 1 : 0;
                log_info("Player %d ready=%d", i, ready[i]);
                if (broadcast_lobby_state(clients, connected_players, connected, ready) != 0) {
                    break;
                }
            }
        }
    }

    log_info("All connected players are ready. Starting game...");
    broadcast_start(clients, connected_players);

    for (int i = 0; i < connected_players; ++i) {
        close(clients[i].sock);
    }

    close(listen_sock);
    log_info("Server completed matchmaking for %d players", connected_players);
    return 0;
}

int main(void) {
    return run_server();
}