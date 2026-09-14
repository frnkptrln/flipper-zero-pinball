#include "../pinball_render.h"
#include <stdio.h>
#include <stdlib.h>

static void save(PixelScreen* s, const char* path) {
    FILE* f = fopen(path,"wb");
    if(!f) exit(1);
    fprintf(f,"P4\n64 128\n");
    for(unsigned i=0;i<sizeof(s->bits);i++) {
        unsigned n=s->bits[i], r=0;
        for(unsigned b=0;b<8;b++) r = (r<<1)|((n>>b)&1U);
        fputc((int)r,f);
    }
    fclose(f);
}
int main(int argc, char** argv) {
    if(argc!=2) return 1;
    Pinball g; PixelScreen screen; char path[512];
    for(unsigned i=0;i<6;i++) {
        pinball_init(&g,i==3 ? 1 : 0);
        PbScreen view = i==0 ? PbScreenMenu : i==1 ? PbScreenHelp : PbScreenGame;
        if(i>=3) {
            g.phase=PbPlaying; g.score=1250; g.multiplier=2; g.target_mask=3;
            g.balls[0]=(PbBall){.p={34,83},.active=true};
        }
        if(i==4) view=PbScreenPause;
        if(i==5) g.phase=PbGameOver;
        pinball_render(&screen,&g,view,1000,true,false);
        snprintf(path,sizeof(path),"%s/screen-%u.pbm",argv[1],i); save(&screen,path);
    }
    pinball_init(&g,0); pinball_controls(&g,PbLaunch); pinball_controls(&g,0);
    for(unsigned i=0;i<180;i++) {
        uint8_t c=g.balls[0].p.y>87 ? (g.balls[0].p.x<27 ? PbLeft : PbRight) : 0;
        pinball_controls(&g,c); pinball_advance(&g,33);
        pinball_render(&screen,&g,PbScreenGame,0,true,false);
        snprintf(path,sizeof(path),"%s/frame-%03u.pbm",argv[1],i); save(&screen,path);
    }
    return 0;
}
