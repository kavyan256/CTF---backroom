#ifndef GAME_H
#define GAME_H

#include <pthread.h>

#include "../../common/protocol.h"
#include "../config.h"
#include "../render/texture.h"

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
    int selected_character[MAX_PLAYERS];
    int game_started;

    /* FPS mode */
    PlayerState players[MAX_PLAYERS];
    PlayerState local_player;
    char texture_files[MAX_PLAYERS][128];
    char player_names[MAX_PLAYERS][32];
    int flag_holder;
    float flag_hold_time[MAX_PLAYERS];
    int flag_steals[MAX_PLAYERS];
    float flag_steal_cooldown;
    char flag_event_text[128];
    float flag_event_timer;
    int flag_event_type; /* 1=captured(local), -1=lost(local), 0=neutral */
    
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
void game_set_player_character(int player_id, int character_index);
int game_get_player_character(int player_id);
int game_cycle_local_character(int direction);
int game_get_character_count(void);
const char *game_get_character_name(int character_index);
const char *game_get_character_texture_path(int character_index);
const Texture *game_get_character_texture(int character_index);

int game_get_flag_holder(void);
float game_get_flag_hold_time(int player_id);
int game_get_flag_steals(int player_id);
float game_get_flag_cooldown(void);
const char *game_get_flag_event_text(void);
float game_get_flag_event_time_left(void);
int game_get_flag_event_type(void);

/* Player name support */
void game_set_player_name(int player_id, const char *name);
const char *game_get_player_name(int player_id);

int game_is_local_region(float x, float y, int player_id);

#endif