#ifndef RAY_PHYSICS_H__
#define RAY_PHYSICS_H__

#include "ray_ast.h"
#include "ray_math.h"

// int step_physics(struct context *ctx);
//

int step_physics_velocity(struct context *ctx);
int step_physics_position(struct context *ctx);

#endif	// RAY_PHYSICS_H__
