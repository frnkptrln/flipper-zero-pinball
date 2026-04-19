#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

/* ──────────────────────────────────────────────
 *  Constants
 * ────────────────────────────────────────────── */

#define SCREEN_W 64
#define SCREEN_H 128

/* Playing field boundaries */
#define FIELD_LEFT   1
#define FIELD_RIGHT  52
#define FIELD_TOP    11
#define FIELD_BOTTOM 118

/* Launch lane (right strip) */
#define LANE_LEFT  53
#define LANE_RIGHT 63

/* Physics */
#define GRAVITY        0.12f
#define WALL_DAMPING   0.75f
#define BUMPER_IMPULSE 3.0f
#define FLIPPER_HIT    4.5f
#define MAX_SPEED      6.0f
#define LAUNCH_MAX     6.0f
#define LAUNCH_CHARGE  0.02f  /* per tick */

/* Flipper geometry */
#define FLIPPER_LEN     18.0f
#define FLIPPER_REST    0.6f   /* radians, hanging down */
#define FLIPPER_ACTIVE -0.6f   /* radians, swung up */

/* Ball */
#define BALL_RADIUS 2
#define BALL_START_X 57
#define BALL_START_Y 112

/* Bumpers */
#define NUM_BUMPERS 2

/* Tick rate */
#define TICK_INTERVAL_MS 33  /* ~30 FPS */

/* Sound durations */
#define SND_BUMPER_MS   50
#define SND_FLIPPER_MS  30
#define SND_LAUNCH_MS   80
#define SND_DRAIN_MS   200
#define SND_GAMEOVER_MS 400

/* ──────────────────────────────────────────────
 *  Data structures
 * ────────────────────────────────────────────── */

typedef struct {
    float x, y;
    float vx, vy;
    bool active;
} Ball;

typedef struct {
    float pivot_x, pivot_y;
    float angle;       /* current angle in radians */
    bool is_left;
    bool pressed;
} PinballFlipper;

typedef struct {
    float x, y;
    uint8_t radius;
    uint16_t points;
} Bumper;

typedef enum {
    StateMenu,
    StatePlaying,
    StateGameOver,
} GameState;

typedef struct {
    Ball ball;
    PinballFlipper left_flipper;
    PinballFlipper right_flipper;
    Bumper bumpers[NUM_BUMPERS];
    uint32_t score;
    uint8_t lives;
    GameState state;
    float launch_power;
    bool launching;
    bool ball_in_lane; /* ball is still in the launch lane */
    FuriMutex* mutex;
} PinballGame;

/* Event system */
typedef enum {
    EventTypeInput,
    EventTypeTick,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} GameEvent;

/* ──────────────────────────────────────────────
 *  Sound helpers
 * ────────────────────────────────────────────── */

