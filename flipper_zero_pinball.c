#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include "pinball_render.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    Pinball game;
    PixelScreen screen;
    PbScreen view;
    FuriMutex* mutex;
    FuriMessageQueue* input;
    uint32_t held, best[2];
    bool sound, haptics, speaker, vibrating, dirty;
    uint32_t sound_until, vibration_until, last_save;
    uint8_t sound_priority;
} PinballApp;

static uint32_t milliseconds(void) {
    return (uint32_t)((uint64_t)furi_get_tick()*1000/furi_kernel_get_tick_frequency());
}
static void feedback_stop(PinballApp* a) {
    if(a->speaker) {
        furi_hal_speaker_stop(); furi_hal_speaker_release(); a->speaker = false;
    }
    if(a->vibrating) { furi_hal_vibro_on(false); a->vibrating = false; }
}
static void feedback(PinballApp* a, uint8_t events, uint32_t now) {
    if(a->speaker && (int32_t)(now-a->sound_until) >= 0) {
        furi_hal_speaker_stop(); furi_hal_speaker_release(); a->speaker = false;
    }
    if(a->vibrating && (int32_t)(now-a->vibration_until) >= 0) {
        furi_hal_vibro_on(false); a->vibrating = false;
    }
    if(!events) return;
    unsigned duration = 18, priority = 1;
    float frequency = 440;
    if(events & PbSoundBumper) { frequency = 880; duration = 24; priority = 2; }
    if(events & PbSoundLaunch) { frequency = 620; duration = 40; priority = 3; }
    if(events & PbSoundTarget) { frequency = 1320; duration = 65; priority = 4; }
    if(events & PbSoundMission) { frequency = 1760; duration = 120; priority = 5; }
    if(events & PbSoundDrain) { frequency = 180; duration = 160; priority = 6; }
    if(events & PbSoundTilt) { frequency = 130; duration = 180; priority = 7; }
    if(a->sound && (!a->speaker || priority >= a->sound_priority)) {
        /* Never wait for the speaker or for a tone to finish. */
        if(a->speaker || furi_hal_speaker_acquire(0)) {
            a->speaker = true; a->sound_priority = (uint8_t)priority;
            a->sound_until = now+duration;
            furi_hal_speaker_start(frequency,.35f);
        }
    }
    if(a->haptics && (events & (PbSoundBumper|PbSoundMission|PbSoundTilt))) {
        furi_hal_vibro_on(true); a->vibrating = true;
        a->vibration_until = now + (priority >= 5 ? 55 : 18);
    }
}
static uint32_t checksum(const uint8_t* data, unsigned count) {
    uint32_t n = 2166136261U;
    for(unsigned i = 0; i < count; i++) n = (n^data[i])*16777619U;
    return n;
}
static void saved_state(PinballApp* a, bool write) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    uint8_t data[20] = {'P','B','Z',1,0,0,0,0};
    if(write) {
        data[4] = a->sound; data[5] = a->haptics;
        memcpy(data+8,a->best,sizeof(a->best));
        uint32_t sum = checksum(data,16); memcpy(data+16,&sum,4);
        storage_simply_mkdir(storage,APP_DATA_PATH(""));
        if(storage_file_open(file,APP_DATA_PATH("scores.tmp"),FSAM_WRITE,FSOM_CREATE_ALWAYS)) {
            bool ok = storage_file_write(file,data,sizeof(data)) == sizeof(data);
            ok = storage_file_sync(file) && ok;
            storage_file_close(file);
            if(ok && storage_common_rename(storage,APP_DATA_PATH("scores.tmp"),
                                            APP_DATA_PATH("scores.bin")) == FSE_OK)
                a->dirty = false;
        }
    } else if(storage_file_open(file,APP_DATA_PATH("scores.bin"),FSAM_READ,FSOM_OPEN_EXISTING)) {
        if(storage_file_size(file) == sizeof(data) &&
           storage_file_read(file,data,sizeof(data)) == sizeof(data)) {
            uint32_t sum; memcpy(&sum,data+16,4);
            if(memcmp(data,"PBZ\1",4) == 0 && data[4] <= 1 && data[5] <= 1 &&
               checksum(data,16) == sum) {
                a->sound = data[4]; a->haptics = data[5];
                memcpy(a->best,data+8,sizeof(a->best));
            }
        }
        storage_file_close(file);
    }
    storage_file_free(file); furi_record_close(RECORD_STORAGE);
}
static void remember_score(PinballApp* a) {
    if(a->game.score > a->best[a->game.table]) {
        a->best[a->game.table] = a->game.score; a->dirty = true;
    }
}
static void draw(Canvas* canvas, void* context) {
    PinballApp* a = context;
    furi_mutex_acquire(a->mutex,FuriWaitForever);
    canvas_clear(canvas); canvas_draw_xbm(canvas,0,0,64,128,a->screen.bits);
    furi_mutex_release(a->mutex);
}
static void input(InputEvent* event, void* context) {
    PinballApp* a = context;
    uint32_t mask = 1U << event->key;
    /* Held state bypasses the bounded action queue, so releases cannot stick. */
    if(event->type == InputTypePress) __atomic_fetch_or(&a->held,mask,__ATOMIC_RELAXED);
    else if(event->type == InputTypeRelease) __atomic_fetch_and(&a->held,~mask,__ATOMIC_RELAXED);
    if(event->type == InputTypePress) furi_message_queue_put(a->input,event,0);
}
static bool action(PinballApp* a, InputKey key) {
    if(a->view == PbScreenMenu) {
        if(key == InputKeyBack) return false;
        if(key == InputKeyLeft || key == InputKeyRight) pinball_init(&a->game,1-a->game.table);
        if(key == InputKeyUp) { a->sound = !a->sound; a->dirty = true; feedback_stop(a); }
        if(key == InputKeyDown) { a->haptics = !a->haptics; a->dirty = true; feedback_stop(a); }
        if(key == InputKeyOk) { pinball_init(&a->game,a->game.table); a->view = PbScreenHelp; }
    } else if(a->view == PbScreenHelp) {
        if(key == InputKeyOk) a->view = PbScreenGame;
        if(key == InputKeyBack) a->view = PbScreenMenu;
    } else if(a->view == PbScreenPause) {
        if(key == InputKeyOk) a->view = PbScreenGame;
        if(key == InputKeyBack) { remember_score(a); a->view = PbScreenMenu; }
    } else if(a->game.phase == PbGameOver) {
        if(key == InputKeyOk) pinball_init(&a->game,a->game.table);
        if(key == InputKeyBack) a->view = PbScreenMenu;
    } else {
        if(key == InputKeyBack) { a->view = PbScreenPause; feedback_stop(a); }
        if(key == InputKeyOk && a->game.phase == PbPlaying) pinball_nudge(&a->game);
    }
    return true;
}
int32_t flipper_zero_pinball_app(void* context) {
    UNUSED(context);
    PinballApp* a = calloc(1,sizeof(PinballApp));
    furi_check(a);
    a->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    a->input = furi_message_queue_alloc(16,sizeof(InputEvent));
    a->sound = true; a->view = PbScreenMenu;
    pinball_init(&a->game,0); saved_state(a,false);
    pinball_render(&a->screen,&a->game,a->view,a->best[0],a->sound,a->haptics);
    ViewPort* viewport = view_port_alloc();
    view_port_set_orientation(viewport,ViewPortOrientationVertical);
    view_port_draw_callback_set(viewport,draw,a); view_port_input_callback_set(viewport,input,a);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui,viewport,GuiLayerFullscreen);
    uint32_t last = milliseconds(), last_draw = last;
    bool running = true;
    while(running) {
        InputEvent event;
        FuriStatus received = furi_message_queue_get(a->input,&event,10);
        uint32_t now = milliseconds();
        if(received == FuriStatusOk) running = action(a,event.key);
        uint32_t held = __atomic_load_n(&a->held,__ATOMIC_RELAXED);
        if(a->view == PbScreenGame && running) {
            uint8_t controls = 0;
            if(held & ((1U<<InputKeyUp)|(1U<<InputKeyLeft))) controls |= PbLeft;
            if(held & ((1U<<InputKeyDown)|(1U<<InputKeyRight))) controls |= PbRight;
            if(held & (1U<<InputKeyOk)) controls |= PbLaunch;
            pinball_controls(&a->game,controls); pinball_advance(&a->game,now-last);
            if(a->game.phase == PbGameOver) remember_score(a);
        } else a->game.controls = 0;
        last = now;
        feedback(a,a->game.events,now); a->game.events = 0;
        if(now-last_draw >= 33 || received == FuriStatusOk) {
            furi_mutex_acquire(a->mutex,FuriWaitForever);
            pinball_render(&a->screen,&a->game,a->view,a->best[a->game.table],a->sound,a->haptics);
            furi_mutex_release(a->mutex);
            view_port_update(viewport); last_draw = now;
        }
        if(a->dirty && now-a->last_save>=2000 &&
           (a->view == PbScreenMenu || a->game.phase == PbGameOver)) {
            saved_state(a,true); a->last_save=now;
        }
    }
    feedback_stop(a); remember_score(a);
    if(a->dirty) saved_state(a,true);
    view_port_enabled_set(viewport,false); gui_remove_view_port(gui,viewport); view_port_free(viewport);
    furi_record_close(RECORD_GUI); furi_message_queue_free(a->input); furi_mutex_free(a->mutex);
    free(a); return 0;
}
