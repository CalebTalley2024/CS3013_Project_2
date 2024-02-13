#ifndef RAY_PHYSICS_H__
#define RAY_PHYSICS_H__

#include "ray_ast.h"
#include "ray_math.h"

// int step_physics(struct context *ctx);
//

int step_physics_velocity(struct context *ctx);
int step_physics_position(struct context *ctx);


// custom

struct Sphere_Int_Args{
    sphere *si; 
    pt3 *pi;
    pt3 *vi; 
    sphere *sj;
    int i;
    int j;
};
#endif	// RAY_PHYSICS_H__
