#include "game.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <dirent.h>
#include <ctype.h>

#include "../simulation/level.h"
#include "../input/input.h"
#include "../networking/peer_manager.h"
#include "../networking/net.h"
#include "../render/sprite.h"
#include "../render/texture.h"

#define FLAG_STEAL_DISTANCE 1.2f
#define FLAG_STEAL_COOLDOWN_SEC 0.8f
#define MAX_CHARACTERS 128

GameState g_game;

typedef struct {
    char name[64];
    char path[160];
} CharacterEntry;

static CharacterEntry g_character_entries[MAX_CHARACTERS];
static Texture *g_character_base_textures[MAX_CHARACTERS];
static int g_character_count = 0;

static void reset_character_catalog(void) {
    for (int i = 0; i < MAX_CHARACTERS; ++i) {
        if (g_character_base_textures[i]) {
            texture_free(g_character_base_textures[i]);
            g_character_base_textures[i] = NULL;
        }
        g_character_entries[i].name[0] = '\0';
        g_character_entries[i].path[0] = '\0';
    }
    g_character_count = 0;
}

static int has_ppm_extension(const char *name) {
    size_t len = strlen(name);
    if (len < 4) {
        return 0;
    }
    char c1 = (char)tolower((unsigned char)name[len - 4]);
    char c2 = (char)tolower((unsigned char)name[len - 3]);
    char c3 = (char)tolower((unsigned char)name[len - 2]);
    char c4 = (char)tolower((unsigned char)name[len - 1]);
    return c1 == '.' && c2 == 'p' && c3 == 'p' && c4 == 'm';
}

static void build_character_name(const char *filename, char *out, size_t out_size) {
    size_t len = strlen(filename);
    size_t stem_len = len;
    for (size_t i = 0; i < len; ++i) {
        if (filename[i] == '.') {
            stem_len = i;
            break;
        }
    }
    if (stem_len >= out_size) {
        stem_len = out_size - 1;
    }

    for (size_t i = 0; i < stem_len; ++i) {
        char ch = filename[i];
        out[i] = (char)((ch == '_') ? ' ' : toupper((unsigned char)ch));
    }
    out[stem_len] = '\0';
}

static int compare_characters(const void *a, const void *b) {
    const CharacterEntry *ca = (const CharacterEntry *)a;
    const CharacterEntry *cb = (const CharacterEntry *)b;
    return strcmp(ca->name, cb->name);
}

static void load_character_catalog(void) {
    reset_character_catalog();

    DIR *dir = opendir("textures");
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (!has_ppm_extension(entry->d_name)) {
                continue;
            }
            if (g_character_count >= MAX_CHARACTERS) {
                break;
            }

            CharacterEntry *dst = &g_character_entries[g_character_count];
            snprintf(dst->path, sizeof(dst->path), "textures/%s", entry->d_name);
            build_character_name(entry->d_name, dst->name, sizeof(dst->name));
            g_character_count++;
        }
        closedir(dir);
    }

    if (g_character_count > 1) {
        qsort(g_character_entries, (size_t)g_character_count, sizeof(g_character_entries[0]), compare_characters);
    }

    if (g_character_count == 0) {
        snprintf(g_character_entries[0].name, sizeof(g_character_entries[0].name), "DEFAULT");
        snprintf(g_character_entries[0].path, sizeof(g_character_entries[0].path), "textures/lord.ppm");
        g_character_count = 1;
    }

    for (int i = 0; i < g_character_count; ++i) {
        g_character_base_textures[i] = texture_load_ppm(g_character_entries[i].path);
        if (!g_character_base_textures[i]) {
            unsigned char r = (unsigned char)((i * 67) % 255);
            unsigned char g = (unsigned char)((i * 101) % 255);
            unsigned char b = (unsigned char)((i * 149) % 255);
            g_character_base_textures[i] = texture_create_placeholder(32, 64, r, g, b);
        }
    }
}

