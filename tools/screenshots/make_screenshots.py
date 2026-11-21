"""Convert rendered frames to PNG and compose the navigation map."""
import os, sys, glob
from PIL import Image, ImageDraw, ImageFont

src, dst = sys.argv[1], sys.argv[2]
shots = {}
for p in sorted(glob.glob(os.path.join(src, "*.ppm"))):
    name = os.path.splitext(os.path.basename(p))[0]
    im = Image.open(p).convert("RGB")
    im.save(os.path.join(dst, name + ".png"))
    shots[name] = im
print(f"{len(shots)} screenshots -> {dst}")

GREEN, DIM, BG = (0, 255, 65), (0, 143, 17), (0, 0, 0)
try:
    font = ImageFont.truetype("DejaVuSansMono-Bold.ttf", 22)
    small = ImageFont.truetype("DejaVuSansMono.ttf", 16)
except OSError:
    font = small = ImageFont.load_default()

S = 0.5                       # thumbnail scale (240x160)
TW, TH = int(480 * S), int(320 * S)
GX, GY = 90, 70               # gaps
# grid positions (col, row) of each page in the navigation map
layout = {
    "01_splash": (0, 0), "02_mesh_public_channel": (1, 0), "03_mesh_keyboard": (2, 0), "04_attach_menu": (3, 0),
    "05_doodle": (4, 0),
    "08_private_chat": (0, 1), "07_peers": (1, 1), "06_image_viewer": (2, 1), "10_sd_image_picker": (4, 1),
    "09_voice_recording": (0, 2), "11_identity": (1, 2), "12_keys_hidden": (2, 2), "13_keys_revealed": (3, 2),
    "14_key_import": (4, 2),
    "15_settings": (1, 3), "16_settings_bottom": (2, 3), "17_panic_confirm": (3, 3), "18_screensaver": (4, 3),
}
edges = [
    ("01_splash", "02_mesh_public_channel", "3s"),
    ("02_mesh_public_channel", "03_mesh_keyboard", "tap input"),
    ("03_mesh_keyboard", "04_attach_menu", "[+]"),
    ("04_attach_menu", "05_doodle", "DOODLE"),
    ("04_attach_menu", "10_sd_image_picker", "SD CARD"),
    ("02_mesh_public_channel", "06_image_viewer", "tap image"),
    ("02_mesh_public_channel", "07_peers", "PEERS"),
    ("07_peers", "08_private_chat", "tap node"),
    ("08_private_chat", "09_voice_recording", "hold mic"),
    ("07_peers", "11_identity", "ID"),
    ("11_identity", "12_keys_hidden", "KEYS"),
    ("12_keys_hidden", "13_keys_revealed", "REVEAL"),
    ("13_keys_revealed", "14_key_import", "import"),
    ("11_identity", "15_settings", "CFG"),
    ("15_settings", "16_settings_bottom", "scroll"),
    ("16_settings_bottom", "17_panic_confirm", "PANIC"),
    ("17_panic_confirm", "18_screensaver", "idle"),
]
cols, rows = 5, 4
W = 60 + cols * TW + (cols - 1) * GX + 60
H = 110 + rows * (TH + 40) + (rows - 1) * GY + 40
canvas = Image.new("RGB", (W, H), BG)
d = ImageDraw.Draw(canvas)
d.text((60, 30), "ESPChat // UI navigation map (WT32-SC01 / SC01 Plus, landscape 480x320)", fill=GREEN, font=font)

def box(name):
    c, r = layout[name]
    x = 60 + c * (TW + GX); y = 110 + r * (TH + 40 + GY)
    return x, y

for name, (c, r) in layout.items():
    if name not in shots: continue
    x, y = box(name)
    canvas.paste(shots[name].resize((TW, TH), Image.LANCZOS), (x, y))
    d.rectangle([x - 2, y - 2, x + TW + 1, y + TH + 1], outline=DIM, width=2)
    d.text((x, y + TH + 8), name[3:].replace("_", " "), fill=GREEN, font=small)

def arrow(a, b, label):
    ax, ay = box(a); bx, by = box(b)
    if ay == by:              # same row: right/left edge
        if bx > ax: p0, p1 = (ax + TW + 4, ay + TH // 2), (bx - 6, by + TH // 2)
        else: p0, p1 = (ax - 4, ay + TH // 2), (bx + TW + 6, by + TH // 2)
    elif ax == bx:            # same column: bottom -> top
        p0, p1 = (ax + TW // 2, ay + TH + 30), (bx + TW // 2, by - 6)
    else:
        p0 = (ax + TW // 2 + (20 if bx > ax else -20), ay + TH + 30); p1 = (bx + TW // 2, by - 6)
    d.line([p0, p1], fill=GREEN, width=3)
    import math
    ang = math.atan2(p1[1] - p0[1], p1[0] - p0[0])
    for s in (-0.45, 0.45):
        d.line([p1, (p1[0] - 14 * math.cos(ang + s), p1[1] - 14 * math.sin(ang + s))], fill=GREEN, width=3)
    mx, my = (p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2
    tw = d.textlength(label, font=small)
    d.rectangle([mx - tw / 2 - 4, my - 11, mx + tw / 2 + 4, my + 11], fill=BG)
    d.text((mx - tw / 2, my - 9), label, fill=(182, 255, 196), font=small)

for a, b, l in edges:
    if a in layout and b in layout: arrow(a, b, l)
canvas.save(os.path.join(dst, "navigation.png"))
print("navigation.png written")
