/* SPDX-License-Identifier: GPL-3.0-only */
/* Punicorn on the host: the firmware's sources against a simulated FM-1 (web/emu/felucca_web.c), driven by a
 * little script, writing the screen as PPM pictures and the sound as a WAV file. Used to look at every scene
 * without an FM-1 (tools/punisim.py makes the pictures into PNG sheets).
 *   cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/punisim tools/punisim.c -lm
 *   build/punisim OUTDIR "script"
 * script, words apart: wN wait N ms | bNAME tap a button (FX SCL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+)
 *   dNAME / uNAME hold / let go of a button | kN key N down (0 = F3 .. 26) | KN key up | eROLE:N turn an encoder
 *   (SELECT ALGORITHM PRESETS K1 K2 K3 K4) N clicks | s a picture now | SN a picture every N ms (0 off) | mN MASTER 0..1023 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../web/emu/felucca_web.c"

static const char *dir;
static int shot, every;
static uint32_t next_shot;
static FILE *wav;
static uint32_t wav_n;
static uint64_t sent_sum;
static uint32_t frames_n, sent_max;

static void snap(void)
{
    char name[512];
    FILE *f;
    uint32_t i;
    snprintf(name, sizeof name, "%s/%04d.ppm", dir, shot++);
    f = fopen(name, "wb");
    if (!f) return;
    fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = web_fb[i];
        uint8_t rgb[3];
        c = (uint16_t)((c >> 8) | (c << 8));
        rgb[0] = (uint8_t)((c >> 11) * 255 / 31);
        rgb[1] = (uint8_t)(((c >> 5) & 63) * 255 / 63);
        rgb[2] = (uint8_t)((c & 31) * 255 / 31);
        fwrite(rgb, 3, 1, f);
    }
    fclose(f);
}
static void run(uint32_t ms)
{
    uint32_t end = fm1_ms + ms;
    while ((int32_t)(end - fm1_ms) > 0) {
        uint32_t d0 = web_draws;
        web_render(44);                       /* about a millisecond of sound */
        {
            uint32_t i;
            for (i = 0; wav && i < 44u; i++) {
                int16_t s[2];
                float l = out_l[i] * 32767.0f, r = out_r[i] * 32767.0f;
                s[0] = (int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
                s[1] = (int16_t)(r > 32767 ? 32767 : r < -32768 ? -32768 : r);
                fwrite(s, 4, 1, wav);
                wav_n++;
            }
        }
        if (web_draws != d0 && pg.sent) {
            frames_n++;
            sent_sum += pg.sent;
            if (pg.sent > sent_max) sent_max = pg.sent;
        }
        if (every && (int32_t)(fm1_ms - next_shot) >= 0) {
            snap();
            next_shot = fm1_ms + (uint32_t)every;
        }
    }
}
static int btn_of(const char *s)
{
    int b;
    for (b = 0; b < NB; b++)
        if (!strcmp(s, B_NAME[b])) return b;
    fprintf(stderr, "button %s?\n", s);
    return 0;
}
static int enc_of(const char *s)
{
    static const char *const N[] = {"SELECT", "ALGORITHM", "PRESETS", "K1", "K2", "K3", "K4"};
    int e;
    for (e = 0; e < 7; e++)
        if (!strcmp(s, N[e])) return e;
    fprintf(stderr, "encoder %s?\n", s);
    return 0;
}
int main(int argc, char **argv)
{
    char *sc, *tok;
    uint32_t btns = 0, keys = 0;
    if (argc < 3) return 2;
    dir = argv[1];
    {
        char name[512];
        uint8_t h[44] = {0};
        snprintf(name, sizeof name, "%s/audio.wav", dir);
        wav = fopen(name, "wb");
        if (wav) fwrite(h, 44, 1, wav);
    }
    web_nor_erase();
    web_boot();
    sc = strdup(argv[2]);
    for (tok = strtok(sc, " \n"); tok; tok = strtok(0, " \n")) {
        switch (tok[0]) {
        case 'w': run((uint32_t)atoi(tok + 1)); break;
        case 'b': btns |= 1u << btn_of(tok + 1); web_buttons(btns); run(70); btns &= ~(1u << btn_of(tok + 1)); web_buttons(btns); run(30); break;
        case 'd': btns |= 1u << btn_of(tok + 1); web_buttons(btns); break;
        case 'u': btns &= ~(1u << btn_of(tok + 1)); web_buttons(btns); break;
        case 'k': keys |= 1u << atoi(tok + 1); web_keys(keys); break;
        case 'K': keys &= ~(1u << atoi(tok + 1)); web_keys(keys); break;
        case 'e': { char *c = strchr(tok, ':'); if (c) { *c = 0; web_enc((uint32_t)enc_of(tok + 1), atoi(c + 1)); } break; }
        case 's': snap(); break;
        case 'S': every = atoi(tok + 1); next_shot = fm1_ms; break;
        case 'm': web_master((uint32_t)atoi(tok + 1)); break;
        default: fprintf(stderr, "word %s?\n", tok); break;
        }
    }
    if (wav) {
        uint32_t bytes = wav_n * 4u, v;
        uint16_t w;
        fseek(wav, 0, SEEK_SET);
        fwrite("RIFF", 4, 1, wav); v = 36u + bytes; fwrite(&v, 4, 1, wav); fwrite("WAVEfmt ", 8, 1, wav);
        v = 16; fwrite(&v, 4, 1, wav); w = 1; fwrite(&w, 2, 1, wav); w = 2; fwrite(&w, 2, 1, wav);
        v = 44100; fwrite(&v, 4, 1, wav); v = 44100 * 4; fwrite(&v, 4, 1, wav); w = 4; fwrite(&w, 2, 1, wav); w = 16; fwrite(&w, 2, 1, wav);
        fwrite("data", 4, 1, wav); fwrite(&bytes, 4, 1, wav);
        fclose(wav);
    }
    printf("frames %u, mean %u px a frame (%.1f ms on the panel), worst %u px (%.1f ms)\n", frames_n,
           frames_n ? (uint32_t)(sent_sum / frames_n) : 0u, frames_n ? (double)(sent_sum / frames_n) / 750.0 : 0.0, sent_max, sent_max / 750.0);
    return 0;
}
