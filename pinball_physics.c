#include "pinball_physics.h"

#include <math.h>
#include <string.h>

#define PI 3.14159265358979323846f
#define RADIUS 1.8f
#define GRAVITY 85.0f
#define MAX_VELOCITY 225.0f

static const PbTable tables[] = {
    {"ORBIT", "LIGHT A B C",
     {{{17, 39}, 4.5f}, {{39, 47}, 4.5f}, {{24, 64}, 4.0f}},
     {{12, 25}, {31, 25}, {46, 69}},
     {{{3, 80}, {10, 94}, .82f}, {{10, 94}, {9, 103}, .72f},
      {{52, 80}, {45, 94}, .82f}, {{45, 94}, {45, 103}, .72f},
      {{3, 18}, {10, 14}, .85f}, {{49, 14}, {53, 20}, .85f}}, 6},
    {"REACTOR", "CHARGE A B C",
     {{{18, 43}, 4.0f}, {{38, 35}, 4.0f}, {{34, 66}, 5.0f}},
     {{10, 61}, {28, 23}, {46, 56}},
     {{{3, 78}, {11, 94}, .9f}, {{11, 94}, {9, 103}, .72f},
      {{52, 81}, {44, 94}, .9f}, {{44, 94}, {45, 103}, .72f},
      {{7, 32}, {13, 26}, .9f}, {{43, 76}, {49, 68}, .95f}}, 6}
};

static float limit(float x, float lo, float hi) { return fminf(hi, fmaxf(lo, x)); }
static PbVec sub(PbVec a, PbVec b) { return (PbVec){a.x-b.x, a.y-b.y}; }
static float dot(PbVec a, PbVec b) { return a.x*b.x + a.y*b.y; }
static float length(PbVec a) { return sqrtf(dot(a,a)); }

const PbTable* pinball_table(unsigned index) { return &tables[index % 2]; }

static void prepare_ball(Pinball* g) {
    memset(g->balls, 0, sizeof(g->balls));
    g->balls[0].p = (PbVec){59, 117};
    g->balls[0].lane = true;
    g->phase = PbReady;
    g->charge = 0;
    g->controls = 0;
    g->tilted = false;
    g->nudge_heat = 0;
    g->nudge_cooldown = 0;
    g->combo = 0;
    g->combo_time = 0;
    g->accumulator = 0;
}

void pinball_init(Pinball* g, unsigned table) {
    memset(g, 0, sizeof(*g));
    g->table = (uint8_t)(table % 2);
    g->lives = 3;
    g->multiplier = 1;
    g->last_bumper = 255;
    g->flippers[0] = (PbFlipper){{9, 105}, .5f, 0};
    g->flippers[1] = (PbFlipper){{45, 105}, PI-.5f, 0};
    prepare_ball(g);
}

void pinball_controls(Pinball* g, uint8_t controls) {
    if(g->phase == PbReady && (g->controls & PbLaunch) && !(controls & PbLaunch)) {
        PbBall* b = &g->balls[0];
        b->active = true;
        b->age = 0;
        /* Even a tap leaves the lane. Holding chooses speed and skill shot timing. */
        b->v = (PbVec){0, -(145.0f + 60.0f*g->charge)};
        g->phase = PbPlaying;
        g->events |= PbSoundLaunch;
    }
    g->controls = controls;
}

void pinball_nudge(Pinball* g) {
    if(g->phase != PbPlaying || g->tilted || g->nudge_cooldown > 0) return;
    g->nudge_heat += 1.0f;
    g->nudge_cooldown = .35f;
    if(g->nudge_heat > 2.5f) {
        g->tilted = true;
        g->notice = PbNoticeTilt;
        g->notice_time = 1.5f;
        g->events |= PbSoundTilt;
        return;
    }
    for(unsigned i = 0; i < PB_BALLS; i++) if(g->balls[i].active && !g->balls[i].lane) {
        g->balls[i].v.y -= 26;
        g->balls[i].v.x += g->balls[i].p.x < 27 ? 10 : -10;
    }
}

