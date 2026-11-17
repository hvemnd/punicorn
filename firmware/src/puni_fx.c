/* SPDX-License-Identifier: GPL-3.0-only */
/* PUNICORN's particles and worlds.
 * Particles: one pool, a type each (dots, cloud puffs, sparks, stars, drops, rings, hearts, notes, bubbles, goo,
 * letters); pfx_update moves them (1/16 px, a step every 16 ms), pfx_draw draws them. When the pool is full the
 * one closest to its end goes.
 * Worlds (themes): a dithered sky, something in it, silhouettes on the horizon, a ground, and weather that is a
 * function of the time alone (stars, bats, embers, snow, bubbles, fireflies, hearts, rain): no state. */
enum { PT_DOT, PT_PUFF, PT_SPARK, PT_STAR, PT_DROP, PT_RING, PT_HEART, PT_NOTE, PT_BUBBLE, PT_GOO, PT_CHAR, PT_CHUNK };
typedef struct {
    int16_t x, y, vx, vy;
    uint16_t col, life, life0;
    uint8_t type, size, aux;
    int8_t grav;
} part_t;
#define PFX_N 110u
static part_t pfx[PFX_N] __attribute__((section(".pool")));
static uint32_t pu_seed = 0x1234ABCDu;
static uint32_t pu_rnd(void)
{
    pu_seed ^= pu_seed << 13;
    pu_seed ^= pu_seed >> 17;
    pu_seed ^= pu_seed << 5;
    return pu_seed;
}
static int32_t pu_rand(int32_t lo, int32_t hi) { return hi <= lo ? lo : lo + (int32_t)(pu_rnd() % (uint32_t)(hi - lo + 1)); }

