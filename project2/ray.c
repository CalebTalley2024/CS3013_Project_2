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

// @caleb added
#include <pthread.h>
#include <semaphore.h>


// create threads and semaphores
pthread_t render_thread, physics_thread;
sem_t new_frame_ready, physics_updated;

pthread_mutex_t mutex;


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

	
	// make render_args to put all arguements into renderThread
	struct Render_Args* render_args = (struct Render_Args*)malloc(sizeof(struct Render_Args));

	for (int frame = 0; frame < 4 * 25; frame++) {
		

		// update render args
		render_args->fb = fb;
		render_args->ctx = ctx;
		render_args->render_to_console = render_to_console;
		render_args->frame = frame;
		render_args->argv = argv;

		

		sem_init(&new_frame_ready,0,0); // frame is not ready at first (0)

		sem_init(&physics_updated,0,1); // initial physics considerd updated (1)

		// thread for rendering
		pthread_create(&render_thread,NULL,update_render,render_args);

		// thread for physics
		//(void *(*)(void *)): @caleb not entirely sure, but the error told me this what I needed
		// void *: return type

		// * pointer to function

		// (void*) type of function pointer
		pthread_create(&physics_thread,NULL,update_physics,ctx);


		// Join threads
		pthread_join(render_thread, NULL);
		pthread_join(physics_thread, NULL);

		// Destroy semaphores
		sem_destroy(&new_frame_ready);
		sem_destroy(&physics_updated);
	}

out:
	yylex_destroy(scanner);
	if (finput) fclose(finput);
	free_context(ctx);
	if (fb) free_framebuffer_pt4(fb);

	return 0;
}

void *update_render(void *args){
	// printf("\nhe\n");
	// @caleb arugments: struct framebuffer_pt4 * fb,struct context* ctx, int render_to_console,int frame,char **argv

	// cast void pointer to render_arg pointer

	struct Render_Args *render_args = args;
	// printf("\nshe\n");
	// get render args from render_args

		// wait for physics to be updated
		// sem_wait(&physics_updated);

		// pthread_mutex_lock(&mutex);

		render_scene(render_args->fb, render_args->ctx);

		if (render_args->render_to_console) {
			render_console(render_args->fb);
			usleep(50000);
		} else {
			char filepath[128];
			//snprintf(filepath, sizeof(filepath) - 1, "%s-%05d.png", argv[2], frame);
			//render_png(fb, filepath);
			snprintf(filepath, sizeof(filepath) - 1, "%s-%05d.bmp", render_args->argv[2], render_args->frame);
			render_bmp(render_args->fb, filepath);
		}

		// pthread_mutex_unlock(&mutex);

		// sem_post(&new_frame_ready);

		return 0;

}


void *update_physics(void * _ctx){ //#TODO Make sure velocity goes before position

struct context* ctx = _ctx;

		// sem_wait(&new_frame_ready);

		pthread_mutex_lock(&mutex);
		step_physics_velocity(ctx);
		pthread_mutex_unlock(&mutex);

		pthread_mutex_lock(&mutex);
		step_physics_position(ctx);
		pthread_mutex_unlock(&mutex);
		

		// sem_post(&physics_updated);

		return 0;
}
