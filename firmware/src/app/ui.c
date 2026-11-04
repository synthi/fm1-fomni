/* SPDX-License-Identifier: GPL-3.0-only */
/* OMNI UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too.
 *
 *   white keys     the strum plate: each one plucks a string of the chord
 *   black keys     the chord buttons; hold ENV for the minor, LFO for the 7th, both for the m7
 *   PLAY           the rhythm on / off (with SYNC START: it waits for the first chord)
 *   REC            SYNC START          ARP   CHORD HOLD          OCT- / OCT+   the plate's octave
 *   HOME SEQ FX SEL GLO   the pages: PLAY, RHYTHM, SOUND, CHORDS, SETUP, SETUP 2
 *                  GLO pressed again on SETUP goes on to SETUP 2 (the palette and the size of the
 *                  knob band); EDIT steps through all six
 *   SELECT tempo   ALGORITHM rhythm   PRESETS transpose   KNOB 1-4 the page's four values
 *   SAVE           saves (it also saves by itself, a few seconds after a change, when quiet, and
 *                  when the settings pages are left: a palette or a size is played with, then kept) */

enum { V_PLAY, V_RHYTHM, V_SOUND, V_PADS, V_COLOURS, V_SETUP, V_ENV, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"Play", "Rhythm", "Sound", "Chords", "Colours", "Setup", "Env"};
/* the four knobs of each page: a parameter, or one of these */
enum { K_PADROOT = 100, K_PADTYPE, K_LEDS, K_PALETTE, K_SIZE, K_NONE };
static const uint8_t VIEW_KNOB[NVIEWS][4] = {
    {P_VOICE1, P_VOICE2, P_SUSTAIN, P_CHORD},
    {P_RHYTHM, P_TEMPO, P_RHYVOL, P_ABC},
    {P_REVERB, P_SPACE, P_WIDTH, P_CREV},
    {K_PADROOT, K_PADTYPE, P_TRANSPOSE, P_OCTAVE},
    {K_PALETTE, K_SIZE, K_NONE, K_NONE},
    {P_TUNE, P_MIDI, K_LEDS, K_NONE},
    {P_ATKH, P_RELH, P_ATKC, P_RELC},
};

static const uint8_t WHITE_K[OM_NSTR] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t BLACK_K[NPADS] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};

/* the screen's colour: a palette from palettes.h, the rest of the tokens derived (mix, below). One soft
 * colour per section (harp, chords, rhythm), each with a pale tint for the shapes at rest */
#include "palettes.h"

static struct {
    uint8_t view;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    int8_t pad;                        /* the chord button sounding (or held), -1 none, -2 a chord from MIDI */
    uint8_t pad_sel;                   /* the chord button the CHORDS page edits */
    uint8_t npads_down;
    uint8_t root, type;                /* the chord (before transpose) */
    uint8_t armed;                     /* SYNC START waiting for a chord */
    uint8_t dirty;                     /* the project changed since the last save */
    uint32_t act_t, saved_t;
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;                    /* the knob turned last (0..3), shown bright for a moment */
    uint32_t touch_until;
    uint32_t frame;
    uint32_t sig[3];                   /* what each band shows: redrawn when it changes */
} ui;

/* ---------------------------------------------------------------- theme --- */
/* t 0..16: a -> b, channel by channel (RGB565); the palette's derived tokens come from here */
static uint16_t mix(uint16_t a, uint16_t b, int t)
{
    int r = ((a >> 11) * (16 - t) + (b >> 11) * t) / 16, g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) / 16,
        bl = ((a & 31) * (16 - t) + (b & 31) * t) / 16;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static uint16_t pal565(const uint8_t *c) { return RGB(c[0], c[1], c[2]); }

static struct {
    uint16_t bg, surf, line, dim, text, ink, harp, harp_t, chord, chord_t, rhy, rhy_t;
} th;
static uint8_t theme_i;                /* the palette in use (the Colours page shows its index) */

#define K_BG th.bg                    /* the paper */
#define K_PANEL th.surf               /* panels, pills, the knob band's card */
#define K_LINE th.line                /* rules, the gauge's track */
#define K_DIM th.dim                  /* labels, units, what is off */
#define K_TEXT th.text                /* values, the chord */
#define K_WHITE th.ink                /* text on a coloured fill (a lit pad, a pill) */
#define K_HARP th.harp                /* the strings */
#define K_HARP_T th.harp_t
#define K_CHORD th.chord              /* the chord pads */
#define K_CHORD_T th.chord_t
#define K_RHY th.rhy                  /* the rhythm */
#define K_RHY_T th.rhy_t

/* The palette in use: LINE and the pale tints are blended from its five hues (the fixed blends
 * Patagonia's gfx.c uses). A change has to repaint every band, and a band is only drawn again when
 * its hash moves: the three hashes go to a value a hash never takes. */
static void theme_set(uint32_t i)
{
    const om_theme_t *p = &OM_THEMES[i % NTHEMES];
    theme_i = (uint8_t)(i % NTHEMES);
    th.bg = pal565(p->c[TC_BG]);
    th.surf = pal565(p->c[TC_SURF]);
    th.text = pal565(p->c[TC_TEXT]);
    th.dim = pal565(p->c[TC_DIM]);
    th.ink = pal565(p->c[TC_INK]);
    th.harp = pal565(p->c[TC_HARP]);
    th.chord = pal565(p->c[TC_CHORD]);
    th.rhy = pal565(p->c[TC_RHY]);
    th.line = mix(th.bg, th.text, 3);        /* 18 % */
    th.harp_t = mix(th.bg, th.harp, 5);      /* 30 % */
    th.chord_t = mix(th.bg, th.chord, 5);
    th.rhy_t = mix(th.bg, th.rhy, 5);
    ui.sig[0] = ui.sig[1] = ui.sig[2] = 0u;
}

static uint8_t sp_said;                 /* a message went to the screen reader this frame: speak() leaves it be */
static void msg_text(const char *a, const char *b, char *t)   /* the message as a reader says it: "a b" */
{
    uint32_t i, k = 0;
    for (i = 0; i < 23u && a && a[i]; i++)
        t[k++] = a[i];
    if (b && b[0])
        t[k++] = ' ';
    for (i = 0; i < 23u && b && b[i]; i++)
        t[k++] = b[i];
    while (k && t[k - 1] == '.')               /* "UPDATE MODE IN 3..": the dots are for the eye */
        k--;
    t[k] = 0;
}
static void say_as(const char *a, const char *b, uint32_t flags)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    if (ann_on()) {                            /* and to a screen reader (announce.c) */
        msg_text(a, b, t);
        ann_say(t, 0, flags);
        sp_said = 1;
    }
}
static void say(const char *a, const char *b) { say_as(a, b, ANN_NOW); }
/* something did not happen: a screen reader says it whole (what follows does not cut it off) */
static void warn(const char *a, const char *b) { say_as(a, b, ANN_NOW | ANN_KEEP); }
void ui_say(const char *a, const char *b) { say(a, b); }