static part_t *pfx_add(uint32_t type, int32_t x, int32_t y, int32_t vx, int32_t vy, uint32_t size, uint16_t col, uint32_t life, int32_t grav)
{
    uint32_t i, best = 0, bl = 0xFFFFFFFFu;
    part_t *p;
    for (i = 0; i < PFX_N; i++) {
        if (!pfx[i].life) {
            best = i;
            break;
        }
        if (pfx[i].life < bl) {
            bl = pfx[i].life;
            best = i;
        }
    }
    p = &pfx[best];
    p->type = (uint8_t)type;
    p->x = (int16_t)clamp(x * 16, -32000, 32000);
    p->y = (int16_t)clamp(y * 16, -32000, 32000);
    p->vx = (int16_t)vx;
    p->vy = (int16_t)vy;
    p->size = (uint8_t)size;
    p->col = col;
    p->life = p->life0 = (uint16_t)life;
    p->grav = (int8_t)grav;
    p->aux = 0;
    return p;
}
/* n of them from (x, y), every way, at about speed (1/16 px a step) */
static void pfx_burst(uint32_t type, int32_t x, int32_t y, uint32_t n, int32_t speed, uint32_t size, uint16_t col, uint32_t life, int32_t grav)
{
    while (n--) {
        int32_t a = (int32_t)(pu_rnd() & 1023u), s = speed / 2 + (int32_t)(pu_rnd() % (uint32_t)(speed + 1));
        pfx_add(type, x, y, pg_cos(a) * s >> 15, pg_sin(a) * s >> 15, size, col, life * (192u + (pu_rnd() & 127u)) / 256u, grav);
    }
}
static void pfx_clear(void)
{
    uint32_t i;
    for (i = 0; i < PFX_N; i++)
        pfx[i].life = 0;
}
static void pfx_update(uint32_t dt)
{
    uint32_t i;
    int32_t k = (int32_t)dt;             /* steps of 16 ms, in 1/16 */
    for (i = 0; i < PFX_N; i++) {
        part_t *p = &pfx[i];
        if (!p->life)
            continue;
        if (p->life <= dt) {
            p->life = 0;
            continue;
        }
        p->life = (uint16_t)(p->life - dt);
        p->x = (int16_t)clamp(p->x + p->vx * k / 16, -32000, 32000);
        p->y = (int16_t)clamp(p->y + p->vy * k / 16, -32000, 32000);
        p->vy = (int16_t)clamp(p->vy + p->grav * k / 16, -4000, 4000);
        if (p->type == PT_PUFF || p->type == PT_BUBBLE) {   /* air: they slow down */
            p->vx = (int16_t)(p->vx - p->vx * k / 160);
            p->vy = (int16_t)(p->vy - p->vy * k / 200);
        }
        if (p->y > 250 * 16 || p->x < -30 * 16 || p->x > 270 * 16)
            p->life = 0;
    }
}
static void pg_heart(int32_t x, int32_t y, int32_t s, uint16_t c)
{
    pg_circle(x - s / 2, y - s / 3, s / 2 + 1, c);
    pg_circle(x + s / 2, y - s / 3, s / 2 + 1, c);
    pg_tri(x - s, y - s / 6, x + s, y - s / 6, x, y + s, c);
}
static void pg_star(int32_t x, int32_t y, int32_t s, uint16_t c)
{
    int32_t t = s / 4 + 1;
    pg_quad(x - s, y, x, y - t, x + s, y, x, y + t, c);
    pg_quad(x, y - s, x + t, y, x, y + s, x - t, y, c);
}
static void pfx_draw(void)
{
    uint32_t i;
    for (i = 0; i < PFX_N; i++) {
        const part_t *p = &pfx[i];
        int32_t x, y, s, f;
        if (!p->life)
            continue;
        x = p->x / 16; y = p->y / 16;
        f = (int32_t)p->life * 256 / (int32_t)p->life0;          /* 256 new .. 0 gone */
        s = p->size;
        switch (p->type) {
        case PT_PUFF:
            s = s * (384 - f / 2) / 256;                          /* it grows as it goes */
            pg.blend = f < 70 ? PB_STIP : f < 150 && (i & 1u) ? PB_HALF : PB_SOLID;
            pg_circle(x, y, s, p->col);
            if (f >= 70)
                pg_circle(x - s / 4, y - s / 4, s / 2, pg_mix(p->col, 0xFFFF, 70));
            pg.blend = PB_SOLID;
            break;
        case PT_SPARK:
            pg_line(x, y, x - p->vx / 6, y - p->vy / 6, 1 + (s > 1), f > 100 ? p->col : pg_mix(0, p->col, 140));
            break;
        case PT_STAR:
            pg_star(x, y, s * f / 256 + 1, p->col);
            break;
        case PT_DROP:
            pg_capsule(x, y, s, x - p->vx / 8, y - p->vy / 8, 0, p->col);
            break;
        case PT_RING:
            s = s + (256 - f) * p->aux / 64;
            pg.blend = f < 90 ? PB_STIP : PB_SOLID;
            pg_ring(x, y, s, f > 128 ? 3 : 2, p->col);
            pg.blend = PB_SOLID;
            break;
        case PT_HEART:
            pg_heart(x, y, s * (f > 200 ? 456 - f : 256) / 256, p->col);
            break;
        case PT_NOTE:
            pg_ellipse(x, y, s, s * 3 / 4, p->col);
            pg_rect(x + s - 1, y - s * 3, 2, s * 3, p->col);
            pg_rect(x + s - 1, y - s * 3, s + 2, 2, p->col);
            break;
        case PT_BUBBLE:
            pg_ring(x, y, s, 1, p->col);
            pg_rect(x - s / 2, y - s / 2, 2, 2, 0xFFFF);
            break;
        case PT_GOO:
            pg_capsule(x, y, s, x, y + s + (256 - f) * s / 128, s / 2, p->col);
            break;
        case PT_CHAR: {
            char b[2];
            b[0] = (char)p->aux; b[1] = 0;
            pg_text(x, y, b, s, p->col, RGB(20, 10, 30));
            break;
        }
        case PT_CHUNK:
            pg_rect(x - s / 2, y - s / 2, s, s, p->col);
            break;
        default:
            pg_circle(x, y, s * f / 256, p->col);
            break;
        }
    }
}

