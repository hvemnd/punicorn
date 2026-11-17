/* SPDX-License-Identifier: GPL-3.0-only */
/* PUNICORN: the FM-1 as a loud, rude, dark-fantasy music toy for a small child. It takes the place of Rainbow
 * mode (kid.c) on top of Felucca: on from power-up, HOME and SAVE held together for 3 s leave it for the full
 * Felucca until the next power-up.
 *
 * Every function button is a scene, a toy with a unicorn in it and a kind of synthesizer behind it:
 *   HOME  DRESS UP      the unicorn and its world. KNOB 1..4: colour, clothes, horn, hat. SELECT: the world,
 *                       ALGORITHM: its weather, PRESETS: what the keys play. Kept over power-off.
 *   FX    FART BASS     an 808 bass drum on every key, and the fart that goes with it (eng_puni.c FART). The
 *                       cloud is the sound's envelope: as loud, as long and as rude as what is heard.
 *   SCL   BEAT STREET   a 16-step drum sequencer: a sidewalk of 16 tiles, a road a drum (SELECT turns the roads
 *                       through, 8 of them). The white keys are the tiles, the black keys play the drums.
 *   ENV   PEE WAVES     an additive synth: two unicorns, a waveform each, and where the streams meet they add up
 *                       (eng_puni.c PEE). PRESETS: both, or one alone and close up.
 *   LFO   BOOGER BELLS  physical modelling (Felucca's PHYS): a booger is the striker, the thing it hits is the
 *                       resonator. They stick, and drip.
 *   ARP   BURP BOX      a formant voice (Felucca's VOICE): a close-up of a face that burps vowels.
 *   EDIT  RAINBOW BARF  an arpeggiator: held keys come out as a rainbow, a blob a note.
 *   GLO   GHOST GLOW    a choir pad with slow envelopes: every key calls a ghost, the horn holds them.
 * Everywhere: PLAY the beat of BEAT STREET on and off, SEQ the next ready-made beat, REC the next world, SAVE a
 * surprise outfit, OCT- / OCT+ the octave, MASTER the volume (never above the grown-ups' maximum: HOME held 1 s,
 * then OCT- / OCT+). After 45 s of nothing the unicorn falls asleep.
 *
 * The parts: puni_gfx.c (drawing without a frame buffer), puni_uni.c (the unicorn), puni_fx.c (particles and
 * worlds), puni_scenes.c (the eight scenes), eng_puni.c (the FART and PEE sounds). */
#ifndef FELUCCA_KID
#define FELUCCA_KID 1
#endif
#if FELUCCA_KID
#include "puni_gfx.c"
#include "puni_uni.c"
#include "puni_fx.c"

enum { SC_HOME, SC_FART, SC_WALK, SC_PEE, SC_BOOG, SC_BURP, SC_PUKE, SC_GHOST, SC_COUNT };
static const uint8_t SC_BTN[SC_COUNT] = {B_HOME, B_FX, B_SCL, B_ENV, B_LFO, B_ARP, B_EDIT, B_GLO};
static const char *const SC_NAME[SC_COUNT] = {"DRESS UP", "FART BASS", "BEAT STREET", "PEE WAVES", "BOOGER BELLS", "BURP BOX",
                                              "RAINBOW BARF", "GHOST GLOW"};
