#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PB_BALLS 2
#define PB_BUMPERS 3
#define PB_TARGETS 3
#define PB_STEP (1.0f / 120.0f)

typedef struct { float x, y; } PbVec;
typedef struct {
    PbVec p, v;
    bool active, lane;
    float age;
    float contacts[PB_BUMPERS];
} PbBall;
typedef struct { PbVec pivot; float angle, omega; } PbFlipper;
typedef struct { PbVec p; float radius; } PbBumper;
typedef struct { PbVec a, b; float bounce; } PbRail;
typedef struct {
    const char* name;
    const char* mission;
    PbBumper bumpers[PB_BUMPERS];
    PbVec targets[PB_TARGETS];
    PbRail rails[6];
    unsigned rail_count;
} PbTable;
typedef enum { PbReady, PbPlaying, PbBallLost, PbGameOver } PbPhase;
enum {
    PbLeft = 1, PbRight = 2, PbLaunch = 4,
    PbSoundLaunch = 1, PbSoundBumper = 2, PbSoundFlipper = 4,
    PbSoundTarget = 8, PbSoundMission = 16, PbSoundDrain = 32, PbSoundTilt = 64
};
typedef enum { PbNoticeNone, PbNoticeSaved, PbNoticeSkill, PbNoticeMission, PbNoticeTilt } PbNotice;
typedef struct {
    PbBall balls[PB_BALLS];
    PbFlipper flippers[2];
    uint32_t score, accumulator;
    uint8_t table, lives, target_mask, multiplier, controls, events;
    uint8_t combo, last_bumper;
    PbPhase phase;
    PbNotice notice;
    bool saved_this_ball, tilted;
    float charge, elapsed, combo_time, notice_time, phase_time, nudge_heat, nudge_cooldown;
    float bumper_flash[PB_BUMPERS];
    unsigned missions;
} Pinball;

const PbTable* pinball_table(unsigned index);
void pinball_init(Pinball* game, unsigned table);
void pinball_controls(Pinball* game, uint8_t controls);
void pinball_nudge(Pinball* game);
void pinball_advance(Pinball* game, uint32_t elapsed_ms);
/* Exposed for meaningful collision and energy regression tests. */
bool pinball_contact(PbBall* ball, PbVec center, float radius, PbVec surface, float bounce);

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
