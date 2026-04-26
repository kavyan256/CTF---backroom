#ifndef RAYCASTER_H
#define RAYCASTER_H

#include "../simulation/level.h"
#include "../core/game.h"

typedef struct {
    float dist;       /* Distance from player */
    int wall_type;    /* WALL_BRICK, WALL_STONE, etc */
    float hit_x, hit_y; /* World coords of hit */
} RayHit;

void raycaster_render(const PlayerState *player);
int raycaster_cast_ray(const PlayerState *player, float angle, RayHit *out);
float raycaster_get_depth_at_column(int x);

#endif