enum { KN_1, KN_2, KN_3, KN_4, KN_SEL, KN_ALG, KN_PRE, KN_COUNT };
static const uint8_t KN_ROLE[KN_COUNT] = {EN_K1, EN_K2, EN_K3, EN_K4, EN_SELECT, EN_ALGO, EN_PRESET};
/* a scene's knob: its name, its top value, where it starts; opt: it steps through max + 1 named things and wraps */
typedef struct { const char *name; uint8_t max, def, opt; } kdef_t;
static const kdef_t KD[SC_COUNT][KN_COUNT] = {
    {{"COLOR", UNI_NCOL - 1, 0, 1}, {"CLOTHES", UC_COUNT - 1, 0, 1}, {"HORN", UH_COUNT - 1, 0, 1}, {"HAT", UT_COUNT - 1, 0, 1},
     {"WORLD", PU_NTHEME - 1, 0, 1}, {"WEATHER", 8, 0, 1}, {"SOUND", 5, 0, 1}},
    {{"LENGTH", 127, 70, 0}, {"DROP", 127, 70, 0}, {"FLAP", 127, 50, 0}, {"STINK", 127, 30, 0},
     {"TONE", 127, 90, 0}, {"FLUTTER", 127, 40, 0}, {"FLAVOR", 4, 0, 1}},
    {{"TEMPO", 127, 55, 0}, {"SWING", 127, 0, 0}, {"KIT", 5, 0, 1}, {"TUNE", 127, 64, 0},
     {"ROAD", NLANE - 1, 0, 1}, {"DECAY", 127, 64, 0}, {"BEAT", 7, 1, 1}},
    {{"WAVE A", 127, 0, 0}, {"WAVE B", 127, 90, 0}, {"HARMONIC", 7, 3, 1}, {"MIX", 127, 56, 0},
     {"TONE", 127, 100, 0}, {"SLOW", 127, 16, 0}, {"VIEW", 2, 0, 1}},
    {{"SHAPE", 127, 64, 0}, {"CRUST", 127, 80, 0}, {"STICKY", 127, 80, 0}, {"AIM", 127, 30, 0},
     {"CAVE", 127, 44, 0}, {"SIZE", 127, 90, 0}, {"TARGET", 4, 0, 1}},
    {{"MOUTH", 127, 10, 0}, {"MOUTH 2", 127, 70, 0}, {"GROWL", 127, 70, 0}, {"GAS", 127, 40, 0},
     {"SIZE", 24, 12, 0}, {"TALK", 127, 50, 0}, {"VOICE", 3, 0, 1}},
    {{"SPEED", 3, 2, 1}, {"RANGE", 3, 1, 1}, {"ORDER", 3, 0, 1}, {"ECHO", 127, 60, 0},
     {"SOUND", 4, 0, 1}, {"CHUNKS", 127, 64, 0}, {"LATCH", 1, 0, 1}},
    {{"RISE", 127, 60, 0}, {"LINGER", 127, 92, 0}, {"WOBBLE", 63, 8, 0}, {"CAVE", 127, 90, 0},
     {"CHOIR", 3, 0, 1}, {"CHORD", 3, 0, 1}, {"BRIGHT", 127, 70, 0}},
};

#define PU_VOL_BYTE (favorites.factory[15][26])   /* the grown-ups' maximum, as Rainbow mode kept it (settings_persist.c) */
#define PU_LOOK (favorites.factory[14])           /* the unicorn's look and world: 0xA7, 7 values, a check byte */
#define PU_VOL_DEF 6u
static const uint16_t PU_VOL_CAP[8] = {362, 512, 724, 1024, 1448, 2048, 2896, 4096};

static struct {
    uint8_t on, ready, scene, asleep;
    uint8_t k[SC_COUNT][KN_COUNT];
    uint32_t now, dt, last, idle_ms, scene_ms, splash_ms, save_ms, home_ms, exit_ms;
    uint32_t notes, edges;               /* the keys held, and those that went down this frame */
    int16_t env;                         /* how loud track 1 is, 0 .. 256 */
    int16_t shake;
    int8_t oct, key;                     /* the octave; the last key that went down */
    uint8_t nheld, vol_on, home_used, look_dirty;
    uint8_t hint_k, hint_on;
    uint32_t hint_ms;
    uni_t u[2];
} pu = {1};

#define KV(n_) (pu.k[pu.scene][n_])

static void settings_poll(void);                 /* (project.c) */
static int autosave_boot(int allowed);
static void pu_scene(uint32_t s);

