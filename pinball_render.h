#pragma once
#include "pinball_physics.h"
#include "pixel_ui.h"

typedef enum { PbScreenMenu, PbScreenGame, PbScreenPause, PbScreenHelp } PbScreen;
void pinball_render(PixelScreen* screen, const Pinball* game, PbScreen view,
                    uint32_t best, bool sound, bool haptics);