/* a word that pops up and wobbles away: the onomatopoeia, the knobs' names */
static struct { const char *s; uint32_t ms; int16_t x, y; uint8_t sc; uint16_t col; } pop;
static void pop_say(const char *s, int32_t x, int32_t y, uint32_t sc, uint16_t col)
{
    pop.s = s;
    pop.ms = PU_NZ(uni_now);
    pop.x = (int16_t)x; pop.y = (int16_t)y;
    pop.sc = (uint8_t)sc;
    pop.col = col;
}
static void pop_draw(void)
{
    uint32_t t = uni_now - pop.ms, i;
    int32_t w, x;
    char b[2];
    if (!pop.s || !pop.ms)
        return;
    if (t > 900u) {
        pop.ms = 0;
        return;
    }
    w = pg_text_w(pop.s, pop.sc);
    x = clamp(pop.x - w / 2, 2, 238 - w);
    b[1] = 0;
    for (i = 0; pop.s[i]; i++) {         /* letter by letter: each jumps in after the one before */
        int32_t age = (int32_t)t - (int32_t)i * 25, dy;
        if (age < 0)
            break;
        dy = age < 160 ? -(pg_sin(age * 512 / 160) * 12 >> 15) : 0;
        if (t > 700u && ((i + t / 40u) & 1u))
            continue;
        b[0] = pop.s[i];
        pg_text(x + (int32_t)i * 6 * pop.sc, pop.y + dy - (int32_t)t / 40, b, pop.sc, pop.col, RGB(20, 10, 30));
    }
}

/* ------------------------------------------------------------------ worlds */
enum { AM_STARS = 1, AM_BATS = 2, AM_EMBERS = 4, AM_SNOW = 8, AM_BUBBLES = 16, AM_FLIES = 32, AM_HEARTS = 64, AM_RAIN = 128 };
enum { OB_NONE, OB_MOON, OB_CRESCENT, OB_BLOOD, OB_PLANET, OB_EYE, OB_SUN };
enum { SK_TOMBS, SK_TREES, SK_SPIKES, SK_CASTLE, SK_DRIPS, SK_CANDY, SK_CRATERS, SK_PINES };
typedef struct {
    const char *name;
    uint16_t sky0, sky1, gnd0, gnd1, sil, acc;
    uint8_t obj, silk, amb;
} theme_t;
#define PU_NTHEME 8u
static const theme_t THEMES[PU_NTHEME] = {
    {"GRAVEYARD", RGB(22, 12, 58), RGB(100, 45, 120), RGB(34, 52, 46), RGB(20, 30, 32), RGB(14, 8, 30), RGB(215, 240, 160), OB_MOON, SK_TOMBS, AM_STARS | AM_BATS},
    {"SLIME SEWER", RGB(8, 36, 28), RGB(50, 120, 50), RGB(56, 66, 40), RGB(32, 42, 24), RGB(8, 22, 14), RGB(170, 255, 60), OB_EYE, SK_DRIPS, AM_BUBBLES},
    {"SPOOKY WOODS", RGB(12, 18, 44), RGB(44, 78, 96), RGB(22, 38, 34), RGB(12, 22, 22), RGB(6, 10, 18), RGB(255, 230, 120), OB_CRESCENT, SK_TREES, AM_FLIES | AM_STARS},
    {"CANDY GOTH", RGB(255, 165, 205), RGB(185, 150, 240), RGB(120, 80, 165), RGB(88, 56, 128), RGB(66, 28, 88), RGB(255, 245, 170), OB_SUN, SK_CANDY, AM_HEARTS},
    {"HELLFIRE", RGB(50, 4, 6), RGB(225, 70, 12), RGB(46, 12, 12), RGB(22, 5, 6), RGB(12, 2, 4), RGB(255, 190, 40), OB_BLOOD, SK_SPIKES, AM_EMBERS},
    {"DEEP SPACE", RGB(4, 4, 18), RGB(34, 12, 66), RGB(78, 68, 98), RGB(52, 44, 70), RGB(30, 24, 46), RGB(120, 220, 255), OB_PLANET, SK_CRATERS, AM_STARS},
    {"FROST CRYPT", RGB(120, 170, 225), RGB(215, 228, 250), RGB(236, 242, 252), RGB(190, 206, 232), RGB(84, 104, 150), RGB(255, 255, 255), OB_MOON, SK_PINES, AM_SNOW},
    {"BLOOD CASTLE", RGB(18, 4, 24), RGB(130, 22, 52), RGB(28, 16, 34), RGB(16, 8, 20), RGB(8, 2, 12), RGB(255, 80, 60), OB_BLOOD, SK_CASTLE, AM_BATS | AM_RAIN},
};
static const char *const VIBE_NAME[9] = {"CALM", "STARS", "BATS", "EMBERS", "SNOW", "BUBBLES", "FIREFLIES", "HEARTS", "RAIN"};
#define PU_GY 188                         /* the horizon */
static const theme_t *th;                 /* the world now */
static uint8_t th_amb;                    /* its weather now (the theme's, or the vibe's) */

