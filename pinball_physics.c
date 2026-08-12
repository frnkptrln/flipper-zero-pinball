#include "pinball_physics.h"

#include <math.h>


bool pinball_route_launcher(
    float* x,
    float* y,
    float* vx,
    float* vy,
    float top_wall,
    float lane_left,
    float field_right,
    float ball_radius,
    float damping) {
    float lane_min = lane_left + ball_radius;

    if(*y > top_wall) {
        if(*x < lane_min) {
            *x = lane_min;
            *vx = fabsf(*vx) * damping;
        }
        return false;
    }

    float inbound_speed = fabsf(*vy);
    *x = field_right - ball_radius - 1.0f;
    *y = top_wall + 1.0f;
    *vx = -fmaxf(1.25f, inbound_speed * 0.35f);
    *vy = fmaxf(0.45f, inbound_speed * 0.18f);
    return true;
}
