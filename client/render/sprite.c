#include "sprite.h"
#include "raycaster.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/glut.h>
#include <pthread.h>

SpriteManager g_sprite_manager;

void sprite_manager_init(void) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        g_sprite_manager.sprites[i].player_id = i;
        g_sprite_manager.sprites[i].texture = NULL;
        g_sprite_manager.sprites[i].width = 0.8f;
        g_sprite_manager.sprites[i].height = 1.6f;
    }
    g_sprite_manager.count = MAX_PLAYERS;
}

void sprite_manager_shutdown(void) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (g_sprite_manager.sprites[i].texture) {
            texture_free(g_sprite_manager.sprites[i].texture);
            g_sprite_manager.sprites[i].texture = NULL;
        }
    }
}

void sprite_set_texture(int player_id, Texture *tex, float width, float height) {
    if (player_id >= 0 && player_id < MAX_PLAYERS) {
        if (g_sprite_manager.sprites[player_id].texture) {
            texture_free(g_sprite_manager.sprites[player_id].texture);
        }
        g_sprite_manager.sprites[player_id].texture = tex;
        g_sprite_manager.sprites[player_id].width = width;
        g_sprite_manager.sprites[player_id].height = height;
    }
}

PlayerSprite *sprite_get(int player_id) {
    if (player_id >= 0 && player_id < MAX_PLAYERS) {
        return &g_sprite_manager.sprites[player_id];
    }
    return NULL;
}

void sprite_render_players(const PlayerState *camera) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);

    /* Find all players in field of view and render them */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (g_game.local_player_id == i) {
            continue; /* Don't render yourself */
        }
        
        /* Skip players that didn't connect */
        if (g_game.join_info.players[i].ip[0] == '\0') {
            continue;
        }
        
        PlayerSprite *sprite = &g_sprite_manager.sprites[i];
        if (!sprite->texture) {
            /* Create placeholder if no texture set */
            static Texture *placeholders[MAX_PLAYERS] = {NULL};
            if (!placeholders[i]) {
                unsigned char colors[MAX_PLAYERS][3] = {
                    {255, 0, 0},     /* Red */
                    {0, 255, 0},     /* Green */
                    {0, 0, 255},     /* Blue */
                    {255, 255, 0},   /* Yellow */
                    {255, 0, 255},   /* Magenta */
                    {0, 255, 255}    /* Cyan */
                };
                placeholders[i] = texture_create_placeholder(32, 64, 
                    colors[i][0], colors[i][1], colors[i][2]);
            }
            sprite->texture = placeholders[i];
        }
        
        /* Get other player position safely */
        float other_x, other_y;
        pthread_mutex_lock(&g_game.player_mutex);
        other_x = g_game.players[i].x;
        other_y = g_game.players[i].y;
        pthread_mutex_unlock(&g_game.player_mutex);
        
        /* Calculate vector from camera to sprite */
        float dx = other_x - camera->x;
        float dy = other_y - camera->y;
        float dist_sq = dx * dx + dy * dy;
        
        if (dist_sq < 0.1f || dist_sq > 400.0f) {
            continue; /* Too close or too far */
        }
        
        float dist = sqrtf(dist_sq);
        
        /* Get angle to sprite */
        float angle_to_sprite = atan2f(dy, dx);
        float angle_diff = angle_to_sprite - camera->angle;
        
        /* Normalize angle to [-pi, pi] */
        while (angle_diff > 3.14159f) angle_diff -= 6.28318f;
        while (angle_diff < -3.14159f) angle_diff += 6.28318f;
        
        /* Check if in FOV */
        float fov_half = camera->fov / 2.0f;
        if (fabsf(angle_diff) > fov_half) {
            continue;
        }
        
        /* Calculate screen position from angle relative to FOV */
        float screen_x_center = WIDTH * (0.5f + (angle_diff / camera->fov));
        
        /* Calculate sprite size based on distance */
        float sprite_screen_height = (sprite->height / dist) * (float)WIDTH / 2.0f;
        float sprite_screen_width = (sprite->width / dist) * (float)WIDTH / 2.0f;
        
        float screen_x = screen_x_center - sprite_screen_width / 2.0f;
        float screen_y = (HEIGHT - sprite_screen_height) / 2.0f;
        
        if (screen_x + sprite_screen_width < 0 || screen_x > WIDTH) {
            continue; /* Off screen horizontally */
        }
        
        Texture *tex = sprite->texture;
        if (!tex || !tex->pixels || texture_bind_gl(tex) != 0) {
            continue;
        }

        int x_start = (int)floorf(screen_x);
        int x_end = (int)ceilf(screen_x + sprite_screen_width) - 1;
        if (x_start < 0) x_start = 0;
        if (x_end >= WIDTH) x_end = WIDTH - 1;
        if (x_start > x_end || sprite_screen_width <= 0.001f) {
            continue;
        }

        const float inv_w = 1.0f / sprite_screen_width;

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
        for (int sx = x_start; sx <= x_end; ++sx) {
            float wall_dist = raycaster_get_depth_at_column(sx);
            if (dist > wall_dist + 0.02f) {
                continue;
            }

            float x1 = (float)sx;
            float x2 = (float)(sx + 1);
            float u1 = (x1 - screen_x) * inv_w;
            float u2 = (x2 - screen_x) * inv_w;

            if (u2 <= 0.0f || u1 >= 1.0f) {
                continue;
            }

            if (u1 < 0.0f) u1 = 0.0f;
            if (u2 > 1.0f) u2 = 1.0f;

            glTexCoord2f(u1, 0.0f); glVertex2f(x1, screen_y);
            glTexCoord2f(u2, 0.0f); glVertex2f(x2, screen_y);
            glTexCoord2f(u2, 1.0f); glVertex2f(x2, screen_y + sprite_screen_height);
            glTexCoord2f(u1, 1.0f); glVertex2f(x1, screen_y + sprite_screen_height);
        }
        glEnd();

        /* Highlight flag holder with a yellow border */
        if (i == game_get_flag_holder()) {
            glDisable(GL_TEXTURE_2D);
            glLineWidth(2.5f);
            glColor3f(1.0f, 0.95f, 0.15f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(screen_x - 2.0f, screen_y - 2.0f);
            glVertex2f(screen_x + sprite_screen_width + 2.0f, screen_y - 2.0f);
            glVertex2f(screen_x + sprite_screen_width + 2.0f, screen_y + sprite_screen_height + 2.0f);
            glVertex2f(screen_x - 2.0f, screen_y + sprite_screen_height + 2.0f);
            glEnd();
            glLineWidth(1.0f);
            glEnable(GL_TEXTURE_2D);
        }
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}