static void itoa_u(uint32_t v, char *b)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static int str_len_(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}

static void mark_dirty(void) { ui.dirty = 1; }

static uint32_t kb_held[4];                    /* MIDI in channel 2: the notes held (128) */
static int kb_any(void) { return (kb_held[0] | kb_held[1] | kb_held[2] | kb_held[3]) != 0; }
int ui_dirty(void) { return ui.dirty != 0; }

static void chord_name(int root, int type, char *b)
{
    const char *n = OM_NOTE_NAME[((root + proj.par[P_TRANSPOSE]) % 12 + 12) % 12], *t = OM_TYPE_NAME[type];
    while (*n)
        *b++ = *n++;
    while (*t)
        *b++ = *t++;
    *b = 0;
}

/* ------------------------------------------------------------ values --- */
static int knob_get(int k)
{
    if (k < P_NPARAMS)
        return proj.par[k];
    if (k == K_PADROOT)
        return proj.pad_root[ui.pad_sel];
    if (k == K_PADTYPE)
        return proj.pad_type[ui.pad_sel];
    if (k == K_LEDS)
        return proj.leds;
    if (k == K_PALETTE)
        return proj.theme;
    if (k == K_SIZE)
        return proj.large;
    return 0;
}
static void knob_range(int k, int *lo, int *hi)
{
    if (k < P_NPARAMS) {
        *lo = om_param_info(k)->lo;
        *hi = om_param_info(k)->hi;
    } else {
        *lo = 0;
        *hi = k == K_PADROOT ? 11
            : k == K_PADTYPE ? CH_NTYPES - 1
            : k == K_LEDS ? 2
            : k == K_PALETTE ? (int)NTHEMES - 1
            : k == K_SIZE ? 1
            : 0;
    }
}
static const char *knob_name(int k)
{
    if (k < P_NPARAMS)
        return om_param_info(k)->name;
    return k == K_PADROOT ? "Root"
         : k == K_PADTYPE ? "Type"
         : k == K_LEDS ? "Lights"
         : k == K_PALETTE ? "Palette"
         : k == K_SIZE ? "Size"
         : "";
}
static void knob_text(int k, char *b)
{
    int v = knob_get(k);
    if (k < P_NPARAMS) {
        om_param_text(k, v, b);
    } else if (k == K_PADROOT) {
        const char *n = OM_NOTE_NAME[v];
        while ((*b++ = *n++))
            ;
    } else if (k == K_PADTYPE) {
        const char *n = v ? OM_TYPE_NAME[v] : "maj";
        while ((*b++ = *n++))
            ;
    } else if (k == K_LEDS) {                    /* On: the keys, and the unlit buttons glow; Keys: no glow */
        const char *n = v == 2 ? "Keys" : v ? "On" : "Off";
        while ((*b++ = *n++))
            ;
    } else if (k == K_PALETTE) {                 /* short: the full name goes on the page, big */
        const char *n = OM_THEMES[v % NTHEMES].short_name;
        while ((*b++ = *n++))
            ;
    } else if (k == K_SIZE) {
        const char *n = v ? "Large" : "Normal";
        while ((*b++ = *n++))
            ;
    } else {
        b[0] = 0;
    }
}

static void sound_chord(void);
static void knob_set(int k, int v)
{
    int lo, hi;
    knob_range(k, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (v == knob_get(k))
        return;
    if (k < P_NPARAMS) {
        proj.par[k] = (int16_t)v;
        om_set(k, v);
    } else if (k == K_PADROOT || k == K_PADTYPE) {
        if (k == K_PADROOT)
            proj.pad_root[ui.pad_sel] = (uint8_t)v;
        else
            proj.pad_type[ui.pad_sel] = (uint8_t)v;
        if (ui.pad == (int8_t)ui.pad_sel) {        /* the button being edited is the one sounding */
            ui.root = proj.pad_root[ui.pad_sel];
            ui.type = proj.pad_type[ui.pad_sel];
            om_chord(ui.root, ui.type);
        }
    } else if (k == K_LEDS) {
        proj.leds = (uint8_t)v;
    } else if (k == K_PALETTE) {
        proj.theme = (uint8_t)v;
        theme_set((uint32_t)v);                /* the whole screen in the new palette, now */
    } else if (k == K_SIZE) {
        proj.large = (uint8_t)v;
        ui.sig[2] = 0u;                        /* the knob band, drawn again */
    }
    mark_dirty();
}

/* knob acceleration by speed: a slow turn is one step a detent, a quick one up to 6 */
static uint8_t accel_off;              /* the host simulator's "spin": exact steps */
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (range <= 24 || !a || accel_off)
        return s;
    if (a > 1)
        dt /= a;
    m = dt < 15u ? 6u : dt < 30u ? 4u : dt < 60u ? 2u : 1u;
    return s * (int32_t)m;
}

static void turn(int role, int k, int32_t e)
{
    int lo, hi;
    if (!e || k == K_NONE)
        return;
    knob_range(k, &lo, &hi);
    knob_set(k, knob_get(k) + accel(role, e, hi - lo));
}

/* ------------------------------------------------------------- chords --- */
static void sound_chord(void) { om_chord(ui.root, ui.type); }

static void pad_down(int p)
{
    uint32_t b = ui.btn;
    int type = proj.pad_type[p];
    int mods = (b >> B_ENV & 1u) | (b >> B_LFO & 1u) << 1;
    if (mods)                                    /* the OM's rows: minor, 7th, both: m7 */
        type = mods == 1 ? CH_MIN : mods == 2 ? CH_7 : CH_M7;
    if (mods & 1)
        ui.btn_used |= 1u << B_ENV;
    if (mods & 2)
        ui.btn_used |= 1u << B_LFO;
    if (ui.view == V_PADS)
        ui.pad_sel = (uint8_t)p;
    if (proj.hold && ui.pad == p && !ui.npads_down && ui.root == proj.pad_root[p] && ui.type == type) {
        ui.pad = -1;                             /* HOLD: the sounding button again lets it go */
        om_gate(0);
        ui.npads_down++;
        return;
    }
    ui.npads_down++;
    ui.pad = (int8_t)p;
    ui.root = proj.pad_root[p];
    ui.type = (uint8_t)type;
    sound_chord();
    om_gate(1);
    if (ui.armed) {                              /* SYNC START: the rhythm starts with the chord */
        ui.armed = 0;
        om_play(1);
    }
}

