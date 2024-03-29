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

// must be divisible by 1280
#define NUM_COLS 55
const int num_cols = 55;

// @caleb create threads and semaphores
pthread_t console_or_disk_thread, physics_thread, render_col_threads[NUM_COLS];
sem_t full_render_updated, position_updated[NUM_COLS], render_col_updated;

int main(int argc, char **argv)
{

	int rc;

	struct context *ctx = new_context();
	FILE *finput = NULL;
	yyscan_t scanner;
	yylex_init(&scanner);

	// Read from a script. By default this is stdin.
	if (argc > 1)
	{
		// If a file is specified as a command line argument, read from that instead of stdin.
		const char *source = argv[1];
		finput = fopen(source, "rb");
		if (finput == NULL)
		{
			fprintf(stderr, "Could not open '%s' for reading, errno %d (%s)\n", source, errno, strerror(errno));
			return 1;
		}
		yyset_in(finput, scanner);
	}
	// Parse the input file and run the parsed script if parsing was successful.
	if ((rc = yyparse(ctx, scanner)) != 0)
	{
		fprintf(stderr, "Parse failure for script\n");
		goto out;
	}

	// Calculate framebuffer size. If we're outputting into a png file, use a high resolution.
	// If we're rendering to the active console, use ioctls to find the window size.
	// Initialize a framebuffer with the chosen resolution.
	struct framebuffer_pt4 *fb_curr = NULL;
	struct framebuffer_pt4 *fb_prev = NULL;
	int render_to_console = 1;
	if (argc > 2)
	{
		render_to_console = 0;
		///////////////////////////////////////////////////////////////////////////////////////
		// HINT: changing the resolutions here will alter the performance. If you want pngs  //
		// but faster, try lowering the resolution here.                                     //
		///////////////////////////////////////////////////////////////////////////////////////
		// fb = new_framebuffer_pt4(2560, 1440);
		fb_curr = new_framebuffer_pt4(1280, 720);
		fb_prev = new_framebuffer_pt4(1280, 720);
	}
	else
	{
		struct winsize ws;

		if (isatty(STDIN_FILENO))
		{
			if ((rc = ioctl(0, TIOCGWINSZ, &ws)) < 0)
			{
				fprintf(stderr, "Failed to get window size: %d %s\n", errno, strerror(errno));
				return 1;
			}
			printf("cols (x) %d lines (y) %d\n", ws.ws_col, ws.ws_row);
		}
		else
		{
			ws.ws_row = 128;
			ws.ws_col = 128;
		}

		fb_curr = new_framebuffer_pt4(ws.ws_col, ws.ws_row);
		fb_prev = new_framebuffer_pt4(ws.ws_col, ws.ws_row);

	}

	///////////////////////////////////////////////////////////////////////////////////////
	// Now we have a framebuffer and a scene graph.                                      //
	// Alternate render and physics passes.                                              //
	// However: we can parallelize the output here, as long as we are not corrupting the //
	// framebuffer whilst outputting.                                                    //
	// TODO: section 2: instead of one framebuffer, use
	///////////////////////////////////////////////////////////////////////////////////////	

	// for each frame, do this aka. for loop

	
	// int num_cols; // number of separated columns per frame
	int work_length = fb_curr->width / num_cols;

	subset_info *frame_col_info[NUM_COLS]; // frame column info * 5 ?

	// fill frame_col_info for each column
	for (int i = 0; i < num_cols; i++)
	{
		// allocate memory
		frame_col_info[i] = (subset_info *)malloc(sizeof(subset_info));
		// init the pointers for the chunk
		frame_col_info[i]->col_num = i; // set column number
		frame_col_info[i]->fb_curr = fb_curr;
		frame_col_info[i]->fb_prev = fb_prev;
		frame_col_info[i]->ctx = ctx;

		// make sure info contain a workload's width of area
		frame_col_info[i]->lower_x_bound = i * work_length;
		frame_col_info[i]->upper_x_bound = (i + 1) * work_length;
	}

	// initiate semaphores
	for (int col = 0; col < num_cols; col++) // for each column thread
	{
		sem_init(&position_updated[col], 0, 1); // initial position for column i is considered updated
	}
	sem_init(&render_col_updated, 0, 0); // info_sem columns are initially not updated
	sem_init(&full_render_updated,0,0); // nothing is rendered initially

	// make console_disk_args to put all arguments into console_or_disk_thread's function
	struct Console_Disk_Args *console_disk_args = (struct Console_Disk_Args *)malloc(sizeof(struct Console_Disk_Args));
	console_disk_args->fb_curr = fb_curr;
	console_disk_args->ctx = ctx;
	console_disk_args->render_to_console = render_to_console;
	console_disk_args->frame = 0; // start frame at 0
	console_disk_args->argv = argv;

	// create concurrent threads
	pthread_create(&console_or_disk_thread, NULL, render_console_or_disk, console_disk_args); // thread for rendering
	pthread_create(&physics_thread, NULL, update_physics, ctx);
	for (int i = 0; i < num_cols; i++)
	{
		pthread_create(&render_col_threads[i], NULL, update_render_col, frame_col_info[i]);
	}

	// join all threads
	if (pthread_join(console_or_disk_thread, NULL) != 0)
	{
		printf("thread not working");
		exit(-1);
	}
	else
	{
		// printf("\nrender thread works\n");
	}
	if (pthread_join(physics_thread, NULL) != 0)
	{
		printf("thread not working");
		exit(-1);
	}
	else
	{
		printf("\n velocity thread not working: %d\n", pthread_join(physics_thread, NULL));
	}
	for (int i = 0; i < num_cols; i++)
	{
		pthread_join(render_col_threads[i], NULL);
	}

	// Destroy semaphores
	sem_destroy(&full_render_updated);
	sem_destroy(&render_col_updated);
	for (int i = 0; i < num_cols; i++)
	{
		sem_destroy(&position_updated[i]); // initial position considerd updated (1 * num_cols)
	}

out:
	yylex_destroy(scanner);
	if (finput)
		fclose(finput);
	free_context(ctx);
	if (fb_curr)
		free_framebuffer_pt4(fb_curr);
	if (fb_prev)
		free_framebuffer_pt4(fb_prev);
	return 0;
}


