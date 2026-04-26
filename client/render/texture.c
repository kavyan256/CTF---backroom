#include "texture.h"
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Texture *texture_load_ppm(const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        printf("Warning: Could not open texture file %s\n", filepath);
        return NULL;
    }

    char magic[3];
    if (fgets(magic, 3, f) == NULL) {
        fclose(f);
        return NULL;
    }

    if (magic[0] != 'P' || (magic[1] != '6' && magic[1] != '5')) {
        printf("Error: Not a PPM file (P5 or P6 format required)\n");
        fclose(f);
        return NULL;
    }

    int width, height, maxval;
    if (fscanf(f, "%d %d %d", &width, &height, &maxval) != 3) {
        printf("Error: Invalid PPM header\n");
        fclose(f);
        return NULL;
    }

    /* Skip whitespace after maxval */
    fgetc(f);

    Texture *tex = malloc(sizeof(Texture));
    if (!tex) {
        fclose(f);
        return NULL;
    }

    tex->width = width;
    tex->height = height;
    tex->id = 0;
    tex->pixels = malloc(width * height * 4);

    if (!tex->pixels) {
        free(tex);
        fclose(f);
        return NULL;
    }

    /* Read PPM data and convert to RGBA */
    unsigned char *rgb_data = malloc(width * height * 3);
    if (fread(rgb_data, 1, width * height * 3, f) != (size_t)(width * height * 3)) {
        printf("Error: Could not read PPM pixel data\n");
        free(rgb_data);
        free(tex->pixels);
        free(tex);
        fclose(f);
        return NULL;
    }

    /* Convert RGB to RGBA */
    for (int i = 0; i < width * height; i++) {
        tex->pixels[i * 4 + 0] = rgb_data[i * 3 + 0];
        tex->pixels[i * 4 + 1] = rgb_data[i * 3 + 1];
        tex->pixels[i * 4 + 2] = rgb_data[i * 3 + 2];
        tex->pixels[i * 4 + 3] = 255;
    }

    free(rgb_data);
    fclose(f);

    printf("Loaded texture: %s (%dx%d)\n", filepath, width, height);
    return tex;
}

Texture *texture_create_placeholder(int width, int height, unsigned char r, unsigned char g, unsigned char b) {
    Texture *tex = malloc(sizeof(Texture));
    if (!tex) return NULL;

    tex->width = width;
    tex->height = height;
    tex->id = 0;
    tex->pixels = malloc(width * height * 4);

    if (!tex->pixels) {
        free(tex);
        return NULL;
    }

    for (int i = 0; i < width * height; i++) {
        tex->pixels[i * 4 + 0] = r;
        tex->pixels[i * 4 + 1] = g;
        tex->pixels[i * 4 + 2] = b;
        tex->pixels[i * 4 + 3] = 255;
    }

    return tex;
}

Texture *texture_clone(const Texture *src) {
    if (!src || !src->pixels || src->width <= 0 || src->height <= 0) {
        return NULL;
    }

    Texture *clone = malloc(sizeof(Texture));
    if (!clone) {
        return NULL;
    }

    clone->width = src->width;
    clone->height = src->height;
    clone->id = 0;

    size_t bytes = (size_t)src->width * (size_t)src->height * 4u;
    clone->pixels = malloc(bytes);
    if (!clone->pixels) {
        free(clone);
        return NULL;
    }

    memcpy(clone->pixels, src->pixels, bytes);
    return clone;
}

int texture_bind_gl(Texture *tex) {
    if (!tex || !tex->pixels || tex->width <= 0 || tex->height <= 0) {
        return -1;
    }

    if (tex->id == 0) {
        GLuint gl_id = 0;
        glGenTextures(1, &gl_id);
        if (gl_id == 0) {
            return -1;
        }
        tex->id = (int)gl_id;

        glBindTexture(GL_TEXTURE_2D, gl_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            tex->width,
            tex->height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            tex->pixels
        );
    } else {
        glBindTexture(GL_TEXTURE_2D, (GLuint)tex->id);
    }

    return 0;
}

void texture_free(Texture *tex) {
    if (!tex) return;
    if (tex->id != 0) {
        GLuint gl_id = (GLuint)tex->id;
        glDeleteTextures(1, &gl_id);
        tex->id = 0;
    }
    if (tex->pixels) free(tex->pixels);
    free(tex);
}

void texture_get_pixel(Texture *tex, int x, int y, unsigned char *r, unsigned char *g, unsigned char *b, unsigned char *a) {
    if (!tex || !tex->pixels || x < 0 || x >= tex->width || y < 0 || y >= tex->height) {
        *r = *g = *b = 128;
        *a = 0;
        return;
    }

    int idx = (y * tex->width + x) * 4;
    *r = tex->pixels[idx + 0];
    *g = tex->pixels[idx + 1];
    *b = tex->pixels[idx + 2];
    *a = tex->pixels[idx + 3];
}
