# Screenshots

Every page of the UI, rendered from the **real firmware UI code** (LVGL 8.3, same theme and
fonts) by a headless PC build with demo data. These were refreshed from the current firmware
source with `tools/screenshots/build.sh`. The GitHub Pages gallery is in [`docs/screenshots.md`](../docs/screenshots.md).

![navigation](navigation.png)

| # | Page | How to get there |
|---|------|------------------|
| 01 | ![](01_splash.png) splash | boot (digital rain, 3 s) |
| 02 | ![](02_mesh_public_channel.png) #MESH public channel | default page; Ed25519‑signed broadcast |
| 03 | ![](03_mesh_keyboard.png) keyboard | tap the input field; ✓ sends |
| 04 | ![](04_attach_menu.png) attach menu | `+` in the input bar |
| 05 | ![](05_doodle.png) doodle pad | attach → DOODLE, draws and sends a JPEG |
| 06 | ![](06_image_viewer.png) image viewer | tap an image thumbnail |
| 07 | ![](07_peers.png) peers | sidebar PEERS; red badge = unread, `E2E` = Noise session |
| 08 | ![](08_private_chat.png) private chat | tap a node; ✓ sent, ✓✓ delivered, 👁 read |
| 09 | ![](09_voice_recording.png) voice recording | hold the mic button (needs an I2S mic) |
| 10 | ![](10_sd_image_picker.png) SD image picker | attach → SD CARD |
| 11 | ![](11_identity.png) identity | sidebar ID: nick, peer id, public keys + QR |
| 12 | ![](12_keys_hidden.png) private keys (hidden) | sidebar KEYS |
| 13 | ![](13_keys_revealed.png) private keys (revealed) | KEYS → REVEAL: hex + QR |
| 14 | ![](14_key_import.png) key import | KEYS → paste hex / LOAD SD → confirm |
| 15 | ![](15_settings.png) settings | sidebar CFG |
| 16 | ![](16_settings_bottom.png) settings (bottom) | scroll CFG |
| 17 | ![](17_panic_confirm.png) panic | CFG → PANIC |
| 18 | ![](18_screensaver.png) screensaver | 2 min idle; tap to wake |

Virtual LEDs next to the title (and in the DM header) blink while there are unread messages:
**red** = unread on the public #mesh channel, **blue** = unread private messages. The number is the count.