static void pad_up(int p)
{
    if (ui.npads_down)
        ui.npads_down--;
    if (ui.npads_down || proj.hold)
        return;
    if (ui.pad == p || ui.pad >= 0) {
        om_gate(0);
        ui.pad = -1;
    }
}

/* ------------------------------------------------------------ buttons --- */
/* The settings pages are where a palette and a size are tried on: leaving them keeps what was chosen
 * (the autosave would a few seconds later, too, but leaving is the promise). */
static void set_view(int v)
{
    int was = ui.view == V_COLOURS || ui.view == V_SETUP;
    int now = v == V_COLOURS || v == V_SETUP;
    if (was && !now && ui.dirty) {
        if (project_save() == 0) {
            ui.dirty = 0;
            ui.saved_t = plat_ms();
        } else {
            warn("NOT SAVED", "FLASH ERROR");
        }
    }
    ui.view = (uint8_t)v;
    ui.touched = -1;
}

static void button(int b)
{
    switch (b) {
    case B_HOME: set_view(V_PLAY); break;
    case B_SEQ: set_view(V_RHYTHM); break;
    case B_FX: set_view(V_SOUND); break;
    case B_SEL: set_view(V_PADS); break;
    case B_GLO: set_view(ui.view == V_COLOURS ? V_SETUP : V_COLOURS); break;  /* the palette first, GLO again the rest */
    case B_ENV: set_view(V_ENV); break;                                      /* a tap of ENV (held it is the minor row) */
    case B_EDIT: set_view((ui.view + 1) % NVIEWS); break;
    case B_PLAY:
        if (om_playing) {
            om_play(0);
            ui.armed = 0;
        } else if (ui.armed) {
            ui.armed = 0;
            say("SYNC START", "CANCELLED");
        } else if (proj.sync && ui.pad == -1) {
            ui.armed = 1;
            say("WAITING FOR", "A CHORD");
        } else {
            om_play(1);
        }
        break;
    case B_REC:
        proj.sync = (uint8_t)!proj.sync;
        if (!proj.sync)
            ui.armed = 0;
        say("SYNC START", proj.sync ? "ON" : "OFF");
        mark_dirty();
        break;
    case B_ARP:
        proj.hold = (uint8_t)!proj.hold;
        if (!proj.hold && !ui.npads_down && ui.pad != -1 && !(ui.pad == -2 && kb_any())) {
            om_gate(0);
            ui.pad = -1;
        }
        say("CHORD HOLD", proj.hold ? "ON" : "OFF");
        mark_dirty();
        break;
    case B_OCTDN: case B_OCTUP:
        knob_set(P_OCTAVE, proj.par[P_OCTAVE] + (b == B_OCTUP ? 1 : -1));
        {
            char t[8];
            om_param_text(P_OCTAVE, proj.par[P_OCTAVE], t);
            say("OCTAVE", t);
        }
        break;
    case B_SAVE:
        if (project_save() == 0) {
            ui.dirty = 0;
            say("SAVED", 0);
        } else {
            warn("NOT SAVED", "FLASH ERROR");
        }
        break;
    default: break;
    }
}

/* MIDI in, channel 2: the chord held on a keyboard. The best fit of the nine types: a root among the
 * notes (the lowest preferred), as many of its tones held as possible and nothing left over but its
 * fifth (a held C E G Bb is C7, which the OM plays without the G). Under two notes: no chord. */
static int kb_chord(int *root, int *type)
{
    uint32_t pcs = 0, n;
    int low = -1, r, t, best = -1;
    for (n = 0; n < 128u; n++)
        if (kb_held[n >> 5] >> (n & 31u) & 1u) {
            pcs |= 1u << (n % 12u);
            if (low < 0)
                low = (int)(n % 12u);
        }
    if (low < 0 || !(pcs & (pcs - 1u)))
        return 0;
    for (r = 0; r < 12; r++) {
        if (!(pcs >> r & 1u))
            continue;
        for (t = 0; t < CH_NTYPES; t++) {
            uint32_t tones = 0, k;
            int score, hit = 0, extra = 0;
            for (k = 0; k < 3u; k++)
                tones |= 1u << ((r + OM_TYPE_TONES[t][k]) % 12);
            for (k = 0; k < 12u; k++) {
                hit += (pcs & tones) >> k & 1u;
                extra += (pcs & ~tones & ~(1u << ((r + 7) % 12))) >> k & 1u;
            }
            score = hit * 10 - extra * 3 + (r == low) * 2 - t / 4;   /* the common types first, on a tie */
            if (score > best) {
                best = score;
                *root = r;
                *type = t;
            }
        }
    }
    return best >= 20;
}

static void kb_update(void)
{
    int root, type;
    if (kb_chord(&root, &type)) {
        if (ui.pad == -2 && ui.root == root && ui.type == type)
            return;
        ui.root = (uint8_t)root;
        ui.type = (uint8_t)type;
        ui.pad = -2;                             /* a chord from MIDI: no button lit */
        sound_chord();
        om_gate(1);
        if (ui.armed) {
            ui.armed = 0;
            om_play(1);
        }
    } else if (ui.pad == -2 && !kb_any() && !proj.hold) {
        ui.pad = -1;                             /* all let go */
        om_gate(0);
    }
}

/* MIDI in (USB and the TRS jack): channel 1 notes pluck the nearest string, channel 2 notes pick the
 * chord, clock and start / stop drive the rhythm */
