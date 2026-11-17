/* SPDX-License-Identifier: GPL-3.0-only */
/* PUNICORN's scenes (puni.c has what each one is). A scene has a sound (scene_sound: an engine and a preset on
 * track 1), knobs (scene_apply: this scene's seven values into the sound's parameters; scene_knob: what the
 * screen does when one turns), a state that moves (scene_update, once a frame) and a picture (scene_draw, run
 * for every part of the screen that changed: it must only read). What is seen comes from what is heard: pu.env
 * is track 1's envelope, the sequencer's step is the unicorn's tile, the arpeggiator's step is a blob. */
static struct {
    /* FART BASS */
    int16_t recoil, recoil_v;
    uint32_t emit;
    /* BEAT STREET */
    uint8_t w_idx, w_prev, w_flash[NLANE];
    uint32_t w_ms;
    int16_t slide;
    /* PEE WAVES, RAINBOW BARF: the stream's front and its end, 0 .. 512 */
    int16_t head, tail;
    /* BOOGER BELLS */
    struct { int16_t x0, y0, x1, y1; uint32_t t0; uint8_t on, size; } fly[4];
    struct { int8_t x, y; uint8_t size, on; uint16_t age; } stuck[18];
    int16_t wob, wob_v, hit_y;
    uint32_t note_ms;
    uint8_t count;
    /* RAINBOW BARF */
    struct { uint32_t t0; uint8_t hue, on; } chunk[8];
    uint32_t arp_last;
    int16_t puddle, heave;
    /* GHOST GLOW */
    struct { int16_t x, y, ty; uint8_t key, state, r; uint16_t age; } gh[8];
} ss;

static const uint16_t LANE_COL[NLANE] = {RGB(235, 50, 55), RGB(245, 140, 30), RGB(245, 220, 50), RGB(70, 210, 80),
                                         RGB(60, 210, 220), RGB(80, 120, 245), RGB(165, 85, 235), RGB(245, 100, 190)};
static const char *const ROAD_NAME[NLANE] = {"KICK", "SNARE", "CLAP", "HAT", "OPEN HAT", "TOM", "RIM", "BELL"};
static const char *const KIT_NAME[6] = {"CLASSIC", "KIT 80", "KIT 10", "KIT 66", "KIT 55", "KIT 77"};
static const uint8_t KIT_OF[6] = {DK_STD, DK_80, DK_10, DK_66, DK_55, DK_77};
static const char *const HOME_SND[6] = {"MUSIC BOX", "HARP", "ORGAN", "CHIP", "STRINGS", "FLUTE"};
static const char *const FLAVOR[5] = {"BRAAAP", "BOOM", "SQUEAKER", "RIPPER", "WET ONE"};
static const uint8_t FLAVOR_K[5][6] = {{70, 70, 50, 30, 90, 40}, {95, 85, 0, 12, 70, 40}, {30, 25, 75, 10, 127, 95},
                                       {115, 40, 115, 75, 100, 20}, {60, 95, 95, 50, 62, 115}};
static const char *const HARM_NAME[8] = {"HALF", "SAME", "FIFTH", "DOUBLE", "X3", "X4", "X5", "X6"};
static const uint8_t HARM_SIZE[8] = {20, 16, 14, 13, 11, 10, 9, 8};      /* the second unicorn's size, /16: higher is smaller */
static const char *const VIEW_NAME[3] = {"BOTH", "SOLO A", "SOLO B"};
static const char *const TARGET_NAME[5] = {"BELL", "BONES", "WEB", "CAULDRON", "CRYSTAL"};
static const char *const TARGET_SND[5] = {"BELL TREE", "MARIMBA", "PLUCK", "HAND DRUM", "KALIMBA"};
static const char *const VOICE_NAME[4] = {"MONSTER", "SINGER", "ANGEL", "GHOST"};
static const char *const VOICE_SND[4] = {"WOW BASS", "VOX LEAD", "CHOIR AAH", "WHISPER"};
static const char *const SPEED_NAME[4] = {"SLOW", "MEDIUM", "FAST", "WILD"};
static const uint8_t SPEED_DIV[4] = {1, 2, 5, 3};
static const char *const RANGE_NAME[4] = {"1 OCTAVE", "2 OCTAVES", "3 OCTAVES", "4 OCTAVES"};
static const char *const ORDER_NAME[4] = {"UP", "DOWN", "UP DOWN", "RANDOM"};
static const char *const BARF_SND[5] = {"PLUCK", "CHIP", "LEAD", "MARIMBA", "SAW"};
static const char *const CHOIR_NAME[4] = {"CHOIR", "PAD", "STRINGS", "ORGAN"};
static const char *const CHORD_NAME[4] = {"ONE", "MAJOR", "MINOR", "POWER"};
static const uint8_t CHORD_OF[4] = {CH_OFF, CH_MAJ, CH_MIN, CH_POW};
static const char *const ONOFF_NAME[2] = {"OFF", "ON"};
#define BOOG_X 178
#define BOOG_Y 128

static const char *scene_kname(uint32_t k, uint32_t v)
{
    switch (pu.scene) {
    case SC_HOME:
        return k == KN_1 ? UNI_COL_NAME[v] : k == KN_2 ? UNI_CLOTH_NAME[v] : k == KN_3 ? UNI_HORN_NAME[v] : k == KN_4 ? UNI_HAT_NAME[v]
               : k == KN_SEL ? THEMES[v].name : k == KN_ALG ? VIBE_NAME[v] : HOME_SND[v];
    case SC_FART: return k == KN_PRE ? FLAVOR[v] : 0;
    case SC_WALK: return k == KN_3 ? KIT_NAME[v] : k == KN_SEL ? ROAD_NAME[v] : k == KN_PRE ? PU_BEAT_NAME[v] : 0;
    case SC_PEE: return k == KN_3 ? HARM_NAME[v] : k == KN_PRE ? VIEW_NAME[v] : 0;
    case SC_BOOG: return k == KN_PRE ? TARGET_NAME[v] : 0;
    case SC_BURP: return k == KN_PRE ? VOICE_NAME[v] : 0;
    case SC_PUKE:
        return k == KN_1 ? SPEED_NAME[v] : k == KN_2 ? RANGE_NAME[v] : k == KN_3 ? ORDER_NAME[v] : k == KN_SEL ? BARF_SND[v]
               : k == KN_PRE ? ONOFF_NAME[v] : 0;
    default: return k == KN_SEL ? CHOIR_NAME[v] : k == KN_ALG ? CHORD_NAME[v] : 0;
    }
}
static int32_t scene_octave(void) { return pu.scene == SC_FART || pu.scene == SC_BURP ? -1 : 0; }

