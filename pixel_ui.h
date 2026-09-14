#pragma once

/* Shared, SDK-independent 64x128 one-bit renderer. XBM byte order. */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint8_t bits[1024]; } PixelScreen;

static inline void px_clear(PixelScreen* s) { memset(s->bits, 0, sizeof(s->bits)); }
static inline void px_dot(PixelScreen* s, int x, int y, bool on) {
    if(x < 0 || x >= 64 || y < 0 || y >= 128) return;
    uint8_t mask = (uint8_t)(1U << (x & 7));
    if(on) s->bits[y * 8 + x / 8] |= mask;
    else s->bits[y * 8 + x / 8] &= (uint8_t)~mask;
}
static inline void px_line(PixelScreen* s, int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for(;;) {
        px_dot(s, x0, y0, true);
        if(x0 == x1 && y0 == y1) break;
        int e2 = 2 * e;
        if(e2 >= dy) { e += dy; x0 += sx; }
        if(e2 <= dx) { e += dx; y0 += sy; }
    }
}
static inline void px_box(PixelScreen* s, int x, int y, int w, int h, bool fill) {
    for(int j = 0; j < h; j++) for(int i = 0; i < w; i++)
        if(fill || i == 0 || j == 0 || i == w - 1 || j == h - 1)
            px_dot(s, x + i, y + j, true);
}
static inline void px_erase(PixelScreen* s, int x, int y, int w, int h) {
    for(int j = 0; j < h; j++) for(int i = 0; i < w; i++) px_dot(s, x+i, y+j, false);
}
static inline void px_circle(PixelScreen* s, int x, int y, int r, bool fill) {
    for(int j = -r; j <= r; j++) for(int i = -r; i <= r; i++) {
        int d = i*i + j*j;
        if(d <= r*r && (fill || d >= (r-1)*(r-1))) px_dot(s, x+i, y+j, true);
    }
}
static inline uint16_t px_glyph(char c) {
    /* Five rows of three bits, most significant row first. */
    static const uint16_t digits[] = {
        0x7B6F,0x2492,0x73E7,0x73CF,0x5BC9,0x79CF,0x79EF,0x7249,0x7BEF,0x7BCF};
    static const uint16_t letters[] = {
        0x2BED,0x6BAE,0x7927,0x6B6E,0x79A7,0x79A4,0x796F,0x5BED,0x7497,
        0x124E,0x5BAD,0x4927,0x5FED,0x5FFD,0x2B6A,0x7BE4,0x2B7B,0x6BAD,
        0x79CF,0x7492,0x5B6F,0x5B6A,0x5BFD,0x5AAD,0x5A92,0x72A7};
    if(c >= '0' && c <= '9') return digits[c-'0'];
    if(c >= 'a' && c <= 'z') c = (char)(c-'a'+'A');
    if(c >= 'A' && c <= 'Z') return letters[c-'A'];
    switch(c) {
    case '-': return 0x01C0;
    case '+': return 0x05D0;
    case ':': return 0x0410;
    case '.': return 0x0002;
    case '/': return 0x12A4;
    case '>': return 0x4454;
    case '<': return 0x1511;
    case '!': return 0x2482;
    case '?': return 0x7282;
    default: return 0;
    }
}
static inline void px_text(PixelScreen* s, int x, int y, const char* str, int scale) {
    for(; *str; str++, x += 4*scale) {
        uint16_t bits = px_glyph(*str);
        for(int row = 0; row < 5; row++) for(int col = 0; col < 3; col++)
            if(bits & (1U << (14 - row*3-col)))
                px_box(s, x+col*scale, y+row*scale, scale, scale, true);
    }
}
static inline void px_center(PixelScreen* s, int y, const char* str, int scale) {
    px_text(s, (64-(int)strlen(str)*4*scale+scale)/2, y, str, scale);
}