bool pinball_contact(PbBall* b, PbVec center, float radius, PbVec surface, float bounce) {
    PbVec n = sub(b->p, center);
    float distance = length(n), total = RADIUS + radius;
    if(distance >= total) return false;
    if(distance < .0001f) { n = (PbVec){0,-1}; distance = 0; }
    else { n.x /= distance; n.y /= distance; }
    b->p.x += n.x*(total-distance+.005f);
    b->p.y += n.y*(total-distance+.005f);
    float incoming = dot(sub(b->v, surface), n);
    if(incoming >= 0) return false;
    b->v.x -= (1+bounce)*incoming*n.x;
    b->v.y -= (1+bounce)*incoming*n.y;
    return true;
}

static PbVec closest(PbVec a, PbVec b, PbVec point) {
    PbVec line = sub(b,a);
    float t = limit(dot(sub(point,a),line)/fmaxf(.0001f,dot(line,line)), 0, 1);
    return (PbVec){a.x+t*line.x, a.y+t*line.y};
}

static void step_flippers(Pinball* g) {
    for(unsigned i = 0; i < 2; i++) {
        PbFlipper* f = &g->flippers[i];
        bool active = !g->tilted && (g->controls & (i ? PbRight : PbLeft));
        float a = active ? -.55f : .5f;
        float target = i ? PI-a : a;
        float amount = (active ? 14.0f : 9.0f)*PB_STEP;
        float next = f->angle + limit(target-f->angle, -amount, amount);
        f->omega = (next-f->angle)/PB_STEP;
        f->angle = next;
    }
}

static void add_score(Pinball* g, uint32_t score) {
    if(!g->tilted) g->score += score * g->multiplier;
}

static void mission(Pinball* g) {
    add_score(g, 500);
    g->missions++;
    g->multiplier = g->multiplier < 5 ? g->multiplier+1 : 5;
    g->target_mask = 0;
    g->notice = PbNoticeMission;
    g->notice_time = 1.4f;
    g->events |= PbSoundMission;
    if(!g->balls[1].active) {
        memset(&g->balls[1], 0, sizeof(PbBall));
        g->balls[1].active = true;
        g->balls[1].p = (PbVec){28, 83};
        g->balls[1].v = (PbVec){-42,-115};
        g->balls[1].age = 6;
    }
}