/* the scene's sound into track 1 */
static void scene_sound(void)
{
    track_t *t = &trk[0];
    switch (pu.scene) {
    case SC_HOME:
        switch (KV(KN_PRE)) {
        case 1: pu_sound(t, ENGI_PHYS, "HARP"); break;
        case 2: pu_sound(t, 7, "FULL ORGAN"); break;
        case 3: pu_sound(t, 3, "PULSE LD"); break;
        case 4: pu_sound(t, 0, "STRINGS"); break;
        case 5: pu_sound(t, 7, "SOFT FLUTE"); break;
        default: pu_sound(t, ENGI_PHYS, "KALIMBA"); break;
        }
        t->p[P_VOICE] = V_POLY;
        if (t->p[P_REV] < 40)
            t->p[P_REV] = 40;
        break;
    case SC_FART:
        pu_puni(t, 0);
        break;
    case SC_PEE:
        pu_puni(t, 1);
        break;
    case SC_BOOG:
        pu_sound(t, ENGI_PHYS, TARGET_SND[KV(KN_PRE) % 5u]);
        KV(KN_1) = (uint8_t)t->p[P_E1];           /* the knobs start where the thing's own sound is */
        KV(KN_2) = (uint8_t)t->p[P_E2];
        KV(KN_3) = (uint8_t)t->p[P_E3];
        KV(KN_4) = (uint8_t)t->p[P_E4];
        KV(KN_ALG) = (uint8_t)t->p[P_E5];
        break;
    case SC_BURP:
        pu_sound(t, 5, VOICE_SND[KV(KN_PRE) % 4u]);
        KV(KN_1) = (uint8_t)t->p[P_E0];
        KV(KN_2) = (uint8_t)t->p[P_E1];
        KV(KN_ALG) = (uint8_t)clamp(t->p[P_E2], 30, 127);
        break;
    case SC_PUKE:
        switch (KV(KN_SEL)) {
        case 1: pu_sound(t, 3, "ARP 8BIT"); break;
        case 2: pu_sound(t, 6, "ARP LEAD"); break;
        case 3: pu_sound(t, ENGI_PHYS, "MARIMBA"); break;
        case 4: pu_sound(t, 0, "SAW LEAD"); break;
        default: pu_sound(t, 0, "PLUCK"); break;
        }
        t->p[P_VOICE] = V_POLY;
        break;
    case SC_GHOST:
        switch (KV(KN_SEL)) {
        case 1: pu_sound(t, 0, "SOFT PAD"); break;
        case 2: pu_sound(t, 0, "STRINGS"); break;
        case 3: pu_sound(t, 7, "FULL ORGAN"); break;
        default: pu_sound(t, 5, "CHOIR AAH"); break;
        }
        t->p[P_VOICE] = V_POLY;
        break;
    default:
        break;
    }
}

/* the scene's knobs into the sound */
static void scene_apply(void)
{
    track_t *t = &trk[0], *d = &trk[3];
    perf_k[0] = 0;
    switch (pu.scene) {
    case SC_FART:
        t->p[P_ATK] = 0;
        t->p[P_DEC] = t->p[P_REL] = (int16_t)(46 + KV(KN_1) * 64 / 127);
        t->p[P_SUS] = 0;
        t->p[P_E1] = KV(KN_2);
        t->p[P_E2] = 70;
        t->p[P_E3] = KV(KN_3);
        t->p[P_E4] = KV(KN_4);
        t->p[P_E5] = KV(KN_SEL);
        t->p[P_E6] = KV(KN_ALG);
        t->p[P_LEVEL] = 112;
        t->p[P_REV] = 8;
        t->p[P_DLY] = 0;
        break;
    case SC_WALK:
        song.g[G_BPM] = (int16_t)(55 + KV(KN_1));
        song.g[G_SWING] = (int16_t)(KV(KN_2) * 80 / 127);
        d->p[P_E0] = KIT_OF[KV(KN_3) % 6u];
        d->p[P_E1] = KV(KN_4);
        d->p[P_E3] = KV(KN_ALG);
        break;
    case SC_PEE:
        t->p[P_E1] = KV(KN_1);
        t->p[P_E2] = KV(KN_2);
        t->p[P_E3] = KV(KN_3);
        t->p[P_E4] = (int16_t)(KV(KN_PRE) == 1u ? 0 : KV(KN_PRE) == 2u ? 127 : KV(KN_4));
        t->p[P_E5] = KV(KN_SEL);
        t->p[P_ATK] = KV(KN_ALG);
        t->p[P_DEC] = 70;
        t->p[P_SUS] = 105;
        t->p[P_REL] = (int16_t)clamp(24 + KV(KN_ALG), 0, 127);
        t->p[P_LEVEL] = 100;
        t->p[P_REV] = 36;
        t->p[P_CHOR] = 20;
        break;
    case SC_BOOG:
        t->p[P_E1] = KV(KN_1);
        t->p[P_E2] = KV(KN_2);
        t->p[P_E3] = KV(KN_3);
        t->p[P_E4] = KV(KN_4);
        t->p[P_E5] = KV(KN_ALG);
        t->p[P_REV] = KV(KN_SEL);
        t->p[P_VOICE] = V_POLY;
        break;
    case SC_BURP:
        t->p[P_E0] = KV(KN_1);
        t->p[P_E1] = KV(KN_2);
        t->p[P_E2] = KV(KN_ALG);
        t->p[P_E3] = (int16_t)(KV(KN_SEL) - 12);
        t->p[P_E4] = KV(KN_3);
        t->p[P_E5] = KV(KN_4);
        t->p[P_LWAVE] = 4;                        /* a random wobble of the pitch: the gurgle */
        t->p[P_LRATE] = 112;
        t->p[P_LD_PIT] = (int16_t)(KV(KN_3) / 9);
        t->p[P_ED_PIT] = -14;
        t->p[P_REV] = 20;
        t->p[P_DLY] = 0;
        break;
    case SC_PUKE:
        t->p[P_AMODE] = (int16_t)(1 + KV(KN_3));  /* UP DN UPDN RND */
        t->p[P_ARATE] = SPEED_DIV[KV(KN_1) & 3u];
        t->p[P_AOCT] = (int16_t)(1 + KV(KN_2));
        t->p[P_AGATE] = (int16_t)(24 + KV(KN_ALG) * 90 / 127);
        t->p[P_AHOLD] = KV(KN_PRE);
        t->p[P_DLY] = KV(KN_4);
        break;
    case SC_GHOST:
        t->p[P_ATK] = KV(KN_1);
        t->p[P_REL] = KV(KN_2);
        t->p[P_SUS] = 110;
        t->p[P_LD_PIT] = (int16_t)(KV(KN_3) / 6);
        t->p[P_LRATE] = 74;
        t->p[P_REV] = KV(KN_4);
        t->p[P_CHOR] = 60;
        t->p[P_CHRD] = CHORD_OF[KV(KN_ALG) & 3u];
        perf_k[0] = (int8_t)-((127 - KV(KN_PRE)) / 3);   /* BRIGHT: Felucca's FX filter, closing */
        break;
    default:
        break;
    }
}

static void pu_scene(uint32_t s)
{
    uint32_t i;
    pu.scene = (uint8_t)s;
    pu.scene_ms = pu.now;
    pu.hint_on = 0;
    pfx_clear();
    pg.full = 1;
    for (i = 0; i < sizeof ss; i++)
        ((uint8_t *)&ss)[i] = 0;
    ss.arp_last = trk[0].arp_idx;
    uni_init(&pu.u[0], 112, 210, 300, 1);
    uni_init(&pu.u[1], 200, 208, 150, -1);
    switch (s) {
    case SC_FART: pu.u[0].x = 146; pu.u[0].sc = 250; break;
    case SC_WALK: pu.u[0].sc = 100; ss.w_idx = 0xFF; break;
    case SC_PEE: pu.u[0].x = 40; pu.u[0].y = 208; pu.u[0].sc = 150; break;
    case SC_BOOG: pu.u[0].x = 58; pu.u[0].sc = 250; break;
    case SC_PUKE: pu.u[0].x = 52; pu.u[0].sc = 240; break;
    case SC_GHOST: pu.u[0].x = 46; pu.u[0].y = 212; pu.u[0].sc = 200; break;
    default: break;
    }
    pu_look_apply();
    scene_sound();
    scene_apply();
    if (s == SC_WALK && !song.playing)            /* the street walks by itself */
        transport_req = 1;
    pop_say(SC_NAME[s], 120, 34, 2, th->acc);
    pfx_burst(PT_STAR, 120, 110, 14, 110, 5, th->acc, 700, 2);
}