static void midi_in(void)
{
    uint32_t pkt;
    int chords = 0;
    while (plat_midi_in(&pkt)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        if (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu) {
            om_clock(st == 0xF8u ? OM_CLK_TICK : st == 0xFAu ? OM_CLK_START : st == 0xFBu ? OM_CLK_CONTINUE : OM_CLK_STOP);
            if (st == 0xFAu)
                ui.armed = 0;
        } else if (st == 0x90u && v) {
            int s, best = 0, bd = 999;
            for (s = 0; s < OM_NSTR; s++) {
                int d = om_str_note[s] - (int)n;
                d = d < 0 ? -d : d;
                if (d < bd) {
                    bd = d;
                    best = s;
                }
            }
            om_strum(best);
        } else if ((st == 0x80u || st == 0x90u) && !v) {
            int s, best = 0, bd = 999;              /* the note let go: the nearest string's release */
            for (s = 0; s < OM_NSTR; s++) {
                int d = om_str_note[s] - (int)n;
                d = d < 0 ? -d : d;
                if (d < bd) {
                    bd = d;
                    best = s;
                }
            }
            om_damp(best);
        } else if (st == 0x91u || st == 0x81u) {
            if (st == 0x91u && v)
                kb_held[n >> 5] |= 1u << (n & 31u);
            else
                kb_held[n >> 5] &= ~(1u << (n & 31u));
            chords = 1;
        }
    }
    if (chords)
        kb_update();
}

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            if (i == B_PLAY)                   /* transport on the press */
                button((int)i);
        } else if (!(ui.btn_used & m) && i == B_ENV) {
            button(B_ENV);                     /* a tap: the ENV page (held with a black key, the minor row) */
        } else if (!(ui.btn_used & m) && i != B_PLAY && i != B_LFO) {
            button((int)i);
        }
    }
    ch = keys ^ ui.keys;
    for (i = 0; i < OM_NSTR; i++) {            /* strings first: a strum is the fastest thing here */
        uint32_t m = 1u << WHITE_K[i];
        if ((ch & keys) & m)
            om_strum((int)i);
        else if ((ch & ui.keys) & m)           /* the key let go: its string's release (Str R) */
            om_damp((int)i);
    }
    ui.keys = keys;
    for (i = 0; i < NPADS; i++)
        if (ch >> BLACK_K[i] & 1u) {
            if (keys >> BLACK_K[i] & 1u)
                pad_down((int)i);
            else
                pad_up((int)i);
        }
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    if ((e = plat_enc(EN_SELECT)) != 0)
        turn(EN_SELECT, P_TEMPO, e);
    if ((e = plat_enc(EN_ALGO)) != 0)
        knob_set(P_RHYTHM, proj.par[P_RHYTHM] + (e > 0 ? 1 : -1));
    if ((e = plat_enc(EN_PRESET)) != 0)
        knob_set(P_TRANSPOSE, proj.par[P_TRANSPOSE] + (e > 0 ? 1 : -1));
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            turn(EN_K1 + (int)i, VIEW_KNOB[ui.view][i], e);
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = plat_ms();
        }
}

/* ------------------------------------------------------------- screen --- */
/* three bands: the header (32 px), the instrument (120), the knobs (88). The letters are as big as the
 * band and the cell allow (a tired eye, a 240 x 240 screen): the value of a knob is FONT_L (32 px) when
 * the string fits, then M, then B, then S */
#define HDR_H 32
#define MAIN_Y 32
#define MAIN_H 120
#define KNB_Y 152
#define KNB_H 88

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/* a rectangle with rounded corners (radius r, up to 6) */
static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const uint8_t IN[7][6] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}, {3, 2, 1, 0, 0}, {4, 2, 1, 1, 0, 0}};
    int32_t j;
    if (r > 6)
        r = 6;
    if (r > h / 2)
        r = h / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        int32_t d = IN[r][j];
        cv_rect(x + d, y + j, w - 2 * d, 1, c);
        cv_rect(x + d, y + h - 1 - j, w - 2 * d, 1, c);
    }
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

static void text_r(int32_t rx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(rx - text_w(f, s), y, f, s, c);
}

/* does the face have every glyph of the string? (FONT_M holds 32..95: capitals and digits, no lower case) */
static int face_has(const felucca_font_t *f, const char *s)
{
    for (; *s; s++)
        if ((uint8_t)*s < f->first || (uint8_t)*s > f->last)
            return 0;
    return 1;
}

/* The biggest face the string has and that fits w px: L (32 px) -> M (24) -> B (16) -> S (16, and the
 * one used when nothing fits). It is what makes the values readable: a "100" is 48 px in L and fits a
 * knob's 54, while a name like "Sustain" (92 px in L) steps down to B. */
static const felucca_font_t *value_face(const char *s, int32_t w)
{
    static const felucca_font_t *const F[4] = {&FONT_L, &FONT_M, &FONT_B, &FONT_S};
    int i;
    for (i = 0; i < 4; i++)
        if (face_has(F[i], s) && text_w(F[i], s) <= w)
            return F[i];
    return &FONT_S;
}

/* a knob's label: S (16) while it fits the cell, else XS (12): "Auto bass" is 62 px in S and a cell is
 * 60 wide, and a label cut by the screen's edge is worse than a small one */
static const felucca_font_t *label_face(const char *s, int32_t w)
{
    return text_w(&FONT_S, s) <= w ? &FONT_S : &FONT_XS;
}

/* a pad's chord type: B (16) while it fits the 18 px button, else XS. Never bigger: a lone "7" in L is
 * 18 px wide, exactly the pad, and its ink overflows it */
static const felucca_font_t *type_face(const char *s, int32_t w)
{
    return text_w(&FONT_B, s) <= w ? &FONT_B : &FONT_XS;
}

static void draw_header(void)
{
    char t[24];
    int msg = plat_ms() < ui.msg_until;
    uint32_t h = hash(hash(hash(2166136261u, ui.view), om_playing | ui.armed << 1 | proj.hold << 2 | proj.sync << 3 |
                                                       ui.dirty << 4 | (uint32_t)msg << 5),
                      (uint32_t)proj.par[P_RHYTHM] << 8 | (uint32_t)proj.par[P_TEMPO] | (uint32_t)om_ext << 20);
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
    if (om_playing)
        h = hash(h, om_step / 4u);
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, HDR_H, K_BG);
    if (msg) {
        int32_t x = cv_text(10, 8, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 8, &FONT_B, ui.msg[1], K_HARP);
    } else {
        int32_t x = 10, left;
        x = cv_text(x, 8, &FONT_B, VIEW_NAME[ui.view], K_TEXT);
        left = x + 6;
        if (proj.hold) {                          /* the tags, read from a metre away: ink on the colour */
            int32_t w = text_w(&FONT_S, "HOLD") + 12;
            cv_round(left, 5, w, 22, 7, K_CHORD);
            cv_text(left + 6, 7, &FONT_S, "HOLD", K_WHITE);
            left += w + 5;
        }
        if (proj.sync) {
            int32_t w = text_w(&FONT_S, "SYNC") + 12;
            cv_round(left, 5, w, 22, 7, K_RHY);
            cv_text(left + 6, 7, &FONT_S, "SYNC", K_WHITE);
            left += w + 5;
        }
        if (ui.dirty)
            cv_round(left, 13, 6, 6, 3, K_DIM);
        {   /* right: the state, the tempo, and the rhythm's name while it fits beside the tags */
            x = 232;
            if (om_playing) {                     /* a beat light */
                int on = (om_step & 3u) < 2u;
                cv_round(x - 12, 10, 12, 12, 6, on ? K_RHY : K_RHY_T);
                x -= 18;
            } else if (ui.armed) {
                cv_round(x - 12, 10, 12, 12, 6, (ui.frame / 8u) & 1u ? K_RHY : K_RHY_T);
                x -= 18;
            } else {
                int32_t j;                        /* stopped: a play triangle */
                for (j = 0; j < 5; j++)
                    cv_rect(x - 12 + j, 10 + j, 1, 12 - 2 * j, K_DIM);
                x -= 16;
            }
            if (om_ext) {                         /* following MIDI clock */
                t[0] = 'E';
                t[1] = 'X';
                t[2] = 'T';
                t[3] = 0;
            } else {
                itoa_u((uint32_t)proj.par[P_TEMPO], t);
            }
            x -= text_w(&FONT_M, t);
            cv_text(x, 3, &FONT_M, t, om_playing ? K_TEXT : K_DIM);
            x -= 10;
            if (x - text_w(&FONT_S, om_rhythm(proj.par[P_RHYTHM])->name) > left + 4) {
                x -= text_w(&FONT_S, om_rhythm(proj.par[P_RHYTHM])->name);
                cv_text(x, 8, &FONT_S, om_rhythm(proj.par[P_RHYTHM])->name, om_playing ? K_RHY : K_DIM);
            }
        }
    }
    cv_blit(0, 0);
}