static void play_sound(float freq, uint32_t duration_ms) {
    if(furi_hal_speaker_acquire(100)) {
        furi_hal_speaker_start(freq, 0.6f);
        furi_delay_ms(duration_ms);
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

/* ──────────────────────────────────────────────
 *  Game initialisation
 * ────────────────────────────────────────────── */

static void game_init(PinballGame* game) {
    /* Ball */
    game->ball.x = BALL_START_X;
    game->ball.y = BALL_START_Y;
    game->ball.vx = 0;
    game->ball.vy = 0;
    game->ball.active = false;

    /* Left flipper – pivot on the left side */
    game->left_flipper.pivot_x = 12.0f;
    game->left_flipper.pivot_y = 105.0f;
    game->left_flipper.angle = FLIPPER_REST;
    game->left_flipper.is_left = true;
    game->left_flipper.pressed = false;

    /* Right flipper – pivot on the right side */
    game->right_flipper.pivot_x = 42.0f;
    game->right_flipper.pivot_y = 105.0f;
    game->right_flipper.angle = (float)M_PI - FLIPPER_REST;
    game->right_flipper.is_left = false;
    game->right_flipper.pressed = false;

    /* Bumpers */
    game->bumpers[0].x = 20.0f;
    game->bumpers[0].y = 35.0f;
    game->bumpers[0].radius = 5;
    game->bumpers[0].points = 100;

    game->bumpers[1].x = 38.0f;
    game->bumpers[1].y = 55.0f;
    game->bumpers[1].radius = 5;
    game->bumpers[1].points = 50;

    game->score = 0;
    game->lives = 3;
    game->state = StatePlaying;
    game->launch_power = 0.0f;
    game->launching = false;
    game->ball_in_lane = true;
}

/* ──────────────────────────────────────────────
 *  Flipper angle helpers
 * ────────────────────────────────────────────── */

static void flipper_get_endpoint(
    const PinballFlipper* f,
    float* ex,
    float* ey) {
    if(f->is_left) {
        *ex = f->pivot_x + FLIPPER_LEN * cosf(f->angle);
        *ey = f->pivot_y + FLIPPER_LEN * sinf(f->angle);
    } else {
        *ex = f->pivot_x + FLIPPER_LEN * cosf(f->angle);
        *ey = f->pivot_y + FLIPPER_LEN * sinf(f->angle);
    }
}

static void flipper_update(PinballFlipper* f) {
    float target;
    if(f->is_left) {
        target = f->pressed ? FLIPPER_ACTIVE : FLIPPER_REST;
    } else {
        target = f->pressed ? ((float)M_PI - FLIPPER_ACTIVE)
                            : ((float)M_PI - FLIPPER_REST);
    }
    /* Snap to target (instant) */
    f->angle = target;
}

/* ──────────────────────────────────────────────
 *  Collision helpers
 * ────────────────────────────────────────────── */

static float clampf(float v, float lo, float hi) {
    if(v < lo) return lo;
    if(v > hi) return hi;
    return v;
}

static float dist2d(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrtf(dx * dx + dy * dy);
}

/* Closest point on line segment (px,py)-(qx,qy) to point (cx,cy) */
static void closest_point_on_segment(
    float px, float py, float qx, float qy,
    float cx, float cy,
    float* out_x, float* out_y) {
    float dx = qx - px;
    float dy = qy - py;
    float len2 = dx * dx + dy * dy;
    if(len2 < 0.001f) {
        *out_x = px;
        *out_y = py;
        return;
    }
    float t = ((cx - px) * dx + (cy - py) * dy) / len2;
    t = clampf(t, 0.0f, 1.0f);
    *out_x = px + t * dx;
    *out_y = py + t * dy;
}

/* ──────────────────────────────────────────────
 *  Physics update
 * ────────────────────────────────────────────── */

static void update_physics(PinballGame* game) {
    Ball* b = &game->ball;
    if(!b->active) return;

    /* Gravity */
    b->vy += GRAVITY;

    /* Clamp speed */
    if(b->vx > MAX_SPEED) b->vx = MAX_SPEED;
    if(b->vx < -MAX_SPEED) b->vx = -MAX_SPEED;
    if(b->vy > MAX_SPEED) b->vy = MAX_SPEED;
    if(b->vy < -MAX_SPEED) b->vy = -MAX_SPEED;

    /* Move */
    b->x += b->vx;
    b->y += b->vy;

    /* Determine horizontal bounds depending on whether ball is in lane */
    float left_wall = (float)FIELD_LEFT + BALL_RADIUS;
    float right_wall;
    float top_wall = (float)FIELD_TOP + BALL_RADIUS;

    if(game->ball_in_lane) {
        /* Ball is in the launch lane */
        right_wall = (float)LANE_RIGHT - BALL_RADIUS;
        float lane_left = (float)LANE_LEFT + BALL_RADIUS;

        if(b->x < lane_left) {
            /* Ball exited the lane to the left → now in the field */
            game->ball_in_lane = false;
        } else {
            /* Bounce within lane walls */
            if(b->x < lane_left) {
                b->x = lane_left;
                b->vx = fabsf(b->vx) * WALL_DAMPING;
            }
        }
    } else {
        right_wall = (float)FIELD_RIGHT - BALL_RADIUS;
    }

    /* Wall collisions – left */
    if(b->x < left_wall) {
        b->x = left_wall;
        b->vx = fabsf(b->vx) * WALL_DAMPING;
    }
    /* Wall collisions – right */
    if(b->x > right_wall) {
        b->x = right_wall;
        b->vx = -fabsf(b->vx) * WALL_DAMPING;
    }
    /* Wall collisions – top */
    if(b->y < top_wall) {
        b->y = top_wall;
        b->vy = fabsf(b->vy) * WALL_DAMPING;
    }

    /* Bumper collisions */
    for(int i = 0; i < NUM_BUMPERS; i++) {
        Bumper* bmp = &game->bumpers[i];
        float d = dist2d(b->x, b->y, bmp->x, bmp->y);
        float min_dist = (float)BALL_RADIUS + (float)bmp->radius;
        if(d < min_dist && d > 0.1f) {
            /* Push ball away from bumper */
            float nx = (b->x - bmp->x) / d;
            float ny = (b->y - bmp->y) / d;
            b->x = bmp->x + nx * (min_dist + 1.0f);
            b->y = bmp->y + ny * (min_dist + 1.0f);
            b->vx = nx * BUMPER_IMPULSE;
            b->vy = ny * BUMPER_IMPULSE;
            game->score += bmp->points;
            play_sound(800.0f, SND_BUMPER_MS);
        }
    }

    /* Flipper collisions */
    PinballFlipper* flippers[2] = {&game->left_flipper, &game->right_flipper};
    for(int i = 0; i < 2; i++) {
        PinballFlipper* f = flippers[i];
        float ex, ey;
        flipper_get_endpoint(f, &ex, &ey);

        float cpx, cpy;
        closest_point_on_segment(
            f->pivot_x, f->pivot_y, ex, ey, b->x, b->y, &cpx, &cpy);

        float d = dist2d(b->x, b->y, cpx, cpy);
        float min_dist = (float)BALL_RADIUS + 2.0f; /* 2px flipper thickness */

        if(d < min_dist && d > 0.1f) {
            float nx = (b->x - cpx) / d;
            float ny = (b->y - cpy) / d;

            /* Push ball out of flipper */
            b->x = cpx + nx * (min_dist + 1.0f);
            b->y = cpy + ny * (min_dist + 1.0f);

            if(f->pressed) {
                /* Flipper is active: strong upward hit */
                b->vx = nx * FLIPPER_HIT * 0.5f;
                b->vy = -fabsf(FLIPPER_HIT);
                play_sound(400.0f, SND_FLIPPER_MS);
            } else {
                /* Just bounce off resting flipper */
                b->vx = nx * WALL_DAMPING * 2.0f;
                b->vy = ny * WALL_DAMPING * 2.0f;
            }
        }
    }

    /* Drain – ball fell past the bottom */
    if(b->y > FIELD_BOTTOM + BALL_RADIUS) {
        b->active = false;
        game->lives--;
        if(game->lives == 0) {
            game->state = StateGameOver;
            play_sound(150.0f, SND_GAMEOVER_MS);
        } else {
            play_sound(200.0f, SND_DRAIN_MS);
            /* Reset ball to launch lane */
            b->x = BALL_START_X;
            b->y = BALL_START_Y;
            b->vx = 0;
            b->vy = 0;
            game->ball_in_lane = true;
            game->launch_power = 0;
            game->launching = false;
        }
    }
}

/* ──────────────────────────────────────────────
 *  Drawing helpers
 * ────────────────────────────────────────────── */

static void draw_flipper_arm(Canvas* canvas, const PinballFlipper* f) {
    float ex, ey;
    flipper_get_endpoint(f, &ex, &ey);

    int px = (int)f->pivot_x;
    int py = (int)f->pivot_y;
    int epx = (int)ex;
    int epy = (int)ey;

    /* Draw the arm as 3 parallel lines for thickness */
    canvas_draw_line(canvas, px, py, epx, epy);
    canvas_draw_line(canvas, px, py - 1, epx, epy - 1);
    canvas_draw_line(canvas, px, py + 1, epx, epy + 1);

    /* Pivot dot */
    canvas_draw_disc(canvas, px, py, 2);
}

/* ──────────────────────────────────────────────
 *  Render callback
 * ────────────────────────────────────────────── */

static void render_callback(Canvas* canvas, void* ctx) {
    PinballGame* game = (PinballGame*)ctx;
    furi_mutex_acquire(game->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(game->state == StateMenu) {
        /* ── Title Screen ── */
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2, 40, AlignCenter, AlignCenter, "PINBALL");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2, 60, AlignCenter, AlignCenter, "Press OK");
        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2, 72, AlignCenter, AlignCenter, "to start!");

        /* Decorative ball */
        canvas_draw_disc(canvas, SCREEN_W / 2, 90, 4);
        canvas_draw_circle(canvas, SCREEN_W / 2, 90, 6);

    } else if(game->state == StatePlaying) {
        /* ── HUD ── */
        char score_buf[16];
        snprintf(score_buf, sizeof(score_buf), "%lu", (unsigned long)game->score);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 8, score_buf);

        /* Lives as small filled circles */
        for(uint8_t i = 0; i < game->lives; i++) {
            canvas_draw_disc(canvas, 46 + i * 7, 5, 2);
        }

        /* ── Playing field border ── */
        canvas_draw_frame(canvas, 0, 10, FIELD_RIGHT + 1, FIELD_BOTTOM - 9);

        /* Lane separator */
        canvas_draw_line(canvas, LANE_LEFT, FIELD_TOP, LANE_LEFT, FIELD_BOTTOM);

        /* ── Bumpers ── */
        for(int i = 0; i < NUM_BUMPERS; i++) {
            Bumper* bmp = &game->bumpers[i];
            canvas_draw_disc(canvas, (int)bmp->x, (int)bmp->y, bmp->radius);
            /* Inner ring for visual flair */
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_disc(canvas, (int)bmp->x, (int)bmp->y, bmp->radius - 2);
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_circle(
                canvas, (int)bmp->x, (int)bmp->y, bmp->radius - 2);
        }

        /* ── Flipper arms ── */
        draw_flipper_arm(canvas, &game->left_flipper);
        draw_flipper_arm(canvas, &game->right_flipper);

        /* ── Ball ── */
        if(game->ball.active) {
            canvas_draw_disc(
                canvas, (int)game->ball.x, (int)game->ball.y, BALL_RADIUS);
        } else if(game->ball_in_lane) {
            /* Show ball sitting in lane waiting for launch */
            canvas_draw_disc(canvas, BALL_START_X, BALL_START_Y, BALL_RADIUS);
        }

        /* ── Launch power bar ── */
        if(game->launching) {
            int bar_h = (int)(game->launch_power * 40.0f);
            canvas_draw_frame(canvas, 56, 70, 5, 42);
            if(bar_h > 0) {
                canvas_draw_box(canvas, 57, 112 - bar_h, 3, bar_h);
            }
        }

    } else if(game->state == StateGameOver) {
        /* ── Game Over Screen ── */
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2, 35, AlignCenter, AlignCenter, "GAME");
        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2, 50, AlignCenter, AlignCenter, "OVER");

        char score_buf[24];
        snprintf(
            score_buf, sizeof(score_buf), "Score: %lu",
            (unsigned long)game->score);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2, 70, AlignCenter, AlignCenter, score_buf);

        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2, 90, AlignCenter, AlignCenter, "OK = Retry");
        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2, 102, AlignCenter, AlignCenter, "Back = Exit");
    }

    furi_mutex_release(game->mutex);
}

