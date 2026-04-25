#include "input.h"

#include <GL/glut.h>
#include <stdlib.h>
#include <string.h>

static PlayerInput g_input;
static volatile int g_ready_toggle_requested = 0;

static void on_key_down(unsigned char key, int x, int y) {
    (void)x;
    (void)y;
    
    switch (key) {
        case 'w': case 'W': g_input.forward = 1; break;
        case 's': case 'S': g_input.backward = 1; break;
        case 'a': case 'A': g_input.strafe_left = 1; break;
        case 'd': case 'D': g_input.strafe_right = 1; break;
        case 'r': case 'R': g_ready_toggle_requested = 1; break;
        case 27: /* ESC */ exit(0); break;
    }
}

static void on_key_up(unsigned char key, int x, int y) {
    (void)x;
    (void)y;
    
    switch (key) {
        case 'w': case 'W': g_input.forward = 0; break;
        case 's': case 'S': g_input.backward = 0; break;
        case 'a': case 'A': g_input.strafe_left = 0; break;
        case 'd': case 'D': g_input.strafe_right = 0; break;
    }
}

static void on_special_down(int key, int x, int y) {
    (void)x;
    (void)y;
    switch (key) {
        case GLUT_KEY_LEFT: g_input.turn_left = 1; break;
        case GLUT_KEY_RIGHT: g_input.turn_right = 1; break;
    }
}

static void on_special_up(int key, int x, int y) {
    (void)x;
    (void)y;
    switch (key) {
        case GLUT_KEY_LEFT: g_input.turn_left = 0; break;
        case GLUT_KEY_RIGHT: g_input.turn_right = 0; break;
    }
}

PlayerInput *input_get_current(void) {
    return &g_input;
}

int input_consume_ready_toggle(void) {
    if (g_ready_toggle_requested) {
        g_ready_toggle_requested = 0;
        return 1;
    }
    return 0;
}

void input_init_callbacks(void) {
    memset(&g_input, 0, sizeof(g_input));
    
    glutKeyboardFunc(on_key_down);
    glutKeyboardUpFunc(on_key_up);
    glutSpecialFunc(on_special_down);
    glutSpecialUpFunc(on_special_up);
}

void input_apply_forces(void) {
    /* Not used in FPS mode */
}
