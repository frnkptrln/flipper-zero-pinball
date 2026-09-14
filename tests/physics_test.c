#include "../pinball_physics.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <string.h>


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


static void launch(Pinball* g) {
    pinball_controls(g,PbLaunch);
    pinball_controls(g,0);
}

static void test_real_launches(void) {
    for(unsigned table = 0; table < 2; table++) {
        Pinball g; pinball_init(&g,table); launch(&g);
        for(unsigned i = 0; i < 160 && g.balls[0].lane; i++) pinball_advance(&g,10);
        assert(!g.balls[0].lane);
        assert(g.balls[0].p.x < 53);
        assert(g.balls[0].v.x < 0);
    }
}

static void test_collision_energy(void) {
    PbBall resting = {.p={10,9}, .v={0,30}};
    assert(pinball_contact(&resting,(PbVec){10,10},1,(PbVec){0,0},.65f));
    assert(resting.v.y < 0 && fabsf(resting.v.y) < 30);
    PbBall moving = {.p={10,9}, .v={0,30}};
    assert(pinball_contact(&moving,(PbVec){10,10},1,(PbVec){0,-100},.65f));
    assert(moving.v.y < -100);
    PbBall separating = {.p={10,9}, .v={0,-30}};
    assert(!pinball_contact(&separating,(PbVec){10,10},1,(PbVec){0,0},.65f));
    assert(separating.v.y == -30);
    PbBall centered = {.p={10,10}, .v={0,30}};
    pinball_contact(&centered,(PbVec){10,10},1,(PbVec){0,0},.65f);
    assert(isfinite(centered.p.x) && isfinite(centered.v.y));
}

static void test_fixed_clock(void) {
    Pinball a,b; pinball_init(&a,0); pinball_init(&b,0); launch(&a); launch(&b);
    for(int i=0;i<1000;i++) pinball_advance(&a,1);
    for(int i=0;i<50;i++) pinball_advance(&b,20);
    assert(memcmp(&a,&b,sizeof(a)) == 0);
    pinball_controls(&a,PbLeft);
    for(int i=0;i<20;i++) pinball_advance(&a,10);
    assert(fabsf(a.flippers[0].angle+.55f) < .001f);
    assert(fabsf(a.flippers[0].omega) < .001f);
    pinball_controls(&a,0);
    for(int i=0;i<20;i++) pinball_advance(&a,10);
    assert(fabsf(a.flippers[0].angle-.5f) < .001f);
}

static void drain(Pinball* g, float age) {
    g->balls[0].p = (PbVec){27,126};
    g->balls[0].v = (PbVec){0,50};
    g->balls[0].lane = false; g->balls[0].active = true; g->balls[0].age = age;
    g->phase = PbPlaying;
    pinball_advance(g,10);
}

static void test_lives_and_save(void) {
    Pinball g; pinball_init(&g,0); launch(&g); drain(&g,1);
    assert(g.lives == 3 && g.phase == PbReady && g.saved_this_ball);
    launch(&g); drain(&g,1);
    assert(g.lives == 2 && g.phase == PbBallLost);
    for(int i=0;i<110;i++) pinball_advance(&g,10);
    assert(g.phase == PbReady);
    launch(&g); drain(&g,6);
    assert(g.lives == 1);
    for(int i=0;i<110;i++) pinball_advance(&g,10);
    launch(&g); drain(&g,6);
    assert(g.phase == PbGameOver && g.lives == 0);
}

static void test_mission_and_multiball(void) {
    Pinball g; pinball_init(&g,0); launch(&g);
    for(unsigned i=0;i<3;i++) {
        g.balls[0].p = pinball_table(0)->targets[i];
        g.balls[0].v = (PbVec){0,0}; g.balls[0].age = 6; g.balls[0].lane = false;
        pinball_advance(&g,10);
    }
    assert(g.missions == 1 && g.multiplier == 2 && g.target_mask == 0);
    assert(g.balls[1].active && g.score == 800);
    drain(&g,6);
    assert(g.lives == 3 && g.phase == PbPlaying && g.balls[1].active);
}

static void test_tilt(void) {
    Pinball g; pinball_init(&g,0); launch(&g);
    for(int n=0;n<3;n++) {
        pinball_nudge(&g);
        if(n < 2) for(int i=0;i<36;i++) pinball_advance(&g,10);
    }
    assert(g.tilted);
    pinball_controls(&g,PbLeft|PbRight);
    for(int i=0;i<30;i++) pinball_advance(&g,10);
    assert(g.flippers[0].angle > .49f);
}

static void test_long_sessions(void) {
    for(unsigned table=0;table<2;table++) {
        Pinball g; pinball_init(&g,table);
        for(unsigned i=0;i<60000;i++) {
            if(g.phase == PbGameOver) pinball_init(&g,table);
            if(g.phase == PbReady) launch(&g);
            uint8_t controls = 0;
            for(unsigned b=0;b<PB_BALLS;b++) if(g.balls[b].active && g.balls[b].p.y > 90)
                controls |= g.balls[b].p.x < 27 ? PbLeft : PbRight;
            pinball_controls(&g,controls); pinball_advance(&g,10);
            if(i%200 == 0) pinball_nudge(&g);
            assert(g.lives <= 3 && g.multiplier <= 5);
            for(unsigned b=0;b<PB_BALLS;b++) {
                assert(isfinite(g.balls[b].p.x) && isfinite(g.balls[b].p.y));
                assert(isfinite(g.balls[b].v.x) && isfinite(g.balls[b].v.y));
                if(g.balls[b].active) assert(g.balls[b].p.y > 0 && g.balls[b].p.y < 128);
            }
        }
    }
}

int main(void) {
    test_lane_clamp();
    test_charged_launch_reaches_field();
    test_real_launches();
    test_collision_energy();
    test_fixed_clock();
    test_lives_and_save();
    test_mission_and_multiball();
    test_tilt();
    test_long_sessions();
    puts("physics tests: ok");
    return 0;
}
