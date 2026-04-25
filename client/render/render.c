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
    draw_text(40.0f, 90.0f, GLUT_BITMAP_HELVETICA_12, "Connected players (press R to toggle READY):");

    int y = 130;
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!game_get_connected_player(i)) {
            continue;
        }

        const int is_local = (i == g_game.local_player_id);
        const int is_ready = game_get_player_ready(i);
        char line[128];
        snprintf(line, sizeof(line), "Player %d%s  -  %s", i, is_local ? " (You)" : "", is_ready ? "READY" : "NOT READY");

        if (is_ready) {
            glColor3f(0.4f, 1.0f, 0.5f);
        } else {
            glColor3f(1.0f, 0.55f, 0.45f);
        }
        draw_text(60.0f, (float)y, GLUT_BITMAP_HELVETICA_18, line);
        y += 30;
    }

    glColor3f(0.9f, 0.9f, 0.9f);
    if (game_get_local_ready()) {
        draw_text(40.0f, (float)(HEIGHT - 60), GLUT_BITMAP_HELVETICA_18, "You are READY. Waiting for others...");
    } else {
        draw_text(40.0f, (float)(HEIGHT - 60), GLUT_BITMAP_HELVETICA_18, "You are NOT READY. Press R when ready.");
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