static int normalize_character_index(int idx) {
    if (g_character_count <= 0) {
        return 0;
    }
    while (idx < 0) {
        idx += g_character_count;
    }
    return idx % g_character_count;
}

void game_init(int local_player_id, const JoinResponse *join_info) {
    load_character_catalog();

    g_game.local_player_id = local_player_id;
    memcpy(&g_game.join_info, join_info, sizeof(*join_info));
    
    pthread_mutex_init(&g_game.player_mutex, NULL);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        snprintf(g_game.texture_files[i], sizeof(g_game.texture_files[i]), "(none)");
        g_game.flag_hold_time[i] = 0.0f;
        g_game.flag_steals[i] = 0;
        g_game.connected_players[i] = (join_info->players[i].ip[0] != '\0') ? 1 : 0;
        g_game.ready_players[i] = 0;
        g_game.selected_character[i] = i % g_character_count;
        /* Initialize player names from join info if present */
        if (join_info->players[i].name[0] != '\0') {
            snprintf(g_game.player_names[i], sizeof(g_game.player_names[i]), "%s", join_info->players[i].name);
        } else {
            snprintf(g_game.player_names[i], sizeof(g_game.player_names[i]), "P%d", i);
        }
    }
    g_game.game_started = 0;
    g_game.flag_holder = -1;
    g_game.flag_steal_cooldown = 0.0f;
    g_game.flag_event_text[0] = '\0';
    g_game.flag_event_timer = 0.0f;
    g_game.flag_event_type = 0;
    
    /* Initialize all player positions at different locations */
    float spawn_positions[MAX_PLAYERS][4] = {
        {6.5f, 6.5f, 0.0f, 0.785f},          /* Player 0: top-left */
        {23.5f, 23.5f, 3.14159f, 3.926f},    /* Player 1: center-south */
        {26.5f, 6.5f, 1.57f, 2.356f},        /* Player 2: top-right */
        {6.5f, 26.5f, 4.71f, 5.498f},        /* Player 3: bottom-left */
        {35.5f, 6.5f, 1.57f, 2.356f},        /* Player 4: east wing north */
        {35.5f, 26.5f, 4.71f, 5.498f}        /* Player 5: east wing south */
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
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (join_info->players[i].ip[0] != '\0') {
            game_set_player_character(i, g_game.selected_character[i]);
        }
    }
    
    g_game.running = 1;
    
    printf("Game initialized for player %d\n", local_player_id);
    if (g_game.flag_holder >= 0) {
        printf("CTF: %s starts with the flag\n", g_game.player_names[g_game.flag_holder]);
    }
}