/* ──────────────────────────────────────────────
 *  Input callback
 * ────────────────────────────────────────────── */

static void input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* queue = (FuriMessageQueue*)ctx;
    GameEvent event = {.type = EventTypeInput, .input = *input_event};
    furi_message_queue_put(queue, &event, FuriWaitForever);
}

/* ──────────────────────────────────────────────
 *  Tick timer callback
 * ────────────────────────────────────────────── */

static void tick_callback(void* ctx) {
    FuriMessageQueue* queue = (FuriMessageQueue*)ctx;
    GameEvent event = {.type = EventTypeTick};
    furi_message_queue_put(queue, &event, 0);
}

/* ──────────────────────────────────────────────
 *  Input handler
 * ────────────────────────────────────────────── */

static bool handle_input(PinballGame* game, InputEvent* input, bool* running) {
    if(game->state == StateMenu) {
        if(input->key == InputKeyOk && input->type == InputTypeShort) {
            game_init(game);
            return true;
        }
        if(input->key == InputKeyBack) {
            *running = false;
            return true;
        }
        return false;
    }

    if(game->state == StateGameOver) {
        if(input->key == InputKeyOk && input->type == InputTypeShort) {
            game->state = StateMenu;
            return true;
        }
        if(input->key == InputKeyBack) {
            *running = false;
            return true;
        }
        return false;
    }

    /* ── StatePlaying ── */

    /* Back button exits */
    if(input->key == InputKeyBack) {
        *running = false;
        return true;
    }

    /*
     * Flipper controls:
     *   When holding the Flipper Zero sideways (landscape → portrait via
     *   ViewPortOrientationVertical), the physical Up/Down buttons become
     *   the left/right inputs for the player.
     *
     *   InputKeyUp   → Left flipper
     *   InputKeyDown → Right flipper
     *
     *   We also support Left/Right as alternative mapping.
     */

    /* Left flipper: Up or Left */
    if(input->key == InputKeyUp || input->key == InputKeyLeft) {
        if(input->type == InputTypePress) {
            game->left_flipper.pressed = true;
        } else if(input->type == InputTypeRelease) {
            game->left_flipper.pressed = false;
        }
        return true;
    }

    /* Right flipper: Down or Right */
    if(input->key == InputKeyDown || input->key == InputKeyRight) {
        if(input->type == InputTypePress) {
            game->right_flipper.pressed = true;
        } else if(input->type == InputTypeRelease) {
            game->right_flipper.pressed = false;
        }
        return true;
    }

    /* OK button: ball launch */
    if(input->key == InputKeyOk) {
        if(!game->ball.active && game->ball_in_lane) {
            if(input->type == InputTypePress) {
                game->launching = true;
                game->launch_power = 0.0f;
            } else if(input->type == InputTypeRelease && game->launching) {
                /* Fire the ball! */
                game->ball.active = true;
                game->ball.vx = 0;
                game->ball.vy = -(game->launch_power * LAUNCH_MAX + 1.5f);
                game->launching = false;
                play_sound(600.0f, SND_LAUNCH_MS);
            }
        }
        return true;
    }

    return false;
}

