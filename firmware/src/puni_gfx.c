/* SPDX-License-Identifier: GPL-3.0-only */
/* PUNICORN drawing: the screen has no frame buffer, so a frame is a draw function that is run several times.
 *   1. the signature pass (pg.mark): every primitive adds a hash of its arguments to the 16 x 16 px tiles its box
 *      touches. A tile whose signature differs from the last frame's is dirty.
 *   2. the dirty tiles, in rows of runs: the draw function runs again for each run, clipped to it, into one half of
 *      the canvas (gfx.c cv_px) while the other half is still on its way to the panel (lcd_blit).
 * So nothing keeps track of what moved: whatever draws differently is sent, and only that. The panel takes about
 * 77 ms for a whole screen (12 MHz SPI); a unicorn that only blinks costs a few tiles.
 * Primitives: spans with a blend mode, rectangles, circles, ellipses, rings, convex polygons, capsules (the rig's
 * bones), lines, a sky gradient (dithered) and a 5 x 7 font. Coordinates are screen pixels; pg.ox / pg.oy shake
 * everything. */
#define PG_TILE 16
#define PG_NT 15
#define PG_HALF (CV_MAX / 2u)
enum { PB_SOLID, PB_HALF, PB_ADD, PB_STIP, PB_DARK };

static struct {
    uint16_t *px;
    int16_t x0, y0, x1, y1, stride;
    int16_t ox, oy;
    uint8_t mark, blend, full, flip;
    uint16_t *sig;
    uint32_t sent;               /* pixels sent this frame */
} pg;
static uint16_t pg_sigs[2][PG_NT * PG_NT] __attribute__((section(".pool")));

#define PU_NZ(t) ((t) ? (uint32_t)(t) : 1u)       /* a time stamp, 0 kept for "none" (never ahead of now, as t | 1 would be) */
#define PH4(a, b, c, d) ((uint32_t)(a) * 73856093u ^ (uint32_t)(b) * 19349663u ^ (uint32_t)(c) * 83492791u ^ (uint32_t)(d) * 2654435761u)

static inline uint32_t pg_hash(uint32_t a)
{
    a ^= a >> 15; a *= 0x2C1B3C6Du; a ^= a >> 12; a *= 0x297A2D39u; a ^= a >> 15;
    return a;
}
static inline int32_t pg_sin(int32_t a) { return SINE[a & 1023]; }          /* Q15, a: 1024 a turn */
static inline int32_t pg_cos(int32_t a) { return SINE[(a + 256) & 1023]; }

static uint32_t pg_isqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v)
        b >>= 2;
    while (b) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return r;
}