static void draw_strings(int32_t y0, int32_t h)
{
    int s;
    for (s = 0; s < OM_NSTR; s++) {
        float lv = om_str_level[s];
        int t = (int)(lv * 16.0f + 0.5f), x = 9 + s * 14, w = 1 + (int)(lv * 4.0f + 0.3f);
        uint16_t c = mix(K_HARP_T, K_HARP, t > 16 ? 16 : t);
        int amp = (int)(lv * 3.0f), y;
        if (amp && (ui.frame & 1u))
            amp = -amp;
        for (y = 0; y < h; y++) {                  /* a string: bowed out in the middle while it rings */
            int d = (y < h / 2 ? y : h - 1 - y) * amp * 2 / h;
            cv_rect(x + 6 - w / 2 + d, y0 + y, w, 1, c);
        }
        if ((om_str_note[s] % 12) == (uint8_t)(((ui.root + proj.par[P_TRANSPOSE]) % 12 + 12) % 12))
            cv_round(x + 4, y0 + h + 3, 5, 5, 2, t > 2 ? K_HARP : K_LINE);   /* the roots: the plate's dots */
    }
}

static void draw_rhythm(int32_t y0, int32_t h)
{
    const om_rhythm_t *r = om_rhythm(proj.par[P_RHYTHM]);
    int len = r->len, s, d, cw = 224 / len;
    int32_t x0 = 120 - cw * len / 2, rowh = h / 6;
    static const char *const DN[5] = {"BD", "CL", "HH", "CY", "SD"};
    for (d = 0; d < 6; d++) {
        for (s = 0; s < len; s++) {
            int on, cur = om_playing && om_step == s;
            uint16_t c;
            if (d < 5) {
                on = r->drum[s] >> d & 1u;
                c = on ? (cur ? K_TEXT : K_RHY) : (s % 4 ? K_PANEL : K_RHY_T);
            } else {                               /* the bass line */
                on = r->bass[s] == 'T' || r->bass[s] == 'O';
                c = on ? (cur ? K_TEXT : K_CHORD) : (s % 4 ? K_PANEL : K_CHORD_T);
            }
            cv_round(x0 + s * cw, y0 + d * rowh, cw - 1, rowh - 2, 2, c);
        }
        (void)DN;
    }
}

/* the nine chord types as colours, so a pad says which one at a glance (the word is small on an 18 px
 * button): a major is the pad's own rule, a minor the chord's hue, a 7th the rhythm's, and so on */
static uint16_t pad_type_tone(int t)
{
    switch (t) {
    case CH_MAJ: return K_LINE;
    case CH_MIN: return K_CHORD;
    case CH_7: return K_RHY;
    case CH_MAJ7: return K_HARP;
    case CH_M7: return mix(K_CHORD, K_RHY, 8);
    case CH_DIM: return K_DIM;
    case CH_AUG: return mix(K_HARP, K_RHY, 8);
    case CH_SUS4: return mix(K_CHORD, K_HARP, 8);
    default: return K_TEXT;                  /* add 9 */
    }
}

static void draw_pads(int32_t y0)
{
    int p;
    for (p = 0; p < NPADS; p++) {
        char t[12];
        int32_t x = 4 + p * 20 + ((p >= 3) + (p >= 5) + (p >= 8) + (p >= 10)) * 3;   /* grouped as the keys are */
        int on = ui.pad == p, sel = ui.view == V_PADS && ui.pad_sel == p;
        const char *tn = proj.pad_type[p] ? OM_TYPE_NAME[proj.pad_type[p]] : "";
        if (sel)
            cv_round(x - 1, y0 - 1, 20, 34, 6, K_TEXT);
        cv_round(x, y0, 18, 32, 5, on ? K_CHORD : K_CHORD_T);
        chord_name(proj.pad_root[p], 0, t);
        text_c(x + 9, y0 + 1, &FONT_B, t, on ? K_WHITE : K_TEXT);
        if (tn[0]) {                              /* its type: the word in the biggest face that fits, and its colour */
            text_c(x + 9, y0 + 15, type_face(tn, 18), tn, on ? K_WHITE : K_TEXT);
            if (proj.pad_type[p] != CH_MAJ)
                cv_round(x + 13, y0 + 4, 5, 5, 2, on ? K_WHITE : pad_type_tone(proj.pad_type[p]));
        }
    }
}

/* The ENV page: the two envelopes, drawn from their four times. Attack linear, release exponential,
 * both on a log scale (a 10 s release cannot be to scale beside a 5 ms attack). The strings in the
 * harp's colour, the chord and its bass in the chord's: the knob labels say which is which. */
static int32_t env_width(int32_t span, int v)   /* the knob's 0..100 as a width: already a log scale */
{
    if (v <= 0)
        return 0;
    if (v > 100)
        v = 100;
    return span * v / 100;
}

static void env_shape(int32_t x0, int32_t w, int32_t y0, int32_t h, int atk_ms, int rel_ms, uint16_t c)
{
    int32_t aw = 2 + env_width(w / 2 - 4, atk_ms), rw = 2 + env_width(w / 2 - 4, rel_ms);
    int32_t i, top = y0 + 2, base = y0 + h - 3;
    for (i = 0; i < w; i++) {
        float lvl;
        int32_t y;
        if (i < aw) {
            lvl = (float)i / (float)aw;              /* the attack: a straight line up */
        } else if (i < w - rw) {
            lvl = 1.0f;                              /* held */
        } else {
            float u = (float)(i - (w - rw)) / (float)(rw > 0 ? rw : 1);
            lvl = 1.0f / (1.0f + 9.0f * u * u);      /* the release: the shape of an exponential */
        }
        y = base - (int32_t)(lvl * (float)(base - top));
        cv_rect(x0 + i, y, 1, base - y + 1, c);
    }
    cv_rect(x0, base + 2, w, 1, K_LINE);
}