/* ------------------------------------------------------------------ sound */
static void pu_common(track_t *t)
{
    t->p[P_QUANT] = QN_SNAP;                      /* every key lands in the scale: C major pentatonic, nothing sounds wrong */
    t->p[P_SCALE] = 5;
    t->p[P_ROOT] = 0;
    t->p[P_TRANS] = 0;
    t->p[P_AMODE] = 0;
    t->p[P_AHOLD] = 0;
    t->p[P_CHRD] = CH_OFF;
    t->p[P_MUTE] = 0;
    t->p[P_PAN] = 0;
    t->p[P_GLIDE] = 0;
}
static void pu_sound(track_t *t, uint32_t eng, const char *preset)
{
    const engine_t *e = ENGINES[eng % NENGINES];
    uint32_t i, p = 0;
    for (i = 0; i < e->npresets; i++)
        if (str_eq(e->presets[preset_orig(e, i)].name, preset)) {
            p = i;
            break;
        }
    if (t->eng_req != eng)
        set_engine_of(t, eng);
    apply_preset_to(t, p);
    pu_common(t);
}
/* Punicorn's own engine (eng_puni.c), mode 0 FART, 1 PEE: set by hand (the full Felucca's loaders turn engine 1 into FM6) */
static void pu_puni(track_t *t, uint32_t mode)
{
    uint32_t i;
    fm1_irq_off();
    t->eng_req = 1;
    for (i = 0; i < P_E0; i++)
        t->p[i] = TP[i].def;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = ENG_PUNI.edit[i].def;
    t->p[P_E0] = (int16_t)mode;
    t->p[P_VOICE] = V_POLY;
    t->user = 0;
    t->preset = 0;
    panic_req |= (uint8_t)(1u << (uint32_t)(t - trk));
    fm1_irq_on();
    pu_common(t);
}

/* the ready-made beats of BEAT STREET: a lane's steps as bits (bit 0 the first step); lanes KICK SNARE CLAP HAT
 * OPEN TOM RIM BELL */
static const uint16_t PU_BEATS[8][NLANE] = {
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0x1111, 0x1010, 0, 0x4444, 0, 0, 0, 0},                       /* BOOM */
    {0x0501, 0x1010, 0, 0x5555, 0, 0, 0, 0},                       /* ROCK */
    {0x0581, 0x1010, 0, 0x1555, 0x4000, 0, 0, 0},                  /* HIP HOP */
    {0x1111, 0, 0x1010, 0xAAAA, 0x4444, 0, 0, 0},                  /* DISCO */
    {0x0141, 0, 0, 0, 0, 0xE000, 0x0808, 0x0401},                  /* SPOOKY */
    {0x0909, 0x1010, 0x1000, 0xFFFF, 0, 0, 0, 0},                  /* TRAP */
    {0x1111, 0x4C4C, 0, 0, 0, 0x8000, 0, 0},                       /* MARCH */
};
static const char *const PU_BEAT_NAME[8] = {"EMPTY", "BOOM", "ROCK", "HIP HOP", "DISCO", "SPOOKY", "TRAP", "MARCH"};
static const uint8_t PU_BEAT_BPM[8] = {110, 110, 120, 90, 122, 96, 140, 112};
static void pu_beat_load(uint32_t b)
{
    track_t *t = &trk[3];
    uint32_t i, l;
    track_defaults_steps(t);
    t->p[P_SLEN] = 16;
    for (l = 0; l < NLANE; l++)
        for (i = 0; i < 16u; i++)
            if ((PU_BEATS[b % 8u][l] >> i) & 1u)
                grid_hit(t, i, l, 1);
    if (b % 8u)
        pu.k[SC_WALK][KN_1] = (uint8_t)(PU_BEAT_BPM[b % 8u] - 55u);
}

/* --------------------------------------------------------------- the look */
static void pu_look_save(void)
{
    uint8_t *b = PU_LOOK, s = 0x5A;
    uint32_t i;
    b[0] = 0xA7;
    for (i = 0; i < 7u; i++) {
        b[1u + i] = pu.k[SC_HOME][i];
        s = (uint8_t)(s * 31u + b[1u + i]);
    }
    b[8] = s;
}
static void pu_look_load(void)
{
    const uint8_t *b = PU_LOOK;
    uint8_t s = 0x5A;
    uint32_t i, ok = b[0] == 0xA7;
    for (i = 0; i < 7u; i++) {
        s = (uint8_t)(s * 31u + b[1u + i]);
        ok = ok && b[1u + i] <= KD[SC_HOME][i].max;
    }
    if (ok && s == b[8])
        for (i = 0; i < 7u; i++)
            pu.k[SC_HOME][i] = b[1u + i];
}
static void pu_look_apply(void)
{
    uint32_t i;
    for (i = 0; i < 2u; i++) {
        pu.u[i].col = pu.k[SC_HOME][KN_1];
        pu.u[i].cloth = pu.k[SC_HOME][KN_2];
        pu.u[i].horn = pu.k[SC_HOME][KN_3];
        pu.u[i].hat = pu.k[SC_HOME][KN_4];
    }
    pu.u[1].col = (uint8_t)((pu.u[0].col + 4u) % UNI_NCOL);   /* the second unicorn: the same wardrobe, another coat */
    th = &THEMES[pu.k[SC_HOME][KN_SEL] % PU_NTHEME];
    th_amb = pu.k[SC_HOME][KN_ALG] ? (uint8_t)(1u << (pu.k[SC_HOME][KN_ALG] - 1u)) : th->amb;
}

