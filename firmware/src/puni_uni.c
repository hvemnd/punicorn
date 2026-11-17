/* SPDX-License-Identifier: GPL-3.0-only */
/* PUNICORN's unicorn: a rig of bones (capsules) drawn with an ink outline, posed by springs.
 * Local space: pixels at scale 1, x forward, y down, the origin on the ground under the body; angles 1024 a turn,
 * 0 forward, 256 down. A spring holds a value, a speed and a target (uni_update steps them every 4 ms), so every
 * pose overshoots and settles: the squash and the bounce come from there. The legs are two-bone IK from the hips
 * to the feet, so a body that drops bends its knees. uni_solve turns the springs into joints on the screen;
 * uni_draw draws them: tail, far legs, then body, near legs, neck and head as one outlined shape, the face, the
 * horn, the hat and the clothes. */
typedef struct { int16_t x, v, to; } spr_t;
enum { US_BOB, US_LEAN, US_NECK, US_HEAD, US_MOUTH, US_EYE, US_BROW, US_TAIL, US_EAR, US_PUFF, US_PX, US_PY,
       US_F0X, US_F0Y, US_F1X, US_F1Y, US_F2X, US_F2Y, US_F3X, US_F3Y, US_N };
enum { UA_IDLE, UA_HOP, UA_FART, UA_PEE, UA_PICK, UA_FLICK, UA_BURP, UA_PUKE, UA_SING, UA_WOW, UA_SLEEP, UA_DANCE, UA_ZAP };
enum { UC_NONE, UC_CAPE, UC_SWEATER, UC_TUTU, UC_BONES, UC_WINGS, UC_MUMMY, UC_SPIKES, UC_COUNT };
enum { UH_SPIRAL, UH_GOLD, UH_DEVIL, UH_STUMP, UH_CRYSTAL, UH_CANDLE, UH_RAINBOW, UH_ANTLER, UH_COUNT };
enum { UT_NONE, UT_CROWN, UT_WITCH, UT_TOP, UT_BOW, UT_HALO, UT_PARTY, UT_SKULL, UT_PROP, UT_COUNT };
#define UNI_NCOL 12u

typedef struct { int16_t x, y; } pt_t;
typedef struct {
    uint8_t col, cloth, horn, hat;
    int16_t x, y, sc;                    /* the ground point on the screen; the scale, Q8 */
    int8_t flip;                         /* 1: facing right, -1: left */
    uint8_t act;
    uint32_t act_ms, blink_ms;
    int16_t amp;                         /* 0 .. 256: how hard the action is going (the sound's envelope) */
    int16_t look_x, look_y;              /* where the pupils go, -32 .. 32 */
    spr_t s[US_N];
    /* joints on the screen (uni_solve) */
    pt_t rump, chest, hip[4], knee[4], foot[4], neck0, head, skull, muz, jawp, jawt, eye, nostril, ear0, ear1,
         horn0, horn1, hat0, tail[6], butt, pee, mouth, cheek, wither;
    int16_t ha;                          /* the head's angle */
} uni_t;

typedef struct { uint16_t body, shade, mane, mane2, hoof, ink, eye; } ucol_t;
static const ucol_t UNI_COL[UNI_NCOL] = {
    {RGB(235, 230, 215), RGB(165, 155, 165), RGB(150, 60, 200), RGB(70, 200, 190), RGB(60, 50, 70), RGB(22, 12, 30), RGB(60, 160, 220)},   /* BONE */
    {RGB(255, 175, 205), RGB(205, 115, 170), RGB(255, 240, 150), RGB(150, 220, 255), RGB(120, 60, 110), RGB(40, 10, 40), RGB(120, 60, 200)},  /* PINK */
    {RGB(200, 170, 255), RGB(140, 110, 210), RGB(255, 150, 200), RGB(255, 255, 255), RGB(80, 50, 130), RGB(30, 12, 50), RGB(255, 90, 160)},  /* LILAC */
    {RGB(160, 240, 200), RGB(90, 180, 150), RGB(255, 120, 160), RGB(255, 220, 120), RGB(40, 90, 80), RGB(12, 35, 35), RGB(240, 80, 120)},    /* MINT */
    {RGB(205, 40, 30), RGB(120, 15, 20), RGB(35, 20, 25), RGB(245, 150, 30), RGB(40, 15, 15), RGB(20, 5, 8), RGB(255, 220, 40)},             /* HELL */
    {RGB(120, 220, 60), RGB(60, 150, 40), RGB(160, 60, 200), RGB(240, 240, 80), RGB(40, 70, 20), RGB(15, 30, 10), RGB(230, 60, 200)},        /* SLIME */
    {RGB(55, 65, 140), RGB(28, 32, 85), RGB(120, 220, 255), RGB(240, 240, 255), RGB(15, 15, 45), RGB(8, 6, 25), RGB(255, 230, 90)},          /* MIDNIGHT */
    {RGB(190, 240, 250), RGB(120, 180, 210), RGB(255, 255, 255), RGB(150, 255, 230), RGB(80, 130, 160), RGB(30, 50, 80), RGB(40, 200, 180)}, /* GHOST */
    {RGB(38, 30, 52), RGB(20, 14, 30), RGB(235, 40, 120), RGB(120, 40, 220), RGB(90, 60, 120), RGB(150, 90, 220), RGB(255, 60, 60)},         /* VOID */
    {RGB(245, 200, 60), RGB(190, 130, 30), RGB(255, 255, 230), RGB(255, 120, 40), RGB(110, 70, 10), RGB(60, 30, 5), RGB(40, 120, 60)},       /* GOLD */
    {RGB(150, 172, 120), RGB(95, 115, 85), RGB(95, 40, 55), RGB(200, 200, 90), RGB(50, 55, 40), RGB(22, 28, 20), RGB(230, 230, 60)},         /* ZOMBIE */
    {RGB(255, 255, 255), RGB(180, 180, 200), RGB(255, 255, 255), RGB(255, 230, 250), RGB(90, 90, 110), RGB(25, 15, 35), RGB(255, 80, 180)},  /* RAINBOW (the body's hue turns) */
};
static const char *const UNI_COL_NAME[UNI_NCOL] = {"BONE", "PINK", "LILAC", "MINT", "HELL RED", "SLIME", "MIDNIGHT", "GHOST",
                                                   "VOID", "GOLD", "ZOMBIE", "RAINBOW"};
static const char *const UNI_CLOTH_NAME[UC_COUNT] = {"NO CLOTHES", "CAPE", "SWEATER", "TUTU", "SKELETON", "BAT WINGS", "MUMMY", "SPIKES"};
static const char *const UNI_HORN_NAME[UH_COUNT] = {"SPIRAL", "GOLD", "DEVIL", "STUMP", "CRYSTAL", "CANDLE", "RAINBOW", "ANTLER"};
static const char *const UNI_HAT_NAME[UT_COUNT] = {"NO HAT", "CROWN", "WITCH", "TOP HAT", "BOW", "HALO", "PARTY", "SKULL", "PROPELLER"};