void game_shutdown(void) {
    g_game.running = 0;
    reset_character_catalog();
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

    if (g_game.flag_event_timer > 0.0f) {
        g_game.flag_event_timer -= DT;
        if (g_game.flag_event_timer <= 0.0f) {
            g_game.flag_event_timer = 0.0f;
            g_game.flag_event_text[0] = '\0';
            g_game.flag_event_type = 0;
        }
    }

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
                  printf("CTF: %s stole the flag from %s\n",
                      g_game.player_names[new_holder], g_game.player_names[prev_holder]);

                /* Use player names for shared event text */
                char new_name[32];
                char prev_name[32];
                snprintf(new_name, sizeof(new_name), "%s", g_game.player_names[new_holder]);
                snprintf(prev_name, sizeof(prev_name), "%s", g_game.player_names[prev_holder]);
                snprintf(g_game.flag_event_text, sizeof(g_game.flag_event_text),
                         "%s stole the flag from %s", new_name, prev_name);
                if (new_holder == g_game.local_player_id) {
                    g_game.flag_event_type = 1;
                } else if (prev_holder == g_game.local_player_id) {
                    g_game.flag_event_type = -1;
                } else {
                    g_game.flag_event_type = 0;
                }
                g_game.flag_event_timer = 2.2f;
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

void game_set_player_character(int player_id, int character_index) {
    if (player_id < 0 || player_id >= MAX_PLAYERS) {
        return;
    }

    int normalized_index = normalize_character_index(character_index);
    int current_index;

    pthread_mutex_lock(&g_game.player_mutex);
    current_index = g_game.selected_character[player_id];
    if (current_index == normalized_index) {
        pthread_mutex_unlock(&g_game.player_mutex);
        return;
    }
    g_game.selected_character[player_id] = normalized_index;
    pthread_mutex_unlock(&g_game.player_mutex);

    const char *texture_path = g_character_entries[normalized_index].path;
    Texture *tex = texture_clone(g_character_base_textures[normalized_index]);
    if (tex) {
        sprite_set_texture(player_id, tex, 0.8f, 1.6f);
        pthread_mutex_lock(&g_game.player_mutex);
        snprintf(g_game.texture_files[player_id], sizeof(g_game.texture_files[player_id]), "%s", texture_path);
        pthread_mutex_unlock(&g_game.player_mutex);
    } else {
        pthread_mutex_lock(&g_game.player_mutex);
        snprintf(g_game.texture_files[player_id], sizeof(g_game.texture_files[player_id]), "%s (FAILED)", texture_path);
        pthread_mutex_unlock(&g_game.player_mutex);
    }
}

int game_get_player_character(int player_id) {
    int value = 0;
    pthread_mutex_lock(&g_game.player_mutex);
    if (player_id >= 0 && player_id < MAX_PLAYERS) {
        value = g_game.selected_character[player_id];
    }
    pthread_mutex_unlock(&g_game.player_mutex);
    return value;
}

int game_cycle_local_character(int direction) {
    int next_index;
    pthread_mutex_lock(&g_game.player_mutex);
    next_index = g_game.selected_character[g_game.local_player_id] + direction;
    pthread_mutex_unlock(&g_game.player_mutex);

    next_index = normalize_character_index(next_index);
    game_set_player_character(g_game.local_player_id, next_index);
    return next_index;
}

int game_get_character_count(void) {
    return g_character_count;
}

const char *game_get_character_name(int character_index) {
    int idx = normalize_character_index(character_index);
    return g_character_entries[idx].name;
}

const char *game_get_character_texture_path(int character_index) {
    int idx = normalize_character_index(character_index);
    return g_character_entries[idx].path;
}

const Texture *game_get_character_texture(int character_index) {
    int idx = normalize_character_index(character_index);
    return g_character_base_textures[idx];
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

const char *game_get_flag_event_text(void) {
    static char msg[128];
    pthread_mutex_lock(&g_game.player_mutex);
    snprintf(msg, sizeof(msg), "%s", g_game.flag_event_text);
    pthread_mutex_unlock(&g_game.player_mutex);
    return msg;
}

float game_get_flag_event_time_left(void) {
    float t;
    pthread_mutex_lock(&g_game.player_mutex);
    t = g_game.flag_event_timer;
    pthread_mutex_unlock(&g_game.player_mutex);
    return t;
}

int game_get_flag_event_type(void) {
    int type;
    pthread_mutex_lock(&g_game.player_mutex);
    type = g_game.flag_event_type;
    pthread_mutex_unlock(&g_game.player_mutex);
    return type;
}

void game_set_player_name(int player_id, const char *name) {
    if (player_id < 0 || player_id >= MAX_PLAYERS || !name) return;
    pthread_mutex_lock(&g_game.player_mutex);
    snprintf(g_game.player_names[player_id], sizeof(g_game.player_names[player_id]), "%s", name);
    pthread_mutex_unlock(&g_game.player_mutex);
}

const char *game_get_player_name(int player_id) {
    static char tmp[32];
    if (player_id < 0 || player_id >= MAX_PLAYERS) return "(none)";
    pthread_mutex_lock(&g_game.player_mutex);
    snprintf(tmp, sizeof(tmp), "%s", g_game.player_names[player_id]);
    pthread_mutex_unlock(&g_game.player_mutex);
    return tmp;
}
