#include "../pinball_physics.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>


static void test_lane_clamp(void) {
    float x = 50.0f;
    float y = 80.0f;
    float vx = -2.0f;
    float vy = -3.0f;

    bool transferred = pinball_route_launcher(
        &x, &y, &vx, &vy, 13.0f, 53.0f, 52.0f, 2.0f, 0.75f);

    assert(!transferred);
    assert(x == 55.0f);
    assert(vx == 1.5f);
}


static void test_charged_launch_reaches_field(void) {
    float x = 57.0f;
    float y = 112.0f;
    float vx = 0.0f;
    float vy = -6.0f;
    bool transferred = false;

    for(int tick = 0; tick < 180 && !transferred; tick++) {
        vy += 0.12f;
        x += vx;
        y += vy;
        transferred = pinball_route_launcher(
            &x, &y, &vx, &vy, 13.0f, 53.0f, 52.0f, 2.0f, 0.75f);
    }

    assert(transferred);
    assert(x < 52.0f);
    assert(vx < 0.0f);
    assert(vy > 0.0f);
    assert(y > 13.0f);
}


int main(void) {
    test_lane_clamp();
    test_charged_launch_reaches_field();
    puts("physics tests: ok");
    return 0;
}
