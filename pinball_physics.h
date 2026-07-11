#pragma once

#include <stdbool.h>

/*
 * Keep the launcher transition independent from the Flipper SDK so its
 * geometry can be exercised on a normal host compiler.
 */
bool pinball_route_launcher(
    float* x,
    float* y,
    float* vx,
    float* vy,
    float top_wall,
    float lane_left,
    float field_right,
    float ball_radius,
    float damping);
