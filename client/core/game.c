#include "game.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#include "../simulation/level.h"
#include "../input/input.h"
#include "../networking/peer_manager.h"
#include "../networking/net.h"
#include "../render/sprite.h"
#include "../render/texture.h"

#define FLAG_STEAL_DISTANCE 1.2f
#define FLAG_STEAL_COOLDOWN_SEC 0.8f

GameState g_game;

void game_init(int local_player_id, const JoinResponse *join_info) {
    g_game.local_player_id = local_player_id;
    memcpy(&g_game.join_info, join_info, sizeof(*join_info));
    
    pthread_mutex_init(&g_game.player_mutex, NULL);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        snprintf(g_game.texture_files[i], sizeof(g_game.texture_files[i]), "(none)");
        g_game.flag_hold_time[i] = 0.0f;
        g_game.flag_steals[i] = 0;
        g_game.connected_players[i] = (join_info->players[i].ip[0] != '\0') ? 1 : 0;
        g_game.ready_players[i] = 0;
    }
    g_game.game_started = 0;
    g_game.flag_holder = -1;
    g_game.flag_steal_cooldown = 0.0f;
    
    /* Initialize all player positions at different locations */
    float spawn_positions[MAX_PLAYERS][4] = {
        {6.5f, 6.5f, 0.0f, 0.785f},          /* Player 0: (6.5,6.5), angle 45° */
        {23.5f, 23.5f, 3.14159f, 3.926f},    /* Player 1: (23.5,23.5), angle 225° */
        {26.5f, 6.5f, 1.57f, 2.356f},        /* Player 2: (26.5,6.5), angle 135° */
        {6.5f, 26.5f, 4.71f, 5.498f}         /* Player 3: (6.5,26.5), angle 315° */
    };
    
    /* Only initialize connected players */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (join_info->players[i].ip[0] != '\0') {
            /* Player is connected */
            g_game.players[i].x = spawn_positions[i][0];
            g_game.players[i].y = spawn_positions[i][1];
            g_game.players[i].angle = spawn_positions[i][2];
            g_game.players[i].fov = 1.047f;
            if (g_game.flag_holder < 0) {
                g_game.flag_holder = i;
            }
        }
    }
    
    /* Local player gets their assigned spawn point */
    g_game.local_player.x = spawn_positions[local_player_id][0];
    g_game.local_player.y = spawn_positions[local_player_id][1];
    g_game.local_player.angle = spawn_positions[local_player_id][2];
    g_game.local_player.fov = 1.047f;
    
    printf("Player %d spawned at (%.1f, %.1f) angle %.2f\n", 
           local_player_id, g_game.local_player.x, g_game.local_player.y, g_game.local_player.angle);
    
    /* Load player sprite textures for connected players only */
    const char *texture_files[] = {
        "textures/lord.ppm",
        "textures/ohyea.ppm",
        "textures/player_blue.ppm",
        "textures/player_yellow.ppm"
    };
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (join_info->players[i].ip[0] != '\0') {
            Texture *tex = texture_load_ppm(texture_files[i]);
            if (tex) {
                sprite_set_texture(i, tex, 0.8f, 1.6f);
                snprintf(g_game.texture_files[i], sizeof(g_game.texture_files[i]), "%s", texture_files[i]);
            } else {
                snprintf(g_game.texture_files[i], sizeof(g_game.texture_files[i]), "%s (FAILED)", texture_files[i]);
            }
        }
    }
    
    g_game.running = 1;
    
    printf("Game initialized for player %d\n", local_player_id);
    if (g_game.flag_holder >= 0) {
        printf("CTF: Player %d starts with the flag\n", g_game.flag_holder);
    }
}

void game_shutdown(void) {
    g_game.running = 0;
    pthread_mutex_destroy(&g_game.player_mutex);
}