/* ──────────────────────────────────────────────
 *  Main application entry point
 * ────────────────────────────────────────────── */

int32_t flipper_zero_pinball_app(void* p) {
    UNUSED(p);

    /* Allocate game state */
    PinballGame* game = malloc(sizeof(PinballGame));
    memset(game, 0, sizeof(PinballGame));
    game->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    game->state = StateMenu;
    game->lives = 3;

    /* Message queue for events */
    FuriMessageQueue* event_queue =
        furi_message_queue_alloc(8, sizeof(GameEvent));

    /* ViewPort setup */
    ViewPort* view_port = view_port_alloc();
    view_port_set_orientation(view_port, ViewPortOrientationVertical);
    view_port_draw_callback_set(view_port, render_callback, game);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    /* Register with GUI */
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    /* Game tick timer (~30 FPS) */
    FuriTimer* timer =
        furi_timer_alloc(tick_callback, FuriTimerTypePeriodic, event_queue);
    furi_timer_start(timer, furi_kernel_get_tick_frequency() / 30);

    /* ── Main event loop ── */
    bool running = true;
    GameEvent event;

    while(running) {
        FuriStatus status =
            furi_message_queue_get(event_queue, &event, 100);

        if(status != FuriStatusOk) continue;

        furi_mutex_acquire(game->mutex, FuriWaitForever);

        if(event.type == EventTypeTick) {
            if(game->state == StatePlaying) {
                /* Update flipper positions */
                flipper_update(&game->left_flipper);
                flipper_update(&game->right_flipper);

                /* Charge launch power while holding OK */
                if(game->launching) {
                    game->launch_power += LAUNCH_CHARGE;
                    if(game->launch_power > 1.0f) {
                        game->launch_power = 1.0f;
                    }
                }

                /* Run physics */
                update_physics(game);
            }
        } else if(event.type == EventTypeInput) {
            handle_input(game, &event.input, &running);
        }

        furi_mutex_release(game->mutex);

        /* Request screen redraw */
        view_port_update(view_port);
    }

    /* ── Cleanup ── */
    furi_timer_stop(timer);
    furi_timer_free(timer);

    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);

    furi_message_queue_free(event_queue);
    furi_mutex_free(game->mutex);
    free(game);

    return 0;
}