static uint32_t uni_now;                 /* the frame's time (the rig's wobbles) */

/* ---------------------------------------------------------------- springs */
static void spr_step(spr_t *s, int32_t k, int32_t d, uint32_t n)
{
    int32_t x = s->x, v = s->v;
    while (n--) {
        v += ((s->to - x) * k) / 256;
        v -= (v * d) / 256;
        x += v;
    }
    s->x = (int16_t)clamp(x, -30000, 30000);
    s->v = (int16_t)clamp(v, -8000, 8000);
}
#define A16(a) ((int16_t)((a) * 16))     /* a spring's unit: 1/16 px, 1/16 angle step */

static void uni_init(uni_t *u, int32_t x, int32_t y, int32_t sc, int32_t flip)
{
    uint32_t i;
    for (i = 0; i < US_N; i++)
        u->s[i].x = u->s[i].v = u->s[i].to = 0;
    u->s[US_EYE].x = u->s[US_EYE].to = 256;
    u->x = (int16_t)x;
    u->y = (int16_t)y;
    u->sc = (int16_t)sc;
    u->flip = (int8_t)flip;
    u->act = UA_IDLE;
    u->amp = 0;
}
static void uni_act(uni_t *u, uint32_t act)
{
    u->act = (uint8_t)act;
    u->act_ms = uni_now;
}

/* the pose: the springs' targets from the action, its age and u->amp; then the springs move */
static void uni_update(uni_t *u, uint32_t now, uint32_t dt)
{
    int32_t t = (int32_t)(now - u->act_ms), a = u->amp, i, T[US_N];
    uint32_t n = dt / 4u, r = pg_hash(now / 60u + (uint32_t)u->x);
    for (i = 0; i < US_N; i++)
        T[i] = 0;
    /* alive: breath, a tail that swishes, ears that flick, a blink */
    T[US_BOB] = pg_sin((int32_t)now * 2 / 3) * 20 / 32768;
    T[US_TAIL] = A16(pg_sin((int32_t)now) * 14 / 32768);
    T[US_EAR] = (pg_hash(now / 900u) & 7u) == 0u ? A16(40) : 0;
    T[US_EYE] = 256;
    T[US_PX] = u->look_x;
    T[US_PY] = u->look_y;
    if (now - u->blink_ms > 2600u + (pg_hash(u->blink_ms) & 2047u))
        u->blink_ms = now;
    if (now - u->blink_ms < 110u)
        T[US_EYE] = 20;
    switch (u->act) {
    case UA_HOP:
        if (t < 70) {
            T[US_BOB] = A16(7);
        } else if (t < 300) {
            int32_t h = pg_sin((t - 70) * 512 / 230) * 26 / 32768;
            T[US_BOB] = -A16(h);
            T[US_F0Y] = T[US_F1Y] = T[US_F2Y] = T[US_F3Y] = -A16(h + 6);
            T[US_F2X] = T[US_F3X] = A16(8);
            T[US_F0X] = T[US_F1X] = -A16(8);
            T[US_TAIL] = -A16(120);
            T[US_EAR] = -A16(60);
        } else if (t < 420) {
            T[US_BOB] = A16(6);
        } else {
            u->act = UA_IDLE;
        }
        T[US_MOUTH] = t < 300 ? 110 : 30;
        break;
    case UA_FART:                        /* the back end up, the tail out of the way, the face says it all */
        T[US_LEAN] = A16(a * 34 / 256);
        T[US_BOB] = A16(a * 4 / 256) + (a > 20 ? (int32_t)(r & 63u) - 32 : 0) * a / 256;
        T[US_TAIL] = -A16(40 + a * 230 / 256);
        T[US_EYE] = 256 + a * 150 / 256;
        T[US_BROW] = A16(a * 50 / 256);
        T[US_MOUTH] = 40 + a * 60 / 256;
        T[US_PUFF] = A16(a * 5 / 256);
        T[US_NECK] = -A16(a * 40 / 256);
        T[US_HEAD] = -A16(a * 30 / 256);
        T[US_F0X] = T[US_F1X] = -A16(a * 7 / 256);
        T[US_F2X] = T[US_F3X] = A16(a * 5 / 256);
        T[US_PX] = -a / 10;
        if (a < 6 && t > 300)
            u->act = UA_IDLE;
        break;
    case UA_PEE:                         /* a hind leg up, bliss */
        T[US_F1X] = -A16(6 + a * 6 / 256);
        T[US_F1Y] = -A16(a * 26 / 256);
        T[US_LEAN] = -A16(a * 10 / 256);
        T[US_EYE] = 256 - a * 150 / 256;
        T[US_MOUTH] = a * 50 / 256;
        T[US_BROW] = A16(a * 30 / 256);
        T[US_TAIL] = -A16(a * 120 / 256);
        T[US_HEAD] = -A16(a * 20 / 256);
        T[US_BOB] += a > 30 ? (pg_sin((int32_t)now * 12) * a / 256) * 12 / 32768 : 0;
        break;
    case UA_PICK:                        /* digging: the hoof in the nose, turning; the eyes roll with it */
    case UA_FLICK: {
        int32_t dig = u->act == UA_PICK;
        if (dig) {
            T[US_F3X] = A16(30) + pg_cos((int32_t)now * 5) * 40 / 32768;
            T[US_F3Y] = -A16(62) + pg_sin((int32_t)now * 5) * 50 / 32768;
            T[US_NECK] = A16(90);
            T[US_HEAD] = A16(40) + pg_sin((int32_t)now * 5) * 200 / 32768;
            T[US_EYE] = 150;
            T[US_PX] = 24;
            T[US_PY] = 8 + pg_sin((int32_t)now * 5) * 16 / 32768;
            T[US_BROW] = -A16(30);
            T[US_MOUTH] = 60;
            T[US_LEAN] = A16(6);
        } else if (t < 60) {             /* wind up */
            T[US_F3X] = A16(8);
            T[US_F3Y] = -A16(50);
            T[US_LEAN] = -A16(20);
            T[US_NECK] = -A16(40);
            T[US_EYE] = 330;
        } else if (t < 260) {            /* and let go */
            T[US_F3X] = A16(46);
            T[US_F3Y] = -A16(58);
            T[US_LEAN] = A16(26);
            T[US_NECK] = A16(40);
            T[US_HEAD] = A16(30);
            T[US_EYE] = 300;
            T[US_BROW] = -A16(50);
            T[US_MOUTH] = 150;
            T[US_TAIL] = -A16(150);
            T[US_F0X] = T[US_F1X] = -A16(6);
        } else {
            u->act = UA_IDLE;
        }
        break;
    }
    case UA_BURP:                        /* the head goes back, the mouth as wide as the burp is loud */
        T[US_NECK] = -A16(a * 50 / 256);
        T[US_HEAD] = -A16(a * 70 / 256);
        T[US_MOUTH] = a * 300 / 256;
        T[US_EYE] = a > 150 ? 30 : 256 + a;
        T[US_BROW] = A16(a * 60 / 256);
        T[US_EAR] = -A16(a * 90 / 256);
        T[US_PUFF] = a < 40 ? A16(6) : 0;
        T[US_BOB] += (int32_t)((r >> 8) & 63u) * a / 256 - a / 8;
        T[US_LEAN] = -A16(a * 14 / 256);
        break;
    case UA_PUKE:                        /* heaving forward */
        T[US_NECK] = A16(a * 60 / 256);
        T[US_HEAD] = A16(a * 50 / 256);
        T[US_MOUTH] = a * 330 / 256;
        T[US_EYE] = a > 60 ? 16 : 256;
        T[US_BROW] = A16(a * 70 / 256);
        T[US_LEAN] = A16(a * 16 / 256) + (pg_sin((int32_t)now * 9) * a / 256) * 60 / 32768;
        T[US_TAIL] = -A16(a * 200 / 256);
        T[US_EAR] = -A16(a * 70 / 256);
        T[US_F2X] = T[US_F3X] = A16(a * 6 / 256);
        break;
    case UA_SING:
        T[US_MOUTH] = a * 220 / 256;
        T[US_HEAD] = -A16(a * 30 / 256) + pg_sin((int32_t)now * 3) * 100 / 32768;
        T[US_NECK] = pg_sin((int32_t)now * 2) * 160 / 32768;
        T[US_EYE] = a > 100 ? 40 : 256;
        T[US_BROW] = A16(a * 40 / 256);
        break;
    case UA_WOW:
        T[US_EYE] = 400;
        T[US_BROW] = A16(70);
        T[US_MOUTH] = 180;
        T[US_BOB] = t < 120 ? -A16(10) : 0;
        T[US_EAR] = -A16(80);
        T[US_TAIL] = -A16(180);
        if (t > 500)
            u->act = UA_IDLE;
        break;
    case UA_SLEEP:
        T[US_NECK] = A16(110);
        T[US_HEAD] = A16(90);
        T[US_EYE] = 8;
        T[US_BOB] = A16(5) + pg_sin((int32_t)now / 3) * 50 / 32768;
        T[US_TAIL] = A16(60);
        T[US_EAR] = A16(60);
        T[US_MOUTH] = 30 + pg_sin((int32_t)now / 3) * 30 / 32768;
        break;
    case UA_DANCE: {                     /* to the beat: a = the beat's phase, 0 .. 256 */
        int32_t s = pg_sin(a * 4), c2 = pg_sin(a * 2);
        T[US_BOB] = -(s > 0 ? s : -s) * 110 / 32768;
        T[US_LEAN] = c2 * A16(10) / 32768;
        T[US_HEAD] = s * A16(26) / 32768;
        T[US_NECK] = c2 * A16(18) / 32768;
        T[US_F3Y] = c2 > 0 ? -c2 * A16(14) / 32768 : 0;
        T[US_F1Y] = c2 < 0 ? c2 * A16(14) / 32768 : 0;
        T[US_TAIL] = s * A16(90) / 32768;
        T[US_MOUTH] = 90;
        break;
    }
    case UA_ZAP:                         /* the horn fires: the head down and forward, braced */
        T[US_NECK] = A16(50);
        T[US_HEAD] = A16(70);
        T[US_BROW] = -A16(60);
        T[US_EYE] = 200;
        T[US_F0X] = T[US_F1X] = -A16(8);
        T[US_LEAN] = A16(8) + (a > 30 ? (int32_t)(r & 31u) - 16 : 0);
        T[US_TAIL] = -A16(160);
        T[US_MOUTH] = 60;
        break;
    default:
        break;
    }
    if (n < 1u) n = 1u;
    if (n > 24u) n = 24u;
    for (i = 0; i < US_N; i++) {
        u->s[i].to = (int16_t)T[i];
        spr_step(&u->s[i], i == US_EYE || i == US_MOUTH ? 22 : i >= US_F0X ? 16 : 10, i == US_EYE ? 90 : 44, n);
    }
}