static uint32_t pu_vol_level(void)
{
    uint32_t l = ((uint32_t)PU_VOL_BYTE & 0x7Fu) ^ 6u;
    return l >= 1u && l <= 8u ? l : PU_VOL_DEF;
}
/* every main-loop pass (main.c): MASTER at most the grown-ups' maximum, and the limiter's ceiling just over it */
static void kid_master(void)
{
    int32_t c;
    if (!pu.on) {
        lim_t = LIM_T;
        return;
    }
    c = PU_VOL_CAP[pu_vol_level() - 1u];
    if (song.master_q12 > (uint32_t)c)
        song.master_q12 = (uint32_t)c;
    c = (int32_t)song.master_q12 * 15;
    lim_t = c < 400 ? 400 : c > LIM_T ? LIM_T : c;
}

static void pu_shake(int32_t s)
{
    if (s > pu.shake)
        pu.shake = (int16_t)(s > 9 ? 9 : s);
}
static void pu_hint(uint32_t k)
{
    pu.hint_k = (uint8_t)k;
    pu.hint_on = 1;
    pu.hint_ms = pu.now;
}

#include "puni_scenes.c"

/* ------------------------------------------------------------------ input */
static void pu_enter(void)
{
    uint32_t s, k;
    fx_warm = 1;                                  /* (fx.c: the highs softened for small ears and a small speaker) */
    led_pos_init();
    for (s = 0; s < SC_COUNT; s++)
        for (k = 0; k < KN_COUNT; k++)
            pu.k[s][k] = KD[s][k].def;
    pu_look_load();
    pu.ready = 1;
    pu.key = 13;
    pu.now = pu.last = fm1_ms;
    uni_now = fm1_ms;
    if (trk[3].eng_req != ENGI_DRUM)
        set_engine_of(&trk[3], ENGI_DRUM);
    trk[3].p[P_LEVEL] = 96;
    trk[3].p[P_MUTE] = 0;
    pu_beat_load(1);
    trk[1].p[P_MUTE] = trk[2].p[P_MUTE] = 1;
    song.g[G_SWING] = 0;
    pu_look_apply();
    pu_scene(SC_HOME);
    pu.splash_ms = PU_NZ(fm1_ms);
    pu.idle_ms = fm1_ms;
}

static void pu_leave(void)
{
    uint32_t i;
    fx_warm = 0;
    if (pu.look_dirty) {
        pu_look_save();
        settings_save();
    }
    pu.on = 0;
    song.grid = 0;
    song.sel = 0;
    song.octave = 0;
    if (song.playing)
        transport_req = 2;
    for (i = 0; i < NTRK; i++) {                  /* the parts as at power-on (the autosave, if there is one, comes over them) */
        set_engine_of(&trk[i], TRK_DEF[i][0]);
        apply_preset_to(&trk[i], TRK_DEF[i][1]);
        trk[i].p[P_MUTE] = 0;
        track_defaults_steps(&trk[i]);
    }
    song.g[G_BPM] = GP[G_BPM].def;
    autosave_boot(1);
    lcd_fill(0, 0, 240, 240, T_BG);
    ui.home = 1;
    ui.force = 1;
}

static void pu_wake(void)
{
    pu.idle_ms = pu.now;
    if (pu.asleep) {
        pu.asleep = 0;
        uni_act(&pu.u[0], UA_WOW);
    }
}

