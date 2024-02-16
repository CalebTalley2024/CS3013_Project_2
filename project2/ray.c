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



// @caleb create threads and semaphores
pthread_t render_thread, velocity_thread;
sem_t render_updated, position_updated;
sem_t velocity_updated;

//@both info threads
pthread_t infoThreads[5];
sem_t info_sem_updated_idx[5];



pthread_mutex_t mutex;

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
	struct framebuffer_pt4 *fb = NULL;
	int render_to_console = 1;
	if (argc > 2)
	{
		render_to_console = 0;
		///////////////////////////////////////////////////////////////////////////////////////
		// HINT: changing the resolutions here will alter the performance. If you want pngs  //
		// but faster, try lowering the resolution here.                                     //
		///////////////////////////////////////////////////////////////////////////////////////
		// fb = new_framebuffer_pt4(2560, 1440);
		fb = new_framebuffer_pt4(1280, 720);
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

		fb = new_framebuffer_pt4(ws.ws_col, ws.ws_row);
	}

	///////////////////////////////////////////////////////////////////////////////////////
	// Now we have a framebuffer and a scene graph.                                      //
	// Alternate render and physics passes.                                              //
	// However: we can parallelize the output here, as long as we are not corrupting the //
	// framebuffer whilst outputting.                                                    //
	// TODO: section 2: instead of one framebuffer, use
	///////////////////////////////////////////////////////////////////////////////////////

	// sem_init(&velocity_updated, 0, 0); // velocity is consided to not be updated (0)
	// init position_update to 1, (want velocity to start)
	sem_init(&position_updated, 0, 5); // initial position considerd updated (1 * 5)

	sem_init(&render_updated, 0, 0); // no initial render

	// make render_args to put all arguements into renderThread
	struct Render_Args *render_args = (struct Render_Args *)malloc(sizeof(struct Render_Args));

	render_args->fb = fb;
	render_args->ctx = ctx;
	render_args->render_to_console = render_to_console;
	// render_args->frame = frame;
	render_args->frame = 0; // start frame at 0

	render_args->argv = argv;

	pthread_create(&render_thread, NULL, render_console_or_disk, render_args); // thread for rendering
	pthread_create(&velocity_thread, NULL, update_velocity, ctx);	  // (void*) type of function pointer


	//for each frame, do this aka. for loop
	int work_length = fb->width/5;

	// subset_info *info_chunks[5] = (struct subset_info*)malloc(sizeof(subset_info));
	subset_info *info_chunks[5];

	for(int i = 0; i<5; i++){
	info_chunks[i] = (subset_info*)malloc(sizeof(subset_info) );

	}

	// fill info_chunks
	for (int i = 0; i < 5; i++) {
		// init the pointers for the chunk
		info_chunks[i] -> info_num = i;
		info_chunks[i]->fb = fb;
		info_chunks[i]->ctx = ctx;
		info_chunks[i]->lower_x_bound = i * work_length;
		info_chunks[i]->upper_x_bound = (i + 1) * work_length;

		// init the semaphores for chunk
		sem_init(&info_sem_updated_idx[i], 0, 0); // no initial render
	}

	// render chunks concurrently
	for (int i = 0; i<5; i++){
		pthread_create(&infoThreads[i], NULL, update_render_column, info_chunks[i]);
	}


	if (pthread_join(render_thread, NULL) != 0)
	{
		printf("thread not working");
		exit(-1);
	}
	else
	{
		// printf("\nrender thread works\n");
	}
	if (pthread_join(velocity_thread, NULL) != 0)
	{
		printf("thread not working");
		exit(-1);
	}
	else
	{
		printf("\n velocity thread not working: %d\n", pthread_join(velocity_thread, NULL));
	}

	// join infoThreads
	for (int i = 0; i<5; i++){
		pthread_join(infoThreads[i],NULL);
	}
	// Destroy semaphores
	// sem_destroy(&velocity_updated);
	sem_destroy(&render_updated);
	sem_destroy(&position_updated);

out:
	yylex_destroy(scanner);
	if (finput)
		fclose(finput);
	free_context(ctx);
	if (fb)
		free_framebuffer_pt4(fb);

	return 0;
}



void *render_console_or_disk(void *args)
{
	// printf("\nhe\n");
	// @caleb arugments: struct framebuffer_pt4 * fb,struct context* ctx, int render_to_console,int frame,char **argv

	// cast void pointer to render_arg pointer
	struct Render_Args *render_args = args;
	// printf("\nshe\n");
	// get render args from render_args

	for (int frame = 0; frame < 4 * 25; frame++)
	{
		
		// wait for all info frame columns  to be updated
		// printf("staring console/disk\n");

		// pthread_mutex_lock(&mutex);
		for (int i = 0; i<5; i++){
			printf(" wait for column  %d\n", i);
			sem_wait(&info_sem_updated_idx[i]);
			
		}
		// pthread_mutex_unlock(&mutex);

		// printf("\n received all columns\n");
		// allow for next frame to start rendering


		if (render_args->render_to_console)
		{
			render_console(render_args->fb);
			usleep(50000);
		}
		else
		{
			char filepath[128];
			// snprintf(filepath, sizeof(filepath) - 1, "%s-%05d.png", argv[2], frame);
			// render_png(fb(physics), filepath);
			snprintf(filepath, sizeof(filepath) - 1, "%s-%05d.bmp", render_args->argv[2], frame);
			render_bmp(render_args->fb, filepath);
		}
		// printf("Rendering finished\n");
		sem_post(&render_updated);
		
	}
	return 0;
}

void *update_velocity(void *_ctx)
{ // #TODO Make sure velocity goes before position

	for (int frame = 0; frame < 4 * 25; frame++)
	{
		
		// printf("\nINDEX: %d \n", frame);
		// no need for semaphores for velocity
		// printf("Velocity update started\n");

		struct context *ctx = _ctx;
		step_physics_velocity(ctx);

		// printf("Velocity update finished\n");
		// sem_post(&velocity_updated);

		sem_wait(&render_updated);
		// sem_wait(&velocity_updated);

		// printf("position started\n");
		step_physics_position(ctx);
		// printf("position finished\n");

		// pthread_mutex_lock(&mutex);
		for(int i = 0; i< 5;i++){
			sem_post(&position_updated);
		}
		// pthread_mutex_unlock(&mutex);

		sem_destroy(&position_updated);
		sem_init(&position_updated, 0, 5);

		int x;
		sem_getvalue(&position_updated,&x);
		printf("val of position sem: %d \n", x);
		
		// Join threads
	}

	return 0;
}

// used to multiple columns
void *update_render_column(void *_args){
    subset_info *args = _args;
	for (int frame = 0; frame < 4 * 25; frame++)
	{
	// printf("waitng at frame %d \n",frame);
	sem_wait(&position_updated);
	

    render_scene(args->fb, args->ctx, args->lower_x_bound, args->upper_x_bound);

	// printf("\nrender for column %d\n", args->info_num);

	sem_post(&info_sem_updated_idx[args->info_num]);
	}

	return 0;
}