/* ----------------------------------------------------------------- joints */
static inline void uni_out(const uni_t *u, int32_t lx, int32_t ly, pt_t *o)     /* local 1/16 px -> the screen */
{
    o->x = (int16_t)(u->x + u->flip * ((lx * u->sc) >> 12));
    o->y = (int16_t)(u->y + ((ly * u->sc) >> 12));
}
static inline void rot16(int32_t x, int32_t y, int32_t a, int32_t *ox, int32_t *oy)
{
    int32_t c = pg_cos(a), s = pg_sin(a);
    *ox = (x * c - y * s) >> 15;
    *oy = (x * s + y * c) >> 15;
}
static inline int32_t uni_r(const uni_t *u, int32_t r) { r = (r * u->sc) >> 8; return r < 1 ? 1 : r; }

static void uni_solve(uni_t *u)
{
    static const int16_t HIPX[4] = {-19, -25, 25, 19}, FOOTX[4] = {-18, -27, 30, 21};
    int32_t lean = u->s[US_LEAN].x / 16, cx = 0, cy = -52 * 16 + u->s[US_BOB].x, x, y, i;
    int32_t na, ha, hx, hy, ja;
#define BODY(qx, qy, out) do { rot16((qx) * 16, (qy) * 16, lean, &x, &y); x += cx; y += cy; uni_out(u, x, y, out); } while (0)
    BODY(-22, 0, &u->rump);
    BODY(18, -3, &u->chest);
    BODY(-45, 2, &u->butt);
    BODY(-12, 22, &u->pee);
    BODY(16, -22, &u->wither);
    /* the neck, from the chest up and forward; the head on its end */
    BODY(25, -13, &u->neck0);
    rot16(25 * 16, -13 * 16, lean, &x, &y);
    na = -180 + lean + u->s[US_NECK].x / 16;
    hx = cx + x + (pg_cos(na) * 31 * 16 >> 15);
    hy = cy + y + (pg_sin(na) * 31 * 16 >> 15);
    uni_out(u, hx, hy, &u->head);
    ha = lean / 2 + u->s[US_HEAD].x / 16;
    u->ha = (int16_t)ha;
#define HEAD(qx, qy, out) do { rot16((qx) * 16, (qy) * 16, ha, &x, &y); uni_out(u, hx + x, hy + y, out); } while (0)
    HEAD(2, -3, &u->skull);
    HEAD(23, 5, &u->muz);
    HEAD(6, 9, &u->jawp);
    ja = u->s[US_MOUTH].x * 150 / 256;   /* the jaw swings open */
    if (ja < 0) ja = 0;
    {
        int32_t jx, jy;
        rot16(17 * 16, 4 * 16, ja, &jx, &jy);
        rot16(6 * 16 + jx, 9 * 16 + jy, ha, &x, &y);
        uni_out(u, hx + x, hy + y, &u->jawt);
    }
    HEAD(8, -6, &u->eye);
    HEAD(30, 2, &u->nostril);
    HEAD(-7, -13, &u->ear0);
    rot16(-6 * 16, -19 * 16, u->s[US_EAR].x / 16, &x, &y);
    rot16(-7 * 16 + x, -13 * 16 + y, ha, &x, &y);
    uni_out(u, hx + x, hy + y, &u->ear1);
    HEAD(9, -15, &u->horn0);
    HEAD(19, -43, &u->horn1);
    HEAD(-1, -16, &u->hat0);
    HEAD(27, 12, &u->mouth);
    HEAD(8, 7, &u->cheek);
    {
        int32_t reach = u->act == UA_PICK || (u->act == UA_FLICK && uni_now - u->act_ms < 60u);
        for (i = 0; i < 4; i++) {            /* 0 1: the hind legs (far, near), 2 3: the front legs (far, near) */
            int32_t hxl, hyl, fx, fy, dx, dy, d, l = (i == 3 && reach) ? 33 * 16 : 23 * 16, maxd = 2 * l - 8, aa, hh, side = i < 2 || (i == 3 && reach) ? 1 : -1, kx, ky;
            rot16(HIPX[i] * 16, 9 * 16, lean, &hxl, &hyl);
            hxl += cx; hyl += cy;
            fx = FOOTX[i] * 16 + u->s[US_F0X + 2 * i].x;
            fy = u->s[US_F0Y + 2 * i].x;
            if (fy > 0) fy = 0;
            if (i == 3 && reach) {           /* picking: the hoof up the nostril, turning */
                rot16(28 * 16, 4 * 16, ha, &x, &y);
                fx = hx + x + (pg_cos((int32_t)uni_now * 5) * 3 * 16 >> 15);
                fy = hy + y + (pg_sin((int32_t)uni_now * 5) * 3 * 16 >> 15);
            }
            dx = fx - hxl; dy = fy - hyl;
            d = (int32_t)pg_isqrt((uint32_t)(dx * dx + dy * dy));
            if (d < 16) d = 16;
            if (d > maxd) {
                fx = hxl + dx * maxd / d;
                fy = hyl + dy * maxd / d;
                dx = fx - hxl; dy = fy - hyl;
                d = maxd;
            }
            aa = d / 2;                      /* (both bones the same length) */
            hh = (int32_t)pg_isqrt((uint32_t)(l * l - aa * aa));
            kx = hxl + dx / 2 + side * (-dy) * hh / d;
            ky = hyl + dy / 2 + side * dx * hh / d;
            uni_out(u, hxl, hyl, &u->hip[i]);
            uni_out(u, kx, ky, &u->knee[i]);
            uni_out(u, fx, fy, &u->foot[i]);
        }
    }
    /* the tail: a chain from the rump, each link turning a little more, wagging */
    {
        int32_t tx, ty, ang = 600 + lean + u->s[US_TAIL].x / 16;
        rot16(-40 * 16, -8 * 16, lean, &tx, &ty);
        tx += cx; ty += cy;
        for (i = 0; i < 6; i++) {
            uni_out(u, tx, ty, &u->tail[i]);
            ang += -56 + pg_sin((int32_t)uni_now * 2 + i * 130 + u->x) * 26 / 32768 - u->s[US_TAIL].v / 24;
            tx += pg_cos(ang) * 11 * 16 >> 15;
            ty += pg_sin(ang) * 11 * 16 >> 15;
        }
    }
#undef BODY
#undef HEAD
}