static void pu_input(void)
{
    uint32_t released = 0, pressed = fm1_input_edges(&released), held = fm1_in.buttons, id, k, s;
    uint32_t home = 1u << panel.btn[B_HOME], save = 1u << panel.btn[B_SAVE];
    fm6_poll();
    perf_kill = 0;
    perf_latch_on = 0;
    pu.edges = fm1_input_note_edges();
    pu.notes = fm1_in.notes;
    pu.nheld = 0;
    for (k = 0; k < 27u; k++) {
        pu.nheld = (uint8_t)(pu.nheld + ((pu.notes >> k) & 1u));
        if ((pu.edges >> k) & 1u)
            pu.key = (int8_t)k;
    }
    if (pressed | pu.edges)
        pu_wake();
    if (pu.splash_ms && (pressed | pu.edges)) {   /* the hello: any key ends it */
        pu.splash_ms = 0;
        pg.full = 1;
    }
    /* HOME: a tap is DRESS UP; held 1 s the grown-ups' VOLUME (then OCT- / OCT+); with SAVE for 3 s, the full Felucca */
    if (pressed & home) {
        pu.home_ms = PU_NZ(pu.now);
        pu.home_used = 0;
    }
    if ((held & (home | save)) == (home | save)) {
        if (!pu.exit_ms)
            pu.exit_ms = PU_NZ(pu.now);
        else if (pu.now - pu.exit_ms > 3000u) {
            pu_leave();
            return;
        }
        pu.home_used = 1;
    } else {
        pu.exit_ms = 0;
    }
    pu.vol_on = (uint8_t)((held & home) && pu.home_ms && pu.now - pu.home_ms > 1000u && !pu.exit_ms);
    if ((released & home) && pu.home_ms) {
        if (!pu.home_used && pu.now - pu.home_ms <= 1000u)
            pu_scene(SC_HOME);
        pu.home_ms = 0;
        pg.full = 1;
    }
    for (id = 0; id < FM1_NBTN; id++) {
        uint32_t b;
        if (!((pressed >> id) & 1u))
            continue;
        b = panel_btn_of(id);
        for (s = 1; s < SC_COUNT; s++)
            if (b == SC_BTN[s] && !(held & home))
                pu_scene(s);
        switch (b) {
        case B_PLAY:
            transport_req = (uint8_t)(song.playing ? 2 : 1);
            break;
        case B_SEQ: {                             /* the next ready-made beat, playing */
            uint8_t *bt = &pu.k[SC_WALK][KN_PRE];
            *bt = (uint8_t)(*bt % 7u + 1u);
            pu_beat_load(*bt);
            song.g[G_BPM] = (int16_t)(55 + pu.k[SC_WALK][KN_1]);
            if (!song.playing)
                transport_req = 1;
            pop_say(PU_BEAT_NAME[*bt], 120, 70, 3, th->acc);
            break;
        }
        case B_REC:                               /* the next world, with a bang */
            pu.k[SC_HOME][KN_SEL] = (uint8_t)((pu.k[SC_HOME][KN_SEL] + 1u) % PU_NTHEME);
            pu_look_apply();
            pu.look_dirty = 1;
            pg.full = 1;
            pfx_burst(PT_STAR, 120, 110, 26, 90, 6, th->acc, 900, 2);
            pop_say(th->name, 120, 70, 2, th->acc);
            pu_shake(4);
            break;
        case B_SAVE:                              /* a surprise: another unicorn altogether */
            if (held & home)
                break;
            for (k = KN_1; k <= KN_4; k++)
                pu.k[SC_HOME][k] = (uint8_t)(pu_rnd() % (KD[SC_HOME][k].max + 1u));
            pu_look_apply();
            pu.look_dirty = 1;
            uni_act(&pu.u[0], UA_HOP);
            pfx_burst(PT_HEART, pu.u[0].chest.x, pu.u[0].chest.y, 14, 80, 5, RGB(255, 90, 160), 900, 3);
            pfx_burst(PT_STAR, pu.u[0].chest.x, pu.u[0].chest.y, 14, 100, 5, 0xFFE0, 800, 3);
            pop_say("SURPRISE!", 120, 60, 3, 0xFFFF);
            break;
        case B_OCTDN:
        case B_OCTUP:
            if (pu.vol_on) {                      /* the grown-ups' VOLUME */
                int32_t l = clamp((int32_t)pu_vol_level() + (b == B_OCTUP ? 1 : -1), 1, 8);
                PU_VOL_BYTE = (uint8_t)((PU_VOL_BYTE & 0x80u) | ((uint32_t)l ^ 6u));
                pu.look_dirty = 1;
                pu.home_used = 1;
            } else {
                pu.oct = (int8_t)clamp(pu.oct + (b == B_OCTUP ? 1 : -1), -2, 2);
                pop_say(pu.oct > 0 ? "HIGH" : pu.oct < 0 ? "LOW" : "MIDDLE", 120, 70, 3, 0xFFFF);
            }
            break;
        default:
            break;
        }
    }
    for (k = 0; k < KN_COUNT; k++) {              /* the knobs: this scene's */
        int32_t d = panel_enc(KN_ROLE[k]), v;
        const kdef_t *kd = &KD[pu.scene][k];
        if (!d)
            continue;
        pu_wake();
        v = KV(k);
        if (kd->opt) {
            v = (v + (d > 0 ? 1 : (int32_t)kd->max)) % ((int32_t)kd->max + 1);
        } else {
            v = clamp(v + d * (kd->max > 40 ? 5 : 1), 0, kd->max);
        }
        KV(k) = (uint8_t)v;
        pu_hint(k);
        scene_knob(k, d);
    }
    song.octave = (int8_t)(pu.oct + scene_octave());
    song.grid = (uint8_t)(pu.scene == SC_WALK);
    song.sel = (uint8_t)(pu.scene == SC_WALK ? 3 : 0);
    {   /* how loud the unicorn's own track is: the loudest of its voices */
        int32_t e = 0;
        for (k = 0; k < NVOICE; k++)
            if (trk[0].v[k].active && trk[0].v[k].env_out > e)
                e = trk[0].v[k].env_out;
        e >>= 7;
        pu.env = (int16_t)(e > 256 ? 256 : e);
    }
}

