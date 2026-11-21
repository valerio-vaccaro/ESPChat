# Fonts

`src/ui/fonts/font_mono_{13,16,20}.c` are generated from **DejaVu Sans Mono**
(Bitstream Vera / DejaVu license, free to embed) by `gen_font.py`. The script is a
small TTF → LVGL 8 converter (4 bpp, Latin‑1 + common punctuation, with a Montserrat
fallback for LVGL symbols), so no npm tooling is needed:

```sh
T=/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf
python3 tools/fonts/gen_font.py $T 16 font_mono_16 lv_font_montserrat_16 src/ui/fonts/font_mono_16.c
```
