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
 * the FM-1's screen is 240 x 240 and small, so a bright background washes the letters out.
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
    {"P SAKURA",      "P-SAKU", {{4, 2, 4},       {34, 26, 34},    {240, 228, 236}, {180, 150, 165}, {226, 170, 196}, {190, 224, 200}, {255, 200, 140}, {4, 2, 4}}},
    {"P TERRA",       "P-TERR", {{0, 0, 0},       {30, 16, 4},     {248, 238, 222}, {170, 140, 110}, {255, 138, 32},  {255, 208, 120}, {120, 200, 220}, {0, 0, 0}}},
    {"P AQUA",        "P-AQUA", {{0, 0, 0},       {4, 20, 26},     {226, 246, 248}, {140, 170, 180}, {52, 224, 208},  {128, 176, 255}, {255, 190, 120}, {0, 0, 0}}},
};
#define NTHEMES (sizeof OM_THEMES / sizeof OM_THEMES[0])
#define THEME_DEFAULT 1u               /* AUBERGINE: a fresh FM-1 starts dark */
