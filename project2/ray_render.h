#ifndef RAY_RENDER_H__
#define RAY_RENDER_H__

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "ray_ast.h"

#define DEFINE_FRAMEBUFFER(t, n)								\
struct framebuffer_##t {									\
	int width;										\
	int height;										\
	int channels;										\
	t *pixels;										\
};												\
												\
static inline struct framebuffer_##t *new_framebuffer_##t(int width, int height) {		\
	struct framebuffer_##t *ret = malloc(sizeof(*ret) + sizeof(t) * n * width * height);	\
	ret->width = width;									\
	ret->height = height;									\
	ret->channels = n;									\
	ret->pixels = (void*)(ret + 1);								\
	return ret;										\
}												\
												\
static inline void free_framebuffer_##t(struct framebuffer_##t *fb) {				\
	free(fb);										\
}												\
												\
static inline int framebuffer_##t##_pixel_index(struct framebuffer_##t *fb, int x, int y) {	\
	return n * (y * fb->width + x);								\
}												\
												\
static inline void framebuffer_##t##_set(struct framebuffer_##t *fb, int x, int y, t px) {	\
	fb->pixels[framebuffer_##t##_pixel_index(fb, x, y)] = px;				\
}												\
												\
static inline t* framebuffer_##t##_get(struct framebuffer_##t *fb, int x, int y) {		\
	return &fb->pixels[framebuffer_##t##_pixel_index(fb, x, y)];				\
}

//DEFINE_FRAMEBUFFER(uint8_t)	// for the rgb data before export
DEFINE_FRAMEBUFFER(pt4, 1)	// for the actual rendering passes

// void render_scene(struct framebuffer_pt4 *fb, const struct context* ctx);

static inline uint8_t color_double_to_u8(double d) {
	if (d < 0) return 0;
	if (d >= 1) return 255;
	return (uint8_t)(d * 255);
}

// @caleb custom function and struct declartions
struct Console_Disk_Args {
	struct framebuffer_pt4* fb_curr;
	struct framebuffer_pt4* fb_prev;
	struct context* ctx;
	int render_to_console;
	int frame;
	char** argv;
};

void render_scene(struct framebuffer_pt4 *fb, const struct context *ctx, int lower_x_bound, int upper_x_bound);

void *render_console_or_disk(void * args);
void *update_physics(void * _ctx);
void *update_render_col(void *_args);

// @chris' structs
typedef struct subset_info {
	int col_num;
    struct framebuffer_pt4 *fb_curr;
    struct framebuffer_pt4 *fb_prev;
    const struct context *ctx;
    int lower_x_bound;
    int upper_x_bound;
} subset_info;



#endif	// RAY_RENDER_H__

