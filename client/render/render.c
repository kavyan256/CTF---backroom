#include "render.h"
#include "raycaster.h"
#include "sprite.h"

#include <GL/glut.h>
#include <math.h>
#include <stdio.h>

#include "../config.h"
#include "../core/game.h"
#include "../simulation/level.h"

static void draw_text(float x, float y, void *font, const char *text) {
    glRasterPos2f(x, y);
    for (const char *c = text; *c; ++c) {
        glutBitmapCharacter(font, *c);
    }
}

static void draw_texture_preview(Texture *tex, float x, float y, float w, float h, int max_samples_x, int max_samples_y) {
    glColor3f(0.18f, 0.20f, 0.26f);
    glBegin(GL_QUADS);
    glVertex2f(x - 6.0f, y - 6.0f);
    glVertex2f(x + w + 6.0f, y - 6.0f);
    glVertex2f(x + w + 6.0f, y + h + 6.0f);
    glVertex2f(x - 6.0f, y + h + 6.0f);
    glEnd();

    if (!tex || !tex->pixels || tex->width <= 0 || tex->height <= 0) {
        glColor3f(0.75f, 0.30f, 0.30f);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x, y + h);
        glEnd();
        return;
    }

    int samples_x = tex->width;
    int samples_y = tex->height;
    if (max_samples_x > 0 && samples_x > max_samples_x) {
        samples_x = max_samples_x;
    }
    if (max_samples_y > 0 && samples_y > max_samples_y) {
        samples_y = max_samples_y;
    }

    if (samples_x <= 0) {
        samples_x = 1;
    }
    if (samples_y <= 0) {
        samples_y = 1;
    }

    const float src_step_x = (float)tex->width / (float)samples_x;
    const float src_step_y = (float)tex->height / (float)samples_y;
    const float px_w = w / (float)samples_x;
    const float px_h = h / (float)samples_y;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBegin(GL_QUADS);
    for (int ty = 0; ty < samples_y; ++ty) {
        int src_y = (int)((float)ty * src_step_y);
        if (src_y >= tex->height) {
            src_y = tex->height - 1;
        }
        for (int tx = 0; tx < samples_x; ++tx) {
            int src_x = (int)((float)tx * src_step_x);
            if (src_x >= tex->width) {
                src_x = tex->width - 1;
            }
            unsigned char r, g, b, a;
            texture_get_pixel(tex, src_x, src_y, &r, &g, &b, &a);
            if (a < 20) {
                continue;
            }

            const float x1 = x + tx * px_w;
            const float y1 = y + ty * px_h;
            const float x2 = x1 + px_w;
            const float y2 = y1 + px_h;

            glColor4ub(r, g, b, a);
            glColor4ub(r, g, b, a);
            glVertex2f(x1, y1);
            glVertex2f(x2, y1);
            glVertex2f(x2, y2);
            glVertex2f(x1, y2);
        }
    }
    glEnd();

    glDisable(GL_BLEND);
}

static Texture *get_character_grid_texture(int character_index) {
    return (Texture *)game_get_character_texture(character_index);
}

static void draw_character_grid(float y) {
    int count = game_get_character_count();
    if (count <= 0) {
        return;
    }

    const float cell_w = 46.0f;
    const float cell_h = 62.0f;
    const float gap = 8.0f;
    const float total_w = (count * cell_w) + ((count - 1) * gap);
    const float start_x = ((float)WIDTH - total_w) * 0.5f;
    const int selected = game_get_player_character(g_game.local_player_id);

    glColor3f(0.08f, 0.10f, 0.14f);
    glBegin(GL_QUADS);
    glVertex2f(start_x - 18.0f, y - 14.0f);
    glVertex2f(start_x + total_w + 18.0f, y - 14.0f);
    glVertex2f(start_x + total_w + 18.0f, y + cell_h + 14.0f);
    glVertex2f(start_x - 18.0f, y + cell_h + 14.0f);
    glEnd();

    for (int i = 0; i < count; ++i) {
        float x = start_x + i * (cell_w + gap);
        Texture *tex = get_character_grid_texture(i);
        draw_texture_preview(tex, x, y, cell_w, cell_h, 24, 32);

        if (i == selected) {
            glLineWidth(3.0f);
            glColor3f(1.0f, 0.92f, 0.30f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(x - 8.0f, y - 8.0f);
            glVertex2f(x + cell_w + 8.0f, y - 8.0f);
            glVertex2f(x + cell_w + 8.0f, y + cell_h + 8.0f);
            glVertex2f(x - 8.0f, y + cell_h + 8.0f);
            glEnd();
            glLineWidth(1.0f);
        }
    }
}

static void render_lobby_screen(void) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0, (double)WIDTH, (double)HEIGHT, 0.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor3f(0.10f, 0.13f, 0.18f);
    glBegin(GL_QUADS);
    glVertex2f(0.0f, 0.0f);
    glVertex2f((float)WIDTH, 0.0f);
    glVertex2f((float)WIDTH, (float)HEIGHT);
    glVertex2f(0.0f, (float)HEIGHT);
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
    draw_text(40.0f, 60.0f, GLUT_BITMAP_HELVETICA_18, "CGV Lobby");
    draw_text(40.0f, 90.0f, GLUT_BITMAP_HELVETICA_12, "Mortal Kombat style selection: Q/E to choose fighter, R to READY");

    PlayerSprite *local_sprite = sprite_get(g_game.local_player_id);
    draw_text(40.0f, 130.0f, GLUT_BITMAP_HELVETICA_12, "Your texture preview:");
    draw_texture_preview(local_sprite ? local_sprite->texture : NULL, 40.0f, 145.0f, 120.0f, 180.0f, 56, 84);

    {
        char texture_label[196];
        snprintf(texture_label, sizeof(texture_label), "Texture: %s", g_game.texture_files[g_game.local_player_id]);
        glColor3f(0.85f, 0.85f, 0.90f);
        draw_text(40.0f, 345.0f, GLUT_BITMAP_HELVETICA_12, texture_label);
    }

    int y = 145;
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!game_get_connected_player(i)) {
            continue;
        }

        const int is_local = (i == g_game.local_player_id);
        const int is_ready = game_get_player_ready(i);
        const char *name = game_get_player_name(i);
        char line[128];
        snprintf(line, sizeof(line), "%s%s  -  %s", name, is_local ? " (You)" : "", is_ready ? "READY" : "NOT READY");

        if (is_ready) {
            glColor3f(0.4f, 1.0f, 0.5f);
        } else {
            glColor3f(1.0f, 0.55f, 0.45f);
        }
        draw_text(220.0f, (float)y, GLUT_BITMAP_HELVETICA_18, line);
        y += 30;
    }

    draw_character_grid((float)(HEIGHT - 96));

    glColor3f(0.9f, 0.9f, 0.9f);
    if (game_get_local_ready()) {
        draw_text(40.0f, (float)(HEIGHT - 172), GLUT_BITMAP_HELVETICA_18, "You are READY. Waiting for others...");
    } else {
        draw_text(40.0f, (float)(HEIGHT - 172), GLUT_BITMAP_HELVETICA_18, "You are NOT READY. Press R when ready.");
    }
}

void render_init(void) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0, (double)WIDTH, 0.0, (double)HEIGHT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClearColor(0.06f, 0.08f, 0.10f, 1.0f);
    
    /* Initialize level */
    level_init();
}

void render_scene(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();

    if (!game_has_started()) {
        render_lobby_screen();
    } else {
        /* Render first-person view using raycasting */
        raycaster_render(&g_game.local_player);
    }
    
    glutSwapBuffers();
}