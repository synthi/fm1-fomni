#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check the screen's palettes (firmware/src/app/palettes.h): WCAG contrast of every pair the UI draws,
and that each colour survives RGB565 (the panel takes 5-6-5 bits, and the derived tints are blended in
that space, exactly as ui.c does).

  tools/check_palettes.py            the table, and a failure list; exit 1 on any miss
  tools/check_palettes.py --quiet    only the failures

The pairs are the ones the screen really draws: a value on the paper, a label, the rhythm's name, the
chord type under a pad, and text on a pale tint (a pad at rest), on a solid accent (a lit pad, a pill)
and on the pale tint of a chord pad. 7:1 (AAA) for the text on the paper, 4.5:1 (AA) for the rest: the
FM-1's screen is 240 x 240 and the reader may be a tired eye, so nothing sits at 3:1.
"""
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parents[1]
SRC = HERE / "firmware" / "src" / "app" / "palettes.h"

ROW = re.compile(r'\{"([^"]+)",\s*"([^"]+)",\s*\{((?:\{\s*\d+\s*,\s*\d+\s*,\s*\d+\s*\},?\s*){8})\}')
TRIPLE = re.compile(r'\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}')

# the project stores the palette's index: a reordering would silently change every saved device's screen
FIRST_NAME = "CREAM"


def to565(c):
    return (c[0] >> 3, c[1] >> 2, c[2] >> 3)


def from565(p):
    r, g, b = p
    return (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)


def pack(c):
    """8-bit RGB -> what the screen shows (the RGB565 round trip)"""
    return from565(to565(c))


def mix(a, b, t):
    """ui.c's mix(): t 0..16, in RGB565, channel by channel (the firmware's own arithmetic)"""
    pa, pb = to565(a), to565(b)
    mx = (31, 63, 31)
    out = []
    for i in range(3):
        v = (pa[i] * (16 - t) + pb[i] * t) // 16
        out.append(v if v < mx[i] else mx[i])
    return from565(tuple(out))


def lin(c):
    c /= 255.0
    return c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4


def lum(c):
    return 0.2126 * lin(c[0]) + 0.7152 * lin(c[1]) + 0.0722 * lin(c[2])


def contrast(a, b):
    la, lb = lum(a), lum(b)
    hi, lo = max(la, lb), min(la, lb)
    return (hi + 0.05) / (lo + 0.05)


def parse():
    text = SRC.read_text(encoding="utf-8")
    out = []
    for m in ROW.finditer(text):
        cols = [(int(r), int(g), int(b)) for r, g, b in TRIPLE.findall(m.group(3))]
        out.append((m.group(1), m.group(2), cols))
    return out


def main(argv):
    quiet = "--quiet" in argv
    themes = parse()
    if not themes:
        sys.exit(f"{SRC}: no palettes found (the table's shape changed)")
    if themes[0][0] != FIRST_NAME:
        sys.exit(f"{SRC}: palette 0 is {themes[0][0]!r}: the project stores indices, so {FIRST_NAME} "
                 f"(the light one: an old project's theme 0) has to stay first")
    fails, worst_delta = 0, 0
    if not quiet:
        print(f"{'palette':<16}{'text':>6}{'dim':>6}{'harp':>6}{'chord':>6}{'rhy':>6}"
              f"{'txt/sf':>7}{'tint':>6}{'lit':>6}{'565':>5}")
    for name, _short, cols in themes:
        bg, surf, text, dim, harp, chord, rhy, ink = [pack(c) for c in cols]
        harp_t, chord_t, rhy_t = mix(bg, harp, 5), mix(bg, chord, 5), mix(bg, rhy, 5)
        checks = [
            ("text on the paper", text, bg, 7.0),
            ("text on a panel", text, surf, 7.0),
            ("labels on the paper", dim, bg, 4.5),
            ("the strings", harp, bg, 4.5),
            ("the chord", chord, bg, 4.5),
            ("the rhythm", rhy, bg, 4.5),
            ("text on a tint", text, harp_t, 4.5),
            ("text on a chord tint", text, chord_t, 4.5),
            ("text on a rhythm tint", text, rhy_t, 4.5),
            ("ink on the strings", ink, harp, 4.5),
            ("ink on a chord pad", ink, chord, 4.5),
            ("ink on the rhythm", ink, rhy, 4.5),
        ]
        bad = [(what, contrast(a, b), want) for what, a, b, want in checks if contrast(a, b) < want]
        delta = max(max(abs(pack(c)[i] - c[i]) for i in range(3)) for c in cols)
        worst_delta = max(worst_delta, delta)
        if not quiet:
            t = {what: r for what, a, b, _ in checks for r in [contrast(a, b)]}
            tint = min(contrast(text, harp_t), contrast(text, chord_t), contrast(text, rhy_t))
            lit = min(contrast(ink, harp), contrast(ink, chord), contrast(ink, rhy))
            print(f"{name:<16}{t['text on the paper']:>6.1f}{t['labels on the paper']:>6.1f}"
                  f"{t['the strings']:>6.1f}{t['the chord']:>6.1f}{t['the rhythm']:>6.1f}"
                  f"{t['text on a panel']:>7.1f}{tint:>6.1f}{lit:>6.1f}{delta:>5}")
        for what, got, want in bad:
            fails += 1
            print(f"  FAIL {name}: {what} {got:.2f} < {want} (WCAG)")
    print(f"palettes: {len(themes)}, WCAG 7:1 for the text and 4.5:1 for the rest, "
          f"worst RGB565 shift {worst_delta} of 255")
    if fails:
        print(f"PALETTES: {fails} FAILED")
        return 1
    print("palettes: all pass")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
