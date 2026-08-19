#!/usr/bin/env python3
"""Regenerate resources/misc/dmg-background.png.
Requires: pillow, and rsvg-convert (brew install librsvg).

Positions are in the same point space as build_dmg.sh's --window-size/--icon
flags and must stay in lockstep with them: 660x420pt window, guide boxes at
x=150/510 y=260.

Saved at 1320x840px tagged 144dpi, because create-dmg sizes the background by
pixels/dpi*72 rather than scaling it to the window -- a default 72dpi save comes
out at literal pixel size. YSHIFT offsets the artwork for the window chrome,
which draws it lower than its canvas position.
"""
import subprocess
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent.parent
OUT = HERE / "resources/misc/dmg-background.png"

SCALE = 2
W_PT, H_PT = 660, 420
W, H = W_PT * SCALE, H_PT * SCALE
YSHIFT = -27

BG = (214, 233, 250)
DARK = (55, 65, 75)
GRAY = (110, 122, 134)


def px(v):
    return round(v * SCALE)


def pxy(v):
    return round((v + YSHIFT) * SCALE)


def find_logo_svg():
    # Lives in the libminizinc repo this one is normally checked out
    # alongside; fall back to searching upward in case that is not the case.
    candidates = [
        HERE / "../libminizinc/docs/logo/MiniZn_logo_2.svg",
        HERE / "../docs/logo/MiniZn_logo_2.svg",
    ]
    for c in candidates:
        if c.resolve().exists():
            return c.resolve()
    raise SystemExit(
        "Could not find MiniZn_logo_2.svg (checked libminizinc/docs/logo/ "
        "next to this checkout). Pass its path as this script's first arg."
    )


def render_logo(svg_path, size_px):
    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as f:
        out = Path(f.name)
    subprocess.run(
        ["rsvg-convert", "-w", str(size_px), "-h", str(size_px), str(svg_path), "-o", str(out)],
        check=True,
    )
    return Image.open(out).convert("RGBA")


def main():
    import sys

    svg_path = Path(sys.argv[1]) if len(sys.argv) > 1 else find_logo_svg()

    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im, "RGBA")

    logo_size = px(84)
    logo = render_logo(svg_path, logo_size)
    logo_pos = (px(56), pxy(40))
    im.paste(logo, logo_pos, logo)

    bold = ImageFont.truetype("/System/Library/Fonts/HelveticaNeue.ttc", px(26), index=1)
    reg = ImageFont.truetype("/System/Library/Fonts/HelveticaNeue.ttc", px(15), index=0)
    text_x = logo_pos[0] + logo_size + px(20)
    d.text((text_x, pxy(46)), "Welcome to the MiniZinc IDE", font=bold, fill=DARK)
    d.text((text_x, pxy(46) + px(34)), "Drag the icon into Applications to install.", font=reg, fill=GRAY)

    d.line([(px(40), pxy(150)), (W - px(40), pxy(150))], fill=(255, 255, 255, 140), width=px(1))

    icon_y = pxy(260)
    box = px(130)
    left_x, right_x = px(150), px(510)

    def guide_box(cx, cy):
        r = box // 2
        d.rounded_rectangle(
            [cx - r, cy - r, cx + r, cy + r], radius=px(22), fill=(255, 255, 255, 90), outline=(255, 255, 255, 160), width=px(2)
        )

    guide_box(left_x, icon_y)
    guide_box(right_x, icon_y)

    ax0, ax1 = left_x + box // 2 + px(14), right_x - box // 2 - px(14)
    ay = icon_y
    d.line([(ax0, ay), (ax1 - px(18), ay)], fill=(255, 255, 255, 220), width=px(6))
    d.polygon([(ax1 - px(18), ay - px(16)), (ax1 - px(18), ay + px(16)), (ax1, ay)], fill=(255, 255, 255, 220))

    im.save(OUT, dpi=(144, 144))
    print(f"wrote {OUT} ({im.size[0]}x{im.size[1]}px @144dpi = {W_PT}x{H_PT}pt)")


if __name__ == "__main__":
    main()