static inline uint16_t pg_mix(uint32_t a, uint32_t b, uint32_t t)           /* a .. b, t 0 .. 256 */
{
    uint32_t r = ((a >> 11) * (256u - t) + (b >> 11) * t) >> 8;
    uint32_t g = (((a >> 5) & 63u) * (256u - t) + ((b >> 5) & 63u) * t) >> 8;
    uint32_t bl = ((a & 31u) * (256u - t) + (b & 31u) * t) >> 8;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static uint16_t pg_hue(uint32_t h)                                          /* a bright colour, h: 256 a turn */
{
    uint32_t s = (h & 255u) * 6u, f = s & 255u, r, g, b;
    switch (s >> 8) {
    case 0: r = 255; g = f; b = 0; break;
    case 1: r = 255 - f; g = 255; b = 0; break;
    case 2: r = 0; g = 255; b = f; break;
    case 3: r = 0; g = 255 - f; b = 255; break;
    case 4: r = f; g = 0; b = 255; break;
    default: r = 255; g = 0; b = 255 - f; break;
    }
    return RGB(r, g, b);
}

static void pg_touch(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t h)   /* box [x0,x1) x [y0,y1) */
{
    int32_t tx, ty, tx0, tx1, ty0, ty1;
    if (x1 <= 0 || y1 <= 0 || x0 >= 240 || y0 >= 240 || x1 <= x0 || y1 <= y0)
        return;
    tx0 = x0 < 0 ? 0 : x0 / PG_TILE;
    ty0 = y0 < 0 ? 0 : y0 / PG_TILE;
    tx1 = x1 > 240 ? PG_NT - 1 : (x1 - 1) / PG_TILE;
    ty1 = y1 > 240 ? PG_NT - 1 : (y1 - 1) / PG_TILE;
    h = pg_hash(h + pg.blend);
    for (ty = ty0; ty <= ty1; ty++)
        for (tx = tx0; tx <= tx1; tx++) {
            uint16_t *s = &pg.sig[ty * PG_NT + tx];
            *s = (uint16_t)(*s * 40503u + h + (h >> 16));
        }
}
/* 1: the box is outside what is being drawn (a group of primitives can be skipped) */
static inline int pg_out(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    x0 += pg.ox; x1 += pg.ox; y0 += pg.oy; y1 += pg.oy;
    return !pg.mark && (x1 <= pg.x0 || x0 >= pg.x1 || y1 <= pg.y0 || y0 >= pg.y1);
}

static void pg_span(int32_t y, int32_t xa, int32_t xb, uint16_t c)          /* row y, [xa, xb) */
{
    uint16_t *p;
    int32_t n;
    if (y < pg.y0 || y >= pg.y1)
        return;
    if (xa < pg.x0) xa = pg.x0;
    if (xb > pg.x1) xb = pg.x1;
    n = xb - xa;
    if (n <= 0)
        return;
    p = pg.px + (y - pg.y0) * pg.stride + (xa - pg.x0);
    switch (pg.blend) {
    case PB_SOLID:
        while (n--)
            *p++ = c;
        break;
    case PB_HALF: {
        uint32_t cm = (c >> 1) & 0x7BEFu;
        for (; n--; p++)
            *p = (uint16_t)(((*p >> 1) & 0x7BEFu) + cm);
        break;
    }
    case PB_ADD:
        for (; n--; p++) {
            uint32_t d = *p, r = (d & 0xF800u) + (c & 0xF800u), g = (d & 0x07E0u) + (c & 0x07E0u), b = (d & 31u) + (c & 31u);
            *p = (uint16_t)((r > 0xF800u ? 0xF800u : r) | (g > 0x07E0u ? 0x07E0u : g) | (b > 31u ? 31u : b));
        }
        break;
    case PB_STIP:
        for (; n--; p++, xa++)
            if ((xa ^ y) & 1)
                *p = c;
        break;
    default:                                                                /* PB_DARK: what is there, halved */
        for (; n--; p++)
            *p = (uint16_t)((*p >> 1) & 0x7BEFu);
        break;
    }
}

static void pg_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    int32_t yy, y1;
    x += pg.ox; y += pg.oy;
    if (w <= 0 || h <= 0)
        return;
    if (pg.mark) {
        pg_touch(x, y, x + w, y + h, PH4(x, y, w, h) ^ c);
        return;
    }
    yy = y < pg.y0 ? pg.y0 : y;
    y1 = y + h > pg.y1 ? pg.y1 : y + h;
    for (; yy < y1; yy++)
        pg_span(yy, x, x + w, c);
}

static void pg_circle(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t dy, lo, hi;
    cx += pg.ox; cy += pg.oy;
    if (r < 0)
        return;
    if (pg.mark) {
        pg_touch(cx - r, cy - r, cx + r + 1, cy + r + 1, PH4(cx, cy, r, c) ^ 0xC1u);
        return;
    }
    if (cx + r < pg.x0 || cx - r >= pg.x1)
        return;
    lo = pg.y0 - cy > -r ? pg.y0 - cy : -r;
    hi = pg.y1 - 1 - cy < r ? pg.y1 - 1 - cy : r;
    for (dy = lo; dy <= hi; dy++) {
        int32_t x = (int32_t)pg_isqrt((uint32_t)(r * r - dy * dy + r));
        pg_span(cy + dy, cx - x, cx + x + 1, c);
    }
}

static void pg_ellipse(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint16_t c)
{
    int32_t dy, lo, hi;
    cx += pg.ox; cy += pg.oy;
    if (rx < 0 || ry < 0)
        return;
    if (pg.mark) {
        pg_touch(cx - rx, cy - ry, cx + rx + 1, cy + ry + 1, PH4(cx, cy, rx * 512 + ry, c) ^ 0xE1u);
        return;
    }
    if (cx + rx < pg.x0 || cx - rx >= pg.x1)
        return;
    if (!ry) {
        pg_span(cy, cx - rx, cx + rx + 1, c);
        return;
    }
    lo = pg.y0 - cy > -ry ? pg.y0 - cy : -ry;
    hi = pg.y1 - 1 - cy < ry ? pg.y1 - 1 - cy : ry;
    for (dy = lo; dy <= hi; dy++) {
        int32_t x = (int32_t)(pg_isqrt((uint32_t)(ry * ry - dy * dy) << 8) * (uint32_t)rx / (uint32_t)(ry * 16));
        pg_span(cy + dy, cx - x, cx + x + 1, c);
    }
}

