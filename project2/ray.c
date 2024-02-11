#include <stdio.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "ray_ast.h"
#include "ray_math.h"
#include "ray.yacc.generated_h"
#include "ray.lex.generated_h"

#include "ray_render.h"
#include "ray_png.h"
#include "ray_bmp.h"
#include "ray_physics.h"
#include "ray_console.h"

int main(int argc, char **argv) {

	int rc;

	struct context *ctx = new_context();
	FILE *finput = NULL;
	yyscan_t scanner;
	yylex_init(&scanner);

	// Read from a script. By default this is stdin.
	if (argc > 1) {
		// If a file is specified as a command line argument, read from that instead of stdin.
		const char *source = argv[1];
		finput = fopen(source, "rb");
		if (finput == NULL) {
			fprintf(stderr, "Could not open '%s' for reading, errno %d (%s)\n", source, errno, strerror(errno));
			return 1;
		}
		yyset_in(finput, scanner);
	}
	// Parse the input file and run the parsed script if parsing was successful.
	if ((rc = yyparse(ctx, scanner)) != 0) {
		fprintf(stderr, "Parse failure for script\n");
		goto out;		
	}

	// Calculate framebuffer size. If we're outputting into a png file, use a high resolution.
	// If we're rendering to the active console, use ioctls to find the window size.
	// Initialize a framebuffer with the chosen resolution.
	struct framebuffer_pt4 *fb = NULL;
	int render_to_console = 1;
	if (argc > 2) {
		render_to_console = 0;
		///////////////////////////////////////////////////////////////////////////////////////
		// HINT: changing the resolutions here will alter the performance. If you want pngs  //
		// but faster, try lowering the resolution here.                                     //
		///////////////////////////////////////////////////////////////////////////////////////
		// fb = new_framebuffer_pt4(2560, 1440);
		fb = new_framebuffer_pt4(1280, 720);
	} else {
		struct winsize ws;

		if (isatty(STDIN_FILENO)) {
			if ((rc = ioctl(0, TIOCGWINSZ, &ws)) < 0) {
				fprintf(stderr, "Failed to get window size: %d %s\n", errno, strerror(errno));
				return 1;
			}
			printf ("cols (x) %d lines (y) %d\n", ws.ws_col, ws.ws_row);
		} else {
			ws.ws_row = 128;
			ws.ws_col = 128;
		}
	
		fb = new_framebuffer_pt4(ws.ws_col, ws.ws_row);
	}
	
	///////////////////////////////////////////////////////////////////////////////////////
	// Now we have a framebuffer and a scene graph.                                      //
	// Alternate render and physics passes.                                              //
	// However: we can parallelize the output here, as long as we are not corrupting the //
	// framebuffer whilst outputting.                                                    //
	// TODO: section 2: instead of one framebuffer, use 
	///////////////////////////////////////////////////////////////////////////////////////
	for (int frame = 0; frame < 4 * 25; frame++) {
		render_scene(fb, ctx);
		if (render_to_console) {
			render_console(fb);
			usleep(50000);
		} else {
			char filepath[128];
			//snprintf(filepath, sizeof(filepath) - 1, "%s-%05d.png", argv[2], frame);
			//render_png(fb, filepath);
			snprintf(filepath, sizeof(filepath) - 1, "%s-%05d.bmp", argv[2], frame);
			render_bmp(fb, filepath);
		}
		step_physics_velocity(ctx);
		step_physics_position(ctx);

	}

out:
	yylex_destroy(scanner);
	if (finput) fclose(finput);
	free_context(ctx);
	if (fb) free_framebuffer_pt4(fb);

	return 0;
}

