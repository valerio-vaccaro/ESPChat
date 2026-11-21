"""Generate demo pictures for the screenshot renderer (raw swapped-RGB565 'RAW565' blobs)."""
import struct, sys, random
from PIL import Image, ImageDraw

def raw565(img, path):
    img = img.convert("RGB"); w, h = img.size
    out = bytearray(b"RAW565" + struct.pack(">HH", w, h))
    for r, g, b in (img.getpixel((x, y)) for y in range(h) for x in range(w)):
        v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        out += struct.pack(">H", v)          # LV_COLOR_16_SWAP byte order
    open(path, "wb").write(out)

out = sys.argv[1]
random.seed(7)
# "photo": night street with digital rain and a white rabbit silhouette
im = Image.new("RGB", (480, 320), (2, 10, 4)); d = ImageDraw.Draw(im)
for y in range(320):
    d.line([(0, y), (480, y)], fill=(0, int(10 + 40 * y / 320), int(6 + 12 * y / 320)))
for x in range(0, 480, 12):
    top = random.randint(-200, 200)
    for k in range(random.randint(4, 20)):
        yy = top + k * 14
        if 0 <= yy < 320:
            g = 80 + int(175 * k / 20)
            d.text((x, yy), random.choice("01ABZ:.=*+<>"), fill=(0, g, int(g * 0.3)))
d.rectangle([0, 250, 480, 320], fill=(4, 18, 8))
d.ellipse([200, 175, 300, 255], fill=(235, 255, 235))      # body
d.ellipse([270, 150, 320, 200], fill=(235, 255, 235))      # head
d.ellipse([280, 80, 296, 165], fill=(235, 255, 235))       # ears
d.ellipse([298, 88, 314, 168], fill=(235, 255, 235))
d.ellipse([303, 168, 309, 174], fill=(200, 0, 0))          # eye
d.ellipse([188, 205, 214, 231], fill=(235, 255, 235))      # tail
raw565(im, f"{out}/white_rabbit.raw")

# doodle as sent from the on-device pad
dd = Image.new("RGB", (300, 300), (0, 0, 0)); g = ImageDraw.Draw(dd)
g.ellipse([80, 120, 220, 230], outline=(0, 255, 65), width=6)
g.line([(110, 130), (95, 40), (125, 125)], fill=(0, 255, 65), width=6)
g.line([(175, 125), (190, 35), (195, 128)], fill=(0, 255, 65), width=6)
g.ellipse([122, 160, 132, 170], fill=(182, 255, 196)); g.ellipse([170, 160, 180, 170], fill=(182, 255, 196))
g.line([(140, 200), (150, 207), (160, 200)], fill=(182, 255, 196), width=3)
g.text((60, 255), "follow the white rabbit", fill=(0, 143, 17))
raw565(dd, f"{out}/doodle.raw")
