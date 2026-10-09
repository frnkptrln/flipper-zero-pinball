/* Independent native build of the same adapter, for WASM/host comparison. */
#include "../web/bridge.c"
#include <stdio.h>

int main(void) {
    for (unsigned table = 0; table < 2; table++) {
        pb_init(table);
        const uint8_t* bits = pb_render(PbScreenMenu, 1234, 0);
        uint32_t checksum = 2166136261U;
        for (unsigned i = 0; i < 1024; i++) checksum = (checksum ^ bits[i]) * 16777619U;
        printf("menu %u %u\n", table, checksum);
        for (unsigned frame = 0; frame < 240; frame++) {
            unsigned controls = frame < 20 ? PbLaunch : ((frame % 60 < 25) ? PbLeft : PbRight);
            pb_controls(controls);
            pb_advance(frame % 3 == 0 ? 16 : 17);
            printf("%u %u %u %u %u %.8f %.8f\n", table, frame, pb_phase(), pb_score(), pb_lives(), pb_ball_x(0), pb_ball_y(0));
        }
    }
    return 0;
}
