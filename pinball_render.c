#include "pinball_render.h"
#include <math.h>
#include <stdio.h>

static int rounded(float x) { return (int)lroundf(x); }
static void number(PixelScreen* s, int x, int y, uint32_t n) {
    char text[12];
    snprintf(text, sizeof(text), "%lu", (unsigned long)n);
    px_text(s,x,y,text,1);
}
static void overlay(PixelScreen* s, int y, const char* line1, const char* line2) {
    px_erase(s,4,y,56,22);
    px_box(s,4,y,56,22,false);
    px_center(s,y+5,line1,1);
    if(line2) px_center(s,y+13,line2,1);
}
static void board(PixelScreen* s, const Pinball* g) {
    const PbTable* t = pinball_table(g->table);
    number(s,2,1,g->score);
    for(unsigned i = 0; i < g->lives; i++) px_circle(s,49+(int)i*6,3,1,true);
    px_line(s,1,9,62,9);
    px_line(s,2,18,2,122);
    px_line(s,2,18,9,13);
    px_line(s,9,13,54,13);
    px_line(s,54,13,62,21);
    px_line(s,62,21,62,123);
    px_line(s,55,28,55,123);
    px_line(s,55,123,62,123);
    for(unsigned i = 0; i < t->rail_count; i++) {
        const PbRail* r = &t->rails[i];
        px_line(s,rounded(r->a.x),rounded(r->a.y),rounded(r->b.x),rounded(r->b.y));
    }
    for(unsigned i = 0; i < PB_BUMPERS; i++) {
        int x = rounded(t->bumpers[i].p.x), y = rounded(t->bumpers[i].p.y);
        int radius = rounded(t->bumpers[i].radius);
        px_circle(s,x,y,radius,g->bumper_flash[i] > 0);
        if(g->bumper_flash[i] <= 0) px_circle(s,x,y,1,true);
    }
    for(unsigned i = 0; i < PB_TARGETS; i++) {
        int x = rounded(t->targets[i].x), y = rounded(t->targets[i].y);
        if(g->target_mask & (1U<<i)) px_box(s,x-2,y-3,5,7,true);
        else { char c[2] = {(char)('A'+i),0}; px_text(s,x-1,y-2,c,1); }
    }
    for(unsigned i = 0; i < 2; i++) {
        const PbFlipper* f = &g->flippers[i];
        int x = rounded(f->pivot.x), y = rounded(f->pivot.y);
        int tx = rounded(f->pivot.x+16*cosf(f->angle));
        int ty = rounded(f->pivot.y+16*sinf(f->angle));
        px_line(s,x,y,tx,ty);
        px_line(s,x,y+1,tx,ty+1);
        px_circle(s,x,y,2,true);
    }
    px_text(s,21,85,"X",1); number(s,26,85,g->multiplier);
    if(g->combo > 1 && g->combo_time > 0) { px_text(s,18,76,"COMBO",1); number(s,39,76,g->combo); }
    if(g->nudge_heat > .2f) {
        int h = (int)(fminf(g->nudge_heat,3)*6);
        px_box(s,0,120-h,1,h,true);
    }
    if(g->phase == PbReady) {
        px_center(s,119,"HOLD OK",1);
        px_box(s,57,92-(int)(g->charge*46),3,(int)(g->charge*46),true);
    }
    if(g->notice_time > 0) {
        const char* notices[] = {"", "BALL SAVED", "SKILL SHOT", "MULTIBALL!", "TILT"};
        px_erase(s,5,70,47,7);
        px_center(s,71,notices[g->notice],1);
    }
    /* A score notice must never erase the moving ball. */
    for(unsigned i = 0; i < PB_BALLS; i++) if(g->balls[i].active || (i == 0 && g->phase == PbReady)) {
        int x = rounded(g->balls[i].p.x), y = rounded(g->balls[i].p.y);
        px_circle(s,x,y,2,true);
        px_dot(s,x-1,y-1,false);
    }
}

void pinball_render(PixelScreen* s, const Pinball* g, PbScreen view,
                    uint32_t best, bool sound, bool haptics) {
    px_clear(s);
    if(view == PbScreenMenu) {
        for(int i = 0; i < 26; i++) {
            int x = (i*37+7)%64, y = (i*19+11)%60;
            if(y < 8 || y > 39) px_dot(s,x,y,true);
        }
        px_center(s,13,"PINBALL",2);
        px_center(s,28,"ZERO",2);
        px_line(s,8,47,55,47);
        px_circle(s,32,57,3,true);
        px_line(s,11,70,25,77); px_line(s,39,77,53,70);
        px_center(s,86,pinball_table(g->table)->name,1);
        px_text(s,3,86,"<",1); px_text(s,58,86,">",1);
        px_center(s,99,"OK PLAY",1);
        px_text(s,2,112,sound ? "SND ON" : "SND OFF",1);
        px_text(s,36,112,haptics ? "VIB ON" : "VIB OFF",1);
        px_text(s,2,121,"BEST",1); number(s,24,121,best);
        return;
    }
    if(view == PbScreenHelp) {
        px_center(s,6,pinball_table(g->table)->name,2);
        px_line(s,3,20,60,20);
        px_center(s,29,"LIGHT A B C",1);
        px_center(s,39,"FOR MULTIBALL",1);
        px_center(s,52,"HOLD OK: POWER",1);
        px_center(s,62,"IN PLAY: NUDGE",1);
        px_center(s,72,"BACK: PAUSE",1);
        px_center(s,86,"EARLY DRAIN?",1);
        px_center(s,95,"ONE BALL SAVE",1);
        px_center(s,115,"OK START",1);
        return;
    }
    board(s,g);
    if(view == PbScreenPause) overlay(s,45,"PAUSED","OK RESUME");
    else if(g->phase == PbBallLost) overlay(s,44,"BALL LOST","GET READY");
    else if(g->phase == PbGameOver) {
        px_erase(s,5,36,54,58);
        px_box(s,5,36,54,58,false);
        px_center(s,43,"GAME OVER",1);
        px_center(s,54,g->score >= best && g->score > 0 ? "NEW BEST!" : "FINAL SCORE",1);
        char text[12]; snprintf(text,sizeof(text),"%lu",(unsigned long)g->score);
        px_center(s,64,text,strlen(text)>6 ? 1 : 2);
        px_center(s,83,"OK AGAIN",1);
    }
    if(view == PbScreenPause) {
        px_erase(s,5,72,54,8);
        px_center(s,73,"BACK TO MENU",1);
    }
}
