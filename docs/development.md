# Development

<nav class="site-nav"><a href="index">Home</a><a href="getting-started">Get started</a><a href="hardware">Hardware</a><a href="protocol">Protocol</a><a href="screenshots">Screenshots</a></nav>

## Repository map

| Path | Responsibility |
|---|---|
| `src/main.cpp` | Firmware startup and main loop |
| `src/ui/` | LVGL screens, navigation, theme, and chat views |
| `src/chat/` | Conversations, peers, message handling, and mesh integration |
| `src/mesh/` | BLE mesh transport, relaying, and fragment reassembly |
| `src/proto/` | Packet encoding, payload TLVs, and compression |
| `src/crypto/` | Identity keys, signatures, and Noise session primitives |
| `src/hal/` | Board-facing display, storage, audio, and settings |
| `include/board_config.h` | Pinout, board features, and hardware limits |
| `tools/host_test/` | Host-side protocol interoperability checks |
| `tools/screenshots/` | Headless LVGL UI renderer and image conversion tools |

## Build firmware

```sh
pio run -e wt32-sc01-plus
pio run -e wt32-sc01
```

Upload and monitor using the commands in [Get started](getting-started). PlatformIO dependencies are pinned or constrained in `platformio.ini`.

## Host interoperability check

Run:

```sh
tools/host_test/run.sh
```

This compiles and exercises firmware protocol code on the host and compares it with an independent Python implementation using `cryptography`. Coverage includes Noise handshake in both roles, transport framing and replay rejection, Ed25519 packet signatures, raw DEFLATE, announce data, and file TLVs. It is not a substitute for testing with the official phone apps.

## Regenerate screenshots

The screenshots use the firmware's actual LVGL pages and theme, with demo data rendered on a PC. Install the PlatformIO libraries first so LVGL is present under `.pio/libdeps`, then run:

```sh
tools/screenshots/build.sh
```

The script writes all page images and `navigation.png` into `screenshots/`. The image set is documented in [Screenshots](screenshots).

## Fonts

Fonts are generated from DejaVu Sans Mono for the Latin-1 range and punctuation. See `tools/fonts/README.md` and `tools/fonts/gen_font.py` in the source repository for regeneration details.

## Logging

The default serial output focuses on errors. Use `-DCORE_DEBUG_LEVEL=4` to enable per-connection BLE logs and periodic mesh summaries. The monitor speed is 115200 baud.