void game_update_step(void) {
    /* Get input and update local player */
    PlayerInput *input = input_get_current();
    
    /* Movement with collision detection */
    float move_speed = 0.1f;
    float collision_margin = 0.1f;
    
    if (input->forward) {
        float new_x = g_game.local_player.x + cosf(g_game.local_player.angle) * move_speed;
        float new_y = g_game.local_player.y + sinf(g_game.local_player.angle) * move_speed;
        if (level_get_wall((int)new_x, (int)new_y) == WALL_NONE &&
            level_get_wall((int)(new_x + collision_margin), (int)new_y) == WALL_NONE &&
            level_get_wall((int)(new_x - collision_margin), (int)new_y) == WALL_NONE &&
            level_get_wall((int)new_x, (int)(new_y + collision_margin)) == WALL_NONE &&
            level_get_wall((int)new_x, (int)(new_y - collision_margin)) == WALL_NONE) {
            g_game.local_player.x = new_x;
            g_game.local_player.y = new_y;
        }
    }
    if (input->backward) {
        float new_x = g_game.local_player.x - cosf(g_game.local_player.angle) * move_speed;
        float new_y = g_game.local_player.y - sinf(g_game.local_player.angle) * move_speed;
        if (level_get_wall((int)new_x, (int)new_y) == WALL_NONE &&
            level_get_wall((int)(new_x + collision_margin), (int)new_y) == WALL_NONE &&
            level_get_wall((int)(new_x - collision_margin), (int)new_y) == WALL_NONE &&
            level_get_wall((int)new_x, (int)(new_y + collision_margin)) == WALL_NONE &&
            level_get_wall((int)new_x, (int)(new_y - collision_margin)) == WALL_NONE) {
            g_game.local_player.x = new_x;
            g_game.local_player.y = new_y;
        }
    }
    if (input->strafe_left) {
        float new_x = g_game.local_player.x + cosf(g_game.local_player.angle - 1.57f) * move_speed;
        float new_y = g_game.local_player.y + sinf(g_game.local_player.angle - 1.57f) * move_speed;
        if (level_get_wall((int)new_x, (int)new_y) == WALL_NONE &&
            level_get_wall((int)(new_x + collision_margin), (int)new_y) == WALL_NONE &&
            level_get_wall((int)(new_x - collision_margin), (int)new_y) == WALL_NONE &&
            level_get_wall((int)new_x, (int)(new_y + collision_margin)) == WALL_NONE &&
            level_get_wall((int)new_x, (int)(new_y - collision_margin)) == WALL_NONE) {
            g_game.local_player.x = new_x;
            g_game.local_player.y = new_y;
        }
    }
    if (input->strafe_right) {
        float new_x = g_game.local_player.x + cosf(g_game.local_player.angle + 1.57f) * move_speed;
        float new_y = g_game.local_player.y + sinf(g_game.local_player.angle + 1.57f) * move_speed;
        if (level_get_wall((int)new_x, (int)new_y) == WALL_NONE &&
            level_get_wall((int)(new_x + collision_margin), (int)new_y) == WALL_NONE &&
            level_get_wall((int)(new_x - collision_margin), (int)new_y) == WALL_NONE &&
            level_get_wall((int)new_x, (int)(new_y + collision_margin)) == WALL_NONE &&
            level_get_wall((int)new_x, (int)(new_y - collision_margin)) == WALL_NONE) {
            g_game.local_player.x = new_x;
            g_game.local_player.y = new_y;
        }
    }
    
    /* Rotation */
    float rotate_speed = 0.05f;
    if (input->turn_left) {
        g_game.local_player.angle -= rotate_speed;
    }
    if (input->turn_right) {
        g_game.local_player.angle += rotate_speed;
    }
    
    /* Clamp to valid position */
    if (g_game.local_player.x < 1.0f) g_game.local_player.x = 1.0f;
    if (g_game.local_player.x > LEVEL_WIDTH - 1.0f) g_game.local_player.x = LEVEL_WIDTH - 1.0f;
    if (g_game.local_player.y < 1.0f) g_game.local_player.y = 1.0f;
    if (g_game.local_player.y > LEVEL_HEIGHT - 1.0f) g_game.local_player.y = LEVEL_HEIGHT - 1.0f;
    
    /* Update local player position in shared array and CTF state */
    pthread_mutex_lock(&g_game.player_mutex);
    g_game.players[g_game.local_player_id].x = g_game.local_player.x;
    g_game.players[g_game.local_player_id].y = g_game.local_player.y;
    g_game.players[g_game.local_player_id].angle = g_game.local_player.angle;

    if (g_game.flag_steal_cooldown > 0.0f) {
        g_game.flag_steal_cooldown -= DT;
        if (g_game.flag_steal_cooldown < 0.0f) {
            g_game.flag_steal_cooldown = 0.0f;
        }
    }

    if (g_game.flag_holder >= 0 && g_game.flag_holder < MAX_PLAYERS) {
        g_game.flag_hold_time[g_game.flag_holder] += DT;

        if (g_game.flag_steal_cooldown <= 0.0f) {
            float holder_x = g_game.players[g_game.flag_holder].x;
            float holder_y = g_game.players[g_game.flag_holder].y;
            float best_dist_sq = FLAG_STEAL_DISTANCE * FLAG_STEAL_DISTANCE;
            int new_holder = -1;

            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (i == g_game.flag_holder || g_game.join_info.players[i].ip[0] == '\0') {
                    continue;
                }

                float dx = g_game.players[i].x - holder_x;
                float dy = g_game.players[i].y - holder_y;
                float dist_sq = dx * dx + dy * dy;
                if (dist_sq <= best_dist_sq) {
                    best_dist_sq = dist_sq;
                    new_holder = i;
                }
            }

            if (new_holder >= 0) {
                int prev_holder = g_game.flag_holder;
                g_game.flag_holder = new_holder;
                g_game.flag_steals[new_holder] += 1;
                g_game.flag_steal_cooldown = FLAG_STEAL_COOLDOWN_SEC;
                printf("CTF: Player %d stole the flag from Player %d\n", new_holder, prev_holder);
            }
        }
    }
    pthread_mutex_unlock(&g_game.player_mutex);
    
    /* Broadcast position to peers */
    PlayerUpdate update;
    update.player_id = g_game.local_player_id;
    update.x = g_game.local_player.x;
    update.y = g_game.local_player.y;
    update.angle = g_game.local_player.angle;
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (i == g_game.local_player_id) continue;
        
        int sock = peer_manager_get_socket(i);
        if (sock >= 0) {
            send_all(sock, &update, sizeof(update));
        }
    }
}

