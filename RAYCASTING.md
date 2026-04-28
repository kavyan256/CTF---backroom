# Ray Casting in This Project

This project uses ray casting to draw the first-person view in the client. The core implementation lives in `client/render/raycaster.c`.

## What ray casting means
Ray casting is a technique where you imagine a ray shooting out from the player into the world. By checking what that ray hits, you can estimate what the player should see in front of them.

In a 2D map, this is often used to fake a 3D view:
- walls closer to the player appear taller
- walls farther away appear shorter
- each screen column gets its own ray

## How it works here
The renderer does the following each frame:

1. **Take the local player position and angle**
   - The camera is `g_game.local_player`.
   - Its `x`, `y`, `angle`, and `fov` define where the view starts and what direction it faces.

2. **Cast one ray per screen column**
   - For each `x` on the screen, the code computes a ray angle.
   - That angle is based on the player’s facing direction plus a small offset across the field of view.

3. **March forward until a wall is hit**
   - `raycaster_cast_ray(...)` moves along the ray in small steps.
   - At each step it checks `level_get_wall((int)x, (int)y)`.
   - When the ray enters a wall cell, the ray stops.

4. **Convert distance into wall height**
   - The hit distance is used to calculate a vertical slice height.
   - Closer walls produce taller slices.
   - Farther walls produce shorter slices.

5. **Store depth information**
   - Each column’s distance is saved in `g_depth_buffer`.
   - This is used later so sprites can be hidden behind walls correctly.

6. **Draw the rest of the frame**
   - If a ray does not hit a wall, the code draws sky and floor for that column.
   - The HUD, minimap, and sprites are drawn on top.

## Why this works
Even though the world is represented as a 2D grid, ray casting makes it feel like a 3D scene because:
- each screen column gets a depth value
- wall height changes with distance
- the player sees a perspective-correct corridor or room layout

## Why this project uses it
This project is a multiplayer FPS prototype, so ray casting gives a simple and fast way to:
- render the map walls
- show the player’s viewpoint
- support sprite occlusion with the depth buffer
- keep the rendering logic straightforward

## Important functions
- `raycaster_render(const PlayerState *player)`
  - Main rendering entry point for the first-person view.
- `raycaster_cast_ray(const PlayerState *player, float angle, RayHit *out)`
  - Steps a ray through the map until it hits a wall.
- `raycaster_get_depth_at_column(int x)`
  - Returns the stored wall distance for one screen column.

## How sprites use it
`client/render/sprite.c` calls `raycaster_get_depth_at_column(...)` to decide whether a player sprite is in front of or behind a wall. This is what makes remote players disappear when a wall blocks them.

## Summary
In short, this project uses classic per-column ray casting to turn a 2D tile map into a pseudo-3D first-person scene.