/* ----------------------------------------------------------------- drawing */
static struct { uint8_t pass; uint16_t ink; int16_t ol; } rg;
static void rg_cap(pt_t a, int32_t ra, pt_t b, int32_t rb, uint16_t c)
{
    if (rg.pass)
        pg_capsule(a.x, a.y, ra, b.x, b.y, rb, c);
    else
        pg_capsule(a.x, a.y, ra + rg.ol, b.x, b.y, rb + rg.ol, rg.ink);
}
static void rg_circ(pt_t a, int32_t r, uint16_t c)
{
    if (rg.pass)
        pg_circle(a.x, a.y, r, c);
    else
        pg_circle(a.x, a.y, r + rg.ol, rg.ink);
}
static void rg_tri(pt_t a, pt_t b, pt_t c2, uint16_t c)
{
    if (rg.pass) {
        pg_tri(a.x, a.y, b.x, b.y, c2.x, c2.y, c);
    } else {                             /* (the outline of a triangle: thick lines along its sides) */
        pg_line(a.x, a.y, b.x, b.y, rg.ol * 2 + 1, rg.ink);
        pg_line(b.x, b.y, c2.x, c2.y, rg.ol * 2 + 1, rg.ink);
        pg_line(c2.x, c2.y, a.x, a.y, rg.ol * 2 + 1, rg.ink);
        pg_tri(a.x, a.y, b.x, b.y, c2.x, c2.y, rg.ink);
    }
}
static inline pt_t pt_lerp(pt_t a, pt_t b, int32_t t)                 /* t 0 .. 256 */
{
    pt_t p;
    p.x = (int16_t)(a.x + (b.x - a.x) * t / 256);
    p.y = (int16_t)(a.y + (b.y - a.y) * t / 256);
    return p;
}
/* a point off the line a -> b: t along it (0 .. 256), n pixels (at scale 1) to its left */
static pt_t pt_off(const uni_t *u, pt_t a, pt_t b, int32_t t, int32_t n)
{
    int32_t dx = b.x - a.x, dy = b.y - a.y, len = (int32_t)pg_isqrt((uint32_t)(dx * dx + dy * dy));
    pt_t p = pt_lerp(a, b, t);
    if (len < 1) len = 1;
    n = n * u->sc / 256;
    p.x = (int16_t)(p.x + dy * n / len);
    p.y = (int16_t)(p.y - dx * n / len);
    return p;
}

static ucol_t uni_colors(const uni_t *u)
{
    ucol_t c = UNI_COL[u->col % UNI_NCOL];
    if (u->col % UNI_NCOL == 11u) {      /* RAINBOW: the body's hue turns, the mane runs the other way */
        c.body = pg_mix(pg_hue(uni_now / 24u), 0xFFFF, 110);
        c.shade = pg_mix(pg_hue(uni_now / 24u), 0, 90);
        c.mane = pg_hue(uni_now / 12u + 128u);
        c.mane2 = pg_hue(uni_now / 12u + 40u);
    }
    return c;
}

static void uni_leg(const uni_t *u, uint32_t i, uint16_t c, uint16_t hoof)
{
    rg_cap(u->hip[i], uni_r(u, 8), u->knee[i], uni_r(u, 6), c);
    rg_cap(u->knee[i], uni_r(u, 6), u->foot[i], uni_r(u, 5), c);
    if (rg.pass) {
        pt_t h = pt_lerp(u->knee[i], u->foot[i], 200);
        pg_capsule(h.x, h.y, uni_r(u, 5), u->foot[i].x, u->foot[i].y, uni_r(u, 6), hoof);
    } else {
        rg_circ(u->foot[i], uni_r(u, 6), 0);
    }
}