static void draw_env(int32_t y0, int32_t h)
{
    env_shape(6, 110, y0, h, proj.par[P_ATKH], proj.par[P_RELH], K_HARP);
    cv_rect(120, y0, 1, h, K_LINE);
    env_shape(124, 110, y0, h, proj.par[P_ATKC], proj.par[P_RELC], K_CHORD);
}

/* The Colours page: the palette itself, big, with its six colours as samples. The pads below are drawn
 * in it, so the page is the preview of what is being chosen. */
static void draw_theme_page(void)
{
    static const uint8_t MAP[6] = {TC_BG, TC_SURF, TC_TEXT, TC_HARP, TC_CHORD, TC_RHY};
    const om_theme_t *p = &OM_THEMES[theme_i];
    char a[8], b[8];
    int i;
    int32_t x, w;
    cv_text(12, 0, value_face(p->name, 170), p->name, K_TEXT);
    itoa_u((uint32_t)theme_i + 1u, a);
    itoa_u((uint32_t)NTHEMES, b);
    w = text_w(&FONT_B, a) + text_w(&FONT_B, "/") + 4 + text_w(&FONT_B, b);
    x = 240 - 12 - w;
    x = cv_text(x, 10, &FONT_B, a, K_TEXT);
    x = cv_text(x + 2, 10, &FONT_B, "/", K_DIM);
    cv_text(x + 3, 10, &FONT_B, b, K_DIM);
    for (i = 0; i < 6; i++) {                     /* each colour in its place, with a hairline: BG on BG */
        int32_t sx = 4 + i * 39;
        cv_round(sx, 42, 35, 26, 6, K_LINE);
        cv_round(sx + 1, 43, 33, 24, 5, pal565(p->c[MAP[i]]));
    }
}

static void draw_main(void)
{
    char name[16], notes[24];
    uint32_t h = hash(2166136261u, ui.view), s;
    for (s = 0; s < OM_NSTR; s++)
        h = hash(h, (uint32_t)(om_str_level[s] * 24.0f) | (uint32_t)om_str_note[s] << 8);
    h = hash(hash(h, (uint32_t)ui.pad | ui.pad_sel << 8 | ui.root << 16 | ui.type << 24),
             (uint32_t)proj.par[P_TRANSPOSE] << 8 | (uint32_t)(om_chord_level * 8.0f));
    for (s = 0; s < NPADS; s++)
        h = hash(h, proj.pad_root[s] | proj.pad_type[s] << 4);
    if (ui.view == V_RHYTHM)
        h = hash(hash(h, om_playing ? om_step : 255u), (uint32_t)proj.par[P_RHYTHM]);
    if (ui.view == V_COLOURS)
        h = hash(hash(h, (uint32_t)proj.theme), (uint32_t)proj.large);
    if (ui.view == V_ENV)
        h = hash(hash(h, (uint32_t)proj.par[P_ATKH] << 16 | (uint32_t)proj.par[P_RELH]),
                 (uint32_t)proj.par[P_ATKC] << 16 | (uint32_t)proj.par[P_RELC]);
    if (h == ui.sig[1])
        return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);
    if (ui.view == V_COLOURS) {
        draw_theme_page();
    } else {
        chord_name(ui.root, ui.type, name);
        {   /* the chord: its name, big, and its notes */
            uint8_t hp[OM_NSTR], cn[OM_NCHORD], bn;
            int i, k = 0;
            uint16_t c = mix(K_TEXT, K_CHORD, (int)(om_chord_level * 16.0f));
            const char *tn = OM_TYPE_NAME[ui.type];
            int32_t x = 12;
            name[str_len_(name) - str_len_(tn)] = 0;        /* the root, big; the type beside it */
            x = cv_text(x, 0, &FONT_L, name, c);
            cv_text(x + 2, 14, &FONT_B, tn, c);
            om_voicing(ui.root, ui.type, proj.par[P_TRANSPOSE], proj.par[P_OCTAVE], hp, cn, &bn);
            for (i = 0; i < 3; i++) {
                const char *n = OM_NOTE_NAME[cn[i] % 12];
                if (i)
                    notes[k++] = ' ';
                while (*n)
                    notes[k++] = *n++;
            }
            notes[k] = 0;
            cv_text(240 - 12 - text_w(&FONT_S, notes), 10, &FONT_S, notes, K_DIM);
        }
        if (ui.view == V_RHYTHM)
            draw_rhythm(34, 42);
        else if (ui.view == V_ENV)
            draw_env(34, 42);
        else
            draw_strings(34, 42);
    }
    draw_pads(86);
    cv_blit(0, MAIN_Y);
}