static void bg_object(void)
{
    int32_t x = 186, y = 44, i;
    switch (th->obj) {
    case OB_MOON:
    case OB_BLOOD: {
        uint16_t c = th->obj == OB_BLOOD ? RGB(200, 30, 30) : RGB(240, 240, 215), d = pg_mix(c, th->sky0, 90);
        pg.blend = PB_ADD;
        pg_circle(x, y, 38, pg_mix(0, c, 26));
        pg.blend = PB_SOLID;
        pg_circle(x, y, 28, c);
        pg_circle(x - 9, y - 6, 6, d);
        pg_circle(x + 8, y + 9, 4, d);
        pg_circle(x + 6, y - 13, 3, d);
        break;
    }
    case OB_CRESCENT:
        pg_circle(x, y, 24, RGB(250, 235, 160));
        pg_circle(x + 10, y - 6, 21, pg_mix(th->sky0, th->sky1, 50));
        break;
    case OB_PLANET:
        pg_circle(x, y, 22, RGB(230, 140, 70));
        pg_rect(x - 20, y - 6, 40, 4, RGB(250, 190, 110));
        pg_rect(x - 17, y + 8, 34, 3, RGB(190, 100, 60));
        pg_line(x - 38, y + 8, x + 38, y - 8, 3, RGB(200, 220, 255));
        break;
    case OB_EYE: {                        /* something is watching from the dark */
        int32_t px = pg_sin((int32_t)uni_now / 3) * 7 >> 15, bl = (uni_now % 4100u) < 140u;
        pg_ellipse(x, y, 30, bl ? 2 : 17, RGB(210, 255, 150));
        if (!bl) {
            pg_circle(x + px, y, 12, RGB(40, 120, 30));
            pg_ellipse(x + px, y, 4, 11, RGB(8, 20, 8));
        }
        break;
    }
    case OB_SUN:
        for (i = 0; i < 8; i++) {
            int32_t a = i * 128 + (int32_t)uni_now / 16;
            pg_line(x + (pg_cos(a) * 30 >> 15), y + (pg_sin(a) * 30 >> 15), x + (pg_cos(a) * 40 >> 15), y + (pg_sin(a) * 40 >> 15), 4, th->acc);
        }
        pg_circle(x, y, 25, th->acc);
        pg_circle(x - 8, y - 4, 4, RGB(66, 28, 88));
        pg_circle(x + 8, y - 4, 4, RGB(66, 28, 88));
        pg_tri(x - 3, y + 6, x + 3, y + 6, x, y + 1, RGB(66, 28, 88));
        break;
    default:
        break;
    }
}