static void uni_horn(const uni_t *u, const ucol_t *c)
{
    pt_t a = u->horn0, b = u->horn1, m;
    int32_t i;
    switch (u->horn % UH_COUNT) {
    case UH_GOLD:
        b = pt_lerp(a, b, 330);
        for (rg.pass = 0; rg.pass < 2; rg.pass++)
            rg_cap(a, uni_r(u, 5), b, 0, RGB(250, 205, 50));
        m = pt_off(u, a, b, 60, -2);
        pg_line(m.x, m.y, pt_lerp(m, b, 150).x, pt_lerp(m, b, 150).y, 1, RGB(255, 255, 220));
        break;
    case UH_DEVIL:
        for (i = 0; i < 2; i++) {
            pt_t r0 = pt_off(u, a, b, i ? -70 : 20, 0), r1 = pt_off(u, a, b, i ? 40 : 130, i ? 9 : 9), r2 = pt_off(u, a, b, i ? 110 : 200, i ? 2 : 2);
            for (rg.pass = 0; rg.pass < 2; rg.pass++) {
                rg_cap(r0, uni_r(u, 5), r1, uni_r(u, 3), RGB(150, 20, 20));
                rg_cap(r1, uni_r(u, 3), r2, 0, RGB(230, 60, 30));
            }
        }
        break;
    case UH_STUMP:
        b = pt_lerp(a, b, 70);
        for (rg.pass = 0; rg.pass < 2; rg.pass++)
            rg_cap(a, uni_r(u, 6), b, uni_r(u, 6), RGB(225, 200, 170));
        pg_circle(b.x, b.y, uni_r(u, 4), RGB(190, 150, 120));
        pg_circle(b.x, b.y, uni_r(u, 2), RGB(225, 200, 170));
        break;
    case UH_CRYSTAL: {
        pt_t l = pt_off(u, a, b, 110, 7), r = pt_off(u, a, b, 110, -7);
        b = pt_lerp(a, b, 300);
        for (rg.pass = 0; rg.pass < 2; rg.pass++) {
            rg_tri(a, l, r, RGB(60, 190, 230));
            rg_tri(l, b, r, RGB(120, 230, 255));
        }
        pg_tri(a.x, a.y, l.x, l.y, pt_lerp(l, r, 128).x, pt_lerp(l, r, 128).y, RGB(200, 250, 255));
        break;
    }
    case UH_CANDLE: {
        pt_t f;
        b = pt_lerp(a, b, 150);
        for (rg.pass = 0; rg.pass < 2; rg.pass++)
            rg_cap(a, uni_r(u, 4), b, uni_r(u, 4), RGB(240, 235, 215));
        m = pt_off(u, a, b, 130, 4);
        pg_circle(m.x, m.y, uni_r(u, 2), RGB(240, 235, 215));
        f = pt_lerp(a, b, 330 + (int32_t)(pg_hash(uni_now / 70u) & 31u));
        pg_capsule(b.x, b.y, uni_r(u, 4), f.x, f.y, 0, RGB(255, 140, 20));
        pg_capsule(b.x, b.y, uni_r(u, 2), pt_lerp(b, f, 150).x, pt_lerp(b, f, 150).y, 0, RGB(255, 240, 120));
        pg.blend = PB_ADD;
        pg_circle(f.x, f.y, uni_r(u, 9), RGB(40, 24, 0));
        pg.blend = PB_SOLID;
        break;
    }
    case UH_RAINBOW:
        pg_capsule(a.x, a.y, uni_r(u, 5) + rg.ol, b.x, b.y, rg.ol, c->ink);
        for (i = 0; i < 6; i++) {
            m = pt_lerp(a, b, i * 44);
            pg_circle(m.x, m.y, uni_r(u, 5 - i * 4 / 5), pg_hue(uni_now / 6u + (uint32_t)i * 40u));
        }
        pg.blend = PB_ADD;
        pg_circle(b.x, b.y, uni_r(u, 8), pg_mix(0, pg_hue(uni_now / 6u), 70));
        pg.blend = PB_SOLID;
        break;
    case UH_ANTLER: {
        pt_t k1 = pt_lerp(a, b, 110), k2 = pt_lerp(a, b, 190);
        for (rg.pass = 0; rg.pass < 2; rg.pass++) {
            rg_cap(a, uni_r(u, 4), b, uni_r(u, 2), RGB(215, 195, 160));
            rg_cap(k1, uni_r(u, 3), pt_off(u, a, b, 200, 12), uni_r(u, 1), RGB(215, 195, 160));
            rg_cap(k2, uni_r(u, 2), pt_off(u, a, b, 250, -10), uni_r(u, 1), RGB(215, 195, 160));
        }
        break;
    }
    default:                             /* the spiral */
        for (rg.pass = 0; rg.pass < 2; rg.pass++)
            rg_cap(a, uni_r(u, 5), b, 0, RGB(255, 240, 205));
        for (i = 1; i < 4; i++) {
            pt_t l = pt_off(u, a, b, i * 56 - 16, 4 - i), r = pt_off(u, a, b, i * 56 + 14, i - 4);
            pg_line(l.x, l.y, r.x, r.y, u->sc > 300 ? 3 : 1, RGB(240, 130, 180));
        }
        break;
    }
    rg.pass = 1;
}

