"""Draw the Billboard badge showing a PebbleOS frame, as the port displays it.

usage: render_badge.py FRAME.png OUT.png

FRAME is a 144x168 Flint frame (from the simulator: pebbleos -S frame.bmp).
The panel is 960x672; the port draws the frame 4x (576x672), centred.
"""
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

frame_path, out_path = sys.argv[1], sys.argv[2]

S = 2  # supersampling
PANEL_W, PANEL_H = 960, 672
BEZEL = 70
STRIP_H = 150
W = PANEL_W + 2 * BEZEL + 2 * 90
H = PANEL_H + 2 * BEZEL + STRIP_H + 2 * 70

PAPER = (232, 229, 219)
INK = (28, 28, 30)
FRAME = (38, 38, 40)
FRAME_HI = (58, 58, 62)
PCB = (18, 18, 20)
SILK = (225, 225, 220)
GOLD = (196, 160, 92)
BG_TOP = (236, 238, 242)
BG_BOT = (205, 210, 218)


def font(size, bold=False):
    names = [
        "/System/Library/Fonts/Supplemental/Arial Bold.ttf" if bold else
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
    ]
    for n in names:
        try:
            return ImageFont.truetype(n, size * S)
        except OSError:
            pass
    return ImageFont.load_default()


img = Image.new("RGB", (W * S, H * S), BG_TOP)
d = ImageDraw.Draw(img)

# Background gradient.
for y in range(H * S):
    t = y / (H * S)
    c = tuple(int(a + (b - a) * t) for a, b in zip(BG_TOP, BG_BOT))
    d.line([(0, y), (W * S, y)], fill=c)

bx0, by0 = 90, 70
bx1, by1 = W - 90, H - 70

# Shadow.
shadow = Image.new("L", img.size, 0)
ImageDraw.Draw(shadow).rounded_rectangle(
    [(bx0 + 14) * S, (by0 + 22) * S, (bx1 + 14) * S, (by1 + 22) * S], 40 * S, fill=150)
shadow = shadow.filter(ImageFilter.GaussianBlur(28 * S))
img.paste((60, 64, 72), (0, 0), shadow)
d = ImageDraw.Draw(img)

# PCB strip along the top, behind the frame.
sy0, sy1 = by0, by0 + STRIP_H
d.rounded_rectangle([bx0 * S, sy0 * S, bx1 * S, (sy1 + 40) * S], 28 * S, fill=PCB)
# Hazard stripes at the ends, each clipped to its own box.
SW, SH = 90, 40
tile = Image.new("RGB", (SW * S, SH * S), PCB)
td = ImageDraw.Draw(tile)
for i in range(-4, 10):
    x = i * 14
    td.polygon([(x * S, 0), ((x + 7) * S, 0), ((x + 7 + SH) * S, SH * S), ((x + SH) * S, SH * S)],
               fill=SILK)
for x0 in (bx0 + 30, bx1 - 30 - SW):
    img.paste(tile, (x0 * S, (sy0 + 20) * S))
d.text(((bx0 + 140) * S, (sy0 + 16) * S), "HACKADAY", font=font(26, True), fill=SILK)
d.text(((bx0 + 140) * S, (sy0 + 46) * S), "SUPERCON 2026", font=font(26, True), fill=SILK)
# LED row.
for i in range(8):
    cx = (W / 2 - 175 + i * 50)
    d.rounded_rectangle([(cx - 9) * S, (sy0 + 26) * S, (cx + 9) * S, (sy0 + 44) * S], 3 * S,
                        fill=(250, 250, 245))
    d.ellipse([(cx - 4) * S, (sy0 + 31) * S, (cx + 4) * S, (sy0 + 39) * S], fill=(255, 214, 120))
# Buttons, labelled as the port maps them.
labels = [("MENU +", "Up"), ("MENU -", "Down"), ("A", "Select"), ("B", "Back")]
for i, (name, role) in enumerate(labels):
    cx = bx1 - 400 + i * 80
    d.ellipse([(cx - 16) * S, (sy0 + 64) * S, (cx + 16) * S, (sy0 + 96) * S], fill=(70, 70, 74))
    d.ellipse([(cx - 11) * S, (sy0 + 69) * S, (cx + 11) * S, (sy0 + 91) * S], fill=(110, 110, 116))
    d.text((cx * S, (sy0 + 108) * S), name, font=font(13, True), fill=SILK, anchor="mt")
d.ellipse([(bx0 + 40) * S, (sy0 + 70) * S, (bx0 + 76) * S, (sy0 + 106) * S], outline=GOLD,
          width=3 * S)

# Frame around the panel.
fx0, fy0 = bx0, sy1 - 10
fx1, fy1 = bx1, by1
d.rounded_rectangle([fx0 * S, fy0 * S, fx1 * S, fy1 * S], 30 * S, fill=FRAME)
d.rounded_rectangle([(fx0 + 4) * S, (fy0 + 4) * S, (fx1 - 4) * S, (fy1 - 4) * S], 26 * S,
                    outline=FRAME_HI, width=2 * S)
# Speckled 3D-print texture.
import random
random.seed(7)
for _ in range(9000):
    x = random.uniform(fx0 + 6, fx1 - 6)
    y = random.uniform(fy0 + 6, fy1 - 6)
    if fx0 + BEZEL - 8 < x < fx1 - BEZEL + 8 and fy0 + BEZEL - 8 < y < fy1 - BEZEL + 8:
        continue
    g = random.randint(50, 80)
    d.point((x * S, y * S), fill=(g, g, g + 4))

# Panel.
px0 = (W - PANEL_W) // 2
py0 = fy0 + BEZEL
d.rectangle([(px0 - 6) * S, (py0 - 6) * S, (px0 + PANEL_W + 6) * S, (py0 + PANEL_H + 6) * S],
            fill=(20, 20, 22))
paper = Image.new("RGB", (PANEL_W, PANEL_H), PAPER)
frame = Image.open(frame_path).convert("L").resize((144 * 4, 168 * 4), Image.NEAREST)
mask = frame.point(lambda v: 255 if v < 128 else 0)
paper.paste(INK, ((PANEL_W - 576) // 2, (PANEL_H - 672) // 2), mask)
# A little e-paper grain.
grain = Image.effect_noise((PANEL_W, PANEL_H), 6).convert("RGB")
paper = Image.blend(paper, Image.composite(paper, grain, Image.new("L", paper.size, 235)), 0.25)
img.paste(paper.resize((PANEL_W * S, PANEL_H * S), Image.NEAREST), (px0 * S, py0 * S))

img = img.resize((W, H), Image.LANCZOS)
img.save(out_path, optimize=True)
print(out_path, img.size)