static void bg_silhouettes(void)
{
    uint32_t i;
    uint16_t c = th->sil, c2 = pg_mix(th->sil, th->sky1, 80);
    for (i = 0; i < 7u; i++) {
        uint32_t h = pg_hash(i * 7919u + th->silk * 131u);
        int32_t x = (int32_t)i * 38 - 8 + (int32_t)(h & 15u), s = 14 + (int32_t)((h >> 4) & 15u), y = PU_GY;
        switch (th->silk) {
        case SK_TOMBS:
            if (i & 1u) {                 /* a cross */
                pg_rect(x + 6, y - s - 16, 5, s + 16, c);
                pg_rect(x - 1, y - s - 8, 19, 5, c);
            } else {
                pg_rect(x, y - s, 20, s, c);
                pg_circle(x + 10, y - s, 10, c);
                pg_rect(x + 5, y - s + 2, 10, 2, c2);
                pg_rect(x + 7, y - s + 7, 6, 2, c2);
            }
            break;
        case SK_TREES:
            pg_quad(x + 4, y, x + 8, y - s * 3, x + 12, y - s * 3, x + 16, y, c);
            pg_line(x + 10, y - s * 2, x + 28, y - s * 3 - 4, 3, c);
            pg_line(x + 9, y - s * 2 - 8, x - 12, y - s * 3 - 10, 3, c);
            pg_line(x + 22, y - s * 3, x + 26, y - s * 4, 2, c);
            if ((h >> 9) & 1u) {          /* eyes in the dark */
                pg_rect(x + 7, y - s - 6, 2, 2, th->acc);
                pg_rect(x + 12, y - s - 6, 2, 2, th->acc);
            }
            break;
        case SK_SPIKES:
            pg_tri(x - 6, y, x + 8, y - s * 3 - 10, x + 24, y, c);
            pg_tri(x + 14, y, x + 22, y - s - 20, x + 34, y, c2);
            break;
        case SK_CASTLE:
            pg_rect(x, y - s * 2, 30, s * 2, c);
            pg_rect(x + 4, y - s * 3 - 6, 12, s + 8, c);
            pg_tri(x + 1, y - s * 3 - 6, x + 10, y - s * 3 - 24, x + 19, y - s * 3 - 6, c);
            pg_rect(x - 2, y - s * 2 - 5, 6, 5, c);
            pg_rect(x + 24, y - s * 2 - 5, 6, 5, c);
            pg_rect(x + 8, y - s * 3 + 2, 4, 6, (h >> 9) & 1u ? RGB(255, 200, 60) : c2);
            break;
        case SK_DRIPS: {                  /* slime from the ceiling, and pipes */
            int32_t l = s * 2 + (pg_sin((int32_t)uni_now / 2 + (int32_t)i * 200) * 6 >> 15);
            pg_rect(x, 0, 14, l, RGB(60, 170, 40));
            pg_circle(x + 7, l, 7, RGB(60, 170, 40));
            pg_rect(x + 3, 0, 3, l - 4, RGB(130, 230, 80));
            pg_rect(x + 18, y - s, 16, s, c);
            pg_rect(x + 15, y - s - 4, 22, 6, c);
            break;
        }
        case SK_CANDY:
            pg_circle(x + 10, y + 6, s + 12, c2);
            pg_rect(x + 9, y - s * 2 - 8, 3, s * 2 + 8, c);
            pg_circle(x + 10, y - s * 2 - 12, 9, i & 1u ? RGB(255, 110, 170) : RGB(130, 230, 220));
            pg_circle(x + 10, y - s * 2 - 12, 4, RGB(255, 245, 250));
            break;
        case SK_CRATERS:
            pg_ellipse(x + 12, y + 16 + (int32_t)((h >> 10) & 15u), s, s / 3, th->gnd1);
            if (i & 1u)
                pg_tri(x - 10, y, x + 6, y - s - 6, x + 26, y, c);
            break;
        default:                          /* pines */
            pg_rect(x + 9, y - 8, 4, 8, c);
            pg_tri(x - 4, y - 6, x + 11, y - s * 2 - 4, x + 26, y - 6, c);
            pg_tri(x, y - s - 6, x + 11, y - s * 3 - 4, x + 22, y - s - 6, c);
            break;
        }
    }
}