static void uni_hat(const uni_t *u)
{
    /* the hat's frame: h0 on the skull's top, up along the head's own up */
    pt_t h0 = u->hat0, up, l, r, t;
    int32_t ux, uy, i;
    rot16(0, -16 * 16, u->ha - 30, &ux, &uy);
    up.x = (int16_t)(h0.x + u->flip * ((ux * u->sc) >> 12));
    up.y = (int16_t)(h0.y + ((uy * u->sc) >> 12));
    l = pt_off(u, h0, up, 0, 13);
    r = pt_off(u, h0, up, 0, -13);
    switch (u->hat % UT_COUNT) {
    case UT_CROWN:
        for (rg.pass = 0; rg.pass < 2; rg.pass++) {
            rg_cap(pt_off(u, h0, up, 30, 10), uni_r(u, 3), pt_off(u, h0, up, 30, -10), uni_r(u, 3), RGB(250, 200, 40));
            for (i = -1; i <= 1; i++)
                rg_tri(pt_off(u, h0, up, 40, i * 9 + 5), pt_off(u, h0, up, 230, i * 10), pt_off(u, h0, up, 40, i * 9 - 5), RGB(250, 200, 40));
        }
        for (i = -1; i <= 1; i++) {
            t = pt_off(u, h0, up, 40, i * 8);
            pg_circle(t.x, t.y, uni_r(u, 2), i ? RGB(230, 40, 80) : RGB(60, 220, 230));
        }
        break;
    case UT_WITCH:
        t = pt_off(u, h0, up, 620, 12 + pg_sin((int32_t)uni_now * 2) * 3 / 32768);
        for (rg.pass = 0; rg.pass < 2; rg.pass++) {
            rg_cap(pt_off(u, h0, up, 20, 22), uni_r(u, 3), pt_off(u, h0, up, 20, -22), uni_r(u, 3), RGB(45, 25, 70));
            rg_tri(pt_off(u, h0, up, 20, 12), pt_off(u, h0, up, 420, 3), pt_off(u, h0, up, 20, -12), RGB(45, 25, 70));
            rg_tri(pt_off(u, h0, up, 330, 7), t, pt_off(u, h0, up, 330, -3), RGB(45, 25, 70));
        }
        pg_line(pt_off(u, h0, up, 95, 10).x, pt_off(u, h0, up, 95, 10).y, pt_off(u, h0, up, 95, -10).x, pt_off(u, h0, up, 95, -10).y,
                uni_r(u, 4), RGB(120, 230, 60));
        t = pt_off(u, h0, up, 95, 0);
        pg_circle(t.x, t.y, uni_r(u, 3), RGB(250, 210, 50));
        break;
    case UT_TOP:
        for (rg.pass = 0; rg.pass < 2; rg.pass++) {
            rg_cap(pt_off(u, h0, up, 20, 18), uni_r(u, 3), pt_off(u, h0, up, 20, -18), uni_r(u, 3), RGB(30, 28, 40));
            rg_cap(pt_off(u, h0, up, 120, 0), uni_r(u, 11), pt_off(u, h0, up, 330, 0), uni_r(u, 12), RGB(30, 28, 40));
        }
        pg_line(pt_off(u, h0, up, 110, 11).x, pt_off(u, h0, up, 110, 11).y, pt_off(u, h0, up, 110, -11).x, pt_off(u, h0, up, 110, -11).y,
                uni_r(u, 5), RGB(200, 30, 50));
        break;
    case UT_BOW:
        t = pt_off(u, h0, up, 40, 6);
        for (rg.pass = 0; rg.pass < 2; rg.pass++) {
            rg_tri(t, pt_off(u, h0, up, 190, 20), pt_off(u, h0, up, -40, 22), RGB(240, 50, 110));
            rg_tri(t, pt_off(u, h0, up, 200, -8), pt_off(u, h0, up, 20, -12), RGB(240, 50, 110));
            rg_circ(t, uni_r(u, 4), RGB(255, 120, 170));
        }
        break;
    case UT_HALO: {
        pt_t c0 = pt_off(u, h0, up, 420 + pg_sin((int32_t)uni_now * 2) * 30 / 32768, 0);
        int32_t rx = uni_r(u, 15), ry = uni_r(u, 5);
        pg.blend = PB_ADD;
        pg_ellipse(c0.x, c0.y, rx + 3, ry + 3, RGB(50, 40, 0));
        pg.blend = PB_SOLID;
        for (i = 0; i < 12; i++) {
            int32_t a0 = i * 1024 / 12, a1 = (i + 1) * 1024 / 12;
            pg_line(c0.x + (pg_cos(a0) * rx >> 15), c0.y + (pg_sin(a0) * ry >> 15), c0.x + (pg_cos(a1) * rx >> 15),
                    c0.y + (pg_sin(a1) * ry >> 15), uni_r(u, 3), RGB(255, 235, 110));
        }
        break;
    }
    case UT_PARTY:
        t = pt_off(u, h0, up, 420, 0);
        for (rg.pass = 0; rg.pass < 2; rg.pass++)
            rg_tri(pt_off(u, h0, up, 10, 10), t, pt_off(u, h0, up, 10, -10), RGB(60, 200, 230));
        pg_line(pt_off(u, h0, up, 110, 7).x, pt_off(u, h0, up, 110, 7).y, pt_off(u, h0, up, 110, -7).x, pt_off(u, h0, up, 110, -7).y,
                uni_r(u, 4), RGB(250, 80, 160));
        pg_line(pt_off(u, h0, up, 240, 4).x, pt_off(u, h0, up, 240, 4).y, pt_off(u, h0, up, 240, -4).x, pt_off(u, h0, up, 240, -4).y,
                uni_r(u, 3), RGB(250, 220, 60));
        pg_circle(t.x, t.y, uni_r(u, 4), RGB(250, 220, 60));
        break;
    case UT_SKULL: {
        pt_t c0 = pt_off(u, h0, up, 60, -2), e;
        for (rg.pass = 0; rg.pass < 2; rg.pass++) {
            rg_circ(c0, uni_r(u, 12), RGB(235, 230, 215));
            rg_cap(pt_off(u, h0, up, -10, -10), uni_r(u, 5), pt_off(u, h0, up, -10, -16), uni_r(u, 4), RGB(235, 230, 215));
        }
        e = pt_off(u, h0, up, 60, -8);
        pg_circle(e.x, e.y, uni_r(u, 4), RGB(25, 15, 35));
        e = pt_off(u, h0, up, 70, 3);
        pg_circle(e.x, e.y, uni_r(u, 3), RGB(25, 15, 35));
        break;
    }
    case UT_PROP: {
        int32_t a0 = (int32_t)uni_now * 5, w = pg_cos(a0) * 16 / 32768;
        t = pt_off(u, h0, up, 250, 0);
        for (rg.pass = 0; rg.pass < 2; rg.pass++)
            rg_cap(pt_off(u, h0, up, 30, 9), uni_r(u, 7), pt_off(u, h0, up, 30, -9), uni_r(u, 7), RGB(230, 60, 50));
        pg_line(pt_off(u, h0, up, 60, 0).x, pt_off(u, h0, up, 60, 0).y, t.x, t.y, uni_r(u, 2), RGB(250, 220, 60));
        pg_line(pt_off(u, h0, up, 250, w).x, pt_off(u, h0, up, 250, w).y, pt_off(u, h0, up, 250, -w).x, pt_off(u, h0, up, 250, -w).y,
                uni_r(u, 4), RGB(60, 200, 240));
        break;
    }
    default:
        break;
    }
    (void)l; (void)r;
    rg.pass = 1;
}

