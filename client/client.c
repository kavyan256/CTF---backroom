#include <GL/glut.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "core/game.h"
#include "input/input.h"
#include "networking/net.h"
#include "networking/peer_manager.h"
#include "networking/recv_thread.h"
#include "render/render.h"
#include "render/sprite.h"
#include "../common/logging.h"

static pthread_t g_recv_thread;
static int g_listener_sock = -1;
static int g_server_sock = -1;

static void on_shutdown(void) {
    log_info("Shutting down client...");
    recv_thread_stop();
    pthread_join(g_recv_thread, NULL);

    if (g_listener_sock >= 0) {
        close(g_listener_sock);
        g_listener_sock = -1;
    }
    peer_manager_shutdown();
    game_shutdown();
    
    extern void sprite_manager_shutdown(void);
    sprite_manager_shutdown();
    
    log_info("Client shutdown complete");
}

static void on_sigint(int sig) {
    (void)sig;
    g_game.running = 0;
    exit(0);
}

static void display_cb(void) {
    render_scene();
}

static void timer_cb(int value) {
    (void)value;
    if (!g_game.running) {
        exit(0);
        return;
    }

    if (!game_has_started() && input_consume_ready_toggle()) {
        int ready = game_toggle_local_ready();
        if (recv_thread_send_ready_state(ready) != 0) {
            log_error("Failed to send ready state to server");
        }
    }

    if (game_has_started()) {
        game_update_step();
    }
    glutPostRedisplay();
    glutTimerFunc((unsigned int)(DT * 1000.0f), timer_cb, 0);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }

    log_info("Initializing client connecting to %s", argv[1]);
    signal(SIGINT, on_sigint);

    int listen_port = 0;
    g_listener_sock = create_client_listener(&listen_port);
    if (g_listener_sock < 0) {
        log_error("Failed to create client listener socket");
        return 1;
    }

    JoinResponse join;
    if (connect_to_server(argv[1], listen_port, &join, &g_server_sock) != 0) {
        log_error("Failed to connect to server");
        close(g_listener_sock);
        return 1;
    }

    int local_id = join.assigned_id;
    log_info("Assigned local player: %d", local_id);

    peer_manager_init(local_id);
    if (peer_manager_connect_mesh(&join, g_listener_sock) != 0) {
        log_error("Failed to establish mesh connections");
        close(g_listener_sock);
        return 1;
    }

    sprite_manager_init();
    game_init(local_id, &join);

    recv_thread_set_server_socket(g_server_sock);
    g_server_sock = -1;
    if (recv_thread_start(&g_recv_thread) != 0) {
        log_error("Failed to start receive thread");
        on_shutdown();
        return 1;
    }

    log_info("Starting OpenGL window");
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("Collaborative Game Visualization");

    render_init();
    input_init_callbacks();

    glutDisplayFunc(display_cb);
    glutTimerFunc((unsigned int)(DT * 1000.0f), timer_cb, 0);
    glutMainLoop();

    on_shutdown();
    return 0;
}