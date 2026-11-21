#!/usr/bin/env bash
# Render every UI page on the PC (no hardware needed) into ./screenshots.
# Requires: g++, gcc, python3 + Pillow, and the PlatformIO libdeps (run `pio pkg install` once).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LVGL="$(ls -d "$ROOT"/.pio/libdeps/*/lvgl | head -1)"
BUILD="${BUILD_DIR:-$ROOT/.pio/screenshots}"
mkdir -p "$BUILD/obj" "$BUILD/ppm" "$ROOT/screenshots"

CFLAGS="-O1 -DESPCHAT_NATIVE -DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE -DBOARD_WT32_SC01_PLUS -DHAS_MIC=1 -I$ROOT/include -I$LVGL -I$LVGL/src"
# LVGL (compiled once, cached; rebuilt when lv_conf.h changes)
if [ ! -f "$BUILD/obj/.stamp" ] || [ "$ROOT/include/lv_conf.h" -nt "$BUILD/obj/.stamp" ]; then rm -f "$BUILD"/obj/*.o; touch "$BUILD/obj/.stamp"; fi
for f in $(find "$LVGL/src" -name '*.c'); do
  o="$BUILD/obj/$(echo "${f#$LVGL/}" | tr '/' '_').o"
  [ -f "$o" ] || gcc $CFLAGS -c "$f" -o "$o"
done
for f in "$ROOT"/src/ui/fonts/*.c; do gcc $CFLAGS -c "$f" -o "$BUILD/$(basename "$f").o"; done
APP="$ROOT/src/ui/ui.cpp $ROOT/src/ui/chat_view.cpp $ROOT/src/ui/theme.cpp $ROOT/src/ui/matrix_rain.cpp \
     $ROOT/src/ui/media_common.cpp $ROOT/src/chat/chat_common.cpp $ROOT/src/hal/audio_parse.cpp \
     $ROOT/src/proto/bytes.cpp $ROOT/tools/screenshots/native_stubs.cpp $ROOT/tools/screenshots/sim_main.cpp"
g++ -std=gnu++17 $CFLAGS $APP "$BUILD"/obj/*.o "$BUILD"/font_mono_*.o -o "$BUILD/sim"

python3 "$ROOT/tools/screenshots/make_assets.py" "$BUILD"
(cd "$BUILD" && ./sim "$BUILD/ppm" "$BUILD")
python3 "$ROOT/tools/screenshots/make_screenshots.py" "$BUILD/ppm" "$ROOT/screenshots"
