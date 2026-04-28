#include "raycaster.h"
#include "sprite.h"
#include <GL/glut.h>
#include <math.h>
#include <stdio.h>
#include <pthread.h>
#include "../config.h"
#include "../simulation/level.h"

#define RAY_STEP 0.1f
#define MAX_RAY_DIST 100.0f

static float g_depth_buffer[WIDTH];

float raycaster_get_depth_at_column(int x) {
    if (x < 0 || x >= WIDTH) {
        return MAX_RAY_DIST;
    }
    return g_depth_buffer[x];
}

static void draw_wall_slice(int x, float dist, int wall_type) {
    if (dist <= 0.01f) return;
    
    /* Perspective correction and wall height */
    float wall_height = (float)HEIGHT / dist;
    float y_top = (HEIGHT - wall_height) / 2.0f;
    float y_bot = (HEIGHT + wall_height) / 2.0f;
    
    /* Color based on wall type and distance */
    float brightness = 1.0f / (1.0f + dist * 0.04f);  /* Slightly brighter */
    
    if (wall_type == WALL_BRICK) {
        glColor3f(0.9f * brightness, 0.5f * brightness, 0.3f * brightness);  /* Brighter brick */
    } else if (wall_type == WALL_STONE) {
        glColor3f(0.7f * brightness, 0.7f * brightness, 0.7f * brightness);  /* Brighter stone */
    } else {
        glColor3f(0.8f * brightness, 0.6f * brightness, 0.3f * brightness);  /* Brighter wood */
    }
    
    glBegin(GL_QUADS);
    glVertex2f(x, y_top);
    glVertex2f(x + 1, y_top);
    glVertex2f(x + 1, y_bot);
    glVertex2f(x, y_bot);
    glEnd();
}

int raycaster_cast_ray(const PlayerState *player, float angle, RayHit *out) {
    float step_x = cosf(angle) * RAY_STEP;
    float step_y = sinf(angle) * RAY_STEP;
    
    float x = player->x;
    float y = player->y;
    float dist = 0.0f;
    
    while (dist < MAX_RAY_DIST) {
        x += step_x;
        y += step_y;
        dist += RAY_STEP;
        
        int wall = level_get_wall((int)x, (int)y);
        if (wall != WALL_NONE) {
            out->dist = dist;
            out->wall_type = wall;
            out->hit_x = x;
            out->hit_y = y;
            return 1;
        }
    }
    
    return 0;
}