/* a knob turned (its value is in already): the sound, and what the screen does about it */
static void scene_knob(uint32_t k, int32_t d)
{
    uni_t *u = &pu.u[0];
    switch (pu.scene) {
    case SC_HOME:
        pu_look_apply();
        pu.look_dirty = 1;
        if (k <= KN_4) {
            pt_t at = k == KN_3 ? u->horn1 : k == KN_4 ? u->hat0 : u->chest;
            uni_act(u, UA_WOW);
            pfx_burst(PT_STAR, at.x, at.y, 10, 90, 5, pg_hue(pu_rnd()), 600, 3);
            pfx_burst(PT_HEART, at.x, at.y, 3, 60, 4, RGB(255, 100, 160), 700, 2);
        } else if (k == KN_PRE) {
            scene_sound();
        } else {
            pg.full = 1;
            pfx_burst(PT_STAR, 120, 100, 16, 100, 5, th->acc, 700, 2);
        }
        break;
    case SC_FART:
        if (k == KN_PRE) {
            uint32_t i;
            for (i = 0; i < 6u; i++)
                KV(i) = FLAVOR_K[KV(KN_PRE) % 5u][i];
        }
        break;
    case SC_WALK:
        if (k == KN_PRE) {
            pu_beat_load(KV(KN_PRE));
            pfx_burst(PT_STAR, 120, 150, 14, 100, 5, 0xFFFF, 600, 2);
        }
        if (k == KN_SEL)
            ss.slide = (int16_t)(d > 0 ? 48 * 16 : -48 * 16);
        break;
    case SC_PEE:
        if (k == KN_PRE)
            pg.full = 1;
        break;
    case SC_BOOG:
        if (k == KN_PRE) {
            uint32_t i;
            scene_sound();
            for (i = 0; i < 18u; i++)
                ss.stuck[i].on = 0;
            pfx_burst(PT_GOO, BOOG_X, BOOG_Y, 10, 70, 4, RGB(140, 230, 60), 700, 5);
        }
        break;
    case SC_BURP:
        if (k == KN_PRE)
            scene_sound();
        break;
    case SC_PUKE:
        if (k == KN_SEL)
            scene_sound();
        break;
    case SC_GHOST:
        if (k == KN_SEL)
            scene_sound();
        break;
    default:
        break;
    }
    scene_apply();
}

/* the white keys' lights on BEAT STREET: the road's tiles, the playing one blinking */
static uint32_t scene_key_leds(void)
{
    uint32_t m = 0, k, lane = KV(KN_SEL) % NLANE;
    if (pu.scene != SC_WALK)
        return 0;
    for (k = 0; k < 27u; k++)
        if (!key_black(k)) {
            uint32_t i = key_place(k), on = (trk[3].step[i].hit >> lane) & 1u;
            if (song.playing && i == ss.w_idx)
                on = !on;
            m |= on << k;
        }
    return m;
}

/* a point of an arc from (x0, y0) to (x1, y1) that rises h in its middle; s 0 .. 256 */
static void arc_pt(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t h, int32_t s, int32_t *x, int32_t *y)
{
    *x = x0 + (x1 - x0) * s / 256;
    *y = y0 + (y1 - y0) * s / 256 - h * 4 * s * (256 - s) / 65536;
}
/* what PEE's oscillators make (eng_puni.c puni_wave), for the eye: ph 1024 a cycle, -256 .. 256 */
static int32_t pee_wave(int32_t ph, int32_t shape)
{
    int32_t pos = shape * 3, seg = pos >> 7, f = pos & 127, w[4], p = ph & 1023;
    w[0] = pg_sin(p) >> 7;
    w[1] = p < 256 ? p : p < 768 ? 512 - p : p - 1024;
    w[2] = p < 512 ? p / 2 : p / 2 - 512;
    w[3] = p < 512 ? 230 : -230;
    if (seg > 2) { seg = 2; f = 127; }
    return w[seg] + (w[seg + 1] - w[seg]) * f / 128;
}
/* the stream's front and end: it runs out while the sound lasts, and drains when it stops */
static void stream_flow(int32_t on, int32_t rate)
{
    if (on) {
        ss.head = (int16_t)clamp(ss.head + rate, 0, 512);
        ss.tail = 0;
    } else if (ss.head > 0) {
        ss.tail = (int16_t)(ss.tail + rate);
        if (ss.tail >= ss.head)
            ss.head = ss.tail = 0;
    }
}