void game_update_player_position(int player_id, float x, float y, float angle) {
    pthread_mutex_lock(&g_game.player_mutex);
    if (player_id >= 0 && player_id < MAX_PLAYERS) {
        g_game.players[player_id].x = x;
        g_game.players[player_id].y = y;
        g_game.players[player_id].angle = angle;
    }
    pthread_mutex_unlock(&g_game.player_mutex);
}

void game_set_player_ready(int player_id, int ready) {
    pthread_mutex_lock(&g_game.player_mutex);
    if (player_id >= 0 && player_id < MAX_PLAYERS) {
        g_game.ready_players[player_id] = ready ? 1 : 0;
    }
    pthread_mutex_unlock(&g_game.player_mutex);
}

int game_get_player_ready(int player_id) {
    int value = 0;
    pthread_mutex_lock(&g_game.player_mutex);
    if (player_id >= 0 && player_id < MAX_PLAYERS) {
        value = g_game.ready_players[player_id];
    }
    pthread_mutex_unlock(&g_game.player_mutex);
    return value;
}

void game_set_player_connected(int player_id, int connected) {
    pthread_mutex_lock(&g_game.player_mutex);
    if (player_id >= 0 && player_id < MAX_PLAYERS) {
        g_game.connected_players[player_id] = connected ? 1 : 0;
    }
    pthread_mutex_unlock(&g_game.player_mutex);
}

int game_get_connected_player(int player_id) {
    int value = 0;
    pthread_mutex_lock(&g_game.player_mutex);
    if (player_id >= 0 && player_id < MAX_PLAYERS) {
        value = g_game.connected_players[player_id];
    }
    pthread_mutex_unlock(&g_game.player_mutex);
    return value;
}

void game_set_game_started(int started) {
    pthread_mutex_lock(&g_game.player_mutex);
    g_game.game_started = started ? 1 : 0;
    pthread_mutex_unlock(&g_game.player_mutex);
}

int game_has_started(void) {
    int started;
    pthread_mutex_lock(&g_game.player_mutex);
    started = g_game.game_started;
    pthread_mutex_unlock(&g_game.player_mutex);
    return started;
}

int game_toggle_local_ready(void) {
    int ready;
    pthread_mutex_lock(&g_game.player_mutex);
    ready = !g_game.ready_players[g_game.local_player_id];
    g_game.ready_players[g_game.local_player_id] = ready;
    pthread_mutex_unlock(&g_game.player_mutex);
    return ready;
}

int game_get_local_ready(void) {
    int ready;
    pthread_mutex_lock(&g_game.player_mutex);
    ready = g_game.ready_players[g_game.local_player_id];
    pthread_mutex_unlock(&g_game.player_mutex);
    return ready;
}

int game_is_local_region(float x, float y, int player_id) {
    (void)x;
    (void)y;
    (void)player_id;
    return 1;
}

int game_get_flag_holder(void) {
    int holder;
    pthread_mutex_lock(&g_game.player_mutex);
    holder = g_game.flag_holder;
    pthread_mutex_unlock(&g_game.player_mutex);
    return holder;
}

float game_get_flag_hold_time(int player_id) {
    float t = 0.0f;
    pthread_mutex_lock(&g_game.player_mutex);
    if (player_id >= 0 && player_id < MAX_PLAYERS) {
        t = g_game.flag_hold_time[player_id];
    }
    pthread_mutex_unlock(&g_game.player_mutex);
    return t;
}

int game_get_flag_steals(int player_id) {
    int s = 0;
    pthread_mutex_lock(&g_game.player_mutex);
    if (player_id >= 0 && player_id < MAX_PLAYERS) {
        s = g_game.flag_steals[player_id];
    }
    pthread_mutex_unlock(&g_game.player_mutex);
    return s;
}

float game_get_flag_cooldown(void) {
    float c;
    pthread_mutex_lock(&g_game.player_mutex);
    c = g_game.flag_steal_cooldown;
    pthread_mutex_unlock(&g_game.player_mutex);
    return c;
}
