#include "recv_thread.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include "../config.h"
#include "../core/game.h"
#include "net.h"
#include "peer_manager.h"

static volatile int g_recv_running = 0;
static int g_server_sock = -1;
static pthread_mutex_t g_server_sock_mutex = PTHREAD_MUTEX_INITIALIZER;

void recv_thread_set_server_socket(int sock) {
    pthread_mutex_lock(&g_server_sock_mutex);
    g_server_sock = sock;
    pthread_mutex_unlock(&g_server_sock_mutex);
}

int recv_thread_send_ready_state(int is_ready) {
    int server_sock;
    pthread_mutex_lock(&g_server_sock_mutex);
    server_sock = g_server_sock;
    pthread_mutex_unlock(&g_server_sock_mutex);

    if (server_sock < 0) {
        return -1;
    }

    LobbyPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.msg_type = LOBBY_MSG_READY_TOGGLE;
    pkt.player_id = g_game.local_player_id;
    pkt.is_ready = is_ready ? 1 : 0;

    return send_all(server_sock, &pkt, sizeof(pkt));
}

static void *recv_thread_main(void *arg) {
    (void)arg;
    g_recv_running = 1;

    while (g_recv_running && g_game.running) {
        fd_set set;
        FD_ZERO(&set);

        int maxfd = -1;
        int server_sock;
        pthread_mutex_lock(&g_server_sock_mutex);
        server_sock = g_server_sock;
        pthread_mutex_unlock(&g_server_sock_mutex);

        if (server_sock >= 0) {
            FD_SET(server_sock, &set);
            maxfd = server_sock;
        }

        for (int pid = 0; pid < MAX_PLAYERS; ++pid) {
            if (pid == g_game.local_player_id) {
                continue;
            }
            if (!game_has_started()) {
                continue;
            }
            int sock = peer_manager_get_socket(pid);
            if (sock >= 0) {
                FD_SET(sock, &set);
                if (sock > maxfd) {
                    maxfd = sock;
                }
            }
        }

        if (maxfd < 0) {
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = RECV_THREAD_TIMEOUT_USEC;
            select(0, NULL, NULL, NULL, &tv);
            continue;
        }

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = RECV_THREAD_TIMEOUT_USEC;

        int ready = select(maxfd + 1, &set, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("select");
            break;
        }
        if (ready == 0) {
            continue;
        }

        if (server_sock >= 0 && FD_ISSET(server_sock, &set)) {
            LobbyPacket pkt;
            if (recv_all(server_sock, &pkt, sizeof(pkt)) != 0) {
                fprintf(stderr, "Lobby server disconnected\n");
                close(server_sock);
                pthread_mutex_lock(&g_server_sock_mutex);
                g_server_sock = -1;
                pthread_mutex_unlock(&g_server_sock_mutex);
            } else if (pkt.msg_type == LOBBY_MSG_STATE) {
                for (int i = 0; i < MAX_PLAYERS; ++i) {
                    game_set_player_connected(i, pkt.connected[i]);
                    if (pkt.connected[i]) {
                        game_set_player_ready(i, pkt.ready[i]);
                    }
                }
            } else if (pkt.msg_type == LOBBY_MSG_START) {
                game_set_game_started(1);
                close(server_sock);
                pthread_mutex_lock(&g_server_sock_mutex);
                g_server_sock = -1;
                pthread_mutex_unlock(&g_server_sock_mutex);
                printf("All players ready. Starting game...\n");
            }
        }

        for (int pid = 0; pid < MAX_PLAYERS; ++pid) {
            if (pid == g_game.local_player_id) {
                continue;
            }
            if (!game_has_started()) {
                continue;
            }
            int sock = peer_manager_get_socket(pid);
            if (sock < 0 || !FD_ISSET(sock, &set)) {
                continue;
            }

            /* Receive player position updates */
            PlayerUpdate update;
            if (recv_all(sock, &update, sizeof(update)) != 0) {
                fprintf(stderr, "Peer %d disconnected\n", pid);
                close(sock);
                peer_manager_set_socket(pid, -1);
                continue;
            }
            
            /* Update remote player position */
            game_update_player_position(update.player_id, update.x, update.y, update.angle);
        }
    }

    g_recv_running = 0;
    return NULL;
}

int recv_thread_start(pthread_t *thread) {
    return pthread_create(thread, NULL, recv_thread_main, NULL);
}

void recv_thread_stop(void) {
    g_recv_running = 0;

    pthread_mutex_lock(&g_server_sock_mutex);
    if (g_server_sock >= 0) {
        close(g_server_sock);
        g_server_sock = -1;
    }
    pthread_mutex_unlock(&g_server_sock_mutex);
}