/* ---------------------------------------------------------------- update */
static void scene_update(void)
{
    uni_t *u = &pu.u[0];
    uint32_t k, i;
    int32_t dt = (int32_t)pu.dt, env = pu.env;
    if (pu.asleep)
        return;
    u->look_x = (int16_t)(pu.nheld ? (pu.key - 13) * 2 : 0);
    u->look_y = (int16_t)(pu.nheld ? 10 : 0);
    switch (pu.scene) {
    case SC_HOME:
        if (env > 12 && u->act == UA_IDLE)
            uni_act(u, UA_SING);
        if (u->act == UA_SING) {
            u->amp = (int16_t)env;
            if (env < 6)
                u->act = UA_IDLE;
        }
        for (k = 0; k < 27u; k++)
            if ((pu.edges >> k) & 1u) {
                pfx_add(PT_NOTE, u->horn1.x, u->horn1.y, pu_rand(-30, 40), -pu_rand(30, 60), 3, pg_hue(k * 9u), 1000, 2);
                pfx_add(PT_STAR, u->horn1.x, u->horn1.y, pu_rand(-20, 20), -pu_rand(0, 20), 5, 0xFFFF, 300, 0);
            }
        break;

    case SC_FART: {
        uint16_t gas = KV(KN_4) < 64 ? pg_mix(RGB(205, 235, 130), RGB(110, 200, 50), KV(KN_4) * 4u)
                                    : pg_mix(RGB(110, 200, 50), RGB(125, 60, 115), (KV(KN_4) - 64u) * 4u);
        int32_t small = 32 - pu.key / 2;          /* low notes: big clouds */
        u->amp = (int16_t)env;
        if (env > 8 && u->act != UA_FART)
            uni_act(u, UA_FART);
        for (k = 0; k < 27u; k++)
            if ((pu.edges >> k) & 1u) {
                pop_say(KV(KN_3) > 90 ? "BRRRAP!" : k < 9u ? "BRAAAP!" : k < 18u ? "PFFFT!" : "TOOT!", u->butt.x - 30, u->butt.y - 46, 3, gas);
                pu_shake(2 + (26 - (int32_t)k) / 5);
                ss.recoil_v = (int16_t)(ss.recoil_v + 500 + (26 - (int32_t)k) * 12);
                pfx_add(PT_RING, u->butt.x, u->butt.y, -30, 0, 4, 0xFFFF, 350, 0)->aux = 60;
                pfx_burst(PT_DOT, u->foot[1].x, u->foot[1].y, 4, 50, 3, th->gnd1, 400, 4);
            }
        /* the gas: as much as the sound is loud, as fast as its pitch falls, as big as it is long */
        ss.emit += (uint32_t)(env * dt);
        while (ss.emit > 2200u) {
            ss.emit -= 2200u;
            pfx_add(PT_PUFF, u->butt.x, u->butt.y + pu_rand(-4, 4), -pu_rand(50, 80 + KV(KN_2)) * env / 256 - 20, pu_rand(-34, 14),
                    (uint32_t)(4 + small * (40 + KV(KN_1)) / 240 + pu_rand(0, 3)), pu_rnd() & 3u ? gas : pg_mix(gas, 0, 60),
                    (uint32_t)(380 + KV(KN_1) * 6), -1);
            if (KV(KN_3) > 30 && (pu_rnd() & 127u) < KV(KN_3))        /* FLAP: wet */
                pfx_add(PT_DROP, u->butt.x, u->butt.y, -pu_rand(40, 130), -pu_rand(10, 70), 2, pg_mix(gas, RGB(90, 60, 20), 120), 600, 7);
            if (KV(KN_3) > 60 && (pu_rnd() & 3u) == 0u)
                pfx_add(PT_RING, u->butt.x - 6, u->butt.y, -50, 0, 3, pg_mix(gas, 0xFFFF, 120), 260, 0)->aux = 40;
            if (KV(KN_4) > 70 && (pu_rnd() & 7u) == 0u)               /* STINK: flies */
                pfx_add(PT_CHUNK, u->butt.x - pu_rand(10, 60), u->butt.y - pu_rand(0, 40), pu_rand(-40, 40), pu_rand(-40, 40), 2,
                        RGB(15, 10, 20), 900, 0);
        }
        /* the kick: the unicorn is pushed forward and springs back */
        ss.recoil_v = (int16_t)(ss.recoil_v - ss.recoil * dt / 24 - ss.recoil_v * dt / 90);
        ss.recoil = (int16_t)clamp(ss.recoil + ss.recoil_v * dt / 16, -200, 900);
        u->x = (int16_t)(146 + ss.recoil / 16);
        break;
    }

    case SC_WALK: {
        track_t *t = &trk[3];
        uint32_t lane = KV(KN_SEL) % NLANE, step_ms = 15000u / (uint32_t)clamp(song.g[G_BPM], 40, 240);
        int32_t ph, tx, y0 = 124 + 34;
        for (k = 0; k < 27u; k++)
            if (((pu.edges >> k) & 1u) && !key_black(k)) {            /* a white key: its tile, on or off */
                i = key_place(k);
                grid_hit(t, i, lane, 2);
                if ((t->step[i].hit >> lane) & 1u) {
                    fm1_irq_off();
                    trk_note_on(t, DRUM_LANE_NOTE[lane], 110);
                    trk_note_off(t, DRUM_LANE_NOTE[lane]);
                    fm1_irq_on();
                    pfx_burst(PT_STAR, (int32_t)i * 15 + 7, 144, 8, 80, 5, LANE_COL[lane], 500, 3);
                } else {
                    pfx_burst(PT_DOT, (int32_t)i * 15 + 7, 144, 5, 50, 3, RGB(90, 90, 110), 300, 4);
                }
                if (!song.playing)
                    ss.w_idx = ss.w_prev = (uint8_t)i;
            }
        if (song.playing) {
            uint32_t idx = t->seq_idx % 16u;
            if (idx != ss.w_idx) {                                    /* a new step: the unicorn lands on it */
                uint32_t hits = t->step[idx].hit, l;
                ss.w_prev = ss.w_idx > 15u ? (uint8_t)idx : ss.w_idx;
                ss.w_idx = (uint8_t)idx;
                ss.w_ms = pu.now;
                for (l = 0; l < NLANE; l++)
                    if ((hits >> l) & 1u) {
                        ss.w_flash[l] = 255;
                        if (l == lane) {
                            pfx_burst(PT_STAR, (int32_t)idx * 15 + 7, 148, 6, 90, 5, 0xFFFF, 400, 3);
                            pfx_add(PT_RING, (int32_t)idx * 15 + 7, 146, 0, 0, 4, LANE_COL[l], 300, 0)->aux = 50;
                        }
                    }
                if (hits & 1u)
                    pu_shake(2);
            }
        }
        for (i = 0; i < NLANE; i++)
            ss.w_flash[i] = (uint8_t)(ss.w_flash[i] > dt * 2 ? ss.w_flash[i] - dt * 2 : 0);
        ss.slide = (int16_t)(ss.slide - ss.slide * (dt > 60 ? 60 : dt) / 70);
        /* hopping from tile to tile, dancing on the beat */
        ph = song.playing ? clamp((int32_t)(pu.now - ss.w_ms) * 256 / (int32_t)step_ms, 0, 256) : 256;
        if (ss.w_idx > 15u)
            ss.w_idx = ss.w_prev = 0;
        tx = ss.w_prev * 15 + 7;
        if (ss.w_idx >= ss.w_prev)
            tx += (ss.w_idx * 15 + 7 - tx) * ph / 256;
        else
            tx = ss.w_idx * 15 + 7;                                   /* (the end of the street: back to its start) */
        u->x = (int16_t)(tx - 6);
        u->y = (int16_t)(y0 - (song.playing ? pg_sin(ph * 2) * 7 >> 15 : 0));
        if (song.playing) {
            u->act = UA_DANCE;
            u->amp = (int16_t)(((ss.w_idx & 3) * 256 + ph) / 4);
        } else if (u->act == UA_DANCE) {
            u->act = UA_IDLE;
        }
        break;
    }

    case SC_PEE: {
        uint32_t view = KV(KN_PRE);
        uni_t *b = &pu.u[1];
        if (view == 1u) {
            u->x = 60; u->y = 214; u->sc = 330;
        } else {
            u->x = 40; u->y = 208; u->sc = 150;
        }
        if (view == 2u) {
            b->x = 180; b->y = 214; b->sc = 330;
        } else {
            b->x = 200; b->y = 208; b->sc = (int16_t)(150 * HARM_SIZE[KV(KN_3) & 7u] / 16);
        }
        for (i = 0; i < 2u; i++) {
            uni_t *q = &pu.u[i];
            q->amp = (int16_t)(pu.nheld || env > 40 ? 256 : env * 4 > 256 ? 256 : env * 4);
            if (q->act != UA_PEE)
                uni_act(q, UA_PEE);
        }
        stream_flow(env > 6, dt * 256 / (50 + KV(KN_ALG) * 4));
        if (ss.head >= (view ? 256 : 512) && env > 6 && (pu_rnd() & 1u)) {
            int32_t lx = view == 1u ? 206 : view == 2u ? 34 : 120;
            pfx_add(PT_DROP, lx + pu_rand(-8, 8), view ? 204 : 196, pu_rand(-50, 50), -pu_rand(30, 80) * env / 256, 2,
                    RGB(250, 225, 50), 500, 8);
        }
        break;
    }

    case SC_BOOG: {
        uint16_t goo = pg_mix(RGB(150, 240, 60), RGB(100, 105, 40), KV(KN_2) * 2u);
        for (k = 0; k < 27u; k++)
            if ((pu.edges >> k) & 1u) {
                uint32_t f;
                uni_act(u, UA_FLICK);
                ss.note_ms = pu.now;
                ss.wob_v = (int16_t)(ss.wob_v + 500);                 /* (the sound is there at once: so is the wobble) */
                for (f = 0; f < 3u && ss.fly[f].on; f++)
                    ;
                ss.fly[f].on = 1;
                ss.fly[f].t0 = pu.now;
                ss.fly[f].x0 = u->nostril.x; ss.fly[f].y0 = u->nostril.y;
                ss.fly[f].x1 = (int16_t)(BOOG_X + (int32_t)((k * 37u) % 41u) - 20);
                ss.fly[f].y1 = (int16_t)(BOOG_Y + 22 - (int32_t)k * 2 + ((int32_t)KV(KN_4) - 64) / 5);
                ss.fly[f].size = (uint8_t)(3 + KV(KN_ALG) / 26);
            }
        for (i = 0; i < 4u; i++)
            if (ss.fly[i].on && pu.now - ss.fly[i].t0 >= 110u) {      /* it lands: it sticks */
                uint32_t s2, best = 0, age = 0;
                int32_t x = ss.fly[i].x1, y = ss.fly[i].y1;
                ss.fly[i].on = 0;
                for (s2 = 0; s2 < 18u; s2++) {
                    if (!ss.stuck[s2].on) {
                        best = s2;
                        break;
                    }
                    if (ss.stuck[s2].age >= age) {
                        age = ss.stuck[s2].age;
                        best = s2;
                    }
                }
                ss.stuck[best].on = 1;
                ss.stuck[best].x = (int8_t)(x - BOOG_X);
                ss.stuck[best].y = (int8_t)(y - BOOG_Y);
                ss.stuck[best].size = ss.fly[i].size;
                ss.stuck[best].age = 0;
                ss.hit_y = (int16_t)y;
                pfx_add(PT_RING, x, y, 0, 0, 3, 0xFFFF, 300, 0)->aux = 50;
                if (KV(KN_2) > 70)
                    pfx_burst(PT_SPARK, x, y, 6, 110, 2, RGB(255, 240, 150), 300, 4);
                else
                    pfx_burst(PT_GOO, x, y, 4, 60, 2, goo, 500, 6);
                if ((++ss.count & 3u) == 0u)
                    pop_say(KV(KN_2) > 70 ? "TINK!" : "SPLAT!", x, y - 44, 2, goo);
            }
        for (i = 0; i < 18u; i++)
            if (ss.stuck[i].on) {
                ss.stuck[i].age = (uint16_t)clamp(ss.stuck[i].age + dt, 0, 60000);
                if (ss.stuck[i].age > 1500u + KV(KN_3) * 70u) {       /* it lets go */
                    ss.stuck[i].on = 0;
                    pfx_add(PT_GOO, BOOG_X + ss.stuck[i].x, BOOG_Y + ss.stuck[i].y, 0, 10, ss.stuck[i].size, goo, 700, 9);
                }
            }
        ss.wob_v = (int16_t)(ss.wob_v - ss.wob * dt / 14 - ss.wob_v * dt / 200);
        ss.wob = (int16_t)clamp(ss.wob + ss.wob_v * dt / 16, -1500, 1500);
        if (u->act == UA_IDLE && pu.now - ss.note_ms > 700u)          /* nothing to do: dig for the next one */
            uni_act(u, UA_PICK);
        break;
    }

    case SC_BURP: {
        int32_t sc = 900 - KV(KN_SEL) * 12, t = (int32_t)(pu.now - ss.note_ms), talk = 60 + KV(KN_ALG) * 6;
        int32_t v = KV(KN_1) + ((int32_t)KV(KN_2) - KV(KN_1)) * clamp(t, 0, talk) / talk;   /* the vowel now: wide .. small */
        uint16_t gas = pg_mix(RGB(225, 240, 150), RGB(90, 200, 60), KV(KN_4) * 2u);
        u->sc = (int16_t)sc;
        u->x = (int16_t)(80 - 60 * sc / 256);
        u->y = (int16_t)(132 + 100 * sc / 256);
        u->act = UA_BURP;
        u->amp = (int16_t)(env * (100 - v * 55 / 127) / 100);
        for (k = 0; k < 27u; k++)
            if ((pu.edges >> k) & 1u) {
                static const char L[] = "BURP!";
                ss.note_ms = pu.now;
                pfx_add(PT_CHAR, u->mouth.x - 4, u->mouth.y - 14, pu_rand(-10, 30), -pu_rand(50, 80), k < 10u ? 4 : 3, pg_hue(k * 9u), 1100, 2)
                    ->aux = (uint8_t)L[ss.count++ % 5u];
                pfx_add(PT_RING, u->mouth.x + 8, u->mouth.y, 60, -10, 6, gas, 420, 0)->aux = 90;
                pu_shake(1 + KV(KN_3) / 40);
            }
        ss.emit += (uint32_t)(env * dt * (20 + KV(KN_4)) / 64);
        while (ss.emit > 2600u) {
            ss.emit -= 2600u;
            pfx_add(PT_PUFF, u->mouth.x + 4, u->mouth.y + pu_rand(-6, 6), pu_rand(50, 130), -pu_rand(-10, 50),
                    (uint32_t)pu_rand(5, 10), gas, 650, -1);
        }
        break;
    }

    case SC_PUKE: {
        int32_t on = env > 6 || pu.nheld;
        ss.heave = (int16_t)clamp(ss.heave + (on ? dt * 3 : -dt * 2), 0, 256);
        u->act = UA_PUKE;
        u->amp = ss.heave;
        stream_flow(on, dt * 256 / 90);
        if (trk[0].arp_idx != ss.arp_last) {                          /* the arpeggiator played a note: a blob of its colour */
            ss.arp_last = trk[0].arp_idx;
            if (trk[0].arp_note && on) {
                for (i = 0; i < 7u && ss.chunk[i].on; i++)
                    ;
                ss.chunk[i].on = 1;
                ss.chunk[i].t0 = pu.now;
                ss.chunk[i].hue = (uint8_t)((trk[0].arp_note % 12u) * 21u);
            }
        }
        for (i = 0; i < 8u; i++)
            if (ss.chunk[i].on && pu.now - ss.chunk[i].t0 > 420u) {   /* it lands */
                uint16_t c = pg_hue(ss.chunk[i].hue);
                ss.chunk[i].on = 0;
                pfx_burst(PT_STAR, 212, 204, 3, 90, 5, c, 500, 5);
                pfx_burst(PT_CHUNK, 212, 204, 3, 80, 4, c, 500, 7);
                if ((pu_rnd() & 3u) == 0u)
                    pfx_add(PT_HEART, 212, 200, pu_rand(-30, 30), -70, 5, c, 700, 5);
                ss.puddle = (int16_t)clamp(ss.puddle + 22, 0, 256);
            }
        if (ss.puddle > 0 && (pu.now & 63u) < (uint32_t)dt)
            ss.puddle--;
        break;
    }

    case SC_GHOST: {
        uint32_t any = 0, n = KV(KN_ALG) ? 3u : 1u, j;
        for (k = 0; k < 27u; k++)
            if ((pu.edges >> k) & 1u)
                for (j = 0; j < n; j++) {                             /* a key calls a ghost (a chord: three) */
                    uint32_t best = 0, age = 0;
                    for (i = 0; i < 8u; i++) {
                        if (!ss.gh[i].state) {
                            best = i;
                            break;
                        }
                        if (ss.gh[i].state == 2u && ss.gh[i].age >= age) {
                            age = ss.gh[i].age;
                            best = i;
                        }
                    }
                    ss.gh[best].state = 1;
                    ss.gh[best].key = (uint8_t)k;
                    ss.gh[best].x = (int16_t)(74 + k * 6u + j * 22u - (n > 1u ? 22 : 0));
                    ss.gh[best].y = 210 * 16;
                    ss.gh[best].ty = (int16_t)((148 - (int32_t)((k * 5u + j * 3u) % 7u) * 13) * 16);
                    ss.gh[best].r = (uint8_t)(20 - k / 3u - (j ? 4u : 0u));
                    ss.gh[best].age = 0;
                    pfx_burst(PT_STAR, ss.gh[best].x, 200, 4, 60, 4, RGB(170, 240, 255), 400, 0);
                }
        for (i = 0; i < 8u; i++) {
            if (!ss.gh[i].state)
                continue;
            if (ss.gh[i].state == 1u) {
                any = 1;
                ss.gh[i].y = (int16_t)(ss.gh[i].y + (ss.gh[i].ty - ss.gh[i].y) * (dt > 50 ? 50 : dt) / (50 + KV(KN_1) * 4));
                if (!((pu.notes >> ss.gh[i].key) & 1u)) {
                    ss.gh[i].state = 2;
                    ss.gh[i].age = 0;
                }
            } else {                                                  /* let go: it drifts up and thins out */
                ss.gh[i].y = (int16_t)(ss.gh[i].y - dt * 3 / 2);
                ss.gh[i].age = (uint16_t)(ss.gh[i].age + dt);
                if (ss.gh[i].age > 250u + KV(KN_2) * 12u || ss.gh[i].y < -30 * 16) {
                    ss.gh[i].state = 0;
                    pfx_burst(PT_STAR, ss.gh[i].x, ss.gh[i].y / 16, 5, 50, 4, 0xFFFF, 400, 0);
                }
            }
        }
        if (any) {
            u->act = UA_ZAP;
            u->amp = (int16_t)env;
        } else if (u->act == UA_ZAP) {
            u->act = UA_IDLE;
        }
        break;
    }
    default:
        break;
    }
}

