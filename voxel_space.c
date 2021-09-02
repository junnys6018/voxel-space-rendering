#include "emscripten.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define SCALE 0.65
#define DISTANCE 512
#define HORIZON 0.4
#define FOV_SCALE 0.8

typedef struct
{
	int cw, ch;
	uint32_t *color_map;
	int dw, dh;
	uint8_t *depth_map;

	int out_w, out_h;
	uint32_t *output;

	uint32_t background;
	float start_x, start_y;
} voxel_space_context;

EMSCRIPTEN_KEEPALIVE
voxel_space_context *create_voxel_space_context(const char *color_map, const char *depth_map, float start_x, float start_y, uint32_t background)
{
	voxel_space_context *ctx = (voxel_space_context *)malloc(sizeof(voxel_space_context));
	int n;
	ctx->color_map = (uint32_t *)stbi_load(color_map, &ctx->cw, &ctx->ch, &n, 4);
	ctx->depth_map = stbi_load(depth_map, &ctx->dw, &ctx->dh, &n, 1);

	ctx->out_w = ctx->out_h = 0;
	ctx->output = NULL;

	ctx->background = background;
	ctx->start_x = start_x;
	ctx->start_y = start_y;

	printf("Created voxel space context: {color_map: \"%s\", depth_map: \"%s\", cw: %i, ch: %i}\n", color_map, depth_map, ctx->cw, ctx->ch);
	return ctx;
}

EMSCRIPTEN_KEEPALIVE
void destroy_voxel_space_context(voxel_space_context *ctx)
{
	stbi_image_free(ctx->color_map);
	stbi_image_free(ctx->depth_map);

	if (ctx->output)
		free(ctx->output);
	free(ctx);
}

// sample functions assume map sizes are a power of 2
inline uint32_t get_index(float v, int modulus)
{
	if (v > 0)
		return (uint32_t)v & modulus;

	uint32_t abs_v = (uint32_t)(-v);
	return modulus - (abs_v & modulus);
}

uint8_t sample_depth(voxel_space_context *ctx, float x, float y)
{
	int ix = get_index(x, ctx->dw - 1);
	int iy = get_index(y, ctx->dh - 1);
	return ctx->depth_map[iy * ctx->dw + ix];
}

uint32_t sample_color(voxel_space_context *ctx, float x, float y)
{
	int ix = get_index(x, ctx->cw - 1);
	int iy = get_index(y, ctx->ch - 1);
	return ctx->color_map[iy * ctx->cw + ix];
}

void draw_line(voxel_space_context *ctx, int x, int y_begin, int y_end, uint32_t color)
{
	if (y_end > ctx->out_h)
		y_end = ctx->out_h;

	// invert y
	int index = (ctx->out_h - y_begin - 1) * ctx->out_w + x;
	for (int i = y_begin; i < y_end; i++)
	{
		ctx->output[index] = color;
		index -= ctx->out_w;
	}
}

EMSCRIPTEN_KEEPALIVE
uint32_t *render_voxel_space(voxel_space_context *ctx, int w, int h, float phi)
{
	static int y_buffer[2048];
	if (w > 2048)
	{
		printf("width, %i is greater than 2048\n", w);
		return NULL;
	}

	bool resize = (ctx->out_w * ctx->out_h) < (w * h);
	if (!ctx->output || resize)
	{
		if (ctx->output)
			free(ctx->output);
		ctx->output = malloc(w * h * sizeof(uint32_t));
	}

	ctx->out_w = w;
	ctx->out_h = h;

	const float horizon = HORIZON * h;

	const float fov = atan((float)w / h * FOV_SCALE);
	const float length_scale_factor = 1.0 / cosf(fov);
	const float start_x_dir = cosf(phi + fov) * length_scale_factor;
	const float start_y_dir = sinf(phi + fov) * length_scale_factor;
	const float end_x_dir = cosf(phi - fov) * length_scale_factor;
	const float end_y_dir = sinf(phi - fov) * length_scale_factor;

	memset(&y_buffer[0], 0, 2048 * sizeof(int));

	// fill output with background color
	for (int i = 0; i < w * h; i++)
	{
		ctx->output[i] = ctx->background;
	}

	float map_ratio = (float)ctx->dw / ctx->cw;

	float z = 1;
	float dz = 1;
	while (z < DISTANCE)
	{
		float start_x = start_x_dir * z + ctx->start_x;
		float start_y = start_y_dir * z + ctx->start_y;
		float end_x = end_x_dir * z + ctx->start_x;
		float end_y = end_y_dir * z + ctx->start_y;

		const float dx = (end_x - start_x) / w;
		const float dy = (end_y - start_y) / w;

		const float inv_z = SCALE * h / z;

		for (int x = 0; x < w; x++)
		{
			const float height = horizon - (50 - sample_depth(ctx, start_x * map_ratio, start_y * map_ratio)) * inv_z;
			if (height > y_buffer[x])
			{
				draw_line(ctx, x, y_buffer[x], height, sample_color(ctx, start_x, start_y));
				y_buffer[x] = height;
			}
			start_x += dx;
			start_y += dy;
		}

		// Go to next line and increase stepsize
		z += dz;
		dz += 0.005;
	}

	return &ctx->output[0];
}