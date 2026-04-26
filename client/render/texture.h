#ifndef TEXTURE_H
#define TEXTURE_H

typedef struct {
    unsigned char *pixels;  /* RGBA format */
    int width;
    int height;
    int id;                /* OpenGL texture ID */
} Texture;

/* Load a PPM image file */
Texture *texture_load_ppm(const char *filepath);

/* Load a simple placeholder texture (colored square) */
Texture *texture_create_placeholder(int width, int height, unsigned char r, unsigned char g, unsigned char b);
Texture *texture_clone(const Texture *src);

/* Upload texture to GPU (lazy) and bind to GL_TEXTURE_2D. Returns 0 on success. */
int texture_bind_gl(Texture *tex);

void texture_free(Texture *tex);

/* Get pixel at position (safe bounds checking) */
void texture_get_pixel(Texture *tex, int x, int y, unsigned char *r, unsigned char *g, unsigned char *b, unsigned char *a);

#endif