/* ------------------------------------------------------------------ draw */
/* a stream along an arc, from s0 to s1 of it (0 .. 256), wiggling as a waveform; wave B < 0: A alone; mix 0 .. 127;
 * vert: the stream falls straight (the wiggle goes sideways) */
static void stream_draw(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t h, int32_t s0, int32_t s1, int32_t wa, int32_t wb,
                        int32_t ratio, int32_t mix, int32_t amp, int32_t w, uint16_t c, int32_t vert)
{
    int32_t i, px = 0, py = 0, have = 0, cyc = 3 * 1024;
    if (s1 <= s0)
        return;
    for (i = 0; i <= 28; i++) {
        int32_t s = i * 256 / 28, x, y, ph = s * cyc / 256 - (int32_t)uni_now * 6, d;
        if (s < s0 || s > s1) {
            have = 0;
            continue;
        }
        arc_pt(x0, y0, x1, y1, h, s, &x, &y);
        d = wb < 0 ? pee_wave(ph, wa) : (pee_wave(ph, wa) * (127 - mix) + pee_wave(ph * ratio / 16, wb) * mix) / 127;
        d = d * amp / 256;
        if (i < 3)
            d = d * i / 3;                                            /* (it leaves the unicorn in one place) */
        if (vert)
            x += d;
        else
            y += d;
        if (have)
            pg_line(px, py, x, y, w, c);
        px = x; py = y;
        have = 1;
    }
}

