# Hardware

<nav class="site-nav"><a href="index">Home</a><a href="getting-started">Get started</a><a href="user-guide">User guide</a><a href="development">Development</a></nav>

## Supported boards

| Feature | WT32-SC01 | WT32-SC01 Plus |
|---|---|---|
| MCU | ESP32 WROVER-B | ESP32-S3 |
| Flash / PSRAM | 4 MB / PSRAM | 16 MB / 2 MB PSRAM |
| Display | 3.5-inch ST7796, SPI | 3.5-inch ST7796, 8-bit parallel |
| Touch | FT6336U | FT6336U |
| Audio output | External I2S DAC required | On-board NS4168 I2S amplifier |
| microSD | Not present | Present |
| BLE connection limit | 3 shared connections | 4 shared connections |

Both variants use a 320 × 480 panel in landscape rotation at 480 × 320. Board pins and feature flags are configured in `include/board_config.h`; PlatformIO board settings are in `platformio.ini` in the source repository.

## Optional microphone

Voice recording uses an external I2S MEMS microphone such as an INMP441. The default builds set `HAS_MIC=0`. To enable it, add `-DHAS_MIC=1` to the selected environment and connect the microphone to the pins defined by `I2S_MIC_SCK`, `I2S_MIC_WS`, and `I2S_MIC_SD` in `board_config.h`.

| Signal | WT32-SC01 | WT32-SC01 Plus |
|---|---:|---:|
| I2S SCK / BCLK | GPIO 32 | GPIO 10 |
| I2S WS / LRCK | GPIO 27 | GPIO 11 |
| I2S SD | GPIO 34 | GPIO 12 |

Check the board and microphone module pinout before wiring. The microphone requires a common ground and suitable power.

## Optional audio output on original SC01

The original board has no on-board audio amplifier. An I2S DAC or amplifier such as MAX98357A can be connected to the output pins in `board_config.h`; enable `HAS_AUDIO_OUT=1` in the build. The Plus already enables its NS4168 output.

## Build options

```sh
pio run -e wt32-sc01-plus
pio run -e wt32-sc01
```

Build flags can be added through PlatformIO environment settings or the project build configuration:

| Flag | Effect |
|---|---|
| `-DHAS_MIC=1` | Enable the configured I2S microphone input |
| `-DHAS_AUDIO_OUT=1` | Enable an external I2S output on SC01 |
| `-DLCD_ROTATION=3` | Flip the display orientation by 180° |
| `-DCORE_DEBUG_LEVEL=4` | Enable detailed BLE and mesh logging |

The firmware uses a 10-second recording limit, an 80-message in-memory limit per conversation, and a 500 KiB ceiling for public media input. Private media has a 256-fragment limit; see [Limitations](limitations) for transfer implications.