static void pu_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0};
    uint32_t k, c, lit = scene_key_leds();
    for (k = 0; k < SC_COUNT; k++)
        led_put(nl, panel.btn[SC_BTN[k]], pu.scene == k);
    for (k = 0; k < 27u; k++) {
        led_put(nl, 14u + k, (int)(((pu.notes | lit) >> k) & 1u));
        led_put(nd, 14u + k, 1);
    }
    for (k = 0; k < NB; k++)
        led_put(nd, panel.btn[k], 1);
    if (song.playing) {
        led_clear(nd, panel.btn[B_PLAY]);
        nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u));
    }
    fm1_led_dim_level(0);
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led_dim[c] = nd[c];
        fm1_led_breath[c] = pu.asleep ? nd[c] : 0;
        fm1_led[c] = nl[c];
    }
}

/* ---------------------------------------------------------------- drawing */
static void pu_overlay(void)
{
    if (pu.hint_on) {                             /* the knob that turned: its name, and what it is set to */
        const kdef_t *kd = &KD[pu.scene][pu.hint_k];
        const char *vn = scene_kname(pu.hint_k, KV(pu.hint_k));
        pg.blend = PB_DARK;
        pg_rect(0, 204, 240, 36, 0);
        pg.blend = PB_SOLID;
        pg_text_c(120, 208, kd->name, 2, th->acc, RGB(20, 10, 30));
        if (vn) {
            pg_text_c(120, 225, vn, 2, 0xFFFF, RGB(20, 10, 30));
        } else {
            pg_rect(40, 227, 160, 8, RGB(20, 10, 30));
            pg_rect(42, 229, 156 * KV(pu.hint_k) / kd->max, 4, pg_hue(pu.hint_k * 36u + KV(pu.hint_k)));
        }
    }
    if (pu.vol_on) {
        uint32_t i, l = pu_vol_level();
        pg.blend = PB_DARK;
        pg_rect(20, 90, 200, 60, 0);
        pg.blend = PB_SOLID;
        pg_text_c(120, 98, "VOLUME", 3, 0xFFFF, RGB(20, 10, 30));
        for (i = 0; i < 8u; i++)
            pg_circle(50 + (int32_t)i * 20, 136, 6, i < l ? RGB(120, 240, 90) : RGB(60, 60, 80));
    }
    if (pu.exit_ms && pu.now - pu.exit_ms > 600u)
        pg_text_c(120, 110, "FELUCCA...", 3, 0xFFFF, RGB(20, 10, 30));
}

