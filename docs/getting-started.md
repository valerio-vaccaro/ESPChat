# Get started

<nav class="site-nav"><a href="index">Home</a><a href="user-guide">User guide</a><a href="hardware">Hardware</a><a href="development">Development</a></nav>

## Requirements

- A WT32-SC01 or WT32-SC01 Plus board.
- USB data cable and a computer supported by [PlatformIO](https://platformio.org/).
- PlatformIO Core or the VS Code PlatformIO extension.
- For full feature use, an optional I2S microphone; the Plus also supports SD images and on-board speaker playback.

The firmware uses PlatformIO, Arduino ESP32, LVGL 8.3, NimBLE, and the Crypto library. Dependencies are declared in `platformio.ini` and are installed by PlatformIO on the first build.

## Build and upload

From the repository root, build and upload the board environment that matches your hardware:

```sh
pio run -e wt32-sc01-plus -t upload
```

```sh
pio run -e wt32-sc01 -t upload
```

Then open the serial monitor at 115200 baud:

```sh
pio device monitor
```

The default environment is `wt32-sc01-plus`. Always specify `-e wt32-sc01` when building for the original board.

## First launch

1. Power on the board. The digital-rain splash screen appears briefly.
2. ESPChat creates or loads the device identity from non-volatile storage (NVS).
3. The `#MESH` conversation opens. The device advertises as a BLE peripheral so nearby bitchat clients can discover it.
4. Open **CFG** to set a nickname. The nickname is announced to the mesh.
5. Open **PEERS** to see nodes that have announced themselves. Select a peer to start a private conversation.

There is no real-time clock on the supported boards. The firmware starts with the build timestamp and adjusts it using incoming peer announces.

## Select a board or feature

PlatformIO environments and optional build flags are described in [Hardware](hardware). For example, microphone recording can be enabled with `-DHAS_MIC=1` when the microphone pins are configured. Keep the board selection and pin definitions aligned in `include/board_config.h`.

## Troubleshooting

| Symptom | Check |
|---|---|
| Upload fails | Select the matching environment, use a USB data cable, and enter the board's bootloader mode if needed. |
| Blank or rotated display | Verify board selection; use `-DLCD_ROTATION=3` to rotate the landscape display by 180 degrees. |
| No microphone control or recording | The microphone is optional and disabled by default; enable `HAS_MIC` and wire the configured I2S pins. |
| No SD image picker | SD storage is available on WT32-SC01 Plus only. |
| No nearby peers | Ensure Bluetooth is enabled on nearby clients and they are running a compatible bitchat implementation. Compatibility remains under real-device testing. |

For verbose BLE connection, discovery, announce, and mesh summary logs, build with `-DCORE_DEBUG_LEVEL=4`.
