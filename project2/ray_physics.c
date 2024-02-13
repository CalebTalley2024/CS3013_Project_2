
#include "ray_physics.h"
#include "ray_math.h"

#include <semaphore.h>
#include <pthread.h>

static const double framerate = 25;
static const pt3 gravity = {{0, -9.8 / framerate, 0}};

typedef int semaphore;

// delta_velocity(int sphere_idx,struct context *ctx )  // producer

pthread_mutex_t collision_mutex;
// delta_position // consumer

void *sphere_interaction(void *args){

	struct Sphere_Int_Args *int_args = args; // interaction args
	
	// get pointers/values from args
    sphere *si = int_args ->si; 
    pt3 *pi = int_args ->pi;
    pt3 *vi = int_args ->vi;
    sphere *sj = int_args ->sj;
    int i = int_args ->i;
    int j = int_args ->j;


	pt3 *pj = &sj->position;
	pt3 *vj = &sj->velocity;
	pt3 pdiff = pt3_sub(pi, pj);
	pt3_normalize_mut(&pdiff);
	pt3 vdiff = pt3_sub(vi, vj);

	// if they're heading away from each other already, this is an unlucky frame
	int approaching = pt3_dot(&vdiff, &pdiff) < 0;
	// if (!approaching) continue;
	if (!approaching){

	pthread_mutex_lock(&collision_mutex);
	// check for a collision (position distance less than sum of radii)
	pt3 hit;
	pt3 normal;
	// if (!intersect_sphere_sphere(si, sj, &hit, &normal)) continue;
	if (intersect_sphere_sphere(si, sj, &hit, &normal)){

		// find projections of existing velocities onto the collision normal
		// use the radii to find comparative masses. we assume uniform density for simplicity.
		double ri = si->radius;
		double rj = sj->radius;
		double mi = 4.0 / 3.0 * M_PI * ri * ri * ri;
		double mj = 4.0 / 3.0 * M_PI * rj * rj * rj;

		double a = pt3_dotv(pt3_mul(&normal, 2), vdiff) / (1/mi + 1/mj);

		if (0) fprintf(stderr, "collision pre  i %d j %d, vi %lf %lf %lf vj %lf %lf %lf ri %lf rj %lf mi %lf mj %lf a %lf\n", i, j,
				vi->v[0], vi->v[1], vi->v[2],
				vj->v[0], vj->v[1], vj->v[2],
				ri, rj, mi, mj,
				a);

		*vi = pt3_addv(*vi, pt3_mul(&normal, -a / mi));
		*vj = pt3_addv(*vj, pt3_mul(&normal, a / mj));

		if (0) fprintf(stderr, "collision post i %d j %d, vi %lf %lf %lf vj %lf %lf %lf\n", i, j,
				vi->v[0], vi->v[1], vi->v[2],
				vj->v[0], vj->v[1], vj->v[2]);

		}	
	pthread_mutex_unlock(&collision_mutex);

	}

	return 0;
}

//slow
int step_physics_velocity(struct context *ctx) {
	// First we loop through all the spheres in the world, collision detect them with everything else,
	// and modify the velocities _in place_. Note that velocities may be manipulated during the render
	// phase as they don't affect position until they are applied.

	pthread_t sphere_thread[ctx->num_spheres-1]; // create threads for spheres

	for (int i = 0; i < ctx->num_spheres; i++) {
		sphere *si = &ctx->spheres[i];
		pt3 *pi = &si->position;
		pt3 *vi = &si->velocity;

		// sphere interaction
		for (int j = i+1; j < ctx->num_spheres; j++) {
			sphere *sj = &ctx->spheres[j];

			// create args
			struct Sphere_Int_Args *int_args = (struct Sphere_Int_Args*)malloc(sizeof(struct Sphere_Int_Args));
			int_args->si = si;
			int_args->pi = pi;
			int_args->vi = vi;
			int_args->sj = sj;
			int_args->i = i;
			int_args->j = j;

			// sphere_interaction(si,pi,vi,sj,i,j);
			pthread_create(&sphere_thread[j],NULL, sphere_interaction, int_args); // j-1 to use all threads
			
			// free(int_args);
			
		}

		// this causes page fault

		//for (int k = 0; k < ctx->num_spheres - 1; k++) { // Loop using the number of created threads
		// 	int ret = pthread_join(sphere_thread[k], NULL); 
		// 	if (ret != 0) {
		// 		fprintf(stderr, "pthread_join failed with error code: %d\n", ret);
		// 	} 
		// }

		
		// plane interaction
		for (int j = 0; j < ctx->num_planes; j++) {
			plane *p = &ctx->planes[j];
			if (intersect_sphere_plane(si, p)) {
				double d = pt3_dot(vi, &p->normal);
				if (d < 0) {
					*vi = pt3_addv(*vi, pt3_mul(&p->normal, -1.99 * d));
				}
			} else {
				// Apply a velocity step to all the spheres; discretized acceleration.
				// But not if we're touching the plane, otherwise we sink into it.
				si->velocity = pt3_addv(si->velocity, gravity);
			}
		}
	}

	return 0;
}

// fast, cannot be updated while raytracing
int step_physics_position(struct context *ctx){
	// Now that velocities are altered in the previous loop, apply the velocities (and gravity) to everything.
	// This can't be run in parallel with rendering as it occurs now because the raytracer is dependent on the
	// positions of each sphere to know how to draw it.
	// Some possibilities include: 
	for (int i = 0; i < ctx->num_spheres; i++) {
		ctx->spheres[i].position = pt3_addv(ctx->spheres[i].position, pt3_mul(&ctx->spheres[i].velocity, 1.0/24));
	}
	return 0;

}