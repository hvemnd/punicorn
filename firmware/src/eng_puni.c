/* SPDX-License-Identifier: GPL-3.0-only */
/* PUNI: Punicorn's own two sounds, in the engine slot the retired DIGITAL left empty (engine 1; the full Felucca
 * never offers it). MODE picks one:
 *   FART  a bass drum that can also be rude. A sine whose pitch falls from a SWEEP above the note (the 808's
 *         thump), crossfaded by FLAP into a driven saw that a fluttering gate chops (the raspberry). SWEEP how
 *         far the pitch falls, TIME how fast, FLAP how rude, DRV the drive, TONE a low-pass, RATE the flutter.
 *   PEE   two oscillators added together: WAVE A and WAVE B each morph sine > triangle > saw > square, RATIO
 *         puts B on a harmonic of A (x0.5 1 1.5 2 3 4 5 6), MIX balances them, TONE is a low-pass.
 * The track's ADSR is the amplitude. Punicorn's screens read the same parameters to draw what is heard. */
static const char *const N_PUNI_MODE[] = {"FART", "PEE"};
static const uint8_t PUNI_RATIO[8] = {8, 16, 24, 32, 48, 64, 80, 96};   /* B's pitch over A's, in 1/16 */

static void puni_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->ph[0] = v->ph[1] = v->ph[2] = 0;
    v->s[0] = 32767;                     /* the sweep, falling */
    v->s[1] = 0;                         /* the low-pass */
    v->s[3] = 0x30000000;                /* the flutter's rate */
    if (!v->s[2])
        v->s[2] = 0x2468ACE + (int32_t)v->age;
}

/* shape 0 .. 127: sine, triangle, saw, square and what lies between */
static inline int32_t puni_wave(uint32_t ph, uint32_t inc, int32_t shape)
{
    int32_t pos = shape * 3, seg = pos >> 7, f = pos & 127, a, b;
    if (seg > 2) { seg = 2; f = 127; }
    a = seg == 0 ? osc_sine(ph) : seg == 1 ? osc_tri(ph) : osc_saw(ph, inc);
    b = seg == 0 ? osc_tri(ph) : seg == 1 ? osc_saw(ph, inc) : osc_pulse(ph, inc, 0x80000000u);
    return a + (((b - a) * f) >> 7);
}

static void puni_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t i, ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2];
    int32_t lp = v->s[1], nst = v->s[2];
    int32_t k = 600 + p[P_E5] * p[P_E5] * 2;             /* the low-pass, Q15 */
    if (p[P_E0] == 1) {                                   /* PEE */
        uint32_t inc1 = m->inc, inc2 = (inc1 >> 4) * PUNI_RATIO[p[P_E3] & 7];
        int32_t mb = p[P_E4] * 258, ma = 32767 - mb;
        if (inc2 > 0x50000000u)
            inc2 = 0x50000000u;
        for (i = 0; i < n; i++) {
            int32_t s = mulq15(puni_wave(ph0, inc1, p[P_E1]), ma) + mulq15(puni_wave(ph1, inc2, p[P_E2]), mb);
            ph0 += inc1;
            ph1 += inc2;
            lp += ((s - lp) * (k >> 3)) >> 12;
            out[i] += voice_amp(lp, m, i) << 1;
        }
    } else {                                              /* FART */
        int32_t sw = v->s[0], flap = p[P_E3] * 258, drv = 32768 + p[P_E4] * 700;
        int32_t mul = 256 + ((p[P_E1] * 8 * sw) >> 15);  /* the pitch, falling to the note */
        uint32_t inc = (m->inc >> 8) * (uint32_t)mul, finc = (uint32_t)v->s[3];
        sw -= (sw * (40 + (127 - p[P_E2]) * 6)) >> 12;
        for (i = 0; i < n; i++) {
            int32_t s = osc_sine(ph0), g = 32767;
            if (flap) {
                int32_t b = softclip(osc_saw(ph0, inc) * 3);
                if (ph2 + finc < ph2) {                   /* a new flutter: another length, another depth */
                    uint32_t r = noise32(&nst);
                    finc = (uint32_t)(0x00200000 + p[P_E6] * 0x8000) + (r >> 11);
                }
                ph2 += finc >> 3;
                g = ph2 < 0x90000000u ? 32767 : 9000;
                s += mulq15(mulq15(b, g) - s, flap);
            }
            ph0 += inc;
            s = softclip(((s >> 2) * (drv >> 2)) >> 11);
            lp += ((s - lp) * (k >> 3)) >> 12;
            out[i] += voice_amp(lp, m, i) << 1;
        }
        v->s[0] = sw;
        v->s[3] = (int32_t)finc;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->s[1] = lp;
    v->s[2] = nst;
}

static const engine_t ENG_PUNI = {
    .name = "PUNI",
    .page_title = {"PUNI", "TONE"},
    .edit = {
        {"MODE", F_ENUM, 0, 1, 0, N_PUNI_MODE, 0},
        {"SWP", F_PCT, 0, 127, 60, 0, 0},
        {"TIME", F_PCT, 0, 127, 64, 0, 0},
        {"FLAP", F_PCT, 0, 127, 0, 0, 0},
        {"DRV", F_PCT, 0, 127, 20, 0, 0},
        {"TONE", F_PCT, 0, 127, 100, 0, 0},
        {"RATE", F_PCT, 0, 127, 40, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},
    },
    .note_on = puni_note_on,
    .render = puni_render,
    .knob = {P_E1, P_E2, P_E3, P_E4},
};