/* a ring: radius r, th thick (inwards) */
static void pg_ring(int32_t cx, int32_t cy, int32_t r, int32_t th, uint16_t c)
{
    int32_t dy, lo, hi, ri = r - th;
    cx += pg.ox; cy += pg.oy;
    if (r < 0)
        return;
    if (pg.mark) {
        pg_touch(cx - r, cy - r, cx + r + 1, cy + r + 1, PH4(cx, cy, r * 64 + th, c) ^ 0x51u);
        return;
    }
    if (cx + r < pg.x0 || cx - r >= pg.x1)
        return;
    lo = pg.y0 - cy > -r ? pg.y0 - cy : -r;
    hi = pg.y1 - 1 - cy < r ? pg.y1 - 1 - cy : r;
    for (dy = lo; dy <= hi; dy++) {
        int32_t x = (int32_t)pg_isqrt((uint32_t)(r * r - dy * dy + r));
        if (ri > 0 && dy > -ri && dy < ri) {
            int32_t xi = (int32_t)pg_isqrt((uint32_t)(ri * ri - dy * dy));
            pg_span(cy + dy, cx - x, cx - xi + 1, c);
            pg_span(cy + dy, cx + xi, cx + x + 1, c);
        } else {
            pg_span(cy + dy, cx - x, cx + x + 1, c);
        }
    }
}

/* a convex polygon: n points, x y x y .. */
static void pg_poly(const int16_t *pt, uint32_t n, uint16_t c)
{
    int32_t x0 = 32767, y0 = 32767, x1 = -32768, y1 = -32768, y;
    uint32_t i, h = 0x90u ^ c;
    for (i = 0; i < n; i++) {
        int32_t x = pt[2u * i] + pg.ox, yy = pt[2u * i + 1u] + pg.oy;
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (yy < y0) y0 = yy;
        if (yy > y1) y1 = yy;
        h = h * 31u + (uint32_t)x * 977u + (uint32_t)yy;
    }
    if (pg.mark) {
        pg_touch(x0, y0, x1 + 1, y1 + 1, h);
        return;
    }
    if (x1 < pg.x0 || x0 >= pg.x1)
        return;
    if (y0 < pg.y0) y0 = pg.y0;
    if (y1 > pg.y1 - 1) y1 = pg.y1 - 1;
    for (y = y0; y <= y1; y++) {
        int32_t xl = 32767, xr = -32768;
        for (i = 0; i < n; i++) {
            uint32_t j = i + 1u == n ? 0u : i + 1u;
            int32_t ax = pt[2u * i] + pg.ox, ay = pt[2u * i + 1u] + pg.oy, bx = pt[2u * j] + pg.ox, by = pt[2u * j + 1u] + pg.oy, x;
            if (ay == by) {
                if (y != ay)
                    continue;
                if (ax < xl) xl = ax;
                if (ax > xr) xr = ax;
                x = bx;
            } else if ((y >= ay && y <= by) || (y >= by && y <= ay)) {
                x = ax + (bx - ax) * (y - ay) / (by - ay);
            } else {
                continue;
            }
            if (x < xl) xl = x;
            if (x > xr) xr = x;
        }
        pg_span(y, xl, xr + 1, c);
    }
}
static void pg_tri(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t c)
{
    int16_t p[6];
    p[0] = (int16_t)x0; p[1] = (int16_t)y0; p[2] = (int16_t)x1; p[3] = (int16_t)y1; p[4] = (int16_t)x2; p[5] = (int16_t)y2;
    pg_poly(p, 3, c);
}
static void pg_quad(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, uint16_t c)
{
    int16_t p[8];
    p[0] = (int16_t)x0; p[1] = (int16_t)y0; p[2] = (int16_t)x1; p[3] = (int16_t)y1;
    p[4] = (int16_t)x2; p[5] = (int16_t)y2; p[6] = (int16_t)x3; p[7] = (int16_t)y3;
    pg_poly(p, 4, c);
}