static void pot_draw(int32_t x, int32_t y, uint16_t liq, int32_t wa, int32_t wb, int32_t ratio, int32_t mix)
{
    int32_t i, px = 0, py = 0;
    pg_ellipse(x, y + 14, 34, 20, RGB(20, 12, 30));
    pg_ellipse(x, y + 14, 31, 17, RGB(52, 44, 66));
    pg_rect(x - 26, y + 30, 8, 6, RGB(20, 12, 30));
    pg_rect(x + 18, y + 30, 8, 6, RGB(20, 12, 30));
    pg_ellipse(x, y, 34, 8, RGB(20, 12, 30));
    pg_ellipse(x, y, 30, 5, liq);
    for (i = 0; i <= 20; i++) {                                       /* the sum of the two waves, on the surface */
        int32_t ph = i * 2048 / 20 - (int32_t)uni_now * 6, d;
        int32_t xx = x - 28 + i * 56 / 20, yy;
        d = wb < 0 ? pee_wave(ph, wa) : (pee_wave(ph, wa) * (127 - mix) + pee_wave(ph * ratio / 16, wb) * mix) / 127;
        yy = y - 12 + d * (3 + pu.env / 20) / 256;
        if (i)
            pg_line(px, py, xx, yy, 2, 0xFFFF);
        px = xx; py = yy;
    }
}