/* clothes that go on over the body (after its fill) */
static void uni_cloth(const uni_t *u, const ucol_t *c)
{
    pt_t a = u->rump, b = u->chest;
    int32_t i;
    switch (u->cloth % UC_COUNT) {
    case UC_SWEATER:
        for (i = 0; i < 5; i++) {
            pt_t t0 = pt_off(u, a, b, 30 + i * 52, 17), t1 = pt_off(u, a, b, 30 + i * 52, -17);
            pg_line(t0.x, t0.y, t1.x, t1.y, uni_r(u, 7), i & 1 ? RGB(40, 150, 60) : RGB(200, 30, 40));
        }
        pg_line(u->neck0.x, u->neck0.y, pt_lerp(u->neck0, u->head, 60).x, pt_lerp(u->neck0, u->head, 60).y, uni_r(u, 14), RGB(200, 30, 40));
        break;
    case UC_TUTU:
        for (i = 0; i < 7; i++) {
            pt_t t0 = pt_off(u, a, b, 20 + i * 14, 14 - i * 6), t1 = pt_off(u, a, b, -40 + i * 26, 34 - i * 13);
            pg_tri(t0.x, t0.y, t1.x - uni_r(u, 6), t1.y, t1.x + uni_r(u, 6), t1.y + uni_r(u, 3), i & 1 ? RGB(255, 150, 210) : RGB(255, 205, 235));
        }
        pg_line(pt_off(u, a, b, 60, 19).x, pt_off(u, a, b, 60, 19).y, pt_off(u, a, b, 60, -20).x, pt_off(u, a, b, 60, -20).y, uni_r(u, 4),
                RGB(230, 70, 150));
        break;
    case UC_BONES:
        for (i = 0; i < 4; i++) {
            pt_t t0 = pt_off(u, a, b, 60 + i * 50, 12), t1 = pt_off(u, a, b, 40 + i * 50, -12);
            pg_line(t0.x, t0.y, t1.x, t1.y, uni_r(u, 3), RGB(240, 240, 225));
        }
        pg_line(pt_off(u, a, b, 20, 13).x, pt_off(u, a, b, 20, 13).y, pt_off(u, a, b, 250, 15).x, pt_off(u, a, b, 250, 15).y, uni_r(u, 3),
                RGB(240, 240, 225));
        for (i = 1; i < 4; i += 2) {
            pg_line(u->hip[i].x, u->hip[i].y, u->knee[i].x, u->knee[i].y, uni_r(u, 3), RGB(240, 240, 225));
            pg_line(u->knee[i].x, u->knee[i].y, pt_lerp(u->knee[i], u->foot[i], 180).x, pt_lerp(u->knee[i], u->foot[i], 180).y, uni_r(u, 3),
                    RGB(240, 240, 225));
            pg_circle(u->knee[i].x, u->knee[i].y, uni_r(u, 3), RGB(240, 240, 225));
        }
        break;
    case UC_MUMMY:
        for (i = 0; i < 6; i++) {
            pt_t t0 = pt_off(u, a, b, 10 + i * 46, 19), t1 = pt_off(u, a, b, 30 + i * 46 + (i & 1 ? 20 : -14), -19);
            pg_line(t0.x, t0.y, t1.x, t1.y, uni_r(u, 3), i & 1 ? RGB(225, 215, 190) : RGB(190, 180, 150));
        }
        for (i = 0; i < 4; i++) {
            pt_t k = pt_lerp(u->knee[i | 1], u->foot[i | 1], 60 + i * 30);
            pg_line(k.x - uni_r(u, 5), k.y, k.x + uni_r(u, 5), k.y + uni_r(u, 2), uni_r(u, 3), RGB(225, 215, 190));
        }
        {
            pt_t d0 = pt_off(u, a, b, 40, -19), d1;
            d1.x = (int16_t)(d0.x - u->flip * uni_r(u, 6) + pg_sin((int32_t)uni_now * 3) * 3 / 32768);
            d1.y = (int16_t)(d0.y + uni_r(u, 14));
            pg_line(d0.x, d0.y, d1.x, d1.y, uni_r(u, 3), RGB(225, 215, 190));
        }
        break;
    case UC_SPIKES:
        for (i = 0; i < 5; i++) {
            pt_t t0 = pt_off(u, a, b, 10 + i * 56, 17), t1 = pt_off(u, a, b, 60 + i * 56, 17), tp = pt_off(u, a, b, 20 + i * 56, 32);
            rg.pass = 0; rg_tri(t0, tp, t1, 0);
            rg.pass = 1; rg_tri(t0, tp, t1, i & 1 ? RGB(150, 150, 170) : RGB(210, 210, 225));
        }
        pg_line(u->neck0.x, u->neck0.y - uni_r(u, 6), pt_lerp(u->neck0, u->head, 40).x, pt_lerp(u->neck0, u->head, 40).y, uni_r(u, 6), RGB(40, 30, 50));
        break;
    default:
        break;
    }
    (void)c;
}

