/* Browser adapter: the portable C engine and one-bit renderer stay shared. */
#include "../pinball_render.h"

static Pinball game;
static PixelScreen screen;

void pb_init(unsigned table) { pinball_init(&game, table); }
void pb_controls(unsigned controls) { pinball_controls(&game, (uint8_t)controls); }
/* Losing focus cancels a held launcher instead of releasing it into play. */
void pb_cancel_controls(void) { game.controls = 0; game.charge = 0; }
void pb_nudge(void) { pinball_nudge(&game); }
unsigned pb_advance(unsigned milliseconds) {
    pinball_advance(&game, milliseconds);
    unsigned events = game.events;
    game.events = 0;
    return events;
}
const uint8_t* pb_render(unsigned view, unsigned best, unsigned sound) {
    pinball_render(&screen, &game, (PbScreen)view, best, sound != 0, false);
    return screen.bits;
}
unsigned pb_phase(void) { return (unsigned)game.phase; }
unsigned pb_score(void) { return game.score; }
unsigned pb_lives(void) { return game.lives; }
unsigned pb_table(void) { return game.table; }
unsigned pb_multiplier(void) { return game.multiplier; }
unsigned pb_tilted(void) { return game.tilted; }
unsigned pb_inputs(void) { return game.controls; }
float pb_charge(void) { return game.charge; }
float pb_ball_x(unsigned ball) { return game.balls[ball % PB_BALLS].p.x; }
float pb_ball_y(unsigned ball) { return game.balls[ball % PB_BALLS].p.y; }
unsigned pb_ball_active(unsigned ball) { return game.balls[ball % PB_BALLS].active; }