void raycaster_render(const PlayerState *player) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, WIDTH, HEIGHT, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glDisable(GL_TEXTURE_2D);
    
    /* Cast rays for each screen column */
    for (int x = 0; x < WIDTH; x++) {
        /* Calculate ray angle */
        float ray_offset = (float)x / (float)WIDTH - 0.5f;
        float ray_angle = player->angle + ray_offset * player->fov;
        
        RayHit hit;
        if (raycaster_cast_ray(player, ray_angle, &hit)) {
            g_depth_buffer[x] = hit.dist;
            draw_wall_slice(x, hit.dist, hit.wall_type);
        } else {
            g_depth_buffer[x] = MAX_RAY_DIST;
            /* Sky */
            glColor3f(0.2f, 0.3f, 0.5f);
            glBegin(GL_QUADS);
            glVertex2f(x, 0);
            glVertex2f(x + 1, 0);
            glVertex2f(x + 1, HEIGHT / 2.0f);
            glVertex2f(x, HEIGHT / 2.0f);
            glEnd();
            
            /* Floor */
            glColor3f(0.3f, 0.25f, 0.1f);
            glBegin(GL_QUADS);
            glVertex2f(x, HEIGHT / 2.0f);
            glVertex2f(x + 1, HEIGHT / 2.0f);
            glVertex2f(x + 1, HEIGHT);
            glVertex2f(x, HEIGHT);
            glEnd();
        }
    }
    
    /* Draw crosshair */
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_LINES);
    glVertex2f(WIDTH / 2.0f - 5, HEIGHT / 2.0f);
    glVertex2f(WIDTH / 2.0f + 5, HEIGHT / 2.0f);
    glVertex2f(WIDTH / 2.0f, HEIGHT / 2.0f - 5);
    glVertex2f(WIDTH / 2.0f, HEIGHT / 2.0f + 5);
    glEnd();
    
    /* Draw on-screen info */
    glColor3f(1.0f, 1.0f, 0.0f);
    glRasterPos2f(10, 20);
    char buf[128];
    snprintf(buf, sizeof(buf), "Player %d | Pos: (%.1f, %.1f) | Angle: %.2f", 
             g_game.local_player_id, player->x, player->y, player->angle);
    for (char *c = buf; *c; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }

    /* Capture-the-Flag HUD */
    int flag_holder = game_get_flag_holder();
    float flag_cd = game_get_flag_cooldown();
    char ctf_buf[128];
    snprintf(ctf_buf, sizeof(ctf_buf), "FLAG HOLDER: P%d | Steal cooldown: %.1fs", flag_holder, flag_cd);
    glColor3f(1.0f, 0.9f, 0.2f);
    glRasterPos2f(10, 94);
    for (char *c = ctf_buf; *c; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }

    if (flag_holder == g_game.local_player_id) {
        const char *owned_msg = "YOU HAVE THE FLAG";
        glColor3f(1.0f, 0.95f, 0.25f);
        glRasterPos2f((float)(WIDTH / 2 - 90), 74.0f);
        for (const char *c = owned_msg; *c; ++c) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
        }
    }

    /* Flag capture/loss toast for local awareness */
    {
        float evt_t = game_get_flag_event_time_left();
        const char *evt = game_get_flag_event_text();
        if (evt_t > 0.0f && evt[0] != '\0') {
            int evt_type = game_get_flag_event_type();
            if (evt_type > 0) {
                glColor3f(0.35f, 1.0f, 0.45f); /* Captured */
            } else if (evt_type < 0) {
                glColor3f(1.0f, 0.35f, 0.35f); /* Lost */
            } else {
                glColor3f(1.0f, 1.0f, 0.85f); /* Neutral */
            }

            glRasterPos2f((float)(WIDTH / 2 - 110), 46.0f);
            for (const char *c = evt; *c; ++c) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
            }
        }
    }

    /* Ranking by time holding flag */
    int rank_ids[MAX_PLAYERS];
    float rank_times[MAX_PLAYERS];
    int rank_count = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (g_game.join_info.players[i].ip[0] == '\0') {
            continue;
        }
        rank_ids[rank_count] = i;
        rank_times[rank_count] = game_get_flag_hold_time(i);
        rank_count++;
    }
    for (int i = 0; i < rank_count; i++) {
        for (int j = i + 1; j < rank_count; j++) {
            if (rank_times[j] > rank_times[i]) {
                float tf = rank_times[i];
                int ti = rank_ids[i];
                rank_times[i] = rank_times[j];
                rank_ids[i] = rank_ids[j];
                rank_times[j] = tf;
                rank_ids[j] = ti;
            }
        }
    }

    glColor3f(0.9f, 0.9f, 1.0f);
    glRasterPos2f(10, 110);
    for (char *c = "CTF Ranking:"; *c; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
    for (int r = 0; r < rank_count; r++) {
        char rank_line[128];
        int pid = rank_ids[r];
        int steals = game_get_flag_steals(pid);
        if (pid == flag_holder) {
            snprintf(rank_line, sizeof(rank_line), "%d) P%d  %.1fs  steals:%d  [FLAG]", r + 1, pid, rank_times[r], steals);
        } else {
            snprintf(rank_line, sizeof(rank_line), "%d) P%d  %.1fs  steals:%d", r + 1, pid, rank_times[r], steals);
        }
        glRasterPos2f(10, (float)(126 + r * 14));
        for (char *c = rank_line; *c; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
        }
    }

    /* Render minimap in top-right corner */
    {
        const float minimap_size = 100.0f;
        const float minimap_x = WIDTH - minimap_size - 10.0f;
        const float minimap_y = 10.0f;
        const float scale = minimap_size / (float)((LEVEL_WIDTH > LEVEL_HEIGHT) ? LEVEL_WIDTH : LEVEL_HEIGHT);  /* Scale factor for map tiles */
        
        /* Draw minimap background */
        glColor3f(0.1f, 0.1f, 0.1f);
        glBegin(GL_QUADS);
        glVertex2f(minimap_x, minimap_y);
        glVertex2f(minimap_x + minimap_size, minimap_y);
        glVertex2f(minimap_x + minimap_size, minimap_y + minimap_size);
        glVertex2f(minimap_x, minimap_y + minimap_size);
        glEnd();
        
        /* Draw walls on minimap */
        for (int x = 0; x < LEVEL_WIDTH; x++) {
            for (int y = 0; y < LEVEL_HEIGHT; y++) {
                int wall = level_get_wall(x, y);
                if (wall != WALL_NONE) {
                    if (wall == WALL_BRICK) {
                        glColor3f(0.8f, 0.4f, 0.2f);
                    } else if (wall == WALL_STONE) {
                        glColor3f(0.5f, 0.5f, 0.5f);
                    } else {
                        glColor3f(0.6f, 0.4f, 0.2f);
                    }
                    
                    glBegin(GL_QUADS);
                    glVertex2f(minimap_x + x * scale, minimap_y + y * scale);
                    glVertex2f(minimap_x + (x+1) * scale, minimap_y + y * scale);
                    glVertex2f(minimap_x + (x+1) * scale, minimap_y + (y+1) * scale);
                    glVertex2f(minimap_x + x * scale, minimap_y + (y+1) * scale);
                    glEnd();
                }
            }
        }
        
        /* Draw current player on minimap */
        glColor3f(1.0f, 1.0f, 0.0f);
        float px = minimap_x + player->x * scale;
        float py = minimap_y + player->y * scale;
        glBegin(GL_QUADS);
        glVertex2f(px - 2, py - 2);
        glVertex2f(px + 2, py - 2);
        glVertex2f(px + 2, py + 2);
        glVertex2f(px - 2, py + 2);
        glEnd();
        
        /* Draw other players on minimap */
        pthread_mutex_lock(&g_game.player_mutex);
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (i == g_game.local_player_id || g_game.join_info.players[i].ip[0] == '\0') {
                continue;
            }
            glColor3f(1.0f, 0.0f, 0.0f);
            float ox = minimap_x + g_game.players[i].x * scale;
            float oy = minimap_y + g_game.players[i].y * scale;
            glBegin(GL_QUADS);
            glVertex2f(ox - 2, oy - 2);
            glVertex2f(ox + 2, oy - 2);
            glVertex2f(ox + 2, oy + 2);
            glVertex2f(ox - 2, oy + 2);
            glEnd();
        }
        pthread_mutex_unlock(&g_game.player_mutex);

        /* Draw flag holder marker on minimap */
        if (flag_holder >= 0 && flag_holder < MAX_PLAYERS && g_game.join_info.players[flag_holder].ip[0] != '\0') {
            glColor3f(0.0f, 1.0f, 1.0f);
            float fx = minimap_x + g_game.players[flag_holder].x * scale;
            float fy = minimap_y + g_game.players[flag_holder].y * scale;
            glBegin(GL_QUADS);
            glVertex2f(fx - 3, fy - 3);
            glVertex2f(fx + 3, fy - 3);
            glVertex2f(fx + 3, fy + 3);
            glVertex2f(fx - 3, fy + 3);
            glEnd();
        }
        
        /* Draw minimap border */
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(minimap_x, minimap_y);
        glVertex2f(minimap_x + minimap_size, minimap_y);
        glVertex2f(minimap_x + minimap_size, minimap_y + minimap_size);
        glVertex2f(minimap_x, minimap_y + minimap_size);
        glEnd();
    }
    
    /* Render other players as billboarded sprites */
    sprite_render_players(player);
    
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}