/* a bone: a circle of radius r0 at (x0, y0), one of r1 at (x1, y1), and what lies between */
static void pg_capsule(int32_t x0, int32_t y0, int32_t r0, int32_t x1, int32_t y1, int32_t r1, uint16_t c)
{
    int32_t dx = x1 - x0, dy = y1 - y0, len = (int32_t)pg_isqrt((uint32_t)(dx * dx + dy * dy));
    if (r0 > 0)
        pg_circle(x0, y0, r0, c);
    if (len <= 0)
        return;
    if (r1 > 0)
        pg_circle(x1, y1, r1, c);
    {
        int32_t ax = -dy * r0 / len, ay = dx * r0 / len, bx = -dy * r1 / len, by = dx * r1 / len;
        if (r0 <= 0 && r1 <= 0) { ax = ay = bx = by = 0; }
        pg_quad(x0 + ax, y0 + ay, x1 + bx, y1 + by, x1 - bx, y1 - by, x0 - ax, y0 - ay, c);
    }
}
static void pg_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t w, uint16_t c)   /* w: its width */
{
    if (w >= 3) {
        pg_capsule(x0, y0, w / 2, x1, y1, w / 2, c);
        return;
    }
    {   /* thin: a quad one or two pixels wide along the longer axis */
        int32_t dx = x1 - x0, dy = y1 - y0, hx = 0, hy = 0;
        if ((dx < 0 ? -dx : dx) >= (dy < 0 ? -dy : dy))
            hy = w;
        else
            hx = w;
        pg_quad(x0, y0, x1, y1, x1 + hx, y1 + hy, x0 + hx, y0 + hy, c);
    }
}

/* the sky: rows y0 .. y1 - 1 across the screen, from colour a to b, dithered (a printed look, no bands) */
static void pg_vgrad(int32_t y0, int32_t y1, uint16_t a, uint16_t b)
{
    static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    int32_t y, h = y1 - y0, ya, yb;
    if (h <= 0)
        return;
    if (pg.mark) {
        pg_touch(pg.ox, y0 + pg.oy, 240 + pg.ox, y1 + pg.oy, PH4(y0, y1, a, b) ^ 0x6Au);
        return;
    }
    ya = y0 + pg.oy < pg.y0 ? pg.y0 : y0 + pg.oy;
    yb = y1 + pg.oy > pg.y1 ? pg.y1 : y1 + pg.oy;
    for (y = ya; y < yb; y++) {
        int32_t t = (y - pg.oy - y0) * 4096 / h, k, x;
        /* channels with 4 fraction bits */
        int32_t r = (int32_t)(a >> 11) * 16 + (((int32_t)(b >> 11) - (int32_t)(a >> 11)) * t >> 8);
        int32_t g = (int32_t)((a >> 5) & 63) * 16 + (((int32_t)((b >> 5) & 63) - (int32_t)((a >> 5) & 63)) * t >> 8);
        int32_t bl = (int32_t)(a & 31) * 16 + (((int32_t)(b & 31) - (int32_t)(a & 31)) * t >> 8);
        uint16_t col[4], *p = pg.px + (y - pg.y0) * pg.stride;
        for (k = 0; k < 4; k++) {
            int32_t d = BAYER[y & 3][k];
            int32_t rr = (r + d) >> 4, gg = (g + d) >> 4, bb = (bl + d) >> 4;
            col[k] = (uint16_t)((rr > 31 ? 31 : rr) << 11 | (gg > 63 ? 63 : gg) << 5 | (bb > 31 ? 31 : bb));
        }
        for (x = pg.x0; x < pg.x1; x++)
            *p++ = col[x & 3];
    }
}

/* ------------------------------------------------------------------ text */
/* 5 x 7, a column a byte (bit 0 the top row): space ! ' + , - . 0-9 : ? A-Z and a heart ('$') */
static const char PG_FONT_CH[] = " !'+,-.0123456789:?ABCDEFGHIJKLMNOPQRSTUVWXYZ$";
static const uint8_t PG_FONT[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5F, 0x00, 0x00}, {0x00, 0x00, 0x07, 0x00, 0x00},
    {0x08, 0x08, 0x3E, 0x08, 0x08}, {0x00, 0x50, 0x30, 0x00, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08},
    {0x00, 0x60, 0x60, 0x00, 0x00},
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00}, {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31}, {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03}, {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1E},
    {0x00, 0x36, 0x36, 0x00, 0x00}, {0x02, 0x01, 0x51, 0x09, 0x06},
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01}, {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43},
    {0x0C, 0x1E, 0x3C, 0x1E, 0x0C},
};
static uint32_t pg_strlen(const char *s) { uint32_t n = 0; while (s[n]) n++; return n; }
static int32_t pg_text_w(const char *s, int32_t sc) { return (int32_t)pg_strlen(s) * 6 * sc - sc; }
static void pg_glyphs(int32_t x, int32_t y, const char *s, int32_t sc, uint16_t c)
{
    for (; *s; s++, x += 6 * sc) {
        uint32_t g, col, row;
        for (g = 0; PG_FONT_CH[g] && PG_FONT_CH[g] != *s; g++)
            ;
        if (!PG_FONT_CH[g] || x + 5 * sc <= pg.x0 || x >= pg.x1)
            continue;
        for (col = 0; col < 5u; col++)
            for (row = 0; row < 7u; row++)
                if ((PG_FONT[g][col] >> row) & 1u) {
                    int32_t yy, xa = x + (int32_t)col * sc, ya = y + (int32_t)row * sc;
                    for (yy = ya; yy < ya + sc; yy++)
                        pg_span(yy, xa, xa + sc, c);
                }
    }
}
/* text at (x, y), sc pixels a dot, with an outline of colour ol (ol == c: none) */
static void pg_text(int32_t x, int32_t y, const char *s, int32_t sc, uint16_t c, uint16_t ol)
{
    int32_t w = pg_text_w(s, sc), o = sc > 2 ? 2 : 1;
    x += pg.ox; y += pg.oy;
    if (pg.mark) {
        uint32_t h = PH4(x, y, sc, c) ^ ol;
        const char *q;
        for (q = s; *q; q++)
            h = h * 33u + (uint8_t)*q;
        pg_touch(x - o, y - o, x + w + o, y + 7 * sc + o + sc, h);
        return;
    }
    if (y + 8 * sc + o <= pg.y0 || y - o >= pg.y1)
        return;
    if (ol != c) {
        pg_glyphs(x - o, y, s, sc, ol);
        pg_glyphs(x + o, y, s, sc, ol);
        pg_glyphs(x, y - o, s, sc, ol);
        pg_glyphs(x, y + o, s, sc, ol);
        pg_glyphs(x + o, y + sc, s, sc, ol);            /* a drop shadow */
    }
    pg_glyphs(x, y, s, sc, c);
}
static void pg_text_c(int32_t cx, int32_t y, const char *s, int32_t sc, uint16_t c, uint16_t ol)
{
    pg_text(cx - pg_text_w(s, sc) / 2, y, s, sc, c, ol);
}

