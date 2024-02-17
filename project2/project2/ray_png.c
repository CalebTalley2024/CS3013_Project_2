#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <png.h>

#include "ray_png.h"

int render_png(struct framebuffer_pt4 *fb, const char *output_filepath) {
	FILE * fout = fopen(output_filepath, "wb");
	if (!fout) {
		fprintf(stderr, "Error opening '%s' for write: %d %s\n", output_filepath, errno, strerror(errno));
		goto err_fopen;
	}

	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png){
		goto err_png_create_write_struct;
	}
	png_infop info = png_create_info_struct(png);
	if (!info) {
		goto err_png_create_info_struct;
	}

	int bit_depth = 8;
	png_init_io(png, fout);
	png_set_IHDR(png, info, fb->width, fb->height, bit_depth,
			PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	png_bytepp row_pointers = malloc(sizeof(png_bytep) * fb->height);
	for(int y = 0; y < fb->height; y++) {
		row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png, info));
	}
	for (int y = 0; y < fb->height; y++) {
		for (int x = 0; x < fb->width; x++) {
			// bmp_source is source data that we convert to png
			const pt4 *p = framebuffer_pt4_get(fb, x, y);
			png_bytep px = &row_pointers[y][x*3];
			*px++ = color_double_to_u8(p->v[0]);
			*px++ = color_double_to_u8(p->v[1]);
			*px++ = color_double_to_u8(p->v[2]);
		}
	}

	// 4. Write png file
	png_write_info(png, info);
	png_write_image(png, row_pointers);
	png_write_end(png, info);
	png_destroy_write_struct(&png, &info);

	return 0;


err_png_create_info_struct:
	png_destroy_write_struct(&png, (png_infopp)NULL);
err_png_create_write_struct:
	fclose(fout);
err_fopen:
	return -1;
}