static void uni_draw(uni_t *u)
{
    ucol_t c = uni_colors(u);
    int32_t i, eo = u->s[US_EYE].x, mo = u->s[US_MOUTH].x, big = u->sc > 420;
    uni_solve(u);
    {   /* the whole unicorn's box: nothing to do outside it */
        int32_t r = uni_r(u, 75), cx = (u->chest.x + u->rump.x) / 2, cy = u->chest.y - uni_r(u, 14);
        if (pg_out(cx - r - uni_r(u, 20), cy - r - uni_r(u, 30), cx + r + uni_r(u, 20), u->y + 4))
            return;
    }
    rg.ink = c.ink;
    rg.ol = (int16_t)(u->sc >= 500 ? 4 : u->sc >= 200 ? 2 : 1);
    /* the cape and the wings' far side go behind everything */
    if (u->cloth % UC_COUNT == UC_CAPE) {
        int32_t w = pg_sin((int32_t)uni_now * 3) * uni_r(u, 5) / 32768 - u->s[US_BOB].v / 6;
        pt_t n0 = pt_lerp(u->neck0, u->head, 70), e0, e1;
        e0.x = (int16_t)(u->rump.x - u->flip * uni_r(u, 46)); e0.y = (int16_t)(u->rump.y - uni_r(u, 14) + w);
        e1.x = (int16_t)(u->rump.x - u->flip * uni_r(u, 34)); e1.y = (int16_t)(u->rump.y + uni_r(u, 26) + w / 2);
        for (rg.pass = 0; rg.pass < 2; rg.pass++) {
            rg_tri(n0, e0, e1, RGB(170, 20, 40));
            rg_tri(n0, e1, u->chest, RGB(120, 10, 35));
        }
    }
    if (u->cloth % UC_COUNT == UC_WINGS) {
        int32_t fl = pg_sin((int32_t)uni_now * 6) * uni_r(u, 12) / 32768;
        pt_t w0 = u->wither, w1, w2, w3;
        w1.x = (int16_t)(w0.x - u->flip * uni_r(u, 30)); w1.y = (int16_t)(w0.y - uni_r(u, 44) + fl);
        w2.x = (int16_t)(w0.x - u->flip * uni_r(u, 58)); w2.y = (int16_t)(w0.y - uni_r(u, 18) + fl * 2);
        w3.x = (int16_t)(w0.x - u->flip * uni_r(u, 34)); w3.y = (int16_t)(w0.y + uni_r(u, 2) + fl);
        for (rg.pass = 0; rg.pass < 2; rg.pass++) {
            rg_tri(w0, w1, w2, RGB(70, 30, 100));
            rg_tri(w0, w2, w3, RGB(100, 45, 140));
        }
    }
    /* the tail */
    for (rg.pass = 0; rg.pass < 2; rg.pass++)
        for (i = 0; i < 5; i++)
            rg_cap(u->tail[i], uni_r(u, 8 - i), u->tail[i + 1], uni_r(u, 7 - i), i & 1 ? c.mane2 : c.mane);
    /* the far legs */
    for (rg.pass = 0; rg.pass < 2; rg.pass++) {
        uni_leg(u, 0, c.shade, c.hoof);
        uni_leg(u, 2, c.shade, c.hoof);
    }
    /* the mane, behind the neck */
    for (rg.pass = 0; rg.pass < 2; rg.pass++)
        for (i = 0; i < 6; i++) {
            pt_t m = pt_off(u, u->wither, u->skull, 20 + i * 44, 9 + (pg_sin((int32_t)uni_now * 3 + i * 170) * 3 >> 15));
            rg_circ(m, uni_r(u, 9 - (i == 5)), i & 1 ? c.mane2 : c.mane);
        }
    /* body, near legs, neck and head: one shape under one outline */
    for (rg.pass = 0; rg.pass < 2; rg.pass++) {
        rg_cap(u->rump, uni_r(u, 21), u->chest, uni_r(u, 22), c.body);
        if (rg.pass) {                   /* the belly in shadow */
            pt_t s0 = pt_off(u, u->rump, u->chest, 30, -13), s1 = pt_off(u, u->rump, u->chest, 220, -14);
            pg_capsule(s0.x, s0.y, uni_r(u, 7), s1.x, s1.y, uni_r(u, 7), c.shade);
            uni_cloth(u, &c);
        }
        uni_leg(u, 1, c.body, c.hoof);
        uni_leg(u, 3, c.body, c.hoof);
        rg_cap(u->neck0, uni_r(u, 13), u->head, uni_r(u, 10), c.body);
        rg_tri(u->ear0, u->ear1, pt_off(u, u->ear0, u->ear1, 0, -7), c.body);
        rg_cap(u->jawp, uni_r(u, 7), u->jawt, uni_r(u, 6), c.body);
        if (rg.pass && mo > 40) {        /* the open mouth: dark, a tongue */
            pt_t top = pt_lerp(u->skull, u->muz, 200);
            pg_tri(u->jawp.x, u->jawp.y, top.x, top.y + uni_r(u, 6), u->jawt.x, u->jawt.y - uni_r(u, 3), RGB(90, 15, 35));
            pg_circle(pt_lerp(u->jawp, u->jawt, 150).x, pt_lerp(u->jawp, u->jawt, 150).y - uni_r(u, 3), uni_r(u, 3), RGB(240, 110, 140));
        }
        rg_cap(u->skull, uni_r(u, 15), u->muz, uni_r(u, 10), c.body);
        if (u->s[US_PUFF].x > 24)
            rg_circ(u->cheek, uni_r(u, 5) + u->s[US_PUFF].x * u->sc / 4096, c.body);
    }
    /* the forelock */
    for (rg.pass = 0; rg.pass < 2; rg.pass++) {
        pt_t f0 = pt_off(u, u->skull, u->horn0, 60, 8), f1 = pt_off(u, u->skull, u->muz, 40, 13);
        rg_cap(f0, uni_r(u, 7), f1, uni_r(u, 4), c.mane);
    }
    /* the face */
    {
        pt_t e = u->eye, p;
        int32_t er = uni_r(u, 6) * (eo > 256 ? eo : 256) / 256, ery = er * (eo > 256 ? 256 : eo < 0 ? 0 : eo) / 256;
        if (ery < 2) {                   /* shut: a line */
            pg_line(e.x - er, e.y, e.x + er, e.y + 1, uni_r(u, 2), c.ink);
        } else {
            pg_ellipse(e.x, e.y, er + 1, ery + 1, c.ink);
            pg_ellipse(e.x, e.y, er - (big ? 2 : 0), ery - (big ? 2 : 0), 0xFFFF);
            p.x = (int16_t)(e.x + u->flip * (u->s[US_PX].x * er / 80));
            p.y = (int16_t)(e.y + u->s[US_PY].x * ery / 80);
            pg_ellipse(p.x, p.y, er * 5 / 8, ery * 5 / 8 > er * 5 / 8 ? er * 5 / 8 : ery * 5 / 8, c.eye);
            pg_ellipse(p.x, p.y, er * 3 / 10, ery * 3 / 10, c.ink);
            pg_circle(p.x - er / 4, p.y - ery / 3, er / 6, 0xFFFF);
        }
        {   /* the brow */
            int32_t ba = u->ha + u->s[US_BROW].x / 16 * -1, bx, by;
            pt_t b0;
            rot16(7 * 16, 0, ba, &bx, &by);
            b0.x = e.x; b0.y = (int16_t)(e.y - er - uni_r(u, 3));
            bx = u->flip * ((bx * u->sc) >> 12); by = (by * u->sc) >> 12;
            pg_line(b0.x - bx, b0.y - by, b0.x + bx, b0.y + by, uni_r(u, 2) + 1, c.ink);
        }
        pg.blend = PB_STIP;
        pg_circle(u->cheek.x, u->cheek.y, uni_r(u, 4), RGB(255, 110, 150));
        pg.blend = PB_SOLID;
        pg_circle(u->nostril.x, u->nostril.y, uni_r(u, 2) - (u->sc < 200), c.ink);
        if (mo <= 40) {                  /* shut: a smile */
            pt_t m0 = pt_lerp(u->jawp, u->jawt, 60), m1 = u->mouth;
            pg_line(m0.x, m0.y - uni_r(u, 1), m1.x, m1.y - uni_r(u, 3), uni_r(u, 1) + (big ? 2 : 0), c.ink);
        } else if (big) {                /* close up: teeth */
            pt_t t0 = pt_lerp(u->skull, u->muz, 150), t1 = pt_lerp(u->skull, u->muz, 240);
            for (i = 0; i < 4; i++) {
                pt_t q = pt_lerp(t0, t1, i * 64 + 20);
                pg_rect(q.x - uni_r(u, 1), q.y + uni_r(u, 7), uni_r(u, 3), uni_r(u, 3), 0xFFFF);
            }
        }
    }
    rg.pass = 1;
    uni_hat(u);
    uni_horn(u, &c);
    if (u->col % UNI_NCOL == 10u) {      /* ZOMBIE: stitches */
        pt_t s0 = pt_off(u, u->rump, u->chest, 110, 14), s1 = pt_off(u, u->rump, u->chest, 150, -6);
        pg_line(s0.x, s0.y, s1.x, s1.y, 1, c.ink);
        for (i = 0; i < 3; i++) {
            pt_t q = pt_lerp(s0, s1, 50 + i * 70);
            pg_line(q.x - uni_r(u, 3), q.y - 1, q.x + uni_r(u, 3), q.y + 1, 1, c.ink);
        }
    }
}