static void step_ball(Pinball* g, PbBall* b) {
    const PbTable* table = pinball_table(g->table);
    b->age += PB_STEP;
    b->v.y += GRAVITY*PB_STEP;
    float speed = length(b->v);
    if(speed > MAX_VELOCITY) {
        b->v.x *= MAX_VELOCITY/speed;
        b->v.y *= MAX_VELOCITY/speed;
    }
    b->p.x += b->v.x*PB_STEP;
    b->p.y += b->v.y*PB_STEP;
    if(b->lane) {
        if(b->p.y <= 18) {
            b->lane = false;
            b->p = (PbVec){50,19};
            b->v = (PbVec){-fmaxf(50, fabsf(b->v.y)*.65f), 12};
        } else if(b->p.y >= 117 && b->v.y > 0) {
            b->p.y = 117;
            b->v.y = -155;
        }
        return;
    }
    if(b->p.x < 3+RADIUS) { b->p.x = 3+RADIUS; b->v.x = fabsf(b->v.x)*.83f; }
    if(b->p.x > 53-RADIUS) { b->p.x = 53-RADIUS; b->v.x = -fabsf(b->v.x)*.83f; }
    if(b->p.y < 14+RADIUS) { b->p.y = 14+RADIUS; b->v.y = fabsf(b->v.y)*.83f; }
    for(unsigned i = 0; i < table->rail_count; i++) {
        const PbRail* rail = &table->rails[i];
        pinball_contact(b, closest(rail->a,rail->b,b->p), .5f, (PbVec){0,0}, rail->bounce);
    }
    for(unsigned i = 0; i < PB_BUMPERS; i++) {
        const PbBumper* bumper = &table->bumpers[i];
        b->contacts[i] = fmaxf(0,b->contacts[i]-PB_STEP);
        bool hit = pinball_contact(b, bumper->p, bumper->radius, (PbVec){0,0}, .95f);
        if(hit && b->contacts[i] <= 0 && !g->tilted) {
            PbVec n = sub(b->p,bumper->p);
            float d = fmaxf(.001f,length(n));
            b->v.x += n.x/d*45;
            b->v.y += n.y/d*45;
            b->contacts[i] = .13f;
            g->combo = g->combo_time > 0 && g->last_bumper != i ?
                (g->combo < 3 ? g->combo+1 : 3) : 1;
            g->last_bumper = (uint8_t)i;
            g->combo_time = 1.5f;
            g->bumper_flash[i] = .1f;
            add_score(g, 50*g->combo);
            g->events |= PbSoundBumper;
        }
    }
    for(unsigned i = 0; i < PB_TARGETS; i++) {
        if(!(g->target_mask & (1U<<i)) && length(sub(b->p, table->targets[i])) < 4.3f && !g->tilted) {
            g->target_mask |= (uint8_t)(1U<<i);
            add_score(g,100);
            g->events |= PbSoundTarget;
            if(b->age < 1.1f) {
                add_score(g,250);
                b->age = 1.2f;
                g->notice = PbNoticeSkill;
                g->notice_time = 1;
            }
            if(g->target_mask == 7) mission(g);
        }
    }
    for(unsigned i = 0; i < 2; i++) {
        PbFlipper* f = &g->flippers[i];
        PbVec tip = {f->pivot.x+16*cosf(f->angle), f->pivot.y+16*sinf(f->angle)};
        PbVec c = closest(f->pivot,tip,b->p), arm = sub(c,f->pivot);
        PbVec surface = {-f->omega*arm.y, f->omega*arm.x};
        if(pinball_contact(b,c,1.35f,surface,.65f) && fabsf(f->omega) > 1)
            g->events |= PbSoundFlipper;
    }
    if(b->p.y > 125) b->active = false;
}

static void step(Pinball* g) {
    g->elapsed += PB_STEP;
    g->notice_time = fmaxf(0,g->notice_time-PB_STEP);
    g->combo_time = fmaxf(0,g->combo_time-PB_STEP);
    g->nudge_heat = fmaxf(0,g->nudge_heat-PB_STEP*.4f);
    g->nudge_cooldown = fmaxf(0,g->nudge_cooldown-PB_STEP);
    for(unsigned i = 0; i < PB_BUMPERS; i++)
        g->bumper_flash[i] = fmaxf(0,g->bumper_flash[i]-PB_STEP);
    step_flippers(g);
    if(g->phase == PbReady) {
        if(g->controls & PbLaunch) g->charge = fminf(1,g->charge+PB_STEP*.8f);
        return;
    }
    if(g->phase == PbBallLost) {
        g->phase_time -= PB_STEP;
        if(g->phase_time <= 0) prepare_ball(g);
        return;
    }
    if(g->phase != PbPlaying) return;
    bool was_active = false, remaining = false;
    float age = 0;
    for(unsigned i = 0; i < PB_BALLS; i++) {
        PbBall* b = &g->balls[i];
        if(!b->active) continue;
        was_active = true;
        age = fmaxf(age,b->age);
        step_ball(g,b);
        remaining |= b->active;
    }
    if(was_active && !remaining) {
        if(age < 5 && !g->saved_this_ball && !g->tilted) {
            g->saved_this_ball = true;
            prepare_ball(g);
            g->notice = PbNoticeSaved;
            g->notice_time = 1.5f;
        } else {
            g->lives--;
            g->saved_this_ball = false;
            g->phase = g->lives ? PbBallLost : PbGameOver;
            g->phase_time = 1;
            g->events |= PbSoundDrain;
        }
    }
}

void pinball_advance(Pinball* g, uint32_t elapsed_ms) {
    /* One fixed clock for all hosts. Long suspension never causes a catch-up storm. */
    if(elapsed_ms > 100) elapsed_ms = 100;
    g->accumulator += elapsed_ms*120;
    while(g->accumulator >= 1000) {
        g->accumulator -= 1000;
        step(g);
    }
}


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