static void boog_target(void)
{
    int32_t w = ss.wob / 16, i, x = BOOG_X, y = BOOG_Y;
    uint16_t ink = RGB(20, 12, 30);
    switch (KV(KN_PRE) % 5u) {
    case 0: {                                                         /* a bell on a rope, swinging */
        int32_t bx = x + w * 2 / 3, cx2 = x + w * 5 / 4;
        pg_line(x, 0, x + w / 6, 76, 3, RGB(120, 90, 60));
        pg_circle(x + w / 6, 96, 26, ink);
        pg_quad(x + w / 6 - 26, 96, x + w / 6 + 26, 96, bx + 40, 168, bx - 40, 168, ink);
        pg_circle(cx2, 176, 9, ink);
        pg_circle(x + w / 6, 96, 23, RGB(120, 170, 120));
        pg_quad(x + w / 6 - 23, 96, x + w / 6 + 23, 96, bx + 36, 165, bx - 36, 165, RGB(120, 170, 120));
        pg_quad(x + w / 6 - 23, 96, x + w / 6 - 8, 96, bx - 22, 165, bx - 36, 165, RGB(80, 125, 100));
        pg_line(bx - 38, 166, bx + 38, 166, 5, RGB(200, 220, 150));
        pg_circle(cx2, 176, 7, RGB(200, 220, 150));
        break;
    }
    case 1:                                                           /* ribs between two posts */
        pg_rect(x - 36, 70, 6, 118, ink);
        pg_rect(x + 30, 70, 6, 118, ink);
        for (i = 0; i < 7; i++) {
            int32_t by = 84 + i * 14, hw = 30 - i * 3, dy = (by - ss.hit_y < 9 && ss.hit_y - by < 9) ? w / 3 : w / 12;
            pg_capsule(x - hw, by + dy, 5, x + hw, by + dy, 5, ink);
            pg_capsule(x - hw, by + dy, 3, x + hw, by + dy, 3, RGB(240, 235, 215));
            pg_circle(x - hw, by + dy, 4, RGB(240, 235, 215));
            pg_circle(x + hw, by + dy, 4, RGB(240, 235, 215));
        }
        break;
    case 2:                                                           /* a web, and who lives in it */
        for (i = 0; i < 8; i++) {
            int32_t a = i * 128, a2 = a + 128, r;
            pg_line(x, y, x + (pg_cos(a) * 54 >> 15), y + (pg_sin(a) * 54 >> 15), 1, RGB(210, 210, 230));
            for (r = 16; r <= 48; r += 16) {
                int32_t rr = r + (w * (i & 1 ? 1 : -1) * r >> 9);
                pg_line(x + (pg_cos(a) * rr >> 15), y + (pg_sin(a) * rr >> 15), x + (pg_cos(a2) * rr >> 15), y + (pg_sin(a2) * rr >> 15),
                        1, RGB(210, 210, 230));
            }
        }
        for (i = 0; i < 4; i++) {
            pg_line(x + 24, y - 30 + w / 4, x + 12, y - 40 + i * 6 + w / 4, 1, ink);
            pg_line(x + 24, y - 30 + w / 4, x + 36, y - 40 + i * 6 + w / 4, 1, ink);
        }
        pg_circle(x + 24, y - 30 + w / 4, 6, ink);
        pg_rect(x + 21, y - 32 + w / 4, 2, 2, RGB(255, 60, 60));
        pg_rect(x + 26, y - 32 + w / 4, 2, 2, RGB(255, 60, 60));
        break;
    case 3:                                                           /* a cauldron, played as a drum */
        pg_ellipse(x, y + 30, 46, 34, ink);
        pg_ellipse(x, y + 30, 43, 31, RGB(48, 42, 62));
        pg_ellipse(x - 14, y + 34, 12, 20, RGB(70, 62, 88));
        pg_rect(x - 34, y + 58, 10, 10, ink);
        pg_rect(x + 24, y + 58, 10, 10, ink);
        pg_ellipse(x, y, 48, 11, ink);
        pg_ellipse(x, y, 43, 7 + (w > 0 ? w : -w) / 30, RGB(110, 220, 60));
        pg_circle(x - 16 + w / 8, y - 2, 4, RGB(190, 250, 120));
        pg_circle(x + 14 - w / 8, y + 1, 3, RGB(190, 250, 120));
        break;
    default: {                                                        /* crystals that light up */
        static const int8_t CX[3] = {-22, 0, 22}, CH[3] = {56, 82, 46}, CL[3] = {-8, 2, 10};
        static const uint16_t CC[3] = {RGB(230, 110, 210), RGB(90, 210, 240), RGB(150, 110, 240)};
        for (i = 0; i < 3; i++) {
            int32_t bx = x + CX[i], tx = bx + CL[i] + w / 6, ty = 176 - CH[i];
            pg_quad(bx - 12, 178, tx - 12, ty + 12, tx, ty - 6, bx + 12, 178, ink);
            pg_quad(bx + 12, 178, tx, ty - 6, tx + 12, ty + 12, bx + 12, 178, ink);
            pg_quad(bx - 9, 176, tx - 9, ty + 12, tx, ty, bx + 2, 176, CC[i]);
            pg_quad(bx + 2, 176, tx, ty, tx + 9, ty + 12, bx + 9, 176, pg_mix(CC[i], 0, 80));
        }
        if (w > 12 || w < -12) {
            pg.blend = PB_ADD;
            pg_circle(x, 130, 30 + (w > 0 ? w : -w) / 3, RGB(30, 40, 60));
            pg.blend = PB_SOLID;
        }
        break;
    }
    }
}