static void bg_weather(void)
{
    uint32_t i, t = uni_now;
    if (th_amb & AM_STARS)
        for (i = 0; i < 18u; i++) {
            uint32_t h = pg_hash(i * 2654435761u + 17u), ph = (t / 180u + (h >> 20)) & 15u;
            int32_t x = (int32_t)(h % 236u) + 2, y = (int32_t)((h >> 8) % 120u) + 4;
            if (ph < 2u)
                pg_star(x, y, 3, 0xFFFF);
            else
                pg_rect(x, y, ph < 9u ? 2 : 1, ph < 9u ? 2 : 1, ph < 9u ? 0xFFFF : RGB(160, 160, 190));
        }
    if (th_amb & AM_BATS)
        for (i = 0; i < 3u; i++) {
            int32_t a = (int32_t)(t / 6u) + (int32_t)i * 340, x = 120 + (pg_sin(a + (int32_t)i * 90) * 130 >> 15);
            int32_t y = 50 + (int32_t)i * 22 + (pg_sin(a * 2) * 18 >> 15), w = pg_sin((int32_t)t * 4 + (int32_t)i * 300) * 6 >> 15;
            pg_tri(x, y, x - 10, y - 3 + w, x - 4, y + 4, RGB(10, 5, 20));
            pg_tri(x, y, x + 10, y - 3 + w, x + 4, y + 4, RGB(10, 5, 20));
            pg_circle(x, y, 2, RGB(10, 5, 20));
        }
    if (th_amb & (AM_EMBERS | AM_SNOW | AM_BUBBLES | AM_HEARTS | AM_RAIN))
        for (i = 0; i < 12u; i++) {
            uint32_t h = pg_hash(i * 977u + 5u), sp = 20u + (h & 31u), up = th_amb & (AM_EMBERS | AM_BUBBLES | AM_HEARTS);
            int32_t y = (int32_t)(((t * (th_amb & AM_RAIN ? sp * 6u : sp) / 1000u) + (h >> 8)) % 250u);
            int32_t x = (int32_t)((h >> 16) % 240u) + (pg_sin((int32_t)t / 2 + (int32_t)i * 150) * 8 >> 15);
            if (up)
                y = 244 - y;
            if (th_amb & AM_EMBERS)
                pg_rect(x, y, 2, 2, (i + t / 120u) & 1u ? RGB(255, 200, 50) : RGB(255, 90, 20));
            else if (th_amb & AM_SNOW)
                pg_circle(x, y, 1 + (int32_t)(i & 1u), 0xFFFF);
            else if (th_amb & AM_BUBBLES)
                pg_ring(x, y, 3 + (int32_t)(i % 3u), 1, RGB(170, 255, 90));
            else if (th_amb & AM_HEARTS)
                pg_heart(x, y, 4, i & 1u ? RGB(255, 90, 150) : RGB(40, 10, 50));
            else
                pg_line(x, y, x - 2, y + 9, 1, RGB(150, 170, 220));
        }
    if (th_amb & AM_FLIES)
        for (i = 0; i < 7u; i++) {
            uint32_t h = pg_hash(i * 311u + 3u);
            int32_t x = (int32_t)(h % 220u) + 10 + (pg_sin((int32_t)t / 2 + (int32_t)(h >> 9)) * 14 >> 15);
            int32_t y = 90 + (int32_t)((h >> 8) % 90u) + (pg_sin((int32_t)t / 3 + (int32_t)(h >> 14)) * 10 >> 15);
            if (((t / 300u + i) % 5u) < 3u) {
                pg.blend = PB_ADD;
                pg_circle(x, y, 4, RGB(50, 60, 0));
                pg.blend = PB_SOLID;
                pg_rect(x, y, 2, 2, RGB(240, 255, 120));
            }
        }
}

static void bg_draw(void)
{
    uint32_t i;
    pg_vgrad(0, PU_GY, th->sky0, th->sky1);
    bg_object();
    if (th_amb & AM_STARS)
        ;
    bg_silhouettes();
    pg_rect(0, PU_GY, 240, 240 - PU_GY, th->gnd0);
    pg_rect(0, PU_GY, 240, 3, pg_mix(th->gnd0, 0xFFFF, 40));
    pg_rect(0, 224, 240, 16, th->gnd1);
    for (i = 0; i < 10u; i++) {          /* marks on the ground */
        uint32_t h = pg_hash(i * 4441u + th->silk);
        pg_rect((int32_t)(h % 230u), PU_GY + 8 + (int32_t)((h >> 8) % 30u), 6 + (int32_t)((h >> 16) & 7u), 2, th->gnd1);
    }
    bg_weather();
}