static void draw_knobs(void)
{
    uint32_t h = hash(hash(2166136261u, ui.view | (uint32_t)ui.touched << 8 | ui.pad_sel << 16),
                      (uint32_t)proj.large << 24),
                 i;
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)knob_get(VIEW_KNOB[ui.view][i]));
    h = hash(h, (uint32_t)now_touch);
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    if (!proj.large) {
        /* four cards: the label, the value in the biggest face that fits, and the gauge */
        cv_round(4, 2, 232, KNB_H - 6, 7, K_PANEL);
        for (i = 0; i < 4; i++) {
            int k = VIEW_KNOB[ui.view][i], lo, hi, v;
            int32_t x = (int32_t)i * 60, cx = x + 30, bw;
            int hot = now_touch && ui.touched == (int)i;
            char t[16];
            if (k == K_NONE)
                continue;
            if (i)
                cv_rect(x, 8, 1, KNB_H - 20, K_LINE);
            knob_range(k, &lo, &hi);
            v = knob_get(k);
            text_c(cx, 5, label_face(knob_name(k), 56), knob_name(k), hot ? K_TEXT : K_DIM);
            knob_text(k, t);
            text_c(cx, 22, value_face(t, 54), t, hot ? K_HARP : K_TEXT);
            bw = hi > lo ? (int32_t)(44 * (v - lo) / (hi - lo)) : 0;
            cv_round(cx - 22, 62, 44, 8, 4, K_LINE);
            if (lo < 0) {                            /* centred values: a bar from the middle */
                int32_t m = cx - 22 + 44 * (0 - lo) / (hi - lo), e = cx - 22 + bw;
                cv_round(e < m ? e : m, 62, (e < m ? m - e : e - m) + 8, 8, 4, hot ? K_HARP : K_CHORD);
            } else if (bw) {
                cv_round(cx - 22, 62, bw < 8 ? 8 : bw, 8, 4, hot ? K_HARP : K_CHORD);
            }
        }
    } else {
        /* LARGE: two rows of two cards; the label and the value share a line, the gauge runs under it.
         * 114 px of card is room for a 32 px value beside its name */
        for (i = 0; i < 4; i++) {
            int k = VIEW_KNOB[ui.view][i], lo, hi, v;
            int32_t x = 4 + (int32_t)(i & 1) * 118, y = 4 + (int32_t)(i >> 1) * 42, bw;
            int hot = now_touch && ui.touched == (int)i;
            char t[16];
            if (k == K_NONE)
                continue;
            cv_round(x, y, 114, 38, 6, K_PANEL);
            knob_range(k, &lo, &hi);
            v = knob_get(k);
            knob_text(k, t);
            {
                const char *nm = knob_name(k);
                int32_t half = 100 - text_w(&FONT_S, nm);
                cv_text(x + 7, y + 12, label_face(nm, 96), nm, hot ? K_TEXT : K_DIM);
                text_r(x + 107, y + 3, value_face(t, half - 6), t, hot ? K_HARP : K_TEXT);
            }
            bw = hi > lo ? (int32_t)(102 * (v - lo) / (hi - lo)) : 0;
            cv_rect(x + 6, y + 33, 102, 2, K_LINE);
            if (lo < 0) {
                int32_t m = 102 * (0 - lo) / (hi - lo), e = bw;
                cv_rect(x + 6 + (e < m ? e : m), y + 33, (e < m ? m - e : e - m) + 2, 2, hot ? K_HARP : K_CHORD);
            } else if (bw) {
                cv_rect(x + 6, y + 33, bw < 2 ? 2 : bw, 2, hot ? K_HARP : K_CHORD);
            }
        }
    }
    cv_blit(0, KNB_Y);
}

static void leds(void)
{
    uint32_t b = 0, k = 0, i;
    static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_SEQ, B_FX, B_SEL, B_GLO, B_GLO, B_ENV};
    b |= 1u << VIEW_BTN[ui.view];
    if (om_playing)
        b |= (om_step & 3u) < 2u ? 1u << B_PLAY : 0u;
    else if (ui.armed && (ui.frame / 8u) & 1u)
        b |= 1u << B_PLAY;
    if (proj.sync)
        b |= 1u << B_REC;
    if (proj.hold)
        b |= 1u << B_ARP;
    if (proj.leds) {
        if (ui.pad >= 0)
            k |= 1u << BLACK_K[ui.pad];
        for (i = 0; i < OM_NSTR; i++)
            if (om_str_level[i] > 0.2f)
                k |= 1u << WHITE_K[i];
    }
    plat_glow(proj.leds == 1);
    plat_leds(b, k);
}

/* AUTOSAVE: a few seconds after a change, when nothing sounds and nothing is touched (a flash erase
 * silences the audio for a moment) */
#define AUTOSAVE_QUIET 4000u
static void autosave(void)
{
    uint32_t now = plat_ms(), i;
    if (!ui.dirty || om_playing || ui.btn || ui.keys || now - ui.act_t < AUTOSAVE_QUIET || om_chord_level > 0.0f ||
        om_bass_level > 0.0f)
        return;
    for (i = 0; i < OM_NSTR; i++)
        if (om_str_level[i] > 0.0f)
            return;
    if (project_save() == 0) {
        ui.dirty = 0;
        ui.saved_t = now;
    }
}

/* ------------------------------------------------------ screen reader --- */
/* The screen as text for a screen reader on the computer (announce.c; fm1-reader): a page once it settles,
 * with its knobs; a knob's value as it turns; the chord a button (or MIDI) plays; play and stop; what a
 * button did (say). WHERE (the reader asks, or it has just started listening) is everything on the screen.
 * Nothing is built while no reader listens. */
#define SP_NV (P_NPARAMS + 5)                  /* the values watched: the parameters, the button's root and type,
                                                * lights, the palette, the size */
static struct {
    uint32_t ctx;                              /* the page, hashed: a change is said */
    int32_t chord;                             /* the chord said last: button | root | type, -1 none */
    int16_t val[SP_NV];
    int8_t playing, armed;
    uint8_t started;
} sp OM_POOL;
static char sp_buf[ANN_MAX + 1] OM_POOL;
static uint32_t sp_n;
/* the chord types in words: the screen's "m" and "M7" differ only in capitals, which the words file ignores */
static const char *const TYPE_SAID[CH_NTYPES] = {"major", "minor", "7", "major 7", "minor 7", "diminished",
                                                 "augmented", "suspended 4", "add 9"};

static void sp_cat(const char *s)
{
    while (*s && sp_n < ANN_MAX)
        sp_buf[sp_n++] = *s++;
    sp_buf[sp_n] = 0;
}
static int sp_knob_of(int i) { return i < P_NPARAMS ? i : K_PADROOT + (i - P_NPARAMS); }
static void sp_knob(int k)                     /* "Voice 1 80", "Type minor 7" */
{
    char t[16];
    sp_cat(knob_name(k));
    sp_cat(" ");
    if (k == K_PADTYPE) {
        sp_cat(TYPE_SAID[knob_get(k)]);
    } else if (k == K_PALETTE) {                   /* the whole name: a knob holds six characters */
        sp_cat(OM_THEMES[knob_get(k) % NTHEMES].name);
    } else {
        knob_text(k, t);
        sp_cat(t);
    }
}
static void sp_chord(int root, int type)       /* "C# minor 7" (transposed, as the screen shows it) */
{
    sp_cat(OM_NOTE_NAME[((root + proj.par[P_TRANSPOSE]) % 12 + 12) % 12]);
    sp_cat(" ");
    sp_cat(TYPE_SAID[type]);
}

/* the page: its name, and its knobs; all: the header and the chord too */
static void sp_screen(int all)
{
    char t[12];
    int i;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat(VIEW_NAME[ui.view]);
    sp_cat(" page");                           /* "Play page": not the transport's "Play" */
    if (ui.view == V_PADS) {
        sp_cat(", button ");
        itoa_u(ui.pad_sel + 1u, t);
        sp_cat(t);
    }
    for (i = 0; i < 4; i++)
        if (VIEW_KNOB[ui.view][i] != K_NONE) {
            sp_cat(i ? ", " : ": ");
            sp_knob(VIEW_KNOB[ui.view][i]);
        }
    if (!all)
        return;
    sp_cat(". Chord ");                        /* the big name in the middle, then the header */
    sp_chord(ui.root, ui.type);
    sp_cat(". Rhythm ");
    sp_cat(om_rhythm(proj.par[P_RHYTHM])->name);
    sp_cat(", tempo ");
    if (om_ext) {
        sp_cat("EXT");
    } else {
        itoa_u((uint32_t)proj.par[P_TEMPO], t);
        sp_cat(t);
    }
    sp_cat(om_playing ? ", playing" : ui.armed ? ", waiting for a chord" : ", stopped");
    if (proj.hold)
        sp_cat(". Chord hold on");
    if (proj.sync)
        sp_cat(". Sync start on");
}