static void scene_draw(void)
{
    uni_t *u = &pu.u[0];
    int32_t i, j, x, y;
    switch (pu.scene) {
    case SC_WALK: {
        track_t *t = &trk[3];
        int32_t lane = KV(KN_SEL) % NLANE, o;
        uint16_t street = RGB(30, 26, 42);
        pg_rect(0, 100, 240, 140, street);
        pg_rect(0, 98, 240, 3, RGB(12, 8, 18));
        for (o = -1; o <= 2; o++) {
            int32_t l = (lane + o + NLANE) % NLANE, y0 = 124 + o * 48 + ss.slide / 16, cur = o == 0;
            if (y0 < 100 || y0 > 236 || pg_out(0, y0 - 3, 240, y0 + 44))
                continue;
            pg_rect(0, y0 - 2, 240, 44, RGB(12, 8, 18));
            for (i = 0; i < 16; i++) {
                int32_t on = (t->step[i].hit >> l) & 1;
                uint16_t c = on ? LANE_COL[l] : pg_mix(LANE_COL[l], 0, (i & 3) ? 200 : 170);
                if (!cur)
                    c = pg_mix(c, street, 120);
                if (song.playing && i == ss.w_idx && on)
                    c = pg_mix(c, 0xFFFF, ss.w_flash[l] / 2u);
                pg_rect(i * 15 + 1, y0, 13, 40, c);
                if (on) {
                    int32_t r = cur ? 5 : 3;
                    if (song.playing && i == ss.w_idx)
                        r += ss.w_flash[l] / 60;
                    pg_circle(i * 15 + 7, y0 + 20, r, pg_mix(c, 0xFFFF, 170));
                    pg_circle(i * 15 + 7, y0 + 20, r - 2, c);
                } else if (cur) {
                    pg_rect(i * 15 + 4, y0 + 34, 7, 2, pg_mix(c, 0, 90));        /* a crack in the tile */
                }
            }
            if (song.playing && ss.w_idx < 16u) {                    /* the playing column */
                pg_rect(ss.w_idx * 15, y0 - 2, 15, 2, 0xFFFF);
                pg_rect(ss.w_idx * 15, y0 + 40, 15, 2, 0xFFFF);
            }
        }
        pg_text_c(120, 104, ROAD_NAME[lane], 2, LANE_COL[lane], RGB(12, 8, 18));
        uni_draw(u);
        break;
    }

    case SC_PEE: {
        uint32_t view = KV(KN_PRE);
        uni_t *b = &pu.u[1];
        int32_t wa = KV(KN_1), wb = KV(KN_2), ratio = PUNI_RATIO[KV(KN_3) & 7u], mix = KV(KN_4);
        int32_t h = ss.head, tl = ss.tail, murk = (127 - KV(KN_SEL)) / 32;
        uint16_t ca = RGB(250, 225, 50), cb = RGB(170, 240, 60), cm = pg_mix(ca, cb, (uint32_t)mix * 2u);
        if (murk > 0) {                                               /* TONE down: the night closes in */
            pg.blend = murk > 2 ? PB_DARK : PB_STIP;
            pg_rect(0, 0, 240, PU_GY, th->sil);
            pg.blend = PB_SOLID;
        }
        if (view == 0u) {
            pot_draw(120, 206, cm, wa, wb, ratio, mix);
            uni_draw(u);
            uni_draw(b);
            stream_draw(u->pee.x, u->pee.y, 120, 150, 34, tl > 256 ? 256 : tl, h > 256 ? 256 : h, wa, -1, 16, 0, 5, 2 + (127 - mix) / 40, ca, 0);
            stream_draw(b->pee.x, b->pee.y, 120, 150, 34 * b->sc / 150, tl > 256 ? 256 : tl, h > 256 ? 256 : h, wb, -1, 16, 0,
                        5 * b->sc / 150 + 1, 2 + mix / 40, cb, 0);
            if (h > 256)                                              /* where they meet they add up */
                stream_draw(120, 150, 120, 200, 0, tl > 256 ? tl - 256 : 0, h - 256, wa, wb, ratio, mix, 8, 5, cm, 1);
        } else {
            uni_t *q = view == 1u ? u : b;
            int32_t lx = view == 1u ? 206 : 34;
            pot_draw(lx, 210, view == 1u ? ca : cb, view == 1u ? wa : wb, -1, 16, 0);
            uni_draw(q);
            stream_draw(q->pee.x, q->pee.y, lx, 204, 60, tl * 256 / (h > 0 ? (h > 256 ? 256 : h) : 1) * (h > 256 ? 256 : h) / 256,
                        h > 256 ? 256 : h, view == 1u ? wa : wb, -1, 16, 0, 12, 5, view == 1u ? ca : cb, 0);
        }
        break;
    }

    case SC_BOOG: {
        uint16_t goo = pg_mix(RGB(150, 240, 60), RGB(100, 105, 40), KV(KN_2) * 2u);
        boog_target();
        for (i = 0; i < 18; i++)
            if (ss.stuck[i].on) {
                int32_t drip = ss.stuck[i].age / 50;
                if (drip > 4 + KV(KN_3) / 8)
                    drip = 4 + KV(KN_3) / 8;
                x = BOOG_X + ss.stuck[i].x + ss.wob * (ss.stuck[i].y + 40) / 1400;
                y = BOOG_Y + ss.stuck[i].y;
                pg_capsule(x, y, ss.stuck[i].size + 1, x, y + drip, ss.stuck[i].size / 2 + 1, RGB(20, 12, 30));
                pg_capsule(x, y, ss.stuck[i].size, x, y + drip, ss.stuck[i].size / 2, goo);
                pg_rect(x - ss.stuck[i].size / 2, y - ss.stuck[i].size / 2, 2, 2, 0xFFFF);
            }
        uni_draw(u);
        if (u->act == UA_PICK)                                        /* the one on the hoof */
            pg_circle(u->foot[3].x, u->foot[3].y - 4, 3, goo);
        for (i = 0; i < 4; i++)
            if (ss.fly[i].on) {
                int32_t f = (int32_t)(pu.now - ss.fly[i].t0) * 256 / 110;
                arc_pt(ss.fly[i].x0, ss.fly[i].y0, ss.fly[i].x1, ss.fly[i].y1, 18, f > 256 ? 256 : f, &x, &y);
                pg_capsule(x, y, ss.fly[i].size, x - 12, y + 2, 1, goo);
            }
        break;
    }

    case SC_PUKE: {
        int32_t mx = u->mouth.x + 2, my = u->mouth.y, lx = 212, ly = 206, arch = 46 + KV(KN_2) * 16, px[5], py[5], have = 0;
        int32_t s0 = ss.tail / 2, s1 = ss.head / 2;
        pg_ellipse(lx, ly + 4, 12 + ss.puddle / 7, 4 + ss.puddle / 40, RGB(230, 70, 160));
        pg_ellipse(lx, ly + 4, 8 + ss.puddle / 9, 3 + ss.puddle / 60, RGB(250, 220, 60));
        pg_ellipse(lx, ly + 4, 4 + ss.puddle / 14, 1 + ss.puddle / 90, RGB(70, 210, 230));
        uni_draw(u);
        if (KV(KN_4) > 40 && s1 > s0) {                               /* ECHO: a ghost of the rainbow behind it */
            pg.blend = PB_STIP;
            stream_draw(mx - 8, my + 10, lx - 12, ly, arch, s0, s1, 0, -1, 16, 0, 0, 12, pg_hue(uni_now / 5u), 0);
            pg.blend = PB_SOLID;
        }
        for (i = 0; i <= 16; i++) {
            int32_t s = i * 256 / 16;
            if (s < s0 || s > s1) {
                have = 0;
                continue;
            }
            arc_pt(mx, my, lx, ly, arch, s, &x, &y);
            for (j = 0; j < 5; j++) {
                int32_t yy = y + (j - 2) * 4 + (pg_sin((int32_t)uni_now * 8 + i * 90) * 2 >> 15);
                if (have)
                    pg_line(px[j], py[j], x, yy, 5, pg_hue((uint32_t)(j * 44) + uni_now / 6u));
                px[j] = x; py[j] = yy;
            }
            have = 1;
        }
        for (i = 0; i < 8; i++)
            if (ss.chunk[i].on) {                                     /* the notes, on their way */
                int32_t s = (int32_t)(pu.now - ss.chunk[i].t0) * 256 / 420, r = 6 + KV(KN_ALG) / 18;
                arc_pt(mx, my, lx, ly, arch, s > 256 ? 256 : s, &x, &y);
                pg_circle(x, y, r + 2, RGB(20, 12, 30));
                pg_circle(x, y, r, pg_hue(ss.chunk[i].hue));
                pg_circle(x - r / 3, y - r / 3, r / 3, 0xFFFF);
            }
        break;
    }

    case SC_GHOST: {
        int32_t wob = KV(KN_3);
        uni_draw(u);
        pg.blend = PB_ADD;
        pg_circle(u->horn1.x, u->horn1.y, 9 + pu.env / 14, RGB(30, 60, 80));
        pg.blend = PB_SOLID;
        for (i = 0; i < 8; i++) {
            int32_t r, gx, gy, held, mo;
            uint16_t c;
            if (!ss.gh[i].state)
                continue;
            held = ss.gh[i].state == 1u;
            r = ss.gh[i].r;
            gx = ss.gh[i].x + (pg_sin((int32_t)uni_now * 5 + i * 200) * (2 + wob / 5) >> 15);
            gy = ss.gh[i].y / 16 + (pg_sin((int32_t)uni_now * 2 + i * 300) * 3 >> 15);
            if (pg_out(gx - r - 60, 0, gx + r + 60, 240))
                continue;
            if (held) {                                               /* the horn's lightning holds it */
                int32_t ax = u->horn1.x, ay = u->horn1.y;
                for (j = 1; j <= 4; j++) {
                    uint32_t hh = pg_hash(uni_now / 60u + (uint32_t)(i * 16 + j));
                    int32_t bx = u->horn1.x + (gx - u->horn1.x) * j / 4 + (j < 4 ? (int32_t)(hh & 15u) - 8 : 0);
                    int32_t by = u->horn1.y + (gy - u->horn1.y) * j / 4 + (j < 4 ? (int32_t)((hh >> 4) & 15u) - 8 : 0);
                    pg_line(ax, ay, bx, by, 2, (hh >> 9) & 1u ? RGB(170, 240, 255) : 0xFFFF);
                    ax = bx; ay = by;
                }
                pg.blend = PB_ADD;
                pg_circle(gx, gy, r + 9, RGB(0, 34, 44));
            } else {
                pg.blend = ss.gh[i].age > 200u ? PB_STIP : PB_HALF;
            }
            c = pg_mix(RGB(225, 250, 255), th->acc, 40);
            if (held)
                pg.blend = PB_SOLID;
            pg_circle(gx, gy, r, c);
            pg_rect(gx - r, gy, 2 * r + 1, r, c);
            for (j = -1; j <= 1; j++)
                pg_circle(gx + j * (r * 2 / 3) + (pg_sin((int32_t)uni_now * 6 + j * 300) * 2 >> 15), gy + r, r / 3 + 1, c);
            pg.blend = PB_SOLID;
            mo = held ? 2 + pu.env * r / 700 : 2;
            pg_ellipse(gx - r / 3, gy - r / 5, r / 6 + 1, r / 4 + 1, RGB(20, 12, 40));
            pg_ellipse(gx + r / 3, gy - r / 5, r / 6 + 1, r / 4 + 1, RGB(20, 12, 40));
            pg_ellipse(gx, gy + r / 3, r / 4 + 1, mo, RGB(20, 12, 40));
        }
        break;
    }

    default:                                                          /* DRESS UP, FART BASS, BURP BOX: the unicorn */
        uni_draw(u);
        break;
    }
}