// either render to console or save rendered photo to disk as bmp
// done for each frame
void *render_console_or_disk(void *args)
{
	// @caleb arugments: struct framebuffer_pt4 * fb,struct context* ctx, int render_to_console,int frame,char **argv

	// cast void pointer to render_arg pointer
	struct Console_Disk_Args *console_disk_args = args;
	// get render args from console_disk_args

	for (int frame = 0; frame < 4 * 25; frame++)
	{
		// wait for all info frame columns  to be updated
		// printf("staring console/disk\n");

		for (int i = 0; i < num_cols; i++)
		{
			// printf(" wait for column  %d\n", i);
			sem_wait(&render_col_updated);
		}

		console_disk_args -> fb_prev = console_disk_args -> fb_curr;
		// allow for next frame to start rendering
		if (console_disk_args->render_to_console)
		{
			render_console(console_disk_args->fb_prev);
			usleep(50000);
		}
		else
		{
			char filepath[128];
			snprintf(filepath, sizeof(filepath) - 1, "%s-%05d.bmp", console_disk_args->argv[2], frame);
			render_bmp(console_disk_args->fb_prev, filepath);
		}
		sem_post(&full_render_updated);

	}
	return 0;
}

// update velocity for each frame
void *update_physics(void *_ctx)
{ // #TODO Make sure velocity goes before position
	for (int frame = 0; frame < 4 * 25; frame++)
	{
		// printf("Velocity update started\n");
		struct context *ctx = _ctx;
		step_physics_velocity(ctx);
		// printf("Velocity update finished\n");
		sem_wait(&full_render_updated);

		// printf("position started\n");
		step_physics_position(ctx);
		// printf("position finished\n");

		printf("Frame %d is done\n", frame); // keeps track of current frame

		for (int i = 0; i < num_cols; i++)
		{
			sem_post(&position_updated[i]);
		}
	}
	return 0;
}

// render one column of a each frame
void *update_render_col(void *_args)
{
	subset_info *args = _args;
	for (int frame = 0; frame < 4 * 25; frame++)
	{
		// printf("waitng at frame %d \n",frame);
		sem_wait(&position_updated[args->col_num]);
		render_scene(args->fb_curr, args->ctx, args->lower_x_bound, args->upper_x_bound);
		sem_post(&render_col_updated);
	}
	return 0;
}