static void pu_splash_draw(void)
{
    static const char T[] = "PUNICORN";
    uint32_t t = pu.now - pu.splash_ms, i;
    char b[2];
    pg_vgrad(0, 240, RGB(30, 8, 50), RGB(140, 30, 90));
    for (i = 0; i < 14u; i++) {                   /* lightning-coloured rays behind */
        int32_t a = (int32_t)i * 73 + (int32_t)t / 8;
        pg_tri(120, 150, 120 + (pg_cos(a) * 220 >> 15), 150 + (pg_sin(a) * 220 >> 15), 120 + (pg_cos(a + 26) * 220 >> 15),
               150 + (pg_sin(a + 26) * 220 >> 15), i & 1u ? RGB(70, 16, 80) : RGB(110, 24, 90));
    }
    uni_draw(&pu.u[0]);
    b[1] = 0;
    for (i = 0; i < 8u; i++) {
        int32_t age = (int32_t)t - (int32_t)i * 70, dy = age < 0 ? -80 : age < 300 ? -(pg_sin(age * 512 / 300) * 26 >> 15) : (pg_sin((int32_t)t * 3 + (int32_t)i * 120) * 3 >> 15);
        b[0] = T[i];
        pg_text(26 + (int32_t)i * 24, 24 + dy, b, 4, pg_hue(i * 30u + t / 6u), RGB(20, 8, 30));
    }
    pfx_draw();
}

static void pu_draw(void)
{
    if (pu.splash_ms) {
        pu_splash_draw();
        return;
    }
    bg_draw();
    scene_draw();
    pfx_draw();
    pop_draw();
    pu_overlay();
}

static void pu_frame(void)
{
    uint32_t now = fm1_ms, dt = now - pu.last;
    if (dt > 100u)
        dt = 100u;
    if (!dt)
        dt = 1;
    pu.last = now;
    pu.now = now;
    pu.dt = dt;
    uni_now = now;
    pu_input();
    if (!pu.on)
        return;
    if (pu.splash_ms) {                           /* the hello */
        uint32_t t = now - pu.splash_ms;
        if (t > 2600u) {
            pu.splash_ms = 0;
            pg.full = 1;
        } else {
            if (pu.u[0].act == UA_IDLE && t % 900u < 40u)
                uni_act(&pu.u[0], UA_HOP);
            if (pu_rnd() % 3u == 0u)
                pfx_add(PT_STAR, pu_rand(10, 230), pu_rand(60, 230), 0, -pu_rand(4, 20), (uint32_t)pu_rand(3, 6), pg_hue(pu_rnd()), 500, 0);
        }
    }
    if (!pu.asleep && now - pu.idle_ms > 45000u && !song.playing) {
        pu.asleep = 1;
        uni_act(&pu.u[0], UA_SLEEP);
    }
    if (pu.asleep && pu_rnd() % 40u == 0u)
        pfx_add(PT_CHAR, pu.u[0].head.x, pu.u[0].head.y - 20, pu_rand(4, 10), -12, 2, 0xFFFF, 1500, 0)->aux = 'Z';
    if (!pu.splash_ms)
        scene_update();
    uni_update(&pu.u[0], now, dt);
    uni_update(&pu.u[1], now, dt);
    pfx_update(dt);
    if (pu.hint_on && now - pu.hint_ms > 1300u)
        pu.hint_on = 0;
    if (pu.shake) {                               /* everything jumps, then settles */
        pg.ox = (int16_t)pu_rand(-pu.shake, pu.shake);
        pg.oy = (int16_t)pu_rand(-pu.shake, pu.shake);
        pu.shake = (int16_t)(pu.shake - 1 - (int32_t)dt / 40);
        if (pu.shake < 0)
            pu.shake = 0;
    } else {
        pg.ox = pg.oy = 0;
    }
    if (pu.look_dirty && !song.playing && !pu.nheld && now - pu.idle_ms > 5000u) {   /* kept over power-off, when it is quiet */
        pu.look_dirty = 0;
        pu_look_save();
        settings_save();
    }
    pu_leds();
    pg_frame(pu_draw);
}

/* the main loop's frame (main.c, the browser's web_frame): 1 if Punicorn took it */
static int kid_frame(void)
{
    if (!pu.on)
        return 0;
    if (!pu.ready)
        pu_enter();
    pu_frame();
    settings_poll();
    return pu.on;
}
#define KID_ON() (pu.on)
#else
#define KID_ON() 0
static int kid_frame(void) { return 0; }
static void kid_master(void) {}
#endif