static uint32_t sp_ctx_sig(void) { return hash(hash(2166136261u, ui.view), ui.view == V_PADS ? ui.pad_sel : 0u); }
static int32_t sp_chord_now(void) { return ui.pad == -1 ? -1 : (int32_t)((uint8_t)ui.pad | ui.root << 8 | ui.type << 16); }
static void sp_snap(void)
{
    int i;
    for (i = 0; i < SP_NV; i++)
        sp.val[i] = (int16_t)knob_get(sp_knob_of(i));
    sp.chord = sp_chord_now();
    sp.playing = (int8_t)om_playing;
    sp.armed = (int8_t)ui.armed;
}

static void speak(void)
{
    uint32_t now = plat_ms(), c;
    int where = ann_where(), i, changed = -1;
    int32_t ch;
    if (!ann_on()) {
        sp.started = 0;
        sp_said = 0;
        return;
    }
    if (!sp.started || where) {                       /* everything on the screen */
        sp.started = 1;
        sp.ctx = sp_ctx_sig();
        sp_snap();
        sp_screen(1);
        ann_say(sp_buf, 0, ANN_NOW);
        sp_said = 0;
        return;
    }
    if (sp_said) {                                    /* a button said what it did (and what it changed) */
        sp_said = 0;
        sp_snap();
        return;
    }
    if (om_playing != sp.playing) {
        sp.playing = (int8_t)om_playing;
        ann_say(om_playing ? "Play" : "Stop", 0, ANN_NOW);
    }
    c = sp_ctx_sig();
    if (c != sp.ctx) {                                /* a new page: said when it settles */
        sp.ctx = c;
        sp_snap();
        sp_screen(0);
        ann_later(sp_buf, ANN_NOW, 200u, now);
        return;
    }
    for (i = 0; i < SP_NV; i++) {                     /* a knob turning (or an encoder) */
        int v = knob_get(sp_knob_of(i));
        if (v != sp.val[i]) {
            sp.val[i] = (int16_t)v;
            if (changed < 0)
                changed = i;
        }
    }
    ch = sp_chord_now();
    if (changed >= 0) {
        sp.chord = ch;                                /* (a root or a type turned: the readout says it) */
        sp_n = 0;
        sp_knob(sp_knob_of(changed));
        ann_later(sp_buf, ANN_NOW, 120u, now);
        return;
    }
    if (ch != sp.chord) {                             /* a chord played: its name */
        sp.chord = ch;
        if (ch >= 0) {
            sp_n = 0;
            sp_chord(ui.root, ui.type);
            ann_say(sp_buf, 0, ANN_NOW);
        }
    }
}

#ifdef OM_HOST
/* every page, knob value, chord and message as a screen reader gets them (the "speakdump" command):
 * KIND <tab> the text sent (fm1-reader/tools/audit.mjs puts it in words with web/words/ and checks it) */
static void sp_print(const char *kind, const char *t) { printf("%s\t%s\n", kind, t); }
static void speak_dump(void)
{
    static const char *const MSG[][2] = {{"UPDATE MODE IN", "3.."}, {"UPDATE MODE", "CANCELLED"}};
    uint8_t keep_view = ui.view, keep_sel = ui.pad_sel, keep_ext = om_ext;
    project_t keep = proj;
    int v, q, i, x, lo, hi;
    for (v = 0; v < NVIEWS; v++)
        for (q = 0; q < (v == V_PADS ? NPADS : 1); q++) {
            ui.view = (uint8_t)v;
            ui.pad_sel = (uint8_t)q;
            sp_screen(0);
            sp_print("SCREEN", sp_buf);
            for (i = 0; i < 4; i++) {
                int k = VIEW_KNOB[v][i];
                if (k == K_NONE || (v == V_PADS && q))
                    continue;
                knob_range(k, &lo, &hi);
                for (x = lo; x <= hi; x++) {
                    if (k < P_NPARAMS)
                        proj.par[k] = (int16_t)x;
                    else if (k == K_PADROOT)
                        proj.pad_root[0] = (uint8_t)x;
                    else if (k == K_PADTYPE)
                        proj.pad_type[0] = (uint8_t)x;
                    else
                        proj.leds = (uint8_t)x;
                    sp_n = 0;
                    sp_knob(k);
                    sp_print("KNOB", sp_buf);
                }
                proj = keep;
            }
        }
    ui.view = V_PLAY;
    for (x = 0; x < 4; x++) {                         /* WHERE: stopped, playing, waiting, the clock from MIDI */
        proj.hold = proj.sync = (uint8_t)(x & 1);
        om_ext = (uint8_t)(x == 3);
        om_playing = (uint8_t)(x == 1 || x == 3);
        ui.armed = (uint8_t)(x == 2);
        sp_screen(1);
        sp_print("WHERE", sp_buf);
    }
    proj = keep;
    om_ext = keep_ext;
    om_playing = 0;
    ui.armed = 0;
    for (x = 0; x < 12; x++)
        for (i = 0; i < CH_NTYPES; i++) {
            sp_n = 0;
            sp_chord(x, i);
            sp_print("CHORD", sp_buf);
        }
    for (i = 0; i < (int)(sizeof MSG / sizeof MSG[0]); i++) {   /* main_fm1.c's (not in the simulator) */
        msg_text(MSG[i][0], MSG[i][1], sp_buf);
        sp_print("MSG", sp_buf);
    }
    ui.view = keep_view;
    ui.pad_sel = keep_sel;
}
#endif

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    ui.pad = -1;
    ui.touched = -1;
    ui.root = proj.pad_root[1];                  /* C, the second button */
    ui.type = proj.pad_type[1];
    theme_set(proj.theme);                       /* the palette saved with the project */
    sound_chord();
    lcd_fill(0, 0, 240, 240, K_BG);
}

void ui_frame(void)
{
    input();
    autosave();
    draw_header();
    draw_main();
    draw_knobs();
    leds();
    speak();
    ann_tick(plat_ms());
    ui.frame++;
}

void ui_input_only(void) { input(); }
