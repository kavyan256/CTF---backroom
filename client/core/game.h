#ifndef GAME_H
#define GAME_H

#include <pthread.h>

#include "../../common/protocol.h"
#include "../config.h"

/* Player state for FPS */
typedef struct {
    float x, y;         /* World position */
    float angle;        /* View angle in radians */
    float fov;          /* Field of view (radians) */
} PlayerState;

typedef struct {
    int local_player_id;
    JoinResponse join_info;
    int connected_players[MAX_PLAYERS];
    int ready_players[MAX_PLAYERS];
    int game_started;

    /* FPS mode */
    PlayerState players[MAX_PLAYERS];
    PlayerState local_player;
    char texture_files[MAX_PLAYERS][128];
    int flag_holder;
    float flag_hold_time[MAX_PLAYERS];
    int flag_steals[MAX_PLAYERS];
    float flag_steal_cooldown;
    
    pthread_mutex_t player_mutex;
    volatile int running;
} GameState;

extern GameState g_game;

void game_init(int local_player_id, const JoinResponse *join_info);
void game_shutdown(void);

void game_update_step(void);
void game_update_player_position(int player_id, float x, float y, float angle);
void game_set_player_ready(int player_id, int ready);
int game_get_player_ready(int player_id);
void game_set_player_connected(int player_id, int connected);
int game_get_connected_player(int player_id);
void game_set_game_started(int started);
int game_has_started(void);
int game_toggle_local_ready(void);
int game_get_local_ready(void);

int game_get_flag_holder(void);
float game_get_flag_hold_time(int player_id);
int game_get_flag_steals(int player_id);
float game_get_flag_cooldown(void);

int game_is_local_region(float x, float y, int player_id);

#endif