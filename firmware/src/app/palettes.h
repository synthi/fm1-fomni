/* SPDX-License-Identifier: GPL-3.0-only */
/* FoMni-1 LALI: the screen's palettes. Eight colours each, 8-bit RGB (the panel takes RGB565):
 *
 *   BG     the paper              SURF   panels, pills, the knob band's card
 *   TEXT   values, the chord      DIM    labels, units, what is off
 *   HARP   the strings            CHORD  the chord pads      RHY  the rhythm
 *   INK    text on a coloured fill (a lit pad, a pill)
 *
 * The rest is derived in ui.c (mix): LINE = 18 % of TEXT over BG, and the three pale tints
 * (a pad at rest, an empty step) = 30 % of their colour over BG.
 *
 * Every pair is checked by tools/check_palettes.py (WCAG contrast, >= 4.5:1, and that the colour
 * survives RGB565); tests/run_tests.sh runs it. The dark ones come first, and they are the point:
 * the FM-1's screen is 240 x 240 and small, so a bright background washes the letters out. The five
 * pale papers at the end are the exception, with dark letters, for a room in daylight.
 *
 * The W. ones are Sanzo Wada's: the hues of a combination from A Dictionary of Color Combinations
 * (1933-34, 348 combinations of two to four colours, drawn from 159), lifted in value and
 * saturation the way a 240 px screen needs and the checker insists on, and named W. for Wada since
 * the page has little room. Each row names its combination. The site's data (hex) is spectro.dev's,
 * from Matt DesLauriers and Dain M. Blodorn Kim (MIT). SAKURA is one of them too (No. 273), which is
 * what the old pastel P SAKURA became.
 *
 * Add a palette at the END (the project stores its index; old projects have 0, CREAM). */
#pragma once
#include <stdint.h>

#define TC_BG 0
#define TC_SURF 1
#define TC_TEXT 2
#define TC_DIM 3
#define TC_HARP 4
#define TC_CHORD 5
#define TC_RHY 6
#define TC_INK 7

typedef struct {
    const char *name;       /* as the Setup 2 page shows it, and a screen reader says it */
    const char *short_name; /* on a knob: what fits 54 px (six characters or so) */
    uint8_t c[8][3];
} om_theme_t;