/* ----------------------------------------------------------------- frame */
static void pg_paint(void (*draw)(void), int32_t x, int32_t y, int32_t w, int32_t h)
{
    uint16_t *px = cv_px + pg.flip * PG_HALF, *p;
    int32_t n = w * h;
    pg.px = px;                                           /* (the other half may still be going out; this one is free) */
    pg.x0 = (int16_t)x; pg.y0 = (int16_t)y; pg.x1 = (int16_t)(x + w); pg.y1 = (int16_t)(y + h);
    pg.stride = (int16_t)w;
    pg.blend = PB_SOLID;
    draw();
    for (p = px; n--; p++)
        *p = swap16(*p);
    lcd_blit((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, px);
    pg.flip ^= 1u;
    pg.sent += (uint32_t)(w * h);
}

static void pg_frame(void (*draw)(void))
{
    uint16_t *cur, *old;
    uint16_t rows[PG_NT];
    uint32_t i, ty;
    static uint8_t which;
    cur = pg_sigs[which];
    old = pg_sigs[which ^ 1u];
    which ^= 1u;
    for (i = 0; i < PG_NT * PG_NT; i++)
        cur[i] = 0;
    pg.sig = cur;
    pg.mark = 1;
    pg.blend = PB_SOLID;
    pg.x0 = pg.y0 = 0;
    pg.x1 = pg.y1 = 240;
    draw();
    pg.mark = 0;
    pg.sent = 0;
    for (ty = 0; ty < PG_NT; ty++) {
        uint32_t m = 0, tx;
        for (tx = 0; tx < PG_NT; tx++)
            if (pg.full || cur[ty * PG_NT + tx] != old[ty * PG_NT + tx])
                m |= 1u << tx;
        rows[ty] = (uint16_t)m;
    }
    pg.full = 0;
    for (ty = 0; ty < PG_NT;) {
        uint32_t m = rows[ty], nr = 1, tx, wmax = 0;
        if (!m) {
            ty++;
            continue;
        }
        for (tx = 0; tx < PG_NT;) {                       /* the widest run decides how many rows go together */
            uint32_t a = tx;
            while (tx < PG_NT && ((m >> tx) & 1u))
                tx++;
            if (tx - a > wmax)
                wmax = tx - a;
            if (tx == a)
                tx++;
        }
        while (ty + nr < PG_NT && rows[ty + nr] == m && (nr + 1u) * PG_TILE * wmax * PG_TILE <= PG_HALF)
            nr++;
        for (tx = 0; tx < PG_NT;) {
            uint32_t a = tx;
            while (tx < PG_NT && ((m >> tx) & 1u))
                tx++;
            if (tx > a)
                pg_paint(draw, (int32_t)(a * PG_TILE), (int32_t)(ty * PG_TILE), (int32_t)((tx - a) * PG_TILE), (int32_t)(nr * PG_TILE));
            else
                tx++;
        }
        ty += nr;
    }
}