/* name                 short     BG              SURF            TEXT            DIM             HARP            CHORD           RHY             INK   */
static const om_theme_t OM_THEMES[] = {
    {"CREAM",         "CREAM",  {{250, 244, 232}, {238, 231, 216}, {58, 46, 38},   {110, 94, 78},  {188, 68, 32},  {26, 124, 116}, {186, 52, 92},  {255, 255, 255}}},
    {"AUBERGINE",     "AUBERG", {{24, 14, 30},    {40, 26, 48},    {244, 230, 210}, {200, 180, 208}, {242, 160, 122}, {127, 216, 200}, {232, 139, 177}, {20, 12, 26}}},
    {"NIGHT ROSE",    "ROSE",   {{26, 11, 17},    {44, 20, 30},    {250, 228, 234}, {200, 158, 172}, {255, 158, 122}, {111, 227, 210}, {255, 123, 166}, {26, 11, 17}}},
    {"MIDNIGHT",      "MIDNT",  {{10, 14, 30},    {22, 28, 48},    {238, 230, 214}, {178, 188, 212}, {240, 190, 110}, {96, 214, 204},  {235, 140, 160}, {10, 14, 30}}},
    {"OLIVE",         "OLIVE",  {{20, 22, 14},    {32, 36, 24},    {237, 230, 211}, {182, 192, 160}, {214, 196, 120}, {150, 200, 140}, {226, 150, 120}, {20, 22, 14}}},
    {"GRAPHITE",      "GRAPHT", {{16, 16, 18},    {28, 28, 32},    {236, 236, 240}, {178, 178, 186}, {226, 226, 232}, {196, 196, 204}, {160, 160, 170}, {16, 16, 18}}},
    {"WINE",          "WINE",   {{30, 10, 14},    {46, 20, 24},    {244, 226, 206}, {198, 166, 160}, {230, 120, 110}, {150, 200, 190}, {230, 160, 120}, {30, 10, 14}}},
    {"PETROL",        "PETROL", {{8, 20, 26},     {18, 34, 42},    {230, 226, 210}, {170, 196, 204}, {240, 180, 120}, {110, 210, 200}, {200, 150, 190}, {8, 20, 26}}},
    {"FOREST",        "FOREST", {{10, 20, 16},    {20, 34, 28},    {235, 232, 215}, {172, 196, 180}, {235, 190, 120}, {140, 210, 170}, {215, 150, 160}, {10, 20, 16}}},
    {"COCOA",         "COCOA",  {{22, 14, 10},    {38, 26, 20},    {242, 228, 206}, {198, 178, 162}, {240, 170, 120}, {150, 205, 180}, {225, 150, 150}, {22, 14, 10}}},
    {"INDIGO",        "INDIGO", {{14, 12, 32},    {24, 22, 48},    {232, 226, 246}, {176, 172, 210}, {150, 190, 255}, {120, 220, 210}, {240, 160, 200}, {14, 12, 32}}},
    {"AMBER NIGHT",   "AMBER",  {{18, 13, 6},     {32, 24, 12},    {246, 226, 180}, {190, 172, 140}, {255, 190, 90},  {130, 215, 200}, {240, 150, 140}, {18, 13, 6}}},
    {"MONO",          "MONO",   {{0, 0, 0},       {18, 18, 18},    {255, 255, 255}, {170, 170, 170}, {255, 255, 255}, {212, 212, 212}, {160, 160, 160}, {0, 0, 0}}},
    {"HIGH CONTRAST", "HI-CON", {{0, 0, 0},       {10, 10, 10},    {255, 255, 255}, {190, 190, 190}, {255, 194, 77},  {0, 229, 192},   {255, 77, 141},  {0, 0, 0}}},
    {"P ROSE",        "P-ROSE", {{6, 2, 5},       {34, 14, 26},    {252, 232, 242}, {200, 150, 175}, {244, 114, 182}, {140, 205, 225}, {255, 214, 110}, {6, 2, 5}}},
    {"P VIOLET",      "P-VIOL", {{4, 2, 9},       {24, 18, 42},    {240, 232, 252}, {170, 150, 200}, {178, 136, 246}, {110, 228, 168}, {250, 190, 140}, {4, 2, 9}}},
    {"SAKURA",        "SAKURA", {{38, 6, 18},     {60, 31, 42},    {254, 240, 243}, {171, 150, 157}, {235, 176, 155}, {101, 202, 224}, {251, 162, 183}, {29, 5, 14}}},     /* Wada 273 */
    {"P TERRA",       "P-TERR", {{0, 0, 0},       {30, 16, 4},     {248, 238, 222}, {170, 140, 110}, {255, 138, 32},  {255, 208, 120}, {120, 200, 220}, {0, 0, 0}}},
    {"P AQUA",        "P-AQUA", {{0, 0, 0},       {4, 20, 26},     {226, 246, 248}, {140, 170, 180}, {52, 224, 208},  {128, 176, 255}, {255, 190, 120}, {0, 0, 0}}},
    /* Sanzo Wada's combinations (the number is the one in A Dictionary of Color Combinations).
     * Two families: the paper dark (the first three, with the rest) and the paper pale (the last
     * five). No two repeat a paper's hue; each accent set is the combination's own. */
    {"W. CARMINE",    "CARMIN", {{ 16,  18,  19}, { 40,  42,  43}, {255, 252, 191}, {158, 158, 122}, { 50, 224, 132}, {235, 102, 128}, {255, 242,   0}, { 12,  14,  14}}},   /* Wada 313: Carmine, Yellow, Diamine Green, Black */
    {"W. SALVIA",     "SALVIA", {{ 15,  20,  14}, { 39,  44,  38}, {252, 250, 240}, {156, 157, 148}, {235, 158, 155}, {155, 189, 235}, {245, 221, 110}, { 10,  14,  10}}},   /* Wada 321: Light Brown Drab, Sulphur Yellow, Deep Slate Olive, Salvia Blue */
    {"W. VIOLET",     "VIOLET", {{ 22,  11,  46}, { 45,  35,  67}, {255, 252, 191}, {162, 156, 133}, { 89, 176, 235}, {253, 191, 104}, {255, 242,   0}, { 17,   8,  35}}},   /* Wada 295: Cream Yellow, Yellow, Blue, Dull Violet Black */
    {"W. COTINGA",    "COTING", {{ 34,   8,  29}, { 56,  33,  52}, {253, 250, 208}, {167, 154, 137}, {101, 188, 224}, {246, 140,  80}, {248, 237,  67}, { 26,   6,  22}}},   /* Wada 253: Lemon Yellow, Apricot Orange, Cotinga Purple, Slate Color */
    {"W. UMBER",      "UMBER",  {{ 29,  15,   4}, { 52,  39,  29}, {255, 251, 235}, {164, 155, 141}, {155, 189, 235}, {101, 224, 170}, {255, 227, 115}, { 21,  11,   3}}},   /* Wada 272: Pale Lemon Yellow, Orange, Turquoise Green, Salvia Blue */
    {"W. EUGENIA",    "EUGEN",  {{  0,  22,  19}, { 26,  45,  43}, {255, 246, 191}, {156, 159, 124}, {236, 153, 150}, {  0, 224, 193}, {255, 221,   0}, {  0,  16,  13}}},   /* Wada 284: Eugenia Red A, Apricot Yellow, Sea Green, Dusky Green */
    {"HIGH ROSE",     "HI-ROS", {{6, 2, 8},       {26, 10, 32},    {255, 240, 248}, {208, 160, 196}, {255, 79, 163},  {185, 139, 255}, {255, 201, 60},  {6, 2, 8}}},      /* Wada 170's hues (Rosolane Purple, Red Violet, Orange Yellow), at HIGH CONTRAST */
    /* the dark papers: a night blue, a plum and a warm black, which the wheel did not have yet */
    {"W. TYRIAN",     "TYRIAN", {{ 16,  17,  27}, { 40,  41,  50}, {236, 246, 249}, {150, 157, 162}, {103, 195, 228}, {212, 105, 154}, { 99, 204,  78}, { 12,  12,  20}}},   /* Wada 75: Pale King's Blue, Violet Blue */
    {"W. EOSINE",     "EOSINE", {{ 26,  15,  20}, { 49,  39,  44}, {239, 238, 246}, {159, 155, 161}, {142, 133, 225}, {204, 149,  47}, {243, 109, 134}, { 19,  11,  14}}},   /* Wada 248: Eosine Pink, Khaki, Grayish Lavender A, Dark Slate Purple */
    {"W. KHAKI",      "KHAKI",  {{ 19,  18,  11}, { 43,  42,  35}, {240, 229, 208}, {162, 155, 139}, { 46, 204, 120}, {191, 117, 215}, {201, 204,  78}, { 14,  14,   8}}},   /* Wada 146: Khaki, Deep Grayish Olive, Diamine Green */
    /* the pale papers: the same eight roles, the letters dark (the checker holds them to 7:1 too) */
    {"W. SPINEL",     "SPINEL", {{255, 204, 216}, {241, 193, 204}, { 49,  27,  35}, { 89,  61,  70}, { 44,  87,  97}, {133,  33,  87}, { 43,  97,  24}, {255, 252, 253}}},   /* Wada 224: Spinel Red, Dark Medici Blue, Taupe Brown */
    {"W. LILAC",      "LILAC",  {{221, 204, 255}, {209, 193, 241}, { 52,  24,  49}, { 83,  56,  86}, {120,  54, 112}, { 61,  87,  22}, { 33,  66, 133}, {253, 252, 255}}},   /* Wada 9: Violet Red, Dull Blue Violet */
    {"W. MINT",       "MINT",   {{183, 229, 198}, {173, 216, 187}, { 36,  29,  62}, { 65,  69,  89}, { 75,  60, 133}, { 97,  82,  28}, { 37,  94,  97}, {251, 253, 252}}},   /* Wada 156: Olive Ocher, Cobalt Green, Violet */
    {"W. BUFF",       "BUFF",   {{221, 224, 179}, {209, 212, 169}, { 65,  17,  22}, { 99,  62,  56}, {133,  35,  44}, { 45,  97,  44}, { 58,  33, 133}, {253, 253, 250}}},   /* Wada 200: Carmine Red, Olive Buff, Chromium Green */
    {"W. GLAUCOUS",   "GLAUC",  {{187, 225, 234}, {177, 213, 221}, { 12,  36,  53}, { 47,  73,  89}, {133,  51,  94}, { 52,  97,  37}, { 53,  51, 133}, {251, 253, 254}}},   /* Wada 119: Light Glaucous Blue, Dark Tyrian Blue */
};
#define NTHEMES (sizeof OM_THEMES / sizeof OM_THEMES[0])
#define THEME_DEFAULT 1u               /* AUBERGINE: a fresh FM-1 starts dark */
